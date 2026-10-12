// M7c "Order of Streets" (VISION_PLAN 15.M7c): the planned city's street network inside the walls. CITY lane.
//
// The M7b plan laid the avenues (every gate to the market square, the market to the seat), the precincts, the park and
// a few cross streets with winding lanes between; the city did not read as planned. Here the streets off the avenues
// are a real lattice of blocks:
//  - the city's KIND of plan (cityPick) by its culture's layout, age and order: the heartland's market grid round its
//    cathedral close, the imperial lattice, the jade axial plan, the courtyard blocks of the dune and the highlands,
//    the steppe's rings round the khan's court, the fjord's terraces along the harbour, the river city's grid bent to its
//    banks, the marsh boardwalks, the elves' wide blocks among their trees; its block sizes from the kind, the age (an
//    old city denser) and the plan's seed;
//  - the WALL LANE round the inside of the wall (parallel to its straight and slanted runs), a LANE ALONG EVERY BANK
//    inside the walls (the quays' street, bending with the water), the RINGS of the rings plan, the SPINE along the
//    water of the harbour and river plans;
//  - SECTIONS: the land the avenues, rings, lanes and water cut the city into. Each section lays its own lattice:
//    east-west BLOCK STREETS (three wide: a carriage lane between two walks, the row houses' fronts) a block apart,
//    anchored on the avenue it borders (its rows parallel to that avenue's run, so the lattice turns where the avenue
//    does and staggers across it), then NORTH-SOUTH LANES (some three-wide streets) between them a block apart. A street
//    is laid only from a street to a street (no stub, no dead end), never alongside another (no lane beside a lane, no
//    sliver of land between two), and a block keeps the depth its houses need;
//  - ROLE BLOCKS (cityRoleBlocks): a few whole blocks kept as orchards, kitchen gardens or craft yards where the city has
//    the frontage to spare.
// No draw from the generator's Rng (every choice hashed from the plan's seed). Integer and dmath maths only.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>
#include "rpg/culture/society.h"
#include "rpg/world/town_gen.h"
#include "rpg/world/urban.h"

