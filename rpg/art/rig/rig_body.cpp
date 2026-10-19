// EMBERVALE art, v2 rig (RIG-BODY, P2a): man and woman x slim / average / broad x human / half-breed / elf, skins
// S1-S8, the locomotion atlas (Idle 3, Walk 6, Hurt 1 in four facings) and the statue figure (STYLE_P2 2, 3, 14).
#include <algorithm>
#include <cmath>
#include "rpg/art/rig/rig_internal.h"

namespace art {
namespace rig2 {

// ---------------------------------------------------------------- the body base (STYLE_P2 3, 14)
Body bodyOf(const HumanLook& L) {
  Body b;
  b.woman = L.body == 1 ? 1 : 0;
  b.people = L.people > 2 ? 0 : L.people;
  b.age = L.age > 2 ? 0 : L.age;
  b.build = L.build > 2 ? 0 : L.build;
  if (b.people == 2) b.build = L.build == 2 ? 0 : 1;        // elves a step slimmer
  else if (b.people == 1 && b.build == 0) b.build = 1;      // half-breeds a touch slimmer (one step at average)
  b.ear = b.people;
  b.chinW = b.woman || b.people == 2 ? 4 : 6;
  if (b.age == 1) {
    // the child (STYLE_P2 3): 21 px; an 11 px head (one forehead row fewer: rounder), torso 5 rows (shoulders F-9 ..
    // hips F-5) with the chin over the collar, legs 5; shoulders 10 incl. the outline, the arms over the torso's sides
    b.legs = 5; b.hip = -5; b.shoulder = b.hip - 4; b.neck = b.shoulder - 1; b.headTop = b.shoulder - 10;
    b.headRows = 11;
    b.shoulderHalf = 4; b.torsoHalf = 3; b.waistHalf = 3; b.hipHalf = 3;
    b.stance = 1; b.armSwing = 2; b.armU = 3; b.armF = 2; b.stride = 4; b.chinW = 4;
    b.legW = 2; b.hipJoint = 2.0f; b.shoeW = 3;
    b.build = 0; b.trimR = 0;
    return b;
  }
  if (b.age == 2) {
    // the youth: 26 px; the adult's 12 px head, torso 6 rows (shoulders F-12 .. hips F-7), legs 7; shoulders 12
    b.legs = 7; b.hip = -7; b.shoulder = b.hip - 5; b.neck = b.shoulder - 1; b.headTop = b.neck - 11;
    b.shoulderHalf = 5; b.torsoHalf = 4; b.waistHalf = 3 + b.woman * 0; b.hipHalf = 4;
    b.stance = 1; b.armSwing = 2; b.armU = 3; b.armF = 3; b.stride = 5; b.chinW = 4;
    b.build = 0; b.trimR = 0;
    return b;
  }
  b.legs = (b.woman ? 10 : 11) + (b.people == 2 ? 1 : 0);   // an elf stands 1 px taller, all of it in the legs (STYLE_P2 3)
  b.hip = -b.legs;
  b.shoulder = b.hip - 7;
  b.neck = b.shoulder - 1;
  b.headTop = b.hip - 19;
  // the builds (STYLE_P2 3: shoulders incl. the arms and the outline, man 15 / 16 / 18, woman 13 / 14 / 16): slim takes
  // the odd width with the right edge 1 px in (the shadow side), its waist the narrowest; broad heavy shoulders and a
  // deep chest over the same hips (the feet, hands and hips stay within 1 px of the average: one skeleton, STYLE_P2 14)
  if (b.woman) {
    b.shoulderHalf = 6; b.torsoHalf = 4; b.waistHalf = 4; b.hipHalf = 5; b.stance = 1; b.armSwing = 2;
    if (b.build == 1) b.trimR = 1;
    else if (b.build == 2) { b.shoulderHalf = 7; b.torsoHalf = 5; b.chestDepth = 1; }
  } else {
    b.shoulderHalf = 7; b.torsoHalf = 5; b.waistHalf = 4; b.hipHalf = 5; b.stance = 2; b.armSwing = 3;
    if (b.build == 1) b.trimR = 1;
    else if (b.build == 2) { b.shoulderHalf = 8; b.torsoHalf = 6; b.waistHalf = 5; b.chestDepth = 1; }
  }
  return b;
}

namespace {

// the feet's columns at rest (front views: the screen-left leg's ankle centre and the screen-right one's)
void restFeet(const Body& B, int& left, int& right) {
  left = -2 - B.stance;
  right = B.legW == 2 ? B.stance : 1 + B.stance;   // (2 px legs stand straight under their hips)
}

// the hands at rest (fist top-left): screen-left and screen-right
void restHands(const Body& B, int& lx, int& rx, int& y) {
  lx = -B.shoulderHalf - 1;
  rx = B.shoulderHalf - 1 - B.trimR;
  y = B.hip + 1;
}

Pose base(const Body& B, int facing) {
  Pose p;
  p.facing = (uint8_t)facing;
  int fl, fr, hl, hr, hy;
  restFeet(B, fl, fr);
  restHands(B, hl, hr, hy);
  if (facing == FDown) { p.fRx = (int8_t)fl; p.fLx = (int8_t)fr; p.hRx = (int8_t)hl; p.hLx = (int8_t)hr; }
  else if (facing == FUp) { p.fRx = (int8_t)fr; p.fLx = (int8_t)fl; p.hRx = (int8_t)hr; p.hLx = (int8_t)hl; }
  else {
    const int s = facing == FRight ? 1 : -1;
    // the near foot under the hips, the far foot a step behind
    if (s > 0) { p.fRx = 0; p.fLx = -2; p.hRx = -2; p.hLx = -1; p.hLfront = 0; }
    else { p.fLx = -1; p.fRx = 1; p.hLx = 0; p.hRx = -1; p.hRfront = 0; }
  }
  p.hRy = p.hLy = (int8_t)hy;
  return p;
}

// the sword at rest (STYLE_P2 9, 11): held upright before the weapon shoulder, the guard and pommel either side of the
// fist; side views carry it over the shoulder
void restWeapon(const Body& B, Pose& p) {
  const int hy = B.shoulder + 3;
  switch (p.facing) {
    case FDown: p.hRx = -8; p.hRy = (int8_t)hy; p.wDir = 12; break;   // (both bodies: the hands within 1 px, STYLE_P2 14)
    case FUp: p.hRx = 6; p.hRy = (int8_t)hy; p.wDir = 12; p.wBehind = 0; break;
    // side views: the arm hangs, the blade points forward and down past the body
    case FRight: p.hRx = 0; p.hRy = (int8_t)(B.hip + 1); p.wDir = 1; p.wBehind = 0; break;
    case FLeft: p.hRx = -5; p.hRy = (int8_t)(B.hip + 1); p.wDir = 7; p.wBehind = 1; p.hRfront = 0; break;
  }
}

// what a LOCO frame carries: the sword (the slice's rest), any other weapon rig_arms.cpp draws, carried at RIG-ARMS's
// rest (armsRest), or the bow braced in the off hand; 0 = empty hands
int carried(const HumanLook& L) { return (L.weapon == 1 || L.weapon == 3 || armsDrawn(L.weapon)) ? L.weapon : 0; }
// the bow at rest (STYLE_P2 11: the off hand holds the bow): braced and upright at the off side, its tips bending back
// toward the body; side-on the off arm brings it before the body (the far hand shows when it holds something, STYLE_P2 5)
void bowRest(const Body& B, Pose& p) {
  const int y = B.hip - 3;   // (its lower tip clears the ground: the 20 px stave is centred on the fist)
  p.bowRest = 1;
  switch (p.facing) {
    case FDown: p.hLx = (int8_t)(B.shoulderHalf - 1 - B.trimR); p.hLy = (int8_t)y; p.wDir = 0; break;
    case FUp: p.hLx = (int8_t)(-B.shoulderHalf - 1); p.hLy = (int8_t)y; p.wDir = 8; break;
    case FRight: p.hLx = 3; p.hLy = (int8_t)y; p.hLfront = 1; p.wDir = 0; break;
    case FLeft: p.hLx = -5; p.hLy = (int8_t)y; p.wDir = 8; break;
  }
}
void carryRest(const Body& B, int weapon, Pose& p) {
  if (weapon == 1) { restWeapon(B, p); return; }
  if (weapon == 3) { bowRest(B, p); return; }
  armsRest(B, weapon, p);
  // (until armsRest carries the greatsword point-down side-on, as its comment says, it points it forward like the
  // sword: 24 px out in front makes every side frame 30 px wide and the LOCO atlas 136 KB, over R6's 128; tipped to the
  // ground before the feet, foreshortened, it stays within them)
  if (weapon == 7 && (p.facing == FRight || p.facing == FLeft) && (p.wDir == 1 || p.wDir == 7)) {
    p.wDir = p.facing == FRight ? 2 : 6;
    p.wShort = 4;
  }
}

}  // namespace

Pose restPose(const Body& B, int facing, bool sword) {
  Pose p = base(B, facing);
  if (sword) restWeapon(B, p);
  return p;
}

RigSheet rigLoco(const HumanLook& look) {
  const Body B = bodyOf(look);
  Kit K;
  makeKit(look, K);
  SheetBuilder sb;
  const int carry = carried(look);
  const bool sword = carry != 0;   // (any weapon in hand: the weapon hand keeps its grip through the clips)
  for (int fc = 0; fc < RIG_FACINGS; fc++) {
    const bool side = fc == FRight || fc == FLeft;
    const int s = fc == FLeft ? -1 : 1;
    // ---- Idle 3: neutral, breathe (shoulders and chest up 1 px), blink
    Pose idle = base(B, fc);
    if (B.woman && !side) idle.hipX = fc == FDown ? -1 : 0;   // weight on one leg
    if (sword) carryRest(B, carry, idle);
    Cut neutral = paintFrame(look, B, K, idle, LOCO_W, LOCO_H, LOCO_FEET);
    Cut blink = blinkOf(neutral, B, K, fc);   // the same frame with the eyes shut (rig_face.cpp's ExBlink, pixel for pixel)
    sb.add(fc, Clip::Idle, 0, std::move(neutral));
    {
      // the breath: the shoulders, chest and arms rise 1 px under a still head (the crown never moves: the figure
      // stays 32 / 31 px in every frame, STYLE_P2 3 and 10)
      Pose p = idle;
      p.chest = -1; p.headY = 1; p.hLy = (int8_t)(p.hLy - 1); p.hRy = (int8_t)(p.hRy - 1);
      sb.add(fc, Clip::Idle, 0, paintFrame(look, B, K, p, LOCO_W, LOCO_H, LOCO_FEET));
    }
    sb.add(fc, Clip::Idle, 0, std::move(blink));
    // ---- Walk 6: contact, down, pass, contact, down, pass. The planted sole steps 5 / 4 / 5 px between frames and
    // rpg/sim/anim.cpp (kWalkAt) changes the frame on exactly those whole px of drawn travel, so the foot lands back on
    // its spot at every frame change: keep the two tables in step. Frame 5's planted foot is the idle stance's back
    // foot to the pixel (side views), so a walk starts there (animWalkBegin) and steps off without a skip.
    // the strides (side views): the adult's 7 px (the clock's 5 / 4 / 5 px steps: a 28 px cycle); a youth's 5 and a
    // child's 4 step 4 / 3 / 3 and 3 / 2 / 3 (20 and 16 px cycles: rpg/sim/anim.cpp keys kids' frames to them)
    static const int8_t kNear[3][6] = {{7, 2, -2, -7, -2, 2}, {5, 1, -2, -5, -1, 2}, {4, 1, -1, -4, -1, 1}};
    const int strideRow = B.stride >= 7 ? 0 : B.stride >= 5 ? 1 : 2;
    Pose walk[6];
    for (int i = 0; i < 6; i++) {
      Pose p = base(B, fc);
      static const int8_t bob[6] = {0, 1, 0, 0, 1, 0};
      p.dy = bob[i];
      // secondary motion (STYLE_P2 10, depth): a braid's or a tail's hanging part swings one frame after the bob (side
      // views: back on the pass frames; front and back views: out to alternate sides); the sheen dips with the head
      static const int8_t swingSide[6] = {0, 0, 1, 0, 0, 1}, swingFront[6] = {0, 0, 1, 0, 0, -1};
      p.hairSwing = side ? swingSide[i] : swingFront[i];
      p.sheen = (uint8_t)bob[i];
      if (side) {
        const int8_t* near = kNear[strideRow];
        static const int8_t nearLift[6] = {0, 0, 0, 0, 1, 2};
        const int8_t far[6] = {(int8_t)-near[0], (int8_t)-near[1], (int8_t)-near[2], (int8_t)-near[3], (int8_t)-near[4], (int8_t)-near[5]};
        static const int8_t farLift[6] = {0, 1, 2, 0, 0, 0};
        static const int8_t handNear[6] = {-3, -2, 1, 3, 2, -1};
        const int sw = B.armSwing;
        auto hx = [&](int v) { return (int8_t)std::lround(v * sw / 3.0f); };
        if (s > 0) {
          p.fRx = near[i]; p.fRy = nearLift[i]; p.fLx = far[i]; p.fLy = farLift[i];
          p.hRx = (int8_t)(hx(handNear[i]) - 1); p.hLx = (int8_t)(hx(-handNear[i]) - 1); p.hLfront = 0;
        } else {
          p.fLx = (int8_t)(-near[i] - 1); p.fLy = nearLift[i]; p.fRx = (int8_t)(-far[i] - 1); p.fRy = farLift[i];
          p.hLx = (int8_t)(hx(-handNear[i]) - 1); p.hRx = (int8_t)(hx(handNear[i]) - 1); p.hRfront = 0;
        }
        const int hy = B.hip + 1;
        p.hRy = (int8_t)(hy + p.dy - (std::abs(handNear[i]) >= 3 ? 1 : 0));
        p.hLy = (int8_t)(hy + p.dy - (std::abs(handNear[i]) >= 3 ? 1 : 0));
      } else {
        // front / back: the screen-left leg is the right leg facing down
        static const int8_t liftA[6] = {0, 0, 0, 1, 2, 1}, liftB[6] = {1, 2, 1, 0, 0, 0};
        static const int8_t hdx[6] = {0, 0, 0, 1, 1, 0}, hdy[6] = {-1, -1, 0, 1, 1, 0};
        const bool dn = fc == FDown;
        int8_t& la = dn ? p.fRy : p.fLy;   // the screen-left leg's lift
        int8_t& lb = dn ? p.fLy : p.fRy;
        la = liftA[i]; lb = liftB[i];
        int8_t& ax = dn ? p.hRx : p.hLx;  int8_t& ay = dn ? p.hRy : p.hLy;   // the screen-left hand
        int8_t& bx = dn ? p.hLx : p.hRx;  int8_t& by = dn ? p.hLy : p.hRy;
        const int j = (i + 3) % 6;
        ax = (int8_t)(ax + hdx[i]); ay = (int8_t)(ay + hdy[i] + p.dy);
        bx = (int8_t)(bx - hdx[j]); by = (int8_t)(by + hdy[j] + p.dy);
      }
      if (sword) {
        Pose r = p;
        carryRest(B, carry, r);
        if (carry == 3) {   // the bow hand holds still (the empty hand keeps its swing)
          p.hLx = r.hLx; p.hLy = (int8_t)(r.hLy + p.dy); p.hLfront = r.hLfront; p.wDir = r.wDir; p.bowRest = 1;
        } else {
          p.hRx = r.hRx; p.hRy = (int8_t)(r.hRy + p.dy); p.wDir = r.wDir; p.wBehind = r.wBehind; p.hRfront = r.hRfront;
          if (side && s > 0) p.hLfront = 0;
        }
      }
      walk[i] = p;
    }
    for (int i = 0; i < 6; i++) {
      Pose p = walk[i];
      // a hanging hem follows the last frame's feet (it lags one frame behind the legs)
      const Pose& q = walk[(i + 5) % 6];
      p.hemLag = 1; p.hemRx = q.fRx; p.hemLx = q.fLx;
      sb.add(fc, Clip::Walk, 0, paintFrame(look, B, K, p, LOCO_W, LOCO_H, LOCO_FEET));
    }
    // ---- Hurt 1: the flinch (head back, shoulders up, eyes squeezed)
    {
      Pose p = idle;
      p.expr = ExHurt;
      p.chest = -1;
      if (side) { p.lean = (int8_t)(-s); p.headX = (int8_t)(-s); }
      else { p.headY = -1; }
      const bool held = sword && carry != 3;   // the weapon hand keeps its grip (the bow is in the off hand)
      if (!side) {
        const int out = fc == FDown ? 1 : -1;
        p.hLx = (int8_t)(p.hLx + out); p.hLy = (int8_t)(p.hLy - 2);
        if (!held) { p.hRx = (int8_t)(p.hRx - out); p.hRy = (int8_t)(p.hRy - 2); }
        else p.hRy = (int8_t)(p.hRy - 1);
      } else {
        if (s > 0) { p.hLx = (int8_t)(p.hLx + 2); p.hLy = (int8_t)(p.hLy - 2); if (!held) { p.hRx = (int8_t)(p.hRx + 3); p.hRy = (int8_t)(p.hRy - 3); } }
        else { p.hRx = (int8_t)(p.hRx - 2); p.hRy = (int8_t)(p.hRy - 2); if (!held) { p.hLx = (int8_t)(p.hLx - 3); p.hLy = (int8_t)(p.hLy - 3); } }
      }
      sb.add(fc, Clip::Hurt, 0, paintFrame(look, B, K, p, LOCO_W, LOCO_H, LOCO_FEET));
    }
  }
  return sb.finish();
}

// ---------------------------------------------------------------- the slice's sword swings (STYLE_P2 12)
// The template every RIG-ARMS family follows: three blows that read as three in every facing.
//   hit 1 forehand   a horizontal sweep at chest height from the weapon side, across the front, round to the off side
//   hit 2 backhand   the mirror path with the same hand: cocked over the off shoulder, across the front, out low on the
//                    weapon side (each blow ends where the next begins, so a combo flows)
//   hit 3 finisher   raised overhead, an overhead vertical cut down to the low front, the body dropping into it
// Each is Windup 2 + Strike 2 + Follow 2. The paths are the swing's real geometry seen in the 3/4 view: a horizontal
// circle round the chest shows as an ellipse, so toward the viewer reads as down on the screen (foreshortened) and
// away as up (behind the body); the blade points out from the body along the circle.
//   right  the weapon hand is the NEAR hand: hit 1 near-back -> forward-near -> forward-far -> far (behind the body);
//          hit 2 far-back (behind) -> forward-far -> forward-near -> low near; hit 3 up -> back -> over -> low front.
//          Hits 1 and 2 strike at shoulder height (the tip within 4 px of F-18).
//   left   the weapon hand is the FAR hand (painted, never mirrored). Its forehand goes far -> front -> near, which on
//          the screen is the right facing's backhand path mirrored, and its backhand the right forehand's: the left
//          facing takes the right keys mirrored with hits 1 and 2 swapped, the far arm in front of the torso while it
//          reaches forward or across (ff) and behind it at its own side; every strike passes in front of the body.
//   down   the weapon hand on the screen's left: back over the shoulder (behind, up), across the front (down-left,
//          down-right: over the body), round the far side; the cut comes straight at the viewer to the low front.
//   up     the weapon hand on the screen's right: back = toward the viewer (down, in front), the front of the circle
//          beyond the head (up, behind: the blade shows above the head and shoulders); the cut drops the blade from
//          down the back to over the head.
// rpg_test --rig and the judge's tips.sh read the tip angle about the grip from the anchors: per facing, the three
// variants differ by 45 degrees or more on windup frame 0 and on a strike frame; hit 3 runs from 80 or more to -45 or less.
namespace {

struct SwingKey {
  int8_t hx, hy, dir, behind;   // the weapon hand (fist top-left, man), the blade's direction (0..15 clockwise from
                                //   pointing right), the blade drawn behind the body
  int8_t ox, oy;                // the off hand
  int8_t lean, dy;              // the torso's lean (+ = toward the facing in side views; + = screen right in front views);
                                //   crouch (+ down, - up on the toes)
  int8_t fr, fl;                // the feet (side: near / far ankle column; front views: the screen-left / screen-right leg)
  int8_t expr, shorten, elbow;  // ExEffort on the blow; the blade foreshortened (toward or away from the viewer); raised
                                //   elbows bend out
  int8_t ff;                    // (side keys, used by the left facing) the far weapon arm drawn in front of the torso
};
// [variant][frame] for facing right (side: near foot fr, far foot fl). Shoulder row F-18; the near shoulder joint at
// (-1, -17.5); an arm reaches about 8 px to the wrist.
const SwingKey kSwingRight[3][6] = {
    {   // hit 1 forehand: drawn back low on the near side -> through the front at shoulder height -> round the far side
        {-6, -13, 6, 0, 4, -15, -1, 0, 3, -3, 0, 0, 0, 1}, {-8, -16, 7, 0, 4, -16, -2, 1, 3, -4, 0, 0, 0, 1},
        {5, -16, 0, 0, -2, -14, 1, 1, 5, -4, ExEffort, 2, 0, 1}, {6, -16, 15, 0, -3, -13, 2, 1, 6, -5, ExEffort, 2, 0, 1},
        {5, -18, 13, 1, -3, -14, 2, 0, 6, -5, 0, 0, 1, 0}, {2, -21, 11, 1, -2, -15, 1, 0, 4, -3, 0, 0, 1, 0}},
    {   // hit 2 backhand: cocked over the far shoulder (behind) -> through the front at shoulder height -> low near side
        {-3, -20, 9, 1, 3, -14, -1, 0, 3, -3, 0, 0, 1, 0}, {-5, -21, 8, 1, 3, -15, -2, 1, 3, -4, 0, 0, 1, 0},
        {2, -16, 15, 0, 2, -15, 0, 0, 4, -3, ExEffort, 2, 0, 1}, {7, -17, 1, 0, 1, -14, 1, 0, 4, -3, ExEffort, 6, 0, 1},
        {4, -14, 3, 0, 0, -13, 2, 1, 5, -3, 0, 1, 0, 1}, {0, -12, 5, 0, -1, -13, 1, 0, 4, -3, 0, 0, 0, 1}},
    {   // hit 3 overhead: raised high on the toes -> back behind the head -> over the top -> down to the low front
        {-5, -27, 12, 1, 3, -16, -1, -1, 2, -3, 0, 0, 1, 0}, {-5, -25, 10, 1, 3, -16, -2, -1, 2, -4, 0, 0, 1, 0},
        {-1, -28, 13, 1, 3, -16, 0, -1, 4, -3, ExEffort, 0, 1, 0}, {5, -12, 4, 0, 3, -14, 3, 2, 6, -4, ExEffort, 1, 0, 1},
        {5, -10, 3, 0, 3, -12, 3, 3, 6, -4, 0, 1, 0, 1}, {3, -10, 3, 0, 2, -12, 2, 2, 5, -4, 0, 2, 0, 1}},
};
// facing down: the weapon hand on the screen's left; shoulder joints at (-6, -16.5) and (6, -16.5)
const SwingKey kSwingDown[3][6] = {
    {   // hit 1 forehand: back over the right shoulder -> down-left, down-right across the front -> round the left
        {-12, -20, 9, 1, 6, -12, -1, 0, -5, 4, 0, 0, 1, 0}, {-12, -21, 10, 1, 6, -12, -2, 1, -5, 4, 0, 0, 1, 0},
        {-9, -17, 7, 0, 6, -13, -1, 1, -5, 4, ExEffort, 2, 0, 0}, {1, -16, 1, 0, 7, -14, 2, 1, -5, 4, ExEffort, 2, 0, 0},
        {3, -18, 15, 1, 7, -15, 2, 0, -5, 4, 0, 0, 0, 0}, {2, -21, 13, 1, 6, -16, 1, 0, -5, 4, 0, 0, 1, 0}},
    {   // hit 2 backhand: cocked over the left shoulder -> down-right, down-left across the front -> round the right
        {4, -19, 15, 1, 8, -11, 2, 0, -5, 4, 0, 0, 1, 0}, {4, -20, 14, 1, 8, -11, 3, 0, -5, 4, 0, 0, 1, 0},
        {2, -16, 1, 0, 6, -13, 1, 1, -5, 4, ExEffort, 2, 0, 0}, {-9, -17, 7, 0, 6, -14, -2, 1, -5, 4, ExEffort, 2, 0, 0},
        {-12, -18, 9, 1, 6, -15, -2, 0, -5, 4, 0, 0, 0, 0}, {-11, -20, 11, 1, 6, -16, -1, 0, -5, 4, 0, 0, 1, 0}},
    {   // hit 3 overhead: raised above the right shoulder on the toes -> over the top -> straight at the viewer, low
        {-7, -26, 12, 1, 4, -20, 0, -1, -4, 3, 0, 0, 1, 0}, {-6, -27, 12, 1, 4, -21, 0, -1, -4, 3, 0, 2, 1, 0},
        {-7, -20, 4, 0, 8, -19, 0, 0, -5, 4, ExEffort, 4, 0, 0}, {-6, -13, 5, 0, 7, -16, -1, 2, -6, 5, ExEffort, 1, 0, 0},
        {-5, -11, 5, 0, 4, -11, 0, 3, -6, 5, 0, 2, 0, 0}, {-4, -11, 4, 0, 4, -11, 0, 2, -5, 4, 0, 4, 0, 0}},
};
// facing up: the weapon hand on the screen's right; shoulder joints at (6, -16.5) and (-6, -16.5)
const SwingKey kSwingUp[3][6] = {
    {   // hit 1 forehand: back toward the viewer on the right -> over the front beyond the head -> round the left
        {9, -15, 1, 0, -7, -13, 1, 0, 4, -5, 0, 0, 0, 0}, {10, -14, 2, 0, -7, -13, 2, 1, 4, -5, 0, 0, 0, 0},
        {3, -21, 15, 1, -7, -14, 1, 1, 4, -5, 0, 0, 1, 0}, {-2, -21, 9, 1, -8, -15, -1, 1, 4, -5, 0, 0, 1, 0},
        {-4, -16, 7, 0, -8, -15, -2, 0, 4, -5, 0, 0, 0, 0}, {-3, -13, 6, 0, -7, -14, -1, 0, 4, -5, 0, 0, 0, 0}},
    {   // hit 2 backhand: back toward the viewer on the left -> over the front beyond the head -> round the right
        {-4, -17, 7, 0, -8, -12, -2, 0, 4, -5, 0, 0, 0, 0}, {-5, -16, 6, 0, -8, -12, -2, 1, 4, -5, 0, 0, 0, 0},
        {-2, -21, 9, 1, -8, -14, -1, 1, 4, -5, 0, 0, 1, 0}, {3, -21, 15, 1, -7, -15, 1, 1, 4, -5, 0, 0, 1, 0},
        {9, -16, 1, 0, -7, -15, 1, 0, 4, -5, 0, 0, 0, 0}, {9, -14, 2, 0, -7, -14, 0, 0, 4, -5, 0, 0, 0, 0}},
    {   // hit 3 overhead: the blade hanging down the back -> thrown up over the head -> forward and down beyond it
        {4, -26, 4, 0, -5, -20, 0, -1, 3, -4, 0, 0, 1, 0}, {3, -27, 4, 0, -5, -21, 0, -1, 3, -4, 0, 2, 1, 0},
        {2, -26, 12, 1, -5, -19, 0, 0, 4, -5, 0, 0, 1, 0}, {1, -22, 12, 1, -2, -21, 0, 2, 4, -5, 0, 0, 1, 0},
        {2, -17, 13, 1, -5, -13, 0, 3, 4, -5, 0, 3, 0, 0}, {2, -16, 13, 1, -5, -12, 0, 2, 4, -5, 0, 4, 0, 0}},
};

// the key of (facing, variant, frame): the left facing reads the right keys with hits 1 and 2 swapped (see above)
const SwingKey& swingKey(int fc, int v, int i) {
  if (fc == FDown) return kSwingDown[v][i];
  if (fc == FUp) return kSwingUp[v][i];
  if (fc == FLeft) return kSwingRight[v == 0 ? 1 : v == 1 ? 0 : 2][i];
  return kSwingRight[v][i];
}

Pose swingPose(const Body& B, int fc, const SwingKey& k) {
  Pose p = restPose(B, fc, false);
  const int wy = B.hip + 11;   // the woman's upper body is a row lower
  p.expr = (uint8_t)k.expr;
  p.dy = k.dy;
  p.wShort = k.shorten;
  p.elbowOut = (uint8_t)k.elbow;
  if (fc == FRight || fc == FLeft) {
    const int s = fc == FRight ? 1 : -1;
    auto mx = [&](int x) { return s > 0 ? x : -2 - x; };     // a fist's top-left mirrored
    auto mf = [&](int x) { return s > 0 ? x : -1 - x; };     // a column mirrored
    p.lean = (int8_t)(k.lean * s);
    p.hRx = (int8_t)mx(k.hx); p.hRy = (int8_t)(k.hy + wy);
    p.wDir = (int8_t)(s > 0 ? k.dir : (8 - k.dir + 16) % 16);
    p.wBehind = (uint8_t)k.behind;
    if (s > 0) {
      p.fRx = k.fr; p.fLx = k.fl;
      p.hLx = (int8_t)mx(k.ox); p.hLy = (int8_t)(k.oy + wy); p.hLfront = 0;
    } else {
      // the weapon in the far hand: the arm in front of the torso while it reaches forward or across (a blade in front
      // of the body always has its arm in front), behind it at its own side
      p.fLx = (int8_t)mf(k.fr); p.fRx = (int8_t)mf(k.fl);
      p.hLx = (int8_t)mx(k.ox); p.hLy = (int8_t)(k.oy + wy);
      p.hRfront = (uint8_t)((k.ff || !k.behind) ? 1 : 0);
    }
  } else {
    p.lean = k.lean;
    p.hRx = k.hx; p.hRy = (int8_t)(k.hy + wy);
    p.hLx = k.ox; p.hLy = (int8_t)(k.oy + wy);
    p.wDir = k.dir; p.wBehind = (uint8_t)k.behind;
    if (fc == FDown) { p.fRx = k.fr; p.fLx = k.fl; } else { p.fRx = k.fr; p.fLx = k.fl; }
  }
  return p;
}

}  // namespace

RigSheet sliceSwordAction(const HumanLook& look) {
  if (!(look.weapon == 1 || look.weapon == 7)) return RigSheet();
  const Body B = bodyOf(look);
  Kit K;
  makeKit(look, K);
  SheetBuilder sb;
  static const Clip clips[3] = {Clip::Windup, Clip::Strike, Clip::Follow};
  for (int fc = 0; fc < RIG_FACINGS; fc++)
    for (int v = 0; v < RIG_VARIANTS; v++)
      for (int c = 0; c < 3; c++)
        for (int i = 0; i < 2; i++)
          sb.add(fc, clips[c], v, paintFrame(look, B, K, swingPose(B, fc, swingKey(fc, v, c * 2 + i)), ACT_W, ACT_H, ACT_FEET));
  return sb.finish();
}

// ---------------------------------------------------------------- the Steel Arc (STYLE_P2 6, 12; rig_internal.h)
// A crescent on a circle round the cell's left-middle: the cutting rim one px of core, the blade's body edge colour
// inside it (both opaque), its innermost px half clear (a soft inner edge, no gradient). The head of the sweep is
// the thick end; the full frame thins at both ends like a new moon.
void steelArc(Canvas& c, int frame) {
  const uint32_t core = rgba(244, 241, 230), edge = rgba(184, 196, 216);
  struct Sweep { float a0, a1, wTail, wHead; int alpha; bool moon; };
  // visible angles (degrees: 0 points right, + clockwise = down the screen), widths at the tail and the head
  // (slice fix r2: every frame opaque, so the named colours land exactly on screen, STYLE_P2 6; the last frame fades by
  // thinning to the core and a 1 px edge at its head, not by alpha)
  static const Sweep kSweep[4] = {
      {-64, -30, 1, 3, 255, false}, {-64, 16, 1, 5, 255, false}, {-50, 64, 1, 5, 255, true}, {6, 64, 1, 2, 255, false}};
  const Sweep& s = kSweep[frame & 3];
  // drawn for a 32 px cell and scaled to the cell's height, so the finisher's 1.2x is painted natively (a 38 x 38 cell),
  // never a 32 px sprite stretched at runtime (STYLE_P2 1, 20: one pixel size)
  const float k = c.h > 0 ? c.h / 32.0f : 1.0f;
  const float cx = -1.0f * k, cy = 16.0f * k, R = 18.5f * k;
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
      const float a = std::atan2(dy, dx) * 57.29578f;
      if (a < s.a0 || a > s.a1) continue;
      const float t = (a - s.a0) / (s.a1 - s.a0);   // 0 the tail .. 1 the head
      const float w = (s.moon ? 1.0f + (s.wHead - 1.0f) * std::sin(t * 3.14159265f) : s.wTail + (s.wHead - s.wTail) * t) * (k > 1.0f ? k : 1.0f);
      const float d = R - std::sqrt(dx * dx + dy * dy);   // 0 on the outer rim, growing inward
      if (d < 0 || d >= w) continue;
      if (d < 1.0f) c.set(x, y, withA(core, s.alpha));
      else if (d < w - 1.0f || w < 2.5f) c.set(x, y, withA(edge, s.alpha));
      else c.set(x, y, withA(edge, s.alpha / 2));   // the soft inner edge: one half-clear px
    }
  if ((frame & 3) == 1) {   // the glint riding the head of the blade
    const float a = s.a1 / 57.29578f;
    const int gx = (int)std::floor(cx + std::cos(a) * (R - 0.5f)), gy = (int)std::floor(cy + std::sin(a) * (R - 0.5f));
    const uint32_t halo = rgba(255, 233, 168, 200);
    c.set(gx, gy, rgba(255, 255, 255));
    c.set(gx - 1, gy, halo); c.set(gx + 1, gy, halo); c.set(gx, gy - 1, halo); c.set(gx, gy + 1, halo);
  }
}

}  // namespace rig2

