// EMBERVALE story: the story engine's API for other lanes (SOLID 2a.5). Who may include it: any lane (rpg/sim, rpg/view
// and its scripts, tools) that needs the story people's slot ranges, the story lane's Game::marks tags (MK_*) or the
// lookups below; rpg/story's own files get it through story_internal.h, which holds the engine's private helpers.
// The debug and host entry points scripts drive (Engine::debugComplete ..., debugBout, debugEscort) are in story.h.
// Declarations only moved here from story_internal.h: values, names and signatures unchanged. Never includes a
// *_internal.h.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/story/dsl.h"
#include "rpg/story/story.h"

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
// (M6b) a script by id: the library's, or a generated story's (a saga spec id: saga::script); nullptr none
const dsl::Script* scriptById(const std::string& id);
const Binding* bindingOf(const Instance& in, const std::string& role);
Binding* bindingOf(Instance& in, const std::string& role);
// FNV-1a of a string (mark keys, the repetition guard)
uint64_t strHash64s(const std::string& s);

// where the player is (global tile; inside: the door or entrance they went in by)
void playerGlobal(const Game& g, int32_t& gx, int32_t& gy);
// a person's home settlement handle (their site, or the site of the building they stand in; -1)
int homeSiteOf(const Game& g, const Actor& a);
// a site's handle by id, loading its record when needed (-1: none)
int siteByIdLoad(Game& g, ew::Gid id);
// the kingdom's name ("A FAR REALM" when unknown)
std::string kingdomName(const Game& g, ew::Gid k);

// (P1b) may script `s` be OFFERED at settlement `hookSite` now (family test never; settle, needs, once site, cooldown)?
// The pickers ask it (Engine::offerFor / offerForPlace precheck); a forced start (tests, scripts) casts anywhere.
bool offerGates(const Game& g, const dsl::Script& s, int hookSite, std::string* why = nullptr);
// (P1b, REQUESTS lane: rpg/story/req/req_offers.cpp) the library request the place raises (a board, the news), as a
// library index (-1 none); Engine::offerForPlace asks it first (story_internal.h: requestOfferFor, for a person)
int requestOfferForPlace(Game& g, int hookKind, int site);

// lay lore props in a ruin map (rpg/sim/dungeon.cpp, beside genRuin): props[i] (art::Prop) for clue i, each on a
// deterministic tile (key: the site id) that fits it (slabs and graves against a wall in a room, a mural on a wall's foot,
// a journal on the floor, a statue across a room's middle), never on the way in, a spawn or another prop. Returns the
// props placed (tile, clue, prop); the map's solidity is rebuilt.
void placeLoreProps(Map& m, uint64_t key, const std::vector<int>& props, std::vector<Engine::PropRef>& out);

}  // namespace story
