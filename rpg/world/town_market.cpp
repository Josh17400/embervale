// Settlement economy (M1, owner 2026-10-05; rpg/world/economy.h, VISION_PLAN 15.8): what a settlement lives from and how
// you can see it. See rpg/world/town_gen.h.
//  - the specialisation (farming, fishing, mining, lumber, herding) from the region plan, else from the archetype and
//    the land; its production buildings (mill, granary, fishmonger, smelter, sawmill, tannery) and, in towns and
//    cities, the full set of trades (bakery, butcher, weaver...);
//  - the market, laid out the way real ones are: stalls in tidy rows, every counter facing the walking space with an
//    aisle before it and its keeper behind it (stall facings, owner 2026-10-05: a row along the north side faces south,
//    the row across the aisle from it faces north so we see its back, a short row down a side stands in profile facing
//    in; a street's stalls face the street; a village's face its well), the stalls touching in groups with a walkway
//    between the groups, crates, sacks and baskets stacked at the row ends; never a ring round the fountain, never on a
//    street, never in front of a door (every door and street the town could reach before a stall went up is still
//    reached after). Villages get one or two stalls and a cart on the green; towns a cluster; cities a market place;
//    capitals a grand one;
//  - the other squares: benches, a tree, a lamp (no filler stalls);
//  - the trades' yards: the mine and its ore carts, log piles, drying racks, hide frames, troughs, sacks.
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
bool lowProp(Prop p) {
  switch (p) {
    case Prop::Flowers1: case Prop::Flowers2: case Prop::Flowers3: case Prop::TallGrass: case Prop::Mushrooms:
    case Prop::Bush: case Prop::Barrel: case Prop::Crate: case Prop::Woodpile: case Prop::FenceH: case Prop::FenceV:
    case Prop::Haystack: case Prop::Anvil: case Prop::Rock: case Prop::Sacks: case Prop::Baskets: case Prop::OrePile:
    case Prop::LogPile: case Prop::Trough: case Prop::Stump: case Prop::Filler:
      return true;
    default: return false;
  }
}
bool treeLike(Prop p) {
  return p == Prop::OakTree || p == Prop::OakTree2 || p == Prop::PineTree || p == Prop::PineTree2 || p == Prop::SnowPine ||
         p == Prop::BirchTree || p == Prop::WillowTree || p == Prop::PalmTree || p == Prop::AutumnTree;
}
// the stall a settlement's own trade keeps
int tradeOf(Specialty s) {
  switch (s) {
    case Specialty::Fishing: return (int)StallTrade::Fish;
    case Specialty::Mining: return (int)StallTrade::Tools;     // ingots worked into picks, nails and pots
    case Specialty::Lumber: return (int)StallTrade::Timber;    // planks, firewood, a log or two
    case Specialty::Herding: return (int)StallTrade::Meat;
    default: return (int)StallTrade::Produce;
  }
}
// what stands in the yard of a trade's stall row end
Prop stockFor(int trade, uint32_t h) {
  switch ((StallTrade)trade) {
    case StallTrade::Produce: return h & 1 ? Prop::Baskets : Prop::Crate;
    case StallTrade::Bread: return Prop::Sacks;
    case StallTrade::Timber: return Prop::Woodpile;
    case StallTrade::Fish: case StallTrade::Meat: return h & 1 ? Prop::Barrel : Prop::Crate;
    default: return h & 1 ? Prop::Crate : Prop::Barrel;
  }
}
}  // namespace

// ------------------------------------------------------------------------------------------------ the specialisation
void Gen::pickSpecialty() {
  spec = P.special;
  if (spec != Specialty::None) return;
  switch (arch) {
    case Archetype::Fishing: case Archetype::Port: spec = Specialty::Fishing; break;
    case Archetype::Mining: spec = Specialty::Mining; break;
    case Archetype::Farming: spec = Specialty::Farming; break;
    case Archetype::HillFort: spec = Specialty::Herding; break;
    case Archetype::RiverCrossing: spec = (P.seed >> 5) & 1 ? Specialty::Fishing : Specialty::Farming; break;
    default:
      spec = bio == Biome::Forest || bio == Biome::Taiga || bio == Biome::Autumn ? Specialty::Lumber
           : (bio == Biome::Desert || bio == Biome::Snow ? Specialty::Herding : Specialty::Farming);
      break;
  }
}

bool Gen::walkable(int x, int y) const {
  if (!in(x, y)) return false;
  const size_t i = I(x, y);
  if (M.bldgAt[i] >= 0 || M.wall[i] || groundSolid(M.at(x, y))) return false;
  if (M.prop[i] && propSolid((Prop)(M.prop[i] - 1))) return false;
  // (M3 terraced) a terrace's face (the row under a higher neighbour) is a retaining wall, unless a street climbs it
  if (terraced && !isStreet(x, y) && get(x, y) != K_YARD)
    for (int d = 0; d < 4; d++) {
      static const int fx[4] = {0, 1, -1, 0}, fy[4] = {1, 0, 0, -1};
      if (in(x + fx[d], y + fy[d]) && lvl[I(x + fx[d], y + fy[d])] > lvl[i]) return false;
    }
  // a fountain's basin also blocks the tiles beside it and its back rim the tile above (Map::rebuildSolid)
  const int fz = (int)Prop::Fountain + 1;
  if (M.propAt(x - 1, y) == fz || M.propAt(x + 1, y) == fz || M.propAt(x, y + 1) == fz) return false;
  return true;
}

// a river or lake (not the sea) runs inside the town's outline
static bool riverInTown(const Gen& g) {
  for (int y = 1; y < g.H - 1; y++)
    for (int x = 1; x < g.W - 1; x++)
      if (g.water[g.I(x, y)] && g.M.biomeAt(x, y) != Biome::Ocean && g.dist(x, y) < 1.05f) return true;
  return false;
}

// The production buildings (services() places them with the rest, in this order). Owners: millers and the granary's
// keeper are farmers, the food and cloth trades sell over their counters (merchants), the smelter is a smith.
void Gen::economyServices(std::vector<Want>& want) {
  auto add = [&](Building b, Role r, int w, int h, float near, bool req, District d = District::COUNT, int water = 0) {
    Want wt{b, r, w, h, near, req, false, d, -1, -1};
    wt.water = water;
    want.push_back(wt);
  };
  const bool river = riverInTown(*this);
  auto mill = [&](bool req) {
    // a watermill where a river runs through the town (its wheel in the stream), else a windmill out by the fields
    if (river) add(Building::Watermill, Role::Farmer, 4, 3, 1.15f, req, District::COUNT, 1);
    else add(Building::Windmill, Role::Farmer, 4, 3, village ? 0.9f : 1.2f, req, city ? District::Crafts : District::COUNT);
  };
  switch (spec) {
    case Specialty::Farming:
      mill(true);
      add(Building::Granary, Role::Farmer, 4, 3, 1.0f, true);
      break;
    case Specialty::Fishing: add(Building::Fishmonger, Role::Merchant, 4, 3, 1.0f, true, District::COUNT, 2); break;
    case Specialty::Mining: add(Building::Smelter, Role::Smith, 5, 3, 1.0f, true, District::Crafts); break;
    case Specialty::Lumber: add(Building::Sawmill, Role::Villager, 5, 3, 1.1f, true, District::Crafts); break;
    case Specialty::Herding: add(Building::Tanner, Role::Villager, 4, 3, 1.0f, true, District::Poor); break;
    default: break;
  }
  if (village) {
    if (spec != Specialty::Farming && river && rng.f() < 0.5f) mill(false);   // a mill where the stream turns it
    return;
  }
  // towns and cities: the full set of trades
  add(Building::Bakery, Role::Merchant, 4, 3, 0.5f, true, District::Centre);
  add(Building::Butcher, Role::Merchant, 4, 3, 0.6f, true, District::Centre);
  add(Building::Weaver, Role::Merchant, 4, 3, 0.7f, town, District::Crafts);
  if (spec != Specialty::Farming) mill(false);
  if (spec != Specialty::Herding) add(Building::Tanner, Role::Villager, 4, 3, 0.95f, false, District::Poor);
  if (city) {
    add(Building::Bakery, Role::Merchant, 4, 3, 0.7f, false, District::Crafts);
    if (spec != Specialty::Fishing && (river || arch == Archetype::Port)) add(Building::Fishmonger, Role::Merchant, 4, 3, 0.9f, false, District::COUNT, 2);
    if (spec != Specialty::Mining) add(Building::Smelter, Role::Smith, 5, 3, 0.85f, false, District::Crafts);
    if (spec != Specialty::Farming) add(Building::Granary, Role::Farmer, 4, 3, 0.9f, false, District::Crafts);
  }
}

// A building against the water: water 1 = a mill whose wheel turns in a river beside its east or west wall (the
// variant says which side); water 2 = a trade that works by the shore (its door within a few steps of the water)
bool Gen::placeByWater(Building type, Role owner, int bw, int bh, bool sideWater) {
  std::vector<int> cand;
  for (int y = 2; y < H - 2; y++)
    for (int x = 2; x < W - 2; x++) {
      if (water[I(x, y)] || dist(x, y) > 1.15f || groundSolid(M.at(x, y))) continue;
      bool ok = false;
      // (M3: the land's own river, not a stilt town's marsh pool)
      auto river = [&](int tx, int ty) { return in(tx, ty) && water[I(tx, ty)] && groundWater(M.at(tx, ty)) && M.biomeAt(tx, ty) != Biome::Ocean; };
      if (sideWater) ok = river(x + 1, y) || river(x - 1, y);
      else
        for (int oy = -3; oy <= 3 && !ok; oy++)
          for (int ox = -3; ox <= 3; ox++) if (in(x + ox, y + oy) && water[I(x + ox, y + oy)]) { ok = true; break; }
      if (ok) cand.push_back(y * W + x);
    }
  for (size_t i = cand.size(); i > 1; i--) std::swap(cand[i - 1], cand[(size_t)rng.irange((int)i)]);
  int tries = 0;
  for (int c : cand) {
    if (++tries > 400) break;
    const int x = c % W, y = c / W;
    if (sideWater) {
      const bool east = water[I(x + 1, y)] && groundWater(M.at(x + 1, y)) && M.biomeAt(x + 1, y) != Biome::Ocean;
      // the footprint's river-side column is this tile's column; try it at each height along the bank
      const int rx0 = east ? x - bw + 1 : x;
      for (int k = 0; k < bh; k++) {
        const int ry0 = y - k;
        // the wheel needs the river along at least two rows of that side
        int along = 0;
        for (int yy = ry0; yy < ry0 + bh; yy++) if (in(east ? x + 1 : x - 1, yy) && water[I(east ? x + 1 : x - 1, yy)] && groundWater(M.at(east ? x + 1 : x - 1, yy))) along++;
        if (along < 2) continue;
        const int before = (int)M.bldgs.size();
        if (tryPlace(type, owner, bw, bh, rx0 + bw / 2, ry0 + bh, true)) {
          if ((int)M.bldgs.size() > before) {
            Bldg& b = M.bldgs.back();
            b.variant = east ? 0 : 1;
            // the wheel turns in the river against the mill's side wall, a row behind its front
            const int wx = east ? b.r.x + b.r.w : b.r.x - 1;
            for (int wy : {b.r.y + b.r.h - 2, b.r.y + b.r.h - 1, b.r.y})
              if (in(wx, wy) && groundWater(M.at(wx, wy)) && !M.prop[I(wx, wy)]) { M.setProp(wx, wy, Prop::WaterWheel); break; }
          }
          return true;
        }
      }
    } else {
      // the door toward the town: the footprint on the tile's land side
      if (tryPlace(type, owner, bw, bh, x, y, true)) return true;
    }
  }
  return false;
}

// ------------------------------------------------------------------------------------------------ the market
// (M1 fixer) a solid prop on (x, y) cuts nobody off when the open tiles beside it still reach each other round it:
// then any way that ran through (x, y) runs round it instead. A small window keeps it cheap (the web builds a capital
// within a frame budget), and only errs on the safe side.
bool Gen::keepsWay(int x, int y) const {
  if (!walkable(x, y)) return true;
  constexpr int R = 6, N = 2 * R + 1;
  uint8_t seen[N * N] = {};
  int q[N * N];
  int nb[4][2], nn = 0;
  for (int d = 0; d < 4; d++)
    if (walkable(x + D4X[d], y + D4Y[d])) { nb[nn][0] = x + D4X[d]; nb[nn][1] = y + D4Y[d]; nn++; }
  if (nn <= 1) return true;
  auto idx = [&](int tx, int ty) { return (ty - y + R) * N + (tx - x + R); };
  seen[idx(x, y)] = 1;
  int qh = 0, qt = 0;
  seen[idx(nb[0][0], nb[0][1])] = 1;
  q[qt++] = idx(nb[0][0], nb[0][1]);
  while (qh < qt) {
    const int c = q[qh++];
    const int tx = c % N + x - R, ty = c / N + y - R;
    for (int d = 0; d < 4; d++) {
      const int nx = tx + D4X[d], ny = ty + D4Y[d];
      if (std::abs(nx - x) > R || std::abs(ny - y) > R) continue;
      const int i = idx(nx, ny);
      if (seen[i] || !walkable(nx, ny)) continue;
      seen[i] = 1;
      q[qt++] = i;
    }
  }
  for (int k = 1; k < nn; k++) if (!seen[idx(nb[k][0], nb[k][1])]) return false;
  return true;
}

bool Gen::putSolid(int x, int y, Prop p) {
  if (!in(x, y)) return false;
  if (propSolid(p) && !keepsWay(x, y)) return false;
  M.setProp(x, y, p);
  return true;
}

