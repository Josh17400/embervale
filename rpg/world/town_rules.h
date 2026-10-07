// Settlement rules shared by the TOWNS lane's generator (rpg/world/town_*.cpp), its tests (tools/tests/test_towns.cpp)
// and its galleries (tools/preview/town_gallery.cpp). Header-only: rpg_sim, rpg_art tools and rpg_test all read it.
#pragma once
#include <algorithm>
#include "rpg/culture/culture.h"
#include "rpg/sim/world.h"
#include "rpg/world/coords.h"

namespace ew {

// How many tiles a building's sprite may rise above its footprint's top row (roof, steeple, cone, palace towers), as the
// settlement generator budgets it when it keeps every sprite clear of its neighbours' fronts and doorsteps. The building
// painter (rpg/art/art_building.cpp riseBudgetPx) mirrors this table; it agrees with world.h bldgRiseTiles for every
// classic type and adds the M1 types (the palace rises seven tiles, the barracks four).
inline int townRiseTiles(art::Building t, int storeys) {
  switch (t) {
    case art::Building::Palace: return 7;
    case art::Building::Barracks: return 4;
    // M3b: the society's halls (their massing is the builder's: a two-storey guildhall, a domed bathhouse, a mead hall's
    // steep roof, a council spire)
    case art::Building::Guildhall: case art::Building::Exchange: case art::Building::Bathhouse: case art::Building::MeadHall:
    case art::Building::TeaHouse: case art::Building::Lodge: return 4;
    case art::Building::CouncilHall: return 6;
    default: return bldgRiseTiles(t, storeys);
  }
}

// A home for the census and the scale targets (VISION_PLAN 15.8): houses, stone houses, huts and farmhouses
inline bool townIsHome(art::Building t) {
  return t == art::Building::House || t == art::Building::StoneHouse || t == art::Building::Hut || t == art::Building::Farmhouse;
}

// The scale targets (VISION_PLAN 15.8, 15.10): homes per settlement type, and the cap on all buildings
struct TownScale { int homesMin, homesMax, bldgMax; };
inline TownScale townScale(SiteType t) {
  switch (t) {
    case SiteType::City: return {160, 220, 220};
    case SiteType::Town: return {40, 60, 80};
    default: return {10, 15, 24};
  }
}

// Kingdom banners: how many emblems art::kingdomBanner paints (emblem % count)
constexpr int KINGDOM_EMBLEMS = 8;

// M3 (VISION_PLAN 5.6, 5.7, the repetition audit): a settlement's colour on a lattice of region-sized cells (256 tiles,
// about the spacing of settlements), turned by its culture's seed. The colour picks the settlement's layout (the
// culture's alternate in 1 of 4), its wealth and, for a village, its street form (a crossroads hamlet, a road village,
// a village green), so that a people's settlements next to each other come out different (no more than 2 alike within
// 1500 tiles: rpg_test --town-audit), where independent draws per seed let three or four of a kingdom's farming
// villages match. Villages and towns: (i + 5j) mod 24, whose nearest cells of one colour lie five cells (1300 tiles)
// apart, every colour its own (layout, form, wealth) but for six pairs twelve colours apart (a village's form is its
// street form, a town's whether it has a second square); cities (few, far apart): (i + 5j) mod 8, three cells apart.
//
// M3b (owner, town audit seed 22): the lattice alone let three alike stand within 1500 tiles where two colours twelve
// apart give one look (two cells apart on the lattice). The region planner now chooses neighbouring settlements' looks
// together (region.cpp townLookFor): it keeps the lattice colour where no nearer settlement of the same culture, type
// and land already has that look, else picks a free look, and hands the colour to the generator in SitePlan::kind
// (settlements only: 1 + colour; 0 = the lattice's). `planned` is that SitePlan::kind.
constexpr int TOWN_COLOURS = 24, CITY_COLOURS = 8;
inline int townColour(const cult::Culture* c, SiteType t, int32_t gx, int32_t gy, int planned = 0) {
  if (planned > 0) return (planned - 1) % (t != SiteType::City ? TOWN_COLOURS : CITY_COLOURS);
  const int32_t i = floorDiv(gx, 256), j = floorDiv(gy, 256);
  const uint32_t r = c ? hash32(c->seed ^ 0x7A11u) : 0u;
  return t != SiteType::City ? (int)(((uint32_t)(i + 5 * j) + r) % 24u) : (int)(((uint32_t)(i + 5 * j) + r) & 7u);
}
inline bool townColourAlt(SiteType t, int col) {
  if (t != SiteType::City) return col < 12 && (col & 1) == 0;   // 6 of 24
  return col == 0 || col == 5;                                      // 2 of 8
}
inline int townColourWealth(SiteType t, int col) {
  if (t != SiteType::City) return col & 3;
  static const int w[8] = {1, 0, 1, 2, 0, 3, 2, 3};
  return w[col & 7];
}
// a village's street form (0 a crossroads hamlet, 1 a road village, 2 a village green); a town's second square (0 none,
// 1 a small one, 2 a larger one)
inline int townForm(int col) { return (col / 4) % 3; }

// M3 (VISION_PLAN 5.6, 5.7): the layout a settlement of this culture is built in: its TownStyle::layout, or its
// altLayout in 1 of every 4 (townColour, spread over its land); a village of a grid-planning people strings along its
// road instead (a dozen houses make no grid). Organic without a culture. The generator and the repetition audit both
// ask here. (gx, gy): the settlement's heart (SitePlan::ex, ey).
inline cult::Layout townLayoutFor(const cult::Culture* c, SiteType t, uint32_t seed, int32_t gx, int32_t gy, int planned = 0) {
  (void)seed;
  if (!c) return cult::Layout::Organic;
  cult::Layout l = townColourAlt(t, townColour(c, t, gx, gy, planned)) ? c->town.altLayout : c->town.layout;
  if ((int)l >= (int)cult::Layout::COUNT) l = cult::Layout::Organic;
  if (t == SiteType::Village && l == cult::Layout::Grid) l = cult::Layout::Linear;
  return l;
}

// M3 (VISION_PLAN 5.7): a settlement's wealth, 0 poor .. 3 rich: a village or a town anywhere from a poor hamlet to a
// prosperous one, a city at least comfortable, a royal seat rich (townColour: its neighbours differ). It skews every
// building's wealth (cult::buildingArch) a step either way.
inline int townWealthFor(const cult::Culture* c, SiteType t, bool capital, uint32_t seed, int32_t gx, int32_t gy, int planned = 0) {
  (void)seed;
  if (capital) return 3;
  const int w = townColourWealth(t, townColour(c, t, gx, gy, planned));
  return t == SiteType::City ? std::max(1, w) : w;
}

// M3b: what the eye (and rpg_test --town-audit) tells apart between two settlements of one culture, type and land:
// the layout (with a village's street form, a town's squares) and the wealth
inline int townLookSig(const cult::Culture* c, SiteType t, bool capital, int32_t gx, int32_t gy, int planned) {
  const int layout = (int)townLayoutFor(c, t, 0, gx, gy, planned) * 3 + (t != SiteType::City ? townForm(townColour(c, t, gx, gy, planned)) : 0);
  return layout * 4 + townWealthFor(c, t, capital, 0, gx, gy, planned);
}

}  // namespace ew
