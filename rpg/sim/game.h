// EMBERVALE simulation: actors, combat, AI, inventory, quests, dialogue, time of day, saving. No SDL.
#pragma once
#include <array>
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
#include "rpg/sim/explored.h"
#include "rpg/sim/factions.h"
#include "rpg/sim/items.h"
#include "rpg/sim/world.h"

// Creator: the character creator after NEW GAME (the world exists; the player picks looks and a background).
enum class Mode : uint8_t { Title, Play, Dialogue, Menu, Shop, LevelUp, Dead, Paused, Creator };

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
};

// WindowShift (M1): the endless world's Active Window moved; p is the pixel offset every overworld position just got
// (the view moves its camera, particles and floating texts by it)
enum class Ev : uint8_t { Sfx, Hit, Blood, Explode, Sparkle, Dust, Heal, Frost, Text, Discover, LevelUp, QuestUpdate, Shake, MapChange, Notice,
                          WindowShift };
struct Event {
  Ev type = Ev::Sfx;
  Vec2 p;
  int a = 0;          // Sfx id / colour / amount
  float f = 1;        // pitch / magnitude
  std::string s;
  float vol = 1;      // Sfx volume
};

enum class QType : uint8_t { Main, Clear, Hunt, Retrieve, Bounty };
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
  int stage = 0;                // main quest
};

struct DlgOpt { std::string label; int action = 0; int arg = 0; };
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

struct ShopState {
  int actor = -1;
  uint64_t key = 0;        // merchant identity (npcKey)
  std::vector<Item> stock;
  std::string title;
};

class Game {
 public:
  explicit Game(uint64_t seed = 1);
  void newGame(uint64_t seed, int genVer = WORLDGEN_LATEST);   // a classic island world (tests; genVer: its generator)
  void newEndlessGame(uint64_t seed);                          // M1: a new game on the endless mainland (VISION_PLAN 15.6)
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

  // UI-facing state
  Dialogue dlg;
  ShopState shop;
  std::string locName;     // current region/site label
  int curSite = -1;
  float noticeT = 0;
  std::string notice;

  // --- actions called by the UI
  void startPlay() { mode = Mode::Play; }
  void beginCreator() { mode = Mode::Creator; }   // after newGame(): the UI edits app/background, then...
  void finishCreator();                            // ...applies the background's trait, rebuilds the look, starts play
  bool rewardWaiting(const Actor& npc) const;      // a completed quest's reward waits with this NPC (world "!" marker)
  std::string questStatus(const Quest& q) const;   // one journal line: what to do next ("" = nothing extra)
  void dialogueChoose(int optIndex);
  void closeDialogue();
  bool buy(int stockIndex);
  bool sell(int invIndex);
  void useItem(int invIndex);          // equip / consume
  void dropItem(int invIndex);
  void chooseLevelUp(int stat);        // 0 health 1 magicka 2 stamina
  bool fastTravel(int site);
  void respawn();
  int interactTarget() const;          // actor id the player would talk to (-1 none)
  int interactProp(int& tx, int& ty) const;   // usable prop in front of the player (art::Prop + 1, 0 none)
  bool bedIsYours(int tx, int ty) const;      // M0b: a bed you may sleep in (an inn's beds are let room by room)
  bool nearDoorOrExit() const;
  float armorRating() const;
  float weaponDamage() const;
  // first level in ~4 min, level 5 in ~30 (PLAN.md targets; measured by rpg_test --metrics)
  int xpForNext() const { return 120 + (plLevel - 1) * 100 + (plLevel - 1) * (plLevel - 1) * 14; }
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
  // refused and the title says so (saveVersion tells the UI which case it is).
  void serialize(std::vector<uint8_t>& out) const;
  bool deserialize(const std::vector<uint8_t>& in);
  static int saveVersion(const std::vector<uint8_t>& in);   // the file's SAVE_VER (0: not a save at all)
  static int currentSaveVersion();
  // a current-format save whose world came from an older generator (endless ENDLESS_GEN_VER, classic WORLDGEN_*):
  // refused like an older SAVE_VER, and the title words it the same way
  static bool saveFromOlderGenerator(const std::vector<uint8_t>& in);
  bool worldChanged = false;   // set by deserialize: the regenerated world's fingerprint differs from the saved one
  // M1 stable keys: an overworld chest (endless worlds key it by global tile, so it stays looted wherever the window is)
  uint64_t lootKey(int tx, int ty) const;
  uint64_t npcKeyOf(int site, int bldg, int slot) const;   // npcKey's formula from handles (quest givers)

  // M1 endless streaming and NPC level of detail (SIM lane)
  void frameWork(double budgetMs);     // once per rendered frame: the streamer's work (web: generation within the budget)
  struct PerfCounters {
    int npcAwake = 0, npcAsleep = 0;   // townsfolk in `actors` this step, by LOD
    int hostiles = 0;                  // monsters and bandits in `actors`
    int activeSites = 0;               // settlements and camps whose people are streamed in
    int spawnedNpcs = 0, despawnedNpcs = 0;   // people streamed in / out so far
  };
  PerfCounters perf;
  static constexpr int FOLK_CAP = 80;          // townsfolk streamed in at once (nearest first)
  static constexpr int FOLK_IN = 34, FOLK_OUT = 44;   // tiles: a person streams in within FOLK_IN, out beyond FOLK_OUT
  static constexpr int SITE_IN = 26, SITE_OUT = 40;   // tiles from a site's area: it activates / deactivates
  // half the view in tiles plus a margin (the platform layer sets it from the logical canvas: the VIEW lane's screen
  // fit makes it wider on phones); townsfolk beyond it may sleep
  float sleepHalfW = 22.0f, sleepHalfH = 13.0f;

  // test helpers
  bool godMode = false;
  bool noWildSpawns = false;           // metrics arenas: no roaming spawns or dens
  bool streamThreads = true;           // false: stream as the web build does (no worker; frameWork generates); tests
  void debugSpawn(art::Monster m, int n, float dist);
  int debugSpawnAt(art::Monster m, Vec2 at, int level);   // returns the actor id (already aggro)
  void debugFell(int actorId);         // tests: an actor falls as if a monster struck it down (kill with no killer)
  void debugKit();                     // the pre-M0 starting kit (iron sword, hunting bow, 20 arrows, 3 potions, bread),
                                       // equipped: for fight scripts and bots once the real start is shirt-only
  // M0b: go into building bi (from anywhere, leaving the current sub-level) and up to floor f; false if f is not one
  // of its floors. Scripts, tests and save loading use it; play goes through doors and stairs.
  bool debugEnterBuilding(int bi, int floor = 0);
  void debugLeave() { if (inside) leaveSub(); }   // step out of the building or site (tests)
  // the dragon's lair by name (its peak is named per world: makeDungeonName)
  std::string lairName() const { return world.lair >= 0 && world.lair < (int)world.sites.size() ? world.sites[(size_t)world.lair].name : std::string("THE DRAGON'S PEAK"); }
  // M1: put the player on a GLOBAL tile (endless: the window recentres there; classic: island tiles), outdoors, on the
  // nearest free tile. Scripts (`at X Y`), --at and tests use it.
  void teleportGlobal(int32_t gx, int32_t gy);
  void changeFloor(int floor);         // inside a building: move to another of its floors, arriving by its stairs
  bool stairsAsleep() const { return stairsArrive_ >= 0; }   // just climbed: the stairwell down sleeps against a push north

 private:
  int nextId_ = 1;
  float exploreT_ = 0;
  float spawnT_ = 0;
  bool exitArmed_ = false;
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
  void enterBuilding(int bldg);
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
};
