// Settlement dressing (TOWNS lane, VISION_PLAN 15.8): what makes a street lived in. Centrepieces on the squares, market
// stalls, lamps, kingdom banners, signposts where the roads arrive, yards and gardens, fields and pastures thinning out
// at the edge, the archetype's touch (quays, mine carts, palisades), old trees and flowers between the houses, the
// townsfolk and the watch; then the relief, the used mask and the building ids. See rpg/world/town_gen.h.
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <queue>
#include "rpg/world/town_gen.h"

namespace ew {
namespace town {

using art::Building;
using art::Prop;

namespace {
const int D4X[4] = {0, 1, -1, 0}, D4Y[4] = {1, 0, 0, -1};
bool tallProp(Prop p) {
  switch (p) {
    case Prop::Flowers1: case Prop::Flowers2: case Prop::Flowers3: case Prop::TallGrass: case Prop::Mushrooms:
    case Prop::Bush: case Prop::Barrel: case Prop::Crate: case Prop::Woodpile: case Prop::FenceH: case Prop::FenceV:
    case Prop::Haystack: case Prop::Anvil: case Prop::Rock:
    case Prop::Sacks: case Prop::Baskets: case Prop::OrePile: case Prop::LogPile: case Prop::Trough: case Prop::Filler: case Prop::Stump:
      return false;
    default: return true;
  }
}
}  // namespace

void Gen::addSpawn(Role r, int x, int y) {
  if (!in(x, y)) return;
  Spawn sp;
  sp.npc = true;
  sp.role = r;
  sp.site = -1;
  sp.slot = slot++;
  sp.x = x;
  sp.y = y;
  M.spawns.push_back(sp);
}

// a tile a yard prop may take: open land of the town, nobody's front, nothing standing there
bool Gen::freeTile(int x, int y) const {
  if (!in(x, y)) return false;
  const size_t i = I(x, y);
  if (mask[i] != K_NONE || M.bldgAt[i] >= 0 || M.wall[i] || M.prop[i] || front[i]) return false;
  if (!reserved.empty() && reserved[i]) return false;   // (M1 economy) a market keeper's walk or aisle
  Ground g = M.at(x, y);
  return !groundSolid(g) && g != Ground::Bridge && !water[i];
}

// ------------------------------------------------------------------------------------------------ squares
void Gen::centrepieces() {
  for (size_t k = 0; k < squares.size(); k++) {
    const Square& s = squares[k];
    if (!in(s.x, s.y) || get(s.x, s.y) != K_SQUARE) continue;
    float cq = hfAt(s.x, s.y, 3u);
    Prop centre;
    if (k == 0) {
      // (M1 fixer) a capital's great fountain; (M1 round 3) no oak among a town's stalls; (M1 fixer round 2) a market
      // cross in some towns and cities, where the market was granted
      centre = city ? (capital || cq < 0.45f ? Prop::Fountain : (cq < 0.72f ? Prop::MarketCross : Prop::Statue))
                    : town ? (cq < 0.35f ? Prop::Well : (cq < 0.65f || arch == Archetype::Market ? Prop::MarketCross : Prop::Fountain))
                           : Prop::Well;
      if (village && layout == 2 && cq < 0.5f) {
        // the village green's old oak, and the well beside it
        centre = Prop::OakTree;
        if (get(s.x - 2, s.y + 1) == K_SQUARE) M.setProp(s.x - 2, s.y + 1, Prop::Well);
        else if (get(s.x + 2, s.y + 1) == K_SQUARE) M.setProp(s.x + 2, s.y + 1, Prop::Well);
        else centre = Prop::Well;
      }
      if ((bio == Biome::Desert) && centre == Prop::OakTree) centre = Prop::PalmTree;
    } else {
      switch (s.d) {
        case District::Temple: centre = Prop::Statue; break;
        case District::Noble: centre = cq < 0.6f ? Prop::Fountain : Prop::OakTree; break;
        case District::Crafts: centre = Prop::Well; break;
        case District::Poor: centre = Prop::Well; break;
        default: centre = cq < 0.5f ? Prop::Well : Prop::Statue; break;
      }
      if (bio == Biome::Desert && centre == Prop::OakTree) centre = Prop::PalmTree;
      if ((bio == Biome::Snow || bio == Biome::Taiga) && centre == Prop::OakTree) centre = Prop::PineTree;
    }
    int px = s.x, py = s.y;
    if (k == 0 && !(village && layout == 2 && centre == Prop::OakTree)) {
      // (M1 economy) the market fills one side of the square: the fountain or the well stands toward the other side,
      // in a plaza of its own (M1 fixer: the market's side is mktSide)
      const int ddx = mktSide == 2 ? -1 : (mktSide == 3 ? 1 : 0), ddy = mktSide == 0 ? 1 : (mktSide == 1 ? -1 : 0);
      // (M1 fixer round 2) and off the square's axis by the town's own draw (a few tiles to one side), so no two
      // squares hold it in the same spot
      const int lat0 = village ? 0 : (int)(hashAt(s.x, s.y, 557u) % 7u) - 3;
      bool placed = false;
      for (int li = 0; li < 3 && !placed; li++) {
        const int lat = li == 0 ? lat0 : (li == 1 ? lat0 / 2 : 0);
        for (int d = town || city ? 3 : 1; d > 0; d--) {
          const int bx = s.x + ddx * d + (ddx ? 0 : lat), by = s.y + ddy * d + (ddx ? lat / 2 : 0);
          bool ok = true;
          for (int oy = -1; oy <= 2 && ok; oy++)
            for (int ox = -2; ox <= 2; ox++) {
              const int tx = bx + ox, ty = by + oy;
              // on the square, and never before a door (its step and apron: the basin's rim would shut it)
              if (get(tx, ty) != K_SQUARE || front[I(tx, ty)] || (in(tx, ty - 1) && front[I(tx, ty - 1)])) { ok = false; break; }
            }
          if (ok) { px = bx; py = by; placed = true; break; }
        }
      }
      cpX = px; cpY = py;
    }
    M.setProp(px, py, centre);
  }
  // villages always have their well (VISION_PLAN 15.8)
  if (village) {
    bool well = false;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) if (M.propAt(x, y) == (int)Prop::Well + 1) well = true;
    if (!well) {
      for (int r = 1; r < 8 && !well; r++)
        for (int oy = -r; oy <= r && !well; oy++)
          for (int ox = -r; ox <= r && !well; ox++)
            if (freeTile(cx + ox, cy + oy) && !cover[I(cx + ox, cy + oy)]) { M.setProp(cx + ox, cy + oy, Prop::Well); well = true; }
    }
  }
}

// a square tile a stall, bench or banner may stand on: the square itself, clear of every building's roof and front
bool squareSpot(const Gen& g, int x, int y, int pad) {
  for (int oy = -pad; oy <= pad; oy++)
    for (int ox = -pad; ox <= pad; ox++) {
      int tx = x + ox, ty = y + oy;
      if (!g.in(tx, ty) || g.M.prop[g.I(tx, ty)]) return false;
    }
  if (!g.reserved.empty() && g.reserved[g.I(x, y)]) return false;   // (M1 economy) a market aisle or a keeper's walk
  if (!g.in(x, y) || g.get(x, y) != K_SQUARE || g.cover[g.I(x, y)] || g.front[g.I(x, y)]) return false;
  // nor against a facade: a free row between a stall and the house behind it, none in front under a roof
  for (int oy = -2; oy <= 3; oy++)
    for (int ox = -1; ox <= 1; ox++) if (g.bldgAt(x + ox, y + oy) && g.in(x + ox, y + oy)) return false;
  return true;
}

void Gen::stallsAndLamps() {
  // (M1 economy) the market rows and the other squares' benches and trees: rpg/world/town_market.cpp
  markets();
  squareDress();
  // lampposts along the main streets (towns and cities) and by the squares
  if (!village) {
    int step = 0;
    for (auto& s : mainTiles) {
      if (++step % 9) continue;
      for (int k = 0; k < 4; k++) {
        static const int dx[4] = {2, -1, 0, 0}, dy[4] = {0, 0, 2, -1};
        int x = s.first + dx[k], y = s.second + dy[k];
        if (!freeTile(x, y) || cover[I(x, y)] || noBuild[I(x, y)]) continue;
        bool lampNear = false;
        for (int oy = -5; oy <= 5; oy++) for (int ox = -5; ox <= 5; ox++) if (M.propAt(x + ox, y + oy) == (int)Prop::Lamppost + 1) lampNear = true;
        if (lampNear) break;
        if (!putSolid(x, y, Prop::Lamppost)) continue;   // (M1 fixer) never across the only way past
        set(x, y, K_YARD);
        break;
      }
    }
  }
}

// ------------------------------------------------------------------------------------------------ banners and signs
void Gen::banners() {
  if (!C.kingdom && !capital) return;   // the wildlands fly nobody's colours
  auto put = [&](int x, int y) {
    if (!in(x, y) || M.prop[I(x, y)] || M.bldgAt[I(x, y)] >= 0 || M.wall[I(x, y)] || groundSolid(M.at(x, y)) || water[I(x, y)]) return false;
    if (cover[I(x, y)] || front[I(x, y)] || (!reserved.empty() && reserved[I(x, y)])) return false;
    M.setProp(x, y, Prop::Banner);
    if (get(x, y) == K_NONE) set(x, y, K_YARD);
    return true;
  };
  // the city gates: a banner either side of the road just inside every main gate
  for (size_t k = 0; k < O.wallGaps.size(); k++) {
    if (k >= gateBearing.size() || gateBearing[k] < 0) continue;
    const IRect& g = O.wallGaps[k];
    if (g.h == 1) {
      int dy = ins(g.x + 1, g.y - 1) ? -1 : 1;
      // clear of the gatehouse's towers, which rise three rows over the passage
      for (int s : {-3, 5}) put(g.x + s, g.y + dy * 4) || put(g.x + s, g.y + dy * 5) || put(g.x + s + (s < 0 ? -1 : 1), g.y + dy * 4);
    } else if (g.w == 1) {
      int dx = ins(g.x - 1, g.y + 1) ? -1 : 1;
      for (int s : {-2, 4}) put(g.x + dx * 3, g.y + s) || put(g.x + dx * 4, g.y + s);
    }
  }
  // the keep's door
  if (keepIdx >= 0) {
    const Bldg& b = M.bldgs[(size_t)keepIdx];
    for (int s : {-3, 3}) put(b.doorX() + s, b.r.y + b.r.h) || put(b.doorX() + s, b.r.y + b.r.h + 1);
  }
  // the inns fly the colours by their doors
  for (const Bldg& b : M.bldgs)
    if (b.type == Building::Inn) put(b.doorX() + 2, b.r.y + b.r.h) || put(b.doorX() - 2, b.r.y + b.r.h) || put(b.doorX() + 2, b.r.y + b.r.h + 1);
  // the main square's north edge
  if (!squares.empty()) {
    const Square& s = squares[0];
    int placed = 0;
    for (int y = s.y - (int)s.r - 1; y <= s.y && placed < 2; y++)
      for (int x = s.x - (int)(s.r * 1.25f); x <= s.x + (int)(s.r * 1.25f) && placed < 2; x++) {
        if (get(x, y) != K_SQUARE || get(x, y - 1) == K_SQUARE || std::abs(x - s.x) < 2) continue;
        if (placed == 1 && x <= s.x) continue;
        if (squareSpot(*this, x, y, 1)) { M.setProp(x, y, Prop::Banner); placed++; x += 3; }
      }
    // (M1 economy) the market's keepers have the north edge: the colours fly either side of the centrepiece instead
    // (M1 fixer round 2: not a matched pair flanking it: one close by, the other further out on the far side)
    const int px = cpX >= 0 ? cpX : s.x, py = cpX >= 0 ? cpY : s.y;
    const int first = (hashAt(px, py, 571u) & 1) ? 1 : -1;
    for (int sd : {first, -first})
      for (int t = sd == first ? 3 : 6; t <= (sd == first ? 5 : 9) && placed < 2; t++) {
        const int x = px + sd * t, y = py - (sd == first ? 1 : 0);
        if (squareSpot(*this, x, y, 1)) { M.setProp(x, y, Prop::Banner); placed++; break; }
      }
    // (M1 fixer) a crowded square: the colours fly at its edge wherever a pole fits
    for (int r = 2; r <= 14 && placed == 0; r++)
      for (int oy = -r; oy <= r && placed == 0; oy++)
        for (int ox = -r; ox <= r && placed == 0; ox++) {
          if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
          const int x = s.x + ox, y = s.y + oy;
          if (!in(x, y) || (get(x, y) != K_SQUARE && get(x, y) != K_YARD && get(x, y) != K_NONE)) continue;
          bool clear = true;
          for (int qy = -1; qy <= 1 && clear; qy++)
            for (int qx = -1; qx <= 1; qx++) if (!in(x + qx, y + qy) || M.prop[I(x + qx, y + qy)] || M.bldgAt[I(x + qx, y + qy)] >= 0) { clear = false; break; }
          if (clear && put(x, y)) placed++;
        }
  }
}

void Gen::signposts() {
  auto besideRoad = [&](int x, int y, int dx, int dy) {
    // the first free tile to the right of the road (looking out of town), else to the left
    for (int side : {1, -1})
      for (int k = 1; k <= 3; k++) {
        int tx = x - dy * side * k, ty = y + dx * side * k;
        if (freeTile(tx, ty) && !cover[I(tx, ty)]) return std::make_pair(tx, ty);
      }
    return std::make_pair(-1, -1);
  };
  if (walled) {
    for (size_t k = 0; k < O.wallGaps.size(); k++) {
      if (k >= gateBearing.size() || gateBearing[k] < 0) continue;
      const IRect& g = O.wallGaps[k];
      int gx = g.x + g.w / 2, gy = g.y + g.h / 2, dx = 0, dy = 0;
      if (g.h == 1) dy = ins(gx, gy - 1) ? 1 : -1;
      else if (g.w == 1) dx = ins(gx - 1, gy) ? 1 : -1;
      else continue;
      int x = gx + dx * 4, y = gy + dy * 4;
      auto p = besideRoad(x, y, dx, dy);
      if (p.first < 0) continue;
      M.setProp(p.first, p.second, Prop::Signpost);
      set(p.first, p.second, K_YARD);
      // the arrival banner on the other side of the road
      if (C.kingdom) {
        int bx = x + dy * 2, by = y - dx * 2;
        if (freeTile(bx, by) && !cover[I(bx, by)]) { M.setProp(bx, by, Prop::Banner); set(bx, by, K_YARD); }
      }
    }
  }
  // villages and towns: where each road leaves the houses behind
  for (size_t b = 0; b < bearings.size() && !walled; b++) {
    float ba = bearings[b];
    int bx = -1, by = -1;
    float bd = 1e9f;
    for (auto& s : mainTiles) {
      float d = dist(s.first, s.second);
      if (d < 1.0f || d > 1.2f) continue;
      float da = std::fabs(dwrap(angleOf(s.first, s.second) - ba));
      if (da < bd) { bd = da; bx = s.first; by = s.second; }
    }
    if (bx < 0 || bd > 0.6f) continue;
    int dx = std::fabs(dcos(ba)) > std::fabs(dsin(ba)) ? (dcos(ba) > 0 ? 1 : -1) : 0;
    int dy = dx ? 0 : (dsin(ba) > 0 ? 1 : -1);
    auto p = besideRoad(bx, by, dx, dy);
    if (p.first < 0) continue;
    M.setProp(p.first, p.second, Prop::Signpost);
    set(p.first, p.second, K_YARD);
    if (b == 0 && C.kingdom) {   // the arrival banner on the strongest road
      int qx = bx + dy * 2 * (p.first - bx > 0 || p.second - by > 0 ? -1 : 1), qy = by - dx * 2 * (p.first - bx > 0 || p.second - by > 0 ? -1 : 1);
      if (freeTile(qx, qy) && !cover[I(qx, qy)]) { M.setProp(qx, qy, Prop::Banner); set(qx, qy, K_YARD); }
    }
  }
  // every town has its signpost: if no road leaves past the houses (a quay town, a road into the sea), by the main
  // street at the edge of the houses
  bool any = false;
  for (int i = 0; i < W * H && !any; i++) if (M.prop[(size_t)i] == (int)Prop::Signpost + 1) any = true;
  for (float lim : {0.9f, 0.75f, 0.5f}) {
    if (any) break;
    for (auto& s : mainTiles) {
      if (dist(s.first, s.second) < lim) continue;
      auto p = besideRoad(s.first, s.second, 1, 0);
      if (p.first < 0) p = besideRoad(s.first, s.second, 0, 1);
      if (p.first < 0) continue;
      M.setProp(p.first, p.second, Prop::Signpost);
      set(p.first, p.second, K_YARD);
      any = true;
      break;
    }
  }
}

// ------------------------------------------------------------------------------------------------ yards and fields
void Gen::yards() {
  const uint32_t ys = bseed + 101u;
  for (size_t i = 0; i < M.bldgs.size(); i++) {
    const Bldg b = M.bldgs[i];
    if ((int)i == palaceIdx || (int)i == barracksIdx) continue;
    auto ok = [&](int x, int y) { return freeTile(x, y) && !inCompound(x, y, 1); };
    int side = rng.f() < 0.5f ? b.r.x - 1 : b.r.x + b.r.w;
    int fy = b.r.y + b.r.h - 1;
    const District d = districtAt(b.r.cx(), b.r.cy());
    if (b.type == Building::Smithy) { if (ok(side, fy)) { M.setProp(side, fy, Prop::Anvil); set(side, fy, K_YARD); } continue; }
    if (b.type == Building::Inn) {
      if (ok(side, fy)) { M.setProp(side, fy, Prop::Woodpile); set(side, fy, K_YARD); }
      if (ok(b.doorX() + 2, b.r.y + b.r.h)) M.setProp(b.doorX() + 2, b.r.y + b.r.h, Prop::Barrel);
      continue;
    }
    if (b.type == Building::Barracks) { if (ok(side, fy)) { M.setProp(side, fy, Prop::Crate); set(side, fy, K_YARD); } continue; }
    if (b.type == Building::Keep || b.type == Building::Temple || b.type == Building::Tower) continue;
    if (!townIsHome(b.type) && b.type != Building::Shop) continue;   // (M1 economy) the trades' yards: tradeYards
    float q = rng.f();
    bool gardens = !city || d == District::Noble || d == District::Temple;
    if (q < 0.35f && gardens) {
      // a fenced garden beside the house: flowers or crops behind a short fence
      int gx0 = side == b.r.x - 1 ? b.r.x - 3 : b.r.x + b.r.w;
      bool good = true;
      for (int y = b.r.y; y < b.r.y + b.r.h && good; y++) for (int x = gx0; x < gx0 + 3 && good; x++) if (!ok(x, y)) good = false;
      if (good) {
        // (M1 fixer round 2) fenced on its three open sides (the house wall the fourth), so it encloses its beds
        const int outer = side == b.r.x - 1 ? gx0 : gx0 + 2;
        for (int y = b.r.y; y < b.r.y + b.r.h; y++)
          for (int x = gx0; x < gx0 + 3; x++) {
            set(x, y, K_YARD);
            const bool edgeY = y == b.r.y + b.r.h - 1 || y == b.r.y;
            if (edgeY) M.setProp(x, y, Prop::FenceH);
            else if (x == outer) M.setProp(x, y, Prop::FenceV);
            else if (hashf(x, y, ys) < 0.6f) M.setProp(x, y, hashf(x, y, ys + 1) < 0.5f ? Prop::Flowers2 : (village ? Prop::Mushrooms : Prop::Flowers3));
            if (village && !edgeY && x != outer && hashf(x, y, ys + 2) < 0.5f) M.setG(x, y, Ground::Farmland);
          }
      }
    } else if (q < 0.65f) {
      if (ok(side, fy)) {
        Prop p = rng.f() < 0.5f ? Prop::Barrel : (rng.f() < 0.5f ? Prop::Crate : Prop::Woodpile);
        if (city && d == District::Poor && rng.f() < 0.4f) p = Prop::Crate;
        M.setProp(side, fy, p);
        set(side, fy, K_YARD);
      }
    } else if (q < 0.82f && (!city || d == District::Noble)) {
      int tx = side + (side < b.r.x ? -1 : 1), ty = b.r.y + rng.irange(b.r.h);
      if (ok(tx, ty) && ok(tx, ty + 1) && !cover[I(tx, ty)]) {
        Biome bb = M.biomeAt(tx, ty);
        Prop tree = bb == Biome::Taiga || bb == Biome::Snow ? Prop::PineTree : (bb == Biome::Autumn ? Prop::AutumnTree : (bb == Biome::Desert ? Prop::PalmTree : Prop::OakTree));
        M.setProp(tx, ty, tree);
      }
    }
  }
}

void Gen::fields() {
  int fields = city ? 8 + rng.irange(4) : (town ? 6 + rng.irange(3) : 4 + rng.irange(3));
  if (arch == Archetype::Farming || spec == Specialty::Farming) fields += village ? 4 : 5;
  if (spec == Specialty::Herding) fields += village ? 2 : 3;
  const uint32_t fs = bseed + 9u;
  for (int k = 0, t = 0; k < fields && t < 700; t++) {
    int fw = 5 + rng.irange(5), fh = 3 + rng.irange(3);
    int fx, fy;
    if (t % 3 != 2 && !mainTiles.empty()) {
      // most fields line the roads out of town, a hedge's width back from the road
      auto s = mainTiles[(size_t)rng.irange((int)mainTiles.size())];
      float d = dist(s.first, s.second);
      if (d < (city ? 1.0f : 0.8f) || d > 1.35f) continue;
      int side = rng.f() < 0.5f ? 1 : -1;
      if (rng.f() < 0.5f) { fx = s.first + (side > 0 ? 2 : -1 - fw); fy = s.second - fh / 2; }
      else { fx = s.first - fw / 2; fy = s.second + (side > 0 ? 2 : -1 - fh); }
    } else {
      float ang = rng.f() * D_TAU;
      float rr = city ? rng.range(1.0f, 1.12f) : rng.range(0.7f, 1.15f);
      fx = cx + (int)std::floor(dcos(ang) * rx * rr) - fw / 2; fy = cy + (int)std::floor(dsin(ang) * ry * rr) - fh / 2;
    }
    bool good = true;
    for (int y = fy - 1; y <= fy + fh && good; y++)
      for (int x = fx - 1; x <= fx + fw && good; x++) {
        if (!in(x, y) || x < 1 || y < 1 || x >= W - 1 || y >= H - 1) { good = false; break; }
        const size_t i = I(x, y);
        if (mask[i] != K_NONE || M.bldgAt[i] >= 0 || M.wall[i] || M.prop[i] || water[i] || groundSolid(M.at(x, y)) || M.at(x, y) == Ground::Bridge) good = false;
        else if (cover[i] || front[i]) good = false;
        else if (walled && blob(x, y, cx, cy, rx, ry, wseed) < wallR + 0.06f) good = false;
        else if (lvl[i] != lvl[I(fx, fy)]) good = false;
      }
    if (!good) continue;
    bool pasture = fh >= 4 && rng.f() < (spec == Specialty::Herding ? 0.75f : (arch == Archetype::Farming ? 0.4f : 0.3f));
    // (M1 fixer round 2) a pasture is fenced all round, a gate on the side toward the town and its beasts inside; a
    // field of crops is open to its headland (no stray run of fence along one side)
    int gateX = -1, gateY = -1;
    if (pasture) {
      const int mx = fx + fw / 2, my = fy + fh / 2;
      if (std::abs(cx - mx) * fh > std::abs(cy - my) * fw) { gateX = cx < mx ? fx : fx + fw - 1; gateY = my; }
      else { gateX = mx; gateY = cy < my ? fy : fy + fh - 1; }
    }
    for (int y = fy; y < fy + fh; y++)
      for (int x = fx; x < fx + fw; x++) {
        set(x, y, K_FIELD);
        M.setP(x, y, 0);
        if (!pasture) M.setG(x, y, Ground::Farmland);
        else if (x == gateX && y == gateY) continue;
        else if (y == fy || y == fy + fh - 1) M.setProp(x, y, Prop::FenceH);
        else if (x == fx || x == fx + fw - 1) M.setProp(x, y, Prop::FenceV);
      }
    if (!pasture) {
      if (freeTile(fx + fw, fy + 1)) M.setProp(fx + fw, fy + 1, Prop::Haystack);
      if (rng.f() < 0.5f && freeTile(fx - 1, fy + fh - 1)) M.setProp(fx - 1, fy + fh - 1, Prop::Cart);
    } else {
      M.setProp(fx + fw / 2, fy + 1, Prop::Haystack);
      if (fw >= 6) M.setProp(fx + 2, fy + fh - 2, Prop::Trough);   // (M1 economy) the beasts' water
      // the flock or the herd grazing, never on top of one another
      const bool cows = spec != Specialty::Herding ? hashf(fx, fy, fs + 3) < 0.6f : hashf(fx, fy, fs + 3) < 0.3f;
      const int beasts = std::max(2, (fw - 2) * (fh - 2) / 5);
      int put = 0;
      for (int t = 0; t < 40 && put < beasts; t++) {
        const int x = fx + 1 + rng.irange(fw - 2), y = fy + 1 + rng.irange(fh - 2);
        if (M.prop[I(x, y)]) continue;
        bool crowd = false;
        for (int oy = -1; oy <= 1; oy++)
          for (int ox = -1; ox <= 1; ox++) {
            const int q = M.propAt(x + ox, y + oy);
            if (q == (int)Prop::Sheep + 1 || q == (int)Prop::Cow + 1) crowd = true;
          }
        if (crowd) continue;
        M.setProp(x, y, cows && (t & 1) == 0 ? Prop::Cow : Prop::Sheep);
        put++;
      }
    }
    k++;
  }
}

// City yards and gardens: packed earth along the streets of the crowded quarters (no lawn where a city should be), and
// the open ground the houses left becomes kitchen gardens and allotments, orchards, and in the better quarters flower
// gardens with clipped hedges; in towns a few allotments and orchards among the houses
void Gen::gardens() {
  if (village) return;
  if (city)
    for (int y = 1; y < H - 1; y++)
      for (int x = 1; x < W - 1; x++) {
        const size_t i = I(x, y);
        if (mask[i] != K_NONE || M.bldgAt[i] >= 0 || M.wall[i] || water[i] || !ins(x, y)) continue;
        Ground g = M.at(x, y);
        if (g != Ground::Grass && g != Ground::Meadow) continue;
        District d = districtAt(x, y);
        if (d == District::Noble || d == District::Temple) continue;
        bool near = false;
        for (int oy = -1; oy <= 1 && !near; oy++)
          for (int ox = -1; ox <= 1; ox++) if (isStreet(x + ox, y + oy) || bldgAt(x + ox, y + oy) || get(x + ox, y + oy) == K_YARD) { near = true; break; }
        // (M1 round 3) a capital is paved: the ground along its streets and between its houses is cobbled right up to
        // the walls (lawns and earth blotches through the paving made the king's city read as a village); the
        // gardens below still take the open plots further from the street
        if (capital) {
          bool near2 = near;
          for (int oy = -2; oy <= 2 && !near2; oy++)
            for (int ox = -2; ox <= 2; ox++) if (isStreet(x + ox, y + oy)) { near2 = true; break; }
          if (near2) { M.setG(x, y, Ground::Road); continue; }
        }
        if (near && vnoise(x * 0.3f, y * 0.3f, bseed + 55u) < 0.6f) M.setG(x, y, Ground::Dirt);
      }
  const Biome gb = bio;
  const Prop fruit = gb == Biome::Snow || gb == Biome::Taiga ? Prop::PineTree : (gb == Biome::Desert ? Prop::PalmTree : (gb == Biome::Autumn ? Prop::AutumnTree : Prop::OakTree));
  const int want = city ? 60 : 8;
  // every spot on a coarse lattice, in a shuffled order (so the gardens are spread, not swept from one corner)
  std::vector<int> spots;
  for (int y = 2; y < H - 8; y += 2)
    for (int x = 2; x < W - 10; x += 3) spots.push_back(y * W + x);
  for (size_t i = spots.size(); i > 1; i--) std::swap(spots[i - 1], spots[(size_t)rng.irange((int)i)]);
  int placed = 0;
  for (size_t t = 0; t < spots.size() && placed < want; t++) {
    const int gw = 4 + rng.irange(5), gh = 3 + rng.irange(2);
    const int x0 = spots[t] % W, y0 = spots[t] / W;
    bool ok = true;
    for (int y = y0 - 1; y <= y0 + gh && ok; y++)
      for (int x = x0 - 1; x <= x0 + gw && ok; x++) {
        const size_t i = I(x, y);
        const bool inner = x >= x0 && x < x0 + gw && y >= y0 && y < y0 + gh;
        if (M.bldgAt[i] >= 0 || M.wall[i]) ok = false;   // the margin: nobody's house or wall
        if (!inner || !ok) continue;
        if (mask[i] != K_NONE || (!reserved.empty() && reserved[i])) ok = false;
        else if (M.prop[i] || water[i] || front[i]) ok = false;
        else if (cover[i]) ok = false;
        else if (noBuild[i]) ok = false;
        else if (lvl[i] != lvl[I(x0, y0)] || groundSolid(M.at(x, y)) || M.at(x, y) == Ground::Bridge) ok = false;
        else if (city ? !ins(x, y) : dist(x, y) > 0.85f) ok = false;
      }
    if (!ok) continue;
    const District d = districtAt(x0 + gw / 2, y0 + gh / 2);
    const bool fine = city && (d == District::Noble || d == District::Temple);
    const float q = rng.f();
    for (int y = y0; y < y0 + gh; y++) for (int x = x0; x < x0 + gw; x++) set(x, y, K_FIELD);
    if (fine && q < 0.7f) {
      // a flower garden: a clipped hedge round beds of flowers, a little lawn
      for (int y = y0; y < y0 + gh; y++)
        for (int x = x0; x < x0 + gw; x++) {
          M.setG(x, y, Ground::Meadow);
          bool edge = y == y0 || y == y0 + gh - 1 || x == x0 || x == x0 + gw - 1;
          if (edge) { if (((x + y) & 1) == 0 && !(y == y0 + gh - 1 && x == x0 + gw / 2)) M.setProp(x, y, Prop::Bush); }
          else if (((x - x0) % 2 == 1) && ((y - y0) % 2 == 1)) M.setProp(x, y, (x + y) % 3 == 0 ? Prop::Flowers1 : ((x + y) % 3 == 1 ? Prop::Flowers2 : Prop::Flowers3));
        }
    } else if (q < 0.45f) {
      // an orchard: fruit trees in rows on the grass (none whose crown would hide a doorstep)
      for (int y = y0; y < y0 + gh; y++)
        for (int x = x0; x < x0 + gw; x++) if (M.at(x, y) == Ground::Road) M.setG(x, y, Ground::Meadow);   // (a capital's paving)
      for (int y = y0 + 1; y < y0 + gh; y += 3)
        for (int x = x0 + 1; x < x0 + gw; x += 3) {
          bool clear = true;
          for (int oy = -4; oy <= 0 && clear; oy++) for (int ox = -1; ox <= 1; ox++) if (in(x + ox, y + oy) && front[I(x + ox, y + oy)]) { clear = false; break; }
          if (clear) M.setProp(x, y, fruit);
        }
      if (M.at(x0, y0 + gh - 1) == Ground::Grass) M.setG(x0, y0 + gh - 1, Ground::Meadow);
    } else {
      // an allotment: rows of crops between earth paths, a fence along the top, a woodpile or a barrel in a corner
      for (int y = y0; y < y0 + gh; y++)
        for (int x = x0; x < x0 + gw; x++) {
          M.setG(x, y, (y - y0) % 2 == 0 ? Ground::Farmland : Ground::Dirt);
          if (y == y0) M.setProp(x, y, Prop::FenceH);
        }
      M.setProp(x0 + gw - 1, y0 + gh - 1, rng.f() < 0.5f ? Prop::Woodpile : Prop::Barrel);
      if (rng.f() < 0.5f) M.setProp(x0, y0 + gh - 1, Prop::Haystack);
    }
    placed++;
  }
}

// ------------------------------------------------------------------------------------------------ archetypes
void Gen::archetypeDress() {
  if (arch == Archetype::Fishing || arch == Archetype::Port || spec == Specialty::Fishing) {
    // quays: plank piers out into the water from the shore nearest the streets, crates and barrels at their roots
    int piers = arch == Archetype::Port ? 3 : 2;
    std::vector<std::pair<int, int>> made;
    for (int t = 0; t < 400 && (int)made.size() < piers; t++) {
      int x = 2 + rng.irange(W - 4), y = 2 + rng.irange(H - 4);
      if (water[I(x, y)] || groundSolid(M.at(x, y)) || M.bldgAt[I(x, y)] >= 0 || M.wall[I(x, y)] || dist(x, y) > 1.15f) continue;
      if (get(x, y) == K_FIELD || get(x, y) == K_COMPOUND) continue;
      // (M1 fixer round 2) never rooted on something standing there (a stall's counter, a cart) or a market's aisle:
      // the lane painted to it would clear it away
      if (M.prop[I(x, y)] || (!reserved.empty() && reserved[I(x, y)])) continue;
      int dir = -1;
      for (int d = 0; d < 4; d++) if (in(x + D4X[d] * 3, y + D4Y[d] * 3) && water[I(x + D4X[d], y + D4Y[d])] && water[I(x + D4X[d] * 2, y + D4Y[d] * 2)]) dir = d;
      if (dir < 0) continue;
      bool far = true;
      for (auto& m : made) if (std::abs(m.first - x) + std::abs(m.second - y) < 10) far = false;
      for (const Bldg& b : M.bldgs)   // (M1 economy) no jetty across a watermill's race
        if (b.type == Building::Watermill && x >= b.r.x - 4 && x < b.r.x + b.r.w + 4 && y >= b.r.y - 3 && y < b.r.y + b.r.h + 3) far = false;
      if (!far) continue;
      // join the root to the streets
      std::vector<int> prev((size_t)W * H, -2);
      std::queue<int> q;
      prev[I(x, y)] = -1;
      q.push((int)I(x, y));
      int found = -1;
      while (!q.empty() && found < 0) {
        int c = q.front(); q.pop();
        int px = c % W, py = c / W;
        if (isStreet(px, py)) { found = c; break; }
        if (std::abs(px - x) + std::abs(py - y) > 16) continue;
        for (int d = 0; d < 4; d++) {
          int nx = px + D4X[d], ny = py + D4Y[d];
          if (!in(nx, ny) || prev[I(nx, ny)] != -2 || M.bldgAt[I(nx, ny)] >= 0 || M.wall[I(nx, ny)] || groundSolid(M.at(nx, ny)) || get(nx, ny) == K_FIELD) continue;
          if (M.prop[I(nx, ny)] && propSolid((Prop)(M.prop[I(nx, ny)] - 1))) continue;
          prev[I(nx, ny)] = c;
          q.push((int)I(nx, ny));
        }
      }
      if (found < 0) continue;
      for (int c = prev[(size_t)found]; c >= 0; c = prev[(size_t)c]) paintStreet(c % W, c / W, K_LANE, laneG);
      int len = 0;
      for (int k = 1; k <= 6; k++) {
        int px = x + D4X[dir] * k, py = y + D4Y[dir] * k;
        if (!in(px, py) || !water[I(px, py)] || M.wall[I(px, py)]) break;
        M.setG(px, py, Ground::Bridge);
        set(px, py, K_LANE);
        len++;
      }
      if (len < 2) continue;
      for (int s : {-1, 1}) {
        int bx = x + D4Y[dir] * s, by = y + D4X[dir] * s;
        if (freeTile(bx, by) && !cover[I(bx, by)]) M.setProp(bx, by, s < 0 ? Prop::Crate : Prop::Barrel);
      }
      made.push_back({x, y});
    }
  }
  if (arch == Archetype::Mining) {
    // ore carts and crates along the main streets at the edge, boulders where the hill was quarried
    int carts = 0;
    for (auto& s : mainTiles) {
      if (carts >= 4) break;
      float d = dist(s.first, s.second);
      if (d < 0.7f || d > 1.0f || hashAt(s.first, s.second, 71u) % 7u) continue;
      for (int k = 0; k < 4; k++) {
        int x = s.first + D4X[k] * 2, y = s.second + D4Y[k] * 2;
        if (freeTile(x, y) && !cover[I(x, y)]) { M.setProp(x, y, carts % 2 ? Prop::Crate : Prop::Cart); set(x, y, K_YARD); carts++; break; }
      }
    }
    for (int t = 0, n = 0; t < 200 && n < 5; t++) {
      float a = rng.f() * D_TAU, rr = rng.range(1.0f, 1.2f);
      int x = cx + (int)std::floor(dcos(a) * rx * rr), y = cy + (int)std::floor(dsin(a) * ry * rr);
      if (freeTile(x, y) && !cover[I(x, y)]) { M.setProp(x, y, rng.f() < 0.5f ? Prop::Boulder : Prop::Rock); n++; }
    }
  }
  if (arch == Archetype::Farming || spec == Specialty::Farming) {
    for (const Bldg& b : M.bldgs) {
      if (b.type != Building::Farmhouse) continue;
      for (int s : {-2, b.r.w + 1}) {
        int x = b.r.x + s, y = b.r.y + b.r.h - 1;
        if (freeTile(x, y) && !cover[I(x, y)]) { M.setProp(x, y, Prop::Haystack); set(x, y, K_YARD); }
      }
    }
  }
  if (arch == Archetype::RiverCrossing || arch == Archetype::Port) {
    // lamps at the bridge heads
    for (auto& s : mainTiles) {
      if (M.at(s.first, s.second) != Ground::Bridge) continue;
      for (int d = 0; d < 4; d++) {
        int x = s.first + D4X[d], y = s.second + D4Y[d];
        if (!in(x, y) || M.at(x, y) == Ground::Bridge || water[I(x, y)] || !isStreet(x, y)) continue;
        for (int sd : {-1, 1}) {
          int lx = x + D4Y[d] * sd * 2, ly = y + D4X[d] * sd * 2;
          bool near = false;
          for (int oy = -3; oy <= 3; oy++) for (int ox = -3; ox <= 3; ox++) if (M.propAt(lx + ox, ly + oy) == (int)Prop::Lamppost + 1) near = true;
          if (!near && freeTile(lx, ly) && !cover[I(lx, ly)]) { M.setProp(lx, ly, Prop::Lamppost); set(lx, ly, K_YARD); }
        }
      }
    }
  }
  if (palisade) palisadeRing();
}

// ------------------------------------------------------------------------------------------------ greenery and people
void Gen::greenery() {
  float treeP = city ? 0.010f : (town ? 0.028f : 0.055f);
  const uint32_t gs = bseed + 77u;
  for (int y = 1; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++) {
      if (!freeTile(x, y) || inCompound(x, y, 1)) continue;
      float b = blob(x, y, cx, cy, rx + 1.5f, ry + 1.5f, bseed);
      if (b > 1.0f) continue;
      if (walled && !ins(x, y) && b > 0.9f) continue;
      if (M.bldgAt[I(x, y - 1)] >= 0 || wallAt(x, y + 1) || wallAt(x, y - 1)) continue;   // keep faces and wall feet clear
      bool besideStreet = false;
      for (int d = 0; d < 4; d++) if (get(x + D4X[d], y + D4Y[d]) == K_MAIN) besideStreet = true;
      float r = hashf(x, y, gs);
      const Biome bb = M.biomeAt(x, y);
      bool nearStall = false;   // (M1 round 3) no crown spreading over a market stall
      for (int oy = -3; oy <= 1 && !nearStall; oy++)
        for (int ox = -2; ox <= 2; ox++) {
          const int q = in(x + ox, y + oy) ? M.prop[I(x + ox, y + oy)] : 0;
          if (q && (art::isVendorProp((Prop)(q - 1)) || (Prop)(q - 1) == Prop::MineEntrance || (Prop)(q - 1) == Prop::MineHill)) { nearStall = true; break; }
        }
      if (!reserved.empty() && reserved[I(x, y)]) continue;
      bool nearField = false;   // (M1 fixer round 2) no crown spreading over a fence, a pen or a field
      for (int oy = -3; oy <= 1 && !nearField; oy++)
        for (int ox = -2; ox <= 2; ox++) {
          const int q = in(x + ox, y + oy) ? M.prop[I(x + ox, y + oy)] : 0;
          if (get(x + ox, y + oy) == K_FIELD || q == (int)Prop::FenceH + 1 || q == (int)Prop::FenceV + 1) { nearField = true; break; }
        }
      if (nearField && r < treeP + 0.03f) continue;
      if (r < treeP && !besideStreet && !nearStall && !cover[I(x, y)] && !noBuild[I(x, y)]) {
        Prop tree = bb == Biome::Taiga ? Prop::PineTree : bb == Biome::Autumn ? Prop::AutumnTree : bb == Biome::Desert ? Prop::PalmTree
                  : (hashf(x, y, gs + 1) < 0.3f ? Prop::BirchTree : Prop::OakTree);
        if (bb == Biome::Snow) tree = Prop::SnowPine;
        M.setProp(x, y, tree);
      } else if (r < treeP + 0.03f) M.setProp(x, y, bb == Biome::Snow ? Prop::SnowBush : Prop::Bush);
      else if (r < treeP + 0.10f && bb != Biome::Snow && bb != Biome::Desert) M.setProp(x, y, hashf(x, y, gs + 2) < 0.5f ? Prop::TallGrass : Prop::Flowers1);
    }
  // nothing tall stands where a building's roof would swallow it or in front of a door
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      int p = M.prop[I(x, y)];
      if (!p || inCompound(x, y)) continue;
      if (front[I(x, y)] && get(x, y) != K_SQUARE) { M.setP(x, y, 0); continue; }
      if (cover[I(x, y)] && tallProp((Prop)(p - 1)) && (Prop)(p - 1) != Prop::Fountain && (Prop)(p - 1) != Prop::Well && M.bldgAt[I(x, y)] < 0) {
        Prop pp = (Prop)(p - 1);
        if (pp == Prop::Banner || pp == Prop::Lamppost || pp == Prop::Signpost || pp == Prop::MarketStall || pp == Prop::Statue || pp == Prop::DryingRack || pp == Prop::HideRack ||
            pp == Prop::OakTree || pp == Prop::BirchTree || pp == Prop::PineTree || pp == Prop::AutumnTree || pp == Prop::PalmTree || pp == Prop::SnowPine || pp == Prop::Cart)
          M.setP(x, y, 0);
      }
    }
}

