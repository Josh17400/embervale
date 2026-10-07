// rpg_test --forest [--seeds A..B] [--verbose]: the wild woods stay walkable (owner request, Oct 2026: "walking through
// forests is dang near impossible because the trees can be so close together").
//
// Per seed, deep patches of every wooded biome (forest, autumn, taiga, swamp, snow) are found from the macro map and a
// 4 x 4 chunk window (128 x 128 tiles) is generated round each. Away from towns and roads (the dressing there has
// its own rules):
//   (a) spacing: no two solid trees (any kind) touch, diagonals included: there is always a free tile between trunks;
//       no two solid wild props (trees, bushes, stumps, logs, rocks, boulders, cacti) touch either (reported, and
//       counted as a failure too: a bush between two trees walls the gap as well as a third tree would);
//   (b) walkability: of the patch's open ground that can be reached on foot from the window's edge when the flora is
//       ignored (cliffs, water and rock still count), >= 98 % is still reached with the flora in place;
//   (c) canopy: the crowns (a 3 x 3 box: the trunk's tile, its sides and the two rows above, the art's crown) still
//       cover a floor share of the patch's dry ground, so a forest still reads as one.
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <vector>
#include "rpg/art.h"
#include "rpg/world/coords.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {
using namespace ew;
using art::Prop;

bool isTree(int pr) { return pr >= 1 && pr - 1 <= (int)Prop::AutumnTree; }   // OakTree .. AutumnTree
bool wildSolid(int pr) {
  if (!pr) return false;
  const Prop p = (Prop)(pr - 1);
  if ((int)p <= (int)Prop::AutumnTree) return true;
  switch (p) {
    case Prop::Bush: case Prop::BerryBush: case Prop::SnowBush: case Prop::Boulder: case Prop::Rock: case Prop::MossRock:
    case Prop::SnowRock: case Prop::Stump: case Prop::Log: case Prop::Cactus: return true;
    default: return false;
  }
}

struct Win {
  int32_t gx0 = 0, gy0 = 0;   // global tile of the window's corner
  static constexpr int W = 4 * CHUNK;
  std::vector<uint8_t> ground, prop, biome, height, wall, bldg;
  std::vector<uint8_t> authored;   // sites, dens and peaks (with a margin): their dressing is placed on purpose
  int at(int x, int y) const { return y * W + x; }
};

void load(EndlessSource& A, int32_t cx0, int32_t cy0, Win& w) {
  w.gx0 = cx0 * CHUNK; w.gy0 = cy0 * CHUNK;
  const size_t n = (size_t)Win::W * Win::W;
  w.ground.assign(n, 0); w.prop.assign(n, 0); w.biome.assign(n, 0); w.height.assign(n, 0); w.wall.assign(n, 0); w.bldg.assign(n, 0);
  ChunkData c;
  for (int j = 0; j < 4; j++)
    for (int i = 0; i < 4; i++) {
      A.chunk(cx0 + i, cy0 + j, c);
      for (int ly = 0; ly < CHUNK; ly++)
        for (int lx = 0; lx < CHUNK; lx++) {
          const int s = c.at(lx, ly), d = w.at(i * CHUNK + lx, j * CHUNK + ly);
          w.ground[d] = c.ground[s]; w.prop[d] = c.prop[s]; w.biome[d] = c.biome[s]; w.height[d] = c.height[s];
          w.wall[d] = c.wall[s]; w.bldg[d] = c.bldg[s] ? 1 : 0;
        }
    }
  w.authored.assign(n, 0);
  auto mark = [&](int32_t gx0, int32_t gy0, int32_t gx1, int32_t gy1) {   // inclusive global rect
    for (int32_t y = std::max(gy0, w.gy0); y <= std::min(gy1, w.gy0 + Win::W - 1); y++)
      for (int32_t x = std::max(gx0, w.gx0); x <= std::min(gx1, w.gx0 + Win::W - 1); x++) w.authored[(size_t)w.at(x - w.gx0, y - w.gy0)] = 1;
  };
  for (int32_t ry = regionOf(w.gy0) - 1; ry <= regionOf(w.gy0 + Win::W) + 1; ry++)
    for (int32_t rx = regionOf(w.gx0) - 1; rx <= regionOf(w.gx0 + Win::W) + 1; rx++) {
      const RegionPlan& P = A.region(rx, ry);
      for (const SitePlan& s : P.sites) {
        mark(s.gx - 8, s.gy - 8, s.gx + s.w + 8, s.gy + s.h + 8);
        mark(s.ex - 10, s.ey - 10, s.ex + 10, s.ey + 10);
      }
      for (const DenPlan& dn : P.dens) mark(dn.x - 8, dn.y - 8, dn.x + 8, dn.y + 8);
    }
  for (int y = 0; y < Win::W; y++)   // peaks and their scree, wild landmarks (Peak .. GreatPeak), multi-tile fillers
    for (int x = 0; x < Win::W; x++) {
      const int pr = w.prop[(size_t)w.at(x, y)];
      if (pr && (pr - 1 >= (int)Prop::Peak || pr - 1 == (int)Prop::Filler)) mark(w.gx0 + x - 5, w.gy0 + y - 5, w.gx0 + x + 5, w.gy0 + y + 5);
    }
}