// true when every field of the look is painted on v2 (P2a: the plain looks RIG-BODY covers, R3 99 % of a census's plain
// looks; P2b makes it real per gear field and `rpg_test --rig-coverage` names what is missing). Until it holds, the view
// keeps drawing the look's v1 sheet, so `rig 2` never shows a half-painted person.
bool legacyRig2Covers(const HumanLook& L) {
  // the civilian base (RIG-BODY): every body, build, people and age; skins; the 8 hair styles and the beard; Tunic,
  // Dress, Robe and Rags with the 8 culture cuts and the 6 patterns; the 8 culture headwear kinds and the legacy hood;
  // face paint and jewellery. Gear is P2b's: armour, helms, cloaks and capes, shields, gloves, boots, back items, the
  // amulet and ring glints, the legendary glow, the culture arms forms and dials, alloy tints.
  if (L.helmet || L.helmStyle || L.cape || L.cloak || L.shield || L.shieldStyle || L.armorStyle || L.gloves || L.boots) return false;
  if (L.backItem || L.amulet || L.ring || L.glow || L.sheen || L.armourTint) return false;
  if (L.helmForm || L.bodyForm || L.shieldForm || L.bladeForm || L.bowForm || L.armsOrnament) return false;
  // the polearm: polearmForm 1 is cult::Polearm::Spear + 1, the plain spear every M6 spear wears (gear_look.cpp
  // polearmFormOf, looks.cpp) and the one RIG-ARMS's v2 spear paints (R7-R9); the culture glaive (2) and halberd (3) are P2b's
  if (L.polearmForm > (uint8_t)cult::Polearm::Spear + 1) return false;
  if (L.pauldron || L.skirt || L.crest) return false;
  if (!(L.outfit == Outfit::Tunic || L.outfit == Outfit::Dress || L.outfit == Outfit::Robe || L.outfit == Outfit::Rags)) return false;
  if ((int)L.hair >= (int)Hair::COUNT || L.cut > 8 || L.pattern > 6 || L.headwear > 8 || L.facePaint > 4) return false;
  if (L.people > 2 || L.age > 2 || L.build > 2 || L.body > 1) return false;
  // in hand: nothing, or a weapon LOCO carries (the sword, every art id rig_arms.cpp draws, at armsRest, and the bow in
  // the off hand) whose family RIG-ARMS's action library paints (R7-R10: the eight melee families and the bow's draw);
  // a ledger, scroll case or horn (art ids 10-12) is v1's
  return L.weapon == 0 || L.weapon == 1 || L.weapon == 3 || rig2::armsDrawn(L.weapon);
}

// humanFigureStill at rig 2 (statues and other props reuse the rig): an empty canvas = not painted yet (v1 is drawn)
Canvas legacyFigureStill2(const HumanLook& look, int facing) {
  (void)look; (void)facing;
  return Canvas();
}
}  // namespace art
