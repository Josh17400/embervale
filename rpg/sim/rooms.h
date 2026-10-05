// M0b: rooms and storeys inside buildings (VISION_PLAN 15.7, binding). genInterior (WORLDGEN_V7+) lays a building's
// floors out as rooms with a purpose, partitioned by interior walls with doors, and records them on the Map so the
// game (renting "your" room), the renderer and the tests (no bed in a common room, every room reachable, stairs iff
// 2+ storeys) can all read the same facts. Included by world.h; nothing here is saved (maps regenerate from seeds).
//
// GEOMETRY CONTRACT (the generator builds it, the interior art draws it, the tests check it):
//  - Every wall is Ground::InteriorWall. The outer shell is as before: rows 0..1 are the back wall (row 1 is its face
//    row), column 0 / W-1 the side walls, row H-1 the front wall with the entrance at (exitX, exitY) on floor 0.
//  - An E-W (horizontal) partition is exactly TWO tile rows of wall: a cap row (the wall's top seen from above) over a
//    face row (the wall's 16 px face, seen from the room below). It runs wall to wall or ends in a T against another
//    partition. Nothing stands on either row except a door.
//  - An N-S (vertical) partition is ONE tile column of wall, from a partition or the back wall down to a partition or
//    the front wall.
//  - A doorway is a single floor tile cut through a partition: for an E-W partition both its rows are floor at the
//    door column (the door prop, Prop::DoorH, stands on the face-row tile); for an N-S partition one tile (Prop::DoorV
//    on it). A doorway has wall on both sides along the partition, never sits at a T or corner, and is never next to
//    another doorway. Doors are walk-through (propSolid false).
//  - Stairs: Prop::StairsUp stands on a floor tile directly below a wall FACE row (the back wall's row 1 or an E-W
//    partition's face row): the flight climbs north into that wall. Prop::StairsDown is a railed stairwell opening on
//    a floor tile of the upper floor. Both are walk-through; walking onto the tile (with movement) changes floor.
//    Map::up / Map::down record them with the tile you stand on when you arrive on this floor by them.
//    (M0b fix round 1) A flight is two tiles wide where the plan fits it: the tile beside the recorded one carries the
//    same prop and takes you too (the plain fallback plan may still lay a one-tile flight).
//  - Beds (M0b fix round 1) are two tiles long where they fit: Prop::Filler on the head tile against the wall, the
//    Prop::Bed on the tile in front of it (RoomInfo::bedX/bedY); a one-tile bed stands right under the wall.
//  - Spawn slots are unique per building across its floors: floor f uses slots 16*f .. 16*f+15 (quest givers and
//    killed spawns are keyed by building + slot, and by Game::mapKey, which differs per floor).
#pragma once
#include <cstdint>
#include "rpg/sim/common.h"

// A staircase on this floor. (x, y) is the trigger tile; (ax, ay) is where the player stands after arriving on this
// floor through it (a free floor tile next to it, never the trigger itself). x < 0: none.
struct Stairs {
  int x = -1, y = -1;
  int ax = -1, ay = -1;
  bool valid() const { return x >= 0; }
};

// What a room is for. The layout must explain it (VISION_PLAN 15.7): kitchens by a hearth, bedrooms private behind a
// door, the shop counter between customer and stock, the forge vented at a wall, temples oriented to the altar.
enum class RoomKind : uint8_t {
  Hall,           // a home's main room: table, hearth or stove, storage (no bed unless the home is a Cottage)
  Cottage,        // a one-room home (huts, the smallest houses): hearth, bed, table share one room
  Kitchen,        // hearth or oven, prep table, pantry shelves, barrels
  Bedroom,        // a household's private bedroom
  GuestRoom,      // an inn's rented room: bed, chest, small table or nightstand with a candle (RoomInfo::guest)
  OwnerRoom,      // the innkeeper's / shopkeeper's / lord's private quarters
  Corridor,       // a passage or landing that only connects rooms (stairs may stand in it)
  Common,         // an inn's public room: bar, tables and benches, patrons
  Shopfloor,      // the customers' side of a shop counter
  Stockroom,      // behind the counter / the store room: shelves, crates, barrels
  Forge,          // a smithy's work hall: forge vented at an outer wall, anvil, quench tub
  Workshop,       // crafts and study at a bench (a farmhouse's work room, a mage's laboratory)
  Nave,           // a temple's hall, pews facing the altar
  Vestry,         // the priest's room behind or beside the nave
  ThroneHall,     // a keep's great hall: the throne at the far end, banners, a long table
  Barracks,       // guards' bunks and weapon racks
  Study,          // books, desk, lectern (towers, keeps, wealthy homes)
  Storeroom,      // sacks, crates, barrels, firewood
  Barn,           // a farmhouse's animal / hay end
  Council,        // M1: a palace's council chamber: the long table with the king's chair at its head, maps, banners
  COUNT
};
const char* roomKindName(RoomKind k);   // "COMMON ROOM", "GUEST ROOM"... (rpg/sim/rooms.cpp)
// beds may stand only in these rooms (the automated check: no bed in a common room, kitchen, shop or hall)
inline bool roomAllowsBed(RoomKind k) {
  return k == RoomKind::Cottage || k == RoomKind::Bedroom || k == RoomKind::GuestRoom || k == RoomKind::OwnerRoom ||
         k == RoomKind::Barracks;
}
// rooms a visitor should not wander into uninvited (the view may dim their door; NPC schedules later)
inline bool roomPrivate(RoomKind k) {
  return k == RoomKind::Bedroom || k == RoomKind::GuestRoom || k == RoomKind::OwnerRoom || k == RoomKind::Vestry ||
         k == RoomKind::Stockroom || k == RoomKind::Barracks;
}

// One room of one floor. Map::roomAt holds, per tile, the index of the room the tile belongs to (-1 for walls).
struct RoomInfo {
  RoomKind kind = RoomKind::Hall;
  IRect r;                   // bounding box in tiles (floor tiles only)
  int doorX = -1, doorY = -1;   // the doorway tile into it (-1: open to the floor's entrance room, or the entrance room itself)
  int bedX = -1, bedY = -1;     // its (first) bed, -1 none
  int8_t guest = -1;         // GuestRoom: 0, 1, 2... the number the innkeeper hands out; -1 otherwise
};
