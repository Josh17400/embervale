// The WARDS lane's hooks into the core loop (SOLID 4.1: they were Game::warStep / warTalk / warChoose / warKill /
// warHostile): siege camps, patrols, refugees, garrisons, charred overlays, guard strength; the war's dialogue; a kill's
// siege contributions. The core (sim_schedule.cpp, game_dialogue.cpp, game_combat.cpp, ai.cpp ...) calls them; they are
// DEFINED in rpg/sim/war_game.cpp. WarOps is a friend of Game (game.h). Game::warPropKingdom stays public (the view).
#pragma once
#include "rpg/sim/game.h"

struct WarOps {
  // two actors on opposite sides of a war between kingdoms (enemy soldiers, an enemy town's guards; war_game.cpp).
  // Everything else about hostility stays factionsHostile.
  static bool hostile(const Game& g, const Actor& a, const Actor& b);
  //   WARDS (rpg/sim/war_game.cpp): once per update step (siege camps, patrols, refugees, garrisons, charred overlays,
  //   guard strength); the war's dialogue (siege commanders, guards' war talk); a kill (siege contributions, reputation)
  static void step(Game& g, float dt);
  static void talk(Game& g, Actor& a);   // may add options (DLG_WAR..) and set dlg.text; called by talkTo before FAREWELL
  static bool choose(Game& g, const DlgOpt& o);   // an option in [DLG_WAR, DLG_END): true handled
  static void kill(Game& g, const Actor& victim, int killer);
};
