// rpg_test --endless [--seeds A..B] [--map dir] [--quick] [--golden [--write]]: the endless generator (rpg/world,
// VISION_PLAN 2, 11, 15.8). WORLD lane owns this file.
//
// Per seed:
//   - the start plan (2.6 adjusted for 15.8): start village in plains or forest 80+ tiles from the sea; the story city
//     (story + capital) 150-300 tiles away; three shard ruins 180-450 tiles away with rising danger; the lair 400-650
//     tiles away on high ground; at least 6 POI kinds within 200 tiles
//   - order independence: the same chunks in two orders (and cache histories), near the start and 1e5 tiles away
//   - spacing over 2048 x 2048 (15.8): nearest-neighbour distance >= 150 everywhere, median >= 220; cities >= ~600 apart
//   - kingdoms: every city is its kingdom's capital (SPF_CAPITAL), every kingdom's capital is its city; settlement
//     names unique within 3x3 regions
//   - land: rock <= 10 % of land (the brown blobs are gone), relief present
//   - reachability (2.7, 11.4; skipped with --quick): one 1024 x 1024 window around the start is generated chunk by chunk
//     and flooded from the road network; every settlement and story site in the inner 768 x 768 must be reached, and no
//     plateau of 24+ tiles may be walled in by cliffs alone
//   - cost budgets (2.8): chunk average <= 1 ms native, region plans <= 3 ms on average (--no-budget: report only)
// --golden: hashes of region plans and chunks for three seeds near the origin and at +-1e5 tiles, compared with
//   tests/fixtures/golden_endless.txt (integer maths only: must match on every platform; --write prints fresh values).
//   Settlement interiors (the towns lane's output) are masked out, so only this generator's decisions are pinned.
// --map dir writes, per seed: endless_<seed>.png (2048^2 tiles at 2 tiles/px: biomes, relief shading, rivers, lakes,
//   roads, kingdom borders, sites), endless_<seed>_far.png (16384^2 at 32 tiles/px: continents) and, unless --quick,
//   endless_<seed>_window.png (the 1024^2 reachability window at 1 tile/px: cliffs, ramps, unreached ground in red).
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/world/coords.h"
#include "rpg/world/ids.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {
using namespace ew;

uint64_t chunkHash(const ChunkData& c) {
  uint64_t h = 1469598103934665603ull;
  auto add = [&](const uint8_t* p, size_t n) { for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 1099511628211ull; } };
  add(c.ground, sizeof c.ground); add(c.prop, sizeof c.prop); add(c.biome, sizeof c.biome); add(c.height, sizeof c.height); add(c.wall, sizeof c.wall);
  add(c.blend, sizeof c.blend);   // (M2 ecotones)
  for (const Bldg& b : c.bldgs) { uint64_t v[] = {b.id, (uint64_t)(uint32_t)b.r.x, (uint64_t)(uint32_t)b.r.y, (uint64_t)b.type}; add((const uint8_t*)v, sizeof v); }
  for (const SpawnPlan& s : c.spawns) { uint64_t v[] = {s.siteId, (uint64_t)(uint32_t)s.sp.x, (uint64_t)(uint32_t)s.sp.y, (uint64_t)s.sp.slot}; add((const uint8_t*)v, sizeof v); }
  return h;
}

bool isSettle(SiteType t) { return t == SiteType::City || t == SiteType::Town || t == SiteType::Village; }

// (M1 economy) the real world's spread of specialisations (owner: believable variety; lumber was the default for
// temperate land): tallied over every seed a command visits, checked at its end
int g_spec[(int)Specialty::COUNT] = {};
int g_specArch[8] = {};
void tallySpecialties(const SitePlan& p) {
  if ((int)p.special < (int)Specialty::COUNT) g_spec[(int)p.special]++;
  if ((int)p.archetype < 8) g_specArch[(int)p.archetype]++;
}
// prints the spread; fails when one specialisation holds more than 40 % (with enough settlements to judge) or a
// specialisation never occurs
int checkSpecialties(const char* tag) {
  int n = 0, bad = 0;
  for (int k = 1; k < (int)Specialty::COUNT; k++) n += g_spec[k];
  printf("%s: specialisations over %d settlements:", tag, n);
  for (int k = 1; k < (int)Specialty::COUNT; k++) printf(" %s %d (%d%%)", specialtyName((Specialty)k), g_spec[k], n ? g_spec[k] * 100 / n : 0);
  printf(" | archetypes:");
  static const char* an[8] = {"plain", "farming", "fishing", "port", "mining", "river", "hillfort", "market"};
  for (int k = 0; k < 8; k++) printf(" %s %d", an[k], g_specArch[k]);
  printf("\n");
  if (n >= 40)
    for (int k = 1; k < (int)Specialty::COUNT; k++) {
      if (g_spec[k] * 100 > n * 40) { printf("FAIL: %s: %s is %d%% of settlements (max 40%%)\n", tag, specialtyName((Specialty)k), g_spec[k] * 100 / n); bad++; }
      if (!g_spec[k]) { printf("FAIL: %s: no %s settlement at all\n", tag, specialtyName((Specialty)k)); bad++; }
    }
  return bad;
}
double dst(int32_t ax, int32_t ay, int32_t bx, int32_t by) { return std::sqrt((double)(ax - bx) * (ax - bx) + (double)(ay - by) * (ay - by)); }

uint32_t rgb(int r, int g, int b) { return 0xFF000000u | ((uint32_t)std::clamp(b, 0, 255) << 16) | ((uint32_t)std::clamp(g, 0, 255) << 8) | (uint32_t)std::clamp(r, 0, 255); }
uint32_t scale(uint32_t c, int pct) { return rgb((int)(c & 255) * pct / 100, (int)((c >> 8) & 255) * pct / 100, (int)((c >> 16) & 255) * pct / 100); }
uint32_t mixc(uint32_t a, uint32_t b, int pctB) {
  auto ch = [&](int s) { return ((int)((a >> s) & 255) * (100 - pctB) + (int)((b >> s) & 255) * pctB) / 100; };
  return rgb(ch(0), ch(8), ch(16));
}
uint32_t biomeColor(Biome b) {
  switch (b) {
    case Biome::Ocean: return rgb(40, 70, 140);
    case Biome::Beach: return rgb(222, 206, 150);
    case Biome::Plains: return rgb(120, 172, 80);
    case Biome::Forest: return rgb(54, 116, 52);
    case Biome::Autumn: return rgb(176, 110, 46);
    case Biome::Taiga: return rgb(84, 116, 96);
    case Biome::Snow: return rgb(232, 236, 242);
    case Biome::Swamp: return rgb(86, 98, 62);
    case Biome::Desert: return rgb(220, 190, 120);
    case Biome::Mountain: return rgb(112, 108, 104);
    default: return rgb(255, 0, 255);
  }
}
uint32_t kingdomColor(EndlessSource& A, Gid k) {
  const KingdomPlan* kp = A.kingdom(k);
  if (!kp) return rgb(30, 30, 30);
  return kp->color | 0xFF000000u;
}

void drawLine(std::vector<uint32_t>& px, int W, int H, int ax, int ay, int bx, int by, uint32_t col, int thick) {
  int n = std::max(std::abs(bx - ax), std::abs(by - ay));
  for (int s = 0; s <= n; s++) {
    int x = ax + (n ? (bx - ax) * s / n : 0), y = ay + (n ? (by - ay) * s / n : 0);
    for (int oy = 0; oy < thick; oy++)
      for (int ox = 0; ox < thick; ox++)
        if (x + ox >= 0 && y + oy >= 0 && x + ox < W && y + oy < H) px[(size_t)(y + oy) * W + x + ox] = col;
  }
}
void dot(std::vector<uint32_t>& px, int W, int H, int x, int y, int r, uint32_t col) {
  for (int oy = -r; oy <= r; oy++)
    for (int ox = -r; ox <= r; ox++)
      if (x + ox >= 0 && y + oy >= 0 && x + ox < W && y + oy < H) px[(size_t)(y + oy) * W + x + ox] = col;
}

