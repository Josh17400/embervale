// Headless EMBERVALE checks: world generation stats + overview map PNG, a wandering combat bot, save round-trip,
// and the per-milestone lane checks (see tests.h for which file holds what).
//   rpg_test [seed] [--map out.png] [--secs N] [--mortal] [--noaudit]
//   rpg_test --seeds 1..20      one summary line per seed, a repetition-audit summary, nonzero exit on any failure
//   rpg_test --metrics [--seeds 1..5] [--metric-secs 2400]   game-feel and balance metrics (PLAN.md targets table)
//   rpg_test --golden [--write]                               rpg/world noise + dmath golden values (M1 prep)
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include "tools/tests/tests.h"

bool g_quiet = false;
uint64_t g_curSeed = 0;
void out(const char* fmt, ...) {
  if (g_quiet && strncmp(fmt, "FAIL", 4) != 0) return;
  if (g_quiet) printf("  [seed %llu] ", (unsigned long long)g_curSeed);
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
}

std::vector<TestCmd>& testCmds() {
  static std::vector<TestCmd> cmds;
  return cmds;
}
bool parseSeedRange(const char* s, uint64_t& a, uint64_t& b);

namespace {
bool parseRange(const char* s, uint64_t& a, uint64_t& b) { return parseSeedRange(s, a, b); }
}  // namespace

// "A..B", "A-B" or a single number
bool parseSeedRange(const char* s, uint64_t& a, uint64_t& b) {
  char* e = nullptr;
  a = strtoull(s, &e, 10);
  if (e == s) return false;
  if (*e == 0) { b = a; return true; }
  if (e[0] == '.' && e[1] == '.') e += 2; else if (e[0] == '-') e += 1; else return false;
  const char* s2 = e;
  b = strtoull(s2, &e, 10);
  return e != s2 && *e == 0 && b >= a;
}

namespace {
// every lane's per-seed checks (tests.h); a crash-free 0 means "nothing to check yet"
int laneChecks(uint64_t seed) {
  g_curSeed = seed;
  return heroChecks(seed) + interiorChecks(seed) + defenceChecks(seed) + archChecks(seed);
}
}  // namespace

