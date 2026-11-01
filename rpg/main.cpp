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

#ifdef EMB_WITNESS
// WITNESS_RECORD=<file.jsonl> in normal play: a replay `witness replay` re-simulates, one line per sim step. It begins
// when play does (--play, or the creator handing over); the header holds the seed the world used and the params the
// harness's reset needs to rebuild that start (scene, the creator's background)

void App::recordBegin(const std::string& params) {
  if (!witness) witness_record_begin("Embervale", 60, (long long)game.seed, params.c_str());
}

std::string App::jsonStr(const std::string& v) {
  std::string r = "\"";
  for (char ch : v) { if (ch == '"' || ch == '\\') r += '\\'; if ((unsigned char)ch >= 0x20) r += ch; }
  return r + "\"";
}

#endif
// test helpers shared by the command-line flags and the script commands



// test helper: a full armour set of one band (1 leather .. 7 emberforged), equipped, plus a weapon and a bow.
// (M8b, LootSrc::Debug) made by the gear rules at the band's middle item level (never the classic makers): the armour,
// weapon and bow Common, the ring and amulet Uncommon (a jewel's worth is its affix), so every piece passes lootValid



#ifdef EMB_WITNESS
// (Witness) how a run starts: a harness reset's params, or a recorded --play session's flags (its replay header).
// Both go through applyStart, so a replay begins exactly where the recording did: a brand-new Game (no timer, id
// counter or cache of an earlier world), then the steps --play takes, in its order

void App::applyStart(uint64_t s, const StartSpec& sp) {
  game.~Game();
  new (&game) Game(s);
  game.streamThreads = false;
  game.syncStreams = true;
  if (sp.ids > 0) game.setIdCounter(sp.ids);
  startNew(s);
  if (sp.creator) {   // a recorded new game: the creator's choices that change play
    game.app.name = sp.name;
    game.background = (Background)sp.background;
    game.finishCreator();
  }
  game.godMode = sp.god;
  if (sp.hour >= 0) game.hour = sp.hour;
  if (sp.atSet) { game.teleportGlobal(sp.atX, sp.atY); view.snap(game); }
  if (!sp.scene.empty()) doGoto(sp.scene, sp.enter);
  if (sp.hour >= 0) game.hour = sp.hour;   // (a fast travel passes the hours of the journey: the hour asked for wins)
  if (sp.talk >= 0) doTalk(sp.talk);
  view.snap(game);
  if (sp.menu >= 0) view.openMenu(game, sp.menu);
  if (!sp.fight.empty()) doFight(sp.fight, sp.count);
  wStart = WitnessExtra();
  wStart.hasStart = !game.inside;
  wStart.startGX = game.world.ox + (int)std::floor(game.pl().p.x / TILE);
  wStart.startGY = game.world.oy + (int)std::floor((game.pl().p.y - 2) / TILE);
}

std::string App::specJson(const StartSpec& sp) {
  std::string p = "{\"god\":" + std::string(sp.god ? "true" : "false");
  if (!sp.scene.empty()) p += ",\"scene\":" + jsonStr(sp.scene) + ",\"enter\":" + (sp.enter ? "true" : "false");
  if (sp.hour >= 0) { char b[32]; std::snprintf(b, sizeof b, "%.9g", sp.hour); p += ",\"hour\":" + std::string(b); }   // (exact as a float)
  if (sp.atSet) p += ",\"at\":[" + std::to_string(sp.atX) + "," + std::to_string(sp.atY) + "]";
  if (sp.talk >= 0) p += ",\"talk\":" + std::to_string(sp.talk);
  if (sp.menu >= 0) p += ",\"menu\":" + std::to_string(sp.menu);
  if (!sp.fight.empty()) p += ",\"fight\":" + jsonStr(sp.fight) + ",\"count\":" + std::to_string(sp.count);
  if (sp.creator) p += ",\"creator\":true,\"background\":" + std::to_string(sp.background) + ",\"name\":" + jsonStr(sp.name);
  // (what the window decides, state.screen and state.player.sprite, differs between the player's window and the
  // replay's hidden one: witness.json "replay": {"ignore": [...]} leaves it out of the end-state check)
  return p + ",\"ids\":" + std::to_string(sp.ids > 0 ? sp.ids : 1) + "}";
}

#endif

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







#ifdef EMB_WITNESS
// (WITNESS_RECORD) what the recording holds: every key, mouse button and finger it recorded as pressed and not yet as
// released. A lost focus lets go of all of it (recLetGo below), and the late release the platform may still send for
// one of those afterwards is dropped, so neither the view nor the replay sees a release twice.
// window px -> the logical px the view sees under Witness (View::witnessSnap: the replay re-creates them exactly)

void App::recLogical(float wx, float wy, float& lx, float& ly) {
  pix.windowToLogical(wx, wy, lx, ly);
  lx = View::witnessSnap(lx);
  ly = View::witnessSnap(ly);
}

// as View::event converts a finger
void App::recFinger(const SDL_TouchFingerEvent& t, float& lx, float& ly) {
  int ww = 1, wh = 1;
  SDL_GetWindowSize(pix.window(), &ww, &wh);
  recLogical(t.x * ww, t.y * wh, lx, ly);
}

