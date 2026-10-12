// EMBERVALE M8 "Fellowship": companions (VISION_PLAN 9, 13 M8, 15.6.2 permadeath, 15.26 phase A contracts).
//
// Companions are real people of the living world: tavern regulars and other census residents (M5: their own job, home,
// household, friends, needs and history), mercenaries, people rescued in sagas, and one bonded animal (a hound or a wolf
// raised from a pup). Each has a personality, a culture (M3), a vocation, a stake in the world (a burned home village, a
// war, a lost kin...), approval of the player's deeds and a bond that unlocks skills. They fight beside the player,
// follow through towns, doors, stairs and journeys, take orders from a radial (follow, wait, attack, defend, a skill,
// hold back, go home), wear gear on their own paper doll in its maker's style (M6), and can DIE FOR GOOD (owner 15.6.2;
// the player cannot): at 0 HP they are downed and bleed out unless revived in time; a death leaves a grave, belongings,
// and grief in their family and friends (life::Life::residentDied).
//
// Shared contract, frozen after M8 phase A (lead, 2026-10-11). The FELLOWS lane is its only editor: it may ADD
// declarations, fields and enum values (append-only, before COUNT), never rename, remove or change the meaning of what
// is here. Fields marked VOICE are the VOICE lane's to fill and read (banter, talk, personal quests); the FELLOWS lane
// saves them in the party block untouched. Pure rules live in rpg/sim/party.cpp (FELLOWS); the Game side (actors, AI,
// commands, downed and death, deeds, recruiting, graves) in rpg/sim/party_game.cpp (FELLOWS); the companions' talk in
// rpg/sim/party_talk.cpp and the banter in rpg/sim/banter.cpp (VOICE).
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "rpg/art/art_human.h"
#include "rpg/sim/items.h"
#include "rpg/world/ids.h"

class Game;
struct Actor;

