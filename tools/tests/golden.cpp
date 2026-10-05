// rpg_test --golden: pins the endless-world prep math (rpg/world/*.h) so it gives the same bits on every platform.
// Integer outputs (ids, cell seeds, Q16 value noise, fbm, domain warp, isqrt) are hashed and compared with
// tests/fixtures/golden_world.txt; the float approximations (dsin, dcos, datan2) are checked for accuracy only,
// since this test binary is built with /fp:fast. `rpg_test --golden --write` prints fresh values for the file.
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include "rpg/world/coords.h"
#include "rpg/world/dmath.h"
#include "rpg/world/ids.h"
#include "rpg/world/jobs.h"
#include "rpg/world/noise.h"
#include "tools/tests/tests.h"

namespace {

struct Fnv {
  uint64_t h = 1469598103934665603ull;
  void add(uint64_t v) { for (int i = 0; i < 8; i++) { h ^= (uint8_t)(v >> (i * 8)); h *= 1099511628211ull; } }
};

std::map<std::string, uint64_t> computeGolden() {
  using namespace ew;
  std::map<std::string, uint64_t> g;
  const uint64_t S = 0x00C0FFEE12345678ull;
  Fnv ids, seeds, vn, fb, wp, sq, cd, tr;
  for (int32_t ry = -3; ry <= 3; ry++)
    for (int32_t rx = -3; rx <= 3; rx++)
      for (uint32_t l : {0u, 1u, 77u, 4095u}) {
        Gid id = makeId(rx * 1000003, ry * 999983, IdKind::Npc, l);
        ids.add(id);
        ids.add((uint64_t)(uint32_t)idRx(id) ^ ((uint64_t)(uint32_t)idRy(id) << 32) ^ ((uint64_t)idKind(id) << 8) ^ idLocal(id));
      }
  for (int32_t y = -40; y <= 40; y += 7)
    for (int32_t x = -40; x <= 40; x += 5) seeds.add(cellSeed(S, tag("region.villages"), x * 131, y * 977));
  for (int32_t y = -300; y <= 300; y += 13)
    for (int32_t x = -300; x <= 300; x += 11) {
      vn.add((uint64_t)(uint32_t)vnoiseQ(x, y, 5, S));
      vn.add((uint64_t)(uint32_t)vnoiseQ(x * 37, y * 41, 9, S + 1));
      fb.add((uint64_t)(uint32_t)fbmQ(x * 3, y * 3, 7, 5, S + 2));
      int32_t wx = x * 5, wy = y * 5;
      warpQ(wx, wy, 6, 7, S + 3);
      wp.add((uint64_t)(uint32_t)wx | ((uint64_t)(uint32_t)wy << 32));
    }
  for (int32_t t = -2048; t <= 2048; t += 7) tr.add((uint64_t)(uint32_t)isinT(t) ^ ((uint64_t)(uint32_t)icosR(t, 1000 + t) << 32));
  for (uint64_t v = 0; v < 200000; v += 997) sq.add(isqrt(v * v + v * 31));
  sq.add(isqrt(0xFFFFFFFFFFFFull));
  for (int32_t t = -100000; t <= 100000; t += 4093) {
    cd.add((uint64_t)(uint32_t)chunkOf(t) | ((uint64_t)(uint32_t)regionOf(t) << 32));
    cd.add((uint64_t)(uint32_t)floorDiv(t, 1024) | ((uint64_t)(uint32_t)floorMod(t, 6144) << 32));
  }
  g["ids"] = ids.h; g["cellseed"] = seeds.h; g["vnoise"] = vn.h; g["fbm"] = fb.h; g["warp"] = wp.h; g["isqrt"] = sq.h; g["coords"] = cd.h; g["isin"] = tr.h;
  return g;
}

std::string fixturePath() {
#ifdef EMB_SOURCE_DIR
  return std::string(EMB_SOURCE_DIR) + "/tests/fixtures/golden_world.txt";
#else
  return "tests/fixtures/golden_world.txt";
#endif
}

}  // namespace

int goldenCheck(bool write) {
  using namespace ew;
  int bad = 0;
  std::map<std::string, uint64_t> g = computeGolden();
  if (write) {
    printf("# rpg/world golden values (rpg_test --golden --write). Integer math only: must match on every platform.\n");
    for (auto& kv : g) printf("%s %016llx\n", kv.first.c_str(), (unsigned long long)kv.second);
    return 0;
  }
  FILE* f = fopen(fixturePath().c_str(), "r");
  if (!f) { printf("FAIL: cannot read %s\n", fixturePath().c_str()); return 1; }
  char key[64];
  unsigned long long val;
  int seen = 0;
  char line[256];
  while (fgets(line, sizeof line, f)) {
    if (line[0] == '#' || sscanf(line, "%63s %llx", key, &val) != 2) continue;
    auto it = g.find(key);
    if (it == g.end()) { printf("FAIL: golden key %s is no longer computed\n", key); bad++; continue; }
    seen++;
    if (it->second != val) { printf("FAIL: golden %s = %016llx, expected %016llx\n", key, (unsigned long long)it->second, val); bad++; }
  }
  fclose(f);
  if (seen != (int)g.size()) { printf("FAIL: golden file has %d of %zu keys\n", seen, g.size()); bad++; }
  // float approximations: accuracy only
  double es = 0, ea = 0;
  for (int i = -2000; i <= 2000; i++) {
    float a = i * 0.00731f;
    es = std::max(es, std::fabs((double)dsin(a) - std::sin((double)a)));
    es = std::max(es, std::fabs((double)dcos(a) - std::cos((double)a)));
    float y = std::sin(i * 0.37f) * (1 + (i & 7)), x = std::cos(i * 0.53f) * (1 + (i % 5 + 5) % 5);
    ea = std::max(ea, std::fabs((double)datan2(y, x) - std::atan2((double)y, (double)x)));
  }
  if (es > 2e-5) { printf("FAIL: dsin/dcos max error %.2e\n", es); bad++; }
  if (ea > 5e-5) { printf("FAIL: datan2 max error %.2e\n", ea); bad++; }
  // the job queue runs sliced jobs to completion in priority order
  {
    struct Count : Job {
      int left, id; std::vector<int>* log;
      Count(int n, int i, int p, std::vector<int>* l) : left(n), id(i), log(l) { priority = p; }
      bool step(double) override { log->push_back(id); return --left <= 0; }
    };
    std::vector<int> log;
    JobQueue q;
    q.post(std::make_unique<Count>(2, 1, 0, &log));
    q.post(std::make_unique<Count>(3, 2, 5, &log));
    while (!q.empty()) q.pump(1000);
    if (log != std::vector<int>{2, 2, 2, 1, 1}) { printf("FAIL: job queue order\n"); bad++; }
  }
  printf("golden: %zu integer hashes, dsin/dcos err %.1e, datan2 err %.1e: %s\n", g.size(), es, ea, bad ? "FAILED" : "ok");
  return bad;
}
