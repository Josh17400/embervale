// The endless window's upkeep and the NPC level of detail (SOLID 4.1: these were Game members): keeping the window
// centred on the player and moving everything with it, the streamer's wish list, the people of the active sites, the
// dens, the roaming spawns, the threats round the townsfolk and who may sleep. DEFINED in rpg/sim/game.cpp; Game::update
// (sim_schedule.cpp) calls them. Game::frameWork and teleportGlobal stay public. StreamOps is a friend of Game (game.h).
#pragma once
#include "rpg/sim/game.h"

struct StreamOps {
  static void maybeRecentre(Game& g);   // M1: keep the player in the window's middle (VISION_PLAN 2.9)
  static void windowMoved(Game& g, int dx, int dy);   // M1: the window moved by (dx, dy) tiles: translate everything overworld
  static void reapplyLooted(Game& g);   // M1: open the looted overworld chests the window shows
  static void updateDens(Game& g, int ptx, int pty);
  static void updateSpawning(Game& g, float dt);
  static void prefetchTick(Game& g, float dt);   // keep the streamer's wish list current (creates the streamer on first use)
  static void streamSitePeople(Game& g, int si);   // spawn the site's people near the player, put the far calm ones away
  static void collectHostiles(Game& g);
  static bool sleepy(const Game& g, const Actor& a);   // NPC LOD: far off screen with nothing to react to
};
