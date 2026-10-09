// M4 "Banners": the living world simulation v1 (VISION_PLAN 4, 13 M4, and the owner notes that bind it: 15.3 ruins from
// simulated history, 15.6 slow realistic diplomacy (famine -> failed food trade -> border tension -> skirmishes -> war,
// rare wars with reasons, foreshadowed) and the renown / land hooks for founding or conquering a kingdom later, 15.8
// kingdom identity, 15.12 settlement food and mood in aggregate, 15.14 governments and seats from the society).
//
// Ownership (M4 phase B): the REALM lane owns this header and rpg/sim/realm*.cpp. It may ADD to this header freely; what
// phase A declared here is the contract the other lanes build on (WARDS reads settlement states, sieges and wars and
// feeds siege contributions back; STORY reads events, kingdoms, rulers and ruin records for rumours, quests and lore;
// VIEW draws owners, borders, wars and news): never rename or remove it, never change its meaning.
//
// Determinism: the realm is a pure function of (world seed, the days ticked, the player's contributions). Genesis and
// the history pre-roll are pure functions of the seed and the generator's plans (any order of instantiation gives the
// same kingdoms). The daily tick draws from the realm's OWN saved RNG stream and processes kingdoms in Gid order. It
// never touches the world generator's output (identity never changes, only overlays: VISION_PLAN 4.5), so goldens are
// unaffected. Floating point is fine here (the sim is saved, not regenerated), but keep it deterministic on one machine.
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "rpg/world/ids.h"

namespace ew { class EndlessSource; }

