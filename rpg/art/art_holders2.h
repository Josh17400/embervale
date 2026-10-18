// EMBERVALE art, P2a HOLDERS (VISION_PLAN 15.36, STYLE_P2 1 and 16): the rig-2 geometry of everything a body uses.
// At rig 1 nothing here is read (every holder paints today's pixels: H1). At rig 2 the holders' painters (art_props.cpp,
// art_building.cpp, art_home.cpp, art_sea.cpp, art_city.cpp, art_market.cpp, art_handcart.cpp, art_party.cpp,
// art_parts_gates.cpp, art_walls.cpp, art_culture_furniture.cpp) paint with these numbers and holderPx (art_rig.h)
// reports them, so the H2 ratios are measured from the very numbers the pixels are painted with.
//
// Conventions: heights are art px above the FLOOR ROW, the canvas's bottom row (a prop stands bottom-centre on its
// tile's bottom-centre, so the floor row is the tile's row 15). "Front edge" = the nearest (lowest on screen) row of a
// surface seen from the high 3/4 view: a seat's or a table top's lit lip.
//
// For POSES (seatFit2 / bedFit2 / sleeper2 / Ride): seatRow2 and berth2 say where a v2 body goes on these holders; the
// numbers are the painters' own (art_props.cpp "(P2a HOLDERS) rig 2") and never change without this header changing.
#pragma once
#include "rpg/art/art_props.h"
#include "rpg/art/art_life.h"

namespace art {
namespace holders {

constexpr int kFigure = 32;          // the v2 adult man (STYLE_P2 3)

// ---- seats and tops (STYLE_P2 1: seat 0.31-0.37, table / counter 0.44-0.50 of the figure)
constexpr int kSeat = 11;            // chair, stool, bench: the seat's front edge above the floor (0.34)
constexpr int kThroneSeat = 12;      // the throne's seat front edge above the floor (on its 2 px dais)
constexpr int kTable = 15;           // a table's top, front edge (0.47)
constexpr int kCounter = 15;         // a counter's / stall counter's top, front edge (0.47)
constexpr int kSeatDepth = 3;        // rows of seat surface behind the front edge (the sitter's hips sit on its middle)
// the seat surface's row inside its tile (0 top .. 15 bottom) where a chair-height sitter's seat line lands: the same
// convention as the v1 SeatFit::seatY (the middle of the seat's top surface, 2 rows above the front edge)
inline int seatRow2(Prop seat) {
  switch (seat) {
    case Prop::Chair: case Prop::Stool: case Prop::Bench: return 15 - kSeat - 2;          // 2
    case Prop::Throne: return 15 - kThroneSeat - 2;                                      // 1 (the throne overhangs north)
    case Prop::Cushion: return 15 - 5;                                                   // its plump top: 10
    default: return 12;                                                                  // floor seats: as v1
  }
}

// ---- berths (STYLE_P2 1: "footprint kept; the sprite may widen to 24 and rise for the headboard; a body lying
//      north-south is drawn at 0.8 of its length")
struct Berth2 {
  int w = 0, h = 0;          // the berth sprite's canvas at rig 2 (bottom-centre on the anchor tile, as v1)
  int headX = 0, headY = 0;  // the sleeper's face centre in that canvas (on the pillow)
  int quiltTop = 0;          // the first row of blanket over the body (the turned-down sheet's lower row + 1)
  int quiltEnd = 0;          // the last row of blanket over the body (the feet)
  bool across = false;       // lying west-east (head west)
};
Berth2 berth2(Berth b, int kit);   // art_props.cpp

// ---- the rest (px above the floor / ground row unless noted)
constexpr int kFence = 15;           // a fence's posts (0.47: it reaches the hip)
constexpr int kGardenWall = 16;      // a garden wall's coping (0.50)
constexpr int kHedge = 17;           // a clipped hedge (0.53)
constexpr int kCartWheel = 16;       // a market / farm cart's wheel diameter (0.50)
constexpr int kHandcartWheel = 15;   // the hand cart's wheel diameter (0.47)
constexpr int kLadderRung = 6;       // rungs every 6 px
constexpr int kGraveMarker = 15;     // the headstone's height above the ground: today's 13 x 1.2 at most

// ---- passages (STYLE_P2 1: clear height 1.25 of the figure or more; "the hero walks under, never through"). The clear
//      height is from the ground at the passage's mouth (its front ground row) to the lowest thing over the walk: the
//      underside of a lintel, an arch's soffit at its crown, a deck's beam, a cord's flags. Every form is painted with
//      at least these numbers over the middle of its passage (a 32 px walker in the middle 14 px clears it by 2 px or
//      more on the arched forms); holderPx reports the smallest of its group.
constexpr int kGateClear = 41;       // the timber gate's lintel, the earthwork bridge-gate's beam, the hedge gap's flags
constexpr int kArchClear = 42;       // the arched forms' crowns: iwan, gate pavilion, moon gate, elven and living arch,
                                     // a wall's span arch (art_walls.cpp)
constexpr int kCityGateClear = 41;   // the masonry gatehouses' arch (drum towers, square towers, pylons; the timber
                                     // gatehouse's lintel 41, the arches 42)
// the wall's span over an opening (art_walls.cpp, an east-west run): a raised arch block over the middle two tiles of
// the three-tile opening, its walk kSpanTop2 px over the ground (the wall itself stays WALL_H)
constexpr int kSpanTop2 = 48;

// ---- boats (STYLE_P2 1, the base recorded by HOLDERS for the planner: the gunwale and the rail are measured from the
//      floor the body is in, the rowboat's bottom boards and the deck a crew stands on, at amidships; the floor may lie
//      under the waterline, inside the hull, as in a real boat)
constexpr int kGunwale2 = 13;        // the rowboat's gunwale over its bottom boards (0.41)
constexpr int kDeckRail2 = 16;       // a sloop's (and a galleon's waist) rail over its deck (0.50)
constexpr int kThwart2 = 11;         // the rowboat's thwart over its bottom boards: a seated rower's hips (= kSeat, 0.34)

}  // namespace holders

// (P2a HOLDERS) the rowboat's thwart: where a seated rower's seat line goes, px over the waterline at the boat's anchor
// (negative: under it), the same frame as shipDeckY (art_sea.h; the floor a standing figure's soles rest on). Rig 1:
// the rowboat's own thwart over today's floor. For the view's seated rower (ANIM / LENS: a Sit posture aboard).
int shipThwartY(int hull);   // art_sea.cpp
}  // namespace art
