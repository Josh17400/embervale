// One settlement, built on its own local buffer (VISION_PLAN 2.5 L2a, 15.8). Contract between the WORLD lane (which
// plans the site, knows the land and the roads, and calls this once per settlement, caching the result) and the
// TOWNS lane (which owns rpg/world/settlement.cpp: streets, squares, walls, gates, buildings, palace compounds,
// banners, yards, fields, the census).
//
// Rules: deterministic from the inputs only (ctx.plan->seed and the plan's fields; never a shared stream); integer or
// IEEE-exact float maths (+ - * / sqrt; ew::dsin/dcos/datan2 instead of libm), so iPhone, web and Windows build the same
// town. The organic style is the owner's standing direction: winding streets that follow the approach roads, squares
// and greens, uneven setbacks, gardens, fields; cities get curved walls with real gatehouses where roads arrive.
#pragma once
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>
#include "rpg/world/source.h"

namespace ew {

struct SettlementCtx {
  const SitePlan* plan = nullptr;            // type, footprint (gx, gy, w, h), seed, name, flags (SPF_CAPITAL...),
                                             // archetype, bldgBase / bldgCap (building ids)
  const KingdomPlan* kingdom = nullptr;      // banner colours; nullptr in the wildlands
  int32_t rx = 0, ry = 0;                    // the plan's region (building ids: makeId(rx, ry, IdKind::Bldg, ...))
  // the land before the town is built, at a global tile: ground, biome, relief height (L0 + rivers + lakes). Water
  // that comes back here is a river or lake the town must respect (bridges, culverts, quays).
  std::function<void(int32_t gx, int32_t gy, Ground& g, Biome& b, uint8_t& height)> base;
  // where the roads to the neighbours leave, as bearings in radians (0 = east, +pi/2 = south, y down), strongest
  // road first. The main streets aim at these and city gates open toward them. May be empty (no roads yet).
  std::vector<float> roadBearings;
};

struct SettlementOut {
  int32_t gx = 0, gy = 0;                    // global tile of buf's (0, 0) (the footprint plus a margin)
  Map buf;                                   // LOCAL tiles: ground, prop, wall, biome, bldgAt, bldgs (Bldg::r local,
                                             // Bldg::id set, Bldg::site = -1), spawns (Spawn::site = -1, slot unique
                                             // within the settlement)
  std::vector<std::pair<int, int>> gates;    // gatehouses (local: left tile of the 3-wide opening)
  std::vector<IRect> wallGaps;               // wall openings (local)
  std::vector<uint8_t> used;                 // per buf tile: 1 = part of the town (vegetation and roads keep off)
  int32_t ex = 0, ey = 0;                    // the heart (global): square or green
  // counts for rpg_test's scale checks (VISION_PLAN 15.8: villages 10-15 houses, towns 40-60, cities 160-220)
  int homes = 0;
  Specialty special = Specialty::None;       // (M1 economy) what it lives from (the plan's, or derived when it had none)
};

// Build one settlement. Called by the chunk pipeline (rpg/world/*.cpp) on a cache miss.
void buildSettlement(const SettlementCtx& ctx, SettlementOut& out);

// The footprint a settlement of this type asks for (the area its streets, buildings, walls and fields may use; the
// buffer adds a margin). The TOWNS lane sizes it for VISION_PLAN 15.8; the WORLD lane's region plans use it for
// SitePlan::w / h and keep the lattice spacing well above it. Pure and cheap.
void settlementFootprint(SiteType type, Archetype arch, uint32_t seed, int& w, int& h);

}  // namespace ew