namespace ew {
namespace town {

using art::Building;
using art::Prop;

namespace {
const int D4X[4] = {0, 1, -1, 0}, D4Y[4] = {1, 0, 0, -1};
int ifl(float v) { return (int)std::floor(v); }
}  // namespace

// ------------------------------------------------------------------------------------------------ the plan's kind
void Gen::cityPick() {
  using A = cult::Archetype;
  CityParams& c = cp;
  c = CityParams();
  const uint32_t h = hash32(P.seed ^ 0xC17E5EEDu);
  bool anyWater = false;
  for (size_t i = 0; i < water.size() && !anyWater; i++) anyWater = water[i] != 0;
  switch (style) {
    case cult::Layout::Grid: c.kind = cultureIs((int)A::Jade) ? CK_AXIAL : CK_IMPERIAL; break;
    case cult::Layout::Compound: c.kind = CK_COURTYARD; break;
    case cult::Layout::Radial: c.kind = CK_RINGS; break;
    case cult::Layout::Linear: c.kind = anyWater ? (cultureIs((int)A::Fjordfolk) ? CK_HARBOUR : CK_RIVER) : CK_MARKET; break;
    case cult::Layout::Stilt: c.kind = CK_BOARDWALK; break;
    case cult::Layout::Terraced: c.kind = cultureIs((int)A::Jade) ? CK_AXIAL : CK_MARKET; break;
    default: c.kind = sylvan() ? CK_GROVE : (cultureIs((int)A::Jade) ? CK_AXIAL : CK_MARKET); break;
  }
  if (c.kind == CK_MARKET && anyWater && (h & 3u) != 0) c.kind = CK_RIVER;   // (a market city on a river bends to its banks)
  // the pitches by kind; the seed moves them a little (no two cities with one block)
  const int jP = (int)((h >> 4) % 3u) - 1, jS = (int)((h >> 8) % 5u) - 2;
  switch (c.kind) {
    case CK_IMPERIAL: c.P = 11; c.SX = 15 + jS / 2; c.nsWide = 2; c.aligned = true; break;
    case CK_AXIAL: c.P = 12; c.SX = 17 + jS / 2; c.nsWide = 0; c.aligned = true; c.symmetric = true; break;
    case CK_COURTYARD: c.P = 14 + (jP > 0 ? 1 : 0); c.SX = 22 + jS; c.nsWide = 0; break;
    case CK_RINGS: c.P = 12 + (jP > 0 ? 1 : 0); c.SX = 18 + jS; c.rings = 2; c.nsWide = 0; break;
    case CK_HARBOUR: c.P = 11 + (jP > 0 ? 1 : 0); c.SX = 17 + jS; c.nsWide = 3; break;
    case CK_RIVER: c.P = 12 + jP; c.SX = 19 + jS; c.nsWide = 3; break;
    case CK_BOARDWALK: c.P = 11 + (jP > 0 ? 1 : 0); c.SX = 16 + jS; c.nsWide = 0; break;
    case CK_GROVE: c.P = 13 + (jP > 0 ? 1 : 0); c.SX = 22 + jS; c.nsWide = 0; break;
    default: c.P = 12 + jP; c.SX = 18 + jS; c.nsWide = (h >> 12) % 3u == 0 ? 0 : 2 + (int)((h >> 14) % 2u); break;
  }
  // an old city is denser and more layered (smaller blocks), a young frontier city looser
  if (age >= 3) { c.P -= 1; c.SX -= 2; }
  else if (age == 0) { c.SX += 2; }
  if (terraced) c.P = std::max(c.P, 12);
  c.P = std::clamp(c.P, 10, 16);
  c.SX = std::clamp(c.SX, 13, 28);
  c.orchards = 1 + (int)((h >> 16) % 2u) + (age >= 2 ? 1 : 0);
  c.gardens = 1 + (int)((h >> 18) % 2u);
  c.crafts = (h >> 20) % 3u == 0 ? 0 : 1;
  if (c.kind == CK_GROVE) { c.orchards += 1; c.crafts = 0; }
  c.chain = (h >> 22) % 3u == 0 ? 1 : 0;
}

// ------------------------------------------------------------------------------------------------ the wall lane
// The lane round the inside of the wall, four tiles in from the ring (Chebyshev: parallel to its straight runs and its
// 45-degree runs), 4-connected (a slanted run's steps filled on the inner side), one tile wide, paved. Stops at the
// compound, the precincts and the park (paintStreet keeps off them); the gates' throats and avenues cross it.
void Gen::cityWallLane() {
  if (ringNear.empty() || !walled) return;
  const size_t n = (size_t)W * H;
  std::vector<uint8_t> on(n, 0);
  for (int y = 1; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++) if (inside[I(x, y)] && ringNear[I(x, y)] == 4) on[I(x, y)] = 1;
  // a diagonal step (two tiles corner to corner, neither side between them on) gets the tile on the inner side
  std::vector<std::pair<int, int>> add;
  for (int y = 1; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++) {
      if (!on[I(x, y)]) continue;
      for (int sx : {-1, 1})
        for (int sy : {-1, 1}) {
          if (!in(x + sx, y + sy) || !on[I(x + sx, y + sy)] || on[I(x + sx, y)] || on[I(x, y + sy)]) continue;
          const int ax = x + sx, ay = y, bx = x, by = y + sy;
          const bool aIn = inside[I(ax, ay)] && ringNear[I(ax, ay)] >= 4, bIn = inside[I(bx, by)] && ringNear[I(bx, by)] >= 4;
          if (aIn && (!bIn || ringNear[I(ax, ay)] >= ringNear[I(bx, by)])) add.push_back({ax, ay});
          else if (bIn) add.push_back({bx, by});
        }
    }
  for (const auto& a : add) on[I(a.first, a.second)] = 1;
  if (cityMade.size() != n) cityMade.assign(n, 0);
  const Ground g = laneG == Ground::Dirt ? Ground::Road : laneG;
  for (int y = 1; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++) {
      if (!on[I(x, y)] || water[I(x, y)] || !bandFree(x, y)) continue;
      const uint8_t was = mask[I(x, y)];
      if (was == K_SQUARE) continue;
      paintStreet(x, y, K_LANE, g);
      if (get(x, y) == K_LANE && was == K_NONE) {
        plan[I(x, y)] = (uint8_t)Urb::Lane;
        cityMade[I(x, y)] = 1;
      }
    }
  // a piece of the lane the compound or a precinct cut short of any other street goes (no stub along the wall)
  std::vector<uint8_t> seen(n, 0);
  for (int y = 1; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++) {
      const size_t i0 = I(x, y);
      if (seen[i0] || !cityMade[i0] || cityMade[i0] != 1) continue;
      std::vector<int> comp{(int)i0};
      seen[i0] = 1;
      bool meets = false;
      for (size_t k = 0; k < comp.size(); k++) {
        const int qx = comp[k] % W, qy = comp[k] / W;
        for (int d = 0; d < 4; d++) {
          const int nx = qx + D4X[d], ny = qy + D4Y[d];
          if (!in(nx, ny)) continue;
          const size_t j = I(nx, ny);
          if (cityMade[j] == 1) { if (!seen[j]) { seen[j] = 1; comp.push_back((int)j); } }
          else if (isStreet(nx, ny)) meets = true;
        }
      }
      if (meets || comp.size() >= 60) continue;
      for (int c : comp) {
        set(c % W, c / W, K_NONE);
        M.setG(c % W, c / W, base);
        plan[(size_t)c] = 0;
        cityMade[(size_t)c] = 0;
      }
    }
}

// ------------------------------------------------------------------------------------------------ the rings
// The rings plan: octagonal ring streets round the heart (three wide with their walks), axis-aligned sides and 45-degree
// chamfers, at a third and three fifths of the way out (a capital's court inside the first), each kept inside the wall
// lane; the avenues are the spokes, the lattice fills the bands between the rings
void Gen::cityRingStreets() {
  if (cp.rings <= 0) return;
  static const float fr[3] = {0.36f, 0.62f, 0.80f};
  const uint32_t h = hash32(P.seed ^ 0x0C7A6E5u);
  for (int k = 0; k < cp.rings && k < 3; k++) {
    float f = fr[k] + ((float)((h >> (k * 4)) % 5u) - 2.0f) * 0.015f;
    // shrink until the octagon stands inside the walls with room (the lane round the wall four tiles in)
    for (int tries = 0; tries < 8; tries++, f -= 0.03f) {
      const int a = (int)(rx * f), b = (int)(ry * f), ch = std::max(3, std::min(a, b) * 2 / 5);
      const int x0 = cx - a, x1 = cx + a, y0 = cy - b, y1 = cy + b;
      const std::vector<std::pair<int, int>> pts = {{x0 + ch, y0}, {x1 - ch, y0}, {x1, y0 + ch}, {x1, y1 - ch}, {x1 - ch, y1}, {x0 + ch, y1},
                                                    {x0, y1 - ch}, {x0, y0 + ch}, {x0 + ch, y0}};
      bool fits = true;
      for (size_t s = 0; s + 1 < pts.size() && fits; s++) {
        const int ax = pts[s].first, ay = pts[s].second, bx = pts[s + 1].first, by = pts[s + 1].second;
        const int len = std::max(std::abs(bx - ax), std::abs(by - ay));
        for (int t = 0; t <= len; t++) {
          const int x = ax + (bx - ax) * t / std::max(1, len), y = ay + (by - ay) * t / std::max(1, len);
          if (!in(x, y) || (walled && (!inside[I(x, y)] || nearRing(x, y, 6)))) { fits = false; break; }
        }
      }
      if (!fits) continue;
      paintBand(pts, 1.5f, (uint8_t)Urb::Street, K_LANE, Ground::Road);
      break;
    }
  }
}

// ------------------------------------------------------------------------------------------------ the spine
// The harbour and river plans: the avenue along the water (pickSpine: east-west or north-south, the bank a few tiles
// off one side), joined to the heart straight across; the lattice lies parallel to it
void Gen::citySpine() {
  pickSpine();
  const float ext = std::max(rx, ry) * 1.2f;
  const int ax = ifl(spineX + dcos(spineA) * ext), ay = ifl(spineY + dsin(spineA) * ext);
  const int bx = ifl(spineX - dcos(spineA) * ext), by = ifl(spineY - dsin(spineA) * ext);
  paintBand({{ax, ay}, {bx, by}}, avenueHW, (uint8_t)Urb::Avenue, K_MAIN, mainG);
  if (std::fabs(spineX - (cx + 0.5f)) + std::fabs(spineY - (cy + 0.5f)) > 2.0f)
    paintBand({{cx, cy}, {ifl(spineX), ifl(spineY)}}, avenueHW, (uint8_t)Urb::Avenue, K_MAIN, mainG);
}

// ------------------------------------------------------------------------------------------------ the banks
// Inside the walls every bank of a river, a lake or the sea has its street: a lane two tiles from the water (the tile
// between becomes the quay: quays()), following the bank as it bends, 4-connected; pieces under eight tiles are left out
void Gen::cityBankStreets() {
  const size_t n = (size_t)W * H;
  bool any = false;
  for (int y = 0; y < H && !any; y++)
    for (int x = 0; x < W; x++) if (water[I(x, y)] && (!walled || inside[I(x, y)])) { any = true; break; }
  if (!any) return;
  // Chebyshev distance to the water, capped at 3
  std::vector<uint8_t> d(n, 3);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (!water[I(x, y)]) continue;
      for (int oy = -2; oy <= 2; oy++)
        for (int ox = -2; ox <= 2; ox++) {
          if (!in(x + ox, y + oy)) continue;
          const uint8_t v = (uint8_t)std::max(std::abs(ox), std::abs(oy));
          if (v < d[I(x + ox, y + oy)]) d[I(x + ox, y + oy)] = v;
        }
    }
  auto okT = [&](int x, int y) {
    if (!in(x, y) || x < 2 || y < 2 || x >= W - 2 || y >= H - 2) return false;
    const size_t i = I(x, y);
    if (water[i] || (walled && (!inside[i] || nearRing(x, y, 3)))) return false;
    if (keep[i] || inCompound(x, y, 1) || groundSolid(M.at(x, y))) return false;
    const uint8_t k = mask[i];
    return k == K_NONE || k == K_LANE || k == K_MAIN;
  };
  std::vector<uint8_t> on(n, 0);
  for (int y = 2; y < H - 2; y++)
    for (int x = 2; x < W - 2; x++) if (d[I(x, y)] == 2 && okT(x, y)) on[I(x, y)] = 1;
  std::vector<std::pair<int, int>> add;
  for (int y = 2; y < H - 2; y++)
    for (int x = 2; x < W - 2; x++) {
      if (!on[I(x, y)]) continue;
      for (int sx : {-1, 1})
        for (int sy : {-1, 1}) {
          if (!on[I(x + sx, y + sy)] || on[I(x + sx, y)] || on[I(x, y + sy)]) continue;
          // the step's tile away from the water
          const bool aOk = okT(x + sx, y) && d[I(x + sx, y)] >= 2, bOk = okT(x, y + sy) && d[I(x, y + sy)] >= 2;
          if (aOk && (!bOk || d[I(x + sx, y)] >= d[I(x, y + sy)])) add.push_back({x + sx, y});
          else if (bOk) add.push_back({x, y + sy});
        }
    }
  for (const auto& a : add) on[I(a.first, a.second)] = 1;
  // the pieces: eight tiles or more
  if (cityMade.size() != n) cityMade.assign(n, 0);
  std::vector<uint8_t> seen(n, 0);
  const Ground g = laneG == Ground::Dirt ? Ground::Road : laneG;
  for (int y = 2; y < H - 2; y++)
    for (int x = 2; x < W - 2; x++) {
      if (!on[I(x, y)] || seen[I(x, y)]) continue;
      std::vector<int> comp{(int)I(x, y)};
      seen[I(x, y)] = 1;
      for (size_t k = 0; k < comp.size(); k++) {
        const int qx = comp[k] % W, qy = comp[k] / W;
        for (int dd = 0; dd < 4; dd++) {
          const int nx = qx + D4X[dd], ny = qy + D4Y[dd];
          if (in(nx, ny) && on[I(nx, ny)] && !seen[I(nx, ny)]) { seen[I(nx, ny)] = 1; comp.push_back((int)I(nx, ny)); }
        }
      }
      if (comp.size() < 8) continue;
      for (int c : comp) {
        const int qx = c % W, qy = c / W;
        const uint8_t was = mask[(size_t)c];
        if (was != K_NONE) continue;
        paintStreet(qx, qy, K_LANE, g);
        if (get(qx, qy) == K_LANE) { plan[(size_t)c] = (uint8_t)Urb::Lane; cityMade[(size_t)c] = 2; }
      }
    }
}

// ------------------------------------------------------------------------------------------------ the lattice
void Gen::cityGrid() {
  const size_t n = (size_t)W * H;
  if (cityMade.size() != n) cityMade.assign(n, 0);
  cityFeet.clear();
  // land the lattice may take: open ground inside the wall lane, dry (a marsh city's pools are boardwalked), clear of the
  // compound, the kept plots, the squares and everything laid
  auto open = [&](int x, int y) {
    if (!in(x, y) || x < 2 || y < 2 || x >= W - 2 || y >= H - 2) return false;
    const size_t i = I(x, y);
    if (mask[i] != K_NONE || water[i] || keep[i] || inCompound(x, y, 1) || noBuild[i]) return false;
    if (walled && (!inside[i] || nearRing(x, y, 3))) return false;
    const Ground g = M.at(x, y);
    if (g == Ground::Bridge) return false;
    if (groundSolid(g) && !(stilt && (g == Ground::Water || g == Ground::Swamp))) return false;
    return true;
  };
  // --- the sections: the open land's 4-connected pieces
  csec.assign(n, -1);
  csecs.clear();
  {
    std::vector<int> st;
    for (int y = 2; y < H - 2; y++)
      for (int x = 2; x < W - 2; x++) {
        if (csec[I(x, y)] >= 0 || !open(x, y)) continue;
        const int id = (int)csecs.size();
        CitySec S;
        S.x0 = S.x1 = x; S.y0 = S.y1 = y;
        st.clear();
        st.push_back((int)I(x, y));
        csec[I(x, y)] = (int16_t)id;
        while (!st.empty()) {
          const int c = st.back();
          st.pop_back();
          const int qx = c % W, qy = c / W;
          S.area++;
          S.x0 = std::min(S.x0, qx); S.x1 = std::max(S.x1, qx); S.y0 = std::min(S.y0, qy); S.y1 = std::max(S.y1, qy);
          for (int d = 0; d < 4; d++) {
            const int nx = qx + D4X[d], ny = qy + D4Y[d];
            if (!in(nx, ny) || csec[I(nx, ny)] >= 0 || !open(nx, ny)) continue;
            csec[I(nx, ny)] = (int16_t)id;
            st.push_back((int)I(nx, ny));
          }
        }
        csecs.push_back(S);
        if (csecs.size() >= 30000) break;
      }
  }
  // each section's pitches: the plan's, the quarter leaning them (the rich quarter's deep gardens, the poor quarter's
  // crowded little blocks), the seed moving them a little (the strict plans keep one lattice)
  for (size_t s = 0; s < csecs.size(); s++) {
    CitySec& S = csecs[s];
    const District q = districtAt((S.x0 + S.x1) / 2, (S.y0 + S.y1) / 2);
    S.P = cp.P;
    S.SX = cp.SX;
    if (!cp.aligned) {
      if (q == District::Noble) { S.P += 2; S.SX += 3; }
      else if (q == District::Poor) { S.P -= 1; S.SX -= 2; }
      const uint32_t hs = hashAt(S.x0, S.y0, 0x5EC7u);
      S.SX += (int)(hs % 5u) - 2;
    }
    S.P = std::clamp(S.P, 10, 17);
    S.SX = std::clamp(S.SX, 12, 30);
  }
  // per street tile: its horizontal and vertical runs (how far the street goes on either way through it)
  std::vector<uint8_t> hrun(n, 0), vrun(n, 0);
  auto runs = [&]() {
    std::fill(hrun.begin(), hrun.end(), 0);
    std::fill(vrun.begin(), vrun.end(), 0);
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W;) {
        if (!isStreet(x, y)) { x++; continue; }
        int e = x;
        while (e < W && isStreet(e, y)) e++;
        const uint8_t v = (uint8_t)std::min(255, e - x);
        for (int k = x; k < e; k++) hrun[I(k, y)] = v;
        x = e;
      }
    for (int x = 0; x < W; x++)
      for (int y = 0; y < H;) {
        if (!isStreet(x, y)) { y++; continue; }
        int e = y;
        while (e < H && isStreet(x, e)) e++;
        const uint8_t v = (uint8_t)std::min(255, e - y);
        for (int k = y; k < e; k++) vrun[I(x, k)] = v;
        y = e;
      }
  };
  runs();
  // a horizontal / vertical street tile (a run of 4 or more that way through it)
  auto hz = [&](int x, int y) { return in(x, y) && isStreet(x, y) && hrun[I(x, y)] >= 4; };
  auto vt = [&](int x, int y) { return in(x, y) && isStreet(x, y) && vrun[I(x, y)] >= 4; };
  const Ground streetG = Ground::Road;
  const Ground lane = laneG == Ground::Dirt ? Ground::Road : laneG;
  auto mark = [&](int x0, int y0, int x1, int y1, uint8_t tag) {   // the tiles a band just made
    for (int y = std::min(y0, y1) - 1; y <= std::max(y0, y1) + 1; y++)
      for (int x = std::min(x0, x1) - 1; x <= std::max(x0, x1) + 1; x++)
        if (in(x, y) && isStreet(x, y) && !cityMade[I(x, y)] && plan[I(x, y)]) {
          const Urb u = (Urb)plan[I(x, y)];
          if (u == Urb::Avenue || u == Urb::Gutter) continue;
          cityMade[I(x, y)] = tag;
        }
  };
  // --- the anchors: per section, the way edges it borders that run straight east-west (a run of 12 or more), the
  // longest first: its rows run parallel, a block apart from it
  struct Edge { int y, dir, len; };
  auto hAnchor = [&](int s, Edge& e) {
    const CitySec& S = csecs[(size_t)s];
    int bestLen = 0;
    for (int y = std::max(1, S.y0 - 1); y <= std::min(H - 2, S.y1 + 1); y++)
      for (int dir : {-1, 1}) {
        int len = 0;
        for (int x = S.x0; x <= S.x1; x++)
          if (isStreet(x, y) && hrun[I(x, y)] >= 12 && csec[I(x, y + dir)] == s) len++;
        // (an avenue's drain row is not its edge: the edge is the band's last row)
        if (len > bestLen) { bestLen = len; e = Edge{y, dir, len}; }
      }
    return bestLen >= 8;
  };
  auto vAnchor = [&](int s, Edge& e) {
    const CitySec& S = csecs[(size_t)s];
    int bestLen = 0;
    for (int x = std::max(1, S.x0 - 1); x <= std::min(W - 2, S.x1 + 1); x++)
      for (int dir : {-1, 1}) {
        int len = 0;
        for (int y = S.y0; y <= S.y1; y++)
          if (isStreet(x, y) && vrun[I(x, y)] >= 12 && csec[I(x + dir, y)] == s) len++;
        if (len > bestLen) { bestLen = len; e = Edge{x, dir, len}; }
      }
    return bestLen >= 6;
  };
  // the strict plans' one lattice: the rows on the heart's square's edges, the columns on the heart's column
  Edge gH{cy, 1, 0}, gV{cx, 1, 0};
  bool haveGH = false;
  {
    // the heart's own east-west way (the avenue through it, else the square's south edge)
    for (int dy = 0; dy <= 12 && !haveGH; dy++)
      for (int sgn : {1, -1}) {
        const int y = cy + sgn * dy;
        if (in(cx, y) && isStreet(cx, y) && !isStreet(cx, y + 1) && hrun[I(cx, y)] >= 12) { gH = Edge{y, 1, 0}; haveGH = true; break; }
      }
  }
  // --- the rows: three-wide block streets (a carriage lane between two walks)
  const bool dbg = std::getenv("EMB_DEBUG_CITY") != nullptr;
  if (dbg) std::printf("citygrid: city %08x kind %d P %d SX %d sections %d\n", (unsigned)P.seed, (int)cp.kind, cp.P, cp.SX, (int)csecs.size());
  const int MINB = 5;   // the depth a block keeps between a block street and any other street parallel to it
  for (size_t s = 0; s < csecs.size(); s++) {
    const CitySec& S = csecs[s];
    if (S.area < 60 || S.y1 - S.y0 < 8) continue;
    const int Pp = S.P, In = Pp - 3;
    Edge e{cy, 1, 0};
    const bool own = !cp.aligned && hAnchor((int)s, e);
    int c0;
    if (cp.aligned) c0 = haveGH ? gH.y + In + 2 : cy + (int)(hashAt(1, 1, 0x5EC8u) % (uint32_t)Pp);
    else if (own) c0 = e.dir < 0 ? e.y - In - 2 : e.y + In + 2;
    else c0 = cy + (int)(hashAt(S.x0, S.y1, 0x5EC9u) % (uint32_t)Pp);
    // every row of the lattice crossing the section
    int k0 = (S.y0 - 1 - c0) / Pp - 1, k1 = (S.y1 + 1 - c0) / Pp + 1;
    if (dbg) std::printf("citygrid: sec %d area %d box %d,%d..%d,%d P %d anchor %d (y %d dir %d len %d) c0 %d\n", (int)s, S.area, S.x0, S.y0, S.x1, S.y1, Pp, own ? 1 : 0, e.y, e.dir, e.len, c0);
    for (int k = k0; k <= k1; k++) {
      const int c = c0 + k * Pp;
      if (c - 1 < S.y0 - 1 || c + 1 > S.y1 + 1 || c < 3 || c >= H - 3) continue;
      // the states along the row: 0 hard, 1 a sliver (too near a parallel street), 2 open, 3 a street met
      std::vector<uint8_t> stt((size_t)W, 0);
      for (int x = std::max(1, S.x0 - 1); x <= std::min(W - 2, S.x1 + 1); x++) {
        if (isStreet(x, c)) { stt[(size_t)x] = 3; continue; }
        // (a slanted or bending street met: its tile on the band's edge row, not one running alongside)
        if ((isStreet(x, c - 1) && hrun[I(x, c - 1)] < 4) || (isStreet(x, c + 1) && hrun[I(x, c + 1)] < 4)) { stt[(size_t)x] = 3; continue; }
        bool ok = true;
        for (int dy = -1; dy <= 1 && ok; dy++)
          if (!open(x, c + dy) || csec[I(x, c + dy)] != (int16_t)s) ok = false;
        if (ok && terraced && (lvl[I(x, c - 1)] != lvl[I(x, c)] || lvl[I(x, c + 1)] != lvl[I(x, c)])) ok = false;
        if (!ok) continue;
        bool sliver = false;
        for (int k2 = 1; k2 <= MINB && !sliver; k2++)
          if (hz(x, c - 1 - k2) || hz(x, c + 1 + k2)) sliver = true;
        stt[(size_t)x] = sliver ? 1 : 2;
      }
      // the segments from a street to a street
      int a = -1;
      for (int x = std::max(1, S.x0 - 1); x <= std::min(W - 2, S.x1 + 1); x++) {
        if (stt[(size_t)x] == 3) {
          if (a >= 0 && x - a >= 5) {
            int good = 0, run = 0, worst = 0;
            for (int t = a; t < x; t++) {
              if (stt[(size_t)t] == 2) { good++; run = 0; }
              else { run++; worst = std::max(worst, run); }
            }
            if (dbg) std::printf("citygrid:   row %d seg %d..%d good %d worst %d\n", c, a, x - 1, good, worst);
            if (good * 10 >= (x - a) * 7 && worst <= 6) {
              paintBand({{a - 1, c}, {x, c}}, 1.5f, (uint8_t)Urb::Street, K_LANE, streetG);
              mark(a - 1, c, x, c, 3);
              for (int t = a - 1; t <= x; t++)
                for (int dy = -1; dy <= 1; dy++) if (in(t, c + dy) && isStreet(t, c + dy)) hrun[I(t, c + dy)] = 20;   // (the next rows keep off it)
            }
          }
          a = x + 1;
          continue;
        }
        if (stt[(size_t)x] == 0) { a = -1; continue; }
        if (a < 0) {
          // a segment starts only off a street
          if (x > 0 && stt[(size_t)(x - 1)] == 3) a = x;
        }
      }
    }
  }
  runs();
  // --- the columns: north-south lanes (some three-wide streets) between the rows, a block apart
  for (size_t s = 0; s < csecs.size(); s++) {
    const CitySec& S = csecs[s];
    if (S.area < 40) continue;
    const int Sx = S.SX;
    Edge e{cx, 1, 0};
    const bool own = !cp.aligned && vAnchor((int)s, e);
    int c0;
    if (cp.aligned) c0 = cx + (cp.symmetric ? Sx / 2 : (int)(hashAt(2, 2, 0x5ECAu) % (uint32_t)Sx));
    else if (own) c0 = e.dir < 0 ? e.y - Sx + 1 : e.y + Sx - 1;
    else c0 = cx + (int)(hashAt(S.x1, S.y0, 0x5ECBu) % (uint32_t)Sx);
    int k0 = (S.x0 - 1 - c0) / Sx - 1, k1 = (S.x1 + 1 - c0) / Sx + 1;
    for (int k = k0; k <= k1; k++) {
      int x = c0 + k * Sx;
      if (cp.symmetric) {
        // mirrored: the columns stand at the same distance either side of the axis
        const int off = std::abs(x - cx);
        x = x < cx ? cx - off : cx + off;
      }
      if (x < S.x0 - 1 || x > S.x1 + 1 || x < 3 || x >= W - 3) continue;
      const bool wide = cp.nsWide > 0 && ((k % cp.nsWide) + cp.nsWide) % cp.nsWide == 0;
      const int half = wide ? 1 : 0;
      // the column may shift a tile or two where a vertical street stands too near
      for (int y = std::max(1, S.y0 - 1); y <= std::min(H - 2, S.y1 + 1);) {
        if (!isStreet(x, y) || isStreet(x, y + 1)) { y++; continue; }
        // a segment from the street at y down to the next street below
        int e2 = y + 1;
        while (e2 < H - 1 && !isStreet(x, e2) && e2 - y < 40) e2++;
        const int len = e2 - y - 1;
        if (len < 3 || !isStreet(x, e2)) { y = e2; continue; }
        int best = -99;
        for (int sh : {0, 1, -1, 2, -2}) {
          const int qx = x + sh;
          if (!isStreet(qx, y) || !isStreet(qx, e2)) continue;
          bool ok = true;
          for (int t = y + 1; t < e2 && ok; t++) {
            for (int w = -half; w <= half && ok; w++)
              if (!open(qx + w, t) || csec[I(qx + w, t)] != (int16_t)s) ok = false;
            // never alongside another north-south street (three tiles clear either side)
            for (int k2 = half + 1; k2 <= half + 3 && ok; k2++)
              if (vt(qx - k2, t) || vt(qx + k2, t)) ok = false;
          }
          if (ok) { best = sh; break; }
        }
        if (best != -99) {
          const int qx = x + best;
          if (wide) paintBand({{qx, y}, {qx, e2}}, 1.5f, (uint8_t)Urb::Street, K_LANE, streetG);
          else paintBand({{qx, y}, {qx, e2}}, 0.5f, (uint8_t)Urb::Lane, K_LANE, lane);
          mark(qx, y, qx, e2, 4);
          if (!wide) cityFeet.push_back({qx, e2 - 1});   // (the land row just above the street the lane comes down to)
          for (int t = y; t <= e2; t++) vrun[I(qx, t)] = 20;   // (the next columns keep off it)
        }
        y = e2;
      }
    }
  }
  cityInfill();
  cityRoleBlocks();
}