int main(int argc, char** argv) {
  // M1: commands registered by the lanes' test files (RPG_TEST_CMD in tests.h)
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--help")) {
      printf("rpg_test [seed] [--map out.png] [--secs N] [--mortal] [--noaudit] | --seeds A..B | --metrics | --golden [--write]\n");
      for (const TestCmd& c : testCmds()) printf("  %-14s %s\n", c.flag, c.help);
      return 0;
    }
    for (const TestCmd& c : testCmds())
      if (!strcmp(argv[i], c.flag)) return c.run(argc, argv);
  }
  uint64_t seed = 12345, seedA = 0, seedB = 0;
  bool range = false, audit = true;
  const char* mapOut = nullptr;
  float secs = 120;
  bool mortal = false, metrics = false, golden = false, goldenWrite = false;
  float metricSecs = 2400;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--map") && i + 1 < argc) mapOut = argv[++i];
    else if (!strcmp(argv[i], "--metrics")) metrics = true;
    else if (!strcmp(argv[i], "--golden")) golden = true;
    else if (!strcmp(argv[i], "--write")) goldenWrite = true;
    else if (!strcmp(argv[i], "--metric-secs") && i + 1 < argc) metricSecs = (float)atof(argv[++i]);
    else if (!strcmp(argv[i], "--secs") && i + 1 < argc) secs = (float)atof(argv[++i]);
    else if (!strcmp(argv[i], "--mortal")) mortal = true;
    else if (!strcmp(argv[i], "--noaudit")) audit = false;
    else if (!strcmp(argv[i], "--seeds") && i + 1 < argc) {
      if (!parseRange(argv[++i], seedA, seedB)) { printf("bad --seeds range '%s' (use A..B)\n", argv[i]); return 2; }
      range = true;
    } else seed = (uint64_t)atoll(argv[i]);
  }
  if (golden) return goldenCheck(goldenWrite) ? 1 : 0;
  if (metrics) {
    if (!range) { seedA = 1; seedB = 5; }
    return runMetrics(seedA, seedB, metricSecs);
  }
  if (!range) {
    SeedResult r;
    int bad = runSeed(seed, mapOut, secs, mortal, r);
    bad += laneChecks(seed);
    if (audit) printAudit(repetitionAudit(seed));
    printf(bad ? "FAILED (%d)\n" : "ALL OK\n", bad);
    return bad ? 1 : 0;
  }

  // range mode: one summary line per seed, then the totals
  g_quiet = true;
  int pass = 0, fail = 0;
  std::vector<uint64_t> failed;
  Audit sum;
  int n = 0, minPoi = 1 << 30, maxLayout = 0, maxNearLayout = 0, maxCount = 0, maxBldg = 0, minGreetPct = 100;
  for (uint64_t s = seedA; s <= seedB; s++) {
    SeedResult r;
    int bad = runSeed(s, nullptr, secs, mortal, r);
    bad += laneChecks(s);
    if (bad) { fail++; failed.push_back(s); } else pass++;
    printf("seed %-6llu %s  gen %4.0f ms  sites %3d  bot kills %3d lvl %2d  step %.2f ms", (unsigned long long)s, bad ? "FAIL" : "ok  ",
           r.genMs, r.sites, r.kills, r.level, r.maxStep);
    if (audit) {
      Audit a = repetitionAudit(s);
      int gp = a.greetTalks ? 100 * a.greetDistinct / a.greetTalks : 100;
      printf("  | poi %d  shapes %d/%d (grp %d, near %d, count %d)  greet %d%%  bldg grp %d", a.poiKinds, a.layoutSigs, a.settlements, a.layoutMaxGroup,
             a.nearLayoutMaxGroup, a.countMaxGroup, gp, a.bldgMaxGroup);
      n++;
      sum.poiKinds += a.poiKinds; sum.settlements += a.settlements; sum.layoutSigs += a.layoutSigs;
      sum.greetTalks += a.greetTalks; sum.greetDistinct += a.greetDistinct; sum.bldgMaxGroup += a.bldgMaxGroup;
      minPoi = std::min(minPoi, a.poiKinds); maxLayout = std::max(maxLayout, a.layoutMaxGroup);
      maxNearLayout = std::max(maxNearLayout, a.nearLayoutMaxGroup); maxCount = std::max(maxCount, a.countMaxGroup); maxBldg = std::max(maxBldg, a.bldgMaxGroup);
      minGreetPct = std::min(minGreetPct, gp);
    }
    printf("\n");
    fflush(stdout);
  }
  if (audit && n) {
    printf("repetition audit over %d seeds (informational):\n", n);
    printf("  poi kinds within 60 tiles: avg %.1f, min %d (target >= 8)\n", (double)sum.poiKinds / n, minPoi);
    printf("  settlement shapes: %.0f%% distinct, worst same-shape group %d world-wide, %d within 120 tiles (target 1), same building count %d\n",
           sum.settlements ? 100.0 * sum.layoutSigs / sum.settlements : 0.0, maxLayout, maxNearLayout, maxCount);
    printf("  greetings: %.0f%% distinct per town on average, worst seed %d%%\n", sum.greetTalks ? 100.0 * sum.greetDistinct / sum.greetTalks : 0.0, minGreetPct);
    printf("  identical buildings: avg largest group %.1f, worst %d\n", (double)sum.bldgMaxGroup / n, maxBldg);
  }
  printf("%d passed, %d failed", pass, fail);
  if (!failed.empty()) { printf(" (seeds"); for (uint64_t s : failed) printf(" %llu", (unsigned long long)s); printf(")"); }
  printf("\n");
  return fail ? 1 : 0;
}
