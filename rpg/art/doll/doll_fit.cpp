// EMBERVALE art: where a doll body sits and sleeps (DOLL; P2b phase A, lead). seatFit2 and bedFit2 moved here from the
// procedural rig (rig_poses.cpp, rig_sleep.cpp) with their numbers unchanged (VISION_PLAN 15.36 "Adapted"): they read
// only HOLDERS' seat rows and berths (art_holders2.h), agreed with POSES in P2a.
#include <algorithm>
#include "rpg/art/art_holders2.h"
#include "rpg/art/doll/doll.h"

namespace art {

// HOLDERS' seatRow2 is the seat surface's row inside its tile where a sitter's seat line lands (the middle of the seat's
// top, 2 rows behind its lit front edge, kSeat = 11 px over the floor row 15). A chair-height sitter's hips are on that
// row in every facing; the posture holds them kChairHip rows over its pivot, so the pivot lands on the floor row (the
// feet on the floor). Floor seats (cushion, mat, bedroll, rug, the bare ground): cross-legged, the hips 2 px over the
// pivot (the floor, or the cushion's plump top).
SeatFit seatFit2(Prop seat, int kit, int facing) {
  (void)kit;
  SeatFit f;
  const int fc = facing == 3 ? 2 : facing;
  f.front = true;
  const int chairHip = -(holders::kSeat + 2), floorHip = -2;
  switch (seat) {
    case Prop::Chair:   // the back to the north: down or sideways, never into the back
      if (fc == 1) return f;
      [[fallthrough]];
    case Prop::Stool: case Prop::Bench: {
      f.ok = true;
      f.seatY = (int8_t)holders::seatRow2(seat);
      f.ay = (int8_t)(f.seatY - 16 - chairHip);
      break;
    }
    case Prop::Throne:
      if (fc != 0) return f;
      f.ok = true;
      f.seatY = (int8_t)holders::seatRow2(seat);
      f.ay = (int8_t)(f.seatY - 16 - chairHip);
      break;
    case Prop::Cushion:
      // facing up the sitter sits back on the cushion's south edge (as v1)
      f.ok = true; f.seatY = (int8_t)(holders::seatRow2(seat) + (fc == 1 ? 3 : 0)); f.sit = Posture::SitFloor;
      f.ay = (int8_t)(f.seatY - 16 - floorHip);
      break;
    case Prop::SleepingMat: case Prop::Bedroll: case Prop::Rug: case Prop::LowTable: case Prop::COUNT:
      f.ok = seat != Prop::LowTable; f.seatY = 12; f.sit = Posture::SitFloor;
      f.ay = (int8_t)(f.seatY - 16 - floorHip);
      break;
    default: break;
  }
  return f;
}

BedFit bedFit2(Berth b, int kit) {
  const holders::Berth2 g = holders::berth2(b, kit);
  BedFit f;
  f.w = (int8_t)g.w; f.h = (int8_t)g.h;
  f.headX = (int8_t)g.headX; f.headY = (int8_t)g.headY;
  f.across = g.across;
  f.len = (int8_t)std::max(0, g.quiltEnd - g.quiltTop);
  if (!g.across) {
    // the generic Sleep posture (the classic bed's sleeper, its pivot at its canvas's bottom-centre): the actor's p so
    // that its head lies on this berth's pillow
    const holders::Berth2 bed = holders::berth2(Berth::Bed, -1);
    const int relX = bed.headX - bed.w / 2, relY = bed.headY - (bed.h - 1);
    f.ax = (int8_t)(-g.w / 2 + g.headX - relX);
    f.ay = (int8_t)(-g.h + g.headY - relY);
  }
  return f;
}

}  // namespace art