namespace realm {
using ew::Gid;

// ---- diplomacy (VISION_PLAN 4.1, owner 15.6.3: slow and realistic)
enum class Rel : uint8_t { Unknown, Contact, Peace, Trade, Alliance, Rivalry, War, Vassal, Overlord, COUNT };
// The road to war climbs one step at a time, each with a cause the player can hear about (rumours, prices, refugees,
// troop movements) long before a war is declared. War itself is Rel::War.
enum class Tension : uint8_t { Calm, Strained, TradeBroken, BorderTension, Skirmishes, COUNT };
enum class WarGoal : uint8_t { Conquer, Raid, Tribute, Subjugate, COUNT };
// why a war began (15.6.3: wars have reasons): the chronicle and the rumours name it
enum class WarCause : uint8_t { Famine, BrokenTrade, BorderDispute, Succession, Revenge, Conquest, Raiders, COUNT };

// kingdom values from the culture (VISION_PLAN 4.1 values[8])
enum Value : uint8_t { V_MARTIAL, V_MERCANTILE, V_PIOUS, V_SCHOLARLY, V_SEAFARING, V_EXPANSIONIST, V_ISOLATIONIST, V_HONOUR, V_COUNT };
// ruler traits (bits)
constexpr uint8_t RT_AGGRESSIVE = 1, RT_CAUTIOUS = 2, RT_GREEDY = 4, RT_PIOUS = 8, RT_SCHOLARLY = 16, RT_HONOURABLE = 32;

// level of detail (VISION_PLAN 4.4)
enum class Tier : uint8_t { Latent, Dormant, Active, Present };

struct Ruler {
  std::string name;        // "HROLF STONEHAND"
  std::string title;       // from the society (cult::Society titles): "KING", "KHAN", "HIGH PRIESTESS", "DOGE"...
  uint8_t age = 40;
  uint8_t traits = 0;      // RT_* bits
  bool female = false;
  uint16_t sinceDay = 0;   // the day they took the seat (0: before the game began)
  uint64_t dynasty = 0;    // a dynasty key (a civil war's rebels found a new one)
  // REALM lane (M4 phase B) additions
  std::string house;       // the dynasty's name ("HOUSE VALDR"): kept by an heir, a new one on an elective win or a revolt
};

struct KingdomState {
  Gid id = 0;              // the generator's kingdom id (makeId(kx, ky, IdKind::Kingdom, 0)); rebels: 0x800 + n locals
  Gid capital = 0;         // its capital's site id (may move when the capital falls)
  uint64_t culture = 0;    // cult::CultureId (its dialect)
  std::string name;        // "ASHMARK" (rebels and warlords: a new name)
  uint32_t color = 0, color2 = 0;   // banner field and charge (rebels get new arms)
  uint8_t emblem = 0;
  uint8_t gov = 0;         // cult::Government (rpg/culture/society.h) of its society
  Ruler ruler;
  float pop = 0;           // thousands (village 0.1-0.3, town 0.6-1.5, city 3-8)
  float wealth = 0;        // treasury, gold / 100
  float military = 0;      // strength points (1 = a company of about 25)
  float stability = 0.7f;  // 0..1
  float exhaustion = 0;    // war weariness 0..1
  float food = 0.6f;       // granaries 0..1 (15.12: the harvest, the food trade and the residents' hunger in aggregate)
  uint8_t values[V_COUNT] = {};
  bool fallen = false;     // no settlements left
  bool rebel = false;      // born of a civil war or a warlord (VISION_PLAN 4.3 "Rise")
  Tier tier = Tier::Latent;
  int32_t foundedDay = 0;  // negative: founded before the game began (years * -360) (phase B: int32, 200 years overflowed int16)
  int16_t playerRep = 0;   // -100..100 (VISION_PLAN 4.7)
  std::vector<Gid> settlements;     // owned now (site ids), the capital first
  // REALM lane (M4 phase B) additions
  Gid parent = 0;          // rebels: the realm they broke away from
  uint16_t lastTick = 0;   // the last day it was ticked (a dormant realm catches up from here on re-entry: VISION_PLAN 4.4)
  bool famine = false;     // its granaries are empty (15.6.3: the start of the road to war)
  uint16_t famineDay = 0;  // when the famine began
  float yieldRatio = 1;    // its settlements' harvest against their need (15.11 specialisations; < 1 must buy food)
  uint16_t harvestDoy = 200;        // the day of the year (0..359) its harvest comes in
  int16_t harvestYear = -1;         // the last year whose harvest it brought in
  float lastHarvest = 1;   // that harvest's quality (0.3 a failed harvest .. 1.1 a bumper year)
  uint16_t capitalLostDay = 0;      // when its capital last fell (0 never): stability suffers for 180 days
  uint8_t inherit = 0;     // cult::Inheritance of its society (who follows a ruler who dies)
};

struct Relation {
  Gid a = 0, b = 0;        // a < b
  Rel state = Rel::Unknown;
  Tension tension = Tension::Calm;
  int8_t opinion = 0;      // -100..100
  uint16_t sinceDay = 0;
  uint8_t border = 0;      // shared border settlements
  bool foodDeal = false;   // one feeds the other (a famine on the seller's side breaks it: 15.6.3)
  // REALM lane (M4 phase B) additions: the road to war (15.6.3)
  Gid seller = 0;          // foodDeal: the side that sells the grain
  int8_t base = 0;         // the opinion it drifts toward (culture distance, faith, trade, border friction)
  Gid aggrieved = 0;       // the side climbing the ladder (the hungry one, the one whose deal was broken)
  WarCause cause = WarCause::BorderDispute;   // why the ladder is being climbed (a war that comes of it carries it)
  uint16_t stepDay = 0;    // the day the tension last changed
  uint16_t lastIncident = 0;        // the last foreshadowing event between them (border incident, skirmish, troops)
  uint16_t marchDay = 0;   // != 0: troops are marching, war is declared on this day unless tempers cool
  uint16_t truceUntil = 0; // after a war: no new war before this day
};

struct War {
  uint32_t id = 0;
  Gid attacker = 0, defender = 0;
  uint16_t startDay = 0, endDay = 0;   // endDay 0: still being fought
  float score = 0;         // -1..1, + the attacker winning
  WarGoal goal = WarGoal::Conquer;
  WarCause cause = WarCause::BorderDispute;
  std::string name;        // "THE WAR OF THE SALT CROWN" (the chronicle, rumours, the news)
  // REALM lane (M4 phase B) additions
  std::vector<Gid> taken, takenFrom;  // settlements captured in this war and who held them before (the treaty decides)
  uint16_t pausedDays = 0; // days one side lay dormant (VISION_PLAN 4.4: the war pauses there)
  bool forced = false;     // made by forceWar (scripts and tests), not by the road to war
};

struct Siege {
  uint32_t id = 0, war = 0;
  Gid site = 0;            // the besieged settlement
  Gid attacker = 0, defender = 0;   // kingdoms
  float atk = 0, def = 0;  // strength in the field (the player's contribution adds to one: contribute())
  uint16_t startDay = 0, resolveDay = 0;
  bool over = false;
  bool attackerWon = false;
  bool playerJoined = false;
  uint8_t playerSide = 0;  // 0 none, 1 with the attackers, 2 with the defenders
  int32_t campX = 0, campY = 0;   // where the attackers camp (global tile, 14-30 tiles outside the walls; WARDS draws it)
  // REALM lane (M4 phase B) additions
  bool forced = false;     // made by forceSiege: resolves atk > def without a roll (the lanes' tests rely on it)
  bool raid = false;       // a raid: the place burns instead of changing hands
};

// SettlementState::flags (VISION_PLAN 4.1, 4.5)
constexpr uint16_t SS_BESIEGED = 1, SS_BURNED = 2, SS_ABANDONED = 4, SS_OCCUPIED = 8, SS_REFUGEES = 16, SS_FAMINE = 32,
                   SS_UNREST = 64, SS_RUINED = 128, SS_FRONTIER = 256, SS_REBUILDING = 512, SS_GARRISON = 1024;
// SS_FRONTIER: no kingdom holds it (the wildlands): militia only, no kingdom guards (owner 2026-10-03)
// SS_GARRISON: a conqueror of another culture keeps a garrison tower here (VISION_PLAN 4.5)
// SS_REFUGEES: a refugee camp stands outside it (refugeesFrom says whose)

// One settlement's state as the realm sees it. Identity never changes; the overlay does (WARDS lane draws it).
struct SettlementState {
  Gid site = 0;
  Gid owner = 0;           // the kingdom that holds it now (0: nobody)
  Gid home = 0;            // its genesis kingdom (SitePlan::kingdom)
  uint8_t type = 0;        // SiteType (City, Town, Village)
  int32_t gx = 0, gy = 0;  // heart (global tile)
  uint8_t popPct = 100;    // of its generated population
  uint8_t prosperity = 50; // 0..100
  uint8_t damage = 0;      // 0..100: damage/100 * 0.7 of its buildings draw charred (VISION_PLAN 4.5)
  uint8_t garrison = 0;    // guard strength posted (guards on the streets scale with it and the kingdom's military)
  uint8_t food = 60;       // 0..100 stock (15.12)
  uint8_t mood = 60;       // 0..100 (15.12: content towns full taverns; hungry or war-torn ones beggars, shut stalls)
  uint16_t flags = 0;      // SS_*
  uint16_t changedDay = 0; // the day its owner or flags last changed
  Gid refugeesFrom = 0;    // SS_REFUGEES: the settlement whose people shelter here
  Gid garrisonOf = 0;      // SS_GARRISON: the kingdom whose tower stands here
  // REALM lane (M4 phase B) additions
  uint8_t special = 0;     // ew::Specialty (15.11): estimated from the lattice, the true one once the site loads
  uint8_t yield = 100;     // its food against its need, percent (a farming village ~135, a city ~90)
  uint16_t refugeesDay = 0;         // SS_REFUGEES: when the camp went up (it empties after 45 days)
  uint16_t markDay = 0;             // SS_ABANDONED: when it was abandoned (a ruin after 30 days, resettled after 60)
  uint16_t takenDay = 0;            // SS_OCCUPIED: when it was taken (occupied for 30 days)
  uint16_t burnDay = 0;             // SS_BURNED: when it burned (rebuilding starts 30 days later)
};

// Events (VISION_PLAN 4.6): each makes a rumour whose reach grows 30 tiles a day from (gx, gy) and fades after 30 days
// (60 for a fall or a first contact). Saved as u8: append.
enum class EvType : uint8_t {
  FirstContact, TradeDeal, TradeBroken, HarvestFailed, Famine, BorderIncident, Skirmish, WarDeclared, SiegeBegun,
  SiegeBroken, TownTaken, TownBurned, Refugees, Peace, RulerDied, Succession, CivilWar, KingdomFell, KingdomRose,
  Resettled, Ruined, Festival, TroopsMarching, PricesRising,
  // M6 Steel (FOES lane): a world boss raided a settlement (a = its land's kingdom, site, mag = the boss's art::Monster);
  // a world boss or a named unique was slain (by the player: mag = its art::Monster, gx / gy where it fell)
  BeastRaid, BeastSlain,
  COUNT
};
struct WorldEvent {
  uint32_t id = 0;         // serial, never reused
  uint16_t day = 0;
  EvType type = EvType::FirstContact;
  Gid a = 0, b = 0;        // kingdoms (b: the other side; 0 none)
  Gid site = 0;            // the settlement it happened at (0 none)
  int16_t mag = 0;         // type-specific size (a famine's depth, a war's id low bits...)
  int32_t gx = 0, gy = 0;  // where (global tile)
  bool heard = false;      // the player has heard of it (the News journal lists heard events: VISION_PLAN 4.6)
};

// 15.3: the true record behind a ruin or a site that fell in the pre-play history. A pure function of the seed and the
// generator's plans (computed on demand, memoised, never saved).
enum class FallCause : uint8_t { War, Plague, Flood, Famine, Collapse, Dragon, Curse, COUNT };
struct RuinRecord {
  Gid site = 0;
  bool valid = false;
  uint64_t culture = 0;            // whose it was (inscriptions in that culture's script; its draugr)
  std::string oldName;             // what it was called when it lived: "HRAFNSTAD"
  std::string builtBy;             // the realm that built it (extinct or living): "THE KINGDOM OF OSKVAR"
  bool extinct = true;             // that realm is gone (else builtByKingdom is a living kingdom)
  Gid builtByKingdom = 0;
  std::string lastLord;            // its last lord or ruler: "QUEEN ASGERD THE PALE"
  int foundedYearsAgo = 0, fellYearsAgo = 0;
  FallCause cause = FallCause::War;
  Gid destroyer = 0;               // a living kingdom that destroyed it (0: none, or extinct)
  std::string destroyerName;
  std::vector<std::string> clues;  // short lines for inscriptions, journals, graves, murals (STORY places them)
  // REALM lane (M4 phase B) additions
  std::string builtByCulture;      // the culture's name ("VETHMARKI"): whose script the inscriptions are in
  bool simulated = false;          // it fell during play (the live sim), not in the pre-play history
};

// REALM lane (M4 phase B): a settlement the live sim ruined (burned or starved, then abandoned 30 days): its ruin record
// is made from this (Realm::ruin). Saved.
struct SimRuin {
  Gid site = 0;
  uint16_t day = 0;        // the day it became a ruin
  FallCause cause = FallCause::War;
  Gid destroyer = 0;       // the kingdom that burned it (0: hunger)
  Gid owner = 0;           // who held it when it fell
  Gid home = 0;            // the realm that built it
};

// REALM lane (M4 phase B): one line of a kingdom's pre-play history (VISION_PLAN 4.2), oldest first in chronicle()
struct HistoryEntry {
  int yearsAgo = 0;
  std::string text;
};

// 15.6.4: renown and land, the hooks for founding or conquering a kingdom late in the game (M12); M4 records them.
struct Renown {
  int fame = 0;            // global (dialogue, offers): grows with deeds told of in the news
  Gid lordOf = 0;          // a settlement the player holds (M7 / M12), 0 none
  uint8_t rank = 0;        // enlisted rank (VISION_PLAN 4.7 M12): 0 none, 1 recruit, 2 sergeant, 3 captain, 4 marshal
  Gid sworn = 0;           // the kingdom the player serves (0 none)
};

class Realm {
 public:
  // a new world: forget everything (Game::resetSession calls it with the world seed)
  void reset(uint64_t worldSeed);
  uint64_t seed() const { return seed_; }

