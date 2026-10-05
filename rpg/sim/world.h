// World data: tile maps (overworld, caves, interiors), sites (cities, towns, caves...), buildings, generation.
#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "rpg/art.h"
#include "rpg/sim/common.h"
#include "rpg/sim/rooms.h"
#include "rpg/world/coords.h"
#include "rpg/world/ids.h"

namespace ew {   // rpg/world/source.h (the endless generator, M1)
class EndlessSource;
struct SitePlan;
struct RegionPlan;
struct ChunkData;
}
class ChunkStreamer;   // rpg/sim/stream.h (SIM lane, M1): prefetching chunks off the main thread

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
  if (t == art::Building::Windmill) return 6;   // (M1 fixer) a tall tower mill whose sails reach over its cap
  switch (art::artBase(t)) {   // M1 economy: the production buildings rise like their frames (the windmill like a tower)
    case art::Building::Tower: case art::Building::Temple: return 5;
    case art::Building::Keep: case art::Building::Inn: return 4;
    default: return 3;
  }
}
// WORLDGEN_V7: the same with the building's storeys: a 2-storey house, stone house or shop rises like an inn.
inline int bldgRiseTiles(art::Building t, int storeys) {
  int r = bldgRiseTiles(t);
  t = art::artBase(t);
  if (storeys >= 2 && (t == art::Building::House || t == art::Building::StoneHouse || t == art::Building::Shop || t == art::Building::Farmhouse))
    r = std::max(r, 4);
  return r;
}

// King (M1): the ruler in a capital's palace (VISION_PLAN 15.8); the main quest's Jarl keeps a city's keep
enum class Role : uint8_t { Villager, Guard, Merchant, Smith, Innkeeper, Priest, Jarl, Farmer, Child, Mage, Bandit, King, COUNT };

// the name a building type goes by in the HUD ("INN", "THE KEEP", "THE PALACE"...)
const char* bldgTypeName(art::Building t);

struct Bldg {
  ew::Gid id = 0;          // M1: stable identity (endless worlds: makeId(region, IdKind::Bldg, ...); classic: legacyBldgId)
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
  // M1 kingdom identity: the ruling kingdom's banner (rgba; banner == 0: none). Set by the settlement generator; keeps,
  // palaces, barracks, gatehouses and inns may fly it (the painter decides where, BuildingFacts carries it)
  uint32_t banner = 0, banner2 = 0;
  uint8_t emblem = 0;
  // M1: how urban the place it stands in is (art::urbanize): 0 countryside / village, 1 town, 2 city, 3 a capital
  uint8_t urban = 0;
  // M1 economy: type-specific detail the exterior shows (art::BuildingFacts::variant; the watermill: bit 0 = its wheel
  // on the west side, where its river runs)
  uint8_t variant = 0;
  int floors() const { return genVer >= WORLDGEN_V7 ? std::max(1, (int)storeys) : 1; }
  int doorX() const { return r.x + r.w / 2; }
  int doorY() const { return r.y + r.h - 1; }
};
// M0b: the exterior's style (what the view paints) and its art facts, so interiors can match their outside.
inline art::ArchStyle bldgArch(const Bldg& b) {
  return art::withRoofTint(art::urbanize(art::archForBiome((int)b.biome, b.seed), b.urban, b.seed), b.roof);
}
inline art::BuildingFacts bldgFacts(const Bldg& b) {
  art::BuildingFacts f;
  f.storeys = b.storeys;
  f.hearth = b.hearth;
  f.banner = b.banner;
  f.banner2 = b.banner2;
  f.emblem = b.emblem;
  f.variant = b.variant;
  return f;
}
// WORLDGEN_V7 decisions (rpg/sim/world.cpp), deterministic from the type, the footprint and a hash:
int bldgStoreysV7(art::Building t, int wTiles, int hTiles, uint32_t h);
bool bldgHearthV7(art::Building t, int storeys, uint32_t h);

struct Site {
  ew::Gid id = 0;     // M1: stable identity (endless worlds: the SitePlan id; classic: legacySiteId(index))
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
  // M1 kingdom identity (VISION_PLAN 15.8, M4 groundwork): which kingdom holds it (World::kingdoms index, -1 none /
  // wildlands) and whether it is that kingdom's capital (the king's palace stands there)
  int kingdom = -1;
  bool capital = false;
  bool start = false;            // M1: the start village of an endless world
  uint8_t archetype = 0;         // M1: ew::Archetype (fishing, mining, farming...; rpg/world/source.h)
  // M1 economy (rpg/world/economy.h): what the settlement lives from (ew::Specialty), and the goods it makes and must
  // buy (ew::goodBit masks), for the economy (M4) and crafting (M6)
  uint8_t special = 0;
  uint32_t produces = 0, needs = 0;
  bool settlement() const { return type == SiteType::City || type == SiteType::Town || type == SiteType::Village; }
};

