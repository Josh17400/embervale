// Internals of the endless generator (EndlessSource::Impl), shared by rpg/world/{macro,hydro,region,roads,chunkgen}.cpp.
// WORLD lane. Nothing outside rpg/world includes this header; other lanes use rpg/world/source.h.
//
// Layering (each layer reads only the pure layers below it, so nothing depends on which chunk or region was asked for
// first; every cache below holds pure values and only saves time):
//   L0    macro.cpp    continents, plates / ridges / passes, elevation, climate, biome, relief level (coarse 8-tile
//                      samples cached per region-sized block, bilinear per tile)
//   L0.5  hydro.cpp    lake lattice, spring-traced rivers on a 32-tile grid (traces cached by spring), per-region index
//   L1a   region.cpp   kingdom cells and capitals, the start plan, settlement lattices (pure candidates)
//   L1c   roads.cpp    Gabriel graph over settlements, corridor A* on an 8-tile grid, Chaikin, rasterisation
//   L1b   region.cpp   region plans: settlements, POIs (caves on real cliffs, ruins, camps, shrines, lairs), dens, spurs
//   L2b   chunkgen.cpp the chunk pipeline: base + rivers + lakes, settlement buffers, site stamps, roads, relief,
//                      vegetation
// Determinism: integer / Q16 maths only for anything structural (the only floats are road bearings handed to the
// towns lane, computed with datan2 from integers). No unordered_map iteration feeds an output.
#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "rpg/world/dmath.h"
#include "rpg/world/noise.h"
#include "rpg/world/settlement.h"
#include "rpg/world/source.h"

