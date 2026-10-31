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
#include "rpg/core/art_ids.h"
#include "rpg/build/blueprint.h"
#include "rpg/core/common.h"
#include "rpg/world/map.h"   // Map, the bldg* helpers, propSolid, the name makers
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


// SOLID 2a.3: the records moved to rpg/world/records.h with their layouts unchanged. Sizes as at 4c0c9a1 on a 64-bit
// build whose std::string is 32 bytes and std::vector 24 (MSVC x64 Release, clang/libstdc++ x86_64); MapBits is empty.
static_assert(std::is_empty_v<MapBits>);
static_assert(sizeof(MapKind) == 1 && sizeof(SiteType) == 1 && sizeof(Role) == 1);
static_assert((int)SiteType::COUNT == 10 && (int)Role::COUNT == 27 && WORLDGEN_LATEST == 7);
static_assert(sizeof(void*) != 8 || sizeof(std::string) != 32 || sizeof(std::vector<int>) != 24 ||
              (sizeof(Bldg) == 208 && sizeof(Bldg::Open) == 80 && sizeof(Site) == 136 && sizeof(Kingdom) == 80 &&
               sizeof(Spawn) == 24 && sizeof(Den) == 24 && sizeof(Map) == 480));


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
