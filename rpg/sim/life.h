// M5 "Hearth and Hall": needs-driven citizens (VISION_PLAN 10, 15.2, 15.12). Phase A (lead) contracts.
//
// The census (who lives where, their job, household, friends), the needs model (hunger, rest, social, faith, money:
// 0..100, 100 = satisfied), the hour plan (job schedule windows of VISION_PLAN 10.2 with a deterministic per-resident
// utility pick inside them, 15.12), the settlement's stock and mood (15.11 goods, 15.12 visible mood, coupled to the
// realm's SettlementState food / mood), level of detail, relationships, the player's light hunger and sleep buffs (15.2)
// and the save block. No SDL; the view only reads (moods, occupancy, lamps, the player's buffs).
//
// Level of detail (15.12; budget: 120 residents <= 1.5 ms on iPhone web):
//   - FULL: residents near the player are actors (Actor::resident >= 0). Game::lifeStep (life_game.cpp, TOWNSFOLK
//     lane) spawns them where their plan puts them this hour, walks them door to door, seats them, and so on.
//   - HOUR: every other resident of a loaded settlement is simulated in aggregate once per in-game hour by Life::tick
//     (needs decay and refill by activity, purchases consume stock).
//   - DAY: settlements out of the window are not simulated here: the realm's daily tick (rpg/sim/realm*.cpp) carries
//     their food and mood; Life::tick reports loaded settlements' consumption and mood to the realm
//     (realm::Realm::lifeReport) so a hungry town feeds the M4 famine -> trade collapse -> war chain.
//
// Ownership (M5 phase B): this header and life.cpp / census.cpp / life_quests.cpp / life_raids.cpp / realm_life.cpp
// belong to the CITIZENS lane, which may ADD to this header (never rename, remove or change the meaning of what phase A
// put here). life_game.cpp (the actors) belongs to the TOWNSFOLK lane.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "rpg/sim/world.h"
#include "rpg/world/economy.h"
#include "rpg/world/ids.h"

class Game;

