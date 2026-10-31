// World data: tile maps (overworld, caves, interiors), sites (cities, towns, caves...), buildings, generation.
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "rpg/art.h"
#include "rpg/build/blueprint.h"
#include "rpg/sim/common.h"
#include "rpg/sim/rooms.h"
#include "rpg/world/records.h"   // MapKind, SiteType, Role, Bldg, Site, Kingdom, Spawn, Den, WORLDGEN_*, MapBits
#include "rpg/world/biomes.h"
#include "rpg/world/coords.h"
#include "rpg/world/ids.h"
#include "rpg/world/plots.h"

namespace cult { struct Culture; }   // rpg/culture/culture.h (M3)
namespace ew {   // rpg/world/source.h (the endless generator, M1)
class EndlessSource;
struct SitePlan;
struct RegionPlan;
struct ChunkData;
}
class ChunkStreamer;   // rpg/sim/stream.h (SIM lane, M1): prefetching chunks off the main thread

// Ground, Biome (the family) and Eco (the biome proper, M3c) live in rpg/world/biomes.h.

const char* siteTypeName(SiteType t);

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

// the name a building type goes by in the HUD ("INN", "THE KEEP", "THE PALACE"...)
const char* bldgTypeName(art::Building t);

// M0b: the exterior's style (what the view paints) and its art facts, so interiors can match their outside.
inline art::ArchStyle bldgArch(const Bldg& b) {
  if (b.styled) return b.arch;
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
  f.charred = b.charred;
  return f;
}
// M3b: the builder's request for a building, and its blueprint (rpg/build/blueprint.h). Every view, interior and test
// that needs a building's look or plan goes through these (no building bypasses the builder).
inline bld::Request bldgRequest(const Bldg& b) {
  bld::Request r = bld::simpleRequest(b.type, b.r.w, b.r.h, bldgArch(b), b.seed, bldgFacts(b));
  r.wealth = b.wealth;
  r.urban = b.urban;
  r.form = (bld::Form)(b.form < (uint8_t)bld::Form::COUNT ? b.form : 0);
  r.civic = b.civic;
  r.seat = b.seat;
  r.home = (uint8_t)(b.home == 1 || b.home == 3 ? 1 : 0);   // (M7) the player's own house (a bought one keeps its look)
  r.row = b.row;                                            // (M7b) a row house's party walls
  return r;
}
inline bld::Blueprint bldgBlueprint(const Bldg& b) { return bld::design(bldgRequest(b)); }
// (owner 2026-10-06) open fronts: the building's entry tiles and pillars (Bldg::open, made on demand from the blueprint
// and cached). bldgEntryAt: is tile (x, y) (the Bldg's map frame) one of its entry tiles (the door, or a walk-in bay of
// an open front). bldgPillarSolid: on an entry tile, does the point (px, py) in map pixels hit a pillar or the wall
// beyond the open span (the only parts of an open front that block). bldgEntryColumns: the entry columns, west to east.
const Bldg::Open& bldgOpenFront(const Bldg& b);
const Bldg::Open& bldgOpenFront(const Bldg& b, const bld::Blueprint& bp);
bool bldgEntryAt(const Bldg& b, int x, int y);
bool bldgPillarSolid(const Bldg& b, float px, float py);
std::vector<int> bldgEntryColumns(const Bldg& b);
// M3b: the seat of power, whatever the society builds it as (a castle keep, a great hall, a tent court, a temple
// complex, a guildhall, a council spire...): game code finds the ruler's and the lord's seats by these, never by type.
// The royal seat holds the king (Role::King) on its ground floor; a city's seat holds its lord (Role::Jarl).
inline bool bldgIsSeat(const Bldg& b) {
  return (b.civic & bld::CIVIC_SEAT) != 0 || b.type == art::Building::Palace || b.type == art::Building::Keep;
}
inline bool bldgIsRoyalSeat(const Bldg& b) {
  return b.type == art::Building::Palace || ((b.civic & bld::CIVIC_SEAT) != 0 && b.urban >= 3);
}
// WORLDGEN_V7 decisions (rpg/sim/world.cpp), deterministic from the type, the footprint and a hash:
int bldgStoreysV7(art::Building t, int wTiles, int hTiles, uint32_t h);
bool bldgHearthV7(art::Building t, int storeys, uint32_t h);

