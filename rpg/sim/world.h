// World data: tile maps (overworld, caves, interiors), sites (cities, towns, caves...), buildings, generation.
#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/sim/common.h"
#include "rpg/sim/rooms.h"

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
constexpr int WORLDGEN_V4 = 4;       // M0 fix round: city walls are built before the town's buildings (final ring, gates and
                                     // side gates first; buildings, lamps and trees keep clear of the ring and every gate
                                     // approach; overland roads route through the real openings, which are paved)
constexpr int WORLDGEN_V5 = 5;       // M0 fix round 2: every road through a city wall gets a real gate (gatehouses searched
                                     // wider, a lane from each gate to the streets, street stubs outside the ring removed),
                                     // gates keep off rivers (a river crossing the ring keeps its water under a culvert arch),
                                     // buildings keep their roofs, sprites and doorsteps clear of each other, stalls keep
                                     // off facades
constexpr int WORLDGEN_V6 = 6;       // M0 fix round 3: worlds left with fewer than 8 caves get a top-up pass on its own
                                     // stream (rock edges scanned in a shuffled order, up to 10 caves); others are unchanged
constexpr int WORLDGEN_V7 = 7;       // M0b: storeys and rooms. Bldg::storeys / Bldg::hearth decided at generation (hashed on
                                     // their own "storeys" stream: no rng draw moves), 2-storey houses and shops rise a
                                     // tile higher in the V5 clearance test, and genInterior lays out rooms, partitions,
                                     // doors and real upper floors reached by stairs (genInteriorV4)
constexpr int WORLDGEN_LATEST = 7;   // what new games use; bump when generator output changes, gating the change on it

// WORLDGEN_V5 placement: how many tiles a building's sprite may rise above its footprint's top row (roof, steeple,
// cone), generous on purpose. A generator constant (never measured from the art), so art changes never move buildings.
inline int bldgRiseTiles(art::Building t) {
  switch (t) {
    case art::Building::Tower: case art::Building::Temple: return 5;
    case art::Building::Keep: case art::Building::Inn: return 4;
    default: return 3;
  }
}
// WORLDGEN_V7: the same with the building's storeys: a 2-storey house, stone house or shop rises like an inn.
inline int bldgRiseTiles(art::Building t, int storeys) {
  int r = bldgRiseTiles(t);
  if (storeys >= 2 && (t == art::Building::House || t == art::Building::StoneHouse || t == art::Building::Shop || t == art::Building::Farmhouse))
    r = std::max(r, 4);
  return r;
}

enum class Role : uint8_t { Villager, Guard, Merchant, Smith, Innkeeper, Priest, Jarl, Farmer, Child, Mage, Bandit, COUNT };

struct Bldg {
  art::Building type = art::Building::House;
  IRect r;                 // footprint in tiles
  uint32_t roof = 0;
  uint32_t seed = 0;
  int site = -1;
  Role owner = Role::Villager;   // who lives/works here
  int genVer = WORLDGEN_LATEST;  // generator version of the world it belongs to (gates genInterior changes)
  // M0b. storeys: how many storeys the exterior shows; from WORLDGEN_V7 also how many interior floors it has (older
  // worlds: the type's classic look, art::defaultStoreys, over a single interior floor). hearth: something inside burns
  // a fire (hearth, oven, forge), so the roof may carry a chimney; without it the exterior shows none (VISION_PLAN
  // 15.7: chimney => hearth). biome: where it stands (its ArchStyle and its interior's regional style). All three are
  // set for every generator version and never feed back into generation.
  uint8_t storeys = 1;
  bool hearth = true;
  Biome biome = Biome::Plains;
  int floors() const { return genVer >= WORLDGEN_V7 ? std::max(1, (int)storeys) : 1; }
  int doorX() const { return r.x + r.w / 2; }
  int doorY() const { return r.y + r.h - 1; }
};
// M0b: the exterior's style (what the view paints) and its art facts, so interiors can match their outside.
inline art::ArchStyle bldgArch(const Bldg& b) { return art::withRoofTint(art::archForBiome((int)b.biome, b.seed), b.roof); }
inline art::BuildingFacts bldgFacts(const Bldg& b) {
  art::BuildingFacts f;
  f.storeys = b.storeys;
  f.hearth = b.hearth;
  return f;
}
// WORLDGEN_V7 decisions (rpg/sim/world.cpp), deterministic from the type, the footprint and a hash:
int bldgStoreysV7(art::Building t, int wTiles, int hTiles, uint32_t h);
bool bldgHearthV7(art::Building t, int storeys, uint32_t h);

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
  int exitX = 0, exitY = 0;      // caves/interiors: tile you appear on when entering (and where the exit is); upper
                                 // floors of a building have no exit (-1)
  // M0b interiors (WORLDGEN_V7+, rooms.h): which floor of its building this map is, its stairs, and its rooms
  int floor = 0;
  Stairs up, down;               // the staircase up to floor+1 / down to floor-1 (x < 0: none)
  std::vector<RoomInfo> rooms;
  std::vector<int8_t> roomAt;    // per tile: index into rooms, -1 for walls (empty for older interiors)
  int roomIndexAt(int x, int y) const { return (x < 0 || y < 0 || x >= w || y >= h || roomAt.empty()) ? -1 : roomAt[(size_t)y * w + x]; }
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
// floor: 0 ground .. b.floors()-1. Worlds before WORLDGEN_V7 have one floor and keep their exact rooms.
void genInterior(Map& m, const Bldg& b, uint32_t seed, int floor = 0);

// names
std::string makeTownName(Rng& r);
std::string makeDungeonName(Rng& r, SiteType t, Biome b = Biome::Plains);
std::string makePersonName(Rng& r, bool female);