namespace life {

// ---------------------------------------------------------------- jobs, activities, places
// VISION_PLAN 10.1 jobs (+ the inn's server, the seat's servant, the lamplighter, the herder and the plain labourer).
// Saved as u8 inside nothing (the census is regenerated); append anyway.
enum class Job : uint8_t {
  None, Farmer, Fisher, Miner, Woodcutter, Hunter, Herder, Smith, Merchant, Baker, Tailor, Stablehand, Innkeeper, Server,
  Bard, Scholar, Priest, Guard, Noble, Servant, Labourer, Lamplighter, Child, Elder, Beggar, COUNT
};
const char* jobName(Job j);   // "FARMER", "INNKEEPER"...
Role jobRole(Job j);          // the Role an actor of this job is spawned as (looks, dialogue, shops)

// What a resident is doing this hour (VISION_PLAN 10.2 + 15.12 Beg / Bathe / Emigrate / Brawl / LightLamps)
enum class Act : uint8_t {
  Sleep, Eat, Work, Wander, Socialise, Tavern, Pray, Patrol, Play, Perform, Shop, Home, Beg, Bathe, LightLamps, Brawl,
  Emigrate, COUNT
};
const char* actName(Act a);   // "SLEEP", "TAVERN"... (scripts, tests)
// Where it happens. Gathering = the society's usual evening place (cult::Society::gathering: the inn's common room, a
// mead hall, a tea house, a bathhouse, a grove or the plaza itself); Gathering2 a town's second one.
enum class Place : uint8_t { Home, Work, Gathering, Gathering2, Plaza, Temple, Field, Gate, Market, Street, Away, COUNT };
const char* placeName(Place p);

// One block of a job template (VISION_PLAN 10.2): hours [from, to) (to may pass 24: wraps), the activity, where, and
// the chance (percent) the block applies on a given day (else the resident's utility pick decides)
struct Block {
  uint8_t from = 0, to = 0;
  Act act = Act::Sleep;
  Place place = Place::Home;
  uint8_t prob = 100;
};
// the template of a job (n blocks covering 24 h); guards have a day and a night shift (shift 0 / 1)
const Block* jobTemplate(Job j, int shift, int& n);

// ---------------------------------------------------------------- needs
enum class Need : uint8_t { Hunger, Rest, Social, Faith, Money, COUNT };
constexpr int NEEDS = (int)Need::COUNT;
const char* needName(Need n);

// Resident::traits bits (decay rates and the utility weights read them)
constexpr uint8_t TR_SOCIABLE = 1, TR_DEVOUT = 2, TR_THRIFTY = 4, TR_LAZY = 8, TR_GLUTTON = 16, TR_BRAVE = 32,
                  TR_NIGHTOWL = 64, TR_GRUMPY = 128;
// Resident::flags (runtime and saved deltas)
constexpr uint16_t RF_DEAD = 1, RF_AWAY = 2, RF_BEFRIENDED = 4, RF_EMPLOYED = 8, RF_GRIEVING = 16, RF_FED = 32,
                   RF_HOMELESS = 64, RF_REFUGEE = 128, RF_KEY = 256;   // RF_KEY: a key person (a spawn with a quest identity)

// ---------------------------------------------------------------- the census (VISION_PLAN 10.1)
struct Resident {
  uint16_t idx = 0;            // index in its census (the NpcId's local part: life::npcId(site, idx))
  Job job = Job::None;
  uint8_t age = 30;            // years
  uint8_t traits = 0;          // TR_*
  bool female = false;
  uint8_t shift = 0;           // guards: 0 day, 1 night (jobTemplate's shift); others 0
  int8_t jitter = 0;           // schedule jitter in 5-minute steps (-6..6: +-30 min, VISION_PLAN 10.2)
  uint32_t lookSeed = 0;       // the look's seed (Game::makeLook draws from Rng(lookSeed))
  int16_t home = -1;           // the home building: offset from Site::bldgFirst (-1: none, sleeps at work or rough)
  int16_t work = -1;           // the workplace building offset (-1: works outdoors: fields, docks, the gate, a stall)
  uint16_t household = 0;      // household index (spouses, children, elders of one home share it)
  int16_t spouse = -1, parent = -1;   // resident indices (-1 none)
  // the generator spawn this resident IS, when it is one (its quest identity: Game::npcKeyOf(site, keyBldg, keySlot);
  // keyBldg -1 = an overworld spawn of the site, else a building offset whose interior spawn slot keySlot it is)
  int16_t keyBldg = -2, keySlot = -1;
  std::string name;
  // ---- simulation state (HOUR LOD; the full actor writes back into it when it is put away)
  std::array<uint8_t, NEEDS> need{{70, 70, 60, 60, 50}};
  uint16_t coin = 20;          // money in hand (the Money need follows it)
  Act act = Act::Sleep;        // the current plan (refreshed every in-game hour)
  Place place = Place::Home;
  int16_t at = -1;             // building offset it is in this hour (-1: outdoors at `place`)
  uint8_t mood = 60;           // 0..100 from its needs, grief and the town's safety
  uint16_t flags = 0;          // RF_*
  uint16_t griefDay = 0;       // RF_GRIEVING: the day the loss happened
  int actor = -1;              // runtime: the Actor::id embodying it (-1: aggregate only). Never saved.
  // ---- CITIZENS lane (M5 phase B) additions
  int16_t payer = -1;          // who pays for its meals (the household's earner; itself for earners; -1: charity)
  uint8_t meals = 0, slept = 0;        // today: hours eating / asleep (the aggregate's counters; the --life gates)
  uint8_t mealsY = 0, sleptY = 0;      // yesterday's
  uint8_t sinceMeal = 0;       // in-game hours since its last meal (barks: "HAVEN'T EATEN SINCE YESTERDAY"; capped 255)
  uint8_t misery = 0;          // days in a row its mood was below 25 (3: it packs up and leaves: RF_AWAY, emigrating)
  uint8_t origin = 0;          // ORIGIN_*: how it came to be in the census (born here, a refugee, kin of a keeper...)
};
constexpr uint8_t ORIGIN_NATIVE = 0, ORIGIN_REFUGEE = 1;
// Resident::flags bits that are saved deltas (the rest, RF_KEY / RF_HOMELESS / RF_REFUGEE, the census derives)
constexpr uint16_t RF_SAVED = RF_DEAD | RF_AWAY | RF_BEFRIENDED | RF_EMPLOYED | RF_GRIEVING | RF_FED;

// a friendship / bond (both ways). bond 0..100; kin ties are households, not Ties. b == PLAYER_TIE: a friend of the
// player's (Life::befriend; saved)
constexpr uint16_t PLAYER_TIE = 0xFFFF;
struct Tie {
  uint16_t a = 0, b = 0;
  uint8_t bond = 50;
};

// Settlement mood flags (Census::moodFlags): what the view shows (15.12) and the townsfolk act out
constexpr uint16_t MF_CONTENT = 1, MF_FESTIVAL = 2, MF_HUNGRY = 4, MF_FAMINE = 8, MF_WARTORN = 16, MF_GRIEF = 32,
                   MF_BRAWLS = 64, MF_EMIGRATING = 128, MF_SHUTTERED = 256, MF_RAIDED = 512;

struct Census {
  ew::Gid site = 0;
  int handle = -1;             // World::sites handle when built (runtime)
  uint32_t seed = 0;
  uint8_t tier = 0;            // 0 village, 1 town, 2 city, 3 capital
  std::vector<Resident> res;
  std::vector<Tie> ties;
  // the places people gather / pray / shop (building offsets, -1 none: the plaza serves)
  int16_t gathering = -1, gathering2 = -1, temple = -1, market = -1, seat = -1, inn = -1;
  // occupancy of each building (by offset) at hour `occHour` (absolute), from the hourly aggregate
  std::vector<uint8_t> occ;
  int32_t occHour = -1;
  // ---- aggregate economy (15.11 / 15.12): units of each good in the settlement's stores and stalls
  std::array<uint16_t, (size_t)ew::Good::COUNT> stock{};
  std::array<uint8_t, (size_t)ew::Good::COUNT> price{};   // percent of list (100 = normal; shortages raise it)
  uint8_t mood = 60;           // 0..100, the settlement's visible mood
  uint16_t moodFlags = MF_CONTENT;
  int32_t lastHour = -1;       // absolute in-game hour (day * 24 + hour) of the last aggregate update
  int32_t festivalDay = -1;    // the day of the next / current festival (-1 none planned)
  // helpers
  int bldgHandle(const Site& s, int off) const { return off >= 0 && off < s.bldgCount ? s.bldgFirst + off : -1; }
  // ---- CITIZENS lane (M5 phase B) additions
  uint8_t gatherKind = 0;      // cult::Gathering of its society (Bathhouse: the evening is a bath; Grove / Plaza: no building)
  uint8_t festivalKind = 0;    // FEST_*: the culture's kind of feast (festivalName / festivalTitle)
  int16_t bakery = -1, mill = -1, smithy = -1;   // the first of each (building offsets; -1 none)
  int32_t lastFestival = -1;   // the day of the last festival held
  int32_t famineUntil = -1;    // a famine felt in the streets until this day (forced, or the realm's SS_FAMINE): no imports
  int32_t raidDay = -1;        // the day of the last raid that broke in (MF_RAIDED for 3 days)
  int32_t raidNight = -1;      // the last night whose raid roll was decided (once a night)
  int16_t takenIdx = -1;       // a villager carried off by raiders (RF_AWAY) waiting for a rescue (-1 none)
  int32_t takenQuest = 0;      // the rescue quest's id once the player took it (0 none yet)
  int32_t emigrateDay = -1;    // the last day someone packed up and left (MF_EMIGRATING for 3 days)
  uint16_t refugees = 0;       // refugee residents appended after the census (RF_REFUGEE; rebuilt with it from the save)
  uint8_t realmFlags = 0;      // runtime: SS_* bits read back from the realm at the last daily update (low 8 bits)
  uint16_t baseTies = 0;       // ties[0, baseTies) came with the census; the rest formed in play (or are the player's)
  // what each good is used per day (moving average) and today's tallies (the daily report to the realm)
  std::array<uint16_t, (size_t)ew::Good::COUNT> use{};
  std::array<uint16_t, (size_t)ew::Good::COUNT> usedToday{};
  int32_t tallyDay = -1;       // the day the tallies below count
  int32_t foodMade = 0, foodEaten = 0, foodMissed = 0, foodImported = 0;
  // generator spawns -> residents (spawnResident; one to one): keyed and overworld spawns by (bldgOff + 1) << 32 | slot,
  // sorted; and each building's own people for its interior's other slots (inner[innerStart[b] .. innerStart[b + 1]))
  std::vector<int64_t> spawnKeys;
  std::vector<int16_t> spawnRes;
  std::vector<uint16_t> innerStart, inner;
  // runtime index of the ties per resident (rebuilt by tieIndex(); never saved)
  std::vector<std::vector<uint32_t>> tiesOf;
  int32_t lastDayRun = -1;     // the last day the daily update ran for it
  uint16_t contentDays = 0;    // days in a row it was content (emigrants come home; festivals)
  uint8_t hoursToday = 0;      // aggregate hours run on tallyDay (a whole day counts toward the --life gates)
  uint8_t harvestPct = 100;    // runtime: its fields' yield today (the realm's last harvest; a famine halves it)
};
// Quest::flags bits of the CITIZENS lane's quests (QType::Supply: the goods were collected / handed in)
constexpr uint32_t QF_SUP_GOODS = 1u << 20, QF_SUP_DONE = 1u << 21;

// FEST_*: the kind of a settlement's festival (by its culture's faith, drink and trades)
enum : uint8_t { FEST_HARVEST, FEST_GOD, FEST_LANTERNS, FEST_ANCESTORS, FEST_SPRING, FEST_SEA, FEST_FIRES, FEST_MEAD, FEST_COUNT };
const char* festivalName(uint8_t kind);   // "HARVEST FAIR", "LANTERN NIGHT"...
const char* moodFlagName(uint16_t bit);   // "CONTENT", "FESTIVAL", "HUNGRY"... (one MF_* bit)

// stable identity of a resident (a 64-bit key mixed from the site id and the index; never 0): equal for the same site
// and index in every session (rumours, marks, the story engine's cast may key on it)
ew::Gid npcId(ew::Gid site, int idx);

// What a resident should be doing at an hour (VISION_PLAN 10.2 + 15.12): the activity, where, and the building
// (offset; -1 outdoors). Deterministic per resident, day and hour.
struct Plan {
  Act act = Act::Sleep;
  Place place = Place::Home;
  int16_t bldg = -1;
};

// ---------------------------------------------------------------- the player (15.2: buffs, never chores)
// Well Fed (a meal: faster stamina and health regen for a while), Rested (a night in a bed: a small XP bonus for a
// while), Hungry (a long time without food: slower stamina regen), Weary (days without sleep: a mild penalty). Never
// lethal in the default mode; `survival` (an optional setting, stubbed) would add real hunger, thirst and cold.
constexpr uint8_t BUFF_WELLFED = 1, BUFF_RESTED = 2, BUFF_HUNGRY = 4, BUFF_WEARY = 8;
struct PlayerNeeds {
  float fedH = 0;              // in-game hours of Well Fed left
  float restedH = 0;           // in-game hours of Rested left
  float sinceMealH = 0;        // in-game hours since the last meal (Hungry from 30)
  float sinceSleepH = 0;       // in-game hours since the last sleep in a bed or bedroll (Weary from 48)
  uint8_t mealQuality = 0;     // 0 a snack .. 3 a cooked inn meal (Well Fed's strength and length)
  bool survival = false;       // the optional Survival setting (stub: no effect yet)
  uint8_t buffs() const {
    uint8_t b = 0;
    if (fedH > 0) b |= BUFF_WELLFED;
    if (restedH > 0) b |= BUFF_RESTED;
    if (fedH <= 0 && sinceMealH >= 30.0f) b |= BUFF_HUNGRY;
    if (restedH <= 0 && sinceSleepH >= 48.0f) b |= BUFF_WEARY;
    return b;
  }
};
const char* buffName(uint8_t buffBit);   // "WELL FED", "RESTED", "HUNGRY", "WEARY"

// ---------------------------------------------------------------- lamps (VISION_PLAN 10.3 lamplighters)
// The hour a street lamp at a GLOBAL tile is lit at dusk (19:00 .. 20:30, by tile: the lamplighter's round) and put
// out at dawn (5:30 .. 6:30). Pure: the view lights a lamppost only while lampLit says so; the TOWNSFOLK lane's
// lamplighter walks the round to arrive at each lamp at its hour.
inline float lampLightHour(int32_t gx, int32_t gy) {
  uint32_t h = (uint32_t)gx * 2654435761u ^ ((uint32_t)gy * 40503u + 0x1A3Bu);
  h = (h ^ (h >> 15)) * 2246822519u;
  h ^= h >> 13;
  return 19.0f + (float)(h % 7u) * 0.25f;
}
inline float lampOutHour(int32_t gx, int32_t gy) { return 5.5f + (float)((uint32_t)(gx * 7 + gy * 13) % 5u) * 0.25f; }
inline bool lampLit(int32_t gx, int32_t gy, float hour) { return hour >= lampLightHour(gx, gy) || hour < lampOutHour(gx, gy); }

// ---------------------------------------------------------------- the simulation
class Life {
 public:
  // the census of a settlement (built on first use, deterministic from the site and its buildings; nullptr for a
  // non-settlement or one whose buildings have not streamed in yet). Saved deltas (deaths, friends, needs) re-apply.
  Census* census(const World& w, int site);
  const Census* find(ew::Gid site) const;
  Census* findMut(ew::Gid site);
  // the plan of one resident at day / hour (jobTemplate + needs utility inside the job's window; 15.12)
  Plan plan(const World& w, const Census& c, const Resident& r, int day, float hour) const;
  // once per Game::update step (called from the core loop): hourly aggregate of the loaded settlements, daily
  // reports to the realm, the player's buffs ticking (in-game hours from Game::hour / Game::day)
  void tick(Game& g, float dt);
  // a resident died (by any hand): kin and friends grieve (RF_GRIEVING), the census marks it (RF_DEAD)
  void residentDied(ew::Gid site, int idx, int day);
  // the player's hooks (15.12): feed (food changes hands: Hunger refills), employ (the resident works for the player
  // a day: Money), supply (goods into the settlement's stock), befriend (a tie with the player). Return false when
  // it makes no sense (dead, away, nothing to give).
  bool feed(ew::Gid site, int idx, int quality);
  bool employ(ew::Gid site, int idx, int day);
  bool supply(ew::Gid site, ew::Good g, int units);
  bool befriend(ew::Gid site, int idx);
  // friends of a resident (indices), strongest first; `name` lookup for greetings ("EVENING, HALLA")
  std::vector<int> friendsOf(const Census& c, int idx) const;

