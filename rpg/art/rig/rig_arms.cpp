// EMBERVALE art, v2 rig (RIG-ARMS, P2a): the base weapons rasterized procedurally at 16 angles with the grip and depth
// rules; the greatsword is its own art id 7 (art::WEAPON_GREATSWORD) (STYLE_P2 11).
//
// Every weapon is built along its own axis from the fist: n = 0 is the fist's top-left pixel (the grip runs under the
// fist), n > 0 toward the tip, n < 0 toward the pommel / butt, one pixel per step of the direction's major axis on the
// 16 clean slopes (0, 1:2, 1:1, 2:1 ...: rig_core.cpp's kDir and stepAlong, so the slice's sword and these agree to the
// pixel). Lengths are Euclidean (a diagonal takes fewer steps), so a weapon keeps its length at every angle. Thin parts
// (blades, hafts, shafts) are clean pixel lines: no doubled pixels, no L-shaped jaggies. Heads (the axe's bearded bit,
// the mace's flanges, the spear's leaf, the staff's gem, the greatsword's quillons) are small shapes laid on the same
// lattice (their edges run on the clean slopes) and lit afterwards from their own silhouette: the top-left boundary
// catches the light, the bottom-right falls into shadow, the cutting edge is the brightest metal, whatever the angle.
//
//   art id  weapon        length on a 32 px figure (STYLE_P2 11)        grip
//   5       dagger        blade 5 + hilt 3                              one hand
//   1       sword         blade 11 + hilt 4 (rig_core.cpp paintWeapon)  one hand, guard and pommel either side
//   7       greatsword    blade 18 + hilt 6, 3 px at the ricasso        two hands 3 px apart
//   2       axe           haft 13, bearded head 5 x 6                   one hand (two for the finisher)
//   6       mace          haft 11, flanged head 4 x 5                   one hand (two for the finisher)
//   8       spear         shaft 34, leaf head 5                         two hands 7 px apart
//   4       staff         30, a 3 x 3 gem head                          two hands 7 px apart
//   9       walking stick 24, a crook                                   one hand
//   3       bow           22 stave, a string, the arrow                 the off hand holds it, the right hand draws
#include <algorithm>
#include <cmath>
#include "rpg/art/rig/rig_internal.h"

