// M7c the order dial's low and mid end (VISION_PLAN 15.M7c). EDGE lane.
//   Villages (low order): one spine road through the middle, the houses scattered round it and loosely facing it, the
//   paths wandering off it; the organic charm the owner loves. A people that plans its towns (the imperial road
//   village, the steppe camp, the sun-temple and spire villages, the dune and highland courts) keeps its flavour on
//   that spine instead of a lattice or rings of lanes: villageSpine. villageLoops then opens every loop of lanes past
//   the first two (a village is no grid: its lanes end in yards and fields).
//   Towns (mid order): the high street and the square (town_layout.cpp mainStreets, squaresPass), a few structured side
//   streets off it near the heart (straight, ranked Rank::Street, the houses fronting them), loosening into the old
//   winding lanes, scattered houses and yards toward the edge: townStreets ranks them.
// Hash-driven choices; the Rng only where the street walker draws its own wander (in the generator's sequence).
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>
#include "rpg/world/town_edge.h"
#include "rpg/world/town_gen.h"

namespace ew {
namespace town {

namespace {
const int D4X[4] = {0, 1, -1, 0}, D4Y[4] = {1, 0, 0, -1};
int ifl(float v) { return (int)std::floor(v); }

// a lane straight out from (x, y) along (ux, uy), up to len tiles, over open land of the village (stops at water, a
// street it would run alongside, or the edge); returns the tiles laid
int spur(Gen& g, float x, float y, float ux, float uy, int len, std::vector<std::pair<int, int>>* laid = nullptr) {
  int px = ifl(x), py = ifl(y), n = 0;
  const bool ns = std::fabs(uy) > std::fabs(ux);
  for (int s = 1; s <= len; s++) {
    const int qx = ifl(x + ux * s), qy = ifl(y + uy * s);
    if (!g.in(qx, qy) || g.water[g.I(qx, qy)] || g.dist(qx, qy) > 0.86f || g.inCompound(qx, qy, 2)) break;
    if (g.isStreet(qx, qy)) { if (s > 2) break; px = qx; py = qy; continue; }
    if (!g.laneOk(qx, qy, ns, px, py)) break;
    if (qx != px && qy != py) { g.paintStreet(qx, py, K_LANE, g.laneG); if (laid) laid->push_back({qx, py}); }
    g.paintStreet(qx, qy, K_LANE, g.laneG);
    if (laid) laid->push_back({qx, qy});
    px = qx; py = qy;
    n++;
  }
  return n;
}

}  // namespace

// A village of a planned people: the spine and its people's flavour along it (called by ringRoads in place of the
// lattice, the rings or the back-lane grid; the main roads along the bearings are already laid).
//   Linear (the road village of the imperial, river and fjord folk): the spine along the water or the strongest road,
//          one short back lane on the land side joined to it once, a farm track or two out to the fields
//   Radial (the steppe camp, the sun-temple and spire villages): the roads meet at the round green; loose clusters of
//          homes on short spur lanes off the spine, alternating sides, each ending in its own trodden yard
//   Compound (the dune and highland courts): the courts line the spine and two or three straight spurs off it
void villageSpine(Gen& g) {
  const uint32_t h = hash32(g.P.seed ^ 0x5B1E7A11u);
  if (g.style == cult::Layout::Linear) {
    g.pickSpine();
    const float ext = std::max(g.rx, g.ry) * 1.2f;
    g.walkStreet(g.spineX, g.spineY, g.spineA, g.spineA, (int)ext, 1, K_MAIN, g.mainG, 1);
    g.walkStreet(g.spineX, g.spineY, g.spineA + D_PI, dwrap(g.spineA + D_PI), (int)ext, 1, K_MAIN, g.mainG, 1);
    if (std::fabs(g.spineX - (g.cx + 0.5f)) + std::fabs(g.spineY - (g.cy + 0.5f)) > 2.0f) g.line(g.cx + 0.5f, g.cy + 0.5f, g.spineX, g.spineY, 1, K_MAIN, g.mainG);
    const float ca = dcos(g.spineA), sa = dsin(g.spineA), nx = -sa, ny = ca;
    const bool ns = std::fabs(sa) > std::fabs(ca);
    // the land side: the side with less water seven tiles off the spine
    int wet[2] = {0, 0};
    for (int side = 0; side < 2; side++)
      for (float t = -ext * 0.5f; t <= ext * 0.5f; t += 2.0f) {
        const int x = ifl(g.spineX + ca * t + nx * (side ? 7 : -7)), y = ifl(g.spineY + sa * t + ny * (side ? 7 : -7));
        if (!g.in(x, y) || g.water[g.I(x, y)]) wet[side]++;
      }
    const int side = wet[0] == wet[1] ? ((h & 1u) ? 1 : -1) : (wet[1] < wet[0] ? 1 : -1);
    const float off = (float)side * (6.5f + (float)((h >> 1) % 3u));
    const float t0 = -ext * (0.22f + ((h >> 3) % 4u) * 0.05f), t1 = ext * (0.25f + ((h >> 5) % 4u) * 0.06f);
    const float ph = hashf(1, 2, g.bseed) * D_TAU;
    int px = -10000, py = 0;
    std::vector<std::pair<int, int>> back;
    for (float t = t0; t <= t1; t += 0.5f) {
      const float wob = dsin(t * 0.09f + ph) * 1.1f;
      const int x = ifl(g.spineX + ca * t + nx * (off + wob)), y = ifl(g.spineY + sa * t + ny * (off + wob));
      if (x == px && y == py) continue;
      const bool ok = g.laneOk(x, y, ns, px, py) && g.dist(x, y) < 0.85f;
      if (!ok) { px = -10000; continue; }
      if (px != -10000 && x != px && y != py) g.paintStreet(x, py, K_LANE, g.laneG);
      g.paintStreet(x, y, K_LANE, g.laneG);
      back.push_back({x, y});
      px = x; py = y;
    }
    // joined to the spine once, near the heart (the back lane's other end runs out into the yards)
    if (!back.empty()) {
      const float tj = (hashf(3, 4, g.bseed) - 0.5f) * 6.0f;
      const float x = g.spineX + ca * tj, y = g.spineY + sa * tj;
      spur(g, x + 0.5f * nx * side, y + 0.5f * ny * side, nx * side, ny * side, (int)std::fabs(off) + 2);
    }
    // a farm track or two out the far side to the fields (dead ends)
    const int tracks = 1 + (int)((h >> 9) % 2u);
    for (int k = 0; k < tracks; k++) {
      const float t = ((k ? -1.0f : 1.0f) * (0.30f + hashf(k, 5, g.bseed) * 0.25f)) * ext;
      spur(g, g.spineX + ca * t, g.spineY + sa * t, -nx * side, -ny * side, 5 + (int)((h >> (11 + k)) % 3u));
    }
    return;
  }
  // Radial and Compound: spurs off the main roads, alternating sides
  std::vector<std::pair<int, int>> mains;
  for (auto& s : g.mainTiles) {
    const float d = g.dist(s.first, s.second);
    if (d > 0.22f && d < 0.62f) mains.push_back(s);
  }
  if (mains.empty()) return;
  const bool court = g.style == cult::Layout::Compound;
  // the camp's ring: a trodden lane round the green, hugging its edge (the people gather round their green: the
  // rings of the old camp kept as the one ring that encloses nothing but the green), crossing the roads it meets
  if (!court) {
    // the green's own outline per bearing (the round green and the market opened out on one side), smoothed
    const int NR = 48;
    std::vector<float> edge((size_t)NR, 3.0f);
    for (int k = 0; k < NR; k++) {
      const float a = k * (D_TAU / NR);
      for (float r = 0.5f; r < 14.0f; r += 0.5f) {
        const int x = ifl(g.cx + 0.5f + dcos(a) * r), y = ifl(g.cy + 0.5f + dsin(a) * r);
        if (!g.in(x, y)) break;
        if (g.get(x, y) == K_SQUARE) edge[(size_t)k] = r;
        else if (r > edge[(size_t)k] + 1.5f) break;
      }
    }
    for (int pass = 0; pass < 2; pass++) {
      std::vector<float> t = edge;
      for (int k = 0; k < NR; k++) t[(size_t)k] = std::max(edge[(size_t)k], (edge[(size_t)((k + NR - 1) % NR)] + 2 * edge[(size_t)k] + edge[(size_t)((k + 1) % NR)]) * 0.25f);
      edge.swap(t);
    }
    const float off = 1.4f + (float)((h >> 6) % 2u) * 0.5f;
    int px = -10000, py = 0;
    const float step = D_TAU / 360.0f;
    for (float a = 0; a <= D_TAU + step; a += step) {
      const float u = a * (NR / D_TAU);
      const int k0 = ((int)std::floor(u)) % NR, k1 = (k0 + 1) % NR;
      const float f = u - std::floor(u);
      const float wob = (vnoise(dcos(a) * 1.6f + 3, dsin(a) * 1.6f + 3, g.bseed + 0x41Cu) - 0.5f) * 0.9f;
      const float R = edge[(size_t)k0] * (1 - f) + edge[(size_t)k1] * f + off + wob;
      const int x = ifl(g.cx + 0.5f + dcos(a) * R), y = ifl(g.cy + 0.5f + dsin(a) * R);
      if (x == px && y == py) continue;
      if (!g.in(x, y) || g.water[g.I(x, y)] || g.inCompound(x, y, 2) || g.get(x, y) == K_SQUARE) { px = -10000; continue; }
      // (4-connected: a diagonal step takes the corner tile too, unless that is the green's)
      if (px != -10000 && x != px && y != py && g.get(x, py) != K_SQUARE && !g.water[g.I(x, py)]) g.paintStreet(x, py, K_LANE, g.laneG);
      if (g.get(x, y) == K_NONE || g.get(x, y) == K_YARD) g.paintStreet(x, y, K_LANE, g.laneG);
      px = x; py = y;
    }
  }
  const int want = court ? 2 + (int)(h % 2u) : 3 + (int)(h % 2u);
  std::vector<std::pair<int, int>> starts;
  for (int k = 0, t = 0; k < want && t < 60; t++) {
    const auto s = mains[(size_t)(hash2(t, k, g.bseed ^ 0x5B1Eu) % (uint32_t)mains.size())];
    bool near = false;
    for (auto& o : starts) if (std::abs(o.first - s.first) + std::abs(o.second - s.second) < 9) near = true;
    if (near) continue;
    // across the road: the main road's heading here is the bearing from the heart
    const float a = g.angleOf(s.first, s.second);
    const int sd = ((k + (int)(h >> 4)) & 1) ? 1 : -1;
    float ux = -dsin(a) * sd, uy = dcos(a) * sd;
    // the camp's spurs lean outward a little (a cluster off the road, not a cross street)
    ux += dcos(a) * 0.45f; uy += dsin(a) * 0.45f;
    const float L = std::sqrt(ux * ux + uy * uy);
    ux /= L; uy /= L;
    const int len = court ? 5 + (int)(hash2(k, 7, g.bseed) % 3u) : 5 + (int)(hash2(k, 7, g.bseed) % 4u);   // (a court spur stays a short lane, never a straight street)
    std::vector<std::pair<int, int>> laid;
    if (spur(g, s.first + 0.5f, s.second + 0.5f, ux, uy, len, &laid) < 3) continue;
    starts.push_back(s);
    k++;
    if (court || laid.empty()) continue;
    // its trodden yard at the end, where the tents of the cluster gather round
    const int ex = laid.back().first, ey = laid.back().second;
    for (int oy = -1; oy <= 1; oy++)
      for (int ox = -1; ox <= 1; ox++) {
        const int x = ex + ox, y = ey + oy;
        if (!g.in(x, y) || g.water[g.I(x, y)] || g.get(x, y) != K_NONE || (std::abs(ox) + std::abs(oy) == 2 && (hash2(x, y, g.bseed) & 1u))) continue;
        g.paintStreet(x, y, K_LANE, g.laneG);
      }
  }
}

// A village is no grid: past the first two (one by the sea), every loop of lanes is opened where it does least (the lane stretch on
// the loop's rim farthest from the heart, a connector between two junctions taken up whole when short, else its
// middle five tiles), so the lanes end in yards and fields. The village's main roads and squares stay. Loops are the
// blocks of rpg/world/town_metrics.cpp (land of 16..700 tiles cut off by ways, water and walls, half its rim street).
void villageLoops(Gen& g) {
  const int W = g.W, H = g.H;
  auto way = [&](int x, int y) { return g.in(x, y) && g.isStreet(x, y); };
  auto land = [&](int x, int y) { return g.in(x, y) && !way(x, y) && !g.water[g.I(x, y)] && !g.wet(x, y) && !g.M.wall[g.I(x, y)]; };
  bool cut = false;
  // ((M7c fixer) a lot's gate keeps the lane to it: phase 15's look (after the lots) cut the only way to seed 33's
  // river-village lot, its gate then opened onto grass)
  std::vector<uint8_t> lotWay((size_t)W * H, 0);
  for (const PlotPlan& pp : g.O.plots) {
    const int lx = pp.gateX - g.O.gx, ly = pp.gateY - g.O.gy;
    for (int oy = -2; oy <= 2; oy++)
      for (int ox = -2; ox <= 2; ox++) if (g.in(lx + ox, ly + oy)) lotWay[g.I(lx + ox, ly + oy)] = 1;
  }
  // (a village by the sea or a lake has little land: its shore closes loops of its own, so one loop is its share)
  int wetN = 0;
  for (uint8_t w : g.water) wetN += w ? 1 : 0;
  // ((M7c fixer, review: "villages over the structure cap of 40" on seeds 11..90: the dune and highland courts, the
  // jade and the elven villages) a people that plans its towns keeps one loop in its village: its courts and spurs
  // closed a second loop into a little grid of lanes; the organic and road villages keep their two)
  const bool planner = g.style != cult::Layout::Organic && g.style != cult::Layout::Linear;
  const int allowed = (wetN * 12 >= W * H || planner) ? 1 : 2;
  for (int round = 0; round < 12; round++) {
    std::vector<int> lab((size_t)W * H, -1);
    struct Blk { int area; bool edge; int rim, rimWay; float far; int cutAt; std::vector<std::pair<int, int>> cand; };
    std::vector<Blk> blocks;
    std::vector<int> st;
    for (int y0 = 0; y0 < H; y0++)
      for (int x0 = 0; x0 < W; x0++) {
        if (!land(x0, y0) || lab[g.I(x0, y0)] >= 0) continue;
        const int id = (int)blocks.size();
        Blk b{0, false, 0, 0, -1.0f, -1, {}};
        st.clear();
        st.push_back((int)g.I(x0, y0));
        lab[g.I(x0, y0)] = id;
        while (!st.empty()) {
          const int i = st.back();
          st.pop_back();
          const int x = i % W, y = i / W;
          b.area++;
          if (x == 0 || y == 0 || x == W - 1 || y == H - 1) b.edge = true;
          for (int d = 0; d < 4; d++) {
            const int nx = x + D4X[d], ny = y + D4Y[d];
            if (!land(nx, ny)) {
              b.rim++;
              if (way(nx, ny)) {
                b.rimWay++;
                // a cut candidate: a plain lane tile (two street neighbours in a line, no bridge) with land of
                // another piece straight across it
                const int ax = nx + D4X[d], ay = ny + D4Y[d];
                // (once the homes stand, never a lane a door opens onto)
                const bool fronted = (!g.front.empty() && (g.front[g.I(nx, ny)] || (ny > 0 && g.M.bldgAt[g.I(nx, ny - 1)] >= 0))) || lotWay[g.I(nx, ny)];
                if (g.get(nx, ny) == K_LANE && g.M.at(nx, ny) != Ground::Bridge && land(ax, ay) && !fronted) {
                  int nb = 0;
                  for (int e = 0; e < 4; e++) if (way(nx + D4X[e], ny + D4Y[e])) nb++;
                  const bool line = nb == 2 && ((way(nx - 1, ny) && way(nx + 1, ny)) || (way(nx, ny - 1) && way(nx, ny + 1)));
                  if (line) b.cand.push_back({(int)g.I(nx, ny), (int)g.I(ax, ay)});
                }
              }
              continue;
            }
            if (lab[g.I(nx, ny)] >= 0) continue;
            lab[g.I(nx, ny)] = id;
            st.push_back((int)g.I(nx, ny));
          }
        }
        blocks.push_back(b);
      }
    std::vector<int> real;
    for (int k = 0; k < (int)blocks.size(); k++) {
      Blk& b = blocks[(size_t)k];
      if (b.edge || b.area < 16 || b.area > 700 || b.rimWay * 2 < b.rim) continue;
      real.push_back(k);
      // a cut opens the loop only where the land straight across the lane is another piece (the lane lies between)
      for (const auto& c : b.cand) {
        if (lab[(size_t)c.second] == k) continue;
        const int x = c.first % W, y = c.first / W;
        const float far = g.dist(x, y) + (float)(hash2(x, y, g.bseed ^ 0x100Bu) % 64u) * 0.0005f;
        if (far > b.far) { b.far = far; b.cutAt = c.first; }
      }
    }
    if ((int)real.size() <= allowed) break;
    // the loop to open: the one whose cut lies farthest out
    int pick = -1;
    for (int k : real) if (blocks[(size_t)k].cutAt >= 0 && (pick < 0 || blocks[(size_t)k].far > blocks[(size_t)pick].far)) pick = k;
    if (pick < 0) break;
    // the lane stretch through the cut: along the plain lane tiles both ways to the junctions
    const int c0 = blocks[(size_t)pick].cutAt;
    std::vector<int> seg{c0};
    std::vector<uint8_t> inSeg((size_t)W * H, 0);
    inSeg[(size_t)c0] = 1;
    for (size_t q = 0; q < seg.size() && seg.size() < 40; q++) {
      const int x = seg[q] % W, y = seg[q] / W;
      for (int d = 0; d < 4; d++) {
        const int nx = x + D4X[d], ny = y + D4Y[d];
        if (!g.in(nx, ny) || inSeg[g.I(nx, ny)] || g.get(nx, ny) != K_LANE || g.M.at(nx, ny) == Ground::Bridge) continue;
        int nb = 0;
        for (int e = 0; e < 4; e++) if (way(nx + D4X[e], ny + D4Y[e])) nb++;
        if (nb > 2) continue;   // a junction ends the stretch
        if (g.front[g.I(nx, ny)] || (ny > 0 && g.M.bldgAt[g.I(nx, ny - 1)] >= 0) || lotWay[g.I(nx, ny)]) continue;   // (a door's lane stays, a lot's)
        inSeg[g.I(nx, ny)] = 1;
        seg.push_back((int)g.I(nx, ny));
      }
    }
    std::vector<int> take;
    if (seg.size() <= 12) take = seg;
    else {
      // its middle five: the five tiles nearest the cut along the stretch (breadth-first order from it)
      for (size_t q = 0; q < seg.size() && take.size() < 5; q++) take.push_back(seg[q]);
    }
    for (int i : take) {
      const int x = i % W, y = i / W;
      g.set(x, y, K_NONE);
      g.M.setG(x, y, g.water[(size_t)i] ? Ground::Water : g.base);
    }
    cut = true;
  }
  if (cut) {
    auto gone = [&](const std::pair<int, int>& t) { const uint8_t k = g.get(t.first, t.second); return k != K_MAIN && k != K_LANE && k != K_SQUARE; };
    g.mainTiles.erase(std::remove_if(g.mainTiles.begin(), g.mainTiles.end(), gone), g.mainTiles.end());
    g.allStreet.erase(std::remove_if(g.allStreet.begin(), g.allStreet.end(), gone), g.allStreet.end());
  }
}

// A town's structured side streets: the lanes near the heart that run straight for a block (seven tiles or more,
// either way) are its streets (Rank::Street), the houses fronting them; further out they stay the town's lanes.
void townStreets(Gen& g) {
  const int W = g.W, H = g.H;
  auto lane = [&](int x, int y) { return g.in(x, y) && g.get(x, y) == K_LANE; };
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (!lane(x, y) || g.dist(x, y) > 0.62f) continue;
      int hx = 1, vy = 1;
      for (int k = 1; lane(x - k, y) && k < 40; k++) hx++;
      for (int k = 1; lane(x + k, y) && k < 40; k++) hx++;
      for (int k = 1; lane(x, y - k) && k < 40; k++) vy++;
      for (int k = 1; lane(x, y + k) && k < 40; k++) vy++;
      if (std::max(hx, vy) >= 7) g.setRank(x, y, Rank::Street);
    }
}

}  // namespace town
}  // namespace ew

