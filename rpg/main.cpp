// EMBERVALE - SDL3 platform layer: window, events, fixed-timestep loop, autosave.
//   embervale.exe                                   play (autosaves; title offers Continue)
//   embervale.exe --play --seed 7                   skip the title, fresh world (never touches your save)
//   embervale.exe --play --goto city|town|cave|ruin|camp|lair [--enter] [--hour 22] [--menu 0-4]
//                 --shot out.png --after 2           screenshot after N seconds, then quit
//   embervale.exe --script file.txt [--seed 7] [--play]   drive the game from a timed script (test only, never saves)
//   embervale.exe --play --at 4100,-2300            (M1) start on that GLOBAL tile of the endless world
//   --perf                                          once a second: fps, frame work ms (avg / worst), NPCs awake and
//                                                   asleep, hostiles, active sites, chunks/s, worst shift ms, regions,
//                                                   settlements built; at exit a summary (worst frame at a window shift)
//   embervale.exe --witness                         (desktop, EMBERVALE_WITNESS) driven by the Witness test harness over a
//                                                   local socket (WITNESS_ADDR, set by `witness test|doctor|shot|...` from
//                                                   witness.json): headless, deterministic, never saves; see "Witness" below.
//                                                   WITNESS_WATCH=1 (`witness play start --watch`) shows the window and
//                                                   presents every step; real input is dropped; the sim is unchanged
//   WITNESS_RECORD=file.jsonl embervale.exe         normal play records a replay (one line per sim step, F8 marks a
//                                                   moment) that `witness replay` re-simulates exactly (the session runs
//                                                   as witness mode does: no stream worker, inline bakes, the 480 x 270
//                                                   canvas); at exit file.jsonl.state.json holds the end state to check.
//                                                   Keys, mouse, wheel and fingers are recorded (a system-ended finger as
//                                                   a cancel, with its pressure); a lost focus lets go of all that is held
//                                                   (fingers as cancels) and records its pause (tools/scripts/
//                                                   record/witness_record_touch.txt checks a touch session end to end)
//
// The test-script format and its commands: rpg/app/script_runner.h.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <functional>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "engine/pix.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"

#include "rpg/sim/gear.h"
#include "rpg/sim/loot_ops.h"
#include "rpg/view/screen.h"
#include "rpg/view/script_api.h"
#include "rpg/view/skills_ui.h"
#include "rpg/view/view.h"
#include "rpg/sim/stream.h"
#include "rpg/world/source.h"

#include "rpg/app/app.h"
#include "rpg/app/input_inject.h"
#include "rpg/app/save_store.h"
#include "rpg/app/script_runner.h"

#ifdef EMB_WITNESS
#include <map>
#include <new>
#include "engine/witness.h"
#include "rpg/view/witness_state.h"
#include "rpg/sim/anim.h"   // (P2a) AnimOps::refreshHero (the reset's female)
#endif

namespace {
void newWorld(Game& g, uint64_t s) {
  g.newEndlessGame(s);
  if (!g.streamThreads) g.world.streamer.reset();   // (Witness) no prefetch worker: every chunk is made on demand, in order
}



#ifdef __EMSCRIPTEN__
// (P2a) a whole-number query parameter of the page's URL (?rig=2: the owner's preview of the v2 people); 0 when absent
EM_JS(int, emb_url_int, (const char* name), {
  try {
    var v = new URLSearchParams(window.location.search).get(UTF8ToString(name));
    var n = parseInt(v, 10);
    return isNaN(n) ? 0 : n;
  } catch (e) { return 0; }
});
#endif

}  // namespace

// (SOLID 3.1d) rpg/app/script_runner.h's seam to the sim's private helpers: rpg/sim/game_internal.h is main.cpp's alone
// (check_layers), so the script commands reach dlgReach and faceVec through these two
int dlgReach(Game& g, const std::function<bool(const DlgOpt&)>& match, bool open) { return gsim::dlgReach(g, match, open); }
Vec2 faceVec(int f) { return gsim::faceVec(f); }



