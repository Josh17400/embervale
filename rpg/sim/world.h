// World data: tile maps (overworld, caves, interiors), sites (cities, towns, caves...), buildings, generation.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/sim/common.h"

// Ground terrain. Order matters for rendering blend priority (higher index draws over lower at edges).
enum class Ground : uint8_t {
  DeepWater, Water, Sand, Swamp, Grass, Meadow, ForestFloor, Autumn, Tundra, Snow, Dirt, Farmland, Road, Plaza,
  Rock,         // mountain, solid
  CaveFloor, CaveWall, WoodFloor, StoneFloor, InteriorWall, Bridge, Lava, Ice, Void,
  COUNT
};
inline bool groundSolid(Ground g) {
  return g == Ground::DeepWater || g == Ground::Water || g == Ground::Rock || g == Ground::CaveWall ||
         g == Ground::InteriorWall || g == Ground::Lava || g == Ground::Void;
}
inline bool groundWater(Ground g) { return g == Ground::DeepWater || g == Ground::Water; }

enum class Biome : uint8_t { Ocean, Beach, Plains, Forest, Autumn, Taiga, Snow, Swamp, Desert, Mountain, COUNT };
const char* biomeName(Biome b);

enum class MapKind : uint8_t { Overworld, Cave, Ruin, Interior };

enum class SiteType : uint8_t { City, Town, Village, Cave, Ruin, BanditCamp, Shrine, DragonLair, COUNT };
const char* siteTypeName(SiteType t);

// World-generator versions (see the rule at the top of world.cpp). A world is always regenerated from
// (seed, genVersion), and saves store both, so a save keeps the exact world it was made in.
constexpr int WORLDGEN_V1 = 1;       // the original generator: every save before SAVE_VER 2
constexpr int WORLDGEN_V2 = 2;       // + wilderness dens (Gen::dens, own "dens" stream)
constexpr int WORLDGEN_V3 = 3;       // M0: cluttered interiors (genInterior via Bldg::genVer) and the M0 building/wall
                                     // pass (city-wall joins, building footprints); every such change is gated on ver >= 3
constexpr int WORLDGEN_LATEST = 3;   // what new games use; bump when generator output changes, gating the change on it

enum class Role : uint8_t { Villager, Guard, Merchant, Smith, Innkeeper, Priest, Jarl, Farmer, Child, Mage, Bandit, COUNT };

struct Bldg {
  art::Building type = art::Building::House;
  IRect r;                 // footprint in tiles
  uint32_t roof = 0;
  uint32_t seed = 0;
  int site = -1;
  Role owner = Role::Villager;   // who lives/works here
  int genVer = WORLDGEN_LATEST;  // generator version of the world it belongs to (gates genInterior changes)
  int doorX() const { return r.x + r.w / 2; }
  int doorY() const { return r.y + r.h - 1; }
};

struct Site {
  SiteType type = SiteType::Village;
  std::string name;
  IRect r;            // area in tiles (overworld)
  int ex = 0, ey = 0; // entrance / centre tile (caves: the tile in front of the entrance)
  int level = 1;      // difficulty
  bool discovered = false;
  bool cleared = false;   // dungeon boss / camp chief dead
  int bldgFirst = 0, bldgCount = 0;
  uint32_t seed = 0;
  bool mainQuest = false; // one of the ancient ruins holding an Ember Shard
  art::Monster theme = art::Monster::Spider;   // dungeon inhabitants
  int genVer = WORLDGEN_LATEST;  // generator version of the world it belongs to (gates genCave/genRuin changes)
};

// Something to populate when a map becomes active (deterministic per map).
struct Spawn {
  int x = 0, y = 0;
  bool npc = false;
  art::Monster mon = art::Monster::Wolf;
  Role role = Role::Villager;
  bool boss = false;
  bool bandit = false;   // human enemy (mon ignored)
  int site = -1;
  int slot = 0;          // stable identity within the map (names/looks/quests derive from it)
};

// A wilderness den (WORLDGEN_V2+): the home of a wild pack. Its props (bones, boulders, a chest...) are on the
// overworld prop layer; the pack spawns here when the player comes near and walks back here when it loses them.
// Index = identity (Game::killedSlots[-1] records cleared dens by index).
struct Den {
  int x = 0, y = 0;                        // centre tile
  art::Monster mon = art::Monster::Wolf;   // who lives here
  uint8_t pack = 3;                        // members at full strength (trimmed near the start village)
};

