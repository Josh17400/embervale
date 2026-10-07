// rpg_test --forest [--seeds A..B] [--verbose]: the wild woods stay walkable (owner request, Oct 2026: "walking through
// forests is dang near impossible because the trees can be so close together").
//
// Per seed, deep patches of every wooded or rocky biome (M3c: every eco flagged EF_WOODED or EF_ROCKY, rpg/world/biomes.h,
// from the mixed forest to the bamboo, the mangroves, the badlands' hoodoos and the crystal barrens) are found with the
// tile classifier (EndlessSource::ecoAt) and a 4 x 4 chunk window (128 x 128 tiles) is generated round each. Away from
// towns and roads (the dressing there has its own rules):
//   (a) spacing: no two solid trees (any kind) touch, diagonals included: there is always a free tile between trunks;
//       no two solid wild props (trees, bushes, stumps, logs, rocks, boulders, cacti) touch either (reported, and
//       counted as a failure too: a bush between two trees walls the gap as well as a third tree would);
//   (b) walkability: of the patch's open ground that can be reached on foot from the window's edge when the flora is
//       ignored (cliffs, water and rock still count), >= 98 % is still reached with the flora in place;
//   (c) canopy: the crowns (a 3 x 3 box: the trunk's tile, its sides and the two rows above, the art's crown) still
//       cover a floor share of the patch's dry ground (per eco: closed woods 45 %, the taiga and the old giants 30 %,
//       the open and wet woods less, the rocky lands none), so a forest still reads as one.
// The report gives every eco's windows, trees, worst walk and canopy; the woods every seed range has (mixed forest,
// autumn woods, taiga) must have a window, the others are checked wherever they are found.
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

// (M3c) the shared answers (art_props.h): the classic trees and the Wildlands trees; every solid wild prop
bool isTree(int pr) { return pr >= 1 && art::isTreeProp((Prop)(pr - 1)); }
bool wildSolid(int pr) { return pr >= 1 && art::isWildSolidProp((Prop)(pr - 1)); }

struct Win {
  int32_t gx0 = 0, gy0 = 0;   // global tile of the window's corner
  static constexpr int W = 4 * CHUNK;
  std::vector<uint8_t> ground, prop, biome, height, wall, bldg, eco;
  std::vector<uint8_t> authored;   // sites, dens and peaks (with a margin): their dressing is placed on purpose
  int at(int x, int y) const { return y * W + x; }
};

