// The REALM lane's hook into the core loop (SOLID 4.1: it was Game::realmStep). Game::update (sim_schedule.cpp) calls
// it once per step; it is DEFINED in rpg/sim/realm_game.cpp. RealmOps is a friend of Game (game.h). Game::realmSync and
// Game::realmNoteSites stay public members: scripts and tests call them.
#pragma once
#include "rpg/sim/game.h"

// API (SOLID 5.6): no stable test/script API. Every member is internal to the lane: only rpg/sim and rpg/story
// call it (tests and scripts go through Game's public members), and it may change with the lane.
struct RealmOps {
  //   REALM (rpg/sim/realm_game.cpp): once per update step; ticks the realm at a day change, focuses it on the player,
  //   registers the settlements the window loads, applies owners (World::setSiteOwner), emits Ev::Border on crossings
  static void step(Game& g, float dt);
};
