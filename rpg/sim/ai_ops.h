// The actors' behaviour (SOLID 4.1: these were Game members): the hostiles' AI by species (ai_beasts.cpp: the kind
// flags, pursuit, attacks, packs, the dragon, wandering), the friendly folk (ai.cpp: guards, militia, the flight from
// danger, the stall keeper, the missing person), town defence (the bells, the shelters) and the hostiles' choice of
// target. Game::update (sim_schedule.cpp) and updateActor (game.cpp) call them. AiOps is a friend of Game (game.h).
#pragma once
#include "rpg/sim/game.h"

struct AiOps {
  static void heavySlam(Game& g, Actor& a);
  static void updateTownDefence(Game& g, float dt);
  static int pickTarget(const Game& g, const Actor& a);   // hostiles: the actors index to fight (0 = the player)
  static int homeDoor(Game& g, Actor& a);   // the building a townsperson shelters in (chosen once), -1 none
  static void updateFolk(Game& g, Actor& a, float dt);   // friendly NPC behaviour (guards, militia, fleeing, wandering)
  static void updateAI(Game& g, Actor& a, float dt);
  // updateFolk's pieces (ai.cpp): the missing person, the watch's fight, the flight from danger, the stall keeper
  // (true: this step is done)
  static void folkMissing(Game& g, Actor& a, float dt, bool talking);
  static bool folkGuard(Game& g, Actor& a, float dt, bool town, bool ringing, bool soldier, bool guard);
  static bool folkShelter(Game& g, Actor& a, float dt, bool ringing);
  static bool folkStall(Game& g, Actor& a, float dt);
  static Game::BeastKind beastKindOf(const Actor& a);
  static void beastDragon(Game& g, Actor& a, Actor& p, Vec2 toP, float dist, float dt);
  static void beastStep(Game& g, Actor& a, Actor& p, Vec2 toP, float dist, float dt, float slow);
  static void beastPursue(Game& g, Actor& a, Actor& p, Vec2 toP, float dist, float dt, float slow, const Game::BeastKind& k);
  static void beastAttack(Game& g, Actor& a, Actor& p, float dist, float dt, const Game::BeastKind& k);
  static void beastPackAttack(Game& g, Actor& a, Actor& p, float dist, bool inMelee, const Game::BeastKind& k);
  static void beastWander(Game& g, Actor& a, float dt, float slow);
};
