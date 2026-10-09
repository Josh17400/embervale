// EMBERVALE simulation: actors, combat, AI, inventory, quests, dialogue, time of day, saving. No SDL.
#pragma once
#include <array>
#ifndef __EMSCRIPTEN__
#include <future>
#endif
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/sim/appearance.h"
#include "rpg/sim/backgrounds.h"
#include "rpg/sim/common.h"
#include "rpg/sim/craft.h"
#include "rpg/sim/explored.h"
#include "rpg/sim/factions.h"
#include "rpg/sim/items.h"
#include "rpg/sim/life.h"
#include "rpg/sim/realm.h"
#include "rpg/sim/war.h"
#include "rpg/sim/world.h"
#include "rpg/story/story.h"

// Creator: the character creator after NEW GAME (the world exists; the player picks looks and a background).
// Forge (M6): the crafting screen at a workstation (Game::bench; rpg/view/craft_ui.cpp draws it, the NUMBERS lane).
enum class Mode : uint8_t { Title, Play, Dialogue, Menu, Shop, LevelUp, Dead, Paused, Creator, Forge };

// Game::storyFlags bits (saved, SAVE_VER 3). Bits 8..31 are free: define them next to the code that owns them,
// with a comment here when they become permanent.
constexpr uint32_t SF_CREATED = 1u << 0;        // the character creator was finished for this save
constexpr uint32_t SF_FIRST_WEAPON = 1u << 1;   // the opening's guaranteed first weapon has been handed out

struct Input {
  Vec2 move;
  bool attack = false, bow = false, spell = false, roll = false, interact = false, potion = false, swapSpell = false;
};

// Down: a skeleton that has collapsed and may reassemble (a hit while it is down finishes it)
enum class AState : uint8_t { Idle, Walk, Windup, Strike, Recover, Roll, Hurt, Dead, Cast, Down };

enum class Spell : uint8_t { Flames, Heal, IceSpike, COUNT };
const char* spellName(Spell s);
int spellCost(Spell s);

struct Actor {
  int id = 0;
  bool player = false, npc = false, hostile = false, human = false, boss = false, ranged = false, flying = false;
  Faction faction = Faction::Town;   // who it fights (factions.h); `hostile` stays == factionsHostile(faction, Player)
  art::Monster mon = art::Monster::Wolf;
  art::HumanLook look;
  std::string name;
  Role role = Role::Villager;
  int level = 1;
  Vec2 p, vel, knock, aim{0, 1};
  int face = 0;            // 0 down, 1 up, 2 right, 3 left
  float animT = 0;
  float hp = 10, maxHp = 10, dmg = 1, speed = 40, armor = 0, radius = 5;
  float range = 14, aggroR = 100, windup = 0.3f;
  AState st = AState::Idle;
  float stT = 0;
  float atkCd = 0, shootCd = 0, flash = 0, burnT = 0, slowT = 0, regen = 0, iframes = 0;
  int combo = 0;
  float comboT = 0;
  bool hitDone = false;
  int xp = 0;              // reward
  // AI
  int target = -1;
  Vec2 home, goal;
  float thinkT = 0;
  bool aggro = false;
  int site = -1, slot = -1, bldg = -1;
  bool fromMap = false;    // created from a map Spawn (persistent identity)
  bool wild = false;       // dynamic wilderness spawn
  bool fly = false;        // dragon in the air
  float special = 0;       // ability timer
  bool dropped = false;
  // behaviour sets (game feel)
  bool heavy = false;      // the current windup is a heavy, roll-through attack (bears, trolls)
  bool lunge = false;      // the current strike is a wolf lunge (hits on contact while it travels)
  bool fleeing = false;    // goblins run at low health
  bool reassembled = false;   // skeletons get back up at most once
  int den = -1;            // World::dens index this pack member belongs to
  int atkN = 0;            // attacks made (heavy-attack cadence)
  int8_t orbitDir = 1;     // wolves: circling direction
  float lastHitT = -99;    // Game::time this actor last took damage (troll regen pauses)
  float plHitT = -99;      // (M4 integration) Game::time the player last struck it: a hunt beast the guards finish counts
  // town defence (M0): townsfolk run home and hide, brave ones with a tool fight as militia, guards converge
  bool militia = false;    // a brave adult with a tool (smith, farmer...): fights weakly when monsters come
  bool stallKeeper = false; // (M1 economy) keeps a market stall: stands behind its counter facing the customers
  bool indoors = false;    // reached its home door this frame: Game moves it indoors (out of `actors`) until it is safe
  int homeBldg = -1;       // overworld building it shelters in (-1 not chosen yet, -2 none)
  float fleeT = 0;         // seconds spent running for home during this threat
  int navGoal = -1, navNext = -1;   // tile path-finding: goal tile index and the next tile on the way
  float navT = 0;          // time until the path is re-planned
  int unreach = -1;        // guards: a target no path reaches (across a wall, in water) is ignored for unreachT s
  float unreachT = 0;
  // M1 NPC level of detail (VISION_PLAN 4.4): townsfolk well off screen with nothing to react to sleep (their AI runs
  // a few times a second with the gathered time instead of every step)
  bool asleep = false;
  float lodAcc = 0;
  // M2 (SIM lane, never saved): the quest this actor belongs to (Quest::id; 0 none): a missing person to lead out, a
  // beast of a farm raid, a named bandit chief. nightHome: a square-goer gone home for the night (sheltered until dawn)
  int quest = 0;
  bool nightHome = false;
  // M4 Banners (never saved): the kingdom this guard, soldier, captain or herald serves (0: none, a frontier militia or
  // a civilian). Who fights whom between kingdoms is Game::warHostile (war_game.cpp), from this and the realm's wars.
  ew::Gid realm = 0;
  // M5 Hearth and Hall (never saved). resident: the census index (life::Census::res) this actor embodies within its
  // site `site` (-1: not a resident: a wayside person, a soldier, a monster). posture: what the body is doing (art::Posture;
  // None = the standing sheet) and postureT its animation clock; useX / useY the furniture tile it is using (a chair, a
  // bed, an anvil: no two actors take the same one; -1 none). bubble: the speech icon over its head and how long it
  // stays (art::Bubble; a chat pair, a greeting, a bark). critter: a village animal (art::Critter + 1; 0 a person or a
  // monster): npc, not human, Faction::Town, drawn from art::critterSheet(kind, critterVar). lifeBits / lifeT / lifeA /
  // lifeB: per-actor scratch whose meaning the TOWNSFOLK lane defines in life_game.cpp (route steps, chat partner,
  // the seat reserved...).
  int resident = -1;
  // a keyed resident's other quest identity (its street body's (bldg -1, LIFE_SLOT0 + idx) when this is the building's
  // slot-0 keeper, the keeper's (building, slot) when this is the street body; altSlot -1: none): isGiver matches either
  int altBldg = -2, altSlot = -1;
  art::Posture posture = art::Posture::None;
  float postureT = 0;
  int useX = -1, useY = -1;
  art::Bubble bubble = art::Bubble::None;
  float bubbleT = 0;
  uint8_t critter = 0;
  uint32_t critterVar = 0;
  uint32_t lifeBits = 0;
  float lifeT = 0;
  int lifeA = -1, lifeB = -1;
  // M6 Steel (never saved; rpg/sim/foes.h, FOES lane fills them in foeSpawned). rank: foes::Rank; affixes: foes::Affix
  // bits; overlays / bodyTint / scalePct: its art::MonsterLook (variant overlays, recolour, draw scale; the view draws
  // monsterSheetLook and the aura from these); bossPhase: dungeon-boss phases passed (0, 1 at 66 %, 2 at 33 %);
  // affixT / affixT2: affix timers (summoner, blinking, burning trail...); unique: a named unique's or world boss's id
  // (0 none); pack: champion pack id (members share affixes; 0 none); culture: the cult::CultureId whose arms it wears
  // and whose make its loot is (soldiers, guards, bandits; 0 the land's own); dropD: the danger its loot rolls at (0:
  // its level)
  uint8_t rank = 0;
  uint16_t affixes = 0;
  uint16_t overlays = 0;
  uint32_t bodyTint = 0;
  uint8_t scalePct = 0;
  uint8_t bossPhase = 0;
  float affixT = 0, affixT2 = 0;
  ew::Gid unique = 0;
  int pack = 0;
  uint64_t culture = 0;
  int dropD = 0;
};