  // ---- the clock (Game::realmStep drives these; rpg/sim/realm_game.cpp)
  // the player stands at global tile (gx, gy) on `day`: kingdoms within 3 kingdom cells are instantiated (genesis and
  // the history pre-roll; at most 48 active, nearest first), farther ones go dormant (VISION_PLAN 4.4). Cheap when the
  // player has not moved a kingdom cell.
  void focus(ew::EndlessSource& src, int32_t gx, int32_t gy, int day);
  // run the daily ticks up to `day` (at most 60 daily ticks per call, weekly ticks beyond: VISION_PLAN 4.8)
  void advanceTo(ew::EndlessSource& src, int day);
  int day() const { return day_; }

  // ---- what the world reads
  const KingdomState* kingdom(Gid id) const;
  const std::vector<KingdomState>& kingdoms() const { return kingdoms_; }
  // nullptr: the sim has never touched it (its genesis owner holds it in peace)
  const SettlementState* settlement(Gid site) const;
  // who holds this site now (genesis: its plan's kingdom, passed in, when the realm has no state for it)
  Gid ownerOf(Gid site, Gid genesis) const;
  // whose land a global tile is NOW (the map's borders, the border-crossing banner): the generator's kingdom there,
  // unless the realm moved that land's settlements to another owner, in which case the owner of the nearest settlement
  // of that genesis kingdom (a Voronoi of owned settlements inside the old realm). `genesis` = src.kingdomAt(gx, gy).
  Gid landOwner(Gid genesis, int32_t gx, int32_t gy) const;
  const Relation* relation(Gid a, Gid b) const;
  const std::vector<Relation>& relations() const { return relations_; }
  const std::vector<War>& wars() const { return wars_; }
  const std::vector<Siege>& sieges() const { return sieges_; }
  const Siege* siegeAt(Gid site) const;                 // the siege of this site still being fought (nullptr none)
  const War* warBetween(Gid a, Gid b) const;            // an ongoing war between two kingdoms (either side)
  bool atWar(Gid a, Gid b) const { return warBetween(a, b) != nullptr; }
  const std::vector<WorldEvent>& events() const { return events_; }   // bounded log, oldest first
  uint32_t eventSerial() const { return nextEvent_; }   // grows with every new event (a cheap "anything new?" test)
  // 15.3: the record behind a ruin (or any fallen settlement) by its site id; valid == false for a site with none
  RuinRecord ruin(ew::EndlessSource& src, Gid site);
  // the history pre-roll's text for a kingdom (VISION_PLAN 4.2: "the War of the Salt Crown, 40 winters ago")
  std::vector<std::string> chronicle(Gid kingdom) const;