// the 2048^2 overview at 2 tiles/px
void overviewMap(EndlessSource& A, uint64_t seed, const char* dir, int RX0, int RY0, int NR, const std::map<Gid, SitePlan>& all,
                 const std::vector<DenPlan>& dens, const std::string& suffix, bool far) {
  const int T = 2, W = NR * REGION / T;
  const int32_t gx0 = RX0 * REGION, gy0 = RY0 * REGION;
  std::vector<uint32_t> px((size_t)W * W);
  std::vector<uint8_t> lv((size_t)W * W);
  std::vector<Gid> kg((size_t)W * W);
  std::vector<uint8_t> wet((size_t)W * W);
  for (int y = 0; y < W; y++)
    for (int x = 0; x < W; x++) {
      MacroSample m = A.macro(gx0 + x * T, gy0 + y * T);
      size_t i = (size_t)y * W + x;
      lv[i] = m.height;
      kg[i] = m.kingdom;
      wet[i] = m.water;
      uint32_t c = biomeColor(m.biome);
      if (m.water) c = m.biome == Biome::Ocean ? (m.elev < ELEV_SEA - 2300 ? rgb(30, 54, 116) : rgb(44, 84, 156)) : rgb(60, 120, 200);
      else c = scale(c, 88 + 5 * m.height);   // higher ground reads lighter
      px[i] = c;
    }
  for (int y = 1; y < W - 1; y++)
    for (int x = 1; x < W - 1; x++) {
      size_t i = (size_t)y * W + x;
      if (wet[i]) continue;
      if (lv[i - (size_t)W] > lv[i]) px[i] = scale(px[i], 55);          // a face toward the south: dark
      else if (lv[i - 1] > lv[i] || lv[i + 1] > lv[i]) px[i] = scale(px[i], 75);
      if ((kg[i] != kg[i + 1] || kg[i] != kg[i + (size_t)W]) && !wet[i + 1] && !wet[i + (size_t)W]) px[i] = rgb(140, 20, 20);
    }
  // roads (deduped by id) from every region plan
  std::set<Gid> seen;
  for (int ry = RY0; ry < RY0 + NR; ry++)
    for (int rx = RX0; rx < RX0 + NR; rx++) {
      const RegionPlan R = A.region(rx, ry);
      for (const RoadPlan& rp : R.roads) {
        if (!seen.insert(rp.id ^ ((uint64_t)rp.cls << 60) ^ rp.a * 31).second) continue;
        uint32_t col = rp.cls == 0 ? rgb(120, 70, 30) : rp.cls == 1 ? rgb(170, 120, 60) : rgb(210, 180, 120);
        for (size_t k = 0; k + 1 < rp.pts.size(); k++)
          drawLine(px, W, W, (rp.pts[k].x - gx0) / T, (rp.pts[k].y - gy0) / T, (rp.pts[k + 1].x - gx0) / T, (rp.pts[k + 1].y - gy0) / T, col,
                   rp.cls == 0 ? 2 : 1);
      }
    }
  for (const DenPlan& d : dens) dot(px, W, W, (d.x - gx0) / T, (d.y - gy0) / T, 0, rgb(90, 60, 60));
  for (auto& kv : all) {
    const SitePlan& p = kv.second;
    int x = (p.ex - gx0) / T, y = (p.ey - gy0) / T;
    if (isSettle(p.type)) {
      int hw = std::max(2, p.w / (2 * T)), hh = std::max(2, p.h / (2 * T));
      uint32_t kc = kingdomColor(A, p.kingdom);
      for (int oy = -hh; oy <= hh; oy++)
        for (int ox = -hw; ox <= hw; ox++) {
          if (x + ox < 0 || y + oy < 0 || x + ox >= W || y + oy >= W) continue;
          bool edge = std::abs(ox) == hw || std::abs(oy) == hh;
          px[(size_t)(y + oy) * W + x + ox] = edge ? ((p.flags & SPF_START) ? rgb(255, 0, 255) : (p.flags & SPF_CAPITAL) ? rgb(255, 230, 60) : rgb(20, 20, 20)) : mixc(kc, rgb(240, 230, 210), 40);
        }
    } else {
      uint32_t col = p.type == SiteType::Cave ? rgb(0, 0, 0) : p.type == SiteType::Ruin ? ((p.flags & SPF_MAINQUEST) ? rgb(255, 0, 255) : rgb(150, 60, 190))
                   : p.type == SiteType::BanditCamp ? rgb(230, 30, 30) : p.type == SiteType::Shrine ? rgb(255, 255, 255) : rgb(120, 0, 0);
      dot(px, W, W, x, y, p.type == SiteType::DragonLair ? 4 : (p.flags & SPF_MAINQUEST) ? 3 : 1, col);
    }
  }
  std::string path = std::string(dir) + "/endless_" + std::to_string(seed) + suffix + ".png";
  writePng(path.c_str(), W, W, px);
  if (!far) return;
  // continents: 16384^2 at 32 tiles/px
  const int FW = 512, FT = 32;
  std::vector<uint32_t> fp((size_t)FW * FW);
  std::vector<Gid> fk((size_t)FW * FW);
  for (int y = 0; y < FW; y++)
    for (int x = 0; x < FW; x++) {
      MacroSample m = A.macroFar((x - FW / 2) * FT, (y - FW / 2) * FT);
      uint32_t c = m.water ? (m.elev < ELEV_SEA - 4000 ? rgb(28, 48, 110) : rgb(44, 84, 156)) : scale(biomeColor(m.biome), 80 + 6 * m.height);
      fp[(size_t)y * FW + x] = c;
      fk[(size_t)y * FW + x] = m.water ? 1 : m.kingdom;
    }
  for (int y = 0; y < FW - 1; y++)
    for (int x = 0; x < FW - 1; x++) {
      size_t i = (size_t)y * FW + x;
      if (fk[i] != 1 && fk[i + 1] != 1 && fk[i + FW] != 1 && (fk[i] != fk[i + 1] || fk[i] != fk[i + FW])) fp[i] = rgb(140, 20, 20);
    }
  for (int k = -3; k <= 3; k++) { fp[(size_t)(FW / 2) * FW + FW / 2 + k] = rgb(255, 0, 255); fp[(size_t)(FW / 2 + k) * FW + FW / 2] = rgb(255, 0, 255); }
  path = std::string(dir) + "/endless_" + std::to_string(seed) + "_far.png";
  writePng(path.c_str(), FW, FW, fp);
}

bool g_budget = true;   // --no-budget clears it (shared CI runners)
int g_slivers = 0;      // relief slivers counted by reachWindow (target 0)

