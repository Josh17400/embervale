// EMBERVALE art, v2 rig (RIG-BODY, P2a): the skeleton and pose solver (per-frame anchors: head box, neck, shoulders,
// elbows, hands with grip angle, hips, knees, feet, the back slot; the depth order; four native facings), the atlas
// packer with pivots (art::FrameRect: the point between the feet on the sole row) and the v2 ink outline (STYLE_P2 3, 8,
// 9, 10). The parts are painted by rig_face.cpp (head, hands), rig_hair.cpp, rig_cloth.cpp (torso, skirt); this file
// lays them out in depth order for each facing.
#include <algorithm>
#include <cmath>
#include <cstring>
#include "rpg/art/rig/rig_internal.h"

namespace art {
namespace rig2 {

// ---------------------------------------------------------------- the inner contour (STYLE_P2 8: the `layered` rule)
void Fig::contour(float strength, float similar, uint8_t skipLayer) {
  if (lx1 < lx0) return;
  static const int nx[4] = {0, -1, 1, 0}, ny[4] = {1, 0, 0, -1};
  const int x0 = std::max(0, lx0 - 1), y0 = std::max(0, ly0 - 1), x1 = std::min(W - 1, lx1 + 1), y1 = std::min(H - 1, ly1 + 1);
  // (R6: plain pointers and locals, so the loop never reloads the vectors through `this`)
  uint32_t* const P = px.data();
  const uint8_t* const Ly = lay.data();
  const uint8_t c = cur, skip = skipLayer;
  const int w = W, h = H;
  for (int cy = y0; cy <= y1; cy++)
    for (int cx = x0; cx <= x1; cx++) {
      const size_t i = (size_t)cy * w + cx;
      const uint8_t l = Ly[i];
      if (!l || l == c || l == skip) continue;
      for (int k = 0; k < 4; k++) {
        const int qx = cx + nx[k], qy = cy + ny[k];
        if (qx < 0 || qy < 0 || qx >= w || qy >= h) continue;
        const size_t j = (size_t)qy * w + qx;
        if (Ly[j] != c) continue;
        const uint32_t n = P[j];
        if (std::fabs(luma(n) - luma(P[i])) > similar) break;
        P[i] = opaque(mix(P[i], outlineOf(n, 1.0f), strength));
        break;
      }
    }
}

// ---------------------------------------------------------------- limbs
// a limb from a (top) to b, w px wide, in ramp r: row spans for a steep limb, column spans for a flat one (clean
// pixel steps, never a blob); lit on the left / top column, shadowed on the right / bottom (STYLE_P2 7: cylinders)
void limb(Fig& f, float ax, float ay, float bx, float by, int w, const Ramp5& r, int litBias) {
  const float dx = bx - ax, dy = by - ay;
  auto shadeOf = [&](int k) {   // k: 0 = the lit edge .. w - 1 = the shadow edge (3 px: lit, base, shadow)
    if (w <= 1) return r[2 + litBias];
    if (k == w - 1) return r[1 + litBias];
    if (k == 0 && w >= 3) return r[3 + litBias];
    return r[2 + litBias];
  };
  if (std::fabs(dy) >= std::fabs(dx)) {
    const int y0 = (int)std::floor(std::min(ay, by)), y1 = (int)std::floor(std::max(ay, by) - 0.001f);
    for (int y = y0; y <= y1; y++) {
      const float t = std::fabs(dy) < 1e-3f ? 0.0f : clampf((y + 0.5f - ay) / dy, 0.0f, 1.0f);
      const float cx = ax + dx * t;
      const int left = (int)std::floor(cx - w * 0.5f + 0.5f);
      for (int k = 0; k < w; k++) f.set(left + k, y, shadeOf(k));
    }
  } else {
    const int x0 = (int)std::floor(std::min(ax, bx)), x1 = (int)std::floor(std::max(ax, bx) - 0.001f);
    for (int x = x0; x <= x1; x++) {
      const float t = clampf((x + 0.5f - ax) / dx, 0.0f, 1.0f);
      const float cy = ay + dy * t;
      const int top = (int)std::floor(cy - w * 0.5f + 0.5f);
      for (int k = 0; k < w; k++) f.set(x, top + k, shadeOf(k));
    }
  }
}

namespace {

// two-bone solve: the joint between a and b (lengths l1, l2), bent toward side (+1 / -1 of the a->b normal)
void joint(float ax, float ay, float bx, float by, float l1, float l2, float side, float& jx, float& jy) {
  const float dx = bx - ax, dy = by - ay, d = std::max(1e-3f, std::sqrt(dx * dx + dy * dy));
  if (d >= l1 + l2 - 0.05f) { jx = ax + dx * l1 / (l1 + l2); jy = ay + dy * l1 / (l1 + l2); return; }
  const float a = (l1 * l1 - l2 * l2 + d * d) / (2 * d), h = std::sqrt(std::max(0.0f, l1 * l1 - a * a));
  const float ux = dx / d, uy = dy / d;
  jx = ax + ux * a - uy * h * side;
  jy = ay + uy * a + ux * h * side;
}

}  // namespace

// ---------------------------------------------------------------- the look's colours (STYLE_P2 6)
void makeKit(const HumanLook& L, Kit& k) {
  k.skin = r5(ramp(L.skin, 0.8f));
  k.hair = r5(ramp(L.hairColor));
  k.hair_ = (int)L.hair;
  k.outfit = (int)L.outfit;
  k.cut = (L.outfit == Outfit::Tunic || L.outfit == Outfit::Dress || L.outfit == Outfit::Robe) && L.cut <= 8 ? L.cut : 0;
  k.pattern = L.pattern <= 6 ? L.pattern : 0;
  k.skirt = L.outfit == Outfit::Dress || L.outfit == Outfit::Robe || k.cut == 2 || k.cut == 3 || k.cut == 4 || k.cut == 8;
  k.beard = L.beard && L.body == 0;
  k.weapon = L.weapon;
  k.top = r5(ramp(L.topColor));
  k.sleeve = k.top;
  k.leg = r5(ramp(L.bottomColor));
  k.boot = r5(ramp5(rgba(46, 28, 34), rgba(70, 42, 38), rgba(104, 64, 46), rgba(138, 92, 60), rgba(170, 124, 80)));
  k.belt = r5(kLeather);
  k.trim = r5(ramp(L.tabardColor));
  k.pat = r5(ramp(L.patternColor ? L.patternColor : shade(L.topColor, 0.62f)));
  k.metal = r5(ramp(L.weaponColor, 1.1f));
  k.guard = r5(kBrass);
  if (L.outfit == Outfit::Rags) {
    // torn sacking over bare arms and bare feet, the trousers frayed at the shins (paintLegWear)
    k.top = r5(ramp(rgba(150, 128, 98), 0.9f)); k.sleeve = k.skin; k.leg = r5(ramp(rgba(122, 104, 80), 0.9f));
    k.boot = r5(ramp5(k.skin[0], k.skin[0], k.skin[1], k.skin[2], k.skin[3]));
    k.rags = true;
  }
  // (RIG-BODY, after the slice) what hangs below the hips, by outfit and culture cut (cult::Cut + 1: 1 tunic, 2 robe,
  // 3 kaftan, 4 wrap, 5 coat, 6 kilt, 7 poncho, 8 gown)
  k.hang = k.skirt ? HangLong : HangNone;
  if (k.cut == 1 || k.cut == 7) k.hang = HangShort;
  if (k.cut == 5 || k.cut == 6) k.hang = HangKnee;
  k.openFront = k.cut == 3 || k.cut == 5;
  // headwear: a helmet hides it (P2b paints helmets), the legacy hood wins over a culture headwear (v1's rule)
  k.hood = L.hood && !L.helmet && !L.helmStyle;
  k.headwear = (L.helmet || L.helmStyle || L.hood || L.headwear > 8) ? 0 : L.headwear;
  k.head = r5(ramp(L.headColor ? L.headColor : L.topColor));
  k.hoodR = (L.outfit == Outfit::Robe || k.cut) ? k.top : r5(ramp(shade(L.bottomColor, 0.95f)));
  k.fur = r5(ramp(rgba(150, 126, 100)));   // the fur hat's brim: a natural pelt under any cloth colour
  k.straw = L.headColor ? k.head : r5(ramp5(rgba(96, 70, 44), rgba(150, 112, 60), rgba(196, 160, 90), rgba(226, 198, 126), rgba(244, 228, 170)));
  k.gold = r5(kGold);
  k.facePaint = L.facePaint <= 4 ? L.facePaint : 0;
  k.jewellery = L.jewellery & 3;
  // the face (STYLE_P2 4): lash = the eye colour darkened 55 % toward Ink; brows = the hair darkened 30 %
  k.iris = L.eyeColor ? opaque(L.eyeColor) : rgba(36, 26, 48);   // Eye Dusk #241A30
  k.lash = mix(mix(k.iris, kInk, 0.55f), kInk, 0.0f);
  k.brow = mix(shade(opaque(L.hairColor), 0.70f), kInk, 0.18f);
  k.mouth = mix(k.skin[0], rgba(122, 59, 51), 0.30f);
  k.rose = mix(k.skin[1], rgba(160, 64, 60), 0.10f);
  k.blush = mix(k.skin[2], rgba(220, 90, 80), 0.08f);
}

// ---------------------------------------------------------------- the slice's sword (STYLE_P2 11)
// 16 directions on clean pixel slopes (0, 1:2, 1:1, 2:1 ... ), clockwise from pointing right (screen y down)
namespace {
const int kDir[16][2] = {{2, 0}, {2, 1}, {2, 2}, {1, 2}, {0, 2}, {-1, 2}, {-2, 2}, {-2, 1},
                         {-2, 0}, {-2, -1}, {-2, -2}, {-1, -2}, {0, -2}, {1, -2}, {2, -2}, {2, -1}};
// the n-th pixel along direction d from (0,0): one pixel per step of the major axis (no doubled pixels)
void stepAlong(int d, int n, int& x, int& y) {
  const int vx = kDir[d & 15][0], vy = kDir[d & 15][1];
  const int ax = std::abs(vx), ay = std::abs(vy);
  if (ax >= ay) { x = n * (vx > 0 ? 1 : -1); y = ay == 0 ? 0 : (int)std::floor((n * ay + ax / 2) / (float)ax) * (vy > 0 ? 1 : -1); }
  else { y = n * (vy > 0 ? 1 : -1); x = ax == 0 ? 0 : (int)std::floor((n * ax + ay / 2) / (float)ay) * (vx > 0 ? 1 : -1); }
}
float dirLen(int d) {   // euclidean px per major-axis step
  const int vx = kDir[d & 15][0], vy = kDir[d & 15][1];
  return std::sqrt((float)(vx * vx + vy * vy)) / (float)std::max(std::abs(vx), std::abs(vy));
}
}  // namespace

void paintWeapon(Fig& f, const Kit& K, int weapon, float gx, float gy, int dir, int shorten, int& tipX, int& tipY) {
  tipX = (int)gx; tipY = (int)gy;
  if (weapon != 1 && weapon != 7) return;   // the slice paints the sword (RIG-ARMS: the rest)
  const int d = dir & 15;
  const int vx = kDir[d][0], vy = kDir[d][1];
  const bool steep = std::abs(vy) > std::abs(vx);
  const int gxI = (int)std::floor(gx), gyI = (int)std::floor(gy);
  // the edge offset (the blade's second column / row) and which edge the top-left light finds
  int ex = 0, ey = 0;
  if (steep) ex = 1; else ey = 1;
  if (std::abs(vx) == std::abs(vy)) { ex = vx * vy > 0 ? 1 : 0; ey = vx * vy > 0 ? 0 : 1; }
  // the light's side: the edge whose outward normal faces up-left is lit
  const float nx = (float)ex, ny = (float)ey;   // the offset edge's outward direction
  const bool offsetLit = (nx * -0.56f + ny * -0.62f) > 0;
  const float per = dirLen(d);
  const int blade = std::max(4, (int)std::lround((11 - shorten) / per)), hilt = 2;
  // pommel (1 px behind the grip), the grip (under the fist), the guard across, the blade
  int px0, py0;
  stepAlong((d + 8) & 15, hilt, px0, py0);
  f.set(gxI + px0, gyI + py0, K.guard[3]);
  for (int n = -hilt + 1; n <= 1; n++) {
    int x, y;
    if (n >= 0) stepAlong(d, n, x, y); else { stepAlong((d + 8) & 15, -n, x, y); }
    f.set(gxI + x, gyI + y, K.belt[1]);
  }
  int gdx, gdy;
  stepAlong(d, 2, gdx, gdy);
  // the guard: 4 px across the blade (2 each side), brass, lit on its top-left
  const int cxG = gxI + gdx, cyG = gyI + gdy;
  const int qx = steep ? 1 : 0, qy = steep ? 0 : 1;
  for (int k = -2; k <= 1; k++) {
    int ox = qx * k, oy = qy * k;
    if (std::abs(vx) == std::abs(vy)) { ox = (vx * vy > 0) ? k : k; oy = (vx * vy > 0) ? -k : k; }
    f.set(cxG + ox, cyG + oy, K.guard[k < 0 ? 3 : 1]);
  }
  for (int n = 1; n <= blade; n++) {
    int x, y;
    stepAlong(d, 2 + n, x, y);
    const int bx = gxI + x, by = gyI + y;
    const bool tip = n == blade;
    f.set(bx, by, tip ? K.metal[4] : (offsetLit ? K.metal[1] : K.metal[3]));
    if (!tip) f.set(bx + ex, by + ey, offsetLit ? K.metal[3] : K.metal[1]);
    if (n == 2) f.set(offsetLit ? bx + ex : bx, offsetLit ? by + ey : by, K.metal[4]);   // the specular near the guard
    if (tip) { tipX = bx; tipY = by; }
  }
}

// ---------------------------------------------------------------- one frame
namespace {

struct Joints {
  int cx = 0;          // the torso's centre column at the hips (pixels -1 / 0 straddle it)
  int hipRow = 0, shoulderRow = 0;
  int sx = 0;          // the torso's centre column at the shoulders (the lean)
  HeadAt head;
};

// an arm: the upper arm (sleeve) from the shoulder to the elbow, the forearm (skin or sleeve) to the wrist; returns
// nothing (the hand is painted after, over what it holds)
void arm(Fig& f, const Kit& K, float sx, float sy, float wx, float wy, float bendSide, bool farSide, int lu, int lf, bool longSleeve) {
  float ex, ey;
  joint(sx, sy, wx, wy, (float)lu, (float)lf, bendSide, ex, ey);
  const int bias = farSide ? -1 : 0;
  const Ramp5& sl = sleeveRamp(K);
  limb(f, ex, ey, wx, wy, 2, longSleeve ? sl : K.skin, bias);
  limb(f, sx, sy, ex, ey, 2, sl, bias);
  // the sleeve's lit hem at the elbow (STYLE_P2 2: hems are lit)
  if (!longSleeve) {
    const int cyI = (int)std::floor(ey - 0.001f), cxI = (int)std::floor(ex - 1.0f + 0.5f);
    for (int k = 0; k < 2; k++) if (f.solid(cxI + k, cyI)) f.set(cxI + k, cyI, sl[k == 0 ? 3 + bias : 2 + bias]);
  }
}

void leg(Fig& f, const Kit& K, float hx, float hy, float ax, float ay, float bendSide, bool farSide, int lift, int facing, float kneeFwd,
         int w = 3, int shoeW = 4) {
  const float lt = (ay - hy) * 0.5f + 0.02f;   // (straight at rest: the knees bend when the hips drop or a foot lifts)
  float kx, ky;
  joint(hx, hy, ax, ay, lt, lt, bendSide, kx, ky);
  kx += kneeFwd;
  const int bias = farSide ? -1 : 0;
  const Ramp5& lr = legRamp(K);
  limb(f, hx, hy, kx, ky, w, lr, bias);
  limb(f, kx, ky, ax, ay, w, lr, bias);
  // the shoe: two rows under the ankle (front / back views: the ankle's columns, then one more on the outer side below
  // (4 px for a 3 px leg); side views: heel 1 px behind, toe 2 ahead (a child's 1))
  const int sy = (int)std::floor(ay);
  const int axI = (int)std::floor(ax);
  const Ramp5& b = K.boot;
  if (facing == FDown || facing == FUp) {
    const int a0 = (int)std::floor(ax - w * 0.5f + 0.5f), a1 = a0 + w - 1;   // the ankle's columns (axI - 1 .. axI + 1 at 3 px)
    for (int x = a0; x <= a1; x++) f.set(x, sy, b[x == a0 ? 3 + bias : 2 + bias]);
    for (int x = a0 - 1; x <= a1; x++) f.set(x, sy + 1, b[x == a1 ? 1 + bias : 2 + bias]);
    if (facing == FDown) f.set(a0, sy, b[4 + bias]);
  } else {
    const int s = facing == FRight ? 1 : -1;
    const int toe = shoeW - 1;   // 3 (2 for a child)
    for (int k = -1; k <= 1; k++) f.set(axI + k * s, sy, b[k == -1 ? (s > 0 ? 3 : 1) + bias : 2 + bias]);
    for (int k = -1; k <= toe; k++) f.set(axI + k * s, sy + 1, b[k == toe ? 1 + bias : 2 + bias]);
    if (toe >= 3) f.set(axI + 2 * s, sy, b[2 + bias]);
  }
  (void)lift;
}

}  // namespace

namespace {

// The weapon in hand, memoised (R6): a weapon's pixels depend only on its art id, direction, foreshortening and the
// look's metal, guard and grip colours, and move with the fist by whole px (the grip is always a fist centre, x.5), so
// the first frame that holds it at a (dir, shorten) records the raster and the other frames of the atlas replay it,
// pixel for pixel what paintWeapon / paintArms would paint there.
struct WeaponRaster {
  uint64_t key = 0;
  int tipDx = 0, tipDy = 0;
  std::vector<int16_t> dx, dy;
  std::vector<uint32_t> c;
};
void paintHeld(Fig& f, const Kit& K, int weapon, float gx, float gy, int dir, int shorten, int& tipX, int& tipY) {
  uint64_t key = 1469598103934665603ull;
  auto mixIn = [&](uint64_t v) { key ^= v; key *= 1099511628211ull; };
  mixIn((uint64_t)weapon | (uint64_t)(dir & 255) << 8 | (uint64_t)(shorten & 255) << 16);
  for (int i = 0; i < 5; i++) { mixIn(K.metal.c[i]); mixIn(K.guard.c[i]); mixIn(K.belt.c[i]); }
  thread_local std::vector<WeaponRaster> memo;
  const int bx = (int)std::floor(gx), by = (int)std::floor(gy);
  const WeaponRaster* hit = nullptr;
  for (const WeaponRaster& w : memo) if (w.key == key) { hit = &w; break; }
  if (!hit) {
    if (memo.size() >= 48) memo.clear();
    thread_local Fig rec(112, 112, 56, 56);   // (kept clear between uses: only the box a weapon painted is wiped)
    rec.cur = 1; rec.lx0 = rec.W; rec.ly0 = rec.H; rec.lx1 = -1; rec.ly1 = -1;
    int tx = 0, ty = 0;
    const float fx = gx - (float)bx, fy = gy - (float)by;   // (x.5: the fist's centre)
    if (weapon == 1) paintWeapon(rec, K, weapon, fx, fy, dir, shorten, tx, ty);
    else paintArms(rec, K, weapon, fx, fy, dir, shorten, 1, tx, ty);
    WeaponRaster w;
    w.key = key; w.tipDx = tx; w.tipDy = ty;
    for (int y = rec.ly0; y <= rec.ly1; y++)
      for (int x = rec.lx0; x <= rec.lx1; x++) {
        const size_t i = (size_t)y * rec.W + x;
        if (!rec.lay[i]) continue;
        w.dx.push_back((int16_t)(x - rec.ox)); w.dy.push_back((int16_t)(y - rec.oy));
        w.c.push_back(rec.px[i]);
        rec.lay[i] = 0; rec.px[i] = 0;
      }
    memo.push_back(std::move(w));
    hit = &memo.back();
  }
  for (size_t i = 0; i < hit->c.size(); i++) f.set(bx + hit->dx[i], by + hit->dy[i], hit->c[i]);
  tipX = bx + hit->tipDx; tipY = by + hit->tipDy;
}

void paintInto(Fig& f, const HumanLook& look, const Body& B, const Kit& K, const Pose& P, RigAnchors& an) {
  an = RigAnchors();
  const int fc = P.facing;
  const bool front = fc == FDown, back = fc == FUp, side = fc == FRight || fc == FLeft;
  const int s = fc == FLeft ? -1 : 1;   // side views: +1 facing right
  Joints J;
  J.cx = P.hipX;
  J.hipRow = B.hip + P.dy;
  J.shoulderRow = B.shoulder + P.dy + P.chest;
  J.sx = J.cx + P.lean;
  J.head.x = J.sx - 6 + P.headX;
  J.head.y = B.headTop - 1 + P.dy + P.chest + P.headY;   // the head box's top (the crown outline row)
  const TorsoAt T{J.sx, J.shoulderRow};
  const float hipY = (float)J.hipRow + 1.0f;               // the legs start under the hip row
  const float ankleY = -2.0f;
  const bool longSleeve = longSleeved(K);
  const int lu = B.armU, lf = B.armF;
  // ---- the legs' joints
  float hRx, hLx;   // hip joints (continuous centres)
  if (front) { hRx = J.cx - B.hipJoint; hLx = J.cx + B.hipJoint; }
  else if (back) { hRx = J.cx + B.hipJoint; hLx = J.cx - B.hipJoint; }
  else { hRx = J.cx + 0.5f - 0.5f * s; hLx = J.cx + 0.5f + 0.5f * s; }
  const float aRx = P.fRx + 0.5f, aLx = P.fLx + 0.5f;
  const float aRy = ankleY - P.fRy, aLy = ankleY - P.fLy;
  // ---- the arms' joints: shoulders (continuous) and wrists
  float shRx, shLx, shY = (float)J.shoulderRow + (side ? 0.5f : 1.5f);   // front views: the arm hangs from under the rounded shoulder
  if (front) { shRx = J.sx - B.shoulderHalf + 1.0f; shLx = J.sx + B.shoulderHalf - 1.0f - B.trimR; }
  else if (back) { shRx = J.sx + B.shoulderHalf - 1.0f - B.trimR; shLx = J.sx - B.shoulderHalf + 1.0f; }
  else { shRx = (float)J.sx - 1.0f * (float)s; shLx = (float)J.sx; if (s < 0) { shRx = (float)J.sx + 1.0f; shLx = (float)J.sx; } }
  const float wRx = P.hRx + 1.0f, wRy = P.hRy - 0.5f, wLx = P.hLx + 1.0f, wLy = P.hLy - 0.5f;
  // which way the elbows bend: down and out in front views; backward (against the facing) in side views
  auto bendOf = [&](bool right) {
    if (front || back) {
      const bool screenLeft = (front && right) || (back && !right);
      return (screenLeft ? 1.0f : -1.0f) * (P.elbowOut ? 1.0f : 0.3f);
    }
    return (P.elbowOut ? -1.0f : 1.0f) * (float)s;
  };
  // ---- the weapon
  int tipX = 0, tipY = 0;
  // the sword is the slice's (paintWeapon); every other drawn weapon is RIG-ARMS's (paintArms, rig_arms.cpp)
  const bool hasWeapon = P.wDir >= 0 && (K.weapon == 1 || armsDrawn(K.weapon));
  const float gripX = P.hRx + 1.0f, gripY = P.hRy + 1.0f;   // under the fist's centre
  auto drawWeapon = [&]() {
    if (!hasWeapon) return;
    f.begin();
    paintHeld(f, K, K.weapon, gripX - 0.5f, gripY - 0.5f, P.wDir, P.wShort, tipX, tipY);
    f.contour(0.6f, 0.2f);
  };
  // (side views: the near leg drawn over the far one gets the 1 px inner contour, STYLE_P2 8, so two legs of one cloth
  // read as two, standing, striding or lying)
  auto drawLegR = [&](bool farSide) {
    f.begin();
    leg(f, K, hRx, hipY, aRx, aRy, side ? -(float)s : (front ? 0.2f : -0.2f), farSide, P.fRy, fc, side ? (float)(P.kR * s) : 0.0f, B.legW, B.shoeW);
    if (side && !farSide) f.contour(0.55f, 0.2f);
  };
  auto drawLegL = [&](bool farSide) {
    f.begin();
    leg(f, K, hLx, hipY, aLx, aLy, side ? -(float)s : (front ? -0.2f : 0.2f), farSide, P.fLy, fc, side ? (float)(P.kL * s) : 0.0f, B.legW, B.shoeW);
    if (side && !farSide) f.contour(0.55f, 0.2f);
  };
  // the kilt's bare knees, the rags' torn shins: over both legs once they are painted (the far leg's pixels too)
  uint8_t legLayerA = 0, legLayerB = 0;
  auto drawLegs = [&](bool rFar, bool lFar, bool rFirst) {
    if (rFirst) { drawLegR(rFar); legLayerA = f.cur; drawLegL(lFar); legLayerB = f.cur; }
    else { drawLegL(lFar); legLayerA = f.cur; drawLegR(rFar); legLayerB = f.cur; }
    f.legLay[0] = legLayerA; f.legLay[1] = legLayerB;
    if (K.rags || K.cut == 6) { f.begin(); paintLegWear(f, B, K, P, legLayerA, legLayerB); }
  };
  uint8_t armLayerR = 0, armLayerL = 0;
  // the poncho hides an arm that hangs under it (the hand shows at its edge); an arm raised out of it is drawn over it
  const int ponchoHem = J.hipRow + 2;
  auto underPoncho = [&](int handY) { return K.cut == 7 && handY >= J.shoulderRow + 5 && handY <= ponchoHem + 1; };
  auto drawArmR = [&](bool farSide) {
    f.begin();
    armLayerR = f.cur;
    if (underPoncho(P.hRy)) return;
    arm(f, K, shRx, shY, wRx, wRy, bendOf(true), farSide, lu, lf, longSleeve);
    f.contour();
  };
  auto drawArmL = [&](bool farSide) {
    f.begin();
    armLayerL = f.cur;
    if (underPoncho(P.hLy)) return;
    arm(f, K, shLx, shY, wLx, wLy, bendOf(false), farSide, lu, lf, longSleeve);
    f.contour();
  };
  auto drawHandR = [&]() { f.begin(); paintHand(f, K, P.hRx, P.hRy); f.contour(0.7f, 0.25f, armLayerR); };
  // the bow carried braced in the off hand (rig_arms.cpp paintBow, 20 px), under the fist that holds it
  auto drawBow = [&]() {
    if (!(P.bowRest && K.weapon == 3)) return;
    f.begin();
    int tx = 0, ty = 0;
    paintBow(f, K, P.hLx, P.hLy, P.wDir < 0 ? 0 : P.wDir, 10, P.hLx, P.hLy, 0, 0, false, tx, ty);
    f.contour(0.6f, 0.2f);
  };
  auto drawHandL = [&]() { drawBow(); f.begin(); paintHand(f, K, P.hLx, P.hLy); f.contour(0.7f, 0.25f, armLayerL); };
  auto drawBody = [&]() {
    f.begin();
    paintTorso(f, B, K, P, T);
    if (K.hang != HangNone) {
      f.begin();
      paintSkirt(f, B, K, P, T, P.hemLag ? P.hemRx : P.fRx, P.hemLag ? P.hemLx : P.fLx);
      f.contour(0.5f, 0.12f);
    }
    if (K.jewellery & 2) { f.begin(); paintTorc(f, B, K, P, T); }
  };
  auto drawHead = [&]() {
    f.begin();
    paintFace(f, B, K, P, J.head, an);
    if (K.beard) { f.begin(); paintBeard(f, B, K, P, J.head); }
    f.begin();
    const uint8_t hairLayer = f.cur;
    paintHair(f, B, K, P, J.head, false);
    if (!headwearHides(K) && K.hair_ != (int)Hair::Bald) hairContactShadow(f, K, J.head, hairLayer);
    if (K.facePaint || (K.jewellery & 1) || B.ear) { f.begin(); paintFaceMarks(f, B, K, P, J.head); }
    if (K.headwear || K.hood) { f.begin(); paintHeadwear(f, B, K, P, J.head, false); f.contour(0.55f, 0.16f); }
  };
  auto drawHairBack = [&]() {
    f.begin();
    paintHair(f, B, K, P, J.head, true);
    f.contour(0.6f, 0.2f);
    if (K.headwear || K.hood) { f.begin(); paintHeadwear(f, B, K, P, J.head, true); f.contour(0.55f, 0.16f); }
  };

  if (front) {
    drawHairBack();
    drawLegs(false, false, true);
    drawBody();
    if (hasWeapon && P.wBehind) drawWeapon();
    drawArmR(false); drawArmL(false);
    drawHead();
    drawHandL();
    if (hasWeapon && !P.wBehind) drawWeapon();
    drawHandR();
  } else if (back) {
    if (hasWeapon && P.wBehind) drawWeapon();
    if (hasWeapon && P.wBehind) drawHandR();
    drawLegs(false, false, true);
    drawBody();
    drawArmR(false); drawArmL(false);
    drawHead();
    drawHairBack();
    drawHandL();
    if (hasWeapon && !P.wBehind) drawWeapon();
    if (!(hasWeapon && P.wBehind)) drawHandR();
  } else {
    // side views: the near side is the right side facing right, the left side facing left
    const bool rightNear = fc == FRight;
    const bool farFront = rightNear ? P.hLfront : P.hRfront;
    // behind everything: the far arm (when it is behind), the weapon if behind, the far leg
    if (rightNear) {
      if (!farFront) { drawArmL(true); drawHandL(); }
      if (hasWeapon && P.wBehind) drawWeapon();
      drawLegs(false, true, false);
    } else {
      // facing left the weapon hand is the far hand: a blade behind the body goes behind everything, its arm behind the
      // torso (at its own side) or in front of it (reaching forward or across)
      if (hasWeapon && P.wBehind) drawWeapon();
      if (!farFront) { drawArmR(true); drawHandR(); }
      drawLegs(true, false, true);
    }
    drawHairBack();
    drawBody();
    drawHead();
    if (rightNear) {
      if (farFront) { drawArmL(true); drawHandL(); }
      if (hasWeapon && !P.wBehind) drawWeapon();
      drawArmR(false);
      drawHandR();
    } else {
      if (farFront) { drawArmR(true); if (hasWeapon && !P.wBehind) drawWeapon(); drawHandR(); }
      else if (hasWeapon && !P.wBehind) drawWeapon();   // (a blade in front of the body has its arm in front: swingPose)
      drawArmL(false);
      drawHandL();
    }
  }
  // ---- the pose data (pivot-relative)
  an.headX = (int16_t)J.head.x; an.headY = (int16_t)J.head.y; an.headW = 12; an.headH = 12;
  an.handRX = (int16_t)P.hRx; an.handRY = (int16_t)P.hRy;
  an.handLX = (int16_t)P.hLx; an.handLY = (int16_t)P.hLy;
  an.hipX = (int16_t)J.cx; an.hipY = (int16_t)J.hipRow;
  if (P.fRy == 0) an.footRX = (int16_t)P.fRx;
  if (P.fLy == 0) an.footLX = (int16_t)P.fLx;
  if (hasWeapon) {
    an.gripX = (int16_t)P.hRx; an.gripY = (int16_t)P.hRy;
    an.tipX = (int16_t)tipX; an.tipY = (int16_t)tipY;
    an.weaponBehind = P.wBehind != 0;
  }
  (void)look;
}

// the ink outer outline (outlineInk's rule: Ink below / right, Lit Ink above / left), the pinholes closed and the trim,
// all inside the painted box (a frame bakes in a few microseconds: R6)
Cut finishFig(Fig& f, const RigAnchors& an) {
  f.mergeBounds();
  Cut out;
  if (f.gx1 < f.gx0) return out;
  const int bw = f.gx1 - f.gx0 + 1, bh = f.gy1 - f.gy0 + 1;
  const int w = bw + 2, h = bh + 2;   // the painted box and a ring for the outline
  // (R6: the scratch buffers live on between frames, so a frame allocates only its own pixels)
  // (R6: one sweep paints the outline, settles each Lit Ink pixel and finds every row's extent; the pinhole pass then
  // walks only inside those extents; the scratch buffers live on between frames)
  thread_local std::vector<uint32_t> src;
  thread_local std::vector<uint8_t> state;
  thread_local std::vector<int> q, rowL, rowR, cand;
  // the painted pixels are read straight off the figure when the box's ring lies inside the canvas (always, as R5 keeps
  // every frame off the scratch edge: outside the box nothing was painted), else off a padded copy; ss = their stride
  const uint32_t* s;
  int ss;
  out.c.w = w; out.c.h = h;
  if (f.gx0 >= 1 && f.gy0 >= 1 && f.gx1 <= f.W - 2 && f.gy1 <= f.H - 2) {
    out.c.px.assign((size_t)w * h, 0u);
    for (int y = 0; y < bh; y++) std::memcpy(&out.c.px[(size_t)(y + 1) * w + 1], &f.px[(size_t)(f.gy0 + y) * f.W + f.gx0], (size_t)bw * 4);
    s = f.px.data() + (size_t)(f.gy0 - 1) * f.W + (f.gx0 - 1);
    ss = f.W;
  } else {
    src.assign((size_t)w * h, 0u);
    for (int y = 0; y < bh; y++) std::memcpy(&src[(size_t)(y + 1) * w + 1], &f.px[(size_t)(f.gy0 + y) * f.W + f.gx0], (size_t)bw * 4);
    out.c.px = src;
    s = src.data();
    ss = w;
  }
  uint32_t* o = out.c.px.data();
  auto shape = [&](int x, int y) { return x >= 0 && y >= 0 && x < w && y < h && (s[(size_t)y * ss + x] >> 24) > 96; };
  // a pixel that is opaque once outlined: painted, or an outline pixel (a shape pixel beside it)
  auto filled = [&](int x, int y) {
    if (x < 0 || y < 0 || x >= w || y >= h) return false;
    if (s[(size_t)y * ss + x] >> 24) return true;
    return shape(x - 1, y) || shape(x, y - 1) || shape(x, y + 1) || shape(x + 1, y);
  };
  rowL.assign((size_t)h, -1); rowR.assign((size_t)h, -1);
  // each row's painted extent (only pixels within a px of a painted pixel of this row or the rows either side can take
  // an outline, so the sweep walks just that band)
  thread_local std::vector<int> srcL, srcR;
  srcL.assign((size_t)h, w); srcR.assign((size_t)h, -1);
  int* const sL = srcL.data();
  int* const sR = srcR.data();
  int* const rL = rowL.data();
  int* const rR = rowR.data();
  for (int y = 1; y < h - 1; y++) {
    const uint32_t* row = s + (size_t)y * ss;
    int l = 1;
    while (l < w - 1 && !(row[l] >> 24)) l++;
    if (l >= w - 1) continue;
    int r = w - 2;
    while (r > l && !(row[r] >> 24)) r--;
    sL[y] = l; sR[y] = r;
  }
  // Lit Ink: Ink 30 % toward the shape it outlines (mix(kInk, opaque(n), 0.30f), inline: the same float arithmetic)
  auto litInk = [](uint32_t n) {
    const float t = 0.30f;
    return rgba(clamp255(chR(kInk) + (chR(n) - chR(kInk)) * t), clamp255(chG(kInk) + (chG(n) - chG(kInk)) * t),
                clamp255(chB(kInk) + (chB(n) - chB(kInk)) * t), chA(kInk));
  };
  for (int y = 0; y < h; y++) {
    int bandL = w, bandR = -1;
    for (int k = y - 1; k <= y + 1; k++)
      if (k >= 0 && k < h && sR[k] >= 0) { bandL = std::min(bandL, sL[k] - 1); bandR = std::max(bandR, sR[k] + 1); }
    if (bandR < 0) continue;
    bandL = std::max(0, bandL); bandR = std::min(w - 1, bandR);
    int rl = -1, rr = -1;
    const uint32_t* const srow = s + (size_t)y * ss;
    uint32_t* const orow = o + (size_t)y * w;
    for (int x = bandL; x <= bandR; x++) {
      if (srow[x] >> 24) { if (rl < 0) rl = x; rr = x; continue; }
      // a pixel on a bottom- or right-facing edge of the shape (the shape above it or to its left) is Ink, even where
      // it also tops or leads another part (a gap, a concave corner); only a purely top- or left-facing edge pixel
      // (the shape below it or to its right) is Lit Ink
      uint32_t n;
      if ((x > 0 && (srow[x - 1] >> 24) > 96) || (y > 0 && (srow[x - ss] >> 24) > 96)) orow[x] = kInk;
      else if (y + 1 < h && ((n = srow[x + ss]) >> 24) > 96) orow[x] = litInk(n);   // the shape below: Lit Ink
      else if (x + 1 < w && ((n = srow[x + 1]) >> 24) > 96) orow[x] = litInk(n);    // right of it: Lit Ink
      else continue;
      if (rl < 0) rl = x;
      rr = x;
      // an outline pixel that meets the open air only below or to its right (the foot of a 1 px gap between the
      // torso's outline and an arm held off the body) is a bottom- / right-facing edge of the silhouette: Ink, never
      // Lit Ink
      if (orow[x] != kInk && filled(x - 1, y) && filled(x, y - 1) && (!filled(x + 1, y) || !filled(x, y + 1))) orow[x] = kInk;
    }
    rL[y] = rl; rR[y] = rr;
  }
  // pinholes the outlines left between two parts close with Ink (R5): only a transparent pixel with opaque pixels on
  // both sides in its row can be enclosed; flood those from the ones touching the open outside
  cand.clear();
  for (int y = 1; y < h - 1; y++) {
    const int l = rL[y], r = rR[y];
    if (l < 0) continue;
    const uint32_t* const orow = o + (size_t)y * w;
    for (int x = l + 1; x < r; x++)
      if (!(orow[x] >> 24)) cand.push_back(y * w + x);
  }
  if (!cand.empty()) {
    state.assign((size_t)w * h, 0);   // 2 a candidate, 3 a candidate reached from outside
    uint8_t* const st = state.data();
    for (int i : cand) st[i] = 2;
    auto outside = [&](int x, int y) {   // a transparent pixel outside every row extent (or on the first / last row)
      if (y <= 0 || y >= h - 1) return true;
      const int l = rL[y];
      return l < 0 || x < l || x > rR[y];
    };
    q.clear();
    for (int i : cand) {
      const int x = i % w, y = i / w;
      if (x < 1 || x > w - 2) continue;
      bool seed = false;
      for (int k = 0; k < 4 && !seed; k++) {
        const int nx = x + (k == 0 ? -1 : k == 1 ? 1 : 0), ny = y + (k == 2 ? -1 : k == 3 ? 1 : 0);
        if (!(o[(size_t)ny * w + nx] >> 24) && outside(nx, ny)) seed = true;
      }
      if (seed) { st[i] = 3; q.push_back(i); }
    }
    while (!q.empty()) {
      const int i = q.back();
      q.pop_back();
      for (int j : {i - 1, i + 1, i - w, i + w})
        if (j >= 0 && j < w * h && st[j] == 2) { st[j] = 3; q.push_back(j); }
    }
    for (int i : cand) if (st[i] == 2) o[(size_t)i] = kInk;
  }
  out.px = f.ox - (f.gx0 - 1);
  out.py = f.oy - (f.gy0 - 1);
  out.an = an;
  return out;
}
// a cleared scratch figure (R6: one canvas per thread, reused frame after frame instead of allocated and zeroed anew)
Fig& scratchFig(int W, int H, int pivX, int pivY) {
  thread_local Fig f(1, 1, 0, 0);
  if (f.W != W || f.H != H) f = Fig(W, H, pivX, pivY);
  else {
    // only the box the last figure painted needs clearing (every pixel a painter sets widens it)
    f.mergeBounds();
    const int x0 = std::max(0, f.gx0), x1 = std::min(f.W - 1, f.gx1), y0 = std::max(0, f.gy0), y1 = std::min(f.H - 1, f.gy1);
    for (int y = y0; y <= y1 && x0 <= x1; y++) {
      std::fill(f.px.begin() + (size_t)y * f.W + x0, f.px.begin() + (size_t)y * f.W + x1 + 1, 0u);
      std::fill(f.lay.begin() + (size_t)y * f.W + x0, f.lay.begin() + (size_t)y * f.W + x1 + 1, (uint8_t)0);
    }
  }
  f.ox = pivX; f.oy = pivY; f.cur = 0; f.legLay[0] = f.legLay[1] = 0;
  f.lx0 = 0; f.ly0 = 0; f.lx1 = -1; f.ly1 = -1;
  f.gx0 = f.gy0 = 1 << 20; f.gx1 = f.gy1 = -1;
  return f;
}
}  // namespace

Canvas paintRaw(const HumanLook& look, const Body& B, const Kit& K, const Pose& P, int W, int H, int feet, RigAnchors& an) {
  Fig& f = scratchFig(W, H, W / 2, feet);
  paintInto(f, look, B, K, P, an);
  Canvas c(W, H);
  c.px = f.px;
  return c;
}

// the ink outer outline (STYLE_P2 8) and the trim to the opaque box, the pivot kept
Cut finishFrame(Canvas c, int pivX, int pivY, const RigAnchors& an) {
  outlineInk(c);
  int x0, y0, x1, y1;
  bounds(c, x0, y0, x1, y1);
  Cut out;
  if (x1 < x0) return out;
  // a pinhole the outlines left between two parts (an arm held a few px off the body) closes with Ink: nothing
  // see-through inside a silhouette (R5)
  {
    const int bw = x1 - x0 + 3, bh = y1 - y0 + 3;   // the box plus a transparent ring
    std::vector<uint8_t> seen((size_t)bw * bh, 0);
    std::vector<int> q;
    auto opaqueAt = [&](int bx, int by) { return (c.get(x0 - 1 + bx, y0 - 1 + by) >> 24) != 0; };
    seen[0] = 1;
    q.push_back(0);
    while (!q.empty()) {
      const int i = q.back();
      q.pop_back();
      const int bx = i % bw, by = i / bw;
      const int nx[4] = {bx - 1, bx + 1, bx, bx}, ny[4] = {by, by, by - 1, by + 1};
      for (int k = 0; k < 4; k++) {
        if (nx[k] < 0 || ny[k] < 0 || nx[k] >= bw || ny[k] >= bh) continue;
        const size_t j = (size_t)ny[k] * bw + nx[k];
        if (seen[j] || opaqueAt(nx[k], ny[k])) continue;
        seen[j] = 1;
        q.push_back((int)j);
      }
    }
    for (int by = 1; by < bh - 1; by++)
      for (int bx = 1; bx < bw - 1; bx++)
        if (!seen[(size_t)by * bw + bx] && !opaqueAt(bx, by)) c.set(x0 - 1 + bx, y0 - 1 + by, kInk);
  }
  out.c = Canvas(x1 - x0 + 1, y1 - y0 + 1);
  for (int y = y0; y <= y1; y++)
    std::memcpy(&out.c.px[(size_t)(y - y0) * out.c.w], &c.px[(size_t)y * c.w + x0], (size_t)out.c.w * 4);
  out.px = pivX - x0;
  out.py = pivY - y0;
  out.an = an;
  return out;
}

Cut paintFrame(const HumanLook& look, const Body& B, const Kit& K, const Pose& P, int W, int H, int feet) {
  RigAnchors an;
  Fig& f = scratchFig(W, H, W / 2, feet);
  paintInto(f, look, B, K, P, an);
  return finishFig(f, an);
}

// ---------------------------------------------------------------- the atlas
namespace {
uint64_t cutHash(const Cut& c) {   // the frame's size, pivot and every 3rd pixel (FNV-1a; a match is confirmed pixel by pixel)
  uint64_t h = 1469598103934665603ull ^ ((uint64_t)(uint16_t)c.c.w << 48 | (uint64_t)(uint16_t)c.c.h << 32 | (uint64_t)(uint16_t)c.px << 16 | (uint16_t)c.py);
  for (size_t i = 0; i < c.c.px.size(); i += 3) { h ^= c.c.px[i]; h *= 1099511628211ull; }
  return h;
}
}  // namespace

int SheetBuilder::add(int facing, Clip c, int variant, Cut&& cut) {
  // an identical frame already painted shares its rect (the hashes first, the pixels only on a match)
  if (hashes.size() != cuts.size()) { hashes.clear(); for (const Cut& o : cuts) hashes.push_back(cutHash(o)); }
  const uint64_t h = cutHash(cut);
  for (size_t i = 0; i < cuts.size(); i++) {
    if (hashes[i] != h) continue;
    const Cut& o = cuts[i];
    if (o.c.w == cut.c.w && o.c.h == cut.c.h && o.px == cut.px && o.py == cut.py && o.c.px == cut.c.px) {
      items.push_back({facing, c, variant, (int)i});
      return (int)i;
    }
  }
  cuts.push_back(std::move(cut));
  hashes.push_back(h);
  items.push_back({facing, c, variant, (int)cuts.size() - 1});
  return (int)cuts.size() - 1;
}

RigSheet SheetBuilder::finish() {
  RigSheet s;
  if (cuts.empty()) return s;
  // shelf packing by height (tallest first) at the width that wastes the least
  std::vector<int> order(cuts.size());
  for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;
  std::sort(order.begin(), order.end(), [&](int a, int b) { return cuts[(size_t)a].c.h != cuts[(size_t)b].c.h ? cuts[(size_t)a].c.h > cuts[(size_t)b].c.h : a < b; });
  int maxW = 0, area = 0;
  for (const Cut& c : cuts) { maxW = std::max(maxW, c.c.w); area += c.c.w * c.c.h; }
  int bestW = 0, bestH = 1 << 30;
  std::vector<int> bx(cuts.size()), by(cuts.size());
  // the sizes in packing order, side by side (R6: the width search walks these a few hundred times)
  std::vector<int> ow(cuts.size()), oh(cuts.size());
  for (size_t k = 0; k < order.size(); k++) { ow[k] = cuts[(size_t)order[k]].c.w; oh[k] = cuts[(size_t)order[k]].c.h; }
  auto pack = [&](int width, bool write) {
    int x = 0, y = 0, shelfH = 0;
    const size_t n = ow.size();
    for (size_t k = 0; k < n; k++) {
      const int cw = ow[k], ch = oh[k];
      if (x + cw > width) { y += shelfH; x = 0; shelfH = 0; }
      if (write) { bx[(size_t)order[k]] = x; by[(size_t)order[k]] = y; }
      x += cw;
      shelfH = std::max(shelfH, ch);
    }
    return y + shelfH;
  };
  // every width (in steps of 2) from the widest frame to 2.5 x the square root of the area: the one that wastes the
  // least (a pack is a few hundred operations; R6 caps the atlas bytes, so the search pays)
  const int root = (int)std::sqrt((double)area);
  for (int w = maxW; w <= std::max(maxW, root * 5 / 2); w += 2) {
    const int h = pack(w, false);
    if ((long long)w * h < (long long)bestW * bestH || bestW == 0) { bestW = w; bestH = h; }
  }
  pack(bestW, true);
  s.atlas = Canvas(bestW, bestH);
  for (size_t i = 0; i < cuts.size(); i++) {
    const Canvas& c = cuts[i].c;
    for (int y = 0; y < c.h; y++)
      std::memcpy(&s.atlas.px[(size_t)(by[i] + y) * bestW + bx[i]], &c.px[(size_t)y * c.w], (size_t)c.w * 4);
  }
  s.frames.reserve(items.size());
  s.anchors.reserve(items.size());
  for (const Item& it : items) {
    const Cut& c = cuts[(size_t)it.src];
    FrameRect r;
    r.x = (int16_t)bx[(size_t)it.src]; r.y = (int16_t)by[(size_t)it.src];
    r.w = (int16_t)c.c.w; r.h = (int16_t)c.c.h;
    r.px = (int16_t)c.px; r.py = (int16_t)c.py;
    s.add(it.facing, it.clip, it.variant, r, c.an);
  }
  return s;
}

}  // namespace rig2
}  // namespace art
