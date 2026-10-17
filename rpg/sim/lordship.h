// M12 "Rise and Fall": village lordship and kingdom-making (owner decision 2026-10-16: YES; VISION_PLAN 4.7
// "Kingdom-making: own a village (lordship) and pay tribute or declare independence", 14.2 Q4, 15.6.4 "After a long, long
// time the player's renown can become great enough to found a village and grow it into a kingdom, or to conquer an
// existing one", 15.33). LORD lane: it owns this header and rpg/sim/lord*.cpp, rpg/sim/realm_player.cpp. It may ADD to
// this header; what phase A declared is the contract (the VIEW lane reads it for the STANDING page, the fief's banner
// and its works; the SERVICE lane reads `holdings` to know a lord), never renamed, removed or changed in meaning.
//
// The realm keeps the political facts (rpg/sim/realm.h: Renown::lordOf / liege / realmOf, the KO_PLAYER realm, tribute
// events); this block keeps the hall: coffers, tax, loyalty, the steward, the works built, the tribute schedule.
// Saved: the lordship block (SAVE_VER 22, after the service block; its own version byte first).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "rpg/world/ids.h"

namespace lord {
using ew::Gid;

// how a holding came to the player (the realm's FiefGranted mag)
enum class How : uint8_t { None, Granted, Charter, Resettled, Conquered, Founded, COUNT };
// works a lord raises in a holding (bits of Holding::works); the VIEW lane draws them on the village (a palisade
// round it, the lord's banner on the hall, a watchtower, a granary, a market cross, a shrine, stone walls)
constexpr uint16_t LW_BANNER = 1, LW_PALISADE = 2, LW_HALL = 4, LW_GRANARY = 8, LW_MILITIA = 16, LW_MARKET = 32,
                   LW_WATCHTOWER = 64, LW_WALLS = 128, LW_SHRINE = 256;
struct WorkInfo { uint16_t bit; const char* name; int gold; int days; };   // the table (lordship.cpp)
const WorkInfo* workInfo(uint16_t bit);      // nullptr: not a work
int charterPrice(uint8_t siteType);          // gold to buy a village's charter from its ruler (a town / city: never)
constexpr int CHARTER_FAME = 300;            // fame a ruler asks before selling a charter
constexpr int GRANT_RANK = 3;                // service::Rank::Captain: a liege grants a lordship to its captains

struct Holding {
  Gid site = 0;               // the settlement's id
  How how = How::None;
  uint16_t sinceDay = 0;
  uint8_t taxPct = 10;        // 0..30: more coin, less loyalty
  uint8_t loyalty = 60;       // 0..100: the people's love (unrest under 25)
  uint16_t works = 0;         // LW_* built
  uint16_t building = 0;      // the LW_* bit being raised (0 none)
  uint16_t buildDone = 0;     // the day it is finished
  uint8_t militia = 0;        // the levy (LW_MILITIA): fighters who answer the lord's call
  int32_t lastTaxDay = -1;
  uint64_t steward = 0;       // the steward's census key (life::npcId) when one is named (0: the elder speaks)
  // ---- LORD lane (phase B; lordship block v2). Phase B names the steward with a key of its own (stewardKey: a census
  // resident walks its own life, so the steward is a new face of the village's culture, named from the key)
  uint8_t flags = 0;          // HF_*
  uint16_t raidDay = 0;       // the last raid, siege or burning the holding suffered (loyalty, the Raid task)
  uint32_t task = 0;          // the open fief task's id (Quest::stage of its QType::Fief quest; 0 none)
  uint16_t taskDay = 0;       // when the last task was given (one a fortnight at most)
};
// Holding::flags
constexpr uint8_t HF_UNREST = 1,    // loyalty under 25: half the taxes, a task to calm it
                  HF_FAMINE = 2,    // the village hungers (the realm's SS_FAMINE): loyalty bleeds without a granary
                  HF_FEAST = 4;     // a feast was held this fortnight (no second one)

struct Lordship {
  std::vector<Holding> holdings;   // the seat first
  int treasury = 0;                // gold in the hall's coffers (taken with the steward's TAKE)
  Gid liege = 0;                   // the kingdom the lordship answers to (mirror of Renown::liege)
  int tributeDue = 0;              // gold owed at the next tribute day (a quarter of taxSinceTribute)
  int taxSinceTribute = 0;         // taxes collected since the last tribute day
  uint16_t tributeDay = 0;         // the next day tribute is due (every 30 days)
  uint8_t missed = 0;              // tributes withheld in a row
  Gid realm = 0;                   // the player's own realm once independent (mirror of Renown::realmOf)
  std::string realmName;           // what the player named it
  uint16_t independenceDay = 0;
  uint32_t nextTask = 1;           // fief quests (QType::Fief: Quest::stage = the task id)
  // ---- LORD lane (phase B; lordship block v2)
  uint8_t withhold = 0;            // the steward was told to WITHHOLD the next tribute
  uint8_t crowned = 0;             // the player's realm is a kingdom (KING / QUEEN: 3+ settlements)
  int weekTax = 0;                 // taxes since the last weekly summary (the Tax rise line is weekly, not daily)
  uint16_t weekDay = 0;            // the day of the next weekly summary
  uint16_t wrathDay = 0;           // the liege's anger takes its next step on this day (0: calm)
  uint8_t wrath = 0;               // steps taken: 1 the ultimatum, 2.. the tension rungs, then the siege
  uint32_t siege = 0;              // the liege's siege of a holding being fought (Realm siege id; 0 none)
  uint32_t march = 0;              // the player's realm's own siege (a conquest) being fought (0 none)
  uint8_t peaceOffer = 0;          // the old liege's envoy offers peace (the steward's ACCEPT)
  uint16_t peaceDay = 0;           // when peace was made (0: never)

