// M4 "Banners": the war's overlays on the loaded world (VISION_PLAN 4.5: "identity never changes, only overlays do").
// WARDS lane. From each loaded settlement's realm::SettlementState:
//   damage      damage/100 * 0.7 of its buildings (by hash) draw charred (1 scorched, 2 burned out: barred, nobody home),
//               ash scorches and rubble beside the burned-out ones; SS_REBUILDING: charred 3 (scaffolding) + scaffolds
//   SS_GARRISON a garrison tower in the CONQUEROR's architecture on a hash-chosen free lot, flying its banner (a runtime
//               Bldg designed by the builder: see placeTower for why that is safe with streaming)
//   SS_BESIEGED a siege camp 14-30 tiles outside the walls in the attacker's colours (tents, the command tent, a
//               catapult, a stake palisade with walkable gaps, a campfire; never on roads, water or buildings) and the
//               town's openings barricaded until the player has a gate opened
//   SS_REFUGEES a refugee camp of lean-tos round a campfire outside the friendly town
//   SS_ABANDONED / SS_RUINED   no banners (and, ruined, every building burned out)
// Everything applied is recorded (global tiles, what was there) and restored before the next apply, so the overlays are
// idempotent, follow the window as it moves, and leave the generated world exactly as it was when a state clears.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/war.h"
#include "rpg/world/source.h"

using art::Prop;

uint64_t warTileKey(int32_t gx, int32_t gy) { return ((uint64_t)(uint32_t)gx << 32) | (uint64_t)(uint32_t)gy; }

