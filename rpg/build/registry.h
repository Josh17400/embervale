// The builder's registry (VISION_PLAN 15.14 acceptance: "no building kind may bypass the builder; a test enumerates
// every building / prop type"). Every art::Building is designed by bld::design (rpg/build/blueprint.h); every
// art::Prop is classified here: either it is a BUILT thing (a structure a culture makes: wells, fences, lamps,
// statues, standing stones, shrines, market stalls, gates...) and must be drawn from the builder's culture parts
// (rpg/build/parts.h), or it is not built (nature, loose goods, monsters' things) or it is interior furniture (drawn
// from the culture's furniture kit, art_culture_furniture.cpp). rpg_test --builder fails on any prop without a class
// and on any built prop whose painter ignores the culture (arch_gallery / props_gallery --check measure the pixels).
//
// Ownership (M3b): written by the lead in phase A, FROZEN. A new art::Prop must be classified here (the lane that adds
// it asks the lead / main session); the classification function is in rpg/build/registry.cpp.
#pragma once
#include <cstdint>
#include "rpg/art/art_building.h"
#include "rpg/art/art_props.h"

namespace bld {

// what the builder makes: one entry per kind of construction
enum class Kind : uint8_t {
  None,          // not built: nature, terrain features, loose goods, creatures, dungeon dressing
  Furniture,     // interior furniture and wall decor (the culture's furniture kit)
  Building,      // a building (bld::design)
  CityWall, Gatehouse, WallTower, Palisade, Bridge, Road, Paving,   // fortifications and ground (FortParts, RoadParts)
  Fence, Monument, Well, Shrine, Lamp, Bench, MarketStall, Sign, Banner, Tent, WorkYard,   // street furniture (parts)
  COUNT
};
const char* kindName(Kind k);
// the builder kind that makes this prop (Kind::None / Kind::Furniture for the unbuilt and the indoor ones)
Kind kindOfProp(art::Prop p);
// every art::Building is Kind::Building
inline Kind kindOfBuilding(art::Building) { return Kind::Building; }

}  // namespace bld