namespace ew {
namespace gen {

using Clock = std::chrono::steady_clock;
inline double msSince(Clock::time_point t0) { return std::chrono::duration<double, std::milli>(Clock::now() - t0).count(); }

constexpr int32_t Q(double v) { return (int32_t)(v * 65536.0); }   // compile-time Q16 constants only
inline int32_t qm(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 16); }
inline int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
// smoothstep from a to b (Q16 in, Q16 [0, 1] out)
inline int32_t sstep(int32_t a, int32_t b, int32_t x) {
  if (x <= a) return 0;
  if (x >= b) return Q_ONE;
  return smoothQ((int32_t)(((int64_t)(x - a) << 16) / (b - a)));
}
// value noise [0, 1) -> about [-1, 1] with gain (fbm of value noise clusters around 0.5)
inline int32_t centered(int32_t f, int32_t gain) { return clampi(qm(f - 32768, gain) * 2, -Q_ONE, Q_ONE); }
inline int32_t hq(uint64_t h) { return (int32_t)(h >> 48); }
inline uint64_t tileHash(uint64_t s, int32_t x, int32_t y) { return mix64(s ^ (((uint64_t)(uint32_t)x << 32) | (uint32_t)y)); }
inline uint64_t key2(int32_t x, int32_t y) { return ((uint64_t)(uint32_t)x << 32) | (uint32_t)y; }
inline int64_t dist2(int32_t ax, int32_t ay, int32_t bx, int32_t by) {
  int64_t dx = (int64_t)ax - bx, dy = (int64_t)ay - by;
  return dx * dx + dy * dy;
}
inline int32_t idist(int32_t ax, int32_t ay, int32_t bx, int32_t by) { return (int32_t)isqrt((uint64_t)dist2(ax, ay, bx, by)); }
inline bool isSettlement(SiteType t) { return t == SiteType::City || t == SiteType::Town || t == SiteType::Village; }
// local ids of the start plan's sites (makeStart): settlements and first-hour sites 0xF00.., forced vignettes 0xF10..,
// a forced wonder 0xF20
constexpr uint32_t LOCAL_FORCED_VIG = 0xF10, LOCAL_FORCED_WONDER = 0xF20;

// ---- relief levels (VISION_PLAN 11.1): H = clamp((E - H_BASE) / H_STEP, 0, 7)
constexpr int32_t H_BASE = Q(0.335), H_STEP = Q(0.068);
inline int levelOf(int32_t e) { return e < H_BASE ? 0 : std::min(7, (int)((e - H_BASE) / H_STEP)); }
inline int32_t levelFloor(int lv) { return H_BASE + lv * H_STEP; }   // lowest E of level lv (lv >= 1)

// ---- L0: one coarse sample (every 16 tiles), raw fields in Q16
constexpr int CG_SHIFT = 4;
constexpr int CG = 1 << CG_SHIFT;            // coarse grid spacing in tiles (16)
constexpr int BLK = REGION / CG;             // 16 samples per block side (a block covers one region)
constexpr int HC = 8, HN = REGION / HC;      // the per-region river / lake index: 8-tile cells, 32 per side
constexpr int BS = BLK + 3;                  // stored side: samples -1 .. BLK + 1
struct Coarse {
  int32_t e = 0;      // elevation
  int32_t c = 0;      // continentalness
  int32_t t = 0, m = 0;
  int32_t ridge = 0;  // ridge strength 0..1 (rock cores, mining, lairs)
  int32_t rock = 0;   // rockiness 0..1 (Ground::Rock where the bilinear value passes 0.5)
};
struct Block {
  Coarse s[BS * BS];
  int32_t eClean[BS * BS];   // elevation with single-sample relief peaks / pits removed (relief levels read this)
  const Coarse& at(int i, int j) const { return s[(j + 1) * BS + (i + 1)]; }
  int32_t clean(int i, int j) const { return eClean[(j + 1) * BS + (i + 1)]; }
};

// one tile of the base land (L0 + biome; rivers and lakes come from hydro)
struct TileF {
  int32_t e = 0, t = 0, m = 0, ridge = 0;
  Biome biome = Biome::Plains;
  uint8_t h = 0;       // natural relief level (before any flattening)
  bool sea = false;
  bool rock = false;
};

// ---- L0.5
// (M2) river: the river this piece belongs to (riverIdOf: its spring cell; where traces merge, the longest one's), for
// names (landmarks, toll bridges)
struct RiverSeg { int32_t x0, y0, x1, y1; uint8_t w; uint32_t river = 0; };
inline uint32_t riverIdOf(int32_t cx, int32_t cy) { return ((uint32_t)(cx & 0xFFFF) << 16) | (uint32_t)(cy & 0xFFFF); }
inline int32_t riverCellX(uint32_t id) { return (int32_t)(int16_t)(uint16_t)(id >> 16); }
inline int32_t riverCellY(uint32_t id) { return (int32_t)(int16_t)(uint16_t)(id & 0xFFFF); }
struct Lake { int32_t x = 0, y = 0, r = 0; uint32_t seed = 0; };
struct Trace {
  std::vector<RiverSeg> segs;
  std::vector<Lake> lakes;     // lakes the river fills on its way (pits)
  int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;   // bounding box (tiles, including widths and lakes)
};
// a lake's shore radius toward tile (x, y): lobed and irregular (bays and points), never more than 1.6 r
inline int32_t lakeRadius(const Lake& L, int32_t x, int32_t y) {
  int32_t n1 = vnoiseQ(x, y, 4, mix64(L.seed)) - 32768, n2 = vnoiseQ(x, y, 2, mix64(L.seed ^ 0x77u)) - 32768;
  return L.r + (int32_t)((int64_t)n1 * L.r * 9 / 10 / 32768) + (int32_t)((int64_t)n2 * L.r / 6 / 32768);
}
struct RegionHydro {
  std::vector<RiverSeg> segs;          // river segments touching the region (+ margin)
  std::vector<Lake> lakes;             // lakes touching it
  uint8_t riverW[HN * HN] = {};        // max river width per 8-tile cell (roads, archetypes, sites)
  uint8_t lakeCell[HN * HN] = {};      // 1 = a lake covers the cell centre
};

// ---- L1: settlement nodes (pure lattice candidates that survived suppression, plus the start plan's forced sites)
struct Node {
  Gid id = 0;
  SiteType type = SiteType::Village;
  int32_t x = 0, y = 0;     // heart (global tile)
  uint32_t seed = 0;
  uint16_t flags = 0;
};
// nominal radius of a settlement class (planning only; the towns lane's footprint is the real extent)
inline int32_t nominalR(SiteType t) { return t == SiteType::City ? 100 : t == SiteType::Town ? 60 : 40; }

struct Edge {
  uint64_t key = 0;
  Gid a = 0, b = 0;
  SiteType ta = SiteType::Village, tb = SiteType::Village;
  uint8_t cls = 1;                 // 0 highway (2 wide), 1 road, 2 track (spur)
  std::vector<GTile> pts;          // polyline (tiles), from a to b
  int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;   // bbox
};

struct RegionData {
  RegionPlan plan;
  std::vector<std::shared_ptr<const Edge>> edges;       // graph roads touching the region (+32)
  std::unordered_map<Gid, std::vector<float>> bearings;  // settlement id -> road bearings (strongest first)
  std::vector<std::pair<int32_t, int32_t>> flatLevel;    // per plan.sites index: (flatten level, -1 none)
};

// a small LRU: shared_ptr values, so a caller may keep an entry alive across later calls
template <class V>
struct Lru {
  size_t cap = 64;
  uint64_t clock = 0;
  std::unordered_map<uint64_t, std::pair<std::shared_ptr<V>, uint64_t>> m;
  std::shared_ptr<V> get(uint64_t k) {
    auto it = m.find(k);
    if (it == m.end()) return nullptr;
    it->second.second = ++clock;
    return it->second.first;
  }
  void put(uint64_t k, std::shared_ptr<V> v) {
    if (m.size() >= cap) {
      // evict the oldest eighth in one go (amortised)
      std::vector<uint64_t> ages;
      ages.reserve(m.size());
      for (auto& kv : m) ages.push_back(kv.second.second);
      size_t n = std::max<size_t>(1, m.size() / 8);
      std::nth_element(ages.begin(), ages.begin() + (n - 1), ages.end());
      uint64_t cut = ages[n - 1];
      for (auto it = m.begin(); it != m.end();) it = it->second.second <= cut ? m.erase(it) : std::next(it);
    }
    m[k] = {std::move(v), ++clock};
  }
};

// chunk stamping helper: writes into the chunk, clipped
struct Stamp {
  ChunkData& c;
  int32_t x0, y0;
  std::vector<uint8_t>& reserved;
  bool in(int32_t gx, int32_t gy) const { return gx >= x0 && gy >= y0 && gx < x0 + CHUNK && gy < y0 + CHUNK; }
  int idx(int32_t gx, int32_t gy) const { return (gy - y0) * CHUNK + (gx - x0); }
  void g(int32_t gx, int32_t gy, Ground v) { if (in(gx, gy)) c.ground[idx(gx, gy)] = (uint8_t)v; }
  void p(int32_t gx, int32_t gy, art::Prop v) { if (in(gx, gy)) c.prop[idx(gx, gy)] = (uint8_t)((int)v + 1); }
  void clear(int32_t gx, int32_t gy) { if (in(gx, gy)) c.prop[idx(gx, gy)] = 0; }
  void reserve(int32_t gx, int32_t gy) { if (in(gx, gy)) reserved[(size_t)idx(gx, gy)] = 1; }
  void biome(int32_t gx, int32_t gy, Biome b) { if (in(gx, gy)) c.biome[idx(gx, gy)] = (uint8_t)b; }
  Ground at(int32_t gx, int32_t gy) const { return in(gx, gy) ? (Ground)c.ground[idx(gx, gy)] : Ground::Void; }
};

// the base land of a rectangle (steps 1 and 2 of the chunk pipeline): ground, biome, natural relief level, water
struct BaseRect {
  int32_t x0 = 0, y0 = 0;
  int w = 0, h = 0;
  std::vector<uint8_t> ground, biome, level, riverW;   // riverW: 0 none, else the river's width (water tiles)
  std::vector<uint8_t> lake;                           // 1 = lake water
  std::vector<uint8_t> bridge;                         // 1 = a footbridge across a river
  bool in(int32_t gx, int32_t gy) const { return gx >= x0 && gy >= y0 && gx < x0 + w && gy < y0 + h; }
  size_t at(int32_t gx, int32_t gy) const { return (size_t)(gy - y0) * w + (gx - x0); }
};

}  // namespace gen

