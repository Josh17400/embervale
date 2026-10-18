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

namespace cult { struct Culture; }

namespace story {

// Game::marks tags of the story lane (game_internal.h Mk: STORY uses tags below 32; these mirror the enum entries the
// lane appended there, so the files that do not include game_internal.h agree with it)
constexpr uint64_t MK_STORY_GIVER = 8;   // an NPC (npcKey): the day they last started a story (one story per giver)
constexpr uint64_t MK_STORY_MARK = 9;    // a story's persistent mark (script id, mark name, its giver's binding)
constexpr uint64_t MK_BOARD_TAKEN = 10;  // a notice board (site id ^ entry): the day its notice was taken
constexpr uint64_t MK_TALE_TELLER = 11;  // (P1) a tale's teller in a town (key: town id x tale): who (tellerTag), and the
                                         // day the claim was last renewed (key ^ TELLER_DAY_SALT)
constexpr uint64_t MK_TALE_HEARD = 12;   // (P1) a teller (npcKey): the day the player last overheard them speak of their tale
// (P1b, lead) `once site` (script x settlement: the day it ended there) and `cooldown` (a settlement: the day its last
// cooldown script ended). Tags 16..23 are the P1b ENGINE lane's (story_game.cpp / story.cpp); the radiant jobs use
// 160..175 (JOBS lane), the requests 176..191 (REQUESTS lane), the new tales 192..199 (TALES lane).
constexpr uint64_t MK_P1B_PLAYED = 13;
constexpr uint64_t MK_P1B_COOLDOWN = 14;
constexpr uint64_t MK_P1B_GOSSIP = 15;   // a teller (npcKey) of a P1b item: the day their gossip line was last overheard
uint64_t storyMarkKey(uint64_t id, uint64_t tag);   // == gsim::markKey(id, (Mk)tag)
// (P1b) may script `s` be OFFERED at settlement `hookSite` now (family test never; settle, needs, once site, cooldown)?
// The pickers ask it (Engine::offerFor / offerForPlace precheck); a forced start (tests, scripts) casts anywhere.
bool offerGates(const Game& g, const dsl::Script& s, int hookSite, std::string* why = nullptr);
// (P1b) the sea within ~360 tiles of a global tile (macro samples)
bool nearCoast(const Game& g, int32_t gx, int32_t gy);
// (P1b) the pickers' cheap look before casting (offers.cpp): the world has what the roles need (a war, an event) and
// offerGates holds
bool offerPrecheck(const Game& g, const dsl::Script& s, int hookSite);
// (P1b, REQUESTS lane: rpg/story/req/req_offers.cpp) the library request (`family request`) person `a` raises now, or
// the place raises (a board, the news), as a library index (-1 none). Engine::offerFor / offerForPlace ask these
// first and never offer a request themselves.
int requestOfferFor(Game& g, const Actor& a);
int requestOfferForPlace(Game& g, int hookKind, int site);

// (P1 Fresh Roads, offers.cpp) at most one teller per tale per town: true when `a` is that tale's teller in town `site`
// (claiming it when nobody holds it, or the holder has not been met for TELLER_DAYS days, is dead or has started another
// story). Deterministic in the order people are asked; the claim lives in Game::marks (saved).
constexpr int TELLER_DAYS = 14;
bool tellerClaim(Game& g, const Actor& a, int site, uint64_t tale);
// (P1) a library tale hooked on a realm event (hook event <type>): the freshest such event that has reached the site and
// that the tale has not yet answered (its MK_STORY_MARK); 0 none. eventStories marks the id answered when it begins.
uint32_t eventTaleFresh(const Game& g, const dsl::Script& s, int site);
uint64_t eventTaleMarkKey(const dsl::Script& s);

// actors the engine spawns carry a slot in this range (a person or foe of a running story: (instance id % 1024) *
// PERSON_ROLES + its role index), or HERALD_SLOT (a capital's herald). Never fromMap: the town's streaming leaves them
// alone. (M6b) 32 roles per script (campaigns chain arcs with their own %roles); the validator refuses more.
constexpr int PERSON_ROLES = 32;
constexpr int PERSON_SLOT0 = 50000, PERSON_SLOT1 = 50000 + 1024 * PERSON_ROLES;
constexpr int HERALD_SLOT = 950;
inline bool isStoryPerson(const Actor& a) { return a.slot >= PERSON_SLOT0 && a.slot < PERSON_SLOT1; }
inline int personSlot(uint32_t inst, int role) { return PERSON_SLOT0 + (int)(inst % 1024u) * PERSON_ROLES + (role & (PERSON_ROLES - 1)); }
// (P1b ENGINE lane) the beasts of a drove an escort drives (goal escort ... drove): up to DROVE_MAX a story, slot
// DROVE_SLOT0 + (instance % 1024) * DROVE_MAX + k. Never fromMap (the streaming leaves them alone), never saved (the
// engine sets them out again beside their drover)
constexpr int DROVE_MAX = 8;
constexpr int DROVE_SLOT0 = PERSON_SLOT1, DROVE_SLOT1 = PERSON_SLOT1 + 1024 * DROVE_MAX;
inline bool isDroveBeast(const Actor& a) { return a.slot >= DROVE_SLOT0 && a.slot < DROVE_SLOT1; }
// (P1b ENGINE lane) marks: a beacon a story lit (key: its global tile; the day it was lit)
constexpr uint64_t MK_P1B_BEACON = 16;
// (P1b fixer) a site (its gid) whose creatures a FINISHED story calmed for good (`do calm` in an end stage: the day)
constexpr uint64_t MK_P1B_CALM = 17;

// the library's script of an instance (nullptr: a script the library no longer has)
const dsl::Script* scriptOf(const Instance& in);
int scriptIndex(const std::string& id);
// (M6b) a script by id: the library's, or a generated story's (a saga spec id: saga::script); nullptr none
const dsl::Script* scriptById(const std::string& id);
// (M6b) Binding::trade of a resident role bound to a real census resident (id = life::npcId(site, idx)); a resident
// role the caster had to invent (no census there) keeps trade 0 and an invented id
constexpr uint8_t RESIDENT_CENSUS = 0xFF;
// FNV-1a of a string (mark keys, the repetition guard)
uint64_t strHash64s(const std::string& s);
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
// danger D, as a Saga reward (a finale's when `finale`)
Item storyLoot(int D, int playerLevel, uint64_t seed, bool finale);
// the danger (World site level) a story's rewards are paid at: its hook settlement's (1 when unknown)
int storyDanger(Game& g, const Instance& in);
const Binding* bindingOf(const Instance& in, const std::string& role);
Binding* bindingOf(Instance& in, const std::string& role);

// where the player is (global tile; inside: the door or entrance they went in by)
void playerGlobal(const Game& g, int32_t& gx, int32_t& gy);
// a person's home settlement handle (their site, or the site of the building they stand in; -1)
int homeSiteOf(const Game& g, const Actor& a);
// a site's handle by id, loading its record when needed (-1: none)
int siteByIdLoad(Game& g, ew::Gid id);
// the kingdom whose land a global tile is (0: the wildlands)
ew::Gid landAt(const Game& g, int32_t gx, int32_t gy);
// the kingdom's name ("A FAR REALM" when unknown)
std::string kingdomName(const Game& g, ew::Gid k);
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

// lay lore props in a ruin map (rpg/sim/dungeon.cpp, beside genRuin): props[i] (art::Prop) for clue i, each on a
// deterministic tile (key: the site id) that fits it (slabs and graves against a wall in a room, a mural on a wall's foot,
// a journal on the floor, a statue across a room's middle), never on the way in, a spawn or another prop. Returns the
// props placed (tile, clue, prop); the map's solidity is rebuilt.
void placeLoreProps(Map& m, uint64_t key, const std::vector<int>& props, std::vector<Engine::PropRef>& out);

// a free walkable overworld tile near (tx, ty) for a spawned person (window tiles; false none)
bool freeTileNear(const Game& g, int tx, int ty, int& ox, int& oy, uint32_t salt);
void storyAmbush(Game& g, Instance& in, int mword, int count);   // (P1b) n foes from 8-12 tiles round the player
bool storyOnRoad(const Instance& in);                            // (P1b) an escort / follower walks with the player now

}  // namespace story
