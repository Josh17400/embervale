// Travel behind the fade and the interior made ahead (SOLID 4.1: these were Game members). Game::beginTravel,
// beginTravelTo, finishTravel, fastTravel and travelQuote stay public (the view and scripts); the steps below are the
// SIM lane's internals, DEFINED in rpg/sim/travel.cpp and rpg/sim/prep_interior.cpp. TravelOps is a friend of Game.
#pragma once
#include "rpg/sim/game.h"

struct TravelOps {
  // the key of a building floor's interior made ahead (Game::PrepInterior::key; never 0)
  static uint64_t interiorKey(const Bldg& b, int floor) { return (b.id ? b.id : ((uint64_t)b.seed << 20 | 0x5EA7ull)) * 31ull + (uint64_t)floor + 1; }
  // M2 SIM lane: a carriage can be hired here (the player stands in a town or a city, or in one of its buildings)
  static bool carriageHere(const Game& g);
  static void prepTick(Game& g, float dt);
  static bool takePrepared(Game& g, const Bldg& b, int floor, Map& out);   // false: nothing made ahead for it
  static void step(Game& g, float dt);   // one update step of a journey (Gather / Arrive)
  static void want(Game& g);   // the streamer's wish list for the destination window
  static bool ready(Game& g);   // every chunk and region plan the arrival needs is ready
  static void arriveNow(Game& g);   // move the window and the player (the Arrive step)
  static void dest(const Game& g, const Site& s, int kind, int32_t& gx, int32_t& gy);   // the arrival tile (global)
  static void arriveInOpen(Game& g, const Site& s);   // settlements: the nearest open tile with a free step south
  static int32_t landAt(const Game& g, int32_t gx, int32_t gy);   // the landmass id at (or near) a global tile
  static bool startJourney(Game& g, int site, int kind, float hours, int gold);
  static void journeyWindow(Game& g);   // the journey's destination window and wish list (startJourney, beginTravelTo)
};
