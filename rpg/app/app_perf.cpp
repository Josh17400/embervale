// rpg/app/app_perf.cpp - the frame's measurements (SOLID 3.1g): --perf's line every second, EMB_TIMING's slow and
// after-arrival frames, the run's worst frames and the summary at exit (--perf or a script)
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "rpg/app/app.h"
#include "rpg/sim/stream.h"
#include "rpg/world/source.h"

// --perf / script summary (M1): the CPU work of each frame (everything before the present), the worst one, and the
// worst one in which the endless window shifted under a walking player (the "no hitch at shifts" check)
long long App::chunksMade() {
  long long n = 0;
  if (game.world.src) n += game.world.src->stats().chunks;
  if (game.world.streamer) n += game.world.streamer->stats().chunksMade;
  return n;
}

void App::printSummary() {
  const World::StreamStats& ss = game.world.sstats;
  std::printf("perf summary: %d frames, worst frame work %.1f ms, worst frame with a walking window shift %.1f ms (%d such frames), "
              "worst in the second after one %.1f ms; "
              "shifts %d (walking %d), worst shift %.1f ms, worst recentre %.1f ms, chunks prefetched %d, generated in a move %d "
              "(%d in walking shifts), records recycled %d\n",
              pacc.frames, pacc.worstAll, pacc.worstShift, pacc.shiftFrames, pacc.worstAfterShift, ss.shifts, ss.walkShifts, ss.worstShiftMs, ss.worstRecentreMs,
              ss.prefetched, ss.syncChunks, ss.syncInShifts, ss.recycled);
}

// once a second: --perf's line (fps, frame work, actors, streaming, the slowest world systems, the draw)
void App::perfSecond() {
  if (perf) {
    const World::StreamStats& ss = game.world.sstats;
    long long ch = chunksMade();
    int regions = 0, settlements = 0;
    if (game.world.src) { regions = game.world.src->stats().regions; settlements = game.world.src->stats().settlements; }
    if (game.world.streamer) regions += game.world.streamer->stats().regionsMade;
    std::printf("perf fps=%d work avg %.2f ms worst %.2f ms | actors %zu npc awake %d asleep %d hostiles %d active sites %d | "
                "chunks/s %lld worst shift %.1f ms (sync %d) recentre %.1f ms | regions %d settlements %d | window %d,%d mode %d\n",
                (int)(fpsN / fpsT + 0.5), fpsN ? pacc.workSum / fpsN : 0.0, pacc.workWorst, game.actors.size(),
                game.perf.npcAwake, game.perf.npcAsleep, game.perf.hostiles, game.perf.activeSites, (long long)((ch - pacc.chunks0) / fpsT),
                ss.worstShiftMs, ss.syncInShifts, ss.worstRecentreMs, regions, settlements, game.world.ox, game.world.oy, (int)game.mode);
    pacc.chunks0 = ch;
    // (M6 fixer) the worst step of each world subsystem this second (only the ones over 2 ms: a hitch's suspects)
    static const char* const sysName[Game::PerfCounters::S_COUNT] = {"quests", "realm", "war", "story", "lifeStep", "raids", "foes", "lifeTick", "spawning", "home", "party"};
    std::string slow;
    for (int k = 0; k < Game::PerfCounters::S_COUNT; k++) {
      if (game.perf.sysWorstMs[k] >= 2.0) {
        char b[64];
        std::snprintf(b, sizeof b, " %s %.1f", sysName[k], game.perf.sysWorstMs[k]);
        slow += b;
      }
      game.perf.sysWorstMs[k] = 0;
    }
    if (!slow.empty()) std::printf("perf   worst step ms:%s\n", slow.c_str());
    std::printf("perf   draw avg %.2f ms worst %.2f ms | people textures %.1f MB\n", fpsN ? pacc.drawSum / fpsN : 0.0, pacc.drawWorst, view.peopleMB());   // (P2a)
    if (art::rigVersion() >= 2)   // (P2c SWITCH, C8) the rig-2 atlas cache: its MB and cap, evictions, atlases baked again after one
      std::printf("perf   rig2 cache %.1f MB cap %.0f | evictions %d rebakes %d | slowest ACTION atlas %.2f ms\n", view.lensStats.rigMB, view.rigCapMB(),
                  view.lensStats.rigEvictions, view.lensStats.rigRebakes, view.lensStats.actionMsMax);
  }
  pacc.workSum = 0; pacc.workWorst = 0;
  pacc.drawSum = 0; pacc.drawWorst = 0;
  fpsT = 0; fpsN = 0;
}

// the draw's time, EMB_TIMING's frames, and the frame's CPU work (the worst ones, and those at a walking window shift)
void App::frameStats(Uint64 work0, int walkShifts0, Uint64 simT0, Uint64 simT1, Uint64 updT1) {
  {   // (P2a) the draw's own time (the --perf render baseline)
    const double dms = (double)(SDL_GetPerformanceCounter() - updT1) * 1000.0 / (double)freq;
    pacc.drawSum += dms;
    pacc.drawWorst = std::max(pacc.drawWorst, dms);
  }
  {
    static const bool timing = std::getenv("EMB_TIMING") != nullptr;
    auto ms = [&](Uint64 a, Uint64 b) { return (double)(b - a) * 1000.0 / (double)freq; };
    const bool slow = timing && ms(work0, SDL_GetPerformanceCounter()) > 15.0;   // (and any frame over 15 ms)
    const bool after = afterTravel > 0;
    if (after) afterTravel--;
    if (timing && (after || slow))
      std::printf("%s frame %d: pre %.2f ms, sim %.2f ms, view update %.2f ms, draw %.2f ms (npc awake %d)\n", after ? "after arrival" : "slow", 12 - afterTravel,
                  ms(work0, simT0), ms(simT0, simT1), ms(simT1, updT1), ms(updT1, SDL_GetPerformanceCounter()), game.perf.npcAwake);
  }
  {   // the frame's CPU work: everything before the present (vsync waits) and before a test screenshot (PNG writing)
    double workMs = (double)(SDL_GetPerformanceCounter() - work0) * 1000.0 / (double)freq;
    pacc.frames++;
    pacc.workSum += workMs;
    pacc.workWorst = std::max(pacc.workWorst, workMs);
    if (pacc.frames > 30) pacc.worstAll = std::max(pacc.worstAll, workMs);   // (not the first frames: start-up)
    if (game.world.sstats.walkShifts != walkShifts0) { pacc.shiftFrames++; pacc.worstShift = std::max(pacc.worstShift, workMs); pacc.afterShift = 60; }
    else if (pacc.afterShift > 0) { pacc.afterShift--; pacc.worstAfterShift = std::max(pacc.worstAfterShift, workMs); }
  }
}
