// EMBERVALE art, v2 rig (RIG-ARMS, P2a): the action library (STYLE_P2 10, 12): unarmed, dagger, sword, greatsword, axe,
// mace, spear, staff x 3 variants (hit 1 forehand, hit 2 backhand, hit 3 finisher) x (Windup 2, Strike 2, Follow 2) x
// 4 facings; Cast 2 for every family; the bow's Draw 3 (nock, chest, cheek) and Release 1 in every facing; Roll 4.
//
// How a frame is made. The body is RIG-BODY's own figure (rig_core.cpp paintRaw, the weapon left out), so an action
// frame is the same person as its LOCO and DEATH frames to the pixel, whatever RIG-BODY refines. The weapon (rig_arms.cpp
// paintArms / paintBow) is then laid in at its depth with rig_core's rules: a weapon behind the body goes under the whole
// figure (the figure's parts contour against it); a weapon in front goes over the figure (its own inner contour), and
// what rig_core draws after a weapon comes back over it from the figure's own pixels: the gripping hand (a hand is drawn
// over what it holds, STYLE_P2 5), the off hand where it grips the weapon too or holds the bow, and in side views the
// near arm. Then rig_core's ink outer outline (Ink below / right, Lit Ink above / left) and the trim with the pivot.
// The sword family is the style slice's three blows (rig_body.cpp sliceSwordAction), frame for frame.
//
// Each family's three blows are keyed per facing (right, down, up; the left facing takes the right keys mirrored: the
// weapon stays in the right hand, now the far hand, its arm in front of the torso whenever the weapon is, STYLE_P2 9),
// each blow ending near where the next begins so a combo flows. What each family reads as (STYLE_P2 12):
//   unarmed     a jab with the near fist, a cross with the far fist (shoulder turned), a rising hook; fists alternate
//   dagger      a stab from the hip with the arm locked straight and the body leaning in; a short slash across; two
//               quick stabs (high, then low) on a deep lunge
//   greatsword  the heavy overhead cleave (both hands high behind the head, leaning back; the blade down to the knees
//               on a wide crouched stance); a wide waist-high sweep from behind the right hip round to the far side; a
//               rising sweep into an overhead slam that strikes the ground
//   axe         a chop from behind the head biting down to knee height; a diagonal from the other side; a two-handed
//               overhead chop
//   mace        a smash from overhead, up on the toes, landing on the ground in front with the knees bent; a heavy
//               horizontal hook; a two-handed overhead smash
//   spear       a thrust (the shaft slides back, then forward and on, the front foot forward); a second thrust aimed
//               higher; a lunge with a long step
//   staff       a diagonal two-handed strike, the far end leading; a rising strike from low; an overhead two-handed smash
// rpg_test --rig reads the tip paths (R8), the family and variant distinctness (R7, R9) and the painted left frames and
// weapon hand (R10) from these atlases' pose data.
#include <algorithm>
#include <cmath>
#include <cstring>
#include "rpg/art/rig/rig_internal.h"