// (M1 fixer) A street market's lots: before the houses go up, a few stall-runs of open ground on the north verge of a
// main street near the heart (the street their aisle) are kept from building, so the market can spill along the street
// where the town's draw says it does (every market town, a third of the others)
void Gen::marketLots() {
  lotStalls = 0;
  if (village || squares.empty()) return;
  const bool mk = arch == Archetype::Market;
  const uint32_t h = hashAt(cx, cy, 521u);
  int lots = mk ? 2 + (int)(h % 2u) : ((h >> 4) % 100u < 35 ? 1 : 0);
  if (!lots) return;
  const Square& S = squares[0];
  const float maxD = mk ? 0.66f : 0.46f;
  auto posOk = [&](int x, int y) {
    for (int dx = -1; dx <= 1; dx++) {
      const int tx = x + dx;
      if (!in(tx, y - 3) || !in(tx, y + 2)) return false;
      if (get(tx, y + 1) != K_MAIN || (get(tx, y + 2) != K_MAIN && get(tx, y + 2) != K_SQUARE)) return false;
      for (int dy = -2; dy <= 0; dy++) {
        const size_t i = I(tx, y + dy);
        if (mask[i] != K_NONE || water[i] || noBuild[i] || M.bldgAt[i] >= 0 || M.wall[i] || lvl[i] != lvl[I(tx, y + 1)]) return false;
        if (groundSolid(M.at(tx, y + dy)) || inCompound(tx, y + dy, 1)) return false;
      }
      const float d = dist(tx, y);
      if (d < 0.18f || d > maxD) return false;
    }
    if ((O.gy + y) & 1) return false;   // (stall facings) a street's stalls face the street: south, on an even global row
    // apart from the square's own market
    return std::abs(x - S.x) > (int)(S.r * 1.25f) + 5 || std::abs(y - S.y) > (int)S.r + 6;
  };
  struct Cand { int x, y, n; uint32_t k; };
  std::vector<Cand> cands;
  for (auto& t : mainTiles) {
    const int x = t.first, y = t.second - 1;
    if (!posOk(x, y)) continue;
    int n = 1;
    while (n < (mk ? 3 : 2) && posOk(x + 3 * n, y)) n++;
    if (n < 2) continue;   // (M1 fixer round 2) a street market is a run of two or more, never a lone stall
    cands.push_back({x, y, n, hashAt(x, y, 523u)});
  }
  std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {   // (a total order: the same on every platform)
    if (a.n != b.n) return a.n > b.n;
    if (a.k != b.k) return a.k < b.k;
    return a.y != b.y ? a.y < b.y : a.x < b.x;
  });
  std::vector<std::pair<int, int>> made;
  for (const Cand& c : cands) {
    if ((int)made.size() >= lots) break;
    bool far = true;
    for (auto& m : made) if (std::abs(m.first - c.x) < 14 && std::abs(m.second - c.y) < 8) far = false;
    if (!far) continue;
    bool still = true;   // (an earlier lot may have taken part of it)
    for (int k = 0; k < c.n && still; k++) still = posOk(c.x + 3 * k, c.y);
    if (!still) continue;
    for (int y = c.y - 2; y <= c.y; y++)
      for (int x = c.x - 1; x <= c.x + 3 * c.n - 2; x++) noBuild[I(x, y)] |= 4;
    made.push_back({c.x, c.y});
    lotStalls += c.n;
  }
}

