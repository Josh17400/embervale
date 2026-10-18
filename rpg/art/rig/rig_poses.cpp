// EMBERVALE art, v2 rig (POSES, P2a): what town folk do with their bodies on v2 (STYLE_P2 13, 15, 16).
//   the posture painter  rig_core's depth order and RIG-BODY's part painters (face, hair, torso, skirt, hands, the ink
//                        outline) with the posture's own legs (seated at chair height, cross-legged, on one knee,
//                        astride), arms that pass behind the body in the front and back views, and props in hand painted
//                        in their own ramps, lit top-left, the hand drawn over its grip
//   humanPosture2        the 31 postures of art_life.h with today's variants (the bard's instrument by culture, the kneel's
//                        binding and mourning), frame counts and paces (postureInfo), in four facings painted natively:
//                        the left facing is the right one's pose mirrored and painted again (the light stays top-left;
//                        a civilian's prop is in the NEAR hand side-on, so it always reads)
//   seatFit2             where a v2 sitter's pivot goes on each seat kind (agreed with HOLDERS through holderPx)
// Pivots: a standing posture's pivot is the point between the feet on the sole row (as LOCO); a seated one's is the floor
// right under the hips (the seat's own floor line), so the hips' row is -seatH; cross-legged: the floor or the cushion's
// top under the hips (hips at -2); kneeling: the floor under the hips (hips at -5). The hip anchor (RigAnchors::hipX /
// hipY) is the seat point: the hips' row, the row the seat's surface line lands on (rpg_test --rig --gates P2).
#include <algorithm>
#include <cmath>
#include <cstring>
#include "rpg/art/art_holders2.h"
#include "rpg/art/rig/rig_internal.h"