int AppSetup::parse(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--novsync")) vsync = false;
    else if (!std::strcmp(argv[i], "--perf")) perf = true;
    else if (!std::strcmp(argv[i], "--play")) play = true;
    else if (!std::strcmp(argv[i], "--enter")) enter = true;
    else if (!std::strcmp(argv[i], "--god")) god = true;
    else if (!std::strcmp(argv[i], "--shot") && i + 1 < argc) shotPath = argv[++i];
    else if (!std::strcmp(argv[i], "--after") && i + 1 < argc) shotAfter = (float)std::atof(argv[++i]);
    else if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = (uint64_t)std::atoll(argv[++i]);
    else if (!std::strcmp(argv[i], "--goto") && i + 1 < argc) gotoWhat = argv[++i];
    else if (!std::strcmp(argv[i], "--hour") && i + 1 < argc) hourSet = (float)std::atof(argv[++i]);
    else if (!std::strcmp(argv[i], "--menu") && i + 1 < argc) menuTab = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--fight") && i + 1 < argc) fight = argv[++i];
    else if (!std::strcmp(argv[i], "--talk") && i + 1 < argc) talkStep = std::atoi(argv[++i]);   // PT test-only
    else if (!std::strcmp(argv[i], "--script") && i + 1 < argc) scriptPath = argv[++i];
    else if (!std::strcmp(argv[i], "--script-help")) {
      std::printf("script commands registered by the lanes (rpg/view/script_api.h):\n");
      for (const ScriptExt& e : scriptExts()) std::printf("  %-22s %s\n", e.name, e.help);
      return 0;
    }
    else if (!std::strcmp(argv[i], "--at") && i + 1 < argc) {
      int x = 0, y = 0;
      if (std::sscanf(argv[++i], "%d,%d", &x, &y) == 2) { atSet = true; atX = x; atY = y; }
    }
    else if (!std::strcmp(argv[i], "--window") && i + 1 < argc) {
      int w = 0, h = 0;
      if (std::sscanf(argv[++i], "%dx%d", &w, &h) == 2 && w >= 160 && h >= 90) { winW = w; winH = h; windowSet = true; }
    }
  }
  if (scriptPath) {
    if (!loadScript(scriptPath, script, seed)) { std::printf("script %s: cannot open\n", scriptPath); return 2; }
    std::printf("script %s: %zu commands, seed %llu\n", scriptPath, script.size(), (unsigned long long)seed);
    if (g_scriptPhone) {   // the landscape iPhone 15 Pro: 2556x1179, fill, its safe-area insets (639x294 logical)
      if (!windowSet) { winW = 2556; winH = 1179; }
      setEnvIfUnset("EMB_SCREEN", "fill");
      setEnvIfUnset("EMB_SAFE", "177,0,177,63");   // (M6 fixer r2) device px at 3x: 59,0,59,21 points
    }
  }
  scripted = scriptPath != nullptr;
  return -1;
}

void AppSetup::witnessFlags(int argc, char** argv) {
#ifdef EMB_WITNESS
  witness = witness_active(argc, argv);   // a Witness harness drives this run (see "Witness" below)
  // WITNESS_WATCH=1 (`witness play start --watch`, Witness PROTOCOL.md section 9): a person watches the harness play.
  // The window is shown and every step is presented; real input is dropped; nothing else changes (no pacing: the
  // harness paces, and the sim is the headless one)
  const char* watchEnv = std::getenv("WITNESS_WATCH");
  watch = witness && watchEnv && std::strcmp(watchEnv, "1") == 0;
  // WITNESS_RECORD=<file.jsonl> (normal play, not a harness run): the session must replay step for step, so it runs
  // as witness mode does: no stream worker (Game::streamThreads off, syncStreams on), the view's inline bakes and the
  // sim-ended arrival (View::witness), the 480 x 270 canvas, and one view update per sim step (see the frame loop)
  recordEnv = std::getenv("WITNESS_RECORD");
  recordSync = !witness && recordEnv && *recordEnv;
#else
  (void)argc; (void)argv;
#endif
}

App::App(AppSetup s, Pix& p, Audio& a, Game& g, View& v) : AppSetup(std::move(s)), pix(p), audio(a), game(g), view(v) {}



void App::startNew(uint64_t s) {
#ifdef EMB_WITNESS
  recordIds = game.idCounter();
#endif
  newWorld(game, s);
  game.mode = Mode::Play;
  view.snap(game);
  audio.play(Sfx::QuestStart);
}