// ---- reachability over a 1024^2 window (VISION_PLAN 2.7, 11.4)
int reachWindow(EndlessSource& A, uint64_t seed, int32_t wx0, int32_t wy0, const std::map<Gid, SitePlan>& all, const char* mapDir,
                double& chunkAvg, double& chunkMax, int& walledPlateaus) {
  int bad = 0;
  auto fail = [&](const std::string& s) { out("FAIL: %s\n", s.c_str()); bad++; };
  const int N = 1024, NC = N / CHUNK;
  std::vector<uint8_t> g((size_t)N * N), walk((size_t)N * N), hb((size_t)N * N), road((size_t)N * N);
  ChunkData c;
  double sum = 0, mx = 0;
  int nGen = 0, slow = 0;
  for (int cy = 0; cy < NC; cy++)
    for (int cx = 0; cx < NC; cx++) {
      const EndlessSource::Stats s0 = A.stats();
      A.chunk((wx0 >> 5) + cx, (wy0 >> 5) + cy, c);
      const EndlessSource::Stats& s1 = A.stats();
      double ms = s1.chunkMs - s0.chunkMs;
      sum += ms; mx = std::max(mx, ms); nGen++;
      if (ms > 4.0) { out("  slow chunk (%d,%d): %.1f ms\n", (wx0 >> 5) + cx, (wy0 >> 5) + cy, ms); slow++; }
      for (int ly = 0; ly < CHUNK; ly++)
        for (int lx = 0; lx < CHUNK; lx++) {
          int i = c.at(lx, ly);
          size_t k = (size_t)(cy * CHUNK + ly) * N + cx * CHUNK + lx;
          g[k] = c.ground[i];
          hb[k] = c.height[i];
          bool solid = groundSolid((Ground)c.ground[i]) || c.wall[i] || (c.height[i] & Map::HEIGHT_CLIFF) ||
                       (c.prop[i] && propSolid((art::Prop)(c.prop[i] - 1)));
          if (c.bldg[i]) {
            const Bldg& b = c.bldgs[(size_t)c.bldg[i] - 1];
            int gxx = (wx0 + cx * CHUNK + lx), gyy = (wy0 + cy * CHUNK + ly);
            solid = solid || !(gxx == b.doorX() && gyy == b.doorY());
          }
          walk[k] = !solid;
          road[k] = c.ground[i] == (uint8_t)Ground::Road || c.ground[i] == (uint8_t)Ground::Bridge;
          // (a stilt town's boardwalks wind over its marsh by design: not road bridges)
          if (c.ground[i] == (uint8_t)Ground::Bridge && (c.blend[i] >> 4) == (Map::BOARDWALK_MARK >> 4)) g[k] = (uint8_t)Ground::Swamp;
        }
    }
  chunkAvg = nGen ? sum / nGen : 0;
  chunkMax = mx;
  // a single slow chunk is usually the OS (other processes); a pattern of them is the generator
  if (g_budget && slow > 2) fail(std::to_string(slow) + " chunks over the 4 ms budget (worst " + std::to_string(mx) + " ms)");
  // flood from every road tile
  std::vector<uint8_t> reach((size_t)N * N, 0);
  std::vector<int> q;
  q.reserve((size_t)N * N / 4);
  for (size_t k = 0; k < (size_t)N * N; k++) if (road[k] && walk[k]) { reach[k] = 1; q.push_back((int)k); }
  for (size_t h = 0; h < q.size(); h++) {
    int k = q[h], x = k % N, y = k / N;
    const int nb[4] = {x > 0 ? k - 1 : -1, x < N - 1 ? k + 1 : -1, y > 0 ? k - N : -1, y < N - 1 ? k + N : -1};
    for (int n : nb) if (n >= 0 && walk[(size_t)n] && !reach[(size_t)n]) { reach[(size_t)n] = 1; q.push_back(n); }
  }
  auto reached = [&](int32_t gx0, int32_t gy0, int32_t gx1, int32_t gy1) {
    for (int32_t y = gy0; y < gy1; y++)
      for (int32_t x = gx0; x < gx1; x++) {
        int32_t lx = x - wx0, ly = y - wy0;
        if (lx >= 0 && ly >= 0 && lx < N && ly < N && reach[(size_t)ly * N + lx]) return true;
      }
    return false;
  };
  auto roadNear = [&](int32_t gx0, int32_t gy0, int32_t gx1, int32_t gy1) {
    for (int32_t y = gy0; y < gy1; y++)
      for (int32_t x = gx0; x < gx1; x++) {
        int32_t lx = x - wx0, ly = y - wy0;
        if (lx >= 0 && ly >= 0 && lx < N && ly < N && road[(size_t)ly * N + lx]) return true;
      }
    return false;
  };
  int nSet = 0, noRoad = 0, nPoi = 0, poiMiss = 0;
  for (auto& kv : all) {
    const SitePlan& p = kv.second;
    if (p.ex < wx0 + 128 || p.ey < wy0 + 128 || p.ex >= wx0 + N - 128 || p.ey >= wy0 + N - 128) continue;
    if (isSettle(p.type)) {
      nSet++;
      if (!roadNear(p.gx - 60, p.gy - 60, p.gx + p.w + 60, p.gy + p.h + 60)) { noRoad++; continue; }
      if (!reached(p.gx, p.gy, p.gx + p.w, p.gy + p.h)) fail("settlement " + p.name + " is not reachable from the roads");
    } else {
      nPoi++;
      bool ok = p.type == SiteType::Cave ? reached(p.ex, p.ey, p.ex + 1, p.ey + 2) : reached(p.gx - 1, p.gy - 1, p.gx + p.w + 1, p.gy + p.h + 1);
      bool story = (p.flags & SPF_MAINQUEST) || p.type == SiteType::DragonLair;
      // (M2) the wayside places and the wonders are walked to from the road
      const bool wayside = p.type == SiteType::Vignette || p.type == SiteType::Wonder;
      if (!ok) {
        poiMiss++;
        if (story) fail(std::string(siteTypeName(p.type)) + " " + p.name + " (story site) is not reachable from the roads");
        else if (wayside) fail(std::string(poiKindName(p.type, p.kind)) + " " + p.name + " is not reachable from the roads");
      }
    }
  }
  // walled-in plateaus: unreached walkable components of 24+ tiles bounded only by cliffs (no water, no window edge)
  std::vector<int> comp((size_t)N * N, 0);
  int plateaus = 0, islands = 0, pockets = 0;
  int cid = 0;
  std::vector<int> st;
  for (size_t s0 = 0; s0 < (size_t)N * N; s0++) {
    if (!walk[s0] || reach[s0] || comp[s0]) continue;
    cid++;
    st.clear();
    st.push_back((int)s0);
    comp[s0] = cid;
    int size = 0, cliffB = 0, otherB = 0;
    bool edge = false, water = false;
    for (size_t h = 0; h < st.size(); h++) {
      int k = st[h], x = k % N, y = k / N;
      size++;
      if (x == 0 || y == 0 || x == N - 1 || y == N - 1) edge = true;
      const int nb[4] = {x > 0 ? k - 1 : -1, x < N - 1 ? k + 1 : -1, y > 0 ? k - N : -1, y < N - 1 ? k + N : -1};
      for (int n : nb) {
        if (n < 0) continue;
        if (walk[(size_t)n]) { if (!comp[(size_t)n]) { comp[(size_t)n] = cid; st.push_back(n); } continue; }
        if (groundWater((Ground)g[(size_t)n])) water = true;
        else if (hb[(size_t)n] & Map::HEIGHT_CLIFF) cliffB++;
        else otherB++;
      }
    }
    if (size < 24 || edge) continue;
    if (water) islands++;
    else if (cliffB >= 9 * otherB) {   // trees never touch a cliff, so a true walled plateau has an all-cliff rim
      plateaus++;
      int k0 = st.front();
      out("  walled plateau: %d tiles near (%d,%d), %d cliff / %d other boundary tiles\n", size, wx0 + k0 % N, wy0 + k0 / N, cliffB, otherB);
    } else pockets++;
  }
  walledPlateaus = plateaus;
  // (M3b fixer) bridges run straight: a road's deck over a pond or a lake never jogs a tile sideways mid-deck. A deck
  // (4-connected Bridge tiles) whose rows (or columns) are not all the same span is a jog; the diagonal decks of a
  // slanting road over a river (a staircase both ways at once) are their own look and are left out (both spans vary
  // by more than a tile)
  {
    std::vector<uint8_t> seen((size_t)N * N, 0);
    int decks = 0, jogs = 0;
    std::vector<int> st2;
    for (size_t s0 = 0; s0 < (size_t)N * N; s0++) {
      if (seen[s0] || g[s0] != (uint8_t)Ground::Bridge) continue;
      st2.clear(); st2.push_back((int)s0); seen[s0] = 1;
      int x0 = N, y0 = N, x1 = -1, y1 = -1;
      for (size_t h = 0; h < st2.size(); h++) {
        const int k = st2[h], x = k % N, y = k / N;
        x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y);
        const int nb[4] = {x > 0 ? k - 1 : -1, x < N - 1 ? k + 1 : -1, y > 0 ? k - N : -1, y < N - 1 ? k + N : -1};
        for (int n : nb) if (n >= 0 && !seen[(size_t)n] && g[(size_t)n] == (uint8_t)Ground::Bridge) { seen[(size_t)n] = 1; st2.push_back(n); }
      }
      if (st2.size() < 4) continue;
      decks++;
      const int bw = x1 - x0 + 1, bh = y1 - y0 + 1;
      if (bw <= 2 || bh <= 2) continue;   // a straight deck (one or two tiles wide)
      // the span of each row and column of the deck
      int rowVar = 0, colVar = 0;
      std::vector<int> rmin((size_t)bh, N), rmax((size_t)bh, -1), cmin((size_t)bw, N), cmax((size_t)bw, -1);
      for (int k : st2) {
        const int x = k % N - x0, y = k / N - y0;
        rmin[(size_t)y] = std::min(rmin[(size_t)y], x); rmax[(size_t)y] = std::max(rmax[(size_t)y], x);
        cmin[(size_t)x] = std::min(cmin[(size_t)x], y); cmax[(size_t)x] = std::max(cmax[(size_t)x], y);
      }
      for (int y = 1; y < bh; y++) if (rmin[(size_t)y] != rmin[0] || rmax[(size_t)y] != rmax[0]) rowVar++;
      for (int x = 1; x < bw; x++) if (cmin[(size_t)x] != cmin[0] || cmax[(size_t)x] != cmax[0]) colVar++;
      // a deck the view draws as one slanting span (terrain.cpp diagBridgePixel: its tiles spread along a diagonal) is
      // a diagonal crossing, not a jog
      double mx = 0, my = 0;
      for (int k : st2) { mx += k % N; my += k / N; }
      mx /= (double)st2.size(); my /= (double)st2.size();
      double sxx = 0, syy = 0, sxy = 0;
      for (int k : st2) { const double ddx = k % N - mx, ddy = k / N - my; sxx += ddx * ddx; syy += ddy * ddy; sxy += ddx * ddy; }
      const bool diagonal = std::fabs(sxy) >= 0.55 * std::sqrt(sxx * syy) && std::min(sxx, syy) >= 0.35 * std::max(sxx, syy);
      // a long deck one way (3+ tiles) whose cross-section steps: a jog
      const bool horiz = bw >= bh, jog = !diagonal && (horiz ? (bw >= 4 && bh == 3) : (bh >= 4 && bw == 3));
      if (jog) {
        jogs++;
        if (jogs <= 4) out("  bridge jog: a %dx%d deck at (%d,%d)\n", bw, bh, wx0 + x0, wy0 + y0);
      }
      (void)rowVar; (void)colVar;
    }
    printf("  bridges: %d decks, %d jogging a tile mid-deck%s\n", decks, jogs, jogs ? " (FAIL)" : "");
    if (jogs) fail(std::to_string(jogs) + " road bridges jog a tile mid-deck");
  }
  if (plateaus > 0) fail(std::to_string(plateaus) + " plateaus of 24+ tiles are walled in by cliffs (no ramp)");
  // (M2, owner note 3) relief slivers on open ground (land, no water beside it): a 1-tile-deep terrace (a level band
  // one tile thick between a higher and a lower one), a 1-tile strip (a ridge or a channel one tile wide), or a spur /
  // notch (a tile with three or four of its neighbours on one other side: islands, pits, 1-wide fingers)
  // (the terraces round a settlement or a site, where the land is levelled for it, are not open ground)
  {
    std::vector<uint8_t> near((size_t)N * N, 0);
    for (auto& kv : all) {
      const SitePlan& p = kv.second;
      const int32_t pad = isSettle(p.type) ? 70 : 40;
      for (int32_t y = std::max(0, p.gy - pad - wy0); y < std::min(N, p.gy + p.h + pad - wy0); y++)
        for (int32_t x = std::max(0, p.gx - pad - wx0); x < std::min(N, p.gx + p.w + pad - wx0); x++) near[(size_t)y * N + x] = 1;
    }
    int terr = 0, strip = 0, spur = 0, sx = 0, sy = 0;
    auto lvA = [&](int x, int y) { return (int)(hb[(size_t)y * N + x] & Map::HEIGHT_LEVEL); };
    auto wetA = [&](int x, int y) { return groundWater((Ground)g[(size_t)y * N + x]) || g[(size_t)y * N + x] == (uint8_t)Ground::Bridge; };
    for (int y = 1; y < N - 1; y++)
      for (int x = 1; x < N - 1; x++) {
        if (near[(size_t)y * N + x] || wetA(x, y) || wetA(x, y - 1) || wetA(x, y + 1) || wetA(x - 1, y) || wetA(x + 1, y)) continue;
        const int l = lvA(x, y), n = lvA(x, y - 1), s = lvA(x, y + 1), w = lvA(x - 1, y), e = lvA(x + 1, y);
        const int hi = (n > l) + (s > l) + (w > l) + (e > l), lo = (n < l) + (s < l) + (w < l) + (e < l);
        bool hit = true;
        if ((n > l && s < l) || (n < l && s > l) || (w > l && e < l) || (w < l && e > l)) terr++;
        else if (hi >= 3 || lo >= 3) spur++;
        else if ((n > l && s > l) || (n < l && s < l) || (w > l && e > l) || (w < l && e < l)) strip++;
        else hit = false;
        if (hit && !sx) { sx = wx0 + x; sy = wy0 + y; }
      }
    g_slivers += terr + strip + spur;
    // (three cleanup passes leave a handful per million tiles: long 1-wide snakes the passes shorten from both ends)
    if (terr + strip + spur > 8) fail(std::to_string(terr + strip + spur) + " relief slivers in the window (target 0, at most 8)");
    out("  relief slivers: %d 1-tile terraces, %d 1-tile strips, %d spurs / notches / islands%s\n", terr, strip, spur,
        terr + strip + spur ? (" (first at " + std::to_string(sx) + "," + std::to_string(sy) + ")").c_str() : "");
  }
  out("  reach (1024^2 at %d,%d): settlements %d (%d without roads), sites %d (%d unreached); unreached pockets: %d cliff-walled, %d islands, %d other\n",
      wx0, wy0, nSet, noRoad, nPoi, poiMiss, plateaus, islands, pockets);
  if (mapDir) {
    std::vector<uint32_t> px((size_t)N * N);
    for (size_t k = 0; k < (size_t)N * N; k++) {
      uint32_t col = groundColor((Ground)g[k]);
      col = scale(col, 86 + 5 * (hb[k] & 7));
      if (hb[k] & Map::HEIGHT_CLIFF) col = scale(col, 45);
      if (hb[k] & Map::HEIGHT_RAMP) col = rgb(250, 210, 60);
      if (walk[k] && !reach[k]) col = mixc(col, rgb(255, 0, 0), 45);
      px[k] = col;
    }
    std::string path = std::string(mapDir) + "/endless_" + std::to_string(seed) + "_window.png";
    writePng(path.c_str(), N, N, px);
  }
  return bad;
}

