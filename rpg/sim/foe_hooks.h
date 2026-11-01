// The FOES lane's hooks into the core loop (SOLID 4.1: they were Game::foe*): elites, champion packs, named uniques,
// boss phases, world bosses, soldiers' and bandits' culture arms. The core (sim_schedule.cpp, game.cpp, game_combat.cpp,
// game_dialogue.cpp ...) calls them where phase A put the calls; they are DEFINED in rpg/sim/foes_game.cpp. FoeHooks is
// a friend of Game (game.h); it is kept apart from FoeOps (the lane's internals, private to foes_game.cpp).
#pragma once
#include "rpg/sim/game.h"

struct FoeHooks {
  //   a monster or a bandit just came into play (end of spawnMonster / spawnHuman for bandits, after applyLevel): roll its
  //   rank and affixes, set its variant look, name and stats. D: the danger it was spawned at (its level)
  static void spawned(Game& g, Actor& a, int D);
  //   once per update step (after raidStep): affix behaviours (summoner, blinking, burning trail, warded, regenerating,
  //   frenzied), boss phases, champion packs, named uniques near the player, roaming world bosses and their raids
  static void step(Game& g, float dt);
  //   (M6 fixer r2) generate the named uniques' regions and the world bosses' kingdom cells round the player now (a new
  //   game's start, behind the loading): each cold one costs a frame hitch of tens of ms when foeStep meets it in play
  static void warm(Game& g);
  //   a blow is about to land on `victim` (damage(), after armour): returns the damage to apply (warded, armoured...) and
  //   applies on-hit effects (vampiric healing, frost-bound slows, the volatile's...). attacker: actor id or -1
  static float hit(Game& g, Actor& victim, float dmg, int attacker);
  //   something died (kill(), before the loot drops): splitting, volatile, a named unique or world boss slain (marks,
  //   news, the realm's BeastSlain)
  static void killed(Game& g, const Actor& a, int killer);
  //   talkTo: innkeepers and guards warn of the named uniques and world bosses near (options in [DLG_FOES, DLG_CRAFT))
  static void talk(Game& g, Actor& a);
  static bool choose(Game& g, const DlgOpt& o);
};