// The market (M1 fixer round 2: a planned market, never a vending grid nor a scatter). One market place on the main
// square, laid out the way a town would plan it:
//  - the trades' stalls in one or two rows, each row a few groups of two or three stalls touching, a walkway of two or
//    three tiles between the groups, the rows aligned one behind the other with a wide aisle (four tiles of open
//    paving) between them, the rows kept clear of the square's edges; every trade keeps one stall at most in the whole
//    town (a capital's eight trades are eight stalls), each row built alike (cloth booths, canvas tents or shingled
//    timber booths: the view draws the row's form);
//  - the rest of the market in the open: trestle tables and cloths spread on the paving in a line of their own across
//    the square, each selling something no other table sells (cheese, eggs, spices, flowers, honey, baskets, wool,
//    cider; pots, rugs, gourds, furs), a walkway round every one;
//  - stock stacked at the groups' ends, the traders' carts at the row ends, a keeper behind every counter;
//  - a market town's market also runs along its main street (the lots kept for it), always a run of two or more;
//  - a group of one never stands alone: a stall that cannot find a neighbour is not built (its trade sells from a table).
// Villages: one or two stalls (a pair, or a stall and a cart) on the green by the well, a bench and a tree beside it.
void Gen::markets() {
  reserved.assign((size_t)W * H, 0);
  stalls.clear();
  if (squares.empty()) return;
  const Square& S = squares[0];
  const bool mk = arch == Archetype::Market;
  // ---- the trades: one stall each at most; the town's own trade among the first few (a village's first of all)
  const int own = tradeOf(spec);
  std::vector<int> pool{own};
  auto addP = [&](int t) { if (std::find(pool.begin(), pool.end(), t) == pool.end()) pool.push_back(t); };
  if (village) {
    addP(spec == Specialty::Farming ? (int)StallTrade::Bread : (spec == Specialty::Herding ? (int)StallTrade::Cloth : (int)StallTrade::Produce));
    addP(spec == Specialty::Farming ? (int)StallTrade::Meat : (int)StallTrade::Bread);
  } else {
    for (int t = 0; t < (int)StallTrade::COUNT; t++) {
      if (t == (int)StallTrade::Fish && spec != Specialty::Fishing && arch != Archetype::Port && !city) continue;   // inland towns
      if (t == (int)StallTrade::Timber && spec != Specialty::Lumber && !city) continue;
      addP(t);
    }
    for (size_t i = pool.size(); i > 1; i--) std::swap(pool[i - 1], pool[(size_t)rng.irange((int)i)]);
    const size_t at = (size_t)std::min((int)pool.size() - 1, rng.irange(3));
    auto it = std::find(pool.begin(), pool.end(), own);
    pool.erase(it);
    pool.insert(pool.begin() + (std::ptrdiff_t)at, own);
  }
  size_t nextT = 0;
  // ---- how big
  int nTrade, nIsl, maxRows;
  if (village) { nTrade = 1 + rng.irange(2) + (mk ? 1 : 0); nIsl = 0; maxRows = 1; }
  else if (capital) { nTrade = 7 + rng.irange(2); nIsl = 4 + rng.irange(3) + (mk ? 2 : 0); maxRows = 2; }
  else if (city) { nTrade = 5 + rng.irange(2) + (mk ? 1 : 0); nIsl = 2 + rng.irange(3) + (mk ? 2 : 0); maxRows = 2; }
  // (stalls fixer round 2) a town's market has four stalls or more: room for a pair seen from the front and a pair
  // turned (from behind or in profile)
  else { nTrade = mk ? 6 + rng.irange(2) : 4 + rng.irange(2); nIsl = mk ? 3 + rng.irange(2) : rng.irange(3); maxRows = 2; }
  nTrade = std::min(nTrade, (int)pool.size());
  // ---- the rhythm (by the town's own draw)
  const int walk = 2 + rng.irange(2);   // the walkway between two groups of a row
  const int rowGap = city ? 7 : 6 + rng.irange(2);
  const bool bigFirst = rng.f() < 0.5f;   // a row of five: three then two, or two then three

  // ---- where a stall may stand. mode 0: on a square; mode 1: on the verge north of a main street (the street its
  // aisle); mode 2 (a village's green too cramped, a town with no room left): on open ground with two clear rows
  // before it. (qx, qy): the square's centrepiece, which keeps a plaza round it.
  auto tallNear = [&](int x0, int y0, int x1, int y1) {
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) {
        if (!in(x, y)) continue;
        const int p = M.prop[I(x, y)];
        if (p && !lowProp((Prop)(p - 1))) return true;
      }
    return false;
  };
  bool strictView = true;   // cities: two open rows past the aisle (false: one, for a cramped square)
  bool relaxed = false;     // the last resort for a cramped square: only no house right past the aisle
  bool small = false;       // another (smaller) square: its well or statue keeps a smaller plaza
  int nearX = -1, nearY = -1, nearR = 0;   // mode 2: within nearR of this tile (a village: its well)
  // (stall facings, owner 2026-10-05) a stall faces one of four ways (art_props.h StallFacing): its frame here is F, the
  // way to its customers (its aisle), and A, the way its counter runs on to the next stall of its row. An east-west
  // counter (S, N) is x - 1 .. x + 1 on row y; a north-south one (E, W) is y - 2 .. y on column x (the prop on its
  // south tile). Which of each pair by the parity of the prop's global tile (the view reads it back from the map).
  struct Frame { int fx, fy, ax, ay; };
  auto frameOf = [](int f) -> Frame {
    switch (f) {
      case art::StallN: return {0, -1, 1, 0};
      case art::StallE: return {1, 0, 0, 1};
      case art::StallW: return {-1, 0, 0, 1};
      default: return {0, 1, 1, 0};
    }
  };
  auto parityOk = [&](int f, int x, int y) { return art::stallFacingOf(f >= art::StallE, O.gx + x, O.gy + y) == f; };
  // the walking space of the market (aisles and the ways round the rows' ends): a later stall's aisle may share it
  std::vector<uint8_t> walkMk((size_t)W * H, 0);
  std::vector<uint8_t> endMk((size_t)W * H, 0);   // the ways round a row's ends (a row grown by a stall moves them on)
  bool extending = false;                          // a stall added to a row's end may stand on that row's end way
  auto stallOk = [&](int x, int y, int mode, int qx, int qy, int lv, int f) {
    if (!parityOk(f, x, y)) return false;
    const Frame F = frameOf(f);
    int bx0 = 1 << 30, by0 = 1 << 30, bx1 = -(1 << 30), by1 = -(1 << 30);   // the stall's box (counter, keeper's walk)
    for (int ci = 0; ci < 3; ci++) {
      int dx, dy;
      art::stallCounterTile(f, ci, dx, dy);
      const int tx = x + dx, ty = y + dy;
      if (!in(tx - 2 * F.fx, ty - 2 * F.fy) || !in(tx + 4 * F.fx, ty + 4 * F.fy)) return false;
      const size_t i = I(tx, ty);
      const uint8_t k = mask[i];
      if (mode == 0 ? k != K_SQUARE : (mode == 1 ? (k != K_NONE && k != K_YARD) : (k != K_NONE && k != K_YARD && k != K_SQUARE))) return false;
      if (M.prop[i] || front[i] || cover[i] || (reserved[i] && !(extending && endMk[i])) || M.bldgAt[i] >= 0 || M.wall[i] || water[i] || (noBuild[i] & 3))
        return false;
      if (groundSolid(M.at(tx, ty)) || M.at(tx, ty) == Ground::Bridge || lvl[i] != lv) return false;
      if (mode == 1 && (dist(tx, ty) > (mk ? 0.7f : 0.5f) || inCompound(tx, ty, 1))) return false;
      if (mode == 2 && inCompound(tx, ty, 1)) return false;
      if (mode == 2 && (nearR > 0 ? std::max(std::abs(tx - nearX), std::abs(ty - nearY)) > nearR : dist(tx, ty) > 0.7f)) return false;
      // the aisle: two rows of open ground before the counter (public ground on a square or a street)
      for (int d = 1; d <= 2; d++) {
        const int ax = tx + F.fx * d, ay = ty + F.fy * d;
        const size_t j = I(ax, ay);
        if (!walkable(ax, ay)) return false;
        const uint8_t ka = mask[j];
        const bool pub = ka == K_SQUARE || ka == K_MAIN || (mode == 0 && ka == K_LANE) || (walkMk[j] && ka == K_YARD);
        if (mode == 2 ? (ka == K_FIELD || ka == K_COMPOUND || front[j]) : !pub) return false;
        if (M.prop[j] || (reserved[j] && !walkMk[j]) || M.bldgAt[j] >= 0 || M.wall[j] || groundSolid(M.at(ax, ay)) || lvl[j] != lv) return false;
        if (mode == 1 && d == 1 && ka != K_MAIN) return false;
      }
      // past the aisle the counter looks out over open ground, not into the back of a house
      if (!village && mode == 0)
        for (int d = 3; d <= (city ? 4 : 3); d++) {
          const size_t j = I(tx + F.fx * d, ty + F.fy * d);
          if (M.bldgAt[j] >= 0 || (cover[j] && ((d == 3 && !relaxed) || strictView)) || M.wall[j]) return false;
        }
      // behind: the keeper's walk (clear of every house's front and the apron before it, so a stall never backs onto
      // a facade), nobody's door, nobody's wall under the awning
      for (int d = 1; d <= 2; d++) {
        const int bx = tx - F.fx * d, by = ty - F.fy * d;
        const size_t j = I(bx, by);
        if (M.bldgAt[j] >= 0 || front[j] || M.wall[j]) return false;
        if (d == 1 && (M.prop[j] || cover[j] || reserved[j] || groundSolid(M.at(bx, by)) || water[j])) return false;
        bx0 = std::min(bx0, bx); bx1 = std::max(bx1, bx); by0 = std::min(by0, by); by1 = std::max(by1, by);
      }
      bx0 = std::min(bx0, tx); bx1 = std::max(bx1, tx); by0 = std::min(by0, ty); by1 = std::max(by1, ty);
    }
    // the centrepiece keeps a plaza round it: no counter, keeper's walk or roof crowding it (a city's statue or great
    // fountain wants more room)
    if (qx >= 0) {
      // (M1 fixer round 2: a row south of it stands clear of its top, the awning never rising into the basin or the well)
      const int up = small ? 3 : (village ? 3 : (city ? 5 : 4)) - (relaxed ? 1 : 0), down = small ? 3 : 5, side = small ? 4 : (village ? 4 : (city ? 6 : 5));
      const int cx = x, cy = f >= art::StallE ? y - 1 : (f == art::StallN ? y + 1 : y);   // the stall's middle
      if (cy > qy - up && cy < qy + down && std::abs(cx - qx) < side + (f >= art::StallE ? -1 : 0)) return false;
    }
    // nothing tall crowding the roof (the fountain, the well, trees, banners, lamps)
    return !tallNear(bx0 - 1, by0 - (f == art::StallS ? 0 : 1), bx1 + 1, by1 + (f == art::StallN ? 0 : 1));
  };
  auto place = [&](int x, int y, int trade, int f) {
    const Frame F = frameOf(f);
    int c[3][2];
    for (int ci = 0; ci < 3; ci++) {
      art::stallCounterTile(f, ci, c[ci][0], c[ci][1]);
      c[ci][0] += x; c[ci][1] += y;
    }
    // the counter is solid across its three tiles: each must leave the ways round it open
    if (!putSolid(c[0][0], c[0][1], art::stallProp(trade))) return false;
    if (!putSolid(c[1][0], c[1][1], Prop::Filler)) { M.setP(c[0][0], c[0][1], 0); return false; }
    if (!putSolid(c[2][0], c[2][1], Prop::Filler)) { M.setP(c[0][0], c[0][1], 0); M.setP(c[1][0], c[1][1], 0); return false; }
    for (int ci = 0; ci < 3; ci++) {
      const int tx = c[ci][0], ty = c[ci][1];
      if (mask[I(tx, ty)] == K_NONE) set(tx, ty, K_YARD);
      reserved[I(tx - F.fx, ty - F.fy)] = 1;   // the keeper's walk
      for (int d = 1; d <= 2; d++) {           // the trodden aisle
        const size_t j = I(tx + F.fx * d, ty + F.fy * d);
        reserved[j] = 1;
        walkMk[j] = 1;
        if (mask[j] == K_NONE) set(tx + F.fx * d, ty + F.fy * d, K_YARD);
      }
    }
    stalls.push_back(Stall{x, y, trade});
    return true;
  };
  auto facingOf = [&](const Stall& s) { return art::stallFacingAt([&](int px, int py) { return M.propAt(px, py); }, s.x, s.y, O.gx, O.gy); };

  // ---- a row: n stalls in groups of two or three (never one, unless n is 1), a walkway between the groups. Every
  // start between a0 and a1 (along the row: x for an east-west row on line y, y for a north-south one on line x) is
  // tried; the row's ends must open onto walkable ground (a row never runs into a house or the square's edge). The best
  // start is the one nearest prefA (a little of the town's own chance between equals).
  // (stalls fixer round 2, "side stalls next to each other read as one tall strip") a side row's stalls never touch:
  // a tile of open ground between each two of a group (the step along is 4, not 3), so no two side roofs merge into
  // one column
  auto stepOf = [](int f) { return f >= art::StallE ? 4 : 3; };
  auto pattern = [&](int n, int f) {
    std::vector<int> g;
    switch (n) {
      case 1: g = {1}; break;
      case 2: g = {2}; break;
      case 3: g = {3}; break;
      case 4: g = {2, 2}; break;
      case 5: g = bigFirst ? std::vector<int>{3, 2} : std::vector<int>{2, 3}; break;
      case 6: g = walk == 3 ? std::vector<int>{3, 3} : std::vector<int>{2, 2, 2}; break;
      case 7: g = bigFirst ? std::vector<int>{3, 2, 2} : std::vector<int>{2, 2, 3}; break;
      default: g = {3, 2, 3}; break;
    }
    std::vector<int> off;   // the stall centres from the first
    int x = 0;
    for (size_t k = 0; k < g.size(); k++) {
      for (int i = 0; i < g[k]; i++) { off.push_back(x); x += stepOf(f); }
      x += walk - (stepOf(f) - 3);
    }
    return off;
  };
  auto at = [](int f, int line, int a, int& x, int& y) {
    if (f >= art::StallE) { x = line; y = a; } else { x = a; y = line; }
  };
  struct RowFit { int x = 0; int score = -(1 << 30); bool ok = false; };
  auto fitRow = [&](int line, int n, int mode, int a0, int a1, int prefA, int qx, int qy, int lv, int alignA, int f) {
    RowFit best;
    const std::vector<int> off = pattern(n, f);
    const Frame F = frameOf(f);
    const int span = off.back() + 1;
    for (int s = a0 + 1; s + span - 1 <= a1 - 1; s++) {
      bool ok = true;
      for (int o : off) {
        int x, y;
        at(f, line, s + o, x, y);
        if (!stallOk(x, y, mode, qx, qy, lv, f)) { ok = false; break; }
      }
      if (!ok) continue;
      // the row's ends open onto walkable ground (two tiles past each end counter, beside the counter and its aisle),
      // so people walk round it
      if (n > 1 || mode != 2) {
        const int lo = f >= art::StallE ? s - 2 : s - 1, hi = s + off.back() + (f >= art::StallE ? 0 : 1);   // the counters' extent along
        for (int e : {lo - 2, lo - 1, hi + 1, hi + 2}) {
          int x, y;
          at(f, line, e, x, y);
          if (!walkable(x, y) || !walkable(x + F.fx, y + F.fy)) { ok = false; break; }
        }
      }
      if (!ok) continue;
      const int mid = s + off.back() / 2;
      int sc = -std::abs(mid - prefA) * 3 + (int)(hashAt(s, line, 487u + (uint32_t)f) % 5u);
      if (alignA >= 0) sc -= std::abs(mid - alignA) * 6;   // a later row centred on the first
      if (sc > best.score) { best.x = s; best.score = sc; best.ok = true; }
    }
    return best;
  };
  // a whole row or nothing (a counter that would cut a way off leaves no lone stall behind: the row is taken back)
  auto putRow = [&](int line, int s, int n, int f) {
    const std::vector<int> off = pattern(n, f);
    const Frame F = frameOf(f);
    const bool ns = f >= art::StallE;
    const int lo = ns ? s - 2 : s - 1, hi = s + off.back() + (ns ? 0 : 1);
    // the box the row may touch (counters, keepers' walk, aisles, the ways round the ends)
    const int xa = ns ? line - 3 : lo - 3, xb = ns ? line + 3 : hi + 3, ya = ns ? lo - 3 : line - 3, yb = ns ? hi + 3 : line + 3;
    std::vector<uint8_t> keep;
    for (int yy = ya; yy <= yb; yy++)
      for (int xx = xa; xx <= xb; xx++) {
        if (!in(xx, yy)) continue;
        const size_t i = I(xx, yy);
        keep.push_back(reserved[i]); keep.push_back(mask[i]); keep.push_back(M.prop[i]); keep.push_back(walkMk[i]); keep.push_back(endMk[i]);
      }
    const size_t st0 = stalls.size(), t0 = nextT;
    int made = 0;
    for (int o : off) {
      int x, y;
      at(f, line, s + o, x, y);
      if (nextT >= pool.size() || !place(x, y, pool[nextT], f)) break;
      nextT++;
      made++;
    }
    if (made == (int)off.size()) {
      // the way round the row's ends stays open (no lamp, banner or tree may take it later)
      for (int e : {lo - 2, lo - 1, hi + 1, hi + 2})
        for (int d = 0; d <= 1; d++) {
          int x, y;
          at(f, line, e, x, y);
          x += F.fx * d; y += F.fy * d;
          if (!in(x, y)) continue;
          if (d == 1 || e == lo - 2 || e == hi + 2) { reserved[I(x, y)] = 1; walkMk[I(x, y)] = 1; endMk[I(x, y)] = 1; }
        }
      return made;
    }
    size_t k = 0;
    for (int yy = ya; yy <= yb; yy++)
      for (int xx = xa; xx <= xb; xx++) {
        if (!in(xx, yy)) continue;
        const size_t i = I(xx, yy);
        reserved[i] = keep[k++]; mask[i] = keep[k++]; M.prop[i] = keep[k++]; walkMk[i] = keep[k++]; endMk[i] = keep[k++];
      }
    stalls.resize(st0);
    nextT = t0;
    return 0;
  };
  std::vector<int> rowYs;   // the market's east-west rows (their counters' row)
  // ---- the rows on one square: searched over the area A, the market's own zone Zp preferred. The first row as long as
  // it can be (up to its share) near the zone's top, its counters facing south over its aisle; the second faces it
  // across a wide shared aisle (its counters toward the north: from the camera we see its back), or, where that has
  // no room, stands a wide aisle behind or before the first facing south; what is left (a grand market, or a town's
  // draw) goes in a short row down the market's west or east side, its counters turned in toward the walking space
  // (side profiles). Returns the stalls placed.
  bool firstBack = false;   // (stalls fixer round 2) the retry of a market that came out all fronts: its first row turned
  bool firstSide = false;   // (and the next retry: a side row first, its counters turned toward the centrepiece)
  auto squareRows = [&](const IRect& Zp, const IRect& A, int qx, int qy, int lv, int count, int rowsMax, int mode) {
    int placed = 0, firstMid = -1;
    std::vector<int> mine;
    const size_t st0 = stalls.size();
    count = std::min(count, (int)(pool.size() - nextT));
    const int prefX = Zp.x + Zp.w / 2 + (int)(hashAt(Zp.x, Zp.y, 491u) % 5u) - 2;
    const int targetY0 = Zp.y + 1 + (int)(hashAt(Zp.x, Zp.y, 499u) % 2u);
    // a short row down a side: a grand market's last stalls, a third of the towns with four or more
    // (stalls fixer round 2, "some markets still face the camera only") every market of four or more on a square
    // tries a side row, so it never shows only fronts
    int side = 0;
    if (mode == 0 && count >= 4 && !village) side = 2;
    bool turn = false;   // the last pass: a later row faces the first across the aisle (its back to us), never south
    const int rowsCount = count - side;
    auto rowsPass = [&](int target, int rmax) {
    for (int r = (int)mine.size(); r < rmax && placed < target; r++) {
      const int left = target - placed, rowsLeft = rmax - r;
      int want = (left + rowsLeft - 1) / rowsLeft;
      if (rowsLeft > 1 && left - want == 1) want++;   // never leave one stall for the last row
      // the first row as long as it can be (stalls fixer round 2: leaving the side row its pair)
      if (r == 0) want = std::max(want, std::min(3, target - placed));
      int by = -1, bx = 0, bn = 0, bs = -(1 << 30), bf = art::StallS;
      for (int n = want; n >= (village ? 1 : 2); n--) {
        if (r > 0 && n < 2) break;
        for (int y = A.y - 1; y <= A.y + A.h; y++)
          for (int f : {art::StallS, art::StallN}) {
            // the first row: along the north side, facing south (or, where only the south side has room, along it
            // facing north); a later one faces it across the walking space
            if ((turn && r > 0 && f == art::StallS) || (firstBack && r == 0 && f == art::StallS)) continue;   // (fixer round 2) backs only
            bool close = false;                       // every row's aisle stays open (rows stand rowGap apart)...
            bool across = false;                      // ...but a row may face an earlier one across a shared aisle
            // (stalls fixer round 2) the turned pass also takes a row facing back across a five-tile aisle, back to
            // back with the first (their keepers' walks side by side), or a wide aisle behind or before it
            const bool tp = turn && r > 0;
            bool b2b = false;
            for (int ry : rowYs) {
              if (f == art::StallN && (y - ry == 5 || (tp && y - ry == 6))) across = true;
              if (tp && f == art::StallN && ry - y == 3) b2b = true;
            }
            for (int ry : rowYs)
              if (std::abs(y - ry) < rowGap && !(across && (y - ry == 5 || (tp && y - ry == 6))) && !(b2b && ry - y == 3)) close = true;
            if (close) continue;
            if (r > 0) {   // facing an earlier row across a shared aisle, or a wide aisle behind or before one
              bool by1 = across || b2b;
              for (int my : mine) if ((f == art::StallS || tp) && (std::abs(y - my) == rowGap || std::abs(y - my) == rowGap + 1)) by1 = true;
              if (!by1) continue;
            }
            const RowFit fr = fitRow(y, n, mode, A.x - 2, A.x + A.w + 1, r > 0 ? firstMid : prefX, qx, qy, lv, r > 0 ? firstMid : -1, f);
            if (!fr.ok) continue;
            const int mid = fr.x + pattern(n, f).back() / 2;
            int sc = fr.score + n * 40;
            if (r == 0 && f == art::StallN) {
              if (y <= qy + 1 && !across) continue;   // a row showing its back stands on the south side only
              sc -= 16 + std::abs(y - (Zp.y + Zp.h - 2)) * 4 - (across ? 30 : 0);
              if (y < Zp.y - 1 || y > Zp.y + Zp.h + 1) sc -= 40;
              if (mid < Zp.x || mid > Zp.x + Zp.w) sc -= 30;
            } else if (r == 0) {
              sc -= std::abs(y - targetY0) * 4;
              if (y < Zp.y - 1 || y > Zp.y + Zp.h) sc -= 40;   // outside the market's own part of the square
              if (mid < Zp.x || mid > Zp.x + Zp.w) sc -= 30;
            } else {
              if (across) sc += 24;                              // the rows face each other over the walking space
              if (ew::stallFormAt(O.gy + y) != ew::stallFormAt(O.gy + mine[0])) sc += 6;   // the next row built otherwise
            }
            if (sc > bs) { bs = sc; by = y; bx = fr.x; bn = n; bf = f; }
          }
      }
      if (by < 0) break;
      const int made = putRow(by, bx, bn, bf);
      if (!made) break;
      placed += made;
      rowYs.push_back(by);
      mine.push_back(by);
      if (firstMid < 0) firstMid = bx + pattern(bn, bf).back() / 2;
    }
    };
    if (firstSide) {   // a short row in profile beside the centrepiece first, then the rows as ever
      for (int n = std::min(3, count); n >= 2 && placed == 0; n--) {
        if (count - n == 1) continue;   // never one left for the rows
        int bl = -1, ba = 0, bs = -(1 << 30), bf = art::StallE;
        for (int f : {art::StallE, art::StallW})
          for (int line = A.x - 2; line <= A.x + A.w + 1; line++) {
            if (f == art::StallE ? line >= qx : line <= qx) continue;   // its counters toward the centrepiece
            const RowFit fr = fitRow(line, n, mode, A.y - 2, A.y + A.h + 1, qy, qx, qy, lv, -1, f);
            if (!fr.ok) continue;
            const int sc = fr.score - std::abs(std::abs(line - qx) - 6) * 4;
            if (sc > bs) { bs = sc; bl = line; ba = fr.x; bf = f; }
          }
        if (bl >= 0) placed += putRow(bl, ba, n, bf);
      }
      if (!placed) return 0;
    }
    rowsPass(firstSide ? count : rowsCount, rowsMax);
    // the side row: down the west side facing east, or the east side facing west, beside the rows' walking space
    // (gapLo .. gapHi: the walkway between the rows' ends and the side row's line)
    auto sideRow = [&](int gapLo, int gapHi) {
    const int sideWant = std::min((int)(pool.size() - nextT), count - placed);
    if (sideWant >= 2 && !mine.empty()) {
      int lo = 1 << 30, hi = -(1 << 30), top = 1 << 30, bot = -(1 << 30);
      for (const Stall& s : stalls)
        if (std::find(mine.begin(), mine.end(), s.y) != mine.end()) { lo = std::min(lo, s.x - 1); hi = std::max(hi, s.x + 1); top = std::min(top, s.y); bot = std::max(bot, s.y); }
      const int midY = (top + bot) / 2 + 1;
      const bool westFirst = (hashAt(lo, top, 563u) & 1u) != 0;
      for (int n = std::min(3, sideWant); n >= 2 && placed < count; n--) {
        int bl = -1, ba = 0, bs = -(1 << 30), bf = art::StallE;
        for (int f : {art::StallE, art::StallW})
          for (int line = A.x - 2; line <= A.x + A.w + 1; line++) {
            // beside the rows' ends: a walkway of three or four tiles between them and the side row's aisle
            const int gap = f == art::StallE ? lo - line : line - hi;
            if (gap < gapLo || gap > gapHi) continue;
            const RowFit fr = fitRow(line, n, mode, A.y - 2, A.y + A.h + 1, midY, qx, qy, lv, -1, f);
            if (!fr.ok) continue;
            int sc = fr.score - std::abs(gap - 7) * 5 + ((f == art::StallE) == westFirst ? 4 : 0);
            if (sc > bs) { bs = sc; bl = line; ba = fr.x; bf = f; }
          }
        if (bl < 0) continue;
        const int made = putRow(bl, ba, n, bf);
        if (made) { placed += made; break; }
      }
    }
    };
    sideRow(5, 11);
    // (stalls fixer round 2, "some markets still face the camera only") while no stall of this square is turned, what
    // is left first goes in a row facing the first across the aisle (its back to us), else in a side row further out
    auto allFront = [&]() {
      for (size_t i = st0; i < stalls.size(); i++) if (facingOf(stalls[i]) != art::StallS) return false;
      return true;
    };
    if (placed < count && allFront()) {
      turn = true;
      rowsPass(count, rowsMax + (mine.size() >= (size_t)rowsMax ? 0 : 1));
      turn = false;
      if (placed < count && allFront()) sideRow(2, 16);
    }
    // no room down a side: what is left goes in the rows after all (a third row where the second had no room), else a
    // pair grows into a three at whichever end has room
    if (placed < count) rowsPass(count, rowsMax + (mine.size() >= (size_t)rowsMax ? 0 : 1));
    for (size_t i = st0; i < stalls.size() && placed < count && nextT < pool.size(); i++) {
      const Stall s = stalls[i];
      const int f = facingOf(s);
      const Frame F = frameOf(f);
      auto stallAt = [&](int x, int y) {
        const int q = M.propAt(x, y);
        return q && art::isStall((Prop)(q - 1)) && facingOf(Stall{x, y, 0}) == f;
      };
      int gl = 0, gh = 0;
      const int st = stepOf(f);
      while (gl < 3 && stallAt(s.x - st * (gl + 1) * F.ax, s.y - st * (gl + 1) * F.ay)) gl++;
      while (gh < 3 && stallAt(s.x + st * (gh + 1) * F.ax, s.y + st * (gh + 1) * F.ay)) gh++;
      if (gl + gh + 1 != 2) continue;
      for (int dir : {-1, 1}) {
        if ((dir < 0 && gl) || (dir > 0 && gh)) continue;
        const int nx = s.x + dir * st * F.ax, ny = s.y + dir * st * F.ay;
        extending = true;
        bool ok = stallOk(nx, ny, mode, qx, qy, lv, f);
        extending = false;
        // the next group stays a walkway away; the new end opens onto walkable ground
        for (int k = 0; k <= 2 && ok; k++) if (stallAt(nx + dir * (st + k) * F.ax, ny + dir * (st + k) * F.ay)) ok = false;
        const bool ns = f >= art::StallE;
        const int o1 = dir < 0 ? (ns ? -3 : -2) : (ns ? 1 : 2), o2 = o1 + dir;   // past the new counter's end
        for (int o : {o1, o2})
          if (ok && (!walkable(nx + o * F.ax, ny + o * F.ay) || !walkable(nx + o * F.ax + F.fx, ny + o * F.ay + F.fy))) ok = false;
        if (!ok || !place(nx, ny, pool[nextT], f)) continue;
        nextT++;
        placed++;
        for (int o : {o1, o2})
          for (int d = 0; d <= 1; d++) {
            const int x = nx + o * F.ax + F.fx * d, y = ny + o * F.ay + F.fy * d;
            if (!in(x, y) || (d == 0 && o == o1)) continue;
            reserved[I(x, y)] = 1; walkMk[I(x, y)] = 1; endMk[I(x, y)] = 1;
          }
        break;
      }
    }
    return placed;
  };

  // (stalls fixer round 2, "some markets still face the camera only") a market of three or more that came out all
  // fronts (a cramped square: no side row, no row across the aisle) is taken back and laid out again with its first row
  // turned to show its back; if that leaves it smaller than two stalls or still all fronts, the first layout stands
  auto mixedRows = [&](const IRect& Zp, const IRect& A, int qx, int qy, int lv, int count, int rowsMax, int mode) {
    const size_t st0 = stalls.size(), t0 = nextT, ry0 = rowYs.size();
    const std::vector<uint8_t> kR = reserved, kM = mask, kP = M.prop, kW = walkMk, kE = endMk;
    auto restore = [&]() {
      reserved = kR; mask = kM; M.prop = kP; walkMk = kW; endMk = kE;
      stalls.resize(st0); nextT = t0; rowYs.resize(ry0);
    };
    auto allFr = [&]() {
      for (size_t i = st0; i < stalls.size(); i++) if (facingOf(stalls[i]) != art::StallS) return false;
      return true;
    };
    int got = squareRows(Zp, A, qx, qy, lv, count, rowsMax, mode);
    if (got < 3 || village || !allFr()) return got;
    // (the retries look over a cramped square with the view relaxed too: one open row past the aisle)
    const bool sv = strictView;
    for (int k = 0; k < 4; k++) {
      restore();
      strictView = (k & 3) < 2 ? sv : false;
      firstBack = (k & 1) == 0;
      firstSide = !firstBack;
      const int got2 = squareRows(Zp, A, qx, qy, lv, count, rowsMax, mode);
      firstBack = firstSide = false;
      strictView = sv;
      if (got2 >= std::min(got, 3) && !allFr()) return got2;
    }
    // the last resort: one row of fronts a stall shorter, and that stall at the row's end in profile, its counter
    // turned out across the row's end a short walkway from it (an L), facing away along the row's line
    if (got >= 3) {
      restore();
      const int g3 = squareRows(Zp, A, qx, qy, lv, got - 1, rowsMax, mode);
      if (g3 == got - 1 && nextT < pool.size()) {
        int xl = 1 << 30, xh = -(1 << 30), ry = -1;
        bool one = true;
        for (size_t i = st0; i < stalls.size(); i++) {
          if (ry >= 0 && stalls[i].y != ry) one = false;
          ry = stalls[i].y; xl = std::min(xl, stalls[i].x); xh = std::max(xh, stalls[i].x);
        }
        for (int k = 0; k < 4 && one; k++) {
          const int side = k & 1, f = (k < 2) == (side == 1) ? art::StallE : art::StallW;   // out first, then in
          for (int d = 3; d <= 8; d++) {
            const int line = side ? xh + d : xl - d;
            if (!parityOk(f, line, ry)) continue;
            const bool sv0 = strictView, rl0 = relaxed;
            strictView = k >= 2 ? false : sv0;   // (turned in: the view across the row's end relaxed)
            const RowFit fr = fitRow(line, 1, mode, ry - 2, ry + 3, ry, qx, qy, lv, -1, f);
            strictView = sv0; relaxed = rl0;
            if (fr.ok && putRow(line, fr.x, 1, f)) return got;
          }
        }
      }
    }
    restore();
    return squareRows(Zp, A, qx, qy, lv, count, rowsMax, mode);
  };
  const int lv0 = lvl[I(S.x, S.y)];
  const int CX = cpX >= 0 ? cpX : S.x, CY = cpX >= 0 ? cpY : S.y;   // the centrepiece
  IRect Z = mktZone.w > 0 ? mktZone : IRect{S.x - (int)S.r - 2, S.y - (int)S.r - 2, (int)S.r * 2 + 5, (int)S.r * 2 + 5};
  const int R = (int)(S.r * 1.25f) + 3;
  IRect all{std::min(Z.x, S.x - R), std::min(Z.y, S.y - R), 0, 0};
  all.w = std::max(Z.x + Z.w, S.x + R + 1) - all.x;
  all.h = std::max(Z.y + Z.h, S.y + R + 1) - all.y;

  if (village) {
    // ---- the village market: on the green by the well (a pair of stalls touching, or a stall and the trader's cart)
    int wx = -1, wy = -1;
    for (int r = 0; r <= 7 && wx < 0; r++)
      for (int oy = -r; oy <= r && wx < 0; oy++)
        for (int ox = -r; ox <= r; ox++)
          if (std::max(std::abs(ox), std::abs(oy)) == r && M.propAt(S.x + ox, S.y + oy) == (int)Prop::Well + 1) { wx = S.x + ox; wy = S.y + oy; break; }
    if (wx < 0) { wx = CX; wy = CY; }
    // the green first (two stalls, else one), then open ground beside the well, then anywhere near the heart
    int got = 0;
    // (M3 fixer) and last, a wider search round the well: a radial village's rings of lanes can leave no room within
    // 7 tiles of it (seed 37's star-folk village had no stall at all)
    for (int pass = 0; pass < 4 && !got; pass++) {
      const int mode = pass == 0 ? 0 : 2;
      const int span = pass == 3 ? 14 : 7, reach = pass == 3 ? 18 : 10;
      nearX = wx; nearY = wy; nearR = pass >= 2 ? 0 : 7;
      for (int n = nTrade; n >= 1 && !got; n--) {
        int by = -1, bx = 0, bs = -(1 << 30), bf = art::StallS;
        // (stall facings) close to the well, across the green from it, the counters turned toward it from whichever
        // side the stalls stand (north of it facing south, south of it showing their backs, beside it in profile);
        // never with the well crowding the keeper's back
        for (int f = 0; f < art::kStallFacings; f++) {
          const bool ns = f >= art::StallE;
          for (int line = (ns ? wx : wy) - span; line <= (ns ? wx : wy) + span; line++) {
            const RowFit fr = fitRow(line, n, mode, (ns ? wy : wx) - reach, (ns ? wy : wx) + reach, ns ? wy + 1 : wx, wx, wy, lv0, -1, f);
            if (!fr.ok) continue;
            const Frame F = frameOf(f);
            const int mid = fr.x + pattern(n, f).back() / 2;
            int cx, cy;   // the middle of the row's counters
            at(f, line, ns ? mid - 1 : mid, cx, cy);
            const int tw = (wx - cx) * F.fx + (wy - cy) * F.fy;   // how far in front of the counters the well is
            const int off = std::abs((wx - cx) * F.ax + (wy - cy) * F.ay);
            if (tw > -4 && tw < 3 && off < 3 * n / 2 + 3) continue;   // the well against the counter or the keeper's back
            const bool facing = tw >= 3 && tw <= 7 && off <= 3 * n;
            const int sc = fr.score - std::abs(tw - 4) * (facing ? 6 : 3) - off * 2 + (facing ? 40 : 0) +
                           (f == art::StallS ? 6 : (f == art::StallN ? 2 : 3)) + (int)(hashAt(wx, wy, 571u + (uint32_t)f) % 9u);
            if (sc > bs) { bs = sc; by = line; bx = fr.x; bf = f; }
          }
        }
        if (by >= 0) { got = putRow(by, bx, n, bf); if (got && bf <= art::StallN) rowYs.push_back(by); }
      }
    }
    nearR = 0;
    // a bench facing the well, a tree at the green's edge if none shades it (on the green itself or the grass by it)
    auto greenOk = [&](int x, int y) {
      if (!in(x, y)) return false;
      const size_t i = I(x, y);
      const uint8_t k = mask[i];
      if (k != K_NONE && k != K_SQUARE && k != K_YARD) return false;
      if (M.bldgAt[i] >= 0 || M.wall[i] || M.prop[i] || front[i] || reserved[i] || cover[i] || water[i]) return false;
      return !groundSolid(M.at(x, y)) && M.at(x, y) != Ground::Bridge;
    };
    const int bxs[6] = {wx - 2, wx + 2, wx - 1, wx + 1, wx - 3, wx + 3};
    for (int k = 0; k < 6; k++) {
      const int x = bxs[k], y = wy + 2;
      if (greenOk(x, y) && putSolid(x, y, Prop::Bench)) break;
    }
    bool tree = false;
    for (int oy = -6; oy <= 6 && !tree; oy++)
      for (int ox = -6; ox <= 6; ox++) {
        const int q = M.propAt(wx + ox, wy + oy);
        if (q && treeLike((Prop)(q - 1))) { tree = true; break; }
      }
    if (!tree) {
      const Prop t = bio == Biome::Desert ? Prop::PalmTree : (bio == Biome::Snow || bio == Biome::Taiga ? Prop::PineTree : (bio == Biome::Autumn ? Prop::AutumnTree : Prop::OakTree));
      bool done = false;
      for (int r = 3; r <= 6 && !done; r++)
        for (int oy = -r; oy <= r && !done; oy++)
          for (int ox = -r; ox <= r && !done; ox++) {
            if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
            const int x = wx + ox, y = wy + oy;
            if (!greenOk(x, y)) continue;
            bool clear = true;   // its crown over nobody's door, stall or roof
            for (int qy = -3; qy <= 1 && clear; qy++)
              for (int qx = -2; qx <= 2; qx++) {
                const int ax = x + qx, ay = y + qy;
                if (!in(ax, ay) || M.bldgAt[I(ax, ay)] >= 0 || front[I(ax, ay)] || reserved[I(ax, ay)] || (M.prop[I(ax, ay)] && !lowProp((Prop)(M.prop[I(ax, ay)] - 1)))) { clear = false; break; }
              }
            if (clear && putSolid(x, y, t)) done = true;
          }
    }
  } else {
    // ---- the street market first (a market town's, or a town whose draw kept lots for it): runs of two or three
    // stalls touching on the verge of the main street, each where a lot was kept
    int streetN = 0;
    std::vector<int> streetYs;
    // (stalls fixer round 2) the square keeps four of the trades (a pair from the front and a pair turned)
    if (lotStalls > 0 && nTrade >= 6) {
      const int want = std::min(std::min(lotStalls, mk ? 3 : 2), nTrade - 4);
      for (int pass = 0; pass < 2 && streetN < want; pass++) {
        const int n = std::min(want - streetN, 3);
        if (n < 2) break;
        int by = -1, bx = 0, bs = -(1 << 30);
        const int span = mk ? 34 : 24;
        for (int y = S.y - span; y <= S.y + span; y++) {
          bool close = false;
          for (int ry : streetYs) if (std::abs(y - ry) < 4) close = true;
          if (close) continue;
          const RowFit f = fitRow(y, n, 1, S.x - span, S.x + span, S.x, CX, CY, lv0, -1, art::StallS);   // facing the street
          if (!f.ok) continue;
          bool apart = true;   // its own run along the street, clear of the square's market
          for (const Stall& o : stalls) if (std::abs(o.x - f.x) <= 10 && std::abs(o.y - y) <= 6) apart = false;
          if (!apart) continue;
          const int sc = f.score - std::abs(y - S.y) * 3;
          if (sc > bs) { bs = sc; by = y; bx = f.x; }
        }
        if (by < 0) break;
        const int made = putRow(by, bx, n, art::StallS);
        if (!made) break;
        streetYs.push_back(by);
        streetN += made;
      }
    }
    // ---- the rows on the market's side of the main square; if it is too cramped, anywhere on the square; then (a
    // city's other market places) its other squares; then with the view relaxed; never a lone stall
    // (one market place: a later try only when no row stood at all; what does not fit sells from the tables below)
    const int onSquare = nTrade - streetN;
    int got = mixedRows(Z, all, CX, CY, lv0, onSquare, maxRows, 0);
    if (!got) {
      strictView = false;
      got = mixedRows(Z, all, CX, CY, lv0, onSquare, maxRows, 0);
    }
    if (!got) {   // the last resort for a cramped square
      relaxed = true;
      got = mixedRows(Z, all, CX, CY, lv0, onSquare, maxRows, 0);
      relaxed = false;
    }
    if (city && got > 0 && got < 4 && got < onSquare)   // a city's cramped square: a second block of stalls on it
      got += squareRows(all, all, CX, CY, lv0, onSquare - got, 1, 0);
    if (city && got < 4 && squares.size() > 1) {
      std::vector<size_t> order;
      for (size_t k = 1; k < squares.size(); k++) order.push_back(k);
      std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return std::abs(squares[a].x - S.x) + std::abs(squares[a].y - S.y) < std::abs(squares[b].x - S.x) + std::abs(squares[b].y - S.y);
      });
      for (size_t k : order) {
        if (got >= 4 || nextT + 2 > pool.size()) break;
        const Square& Q = squares[k];
        if (!in(Q.x, Q.y)) continue;
        small = true;
        const int r = (int)Q.r + 2;
        const IRect QA{Q.x - r - 1, Q.y - r, r * 2 + 3, r * 2 + 1};
        got += squareRows(QA, QA, Q.x, Q.y, lvl[I(Q.x, Q.y)], std::min(3, (int)(pool.size() - nextT)), 1, 0);
        small = false;
      }
    }
    if (!got && !streetN)   // a town with no room on its square: open ground near the heart
      for (int rad = 8; rad <= 18 && stalls.empty(); rad += 5)
        squareRows(IRect{S.x - rad, S.y - rad, rad * 2 + 1, rad * 2 + 1}, IRect{S.x - rad, S.y - rad, rad * 2 + 1, rad * 2 + 1}, CX, CY, lv0,
                   std::max(2, nTrade - (int)stalls.size()), 1, 2);

    // ---- the open tables and cloths: in lines across the open paving parallel to the stall rows (the first a wide
    // aisle behind or before them, its middle under theirs), a pitch of six (a walkway of four) between them, a walkway
    // round each; none in a stall's aisle or its keeper's walk, none in the centrepiece's plaza; each selling goods no
    // other table or cloth of the town sells
    {
      int need = nIsl + std::max(0, nTrade - (int)stalls.size());   // the stalls that found no room sell here
      const uint32_t mh = hashAt(S.x, S.y, 541u);
      const int clothEvery = 2 + (int)(mh % 3u);   // every second, third or fourth is a cloth on the paving
      bool usedT[art::kTableGoods] = {}, usedC[art::kClothGoods] = {};
      int made = 0;
      // a unit (two tiles) at (x, y) on a square of level lv whose centrepiece stands at (qx, qy)
      auto unitOk = [&](int x, int y, int lv, int qx, int qy) {
        if (!in(x - 1, y - 2) || !in(x + 2, y + 3)) return false;
        for (int dx = 0; dx <= 1; dx++) {
          const int tx = x + dx;
          if (get(tx, y) != K_SQUARE || lvl[I(tx, y)] != lv) return false;
          const size_t i = I(tx, y);
          if (front[i] || cover[i] || (noBuild[i] & 3) || groundSolid(M.at(tx, y))) return false;
        }
        // a clear ring round it (the seller's tile behind, two rows of customers before), off every aisle
        for (int qy2 = y - 1; qy2 <= y + 2; qy2++)
          for (int qx2 = x - 1; qx2 <= x + 2; qx2++) {
            const size_t j = I(qx2, qy2);
            if (M.prop[j] || reserved[j] || M.bldgAt[j] >= 0 || M.wall[j] || water[j] || front[j] || !walkable(qx2, qy2)) return false;
          }
        if (std::abs(x - qx) <= 3 && y >= qy - 2 && y <= qy + 3) return false;   // the centrepiece's plaza
        if (std::abs(x + 1 - qx) <= 3 && y >= qy - 2 && y <= qy + 3) return false;
        if (tallNear(x - 1, y - 2, x + 2, y)) return false;
        for (const Stall& s : stalls) {   // (stall facings) clear of every stall's counter by a walkway
          const bool ns = facingOf(s) >= art::StallE;
          const int cx0 = ns ? s.x : s.x - 1, cx1 = ns ? s.x : s.x + 1, cy0 = ns ? s.y - 2 : s.y, cy1 = s.y;
          if (x + 1 >= cx0 - 3 && x <= cx1 + 3 && y >= cy0 - 4 && y <= cy1 + 4) return false;
        }
        return true;
      };
      auto putUnit = [&](int x, int y, bool cloth) {
        if (!putSolid(x, y, cloth ? Prop::GroundCloth : Prop::MarketTable)) return false;
        if (!putSolid(x + 1, y, Prop::Filler)) { M.setP(x, y, 0); return false; }
        if (cloth) usedC[ew::clothGoodsAt(O.gx + x, O.gy + y)] = true;
        else usedT[ew::tableGoodsAt(O.gx + x, O.gy + y)] = true;
        for (int qy = y - 1; qy <= y + 2; qy++)
          for (int qx = x - 1; qx <= x + 2; qx++) reserved[I(qx, qy)] = 1;
        made++;
        need--;
        // the seller (a table may stand unminded a while; a cloth never)
        if (cloth || rng.f() < 0.85f) addSpawn(Role::Merchant, x, y - 1);
        return true;
      };
      // a line on row y of area A: units out from midX, a pitch of six (a walkway of four); the x's and their kinds
      auto lineAt = [&](const IRect& A, int y, int want, int midX, int lv, int qx, int qy, std::vector<std::pair<int, bool>>& xs) {
        xs.clear();
        bool uT[art::kTableGoods], uC[art::kClothGoods];
        std::copy(usedT, usedT + art::kTableGoods, uT);
        std::copy(usedC, usedC + art::kClothGoods, uC);
        for (int d = 0; d <= A.w && (int)xs.size() < want; d++)
          for (int sd : {-1, 1}) {
            if ((d == 0 && sd > 0) || (int)xs.size() >= want) continue;
            const int x = midX - 1 + sd * d;
            if (x < A.x - 1 || x > A.x + A.w) continue;
            bool spaced = true;
            for (auto& o : xs) if (std::abs(o.first - x) < 5) spaced = false;
            if (!spaced || !unitOk(x, y, lv, qx, qy)) continue;
            const bool cloth = ((made + (int)xs.size()) % clothEvery) == clothEvery - 1;
            const int gx = O.gx + x, gy = O.gy + y;
            bool& u = cloth ? uC[ew::clothGoodsAt(gx, gy)] : uT[ew::tableGoodsAt(gx, gy)];
            if (u) continue;
            u = true;
            xs.push_back({x, cloth});
          }
      };
      // lines over area A (two or more each, an aisle apart), the first near idealY
      auto lines = [&](const IRect& A, int midX, int idealY, int lv, int qx, int qy, int maxLines) {
        std::vector<int> lineYs;
        for (int line = 0; line < maxLines && need > 0; line++) {
          int bestY = -1, bestS = -(1 << 30);
          std::vector<std::pair<int, bool>> bestXs, xs;
          const int ideal = lineYs.empty() ? idealY : lineYs.back() + 5;
          for (int y = A.y; y <= A.y + A.h; y++) {
            bool close = false;   // an aisle's width from the other lines (and from the stalls: unitOk)
            for (int ly : lineYs) if (std::abs(y - ly) < 5) close = true;
            if (close) continue;
            lineAt(A, y, need, midX, lv, qx, qy, xs);
            if (xs.size() < 2) continue;   // a line is two or more (a lone table in the open reads as dropped there)
            const int sc = (int)xs.size() * 100 - std::abs(y - ideal) * 8;
            if (sc > bestS) { bestS = sc; bestY = y; bestXs = xs; }
          }
          if (bestY < 0) break;
          lineYs.push_back(bestY);
          for (auto& u : bestXs) putUnit(u.first, bestY, u.second);
        }
      };
      // the main square: centred under the stall rows, a wide aisle behind the last row, within two aisles of the rows
      {
        int mid = Z.x + Z.w / 2, lo = 1 << 30, hi = -(1 << 30), top = 1 << 30, bot = -(1 << 30);
        for (const Stall& s : stalls)
          if (facingOf(s) <= art::StallN && std::find(rowYs.begin(), rowYs.end(), s.y) != rowYs.end()) {
            lo = std::min(lo, s.x); hi = std::max(hi, s.x); top = std::min(top, s.y); bot = std::max(bot, s.y);
          }
        IRect A = all;
        if (lo <= hi) {
          mid = (lo + hi) / 2;
          const int y0 = std::max(all.y, top - 2 * rowGap - 2), y1 = std::min(all.y + all.h, bot + 2 * rowGap + 2);
          A = IRect{all.x, y0, all.w, y1 - y0};
        }
        lines(A, mid, rowYs.empty() ? Z.y + Z.h / 2 : bot + rowGap, lv0, CX, CY, 4);
      }
      // what is left: a table at the end of a stall row, a walkway past its last counter (the row runs on in the open)
      for (size_t ri = 0; ri < rowYs.size() && need > 0; ri++) {
        const int y = rowYs[ri];
        int lo = 1 << 30, hi = -(1 << 30);
        for (const Stall& s : stalls) if (s.y == y && facingOf(s) <= art::StallN) { lo = std::min(lo, s.x); hi = std::max(hi, s.x); }
        for (int x : {hi + 5, lo - 6}) {
          if (need < 1 || !unitOk(x, y, lv0, CX, CY)) continue;
          const bool cloth = (made % clothEvery) == clothEvery - 1;
          if (cloth ? usedC[ew::clothGoodsAt(O.gx + x, O.gy + y)] : usedT[ew::tableGoodsAt(O.gx + x, O.gy + y)]) continue;
          putUnit(x, y, cloth);
        }
      }
      // a city's other market places: a line of tables on its nearest other squares
      if (city && need > 1)
        for (size_t k = 1; k < squares.size() && need > 1; k++) {
          const Square& Q = squares[k];
          if (!in(Q.x, Q.y) || std::abs(Q.x - S.x) + std::abs(Q.y - S.y) > 60) continue;
          const int r = (int)(Q.r * 1.25f) + 2;
          lines(IRect{Q.x - r, Q.y - r, 2 * r + 1, 2 * r + 1}, Q.x, Q.y + 3, lvl[I(Q.x, Q.y)], Q.x, Q.y, 1);
        }
      // (M3) a market the square left short of its size (a cramped square: the houses of a strung-out river town
      // crowd it) spills over: a line of tables on the town's second square, then single tables on whatever open
      // paving the square still has. Only when short: a market of the right size keeps its approved layout.
      const int minVendors = town ? 3 : (capital ? 9 : 6);
      if ((int)stalls.size() + made < minVendors) {
        for (size_t k = 1; k < squares.size() && (int)stalls.size() + made < minVendors; k++) {
          const Square& Q = squares[k];
          if (!in(Q.x, Q.y) || std::abs(Q.x - S.x) + std::abs(Q.y - S.y) > 60) continue;
          const int r = (int)(Q.r * 1.25f) + 2;
          need = std::max(need, minVendors - (int)stalls.size() - made);
          lines(IRect{Q.x - r, Q.y - r, 2 * r + 1, 2 * r + 1}, Q.x, Q.y + 3, lvl[I(Q.x, Q.y)], Q.x, Q.y, 1);
        }
        for (int y = all.y - 4; y <= all.y + all.h + 4 && (int)stalls.size() + made < minVendors; y++)
          for (int x = all.x - 4; x <= all.x + all.w + 4 && (int)stalls.size() + made < minVendors; x++) {
            if (!unitOk(x, y, lv0, CX, CY)) continue;
            const bool cloth = (made % clothEvery) == clothEvery - 1;
            if (cloth ? usedC[ew::clothGoodsAt(O.gx + x, O.gy + y)] : usedT[ew::tableGoodsAt(O.gx + x, O.gy + y)]) continue;
            putUnit(x, y, cloth);
          }
      }
    }
  }

  // ---- stock stacked at the ends of each group (where the walkway between two groups is wide enough to spare a
  // tile), the traders' carts at the row ends, the keepers behind their counters
  // (stall facings) along a row: the tile past each end of a stall's counter, and the stall three tiles on
  std::vector<std::pair<int, int>> ends;   // (tile index, stall)
  for (size_t i = 0; i < stalls.size(); i++) {
    const Stall& s = stalls[i];
    const int f = facingOf(s);
    const Frame F = frameOf(f);
    bool loN = false, hiN = false;   // a stall touching on that side
    int loGap = 99, hiGap = 99;
    for (const Stall& o : stalls) {
      if (&o == &s || facingOf(o) != f) continue;
      const int da = (o.x - s.x) * F.ax + (o.y - s.y) * F.ay, dp = (o.x - s.x) * F.ay + (o.y - s.y) * F.ax;
      if (dp != 0 || da == 0) continue;
      if (da == -stepOf(f)) loN = true;
      if (da == stepOf(f)) hiN = true;
      if (da < 0) loGap = std::min(loGap, -da - 3);
      if (da > 0) hiGap = std::min(hiGap, da - 3);
    }
    // (a walkway between two groups takes stock on one side only, so two tiles of it stay open)
    const int loA = f >= art::StallE ? -3 : -2, hiA = f >= art::StallE ? 1 : 2;   // past the counter's ends
    if (!loN && loGap >= 5) ends.push_back({(int)I(s.x + F.ax * loA, s.y + F.ay * loA), (int)i});
    if (!hiN && hiGap >= 3) ends.push_back({(int)I(s.x + F.ax * hiA, s.y + F.ay * hiA), (int)i});
  }
  auto clutterOk = [&](int x, int y) {
    if (!in(x, y)) return false;
    const size_t i = I(x, y);
    const uint8_t k = mask[i];
    if (k != K_SQUARE && k != K_NONE && k != K_YARD) return false;
    if (M.prop[i] || front[i] || reserved[i] || M.bldgAt[i] >= 0 || M.wall[i] || groundSolid(M.at(x, y)) || water[i]) return false;
    if (y > CY - 3 && y < CY + 3 && std::abs(x - CX) < 4) return false;   // the centrepiece's plaza
    return true;
  };
  for (auto& e : ends) {
    const Stall& s = stalls[(size_t)e.second];
    const Frame F = frameOf(facingOf(s));
    const int x = e.first % W, y = e.first / W;
    if (!clutterOk(x, y) || lvl[I(x, y)] != lvl[I(s.x, s.y)]) continue;
    const uint32_t h = hashAt(x, y, 401u);
    if ((h >> 7) % 3 == 0) continue;   // not every end is stacked
    if (!putSolid(x, y, stockFor(s.trade, h))) continue;
    reserved[I(x, y)] = 1;
    // a second piece behind it at some ends
    if ((h >> 3) % 3 != 0 && clutterOk(x - F.fx, y - F.fy)) putSolid(x - F.fx, y - F.fy, (h >> 5) & 1 ? Prop::Barrel : Prop::Crate);
  }
  // the traders' carts (a market town's wagons by its stalls; a village's by its stall): past a row's end, or behind it
  const int carts = mk ? 2 + rng.irange(2) : (city ? 1 + rng.irange(2) : 1);
  {
    std::vector<std::pair<int, int>> spots;
    for (const Stall& s : stalls) {
      const int f = facingOf(s);
      const Frame F = frameOf(f);
      bool lft = true, rgt = true;
      for (const Stall& o : stalls) {
        if (&o == &s || facingOf(o) != f) continue;
        const int da = (o.x - s.x) * F.ax + (o.y - s.y) * F.ay, dp = (o.x - s.x) * F.ay + (o.y - s.y) * F.ax;
        if (dp == 0 && std::abs(da) <= 9) { if (da < 0) lft = false; if (da > 0) rgt = false; }
      }
      for (int dir : {-1, 1}) {
        if ((dir < 0 && !lft) || (dir > 0 && !rgt)) continue;
        if (f <= art::StallN) {
          spots.push_back({s.x + dir * 4, s.y});
          spots.push_back({s.x + dir * 5, s.y});
          spots.push_back({s.x + dir * 4, s.y - F.fy});
        } else {   // a side row's cart stands across the column's end, by its keeper's walk
          spots.push_back({s.x - F.fx, dir < 0 ? s.y - 4 : s.y + 2});
          spots.push_back({s.x - 2 * F.fx, dir < 0 ? s.y - 4 : s.y + 2});
        }
      }
    }
    std::vector<std::pair<int, int>> made;
    for (auto& sp : spots) {
      if ((int)made.size() >= carts) break;
      const int x = sp.first, y = sp.second;
      if (!clutterOk(x, y) || !clutterOk(x - 1, y) || !clutterOk(x + 1, y) || cover[I(x, y)] || tallNear(x - 1, y - 1, x + 1, y)) continue;
      bool near = false;
      for (auto& m : made) if (std::abs(m.first - x) + std::abs(m.second - y) < 6) near = true;
      if (near) continue;
      if (!putSolid(x, y, Prop::Cart)) continue;
      for (int dx = -1; dx <= 1; dx++) reserved[I(x + dx, y)] = 1;
      made.push_back({x, y});
    }
  }
  // the keepers: someone behind every counter, on the inside of whichever way it faces
  for (const Stall& s : stalls) {
    const art::StallKeeperSpot k = art::stallKeeperSpot(facingOf(s));
    addSpawn(Role::Merchant, s.x + k.postDx, s.y + k.postDy);
  }

  // ---- what the place lives from, in plain sight of its market: beside the stall of its own trade, at the market's
  // edge (a village's green), never in the middle of the walking space
  {
    Prop show[2];
    switch (spec) {
      case Specialty::Lumber: show[0] = Prop::LogPile; show[1] = Prop::Woodpile; break;
      case Specialty::Mining: show[0] = Prop::Crate; show[1] = Prop::Barrel; break;   // (the ore is in the mine's yard)
      case Specialty::Fishing: show[0] = Prop::DryingRack; show[1] = Prop::Barrel; break;
      case Specialty::Herding: show[0] = Prop::HideRack; show[1] = Prop::Sacks; break;
      default: show[0] = Prop::Sacks; show[1] = Prop::Haystack; break;
    }
    int ax = S.x, ay = S.y;
    for (const Stall& s : stalls) if (s.trade == own) { ax = s.x; ay = s.y; break; }
    int done = 0;
    for (int r = 3; r <= 7 && done < 2; r++)
      for (int oy = -r; oy <= r && done < 2; oy++)
        for (int ox = -r; ox <= r && done < 2; ox++) {
          if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
          const int x = ax + ox, y = ay + oy;
          if (!clutterOk(x, y) || cover[I(x, y)] || hashAt(x, y, 503u) % 3 == 0) continue;
          const Prop p = show[done];
          const bool tall = !lowProp(p);
          if (tall && tallNear(x - 1, y - 2, x + 1, y + 1)) continue;
          bool edge = mask[I(x, y)] != K_SQUARE;
          for (int d = 0; d < 4 && !edge; d++) if (get(x + D4X[d], y + D4Y[d]) != K_SQUARE) edge = true;
          if (!edge) continue;
          if (!putSolid(x, y, p)) continue;
          if (mask[I(x, y)] == K_NONE) set(x, y, K_YARD);
          done++;
        }
  }
}

