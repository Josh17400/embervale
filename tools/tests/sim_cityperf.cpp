// rpg_test --cityperf [--seeds A..B] [--people N] [--secs S]: big cities stay fast (owner, VISION_PLAN 15.8; SIM lane M1).
// A city of 160-220 homes holds hundreds of people. The game streams a settlement's people in and out around the
// player (Game::streamSitePeople: FOLK_IN / FOLK_OUT tiles, at most FOLK_CAP townsfolk), and townsfolk far off screen
// with nothing to react to sleep (NPC level of detail: their AI runs a few times a second).
// The test travels to the biggest settlement near the start, gives it N people in all (the generator's own plus
// synthetic villagers and guards on free tiles across a 200 x 200-tile area, as many as a huge city has), then walks
// the player through it for S seconds (a square loop, ordinary walking speed), then lets three wolves loose in the
// streets (the bell, the guards). It reports step times (average, 99th percentile, worst), how many people were in
// play (awake / asleep) and streamed in and out, and fails when the crowd is not bounded or nobody is near the player.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "tools/tests/tests.h"

namespace {

struct Run { double avg = 0, p99 = 0, worst = 0; int maxActors = 0, maxAwake = 0, maxAsleep = 0, maxFolk = 0, minNear = 1 << 30; };

int cityPerfSeed(uint64_t seed, int people, float secs) {
  int bad = 0;
  auto fail = [&](const std::string& s) { out("FAIL: cityperf: %s\n", s.c_str()); bad++; };
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.godMode = true;
  g.noWildSpawns = true;
  g.hour = 12;
  // the biggest settlement among the records (cities first)
  int city = -1, best = -1;
  for (int i = 0; i < (int)g.world.sites.size(); i++) {
    const Site& s = g.world.sites[(size_t)i];
    if (!s.settlement()) continue;
    int score = (s.type == SiteType::City ? 100000 : s.type == SiteType::Town ? 50000 : 0) + s.r.w * s.r.h;
    if (score > best) { best = score; city = i; }
  }
  if (city < 0) { fail("no settlement"); return bad; }
  const ew::Gid cid = g.world.sites[(size_t)city].id;
  g.world.sites[(size_t)city].discovered = true;
  if (!g.fastTravel(city)) { fail("cannot travel to the city"); return bad; }
  city = g.world.siteHandle(cid);
  g.world.ensureSiteRecords(city);
  const Site C = g.world.sites[(size_t)city];   // (a copy: streaming appends to the sites while the walk runs)
  int own = 0;
  for (const Spawn& sp : g.world.over.spawns) if (sp.site == city && sp.npc) own++;
  // fill the city up to `people`: villagers (a guard in ten) on free tiles around its heart
  Rng r(seed * 977 + 13);
  const int cx = C.ex, cy = C.ey;
  const int32_t gcx = g.world.ox + cx, gcy = g.world.oy + cy;   // (global: the walk moves the window)
  int added = 0;
  for (int tries = 0; tries < people * 40 && own + added < people; tries++) {
    int x = cx + r.irange(200) - 100, y = cy + r.irange(200) - 100;
    const Map& m = g.world.over;
    if (!m.in(x, y) || m.blocked(x, y) || m.bldgAt[(size_t)y * m.w + x] >= 0) continue;
    Spawn sp;
    sp.x = x; sp.y = y; sp.npc = true; sp.site = city; sp.slot = 2000 + added;
    sp.role = added % 10 == 0 ? Role::Guard : (added % 7 == 0 ? Role::Merchant : Role::Villager);
    g.world.siteSpawns[city].push_back((int)g.world.over.spawns.size());
    g.world.over.spawns.push_back(sp);
    added++;
  }
  // a square walk through the city at an ordinary pace (about 3.5 tiles per second), the player teleported along it
  std::vector<double> steps;
  Run run;
  const float speed = 3.5f * TILE / 60.0f;
  const Vec2 c0((cx - 40) * (float)TILE, (cy - 40) * (float)TILE);
  const Vec2 corners[4] = {c0, c0 + Vec2(80.0f * TILE, 0), c0 + Vec2(80.0f * TILE, 80.0f * TILE), c0 + Vec2(0, 80.0f * TILE)};
  g.pl().p = corners[0];
  int leg = 0;
  const int frames = (int)(secs * 60);
  int streamedIn0 = g.perf.spawnedNpcs, streamedOut0 = g.perf.despawnedNpcs;
  for (int f = 0; f < frames; f++) {
    Vec2 to = corners[(leg + 1) % 4] - g.pl().p;
    if (len(to) < speed * 2) leg = (leg + 1) % 4;
    else g.pl().p += norm(to) * speed;
    {   // the walk passes over everything; a door tile would walk in, so the player steps around it
      const Map& m = g.world.over;
      int fx = (int)std::floor(g.pl().p.x / TILE), fy = (int)std::floor((g.pl().p.y - 2) / TILE);
      int bi = m.in(fx, fy) ? m.bldgAt[(size_t)fy * m.w + fx] : -1;
      if (bi >= 0 && bldgEntryAt(m.bldgs[(size_t)bi], fx, fy)) g.pl().p.y += TILE;   // (a door or an open front's bay)
    }
    if (f == frames * 2 / 3) {   // trouble: three wolves loose in the streets near the player
      for (int k = 0; k < 3; k++) {
        int id = g.debugSpawnAt(art::Monster::Wolf, g.pl().p + Vec2(60.0f + 10.0f * k, 30.0f), 3);
        (void)id;
      }
    }
    auto s0 = std::chrono::steady_clock::now();
    g.update(SIM_DT, Input());
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - s0).count();
    g.events.clear();
    if (g.mode != Mode::Play) g.mode = Mode::Play;
    if (g.inside) { fail("walked into a building"); break; }
    steps.push_back(ms);
    run.maxActors = std::max(run.maxActors, (int)g.actors.size());
    run.maxAwake = std::max(run.maxAwake, g.perf.npcAwake);
    run.maxAsleep = std::max(run.maxAsleep, g.perf.npcAsleep);
    int folkNow = 0;
    for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].npc && g.actors[k].role != Role::Guard) folkNow++;
    run.maxFolk = std::max(run.maxFolk, folkNow);
    if (f > 120 && f % 60 == 0) {   // townsfolk within 12 tiles of the player (nobody frozen, nobody missing)
      int near = 0;
      for (size_t k = 1; k < g.actors.size(); k++)
        if (g.actors[k].npc && len(g.actors[k].p - g.pl().p) < 12.0f * TILE) near++;
      run.minNear = std::min(run.minNear, near);
    }
    // nobody on screen may be asleep (the widest phone view: about 18 tiles each way across, 8.5 up and down)
    for (size_t k = 1; k < g.actors.size(); k++) {
      Vec2 d = g.actors[k].p - g.pl().p;
      if (g.actors[k].asleep && std::fabs(d.x) < 18.0f * TILE && std::fabs(d.y) < 9.0f * TILE) {
        fail("a townsperson on screen is asleep");
        f = frames;
        break;
      }
    }
  }
  std::vector<double> sorted = steps;
  std::sort(sorted.begin(), sorted.end());
  double sum = 0;
  for (double v : steps) sum += v;
  if (!sorted.empty()) {
    run.avg = sum / sorted.size();
    run.p99 = sorted[(size_t)(sorted.size() * 0.99)];
    run.worst = sorted.back();
  }
  int in = g.perf.spawnedNpcs - streamedIn0, outN = g.perf.despawnedNpcs - streamedOut0;
  if (run.maxFolk > Game::FOLK_CAP + 4) fail("the crowd is not bounded: " + std::to_string(run.maxFolk) + " townsfolk (not counting the watch) in play");
  if (own + added >= 100 && in < 20) fail("people did not stream in as the player walked (" + std::to_string(in) + ")");
  out("cityperf seed %llu: %s %s (%s, %dx%d tiles, %d buildings): %d people (%d its own, %d synthetic) | step avg %.3f ms, p99 %.2f ms, "
      "worst %.2f ms | in play max %d actors (%d townsfolk besides the watch), %d awake, %d asleep, at least %d townsfolk within 12 tiles | streamed in %d, out %d\n",
      (unsigned long long)seed, siteTypeName(C.type), C.name.c_str(), C.capital ? "capital" : "not a capital", C.r.w, C.r.h, C.bldgCount, own + added, own,
      added, run.avg, run.p99, run.worst, run.maxActors, run.maxFolk, run.maxAwake, run.maxAsleep, run.minNear == (1 << 30) ? 0 : run.minNear, in, outN);
  // (M5 TOWNSFOLK) the census's own people at full strength: the street cap raised past 120 resident actors at the
  // evening's comings and goings (17:30: home from work, out to the tavern), the player at the heart; the 15.12 budget
  // is 1.5 ms per step for 120 residents (lifeStep plus every resident's lifeFolk: Game::lifeFolkStats)
  {
    g.lifeFolkCap = 160;
    g.teleportGlobal(gcx, gcy + 2);
    for (int k = 0; k < 3 && g.inside; k++) { g.debugLeave(); g.pl().p.y += 24.0f; g.update(SIM_DT, Input()); }   // (a doorstep walks in)
    g.hour = 17.5f;
    for (int f = 0; f < 4 * 60; f++) { g.update(SIM_DT, Input()); g.events.clear(); if (g.mode != Mode::Play) g.mode = Mode::Play; }
    const Game::LifeFolkStats s0 = g.lifeFolkStats();
    int minRes = 1 << 30, maxRes = 0, walking = 0;
    double upd = 0;
    for (int f = 0; f < 8 * 60; f++) {
      auto s1 = std::chrono::steady_clock::now();
      g.update(SIM_DT, Input());
      upd += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - s1).count();
      g.events.clear();
      if (g.mode != Mode::Play) g.mode = Mode::Play;
      minRes = std::min(minRes, g.lifeFolkStats().residents);
      maxRes = std::max(maxRes, g.lifeFolkStats().residents);
      walking = std::max(walking, g.lifeFolkStats().walking);
    }
    const Game::LifeFolkStats& s1 = g.lifeFolkStats();
    const double life = (s1.sumMs - s0.sumMs) / std::max(1, s1.steps - s0.steps);
    out("cityperf seed %llu: residents at 17:30: %d-%d resident actors (up to %d walking), townsfolk lane %.3f ms per step on average (worst %.2f), "
        "whole step %.3f ms, paths %d (cache hits %d)\n",
        (unsigned long long)seed, minRes, maxRes, walking, life, s1.worstMs, upd / (8 * 60), s1.pathRequests, s1.pathCacheHits);
    if (minRes >= 120 && life > 1.5) fail("120 resident actors cost " + std::to_string(life) + " ms per step (budget 1.5)");
    g.lifeFolkCap = 0;
  }
  return bad;
}

int cmdCityPerf(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  int people = 450;
  float secs = 90;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--people") && i + 1 < argc) people = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--secs") && i + 1 < argc) secs = (float)atof(argv[++i]);
  }
  int bad = 0;
  for (uint64_t s = a; s <= b; s++) { g_curSeed = s; bad += cityPerfSeed(s, people, secs); }
  printf("cityperf: %d failure(s)\n", bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--cityperf", "a huge city's crowd: streaming people in and out, NPC sleep LOD, step times [--seeds A..B] [--people N] [--secs S]",
             cmdCityPerf);
