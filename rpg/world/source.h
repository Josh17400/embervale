// The endless world generator's interface (VISION_PLAN 2.5, 2.9, M1). This header is the contract between the M1 lanes:
//   WORLD lane  implements EndlessSource (rpg/world/*.cpp: macro fields, hydrology, relief, region plans, roads, sites,
//               kingdoms, the start plan, the chunk pipeline). Internals live behind the pimpl, so they never need this
//               header changed.
//   TOWNS lane  implements buildSettlement (rpg/world/settlement.h): one settlement on a local buffer.
//   SIM lane    streams chunks and region plans into World's Active Window (rpg/sim/world_endless.cpp).
//   VIEW lane   reads macro() / region() / kingdoms for the world map and the minimap.
//
// Determinism contract (VISION_PLAN 2.3, 2.4): everything here is a pure function of (seed, coordinates). Structural
// decisions (where things are, whether they exist) use integer / Q16 maths (rpg/world/noise.h, dmath.h) only, so
// MSVC, clang (iOS) and Emscripten produce the same world. Nothing may depend on which chunk or region was asked for
// first or on cache state. Chunks are cut from the same global picture, so they meet without seams.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/world.h"
#include "rpg/world/coords.h"
#include "rpg/world/economy.h"
#include "rpg/world/geology.h"
#include "rpg/world/ids.h"
#include "rpg/world/poi.h"

