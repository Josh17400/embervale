// rpg/app/app_setup.cpp - before the frame loop (SOLID 3.1g): the command line (AppSetup::parse), the Witness flags,
// the human rig, the platform's few differences (the harness window, the web witness audio, touch-first devices) and
// the start --play asks for
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include "rpg/app/app.h"
#include "rpg/app/script_runner.h"
#include "rpg/art.h"
#ifdef EMB_WITNESS
#include "engine/witness.h"
#endif

#ifdef __EMSCRIPTEN__
namespace {
// (P2a) a whole-number query parameter of the page's URL (?rig=2: the owner's preview of the v2 people); 0 when absent
EM_JS(int, emb_url_int, (const char* name), {
  try {
    var v = new URLSearchParams(window.location.search).get(UTF8ToString(name));
    var n = parseInt(v, 10);
    return isNaN(n) ? 0 : n;
  } catch (e) { return 0; }
});
}  // namespace
#endif

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
  witness = witness_active(argc, argv);   // a Witness harness drives this run (rpg/app/witness_driver.cpp)
  // (P3 D) --witness-creator: New Game opens the creator under the harness too (App::wStep); without it the harness
  // keeps skipping the creator (playtests, the existing tests)
  for (int i = 1; i < argc; i++) if (!std::strcmp(argv[i], "--witness-creator")) wCreatorLaunch = true;
  wCreator = witness && wCreatorLaunch;
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

void chooseRig() {
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
}

// (Witness) a hidden 960x540 window (the 480x270 canvas at 2x), no vsync, no audio device (the mix is rendered per step).
// The web build's window is the page's canvas (SDL_WINDOW_FILL_DOCUMENT): shown, so the harness's browser lays it out
// as a phone would (witness doctor --device), and the canvas stays pinned at 480 x 270 (screen::init)
bool witnessWindowHidden() {
#ifdef __EMSCRIPTEN__
  return false;
#else
  return true;
#endif
}

void openWebWitnessAudio(bool witness) {
#ifdef __EMSCRIPTEN__
  // (Witness 0.7.1, cpp GUIDE section 13) web witness mode opens the audio device too, fed SILENCE (no callback, never
  // SDL_PutAudioStreamData): the page gets its AudioContext, so doctor --no-auto-unlock and web.audio.state can check
  // the iPhone unlock path. What listen hears still comes from witness_audio_write() in step (deterministic).
  if (witness && (SDL_WasInit(SDL_INIT_AUDIO) || SDL_InitSubSystem(SDL_INIT_AUDIO))) {
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 1, 44100};
    if (SDL_AudioStream* dev = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr))
      SDL_ResumeAudioStreamDevice(dev);   // kept open for the page's life
  }
#else
  (void)witness;
#endif
}

bool touchPlatform() {
#if defined(SDL_PLATFORM_IOS) || defined(SDL_PLATFORM_ANDROID)
  return true;
#else
  return false;
#endif
}

void App::startPlay() {
  if (play && recordSync) recordPlayStart();   // (WITNESS_RECORD) the start a replay rebuilds: rpg/app/recorder.cpp
  else if (play) {
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