// The squares that hold no market: benches round the centrepiece, a shade tree at the edge, a lamp
void Gen::squareDress() {
  // the main square's plaza south of the market: benches facing the fountain or the well, a lamp either side
  if (!squares.empty() && cpX >= 0 && !village) {
    auto ok = [&](int x, int y) {
      if (!in(x, y) || get(x, y) != K_SQUARE) return false;
      const size_t i = I(x, y);
      if (M.prop[i] || front[i] || cover[i] || reserved[i]) return false;
      for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++) if (in(x + ox, y + oy) && (M.bldgAt[I(x + ox, y + oy)] >= 0 || reserved[I(x + ox, y + oy)])) return false;
      return true;
    };
    // (M1 fixer round 2) how the plaza is dressed is the town's own: benches either side and one lamp; a pair of shade
    // trees with a bench under each; or lamps on a diagonal and a single bench (never the same pair of lamps flanking
    // every centrepiece)
    const int style = (int)(hashAt(cpX, cpY, 563u) % 3u);
    const int one = (hashAt(cpX, cpY, 569u) & 1) ? 1 : -1;
    const Prop shade = bio == Biome::Desert ? Prop::PalmTree : (bio == Biome::Snow || bio == Biome::Taiga ? Prop::PineTree : Prop::OakTree);
    auto treeAt = [&](int x, int y) {
      bool clear = ok(x, y);
      for (int oy = -3; oy <= 1 && clear; oy++)
        for (int ox = -2; ox <= 2; ox++) if (in(x + ox, y + oy) && (M.prop[I(x + ox, y + oy)] || front[I(x + ox, y + oy)] || reserved[I(x + ox, y + oy)])) { clear = false; break; }
      return clear && putSolid(x, y, shade);
    };
    if (style == 0) {
      for (int sd : {-1, 1})
        for (int t = 3; t <= 4; t++) if (ok(cpX + sd * t, cpY + 2)) { M.setProp(cpX + sd * t, cpY + 2, Prop::Bench); break; }
      for (int t = 5; t <= 7; t++) if (ok(cpX + one * t, cpY) && ok(cpX + one * t, cpY - 1) && putSolid(cpX + one * t, cpY, Prop::Lamppost)) break;
    } else if (style == 1) {
      for (int sd : {-1, 1})
        for (int t = 5; t <= 8; t++) {
          if (!treeAt(cpX + sd * t, cpY + 1)) continue;
          if (ok(cpX + sd * t - sd, cpY + 2)) M.setProp(cpX + sd * t - sd, cpY + 2, Prop::Bench);
          break;
        }
    } else {
      for (int t = 5; t <= 7; t++) if (ok(cpX - one * t, cpY - 2) && ok(cpX - one * t, cpY - 3) && putSolid(cpX - one * t, cpY - 2, Prop::Lamppost)) break;
      for (int t = 5; t <= 7; t++) if (ok(cpX + one * t, cpY + 2) && ok(cpX + one * t, cpY + 1) && putSolid(cpX + one * t, cpY + 2, Prop::Lamppost)) break;
      for (int t = 2; t <= 3; t++) if (ok(cpX + one * t, cpY + 2)) { M.setProp(cpX + one * t, cpY + 2, Prop::Bench); break; }
    }
    if (city && style != 1)   // a capital's or a city's plaza: a tree in one far corner
      for (int t = 8; t <= 10; t++) if (treeAt(cpX - one * t, cpY + 2)) break;
    // (M1 fixer) a city's plaza across from its market is a place of its own: a statue of the founder on the far
    // side of the fountain, benches before it, so the open paving reads as a plaza and not as unused ground
    if (city) {
      const int ddx = mktSide == 2 ? -1 : (mktSide == 3 ? 1 : 0), ddy = mktSide == 0 ? 1 : (mktSide == 1 ? -1 : 0);
      for (int d = 9; d >= 4; d--) {
        const int x = cpX + ddx * d, y = cpY + ddy * d;
        bool clear = ok(x, y);
        for (int oy = -2; oy <= 2 && clear; oy++)
          for (int ox = -2; ox <= 2; ox++)
            if (!in(x + ox, y + oy) || get(x + ox, y + oy) != K_SQUARE || M.prop[I(x + ox, y + oy)] || reserved[I(x + ox, y + oy)] || front[I(x + ox, y + oy)]) { clear = false; break; }
        if (!clear || !putSolid(x, y, Prop::Statue)) continue;
        for (int sd : {-2, 2}) if (ok(x + sd, y + 2)) M.setProp(x + sd, y + 2, Prop::Bench);
        break;
      }
    }
  }
  for (size_t k = 1; k < squares.size(); k++) {
    const Square& s = squares[k];
    auto ok = [&](int x, int y) {
      if (!in(x, y) || get(x, y) != K_SQUARE) return false;
      const size_t i = I(x, y);
      if (M.prop[i] || front[i] || cover[i] || reserved[i]) return false;
      for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++) if (bldgAt(x + ox, y + oy) && in(x + ox, y + oy)) return false;
      return true;
    };
    // benches either side of the centrepiece, facing it from the north
    for (int sd : {-1, 1}) {
      for (int t = 2; t <= 3; t++) {
        const int x = s.x + sd * t, y = s.y - 2;
        if (ok(x, y)) { M.setProp(x, y, Prop::Bench); break; }
      }
    }
    // a shade tree on the square's edge, away from the centrepiece
    const Prop tree = bio == Biome::Desert ? Prop::PalmTree : (bio == Biome::Snow || bio == Biome::Taiga ? Prop::PineTree : (bio == Biome::Autumn ? Prop::AutumnTree : Prop::OakTree));
    int trees = 0;
    for (int y = s.y - (int)s.r - 1; y <= s.y + (int)s.r + 1 && trees < 1 + (city ? 1 : 0); y++)
      for (int x = s.x - (int)(s.r * 1.25f) - 1; x <= s.x + (int)(s.r * 1.25f) + 1 && trees < 1 + (city ? 1 : 0); x++) {
        if (std::abs(x - s.x) + std::abs(y - s.y) < 4 || !ok(x, y)) continue;
        bool edge = false;
        for (int d = 0; d < 4; d++) if (get(x + D4X[d], y + D4Y[d]) != K_SQUARE) edge = true;
        if (!edge || hashAt(x, y, 433u) % 5 != 0) continue;
        bool clear = true;
        for (int oy = -3; oy <= 1 && clear; oy++)
          for (int ox = -2; ox <= 2; ox++) if (in(x + ox, y + oy) && (M.prop[I(x + ox, y + oy)] || front[I(x + ox, y + oy)])) { clear = false; break; }
        if (!clear || !putSolid(x, y, tree)) continue;
        trees++;
      }
  }
}