enum class ProjKind : uint8_t { Arrow, Fireball, IceSpike, Spit, Magic, DragonFire };
struct Projectile {
  Vec2 p, v;
  float life = 1.5f, dmg = 5, radius = 3;
  int owner = -1;
  bool fromPlayer = false;
  ProjKind kind = ProjKind::Arrow;
  Ench ench = Ench::None;
  float enchPow = 0;
  Faction fac = Faction::Player;   // who fired it: it hits whatever that faction is hostile to (factionsHostile)
};

struct Pickup {
  Vec2 p;
  Item item;
  int gold = 0;
  float t = 0;
  bool magnet = false;     // already flying to the player (sound played)
  bool full = false;       // (M6 fixer r2) a potion left lying because the belt is full: no magnet until there is room
  bool told = false;       // the "no more than 10 potions" notice was given for this approach (reset when walked away)
};

// WindowShift (M1): the endless world's Active Window moved; p is the pixel offset every overworld position just got
// (the view moves its camera, particles and floating texts by it)
// Discover: a = the site, s = its name, f = 1 a place found, 2 a WONDER found (M2: the view may make more of it; the
// sim also gives 100 XP and a journal toast "WONDER FOUND: <NAME>")
// M4 Banners: News, the player heard of a world event (a = the realm::WorldEvent id, s = its line: the view shows a
// toast and the News journal gains it); Border, the player crossed into another kingdom's land (s = the kingdom's
// name, a = its banner colour (rgba), f = 1 entering a kingdom, 0 leaving into the wildlands: the view's herald banner
// "ENTERING THE KHAGANATE OF ...")
enum class Ev : uint8_t { Sfx, Hit, Blood, Explode, Sparkle, Dust, Heal, Frost, Text, Discover, LevelUp, QuestUpdate, Shake, MapChange, Notice,
                          WindowShift, News, Border };
struct Event {
  Ev type = Ev::Sfx;
  Vec2 p;
  int a = 0;          // Sfx id / colour / amount
  float f = 1;        // pitch / magnitude
  std::string s;
  float vol = 1;      // Sfx volume
};

// M2 (VISION_PLAN M2 sim lane, PLAN.md task 6) appends: Deliver (a parcel to an NPC in another settlement), Heirloom
// (retrieve a named item from a named chest in a named dungeon), Missing (find a person alive in a cave and walk them
// out), NamedBandit (a unique chief with a title), Protect (a night wave at a settlement's farm). Saved as u8: append.
// M4 Banners appends: Story (a running story quest or campaign stage mirrored from the story engine, rpg/story/story.h:
// Quest::stage is the engine's Instance id), War (a siege quest, "BREAK THE SIEGE" / "JOIN THE ASSAULT", WARDS lane:
// Quest::targetId the besieged site, Quest::stage the realm's Siege id, Quest::flags bit QF_WAR_ATTACK the side).
// M5 Hearth and Hall appends: Supply (a radiant quest from an unmet need, 15.12 "the baker has no flour": bring `need`
// units of a good (Quest::stage = ew::Good) to the giver or the settlement's stores; CITIZENS lane, life_quests.cpp).
enum class QType : uint8_t { Main, Clear, Hunt, Retrieve, Bounty, Deliver, Heirloom, Missing, NamedBandit, Protect, Story, War, Supply, COUNT };
enum class QState : uint8_t { Active, Complete, Done };
struct Quest {
  int id = 0;
  QType type = QType::Clear;
  QState state = QState::Active;
  std::string title, desc, giverName;
  int giverSite = -1, giverBldg = -1, giverSlot = -1;   // NPC identity
  int target = -1;              // site
  art::Monster mon = art::Monster::Wolf;
  int need = 1, have = 0;
  int gold = 0, xp = 0;
  int stage = 0;                // main quest; M2 multi-step quests: their step
  // ---- M2 (SAVE_VER 6): targets anchored in the endless world, valid without any loaded handle (VISION_PLAN 2.11)
  ew::Gid targetId = 0;         // what the objective is by stable id: a site, a building (Deliver's recipient), a den
  bool hasPos = false;          // tgx, tgy hold the objective's GLOBAL tile (markers, walking time, the map)
  int32_t tgx = 0, tgy = 0;
  ew::Gid giverId = 0;          // the giver's site id (with giverBldg / giverSlot: the NPC)
  std::string subject;          // who or what it is about: "ASTRID", "GRANDMOTHER'S SILVER RING", "TORVALD THE RED"
  int destBldg = -1;            // Deliver: the recipient's building (handle; saved by id)
  int deadlineDay = -1;         // Protect: the day of the night attack (-1 none)
  uint32_t flags = 0;           // per-type progress bits (QF_*: defined by the sim lane next to the code that sets them)
};
// Quest::flags (M2 SIM lane, rpg/sim/quests.cpp). Bits 8-15 hold a Deliver's recipient Role; bits 16-31 are free.
constexpr uint32_t QF_DANGER = 1u << 0;   // offered as "DANGEROUS COUNTRY" (the target is beyond the player: +50 % reward)
constexpr uint32_t QF_PARCEL = 1u << 1;   // Deliver: the parcel was handed over at the accept (it is in the pack)
constexpr uint32_t QF_FOUND = 1u << 2;    // Missing: found and following / Heirloom: the chest was opened
constexpr uint32_t QF_WAVE1 = 1u << 3;    // Protect: the first wave came
constexpr uint32_t QF_WAVE2 = 1u << 4;    // Protect: the second wave came
constexpr uint32_t QF_FAILED = 1u << 5;   // the quest ended without success (state Done, no reward)
constexpr uint32_t QF_GRAVE = 1u << 6;    // Heirloom read on a lone grave: the item is laid on the grave (giverSlot -2)
constexpr uint32_t QF_ESCORTED = 1u << 7; // Missing: brought out alive (complete; the reward waits with the giver)
constexpr uint32_t QF_WAR_ATTACK = 1u << 16;   // (M4) War: with the attackers ("JOIN THE ASSAULT"); clear: the defenders
constexpr uint32_t QF_WAR_CAPDEAD = 1u << 17;  // (M4) War, BREAK THE SIEGE: the besiegers' captain fell (saved with the quest)
inline Role questRecipientRole(const Quest& q) { return (Role)((q.flags >> 8) & 0xFF); }