bool g_mapAt = false;
int32_t g_mapX = 0, g_mapY = 0;

}  // namespace
// test_wayfinder.cpp: the M2 start guarantee on the region plans
int startGuarantee(ew::EndlessSource& A, int& near120, int& near60, int& roadPois, double& wonderD, std::string& list);
namespace {

int endlessSeed(uint64_t seed, const char* mapDir, bool quick, bool budget) {
  int bad = 0;
  auto fail = [&](const std::string& s) { out("FAIL: %s\n", s.c_str()); bad++; };
  EndlessSource A(seed);
  auto tStart = std::chrono::steady_clock::now();
  const StartPlan& sp = A.start();
  double startMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tStart).count();
  std::map<Gid, SitePlan> all;
  std::vector<DenPlan> dens;
  const int R0 = -4, R1 = 3;   // regions -4..3: 2048 x 2048 tiles around the origin
  for (int ry = R0; ry <= R1; ry++)
    for (int rx = R0; rx <= R1; rx++) {
      const RegionPlan& R = A.region(rx, ry);
      for (const SitePlan& p : R.sites) all[p.id] = p;
      for (const DenPlan& d : R.dens) dens.push_back(d);
    }
  auto find = [&](Gid id) -> const SitePlan* {
    if (!id) return nullptr;
    auto it = all.find(id);
    if (it != all.end()) return &it->second;
    for (const SitePlan& p : A.region(idRx(id), idRy(id)).sites) if (p.id == id) { all[id] = p; return &all[id]; }
    return nullptr;
  };
  // ---- the start plan
  const SitePlan* home = find(sp.village);
  const SitePlan* cap = find(sp.capital);
  const SitePlan* lair = find(sp.lair);
  if (!home || home->type != SiteType::Village || !(home->flags & SPF_START)) fail("no start village");
  if (!cap || cap->type != SiteType::City || !(cap->flags & SPF_CAPITAL) || !(cap->flags & SPF_STORY)) fail("no story city / capital");
  if (!lair || lair->type != SiteType::DragonLair) fail("no dragon lair");
  auto dist = [](const SitePlan* a, const SitePlan* b) { return dst(a->ex, a->ey, b->ex, b->ey); };
  if (home) {
    MacroSample m = A.macro(home->ex, home->ey);
    if (m.biome != Biome::Plains && m.biome != Biome::Forest && m.biome != Biome::Autumn) fail(std::string("start village in ") + biomeName(m.biome));
    for (int k = 0; k < 32; k++) {
      double a = k * 6.2831853 / 32;
      for (int r : {40, 80}) {
        MacroSample s = A.macro(home->ex + (int)(std::cos(a) * r), home->ey + (int)(std::sin(a) * r));
        if (s.water && s.biome == Biome::Ocean) { fail("the sea within 80 tiles of the start village"); k = 99; break; }
      }
    }
  }
  if (home && cap && (dist(home, cap) < 150 || dist(home, cap) > 300)) fail("story city " + std::to_string((int)dist(home, cap)) + " tiles from the start");
  if (home && lair && (dist(home, lair) < 400 || dist(home, lair) > 650)) fail("lair " + std::to_string((int)dist(home, lair)) + " tiles from the start");
  int lastLv = 0;
  for (Gid g : sp.shards) {
    const SitePlan* s = find(g);
    if (!s || s->type != SiteType::Ruin || !(s->flags & SPF_MAINQUEST)) { fail("missing shard ruin"); continue; }
    if (home && (dist(home, s) < 180 || dist(home, s) > 450)) fail("shard ruin " + std::to_string((int)dist(home, s)) + " tiles from the start");
    if (s->level < lastLv) fail("shard ruins do not rise in danger");
    lastLv = s->level;
  }
  std::set<int> kinds;
  if (home) {
    for (auto& kv : all) if (kv.first != home->id && dst(kv.second.ex, kv.second.ey, home->ex, home->ey) <= 200) kinds.insert((int)kv.second.type);
    for (const DenPlan& d : dens) if (dst(d.x, d.y, home->ex, home->ey) <= 200) { kinds.insert(100); break; }
    if (kinds.size() < 6) {
      std::string l;
      for (int k : kinds) l += std::string(" ") + (k == 100 ? "den" : siteTypeName((SiteType)k));
      fail("only " + std::to_string(kinds.size()) + " POI kinds within 200 tiles of the start:" + l);
    }
  }
  // ---- (M2) the start guarantee (VISION_PLAN 2.6): kinds of place near the start village, places by the story road, a wonder
  {
    int n120 = 0, n60 = 0, road = 0;
    double wd = 0;
    std::string kl;
    bad += startGuarantee(A, n120, n60, road, wd, kl);
    out("  start guarantee: %d kinds within 120 (%s), %d within 60, %d by the story road, wonder %.0f tiles away\n", n120, kl.c_str(), n60, road, wd);
  }
  // ---- order independence: near the start, and far away (1e5 tiles)
  for (int far = 0; far < 2; far++) {
    const int32_t c0x = far ? 3125 : chunkOf(sp.spawn.x) - 3, c0y = far ? -3125 : chunkOf(sp.spawn.y) - 3;
    const int K = far ? 4 : 6;
    std::vector<uint64_t> fwd, back((size_t)K * K);
    ChunkData c;
    for (int j = 0; j < K; j++) for (int i = 0; i < K; i++) { A.chunk(c0x + i, c0y + j, c); fwd.push_back(chunkHash(c)); }
    EndlessSource B(seed);
    B.region(40, -40);
    B.chunk(1000, 1000, c);
    for (int j = K - 1; j >= 0; j--) for (int i = K - 1; i >= 0; i--) { B.chunk(c0x + i, c0y + j, c); back[(size_t)(j * K + i)] = chunkHash(c); }
    int diff = 0;
    for (size_t k = 0; k < fwd.size(); k++) if (fwd[k] != back[k]) diff++;
    if (diff) fail(std::to_string(diff) + " of " + std::to_string(K * K) + (far ? " far" : "") + " chunks differ with generation order (order-independence broken)");
  }
  // ---- spacing, kingdoms, names
  std::vector<const SitePlan*> settle;
  int nCity = 0, nTown = 0, nVillage = 0, nCap = 0, wild = 0, other = 0;
  for (auto& kv : all) {
    const SitePlan& p = kv.second;
    if (p.ex < R0 * REGION || p.ey < R0 * REGION || p.ex >= (R1 + 1) * REGION || p.ey >= (R1 + 1) * REGION) continue;
    if (!isSettle(p.type)) { other++; continue; }
    settle.push_back(&p);
    tallySpecialties(p);
    if (p.type == SiteType::City) {
      nCity++;
      if (!(p.flags & SPF_CAPITAL)) fail("city " + p.name + " is not a capital");
      const KingdomPlan* kp = A.kingdom(p.kingdom);
      if (!kp || kp->capital != p.id) fail("city " + p.name + " is not its kingdom's capital");
    } else if (p.type == SiteType::Town) nTown++;
    else nVillage++;
    if (p.flags & SPF_CAPITAL) nCap++;
    if (!p.kingdom) wild++;
    else if (!A.kingdom(p.kingdom)) fail("settlement " + p.name + " has an unknown kingdom");
  }
  std::vector<double> nns;
  double minCity = 1e9;
  for (const SitePlan* a : settle) {
    double nn = 1e9;
    for (const SitePlan* b : settle) {
      if (a == b) continue;
      double d = dist(a, b);
      nn = std::min(nn, d);
      if (a->type == SiteType::City && b->type == SiteType::City) minCity = std::min(minCity, d);
      if (d < 768 && a->name == b->name && a->id < b->id) fail("two settlements called " + a->name + " within 3 regions");
    }
    nns.push_back(nn);
  }
  std::sort(nns.begin(), nns.end());
  double minNN = nns.empty() ? 0 : nns.front(), medNN = nns.empty() ? 0 : nns[nns.size() / 2];
  if (minNN < 150) fail("two settlements only " + std::to_string((int)minNN) + " tiles apart (target >= 150)");
  if (medNN < 220) fail("median nearest-neighbour spacing " + std::to_string((int)medNN) + " (target >= 220)");
  if (minCity < 590) fail("two cities only " + std::to_string((int)minCity) + " tiles apart (target ~600)");
  // ---- land: rock share, relief
  int land = 0, rock = 0, lvHist[8] = {};
  for (int y = R0 * REGION; y < (R1 + 1) * REGION; y += 16)
    for (int x = R0 * REGION; x < (R1 + 1) * REGION; x += 16) {
      MacroSample m = A.macro(x, y);
      if (m.water) continue;
      land++;
      if (m.biome == Biome::Mountain) rock++;
      lvHist[m.height & 7]++;
    }
  double rockPct = land ? 100.0 * rock / land : 0;
  if (rockPct > 10.0) fail("rock is " + std::to_string((int)rockPct) + " % of land (max 10)");
  // ---- roads touching the area
  int nRoads = 0, nSpurs = 0;
  {
    std::set<Gid> seen;
    for (int ry = R0; ry <= R1; ry++)
      for (int rx = R0; rx <= R1; rx++)
        for (const RoadPlan& rp : A.region(rx, ry).roads)
          if (seen.insert(rp.id ^ ((uint64_t)rp.cls << 60) ^ rp.a * 31).second) (rp.cls == 2 ? nSpurs : nRoads)++;
  }
  const EndlessSource::Stats st = A.stats();
  double farUs = 0;
  {
    auto t0 = std::chrono::steady_clock::now();
    uint64_t acc = 0;
    for (int k = 0; k < 4096; k++) acc += (uint64_t)A.macroFar(50000 + (k & 63) * 8, 50000 + (k >> 6) * 8).elev;
    farUs = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / 4096 + (double)(acc & 0) ;
  }
  out("seed %llu: start (%d,%d)  settlements %zu in 2048^2 (cities %d, towns %d, villages %d, capitals %d, wild %d), other sites %d, dens %zu\n",
      (unsigned long long)seed, sp.spawn.x, sp.spawn.y, settle.size(), nCity, nTown, nVillage, nCap, wild, other, dens.size());
  out("  spacing: min %.0f median %.0f, cities >= %.0f | story city %.0f, lair %.0f | POI kinds near start %zu | roads %d, spurs %d | rock %.1f %% of land, levels",
      minNN, medNN, minCity > 1e8 ? 0.0 : minCity, home && cap ? dist(home, cap) : 0.0, home && lair ? dist(home, lair) : 0.0, kinds.size(), nRoads, nSpurs, rockPct);
  for (int k = 0; k < 8; k++) out(" %d", land ? lvHist[k] * 100 / land : 0);
  out("\n  cost: macro sample %.2f us | blocks %d avg %.2f ms | start plan %.1f ms | regions %d avg %.2f ms max %.1f | traces %d (%.1f ms; %d rivers, %d to the sea, %d to lakes) | road edges %d avg %.2f ms max %.1f | settlements %d avg %.1f ms max %.1f\n",
      farUs, st.blocks, st.blocks ? st.blockMs / st.blocks : 0.0, startMs, st.regions, st.regions ? st.regionMs / st.regions : 0.0, st.maxRegionMs, st.traces, st.traceMs, st.rivers, st.tracesToSea, st.tracesToLake, st.roadEdges,
      st.roadEdges ? st.roadMs / st.roadEdges : 0.0, st.maxRoadMs, st.settlements, st.settlements ? st.settlementMs / st.settlements : 0.0, st.maxSettlementMs);
  if (budget && st.regions && st.regionMs / st.regions > 3.0) fail("region plans average " + std::to_string(st.regionMs / st.regions) + " ms (budget 3)");
  // ---- reachability and chunk cost over a 1024^2 window
  if (!quick) {
    double cAvg = 0, cMax = 0;
    int walled = 0;
    const int32_t wx0 = (chunkOf(sp.spawn.x) - 16) * CHUNK, wy0 = (chunkOf(sp.spawn.y) - 16) * CHUNK;
    bad += reachWindow(A, seed, wx0, wy0, all, mapDir, cAvg, cMax, walled);
    out("  chunks (1024 in the window): avg %.2f ms max %.2f\n", cAvg, cMax);
    if (budget && cAvg > 1.0) fail("chunks average " + std::to_string(cAvg) + " ms (budget 1)");
    // a second window far out on another stretch of land (a sampled place away from the start plan)
    int32_t fx = 0, fy = 0;
    bool land = false;
    for (int k = 0; k < 3000 && !land; k++) {
      int32_t x = 9000 + (int32_t)((seed * 1531) % 7000) + (k % 50) * 512, y = -8000 + (k / 50) * 512;
      MacroSample m = A.macroFar(x, y);
      if (!m.water && m.elev > ELEV_SEA + 3000 && !A.macroFar(x + 300, y).water && !A.macroFar(x, y + 300).water) { land = true; fx = x; fy = y; }
    }
    if (land) {
      std::map<Gid, SitePlan> far;
      for (int ry = regionOf(fy) - 3; ry <= regionOf(fy) + 2; ry++)
        for (int rx = regionOf(fx) - 3; rx <= regionOf(fx) + 2; rx++)
          for (const SitePlan& p : A.region(rx, ry).sites) far[p.id] = p;
      bad += reachWindow(A, seed, (chunkOf(fx) - 16) * CHUNK, (chunkOf(fy) - 16) * CHUNK, far, nullptr, cAvg, cMax, walled);
      out("  chunks (far window): avg %.2f ms max %.2f\n", cAvg, cMax);
      if (budget && cAvg > 1.0) fail("chunks average " + std::to_string(cAvg) + " ms (budget 1)");
    }
  }
  if (mapDir) overviewMap(A, seed, mapDir, R0, R0, R1 - R0 + 1, all, dens, "", true);
  if (mapDir && g_mapAt) {
    // an extra overview elsewhere (coasts, far lands): rpg_test --endless --map dir --map-at X,Y
    std::map<Gid, SitePlan> s2;
    std::vector<DenPlan> d2;
    const int rx0 = regionOf(g_mapX) - 4, ry0 = regionOf(g_mapY) - 4;
    for (int ry = ry0; ry < ry0 + 8; ry++)
      for (int rx = rx0; rx < rx0 + 8; rx++) {
        const RegionPlan& R = A.region(rx, ry);
        for (const SitePlan& p : R.sites) s2[p.id] = p;
        for (const DenPlan& d : R.dens) d2.push_back(d);
      }
    overviewMap(A, seed, mapDir, rx0, ry0, 8, s2, d2, "_at", false);
  }
  return bad;
}

