// The NUMBERS / FORGE lane's hooks into the core loop (SOLID 4.1: they were Game::craftTalk / craftChoose). talkTo and
// dialogueChoose (game_dialogue.cpp) call them; they are DEFINED in rpg/sim/craft_game.cpp. CraftHooks is a friend of
// Game (game.h); it is kept apart from CraftOps (the lane's internals, private to craft_game.cpp).
#pragma once
#include "rpg/sim/game.h"

struct CraftHooks {
  //   NUMBERS / FORGE (rpg/sim/craft_game.cpp): the smith's, smelter's and tanner's work and lessons as dialogue options
  //   in [DLG_CRAFT, DLG_M6_END) (smelt, forge, tan, apprentice, study a foreign piece, steal patterns), and the forge
  //   screen they may open
  static void talk(Game& g, Actor& a);
  static bool choose(Game& g, const DlgOpt& o);
};
