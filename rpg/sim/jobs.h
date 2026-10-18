// P1 "Fresh Roads" (VISION_PLAN 15.34): the settlements' weekly job pools, the journal's LEADS and the street errands.
// Phase A (lead) froze the records and the Game calls below; the JOBS lane owns this header and rpg/sim/jobs.cpp after
// phase A (it may ADD fields and functions, never rename, remove or change the meaning of what is listed here; a layout
// change bumps JOBS_BLOCK_VER and the integrator regenerates the save fixture once).
//
//   Pool / PoolJob   one settlement's jobs for one week (week = day / 7), worked out from its real needs the first time
//                    anyone there is asked (JOBS lane, quests.cpp: the farm's wolves, the smith's ore, the inn's
//                    deliveries, a den nearby, a camp on the road ...) and frozen for the week; each job names the
//                    resident who will ask for it. This replaces the lifetime per-person caps and the 1-in-3 hash of
//                    the M2 hasOffer (Game::npcQuestsDone stays: rotation, party recruiting).
//   Lead             a journal line that points at a quest the player has not been offered yet: a rumour heard at the
//                    inn (JOBS lane), gossip overheard in the street or a street incident seen (LIFE lane), a tale's
//                    teller (STORY lane, optional). Leads go stale after LEAD_DAYS; taking the job they name marks them
//                    taken (JobOps::accepted).
//   ErrandKind       a street incident's quest (QType::Errand, LIFE lane): Quest::stage = the kind, hasPos / tgx / tgy
//                    where it happens; opened with Game::openErrand, ended with Game::finishErrand.
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "rpg/world/ids.h"