namespace ew {
namespace town {

// A town's structured core (the order dial's mid end): round the square and the high street, a back street either side
// of the heart running straight for a few blocks and two cross streets joining them, paved like the high street, so the
// heart of a town reads as streets and blocks with houses fronting them; the town's winding lanes and its scattered
// houses and yards take over beyond (fabric, lanes). Organic towns (a culture's lattice, rings, spine or terraces are
// their own structure). Ranked Rank::Street.
void townCore(Gen& g) {
  const uint32_t h = hash32(g.P.seed ^ 0x70C0E5u);
  const int dy0 = 8 + (int)(h % 3u), dy1 = 8 + (int)((h >> 2) % 3u);
  const int span = (int)(g.rx * (0.30f + ((h >> 4) % 4u) * 0.04f));
  const int dx0 = 11 + (int)((h >> 8) % 4u), dx1 = 11 + (int)((h >> 10) % 4u);
  auto lay = [&](int x, int y) {
    if (!g.in(x, y) || g.water[g.I(x, y)] || g.inCompound(x, y, 2) || g.get(x, y) == K_SQUARE) return false;
    if (g.get(x, y) == K_NONE || g.get(x, y) == K_YARD) g.paintStreet(x, y, K_LANE, g.mainG);
    if (g.get(x, y) == K_LANE) g.setRank(x, y, Rank::Street);
    return true;
  };
  const int yN = g.cy - dy0, yS = g.cy + dy1, xW = g.cx - dx0, xE = g.cx + dx1;
  // the back streets (east-west: the doors face south onto them)
  for (int y : {yN, yS}) {
    for (int x = g.cx; x <= g.cx + span; x++) if (!lay(x, y)) break;
    for (int x = g.cx - 1; x >= g.cx - span; x--) if (!lay(x, y)) break;
  }
  // the cross streets between them (and on a little past, a block each way when there is room)
  const int ext = (int)((h >> 12) % 2u) * 6;
  for (int x : {xW, xE}) {
    for (int y = yN - ((h >> 13) & 1u ? ext : 0); y <= yS + ((h >> 14) & 1u ? ext : 0); y++) lay(x, y);
  }
}

}  // namespace town
}  // namespace ew