// one event of a recording session, in Witness names and logical pixels (F8 marks the moment). false: the view must
// not see it either (the late release of something a lost focus already let go of). A finger's CANCELED (the system
// ended the touch: never a completed tap) is recorded as the protocol's cancel, which the replay injects as CANCELED.

bool App::recordEvent(SDL_Event& e) {
  auto has = [](const std::vector<std::string>& v, const std::string& s) { return std::find(v.begin(), v.end(), s) != v.end(); };
  auto drop = [](std::vector<std::string>& v, const std::string& s) { v.erase(std::remove(v.begin(), v.end(), s), v.end()); };
  switch (e.type) {
    case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP: {
      if (e.key.repeat) return true;
      char k[32];
      const std::string name = witness_key_normalize(SDL_GetScancodeName(e.key.scancode), k, sizeof k);
      const bool down = e.type == SDL_EVENT_KEY_DOWN;
      if (name.empty()) return true;
      if (name != "f8") {   // (F8 only marks the moment: never held)
        if (down) { drop(rec.keysLetGo, name); if (!has(rec.keys, name)) rec.keys.push_back(name); }
        else if (has(rec.keysLetGo, name)) { drop(rec.keysLetGo, name); return true; }   // the view has no key-up handling
        else drop(rec.keys, name);
      }
      witness_record_key(name.c_str(), down);
      return true;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP: {
      if (e.button.which == SDL_TOUCH_MOUSEID) return true;   // made from a finger: the touch is what is recorded
      if (e.button.button < SDL_BUTTON_LEFT || e.button.button > SDL_BUTTON_RIGHT) return true;   // (the protocol has 3)
      const int b = e.button.button == SDL_BUTTON_LEFT ? 0 : e.button.button == SDL_BUTTON_MIDDLE ? 1 : 2;
      const bool down = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
      if (down) { rec.buttonLetGo[b] = false; rec.button[b] = true; rec.buttonEv[b] = e.button; }
      else if (rec.buttonLetGo[b]) { rec.buttonLetGo[b] = false; return false; }
      else rec.button[b] = false;
      rec.mouseX = e.button.x; rec.mouseY = e.button.y;
      float lx, ly;
      recLogical(e.button.x, e.button.y, lx, ly);
      witness_record_mouse(lx, ly, b, down);
      return true;
    }
    case SDL_EVENT_MOUSE_MOTION: {
      if (e.motion.which == SDL_TOUCH_MOUSEID) return true;
      rec.mouseX = e.motion.x; rec.mouseY = e.motion.y;
      float lx, ly;
      recLogical(e.motion.x, e.motion.y, lx, ly);
      witness_record_mouse(lx, ly, -1, 0);
      return true;
    }
    case SDL_EVENT_MOUSE_WHEEL: {
      if (e.wheel.which == SDL_TOUCH_MOUSEID) return true;
      witness_event w{};
      w.type = WITNESS_EV_WHEEL; w.down = -1; w.button = -1;
      w.dx = -e.wheel.x * 100.0; w.dy = -e.wheel.y * 100.0;   // the protocol's sign: + is down / zoom out
      witness_record_event(&w);
      return true;
    }
    case SDL_EVENT_FINGER_DOWN: case SDL_EVENT_FINGER_MOTION: case SDL_EVENT_FINGER_UP: case SDL_EVENT_FINGER_CANCELED: {
      if (e.tfinger.touchID == SDL_MOUSE_TOUCHID) return true;   // made from the mouse: the mouse is what is recorded
      const bool ends = e.type == SDL_EVENT_FINGER_UP || e.type == SDL_EVENT_FINGER_CANCELED;
      const SDL_FingerID id = e.tfinger.fingerID;
      int slot = -1;
      for (int i = 0; i < 10; i++) if (rec.finger[i].on && rec.finger[i].ev.fingerID == id) slot = i;
      if (e.type == SDL_EVENT_FINGER_DOWN) {
        rec.fingersLetGo.erase(std::remove(rec.fingersLetGo.begin(), rec.fingersLetGo.end(), id), rec.fingersLetGo.end());
        for (int i = 0; i < 10 && slot < 0; i++) if (!rec.finger[i].on) slot = i;
        if (slot < 0) return true;   // an eleventh finger (the protocol has 10)
        rec.finger[slot].on = true;
      } else if (slot < 0) {
        // not a finger the recording pressed: one a lost focus let go of (its moves and its late release are dropped:
        // the view had that release) or one put down before the recording began
        auto it = std::find(rec.fingersLetGo.begin(), rec.fingersLetGo.end(), id);
        if (it == rec.fingersLetGo.end()) return true;
        if (ends) rec.fingersLetGo.erase(it);
        return false;
      }
      rec.finger[slot].ev = e.tfinger;
      float lx, ly;
      recFinger(e.tfinger, lx, ly);
      const int phase = e.type == SDL_EVENT_FINGER_DOWN ? WITNESS_TOUCH_DOWN : e.type == SDL_EVENT_FINGER_UP ? WITNESS_TOUCH_UP
                      : e.type == SDL_EVENT_FINGER_CANCELED ? WITNESS_TOUCH_CANCEL : WITNESS_TOUCH_MOVE;
      witness_record_touch_ex(slot, lx, ly, phase, e.tfinger.pressure);   // (pressure <= 0: not given; replays use 1)
      if (ends) rec.finger[slot].on = false;
      return true;
    }
    default: return true;
  }
}

// a lost focus during a recording (the protocol): everything the recording holds is let go of, in the view (which may
// never hear of it: a release while another window has the focus is lost) and in the replay alike, and a script's held
// keys with them, as the keyboard's own state is reset. Then the pause it causes is recorded as the action "pause",
// which the replay applies at the same point of the input stream (a harness's window never loses the focus).

void App::recLetGo(bool pause) {
  for (int i = 0; i < 10; i++) {
    if (!rec.finger[i].on) continue;
    SDL_Event ev;
    SDL_zero(ev);
    ev.tfinger = rec.finger[i].ev;
    ev.type = SDL_EVENT_FINGER_CANCELED;   // the system ended it (the protocol's cancel): the view takes it as no tap
    float lx, ly;
    recFinger(ev.tfinger, lx, ly);
    witness_record_touch_ex(i, lx, ly, WITNESS_TOUCH_CANCEL, ev.tfinger.pressure);
    rec.finger[i].on = false;
    rec.fingersLetGo.push_back(ev.tfinger.fingerID);
    view.event(ev, game);
  }
  for (int b = 0; b < 3; b++) {
    if (!rec.button[b]) continue;
    SDL_Event ev;
    SDL_zero(ev);
    ev.button = rec.buttonEv[b];
    ev.type = SDL_EVENT_MOUSE_BUTTON_UP;
    ev.button.down = false;
    ev.button.x = rec.mouseX; ev.button.y = rec.mouseY;   // where the pointer is (the replay moves it there first)
    float lx, ly;
    recLogical(ev.button.x, ev.button.y, lx, ly);
    witness_record_mouse(lx, ly, b, 0);
    rec.button[b] = false;
    rec.buttonLetGo[b] = true;
    view.event(ev, game);
  }
  for (const std::string& k : rec.keys) { witness_record_key(k.c_str(), 0); rec.keysLetGo.push_back(k); }
  rec.keys.clear();
  for (const Held& hd : held) view.scriptHold(hd.sc, false);
  held.clear();
  if (pause) {
    witness_event w{};
    w.type = WITNESS_EV_ACTION; w.down = -1; w.button = -1;
    std::snprintf(w.name, sizeof w.name, "%s", "pause");
    witness_record_event(&w);
  }
}

#endif
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

#ifdef EMB_WITNESS
// (Witness) one sim step as a harness takes it, and as a WITNESS_RECORD session takes each of its steps (so a replay
// re-runs the very same calls): the view's input, the sim (in play), the view's update at the step's dt

void App::simStep(float dt) {
  Input in = view.input(game);
  if (game.mode == Mode::Play) game.update(dt, in);
  else if (game.mode == Mode::Dead) game.update(dt, Input());   // (P2b B11) the hero's fall runs on the sim's clock
  else if (game.mode == Mode::Title) game.time += dt;
  view.update(game, dt);
}

#endif
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

#ifdef EMB_WITNESS
void App::drawFrame() { screen::frame(pix); view.settleBakes(); view.draw(game, false); }


// the queued events, through the view (the hidden window's own focus / quit events are never a pause or a quit)

void App::drainEvents() {
  SDL_Event ev;
  while (SDL_PollEvent(&ev)) {
    if (ev.type == SDL_EVENT_QUIT || ev.type == SDL_EVENT_WINDOW_FOCUS_LOST || ev.type == SDL_EVENT_WINDOW_HIDDEN ||
        ev.type == SDL_EVENT_WILL_ENTER_BACKGROUND) continue;
    // a replayed "pause": the recorded session lost the focus here and paused, as focusLost does
    if (ev.type == SDL_EVENT_USER && ev.user.code == kPauseCode) { if (game.mode == Mode::Play) game.mode = Mode::Paused; continue; }
    view.event(ev, game);
  }
}

void App::wReset(long long s, const char* scene, witness::Value params) {
  // nothing of the last run may leak into this one. Whatever it held is let go first, through the view as a
  // player's release would be: keys (held by the harness or a script), fingers still down (a tap's or a swipe's
  // pending lift), mouse buttons. Then the walk, a waiting command and every queued event go
  for (SDL_Scancode sc : wHeld) { view.scriptHold(sc, false); pushKey(sc, false); }
  for (const Held& hd : held) { view.scriptHold(hd.sc, false); pushKey(hd.sc, false); }
  for (const Lift& l : lifts) pushFinger(SDL_EVENT_FINGER_UP, l.id, l.x, l.y);
  for (const Lift& l : wFingers) pushFinger(SDL_EVENT_FINGER_UP, l.id, l.x, l.y);
  for (int b : wButtons) pushMouse(SDL_EVENT_MOUSE_BUTTON_UP, (float)mouseX, (float)mouseY, b);
  setMove(false, false, false, false);
  drainEvents();
  wHeld.clear(); held.clear(); lifts.clear(); drags.clear(); wButtons.clear(); wFingers.clear();
  walk = Walk(); extRun = ExtRun();
  heard.clear(); pendingShot.clear(); lastFail.clear();
  scriptFails = 0; scriptT = 0;
  SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
  // a freshly made view and mixer: no clock (t_, the particles' seed), effect, notice, banner, fade, menu cursor,
  // texture cache, voice or piece of music of the last run carries over (View::witnessRelease)
  view.witnessRelease();
  view.~View();
  audio.~Audio();
  // (P2a) the screen and the rig this run asks for, before the view is rebuilt (VISION_PLAN 15.36 item 10):
  //   window [w, h] device px (the hidden window resized; the fit runs unpinned), safe [l, t, r, b] device px,
  //   screen "fill" | "pixel", zoom -1..2 or "keep" (the last run's, as a restart reads the settings file),
  //   rig 1 | 2, female (the hero's body base)
  {
    int ww = 0, wh = 0, safe[4] = {0, 0, 0, 0}, mode = -1, zoom = 0;
    bool hasSafe = false, keep = false;
    if (params["window"].size() == 2) { ww = (int)params["window"][0].integer(); wh = (int)params["window"][1].integer(); }
    if (params["safe"].size() == 4) { hasSafe = true; for (int k = 0; k < 4; k++) safe[k] = (int)params["safe"][k].integer(); }
    if (params["screen"].exists()) mode = std::string(params["screen"].str("fill")) == "pixel" ? (int)screen::PixelPerfect : (int)screen::Fill;
    // (P2b integration, test only) defaults for a run whose harness cannot pass params (witness play batch testers:
    // a --params JSON in their start line is denied): EMB_WPLAY_WINDOW "2556x1179", EMB_WPLAY_SAFE "l,t,r,b",
    // EMB_WPLAY_SCREEN fill|pixel, EMB_WPLAY_RIG, EMB_WPLAY_HOUR, EMB_WPLAY_GOD; a param that is given wins
    if (const char* e = std::getenv("EMB_WPLAY_WINDOW"); e && params["window"].size() != 2) std::sscanf(e, "%dx%d", &ww, &wh);
    if (const char* e = std::getenv("EMB_WPLAY_SAFE"); e && !hasSafe)
      hasSafe = std::sscanf(e, "%d,%d,%d,%d", &safe[0], &safe[1], &safe[2], &safe[3]) == 4;
    if (const char* e = std::getenv("EMB_WPLAY_SCREEN"); e && !params["screen"].exists())
      mode = std::string(e) == "pixel" ? (int)screen::PixelPerfect : (int)screen::Fill;
    if (params["zoom"].exists()) {
      if (params["zoom"].type() == WITNESS_STRING) keep = std::string(params["zoom"].str("")) == "keep";
      else zoom = (int)params["zoom"].integer(0);
    }
    if ((hasSafe || mode >= 0) && ww <= 0) { ww = 960; wh = 540; }   // a safe area or a mode alone: the harness window, unpinned
    screen::witnessScreen(pix, ww, wh, hasSafe ? safe : nullptr, mode, zoom, keep);
    // (P2c flip, D100) the rig defaults to EMB_WPLAY_RIG, else art::kDefaultRig (2); "rig": 1 opts into the v1 fallback
    const char* envRig = std::getenv("EMB_WPLAY_RIG");
    const int wantRig = (int)params["rig"].integer(envRig ? std::atoi(envRig) : art::kDefaultRig);
    if (wantRig == 1 && art::rigVersion() != 1) std::printf("rig 1: the v1 people, an opt-in fallback until the owner's retirement OK\n");
    art::setRigVersion(wantRig);   // a fresh view follows: no cache carries the other rig's pixels
  }
  new (&audio) Audio();
  new (&view) View();
  view.init(pix, audio);
  view.witness = true;
  view.touchUI = touchUIStart;
  // Witness's reserved reset params (PROTOCOL.md reset): camera "hold" keeps the camera where the reset puts it for the
  // whole run (no follow, no shake: the `camhold` script command's flag, View::P2bHooks::camHold); hide [ids or type
  // names] leaves those actors undrawn (drawing only: View::wHide), so `measure figure` diffs an exact silhouette
  view.p2b.camHold = params["camera"].type() == WITNESS_STRING && std::string(params["camera"].str("")) == "hold";
  {
    const witness::Value hide = params["hide"];
    auto hideOne = [&](const witness::Value& e) {
      if (e.type() == WITNESS_STRING) view.wHide.types.push_back(e.str(""));
      else if (e.type() == WITNESS_NUMBER) view.wHide.ids.push_back((int)e.integer(-1));
    };
    if (hide.type() == WITNESS_ARRAY) for (size_t k = 0; k < hide.size(); k++) hideOne(hide.at(k));
    else hideOne(hide);   // (a lone id or type name)
  }
  mouseX = mouseY = 0;
  wAudioHash = 1469598103934665603ull;
  wAudioN = 0;
  startSeed = (uint64_t)s;
  // then a brand-new Game and the start the params ask for (applyStart: the same steps a recorded --play took)
  StartSpec sp;
  sp.god = params["god"].boolean() || (!params["god"].exists() && std::getenv("EMB_WPLAY_GOD"));
  sp.scene = scene && *scene ? scene : params["scene"].str("");
  sp.enter = params["enter"].boolean();
  if (params["hour"].exists()) sp.hour = (float)params["hour"].num();
  else if (const char* e = std::getenv("EMB_WPLAY_HOUR")) sp.hour = (float)std::atof(e);
  if (params["at"].size() == 2) { sp.atSet = true; sp.atX = (int32_t)params["at"][0].integer(); sp.atY = (int32_t)params["at"][1].integer(); }
  sp.talk = (int)params["talk"].integer(-1);
  sp.menu = (int)params["menu"].integer(-1);
  sp.fight = params["fight"].str("");
  sp.count = (int)params["count"].integer(0);
  sp.creator = params["creator"].boolean();
  sp.background = (int)params["background"].integer(0);
  sp.name = params["name"].str("");
  sp.ids = (int)params["ids"].integer(0);   // a recording's id counter at its start (absent: a fresh Game's)
  applyStart(startSeed, sp);
  if (params["female"].boolean()) { game.app.female = true; AnimOps::refreshHero(game); }   // (P2a) the woman's body base
}

void App::wInput(const witness_event& e) {
  if (e.type == WITNESS_EV_KEY && e.down >= 0) {
    const SDL_Scancode sc = keyFromName(e.name);
    if (sc == SDL_SCANCODE_UNKNOWN) { witness_log(WITNESS_WARN, "witness: unknown key '%s'", e.name); return; }
    const bool down = e.down == 1;
    pushKey(sc, down);          // one-shot presses (View::event)
    view.scriptHold(sc, down);  // and the held state (View::input)
    wHeld.erase(std::remove(wHeld.begin(), wHeld.end(), sc), wHeld.end());
    if (down) wHeld.push_back(sc);
  } else if (e.type == WITNESS_EV_MOUSE) {
    if (e.has_pos) { mouseX = e.x; mouseY = e.y; pushMouse(SDL_EVENT_MOUSE_MOTION, (float)mouseX, (float)mouseY, 0); }
    if (e.down >= 0) {
      const int b = e.button == 1 || e.button == 2 ? e.button : 0;
      pushMouse(e.down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP, (float)mouseX, (float)mouseY, b);
      wButtons.erase(std::remove(wButtons.begin(), wButtons.end(), b), wButtons.end());
      if (e.down) wButtons.push_back(b);
    }
  } else if (e.type == WITNESS_EV_WHEEL) {
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = SDL_EVENT_MOUSE_WHEEL;
    ev.wheel.windowID = SDL_GetWindowID(pix.window());
    ev.wheel.x = (float)(-e.dx / 100.0); ev.wheel.y = (float)(-e.dy / 100.0);   // the protocol's + is down
    pushEvent(ev);
  } else if (e.type == WITNESS_EV_TOUCH) {
    // a finger, through the touch path SDL delivers one by (View::event's FINGER_* cases; hello.touch: no mouse copy).
    // cancel (the system ended the touch) arrives as SDL's CANCELED: the finger is lifted, but it is never a tap
    const SDL_FingerID id = (SDL_FingerID)e.id + 1;   // (SDL never uses finger id 0)
    const float x = (float)e.x, y = (float)e.y;
    const SDL_EventType t = e.phase == WITNESS_TOUCH_DOWN ? SDL_EVENT_FINGER_DOWN : e.phase == WITNESS_TOUCH_UP ? SDL_EVENT_FINGER_UP
                          : e.phase == WITNESS_TOUCH_CANCEL ? SDL_EVENT_FINGER_CANCELED : SDL_EVENT_FINGER_MOTION;
    pushFinger(t, id, x, y, e.pressure > 0 ? (float)e.pressure : 1.0f);
    wFingers.erase(std::remove_if(wFingers.begin(), wFingers.end(), [&](const Lift& l) { return l.id == id; }), wFingers.end());
    if (t == SDL_EVENT_FINGER_DOWN || t == SDL_EVENT_FINGER_MOTION) wFingers.push_back({id, x, y, 0.0f});
  } else if (e.type == WITNESS_EV_ACTION && std::strcmp(e.name, "pause") == 0) {
    SDL_Event ev;   // in the event stream, so it lands between the inputs around it exactly as it did when recorded
    SDL_zero(ev);
    ev.type = SDL_EVENT_USER;
    ev.user.code = kPauseCode;
    pushEvent(ev);
  } else {
    witness_log(WITNESS_WARN, "witness: input type %d is not used by Embervale (keys, mouse, wheel, touch and the "
                "\"pause\" action are)", (int)e.type);
  }
}

void App::wStep(double dt) {
  const float fdt = (float)dt;
  scriptStep(fdt);   // held keys, a walk or a registered command under way (script time)
  drainEvents();
  screen::frame(pix);   // (P2a) a ZOOM step re-presents the layers now: the camera and the NPC LOD frame the world view in use
  view.wantSave = false;
  if (view.wantsQuit) { view.wantsQuit = false; game.mode = Mode::Title; }
  if (view.wantNewGame || view.wantContinue) { view.wantNewGame = view.wantContinue = false; startNew(startSeed); }
  game.sleepHalfW = std::max(22.0f, view.viewW() * 0.5f / TILE + 4.0f);   // as frame(): the NPC LOD follows the world view
  game.sleepHalfH = std::max(13.0f, view.viewH() * 0.5f / TILE + 4.0f);
  simStep(fdt);
  const int n = witness_audio_frames_for_step(dt);   // exactly the samples this step covers
  wMix.resize((size_t)n);
  for (int k = 0; k < n; k += 1024) audio.render(wMix.data() + k, std::min(1024, n - k));
  witness_audio_write(wMix.data(), n);
  for (int k = 0; k < n; k++) {
    uint32_t u;
    std::memcpy(&u, &wMix[(size_t)k], 4);
    for (int b = 0; b < 4; b++) { wAudioHash ^= (u >> (8 * b)) & 255u; wAudioHash *= 1099511628211ull; }
  }
  wAudioN += n;
  // (watch mode) the step on screen (drawing never feeds the sim); (Witness 0.7.11) a frame nobody sees (a step sent with
  // "draw": "last", play.lazyDraw) is not drawn
  if (watch && witness_drawing()) { drawFrame(); pix.end(); }
}

void App::wSnapshot(witness::Json& j) {
  WitnessExtra x = wStart;
  x.scriptBusy = walk.on || extRun.e != nullptr;
  x.scriptFails = scriptFails;
  x.lastFail = lastFail;
  x.savesWritten = g_savesWritten;
  x.saveEnabled = !noSave;
  witnessState(j, game, view, x);
}

bool App::wScreenshot(witness_image& img) {
  drawFrame();
  int w = 0, hh = 0;
  // (P2b fix round 1, test only) EMB_WPLAY_SHOT "world:K": a playtester's shot is the world layer at world px (one
  // pixel per art px, the UI left out), each pixel repeated K x K (1-4), so people read at 32 x K px instead of the
  // UI canvas's ~20 px (the phone window's UI scale samples the world below its own resolution)
  // (fix, T4 playtest: "I could not tell the swings apart") "world:fit": the largest K (1-4) whose shot stays within
  // what the tester's eyes take unscaled (1568 px wide, 1.15 MP), so a closer ZOOM (a smaller world layer) gives a
  // bigger K: NORMAL on the phone window (512 x 236) 3, CLOSE (366 x 169) 4. A larger shot is scaled down before
  // the model sees it, which undoes the extra K
  int worldK = 0;
  bool worldFit = false;
  if (const char* e = std::getenv("EMB_WPLAY_SHOT"); e && std::strncmp(e, "world", 5) == 0) {
    worldFit = std::strcmp(e + 5, ":fit") == 0;
    worldK = worldFit ? 1 : std::clamp(e[5] == ':' ? std::atoi(e + 6) : 1, 1, 4);
  }
  bool ok;
  if (worldK > 0) {
    std::vector<uint8_t> src;
    ok = pix.readWorld(src, w, hh);
    if (ok && worldFit)
      while (worldK < 4 && w * (worldK + 1) <= 1568 && (double)w * hh * (worldK + 1) * (worldK + 1) <= 1.15e6) worldK++;
    if (ok && worldK > 1) {
      const int W = w * worldK, H = hh * worldK;
      wShot.assign((size_t)W * H * 4, 0);
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
          std::memcpy(&wShot[((size_t)y * W + x) * 4], &src[((size_t)(y / worldK) * w + x / worldK) * 4], 4);
      w = W; hh = H;
    } else if (ok) wShot.swap(src);
  } else ok = pix.readCanvas(wShot, w, hh);
  pix.end();
  if (!ok) { witness::fail(std::string("screenshot: ") + SDL_GetError()); return false; }
  img.rgba = wShot.data(); img.width = w; img.height = hh;
  return true;
}

void App::wCall(const char* name, witness::Value args, witness::Json& result) {
  ScriptCmd c;
  c.a.push_back(name);
  for (auto& ch : c.a[0]) ch = (char)std::tolower((unsigned char)ch);
  for (size_t i = 0; i < args.size(); i++) {
    const witness::Value v = args.at(i);
    if (v.type() == WITNESS_STRING) { c.a.push_back(v.str()); continue; }
    char b[40];
    const double d = v.num();
    if (d == std::floor(d) && std::fabs(d) < 1e15) std::snprintf(b, sizeof b, "%lld", (long long)d);
    else std::snprintf(b, sizeof b, "%g", d);
    c.a.push_back(b);
  }
  for (size_t k = 0; k < c.a.size(); k++) { if (k) c.text += ' '; c.text += c.a[k]; }
  if (c.a[0] == "quit") { witness::fail("quit: the harness ends the run (its quit command)"); return; }
  if (c.a[0] == "statematch" || c.a[0] == "shotmatch" || c.a[0] == "audiomatch") {
    // the determinism checks: this moment's full state (the snapshot plus every actor exactly: witnessDigest), this
    // frame's canvas (RGBA) or every sample mixed since the reset, against the first run of the label in this process
    const bool shot = c.a[0] == "shotmatch";
    if (c.a.size() < 2) { witness::fail(c.a[0] + ": a label (the runs that must match share it)"); return; }
    std::string now;
    if (shot) {
      drawFrame();
      int w = 0, hh = 0;
      const bool ok = pix.readCanvas(wShot, w, hh);
      pix.end();
      if (!ok) { witness::fail(std::string("shotmatch: ") + SDL_GetError()); return; }
      now.assign((const char*)wShot.data(), wShot.size());
    } else if (c.a[0] == "audiomatch") {
      char ab[48];
      std::snprintf(ab, sizeof ab, "%016llx over %lld samples", (unsigned long long)wAudioHash, wAudioN);
      now = ab;
    } else {
      now = witnessJsonString([&](witness::Json& j) { h.snapshot(j); }) + "\n" + witnessDigest(game);
    }
    uint64_t hash = 1469598103934665603ull;   // FNV-1a 64
    for (unsigned char ch : now) { hash ^= ch; hash *= 1099511628211ull; }
    const std::string key = c.a[0] + ":" + c.a[1];
    auto it = wMatch.find(key);
    const bool first = it == wMatch.end();
    if (first) wMatch[key] = now;
    else if (it->second != now) {
      size_t at = 0;
      while (at < now.size() && at < it->second.size() && now[at] == it->second[at]) at++;
      std::string msg = c.a[0] + " " + c.a[1] + ": this run differs from the first one";
      if (shot) {
        const size_t px = at / 4;
        msg += " (first differing pixel x " + std::to_string(px % (size_t)std::max(1, Pix::W)) + ", y " + std::to_string(px / (size_t)std::max(1, Pix::W)) + ")";
      } else {
        const size_t from = at > 60 ? at - 60 : 0;
        msg += " at char " + std::to_string(at) + ": first ..." + it->second.substr(from, 140) + "... now ..." + now.substr(from, 140) + "...";
      }
      witness::fail(msg);
      return;
    }
    char hb[24];
    std::snprintf(hb, sizeof hb, "%016llx", (unsigned long long)hash);
    result.begin_object().field("label", c.a[1]).field("first", first).field("hash", std::string(hb)).end_object();
    return;
  }
  // (fix, T6 playtest) `cancel` (or `stop`): the call still running (a walkto / talkto's autopilot, a registered
  // command still waiting) is let go at once, the autopilot's held keys released; the harness's own stop ends the
  // whole session, so a tester stuck on a busy call had no way out but to wait out its timeout
  if (c.a[0] == "cancel" || c.a[0] == "stop") {
    const bool was = walk.on || extRun.e != nullptr;
    const std::string what = walk.on ? "walkto " + walk.what : extRun.e ? extRun.cmd.text : std::string();
    if (walk.on) { walk = Walk(); setMove(false, false, false, false); }
    extRun = ExtRun();
    if (was) std::printf("script: cancel: stopped %s\n", what.c_str());
    result.begin_object().field("stopped", was).field("call", what).field("busy", false).end_object();
    return;
  }
  if (walk.on || extRun.e) { witness::fail("busy: the last call is still running (step until state.script.busy is false, or call cancel to stop it)"); return; }
  const int fails0 = scriptFails;
  witnessInCall = true;
  runCmd(c);
  witnessInCall = false;
  if (scriptFails > fails0) { witness::fail(lastFail); return; }
  if (!pendingShot.empty()) {   // `shot file.png`: this frame, to a file
    drawFrame();
    const bool ok = pix.screenshot(pendingShot.c_str());
    pix.end();
    const std::string path = pendingShot;
    pendingShot.clear();
    if (!ok) { witness::fail("shot " + path + ": " + SDL_GetError()); return; }
  }
  result.begin_object().field("busy", walk.on || extRun.e != nullptr).field("mode", (int)game.mode).end_object();
}

void App::runWitness() {
  // ---- Witness (engine/witness.h; github.com/Josh17400/witness): a harness launched this run with WITNESS_ADDR set
  // (or --witness) and drives it over a local socket. The game steps only when told to, at the dt it is given (no
  // clock, no vsync); input goes through the script driver's paths (pushed SDL events, View::scriptHold); the mixer is
  // rendered into the capture buffer each step (no audio device); a frame is drawn only for a screenshot (the window
  // is hidden). `call` runs any script command, built in (runCmd) or registered (rpg/view/script_api.h); one that takes
  // time (walkto, talkto, a registered command that waits) carries on over the next steps (state.script.busy) and a
  // later failure is logged as an error (it fails the test).
  h.reset = [&](long long s, const char* scene, witness::Value params) { wReset(s, scene, params); };
  h.input = [&](const witness_event& e) { wInput(e); };
  h.step = [&](double dt) { wStep(dt); };
  h.snapshot = [&](witness::Json& j) { wSnapshot(j); };
  h.screenshot = [&](witness_image& img) -> bool { return wScreenshot(img); };
  h.call = [&](const char* name, witness::Value args, witness::Json& result) { wCall(name, args, result); };

  // the calls: the built-in script commands, then every registered one (the registry's expect checks ride on `expect`)
  static const struct { const char* name; const char* args; const char* help; } builtin[] = {
      {"goto", "city|town|village|cave|ruin|camp|lair|shrine|capital|market [enter]", "fast travel to the nearest such place"},
      {"walkto", "inn|shop|innkeeper|giver|exit|NAME|x y [timeout]", "autopilot (takes steps: state.script.busy)"},
      {"cancel", "", "stop the call still running (a walkto, a talkto or a waiting command such as war goto patrol); also 'stop'"},
      {"talkto", "NAME|role [timeout]", "walk to someone and talk to them"},
      {"talk", "0|1|2", "the innkeeper: greeting, job offer, shop"},
      {"choose", "words", "choose the open dialogue's option containing the words"},
      {"fight", "wolf|boar|bear|...|dragon [n]", "spawn monsters nearby (gives the test kit if unarmed)"},
      {"enter", "inn|shop|smithy|temple|house|... [floor] [n]", "step into the n-th nearest building of that type"},
      {"floor", "n", "inside a building: go to that floor"},
      {"at", "gx gy", "teleport to a global tile"},
      {"hour", "h", "set the time of day"},
      {"god", "[0|1]", "invulnerability"},
      {"kit", "", "the test kit: sword, bow, arrows, potions"},
      {"gear", "band [weapon]", "a full armour set of band 1..7"},
      {"strip", "", "take off every piece of equipment"},
      {"menu", "tab", "open the menu on a tab"},
      {"newgame", "", "a fresh world (the reset seed)"},
      {"key", "name", "press and release a key"},
      {"hold", "key secs", "hold a key"},
      {"walk", "east|west|north|south secs", "hold a direction"},
      {"tap", "x y", "a touch tap at logical coordinates"},
      {"click", "x y", "a left click at logical coordinates"},
      {"chestgo", "[hoard|explore|chest]", "stand beside the nearest closed chest"},
      {"pos", "[map]", "print where the player is (game output)"},
      {"shot", "file.png", "save this frame to a file"},
      {"expect", "mode|gold|quest|heard|option|inside|... args", "a script check (fails the call)"},
      {"statematch", "label", "the full state (every actor exactly) must equal the first run of this label in this process"},
      {"shotmatch", "label", "this frame's canvas must equal the first run of this label in this process"},
      {"audiomatch", "label", "every sample mixed since the reset must equal the first run of this label in this process"}};
  std::vector<witness_call_info> calls;
  for (const auto& b : builtin) calls.push_back({b.name, b.args, b.help});
  for (const ScriptExt& e : scriptExts())
    if (std::strncmp(e.name, "expect:", 7) != 0) calls.push_back({e.name, nullptr, e.help});
  static const char* const keys[] = {"w", "a", "s", "d", "up", "down", "left", "right", "space", "e", "j", "k", "l", "z", "q", "f", "r",
                                     "i", "m", "tab", "escape", "enter", "shift", "alt", "1", "2", "3", "4",
                                     "minus", "equals"};   // (P2a) ZOOM out / in
  witness_hello hello = witness_hello();
  hello.name = "Embervale";
  hello.engine = "custom C++/SDL3";
  hello.language = "c++";
  hello.fps = 60;
  screen::frame(pix);   // the canvas size (Pix::W x Pix::H) the harness sees
  hello.width = Pix::W;
  hello.height = Pix::H;
  hello.calls = calls.data();
  hello.call_count = (int)calls.size();
  hello.keys = keys;
  hello.key_count = (int)(sizeof keys / sizeof keys[0]);
  hello.audio_sample_rate = Audio::kSampleRate;
  hello.audio_channels = 1;
  hello.touch = 1;   // touch events reach the touch path; Witness sends no mouse copy of finger 0
  // (Witness 0.6) every sound the game starts is a protocol "sound" event (its name and volume, stamped with the step's
  // frame), so `witness listen` / `effect` can name sounds and match them to their causes. Nothing else changes
  Audio::setPlayListener([](const char* name, float vol) { witness_sound_ex(name, vol, 0, 0.0, 0.0); });
  if (witness::connect(hello, h)) {
#ifdef __EMSCRIPTEN__
    // (web, EMBERVALE_WITNESS_WEB) the page's ?witness=host:port: witness_run() hands control back to the browser and
    // keeps the runtime alive (emscripten_exit_with_live_runtime, like simulate_infinite_loop: main's frame and
    // everything the handlers reference stay alive, nothing after this line runs). The game then steps only inside the
    // harness's commands (witness_emscripten.js calls in): emscripten_set_main_loop is never started in witness mode
    witness_run();
#else
    // (watch mode) between commands the window's events keep being pumped about every 30 ms, so the OS never marks
    // it "not responding" and it can be moved; queued input stays queued for the next step, real input is filtered
    if (watch) while (witness_poll(30)) SDL_PumpEvents();
    else witness_run();
#endif
  } else std::printf("witness: could not connect to the harness\n");
  witness_shutdown();
}
#endif

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