namespace art {
namespace rig2 {

// the hips' row of a chair-height sitter over its pivot (the floor row): HOLDERS' seat line (seatRow2, the middle of the
// seat's top) is kSeat + 2 = 13 rows over the floor row
constexpr int kChairHip = -(holders::kSeat + 2);

// ================================================================ the posture painter
namespace {

// two-bone solve (rig_core's): the joint between a and b (lengths l1, l2), bent toward side (+1 / -1 of the a->b normal)
void joint2(float ax, float ay, float bx, float by, float l1, float l2, float side, float& jx, float& jy) {
  const float dx = bx - ax, dy = by - ay, d = std::max(1e-3f, std::sqrt(dx * dx + dy * dy));
  if (d >= l1 + l2 - 0.05f) { jx = ax + dx * l1 / (l1 + l2); jy = ay + dy * l1 / (l1 + l2); return; }
  const float a = (l1 * l1 - l2 * l2 + d * d) / (2 * d), h = std::sqrt(std::max(0.0f, l1 * l1 - a * a));
  const float ux = dx / d, uy = dy / d;
  jx = ax + ux * a - uy * h * side;
  jy = ay + uy * a + ux * h * side;
}

// an arm (rig_core's): the sleeve from the shoulder to the elbow, the forearm (skin or sleeve) to the wrist
void armX(Fig& f, const Kit& K, float sx, float sy, float wx, float wy, float bendSide, bool farSide, int lu, int lf, bool longSleeve) {
  float ex, ey;
  joint2(sx, sy, wx, wy, (float)lu, (float)lf, bendSide, ex, ey);
  const int bias = farSide ? -1 : 0;
  const Ramp5& sl = sleeveRamp(K);
  limb(f, ex, ey, wx, wy, 2, longSleeve ? sl : K.skin, bias);
  limb(f, sx, sy, ex, ey, 2, sl, bias);
  if (!longSleeve) {
    const int cyI = (int)std::floor(ey - 0.001f), cxI = (int)std::floor(ex - 1.0f + 0.5f);
    for (int k = 0; k < 2; k++) if (f.solid(cxI + k, cyI)) f.set(cxI + k, cyI, sl[k == 0 ? 3 + bias : 2 + bias]);
  }
}

// a shoe at the ankle (rig_core's): front / back views the ankle's columns then one more outside below; side views heel
// 1 px behind, toe ahead
void shoe(Fig& f, const Kit& K, float ax, int sy, int facing, int w, int shoeW, int bias) {
  const Ramp5& b = K.boot;
  const int axI = (int)std::floor(ax);
  if (facing == FDown || facing == FUp) {
    const int a0 = (int)std::floor(ax - w * 0.5f + 0.5f), a1 = a0 + w - 1;
    for (int x = a0; x <= a1; x++) f.set(x, sy, b[x == a0 ? 3 + bias : 2 + bias]);
    for (int x = a0 - 1; x <= a1; x++) f.set(x, sy + 1, b[x == a1 ? 1 + bias : 2 + bias]);
    if (facing == FDown) f.set(a0, sy, b[4 + bias]);
  } else {
    const int s = facing == FRight ? 1 : -1;
    const int toe = shoeW - 1;
    for (int k = -1; k <= 1; k++) f.set(axI + k * s, sy, b[k == -1 ? (s > 0 ? 3 : 1) + bias : 2 + bias]);
    for (int k = -1; k <= toe; k++) f.set(axI + k * s, sy + 1, b[k == toe ? 1 + bias : 2 + bias]);
    if (toe >= 3) f.set(axI + 2 * s, sy, b[2 + bias]);
  }
}

// a standing leg (rig_core's leg): hip joint -> knee -> ankle, the shoe under the ankle
void standLeg(Fig& f, const Kit& K, float hx, float hy, float ax, float ay, float bendSide, bool farSide, int facing, float kneeFwd, int w, int shoeW) {
  const float lt = (ay - hy) * 0.5f + 0.02f;
  float kx, ky;
  joint2(hx, hy, ax, ay, lt, lt, bendSide, kx, ky);
  kx += kneeFwd;
  const int bias = farSide ? -1 : 0;
  const Ramp5& lr = legRamp(K);
  limb(f, hx, hy, kx, ky, w, lr, bias);
  limb(f, kx, ky, ax, ay, w, lr, bias);
  shoe(f, K, ax, (int)std::floor(ay), facing, w, shoeW, bias);
}

// a row of cloth (trousers or a skirt) from x0 to x1 on row y: lit at the left end, shaded at the right, idx in between
void clothRow(Fig& f, const Kit& K, const Ramp5& R, bool pattern, int x0, int x1, int y, int idx, int litL = 3, int darkR = 1) {
  for (int x = x0; x <= x1; x++) {
    const int k = x == x0 ? std::max(idx, litL) : (x == x1 ? std::min(idx, darkR) : idx);
    f.set(x, y, pattern ? patternAt(K, x, y, R[k], k) : R[k]);
  }
}

struct Joints {
  int cx = 0, hipRow = 0, shoulderRow = 0, sx = 0;
  HeadAt head;
};

// ---------------------------------------------------------------- the posture legs (pivot-relative)
// The seated, cross-legged, kneeling and riding legs, in the trousers' ramp (a skirt's cloth instead where the garment
// is long), the shoes in the boot ramp. far: side views paint the far leg first (one ramp step darker), then the near.
void chairLegs(Fig& f, const Body& B, const Kit& K, const PoseX& X, const Joints& J, bool farPass) {
  const int fc = X.p.facing, H = J.hipRow, cx = J.cx;
  const Ramp5& lr = legRamp(K);
  const int w = B.legW;
  const int shinRows = kinSeatLegs(B, X.seatH);
  if (fc == FRight || fc == FLeft) {
    const int s = fc == FRight ? 1 : -1;
    const int bias = farPass ? -1 : 0;
    const float back = farPass ? -(float)s : 0.0f;               // the far leg a pixel behind the near one
    const float kneeX = cx + 0.5f + 5.0f * s + back;              // the knee's centre column
    const float hipX = cx + 0.5f - 1.5f * s + back;
    limb(f, hipX, H + 1.5f, kneeX, H + 2.5f, w, lr, bias);      // the thigh, sloping 1 px down to the knee
    const float ankY = std::min(-2.0f, (float)(H + 3 + shinRows));
    const float ankX = kneeX + 0.5f * s;
    limb(f, kneeX, H + 2.5f, ankX, ankY, w, lr, bias);           // the shin, down to the floor (a child's dangles)
    shoe(f, K, ankX, (int)std::floor(ankY), fc, w, B.shoeW, bias);
    // the knee's top catches the light (the lap is a horizontal plane)
    for (int k = -1; k <= 1; k++) if (f.solid((int)std::floor(kneeX) + k, H + 1)) f.set((int)std::floor(kneeX) + k, H + 1, lr[std::min(4, 3 + bias + (k < 0 ? 1 : 0))]);
    return;
  }
  if (farPass) return;
  if (fc == FDown) {
    // the lap: the thighs come toward the camera as a flat, lit plane (two rows of light on their tops, the knees' front
    // faces below), the knees a pixel apart; the shins hang in the lap's shadow (a ramp darker, the row under the knees
    // darkest) to the feet set a little forward of the seat (the soles on row +1 under the hips' pivot)
    // the thighs splay from the hips (touching at the crotch) to knees set apart, so the chair's seat shows between them
    const int lapRows = B.age == 1 ? 3 : 4;
    const int tw = w + 1;   // a thigh seen from above is a pixel wider than its shin
    const float ankY = std::min(-2.0f, (float)(H + 1 + lapRows + shinRows));   // the feet tucked under the seat's front
    for (int side = -1; side <= 1; side += 2) {
      int kneeL = 0, kneeR = 0;
      for (int r = 0; r < lapRows; r++) {
        const int y = H + 1 + r;
        const bool knee = r == lapRows - 1;
        const int inner = side < 0 ? cx - 1 - (r + 1) / 2 : cx + (r + 1) / 2;
        const int x0 = side < 0 ? inner - tw + 1 : inner, x1 = x0 + tw - 1;
        for (int x = x0; x <= x1; x++) {
          int idx;
          if (r == 0) idx = x == x0 ? 4 : (x == x1 ? 2 : 3);       // the lap's top catches the light
          else if (!knee) idx = x == x0 ? 3 : (x == x1 ? 1 : 2);
          else idx = x == x0 ? 2 : (x == x1 ? 0 : 1);              // the knee's front face, turned from the light
          if (knee && ((side < 0 && x == x0) || (side > 0 && x == x1))) continue;   // the knee's round outer corner
          f.set(x, y, lr[idx]);
        }
        if (r == 0 && side < 0) f.set(x0 + 1, y, lr[4]);           // a sheen on the lit thigh
        if (knee) { kneeL = x0; kneeR = x1; }
      }
      const float kx = (kneeL + kneeR + 1) * 0.5f;
      const int shin0 = H + 1 + lapRows;
      limb(f, kx, (float)shin0, kx, ankY, w, lr, -1);             // the shin, in the lap's shadow
      for (int x = kneeL; x <= kneeR; x++) if (f.solid(x, shin0)) f.set(x, shin0, lr[0]);   // the shadow under the knee
      shoe(f, K, kx, (int)std::floor(ankY), fc, w, B.shoeW, 0);
    }
    return;
  }
  // facing away: the seat of the trousers on the seat; the legs go away under the body
  const int hw = B.hipHalf;
  clothRow(f, K, lr, false, cx - hw + 1, cx + hw - 2, H + 1, 1, 2, 0);
}

void floorLegs(Fig& f, const Body& B, const Kit& K, const PoseX& X, const Joints& J, bool farPass) {
  if (farPass) return;
  const int fc = X.p.facing, H = J.hipRow, cx = J.cx;
  const Ramp5& lr = legRamp(K);
  const Ramp5& bt = K.boot;
  const int hw = B.hipHalf;
  const int kid = B.age == 1 ? 1 : 0;   // a child's lap is a pixel narrower each side
  if (fc == FDown) {
    // the lap: the thighs out to the knees, the crossed shins in front, the feet tucked under the opposite knees
    const int half[4] = {hw + 1 - kid, hw + 2 - kid, hw + 2 - kid, hw - kid};
    for (int r = 0; r < 4; r++) {
      const int y = H + 1 + r, x0 = cx - half[r], x1 = cx + half[r] - 1;
      const int idx = r == 0 ? 3 : (r == 3 ? 1 : 2);
      clothRow(f, K, lr, false, x0, x1, y, idx, r == 0 ? 4 : 3, r == 0 ? 2 : 1);
    }
    // the knees: a lit cap at each outer end of the widest rows
    f.set(cx - half[1], H + 2, lr[3]);
    f.set(cx + half[1] - 1, H + 2, lr[1]);
    // the crossed shins: a fold line where the near shin lies over the far one
    for (int k = 0; k <= 3; k++) f.set(cx - 3 + k, H + 3 - (k >= 2 ? 1 : 0), lr[1]);
    f.set(cx + 1, H + 2, lr[1]);
    // the shoes tucked under the knees
    const int sl = cx - half[1] + 2, sr = cx + half[1] - 4;
    f.set(sl, H + 3, bt[3]); f.set(sl + 1, H + 3, bt[2]); f.set(sl + 1, H + 4, bt[1]);
    f.set(sr, H + 3, bt[2]); f.set(sr + 1, H + 3, bt[1]); f.set(sr, H + 4, bt[1]);
    return;
  }
  if (fc == FRight || fc == FLeft) {
    const int s = fc == FRight ? 1 : -1;
    auto X0 = [&](int k) { return s > 0 ? cx + k : cx - 1 - k; };   // a column k px toward the facing
    const int len[3] = {5 - kid, 6 - kid, 6 - kid}, from[3] = {-3, -3, -2};
    for (int r = 0; r < 3; r++) {
      const int y = H + 1 + r;
      for (int k = from[r]; k <= len[r]; k++) {
        int idx = r == 0 ? 3 : (r == 2 ? 1 : 2);
        if (k == len[r]) idx = std::max(0, idx - 1);             // the knee's front end
        if (k == from[r] && s > 0) idx = std::min(4, idx + 1);  // the lit back (facing right the back is on the left)
        f.set(X0(k), y, lr[idx]);
      }
    }
    // the near foot tucked under the knee
    f.set(X0(2 - kid), H + 3, bt[s > 0 ? 3 : 2]); f.set(X0(3 - kid), H + 3, bt[2]); f.set(X0(4 - kid), H + 3, bt[1]);
    return;
  }
  // facing away: the knees out at both sides of the hips
  clothRow(f, K, lr, false, cx - hw - 1 + kid, cx + hw - kid, H + 1, 1, 2, 0);
  for (int k = 0; k < 2; k++) { f.set(cx - hw - 1 + kid + k, H + 2, lr[k ? 1 : 2]); f.set(cx + hw - 1 - kid + k, H + 2, lr[k ? 0 : 1]); }
}

void kneelLegs(Fig& f, const Body& B, const Kit& K, const PoseX& X, const Joints& J, bool farPass) {
  const int fc = X.p.facing, H = J.hipRow, cx = J.cx;
  const Ramp5& lr = legRamp(K);
  const Ramp5& bt = K.boot;
  const int w = B.legW;
  if (fc == FRight || fc == FLeft) {
    const int s = fc == FRight ? 1 : -1;
    if (farPass) {
      // the far leg kneels: the thigh straight down to the knee on the ground, the shin back along it, toes down
      const float hx = cx + 0.5f - 0.5f * s, kx = cx + 0.5f - 1.0f * s;
      limb(f, hx, H + 1.0f, kx, -1.5f, w, lr, -1);
      limb(f, kx, -1.5f, kx - 5.0f * s, -1.5f, 2, lr, -1);
      const int tx = (int)std::floor(kx - 5.0f * s);
      f.set(tx, -2, bt[1]); f.set(tx - s, -2, bt[1]); f.set(tx - s, -1, bt[0]); f.set(tx, -1, bt[1]);
      return;
    }
    // the near leg: the thigh level forward to the raised knee, the shin straight down, the shoe flat
    const float hx = cx + 0.5f - 1.0f * s, kx = cx + 0.5f + 4.0f * s;
    limb(f, hx, H + 1.5f, kx, H + 1.5f, w, lr, 0);
    limb(f, kx, H + 1.5f, kx, -2.0f, w, lr, 0);
    shoe(f, K, kx, -2, fc, w, B.shoeW, 0);
    for (int k = -1; k <= 1; k++) if (f.solid((int)std::floor(kx) + k, H)) f.set((int)std::floor(kx) + k, H, lr[k < 0 ? 4 : 3]);
    return;
  }
  if (farPass) return;
  const float hj = B.hipJoint;
  if (fc == FDown) {
    // screen-left (the right leg): the knee up, the thigh toward the camera, the shin down to the planted foot
    const float hx = cx - hj, kx = cx - hj - 0.5f;
    limb(f, hx, H + 1.0f, kx, H + 3.0f, w, lr, 0);
    const int x0 = (int)std::floor(kx - w * 0.5f + 0.5f);
    for (int k = 0; k < w; k++) f.set(x0 + k, H + 1, lr[k == 0 ? 4 : 3]);
    limb(f, kx, H + 3.0f, kx, -1.0f, w, lr, 0);
    shoe(f, K, kx, -1, fc, w, B.shoeW, 0);
    // screen-right (the left leg): the thigh straight down to the knee on the ground (the shin goes away, unseen)
    const float lx = cx + hj;
    limb(f, lx, H + 1.0f, lx, 0.0f, w, lr, 0);
    const int l0 = (int)std::floor(lx - w * 0.5f + 0.5f);
    for (int k = 0; k < w; k++) f.set(l0 + k, -1, lr[k == 0 ? 2 : 1]);
    return;
  }
  // facing away: the kneeling leg (screen-left) down to its knee, the shin toward the camera on the ground, the sole up;
  // the raised leg goes away under the body (its thigh's top only)
  const float lx = cx - hj;
  limb(f, lx, H + 1.0f, lx, 0.0f, w, lr, -1);
  const int l0 = (int)std::floor(lx - w * 0.5f + 0.5f);
  for (int k = 0; k < w; k++) { f.set(l0 + k, 0, lr[k == 0 ? 2 : 1]); f.set(l0 + k, 1, lr[1]); }
  for (int k = 0; k < w; k++) f.set(l0 + k, 2, bt[k == 0 ? 2 : 1]);
  const float rx = cx + hj;
  limb(f, rx, H + 1.0f, rx, H + 3.0f, w, lr, -1);
}

void rideLegs(Fig& f, const Body& B, const Kit& K, const PoseX& X, const Joints& J, bool farPass) {
  const int fc = X.p.facing, H = J.hipRow, cx = J.cx;
  const Ramp5& lr = legRamp(K);
  const int w = B.legW;
  const float L = (float)B.legs;   // (a kid's legs are shorter)
  if (fc == FRight || fc == FLeft) {
    if (farPass) return;   // the far leg is behind the horse
    const int s = fc == FRight ? 1 : -1;
    const float hx = cx + 0.5f - 0.5f * s, kx = cx + 0.5f + 3.0f * s, ky = H + 1.0f + L * 0.42f;
    const float ax = cx + 0.5f + 1.0f * s, ay = H + 1.0f + L * 0.85f;
    limb(f, hx, H + 1.0f, kx, ky, w, lr, 0);
    limb(f, kx, ky, ax, ay, w, lr, 0);
    shoe(f, K, ax, (int)std::floor(ay), fc, w, B.shoeW, 0);
    return;
  }
  if (farPass) return;
  const float hj = B.hipJoint;
  for (int side = -1; side <= 1; side += 2) {
    const float hx = cx + side * hj, kx = cx + side * (hj + 3.0f), ky = H + 1.0f + L * 0.4f;
    const float ax = cx + side * (hj + 3.5f), ay = H + 1.0f + L * 0.8f;
    limb(f, hx, H + 1.0f, kx, ky, w, lr, fc == FUp ? -1 : 0);
    limb(f, kx, ky, ax, ay, w, lr, fc == FUp ? -1 : 0);
    shoe(f, K, ax, (int)std::floor(ay), fc, w, B.shoeW, fc == FUp ? -1 : 0);
  }
}

// a skirt over seated / cross-legged / kneeling legs (Dress, Robe, the long cuts): the cloth over the lap, falling to
// just above the shoes (STYLE_P2 2: clothes hang, hems lit)
void seatedSkirt(Fig& f, const Body& B, const Kit& K, const PoseX& X, const Joints& J) {
  const int fc = X.p.facing, H = J.hipRow, cx = J.cx;
  const bool longHang = K.hang == HangLong || K.skirt;   // else a short / knee-length hang: over the lap only
  const Ramp5& C = K.top;
  const int hw = B.hipHalf;
  if (X.legs == LegChair) {
    if (fc == FRight || fc == FLeft) {
      const int s = fc == FRight ? 1 : -1;
      auto col = [&](int k) { return s > 0 ? cx + k : cx - 1 - k; };
      // over the thigh from the back of the hips to past the knee, then hanging from the knee to the hem
      for (int y = H; y <= H + 3; y++)
        for (int k = -3; k <= 6; k++) {
          int idx = y == H ? 3 : 2;
          if (k == 6) idx = 1;
          if (k == -3) idx = s > 0 ? 3 : 1;
          f.set(col(k), y, patternAt(K, col(k), y, C[idx], idx));
        }
      const int hem = -4;
      for (int y = H + 4; longHang && y <= hem; y++)
        for (int k = 3; k <= 7; k++) {
          int idx = 2;
          if (k == 7) idx = 1;
          if (k == 3) idx = s > 0 ? 3 : 1;
          if (y == hem) idx = k == 7 ? 2 : 3;
          if (k == 5 && y > H + 4 && y < hem) idx = 1;   // a fold
          f.set(col(k), y, patternAt(K, col(k), y, C[idx], idx));
        }
      return;
    }
    if (fc == FDown) {
      // the lap (wider than the hips: the knees apart under the cloth, each a lit swell, the valley between them in
      // shadow), then the skirt falling in front of the shins to just above the shoes
      for (int y = H; y <= H + 4; y++) {
        const int half = hw + (y >= H + 2 ? 1 : 0);
        for (int x = cx - half; x <= cx + half - 1; x++) {
          int idx = y <= H + 1 ? 3 : 2;
          if (x == cx - half) idx = 3;
          if (x == cx + half - 1) idx = 1;
          const int rel = x - cx;
          if (y >= H + 2 && y <= H + 3 && (rel == -4 || rel == -3 || rel == 1 || rel == 2)) idx = (rel == -4 || rel == 1) ? 4 : 3;   // the knees
          if (y >= H + 3 && (rel == -1 || rel == 0)) idx = rel == -1 ? 2 : 1;   // the valley between the knees
          f.set(x, y, patternAt(K, x, y, C[idx], idx));
        }
      }
      const int hem = -3;
      for (int y = H + 5; longHang && y <= hem; y++) {
        const int half = hw + 1 + (y - (H + 5)) / 3;
        for (int x = cx - half; x <= cx + half - 1; x++) {
          int idx = 2;
          if (x == cx - half) idx = 3;
          else if (x == cx + half - 1) idx = 1;
          const int rel = x - cx;
          if (rel == 0 && y < hem) idx = 1;                   // the fold falling from between the knees
          if (rel == -3 && y < hem && x != cx - half) idx = 3;   // the lit fall from the near knee
          if (y == hem) idx = x == cx + half - 1 ? 2 : 3;
          f.set(x, y, patternAt(K, x, y, C[idx], idx));
        }
      }
      return;
    }
    for (int y = H; y <= H + 2; y++) {
      const int half = hw + (y == H + 2 ? 1 : 0);
      for (int x = cx - half; x <= cx + half - 1; x++) {
        int idx = y == H + 2 ? 3 : 1;
        if (x == cx - half) idx = 2;
        f.set(x, y, patternAt(K, x, y, C[idx], idx));
      }
    }
    return;
  }
  if (!longHang && X.legs != LegChair) return;
  if (X.legs == LegFloor) {
    // the skirt spread over the crossed legs: the lap's shape in cloth, the hem lit, the shoes peeking out
    if (fc == FDown) {
      const int half[4] = {hw + 1, hw + 2, hw + 2, hw + 1};
      for (int r = 0; r < 4; r++) {
        const int y = H + 1 + r;
        for (int x = cx - half[r]; x <= cx + half[r] - 1; x++) {
          int idx = r == 0 ? 3 : 2;
          if (x == cx - half[r]) idx = 3;
          if (x == cx + half[r] - 1) idx = 1;
          if (r == 3) idx = x == cx + half[r] - 1 ? 2 : 3;
          if (r == 2 && (x == cx - 3 || x == cx + 2)) idx = 1;
          f.set(x, y, patternAt(K, x, y, C[idx], idx));
        }
      }
      f.set(cx - hw - 1, H + 5, K.boot[2]); f.set(cx - hw, H + 5, K.boot[1]);
      f.set(cx + hw - 1, H + 5, K.boot[2]); f.set(cx + hw, H + 5, K.boot[1]);
      return;
    }
    if (fc == FRight || fc == FLeft) {
      const int s = fc == FRight ? 1 : -1;
      auto col = [&](int k) { return s > 0 ? cx + k : cx - 1 - k; };
      for (int r = 0; r < 4; r++)
        for (int k = -3; k <= 6; k++) {
          const int y = H + r;
          int idx = r <= 1 ? 3 : 2;
          if (k == 6) idx = 1;
          if (r == 3) idx = k == 6 ? 2 : 3;
          f.set(col(k), y, patternAt(K, col(k), y, C[idx], idx));
        }
      return;
    }
    for (int y = H + 1; y <= H + 2; y++)
      for (int x = cx - hw - 2; x <= cx + hw + 1; x++) {
        int idx = y == H + 2 ? 3 : 1;
        if (x == cx - hw - 2) idx = 2;
        f.set(x, y, patternAt(K, x, y, C[idx], idx));
      }
    return;
  }
  if (X.legs == LegKneel) {
    // to the ground over the kneeling leg; over the raised knee
    if (fc == FRight || fc == FLeft) {
      const int s = fc == FRight ? 1 : -1;
      auto col = [&](int k) { return s > 0 ? cx + k : cx - 1 - k; };
      for (int y = H; y <= -2; y++) {
        const int front = y <= H + 2 ? 5 : 3, back = -3 - (y - H) / 2;
        for (int k = back; k <= front; k++) {
          int idx = y <= H + 1 ? 3 : 2;
          if (k == front) idx = 1;
          if (k == back) idx = s > 0 ? 3 : 1;
          if (y == -2) idx = k == front ? 2 : 3;
          f.set(col(k), y, patternAt(K, col(k), y, C[idx], idx));
        }
      }
      return;
    }
    for (int y = H; y <= -1; y++) {
      const int half = hw + 1 + (y - H) / 3;
      for (int x = cx - half; x <= cx + half - 1; x++) {
        int idx = y <= H + 1 ? 3 : 2;
        if (x == cx - half) idx = 3;
        if (x == cx + half - 1) idx = 1;
        if (y == -1) idx = x == cx + half - 1 ? 2 : 3;
        if (fc == FUp) idx = std::max(0, idx - 1);
        f.set(x, y, patternAt(K, x, y, C[idx], idx));
      }
    }
  }
}

// an open hand raised (fingers up: a wave, a toast): the 2 x 2 fist and its fingers' row above
void handUp(Fig& f, const Kit& K, int x, int y) {
  paintHand(f, K, x, y);
  f.set(x, y - 1, K.skin[3]);
  f.set(x + 1, y - 1, K.skin[2]);
}
// two hands joined in prayer (3 x 2 and the fingertips' 2 px above, STYLE_P2 5)
void handsJoined(Fig& f, const Kit& K, int x, int y) {
  f.set(x, y, K.skin[3]); f.set(x + 1, y, K.skin[2]); f.set(x + 2, y, K.skin[2]);
  f.set(x, y + 1, K.skin[2]); f.set(x + 1, y + 1, K.skin[1]); f.set(x + 2, y + 1, K.skin[1]);
  f.set(x + 1, y - 1, K.skin[3]); f.set(x + 1, y - 2, K.skin[3]); f.set(x + 2, y - 1, K.skin[2]);
}

void paintPoseInto(Fig& f, const HumanLook& look, const Body& B, const Kit& K, const PoseX& X, RigAnchors& an) {
  an = RigAnchors();
  const Pose& P = X.p;
  const int fc = P.facing;
  const bool front = fc == FDown, back = fc == FUp, side = fc == FRight || fc == FLeft;
  const int s = fc == FLeft ? -1 : 1;
  Joints J;
  J.cx = P.hipX;
  J.hipRow = B.hip + P.dy;
  J.shoulderRow = B.shoulder + P.dy + P.chest;
  J.sx = J.cx + P.lean;
  J.head.x = J.sx - 6 + P.headX;
  J.head.y = B.headTop - 1 + P.dy + P.chest + P.headY;
  const TorsoAt T{J.sx, J.shoulderRow};
  const float hipY = (float)J.hipRow + 1.0f;
  const bool longSleeve = longSleeved(K);
  const int lu = B.armU, lf = B.armF;
  float hRx, hLx;
  if (front) { hRx = J.cx - B.hipJoint; hLx = J.cx + B.hipJoint; }
  else if (back) { hRx = J.cx + B.hipJoint; hLx = J.cx - B.hipJoint; }
  else { hRx = J.cx + 0.5f - 0.5f * s; hLx = J.cx + 0.5f + 0.5f * s; }
  const float aRx = P.fRx + 0.5f, aLx = P.fLx + 0.5f;
  const float aRy = -2.0f - P.fRy, aLy = -2.0f - P.fLy;
  float shRx, shLx, shY = (float)J.shoulderRow + (side ? 0.5f : 1.5f);
  if (front) { shRx = J.sx - B.shoulderHalf + 1.0f; shLx = J.sx + B.shoulderHalf - 1.0f - B.trimR; }
  else if (back) { shRx = J.sx + B.shoulderHalf - 1.0f - B.trimR; shLx = J.sx - B.shoulderHalf + 1.0f; }
  else { shRx = (float)J.sx - 1.0f * (float)s; shLx = (float)J.sx; if (s < 0) { shRx = (float)J.sx + 1.0f; shLx = (float)J.sx; } }
  // the wrist meets the fist from below when the hand is raised over the shoulder, from above otherwise
  auto wristY = [&](int hy) { return hy < J.shoulderRow - 1 ? hy + 2.0f : hy - 0.5f; };
  const float wRx = P.hRx + 1.0f, wRy = wristY(P.hRy), wLx = P.hLx + 1.0f, wLy = wristY(P.hLy);
  auto bendOf = [&](bool right) {
    if (front || back) {
      const bool screenLeft = (front && right) || (back && !right);
      return (screenLeft ? 1.0f : -1.0f) * (P.elbowOut ? 1.0f : 0.3f);
    }
    return (P.elbowOut ? -1.0f : 1.0f) * (float)s;
  };
  auto hook = [&](int layer) {
    if (!X.prop) return;
    f.begin();
    X.prop(f, B, K, X, layer, X.ctx);
    f.contour(0.6f, 0.2f);
  };
  auto legsPass = [&](bool far, bool which) {   // which: the R leg (standing legs only)
    f.begin();
    switch (X.legs) {
      case LegStand:
        if (which) standLeg(f, K, hRx, hipY, aRx, aRy, side ? -(float)s : (front ? 0.2f : -0.2f), far, fc, side ? (float)(P.kR * s) : 0.0f, B.legW, B.shoeW);
        else standLeg(f, K, hLx, hipY, aLx, aLy, side ? -(float)s : (front ? -0.2f : 0.2f), far, fc, side ? (float)(P.kL * s) : 0.0f, B.legW, B.shoeW);
        break;
      case LegChair: chairLegs(f, B, K, X, J, far); break;
      case LegFloor: floorLegs(f, B, K, X, J, far); break;
      case LegKneel: kneelLegs(f, B, K, X, J, far); break;
      case LegRide: rideLegs(f, B, K, X, J, far); break;
      default: break;
    }
    if (side && !far) f.contour(0.55f, 0.2f);
  };
  auto drawLegs = [&]() {
    if (X.legs == LegStand) {
      uint8_t la = 0, lb = 0;
      if (side) {
        const bool rightNear = fc == FRight;
        legsPass(true, !rightNear); la = f.cur;
        legsPass(false, rightNear); lb = f.cur;
      } else { legsPass(false, true); la = f.cur; legsPass(false, false); lb = f.cur; }
      // the kilt's bare knees, the rags' torn shins (RIG-BODY's leg wear, over both standing legs)
      if (K.rags || K.cut == 6) { f.begin(); paintLegWear(f, B, K, P, la, lb); }
    } else {
      if (side) legsPass(true, false);
      legsPass(false, false);
    }
  };
  uint8_t armLayerR = 0, armLayerL = 0;
  // (rig_core) the poncho hides an arm that hangs under it; an arm raised out of it is drawn over it
  const int ponchoHem = J.hipRow + 2;
  auto underPoncho = [&](int handY) { return K.cut == 7 && handY >= J.shoulderRow + 5 && handY <= ponchoHem + 1; };
  auto drawArmR = [&](bool farSide) {
    f.begin(); armLayerR = f.cur;
    if (underPoncho(P.hRy)) return;
    armX(f, K, shRx, shY, wRx, wRy, bendOf(true), farSide, lu, lf, longSleeve); f.contour();
  };
  auto drawArmL = [&](bool farSide) {
    f.begin(); armLayerL = f.cur;
    if (underPoncho(P.hLy)) return;
    armX(f, K, shLx, shY, wLx, wLy, bendOf(false), farSide, lu, lf, longSleeve); f.contour();
  };
  auto drawHandR = [&]() {
    if (X.hideHandR) return;
    f.begin();
    if (X.openR) handUp(f, K, P.hRx, P.hRy); else paintHand(f, K, P.hRx, P.hRy);
    f.contour(0.7f, 0.25f, armLayerR);
  };
  auto drawHandL = [&]() {
    if (X.hideHandL) return;
    f.begin();
    if (X.openL) handUp(f, K, P.hLx, P.hLy); else paintHand(f, K, P.hLx, P.hLy);
    f.contour(0.7f, 0.25f, armLayerL);
  };
  auto drawBody = [&]() {
    f.begin();
    if (X.legs == LegStand || X.legs == LegNone) {
      paintTorso(f, B, K, P, T);
      if (K.hang != HangNone && X.legs == LegStand) {
        f.begin();
        paintSkirt(f, B, K, P, T, P.hemLag ? P.hemRx : P.fRx, P.hemLag ? P.hemLx : P.fLx);
        f.contour(0.5f, 0.12f);
      }
    } else {
      // what hangs below the hips over posture legs: the torso as a tunic (its hem at the hips), then the posture's
      // own skirt over the lap (a long garment to the ankles, a knee-length one over the thighs)
      Kit k2 = K;
      k2.skirt = false; k2.hang = HangNone;
      paintTorso(f, B, k2, P, T);
      if (K.hang != HangNone) { f.begin(); seatedSkirt(f, B, K, X, J); f.contour(0.5f, 0.12f); }
    }
    if (K.jewellery & 2) { f.begin(); paintTorc(f, B, K, P, T); }
  };
  // the head (rig_core's order): the face, the beard, the hair and its contact shadow, face paint and earrings, the
  // headwear; behind the body: the hair's back layer and the headwear's drape
  auto drawHead = [&]() {
    f.begin();
    paintFace(f, B, K, P, J.head, an);
    if (K.beard) { f.begin(); paintBeard(f, B, K, P, J.head); }
    if (!X.noHair) {
      f.begin();
      const uint8_t hairLayer = f.cur;
      paintHair(f, B, K, P, J.head, false);
      if (!headwearHides(K) && K.hair_ != (int)Hair::Bald) hairContactShadow(f, K, J.head, hairLayer);
    }
    if (K.facePaint || (K.jewellery & 1) || B.ear) { f.begin(); paintFaceMarks(f, B, K, P, J.head); }
    if (K.headwear || K.hood) { f.begin(); paintHeadwear(f, B, K, P, J.head, false); f.contour(0.55f, 0.16f); }
    hook(PropHead);
  };
  auto drawHairBack = [&]() {
    if (!X.noHair) { f.begin(); paintHair(f, B, K, P, J.head, true); f.contour(0.6f, 0.2f); }
    if (K.headwear || K.hood) { f.begin(); paintHeadwear(f, B, K, P, J.head, true); f.contour(0.55f, 0.16f); }
  };

  hook(PropBack);
  if (front) {
    drawHairBack();
    if (X.armRBack) { drawArmR(false); drawHandR(); }
    if (X.armLBack) { drawArmL(false); drawHandL(); }
    drawLegs();
    drawBody();
    hook(PropBody);
    if (!X.armRBack) drawArmR(false);
    if (!X.armLBack) drawArmL(false);
    drawHead();
    hook(PropGrip);
    hook(PropHand);
    if (!X.armLBack) drawHandL();
    if (!X.armRBack) drawHandR();
  } else if (back) {
    if (X.armRBack) { drawArmR(true); drawHandR(); }
    if (X.armLBack) { drawArmL(true); drawHandL(); }
    drawLegs();
    drawBody();
    hook(PropBody);
    if (!X.armRBack) drawArmR(false);
    if (!X.armLBack) drawArmL(false);
    drawHead();
    drawHairBack();
    hook(PropGrip);
    hook(PropHand);
    if (!X.armLBack) drawHandL();
    if (!X.armRBack) drawHandR();
  } else {
    const bool rightNear = fc == FRight;
    const bool farFront = rightNear ? P.hLfront : P.hRfront;
    if (!farFront) { if (rightNear) { drawArmL(true); drawHandL(); } else { drawArmR(true); drawHandR(); } }
    drawLegs();
    drawHairBack();
    drawBody();
    hook(PropBody);
    drawHead();
    if (farFront) { if (rightNear) { drawArmL(true); drawHandL(); } else { drawArmR(true); drawHandR(); } }
    hook(PropGrip);
    if (rightNear) drawArmR(false); else drawArmL(false);
    hook(PropHand);
    if (rightNear) drawHandR(); else drawHandL();
  }
  hook(PropTop);
  an.headX = (int16_t)J.head.x; an.headY = (int16_t)J.head.y; an.headW = 12; an.headH = (int16_t)B.headRows;
  an.handRX = (int16_t)P.hRx; an.handRY = (int16_t)P.hRy;
  an.handLX = (int16_t)P.hLx; an.handLY = (int16_t)P.hLy;
  an.hipX = (int16_t)J.cx; an.hipY = (int16_t)J.hipRow;
  if (X.legs == LegStand) {
    if (P.fRy == 0) an.footRX = (int16_t)P.fRx;
    if (P.fLy == 0) an.footLX = (int16_t)P.fLx;
  }
  (void)look;
}

}  // namespace

Canvas paintPoseRaw(const HumanLook& look, const Body& B, const Kit& K, const PoseX& X, int W, int H, int feet, RigAnchors& an) {
  Fig f(W, H, W / 2, feet);
  paintPoseInto(f, look, B, K, X, an);
  Canvas c(W, H);
  c.px = f.px;
  return c;
}

// the ink outer outline with rig_core's rule (finishFig: a pixel on a bottom- or right-facing edge is Ink, a purely top-
// or left-facing one Lit Ink), the pinholes closed with Ink, trimmed to the opaque box with the pivot kept
Cut finishPose(const Canvas& raw, int pivX, int pivY, const RigAnchors& an) {
  int x0 = raw.w, y0 = raw.h, x1 = -1, y1 = -1;
  for (int y = 0; y < raw.h; y++)
    for (int x = 0; x < raw.w; x++)
      if (raw.px[(size_t)y * raw.w + x] >> 24) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); }
  Cut out;
  if (x1 < x0) return out;
  const int bw = x1 - x0 + 1, bh = y1 - y0 + 1, w = bw + 2, h = bh + 2;
  std::vector<uint32_t> src((size_t)w * h, 0u);
  for (int y = 0; y < bh; y++) std::memcpy(&src[(size_t)(y + 1) * w + 1], &raw.px[(size_t)(y0 + y) * raw.w + x0], (size_t)bw * 4);
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
  // pinholes: transparent pixels not reached from the outside
  std::vector<uint8_t> seen((size_t)w * h, 0);
  std::vector<int> q;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++)
      if ((x == 0 || y == 0 || x == w - 1 || y == h - 1) && !(o[(size_t)y * w + x] >> 24)) { seen[(size_t)y * w + x] = 1; q.push_back(y * w + x); }
  while (!q.empty()) {
    const int i = q.back();
    q.pop_back();
    const int x = i % w, y = i / w;
    const int nx[4] = {x - 1, x + 1, x, x}, ny[4] = {y, y, y - 1, y + 1};
    for (int k = 0; k < 4; k++) {
      if (nx[k] < 0 || ny[k] < 0 || nx[k] >= w || ny[k] >= h) continue;
      const size_t j = (size_t)ny[k] * w + nx[k];
      if (!seen[j] && !(o[j] >> 24)) { seen[j] = 1; q.push_back((int)j); }
    }
  }
  for (size_t i = 0; i < seen.size(); i++) if (!seen[i] && !(o[i] >> 24)) o[i] = kInk;
  out.px = pivX - (x0 - 1);
  out.py = pivY - (y0 - 1);
  out.an = an;
  return out;
}