namespace ew {

// Bumped when the endless generator's output changes. Old saves are not kept compatible across M1 (owner rule,
// 2026-10-04); the save stores it so a mismatch can say "this save is from an older world".
constexpr int ENDLESS_GEN_VER = 12;  // 12: M3c Wildlands (46 biomes: ChunkData::eco / ecoNb, eco-driven ground, flora,
                                     //     dens, settlements and cultures). Until M3c ships, the M3c lanes change the
                                     //     output under 12 and regenerate the goldens the WORLD lane owns.
                                     // 11: M3b Builders & Societies (societies, the builder: settlements ask for the
                                     //     society's buildings, every building designed by the builder). Until M3b ships,
                                     //     the M3b lanes change the output under 11 and regenerate the goldens they own.
                                     // 10: M3 Many Peoples (cultures: names, architecture, layouts, census looks). Until
                                     //     M3 ships, the M3 lanes change the output under 10 and regenerate the goldens
                                     //     they own; after, bump.
                                     // 9: M2 fixer round 3 (no wild pool by a gatehouse, street props on paving, bare plaza bands filled, towns built a band of land rows at a time); 8: M2 fixer round 2 (massifs at named summits and range crests, toll bridges, scatter off paving and snow); 7: M2 fixer (crags above the face only, roads spare wayside props, snow towns, palace beds, peaks by a drop); 2: M1 round 2 (meandering relief, organic terraces, settlement water); 4: M1 fixer (markets, specialisations)
                                     // 3: settlement economy (specialisations, production buildings, market rows)
                                     // 5: M1 fixer round 2 (planned markets, tables and cloths, pens and their beasts, the mine hill)
                                     // 6: M2 Wayfinder (vignettes, wonders, peaks, ecotone blend, geology, landmarks, the
                                     //    capitals' squares). Until M2 ships, the M2 lanes change the output under 6 and
                                     //    regenerate tests/fixtures/golden_endless.txt / golden_towns.txt; after, bump.

// One sample of the macro fields (L0) at a global tile. Q16 fixed point: 65536 = 1.0.
struct MacroSample {
  int32_t elev = 0;          // elevation; land above ELEV_SEA
  int32_t temp = 0;          // temperature (0 cold .. 1 hot)
  int32_t moist = 0;         // moisture (0 dry .. 1 wet)
  Biome biome = Biome::Plains;
  Eco eco = Eco::Meadow;     // M3c: the biome proper (rpg/world/biomes.h); ecoFamily(eco) == biome
  uint8_t height = 0;        // relief level 0..7 (VISION_PLAN 11.1): cliffs where it steps down
  bool water = false;        // sea, lake or river
  Gid kingdom = 0;           // whose land this is (0: wildlands, nobody's)
};
constexpr int32_t ELEV_SEA = 19661;   // 0.30 in Q16

// Settlement archetypes (VISION_PLAN 2.5 L1b), from the site's context
enum class Archetype : uint8_t { Plain, Farming, Fishing, Port, Mining, RiverCrossing, HillFort, Market, COUNT };

// SitePlan::flags
constexpr uint16_t SPF_START = 1;      // the start village
constexpr uint16_t SPF_CAPITAL = 2;    // its kingdom's capital (the king's palace)
constexpr uint16_t SPF_MAINQUEST = 4;  // an ancient ruin holding an Ember Shard
constexpr uint16_t SPF_STORY = 8;      // the main quest's city (the jarl who gives THE DRAGON'S SHADOW)
constexpr uint16_t SPF_PORT = 16;

// A site planned by its region (L1b). Global tiles throughout.
struct SitePlan {
  Gid id = 0;                       // makeId(rx, ry, IdKind::Site, local)
  SiteType type = SiteType::Village;
  int32_t gx = 0, gy = 0;           // footprint top-left (global tile)
  int w = 0, h = 0;                 // footprint size in tiles (settlements: the area they may build on)
  int32_t ex = 0, ey = 0;           // entrance / heart (global tile); caves: the tile in front of the entrance
  int level = 1;                    // danger (VISION_PLAN 7.1)
  uint32_t seed = 0;
  Gid kingdom = 0;                  // 0: wildlands
  uint16_t flags = 0;               // SPF_*
  Archetype archetype = Archetype::Plain;
  // M1 economy (rpg/world/economy.h): what a settlement lives from (from its surroundings), and the goods it makes and
  // must buy (goodBit masks). Specialty::None: the settlement generator derives it from the archetype and the land.
  Specialty special = Specialty::None;
  uint32_t produces = 0, needs = 0;
  art::Monster theme = art::Monster::Spider;   // dungeon inhabitants
  uint8_t kind = 0;                            // M2: Vignette -> VignetteKind, Wonder -> WonderKind (rpg/world/poi.h)
  uint16_t bldgBase = 0, bldgCap = 0;          // building ids: makeId(rx, ry, IdKind::Bldg, bldgBase + i), i < bldgCap
  std::string name;
  // M3: whose culture it is (cult::CultureId): its kingdom's dialect, or in the wildlands its culture cell's family.
  // Settlements are built and peopled in it; POIs carry it for names and (M4/M12) their ruins' history.
  uint64_t culture = 0;
};

// A wilderness den (L1b; VISION_PLAN 2.5 "Den about 12 per region")
struct DenPlan {
  Gid id = 0;
  int32_t x = 0, y = 0;             // centre (global tile)
  art::Monster mon = art::Monster::Wolf;
  uint8_t pack = 3;
};

struct KingdomPlan {
  Gid id = 0;                       // makeId(kcellX, kcellY, IdKind::Kingdom, 0)
  std::string name;
  uint32_t color = 0, color2 = 0;   // banner colours (rgba)
  uint8_t emblem = 0;
  Gid capital = 0;                  // its capital's SitePlan id (0: wildlands, no capital)
  int32_t gx = 0, gy = 0;           // its seat (capital or cell centre), global tile
  // M3: its culture (the dialect of the family whose cell holds its seat) and its full arms (color / color2 / emblem
  // above stay the field, trim and charge of the same arms, for the M1/M2 painters)
  uint64_t culture = 0;
  cult::Heraldry heraldry;
};

// M1 WORLD lane: roads, rivers and lakes touching a region, for the world map / minimap (the chunks already carry them
// as tiles). Polylines in global tiles.
struct RoadPlan {
  Gid id = 0;
  uint8_t cls = 1;                  // 0 highway (2 wide, Road), 1 road (Road), 2 track / spur to a site (Dirt)
  Gid a = 0, b = 0;                 // the sites it joins (a spur: a = its site, b = 0)
  std::vector<GTile> pts;
};
struct RiverPlan {
  GTile a, b;                       // one straight piece of a river's course
  uint8_t width = 1;                // tiles (1..4)
};
struct LakePlan {
  int32_t x = 0, y = 0, r = 0;      // centre and mean radius (the shore is irregular)
};

// M2: a named natural feature for the world map's labels (VISION_PLAN 11.5 "named ranges and passes", 2.11 Z2 / Z3).
// The WORLD lane decides them per region from the macro fields (one per feature: a range is named once, at its
// highest crest in the region holding it); the VIEW lane draws the label at (x, y) at the zooms that suit its size.
enum class LandmarkKind : uint8_t { Range, Peak, Pass, Lake, River, Forest, Marsh, Desert, Hills, Sea, COUNT };
struct LandmarkPlan {
  Gid id = 0;                       // makeId(rx, ry, IdKind::Poi, 0x800 | n): stable
  LandmarkKind kind = LandmarkKind::Range;
  int32_t x = 0, y = 0;             // where the label belongs (global tile)
  int32_t size = 0;                 // rough extent in tiles (the view hides small ones when zoomed far out)
  std::string name;                 // "THE GREY TEETH", "WOLFGATE PASS", "MIRRORMERE"
};

struct RegionPlan {
  int32_t rx = 0, ry = 0;
  uint16_t genVer = ENDLESS_GEN_VER;
  uint32_t fingerprint = 0;         // hash of site ids, types and positions
  std::vector<SitePlan> sites;      // every site whose heart (ex, ey) lies in this region
  std::vector<DenPlan> dens;
  std::vector<RoadPlan> roads;      // M1: roads whose course touches the region, plus its sites' spurs
  std::vector<RiverPlan> rivers;    // M1: river pieces touching the region
  std::vector<LakePlan> lakes;      // M1: lakes touching the region
  std::vector<LandmarkPlan> landmarks;   // M2: named features whose label point lies in this region
  Geology geology;                  // M2: the geology at the region's centre tile (geology() has it per tile)
};

// A spawn streamed with a chunk: Spawn's x, y are GLOBAL tiles; site is unused (siteId says whose it is)
struct SpawnPlan {
  Spawn sp;
  Gid siteId = 0;
};

// One generated chunk (L2b): 32 x 32 tiles, final ground and props, plus the records of everything that stands on it.
struct ChunkData {
  static constexpr int N = CHUNK * CHUNK;
  int32_t cx = 0, cy = 0;           // global chunk coordinates
  uint8_t ground[N] = {};           // Ground
  uint8_t prop[N] = {};             // art::Prop + 1, 0 = none
  uint8_t biome[N] = {};            // Biome
  uint8_t blend[N] = {};            // M2 biome transitions: Map::blend (bits 0-3 the other biome, 4-7 its weight 0..8)
  uint8_t eco[N] = {};              // M3c: Eco, the biome proper (its family is biome[]); Map::eco
  uint8_t ecoNb[N] = {};            // M3c: the Eco this tile blends toward (blend's weight); == eco where unblended; Map::ecoNb
  uint8_t height[N] = {};           // relief (Map::height bits): level 0..7 in bits 0-2 (VISION_PLAN 11.1), Map::HEIGHT_CLIFF
                                    // = a cliff face (not walkable), Map::HEIGHT_RAMP = a ramp / stairs between levels
  uint8_t wall[N] = {};             // city wall pieces
  uint16_t bldg[N] = {};            // 0 = none, else 1 + index into bldgs (footprints inside this chunk)
  // Records: the WHOLE record list of every site that touches this chunk (all its buildings in the site's order, all
  // its spawns, gates and wall gaps), so a site's buildings always arrive together and World can keep them contiguous
  // (Site::bldgFirst / bldgCount). World dedups by id.
  std::vector<Bldg> bldgs;          // Bldg::r in GLOBAL tiles, Bldg::id set, Bldg::site = -1 (bldgSite holds the owner)
  std::vector<Gid> bldgSite;        // parallel to bldgs
  std::vector<SpawnPlan> spawns;
  std::vector<GTile> gates;         // city gatehouses (left tile of the 3-wide opening)
  std::vector<IRect> wallGaps;      // wall openings (global tiles)
  int at(int lx, int ly) const { return ly * CHUNK + lx; }
};

// The start plan (VISION_PLAN 2.6): where a new game begins and the main quest's places.
struct StartPlan {
  Gid village = 0;                  // the start village
  Gid capital = 0;                  // the city whose jarl gives THE DRAGON'S SHADOW (has a Keep)
  Gid lair = 0;                     // Ashfang's lair
  Gid shards[3] = {0, 0, 0};        // the three ruins holding the Ember Shards
  GTile spawn;                      // where the player stands at the start (global tile)
};

class EndlessSource {
 public:
  explicit EndlessSource(uint64_t seed);
  ~EndlessSource();
  EndlessSource(const EndlessSource&) = delete;
  EndlessSource& operator=(const EndlessSource&) = delete;

