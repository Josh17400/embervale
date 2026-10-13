// M7c the edge rule (VISION_PLAN 15.M7c): every settlement's built-up core loosening into its outskirts and then the
// fields; a city's faubourgs ribboning along each approach road outside its walls (inns, smithies, stables, houses:
// dense near the gate, sparser further out, sporadic away from the road), then organic farmland. EDGE lane.
//   edgeBegin   phase 1, after the wall ring: the belt, the homes kept for the faubourgs (Gen::faubourgWant: the city
//               plan's own homes aim at homesWant less it)
//   outskirts   phase 9, after the homes' sweep and before the top-up rounds: a city's faubourgs along its approach
//               roads, the farmsteads sporadic away from them
//   edgeFarmland  phase 12 (fields()): the belt's farmland: fields of crops, pastures with their flocks, hay meadows and
//               orchards, densest by the roads and the faubourgs and thinning out into the wild land, hedgerows between
// The faubourgs stand outside the wall, where the generator's placement primitives (town_build.cpp fits) refuse to
// build (walled: inside only): each plot is let into the walled set for the one placement (Gen::inside on its own
// footprint and apron, restored at once), so every other rule of a house (clearances, the V5 sprite rule, its
// footpath to the street) holds unchanged. Hash-driven choices and the generator's own Rng only in sequence.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <vector>
#include "rpg/world/biomes.h"
#include "rpg/world/town_edge.h"
#include "rpg/world/town_gen.h"