Cut paintPose(const HumanLook& look, const Body& B, const Kit& K, const PoseX& X, int W, int H, int feet) {
  RigAnchors an;
  const Canvas raw = paintPoseRaw(look, B, K, X, W, H, feet, an);
  return finishPose(raw, W / 2, feet, an);
}

void propStick(Fig& f, float x0, float y0, float x1, float y1, const Ramp5& r, int k) {
  const int n = (int)std::ceil(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0)));
  // the upper end of the stick is lit (whichever end is higher on the screen; ties: the left end)
  const bool firstLit = y0 < y1 || (y0 == y1 && x0 <= x1);
  for (int i = 0; i <= n; i++) {
    const float t = n ? (float)i / n : 0;
    const int x = (int)std::floor(x0 + (x1 - x0) * t + 0.5f), y = (int)std::floor(y0 + (y1 - y0) * t + 0.5f);
    const bool lit = firstLit ? i < n / 3 + 1 : i > n - n / 3 - 1;
    f.set(x, y, r[lit ? k + 1 : k]);
  }
}

// ================================================================ props in hand (pivot-relative screen coordinates)
namespace {

// a wooden tankard (3 x 4 and its handle): the rim (foam when full), the staves lit on the left, an iron hoop, the base
void mug(Fig& f, int x, int y, bool handleRight, bool foam) {
  const Ramp5 W = r5(kWood), I = r5(kIron);
  if (foam) { f.set(x, y, kWhite); f.set(x + 1, y, kWhite); f.set(x + 2, y, r5(kCloth)[4]); }
  else { f.set(x, y, W[3]); f.set(x + 1, y, W[1]); f.set(x + 2, y, W[2]); }
  for (int r = 1; r <= 3; r++) {
    const Ramp5& R = r == 2 ? I : W;
    f.set(x, y + r, R[r == 2 ? 4 : 4]); f.set(x + 1, y + r, R[r == 2 ? 3 : 2]); f.set(x + 2, y + r, R[1]);
  }
  const int hx = handleRight ? x + 3 : x - 1;
  f.set(hx, y + 1, W[2]); f.set(hx, y + 2, W[1]);
}
// a heel of bread (4 x 2): a lit crust on top
void bread(Fig& f, int x, int y) {
  const Ramp5 S = r5(kSand);
  f.set(x, y, S[4]); f.set(x + 1, y, S[3]); f.set(x + 2, y, S[3]); f.set(x + 3, y, S[2]);
  f.set(x, y + 1, S[2]); f.set(x + 1, y + 1, S[2]); f.set(x + 2, y + 1, S[1]); f.set(x + 3, y + 1, r5(kLeather)[2]);
}
// a wooden bowl (5 x 2) seen from above, a coin in it on odd frames
void bowl(Fig& f, int x, int y, bool coin) {
  const Ramp5 W = r5(kWood), D = r5(kWoodDark);
  f.set(x, y, W[4]); f.set(x + 1, y, D[0]); f.set(x + 2, y, D[0]); f.set(x + 3, y, D[1]); f.set(x + 4, y, W[2]);
  f.set(x, y + 1, W[2]); f.set(x + 1, y + 1, W[3]); f.set(x + 2, y + 1, W[2]); f.set(x + 3, y + 1, W[1]); f.set(x + 4, y + 1, W[0]);
  if (coin) { f.set(x + 2, y, r5(kGold)[4]); f.set(x + 1, y, r5(kGold)[2]); }
}
// an open book (8 x 4) held up in both hands: the pages, lines of writing, the gutter, the red cover below
void book(Fig& f, int x, int y, bool turning) {
  const Ramp5 C = r5(kCloth), R = r5(kRed);
  for (int k = 0; k < 8; k++) {
    const bool left = k < 4;
    f.set(x + k, y, left ? kWhite : C[4]);
    f.set(x + k, y + 1, left ? C[4] : C[3]);
    f.set(x + k, y + 2, left ? C[3] : C[3]);
    f.set(x + k, y + 3, R[k == 0 ? 3 : (k == 7 ? 1 : 2)]);
  }
  f.set(x + 3, y + 1, C[1]); f.set(x + 4, y + 1, C[2]); f.set(x + 3, y + 2, C[1]); f.set(x + 4, y + 2, C[2]);   // the gutter
  f.set(x + 1, y + 1, C[2]); f.set(x + 1, y + 2, C[2]); f.set(x + 6, y + 1, C[1]);                         // writing
  if (turning) { f.set(x + 4, y - 1, kWhite); f.set(x + 5, y - 1, C[4]); f.set(x + 5, y - 2, kWhite); f.set(x + 6, y - 2, C[3]); }
}
// tool heads
void hammerHead(Fig& f, int x, int y, bool horiz) {   // centred on (x, y): a 4 x 2 iron block (horiz) or 2 x 4 (upright)
  const Ramp5 I = r5(kIron);
  if (horiz) { for (int k = -2; k <= 1; k++) { f.set(x + k, y, I[k == -2 ? 4 : 3]); f.set(x + k, y + 1, I[k == 1 ? 1 : 2]); } }
  else { for (int k = -2; k <= 1; k++) { f.set(x, y + k, I[k == -2 ? 4 : 3]); f.set(x + 1, y + k, I[k == 1 ? 1 : 2]); } }
}
void hoeBlade(Fig& f, int x, int y, int dir) {   // the blade at the shaft's foot, its edge down
  const Ramp5 I = r5(kIron);
  f.set(x, y, I[3]); f.set(x + dir, y, I[3]); f.set(x + dir, y + 1, I[2]); f.set(x + 2 * dir, y + 1, I[2]); f.set(x, y + 1, I[1]);
  f.set(x - dir, y + 1, I[2]); f.set(x + dir, y + 2, I[4]); f.set(x, y + 2, I[3]); f.set(x - dir, y + 2, I[2]); f.set(x + 2 * dir, y + 2, I[1]);
}
void broomHead(Fig& f, int x, int y, int dir) {   // a besom of twigs fanning from the binding at (x, y), swept toward dir
  const Ramp5 S = r5(kThatch);
  f.set(x, y, r5(kLeather)[2]);
  for (int r = 1; r <= 4; r++) {
    const int half = 1 + r / 2;
    for (int k = -half; k <= half; k++) {
      const int idx = k <= -half + 0 ? 4 : ((k + r) & 1 ? 2 : 3);
      f.set(x + k + (r >= 3 ? dir : 0), y + r, S[r == 4 ? std::min(idx, 2) : idx]);
    }
  }
}
void flame(Fig& f, int x, int y) {
  const Ramp5 F = r5(kFire);
  f.set(x, y, F[4]); f.set(x, y - 1, F[3]); f.set(x + 1, y, F[2]); f.set(x, y - 2, F[2]);
}
// a sack over the shoulder: a lumpy burlap bag, lit top-left, tied at its neck
void sack(Fig& f, int cx, int cy, int rx, int ry) {
  const Ramp5 S = r5(ramp(rgba(184, 150, 104)));
  for (int y = cy - ry; y <= cy + ry; y++)
    for (int x = cx - rx; x <= cx + rx; x++) {
      const float u = (x + 0.5f - cx - 0.5f) / (rx + 0.5f), v = (y + 0.5f - cy - 0.5f) / (ry + 0.5f);
      const float d = u * u + v * v;
      if (d > 1.0f) continue;
      int idx = 2;
      if (u + v < -0.7f) idx = 3;
      if (u + v < -1.05f) idx = 4;
      if (u + v > 0.55f) idx = 1;
      if (((x * 3 + y * 5) & 7) == 0 && idx > 1) idx--;   // the weave's lumps
      f.set(x, y, S[idx]);
    }
  f.set(cx, cy - ry - 1, r5(kLeather)[2]);
}
// a woven basket (variant 1 of Carry) on the shoulder: the rim lit, the weave in alternating rows, a handle arched over
void basket(Fig& f, int cx, int cy) {
  const Ramp5 T = r5(kThatch), W = r5(kWood);
  for (int y = cy - 2; y <= cy + 2; y++)
    for (int x = cx - 4; x <= cx + 3; x++) {
      if (y == cy + 2 && (x == cx - 4 || x == cx + 3)) continue;
      int k = ((x + y) & 1) ? 2 : 3;
      if (x == cx - 4) k = 4;
      if (x == cx + 3) k = 1;
      if (y == cy + 2) k = std::max(0, k - 1);
      f.set(x, y, T[k]);
    }
  for (int x = cx - 4; x <= cx + 3; x++) f.set(x, cy - 3, W[x == cx - 4 ? 4 : 3]);   // the rim
  f.set(cx - 3, cy - 4, W[3]); f.set(cx - 2, cy - 5, W[3]); for (int x = cx - 1; x <= cx; x++) f.set(x, cy - 6, W[4]);
  f.set(cx + 1, cy - 5, W[2]); f.set(cx + 2, cy - 4, W[1]);
  f.set(cx - 2, cy - 4, r5(kRed)[3]); f.set(cx + 1, cy - 4, r5(kLeaf)[3]); f.set(cx, cy - 4, r5(kGold)[3]);   // apples, greens
}
// a wooden crate (variant 2 of Carry) on the shoulder: slatted, lit top and left faces, a dark gap between the slats
void crate(Fig& f, int cx, int cy) {
  const Ramp5 W = r5(kWood);
  for (int y = cy - 4; y <= cy + 3; y++)
    for (int x = cx - 4; x <= cx + 3; x++) {
      int k = 2;
      if (y <= cy - 3) k = y == cy - 4 ? 4 : 3;          // the top face
      else if (x == cx - 4) k = 3;
      else if (x == cx + 3) k = 1;
      if (y == cy && x > cx - 4 && x < cx + 3) k = 1;     // the gap between the slats
      if (y == cy + 3) k = std::max(0, k - 1);
      f.set(x, y, W[k]);
    }
  f.set(cx - 2, cy + 1, r5(kWoodDark)[1]); f.set(cx + 1, cy - 2, r5(kWoodDark)[1]);   // nail heads
}
// the linen strip a kneeler winds round a friend's wound
void linen(Fig& f, int x, int y, bool odd) {
  const Ramp5 C = r5(kCloth);
  f.set(x, y, kWhite); f.set(x + 1, y, C[3]); f.set(x + 2, y, C[2]); f.set(x + 1, y + 1, C[2]);
  if (odd) { f.set(x + 3, y - 1, C[4]); f.set(x + 2, y - 1, kWhite); }
  else f.set(x - 1, y + 1, C[3]);
}

// ---------------------------------------------------------------- instruments
// the lute family seen from the front: the bowl / body at (bx, by) (its centre), the neck running up toward (nx, ny)
void luteFront(Fig& f, int bx, int by, int nx, int ny, LeadInst li) {
  const Ramp5 W = r5(kWood), D = r5(kWoodDark);
  const bool oud = li == LeadInst::Oud;
  const int rx = oud ? 3 : 3, ry = oud ? 3 : 2;
  propStick(f, (float)bx, (float)by, (float)nx, (float)ny, D, 2);              // the neck
  propStick(f, (float)bx + 1, (float)by, (float)nx + 1, (float)ny, D, 1);
  if (oud) { f.set(nx - 1, ny - 1, D[3]); f.set(nx - 2, ny, D[2]); }          // the oud's bent-back pegbox
  else { f.set(nx, ny - 1, D[3]); f.set(nx + 1, ny - 1, D[2]); }
  for (int y = by - ry; y <= by + ry + 1; y++)
    for (int x = bx - rx; x <= bx + rx + 1; x++) {
      const float u = (x + 0.5f - bx - 0.5f) / (rx + 0.6f), v = (y + 0.5f - by - 0.5f) / (ry + 0.7f);
      if (u * u + v * v > 1.0f) continue;
      int idx = 2;
      if (u + v < -0.6f) idx = 3;
      if (u + v < -1.0f) idx = 4;
      if (u + v > 0.6f) idx = 1;
      f.set(x, y, W[idx]);
    }
  f.set(bx, by, D[0]); f.set(bx + 1, by, D[1]);   // the sound hole
  f.set(bx, by + ry, D[2]);                        // the bridge
}
// a fiddle under the chin and its bow
void fiddleFront(Fig& f, int bx, int by, int bowX0, int bowY0, int bowX1, int bowY1) {
  const Ramp5 W = r5(kWood), D = r5(kWoodDark);
  for (int y = by - 2; y <= by + 2; y++)
    for (int x = bx - 1; x <= bx + 2; x++) {
      if ((y == by - 2 || y == by + 2) && (x == bx - 1 || x == bx + 2)) continue;
      const int idx = x == bx - 1 ? 3 : (x == bx + 2 ? 1 : 2);
      f.set(x, y, W[y == by ? idx - 1 : idx]);
    }
  propStick(f, (float)bx + 1, (float)by - 3, (float)bx + 3, (float)by - 6, D, 2);
  propStick(f, (float)bowX0, (float)bowY0, (float)bowX1, (float)bowY1, r5(kCloth), 3);
}
// a small frame harp held upright at the chest: the pillar, the neck, the strings
void harpFront(Fig& f, int x, int y, bool odd) {
  const Ramp5 W = r5(kWood), B = r5(kBrass);
  propStick(f, (float)x, (float)y - 6, (float)x, (float)y + 2, W, 2);            // the pillar
  propStick(f, (float)x, (float)y - 6, (float)x + 5, (float)y - 3, W, 2);        // the neck
  propStick(f, (float)x + 5, (float)y - 3, (float)x + 2, (float)y + 2, W, 1);    // the soundboard
  for (int k = 1; k <= 3; k++) for (int r = y - 5 + k; r <= y + 1; r++) if ((r + k + (odd ? 1 : 0)) % 3 != 0) f.set(x + k, r, B[3]);
}
// a transverse flute / a reed / a horn from the mouth
void windFront(Fig& f, LeadInst li, int mx, int my, int dir, bool odd) {
  const Ramp5 W = r5(kWood), D = r5(kWoodDark), B = r5(kBrass), I = r5(kIron);
  switch (li) {
    case LeadInst::Pipes: {   // pan pipes: a row of tubes under the lips
      for (int k = 0; k < 5; k++) for (int r = 0; r <= 4 - k / 2; r++) f.set(mx - 2 + k, my + r, W[r == 0 ? 4 : (k & 1 ? 2 : 3)]);
      f.set(mx - 2, my + 2, r5(kLeather)[2]); f.set(mx + 2, my + 2, r5(kLeather)[1]);
      break;
    }
    case LeadInst::Reed: {    // a shawm: straight down from the mouth to its flared bell
      propStick(f, (float)mx, (float)my, (float)mx, (float)my + 7, D, 2);
      f.set(mx - 1, my + 8, D[3]); f.set(mx, my + 8, D[2]); f.set(mx + 1, my + 8, D[1]);
      f.set(mx - 1, my + 9, D[2]); f.set(mx, my + 9, D[1]); f.set(mx + 1, my + 9, D[0]); f.set(mx + 2, my + 9, D[0]);
      break;
    }
    case LeadInst::Horn: case LeadInst::Brass: {   // a curved horn raised from the mouth, the bell up to one side
      const Ramp5& R = li == LeadInst::Brass ? B : r5(kBone);
      propStick(f, (float)mx, (float)my, (float)mx + 3 * dir, (float)my - 2, R, 2);
      propStick(f, (float)mx + 3 * dir, (float)my - 2, (float)mx + 5 * dir, (float)my - 6, R, 2);
      for (int k = -1; k <= 1; k++) f.set(mx + 5 * dir + k, my - 7, R[k < 0 ? 4 : (k == 0 ? 3 : 1)]);
      f.set(mx + 5 * dir, my - 8, R[2]);
      break;
    }
    case LeadInst::Bells: {   // hand bells, one up and one down by turns
      const int hy = odd ? my + 6 : my + 9;
      for (int k = 0; k < 3; k++) { f.set(mx - 7 + k, hy + (k == 1 ? 0 : 1), B[k == 0 ? 4 : 3 - k]); f.set(mx + 5 + k, (odd ? my + 9 : my + 6) + (k == 1 ? 0 : 1), B[k == 0 ? 4 : 3 - k]); }
      f.set(mx - 6, hy + 2, I[2]); f.set(mx + 6, (odd ? my + 9 : my + 6) + 2, I[2]);
      break;
    }
    case LeadInst::Voice: break;   // the voice: nothing in hand (a hand on the heart)
    default: {                     // the transverse flute: from the lips out to the player's right
      propStick(f, (float)mx, (float)my + 1, (float)mx + 9 * dir, (float)my + 2, W, 3);
      f.set(mx + 2 * dir, my + 1, D[0]); f.set(mx + 5 * dir, my + 2, D[0]); f.set(mx + 7 * dir, my + 2, D[0]);   // finger holes
      break;
    }
  }
}
// the drum family before the body
void drumFront(Fig& f, PercKind pk, int x, int y, bool odd) {
  const Ramp5 W = r5(kWood), H = r5(kCloth), L = r5(kLeather), B = r5(kBrass), D = r5(kWoodDark);
  auto ellipse = [&](int cx, int cy, int rx, int ry, const Ramp5& R, bool face) {
    for (int yy = cy - ry; yy <= cy + ry; yy++)
      for (int xx = cx - rx; xx <= cx + rx; xx++) {
        const float u = (xx - cx) / (rx + 0.5f), v = (yy - cy) / (ry + 0.5f);
        if (u * u + v * v > 1.0f) continue;
        int idx = face ? 3 : 2;
        if (u + v < -0.7f) idx = 4;
        if (u + v > 0.6f) idx = face ? 2 : 1;
        if (u * u + v * v > 0.62f && face) idx = std::max(1, idx - 2);   // the rim
        f.set(xx, yy, R[idx]);
      }
  };
  switch (pk) {
    case PercKind::Taiko: case PercKind::Kettle: {   // a barrel drum on a strap at the belly, struck with two sticks
      for (int yy = y; yy <= y + 4; yy++) for (int xx = x - 4; xx <= x + 3; xx++) f.set(xx, yy, W[xx == x - 4 ? 3 : (xx == x + 3 ? 1 : (yy == y + 2 ? 1 : 2))]);
      ellipse(x, y - 1, 4, 1, H, true);
      propStick(f, (float)x - 3, (float)y - 2 - (odd ? 3 : 0), (float)x - 6, (float)y - 6 - (odd ? 2 : 0), D, 3);
      propStick(f, (float)x + 2, (float)y - 2 - (odd ? 0 : 3), (float)x + 5, (float)y - 6 - (odd ? 0 : 2), D, 3);
      break;
    }
    case PercKind::Hand: case PercKind::Tabla: {     // a pair of small hand drums at the waist
      ellipse(x - 3, y, 2, 1, H, true);
      ellipse(x + 3, y, 2, 1, L, true);
      for (int k = -2; k <= 2; k++) { f.set(x - 3 + k, y + 2, W[k < 0 ? 3 : 1]); f.set(x + 3 + k, y + 2, W[k < 0 ? 3 : 1]); }
      break;
    }
    case PercKind::Gong: {                           // a small gong on its cord, a padded mallet
      ellipse(x + 2, y - 2, 3, 3, B, true);
      f.set(x + 2, y - 2, B[4]);
      propStick(f, (float)x - 4, (float)y - (odd ? 4 : 1), (float)x - 2, (float)y - (odd ? 6 : 3), D, 2);
      break;
    }
    case PercKind::Wood: {                           // two wooden clappers
      const int g = odd ? 0 : 1;
      for (int k = 0; k < 4; k++) { f.set(x - 3 + k, y - g, W[k == 0 ? 4 : 2]); f.set(x - 3 + k, y + 1 + g, W[k == 0 ? 3 : 1]); }
      break;
    }
    case PercKind::Bells: {                          // a jingle ring shaken high
      ellipse(x, y - 3 - (odd ? 1 : 0), 3, 2, L, false);
      for (int k = 0; k < 4; k++) f.set(x - 3 + k * 2, y - 3 - (odd ? 1 : 0) + (k & 1 ? 2 : -2), B[4]);
      break;
    }
    default: {                                       // the frame drum / bodhran held in the off hand, struck by the other
      ellipse(x + 2, y - 1, 4, 3, H, true);
      if (pk == PercKind::Bodhran) propStick(f, (float)x - 4, (float)y + (odd ? -3 : 1), (float)x - 1, (float)y + (odd ? -5 : -1), D, 2);
      break;
    }
  }
}

}  // namespace