// ------------------------------------------------------------------------------------------------ the infill
// A block the lattice left too big (a row skipped for a sliver at one end, a column that found no clear line, a section
// of an odd shape round the market or a precinct) is cut by one more way from a street to a street: a block street
// across it (three wide, east-west) where it is deep enough for two blocks of houses, else a lane down it (north-south)
// where it is wide enough. Repeated a few rounds, the biggest blocks first; never a stub, never alongside a street.
void Gen::cityInfill() {
  const size_t n = (size_t)W * H;
  auto open = [&](int x, int y) {
    if (!in(x, y) || x < 2 || y < 2 || x >= W - 2 || y >= H - 2) return false;
    const size_t i = I(x, y);
    if (mask[i] != K_NONE || water[i] || keep[i] || inCompound(x, y, 1) || noBuild[i]) return false;
    if (walled && (!inside[i] || nearRing(x, y, 3))) return false;
    if (mktZone.w > 0 && mktZone.contains(x, y)) return false;
    const Ground g = M.at(x, y);
    if (g == Ground::Bridge) return false;
    if (groundSolid(g) && !(stilt && (g == Ground::Water || g == Ground::Swamp))) return false;
    return true;
  };
  const int MINB = 5;
  const Ground streetG = Ground::Road;
  const Ground lane = laneG == Ground::Dirt ? Ground::Road : laneG;
  std::vector<int> lab(n, -1), st;
  struct Blk { int x0, y0, x1, y1, area; };
  for (int round = 0; round < 5; round++) {
    std::fill(lab.begin(), lab.end(), -1);
    std::vector<Blk> blks;
    for (int y = 2; y < H - 2; y++)
      for (int x = 2; x < W - 2; x++) {
        if (lab[I(x, y)] >= 0 || !open(x, y)) continue;
        const int id = (int)blks.size();
        Blk B{x, y, x, y, 0};
        st.assign(1, (int)I(x, y));
        lab[I(x, y)] = id;
        while (!st.empty()) {
          const int c = st.back();
          st.pop_back();
          const int qx = c % W, qy = c / W;
          B.area++;
          B.x0 = std::min(B.x0, qx); B.x1 = std::max(B.x1, qx); B.y0 = std::min(B.y0, qy); B.y1 = std::max(B.y1, qy);
          for (int d = 0; d < 4; d++) {
            const int nx = qx + D4X[d], ny = qy + D4Y[d];
            if (!in(nx, ny) || lab[I(nx, ny)] >= 0 || !open(nx, ny)) continue;
            lab[I(nx, ny)] = id;
            st.push_back((int)I(nx, ny));
          }
        }
        blks.push_back(B);
      }
    // the big blocks, the biggest first (the order the plan's own: by area, then place)
    std::vector<int> order;
    const int bigA = cp.P * cp.SX * 3 / 2;
    for (int i = 0; i < (int)blks.size(); i++) if (blks[(size_t)i].area >= bigA) order.push_back(i);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return blks[(size_t)a].area > blks[(size_t)b].area; });
    auto inB = [&](int x, int y, int id) { return in(x, y) && lab[I(x, y)] == id; };
    auto streetNear = [&](int x, int y) { return in(x, y) && isStreet(x, y); };
    int cuts = 0;
    for (int bi : order) {
      const Blk& B = blks[(size_t)bi];
      bool done = false;
      // --- a block street across: the row nearest the block's middle with MINB rows of the block either side over most
      // of its run, meeting a street at both ends
      if (B.y1 - B.y0 + 1 >= 2 * MINB + 5) {
        const int mid = (B.y0 + B.y1) / 2;
        for (int off = 0; off <= (B.y1 - B.y0) / 2 && !done; off++)
          for (int sgn : {1, -1}) {
            if (done || (off == 0 && sgn < 0)) continue;
            const int c = mid + sgn * off;
            if (c - 1 - MINB < B.y0 - 1 || c + 1 + MINB > B.y1 + 1) continue;
            // the longest run of the row with its three rows in the block
            int bestA = -1, bestB = -1;
            for (int x = B.x0; x <= B.x1;) {
              if (!(inB(x, c - 1, bi) && inB(x, c, bi) && inB(x, c + 1, bi))) { x++; continue; }
              int e = x;
              while (e <= B.x1 && inB(e, c - 1, bi) && inB(e, c, bi) && inB(e, c + 1, bi)) e++;
              if (e - x > bestB - bestA) { bestA = x; bestB = e; }
              x = e;
            }
            if (bestA < 0 || bestB - bestA < 7) continue;
            // both ends on a street (the row, or the row beside it where a slanted street is met)
            auto endOk = [&](int x) { return streetNear(x, c) || streetNear(x, c - 1) || streetNear(x, c + 1); };
            if (!endOk(bestA - 1) || !endOk(bestB)) continue;
            // the depth either side: MINB rows of the block (or of land the lattice keeps) over 80% of the run
            int deep = 0;
            for (int x = bestA; x < bestB; x++) {
              bool up = true, dn = true;
              for (int k = 2; k <= MINB + 1; k++) {
                if (!inB(x, c - k, bi)) up = false;
                if (!inB(x, c + k, bi)) dn = false;
              }
              if (up && dn) deep++;
            }
            if (deep * 10 < (bestB - bestA) * 8) continue;
            if (terraced) {
              bool flat = true;
              for (int x = bestA; x < bestB && flat; x++)
                if (lvl[I(x, c - 1)] != lvl[I(x, c)] || lvl[I(x, c + 1)] != lvl[I(x, c)]) flat = false;
              if (!flat) continue;
            }
            paintBand({{bestA - 1, c}, {bestB, c}}, 1.5f, (uint8_t)Urb::Street, K_LANE, streetG);
            for (int y = c - 2; y <= c + 2; y++)
              for (int x = bestA - 2; x <= bestB + 1; x++)
                if (in(x, y) && isStreet(x, y) && !cityMade[I(x, y)] && plan[I(x, y)]) {
                  const Urb u = (Urb)plan[I(x, y)];
                  if (u != Urb::Avenue && u != Urb::Gutter) cityMade[I(x, y)] = 5;
                }
            done = true;
          }
      }
      // --- a lane down it: the column nearest the middle with four columns of the block either side (and no
      // north-south street within three), meeting a street at both ends
      if (!done && B.x1 - B.x0 + 1 >= std::max(15, cp.SX * 3 / 2)) {
        const int mid = (B.x0 + B.x1) / 2;
        for (int off = 0; off <= (B.x1 - B.x0) / 2 - 5 && !done; off++)
          for (int sgn : {1, -1}) {
            if (done || (off == 0 && sgn < 0)) continue;
            const int x = mid + sgn * off;
            int bestA = -1, bestB = -1;
            for (int y = B.y0; y <= B.y1;) {
              if (!inB(x, y, bi)) { y++; continue; }
              int e = y;
              while (e <= B.y1 && inB(x, e, bi)) e++;
              if (e - y > bestB - bestA) { bestA = y; bestB = e; }
              y = e;
            }
            if (bestA < 0 || bestB - bestA < 5) continue;
            if (!streetNear(x, bestA - 1) || !streetNear(x, bestB)) continue;
            int wide = 0;
            for (int y = bestA; y < bestB; y++) {
              bool l = true, r = true;
              for (int k = 1; k <= 5; k++) {
                if (!inB(x - k, y, bi)) l = false;
                if (!inB(x + k, y, bi)) r = false;
              }
              if (l && r) wide++;
            }
            if (wide * 10 < (bestB - bestA) * 8) continue;
            paintBand({{x, bestA - 1}, {x, bestB}}, 0.5f, (uint8_t)Urb::Lane, K_LANE, lane);
            for (int y = bestA - 1; y <= bestB; y++)
              if (in(x, y) && isStreet(x, y) && !cityMade[I(x, y)] && plan[I(x, y)]) {
                const Urb u = (Urb)plan[I(x, y)];
                if (u != Urb::Avenue && u != Urb::Gutter) cityMade[I(x, y)] = 5;
              }
            cityFeet.push_back({x, bestB - 1});
            done = true;
          }
      }
      if (done) cuts++;
      // (one cut per block a round: the block's labels are stale after it; the next round re-labels)
    }
    if (std::getenv("EMB_DEBUG_CITY")) std::printf("citygrid: infill %08x round %d: %zu big blocks, %d cut\n", (unsigned)P.seed, round, order.size(), cuts);
    if (!cuts) break;
  }
}