namespace {

bool warDbg() { static const bool on = std::getenv("EMB_WAR_DEBUG") != nullptr; return on; }
double nowMs() {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline uint32_t mixh(uint64_t a, uint64_t b) { return (uint32_t)(ew::mix64(a * 0x9E3779B97F4A7C15ull ^ (b + 0x632BE59BD9B4E019ull)) >> 16); }

bool walkableGround(Ground gr) {
  return !groundSolid(gr) && gr != Ground::Road && gr != Ground::Plaza && gr != Ground::Bridge && gr != Ground::Farmland &&
         gr != Ground::Swamp && gr != Ground::Ice && gr != Ground::Lava && gr != Ground::Void;
}

// the overlay being built for one settlement
struct Builder {
  Game& g;
  WarOverlay& o;
  ew::Gid kingdom = 0;
  explicit Builder(Game& gg, WarOverlay& oo) : g(gg), o(oo) {}
  Map& m() { return g.world.over; }
  bool taken(int tx, int ty) const { return takenProp(tx, ty) != 0; }   // (a felled tree's cleared tile is free ground)
  uint8_t takenProp(int tx, int ty) const {
    const int32_t gx = g.world.ox + tx, gy = g.world.oy + ty;
    for (const WarTile& t : o.tiles) if (t.gx == gx && t.gy == gy) return t.now;
    return 0;
  }
  bool takenAny(int tx, int ty) const {
    const int32_t gx = g.world.ox + tx, gy = g.world.oy + ty;
    for (const WarTile& t : o.tiles) if (t.gx == gx && t.gy == gy) return true;
    return false;
  }
  void setSolid(int tx, int ty) {
    Map& M = m();
    const size_t i = (size_t)ty * M.w + tx;
    const int p = M.prop[i];
    M.solid[i] = (uint8_t)((p && propSolid((Prop)(p - 1))) || M.wall[i] || M.bldgAt[i] >= 0 ? 1 : 0);
  }
  // a tile set to a raw prop value (0: cleared), recorded like put
  void setRaw(int tx, int ty, uint8_t now) {
    Map& M = m();
    if (!M.in(tx, ty)) return;
    WarTile t;
    t.gx = g.world.ox + tx; t.gy = g.world.oy + ty;
    t.was = M.prop[(size_t)ty * M.w + tx];
    t.now = now;
    o.tiles.push_back(t);
    M.prop[(size_t)ty * M.w + tx] = now;
    setSolid(tx, ty);
  }
  void put(int tx, int ty, Prop p) {
    Map& M = m();
    if (!M.in(tx, ty)) return;
    WarTile t;
    t.gx = g.world.ox + tx; t.gy = g.world.oy + ty;
    t.was = M.prop[(size_t)ty * M.w + tx];
    t.now = (uint8_t)((int)p + 1);
    o.tiles.push_back(t);
    M.prop[(size_t)ty * M.w + tx] = t.now;
    setSolid(tx, ty);
    if (art::isWarProp(p) && kingdom) g.war.propKingdom[warTileKey(t.gx, t.gy)] = kingdom;
  }
  // a war prop with its frozen footprint (art_props.h m4Footprint): the prop on the bottom-centre tile, Filler elsewhere
  bool fits(int ax, int ay, Prop p, int pad) const {
    int w, h;
    art::m4Footprint(p, w, h);
    for (int y = ay - h + 1 - pad; y <= ay + pad; y++)
      for (int x = ax - w / 2 - pad; x <= ax + w / 2 + pad; x++) {
        const bool core = y >= ay - h + 1 && y <= ay && x >= ax - w / 2 && x <= ax + w / 2;
        if (core && !warFreeTile(g, x, y)) return false;
        if (taken(x, y)) return false;
        if (!core && pad > 0 && !g.world.over.in(x, y)) return false;
      }
    return true;
  }
  void foot(int ax, int ay, Prop p) {
    int w, h;
    art::m4Footprint(p, w, h);
    for (int y = ay - h + 1; y <= ay; y++)
      for (int x = ax - w / 2; x <= ax + w / 2; x++) put(x, y, (x == ax && y == ay) ? p : Prop::Filler);
  }
  // the nearest spot to (ax, ay) within r tiles where p fits (with a clear ring of `pad`): true and the anchor
  bool near(int& ax, int& ay, Prop p, int r, int pad) const {
    for (int d = 0; d <= r; d++)
      for (int dy = -d; dy <= d; dy++)
        for (int dx = -d; dx <= d; dx++) {
          if (std::max(std::abs(dx), std::abs(dy)) != d) continue;
          if (fits(ax + dx, ay + dy, p, pad)) { ax += dx; ay += dy; return true; }
        }
    return false;
  }
};

// how much of the box round (cx, cy) is free ground (a camp needs room)
// a tree or a bush a camp may fell (its stumps and cleared ground restored with the rest when the camp goes)
bool fellable(const Game& g, int x, int y) {
  const Map& M = g.world.over;
  if (!M.in(x, y)) return false;
  const int p = M.propAt(x, y);
  if (!p) return false;
  const Prop pp = (Prop)(p - 1);
  if (!(art::isTreeProp(pp) || pp == Prop::Bush || pp == Prop::BerryBush || pp == Prop::SnowBush || art::isWildlandsSolid(pp))) return false;
  if (pp == Prop::Boulder || pp == Prop::Rock || pp == Prop::MossRock || pp == Prop::SnowRock) return false;
  const size_t i = (size_t)y * M.w + x;
  if (M.bldgAt[i] >= 0 || M.wall[i] || (!M.height.empty() && (M.height[i] & (Map::HEIGHT_CLIFF | Map::HEIGHT_RAMP)))) return false;
  const Ground gr = (Ground)M.ground[i];
  return !groundSolid(gr) && gr != Ground::Road && gr != Ground::Plaza && gr != Ground::Bridge && gr != Ground::Farmland;
}
// open ground counts whole, a tree the camp would have to fell counts half (a clearing is preferred, but an army in a
// forest fells what it must: VISION_PLAN 4.5's camps are not left half-hidden among the trunks)
int freeCount(const Game& g, int cx, int cy, int rx, int ry) {
  int n = 0;
  for (int y = cy - ry; y <= cy + ry; y++)
    for (int x = cx - rx; x <= cx + rx; x++) n += warFreeTile(g, x, y) ? 2 : (fellable(g, x, y) ? 1 : 0);
  return n / 2;
}
int treeCount(const Game& g, int cx, int cy, int rx, int ry) {
  int n = 0;
  for (int y = cy - ry; y <= cy + ry; y++)
    for (int x = cx - rx; x <= cx + rx; x++) n += fellable(g, x, y) ? 1 : 0;
  return n;
}

// distance (tiles) from (x, y) to the settlement's rectangle (0 inside)
int rectDist(const IRect& r, int x, int y) {
  const int dx = std::max({r.x - x, x - (r.x + r.w - 1), 0}), dy = std::max({r.y - y, y - (r.y + r.h - 1), 0});
  return std::max(dx, dy);
}

// a camp site outside the settlement's rectangle, between dMin and dMax tiles from it, preferring the direction
// (dirx, diry); true and its centre
// (tenths: how much of the camp's box must be open ground: a camp pitches in a clearing or on open grass, not among trees)
bool campSpot(const Game& g, const Site& s, float dirx, float diry, int dMin, int dMax, int rx, int ry, uint32_t h, int& ocx, int& ocy, int tenths = 9) {
  const float a0 = std::atan2(diry, dirx);
  const float cx0 = s.r.x + s.r.w * 0.5f, cy0 = s.r.y + s.r.h * 0.5f;
  const int need = (2 * rx + 1) * (2 * ry + 1) * tenths / 10;
  int best = -1, bx = 0, by = 0;
  for (int k = 0; k < 17; k++) {
    const int step = (k + 1) / 2 * ((k & 1) ? 1 : -1);   // 0, +1, -1, +2, -2...
    const float a = a0 + step * 0.3927f + ((h >> (k % 8)) & 3) * 0.05f;
    const float ux = std::cos(a), uy = std::sin(a);
    for (int d = dMin; d <= dMax; d += 2) {
      // walk out from the heart until (d) tiles beyond the rectangle's edge
      int x = (int)std::lround(cx0), y = (int)std::lround(cy0);
      for (float t = 0; t < 400; t += 1.0f) {
        x = (int)std::lround(cx0 + ux * t); y = (int)std::lround(cy0 + uy * t);
        if (rectDist(s.r, x, y) >= d) break;
      }
      if (x - rx < 2 || y - ry < 2 || x + rx >= World::WIN - 2 || y + ry >= World::WIN - 2) continue;
      const int f = freeCount(g, x, y, rx, ry);
      if (f < need) continue;
      const int score = f * 4 - treeCount(g, x, y, rx, ry) * 3 - std::abs(step) * 9 - (d - dMin);
      if (score > best) { best = score; bx = x; by = y; }
    }
    if (best >= 0 && std::abs(step) >= 2) break;   // a good spot near the preferred side is enough
  }
  if (best < 0) return false;
  ocx = bx; ocy = by;
  return true;
}

// a camp's spot, stable in global terms: the one chosen before (if it is in the window now), else a fresh choice, kept
// when the settlement and the room round it (reach tiles) lie wholly in the window
template <class F>
bool stableSpot(Game& g, uint64_t key, int& cx, int& cy, F&& choose, int reach) {
  auto it = g.war.spots.find(key);
  if (it != g.war.spots.end()) {
    cx = it->second.first - g.world.ox; cy = it->second.second - g.world.oy;
    return cx >= 2 && cy >= 2 && cx < World::WIN - 2 && cy < World::WIN - 2;
  }
  if (!choose(cx, cy)) return false;
  // (fixer M4 r2, review: "a besieged capital gets no siege camp") kept once chosen, whole or not: a city's camp often
  // stands near the window's edge, and choosing it afresh after the window moved (a walk or a teleport toward it) put
  // the camp somewhere else, leaving the player at an empty spot where it had been
  (void)reach;
  g.war.spots[key] = {g.world.ox + cx, g.world.oy + cy};
  return true;
}

const Kingdom* kingdomRec(const Game& g, ew::Gid id) {
  auto it = g.world.kingdomById.find(id);
  return it == g.world.kingdomById.end() ? nullptr : &g.world.kingdoms[(size_t)it->second];
}

// ---------------------------------------------------------------- the siege camp
void siegeCamp(Game& g, WarOverlay& o, int si, const realm::Siege& sg) {
  const Site& s = g.world.sites[(size_t)si];
  Builder B(g, o);
  B.kingdom = sg.attacker;
  const uint32_t h = mixh(sg.id, s.id);
  // toward the realm's suggested camp, else south
  float dx = (float)(sg.campX - (g.world.ox + s.ex)), dy = (float)(sg.campY - (g.world.oy + s.ey));
  if (std::fabs(dx) + std::fabs(dy) < 1) dy = 1;
  int cx, cy;
  // the camp's heart 21-27 tiles out, so its tents, catapult and palisade stand 14-34 tiles outside the walls; chosen
  // once (with the room round it in the window) and kept in global terms
  const uint64_t spotKey = s.id ^ ((uint64_t)sg.id << 40) ^ 0x51E6E000ull;
  if (!stableSpot(g, spotKey, cx, cy, [&](int& x, int& y) {
        return campSpot(g, s, dx, dy, 21, 27, 7, 5, h, x, y) || campSpot(g, s, dx, dy, 19, 30, 5, 4, h, x, y) ||
               campSpot(g, s, dx, dy, 19, 32, 5, 4, h, x, y, 7);
      }, 34))
    return;
  WarCamp c;
  c.kind = CampKind::Siege;
  c.site = s.id; c.kingdom = sg.attacker; c.other = sg.defender; c.siege = sg.id;
  c.gx = g.world.ox + cx; c.gy = g.world.oy + cy;
  // the camp faces the town: forward (fx, fy) toward the heart, side (sx, sy) across it
  float fx = (float)(s.ex - cx), fy = (float)(s.ey - cy);
  const float fl = std::sqrt(fx * fx + fy * fy);
  fx /= std::max(1.0f, fl); fy /= std::max(1.0f, fl);
  const float sx = -fy, sy = fx;
  auto at = [&](float f, float sd, int& x, int& y) { x = cx + (int)std::lround(fx * f + sx * sd); y = cy + (int)std::lround(fy * f + sy * sd); };
  // the trees in the camp's ground are felled: stumps here and there, the rest cleared (all restored when it goes)
  for (int y = cy - 6; y <= cy + 6; y++)
    for (int x = cx - 8; x <= cx + 8; x++)
      if (fellable(g, x, y)) {
        const bool stump = hash2(x + g.world.ox, y + g.world.oy, h) % 4 == 0 && (std::abs(x - cx) > 5 || std::abs(y - cy) > 4);
        B.setRaw(x, y, stump ? (uint8_t)((int)Prop::Stump + 1) : (uint8_t)0);
      }
  // the campfire at the heart
  if (warFreeTile(g, cx, cy)) B.put(cx, cy, Prop::Campfire);
  // the command tent behind it, away from the town
  {
    int x, y;
    at(-3.5f, 0.0f, x, y);
    if (B.near(x, y, Prop::CommandTent, 2, 1)) { B.foot(x, y, Prop::CommandTent); c.cmdX = g.world.ox + x; c.cmdY = g.world.oy + y + 1; c.tents.push_back({c.cmdX, c.cmdY - 1}); }
    else { c.cmdX = c.gx; c.cmdY = c.gy + 1; }
  }
  // the soldiers' tents round the fire (3-5, jittered by hash so no two camps repeat)
  const int nt = 3 + (int)(h % 3);
  static const float ring[6][2] = {{-1.0f, -5.5f}, {-1.0f, 5.5f}, {1.5f, -4.5f}, {1.5f, 4.5f}, {-5.5f, -5.0f}, {-5.5f, 5.0f}};
  for (int k = 0; k < nt; k++) {
    int x, y;
    const int j = (int)((h >> (k * 3)) % 3) - 1;
    at(ring[k][0] + j * 0.5f, ring[k][1] + j * 0.7f, x, y);
    if (B.near(x, y, Prop::WarTent, 2, 1)) {
      B.foot(x, y, Prop::WarTent);
      c.tents.push_back({g.world.ox + x, g.world.oy + y});
      // (fixer M4 r1) its post a step to one side of the door and out from it (a man by his tent, not in its doorway)
      int qx = x + ((h >> (k + 11)) & 1 ? 2 : -2), qy = y + 1 + (int)((h >> (k + 5)) & 1);
      if (!warFreeTile(g, qx, qy) || B.taken(qx, qy)) { qx = x; qy = y + 2; }
      c.posts.push_back({g.world.ox + qx, g.world.oy + qy});
    }
  }
  // the catapult in front, toward the walls
  {
    int x, y;
    at(4.5f, (float)((int)(h >> 9) % 3 - 1), x, y);
    if (B.near(x, y, Prop::Catapult, 2, 1)) { B.foot(x, y, Prop::Catapult); c.posts.push_back({g.world.ox + x + 2, g.world.oy + y}); }
  }
  // a few supplies by the tents
  for (int k = 0; k < 3; k++) {
    int x, y;
    at(-1.5f + k, (k - 1) * 2.5f + 2.0f, x, y);
    static const Prop sup[3] = {Prop::Crate, Prop::Barrel, Prop::Sacks};
    if (warFreeTile(g, x, y) && !B.taken(x, y) && !B.taken(x, y + 1) && warFreeTile(g, x, y + 1)) B.put(x, y, sup[(h >> (k * 2)) % 3]);
  }
  // the stake palisade: an arc on the town side, 7-8 tiles out, 4-connected (a stake on the corner tile where a step goes
  // diagonal, so the run joins without gaps), with a walkable gap every few stakes
  {
    const float R = 7.5f;
    int px = INT32_MIN, py = INT32_MIN, run = 0;
    const int gapEvery = 5 + (int)(h % 3);
    for (float a = -1.25f; a <= 1.25f; a += 0.04f) {
      const float ca = std::cos(a), sa = std::sin(a);
      const int x = cx + (int)std::lround((fx * ca + sx * sa) * R);
      const int y = cy + (int)std::lround((fy * ca + sy * sa) * R);
      if (x == px && y == py) continue;
      auto stake = [&](int qx, int qy) {
        if (B.taken(qx, qy) || !warFreeTile(g, qx, qy)) { run = 0; return; }
        // keep the tiles in front of the tents' doors and the catapult clear (nothing stands directly south of a prop)
        const uint8_t above = B.takenProp(qx, qy - 1);
        if (above && above != (uint8_t)((int)Prop::Palisade + 1)) return;
        run++;
        if (run % gapEvery == 0) { c.posts.push_back({g.world.ox + qx, g.world.oy + qy}); return; }   // a gap
        B.put(qx, qy, Prop::Palisade);
      };
      if (px != INT32_MIN && x != px && y != py) stake(x, py);   // the corner tile of a diagonal step
      stake(x, y);
      px = x; py = y;
    }
  }
  // posts round the fire
  for (int k = 0; k < 4; k++) {
    static const int ox4[4] = {-2, 2, 0, 0}, oy4[4] = {0, 0, 2, -2};
    if (warFreeTile(g, cx + ox4[k], cy + oy4[k]) && !B.taken(cx + ox4[k], cy + oy4[k])) c.posts.push_back({g.world.ox + cx + ox4[k], g.world.oy + cy + oy4[k]});
  }
  g.war.camps.push_back(c);
}

// the settlement's openings in its walls barricaded (a siege: closed gates)
void closeGates(Game& g, WarOverlay& o, int si, ew::Gid owner) {
  const Site& s = g.world.sites[(size_t)si];
  if (warGatesOpen(g, s.id)) return;
  Builder B(g, o);
  B.kingdom = owner;
  Map& M = g.world.over;
  for (const IRect& r : g.world.wallGaps) {
    if (r.x + r.w < s.r.x - 2 || r.y + r.h < s.r.y - 2 || r.x > s.r.x + s.r.w + 2 || r.y > s.r.y + s.r.h + 2) continue;
    for (int y = r.y; y < r.y + r.h; y++)
      for (int x = r.x; x < r.x + r.w; x++) {
        if (!M.in(x, y) || M.blocked(x, y) || M.bldgAt[(size_t)y * M.w + x] >= 0 || M.prop[(size_t)y * M.w + x] || B.taken(x, y)) continue;
        if (groundWater(M.at(x, y))) continue;
        B.put(x, y, Prop::Barricade);
        g.war.gateTiles.insert(warTileKey(g.world.ox + x, g.world.oy + y));   // (the player is let through: warGatePass)
      }
  }
}

// ---------------------------------------------------------------- the refugee camp
void refugeeCamp(Game& g, WarOverlay& o, int si, const realm::SettlementState& st) {
  const Site& s = g.world.sites[(size_t)si];
  Builder B(g, o);
  B.kingdom = st.owner;
  const uint32_t h = mixh(s.id, st.refugeesFrom ^ 0x5EF0u);
  float dx = 0, dy = 1;
  if (const realm::SettlementState* f = g.realm.settlement(st.refugeesFrom)) {
    dx = (float)(f->gx - (g.world.ox + s.ex)); dy = (float)(f->gy - (g.world.oy + s.ey));
    if (std::fabs(dx) + std::fabs(dy) < 1) dy = 1;
  }
  int cx, cy;
  const uint64_t spotKey = s.id ^ ((uint64_t)st.refugeesFrom * 0x9E3779B97F4A7C15ull) ^ 0x8EF06Eull;
  if (!stableSpot(g, spotKey, cx, cy, [&](int& x, int& y) {
        return campSpot(g, s, dx, dy, 12, 18, 5, 4, h, x, y) || campSpot(g, s, dx, dy, 9, 24, 4, 3, h, x, y) ||
               campSpot(g, s, dx, dy, 9, 26, 4, 3, h, x, y, 7);
      }, 26))
    return;
  WarCamp c;
  c.kind = CampKind::Refugee;
  c.site = s.id; c.kingdom = st.owner; c.other = st.refugeesFrom;
  c.gx = g.world.ox + cx; c.gy = g.world.oy + cy;
  c.cmdX = c.gx; c.cmdY = c.gy + 1;
  // their firewood and their tent poles came from the trees round them: stumps and cleared ground (restored later)
  for (int y = cy - 5; y <= cy + 5; y++)
    for (int x = cx - 6; x <= cx + 6; x++)
      if (fellable(g, x, y))
        B.setRaw(x, y, hash2(x + g.world.ox, y + g.world.oy, h) % 3 == 0 && (std::abs(x - cx) > 4 || std::abs(y - cy) > 3) ? (uint8_t)((int)Prop::Stump + 1) : (uint8_t)0);
  if (warFreeTile(g, cx, cy)) B.put(cx, cy, Prop::Campfire);
  const int nt = 3 + (int)(h % 2);
  static const int ring[4][2] = {{-4, -1}, {4, -1}, {-2, 4}, {3, 4}};
  for (int k = 0; k < nt; k++) {
    int x = cx + ring[k][0] + (int)((h >> (k * 2)) % 3) - 1, y = cy + ring[k][1] + (int)((h >> (k * 2 + 1)) % 2);
    if (B.near(x, y, Prop::RefugeeTent, 2, 1)) { B.foot(x, y, Prop::RefugeeTent); c.tents.push_back({g.world.ox + x, g.world.oy + y}); c.posts.push_back({g.world.ox + x, g.world.oy + y + 1}); }
  }
  // bundles and bedrolls by the fire
  {
    int x = cx + 2, y = cy - 1;
    if (warFreeTile(g, x, y) && !B.taken(x, y)) B.put(x, y, Prop::Sacks);
    x = cx - 1; y = cy + 2;
    if (warFreeTile(g, x, y) && !B.taken(x, y)) B.put(x, y, Prop::Bedroll);
    x = cx + 1; y = cy + 2;
    if ((h & 4) && warFreeTile(g, x, y) && !B.taken(x, y)) B.put(x, y, Prop::Bedroll);
  }
  for (int k = 0; k < 4; k++) {
    static const int ox4[4] = {-2, 2, 0, -1}, oy4[4] = {1, 1, -2, -2};
    if (warFreeTile(g, cx + ox4[k], cy + oy4[k]) && !B.taken(cx + ox4[k], cy + oy4[k])) c.posts.push_back({g.world.ox + cx + ox4[k], g.world.oy + cy + oy4[k]});
  }
  // the walkers' far point: 22 tiles on toward the burned place
  {
    const float l = std::sqrt(dx * dx + dy * dy);
    int wx = cx + (int)std::lround(dx / std::max(1.0f, l) * 22), wy = cy + (int)std::lround(dy / std::max(1.0f, l) * 22);
    wx = std::clamp(wx, 4, World::WIN - 5); wy = std::clamp(wy, 4, World::WIN - 5);
    c.walkX = g.world.ox + wx; c.walkY = g.world.oy + wy;
  }
  g.war.camps.push_back(c);
}

// ---------------------------------------------------------------- the garrison tower
// A conqueror of another culture keeps a tower in its own architecture (VISION_PLAN 4.5). It is a runtime Bldg (the
// builder designs it from the conqueror culture's ArchStyle, the painter paints it like any other building), which is
// safe with streaming because: buildings are never erased or reordered (World::over.bldgs is append-only, so the handle
// stays valid), the record is appended OUTSIDE its site's bldgFirst..bldgCount range (no settlement loop counts it as a
// home, a shop or the seat), it has no id (bldgById never maps to it, saves never name it), placeWindow translates it
// and rebuildSolid stamps it like the others, and the player cannot go in (warBarred), so no interior, lodging or save
// position ever refers to it. When the state clears it is retired (moved far off, zero size) and its record is reused.
int placeTower(Game& g, WarOverlay& o, int si, ew::Gid conq) {
  const Site& s = g.world.sites[(size_t)si];
  const Kingdom* K = kingdomRec(g, conq);
  if (!K) return -1;
  const cult::Culture* C = g.world.src ? &g.world.src->culture(K->culture) : nullptr;
  if (!C || !K->culture) return -1;
  Map& M = g.world.over;
  const uint32_t h = mixh(s.id, conq ^ 0x70E3u);
  // a free 3 x 3 lot inside the settlement: free ground with a clear ring round it, nothing built in the 5 rows above
  // it (the tower's rise), and a walkable tile before its door
  auto lotOk = [&](int x0, int y0) {
    for (int y = y0 - 1; y <= y0 + 3; y++)
      for (int x = x0 - 1; x <= x0 + 3; x++) {
        if (!M.in(x, y)) return false;
        const size_t i = (size_t)y * M.w + x;
        const bool core = x >= x0 && x < x0 + 3 && y >= y0 && y < y0 + 3;
        if (M.bldgAt[i] >= 0 || M.wall[i] || groundSolid((Ground)M.ground[i]) || (M.height[i] & (Map::HEIGHT_CLIFF | Map::HEIGHT_RAMP))) return false;
        if (core) {
          const Ground gr = (Ground)M.ground[i];
          if (gr == Ground::Road || gr == Ground::Plaza || gr == Ground::Bridge || gr == Ground::Farmland) return false;
          if (M.prop[i] && (propSolid((Prop)(M.prop[i] - 1)) || !art::isFloraProp((Prop)(M.prop[i] - 1)))) return false;
        } else if (M.solid[i] && !(M.prop[i] && art::isFloraProp((Prop)(M.prop[i] - 1)))) return false;
      }
    for (int y = y0 - 6; y < y0 - 1; y++)
      for (int x = x0 - 1; x <= x0 + 3; x++)
        if (M.in(x, y) && M.bldgAt[(size_t)y * M.w + x] >= 0) return false;
    // the door row's tile in front must stay walkable
    return M.in(x0 + 1, y0 + 3) && !M.blocked(x0 + 1, y0 + 3);
  };
  const int x0 = std::max(2, s.r.x + 2), y0 = std::max(7, s.r.y + 6), x1 = std::min(World::WIN - 5, s.r.x + s.r.w - 5), y1 = std::min(World::WIN - 5, s.r.y + s.r.h - 5);
  if (x1 < x0 || y1 < y0) return -1;
  const int W = x1 - x0 + 1, H = y1 - y0 + 1, N = W * H;
  int lotX = -1, lotY = -1, bestD = 1 << 30;
  const int start = (int)(h % (uint32_t)N);
  // a hash-chosen lot: walk the candidates from a hashed start, keep the one nearest the heart among the first few found
  int found = 0;
  for (int k = 0; k < N && found < 6; k += 2) {
    const int idx = (start + k * 7) % N;
    const int x = x0 + idx % W, y = y0 + idx / W;
    if (!lotOk(x, y)) continue;
    found++;
    const int d = std::abs(x + 1 - s.ex) + std::abs(y + 1 - s.ey);
    if (d < bestD) { bestD = d; lotX = x; lotY = y; }
  }
  if (lotX < 0) return -1;
  {   // the lot chosen first is kept (in global terms) while the window moves
    auto it = g.war.spots.find(s.id ^ 0x70E37000ull);
    if (it != g.war.spots.end()) {
      const int kx = it->second.first - g.world.ox, ky = it->second.second - g.world.oy;
      if (lotOk(kx, ky)) { lotX = kx; lotY = ky; }
    } else if (s.r.x >= 0 && s.r.y >= 0 && s.r.x + s.r.w <= World::WIN && s.r.y + s.r.h <= World::WIN)
      g.war.spots[s.id ^ 0x70E37000ull] = {g.world.ox + lotX, g.world.oy + lotY};
  }
  Bldg b;
  b.id = 0;
  b.type = art::Building::Barracks;
  b.r = IRect{lotX, lotY, 3, 3};
  b.seed = h | 1u;
  b.site = si;
  b.owner = Role::Guard;
  b.storeys = 3;
  b.hearth = false;
  b.biome = M.biomeAt(lotX + 1, lotY + 1);
  b.banner = K->color ? K->color : rgba(160, 40, 40);
  b.banner2 = K->color2;
  b.emblem = K->emblem;
  b.urban = 1;
  // the urban rank and the climate (snow on the roof, the style) of the place it stands in, from one of its buildings
  // (fixer M4 r2: the tile's own biome under a snowy town's square gave the tower a bare roof among white ones)
  for (int i = 0; i < s.bldgCount; i++) {
    const int bi = s.bldgFirst + i;
    if (bi >= 0 && bi < (int)M.bldgs.size() && M.bldgs[(size_t)bi].site == si) { b.urban = M.bldgs[(size_t)bi].urban; b.biome = M.bldgs[(size_t)bi].biome; break; }
  }
  b.arch = cult::buildingArch(*C, (int)b.biome, b.urban, 2, b.seed);
  b.styled = true;
  b.wealth = 2;
  b.form = (uint8_t)bld::Form::Tower;
  // clear the ground cover on its lot (restored with the rest)
  Builder Bd(g, o);
  for (int y = lotY; y < lotY + 3; y++)
    for (int x = lotX; x < lotX + 3; x++)
      if (M.prop[(size_t)y * M.w + x]) Bd.setRaw(x, y, 0);
  int bi;
  if (!g.war.towersFree.empty()) { bi = g.war.towersFree.back(); g.war.towersFree.pop_back(); M.bldgs[(size_t)bi] = b; }
  else { bi = (int)M.bldgs.size(); M.bldgs.push_back(b); }
  for (int y = lotY; y < lotY + 3; y++)
    for (int x = lotX; x < lotX + 3; x++) { M.bldgAt[(size_t)y * M.w + x] = bi; M.solid[(size_t)y * M.w + x] = 1; }
  return bi;
}

void retireTower(Game& g, int bi) {
  Map& M = g.world.over;
  if (bi < 0 || bi >= (int)M.bldgs.size()) return;
  Bldg& b = M.bldgs[(size_t)bi];
  for (int y = b.r.y; y < b.r.y + b.r.h; y++)
    for (int x = b.r.x; x < b.r.x + b.r.w; x++) {
      if (!M.in(x, y)) continue;
      const size_t i = (size_t)y * M.w + x;
      if (M.bldgAt[i] == bi) M.bldgAt[i] = -1;
      const int p = M.prop[i];
      M.solid[i] = (uint8_t)((p && propSolid((Prop)(p - 1))) || M.wall[i] || M.bldgAt[i] >= 0 ? 1 : 0);
    }
  b.r = IRect{-1000000, -1000000, 0, 0};
  b.banner = 0;
  b.site = -1;
  g.war.towersFree.push_back(bi);
}

// ---------------------------------------------------------------- damage: charred buildings, ash, rubble, scaffolds
void damageOverlay(Game& g, WarOverlay& o, int si, const realm::SettlementState& st) {
  Map& M = g.world.over;
  const Site& s = g.world.sites[(size_t)si];
  std::vector<std::pair<uint32_t, int>> order;
  for (int i = 0; i < s.bldgCount; i++) {
    const int bi = s.bldgFirst + i;
    if (bi < 0 || bi >= (int)M.bldgs.size() || M.bldgs[(size_t)bi].site != si) continue;
    const Bldg& b = M.bldgs[(size_t)bi];
    order.push_back({mixh(b.id ? b.id : b.seed, s.id ^ 0xC4A2u), bi});
  }
  std::sort(order.begin(), order.end());
  const bool ruined = (st.flags & realm::SS_RUINED) != 0;
  const bool burned = (st.flags & realm::SS_BURNED) != 0;
  const bool rebuilding = (st.flags & realm::SS_REBUILDING) != 0;
  const int pick = ruined ? (int)order.size() : (int)std::lround(order.size() * (st.damage / 100.0) * 0.7);
  Builder B(g, o);
  for (int k = 0; k < pick && k < (int)order.size(); k++) {
    const int bi = order[(size_t)k].second;
    Bldg& b = M.bldgs[(size_t)bi];
    const uint32_t hh = order[(size_t)k].first;
    uint8_t c = 1;
    if (ruined) c = 2;
    else if (burned) c = (hh >> 7) % 4 == 0 ? 1 : 2;
    else if (st.damage >= 55) c = (hh >> 7) % 2 ? 2 : 1;
    if (rebuilding && c == 2) c = 3;
    if (rebuilding && c == 1 && (hh >> 9) % 2) c = 3;
    // (fixer M4 r2) the places the story needs stay open: the seat of power (the lord, the king's throne), the inn, and
    // any building an open quest sends the player into or pays from are scorched, never burned out (barred)
    if (c == 2) {
      bool keep = bldgIsSeat(b) || b.type == art::Building::Inn;
      for (const Quest& q : g.quests)
        if (q.state != QState::Done && (q.destBldg == bi || q.giverBldg == bi)) keep = true;
      if (keep) c = rebuilding ? 3 : 1;
    }
    o.charred.push_back({bi, b.charred});
    b.charred = c;
    if (c == 2) g.war.barred.push_back(bi);
    // the ground round it: ash scorches and fallen rubble beside a burned-out shell, a scaffold by one being rebuilt
    const int doorX = b.doorX(), frontY = b.r.y + b.r.h;
    auto freeBeside = [&](int x, int y) {
      if (!M.in(x, y) || B.taken(x, y) || !warFreeTile(g, x, y)) return false;
      if (x == doorX && y == frontY) return false;   // the doorstep stays clear
      return true;
    };
    if (c == 2) {
      // one or two scorches, apart from each other (never a row of them)
      int ash = 0, ax0 = -100, ay0 = -100;
      const int ashWant = 1 + (int)((hh >> 11) % 2);
      for (int k2 = 0; k2 < 12 && ash < ashWant; k2++) {
        const uint32_t r = mixh(hh, (uint64_t)k2 + 11);
        int x, y;
        switch (r % 3) {
          case 0: x = b.r.x + (int)((r >> 4) % (uint32_t)b.r.w); y = frontY + (int)((r >> 9) % 2); break;   // before it
          case 1: x = b.r.x - 1; y = b.r.y + (int)((r >> 4) % (uint32_t)b.r.h); break;                       // its west side
          default: x = b.r.x + b.r.w; y = b.r.y + (int)((r >> 4) % (uint32_t)b.r.h); break;                 // its east side
        }
        if (!freeBeside(x, y) || std::abs(x - ax0) + std::abs(y - ay0) < 4) continue;
        B.put(x, y, Prop::Ash);
        ax0 = x; ay0 = y;
        ash++;
      }
      // one heap of rubble at a side wall, never before the door
      for (int k2 = 0; k2 < 6; k2++) {
        const uint32_t r = mixh(hh, (uint64_t)k2 + 31);
        const int x = (r & 1) ? b.r.x - 1 : b.r.x + b.r.w, y = b.r.y + b.r.h - 1 - (int)((r >> 3) % (uint32_t)std::max(1, b.r.h - 1));
        if (!freeBeside(x, y) || !freeBeside(x, y + 1)) continue;
        B.put(x, y, Prop::Rubble);
        break;
      }
    } else if (c == 3) {
      for (int k2 = 0; k2 < 4; k2++) {
        const uint32_t r = mixh(hh, (uint64_t)k2 + 51);
        const int x = (r & 1) ? b.r.x - 1 : b.r.x + b.r.w, y = b.r.y + b.r.h - 1;
        if (!freeBeside(x, y) || !freeBeside(x, y + 1)) continue;
        B.put(x, y, Prop::Scaffold);
        if (freeBeside(x + ((r & 1) ? -1 : 1), y)) B.put(x + ((r & 1) ? -1 : 1), y, Prop::Woodpile);
        break;
      }
    }
  }
}

void lowerBanners(Game& g, WarOverlay& o, int si) {
  Map& M = g.world.over;
  const Site& s = g.world.sites[(size_t)si];
  for (int i = 0; i < s.bldgCount; i++) {
    const int bi = s.bldgFirst + i;
    if (bi < 0 || bi >= (int)M.bldgs.size() || M.bldgs[(size_t)bi].site != si || !M.bldgs[(size_t)bi].banner) continue;
    o.banners.push_back({bi, M.bldgs[(size_t)bi].banner});
    M.bldgs[(size_t)bi].banner = 0;
  }
}

}  // namespace

// ---------------------------------------------------------------- shared helpers
bool warFreeTile(const Game& g, int tx, int ty) {
  const Map& M = g.world.over;
  if (!M.in(tx, ty) || M.ground.empty()) return false;
  const size_t i = (size_t)ty * M.w + tx;
  if (!walkableGround((Ground)M.ground[i])) return false;
  if (M.bldgAt[i] >= 0 || M.wall[i]) return false;
  if (!M.height.empty() && (M.height[i] & (Map::HEIGHT_CLIFF | Map::HEIGHT_RAMP))) return false;
  const int p = M.prop[i];
  if (p) {
    const Prop pp = (Prop)(p - 1);
    if (propSolid(pp) || !art::isFloraProp(pp)) return false;   // ground cover only (grass, flowers, ferns)
  }
  // nobody's doorstep
  if (ty > 0 && M.bldgAt[i - (size_t)M.w] >= 0) {
    const Bldg& b = M.bldgs[(size_t)M.bldgAt[i - (size_t)M.w]];
    if (tx == b.doorX() && ty - 1 == b.doorY()) return false;
  }
  return true;
}

const WarCamp* warCampOf(const Game& g, uint32_t siege) {
  for (const WarCamp& c : g.war.camps) if (c.kind == CampKind::Siege && c.siege == siege) return &c;
  return nullptr;
}
const WarCamp* warRefugeCampOf(const Game& g, ew::Gid site) {
  for (const WarCamp& c : g.war.camps) if (c.kind == CampKind::Refugee && c.site == site) return &c;
  return nullptr;
}

void warRestoreOverlays(Game& g) {
  Map& M = g.world.over;
  for (auto it = g.war.ov.rbegin(); it != g.war.ov.rend(); ++it) {
    WarOverlay& o = *it;
    for (auto t = o.tiles.rbegin(); t != o.tiles.rend(); ++t) {
      const int x = t->gx - g.world.ox, y = t->gy - g.world.oy;
      if (!M.in(x, y)) continue;   // out of the window: when it comes back it is the generator's again
      const size_t i = (size_t)y * M.w + x;
      if (M.prop[i] != t->now) continue;   // someone else changed it since (never ours to undo)
      M.prop[i] = t->was;
      const int p = M.prop[i];
      M.solid[i] = (uint8_t)((p && propSolid((Prop)(p - 1))) || M.wall[i] || M.bldgAt[i] >= 0 ? 1 : 0);
    }
    for (auto& c : o.charred)
      if (c.first >= 0 && c.first < (int)M.bldgs.size()) M.bldgs[(size_t)c.first].charred = c.second;
    for (auto& b : o.banners)
      if (b.first >= 0 && b.first < (int)M.bldgs.size()) M.bldgs[(size_t)b.first].banner = b.second;
    if (o.tower >= 0) retireTower(g, o.tower);
  }
  g.war.ov.clear();
  g.war.camps.clear();
  g.war.propKingdom.clear();
  g.war.barred.clear();
  g.war.gateTiles.clear();
}

void warApplyOverlays(Game& g) {
  const double t0 = nowMs();
  warRestoreOverlays(g);
  if (!g.world.endless || !g.world.src || g.world.over.ground.empty()) return;
  int sites = 0;
  for (int si : g.world.nearSites) {
    if (si < 0 || si >= (int)g.world.sites.size()) continue;
    const Site& s = g.world.sites[(size_t)si];
    if (!s.settlement()) continue;
    const realm::SettlementState* st = g.realm.settlement(s.id);
    if (!st) continue;
    const bool besieged = (st->flags & realm::SS_BESIEGED) != 0;
    const realm::Siege* sg = besieged ? g.realm.siegeAt(s.id) : nullptr;
    const bool any = st->damage > 0 || (st->flags & (realm::SS_GARRISON | realm::SS_REFUGEES | realm::SS_ABANDONED | realm::SS_RUINED)) || sg;
    if (!any) continue;
    // only places whose area touches the window (a camp may lie 30 tiles out)
    if (s.r.x + s.r.w + 34 < 0 || s.r.y + s.r.h + 34 < 0 || s.r.x - 34 >= World::WIN || s.r.y - 34 >= World::WIN) continue;
    g.war.ov.push_back(WarOverlay());
    WarOverlay& o = g.war.ov.back();
    o.site = s.id;
    if (st->damage > 0 || (st->flags & realm::SS_RUINED)) damageOverlay(g, o, si, *st);
    if (st->flags & (realm::SS_ABANDONED | realm::SS_RUINED)) lowerBanners(g, o, si);
    if ((st->flags & realm::SS_GARRISON) && st->garrisonOf && !(st->flags & (realm::SS_ABANDONED | realm::SS_RUINED))) {
      o.tower = placeTower(g, o, si, st->garrisonOf);
      if (o.tower >= 0) g.war.barred.push_back(o.tower);
    }
    if (sg) {
      siegeCamp(g, o, si, *sg);
      closeGates(g, o, si, sg->defender);
    }
    if (st->flags & realm::SS_REFUGEES) refugeeCamp(g, o, si, *st);
    sites++;
  }
  // checkpoints where a road crosses into a warring kingdom's land (VISION_PLAN 4.5): barricades either side of the road
  // (the road itself stays open) and two of the land's soldiers on watch; at most two in the window
  {
    bool anyWar = false;
    for (const realm::War& w : g.realm.wars()) if (!w.endDay) anyWar = true;
    if (anyWar) {
      g.war.ov.push_back(WarOverlay());
      WarOverlay& o = g.war.ov.back();
      Builder B(g, o);
      Map& M = g.world.over;
      auto ownerAt = [&](int x, int y) {
        const int32_t gx = g.world.ox + x, gy = g.world.oy + y;
        return g.realm.landOwner(g.world.src->kingdomAt(gx, gy), gx, gy);
      };
      auto warring = [&](ew::Gid k) {
        if (!k) return false;
        for (const realm::War& w : g.realm.wars()) if (!w.endDay && (w.attacker == k || w.defender == k)) return true;
        return false;
      };
      auto road = [&](int x, int y) { return M.in(x, y) && (M.at(x, y) == Ground::Road || M.at(x, y) == Ground::Bridge) && !M.blocked(x, y); };
      int made = 0, roads = 0, pairs = 0;
      for (int y = 8; y < World::WIN - 8 && made < 2; y += 2)
        for (int x = 8 + (y / 2) % 2; x < World::WIN - 8 && made < 2; x += 2) {
          if (!road(x, y) || g.world.siteAt(x, y, 10) >= 0) continue;
          roads++;
          for (int dir = 0; dir < 2 && made < 2; dir++) {
            const int nx = x + (dir == 0 ? 2 : 0), ny = y + (dir == 1 ? 2 : 0);
            if (!road(nx, ny)) continue;
            const ew::Gid a = ownerAt(x, y), b = ownerAt(nx, ny);
            if (a == b) continue;
            pairs++;
            if (warDbg()) std::printf("war: border %d,%d: %llu (war %d) / %llu (war %d)\n", x, y, (unsigned long long)a, (int)warring(a), (unsigned long long)b, (int)warring(b));
            if (!warring(a) && !warring(b)) continue;
            // on the warring side's road tile: barricades on the tiles across the road (perpendicular to it)
            const ew::Gid k = warring(b) ? b : a;
            int cx = k == b ? nx : x, cy = k == b ? ny : y;
            {   // a bridge's checkpoint stands where it meets the land: on along the road into the warring side, to the first
                // tile with ground (not water) beside it
              const int sdx = (dir == 0 ? 1 : 0) * (k == b ? 1 : -1), sdy = (dir == 1 ? 1 : 0) * (k == b ? 1 : -1);
              for (int step = 0; step < 8; step++) {
                const int qx = cx + sdx * step, qy = cy + sdy * step;
                if (!road(qx, qy)) break;
                const int s1x = dir == 1 ? qx - 1 : qx, s1y = dir == 1 ? qy : qy - 1;
                const int s2x = dir == 1 ? qx + 1 : qx, s2y = dir == 1 ? qy : qy + 1;
                const bool landBeside = (!groundWater(M.at(s1x, s1y)) && M.at(s1x, s1y) != Ground::Swamp) ||
                                        (!groundWater(M.at(s2x, s2y)) && M.at(s2x, s2y) != Ground::Swamp);
                if (landBeside && M.at(qx, qy) != Ground::Bridge) { cx = qx; cy = qy; break; }
              }
            }
            const bool horiz = road(cx - 1, cy) && road(cx + 1, cy);   // the road runs east-west here
            int placed = 0;
            WarCamp c;
            c.kind = CampKind::Checkpoint;
            c.kingdom = k; c.other = k == b ? a : b;
            c.gx = g.world.ox + cx; c.gy = g.world.oy + cy;
            B.kingdom = k;
            for (int side = -1; side <= 1; side += 2)
              for (int t = 1; t <= 3; t++) {
                const int bx = horiz ? cx : cx + side * t, by = horiz ? cy + side * t : cy;
                if (road(bx, by)) continue;
                if (fellable(g, bx, by) && !B.taken(bx, by)) B.setRaw(bx, by, 0);   // a tree by the road felled for it
                if (warDbg()) std::printf("war:  side %d,%d ground %d prop %d bldg %d free %d\n", bx, by, (int)M.at(bx, by), M.propAt(bx, by) - 1, M.bldgAt[(size_t)by * M.w + bx], (int)warFreeTile(g, bx, by));
                if (!warFreeTile(g, bx, by) || B.taken(bx, by)) break;
                B.put(bx, by, Prop::Barricade);
                c.posts.push_back({g.world.ox + (horiz ? bx + 1 : bx), g.world.oy + (horiz ? by : by + 1)});
                placed++;
                break;
              }
            if (!placed) {
              // a bridge (water either side) or a road hemmed in: the barrier stands on the road's outer lanes, the middle
              // kept open (never a wall across the way)
              std::vector<std::pair<int, int>> lane;
              for (int t = -3; t <= 3; t++) {
                const int bx = horiz ? cx : cx + t, by = horiz ? cy + t : cy;
                if (road(bx, by)) lane.push_back({bx, by});
              }
              if (lane.size() >= 3) {
                for (int e = 0; e < 2; e++) {
                  const auto& q = e == 0 ? lane.front() : lane.back();
                  if (B.taken(q.first, q.second) || M.propAt(q.first, q.second)) continue;
                  B.put(q.first, q.second, Prop::Barricade);
                  c.posts.push_back({g.world.ox + (horiz ? q.first + 1 : q.first), g.world.oy + (horiz ? q.second : q.second + 1)});
                  placed++;
                }
              }
            }
            if (placed) { g.war.camps.push_back(c); made++; g.war.checkpoints++; }
          }
        }
      if (warDbg()) std::printf("war: checkpoints: %d road samples, %d border pairs, %d made\n", roads, pairs, made);
    }
  }
  g.war.appliedSites = sites;
  const double ms = nowMs() - t0;
  g.war.applies++;
  g.war.applyMs += ms;
  g.war.worstApplyMs = std::max(g.war.worstApplyMs, ms);
}