struct DlgOpt { std::string label; int action = 0; int arg = 0; };
// M4: dialogue action ranges the lanes' own files handle (Game::dialogueChoose routes them): the story engine's
// options (rpg/story/story_game.cpp storyChoose) and the war's (rpg/sim/war_game.cpp warChoose). The built-in actions
// (game_internal.h DlgAct) stay below DLG_STORY.
constexpr int DLG_STORY = 1000, DLG_WAR = 2000, DLG_END = 3000;
// M5: the townsfolk's options (feed, employ, supply, befriend, the inn's meal; rpg/sim/life_game.cpp lifeChoose)
constexpr int DLG_LIFE = 3000, DLG_LIFE_END = 4000;
// M6 Steel: the foes' talk (innkeepers' warnings of named uniques and world bosses; rpg/sim/foes_game.cpp foeChoose) and
// the forge's (smelting, smithing, tanning, apprenticeship, learning secrets; rpg/sim/craft_game.cpp craftChoose)
constexpr int DLG_FOES = 4000, DLG_CRAFT = 4500, DLG_M6_END = 5000;
struct Dialogue {
  int actor = -1;
  std::string speaker, text;
  std::vector<DlgOpt> opts;
  Role role = Role::Villager;
};

// M0b: the inn room the player has rented (SAVE_VER 4). It is theirs until noon of untilDay: its bed can be slept in,
// and the innkeeper sends them to it. room indexes the Map::rooms of that building's floor `floor` (a GuestRoom).
struct Lodging {
  int bldg = -1, floor = 0, room = -1;
  int untilDay = -1;
};

// M2 travel behind the fade (owner carry-over: the ~180 ms fast-travel / respawn hitch, worse on iPhone web). A journey
// started by beginTravel goes dark first (sleepFade held at 1), gathers the destination window's chunks across frames
// while the screen is black (Gather: the streamer's worker, or the web pump with a larger budget), then moves the
// window and the player in one cheap step (Arrive). The view bakes the terrain the arrival shows (budgeted, behind the
// same black) and then calls finishTravel(), which fades back in. Headless runs never call finishTravel: Arrive ends by
// itself after a short while, so tests can drive beginTravel through update() alone.
enum class TravelPhase : uint8_t { None, Gather, Arrive };
struct Travel {
  TravelPhase phase = TravelPhase::None;
  int site = -1;                // destination site handle (-1: a tile, gx / gy)
  int32_t gx = 0, gy = 0;       // destination GLOBAL tile (where the player will stand)
  uint8_t kind = 0;             // 0 fast travel on foot, 1 carriage (paid), 2 respawn after death
  float t = 0;                  // seconds in this phase
  float hours = 0;              // journey time (the clock moves by it on arrival)
  int gold = 0;                 // what it cost
  // M2 SIM lane (rpg/sim/travel.cpp): the destination window and what the journey cost the frame loop
  int32_t nox = 0, noy = 0;     // the window origin at the destination (global tile)
  bool move = false;            // the window moves (false: the destination already lies in the window's middle)
  double wall0 = 0;             // wall-clock ms when the journey began
  double gatherReal = 0;        // (M2 fixer round 2) real ms spent gathering while the game ran (the 2 s cap): a
                                // pause or an app switch stops it (each step adds at most 250 ms)
  double lastStepMs = 0;        // wall-clock ms of the last travel step
  int steps = 0;                // update steps spent travelling
  int syncChunks = 0;           // chunks the arrival generated on the main thread (0: all came from the streamer)
  double worstStepMs = 0;       // the slowest travel step (update + its streaming work)
  double arriveMs = 0;          // the arrival step (moving the window, placing the player)
  double gatherMs = 0;          // real time from the start to the arrival
  bool viewed = false;          // a view drives the arrival (View::travelArrive calls finishTravel once its picture is ready)
};
// what a journey would take (the map shows it before the player commits; VISION_PLAN 2.11)
struct TravelQuote {
  bool ok = false;
  std::string why;              // why not ("YOU CANNOT TRAVEL WITH ENEMIES NEARBY", "ACROSS THE SEA"...)
  float hours = 0;              // in-game hours on the road
  int gold = 0;                 // carriage fare (0 on foot)
};

struct ShopState {
  int actor = -1;
  uint64_t key = 0;        // merchant identity (npcKey)
  std::vector<Item> stock;
  std::string title;
};

class Game {
 public:
  explicit Game(uint64_t seed = 1);
  void newEndlessGame(uint64_t seed);   // a new game on the endless mainland (VISION_PLAN 15.6; M2: the only kind)
  void update(float dt, const Input& in);

  // --- world & level
  World world;
  Map sub;                 // current cave/ruin/interior (when inside)
  bool inside = false;
  int subSite = -1, subBldg = -1;
  int subFloor = 0;        // M0b: which floor of building subBldg (0 ground; SAVE_VER 4)
  Map& map() { return inside ? sub : world.over; }
  const Map& map() const { return inside ? sub : world.over; }
  // identity of the current map (looted chests, killed spawns, terrain caches). Upper floors get their own keys.
  int mapKey() const {
    if (!inside) return 0;
    if (subBldg >= 0) return subFloor > 0 ? 10000000 + subBldg * 16 + subFloor : 100000 + subBldg;
    return 1 + subSite;
  }

  // --- state
  Mode mode = Mode::Title;
  uint64_t seed = 1;
  float time = 0;          // sim seconds since start
  float hour = 8.0f;       // 0..24 time of day
  int day = 1;
  std::vector<Actor> actors;   // actors[0] is always the player
  std::vector<Projectile> projs;
  std::vector<Pickup> pickups;
  std::vector<Event> events;
  Actor& pl() { return actors[0]; }
  const Actor& pl() const { return actors[0]; }

