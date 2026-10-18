// EMBERVALE art, v2 rig (RIG-BODY, P2a): the death atlas: Fall 3 (recoil, buckle, topple) and Lying 2 (head west,
// head east), every facing, 16 px tall or less lying (STYLE_P2 13).
//   Fall    per facing and per side (variant 0 west, 1 east: the Lying variant it ends in, so the head lands on the
//           side the body tipped to): recoil (head snapped back, arms flung, the weapon loose) in the actor's facing;
//           buckle (both knees bent, the head down) leaning toward the side; topple: the side-view figure tipped toward
//           the side (painted, never rotated by 45 degrees): backward for a body facing down, up or away from the
//           fall, forward over its feet for a side-on body falling the way it faces.
//   Lying   the left-facing side figure laid on its back, the face up, eyes closed, arms loose, the weapon dropped
//           beside the hand (on the near side of the body, so it shows): head west is its exact transpose (the top-left
//           light maps onto itself); head east is a quarter turn clockwise (the top stays lit; the ink outline is drawn
//           after the turn). Pivot: the middle of the body on its lowest (back) row.
#include <algorithm>
#include "rpg/art/rig/rig_internal.h"

namespace art {
namespace rig2 {

namespace {

// every weapon rig_core paints (the sword, and RIG-ARMS's drawn arms) falls with the body and lies beside its hand
bool armedLook(const HumanLook& L) { return L.weapon == 1 || armsDrawn(L.weapon); }
// the long arms tilt toward the viewer as they fall (foreshortened past the sword's length, so a falling spearman's
// frames stay within the DEATH atlas's 48 KB, R6): px of foreshortening added to every fall and lying pose
int longShort(int weapon) {
  switch (weapon) {
    case 8: return 7;              // spear (34 px)
    case 4: return 7;              // staff (30)
    case 7: case 9: return 5;      // greatsword, walking stick (24)
    case 2: case 6: return 4;      // axe, mace (their heavy heads)
    default: return 0;
  }
}
Pose withWeapon(Pose p, int weapon) { if (p.wDir >= 0) p.wShort = (int8_t)std::min(9, p.wShort + longShort(weapon)); return p; }

Pose recoil(const Body& B, int fc, bool sword) {
  Pose p = restPose(B, fc, false);
  p.expr = ExHurt;
  p.chest = -1;
  const int sh = B.shoulder;
  switch (fc) {
    case FDown:
      p.headY = -1;
      p.hRx = (int8_t)(-B.shoulderHalf - 3); p.hRy = (int8_t)(sh + 2);
      p.hLx = (int8_t)(B.shoulderHalf + 1); p.hLy = (int8_t)(sh + 2);
      p.elbowOut = 1;
      if (sword) { p.wDir = 6; p.wShort = 2; }
      break;
    case FUp:
      p.headY = -1;
      p.hRx = (int8_t)(B.shoulderHalf + 1); p.hRy = (int8_t)(sh + 2);
      p.hLx = (int8_t)(-B.shoulderHalf - 3); p.hLy = (int8_t)(sh + 2);
      p.elbowOut = 1;
      if (sword) { p.wDir = 2; p.wShort = 2; }
      break;
    case FRight:
      p.headX = -1; p.lean = -1;
      p.hRx = -6; p.hRy = (int8_t)(sh + 3); p.hLx = -4; p.hLy = (int8_t)(sh + 2); p.hLfront = 0;
      p.fRx = 1; p.fLx = -2;
      if (sword) { p.wDir = 5; p.wShort = 1; }
      break;
    case FLeft:
      p.headX = 1; p.lean = 1;
      p.hLx = 4; p.hLy = (int8_t)(sh + 3); p.hRx = 2; p.hRy = (int8_t)(sh + 2); p.hRfront = 0;
      p.fLx = -2; p.fRx = 1;
      if (sword) { p.wDir = 3; p.wShort = 1; p.wBehind = 1; }
      break;
  }
  return p;
}

// the buckle leans toward the side the body falls to (side -1 west, +1 east), so the topple and the lying body that
// follow it are one continuous movement: front views slump sideways; side views slump forward when they fall forward
// and sag back when they fall backward
Pose buckle(const Body& B, int fc, bool sword, int side) {
  Pose p = restPose(B, fc, false);
  p.expr = ExClosed;
  p.hatOff = 1;   // a cap or a straw hat tips off the bowed head (it lies beside the head: lying)
  p.dy = 3;
  const int hip = B.hip + 3;
  switch (fc) {
    case FDown: case FUp: {
      p.headY = 1;
      p.lean = (int8_t)(2 * side); p.headX = (int8_t)side;
      const bool dn = fc == FDown;
      const int l = -B.shoulderHalf + 1, r = B.shoulderHalf - 3;
      if (dn) { p.hRx = (int8_t)(l + side); p.hLx = (int8_t)(r + side); } else { p.hRx = (int8_t)(r + side); p.hLx = (int8_t)(l + side); }
      // the arm on the fall side hangs lower, the other swings out a little
      p.hRy = p.hLy = (int8_t)(hip + 2);
      int8_t& low = ((side < 0) == dn) ? p.hRy : p.hLy;
      low = (int8_t)(low + 1);
      if (sword) { p.wDir = (int8_t)(side < 0 ? 5 : 3); p.wShort = 4; }
      // the knees give way toward the fall: the far-side foot slides under the body
      if (side < 0) { if (dn) p.fLx = (int8_t)(p.fLx - 1); else p.fRx = (int8_t)(p.fRx - 1); }
      else { if (dn) p.fRx = (int8_t)(p.fRx + 1); else p.fLx = (int8_t)(p.fLx + 1); }
      break;
    }
    case FRight:
      if (side > 0) {   // forward: slumped over the knees
        p.lean = 2; p.headX = 1; p.headY = 1;
        p.hRx = 1; p.hRy = (int8_t)(hip + 3); p.hLx = 2; p.hLy = (int8_t)(hip + 2); p.hLfront = 0;
        p.fRx = 2; p.fLx = -1;
        if (sword) { p.wDir = 3; p.wShort = 4; }
      } else {          // backward: the hips drop, the chest sags back, the head lolls
        p.lean = -2; p.headX = -1;
        p.hRx = -3; p.hRy = (int8_t)(hip + 1); p.hLx = -2; p.hLy = (int8_t)(hip); p.hLfront = 0;
        p.fRx = 2; p.fLx = 0;
        if (sword) { p.wDir = 5; p.wShort = 3; }
      }
      break;
    case FLeft:
      if (side < 0) {   // forward
        p.lean = -2; p.headX = -1; p.headY = 1;
        p.hLx = -3; p.hLy = (int8_t)(hip + 3); p.hRx = -4; p.hRy = (int8_t)(hip + 2); p.hRfront = 0;
        p.fLx = -3; p.fRx = 0;
        if (sword) { p.wDir = 5; p.wShort = 4; p.wBehind = 1; }
      } else {          // backward
        p.lean = 2; p.headX = 1;
        p.hLx = 1; p.hLy = (int8_t)(hip + 1); p.hRx = 0; p.hRy = (int8_t)(hip); p.hRfront = 0;
        p.fLx = -3; p.fRx = -1;
        if (sword) { p.wDir = 3; p.wShort = 3; p.wBehind = 1; }
      }
      break;
  }
  return p;
}

// the topple: the side figure tipped toward the fall side (painted, never rotated). Backward (west: the right-facing
// figure; east: the left-facing one) for a body facing down, up or away from the fall; forward for a side-on body that
// falls the way it faces (knocked from behind). The grip has gone: the blade tips away from the loose hand.
Pose topple(const Body& B, bool west, bool sword, bool forward) {
  const bool faceRight = forward ? !west : west;
  Pose p = restPose(B, faceRight ? FRight : FLeft, false);
  const int s = faceRight ? 1 : -1;   // +1: the figure faces right
  p.expr = ExClosed;
  p.hatOff = 1;   // a cap or a straw hat flies off as the body goes over (it lies beside the head: lying)
  p.dy = 6;
  p.elbowOut = 1;
  const int sh = B.shoulder + 6;
  if (!forward) {
    p.lean = (int8_t)(-7 * s);
    p.headX = (int8_t)(-1 * s);
    // the arms swing up and forward as the body goes back (clear of the face), the blade tipping out of the hand
    // into the open space in front of the body
    if (s > 0) {
      p.fRx = 3; p.fLx = 1;
      p.hRx = 0; p.hRy = (int8_t)(sh - 4); p.hLx = -1; p.hLy = (int8_t)(sh - 5); p.hLfront = 0;
      if (sword) { p.wDir = 2; p.wShort = 3; }
    } else {
      p.fLx = -4; p.fRx = -2;
      p.hLx = -2; p.hLy = (int8_t)(sh - 4); p.hRx = -1; p.hRy = (int8_t)(sh - 5); p.hRfront = 0;
      if (sword) { p.wDir = 6; p.wShort = 3; p.wBehind = 1; }
    }
  } else {
    // pitched forward over the feet, the head down, the arms thrown out ahead to break the fall
    p.lean = (int8_t)(7 * s);
    p.headX = (int8_t)s; p.headY = 1;
    p.dy = 7;
    if (s > 0) {
      p.fRx = -1; p.fLx = -3; p.kR = 1;
      p.hRx = 7; p.hRy = (int8_t)(sh + 2); p.hLx = 6; p.hLy = (int8_t)(sh + 1); p.hLfront = 0;
      if (sword) { p.wDir = 1; p.wShort = 3; }
    } else {
      p.fLx = 0; p.fRx = 2; p.kL = 1;
      p.hLx = -9; p.hLy = (int8_t)(sh + 2); p.hRx = -8; p.hRy = (int8_t)(sh + 1); p.hRfront = 1;
      if (sword) { p.wDir = 7; p.wShort = 3; }
    }
  }
  return p;
}

// the figure laid on the ground (see the file comment)
Cut lying(const HumanLook& look, const Body& B, const Kit& K, bool west, bool sword) {
  Pose p = restPose(B, FLeft, false);
  p.expr = ExClosed;
  p.hLx = -4; p.hLy = (int8_t)(B.hip + 1);          // the near arm loose beside the body
  p.hRx = 4; p.hRy = (int8_t)(B.hip - 1);           // the far hand open on the near side (it shows once laid down)
  p.hRfront = 0;
  p.fLx = -2; p.fRx = 2;   // the legs a little apart: two legs on the ground with a sliver between, never one slab
  if (sword) { p.wDir = 4; p.wBehind = 1; p.wShort = 0; }
  if (look.weapon == 3) { p.bowRest = 1; p.wDir = 8; }   // the bow lies along the body by the near hand
  p = withWeapon(p, look.weapon);
  p.hatOff = 1;            // a cap or a straw hat has fallen off (it would stand up off the ground: lying stays 16 px)
  const int W = LOCO_W, H = 56, feet = LOCO_FEET;
  RigAnchors an;
  const Canvas raw = paintRaw(look, B, K, p, W, H, feet, an);
  const int pivX = W / 2, pivY = feet;
  Canvas t(H, W);
  int px, py;
  // the middle of the body (an adult's hips, 16 rows over the soles; a kid's half its height) and the back's outline row
  const int mid = B.age ? -(1 - B.headTop + 1) / 2 : -16;
  if (west) {   // transpose: (x, y) -> (y, x)
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) t.set(y, x, raw.get(x, y));
    px = pivY + mid;
    py = pivX + 4;
  } else {      // a quarter turn clockwise: (x, y) -> (H - 1 - y, x)
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) t.set(H - 1 - y, x, raw.get(x, y));
    px = H - 1 - (pivY + mid);
    py = pivX + 4;
  }
  if (B.age) {   // a kid's slighter back: the pivot on the row under its lowest painted row (where the outline lands)
    int x0, y0, x1, y1;
    bounds(t, x0, y0, x1, y1);
    if (y1 >= y0) py = y1 + 1;
  }
  // a cap or a straw hat that fell off lies on the ground a px beyond the head (it never vanishes with the fall)
  const int kind = K.hood ? 1 : K.headwear;
  if (kind == 2 || kind == 7) {
    static const char* const cone[4] = {"...32..", "..3443.", "2344432", ".12221."};
    static const char* const cap[3] = {".2332.", "234432", ".1221."};
    const char* const* m = kind == 7 ? cone : cap;
    const int rows = kind == 7 ? 4 : 3, cols = kind == 7 ? 7 : 6;
    const Ramp5& R = kind == 7 ? K.straw : K.head;
    int x0, y0, x1, y1;
    bounds(t, x0, y0, x1, y1);
    if (x1 >= x0) {
      const int bx = west ? x0 - 2 - cols : x1 + 3, by = y1 - rows + 1;
      for (int r = 0; r < rows; r++)
        for (int i = 0; i < cols; i++)
          if (m[r][i] >= '0' && m[r][i] <= '4') t.set(bx + i, by + r, R[m[r][i] - '0']);
    }
  }
  RigAnchors none;
  return finishFrame(std::move(t), px, py, none);
}

}  // namespace

