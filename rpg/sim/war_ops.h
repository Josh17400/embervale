// The WARDS lane's hooks into the core loop (SOLID 4.1: they were Game::warStep / warTalk / warChoose / warKill /
// warHostile): siege camps, patrols, refugees, garrisons, charred overlays, guard strength; the war's dialogue; a kill's
// siege contributions. The core (sim_schedule.cpp, game_dialogue.cpp, game_combat.cpp, ai.cpp ...) calls them; they are
// DEFINED in rpg/sim/war_game.cpp. WarOps is a friend of Game (game.h). Game::warPropKingdom stays public (the view).
#pragma once
#include "rpg/sim/game.h"

// API (SOLID 5.6). Stable test/script API: the members named below are called from outside rpg/sim and
// rpg/story, by name, so keep their names, signatures and meaning (a change breaks those callers):
//   tests and scripts (tools/tests):
//     hostile, spoils.
// Every other member is internal to the lane: only rpg/sim and rpg/story call it, and it may change with the lane.
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
  // (P3 LOOT, ruling 10) war spoils: an enemy soldier (or guard) the hero slays at a siege drops a piece of gear 10 % of
  // the time, a captain 50 % (Uncommon or better, an elite's luck); at most gsim::WAR_SPOILS_MAX a siege (the saved mark
  // gsim::kTagWarSpoils keyed by the siege id). A body roll in the soldier's realm's make at the danger where he fell,
  // with the hero as the finder (so a piece is never over the hero's level + 4). Its own seeded draw (the victim, the
  // siege, the world seed). Laid on the ground where he fell; true when a piece dropped.
  static bool spoils(Game& g, const Actor& victim, uint32_t siege);
};