  // player progression
  int plLevel = 1, plXp = 0, gold = 25, perkPts = 0;
  float mp = 60, maxMp = 60, stamina = 80, maxSt = 80;
  float baseHp = 100;
  std::vector<Item> inv;
  int eqWeapon = -1, eqBow = -1, eqStaff = -1, eqArmor = -1, eqHelmet = -1, eqShield = -1, eqRing = -1, eqAmulet = -1;
  int eqGloves = -1, eqBoots = -1, eqCloak = -1;   // SAVE_VER 3
  int* equipSlot(ItemKind k);                       // the eq* index for an equippable kind (nullptr otherwise)
  // everything worn (not wielded): enchantment bonuses come from these
  std::array<int, 8> worn() const { return {eqArmor, eqHelmet, eqShield, eqRing, eqAmulet, eqGloves, eqBoots, eqCloak}; }
  // character (SAVE_VER 3): chosen in the creator; pre-M0 saves load with the defaults (created == false)
  Appearance app;
  Background background = Background::None;
  uint32_t storyFlags = 0;                          // SF_* bits
  bool hasFlag(uint32_t f) const { return (storyFlags & f) != 0; }
  uint8_t spellsKnown = 1;     // bit per Spell
  Spell spell = Spell::Flames;
  int kills = 0, dungeonsCleared = 0;
  float blessT = 0;            // shrine blessing time left
  std::string blessName;
  int lastTown = -1;           // respawn point
  Lodging lodging;             // M0b: the rented inn room (SAVE_VER 4)
  bool lodgingActive() const { return lodging.bldg >= 0 && (day < lodging.untilDay || (day == lodging.untilDay && hour < 12.0f)); }
  float sleepFade = 0;         // view: fade-out when resting/travelling
  float hitStop = 0;           // brief freeze on heavy hits (game feel)
  float slowMo = 0;            // perfect-roll / level-up slow motion left (real seconds; sim runs at 30 %)
  float stFlash = 0;           // view: flash the stamina bar (tried to act with too little stamina)
  float lastHurtT = -99;       // time the player last took damage (out-of-combat regen)

  // quests
  std::vector<Quest> quests;
  int nextQuestId = 1;
  int trackedQuest = -1;
  std::map<uint64_t, int> npcQuestsDone;     // npc key -> completed count (new offers)
  std::set<uint64_t> looted;                 // mapKey<<32 | tile, or lootKey's global key (endless overworld)
  ExploredMask explored;                     // M1: fog of war for the world map (global tiles the player has seen)
  std::map<int, std::set<int>> killedSlots;  // mapKey -> spawn slots defeated (no respawn)
  // M2 (SAVE_VER 6): per-world facts keyed by stable ids, for the wayside places and the quests the sim lane adds (the
  // day a vignette's standing stones were last used, a toll paid, a grave read, a rumour bought...). The sim lane
  // defines its key spaces next to the code (e.g. a site Gid xor a small tag).
  std::map<uint64_t, int32_t> marks;

  // UI-facing state
  Dialogue dlg;
  ShopState shop;
  std::string locName;     // current region/site label
  int curSite = -1;
  float noticeT = 0;
  std::string notice;

  // --- actions called by the UI
  void startPlay() { mode = Mode::Play; }
  void beginCreator() { mode = Mode::Creator; }   // after newEndlessGame(): the UI edits app/background, then...
  void finishCreator();                            // ...applies the background's trait, rebuilds the look, starts play
  bool rewardWaiting(const Actor& npc) const;      // a completed quest's reward waits with this NPC (world "!" marker)
  bool offersWork(const Actor& npc) const { return hasOffer(npc); }   // M2: this NPC has a job to offer (scripts, tests)
  std::string questStatus(const Quest& q) const;   // one journal line: what to do next ("" = nothing extra)
  void dialogueChoose(int optIndex);
  void closeDialogue();
  bool buy(int stockIndex);
  bool sell(int invIndex);
  void useItem(int invIndex);          // equip / consume
  void dropItem(int invIndex);
  void chooseLevelUp(int stat);        // 0 health 1 magicka 2 stamina
  bool fastTravel(int site);           // immediate (scripts, tests); the UI uses beginTravel
  // M2 travel behind the fade (Travel above). beginTravel: false (with a notice) when the journey cannot be made.
  bool beginTravel(int site, bool carriage = false);
  // (M3c carry) a teleport behind the fade: the same journey to a GLOBAL tile (no hours, no fare, no site rules)
  bool beginTravelTo(int32_t gx, int32_t gy);
  void finishTravel();                 // the view: the arrival's terrain is baked, fade back in
  bool travelling() const { return travel.phase != TravelPhase::None; }
  TravelQuote travelQuote(int site, bool carriage = false) const;
  Travel travel;
  Travel lastTravel;                   // M2: the last journey that arrived (its costs: scripts, tests, --perf reports)
  // ---- M4 Banners: the living world (rpg/sim/realm.h, REALM lane), the war's actors and overlays (rpg/sim/war.h, WARDS
  //      lane), the story engine (rpg/story/story.h, STORY lane). realm and story are saved (their own blocks); war is
  //      regenerated from the realm (never saved).
  realm::Realm realm;
  story::Engine story;
  WarState war;
  // ---- M5 Hearth and Hall (rpg/sim/life.h): the census, needs, plans, moods and the player's buffs (CITIZENS lane;
  //      saved: the life block), and the townsfolk's actors (rpg/sim/life_game.cpp, TOWNSFOLK lane)
  life::Life life;
  // ---- M6 Steel: the player's smithing skill, learned culture secrets and apprenticeship trust (rpg/sim/craft.h; NUMBERS
  //      lane; saved: the craft block, SAVE_VER 12)
  craft::Knowledge craft;
  craft::Bench bench;          // the forge screen's state (Mode::Forge; UI-facing, never saved)
  // apply the realm to the loaded world now (owners and banners of every loaded settlement, the news): scripts and tests
  // call it after forcing a realm change; realmStep calls it whenever the realm changed (realm_game.cpp)
  void realmSync();
  // every loaded settlement not yet known to the realm noted (true: one the realm holds for another owner). realmStep
  // calls it each step (fixer M4 r1: factored out; a load calls realmSync alone before re-entering a building)
  bool realmNoteSites();
  // two actors on opposite sides of a war between kingdoms (enemy soldiers, an enemy town's guards; war_game.cpp).
  // Everything else about hostility stays factionsHostile.
  bool warHostile(const Actor& a, const Actor& b) const;
  // the kingdom whose camp a war prop at overworld tile (tx, ty) belongs to (0: none / not a war prop): the view draws
  // it in that kingdom's colours (art::warPropSprite). war_game.cpp.
  ew::Gid warPropKingdom(int tx, int ty) const;
  // M2 SIM lane: a carriage can be hired here (the player stands in a town or a city, or in one of its buildings)
  bool carriageHere() const;
  // M2 quests (rpg/sim/quests.cpp). offerFor: the offer this NPC would make when asked for work, of a given type
  // (QType::COUNT: whatever they would offer); `ok` false when they have none of that kind. Scripts and tests use it
  // with debugAccept to drive every quest type from code.
  Quest offerFor(const Actor& npc, QType want, bool& ok);
  void debugAccept(const Quest& q) { acceptQuest(q); }
  bool questChestAt(int qid, int& tx, int& ty) const;   // inside: the tile of an Heirloom quest's chest (scripts)
  bool debugOfferTalk(int actorId, QType t);   // scripts: this NPC offers a job of that type as a dialogue (accept / decline)
  // M2 rumours (rpg/sim/wayside.cpp): mark the nearest unknown dungeon, wayside place or wonder as rumoured and return
  // the line that tells of it ("" when there is nothing left to hear of)
  std::string hearRumour();
  // M2 wayside: the toll a troll bridge asks (10-25 gold), and whether the player may cross it today
  int tollOf(int site) const;
  bool tollPaid(int site) const;
  void respawn();
  int interactTarget() const;          // actor id the player would talk to (-1 none)
  bool foeInReach() const;             // (fixer M4 r3) an awake foe within striking reach: the use button attacks
  int interactProp(int& tx, int& ty) const;   // usable prop in front of the player (art::Prop + 1, 0 none)
  bool bedIsYours(int tx, int ty) const;      // M0b: a bed you may sleep in (an inn's beds are let room by room)
  bool nearDoorOrExit() const;
  float armorRating() const;
  float weaponDamage() const;
  // first level in ~4 min, level 5 in ~30 (PLAN.md targets; measured by rpg_test --metrics)
  int xpForNext() const { return gear::xpForNext(plLevel); }   // (M6) the tuned curve (rpg/sim/gear.cpp)
  const Quest* questById(int id) const;
  bool questTarget(int qid, int& tx, int& ty) const;   // overworld tile of the tracked quest's objective
  uint64_t npcKey(const Actor& a) const;
  bool isNight() const { return hour < 5.5f || hour > 20.0f; }
  float daylight() const;              // 0 night .. 1 noon
  // town defence (M0): the settlement whose bell is ringing (-1 none). The view plays combat music while it rings.
  int alarmSite = -1;
  int settlementAt(Vec2 p) const;      // the city/town/village whose footprint holds this overworld point (-1 none)
  int shelteredCount(int site = -1) const;   // townsfolk hiding indoors from a threat (site -1: all)
  // background traits (VISION_PLAN 15.1), applied where they act; small and visible in dialogue, shops and stats
  float blessingSecs() const;          // how long a shrine or temple blessing lasts
  float priceFactor(Role seller) const;   // what this background pays at a seller (1 = list price)
  int openingQuest() const;            // id of the opening quest "A BLADE OF YOUR OWN" while it is open (-1 otherwise)