// ------------------------------------------------------------------------------------------------ the role blocks
// Whole blocks kept for a role before the homes (their land K_FIELD: no house, service or footpath takes it): orchards
// (fruit trees in rows on the grass) and kitchen gardens (beds of crops between earth paths, a fence along the top)
// toward the wall, a craft yard (trodden earth, the trades' clutter) in the craft quarter; only where the city has
// frontage to spare for its homes (a block street's north side, about four tiles a house)
void Gen::cityRoleBlocks() {
  const size_t n = (size_t)W * H;
  // the blocks: open land cut off by ways, inside the walls
  std::vector<int> lab(n, -1);
  struct Blk { int x0, y0, x1, y1, area, front; uint32_t key; District q; float d; };
  std::vector<Blk> blks;
  std::vector<int> st;
  for (int y = 2; y < H - 2; y++)
    for (int x = 2; x < W - 2; x++) {
      const size_t i0 = I(x, y);
      if (lab[i0] >= 0 || mask[i0] != K_NONE || water[i0] || keep[i0] || (walled && (!inside[i0] || nearRing(x, y, 3)))) continue;
      Blk B{x, y, x, y, 0, 0, 0, District::Centre, 0};
      const int id = (int)blks.size();
      bool edge = false;
      st.clear();
      st.push_back((int)i0);
      lab[i0] = id;
      while (!st.empty()) {
        const int c = st.back();
        st.pop_back();
        const int qx = c % W, qy = c / W;
        B.area++;
        B.x0 = std::min(B.x0, qx); B.x1 = std::max(B.x1, qx); B.y0 = std::min(B.y0, qy); B.y1 = std::max(B.y1, qy);
        if (isStreet(qx, qy + 1) && !water[I(qx, qy + 1)] && M.at(qx, qy + 1) != Ground::Bridge) B.front++;   // (a front: any street south)
        for (int d = 0; d < 4; d++) {
          const int nx = qx + D4X[d], ny = qy + D4Y[d];
          if (!in(nx, ny)) { edge = true; continue; }
          const size_t j = I(nx, ny);
          if (lab[j] >= 0 || mask[j] != K_NONE || water[j] || keep[j]) continue;
          if (walled && !inside[j]) { edge = true; continue; }
          if (walled && nearRing(nx, ny, 3)) continue;   // (the wall's foot outside the wall lane: no block's land)
          lab[j] = id;
          st.push_back((int)j);
        }
      }
      B.key = hashAt(B.x0, B.y0, 0xB10Cu);
      B.q = districtAt((B.x0 + B.x1) / 2, (B.y0 + B.y1) / 2);
      B.d = dist((B.x0 + B.x1) / 2, (B.y0 + B.y1) / 2);
      if (edge) B.area = -B.area;
      blks.push_back(B);
    }
  // the homes' frontage: a block street's north side holds a house about every 4.5 tiles (the rich quarter's wide houses
  // 6, the poor quarter's cottages 3.4); the city wants its homes and services (homesMin + 70) with a margin, and every
  // block it can spare beyond that becomes a role block (the outermost first: a medieval city's gardens and orchards
  // lay toward its wall), so no block stands as leftover ground
  auto slotsOf = [&](const Blk& b) {
    const float w = b.q == District::Noble ? 6.0f : (b.q == District::Poor ? 3.4f : 4.5f);
    return (float)b.front / w;
  };
  float slots = 0;
  for (const Blk& b : blks) slots += slotsOf(b);   // (every front, a block on the wall's foot too)
  const float need = (float)(townScale(P.type).homesMin + 70) * 1.12f;
  std::vector<int> order;
  for (int i = 0; i < (int)blks.size(); i++) {
    const Blk& b = blks[(size_t)i];
    const int bw = b.x1 - b.x0 + 1, bh = b.y1 - b.y0 + 1;
    if (b.area < 50 || b.area > 520 || bw < 7 || bh < 5) continue;
    if (b.area * 10 < bw * bh * 6) continue;   // (whole rectangular-ish blocks only)
    if (b.d < 0.42f || b.d > 0.92f || b.q == District::Centre) continue;
    order.push_back(i);
  }
  // the outermost first, the seed's hash breaking near-ties (no two cities with their gardens in one place)
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
    const float ka = blks[(size_t)a].d + (float)(blks[(size_t)a].key % 1000u) * 0.00012f, kb = blks[(size_t)b].d + (float)(blks[(size_t)b].key % 1000u) * 0.00012f;
    return ka > kb;
  });
  const int maxRole = 4 + cp.orchards + cp.gardens + cp.crafts;
  int orch = 0, gard = 0, craft = 0;
  std::vector<std::pair<int, int>> taken;   // (centres: role blocks of one kind keep apart)
  std::vector<int> takenRole;
  for (int bi : order) {
    const Blk& b = blks[(size_t)bi];
    if (orch + gard + craft >= maxRole) break;
    if (slots - slotsOf(b) < need) continue;
    const int mx = (b.x0 + b.x1) / 2, my = (b.y0 + b.y1) / 2;
    // the role by the quarter: the rich and the temple quarters' orchards, the poor and the craft quarters' kitchen
    // gardens, a craft yard in the craft quarter
    int role = 0;   // 1 orchard, 2 kitchen garden, 3 craft yard
    if (b.q == District::Crafts) role = craft < cp.crafts ? 3 : 2;
    else if (b.q == District::Noble || b.q == District::Temple) role = (b.key >> 8) % 4u == 0 ? 2 : 1;
    else role = (b.key >> 8) % 3u == 0 ? 1 : 2;
    bool far = true;   // (two of a kind a block or more apart: no patch of five orchards in a row)
    for (size_t t = 0; t < taken.size(); t++)
      if (takenRole[t] == role && std::abs(taken[t].first - mx) + std::abs(taken[t].second - my) < 22) far = false;
    if (!far) role = role == 1 ? 2 : 1;
    if (!role) continue;
    if (role == 3 && (bio == Biome::Snow)) continue;
    const bool winter = bio == Biome::Snow;
    // the block's land (its own label) becomes the role's
    for (int y = b.y0; y <= b.y1; y++)
      for (int x = b.x0; x <= b.x1; x++) {
        if (lab[I(x, y)] != bi) continue;
        set(x, y, K_FIELD);
        M.setP(x, y, 0);
        if (role == 1) M.setG(x, y, winter ? Ground::Snow : (base == Ground::Grass ? Ground::Meadow : base));
        else if (role == 2) M.setG(x, y, (y - b.y0) % 2 == 1 ? Ground::Farmland : Ground::Dirt);
        else M.setG(x, y, Ground::Dirt);
      }
    // the planting and the clutter, a tile in from the block's edge (its walks and lanes stay clear)
    auto inner = [&](int x, int y) {
      if (!in(x, y) || lab[I(x, y)] != bi) return false;
      for (int d = 0; d < 4; d++) if (lab[I(x + D4X[d], y + D4Y[d])] != bi) return false;
      return true;
    };
    const Prop fruit = winter ? townTree(bio, eco, b.key) : Prop::FruitTree;
    for (int y = b.y0; y <= b.y1; y++)
      for (int x = b.x0; x <= b.x1; x++) {
        if (lab[I(x, y)] != bi) continue;
        const bool rim = !inner(x, y);
        if (role == 1) {
          // (rows of trees three apart, staggered every other row: an orchard, not a grid of posts)
          const int ry2 = y - b.y0 - 1, rxx = x - b.x0 - 1 - ((ry2 / 3) % 2) * 1;
          if (!rim && ry2 % 3 == 0 && rxx % 3 == 0 && !(isStreet(x, y + 1))) M.setProp(x, y, fruit);
          else if (rim && y == b.y0 && (x - b.x0) % 2 == 0 && !winter) M.setProp(x, y, Prop::Hedge);
        } else if (role == 2) {
          if (y == b.y0 && !rim) M.setProp(x, y, Prop::FenceH);
          else if (rim && y == b.y0) M.setProp(x, y, Prop::FenceH);
        } else {
          const uint32_t hh = hashAt(x, y, 0xC4AFu);
          if (!rim && hh % 9u == 0) M.setProp(x, y, hh % 2u ? Prop::Crate : Prop::Woodpile);
          else if (!rim && hh % 23u == 1) M.setProp(x, y, Prop::Barrel);
        }
      }
    if (role == 2) {
      if (inner(b.x1 - 1, b.y1 - 1)) M.setProp(b.x1 - 1, b.y1 - 1, (b.key >> 4) % 2u ? Prop::Woodpile : Prop::Barrel);
      if (inner(b.x0 + 1, b.y1 - 1) && (b.key >> 5) % 2u) M.setProp(b.x0 + 1, b.y1 - 1, Prop::Haystack);
    }
    const BlockRole br = role == 1 ? BlockRole::Orchard : (role == 2 ? BlockRole::KitchenGarden : BlockRole::CraftYard);
    for (int y = b.y0; y <= b.y1; y++)
      for (int x = b.x0; x <= b.x1; x++) if (lab[I(x, y)] == bi) setRole(x, y, br);
    taken.push_back({mx, my});
    takenRole.push_back(role);
    slots -= slotsOf(b);
    if (role == 1) orch++;
    else if (role == 2) gard++;
    else craft++;
  }
  if (std::getenv("EMB_DEBUG_CITY"))
    std::printf("citygrid: role blocks %08x: orchards %d gardens %d crafts %d, house slots left %.0f (need %.0f)\n", (unsigned)P.seed, orch, gard, craft, slots, need);
}

