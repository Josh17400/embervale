// rpg/app/recorder.h - WITNESS_RECORD and the replay's start (SOLID 3.1e). Normal play with WITNESS_RECORD=<file.jsonl>
// records a replay `witness replay` re-simulates step for step: App::recordEvent turns each input event into the
// protocol's names and logical pixels, App::recLetGo lets go of all the recording holds when the focus is lost, and
// App::applyStart / specJson make the start a replay (or a harness reset) rebuilds exactly. The App's members are
// defined in recorder.cpp; the whole feature exists only with EMB_WITNESS.
#pragma once
#ifdef EMB_WITNESS
#include <SDL3/SDL.h>
#include <cstdint>
#include <string>
#include <vector>

// (Witness) how a run starts: a harness reset's params, or a recorded --play session's flags (its replay header)
struct StartSpec {
  bool god = false, enter = false, creator = false, atSet = false;
  std::string scene, fight, name;
  float hour = -1;
  int32_t atX = 0, atY = 0;
  int talk = -1, menu = -1, count = 0, background = 0, ids = 0;
};

// (WITNESS_RECORD) what the recording holds: every key, mouse button and finger it recorded as pressed and not yet as
// released. A lost focus lets go of all of it (recLetGo), and the late release the platform may still send for one of
// those afterwards is dropped, so neither the view nor the replay sees a release twice.
struct RecHeld {
  struct Finger { bool on = false; SDL_TouchFingerEvent ev{}; };   // ev: its last event (the release goes there)
  std::vector<std::string> keys, keysLetGo;    // Witness key names
  bool button[3] = {}, buttonLetGo[3] = {};    // 0 left, 1 middle, 2 right
  SDL_MouseButtonEvent buttonEv[3] = {};       // each press (a release is made from it)
  float mouseX = 0, mouseY = 0;                // the pointer's last window position
  Finger finger[10];                           // slot i is the replay's finger i (0 = the first finger down)
  std::vector<SDL_FingerID> fingersLetGo;
};
#endif
