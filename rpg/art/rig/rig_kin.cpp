// EMBERVALE art, v2 rig (POSES, P2a): what kids do with their bodies in the postures. RIG-BODY's bodyOf gives youth
// (age 2: 26 px, torso 6, legs 7) and child (age 1: 21 px, an 11 px head, torso 5, legs 5) proportions (STYLE_P2 3,
// 14), so every posture of rig_poses.cpp paints them natively from the same keys; these are the few places a kid's
// body does something an adult's does not.
#include <algorithm>
#include "rpg/art/rig/rig_internal.h"

namespace art {
namespace rig2 {

// a seated kid's shins: a child's and a youth's short legs dangle from a chair-height seat (the knee at the seat's
// front edge, the shin hanging its own length); an adult's reach the floor
int kinSeatLegs(const Body& B, int seatH) {
  (void)seatH;
  if (!B.age) return 99;
  return std::max(2, B.legs - 3);
}

// tag (Posture::Play): children hop on every other frame (2 px; a youth 1, an adult 1)
int kinHop(const Body& B, int frame) {
  if (frame & 1) return 0;
  return B.age == 1 ? 2 : 1;
}

}  // namespace rig2
}  // namespace art