void load(EndlessSource& A, int32_t cx0, int32_t cy0, Win& w) {
  w.gx0 = cx0 * CHUNK; w.gy0 = cy0 * CHUNK;
  const size_t n = (size_t)Win::W * Win::W;
  w.ground.assign(n, 0); w.prop.assign(n, 0); w.biome.assign(n, 0); w.height.assign(n, 0); w.wall.assign(n, 0); w.bldg.assign(n, 0); w.eco.assign(n, 0);
  ChunkData c;
  for (int j = 0; j < 4; j++)
    for (int i = 0; i < 4; i++) {
      A.chunk(cx0 + i, cy0 + j, c);
      for (int ly = 0; ly < CHUNK; ly++)
        for (int lx = 0; lx < CHUNK; lx++) {
          const int s = c.at(lx, ly), d = w.at(i * CHUNK + lx, j * CHUNK + ly);
          w.ground[d] = c.ground[s]; w.prop[d] = c.prop[s]; w.biome[d] = c.biome[s]; w.height[d] = c.height[s];
          w.wall[d] = c.wall[s]; w.bldg[d] = c.bldg[s] ? 1 : 0; w.eco[d] = c.eco[s];
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
      if (pr && ((pr - 1 >= (int)Prop::Peak && pr - 1 <= (int)Prop::GreatPeak) || pr - 1 == (int)Prop::Filler)) mark(w.gx0 + x - 5, w.gy0 + y - 5, w.gx0 + x + 5, w.gy0 + y + 5);
    }
}

struct Res { int windows = 0, trees = 0, treePairs = 0, solidPairs = 0, fails = 0; double minWalk = 1, minCanopy = 1; };

// the canopy floor of a wooded eco (0: rocky lands, judged on spacing and walking only)
double canopyFloor(Eco e) {
  switch (e) {
    case Eco::MixedForest: case Eco::BirchWood: case Eco::AutumnWood: case Eco::DarkForest: case Eco::Jungle: case Eco::BambooForest:
    case Eco::Silverwood: return 0.45;
    case Eco::BlossomGrove: case Eco::MushroomForest: return 0.35;
    case Eco::Taiga: case Eco::GiantForest: return 0.30;
    case Eco::FloodedForest: return 0.20;
    case Eco::Mangrove: return 0.12;
    case Eco::TaigaBog: return 0.10;
    case Eco::PetrifiedForest: return 0.06;
    default: return 0.0;
  }
}

int checkWindow(const Win& w, Eco target, uint64_t seed, bool verbose, Res& R) {
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
      if ((Eco)w.eco[i] != target || town[(size_t)i]) continue;
      const Ground g = (Ground)w.ground[i];
      if (!groundSolid(g) && !(w.height[i] & Map::HEIGHT_CLIFF)) { dry++; covered += crown[(size_t)i]; }
      if (!r0[(size_t)i] || wildSolid(w.prop[i])) continue;
      want++;
      got += r1[(size_t)i];
    }
  const bool coast = ecoFamily(target) == Biome::Beach;
  const bool mainWood = target == Eco::MixedForest || target == Eco::AutumnWood || target == Eco::Taiga || target == Eco::BirchWood;
  if (dry < (coast ? 300 : mainWood ? 1500 : 700)) {   // too little of the patch in the window: not a sample
    if (verbose) out("  (%s window at %d,%d: only %d tiles of it)\n", ecoName(target), w.gx0, w.gy0, dry);
    return 0;
  }
  const double walk = want ? (double)got / want : 1.0, canopy = (double)covered / dry;
  const double floor = canopyFloor(target);
  R.windows++;
  R.trees += trees;
  R.treePairs += treePairs;
  R.solidPairs += solidPairs;
  if (walk < R.minWalk) R.minWalk = walk;
  if (floor > 0 && canopy < R.minCanopy) R.minCanopy = canopy;
  if (verbose || treePairs || solidPairs || walk < 0.98 || canopy < floor)
    out("  %s window at %d,%d: %d trees (%.1f %% of dry ground), %d touching tree pairs, %d touching solid pairs, walk %.2f %%, canopy %.1f %%\n",
        ecoName(target), w.gx0, w.gy0, trees, 100.0 * trees / dry, treePairs, solidPairs, 100.0 * walk, 100.0 * canopy);
  if (treePairs) { out("FAIL: seed %llu %s at %d,%d: %d pairs of trees touch\n", (unsigned long long)seed, ecoName(target), w.gx0, w.gy0, treePairs); fails++; }
  if (solidPairs) { out("FAIL: seed %llu %s at %d,%d: %d pairs of solid wild props touch\n", (unsigned long long)seed, ecoName(target), w.gx0, w.gy0, solidPairs); fails++; }
  if (walk < 0.98) { out("FAIL: seed %llu %s at %d,%d: only %.2f %% of the open ground reached on foot\n", (unsigned long long)seed, ecoName(target), w.gx0, w.gy0, 100.0 * walk); fails++; }
  if (canopy < floor) { out("FAIL: seed %llu %s at %d,%d: canopy %.1f %% < %.0f %%\n", (unsigned long long)seed, ecoName(target), w.gx0, w.gy0, 100.0 * canopy, 100.0 * floor); fails++; }
  return fails;
}

