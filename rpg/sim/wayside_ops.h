// Wayside life (SOLID 4.1: these were Game members): vignettes, ambushes, toll bridges, standing stones, lone graves
// and rumours, DEFINED in rpg/sim/wayside.cpp. Game::tollPaid stays public (the view). WaysideOps is a friend of Game.
#pragma once
#include "rpg/sim/game.h"

struct WaysideOps {
  // M2 wayside: the toll a troll bridge asks (10-25 gold; Game::tollPaid: whether the player may cross it today)
  static int tollOf(const Game& g, int site);
  static bool spawn(Game& g, int site, const Spawn& sp);   // a vignette's spawn (true: handled, spawned or held back)
  static void tick(Game& g, float dt);   // ambushes, toll bridges
  static bool useProp(Game& g, art::Prop p, int tx, int ty);   // standing stones, a lone grave (true: handled)
  static void tollTalk(Game& g, Actor& troll);   // the troll's demand as a dialogue
  static int rumourSite(Game& g, int32_t gx, int32_t gy);   // the nearest unknown place worth a rumour (-1 none)
  static std::string rumourLine(const Game& g, int site, int32_t fromX, int32_t fromY);
};