// ------------------------------------------------------------------------------------------------ the trades' yards
void Gen::tradeYards() {
  auto yardOk = [&](int x, int y, bool tall) {
    if (!freeTile(x, y) || inCompound(x, y, 1) || reserved.empty() || reserved[I(x, y)]) return false;
    if (tall && cover[I(x, y)]) return false;
    return get(x, y) != K_FIELD;
  };
  // (M1 fixer) every solid yard prop leaves the ways round it open (putSolid)
  auto put = [&](int x, int y, Prop p) {
    const bool tall = !lowProp(p);
    if (!yardOk(x, y, tall)) return false;
    if (!putSolid(x, y, p)) return false;
    set(x, y, K_YARD);
    return true;
  };
  // round a building: its sides (low stock against the walls), then two steps out (tall frames clear of its roof)
  auto around = [&](const Bldg& b, Prop p, int n) {
    int done = 0;
    const bool tall = !lowProp(p);
    const int off = tall ? 2 : 1;
    const int ys[3] = {b.r.y + b.r.h - 1, b.r.y + b.r.h - 2, b.r.y};
    for (int k = 0; k < 6 && done < n; k++) {
      const bool west = ((hashAt(b.r.x, b.r.y, 457u) >> k) & 1) != 0;
      const int x = west ? b.r.x - off : b.r.x + b.r.w - 1 + off;
      const int y = ys[k % 3];
      if (put(x, y, p)) done++;
    }
    return done;
  };
  for (const Bldg& b : M.bldgs) {
    switch (b.type) {
      case Building::Windmill: case Building::Watermill: around(b, Prop::Sacks, 2); break;
      case Building::Granary: around(b, Prop::Sacks, 2); around(b, Prop::Haystack, 1); break;
      case Building::Bakery: around(b, Prop::Woodpile, 1); around(b, Prop::Sacks, 1); break;
      case Building::Butcher: around(b, Prop::Barrel, 1); break;
      case Building::Fishmonger: around(b, Prop::Barrel, 1); around(b, Prop::DryingRack, 1); break;
      case Building::Smelter: around(b, Prop::OrePile, 1); around(b, Prop::Woodpile, 1); break;
      case Building::Sawmill: around(b, Prop::LogPile, 2); around(b, Prop::Woodpile, 1); break;
      case Building::Tanner: around(b, Prop::HideRack, 2); around(b, Prop::Barrel, 1); break;
      case Building::Weaver: around(b, Prop::Baskets, 1); break;
      default: break;
    }
  }
  // (M1 fixer) the trade by the road in: where a main street leaves the houses, the first thing a traveller passes
  // says what the place lives from (logs waiting for the cart, an ore cart, haystacks, a trough, a drying rack)
  {
    Prop by[2];
    switch (spec) {
      case Specialty::Lumber: by[0] = Prop::LogPile; by[1] = Prop::Stump; break;
      case Specialty::Mining: by[0] = Prop::OreCart; by[1] = Prop::OrePile; break;
      case Specialty::Fishing: by[0] = Prop::DryingRack; by[1] = Prop::Barrel; break;
      case Specialty::Herding: by[0] = Prop::HideRack; by[1] = Prop::Barrel; break;   // (the beasts are in their pen)
      default: by[0] = Prop::Haystack; by[1] = Prop::Sacks; break;
    }
    int made = 0;
    const int want = village ? 2 : 3;
    std::vector<std::pair<int, int>> at;
    for (size_t i = 0; i < mainTiles.size() && made < want; i++) {
      const auto& s = mainTiles[(i * 7919u) % mainTiles.size()];
      const float d = dist(s.first, s.second);
      if (d < 0.8f || d > 1.15f) continue;
      bool far = true;
      for (auto& a : at) if (std::abs(a.first - s.first) + std::abs(a.second - s.second) < 12) far = false;
      if (!far) continue;
      for (int k = 0; k < 4; k++) {
        const int x = s.first + D4X[k] * 2, y = s.second + D4Y[k] * 2;
        if (!in(x, y) || cover[I(x, y)] || get(x + D4X[k] * -1, y + D4Y[k] * -1) == K_FIELD) continue;
        if (!put(x, y, by[0])) continue;
        // its companion a step further from the road
        put(x + (D4X[k] ? D4X[k] : 1), y + D4Y[k], by[1]) || put(x + (D4X[k] ? 0 : -1), y + (D4Y[k] ? D4Y[k] : 1), by[1]);
        // (M1 fixer round 2) on a trodden yard of its own, not loose on the grass (the ground only)
        if (spec != Specialty::Farming && spec != Specialty::None)
          for (int oy = -1; oy <= 1; oy++)
            for (int ox = -1; ox <= 1; ox++) {
              const int qx = x + ox, qy = y + oy;
              if (!in(qx, qy) || water[I(qx, qy)] || M.bldgAt[I(qx, qy)] >= 0 || front[I(qx, qy)] || lvl[I(qx, qy)] != lvl[I(x, y)]) continue;
              const uint8_t km = get(qx, qy);
              if ((km == K_NONE || km == K_YARD) && !groundSolid(M.at(qx, qy)) && M.at(qx, qy) != Ground::Bridge && (std::abs(ox) + std::abs(oy) < 2 || hashAt(qx, qy, 599u) % 3u))
                M.setG(qx, qy, Ground::Dirt);
            }
        at.push_back({s.first, s.second});
        made++;
        break;
      }
    }
  }
  // (M1 fixer round 2) the herders' work yard: a fenced pen against the tannery, a lean-to for the flock at its back,
  // the sheep (and a cow or two) inside, the trough and the hay, a gate toward the yard
  if (spec == Specialty::Herding) {
    int tb = -1;
    for (size_t i = 0; i < M.bldgs.size(); i++) if (M.bldgs[i].type == Building::Tanner) { tb = (int)i; break; }
    if (tb >= 0) {
      const Bldg b = M.bldgs[(size_t)tb];
      const int L0 = lvl[I(b.r.x, b.r.y + b.r.h - 1)];
      auto penOk = [&](int x0, int y0, int pw, int ph) {
        // (M3 terraced: on whichever terrace by the tannery has the room, all of it on one level)
        const int L = terraced && in(x0, y0) ? (int)lvl[I(x0, y0)] : L0;
        for (int y = y0 - 1; y <= y0 + ph; y++)
          for (int x = x0 - 1; x <= x0 + pw; x++) {
            if (!in(x, y) || x < 1 || y < 1 || x >= W - 1 || y >= H - 1) return false;
            const size_t i = I(x, y);
            const bool inner = x >= x0 && x < x0 + pw && y >= y0 && y < y0 + ph;
            if (M.bldgAt[i] >= 0 || M.wall[i] || front[i]) return false;   // the ring: nobody's house or door
            if (!inner) continue;
            if (mask[i] != K_NONE || M.prop[i] || cover[i] || water[i] || noBuild[i] || lvl[i] != L || inCompound(x, y, 1) || face(x, y)) return false;
            if (groundSolid(M.at(x, y)) || M.at(x, y) == Ground::Bridge || (!reserved.empty() && reserved[i])) return false;
          }
        return true;
      };
      static const int sizes[6][2] = {{8, 5}, {7, 5}, {6, 5}, {5, 5}, {7, 4}, {6, 4}};
      bool done = false;
      const int reach = village ? 16 : 10;
      for (int si = 0; si < 6 && !done; si++) {
        const int pw = sizes[si][0], ph = sizes[si][1];
        // as near the tannery as it fits (beside it best, its front rows level with the building's; else behind it or
        // a little way off across a lane), never in front of its door
        struct PC { int x, y, d; };
        std::vector<PC> cand;
        for (int y0 = b.r.y - ph - reach + 2; y0 <= b.r.y + b.r.h + reach / 2; y0++)
          for (int x0 = b.r.x - pw - reach; x0 <= b.r.x + b.r.w + reach; x0++) {
            const int gx0 = std::max(b.r.x - (x0 + pw), x0 - (b.r.x + b.r.w)), gy0 = std::max(b.r.y - (y0 + ph), y0 - (b.r.y + b.r.h));
            if (y0 + ph > b.r.y + b.r.h + 1 && x0 + pw > b.r.x - 1 && x0 < b.r.x + b.r.w + 1) continue;   // its front
            const int d = std::max(0, gx0) * 2 + std::max(0, gy0) * 3 + std::abs((y0 + ph) - (b.r.y + b.r.h));
            cand.push_back({x0, y0, d});
          }
        std::stable_sort(cand.begin(), cand.end(), [](const PC& a, const PC& c) { return a.d < c.d; });
        for (size_t ci = 0; ci < cand.size() && !done && ci < 1500; ci++) {
          const int x0 = cand[ci].x, y0 = cand[ci].y;
          if (!penOk(x0, y0, pw, ph)) continue;
          const bool west = x0 + pw / 2 < b.r.cx();
          // the fence (a gate in the side toward the tannery, at its front row; a post that would shut a way stays out)
          const int gx = west ? x0 + pw - 1 : x0, gy = y0 + ph - 2;
          std::vector<int> fence;
          int gaps = 0;
          for (int y = y0; y < y0 + ph; y++)
            for (int x = x0; x < x0 + pw; x++) {
              set(x, y, K_FIELD);
              const bool edgeY = y == y0 || y == y0 + ph - 1, edgeX = x == x0 || x == x0 + pw - 1;
              if ((!edgeY && !edgeX) || (x == gx && y == gy)) continue;
              if (!putSolid(x, y, edgeY ? Prop::FenceH : Prop::FenceV)) { gaps++; continue; }
              fence.push_back((int)I(x, y));
            }
          if (gaps > 0) {   // a pen with holes is no pen: take it down
            for (int f : fence) M.setP(f % W, f / W, 0);
            for (int y = y0; y < y0 + ph; y++) for (int x = x0; x < x0 + pw; x++) set(x, y, K_NONE);
            continue;
          }
          // the lean-to at the back, the hay and the trough, the beasts
          const int sx = x0 + pw / 2, sy = y0 + 1;
          if (M.propAt(sx - 1, sy) == 0 && M.propAt(sx + 1, sy) == 0 && putSolid(sx, sy, Prop::PenShelter)) {
            if (!putSolid(sx - 1, sy, Prop::Filler) || !putSolid(sx + 1, sy, Prop::Filler)) { M.setP(sx - 1, sy, 0); M.setP(sx, sy, 0); M.setP(sx + 1, sy, 0); }
          }
          // the trough inside a roomy pen; outside by the gate where the pen is small
          if (pw >= 7) putSolid(west ? x0 + 1 : x0 + pw - 2, y0 + ph - 2, Prop::Trough);
          else put(west ? x0 + pw : x0 - 1, y0 + ph - 1, Prop::Trough);
          const bool cows = hashAt(x0, y0, 577u) % 3u == 0;
          int beasts = 0;
          for (int y = y0 + 2; y < y0 + ph - 1; y++)
            for (int x = x0 + 1; x < x0 + pw - 1; x++) {
              if (M.prop[I(x, y)] || beasts >= 5) continue;
              bool crowd = false;   // (never two side by side, nor one right below another)
              for (int k = 0; k < 4; k++) {
                const int q = M.propAt(x + D4X[k], y + D4Y[k]);
                if (q == (int)Prop::Sheep + 1 || q == (int)Prop::Cow + 1) crowd = true;
              }
              if (crowd) continue;
              M.setProp(x, y, cows && beasts == 1 ? Prop::Cow : Prop::Sheep);   // (inside the fence: no way runs through)
              beasts++;
            }
          done = true;
        }
      }
    }
  }
  // the specialisation out at the edge
  if (spec == Specialty::Mining) {
    // the mine: a timbered adit into the highest ground at the edge, near the smelter, a path to the streets, ore
    // carts, the spoil heap. (M1 fixer round 2) The adit is cut into the foot of a hill of its own (Prop::MineHill: a
    // grassy knoll whose south face is a cliff of rock, five tiles across and four deep), the track runs out of it
    // along the path, the yard before it is trodden earth; a mouth no path from the streets can reach is never used.
    int smx = cx, smy = cy;
    for (const Bldg& b : M.bldgs) if (b.type == Building::Smelter) { smx = b.r.cx(); smy = b.r.cy(); }
    struct Cand { int x, y, score; };
    std::vector<Cand> cands;
    for (int y = 6; y < H - 3; y++)
      for (int x = 5; x < W - 5; x++) {
        const float d = dist(x, y);
        if (d < 0.85f || d > 1.35f) continue;
        bool ok = true;
        for (int dx = -1; dx <= 1 && ok; dx++) {
          if (!yardOk(x + dx, y, true) || lvl[I(x + dx, y)] != lvl[I(x, y)]) ok = false;
          for (int dy = 1; dy <= 2 && ok; dy++) if (M.bldgAt[I(x + dx, y - dy)] >= 0 || M.wall[I(x + dx, y - dy)] || front[I(x + dx, y - dy)]) ok = false;
        }
        // (M3) the mouth opens onto a yard of its own, not straight onto a street (its track runs out along the path)
        if (!ok || !walkable(x, y + 1) || M.prop[I(x, y + 1)] || get(x, y + 1) == K_FIELD || isStreet(x, y + 1)) continue;
        // (integer scores: the same order on every platform)
        const int rise = (int)lvl[I(x, y - 2)] - (int)lvl[I(x, y + 1)];
        const int d2 = (x - smx) * (x - smx) + (y - smy) * (y - smy);
        int r = 0;
        while ((r + 1) * (r + 1) <= d2) r++;
        const int score = rise * 300 - r * 8 + (int)(hashAt(x, y, 461u) % 50u);
        cands.push_back({x, y, score});
      }
    std::stable_sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.score > b.score; });
    auto hillOk = [&](int x, int y, int L) {   // a tile of the hill: open ground of the town's own, a tile clear of houses
      if (!in(x, y) || x < 2 || y < 2 || x >= W - 2 || y >= H - 2) return false;
      const size_t i = I(x, y);
      if (mask[i] != K_NONE || water[i] || front[i] || cover[i] || noBuild[i] || lvl[i] != L || M.prop[i]) return false;
      if (reserved[i] || groundSolid(M.at(x, y)) || M.at(x, y) == Ground::Bridge || inCompound(x, y, 1)) return false;
      for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++) {
          const size_t j = I(x + ox, y + oy);
          if (M.bldgAt[j] >= 0 || M.wall[j] || mask[j] == K_FIELD || mask[j] == K_MAIN) return false;
        }
      return true;
    };
    bool mined = false;
    // pass 0: a mouth with room for its hill; pass 1 (a crowded edge): a bare adit any path reaches, the spoil and
    // the boulders round it
    for (int pass = 0; pass < 2 && !mined; pass++) {
      int tries = 0;
      for (size_t ci = 0; ci < cands.size() && tries < (pass ? 300 : 120); ci++) {
        const int bx = cands[ci].x, by = cands[ci].y;
        const bool hill = pass == 0;
        const int L = lvl[I(bx, by)];
        if (hill) {   // (cheap first) room for the hill: its footprint, and a row over it clear of every house's front
          bool room = true;
          for (int dy = 0; dy <= 3 && room; dy++)
            for (int dx = -2; dx <= 2; dx++) {
              if (dy == 0 && std::abs(dx) <= 1) continue;   // (the mouth row's middle: yardOk above)
              if (!hillOk(bx + dx, by - dy, L)) { room = false; break; }
            }
          for (int dx = -2; dx <= 2 && room; dx++) {
            const int x = bx + dx, y = by - 4;
            if (!in(x, y) || M.bldgAt[I(x, y)] >= 0 || front[I(x, y)] || M.wall[I(x, y)]) room = false;
          }
          if (!room) continue;
        }
        tries++;
        // the path from its mouth to the streets (searched first: no path, no mine here)
        std::vector<int> prev((size_t)W * H, -2);
        std::queue<int> q;
        prev[I(bx, by + 1)] = -1;
        q.push((int)I(bx, by + 1));
        int found = -1;
        while (!q.empty() && found < 0) {
          const int c = q.front(); q.pop();
          const int px = c % W, py = c / W;
          if (isStreet(px, py)) { found = c; break; }
          if (std::abs(px - bx) + std::abs(py - by) > 40) continue;
          for (int d = 0; d < 4; d++) {
            const int nx = px + D4X[d], ny = py + D4Y[d];
            if (!in(nx, ny) || prev[I(nx, ny)] != -2 || !walkable(nx, ny) || get(nx, ny) == K_FIELD) continue;
            if (ny <= by && std::abs(nx - bx) <= 2) continue;   // not back through the mouth's or the hill's tiles
            prev[I(nx, ny)] = c;
            q.push((int)I(nx, ny));
          }
        }
        if (found < 0) continue;
        // the mouth (and the hill) are solid: each tile only where the ways round it stay open, else none of it
        std::vector<int> laid;
        bool ok = true;
        auto lay = [&](int x, int y, Prop p) {
          if (!ok) return;
          if (!putSolid(x, y, p)) { ok = false; return; }
          laid.push_back((int)I(x, y));
        };
        lay(bx, by, hill ? Prop::MineHill : Prop::MineEntrance);
        lay(bx - 1, by, Prop::Filler);
        lay(bx + 1, by, Prop::Filler);
        if (hill) {
          lay(bx - 2, by, Prop::Filler);
          lay(bx + 2, by, Prop::Filler);
          for (int dy = 1; dy <= 3; dy++)
            for (int dx = -2; dx <= 2; dx++) lay(bx + dx, by - dy, Prop::Filler);
        }
        if (!ok) { for (int k : laid) M.setP(k % W, k / W, 0); continue; }
        if (hill)
          for (int dy = 0; dy <= 3; dy++)
            for (int dx = -2; dx <= 2; dx++) set(bx + dx, by - dy, K_YARD);
        else
          for (int dx = -1; dx <= 1; dx++) set(bx + dx, by, K_YARD);
        // the path trodden to earth, the track laid along its first stretch out of the adit
        std::vector<int> path;
        for (int c = prev[(size_t)found]; c >= 0; c = prev[(size_t)c]) path.push_back(c);
        path.push_back((int)I(bx, by + 1));
        std::reverse(path.begin(), path.end());   // from the mouth outward
        for (int c : path) {
          const int px = c % W, py = c / W;
          if (get(px, py) == K_NONE || get(px, py) == K_YARD) { set(px, py, K_YARD); M.setG(px, py, Ground::Dirt); M.setP(px, py, 0); }
        }
        // the yard before the mouth: trodden earth a few tiles either side (its ground now, the yard's mark once the
        // carts and the spoil stand on it)
        std::vector<int> yard;
        for (int y = by + 1; y <= by + 2; y++)
          for (int x = bx - 4; x <= bx + 4; x++)
            if (in(x, y) && get(x, y) == K_NONE && !M.prop[I(x, y)] && !front[I(x, y)] && !water[I(x, y)] && lvl[I(x, y)] == L && !groundSolid(M.at(x, y))) {
              M.setG(x, y, Ground::Dirt);
              yard.push_back((int)I(x, y));
            }
        int railed = 0;
        for (int c : path) {
          const int px = c % W, py = c / W;
          if (railed >= 9 || isStreet(px, py) || M.prop[(size_t)c]) break;
          M.setProp(px, py, Prop::MineRail);
          railed++;
        }
        // the ore cart on a spur of track beside the mouth, the spoil heap tipped at the other side
        const int sd = (hashAt(bx, by, 593u) & 1) ? 1 : -1;
        bool cart = false;
        for (int s : {sd, -sd}) {
          if (cart) break;
          if (yardOk(bx + s, by + 1, false) && yardOk(bx + 2 * s, by + 1, true) && put(bx + 2 * s, by + 1, Prop::OreCart)) {
            M.setP(bx + s, by + 1, 0);
            if (!M.prop[I(bx + s, by + 1)]) M.setProp(bx + s, by + 1, Prop::MineRail);
            cart = true;
            put(bx - 3 * s, by + 1, Prop::OrePile) || put(bx - 3 * s, by + 2, Prop::OrePile);
            put(bx - 4 * s, by + 1, Prop::Rock);
            put(bx + 3 * s, by + 2, Prop::Crate);
          }
        }
        if (!cart) { put(bx - 3, by + 1, Prop::OrePile) || put(bx + 3, by + 1, Prop::OrePile); }
        if (!hill) {
          put(bx + 4, by, Prop::Boulder) || put(bx - 4, by, Prop::Boulder);
          put(bx - 3, by, Prop::Rock) || put(bx + 3, by, Prop::Rock);
        }
        for (int k : yard) if (mask[(size_t)k] == K_NONE) mask[(size_t)k] = K_YARD;
        mined = true;
        break;
      }
    }
  } else if (spec == Specialty::Lumber) {
    // stumps where the wood was cut back from the houses, logs waiting to be hauled
    for (int t = 0, n = 0; t < 300 && n < 9; t++) {
      const float a = rng.f() * D_TAU, rr = rng.range(1.0f, 1.3f);
      const int x = cx + (int)std::floor(dcos(a) * rx * rr), y = cy + (int)std::floor(dsin(a) * ry * rr);
      if (!in(x, y) || !yardOk(x, y, false)) continue;
      if (!putSolid(x, y, n % 4 == 3 ? Prop::Log : Prop::Stump)) continue;
      n++;
    }
  } else if (spec == Specialty::Fishing) {
    // drying racks along the shore
    int racks = 0;
    for (int y = 2; y < H - 2 && racks < (village ? 2 : 3); y++)
      for (int x = 2; x < W - 2 && racks < (village ? 2 : 3); x++) {
        if (dist(x, y) > 1.2f || !yardOk(x, y, true) || hashAt(x, y, 467u) % 3) continue;
        bool shore = false;
        for (int oy = 0; oy <= 2 && !shore; oy++)
          for (int ox = -2; ox <= 2; ox++) if (in(x + ox, y + oy) && water[I(x + ox, y + oy)]) { shore = true; break; }
        bool crowded = false;
        for (int oy = -2; oy <= 1 && !crowded; oy++)
          for (int ox = -2; ox <= 2; ox++) {
            const int q = in(x + ox, y + oy) ? M.prop[I(x + ox, y + oy)] : 0;
            if (q && !lowProp((Prop)(q - 1))) { crowded = true; break; }
          }
        if (!shore || crowded) continue;
        if (!putSolid(x, y, Prop::DryingRack)) continue;
        set(x, y, K_YARD);
        racks++;
      }
  }
}