// A placed object on a tile (props layer). Buildings are separate.
struct Map : MapBits {   // the bit constants (PAVE_MARK, HEIGHT_*...): rpg/world/records.h
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
  // M2 biome transitions (VISION_PLAN 11.6), endless overworld only (else empty): bits 0-3 the neighbouring biome
  // (Biome) this tile blends toward, bits 4-7 its weight 0..15 (0: pure own biome, 8: half and half; never above 8, a
  // tile past the midpoint belongs to the other biome). The generator writes it (ChunkData::blend); the view dithers
  // the two biomes' ground and the generator ramps transition flora with it.
  // (M3 fixer) on a settlement's own tiles (never an ecotone: town ground keeps its own look) the byte is a paving mark
  // instead: PAVE_MARK | the culture's TownStyle::paving (0..15), or BOARDWALK_MARK | paving on a stilt town's
  // boardwalk (a Bridge over its marsh, not a river bridge). Ecotone readers take weights 1..8 only.
  // (M3b fixer) POOL_MARK | paving on the open water of a seat's grounds laid by hand (pools, canals, rills): the view
  // draws them in cut stone kerbs, never with a pond's organic banks.
  std::vector<uint8_t> blend;
  // M3c Wildlands (rpg/world/biomes.h), endless overworld only (else empty): eco = the biome proper (Eco; its family is
  // the biome byte), ecoNb = the Eco this tile blends toward (with blend's weight; == eco where the weight is 0 or the
  // byte is a PAVE / BOARDWALK / POOL mark). Written by the generator (ChunkData::eco / ecoNb), never saved.
  std::vector<uint8_t> eco, ecoNb;
  std::vector<uint8_t> height;   // M1: relief per tile (endless overworld only, else empty): bits 0..2 the level 0..7
                                 // (VISION_PLAN 11.1), HEIGHT_CLIFF a cliff face (not walkable), HEIGHT_RAMP a ramp or
                                 // stairs between levels (walkable). The view draws faces where the level steps down.
                                 // M11 High Ground (VISION_PLAN 11.2 / 11.3 / 15.32) adds three bits; every step bit
                                 // (CLIFF, RAMP, SLOPE) sits on the LOWER tile of a 4-neighbour level step:
                                 //   HEIGHT_SLOPE a gentle 1-level step (a hillside): walkable at reliefSpeed's 0.8,
                                 //                drawn as a shaded hillside band, never with CLIFF or RAMP
                                 //   HEIGHT_FALL  a water tile a river (or a plateau lake's spill) pours over a step
                                 //                into (always with CLIFF): an animated waterfall; a wall to walkers,
                                 //                swimmers and boats alike
                                 //   HEIGHT_STAIR with RAMP: a built way up (stone stairs: roads, tracks, towns, a summit's
                                 //                trail); a RAMP without it is a worn earth ramp
  // M7b Cities (rpg/world/urban.h): the urban fabric per tile (endless overworld and settlement buffers only, else
  // empty): the street class (avenue, street, lane, alley, sidewalk, gutter, square, close, park, garden, court, quay),
  // the upkeep and the quarter. Written by the settlement generator (cities; a town's main street and square), always
  // 0 in villages; the view draws kerbs, sidewalks, drains and the quarters' paving from it. Regenerated, never saved.
  std::vector<uint8_t> urb;
  std::vector<Bldg> bldgs;
  int exitX = 0, exitY = 0;      // caves/interiors: tile you appear on when entering (and where the exit is); upper
                                 // floors of a building have no exit (-1)
  std::vector<int16_t> exits;    // (owner 2026-10-06) a building walked into between pillars, its ground floor: the
                                 // ways out on row exitY, west to east (exitX among them); empty: the one door at exitX
  bool exitOpen = false;         // exitX is an open bay too (an open entrance); false: exitX is a door
  bool isExit(int x, int y) const {
    if (y != exitY || exitY < 0) return false;
    if (x == exitX) return true;
    for (int16_t e : exits) if (e == x) return true;
    return false;
  }
  // M0b interiors (WORLDGEN_V7+, rooms.h): which floor of its building this map is, its stairs, and its rooms
  int floor = 0;
  uint8_t kit = 0;               // (M3 fixer) interiors: 1 + the cult::Archetype whose own furniture the view draws
                                 // (art::cultureInteriorPiece), 0 the classic kit. Regenerated with the map, never saved
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
  // M11: the walking speed factor of a relief tile (VISION_PLAN 11.2): a hillside 0.8, an earth ramp 0.8, stone stairs
  // 0.9, anything else 1. The SIM lane applies it (the player, NPCs, companions, mounts); this is its one definition.
  static float reliefSpeed(uint8_t bits) {
    if (bits & HEIGHT_SLOPE) return 0.8f;
    if (bits & HEIGHT_RAMP) return (bits & HEIGHT_STAIR) ? 0.9f : 0.8f;
    return 1.0f;
  }
  int heightAt(int x, int y) const { return in(x, y) && !height.empty() ? (height[(size_t)y * w + x] & HEIGHT_LEVEL) : 0; }
  uint8_t heightBits(int x, int y) const { return in(x, y) && !height.empty() ? height[(size_t)y * w + x] : 0; }
  float reliefSpeedAt(int x, int y) const { return reliefSpeed(heightBits(x, y)); }
  bool fallAt(int x, int y) const { return (heightBits(x, y) & HEIGHT_FALL) != 0; }   // M11: a waterfall tile
  Biome biomeAt(int x, int y) const { return in(x, y) && !biome.empty() ? (Biome)biome[(size_t)y * w + x] : Biome::Plains; }
  uint8_t blendAt(int x, int y) const { return in(x, y) && !blend.empty() ? blend[(size_t)y * w + x] : 0; }
  uint8_t urbAt(int x, int y) const { return in(x, y) && !urb.empty() ? urb[(size_t)y * w + x] : 0; }   // M7b (rpg/world/urban.h)
  // M3c: the biome proper (a map without the layer: its family's classic eco) and the eco it blends toward
  Eco ecoAt(int x, int y) const { return in(x, y) && !eco.empty() ? (Eco)eco[(size_t)y * w + x] : ecoOfFamily(biomeAt(x, y)); }
  Eco ecoNbAt(int x, int y) const { return in(x, y) && !ecoNb.empty() ? (Eco)ecoNb[(size_t)y * w + x] : ecoAt(x, y); }
  void alloc(int w_, int h_, Ground fill);
  void rebuildSolid();
};

