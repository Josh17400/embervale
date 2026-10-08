// EndlessSource: the endless world generator's public face (VISION_PLAN 2.5). WORLD lane.
// The work lives in EndlessSource::Impl (rpg/world/gen.h), split by layer:
//   macro.cpp     L0   continents, plates / ridges / passes, elevation, climate, biomes, rock, relief levels
//   hydro.cpp     L0.5 lakes, spring-traced rivers, the per-region river index
//   region.cpp    L1   kingdoms and capitals, the start plan, settlement lattices, POIs, dens, names, danger, region plans
//   roads.cpp     L1c  the Gabriel road graph, corridor A*, Chaikin smoothing
//   chunkgen.cpp  L2b  the chunk pipeline (base, rivers, lakes, settlements, stamps, roads, relief, vegetation)
// Order-independence contract (VISION_PLAN 2.3): every structural decision comes from integer maths on (seed,
// coordinates) only; caches hold pure values. One instance is not thread-safe; two instances never share state.
#include <algorithm>
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
  m.eco = d_->ecoFar(m.biome, c, gx, gy);
  if (m.eco == Eco::Blight) m.biome = Biome::Plains;
  m.kingdom = d_->kingdomAt(gx, gy);
  return m;
}
Eco EndlessSource::ecoAt(int32_t gx, int32_t gy) { d_->makeStart(); return d_->tile(gx, gy).eco; }
Eco EndlessSource::ecoFar(int32_t gx, int32_t gy) {
  d_->makeStart();
  const gen::Coarse c = d_->coarse(gx, gy);
  const bool water = c.e < ELEV_SEA;
  const Biome b = c.rock > gen::Q(0.5) && !water ? Biome::Mountain : d_->classify(c.e, c.t, c.m, gx, gy, water);
  return d_->ecoFar(b, c, gx, gy);
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
Gid EndlessSource::kingdomOfCell(int32_t kx, int32_t ky) {
  d_->makeStart();
  return d_->kcell(kx, ky).capital ? makeId(kx, ky, IdKind::Kingdom, 0) : 0;
}
Gid EndlessSource::kingdomAt(int32_t gx, int32_t gy) { d_->makeStart(); return d_->kingdomAt(gx, gy); }
std::vector<SettlementNode> EndlessSource::settlementsIn(int32_t x0, int32_t y0, int32_t x1, int32_t y1, bool withNames) {
  d_->makeStart();
  std::vector<gen::Node> nodes;
  d_->nodesIn(x0, y0, x1, y1, nodes);
  std::vector<SettlementNode> out;
  out.reserve(nodes.size());
  for (const gen::Node& n : nodes) {
    if (n.x < x0 || n.y < y0 || n.x >= x1 || n.y >= y1) continue;
    SettlementNode s;
    s.id = n.id; s.type = n.type; s.x = n.x; s.y = n.y; s.flags = n.flags; s.seed = n.seed;
    s.kingdom = d_->kingdomAt(n.x, n.y);
    if (withNames) s.name = d_->siteName(n);
    out.push_back(std::move(s));
  }
  std::sort(out.begin(), out.end(), [](const SettlementNode& a, const SettlementNode& b) { return a.id < b.id; });
  out.erase(std::unique(out.begin(), out.end(), [](const SettlementNode& a, const SettlementNode& b) { return a.id == b.id; }), out.end());
  return out;
}
const StartPlan& EndlessSource::start() { d_->makeStart(); return d_->sp; }
int EndlessSource::danger(int32_t gx, int32_t gy) { d_->makeStart(); return d_->danger(gx, gy); }
uint32_t EndlessSource::landmass(int32_t gx, int32_t gy) { d_->makeStart(); return d_->landmass(gx, gy); }
const EndlessSource::Stats& EndlessSource::stats() const { return d_->stats; }

}  // namespace ew
