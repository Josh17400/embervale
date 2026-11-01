// The TOWNSFOLK and CITIZENS lanes' hooks into the core loop (SOLID 4.1: they were Game::life*, Game::raidStep). The
// core (sim_schedule.cpp, game.cpp, game_rpg.cpp, ai.cpp, game_dialogue.cpp, quests.cpp ...) calls them where phase A
// put the calls; they are DEFINED in the lanes' own files (life_game.cpp and its cluster files, life_raids.cpp,
// life_quests.cpp). LifeHooks is a friend of Game (game.h). It is kept apart from LifeOps (life_ops.h: the lane's
// internals, with their file-local helpers), so the core includes this header and nothing else of the lane.
#pragma once
#include "rpg/sim/game.h"

struct LifeHooks {
  // ---- the hooks, called from the core loop
  //      where a quest giver is now when the map has no actor for them (LIFE lane, life_game.cpp: the resident's
  //      current spot from the census: a street, a stall, a friend's house; false: unknown). The tracked quest's pin
  //      asks it before falling back to the giver's door (questTarget).
  static bool giverSpot(const Game& g, const Quest& q, int& tx, int& ty);
  // (M5) resident `idx` of site `si` gave the player an open quest: it never emigrates or is carried off (life.cpp,
  // life_raids.cpp), so the quest can always be handed in
  static bool giverOpen(const Game& g, int si, int idx);
  //   TOWNSFOLK (rpg/sim/life_game.cpp): the actors.
  //   once per update step (after storyStep): residents near the player streamed in / put away by their plan, routes,
  //   furniture, chat pairs, barks, the lamplighter, the animals
  static void step(Game& g, float dt);
  //   a friendly townsperson's behaviour this step (updateFolk, after the flight from danger and before the stroll):
  //   true = handled (its plan drove it)
  static bool folk(Game& g, Actor& a, float dt);
  //   streamSitePeople: may this site's generator spawn come out now (false: its resident is indoors / away this hour)
  static bool spawnAllowed(Game& g, int site, const Spawn& sp);
  //   a person just came into play from a generator spawn (streamSitePeople, loadMapActors): bind it to its resident
  //   (Actor::resident, name, look), place it where its plan has it
  static void spawned(Game& g, Actor& a, const Spawn& sp);
  //   loadMapActors finished a building interior: populate it by schedule (who is home, at the tables, asleep upstairs)
  static void interior(Game& g);
  //   talkTo: greetings by name, barks of need ("HAVEN'T EATEN SINCE YESTERDAY"), the player's hooks (feed, employ,
  //   supply, befriend) as options in [DLG_LIFE, DLG_LIFE_END); called before warTalk / storyTalk
  static void talk(Game& g, Actor& a);
  static bool choose(Game& g, const DlgOpt& o);   // an option in [DLG_LIFE, DLG_LIFE_END): true handled
  static void kill(Game& g, const Actor& victim, int killer);   // a resident fell: grief, the census (Life::residentDied)
  //   the player used a prop (a bed or bedroll: sleep, Rested, skip the night; a table with food...): true handled.
  //   Called before the built-in prop handling.
  static bool useProp(Game& g, art::Prop p, int tx, int ty);
  static void ate(Game& g, const Item& food);   // the player ate something (useItem): Well Fed (15.2)
  static void slept(Game& g, int hours, bool bed);   // the player slept / rested (rest()): Rested when in a bed or bedroll (15.2)
  //   the player's buff multipliers (15.2), applied in updatePlayer / gainXp (1 = no effect)
  static float staminaRegenMul(const Game& g);
  static float healthRegenMul(const Game& g);
  static float xpMul(const Game& g);
  //   CITIZENS (rpg/sim/life_raids.cpp): night raids by monster pressure (VISION_PLAN 10.4 M5): once per update step
  static void raidStep(Game& g, float dt);
  //   CITIZENS (rpg/sim/life_quests.cpp): radiant quests from unmet needs ("THE BAKER HAS NO FLOUR"). hasOffer /
  //   makeOffer ask it first: true = this NPC offers `q` (filled in)
  static bool offer(const Game& g, const Actor& npc, Quest& q);
};