// ---- golden fixtures (owner rule D: cross-platform determinism)
struct Fnv {
  uint64_t h = 1469598103934665603ull;
  void add(uint64_t v) { for (int i = 0; i < 8; i++) { h ^= (uint8_t)(v >> (i * 8)); h *= 1099511628211ull; } }
  void str(const std::string& s) { for (char ch : s) { h ^= (uint8_t)ch; h *= 1099511628211ull; } add(s.size()); }
};

std::map<std::string, uint64_t> computeEndlessGolden() {
  std::map<std::string, uint64_t> g;
  for (uint64_t seed : {1ull, 7ull, 12345ull}) {
    EndlessSource A(seed);
    const StartPlan& sp = A.start();
    const std::string S = std::to_string(seed);
    Fnv fs;
    fs.add(sp.village); fs.add(sp.capital); fs.add(sp.lair);
    for (Gid x : sp.shards) fs.add(x);
    fs.add((uint64_t)(uint32_t)sp.spawn.x); fs.add((uint64_t)(uint32_t)sp.spawn.y);
    g[S + ".start"] = fs.h;

    // places: the start, and two far places (about +-1e5 tiles)
    // far places: the first land found walking a fixed lattice out from (+-1e5, +-1e5)
    int32_t places[3][2] = {{regionOf(sp.spawn.x), regionOf(sp.spawn.y)}, {390, 391}, {-391, -390}};
    for (int pl = 1; pl < 3; pl++) {
      const int32_t sgn = pl == 1 ? 1 : -1;
      for (int k = 0; k < 4000; k++) {
        int32_t x = sgn * 100000 + (k % 60) * 640 * sgn, y = sgn * 100000 + (k / 60) * 640 * sgn;
        MacroSample m = A.macroFar(x, y);
        if (!m.water && m.elev > ELEV_SEA + 3000 && !A.macroFar(x + 256, y).water && !A.macroFar(x, y + 256).water) {
          places[pl][0] = regionOf(x); places[pl][1] = regionOf(y);
          break;
        }
      }
    }
    const char* pname[3] = {"near", "farpos", "farneg"};
    g[S + ".places"] = ((uint64_t)(uint32_t)places[1][0] << 48) ^ ((uint64_t)(uint32_t)places[1][1] << 32) ^ ((uint64_t)(uint32_t)places[2][0] << 16) ^ (uint64_t)(uint32_t)places[2][1];
    for (int pl = 0; pl < 3; pl++) {
      Fnv fr, fm;
      std::vector<std::pair<int32_t, int32_t>> masks;   // settlement hearts and their fixed mask radii
      std::vector<int32_t> maskR;
      std::set<Gid> settlementIds;
      for (int dy = -2; dy <= 2; dy++)
        for (int dx = -2; dx <= 2; dx++) {
          const RegionPlan R = A.region(places[pl][0] + dx, places[pl][1] + dy);
          for (const SitePlan& p : R.sites) {
            if (isSettle(p.type)) {
              masks.push_back({p.ex, p.ey});
              maskR.push_back(p.type == SiteType::City ? 200 : p.type == SiteType::Town ? 150 : 125);
              settlementIds.insert(p.id);
            }
            if (dx || dy) continue;
            // footprints (w, h, gx, gy) and building ranges belong to the towns lane: not pinned here
            fr.add(p.id); fr.add((uint64_t)p.type); fr.add((uint64_t)(uint32_t)p.ex); fr.add((uint64_t)(uint32_t)p.ey);
            fr.add(p.flags); fr.add((uint64_t)p.archetype); fr.add((uint64_t)p.level); fr.add(p.kingdom); fr.add(p.seed);
            fr.add((uint64_t)p.theme); fr.str(p.name);
          }
          if (dx || dy) continue;
          for (const DenPlan& d : R.dens) { fr.add(d.id); fr.add((uint64_t)(uint32_t)d.x); fr.add((uint64_t)(uint32_t)d.y); fr.add((uint64_t)d.mon); fr.add(d.pack); }
          for (const RoadPlan& rp : R.roads) {
            fr.add(rp.id); fr.add(rp.cls); fr.add(rp.a); fr.add(rp.b);
            for (const GTile& t : rp.pts) fr.add(((uint64_t)(uint32_t)t.x << 32) | (uint32_t)t.y);
          }
          for (const RiverPlan& rv : R.rivers) { fr.add(((uint64_t)(uint32_t)rv.a.x << 32) | (uint32_t)rv.a.y); fr.add(((uint64_t)(uint32_t)rv.b.x << 32) | (uint32_t)rv.b.y); fr.add(rv.width); }
          for (const LakePlan& lk : R.lakes) { fr.add((uint64_t)(uint32_t)lk.x); fr.add((uint64_t)(uint32_t)lk.y); fr.add((uint64_t)lk.r); }
          // (M2) the wayside places' kinds, the landmarks and the geology
          for (const SitePlan& p : R.sites) fr.add(p.kind);
          for (const LandmarkPlan& l : R.landmarks) { fr.add(l.id); fr.add((uint64_t)l.kind); fr.add((uint64_t)(uint32_t)l.x ^ ((uint64_t)(uint32_t)l.y << 32)); fr.str(l.name); }
          fr.add((uint64_t)R.geology.rock); fr.add(R.geology.province);
          for (int o = 0; o < (int)Ore::COUNT; o++) fr.add(R.geology.ore[o]);
        }
      g[S + "." + pname[pl] + ".region"] = fr.h;
      // chunks across the region: settlement interiors masked out
      ChunkData c;
      uint64_t hashed = 0;
      for (int k = 0; k < 16; k++) {
        int32_t cx = places[pl][0] * 8 - 8 + (k * 5) % 24, cy = places[pl][1] * 8 - 8 + (k * 11) % 24;
        A.chunk(cx, cy, c);
        for (int ly = 0; ly < CHUNK; ly++)
          for (int lx = 0; lx < CHUNK; lx++) {
            int32_t x = cx * CHUNK + lx, y = cy * CHUNK + ly;
            bool masked = false;
            for (size_t m = 0; m < masks.size() && !masked; m++)
              if (std::abs(x - masks[m].first) <= maskR[m] && std::abs(y - masks[m].second) <= maskR[m]) masked = true;
            if (masked) continue;
            int i = c.at(lx, ly);
            fm.add((uint64_t)c.ground[i] | ((uint64_t)c.prop[i] << 8) | ((uint64_t)c.biome[i] << 16) | ((uint64_t)c.height[i] << 24) |
                   ((uint64_t)(uint32_t)i << 32) | ((uint64_t)c.blend[i] << 48));   // (M2: the ecotone byte too)
            hashed++;
          }
        for (const SpawnPlan& s : c.spawns)
          if (!settlementIds.count(s.siteId)) { fm.add(s.siteId); fm.add((uint64_t)(uint32_t)s.sp.x ^ ((uint64_t)(uint32_t)s.sp.y << 32)); }
      }
      fm.add(hashed);
      g[S + "." + pname[pl] + ".chunks"] = fm.h;
      g[S + "." + pname[pl] + ".tiles"] = hashed;
    }
    // (M3) the culture engine: the families of a 4 x 4 block of culture cells near the origin and far out, the dialects
    // and arms of the kingdoms round the start, and a few buildings' architecture (integer / Q16 maths only: the
    // maximin must pick the same cultures on every platform)
    {
      Fnv fc;
      cult::Atlas& at = A.atlas();
      auto addCulture = [&](const cult::Culture& c) {
        fc.add(c.id); fc.add(c.seed);
        fc.add((uint64_t)c.archetype | (uint64_t)c.archetype2 << 8 | (uint64_t)c.isolated << 16 | (uint64_t)c.homeBiome << 24);
        fc.str(c.name); fc.str(c.adjective);
        fc.add(c.arch.key()); fc.add((uint64_t)c.altRoof); fc.add(c.props.key()); fc.add(c.music.pack()); fc.add(c.heraldry.key());
        for (uint32_t cl : c.dress.cloth) fc.add(cl);
        fc.add((uint64_t)c.dress.cutM | (uint64_t)c.dress.cutF << 8 | (uint64_t)c.dress.head[0] << 16 | (uint64_t)c.dress.pattern << 24);
        fc.add((uint64_t)c.town.layout | (uint64_t)c.town.altLayout << 8 | (uint64_t)c.town.wall << 16 | (uint64_t)c.town.fence << 24 |
               (uint64_t)c.town.density << 32 | (uint64_t)c.town.trees << 40 | (uint64_t)c.town.paving << 48);
        fc.add((uint64_t)c.arms.helm[0] | (uint64_t)c.arms.body[0] << 8 | (uint64_t)c.arms.shield << 16 | (uint64_t)c.arms.blade << 24 |
               (uint64_t)c.arms.ornament << 32 | (uint64_t)(uint8_t)c.arms.favouredOre << 48);
        for (const cult::Alloy& a : c.arms.alloys) {
          fc.str(a.name); fc.add(a.color); fc.add((uint64_t)a.baseTier | (uint64_t)a.tierStep << 8 | (uint64_t)a.sheen << 16);
          for (const cult::Ingredient& in : a.recipe) { fc.add((uint64_t)(uint8_t)in.ore | (uint64_t)in.parts << 8); fc.str(in.reagent); }
        }
        for (const std::string& n : c.faith.names) fc.str(n);
        fc.add((uint64_t)c.faith.kind | (uint64_t)c.faith.domains << 8 | (uint64_t)c.customs.furniture << 24);
        for (uint8_t v : c.values) fc.add(v);
        fc.add((uint64_t)c.peopleMix[0] | (uint64_t)c.peopleMix[1] << 8 | (uint64_t)c.peopleMix[2] << 16);
      };
      for (int far = 0; far < 2; far++)
        for (int j = 0; j < 4; j++)
          for (int i = 0; i < 4; i++) {
            const int32_t o = far ? 49 : -2;
            const cult::Culture& c = at.family(o + i, o + j);
            addCulture(c);
            for (uint32_t b = 0; b < 3; b++) fc.add(cult::buildingArch(c, c.homeBiome, (int)b, (int)(3 - b), b * 977u + 5u).key());
          }
      for (int32_t ky = floorDiv(sp.spawn.y, KCELL) - 2; ky <= floorDiv(sp.spawn.y, KCELL) + 2; ky++)
        for (int32_t kx = floorDiv(sp.spawn.x, KCELL) - 2; kx <= floorDiv(sp.spawn.x, KCELL) + 2; kx++) {
          const KingdomPlan* K = A.kingdom(makeId(kx, ky, IdKind::Kingdom, 0));
          if (!K) continue;
          fc.str(K->name); fc.add(K->culture); fc.add(K->heraldry.key());
          addCulture(A.culture(K->culture));
        }
      g[S + ".cultures"] = fc.h;
    }
    // macro samples on a coarse lattice, near and far
    Fnv fmac;
    for (int32_t y = -100000; y <= 100000; y += 12503)
      for (int32_t x = -100000; x <= 100000; x += 9973) {
        MacroSample m = A.macroFar(x, y);
        fmac.add((uint64_t)(uint32_t)m.elev ^ ((uint64_t)(uint32_t)m.temp << 32)); fmac.add((uint64_t)(uint32_t)m.moist ^ ((uint64_t)m.biome << 32) ^ ((uint64_t)m.height << 40));
        fmac.add(m.kingdom);
      }
    g[S + ".macro"] = fmac.h;
  }
  return g;
}