struct EndlessSource::Impl {
  using Coarse = gen::Coarse;
  uint64_t seed;
  Stats stats;
  explicit Impl(uint64_t s);
  // time spent building cached things (regions, hydrology, towns) nested inside another call: chunk() reports its own
  // cost without them
  int nestDepth = 0;
  double nestedMs = 0;
  struct Nest {
    Impl& I;
    gen::Clock::time_point t0;
    explicit Nest(Impl& i) : I(i) { if (I.nestDepth++ == 0) t0 = gen::Clock::now(); }
    ~Nest() { if (--I.nestDepth == 0) I.nestedMs += gen::msSince(t0); }
  };

  // ================================================================ L0 (macro.cpp)
  int32_t continent(int32_t x, int32_t y);                        // C, Q16
  // ridge strength 0..1, rift 0..1; (M2) pair: the two nearest plate cells (a ridge system / rift is one pair), packed
  void plates(int32_t x, int32_t y, int32_t& ridge, int32_t& rift, uint64_t* pair = nullptr);
  struct PlatePt { int32_t x, y; uint64_t h; };
  PlatePt platePoint(int32_t cx, int32_t cy);                       // a plate cell's point and hash
  int32_t elevation(int32_t x, int32_t y, int32_t* cOut, int32_t* ridgeOut, int hillOct = 4);   // E (Q16), no climate
  Coarse coarse(int32_t x, int32_t y);                            // all fields at a tile (pure, slow)
  std::shared_ptr<const gen::Block> block(int32_t bx, int32_t by);
  // per-tile fields: bilinear from the blocks at warped coordinates (pure)
  gen::TileF tile(int32_t x, int32_t y);
  int natLevel(int32_t x, int32_t y);                             // relief level only (fast path)
  int32_t waterE(int32_t x, int32_t y);                           // the elevation tile() decides the sea with
  Biome classify(int32_t e, int32_t t, int32_t m, int32_t x, int32_t y, bool sea);
  Ground groundFor(Biome b, int32_t e, int32_t t, int32_t x, int32_t y);
  MacroSample macro(int32_t x, int32_t y);                        // the public sample
  gen::Lru<gen::Block> blocks;
  std::shared_ptr<const gen::Block> lastBlock;
  int32_t lastBx = INT32_MIN, lastBy = INT32_MIN;