  // ---- the player's influence (VISION_PLAN 4.7, 15.6.4)
  int rep(Gid kingdom) const;
  void addRep(Gid kingdom, int delta);
  Renown& renown() { return renown_; }
  const Renown& renown() const { return renown_; }
  // the player's part in a siege: side 1 attackers, 2 defenders; amount in strength points (a kill ~0.5)
  void contribute(uint32_t siege, int side, float amount);
  void markHeard(uint32_t eventId);

  // ---- forcing (scripts' `realm ...` commands, rpg_test, the WARDS / STORY / VIEW lanes' tests before the sim makes
  //      these by itself). Each leaves a consistent state (wars, sieges, relations, flags, events) and returns its id.
  uint32_t forceWar(Gid attacker, Gid defender, int day);
  uint32_t forceSiege(Gid site, Gid attacker, int day);  // declares the war too when needed; the camp is placed
  void forceOwner(Gid site, Gid owner, int day);         // a conquest: Occupied for 30 days
  void forceBurn(Gid site, int day);                     // Burned (damage 80, popPct -50), refugees at the nearest friend
  void forceFamine(Gid site, int day);                   // food 0, SS_FAMINE; the chain of 15.6.3 starts from here
  void forceEvent(EvType t, Gid a, Gid b, Gid site, int32_t gx, int32_t gy, int day);   // a bare event (rumour tests)
  // the realm's record of a site (created from the node when missing; the forcing and the tick use it). Phase A: the
  // genesis owner, type and heart must be given by the caller the first time (Game::realmStep registers every
  // settlement it loads: noteSite)
  void noteSite(Gid site, Gid home, uint8_t type, int32_t gx, int32_t gy);