  // persistence. Only the current SAVE_VER loads (owner, 2026-10-04: old saves are not a concern); an older save is
  // refused and the title says so (saveVersion tells the UI which case it is). M2: SAVE_VER 6.
  void serialize(std::vector<uint8_t>& out) const;
  bool deserialize(const std::vector<uint8_t>& in);
  static int saveVersion(const std::vector<uint8_t>& in);   // the file's SAVE_VER (0: not a save at all)
  static int currentSaveVersion();
  // a current-format save whose world came from an older generator (ew::ENDLESS_GEN_VER):
  // refused like an older SAVE_VER, and the title words it the same way
  static bool saveFromOlderGenerator(const std::vector<uint8_t>& in);
  // M1 stable keys: an overworld chest (endless worlds key it by global tile, so it stays looted wherever the window is)
  uint64_t lootKey(int tx, int ty) const;   // (endless: by global tile)
  uint64_t npcKeyOf(int site, int bldg, int slot) const;   // npcKey's formula from handles (quest givers)
  // (M5) resident `idx` of site `si` gave the player an open quest: it never emigrates or is carried off (life.cpp,
  // life_raids.cpp), so the quest can always be handed in
  bool lifeGiverOpen(int si, int idx) const;

  // M1 endless streaming and NPC level of detail (SIM lane)
  void frameWork(double budgetMs);     // once per rendered frame: the streamer's work (web: generation within the budget)
  // (M3c carry) the interior made ahead for the building the player is walking up to (null: none ready); `bldg` is
  // its index in world.over.bldgs, so a view can bake its terrain and paint its pieces before the door is reached
  // (its map key is 100000 + bldg, as mapKey() will say once inside)
  const Map* preparedInterior(int& bldg) const;
  int prepBldg_ = -1;   // the building prep_'s ready map belongs to (index in world.over.bldgs)
  int prepHits = 0, prepMisses = 0;   // entries that found their interior made ahead / had to make it (tests, perf)
  // (M3c carry) the frame probe: the longest wall-clock gap between two frameWork calls (a whole rendered frame: the
  // update, the draw and the present) since the last reset (scripts' `perfmark`), and the frame it happened in
  double frameGapWorstMs = 0;
  double frameGapLastMs_ = 0;
  struct PerfCounters {
    int npcAwake = 0, npcAsleep = 0;   // townsfolk in `actors` this step, by LOD
    int hostiles = 0;                  // monsters and bandits in `actors`
    int activeSites = 0;               // settlements and camps whose people are streamed in
    int spawnedNpcs = 0, despawnedNpcs = 0;   // people streamed in / out so far
    // (M6 fixer) the worst ms of each world subsystem's step since --perf last reset them (once a second): the
    // systems a hitch can hide in (names: perfSysName)
    enum Sys { S_QUEST, S_REALM, S_WAR, S_STORY, S_LIFESTEP, S_RAID, S_FOE, S_LIFETICK, S_SPAWN, S_COUNT };
    double sysWorstMs[S_COUNT] = {};
  };
  PerfCounters perf;
  static constexpr int FOLK_CAP = 80;          // townsfolk streamed in at once (nearest first)
  static constexpr int FOLK_IN = 34, FOLK_OUT = 44;   // tiles: a person streams in within FOLK_IN, out beyond FOLK_OUT
  static constexpr int SITE_IN = 26, SITE_OUT = 40;   // tiles from a site's area: it activates / deactivates
  // half the view in tiles plus a margin (the platform layer sets it from the logical canvas: the VIEW lane's screen
  // fit makes it wider on phones); townsfolk beyond it may sleep
  float sleepHalfW = 22.0f, sleepHalfH = 13.0f;
  // M5 TOWNSFOLK lane (rpg/sim/life_game.cpp): what the townsfolk's actors are doing (tests, scripts, --perf). Counts are
  // of the actors in play this step; the ms figures time lifeStep plus every resident's lifeFolk (the 15.12 budget:
  // 120 residents <= 1.5 ms per step on average)
  struct LifeFolkStats {
    int residents = 0;        // resident actors in play (Actor::resident >= 0)
    int walking = 0, seated = 0, working = 0, sleeping = 0, critters = 0;
    int spawned = 0, despawned = 0;          // resident actors brought in / put away so far
    int pathRequests = 0, pathCacheHits = 0, pathFails = 0;
    int stuck = 0;            // walkers that moved less than a tile in the last 30 s while their plan moved them
    int steps = 0;
    double lastMs = 0, worstMs = 0, sumMs = 0;
  };
  const LifeFolkStats& lifeFolkStats() const;
  int lifeFolkCap = 0;        // tests: resident actors a settlement may put on the street at once (0: FOLK_CAP)