// M1 (VISION_PLAN 15.8, groundwork for M4): a kingdom. Every settlement records its kingdom; the colours fly on its
// banners, gates, keeps and signs, the name label and the world map show it.
struct Kingdom {
  ew::Gid id = 0;
  std::string name;              // "ASHMARK" (the HUD says "KINGDOM OF ASHMARK")
  uint32_t color = 0;            // banner field (rgba)
  uint32_t color2 = 0;           // banner charge / trim (rgba)
  uint8_t emblem = 0;            // the charge painted on the banner (art decides what each index looks like)
  ew::Gid capitalId = 0;         // its capital's Site id (0: none known yet)
  int32_t gx = 0, gy = 0;        // global tile of its seat (the capital, or the kingdom cell's centre)
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
  ew::Gid id = 0;                          // M1: stable identity (endless worlds; classic dens keep their index)
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
  std::vector<int32_t> bldgAt;   // building index at tile or -1 (M1: 32-bit, an endless session's records may pass 32767)
  std::vector<uint8_t> biome;    // Biome (overworld only)
  std::vector<uint8_t> height;   // M1: relief per tile (endless overworld only, else empty): bits 0..2 the level 0..7
                                 // (VISION_PLAN 11.1), HEIGHT_CLIFF a cliff face (not walkable), HEIGHT_RAMP a ramp or
                                 // stairs between levels (walkable). The view draws faces where the level steps down.
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
  static constexpr uint8_t HEIGHT_LEVEL = 7, HEIGHT_CLIFF = 8, HEIGHT_RAMP = 16;
  int heightAt(int x, int y) const { return in(x, y) && !height.empty() ? (height[(size_t)y * w + x] & HEIGHT_LEVEL) : 0; }
  uint8_t heightBits(int x, int y) const { return in(x, y) && !height.empty() ? height[(size_t)y * w + x] : 0; }
  Biome biomeAt(int x, int y) const { return in(x, y) && !biome.empty() ? (Biome)biome[(size_t)y * w + x] : Biome::Plains; }
  void alloc(int w_, int h_, Ground fill);
  void rebuildSolid();
};

bool propSolid(art::Prop p);   // rpg/sim/prop_rules.cpp

struct World {
  uint64_t seed = 0;
  int genVersion = WORLDGEN_LATEST;   // generator version this world was built with
  // over: the classic island (448x448), or in an endless world the Active Window (VISION_PLAN 2.9): a WIN x WIN tile
  // window onto the endless land whose local (0,0) is global tile (ox, oy). Everything in World and Game keeps using
  // window-local tiles and pixels; shiftWindow moves the window and translates every local coordinate.
  Map over;
  std::vector<Site> sites;
  int startSite = 0;      // village the player starts near
  int capital = 0;        // city with the jarl who gives the main quest
  int lair = -1;
  std::vector<std::pair<int, int>> gates;   // city gatehouses: left tile of a 3-wide opening in a horizontal wall run
  std::vector<Den> dens;                    // empty before WORLDGEN_V2
  std::vector<IRect> wallGaps;              // every opening cut into a city wall (gates included): bookkeeping only,
                                            // recorded by every generator version, never saved (rendering and tests)
  std::vector<Kingdom> kingdoms;            // M1: every kingdom met so far (endless) or the island's one