void App::startPlay() {
#ifdef EMB_WITNESS
  if (play && recordSync) {
    StartSpec sp;
    sp.god = god; sp.enter = enter; sp.hour = hourSet; sp.atSet = atSet; sp.atX = atX; sp.atY = atY;
    sp.talk = talkStep; sp.menu = menuTab;
    if (gotoWhat) sp.scene = gotoWhat;
    if (fight) sp.fight = fight;
    applyStart(startSeed, sp);
    sp.ids = recordIds;
    recordBegin(specJson(sp));
  } else
#endif
  if (play) {
    startNew(startSeed);
    game.godMode = god;
    if (hourSet >= 0) game.hour = hourSet;
    if (atSet) { game.teleportGlobal(atX, atY); view.snap(game); }
    if (gotoWhat) doGoto(gotoWhat, enter);
    if (hourSet >= 0) game.hour = hourSet;   // (a fast travel passes the hours of the journey: the hour asked for wins)
    if (talkStep >= 0) doTalk(talkStep);
    view.snap(game);
    if (menuTab >= 0) view.openMenu(game, menuTab);
    if (fight) doFight(fight, 0);
  }
}

void App::startClock() {
  freq = SDL_GetPerformanceFrequency();
  prev = SDL_GetPerformanceCounter();
  t0 = prev;
}







// the window lost the focus (or was hidden; a script's `blur` forces it): play pauses (not under --shot or a script,
// whose windows often sit behind others), and the game is saved (phones kill backgrounded web apps without warning)

void App::focusLost(bool force) {
  const bool pause = game.mode == Mode::Play && (force || (!shotPath && !scripted));
#ifdef EMB_WITNESS
  if (witness_recording()) recLetGo(pause);
#endif
  if (pause) game.mode = Mode::Paused;
  if (!noSave) writeSave(game);
}







// M2: commands registered through rpg/view/script_api.h. A command that returns false is retried every frame (script
// time stands still) until it returns true or its timeout passes.






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

// one frame of the game; desktop loops on it, the browser calls it once per animation frame

