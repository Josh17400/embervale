// Settlement dressing (TOWNS lane, VISION_PLAN 15.8): what makes a street lived in. Centrepieces on the squares, market
// stalls, lamps, kingdom banners, signposts where the roads arrive, yards and gardens, fields and pastures thinning out
// at the edge, the archetype's touch (quays, mine carts, palisades), old trees and flowers between the houses, the
// townsfolk and the watch; then the relief, the used mask and the building ids. See rpg/world/town_gen.h.
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>
#include "rpg/world/gen.h"
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
  if (face(x, y)) return false;                          // (M3) nothing stands on a terrace's face
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
      if (village && ((layout == 2 && cq < 0.5f) || centreKind == 3)) {
        // the village green's old oak, and the well beside it
        centre = Prop::OakTree;
        if (get(s.x - 2, s.y + 1) == K_SQUARE) M.setProp(s.x - 2, s.y + 1, Prop::Well);
        else if (get(s.x + 2, s.y + 1) == K_SQUARE) M.setProp(s.x + 2, s.y + 1, Prop::Well);
        else centre = Prop::Well;
      }
      if (centre == Prop::OakTree) centre = townTree(bio, eco, (uint32_t)(s.x * 7 + s.y));
      // M3: the culture's centrepiece (art::PropStyle::centre): 0 fountain, 1 statue, 2 well, 3 sacred tree, 4 fire
      // bowl, 5 obelisk (a statue the arch lane paints as one), 6 a ring of standing stones. A village keeps its well
      // (its sacred tree stands on the green with the well beside it); a royal seat's well is a fountain
      if (centreKind >= 0 && !village) {
        static const Prop kCentre[7] = {Prop::Fountain, Prop::Statue, Prop::Well, Prop::OakTree, Prop::Brazier, Prop::Statue, Prop::StandingStone};
        centre = kCentre[std::min(centreKind, 6)];
        if (capital && centre == Prop::Well) centre = Prop::Fountain;
      }
      if (centre == Prop::OakTree) centre = townTree(bio, eco, (uint32_t)(s.x * 7 + s.y));
    } else {
      switch (s.d) {
        case District::Temple: centre = Prop::Statue; break;
        case District::Noble: centre = cq < 0.6f ? Prop::Fountain : Prop::OakTree; break;
        case District::Crafts: centre = Prop::Well; break;
        case District::Poor: centre = Prop::Well; break;
        default: centre = cq < 0.5f ? Prop::Well : Prop::Statue; break;
      }
      if (centre == Prop::OakTree) centre = townTree(bio, eco, (uint32_t)(s.x * 7 + s.y));
    }
    int px = s.x, py = s.y;
    if (k == 0 && !(village && art::isTreeProp(centre))) {
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
    if (k == 0 && centre == Prop::StandingStone) {
      // the ring of stones: six round the square's middle, which stays open (the gathering place)
      // (M3 fixer round 3, review: "the hex of 2-tile-tall menhirs collapsed into two columns of three, crowding the
      // stalls") a wider, uneven ring, every stone in its own column so the tall menhirs never stack into a pillar
      // row, and none stands against a stall or another prop (it keeps a tile of open ground round it)
      static const int ro[6][2] = {{-3, 0}, {-1, -2}, {2, -2}, {3, 1}, {1, 2}, {-2, 2}};
      for (auto& o : ro) {
        const int tx = px + o[0], ty = py + o[1];
        if (get(tx, ty) != K_SQUARE || M.prop[I(tx, ty)] || front[I(tx, ty)]) continue;
        bool clear = true;
        for (int oy = -1; oy <= 1 && clear; oy++)
          for (int ox = -1; ox <= 1; ox++) {
            if (!in(tx + ox, ty + oy)) continue;
            const int q = M.prop[I(tx + ox, ty + oy)];
            if (q && q != (int)Prop::StandingStone + 1) { clear = false; break; }
          }
        if (clear) putSolid(tx, ty, Prop::StandingStone);
      }
      continue;
    }
    M.setProp(px, py, centre);
    // a village of a fire-keeping people keeps its fire bowl on the green beside the well (or the sacred tree)
    if (k == 0 && village && (centreKind == 4 || centreKind == 6))
      for (int ox : {3, -3, 2, -2})
        if (get(px + ox, py + 1) == K_SQUARE && !M.prop[I(px + ox, py + 1)] && !front[I(px + ox, py + 1)] &&
            putSolid(px + ox, py + 1, centreKind == 4 ? Prop::Brazier : Prop::StandingStone)) break;
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
    if (q < 0.35f * std::min(1.6f, treesF) && gardens) {   // (M3: a green-fingered people fences more gardens)
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
            // (M3c fixer round 3, review: "solid lime-green rectangles with two dark square holes ... no volume") beside a
            // house two tiles deep both rows were the fence's top and bottom runs, and a hedge laid two rows deep merged
            // into a flat green slab with the pieces' inner corners as holes: there the bed is fenced only along its
            // front and its outer side (an L against the house wall)
            const bool edgeY = y == b.r.y + b.r.h - 1 || (y == b.r.y && b.r.h >= 3);
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
        Prop tree = townTree(bb, eco, (uint32_t)(tx * 13 + ty * 7));
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
    bool pasture = fh >= 4 && rng.f() < (spec == Specialty::Herding || cArch == (int)cult::Archetype::Steppe ? 0.8f : (arch == Archetype::Farming ? 0.4f : 0.3f));
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
      // (M3c fixer round 2, review: "a haystack and a pile of rocks jammed against the gate tower's foot") the field's
      // haystack and cart stand on its headland only well clear of the town wall and its gate towers (2 tiles)
      auto clearOfWall = [&](int x, int y) {
        for (int oy = -2; oy <= 2; oy++)
          for (int ox = -2; ox <= 2; ox++)
            if (in(x + ox, y + oy) && M.wall[I(x + ox, y + oy)]) return false;
        return true;
      };
      if (freeTile(fx + fw, fy + 1) && clearOfWall(fx + fw, fy + 1)) M.setProp(fx + fw, fy + 1, Prop::Haystack);
      if (rng.f() < 0.5f && freeTile(fx - 1, fy + fh - 1) && clearOfWall(fx - 1, fy + fh - 1)) M.setProp(fx - 1, fy + fh - 1, Prop::Cart);
    } else {
      // (M3c fixer) the haystack stands in the inner corner farthest from the gate: on the tile just inside a north
      // gate it sealed the pen
      const int ix = gateX == fx ? fx + 1 : gateX == fx + fw - 1 ? fx + fw - 2 : gateX;
      const int iy = gateY == fy ? fy + 1 : gateY == fy + fh - 1 ? fy + fh - 2 : gateY;
      {
        int hx = fx + 1, hy = fy + 1, best = -1;
        const int cxs[2] = {fx + 1, fx + fw - 2}, cys[2] = {fy + 1, fy + fh - 2};
        for (int a = 0; a < 2; a++)
          for (int b = 0; b < 2; b++) {
            const int d = std::abs(cxs[a] - gateX) + std::abs(cys[b] - gateY);
            if (d > best) { best = d; hx = cxs[a]; hy = cys[b]; }
          }
        M.setProp(hx, hy, Prop::Haystack);
      }
      if (fw >= 6 && !(fx + 2 == ix && fy + fh - 2 == iy) && !M.prop[I(fx + 2, fy + fh - 2)])
        M.setProp(fx + 2, fy + fh - 2, Prop::Trough);   // (M1 economy) the beasts' water
      // the flock or the herd grazing, never on top of one another
      const bool cows = spec != Specialty::Herding ? hashf(fx, fy, fs + 3) < 0.6f : hashf(fx, fy, fs + 3) < 0.3f;
      const int beasts = std::max(2, (fw - 2) * (fh - 2) / 5);
      int put = 0;
      for (int t = 0; t < 40 && put < beasts; t++) {
        const int x = fx + 1 + rng.irange(fw - 2), y = fy + 1 + rng.irange(fh - 2);
        if (M.prop[I(x, y)] || (x == ix && y == iy)) continue;   // never on the tile just inside the gate
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
          // (M3) but not up against the wall: a strip of grass along its foot (no paving runs into it like a road)
          if (near2 && !(noBuild[i] & 1)) { M.setG(x, y, Ground::Road); continue; }
        }
        if (near && vnoise(x * 0.3f, y * 0.3f, bseed + 55u) < 0.6f) M.setG(x, y, Ground::Dirt);
      }
  const Biome gb = bio;
  const Prop fruit = townTree(gb, eco, 1u);
  const int want = (int)((city ? 60 : 8) * treesF + 0.5f);   // (M3: TownStyle::trees)
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
// ------------------------------------------------------------------------------------------------ M3b society spaces
// An open patch of w x h tiles the town has left (no street, yard, house, wall, prop, water or terrace face; nobody's
// front or sprite; one level), between nearD and farD from the heart (in a city's district d when it is given), with a
// street within three tiles of its south side for the way in. The candidates are scanned in a hashed order (k), so two
// spaces of one town are not stacked in one corner. Integer summed-area table: O(1) per candidate.
bool Gen::findOpen(int w, int h, float nearD, float farD, int& ox, int& oy, uint32_t k, District d) const {
  const int SW = W + 1;
  std::vector<int> sat((size_t)SW * (H + 1), 0);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const size_t i = I(x, y);
      const bool open = mask[i] == K_NONE && M.bldgAt[i] < 0 && !M.wall[i] && !M.prop[i] && !front[i] && !cover[i] && !noBuild[i] && !water[i] &&
                        !groundSolid(M.at(x, y)) && M.at(x, y) != Ground::Bridge && M.at(x, y) != Ground::Swamp && !face(x, y) &&
                        (reserved.empty() || !reserved[i]);
      sat[(size_t)(y + 1) * SW + x + 1] = (open ? 1 : 0) + sat[(size_t)y * SW + x + 1] + sat[(size_t)(y + 1) * SW + x] - sat[(size_t)y * SW + x];
    }
  auto area = [&](int x0, int y0, int x1, int y1) {   // [x0, x1) x [y0, y1)
    return sat[(size_t)y1 * SW + x1] - sat[(size_t)y0 * SW + x1] - sat[(size_t)y1 * SW + x0] + sat[(size_t)y0 * SW + x0];
  };
  int best = -1;
  uint32_t bestH = 0;
  for (int y = 2; y + h + 2 < H; y++)
    for (int x = 2; x + w + 2 < W; x++) {
      const float dd = dist(x + w / 2, y + h / 2);
      if (dd < nearD || dd > farD) continue;
      if (city && d != District::COUNT && districtAt(x + w / 2, y + h / 2) != d) continue;
      if (area(x - 1, y - 1, x + w + 1, y + h + 1) != (w + 2) * (h + 2)) continue;   // (a free ring round it too)
      const int lv = lvl[I(x, y)];
      bool flat = true;
      for (int yy = y; yy < y + h && flat; yy++)
        for (int xx = x; xx < x + w; xx++) if (lvl[I(xx, yy)] != lv) { flat = false; break; }
      if (!flat) continue;
      bool road = false;
      for (int yy = y + h + 1; yy <= y + h + 3 && !road; yy++)
        for (int xx = x; xx < x + w; xx++) if (isStreet(xx, yy) || get(xx, yy) == K_YARD) { road = true; break; }
      if (!road) continue;
      const uint32_t hh = hashAt(x, y, k);
      if (best < 0 || hh < bestH) { best = y * W + x; bestH = hh; }
    }
  if (best < 0) return false;
  ox = best % W; oy = best / W;
  return true;
}