  // ---- M1 endless world (rpg/sim/world_endless.cpp). Classic worlds keep endless == false and ox == oy == 0.
  static constexpr int WIN = 256;           // window size in tiles (8 x 8 chunks)
  static constexpr int WIN_SHIFT = 64;      // the window moves in steps of this many tiles
  bool endless = false;
  int32_t ox = 0, oy = 0;                   // global tile of over's (0,0); always a multiple of ew::CHUNK
  std::shared_ptr<ew::EndlessSource> src;   // the generator (shared: copies of a World share one cache)
  std::unordered_map<ew::Gid, int> siteById, bldgById, denById, kingdomById;   // Gid -> handle (index)
  std::unordered_set<uint64_t> spawnKeys, gateKeys;   // streamed spawns (site id ^ slot) and gates already added
  int windowShifts = 0;                     // how many times the window has moved (tests, perf)
  // ---- M1 SIM lane: streaming without hitches, bounded records, spatial look-ups (world_endless.cpp)
  // the prefetcher (stream.h): Game::prefetchTick creates it on first use (tests that never update never start a
  // thread) and keeps its wish list current; placeWindow takes ready chunks from it and generates the rest itself
  std::shared_ptr<ChunkStreamer> streamer;
  struct StreamStats {
    int shifts = 0;                 // window moves (shifts and recentres)
    int walkShifts = 0;             // ... of those, ordinary walking shifts (the rest: teleports, loads, new games)
    int prefetched = 0;             // chunks a move took ready-made from the streamer
    int syncChunks = 0;             // chunks a move had to generate on the main thread (teleports, or prefetch behind)
    int syncInShifts = 0;           // ... of those, during ordinary walking shifts (should stay 0)
    int recycled = 0;               // far spawn/gate records dropped to bound memory (they re-stream when near again)
    int entrancesRepaired = 0;      // cave / ruin entrance tiles the chunks left without their door (painted over):
                                    // put back so a dungeon (a shard ruin...) can always be entered; a generator bug
    double lastMoveMs = 0, worstShiftMs = 0, worstRecentreMs = 0;
  };
  StreamStats sstats;
  // handles of the sites and dens whose area lies within the window (plus NEAR_MARGIN): every per-frame look-up
  // (siteAt, settlementAt, spawning, dens) walks these instead of every record of the session
  static constexpr int NEAR_MARGIN = 48;
  std::vector<int> nearSites, nearDens;
  std::unordered_map<int, std::vector<int>> siteSpawns;   // site handle -> indices into over.spawns
  static constexpr size_t SPAWN_SOFT_CAP = 6000;          // over.spawns / gates beyond this: recycle far records
  size_t spawnCap = SPAWN_SOFT_CAP;                       // (tests lower it to exercise recycling)
  // the region plan (a copy): the streamer's ready copy, else the main source's (generated now if needed)
  ew::RegionPlan regionPlan(int32_t rx, int32_t ry);
  // refresh the streamer's wish list for a player at window tile (ptx, pty) moving (dirx, diry) (-1, 0, 1 each)
  void prefetch(int ptx, int pty, int dirx, int diry);
  void rebuildNear();                       // nearSites / nearDens for the current window
  void rebuildSiteSpawns();                 // siteSpawns from over.spawns
  void recycleFar();                        // drop far spawns, gates and wall gaps (re-streamed when they come near)
  // the nearest site of a type to a GLOBAL tile among the region plans within `regions` regions (loading it into the
  // records); capitalOnly: a kingdom's capital. -1 if none. Scripts' goto, tests, far quest targets.
  int findSiteNear(int32_t gx, int32_t gy, SiteType t, int regions, bool capitalOnly = false, int archetype = -1);   // archetype: ew::Archetype or -1
  bool nearWindow(int tx, int ty) const { return tx >= -NEAR_MARGIN && ty >= -NEAR_MARGIN && tx < WIN + NEAR_MARGIN && ty < WIN + NEAR_MARGIN; }
  // a new endless world for this seed, the window centred on the start village (sites, kingdoms, startSite, capital,
  // lair and the main-quest ruins filled from the generator's start plan)
  void generateEndless(uint64_t seed);
  // the same with the window placed at a given origin straight away (loading a save: the window is built once, where
  // the save was made, instead of at the start and then again there)
  void generateEndlessAt(uint64_t seed, int32_t originX, int32_t originY);
  // move the window by (sx, sy) tiles (multiples of ew::CHUNK): shifts the map, streams the new strips in from the
  // generator, loads the sites of regions coming near, and translates every window-local coordinate in World.
  // The caller (Game::maybeRecentre) translates actors, projectiles and pickups by (-sx, -sy) tiles.
  void shiftWindow(int sx, int sy);
  // centre the window on a global tile (teleports, loading a save): regenerates the whole window
  void recentreOn(int32_t gx, int32_t gy);
  ew::GTile toGlobal(int lx, int ly) const { return {ox + lx, oy + ly}; }
  int siteHandle(ew::Gid id) const { auto it = siteById.find(id); return it == siteById.end() ? -1 : it->second; }
  int bldgHandle(ew::Gid id) const { auto it = bldgById.find(id); return it == bldgById.end() ? -1 : it->second; }
  // make sure the site with this id is in `sites` (loads its region plan if needed); -1 if no such site
  int ensureSite(ew::Gid id);
  int ensureDen(ew::Gid id);            // the same for a den (dens[] handle, -1 none)
  // make sure a site's buildings, spawns and gates are in the records even when it lies outside the window (a quest
  // giver's house far away, a save made in a distant inn)
  void ensureSiteRecords(int site);
  // window internals (world_endless.cpp)
  void placeWindow(int32_t newOx, int32_t newOy);       // move the origin: keep overlapping chunks, stream the rest
  void streamChunk(int32_t gcx, int32_t gcy, bool tiles);   // records (and with tiles: ground...) of one global chunk
  void loadRegionsAround();                             // site and den plans of the regions near the window
  int addSitePlan(const ew::SitePlan& p);
  int addKingdom(ew::Gid id);
  const Kingdom* kingdomOf(int site) const {
    return site >= 0 && site < (int)sites.size() && sites[(size_t)site].kingdom >= 0 ? &kingdoms[(size_t)sites[(size_t)site].kingdom] : nullptr;
  }

  void generate(uint64_t seed, int genVer = WORLDGEN_LATEST);
  uint32_t fingerprint() const;   // hash of site and building identity (names, places, indices): detects a reshuffled world
  int endlessDanger(int tx, int ty) const;   // world_endless.cpp: the generator's danger at a window tile
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