void App::frame() {
  const Uint64 work0 = SDL_GetPerformanceCounter();
  const int walkShifts0 = game.world.sstats.walkShifts;
  screen::frame(pix);
  game.frameWork(3.0);   // M1 streaming: collect the worker's chunks (native) or generate within ~3 ms (web)
  // NPC LOD: off screen is beyond the view (P2a: the world view, in world px, at the zoom in use)
  game.sleepHalfW = std::max(22.0f, view.viewW() * 0.5f / TILE + 4.0f);
  game.sleepHalfH = std::max(13.0f, view.viewH() * 0.5f / TILE + 4.0f);
  if (scripted) {
    Uint64 nowS = SDL_GetPerformanceCounter();
    static Uint64 prevS = nowS;
    float dtS = (float)std::min(0.1, (double)(nowS - prevS) / (double)freq);
    prevS = nowS;
    scriptStep(dtS);
  }
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
#ifdef EMB_WITNESS
    // a key's auto-repeat is not recorded (a replay presses each key once), so it does not reach the view either
    if (recordSync && (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) && e.key.repeat) continue;
    if (witness_recording() && !recordEvent(e)) continue;   // (a late release of what a lost focus let go of)
#endif
    switch (e.type) {
      case SDL_EVENT_QUIT: running = false; break;
      case SDL_EVENT_WILL_ENTER_BACKGROUND: if (!noSave) writeSave(game); break;
      case SDL_EVENT_WINDOW_FOCUS_LOST: case SDL_EVENT_WINDOW_HIDDEN: focusLost(false); break;
      case SDL_EVENT_KEY_DOWN:
        if (e.key.key == SDLK_F11) {
          bool fs = (SDL_GetWindowFlags(pix.window()) & SDL_WINDOW_FULLSCREEN) != 0;
          SDL_SetWindowFullscreen(pix.window(), !fs);
          break;
        }
        view.event(e, game);
        break;
      default: view.event(e, game); break;
    }
  }
  if (view.wantsQuit) {
    if (game.mode == Mode::Title) running = false;
    else { if (!noSave) writeSave(game); hasSave = !noSave; game.mode = Mode::Title; view.wantsQuit = false; }
  }
  if (view.wantContinue) {
    view.wantContinue = false;
    std::vector<uint8_t> buf;
    if (readSave(buf) && game.deserialize(buf)) { game.mode = Mode::Play; view.snap(game); audio.play(Sfx::Discover); }
    else startNew(seed ? seed : (uint64_t)SDL_GetTicks() * 2654435761ull + 777);
  }
  // NEW ADVENTURE: making the world takes a few hundred ms on a phone, so the "FORGING THE WORLD" card is put on
  // screen first (this frame) and the world is made at the top of the next one, behind the card
  static int newGameIn = 0;
  if (newGameIn > 0 && --newGameIn == 0) {
    startNew(seed ? seed : (uint64_t)SDL_GetTicks() * 2654435761ull + 777);   // --seed / a script's seed: reproducible
    game.beginCreator();   // the character creator; the first save happens when it hands over to play
    view.creatorPrepare(game);   // (its homeland choices read the cultures round the start: behind the card too)
    view.loadingCard.clear();
  }
  if (view.wantNewGame) {
    view.wantNewGame = false;
    view.loadingCard = "FORGING THE WORLD";
    newGameIn = 2;
  }
  {   // the creator just finished: save the new character right away
    static Mode prevMode = Mode::Title;
    if (prevMode == Mode::Creator && game.mode == Mode::Play) {
      if (!noSave) writeSave(game);
      hasSave = !noSave;
      view.snap(game);
#ifdef EMB_WITNESS
      {
        StartSpec sp;
        sp.creator = true; sp.background = (int)game.background; sp.name = game.app.name;
        sp.ids = recordIds;   // the title world handed out ids before this one began: the replay's fresh Game starts there
        recordBegin(specJson(sp));
        rec = RecHeld();   // (a new playthrough's recording holds nothing yet)
        wStart = WitnessExtra();
        wStart.hasStart = !game.inside;
        wStart.startGX = game.world.ox + (int)std::floor(game.pl().p.x / TILE);
        wStart.startGY = game.world.oy + (int)std::floor((game.pl().p.y - 2) / TILE);
      }
#endif
    }
    prevMode = game.mode;
  }
  if (view.wantSave) { view.wantSave = false; if (!noSave) writeSave(game); }

  Uint64 now = SDL_GetPerformanceCounter();
  double frameDt = std::min(0.1, (double)(now - prev) / (double)freq);
  prev = now;
  double wall = (double)(now - t0) / (double)freq;
  fpsT += frameDt; fpsN++;
  if (fpsT >= 1.0) {
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

  // EMB_TIMING: the 12 frames after a journey's fade lifts, split into sim steps, view update and draw
  static int afterTravel = 0;
  static bool wasTravelling = false;
  auto timingMark = [&]() {
    if (wasTravelling && !game.travelling()) afterTravel = 12;
    wasTravelling = game.travelling();
    return SDL_GetPerformanceCounter();
  };
  Uint64 simT0 = 0, simT1 = 0;
#ifdef EMB_WITNESS
  if (recordSync) {
    simT0 = timingMark();
    // WITNESS_RECORD: every recorded step is the replay's step (simStep: input, sim, view update), so a replay
    // re-runs the session call for call; menus and dialogue step once a frame at its dt, a lost-focus pause not at all
    if (game.mode == Mode::Play) {
      acc += frameDt;
      int steps = 0;
      while (acc >= SIM_DT && steps < 8) {
        simStep(SIM_DT);
        witness_record_frame(SIM_DT);
        acc -= SIM_DT; steps++;
      }
      if (steps == 8) acc = 0;
      saveT += (float)frameDt;
      if (!noSave && saveT > 20.0f) { saveT = 0; writeSave(game); }
    } else {
      acc = 0;
      if (game.mode == Mode::Title) { game.time += (float)frameDt; view.update(game, (float)frameDt); }
      else if (game.mode != Mode::Paused) { simStep((float)frameDt); witness_record_frame(frameDt); }
    }
    simT1 = SDL_GetPerformanceCounter();
  } else
#endif
  {
  Input in = view.input(game);
  // one-shot presses must survive render frames in which no sim step runs (on 120/144 Hz displays, about half
  // of all frames): the view clears them every frame, so carry them until a step actually consumes them
  static Input latched;
  in.attack |= latched.attack; in.bow |= latched.bow; in.spell |= latched.spell; in.roll |= latched.roll;
  in.interact |= latched.interact; in.potion |= latched.potion; in.swapSpell |= latched.swapSpell;
  // (M9) the quick-cast press and the sneak toggle are one-shots too; a dual-cast's release (castDual with castHold
  // off) must reach a step as well
  if (in.castSlot < 0) in.castSlot = latched.castSlot;
  in.sneak |= latched.sneak;
  if (!in.castHold) in.castDual |= latched.castDual && !latched.castHold;
  latched = Input();
  simT0 = timingMark();
  if (game.mode == Mode::Play) {
    acc += frameDt;
    int steps = 0;
    bool first = true;
    while (acc >= SIM_DT && steps < 8) {
      game.update(SIM_DT, in);
      if (first) { in.attack = in.bow = in.spell = in.roll = in.interact = in.potion = in.swapSpell = false; in.castSlot = -1; in.sneak = false; first = false; }
      acc -= SIM_DT; steps++;
    }
    if (steps == 0) { latched = in; latched.move = Vec2(); }
    if (steps == 8) acc = 0;
    saveT += (float)frameDt;
    if (!noSave && saveT > 20.0f) { saveT = 0; writeSave(game); }
  } else {
    acc = 0;
    if (game.mode == Mode::Title) game.time += (float)frameDt;
    // (P2b B11) under the death screen the sim steps only the hero's fall (Game::update: stT, then it returns)
    else if (game.mode == Mode::Dead) game.update((float)frameDt, Input());
  }
  simT1 = SDL_GetPerformanceCounter();
  view.update(game, (float)frameDt);
  }
  const Uint64 updT1 = SDL_GetPerformanceCounter();
  view.draw(game, hasSave);
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

  if (!pendingShot.empty()) {
    bool ok = pix.screenshot(pendingShot.c_str());
    std::printf("screenshot %s: %s\n", pendingShot.c_str(), ok ? "ok" : "FAILED");
    if (!ok) scriptFails++;
    pendingShot.clear();
  }
  if (shotPath && !scripted && !shotTaken && wall >= shotAfter) {
    shotTaken = pix.screenshot(shotPath);
    std::printf("screenshot %s: %s\n", shotPath, shotTaken ? "ok" : "FAILED");
    running = false;
  }
  pix.end();
  std::fflush(stdout);
}