namespace party {

// ---------------------------------------------------------------- who they are
// What they do in a fight (VISION_PLAN 9 AI): the Warrior taunts and holds the line, the Archer keeps its distance (the
// bandit-archer kiting), the Mage casts its culture tradition's rank-1 spells (M9 deepens it), the Healer heals below
// 50 %, the Rogue flanks (the wolf circling logic). Hound: the bonded animal (bites, pins, guards the player).
enum class Vocation : uint8_t { Warrior, Archer, Mage, Healer, Rogue, Hound, COUNT };
// How they came to the party
enum class Origin : uint8_t {
  Tavern,      // a tavern regular (a census resident met at the inn; VISION_PLAN 9: 40 % of towns, 2 per city)
  Resident,    // another census resident won over (a friend, someone helped)
  Mercenary,   // a hireling: paid by the day (Member::wage), no personal arc
  Rescued,     // someone saved in a saga or a quest (a missing person led out, a captive freed)
  Saga,        // a saga's own payoff (a story's ally who stays)
  Animal,      // the bonded hound or wolf
  COUNT
};
// Where they are in their life with the party
enum class State : uint8_t {
  With,        // travelling with the player (Member::stance says how they fight)
  Downed,      // at 0 HP, bleeding out: revived within DOWNED_SECS (Member::downT counts down) or Dead
  Waiting,     // told to wait at a place (Member::gx / gy / mapId / floor): there when the player comes back
  Home,        // living at home: their census home, or the player's house (Member::homePlot)
  Left,        // left over the player's deeds (approval below APPROVAL_LEAVE): at home, recruitable after amends
  Dead,        // gone for good (a Fallen record keeps the grave); the record is removed from Party::members
  COUNT
};
// The command radial (hold a portrait, or the companion's dialogue). Follow / Attack / Defend / Passive are stances
// (Member::stance); Wait and Home change the state; Skill uses the best unlocked bond skill now.
enum class Cmd : uint8_t {
  Follow,      // keep 1.5 tiles behind; fight what the player fights or what attacks the party
  Wait,        // wait here (State::Waiting)
  Attack,      // go for the player's target (the last foe the player struck or aimed at)
  Defend,      // stay at the player's side, intercept what comes at the player, never chase
  Skill,       // use the best ready bond skill now
  Passive,     // hold back: keep out of fights (danger feedback: the owner's "tell them to hold back")
  Home,        // go home (State::Home): the player's house when they have one, else their own
  COUNT
};
// The stake in the world their story grows from (personal quests are generated from it through the M6b saga composer)
enum class Hook : uint8_t {
  None,
  BurnedHome,  // their village was burned (a raid, a war: Stake::site): avenge it, or rebuild it
  LostKin,     // a sibling / child / spouse missing (refugees, carried off by raiders, a census RF_AWAY: Stake::kin)
  War,         // a veteran or a deserter of a war (Stake::kingdom: the side they fought for or against)
  Revenge,     // a named beast or a bandit chief killed someone dear (Stake::foe)
  Heirloom,    // a family heirloom lost in a ruin (Stake::ruin)
  Debt,        // owes money or a life (a mercenary's reason)
  Exile,       // cast out of their homeland (Stake::kingdom)
  Faith,       // a pilgrimage or a vow to their god
  Ambition,    // wants renown, a name in the songs
  Love,        // someone waiting for them elsewhere (Stake::kin)
  COUNT
};
struct Stake {
  Hook hook = Hook::None;
  ew::Gid site = 0;        // the place it happened (their burned village, where the kin was last seen)
  ew::Gid kin = 0;         // life::npcId of the kin it is about (0 none)
  ew::Gid foe = 0;         // a named unique / world boss id (foes.h), or a bandit camp's site id (0 none)
  ew::Gid kingdom = 0;     // the kingdom it is about (0 none)
  ew::Gid ruin = 0;        // the ruin it is about (0 none)
  int32_t day = 0;         // when it happened (game days; negative: before the adventure began)
  uint32_t seed = 0;       // the details' seed (names of the dead, the heirloom's name...)
};

// Personality (VISION_PLAN 9): four axes -2..2, and likes / dislikes as Deed bit sets (1u << Deed)
struct Personality {
  int8_t brave = 0, kind = 0, pious = 0, lawful = 0;
  uint32_t likes = 0, dislikes = 0;
};

// ---------------------------------------------------------------- the deeds bus
// Player deeds the companions react to (approval), remember (Member::mem) and talk about (banter). Raised through
// Game::deed() by the code where they happen (phase A wired: kills (partyKilled), quests done and failed (partyQuest),
// stealing a smith's patterns, joining a war's side, feeding and befriending townsfolk, buying the inn's meal, praying at
// a shrine, entering a ruin or cave first, resting; sagas through the DSL effect `deed <word>` (VOICE)).
enum class Deed : uint8_t {
  QuestDone,        // helped someone (a quest completed); about: the giver's site
  QuestFailed,      // let someone down
  Rescued,          // brought a missing person or captive home alive
  HelpedRefugees,   // fed, housed or escorted refugees
  Charity,          // gave gold or food to someone in need (feed, supply)
  Befriended,       // made a friend of a townsperson
  Mercy,            // spared a foe who surrendered, forgave a debt (sagas)
  Cruelty,          // killed the helpless, tortured, broke a promise to the weak (sagas)
  KilledInnocent,   // a civilian died by the player's hand; about: their site
  KilledGuard,      // a town's guard or a kingdom's soldier died by the player's hand; about: their kingdom
  Stole,            // theft (a smith's patterns, a saga's theft); about: the victim's site
  ClearedBandits,   // a bandit or bandit camp put down
  SlewBeast,        // a dangerous beast or monster killed near people
  SlewMonster,      // a named unique or a world boss slain
  JoinedWar,        // took a side in a war; about: the kingdom fought AGAINST (a companion of that homeland minds)
  DefendedTown,     // fought off a raid or broke a siege; about: the town
  ExploredRuin,     // went into a ruin or a cave for the first time
  Tavern,           // drank and ate at an inn
  Prayed,           // prayed at a shrine or temple
  Bribed,           // paid off a guard, a toll, a liar (sagas)
  Lied,             // lied to someone who trusted you (sagas)
  Oathbreak,        // broke an oath or a deal (sagas)
  DefiledGrave,     // robbed a grave or a tomb (sagas, lore)
  Abandoned,        // left a companion bleeding out, or fled while they fought
  COUNT
};
struct DeedInfo {
  const char* name;            // "HELPED REFUGEES"
  int8_t brave, kind, pious, lawful;   // how each personality axis weighs it (-3..3)
  int8_t base;                 // the reaction of a companion with all axes 0 (-5..5)
  uint8_t flags;               // DF_*
};
constexpr uint8_t DF_HOME = 1;       // about == the companion's homeland kingdom or home site: weighs 2x (and flips JoinedWar)
constexpr uint8_t DF_LOUD = 2;       // remembered for long (Member::mem keeps it past MEMORY_DAYS)
const DeedInfo& deedInfo(Deed d);
const char* deedName(Deed d);
inline uint32_t deedBit(Deed d) { return 1u << (unsigned)d; }

// what a companion thinks of a deed: -15..15 (VISION_PLAN 9: ±1 to ±15). about: the deed's subject (a site, a kingdom,
// a person); homeland / home: the companion's own (DF_HOME doubles it; JoinedWar against the homeland is a betrayal)
struct Member;
int approvalDelta(const Member& m, Deed d, ew::Gid about, ew::Gid homeland);

// ---------------------------------------------------------------- bond skills (unlocked by bond, not XP)
enum class Skill : uint8_t {
  Taunt, ShieldWall, LastStand,        // Warrior: pulls aggro in 5 tiles / halves damage to the party 6 s / at 0 HP stands 5 s more
  PinningShot, Volley, EagleEye,       // Archer: slows a foe / arrows on an area / long range crits
  Firebolt, Ward, Storm,               // Mage (the culture's tradition dresses them; M9 replaces them): a bolt / a shield on the player / an area burst
  Mend, Purify, SecondWind,            // Healer: heals the most hurt / cleanses burns and slows / revives a downed companion from afar
  Flank, Smoke, Assassinate,           // Rogue: behind the target / foes lose them / a finishing blow on the weakened
  Guard, PinDown, Fetch,               // Hound: stands over the downed player or companion / holds a foe in place / brings back a pickup
  COUNT
};
struct SkillInfo {
  const char* name;            // "SHIELD WALL"
  Vocation voc;
  uint8_t bond;                // the bond it unlocks at (SKILL_BOND[tier])
  float cooldown;              // seconds
  const char* what;            // one phone-panel line ("HALVES THE BLOWS THAT LAND ON YOU BOTH FOR SIX SECONDS.")
};
constexpr uint8_t SKILL_BOND[3] = {20, 50, 80};
const SkillInfo& skillInfo(Skill s);
// the three skills of a vocation, in unlock order
std::array<Skill, 3> skillsOf(Vocation v);
// bit k set: skill k of skillsOf(m.voc) is unlocked by the member's bond
uint8_t unlockedSkills(const Member& m);

// ---------------------------------------------------------------- gear (the paper doll: the same slots as the player's)
enum Slot : uint8_t { S_WEAPON, S_BOW, S_STAFF, S_ARMOR, S_HELMET, S_SHIELD, S_RING, S_AMULET, S_GLOVES, S_BOOTS, S_CLOAK, SLOTS };
int slotOf(ItemKind k);        // -1: not wearable
const char* slotName(int s);   // "WEAPON"...

// ---------------------------------------------------------------- tuning (VISION_PLAN 9, 15.6.2, M8 verification)
constexpr int MAX_RECORDS = 8;         // members kept (with the player, waiting, at home, left)
constexpr int MAX_FALLEN = 64;         // graves remembered (the oldest beyond: their record kept as a name only)
constexpr int MAX_MEMORIES = 24;       // deeds a member remembers
constexpr int MEMORY_DAYS = 60;        // a deed without DF_LOUD fades after this
constexpr int PARTY_PEOPLE = 1;        // people with the player at once (plus one animal)
constexpr int PARTY_PEOPLE_LEADER = 2; // from LEADERSHIP_LEVEL
constexpr int LEADERSHIP_LEVEL = 20;
constexpr int APPROVAL_COMPLAIN = -40; // below: complains
constexpr int APPROVAL_LEAVE = -70;    // below: leaves (State::Left)
constexpr int APPROVAL_QUEST = 50;     // from: the personal quest is offered
constexpr int BOND_PER_DAY = 1;        // bond grows by a day together (and with approval)
constexpr float DOWNED_SECS = 30.0f;   // the bleeding-out window (owner 15.6.2: "a few seconds to revive")
constexpr float REVIVE_SECS = 2.0f;    // binding the wounds (the player stays beside them, unhurt)
constexpr int REVIVE_HP_PCT = 30;      // back on their feet with this much health
constexpr int LOW_HP_PCT = 35;         // the low-health warning (portrait pulse, a cry)
constexpr float FOLLOW_TILES = 1.5f;   // formation offset behind the player
constexpr int CATCH_UP_TILES = 30;     // never more than this far behind: beyond, they catch up out of sight
constexpr float BANTER_GAP_SECS = 90;  // at most one banter line in this long (VISION_PLAN 9)
constexpr int WAGE_MIN = 30, WAGE_MAX = 120;   // a mercenary's gold a day (VISION_PLAN 9)
// the M8 balance gate (rpg_test --companion): the party takes 25..40 % of the damage dealt to player and party
constexpr int SHARE_LO_PCT = 25, SHARE_HI_PCT = 40;

// ---------------------------------------------------------------- the records
// a deed remembered (what they bring up in talk and banter: "I HAVEN'T FORGOTTEN BRIGHTWATER.")
struct Memory {
  Deed deed = Deed::QuestDone;
  int8_t delta = 0;            // what it did to their approval
  int32_t day = 0;
  ew::Gid about = 0;
};
// Member::flags
constexpr uint16_t MF_COMPLAINED = 1;   // has said they are unhappy (below APPROVAL_COMPLAIN) since approval last rose past it
constexpr uint16_t MF_QUEST_OFFERED = 2;   // the personal quest was offered (VOICE)
constexpr uint16_t MF_QUEST_DONE = 4;      // the personal quest is over (VOICE)
constexpr uint16_t MF_LEFT_ONCE = 8;       // has left the player before (an apology is owed: VOICE)
constexpr uint16_t MF_MOUNTED = 16;        // rides their own horse when the player rides (Member::horse)
constexpr uint16_t MF_WOUNDED = 32;        // revived lately: weaker until woundedUntil (a rest heals it)
constexpr uint16_t MF_PUP = 64;            // Animal: still a pup (grows on its own day count)

struct Member {
  uint32_t id = 0;             // party-unique, stable across saves (Party::nextId)
  Origin origin = Origin::Tavern;
  Vocation voc = Vocation::Warrior;
  State state = State::With;
  Cmd stance = Cmd::Follow;    // Follow / Attack / Defend / Passive (the fighting stance while State::With)
  // ---- identity (a census person keeps their census identity: kin, friends, home, grief all come from M5)
  ew::Gid npc = 0;             // life::npcId(site, res) (0: a mercenary, a saga's person, the animal)
  ew::Gid site = 0;            // their home settlement (census site; for others where they were met)
  int16_t res = -1;            // census index in `site` (-1 none)
  std::string name;
  bool female = false;
  uint8_t age = 30;
  uint8_t job = 0;             // life::Job they had (their trade: what they talk about, their skills' flavour)
  uint64_t culture = 0;        // cult::CultureId (dress, speech, banter's border comments)
  uint8_t people = 0;          // the M3 people (human, half-breed, elf)
  uint32_t lookSeed = 0;       // the census look seed (Game::dressAsResident), or the generated one
  uint8_t species = 0;         // Animal: 0 hound, 1 wolf (art::houndSheet)
  Personality p;
  Stake stake;
  // ---- standing
  int8_t approval = 0;         // -100..100
  uint8_t bond = 0;            // 0..100: skills (SKILL_BOND)
  uint16_t flags = 0;          // MF_*
  // ---- body (hp < 0: full health)
  float hp = -1;
  float downT = 0;             // State::Downed: seconds of bleeding out left
  int32_t woundedUntil = -1;   // MF_WOUNDED until this day
  // ---- gear (their own pack; eq indexes inv per Slot, -1 empty)
  std::vector<Item> inv;
  std::array<int16_t, SLOTS> eq{{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}};
  uint8_t horse = 0;           // their own horse: home::Breed + 1 (0 none)
  int32_t wage = 0;            // Mercenary: gold a day
  int32_t paidUntil = -1;      // Mercenary: paid up to this day
  // ---- where (Waiting, Home, Left)
  int32_t gx = 0, gy = 0;      // global tile (outdoors) or the tile inside the building / site map
  ew::Gid mapId = 0;           // 0 the overworld, else the building's id (Bldg::id) or the cave / ruin's site id
  uint8_t mapKind = 0;         // 0 overworld, 1 cave / ruin, 2 building
  uint8_t floor = 0;
  uint32_t homePlot = 0;       // State::Home at the player's house: its home::Plot id (0: their own home)
  // ---- their record together
  int32_t joinedDay = 0, lastDay = 0;   // joined; the last day bond grew
  uint32_t days = 0;           // days together
  uint16_t kills = 0, downs = 0, revives = 0;
  std::vector<Memory> mem;     // the deeds they remember (MAX_MEMORIES, newest last)
  // ---- VOICE lane (saved by FELLOWS, meaning owned by VOICE): banter and talk memory, the personal quest
  std::string quest;           // the personal quest's saga spec id ("" none)
  uint32_t questInst = 0;      // its story::Engine instance id while running (0 none)
  uint8_t questState = 0;      // VOICE's own stages (0 not offered)
  uint32_t talkBits = 0;       // topics already talked through (VOICE's bits)
  int32_t lastTalkDay = -1;
  uint32_t lastLine = 0;       // speech::lineHash of the last banter / talk line
  // ---- runtime (never saved)
  int actor = -1;              // Actor::id in Game::actors (-1: not in play)
  float skillCd[3] = {0, 0, 0};
  float reviveT = 0;           // Downed: seconds of binding done by the player
  float lowCryT = 0;           // the low-health cry's cool-down
  int target = -1;             // the actor it fights (Actor::id)
  // ---- phase B (FELLOWS lane). Saved (party block VERSION 2):
  int32_t leftDay = -1;        // Left / dismissed: the day they walked off home (VOICE: amends; the census life resumes)
  ew::Gid hiredAt = 0;         // Mercenary: the inn (Bldg::id) they were hired at (they go back there when let go)
  uint8_t mercSlot = 0;        // Mercenary: which of the inn's sellswords (0..MERC_MAX-1)
  int32_t mountedDay = -1;     // the day their horse was bought (Member::horse; MF_MOUNTED)
  // ---- (fixer M8 r1) saved (party block VERSION 3): their look, fixed when they join. The census that dresses a
  // street body is put away when its place leaves the window (Life::evictFar) and its seed is keyed by site handles,
  // so re-deriving it later changed a companion's hair, ears and skin partway through a run.
  bool hasLook = false;
  art::HumanLook look;         // the street look before their gear (Game::partyLook wears the gear over it)
  // ---- phase B runtime (never saved)
  bool inPlay = false;         // their actor was in play since the session began (a later absence is a map change: carry)
  float stuckT = 0;            // seconds without progress toward where they want to be (the trail takes over)
  Vec2 lastPos;                // where they stood when stuckT last reset
  int trailAt = -1;            // the Party::trail crumb they are walking to (-1: straight for the formation slot)
  float lastBlow = 0;          // the last blow that landed on them (damage), and whether it was heavy
  bool lastBlowHeavy = false;
  float lastStandT = 0;        // Last Stand: seconds of fighting on at 1 HP left
  float eagleT = 0;            // Eagle Eye: seconds of long, armour-piercing shots left
  float guardT = 0;            // Guard (the hound): seconds standing over the downed
  float potionCd = 0;          // seconds before another potion
  float kiteT = 0;             // the archer's / mage's back-step clock
  float homeT = 0;             // living at the player's house: seconds into the current activity
  int orbit = 1;               // the rogue's circling direction (the wolf circling logic)
  bool walkingOff = false;     // Left / dismissed: their body walks home out of sight before it goes
  float tauntT = 0;            // Taunt: seconds the foes near keep to this member (partyAggro)
  float flankT = 0;            // the rogue: seconds of the current circling
  bool closeIn = false;        // an archer or mage cornered: the next blow is the knife's (or the staff's), not a shot
};

// a companion who died (VISION_PLAN 15.6.2: a grave you can visit, their belongings, others remember them)
struct Fallen {
  uint32_t id = 0;             // the Member::id they had
  std::string name;
  Vocation voc = Vocation::Warrior;
  Origin origin = Origin::Tavern;
  uint64_t culture = 0;
  ew::Gid npc = 0, site = 0;   // their census identity (kin and friends grieve: life::Life::residentDied)
  int32_t gx = 0, gy = 0;      // the grave's global tile (overworld): their home's graveyard, or where they fell
  int32_t day = 0;             // the day they died
  std::string cause;           // "FELL TO A FROST TROLL ON THE HIGH ROAD"
  uint32_t seed = 0;           // the epitaph's and the stone's seed
  uint32_t days = 0;           // days they travelled with the player
  uint16_t kills = 0;
  uint8_t bond = 0;
  uint8_t graveFlags = 0;      // GF_*
  std::vector<Item> belongings;   // laid with the grave (the player may take one keepsake: GF_KEEPSAKE)
  // ---- phase B (FELLOWS lane; saved, party block VERSION 2)
  uint8_t rite = 0;            // art::GraveLook::rite of their people (0..art::GRAVE_RITES-1; the hound: a cairn)
  uint8_t weapon = 0;          // art::HumanLook::weapon laid on the stone (0 none; the hound: none)
  uint8_t species = 0xFF;      // the bonded animal's species (0 hound, 1 wolf); 0xFF a person
  ew::Gid wasAt = 0;           // where they fell: the site id (a cave, a ruin, a settlement), 0 the open country
  // ---- (fixer M8 r3; saved, party block VERSION 4)
  bool female = false;         // Member::female (the grave's words and their kin say SHE / HE right for everyone)
};
constexpr uint8_t GF_VISITED = 1, GF_KEEPSAKE = 2, GF_FLOWERS = 4;

// test and HUD figures (never saved)
struct Stats {
  double dmgPlayer = 0, dmgParty = 0;           // damage taken since the last reset (the share gate)
  double dealtPlayer = 0, dealtParty = 0;       // damage dealt
  float worstLagTiles = 0;                      // the farthest a following member fell behind (tiles)
  int catchUps = 0;                             // times a member caught up out of sight
  int downs = 0, revives = 0, deaths = 0, deeds = 0, banter = 0;
  // ---- phase B (FELLOWS lane): what the vocations and skills did (rpg_test --companion observes them)
  int taunts = 0, shieldWalls = 0, lastStands = 0, pins = 0, volleys = 0, eagleEyes = 0, bolts = 0, wards = 0, storms = 0;
  int heals = 0, purifies = 0, secondWinds = 0, flanks = 0, backstabs = 0, smokes = 0, assassinations = 0;
  int guards = 0, pinDowns = 0, fetches = 0, bites = 0, potions = 0, intercepts = 0, carried = 0, abandoned = 0;
  int arrows = 0;                               // shots loosed by the party
  float archerMinTiles = 1e9f;                  // the nearest a kiting archer let its foe come while loosing (tiles)
  float defendMaxTiles = 0;                     // the farthest a Defend member strayed from the player (tiles)
  double stepMsSum = 0, stepMsWorst = 0;        // partyStep + the members' partyActor (wall ms) per update step
  int stepN = 0;
  double banterMsWorst = 0;                     // party::banterStep's worst step (VOICE; not in stepMs*)
};

class Party {
 public:
  std::vector<Member> members;
  std::vector<Fallen> fallen;
  uint32_t nextId = 1;
  int32_t animalDay = -1;      // the day the animal was taken in (a pup grows up over 14 days)
  // ---- runtime (never saved)
  struct Pending { Deed d; ew::Gid about; int weight; };
  std::vector<Pending> pending;   // deeds raised this step (Game::deed), applied by partyStep
  Stats stats;
  float banterT = 0;           // seconds since the last banter line
  std::string lastBanter;      // the last line said (tests, the HUD)
  int lastBanterBy = -1;       // its member index
  // ---- phase B runtime (FELLOWS lane; never saved)
  std::vector<Vec2> trail;     // the player's footsteps on this map (a crumb a tile apart, newest last; the window's
                               // shifts move them): the way a member who lost the formation follows, and the catch-up's
                               // out-of-sight landing place
  int trailMap = -9;           // the map key the trail belongs to (Game::mapKey(); another map clears it)
  int mapSeen = -9;            // the map key partyStep last saw (a change: mercenaries, waiting members, the carry)
  int mapChanges = 0;          // map changes seen (tests)
  float shieldWallT = 0;       // Shield Wall: seconds left (blows on the player and the party halved)
  float wardHp = 0;            // Ward: what the mage's shield on the player still soaks
  float smokeT = 0;            // Smoke: seconds the foes stay lost
  // (never saved) the blows of the last half minute or so, on the player and on the party (decayed in partyStep): the
  // pack's choice leans toward whoever has been spared (Game::partyAggro), so the party's share of a fight's blows
  // holds near the middle of SHARE_LO_PCT..SHARE_HI_PCT whatever the ground and the vocation
  float recentPl = 0, recentParty = 0;
  float slowT = 0;             // the slow tick's clock (graves, waiting members, the house: a few times a second)
  double actorMs = 0;          // the members' partyActor wall time this step (Stats::stepMs*)
  int walkOffs = 0;            // bodies walking home out of sight (Actor::partyTag PartyOps::PT_WALKOFF)
  int playerTarget = -1;       // the foe the player last struck (Actor::id; the Attack stance goes for it)
  float playerTargetT = -99;   // Game::time of that blow
  uint32_t graveSerial = 0;    // World::placeSerial the graves were last stamped at
  bool graveDirty = true;      // a grave was added or moved: stamp again
  std::vector<std::pair<int32_t, int32_t>> stamped;   // the global tiles stamped with a grave (to lift a moved one)
  // the tavern regulars of a settlement (census indices), worked out once per session (Game::partyRegulars)
  std::vector<std::pair<ew::Gid, std::vector<int16_t>>> regulars;