  // ---- persistence (Game's realm block: a length-prefixed blob, its own version byte first)
  void serialize(std::vector<uint8_t>& out) const;
  bool deserialize(const std::vector<uint8_t>& in);

  struct Stats {
    int ticks = 0, weeklyTicks = 0, instantiated = 0, active = 0, dormant = 0;
    double lastTickMs = 0, worstTickMs = 0, lastFocusMs = 0, worstFocusMs = 0;
    // REALM lane (M4 phase B) additions
    int catchUps = 0;              // dormant realms brought up to date on re-entry
    int scans = 0;                 // kingdom cell quarters whose settlements were read (EndlessSource::settlementsIn)
    double tickMsSum = 0;          // over `ticks` (average = tickMsSum / ticks)
    double worstProbeMs = 0, worstScanMs = 0, worstPlanMs = 0, worstInstMs = 0, worstHistMs = 0;   // focus's units of work
  };
  Stats stats;

  // ---- REALM lane (M4 phase B) additions
  static constexpr int YEAR = 360;                    // days in a year (harvests, ages, the history's "winters ago")
  // focus, then finish every pending instantiation at once (scripts, tests; no time-slicing)
  void focusNow(ew::EndlessSource& src, int32_t gx, int32_t gy, int day);
  bool focusPending() const { return !probe_.empty() || !pending_.empty(); }   // focus still has kingdoms to instantiate (time-sliced)
  int activeRadius = 3;            // kingdom cells round the player whose realms are active (VISION_PLAN 4.4)
  int maxActive = 48;              // nearest first; the rest dormant
  double focusBudgetMs = 2.0;      // per focus call (the web has no threads: instantiation is time-sliced)
  // the pre-play history of a kingdom, oldest first (VISION_PLAN 4.2; memoised, never saved). chronicle() gives the
  // same lines plus the realm's own deeds since the game began.
  std::vector<HistoryEntry> history(Gid kingdom) const;
  const std::vector<SimRuin>& simRuins() const { return simRuins_; }
  // the true specialisation of a site the window loaded (15.11; the lattice only estimates it): its food yield follows
  void noteEconomy(Gid site, uint8_t special);
  size_t memoryBytes() const;      // the sim's approximate memory (tests: under 200 KB)
  // more forcing (scripts `realm succession | rebels | peace | harvest | tension`, the lanes' tests)
  void forceRulerDeath(Gid kingdom, int day);                    // the ruler dies; the society's inheritance decides
  Gid forceCivilWar(ew::EndlessSource& src, Gid kingdom, int day);   // the realm splits: the rebels' id (0: too small)
  void forcePeace(Gid a, Gid b, int day);                        // their war ends in a treaty
  void forceHarvestFail(Gid kingdom, int day);                   // its harvest fails (granaries and food deals suffer)
  void forceTension(Gid a, Gid b, Tension t, WarCause cause, int day);   // put two realms on a rung of the ladder