  // ---- what the view reads (cheap; defaults when nothing is known)
  uint8_t mood(ew::Gid site) const;            // 0..100 (60 when unknown)
  uint16_t moodFlags(ew::Gid site) const;      // MF_* (MF_CONTENT when unknown)
  bool festival(ew::Gid site, int day) const;  // a festival today
  // people inside building `bldgOff` (offset from Site::bldgFirst) of site `site` this in-game hour, from the hourly
  // aggregate (window light by occupancy: 0 = dark windows). -1: not known (no census yet, or it has not run this
  // hour): the view keeps its own rule.
  int occupants(const World& w, int site, int bldgOff) const;

  PlayerNeeds player;
  // the clock the buffs and aggregates count from jumps to (day, hour) without charging the hours in between (a night
  // slept: Game::lifeSlept charges them itself; a load, a new game)
  void resync(int day, float hour);

  // ---- persistence (Game's life block: a length-prefixed blob, its own version byte first)
  void serialize(std::vector<uint8_t>& out) const;
  bool deserialize(const std::vector<uint8_t>& in);
  void clear();

  struct Stats {
    int censuses = 0, residents = 0;      // built so far / residents in them
    int hourTicks = 0;                    // aggregate hours run
    double lastTickMs = 0, worstTickMs = 0, tickMsSum = 0;   // Life::tick wall time (the 1.5 ms budget)
    double worstCensusMs = 0;             // the slowest census build
    // ---- CITIZENS lane (M5 phase B) additions
    int ticks = 0;                        // Life::tick calls (average = tickMsSum / ticks)
    double worstSliceMs = 0;              // the slowest census build slice inside one tick
    int dayResidents = 0, dayMealsOk = 0, daySleepOk = 0;   // resident-days counted / with >= 2 meals / >= 6 h asleep
    int64_t needSum[NEEDS] = {};          // needs summed over residents and aggregate hours (averages for the gates)
    int64_t needSamples = 0;
    int raidsLive = 0, raidsAbstract = 0, raidsFailed = 0, raidRolls = 0;
    int emigrated = 0, festivals = 0, reports = 0, trades = 0;
  };
  Stats stats;