struct Res { int windows = 0, trees = 0, treePairs = 0, solidPairs = 0, fails = 0; double minWalk = 1, minCanopy = 1; };

int checkWindow(const Win& w, Biome target, uint64_t seed, bool verbose, Res& R) {
  const int W = Win::W;
  int fails = 0;
  // town ground (and 3 tiles round it) is left to the towns' own dressing rules
  std::vector<uint8_t> town((size_t)W * W, 0);
  for (int y = 0; y < W; y++)
    for (int x = 0; x < W; x++) {
      const int i = w.at(x, y);
      const Ground g = (Ground)w.ground[i];
      if (w.authored[(size_t)i]) town[(size_t)i] = 1;
      const bool t = w.wall[i] || w.bldg[i] || g == Ground::Road || g == Ground::Plaza || g == Ground::StoneFloor || g == Ground::WoodFloor ||
                     g == Ground::Farmland || g == Ground::Bridge || g == Ground::Dirt;
      if (!t) continue;
      for (int oy = -3; oy <= 3; oy++)
        for (int ox = -3; ox <= 3; ox++)
          if (x + ox >= 0 && y + oy >= 0 && x + ox < W && y + oy < W) town[(size_t)w.at(x + ox, y + oy)] = 1;
    }
  // (a) spacing
  int treePairs = 0, solidPairs = 0, trees = 0;
  for (int y = 1; y < W - 1; y++)
    for (int x = 1; x < W - 1; x++) {
      const int i = w.at(x, y);
      if (town[(size_t)i] || !wildSolid(w.prop[i])) continue;
      if (isTree(w.prop[i])) trees++;
      // each pair once: look E, SE, S, SW
      static const int d[4][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}};
      for (auto& o : d) {
        const int j = w.at(x + o[0], y + o[1]);
        if (town[(size_t)j] || !wildSolid(w.prop[j])) continue;
        solidPairs++;
        if (verbose && solidPairs <= 4)
          out("  solids touch at %d,%d (prop %d) and %d,%d (prop %d)\n", w.gx0 + x, w.gy0 + y, w.prop[i] - 1, w.gx0 + x + o[0], w.gy0 + y + o[1], w.prop[j] - 1);
        if (isTree(w.prop[i]) && isTree(w.prop[j])) {
          treePairs++;
          if (verbose && treePairs <= 3) out("  trees touch at %d,%d and %d,%d\n", w.gx0 + x, w.gy0 + y, w.gx0 + x + o[0], w.gy0 + y + o[1]);
        }
      }
    }
  // (b) walkability: flood from the window's edge with and without the wild flora
  auto blockedBase = [&](int i) {
    const Ground g = (Ground)w.ground[i];
    if (groundSolid(g) || (w.height[i] & Map::HEIGHT_CLIFF) || w.wall[i] || w.bldg[i]) return true;
    return w.prop[i] && propSolid((Prop)(w.prop[i] - 1)) && (town[(size_t)i] || !wildSolid(w.prop[i]));
  };
  auto flood = [&](bool flora, std::vector<uint8_t>& seen) {
    seen.assign((size_t)W * W, 0);
    std::vector<int> q;
    auto blocked = [&](int i) { return blockedBase(i) || (flora && wildSolid(w.prop[i])); };
    for (int k = 0; k < W; k++) {
      const int e[4] = {w.at(k, 0), w.at(k, W - 1), w.at(0, k), w.at(W - 1, k)};
      for (int i : e) if (!seen[(size_t)i] && !blocked(i)) { seen[(size_t)i] = 1; q.push_back(i); }
    }
    for (size_t h = 0; h < q.size(); h++) {
      const int x = q[h] % W, y = q[h] / W;
      static const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
      for (auto& o : d) {
        const int nx = x + o[0], ny = y + o[1];
        if (nx < 0 || ny < 0 || nx >= W || ny >= W) continue;
        const int j = w.at(nx, ny);
        if (seen[(size_t)j] || blocked(j)) continue;
        seen[(size_t)j] = 1;
        q.push_back(j);
      }
    }
  };
  std::vector<uint8_t> r0, r1;
  flood(false, r0);
  flood(true, r1);
  int want = 0, got = 0, dry = 0, covered = 0;
  std::vector<uint8_t> crown((size_t)W * W, 0);
  for (int y = 0; y < W; y++)
    for (int x = 0; x < W; x++) {
      const int i = w.at(x, y);
      if (!isTree(w.prop[i])) continue;
      for (int oy = -2; oy <= 0; oy++)
        for (int ox = -1; ox <= 1; ox++)
          if (x + ox >= 0 && y + oy >= 0 && x + ox < W && y + oy < W) crown[(size_t)w.at(x + ox, y + oy)] = 1;
    }
  for (int y = 8; y < W - 8; y++)
    for (int x = 8; x < W - 8; x++) {
      const int i = w.at(x, y);
      if ((Biome)w.biome[i] != target || town[(size_t)i]) continue;
      const Ground g = (Ground)w.ground[i];
      if (!groundSolid(g) && !(w.height[i] & Map::HEIGHT_CLIFF)) { dry++; covered += crown[(size_t)i]; }
      if (!r0[(size_t)i] || wildSolid(w.prop[i])) continue;
      want++;
      got += r1[(size_t)i];
    }
  if (dry < 1500) return 0;   // too little of the patch in the window (town, lake, mountain): not a sample
  const double walk = want ? (double)got / want : 1.0, canopy = (double)covered / dry;
  // canopy floors: the closed woods, the open ones (swamp willows, snowfield pines)
  const double floor = target == Biome::Forest || target == Biome::Autumn ? 0.45 : target == Biome::Taiga ? 0.30 : 0.10;
  R.windows++;
  R.trees += trees;
  R.treePairs += treePairs;
  R.solidPairs += solidPairs;
  if (walk < R.minWalk) R.minWalk = walk;
  if (canopy < R.minCanopy && target != Biome::Swamp && target != Biome::Snow) R.minCanopy = canopy;
  if (verbose || treePairs || solidPairs || walk < 0.98 || canopy < floor)
    out("  %s window at %d,%d: %d trees (%.1f %% of dry ground), %d touching tree pairs, %d touching solid pairs, walk %.2f %%, canopy %.1f %%\n",
        biomeName(target), w.gx0, w.gy0, trees, 100.0 * trees / dry, treePairs, solidPairs, 100.0 * walk, 100.0 * canopy);
  if (treePairs) { out("FAIL: seed %llu %s at %d,%d: %d pairs of trees touch\n", (unsigned long long)seed, biomeName(target), w.gx0, w.gy0, treePairs); fails++; }
  if (solidPairs) { out("FAIL: seed %llu %s at %d,%d: %d pairs of solid wild props touch\n", (unsigned long long)seed, biomeName(target), w.gx0, w.gy0, solidPairs); fails++; }
  if (walk < 0.98) { out("FAIL: seed %llu %s at %d,%d: only %.2f %% of the open ground reached on foot\n", (unsigned long long)seed, biomeName(target), w.gx0, w.gy0, 100.0 * walk); fails++; }
  if (canopy < floor) { out("FAIL: seed %llu %s at %d,%d: canopy %.1f %% < %.0f %%\n", (unsigned long long)seed, biomeName(target), w.gx0, w.gy0, 100.0 * canopy, 100.0 * floor); fails++; }
  return fails;
}

