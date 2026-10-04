// M0b interiors (WORLDGEN_V7, VISION_PLAN 15.7): buildings laid out as rooms with a purpose on one or more floors.
// genInterior (rpg/sim/dungeon.cpp) calls genInteriorRooms for V7 buildings; rpg/sim/rooms.h has the geometry contract.
#pragma once
#include <cstdint>
#include "rpg/sim/world.h"

// Builds floor `floor` (0 .. b.floors()-1) of building b into m (m must be empty: genInterior resets it).
// Deterministic from (b, seed, floor). Every floor of a building has the same size, and the stairs line up: the
// StairsDown of floor f+1 stands on the tile of floor f's StairsUp.
void genInteriorRooms(Map& m, const Bldg& b, uint32_t seed, int floor);

// A structural signature of one floor (walls, doors, room kinds and stairs; not the furniture), for the variety audit.
uint64_t interiorLayoutSignature(const Map& m);