  // ---- CITIZENS lane (M5 phase B) additions
  // a raid going on in front of the player (life_raids.cpp; never saved: the party is gone after a load)
  struct RaidLive {
    ew::Gid site = 0;
    int si = -1;
    int night = -1;
    std::vector<int> ids;      // the raiders' Actor::id
    float t = 0, insideT = 0;  // seconds since it began / with 3+ raiders inside the footprint
    int outcome = 0;           // 0 going on, 1 repelled, 2 the defence failed
  };
  RaidLive raid;
  int lastRaidOutcome = 0;     // the last raid's outcome near the player (scripts: expect raid ...)
  ew::Gid forceRaidSite = 0;   // scripts: raid this settlement now (0 none)
  // percent chance of a raid tonight for a settlement (VISION_PLAN 10.4 M5: 2..8 % by monster pressure; 0 when nothing
  // prowls within 60 tiles). pressure: uncleared dens and lairs within 60 tiles; military 0..1 (the kingdom's strength
  // for its size; 0 for the wildlands); doy 0..359 (the season)
  static int raidChancePct(int pressure, float military, int doy);
  // deterministic raid roll of a settlement for a night
  static bool raidRoll(ew::Gid site, int night, int pct);
  // forcing (scripts, tests): a famine felt in the streets for `days` (no imports, half the harvest, food stock gone),
  // a festival today, the clock run `hours` in-game hours (each hour aggregated once)
  bool forceFamine(ew::Gid site, int day, int days);
  bool forceFestival(ew::Gid site, int day);
  void advanceHours(Game& g, int hours);
  // the settlement's festival as the people call it ("THE FEAST OF ODRA", "HARVEST FAIR")
  std::string festivalTitle(const World& w, const Census& c) const;
  // the average of one need over its living residents
  static int needAverage(const Census& c, Need n);
  // a census being built in slices (census.cpp; copyable so Game stays copyable)
  struct Build {
    int site = -1;
    int phase = 0;
    size_t pos = 0;
    Census c;
    struct Home { int off = 0, cap = 0, used = 0, cx = 0, cy = 0; };
    std::vector<Home> homes;
    std::vector<int> works;
    uint16_t household = 0;
    std::vector<int16_t> hx, hy;       // each resident's home centre (ties)
    std::vector<uint8_t> anchored;     // spawn map: residents already given a spawn
    std::vector<uint16_t> order;       // residents in the spawn map's hashed order
    std::vector<int> owSpawns;         // the site's overworld spawn indices, by slot
    std::vector<size_t> cursor;        // per candidate class, the next place in `order`
    double ms = 0;                     // time spent so far
  };

