// HOLDLINE simulation: open pixel world, villages with inns, found units that merge, camps, raids.
// Pure C++ (no SDL) so it runs headless for tests.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "engine/mathx.h"

constexpr int TILE = 16;
constexpr int MAP_W = 224, MAP_H = 144;
constexpr float SIM_DT2 = 1.0f / 120.0f;
constexpr int MAX_LV = 6;
constexpr int MAX_PARTY = 5;
constexpr float VILLAGE_R = 70.0f;

enum class Tile : uint8_t { Water, Sand, Grass, Dirt, Rock };
enum class Obj : uint8_t { None, Tree, Bush, Boulder };
enum class Species : uint8_t { Knight, Archer, Mage, COUNT };
enum class UState : uint8_t { Wild, Party, Lodged };
enum class MMode : uint8_t { Title, Play, Paused };

struct Unit {
  int id = 0;
  Species sp = Species::Knight;
  int lv = 1;
  UState st = UState::Wild;
  int village = -1;            // inn it lodges in
  Vec2 p, v, dest;
  float hp = 1, maxhp = 1, cd = 0, anim = 0, face = 1, flash = 0, fallen = 0, wander = 0;
  bool moving = false;
};
struct Enemy {
  int id = 0;
  int kind = 0;                // 0 goblin, 1 brute
  Vec2 p, home;
  float hp = 1, maxhp = 1, cd = 0, anim = 0, face = -1, flash = 0;
  int camp = -1;               // -1 = raider
  bool moving = false;
};
struct Camp {
  Vec2 p;
  bool cleared = false, chestTaken = false;
  int total = 0;
};
struct Village {
  Vec2 p, door;
  bool home = false, discovered = false;
  float hp = 100, maxhp = 100;
  std::string name;
};
struct Proj { Vec2 p; int targetId = 0; float dmg = 0, splash = 0; bool arrow = true; };
struct Pop { Vec2 p; std::string text; uint32_t color = 0xFFFFFFFF; float t = 0; };

enum class Ev : uint8_t {
  Recruit, Merge, Lucky, Hit, Kill, CampCleared, RaidWarn, RaidEnd, VillageHit, HeroHurt, HeroDown,
  Chest, Discover, Lodge, Join, Hire, Shot, Plundered, Denied
};
struct Event { Ev t; Vec2 p; float a = 0; int n = 0; };

struct Hero {
  Vec2 p, v;
  float hp = 100, maxhp = 100, cd = 0, anim = 0, hurt = 0, respawn = 0;
  float face = 1;
  bool dead = false, moving = false;
};

class MGame {
 public:
  explicit MGame(uint64_t seed = 1) { generate(seed); }
  void generate(uint64_t seed);              // new world
  void update(float dt, Vec2 move);          // advance (move: desired hero direction, len 0..1)

  // --- player actions ---
  int  unitAt(Vec2 p, float r = 11.0f) const;            // owned unit under a world point, or -1
  bool tryMerge(int draggedId, int targetId);            // merge two units if same species + level
  void clickUnit(int id);                                // toggle party <-> lodged
  int  lodgeParty();                                     // lodge all party units (must be near a village)
  bool hire();                                           // pay gold for a new lodged unit at the home inn
  int  autoMerge();                                      // merge every available pair; returns merges
  int  hireCost() const { return 20 + 10 * hired; }
  int  nearVillage(Vec2 p, float r = VILLAGE_R) const;   // discovered village index or -1

  // --- queries ---
  bool solidAt(float x, float y) const;
  Tile tileAt(int tx, int ty) const { return inMap(tx, ty) ? tiles[ty * MAP_W + tx] : Tile::Water; }
  Obj  objAt(int tx, int ty) const { return inMap(tx, ty) ? objs[ty * MAP_W + tx] : Obj::None; }
  static bool inMap(int tx, int ty) { return tx >= 0 && ty >= 0 && tx < MAP_W && ty < MAP_H; }
  Unit* unitById(int id);
  const Unit* unitById(int id) const;
  int partyCount() const;
  static float unitMaxHp(Species s, int lv);
  static float unitDmg(Species s, int lv);
  static const char* speciesName(Species s);

  // --- save / load ---
  void serialize(std::vector<uint8_t>& out) const;
  bool deserialize(const std::vector<uint8_t>& in);

  // --- state (read by renderer) ---
  MMode mode = MMode::Title;
  uint64_t seed = 1;
  std::vector<Tile> tiles;
  std::vector<Obj> objs;
  std::vector<uint8_t> explored;             // per tile
  bool exploredDirty = true;
  Hero hero;
  std::vector<Unit> units;
  std::vector<Enemy> enemies;
  std::vector<Camp> camps;
  std::vector<Village> villages;
  std::vector<Proj> projs;
  std::vector<Pop> pops;
  std::vector<Event> events;
  std::vector<Rng> dummy_;
  int gold = 30;
  int hired = 0;
  int raidNum = 0;
  float raidTimer = 70.0f, raidBanner = 0;
  bool raidActive = false;
  Vec2 raidFrom;
  float time = 0;
  int kills = 0, merges = 0;
  bool godMode = false;
  std::string banner;
  float bannerT = 0;

 private:
  Rng rng{1};
  bool plundered_ = false;
  int nextId = 1;
  std::vector<Vec2> solidRects;              // x0,y0 / x1,y1 pairs (building footprints)
  void genWorld();
  void carveDisc(int cx, int cy, int r, Tile t);
  void carveRoad(Vec2 a, Vec2 b, int halfW);
  void placeBuildings();
  void spawnCampEnemies(int ci);
  void spawnUnit(Species s, int lv, Vec2 p, UState st, int village = -1);
  void revealAround(Vec2 p, int radiusTiles);
  void updateHero(float dt, Vec2 move);
  void updateUnits(float dt);
  void updateEnemies(float dt);
  void updateProjs(float dt);
  void updateRaids(float dt);
  void damageEnemy(int idx, float dmg, bool fromHero);
  void hurtHero(float dmg);
  void hurtUnit(Unit& u, float dmg);
  void say(const std::string& s) { banner = s; bannerT = 3.0f; }
  void pop(Vec2 p, const std::string& t, uint32_t c) { pops.push_back({p, t, c, 0}); }
  void emit(Ev t, Vec2 p, float a = 0, int n = 0) { events.push_back({t, p, a, n}); }
  bool moveBody(Vec2& p, Vec2 delta, float radius);
  void recomputeStats(Unit& u, bool heal);
};
