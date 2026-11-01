// The quests' machinery (SOLID 4.1: these were Game members): offers and their builders, the M2 quest types' ticks,
// markers, journal lines, chests, escorts and failures, the main quest's stages and the opening's first weapon.
// DEFINED in rpg/sim/quests.cpp, quest_offers.cpp and game_quests.cpp. Game keeps what the view, scripts and the
// story engine call (offerFor, debugAccept, questStatus, questTarget, acceptQuest, hasOffer ...). QuestOps is a friend
// of Game (game.h).
#pragma once
#include <string>
#include "rpg/sim/game.h"

struct QuestOps {
  static int openingQuest(const Game& g);   // id of the opening quest "A BLADE OF YOUR OWN" while it is open (-1 otherwise)
  static void giveFirstWeapon(Game& g, Quest& q);   // the opening: the start village innkeeper hands over the old blade
  static void questKill(Game& g, const Actor& a);
  static void checkDungeonCleared(Game& g, int site);
  static Quest makeOffer(Game& g, const Actor& npc, bool* ok = nullptr);   // (P1 fixer) ok: false when nothing could be offered
  static void completeQuest(Game& g, Quest& q);
  static void advanceMain(Game& g, int stage);
  static std::string questReminder(const Game& g, const Quest& q);   // (M7b) the giver's own words while the job is under way
  static std::string acceptLine(const Game& g, const Quest& q);   // (M7b) the giver's words when the job is taken
  static int pickRadiant(Game& g, SiteType t, int32_t gx, int32_t gy, Rng& r, bool& danger, int exclude = -1);
  static int pickDestination(Game& g, int32_t gx, int32_t gy, Rng& r);   // Deliver: a settlement 150-700 tiles away
  static void questTick(Game& g, float dt);   // per step: Protect waves, escorts, Deliver recipients, failures
  static void questMapLoaded(Game& g);   // a cave, ruin or building map was just made: quest chests and people
  static bool questTargetM2(const Game& g, const Quest& q, int& tx, int& ty);   // the M2 types' markers (false: the classic rules)
  static std::string questStatusM2(const Game& g, const Quest& q);   // the M2 types' journal lines ("" none)
  static std::string turnInLine(const Game& g, const Quest& q);   // what the giver says when the reward is collected
  static std::string questDialogue(Game& g, Actor& a);   // M2 quest talk with this NPC: options added to dlg
  static void questLeftSite(Game& g, int site);   // the player came out of a site: escorts follow, missing persons are safe
  static bool questChest(Game& g, int tx, int ty);   // a quest chest in the current map was opened (true: handled)
  static void questSpawned(Game& g, Actor& a);   // a person just streamed in: a named chief, a Deliver recipient
  static void questActorDown(Game& g, const Actor& a);   // someone fell: a farmer, a missing person
  static bool isRecipient(const Game& g, const Quest& q, const Actor& a);
  static void questFail(Game& g, Quest& q, const std::string& why);
  static int escortFor(const Game& g, const Quest& q);   // actors index of the quest's missing person (-1 none)
  static void spawnEscort(Game& g, Quest& q, Vec2 at);
  static Vec2 questChestTile(const Game& g, const Quest& q);   // a far floor tile of the current dungeon map (tile coordinates)
  // offerFor's pieces (quest_offers.cpp): one builder per kind of offer (false: it does not fit), the pitch
  static bool offerBuild(Game& g, Game::OfferCtx& oc, int kind);
  static bool offerClear(Game& g, Game::OfferCtx& oc, int kind);
  static bool offerBounty(Game& g, Game::OfferCtx& oc, int kind);
  static bool offerHunt(Game& g, Game::OfferCtx& oc);
  static bool offerDeliver(Game& g, Game::OfferCtx& oc);
  static bool offerHeirloom(Game& g, Game::OfferCtx& oc);
  static bool offerStolen(Game& g, Game::OfferCtx& oc);
  static bool offerSupply(Game& g, Game::OfferCtx& oc);
  static bool offerMissing(Game& g, Game::OfferCtx& oc);
  static bool offerProtect(Game& g, Game::OfferCtx& oc);
  static void offerPitch(Game& g, Game::OfferCtx& oc, int madeKind);
};