// SOLID 2a.3: the records moved to rpg/world/records.h with their layouts unchanged. Sizes as at 4c0c9a1 on a 64-bit
// build whose std::string is 32 bytes and std::vector 24 (MSVC x64 Release, clang/libstdc++ x86_64); MapBits is empty.
static_assert(std::is_empty_v<MapBits>);
static_assert(sizeof(MapKind) == 1 && sizeof(SiteType) == 1 && sizeof(Role) == 1);
static_assert((int)SiteType::COUNT == 10 && (int)Role::COUNT == 27 && WORLDGEN_LATEST == 7);
static_assert(sizeof(void*) != 8 || sizeof(std::string) != 32 || sizeof(std::vector<int>) != 24 ||
              (sizeof(Bldg) == 208 && sizeof(Bldg::Open) == 80 && sizeof(Site) == 136 && sizeof(Kingdom) == 80 &&
               sizeof(Spawn) == 24 && sizeof(Den) == 24 && sizeof(Map) == 480));

bool propSolid(art::Prop p);   // rpg/sim/prop_rules.cpp

struct World {
  uint64_t seed = 0;
  int genVersion = WORLDGEN_LATEST;   // the interior / dungeon generator version its records carry (M2: always latest)
  // over: the Active Window onto the endless world (VISION_PLAN 2.9): a WIN x WIN tile window onto the endless land
  // whose local (0,0) is global tile (ox, oy). Everything in World and Game keeps using
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
  std::vector<Kingdom> kingdoms;            // M1: every kingdom met so far