void Gen::folk() {
  std::vector<std::pair<int, int>> spots;
  for (auto& s : allStreet)
    if (in(s.first, s.second) && !M.prop[I(s.first, s.second)] && !M.wall[I(s.first, s.second)] && M.at(s.first, s.second) != Ground::Bridge) spots.push_back(s);
  for (const Square& q : squares)
    for (int y = q.y - 4; y <= q.y + 4; y++)
      for (int x = q.x - 5; x <= q.x + 5; x++)
        if (get(x, y) == K_SQUARE && !M.prop[I(x, y)]) spots.push_back({x, y});
  int folkN = city ? 36 : (town ? 16 : 6);
  if (capital) folkN += 6;
  for (int i = 0; i < folkN && !spots.empty(); i++) {
    auto p = spots[(size_t)rng.irange((int)spots.size())];
    float r = rng.f();
    Role role = r < 0.15f ? Role::Child : ((village || arch == Archetype::Farming) && r < 0.35f ? Role::Farmer : Role::Villager);
    addSpawn(role, p.first, p.second);
  }
  // the watch: two inside every main gate, patrols on the main streets
  if (walled) {
    for (size_t k = 0; k < O.wallGaps.size(); k++) {
      if (k >= gateBearing.size() || gateBearing[k] < 0) continue;
      const IRect& g = O.wallGaps[k];
      int gx = g.x + g.w / 2, gy = g.y + g.h / 2;
      int dx = g.w == 1 ? (ins(gx - 1, gy) ? -1 : 1) : 0, dy = g.h == 1 ? (ins(gx, gy - 1) ? -1 : 1) : 0;
      if (!dx && !dy) dy = 1;
      for (int s : {-1, 1}) {
        int x = gx + dx * 2 + (dy ? s : 0), y = gy + dy * 2 + (dx ? s : 0);
        if (in(x, y) && !M.blocked(x, y) && !M.prop[I(x, y)]) addSpawn(Role::Guard, x, y);
      }
    }
  }
  int patrols = city ? 8 : (town ? 4 : 0);
  for (int i = 0; i < patrols && !mainTiles.empty(); i++) {
    auto p = mainTiles[(size_t)rng.irange((int)mainTiles.size())];
    if (!M.prop[I(p.first, p.second)] && !M.wall[I(p.first, p.second)]) addSpawn(Role::Guard, p.first, p.second);
  }
}