  // ---- M5 Hearth and Hall (rpg/sim/realm_life.cpp, CITIZENS lane; VISION_PLAN 15.12). A loaded settlement's day as
  //      its residents lived it (life::Life::tick, once per in-game day per settlement it simulated): `foodDelta` the
  //      change of its food stock in percent points (bought and eaten beyond what it made: negative; a surplus:
  //      positive), `mood` its residents' mood 0..100. The realm folds them into SettlementState::food / mood, so
  //      hunger in the streets feeds the famine -> trade collapse -> war chain (15.6.3). A site the realm has never
  //      noted is ignored (noteSite first).
  void lifeReport(Gid site, int foodDelta, uint8_t mood, int day);
  // (CITIZENS lane, M5 phase B) a night raid on a loaded settlement (VISION_PLAN 10.4 M5): a failed defence costs it
  // prosperity 10 and damage 5 (its mood suffers a little either way). A site the realm has never noted is ignored.
  void lifeRaid(Gid site, bool failed, int day);
  // ---- M6 Steel (rpg/sim/realm_beasts.cpp, FOES lane; VISION_PLAN 7.6 "World bosses ... can raid settlements as a sim
  //      event"): a world boss raided a settlement (`damage` 0..100 added to its damage, prosperity and mood suffer; the
  //      event BeastRaid names the boss by `mon` (art::Monster)), and a world boss / named unique fell (BeastSlain: news
  //      across the horizon, +fame). A site the realm has never noted is ignored by beastRaid.
  void beastRaid(Gid site, uint8_t mon, int damage, int day);
  // (M6 fixer r3) the event's mag packs mon (bits 0-7), the foes::Rank of the beast (bits 8-10) and whether the
  // player made the kill (bit 11), so the news names the right beast (a named unique never passes for a world boss)
  void beastSlain(uint8_t mon, int32_t gx, int32_t gy, int day, uint8_t rank = 0, bool byPlayer = true);