// ================================================================ the 31 postures
namespace {

struct PzCtx { Posture pz; };

inline int mxF(int x) { return -2 - x; }   // a fist's top-left mirrored about the body's centre
inline int mxC(int x) { return -1 - x; }   // a column mirrored

// facing left: the right-facing pose mirrored (the near hand is the left one: a civilian's prop stays in the near hand)
PoseX mirrorSide(const PoseX& r) {
  PoseX l = r;
  Pose& p = l.p;
  const Pose& q = r.p;
  p.facing = FLeft;
  p.hLx = (int8_t)mxF(q.hRx); p.hLy = q.hRy;
  p.hRx = (int8_t)mxF(q.hLx); p.hRy = q.hLy;
  p.hRfront = q.hLfront; p.hLfront = 1;
  p.fLx = (int8_t)mxC(q.fRx); p.fLy = q.fRy;
  p.fRx = (int8_t)mxC(q.fLx); p.fRy = q.fLy;
  p.kL = q.kR; p.kR = q.kL;
  p.lean = (int8_t)-q.lean; p.headX = (int8_t)-q.headX; p.hipX = (int8_t)-q.hipX;
  std::swap(l.openR, l.openL);
  std::swap(l.hideHandR, l.hideHandL);
  return l;
}
// facing away: the facing-down pose mirrored (the right hand on the screen's right); an arm whose hand is before the
// chest goes behind the body
PoseX mirrorUp(const PoseX& d, const Body& B) {
  PoseX u = d;
  Pose& p = u.p;
  const Pose& q = d.p;
  p.facing = FUp;
  p.hRx = (int8_t)mxF(q.hRx); p.hLx = (int8_t)mxF(q.hLx);
  p.fRx = (int8_t)mxC(q.fRx); p.fLx = (int8_t)mxC(q.fLx);
  p.lean = (int8_t)-q.lean; p.headX = (int8_t)-q.headX; p.hipX = (int8_t)-q.hipX;
  auto inFront = [&](int hx, int hy) {
    const int c = p.hipX + p.lean;
    return hx + 1 >= c - B.torsoHalf && hx <= c + B.torsoHalf - 1 && hy >= B.shoulder + q.dy - 1 && hy <= B.hip + q.dy + 3;
  };
  u.armRBack = d.armRBack || inFront(p.hRx, p.hRy);
  u.armLBack = d.armLBack || inFront(p.hLx, p.hLy);
  return u;
}

// rows of the body (pivot-relative) at the pose's dy
struct Rows {
  int S, H, T, M;   // shoulder, hips, the head's top painted row, the mouth
};
Rows rowsOf(const Body& B, int dy, int chest = 0) {
  Rows r;
  r.S = B.shoulder + dy + chest; r.H = B.hip + dy; r.T = B.headTop + dy + chest; r.M = r.T + 8;
  return r;
}

// the dy that puts the hips' row on hipRow
int dyFor(const Body& B, int hipRow) { return hipRow - B.hip; }

// the props of a posture frame, painted on the layer they belong to (front views pass the down-view coordinates
// mirrored for the back view: everything a back view holds is behind the body, so it shows only where it clears it)
void pzProps(Fig& f, const Body& B, const Kit& K, const PoseX& X, int layer, const void* ctx) {
  const Posture pz = ((const PzCtx*)ctx)->pz;
  const int fc = X.p.facing, fr = X.fr;
  const bool odd = (fr & 1) != 0;
  const bool up = fc == FUp, sideV = fc == FRight || fc == FLeft;
  const int s = fc == FLeft ? -1 : 1;
  // what layer a held prop goes on: in front of the hands' arms (front), behind everything (back view), between the far
  // and near arms (side)
  const int grip = up ? PropBack : PropGrip;
  const Rows R = rowsOf(B, X.p.dy, X.p.chest);
  const Pose& P = X.p;
  auto mx = [&](int x) { return (up || fc == FLeft) ? -1 - x : x; };   // authoring coordinates -> this facing's screen
  const Ramp5 Wd = r5(kWood), Wk = r5(kWoodDark), Ir = r5(kIron), Lt = r5(kLeather);
  // the near / prop hand's fist (top-left) in screen coordinates
  const int hx = sideV ? (fc == FRight ? P.hRx : P.hLx) : P.hRx, hy = sideV ? (fc == FRight ? P.hRy : P.hLy) : P.hRy;
  const int ox = sideV ? (fc == FRight ? P.hLx : P.hRx) : P.hLx, oy = sideV ? (fc == FRight ? P.hLy : P.hRy) : P.hLy;
  (void)K; (void)ox; (void)oy;
  switch (pz) {
    case Posture::Eat: case Posture::SitEat: case Posture::SitFloorEat:
      if (layer != grip) return;
      bread(f, sideV ? (s > 0 ? hx + 1 : hx - 3) : (up ? hx - 2 : hx), hy - 2 + (sideV ? 0 : 0));
      return;
    case Posture::Drink: case Posture::SitDrink: case Posture::SitFloorDrink:
      if (layer != grip) return;
      if (sideV) mug(f, s > 0 ? hx + 1 : hx - 2, hy - 2, s < 0, !odd);
      else mug(f, up ? hx : hx, hy - 3, !up, !odd);
      return;
    case Posture::Cheer:
      if (layer != (up ? PropBack : PropHand)) return;
      if (sideV) mug(f, s > 0 ? hx + 1 : hx - 2, hy - 3, s < 0, true);
      else mug(f, hx, hy - 3, !up, true);
      if (odd) { const int sx = sideV ? (s > 0 ? hx + 2 : hx - 1) : hx + 1; f.set(sx, hy - 5, kWhite); f.set(sx + (sideV ? s : 1), hy - 6, kWhite); }   // the toast's splash
      return;
    case Posture::Hammer: {
      if (layer != grip) return;
      // the haft from the fist; raised (frame 0) it points up and back, struck (frame 1) it points down at the anvil
      if (sideV) {
        if (!odd) { propStick(f, (float)hx + 1, (float)hy, (float)hx + 1 - 3 * s, (float)hy - 5, Wd, 2); hammerHead(f, hx + 1 - 4 * s, hy - 6, true); }
        else { propStick(f, (float)hx + 1, (float)hy + 1, (float)hx + 1 + 5 * s, (float)hy + 3, Wd, 2); hammerHead(f, hx + 1 + 6 * s, hy + 2, false); }
      } else {
        if (!odd) { propStick(f, (float)hx + 1, (float)hy, (float)hx + 1, (float)hy - 5, Wd, 2); hammerHead(f, hx + 1, hy - 6, true); }
        else { propStick(f, (float)hx + 1, (float)hy + 1, (float)hx + 1 + (up ? -4 : 4), (float)hy + 3, Wd, 2); hammerHead(f, hx + 1 + (up ? -5 : 5), hy + 3, true); }
      }
      return;
    }
    case Posture::Chop: {
      // the woodcutter's axe (RIG-ARMS' bearded axe, art id 2): raised over the shoulder, then bitten down into the block
      if (layer != grip) return;
      Kit ka = K;
      ka.metal = r5(ramp(rgba(170, 176, 186), 1.1f));
      int tx, ty, dir, edge;
      if (sideV) { dir = !odd ? (s > 0 ? 10 : 6) : (s > 0 ? 2 : 6 - 0); edge = s; if (odd && s < 0) dir = 6; }
      else { dir = !odd ? (up ? 13 : 11) : 4; edge = up ? 1 : -1; }
      paintArms(f, ka, 2, hx + 1.0f - 0.5f, hy + 1.0f - 0.5f, dir, sideV ? 0 : (odd ? 3 : 1), edge, tx, ty);
      return;
    }
    case Posture::Hoe: {
      if (layer != grip) return;
      if (sideV) {
        if (!odd) { propStick(f, (float)hx + 1, (float)hy + 1, (float)hx + 1 - 5 * s, (float)hy - 10, Wd, 2); hoeBlade(f, hx + 1 - 5 * s, hy - 11, -s); }
        else { propStick(f, (float)hx + 1 - 3 * s, (float)hy - 2, (float)hx + 1 + 8 * s, -2.0f, Wd, 2); hoeBlade(f, hx + 1 + 9 * s, -3, s); }
      } else {
        const int d = up ? 1 : -1;
        if (!odd) { propStick(f, (float)hx + 1, (float)hy + 2, (float)hx + 1 - 2 * d, (float)hy - 10, Wd, 2); hoeBlade(f, hx + 1 - 2 * d, hy - 11, d); }
        else { propStick(f, (float)hx + 1, (float)hy - 3, (float)hx + 1 - 2 * d, 1.0f, Wd, 2); hoeBlade(f, hx + 1 - 2 * d, 1, -d); }
      }
      return;
    }
    case Posture::Sweep: {
      if (layer != grip) return;
      // the broom's top over the hands, its foot swinging out to one side and back
      const int dir = odd ? -1 : 1;
      if (sideV) {
        const int footX = hx + 1 + (odd ? 1 : 6) * s;
        propStick(f, (float)hx + 1 - 3 * s, (float)hy - 6, (float)footX, -5.0f, Wd, 2);
        broomHead(f, footX, -5, odd ? -s : s);
      } else {
        const int d = up ? -dir : dir;
        const int topX = hx + 1 - 2 * d, footX = hx + 1 + 5 * d;
        propStick(f, (float)topX, (float)hy - 6, (float)footX, -4.0f, Wd, 2);
        broomHead(f, footX, -4, d);
      }
      return;
    }
    case Posture::Stir: {
      if (layer != grip) return;
      static const int dx[4] = {-1, 0, 1, 0}, dyy[4] = {0, 1, 0, -1};
      const int k = fr & 3;
      if (sideV) {
        const int lx = hx + 1 + 4 * s + dx[k] * s, ly = hy + 6 + dyy[k];
        propStick(f, (float)hx + 1, (float)hy, (float)lx, (float)ly, Wd, 3);
        f.set(lx, ly + 1, Ir[3]); f.set(lx + s, ly + 1, Ir[2]);
      } else {
        const int lx = hx + 1 + (up ? -2 : 2) + dx[k], ly = hy + 6 + dyy[k];
        propStick(f, (float)hx + 1, (float)hy, (float)lx, (float)ly, Wd, 3);
        f.set(lx, ly + 1, Ir[3]); f.set(lx + 1, ly + 1, Ir[2]);
      }
      return;
    }
    case Posture::Carry: {
      // the sack on the near shoulder, steadied by the hand above it
      auto load = [&](int x, int y) {
        if (X.var == 1) basket(f, x, y);
        else if (X.var == 2) crate(f, x, y);
        else sack(f, x, y, 4, 3);
      };
      if (sideV) { if (layer == PropBody) load(s > 0 ? -4 : 3, R.S - 2); return; }   // behind the head, over the shoulder
      if (up) { if (layer == PropHand) load(mx(-5), R.S - 2); return; }
      if (layer == PropBody) load(-5, R.S - 2);
      return;
    }
    case Posture::Fish: {
      // the rod held out over the water, its tip dipping on odd frames; the line hangs from the tip
      const int tipY = R.S - 6 + (odd ? 2 : 0);
      if (sideV) {
        if (layer == PropBack) for (int y = tipY + 1; y <= 4; y++) if ((y + (odd ? 1 : 0)) % 5 != 0) f.set(hx + 1 + 13 * s, y, withA(kWhite, 150));
        if (layer == PropGrip) propStick(f, (float)hx + 1, (float)hy + 1, (float)hx + 1 + 13 * s, (float)tipY, Wd, 3);
        return;
      }
      const int tx = up ? hx + 1 - 7 : hx + 1 - 7;
      if (layer == PropBack) for (int y = tipY + 1; y <= (up ? R.S : 3); y++) if ((y + (odd ? 1 : 0)) % 5 != 0) f.set(tx, y, withA(kWhite, 150));
      if (layer == grip) propStick(f, (float)hx + 1, (float)hy + 1, (float)tx, (float)tipY, Wd, 3);
      return;
    }
    case Posture::Lamp: {
      // the lamplighter's pole raised to a lamp: a brass hook and a taper's flame at its top
      const int lift = odd ? 1 : 0;
      int tx, ty;
      if (sideV) { tx = hx + 1 + 6 * s; ty = R.T - 6 - lift; }
      else { tx = hx + 1 + (up ? 4 : -4); ty = R.T - 6 - lift; }
      if (layer == grip) {
        propStick(f, (float)hx + 1, (float)hy + 4, (float)tx, (float)ty, Wd, 2);
        f.set(tx, ty - 1, r5(kBrass)[4]); f.set(tx + (sideV ? s : (up ? 1 : -1)), ty - 1, r5(kBrass)[2]);
      }
      if (layer == PropTop) flame(f, tx + (sideV ? s : (up ? 1 : -1)), ty - 2);
      return;
    }
    case Posture::Read:
      if (layer != grip) return;
      if (sideV) {   // the book held open before the chest, seen edge-on: the pages' top edge and the cover
        const int x0 = s > 0 ? hx : hx - 4;
        for (int k = 0; k < 5; k++) { f.set(x0 + k, hy - 2, k == (s > 0 ? 0 : 4) ? kWhite : r5(kCloth)[3]); f.set(x0 + k, hy - 1, r5(kRed)[k == 0 ? 3 : 1]); }
        if (odd) f.set(x0 + 2, hy - 3, kWhite);
      } else book(f, up ? -4 : -4, hy - 2, odd);
      return;
    case Posture::Beg:
      if (layer != grip) return;
      if (sideV) bowl(f, s > 0 ? hx + 1 : hx - 4, hy - 1, odd);
      else bowl(f, up ? hx - 3 : hx - 1, hy - 1, odd);
      return;
    case Posture::Kneel:
      if (X.var == 1 || layer != grip) return;   // (mourning: nothing in hand)
      if (sideV) linen(f, s > 0 ? hx + 1 : hx - 3, hy + 1, odd);
      else linen(f, up ? -2 : -2, hy + 2, odd);
      return;
    case Posture::Downed:
      if (layer != PropTop || up) return;
      // the wound under the pressed hand: a dark red stain spreading from under the fingers
      if (sideV) { f.set(ox + (s > 0 ? -1 : 2), oy + 1, r5(kRed)[1]); f.set(ox + (s > 0 ? 0 : 1), oy + 2, r5(kRed)[0]); }
      else { f.set(ox - 1, oy + 1, r5(kRed)[1]); f.set(ox + 2, oy, r5(kRed)[1]); f.set(ox + 2, oy + 1, r5(kRed)[0]); }
      return;
    case Posture::Ride: {
      // the reins: from the fists down and forward to the horse's bit
      if (layer != (up ? PropBack : PropTop)) return;
      if (sideV) { propStick(f, (float)hx + 2 * s, (float)hy + 1, (float)hx + 6 * s, (float)hy + 4, Lt, 1); return; }
      propStick(f, (float)P.hRx + 1, (float)P.hRy + 2, (float)P.hRx + 1, (float)P.hRy + 5, Lt, 1);
      propStick(f, (float)P.hLx, (float)P.hLy + 2, (float)P.hLx, (float)P.hLy + 5, Lt, 1);
      return;
    }
    case Posture::Lute: {
      const LeadInst li = (LeadInst)X.var;
      if (sideV) {   // side-on: the body before the belly, the neck forward and up
        if (layer != PropGrip) return;
        if (li == LeadInst::Fiddle) { fiddleFront(f, s > 0 ? 3 : -5, R.S + 2, (s > 0 ? -2 : 1) + (odd ? s : 0), R.S + 5, (s > 0 ? 6 : -7) + (odd ? s : 0), R.S + 1); return; }
        if (li == LeadInst::Harp) { harpFront(f, s > 0 ? 2 : -7, R.S + 7, odd); return; }
        luteFront(f, s > 0 ? 0 : -2, R.S + 7, s > 0 ? 8 : -9, R.S + 2, li);
        return;
      }
      if (layer != grip) return;
      if (li == LeadInst::Fiddle) { fiddleFront(f, mx(3), R.S + 2, mx(-5 + (odd ? 2 : 0)), R.S + 6, mx(3 + (odd ? 2 : 0)), R.S + 5); return; }
      if (li == LeadInst::Harp) { harpFront(f, up ? mx(5) : -3, R.S + 8, odd); return; }
      luteFront(f, mx(-3), R.S + 8, mx(5), R.S + 1, li);
      return;
    }
    case Posture::Flute: {
      const LeadInst li = (LeadInst)X.var;
      if (li == LeadInst::Voice) return;
      if (sideV) {
        if (layer != PropGrip) return;
        const int mxS = s > 0 ? 5 : -6, my = R.M;
        if (li == LeadInst::Pipes) { for (int k = 0; k < 4; k++) for (int r = 0; r <= 3 - k / 2; r++) f.set(mxS + k * s, my + r, Wd[r == 0 ? 4 : 2]); return; }
        if (li == LeadInst::Reed) { propStick(f, (float)mxS, (float)my + 1, (float)mxS + 4 * s, (float)my + 8, Wk, 2); f.set(mxS + 5 * s, my + 9, Wk[2]); f.set(mxS + 4 * s, my + 9, Wk[3]); return; }
        if (li == LeadInst::Horn || li == LeadInst::Brass) { windFront(f, li, mxS, my, s, odd); return; }
        if (li == LeadInst::Bells) { windFront(f, li, s > 0 ? 3 : -4, my, s, odd); return; }
        propStick(f, (float)mxS, (float)my + 1, (float)mxS + 8 * s, (float)my + 2, Wd, 3);   // the flute to the side
        return;
      }
      if (layer != grip) return;
      windFront(f, li, mx(-1), R.M, up ? 1 : -1, odd);
      return;
    }
    case Posture::Drum: {
      const PercKind pk = (PercKind)X.var;
      if (sideV) {
        if (layer != PropGrip) return;
        drumFront(f, pk, s > 0 ? 3 : -4, R.S + 7, odd);
        return;
      }
      if (layer != grip) return;
      drumFront(f, pk, mx(0), R.S + 7, odd);
      return;
    }
    default: return;
  }
}

// ---------------------------------------------------------------- the pose of a posture frame
// Authored for the man (hands, rows and heights read the Body, so the woman, the elf and kids follow); the down and the
// right facings are painted from these keys, the up facing from the down keys mirrored (mirrorUp), the left from the
// right (mirrorSide).
PoseX pzDown(const Body& B, Posture pz, uint8_t var, int fr);
PoseX pzRight(const Body& B, Posture pz, uint8_t var, int fr);

PoseX baseX(const Body& B, int fc, uint8_t legs, int hipRow) {
  PoseX X;
  X.p = restPose(B, fc, false);
  X.legs = legs;
  if (legs != LegStand) X.p.dy = (int8_t)dyFor(B, hipRow);
  return X;
}
uint8_t legsOf(Posture pz) {
  switch (pz) {
    case Posture::Sit: case Posture::SitEat: case Posture::SitDrink: return LegChair;
    case Posture::SitFloor: case Posture::SitFloorEat: case Posture::SitFloorDrink: case Posture::Beg: return LegFloor;
    case Posture::Kneel: case Posture::Downed: return LegKneel;
    case Posture::Ride: return LegRide;
    default: return LegStand;
  }
}
int hipOf(const Body& B, uint8_t legs) {
  switch (legs) {
    case LegChair: return kChairHip;
    case LegFloor: return -2;
    case LegKneel: return -5;
    default: return B.hip;
  }
}

// facing down (the right hand on the screen's left: the prop hand)
PoseX pzDown(const Body& B, Posture pz, uint8_t var, int fr) {
  const uint8_t legs = legsOf(pz);
  PoseX X = baseX(B, FDown, legs, hipOf(B, legs));
  Pose& p = X.p;
  const bool odd = (fr & 1) != 0;
  const Rows R = rowsOf(B, p.dy);
  const int sh = B.shoulderHalf;
  auto R_ = [&](int x, int y) { p.hRx = (int8_t)x; p.hRy = (int8_t)y; };
  auto L_ = [&](int x, int y) { p.hLx = (int8_t)x; p.hLy = (int8_t)y; };
  const int lapY = R.H + 1;   // a seated / kneeling body's lap row
  // the free hand: at the side standing, on the knee seated, on the shin cross-legged
  auto offLap = [&]() {
    if (legs == LegStand) L_(sh - 1, R.H + 1);
    else if (legs == LegChair) L_(2, lapY + 1);
    else L_(sh - 3, lapY);
  };
  switch (pz) {
    case Posture::Sit: case Posture::SitFloor:
      if (legs == LegChair) { R_(-4, lapY + 1); L_(2, lapY + 1); }   // the hands rest on the knees
      else { R_(-sh + 1, lapY); L_(sh - 3, lapY); }
      break;
    case Posture::Eat: case Posture::SitEat: case Posture::SitFloorEat:
      offLap();
      if (!odd) R_(-4, R.S + 5); else { R_(-3, R.M + 2); p.headY = 0; }
      break;
    case Posture::Drink: case Posture::SitDrink: case Posture::SitFloorDrink:
      offLap();
      if (!odd) R_(-4, R.S + 6); else { R_(-3, R.M + 3); }
      break;
    case Posture::Cheer:
      L_(sh - 1, R.H + 1);
      if (!odd) R_(-sh - 2, R.T + 2); else { R_(-sh - 1, R.T - 1); L_(sh, R.S + 4); }
      p.elbowOut = 1;
      break;
    case Posture::Wave:
      L_(sh - 1, R.H + 1);
      R_(-sh - (odd ? 3 : 2), R.T + 2 - (odd ? 0 : 1));
      X.openR = true;
      p.elbowOut = 1;
      break;
    case Posture::Hammer:
      L_(sh - 3, R.S + 6);   // the off hand at the work (the tongs on the anvil)
      if (!odd) { R_(-sh - 1, R.T + 4); p.elbowOut = 1; } else { R_(-3, R.S + 7); p.dy = 1; }
      break;
    case Posture::Chop:
      if (!odd) { R_(-4, R.T + 3); L_(-2, R.T + 5); p.elbowOut = 1; p.headY = -1; }
      else { R_(-1, R.S + 8); L_(1, R.S + 6); p.dy = 1; }
      break;
    case Posture::Hoe:
      if (!odd) { R_(-4, R.T + 4); L_(-1, R.S + 4); p.elbowOut = 1; }
      else { R_(-4, R.S + 8); L_(-1, R.S + 5); p.dy = 1; }
      break;
    case Posture::Sweep:
      if (!odd) { R_(-5, R.S + 3); L_(-2, R.S + 7); } else { R_(2, R.S + 3); L_(-1, R.S + 7); }
      break;
    case Posture::Stir: {
      static const int dx[4] = {-1, 0, 1, 0};
      R_(-3 + dx[fr & 3], R.S + 6); L_(sh - 2, R.S + 7);
      break;
    }
    case Posture::Carry: {
      // walking with the sack on the right shoulder: the hand up steadying it, the free arm swinging
      static const int8_t liftA[4] = {0, 1, 0, 0}, liftB[4] = {0, 0, 0, 1}, sw[4] = {0, 1, 0, -1};
      p.fRy = liftA[fr & 3]; p.fLy = liftB[fr & 3];
      p.dy = (int8_t)((fr & 1) ? 0 : 1);
      const Rows Rw = rowsOf(B, p.dy);
      R_(-sh + 1, Rw.S - 2);
      L_(sh - 1 + sw[fr & 3], Rw.H + 1 - std::abs(sw[fr & 3]));
      break;
    }
    case Posture::Fish:
      R_(-5, R.S + 5); L_(-2, R.S + 6);
      break;
    case Posture::Pray:
      R_(-2, R.S + 4); L_(0, R.S + 4);
      X.hideHandR = X.hideHandL = true;   // (the joined hands are painted as one by the hook)
      p.headY = (int8_t)(odd ? 2 : 1); p.expr = ExClosed;
      break;
    case Posture::Lamp:
      R_(-sh, R.S - 1 - (odd ? 1 : 0)); L_(-2, R.S + 4);
      p.elbowOut = 1; p.headY = -1;
      break;
    case Posture::Read:
      R_(-5, R.S + 5); L_(3, R.S + 5);
      p.headY = 1;
      break;
    case Posture::Play: {
      // tag: running with both arms up, a hop every other frame
      static const int8_t liftA[4] = {2, 0, 0, 0}, liftB[4] = {0, 0, 2, 0};
      p.fRy = liftA[fr & 3]; p.fLy = liftB[fr & 3];
      p.dy = (int8_t)-kinHop(B, fr);
      const Rows Rw = rowsOf(B, p.dy);
      R_(-sh - 1, Rw.T + 3 + (odd ? 0 : 2)); L_(sh - 1, Rw.T + 3 + (odd ? 2 : 0));
      X.openR = X.openL = true; p.elbowOut = 1;
      break;
    }
    case Posture::Dance: {
      static const int8_t liftA[4] = {2, 0, 0, 0}, liftB[4] = {0, 0, 2, 0}, bob[4] = {-1, 0, -1, 0}, hip[4] = {-1, 0, 1, 0};
      const int k = fr & 3;
      p.fRy = liftA[k]; p.fLy = liftB[k];
      p.dy = bob[k]; p.hipX = hip[k];
      const Rows Rw = rowsOf(B, p.dy);
      if (k == 0) { R_(-sh - 2, Rw.T + 3); L_(sh - 3, Rw.H - 1); X.openR = true; p.elbowOut = 1; }        // a hand up, the other on the hip
      else if (k == 2) { L_(sh, Rw.T + 3); R_(-sh + 1, Rw.H - 1); X.openL = true; p.elbowOut = 1; }
      else { R_(-sh - 3, Rw.S + 3); L_(sh + 1, Rw.S + 3); }                                               // arms out, turning
      break;
    }
    case Posture::Lute: {
      const LeadInst li = (LeadInst)var;
      if (li == LeadInst::Fiddle) { R_(-5 + (odd ? 2 : 0), R.S + 6); L_(sh - 1, R.S + 1); p.headX = 1; }
      else if (li == LeadInst::Harp) { R_(-4, R.S + 5 + (odd ? 1 : 0)); L_(1, R.S + 3 + (odd ? 0 : 1)); }
      else { R_(-4 + (odd ? 1 : 0), R.S + 8); L_(sh - 2, R.S + 2); }
      break;
    }
    case Posture::Flute: {
      const LeadInst li = (LeadInst)var;
      if (li == LeadInst::Voice) { R_(-3, R.S + 3); L_(sh, R.S + 4 - (odd ? 1 : 0)); X.openL = true; p.expr = odd ? ExNeutral : ExEffort; }
      else if (li == LeadInst::Pipes) { R_(-4, R.M + 2); L_(1, R.M + 2); }
      else if (li == LeadInst::Reed) { R_(-2, R.M + 4); L_(-2, R.M + 7); }
      else if (li == LeadInst::Horn || li == LeadInst::Brass) { R_(-3, R.M + 1); L_(-sh, R.M - 3); p.elbowOut = 1; }
      else if (li == LeadInst::Bells) { R_(-sh - 2, R.M + (odd ? 5 : 8)); L_(sh, R.M + (odd ? 8 : 5)); }
      else { R_(-6, R.M + 2); L_(-3, R.M + 2); }
      break;
    }
    case Posture::Drum: {
      const PercKind pk = (PercKind)var;
      if (pk == PercKind::Taiko || pk == PercKind::Kettle) { R_(-5, R.S + 4 - (odd ? 3 : 0)); L_(3, R.S + 4 - (odd ? 0 : 3)); }
      else if (pk == PercKind::Hand || pk == PercKind::Tabla) { R_(-5, R.S + 6 - (odd ? 1 : 0)); L_(3, R.S + 6 - (odd ? 0 : 1)); }
      else if (pk == PercKind::Wood) { R_(-3, R.S + 6); L_(0, R.S + 6); }
      else if (pk == PercKind::Bells) { R_(-1, R.S + 3 - (odd ? 1 : 0)); L_(sh - 1, R.H); }
      else { R_(-5, R.S + 6 - (odd ? 2 : 0)); L_(4, R.S + 5); }
      break;
    }
    case Posture::Beg:
      R_(-sh + 2, lapY); L_(-1, R.S + 7 - (odd ? 1 : 0));
      p.headY = 1;
      break;
    case Posture::Downed:
      // slumped on one knee: braced on the right hand beside the knee, the left pressed to the wound
      R_(-sh - 2, -2); L_(1, R.S + 5);
      p.lean = 0; p.headY = (int8_t)(2 + (odd ? 1 : 0)); p.expr = ExClosed; p.dy = (int8_t)(p.dy + 1);
      break;
    case Posture::Kneel:
      if (var == 1) { R_(-4, lapY - 1); L_(sh - 3, lapY + (odd ? 1 : 0)); p.headY = 2; p.expr = ExClosed; }   // mourning
      else { R_(odd ? -4 : -3, lapY + 2 - (odd ? 1 : 0)); L_(odd ? 1 : 0, lapY + 2); p.headY = 1; }        // binding a wound
      break;
    case Posture::Ride:
      R_(-4, R.S + 7); L_(2, R.S + 7);
      break;
    default: break;
  }
  return X;
}

// facing right (the right hand is the near hand)
PoseX pzRight(const Body& B, Posture pz, uint8_t var, int fr) {
  const uint8_t legs = legsOf(pz);
  PoseX X = baseX(B, FRight, legs, hipOf(B, legs));
  Pose& p = X.p;
  const bool odd = (fr & 1) != 0;
  const Rows R = rowsOf(B, p.dy);
  auto N_ = [&](int x, int y) { p.hRx = (int8_t)x; p.hRy = (int8_t)y; };   // the near hand
  auto F_ = [&](int x, int y, bool front) { p.hLx = (int8_t)x; p.hLy = (int8_t)y; p.hLfront = front ? 1 : 0; };
  const int lapY = R.H - 1;
  switch (pz) {
    case Posture::Sit: case Posture::SitFloor:
      N_(2, lapY); F_(3, lapY, false);
      break;
    case Posture::Eat: case Posture::SitEat: case Posture::SitFloorEat:
      F_(legs == LegStand ? -1 : 3, legs == LegStand ? R.H + 1 : lapY, false);
      if (!odd) N_(3, R.S + 6); else N_(4, R.M + 1);
      break;
    case Posture::Drink: case Posture::SitDrink: case Posture::SitFloorDrink:
      F_(legs == LegStand ? -1 : 3, legs == LegStand ? R.H + 1 : lapY, false);
      if (!odd) N_(3, R.S + 6); else N_(3, R.M + 2);
      break;
    case Posture::Cheer:
      F_(-1, R.H + 1, false);
      N_(odd ? 3 : 4, odd ? R.T - 1 : R.T + 2);
      p.elbowOut = 1;
      break;
    case Posture::Wave:
      F_(-1, R.H + 1, false);
      N_(odd ? 4 : 3, R.T + 1);
      X.openR = true; p.elbowOut = 1;
      break;
    case Posture::Hammer:
      F_(5, R.S + 7, true);
      if (!odd) { N_(-3, R.T + 4); p.elbowOut = 1; p.lean = -1; } else { N_(5, R.S + 6); p.lean = 1; }
      break;
    case Posture::Chop:
      if (!odd) { N_(-2, R.T + 3); F_(-1, R.T + 5, true); p.elbowOut = 1; p.lean = -1; }
      else { N_(5, R.S + 8); F_(4, R.S + 7, true); p.lean = 1; }
      break;
    case Posture::Hoe:
      if (!odd) { N_(-1, R.T + 4); F_(2, R.S + 4, true); p.elbowOut = 1; }
      else { N_(5, R.S + 7); F_(2, R.S + 4, true); p.lean = 1; }
      break;
    case Posture::Sweep:
      if (!odd) { N_(2, R.S + 7); F_(-1, R.S + 3, true); } else { N_(1, R.S + 7); F_(-2, R.S + 3, true); p.lean = -1; }
      break;
    case Posture::Stir: {
      static const int dx[4] = {-1, 0, 1, 0};
      N_(4 + dx[fr & 3], R.S + 6); F_(4, R.S + 7, true);
      break;
    }
    case Posture::Carry: {
      // walking with the sack on the near shoulder (the side walk's feet)
      static const int8_t nearX[4] = {5, 0, -5, 0}, farX[4] = {-5, 0, 5, 0}, nearL[4] = {0, 2, 0, 0}, farL[4] = {0, 0, 0, 2};
      const int k = fr & 3;
      p.fRx = nearX[k]; p.fLx = farX[k]; p.fRy = nearL[k]; p.fLy = farL[k];
      p.dy = (int8_t)((fr & 1) ? 0 : 1);
      const Rows Rw = rowsOf(B, p.dy);
      N_(-1, Rw.S - 3);
      F_(-1 + (k == 0 ? 2 : (k == 2 ? -2 : 0)), Rw.H + 1, false);
      break;
    }
    case Posture::Fish:
      N_(4, R.S + 6); F_(3, R.S + 7, true);
      break;
    case Posture::Pray:
      N_(3, R.S + 4); F_(3, R.S + 4, true);
      X.hideHandR = true;
      p.headY = (int8_t)(odd ? 2 : 1); p.headX = 1; p.expr = ExClosed;
      break;
    case Posture::Lamp:
      N_(3, R.S - 2 - (odd ? 1 : 0)); F_(2, R.S + 4, true);
      p.elbowOut = 1;
      break;
    case Posture::Read:
      N_(4, R.S + 5); F_(4, R.S + 5, true);
      p.headY = 1;
      break;
    case Posture::Play: {
      static const int8_t nearX[4] = {5, 0, -5, 0}, farX[4] = {-5, 0, 5, 0}, nearL[4] = {0, 2, 0, 0}, farL[4] = {0, 0, 0, 2};
      const int k = fr & 3;
      p.fRx = nearX[k]; p.fLx = farX[k]; p.fRy = nearL[k]; p.fLy = farL[k];
      p.dy = (int8_t)-kinHop(B, fr);
      const Rows Rw = rowsOf(B, p.dy);
      N_(2, Rw.T + 3 + (odd ? 0 : 2)); F_(-3, Rw.T + 3 + (odd ? 2 : 0), false);
      X.openR = true; p.elbowOut = 1;
      break;
    }
    case Posture::Dance: {
      static const int8_t nearX[4] = {3, 0, -2, 0}, nearL[4] = {2, 0, 0, 0}, farL[4] = {0, 0, 2, 0}, bob[4] = {-1, 0, -1, 0};
      const int k = fr & 3;
      p.fRx = nearX[k]; p.fLx = (int8_t)-nearX[k]; p.fRy = nearL[k]; p.fLy = farL[k]; p.dy = bob[k];
      const Rows Rw = rowsOf(B, p.dy);
      if (k == 0) { N_(3, Rw.T + 3); F_(-3, Rw.S + 3, false); X.openR = true; p.elbowOut = 1; }
      else if (k == 2) { N_(-4, Rw.S + 3); F_(1, Rw.T + 3, true); p.elbowOut = 1; }
      else { N_(4, Rw.S + 4); F_(-4, Rw.S + 4, false); }
      break;
    }
    case Posture::Lute: {
      const LeadInst li = (LeadInst)var;
      if (li == LeadInst::Fiddle) { N_(-1 + (odd ? 1 : 0), R.S + 5); F_(5, R.S, true); }
      else if (li == LeadInst::Harp) { N_(3, R.S + 4 + (odd ? 1 : 0)); F_(5, R.S + 3, true); }
      else { N_(1 + (odd ? 1 : 0), R.S + 7); F_(6, R.S + 3, true); }
      break;
    }
    case Posture::Flute: {
      const LeadInst li = (LeadInst)var;
      if (li == LeadInst::Voice) { N_(2, R.S + 3); F_(4, R.S + 4 - (odd ? 1 : 0), false); X.openR = true; p.expr = odd ? ExNeutral : ExEffort; }
      else if (li == LeadInst::Bells) { N_(4, R.M + (odd ? 5 : 8)); F_(2, R.M + (odd ? 8 : 5), true); }
      else if (li == LeadInst::Horn || li == LeadInst::Brass) { N_(5, R.M + 1); F_(7, R.M - 2, true); }
      else if (li == LeadInst::Reed) { N_(6, R.M + 4); F_(7, R.M + 7, true); }
      else { N_(6, R.M + 1); F_(9, R.M + 1, true); }
      break;
    }
    case Posture::Drum: {
      const PercKind pk = (PercKind)var;
      if (pk == PercKind::Taiko || pk == PercKind::Kettle) { N_(3, R.S + 4 - (odd ? 3 : 0)); F_(6, R.S + 4 - (odd ? 0 : 3), true); }
      else if (pk == PercKind::Bells) { N_(3, R.S + 2 - (odd ? 1 : 0)); F_(-1, R.H + 1, false); }
      else { N_(2, R.S + 6 - (odd ? 2 : 0)); F_(5, R.S + 5, true); }
      break;
    }
    case Posture::Beg:
      N_(4, R.S + 7 - (odd ? 1 : 0)); F_(3, lapY, false);
      p.headY = 1; p.lean = 1;
      break;
    case Posture::Downed:
      N_(5, -3); F_(1, R.S + 5, true);
      p.lean = 2; p.headX = 1; p.headY = (int8_t)(2 + (odd ? 1 : 0)); p.expr = ExClosed; p.dy = (int8_t)(p.dy + 1);
      break;
    case Posture::Kneel:
      if (var == 1) { N_(4, R.H - 1); F_(3, R.H, false); p.headY = 2; p.headX = 1; p.expr = ExClosed; }
      else { N_(odd ? 6 : 5, R.H + 1 - (odd ? 1 : 0)); F_(odd ? 5 : 6, R.H + 1, true); p.headY = 1; p.lean = 1; }
      break;
    case Posture::Ride:
      N_(4, R.S + 7); F_(5, R.S + 7, true);
      break;
    default: break;
  }
  return X;
}

PoseX pzFrame(const Body& B, Posture pz, uint8_t var, int fc, int fr) {
  PoseX X;
  if (fc == FDown) X = pzDown(B, pz, var, fr);
  else if (fc == FUp) X = mirrorUp(pzDown(B, pz, var, fr), B);
  else if (fc == FRight) X = pzRight(B, pz, var, fr);
  else X = mirrorSide(pzRight(B, pz, var, fr));
  X.fr = (int8_t)fr;
  X.var = var;
  return X;
}

// the praying hands (joined: one shape, drawn over the chest)
void prayHook(Fig& f, const Body& B, const Kit& K, const PoseX& X, int layer, const void* ctx) {
  (void)B;
  if (layer == PropHand && X.p.facing == FDown) handsJoined(f, K, X.p.hRx + 1, X.p.hRy);
  if (layer == PropHand && (X.p.facing == FRight || X.p.facing == FLeft)) {
    const bool r = X.p.facing == FRight;
    const int x = r ? X.p.hRx : X.p.hLx, y = r ? X.p.hRy : X.p.hLy;
    f.set(x + (r ? 1 : 0), y - 1, K.skin[3]); f.set(x + (r ? 1 : 0), y - 2, K.skin[3]);
  }
  (void)ctx;
}
void pzHook(Fig& f, const Body& B, const Kit& K, const PoseX& X, int layer, const void* ctx) {
  if (((const PzCtx*)ctx)->pz == Posture::Pray) prayHook(f, B, K, X, layer, ctx);
  pzProps(f, B, K, X, layer, ctx);
}

}  // namespace

}  // namespace rig2

