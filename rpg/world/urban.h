// M7b "Cities & Voice" (VISION_PLAN 15.23, 15.24): the urban fabric of a settlement, one byte per tile (Map::urb,
// ChunkData::urb, SettlementOut::buf.urb). The CITY lane's generator (rpg/world/town_*.cpp) writes it; the CITYART
// lane's view (rpg/view/terrain.cpp, render.cpp) draws from it: avenues and lanes in their paving, raised kerbed
// sidewalks along house fronts with the kerb's face where the walk meets the carriageway, gutters and central drains,
// corner stones at junctions, squares, closes, park lawns and paths, garden ground, courts. 0 everywhere outside a
// city's plan, and ALWAYS 0 in a village (owner, 2026-10-10: "villages look great"; rpg_test --villages).
//
// Layout of the byte (frozen; append Urb values before COUNT, never reorder):
//   bits 0-3  Urb, the street class (what the tile is in the plan)
//   bits 4-5  upkeep 0..3 (0 a poor quarter's patched dirt and broken flags .. 3 a rich quarter's swept, even paving)
//   bits 6-7  Quarter (the district the tile belongs to)
// Regenerated with the chunks, never saved. Purely visual: walkability is the ground's and the props' (a sidewalk, a
// gutter and a drain are walked on; a kerb is a 3/4-view step the view draws, not a wall).
#pragma once
#include <cstdint>

namespace ew {

enum class Urb : uint8_t {
  None = 0,     // not part of the plan (yards, gardens behind houses, fields, open ground)
  Avenue,       // a main avenue (gate -> market -> seat of power): wide, straight, the best paving, kerbed both sides
  Street,       // a cross street off the avenues (paved, kerbed in the better quarters); a town's tidied main street
  Lane,         // a lane between blocks (narrow, paved or cobbled, no kerb)
  Alley,        // an alley or a poor quarter's dirt lane
  Sidewalk,     // a raised kerbed walkway or flagged edge along house fronts (the kerb shows where it meets a carriageway)
  Gutter,       // a gutter or central drain channel down a street (walked over; a grate where it meets a square)
  Square,       // a square's paving (market square, the square before the seat, a well-square at a lane junction)
  Close,        // a temple or cathedral close (paving round lawns)
  Park,         // a park or green's lawn
  ParkPath,     // a park's gravel path
  Garden,       // a walled garden's or kitchen garden's ground, an orchard's grass
  Court,        // a courtyard inside a block or compound (flagged, a tree or a well in it)
  Quay,         // a harbour or river quay's edge (fjord harbour city, river towns)
  COUNT         // <= 16
};

enum class Quarter : uint8_t { Centre = 0, Rich = 1, Craft = 2, Poor = 3 };   // the market / civic heart, the rich, the crafts, the poor

constexpr uint8_t URB_CLASS_MASK = 0x0F;
inline uint8_t urbPack(Urb u, int upkeep, Quarter q) {
  const int k = upkeep < 0 ? 0 : (upkeep > 3 ? 3 : upkeep);
  return (uint8_t)(((uint8_t)u & URB_CLASS_MASK) | (k << 4) | ((uint8_t)q << 6));
}
inline Urb urbClass(uint8_t b) { const uint8_t c = b & URB_CLASS_MASK; return c < (uint8_t)Urb::COUNT ? (Urb)c : Urb::None; }
inline int urbUpkeep(uint8_t b) { return (b >> 4) & 3; }
inline Quarter urbQuarter(uint8_t b) { return (Quarter)((b >> 6) & 3); }
// a carriageway (the kerb's lower side): avenues, streets, lanes, alleys
inline bool urbCarriage(Urb u) { return u == Urb::Avenue || u == Urb::Street || u == Urb::Lane || u == Urb::Alley; }
// paved open space (squares, closes, courts)
inline bool urbOpen(Urb u) { return u == Urb::Square || u == Urb::Close || u == Urb::Court; }
// green ground (lawns, paths, gardens)
inline bool urbGreen(Urb u) { return u == Urb::Park || u == Urb::ParkPath || u == Urb::Garden; }
const char* urbName(Urb u);   // rpg/world/town_urban.cpp (tests, dumps)

}  // namespace ew