 private:
  uint64_t seed_ = 0;
  uint64_t rng_ = 0;               // the sim's own stream (saved)
  int day_ = 0;
  uint32_t nextEvent_ = 1, nextWar_ = 1, nextSiege_ = 1;
  std::vector<KingdomState> kingdoms_;            // Gid order
  std::unordered_map<Gid, int> kingdomIx_;
  std::vector<Relation> relations_;
  std::vector<War> wars_;
  std::vector<Siege> sieges_;
  std::vector<SettlementState> settlements_;
  std::unordered_map<Gid, int> settlementIx_;
  std::vector<WorldEvent> events_;
  Renown renown_;
  int32_t focusKx_ = 1 << 30, focusKy_ = 1 << 30;
  SettlementState& state(Gid site);               // the record (must exist: noteSite first)
  KingdomState* kingdomMut(Gid id);
  WorldEvent& addEvent(EvType t, Gid a, Gid b, Gid site, int32_t gx, int32_t gy, int day);

  // REALM lane (M4 phase B)
  uint32_t nextRebel_ = 0;                        // rebel realms born so far (their ids: 0x800 + n)
  std::vector<SimRuin> simRuins_;
  // genesis (realm_genesis.cpp): not saved; rebuilt by the next focus
  ew::EndlessSource* src_ = nullptr;              // the source of the last focus (history and ruins need cultures)
  std::vector<Gid> pending_;                      // kingdoms in range still to instantiate, nearest first
  Gid planned_ = 0;                               // the pending kingdom whose plan and culture are built
  std::vector<uint64_t> probe_;                   // kingdom cells still to look at after a move, nearest first
  std::vector<Gid> histQueue_;                    // kingdoms whose history pre-roll is still to be written
  std::unordered_map<uint64_t, uint8_t> scanned_; // kingdom cells whose settlements are noted (quarter bits; 15 done)
  int32_t focusGx_ = 0, focusGy_ = 0;
  bool refocus_ = false;                          // a save was just loaded: rebuild the focus state
  mutable std::unordered_map<Gid, std::vector<HistoryEntry>> hist_;
  mutable std::unordered_map<Gid, RuinRecord> ruins_;
  void focusImpl(ew::EndlessSource& src, int32_t gx, int32_t gy, int day, double budgetMs);
  bool scanCell(ew::EndlessSource& src, int32_t cx, int32_t cy);   // reads one quarter; true once the cell is done
  void instantiate(ew::EndlessSource& src, Gid id);
  void linkRelations(ew::EndlessSource& src, Gid id);
  void setTiers();
  void rebuildIndex();
  void warmHistory(ew::EndlessSource& src, Gid id) const;
  int32_t seatX(const KingdomState& k) const;
  int32_t seatY(const KingdomState& k) const;
  // the tick (realm_tick.cpp)
  uint64_t roll();                                // the saved stream
  float frand() { return (float)(roll() >> 40) / (float)(1ull << 24); }
  bool chance(float p) { return frand() < p; }
  void tickDay(ew::EndlessSource& src, int span);  // span 1 (daily) or 7 (weekly, at 7x rates)
  void catchUp(KingdomState& k, int weeks);
  void tickEconomy(KingdomState& k, int span, int trades);
  void tickSettlement(SettlementState& s, KingdomState* K, int span);
  float harvestQuality(const KingdomState& k, int year) const;   // a pure hash of the seed, the land and the year
  void giveSite(SettlementState& s, Gid to, int day);
  Gid borderSite(const KingdomState& from, const KingdomState& to) const;
  std::vector<uint8_t> warNow_;                   // per kingdom index: at war today (tick scratch)
  void tickFood(KingdomState& k, int span);
  void tickSettlements(int span);
  void tickDiplomacy(int span);
  void tickWars(int span);
  void tickSieges();
  void tickRulers(ew::EndlessSource& src, int span);
  void tickCivil(ew::EndlessSource& src, int span);
  void recount(KingdomState& k);                  // pop, the capital, food yield from its settlements
  Relation* relMut(Gid a, Gid b, bool create);
  void setTension(Relation& r, Tension t, int day);
  uint32_t declareWar(Gid attacker, Gid defender, WarCause cause, int day, bool forced);
  void endWar(War& w, int day);
  uint32_t startSiege(War* w, Gid site, Gid attacker, int day, bool forced);
  void capture(Gid site, Gid owner, int day, War* w);
  void burn(Gid site, Gid by, int day);
  void succession(KingdomState& k, int day, bool died);
  Gid splitRealm(ew::EndlessSource* src, KingdomState& k, int day);
  void fallKingdom(KingdomState& k, int day, Gid by);
  void ruinSite(SettlementState& s, FallCause cause, Gid destroyer, int day);
  void prune();
  int32_t capX(const KingdomState& k) const;      // its capital's heart (global tile) or its seat
  int32_t capY(const KingdomState& k) const;
};

const char* evTypeName(EvType t);     // "WAR DECLARED"... (scripts, tests, debug)
const char* relName(Rel r);
const char* tensionName(Tension t);
const char* warCauseName(WarCause c);   // REALM lane: "FAMINE", "BROKEN TRADE"...
const char* fallCauseName(FallCause c);

// internal helpers shared by rpg/sim/realm*.cpp
namespace detail {
double nowMs();
float sitePop(uint8_t type);                       // thousands
uint8_t baseGarrison(uint8_t type, bool kingdom);
uint8_t foodYield(uint8_t type, uint8_t special);  // percent of its need
uint8_t estimateSpecial(uint8_t type, uint64_t h);
uint8_t traitsFor(const uint8_t* values, uint64_t h);  // ruler traits (RT_*) from the culture's values
}  // namespace detail

}  // namespace realm