namespace ew {
namespace town {

using art::Building;
using art::Prop;

namespace {
const int D4X[4] = {0, 1, -1, 0}, D4Y[4] = {1, 0, 0, -1};

// the tile's place in the belt: its normalised radius over the wall's at its bearing (1 = on the wall line)
float beltT(const Gen& g, int x, int y) {
  const float dx = (x - g.cx) / g.rx, dy = (y - g.cy) / g.ry;
  const float r = std::sqrt(dx * dx + dy * dy);
  return r / std::max(0.05f, edgeWallRho(g, datan2(dy, dx)));
}

// a placement outside the wall: the footprint, its ring and the apron let into the walled set for the one call
bool placeOutside(Gen& g, Building t, Role owner, int bw, int bh, int ax, int ay, IRect* made) {
  const IRect r{ax - bw / 2, ay - bh, bw, bh};
  if (!g.in(r.x - 1, r.y - 1) || !g.in(r.x + r.w, r.y + r.h + 2)) return false;
  // ((M7c fixer) never on waste land, nor within two tiles of it: seed 7's HEHO had its west faubourg, barn and
  // haystack on an ash field's glowing cracks, a lava vent by the barn door)
  for (int y = r.y - 2; y <= r.y + r.h + 2; y++)
    for (int x = r.x - 2; x <= r.x + r.w + 1; x++)
      if (g.harshAt(x, y)) return false;
  // never astride the wall or inside it (the city's own ground is the city plan's)
  for (int y = r.y - 1; y <= r.y + r.h + 1; y++)
    for (int x = r.x - 1; x <= r.x + r.w; x++)
      if (g.inside[g.I(x, y)] || g.M.wall[g.I(x, y)]) return false;
  std::vector<std::pair<int, int>> path;
  std::vector<size_t> opened;
  for (int y = r.y; y < r.y + r.h; y++)
    for (int x = r.x; x < r.x + r.w; x++) { const size_t i = g.I(x, y); g.inside[i] = 1; opened.push_back(i); }
  bool ok = g.footpath(ax, ay, path, &r) && g.tryPlace(t, owner, bw, bh, ax, ay, false);
  for (size_t i : opened) g.inside[i] = 0;
  if (!ok) return false;
  // a country path of trodden earth from the door to the road (tryPlace paves a city's footpaths)
  path.push_back({ax, ay});
  for (auto& p : path)
    if (g.in(p.first, p.second) && g.get(p.first, p.second) == K_YARD && g.M.at(p.first, p.second) == Ground::Road) {
      g.M.setG(p.first, p.second, Ground::Dirt);
      g.setRank(p.first, p.second, Rank::Footpath);
    }
  if (made) *made = g.M.bldgs.back().r;
  return true;
}

void markPlot(Gen& g, EdgeData& E, const IRect& r, BlockRole role, uint8_t bit) {
  for (int y = r.y - 1; y <= r.y + r.h + 1; y++)
    for (int x = r.x - 1; x <= r.x + r.w; x++) {
      if (!g.in(x, y) || g.isStreet(x, y) || g.inside[g.I(x, y)]) continue;
      g.setRole(x, y, role);
      E.tile[g.I(x, y)] |= bit;
    }
}

}  // namespace

// (M7c fixer) waste land at a local tile: the biome proper (C.ecoAt) is one of the rare hostile kinds (ash fields,
// crystal barrens, blight). Outside the buffer and without ecoAt: never. No Rng draw.
bool Gen::harshAt(int x, int y) {
  if (!C.ecoAt) return false;
  if (x < -2 || y < -2 || x >= W + 2 || y >= H + 2) return false;
  x = std::clamp(x, 0, W - 1);
  y = std::clamp(y, 0, H - 1);
  if (harshC.size() != (size_t)W * H) harshC.assign((size_t)W * H, 0);
  uint8_t& c = harshC[I(x, y)];
  if (!c) {
    const Eco e = C.ecoAt(O.gx + x, O.gy + y);
    c = ecoHas(e, EF_HOSTILE) && ecoHas(e, EF_RARE) ? 2 : 1;
  }
  return c == 2;
}

// phase 1, after the wall ring and the palace compound's place: the faubourg belt and the homes kept for it
void Gen::edgeBegin() {
  EdgeData& E = edgeOf(*this);   // (made in land(): the belt and the wall's outline)
  E.tile.assign((size_t)W * H, 0);
  E.plots.clear();
  E.faubourgs = E.faubourgBldgs = E.farms = 0;
  faubourgWant = 0;
  if (!city || E.belt <= 0) return;
  // about nine homes along each approach road (a capital's roads busier), the rest of the scale inside the walls
  const uint32_t h = hash32(P.seed ^ 0xFA0B0E6Bu);
  const int roads = (int)bearings.size();
  faubourgWant = std::clamp(roads * 9 + (int)(h % 7u) + (capital ? 6 : 0) + age * 2, 18, 48);
}

// phase 9, after the homes' sweep and before the top-up rounds: the faubourgs along a city's approach roads, then the
// farmsteads sporadic away from them
void Gen::outskirts() {
  EdgeData& E = edgeOf(*this);
  if (!city || E.belt <= 0 || E.tile.size() != (size_t)W * H) return;
  const int pathMax0 = pathMax;
  pathMax = 8;
  // every main gate's approach road, traced outward from the gate (breadth first along its paving, outside the ring)
  std::vector<int> depth((size_t)W * H, -1);
  struct Road { int gate; std::vector<std::vector<int>> at; };   // the road's tiles by distance from the gate
  std::vector<Road> roadsOut;
  for (size_t k = 0; k < O.wallGaps.size(); k++) {
    if (k >= gateBearing.size() || gateBearing[k] < 0) continue;
    const IRect gp = O.wallGaps[k];
    Road R;
    R.gate = (int)k;
    std::vector<int> q;
    for (int y = gp.y - 2; y <= gp.y + gp.h + 1; y++)
      for (int x = gp.x - 2; x <= gp.x + gp.w + 1; x++)
        if (in(x, y) && get(x, y) == K_MAIN && !inside[I(x, y)] && !M.wall[I(x, y)] && depth[I(x, y)] < 0) { depth[I(x, y)] = 0; q.push_back((int)I(x, y)); }
    for (size_t h = 0; h < q.size(); h++) {
      const int c = q[h], x = c % W, y = c / W, d = depth[(size_t)c];
      if ((int)R.at.size() <= d) R.at.resize((size_t)d + 1);
      R.at[(size_t)d].push_back(c);
      for (int dd = 0; dd < 4; dd++) {
        const int nx = x + D4X[dd], ny = y + D4Y[dd];
        if (!in(nx, ny) || depth[I(nx, ny)] >= 0 || get(nx, ny) != K_MAIN || inside[I(nx, ny)] || M.wall[I(nx, ny)]) continue;
        depth[I(nx, ny)] = d + 1;
        q.push_back((int)I(nx, ny));
      }
    }
    if (R.at.size() >= 8) roadsOut.push_back(R);
  }
  // the busiest roads first (the strongest bearings come first in the plan)
  std::stable_sort(roadsOut.begin(), roadsOut.end(), [&](const Road& a, const Road& b) { return gateBearing[(size_t)a.gate] < gateBearing[(size_t)b.gate]; });
  // (the top-up: homesFinish still short of the scale; the ribbons reach further out, cottages only)
  const bool again = E.topUp > 0;
  int innsLeft = again ? 0 : (capital || roadsOut.size() >= 4 ? 2 : 1), smithsLeft = again ? 0 : 1;
  int homesLeft = again ? E.topUp : faubourgWant;
  int innRoad = -1;   // the road the last coaching inn went up on
  const int nRoads = std::max<int>(1, (int)roadsOut.size());
  for (size_t ri = 0; ri < roadsOut.size(); ri++) {
    const Road& R = roadsOut[ri];
    const uint32_t hs = hash32(P.seed ^ (0xFA0Bu + (uint32_t)R.gate * 0x9E37u));
    // how far it ribbons out: the main roads further, every faubourg its own
    const float L = ((ri == 0 ? 26.0f : 18.0f) + (float)(hs % 9u) + (capital ? 4.0f : 0.0f) + age) * (again ? 1.6f : 1.0f);
    int quota = std::max(4, homesLeft / std::max(1, nRoads - (int)ri) + (int)((hs >> 4) % 3u) - 1);
    int made = 0, homes = 0;
    bool barn = !again && (((hs >> 8) & 1u) != 0 || ri == 0);
    const int D = (int)R.at.size();
    for (int d = 3; d < D && homes < quota; d++) {
      if (d > (int)(L * 1.45f)) break;
      // the road's heading here (from a few tiles back to a few ahead)
      auto mid = [&](int dd, float& mx, float& my) {
        dd = std::clamp(dd, 0, D - 1);
        mx = 0; my = 0;
        for (int c : R.at[(size_t)dd]) { mx += c % W; my += c / W; }
        mx /= (float)R.at[(size_t)dd].size(); my /= (float)R.at[(size_t)dd].size();
      };
      float ax0, ay0, ax1, ay1;
      mid(d - 3, ax0, ay0);
      mid(d + 3, ax1, ay1);
      const bool ew = std::fabs(ax1 - ax0) >= std::fabs(ay1 - ay0);
      // dense by the gate, thinning out along the road
      const float p = std::clamp(1.25f - (d - 3) / L, 0.10f, 0.95f);
      for (int side = 0; side < 2 && homes < quota; side++) {
        if (hashf(d, side + (int)R.gate * 7, P.seed ^ (again ? 0xFA1Cu : 0xFA0Cu)) > (again ? std::max(p, 0.6f) : p)) continue;
        // the road's edge tile on this side
        int ex = -1, ey = -1;
        for (int c : R.at[(size_t)d]) {
          const int x = c % W, y = c / W;
          if (ex < 0) { ex = x; ey = y; continue; }
          if (ew ? (side == 0 ? y < ey : y > ey) : (side == 0 ? x < ex : x > ex)) { ex = x; ey = y; }
        }
        if (ex < 0) continue;
        // what stands here: the coaching inn and the farrier near the gate, a stable barn, then houses
        Building t = Building::House;
        Role owner = Role::Villager;
        int bw = 3, bh = 2;
        // (one coaching inn to a road, a capital's second on another road and of its own size: never two alike)
        if (innsLeft > 0 && d <= 16 && innRoad != (int)ri) { t = Building::Inn; owner = Role::Innkeeper; bw = 5 + (int)((hs + (uint32_t)innsLeft) % 2u); bh = 3 + (innRoad >= 0 ? 1 : 0); }
        else if (smithsLeft > 0 && d >= 6 && d <= 22 && made >= 1) { t = Building::Smithy; owner = Role::Smith; bw = 5; bh = 3; }
        else if (barn && d >= 8 && made >= 2) { t = Building::Farmhouse; owner = Role::Farmer; bw = 5; bh = 3; }
        else homeShape(d > L * 0.6f ? District::Poor : District::Crafts, t, bw, bh);
        if (t == Building::StoneHouse && d > L * 0.7f) t = Building::House;   // (out along the road, cottages)
        const int jx0 = (int)(hashAt(d, side, 0xFA0Du) % 3u) - 1, sb = (int)(hashAt(d, side, 0xFA0Eu) % 3u) == 0 ? 1 : 0;
        IRect r;
        bool ok = false;
        for (int jt = 0; jt < 3 && !ok; jt++) {
        const int jx = jx0 + (jt == 1 ? 1 : (jt == 2 ? -1 : 0));
        if (ew) {
          // north of the road the door opens onto it; south of it the house turns its back and a path runs round
          if (side == 0) ok = placeOutside(*this, t, owner, bw, bh, ex + jx, ey - 1 - sb, &r) || placeOutside(*this, t, owner, bw, bh, ex + jx, ey - 2 + sb, &r);
          else ok = placeOutside(*this, t, owner, bw, bh, ex + jx, ey + 2 + bh + sb, &r) || placeOutside(*this, t, owner, bw, bh, ex + jx, ey + 3 + bh - sb, &r);
        } else {
          // beside a road running north-south, the house's side to it, the door south onto a short path
          const int g0 = 1 + sb;
          if (side == 0) ok = placeOutside(*this, t, owner, bw, bh, ex - g0 - bw + bw / 2, ey + 1 + jx, &r) || placeOutside(*this, t, owner, bw, bh, ex - g0 - 1 - bw + bw / 2, ey + jx, &r);
          else ok = placeOutside(*this, t, owner, bw, bh, ex + g0 + bw / 2, ey + 1 + jx, &r) || placeOutside(*this, t, owner, bw, bh, ex + g0 + 1 + bw / 2, ey + jx, &r);
        }
        }
        if (!ok) continue;
        made++;
        if (t == Building::Inn) { innsLeft--; innRoad = (int)ri; }
        else if (t == Building::Smithy) smithsLeft--;
        else if (t == Building::Farmhouse) {
          barn = false;
          // the stable yard: a trough and a haystack beside the barn
          const int tx = r.x + r.w + 1, ty = r.y + r.h - 1;
          if (freeTile(tx, ty) && !cover[I(tx, ty)]) { M.setProp(tx, ty, Prop::Trough); set(tx, ty, K_YARD); }
          if (freeTile(r.x - 2, ty) && !cover[I(r.x - 2, ty)]) { M.setProp(r.x - 2, ty, Prop::Haystack); set(r.x - 2, ty, K_YARD); }
        }
        if (townIsHome(t)) { homes++; hHave++; }
        markPlot(*this, E, r, BlockRole::Faubourg, EDGE_FAUBOURG);
        E.plots.push_back(r);
        E.faubourgBldgs++;
      }
    }
    homesLeft -= homes;
    if (made >= 2 && !again) E.faubourgs++;
  }
  // the farmsteads sporadic away from the roads: a farmhouse out in the belt, its path to the nearest road
  if (!again) {
    pathMax = 16;
    const int want = 2 + (int)(hash32(P.seed ^ 0xFA0Fu) % 4u);
    int put = 0;
    for (int t = 0; t < 240 && put < want; t++) {
      const float a = hashf(t, 1, P.seed ^ 0xFA10u) * D_TAU - D_PI;
      const float rr = edgeWallRho(*this, a) * (1.14f + hashf(t, 2, P.seed ^ 0xFA10u) * 0.22f);
      const int x = (int)std::floor(cx + 0.5f + dcos(a) * rx * rr), y = (int)std::floor(cy + 0.5f + dsin(a) * ry * rr);
      if (!in(x, y) || x < 4 || y < 6 || x >= W - 6 || y >= H - 4) continue;
      // away from the roads (no main road within six tiles)
      bool nearRoad = false;
      for (int oy = -6; oy <= 6 && !nearRoad; oy += 2)
        for (int ox = -6; ox <= 6; ox += 2) if (get(x + ox, y + oy) == K_MAIN) { nearRoad = true; break; }
      if (nearRoad) continue;
      IRect r;
      if (!placeOutside(*this, Building::Farmhouse, Role::Farmer, 5, 3, x, y, &r)) continue;
      put++;
      hHave++;
      markPlot(*this, E, r, BlockRole::Farm, EDGE_FARM);
      E.faubourgBldgs++;
    }
  }
  pathMax = pathMax0;
  O.traits.faubourgs = (uint8_t)std::min(255, E.faubourgs);
  O.traits.faubourgBldgs = (int16_t)E.faubourgBldgs;
}

// phase 12 (fields()): a city's farmland round its faubourgs, organic like the countryside. Patchworks of fields
// grow out from the approach roads (a hedge's width back, past the faubourg's houses) and from the belt: each patch a
// cluster of parcels of their own sizes, a trodden balk between neighbours (a hedgerow of bushes along some), thinning
// out with the distance from the wall into the wild land. Crops mostly; hay meadows, pastures with their flocks and
// orchards among them. Marked Farm.
namespace {
bool fieldFree(Gen& g, int fx, int fy, int fw, int fh) {
  if (fw < 3 || fh < 3) return false;
  const int lv = g.in(fx, fy) ? g.lvl[g.I(fx, fy)] : -1;
  for (int y = fy - 1; y <= fy + fh; y++)
    for (int x = fx - 1; x <= fx + fw; x++) {
      if (!g.in(x, y) || x < 2 || y < 2 || x >= g.W - 2 || y >= g.H - 2) return false;
      const size_t i = g.I(x, y);
      const bool rim = x < fx || y < fy || x >= fx + fw || y >= fy + fh;
      // the rim (the balk round it) may be a neighbour's balk, never a house, a wall or the water
      if (g.M.bldgAt[i] >= 0 || g.M.wall[i] || g.water[i] || g.inside[i] || g.front[i]) return false;
      if (rim) { if (g.mask[i] == K_FIELD) return false; continue; }
      if (g.mask[i] != K_NONE || g.M.prop[i] || groundSolid(g.M.at(x, y)) || g.M.at(x, y) == Ground::Bridge || g.M.at(x, y) == Ground::Swamp) return false;
      if (g.cover[i] || g.noBuild[i] || g.lvl[i] != lv) return false;
      if (g.harshAt(x, y)) return false;   // ((M7c fixer) no field on an ash field or a blight)
    }
  return !edgeInWall(g, fx + fw / 2, fy + fh / 2, -0.07f);
}

// kind: 0 crops, 1 pasture, 2 orchard, 3 hay meadow
void layField(Gen& g, EdgeData& E, int fx, int fy, int fw, int fh, int kind, uint32_t fs) {
  const uint32_t kh = hash2(fx, fy, fs + 2u);
  for (int y = fy; y < fy + fh; y++)
    for (int x = fx; x < fx + fw; x++) {
      g.set(x, y, K_FIELD);
      g.M.setP(x, y, 0);
      g.setRole(x, y, BlockRole::Farm);
      E.tile[g.I(x, y)] |= EDGE_FARM;
    }
  if (kind == 0) {
    for (int y = fy; y < fy + fh; y++)
      for (int x = fx; x < fx + fw; x++) g.M.setG(x, y, Ground::Farmland);
    // a stook of the harvest at one end
    if ((kh >> 8) % 3u == 0) g.M.setProp(fx + (((kh >> 10) & 1u) ? fw - 1 : 0), fy + fh - 1, Prop::Haystack);
  } else if (kind == 1) {
    // a pasture: fenced all round, its gate on the side toward the road, the flock or the herd inside
    const int gx = fx + fw / 2;
    int gy = fy + fh - 1;
    if (g.get(gx, fy - 2) == K_MAIN || g.get(gx, fy - 3) == K_MAIN) gy = fy;
    for (int y = fy; y < fy + fh; y++)
      for (int x = fx; x < fx + fw; x++) {
        if (x == gx && y == gy) continue;
        if (y == fy || y == fy + fh - 1) g.M.setProp(x, y, Prop::FenceH);
        else if (x == fx || x == fx + fw - 1) g.M.setProp(x, y, Prop::FenceV);
      }
    const int iy = gy == fy ? fy + 1 : fy + fh - 2;
    const int tx = fx + 1, ty = gy == fy ? fy + fh - 2 : fy + 1;
    if (fw >= 6 && !(tx == gx && ty == iy)) g.M.setProp(tx, ty, Prop::Trough);
    const bool cows = hashf(fx, fy, fs + 3u) < (g.spec == Specialty::Herding ? 0.35f : 0.55f);
    const int beasts = std::max(2, (fw - 2) * (fh - 2) / 6);
    int put = 0;
    for (int q = 0; q < 40 && put < beasts; q++) {
      const int x = fx + 1 + (int)(hash2(q, fx, fs + 5u) % (uint32_t)(fw - 2)), y = fy + 1 + (int)(hash2(q, fy, fs + 6u) % (uint32_t)(fh - 2));
      // (M7c integration) never on or beside the tile just inside the gate: the herders stand by their beasts (life_game's
      // pasture spots), and one standing there sealed the pen to the player
      if (g.M.prop[g.I(x, y)] || std::abs(x - gx) + std::abs(y - iy) <= 1) continue;
      bool crowd = false;
      for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++) {
          const int pq = g.M.propAt(x + ox, y + oy);
          if (pq == (int)Prop::Sheep + 1 || pq == (int)Prop::Cow + 1) crowd = true;
        }
      if (crowd) continue;
      g.M.setProp(x, y, cows && (q & 1) == 0 ? Prop::Cow : Prop::Sheep);
      if (!g.penOpen(fx, fy, fw, fh, gx, iy)) { g.M.setP(x, y, 0); continue; }   // (the pen's inside stays one piece)
      put++;
    }
  } else if (kind == 2) {
    // an orchard: fruit trees in staggered rows on the grass (their crowns clear of everybody's front)
    for (int y = fy; y < fy + fh; y++)
      for (int x = fx; x < fx + fw; x++) g.M.setG(x, y, g.base == Ground::Grass ? Ground::Meadow : g.base);
    const Prop fruit = g.bio == Biome::Snow ? Prop::SnowPine : Prop::FruitTree;
    for (int y = fy + 1; y < fy + fh; y += 2)
      for (int x = fx + ((y - fy) % 4 == 1 ? 0 : 1); x < fx + fw; x += 2) {
        bool clear = true;
        for (int oy = -4; oy <= 0 && clear; oy++)
          for (int ox = -1; ox <= 1; ox++) if (g.in(x + ox, y + oy) && g.front[g.I(x + ox, y + oy)]) { clear = false; break; }
        if (clear) g.M.setProp(x, y, fruit);
      }
  } else {
    // a hay meadow: long grass and flowers, the cut hay stacked in a row
    for (int y = fy; y < fy + fh; y++)
      for (int x = fx; x < fx + fw; x++) {
        if (g.base == Ground::Grass) g.M.setG(x, y, Ground::Meadow);
        const float r = hashf(x, y, fs + 4u);
        if (y == fy + fh / 2 && (x - fx) % 3 == 1) g.M.setProp(x, y, Prop::Haystack);
        else if (r < 0.22f && g.bio != Biome::Snow && g.bio != Biome::Desert) g.M.setProp(x, y, r < 0.14f ? Prop::TallGrass : Prop::Wildflowers);
      }
  }
}
}  // namespace

void edgeFarmland(Gen& g) {
  EdgeData& E = edgeOf(g);
  if (E.tile.size() != (size_t)g.W * g.H) E.tile.assign((size_t)g.W * g.H, 0);
  const uint32_t fs = g.bseed + 0xFA11u;
  // where the patchworks start: beside the approach roads (outside the wall, past the gate)
  std::vector<std::pair<int, int>> roadT;
  for (auto& s : g.mainTiles)
    if (g.in(s.first, s.second) && !g.inside[g.I(s.first, s.second)] && g.get(s.first, s.second) == K_MAIN && beltT(g, s.first, s.second) > 1.10f) roadT.push_back(s);
  int want = 46 + (int)(hash32(fs) % 14u);
  if (g.arch == Archetype::Farming || g.spec == Specialty::Farming) want += 12;
  const bool herd = g.spec == Specialty::Herding || g.cultureIs((int)cult::Archetype::Steppe);
  auto kindFor = [&](uint32_t h, int depth) {
    const int k = (int)(h % 100u);
    int kind = herd ? (k < 40 ? 0 : (k < 80 ? 1 : 3)) : (k < 62 ? 0 : (k < 74 ? 3 : (k < 87 ? 1 : 2)));
    if ((g.bio == Biome::Snow || g.bio == Biome::Desert) && kind == 2) kind = 0;
    if (depth == 0 && kind == 1 && !herd) kind = 0;
    return kind;
  };
  int made = 0;
  for (int c = 0; c < 140 && made < want; c++) {
    // the patch's first parcel: beside a road (most), else out in the belt
    int fx, fy;
    const uint32_t ch = hash2(c, 17, fs);
    const int fw = 6 + (int)(ch % 5u), fh = 4 + (int)((ch >> 4) % 3u);
    if (c % 4 != 3 && !roadT.empty()) {
      const auto s = roadT[(size_t)((ch >> 8) % (uint32_t)roadT.size())];
      const int side = ((ch >> 20) & 1u) ? 1 : -1, back = 2 + (int)((ch >> 21) % 2u);
      // across the road's run (its heading from its neighbours)
      const bool ew = g.get(s.first - 2, s.second) == K_MAIN || g.get(s.first + 2, s.second) == K_MAIN;
      if (ew) { fx = s.first - fw / 2; fy = side > 0 ? s.second + back : s.second - back - fh + 1; }
      else { fy = s.second - fh / 2; fx = side > 0 ? s.first + back : s.first - back - fw + 1; }
    } else {
      const float a = hashf(c, 3, fs) * D_TAU - D_PI;
      const float rr = edgeWallRho(g, a) * (1.12f + hashf(c, 4, fs) * 0.26f);
      fx = (int)std::floor(g.cx + dcos(a) * g.rx * rr) - fw / 2;
      fy = (int)std::floor(g.cy + dsin(a) * g.ry * rr) - fh / 2;
    }
    if (beltT(g, fx + fw / 2, fy + fh / 2) < 1.08f || !fieldFree(g, fx, fy, fw, fh)) continue;
    // grow the patch: neighbours across a one-tile balk, each its own size, offset a little, fewer the further out
    struct Parcel { int x, y, w, h, depth; };
    std::vector<Parcel> patch{{fx, fy, fw, fh, 0}};
    layField(g, E, fx, fy, fw, fh, kindFor(hash2(fx, fy, fs + 7u), 0), fs);
    made++;
    const int size = 3 + (int)((ch >> 12) % 5u);
    const bool hedged = ((ch >> 16) % 3u) == 0 && g.bio != Biome::Desert;
    for (size_t q = 0; q < patch.size() && (int)patch.size() < size && made < want; q++) {
      const Parcel P = patch[q];
      for (int dir = 0; dir < 4 && (int)patch.size() < size; dir++) {
        const uint32_t nh = hash2(P.x * 7 + dir, P.y * 3 + (int)q, fs + 9u);
        if (nh % 100u < 22u) continue;
        int nw = std::max(4, P.w + (int)(nh % 5u) - 2), nhh = std::max(3, P.h + (int)((nh >> 4) % 3u) - 1);
        const int jit = (int)((nh >> 8) % 3u) - 1;
        int nx = P.x, ny = P.y;
        switch (dir) {
          case 0: nx = P.x + P.w + 1; ny = P.y + jit; break;   // east
          case 1: nx = P.x - 1 - nw; ny = P.y + jit; break;    // west
          case 2: nx = P.x + jit; ny = P.y + P.h + 1; break;   // south
          default: nx = P.x + jit; ny = P.y - 1 - nhh; break;  // north
        }
        const float bt = beltT(g, nx + nw / 2, ny + nhh / 2);
        if (bt < 1.08f || hashf(nx, ny, fs + 11u) > std::clamp(1.6f - (bt - 1.08f) * 1.5f, 0.2f, 1.0f)) continue;
        if (!fieldFree(g, nx, ny, nw, nhh)) {
          // a little smaller fits the corner
          nw = std::max(4, nw - 2); nhh = std::max(3, nhh - 1);
          if (dir == 1) nx = P.x - 1 - nw;
          if (dir == 3) ny = P.y - 1 - nhh;
          if (!fieldFree(g, nx, ny, nw, nhh)) continue;
        }
        layField(g, E, nx, ny, nw, nhh, kindFor(hash2(nx, ny, fs + 7u), P.depth + 1), fs);
        patch.push_back({nx, ny, nw, nhh, P.depth + 1});
        made++;
        // the balk between them: a hedgerow along it in a bocage patch (a gap for the farmer)
        if (hedged) {
          const bool vert = dir < 2;
          const int bx = dir == 0 ? P.x + P.w : (dir == 1 ? P.x - 1 : 0), by = dir == 2 ? P.y + P.h : (dir == 3 ? P.y - 1 : 0);
          const int from = vert ? std::max(P.y, ny) : std::max(P.x, nx), to = vert ? std::min(P.y + P.h, ny + nhh) : std::min(P.x + P.w, nx + nw);
          const int gap = from + (to - from) / 2;
          for (int t = from; t < to; t++) {
            if (t == gap) continue;
            const int x = vert ? bx : t, y = vert ? t : by;
            if (!g.in(x, y)) continue;
            const size_t i = g.I(x, y);
            if (g.mask[i] != K_NONE || g.M.prop[i] || g.M.bldgAt[i] >= 0 || g.cover[i] || g.front[i] || g.water[i]) continue;
            if (hashf(x, y, fs + 13u) < 0.18f) continue;
            g.M.setProp(x, y, g.bio == Biome::Snow ? Prop::SnowBush : Prop::Bush);
            g.set(x, y, K_YARD);
          }
        }
      }
    }
    E.farms++;
  }
}

}  // namespace town
}  // namespace ew