int cmdForest(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  bool verbose = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--verbose")) verbose = true;
  }
  static const Biome kinds[5] = {Biome::Forest, Biome::Autumn, Biome::Taiga, Biome::Swamp, Biome::Snow};
  int fails = 0;
  Res R;
  int perKind[5] = {};
  for (uint64_t seed = a; seed <= b; seed++) {
    g_curSeed = seed;
    EndlessSource A(seed);
    int found[5] = {};
    // deep patches: a coarse spiral of macro samples, the centre and 8 points 40 tiles out all of the one biome
    for (int ring = 0; ring <= 24; ring++)
      for (int j = -ring; j <= ring; j++)
        for (int i = -ring; i <= ring; i++) {
          if (std::max(std::abs(i), std::abs(j)) != ring) continue;
          const int32_t gx = i * 131, gy = j * 131;
          const Biome m = A.macro(gx, gy).biome;
          int t = -1;
          for (int q = 0; q < 5; q++) if (kinds[q] == m) t = q;
          if (t < 0 || found[t] >= 2) continue;
          bool deep = true;
          for (int oy = -1; oy <= 1 && deep; oy++)
            for (int ox = -1; ox <= 1 && deep; ox++) deep = A.macro(gx + ox * 40, gy + oy * 40).biome == m && !A.macro(gx + ox * 40, gy + oy * 40).water;
          if (!deep) continue;
          Win w;
          load(A, chunkOf(gx) - 2, chunkOf(gy) - 2, w);
          const int before = R.windows;
          fails += checkWindow(w, m, seed, verbose, R);
          if (R.windows > before) { found[t]++; perKind[t]++; }
        }
  }
  out("forest: %d windows (forest %d, autumn %d, taiga %d, swamp %d, snow %d), %d trees, %d touching tree pairs, %d touching solid pairs, "
      "worst walk %.2f %%, worst closed-wood canopy %.1f %%\n",
      R.windows, perKind[0], perKind[1], perKind[2], perKind[3], perKind[4], R.trees, R.treePairs, R.solidPairs, 100.0 * R.minWalk, 100.0 * R.minCanopy);
  for (int q = 0; q < 3; q++)
    if (!perKind[q]) { out("FAIL: no %s window found\n", biomeName(kinds[q])); fails++; }
  out("%s\n", fails ? "FOREST: FAIL" : "FOREST: OK");
  return fails ? 1 : 0;
}
}  // namespace

RPG_TEST_CMD("--forest", "wild woods: no two trees (or solid wild props) touch, >= 98 % of a wood's open ground reached on foot, "
             "canopy floors [--seeds A..B] [--verbose]", cmdForest);