namespace art {
namespace rig2 {

namespace {

// rig_core.cpp's 16 directions and stepping (kept identical: the slice's sword is painted by rig_core.cpp)
const int kDir[16][2] = {{2, 0}, {2, 1}, {2, 2}, {1, 2}, {0, 2}, {-1, 2}, {-2, 2}, {-2, 1},
                         {-2, 0}, {-2, -1}, {-2, -2}, {-1, -2}, {0, -2}, {1, -2}, {2, -2}, {2, -1}};
void stepAlong(int d, int n, int& x, int& y) {
  const int vx = kDir[d & 15][0], vy = kDir[d & 15][1];
  const int ax = std::abs(vx), ay = std::abs(vy);
  if (ax >= ay) { x = n * (vx > 0 ? 1 : -1); y = ay == 0 ? 0 : (int)std::floor((n * ay + ax / 2) / (float)ax) * (vy > 0 ? 1 : -1); }
  else { y = n * (vy > 0 ? 1 : -1); x = ax == 0 ? 0 : (int)std::floor((n * ax + ay / 2) / (float)ay) * (vx > 0 ? 1 : -1); }
}
// signed: n < 0 steps backward along the axis (the pommel side), mirrored exactly
void at(int d, int n, int& x, int& y) {
  if (n >= 0) stepAlong(d, n, x, y);
  else stepAlong((d + 8) & 15, -n, x, y);
}
float perStep(int d) {   // euclidean px per major-axis step
  const int vx = kDir[d & 15][0], vy = kDir[d & 15][1];
  return std::sqrt((float)(vx * vx + vy * vy)) / (float)std::max(std::abs(vx), std::abs(vy));
}
int steps(int d, float px) { return std::max(1, (int)std::lround(px / perStep(d))); }

// the second column / row of a 2 px line (the sword's rule): steep lines widen in x, shallow ones in y, diagonals on
// the side that keeps the pixels edge-joined
void widen(int d, int& ex, int& ey) {
  const int vx = kDir[d & 15][0], vy = kDir[d & 15][1];
  ex = 0; ey = 0;
  if (std::abs(vy) > std::abs(vx)) ex = 1; else ey = 1;
  if (std::abs(vx) == std::abs(vy)) { ex = vx * vy > 0 ? 1 : 0; ey = vx * vy > 0 ? 0 : 1; }
}
// is the offset (ex, ey) side the lit one (its outward normal faces up-left)?
bool litSide(int ex, int ey) { return (ex * -0.56f + ey * -0.62f) > 0; }

// ---------------------------------------------------------------- a head: a small shape on the weapon's lattice
// U runs along the weapon from the head's anchor pixel (+ toward the tip), V across it (+ toward the edge side: the
// clockwise side of the direction when edge = +1). The shape gives each pixel a role; the light is resolved from the
// finished silhouette, so it stays top-left at every angle.
enum Role : uint8_t { RNone, RMetal, REdge, RDark, RBrass, RWood, RGem, RGemCore };
struct Px { int x, y; uint8_t role; };
struct Head {
  std::vector<Px> px;
  bool has(int x, int y) const {
    for (const Px& p : px) if (p.x == x && p.y == y) return true;
    return false;
  }
};
template <class F>
void shapeOn(int d, int edge, int ax, int ay, float uMin, float uMax, float vMin, float vMax, F role, Head& h) {
  const float ux0 = (float)kDir[d & 15][0], uy0 = (float)kDir[d & 15][1];
  const float ul = std::sqrt(ux0 * ux0 + uy0 * uy0), ux = ux0 / ul, uy = uy0 / ul;
  const float vx = -uy * (float)edge, vy = ux * (float)edge;   // the clockwise perpendicular (edge +1)
  const int R = (int)std::ceil(std::max(std::max(std::fabs(uMin), std::fabs(uMax)), std::max(std::fabs(vMin), std::fabs(vMax)))) + 2;
  for (int y = ay - R; y <= ay + R; y++)
    for (int x = ax - R; x <= ax + R; x++) {
      const float dx = (float)(x - ax), dy = (float)(y - ay);
      const float U = dx * ux + dy * uy, V = dx * vx + dy * vy;
      if (U < uMin - 0.5f || U > uMax + 0.5f || V < vMin - 0.5f || V > vMax + 0.5f) continue;
      const uint8_t r = role(U, V);
      if (r) h.px.push_back({x, y, r});
    }
}
// spurs (a pixel touching the shape by one side only) go: no orphan pixels, no jaggies (STYLE_P2 7)
void prune(Head& h) {
  std::vector<Px> keep;
  for (const Px& p : h.px) {
    int n = 0;
    n += h.has(p.x - 1, p.y); n += h.has(p.x + 1, p.y); n += h.has(p.x, p.y - 1); n += h.has(p.x, p.y + 1);
    if (n >= 2 || p.role == REdge || p.role == RGemCore) keep.push_back(p);
  }
  h.px.swap(keep);
}
void inkHead(Fig& f, const Kit& K, const Head& h, const Ramp5& gem) {
  for (const Px& p : h.px) {
    const bool lit = !h.has(p.x - 1, p.y) || !h.has(p.x, p.y - 1);
    const bool dark = !h.has(p.x + 1, p.y) || !h.has(p.x, p.y + 1);
    uint32_t c = 0;
    switch (p.role) {
      case RMetal: c = K.metal[lit && !dark ? 3 : dark && !lit ? 1 : 2]; break;
      case REdge: c = K.metal[lit ? 4 : 3]; break;
      case RDark: c = K.metal[lit ? 1 : 0]; break;
      case RBrass: c = K.guard[lit && !dark ? 3 : dark && !lit ? 1 : 2]; break;
      case RWood: c = kWood[lit ? 3 : dark ? 1 : 2]; break;
      case RGem: c = gem[lit && !dark ? 3 : dark && !lit ? 1 : 2]; break;
      case RGemCore: c = gem[4]; break;
      default: break;
    }
    if (c) f.set(p.x, p.y, c);
  }
}

// a 1 px haft / shaft from n0 to n1 (inclusive) in wood, a darker wrap from w0 to w1 (the grip's binding)
void haft(Fig& f, int d, int gx, int gy, int n0, int n1, int w0, int w1, const Ramp5& wood) {
  for (int n = n0; n <= n1; n++) {
    int x, y;
    at(d, n, x, y);
    const bool wrap = n >= w0 && n <= w1;
    f.set(gx + x, gy + y, wrap ? kLeather[1] : wood[n == n1 ? 3 : 2]);
  }
}
// a blade from n0 (at the guard) to the tip n1: `wide` px wide at the base (1..3), narrowing over its last third;
// the lit edge and the shadow edge, a specular pixel near the guard, the bright tip (STYLE_P2 7, 11)
void blade(Fig& f, const Kit& K, int d, int gx, int gy, int n0, int n1, int wide) {
  int ex, ey;
  widen(d, ex, ey);
  const bool offLit = litSide(ex, ey);
  const int len = std::max(1, n1 - n0 + 1);
  for (int n = n0; n <= n1; n++) {
    int x, y;
    at(d, n, x, y);
    const int bx = gx + x, by = gy + y;
    const int k = n - n0;
    const bool tip = n == n1;
    int w = wide;
    if (k >= len - std::max(1, len / 3)) w = std::min(w, 2);   // the point narrows
    if (tip) w = 1;
    // the spine line (the main pixel line), the second edge, and (3 px) the third edge on the far side of the spine
    f.set(bx, by, tip ? K.metal[4] : (offLit ? K.metal[1] : K.metal[3]));
    if (w >= 2) f.set(bx + ex, by + ey, offLit ? K.metal[3] : K.metal[1]);
    if (w >= 3) {   // the broad ricasso: the spine becomes the fuller (one ramp lower), lit / shadow edges either side
      f.set(bx, by, K.metal[2]);
      f.set(bx - ex, by - ey, offLit ? K.metal[1] : K.metal[3]);
      f.set(bx + ex, by + ey, offLit ? K.metal[3] : K.metal[1]);
    }
    if (k == 1) f.set(offLit ? bx + ex : bx, offLit ? by + ey : by, K.metal[4]);   // the specular near the guard
  }
}
// a guard (crossguard) `half` px either side of the axis at step n (brass, lit on its top-left)
void crossguard(Fig& f, const Kit& K, int d, int gx, int gy, int n, int lo, int hi) {
  int cx, cy;
  at(d, n, cx, cy);
  const int vx = kDir[d & 15][0], vy = kDir[d & 15][1];
  const bool steep = std::abs(vy) > std::abs(vx);
  for (int k = lo; k <= hi; k++) {
    int ox = steep ? k : 0, oy = steep ? 0 : k;
    if (std::abs(vx) == std::abs(vy)) { ox = k; oy = vx * vy > 0 ? -k : k; }
    const bool end = k == lo || k == hi;
    f.set(gx + cx + ox, gy + cy + oy, K.guard[k < 0 ? (end ? 2 : 3) : (end ? 1 : 2)]);
  }
}

}  // namespace

bool armsDrawn(int weapon) { return weapon == 1 || weapon == 2 || weapon == 4 || weapon == 5 || weapon == 6 || weapon == 7 || weapon == 8 || weapon == 9; }

int armsTwoHandGap(int weapon) {
  switch (weapon) {
    case 7: return 3;            // greatsword: the rear hand 3 px down the grip
    case 4: case 8: return 7;    // staff, spear: 6-8 px down the shaft
    case 2: case 6: return 3;    // axe, mace held in both hands (the finishers)
    default: return 0;
  }
}

void paintArms(Fig& f, const Kit& K, int weapon, float gxF, float gyF, int dir, int shorten, int edge, int& tipX, int& tipY) {
  tipX = (int)std::floor(gxF); tipY = (int)std::floor(gyF);
  if (weapon == 1) { paintWeapon(f, K, 1, gxF, gyF, dir, shorten, tipX, tipY); return; }   // the slice's sword, to the pixel
  const int d = dir & 15;
  const int gx = (int)std::floor(gxF), gy = (int)std::floor(gyF);
  edge = edge < 0 ? -1 : 1;
  // foreshortening (toward or away from the viewer): the parts ahead of the fist lose `shorten` px per 11 of length
  auto ahead = [&](float px) { return std::max(2.0f, px - shorten * px / 11.0f); };
  Head head;
  static const Ramp5 wood = r5(kWood), woodDark = r5(kWoodDark);
  switch (weapon) {
    case 5: {   // dagger: pommel, grip under the fist, a short 3 px guard, a 5 px blade broad at its base
      int x, y;
      at(d, -1, x, y);
      f.set(gx + x, gy + y, K.guard[3]);
      for (int n = 0; n <= 1; n++) { at(d, n, x, y); f.set(gx + x, gy + y, K.belt[1]); }
      crossguard(f, K, d, gx, gy, 2, -1, 1);
      const int b = steps(d, ahead(5.0f));
      blade(f, K, d, gx, gy, 3, 2 + b, 2);
      at(d, 2 + b, tipX, tipY); tipX += gx; tipY += gy;
      break;
    }
    case 7: {   // greatsword: a long two-hand grip, a 6 px guard with dropped quillons, an 18 px blade 3 px at the ricasso
      int x, y;
      at(d, -4, x, y);
      f.set(gx + x, gy + y, K.guard[3]);
      {   // the pommel: a round 2 px knob across the axis
        int ex, ey;
        widen(d, ex, ey);
        f.set(gx + x + ex, gy + y + ey, K.guard[1]);
      }
      for (int n = -3; n <= 1; n++) { at(d, n, x, y); f.set(gx + x, gy + y, n == -1 ? K.belt[2] : K.belt[1]); }
      crossguard(f, K, d, gx, gy, 2, -3, 2);
      const int b = steps(d, ahead(18.0f));
      blade(f, K, d, gx, gy, 3, 2 + b, 3);
      at(d, 2 + b, tipX, tipY); tipX += gx; tipY += gy;
      break;
    }
    case 2: {   // axe: a 13 px haft (a knob at the butt), the bearded head at its end, the bit on the edge side
      const int top = steps(d, ahead(10.0f));
      haft(f, d, gx, gy, -2, top, -2, -2, wood);
      int hx, hy;
      at(d, top, hx, hy);
      hx += gx; hy += gy;
      // the head in (U along toward the tip, V across toward the bit): the eye wraps the haft's last 3 px (and a 1 px
      // poll behind it), the blade runs out 5 px to the bit; its top edge is square with the haft's end, its bottom
      // sweeps back down the haft into the beard; the cutting edge is the outermost column
      shapeOn(d, edge, hx, hy, -6.5f, 2.5f, -1.4f, 6.4f, [](float U, float V) -> uint8_t {
        if (V < -1.5f) return RNone;
        if (V < -0.5f) return (U > -1.5f && U < 0.6f) ? RDark : RNone;                // the poll behind the haft
        if (V < 0.5f) return (U > -2.5f && U < 0.6f) ? RDark : RNone;                 // the eye round the haft
        // the bit: a short neck from the eye, then a fan whose toe lifts a little above the haft's end and whose beard
        // sweeps down the haft; the cutting edge is a convex arc, the brightest metal
        const float top = 0.5f + std::max(0.0f, V - 2.5f) * 0.4f;
        const float bottom = -1.5f - std::max(0.0f, V - 1.5f) * 1.05f;
        if (U > top || U < bottom) return RNone;
        const float mid = (top + bottom) * 0.5f, half = std::max(0.5f, (top - bottom) * 0.5f);
        const float q = (U - mid) / half;
        const float edgeV = 5.7f - 1.4f * q * q;
        if (V > edgeV + 0.5f) return RNone;
        return V > edgeV - 0.6f ? REdge : RMetal;
      }, head);
      prune(head);
      inkHead(f, K, head, K.metal);
      int cx, cy;
      at(d, top - 1, cx, cy);
      tipX = gx + cx; tipY = gy + cy;
      {   // the tip anchor: the bit's middle (2.5 px out on the edge side)
        const float ux0 = (float)kDir[d][0], uy0 = (float)kDir[d][1], ul = std::sqrt(ux0 * ux0 + uy0 * uy0);
        tipX += (int)std::lround(-uy0 / ul * edge * 2.5f); tipY += (int)std::lround(ux0 / ul * edge * 2.5f);
      }
      break;
    }
    case 6: {   // mace: an 11 px haft, the flanged head 4 x 5 on its end, a knob atop
      const int top = steps(d, ahead(8.0f));
      haft(f, d, gx, gy, -2, top - 1, -2, -2, wood);
      int hx, hy;
      at(d, top, hx, hy);
      hx += gx; hy += gy;
      shapeOn(d, 1, hx, hy, -1.0f, 4.0f, -2.6f, 2.6f, [](float U, float V) -> uint8_t {
        const float a = std::fabs(V);
        if (U > 3.5f) return a < 0.6f ? REdge : RNone;            // the knob atop
        if (U < -0.5f) return a < 0.6f ? RDark : RNone;           // the collar under the head
        if (a > 2.5f) return RNone;
        if (a > 1.5f) return (U > 0.5f && U < 2.5f) ? REdge : RNone;   // the side flanges, proud of the head, catching the light
        if (U > 2.5f && a > 0.5f) return RNone;                   // the head's rounded top
        return a > 0.5f ? RDark : RMetal;                         // the grooves either side of the middle flange
      }, head);
      prune(head);
      inkHead(f, K, head, K.metal);
      int cx, cy;
      at(d, top + 1, cx, cy);
      tipX = gx + cx; tipY = gy + cy;
      break;
    }
    case 8: {   // spear: a 34 px ash shaft (an iron ferrule at the butt), a socket ring, a 5 px leaf head
      const int back = steps(d, 12.0f), fwd = steps(d, ahead(16.0f));
      haft(f, d, gx, gy, -back, fwd, 9999, 9999, wood);
      int x, y;
      at(d, -back, x, y);
      f.set(gx + x, gy + y, kIron[2]);
      at(d, fwd + 1, x, y);
      f.set(gx + x, gy + y, kIron[1]);
      const int hl = steps(d, ahead(6.0f));
      int hx, hy;
      at(d, fwd + 2, hx, hy);
      hx += gx; hy += gy;
      const float L = (float)hl * perStep(d);
      shapeOn(d, 1, hx, hy, 0.0f, L - 0.5f, -1.2f, 1.2f, [L](float U, float V) -> uint8_t {
        const float t = U / std::max(1.0f, L - 1.0f);   // 0 at the socket .. 1 at the point
        const float half = t < 0.4f ? 0.55f + t * 2.0f : 1.35f * (1.0f - t) / 0.6f;   // the leaf swells, then points
        if (std::fabs(V) > half + 0.15f) return RNone;
        if (t > 0.92f) return REdge;
        return V < -0.2f ? REdge : RMetal;   // the head's spine: one edge bright
      }, head);
      // the leaf's own line always shows (a 1 px point even where the shape thins away)
      for (int n = 0; n < hl; n++) {
        int qx, qy;
        at(d, fwd + 2 + n, qx, qy);
        if (!head.has(gx + qx, gy + qy)) head.px.push_back({gx + qx, gy + qy, n == hl - 1 ? (uint8_t)REdge : (uint8_t)RMetal});
      }
      prune(head);
      inkHead(f, K, head, K.metal);
      at(d, fwd + 1 + hl, tipX, tipY); tipX += gx; tipY += gy;
      break;
    }
    case 4: {   // staff: a 30 px stave, a 3 x 3 gem in a wooden cup at its head
      const int back = steps(d, 12.0f), fwd = steps(d, ahead(14.0f));
      haft(f, d, gx, gy, -back, fwd, 9999, 9999, wood);
      int hx, hy;
      at(d, fwd + 2, hx, hy);
      hx += gx; hy += gy;
      static const Ramp5 gem = r5(ramp(rgba(120, 196, 250)));
      shapeOn(d, 1, hx, hy, -1.6f, 1.6f, -1.6f, 1.6f, [](float U, float V) -> uint8_t {
        const float r2 = U * U + V * V;
        if (r2 > 2.6f) return U < -0.8f && std::fabs(V) < 2.1f ? RWood : RNone;   // the cup's prongs round the gem
        return (U > 0.2f && V < -0.2f) ? RGemCore : RGem;
      }, head);
      prune(head);
      inkHead(f, K, head, gem);
      tipX = hx; tipY = hy;
      break;
    }
    case 9: {   // walking stick: 24 px, held just under its crook; the long end leads (planted at rest, swung in a fight)
      const int back = 2, fwd = steps(d, ahead(21.0f));
      haft(f, d, gx, gy, -back, fwd, 9999, 9999, woodDark);
      int hx, hy;
      at(d, -back, hx, hy);
      hx += gx; hy += gy;
      // the crook: from the stick's top end, over (U -1..-2 behind it) and down the other side 2 px
      shapeOn(d, 1, hx, hy, -1.6f, 2.6f, 0.0f, 3.4f, [](float U, float V) -> uint8_t {
        if (U < -0.4f && U > -1.6f && V < 3.1f) return RWood;               // over the top
        if (V > 2.4f && V < 3.5f && U > -1.6f && U < 2.1f) return RWood;     // down the far side
        return RNone;
      }, head);
      prune(head);
      for (const Px& p : head.px) {
        const bool lit = !head.has(p.x - 1, p.y) || !head.has(p.x, p.y - 1);
        f.set(p.x, p.y, woodDark[lit ? 3 : 1]);
      }
      at(d, fwd, tipX, tipY); tipX += gx; tipY += gy;
      break;
    }
    default: break;
  }
}

// ---------------------------------------------------------------- at rest (STYLE_P2 9, 11)
// How a fighter carries each weapon standing and walking (rig_body.cpp restWeapon is the sword's: upright before the
// weapon shoulder facing down and up, hanging forward past the body side-on): one-handed arms like the sword; the
// greatsword point-down before the feet side-on; the spear and staff upright, butt on the ground; the walking stick
// planted. Facing left the weapon is in the far hand behind the body, at least 4 px of it showing beyond the body.
void armsRest(const Body& B, int weapon, Pose& p) {
  const int sh = B.shoulder + 3, hip = B.hip + 1;
  const bool pole = weapon == 4 || weapon == 8;
  switch (p.facing) {
    case FDown:
      p.hRx = -8; p.hRy = (int8_t)(pole ? hip - 4 : sh); p.wDir = 12;
      if (weapon == 9) { p.hRy = (int8_t)(hip - 3); p.wDir = 4; }
      // an axe or a mace rests back on the weapon shoulder, its head up beside the head (never over the face; the
      // axe's bit faces up and away)
      if (weapon == 2 || weapon == 6) { p.hRx = -8; p.hRy = (int8_t)(sh + 1); p.wDir = 10; p.wBehind = 1; }
      break;
    case FUp:
      p.hRx = 6; p.hRy = (int8_t)(pole ? hip - 4 : sh); p.wDir = 12; p.wBehind = 0;
      if (weapon == 9) { p.hRy = (int8_t)(hip - 3); p.wDir = 4; }
      break;
    case FRight:
      p.hRx = 0; p.hRy = (int8_t)hip; p.wDir = weapon == 7 ? 1 : 1; p.wBehind = 0;
      if (pole) { p.hRx = 5; p.hRy = (int8_t)(hip - 4); p.wDir = 12; }   // upright before the nose, clear of the face
      if (weapon == 9) { p.hRx = 3; p.hRy = (int8_t)(hip - 3); p.wDir = 4; }
      if (weapon == 2 || weapon == 6) p.wDir = 2;
      break;
    case FLeft:
      p.hRx = -5; p.hRy = (int8_t)hip; p.wDir = 7; p.wBehind = 1; p.hRfront = 0;
      if (pole) { p.hRx = 2; p.hRy = (int8_t)(hip - 4); p.wDir = 12; }   // upright behind the back, the head over the crown
      if (weapon == 9) { p.hRx = -5; p.hRy = (int8_t)(hip - 3); p.wDir = 4; }
      if (weapon == 2 || weapon == 6) p.wDir = 6;
      break;
  }
}

// ---------------------------------------------------------------- the bow (STYLE_P2 10, 11)
// The stave is a shallow arc across the aim through the bow hand (the off hand's fist), its belly toward the archer;
// the string runs from tip to tip, pulled back to the string hand while drawn; the arrow lies on the string, its head
// forward of the bow. draw: 0 braced (the string straight, the arrow nocked), 1 half drawn, 2 full (at the cheek),
// 3 loosed (the string straight and quivering, no arrow). aim 0..15 as the weapons; half = the stave's half length
// along the cross axis (foreshortened views pass less); arrowLen the arrow's px along the aim.
void paintBow(Fig& f, const Kit& K, int bowX, int bowY, int aim, int half, int stringX, int stringY, int draw, int arrowLen, bool arrowFirst, int& tipX, int& tipY) {
  const float ax0 = (float)kDir[aim & 15][0], ay0 = (float)kDir[aim & 15][1], al = std::sqrt(ax0 * ax0 + ay0 * ay0);
  const float ux = ax0 / al, uy = ay0 / al;   // toward the target
  const float vx = -uy, vy = ux;              // across
  const float cx = bowX + 1.0f, cy = bowY + 1.0f;   // the fist's centre: the stave passes under it
  const float bend = draw == 2 ? 2.6f : draw == 1 ? 2.0f : 1.4f;   // the tips pulled back toward the archer
  auto stave = [&](float s, float& x, float& y) {   // s -1 .. 1 along the stave
    const float back = bend * s * s;
    x = cx + vx * s * half - ux * back;
    y = cy + vy * s * half - uy * back;
  };
  const Ramp5 wood = r5(kWood);
  // the arrow first when it lies behind the stave (the archer's side), so the stave crosses it
  auto drawArrow = [&]() {
    if (draw >= 3 || arrowLen <= 0) return;
    // the arrow's nock at the string hand when drawn, at the string when braced; it points along the aim
    const float nx = draw == 0 ? cx - ux * 1.8f : stringX + 1.0f, ny = draw == 0 ? cy - uy * 1.8f : stringY + 1.0f;
    const int n = arrowLen;
    for (int k = 0; k < n; k++) {
      const int x = (int)std::floor(nx + ux * k), y = (int)std::floor(ny + uy * k);
      uint32_t c = k < 2 ? rgba(214, 206, 188) : kWood[3];          // the fletching, the shaft
      if (k >= n - 2) c = k == n - 1 ? K.metal[4] : K.metal[2];      // the iron head
      f.set(x, y, c);
    }
    tipX = (int)std::floor(nx + ux * (n - 1)); tipY = (int)std::floor(ny + uy * (n - 1));
  };
  if (arrowFirst) drawArrow();
  // the string (before the stave, so the stave's tips sit over it): from each tip to the string hand (or straight)
  float t0x, t0y, t1x, t1y;
  stave(-1.0f, t0x, t0y);
  stave(1.0f, t1x, t1y);
  const uint32_t str = rgba(232, 224, 200);
  auto line = [&](float x0, float y0, float x1, float y1) {
    const int n = (int)std::ceil(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0)));
    for (int k = 0; k <= n; k++) {
      const float t = n ? (float)k / n : 0.0f;
      f.set((int)std::floor(x0 + (x1 - x0) * t), (int)std::floor(y0 + (y1 - y0) * t), str);
    }
  };
  if (draw == 1 || draw == 2) {
    const float sx = stringX + 1.0f, sy = stringY + 1.0f;
    line(t0x, t0y, sx, sy);
    line(t1x, t1y, sx, sy);
  } else {
    // braced or loosed: straight from tip to tip (loosed: one px of quiver bowed toward the target at the middle)
    const float q = draw == 3 ? 0.8f : 0.0f;
    const float mx = (t0x + t1x) * 0.5f + ux * q, my = (t0y + t1y) * 0.5f + uy * q;
    line(t0x, t0y, mx, my);
    line(mx, my, t1x, t1y);
  }
  // the stave: one px of light wood (its ink outline makes it a 3 px band, so it is painted a step lighter than a haft),
  // the half that faces the top-left light lighter still, a 2 px grip under the fist, the tips' nocks a shade darker
  const int segs = half * 3;
  for (int k = 0; k <= segs; k++) {
    const float s = -1.0f + 2.0f * k / segs;
    float x, y;
    stave(s, x, y);
    const bool tipPx = std::fabs(s) > 0.92f;
    const float lit = -(vx * s) * 0.56f - (vy * s) * 0.62f;   // which half of the stave faces the light
    f.set((int)std::floor(x), (int)std::floor(y), tipPx ? wood[2] : lit > 0 ? wood[4] : wood[3]);
    if (std::fabs(s) < 0.15f) f.set((int)std::floor(x - ux), (int)std::floor(y - uy), kLeather[2]);   // the leather grip
  }
  if (!arrowFirst) drawArrow();
  if (draw >= 3 || arrowLen <= 0) { tipX = (int)std::floor(cx + ux * 2); tipY = (int)std::floor(cy + uy * 2); }
}

}  // namespace rig2
}  // namespace art
