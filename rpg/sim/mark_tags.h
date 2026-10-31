// Game::marks tags: every saved tag by name, with its owner lane and its value, and markKey (SOLID 2a.4 SIM-SEAMS).
// Who may include it: rpg/sim, rpg/story and tools/tests (the view reads marks through game_internal.h, which includes
// this file). Nothing here depends on Game.
//
// A mark is Game::marks[markKey(id, tag)] = i32. The key mixes a stable id (a site's Gid, an npcKey, a life::npcId, a
// quest id ...) with a tag, and the marks map is saved as u64 -> i32 (game_save.cpp), so the tag VALUES and the mix are
// part of the save format: never renumber a tag, never change markKey. `rpg_test --marks` (tools/tests/test_marks.cpp)
// hashes known keys against hard-coded values to prove it.
//
// The lanes still define their own named constants in their own files (foes.h MK_FOES_*, story_internal.h MK_*,
// PartyOps::MK_*, craft_game.cpp, jobs.cpp, sea_trade.cpp ...). kMarkTags below is the register of all of them, gathered
// by grep across every lane at 4c0c9a1; a new tag goes into its lane's range AND into kMarkTags, and the static_assert
// at the bottom proves the values stay unique (apart from the documented shares).
#pragma once
#include <cstddef>
#include <cstdint>
#include "rpg/world/ids.h"

