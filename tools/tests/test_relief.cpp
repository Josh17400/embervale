// rpg_test --relief [--seeds A..B] [--windows N] [--size T] [--map dir] [--no-budget]: M11 "High Ground", the terrain
// relief as the generator makes it (VISION_PLAN 11, 13 M11, 15.32). WORLD lane owns this file (the lead wrote phase A).
//
// Per seed, sampled windows of T x T tiles (default 512): the start's, then one centred on each of the N - 1 named
// ranges / peaks nearest the start (default N = 4), generated chunk by chunk. Per window:
//   - reachability (11.4, 13 M11): the walkable tiles are split into 4-connected components; every component of 24 or
//     more tiles that keeps off the window's edge and off the water (an island: swum or sailed to) and is not the
//     largest one must NOT be walled in by the relief: a component whose dry boundary is at least half cliff tiles is a
//     walled plateau and FAILS (100 % reachability of plateaus of 24+ tiles in the sampled windows)
//   - the relief's census: tiles per level, cliffs, ramps, stairs, slopes, falls, stacked steps (2+ levels), the
//     highest level, plateaus (flat components of one level, 24+ tiles) and the smallest plateau
//   - the bake cost (13 M11: "the bake cost rises by at most 20 %"): the chunk generation average over the windows,
//     against the phase A baseline recorded below (BASE_CHUNK_MS, the same windows before M11); FAILS above 1.2 x
//     (--no-budget: report only). The terrain bake (the view's half) is measured by tools/scripts/m11_lead_bake.txt
//     with EMB_BAKEBENCH=1 (VISION_PLAN 15.32 records its baseline).
// (WORLD lane, M11) and per window: the census adds the 1-level steps and the share of them that are hillsides, the
// raised knobs under 24 tiles (the plateau minimum flattens those it may; a knob at a chunk edge keeps a way up), the
// rock tiles and the summit shrines; FAILS on a summit shrine (SPF_SUMMIT) off the main land or under 3 levels above
// the window's lowland (the lowest level holding 2 % of its land), a fall on or beside the sea, a pocket of water (under
// 12 tiles, a bridge's deck joining the water either side) shut in by falls, a mountain window (a tenth of its land at
// level 5+) without stacked faces, a hill window (half of its land at levels 1..3) without hillsides; and the second
// window's first 256 x 256 tiles, made by two fresh sources walking the chunks forwards and backwards, must hash alike
// (--no-order skips it). --probe X Y prints the 15 x 15 tiles round a tile of the first seed.
// --map dir writes relief_<seed>_<k>.png per window (1 tile/px: levels as grey, cliffs dark red, ramps yellow, stairs
// white, slopes green, falls cyan, unreached plateaus magenta).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "rpg/world/coords.h"
#include "rpg/world/ids.h"
#include "rpg/world/source.h"
#include "rpg/sim/game.h"
#include "tools/tests/tests.h"

