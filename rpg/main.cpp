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
//                                                   witness.json): headless, deterministic, never saves; see rpg/app/witness_driver.cpp.
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
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "engine/pix.h"
#include "rpg/app/app.h"
#include "rpg/app/input_inject.h"
#include "rpg/app/save_store.h"
#include "rpg/app/script_runner.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"   // (main.cpp alone in the shell may: the seam below)
#include "rpg/view/screen.h"
#include "rpg/view/view.h"

namespace {
void newWorld(Game& g, uint64_t s) {
  g.newEndlessGame(s);
  if (!g.streamThreads) g.world.streamer.reset();   // (Witness) no prefetch worker: every chunk is made on demand, in order
}
}  // namespace

// (SOLID 3.1d) rpg/app/script_runner.h's seam to the sim's private helpers: rpg/sim/game_internal.h is main.cpp's alone
// (check_layers), so the script commands reach dlgReach and faceVec through these two
int dlgReach(Game& g, const std::function<bool(const DlgOpt&)>& match, bool open) { return gsim::dlgReach(g, match, open); }
Vec2 faceVec(int f) { return gsim::faceVec(f); }

App::App(AppSetup s, Pix& p, Audio& a, Game& g, View& v) : AppSetup(std::move(s)), pix(p), audio(a), game(g), view(v) {}

void App::startNew(uint64_t s) {
  recordIds = game.idCounter();
  newWorld(game, s);
  game.mode = Mode::Play;
  view.snap(game);
  audio.play(Sfx::QuestStart);
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
  recordFocusLost(pause);   // (WITNESS_RECORD) a recording lets go of all it holds
  if (pause) game.mode = Mode::Paused;
  if (!noSave) writeSave(game);
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
  frameEvents();
  frameRequests();

  Uint64 now = SDL_GetPerformanceCounter();
  double frameDt = std::min(0.1, (double)(now - prev) / (double)freq);
  prev = now;
  double wall = (double)(now - t0) / (double)freq;
  fpsT += frameDt; fpsN++;
  if (fpsT >= 1.0) perfSecond();

  Uint64 simT0 = 0, simT1 = 0;
  if (recordSync) recordSteps(frameDt, simT0, simT1);   // (WITNESS_RECORD: rpg/app/recorder.cpp)
  else frameSteps(frameDt, simT0, simT1);
  const Uint64 updT1 = SDL_GetPerformanceCounter();
  view.draw(game, hasSave);
  frameStats(work0, walkShifts0, simT0, simT1, updT1);
  frameShots(wall);
  pix.end();
  std::fflush(stdout);
}

// the events of the frame, through the view (F11: fullscreen; a lost focus pauses and saves)
void App::frameEvents() {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (!recordFilter(e)) continue;   // (WITNESS_RECORD: rpg/app/recorder.cpp)
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
}

// what the view asked for this frame: quit to the title, continue, a new adventure, the creator's hand-over, a save
void App::frameRequests() {
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
      recordCreatorStart();   // (WITNESS_RECORD) the recording begins with the new playthrough
    }
    prevMode = game.mode;
  }
  if (view.wantSave) { view.wantSave = false; if (!noSave) writeSave(game); }
}

// EMB_TIMING: the 12 frames after a journey's fade lifts, split into sim steps, view update and draw
Uint64 App::timingMark() {
  if (wasTravelling && !game.travelling()) afterTravel = 12;
  wasTravelling = game.travelling();
  return SDL_GetPerformanceCounter();
}

// the sim steps of a frame (fixed SIM_DT steps, at most 8) and the view's update
void App::frameSteps(double frameDt, Uint64& simT0, Uint64& simT1) {
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

// a script's `shot` of this frame, and --shot's screenshot after --after seconds (then the run ends)
void App::frameShots(double wall) {
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
}

int App::shutdown() {
  recordFinish();   // (WITNESS_RECORD) the end state beside the recording
  if (!noSave && game.mode != Mode::Title) writeSave(game);
  view.shutdown();
  audio.shutdown();
  pix.shutdown();
  SDL_Quit();
  recordStop();
  if (perf || scripted) printSummary();
  if (scripted) {
    // (P2c flip, D103: C2 over the sweep) the people frames drawn from v1 sheets over the run, by the rig they were drawn at
    std::printf("v1draws rig2 %lld rig1 %lld\n", view.v1DrawsRunAt(2), view.v1DrawsRunAt(1));
    std::printf("script: %d failure(s)\n", scriptFails);
    return scriptFails ? 3 : 0;
  }
  return 0;
}

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// the browser calls the frame once per animation frame; simulate_infinite_loop keeps main's stack frame (and the App
// it holds) alive
static void runLoop(App& app) {
  static App* loopApp = &app;
  emscripten_set_main_loop([] { loopApp->frame(); }, 0, 1);
}
#else
static void runLoop(App& app) { while (app.running) app.frame(); }
#endif

int main(int argc, char** argv) {
  AppSetup s;
  if (const int rc = s.parse(argc, argv); rc >= 0) return rc;
  chooseRig();   // EMB_LEGACY_OUTLINE, EMB_RIG, the web's ?rig=
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
  if (!(s.witness ? pix.init("EMBERVALE", 960, 540, false, witnessWindowHidden()) : pix.init("EMBERVALE", s.winW, s.winH, s.vsync))) return 1;
  if (s.witness) pix.markTextures();   // (a reset destroys every texture made after this: Pix::destroyUnmarked)
  if (s.watch) watchWindow(pix);   // (Witness watch mode) the window shown; real input never reaches gameplay
  static Audio audio;   // ~175 KB: keep it off the stack
  if (!s.witness) audio.init();
  openWebWitnessAudio(s.witness);   // (web witness mode) the page's AudioContext, fed silence

  s.startSeed = s.seed ? s.seed : s.witness ? 1 : (uint64_t)SDL_GetTicks() * 2654435761ull + 12345;
  Game game(s.startSeed);
  if (s.witness || s.recordSync) { game.streamThreads = false; game.syncStreams = true; }   // runs never depend on worker timing
  newWorld(game, s.startSeed);   // title screen drifts over this world
  game.mode = Mode::Title;
  View view;
  view.init(pix, audio);
  view.witness = s.witness || s.recordSync;
  if (touchPlatform()) view.touchUI = true;   // (iOS, Android)
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
  if (app.witness) app.runWitness();   // (rpg/app/witness_driver.cpp) the harness steps the game
  else runLoop(app);
  return app.shutdown();
}