int App::shutdown() {
#ifdef EMB_WITNESS
  if (recordSync && witness_recording()) {
    // the state the recording ended in, next to it (<file>.state.json, the harness's snapshot): a replay of the file
    // can be checked against it for an exact match (`witness replays` lists only the .jsonl)
    WitnessExtra x = wStart;
    x.scriptBusy = false;   // (a harness replay has no script of its own)
    x.savesWritten = g_savesWritten;
    x.saveEnabled = !noSave;
    const std::string st = witnessJsonString([&](witness::Json& j) { witnessState(j, game, view, x); });
    if (FILE* f = std::fopen((std::string(recordEnv) + ".state.json").c_str(), "wb")) {
      std::fwrite(st.data(), 1, st.size(), f);
      std::fclose(f);
    }
  }
#endif
  if (!noSave && game.mode != Mode::Title) writeSave(game);
  view.shutdown();
  audio.shutdown();
  pix.shutdown();
  SDL_Quit();
#ifdef EMB_WITNESS
  witness_record_stop();
#endif
  if (perf || scripted) printSummary();
  if (scripted) {
    // (P2c flip, D103: C2 over the sweep) the people frames drawn from v1 sheets over the run, by the rig they were drawn at
    std::printf("v1draws rig2 %lld rig1 %lld\n", view.v1DrawsRunAt(2), view.v1DrawsRunAt(1));
    std::printf("script: %d failure(s)\n", scriptFails);
    return scriptFails ? 3 : 0;
  }
  return 0;
}


int main(int argc, char** argv) {
  AppSetup s;
  if (const int rc = s.parse(argc, argv); rc >= 0) return rc;
  // (P2a test hook) EMB_LEGACY_OUTLINE=1: the character sheets with the pre-P2a selective outline, so a run's shots can be
  // compared byte for byte with a pre-P2a build (the ink outline is phase A's only intended pixel change)
  if (std::getenv("EMB_LEGACY_OUTLINE")) art::outlineLegacyForTests() = true;
  // EMB_RIG=1|2: the human rig, set before the first sheet is baked. (P2c flip, D100) rig 2, the doll, is the default
  // (art::kDefaultRig); 1 opts into the v1 fallback until the owner's retirement OK, and 2 is still accepted
  if (const char* e = std::getenv("EMB_RIG")) art::setRigVersion(std::atoi(e));
#ifdef __EMSCRIPTEN__
  if (const int r = emb_url_int("rig"); r > 0) art::setRigVersion(r);   // the web's ?rig=1 (the fallback) or ?rig=2
#endif
  if (art::rigVersion() == 1) std::printf("rig 1: the v1 people, an opt-in fallback until the owner's retirement OK\n");
  s.witnessFlags(argc, argv);
  s.noSave = s.play || s.shotPath || s.seed != 0 || s.scripted || s.witness;   // test runs never touch the player's save

  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
  SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
  SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) { SDL_Log("SDL_Init failed: %s", SDL_GetError()); return 1; }
  g_savePath = userFilePath("save.bin");
  if (s.scripted || s.witness) g_savePath.clear();   // belt and braces: a script or harness never reads or writes the real save
  // test hook: read the title's save from another file (e.g. an old-format save, to see the title's notice). Test runs
  // (--shot, --play, --seed) still never write it.
  s.saveOverride = std::getenv("EMB_SAVE_PATH");
  if (s.saveOverride && *s.saveOverride && !s.scripted && !s.witness) g_savePath = s.saveOverride;

  Pix pix;
  // (Witness) a hidden 960x540 window (the 480x270 canvas at 2x), no vsync, no audio device (the mix is rendered per step).
  // The web build's window is the page's canvas (SDL_WINDOW_FILL_DOCUMENT): shown, so the harness's browser lays it out
  // as a phone would (witness doctor --device), and the canvas stays pinned at 480 x 270 (screen::init below)