namespace gsim {

// M2 (SIM lane): Game::marks key spaces. A key is a stable id (a site's Gid, an npcKey, a quest id) mixed with a tag,
// so facts about different things never collide and survive saves (marks is saved as u64 -> i32).
enum class Mk : uint64_t {
  Stones = 1,      // a standing-stones vignette (site id): the day its heart stone last blessed the player
  Toll = 2,        // a toll bridge (site id): the day the toll was paid (free passage that day)
  Grave = 3,       // a lone grave (site id): read (1)
  News = 4,        // a traveller (npcKey): the day they last told the player a rumour
  HeirChest = 5,   // an Heirloom quest (quest id): the tile (y * w + x) of its chest in the dungeon map
  LastOffer = 6,   // a quest giver (npcKey): 1 + the QType of the last job they gave (the next one differs)
  Ambush = 7,      // a caravan vignette (site id): the ambush has sprung (1)
  // M4 STORY lane (rpg/story/story_internal.h mirrors these as story::MK_*)
  StoryGiver = 8,  // an NPC (npcKey) who started a story: the day (one story per giver, then they remember)
  StoryMark = 9,   // a story's persistent mark (script, mark name, its giver), an event story's last event id
  BoardTaken = 10, // a notice board notice (site id ^ entry): the day it was taken
  // M4: the STORY lane (owner of this file in M4) appends its tags here, below 32. The WARDS lane uses tags 32..47 and
  // the REALM lane 48..63, each defined in its own files as (Mk)(32 + n), so nobody else edits this enum.
  // M6: the FOES lane uses 64..79 (rpg/sim/foes.h MK_FOES_*), the NUMBERS / FORGE lane 80..95 (craft_game.cpp).
  // M7: the HOMESTEAD lane uses 96..111 (rpg/sim/home_game.cpp).
  // M8: the FELLOWS lane uses 112..127 (rpg/sim/party_*.cpp: PartyOps::MK_*). (M10 SEA: 200, sea_trade.cpp.)
  // P1 Fresh Roads: the JOBS lane uses 128..143 (quests.cpp, jobs.cpp), the LIFE lane 144..159 (life_game.cpp: gossip
  // heard, incidents seen), the STORY lane keeps its tags below 32 (story_internal.h); each defined in its own files.
};
inline uint64_t markKey(uint64_t id, Mk tag) { return ew::mix64(id ^ ((uint64_t)tag * 0xD1B54A32D192ED03ull)); }

// ---- the register
// The owner lanes and the ranges they hold (lo..hi inclusive). WARDS and REALM hold ranges but define no tag yet.
enum class MarkOwner : uint8_t { Sim, Story, Wards, Realm, Foes, Craft, Home, Fellows, Jobs, Life, Requests, Tales, Sea, View };
struct MarkRange { MarkOwner owner; uint64_t lo, hi; };
inline constexpr MarkRange kMarkRanges[] = {
    {MarkOwner::Sim, 1, 10},          // the Mk enum above
    {MarkOwner::Story, 11, 31},       // story_internal.h (16..23 the P1b ENGINE lane's)
    {MarkOwner::Wards, 32, 47},
    {MarkOwner::Realm, 48, 63},
    {MarkOwner::Foes, 64, 79},        // foes.h
    {MarkOwner::Craft, 80, 95},       // craft_game.cpp (and the LIFE lane's dialogue marks: see kMarkShareLifeDialogue)
    {MarkOwner::Home, 96, 111},
    {MarkOwner::Fellows, 112, 127},   // PartyOps::MK_* (game_internal.h)
    {MarkOwner::Jobs, 128, 143},      // jobs.cpp (story_internal.h also names 160..175 for the radiant jobs: unused)
    {MarkOwner::Life, 144, 159},      // LifeOps::mkLife (life_ops.h)
    {MarkOwner::Requests, 176, 191},  // req_offers.cpp
    {MarkOwner::Tales, 192, 199},     // story_internal.h names it; no tag yet
    {MarkOwner::Sea, 200, 200},       // sea_trade.cpp
};

// share: 0 = the value is this tag's alone; otherwise the id of a documented share (every member of one share has the
// same value and keys a disjoint id space, so their keys never meet).
//   kMarkShareBiome: the view's biome-seen key (render.cpp: tag 8 with ids 0x7E000 + eco) shares StoryGiver's tag 8.
//     StoryGiver keys an npcKey (a 64-bit mix, or on the classic island (site+1)<<24 ^ (bldg+1)<<12 ^ slot, which is
//     1<<24 or above for any NPC with a site), so the two id spaces are disjoint in practice.
//   kMarkShareLifeDialogue: the LIFE lane's townsfolk dialogue marks (LifeOps::markOf, life_lines.cpp: (Mk)(80 + n),
//     key life::npcId(site, census index)) share 80 and 81 with the CRAFT lane's smith marks (key npcKey). life::npcId
//     is mix64(site * phi ^ (idx + 0x4E50C1D)) | 1 and npcKey mix64(site * phi ^ mix64(bldg + 0x51ED27) ^ slot), or the
//     classic island's small handle key: different mixes of different inputs, so the id spaces are disjoint.
//     (Found by the 2a.4 grep; the plan's table did not list it. Recorded, not changed: changing it changes saves.)
inline constexpr int kMarkShareBiome = 1;
inline constexpr int kMarkShareLifeDialogue = 2;
inline constexpr uint64_t kBiomeSeenTag = 8;          // the view's biome-seen key: markKey(kBiomeSeenIdBase + eco, 8)
inline constexpr uint64_t kBiomeSeenIdBase = 0x7E000;

struct MarkTag { const char* name; MarkOwner owner; uint64_t value; int share; const char* key; };
inline constexpr MarkTag kMarkTags[] = {
    // SIM (the Mk enum)
    {"Mk::Stones", MarkOwner::Sim, 1, 0, "site id"},
    {"Mk::Toll", MarkOwner::Sim, 2, 0, "site id"},
    {"Mk::Grave", MarkOwner::Sim, 3, 0, "site id"},
    {"Mk::News", MarkOwner::Sim, 4, 0, "npcKey"},
    {"Mk::HeirChest", MarkOwner::Sim, 5, 0, "quest id"},
    {"Mk::LastOffer", MarkOwner::Sim, 6, 0, "npcKey"},
    {"Mk::Ambush", MarkOwner::Sim, 7, 0, "site id"},
    {"Mk::StoryGiver = story::MK_STORY_GIVER", MarkOwner::Sim, 8, kMarkShareBiome, "npcKey"},
    {"Mk::StoryMark = story::MK_STORY_MARK", MarkOwner::Sim, 9, 0, "script / mark / binding hash"},
    {"Mk::BoardTaken = story::MK_BOARD_TAKEN", MarkOwner::Sim, 10, 0, "site id ^ entry"},
    // STORY (story_internal.h)
    {"story::MK_TALE_TELLER", MarkOwner::Story, 11, 0, "town id x tale (and ^ TELLER_DAY_SALT)"},
    {"story::MK_TALE_HEARD", MarkOwner::Story, 12, 0, "npcKey"},
    {"story::MK_P1B_PLAYED", MarkOwner::Story, 13, 0, "script x settlement"},
    {"story::MK_P1B_COOLDOWN", MarkOwner::Story, 14, 0, "settlement"},
    {"story::MK_P1B_GOSSIP = LifeOps::MK_P1B_GOSSIP_TAG", MarkOwner::Story, 15, 0, "npcKey / board key"},
    {"story::MK_P1B_BEACON", MarkOwner::Story, 16, 0, "global tile"},
    {"story::MK_P1B_CALM", MarkOwner::Story, 17, 0, "site gid"},
    // FOES (foes.h; sea_foes.cpp kBossSlainTag restates 65)
    {"foes::MK_FOES_NAMED_SLAIN", MarkOwner::Foes, 64, 0, "unique id"},
    {"foes::MK_FOES_BOSS_SLAIN", MarkOwner::Foes, 65, 0, "world boss id"},
    {"foes::MK_FOES_RUMOUR", MarkOwner::Foes, 66, 0, "unique id"},
    {"foes::MK_FOES_RAIDDAY", MarkOwner::Foes, 67, 0, "0"},
    // CRAFT (craft_game.cpp)
    {"MK_THEFT", MarkOwner::Craft, 80, kMarkShareLifeDialogue, "npcKey"},
    {"MK_BARRED", MarkOwner::Craft, 81, kMarkShareLifeDialogue, "npcKey"},
    // LIFE dialogue marks in the CRAFT range (LifeOps::markOf(g, si, idx, n) = (Mk)(80 + n))
    {"LifeOps::markOf 0 (kindness count)", MarkOwner::Life, 80, kMarkShareLifeDialogue, "life::npcId"},
    {"LifeOps::markOf 1 (last chat day)", MarkOwner::Life, 81, kMarkShareLifeDialogue, "life::npcId"},
    {"LifeOps::markOf 2 (last hire day)", MarkOwner::Life, 82, 0, "life::npcId"},
    // HOME (home_actors.cpp)
    {"(Mk)(96 + 1) beast patted", MarkOwner::Home, 97, 0, "plot id ^ seed << 16"},
    // FELLOWS (PartyOps, game_internal.h)
    {"PartyOps::MK_HIRED", MarkOwner::Fellows, 112, 0, "inn bldg id * 4 + slot"},
    {"PartyOps::MK_PUP", MarkOwner::Fellows, 113, 0, "den id / npcKey / life::npcId"},
    // JOBS (jobs.cpp)
    {"MK_JOB_WEEK", MarkOwner::Jobs, 128, 0, "npcKey"},
    // LIFE (LifeOps::mkLife, life_ops.h)
    {"LifeOps::mkLife(0) giver overheard", MarkOwner::Life, 144, 0, "npcKey"},
    {"LifeOps::mkLife(1) leads overheard", MarkOwner::Life, 145, 0, "site id"},
    // REQUESTS (req_offers.cpp)
    {"MK_REQ_RAISE", MarkOwner::Requests, 176, 0, "site id ^ request hash"},
    // SEA (sea_trade.cpp)
    {"MK_SEA_EARLYSALE", MarkOwner::Sea, 200, 0, "0"},
    // VIEW (render.cpp): the biome-seen key
    {"view biome seen", MarkOwner::View, kBiomeSeenTag, kMarkShareBiome, "0x7E000 + eco"},
};
inline constexpr size_t kMarkTagCount = sizeof(kMarkTags) / sizeof(kMarkTags[0]);

namespace mark_detail {
// two tags may hold one value only as members of the same documented share
constexpr bool tagsUnique() {
  for (size_t i = 0; i < kMarkTagCount; i++)
    for (size_t j = i + 1; j < kMarkTagCount; j++)
      if (kMarkTags[i].value == kMarkTags[j].value && (kMarkTags[i].share == 0 || kMarkTags[i].share != kMarkTags[j].share))
        return false;
  return true;
}
// every share has at least two members
constexpr bool sharesPaired() {
  for (size_t i = 0; i < kMarkTagCount; i++) {
    if (!kMarkTags[i].share) continue;
    int n = 0;
    for (size_t j = 0; j < kMarkTagCount; j++) n += kMarkTags[j].share == kMarkTags[i].share && kMarkTags[j].value == kMarkTags[i].value;
    if (n < 2) return false;
  }
  return true;
}
// the lane ranges do not overlap
constexpr bool rangesDisjoint() {
  constexpr size_t n = sizeof(kMarkRanges) / sizeof(kMarkRanges[0]);
  for (size_t i = 0; i < n; i++) {
    if (kMarkRanges[i].lo > kMarkRanges[i].hi) return false;
    for (size_t j = i + 1; j < n; j++)
      if (kMarkRanges[i].lo <= kMarkRanges[j].hi && kMarkRanges[j].lo <= kMarkRanges[i].hi) return false;
  }
  return true;
}
}  // namespace mark_detail
static_assert(mark_detail::tagsUnique(), "Game::marks tag values are unique apart from the documented shares");
static_assert(mark_detail::sharesPaired(), "a documented share has at least two members");
static_assert(mark_detail::rangesDisjoint(), "the lanes' Game::marks ranges do not overlap");
static_assert((uint64_t)Mk::BoardTaken == 10 && (uint64_t)Mk::Stones == 1, "the Mk enum's values are saved");

}  // namespace gsim