namespace art {
namespace rig2 {

namespace {

// ---------------------------------------------------------------- one frame on RIG-BODY's figure
struct ActPose {
  Pose p;               // the body; p.wDir / wBehind / wShort the weapon (paintRaw paints the body without it)
  int8_t edge = 1;      // the side the axe's bit faces (+1 the clockwise side of the weapon's direction)
  bool two = false;     // the off hand grips the weapon too (drawn over it)
  int8_t tipHand = 0;   // unarmed: the striking fist (1 right, 2 left) is the tip anchor
  int8_t bowDraw = -1;  // the bow: -1 none, 0 braced, 1 half, 2 full, 3 loosed
  int8_t bowAim = 0, bowHalf = 11, arrowLen = 12;
  bool bowBehind = false, arrowFirst = false;
  bool split = false;   // a pole's far end (beyond the rear hand) on the other side of the body from its head
};

// rig_core.cpp's two-bone solve (the arm's elbow), for the near arm's mask
void jointOf(float ax, float ay, float bx, float by, float l1, float l2, float side, float& jx, float& jy) {
  const float dx = bx - ax, dy = by - ay, d = std::max(1e-3f, std::sqrt(dx * dx + dy * dy));
  if (d >= l1 + l2 - 0.05f) { jx = ax + dx * l1 / (l1 + l2); jy = ay + dy * l1 / (l1 + l2); return; }
  const float a = (l1 * l1 - l2 * l2 + d * d) / (2 * d), h = std::sqrt(std::max(0.0f, l1 * l1 - a * a));
  const float ux = dx / d, uy = dy / d;
  jx = ax + ux * a - uy * h * side;
  jy = ay + uy * a + ux * h * side;
}

// the pixels of a side view's near arm (rig_core.cpp paintInto's shoulder, elbow and wrist), canvas-indexed
void nearArmMask(const Body& B, const Pose& P, int W, int H, int feet, std::vector<uint8_t>& mask) {
  const int fc = P.facing;
  const int s = fc == FLeft ? -1 : 1;
  const int cx = P.hipX, sx = cx + P.lean;
  const int shoulderRow = B.shoulder + P.dy + P.chest;
  const float shY = (float)shoulderRow + 0.5f;
  float shX, wx, wy;
  if (fc == FRight) { shX = (float)sx - 1.0f; wx = P.hRx + 1.0f; wy = P.hRy - 0.5f; }
  else { shX = (float)sx; wx = P.hLx + 1.0f; wy = P.hLy - 0.5f; }
  const float bend = (P.elbowOut ? -1.0f : 1.0f) * (float)s;
  float ex, ey;
  jointOf(shX, shY, wx, wy, (float)B.armU, (float)B.armF, bend, ex, ey);
  Fig m(W, H, W / 2, feet);
  m.begin();
  Ramp5 one;
  for (int i = 0; i < 5; i++) one.c[i] = 0xFFFFFFFFu;
  limb(m, ex, ey, wx, wy, 2, one);
  limb(m, shX, shY, ex, ey, 2, one);
  mask.assign((size_t)W * H, 0);
  for (size_t i = 0; i < mask.size(); i++) mask[i] = (m.px[i] >> 24) ? 1 : 0;
}

// rig_core.cpp's finishFig: the ink outer outline (a pixel on a bottom- or right-facing edge is Ink, a purely top- or
// left-facing one Lit Ink: Ink 30 % toward the shape), the foot of a 1 px gap Ink, the pinholes closed with Ink, and the
// trim to the painted box with the pivot kept (STYLE_P2 8)
// keepOpen (a drawn bow): only pinholes of a few px close; the ground seen between the stave and the string stays clear
Cut inkFinish(Fig& f, const RigAnchors& an, bool keepOpen = false) {
  f.mergeBounds();
  Cut out;
  if (f.gx1 < f.gx0) return out;
  const int bw = f.gx1 - f.gx0 + 1, bh = f.gy1 - f.gy0 + 1;
  const int w = bw + 2, h = bh + 2;
  std::vector<uint32_t> src((size_t)w * h, 0u);
  for (int y = 0; y < bh; y++) std::memcpy(&src[(size_t)(y + 1) * w + 1], &f.px[(size_t)(f.gy0 + y) * f.W + f.gx0], (size_t)bw * 4);
  out.c.w = w; out.c.h = h;
  out.c.px = src;
  uint32_t* o = out.c.px.data();
  const uint32_t* s = src.data();
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      const size_t i = (size_t)y * w + x;
      if (s[i] >> 24) continue;
      uint32_t n;
      if ((x > 0 && (s[i - 1] >> 24) > 96) || (y > 0 && (s[i - w] >> 24) > 96)) o[i] = kInk;
      else if (y + 1 < h && ((n = s[i + w]) >> 24) > 96) o[i] = mix(kInk, opaque(n), 0.30f);
      else if (x + 1 < w && ((n = s[i + 1]) >> 24) > 96) o[i] = mix(kInk, opaque(n), 0.30f);
    }
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      const size_t i = (size_t)y * w + x;
      if ((s[i] >> 24) || !(o[i] >> 24) || o[i] == kInk) continue;
      auto air = [&](int qx, int qy) { return qx < 0 || qy < 0 || qx >= w || qy >= h || !(o[(size_t)qy * w + qx] >> 24); };
      if (!air(x - 1, y) && !air(x, y - 1) && (air(x + 1, y) || air(x, y + 1))) o[i] = kInk;
    }
  std::vector<uint8_t> state((size_t)w * h, 0);
  std::vector<int> q;
  bool any = false;
  for (int y = 0; y < h; y++) {
    int l = -1, r = -1;
    for (int x = 0; x < w; x++) if (o[(size_t)y * w + x] >> 24) { if (l < 0) l = x; r = x; }
    for (int x = 0; x < w; x++) {
      const size_t i = (size_t)y * w + x;
      if (o[i] >> 24) continue;
      if (l < 0 || x < l || x > r || y == 0 || y == h - 1) state[i] = 1;
      else { state[i] = 2; any = true; }
    }
  }
  if (any) {
    for (int y = 1; y < h - 1; y++)
      for (int x = 1; x < w - 1; x++) {
        const size_t i = (size_t)y * w + x;
        if (state[i] == 2 && (state[i - 1] == 1 || state[i + 1] == 1 || state[i - w] == 1 || state[i + w] == 1)) { state[i] = 3; q.push_back((int)i); }
      }
    while (!q.empty()) {
      const int i = q.back();
      q.pop_back();
      for (int j : {i - 1, i + 1, i - w, i + w})
        if (j >= 0 && j < w * h && state[(size_t)j] == 2) { state[(size_t)j] = 3; q.push_back(j); }
    }
    if (keepOpen) {   // a region bigger than a pinhole stays open (state 4), the rest closes as below
      for (int start = 0; start < w * h; start++) {
        if (state[(size_t)start] != 2) continue;
        std::vector<int> comp{start};
        state[(size_t)start] = 5;
        for (size_t k = 0; k < comp.size(); k++)
          for (int j : {comp[k] - 1, comp[k] + 1, comp[k] - w, comp[k] + w})
            if (j >= 0 && j < w * h && state[(size_t)j] == 2) { state[(size_t)j] = 5; comp.push_back(j); }
        for (int j : comp) state[(size_t)j] = comp.size() > 4 ? 4 : 2;
      }
    }
    for (size_t i = 0; i < state.size(); i++) if (state[i] == 2) o[i] = kInk;
  }
  out.px = f.ox - (f.gx0 - 1);
  out.py = f.oy - (f.gy0 - 1);
  out.an = an;
  return out;
}

// the frame composed on f (an ACT_W x ACT_H Fig, the pivot at (ACT_W / 2, ACT_FEET)) before its outline, with its pose data
void composeAct(const HumanLook& look, const Body& B, const Kit& K, const ActPose& A, Fig& f, RigAnchors& an) {
  const int W = ACT_W, H = ACT_H, feet = ACT_FEET;
  Pose body = A.p;
  body.wDir = -1;
  const Canvas raw = paintRaw(look, B, K, body, W, H, feet, an);
  const Pose& P = A.p;
  const bool weapon = P.wDir >= 0 && armsDrawn(K.weapon);
  const bool bow = A.bowDraw >= 0;
  auto putRaw = [&](const std::vector<uint8_t>* only) {   // the figure's pixels (all, or only those in a mask)
    for (int cy = 0; cy < H; cy++)
      for (int cx = 0; cx < W; cx++) {
        const size_t i = (size_t)cy * W + cx;
        if (!(raw.px[i] >> 24)) continue;
        if (only && !(*only)[i]) continue;
        f.set(cx - f.ox, cy - f.oy, raw.px[i]);
      }
  };
  if (!weapon && !bow) {
    f.begin();
    putRaw(nullptr);
  } else {
    // the weapon on its own canvas first (its pixels and its tip)
    Fig wf(W, H, W / 2, feet);
    wf.begin();
    int tipX = 0, tipY = 0;
    const float gx = P.hRx + 0.5f, gy = P.hRy + 0.5f;   // paintInto's grip: the fist's centre - 0.5
    bool behind = P.wBehind != 0;
    if (weapon) paintArms(wf, K, K.weapon, gx, gy, P.wDir, P.wShort, A.edge, tipX, tipY);
    else {
      paintBow(wf, K, P.hLx, P.hLy, A.bowAim, A.bowHalf, P.hRx, P.hRy, A.bowDraw, A.arrowLen, A.arrowFirst, tipX, tipY);
      behind = A.bowBehind;
    }
    // which weapon pixels lie under the figure and which over it: the weapon's own depth, except (split) the part of a
    // pole beyond the rear hand, which lies on the far side of the body from its head (a spear pointing at the viewer:
    // its butt behind the head; pointing away: its butt in front of the back)
    std::vector<uint8_t> over((size_t)W * H, 0), under((size_t)W * H, 0);
    {
      static const int kD[16][2] = {{2, 0}, {2, 1}, {2, 2}, {1, 2}, {0, 2}, {-1, 2}, {-2, 2}, {-2, 1},
                                    {-2, 0}, {-2, -1}, {-2, -2}, {-1, -2}, {0, -2}, {1, -2}, {2, -2}, {2, -1}};
      const int d = P.wDir & 15;
      const float ul = std::sqrt((float)(kD[d][0] * kD[d][0] + kD[d][1] * kD[d][1]));
      const float ux = kD[d][0] / ul, uy = kD[d][1] / ul;
      const float rear = -(float)(armsTwoHandGap(K.weapon) + 2);
      for (size_t i = 0; i < wf.px.size(); i++) {
        if (!(wf.px[i] >> 24)) continue;
        const float dx = (float)((int)(i % W) - f.ox - P.hRx), dy = (float)((int)(i / W) - f.oy - P.hRy);
        const bool far = A.split && weapon && dx * ux + dy * uy < rear;
        (behind != far ? under : over)[i] = 1;
      }
    }
    auto putWeapon = [&](const std::vector<uint8_t>& m) {
      for (size_t i = 0; i < wf.px.size(); i++)
        if (m[i]) f.set((int)(i % W) - f.ox, (int)(i / W) - f.oy, wf.px[i]);
    };
    // under the figure: the figure's parts contour against it (paintInto draws them after a weapon behind the body)
    f.begin();
    putWeapon(under);
    f.begin();
    putRaw(nullptr);
    const uint8_t bodyLayer = f.cur;
    f.contour(0.75f, 0.16f);
    // over the figure: the weapon's own inner contour, then what paintInto draws after a weapon comes back over it from
    // the figure's own pixels: the gripping hand, the off hand on the grip (or the bow), a side view's near arm
    f.begin();
    putWeapon(over);
    f.contour(0.6f, 0.2f);
    std::vector<uint8_t> back((size_t)W * H, 0);
    auto hand = [&](int hx, int hy) {
      for (int dy = 0; dy < 2; dy++)
        for (int dx = 0; dx < 2; dx++) {
          const int cx = hx + dx + f.ox, cy = hy + dy + f.oy;
          if (cx >= 0 && cy >= 0 && cx < W && cy < H) back[(size_t)cy * W + cx] = 1;
        }
    };
    hand(P.hRx, P.hRy);
    if (A.two || bow) hand(P.hLx, P.hLy);
    if (P.facing == FRight || P.facing == FLeft) {
      std::vector<uint8_t> arm;
      nearArmMask(B, P, W, H, feet, arm);
      for (size_t i = 0; i < back.size(); i++) back[i] |= arm[i];
      if (P.facing == FLeft) hand(P.hLx, P.hLy);   // facing left the near hand (the off hand) is drawn last
    }
    for (size_t i = 0; i < back.size(); i++) if (!over[i]) back[i] = 0;   // only where the weapon covered the figure
    f.begin();
    putRaw(&back);
    f.contour(0.7f, 0.25f, bodyLayer);
    if (weapon) {
      an.gripX = (int16_t)P.hRx; an.gripY = (int16_t)P.hRy;
      an.weaponBehind = behind;
    }
    an.tipX = (int16_t)tipX; an.tipY = (int16_t)tipY;
  }
  if (!weapon && !bow && A.tipHand) {   // unarmed: the striking fist's centre
    an.tipX = (int16_t)(A.tipHand == 1 ? P.hRx + 1 : P.hLx + 1);
    an.tipY = (int16_t)(A.tipHand == 1 ? P.hRy + 1 : P.hLy + 1);
  }
}

Cut paintAct(const HumanLook& look, const Body& B, const Kit& K, const ActPose& A) {
  Fig f(ACT_W, ACT_H, ACT_W / 2, ACT_FEET);
  RigAnchors an;
  composeAct(look, B, K, A, f, an);
  return inkFinish(f, an, A.bowDraw >= 0 && A.bowDraw <= 2);
}

// a composed frame turned a quarter (the exact transpose about the pivot: the top-left light maps onto itself, as
// rig_death.cpp lays its bodies down), then stood on the sole row, its middle `ahead` px along the roll; the ink outline
// goes round it after the turn
Cut paintTurned(const HumanLook& look, const Body& B, const Kit& K, const ActPose& A, int ahead) {
  Fig f(ACT_W, ACT_H, ACT_W / 2, ACT_FEET);
  RigAnchors an;
  composeAct(look, B, K, A, f, an);
  int x0 = 1 << 20, x1 = -(1 << 20), y0 = 1 << 20, y1 = -(1 << 20);
  for (int cy = 0; cy < f.H; cy++)
    for (int cx = 0; cx < f.W; cx++)
      if (f.px[(size_t)cy * f.W + cx] >> 24) {
        const int x = cx - f.ox, y = cy - f.oy;   // pivot-relative; turned: (y, x)
        x0 = std::min(x0, y); x1 = std::max(x1, y); y0 = std::min(y0, x); y1 = std::max(y1, x);
      }
  Fig t(ACT_W, ACT_H, ACT_W / 2, ACT_FEET);
  t.begin();
  const int sx = ahead - (x0 + x1) / 2, sy = -1 - y1;   // centred on the pivot (plus the roll's travel), the lowest row on F - 1
  for (int cy = 0; cy < f.H; cy++)
    for (int cx = 0; cx < f.W; cx++) {
      const uint32_t c = f.px[(size_t)cy * f.W + cx];
      if (c >> 24) t.set((cy - f.oy) + sx, (cx - f.ox) + sy, c);
    }
  RigAnchors ta;   // a ball has no hands or face to measure; the head's box marks its top for bubbles and plates
  ta.headX = (int16_t)(x0 + sx); ta.headY = (int16_t)(y0 + sy); ta.headW = 12; ta.headH = 12;
  return inkFinish(t, ta);
}

// ---------------------------------------------------------------- the keys
// One frame of a blow, for the man (a woman's upper body sits a row lower: the slice's rule, wy = hip + 11). Side keys
// are for facing right (x forward); front keys for facing down (the weapon hand on the screen's left; fr / fl the
// screen-left / screen-right foot), back keys for facing up (the weapon hand on the screen's right; fr the screen-right
// foot, fl the screen-left one).
struct Key {
  int8_t hx, hy, dir, bh;      // the weapon hand (fist top-left), the weapon's direction (0..15 clockwise from pointing
                               //   right; -1 none), the weapon drawn behind the body
  int8_t ox, oy;               // the off hand (unless it grips the weapon too: then it sits on the grip)
  int8_t lean, dy;             // the torso's lean (side: + toward the facing; front views: + screen right), crouch (+ down)
  int8_t fr, fl;               // the feet (see above)
  int8_t ex, sh, el;           // ExEffort on the blow, the weapon foreshortened, raised elbows bend out
  int8_t ff;                   // side keys mirrored for the left facing: the far weapon arm in front of the torso
  int8_t lift;                 // side: the far (rear) foot's heel up 1 px (a push off)
  int8_t of;                   // side, facing right: the off (far) arm in front of the torso (a guard, a cross)
};
constexpr int8_t E = ExEffort;

// a blow: 6 keys per facing (right, down, up) and how it is held
struct Blow {
  Key right[6], down[6], up[6];
  bool two;       // the off hand on the weapon too (a family's two-handed grip: armsTwoHandGap)
  int8_t edge;    // the axe's bit side facing right (mirrored facing left)
  int8_t tip;     // unarmed: the striking fist (1 the weapon hand, 2 the off hand)
  bool splitDown, splitUp;   // facing down / up: a pole's far end lies on the other side of the body (split)
};

// The face stays clear: at these proportions an arm cannot lift a fist over the crown, so an overhead weapon is held
// beside the head (front and back views: past the head's side; side views: behind it), never over the eyes; and front
// and back views draw every weapon at its length (the 16-bit way), a blow toward or away from the viewer shown by where
// the weapon points (down past the feet, up past the head), never foreshortened away.

// ---- UNARMED: a jab (near fist), a cross (far fist, shoulders turned), a rising hook; Punch Star
const Blow kUnarmed[3] = {
    {   // hit 1 jab: from the guard at the chest, the near fist snaps straight out at shoulder height
        {{1, -17, -1, 0, 3, -18, 0, 1, 2, -3, 0, 0, 0, 1, 0, 1}, {0, -17, -1, 0, 3, -18, -1, 1, 2, -3, 0, 0, 0, 1, 0, 1},
         {6, -19, -1, 0, 2, -18, 2, 1, 3, -3, E, 0, 0, 1, 0, 1}, {9, -19, -1, 0, 2, -18, 3, 1, 4, -4, E, 0, 0, 1, 1, 1},
         {6, -19, -1, 0, 2, -18, 2, 1, 3, -3, 0, 0, 0, 1, 0, 1}, {2, -18, -1, 0, 3, -18, 0, 1, 2, -3, 0, 0, 0, 1, 0, 1}},
        {{-5, -17, -1, 0, 3, -18, 0, 1, -5, 4, 0, 0, 0, 0, 0, 0}, {-6, -17, -1, 0, 3, -18, -1, 1, -5, 4, 0, 0, 0, 0, 0, 0},
         {-6, -13, -1, 0, 3, -18, -1, 1, -5, 4, E, 0, 0, 0, 0, 0}, {-7, -10, -1, 0, 4, -18, -1, 1, -5, 4, E, 0, 0, 0, 0, 0},
         {-4, -13, -1, 0, 3, -18, 0, 1, -5, 4, 0, 0, 0, 0, 0, 0}, {-5, -16, -1, 0, 3, -18, 0, 1, -5, 4, 0, 0, 0, 0, 0, 0}},
        {{4, -17, -1, 0, -6, -18, 0, 1, 4, -5, 0, 0, 0, 0, 0, 0}, {5, -16, -1, 0, -6, -18, 1, 1, 4, -5, 0, 0, 0, 0, 0, 0},
         {6, -21, -1, 0, -6, -18, 0, 1, 4, -5, E, 0, 0, 0, 0, 0}, {7, -25, -1, 0, -6, -18, -1, 1, 4, -5, E, 0, 0, 0, 0, 0},
         {6, -21, -1, 0, -6, -18, 0, 1, 4, -5, 0, 0, 0, 0, 0, 0}, {4, -17, -1, 0, -6, -18, 0, 1, 4, -5, 0, 0, 0, 0, 0, 0}},
        false, 1, 1, false, false},
    {   // hit 2 cross: the shoulders turn back, the far fist drives straight through, standing tall, the near one back
        {{2, -17, -1, 0, 3, -18, 0, 1, 2, -3, 0, 0, 0, 1, 0, 1}, {2, -18, -1, 0, 0, -18, -1, 1, 2, -3, 0, 0, 0, 1, 0, 1},
         {1, -18, -1, 0, 6, -19, 2, 0, 4, -4, E, 0, 0, 1, 1, 1}, {0, -18, -1, 0, 9, -19, 3, 0, 4, -4, E, 0, 0, 1, 1, 1},
         {1, -18, -1, 0, 6, -19, 2, 0, 3, -3, 0, 0, 0, 1, 0, 1}, {2, -17, -1, 0, 3, -18, 0, 1, 2, -3, 0, 0, 0, 1, 0, 1}},
        {{-5, -17, -1, 0, 3, -18, 0, 1, -5, 4, 0, 0, 0, 0, 0, 0}, {-5, -18, -1, 0, 4, -17, 1, 1, -5, 4, 0, 0, 0, 0, 0, 0},
         {-5, -18, -1, 0, 4, -13, 1, 0, -4, 4, E, 0, 0, 0, 0, 0}, {-6, -18, -1, 0, 5, -10, 1, 0, -4, 4, E, 0, 0, 0, 0, 0},
         {-5, -17, -1, 0, 2, -13, -1, 0, -5, 4, 0, 0, 0, 0, 0, 0}, {-5, -17, -1, 0, 3, -18, 0, 1, -5, 4, 0, 0, 0, 0, 0, 0}},
        {{4, -17, -1, 0, -6, -18, 0, 1, 4, -5, 0, 0, 0, 0, 0, 0}, {4, -18, -1, 0, -7, -16, -1, 1, 4, -5, 0, 0, 0, 0, 0, 0},
         {4, -18, -1, 0, -8, -21, 1, 0, 4, -4, E, 0, 0, 0, 0, 0}, {3, -18, -1, 0, -9, -25, 1, 0, 4, -4, E, 0, 0, 0, 0, 0},
         {4, -17, -1, 0, -8, -21, 1, 0, 4, -5, 0, 0, 0, 0, 0, 0}, {4, -17, -1, 0, -6, -18, 0, 1, 4, -5, 0, 0, 0, 0, 0, 0}},
        false, 1, 2, false, false},
    {   // hit 3 rising hook: dropped low, the near fist comes up from the hip to head height, up on the toes
        {{2, -14, -1, 0, 4, -16, 0, 2, 2, -3, 0, 0, 0, 1, 0, 1}, {1, -12, -1, 0, 4, -16, 1, 3, 3, -3, 0, 0, 0, 1, 0, 1},
         {6, -17, -1, 0, 2, -19, 2, 0, 3, -3, E, 0, 0, 1, 1, 1}, {7, -25, -1, 0, 2, -18, 1, -1, 3, -3, E, 0, 1, 1, 1, 1},
         {7, -23, -1, 0, 2, -18, 1, -1, 3, -3, 0, 0, 1, 1, 0, 1}, {3, -19, -1, 0, 3, -18, 0, 0, 2, -3, 0, 0, 0, 1, 0, 1}},
        {{-7, -12, -1, 0, 3, -16, 0, 2, -5, 4, 0, 0, 0, 0, 0, 0}, {-8, -11, -1, 0, 3, -16, -1, 3, -5, 4, 0, 0, 0, 0, 0, 0},
         {-8, -18, -1, 0, 3, -18, 0, 0, -5, 4, E, 0, 0, 0, 0, 0}, {-8, -25, -1, 0, 3, -17, 1, -1, -5, 4, E, 0, 1, 0, 0, 0},
         {-8, -23, -1, 0, 3, -17, 1, -1, -5, 4, 0, 0, 1, 0, 0, 0}, {-5, -18, -1, 0, 3, -18, 0, 0, -5, 4, 0, 0, 0, 0, 0, 0}},
        {{7, -12, -1, 0, -6, -18, 0, 2, 4, -5, 0, 0, 0, 0, 0, 0}, {8, -11, -1, 0, -6, -16, 1, 3, 4, -5, 0, 0, 0, 0, 0, 0},
         {7, -19, -1, 0, -6, -18, 0, 0, 4, -5, E, 0, 0, 0, 0, 0}, {7, -27, -1, 0, -6, -17, -1, -1, 4, -5, E, 0, 1, 0, 0, 0},
         {7, -25, -1, 0, -6, -17, -1, -1, 4, -5, 0, 0, 1, 0, 0, 0}, {5, -19, -1, 0, -6, -18, 0, 0, 4, -5, 0, 0, 0, 0, 0, 0}},
        false, 1, 1, false, false},
};

// ---- DAGGER: a stab from the hip, a short slash across, two quick stabs on a deep lunge; Glint at the tip
const Blow kDagger[3] = {
    {   // hit 1 stab: the hand at the hip, then the arm locked straight, the body leaning in on a wide crouched stance
        {{-3, -13, 0, 0, 3, -16, -1, 0, 2, -3, 0, 0, 0, 1, 0, 1}, {-4, -13, 0, 0, 4, -17, -1, 1, 2, -4, 0, 0, 0, 1, 0, 1},
         {4, -15, 0, 0, 0, -14, 2, 1, 5, -4, E, 0, 0, 1, 1, 0}, {8, -15, 0, 0, -1, -13, 3, 1, 6, -5, E, 0, 0, 1, 1, 0},
         {6, -15, 0, 0, 0, -13, 2, 1, 5, -4, 0, 0, 0, 1, 0, 0}, {2, -14, 0, 0, 2, -15, 1, 0, 3, -3, 0, 0, 0, 1, 0, 1}},
        {{-7, -12, 4, 0, 5, -15, 0, 0, -5, 4, 0, 2, 0, 0, 0, 0}, {-8, -14, 4, 0, 6, -15, -1, 1, -5, 4, 0, 3, 0, 0, 0, 0},
         {-8, -12, 4, 0, 5, -14, -1, 2, -7, 5, E, 1, 0, 0, 0, 0}, {-10, -9, 4, 0, 6, -13, -1, 2, -7, 5, E, 0, 0, 0, 0, 0},
         {-9, -10, 4, 0, 5, -14, -1, 2, -7, 5, 0, 1, 0, 0, 0, 0}, {-7, -12, 4, 0, 5, -15, 0, 0, -5, 4, 0, 2, 0, 0, 0, 0}},
        {{6, -12, 12, 0, -7, -15, 0, 0, 4, -5, 0, 2, 0, 0, 0, 0}, {7, -11, 12, 0, -7, -15, 1, 1, 4, -5, 0, 2, 0, 0, 0, 0},
         {6, -18, 12, 1, -7, -14, 0, 2, 6, -7, E, 2, 0, 0, 0, 0}, {5, -22, 12, 1, -7, -13, -1, 2, 6, -7, E, 3, 0, 0, 0, 0},
         {6, -19, 12, 1, -7, -14, 0, 2, 6, -7, 0, 2, 0, 0, 0, 0}, {6, -14, 12, 0, -7, -15, 0, 0, 4, -5, 0, 2, 0, 0, 0, 0}},
        false, 1, 0, false, false},
    {   // hit 2 slash: cocked high by the far shoulder -> across the front -> down by the near hip
        {{-3, -21, 10, 1, 3, -15, 0, 0, 3, -3, 0, 0, 1, 0, 0, 1}, {-4, -22, 9, 1, 3, -15, -1, 0, 3, -3, 0, 0, 1, 0, 0, 1},
         {6, -19, 15, 0, 2, -15, 1, 1, 4, -3, E, 0, 0, 1, 0, 0}, {6, -15, 2, 0, 1, -15, 2, 1, 4, -4, E, 0, 0, 1, 0, 0},
         {3, -12, 4, 0, 2, -15, 1, 1, 4, -3, 0, 1, 0, 1, 0, 0}, {0, -12, 3, 0, 2, -15, 0, 0, 3, -3, 0, 0, 0, 1, 0, 0}},
        {{6, -18, 14, 1, 6, -13, 1, 0, -5, 4, 0, 0, 1, 0, 0, 0}, {7, -19, 14, 1, 6, -13, 2, 0, -5, 4, 0, 1, 1, 0, 0, 0},
         {0, -16, 7, 0, 6, -14, 1, 1, -5, 4, E, 0, 0, 0, 0, 0}, {-7, -13, 6, 0, 6, -14, -1, 1, -5, 4, E, 0, 0, 0, 0, 0},
         {-9, -12, 5, 0, 6, -13, -1, 1, -5, 4, 0, 0, 0, 0, 0, 0}, {-8, -13, 4, 0, 6, -13, 0, 0, -5, 4, 0, 1, 0, 0, 0, 0}},
        {{-8, -21, 10, 1, -7, -13, -1, 0, 4, -5, 0, 0, 1, 0, 0, 0}, {-9, -22, 10, 1, -7, -13, -2, 0, 4, -5, 0, 1, 1, 0, 0, 0},
         {0, -19, 15, 1, -7, -14, 0, 1, 4, -5, E, 1, 0, 0, 0, 0}, {7, -15, 1, 0, -7, -14, 1, 1, 4, -5, E, 0, 0, 0, 0, 0},
         {8, -12, 2, 0, -7, -13, 1, 1, 4, -5, 0, 0, 0, 0, 0, 0}, {7, -12, 3, 0, -7, -13, 0, 0, 4, -5, 0, 0, 0, 0, 0, 0}},
        false, 1, 0, false, false},
    {   // hit 3 two stabs: coiled low, then high and low again on a deep lunge
        {{-2, -16, 0, 0, 3, -17, -1, 1, 3, -3, 0, 0, 0, 1, 0, 1}, {-4, -15, 0, 0, 3, -17, -1, 2, 3, -4, 0, 0, 0, 1, 0, 1},
         {9, -20, 15, 0, 0, -15, 3, 1, 6, -5, E, 0, 0, 1, 1, 0}, {9, -12, 1, 0, 0, -13, 3, 3, 7, -6, E, 0, 0, 1, 1, 0},
         {6, -12, 1, 0, 1, -13, 2, 3, 7, -6, 0, 0, 0, 1, 0, 0}, {3, -14, 0, 0, 2, -15, 1, 1, 4, -4, 0, 0, 0, 1, 0, 0}},
        {{-7, -15, 4, 0, 5, -16, 0, 1, -5, 4, 0, 3, 0, 0, 0, 0}, {-8, -16, 4, 0, 5, -16, -1, 2, -5, 4, 0, 4, 0, 0, 0, 0},
         {-7, -17, 4, 0, 4, -15, 0, 1, -6, 5, E, 2, 0, 0, 0, 0}, {-10, -8, 5, 0, 4, -13, -1, 3, -7, 6, E, 0, 0, 0, 0, 0},
         {-9, -9, 4, 0, 4, -13, -1, 3, -7, 6, 0, 1, 0, 0, 0, 0}, {-6, -12, 4, 0, 5, -14, 0, 1, -5, 4, 0, 2, 0, 0, 0, 0}},
        {{6, -14, 12, 0, -6, -16, 0, 1, 4, -5, 0, 2, 0, 0, 0, 0}, {7, -13, 12, 0, -6, -16, 1, 2, 4, -5, 0, 2, 0, 0, 0, 0},
         {7, -23, 12, 1, -6, -15, 0, 1, 5, -6, E, 3, 0, 0, 0, 0}, {5, -17, 12, 1, -6, -13, -1, 3, 6, -7, E, 4, 0, 0, 0, 0},
         {6, -16, 12, 1, -6, -13, 0, 3, 6, -7, 0, 3, 0, 0, 0, 0}, {6, -14, 12, 0, -6, -15, 0, 1, 4, -5, 0, 2, 0, 0, 0, 0}},
        false, 1, 0, false, false},
};

// ---- GREATSWORD (two hands, 3 px apart): the overhead cleave, the waist-high sweep, the rising sweep into a slam; Heavy Arc
const Blow kGreatsword[3] = {
    {   // hit 1 cleave: both hands up behind the head, leaning back -> over the top -> down to the knees, wide and low
        {{-7, -26, 7, 1, 0, 0, -1, 0, 4, -4, 0, 0, 1, 0, 0, 0}, {-8, -27, 8, 1, 0, 0, -1, 0, 4, -5, 0, 0, 1, 0, 1, 0},
         {-4, -29, 14, 1, 0, 0, 0, -1, 5, -5, E, 0, 1, 0, 1, 0}, {6, -15, 1, 0, 0, 0, 2, 1, 6, -5, E, 0, 0, 1, 0, 0},
         {5, -12, 1, 0, 0, 0, 2, 1, 6, -5, 0, 0, 0, 1, 0, 0}, {3, -15, 0, 0, 0, 0, 1, 0, 5, -4, 0, 0, 0, 1, 0, 0}},
        {{-9, -21, 12, 1, 0, 0, -1, 0, -6, 5, 0, 0, 1, 0, 0, 0}, {-10, -23, 11, 1, 0, 0, -1, -1, -6, 5, 0, 0, 1, 0, 0, 0},
         {-5, -17, 2, 0, 0, 0, 0, 0, -6, 5, E, 0, 0, 0, 0, 0}, {-3, -13, 4, 0, 0, 0, 0, 1, -7, 6, E, 0, 0, 0, 0, 0},
         {-3, -11, 4, 0, 0, 0, 0, 1, -7, 6, 0, 0, 0, 0, 0, 0}, {-5, -13, 5, 0, 0, 0, 0, 0, -6, 5, 0, 0, 0, 0, 0, 0}},
        {{7, -21, 4, 0, 0, 0, 0, 0, 5, -6, 0, 0, 1, 0, 0, 0}, {8, -23, 4, 0, 0, 0, 0, -1, 5, -6, 0, 0, 1, 0, 0, 0},
         {7, -24, 13, 1, 0, 0, 0, 0, 5, -6, E, 0, 1, 0, 0, 0}, {6, -17, 12, 1, 0, 0, 0, 1, 6, -7, E, 0, 0, 0, 0, 0},
         {6, -15, 12, 1, 0, 0, 0, 1, 6, -7, 0, 1, 0, 0, 0, 0}, {6, -16, 13, 1, 0, 0, 0, 0, 5, -6, 0, 0, 0, 0, 0, 0}},
        true, 1, 0, false, false},
    {   // hit 2 sweep: loaded low behind the right hip -> round the front at the waist -> on round the far side
        {{-5, -13, 7, 0, 0, 0, -1, 1, 4, -4, 0, 0, 0, 0, 0, 0}, {-6, -12, 8, 0, 0, 0, -2, 1, 4, -5, 0, 0, 0, 0, 0, 0},
         {2, -13, 2, 0, 0, 0, 0, 1, 5, -4, E, 5, 0, 1, 0, 0}, {7, -14, 0, 0, 0, 0, 2, 1, 6, -5, E, 0, 0, 1, 1, 0},
         {6, -17, 14, 1, 0, 0, 2, 0, 6, -5, 0, 5, 0, 1, 0, 0}, {-3, -21, 11, 1, 0, 0, 1, 0, 5, -4, 0, 2, 0, 0, 0, 0}},
        {{-9, -13, 9, 1, 0, 0, -1, 1, -6, 5, 0, 0, 0, 0, 0, 0}, {-10, -12, 8, 1, 0, 0, -1, 1, -6, 5, 0, 0, 0, 0, 0, 0},
         {-7, -12, 6, 0, 0, 0, 0, 1, -6, 5, E, 0, 0, 0, 0, 0}, {-2, -13, 0, 0, 0, 0, 1, 1, -6, 5, E, 0, 0, 0, 0, 0},
         {4, -14, 14, 1, 0, 0, 1, 0, -6, 5, 0, 0, 0, 0, 0, 0}, {5, -16, 13, 1, 0, 0, 1, 0, -6, 5, 0, 0, 0, 0, 0, 0}},
        {{8, -13, 3, 0, 0, 0, 1, 1, 5, -6, 0, 0, 0, 0, 0, 0}, {9, -12, 2, 0, 0, 0, 1, 1, 5, -6, 0, 0, 0, 0, 0, 0},
         {6, -15, 14, 1, 0, 0, 0, 1, 5, -6, E, 0, 0, 0, 0, 0}, {1, -17, 8, 1, 0, 0, -1, 1, 5, -6, E, 0, 0, 0, 0, 0},
         {-6, -15, 6, 0, 0, 0, -1, 0, 5, -6, 0, 0, 0, 0, 0, 0}, {-7, -14, 5, 0, 0, 0, -1, 0, 5, -6, 0, 0, 0, 0, 0, 0}},
        true, 1, 0, false, false},
    {   // hit 3 rising slam: low in front -> swept up behind, up on the toes -> the apex overhead -> slammed into the ground
        {{2, -12, 2, 0, 0, 0, 0, 1, 4, -4, 0, 3, 0, 1, 0, 0}, {-6, -26, 10, 1, 0, 0, -1, -1, 4, -4, 0, 0, 1, 0, 1, 0},
         {-3, -30, 12, 1, 0, 0, 0, -1, 5, -4, E, 0, 1, 0, 1, 0}, {7, -10, 1, 0, 0, 0, 3, 3, 7, -6, E, 0, 0, 1, 0, 0},
         {7, -10, 1, 0, 0, 0, 3, 3, 7, -6, 0, 0, 0, 1, 0, 0}, {5, -13, 1, 0, 0, 0, 2, 2, 6, -5, 0, 0, 0, 1, 0, 0}},
        {{-7, -12, 5, 0, 0, 0, 0, 1, -6, 5, 0, 0, 0, 0, 0, 0}, {-9, -21, 11, 1, 0, 0, 0, -1, -6, 5, 0, 0, 1, 0, 0, 0},
         {-9, -24, 12, 1, 0, 0, 0, -1, -6, 5, E, 0, 1, 0, 0, 0}, {-3, -14, 4, 0, 0, 0, 0, 3, -7, 6, E, 0, 0, 0, 0, 0},
         {-3, -14, 4, 0, 0, 0, 0, 3, -7, 6, 0, 0, 0, 0, 0, 0}, {-4, -14, 4, 0, 0, 0, 0, 2, -6, 5, 0, 0, 0, 0, 0, 0}},
        {{7, -12, 3, 0, 0, 0, 0, 1, 5, -6, 0, 0, 0, 0, 0, 0}, {8, -21, 13, 1, 0, 0, 0, -1, 5, -6, 0, 0, 1, 0, 0, 0},
         {7, -25, 12, 1, 0, 0, 1, -1, 5, -6, E, 0, 1, 0, 0, 0}, {6, -16, 12, 1, 0, 0, 0, 3, 6, -7, E, 4, 0, 0, 0, 0},
         {6, -16, 12, 1, 0, 0, 0, 3, 6, -7, 0, 4, 0, 0, 0, 0}, {6, -16, 13, 1, 0, 0, 0, 2, 5, -6, 0, 2, 0, 0, 0, 0}},
        true, 1, 0, false, false},
};

// ---- AXE (haft 13, bearded head): a chop, a diagonal from the other side, a two-handed overhead chop; Chop Arc
const Blow kAxe[3] = {
    {   // hit 1 chop: raised behind the head, the bit forward -> over the top -> bites down to the knee -> follows low
        {{-6, -25, 10, 1, 3, -15, -1, 0, 3, -3, 0, 0, 1, 0, 0, 0}, {-7, -26, 9, 1, 3, -16, -2, -1, 3, -4, 0, 0, 1, 0, 1, 0},
         {6, -23, 13, 0, 2, -15, 0, 0, 4, -4, E, 0, 0, 1, 0, 0}, {6, -15, 2, 0, 1, -13, 2, 2, 5, -4, E, 0, 0, 1, 0, 0},
         {5, -12, 3, 0, 1, -12, 2, 2, 5, -4, 0, 0, 0, 1, 0, 0}, {3, -13, 2, 0, 2, -13, 1, 1, 4, -3, 0, 0, 0, 1, 0, 0}},
        {{-9, -23, 12, 1, 6, -14, -1, 0, -5, 4, 0, 0, 1, 0, 0, 0}, {-10, -24, 11, 1, 6, -14, -1, -1, -5, 4, 0, 0, 1, 0, 0, 0},
         {-6, -17, 3, 0, 6, -14, 1, 0, -5, 4, E, 0, 0, 0, 0, 0}, {-4, -13, 4, 0, 5, -12, 1, 1, -5, 4, E, 0, 0, 0, 0, 0},
         {-4, -11, 4, 0, 5, -11, 1, 1, -5, 4, 0, 0, 0, 0, 0, 0}, {-5, -13, 5, 0, 6, -12, 0, 1, -5, 4, 0, 0, 0, 0, 0, 0}},
        {{7, -21, 4, 0, -6, -14, 0, 0, 4, -5, 0, 0, 1, 0, 0, 0}, {8, -23, 4, 0, -6, -14, 0, -1, 4, -5, 0, 0, 1, 0, 0, 0},
         {7, -24, 12, 1, -6, -14, 1, 0, 4, -5, E, 0, 1, 0, 0, 0}, {6, -17, 12, 1, -6, -13, 1, 1, 4, -5, E, 0, 0, 0, 0, 0},
         {6, -15, 12, 1, -6, -12, 1, 1, 4, -5, 0, 0, 0, 0, 0, 0}, {6, -15, 13, 1, -6, -12, 0, 1, 4, -5, 0, 0, 0, 0, 0, 0}},
        false, 1, 0, false, false},
    {   // hit 2 diagonal: cocked straight up behind the head -> down across the front -> out low on the near side
        {{-5, -27, 12, 1, 3, -16, 0, 0, 4, -3, 0, 0, 1, 0, 0, 0}, {-6, -28, 11, 1, 3, -17, -1, 0, 4, -4, 0, 0, 1, 0, 0, 0},
         {7, -21, 0, 0, 2, -15, 1, 1, 5, -3, E, 0, 0, 1, 0, 0}, {5, -14, 3, 0, 1, -14, 2, 2, 5, -4, E, 1, 0, 1, 0, 0},
         {1, -11, 5, 0, 1, -13, 1, 2, 5, -4, 0, 2, 0, 1, 0, 0}, {-1, -12, 6, 0, 2, -13, 0, 1, 4, -3, 0, 2, 0, 1, 0, 0}},
        {{6, -20, 12, 1, 7, -13, 1, 0, -5, 4, 0, 0, 1, 0, 0, 0}, {7, -21, 13, 1, 7, -13, 2, 0, -5, 4, 0, 0, 1, 0, 0, 0},
         {1, -17, 6, 0, 7, -14, 1, 1, -5, 4, E, 0, 0, 0, 0, 0}, {-6, -14, 5, 0, 6, -14, -1, 2, -6, 4, E, 0, 0, 0, 0, 0},
         {-8, -12, 6, 0, 6, -13, -2, 2, -6, 4, 0, 0, 0, 0, 0, 0}, {-8, -13, 5, 0, 6, -12, -1, 1, -5, 4, 0, 0, 0, 0, 0, 0}},
        {{-8, -21, 11, 1, -7, -13, -1, 0, 4, -5, 0, 0, 1, 0, 0, 0}, {-9, -22, 10, 1, -7, -13, -2, 0, 4, -5, 0, 0, 1, 0, 0, 0},
         {-1, -19, 14, 1, -7, -14, 0, 1, 4, -5, E, 0, 0, 0, 0, 0}, {6, -15, 2, 0, -7, -13, 1, 2, 4, -5, E, 0, 0, 0, 0, 0},
         {8, -13, 3, 0, -7, -12, 2, 2, 4, -5, 0, 0, 0, 0, 0, 0}, {8, -13, 2, 0, -7, -12, 1, 1, 4, -5, 0, 0, 0, 0, 0, 0}},
        false, 1, 0, false, false},
    {   // hit 3 two-handed chop: both hands up at the shoulder, on the toes -> the head thrown over -> into the ground
        {{-6, -26, 11, 1, 0, 0, -1, -1, 2, -4, 0, 0, 1, 0, 1, 0}, {-7, -27, 10, 1, 0, 0, -2, -1, 2, -4, 0, 0, 1, 0, 1, 0},
         {-3, -30, 13, 1, 0, 0, 0, -1, 4, -4, E, 0, 1, 0, 1, 0}, {6, -12, 2, 0, 0, 0, 3, 3, 6, -5, E, 0, 0, 1, 0, 0},
         {6, -10, 3, 0, 0, 0, 3, 3, 6, -5, 0, 0, 0, 1, 0, 0}, {4, -12, 2, 0, 0, 0, 2, 2, 5, -4, 0, 0, 0, 1, 0, 0}},
        {{-9, -21, 12, 1, 6, -14, 0, -1, -4, 3, 0, 0, 1, 0, 0, 0}, {-9, -23, 12, 1, 6, -14, 0, -1, -4, 3, 0, 0, 1, 0, 0, 0},
         {-5, -17, 2, 0, 6, -14, 0, 0, -5, 4, E, 0, 0, 0, 0, 0}, {-3, -12, 4, 0, 6, -14, 0, 3, -6, 5, E, 0, 0, 0, 0, 0},
         {-3, -12, 4, 0, 6, -14, 0, 3, -6, 5, 0, 0, 0, 0, 0, 0}, {-4, -13, 4, 0, 6, -14, 0, 2, -5, 4, 0, 0, 0, 0, 0, 0}},
        {{7, -21, 4, 0, -7, -14, 0, -1, 3, -4, 0, 0, 1, 0, 0, 0}, {8, -22, 4, 0, -7, -14, 0, -1, 3, -4, 0, 0, 1, 0, 0, 0},
         {7, -24, 12, 1, -7, -14, 1, -1, 4, -5, E, 0, 1, 0, 0, 0}, {6, -15, 12, 1, -7, -14, 0, 3, 4, -5, E, 0, 0, 0, 0, 0},
         {6, -15, 12, 1, -7, -14, 0, 3, 4, -5, 0, 0, 0, 0, 0, 0}, {6, -15, 13, 1, -7, -14, 0, 2, 4, -5, 0, 0, 0, 0, 0, 0}},
        true, 1, 0, false, false},
};

// ---- MACE (haft 11, flanged head): a smash into the ground, a heavy horizontal hook, a two-handed smash; Smash Ring
const Blow kMace[3] = {
    {   // hit 1 smash: raised behind the head up on the toes -> over -> lands on the ground in front, the knees bent 2 px
        {{-6, -25, 11, 1, 3, -16, -1, -1, 3, -3, 0, 0, 1, 0, 0, 1}, {-7, -26, 10, 1, 3, -17, -1, -1, 3, -4, 0, 0, 1, 0, 1, 1},
         {6, -24, 14, 0, 2, -16, 0, -1, 4, -4, E, 0, 1, 1, 1, 1}, {7, -10, 3, 0, 1, -13, 2, 2, 5, -4, E, 0, 0, 1, 0, 0},
         {7, -10, 3, 0, 1, -13, 2, 2, 5, -4, 0, 0, 0, 1, 0, 0}, {5, -12, 2, 0, 2, -13, 1, 1, 4, -3, 0, 0, 0, 1, 0, 0}},
        {{-9, -23, 12, 1, 6, -14, -1, -1, -5, 4, 0, 0, 1, 0, 0, 0}, {-10, -24, 11, 1, 6, -14, -1, -1, -5, 4, 0, 0, 1, 0, 0, 0},
         {-7, -20, 2, 0, 6, -15, 0, -1, -5, 4, E, 0, 0, 0, 0, 0}, {-4, -9, 4, 0, 6, -13, 0, 2, -6, 5, E, 0, 0, 0, 0, 0},
         {-4, -9, 4, 0, 6, -13, 0, 2, -6, 5, 0, 0, 0, 0, 0, 0}, {-5, -11, 4, 0, 6, -13, 0, 1, -5, 4, 0, 0, 0, 0, 0, 0}},
        {{7, -21, 4, 0, -7, -14, 0, -1, 4, -5, 0, 0, 1, 0, 0, 0}, {8, -23, 4, 0, -7, -14, 0, -1, 4, -5, 0, 0, 1, 0, 0, 0},
         {7, -24, 12, 1, -7, -15, 0, -1, 4, -5, E, 0, 1, 0, 0, 0}, {6, -16, 12, 1, -7, -13, 0, 2, 4, -6, E, 0, 0, 0, 0, 0},
         {6, -16, 12, 1, -7, -13, 0, 2, 4, -6, 0, 0, 0, 0, 0, 0}, {6, -15, 13, 1, -7, -13, 0, 1, 4, -5, 0, 0, 0, 0, 0, 0}},
        false, 1, 0, false, false},
    {   // hit 2 hook: drawn back at head height on the near side -> flat round the front -> on round the far side
        {{-6, -19, 9, 0, 3, -16, -2, 0, 3, -3, 0, 0, 0, 0, 0, 1}, {-7, -20, 8, 0, 3, -16, -2, 0, 3, -4, 0, 0, 0, 0, 0, 1},
         {2, -18, 2, 0, 2, -16, 0, 0, 4, -4, E, 4, 0, 1, 0, 1}, {7, -20, 0, 0, 1, -15, 2, 0, 5, -4, E, 0, 0, 1, 1, 0},
         {7, -22, 13, 1, 1, -15, 1, 0, 5, -4, 0, 4, 0, 0, 0, 0}, {-4, -23, 10, 1, 2, -15, 0, 0, 4, -3, 0, 0, 1, 0, 0, 0}},
        {{-9, -20, 10, 1, 6, -14, -1, 0, -5, 4, 0, 0, 1, 0, 0, 0}, {-10, -21, 9, 1, 6, -14, -2, 0, -5, 4, 0, 0, 1, 0, 0, 0},
         {-8, -18, 6, 0, 6, -15, -1, 0, -5, 4, E, 0, 0, 0, 0, 0}, {-3, -18, 0, 0, 6, -15, 1, 0, -5, 4, E, 0, 0, 0, 0, 0},
         {3, -19, 14, 1, 6, -15, 1, 0, -5, 4, 0, 0, 0, 0, 0, 0}, {5, -20, 13, 1, 6, -14, 1, 0, -5, 4, 0, 0, 1, 0, 0, 0}},
        {{8, -20, 2, 0, -7, -14, 1, 0, 4, -5, 0, 0, 0, 0, 0, 0}, {9, -19, 1, 0, -7, -14, 2, 0, 4, -5, 0, 0, 0, 0, 0, 0},
         {5, -21, 14, 1, -7, -15, 0, 0, 4, -5, E, 0, 0, 0, 0, 0}, {0, -21, 8, 1, -7, -15, -1, 0, 4, -5, E, 0, 0, 0, 0, 0},
         {-6, -19, 6, 0, -8, -11, -1, 0, 4, -5, 0, 0, 0, 0, 0, 0}, {-7, -17, 5, 0, -8, -11, -1, 0, 4, -5, 0, 0, 0, 0, 0, 0}},
        false, 1, 0, false, false},
    {   // hit 3 two-handed smash: both hands up behind the head -> over -> slammed into the ground on a deep crouch
        {{-6, -26, 11, 1, 0, 0, -1, -1, 3, -3, 0, 0, 1, 0, 1, 0}, {-7, -27, 10, 1, 0, 0, -2, -1, 3, -4, 0, 0, 1, 0, 1, 0},
         {-3, -30, 13, 1, 0, 0, 0, -1, 4, -4, E, 0, 1, 0, 1, 0}, {7, -10, 3, 0, 0, 0, 3, 3, 6, -5, E, 0, 0, 1, 0, 0},
         {7, -10, 3, 0, 0, 0, 3, 3, 6, -5, 0, 0, 0, 1, 0, 0}, {5, -12, 2, 0, 0, 0, 2, 2, 5, -4, 0, 0, 0, 1, 0, 0}},
        {{-9, -21, 12, 1, 6, -14, 0, -1, -5, 4, 0, 0, 1, 0, 0, 0}, {-9, -23, 12, 1, 6, -14, 0, -1, -5, 4, 0, 0, 1, 0, 0, 0},
         {-5, -17, 2, 0, 6, -14, 0, 0, -5, 4, E, 0, 0, 0, 0, 0}, {-3, -9, 4, 0, 6, -14, 0, 3, -6, 5, E, 0, 0, 0, 0, 0},
         {-3, -9, 4, 0, 6, -14, 0, 3, -6, 5, 0, 0, 0, 0, 0, 0}, {-4, -11, 4, 0, 6, -14, 0, 2, -5, 4, 0, 0, 0, 0, 0, 0}},
        {{7, -21, 4, 0, -7, -14, 0, -1, 4, -5, 0, 0, 1, 0, 0, 0}, {8, -22, 4, 0, -7, -14, 0, -1, 4, -5, 0, 0, 1, 0, 0, 0},
         {7, -24, 12, 1, -7, -14, 1, -1, 4, -5, E, 0, 1, 0, 0, 0}, {6, -15, 12, 1, -7, -14, 0, 3, 4, -6, E, 0, 0, 0, 0, 0},
         {6, -15, 12, 1, -7, -14, 0, 3, 4, -6, 0, 0, 0, 0, 0, 0}, {6, -15, 13, 1, -7, -14, 0, 2, 4, -5, 0, 0, 0, 0, 0, 0}},
        true, 1, 0, false, false},
};

// ---- SPEAR (shaft 34, two hands 7 px apart): a thrust, a higher thrust, a lunge with a long step; Thrust Streak
const Blow kSpear[3] = {
    {   // hit 1 thrust: the shaft slides back 6, drives forward 10 and 4 more, the front foot forward
        {{-3, -15, 0, 0, 0, 0, 0, 0, 3, -4, 0, 0, 0, 1, 0, 0}, {-4, -15, 0, 0, 0, 0, -1, 0, 3, -4, 0, 0, 0, 1, 0, 0},
         {6, -15, 0, 0, 0, 0, 1, 0, 5, -4, E, 0, 0, 1, 0, 0}, {10, -15, 0, 0, 0, 0, 3, 0, 6, -5, E, 0, 0, 1, 1, 0},
         {8, -15, 0, 0, 0, 0, 2, 0, 6, -5, 0, 0, 0, 1, 0, 0}, {2, -15, 0, 0, 0, 0, 0, 0, 4, -4, 0, 0, 0, 1, 0, 0}},
        {{-3, -12, 4, 0, 0, 0, 0, 0, -5, 4, 0, 2, 0, 0, 0, 0}, {-3, -13, 4, 0, 0, 0, 0, 0, -5, 4, 0, 3, 0, 0, 0, 0},
         {-3, -9, 4, 0, 0, 0, 0, 1, -5, 4, E, 1, 0, 0, 0, 0}, {-2, -6, 4, 0, 0, 0, 0, 1, -6, 5, E, 0, 0, 0, 0, 0},
         {-2, -8, 4, 0, 0, 0, 0, 1, -6, 5, 0, 1, 0, 0, 0, 0}, {-3, -11, 4, 0, 0, 0, 0, 0, -5, 4, 0, 2, 0, 0, 0, 0}},
        {{6, -13, 12, 1, 0, 0, 0, 0, 4, -5, 0, 2, 0, 0, 0, 0}, {6, -11, 12, 1, 0, 0, 0, 0, 4, -5, 0, 2, 0, 0, 0, 0},
         {6, -17, 12, 1, 0, 0, 0, 1, 4, -5, E, 3, 0, 0, 0, 0}, {6, -21, 12, 1, 0, 0, 0, 1, 5, -6, E, 3, 0, 0, 0, 0},
         {6, -19, 12, 1, 0, 0, 0, 1, 5, -6, 0, 3, 0, 0, 0, 0}, {6, -14, 12, 1, 0, 0, 0, 0, 4, -5, 0, 2, 0, 0, 0, 0}},
        true, 1, 0, true, true},
    {   // hit 2 higher thrust: the point raised, drawn back, driven up at the head
        {{-3, -17, 15, 0, 0, 0, 0, 0, 3, -4, 0, 0, 0, 1, 0, 0}, {-5, -17, 15, 0, 0, 0, -1, 0, 3, -4, 0, 0, 0, 1, 0, 0},
         {5, -19, 15, 0, 0, 0, 1, 0, 5, -4, E, 0, 0, 1, 0, 0}, {9, -20, 15, 0, 0, 0, 2, -1, 6, -5, E, 0, 0, 1, 1, 0},
         {7, -19, 15, 0, 0, 0, 2, 0, 6, -5, 0, 0, 0, 1, 0, 0}, {2, -16, 0, 0, 0, 0, 0, 0, 4, -4, 0, 0, 0, 1, 0, 0}},
        {{-4, -12, 5, 0, 0, 0, 0, 0, -5, 4, 0, 2, 0, 0, 0, 0}, {-3, -13, 5, 0, 0, 0, 0, 0, -5, 4, 0, 3, 0, 0, 0, 0},
         {-6, -11, 5, 0, 0, 0, -1, 0, -6, 4, E, 1, 0, 0, 0, 0}, {-7, -8, 5, 0, 0, 0, -1, 0, -6, 5, E, 0, 0, 0, 0, 0},
         {-6, -10, 5, 0, 0, 0, -1, 0, -6, 5, 0, 1, 0, 0, 0, 0}, {-4, -12, 4, 0, 0, 0, 0, 0, -5, 4, 0, 2, 0, 0, 0, 0}},
        {{5, -13, 13, 1, 0, 0, 0, 0, 4, -5, 0, 2, 0, 0, 0, 0}, {5, -11, 13, 1, 0, 0, 0, 0, 4, -5, 0, 2, 0, 0, 0, 0},
         {7, -17, 13, 1, 0, 0, 1, 0, 4, -5, E, 3, 0, 0, 0, 0}, {8, -21, 13, 1, 0, 0, 1, -1, 5, -6, E, 3, 0, 0, 0, 0},
         {7, -19, 13, 1, 0, 0, 1, 0, 5, -6, 0, 3, 0, 0, 0, 0}, {6, -14, 12, 1, 0, 0, 0, 0, 4, -5, 0, 2, 0, 0, 0, 0}},
        true, 1, 0, true, true},
    {   // hit 3 lunge: crouched and drawn back -> a long step in, the whole body behind the point
        {{-2, -14, 0, 0, 0, 0, 0, 1, 3, -4, 0, 0, 0, 1, 0, 0}, {-5, -14, 0, 0, 0, 0, -1, 2, 3, -5, 0, 0, 0, 1, 0, 0},
         {8, -14, 0, 0, 0, 0, 3, 2, 8, -6, E, 0, 0, 1, 1, 0}, {12, -13, 0, 0, 0, 0, 4, 3, 9, -7, E, 0, 0, 1, 1, 0},
         {10, -13, 0, 0, 0, 0, 3, 3, 9, -7, 0, 0, 0, 1, 0, 0}, {4, -14, 0, 0, 0, 0, 1, 1, 5, -5, 0, 0, 0, 1, 0, 0}},
        {{-3, -12, 4, 0, 0, 0, 0, 1, -5, 4, 0, 2, 0, 0, 0, 0}, {-3, -12, 4, 0, 0, 0, 0, 2, -6, 5, 0, 3, 0, 0, 0, 0},
         {-2, -8, 4, 0, 0, 0, 0, 2, -7, 6, E, 1, 0, 0, 0, 0}, {-2, -5, 4, 0, 0, 0, 0, 3, -7, 6, E, 0, 0, 0, 0, 0},
         {-2, -6, 4, 0, 0, 0, 0, 3, -7, 6, 0, 0, 0, 0, 0, 0}, {-3, -10, 4, 0, 0, 0, 0, 1, -6, 5, 0, 2, 0, 0, 0, 0}},
        {{6, -13, 12, 1, 0, 0, 0, 1, 4, -5, 0, 2, 0, 0, 0, 0}, {6, -10, 12, 1, 0, 0, 0, 2, 5, -6, 0, 2, 0, 0, 0, 0},
         {6, -18, 12, 1, 0, 0, 0, 2, 6, -7, E, 3, 0, 0, 0, 0}, {5, -21, 12, 1, 0, 0, 0, 3, 6, -7, E, 4, 0, 0, 0, 0},
         {5, -20, 12, 1, 0, 0, 0, 3, 6, -7, 0, 4, 0, 0, 0, 0}, {6, -14, 12, 1, 0, 0, 0, 1, 5, -6, 0, 2, 0, 0, 0, 0}},
        true, 1, 0, true, true},
};

// ---- STAFF (30, two hands 7 px apart; the walking stick in one hand): a diagonal strike, a rising strike, an overhead
// smash; Swish. Facing down and up hits 1 and 2 swing it in the weapon hand (of = 1: the off hand off the stave, or
// it would cross the whole body); hit 3 holds it level overhead in both hands, then smashes it down.
const Blow kStaff[3] = {
    {   // hit 1 diagonal: the head raised back over the shoulder -> down across the front, the head end leading
        {{-3, -24, 10, 1, 2, -15, -1, 0, 3, -3, 0, 0, 1, 0, 0, 0}, {-5, -26, 10, 1, 2, -15, -1, 0, 3, -4, 0, 0, 1, 0, 1, 0},
         {6, -21, 14, 0, 2, -15, 0, 0, 4, -4, E, 0, 0, 1, 0, 0}, {6, -16, 2, 0, 1, -14, 2, 1, 5, -4, E, 0, 0, 1, 0, 0},
         {5, -13, 2, 0, 1, -13, 2, 1, 5, -4, 0, 0, 0, 1, 0, 0}, {3, -15, 1, 0, 2, -14, 1, 0, 4, -3, 0, 0, 0, 1, 0, 0}},
        {{-9, -21, 10, 1, 6, -14, -1, 0, -5, 4, 0, 0, 1, 0, 0, 1}, {-10, -22, 10, 1, 6, -14, -1, 0, -5, 4, 0, 0, 1, 0, 0, 1},
         {-8, -19, 15, 0, 6, -14, -1, 0, -5, 4, E, 0, 0, 0, 0, 1}, {-4, -15, 2, 0, 6, -13, 1, 1, -5, 4, E, 0, 0, 0, 0, 1},
         {-2, -13, 3, 0, 6, -13, 1, 1, -5, 4, 0, 0, 0, 0, 0, 1}, {-5, -14, 2, 0, 6, -14, 0, 0, -5, 4, 0, 0, 0, 0, 0, 1}},
        {{8, -21, 14, 0, -7, -14, 1, 0, 4, -5, 0, 0, 1, 0, 0, 1}, {9, -22, 14, 0, -7, -14, 1, 0, 4, -5, 0, 0, 1, 0, 0, 1},
         {6, -18, 8, 1, -7, -14, 0, 0, 4, -5, E, 0, 0, 0, 0, 1}, {3, -16, 10, 1, -7, -14, -1, 1, 4, -5, E, 0, 0, 0, 0, 1},
         {0, -14, 9, 1, -8, -12, -1, 1, 4, -5, 0, 0, 0, 0, 0, 1}, {4, -15, 10, 1, -7, -13, 0, 0, 4, -5, 0, 0, 0, 0, 0, 1}},
        true, 1, 0, false, false},
    {   // hit 2 rising: the head low before the feet, drawn back -> swept up through the front to high
        {{1, -16, 3, 0, 2, -15, 0, 1, 3, -3, 0, 0, 0, 1, 0, 0}, {-2, -12, 6, 0, 2, -15, -1, 1, 3, -4, 0, 0, 0, 1, 0, 0},
         {4, -15, 0, 0, 2, -15, 1, 0, 4, -4, E, 0, 0, 1, 0, 0}, {6, -20, 14, 0, 2, -16, 2, -1, 5, -4, E, 0, 0, 1, 1, 0},
         {6, -23, 13, 0, 2, -16, 0, -1, 5, -4, 0, 0, 1, 1, 0, 0}, {2, -18, 0, 0, 2, -15, 0, 0, 4, -3, 0, 0, 0, 1, 0, 0}},
        {{-7, -12, 6, 0, 6, -14, -1, 1, -5, 4, 0, 0, 0, 0, 0, 1}, {-8, -11, 5, 0, 6, -14, -1, 1, -5, 4, 0, 0, 0, 0, 0, 1},
         {-5, -15, 0, 0, 6, -15, 0, 0, -5, 4, E, 0, 0, 0, 0, 1}, {-2, -18, 14, 1, 6, -16, 1, -1, -5, 4, E, 0, 0, 0, 0, 1},
         {-2, -18, 13, 1, 6, -16, 1, -1, -5, 4, 0, 0, 1, 0, 0, 1}, {-4, -16, 0, 0, 6, -14, 0, 0, -5, 4, 0, 0, 0, 0, 0, 1}},
        {{7, -12, 2, 0, -7, -14, 1, 1, 4, -5, 0, 0, 0, 0, 0, 1}, {8, -11, 2, 0, -7, -14, 1, 1, 4, -5, 0, 0, 0, 0, 0, 1},
         {6, -15, 0, 0, -7, -15, 0, 0, 4, -5, E, 0, 0, 0, 0, 1}, {4, -19, 11, 1, -7, -16, 0, -1, 4, -5, E, 0, 0, 0, 0, 1},
         {3, -21, 12, 1, -7, -16, 0, -1, 4, -5, 0, 0, 1, 0, 0, 1}, {5, -16, 0, 0, -7, -14, 0, 0, 4, -5, 0, 0, 0, 0, 0, 1}},
        true, 1, 0, false, false},
    {   // hit 3 overhead smash: held level above the head in both hands, leaning back -> brought down onto the ground
        {{-5, -30, 8, 1, 2, -15, -1, -1, 3, -3, 0, 0, 1, 0, 1, 0}, {-6, -31, 8, 1, 2, -15, -2, -1, 3, -4, 0, 0, 1, 0, 1, 0},
         {-4, -30, 14, 1, 2, -15, 0, -1, 4, -4, E, 0, 1, 0, 1, 0}, {6, -14, 1, 0, 1, -14, 3, 2, 6, -5, E, 0, 0, 1, 0, 0},
         {6, -13, 1, 0, 1, -13, 3, 2, 6, -5, 0, 0, 0, 1, 0, 0}, {4, -15, 0, 0, 2, -14, 2, 1, 5, -4, 0, 0, 0, 1, 0, 0}},
        {{-7, -30, 8, 1, 6, -14, 0, -1, -5, 4, 0, 0, 1, 0, 0, 0}, {-7, -31, 8, 1, 6, -14, 0, -1, -5, 4, 0, 0, 1, 0, 0, 0},
         {-7, -16, 8, 0, 6, -14, 0, 1, -5, 4, E, 0, 0, 0, 0, 0}, {-6, -10, 8, 0, 6, -14, 0, 2, -6, 5, E, 0, 0, 0, 0, 0},
         {-6, -10, 8, 0, 6, -14, 0, 2, -6, 5, 0, 0, 0, 0, 0, 0}, {-6, -12, 8, 0, 6, -14, 0, 1, -5, 4, 0, 0, 0, 0, 0, 0}},
        {{6, -30, 0, 0, -7, -14, 0, -1, 4, -5, 0, 0, 1, 0, 0, 0}, {6, -31, 0, 0, -7, -14, 0, -1, 4, -5, 0, 0, 1, 0, 0, 0},
         {6, -17, 0, 1, -7, -14, 0, 0, 4, -5, E, 0, 0, 0, 0, 0}, {6, -10, 0, 1, -7, -14, 0, 2, 4, -6, E, 0, 0, 0, 0, 0},
         {6, -10, 0, 1, -7, -14, 0, 2, 4, -6, 0, 0, 0, 0, 0, 0}, {6, -12, 0, 1, -7, -14, 0, 1, 4, -5, 0, 0, 0, 0, 0, 0}},
        true, 1, 0, false, false},
};

const Blow* blowsOf(Family f) {
  switch (f) {
    case Family::Unarmed: return kUnarmed;
    case Family::Dagger: return kDagger;
    case Family::Greatsword: return kGreatsword;
    case Family::Axe: return kAxe;
    case Family::Mace: return kMace;
    case Family::Spear: return kSpear;
    case Family::Staff: return kStaff;
    default: return nullptr;
  }
}

// the off hand's px down the grip from the weapon hand along the weapon's line (rig_arms.cpp's stepping, backward)
void gripOffset(int dir, int gap, int& ox, int& oy) {
  static const int kD[16][2] = {{2, 0}, {2, 1}, {2, 2}, {1, 2}, {0, 2}, {-1, 2}, {-2, 2}, {-2, 1},
                                {-2, 0}, {-2, -1}, {-2, -2}, {-1, -2}, {0, -2}, {1, -2}, {2, -2}, {2, -1}};
  const int d = dir & 15, vx = -kD[d][0], vy = -kD[d][1], ax = std::abs(vx), ay = std::abs(vy);
  if (ax >= ay) { ox = gap * (vx > 0 ? 1 : -1); oy = ay == 0 ? 0 : (int)std::floor((gap * ay + ax / 2) / (float)ax) * (vy > 0 ? 1 : -1); }
  else { oy = gap * (vy > 0 ? 1 : -1); ox = ax == 0 ? 0 : (int)std::floor((gap * ax + ay / 2) / (float)ay) * (vx > 0 ? 1 : -1); }
}

// the pose of a key in a facing (the left facing mirrors the right keys)
ActPose actPose(const Body& B, const Kit& K, int fc, const Blow& b, int i) {
  ActPose A;
  const Key& k = fc == FDown ? b.down[i] : fc == FUp ? b.up[i] : b.right[i];
  Pose p = restPose(B, fc, false);
  const int wy = B.hip + 11;   // the woman's upper body is a row lower
  p.expr = (uint8_t)k.ex;
  p.dy = k.dy;
  p.wShort = k.sh;
  p.elbowOut = (uint8_t)k.el;
  p.wDir = armsDrawn(K.weapon) ? k.dir : (int8_t)-1;
  p.wBehind = (uint8_t)k.bh;
  A.edge = b.edge;
  const int gap = armsTwoHandGap(K.weapon);
  // two-handed unless the family's weapon is held in one hand (the walking stick) or a front / back view key frees
  // the off hand (of = 1)
  A.two = b.two && gap > 0 && k.dir >= 0 && !((fc == FDown || fc == FUp) && k.of);
  A.tipHand = b.tip ? b.tip : 1;
  A.split = (fc == FDown && b.splitDown) || (fc == FUp && b.splitUp);
  if (fc == FRight || fc == FLeft) {
    const int s = fc == FRight ? 1 : -1;
    auto mx = [&](int x) { return s > 0 ? x : -2 - x; };   // a fist's top-left mirrored
    auto mf = [&](int x) { return s > 0 ? x : -1 - x; };   // a column mirrored
    p.lean = (int8_t)(k.lean * s);
    p.hRx = (int8_t)mx(k.hx); p.hRy = (int8_t)(k.hy + wy);
    if (p.wDir >= 0) p.wDir = (int8_t)(s > 0 ? k.dir : (8 - k.dir + 16) % 16);
    if (s > 0) {
      p.fRx = k.fr; p.fLx = k.fl;
      if (k.ox || k.oy) { p.hLx = (int8_t)mx(k.ox); p.hLy = (int8_t)(k.oy + wy); }
      p.hLfront = (uint8_t)(k.of ? 1 : 0);
      if (k.lift) p.fLy = 1;
    } else {
      p.fLx = (int8_t)mf(k.fr); p.fRx = (int8_t)mf(k.fl);
      if (k.ox || k.oy) { p.hLx = (int8_t)mx(k.ox); p.hLy = (int8_t)(k.oy + wy); }
      // the weapon in the far hand: its arm in front of the torso while the weapon is (a blade in front of the body
      // always has its arm in front), behind it at its own side; unarmed, the far fist in front whenever it strikes
      p.hRfront = (uint8_t)((k.ff || !k.bh || p.wDir < 0) ? 1 : 0);
      if (k.lift) p.fRy = 1;
      A.edge = (int8_t)-b.edge;
    }
  } else {
    p.lean = k.lean;
    p.hRx = k.hx; p.hRy = (int8_t)(k.hy + wy);
    if (k.ox || k.oy) { p.hLx = k.ox; p.hLy = (int8_t)(k.oy + wy); }
    p.fRx = k.fr; p.fLx = k.fl;
    // the bit faces out from the body: facing down the weapon side is the screen's left (clockwise of a haft pointing
    // down), facing up its right (clockwise of a haft pointing up), so it shows past the head instead of behind it
    A.edge = b.edge;
  }
  if (A.two) {   // the off hand on the grip, armsTwoHandGap px down it (foreshortened with the weapon) from the weapon hand
    int ox, oy;
    const int g = std::max(2, (int)std::lround(gap * (1.0f - std::min(8, (int)k.sh) / 16.0f)));
    gripOffset(p.wDir, g, ox, oy);
    p.hLx = (int8_t)(p.hRx + ox); p.hLy = (int8_t)(p.hRy + oy);
    if (fc == FRight) p.hLfront = 1;   // the far arm reaches across the front to the grip
  }
  A.p = p;
  return A;
}

// ---- Cast 2 (every family): gather (the off hand drawn in before the chest), release (thrust out toward the aim, the
// body leaning in); the weapon carried as at rest (armsRest; the sword as rig_body.cpp carries it); the archer casts
// with the string hand, the bow hanging in the off hand
ActPose castPose(const Body& B, const Kit& K, int fc, int i) {
  ActPose A;
  Pose p = restPose(B, fc, false);
  const int wy = B.hip + 11;
  const bool bow = K.weapon == 3;
  if (K.weapon == 1) {
    const Pose r = restPose(B, fc, true);
    p.hRx = r.hRx; p.hRy = r.hRy; p.wDir = r.wDir; p.wBehind = r.wBehind; p.hRfront = r.hRfront;
  } else if (armsDrawn(K.weapon)) {
    armsRest(B, K.weapon, p);
  }
  const int s = fc == FLeft ? -1 : 1;
  int hx = 0, hy = 0;   // the casting hand
  switch (fc) {
    case FRight: case FLeft: hx = i == 0 ? 3 : 7; hy = i == 0 ? -18 : -20; break;
    case FDown: hx = i == 0 ? 2 : 1; hy = i == 0 ? -18 : -12; break;
    case FUp: hx = i == 0 ? -6 : -7; hy = i == 0 ? -19 : -25; break;
  }
  if (fc == FRight || fc == FLeft) hx = s > 0 ? hx : -2 - hx;
  p.lean = (int8_t)(i == 1 ? (fc == FRight ? 1 : fc == FLeft ? -1 : 0) : 0);
  p.expr = (uint8_t)(i == 1 ? ExEffort : ExNeutral);
  if (bow) {   // the string hand casts; the bow hangs in the off hand, its stave down the side
    if (fc == FDown || fc == FUp) hx = -hx - 2;
    p.hRx = (int8_t)hx; p.hRy = (int8_t)(hy + wy);
    if (fc == FRight) p.hRfront = 1;
    if (fc == FLeft) p.hRfront = 1;
    A.bowDraw = 3;
    A.arrowLen = 0;
    A.bowAim = fc == FRight ? 0 : fc == FLeft ? 8 : fc == FDown ? 4 : 12;
    A.bowHalf = 9;
    A.bowBehind = fc == FRight || fc == FUp;
    A.p = p;
    return A;
  }
  p.hLx = (int8_t)hx; p.hLy = (int8_t)(hy + wy);
  if (fc == FRight) p.hLfront = 1;
  A.p = p;
  return A;
}

// ---- the bow: Draw 3 (nock, chest, cheek) and Release 1 in every facing (STYLE_P2 10); the off hand holds the bow out
// along the aim, the string hand draws back to the cheek
ActPose bowPose(const Body& B, int fc, int i) {   // i 0..2 draw, 3 release
  ActPose A;
  Pose p = restPose(B, fc, false);
  const int wy = B.hip + 11;
  A.bowDraw = (int8_t)i;
  A.arrowLen = 15;
  switch (fc) {
    case FRight: case FLeft: {
      const int s = fc == FRight ? 1 : -1;
      // the bow arm straight out at the jaw's height, the stave clear of the face; the string hand on the arrow's line
      static const int8_t rx[4] = {7, 3, -2, -5}, ry[4] = {-19, -19, -19, -20};
      auto mx = [&](int x) { return s > 0 ? x : -2 - x; };
      p.hLx = (int8_t)mx(9); p.hLy = (int8_t)(-19 + wy);
      p.hRx = (int8_t)mx(rx[i]); p.hRy = (int8_t)(ry[i] + wy);
      if (s > 0) p.hLfront = 1; else p.hRfront = 1;
      p.fRx = (int8_t)(s > 0 ? 3 : -4); p.fLx = (int8_t)(s > 0 ? -3 : 2);
      p.lean = (int8_t)(i == 2 ? -s : 0);
      p.expr = (uint8_t)(i == 2 ? ExEffort : ExNeutral);
      A.bowAim = s > 0 ? 0 : 8;
      A.bowHalf = 11;
      break;
    }
    case FDown: {
      static const int8_t ry[4] = {-15, -17, -19, -19};
      p.hLx = 0; p.hLy = (int8_t)(-14 + wy);
      p.hRx = (int8_t)(i == 3 ? -5 : -1); p.hRy = (int8_t)(ry[i] + wy);
      p.fRx = -5; p.fLx = 4;
      p.expr = (uint8_t)(i == 2 ? ExEffort : ExNeutral);
      A.bowAim = 4;
      A.bowHalf = 10;
      A.arrowLen = 9;
      break;
    }
    case FUp: {
      static const int8_t rx[4] = {-6, -4, -1, 1}, ry[4] = {-23, -21, -19, -19};
      p.hLx = -9; p.hLy = (int8_t)(-25 + wy);
      p.hRx = rx[i]; p.hRy = (int8_t)(ry[i] + wy);
      p.fRx = 4; p.fLx = -5;
      A.bowAim = 10;
      A.bowHalf = 10;
      A.arrowLen = 12;
      A.bowBehind = true;
      break;
    }
  }
  A.p = p;
  return A;
}

// ---- Roll 4 (depth; STYLE_P2 10: crouch, tuck, over, rise, 0.085 s each over the sim's 0.34 s roll): dropped into a
// crouch, curled tight (eyes shut, the head tucked), over (the tuck turned a quarter: the tumble), and rising; the weapon
// carried back along the body (behind it), so it never leads the roll
ActPose rollPose(const Body& B, const Kit& K, int fc, int i) {   // i 0 crouch, 1 tuck, 3 rise (2 = the tuck, turned)
  ActPose A;
  Pose p = restPose(B, fc, false);
  const int wy = B.hip + 11;
  static const int8_t dyOf[4] = {3, 6, 8, 2};
  p.dy = dyOf[i & 3];
  const bool tuck = i == 1 || i == 2;
  p.headY = (int8_t)(i == 2 ? 2 : tuck ? 1 : 0);
  p.expr = (uint8_t)(tuck ? ExClosed : ExEffort);
  p.wBehind = 1;
  if (fc == FRight || fc == FLeft) {
    const int s = fc == FRight ? 1 : -1;
    auto mx = [&](int x) { return s > 0 ? x : -2 - x; };
    auto mf = [&](int x) { return s > 0 ? x : -1 - x; };
    struct R { int8_t nx, ny, fx, fy, lean, fn, ff; };
    static const R kR[4] = {{3, -9, 2, -10, 1, 2, -3}, {2, -6, 1, -7, 2, 1, -2}, {2, -6, 1, -7, 2, 1, -2}, {3, -12, 1, -12, 1, 3, -3}};
    const R& r = kR[i & 3];
    p.lean = (int8_t)(r.lean * s);
    p.hRx = (int8_t)mx(r.nx); p.hRy = (int8_t)(r.ny + wy);
    p.hLx = (int8_t)mx(r.fx); p.hLy = (int8_t)(r.fy + wy);
    if (s > 0) { p.fRx = r.fn; p.fLx = r.ff; p.hLfront = 1; }
    else { p.fLx = (int8_t)mf(r.fn); p.fRx = (int8_t)mf(r.ff); p.hRfront = 0; }
    p.wDir = (int8_t)(s > 0 ? 8 : 0);   // pointing back along the body
  } else {
    const bool dn = fc == FDown;
    struct R { int8_t rx, ry, lx, ly, fa, fb; };
    static const R kR[4] = {{-5, -9, 3, -9, -5, 4}, {-4, -6, 2, -6, -3, 2}, {-4, -6, 2, -6, -3, 2}, {-5, -12, 3, -12, -5, 4}};
    const R& r = kR[i & 3];
    auto sx = [&](int x) { return dn ? x : -2 - x; };   // facing up the weapon hand is on the screen's right
    p.hRx = (int8_t)sx(r.rx); p.hRy = (int8_t)(r.ry + wy);
    p.hLx = (int8_t)sx(r.lx); p.hLy = (int8_t)(r.ly + wy);
    p.fRx = (int8_t)(dn ? r.fa : -1 - r.fa); p.fLx = (int8_t)(dn ? r.fb : -1 - r.fb);
    p.wDir = (int8_t)(dn ? 10 : 14);   // back over the weapon shoulder
  }
  if (i == 2) {   // the ball the tumble turns: the legs folded right up under the body (the turn stands it back on the ground)
    p.fRy = p.fLy = 4;
    if (fc == FRight || fc == FLeft) { p.kR = p.kL = 2; }
  }
  if (!armsDrawn(K.weapon)) p.wDir = -1;
  A.p = p;
  return A;
}

}  // namespace