// ------------------------------------------------------------------------------------------------ the river links
// (M7c fixer) a river through a walled city cut its far bank off: the bank's block streets ended at the water with
// no bridge (seed 65's dune river capital: the west bank had streets but no way to them, so it got fields and almost no
// houses; structure 67). Every piece of the street network inside the walls that no street joins to the main piece
// gets a bridge: the shortest straight crossing (a tile or two of bank, up to ten of water, a tile or two of bank) from
// one of its streets to a street of the main piece, as wide as the street it carries (one or three tiles), the
// shortest nearest the heart (the first in scan order on a tie); no Rng draw.
void Gen::cityRiverLinks() {
  const size_t n = (size_t)W * H;
  auto streetIn = [&](int x, int y) { return in(x, y) && isStreet(x, y) && (!walled || inside[I(x, y)]); };
  for (int round = 0; round < 8; round++) {
    // the street pieces (4-connected)
    std::vector<int> comp(n, -1), sizes;
    std::vector<int> st;
    for (int y = 1; y < H - 1; y++)
      for (int x = 1; x < W - 1; x++) {
        if (comp[I(x, y)] >= 0 || !streetIn(x, y)) continue;
        const int id = (int)sizes.size();
        int cnt = 0;
        st.clear();
        st.push_back((int)I(x, y));
        comp[I(x, y)] = id;
        while (!st.empty()) {
          const int c = st.back();
          st.pop_back();
          cnt++;
          const int qx = c % W, qy = c / W;
          for (int d = 0; d < 4; d++) {
            const int nx = qx + D4X[d], ny = qy + D4Y[d];
            if (!streetIn(nx, ny) || comp[I(nx, ny)] >= 0) continue;
            comp[I(nx, ny)] = id;
            st.push_back((int)I(nx, ny));
          }
        }
        sizes.push_back(cnt);
      }
    if (sizes.size() < 2) return;
    int mainC = 0;
    for (size_t k = 1; k < sizes.size(); k++) if (sizes[k] > sizes[(size_t)mainC]) mainC = (int)k;
    // a bank tile a bridge may start or land on: open ground, nothing built
    auto bank = [&](int x, int y) {
      if (!in(x, y) || x < 1 || y < 1 || x >= W - 1 || y >= H - 1) return false;
      const size_t i = I(x, y);
      if (water[i] || M.wall[i] || M.bldgAt[i] >= 0 || (!keep.empty() && keep[i]) || inCompound(x, y)) return false;
      if (walled && !inside[i]) return false;
      return mask[i] == K_NONE || mask[i] == K_YARD;
    };
    // the best crossing of any piece of 20+ tiles over the water to the main piece
    int bestScore = 1 << 30, bestLen = 0, bx = -1, by = -1, bd = -1, bw = 1;
    for (int y = 1; y < H - 1; y++)
      for (int x = 1; x < W - 1; x++) {
        const int c = comp[I(x, y)];
        if (c < 0 || c == mainC || sizes[(size_t)c] < 20) continue;
        for (int d = 0; d < 4; d++) {
          int k = 1, gap1 = 0, wet = 0, gap2 = 0;
          int px = x + D4X[d], py = y + D4Y[d];
          while (gap1 < 2 && bank(px, py)) { gap1++; px += D4X[d]; py += D4Y[d]; k++; }
          while (wet < 10 && in(px, py) && water[I(px, py)] && !M.wall[I(px, py)] && (!walled || inside[I(px, py)])) { wet++; px += D4X[d]; py += D4Y[d]; k++; }
          if (!wet) continue;
          while (gap2 < 2 && bank(px, py)) { gap2++; px += D4X[d]; py += D4Y[d]; k++; }
          if (!in(px, py) || comp[I(px, py)] != mainC) continue;
          // (short first, then near the heart: a bridge where the city's life is, not in a corner by the wall)
          const int score = k * 8 + (std::abs(x - cx) + std::abs(y - cy)) / 4;
          if (score >= bestScore) continue;
          // its width: the street's own across (three when the tiles either side carry the same street and land on the
          // main piece too, else one)
          int w = 1;
          {
            const int ox = D4Y[d] != 0 ? 1 : 0, oy = D4X[d] != 0 ? 1 : 0;   // across the crossing
            bool wide = true;
            for (int s2 : {-1, 1}) {
              const int sx = x + ox * s2, sy = y + oy * s2;
              if (!in(sx, sy) || comp[I(sx, sy)] != c) { wide = false; break; }
              const int ex = sx + D4X[d] * k, ey = sy + D4Y[d] * k;
              if (!in(ex, ey) || comp[I(ex, ey)] != mainC) { wide = false; break; }
              for (int t = 1; t < k && wide; t++) {
                const int qx = sx + D4X[d] * t, qy = sy + D4Y[d] * t;
                const size_t qi = I(qx, qy);
                if (M.wall[qi] || M.bldgAt[qi] >= 0 || (walled && !inside[qi]) || inCompound(qx, qy) || (!water[qi] && !bank(qx, qy) && !isStreet(qx, qy))) wide = false;
              }
            }
            if (wide) w = 3;
          }
          bestScore = score; bestLen = k; bx = x; by = y; bd = d; bw = w;
        }
      }
    if (bd < 0) return;
    const int ox = D4Y[bd] != 0 ? 1 : 0, oy = D4X[bd] != 0 ? 1 : 0;
    for (int s2 = -(bw / 2); s2 <= bw / 2; s2++) {
      const int sx = bx + ox * s2, sy = by + oy * s2;
      const uint8_t kind = get(sx, sy) == K_MAIN ? K_MAIN : K_LANE;
      const uint8_t cls = plan.empty() ? 0 : plan[I(sx, sy)];
      for (int t = 1; t < bestLen; t++) {
        const int qx = sx + D4X[bd] * t, qy = sy + D4Y[bd] * t;
        if (isStreet(qx, qy)) continue;
        paintStreet(qx, qy, kind, kind == K_MAIN ? mainG : (laneG == Ground::Dirt ? Ground::Road : laneG));
        if (!plan.empty() && isStreet(qx, qy)) plan[I(qx, qy)] = cls ? cls : (uint8_t)Urb::Lane;
      }
    }
    if (std::getenv("EMB_DEBUG_CITY"))
      std::printf("citygrid: river link %08x at %d,%d dir %d length %d width %d\n", (unsigned)P.seed, bx, by, bd, bestLen, bw);
  }
}

}  // namespace town
}  // namespace ew
