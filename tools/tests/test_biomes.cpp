// rpg_test --biomes [--seeds A..B] [--radius R] [--list] [--strict] [--at X,Y]: the M3c Wildlands biomes
// (rpg/world/biomes.h, VISION_PLAN 15.15 / 15.16). WORLD lane owns this file.
//
// Per seed:
//   - census: the tile classifier (EndlessSource::ecoAt: what the chunks carry before settlements and stamps) on a
//     48-tile lattice out to R (default 6000) tiles round the origin, every eco counted; the far estimate (ecoFar, the
//     maps' and searches' cheap guess) is asked at the same points and its agreement reported;
//   - rare lands: the wondrous biomes (EF_RARE) come in patches: their share of the land stays small, and each patch
//     is small (the biggest 4-connected run of rare lattice points is reported);
//   - consistency: in a few chunks near the start and far out, every tile's family == ecoFamily(eco), ecoNb is an eco,
//     ecoNb == eco wherever the blend weight is 0 (and not a paving mark), and the blend's family nibble ==
//     ecoFamily(ecoNb) where it is a real ecotone;
//   - determinism: ecoAt on two fresh sources asked in different orders agrees.
// Over the seed range: how many of the Eco::COUNT biomes were seen and the share of each. --strict fails unless every
// eco appears, no eco covers more than 25 % of the land, the rare lands together stay under 7 % and none alone over
// 2 %. --at X,Y prints the ecos round a tile (two letters of each key on an 8-tile grid) for debugging by eye.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>
#include "rpg/world/biomes.h"
#include "rpg/world/coords.h"
#include "rpg/world/dmath.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {
using namespace ew;

struct Census {
  std::vector<long> count = std::vector<long>((size_t)Eco::COUNT, 0);
  long agree = 0, asked = 0;
  int biggestRare = 0;   // the biggest 4-connected run of rare lattice points (48-tile spacing) of one eco
  Eco biggestEco = Eco::Ocean;
  uint64_t biggestSeed = 0;
};