  Member* byId(uint32_t id);
  const Member* byId(uint32_t id) const;
  int indexOfActor(int actorId) const;   // members index of the member embodied by this actor (-1 none)
  int with() const;                      // members travelling with the player (State::With or Downed)
  int people() const;                    // ... of them, people (not the animal)
  bool animal() const;                   // an animal is with the player
  bool empty() const { return members.empty(); }
  void clear();
  // the party block (Game's save, after the talk block: a length-prefixed blob, its own version byte first)
  static constexpr uint8_t VERSION = 4;   // 4: fixer M8 r3 (Fallen female)
  //  3: fixer M8 r1 (Member hasLook / look)
  //  2: M8 phase B (Member leftDay / hiredAt / mercSlot / mountedDay; Fallen rite / weapon / species / wasAt)
  void serialize(std::vector<uint8_t>& out) const;
  bool deserialize(const std::vector<uint8_t>& in);
  bool operator==(const Party& o) const;  // the saved fields (tests)
};

// how many people may travel with the player at this level (PARTY_PEOPLE, PARTY_PEOPLE_LEADER from LEADERSHIP_LEVEL)
int capacity(int playerLevel);
// a member's level: the player's level - 1 (VISION_PLAN 9: no grind), at least 1
int levelFor(int playerLevel);
// a member's fighting numbers at this player level, before gear: the --companion balance knobs (FELLOWS tunes them)
struct Body { float maxHp, dmg, armor, speed, range, aggroR; };
Body bodyFor(const Member& m, int playerLevel);

// what a census person would be as a companion: vocation from job and traits, personality from traits, culture and a
// seed, a hook from their real history (the stake is filled by the FELLOWS lane's profile in party_game.cpp from the
// census, the realm and the world). Deterministic.
Vocation vocationFor(uint8_t job, uint8_t traits, uint32_t seed);
Personality personalityFor(uint8_t traits, uint8_t job, uint32_t seed);

const char* vocationName(Vocation v);   // "WARRIOR"
const char* originName(Origin o);       // "TAVERN REGULAR"
const char* stateName(State s);         // "WITH YOU"
const char* cmdName(Cmd c);             // "FOLLOW" (the radial's labels)
const char* hookName(Hook h);           // "BURNED HOME"

// ---------------------------------------------------------------- the VOICE lane's entry points (rpg/sim/banter.cpp)
// Every line: first person, natural, the M7b speech bar (rpg/sim/talk.h lint), at most speech::PANEL_CHARS x 2 lines.
// once per Game::update step (partyStep calls it last): banter by context (crossing a culture border first, weather,
// night, after a hard fight, a wonder, low health, approval thresholds, news of their homeland's war, each other), at
// most one line per BANTER_GAP_SECS, said through Game::partySay
void banterStep(Game& g, float dt);
// what a member says when a deed moves their approval a lot (|delta| >= 4): "" for nothing. The FELLOWS lane's deed
// handling says it (Game::partySay) for the member it moved most
std::string deedLine(const Game& g, const Member& m, Deed d, int delta);
// what a member cries out at a turn of the fight or of their heart (kind as Ev::PartyAlert: 1 low health, 2 downed,
// 3 revived, 5 joined, 6 leaving, 7 complaining, 8 a skill learned): "" for nothing. Said along with the alert
std::string cryLine(const Game& g, const Member& m, int kind);
// the words carved on a fallen companion's grave (two or three short lines joined by " / ")
std::string epitaph(const Game& g, const Fallen& f);
// a member's own story in two or three first-person sentences from their real history (their trade, home, kin, the
// stake): the paper doll's page (VIEW lane shows it)
std::string aboutLine(const Game& g, const Member& m);

// ================================================================ M8 phase B (FELLOWS lane) additions
// ---------------------------------------------------------------- recruiting (Game::partyOffer / partyRecruit)
// PartyOffer::why: the reason a person will not come, as one keyword the VOICE lane puts into their own words:
//   NONE       not someone who could join at all (a monster, a companion already, a body the census does not know)
//   DEAD       the census has them dead
//   KEY        a key person (life::RF_KEY: a quest's giver, a ruler's household): their place is here
//   DUTY       at work they cannot leave this hour: a guard on watch, the innkeeper serving
//   CHILD      a child
//   OLD        an elder (or past 62)
//   FULL       the party already has party::capacity(level) people with the player (the animal counts apart)
//   MEMBER     already one of the party (waiting, at home): ask them to come along instead (Cmd::Follow)
//   ANGRY      left the player over their deeds (State::Left): amends first (VOICE), then partyCommand(Follow)
//   UNWILLING  no reason to go: not a tavern regular met at the inn of an evening, not a friend, never helped by the
//              player (the census person is happy where they are)
//   BROKE      a sellsword whose first day's wage the purse cannot pay (PartyOffer::wage says how much)
//   HIRED      a sellsword already hired by the player (at the inn: the one left there)
//   ANIMAL     a second animal: one bonded animal at a time
// Origins partyOffer gives: Tavern (a regular at the inn of an evening, or a befriended regular anywhere), Resident (a
// friend of the player's, or someone whose job the player did: npcQuestsDone), Mercenary (a sellsword at an inn:
// Actor::partyTag PT_MERC0 + slot, wage by the place's danger), Rescued (an M2 missing person led home: Actor::quest
// with QF_ESCORTED; their census resident when bound, else a stranger of that name).
constexpr int TAVERN_TOWN_PCT = 40;    // towns with a tavern regular (VISION_PLAN 9); a city has TAVERN_CITY
constexpr int TAVERN_CITY = 2;
constexpr int MERC_MAX = 2;            // sellswords at a town's or a city's inn (0..2, by the inn)
constexpr int PUP_DAYS = 14;           // a pup grows up over this many days (bodyFor scales; art::HoundLook::age)
constexpr int PUP_PRICE = 60;          // gold for a pup from a farmer, a herder or a stablemaster
constexpr int HORSE_FOR_COMPANION_PCT = 100;   // a companion's horse costs the breed's price (home::breedInfo) x this / 100
// Actor::partyTag (the FELLOWS lane's scratch on actors that are not companions)
constexpr uint8_t PT_MERC0 = 1;        // a sellsword for hire at an inn: PT_MERC0 + slot (slot 0..MERC_MAX-1)
// Member::flags (phase B)
constexpr uint16_t MF_CARRIED = 128;   // carried out of a map while bleeding out: Member::downT was halved (CARRY_FACTOR)
constexpr uint16_t MF_DOWN_WAIT = 256; // (fixer M8 r2) downed while Waiting: revived back to Waiting, not into the party
constexpr uint16_t MF_DOWN_HOME = 512; // (fixer M8 r2) downed while living at Home: revived back to Home
// downed while travelling with the player (not while Waiting or at Home): counts toward the party's size
inline bool downedWith(const Member& m) { return m.state == State::Downed && !(m.flags & (MF_DOWN_WAIT | MF_DOWN_HOME)); }
constexpr float CARRY_FACTOR = 0.5f;   // the bleeding-out time left after each map change made while downed
constexpr float HEAVY_BLOW_PCT = 0.3f; // a blow of this share of their max health while down ends it at once
// Fallen::graveFlags (phase B)
constexpr uint8_t GF_SITED = 8;        // the grave is in the world at (gx, gy) (stamped: a solid tile, interactable)
constexpr uint8_t GF_HOME = 16;        // ... in their home settlement's temple yard or graveyard (not where they fell)
// how VIEW draws the companions (sim fields it reads; no other state):
//   - downed: Actor::posture == art::Posture::Downed with Actor::st Idle (the posture path draws it); a member's
//     State::Downed and Member::downT / DOWNED_SECS the bleeding-out ring; Member::reviveT / REVIVE_SECS the binding ring
//   - the player binding wounds: the player's Actor::posture == art::Posture::Kneel (st Idle) while Member::reviveT > 0
//   - mounted: Actor::posture == art::Posture::Ride on a companion actor (Actor::comp) whose Member::horse (home::Breed
//     + 1) is not 0: art::horseSheet of that breed under them, the rider at riderSeat, as the player's mount; they ride
//     only while the player rides (Game::homeRiding()) and dismount with them
//   - the hound: a companion actor with !Actor::human; Game::partyHoundLook(member) gives its art::HoundLook (age from
//     MF_PUP and the days since Party::animalDay), Actor::st / face / animT the frame (HF_RUN walking, HF_BITE in
//     Windup / Strike, HF_DOWN downed, HF_SIT idle beside the player)
//   - graves: every Fallen with GF_SITED stands at global tile (gx, gy) (local: gx - World::ox); Game::partyGraveLook(i)
//     gives the art::GraveLook (rite, weapon, fresh for 3 days, GF_FLOWERS), art::graveSprite anchored bottom-centre on
//     the tile; the tile is stamped art::Prop::Filler (solid, never drawn) like the M7 yard objects;
//     Game::partyGraveAt(tx, ty) finds the grave at a local overworld tile (-1 none)
//   - mercenaries for hire: actors with Actor::partyTag in [PT_MERC0, PT_MERC0 + MERC_MAX) in an inn interior

// the stake in the world of a census person, from the real world (the census, the realm, the region's named uniques,
// the ruins near home), deterministic: the hook and every id it can fill (site, kin, foe, kingdom, ruin, day, seed).
// Game::partyRecruit calls it; tests and VOICE may call it for anyone (party_recruit.cpp)
Stake stakeFor(Game& g, int site, int res);

}  // namespace party