  // ================================================================ L0.5 (hydro.cpp)
  int32_t hydroE(int32_t i, int32_t j);                            // routing elevation on the 32-tile grid
  bool latticeLake(int32_t cx, int32_t cy, gen::Lake& out);
  bool latticeLakeRaw(int32_t cx, int32_t cy, gen::Lake& out);
  bool lakeNear(int32_t x, int32_t y, int32_t pad);                // a lattice lake within pad of (x, y)?
  bool spring(int32_t cx, int32_t cy, int32_t& x, int32_t& y);
  std::shared_ptr<const gen::Trace> trace(int32_t cx, int32_t cy);
  std::shared_ptr<const gen::RegionHydro> hydro(int32_t rx, int32_t ry);
  int riverWidthCell(int32_t x, int32_t y);                        // river width in the 8-tile cell holding (x, y)
  bool waterAt(int32_t x, int32_t y);                               // sea, lake or river at a tile (slowish)
  std::unordered_map<uint64_t, int32_t> hydroMemo;
  std::unordered_map<uint64_t, std::pair<bool, gen::Lake>> lakeMemo;
  // a lake that would lie under a settlement is not made (the town is built on dry land; rivers still run through)
  bool lakeUnderSettlement(const gen::Lake& L);
  std::unordered_map<uint64_t, bool> lakeTownMemo;
  gen::Lru<gen::Trace> traces;
  gen::Lru<gen::RegionHydro> hydros;