int cmdForest(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  bool verbose = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--verbose")) verbose = true;
  }
  // every wooded or rocky eco but bare mountain rock (no flora grows on Ground::Rock)
  std::vector<Eco> kinds;
  for (int e = 0; e < (int)Eco::COUNT; e++)
    if ((Eco)e != Eco::Mountain && (ecoHas((Eco)e, EF_WOODED) || ecoHas((Eco)e, EF_ROCKY))) kinds.push_back((Eco)e);
  const int K = (int)kinds.size();
  int fails = 0;
  Res R;
  std::vector<Res> per((size_t)K);
  for (uint64_t seed = a; seed <= b; seed++) {
    g_curSeed = seed;
    EndlessSource A(seed);
    std::vector<int> found((size_t)K, 0);
    // deep patches: a spiral of tile-classifier samples, the centre and 8 points round it (40 tiles out; 18 for the
    // smaller woods and lands, 3 for the coasts' narrow bands): the centre and six of them of the one eco
    int left = K * 2;
    for (int ring = 0; ring <= 64 && left > 0; ring++)
      for (int j = -ring; j <= ring; j++)
        for (int i = -ring; i <= ring; i++) {
          if (std::max(std::abs(i), std::abs(j)) != ring) continue;
          const int32_t gx = i * 97, gy = j * 97;
          const Eco m = A.ecoAt(gx, gy);
          int t = -1;
          for (int q = 0; q < K; q++) if (kinds[(size_t)q] == m) t = q;
          if (t < 0 || found[(size_t)t] >= 2) continue;
          const bool mainWood = m == Eco::MixedForest || m == Eco::AutumnWood || m == Eco::Taiga || m == Eco::BirchWood;
          const int32_t D = mainWood ? 40 : ecoFamily(m) == Biome::Beach ? 3 : 18;
          int same = 0;
          for (int oy = -1; oy <= 1; oy++)
            for (int ox = -1; ox <= 1; ox++) if ((ox || oy) && A.ecoAt(gx + ox * D, gy + oy * D) == m) same++;
          if (same < 6) continue;
          Win w;
          load(A, chunkOf(gx) - 2, chunkOf(gy) - 2, w);
          Res one;
          const int f = checkWindow(w, m, seed, verbose, one);
          fails += f;
          if (!one.windows) continue;
          found[(size_t)t]++;
          left--;
          R.windows++; R.trees += one.trees; R.treePairs += one.treePairs; R.solidPairs += one.solidPairs;
          R.minWalk = std::min(R.minWalk, one.minWalk); R.minCanopy = std::min(R.minCanopy, one.minCanopy);
          Res& P = per[(size_t)t];
          P.windows++; P.trees += one.trees; P.treePairs += one.treePairs; P.solidPairs += one.solidPairs; P.fails += f;
          P.minWalk = std::min(P.minWalk, one.minWalk); P.minCanopy = std::min(P.minCanopy, one.minCanopy);
        }
  }
  out("forest: per eco (windows, trees, touching tree / solid pairs, worst walk, worst canopy / its floor):\n");
  for (int q = 0; q < K; q++) {
    const Res& P = per[(size_t)q];
    const Eco e = kinds[(size_t)q];
    if (!P.windows) { out("  %-22s no window found\n", ecoName(e)); continue; }
    out("  %-22s %2d windows %6d trees  %d / %d pairs  walk %.2f %%  canopy %5.1f %% / %2.0f %%%s\n", ecoName(e), P.windows, P.trees, P.treePairs,
        P.solidPairs, 100.0 * P.minWalk, canopyFloor(e) > 0 ? 100.0 * P.minCanopy : 0.0, 100.0 * canopyFloor(e), P.fails ? "  FAIL" : "");
  }
  out("forest: %d windows over %d wooded / rocky biomes, %d trees, %d touching tree pairs, %d touching solid pairs, "
      "worst walk %.2f %%, worst wood canopy %.1f %%\n",
      R.windows, K, R.trees, R.treePairs, R.solidPairs, 100.0 * R.minWalk, 100.0 * R.minCanopy);
  for (Eco must : {Eco::MixedForest, Eco::AutumnWood, Eco::Taiga})
    for (int q = 0; q < K; q++)
      if (kinds[(size_t)q] == must && !per[(size_t)q].windows) { out("FAIL: no %s window found\n", ecoName(must)); fails++; }
  out("%s\n", fails ? "FOREST: FAIL" : "FOREST: OK");
  return fails ? 1 : 0;
}
}  // namespace

RPG_TEST_CMD("--forest", "wild woods and rocky lands (every EF_WOODED / EF_ROCKY biome): no two trees (or solid wild props) touch, "
             ">= 98 % of the open ground reached on foot, canopy floors per biome [--seeds A..B] [--verbose]", cmdForest);