// ------------------------------------------------------------------------------------------------ (M2) the plazas
// The side of a city's main square away from its market (and the paving round a capital's palace approach) could
// lie bare for a dozen tiles each way (owner note 6: "a large empty plaza"). While any paved block of about 8 x 8 or
// more holds nothing, a feature goes up at its middle, in turn: a shade tree with benches under it, a monument
// between two lamps, a well with a bench, a planted tree ringed with flowers. Every solid piece keeps the way (no
// door, gate or street is cut off), and nothing stands on a market's aisles, a keeper's walk or a building's front.
int Gen::largestEmptyPlaza(int& bx, int& by, const std::vector<uint8_t>& skip) const {
  // the largest square of empty paving (dynamic programming over the buffer)
  std::vector<uint16_t> dp((size_t)W * H, 0);
  int best = 0;
  bx = by = -1;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const size_t i = I(x, y);
      const bool empty = M.at(x, y) == Ground::Plaza && !M.prop[i] && M.bldgAt[i] < 0 && !M.wall[i] && !skip[i] && !inCompound(x, y);
      if (!empty) continue;
      int v = 1;
      if (x > 0 && y > 0) v = 1 + std::min({(int)dp[I(x - 1, y)], (int)dp[I(x, y - 1)], (int)dp[I(x - 1, y - 1)]});
      dp[i] = (uint16_t)v;
      if (v > best) { best = v; bx = x - v + 1; by = y - v + 1; }
    }
  return best;
}