// the 31 postures on v2: clip Idle variant 0 holds the posture's frames (postureInfo's count) in four facings
RigSheet humanPosture2(const HumanLook& look, Posture p, uint8_t variant) {
  using namespace rig2;
  if (p == Posture::None || (int)p >= (int)Posture::COUNT) return RigSheet();
  const Body B = bodyOf(look);
  Kit K;
  makeKit(look, K);
  const PostureInfo pi = postureInfo(p);
  const int nf = std::clamp((int)pi.frames, 1, POSTURE_FRAMES);
  SheetBuilder sb;
  if (p == Posture::Sleep) {
    // the generic sleeper (one row: the classic bed's fit; sleeper2 fits every berth): its two breathing frames
    for (int f = 0; f < nf; f++) {
      const Canvas c = sleeper2(look, Berth::Bed, -1, f);
      if (c.w <= 0) continue;
      Cut cut;
      cut.c = c;
      cut.px = c.w / 2;
      cut.py = c.h - 1;
      sb.add(0, Clip::Idle, 0, std::move(cut));
    }
    return sb.finish();
  }
  // which variant paints: the kneel's two; the bard's instrument (art_life.h: an unknown value plays the default)
  uint8_t v = variant;
  if (p == Posture::Kneel) v = variant == 1 ? 1 : 0;
  else if (p == Posture::Lute) v = (variant == (uint8_t)LeadInst::Oud || variant == (uint8_t)LeadInst::Fiddle || variant == (uint8_t)LeadInst::Harp) ? variant : (uint8_t)LeadInst::Lute;
  else if (p == Posture::Flute)
    v = (variant == (uint8_t)LeadInst::Pipes || variant == (uint8_t)LeadInst::Reed || variant == (uint8_t)LeadInst::Horn || variant == (uint8_t)LeadInst::Brass ||
         variant == (uint8_t)LeadInst::Bells || variant == (uint8_t)LeadInst::Voice)
            ? variant
            : (uint8_t)LeadInst::Flute;
  else if (p == Posture::Drum) v = (variant >= (uint8_t)PercKind::Frame && variant < (uint8_t)PercKind::COUNT) ? variant : (uint8_t)PercKind::Frame;
  else if (p == Posture::Carry) v = variant <= 2 ? variant : 0;   // (POSES depth) the load: 0 a sack, 1 a basket, 2 a crate
  else v = 0;
  const PzCtx ctx{p};
  for (int fc = 0; fc < RIG_FACINGS; fc++)
    for (int f = 0; f < nf; f++) {
      PoseX X = pzFrame(B, p, v, fc, f);
      X.prop = pzHook;
      X.ctx = &ctx;
      sb.add(fc, Clip::Idle, 0, paintPose(look, B, K, X, LOCO_W, LOCO_H, LOCO_FEET));
    }
  return sb.finish();
}

// ---------------------------------------------------------------- seats (agreed with HOLDERS: art_holders2.h)
// HOLDERS' seatRow2 is the seat surface's row inside its tile where a sitter's seat line lands (the middle of the seat's
// top, 2 rows behind its lit front edge, kSeat = 11 px over the floor row 15). A chair-height sitter's hips are on that
// row in every facing; the posture paints them kChairHip rows over its pivot, so the pivot lands on the floor row (the
// feet on the floor). Floor seats (cushion, mat, bedroll, rug, the bare ground): cross-legged, the hips 2 px over the
// pivot (the floor, or the cushion's plump top).
SeatFit seatFit2(Prop seat, int kit, int facing) {
  (void)kit;
  SeatFit f;
  const int fc = facing == 3 ? 2 : facing;
  f.front = true;
  const int chairHip = rig2::kChairHip, floorHip = -2;
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

}  // namespace art
