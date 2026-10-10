// M4 "Banners": helpers shared by the story engine's files (rpg/story/*.cpp). STORY lane. Not used outside rpg/story
// (and Game's story hooks in story_game.cpp).
#pragma once
#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/story/dsl.h"
#include "rpg/story/story.h"

namespace cult { struct Culture; }

namespace story {

// Game::marks tags of the story lane (game_internal.h Mk: STORY uses tags below 32; these mirror the enum entries the
// lane appended there, so the files that do not include game_internal.h agree with it)
constexpr uint64_t MK_STORY_GIVER = 8;   // an NPC (npcKey): the day they last started a story (one story per giver)
constexpr uint64_t MK_STORY_MARK = 9;    // a story's persistent mark (script id, mark name, its giver's binding)
constexpr uint64_t MK_BOARD_TAKEN = 10;  // a notice board (site id ^ entry): the day its notice was taken
uint64_t storyMarkKey(uint64_t id, uint64_t tag);   // == gsim::markKey(id, (Mk)tag)

// actors the engine spawns carry a slot in this range (a person or foe of a running story: (instance id % 1024) *
// PERSON_ROLES + its role index), or HERALD_SLOT (a capital's herald). Never fromMap: the town's streaming leaves them
// alone. (M6b) 32 roles per script (campaigns chain arcs with their own %roles); the validator refuses more.
constexpr int PERSON_ROLES = 32;
constexpr int PERSON_SLOT0 = 50000, PERSON_SLOT1 = 50000 + 1024 * PERSON_ROLES;
constexpr int HERALD_SLOT = 950;
inline bool isStoryPerson(const Actor& a) { return a.slot >= PERSON_SLOT0 && a.slot < PERSON_SLOT1; }
inline int personSlot(uint32_t inst, int role) { return PERSON_SLOT0 + (int)(inst % 1024u) * PERSON_ROLES + (role & (PERSON_ROLES - 1)); }

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
// gear rolled at item level D (rich: uncommon, great: rare; never above). tier 0 small .. 3 great. Pure in its inputs.
struct RewardRoll { int gold = 0, xp = 0; bool hasItem = false; Item item; };
RewardRoll rewardRoll(int D, int tier, uint64_t culture, const cult::Culture* maker, uint64_t seed);
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

}  // namespace story
