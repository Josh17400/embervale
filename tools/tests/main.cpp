// Headless EMBERVALE checks: world generation stats + overview map PNG, a wandering combat bot, save round-trip,
// and the per-milestone lane checks (see tests.h for which file holds what).
//   rpg_test [seed] [--map out.png] [--secs N] [--mortal] [--noaudit]
//   rpg_test --seeds 1..20      one summary line per seed, a repetition-audit summary, nonzero exit on any failure
//   rpg_test --metrics [--seeds 1..5] [--metric-secs 2400]   game-feel and balance metrics (PLAN.md targets table)
//   rpg_test --pack-probe [--seeds 1..5]   three wolves vs a bot that never rolls / rolls after watching 0.25 s / 0.35 s
//   rpg_test --golden [--write]                               rpg/world noise + dmath golden values (M1 prep)
//   rpg_test --list-ci                                        the CI list (SOLID 5.4; equals tools/gates/ci_cmds.sh)
//   rpg_test --help                                           every registered command with its kind (gate, report, tool)
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
#include "rpg/art/art_rig.h"
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
std::vector<SeedCheck>& seedChecks() {
  static std::vector<SeedCheck> v;
  return v;
}
// ---- (SOLID 5.4) the test registry's tables
namespace {
// the commands that are not gates (TestKind in tests.h); every other registered command is a Gate
struct KindRow { const char* flag; TestKind kind; };
constexpr KindRow kKinds[] = {
    // reports: numbers for a reader, never gated
    {"--chunkbench", TestKind::Report}, {"--cityperf", TestKind::Report}, {"--wayside-census", TestKind::Report},
    {"--tale-reach", TestKind::Report}, {"--ladder-pace", TestKind::Report}, {"--forge-picks", TestKind::Report},
    {"--plan", TestKind::Report}, {"--envelope", TestKind::Report}, {"--saga-read", TestKind::Report},
    {"--doll-recipe", TestKind::Report}, {"--forge-ramps", TestKind::Report},
    // tools: debugging dumps, places for screenshot scripts, file writers
    {"--city-png", TestKind::Tool}, {"--ground-at", TestKind::Tool}, {"--sites-at", TestKind::Tool},
    {"--startvillage", TestKind::Tool}, {"--world-places", TestKind::Tool}, {"--home-places", TestKind::Tool},
    {"--expand-looks", TestKind::Tool}, {"--forge-solve", TestKind::Tool}, {"--forge-frozen", TestKind::Tool},
};
// The CI list: every test command .github/workflows/web.yml's test job runs, in its order, as tools/gates/ci_cmds.sh
// extracts it ("./build-native/" dropped; the `for s in 7 99 12345` loop left out). `rpg_test --list-ci` prints it and
// CI diffs it against ci_cmds.sh, so a line added to one and not the other fails the build.
constexpr const char* kCiList[] = {
    "rpg_test --seeds 1..20",
    "save_test",
    "rpg_test --endless --seeds 1..10 --no-budget",
    "rpg_test --window --seeds 1..5",
    "rpg_test --towns --seeds 1..10",
    "rpg_test --realm --seeds 1..10",
    "rpg_test --history 2000 --seeds 1..20",
    "rpg_test --wards --seeds 1..10",
    "rpg_test --story --seeds 1..5",
    "rpg_test --sagalib",
    "rpg_test --sagas 2000 --seeds 1..5",
    "rpg_test --campaigns --seeds 1..5",
    "rpg_test --voice",
    "rpg_test --voice --lane",
    "rpg_test --life --seeds 1..5",
    "rpg_test --folk --seeds 1..5",
    "rpg_test --gear --seeds 1..3",
    "rpg_test --power-curve",
    "rpg_test --loot-audit",
    "rpg_test --foes --seeds 1..3",
    "rpg_test --arms --seeds 1..3",
    "rpg_test --arms --seeds 1..3 --rig=1",
    "rpg_test --loot",
    "rpg_test --loot-sources --seeds 1..3",
    "rpg_test --powers",
    "rpg_test --skills",
    "rpg_test --spells",
    "rpg_test --sea --seeds 1..3",
    "rpg_test --sail --seeds 1..3",
    "rpg_test --relief --seeds 1..3 --no-budget",
    "rpg_test --climb --seeds 1..3",
    "rpg_test --rise --seeds 1..3",
    "rpg_test --save-soak 100",
    "rpg_test --service --seeds 1..3",
    "rpg_test --lord --seeds 1..3",
    "rpg_test --quest-mix --seeds 1..20 --gates --p1b --gates-p1b --ports",
    "rpg_test --town-motion --seeds 1..8 --city --gates",
    "rpg_test --jobs --seeds 1..3",
    "rpg_test --life-p1 --seeds 1..3 --hooks",
    "rpg_test --story-p1 --seeds 1..3",
    "rpg_test --story-events --seeds 1..3",
    "rpg_test --content-p1b --seeds 1..3 --gates",
    "rpg_test --radiant --seeds 1..3",
    "rpg_test --p1b-engine --seeds 1..5",
    "rpg_test --rig",
    "rpg_test --anim --gates",
    "rpg_test --doll --gates",
    "rpg_test --doll-coverage --scope all --seeds 1..20 --gates",
    "rpg_test --doll-variety --seeds 1..3 --gates",
    "rpg_test --doll-ladder --selftest",
    "rpg_test --look-key",
    "rpg_test --home --seeds 1..3",
    "rpg_test --companion --seeds 1..3",
    "rpg_test --banter --seeds 1..3",
    "rpg_test --quests",
    "rpg_test --mainquest",
    "rpg_test --towns --golden",
    "rpg_test --villages --strict",
    "rpg_test --structure --strict",
    "rpg_test --city-unique --strict",
    "rpg_test --plan-controls",
    "rpg_test --dialogue --seeds 1..3",
    "rpg_test --cultures 400 --seeds 1..5",
    "rpg_test --builder --seeds 1..12 --strict --society-strict",
    "rpg_test --biomes --seeds 1..5 --strict",
    "rpg_test --forest --seeds 1..20",
    "rpg_test --specialties --seeds 1..20",
    "rpg_test --golden",
    "rpg_test --endless --golden",
};
// main()'s own modes (not registered commands): gates too
constexpr const char* kBuiltinGates[] = {"--seeds", "--golden"};

// `rpg_test --list-ci`: the CI list, one command per line; exit 1 (and FAIL on stderr) when a line's rpg_test command is
// not a registered Gate or one of main()'s gate modes
int listCi() {
  int bad = 0;
  for (const char* line : kCiList) {
    printf("%s\n", line);
    if (strncmp(line, "rpg_test ", 9) != 0) continue;
    const char* a = line + 9;
    const std::string flag(a, strcspn(a, " "));
    bool ok = false;
    for (const char* b : kBuiltinGates) ok |= flag == b;
    for (const TestCmd& c : testCmds()) ok |= flag == c.flag && c.kind == TestKind::Gate;
    if (!ok) { fprintf(stderr, "FAIL: --list-ci: '%s' is not a registered gate\n", line); bad++; }
  }
  return bad ? 1 : 0;
}
}  // namespace