  uint64_t seed() const;
  MacroSample macro(int32_t gx, int32_t gy);              // pure (may use a sample cache)
  // M1: a cheap far-zoom sample (world map zoomed out, overviews): the raw macro fields at one point, no tile detail,
  // no rivers or lakes (water = sea only), no relief cleanup. Builds no caches.
  MacroSample macroFar(int32_t gx, int32_t gy);
  // M3c: the eco of a global tile as the chunks have it before settlements and stamps (tile() + the classifier: no
  // rivers, lakes, kingdoms; cheaper than macro()). ecoFar: the far-zoom estimate from the raw macro fields (like
  // macroFar: no warps, no beach band, no caches built), for maps and searches; ecoAt confirms.
  Eco ecoAt(int32_t gx, int32_t gy);
  Eco ecoFar(int32_t gx, int32_t gy);
  // M1: the road bearings handed to a settlement's generator (radians, 0 = east, y down; strongest first)
  std::vector<float> roadBearings(Gid site);
  const RegionPlan& region(int32_t rx, int32_t ry);       // cached; the reference stays valid until the next call
  void chunk(int32_t cx, int32_t cy, ChunkData& out);     // pure; may cache settlement buffers internally
  // Streaming without threads (the web): does up to about budgetMs of the work chunk(cx, cy) needs first (its region
  // plans, then each settlement it touches, built a generator phase at a time and cached). True once chunk(cx, cy) is
  // cheap; false: call again next frame. The result of chunk() is the same either way.
  bool prepareChunk(int32_t cx, int32_t cy, double budgetMs);
  const KingdomPlan* kingdom(Gid id);                      // nullptr for 0 / unknown
  const StartPlan& start();                                // computed once per seed
  int danger(int32_t gx, int32_t gy);                      // VISION_PLAN 7.1: the level of what lives here
  // M2: the geology of the province holding a global tile (rpg/world/geology.h; pure, cheap enough per map pixel at far
  // zooms: it reads the coarse macro fields only)
  Geology geology(int32_t gx, int32_t gy);
  // M2 fast travel's landmass rule (VISION_PLAN 2.11: fast travel stays on the same landmass): an id that is equal for
  // two land tiles joined by land and differs across the sea (0: water). Coarse (judged on the macro sea field, about
  // 16-tile cells), stable for a seed, cheap enough to ask for a journey. PHASE A STUB: 1 for any land; the WORLD lane
  // makes islands and other continents distinct.
  uint32_t landmass(int32_t gx, int32_t gy);

  // M3 culture engine (rpg/culture/culture.h): the culture of a global tile (its kingdom's dialect, else its culture
  // cell's family) and any culture by id. Memoised in this source's own cult::Atlas (one per thread, like the source).
  uint64_t cultureAt(int32_t gx, int32_t gy);
  const cult::Culture& culture(uint64_t id);
  cult::Atlas& atlas();

  // --perf / tests: what generation cost so far
  struct Stats {
    int regions = 0, chunks = 0, settlements = 0;
    double regionMs = 0, chunkMs = 0, settlementMs = 0, maxChunkMs = 0, maxSettlementMs = 0;
    // M1 WORLD lane: river traces (L0.5) and road edges (L1c), cached by spring / edge
    int traces = 0, roadEdges = 0, blocks = 0, rivers = 0, tracesToSea = 0, tracesToLake = 0;   // springs with a river; rivers reaching the sea / a lake
    double traceMs = 0, roadMs = 0, maxRoadMs = 0, maxRegionMs = 0, blockMs = 0;   // blocks: L0 macro samples per region
  };
  const Stats& stats() const;

  struct Impl;   // rpg/world/endless.cpp (WORLD lane)

 private:
  std::unique_ptr<Impl> d_;
};

}  // namespace ew