RigSheet rigDeath(const HumanLook& look) {
  const Body B = bodyOf(look);
  Kit K;
  makeKit(look, K);
  const bool sword = armedLook(look);
  SheetBuilder sb;
  // the topples: backward to the west / east, and forward (a side-on body falling the way it faces)
  const int w = look.weapon;
  const Cut topW = paintFrame(look, B, K, withWeapon(topple(B, true, sword, false), w), LOCO_W, LOCO_H, LOCO_FEET);
  const Cut topE = paintFrame(look, B, K, withWeapon(topple(B, false, sword, false), w), LOCO_W, LOCO_H, LOCO_FEET);
  const Cut fwdW = paintFrame(look, B, K, withWeapon(topple(B, true, sword, true), w), LOCO_W, LOCO_H, LOCO_FEET);
  const Cut fwdE = paintFrame(look, B, K, withWeapon(topple(B, false, sword, true), w), LOCO_W, LOCO_H, LOCO_FEET);
  const Cut lyW = lying(look, B, K, true, sword);
  const Cut lyE = lying(look, B, K, false, sword);
  // Fall per facing and side (variant 0 west, 1 east: the Lying variant it ends in): the recoil is the facing's own,
  // the buckle leans toward the side, the topple tips that way
  for (int fc = 0; fc < RIG_FACINGS; fc++) {
    const int rec = sb.add(fc, Clip::Fall, 0, paintFrame(look, B, K, withWeapon(recoil(B, fc, sword), w), LOCO_W, LOCO_H, LOCO_FEET));
    sb.add(fc, Clip::Fall, 0, paintFrame(look, B, K, withWeapon(buckle(B, fc, sword, -1), w), LOCO_W, LOCO_H, LOCO_FEET));
    sb.add(fc, Clip::Fall, 0, Cut(fc == FLeft ? fwdW : topW));
    sb.addShared(fc, Clip::Fall, 1, rec);
    sb.add(fc, Clip::Fall, 1, paintFrame(look, B, K, withWeapon(buckle(B, fc, sword, 1), w), LOCO_W, LOCO_H, LOCO_FEET));
    sb.add(fc, Clip::Fall, 1, Cut(fc == FRight ? fwdE : topE));
  }
  for (int fc = 0; fc < RIG_FACINGS; fc++) {
    sb.add(fc, Clip::Lying, 0, Cut(lyW));
    sb.add(fc, Clip::Lying, 1, Cut(lyE));
  }
  return sb.finish();
}

}  // namespace rig2
}  // namespace art