// ------------------------------------------------------------------------------------------------ the end
void Gen::finish() {
  // (M2 fixer round 3) a street prop (a lamp, a signpost, a banner) standing on a tile of grass in the middle of the
  // paving showed as a green tuft round its foot: its tile takes the paving most of its neighbours have
  for (int y = 1; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++) {
      const int pp = M.prop[I(x, y)];
      if (pp != (int)Prop::Lamppost + 1 && pp != (int)Prop::Signpost + 1 && pp != (int)Prop::Banner + 1) continue;
      const Ground g = M.at(x, y);
      if (g == Ground::Road || g == Ground::Plaza || g == Ground::Bridge || groundWater(g) || groundSolid(g)) continue;
      int road = 0, plaza = 0;
      for (int d = 0; d < 4; d++) {
        const Ground n = M.at(x + D4X[d], y + D4Y[d]);
        road += n == Ground::Road;
        plaza += n == Ground::Plaza;
      }
      if (road + plaza >= 3) M.setG(x, y, plaza > road ? Ground::Plaza : Ground::Road);
    }
  // relief: the base levels, and a ramp wherever a street steps between levels
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      uint8_t h = lvl[I(x, y)];
      if (isStreet(x, y) || get(x, y) == K_YARD)
        for (int d = 0; d < 4; d++) {
          int nx = x + D4X[d], ny = y + D4Y[d];
          if (in(nx, ny) && lvl[I(nx, ny)] != lvl[I(x, y)] && (isStreet(nx, ny) || get(nx, ny) == K_YARD)) h |= Map::HEIGHT_RAMP;
        }
      M.height[I(x, y)] = h;
    }
  // the used mask: the town's outline (inside the wall for cities) and everything the town touched, grown by a tile
  std::vector<uint8_t> u((size_t)W * H, 0);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const size_t i = I(x, y);
      bool t = mask[i] != K_NONE || M.bldgAt[i] >= 0 || M.wall[i] || M.prop[i];
      if (!t) t = walled ? ins(x, y) : blob(x, y, cx, cy, rx + 1.5f, ry + 1.5f, bseed) < 1.0f;
      u[i] = t ? 1 : 0;
    }
  // (M2 fixer round 3) the land round every gatehouse is the town's too (its drained ground, not the wild's): a wild
  // marsh pool left just outside the wall used to hug a gate tower (seeds 36 and 37)
  for (const auto& gt : O.gates)
    for (int y = gt.second - 3; y <= gt.second + 3; y++)
      for (int x = gt.first - 3; x <= gt.first + 5; x++)
        if (in(x, y)) u[I(x, y)] = 1;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      bool t = false;
      for (int oy = -1; oy <= 1 && !t; oy++) for (int ox = -1; ox <= 1; ox++) if (in(x + ox, y + oy) && u[I(x + ox, y + oy)]) { t = true; break; }
      O.used[I(x, y)] = t ? 1 : 0;
    }
  M.rebuildSolid();
  const uint32_t cap = P.bldgCap ? P.bldgCap : 4096u;
  O.homes = 0;
  for (size_t i = 0; i < M.bldgs.size(); i++) {
    Bldg& b = M.bldgs[i];
    b.id = makeId(C.rx, C.ry, IdKind::Bldg, (uint32_t)P.bldgBase + std::min((uint32_t)i, cap - 1));
    b.site = -1;
    if (townIsHome(b.type)) O.homes++;
  }
  O.ex = O.gx + cx;
  O.ey = O.gy + cy;
  O.special = spec;
}