std::string endlessFixture() {
#ifdef EMB_SOURCE_DIR
  return std::string(EMB_SOURCE_DIR) + "/tests/fixtures/golden_endless.txt";
#else
  return "tests/fixtures/golden_endless.txt";
#endif
}

int endlessGolden(bool write) {
  std::map<std::string, uint64_t> g = computeEndlessGolden();
  if (write) {
    printf("# rpg/world endless generator golden values (rpg_test --endless --golden --write). Integer maths only: must match on\n");
    printf("# every platform. Settlement interiors and footprints (the towns lane's output) are masked out.\n");
    for (auto& kv : g) printf("%s %016llx\n", kv.first.c_str(), (unsigned long long)kv.second);
    return 0;
  }
  FILE* f = fopen(endlessFixture().c_str(), "r");
  if (!f) { printf("FAIL: cannot read %s\n", endlessFixture().c_str()); return 1; }
  int bad = 0, seen = 0;
  char line[256], key[96];
  unsigned long long val;
  while (fgets(line, sizeof line, f)) {
    if (line[0] == '#' || sscanf(line, "%95s %llx", key, &val) != 2) continue;
    auto it = g.find(key);
    if (it == g.end()) { printf("FAIL: endless golden key %s is no longer computed\n", key); bad++; continue; }
    seen++;
    if (it->second != val) { printf("FAIL: endless golden %s = %016llx, expected %016llx\n", key, (unsigned long long)it->second, val); bad++; }
  }
  fclose(f);
  if (seen != (int)g.size()) { printf("FAIL: endless golden file has %d of %zu keys\n", seen, g.size()); bad++; }
  printf("endless golden: %zu hashes: %s\n", g.size(), bad ? "FAILED" : "ok");
  return bad;
}

