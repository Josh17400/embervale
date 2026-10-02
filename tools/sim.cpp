// Headless TAILSPIN tool: unit tests + a bot that plays full runs (balance / perf checks).
//   tailspin_sim --test
//   tailspin_sim [seconds=300] [seed=1] [runs=1]
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "game/bot.h"

static int g_fail = 0;
#define CHECK(c, msg) do { if (!(c)) { std::printf("FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__); g_fail++; } \
                           else std::printf("ok:   %s\n", msg); } while (0)

static Enemy mk(Vec2 p, float hp = 5) {
  Enemy e; e.p = p; e.hp = e.maxhp = hp; e.r = 18; e.t = EType::Drifter; return e;
}

static int runTests() {
  // 1. doLoop kills exactly what's inside the polygon
  {
    Game g; g.reset(7);
    g.enemies.clear();
    g.enemies.push_back(mk({1000, 1000}));   // inside
    g.enemies.push_back(mk({1050, 960}));    // inside
    g.enemies.push_back(mk({1500, 1000}));   // outside
    std::vector<Vec2> sq = {{900, 900}, {1200, 900}, {1200, 1100}, {900, 1100}};
    int k = g.doLoop(sq, {1050, 1000});
    CHECK(k == 2, "doLoop kills the 2 enemies inside the square");
    CHECK(g.enemies[2].maxhp > 0, "enemy outside the loop survives");
  }
  // 2. chain blast: a kill inside the loop detonates and takes a neighbour outside it
  {
    Game g; g.reset(7);
    g.enemies.clear();
    g.enemies.push_back(mk({1190, 1000}, 5));    // inside, dies, blasts
    g.enemies.push_back(mk({1260, 1000}, 5));    // just outside (70u away), within chain radius
    std::vector<Vec2> sq = {{900, 900}, {1200, 900}, {1200, 1100}, {900, 1100}};
    int k = g.doLoop(sq, {1050, 1000});
    CHECK(k == 2, "chain blast pops the neighbour outside the polygon");
  }
  // 3. End to end: steer in a circle -> tail crosses itself -> loop fires and kills a pinned enemy
  {
    Game g; g.reset(3);
    g.enemies.clear(); g.gems.clear();
    Input in;
    // circle centre sits one turn-radius to the right of the start heading
    float R = g.cruiseSpeed() / g.turnRate();
    Vec2 c = g.head + fromAngle(g.heading + PI * 0.5f) * R;
    Enemy e = mk(c, 9999); e.t = EType::Boss; e.r = 20; e.maxhp = e.hp = 9999;   // boss: never knocked, never moves toward us
    e.p = c;
    g.enemies.push_back(e);
    int loops = 0;
    for (int i = 0; i < 120 * 4 && g.stats.loops == 0; i++) {
      in.steer = fromAngle(g.heading + 1.0f);
      g.enemies[0].p = c; g.enemies[0].v = Vec2();
      g.update(SIM_DT, in);
      g.events.clear();
      loops = g.stats.loops;
    }
    CHECK(loops >= 1, "circling creates a loop (tail self-crossing detected)");
    CHECK(g.enemies[0].hp < 9999, "the loop damaged the enemy at its centre");
  }
  // 4. a straight line never loops
  {
    Game g; g.reset(5);
    g.enemies.clear();
    Input in; in.steer = Vec2(0, -1);
    for (int i = 0; i < 120 * 2; i++) { g.update(SIM_DT, in); g.events.clear(); }
    CHECK(g.stats.loops == 0, "flying straight does not loop");
  }
  // 5. level up flow
  {
    Game g; g.reset(9);
    g.enemies.clear();
    Gem gm; gm.p = g.head; gm.value = 9;   // xpNeed is 8 -> exactly one level
    g.gems.push_back(gm);
    Input in; in.steer = Vec2(0, -1);
    g.update(SIM_DT, in);
    CHECK(g.mode == Mode::LevelUp && g.numChoices == 3, "collecting XP opens a 3-card level up");
    g.pickUpgrade(0);
    CHECK(g.mode == Mode::Play, "picking a card resumes play");
  }
  std::printf("\n%s (%d failures)\n", g_fail ? "TESTS FAILED" : "ALL TESTS PASSED", g_fail);
  return g_fail ? 1 : 0;
}

int main(int argc, char** argv) {
  if (argc > 1 && !std::strcmp(argv[1], "--test")) return runTests();
  float seconds = argc > 1 ? (float)std::atof(argv[1]) : 300.0f;
  uint64_t seed = argc > 2 ? (uint64_t)std::atoll(argv[2]) : 1;
  int runs = argc > 3 ? std::atoi(argv[3]) : 1;
  for (int run = 0; run < runs; run++) {
    Game g; g.reset(seed + run);
    Bot bot; bot.r = Rng(seed + run * 77);
    double worst = 0, total = 0; long steps = 0;
    float nextReport = 30;
    while (g.mode != Mode::Dead && g.stats.time < seconds) {
      if (g.mode == Mode::LevelUp) { g.pickUpgrade(bot.r.irange(g.numChoices)); continue; }
      Input in = bot.act(g, SIM_DT);
      auto t0 = std::chrono::high_resolution_clock::now();
      g.update(SIM_DT, in);
      double ms = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t0).count();
      worst = std::max(worst, ms); total += ms; steps++;
      g.events.clear();
      if (g.stats.time >= nextReport) {
        nextReport += 30;
        std::printf("  t=%3.0fs hp=%3.0f lvl=%2d enemies=%3zu kills=%4d loops=%3d best=%2d\n", g.stats.time, g.hp,
                    g.level, g.enemies.size(), g.stats.kills, g.stats.loops, g.stats.bestLoop);
      }
    }
    std::printf("run %d seed %llu: %s at %.0fs  kills=%d loops=%d bestLoop=%d level=%d bosses=%d  | step avg %.3fms worst %.3fms\n",
                run, (unsigned long long)(seed + run), g.mode == Mode::Dead ? "DIED" : "survived", g.stats.time, g.stats.kills,
                g.stats.loops, g.stats.bestLoop, g.level, g.stats.bosses, total / std::max(1L, steps), worst);
  }
  return 0;
}