void Gen::run() {
  while (step()) {}
}

// One phase of the build per call (false once the town is finished). The order is run()'s and must never change: the
// phases share one Rng, so the web's phase-at-a-time build (EndlessSource::prepareChunk) makes the same town.
bool Gen::step() {
  switch (phase++) {
    case 0: land(); break;
    case 1: pickBearings(); if (city) pickDistricts(); if (capital) placeCompound(); break;
    case 2: squaresPass(); mainStreets(); ringRoads(); break;
    case 3: fabric(); lanes(); break;
    case 4: if (walled) cityWall(); break;
    case 5: if (capital) buildCompound(); pruneStreets(); if (capital) compoundApproach(); break;
    case 6: {
      // nothing is built on a wall's foot (one tile round every wall piece, the compound's too)
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
          if (!M.wall[I(x, y)]) continue;
          for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if (in(x + ox, y + oy)) noBuild[I(x + ox, y + oy)] |= 1;
        }
      // and nothing is built (or roofed) in a gate's approach
      for (const IRect& g : O.wallGaps) {
        IRect z = g.h == 1 ? IRect{g.x - 2, g.y - 4, g.w + 4, 9} : (g.w == 1 ? IRect{g.x - 4, g.y - 3, 9, g.h + 5} : IRect{g.x - 3, g.y - 4, 9, 10});
        for (int y = z.y; y < z.y + z.h; y++)
          for (int x = z.x; x < z.x + z.w; x++) if (in(x, y)) noBuild[I(x, y)] |= 2;
      }
      marketLots();
      services();
      break;
    }
    case 7: homes(); break;
    case 8: centrepieces(); stallsAndLamps(); archetypeDress(); break;
    case 9: yards(); tradeYards(); gardens(); break;
    case 10: fields(); banners(); signposts(); break;
    case 11: greenery(); plazaFill(); break;
    case 12: {
      // (M2 fixer) a settlement in the snow: the builders' summer greenery (flower beds and tufts, leafy shrubs and
      // trees, lawns) is wintered: evergreens and snowy shrubs stand, flowers and grass tufts are under the snow, lawns
      // are snow (they stood on white ground as summer art pasted on a snowfield)
      if (bio == Biome::Snow)
        for (int y = 0; y < H; y++)
          for (int x = 0; x < W; x++) {
            const size_t i = I(x, y);
            const Ground g = (Ground)M.ground[i];
            if (g == Ground::Grass || g == Ground::Meadow || g == Ground::ForestFloor) M.ground[i] = (uint8_t)Ground::Snow;
            const int pr = M.prop[i];
            if (!pr) continue;
            switch ((Prop)(pr - 1)) {
              case Prop::Flowers1: case Prop::Flowers2: case Prop::Flowers3: case Prop::TallGrass: case Prop::Mushrooms: case Prop::Fern:
                M.prop[i] = 0; break;
              case Prop::Bush: case Prop::BerryBush: M.prop[i] = (uint8_t)((int)Prop::SnowBush + 1); break;
              case Prop::OakTree: case Prop::OakTree2: case Prop::BirchTree: case Prop::AutumnTree: case Prop::WillowTree:
              case Prop::PineTree: case Prop::PineTree2: M.prop[i] = (uint8_t)((int)Prop::SnowPine + 1); break;
              default: break;
            }
          }
      folk(); squareFolk(); finish(); return false;
    }
    default: return false;
  }
  return true;
}

}  // namespace town
}  // namespace ew
