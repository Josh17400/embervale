// M7c the EDGE lane's own state on the settlement generator (rpg/world/town_gen.h Gen::edge): what the edge rule
// plans for the outskirts, a city's faubourgs along its approach roads and the farmland beyond. EDGE lane (phase B):
// add whatever the edge planner needs here; only rpg/world/town_edge*.cpp, town_layout.cpp, town_homes.cpp,
// town_dress.cpp and town_plan.cpp include it.
//
// The EDGE lane's work is free functions over Gen& (it adds no member to Gen, whose header is the CITY lane's):
//   town_edge_wall.cpp   the city wall's outline (round, oval, polygon, squeezed along a river or coast, following the
//                        land): edgeWallShape (land(), phase 0) and the radial profile every wall test reads
//   town_edge.cpp        edgeBegin (phase 1: the belt, faubourgWant), outskirts (phase 9: the faubourgs along the
//                        approach roads, sporadic farmsteads), edgeFarmland (phase 12: the organic farmland)
//   town_edge_order.cpp  the order dial's low and mid end: a village's spine and its culture's flavour, a town's
//                        structured side streets (Rank::Street)
#pragma once
#include <cstdint>
#include <utility>
#include <vector>
#include "rpg/sim/common.h"
#include "rpg/world/settlement.h"
#include "rpg/world/town_plan.h"

namespace ew {

// (M7c) the faubourg belt round a city's walled core (tiles each side; 0 for towns and villages): settlementFootprint
// adds it to the core, the generator takes it off again (the radii rx / ry stay the walled core's). settlement.cpp.
int settlementBelt(SiteType type, uint32_t seed);

namespace town {

struct Gen;

struct EdgeData {
  int belt = 0;                              // the faubourg belt's depth outside the wall (tiles; 0: none)
  std::vector<uint8_t> tile;                 // per buffer tile, the edge planner's own marks (EDGE_* bits)
  std::vector<IRect> plots;                  // faubourg plots along the approach roads
  // the wall's outline (town_edge_wall.cpp): its shape and its normalised radius per normalised bearing (EDGE_RAYS
  // samples from -pi; empty: the classic blob)
  WallShape shape = WallShape::None;
  std::vector<float> rho;
  std::vector<std::pair<float, float>> corners;   // Polygon: its corners (normalised bearing, normalised radius)
  int faubourgs = 0, faubourgBldgs = 0, farms = 0;
  int topUp = 0;                             // > 0: outskirts() again for the homes a city is still short (homesFinish)
};
constexpr int EDGE_RAYS = 128;
constexpr uint8_t EDGE_FAUBOURG = 1, EDGE_FARM = 2, EDGE_ROAD = 4;

// the edge state, made on first use (land() asks for it before edgeBegin runs)
EdgeData& edgeOf(Gen& g);
// town_edge_wall.cpp
void edgeWallShape(Gen& g);                                  // land(): the outline, before `inside` is set
float edgeWallRho(const Gen& g, float a);                    // the wall's normalised radius at normalised bearing a
bool edgeInWall(const Gen& g, int x, int y, float inset);    // (x, y) lies inside the outline less inset (normalised)
// town_edge.cpp
void edgeFarmland(Gen& g);                                   // fields() of a city: the farmland of the belt
// town_edge_order.cpp
void villageSpine(Gen& g);                                   // ringRoads() of a village of a planned people
void villageLoops(Gen& g);                                   // step() phase 5: a village's loops of lanes past two opened
void townCore(Gen& g);                                       // fabric() of an organic town: its structured core
void townStreets(Gen& g);                                    // fabric() of a town: its structured side streets

}  // namespace town
}  // namespace ew
