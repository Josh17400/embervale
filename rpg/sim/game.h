// EMBERVALE simulation: actors, combat, AI, inventory, quests, dialogue, time of day, saving. No SDL.
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/sim/common.h"
#include "rpg/sim/items.h"
#include "rpg/sim/world.h"

enum class Mode : uint8_t { Title, Play, Dialogue, Menu, Shop, LevelUp, Dead, Paused };

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
};

struct Pickup {
  Vec2 p;
  Item item;
  int gold = 0;
  float t = 0;
  bool magnet = false;     // already flying to the player (sound played)
};

enum class Ev : uint8_t { Sfx, Hit, Blood, Explode, Sparkle, Dust, Heal, Frost, Text, Discover, LevelUp, QuestUpdate, Shake, MapChange, Notice };
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

struct ShopState {
  int actor = -1;
  uint64_t key = 0;        // merchant identity (npcKey)
  std::vector<Item> stock;
  std::string title;
};

class Game {
 public:
  explicit Game(uint64_t seed = 1);
  void newGame(uint64_t seed, int genVer = WORLDGEN_LATEST);   // genVer: world-generator version (old saves pass theirs)
  void update(float dt, const Input& in);

  // --- world & level
  World world;
  Map sub;                 // current cave/ruin/interior (when inside)
  bool inside = false;
  int subSite = -1, subBldg = -1;
  Map& map() { return inside ? sub : world.over; }
  const Map& map() const { return inside ? sub : world.over; }
  int mapKey() const { return !inside ? 0 : (subBldg >= 0 ? 100000 + subBldg : 1 + subSite); }

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
  uint8_t spellsKnown = 1;     // bit per Spell
  Spell spell = Spell::Flames;
  int kills = 0, dungeonsCleared = 0;
  float blessT = 0;            // shrine blessing time left
  std::string blessName;
  int lastTown = -1;           // respawn point
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
  std::set<uint64_t> looted;                 // mapKey<<32 | tile
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

  // persistence
  void serialize(std::vector<uint8_t>& out) const;
  bool deserialize(const std::vector<uint8_t>& in);   // reads every SAVE_VER from 1 up
  bool worldChanged = false;   // set by deserialize: the regenerated world's fingerprint differs from the saved one

  // test helpers
  bool godMode = false;
  bool noWildSpawns = false;           // metrics arenas: no roaming spawns or dens
  void debugSpawn(art::Monster m, int n, float dist);
  int debugSpawnAt(art::Monster m, Vec2 at, int level);   // returns the actor id (already aggro)

 private:
  int nextId_ = 1;
  float spawnT_ = 0;
  bool exitArmed_ = false;
  Quest pendingOffer_;
  int dragonId_ = -1;
  // what each merchant has left this restock period (every 2 days): buying must not refill the shelf
  std::map<uint64_t, std::pair<int, std::vector<Item>>> shopCache_;
  int findActor(int id) const;
  Vec2 freeSpot(int tx, int ty) const;
  void placePlayerAt(int tx, int ty);
  std::set<int> activeSites_;
  std::set<int> activeDens_;
  float denT_ = 0;
  bool perfectRoll_ = false;   // this roll already earned its slow-mo blip
  void updateDens(int ptx, int pty);
  int denClearedDay(int den) const;    // -1 = not cleared
  void heavySlam(Actor& a);
  Rng rng_;
  void emit(Ev t, Vec2 p, int a = 0, float f = 1, const std::string& s = "") { events.push_back({t, p, a, f, s}); }
  void sfx(int s, Vec2 p, float pitch = 1, float vol = 1);
  void say(const std::string& s);

  void resetPlayer();
  void recalcPlayer();
  void updatePlayer(float dt, const Input& in);
  void updateActor(Actor& a, float dt);
  void updateAI(Actor& a, float dt);
  void updateProjectiles(float dt);
  void updatePickups(float dt);
  void updateSpawning(float dt);
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
