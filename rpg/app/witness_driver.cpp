// rpg/app/witness_driver.cpp - the Witness harness's handlers (SOLID 3.1f): engine/witness.h; github.com/Josh17400/witness.
// A harness launched this run (WITNESS_ADDR set, or --witness) and drives it over a local socket: reset, input, step,
// snapshot, screenshot and call, each one App member below (App::runWitness connects them). The whole file exists only
// with EMB_WITNESS.
#ifdef EMB_WITNESS
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include "engine/witness.h"
#include "rpg/app/app.h"
#include "rpg/app/input_inject.h"
#include "rpg/app/save_store.h"
#include "rpg/app/script_runner.h"
#include "rpg/sim/anim.h"   // (P2a) AnimOps::refreshHero (the reset's female)
#include "rpg/view/screen.h"
#include "rpg/view/witness_state.h"

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