int Gen::largestEmptyBand(int& bx, int& by, int& bw, int& bh, int minSide, const std::vector<uint8_t>& skip) const {
  // every maximal empty rectangle is some row's bar extended left and right while the bars stay as tall (a stack over
  // each row's column heights); the largest by area with both sides >= minSide wins
  std::vector<int> hgt((size_t)W, 0), st;
  int best = 0;
  bx = by = -1; bw = bh = 0;
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      const size_t i = I(x, y);
      const bool empty = M.at(x, y) == Ground::Plaza && !M.prop[i] && M.bldgAt[i] < 0 && !M.wall[i] && !skip[i] && !inCompound(x, y);
      hgt[(size_t)x] = empty ? hgt[(size_t)x] + 1 : 0;
    }
    st.clear();
    for (int x = 0; x <= W; x++) {
      const int h = x < W ? hgt[(size_t)x] : 0;
      while (!st.empty() && hgt[(size_t)st.back()] >= h) {
        const int top = st.back();
        st.pop_back();
        const int th = hgt[(size_t)top];
        const int left = st.empty() ? 0 : st.back() + 1, w = x - left;
        if (th >= minSide && w >= minSide && th * w > best) { best = th * w; bx = left; by = y - th + 1; bw = w; bh = th; }
      }
      st.push_back(x);
    }
  }
  return best;
}