int biomesSeed(uint64_t seed, int32_t R, Census& C, bool list) {
  int bad = 0;
  EndlessSource A(seed);
  std::vector<int32_t> firstX((size_t)Eco::COUNT, INT32_MIN), firstY((size_t)Eco::COUNT, 0);
  const int N = 2 * (R / 48) + 1;
  std::vector<uint8_t> grid((size_t)N * N, 0);
  for (int j = 0; j < N; j++)
    for (int i = 0; i < N; i++) {
      const int32_t x = -R + i * 48, y = -R + j * 48;
      const Eco e = A.ecoAt(x, y);
      if ((int)e >= (int)Eco::COUNT) { out("FAIL: ecoAt out of range at %d,%d\n", x, y); bad++; continue; }
      grid[(size_t)j * N + i] = (uint8_t)e;
      C.count[(size_t)e]++;
      if ((i & 3) == 0 && (j & 3) == 0) {   // (the far estimate on every fourth point: it is the slow one)
        C.asked++;
        if (A.ecoFar(x, y) == e) C.agree++;
      }
      if (firstX[(size_t)e] == INT32_MIN) { firstX[(size_t)e] = x; firstY[(size_t)e] = y; }
    }
  // the rare lands' patches: 4-connected runs of one rare eco on the lattice
  {
    std::vector<uint8_t> seen((size_t)N * N, 0);
    std::vector<int> st;
    for (int s0 = 0; s0 < N * N; s0++) {
      const Eco e = (Eco)grid[(size_t)s0];
      if (seen[(size_t)s0] || !ecoHas(e, EF_RARE)) continue;
      st.clear(); st.push_back(s0); seen[(size_t)s0] = 1;
      for (size_t h = 0; h < st.size(); h++) {
        const int k = st[h], x = k % N, y = k / N;
        const int nb[4] = {x > 0 ? k - 1 : -1, x < N - 1 ? k + 1 : -1, y > 0 ? k - N : -1, y < N - 1 ? k + N : -1};
        for (int n : nb)
          if (n >= 0 && !seen[(size_t)n] && (Eco)grid[(size_t)n] == e) { seen[(size_t)n] = 1; st.push_back(n); }
      }
      if ((int)st.size() > C.biggestRare) { C.biggestRare = (int)st.size(); C.biggestEco = e; C.biggestSeed = seed; }
    }
  }
  int seen = 0;
  for (int i = 0; i < (int)Eco::COUNT; i++) {
    if (firstX[(size_t)i] == INT32_MIN) continue;
    seen++;
    if (list) out("  seed %llu %-22s first at %d,%d\n", (unsigned long long)seed, ecoName((Eco)i), firstX[(size_t)i], firstY[(size_t)i]);
  }
  // consistency of the chunk layers
  const int32_t cks[][2] = {{0, 0}, {3, -2}, {-5, 4}, {40, 12}, {-60, -33}, {3125, -3125}};
  for (auto& ck : cks) {
    ChunkData c;
    A.chunk(ck[0], ck[1], c);
    for (int i = 0; i < ChunkData::N; i++) {
      const Eco e = (Eco)c.eco[i], nb = (Eco)c.ecoNb[i];
      if ((int)e >= (int)Eco::COUNT || (int)nb >= (int)Eco::COUNT) { out("FAIL: chunk %d,%d tile %d: eco %d / ecoNb %d out of range\n", ck[0], ck[1], i, (int)e, (int)nb); bad++; break; }
      if (ecoFamily(e) != (Biome)c.biome[i]) {
        out("FAIL: chunk %d,%d tile %d: eco %s is not of family %s\n", ck[0], ck[1], i, ecoName(e), biomeName((Biome)c.biome[i]));
        bad++;
        break;
      }
      const uint8_t bl = c.blend[i], w = (uint8_t)(bl >> 4);
      const bool mark = w > 8;   // PAVE / BOARDWALK / POOL marks
      if (!mark && w == 0 && nb != e) { out("FAIL: chunk %d,%d tile %d: unblended tile with ecoNb %s != eco %s\n", ck[0], ck[1], i, ecoName(nb), ecoName(e)); bad++; break; }
      if (!mark && w > 0 && (Biome)(bl & 15) != ecoFamily(nb)) { out("FAIL: chunk %d,%d tile %d: blend family %d != ecoNb's family\n", ck[0], ck[1], i, bl & 15); bad++; break; }
    }
  }
  // determinism: two sources, opposite orders
  {
    EndlessSource B1(seed), B2(seed);
    std::vector<uint8_t> a, b;
    for (int k = 0; k < 64; k++) a.push_back((uint8_t)B1.ecoAt(k * 977 - 30000, k * 613 - 20000));
    for (int k = 63; k >= 0; k--) b.push_back((uint8_t)B2.ecoAt(k * 977 - 30000, k * 613 - 20000));
    std::reverse(b.begin(), b.end());
    if (a != b) { out("FAIL: ecoAt depends on the order of the questions\n"); bad++; }
  }
  out("seed %llu: %d of %d biomes seen within %d tiles\n", (unsigned long long)seed, seen, (int)Eco::COUNT, R);
  return bad;
}

