// EndlessSource: the endless world generator's public face (VISION_PLAN 2.5). WORLD lane.
// The work lives in EndlessSource::Impl (rpg/world/gen.h), split by layer:
//   macro.cpp     L0   continents, plates / ridges / passes, elevation, climate, biomes, rock, relief levels
//   hydro.cpp     L0.5 lakes, spring-traced rivers, the per-region river index
//   region.cpp    L1   kingdoms and capitals, the start plan, settlement lattices, POIs, dens, names, danger, region plans
//   roads.cpp     L1c  the Gabriel road graph, corridor A*, Chaikin smoothing
//   chunkgen.cpp  L2b  the chunk pipeline (base, rivers, lakes, settlements, stamps, roads, relief, vegetation)
// Order-independence contract (VISION_PLAN 2.3): every structural decision comes from integer maths on (seed,
// coordinates) only; caches hold pure values. One instance is not thread-safe; two instances never share state.
#include "rpg/world/gen.h"

namespace ew {

EndlessSource::EndlessSource(uint64_t seed) : d_(std::make_unique<Impl>(seed)) {}
EndlessSource::~EndlessSource() = default;
uint64_t EndlessSource::seed() const { return d_->seed; }
MacroSample EndlessSource::macro(int32_t gx, int32_t gy) { d_->makeStart(); return d_->macro(gx, gy); }
const RegionPlan& EndlessSource::region(int32_t rx, int32_t ry) {
  d_->makeStart();
  d_->lastRegion = d_->regionData(rx, ry);
  return d_->lastRegion->plan;
}
MacroSample EndlessSource::macroFar(int32_t gx, int32_t gy) {
  d_->makeStart();
  gen::Coarse c = d_->coarse(gx, gy);
  MacroSample m;
  m.elev = c.e; m.temp = c.t; m.moist = c.m;
  m.water = c.e < ELEV_SEA;
  m.biome = c.rock > gen::Q(0.5) && !m.water ? Biome::Mountain : d_->classify(c.e, c.t, c.m, gx, gy, m.water);
  m.height = m.water ? 0 : (uint8_t)gen::levelOf(c.e);
  m.kingdom = d_->kingdomAt(gx, gy);
  return m;
}
std::vector<float> EndlessSource::roadBearings(Gid site) {
  d_->makeStart();
  std::shared_ptr<const gen::RegionData> D = d_->regionData(idRx(site), idRy(site));
  auto it = D->bearings.find(site);
  return it == D->bearings.end() ? std::vector<float>() : it->second;
}
void EndlessSource::chunk(int32_t cx, int32_t cy, ChunkData& out) { d_->chunk(cx, cy, out); }
bool EndlessSource::prepareChunk(int32_t cx, int32_t cy, double budgetMs) { return d_->prepareChunk(cx, cy, budgetMs); }
const KingdomPlan* EndlessSource::kingdom(Gid id) { d_->makeStart(); return d_->kingdom(id); }
const StartPlan& EndlessSource::start() { d_->makeStart(); return d_->sp; }
int EndlessSource::danger(int32_t gx, int32_t gy) { d_->makeStart(); return d_->danger(gx, gy); }
const EndlessSource::Stats& EndlessSource::stats() const { return d_->stats; }

}  // namespace ew