int cmdEndless(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  const char* mapDir = nullptr;
  bool quick = false, golden = false, write = false, budget = true;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--map") && i + 1 < argc) mapDir = argv[++i];
    else if (!strcmp(argv[i], "--quick")) quick = true;
    else if (!strcmp(argv[i], "--map-at") && i + 1 < argc) { g_mapAt = sscanf(argv[++i], "%d,%d", &g_mapX, &g_mapY) == 2; }
    else if (!strcmp(argv[i], "--no-budget")) budget = g_budget = false;   // shared CI runners: report the costs, do not fail on them
    else if (!strcmp(argv[i], "--golden")) golden = true;
    else if (!strcmp(argv[i], "--write")) write = true;
  }
  if (golden) return endlessGolden(write) ? 1 : 0;
  int bad = 0, failedSeeds = 0;
  for (uint64_t s = a; s <= b; s++) {
    g_curSeed = s;
    int f = endlessSeed(s, mapDir, quick, budget);
    bad += f;
    if (f) failedSeeds++;
  }
  bad += checkSpecialties("endless");
  printf("endless: %llu seeds, %d failed (%d failures)\n", (unsigned long long)(b - a + 1), failedSeeds, bad);
  return bad ? 1 : 0;
}

// rpg_test --specialties [--seeds A..B]: the spread of specialisations over the real world's settlements (region plans
// only, 2048 x 2048 tiles round the origin per seed): no specialisation above 40 %, every one present
int cmdSpecialties(int argc, char** argv) {
  uint64_t a = 1, b = 20;
  bool list = false;   // --list: every settlement (for screenshot scripts: --play --at X,Y)
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--list")) list = true;
  }
  for (uint64_t s = a; s <= b; s++) {
    EndlessSource A(s);
    std::set<Gid> seen;
    for (int ry = -4; ry <= 3; ry++)
      for (int rx = -4; rx <= 3; rx++)
        for (const SitePlan& p : A.region(rx, ry).sites)
          if (isSettle(p.type) && seen.insert(p.id).second) {
            tallySpecialties(p);
            if (list) {
              printf("seed %llu %s %s arch %d %s at %d,%d", (unsigned long long)s, siteTypeName(p.type), specialtyName(p.special), (int)p.archetype, p.name.c_str(), p.ex, p.ey);
              if (p.special == Specialty::Mining) {   // where its mine is (the chunks over its footprint)
                ChunkData c;
                for (int32_t cy = (p.gy - 12) >> 5; cy <= (p.gy + p.h + 12) >> 5; cy++)
                  for (int32_t cx = (p.gx - 12) >> 5; cx <= (p.gx + p.w + 12) >> 5; cx++) {
                    A.chunk(cx, cy, c);
                    for (int i = 0; i < ChunkData::N; i++)
                      if (c.prop[i] == (int)art::Prop::MineEntrance + 1 || c.prop[i] == (int)art::Prop::MineHill + 1) printf("  mine %d,%d", cx * CHUNK + i % CHUNK, cy * CHUNK + i / CHUNK);
                  }
              }
              printf("\n");
            }
          }
  }
  const int bad = checkSpecialties("specialties");
  printf("specialties: %d failures\n", bad);
  return bad ? 1 : 0;
}

// rpg_test --world-places [--seeds A..B]: interesting global tiles for screenshot scripts (tools/scripts/world_*.txt):
// a road bridge, a footbridge, a ford, stairs where a road climbs, a lattice ramp, a tall cliff, a lake shore, a cave,
// a coast (beach next to the sea), a road far out in the wilderness
int cmdPlaces(int argc, char** argv) {
  uint64_t a = 7, b = 7;
  for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
  for (uint64_t seed = a; seed <= b; seed++) {
    EndlessSource A(seed);
    const StartPlan& sp = A.start();
    std::map<std::string, std::pair<int32_t, int32_t>> found;
    std::map<std::string, double> best;
    auto take = [&](const char* k, int32_t x, int32_t y, double score) {
      if (!best.count(k) || score < best[k]) { best[k] = score; found[k] = {x, y}; }
    };
    std::vector<IRect> towns;
    for (int ry = -3; ry <= 2; ry++)
      for (int rx = -3; rx <= 2; rx++)
        for (const SitePlan& p : A.region(rx, ry).sites) {
          if (isSettle(p.type)) towns.push_back(IRect{p.gx - 12, p.gy - 12, p.w + 24, p.h + 24});
          else if (p.type == SiteType::Cave) take("cave", p.ex, p.ey + 2, dst(p.ex, p.ey, sp.spawn.x, sp.spawn.y));
        }
    auto inTown = [&](int32_t x, int32_t y) {
      for (const IRect& r : towns) if (x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h) return true;
      return false;
    };
    ChunkData c;
    const int32_t c0x = chunkOf(sp.spawn.x) - 20, c0y = chunkOf(sp.spawn.y) - 20;
    for (int cy = 0; cy < 40; cy++)
      for (int cx = 0; cx < 40; cx++) {
        A.chunk(c0x + cx, c0y + cy, c);
        for (int ly = 1; ly < CHUNK - 1; ly++)
          for (int lx = 1; lx < CHUNK - 1; lx++) {
            const int i = c.at(lx, ly);
            const int32_t x = c.cx * CHUNK + lx, y = c.cy * CHUNK + ly;
            if (inTown(x, y)) continue;
            const double d = dst(x, y, sp.spawn.x, sp.spawn.y);
            const Ground g = (Ground)c.ground[i];
            auto G = [&](int dx, int dy) { return (Ground)c.ground[c.at(lx + dx, ly + dy)]; };
            bool roadN = G(1, 0) == Ground::Road || G(-1, 0) == Ground::Road || G(0, 1) == Ground::Road || G(0, -1) == Ground::Road;
            if (g == Ground::Bridge) take(roadN ? "bridge" : "footbridge", x, y + 3, d);
            auto path = [&](Ground q) { return q == Ground::Dirt || q == Ground::Road; };
            if (g == Ground::Dirt && ((groundWater(G(1, 0)) && groundWater(G(-1, 0)) && (path(G(0, 1)) || path(G(0, -1)))) ||
                                      (groundWater(G(0, 1)) && groundWater(G(0, -1)) && (path(G(1, 0)) || path(G(-1, 0))))))
              take("ford", x, y + 2, d);
            if (g == Ground::Road && (c.height[i] & Map::HEIGHT_RAMP)) take("stairs", x, y + 3, d);
            if (!roadN && g != Ground::Road && (c.height[i] & Map::HEIGHT_RAMP)) take("ramp", x, y + 3, d);
            if ((c.height[i] & Map::HEIGHT_CLIFF) && !groundWater(g) && ly >= 3) {
              int up = c.height[c.at(lx, ly - 1)] & 7, lo = c.height[i] & 7;
              if (up - lo >= 2) take("tallcliff", x, y + 3, d);
            }
            if (!groundWater(g) && groundWater(G(0, -1)) && c.biome[i] != (uint8_t)Biome::Ocean && G(0, -2) == Ground::Water) take("lakeshore", x, y + 1, d);
            if (g == Ground::Road && d > 350) take("wildroad", x, y, std::abs(d - 420));
          }
      }
    // a coast: walk out from the start in eight directions until the sea, take the nearest
    for (int k = 0; k < 8; k++) {
      double ang = k * 0.785398;
      for (int r = 200; r < 9000; r += 24) {
        int32_t x = sp.spawn.x + (int32_t)(std::cos(ang) * r), y = sp.spawn.y + (int32_t)(std::sin(ang) * r);
        MacroSample m = A.macro(x, y);
        if (m.biome == Biome::Ocean) {
          // step back onto the beach
          for (int s = 0; s < 40; s++) {
            int32_t bx = sp.spawn.x + (int32_t)(std::cos(ang) * (r - s)), by = sp.spawn.y + (int32_t)(std::sin(ang) * (r - s));
            if (!A.macro(bx, by).water) { take("coast", bx, by, r); break; }
          }
          break;
        }
      }
    }
    printf("seed %llu: spawn %d %d\n", (unsigned long long)seed, sp.spawn.x, sp.spawn.y);
    for (auto& kv : found) printf("  %-10s %d %d  (%.0f)\n", kv.first.c_str(), kv.second.first, kv.second.second, best[kv.first]);
  }
  return 0;
}