  // ================================================================ L1 (region.cpp)
  struct KCell { bool capital = false; int32_t x = 0, y = 0; uint32_t seed = 0; Gid id = 0; };
  KCell kcell(int32_t kx, int32_t ky);                              // the kingdom cell's capital (pure, memoised)
  Gid kingdomAt(int32_t x, int32_t y);                              // whose land (0: wildlands)
  const KingdomPlan* kingdom(Gid id);
  int habitability(SiteType t, int32_t x, int32_t y, int* levelOut);   // 0..256 (0: not buildable)
  struct Cand { bool ok = false; int32_t x = 0, y = 0; uint32_t seed = 0; };
  Cand settleCand(SiteType t, int32_t cx, int32_t cy);              // raw lattice candidate (town / village)
  bool settleNode(SiteType t, int32_t cx, int32_t cy, gen::Node& out);   // the candidate, if it survives suppression
  void nodesIn(int32_t x0, int32_t y0, int32_t x1, int32_t y1, std::vector<gen::Node>& out);   // every settlement
  void makeStart();
  int danger(int32_t x, int32_t y);
  std::string siteName(const gen::Node& n);                         // unique within ~3x3 regions
  // POIs
  struct Poi { bool ok = false; SiteType type = SiteType::Cave; int32_t x = 0, y = 0; uint32_t seed = 0; };
  Poi poiRaw(SiteType t, int32_t cx, int32_t cy);                   // raw candidate (before roads / suppression)
  bool caveSpot(int32_t px, int32_t py, uint32_t seed, int32_t& ox, int32_t& oy);
  bool lairOf(int32_t lx, int32_t ly, int32_t& ox, int32_t& oy, uint32_t& sd);   // per 2x2 kingdom cells
  std::shared_ptr<const gen::RegionData> regionData(int32_t rx, int32_t ry);
  void buildRegion(int32_t rx, int32_t ry, gen::RegionData& D);
  std::unordered_map<uint64_t, KCell> kcells;
  std::unordered_map<uint64_t, Cand> cands;
  std::unordered_map<uint64_t, std::pair<bool, gen::Node>> nodeMemo;
  std::unordered_map<uint64_t, Poi> poiMemo;
  std::unordered_map<uint64_t, std::pair<bool, Poi>> lairMemo;
  std::unordered_map<Gid, KingdomPlan> kingdoms;
  gen::Lru<gen::RegionData> regions;
  std::shared_ptr<const gen::RegionData> lastRegion;   // keeps region()'s reference valid until the next call
  bool started = false, starting = false;
  StartPlan sp;
  std::vector<gen::Node> forced;      // the start plan's settlements (start village, story city)
  std::vector<SitePlan> forcedSites;  // and its other sites (shard ruins, the lair, first-hour POIs), positions only
  std::vector<DenPlan> forcedDens;    // a den near the start

  // ================================================================ L1c (roads.cpp)
  void graphEdges(int32_t x0, int32_t y0, int32_t x1, int32_t y1, std::vector<std::shared_ptr<const gen::Edge>>& out);
  std::shared_ptr<const gen::Edge> edge(const gen::Node& a, const gen::Node& b);
  bool roadPath(int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t corridor, std::vector<GTile>& out);
  gen::Lru<gen::Edge> edges;

  // ================================================================ M2 geology (geology.cpp)
  Geology geology(int32_t x, int32_t y);                            // the province's (memoised per province)
  std::unordered_map<uint64_t, Geology> geoMemo;
  uint32_t landmass(int32_t x, int32_t y);                          // the landmass rule (geology.cpp)
  std::unordered_map<uint64_t, uint32_t> landMemo;                  // 128-tile land cell -> landmass id

  // ================================================================ M2 wayside places and wonders (vignettes.cpp)
  // a vignette of one kind at a heart (and the layout direction the plan chose); false where the land does not take it
  bool vignetteAt(VignetteKind k, int32_t x, int32_t y, int dir, uint32_t sd, SitePlan& out, bool relaxed);
  bool waterAtPlan(int32_t x, int32_t y);                           // a river, lake or the sea, from the plans only
  void planVignettes(int32_t rx, int32_t ry, gen::RegionData& D, const std::vector<gen::Node>& nodes,
                     const std::vector<std::shared_ptr<const gen::Edge>>& near);
  void stampVignette(const SitePlan& p, gen::Stamp& S);
  void stampWonder(const SitePlan& p, gen::Stamp& S);
  // the wonder of a wonder-lattice cell (pure, memoised); wonders near the start plan's forced one stand down
  bool wonderOf(int32_t i, int32_t j, SitePlan& out);
  bool wonderSpot(int32_t x, int32_t y, uint32_t sd, SitePlan& out);   // a wonder that fits this spot
  bool nearWonder(int32_t x, int32_t y, int32_t r);                 // a lattice or forced wonder within r
  std::unordered_map<uint64_t, std::pair<bool, SitePlan>> wonderMemo;
  bool settleZone(const std::vector<gen::Node>& nodes, int32_t x, int32_t y, int32_t pad);   // inside a settlement + pad
  void forceStartPois();                                            // makeStart: the start guarantee (VISION_PLAN 2.6)
  std::string riverName(uint32_t river);
  std::string vignetteTitle(VignetteKind k, int32_t x, int32_t y, uint32_t sd, uint32_t river, int instance);
  std::string wonderTitle(WonderKind k, int32_t x, int32_t y, uint32_t sd, bool forcedOne = false);
  // M2 landmarks (landmarks.cpp): named ranges, peaks, passes, lakes, rivers, forests, marshes, deserts, hills, seas
  void planLandmarks(int32_t rx, int32_t ry, gen::RegionData& D);
  struct AreaLabels;                                                // the biome areas of a landmark cell (memoised)
  std::unordered_map<uint64_t, std::shared_ptr<AreaLabels>> areaMemo;
  std::shared_ptr<AreaLabels> areaLabels(int32_t i, int32_t j);
  struct RangeCrest { bool ok = false; int32_t x = 0, y = 0; int32_t e = 0; uint32_t key = 0; };
  std::unordered_map<uint64_t, RangeCrest> crestMemo;
  RangeCrest rangeCrest(int32_t cellX, int32_t cellY);              // the highest crest of a ridge cell
  // M2 relief helpers
  int32_t reliefE(int32_t x, int32_t y);                            // the warped clean elevation natLevel reads
  Biome biomeLite(int32_t x, int32_t y);                            // tile()'s land biome without the beach / rock