RigSheet rigAction(const HumanLook& look) {
  const Family fam = familyOf(look);
  const Body B = bodyOf(look);
  Kit K;
  makeKit(look, K);
  SheetBuilder sb;
  if (fam == Family::Sword) {
    // the style slice's three blows, frame for frame (cut back out of its atlas with their pivots and pose data)
    const RigSheet s = sliceSwordAction(look);
    for (int fc = 0; fc < RIG_FACINGS; fc++)
      for (int c = 0; c < CLIP_COUNT; c++)
        for (int v = 0; v < RIG_VARIANTS; v++) {
          const int n = s.spans[RigSheet::slot(fc, (Clip)c, v)].count;
          for (int i = 0; i < n; i++) {
            const FrameRect* r = s.frame(fc, (Clip)c, v, i);
            const RigAnchors* an = s.anchorsOf(fc, (Clip)c, v, i);
            Cut cut;
            cut.c = Canvas(r->w, r->h);
            for (int y = 0; y < r->h; y++) for (int x = 0; x < r->w; x++) cut.c.set(x, y, s.atlas.get(r->x + x, r->y + y));
            cut.px = r->px; cut.py = r->py;
            if (an) cut.an = *an;
            sb.add(fc, (Clip)c, v, std::move(cut));
          }
        }
  } else if (const Blow* blows = blowsOf(fam)) {
    static const Clip clips[3] = {Clip::Windup, Clip::Strike, Clip::Follow};
    for (int fc = 0; fc < RIG_FACINGS; fc++)
      for (int v = 0; v < RIG_VARIANTS; v++)
        for (int i = 0; i < 6; i++) sb.add(fc, clips[i / 2], v, paintAct(look, B, K, actPose(B, K, fc, blows[v], i)));
  }
  if (fam == Family::Bow)
    for (int fc = 0; fc < RIG_FACINGS; fc++) {
      for (int i = 0; i < 3; i++) sb.add(fc, Clip::Draw, 0, paintAct(look, B, K, bowPose(B, fc, i)));
      sb.add(fc, Clip::Release, 0, paintAct(look, B, K, bowPose(B, fc, 3)));
    }
  for (int fc = 0; fc < RIG_FACINGS; fc++)
    for (int i = 0; i < 2; i++) sb.add(fc, Clip::Cast, 0, paintAct(look, B, K, castPose(B, K, fc, i)));
  // Roll 4 (depth): crouch, tuck, over (the tuck turned a quarter, a few px along the roll), rise
  for (int fc = 0; fc < RIG_FACINGS; fc++)
    for (int i = 0; i < 4; i++) {
      const int ahead = fc == FRight ? 3 : fc == FLeft ? -3 : 0;
      sb.add(fc, Clip::Roll, 0, i == 2 ? paintTurned(look, B, K, rollPose(B, K, fc, 2), ahead) : paintAct(look, B, K, rollPose(B, K, fc, i)));
    }
  return sb.finish();
}

}  // namespace rig2
}  // namespace art
