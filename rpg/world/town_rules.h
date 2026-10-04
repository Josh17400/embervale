// Settlement rules shared by the TOWNS lane's generator (rpg/world/town_*.cpp), its tests (tools/tests/test_towns.cpp)
// and its galleries (tools/preview/town_gallery.cpp). Header-only: rpg_sim, rpg_art tools and rpg_test all read it.
#pragma once
#include <algorithm>
#include "rpg/sim/world.h"

namespace ew {

// How many tiles a building's sprite may rise above its footprint's top row (roof, steeple, cone, palace towers), as the
// settlement generator budgets it when it keeps every sprite clear of its neighbours' fronts and doorsteps. The building
// painter (rpg/art/art_building.cpp riseBudgetPx) mirrors this table; it agrees with world.h bldgRiseTiles for every
// classic type and adds the M1 types (the palace rises seven tiles, the barracks four).
inline int townRiseTiles(art::Building t, int storeys) {
  switch (t) {
    case art::Building::Palace: return 7;
    case art::Building::Barracks: return 4;
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

}  // namespace ew