  // ================================================================ M3 cultures (culture_map.cpp, CULTURE lane)
  // Where each culture lives on the land, and the names it gives things. The atlas (rpg/culture) holds the cultures.
  std::unique_ptr<cult::Atlas> atlasPtr;
  cult::Atlas& atlas();
  uint64_t kingdomCulture(int32_t kx, int32_t ky);                   // the dialect of a kingdom cell (0: no kingdom)
  uint64_t cultureAt(int32_t x, int32_t y);                          // its kingdom's dialect, else its cell's family
  std::string placeBaseName(const gen::Node& n);                     // a settlement's name before siteName de-duplicates
  std::string kingdomBaseName(int32_t kx, int32_t ky, uint32_t seed); // a kingdom's name
  // a cave / ruin / camp / shrine / lair's name (Rng nr: the caller's name stream, kept so the M2 names can stay)
  std::string poiName(Rng& nr, SiteType t, int32_t x, int32_t y, Biome b);

  // ================================================================ L2 (chunkgen.cpp)
  void baseRect(int32_t x0, int32_t y0, int w, int h, gen::BaseRect& B);
  // (M2 fixer round 3) baseRect in pieces, for the web's phase-at-a-time town build: the per-tile terrain of rows
  // [yFrom, yTo) (yFrom 0 also sizes the rect), then the rivers, lakes and footbridges once every row is done
  void baseRectRows(int32_t x0, int32_t y0, int w, int h, gen::BaseRect& B, int yFrom, int yTo);
  void baseRectWater(gen::BaseRect& B);
  void chunk(int32_t cx, int32_t cy, ChunkData& out);
  std::shared_ptr<SettlementOut> town(const SitePlan& p, std::shared_ptr<const gen::RegionData> D);
  // web streaming (no threads): a settlement built a phase at a time across frames (EndlessSource::prepareChunk)
  struct TownJob;
  std::shared_ptr<TownJob> townJob;   // the one under way, if any
  std::shared_ptr<TownJob> townJobFor(const SitePlan& p, std::shared_ptr<const gen::RegionData> D);
  bool townJobStep(TownJob& J);
  void drainDeadEndRivers(const SitePlan& p, gen::BaseRect& B);   // a river ending in a town: its last reaches dry
  bool prepareChunk(int32_t cx, int32_t cy, double budgetMs);
  void stampSite(const SitePlan& p, gen::Stamp& S);
  void stampDen(const DenPlan& d, gen::Stamp& S);
  struct TownEntry { std::shared_ptr<SettlementOut> out; uint64_t used = 0; };
  std::unordered_map<Gid, TownEntry> towns;
  uint64_t townClock = 0;
};

namespace gen {
// polyline helpers (roads.cpp)
void chaikin(std::vector<GTile>& pts, int passes);
// walks a segment 4-connected (every step moves one tile in x or y), calling f(x, y) on each tile, both ends included
template <class F>
void walk4(int32_t x0, int32_t y0, int32_t x1, int32_t y1, F&& f) {
  int32_t dx = std::abs(x1 - x0), dy = std::abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
  int64_t err = 0;   // signed area: dy * (x - x0) * sx - dx * (y - y0) * sy, kept near 0
  int32_t x = x0, y = y0;
  f(x, y);
  while (x != x1 || y != y1) {
    // step in x if that keeps us closer to the line, else y
    int64_t ex = err + dy, ey = err - dx;
    if (x != x1 && (y == y1 || std::llabs(ex) <= std::llabs(ey))) { x += sx; err = ex; }
    else { y += sy; err = ey; }
    f(x, y);
  }
}
}  // namespace gen

}  // namespace ew