  // test helpers
  bool godMode = false;
  bool noWildSpawns = false;           // metrics arenas: no roaming spawns or dens
  bool streamThreads = true;           // false: stream as the web build does (no worker; frameWork generates); tests
  void debugSpawn(art::Monster m, int n, float dist);
  int debugSpawnAt(art::Monster m, Vec2 at, int level);   // returns the actor id (already aggro)
  // (M6 fixer r2) a free spot with every tile within `rad` of it free too (a big body: a world boss is not stood on a wall)
  Vec2 freeSpotClear(int tx, int ty, int rad) const;
  void debugFell(int actorId, bool byPlayer = false);   // tests: an actor falls as if a monster struck it down (no killer), or as the player's kill
  void debugKit();                     // the pre-M0 starting kit (iron sword, hunting bow, 20 arrows, 3 potions, bread),
                                       // equipped: for fight scripts and bots once the real start is shirt-only
  // M0b: go into building bi (from anywhere, leaving the current sub-level) and up to floor f; false if f is not one
  // of its floors. Scripts, tests and save loading use it; play goes through doors and stairs.
  bool debugEnterBuilding(int bi, int floor = 0);
  void debugLeave() { if (inside) leaveSub(); }   // step out of the building or site (tests)
  bool debugEnterSite(int si);         // M2: into a cave or ruin from anywhere (the window comes along; tests, scripts)
  // the dragon's lair by name (its peak is named per world: makeDungeonName)
  std::string lairName() const { return world.lair >= 0 && world.lair < (int)world.sites.size() ? world.sites[(size_t)world.lair].name : std::string("THE DRAGON'S PEAK"); }
  // M1: put the player on a GLOBAL tile (the window recentres there), outdoors, on the
  // nearest free tile. Scripts (`at X Y`), --at and tests use it.
  void teleportGlobal(int32_t gx, int32_t gy);
  void changeFloor(int floor);         // inside a building: move to another of its floors, arriving by its stairs
  bool stairsAsleep() const { return stairsArrive_ >= 0; }   // just climbed: the stairwell down sleeps against a push north

 private:
  int nextId_ = 1;
  float exploreT_ = 0;
  float spawnT_ = 0;
  bool exitArmed_ = false;
  bool pillarHit(float x, float y, float rx, float ry) const;   // (owner) a body against an open front's pillars
  int leaveCol_ = -1;          // (owner) the interior exit column walked out by (an open front's bays: the matching bay)
  bool stairsArmed_ = false;   // M0b: the player has stepped off the stairs they arrived by
  bool stairsLatch_ = false;   // M0b: just arrived by the stairs: they stay asleep until the stick is let go or the
                               // player has walked more than a tile and a half away (no floor ping-pong while held)
  Vec2 stairsFrom_;            // where the player arrived
  Vec2 stairsOff_;             // M0b: the player's last position off the stairs (the stairwell's railing turns them back)
  int stairsArrive_ = -1;      // M0b: the tile (y * w + x) the player arrived on by the stairs: the stairs stay asleep
                               // until the player has stood on some other tile off the steps (-1: none)
  bool stairsNorth_ = false;   // M0b: the stick was let go while standing on the stairs: pushing north takes them
  Quest pendingOffer_;
  int dragonId_ = -1;
  // what each merchant has left this restock period (every 2 days): buying must not refill the shelf
  std::map<uint64_t, std::pair<int, std::vector<Item>>> shopCache_;
  int findActor(int id) const;
  Vec2 freeSpot(int tx, int ty) const;
  void placePlayerAt(int tx, int ty);   // endless: recentres the window first when the tile is off its middle
  void maybeRecentre();                 // M1: keep the player in the window's middle (VISION_PLAN 2.9)
  void windowMoved(int dx, int dy);     // M1: the window moved by (dx, dy) tiles: translate everything overworld
  void reapplyLooted();                 // M1: open the looted overworld chests the window shows
  std::set<int> activeSites_;
  std::map<int, std::set<int>> felled_;   // site -> townsfolk slots felled while it is active (no respawn until it reloads)
  std::set<int> activeDens_;
  float denT_ = 0;
  bool perfectRoll_ = false;   // this roll already earned its slow-mo blip
  void updateDens(int ptx, int pty);
  int denClearedDay(int den) const;    // -1 = not cleared
  void heavySlam(Actor& a);
  // town defence (M0, ai.cpp)
  struct SiteAlarm { float lastThreatT = -99, bellT = 0, quietT = 0; bool ringing = false; int hostiles = 0; };
  std::map<int, SiteAlarm> alarms_;    // per active settlement (derived each frame, never saved)
  std::vector<Actor> sheltered_;       // townsfolk hiding indoors; they come back out ~30 s after the threat ends
  std::vector<int> navPrev_;           // path-finding scratch
  void updateTownDefence(float dt);
  int pickTarget(const Actor& a) const;            // hostiles: the actors index to fight (0 = the player)
  bool navStep(Actor& a, Vec2 goal, float speed, float dt);   // walk toward goal around buildings; false = no path
  int homeDoor(Actor& a);              // the building a townsperson shelters in (chosen once), -1 none
  void updateFolk(Actor& a, float dt);   // friendly NPC behaviour (guards, militia, fleeing, wandering)
  void giveFirstWeapon(Quest& q);      // the opening: the start village innkeeper hands over the old blade
  Rng rng_;
  void emit(Ev t, Vec2 p, int a = 0, float f = 1, const std::string& s = "") { events.push_back({t, p, a, f, s}); }
  void sfx(int s, Vec2 p, float pitch = 1, float vol = 1);
  void say(const std::string& s);