// A placed object on a tile (props layer). Buildings are separate.
struct Map {
  std::vector<Spawn> spawns;
  MapKind kind = MapKind::Overworld;
  int w = 0, h = 0;
  uint32_t seed = 0;
  std::vector<uint8_t> ground;   // Ground
  std::vector<uint8_t> prop;     // art::Prop + 1, 0 = none
  std::vector<uint8_t> solid;    // 1 = blocks movement (props, buildings, walls)
  std::vector<uint8_t> wall;     // city wall pieces (1)
  std::vector<uint8_t> deco;     // per-tile decoration id (rpg/sim/deco.h; 0 = none): non-blocking clutter drawn on
                                 // the floor (stains, straw, scattered papers...). Regenerated with the map, never saved
  std::vector<int16_t> bldgAt;   // building index at tile or -1
  std::vector<uint8_t> biome;    // Biome (overworld only)
  std::vector<Bldg> bldgs;
  int exitX = 0, exitY = 0;      // caves/interiors: tile you appear on when entering (and where the exit is)
  Ground at(int x, int y) const { return (x < 0 || y < 0 || x >= w || y >= h) ? Ground::Void : (Ground)ground[(size_t)y * w + x]; }
  int decoAt(int x, int y) const { return (x < 0 || y < 0 || x >= w || y >= h || deco.empty()) ? 0 : deco[(size_t)y * w + x]; }
  int propAt(int x, int y) const { return (x < 0 || y < 0 || x >= w || y >= h) ? 0 : prop[(size_t)y * w + x]; }
  void setG(int x, int y, Ground g) { if (in(x, y)) ground[(size_t)y * w + x] = (uint8_t)g; }
  void setP(int x, int y, int p) { if (in(x, y)) prop[(size_t)y * w + x] = (uint8_t)p; }
  void setProp(int x, int y, art::Prop p) { setP(x, y, (int)p + 1); }
  bool in(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
  bool blocked(int x, int y) const;
  Biome biomeAt(int x, int y) const { return in(x, y) && !biome.empty() ? (Biome)biome[(size_t)y * w + x] : Biome::Plains; }
  void alloc(int w_, int h_, Ground fill);
  void rebuildSolid();
};

bool propSolid(art::Prop p);   // rpg/sim/prop_rules.cpp

struct World {
  uint64_t seed = 0;
  int genVersion = WORLDGEN_LATEST;   // generator version this world was built with
  Map over;
  std::vector<Site> sites;
  int startSite = 0;      // village the player starts near
  int capital = 0;        // city with the jarl who gives the main quest
  int lair = -1;
  std::vector<std::pair<int, int>> gates;   // city gatehouses: left tile of a 3-wide opening in a horizontal wall run
  std::vector<Den> dens;                    // empty before WORLDGEN_V2
  std::vector<IRect> wallGaps;              // every opening cut into a city wall (gates included): bookkeeping only,
                                            // recorded by every generator version, never saved (rendering and tests)
  void generate(uint64_t seed, int genVer = WORLDGEN_LATEST);
  uint32_t fingerprint() const;   // hash of site and building identity (names, places, indices): detects a reshuffled world
  int siteAt(int tx, int ty, int pad = 0) const;
  int nearestSite(int tx, int ty, SiteType t, int exclude = -1) const;
  int zoneLevel(int tx, int ty) const;
};

// Independent RNG seed for one generator feature: hash of the world seed and a feature tag such as "dens".
// Features added after WORLDGEN_V1 draw from their own stream so they never shift any other feature's randomness.
uint64_t genSubSeed(uint64_t seed, const char* feature);

// sub-levels (caves, ruins, interiors) are generated deterministically from their seed
void genCave(Map& m, const Site& s, uint32_t seed);
void genRuin(Map& m, const Site& s, uint32_t seed);
void genInterior(Map& m, const Bldg& b, uint32_t seed);

// names
std::string makeTownName(Rng& r);
std::string makeDungeonName(Rng& r, SiteType t, Biome b = Biome::Plains);
std::string makePersonName(Rng& r, bool female);