namespace {
using namespace ew;

// The phase A baseline (lead, 2026-10-16, MSVC Release on the dev PC, ENDLESS_GEN_VER 23 before any M11 change): the
// chunk generation average over the default windows (--windows 4 --size 512) of seeds 1..3.
constexpr double BASE_CHUNK_MS = 0.790;   // 3072 chunks; per window 0.63 .. 0.90 ms
// (WORLD lane) the same phase A binary on the same PC, per seed (the default windows, 1024 chunks each; their mean
// over seeds 1..3 is 0.799, the lead's 0.790 within the noise): a run whose seeds all lie in 1..5 is held to the
// mean of its own seeds' baselines (seeds 4 and 5 are dearer windows: the lead's figure would hold them to seeds 1..3)
constexpr double BASE_SEED_MS[5] = {0.801, 0.892, 0.704, 0.894, 0.966};

int g_fail = 0;
void fail(const std::string& s) { out("FAIL: %s\n", s.c_str()); g_fail++; }

struct Census {
  int lv[8] = {}, cliff = 0, ramp = 0, stair = 0, slope = 0, fall = 0, stacked = 0, top = 0;
  int plateaus = 0, smallest = 0, walled = 0, mixed = 0, islands = 0;
  int oneStep = 0, knobs = 0, smallestRaised = 0, rock = 0;   // (WORLD lane) 1-level land steps; raised components < 24
  int summits = 0, pockets = 0, seaFalls = 0, lowland = 0, land = 0;
  uint64_t hash = 1469598103934665603ull;                       // the window's relief, ground and props (determinism)
  double chunkMs = 0;
  int chunks = 0;
};

void writeReliefPng(const char* dir, uint64_t seed, int k, int N, const std::vector<uint8_t>& hb, const std::vector<uint8_t>& g,
              const std::vector<uint8_t>& bad) {
  std::vector<uint32_t> px((size_t)N * N);
  for (size_t i = 0; i < px.size(); i++) {
    const uint8_t b = hb[i];
    const int l = b & Map::HEIGHT_LEVEL;
    uint32_t c = 0xFF000000u | (uint32_t)(40 + l * 28) * 0x010101u;
    if (groundWater((Ground)g[i])) c = 0xFF904020u;
    if (b & Map::HEIGHT_CLIFF) c = 0xFF101060u;
    if (b & Map::HEIGHT_SLOPE) c = 0xFF30A040u;
    if (b & Map::HEIGHT_RAMP) c = (b & Map::HEIGHT_STAIR) ? 0xFFFFFFFFu : 0xFF20D0F0u;
    if (b & Map::HEIGHT_FALL) c = 0xFFF0E040u;
    if (bad[i]) c = 0xFFFF00FFu;
    px[i] = c;
  }
  char path[512];
  std::snprintf(path, sizeof path, "%s/relief_%llu_%d.png", dir, (unsigned long long)seed, k);
  ::writePng(path, N, N, px);
}

// order: 0 the census and every check; 1 / 2 the window's hash only, its chunks made forwards / backwards
Census window(EndlessSource& S, uint64_t seed, int k, int32_t wx0, int32_t wy0, int N, const char* mapDir, int order = 0) {
  const bool reverse = order == 2;
  Census C;
  const int NC = N / CHUNK;
  std::vector<uint8_t> g((size_t)N * N), walk((size_t)N * N), hb((size_t)N * N), bad((size_t)N * N, 0), sea((size_t)N * N, 0);
  std::vector<uint8_t> pr((size_t)N * N, 0);
  ChunkData c;
  // (WORLD lane) reverse: the chunks in the opposite order (the determinism check makes the window twice)
  for (int n = 0; n < NC * NC; n++) {
      const int cy = (reverse ? NC * NC - 1 - n : n) / NC, cx = (reverse ? NC * NC - 1 - n : n) % NC;
      const double ms0 = S.stats().chunkMs;
      S.chunk((wx0 >> 5) + cx, (wy0 >> 5) + cy, c);
      C.chunkMs += S.stats().chunkMs - ms0;
      C.chunks++;
      for (int ly = 0; ly < CHUNK; ly++)
        for (int lx = 0; lx < CHUNK; lx++) {
          const int i = c.at(lx, ly);
          const size_t q = (size_t)(cy * CHUNK + ly) * N + cx * CHUNK + lx;
          g[q] = c.ground[i];
          hb[q] = c.height[i];
          pr[q] = c.prop[i];
          sea[q] = c.biome[i] == (uint8_t)Biome::Ocean;
          bool solid = groundSolid((Ground)c.ground[i]) || c.wall[i] || (c.height[i] & Map::HEIGHT_CLIFF) ||
                       (c.prop[i] && propSolid((art::Prop)(c.prop[i] - 1)));
          if (c.bldg[i]) {
            const Bldg& b = c.bldgs[(size_t)c.bldg[i] - 1];
            const int32_t gx = wx0 + cx * CHUNK + lx, gy = wy0 + cy * CHUNK + ly;
            solid = solid || !(gx == b.doorX() && gy == b.doorY());
          }
          walk[q] = !solid;
        }
  }
  for (size_t q = 0; q < g.size(); q++) {
    const uint8_t v[3] = {g[q], hb[q], pr[q]};
    for (uint8_t b : v) { C.hash ^= b; C.hash *= 1099511628211ull; }
  }
  if (order) return C;
  // census
  for (int y = 0; y < N; y++)
    for (int x = 0; x < N; x++) {
      const size_t i = (size_t)y * N + x;
      const uint8_t b = hb[i];
      const int l = b & Map::HEIGHT_LEVEL;
      C.lv[l]++;
      C.top = std::max(C.top, l);
      if (b & Map::HEIGHT_CLIFF) C.cliff++;
      if (b & Map::HEIGHT_RAMP) C.ramp++;
      if ((b & Map::HEIGHT_RAMP) && (b & Map::HEIGHT_STAIR)) C.stair++;
      if (b & Map::HEIGHT_SLOPE) C.slope++;
      if (b & Map::HEIGHT_FALL) C.fall++;
      if ((b & Map::HEIGHT_CLIFF) && y > 0 && (hb[i - N] & Map::HEIGHT_LEVEL) >= l + 2) C.stacked++;
      if ((Ground)g[i] == Ground::Rock) C.rock++;
      if ((b & Map::HEIGHT_STEP) && !groundWater((Ground)g[i]) && x > 0 && y > 0 && x < N - 1 && y < N - 1) {
        int hi = 0;
        const size_t nb4[4] = {i - 1, i + 1, i - N, i + N};
        for (size_t n : nb4) hi = std::max(hi, hb[n] & Map::HEIGHT_LEVEL);
        if (hi == l + 1) C.oneStep++;
      }
    }
  // raised components (4-connected tiles of one level, any step bits, off the water) under 24 tiles whose every
  // neighbour outside is lower: the knobs the plateau minimum (11.4) flattens
  {
    std::vector<uint8_t> seen((size_t)N * N, 0);
    std::vector<int> st;
    for (size_t s0 = 0; s0 < (size_t)N * N; s0++) {
      if (seen[s0] || (hb[s0] & Map::HEIGHT_LEVEL) == 0 || groundWater((Ground)g[s0])) continue;
      const int l = hb[s0] & Map::HEIGHT_LEVEL;
      st.assign(1, (int)s0);
      seen[s0] = 1;
      bool edge = false, raised = true;
      for (size_t h = 0; h < st.size(); h++) {
        const int q = st[h], x = q % N, y = q / N;
        if (x == 0 || y == 0 || x == N - 1 || y == N - 1) edge = true;
        const int nb[4] = {x > 0 ? q - 1 : -1, x < N - 1 ? q + 1 : -1, y > 0 ? q - N : -1, y < N - 1 ? q + N : -1};
        for (int n : nb) {
          if (n < 0) continue;
          const int ln = hb[(size_t)n] & Map::HEIGHT_LEVEL;
          if (ln == l && !groundWater((Ground)g[(size_t)n])) {
            if (!seen[(size_t)n]) { seen[(size_t)n] = 1; st.push_back(n); }
          } else if (ln >= l) raised = false;
        }
      }
      if (!edge && raised && st.size() < 24) {
        if (!C.knobs) out("  (a raised knob of %d tiles at level %d at %d,%d)\n", (int)st.size(), l, wx0 + st[0] % N, wy0 + st[0] / N);
        C.knobs++;
        if (!C.smallestRaised || (int)st.size() < C.smallestRaised) C.smallestRaised = (int)st.size();
      }
    }
  }
  // flat plateaus: 4-connected tiles of one level with no step bit, 24+ tiles, level 2 or more
  {
    std::vector<uint8_t> seen((size_t)N * N, 0);
    std::vector<int> st;
    for (size_t s0 = 0; s0 < (size_t)N * N; s0++) {
      if (seen[s0] || (hb[s0] & Map::HEIGHT_STEP) || (hb[s0] & Map::HEIGHT_LEVEL) < 2 || groundWater((Ground)g[s0])) continue;
      const int l = hb[s0] & Map::HEIGHT_LEVEL;
      st.assign(1, (int)s0);
      seen[s0] = 1;
      for (size_t h = 0; h < st.size(); h++) {
        const int q = st[h], x = q % N, y = q / N;
        const int nb[4] = {x > 0 ? q - 1 : -1, x < N - 1 ? q + 1 : -1, y > 0 ? q - N : -1, y < N - 1 ? q + N : -1};
        for (int n : nb)
          if (n >= 0 && !seen[(size_t)n] && !(hb[(size_t)n] & Map::HEIGHT_STEP) && (hb[(size_t)n] & Map::HEIGHT_LEVEL) == l &&
              !groundWater((Ground)g[(size_t)n])) {
            seen[(size_t)n] = 1;
            st.push_back(n);
          }
      }
      if ((int)st.size() >= 24) {
        C.plateaus++;
        if (!C.smallest || (int)st.size() < C.smallest) C.smallest = (int)st.size();
      }
    }
  }
  // walkable components: the largest is the land everyone walks; the others must not be walled in by cliffs
  std::vector<int> comp((size_t)N * N, 0);
  std::vector<int> sizes(1, 0);
  std::vector<int> st;
  for (size_t s0 = 0; s0 < (size_t)N * N; s0++) {
    if (!walk[s0] || comp[s0]) continue;
    const int id = (int)sizes.size();
    st.assign(1, (int)s0);
    comp[s0] = id;
    for (size_t h = 0; h < st.size(); h++) {
      const int q = st[h], x = q % N, y = q / N;
      const int nb[4] = {x > 0 ? q - 1 : -1, x < N - 1 ? q + 1 : -1, y > 0 ? q - N : -1, y < N - 1 ? q + N : -1};
      for (int n : nb) if (n >= 0 && walk[(size_t)n] && !comp[(size_t)n]) { comp[(size_t)n] = id; st.push_back(n); }
    }
    sizes.push_back((int)st.size());
  }
  const int main = (int)(std::max_element(sizes.begin(), sizes.end()) - sizes.begin());
  std::vector<uint8_t> done(sizes.size(), 0);
  for (size_t s0 = 0; s0 < (size_t)N * N; s0++) {
    const int id = comp[s0];
    if (!id || id == main || done[(size_t)id]) continue;
    done[(size_t)id] = 1;
    if (sizes[(size_t)id] < 24) continue;
    int cliffB = 0, otherB = 0;
    bool edge = false, water = false;
    std::vector<int> all(1, (int)s0);
    std::vector<uint8_t>& mark = bad;   // reuse: mark the component while flooding, unmark if it passes
    mark[s0] = 2;
    for (size_t h = 0; h < all.size(); h++) {
      const int q = all[h], x = q % N, y = q / N;
      if (x == 0 || y == 0 || x == N - 1 || y == N - 1) edge = true;
      const int nb[4] = {x > 0 ? q - 1 : -1, x < N - 1 ? q + 1 : -1, y > 0 ? q - N : -1, y < N - 1 ? q + N : -1};
      for (int n : nb) {
        if (n < 0) continue;
        if (comp[(size_t)n] == id) { if (!mark[(size_t)n]) { mark[(size_t)n] = 2; all.push_back(n); } continue; }
        if (groundWater((Ground)g[(size_t)n])) water = true;
        else if (hb[(size_t)n] & Map::HEIGHT_CLIFF) cliffB++;
        else otherB++;
      }
    }
    const bool walled = !edge && !water && cliffB > 0 && cliffB >= otherB;
    for (int q : all) mark[(size_t)q] = walled ? 1 : 0;
    if (edge) continue;
    if (water) { C.islands++; continue; }
    if (walled) {
      C.walled++;
      const int q0 = all.front();
      fail("seed " + std::to_string(seed) + " window " + std::to_string(k) + ": a plateau of " + std::to_string(all.size()) +
           " tiles near (" + std::to_string(wx0 + q0 % N) + "," + std::to_string(wy0 + q0 / N) + ") is walled in (" +
           std::to_string(cliffB) + " cliff / " + std::to_string(otherB) + " other boundary tiles)");
    } else if (cliffB > 0) C.mixed++;
  }
  // (WORLD lane) the window's lowland: the lowest land level holding 2 % of the land
  {
    int hist[8] = {};
    for (size_t q = 0; q < hb.size(); q++) if (!groundWater((Ground)g[q])) { hist[hb[q] & Map::HEIGHT_LEVEL]++; C.land++; }
    int acc = 0;
    C.lowland = 0;
    for (int l = 0; l < 8; l++) { acc += hist[l]; if (acc * 50 >= C.land) { C.lowland = l; break; } }
  }
  // (WORLD lane) the summit shrines in the window: reached on foot from the land everyone walks (the main component)
  // and standing 3 levels or more above the window's lowland
  {
    const int32_t rx0 = regionOf(wx0) - 1, ry0 = regionOf(wy0) - 1, rx1 = regionOf(wx0 + N) + 1, ry1 = regionOf(wy0 + N) + 1;
    for (int32_t ry = ry0; ry <= ry1; ry++)
      for (int32_t rx = rx0; rx <= rx1; rx++)
        for (const SitePlan& s : S.region(rx, ry).sites) {
          if (!(s.flags & SPF_SUMMIT)) continue;
          const int32_t fx = s.ex - wx0, fy = s.ey + 2 - wy0;   // the floor's front tile
          if (fx < 8 || fy < 8 || fx >= N - 8 || fy >= N - 8) continue;
          C.summits++;
          const size_t q = (size_t)fy * N + fx;
          const int lv = hb[(size_t)(s.ey - wy0) * N + fx] & Map::HEIGHT_LEVEL;
          if (!walk[q] || comp[q] != main)
            fail("seed " + std::to_string(seed) + " window " + std::to_string(k) + ": the summit shrine " + s.name + " at (" + std::to_string(s.ex) + "," +
                 std::to_string(s.ey) + ") is not reached from the main land");
          if (lv < C.lowland + 3)
            fail("seed " + std::to_string(seed) + " window " + std::to_string(k) + ": the summit shrine " + s.name + " stands on level " + std::to_string(lv) +
                 ", the lowland is level " + std::to_string(C.lowland) + " (needs 3 levels above it)");
        }
  }
  // (WORLD lane) waterfalls: never on the sea, never beside it; no sailable water left as a pocket among falls (a
  // water component of under 12 tiles off the window's edge that touches a fall: a boat there could never leave)
  {
    std::vector<int> wc((size_t)N * N, 0);
    std::vector<int> stk;
    int id = 0;
    for (size_t s0 = 0; s0 < (size_t)N * N; s0++) {
      if ((hb[s0] & Map::HEIGHT_FALL) && sea[s0]) C.seaFalls++;
      if (wc[s0] || !groundWater((Ground)g[s0]) || (hb[s0] & Map::HEIGHT_FALL)) continue;
      id++;
      stk.assign(1, (int)s0);
      wc[s0] = id;
      bool edge = false, fallB = false, seaB = false;
      for (size_t h = 0; h < stk.size(); h++) {
        const int q = stk[h], x = q % N, y = q / N;
        if (x == 0 || y == 0 || x == N - 1 || y == N - 1) edge = true;
        if (sea[(size_t)q]) seaB = true;
        const int nb[4] = {x > 0 ? q - 1 : -1, x < N - 1 ? q + 1 : -1, y > 0 ? q - N : -1, y < N - 1 ? q + N : -1};
        for (int n : nb) {
          if (n < 0) continue;
          if (hb[(size_t)n] & Map::HEIGHT_FALL) { fallB = true; continue; }
          // (a bridge's deck joins the water either side: the river runs on under it)
          if (!wc[(size_t)n] && (groundWater((Ground)g[(size_t)n]) || (Ground)g[(size_t)n] == Ground::Bridge)) { wc[(size_t)n] = id; stk.push_back(n); }
        }
      }
      if (fallB && seaB) C.seaFalls++;
      if (fallB && !edge && stk.size() < 12) {
        if (!C.pockets) out("  (a pocket of %d water tiles shut in by falls at %d,%d)\n", (int)stk.size(), wx0 + stk[0] % N, wy0 + stk[0] / N);
        C.pockets++;
        for (int q : stk) bad[(size_t)q] = 1;
      }
    }
    if (C.seaFalls) fail("seed " + std::to_string(seed) + " window " + std::to_string(k) + ": " + std::to_string(C.seaFalls) + " falls on or beside the sea");
    if (C.pockets) fail("seed " + std::to_string(seed) + " window " + std::to_string(k) + ": " + std::to_string(C.pockets) + " pockets of water shut in by falls");
    // (M11 integration) perched water: a water tile above BOTH its dry banks across the stream (a river riding on a
    // raised channel, an aqueduct: the terrain draws no face for it and a fall on it is never drawn)
    int perched = 0, px0 = 0, py0 = 0;
    for (int y = 1; y < N - 1; y++)
      for (int x = 1; x < N - 1; x++) {
        const size_t q = (size_t)y * N + x;
        if (!groundWater((Ground)g[q]) || sea[q]) continue;
        const int l = hb[q] & Map::HEIGHT_LEVEL;
        auto dryBelow = [&](size_t n) { return !groundWater((Ground)g[n]) && (Ground)g[n] != Ground::Bridge && (hb[n] & Map::HEIGHT_LEVEL) < l; };
        if ((dryBelow(q - 1) && dryBelow(q + 1)) || (dryBelow(q - N) && dryBelow(q + N))) {
          if (!perched) { px0 = wx0 + x; py0 = wy0 + y; }
          perched++;
        }
      }
    out("  perched water tiles (above both banks): %d%s\n", perched, perched ? (" (first at " + std::to_string(px0) + "," + std::to_string(py0) + ")").c_str() : "");
    // (M11 fixer r1) two-level jumps: a walkable tile beside a walkable 4-neighbour 2+ levels higher (a ramp that
    // skips the landing; 11.2: a stacked face's way up is two flights with a landing) - FAILS; and unmarked steps: a
    // walkable tile beside a higher walkable one with no step bit, on a chunk's edge row or column (a seam
    // disagreement; inside a chunk they are the cave and site entrances, which carry none by design) - FAILS
    {
      int jumps = 0, seamUnmarked = 0, jx = 0, jy = 0, ux = 0, uy = 0;
      for (int y = 1; y < N - 1; y++)
        for (int x = 1; x < N - 1; x++) {
          const size_t q = (size_t)y * N + x;
          if (!walk[q] || groundWater((Ground)g[q])) continue;
          const int l = hb[q] & Map::HEIGHT_LEVEL;
          const size_t nb4[4] = {q - 1, q + 1, q - N, q + N};
          bool jump = false, up = false;
          for (size_t n : nb4) {
            if (!walk[n] || groundWater((Ground)g[n])) continue;
            const int ln = hb[n] & Map::HEIGHT_LEVEL;
            if (ln >= l + 2) jump = true;
            if (ln > l) up = true;
          }
          if (jump) { if (!jumps) { jx = wx0 + x; jy = wy0 + y; } jumps++; }
          const int cx = (wx0 + x) & (CHUNK - 1), cy = (wy0 + y) & (CHUNK - 1);
          const bool seam = cx == 0 || cy == 0 || cx == CHUNK - 1 || cy == CHUNK - 1;
          if (up && seam && !(hb[q] & Map::HEIGHT_STEP) && (Ground)g[q] != Ground::Dirt) {
            if (!seamUnmarked) { ux = wx0 + x; uy = wy0 + y; }
            seamUnmarked++;
          }
        }
      out("  two-level jumps %d%s, unmarked seam steps %d%s\n", jumps, jumps ? (" (first at " + std::to_string(jx) + "," + std::to_string(jy) + ")").c_str() : "",
          seamUnmarked, seamUnmarked ? (" (first at " + std::to_string(ux) + "," + std::to_string(uy) + ")").c_str() : "");
      if (jumps) fail("seed " + std::to_string(seed) + " window " + std::to_string(k) + ": " + std::to_string(jumps) + " walkable two-level jumps (a way up skips its landing)");
      if (seamUnmarked) fail("seed " + std::to_string(seed) + " window " + std::to_string(k) + ": " + std::to_string(seamUnmarked) + " unmarked steps on chunk seams");
    }
    // (report only, an open M11 issue: 0..214 a window over seeds 1..5. The cause: hydro.cpp's RiverSeg::lv is the
    // smooth land's natural level (levelOf), while the chunks build reliefLevels (compressed, ridged, valleys sunk);
    // the bed walk holds the surface up to lvEnd. Reading fineLevel per river cell in hydroLv took it to 0..7 but cost
    // the region plans up to 80 %; sinking the bed to its banks in chunkgen left pockets shut in by falls.)
  }
  // (WORLD lane) the land's character: a mountain window (a tenth of its land at level 5 or more) has stacked faces;
  // a hill window (half of its land at levels 1 to 3) has hillsides
  {
    int high = 0, hills = 0;
    for (size_t q = 0; q < hb.size(); q++) {
      if (groundWater((Ground)g[q])) continue;
      const int l = hb[q] & Map::HEIGHT_LEVEL;
      if (l >= 5) high++;
      if (l >= 1 && l <= 3) hills++;
    }
    if (high * 10 >= C.land && C.stacked == 0) fail("seed " + std::to_string(seed) + " window " + std::to_string(k) + ": a mountain window without stacked faces");
    if (hills * 2 >= C.land && C.slope == 0) fail("seed " + std::to_string(seed) + " window " + std::to_string(k) + ": a hill window without hillsides");
  }
  if (mapDir) writeReliefPng(mapDir, seed, k, N, hb, g, bad);
  return C;
}
}  // namespace