// rpg_test --ground-at X,Y [--seeds S..S] [--r R]: an ASCII dump of the chunk tiles around a global tile (ground
// letter, upper case where the biome is Mountain) plus the biome / relief level counts, to debug terrain by eye
int cmdGroundAt(int argc, char** argv) {
  uint64_t a = 42, b = 42;
  int32_t gx = 0, gy = 0, R = 24;
  bool levels = false, nat = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--ground-at") && i + 1 < argc) sscanf(argv[++i], "%d,%d", &gx, &gy);
    else if (!strcmp(argv[i], "--r") && i + 1 < argc) R = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--levels")) levels = true;
    else if (!strcmp(argv[i], "--nat")) nat = true;
  }
  static const char* letters = "~-swgmfatndlrp#ckoeibvz ????????";
  EndlessSource A(a);
  ChunkData c;
  std::map<int, int> biomes, grounds;
  for (int32_t y = gy - R / 2; y <= gy + R / 2; y++) {
    std::string row;
    for (int32_t x = gx - R; x <= gx + R; x++) {
      A.chunk(chunkOf(x), chunkOf(y), c);
      const int i = c.at(x - c.cx * CHUNK, y - c.cy * CHUNK);
      const int g = c.ground[i];
      char ch = g < 32 ? letters[g] : '?';
      if (c.biome[i] == (uint8_t)Biome::Mountain && ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
      if (levels) ch = (char)('0' + (c.height[i] & Map::HEIGHT_LEVEL));
      if (c.height[i] & Map::HEIGHT_CLIFF) ch = levels ? '|' : '|';
      if (levels && (c.height[i] & Map::HEIGHT_RAMP)) ch = '/';
      if (nat) ch = (char)('0' + A.macro(x, y).height);
      row += ch;
      biomes[c.biome[i]]++; grounds[g]++;
    }
    printf("%s\n", row.c_str());
  }
  printf("biomes:"); for (auto& kv : biomes) printf(" %d:%d", kv.first, kv.second);
  printf("\ngrounds:"); for (auto& kv : grounds) printf(" %d:%d", kv.first, kv.second);
  if (nat) {
    printf("\nelev along the row:");
    for (int32_t x = gx - 8; x <= gx + 8; x++) printf(" %d/%d", A.macro(x, gy).elev, A.macro(x, gy).height);
  }
  MacroSample m = A.macro(gx, gy);
  printf("\nmacro at %d,%d: biome %d height %d elev %d temp %d\n", gx, gy, (int)m.biome, m.height, m.elev, m.temp);
  {
    int peaks = 0, blended = 0, tiles = 0;
    for (int32_t cy = chunkOf(gy) - 2; cy <= chunkOf(gy) + 2; cy++)
      for (int32_t cx = chunkOf(gx) - 2; cx <= chunkOf(gx) + 2; cx++) {
        A.chunk(cx, cy, c);
        for (int i = 0; i < ChunkData::N; i++) {
          tiles++;
          if (c.prop[i] == (uint8_t)((int)art::Prop::Peak + 1)) peaks++;
          if (c.blend[i] >> 4) blended++;
        }
      }
    printf("5x5 chunks round it: %d peaks, %d of %d tiles blended\n", peaks, blended, tiles);
  }
  return 0;
}

// rpg_test --city-png DIR [--seeds S..S]: the story city's chunks at 3 px per tile (debugging the wall ring and the roads
// that reach it): ground colours, wall grey, gates green, wall gaps yellow, the plan's road polylines in red
int cmdCityPng(int argc, char** argv) {
  uint64_t a = 1, b = 1;
  const char* dir = ".";
  bool nearest = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--city-png") && i + 1 < argc) dir = argv[++i];
    else if (!strcmp(argv[i], "--nearest")) nearest = true;
  }
  (void)nearest;
  for (uint64_t seed = a; seed <= b; seed++) {
    EndlessSource A(seed);
    const StartPlan& sp = A.start();
    SitePlan city;
    for (const SitePlan& p : A.region(idRx(sp.capital), idRy(sp.capital)).sites) if (p.id == sp.capital) city = p;
    const int pad = 24, S = 3;
    const int32_t gx0 = city.gx - pad, gy0 = city.gy - pad, W = city.w + 2 * pad, H = city.h + 2 * pad;
    std::vector<uint32_t> px((size_t)W * S * H * S, 0xFF000000u);
    ChunkData c;
    std::vector<GTile> gates;
    std::vector<IRect> gaps;
    for (int32_t cy = chunkOf(gy0); cy <= chunkOf(gy0 + H - 1); cy++)
      for (int32_t cx = chunkOf(gx0); cx <= chunkOf(gx0 + W - 1); cx++) {
        A.chunk(cx, cy, c);
        for (auto& g : c.gates) gates.push_back(g);
        for (auto& g : c.wallGaps) gaps.push_back(g);
        for (int ly = 0; ly < CHUNK; ly++)
          for (int lx = 0; lx < CHUNK; lx++) {
            const int32_t x = cx * CHUNK + lx - gx0, y = cy * CHUNK + ly - gy0;
            if (x < 0 || y < 0 || x >= W || y >= H) continue;
            const int i = c.at(lx, ly);
            uint32_t col = groundColor((Ground)c.ground[i]);
            if (c.prop[i]) col = scale(col, 70);
            if (c.bldg[i]) col = rgb(170, 80, 60);
            if (c.wall[i]) col = rgb(120, 120, 130);
            if (c.height[i] & Map::HEIGHT_CLIFF) col = scale(col, 50);
            for (int j = 0; j < S; j++)
              for (int k = 0; k < S; k++) px[(size_t)(y * S + j) * W * S + x * S + k] = col;
          }
      }
    auto mark = [&](int32_t x, int32_t y, uint32_t col) {
      x -= gx0; y -= gy0;
      if (x < 0 || y < 0 || x >= W || y >= H) return;
      px[(size_t)(y * S + 1) * W * S + x * S + 1] = col;
    };
    for (const IRect& r : gaps) for (int y = r.y; y < r.y + r.h; y++) for (int x = r.x; x < r.x + r.w; x++) mark(x, y, rgb(255, 230, 0));
    for (const GTile& g : gates) for (int k = 0; k < 3; k++) mark(g.x + k, g.y, rgb(0, 255, 0));
    for (int ry = regionOf(gy0) - 1; ry <= regionOf(gy0 + H) + 1; ry++)
      for (int rx = regionOf(gx0) - 1; rx <= regionOf(gx0 + W) + 1; rx++)
        for (const RoadPlan& rp : A.region(rx, ry).roads)
          for (size_t k = 0; k + 1 < rp.pts.size(); k++) {
            const int n = std::max(std::abs(rp.pts[k + 1].x - rp.pts[k].x), std::abs(rp.pts[k + 1].y - rp.pts[k].y));
            for (int s = 0; s <= n; s++)
              mark(rp.pts[k].x + (n ? (rp.pts[k + 1].x - rp.pts[k].x) * s / n : 0), rp.pts[k].y + (n ? (rp.pts[k + 1].y - rp.pts[k].y) * s / n : 0), rgb(255, 0, 0));
          }
    // the planned footprint's outline
    for (int x = city.gx; x < city.gx + city.w; x++) { mark(x, city.gy, rgb(0, 200, 255)); mark(x, city.gy + city.h - 1, rgb(0, 200, 255)); }
    for (int y = city.gy; y < city.gy + city.h; y++) { mark(city.gx, y, rgb(0, 200, 255)); mark(city.gx + city.w - 1, y, rgb(0, 200, 255)); }
    const std::string path = std::string(dir) + "/city_" + std::to_string(seed) + ".png";
    writePng(path.c_str(), W * S, H * S, px);
    printf("seed %llu: %s %s at %d,%d (footprint %d,%d %dx%d) -> %s\n", (unsigned long long)seed, siteTypeName(city.type), city.name.c_str(), city.ex, city.ey, city.gx, city.gy,
           city.w, city.h, path.c_str());
  }
  return 0;
}

}  // namespace

RPG_TEST_CMD("--city-png", "the story city's chunks as a PNG (wall ring and roads debugging) --city-png DIR [--seeds A..B]", cmdCityPng);
RPG_TEST_CMD("--specialties", "the real world's spread of settlement specialisations (none above 40 %) [--seeds A..B]", cmdSpecialties);
RPG_TEST_CMD("--ground-at", "ASCII dump of the endless ground around a global tile [--seeds S..S] --ground-at X,Y [--r R] [--levels | --nat]", cmdGroundAt);
RPG_TEST_CMD("--world-places", "interesting global tiles for world screenshot scripts [--seeds A..B]", cmdPlaces);
RPG_TEST_CMD("--endless", "endless generator: start plan, order independence, spacing, kingdoms, rock, reachability, cost "
             "[--seeds A..B] [--map dir [--map-at X,Y]] [--quick] [--no-budget] [--golden [--write]]", cmdEndless);