  // ---- M1 endless world (rpg/sim/world_endless.cpp). M2: every World is endless once generated (a default-constructed
  // World, before generateEndless, has endless == false and no map).
  static constexpr int WIN = 256;           // window size in tiles (8 x 8 chunks)
  static constexpr int WIN_SHIFT = 64;      // the window moves in steps of this many tiles
  bool endless = false;
  int32_t ox = 0, oy = 0;                   // global tile of over's (0,0); always a multiple of ew::CHUNK
  std::shared_ptr<ew::EndlessSource> src;   // the generator (shared: copies of a World share one cache)
  std::unordered_map<ew::Gid, int> siteById, bldgById, denById, kingdomById;   // Gid -> handle (index)
  std::unordered_set<uint64_t> spawnKeys, gateKeys;   // streamed spawns (site id ^ slot) and gates already added
  int windowShifts = 0;                     // how many times the window has moved (tests, perf)
  // M7 Home: bumped whenever the window's tiles were (re)written from the generator (placeWindow, streamChunk with
  // tiles): the player's plots must be stamped again (home::stampWindow, called by Game::homeStep when it changed)
  uint32_t placeSerial = 0;
  // M7 Home: the settlements' lots met so far (ew::PlotPlan, GLOBAL tiles; append-only, deduped by id; never saved:
  // regenerated with their sites' chunks). lotHandle: index by id (-1 none)
  std::vector<ew::PlotPlan> lots;
  std::unordered_map<ew::Gid, int> lotById;
  int lotHandle(ew::Gid id) const { auto it = lotById.find(id); return it == lotById.end() ? -1 : it->second; }
  // ---- M1 SIM lane: streaming without hitches, bounded records, spatial look-ups (world_endless.cpp)
  // the prefetcher (stream.h): natively initEndless creates it with the world so its worker warms up while the window
  // is built (M3); on the web (and when a test drops it) Game::prefetchTick creates it on first use; prefetchTick
  // keeps its wish list current; placeWindow takes ready chunks from it and generates the rest itself
  std::shared_ptr<ChunkStreamer> streamer;
  // (fixer M6b r1) the region plans this many regions round the player's are wished for too (after the window's own),
  // so the story composer's and caster's look-ups (caves, camps, ruins up to ~640 tiles from a hook) find them ready
  // instead of building them in the frame a teller is talked to. 0: none (tests may lower it)
  int storyRing = 3;
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
  // M4 Banners: a realm-made kingdom that has no generator plan (a rebel realm after a civil war, a warlord's realm;
  // VISION_PLAN 4.3 "Rise"): added (or updated) by id with its own name and arms. Returns its handle.
  int addKingdomRecord(const Kingdom& k);
  // M4 Banners: the site now belongs to kingdom `owner` (0: nobody, an independent / abandoned place): Site::kingdom
  // follows, and so do the banners its buildings fly (Bldg::banner / banner2 / emblem of every building that flies
  // one). Cheap; the realm hook (Game::realmStep) calls it when a SettlementState's owner differs from the site's.
  void setSiteOwner(int site, ew::Gid owner);
  // M3 culture engine: a site's culture (nullptr: no generator, or a culture-less site), a kingdom's, a window tile's
  // (its kingdom's dialect, else its culture cell's family). Cheap after the first call per culture (memoised).
  const cult::Culture* cultureOf(int site) const;
  const cult::Culture* cultureOfKingdom(int kingdom) const;
  const cult::Culture* cultureAtTile(int tx, int ty) const;
  const Kingdom* kingdomOf(int site) const {
    return site >= 0 && site < (int)sites.size() && sites[(size_t)site].kingdom >= 0 ? &kingdoms[(size_t)sites[(size_t)site].kingdom] : nullptr;
  }

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
