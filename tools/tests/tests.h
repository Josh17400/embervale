// rpg_test (headless EMBERVALE checks), split by area so milestone lanes can each own a file:
//   main.cpp        arguments, range mode, the per-seed hook calls                 (lead)
//   png.cpp         PNG writer + overview-map colours                               (lead)
//   audit.cpp       the repetition audit                                            (lead)
//   seed_run.cpp    one seed: world stats, reachability, the wandering bot, save    (sim / opening lane)
//   metrics.cpp     --metrics: game-feel and balance bots                           (sim / opening lane)
//   golden.cpp      --golden: rpg/world noise and dmath golden values               (lead, M1 prep)
//   test_hero.cpp       heroChecks: equipment, creator, backgrounds                 (hero lane)
//   test_interiors.cpp  interiorChecks: interior BFS validity and clutter           (homes lane)
//   test_defence.cpp    defenceChecks: factions, town defence, bounty clarity       (town-defence lane)
//   test_arch.cpp       archChecks: walls, gates, building footprints and styles     (architecture lane)
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "rpg/sim/game.h"

// ---- output: in range mode (--seeds A..B) only FAIL lines are printed (tagged with the seed), plus one summary per seed
extern bool g_quiet;
extern uint64_t g_curSeed;
void out(const char* fmt, ...);

// ---- png.cpp
bool writePng(const char* path, int w, int h, const std::vector<uint32_t>& rgba);
uint32_t groundColor(Ground g);

// ---- audit.cpp: informational numbers that later generator tasks should raise (PLAN.md task 1)
struct Audit {
  int poiKinds = 0, poiCount = 0;          // distinct site kinds / sites within 60 tiles of the start
  std::string poiList;
  int settlements = 0, layoutSigs = 0, layoutMaxGroup = 0;   // settlement layout signatures (world-wide)
  int nearSettlements = 0, nearLayoutMaxGroup = 0;           // ... within 120 tiles of the start
  int countMaxGroup = 0;                                     // settlements of one type with the same building count
  int greetTowns = 0, greetTalks = 0, greetDistinct = 0, greetWorstPct = 100;
  std::string greetWorst;
  int bldgs = 0, bldgSigs = 0, bldgMaxGroup = 0;             // buildings with identical (type, w, h, roof)
  std::string bldgMaxDesc;
};
Audit repetitionAudit(uint64_t seed);
void printAudit(const Audit& a);

// ---- seed_run.cpp
struct SeedResult { int bad = 0; double genMs = 0; int sites = 0, kills = 0, level = 1; float maxStep = 0; };
int runSeed(uint64_t seed, const char* mapOut, float secs, bool mortal, SeedResult& res);

// ---- metrics.cpp
int runMetrics(uint64_t A, uint64_t B, float progSecs);

// ---- golden.cpp: returns failures; write = print fresh values instead of checking
int goldenCheck(bool write);

// ---- per-seed lane checks, run after runSeed in single and range mode. Each returns its number of failures and
//      reports them with out("FAIL: ..."). Keep each under ~2 s per seed (they run for 20 seeds in CI).
int heroChecks(uint64_t seed);
int interiorChecks(uint64_t seed);
int defenceChecks(uint64_t seed);
int archChecks(uint64_t seed);
