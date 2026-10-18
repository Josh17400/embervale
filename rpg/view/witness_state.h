// The Witness harness's view of the game (rpg/main.cpp witness mode; engine/witness.h): the snapshot a test reads as
// `state`. Desktop builds with EMBERVALE_WITNESS only.
#pragma once
#ifdef EMB_WITNESS
#include <functional>
#include <string>
#include "engine/witness.h"

class Game;
class View;

struct WitnessExtra {        // what only the platform layer knows
  bool scriptBusy = false;   // a `call` that takes time (walkto, a registered command) is still under way
  int scriptFails = 0;       // script failures since the reset (expects, walks)
  std::string lastFail;
  int savesWritten = 0;      // saves written to disk this session (witness mode never writes one)
  bool saveEnabled = false;
  bool hasStart = false;     // the global tile the player stood on at the reset (state.player.start / moved)
  int startGX = 0, startGY = 0;
};

// state: mode, clock, player, quests, location, nearby entities (state.entities), UI and counts; JSON-safe, small
void witnessState(witness::Json& j, const Game& g, const View& v, const WitnessExtra& x);
// exact facts the snapshot rounds or leaves out (every actor's id, kind and position as hex floats, the clock): with
// the snapshot it is what the `statematch` call compares between runs
std::string witnessDigest(const Game& g);
// a JSON value written by fill, as a string (engine/witness_impl.cpp: the writer's struct is private to that file)
std::string witnessJsonString(const std::function<void(witness::Json&)>& fill);
#endif
