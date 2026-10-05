// M0b rooms (rooms.h): names for the HUD and the tests.
#include "rpg/sim/rooms.h"

const char* roomKindName(RoomKind k) {
  static const char* n[] = {"HALL",    "COTTAGE",   "KITCHEN", "BEDROOM", "GUEST ROOM", "PRIVATE QUARTERS", "CORRIDOR",
                            "COMMON ROOM", "SHOP", "STOCKROOM", "FORGE", "WORKSHOP", "NAVE",      "VESTRY",
                            "THRONE HALL", "BARRACKS", "STUDY", "STOREROOM", "BARN", "COUNCIL CHAMBER"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)RoomKind::COUNT, "roomKindName: one name per RoomKind");
  return (int)k < (int)RoomKind::COUNT ? n[(int)k] : "ROOM";
}