// a ring of props round (x, y) at radius r (every `every` steps of the circle; keepSouth: a gap at its south for the way
// in); false when fewer than half of them found room
bool Gen::spaceRing(int x, int y, int r, Prop p, int every, bool keepSouth) {
  const int n = std::max(6, (int)(D_TAU * r / std::max(1, every)));
  int put = 0, want = 0;
  for (int k = 0; k < n; k++) {
    const float a = D_TAU * k / n;
    if (keepSouth && dsin(a) > 0.85f) continue;
    const int px = x + (int)std::floor(dcos(a) * r + 0.5f), py = y + (int)std::floor(dsin(a) * r * 0.8f + 0.5f);
    want++;
    if (freeTile(px, py) && !cover[I(px, py)] && putSolid(px, py, p)) { set(px, py, K_YARD); put++; }
  }
  return put * 2 >= want;
}

// M3b (VISION_PLAN 15.14): the open spaces the society asks for (cult::requiredSpaces), made from the props the world
// already has: a moot ring of standing stones (benches where no stone-raising people meets), the horse lines' corrals
// (the culture's fence, beasts, troughs, hay), the parade ground's packed earth with its standards, the temple court
// paved before the chief temple with braziers, a sacred grove's ring of old trees round its shrine. The market square,
// the green, the quays and a capital's gardens are the layout's own (markets, the village green, quays, the seat).
void Gen::spaces() {
  spacesMade = spacesAsked = 0;
  if (!hasSoc) return;
  const std::vector<cult::SpaceReq> reqs = cult::requiredSpaces(soc, *C.culture, tier, (int)P.archetype, P.seed);
  const bool stones = cArch == (int)cult::Archetype::Fjordfolk || cArch == (int)cult::Archetype::Highland || cArch == (int)cult::Archetype::Marsh ||
                      cArch == (int)cult::Archetype::Steppe || cArch == (int)cult::Archetype::Sylvan;
  const bool cold = bio == Biome::Snow || bio == Biome::Taiga;
  const bool dry = bio == Biome::Desert || cArch == (int)cult::Archetype::Dune;
  uint32_t k = 0x5ACEu;
  for (const cult::SpaceReq& q : reqs) {
    k += 0x9E37u;
    bool made = false;
    switch (q.space) {
      case cult::Space::MarketSquare: made = !stalls.empty() || mktZone.w > 0 || lotStalls > 0 || !squares.empty(); break;
      case cult::Space::Green: made = !squares.empty(); break;
      case cult::Space::Harbour: made = arch == Archetype::Port; break;
      case cult::Space::Gardens: made = compound.w > 0 && seatKind != (int)cult::Seat::TentCourt && seatKind != (int)cult::Seat::StiltHall; break;
      case cult::Space::TempleCourt: {
        if (capital && seatKind == (int)cult::Seat::TempleComplex && compound.w > 0) { made = true; break; }
        // the court before the chief temple: its apron widened into a paved court, braziers at its corners
        for (const Bldg& b : M.bldgs) {
          if (b.type != Building::Temple || !(b.civic & bld::CIVIC_SACRED)) continue;
          const int x0 = b.r.x - 1, x1 = b.r.x + b.r.w, y0 = b.r.y + b.r.h, y1 = b.r.y + b.r.h + 3;
          int paved = 0;
          for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
              if (!in(x, y) || M.bldgAt[I(x, y)] >= 0 || M.wall[I(x, y)] || water[I(x, y)] || groundSolid(M.at(x, y)) || lvl[I(x, y)] != lvl[I(b.doorX(), b.doorY())]) continue;
              const uint8_t m = get(x, y);
              if (m != K_NONE && m != K_YARD && m != K_LANE && m != K_SQUARE) continue;
              if (M.prop[I(x, y)] && m == K_NONE) M.setP(x, y, 0);
              if (m == K_NONE || m == K_YARD) { set(x, y, K_SQUARE); M.setG(x, y, Ground::Plaza); }
              paved++;
            }
          for (int s : {x0, x1}) if (in(s, y0 + 1) && get(s, y0 + 1) == K_SQUARE && !M.prop[I(s, y0 + 1)] && !front[I(s, y0 + 1)]) putSolid(s, y0 + 1, Prop::Brazier);
          made = paved >= (b.r.w + 2) * 2;
          break;
        }
        break;
      }
      case cult::Space::MootRing: {
        int ox, oy;
        if (findOpen(9, 7, village ? 0.45f : 0.55f, 1.2f, ox, oy, k)) {
          const int mx = ox + 4, my = oy + 3;
          for (int y = oy; y < oy + 7; y++) for (int x = ox; x < ox + 9; x++) if ((x - mx) * (x - mx) * 9 + (y - my) * (y - my) * 16 <= 160) M.setP(x, y, 0);
          made = spaceRing(mx, my, 4, stones ? Prop::StandingStone : Prop::Bench, 2, !stones);
          if (made && freeTile(mx, my)) putSolid(mx, my, stones ? Prop::StandingStone : Prop::Statue);   // the speaker's stone
          for (int y = oy; y < oy + 7; y++) for (int x = ox; x < ox + 9; x++) if (get(x, y) == K_NONE && (x - mx) * (x - mx) * 9 + (y - my) * (y - my) * 16 <= 160) set(x, y, K_YARD);
        }
        break;
      }
      case cult::Space::SacredGrove: {
        int ox, oy;
        if (findOpen(11, 9, 0.35f, 1.15f, ox, oy, k, District::Temple) || findOpen(11, 9, 0.35f, 1.15f, ox, oy, k)) {
          const int mx = ox + 5, my = oy + 4;
          const Prop t1 = cold ? Prop::PineTree : (dry ? Prop::PalmTree : townTree(bio, eco, 0u)), t2 = cold ? Prop::SnowPine : (dry ? Prop::PalmTree : townTree(bio, eco, 1u));
          int n = 0;
          for (int a = 0; a < 10; a++) {
            const float an = D_TAU * a / 10;
            if (dsin(an) > 0.9f) continue;   // the way in from the south
            const int px = mx + (int)std::floor(dcos(an) * 5 + 0.5f), py = my + (int)std::floor(dsin(an) * 4 + 0.5f);
            if (freeTile(px, py) && !cover[I(px, py)] && putSolid(px, py, a & 1 ? t2 : t1)) n++;
          }
          if (n >= 6 && freeTile(mx, my)) { putSolid(mx, my, Prop::Shrine); made = true; }
          for (int y = oy + 1; y < oy + 8; y++)
            for (int x = ox + 1; x < ox + 10; x++) {
              if (!freeTile(x, y) || (std::abs(x - mx) <= 1 && std::abs(y - my) <= 1)) continue;
              const uint32_t h = hashAt(x, y, k ^ 0x6A0Fu);
              if (h % 7u == 0) M.setProp(x, y, dry ? Prop::Flowers3 : Prop::Fern);
              else if (h % 7u == 1) M.setProp(x, y, Prop::Flowers2);
            }
          for (int y = oy; y < oy + 9; y++) for (int x = ox; x < ox + 11; x++) if (get(x, y) == K_NONE) set(x, y, K_YARD);
        }
        break;
      }
      case cult::Space::Corral: {
        int ox, oy;
        const int cw = village ? 6 : 8, ch = village ? 5 : 6;
        if (findOpen(cw, ch, 0.55f, 1.25f, ox, oy, k)) {
          const int gapX = ox + cw / 2;
          for (int x = ox; x < ox + cw; x++) {
            M.setProp(x, oy, Prop::FenceH);
            if (x != gapX) M.setProp(x, oy + ch - 1, Prop::FenceH);
          }
          for (int y = oy + 1; y < oy + ch - 1; y++) { M.setProp(ox, y, Prop::FenceV); M.setProp(ox + cw - 1, y, Prop::FenceV); }
          for (int y = oy; y < oy + ch; y++) for (int x = ox; x < ox + cw; x++) { set(x, y, K_YARD); if (!M.prop[I(x, y)]) M.setG(x, y, Ground::Dirt); }
          M.setProp(ox + 1, oy + 1, Prop::Trough);
          M.setProp(ox + cw - 2, oy + 1, Prop::Haystack);
          const Prop beast = cArch == (int)cult::Archetype::Steppe || dry ? Prop::Cow : Prop::Sheep;
          M.setProp(ox + 2, oy + ch - 3, beast);
          if (cw >= 8) M.setProp(ox + cw - 3, oy + ch - 3, beast);
          made = true;
        }
        break;
      }
      case cult::Space::ParadeGround: {
        int ox, oy;
        const int pw = 11, ph = 7;
        if (findOpen(pw, ph, 0.4f, 1.1f, ox, oy, k, District::Noble) || findOpen(pw, ph, 0.4f, 1.2f, ox, oy, k)) {
          for (int y = oy; y < oy + ph; y++) for (int x = ox; x < ox + pw; x++) { set(x, y, K_YARD); M.setG(x, y, Ground::Dirt); M.setP(x, y, 0); }
          for (int c = 0; c < 4; c++) putSolid(c & 1 ? ox + pw - 1 : ox, c & 2 ? oy + ph - 1 : oy, Prop::Banner);
          putSolid(ox + pw / 2, oy, Prop::Banner);
          for (int x = ox + 2; x < ox + pw - 2; x += 3) putSolid(x, oy, (x / 3) & 1 ? Prop::Crate : Prop::Barrel);
          made = true;
        }
        break;
      }
      default: break;
    }
    if (q.required) { spacesAsked++; if (made) spacesMade++; }
  }
}

