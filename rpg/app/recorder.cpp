// rpg/app/recorder.cpp - see recorder.h
#ifdef EMB_WITNESS
#include "rpg/app/recorder.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <new>
#include "engine/witness.h"
#include "rpg/app/app.h"
#include "rpg/app/script_runner.h"

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