TestKind testKindOf(const char* flag) {
  for (const KindRow& r : kKinds)
    if (!strcmp(r.flag, flag)) return r.kind;
  return TestKind::Gate;
}
const char* testKindName(TestKind k) { return k == TestKind::Report ? "report" : k == TestKind::Tool ? "tool" : "gate"; }

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
  int bad = heroChecks(seed) + interiorChecks(seed) + defenceChecks(seed) + archChecks(seed);
  for (const SeedCheck& c : seedChecks()) bad += c.run(seed);   // M2: checks registered with RPG_SEED_CHECK
  return bad;
}
}  // namespace

// (P2c flip, D101) the rig this process started at and whether something opted into it (EMB_RIG or --rig=): test_rig's
// "the default rig is 2" reads them
int g_startRig = 0;
bool g_rigChosen = false;

int main(int argc, char** argv) {
  // (P2c flip, D101) the human rig as the game reads it: EMB_RIG=1|2, then `--rig=1|2` (the flag wins; it is taken out of
  // argv before dispatch, so verify_batch's env-less CI lines can opt into the rig-1 fallback). The default is 2
  if (const char* e = std::getenv("EMB_RIG")) { art::setRigVersion(std::atoi(e)); g_rigChosen = true; }
  {
    int w = 1;
    for (int i = 1; i < argc; i++) {
      if (!strncmp(argv[i], "--rig=", 6)) { art::setRigVersion(atoi(argv[i] + 6)); g_rigChosen = true; continue; }
      argv[w++] = argv[i];
    }
    argc = w;
    argv[argc] = nullptr;
  }
  g_startRig = art::rigVersion();
  // M1: commands registered by the lanes' test files (RPG_TEST_CMD in tests.h)
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--help")) {
      printf("rpg_test [seed] [--map out.png] [--secs N] [--mortal] [--noaudit] | --seeds A..B | --metrics | --golden [--write]\n");
      for (const TestCmd& c : testCmds()) printf("  %-14s %-6s %s\n", c.flag, testKindName(c.kind), c.help);
      printf("  %-14s %-6s %s\n", "--list-ci", "tool", "the CI list (web.yml's test commands, one per line; nonzero when a line is not a registered gate)");
      return 0;
    }
    if (!strcmp(argv[i], "--list-ci")) return listCi();
    for (const TestCmd& c : testCmds())
      if (!strcmp(argv[i], c.flag)) return c.run(argc, argv);
  }
  uint64_t seed = 12345, seedA = 0, seedB = 0;
  bool range = false, audit = true;
  const char* mapOut = nullptr;
  float secs = 120;
  bool mortal = false, metrics = false, golden = false, goldenWrite = false, packProbe = false;
  float metricSecs = 2400;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--map") && i + 1 < argc) mapOut = argv[++i];
    else if (!strcmp(argv[i], "--metrics")) metrics = true;
    else if (!strcmp(argv[i], "--pack-probe")) packProbe = true;
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
  if (packProbe) {
    if (!range) { seedA = 1; seedB = 5; }
    return runPackProbe(seedA, seedB);
  }
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
      printf("  | poi %d/%d  shapes %d/%d (grp %d, near %d, count %d)  greet %d%%  bldg grp %d", a.poiKinds, a.poiNear, a.layoutSigs, a.settlements, a.layoutMaxGroup,
             a.nearLayoutMaxGroup, a.countMaxGroup, gp, a.bldgMaxGroup);
      n++;
      sum.poiKinds += a.poiKinds; sum.poiNear += a.poiNear; sum.settlements += a.settlements; sum.layoutSigs += a.layoutSigs;
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
    printf("  poi kinds within 120 tiles of the start village: avg %.1f, min %d (target >= 8); within 60: avg %.1f (target >= 3)\n", (double)sum.poiKinds / n, minPoi, (double)sum.poiNear / n);
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