static int reliefCmd(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  int nWin = 4, N = 512;
  const char* mapDir = nullptr;
  bool budget = true, determinism = true;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!std::strcmp(argv[i], "--windows") && i + 1 < argc) nWin = std::max(1, std::atoi(argv[++i]));
    else if (!std::strcmp(argv[i], "--size") && i + 1 < argc) N = std::max(64, std::atoi(argv[++i]) / CHUNK * CHUNK);
    else if (!std::strcmp(argv[i], "--map") && i + 1 < argc) mapDir = argv[++i];
    else if (!std::strcmp(argv[i], "--no-budget")) budget = false;
    else if (!std::strcmp(argv[i], "--no-order")) determinism = false;
    else if (!std::strcmp(argv[i], "--probe") && i + 2 < argc) {
      // (debug) --probe X Y: the 15 x 15 tiles round a global tile of seed A: level, then a letter per step bit
      // (c cliff, r ramp, s stair, h hillside, F fall) or the ground (~ water, = bridge, # rock, . other)
      const int32_t px = std::atoi(argv[i + 1]), py = std::atoi(argv[i + 2]);
      i += 2;
      EndlessSource S(a);
      ChunkData c;
      for (int32_t y = py - 7; y <= py + 7; y++) {
        std::string row;
        for (int32_t x = px - 7; x <= px + 7; x++) {
          S.chunk(floorDiv(x, CHUNK), floorDiv(y, CHUNK), c);
          const int q = c.at(x - floorDiv(x, CHUNK) * CHUNK, y - floorDiv(y, CHUNK) * CHUNK);
          const uint8_t hb = c.height[q];
          const Ground gg = (Ground)c.ground[q];
          char k = groundWater(gg) ? '~' : gg == Ground::Bridge ? '=' : gg == Ground::Rock ? '#' : '.';
          if (hb & Map::HEIGHT_CLIFF) k = 'c';
          if (hb & Map::HEIGHT_RAMP) k = (hb & Map::HEIGHT_STAIR) ? 's' : 'r';
          if (hb & Map::HEIGHT_SLOPE) k = 'h';
          if (hb & Map::HEIGHT_FALL) k = 'F';
          row += (char)('0' + (hb & Map::HEIGHT_LEVEL));
          row += k;
          row += ' ';
        }
        out("%s\n", row.c_str());
      }
      return 0;
    }
  }
  g_fail = 0;
  double msSum = 0;
  int chunkSum = 0;
  for (uint64_t s = a; s <= b; s++) {
    g_curSeed = s;
    EndlessSource S(s);
    const GTile st = S.start().spawn;
    // the windows: the start's, then the named ranges and peaks nearest the start (by region rings)
    std::vector<GTile> centres(1, st);
    std::vector<std::pair<int64_t, GTile>> highs;
    const int32_t rx0 = regionOf(st.x), ry0 = regionOf(st.y);
    for (int r = 0; r <= 8 && (int)highs.size() < 4 * nWin; r++)
      for (int32_t ry = ry0 - r; ry <= ry0 + r; ry++)
        for (int32_t rx = rx0 - r; rx <= rx0 + r; rx++) {
          if (std::max(std::abs(rx - rx0), std::abs(ry - ry0)) != r) continue;
          for (const LandmarkPlan& L : S.region(rx, ry).landmarks)
            if (L.kind == LandmarkKind::Range || L.kind == LandmarkKind::Peak) {
              const int64_t dx = L.x - st.x, dy = L.y - st.y;
              highs.push_back({dx * dx + dy * dy, GTile{L.x, L.y}});
            }
        }
    std::sort(highs.begin(), highs.end(), [](const std::pair<int64_t, GTile>& p, const std::pair<int64_t, GTile>& q) {
      return p.first != q.first ? p.first < q.first : (p.second.x != q.second.x ? p.second.x < q.second.x : p.second.y < q.second.y);
    });
    for (const auto& h : highs) {
      if ((int)centres.size() >= nWin) break;
      bool near = false;   // windows never overlap much
      for (const GTile& c : centres) if (std::abs(c.x - h.second.x) < N / 2 && std::abs(c.y - h.second.y) < N / 2) near = true;
      if (!near) centres.push_back(h.second);
    }
    for (size_t k = 0; k < centres.size(); k++) {
      const int32_t wx0 = (centres[k].x - N / 2) & ~(CHUNK - 1), wy0 = (centres[k].y - N / 2) & ~(CHUNK - 1);
      const Census C = window(S, s, (int)k, wx0, wy0, N, mapDir);
      msSum += C.chunkMs;
      chunkSum += C.chunks;
      // (WORLD lane) order independence: the second window again, 256 x 256 tiles of it, by a fresh source walking
      // the chunks backwards, against the same tiles walked forwards by another fresh source
      if (k == 1 && determinism) {
        const int M = std::min(N, 256);
        EndlessSource F(s), R(s);
        const Census a = window(F, s, (int)k, wx0, wy0, M, nullptr, 2);
        const Census b2 = window(R, s, (int)k, wx0, wy0, M, nullptr, 1);
        if (a.hash != b2.hash) fail("seed " + std::to_string(s) + " window 1: the relief differs with the chunk order");
        else out("seed %llu window 1: the same %dx%d tiles in two chunk orders (hash %016llx)\n", (unsigned long long)s, M, M, (unsigned long long)a.hash);
      }
      out("seed %llu window %d (%d,%d) %dx%d: levels %d %d %d %d %d %d %d %d (top %d) | cliff %d ramp %d stair %d slope %d fall %d stacked %d | "
          "plateaus %d (smallest %d) knobs<24 %d (smallest %d) | 1-level steps %d (slopes %d%%) rock %d | summits %d (lowland %d) | walled %d, cliff-and-other %d, islands %d | chunks avg %.3f ms\n",
          (unsigned long long)s, (int)k, wx0, wy0, N, N, C.lv[0], C.lv[1], C.lv[2], C.lv[3], C.lv[4], C.lv[5], C.lv[6], C.lv[7],
          C.top, C.cliff, C.ramp, C.stair, C.slope, C.fall, C.stacked, C.plateaus, C.smallest, C.knobs, C.smallestRaised, C.oneStep,
          C.oneStep ? C.slope * 100 / C.oneStep : 0, C.rock, C.summits, C.lowland, C.walled, C.mixed, C.islands,
          C.chunks ? C.chunkMs / C.chunks : 0.0);
    }
  }
  const double avg = chunkSum ? msSum / chunkSum : 0;
  double base = BASE_CHUNK_MS;
  if (a >= 1 && b <= 5 && nWin == 4 && N == 512) {
    base = 0;
    for (uint64_t s = a; s <= b; s++) base += BASE_SEED_MS[s - 1];
    base /= (double)(b - a + 1);
  }
  if (base > 0) {
    out("relief: chunk generation avg %.3f ms over %d chunks (baseline %.3f ms: x%.2f, budget x1.20)\n", avg, chunkSum, base, avg / base);
    if (budget && avg > 1.2 * base) fail("chunk generation rose more than 20 % over the phase A baseline");
  } else
    out("relief: chunk generation avg %.3f ms over %d chunks (no baseline recorded)\n", avg, chunkSum);
  out("relief: %d failures\n", g_fail);
  return g_fail ? 1 : 0;
}
RPG_TEST_CMD("--relief", "M11: terrain relief in sampled high-ground windows: walled plateaus (24+ tiles: none), the relief census, summit shrines, falls and pockets, stacked faces and hillsides, chunk-order determinism, the chunk cost against the phase A baseline [--seeds A..B] [--windows N] [--size T] [--map dir] [--no-budget] [--no-order] [--probe X Y]", reliefCmd);
