// M4 "Banners": helpers shared by the story engine's files (rpg/story/*.cpp). STORY lane. Not used outside rpg/story
// (and Game's story hooks in story_game.cpp).
#pragma once
#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/gear.h"
#include "rpg/story/dsl.h"
#include "rpg/story/story.h"
#include "rpg/story/story_api.h"   // (SOLID 2a.5) the subset other lanes use: slots, MK_* tags, lookups

namespace cult { struct Culture; }

namespace story {

// (P1b) the sea within ~360 tiles of a global tile (macro samples)
bool nearCoast(const Game& g, int32_t gx, int32_t gy);
// (P1b) the pickers' cheap look before casting (offers.cpp): the world has what the roles need (a war, an event) and
// offerGates holds
bool offerPrecheck(const Game& g, const dsl::Script& s, int hookSite);
// (P1b, REQUESTS lane: rpg/story/req/req_offers.cpp) the library request (`family request`) person `a` raises now, or
// the place raises (a board, the news), as a library index (-1 none). Engine::offerFor / offerForPlace ask these
// first and never offer a request themselves.
int requestOfferFor(Game& g, const Actor& a);

// (P1 Fresh Roads, offers.cpp) at most one teller per tale per town: true when `a` is that tale's teller in town `site`
// (claiming it when nobody holds it, or the holder has not been met for TELLER_DAYS days, is dead or has started another
// story). Deterministic in the order people are asked; the claim lives in Game::marks (saved).
constexpr int TELLER_DAYS = 14;
bool tellerClaim(Game& g, const Actor& a, int site, uint64_t tale);
// (P1) a library tale hooked on a realm event (hook event <type>): the freshest such event that has reached the site and
// that the tale has not yet answered (its MK_STORY_MARK); 0 none. eventStories marks the id answered when it begins.
uint32_t eventTaleFresh(const Game& g, const dsl::Script& s, int site);
uint64_t eventTaleMarkKey(const dsl::Script& s);


int scriptIndex(const std::string& id);
// (M6b) Binding::trade of a resident role bound to a real census resident (id = life::npcId(site, idx)); a resident
// role the caster had to invent (no census there) keeps trade 0 and an invented id
constexpr uint8_t RESIDENT_CENSUS = 0xFF;
// (M6b, COMPOSER lane) the census resident an actor embodies: its settlement's id and census index (false: none)
bool residentOfActor(const Game& g, const Actor& a, ew::Gid& site, int& idx);
// (M6b) the census index of a resident binding (RESIDENT_CENSUS; -1: an invented one, or the census is not built)
int residentIndexOf(const Game& g, const Binding& b);
// (M6b) what `do reward` pays at danger D (the hook settlement's level): gold and XP, and for rich / great a piece of
// gear. tier 0 small .. 3 great. Pure in its inputs.
// (M8b Treasure, 15.27.4) the piece is rolled by the gear rules (gear::rollRarity + makeGearC at gear::itemLevelFor)
// from rewardSource: by the story's danger and the finder's level gate. src: LootSrc::Saga (a stage on the way:
// Uncommon to Rare, great boosted a step), SagaFinale (a tale's success ending: Uncommon or better, boosted one step,
// great two), CampaignFinale (a campaign's success ending: the main way to an early Legendary or Mythic: Legendary
// guaranteed, ungated; great boosted a step). A hard story (D >= the player's level + 3) is boosted one step more.
struct RewardRoll { int gold = 0, xp = 0; bool hasItem = false; Item item; };
gear::DropSource rewardSource(int D, int tier, uint64_t culture, int playerLevel, gear::LootSrc src);
RewardRoll rewardRoll(int D, int tier, uint64_t culture, const cult::Culture* maker, uint64_t seed, int playerLevel = 1,
                      gear::LootSrc src = gear::LootSrc::Saga);
// (M8b) the reward source of a stage of a script: a success ending is a finale (a campaign's: tier 3), else Saga
gear::LootSrc rewardSrcOf(const dsl::Script& s, const dsl::StageDef& G);
// (M8b) `do loot` (the M4 tales' random find; no library script uses it now): a piece of gear by the gear rules at
// danger D, as a Saga reward (a finale's when `finale`). (P3 LOOT) maker: the hook's people (the reward's culture; null:
// unknown, the piece is made with no maker)
Item storyLoot(int D, int playerLevel, uint64_t seed, bool finale, const cult::Culture* maker = nullptr);
// the danger (World site level) a story's rewards are paid at: its hook settlement's (1 when unknown)
int storyDanger(Game& g, const Instance& in);

// the kingdom whose land a global tile is (0: the wildlands)
ew::Gid landAt(const Game& g, int32_t gx, int32_t gy);
// a culture-aware person name for a story person (deterministic in the key)
std::string personNameIn(const Game& g, uint64_t culture, uint64_t key, bool female);

// (P1b) the cast key Engine::start uses for a generated story (saga or radiant job) begun by actor A (null: a place) at
// hookSite: an offer that try-casts with castScript(g, s, A->id, hookSite, offerCastKey(...), ...) casts the same people
uint64_t offerCastKey(const Game& g, const dsl::Script& s, const Actor* A, int hookSite);
// the caster (caster.cpp): bind every role of a script from a hook context; false with a reason when it cannot
bool castScript(Game& g, const dsl::Script& s, int hookActor, int hookSite, uint64_t key, std::vector<Binding>& out, std::string& why);
// sites bound by running instances (the caster prefers others)
std::set<uint64_t> boundSites(const Game& g);

// text with the cast filled in (story.cpp)
std::string fillText(const Game& g, const Instance& in, const std::string& text);

// the realm's record of a ruin as the story knows it (lore.cpp)
realm::RuinRecord& ruinCached(Game& g, ew::Gid site);
const char* causeWords(realm::FallCause c);


// a free walkable overworld tile near (tx, ty) for a spawned person (window tiles; false none)
bool freeTileNear(const Game& g, int tx, int ty, int& ox, int& oy, uint32_t salt);
void storyAmbush(Game& g, Instance& in, int mword, int count);   // (P1b) n foes from 8-12 tiles round the player
bool storyOnRoad(const Instance& in);                            // (P1b) an escort / follower walks with the player now

}  // namespace story