#ifdef __EMSCRIPTEN__
  constexpr bool kWitnessHidden = false;
#else
  constexpr bool kWitnessHidden = true;
#endif
  if (!(s.witness ? pix.init("EMBERVALE", 960, 540, false, kWitnessHidden) : pix.init("EMBERVALE", s.winW, s.winH, s.vsync))) return 1;
  if (s.witness) pix.markTextures();   // (a reset destroys every texture made after this: Pix::destroyUnmarked)
  if (s.watch) watchWindow(pix);   // (Witness watch mode) the window shown; real input never reaches gameplay
  static Audio audio;   // ~175 KB: keep it off the stack
  if (!s.witness) audio.init();
#ifdef __EMSCRIPTEN__
  // (Witness 0.7.1, cpp GUIDE section 13) web witness mode opens the audio device too, fed SILENCE (no callback, never
  // SDL_PutAudioStreamData): the page gets its AudioContext, so doctor --no-auto-unlock and web.audio.state can check
  // the iPhone unlock path. What listen hears still comes from witness_audio_write() in step (deterministic).
  if (s.witness && (SDL_WasInit(SDL_INIT_AUDIO) || SDL_InitSubSystem(SDL_INIT_AUDIO))) {
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 1, 44100};
    if (SDL_AudioStream* dev = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr))
      SDL_ResumeAudioStreamDevice(dev);   // kept open for the page's life
  }
#endif

  s.startSeed = s.seed ? s.seed : s.witness ? 1 : (uint64_t)SDL_GetTicks() * 2654435761ull + 12345;
  Game game(s.startSeed);
  if (s.witness || s.recordSync) { game.streamThreads = false; game.syncStreams = true; }   // runs never depend on worker timing
  newWorld(game, s.startSeed);   // title screen drifts over this world
  game.mode = Mode::Title;
  View view;
  view.init(pix, audio);
  view.witness = s.witness || s.recordSync;
#if defined(SDL_PLATFORM_IOS) || defined(SDL_PLATFORM_ANDROID)
  view.touchUI = true;
#endif
  s.touchUIStart = view.touchUI;   // (a Witness reset puts it back: a test's finger turns the controls on)
  {   // M1: screen fit and the player's settings (a small file of its own next to the save; scripts use defaults)
    std::string settingsPath;
    settingsPath = userFilePath("settings.bin");
    if (s.scripted || s.witness) settingsPath.clear();
    // (Witness, WITNESS_RECORD) the canvas pinned at 480 x 270: EMB_SCREEN / EMB_BORDER / EMB_HUD / EMB_SAFE and the
    // settings file never change what a run sees (the canvas size feeds the NPC sleep box: Game::sleepHalfW)
    screen::init(pix, settingsPath, view.touchUI, s.witness || s.recordSync);
  }

  App app(std::move(s), pix, audio, game, view);
  app.probeSave();
  app.startPlay();
  app.startClock();
#ifdef EMB_WITNESS
  if (app.witness) app.runWitness();
  else
#endif
  {
#ifdef __EMSCRIPTEN__
    // simulate_infinite_loop keeps main's stack frame (and the App it holds) alive
    static App* loopApp = &app;
    emscripten_set_main_loop([] { loopApp->frame(); }, 0, 1);
#else
    while (app.running) app.frame();
#endif
  }
  return app.shutdown();
}