void Gen::archetypeDress() {
  bool anyWater = false;
  for (size_t i = 0; i < water.size() && !anyWater; i++) anyWater = water[i] != 0;
  const bool linearShore = style == cult::Layout::Linear && anyWater;
  if (linearShore) quays();
  if (arch == Archetype::Fishing || arch == Archetype::Port || spec == Specialty::Fishing || linearShore) {
    // quays: plank piers out into the water from the shore nearest the streets, crates and barrels at their roots
    int piers = arch == Archetype::Port ? 3 : 2;
    if (linearShore) piers += city ? 2 : 1;   // (M3: a town strung along its shore or its river lives off the water)
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
      // (M3) how far it runs, and never along a watermill (no plank within two tiles of one: its wheel turns there)
      int len = 0;
      bool nearMill = false;
      for (int k = 1; k <= 6; k++) {
        const int px = x + D4X[dir] * k, py = y + D4Y[dir] * k;
        if (!in(px, py) || !water[I(px, py)] || M.wall[I(px, py)]) break;
        for (const Bldg& b : M.bldgs)
          if (b.type == Building::Watermill && px >= b.r.x - 2 && px < b.r.x + b.r.w + 2 && py >= b.r.y - 2 && py < b.r.y + b.r.h + 2) nearMill = true;
        len++;
      }
      if (nearMill) continue;
      // (fix) a jetty never runs up to the town wall (across a stream that hugs the ring it read as a street running
      // head-on into the masonry): it must end over open water, a tile clear of any wall
      {
        bool wallBy = false;
        for (int k = 1; k <= len + 1 && !wallBy; k++)
          for (int d = 0; d < 4 && !wallBy; d++) {
            const int px = x + D4X[dir] * k + D4X[d], py = y + D4Y[dir] * k + D4Y[d];
            if (in(px, py) && M.wall[I(px, py)]) wallBy = true;
          }
        if (wallBy) continue;
      }
      for (int k = 1; k <= len; k++) {
        const int px = x + D4X[dir] * k, py = y + D4Y[dir] * k;
        M.setG(px, py, Ground::Bridge);
        set(px, py, K_LANE);
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

// (M3) Linear by the water: the bank where the town meets the river or the shore is a paved quay (a strip of paving one
// tile wide along the water, where a street or a yard comes near it), with a crate, a barrel or a coil of rope now and
// then; the piers go out from it
void Gen::quays() {
  int n = 0;
  for (int y = 2; y < H - 2; y++)
    for (int x = 2; x < W - 2; x++) {
      const size_t i = I(x, y);
      if (mask[i] != K_NONE || water[i] || M.bldgAt[i] >= 0 || M.wall[i] || M.prop[i] || front[i]) continue;
      if (groundSolid(M.at(x, y)) || M.at(x, y) == Ground::Bridge) continue;
      if (walled ? !ins(x, y) : dist(x, y) > 0.95f) continue;
      bool bank = false;
      for (int d = 0; d < 4; d++) if (water[I(x + D4X[d], y + D4Y[d])] && groundWater(M.at(x + D4X[d], y + D4Y[d]))) bank = true;
      if (!bank) continue;
      bool near = false;
      for (int oy = -2; oy <= 2 && !near; oy++)
        for (int ox = -2; ox <= 2; ox++) {
          const uint8_t k = get(x + ox, y + oy);
          if (k == K_MAIN || k == K_LANE || k == K_SQUARE || k == K_YARD) { near = true; break; }
        }
      if (!near) continue;
      set(x, y, K_YARD);
      M.setG(x, y, Ground::Plaza);
      if ((++n % 7) == 0 && !cover[i]) putSolid(x, y, (n / 7) % 2 ? Prop::Crate : Prop::Barrel);
    }
}

// ------------------------------------------------------------------------------------------------ greenery and people
void Gen::greenery() {
  // (M3 stilt) the marsh between the stilt houses: reeds in the swamp, lily pads on the pools (both walked over or
  // floating: nobody's way is shut)
  if (stilt)
    for (int y = 1; y < H - 1; y++)
      for (int x = 1; x < W - 1; x++) {
        const size_t i = I(x, y);
        if (mask[i] != K_NONE || M.prop[i] || M.bldgAt[i] >= 0 || water[i] || front[i]) continue;
        const Ground g = M.at(x, y);
        const float r = hashf(x, y, bseed + 91u);
        if (g == Ground::Swamp && r < 0.28f) M.setProp(x, y, Prop::Reeds);
        else if (g == Ground::Water && r < 0.07f) M.setProp(x, y, Prop::LilyPad);
      }
  float treeP = (city ? 0.010f : (town ? 0.028f : 0.055f)) * treesF;   // (M3: TownStyle::trees)
  const uint32_t gs = bseed + 77u;
  for (int y = 1; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++) {
      if (!freeTile(x, y) || inCompound(x, y, 1)) continue;
      float b = blob(x, y, cx, cy, rx + 1.5f, ry + 1.5f, bseed);
      if (b > 1.0f) continue;
      if (walled && !ins(x, y) && b > 0.9f) continue;
      if (M.bldgAt[I(x, y - 1)] >= 0 || wallAt(x, y + 1) || wallAt(x, y - 1)) continue;   // keep faces and wall feet clear
      // (M3c fixer round 3, review: "snow-capped shrubs sit on the crest of the long diagonal wall") a diagonal run's
      // tiles touch this one at its sides and corners, and its crest stands a tile or two over them: nothing grows
      // within a tile of a wall on any side, nor two rows under one
      {
        bool nearWall = false;
        for (int oy = -1; oy <= 2 && !nearWall; oy++)
          for (int ox = -1; ox <= 1; ox++) if (wallAt(x + ox, y + oy)) { nearWall = true; break; }
        if (nearWall) continue;
      }
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
        Prop tree = ecoFamily(eco) == bb ? townTree(bb, eco, (uint32_t)(x * 31 + y * 17))
                  : bb == Biome::Taiga ? Prop::PineTree : bb == Biome::Autumn ? Prop::AutumnTree : bb == Biome::Desert ? Prop::PalmTree
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
            art::isTreeProp(pp) || pp == Prop::Cart)
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
void Gen::joinBanks() {
  M.rebuildSolid();
  auto cliff = [&](int x, int y) { return !M.height.empty() && (M.height[I(x, y)] & Map::HEIGHT_CLIFF); };
  for (int pass = 0; pass < 8; pass++) {
    // what the heart reaches on foot
    std::vector<uint8_t> seen((size_t)W * H, 0);
    std::vector<int> q;
    for (int oy = -3; oy <= 3; oy++)
      for (int ox = -3; ox <= 3; ox++) {
        const int x = cx + ox, y = cy + oy;
        if (in(x, y) && !M.blocked(x, y) && !seen[I(x, y)]) { seen[I(x, y)] = 1; q.push_back((int)I(x, y)); }
      }
    for (size_t h = 0; h < q.size(); h++) {
      const int x = q[h] % W, y = q[h] / W;
      for (int d = 0; d < 4; d++) {
        const int nx = x + D4X[d], ny = y + D4Y[d];
        if (!in(nx, ny) || seen[I(nx, ny)] || M.blocked(nx, ny)) continue;
        seen[I(nx, ny)] = 1;
        q.push_back((int)I(nx, ny));
      }
    }
    if (q.empty()) return;
    // the first door it does not reach
    int tx = -1, ty = -1;
    for (const Bldg& b : M.bldgs) {
      const int ax = b.doorX(), ay = b.r.y + b.r.h;
      if (in(ax, ay) && !M.blocked(ax, ay) && !seen[I(ax, ay)]) { tx = ax; ty = ay; break; }
    }
    if (tx < 0) return;
    // the cheapest way from it to the reached ground: dry land costs 1, water 3 (so a bridge goes straight across);
    // never through a building, a wall, a cliff face or the buffer's rim. (M3b round 3) A bridge never turns over the
    // water: the search state is the tile and the heading, and a step onto or across water keeps the heading it had,
    // so every crossing is one straight deck (a diagonal stream was crossed by a staircase of square plank patches)
    std::vector<int> cost((size_t)W * H * 4, 1 << 30), prev((size_t)W * H * 4, -1);
    std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, std::greater<std::pair<int, int>>> pq;
    for (int d = 0; d < 4; d++) { cost[I(tx, ty) * 4 + (size_t)d] = 0; pq.push({0, (int)I(tx, ty) * 4 + d}); }
    int found = -1;
    while (!pq.empty()) {
      const auto [c0, s0] = pq.top();
      pq.pop();
      if (c0 != cost[(size_t)s0]) continue;
      const int i0 = s0 / 4, d0 = s0 % 4;
      if (seen[(size_t)i0]) { found = s0; break; }
      const int x = i0 % W, y = i0 / W;
      const bool wet = groundWater(M.at(x, y)) && !(x == tx && y == ty);
      for (int d = 0; d < 4; d++) {
        if (wet && d != d0) continue;   // on the water: straight on only
        const int nx = x + D4X[d], ny = y + D4Y[d];
        if (nx < 1 || ny < 1 || nx >= W - 1 || ny >= H - 1) continue;
        const size_t ni = I(nx, ny);
        if (M.bldgAt[ni] >= 0 || M.wall[ni] || cliff(nx, ny)) continue;
        const Ground g = M.at(nx, ny);
        if (groundSolid(g) && !groundWater(g)) continue;
        const int step = groundWater(g) ? 3 : 1;
        const size_t ns = ni * 4 + (size_t)d;
        if (c0 + step < cost[ns]) { cost[ns] = c0 + step; prev[ns] = s0; pq.push({c0 + step, (int)ns}); }
      }
    }
    if (found < 0) return;
    // lay it: a bridge over the water, a path on the land (whatever stood in the way is cleared)
    for (int st = prev[(size_t)found]; st >= 0; st = prev[(size_t)st]) {
      const int i = st / 4, x = i % W, y = i / W;
      const Ground g = M.at(x, y);
      M.setP(x, y, 0);
      if (groundWater(g)) M.setG(x, y, Ground::Bridge);
      else if (g != Ground::Road && g != Ground::Plaza && g != Ground::Bridge && g != Ground::Dirt) M.setG(x, y, laneG);
      if (get(x, y) == K_NONE || get(x, y) == K_FIELD) set(x, y, K_LANE);
    }
    M.rebuildSolid();
  }
}

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
  // (M3 terraced) the town cut its own terraces: the row under a higher neighbour is a cliff face (the terrace's
  // retaining wall, not walkable), stairs (a ramp) wherever a street, a path or a yard crosses it; nothing stands on
  // a face
  if (terraced) {
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        const size_t i = I(x, y);
        uint8_t h = lvl[i];
        bool face = false;
        for (int d = 0; d < 4; d++) if (in(x + D4X[d], y + D4Y[d]) && lvl[I(x + D4X[d], y + D4Y[d])] > lvl[i]) face = true;
        if (face) {
          bool way = isStreet(x, y) || get(x, y) == K_YARD;
          // (M3c) the wild land in the town's buffer (natural relief beyond its outline) takes the chunks' ramp lattice
          // (every 8 tiles along a face, as round any settlement's terraces), so a knoll at the town's edge is never
          // walled in where the town's relief meets the land's
          if (!way && get(x, y) == K_NONE && C.worldSeed) {
            const int32_t gx = O.gx + x, gy = O.gy + y;
            const uint64_t rs = C.worldSeed ^ tag("relief.ramp");
            const bool ns = (in(x, y - 1) && lvl[I(x, y - 1)] > lvl[i]) || (in(x, y + 1) && lvl[I(x, y + 1)] > lvl[i]);
            if (ns) way = ((gx + (int32_t)(ew::gen::tileHash(rs, 0, gy >> 4) & 15)) & 7) < 3;
            else way = ((gy + (int32_t)(ew::gen::tileHash(rs, 1, gx >> 4) & 15)) & 7) < 3;
          }
          h |= way ? Map::HEIGHT_RAMP : Map::HEIGHT_CLIFF;
          if (!way && !M.wall[i] && M.prop[i]) M.prop[i] = 0;
        }
        M.height[i] = h;
      }
    // (M3c) every terrace reachable: a shelf no street crosses was walled in by its faces. Flood the town's walkable
    // relief (no cliff face, no water) from its edge; a shelf left out gets stairs: the first face tile beside it (scan
    // order) and up to one face tile either side of it along the face become a ramp, and the flood goes on from there.
    {
      auto passable = [&](int x, int y) {
        const size_t i = I(x, y);
        return !(M.height[i] & Map::HEIGHT_CLIFF) && !groundWater((Ground)M.ground[i]);
      };
      std::vector<uint8_t> seen((size_t)W * H, 0);
      std::vector<int> st;
      auto flood = [&]() {
        for (size_t h = 0; h < st.size(); h++) {
          const int k = st[h], x = k % W, y = k / W;
          for (int d = 0; d < 4; d++) {
            const int nx = x + D4X[d], ny = y + D4Y[d];
            if (!in(nx, ny) || seen[I(nx, ny)] || !passable(nx, ny)) continue;
            seen[I(nx, ny)] = 1;
            st.push_back((int)I(nx, ny));
          }
        }
        st.clear();
      };
      for (int x = 0; x < W; x++)
        for (int y : {0, H - 1})
          if (passable(x, y) && !seen[I(x, y)]) { seen[I(x, y)] = 1; st.push_back((int)I(x, y)); }
      for (int y = 0; y < H; y++)
        for (int x : {0, W - 1})
          if (passable(x, y) && !seen[I(x, y)]) { seen[I(x, y)] = 1; st.push_back((int)I(x, y)); }
      flood();
      for (int guard = 0; guard < 64; guard++) {
        // a cliff face tile beside the reached land with an unreached walkable tile beside it too
        int fx = -1, fy = -1;
        for (int y = 1; y < H - 1 && fx < 0; y++)
          for (int x = 1; x < W - 1 && fx < 0; x++) {
            if (!(M.height[I(x, y)] & Map::HEIGHT_CLIFF) || groundWater((Ground)M.ground[I(x, y)])) continue;
            bool reached = false, cut = false;
            for (int d = 0; d < 4; d++) {
              const int nx = x + D4X[d], ny = y + D4Y[d];
              if (!passable(nx, ny)) continue;
              if (seen[I(nx, ny)]) reached = true; else cut = true;
            }
            if (reached && cut) { fx = x; fy = y; }
          }
        if (fx < 0) break;
        // the stairs: this face tile and its face neighbours along the row (a face running east-west) or column
        const bool ew = (M.height[I(fx - 1, fy)] & Map::HEIGHT_CLIFF) || (M.height[I(fx + 1, fy)] & Map::HEIGHT_CLIFF);
        for (int k = -1; k <= 1; k++) {
          const int sx = ew ? fx + k : fx, sy = ew ? fy : fy + k;
          if (!in(sx, sy) || !(M.height[I(sx, sy)] & Map::HEIGHT_CLIFF)) continue;
          M.height[I(sx, sy)] = (uint8_t)((M.height[I(sx, sy)] & ~Map::HEIGHT_CLIFF) | Map::HEIGHT_RAMP);
          if (!M.wall[I(sx, sy)] && M.prop[I(sx, sy)] && M.bldgAt[I(sx, sy)] < 0) M.prop[I(sx, sy)] = 0;
          if (!seen[I(sx, sy)]) { seen[I(sx, sy)] = 1; st.push_back((int)I(sx, sy)); }
        }
        flood();
      }
    }
    // a fence the faces cut short (a lone post left on its own) goes too
    auto fenceAt = [&](int x, int y) { const int q = M.propAt(x, y); return q == (int)Prop::FenceH + 1 || q == (int)Prop::FenceV + 1; };
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        if (!fenceAt(x, y)) continue;
        bool nb = false;
        for (int oy = -1; oy <= 1 && !nb; oy++)
          for (int ox = -1; ox <= 1; ox++) if ((ox || oy) && fenceAt(x + ox, y + oy)) { nb = true; break; }
        if (!nb) M.prop[I(x, y)] = 0;
      }
  } else
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
  // (M3 fixer) the culture's paving on the town's own tiles (Map::PAVE_MARK): the view lays its squares and streets in
  // it (sun-baked brick, packed earth, slate, moss-grown flags...). A stilt town's boardwalks over its marsh are marked
  // as such (Map::BOARDWALK_MARK), so the view draws them as one walkway on posts, not as river bridges.
  {
    const uint8_t pave = (uint8_t)(C.culture ? std::min<int>(C.culture->town.paving, 15) : 0);
    // (M3 fixer round 3, review: "oak trees grow straight out of plank boardwalk squares") where a people decks its
    // squares and streets in planks (paving 4, the marsh), no tree stands on the deck
    // (M3c fixer round 3, review: "the plank boardwalks are built from axis-aligned rectangles stepped in staircase runs,
    // with short dead-end stubs and notches") a decked town's walkways run on as one deck: the inner corner of every
    // step (open ground with the deck on two orthogonal sides and the diagonal between them) is decked too, so a
    // diagonal street is a continuous two-plank-wide walk with chamfered corners instead of a chain of blocks touching
    // at their corners; a one-tile stub off a walk with nothing at its end (no door, gate or quay) is taken up
    if (pave == 4) {
      auto deck = [&](int x, int y) { if (!in(x, y)) return false; const Ground g = M.at(x, y); return (g == Ground::Road || g == Ground::Plaza) && !water[I(x, y)]; };
      auto openG = [&](int x, int y) {
        if (!in(x, y)) return false;
        const size_t i = I(x, y);
        const Ground g = M.at(x, y);
        return (g == Ground::Grass || g == Ground::Meadow || g == Ground::Dirt || g == Ground::Swamp || g == Ground::Tundra || g == Ground::Sand) &&
               !water[i] && !M.prop[i] && M.bldgAt[i] < 0 && !M.wall[i] && !front[i];
      };
      std::vector<int> fill;
      for (int y = 1; y < H - 1; y++)
        for (int x = 1; x < W - 1; x++) {
          if (!openG(x, y)) continue;
          for (int k = 0; k < 4; k++) {
            const int sx = (k & 1) ? 1 : -1, sy = (k & 2) ? 1 : -1;
            if (deck(x + sx, y) && deck(x, y + sy) && deck(x + sx, y + sy) && lvl[I(x + sx, y)] == lvl[I(x, y)] && lvl[I(x, y + sy)] == lvl[I(x, y)]) {
              fill.push_back(y * W + x);
              break;
            }
          }
        }
      for (int i : fill) M.setG(i % W, i / W, Ground::Road);
      std::vector<int> stubs;
      for (int y = 1; y < H - 1; y++)
        for (int x = 1; x < W - 1; x++) {
          const size_t i = I(x, y);
          if (M.at(x, y) != Ground::Road || water[i] || front[i] || M.prop[i] || mask[i] == K_SQUARE) continue;
          int n = 0, dir = -1;
          for (int d = 0; d < 4; d++) if (deck(x + D4X[d], y + D4Y[d]) || M.at(x + D4X[d], y + D4Y[d]) == Ground::Bridge) { n++; dir = d; }
          if (n != 1) continue;
          // the tile beyond its open end: a door, a building, a gate or the water keeps it
          const int bx = x - D4X[dir], by = y - D4Y[dir];
          bool keep = !in(bx, by) || M.bldgAt[I(bx, by)] >= 0 || M.wall[I(bx, by)] || water[I(bx, by)] || front[I(bx, by)];
          for (int d = 0; d < 4 && !keep; d++) if (in(x + D4X[d], y + D4Y[d]) && (M.bldgAt[I(x + D4X[d], y + D4Y[d])] >= 0 || front[I(x + D4X[d], y + D4Y[d])])) keep = true;
          if (!keep) stubs.push_back((int)i);
        }
      for (int i : stubs) {
        const int x = i % W, y = i / W;
        Ground g = Ground::Grass;
        for (int d = 0; d < 4; d++) if (openG(x + D4X[d], y + D4Y[d])) { g = M.at(x + D4X[d], y + D4Y[d]); break; }
        M.setG(x, y, g);
      }
    }
    bool felled = false;
    if (pave == 4)
      for (size_t i = 0; i < M.prop.size(); i++) {
        const Ground g = (Ground)M.ground[i];
        if (g != Ground::Plaza && g != Ground::Road && g != Ground::Bridge) continue;
        if (M.prop[i] && art::isTreeProp((Prop)(M.prop[i] - 1))) { M.prop[i] = 0; felled = true; }
      }
    if (felled) M.rebuildSolid();
    M.blend.assign((size_t)W * H, 0);
    for (size_t i = 0; i < M.blend.size(); i++) {
      if (!O.used[i]) continue;
      const bool board = stilt && M.ground[i] == (uint8_t)Ground::Bridge && !water[i];
      M.blend[i] = (uint8_t)((board ? Map::BOARDWALK_MARK : Map::PAVE_MARK) | pave);
    }
    // (M3b fixer) the grounds' formal water: kerbed in cut stone (the view), not a pond's banks
    for (int i : O.pools)
      if (i >= 0 && (size_t)i < M.blend.size() && groundWater((Ground)M.ground[(size_t)i])) M.blend[(size_t)i] = (uint8_t)(Map::POOL_MARK | pave);
  }
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
    case 1: pickBearings(); if (city) pickDistricts(); if (walled) wallRing(); if (capital) placeCompound(); break;
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
    case 7: homesBegin(); break;
    case 8: if (homesSweep()) phase--; break;   // (M3: a slice a call, until the sweep is done)
    case 9: homesFinish(); break;
    case 10: centrepieces(); stallsAndLamps(); archetypeDress(); spaces(); break;
    case 11: yards(); tradeYards(); gardens(); break;
    case 12: fields(); banners(); signposts(); break;
    case 13: greenery(); break;
    case 14: plazaFill(); break;
    case 15: {
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
              case Prop::Wildflowers: case Prop::PrairieGrass: case Prop::Heather: case Prop::Petals: case Prop::JungleFern: case Prop::SilverFern:
                M.prop[i] = 0; break;
              case Prop::Bush: case Prop::BerryBush: M.prop[i] = (uint8_t)((int)Prop::SnowBush + 1); break;
              case Prop::OakTree: case Prop::OakTree2: case Prop::BirchTree: case Prop::AutumnTree: case Prop::WillowTree:
              case Prop::PineTree: case Prop::PineTree2: case Prop::LarchTree: case Prop::BlossomTree: case Prop::SilverTree: case Prop::JuniperTree:
              case Prop::AcaciaTree: M.prop[i] = (uint8_t)((int)Prop::SnowPine + 1); break;
              default: break;
            }
          }
      joinBanks(); folk(); squareFolk(); finish(); return false;
    }
    default: return false;
  }
  return true;
}

}  // namespace town
}  // namespace ew