int cmdBiomes(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  int32_t R = 6000;
  bool list = false, strict = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--at") && i + 1 < argc) {
      int32_t ax = 0, ay = 0;
      sscanf(argv[++i], "%d,%d", &ax, &ay);
      for (int k = 1; k < argc; k++) if (!strcmp(argv[k], "--seeds") && k + 1 < argc) parseSeedRange(argv[k + 1], a, b);
      EndlessSource A(a);
      for (int32_t y = ay - 96; y <= ay + 96; y += 8) {
        for (int32_t x = ax - 160; x <= ax + 160; x += 8) { const char* k = ecoInfo(A.ecoAt(x, y)).key; printf("%c%c", k[0], k[1]); }
        printf("\n");
      }
      printf("ecoAt %s, ecoFar %s\n", ecoName(A.ecoAt(ax, ay)), ecoName(A.ecoFar(ax, ay)));
      // the chunks' eco layer on the same grid (it must match ecoAt away from towns and stamps)
      ChunkData c;
      for (int32_t y = ay - 96; y <= ay + 96; y += 8) {
        for (int32_t x = ax - 160; x <= ax + 160; x += 8) {
          A.chunk(chunkOf(x), chunkOf(y), c);
          const char* k = ecoInfo((Eco)c.eco[c.at(x - c.cx * CHUNK, y - c.cy * CHUNK)]).key;
          printf("%c%c", k[0], k[1]);
        }
        printf("\n");
      }
      return 0;
    }
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--radius") && i + 1 < argc) R = std::max(500, atoi(argv[++i]));
    else if (!strcmp(argv[i], "--list")) list = true;
    else if (!strcmp(argv[i], "--strict")) strict = true;
  }
  Census C;
  int bad = 0;
  for (uint64_t s = a; s <= b; s++) { g_curSeed = s; bad += biomesSeed(s, R, C, list); }
  long land = 0, rare = 0;
  for (int i = 0; i < (int)Eco::COUNT; i++) {
    if ((Eco)i == Eco::Ocean) continue;
    land += C.count[(size_t)i];
    if (ecoHas((Eco)i, EF_RARE)) rare += C.count[(size_t)i];
  }
  int seen = 0;
  std::vector<int> order;
  for (int i = 0; i < (int)Eco::COUNT; i++) { if (C.count[(size_t)i]) seen++; order.push_back(i); }
  std::sort(order.begin(), order.end(), [&](int x, int y) { return C.count[(size_t)x] > C.count[(size_t)y]; });
  printf("biomes: %d of %d seen over seeds %llu..%llu; share of land:\n", seen, (int)Eco::COUNT, (unsigned long long)a, (unsigned long long)b);
  for (int i : order) {
    if ((Eco)i == Eco::Ocean) continue;
    printf("  %-22s %6.2f %%%s%s\n", ecoName((Eco)i), land ? 100.0 * C.count[(size_t)i] / land : 0.0, ecoHas((Eco)i, EF_RARE) ? "  (rare)" : "",
           C.count[(size_t)i] ? "" : "   MISSING");
  }
  printf("biomes: rare lands %.2f %% of the land, biggest rare patch %d lattice points (~%d tiles across, %s, seed %llu); far estimate agrees %.1f %%\n",
         land ? 100.0 * rare / land : 0.0, C.biggestRare, 48 * (int)ew::isqrt((uint64_t)std::max(1, C.biggestRare)), ecoName(C.biggestEco),
         (unsigned long long)C.biggestSeed, C.asked ? 100.0 * C.agree / C.asked : 0.0);
  if (strict) {
    for (int i = 0; i < (int)Eco::COUNT; i++)
      if (!C.count[(size_t)i]) { printf("FAIL: biome %s never seen\n", ecoName((Eco)i)); bad++; }
    for (int i = 0; i < (int)Eco::COUNT; i++) {
      if ((Eco)i == Eco::Ocean || !land) continue;
      if (C.count[(size_t)i] * 100 > land * 25) { printf("FAIL: %s covers more than 25 %% of the land\n", ecoName((Eco)i)); bad++; }
      if (ecoHas((Eco)i, EF_RARE) && C.count[(size_t)i] * 100 > land * 2) { printf("FAIL: the rare %s covers more than 2 %% of the land\n", ecoName((Eco)i)); bad++; }
    }
    if (land && rare * 100 > land * 7) { printf("FAIL: the rare lands cover more than 7 %% of the land\n"); bad++; }
    if (C.biggestRare > 150) { printf("FAIL: a rare patch of %s spans %d lattice points (small patches only)\n", ecoName(C.biggestEco), C.biggestRare); bad++; }
  }
  printf("biomes: %d failures\n", bad);
  return bad ? 1 : 0;
}
RPG_TEST_CMD("--biomes", "M3c biomes: census, rare lands, layer consistency, determinism [--seeds A..B] [--radius R] [--list] [--strict] [--at X,Y]",
             cmdBiomes);

}  // namespace