 private:
  std::unordered_map<ew::Gid, Census> sites_;
  // what the save keeps of a settlement (life.cpp writes one per census built, and carries the loaded ones not built
  // again this session as they were): the residents' state in index order, the player's ties, stock and mood
  struct ResState {
    uint16_t flags = 0, griefDay = 0;
    std::array<uint8_t, NEEDS> need{};
    uint16_t coin = 0;
  };
  struct Record {
    ew::Gid site = 0;
    uint8_t mood = 60;
    uint16_t moodFlags = MF_CONTENT;
    int32_t lastHour = -1, festivalDay = -1;
    std::array<uint16_t, (size_t)ew::Good::COUNT> stock{};
    std::vector<ResState> res;
    std::vector<Tie> playerTies;
    // ---- CITIZENS lane (M5 phase B) additions
    bool hasNeeds = false;     // res[] holds every resident's needs and coin (else only the sparse deltas below)
    std::vector<std::pair<uint16_t, ResState>> sparse;   // residents with saved flags / grief / misery (index, state)
    std::vector<uint8_t> misery;                          // (parallel to sparse)
    std::vector<Tie> ties;     // ties formed in play (not the census's own, not the player's)
    int32_t lastFestival = -1, famineUntil = -1, raidDay = -1, raidNight = -1, takenQuest = 0, emigrateDay = -1;
    int16_t takenIdx = -1;
    uint16_t refugees = 0;
    uint16_t contentDays = 0;
  };
  std::unordered_map<ew::Gid, Record> pending_;   // loaded records whose census is not built yet
  double lastAbsH_ = -1;       // the in-game time (day * 24 + hour) the buffs last advanced to (-1: not yet)
  int lastDay_ = -1;           // the day the daily reports last ran
  static Record recordOf(const Census& c);
  static void applyRecord(Census& c, const Record& r);
  void hourTick(Game& g, Census& c, int32_t hourAbs);
  // ---- CITIZENS lane (M5 phase B) additions
  Build build_;                // the census being built in slices by tick (site -1: none)
  ew::Gid buildGid_ = 0;
  std::vector<uint16_t> scratch_;   // the aggregate's reusable lists (no allocation churn per step)
  std::vector<int> queue_;          // settlements (handles) waiting for this hour's aggregate
  size_t queuePos_ = 0;
  int32_t queueHour_ = -1;
  Census* finishBuild(const World& w);
  Census* install(const World& w, Build& b, ew::Gid id);
  void dayTick(Game& g, Census& c, int day);
  void tradeDay(Game& g, int day);
  void addRefugees(const World& w, Census& c, int n);
  void evictFar(Game& g);
};
// build a census in slices: true when done (b.c is complete). b.site and b.phase 0 start it; budgetMs <= 0: all at once
bool buildCensusStep(const World& w, Life::Build& b, double budgetMs);
// the runtime tie index (Census::tiesOf) from Census::ties
void tieIndex(Census& c);
// a tie between two residents (nullptr none)
Tie* findTie(Census& c, int a, int b);

// build a settlement's census (census.cpp; deterministic from the site record, its buildings and its culture)
void buildCensus(const World& w, int site, Census& out);
// the resident a generator spawn of this settlement embodies (-1: none: the watch's guards, a runtime post). bldgOff:
// the building offset (from Site::bldgFirst) whose interior the spawn belongs to, -1 for the site's overworld spawns.
// An interior's slot 0 is the building's keyed resident (Resident::keyBldg / keySlot). (CITIZENS, phase B) One to one:
// no two spawns of a site, overworld or interior, are the same resident. The census maps the site's overworld folk
// spawns by role (stall keepers to traders, children to children...) and each building's other interior slots to its
// own household by rank (ground slots 1..15, then 15 a floor), from the site's deterministic data only (its buildings
// and spawn list, never load order). A spawn left over when the people run out, and the watch's guards, are -1. The
// mapping is an identity, whatever the resident is doing: callers check Resident::flags (RF_DEAD, RF_AWAY) and plan().
int spawnResident(const Census& c, const Spawn& sp, int bldgOff);

}  // namespace life