namespace jobs {

constexpr uint8_t JOBS_BLOCK_VER = 1;   // the save's jobs block (SAVE_VER 23): its own version byte first

// ---- the weekly pool
constexpr uint8_t PJ_OPEN = 0, PJ_GIVEN = 1, PJ_DONE = 2;
struct PoolJob {
  uint8_t qtype = 0;        // the QType it becomes (Hunt, Deliver, Supply, Protect ...)
  uint8_t need = 0;         // why (JOBS lane's own codes: wolves at the folds, ore wanted, a den near ...)
  uint8_t state = PJ_OPEN;  // PJ_*
  uint64_t giver = 0;       // the npcKey of the resident who asks (Game::npcKey / npcKeyOf)
};
struct Pool {
  ew::Gid site = 0;         // the settlement
  int32_t week = -1;        // Game::day / 7
  std::vector<PoolJob> jobs;
};

// ---- the journal's leads
enum class LeadFrom : uint8_t { Rumour, Gossip, Incident, Board, Teller, COUNT };
const char* leadFromName(LeadFrom f);   // "RUMOUR", "OVERHEARD", "SEEN", "NOTICE", "TALE"
constexpr uint8_t LS_OPEN = 0, LS_TAKEN = 1, LS_STALE = 2;
constexpr int LEAD_MAX = 24;    // the journal keeps the newest 24 (the oldest stale ones go first)
constexpr int LEAD_DAYS = 10;   // an open lead goes stale 10 days after it was heard
struct Lead {
  uint32_t id = 0;
  LeadFrom from = LeadFrom::Rumour;
  uint8_t qtype = 0;        // the QType it points to (QType::COUNT: a place or a person, no job yet)
  uint8_t state = LS_OPEN;  // LS_*
  int32_t day = 0;          // the day it was heard
  ew::Gid site = 0;         // the settlement or place it concerns (0 none)
  uint64_t who = 0;         // the npcKey of the person to see (0 none)
  bool hasPos = false;      // gx, gy: a GLOBAL tile to look (the map and the HUD may point there)
  int32_t gx = 0, gy = 0;
  std::string text;         // the journal line, upper case ("THE MILLER OF VOND NEEDS HANDS FOR THE HARVEST")
};

// ---- the street errands (QType::Errand; Quest::stage)
enum class ErrandKind : uint8_t {
  Thief,    // a thief runs with a purse: catch them (the victim pays)
  Cart,     // a broken cart in the road: bring the part or lend a hand
  Brawl,    // a brawl in the street: break it up
  Lost,     // a child or an animal strayed: bring them back
  Help,     // overheard: someone needs a hand with something small, here and now
  COUNT
};
const char* errandName(ErrandKind k);   // "THIEF", "CART", "BRAWL", "LOST", "HELP"

// ---- (JOBS lane) why a pool job exists: PoolJob::need. Each names the QType it becomes and the giver's angle on it
// (the offer's words, its target), so a smith's hunt is for hides and a merchant's bounty is on caravan raiders.
enum JobNeed : uint8_t {
  JN_NONE = 0,
  JN_DEN,         // Hunt: a den within a morning's walk (the target: that den)
  JN_GROUND,      // Hunt: beasts roam a hunting ground near (the target: that ground)
  JN_HIDES,       // Hunt: a smith or tanner wants hides (the target: a den)
  JN_CAMP,        // Bounty: a bandit camp raids the roads
  JN_RAIDERS,     // Bounty: a merchant's caravans robbed from a camp
  JN_CHIEF,       // NamedBandit
  JN_CAVE,        // Clear: something nests in a cave
  JN_RUIN,        // Clear: the dead stir in a ruin
  JN_DESECRATED,  // Clear: a priest's desecrated ruin
  JN_RELIC,       // Heirloom: a family keepsake
  JN_HOLYRELIC,   // Heirloom: a temple's relic
  JN_STOLEN,      // Heirloom: a merchant's stolen strongbox, hidden in a cave
  JN_LOST,        // Missing: someone went into a cave
  JN_STRAYED,     // Missing: a farmhand who strayed after the herd
  JN_PARCEL,      // Deliver: a parcel or letters
  JN_TOOLS,       // Deliver: a smith's tools ordered elsewhere
  JN_BREAD,       // Deliver: a baker's order
  JN_FIELDS,      // Protect: a real threat near the fields (a den or a camp within reach)
  JN_SUPPLY,      // Supply: the goods the giver's trade works (life_quests.cpp tradeSupply)
  JN_FAMINE,      // Supply: food in a famine (the realm's SS_FAMINE)
  JN_WARBAND,     // Bounty: deserters and raiders while the realm is at war
  JN_COUNT,
  // (P1b New Roads Content) a radiant job: PoolJob::qtype QType::Story, need JN_RADIANT0 + the kind (story::radiant::
  // kinds(): 0 ROAD WARDEN .. 14 BEACON CHAIN). "ANY WORK GOING?" starts its composed script (radiant::offerFor); the
  // pool job is given out when the job is begun and done when it ends (JobOps::radiantStep). The saved layout is
  // unchanged (need is a byte): JOBS_BLOCK_VER stays 1.
  JN_RADIANT0 = 32,
  JN_RADIANT_END = JN_RADIANT0 + 15
};
const char* jobNeedName(uint8_t n);     // "DEN", "HIDES", ... (tests, scripts)

constexpr int RES_SLOT0 = 3000;         // a census resident's actor slot (life_game.cpp LIFE_SLOT0): its quest key
constexpr int BOARD_SLOT0 = 960;        // the notice board's two posted jobs (places.cpp: slots 960, 961)

struct Book {
  std::vector<Pool> pools;
  std::vector<Lead> leads;
  uint32_t nextLead = 1;
  // ---- runtime only (never saved): the O(1) door for Game::hasOffer, rebuilt when a pool changes or the week turns
  mutable bool liveDirty = true;
  mutable int32_t liveWeek = -1;
  mutable std::unordered_map<uint64_t, uint32_t> live;    // giver key -> (pool index << 16) | job index, open jobs this week
  mutable std::unordered_map<uint64_t, uint64_t> canon;   // an actor's own npcKey -> its census resident's key (seen by hasOffer)
  uint8_t offerNeed = JN_NONE;   // makeOffer -> offerFor: the pool job's need while its quest is built
  void clear() { *this = Book(); }
  Pool* pool(ew::Gid site, int32_t week);                // nullptr: not worked out yet
  Lead* lead(uint32_t id);
  void serialize(std::vector<uint8_t>& out) const;       // JOBS_BLOCK_VER first; pools sorted by site (stable bytes)
  bool deserialize(const std::vector<uint8_t>& in);
};

}  // namespace jobs