void Gen::plazaFill() {
  if (village) return;
  const Prop shade = bio == Biome::Desert ? Prop::PalmTree : (bio == Biome::Snow || bio == Biome::Taiga ? Prop::PineTree : Prop::OakTree);
  std::vector<uint8_t> skip((size_t)W * H, 0);
  auto tileOk = [&](int x, int y) {
    if (!in(x, y)) return false;
    const size_t i = I(x, y);
    if (M.at(x, y) != Ground::Plaza || M.prop[i] || M.bldgAt[i] >= 0 || M.wall[i] || front[i] || cover[i] || (!reserved.empty() && reserved[i])) return false;
    return !inCompound(x, y, 1);
  };
  // a solid piece: the tile and its neighbours free of fronts and aisles, the way kept
  auto solidAt = [&](int x, int y, Prop p, int rise) {
    if (!tileOk(x, y)) return false;
    for (int oy = -rise; oy <= 1; oy++)
      for (int ox = -1; ox <= 1; ox++) {
        if (!in(x + ox, y + oy)) return false;
        const size_t j = I(x + ox, y + oy);
        if (front[j] || (!reserved.empty() && reserved[j]) || M.bldgAt[j] >= 0) return false;
        if (oy < 0 && M.prop[j] && propSolid((Prop)(M.prop[j] - 1))) return false;   // nothing tall under a crown
      }
    return putSolid(x, y, p);
  };
  auto low = [&](int x, int y, Prop p) { if (tileOk(x, y)) M.setProp(x, y, p); };
  int made = 0;
  for (int guard = 0; guard < 40; guard++) {
    // (M2 fixer round 3, review: "a long, bare, empty paved band south of the market") not only square blocks of 8 x
    // 8: any bare rectangle of 45 tiles or more at least 5 deep (a band 20 x 6 took nothing before), its middle first
    int bx, by, bw, bh;
    const int area = largestEmptyBand(bx, by, bw, bh, 5, skip);
    if (area < 45) break;
    const int side = std::max(bw, bh);
    const int cx0 = bx + bw / 2, cy0 = by + bh / 2;
    bool done = false;
    // (M2 fixer round 3) the turn's feature first, tried on the middle of the block and then the tiles round it;
    // where it does not fit (a tree's crown in a shallow band) the next kind; a well or a monument never repeats one
    // standing within a dozen tiles (two wells side by side read as a copy), and the last resort is low: a pair of
    // benches between flower beds
    auto nearSame = [&](int x, int y, Prop p) {
      for (int oy = -12; oy <= 12; oy++)
        for (int ox = -12; ox <= 12; ox++)
          if (in(x + ox, y + oy) && M.prop[I(x + ox, y + oy)] == (int)p + 1) return true;
      return false;
    };
    const int first = (made + (int)(hashAt(bx, by, 911u) & 1)) % 4;
    for (int kk = 0; kk < 5 && !done; kk++) {
      const int kind = kk < 4 ? (first + kk) % 4 : 4;
      for (int r = 0; r <= 2 && !done; r++)
        for (int oy = -r; oy <= r && !done; oy++)
          for (int ox = -r; ox <= r && !done; ox++) {
            if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
            const int x = cx0 + ox, y = cy0 + oy;
            switch (kind) {
              case 0:   // a shade tree, a bench either side under it
                if (!solidAt(x, y, shade, 3)) continue;
                low(x - 1, y + 1, Prop::Bench); low(x + 1, y + 1, Prop::Bench);
                break;
              case 1:   // a monument between two lamps
                if (nearSame(x, y, Prop::Statue) || !solidAt(x, y, Prop::Statue, 2)) continue;
                if (tileOk(x - 2, y) && tileOk(x - 2, y - 1)) putSolid(x - 2, y, Prop::Lamppost);
                if (tileOk(x + 2, y) && tileOk(x + 2, y - 1)) putSolid(x + 2, y, Prop::Lamppost);
                low(x, y + 2, Prop::Bench);
                break;
              case 2:   // a well, a bench facing it
                if (nearSame(x, y, Prop::Well) || !solidAt(x, y, Prop::Well, 1)) continue;
                low(x, y + 2, Prop::Bench);
                if (tileOk(x + 2, y) && tileOk(x + 2, y - 1)) putSolid(x + 2, y, Prop::Lamppost);
                break;
              case 3:   // a planted tree ringed with flowers
                if (!solidAt(x, y, shade, 3)) continue;
                low(x - 1, y, Prop::Flowers1); low(x + 1, y, Prop::Flowers3); low(x - 1, y + 1, Prop::Flowers2); low(x + 1, y + 1, Prop::Flowers1);
                break;
              default:  // two benches between flower beds (nothing solid)
                if (!tileOk(x - 2, y) || !tileOk(x - 1, y) || !tileOk(x, y) || !tileOk(x + 1, y) || !tileOk(x + 2, y)) continue;
                low(x - 2, y, Prop::Flowers2); low(x - 1, y, Prop::Bench); low(x, y, Prop::Flowers1); low(x + 1, y, Prop::Bench); low(x + 2, y, Prop::Flowers3);
                break;
            }
            done = true;
          }
    }
    if (done) made++;
    else
      for (int y = by; y < by + bh; y++)   // nothing fits this block (aisles, fronts): it stays as it is
        for (int x = bx; x < bx + bw; x++) skip[I(x, y)] = 1;
    (void)side;
  }
}

// (M2, owner note 6) a city's main square is where the town meets: idlers, shoppers, a crier, children, on the
// paving round the centrepiece and the market (the SIM lane keeps them lingering there by day)
void Gen::squareFolk() {
  if (!city || squares.empty()) return;
  const Square& s = squares[0];
  std::vector<std::pair<int, int>> spots;
  for (int y = s.y - 13; y <= s.y + 13; y++)
    for (int x = s.x - 13; x <= s.x + 13; x++) {
      if (!in(x, y) || get(x, y) != K_SQUARE || !walkable(x, y)) continue;
      const size_t i = I(x, y);
      if (M.prop[i] || (!reserved.empty() && reserved[i]) || front[i]) continue;
      if ((x - s.x) * (x - s.x) + (y - s.y) * (y - s.y) > 13 * 13) continue;
      spots.push_back({x, y});
    }
  const int want = capital ? 14 : 8;
  for (int k = 0; k < want && !spots.empty(); k++) {
    const size_t pick = (size_t)rng.irange((int)spots.size());
    const auto p = spots[pick];
    spots.erase(spots.begin() + (long)pick);
    // no two on one tile or side by side in a line: a little room round each
    spots.erase(std::remove_if(spots.begin(), spots.end(), [&](const std::pair<int, int>& q) { return std::abs(q.first - p.first) <= 1 && std::abs(q.second - p.second) <= 1; }), spots.end());
    addSpawn(k % 5 == 4 ? Role::Child : Role::Villager, p.first, p.second);
  }
}

}  // namespace town
}  // namespace ew
