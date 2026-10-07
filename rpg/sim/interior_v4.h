// M0b interiors (WORLDGEN_V7, VISION_PLAN 15.7): buildings laid out as rooms with a purpose on one or more floors.
// genInterior (rpg/sim/dungeon.cpp) calls genInteriorRooms for V7 buildings; rpg/sim/rooms.h has the geometry contract.
// M3b (VISION_PLAN 15.14): the floor plan derives from the builder's blueprint (bldgBlueprint: its InteriorShape and
// body volume): a round body has a round floor, a courtyard plan an open court, an L its wing as a room, a long hall
// its long hearth, a cross its nave and transepts; inns are planned in their culture's own way; seats of power by the
// society's seat (cult::Seat: the khan's tent court, the jarl's great hall, the high priest's sanctum...).
#pragma once
#include <cstdint>
#include "rpg/sim/world.h"

// Builds floor `floor` (0 .. b.floors()-1) of building b into m (m must be empty: genInterior resets it).
// Deterministic from (b, seed, floor). Every floor of a building has the same size, and the stairs line up: the
// StairsDown of floor f+1 stands on the tile of floor f's StairsUp.
void genInteriorRooms(Map& m, const Bldg& b, uint32_t seed, int floor);
// the same from a given blueprint (genInteriorRooms passes bldgBlueprint(b)); tests and galleries use it to try every
// floor plan on every purpose
void genInteriorRooms(Map& m, const Bldg& b, const bld::Blueprint& bp, uint32_t seed, int floor);

// A structural signature of one floor (walls, doors, room kinds and stairs; not the furniture), for the variety audit.
uint64_t interiorLayoutSignature(const Map& m);
// the name of the plan template the generator used for this building ("inn: caravanserai", "round home", "seat: tent
// court", "plain"...), for the variety audits (rpg_test, rooms_gallery)
const char* interiorTemplate(const Bldg& b, const bld::Blueprint& bp, uint32_t seed);
// debugging aid: how often each source line of the plan templates turned a draw down since the last reset
void interiorWhy(int* lines, int n, bool reset);