  void resetPlayer();
  void beginWorld();   // newGame / newEndlessGame: the player, the opening quests, the first actors
  void resetSession(); // beginWorld's first half: every per-game state back to a new game's, the player alone
  void recalcPlayer();
  void updatePlayer(float dt, const Input& in);
  void updateActor(Actor& a, float dt);
  void updateAI(Actor& a, float dt);
  void updateProjectiles(float dt);
  void updatePickups(float dt);
  void updateSpawning(float dt);
  // M1 (SIM lane): streaming and LOD
  float prefetchT_ = 0;
  Vec2 prefetchFrom_;                  // the player's position at the last wish-list update (heading)
  void prefetchTick(float dt);         // keep the streamer's wish list current (creates the streamer on first use)
  float siteScanT_ = 0;
  void streamSitePeople(int si);       // spawn the site's people near the player, put the far calm ones away
  std::vector<int> hostiles_;          // actors indices that may threaten townsfolk (rebuilt each step)
  void collectHostiles();
  bool sleepy(const Actor& a) const;   // NPC LOD: far off screen with nothing to react to
  void updateLocation();
  void moveActor(Actor& a, Vec2 delta);
  bool solidAt(float x, float y, bool flying) const;
  bool bodyFree(Vec2 p, float r, bool flying) const;   // an actor of radius r fits here (moveActor's box)
  void meleeHit(Actor& a);
  void damage(Actor& victim, float dmg, Vec2 from, int attacker, Ench ench = Ench::None, float enchPow = 0, bool crit = false);
  void kill(Actor& a, int killer);
  void gainXp(int xp);
  void addItem(const Item& it, bool announce = true);
  void giveGold(int g);
  void dropLoot(const Actor& a);
  void openChest(int tx, int ty);
  void interact();
  void talkTo(Actor& a);
  void openShop(Actor& a);
  void rest(int hours);
  void shootArrow();
  void castSpell();
  void enterSite(int site);
  void enterBuilding(int bldg, int col = -1);   // col: the front-row column walked in by (an open front's bay)
  void leaveSub();
  void loadMapActors();
  void clearNonPlayer();
  int spawnMonster(art::Monster m, Vec2 p, int level, bool boss);
  int spawnHuman(const Spawn& sp, Vec2 p);
  void makeLook(Actor& a, Role r, Rng& rr);
  void applyLevel(Actor& a, int level);
  void questKill(const Actor& a);
  void checkDungeonCleared(int site);
  Quest makeOffer(const Actor& npc);
  bool hasOffer(const Actor& npc) const;
  void acceptQuest(const Quest& q);
  void completeQuest(Quest& q);
  void advanceMain(int stage);
  std::string greeting(const Actor& a);
  std::vector<Item> shopStock(const Actor& a);
  // ---- M2 SIM lane
  // travel behind the fade (travel.cpp)
  int frameWorkCalls_ = 0;             // frameWork ran this session (else update pumps a thread-less streamer itself)
  // (M3c carry, the seat-of-power entry hitch) the ground floor of the building the player is walking up to, made
  // ahead (prepInteriorTick: natively on a worker thread, on the web in a frame the streamer leaves idle) and used by
  // enterBuilding. genInterior depends on the building alone (never its window position), so the map is the one
  // entering would make, bit for bit; it is keyed by the building's id and floor and dropped with a new world.
  struct PrepInterior {
    uint64_t key = 0;         // interiorKey of the map held (0: none)
    bool ready = false;
    Map map;
  };
  PrepInterior prep_;
  float prepT_ = 0;
#ifndef __EMSCRIPTEN__
  std::shared_ptr<std::future<Map>> prepJob_;   // (shared: Game stays copyable)
  uint64_t prepJobKey_ = 0;
  int prepJobBldg_ = -1;
#endif
  static uint64_t interiorKey(const Bldg& b, int floor) { return (b.id ? b.id : ((uint64_t)b.seed << 20 | 0x5EA7ull)) * 31ull + (uint64_t)floor + 1; }
  void prepInteriorTick(float dt);
  bool takePreparedInterior(const Bldg& b, int floor, Map& out);   // false: nothing made ahead for it
  void travelStep(float dt);           // one update step of a journey (Gather / Arrive)
  void travelWant();                   // the streamer's wish list for the destination window
  bool travelReady();                  // every chunk and region plan the arrival needs is ready
  void travelArriveNow();              // move the window and the player (the Arrive step)
  void travelDest(const Site& s, int kind, int32_t& gx, int32_t& gy) const;   // the arrival tile (global)
  void arriveInOpen(const Site& s);    // settlements: the nearest open tile with a free step south
  int32_t landAt(int32_t gx, int32_t gy) const;   // the landmass id at (or near) a global tile
  bool startJourney(int site, int kind, float hours, int gold);
  void journeyWindow();                // the journey's destination window and wish list (startJourney, beginTravelTo)
  // ---- M4 hooks. Each is called from the core loop (game.cpp / game_rpg.cpp) and DEFINED in its lane's own file, so
  //      the lanes never edit each other's files. Keep the calls where phase A put them.
  //   REALM (rpg/sim/realm_game.cpp): once per update step; ticks the realm at a day change, focuses it on the player,
  //   registers the settlements the window loads, applies owners (World::setSiteOwner), emits Ev::Border on crossings
  void realmStep(float dt);
  uint32_t realmSeen_ = 0;             // the realm's eventSerial() last applied (realm_game.cpp)
  size_t realmSites_ = 0;              // world.sites already registered with the realm (sites is append-only)
  int realmDay_ = -1;                  // the day the realm last advanced to
  ew::Gid realmLand_ = 0;              // whose land the player stood on (Ev::Border when it changes)
  bool realmLandKnown_ = false;
  //   WARDS (rpg/sim/war_game.cpp): once per update step (siege camps, patrols, refugees, garrisons, charred overlays,
  //   guard strength); the war's dialogue (siege commanders, guards' war talk); a kill (siege contributions, reputation)
  void warStep(float dt);
  void warTalk(Actor& a);              // may add options (DLG_WAR..) and set dlg.text; called by talkTo before FAREWELL
  bool warChoose(const DlgOpt& o);     // an option in [DLG_WAR, DLG_END): true handled
  void warKill(const Actor& victim, int killer);
  //   STORY (rpg/story/story_game.cpp): story quests and campaigns, rumours and news, lore in ruins, notice boards,
  //   heralds
  void storyStep(float dt);
  void storyTalk(Actor& a);            // may add options (DLG_STORY..) and set dlg.text; called by talkTo before FAREWELL
  bool storyChoose(const DlgOpt& o);   // an option in [DLG_STORY, DLG_WAR): true handled
  void storyKill(const Actor& victim, bool byPlayer);
  void storyEntered(int site, int bldg);   // the player just entered a site's map (site >= 0) or a building (bldg >= 0)
  bool storyUseProp(art::Prop p, int tx, int ty);   // the player used a prop (a notice board, an inscription...): true handled
  // ---- M5 hooks (VISION_PLAN 10, 15.2, 15.12). Called from the core loop (game.cpp / game_rpg.cpp / ai.cpp) and
  //      DEFINED in the lane's own file. TOWNSFOLK (rpg/sim/life_game.cpp): the actors.
  //   once per update step (after storyStep): residents near the player streamed in / put away by their plan, routes,
  //   furniture, chat pairs, barks, the lamplighter, the animals
  void lifeStep(float dt);
  //   a friendly townsperson's behaviour this step (updateFolk, after the flight from danger and before the stroll):
  //   true = handled (its plan drove it)
  bool lifeFolk(Actor& a, float dt);
  //   streamSitePeople: may this site's generator spawn come out now (false: its resident is indoors / away this hour)
  bool lifeSpawnAllowed(int site, const Spawn& sp);
  //   a person just came into play from a generator spawn (streamSitePeople, loadMapActors): bind it to its resident
  //   (Actor::resident, name, look), place it where its plan has it
  void lifeSpawned(Actor& a, const Spawn& sp);
  //   loadMapActors finished a building interior: populate it by schedule (who is home, at the tables, asleep upstairs)
  void lifeInterior();
  //   talkTo: greetings by name, barks of need ("HAVEN'T EATEN SINCE YESTERDAY"), the player's hooks (feed, employ,
  //   supply, befriend) as options in [DLG_LIFE, DLG_LIFE_END); called before warTalk / storyTalk
  void lifeTalk(Actor& a);
  bool lifeChoose(const DlgOpt& o);    // an option in [DLG_LIFE, DLG_LIFE_END): true handled
  void lifeKill(const Actor& victim, int killer);   // a resident fell: grief, the census (Life::residentDied)
  //   the player used a prop (a bed or bedroll: sleep, Rested, skip the night; a table with food...): true handled.
  //   Called before the built-in prop handling.
  bool lifeUseProp(art::Prop p, int tx, int ty);
  void lifeAte(const Item& food);      // the player ate something (useItem): Well Fed (15.2)
  void lifeSlept(int hours, bool bed); // the player slept / rested (rest()): Rested when in a bed or bedroll (15.2)
  //   the player's buff multipliers (15.2), applied in updatePlayer / gainXp (1 = no effect)
  float lifeStaminaRegenMul() const;
  float lifeHealthRegenMul() const;
  float lifeXpMul() const;
  //   CITIZENS (rpg/sim/life_raids.cpp): night raids by monster pressure (VISION_PLAN 10.4 M5): once per update step
  void raidStep(float dt);
  //   CITIZENS (rpg/sim/life_quests.cpp): radiant quests from unmet needs ("THE BAKER HAS NO FLOUR"). hasOffer /
  //   makeOffer ask it first: true = this NPC offers `q` (filled in)
  bool lifeOffer(const Actor& npc, Quest& q) const;
  // ---- M6 Steel hooks (VISION_PLAN 7, 15.11). Called from the core loop and DEFINED in the lane's own file.
  //   FOES (rpg/sim/foes_game.cpp): elites, champion packs, named uniques, boss phases, world bosses, soldiers' and
  //   bandits' culture arms.
  //   a monster or a bandit just came into play (end of spawnMonster / spawnHuman for bandits, after applyLevel): roll its
  //   rank and affixes, set its variant look, name and stats. D: the danger it was spawned at (its level)
  void foeSpawned(Actor& a, int D);
  //   once per update step (after raidStep): affix behaviours (summoner, blinking, burning trail, warded, regenerating,
  //   frenzied), boss phases, champion packs, named uniques near the player, roaming world bosses and their raids
  void foeStep(float dt);
  bool foeWarmedTick_ = false;   // (M6 fixer r3) foeStep generated a cold region or kingdom cell this tick (one per tick)
  //   (M6 fixer r2) generate the named uniques' regions and the world bosses' kingdom cells round the player now (a new
  //   game's start, behind the loading): each cold one costs a frame hitch of tens of ms when foeStep meets it in play
  void foeWarm();
  //   a blow is about to land on `victim` (damage(), after armour): returns the damage to apply (warded, armoured...) and
  //   applies on-hit effects (vampiric healing, frost-bound slows, the volatile's...). attacker: actor id or -1
  float foeHit(Actor& victim, float dmg, int attacker);
  //   something died (kill(), before the loot drops): splitting, volatile, a named unique or world boss slain (marks,
  //   news, the realm's BeastSlain)
  void foeKilled(const Actor& a, int killer);
  //   talkTo: innkeepers and guards warn of the named uniques and world bosses near (options in [DLG_FOES, DLG_CRAFT))
  void foeTalk(Actor& a);
  bool foeChoose(const DlgOpt& o);
  //   NUMBERS / FORGE (rpg/sim/craft_game.cpp): the smith's, smelter's and tanner's work and lessons as dialogue options
  //   in [DLG_CRAFT, DLG_M6_END) (smelt, forge, tan, apprentice, study a foreign piece, steal patterns), and the forge
  //   screen they may open
  void craftTalk(Actor& a);
  bool craftChoose(const DlgOpt& o);
  // the TOWNSFOLK lane's own runtime state (route caches, seat reservations, chat pairs...): defined in life_game.cpp
  // (shared: Game stays copyable; never saved)
  struct LifeRuntime;
  std::shared_ptr<LifeRuntime> lifeRt_;
  friend struct LifeOps;               // M5 TOWNSFOLK: the actors' internals (life_game.cpp)
  friend struct FoeOps;                // M6 FOES: the foes' internals (foes_game.cpp)
  friend struct CraftOps;              // M6 NUMBERS / FORGE: crafting's internals (craft_game.cpp)
  // quests (quests.cpp)
  std::string pendingPitch_;           // the spoken offer (first person) for pendingOffer_
  int pickRadiant(SiteType t, int32_t gx, int32_t gy, Rng& r, bool& danger, int exclude = -1);
  int pickDestination(int32_t gx, int32_t gy, Rng& r);   // Deliver: a settlement 150-700 tiles away
  void questTick(float dt);            // per step: Protect waves, escorts, Deliver recipients, failures
  void questMapLoaded();               // a cave, ruin or building map was just made: quest chests and people
  bool questTargetM2(const Quest& q, int& tx, int& ty) const;   // the M2 types' markers (false: the classic rules)
  std::string questStatusM2(const Quest& q) const;              // the M2 types' journal lines ("" none)
  std::string turnInLine(const Quest& q) const;                 // what the giver says when the reward is collected
  std::string questDialogue(Actor& a);                          // M2 quest talk with this NPC: options added to dlg
  void questLeftSite(int site);        // the player came out of a site: escorts follow, missing persons are safe
  bool questChest(int tx, int ty);     // a quest chest in the current map was opened (true: handled)
  void questSpawned(Actor& a);         // a person just streamed in: a named chief, a Deliver recipient
  void questActorDown(const Actor& a); // someone fell: a farmer, a missing person
  bool isRecipient(const Quest& q, const Actor& a) const;
  void questFail(Quest& q, const std::string& why);
  int escortFor(const Quest& q) const; // actors index of the quest's missing person (-1 none)
  void spawnEscort(Quest& q, Vec2 at);
  Vec2 questChestTile(const Quest& q) const;   // a far floor tile of the current dungeon map (tile coordinates)
  // wayside life (wayside.cpp)
  float tollAskT_ = 0;                 // the troll asked a moment ago (no repeat while the player stays on the bridge)
  Vec2 offBridge_;                     // the player's last position off a toll bridge (the troll's demand turns them back)
  bool waysideSpawn(int site, const Spawn& sp);        // a vignette's spawn (true: handled, spawned or held back)
  void waysideTick(float dt);          // ambushes, toll bridges
  bool useWaysideProp(art::Prop p, int tx, int ty);   // standing stones, a lone grave (true: handled)
  void tollTalk(Actor& troll);         // the troll's demand as a dialogue
  int rumourSite(int32_t gx, int32_t gy);              // the nearest unknown place worth a rumour (-1 none)
  std::string rumourLine(int site, int32_t fromX, int32_t fromY) const;
};