  bool lord() const { return !holdings.empty(); }
  Holding* holding(Gid site);
  const Holding* holding(Gid site) const;
  void serialize(std::vector<uint8_t>& out) const;
  bool deserialize(const std::vector<uint8_t>& in);
};

// ---- LORD lane (M12 phase B): the rules (lordship.cpp)
// fief tasks (QType::Fief; Quest::stage the task id, Quest::targetId the holding, the kind in Quest::flags bits 24..27)
enum class Task : uint8_t { None, Settlers, Camp, Raid, Unrest, COUNT };
constexpr int TASK_SHIFT = 24;
inline Task taskOf(uint32_t questFlags) { return (Task)((questFlags >> TASK_SHIFT) & 15); }
// tax levels the steward offers (Holding::taxPct)
constexpr uint8_t TAX_LOW = 5, TAX_FAIR = 10, TAX_HEAVY = 20;
const char* taxWord(uint8_t taxPct);                       // "LOW", "FAIR", "HEAVY"
// a charter's price by the village's state (the realm's prosperity 0..100 and popPct): 1500..3500 for a village
int charterPriceFor(uint8_t siteType, int prosperity, int popPct);
// gold a day a holding yields (before the tribute): the realm's popPct and prosperity, the tax, the market, unrest
int taxPerDay(const Holding& h, int popPct, int prosperity);
// the loyalty the people drift toward at this tax with these works (raids and famine pull it down in the step)
int loyaltyTarget(const Holding& h);
constexpr int CROWN_HOLDINGS = 3;      // settlements before the player's realm is a kingdom (LORD / LADY -> KING / QUEEN)
constexpr int MILITIA_WORK = 4;        // levy the MILITIA work raises; MUSTER adds 2 up to MILITIA_MAX
constexpr int MILITIA_MAX = 10;
constexpr int MUSTER_GOLD = 60;        // the coffers' cost of mustering two more
constexpr int FEAST_GOLD = 120;        // a feast for the village (loyalty +15, the Unrest task)
constexpr int TRIBUTE_DAYS = 30;
// the works a steward can raise, in the order the BUILD page lists them
constexpr uint16_t BUILD_ORDER[] = {LW_BANNER, LW_PALISADE, LW_WATCHTOWER, LW_GRANARY, LW_MILITIA, LW_MARKET, LW_SHRINE, LW_HALL, LW_WALLS};
// a work's prerequisite (WALLS need the PALISADE and the WATCHTOWER; 0 none)
uint16_t workNeeds(uint16_t bit);

}  // namespace lord
