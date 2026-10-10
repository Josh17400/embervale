// EMBERVALE art, M7 "Home" (rpg/art/art_home.h): crops, ground edits, yard objects, the building site, the riding horse,
// trophies, paintings and the FOR SALE sign. ART lane.
//
// Everything here keeps the game's camera: the high oblique 3/4 view (a thing's height rises up the screen 1 px per px,
// its ground depth runs up the screen too), light from the top-left and a little above, a 1 px dark outline (a darkened
// neighbour colour, never pure black), 3-4 tone shading from the shared ramps (art_internal.h). No ground shadows (the
// renderer draws them). Yard buildings are small volumes: a lit roof plane and a shaded one, the gable or the front wall
// in the culture's own material, a shaded east side giving them thickness, a plinth or sill where they meet the ground.
// Runs of fence, path and farmland join their neighbours seamlessly (see the fence notes below).
//
// Canvas sizes are read by the view through the functions (farmObjW / H, CROP_W / H, scaffoldSprite's canvas); every
// sprite stands on its canvas's bottom row like the phase A stand-ins did.
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <vector>
#include "rpg/art/art_internal.h"

namespace art {

namespace {

// ================================================================ small painting helpers
inline uint32_t mixc(uint32_t a, uint32_t b, float t) { return mix(opaque(a), opaque(b), t); }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline Vec2 V2(float x, float y) { return Vec2{x, y}; }

// a ramp shifted toward a colour (wilting, weathering) keeping its value order
Ramp tintRamp(const Ramp& r, uint32_t to, float t) {
  Ramp o;
  for (int i = 0; i < 5; i++) o.c[i] = mixc(r.c[i], to, t);
  return o;
}

// A tapered, shaded stroke along a quadratic curve a -> (ctrl) -> b, its radius r0 at a and r1 at b: stalks, leaves,
// ears, vines. Each stamp is lit on its upper-left side and shaded on its lower-right, so a blade reads round.
void tube(Canvas& c, Vec2 a, Vec2 m, Vec2 b, float r0, float r1, const Ramp& R, int k, float rMid = -1.0f) {
  const float L = len(m - a) + len(b - m);
  const int n = std::max(2, (int)std::ceil(L * 2.5f));
  for (int i = 0; i <= n; i++) {
    const float t = (float)i / n, u = 1 - t;
    const Vec2 p{u * u * a.x + 2 * u * t * m.x + t * t * b.x, u * u * a.y + 2 * u * t * m.y + t * t * b.y};
    float r = rMid < 0 ? lerpf(r0, r1, t) : (t < 0.5f ? lerpf(r0, rMid, t * 2) : lerpf(rMid, r1, (t - 0.5f) * 2));
    if (r < 0.5f) { c.set((int)std::floor(p.x), (int)std::floor(p.y), R[k]); continue; }
    for (int y = (int)std::floor(p.y - r); y <= (int)std::ceil(p.y + r); y++)
      for (int x = (int)std::floor(p.x - r); x <= (int)std::ceil(p.x + r); x++) {
        const float dx = x + 0.5f - p.x, dy = y + 0.5f - p.y;
        if (dx * dx + dy * dy > r * r + 0.1f) continue;
        const float s = (dx * -0.67f + dy * -0.74f) / std::max(0.6f, r);
        c.set(x, y, R[k + (s > 0.35f ? 1 : (s < -0.45f ? -1 : 0))]);
      }
  }
}
inline void tube2(Canvas& c, Vec2 a, Vec2 b, float r0, float r1, const Ramp& R, int k) { tube(c, a, (a + b) * 0.5f, b, r0, r1, R, k); }

// a leaf from its base along `ang` (radians, screen: -PI/2 straight up), `len` long, `w` at its widest, its tip pulled
// down by `droop` (gravity), a darker midrib when wide enough
void leaf(Canvas& c, Vec2 base, float ang, float L, float w, float droop, const Ramp& R, int k, bool rib = true) {
  const Vec2 d{std::cos(ang), std::sin(ang)};
  const Vec2 m = base + d * (L * 0.55f) + Vec2{0, droop * L * 0.15f};
  const Vec2 tip = base + d * L + Vec2{0, droop * L * 0.6f};
  tube(c, base, m, tip, 0.45f, 0.4f, R, k, w);
  if (rib && w >= 1.4f) {
    const int n = (int)std::ceil(L * 2);
    for (int i = n / 5; i <= n * 4 / 5; i++) {
      const float t = (float)i / n, u = 1 - t;
      const Vec2 p{u * u * base.x + 2 * u * t * m.x + t * t * tip.x, u * u * base.y + 2 * u * t * m.y + t * t * tip.y};
      c.set((int)std::floor(p.x), (int)std::floor(p.y), R[std::max(0, k - 1)]);
    }
  }
}

// ================================================================ crops
// Ramps of the farm: young growth, ripening straw, the crops' own colours
const Ramp kSprout = ramp5(rgba(30, 64, 54), rgba(52, 106, 58), rgba(86, 150, 60), rgba(134, 190, 70), rgba(194, 224, 108));
const Ramp kBlueLeaf = ramp5(rgba(28, 58, 70), rgba(52, 96, 92), rgba(86, 140, 118), rgba(130, 180, 146), rgba(186, 216, 182));
const Ramp kStraw = ramp5(rgba(110, 76, 40), rgba(164, 120, 52), rgba(208, 164, 70), rgba(234, 202, 104), rgba(250, 232, 158));
const Ramp kWheatEar = ramp5(rgba(122, 72, 34), rgba(182, 118, 44), rgba(222, 164, 62), rgba(244, 204, 96), rgba(255, 236, 160));
const Ramp kBarleyEar = ramp5(rgba(120, 90, 50), rgba(176, 144, 76), rgba(214, 188, 108), rgba(236, 216, 146), rgba(250, 240, 196));
const Ramp kRyeEar = ramp5(rgba(84, 70, 60), rgba(132, 116, 88), rgba(176, 158, 110), rgba(206, 192, 142), rgba(232, 224, 186));
const Ramp kOatEar = ramp5(rgba(118, 98, 64), rgba(170, 152, 102), rgba(208, 196, 144), rgba(230, 222, 178), rgba(248, 244, 214));
const Ramp kRiceEar = ramp5(rgba(126, 86, 36), rgba(186, 136, 50), rgba(222, 182, 76), rgba(240, 214, 118), rgba(252, 238, 174));
const Ramp kYellowing = ramp5(rgba(84, 78, 40), rgba(132, 128, 52), rgba(180, 170, 68), rgba(214, 204, 96), rgba(238, 230, 146));
const Ramp kPotato = ramp5(rgba(98, 64, 46), rgba(146, 104, 66), rgba(190, 146, 96), rgba(220, 184, 132), rgba(240, 214, 170));
const Ramp kTurnip = ramp5(rgba(88, 40, 82), rgba(138, 66, 118), rgba(184, 104, 156), rgba(226, 196, 214), rgba(248, 240, 236));
const Ramp kCabbage = ramp5(rgba(44, 80, 76), rgba(84, 132, 104), rgba(136, 184, 128), rgba(186, 220, 160), rgba(226, 244, 200));
const Ramp kCarrot = ramp5(rgba(130, 46, 30), rgba(194, 84, 34), rgba(236, 128, 44), rgba(250, 170, 76), rgba(255, 212, 132));
const Ramp kOnion = ramp5(rgba(110, 62, 34), rgba(166, 102, 46), rgba(206, 146, 72), rgba(232, 188, 112), rgba(248, 222, 166));
const Ramp kFlaxBlue = ramp5(rgba(44, 52, 120), rgba(70, 90, 176), rgba(104, 136, 222), rgba(150, 182, 244), rgba(212, 228, 255));
const Ramp kPod = ramp5(rgba(36, 70, 48), rgba(60, 110, 56), rgba(98, 156, 66), rgba(146, 196, 84), rgba(200, 230, 130));
const Ramp kGrapeGreen = ramp5(rgba(70, 90, 40), rgba(114, 138, 52), rgba(160, 184, 72), rgba(200, 216, 112), rgba(234, 240, 172));
const Ramp kDate = ramp5(rgba(90, 40, 30), rgba(144, 66, 34), rgba(192, 104, 44), rgba(226, 150, 64), rgba(248, 196, 110));
const Ramp kPalmFrond = ramp5(rgba(30, 60, 50), rgba(52, 100, 56), rgba(90, 140, 60), rgba(140, 178, 74), rgba(196, 214, 112));
const Ramp kCob = ramp5(rgba(140, 90, 30), rgba(206, 146, 34), rgba(240, 196, 60), rgba(252, 226, 110), rgba(255, 246, 176));
const Ramp kHusk = ramp5(rgba(96, 96, 52), rgba(146, 146, 74), rgba(194, 190, 110), rgba(222, 218, 150), rgba(244, 240, 196));
const Ramp kTeaLeaf = ramp5(rgba(16, 44, 40), rgba(28, 74, 50), rgba(46, 108, 60), rgba(80, 146, 70), rgba(150, 198, 96));
const Ramp kLavender = ramp5(rgba(60, 40, 96), rgba(98, 70, 150), rgba(140, 108, 196), rgba(182, 156, 228), rgba(226, 210, 250));
const Ramp kSage = ramp5(rgba(46, 64, 62), rgba(80, 104, 90), rgba(122, 146, 120), rgba(164, 184, 154), rgba(206, 220, 194));
const Ramp kPink = ramp5(rgba(110, 50, 80), rgba(170, 84, 120), rgba(216, 130, 160), rgba(240, 176, 196), rgba(252, 220, 230));
const Ramp kPaddy = ramp5(rgba(30, 50, 78), rgba(48, 84, 110), rgba(74, 122, 138), rgba(118, 168, 172), rgba(196, 226, 220));

// per-tile variety from the view's variant: lean, count, gaps, size
struct CropVar {
  uint32_t v;
  float lean;        // -1 .. 1 the whole clump leans
  int missing;       // a stalk index left out (-1 none)
  float size;        // 0.9 .. 1.08
  uint32_t seed;
};
CropVar cropVar(Crop c, uint32_t v) {
  CropVar r;
  r.v = v;
  const uint32_t h = hash3((int)v, (int)c, 7717u);
  r.lean = ((int)(v % 3) - 1) * 0.55f + ((int)(h % 5) - 2) * 0.12f;
  r.missing = (v & 4) ? (int)(h >> 8) % 7 : -1;
  r.size = 0.92f + (float)((h >> 4) % 5) * 0.035f;
  r.seed = h;
  return r;
}
inline float jit(uint32_t seed, int i, int k, float amp) { return ((int)(hash3(i, k, seed) % 1000) / 1000.0f - 0.5f) * 2 * amp; }

constexpr int kBase = 28;   // the row a crop's stems leave the soil (the tile's row 12: a field's rows overlap upward)

// ---------------------------------------------------------------- grain: wheat, barley, oats, rye (and rice's blades)
void grainCrop(Canvas& c0, Crop k, int st, const CropVar& V, bool wilt) {
  Canvas ears(c0.w, c0.h);   // ears and leaves: outlined like every sprite
  Canvas& c = ears;
  Canvas stalks(c0.w, c0.h); // the thin stalks: a 1 px line lit on one side, never outlined
  // three clumps a tile (two behind, one in front), each a fan of stalks from one root: the tips spread, the heights
  // differ, so a field reads as tufts of grain, never as a palisade
  static const float kH[4] = {4.0f, 9.0f, 13.5f, 15.5f};
  float hMul = 1.0f;
  const Ramp* ear = &kWheatEar;
  const Ramp* straw = &kStraw;
  float bow = 0.45f;      // the ear's angle from the vertical when ripe (radians)
  float earL = 4.0f, earR = 1.15f;
  switch (k) {
    case Crop::Barley: ear = &kBarleyEar; bow = 1.45f; earL = 3.5f; earR = 1.0f; hMul = 0.9f; break;
    case Crop::Oats: ear = &kOatEar; bow = 0.2f; hMul = 0.95f; break;
    case Crop::Rye: ear = &kRyeEar; straw = &kRyeEar; bow = 0.3f; earL = 5.0f; earR = 0.85f; hMul = 1.15f; break;
    default: break;
  }
  const float droop = wilt ? 1.0f : 0.0f;
  struct Clump { float x, y, h; int n; bool back; };
  const float sh = jit(V.seed, 0, 9, 1.0f);
  const Clump cl[3] = {{4.0f + sh, kBase - 3.0f, 0.95f, 3, true}, {12.0f + sh * 0.5f, kBase - 2.0f, 1.04f, 3, true}, {8.0f - sh * 0.5f, kBase + 0.5f, 1.0f, 4, false}};
  for (int ci = 0; ci < 3; ci++) {
    const Clump& C = cl[ci];
    if (V.missing >= 0 && ci == (V.missing % 3) && (V.v & 4) && st >= 2 && ci != 2) continue;   // a gap in the row
    const Ramp& young = C.back ? kLeafDark : kSprout;
    const bool ripe = st == 3;
    const Ramp& stalkR = ripe ? (wilt ? kYellowing : *straw) : young;
    const float hc = C.h * (0.92f + jit(V.seed, ci, 3, 0.08f) + 0.08f);
    if (st == 0) {   // sprouts: a little tuft of blades
      for (int i = 0; i < 3; i++) {
        const float a = -PI / 2 + (i - 1) * 0.45f + jit(V.seed, ci * 7 + i, 4, 0.1f);
        const float L = kH[0] * hc * (i == 1 ? 1.0f : 0.8f);
        tube(c, V2(C.x, C.y), V2(C.x + std::cos(a) * L * 0.5f, C.y + std::sin(a) * L * 0.5f), V2(C.x + std::cos(a) * L, C.y + std::sin(a) * L + droop * 2), 0.5f, 0.45f, young, 3 - (i & 1));
      }
      continue;
    }
    // two leaves arching out from the root
    for (int l = 0; l < (st >= 2 ? 1 : 2); l++) {
      const float side = ((l + ci) & 1) ? 1.0f : -1.0f;
      leaf(c, V2(C.x, C.y - 1), -PI / 2 + side * (1.0f + jit(V.seed, ci * 3 + l, 6, 0.2f)), 3.0f + st * 0.8f, 0.6f, 0.8f + droop, ripe ? *straw : young, ripe ? 1 : (l ? 2 : 3), false);
    }
    for (int i = 0; i < C.n; i++) {
      const float t = C.n > 1 ? (float)i / (C.n - 1) - 0.5f : 0.0f;   // -0.5 .. 0.5 across the fan
      const float spread = t * (0.85f + jit(V.seed, ci * 11 + i, 7, 0.12f)) + V.lean * 0.08f + (wilt ? t * 0.6f : 0.0f);
      const float h = kH[st] * hMul * V.size * hc * (0.86f + std::fabs(jit(V.seed, ci * 11 + i, 3, 0.14f)) + (std::fabs(t) < 0.2f ? 0.1f : 0.0f)) - droop * 2.0f;
      const Vec2 base = V2(C.x + t * 1.5f, C.y);
      const Vec2 tip = base + V2(std::sin(spread) * h, -std::cos(spread) * h);
      line(stalks, (int)std::floor(base.x), (int)std::floor(base.y), (int)std::floor(tip.x), (int)std::floor(tip.y), stalkR[(t < 0 ? 3 : 2) - (C.back ? 1 : 0)]);
      if (st == 1) continue;
      const Ramp& er = ripe ? *ear : young;
      const float dir = spread >= 0 ? 1.0f : -1.0f;
      if (k == Crop::Oats) {   // a loose panicle: spikelets dangling either side of the top
        const Vec2 top = tip + V2(dir * 0.5f, -2.5f);
        tube(c, tip, (tip + top) * 0.5f, top, 0.45f, 0.4f, stalkR, 2);
        for (int s = 0; s < 3; s++) {
          const float sd = (s & 1) ? 1.0f : -1.0f;
          const Vec2 at = tip + (top - tip) * (s / 3.0f);
          const Vec2 sp = at + V2(sd * 1.5f, ripe ? 1.2f : 0.2f);
          c.set((int)std::floor(sp.x), (int)std::floor(sp.y), er[s == 0 ? 4 : 3]);
          c.set((int)std::floor(sp.x), (int)std::floor(sp.y) + 1, er[1]);
        }
        continue;
      }
      // the ear: green and upright while ripening, gold and bowing when ripe; plump kernels lit on one side
      const float b = ripe ? (bow + std::fabs(spread) * 0.6f + (wilt ? 0.6f : 0.0f)) : 0.1f;
      const Vec2 d{std::sin(b) * dir, -std::cos(b)};
      const float L = ripe ? earL : earL * 0.75f;
      // a two-pixel ear: the lit row of kernels, the shaded row, a pointed tip
      const Vec2 perp{-d.y, d.x};
      const float w2 = earR > 1.0f ? 0.6f : 0.45f;
      for (int s = 0; s < (int)L; s++) {
        const Vec2 p = tip + d * (s + 0.5f);
        const Vec2 a1 = p - perp * w2 * dir, a2 = p + perp * w2 * dir;
        c.set((int)std::floor(a1.x), (int)std::floor(a1.y), er[(s & 1) ? 3 : 4]);
        c.set((int)std::floor(a2.x), (int)std::floor(a2.y), er[(s & 1) ? 1 : 2]);
      }
      { const Vec2 p = tip + d * (L + 0.3f); c.set((int)std::floor(p.x), (int)std::floor(p.y), er[3]); }
      if (ripe && (k == Crop::Barley || k == Crop::Rye)) {   // the long awns
        const Vec2 at = tip + d * (L * 0.9f);
        const Vec2 aw = k == Crop::Barley ? at + d * 2.5f + V2(dir * 0.5f, 0.5f) : at + V2(dir * 0.5f, -2.5f);
        line(c, (int)std::floor(at.x), (int)std::floor(at.y), (int)std::floor(aw.x), (int)std::floor(aw.y), er[3]);
      }
    }
  }
  outline(ears, 0.72f);
  for (int y = 0; y < c0.h; y++)
    for (int x = 0; x < c0.w; x++) {
      if (chA(stalks.get(x, y))) c0.set(x, y, stalks.get(x, y));
      if (chA(ears.get(x, y))) c0.set(x, y, ears.get(x, y));
    }
}

// ---------------------------------------------------------------- roots and greens
void rootCrop(Canvas& c, Crop k, int st, const CropVar& V, bool wilt) {
  const float droop = wilt ? 1.0f : 0.0f;
  const float cx = 8.0f + V.lean * 0.6f, by = kBase - 1.0f;
  const Ramp& G = k == Crop::Cabbage || k == Crop::Onion ? kBlueLeaf : (k == Crop::Potato ? kLeafDark : kSprout);
  const bool ripe = st == 3;
  if (st == 0) {   // a seedling: two or three seed leaves
    for (int i = 0; i < 3; i++) {
      if (k == Crop::Onion) { tube(c, V2(cx - 1 + i, by), V2(cx - 1 + i, by - 2), V2(cx - 1.5f + i * 1.3f, by - 4 + droop * 2), 0.5f, 0.4f, G, 2 + (i == 0)); continue; }
      leaf(c, V2(cx, by), -PI / 2 + (i - 1) * 0.9f, 3.0f, 1.0f, 0.3f + droop, G, 2 + (i == 0), false);
    }
    return;
  }
  switch (k) {
    case Crop::Potato: {   // a bushy plant of compound leaves; when ripe it yellows and the tubers show at the soil
      const int nl = 6 + st * 2;
      if (ripe) for (int t = 0; t < 3; t++) ball(c, cx - 4.0f + t * 4.0f + jit(V.seed, t, 9, 0.6f), by + 0.5f - (t == 1 ? 0.6f : 0), 1.8f, 1.3f, kPotato);
      for (int i = 0; i < nl; i++) {
        const float a = -PI / 2 + ((float)i / (nl - 1) - 0.5f) * 2.6f + jit(V.seed, i, 1, 0.15f);
        const float L = (4.0f + st * 1.6f) * V.size * (0.85f + jit(V.seed, i, 2, 0.15f) + 0.15f);
        const Ramp& lr = ripe && (i % 3 == 0) ? kYellowing : G;
        const Vec2 b{cx + jit(V.seed, i, 3, 1.0f), by - 1.0f};
        tube(c, b, b + V2(std::cos(a) * L * 0.5f, std::sin(a) * L * 0.5f), b + V2(std::cos(a) * L, std::sin(a) * L + 1.5f + droop * 2), 0.5f, 0.4f, lr, 1);
        for (int q = 1; q <= 3; q++) {   // leaflets along the stem
          const float t = q / 3.0f;
          const Vec2 p = b + V2(std::cos(a) * L * t, std::sin(a) * L * t + t * t * (1.5f + droop * 2));
          ball(c, p.x, p.y, 1.3f, 1.0f, lr, 0.0f, i < nl / 2 ? 0 : -1);
        }
      }
      if (st >= 2 && !wilt) for (int f = 0; f < 3; f++) { const int fx = (int)cx - 3 + f * 3, fy = (int)by - 6 - st - (f == 1); c.set(fx, fy, kWhite); c.set(fx + 1, fy, rgba(220, 200, 230)); }
      break;
    }
    case Crop::Turnip: case Crop::Carrot: {
      if (ripe) {   // the root's shoulder above the soil
        if (k == Crop::Turnip) { ball(c, cx, by - 0.5f, 3.2f, 2.6f, kTurnip, 0.05f); c.set((int)cx - 1, (int)by - 2, kTurnip[4]); }
        else { ball(c, cx, by, 2.0f, 1.6f, kCarrot, 0.05f); }
      }
      const int nl = k == Crop::Turnip ? 4 + st : 6 + st * 2;
      for (int i = 0; i < nl; i++) {
        const float a = -PI / 2 + ((float)i / (nl - 1) - 0.5f) * 2.2f + jit(V.seed, i, 1, 0.12f);
        const float L = (k == Crop::Turnip ? 4.0f + st * 2.0f : 4.5f + st * 2.0f) * V.size;
        const Vec2 b{cx, by - (ripe ? 2.0f : 0.5f)};
        if (k == Crop::Turnip) leaf(c, b, a, L, 1.2f + st * 0.35f, 0.4f + droop, G, (i & 1) ? 2 : 3);
        else {   // the carrot's feathery fronds: a thin stem with fine leaflets
          const Vec2 e = b + V2(std::cos(a) * L, std::sin(a) * L + droop * 2.5f);
          tube(c, b, (b + e) * 0.5f, e, 0.45f, 0.4f, G, 1);
          for (int q = 2; q <= (int)L; q += 1) {
            const Vec2 p = b + (e - b) * (q / L);
            c.set((int)std::floor(p.x) + ((q & 1) ? 1 : -1), (int)std::floor(p.y), G[(q & 1) ? 3 : 2]);
            if (q > L * 0.6f) c.set((int)std::floor(p.x), (int)std::floor(p.y) - 1, G[4]);
          }
        }
      }
      break;
    }
    case Crop::Cabbage: {   // a rosette of broad blue-green leaves round a head that swells
      const int nl = 5 + st;
      for (int i = 0; i < nl; i++) {
        const float a = -PI / 2 + ((float)i / (nl - 1) - 0.5f) * 3.0f;
        leaf(c, V2(cx, by - 1), a, (3.0f + st * 1.4f) * V.size, 1.6f + st * 0.3f, 0.6f + droop, G, (i < nl / 2) ? 3 : 2);
      }
      if (st >= 2) {
        const float r = st == 2 ? 2.4f : 3.6f;
        ball(c, cx, by - r * 0.8f - 0.5f, r, r * 0.85f, wilt ? tintRamp(kCabbage, rgba(190, 180, 120), 0.4f) : kCabbage, 0.05f);
        // the wrapped leaves' seams
        // the wrapper leaves' veins curving round the head, its crown lit
        for (int q = -1; q <= 1; q += 2)
          for (int i = 0; i < (int)(r * 1.4f); i++) c.set((int)(cx + q * (r * 0.45f) - q * i * 0.25f), (int)(by - r * 0.3f) - i, kCabbage[q < 0 ? 4 : 2]);
        c.set((int)(cx - r * 0.4f), (int)(by - r * 1.4f), kCabbage[4]); c.set((int)(cx - r * 0.4f) + 1, (int)(by - r * 1.4f), kCabbage[4]);
      }
      break;
    }
    case Crop::Onion: {   // upright hollow leaves; when ripe they fall over and yellow, the bulbs swelling at the soil
      if (ripe) for (int b = 0; b < 2; b++) {
        const float bx = cx - 2.5f + b * 5.0f;
        ball(c, bx, by - 0.6f, 2.4f, 2.0f, kOnion, 0.05f);
        c.set((int)bx, (int)by - 3, kOnion[1]);
        c.set((int)bx - 1, (int)by - 1, kOnion[4]);
      }
      const int nl = 5 + st;
      for (int i = 0; i < nl; i++) {
        const float side = ((float)i / (nl - 1) - 0.5f);
        const float L = (5.0f + st * 2.5f) * V.size * (0.85f + jit(V.seed, i, 2, 0.15f) + 0.15f);
        const Vec2 b{cx + side * 4.0f, by - (ripe ? 2.0f : 0.5f)};
        const float fall = ripe ? 2.6f : 0.3f;
        const Vec2 e = b + V2(side * L * 0.6f * fall, -L + (ripe ? L * 0.6f : 0.0f) + droop * 3.0f);
        const Ramp& lr = ripe ? ((i & 1) ? kYellowing : G) : G;
        tube(c, b, b + V2(side * 0.5f, -L * 0.6f), e, 0.6f, 0.45f, lr, (i & 1) ? 2 : 3);
      }
      break;
    }
    default: break;
  }
}

// ---------------------------------------------------------------- flax
void flaxCrop(Canvas& c, int st, const CropVar& V, bool wilt) {
  static const float kH[4] = {3.5f, 8.0f, 13.0f, 15.0f};
  const float droop = wilt ? 1.0f : 0.0f;
  for (int i = 0; i < 9; i++) {
    if (i == V.missing) continue;
    const int row = i & 1;
    const float x0 = 1.5f + i * 13.0f / 8 + jit(V.seed, i, 1, 0.5f), y0 = kBase - (row ? 0.0f : 2.0f);
    const float h = kH[st] * V.size * (0.85f + jit(V.seed, i, 2, 0.15f) + 0.15f) - droop * 2;
    const float lean = V.lean * 0.1f + jit(V.seed, i, 3, 0.08f) + (wilt ? 0.15f : 0);
    const Vec2 tip = V2(x0 + lean * h, y0 - h);
    tube(c, V2(x0, y0), V2(x0 + lean * h * 0.3f, y0 - h * 0.5f), tip, 0.5f, 0.45f, kBlueLeaf, row ? 2 : 1);
    for (int l = 1; l < (int)h - 2; l += 3) c.set((int)std::floor(x0 + lean * l) + ((l / 3) & 1 ? 1 : -1), (int)y0 - l, kBlueLeaf[3]);   // the fine leaves
    if (st >= 2) {
      if (st == 2) { c.set((int)std::floor(tip.x), (int)std::floor(tip.y) - 1, kBlueLeaf[4]); continue; }   // buds
      const Ramp& f = wilt ? tintRamp(kFlaxBlue, rgba(180, 170, 150), 0.45f) : kFlaxBlue;
      const int fx = (int)std::floor(tip.x), fy = (int)std::floor(tip.y) - 1;
      c.set(fx, fy, f[4]); c.set(fx - 1, fy, f[3]); c.set(fx + 1, fy, f[2]); c.set(fx, fy - 1, f[3]); c.set(fx, fy + 1, f[1]);
      c.set(fx, fy, rgba(250, 240, 170));   // the eye
      if (hash3(i, 9, V.seed) % 2) { c.set(fx + 2, fy + 2, f[3]); c.set(fx + 3, fy + 2, f[2]); }
    }
  }
}

// ---------------------------------------------------------------- beans on a stake
void beanCrop(Canvas& c, int st, const CropVar& V, bool wilt) {
  const float droop = wilt ? 1.0f : 0.0f;
  const int sx = 8 + (V.v & 1);
  // the stake (a split pole, lit on the left), from stage 1
  if (st >= 1) {
    for (int y = 6; y <= kBase; y++) { c.set(sx, y, kWood[3]); c.set(sx + 1, y, kWood[1]); }
    c.set(sx, 5, kWood[4]); c.set(sx + 1, 5, kWood[2]);
  }
  if (st == 0) { for (int i = 0; i < 2; i++) leaf(c, V2(sx + 0.5f, kBase), -PI / 2 + (i ? 0.7f : -0.7f), 3.0f, 1.1f, 0.3f + droop, kSprout, 3 - i, false); return; }
  // the vine winding up the stake, its heart-shaped leaves either side
  const int top = st == 1 ? 18 : (st == 2 ? 9 : 7);
  for (int y = kBase; y > top; y--) {
    const int side = ((y / 3) & 1) ? -1 : 2;
    c.set(sx + side, y, kSprout[2]);
  }
  int li = 0;
  for (int y = kBase - 2; y > top; y -= 3, li++) {
    const float side = (li & 1) ? 1.0f : -1.0f;
    leaf(c, V2(sx + 0.5f + side, (float)y), side > 0 ? -0.35f + droop * 0.8f : PI + 0.35f - droop * 0.8f, 3.2f, 1.3f, 0.4f + droop, kSprout, (li & 1) ? 2 : 3);
  }
  if (st >= 2 && !wilt) {   // flowers (white or scarlet runner by variant)
    const uint32_t fc = (V.v & 2) ? rgba(222, 70, 56) : kWhite;
    for (int f = 0; f < 3; f++) { const int fy = top + 3 + f * 5; c.set(sx + ((f & 1) ? 3 : -3), fy, fc); c.set(sx + ((f & 1) ? 3 : -3), fy + 1, mixc(fc, kInk, 0.3f)); }
  }
  if (st == 3) {   // the pods hanging
    for (int p = 0; p < 4; p++) {
      const float px0 = sx + 0.5f + ((p & 1) ? 2.5f : -2.5f), py0 = top + 4.0f + p * 4.0f;
      tube(c, V2(px0, py0), V2(px0 + ((p & 1) ? 0.6f : -0.6f), py0 + 2.5f), V2(px0, py0 + 5.0f + droop), 0.6f, 0.5f, wilt ? kYellowing : kPod, 2, 0.9f);
    }
  }
}

// ---------------------------------------------------------------- grapes on a trellis post
void grapeCrop(Canvas& c, int st, const CropVar& V, bool wilt) {
  const float droop = wilt ? 1.0f : 0.0f;
  // the trellis: a post and a cross-bar with a wire (from stage 1)
  if (st >= 1) {
    for (int y = 8; y <= kBase; y++) { c.set(7, y, kWood[3]); c.set(8, y, kWood[1]); }
    for (int x = 1; x <= 14; x++) c.set(x, 9, kWood[x < 8 ? 3 : 2]);
    c.set(7, 8, kWood[4]);
  }
  // the gnarled trunk
  const float trunkTop = st == 0 ? kBase - 4.0f : 11.0f;
  tube(c, V2(8.5f, kBase), V2(9.8f, (kBase + trunkTop) * 0.5f), V2(8.0f, trunkTop), 1.1f, 0.7f, kBark, 2);
  if (st == 0) { leaf(c, V2(8.0f, trunkTop), -PI / 2 - 0.6f, 3.0f, 1.3f, 0.3f + droop, kSprout, 3); leaf(c, V2(8.0f, trunkTop), -PI / 2 + 0.7f, 3.0f, 1.3f, 0.3f + droop, kSprout, 2); return; }
  // the canes along the wire, lobed leaves hanging off them
  const int nl = 3 + st * 2;
  for (int i = 0; i < nl; i++) {
    const float x = 2.0f + i * 12.0f / (nl - 1) + jit(V.seed, i, 1, 0.5f);
    const float y = 9.5f + jit(V.seed, i, 2, 1.0f) + (st == 1 ? 1.0f : 0.0f);
    ball(c, x, y + 1.0f + droop, 2.2f, 1.9f, (i & 1) ? kLeafDark : kLeaf, 0.08f, wilt ? -1 : 0);
    if (st >= 2 && (i % 2) == 0) ball(c, x + 0.5f, y + 3.5f + droop, 1.7f, 1.5f, kLeafDark, 0.08f, -1);   // leaves hanging lower
    c.set((int)x, (int)(y + 2 + droop), kLeafDark[1]);
  }
  if (st >= 2) for (int i = 0; i < 3; i++) ball(c, 3.0f + i * 5.0f, 8.0f, 1.6f, 1.2f, kLeaf, 0.08f);   // new growth over the bar
  if (st == 3) {   // the bunches: purple (or green by variant)
    const Ramp& G = (V.v & 3) == 3 ? kGrapeGreen : kPurple;
    for (int b = 0; b < 3; b++) {
      if (b == 1 && (V.v & 4)) continue;
      const float bx = 3.5f + b * 4.5f, byy = 14.0f + (b & 1) + droop;
      for (int r = 0; r < 4; r++)
        for (int q = 0; q <= 3 - r; q++) {
          const float gx = bx - (3 - r) * 0.5f + q, gy = byy + r * 1.3f;
          c.set((int)std::floor(gx), (int)std::floor(gy), G[q == 0 ? 4 : (q == 3 - r ? 1 : 2)]);
          c.set((int)std::floor(gx), (int)std::floor(gy) + 1, G[1]);
        }
    }
  }
}

// ---------------------------------------------------------------- a young date palm
void dateCrop(Canvas& c, int st, const CropVar& V, bool wilt) {
  const float droop = wilt ? 1.0f : 0.0f;
  const float trunkH = st <= 1 ? 1.0f : (st == 2 ? 7.0f : 9.0f);
  const float cx = 8.0f + V.lean * 0.4f;
  const Vec2 crown{cx + V.lean * 0.6f, kBase - trunkH};
  if (trunkH > 2) {   // the trunk: stacked leaf bases (lit left, diamond scars)
    for (float y = kBase; y >= crown.y; y -= 1.0f) {
      const float t = (kBase - y) / trunkH, x = cx + V.lean * 0.6f * t;
      const int row = (int)y;
      const int xi = (int)std::floor(x);
      c.set(xi - 1, row, kBark[3]); c.set(xi, row, kBark[2]); c.set(xi + 1, row, kBark[1]);
      if (row % 2 == 0) { c.set(xi, row, kBark[1]); c.set(xi - 1, row, kBark[4]); }
    }
  }
  if (st == 3) {   // the date clusters hanging under the crown
    for (int s = -1; s <= 1; s += 2) {
      const float hx = crown.x + s * 2.2f;
      tube(c, crown, V2(hx, crown.y + 1.0f), V2(hx + s * 0.5f, crown.y + 4.0f + droop), 0.5f, 0.45f, kGold, 2);
      for (int d = 0; d < 6; d++) {
        const int dx = (int)std::floor(hx + s * 0.5f) + (d % 3) - 1, dy = (int)(crown.y + 3.0f + droop) + d / 3;
        c.set(dx, dy, kDate[(d % 3) == 0 ? 4 : ((d % 3) == 1 ? 2 : 1)]);
      }
    }
  }
  // the fronds: arching out and down from the crown, leaflets along them
  const int nf = st == 0 ? 3 : 5 + (st >= 2 ? 2 : 0);
  const float L = (st == 0 ? 4.0f : (st == 1 ? 7.5f : 9.0f)) * V.size;
  for (int i = 0; i < nf; i++) {
    const float a = -PI / 2 + ((float)i / (nf - 1) - 0.5f) * (st == 0 ? 1.6f : 3.4f) + jit(V.seed, i, 1, 0.1f);
    const Vec2 d{std::cos(a), std::sin(a)};
    const Vec2 e = crown + d * L + V2(0, std::fabs(d.x) * L * 0.45f + droop * 3);
    const Vec2 m = crown + d * (L * 0.6f);
    tube(c, crown, m, e, 0.55f, 0.4f, kPalmFrond, 1);
    for (int q = 2; q <= (int)L; q++) {   // the leaflets: short strokes either side, drooping
      const float t = q / L, u = 1 - t;
      const Vec2 p{u * u * crown.x + 2 * u * t * m.x + t * t * e.x, u * u * crown.y + 2 * u * t * m.y + t * t * e.y};
      const int k = d.x < 0 ? 3 : 2;
      c.set((int)std::floor(p.x), (int)std::floor(p.y) + 1, kPalmFrond[k]);
      if (q & 1) c.set((int)std::floor(p.x) + (d.x < 0 ? -1 : 1), (int)std::floor(p.y) + 1, kPalmFrond[k - 1]);
      c.set((int)std::floor(p.x), (int)std::floor(p.y) - 1, kPalmFrond[k + 1]);
    }
  }
}

// ---------------------------------------------------------------- maize: tall stalks, long leaves, cobs, tassels
void maizeCrop(Canvas& c, int st, const CropVar& V, bool wilt) {
  static const float kH[4] = {4.0f, 13.0f, 22.0f, 27.0f};
  const float droop = wilt ? 1.0f : 0.0f;
  for (int s = 0; s < 2; s++) {
    if (s == 1 && V.missing >= 0 && st >= 2 && (V.v & 1)) continue;   // a gap in the row
    const float x0 = s == 0 ? 5.0f + jit(V.seed, s, 1, 0.6f) : 10.5f + jit(V.seed, s, 1, 0.6f);
    const float y0 = kBase - (s == 0 ? 2.0f : 0.0f);
    const float h = kH[st] * V.size * (s == 0 ? 1.0f : 0.94f) - droop * 2;
    const float lean = V.lean * 0.05f + jit(V.seed, s, 2, 0.04f);
    const Vec2 base{x0, y0}, tip{x0 + lean * h, y0 - h};
    const Ramp& G = st == 3 ? (wilt ? kYellowing : tintRamp(kSprout, rgba(196, 180, 104), 0.25f)) : (s == 0 ? kLeafDark : kSprout);
    if (st == 0) { leaf(c, base, -PI / 2 - 0.5f, 4.0f, 1.0f, 0.4f + droop, G, 3, false); leaf(c, base, -PI / 2 + 0.6f, 3.5f, 1.0f, 0.4f + droop, G, 2, false); continue; }
    tube(c, base, (base + tip) * 0.5f, tip, 1.0f, 0.6f, G, 2);
    // the long arching leaves, alternating sides up the stalk
    const int nl = st == 1 ? 3 : 5;
    for (int l = 0; l < nl; l++) {
      const float t = 0.12f + l * (0.72f / nl);
      const Vec2 lb = base + (tip - base) * t;
      const float side = ((l + s) & 1) ? 1.0f : -1.0f;
      const float L = (6.5f + (st >= 2 ? 2.5f : 0.0f)) * (1.0f - t * 0.3f);
      const Ramp& lr = (st == 3 && l == 0) ? kHusk : G;
      leaf(c, lb, -PI / 2 + side * (0.85f + t * 0.4f), L, 1.45f, 1.1f + droop, lr, side < 0 ? 3 : 2);
    }
    if (st >= 2) {   // the tassel at the top (green, then gold), the cob in its husk on the stalk, silk at its tip
      const Ramp& T = st == 3 ? kCob : kSprout;
      for (int b = -1; b <= 1; b++) tube(c, tip, tip + V2(b * 1.0f, -1.5f), tip + V2(b * 2.4f, -1.0f + (b ? 1.5f : -2.0f)), 0.45f, 0.4f, T, b < 0 ? 4 : 3);
      const Vec2 cb = base + (tip - base) * 0.48f + V2(s == 0 ? 1.5f : -1.5f, 0);
      const float dir = s == 0 ? 1.0f : -1.0f;
      tube(c, cb, cb + V2(dir * 1.0f, -2.0f), cb + V2(dir * 1.8f, -4.5f), 1.1f, 0.6f, st == 3 ? kHusk : kPod, 2, 1.4f);
      if (st == 3) {
        c.set((int)std::floor(cb.x + dir * 1.2f), (int)std::floor(cb.y - 3.0f), kCob[3]);
        c.set((int)std::floor(cb.x + dir * 1.2f), (int)std::floor(cb.y - 2.0f), kCob[2]);
      }
      c.set((int)std::floor(cb.x + dir * 2.0f), (int)std::floor(cb.y - 5.5f), st == 3 ? kAutumn[1] : kGold[3]);
    }
  }
}

// ---------------------------------------------------------------- tea: a clipped bush
void teaCrop(Canvas& c, int st, const CropVar& V, bool wilt) {
  const float cx = 8.0f, by = kBase + 0.5f;
  if (st == 0) { for (int i = 0; i < 3; i++) leaf(c, V2(cx, by - 1), -PI / 2 + (i - 1) * 0.8f, 3.0f, 1.2f, 0.2f + (wilt ? 1 : 0), kTeaLeaf, 3, false); return; }
  const float rx = 3.5f + st * 1.3f, ry = 2.6f + st * 1.0f;
  const Ramp& G = wilt ? tintRamp(kTeaLeaf, rgba(150, 140, 90), 0.35f) : kTeaLeaf;
  // stems at the foot, then the clipped dome (flat-topped: the pickers keep it at hand height)
  for (int s = -1; s <= 1; s++) tube2(c, V2(cx + s * 1.2f, by), V2(cx + s * 2.2f, by - ry), 0.5f, 0.45f, kBark, 1);
  furBall(c, cx, by - ry - 0.5f, rx, ry, G, 0.5f, V.seed);
  for (int x = (int)(cx - rx + 1); x <= (int)(cx + rx - 1); x++) {   // the clipped table top
    const int y = (int)std::floor(by - ry * 2 + 0.5f);
    if (solid(c, x, y + 1)) c.set(x, y + 1, G[x < cx ? 4 : 3]);
  }
  // glossy leaves catching the light
  for (int i = 0; i < 6 + st * 2; i++) {
    const int x = (int)(cx - rx + 1) + (int)(hash3(i, 1, V.seed) % (uint32_t)std::max(1, (int)(rx * 2 - 1)));
    const int y = (int)(by - ry * 2) + 1 + (int)(hash3(i, 2, V.seed) % (uint32_t)std::max(1, (int)(ry * 2 - 1)));
    if (solid(c, x, y)) c.set(x, y, G[(x < cx) ? 4 : 3]);
  }
  if (st == 3 && !wilt) {   // the bright new flush: two-leaves-and-a-bud on top, ready to pick
    for (int i = 0; i < 6; i++) {
      const int x = (int)(cx - rx + 2) + i * (int)(rx * 2 - 3) / 5, y = (int)std::floor(by - ry * 2) - (i & 1);
      c.set(x, y, rgba(190, 232, 110)); c.set(x, y - 1, rgba(222, 246, 150));
    }
  }
}

// ---------------------------------------------------------------- rice: a paddy clump standing in water
void riceCrop(Canvas& c, int st, const CropVar& V, bool wilt) {
  // the water round its foot (the paddy: a sheet of water, the sky in it)
  for (int y = kBase - 2; y <= kBase + 2; y++)
    for (int x = 1; x <= 14; x++) {
      const float dx = (x + 0.5f - 8.0f) / 7.0f, dy = (y + 0.5f - kBase) / 2.6f;
      if (dx * dx + dy * dy > 1.0f) continue;
      int k = 2;
      if (dy < -0.4f) k = 3;
      if ((x + y * 3) % 7 == 0) k = 4;
      if (dy > 0.5f) k = 1;
      c.set(x, y, kPaddy[k]);
    }
  static const float kH[4] = {4.0f, 9.0f, 13.0f, 14.0f};
  const float droop = wilt ? 1.0f : 0.0f;
  for (int cl = 0; cl < 2; cl++) {   // two clumps (hills) of transplanted seedlings
    const float cx = cl == 0 ? 4.5f : 11.0f, cy = kBase - (cl == 0 ? 1.0f : 0.0f);
    const int n = 4 + st;
    for (int i = 0; i < n; i++) {
      const float side = ((float)i / (n - 1) - 0.5f);
      const float h = kH[st] * V.size * (0.85f + jit(V.seed, i + cl * 9, 1, 0.15f) + 0.15f) - droop * 2;
      const Vec2 b{cx + side * 1.5f, cy};
      const Vec2 tip = b + V2(side * h * 0.55f + V.lean * 0.6f, -h);
      const Ramp& G = st == 3 ? (wilt ? kYellowing : tintRamp(kSprout, rgba(200, 186, 100), 0.35f)) : (cl == 0 ? kLeafDark : kSprout);
      tube(c, b, b + V2(side * h * 0.1f, -h * 0.6f), tip + V2(0, droop * 2), 0.5f, 0.4f, G, (i & 1) ? 2 : 3);
      if (st >= 2 && (i & 1) == 0) {   // the panicle: green, then heavy gold and drooping
        const Ramp& E = st == 3 ? kRiceEar : kSprout;
        const float dir = side < 0 ? -1.0f : 1.0f;
        const Vec2 e = tip + V2(dir * 2.2f, st == 3 ? 3.0f : 1.0f);
        tube(c, tip, tip + V2(dir * 1.5f, -0.8f), e, 0.7f, 0.5f, E, 3);
        if (st == 3) { c.set((int)std::floor(e.x), (int)std::floor(e.y) + 1, E[2]); c.set((int)std::floor(tip.x + dir), (int)std::floor(tip.y), E[4]); }
      }
    }
  }
}

// ---------------------------------------------------------------- herbs: sage, thyme or lavender by variant
void herbCrop(Canvas& c, int st, const CropVar& V, bool wilt) {
  const int kind = (int)(V.v % 3);   // 0 sage, 1 thyme, 2 lavender
  const float droop = wilt ? 1.0f : 0.0f;
  const float cx = 8.0f + V.lean * 0.5f, by = kBase;
  const Ramp& G = kind == 1 ? kLeafDark : kSage;
  if (st == 0) { for (int i = 0; i < 3; i++) leaf(c, V2(cx, by), -PI / 2 + (i - 1) * 0.8f, 2.5f, 0.9f, 0.3f + droop, G, 3, false); return; }
  if (kind == 1) {   // thyme: a low, fine-leaved mound
    furBall(c, cx, by - 1.0f - st * 0.6f, 3.0f + st * 1.4f, 1.6f + st * 0.8f, G, 0.9f, V.seed);
    if (st == 3 && !wilt) for (int i = 0; i < 10; i++) {
      const int x = (int)cx - 5 + (int)(hash3(i, 3, V.seed) % 11), y = (int)by - 2 - (int)(hash3(i, 4, V.seed) % 4);
      if (solid(c, x, y)) c.set(x, y, kPink[3 + (i & 1)]);
    }
    return;
  }
  const int n = kind == 0 ? 4 + st * 2 : 6 + st * 2;
  for (int i = 0; i < n; i++) {
    const float a = -PI / 2 + ((float)i / (n - 1) - 0.5f) * (kind == 0 ? 2.4f : 1.5f) + jit(V.seed, i, 1, 0.12f);
    const float L = (kind == 0 ? 3.0f + st * 1.3f : 4.0f + st * 2.0f) * V.size;
    const Vec2 b{cx + jit(V.seed, i, 2, 1.0f), by};
    if (kind == 0) leaf(c, b, a, L, 1.2f, 0.4f + droop, G, (i & 1) ? 2 : 3, false);   // sage: soft oval leaves
    else {   // lavender: grey needle stems, purple spikes on top
      const Vec2 e = b + V2(std::cos(a) * L, std::sin(a) * L + droop * 2);
      tube(c, b, (b + e) * 0.5f, e, 0.45f, 0.4f, G, (i & 1) ? 2 : 3);
      if (st >= 2) {
        const Ramp& P = st == 3 && !wilt ? kLavender : G;
        for (int q = 0; q < 3; q++) c.set((int)std::floor(e.x), (int)std::floor(e.y) - q, P[q == 2 ? 4 : 3 - q % 2]);
      }
    }
  }
  if (kind == 0 && st >= 2) for (int s = 0; s < (st == 3 ? 3 : 1); s++) {   // sage flower spikes
    const int x = (int)cx - 3 + s * 3, top = (int)by - 6 - st * 2;
    for (int y = top; y < (int)by - 4; y++) c.set(x, y, (y - top) % 2 ? G[2] : (st == 3 && !wilt ? kLavender[3] : G[3]));
  }
}

// ================================================================ ground edits
const Ramp kTilled = ramp5(rgba(46, 30, 32), rgba(74, 48, 40), rgba(104, 70, 50), rgba(136, 98, 66), rgba(164, 126, 86));
const Ramp kTilledWet = ramp5(rgba(30, 20, 30), rgba(48, 32, 36), rgba(70, 46, 42), rgba(96, 66, 52), rgba(128, 96, 76));
const Ramp kEarth = ramp5(rgba(92, 70, 58), rgba(132, 104, 78), rgba(166, 136, 100), rgba(192, 166, 124), rgba(216, 196, 156));

// how far inside an unjoined edge a pixel is (0 on the edge), and which edges: the rim and corner logic of both tiles
struct EdgeDist { int n, e, s, w; };
EdgeDist edgeDist(int x, int y, uint8_t joins) {
  EdgeDist d{99, 99, 99, 99};
  if (!(joins & 1)) d.n = y;
  if (!(joins & 2)) d.e = 15 - x;
  if (!(joins & 4)) d.s = 15 - y;
  if (!(joins & 8)) d.w = x;
  return d;
}

// smooth value noise on a 4 px lattice that wraps every 16 px (ground tiles meet without a seam)
float tileNoise(int x, int y, uint32_t seed) {
  auto at = [&](int i, int j) { return (hash3(i & 3, j & 3, seed) & 1023) / 1023.0f; };
  const int i = x >> 2, j = y >> 2;
  float fx = ((x & 3) + 0.5f) / 4.0f, fy = ((y & 3) + 0.5f) / 4.0f;
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
  return lerpf(lerpf(at(i, j), at(i + 1, j), fx), lerpf(at(i, j + 1), at(i + 1, j + 1), fx), fy);
}

// path looks (style & 7), stone tint by (style >> 3)
enum PathLook { kPathEarth, kPathFlags, kPathCobble, kPathBrick, kPathGravel, kPathBoards, kPathStepping, kPathSett };

// a 16-periodic Voronoi of `n` jittered seeds (the same in every tile, so stones meet across tile edges): the cell
// index and the distance gap to the next cell (small = a joint)
void periodicCell(int x, int y, int n, uint32_t seed, int& cell, float& gap) {
  float b = 1e9f, s = 1e9f;
  int bi = 0;
  const int g = (int)std::ceil(std::sqrt((float)n));
  for (int i = 0; i < g * g; i++) {
    const float sx = ((i % g) + 0.5f) * 16.0f / g + ((int)(hash3(i, 1, seed) % 100) - 50) * 0.07f;
    const float sy = ((i / g) + 0.5f) * 16.0f / g + ((int)(hash3(i, 2, seed) % 100) - 50) * 0.07f;
    for (int oy = -1; oy <= 1; oy++)
      for (int ox = -1; ox <= 1; ox++) {
        const float dx = x + 0.5f - (sx + ox * 16), dy = (y + 0.5f - (sy + oy * 16)) * 1.15f;
        const float d = std::sqrt(dx * dx + dy * dy);
        if (d < b) { s = b; b = d; bi = i; } else if (d < s) s = d;
      }
  }
  cell = bi;
  gap = s - b;
}

}  // namespace

// ---------------------------------------------------------------- ground edits
// (M7 fix r3, review: "forest-floor root lines run through the fenced yard") a kept yard's ground over the woods' floor:
// short trodden grass (tone 0 green, 1 the autumn woods' olive), worn to earth here and there; it fades out over two
// pixels toward a side that is not yard (the forest floor shows through a dither there)
Canvas yardGroundTile(uint8_t joins, int tone, int32_t tileX, int32_t tileY) {
  Canvas c(16, 16);
  static const uint32_t kG[2][4] = {{rgba(62, 100, 52), rgba(72, 114, 58), rgba(82, 126, 62), rgba(98, 142, 70)},
                                    {rgba(84, 98, 50), rgba(96, 110, 56), rgba(108, 122, 62), rgba(126, 138, 72)}};
  const uint32_t* G = kG[std::clamp(tone, 0, 1)];
  static const int kB[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  // the world pixel (wrapped every 8192 px to keep the float noise exact): the patches run on across tiles, never
  // a stamp repeated every tile
  const int ox = (int)(((int64_t)tileX * 16) % 8192 + 8192) % 8192, oy = (int)(((int64_t)tileY * 16) % 8192 + 8192) % 8192;
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      // the fade toward a side that is not yard: pixels dropped by a 2 px ordered dither
      const EdgeDist e = edgeDist(x, y, joins);
      const int dm = std::min(std::min(e.n, e.e), std::min(e.s, e.w));
      if ((dm == 0 && kB[y & 3][x & 3] < 11) || (dm == 1 && kB[y & 3][x & 3] < 5)) continue;
      const int gx = ox + x, gy = oy + y;
      const float n = vnoise(gx / 11.0f, gy / 9.0f, 913u) * 0.7f + vnoise(gx / 4.0f, gy / 4.0f, 917u) * 0.3f;
      const float wear = vnoise(gx / 23.0f, gy / 19.0f, 919u);
      const int k = n > 0.62f ? 2 : (n < 0.38f ? 0 : 1);
      const uint32_t h = hash3(gx, gy, 2311u);
      uint32_t col = G[k];
      if (wear < 0.24f) col = mix(col, kEarth[2], wear < 0.16f ? 0.55f : 0.3f);   // trodden to the earth in places
      if (h % 9 == 0) {                                                   // a blade lit at its tip, its root in shade
        col = G[3];
        if (y + 1 < 16 && (c.get(x, y + 1) >> 24) == 0) c.set(x, y + 1, mix(G[0], kEarth[1], 0.3f));
      }
      if ((c.get(x, y) >> 24) == 0) c.set(x, y, col);
    }
  return c;
}

Canvas farmlandTile(uint8_t joins, bool watered, uint32_t variant) {
  Canvas c(16, 16);
  const Ramp& S = watered ? kTilledWet : kTilled;
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      // the furrows run east-west, 4 px apart (a ridge's lit crest, its face, the trough), wandering a pixel now and
      // then so the rows look hand-hoed; the wobble repeats every 16 px, so tiles meet without a seam
      const int wob = (int)(hash3((x >> 2) & 3, 0, 917u + (variant & 1)) % 3 == 0);
      static const int kRow[4] = {3, 2, 1, 2};
      const int r = (y + wob) & 3;
      int k = kRow[r];
      const uint32_t h = hash3(x, y, 911u + (variant & 3) * 31u);
      if (r == 0 && (h % 6) == 0) k = 4;                // a clod catching the light
      if (r == 2 && (h % 9) == 0) k = 0;                // a deep hollow
      if (r == 1 && (h % 7) == 0) k = 3;
      if (r == 3 && (h % 5) == 0) k = 1;
      uint32_t col = S[k];
      if (watered && r == 0 && (h % 7) == 0) col = mixc(col, kWater[3], 0.35f);   // a wet glint
      c.set(x, y, col);
    }
  // where the bed ends: a lip of earth lit on the north and west, a shaded edge south and east, rounded outer corners
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      const EdgeDist d = edgeDist(x, y, joins);
      const int m = std::min(std::min(d.n, d.s), std::min(d.e, d.w));
      if (m > 0) continue;
      const bool corner = (std::min(d.n, d.s) == 0 && std::min(d.e, d.w) == 0);
      if (corner) { c.set(x, y, 0); continue; }
      int k = 2;
      if (d.n == 0) k = 3;
      else if (d.w == 0) k = 3;
      else if (d.e == 0) k = 1;
      else if (d.s == 0) k = 1;
      c.set(x, y, S[k]);
    }
  return c;
}

Canvas pathTile(uint8_t joins, uint8_t style, uint32_t variant) {
  Canvas c(16, 16);
  const int look = style & 7;
  const int tintId = (style >> 3) & 1;
  const Ramp& St = tintId ? kStone : kStoneWarm;
  const uint32_t vs = 1301u + (variant & 3) * 17u;
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      uint32_t col = 0;
      const uint32_t h = hash3(x, y, vs);
      switch (look) {
        case kPathEarth: case kPathGravel: {   // packed earth, worn smooth, pebbles pressed in (gravel: more of them)
          const float n = tileNoise(x, y, 77u);   // 16-periodic: no seam between tiles
          int k = n > 0.62f ? 3 : (n < 0.36f ? 1 : 2);
          const uint32_t hp = hash3(x, y, 1777u);
          const bool pebble = look == kPathGravel ? (hp % 4) == 0 : (hp % 23) == 0;
          if (pebble) { col = St[(hp >> 4) % 2 ? 4 : 3]; c.set(x, y, col); if (y + 1 < 16) c.set(x, y + 1, St[1]); continue; }
          col = look == kPathGravel ? St[k] : kEarth[k];
          if ((h % 31) == 0) col = kEarth[1];
          break;
        }
        case kPathFlags: case kPathStepping: {   // irregular flagstones; stepping stones sit in grass-free earth
          int cell; float gap;
          periodicCell(x, y, look == kPathFlags ? 9 : 4, 4243u, cell, gap);
          const float jw = look == kPathFlags ? 0.9f : 2.6f;
          if (gap < jw) { col = look == kPathFlags ? St[0] : kEarth[gap < 1.2f ? 1 : 2]; break; }
          const int base = 2 + (int)(hash3(cell, 3, 991u) % 2);
          int k = base;
          // each slab is a little domed: lit toward its upper-left joint, shaded toward its lower-right
          int cn, cw; float gn, gw;
          periodicCell(x, (y + 15) % 16, look == kPathFlags ? 9 : 4, 4243u, cn, gn);
          periodicCell((x + 15) % 16, y, look == kPathFlags ? 9 : 4, 4243u, cw, gw);
          if (gn < jw || gw < jw) k = std::min(4, base + 1);
          int cs, ce; float gs, ge;
          periodicCell(x, (y + 1) % 16, look == kPathFlags ? 9 : 4, 4243u, cs, gs);
          periodicCell((x + 1) % 16, y, look == kPathFlags ? 9 : 4, 4243u, ce, ge);
          if (gs < jw || ge < jw) k = std::max(1, base - 1);
          if ((h % 29) == 0) k = std::max(0, k - 1);   // pits and wear
          col = St[k];
          if ((variant & 1) && (hash3(cell, 5, 33u) % 5) == 0 && gap < jw + 0.8f) col = kMoss[2];   // moss in the joints
          break;
        }
        case kPathCobble: case kPathSett: {   // round cobbles (sett: squared blocks in courses)
          if (look == kPathSett) {
            const int row = y / 4, bx = (x + (row & 1) * 2) % 16;
            const int lx = bx % 4, ly = y % 4;
            if (lx == 3 || ly == 3) { col = St[0]; break; }
            int k = 2 + (int)(hash3((x + (row & 1) * 2) / 4 % 4, row, 51u) % 2);
            if (lx == 0 || ly == 0) k = std::min(4, k + 1);
            if (lx == 2 || ly == 2) k = std::max(1, k - 1);
            col = St[k];
            break;
          }
          const int row = y / 3, ox = (row & 1) * 2;
          const int bx = (x + ox) % 16, cxi = bx / 4;
          const float dx = (bx % 4) - 1.5f, dy = (y % 3) - 0.8f;
          const float d = dx * dx / 2.6f + dy * dy / 1.4f;
          if (d > 1.0f) { col = kEarth[0]; break; }
          int k = 2 + (int)(hash3(cxi, row, 53u) % 2);
          if (dx + dy < -0.9f) k = 4; else if (dx + dy > 0.9f) k = 1;
          col = St[k];
          break;
        }
        case kPathBrick: {   // herringbone brick (16-periodic)
          const int u = (x + y) % 8, v = (x - y + 16) % 8;
          const bool a = ((x + y) / 4 + (x - y + 32) / 8) & 1;
          const bool joint = a ? (u == 0) : (v == 0);
          int k = 2 + (int)(hash3((x + y) / 4, (x - y + 32) / 4, 61u) % 2);
          if (joint) k = 0;
          col = joint ? kStoneWarm[1] : kBrick[k];
          break;
        }
        case kPathBoards: default: {   // a boardwalk: planks running north-south over earth, nail heads
          const int plank = x / 4, lx = x % 4;
          const int seam = (y + plank * 5) % 16;
          int k = lx == 0 ? 3 : (lx == 3 ? 1 : 2);
          if (seam == 0) k = 0;
          if ((h % 17) == 0 && lx > 0 && lx < 3) k = std::max(1, k - 1);   // grain
          col = kWood[k];
          if (lx == 1 && (seam == 1 || seam == 14)) col = kIron[1];
          break;
        }
      }
      c.set(x, y, col);
    }
  // the path's edges where it does not continue: a ragged worn border into the grass (earth), or a kerb of edge
  // stones (paved looks); rounded outer corners
  const bool paved = look != kPathEarth && look != kPathGravel && look != kPathStepping;
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      const EdgeDist d = edgeDist(x, y, joins);
      const int m = std::min(std::min(d.n, d.s), std::min(d.e, d.w));
      if (m > 2) continue;
      const bool corner = std::min(d.n, d.s) + std::min(d.e, d.w) <= (paved ? 0 : 1);
      if (corner) { c.set(x, y, 0); continue; }
      if (!paved) {
        // ragged: a noisy cut-in of 0..1 px, then a darker trodden rim
        const int along = (d.n <= 2 || d.s <= 2) ? x : y;
        const int side = d.n <= 2 ? 0 : d.s <= 2 ? 1 : d.e <= 2 ? 2 : 3;
        const int cut = (int)(hash3(along, side, 2027u) % 3 == 0);
        if (m < cut) { c.set(x, y, 0); continue; }
        if (m == cut) c.set(x, y, look == kPathStepping ? 0 : kEarth[1]);
        continue;
      }
      if (m > 1) continue;
      // a kerb: one course of edge stones, lit top
      const bool lit = (d.n <= 1 || d.w <= 1);
      const int along = (d.n <= 1 || d.s <= 1) ? x : y;
      if (along % 4 == 3) { c.set(x, y, St[0]); continue; }
      int k = m == 0 ? (lit ? 3 : 1) : (lit ? 4 : 2);
      if (d.s == 0) k = 0;
      c.set(x, y, St[k]);
    }
  return c;
}

// ---------------------------------------------------------------- crops
Canvas cropSprite(Crop c, int stage, uint32_t variant, bool wilted) {
  Canvas out(CROP_W, CROP_H);
  stage = std::clamp(stage, 0, 3);
  const CropVar V = cropVar(c, variant);
  switch (c) {
    case Crop::Wheat: case Crop::Barley: case Crop::Oats: case Crop::Rye: grainCrop(out, c, stage, V, wilted); break;
    case Crop::Potato: case Crop::Turnip: case Crop::Cabbage: case Crop::Carrot: case Crop::Onion: rootCrop(out, c, stage, V, wilted); break;
    case Crop::Flax: flaxCrop(out, stage, V, wilted); break;
    case Crop::Beans: beanCrop(out, stage, V, wilted); break;
    case Crop::Grapes: grapeCrop(out, stage, V, wilted); break;
    case Crop::Dates: dateCrop(out, stage, V, wilted); break;
    case Crop::Maize: maizeCrop(out, stage, V, wilted); break;
    case Crop::Tea: teaCrop(out, stage, V, wilted); break;
    case Crop::Rice: riceCrop(out, stage, V, wilted); break;
    case Crop::Herbs: herbCrop(out, stage, V, wilted); break;
    default: break;
  }
  if (wilted)   // paler: the living green fades toward a dry khaki
    for (uint32_t& p : out.px)
      if (chA(p)) {
        const int r = chR(p), g = chG(p), b = chB(p);
        if (g > r && g > b) p = mixc(p, rgba(168, 156, 104), 0.38f);
        else p = mixc(p, rgba(170, 158, 130), 0.18f);
      }
  // keep one empty column / row round the plant for its outline (a long leaf may reach the edge)
  for (int y = 0; y < out.h; y++) { out.set(0, y, 0); out.set(out.w - 1, y, 0); }
  for (int x = 0; x < out.w; x++) out.set(x, 0, 0);
  const bool grain = c == Crop::Wheat || c == Crop::Barley || c == Crop::Oats || c == Crop::Rye;
  if (!grain) outline(out, 0.72f);
  return out;
}

// ================================================================ the riding horse (and the loose horse critter)
namespace {

struct HorseCoat {
  Ramp coat, points, mane, light, hoof, sock;
  bool dapple = false, dorsal = false, twoTone = false, pointsLegs = false;
  int face = 0;          // 0 plain, 1 a star, 2 a blaze
  uint8_t socks = 0;     // bit per leg: 0 near fore, 1 far fore, 2 near hind, 3 far hind
  Ramp cloth;            // the saddle cloth
};
HorseCoat horseCoat(uint32_t coat, uint32_t v) {
  HorseCoat k;
  uint32_t base = coat ? opaque(coat) : rgba(140, 90, 48);
  const int sh = (int)((v >> 6) % 3) - 1;   // a little lighter or darker per horse
  base = shade(base, 1.0f + sh * 0.06f);
  const int r = chR(base), g = chG(base), b = chB(base);
  const float L = luma(base), sat = (std::max(r, std::max(g, b)) - std::min(r, std::min(g, b))) / 255.0f;
  k.coat = ramp(base, 0.85f);
  k.light = ramp(mixc(base, rgba(238, 222, 192), 0.5f), 0.7f);
  k.hoof = ramp5(rgba(36, 30, 40), rgba(58, 48, 54), rgba(84, 72, 72), rgba(118, 104, 96), rgba(156, 144, 128));
  k.sock = ramp5(rgba(150, 140, 136), rgba(200, 194, 184), rgba(234, 230, 220), rgba(246, 244, 236), rgba(255, 255, 250));
  if (sat < 0.09f && L > 0.5f) {            // grey: dappled, a pale mane, darker knees
    k.dapple = true;
    k.mane = ramp(rgba(226, 224, 218), 0.7f);
    k.points = ramp(mixc(base, rgba(70, 70, 84), 0.5f));
    k.pointsLegs = true;
  } else if (L > 0.6f) {                    // cream dun (the fjord horse): a two-tone mane, a dorsal stripe
    k.twoTone = k.dorsal = true;
    k.mane = ramp(rgba(240, 232, 206), 0.6f);
    k.points = ramp(rgba(74, 58, 52));
  } else if (L > 0.44f) {                   // dun: black points and a dorsal stripe
    k.dorsal = k.pointsLegs = true;
    k.points = ramp(rgba(58, 46, 42));
    k.mane = k.points;
  } else if (L > 0.26f && r > b + 40) {     // bay: black points
    k.pointsLegs = true;
    k.points = ramp(rgba(46, 36, 42));
    k.mane = k.points;
  } else if (r > b + 26) {                  // dark chestnut: a flaxen or a self-coloured mane
    k.points = k.coat;
    k.mane = (v & 8) ? ramp(rgba(226, 196, 140), 0.7f) : ramp(shade(base, 0.78f));
  } else {                                  // black-brown
    k.pointsLegs = true;
    k.points = ramp(rgba(36, 32, 40));
    k.mane = k.points;
  }
  k.face = (int)(v % 3);
  static const uint8_t kSocks[8] = {0, 4, 12, 5, 15, 0, 8, 0};
  k.socks = kSocks[(v >> 2) & 7];
  static const uint32_t kCloth[4] = {rgba(150, 40, 44), rgba(52, 76, 140), rgba(46, 110, 72), rgba(170, 120, 40)};
  k.cloth = ramp(kCloth[(v >> 9) & 3]);
  return k;
}

// a pose: each leg's foot offset (forward +) and lift, the body's bob and the head's carriage
struct HorsePose {
  float bob = 0, headDrop = 0, headFwd = 0, tail = 0, pitch = 0;
  float sw[4] = {0, 0, 0, 0}, lift[4] = {0, 0, 0, 0};   // legs: 0 near fore, 1 far fore, 2 near hind, 3 far hind
  bool graze = false, lying = false;
};
// pose: 0 idle, 1-4 walk, 5-7 gallop, 8-9 graze, 10 lying down
HorsePose horsePose(int pose) {
  HorsePose P;
  if (pose >= 1 && pose <= 4) {   // the four-beat walk: near hind, near fore, far hind, far fore
    static const float ph[4] = {0.25f, 0.75f, 0.0f, 0.5f};
    const float t = (pose - 1) / 4.0f;
    for (int i = 0; i < 4; i++) {
      const float a = (t - ph[i]) * TAU;
      P.sw[i] = std::cos(a) * 2.4f;
      P.lift[i] = std::max(0.0f, std::sin(a)) * 2.2f;
    }
    P.bob = (pose & 1) ? -1.0f : 0.0f;
    P.headDrop = (pose & 1) ? 0.6f : -0.2f;
    P.tail = (pose & 2) ? 0.6f : -0.6f;
  } else if (pose >= 5 && pose <= 7) {   // the gallop: extended, gathered (all four off the ground), landing
    static const float sw[3][4] = {{4.5f, 3.0f, -4.0f, -3.0f}, {-1.5f, -0.5f, 2.5f, 3.5f}, {1.0f, -2.5f, 1.0f, -1.0f}};
    static const float lf[3][4] = {{1.5f, 0.5f, 0.0f, 1.0f}, {3.5f, 3.0f, 3.0f, 2.5f}, {0.0f, 2.0f, 0.0f, 0.5f}};
    const int q = pose - 5;
    for (int i = 0; i < 4; i++) { P.sw[i] = sw[q][i]; P.lift[i] = lf[q][i]; }
    P.bob = q == 1 ? -2.0f : (q == 0 ? -0.5f : 0.0f);
    P.headFwd = q == 0 ? 1.5f : 0.5f;
    P.headDrop = q == 0 ? 1.5f : (q == 2 ? 1.0f : 0.0f);
    P.pitch = q == 0 ? 0.5f : (q == 2 ? -0.6f : 0.0f);
    P.tail = -1.6f;
  } else if (pose == 8 || pose == 9) {
    P.graze = true;
    P.sw[0] = 1.0f; P.sw[1] = -0.5f; P.sw[2] = -0.5f; P.sw[3] = 0.5f;
    P.tail = pose == 9 ? 1.0f : -0.4f;
  } else if (pose == 10) {
    P.lying = true;
  } else {
    P.lift[3] = 0.8f;   // idle: the far hind hoof resting on its toe
  }
  return P;
}

struct HorseRig {
  Canvas& c;
  const HorseCoat& K;
  const HorsePose& P;
  bool saddled;
  int g;
  bool headOnly = false;   // (M7 fixer r2) front(): the neck and head alone (the sheet's overlay row, over the rider)
  HorseRig(Canvas& c_, const HorseCoat& k, const HorsePose& p, bool s) : c(c_), K(k), P(p), saddled(s), g(c_.h - 2) {}

  // ---------------------------------------------------------------- shared parts
  // a leg from its top (on the body) to the hoof on the ground: the upper leg in the coat, the cannon and pastern in
  // the points colour (or a white sock), the hoof dark; fore: the knee folds forward when lifted; hind: the hock
  // angles back
  void legSide(float tx, float ty, float sw, float lift, bool fore, int bias, bool sock) {
    const float fx = tx + sw + (fore ? -lift * 0.4f : lift * 0.2f), fy = g + 0.5f - lift;
    Vec2 T{tx, ty}, F{fx, fy}, J;
    if (fore) J = Vec2{(tx + fx) * 0.5f + lift * 0.9f + sw * 0.15f, ty + (fy - ty) * 0.48f - lift * 0.3f};
    else J = Vec2{tx - 2.0f + sw * 0.55f - lift * 0.5f, ty + (fy - ty) * 0.52f};
    const Ramp& lower = K.pointsLegs ? K.points : K.coat;
    capsule(c, T, J, fore ? 1.55f : 2.1f, 1.0f, K.coat, bias);
    capsule(c, J, F + Vec2{0, -1.2f}, 0.95f, 0.8f, lower, bias);
    const int fxi = (int)std::floor(F.x), fyi = (int)std::floor(F.y);
    if (sock) for (int y = fyi - 3; y <= fyi - 1; y++) for (int x = fxi - 1; x <= fxi; x++) if (solid(c, x, y)) c.set(x, y, K.sock[3 + bias + (x == fxi ? -1 : 0)]);
    // the hoof: a dark wedge, toe forward
    c.set(fxi - 1, fyi, K.hoof[2 + bias]); c.set(fxi, fyi, K.hoof[1 + bias]); c.set(fxi + 1, fyi, K.hoof[0]);
    c.set(fxi - 1, fyi - 1, lower[1 + bias]);
  }

  // ---------------------------------------------------------------- the side view (facing right)
  void side() {
    const float ox = -0.5f, b = P.bob;
    auto Y = [&](float h, float x) { return g + 0.5f - h + b + (x - 17.0f) * P.pitch * 0.08f; };
    if (P.lying) { lyingSide(); return; }
    const float foreX = 23.5f + ox, hindX = 11.0f + ox, legTop = 12.0f;
    // the far legs, a shade darker, a pixel behind
    legSide(foreX - 1.5f, Y(legTop, foreX), P.sw[1], P.lift[1], true, -1, K.socks & 2);
    legSide(hindX - 1.5f, Y(legTop + 1, hindX), P.sw[3], P.lift[3], false, -1, K.socks & 8);
    // the body: hindquarters, barrel, chest; a slight sheen
    ball(c, hindX + 0.5f, Y(15.0f, hindX), 5.0f, 4.7f, K.coat, 0.06f);
    ball(c, 17.0f + ox, Y(14.2f, 17), 7.6f, 4.3f, K.coat, 0.06f);
    ball(c, foreX - 0.5f, Y(14.6f, foreX), 4.3f, 4.5f, K.coat, 0.06f);
    // the belly's lighter underline and the muscle lines
    for (int x = (int)(hindX + 2); x <= (int)(foreX - 2); x++)
      for (int y = (int)Y(10, x) - 2; y < c.h; y++)
        if (solid(c, x, y) && !solid(c, x, y + 1)) { c.set(x, y, K.coat[1]); break; }
    c.set((int)(hindX + 3.5f), (int)Y(14, hindX), K.coat[3]);
    c.set((int)(hindX + 4.5f), (int)Y(12.5f, hindX), K.coat[1]);
    if (K.dapple) for (int i = 0; i < 26; i++) {
      const int x = (int)(hindX - 3) + (int)(hash3(i, 1, 4441u) % 18), y = (int)Y(18, 17) + (int)(hash3(i, 2, 4441u) % 7);
      if (solid(c, x, y) && solid(c, x + 1, y)) { c.set(x, y, K.coat[4]); if (i & 1) c.set(x + 1, y, K.coat[3]); }
    }
    if (K.dorsal) for (int x = (int)(hindX - 1); x <= (int)(foreX - 3); x++) {
      for (int y = 0; y < c.h; y++) if (solid(c, x, y)) { c.set(x, y + 1, K.points[2]); break; }
    }
    // the tail: from the dock, falling in a full sweep behind the buttock
    {
      const float dx = hindX - 4.2f, dy = Y(18.5f, hindX);
      const float sway = P.tail;
      const Ramp& T = K.twoTone ? K.mane : K.mane;
      tube(c, V2(dx + 0.5f, dy), V2(dx - 2.5f + sway, dy + 4.0f), V2(dx - 1.5f + sway * 1.5f, dy + 11.5f - (P.graze ? 0 : 0)), 1.1f, 0.9f, T, 2, 1.7f);
      for (int i = 0; i < 4; i++) c.set((int)std::floor(dx - 1.5f + sway * 1.5f) + (i & 1), (int)std::floor(dy + 11.0f) - i * 2, T[3]);
    }
    // the neck and the head
    const float px = 29.0f + ox + P.headFwd, py = Y(25.5f - P.headDrop, 29);
    Vec2 poll{px, py}, muzzle{px + 3.6f, py + 5.2f};
    if (P.graze) { poll = Vec2{30.0f + ox, Y(9.0f, 30)}; muzzle = Vec2{32.0f + ox, (float)g - 0.5f}; }
    const Vec2 neckBase{foreX - 0.5f, Y(16.0f, foreX)};
    capsule(c, neckBase, poll + Vec2{-0.6f, 1.2f}, 3.3f, 2.0f, K.coat);
    capsule(c, poll, muzzle, 2.25f, 1.45f, K.coat);
    ball(c, poll.x + 0.2f, poll.y + 0.4f, 2.1f, 1.9f, K.coat, 0.04f);   // the jowl
    // the mane along the crest (two-tone: a dark centre stripe)
    {
      const int n = 9;
      for (int i = 0; i <= n; i++) {
        const float t = (float)i / n;
        const Vec2 p = neckBase + (poll - neckBase) * t + Vec2{-2.2f * (1 - t) - 0.6f, -2.6f * (1 - t) - 1.2f};
        const int x = (int)std::floor(p.x), y = (int)std::floor(p.y);
        c.set(x, y, K.mane[i & 1 ? 2 : 3]); c.set(x - 1, y + 1, K.mane[1]); c.set(x, y + 1, K.twoTone ? K.points[1] : K.mane[2]);
      }
      c.set((int)poll.x + 1, (int)poll.y - 1, K.mane[3]);   // the forelock
    }
    // ears, eye, nostril, a star or blaze
    const int ex = (int)std::floor(poll.x), ey = (int)std::floor(poll.y);
    if (!P.graze) { c.set(ex - 1, ey - 2, K.coat[3]); c.set(ex - 1, ey - 3, K.coat[2]); c.set(ex, ey - 2, K.coat[1]); }
    else { c.set(ex - 1, ey - 2, K.coat[2]); c.set(ex - 2, ey - 2, K.coat[3]); }
    c.set(ex + 1, ey, kEye); c.set(ex + 1, ey - 1, K.coat[1]);
    c.set((int)std::floor(muzzle.x + 0.6f), (int)std::floor(muzzle.y - 0.2f), K.coat[0]);
    c.set((int)std::floor(muzzle.x - 0.4f), (int)std::floor(muzzle.y + 0.9f), K.coat[1]);
    if (K.face) {
      const Vec2 d = muzzle - poll;
      for (int i = 1; i <= (K.face == 2 ? 5 : 1); i++) { const Vec2 p = poll + d * (0.18f + i * 0.13f); c.set((int)std::floor(p.x + 1.3f), (int)std::floor(p.y - 0.8f), K.sock[3]); }
    }
    // the near legs
    legSide(foreX, Y(legTop, foreX), P.sw[0], P.lift[0], true, 0, K.socks & 1);
    legSide(hindX, Y(legTop + 1, hindX), P.sw[2], P.lift[2], false, 0, K.socks & 4);
    // tack: the bridle, the saddle cloth and saddle, the girth, the stirrup; the reins to the pommel
    if (saddled) {
      const uint32_t L1 = kLeather[1], L2 = kLeather[2];
      line(c, (int)poll.x, (int)poll.y - 1, (int)std::floor(muzzle.x - 0.5f), (int)std::floor(muzzle.y), L1);   // cheek strap
      c.set((int)std::floor(muzzle.x - 0.5f), (int)std::floor(muzzle.y), kIron[3]);                          // the bit
      const int sx0 = (int)(14.0f + ox), sx1 = (int)(20.5f + ox);
      const int top = (int)std::floor(Y(18.6f, 17));
      for (int x = sx0 - 1; x <= sx1 + 1; x++)   // the cloth, its edge below the saddle flap
        for (int y = top + 1; y <= top + 5; y++) if (solid(c, x, y)) c.set(x, y, K.cloth[x == sx0 - 1 ? 3 : (y == top + 5 || x == sx1 + 1 ? 1 : 2)]);
      for (int x = sx0; x <= sx1; x++) {   // the seat: the cantle behind, the pommel in front
        const int y = top + (x == sx0 || x == sx1 ? -1 : 0);
        c.set(x, y, L2 + 0); c.set(x, y + 1, kLeather[x < sx0 + 3 ? 4 : 3]); c.set(x, y + 2, kLeather[2]); c.set(x, y + 3, kLeather[1]);
      }
      c.set(sx0, top - 2, kLeather[3]); c.set(sx1, top - 2, kLeather[2]); c.set(sx1 + 1, top - 1, kLeather[1]);
      for (int y = top + 4; y <= (int)Y(10.5f, 18); y++) c.set(sx0 + 3, y, L1);   // the girth
      // the stirrup leather and iron
      for (int y = top + 3; y <= top + 7; y++) c.set(sx0 + 4, y, kLeather[0]);
      c.set(sx0 + 3, top + 8, kIron[3]); c.set(sx0 + 4, top + 8, kIron[2]); c.set(sx0 + 5, top + 8, kIron[1]);
      // the reins from the bit up to the pommel
      line(c, (int)std::floor(muzzle.x - 0.5f), (int)std::floor(muzzle.y) - 1, sx1 + 1, top, L2);
    }
    if (K.dapple || K.dorsal) {}   // (painted above)
  }
  void lyingSide() {
    // the horse lying down: legs folded under, the head up and dozing
    const float b = 0;
    ball(c, 16.0f, g + 0.5f - 5.0f + b, 10.0f, 4.6f, K.coat, 0.06f);
    ball(c, 9.0f, g + 0.5f - 6.0f, 5.0f, 4.0f, K.coat, 0.06f);
    capsule(c, V2(21, g - 6.0f), V2(19.5f, g - 0.8f), 1.3f, 1.0f, K.pointsLegs ? K.points : K.coat, -1);   // folded fore
    c.set(20, g, K.hoof[1]); c.set(21, g, K.hoof[0]);
    capsule(c, V2(23.0f, g - 7.0f), V2(27.0f, g - 15.0f), 3.0f, 2.0f, K.coat);
    capsule(c, V2(27.5f, g - 15.5f), V2(31.0f, g - 11.5f), 2.1f, 1.4f, K.coat);
    c.set(27, g - 17, K.coat[3]); c.set(27, g - 18, K.coat[2]);
    c.set(29, g - 15, rgba(70, 50, 60));
    for (int i = 0; i < 8; i++) c.set(21 + (i * 6) / 8, g - 9 - i, K.mane[(i & 1) ? 2 : 3]);
    tube(c, V2(5.0f, g - 7.0f), V2(2.5f, g - 3.0f), V2(4.0f, g - 0.5f), 1.0f, 0.8f, K.mane, 2, 1.4f);
  }

  // ---------------------------------------------------------------- facing the camera
  void front() {
    const float cx = 18.0f, b = P.bob;
    if (P.lying) {
      ball(c, cx, g + 0.5f - 5.0f, 8.0f, 5.0f, K.coat, 0.06f);
      capsule(c, V2(cx, g - 7.0f), V2(cx + 0.5f, g - 13.0f), 3.0f, 2.2f, K.coat);
      ball(c, cx + 0.5f, g - 12.5f, 2.4f, 2.6f, K.coat, 0.05f);
      c.set((int)cx - 1, g - 12, rgba(70, 50, 60)); c.set((int)cx + 2, g - 12, rgba(70, 50, 60));
      return;
    }
    const bool walk = std::fabs(P.sw[0]) + std::fabs(P.lift[0]) > 0.01f;
    (void)walk;
    if (!headOnly) {
    // the hind legs, deep behind and in shade, peeking below the barrel
    for (int s = -1; s <= 1; s += 2) {
      const int i = s < 0 ? 2 : 3;
      const float x = cx + s * 5.0f;   // (M7 fix) the horse broader than its rider: the rider's legs lie on its flanks
      capsule(c, V2(x, g - 14.0f + b), V2(x + s * 0.3f, g - 8.5f - P.lift[i] * 0.5f), 1.5f, 1.0f, K.pointsLegs ? K.points : K.coat, -2);
      c.set((int)x, (int)(g - 8 - P.lift[i] * 0.5f), K.hoof[0]);
    }
    // the rump far behind, the back and barrel rising up the screen (the high camera sees it from above)
    ball(c, cx, g + 0.5f - 25.0f + b, 6.4f, 3.8f, K.coat, 0.05f, -1);
    ball(c, cx, g + 0.5f - 19.5f + b, 7.8f, 6.8f, K.coat, 0.05f);
    if (K.dorsal) for (int y = (int)(g - 28 + b); y <= (int)(g - 15 + b); y++) c.set((int)cx, y, K.points[2]);
    if (K.dapple) for (int i = 0; i < 14; i++) {
      const int x = (int)cx - 5 + (int)(hash3(i, 1, 4443u) % 11), y = (int)(g - 25 + b) + (int)(hash3(i, 2, 4443u) % 10);
      if (solid(c, x, y)) c.set(x, y, K.coat[4]);
    }
    // the tail's tip swinging out behind the rump
    c.set((int)cx + (P.tail > 0 ? 3 : -3), (int)(g - 27 + b), K.mane[2]);
    // the saddle on the back
    if (saddled) {
      for (int y = -4; y <= 3; y++)
        for (int x = -7; x <= 7; x++) {
          const int px = (int)cx + x, py = (int)(g - 20 + b) + y;
          if (!solid(c, px, py)) continue;
          const bool cloth = std::abs(x) >= 5 || y == 3;
          c.set(px, py, cloth ? K.cloth[x < 0 ? 3 : 1] : kLeather[y <= -3 ? 4 : (x < -2 ? 3 : (x > 2 ? 1 : 2))]);
        }
      c.set((int)cx - 1, (int)(g - 25 + b), kLeather[3]); c.set((int)cx, (int)(g - 25 + b), kLeather[2]);   // the cantle
      // stirrups hanging at the sides
      for (int s = -1; s <= 1; s += 2) { const int x = (int)cx + s * 8 - (s > 0 ? 1 : 0); c.set(x, (int)(g - 17 + b), kLeather[0]); c.set(x, (int)(g - 15 + b), kIron[3]); c.set(x, (int)(g - 14 + b), kIron[1]); }
    }
    // the chest, the forelegs
    ball(c, cx - 3.6f, g + 0.5f - 14.0f + b, 3.8f, 4.6f, K.coat, 0.05f);       // the shoulders either side of the chest
    ball(c, cx + 3.6f, g + 0.5f - 14.0f + b, 3.8f, 4.6f, K.coat, 0.05f, -1);
    ball(c, cx, g + 0.5f - 12.5f + b, 6.6f, 4.2f, K.coat, 0.05f);
    for (int y = (int)(g - 15 + b); y <= (int)(g - 10 + b); y++) c.set((int)cx, y, K.coat[1]);   // the cleft between the breast muscles
    for (int s = -1; s <= 1; s += 2) {
      const int i = s < 0 ? 0 : 1;
      const float x = cx + s * 3.6f;
      const float lift = P.lift[i], sw = P.sw[i];
      const float footY = g + 0.5f - lift - std::max(0.0f, sw) * 0.25f;
      const Ramp& lower = K.pointsLegs ? K.points : K.coat;
      capsule(c, V2(x, g - 12.0f + b), V2(x + s * 0.2f, g - 6.0f - lift * 0.6f), 1.7f, 1.1f, K.coat, s > 0 ? -1 : 0);
      capsule(c, V2(x + s * 0.2f, g - 6.0f - lift * 0.6f), V2(x, footY - 1.0f), 1.0f, 0.85f, lower, s > 0 ? -1 : 0);
      const bool sock = K.socks & (s < 0 ? 1 : 2);
      if (sock) for (int y = (int)footY - 3; y < (int)footY; y++) { c.set((int)x - 1, y, K.sock[3]); c.set((int)x, y, K.sock[2]); }
      c.set((int)x - 1, (int)footY, K.hoof[2]); c.set((int)x, (int)footY, K.hoof[1]); c.set((int)x + 1, (int)footY, K.hoof[0]);
    }
    }   // !headOnly
    // (M7 fixer r2, review: "facing south the horse reads as a person with horse legs, or a bear": the head sat low
    // inside the chest's own outline) the neck rises from the breast and the head is carried high in front of it, the
    // poll and the pricked ears up at the withers' height, so its own silhouette stands out over the chest (and over
    // the rider's belly: the view draws this part of the cell in front of the rider)
    const float hy = g + 0.5f - 25.0f + b + P.headDrop * 0.8f + (P.graze ? 11.0f : 0.0f);
    capsule(c, V2(cx, g - 14.0f + b), V2(cx, hy + 3.0f), 3.6f, 3.0f, K.coat, -1);   // the neck, in the head's shade
    // the mane falling to one side of the crest
    for (int y = (int)(hy + 1); y <= (int)(g - 15 + b); y++) { c.set((int)cx - 3, y, K.mane[2]); c.set((int)cx - 4, y, K.twoTone ? K.points[1] : K.mane[1]); }
    // the long face toward the camera, set off from the neck by its own contour: the broad forehead, the narrowing
    // nose, the soft pale muzzle
    layered(c, [&](Canvas& p) {
      capsule(p, V2(cx, hy + 1.0f), V2(cx, hy + 8.4f), 3.2f, 2.1f, K.coat, 1);
      ball(p, cx, hy + 9.0f, 2.5f, 1.9f, K.light, 0.03f, -1);
    }, 0.8f);
    const int hx = (int)cx, hyi = (int)std::floor(hy);
    c.set(hx - 3, hyi + 2, kEye); c.set(hx + 3, hyi + 2, kEye);
    c.set(hx - 3, hyi + 1, K.coat[4]); c.set(hx + 3, hyi + 1, K.coat[2]);   // the brow over each eye
    // the ears, pricked and a little apart (dark-edged so they read at 1x), lit on the west
    for (int k = 0; k < 3; k++) {
      c.set(hx - 2 - (k == 2 ? 1 : 0), hyi - 1 - k, K.coat[k == 2 ? 4 : 3]);
      c.set(hx + 2 + (k == 2 ? 1 : 0), hyi - 1 - k, K.coat[k == 2 ? 2 : 1]);
    }
    c.set(hx - 1, hyi - 1, K.coat[0]); c.set(hx + 1, hyi - 1, K.coat[0]);   // the hollow between them
    c.set(hx, hyi, K.mane[3]); c.set(hx, hyi + 1, K.mane[2]);   // forelock
    c.set(hx - 1, hyi + 9, K.coat[0]); c.set(hx + 1, hyi + 9, K.coat[0]);   // nostrils
    if (K.face) for (int y = hyi + 2; y <= hyi + (K.face == 2 ? 8 : 3); y++) c.set(hx, y, K.sock[3]);
    if (saddled) {   // the bridle: browband and noseband, the reins down to the withers
      for (int x = hx - 3; x <= hx + 3; x++) c.set(x, hyi + 1, kLeather[x < hx ? 2 : 1]);
      for (int x = hx - 2; x <= hx + 2; x++) c.set(x, hyi + 7, kLeather[x < hx ? 2 : 1]);
      c.set(hx - 3, hyi + 8, kIron[3]); c.set(hx + 3, hyi + 8, kIron[2]);
      line(c, hx - 4, hyi + 8, hx - 5, (int)(g - 17 + b), kLeather[1]);
      line(c, hx + 4, hyi + 8, hx + 5, (int)(g - 17 + b), kLeather[0]);
    }
  }

  // ---------------------------------------------------------------- walking away
  void back() {
    const float cx = 18.0f, b = P.bob;
    if (P.lying) {
      ball(c, cx, g + 0.5f - 5.5f, 8.0f, 5.0f, K.coat, 0.06f);
      tube(c, V2(cx, g - 7.0f), V2(cx + 1.5f, g - 4.0f), V2(cx + 0.5f, g - 1.0f), 1.0f, 0.8f, K.mane, 2, 1.3f);
      return;
    }
    // the head and neck beyond the back (hidden behind the rider when saddled)
    if (!saddled) {
      // (M7 fix) the head held up over the withers, the ears pricked and the mane down the crest, so a waiting horse
      // seen from behind reads as a horse (a plain brown lozenge before)
      const float hy = g + 0.5f - 27.0f + b + (P.graze ? 7.0f : 0.0f);
      capsule(c, V2(cx, g - 21.0f + b), V2(cx, hy + 1.0f), 3.2f, 2.4f, K.coat, -1);
      ball(c, cx, hy, 2.6f, 2.2f, K.coat, 0.04f, -1);
      const int hx = (int)cx, hyi = (int)std::floor(hy);
      for (int k = 0; k < 3; k++) { c.set(hx - 2, hyi - 2 - k, K.coat[k == 2 ? 4 : 3]); c.set(hx + 2, hyi - 2 - k, K.coat[k == 2 ? 3 : 1]); }   // the ears
      c.set(hx - 1, hyi - 2, K.coat[1]); c.set(hx + 1, hyi - 2, K.coat[0]);
      for (int y = hyi - 1; y <= (int)(g - 20 + b); y++) { c.set(hx, y, K.mane[(y & 1) ? 3 : 2]); c.set(hx - 1, y, K.mane[1]); }
    }
    // the forelegs, far and in shade, between the hind legs
    for (int s = -1; s <= 1; s += 2) {
      const float x = cx + s * 2.5f;
      capsule(c, V2(x, g - 13.0f + b), V2(x, g - 6.0f - P.lift[s < 0 ? 0 : 1] * 0.5f), 1.1f, 0.8f, K.pointsLegs ? K.points : K.coat, -2);
    }
    // the back (saddled: only up to the cantle; the rider's body covers the rest)
    if (!saddled) {
      ball(c, cx, g + 0.5f - 20.5f + b, 7.0f, 5.8f, K.coat, 0.05f);
      for (int y = (int)(g - 25 + b); y <= (int)(g - 17 + b); y++) c.set((int)cx - 1, y, K.coat[4]);   // the light along the spine
    }
    else {
      for (int y = (int)(g - 22 + b); y <= (int)(g - 14 + b); y++)
        for (int x = (int)cx - 7; x <= (int)cx + 6; x++) {
          const float dx = (x + 0.5f - cx) / 7.0f, dy = (y + 0.5f - (g - 18.0f + b)) / 4.5f;
          if (dx * dx + dy * dy > 1.0f) continue;
          const bool cloth = std::fabs(dx) > 0.66f;
          c.set(x, y, cloth ? K.cloth[x < cx ? 3 : 1] : kLeather[x < cx - 2 ? 3 : (x > cx + 1 ? 1 : 2)]);
        }
      for (int x = (int)cx - 3; x <= (int)cx + 2; x++) c.set(x, (int)(g - 18 + b), kLeather[x < cx ? 4 : 3]);   // the cantle's lit rim
    }
    // the hind legs and the rump nearest the camera
    for (int s = -1; s <= 1; s += 2) {
      const int i = s < 0 ? 2 : 3;
      const float x = cx + s * 4.0f, lift = P.lift[i];
      const float footY = g + 0.5f - lift;
      const Ramp& lower = K.pointsLegs ? K.points : K.coat;
      capsule(c, V2(x, g - 12.0f + b), V2(x + s * 0.4f, g - 6.0f - lift * 0.5f), 2.0f, 1.0f, K.coat, s > 0 ? -1 : 0);
      capsule(c, V2(x + s * 0.4f, g - 6.0f - lift * 0.5f), V2(x, footY - 1.0f), 0.95f, 0.85f, lower, s > 0 ? -1 : 0);
      if (K.socks & (s < 0 ? 4 : 8)) for (int y = (int)footY - 3; y < (int)footY; y++) { c.set((int)x - 1, y, K.sock[3]); c.set((int)x, y, K.sock[2]); }
      c.set((int)x - 1, (int)footY, K.hoof[2]); c.set((int)x, (int)footY, K.hoof[1]); c.set((int)x + 1, (int)footY, K.hoof[0]);
    }
    ball(c, cx - 3.2f, g + 0.5f - 14.0f + b, 4.4f, 4.6f, K.coat, 0.05f);
    ball(c, cx + 3.2f, g + 0.5f - 14.0f + b, 4.4f, 4.6f, K.coat, 0.05f, -1);
    for (int y = (int)(g - 17 + b); y <= (int)(g - 11 + b); y++) c.set((int)cx, y, K.coat[0]);   // the cleft of the quarters
    if (K.dorsal) for (int y = (int)(g - 18 + b); y <= (int)(g - 16 + b); y++) c.set((int)cx, y, K.points[2]);
    // the tail falling over the buttocks between the hind legs
    const float sway = P.tail;
    tube(c, V2(cx, g - 17.5f + b), V2(cx + sway * 0.6f, g - 12.0f + b), V2(cx + sway * 1.4f, g - 6.0f + b), 1.0f, 1.2f, K.mane, 2, 1.6f);
    c.set((int)(cx + sway * 1.4f), (int)(g - 6 + b), K.mane[1]);
  }
};

Canvas horseCellImpl(uint32_t coat, uint32_t variant, bool saddled, int row, int pose) {
  Canvas cell(HORSE_W, HORSE_H);
  const HorseCoat K = horseCoat(coat, variant);
  const HorsePose P = horsePose(pose);
  HorseRig rig(cell, K, P, saddled);
  rig.headOnly = row == 3;
  if (row == 0 || row == 3) rig.front();
  else if (row == 1) rig.back();
  else rig.side();
  outline(cell, 0.9f);
  return cell;
}

}  // namespace

// ---------------------------------------------------------------- the riding horse
Canvas horseSheet(uint32_t coat, uint32_t variant, bool saddled) {
  Canvas out(HORSE_W * MOUNT_FRAMES, HORSE_H * 4);
  for (int row = 0; row < 4; row++)
    for (int f = 0; f < MOUNT_FRAMES; f++) place(out, horseCellImpl(coat, variant, saddled, row, f), f, row);
  return out;
}
Canvas horseCell(uint32_t coat, uint32_t variant, bool saddled, int row, int pose) { return horseCellImpl(coat, variant, saddled, row, pose); }
void riderSeat(int row, int frame, int& dx, int& dy) {
  const HorsePose P = horsePose(std::clamp(frame, 0, MOUNT_FRAMES - 1));
  const int bob = (int)std::lround(P.bob);
  // the rider's cell is drawn with its bottom-centre at (dx, dy) from the horse cell's: its hips (the Ride cell's
  // RIDE_SEAT_ROW) land on the saddle
  if (row == 2) { dx = -2; dy = -13 + bob; }
  else if (row == 1) { dx = 0; dy = -14 + bob; }
  else { dx = 0; dy = -16 + bob; }
}

// ================================================================ yard materials and the little volume renderer
namespace {

// the classic and culture roof / wall palettes (the same base colours as the building painter's, so a yard matches its
// house)
const Ramp kYRoofBrown = ramp5(rgba(54, 34, 38), rgba(88, 56, 46), rgba(124, 82, 58), rgba(158, 112, 74), rgba(192, 148, 100));
const Ramp kYRoofRed = ramp5(rgba(76, 30, 40), rgba(124, 46, 44), rgba(170, 70, 52), rgba(204, 104, 70), rgba(232, 150, 102));
const Ramp kYRoofSlate = ramp5(rgba(38, 42, 64), rgba(58, 66, 92), rgba(82, 94, 122), rgba(114, 128, 154), rgba(156, 170, 190));
const Ramp kYClay = ramp5(rgba(92, 38, 36), rgba(150, 62, 42), rgba(196, 98, 56), rgba(224, 140, 80), rgba(244, 186, 120));
const Ramp kYCopper = ramp5(rgba(30, 70, 72), rgba(52, 112, 104), rgba(82, 152, 130), rgba(126, 190, 158), rgba(184, 226, 192));
const Ramp kYAdobe = ramp5(rgba(122, 82, 64), rgba(172, 122, 86), rgba(212, 166, 114), rgba(232, 198, 144), rgba(246, 226, 182));
const Ramp kYTurf = ramp5(rgba(32, 58, 50), rgba(52, 92, 56), rgba(82, 128, 58), rgba(122, 162, 68), rgba(172, 198, 96));
const Ramp kYPalm = ramp5(rgba(56, 54, 40), rgba(94, 88, 52), rgba(138, 128, 68), rgba(178, 166, 92), rgba(212, 200, 132));
const Ramp kYBarkRoof = ramp5(rgba(40, 30, 38), rgba(64, 50, 48), rgba(94, 74, 60), rgba(126, 104, 80), rgba(160, 140, 110));
const Ramp kYFelt = ramp5(rgba(118, 102, 96), rgba(168, 150, 132), rgba(212, 198, 172), rgba(232, 222, 200), rgba(248, 242, 226));
const Ramp kYJade = ramp5(rgba(14, 50, 56), rgba(24, 88, 80), rgba(40, 132, 104), rgba(88, 180, 132), rgba(178, 230, 182));
const Ramp kYLeafRoof = ramp5(rgba(24, 50, 46), rgba(38, 84, 52), rgba(66, 122, 56), rgba(112, 162, 66), rgba(182, 206, 104));
const Ramp kYPlaster = ramp5(rgba(150, 128, 112), rgba(196, 174, 146), rgba(228, 212, 180), rgba(242, 232, 206), rgba(252, 248, 230));
const Ramp kYLog = ramp5(rgba(52, 32, 36), rgba(86, 56, 44), rgba(124, 84, 58), rgba(160, 118, 78), rgba(196, 158, 108));
const Ramp kYRubble = ramp5(rgba(58, 50, 58), rgba(96, 86, 86), rgba(136, 124, 112), rgba(174, 162, 142), rgba(208, 198, 176));
const Ramp kYAshlar = ramp5(rgba(122, 108, 108), rgba(178, 164, 150), rgba(222, 210, 186), rgba(240, 232, 212), rgba(252, 250, 238));
const Ramp kYDaub = ramp5(rgba(108, 84, 64), rgba(158, 126, 90), rgba(198, 168, 120), rgba(220, 196, 150), rgba(240, 224, 186));
const Ramp kYLiving = ramp5(rgba(42, 34, 40), rgba(70, 54, 48), rgba(104, 82, 62), rgba(140, 114, 82), rgba(176, 152, 108));
const Ramp kYLacquer = ramp5(rgba(70, 18, 30), rgba(120, 28, 34), rgba(170, 44, 40), rgba(208, 80, 56), rgba(238, 136, 96));
const Ramp kYTar = ramp5(rgba(30, 24, 30), rgba(48, 36, 40), rgba(68, 52, 50), rgba(94, 74, 64), rgba(126, 104, 86));
const Ramp kYGreyWood = ramp5(rgba(56, 50, 56), rgba(88, 80, 82), rgba(120, 110, 104), rgba(156, 144, 130), rgba(190, 180, 160));
const Ramp kYPaleWood = ramp5(rgba(108, 84, 62), rgba(156, 126, 90), rgba(196, 166, 120), rgba(222, 198, 152), rgba(244, 228, 190));
const Ramp kYWhite = ramp5(rgba(140, 136, 140), rgba(192, 190, 188), rgba(226, 224, 216), rgba(242, 240, 232), rgba(254, 252, 246));
const Ramp kYBamboo = ramp5(rgba(70, 76, 34), rgba(112, 122, 50), rgba(156, 166, 76), rgba(194, 200, 110), rgba(226, 228, 160));
const Ramp kYHay = ramp5(rgba(120, 96, 42), rgba(172, 140, 58), rgba(212, 180, 82), rgba(236, 212, 120), rgba(250, 236, 170));

enum FenceKind { kFRail, kFPicket, kFWattle, kFDyke, kFBamboo, kFRope, kFHedge };
enum HiveKind { kHiveSkep, kHiveBox, kHivePipe };

// the look of a yard in a culture: its woods, stone, roofing and walling, cloth, the fence it builds
struct YardLook {
  int cul = -1;                 // cult::Archetype (-1: the classic look)
  Ramp wood, woodDark, stone, roof, wall, trim, cloth, metal;
  RoofMat roofMat = RoofMat::Thatch;
  WallMat wallMat = WallMat::Plank;
  RoofShape roofShape = RoofShape::Gable;
  FenceKind fence = kFRail;
  HiveKind hive = kHiveSkep;
  bool snow = false, stoneBase = false;
  uint32_t accent = 0;
};

Ramp roofRampOf(RoofMat m, uint32_t tint, uint32_t seed) {
  switch (m) {
    case RoofMat::Thatch: return kThatch;
    case RoofMat::Shingle: { static const Ramp* o[4] = {&kYRoofBrown, &kYRoofRed, &kYRoofBrown, &kYRoofBrown}; return tint ? ramp(tint) : *o[seed % 4]; }
    case RoofMat::Slate: return tint ? ramp(mix(tint, rgba(84, 94, 122), 0.55f)) : kYRoofSlate;
    case RoofMat::ClayTile: return tint ? ramp(mix(tint, rgba(196, 98, 56), 0.6f)) : kYClay;
    case RoofMat::Turf: return kYTurf;
    case RoofMat::Adobe: return tint ? ramp(tint, 0.8f) : kYAdobe;
    case RoofMat::Copper: return tint ? ramp(mix(tint, rgba(82, 152, 130), 0.5f)) : kYCopper;
    case RoofMat::Palm: return kYPalm;
    case RoofMat::Bark: return kYBarkRoof;
    case RoofMat::Felt: return tint ? ramp(tint, 0.8f) : kYFelt;
    case RoofMat::GlazedTile: return tint ? ramp(tint) : kYJade;
    case RoofMat::Leaf: return tint ? ramp(tint) : kYLeafRoof;
    default: return kYRoofBrown;
  }
}
Ramp wallRampOf(WallMat w, uint32_t tint) {
  switch (w) {
    case WallMat::Timber: case WallMat::Plaster: return tint ? ramp(tint) : kYPlaster;
    case WallMat::Stone: return kStone;
    case WallMat::Brick: return tint ? ramp(mix(tint, rgba(156, 80, 58), 0.6f)) : kBrick;
    case WallMat::Log: return kYLog;
    case WallMat::Adobe: return tint ? ramp(tint, 0.8f) : kYAdobe;
    case WallMat::Plank: return tint ? ramp(tint, 0.9f) : kWood;
    case WallMat::Rubble: return tint ? ramp(mix(tint, rgba(136, 124, 112), 0.5f)) : kYRubble;
    case WallMat::Wattle: return tint ? ramp(tint, 0.8f) : kYDaub;
    case WallMat::Felt: return tint ? ramp(tint, 0.7f) : kYFelt;
    case WallMat::Ashlar: return tint ? ramp(tint, 0.75f) : kYAshlar;
    case WallMat::Living: return tint ? ramp(tint) : kYLiving;
    default: return kWood;
  }
}

YardLook yardLook(const FarmObjLook& l) {
  YardLook Y;
  Y.wood = kWood; Y.woodDark = kWoodDark; Y.stone = kStone; Y.roof = kThatch; Y.wall = kWood; Y.trim = kWoodDark;
  Y.cloth = kCloth; Y.metal = kIron;
  if (!l.style) return Y;
  const ArchStyle& s = *l.style;
  Y.cul = (int)s.culture - 1;
  Y.roofMat = s.roofMat;
  Y.wallMat = s.wall;
  Y.roofShape = s.roof;
  Y.snow = s.snow;
  Y.roof = roofRampOf(s.roofMat, s.roofTint, l.variant);
  Y.wall = wallRampOf(s.wall, s.wallTint);
  Y.accent = s.accentTint;
  Y.stoneBase = s.foundation == Foundation::Plinth || s.foundation == Foundation::Terrace || s.foundation == Foundation::Platform;
  switch (Y.cul) {
    case 0: Y.wood = kYTar; Y.woodDark = ramp5(rgba(22, 18, 24), rgba(36, 28, 32), rgba(52, 40, 42), rgba(74, 58, 54), rgba(100, 82, 70)); Y.fence = kFRail; break;   // Fjordfolk: tarred pine
    case 1: Y.wood = kYGreyWood; Y.fence = kFDyke; Y.stone = kStone; break;                       // Highland: weathered grey, dry-stone dykes
    case 2: Y.fence = kFWattle; break;                                                             // Heartland: hazel wattle
    case 3: Y.wood = kWood; Y.fence = kFPicket; Y.hive = kHiveBox; Y.stone = kYAshlar; break;      // Imperial: white pales, box hives
    case 4: Y.wood = kYPaleWood; Y.fence = kFDyke; Y.hive = kHivePipe; Y.stone = kYAdobe; break;   // Dune: mud walls, clay-pipe hives
    case 5: Y.wood = kYPaleWood; Y.fence = kFRope; break;                                          // Steppe: posts and ropes
    case 6: Y.wood = kYGreyWood; Y.fence = kFWattle; break;                                        // Marsh: reed wattle
    case 7: Y.wood = kYLacquer; Y.woodDark = kWoodDark; Y.fence = kFBamboo; Y.hive = kHiveBox; break;   // Jade: lacquer and bamboo
    case 8: Y.fence = kFPicket; Y.hive = kHiveBox; break;                                          // River
    case 9: Y.wood = kYPaleWood; Y.fence = kFDyke; Y.hive = kHivePipe; Y.stone = kYAshlar; break;  // Sun temple: lime-plastered walls
    case 10: Y.wood = kYLiving; Y.fence = kFHedge; break;                                          // Sylvan: living hedges
    case 11: Y.wood = kYPaleWood; Y.fence = kFPicket; Y.hive = kHiveBox; Y.stone = kYAshlar; break;   // Starspire: pale wood, white stone
    default: break;
  }
  if (s.wall == WallMat::Stone || s.wall == WallMat::Rubble || s.wall == WallMat::Ashlar) Y.stone = wallRampOf(s.wall, s.wallTint);
  if (s.trimTint) Y.trim = ramp(s.trimTint);
  if (s.accentTint) Y.cloth = ramp(s.accentTint);
  return Y;
}
bool picketWhite(const YardLook& Y) { return Y.cul == 3 || Y.cul == 11; }

// ---------------------------------------------------------------- the little volume renderer
// World units are canvas pixels: x east, y south (the ground row), z up; a point lands on the canvas at (x, y - z).
// A scene is a list of solids, each a footprint with a top height function and a bottom; every canvas pixel casts its
// ray from the camera (south, above) back into the scene and takes the first solid it meets: a TOP face (lit by its
// slope from the top-left) or a FRONT face (the south-facing wall, base tone). Parts paint their own texture.
struct Hit {
  int part = -1;
  bool top = false;
  float x = 0, y = 0, z = 0;     // the world point hit
  float light = 0;               // lightAt of the face (top faces)
  float nx = 0, ny = 0;          // the top's slope (dz/dx, dz/dy)
  int px = 0, py = 0;            // the canvas pixel
};
enum PartShape { kBox, kGableX, kGableY, kHip, kShedS, kCone, kCyl, kDome, kPyramid, kEllip, kRing };
struct Part {
  PartShape shape = kBox;
  float x0 = 0, x1 = 0, y0 = 0, y1 = 0;   // footprint
  float z0 = 0, z1 = 0;                   // base and top (roofs: eave and ridge)
  float thick = 0;                        // > 0: a slab this thick under its top (roofs, lids)
  const Ramp* R = nullptr;
  int tex = 0;                            // texture id (see paintHit)
  int bias = 0;                           // ramp shift
  uint32_t seed = 0;
  std::function<bool(const Hit&, uint32_t&)> paint;   // optional: a custom colour (return false to use the default)
  float top(float x, float y) const {
    const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f, hx = (x1 - x0) * 0.5f, hy = (y1 - y0) * 0.5f;
    switch (shape) {
      case kBox: return z1;
      case kGableX: return z0 + (z1 - z0) * (1.0f - std::fabs(y - cy) / hy);        // ridge along x
      case kGableY: return z0 + (z1 - z0) * (1.0f - std::fabs(x - cx) / hx);        // ridge along y
      case kHip: {
        const float a = 1.0f - std::fabs(y - cy) / hy, b = (hx - std::fabs(x - cx)) / hy;
        return z0 + (z1 - z0) * std::max(0.0f, std::min(a, b));
      }
      case kPyramid: return z0 + (z1 - z0) * std::max(0.0f, std::min(1.0f - std::fabs(x - cx) / hx, 1.0f - std::fabs(y - cy) / hy));
      case kShedS: return z1 - (z1 - z0) * ((y - y0) / std::max(0.01f, y1 - y0));    // high at the back, down to the front
      case kCone: { const float d = std::sqrt(((x - cx) / hx) * ((x - cx) / hx) + ((y - cy) / hy) * ((y - cy) / hy)); return d > 1 ? -1e9f : z0 + (z1 - z0) * (1 - d); }
      case kCyl: { const float d = ((x - cx) / hx) * ((x - cx) / hx) + ((y - cy) / hy) * ((y - cy) / hy); return d > 1 ? -1e9f : z1; }
      case kRing: { const float d = ((x - cx) / hx) * ((x - cx) / hx) + ((y - cy) / hy) * ((y - cy) / hy); return (d > 1 || d < 0.45f) ? -1e9f : z1; }
      case kDome: { const float d = ((x - cx) / hx) * ((x - cx) / hx) + ((y - cy) / hy) * ((y - cy) / hy); return d > 1 ? -1e9f : z0 + (z1 - z0) * std::sqrt(1 - d); }
      case kEllip: { const float d = ((x - cx) / hx) * ((x - cx) / hx) + ((y - cy) / hy) * ((y - cy) / hy); return d > 1 ? -1e9f : z0 + (z1 - z0) * std::sqrt(1 - d); }
    }
    return z1;
  }
  float bottom(float x, float y) const {
    if (shape == kEllip) return 2 * z0 - top(x, y);
    if (thick > 0) return top(x, y) - thick;
    return z0 > 0 && (shape == kGableX || shape == kGableY || shape == kHip || shape == kShedS || shape == kPyramid || shape == kCone) ? z0 - 0.01f : z0;
  }
  bool inside(float x, float y) const { return x >= x0 && x < x1 && y >= y0 && y < y1; }
};

struct Scene {
  std::vector<Part> parts;
  Part& add(PartShape s, float x0, float x1, float y0, float y1, float z0, float z1, const Ramp& R, int tex = 0) {
    Part p;
    p.shape = s; p.x0 = x0; p.x1 = x1; p.y0 = y0; p.y1 = y1; p.z0 = z0; p.z1 = z1; p.R = &R; p.tex = tex;
    parts.push_back(p);
    return parts.back();
  }
  // cast every pixel; paint the hits; mark depth edges with a contour
  void render(Canvas& c, std::vector<float>* depthOut = nullptr) const;
};

// default texture of a hit: ramp index from the light (tops) or the base (fronts), then the part's pattern
uint32_t shadeHit(const Part& P, const Hit& h);

void Scene::render(Canvas& c, std::vector<float>* depthOut) const {
  std::vector<float> depth((size_t)c.w * c.h, -1e9f);
  std::vector<int> owner((size_t)c.w * c.h, -1);
  float zMax = 0, yMax = 0, yMin = 1e9f;
  for (const Part& p : parts) { zMax = std::max(zMax, p.z1 + 1); yMax = std::max(yMax, p.y1); yMin = std::min(yMin, p.y0); }
  for (int py = 0; py < c.h; py++)
    for (int px = 0; px < c.w; px++) {
      const float X = px + 0.5f, s = py + 0.5f;
      // the ray: z = y - s; start where it is above everything, march back (north) until it is underground
      float Y = std::min(yMax, s + zMax);
      Y = std::floor(Y * 4) / 4;
      int prevInside[64];
      for (size_t i = 0; i < parts.size() && i < 64; i++) prevInside[i] = 0;
      Hit hit;
      for (; Y >= yMin - 0.01f; Y -= 0.25f) {
        const float Z = Y - s;
        if (Z < -0.3f) break;
        int best = -1;
        float bestTop = -1e9f;
        for (size_t i = 0; i < parts.size(); i++) {
          const Part& P = parts[i];
          if (!P.inside(X, Y)) { if (i < 64) prevInside[i] = 0; continue; }
          const float t = P.top(X, Y);
          const bool col = t > -1e8f;
          if (!col) { if (i < 64) prevInside[i] = 0; continue; }
          const float b = P.bottom(X, Y);
          if (Z <= t + 0.001f && Z >= b - 0.001f && t >= bestTop) { best = (int)i; bestTop = t; }
          if (i < 64) prevInside[i] = prevInside[i] ? prevInside[i] : 1;
        }
        if (best >= 0) {
          const Part& P = parts[(size_t)best];
          hit.part = best; hit.x = X; hit.y = Y; hit.z = Z; hit.px = px; hit.py = py;
          // a top face when the ray came down through the top (the column was inside the part a step nearer and the
          // ray was above its top there); else the front face
          const float tPrev = P.inside(X, Y + 0.25f) ? P.top(X, Y + 0.25f) : -1e9f;
          hit.top = tPrev > -1e8f && (Z + 0.25f) > tPrev - 0.001f;
          if (P.shape == kBox && !P.inside(X, Y + 0.25f)) hit.top = Z > P.z1 - 0.26f;
          if (hit.top) {
            const float e = 0.5f;
            auto T = [&](float xx, float yy) { const float v = P.inside(xx, yy) ? P.top(xx, yy) : -1e9f; return v < -1e8f ? P.top(X, Y) : v; };
            const float dzx = (T(X + e, Y) - T(X - e, Y)) / (2 * e), dzy = (T(X, Y + e) - T(X, Y - e)) / (2 * e);
            hit.nx = dzx; hit.ny = dzy;
            // the surface normal (x east, y south, z up) against the light from the north-west and above: a flat top
            // is lit, a slope facing west or north brighter, one facing south mid, east in shade
            float nx = -dzx, ny = -dzy, nz = 1.0f;
            const float L = std::sqrt(nx * nx + ny * ny + nz * nz);
            nx /= L; ny /= L; nz /= L;
            const float l = nx * -0.6f + ny * -0.25f + nz * 0.76f;
            // remap onto lightIndex's bands: flat and north slopes -> 3, west slopes -> 4, south slopes -> 2, east -> 1
            hit.light = l > 0.88f ? 0.95f : (l > 0.62f ? 0.75f : (l > 0.2f ? 0.45f : (l > -0.05f ? 0.0f : -0.5f)));
          } else {
            hit.light = 0.5f;
          }
          const uint32_t col = shadeHit(P, hit);
          c.set(px, py, col);
          depth[(size_t)py * c.w + px] = Y;
          owner[(size_t)py * c.w + px] = best;
          break;
        }
      }
    }
  // contours: where a nearer surface overlaps a farther one (a roof's eave over its wall, a post before a rail), the
  // farther pixel at the step darkens: crisp edges inside the silhouette (the silhouette itself is outline()'s)
  Canvas src = c;
  for (int py = 0; py < c.h; py++)
    for (int px = 0; px < c.w; px++) {
      const float d = depth[(size_t)py * c.w + px];
      if (d < -1e8f) continue;
      static const int nx[4] = {0, -1, 1, 0}, ny[4] = {-1, 0, 0, 1};
      for (int k = 0; k < 4; k++) {
        const int qx = px + nx[k], qy = py + ny[k];
        if (qx < 0 || qy < 0 || qx >= c.w || qy >= c.h) continue;
        const float dq = depth[(size_t)qy * c.w + qx];
        if (dq < -1e8f) continue;
        if (dq > d + 1.6f && owner[(size_t)qy * c.w + qx] != owner[(size_t)py * c.w + px]) {
          c.set(px, py, outlineOf(src.get(qx, qy), 0.55f));
          break;
        }
      }
    }
  if (depthOut) *depthOut = depth;
}

// ---------------------------------------------------------------- textures
enum Tex {
  kTexPlain = 0, kTexPlanksV, kTexPlanksH, kTexLogs, kTexStone, kTexBrick, kTexPlaster, kTexAdobe, kTexWattle, kTexFelt,
  kTexThatch, kTexShingle, kTexSlate, kTexTile, kTexTurf, kTexPalm, kTexBark, kTexCopper, kTexLeafRoof, kTexWater,
  kTexHay, kTexLogEnds, kTexCoals, kTexMetal, kTexGlazed, kTexAshlar, kTexTimber
};
int roofTex(RoofMat m) {
  switch (m) {
    case RoofMat::Thatch: return kTexThatch;
    case RoofMat::Shingle: return kTexShingle;
    case RoofMat::Slate: return kTexSlate;
    case RoofMat::ClayTile: return kTexTile;
    case RoofMat::Turf: return kTexTurf;
    case RoofMat::Adobe: return kTexAdobe;
    case RoofMat::Copper: return kTexCopper;
    case RoofMat::Palm: return kTexPalm;
    case RoofMat::Bark: return kTexBark;
    case RoofMat::Felt: return kTexFelt;
    case RoofMat::GlazedTile: return kTexGlazed;
    case RoofMat::Leaf: return kTexLeafRoof;
    default: return kTexShingle;
  }
}
int wallTex(WallMat w) {
  switch (w) {
    case WallMat::Timber: return kTexTimber;
    case WallMat::Plaster: return kTexPlaster;
    case WallMat::Stone: case WallMat::Rubble: return kTexStone;
    case WallMat::Brick: return kTexBrick;
    case WallMat::Log: return kTexLogs;
    case WallMat::Adobe: return kTexAdobe;
    case WallMat::Plank: return kTexPlanksV;
    case WallMat::Wattle: return kTexWattle;
    case WallMat::Felt: return kTexFelt;
    case WallMat::Ashlar: return kTexAshlar;
    case WallMat::Living: return kTexBark;
    default: return kTexPlanksV;
  }
}

uint32_t shadeHit(const Part& P, const Hit& h) {
  if (P.paint) { uint32_t col = 0; if (P.paint(h, col)) return col; }
  const Ramp& R = *P.R;
  int k = h.top ? lightIndex(h.light, h.px, h.py, 0.12f) : 2;
  k += P.bias;
  // texture coordinates: tops: u across the slope, v up the slope (the height: courses run along the eaves); fronts:
  // u = x, v = z
  const bool slopeY = std::fabs(h.ny) >= std::fabs(h.nx);
  const float u = h.top ? (slopeY ? h.x : h.y) : h.x;
  const float v = h.top ? (std::fabs(h.nx) + std::fabs(h.ny) > 0.05f ? h.z : h.y) : h.z;
  const int ui = (int)std::floor(u), vi = (int)std::floor(v);
  const uint32_t hs = hash3(ui, vi, P.seed + 17u);
  switch (P.tex) {
    case kTexPlanksV: {   // vertical boards: a seam every 4 px, the board lit on its left
      const int b = ((ui % 4) + 4) % 4;
      if (b == 0) k -= 1; else if (b == 1) k += 1;
      if ((hs % 23) == 0) k -= 1;   // a knot
      break;
    }
    case kTexPlanksH: { const int b = ((vi % 3) + 3) % 3; if (b == 0) k -= 1; break; }
    case kTexLogs: { const int b = ((vi % 3) + 3) % 3; k += b == 2 ? 1 : (b == 0 ? -1 : 0); break; }
    case kTexStone: case kTexAshlar: {   // coursed stones, joints darker, faces varied
      const int ch = P.tex == kTexAshlar ? 4 : 3, cw = P.tex == kTexAshlar ? 7 : 5;
      const int row = (int)std::floor(v / ch);
      const int bx = (int)std::floor((u + (row & 1) * (cw / 2)) / cw);
      const bool joint = (((int)std::floor(v) % ch) + ch) % ch == 0 || ((((int)std::floor(u) + (row & 1) * (cw / 2)) % cw) + cw) % cw == 0;
      if (joint) k -= 1;
      else if (hash3(bx, row, P.seed) % 3 == 0) k += 1;
      else if (hash3(bx, row, P.seed + 1) % 4 == 0) k -= 1;
      break;
    }
    case kTexBrick: { const int row = (int)std::floor(v / 2); const bool joint = (((int)std::floor(v) % 2) + 2) % 2 == 0 || ((((int)std::floor(u) + (row & 1) * 2) % 4) + 4) % 4 == 0; if (joint) k -= 1; break; }
    case kTexPlaster: case kTexAdobe: { if ((hs % 17) == 0) k -= 1; if (P.tex == kTexAdobe && (hs % 29) == 1) k += 1; break; }
    case kTexTimber: {   // plaster panels between dark beams
      const int bx = ((ui % 8) + 8) % 8;
      if (bx == 0 || vi <= 1 || (((vi % 9) + 9) % 9) == 0) { return (*P.R)[0] == kYPlaster[0] ? kYLog[std::max(0, k - 1)] : kWoodDark[std::max(0, k - 1)]; }
      break;
    }
    case kTexWattle: { const bool over = (((ui / 2) + (vi / 2)) & 1) != 0; k += over ? 0 : -1; break; }
    case kTexFelt: { if ((((vi % 6) + 6) % 6) == 0) k -= 1; break; }
    case kTexThatch: case kTexPalm: {   // straw laid in courses: each course's butt ends shaded by the one above
      const int course = (int)std::floor(v / 3.0f);
      const int inC = (((int)std::floor(v) % 3) + 3) % 3;
      const uint32_t q = hash3(ui + course * 2, course, P.seed);
      if (inC == 0 && (q & 1)) k -= 1;              // the shadow line under the course above
      else if (inC == 2 && (q % 3) == 0) k += 1;    // straw ends catching the light
      if ((q % 7) == 0) k -= 1;
      break;
    }
    case kTexShingle: { const int row = (int)std::floor(v / 3); const int bx = (int)std::floor((u + (row & 1) * 2) / 4); if ((((int)std::floor(v) % 3) + 3) % 3 == 0) k -= 1; else if (hash3(bx, row, P.seed) % 4 == 0) k += 1; break; }
    case kTexSlate: { const int row = (int)std::floor(v / 2); const int bx = (int)std::floor((u + (row & 1) * 2) / 3); if ((((int)std::floor(v) % 2) + 2) % 2 == 0) k -= 1; else if (hash3(bx, row, P.seed) % 3 == 0) k += 1; break; }
    case kTexTile: case kTexGlazed: {   // barrel tiles: lit and shaded columns, a course step every 4 px
      const int col = ((ui % 3) + 3) % 3;
      k += col == 0 ? 1 : (col == 2 ? -1 : 0);
      if ((((vi % 4) + 4) % 4) == 0) k -= 1;
      if (P.tex == kTexGlazed && col == 0 && (((vi % 4) + 4) % 4) == 2) k += 1;
      break;
    }
    case kTexTurf: { if ((hs % 4) == 0) k += 1; if ((hs % 7) == 1) k -= 1; if ((hs % 41) == 3) return rgba(240, 214, 96); break; }
    case kTexBark: { if ((hash3(ui, vi / 3, P.seed) % 3) == 0) k -= 1; if ((hs % 9) == 0) k += 1; break; }
    case kTexCopper: { if ((((ui % 4) + 4) % 4) == 0) k += 1; break; }
    case kTexLeafRoof: { if ((hs % 3) == 0) k += 1; if ((hs % 5) == 1) k -= 1; break; }
    case kTexWater: { k = 1 + ((hs % 7) == 0 ? 2 : 0); if (((ui + vi) % 9) == 0) k = 3; break; }
    case kTexHay: { if ((hs % 3) == 0) k += 1; if ((hs % 5) == 1) k -= 1; break; }
    case kTexLogEnds: {   // log ends in a stack: discs with rings, bark rims, gaps between
      const int row = (int)std::floor(v / 5);
      const float lu = std::fmod(u + (row & 1) * 2.5f + 500.0f, 5.0f) - 2.5f, lv = std::fmod(v + 500.0f, 5.0f) - 2.5f;
      const float d = std::sqrt(lu * lu + lv * lv) + (hash3((int)std::floor((u + (row & 1) * 2.5f) / 5), row, 7u) % 3) * 0.15f;
      if (d > 2.6f) return kWoodDark[0];
      if (d > 2.0f) return kBark[(lu + lv < 0) ? 3 : 1];
      return kYPaleWood[d < 0.7f ? 2 : ((lu + lv) < 0 ? 4 : 3)];
    }
    case kTexCoals: { const uint32_t q = hash3(h.px, h.py, P.seed); return q % 3 == 0 ? kFire[3] : (q % 3 == 1 ? kFire[1] : kFire[0]); }
    case kTexMetal: { if (h.top) k = std::min(4, k + 1); break; }
    default: break;
  }
  if (!h.top && P.thick > 0 && P.shape != kBox) return R[1];   // a roof's eave edge: in its own shade
  if (!h.top) {
    // front faces: the corner catches the light on the left, the foot is in shade
    if (h.x - P.x0 < 1.0f) k += 1;
    if (h.z - P.z0 < 1.0f && P.thick <= 0) k -= 1;
    if (P.x1 - h.x < 1.0f) k -= 1;
  }
  return R[std::clamp(k, 0, 4)];
}

}  // namespace

// ================================================================ fences and gates
// One tile each, 16 x FENCE_H, bottom on the tile's bottom edge. The fence line runs through the tile's ground row
// FG (east-west) and its columns 7..8 (north-south); the post stands where they cross. A run north-south is painted
// only over its own tile's ground (y 0..15), so the tile to the north (drawn before) and the one to the south (after)
// meet it end to end: no overlap, no gap. Outline pixels that would land on a neighbour's continuation are cleared.
namespace {

constexpr int FR = 12;                 // the rise above the tile (the tallest post)
constexpr int FENCE_H = 16 + FR;
constexpr int FG = 11;                 // the fence line's ground row in the tile

struct FencePainter {
  Canvas& c;
  const YardLook& Y;
  FenceKind kind;
  uint8_t joins;
  uint32_t var;
  Ramp W;            // the fence's wood (white pales for the imperial pickets)
  FencePainter(Canvas& c_, const YardLook& y, uint8_t j, uint32_t v) : c(c_), Y(y), kind(y.fence), joins(j), var(v) {
    W = kind == kFPicket && picketWhite(Y) ? kYWhite : Y.wood;
    if (var & 2) W = tintRamp(W, rgba(150, 140, 120), 0.12f);   // a weathered run
  }
  // a world point (tile x, ground y, height z)
  void P(int x, int y, int z, uint32_t col) { c.set(x, y + FR - z, col); }
  bool N() const { return joins & 1; }
  bool E() const { return joins & 2; }
  bool S() const { return joins & 4; }
  bool Wj() const { return joins & 8; }

  // ---------------------------------------------------------------- rails (and the rope's two strands)
  void railEW(int x0, int x1, int z) {   // a split rail: lit top, its face, the shaded underside
    for (int x = x0; x <= x1; x++) {
      const uint32_t h = hash3(x, z, 311u + (var & 1));
      P(x, FG - 1, z, W[(h % 9) == 0 ? 3 : 4]);
      P(x, FG, z, W[3]);
      P(x, FG, z - 1, W[(h % 7) == 0 ? 1 : 2]);
    }
  }
  void railNS(int y0, int y1, int z) {   // nailed to the posts' east side: its lit top, its body, its shaded east edge
    // (M7 fix) three px across (a split rail as thick as the E-W run's): at two px the north-south runs read as a
    // dotted line of posts with no rails between them
    for (int y = y0; y <= y1; y++) {
      const uint32_t h = hash3(y + 32, z, 313u + (var & 1));
      P(8, y, z, W[(h % 9) == 0 ? 3 : 4]); P(9, y, z, W[((y + 16) % 5) == 2 ? 2 : 3]); P(10, y, z, W[1]);
      P(10, y, z - 1, W[0]);   // its underside in shade
    }
  }
  void post(int x, int y, int top, const Ramp& R, bool cap = true) {   // 2 px square post: its top, its face
    for (int z = 0; z < top; z++) { P(x, y, z, R[3]); P(x + 1, y, z, R[1]); }
    P(x, y, 0, R[2]); P(x + 1, y, 0, R[0]);
    P(x, y, top, R[cap ? 3 : 2]); P(x + 1, y, top, R[2]);
    P(x, y - 1, top, R[4]); P(x + 1, y - 1, top, R[3]);
    if ((var & 1) && top > 6) P(x, y, top - 3, R[2]);   // a knot
  }

  // ---------------------------------------------------------------- per kind: the east-west piece over x0..x1
  void runEW(int x0, int x1) {
    switch (kind) {
      case kFRail: railEW(x0, x1, 8); railEW(x0, x1, 4); break;
      case kFRope: {   // two ropes sagging between the posts (the curve runs on into the next tile)
        for (int r = 0; r < 2; r++)
          for (int x = x0; x <= x1; x++) {
            const float t = (((x - 8) % 16) + 16) % 16 / 16.0f;
            const int z = (r ? 4 : 8) - (int)std::lround(std::sin(t * PI) * 1.6f);
            P(x, FG, z, kCloth[(x & 1) ? 3 : 2]);
            P(x, FG, z - 1, kCloth[1]);
          }
        break;
      }
      case kFPicket: {   // two stringers behind, pales every 3 px with pointed tops
        for (int x = x0; x <= x1; x++) { P(x, FG - 1, 7, W[1]); P(x, FG - 1, 3, W[1]); P(x, FG - 1, 6, W[0]); P(x, FG - 1, 2, W[0]); }
        for (int x = x0; x <= x1; x++) {
          const int ph = ((x % 3) + 3) % 3;
          if (ph == 2) continue;
          const int top = 9 + ((x / 3 + (int)(var & 1)) % 2 == 0 ? 0 : 0);
          for (int z = 0; z <= top; z++) P(x, FG, z, W[ph == 0 ? 3 : 2]);
          if (ph == 0) P(x, FG, top + 1, W[4]);
          else P(x, FG, top, W[3]);
          P(x, FG, 0, W[ph == 0 ? 2 : 1]);
        }
        break;
      }
      case kFWattle: {   // stakes every 4 px, withies woven between them; a ragged top
        for (int x = x0; x <= x1; x++) {
          const int ph = ((x % 4) + 4) % 4;
          for (int z = 0; z <= 9; z++) {
            if (ph == 1) { P(x, FG, z, W[z == 9 ? 4 : 2]); continue; }
            const int band = z / 2;
            if (z == 9 && ph == 3) continue;
            // each withy snakes over one stake and under the next: bright where it bulges toward the camera
            const float bulge = std::sin(((((x + band * 4) % 8) + 8) % 8) / 8.0f * TAU);
            const int k = 2 + (int)std::lround(bulge * 1.2f) + ((z & 1) ? 1 : -1) * (bulge > -0.3f ? 1 : 0);
            P(x, FG, z, W[std::clamp(k, 0, 4)]);
          }
          if (ph == 1) P(x, FG, 10, W[3]);
          P(x, FG - 1, 9, W[ph == 1 ? 4 : 3]);   // the top edge seen from above
        }
        break;
      }
      case kFBamboo: {   // canes side by side, nodes ringed, two lashed cross-bars
        for (int x = x0; x <= x1; x++) {
          const int ph = ((x % 3) + 3) % 3;
          if (ph == 2) { P(x, FG - 1, 3, W[0]); P(x, FG - 1, 7, W[0]); continue; }
          const int top = 10 + (int)(hash3(x / 3, 1, 47u) % 2);
          for (int z = 0; z <= top; z++) P(x, FG, z, (z % 5 == 2) ? kYBamboo[1] : kYBamboo[ph == 0 ? 3 : 2]);
          P(x, FG, top + 1, kYBamboo[4]);
        }
        for (int x = x0; x <= x1; x++) { P(x, FG + 1, 3, kWood[1]); P(x, FG + 1, 8, kWood[1]); P(x, FG + 1, 4, kWood[2]); P(x, FG + 1, 9, kWood[2]); }
        break;
      }
      case kFDyke: {   // a dry-stone dyke (or a mud wall): coping on top, coursed stones on the face
        const Ramp& S = Y.stone;
        const bool mud = Y.cul == 4 || Y.cul == 9;
        for (int x = x0; x <= x1; x++) {
          for (int y = FG - 2; y <= FG + 1; y++) {   // the top, seen from above
            const uint32_t h = hash3((x + 32) / 3, y, 41u);
            int k = y == FG - 2 ? 4 : 3;
            if (!mud && ((x + 32) % 3) == 0 && y < FG + 1) k = 2;
            if (!mud && h % 5 == 0) k = 2;
            P(x, y, 8, S[k]);
          }
          for (int z = 7; z >= 0; z--) {   // the face toward the camera
            int k = 2;
            if (mud) { k = z >= 6 ? 3 : (z <= 1 ? 1 : 2); if (hash3(x, z, 43u) % 11 == 0) k = 1; }
            else {
              const int row = (7 - z) / 2, bx = x + 32 + (row & 1) * 2;
              const bool joint = ((7 - z) % 2) == 1 || bx % 4 == 0;
              k = joint ? 1 : ((hash3(bx / 4, row, 43u) % 3 == 0) ? 3 : 2);
              if (z == 0) k = std::min(k, 1);
            }
            P(x, FG + 1, z, S[k]);
          }
          if ((var & 1) && !mud && hash3(x, 9, 77u) % 6 == 0) P(x, FG - 1, 8, kMoss[3]);
        }
        break;
      }
      case kFHedge: {   // a clipped hedge: its lit top, its leafy face
        for (int x = x0; x <= x1; x++) {
          for (int y = FG - 3; y <= FG + 1; y++) {
            const uint32_t h = hash3(x + 32, y, 49u);
            P(x, y, 9, kLeaf[y == FG - 3 ? (h % 3 ? 4 : 3) : (h % 4 == 0 ? 2 : 3)]);
          }
          for (int z = 8; z >= 0; z--) {
            const uint32_t h = hash3(x + 32, z, 51u);
            P(x, FG + 1, z, kLeaf[(h % 5 == 0) ? 1 : (z < 2 ? 1 : (z > 6 ? 3 : 2))]);
          }
          if ((var & 1) && hash3(x, 3, 53u) % 9 == 0) P(x, FG + 1, 5, rgba(240, 220, 230));   // a flower
        }
        break;
      }
    }
  }
  // ---------------------------------------------------------------- per kind: the north-south piece over ground y0..y1
  void runNS(int y0, int y1) {
    switch (kind) {
      case kFRail: railNS(y0, y1, 9); railNS(y0, y1, 5); break;
      case kFRope:
        for (int r = 0; r < 2; r++)
          for (int y = y0; y <= y1; y++) {
            const float t = (((y - FG) % 16) + 16) % 16 / 16.0f;
            const int z = (r ? 4 : 8) - (int)std::lround(std::sin(t * PI) * 1.6f);
            P(9, y, z, kCloth[(y & 1) ? 3 : 2]);
          }
        break;
      case kFPicket:   // the pales seen end on: their tops every 3 px down a lit band
        for (int y = y0; y <= y1; y++) {
          const int ph = ((y % 3) + 3) % 3;
          P(7, y, 9, W[ph == 0 ? 4 : 3]); P(8, y, 9, W[ph == 0 ? 3 : 2]);
          if (ph == 0) { P(7, y, 10, W[4]); }
          P(7, y, 8, W[3]); P(8, y, 8, W[2]); P(9, y, 8, W[0]);
        }
        break;
      case kFWattle:
        for (int y = y0; y <= y1; y++)
          for (int x = 6; x <= 8; x++) {
            const bool over = (((y / 2) + x) & 1) != 0;
            P(x, y, 9, W[x == 6 ? (over ? 4 : 3) : (over ? 3 : 1)]);
          }
        for (int y = y0; y <= y1; y++) P(9, y, 9, W[0]);
        break;
      case kFBamboo:
        // (integration) the canes seen end on along the run: a round, lit-from-the-left band as thick as the picket's,
        // the cane tops ringed every 3 px, a node every 5, the lashing showing as dark ties, the east side in shade
        for (int y = y0; y <= y1; y++) {
          const int ph = ((y % 3) + 3) % 3;
          const bool node = ((y % 5) + 5) % 5 == 2;
          P(6, y, 10, node ? kYBamboo[2] : kYBamboo[3]);
          P(7, y, 10, node ? kYBamboo[2] : kYBamboo[ph == 0 ? 4 : 3]);
          P(8, y, 10, node ? kYBamboo[1] : kYBamboo[2]);
          P(9, y, 10, kYBamboo[1]);
          if (ph == 0) P(7, y, 11, kYBamboo[4]);   // a cane's cut top standing proud
          P(10, y, 9, kYBamboo[0]);                 // the east side of the canes
          if (((y % 8) + 8) % 8 == 4) { P(6, y, 10, kWood[1]); P(7, y, 10, kWood[2]); P(8, y, 10, kWood[1]); P(9, y, 10, kWood[0]); }
        }
        break;
      case kFDyke: {
        const Ramp& S = Y.stone;
        const bool mud = Y.cul == 4 || Y.cul == 9;
        for (int y = y0; y <= y1; y++)
          for (int x = 6; x <= 9; x++) {
            int k = x == 6 ? 4 : (x == 9 ? 2 : 3);
            if (!mud && (((y + 32) % 3) == 0 || hash3(x, (y + 32) / 3, 45u) % 5 == 0)) k = std::max(1, k - 1);
            P(x, y, 8, S[k]);
          }
        for (int y = y0; y <= y1; y++) P(10, y, 8, S[1]);   // the east face just showing: the wall's thickness
        break;
      }
      case kFHedge:
        for (int y = y0; y <= y1; y++) P(11, y, 9, kLeaf[1]);
        for (int y = y0; y <= y1; y++)
          for (int x = 5; x <= 10; x++) {
            const uint32_t h = hash3(x, y + 32, 55u);
            P(x, y, 9, kLeaf[x == 5 ? 4 : (x == 10 ? 2 : (h % 4 == 0 ? 2 : 3))]);
          }
        break;
    }
  }
  // the end face of a north-south dyke / hedge where the run stops toward the camera
  void endFaceS(int y) {
    if (kind == kFDyke) {
      const Ramp& S = Y.stone;
      for (int z = 7; z >= 0; z--) for (int x = 6; x <= 9; x++) P(x, y, z, S[x == 6 ? 3 : ((z % 2) ? 1 : 2)]);
    } else if (kind == kFHedge) {
      for (int z = 8; z >= 0; z--) for (int x = 5; x <= 10; x++) P(x, y, z, kLeaf[x == 5 ? 3 : (z < 2 ? 1 : 2)]);
    }
  }
  bool solidWall() const { return kind == kFDyke || kind == kFHedge; }
  // the post (or a wall's pier) where the runs meet
  void centre() {
    if (solidWall()) {
      // the top square of the junction, and the face toward the camera when nothing continues south
      const int xa = kind == kFHedge ? 5 : 6, xb = kind == kFHedge ? 10 : 9;
      const int z = kind == kFHedge ? 9 : 8;
      const int ya = FG - (kind == kFHedge ? 3 : 2), yb = FG + 1;
      for (int y = ya; y <= yb; y++) for (int x = xa; x <= xb; x++) P(x, y, z, kind == kFHedge ? kLeaf[x == xa ? 4 : 3] : Y.stone[x == xa ? 4 : 3]);
      if (!S()) endFaceS(yb);
      return;
    }
    const Ramp& R = kind == kFPicket && picketWhite(Y) ? kYWhite : (kind == kFBamboo ? kYBamboo : Y.wood);
    post(7, FG, kind == kFRope ? 10 : 11, R);
  }

  void paint() {
    if (solidWall()) {
      if (N()) runNS(0, FG - 1);
      if (Wj()) runEW(0, 7);
      if (E()) runEW(8, 15);
      if (!(joins & 10)) {}   // (no east-west run: the pier alone)
      centre();
      if (S()) runNS(FG + 2, 15);
      return;
    }
    if (N()) runNS(0, FG - 1);
    if (Wj()) runEW(0, 7);
    if (E()) runEW(8, 15);
    if (!joins) { runEW(4, 6); runEW(9, 11); }   // a lone post: two short stubs, the start of a run
    centre();
    if (S()) runNS(FG + 1, 15);
  }
};

// a gate: two gateposts and a braced leaf between them, across the run (turned: in a north-south run)
void paintGate(Canvas& c, const YardLook& Y, uint8_t joins, bool ns, uint32_t var) {
  FencePainter F(c, Y, joins, var);
  const bool stone = F.solidWall();
  const Ramp& Pst = stone ? (F.kind == kFHedge ? kLeafDark : Y.stone) : (F.kind == kFPicket && picketWhite(Y) ? kYWhite : Y.wood);
  const Ramp& L = F.kind == kFPicket && picketWhite(Y) ? kYWhite : (Y.cul == 7 ? kYLacquer : (F.kind == kFBamboo ? kYBamboo : kWood));
  if (!ns) {
    // east-west: the posts at the tile's sides, the leaf between
    auto gatePost = [&](int x) {
      if (stone) {   // a pier with a cap stone
        for (int z = 0; z <= 11; z++) for (int xx = x; xx <= x + 2; xx++) F.P(xx, FG, z, Pst[xx == x ? 3 : (xx == x + 2 ? 1 : 2)]);
        for (int y = FG - 2; y <= FG; y++) for (int xx = x; xx <= x + 2; xx++) F.P(xx, y, 12, Pst[xx == x ? 4 : 3]);
      } else F.post(x, FG, 12, Pst);
    };
    if (F.N()) F.runNS(0, FG - 1);
    gatePost(0);
    const int lx0 = stone ? 3 : 2, lx1 = stone ? 12 : 13;
    // the leaf: frame rails top and bottom, stiles, the brace, the infill
    for (int x = lx0; x <= lx1; x++) {
      F.P(x, FG - 1, 10, L[4]); F.P(x, FG, 9, L[2]); F.P(x, FG, 8, L[1]);   // top rail
      F.P(x, FG - 1, 3, L[3]); F.P(x, FG, 2, L[2]); F.P(x, FG, 1, L[1]);    // bottom rail
      if (F.kind == kFPicket || F.kind == kFBamboo) { if (((x - lx0) % 3) != 2) for (int z = 3; z <= 8; z++) F.P(x, FG, z, L[((x - lx0) % 3) == 0 ? 3 : 2]); }
      else F.P(x, FG, 5, L[(x & 1) ? 2 : 1]);   // a middle bar
    }
    for (int z = 1; z <= 9; z++) { F.P(lx0, FG, z, L[3]); F.P(lx1, FG, z, L[1]); }
    for (int i = 0; i <= lx1 - lx0 - 2; i++) {   // the brace, bottom hinge side up to the latch side
      const int x = lx0 + 1 + i, z = 2 + (i * 7) / std::max(1, lx1 - lx0 - 2);
      F.P(x, FG, z, L[3]); F.P(x, FG, z + 1, L[2]);
    }
    F.P(lx0, FG, 8, kIron[3]); F.P(lx0, FG, 3, kIron[3]);   // hinges
    F.P(lx1 - 1, FG, 6, kIron[4]); F.P(lx1, FG, 6, kIron[2]);   // the latch
    gatePost(stone ? 13 : 14);
    if (F.S()) F.runNS(FG + 1, 15);
    (void)var;
    return;
  }
  // north-south: posts at the tile's north and south, the leaf's top rail and frame seen from above between them
  if (F.Wj()) F.runEW(0, 6);
  if (F.E()) F.runEW(9, 15);
  if (stone) for (int y = 0; y <= 1; y++) for (int x = 6; x <= 9; x++) F.P(x, y, 8, Y.stone[x == 6 ? 4 : 3]);
  else F.runNS(0, 1);
  auto nsPost = [&](int y) {
    if (stone) { for (int z = 0; z <= 11; z++) for (int x = 6; x <= 9; x++) F.P(x, y, z, Pst[x == 6 ? 3 : (x == 9 ? 1 : 2)]); for (int x = 6; x <= 9; x++) { F.P(x, y - 1, 12, Pst[x == 6 ? 4 : 3]); F.P(x, y - 2, 12, Pst[3]); } }
    else F.post(7, y, 12, Pst);
  };
  nsPost(2);
  for (int y = 3; y <= 13; y++) {   // the leaf from above: its top rail, the infill's tops, the bottom rail's edge
    F.P(7, y, 10, L[4]); F.P(8, y, 10, L[3]);
    F.P(8, y, 9, L[1]);
  }
  F.P(7, 4, 10, kIron[3]); F.P(7, 12, 10, kIron[4]); F.P(8, 12, 10, kIron[2]);   // hinge and latch
  // the leaf's south stile face (the end the camera sees)
  for (int z = 1; z <= 9; z++) { F.P(7, 13, z, L[3]); F.P(8, 13, z, L[1]); }
  nsPost(15);
  (void)var;
}

Canvas fenceSprite(const FarmObjLook& l) {
  const YardLook Y = yardLook(l);
  Canvas c(16, FENCE_H);
  const bool gate = l.kind == FarmObj::Gate;
  const bool ns = gate && (l.turned || ((l.joins & 5) && !(l.joins & 10)));
  if (gate) paintGate(c, Y, l.joins, ns, l.variant);
  else { FencePainter F(c, Y, l.joins, l.variant); F.paint(); }
  // the outline; then clear what it put where a joined run continues into the neighbour's tile (above the north
  // run's top end and below the south run's bottom end: the neighbour's own pixels are there)
  const Canvas before = c;
  outline(c, 0.85f);
  const bool nRun = gate ? (ns || (l.joins & 1)) : (l.joins & 1);
  const bool sRun = gate ? (ns || (l.joins & 4)) : (l.joins & 4);
  for (int x = 4; x <= 11; x++) {
    int top = -1, bot = -1;
    for (int y = 0; y < c.h; y++) if (chA(before.get(x, y))) { if (top < 0) top = y; bot = y; }
    if (top < 0) continue;
    if (nRun) for (int y = 0; y < top; y++) if (!chA(before.get(x, y))) c.set(x, y, 0);
    if (sRun) for (int y = bot + 1; y < c.h; y++) if (!chA(before.get(x, y))) c.set(x, y, 0);
  }
  // east and west runs reach the canvas edges: no outline is drawn there
  return c;
}

}  // namespace

// ================================================================ yard objects
namespace {

struct ObjSize { int w, h; };
ObjSize objSize(FarmObj k, bool turned) {
  switch (k) {
    case FarmObj::Fence: case FarmObj::Gate: return {16, FENCE_H};
    case FarmObj::Well: return {36, 54};
    case FarmObj::Woodpile: return turned ? ObjSize{20, 42} : ObjSize{34, 26};
    case FarmObj::Beehive: return {16, 30};
    case FarmObj::Scarecrow: return {22, 38};
    case FarmObj::Coop: return {36, 48};
    case FarmObj::Pen: return {50, 50};
    case FarmObj::Stable: return {54, 62};
    case FarmObj::Trough: return turned ? ObjSize{18, 40} : ObjSize{32, 24};
    case FarmObj::Workbench: return turned ? ObjSize{20, 46} : ObjSize{34, 30};
    case FarmObj::Forge: return {34, 64};
    case FarmObj::FlowerBed: return {16, 22};
    case FarmObj::Sapling: return {26, 50};
    case FarmObj::Bench: return turned ? ObjSize{18, 42} : ObjSize{32, 28};
    case FarmObj::Lantern: return {14, 38};
    case FarmObj::Statue: return {22, 46};
    case FarmObj::Banner: return {22, 46};
    case FarmObj::Campfire: return {18, 24};
    case FarmObj::Doghouse: return {22, 32};
    case FarmObj::HayRack: return turned ? ObjSize{20, 50} : ObjSize{34, 34};
    case FarmObj::ShippingCrate: return {16, 26};
    default: return {16, 24};
  }
}
bool turnable(FarmObj k) {
  return k == FarmObj::Woodpile || k == FarmObj::Trough || k == FarmObj::Workbench || k == FarmObj::Bench || k == FarmObj::HayRack;
}

// the object's footprint inside its canvas (world units = canvas px; the ground's front edge is the canvas bottom)
struct Foot {
  float x0, x1, y0, y1;   // the footprint's ground rectangle
  float cx, cy;           // its centre
};
Foot footOf(const Canvas& c, int fw, int fh) {
  Foot f;
  f.x0 = (c.w - fw * 16) * 0.5f; f.x1 = f.x0 + fw * 16;
  f.y1 = (float)c.h; f.y0 = f.y1 - fh * 16;
  f.cx = (f.x0 + f.x1) * 0.5f; f.cy = (f.y0 + f.y1) * 0.5f;
  return f;
}
// a world point to the canvas
inline void WP(Canvas& c, float x, float y, float z, uint32_t col) { c.set((int)std::floor(x), (int)std::floor(y - z), col); }

// a roof of the culture's shape over a footprint (ridge along x unless `alongY`), eave at z0, ridge rise `rise`
void addRoof(Scene& S, const YardLook& Y, float x0, float x1, float y0, float y1, float z0, float rise, bool alongY, uint32_t seed, bool small = false) {
  PartShape sh = alongY ? kGableY : kGableX;
  switch (Y.roofShape) {
    case RoofShape::Hip: case RoofShape::Mansard: case RoofShape::Pagoda: sh = small ? sh : kHip; break;
    case RoofShape::Conical: case RoofShape::Tent: case RoofShape::Spire: sh = kPyramid; break;
    case RoofShape::Dome: case RoofShape::Onion: sh = kPyramid; rise *= 0.8f; break;
    case RoofShape::FlatParapet: case RoofShape::Stepped: sh = kBox; rise = 1.5f; break;
    default: break;
  }
  if (Y.roofMat == RoofMat::Felt && small) sh = kPyramid;
  Part& p = S.add(sh, x0, x1, y0, y1, z0, z0 + rise, Y.roof, roofTex(Y.roofMat));
  p.thick = sh == kBox ? 2.0f : 1.6f;
  p.seed = seed;
}

// a dark opening (door, hatch) on a front face: the inside in shade, lit at its top-left edge
void opening(Canvas& c, int x0, int y0, int x1, int y1, const Ramp& frame, bool arched = false) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      if (arched && y == y0 && (x == x0 || x == x1)) continue;
      c.set(x, y, (y == y0 || x == x0) ? rgba(34, 24, 34) : rgba(52, 38, 46));
    }
  for (int y = y0; y <= y1; y++) { c.set(x0 - 1, y, frame[3]); c.set(x1 + 1, y, frame[1]); }
  for (int x = x0 - 1; x <= x1 + 1; x++) if (!(arched && (x == x0 - 1 || x == x1 + 1))) c.set(x, y0 - 1, frame[2]);
}

// a little hen standing (for the coop's run): 5 x 5 px, facing left or right
void tinyHen(Canvas& c, int x, int y, bool right, uint32_t v) {
  const Ramp H = (v % 3) == 0 ? ramp(rgba(240, 236, 224)) : ((v % 3) == 1 ? ramp(rgba(170, 100, 52)) : ramp(rgba(70, 64, 74)));
  const int d = right ? 1 : -1;
  c.set(x, y, H[2]); c.set(x + d, y, H[3]); c.set(x - d, y, H[1]);
  c.set(x, y - 1, H[3]); c.set(x - d, y - 1, H[2]);
  c.set(x + d, y - 1, H[4]); c.set(x + d, y - 2, H[3]); c.set(x + 2 * d, y - 2, rgba(236, 188, 72));
  c.set(x + d, y - 3, rgba(212, 44, 48));
  c.set(x - 2 * d, y - 2, H[1]);
  c.set(x, y + 1, rgba(226, 176, 70));
}

// ---------------------------------------------------------------- the well
void paintWell(Canvas& c, const YardLook& Y, const FarmObjLook& l) {
  const Foot F = footOf(c, 2, 2);
  Scene S;
  const float cx = F.cx, cy = F.cy + 1;
  const bool sweep = Y.cul == 4 || Y.cul == 9;    // the desert's open well with a sweep pole (shadoof)
  const Ramp& St = Y.stone;
  // the curb: a ring of stone
  Part& curb = S.add(kRing, cx - 10, cx + 10, cy - 8, cy + 8, 0, 8, St, kTexStone);
  curb.seed = 5;
  Part& water = S.add(kCyl, cx - 7, cx + 7, cy - 5.5f, cy + 5.5f, 0, 6.5f, kWater, kTexWater);
  water.paint = [](const Hit& h, uint32_t& col) { if (!h.top) return false; col = h.y < 0 ? kWater[1] : ((hash3(h.px, h.py, 3) % 9) == 0 ? kWater[3] : kWater[1]); return true; };
  (void)water;
  // a coping ring lit on top
  curb.paint = [cx, cy](const Hit& h, uint32_t& col) {
    if (!h.top) return false;
    const float dx = (h.x - cx) / 10.0f, dy = (h.y - cy) / 8.0f;
    const float d = dx * dx + dy * dy;
    (void)d;
    return false;
  };
  if (!sweep) {
    // two posts, the winch roller, a roof over it
    const Ramp& Wd = Y.wood;
    S.add(kBox, cx - 12, cx - 9.5f, cy - 1, cy + 1.5f, 0, 24, Wd, kTexPlanksH);
    S.add(kBox, cx + 9.5f, cx + 12, cy - 1, cy + 1.5f, 0, 24, Wd, kTexPlanksH);
    Part& roll = S.add(kBox, cx - 9.5f, cx + 9.5f, cy - 1, cy + 1, 15, 17, Wd, kTexPlanksH);
    roll.bias = -1;
    addRoof(S, Y, cx - 15, cx + 15, cy - 7, cy + 7, 23, 9, false, l.variant, true);
  } else {
    const Ramp& Wd = Y.wood;
    S.add(kBox, cx + 9, cx + 11.5f, cy - 1, cy + 1.5f, 0, 18, Wd, kTexPlanksH);
  }
  S.render(c);
  if (!sweep) {
    // the rope down into the well, the bucket on the curb, the winch handle
    const int rx = (int)cx;
    for (int y = (int)(cy - 16); y <= (int)(cy - 9); y++) c.set(rx, y, kCloth[1]);
    const int bx = (int)(cx + 4), by = (int)(cy + 5 - 8);
    c.set(bx, by, kWood[3]); c.set(bx + 1, by, kWood[2]); c.set(bx + 2, by, kWood[1]);
    c.set(bx, by + 1, kWood[2]); c.set(bx + 1, by + 1, kWood[2]); c.set(bx + 2, by + 1, kWood[0]);
    c.set(bx, by - 1, kIron[2]); c.set(bx + 2, by - 1, kIron[1]); c.set(bx + 1, by - 2, kIron[3]);
    const int hx = (int)(cx + 13), hy = (int)(cy - 16);
    c.set(hx, hy, kIron[3]); c.set(hx + 1, hy, kIron[2]); c.set(hx + 1, hy + 1, kIron[2]); c.set(hx + 1, hy + 2, kWood[2]);
  } else {
    // the sweep: a long pole pivoting on the post, its bucket over the water, a stone counterweight
    line(c, (int)cx - 8, (int)(cy - 20), (int)cx + 15, (int)(cy - 10), Y.wood[3]);
    line(c, (int)cx - 8, (int)(cy - 19), (int)cx + 15, (int)(cy - 9), Y.wood[1]);
    c.disc((int)cx + 15, (int)(cy - 9), 2, St[2]); c.set((int)cx + 14, (int)(cy - 10), St[4]);
    for (int y = (int)(cy - 19); y <= (int)(cy - 12); y++) c.set((int)cx - 8, y, kCloth[1]);
    c.set((int)cx - 9, (int)(cy - 11), kWood[3]); c.set((int)cx - 8, (int)(cy - 11), kWood[2]); c.set((int)cx - 7, (int)(cy - 11), kWood[1]);
  }
}

// ---------------------------------------------------------------- the woodpile (split logs under a little cover)
void paintWoodpile(Canvas& c, const YardLook& Y, const FarmObjLook& l) {
  const bool t = l.turned;
  const Foot F = footOf(c, t ? 1 : 2, t ? 2 : 1);
  Scene S;
  const float x0 = F.x0 + 1, x1 = F.x1 - 1, y0 = F.y0 + 3, y1 = F.y1 - 2;
  Part& stack = S.add(kBox, x0, x1, y0 + 1, y1, 0, t ? 11 : 10, kBark, kTexLogEnds);
  stack.paint = [t](const Hit& h, uint32_t& col) {
    if (h.top) {   // the bark of the top logs running along the pile
      const float a = t ? h.x : h.y;
      const int r = (((int)std::floor(a) % 4) + 4) % 4;
      col = kBark[r == 0 ? 1 : (r == 1 ? 4 : 3)];
      return true;
    }
    return false;
  };
  // end posts holding the stack
  const Ramp& Wd = Y.wood;
  if (!t) { S.add(kBox, x0 - 1, x0 + 1, y1 - 2, y1, 0, 12, Wd, kTexPlanksV); S.add(kBox, x1 - 1, x1 + 1, y1 - 2, y1, 0, 12, Wd, kTexPlanksV); }
  else { S.add(kBox, x0, x1, y1 - 1.5f, y1, 0, 13, Wd, kTexPlanksV).bias = 0; }
  S.render(c);
  // an axe in the chopping block beside it
  if (!t) {
    const int bx = (int)x1 - 3, by = (int)y1 + 1;
    (void)bx; (void)by;
  }
}

// ---------------------------------------------------------------- the beehive: a straw skep, a box hive or clay pipes
void paintBeehive(Canvas& c, const YardLook& Y, const FarmObjLook& l) {
  const Foot F = footOf(c, 1, 1);
  Scene S;
  const float cx = F.cx, cy = F.cy + 1;
  const Ramp& Wd = Y.wood;
  if (Y.hive == kHiveSkep) {
    // a bench of two stones and a plank, the coiled straw dome, its entrance
    S.add(kBox, cx - 6, cx + 6, cy - 3, cy + 3, 4, 5.5f, Wd, kTexPlanksH);
    S.add(kBox, cx - 5, cx - 3, cy - 2, cy + 2, 0, 4, Y.stone, kTexStone);
    S.add(kBox, cx + 3, cx + 5, cy - 2, cy + 2, 0, 4, Y.stone, kTexStone);
    Part& d = S.add(kDome, cx - 5.5f, cx + 5.5f, cy - 5, cy + 5, 5.5f, 18, kThatch, kTexPlain);
    d.paint = [](const Hit& h, uint32_t& col) {
      int k = lightIndex(h.light, h.px, h.py, 0.1f) + 1;
      const int coil = (((int)std::floor(h.z * 0.5f)) % 3 + 3) % 3;   // the coiled straw rope, a groove every 2 px
      if (coil == 0 && (((int)std::floor(h.z)) & 1)) k -= 2;
      col = kThatch[std::clamp(k, 0, 4)];
      return true;
    };
    S.render(c);
    // the entrance at the foot of the dome, bees
    const int ex = (int)cx, ey = (int)(cy + 4 - 7);
    c.set(ex, ey, rgba(40, 28, 36)); c.set(ex + 1, ey, rgba(40, 28, 36)); c.set(ex, ey - 1, kThatch[0]);
  } else if (Y.hive == kHiveBox) {
    // stacked painted boxes on legs, a little gabled lid
    const Ramp Bx = Y.cul == 7 ? kYLacquer : (Y.cul == 3 || Y.cul == 11 ? kYWhite : ramp(rgba(214, 196, 150)));
    S.add(kBox, cx - 5, cx - 3.5f, cy + 2, cy + 3.5f, 0, 4, Wd);
    S.add(kBox, cx + 3.5f, cx + 5, cy + 2, cy + 3.5f, 0, 4, Wd);
    S.add(kBox, cx - 5, cx - 3.5f, cy - 4, cy - 2.5f, 0, 4, Wd);
    S.add(kBox, cx + 3.5f, cx + 5, cy - 4, cy - 2.5f, 0, 4, Wd);
    Part& b1 = S.add(kBox, cx - 5.5f, cx + 5.5f, cy - 4.5f, cy + 3.5f, 4, 9, Bx, kTexPlanksH);
    Part& b2 = S.add(kBox, cx - 5.5f, cx + 5.5f, cy - 4.5f, cy + 3.5f, 9, 14, Bx, kTexPlanksH);
    b2.bias = (l.variant & 1) ? 0 : -1;
    (void)b1;
    Part& lid = S.add(kGableX, cx - 6.5f, cx + 6.5f, cy - 5.5f, cy + 4.5f, 14, 17, Y.cul == 7 ? Y.roof : kWood, Y.cul == 7 ? roofTex(Y.roofMat) : kTexShingle);
    lid.thick = 1.2f;
    S.render(c);
    const int ey = (int)(cy + 3.5f - 5);
    for (int x = (int)cx - 2; x <= (int)cx + 1; x++) c.set(x, ey, rgba(40, 28, 36));
  } else {
    // clay pipe hives stacked on a mud bench, their round mouths toward the camera
    const Ramp Cl = ramp(rgba(196, 128, 84));
    S.add(kBox, cx - 7, cx + 7, cy - 4, cy + 4, 0, 4, Y.stone, kTexAdobe);
    for (int i = 0; i < 3; i++) {
      const float px = i < 2 ? cx - 3.5f + i * 7.0f : cx, pz = i < 2 ? 4.0f : 10.0f;
      Part& p = S.add(kBox, px - 3, px + 3, cy - 4, cy + 4, pz, pz + 6, Cl, kTexPlain);
      p.paint = [px, pz](const Hit& h, uint32_t& col) {
        const float dx = h.x - px, dz = h.z - (pz + 3);
        const float d = dx * dx + dz * dz;
        if (!h.top && d < 3.0f) { col = d < 1.2f ? rgba(40, 26, 30) : kYClay[1]; return true; }
        const int k = h.top ? 3 : (dx < -1 ? 3 : (dx > 1.5f ? 1 : 2));
        col = ramp(rgba(196, 128, 84))[k];
        return true;
      };
    }
    S.render(c);
  }
  // a few bees about the hive
  const uint32_t bs = l.variant * 7u + 3u;
  for (int b = 0; b < 3; b++) {
    const int bx = 2 + (int)(hash3(b, 1, bs) % 12), by = (int)(cy - 20) + (int)(hash3(b, 2, bs) % 6);
    if (by >= 0) { c.set(bx, by, rgba(250, 210, 60)); c.set(bx + 1, by, rgba(40, 30, 30)); }
  }
}

// ---------------------------------------------------------------- the scarecrow
void paintScarecrow(Canvas& c, const YardLook& Y, const FarmObjLook& l) {
  const float cx = c.w * 0.5f, g = c.h - 5.0f;
  const Ramp& Wd = Y.wood;
  const Ramp Shirt = Y.accent ? ramp(Y.accent) : ((l.variant & 1) ? ramp(rgba(150, 60, 50)) : ramp(rgba(70, 100, 150)));
  // the post and the cross-arm
  for (int y = (int)g - 30; y <= (int)g; y++) { c.set((int)cx, y, Wd[3]); c.set((int)cx + 1, y, Wd[1]); }
  for (int x = (int)cx - 9; x <= (int)cx + 10; x++) { c.set(x, (int)g - 22, Wd[3]); c.set(x, (int)g - 21, Wd[1]); }
  // the shirt stuffed with straw: a shaded body, sleeves along the arm, straw tufts at cuffs and hem
  ball(c, cx + 0.5f, g - 18.0f, 5.0f, 6.0f, Shirt, 0.1f);
  for (int x = (int)cx - 8; x <= (int)cx + 9; x++) { c.set(x, (int)g - 23, Shirt[3]); c.set(x, (int)g - 22, Shirt[2]); c.set(x, (int)g - 21, Shirt[1]); }
  for (int s = -1; s <= 1; s += 2) {   // straw hands
    const int hx = (int)cx + (s < 0 ? -10 : 11);
    c.set(hx, (int)g - 23, kThatch[4]); c.set(hx, (int)g - 22, kThatch[3]); c.set(hx + s, (int)g - 22, kThatch[2]); c.set(hx, (int)g - 21, kThatch[2]); c.set(hx + s, (int)g - 20, kThatch[1]);
  }
  for (int x = (int)cx - 4; x <= (int)cx + 5; x += 2) { c.set(x, (int)g - 12, kThatch[3]); c.set(x, (int)g - 11, kThatch[2]); }
  // patches and a rope belt
  c.set((int)cx - 2, (int)g - 19, kCloth[3]); c.set((int)cx - 1, (int)g - 19, kCloth[3]); c.set((int)cx - 2, (int)g - 18, kCloth[2]);
  for (int x = (int)cx - 4; x <= (int)cx + 5; x++) c.set(x, (int)g - 14, kThatch[1]);
  // the sack head, the stitched face, a straw hat
  ball(c, cx + 0.5f, g - 27.5f, 3.4f, 3.2f, kCloth, 0.05f);
  c.set((int)cx - 1, (int)g - 28, kInk); c.set((int)cx + 2, (int)g - 28, kInk);
  for (int x = (int)cx - 1; x <= (int)cx + 2; x++) c.set(x, (int)g - 26, (x & 1) ? kCloth[0] : kCloth[1]);
  const Ramp& Ht = kThatch;
  for (int x = (int)cx - 5; x <= (int)cx + 6; x++) { c.set(x, (int)g - 30, Ht[x < cx ? 3 : 2]); c.set(x, (int)g - 29, Ht[1]); }
  for (int x = (int)cx - 2; x <= (int)cx + 3; x++) { c.set(x, (int)g - 32, Ht[4]); c.set(x, (int)g - 31, Ht[3]); }
  // a crow on the arm (some)
  if (l.variant & 2) {
    const int bx = (int)cx + 7, by = (int)g - 24;
    c.set(bx, by, rgba(40, 36, 48)); c.set(bx + 1, by, rgba(50, 46, 60)); c.set(bx, by - 1, rgba(50, 46, 60)); c.set(bx - 1, by - 1, rgba(70, 66, 80));
    c.set(bx + 1, by - 1, rgba(230, 180, 60));
  }
}

// ---------------------------------------------------------------- the chicken coop: a little house on legs, a ramp
void paintCoop(Canvas& c, const YardLook& Y, const FarmObjLook& l) {
  const Foot F = footOf(c, 2, 2);
  Scene S;
  const float x0 = F.x0 + 4, x1 = F.x1 - 4, y0 = F.y0 + 6, y1 = F.y1 - 9;
  const Ramp& Wd = Y.wood;
  const float lift = 4;
  // the legs
  for (int i = 0; i < 4; i++) {
    const float lx = (i & 1) ? x1 - 2 : x0, ly = (i & 2) ? y1 - 2 : y0;
    S.add(kBox, lx, lx + 2, ly, ly + 2, 0, lift, Y.woodDark, kTexPlain);
  }
  // the house: the culture's wall (boards by default)
  const int wt = (Y.wallMat == WallMat::Plank || Y.cul < 0) ? kTexPlanksV : wallTex(Y.wallMat);
  Part& body = S.add(kBox, x0, x1, y0, y1, lift, lift + 11, Y.cul < 0 ? Wd : Y.wall, wt);
  body.seed = 3;
  // the nest box on the east side, its own little lid
  Part& nest = S.add(kBox, x1, x1 + 4, y0 + 3, y1 - 3, lift + 2, lift + 8, Y.cul < 0 ? Wd : Y.wall, wt);
  nest.bias = -1;
  Part& lid = S.add(kShedS, x1 - 0.5f, x1 + 4.5f, y0 + 2, y1 - 2, lift + 8, lift + 9.5f, Y.roof, roofTex(Y.roofMat));
  lid.thick = 1;
  // the roof: a front gable (ridge running back), its eaves well out
  addRoof(S, Y, x0 - 2.5f, x1 + 2.5f, y0 - 2, y1 + 2.5f, lift + 11, 9, true, l.variant, true);
  // the ramp up to the pop-hole
  Part& ramp1 = S.add(kShedS, F.cx - 3, F.cx + 3, y1, y1 + 8, 0, lift + 1, Wd, kTexPlanksH);
  ramp1.thick = 1.0f;
  ramp1.paint = [](const Hit& h, uint32_t& col) { if (!h.top) return false; const int r = (((int)std::floor(h.y)) % 2 + 2) % 2; col = kWood[r ? 3 : 1]; return true; };
  S.render(c);
  // the pop-hole and the door, a window with a lath
  const int fy = (int)(y1 - lift);   // the front wall's foot on the canvas
  opening(c, (int)F.cx - 2, fy - 5, (int)F.cx + 1, fy - 1, Y.trim, true);
  opening(c, (int)x0 + 2, fy - 9, (int)x0 + 4, fy - 7, Y.trim);
  c.set((int)x0 + 3, fy - 8, Y.trim[2]);
  // hens by stage: on the ramp and in front
  const int n = std::min(3, (int)l.stage);
  for (int i = 0; i < n; i++) {
    const int hx = (int)F.cx + (i == 0 ? -9 : (i == 1 ? 8 : 0)), hy = (int)F.y1 - (i == 2 ? 5 : 3);
    tinyHen(c, hx, hy, i != 1, l.variant + (uint32_t)i);
  }
}

// ---------------------------------------------------------------- the pen: a fenced run with a lean-to shelter
void paintPen(Canvas& c, const YardLook& Y, const FarmObjLook& l) {
  const Foot F = footOf(c, 3, 2);
  // the trodden earth inside
  for (int y = (int)F.y0 + 3; y < (int)F.y1 - 2; y++)
    for (int x = (int)F.x0 + 2; x < (int)F.x1 - 2; x++) {
      const uint32_t h = hash3(x, y, 1611u);
      c.set(x, y, kEarth[(h % 7) == 0 ? 3 : ((h % 5) == 0 ? 1 : 2)]);
      if ((h % 37) == 0) c.set(x, y, kYHay[3]);
    }
  Scene S;
  // the shelter along the back: posts, a back wall, a lean-to roof
  const Ramp& Wd = Y.wood;
  const float sy0 = F.y0 + 1, sy1 = F.y0 + 12;
  Part& wall = S.add(kBox, F.x0 + 3, F.x1 - 3, sy0, sy0 + 2, 0, 13, Y.cul < 0 ? Wd : Y.wall, Y.cul < 0 ? kTexPlanksV : wallTex(Y.wallMat));
  wall.bias = -1;
  S.add(kBox, F.x0 + 3, F.x0 + 5, sy1 - 2, sy1, 0, 10, Wd, kTexPlanksV);
  S.add(kBox, F.x1 - 5, F.x1 - 3, sy1 - 2, sy1, 0, 10, Wd, kTexPlanksV);
  Part& r = S.add(kShedS, F.x0 + 1, F.x1 - 1, sy0 - 2, sy1 + 2, 9, 16, Y.roof, roofTex(Y.roofMat));
  r.thick = 1.6f; r.seed = l.variant;
  // the trough and a hay pile under it
  Part& tr = S.add(kBox, F.x0 + 8, F.x0 + 20, sy1 + 2, sy1 + 6, 0, 4, Wd, kTexPlanksH);
  tr.paint = [](const Hit& h, uint32_t& col) { (void)h; (void)col; return false; };
  Part& hay = S.add(kDome, F.x1 - 16, F.x1 - 6, sy0 + 3, sy1 - 1, 0, 6, kYHay, kTexHay);
  (void)hay;
  S.render(c);
  // the animals inside by stage (pigs, goats, sheep in turn), standing in the run before the front fence
  const int n = std::min(2, (int)l.stage);
  static const Critter kinds[3] = {Critter::Pig, Critter::Goat, Critter::Sheep};
  for (int i = 0; i < n; i++) {
    const Critter cr = kinds[(i + (int)l.variant) % 3];
    const int cw = critterCellW(cr), ch = critterCellH(cr);
    const Canvas sh = critterSheet(cr, l.variant + (uint32_t)i);
    const bool flipX = (i & 1) != 0;
    const int col = i == 1 ? 5 : 0;   // one idle, one grazing
    const int ax = (int)F.x0 + (i == 0 ? 6 : 24), ay = (int)F.y1 - 6 - ch - (i == 1 ? 2 : 0);
    for (int y = 0; y < ch; y++)
      for (int x = 0; x < cw; x++) {
        const uint32_t p = sh.get(col * cw + (flipX ? cw - 1 - x : x), 2 * ch + y);
        if (chA(p) > 128) c.set(ax + x, ay + y, p);
      }
  }
  // the fence round the run (the culture's rails; a wall culture rails its pens), a gate in the front
  YardLook FY = Y;
  if (FY.fence == kFDyke || FY.fence == kFHedge) FY.fence = kFRail;
  const int tiles = 3;
  for (int ty = 0; ty < 2; ty++)
    for (int tx = 0; tx < tiles; tx++) {
      const bool edgeW = tx == 0, edgeE = tx == tiles - 1, front = ty == 1;
      if (!(edgeW || edgeE || front)) continue;
      uint8_t j = 0;
      if (edgeW || edgeE) j |= ty == 0 ? 4 : 1;
      if (front) { if (!edgeW) j |= 8; if (!edgeE) j |= 2; }
      Canvas f(16, FENCE_H);
      if (front && tx == 1) paintGate(f, FY, j, false, l.variant);
      else { FencePainter P(f, FY, j, l.variant); P.paint(); }
      outline(f, 0.85f);
      const int ox = (int)F.x0 + tx * 16, oy = (int)F.y0 + ty * 16 + 16 - FENCE_H;
      for (int y = 0; y < f.h; y++)
        for (int x = 0; x < f.w; x++) if (chA(f.get(x, y))) c.set(ox + x, oy + y, f.get(x, y));
    }
}

// ---------------------------------------------------------------- the stable: a long barn, stall doors, horses
void paintStable(Canvas& c, const YardLook& Y, const FarmObjLook& l) {
  const Foot F = footOf(c, 3, 3);
  Scene S;
  const float x0 = F.x0 + 2, x1 = F.x1 - 2, y0 = F.y0 + 14, y1 = F.y1 - 4;
  const int wt = (Y.cul < 0) ? kTexPlanksV : wallTex(Y.wallMat);
  const Ramp& Wl = Y.cul < 0 ? kWood : Y.wall;
  if (Y.stoneBase || Y.cul == 1 || Y.cul == 3) S.add(kBox, x0 - 0.5f, x1 + 0.5f, y0, y1 + 0.5f, 0, 3, Y.stone, kTexStone);
  Part& body = S.add(kBox, x0, x1, y0, y1, 0, 15, Wl, wt);
  body.seed = 9;
  {
    YardLook RY = Y;
    if (RY.roofShape == RoofShape::Gable || RY.roofShape == RoofShape::Steep || RY.roofShape == RoofShape::Turf || RY.roofShape == RoofShape::Sweep) RY.roofShape = RoofShape::Hip;
    addRoof(S, RY, x0 - 3, x1 + 3, y0 - 4, y1 + 3.5f, 15, 12, false, l.variant);
  }
  S.render(c);
  // two stall half-doors (the upper leaf open), a window between; the horses look out by stage
  const int fy = (int)y1;
  const Ramp& Dr = Y.cul == 7 ? kYLacquer : (Y.cul < 0 ? kWoodDark : Y.trim);
  for (int s = 0; s < 2; s++) {
    const int dx0 = (int)x0 + 5 + s * 26, dx1 = dx0 + 9;
    // the lower leaf (boards with a brace), the dark stall above it
    opening(c, dx0, fy - 13, dx1, fy - 7, Dr);
    for (int y = fy - 6; y <= fy - 1; y++)
      for (int x = dx0; x <= dx1; x++) c.set(x, y, Dr[x == dx0 ? 3 : (x == dx1 ? 1 : ((x - dx0) % 3 == 0 ? 1 : 2))]);
    for (int i = 0; i <= dx1 - dx0; i++) c.set(dx0 + i, fy - 1 - (i * 5) / std::max(1, dx1 - dx0), Dr[3]);
    for (int x = dx0 - 1; x <= dx1 + 1; x++) c.set(x, fy - 7, Dr[4]);
    // a horse's head over the door
    if ((int)l.stage > s) {
      static const uint32_t coats[3] = {rgba(140, 90, 48), rgba(90, 60, 36), rgba(186, 184, 180)};
      const Ramp H = ramp(coats[(s + l.variant) % 3], 0.85f);
      const int hx = (dx0 + dx1) / 2;
      ball(c, hx, fy - 9.5f, 2.6f, 3.0f, H, 0.04f);
      ball(c, hx, fy - 6.5f, 2.0f, 1.8f, H, 0.04f, -1);
      c.set(hx - 2, fy - 13, H[3]); c.set(hx + 2, fy - 13, H[2]);
      c.set(hx - 1, fy - 10, kEye); c.set(hx + 1, fy - 10, kEye);
      c.set(hx - 1, fy - 6, H[0]); c.set(hx + 1, fy - 6, H[0]);
      if (s == 0) { c.set(hx, fy - 11, kWhite); c.set(hx, fy - 10, kWhite); }
      for (int y = fy - 12; y <= fy - 8; y++) c.set(hx + 3, y, ramp(rgba(50, 40, 44))[2]);
    }
  }
  // the hay-loft hatch under the eaves, a lantern hook
  opening(c, (int)F.cx - 2, fy - 16 + 2, (int)F.cx + 1, fy - 14 + 2, Dr);
  // a hay bale and a pitchfork by the wall when there are horses
  if (l.stage >= 1) {
    const int bx = (int)x1 - 3, by = fy + 1;
    for (int y = by - 4; y <= by; y++) for (int x = bx - 4; x <= bx + 1; x++) c.set(x, y, kYHay[y == by - 4 ? 4 : (x == bx + 1 ? 1 : ((x + y) % 3 ? 2 : 3))]);
    c.set(bx - 2, by - 2, kYHay[0]); c.set(bx - 2, by - 1, kYHay[0]);
  }
}

// ---------------------------------------------------------------- the trough
void paintTrough(Canvas& c, const YardLook& Y, const FarmObjLook& l) {
  const bool t = l.turned;
  const Foot F = footOf(c, t ? 1 : 2, t ? 2 : 1);
  Scene S;
  const bool stone = Y.cul == 1 || Y.cul == 3 || Y.cul == 4 || Y.cul == 9 || Y.cul == 11;
  const Ramp& M = stone ? Y.stone : Y.wood;
  const float x0 = F.x0 + (t ? 3 : 2), x1 = F.x1 - (t ? 3 : 2), y0 = F.y0 + (t ? 2 : 4), y1 = F.y1 - (t ? 2 : 3);
  Part& box = S.add(kBox, x0, x1, y0, y1, 0, 6, M, stone ? kTexStone : kTexPlanksH);
  box.paint = [x0, x1, y0, y1](const Hit& h, uint32_t& col) {
    if (!h.top) return false;
    const bool rim = h.x < x0 + 1.5f || h.x > x1 - 1.5f || h.y < y0 + 1.5f || h.y > y1 - 1.5f;
    if (rim) return false;
    col = (hash3(h.px, h.py, 7) % 11 == 0) ? kWater[4] : (h.y < y0 + 2.5f ? kWater[1] : kWater[2]);
    return true;
  };
  S.render(c);
  // iron bands on a wooden trough
  if (!stone) {
    if (!t) for (int z = 1; z <= 5; z++) { c.set((int)x0 + 3, (int)y1 - z, kIron[2]); c.set((int)x1 - 4, (int)y1 - z, kIron[2]); }
  }
}

// ---------------------------------------------------------------- the workbench
void paintWorkbench(Canvas& c, const YardLook& Y, const FarmObjLook& l) {
  const bool t = l.turned;
  const Foot F = footOf(c, t ? 1 : 2, t ? 2 : 1);
  Scene S;
  const Ramp& Wd = Y.wood;
  const float x0 = F.x0 + 2, x1 = F.x1 - 2, y0 = F.y0 + 3, y1 = F.y1 - 3;
  // four legs, a shelf, the thick top
  S.add(kBox, x0, x0 + 2, y0, y0 + 2, 0, 9, Wd, kTexPlanksV);
  S.add(kBox, x1 - 2, x1, y0, y0 + 2, 0, 9, Wd, kTexPlanksV);
  S.add(kBox, x0, x0 + 2, y1 - 2, y1, 0, 9, Wd, kTexPlanksV);
  S.add(kBox, x1 - 2, x1, y1 - 2, y1, 0, 9, Wd, kTexPlanksV);
  Part& shelf = S.add(kBox, x0 + 1, x1 - 1, y0 + 1, y1 - 1, 2, 3, Wd, kTexPlanksH);
  shelf.bias = -1;
  Part& top = S.add(kBox, x0 - 1, x1 + 1, y0 - 0.5f, y1 + 0.5f, 9, 11, Wd, kTexPlanksH);
  top.paint = [t](const Hit& h, uint32_t& col) {
    if (!h.top) return false;
    const float a = t ? h.x : h.y;
    const int r = (((int)std::floor(a)) % 3 + 3) % 3;
    col = kWood[r == 0 ? 2 : (r == 1 ? 4 : 3)];
    return true;
  };
  S.render(c);
  // tools: a saw and a mallet on top, a vice at the end, offcuts on the shelf
  const int ty = (int)(y0 + (y1 - y0) * 0.5f) - 11;
  if (!t) {
    const int sx = (int)x0 + 4;
    for (int x = sx; x <= sx + 7; x++) c.set(x, ty, kIron[x == sx ? 4 : 3]);
    for (int x = sx + 1; x <= sx + 6; x += 2) c.set(x, ty + 1, kIron[1]);
    c.set(sx + 8, ty, kWood[1]); c.set(sx + 9, ty, kWood[2]);
    const int mx = (int)x1 - 8;
    c.set(mx, ty - 1, kWood[4]); c.set(mx + 1, ty - 1, kWood[3]); c.set(mx, ty, kWood[2]); c.set(mx + 1, ty, kWood[1]);
    c.set(mx + 2, ty, kWood[3]); c.set(mx + 3, ty, kWood[3]); c.set(mx + 4, ty, kWood[2]);
    c.set((int)x1, ty + 1, kIron[3]); c.set((int)x1 + 1, ty + 1, kIron[2]); c.set((int)x1, ty + 2, kIron[1]);
    c.set((int)x0 + 6, (int)y1 - 4, kYPaleWood[3]); c.set((int)x0 + 7, (int)y1 - 4, kYPaleWood[2]); c.set((int)x0 + 10, (int)y1 - 4, kYPaleWood[3]);
  } else {
    const int sx = (int)F.cx, sy = (int)(y0 + 4) - 11;
    for (int y = sy; y <= sy + 7; y++) c.set(sx, y, kIron[3]);
    c.set(sx, sy + 8, kWood[2]); c.set(sx, sy + 9, kWood[1]);
    c.set(sx + 3, sy + 12, kWood[3]); c.set(sx + 3, sy + 13, kWood[2]);
  }
}

// ---------------------------------------------------------------- the forge: a stone hearth under a hood, the anvil
void paintForge(Canvas& c, const YardLook& Y, const FarmObjLook& l) {
  const Foot F = footOf(c, 2, 2);
  Scene S;
  const Ramp& St = Y.stone;
  const float x0 = F.x0 + 3, x1 = F.x1 - 9, y0 = F.y0 + 2, y1 = F.y0 + 18;
  // the hearth: a stone block, its fire bed on top
  Part& hearth = S.add(kBox, x0, x1, y0, y1, 0, 9, St, kTexStone);
  hearth.seed = 4;
  const float fx0 = x0 + 3, fx1 = x1 - 3, fy0 = y0 + 4, fy1 = y1 - 2;
  hearth.paint = [fx0, fx1, fy0, fy1](const Hit& h, uint32_t& col) {
    if (!h.top || h.x < fx0 || h.x > fx1 || h.y < fy0 || h.y > fy1) return false;
    const uint32_t q = hash3(h.px, h.py, 17u);
    col = q % 3 == 0 ? kFire[4] : (q % 3 == 1 ? kFire[2] : kFire[0]);
    if (h.y < fy0 + 1.5f) col = kFire[1];
    return true;
  };
  // the hood over the back half and its chimney
  Part& hood = S.add(kPyramid, x0 - 1, x1 + 1, y0 - 1, y0 + 10, 16, 26, St, kTexStone);
  hood.thick = 3;
  S.add(kBox, (x0 + x1) * 0.5f - 3, (x0 + x1) * 0.5f + 3, y0 + 1.5f, y0 + 7.5f, 20, 40, St, kTexStone);
  // the posts holding the hood's front
  S.add(kBox, x0, x0 + 2, y0 + 8, y0 + 10, 9, 16, Y.wood, kTexPlanksV);
  S.add(kBox, x1 - 2, x1, y0 + 8, y0 + 10, 9, 16, Y.wood, kTexPlanksV);
  // the anvil on its stump, front right
  const float ax = F.x1 - 8, ay = F.y1 - 6;
  Part& stump = S.add(kCyl, ax - 3.5f, ax + 3.5f, ay - 3, ay + 3, 0, 6, kBark, kTexBark);
  stump.paint = [](const Hit& h, uint32_t& col) { if (!h.top) return false; col = kYPaleWood[(hash3(h.px, h.py, 9) % 3) ? 3 : 2]; return true; };
  S.add(kBox, ax - 2, ax + 2, ay - 1.5f, ay + 1.5f, 6, 8, kIron, kTexMetal);
  Part& face = S.add(kBox, ax - 4.5f, ax + 4, ay - 2, ay + 2, 8, 10.5f, kIron, kTexMetal);
  face.paint = [ax](const Hit& h, uint32_t& col) { if (!h.top) return false; col = h.x > ax + 2.5f ? kIron[3] : kIron[4]; return true; };
  // the quench barrel at the front left
  const float bx = F.x0 + 6, by = F.y1 - 5;
  Part& bar = S.add(kCyl, bx - 3.5f, bx + 3.5f, by - 3, by + 3, 0, 8, kWood, kTexPlanksV);
  bar.paint = [](const Hit& h, uint32_t& col) {
    if (h.top) { col = kWater[(hash3(h.px, h.py, 5) % 7 == 0) ? 3 : 1]; return true; }
    if (std::fabs(h.z - 2) < 0.6f || std::fabs(h.z - 6) < 0.6f) { col = kIron[1]; return true; }
    return false;
  };
  S.render(c);
  // the fire's glow on the hood's underside and sparks; the bellows at the hearth's side
  const int gx = (int)((x0 + x1) * 0.5f), gy = (int)(y0 + 10 - 9);
  c.set(gx - 1, gy - 2, kFire[4]); c.set(gx + 2, gy - 4, kFire[3]); c.set(gx, gy - 6, kFire[2]);
  const int bwx = (int)x0 - 1, bwy = (int)y1 - 6;
  for (int y = bwy - 2; y <= bwy + 1; y++) for (int x = bwx - 3; x <= bwx; x++) c.set(x, y, kLeather[x == bwx - 3 ? 3 : ((y & 1) ? 1 : 2)]);
  c.set(bwx - 4, bwy - 1, kWood[2]); c.set(bwx - 4, bwy, kWood[1]);
  (void)l;
}

// ---------------------------------------------------------------- the flower bed
void paintFlowerBed(Canvas& c, const YardLook& Y, const FarmObjLook& l) {
  const Foot F = footOf(c, 1, 1);
  Scene S;
  const bool stone = Y.cul == 3 || Y.cul == 4 || Y.cul == 9 || Y.cul == 11 || Y.cul == 1;
  const float x0 = F.x0 + 1, x1 = F.x1 - 1, y0 = F.y0 + 3, y1 = F.y1 - 2;
  Part& bed = S.add(kBox, x0, x1, y0, y1, 0, 3, stone ? Y.stone : Y.wood, stone ? kTexStone : kTexPlanksH);
  bed.paint = [x0, x1, y0, y1](const Hit& h, uint32_t& col) {
    if (!h.top) return false;
    if (h.x < x0 + 1 || h.x > x1 - 1 || h.y < y0 + 1 || h.y > y1 - 1) return false;
    col = kTilled[(hash3(h.px, h.py, 3) % 4 == 0) ? 3 : 2];
    return true;
  };
  S.render(c);
  // flowers: leaf clumps and blooms in the bed's colours (by variant)
  static const uint32_t pal[5][2] = {{rgba(226, 60, 70), rgba(250, 220, 90)}, {rgba(250, 230, 240), rgba(150, 120, 220)},
                                     {rgba(250, 150, 60), rgba(240, 240, 250)}, {rgba(210, 90, 170), rgba(250, 210, 80)}, {rgba(120, 150, 240), rgba(250, 250, 240)}};
  const int pi = (int)((l.variant + (uint32_t)(Y.cul + 1)) % 5);
  for (int i = 0; i < 7; i++) {
    const int fx = (int)x0 + 2 + (i * 5) % 11, fy = (int)y0 - 3 + 1 + (i % 3) * 3 + (i > 3 ? 1 : 0);
    c.set(fx, fy + 1, kLeaf[2]); c.set(fx - 1, fy + 1, kLeaf[3]); c.set(fx + 1, fy + 2, kLeaf[1]); c.set(fx, fy + 2, kLeaf[2]);
    const uint32_t b = pal[pi][i & 1];
    c.set(fx, fy, b); c.set(fx, fy - 1, mixc(b, kWhite, 0.4f)); c.set(fx + 1, fy, mixc(b, kInk, 0.25f)); c.set(fx - 1, fy, mixc(b, kWhite, 0.15f));
  }
}

// ---------------------------------------------------------------- the sapling: a whip on a stake, growing into a tree
void paintSapling(Canvas& c, const YardLook& Y, const FarmObjLook& l) {
  const float cx = c.w * 0.5f, g = c.h - 6.0f;
  const int st = std::clamp((int)l.stage, 0, 3);
  const Ramp& Lf = (l.variant & 2) ? kBirchLeaf : kLeaf;
  // a little ring of turned earth round its foot
  for (int x = -4; x <= 4; x++) { c.set((int)cx + x, (int)g + 1, kTilled[(x & 1) ? 2 : 3]); if (std::abs(x) < 4) c.set((int)cx + x, (int)g + 2, kTilled[1]); }
  // the stake and its tie
  if (st < 3) { for (int y = (int)g - 14; y <= (int)g; y++) { c.set((int)cx + 3, y, Y.wood[3]); c.set((int)cx + 4, y, Y.wood[1]); } }
  static const float trunkH[4] = {7, 12, 17, 18};
  const float th = trunkH[st];
  tube(c, V2(cx, g), V2(cx + 0.5f, g - th * 0.5f), V2(cx + (l.variant & 1 ? 1.0f : -0.5f), g - th), st >= 2 ? 1.2f : 0.6f, st >= 2 ? 0.8f : 0.5f, (l.variant & 2) ? kBone : kBark, 2);
  if (st < 3) { c.set((int)cx + 1, (int)g - 9, kCloth[2]); c.set((int)cx + 2, (int)g - 9, kCloth[1]); }
  const Vec2 top{cx + (l.variant & 1 ? 1.0f : -0.5f), g - th};
  if (st == 0) {
    for (int i = 0; i < 4; i++) leaf(c, top + V2(0, i * 2.0f), -PI / 2 + (i & 1 ? 0.9f : -0.9f), 3.0f, 1.0f, 0.3f, Lf, 3 - (i & 1), false);
    return;
  }
  // the crown: clusters of leaves lit from the top-left (balls), a few branches showing
  const float r = st == 1 ? 5.0f : (st == 2 ? 7.5f : 10.0f);
  const Vec2 cc = top + V2(0, -r * 0.55f);
  if (st >= 2) { tube2(c, top, top + V2(-r * 0.5f, -r * 0.6f), 0.6f, 0.4f, kBark, 2); tube2(c, top, top + V2(r * 0.5f, -r * 0.5f), 0.6f, 0.4f, kBark, 1); }
  const int nb = 3 + st * 2;
  for (int i = 0; i < nb; i++) {
    const float a = (float)i / nb * TAU + (l.variant & 3) * 0.7f;
    const float d = r * 0.55f * (0.6f + (hash3(i, 1, l.variant) % 5) * 0.1f);
    const Vec2 p = cc + V2(std::cos(a) * d, std::sin(a) * d * 0.8f);
    furBall(c, p.x, p.y, r * 0.45f, r * 0.42f, Lf, 0.4f, l.variant + (uint32_t)i, std::sin(a) > 0.3f ? -1 : 0);
  }
  furBall(c, cc.x - r * 0.15f, cc.y - r * 0.15f, r * 0.55f, r * 0.5f, Lf, 0.4f, l.variant + 99u);
  if (st == 3 && (l.variant & 1)) for (int i = 0; i < 5; i++) {   // fruit
    const int fx = (int)(cc.x - r * 0.6f) + (int)(hash3(i, 5, l.variant) % (uint32_t)(r * 1.2f)), fy = (int)(cc.y - r * 0.3f) + (int)(hash3(i, 6, l.variant) % (uint32_t)(r * 0.8f));
    if (solid(c, fx, fy)) { c.set(fx, fy, kRed[3]); c.set(fx, fy + 1, kRed[1]); }
  }
}

// ---------------------------------------------------------------- the bench (planks, stone or a split log by culture)
void paintBench(Canvas& c, const YardLook& Y, const FarmObjLook& l) {
  const bool t = l.turned;
  const Foot F = footOf(c, t ? 1 : 2, t ? 2 : 1);
  Scene S;
  const int kind = (Y.cul == 3 || Y.cul == 9 || Y.cul == 11) ? 1 : ((Y.cul == 0 || Y.cul == 1 || Y.cul == 10 || Y.cul == 5) ? 2 : 0);
  const float x0 = F.x0 + 2, x1 = F.x1 - 2, y0 = F.y0 + (t ? 2 : 6), y1 = F.y1 - (t ? 2 : 3);
  const Ramp& Wd = Y.wood;
  if (kind == 1) {   // a stone slab on two blocks
    S.add(kBox, x0 + 1, x0 + (t ? x1 - x0 - 1 : 5), y0 + 1, t ? y0 + 5 : y1 - 1, 0, 5, Y.stone, kTexStone);
    S.add(kBox, t ? x0 + 1 : x1 - 5, x1 - 1, t ? y1 - 5 : y0 + 1, y1 - 1, 0, 5, Y.stone, kTexStone);
    S.add(kBox, x0, x1, y0, y1, 5, 7.5f, Y.stone, kTexPlain);
  } else if (kind == 2) {   // a split log on two stubby legs
    if (!t) { S.add(kBox, x0 + 3, x0 + 5, y0 + 1, y1 - 1, 0, 4, Wd); S.add(kBox, x1 - 5, x1 - 3, y0 + 1, y1 - 1, 0, 4, Wd); }
    else { S.add(kBox, x0 + 1, x1 - 1, y0 + 3, y0 + 5, 0, 4, Wd); S.add(kBox, x0 + 1, x1 - 1, y1 - 5, y1 - 3, 0, 4, Wd); }
    Part& lg = S.add(kBox, x0, x1, y0, y1, 4, 7, kBark, kTexBark);
    lg.paint = [t](const Hit& h, uint32_t& col) { if (!h.top) return false; const float a = t ? h.y : h.x; col = kYPaleWood[((int)std::floor(a) % 5 == 0) ? 2 : 3]; return true; };
  } else {   // planks on legs with a back rest
    if (!t) {
      S.add(kBox, x0 + 1, x0 + 3, y0, y1, 0, 5, Wd); S.add(kBox, x1 - 3, x1 - 1, y0, y1, 0, 5, Wd);
      S.add(kBox, x0, x1, y0, y1, 5, 6.5f, Wd, kTexPlanksH);
      S.add(kBox, x0 + 1, x0 + 2.5f, y0 - 1, y0, 0, 12, Wd); S.add(kBox, x1 - 2.5f, x1 - 1, y0 - 1, y0, 0, 12, Wd);
      S.add(kBox, x0, x1, y0 - 1, y0, 9, 12, Wd, kTexPlanksH);
    } else {
      S.add(kBox, x0, x1, y0 + 1, y0 + 3, 0, 5, Wd); S.add(kBox, x0, x1, y1 - 3, y1 - 1, 0, 5, Wd);
      S.add(kBox, x0, x1, y0, y1, 5, 6.5f, Wd, kTexPlanksH);
      S.add(kBox, x1 - 1, x1, y0, y1, 5, 12, Wd, kTexPlanksV);   // the back rest on the east side
    }
  }
  S.render(c);
  (void)l;
}

// ---------------------------------------------------------------- the lantern (iron lamp, paper lantern, stone lantern, brazier)
void paintLantern(Canvas& c, const YardLook& Y, const FarmObjLook& l) {
  const float cx = c.w * 0.5f, g = c.h - 6.0f;
  const int kind = Y.cul == 7 ? 2 : ((Y.cul == 8 || Y.cul == 6) ? 1 : ((Y.cul == 4 || Y.cul == 9) ? 3 : 0));
  if (kind == 2) {   // a stone lantern: base, shaft, fire box, roof cap
    Scene S;
    const Ramp& St = kStone;
    S.add(kBox, cx - 4, cx + 4, g - 3, g + 3, 0, 3, St, kTexStone);
    S.add(kBox, cx - 1.5f, cx + 1.5f, g - 1.5f, g + 1.5f, 3, 13, St);
    S.add(kBox, cx - 3.5f, cx + 3.5f, g - 3, g + 3, 13, 19, St).paint = [cx](const Hit& h, uint32_t& col) {
      if (h.top || std::fabs(h.x - cx) > 1.6f || h.z < 14 || h.z > 18) return false;
      col = kGlow[h.z > 16 ? 3 : 2]; return true;
    };
    Part& cap = S.add(kPyramid, cx - 5.5f, cx + 5.5f, g - 4.5f, g + 4.5f, 19, 24, St);
    cap.thick = 2;
    S.render(c);
    return;
  }
  // a post
  const Ramp& P = kind == 0 ? kIron : Y.wood;
  const int top = kind == 1 ? (int)g - 30 : (int)g - 26;
  for (int y = top; y <= (int)g; y++) { c.set((int)cx, y, P[3]); c.set((int)cx + 1, y, P[1]); }
  c.set((int)cx - 1, (int)g, P[2]); c.set((int)cx + 2, (int)g, P[0]);
  if (kind == 0) {   // an iron lamp: an arm, a glazed lamp with a cap
    for (int x = (int)cx - 3; x <= (int)cx + 1; x++) c.set(x, top + 1, kIron[2]);
    const int lx = (int)cx - 3, ly = top + 3;
    for (int y = ly; y <= ly + 5; y++) for (int x = lx - 2; x <= lx + 2; x++) c.set(x, y, (x == lx - 2 || x == lx + 2) ? kIron[1] : kGlow[y < ly + 2 ? 3 : 2]);
    for (int x = lx - 3; x <= lx + 3; x++) c.set(x, ly - 1, kIron[x < lx ? 3 : 2]);
    c.set(lx, ly - 2, kIron[3]);
    for (int x = lx - 2; x <= lx + 2; x++) c.set(x, ly + 6, kIron[1]);
  } else if (kind == 1) {   // a paper lantern hung from a crooked arm
    for (int x = (int)cx - 4; x <= (int)cx + 1; x++) c.set(x, top, Y.wood[2]);
    const float lx = cx - 4, ly = top + 6.0f;
    for (int y = top + 1; y <= top + 2; y++) c.set((int)lx, y, kCloth[1]);
    ball(c, lx, ly, 3.0f, 3.6f, ramp(Y.accent ? Y.accent : rgba(220, 70, 50)), 0.05f, 1);
    for (int x = (int)lx - 2; x <= (int)lx + 2; x++) { c.set(x, (int)ly - 3, kWoodDark[1]); c.set(x, (int)ly + 3, kWoodDark[1]); }
    c.set((int)lx, (int)ly, kGlow[4]);
  } else {   // a brazier on a tripod
    const int by = top + 2;
    for (int x = (int)cx - 4; x <= (int)cx + 5; x++) { c.set(x, by, kBrass[x < cx ? 3 : 2]); c.set(x, by + 1, kBrass[1]); }
    for (int x = (int)cx - 3; x <= (int)cx + 4; x++) c.set(x, by - 1, kFire[(x & 1) ? 1 : 0]);
    c.set((int)cx, by - 2, kFire[3]); c.set((int)cx + 1, by - 3, kFire[4]); c.set((int)cx - 1, by - 2, kFire[2]); c.set((int)cx + 2, by - 2, kFire[2]);
  }
  (void)l;
}

// ---------------------------------------------------------------- the statue: a figure on a plinth (the human rig in stone)
void paintStatue(Canvas& c, const YardLook& Y, const FarmObjLook& l) {
  const float cx = c.w * 0.5f, g = c.h - 6.0f;
  Scene S;
  const bool bronze = (l.variant & 1) && Y.cul != 4;
  const Ramp& St = (Y.cul == 3 || Y.cul == 11 || Y.cul == 9) ? kYAshlar : kStone;
  S.add(kBox, cx - 7, cx + 7, g - 5, g + 4, 0, 3, St, kTexStone);
  S.add(kBox, cx - 5, cx + 5, g - 4, g + 3, 3, 11, St, kTexPlain);
  S.add(kBox, cx - 6, cx + 6, g - 4.5f, g + 3.5f, 11, 12.5f, St, kTexPlain);
  S.render(c);
  // the figure: a standing hero recoloured in stone or bronze by its light
  HumanLook L;
  L.outfit = (l.variant & 2) ? Outfit::Robe : Outfit::Guard;
  L.helmet = !(l.variant & 2);
  L.weapon = (l.variant & 2) ? 0 : 1;
  L.shield = !(l.variant & 2);
  L.hair = Hair::Short;
  const Canvas fig = humanFigureStill(L, 0);
  const Ramp Bz = ramp5(rgba(54, 40, 34), rgba(96, 68, 44), rgba(140, 100, 58), rgba(186, 140, 76), rgba(226, 188, 112));
  const Ramp& M = bronze ? Bz : St;
  const int ox = (int)cx - 8, oy = (int)(g - 12.5f) - 23;
  for (int y = 0; y < fig.h; y++)
    for (int x = 0; x < fig.w; x++) {
      const uint32_t p = fig.get(x, y);
      if (!chA(p)) continue;
      const float lu = luma(p);
      int k = lu > 0.72f ? 4 : (lu > 0.52f ? 3 : (lu > 0.34f ? 2 : (lu > 0.2f ? 1 : 0)));
      if (x < 6 && k < 4) k++;
      c.set(ox + x, oy + y, M[k]);
    }
  // a little moss / verdigris on the plinth
  if (l.variant & 4) { c.set((int)cx - 5, (int)g - 4, kMoss[2]); c.set((int)cx - 4, (int)g - 4, kMoss[3]); }
}

// ---------------------------------------------------------------- the banner in the player's arms
void paintBanner(Canvas& c, const YardLook& Y, const FarmObjLook& l) {
  const float cx = c.w * 0.5f - 4, g = c.h - 6.0f;
  const uint32_t field = l.banner ? opaque(l.banner) : rgba(150, 40, 40);
  const uint32_t charge = l.banner2 ? opaque(l.banner2) : rgba(230, 200, 90);
  const Ramp Fd = ramp(field), Ch = ramp(charge);
  // the pole, its finial, the cross-bar, a stone footing
  const int top = (int)g - 36;
  for (int y = top; y <= (int)g; y++) { c.set((int)cx, y, Y.wood[3]); c.set((int)cx + 1, y, Y.wood[1]); }
  c.set((int)cx, top - 1, kGold[4]); c.set((int)cx + 1, top - 1, kGold[2]); c.set((int)cx, top - 2, kGold[3]);
  for (int x = (int)cx; x <= (int)cx + 12; x++) { c.set(x, top + 2, Y.wood[3]); c.set(x, top + 3, Y.wood[1]); }
  for (int x = (int)cx - 2; x <= (int)cx + 3; x++) { c.set(x, (int)g, kStone[x < cx ? 3 : 2]); c.set(x, (int)g + 1, kStone[1]); }
  // the cloth: hanging from the bar, a swallow-tail foot, a soft fold, a trim down the edge
  const int bx0 = (int)cx + 2, bx1 = (int)cx + 12, by0 = top + 4, by1 = top + 22;
  for (int y = by0; y <= by1 + 3; y++)
    for (int x = bx0; x <= bx1; x++) {
      const int mid = (bx0 + bx1) / 2;
      if (y > by1 && std::abs(x - mid) < (y - by1) * 2) continue;   // the swallow-tail notch
      int k = 2;
      if (x == bx0) k = 3;
      if (x == bx1) k = 1;
      if ((x - bx0) == 6 + ((y / 5) & 1)) k = 1;                      // a fold
      if ((x - bx0) == 5 + ((y / 5) & 1)) k = 3;
      c.set(x, y, Fd[k]);
    }
  for (int y = by0; y <= by1 + 2; y++) { c.set(bx0, y, Ch[2]); c.set(bx1, y, Ch[1]); }
  for (int x = bx0; x <= bx1; x++) c.set(x, by0, Ch[3]);
  // the charge: a glyph of the arms
  const Canvas gl = glyphSprite(0x5EED0000u + l.emblem * 977u, l.emblem % 4, charge, 1);
  const int gx = (bx0 + bx1) / 2 - gl.w / 2 + 1, gy = by0 + 5;
  for (int y = 0; y < gl.h; y++) for (int x = 0; x < gl.w; x++) if (chA(gl.get(x, y)) > 128) c.set(gx + x, gy + y, gl.get(x, y));
}

// ---------------------------------------------------------------- the campfire
void paintCampfire(Canvas& c, const YardLook& Y, const FarmObjLook& l) {
  const float cx = c.w * 0.5f, cy = c.h - 7.0f;
  // the ring of stones
  for (int i = 0; i < 9; i++) {
    const float a = (float)i / 9 * TAU;
    const float sx = cx + std::cos(a) * 6.0f, sy = cy + std::sin(a) * 3.6f;
    ball(c, sx, sy, 1.7f, 1.4f, Y.stone, 0.05f, std::sin(a) > 0.2f ? 0 : -1);
  }
  // the ash bed, crossed logs, flames
  ellipse(c, cx, cy - 0.2f, 4.2f, 2.0f, rgba(70, 60, 66));
  tube2(c, V2(cx - 4, cy + 1), V2(cx + 3, cy - 2), 1.0f, 1.0f, kBark, 2);
  tube2(c, V2(cx + 4, cy + 1), V2(cx - 3, cy - 2), 1.0f, 1.0f, kBark, 1);
  for (int i = 0; i < 3; i++) {
    const float fx = cx - 2 + i * 2.0f, h = i == 1 ? 8.0f : 5.0f + (l.variant & 1);
    for (int y = 0; y < (int)h; y++) {
      const float t = y / h, half = (1 - t) * 1.6f + 0.3f;
      for (int x = (int)std::floor(fx - half); x <= (int)std::ceil(fx + half - 1); x++)
        c.set(x, (int)(cy - 1 - y), kFire[t < 0.3f ? 4 : (t < 0.6f ? 3 : 2)]);
    }
  }
  c.set((int)cx, (int)cy - 1, kFire[4]);
}

// ---------------------------------------------------------------- the doghouse
void paintDoghouse(Canvas& c, const YardLook& Y, const FarmObjLook& l) {
  const Foot F = footOf(c, 1, 1);
  Scene S;
  const float x0 = F.x0 + 1.5f, x1 = F.x1 - 1.5f, y0 = F.y0 + 2, y1 = F.y1 - 3;
  const Ramp& Wl = Y.cul < 0 ? Y.wood : Y.wall;
  S.add(kBox, x0, x1, y0, y1, 0, 8, Wl, Y.cul < 0 ? kTexPlanksV : wallTex(Y.wallMat));
  YardLook R2 = Y;
  addRoof(S, R2, x0 - 1.5f, x1 + 1.5f, y0 - 1.5f, y1 + 2, 8, 7, true, l.variant, true);
  S.render(c);
  // the arched opening, a bowl, the dog's name board
  const int fy = (int)y1;
  opening(c, (int)F.cx - 2, fy - 6, (int)F.cx + 1, fy - 1, Y.trim, true);
  const int bx = (int)x1 + 1, by = fy + 1;
  c.set(bx - 1, by, kIron[3]); c.set(bx, by, kIron[2]); c.set(bx + 1, by, kIron[1]); c.set(bx, by - 1, kBone[3]);
}

// ---------------------------------------------------------------- the hay rack: a slatted manger of hay under a lean-to
void paintHayRack(Canvas& c, const YardLook& Y, const FarmObjLook& l) {
  const bool t = l.turned;
  const Foot F = footOf(c, t ? 1 : 2, t ? 2 : 1);
  Scene S;
  const Ramp& Wd = Y.wood;
  const float x0 = F.x0 + 2, x1 = F.x1 - 2, y0 = F.y0 + 2, y1 = F.y1 - 3;
  // corner posts
  S.add(kBox, x0, x0 + 2, y0, y0 + 2, 0, 15, Wd, kTexPlanksV);
  S.add(kBox, x1 - 2, x1, y0, y0 + 2, 0, 15, Wd, kTexPlanksV);
  S.add(kBox, x0, x0 + 2, y1 - 2, y1, 0, 15, Wd, kTexPlanksV);
  S.add(kBox, x1 - 2, x1, y1 - 2, y1, 0, 15, Wd, kTexPlanksV);
  // the manger: hay heaped in a slatted box
  Part& hay = S.add(kEllip, x0 + 1, x1 - 1, y0 + 1, y1 - 1, 7, 15, kYHay, kTexHay);
  (void)hay;
  Part& box = S.add(kBox, x0 + 0.5f, x1 - 0.5f, y0 + 0.5f, y1 - 0.5f, 3, 9, Wd, kTexPlain);
  box.paint = [x0, x1, t](const Hit& h, uint32_t& col) {
    if (h.top) return false;
    const float a = t ? h.y : h.x;
    const int ph = (((int)std::floor(a - x0)) % 3 + 3) % 3;
    if (ph == 2 && h.z > 4 && h.z < 8.5f) { col = kYHay[(hash3(h.px, h.py, 4) % 3) ? 2 : 1]; return true; }   // hay between the slats
    col = kWood[ph == 0 ? 3 : 2];
    (void)x1;
    return true;
  };
  S.render(c);
}

// ---------------------------------------------------------------- the shipping crate
void paintCrate(Canvas& c, const YardLook& Y, const FarmObjLook& l) {
  const Foot F = footOf(c, 1, 1);
  Scene S;
  const float x0 = F.x0 + 1, x1 = F.x1 - 1, y0 = F.y0 + 2, y1 = F.y1 - 2;
  Part& b = S.add(kBox, x0, x1, y0, y1, 0, 11, Y.cul == 7 ? kYPaleWood : kWood, kTexPlanksH);
  b.paint = [x0, x1, y0, y1](const Hit& h, uint32_t& col) {
    // the frame: battens round every face, iron corners; the lid's boards on top
    const bool edgeX = h.x < x0 + 1.5f || h.x > x1 - 1.5f;
    if (h.top) {
      const bool edge = edgeX || h.y < y0 + 1.5f || h.y > y1 - 1.5f;
      if (edge && (h.x < x0 + 1.5f || h.x > x1 - 1.5f) && (h.y < y0 + 1.5f || h.y > y1 - 1.5f)) { col = kIron[3]; return true; }
      col = kWood[edge ? 4 : (((int)std::floor(h.x) % 4) == 0 ? 2 : 3)];
      return true;
    }
    const bool edgeZ = h.z < 1.5f || h.z > 9.5f;
    if (edgeX && edgeZ) { col = kIron[edgeX && h.x < x0 + 1.5f ? 3 : 1]; return true; }
    if (edgeX || edgeZ) { col = kWood[h.x < x0 + 1.5f ? 3 : 1]; return true; }
    // the diagonal batten
    const float u = (h.x - x0) / (x1 - x0), v = h.z / 11.0f;
    if (std::fabs(u - v) < 0.12f) { col = kWood[3]; return true; }
    return false;
  };
  S.render(c);
  // a stencilled mark (the farm's) on the face
  const int mx = (int)F.cx + 2, my = (int)y1 - 5;
  c.set(mx, my, kWoodDark[1]); c.set(mx + 1, my, kWoodDark[1]); c.set(mx, my + 1, kWoodDark[1]);
  (void)l;
}

}  // namespace

// ---------------------------------------------------------------- the public entry points
int farmObjW(const FarmObjLook& l) { return objSize(l.kind, l.turned && turnable(l.kind)).w; }
int farmObjH(const FarmObjLook& l) { return objSize(l.kind, l.turned && turnable(l.kind)).h; }

Canvas farmObjSprite(const FarmObjLook& l0) {
  FarmObjLook l = l0;
  if (!turnable(l.kind) && l.kind != FarmObj::Gate) l.turned = false;
  if (l.kind == FarmObj::Fence || l.kind == FarmObj::Gate) return fenceSprite(l);
  const ObjSize s = objSize(l.kind, l.turned);
  Canvas c(s.w, s.h);
  const YardLook Y = yardLook(l);
  switch (l.kind) {
    case FarmObj::Well: paintWell(c, Y, l); break;
    case FarmObj::Woodpile: paintWoodpile(c, Y, l); break;
    case FarmObj::Beehive: paintBeehive(c, Y, l); break;
    case FarmObj::Scarecrow: paintScarecrow(c, Y, l); break;
    case FarmObj::Coop: paintCoop(c, Y, l); break;
    case FarmObj::Pen: paintPen(c, Y, l); break;
    case FarmObj::Stable: paintStable(c, Y, l); break;
    case FarmObj::Trough: paintTrough(c, Y, l); break;
    case FarmObj::Workbench: paintWorkbench(c, Y, l); break;
    case FarmObj::Forge: paintForge(c, Y, l); break;
    case FarmObj::FlowerBed: paintFlowerBed(c, Y, l); break;
    case FarmObj::Sapling: paintSapling(c, Y, l); break;
    case FarmObj::Bench: paintBench(c, Y, l); break;
    case FarmObj::Lantern: paintLantern(c, Y, l); break;
    case FarmObj::Statue: paintStatue(c, Y, l); break;
    case FarmObj::Banner: paintBanner(c, Y, l); break;
    case FarmObj::Campfire: paintCampfire(c, Y, l); break;
    case FarmObj::Doghouse: paintDoghouse(c, Y, l); break;
    case FarmObj::HayRack: paintHayRack(c, Y, l); break;
    case FarmObj::ShippingCrate: paintCrate(c, Y, l); break;
    default: break;
  }
  // a pixel of air on every side for the outline
  for (int y = 0; y < c.h; y++) { c.set(0, y, c.get(0, y) && l.kind == FarmObj::Pen ? c.get(0, y) : 0); c.set(c.w - 1, y, 0); }
  for (int x = 0; x < c.w; x++) c.set(x, 0, 0);
  outline(c, 0.85f);
  return c;
}

// ================================================================ the house under construction
// The canvas: the footprint's width, the building's height (storeys and a roof); its bottom-left on the footprint's
// bottom-left (like the finished building's sprite). Progress 0: the plot pegged out with stakes and string, the
// timber and the stone stacked; 1: the stone footing laid, the ground-floor frame rising inside a scaffold with a ladder;
// 2: the whole frame up to the ridge (posts, beams, rafters), the walls being filled from the bottom; 3: the walls
// closed and half the roof on.
Canvas scaffoldSprite(int wTiles, int hTiles, int storeys, int progress, uint32_t seed) {
  wTiles = std::max(2, wTiles); hTiles = std::max(2, hTiles); storeys = std::clamp(storeys, 1, 3);
  progress = std::clamp(progress, 0, 3);
  const int depth = hTiles * 16;
  const float storeyH = 13.0f;
  const float wallTop = 2 + storeyH * storeys;
  const float rise = std::min(depth * 0.5f - 2, 16.0f);
  const int W = wTiles * 16, H = depth + (int)(wallTop + rise) + 6;
  Canvas c(W, H);
  const float x0 = 3, x1 = W - 3, y1 = (float)H - 3, y0 = y1 - depth + 6;
  const float ym = (y0 + y1) * 0.5f;
  const uint32_t sv = seed * 2654435761u;
  const Ramp& Wd = kWood;
  const Ramp Fresh = ramp5(rgba(110, 72, 46), rgba(160, 112, 66), rgba(204, 156, 94), rgba(230, 192, 128), rgba(248, 222, 168));   // new-sawn timber
  const Ramp& Wall = (sv & 1) ? kYPlaster : kYDaub;
  const Ramp& Roof = (sv & 2) ? kThatch : kYRoofRed;
  Scene S;
  auto box = [&](float ax0, float ax1, float ay0, float ay1, float z0, float z1, const Ramp& R, int tex = kTexPlain) -> Part& { return S.add(kBox, ax0, ax1, ay0, ay1, z0, z1, R, tex); };
  // ---- the materials stacked on the site (fewer as the work goes on)
  const int piles = 3 - progress;
  if (piles > 0) {
    const float px0 = x0 + 4, py1 = y1 - 1;
    for (int i = 0; i < piles + 1; i++) {   // planks / beams, criss-crossed layers
      Part& p = box(px0, px0 + 14, py1 - 6, py1, (float)i * 2, (float)i * 2 + 2, Fresh, kTexPlanksH);
      p.paint = [i](const Hit& h, uint32_t& col) {
        if (h.top) { const int r = ((int)std::floor((i & 1) ? h.x : h.y)) % 3; col = (r == 0) ? kWood[1] : ramp(rgba(204, 156, 94))[3]; return true; }
        const int r = (((int)std::floor(h.x)) % 3 + 3) % 3;
        col = r == 0 ? kWood[1] : ramp(rgba(204, 156, 94))[2];
        return true;
      };
    }
    // a heap of dressed stones
    for (int i = 0; i < 2 + piles; i++) {
      const float sx = x1 - 10 + (i % 3) * 3.0f, sy = y1 - 3 - (i / 3) * 2.0f;
      box(sx - 1.5f, sx + 1.5f, sy - 1.5f, sy + 1.5f, (float)(i / 3) * 2.5f, (float)(i / 3) * 2.5f + 2.5f, kStone, kTexStone);
    }
  }
  if (progress >= 1) {
    // the stone footing round the plan; trodden earth inside it
    Part& floor = box(x0 + 2, x1 - 2, y0 + 2, y1 - 8, 0, 0.6f, kEarth, kTexPlain);
    floor.paint = [](const Hit& h, uint32_t& col) { const uint32_t q = hash3(h.px, h.py, 41u); col = kEarth[q % 5 == 0 ? 1 : (q % 7 == 0 ? 3 : 2)]; return true; };
    box(x0, x1, y0, y0 + 2, 0, 2, kStone, kTexStone);
    box(x0, x1, y1 - 8, y1 - 6, 0, 2, kStone, kTexStone);
    box(x0, x0 + 2, y0, y1 - 6, 0, 2, kStone, kTexStone);
    box(x1 - 2, x1, y0, y1 - 6, 0, 2, kStone, kTexStone);
    // the corner posts and studs of the frame, as high as the work has reached
    const float top = progress == 1 ? 2 + storeyH : wallTop;
    std::vector<float> xs;
    for (float x = x0; x <= x1 - 2; x += 10) xs.push_back(x);
    xs.push_back(x1 - 2);
    for (float x : xs) { box(x, x + 2, y0, y0 + 2, 2, top, Fresh); box(x, x + 2, y1 - 8, y1 - 6, 2, top, Fresh); }
    box(x0, x0 + 2, ym - 1, ym + 1, 2, top, Fresh);
    box(x1 - 2, x1, ym - 1, ym + 1, 2, top, Fresh);
    // the beams at every storey reached
    for (int s = 1; s <= (progress == 1 ? 1 : storeys); s++) {
      const float z = 2 + storeyH * s;
      box(x0, x1, y0, y0 + 2, z - 2, z, Fresh, kTexPlanksH);
      box(x0, x1, y1 - 8, y1 - 6, z - 2, z, Fresh, kTexPlanksH);
      box(x0, x0 + 2, y0, y1 - 6, z - 2, z, Fresh);
      box(x1 - 2, x1, y0, y1 - 6, z - 2, z, Fresh);
    }
    // the walls going in (wattle and daub or boards) from the bottom
    if (progress >= 2) {
      const float wz = progress == 2 ? 2 + storeyH * 0.6f : wallTop - 1;
      Part& fw = box(x0 + 0.5f, x1 - 0.5f, y1 - 7.5f, y1 - 6.5f, 2, wz, Wall, (sv & 1) ? kTexPlaster : kTexWattle);
      fw.bias = 0;
      box(x0 + 0.5f, x1 - 0.5f, y0 + 0.5f, y0 + 1.5f, 2, wz, Wall, kTexPlaster).bias = -1;
    }
  }
  if (progress >= 2) {
    // the rafters (pairs every 8 px along the ridge) and the ridge beam
    for (float x = x0; x <= x1 - 1.5f; x += 8) {
      Part& r = S.add(kGableX, x, x + 1.5f, y0 - 2, y1 - 4, wallTop, wallTop + rise, Fresh);
      r.thick = 1.5f;
    }
    Part& rb = S.add(kGableX, x1 - 1.5f, x1, y0 - 2, y1 - 4, wallTop, wallTop + rise, Fresh);
    rb.thick = 1.5f;
    box(x0, x1, ym - 2, ym, wallTop + rise - 1.5f, wallTop + rise, Fresh, kTexPlanksH);
  }
  if (progress == 3) {   // half the roof covered: the west half, laths showing on the rest
    Part& rc = S.add(kGableX, x0 - 2, x0 + (x1 - x0) * 0.55f, y0 - 3, y1 - 3, wallTop - 1, wallTop + rise, Roof, (sv & 2) ? kTexThatch : kTexShingle);
    rc.thick = 1.8f;
    rc.seed = sv;
    for (float z = wallTop + 3; z < wallTop + rise - 1; z += 4) {
      Part& lath = S.add(kGableX, x0 + (x1 - x0) * 0.55f, x1, y0 - 2, y1 - 4, z - 0.01f, z + 0.01f, Fresh);
      (void)lath;
      S.parts.pop_back();
    }
  }
  // the scaffold along the front: standards, ledgers, a walk of planks at each storey reached, braced
  if (progress >= 1) {
    const float sy0 = y1 - 4, sy1 = y1 - 2;
    const float top = progress == 1 ? 2 + storeyH + 4 : wallTop + 4;
    for (float x = x0 + 1; x <= x1 - 1; x += 14) box(x, x + 1.2f, sy0, sy1, 0, top, Wd);
    const int levels = progress == 1 ? 1 : storeys;
    for (int s = 1; s <= levels; s++) {
      const float z = 2 + storeyH * s - 2;
      Part& walk = box(x0, x1, y1 - 6, sy1, z - 1, z, Wd, kTexPlanksV);
      walk.bias = -1;
    }
  }
  S.render(c);
  // a ladder up the scaffold
  if (progress >= 1) {
    const float top = progress == 1 ? 2 + storeyH + 3 : wallTop + 2;
    const int lx = (int)x1 - 12, ly = (int)y1 - 1;
    for (int i = 0; i <= (int)top; i++) {
      const int y = ly - i, x = lx + i / 6;
      c.set(x, y, Wd[3]); c.set(x + 4, y, Wd[1]);
      if (i % 3 == 1) for (int k = 1; k <= 3; k++) c.set(x + k, y, Wd[2]);
    }
  }
  outline(c, 0.8f);
  // (M7 fixer r2, review: "the site is a 1-px string outline on uncleared ground, it reads as a debug box") the ground
  // of the site, laid under everything standing on it (only where the canvas is still empty): the turf stripped and
  // the earth levelled over the whole plan, a ragged edge where the grass was cut back, scuffed with boot prints and
  // stones; at stage 0 the footing trench dug round the plan (its far lip in shadow, the near one lit) with the spoil
  // heaped outside it, the string lines pegged at the corners and along the sides, and a spade left in the earth.
  {
    std::vector<uint8_t> gnd((size_t)W * H, 0);   // 1: the cleared pad painted here (so the marks below go on earth only)
    const int pt = H - depth + 1, pb = H - 2;
    for (int y = pt; y <= pb; y++)
      for (int x = 1; x <= W - 2; x++) {
        if (solid(c, x, y)) continue;
        const int e = std::min(std::min(x - 1, W - 2 - x), std::min(y - pt, pb - y));
        const uint32_t q = hash3(x, y, sv ^ 0x5173u);
        if (e == 0 && q % 3 == 0) continue;   // the cut edge of the turf: ragged
        int k = 2;
        if (e == 0) k = 1;                                    // the turf's cut lip in shade
        else if (q % 17 == 0) k = 3;                          // a lit crumb / a stone
        else if (q % 13 == 0 || ((x * 3 + y * 5 + (int)(sv & 7)) % 29) == 0) k = 1;   // boot prints, the rake's marks
        c.set(x, y, kEarth[k]);
        gnd[(size_t)y * W + x] = 1;
      }
    auto onGnd = [&](int x, int y) { return x >= 0 && y >= 0 && x < W && y < H && gnd[(size_t)y * W + x]; };
    auto put = [&](int x, int y, uint32_t col) { if (onGnd(x, y)) c.set(x, y, col); };
    if (progress == 0) {
      const int a = (int)x0, b = (int)x1 - 1, t = (int)y0, f = (int)y1 - 6;
      // the footing trench, two px wide, just inside the string: the far (north / west) side in shadow, the bottom dark
      for (int x = a; x <= b; x++) { put(x, t + 1, kTilled[0]); put(x, t + 2, kTilled[1]); put(x, t + 3, kEarth[3]);
                                     put(x, f - 1, kTilled[0]); put(x, f, kTilled[1]); put(x, f + 1, kEarth[3]); }
      for (int y = t + 1; y <= f; y++) { put(a + 1, y, kTilled[0]); put(a + 2, y, kTilled[1]); put(a + 3, y, kEarth[3]);
                                         put(b - 2, y, kTilled[0]); put(b - 1, y, kTilled[1]); put(b, y, kEarth[1]); }
      // the spoil heaped outside it along the front: little mounds, lit up-left, shaded down-right
      for (int x = a + 3; x <= b - 3; x += 7 + (int)(hash3(x, 3, sv) % 4)) {
        for (int dy = -1; dy <= 1; dy++)
          for (int dx = -2; dx <= 2; dx++) {
            if (std::abs(dx) + std::abs(dy) * 2 > 3) continue;
            put(x + dx, f + 3 + dy, dx + dy < 0 ? kEarth[3] : (dx + dy > 1 ? kEarth[1] : kEarth[2]));
          }
        put(x + 3, f + 4, kTilled[2]);   // its shadow
      }
      // the string lines (a lit cord, its shadow a pixel down-right on the earth)
      for (int x = a; x <= b; x++) { put(x + 1, t + 1, kEarth[1]); put(x, t, kCloth[4]); put(x + 1, f + 1, kEarth[1]); put(x, f, kCloth[4]); }
      for (int y = t; y <= f; y++) { put(a + 1, y + 1, kEarth[1]); put(a, y, kCloth[4]); put(b + 1, y + 1, kEarth[1]); put(b, y, kCloth[4]); }
      // the stakes: at the corners and every 16 px along the sides; lit west face, shaded east, a pale cut top
      auto stake = [&](int sx, int sy) {
        for (int y = sy - 4; y <= sy; y++) { c.set(sx, y, Fresh[3]); c.set(sx + 1, y, Fresh[1]); }
        c.set(sx, sy - 5, Fresh[4]); c.set(sx + 1, sy - 5, Fresh[3]);
        put(sx + 2, sy, kTilled[1]); put(sx + 2, sy + 1, kTilled[1]); put(sx + 3, sy + 1, kEarth[1]);   // its shadow
      };
      for (int x = a; x <= b; x += 16) { stake(std::min(x, b - 1), t); stake(std::min(x, b - 1), f); }
      stake(b - 1, t); stake(b - 1, f);
      for (int y = t + 16; y < f - 4; y += 16) { stake(a, y); stake(b - 1, y); }
      // a spade stood in the spoil
      const int sx = b - 6;
      for (int y = f - 4; y <= f + 2; y++) { c.set(sx, y, kWood[2]); c.set(sx + 1, y, kWood[1]); }
      c.set(sx - 1, f - 5, kWood[3]); c.set(sx, f - 5, kWood[3]); c.set(sx + 1, f - 5, kWood[2]); c.set(sx + 2, f - 5, kWood[1]);
      c.set(sx, f + 3, kIron[3]); c.set(sx + 1, f + 3, kIron[2]); c.set(sx - 1, f + 3, kIron[2]); c.set(sx + 2, f + 3, kIron[1]);
      put(sx + 2, f + 4, kTilled[1]); put(sx + 3, f + 4, kTilled[1]);
    }
  }
  return c;
}

// ================================================================ trophies
namespace {
enum TrophyHead { kTCanine, kTBoar, kTBear, kTSkull, kTDragon, kTSpider, kTClaw, kTCroc, kTOrb, kTWings, kTTroll, kTGoblin };
TrophyHead trophyOf(Monster m) {
  switch (m) {
    case Monster::Wolf: case Monster::IceWolf: case Monster::Hyena: case Monster::EmberHound: return kTCanine;
    case Monster::Boar: return kTBoar;
    case Monster::Bear: case Monster::Yeti: return kTBear;
    case Monster::Skeleton: case Monster::Draugr: case Monster::Wraith: return kTSkull;
    case Monster::Dragon: return kTDragon;
    case Monster::Spider: case Monster::FrostSpider: return kTSpider;
    case Monster::Mudcrab: case Monster::Scorpion: return kTClaw;
    case Monster::Lurker: case Monster::Sandworm: return kTCroc;
    case Monster::Wisp: case Monster::Slime: case Monster::Golem: return kTOrb;
    case Monster::Bat: case Monster::Harpy: return kTWings;
    case Monster::Troll: case Monster::Blightspawn: return kTTroll;
    case Monster::Goblin: return kTGoblin;
    default: return kTCanine;
  }
}
Ramp trophyFur(Monster m) {
  switch (m) {
    case Monster::Wolf: return ramp(rgba(128, 122, 120));
    case Monster::IceWolf: return ramp(rgba(214, 226, 236));
    case Monster::Hyena: return ramp(rgba(184, 150, 96));
    case Monster::EmberHound: return ramp(rgba(56, 46, 50));
    case Monster::Boar: return ramp(rgba(110, 76, 56));
    case Monster::Bear: return ramp(rgba(112, 74, 48));
    case Monster::Yeti: return ramp(rgba(230, 230, 236));
    case Monster::Troll: return ramp(rgba(110, 130, 96));
    case Monster::Blightspawn: return ramp(rgba(96, 80, 74));
    case Monster::Goblin: return ramp(rgba(120, 150, 70));
    case Monster::Spider: return ramp(rgba(70, 60, 70));
    case Monster::FrostSpider: return ramp(rgba(170, 200, 220));
    case Monster::Mudcrab: return ramp(rgba(150, 110, 80));
    case Monster::Scorpion: return ramp(rgba(170, 120, 60));
    case Monster::Lurker: return ramp(rgba(84, 110, 72));
    case Monster::Sandworm: return ramp(rgba(196, 160, 110));
    case Monster::Dragon: return ramp(rgba(170, 50, 46));
    case Monster::Bat: return ramp(rgba(80, 66, 76));
    case Monster::Harpy: return ramp(rgba(150, 120, 90));
    case Monster::Slime: return ramp(rgba(90, 190, 110));
    case Monster::Wisp: return ramp(rgba(170, 230, 240));
    case Monster::Golem: return ramp(rgba(130, 130, 140));
    default: return kBone;
  }
}
}  // namespace

Canvas trophySprite(Monster m, uint32_t variant) {
  Canvas c(TROPHY_W, TROPHY_H);
  // the plaque: a carved shield-shaped board, lit top-left, a bevelled rim
  const Ramp& B = (variant & 1) ? kWoodDark : kWood;
  for (int y = 3; y <= 19; y++)
    for (int x = 4; x <= 15; x++) {
      const float half = y < 14 ? 6.0f : 6.0f - (y - 13) * 1.0f;
      if (std::fabs(x + 0.5f - 10.0f) > half) continue;
      int k = 2;
      const bool rim = std::fabs(x + 0.5f - 10.0f) > half - 1.2f || y == 3;
      if (rim) k = (x < 10 || y == 3) ? 4 : 1;
      else if (x < 8) k = 3;
      c.set(x, y, B[k]);
    }
  const Ramp F = trophyFur(m);
  const TrophyHead t = trophyOf(m);
  const float cx = 10.0f, cy = 11.0f;
  switch (t) {
    case kTCanine: {   // a wolf's head: the skull dome, the snout out toward the camera, pricked ears, bared teeth
      ball(c, cx, cy - 1, 4.2f, 3.8f, F, 0.06f);
      ball(c, cx, cy + 3, 2.4f, 2.6f, F, 0.06f);
      c.set((int)cx - 1, (int)cy + 5, kInk); c.set((int)cx, (int)cy + 5, kInk);
      for (int s = -1; s <= 1; s += 2) { c.set((int)cx + s * 3, (int)cy - 5, F[s < 0 ? 3 : 2]); c.set((int)cx + s * 3, (int)cy - 6, F[s < 0 ? 4 : 2]); c.set((int)cx + s * 2, (int)cy - 5, F[2]); }
      c.set((int)cx - 2, (int)cy - 1, m == Monster::EmberHound ? kFire[3] : kEye); c.set((int)cx + 2, (int)cy - 1, m == Monster::EmberHound ? kFire[3] : kEye);
      c.set((int)cx - 1, (int)cy + 6, kBone[4]); c.set((int)cx + 1, (int)cy + 6, kBone[3]);
      if (m == Monster::Hyena) for (int i = 0; i < 4; i++) c.set((int)cx - 3 + i * 2, (int)cy - 2 + (i & 1), F[0]);
      break;
    }
    case kTBoar: {
      ball(c, cx, cy, 4.6f, 4.2f, F, 0.06f);
      ball(c, cx, cy + 4, 2.4f, 1.8f, ramp(rgba(190, 130, 120)), 0.05f);
      c.set((int)cx - 1, (int)cy + 4, F[0]); c.set((int)cx + 1, (int)cy + 4, F[0]);
      for (int s = -1; s <= 1; s += 2) { c.set((int)cx + s * 3, (int)cy + 4, kBone[4]); c.set((int)cx + s * 4, (int)cy + 3, kBone[3]); c.set((int)cx + s * 4, (int)cy + 2, kBone[2]); }
      c.set((int)cx - 2, (int)cy - 1, kEye); c.set((int)cx + 2, (int)cy - 1, kEye);
      for (int s = -1; s <= 1; s += 2) c.set((int)cx + s * 4, (int)cy - 4, F[1]);
      for (int y = (int)cy - 5; y <= (int)cy - 3; y++) c.set((int)cx, y, F[0]);   // the bristly crest
      break;
    }
    case kTBear: {
      ball(c, cx, cy, 5.0f, 4.4f, F, 0.06f);
      ball(c, cx, cy + 3, 2.4f, 2.0f, m == Monster::Yeti ? ramp(rgba(150, 160, 180)) : ramp(rgba(170, 130, 96)), 0.05f);
      c.set((int)cx - 1, (int)cy + 2, kInk); c.set((int)cx, (int)cy + 2, kInk);
      for (int s = -1; s <= 1; s += 2) ball(c, cx + s * 4.0f, cy - 4.0f, 1.5f, 1.4f, F, 0.05f, s > 0 ? -1 : 0);
      c.set((int)cx - 2, (int)cy - 1, kEye); c.set((int)cx + 2, (int)cy - 1, kEye);
      c.set((int)cx - 1, (int)cy + 5, kBone[4]); c.set((int)cx + 1, (int)cy + 5, kBone[3]);
      break;
    }
    case kTSkull: {   // a skull (the draugr's in a rusted helm, the wraith's with a cold glow)
      ball(c, cx, cy - 1, 4.0f, 4.0f, kBone, 0.05f);
      ball(c, cx, cy + 3, 2.6f, 1.8f, kBone, 0.05f, -1);
      const uint32_t eye = m == Monster::Wraith ? rgba(140, 220, 240) : rgba(40, 28, 36);
      c.set((int)cx - 2, (int)cy, eye); c.set((int)cx - 1, (int)cy, eye); c.set((int)cx + 1, (int)cy, eye); c.set((int)cx + 2, (int)cy, eye);
      c.set((int)cx, (int)cy + 2, rgba(40, 28, 36));
      for (int x = (int)cx - 2; x <= (int)cx + 2; x++) c.set(x, (int)cy + 4, (x & 1) ? kBone[1] : kBone[4]);
      if (m == Monster::Draugr) { for (int x = (int)cx - 4; x <= (int)cx + 4; x++) { c.set(x, (int)cy - 4, kIron[x < cx ? 3 : 1]); c.set(x, (int)cy - 5, kIron[2]); } c.set((int)cx, (int)cy - 3, kIron[2]); c.set((int)cx, (int)cy - 2, kIron[1]); }
      break;
    }
    case kTDragon: {   // the dragon's skull-head and its great swept horns
      for (int s = -1; s <= 1; s += 2) {
        tube(c, V2(cx + s * 3.0f, cy - 3), V2(cx + s * 7.0f, cy - 5), V2(cx + s * 8.5f, cy - 10.5f), 1.3f, 0.5f, kBone, s < 0 ? 3 : 2);
      }
      ball(c, cx, cy - 1, 4.0f, 3.6f, F, 0.06f);
      ball(c, cx, cy + 3.5f, 2.6f, 2.8f, F, 0.06f, -1);
      c.set((int)cx - 2, (int)cy - 1, kGold[4]); c.set((int)cx + 2, (int)cy - 1, kGold[3]);
      c.set((int)cx - 1, (int)cy + 5, kInk); c.set((int)cx + 1, (int)cy + 5, kInk);
      for (int y = (int)cy - 5; y <= (int)cy - 3; y++) c.set((int)cx, y, F[4]);
      break;
    }
    case kTSpider: {   // the spider's head: a cluster of eyes, fangs
      ball(c, cx, cy, 4.2f, 3.6f, F, 0.06f);
      for (int i = 0; i < 6; i++) c.set((int)cx - 2 + (i % 3) * 2 - (i / 3), (int)cy - 2 + (i / 3) * 2, m == Monster::FrostSpider ? rgba(160, 230, 250) : rgba(200, 40, 50));
      tube2(c, V2(cx - 1.5f, cy + 3), V2(cx - 2.5f, cy + 7), 0.8f, 0.4f, kBone, 2);
      tube2(c, V2(cx + 1.5f, cy + 3), V2(cx + 2.5f, cy + 7), 0.8f, 0.4f, kBone, 1);
      break;
    }
    case kTClaw: {   // a great pincer mounted on the board
      tube2(c, V2(cx - 2, cy + 5), V2(cx + 1, cy - 1), 1.6f, 2.0f, F, 2);
      tube(c, V2(cx + 1, cy - 1), V2(cx - 3, cy - 4), V2(cx - 3, cy - 7), 1.6f, 0.5f, F, 3);
      tube(c, V2(cx + 1, cy - 1), V2(cx + 4, cy - 4), V2(cx + 2, cy - 7), 1.4f, 0.5f, F, 2);
      break;
    }
    case kTCroc: {   // a long jaw of teeth
      ball(c, cx, cy - 2, 3.6f, 3.2f, F, 0.06f);
      tube2(c, V2(cx, cy), V2(cx, cy + 7), 2.4f, 1.6f, F, 2);
      for (int y = (int)cy + 1; y <= (int)cy + 6; y += 2) { c.set((int)cx - 2, y, kBone[4]); c.set((int)cx + 2, y, kBone[3]); }
      c.set((int)cx - 2, (int)cy - 3, rgba(220, 190, 60)); c.set((int)cx + 2, (int)cy - 3, rgba(220, 190, 60));
      break;
    }
    case kTOrb: {   // a captured core in a brass cage (a wisp's light, a slime's heart, a golem's rune stone)
      ball(c, cx, cy + 1, 3.6f, 3.6f, F, 0.05f, m == Monster::Golem ? -1 : 0);
      if (m == Monster::Golem) { c.set((int)cx, (int)cy, kGlow[4]); c.set((int)cx - 1, (int)cy + 1, kGlow[3]); c.set((int)cx + 1, (int)cy + 1, kGlow[3]); c.set((int)cx, (int)cy + 2, kGlow[2]); }
      else c.set((int)cx - 1, (int)cy - 1, kWhite);
      for (int y = (int)cy - 3; y <= (int)cy + 5; y++) { c.set((int)cx - 4, y, kBrass[3]); c.set((int)cx + 4, y, kBrass[1]); if ((y & 1) == 0) c.set((int)cx, y, kBrass[2]); }
      for (int x = (int)cx - 4; x <= (int)cx + 4; x++) { c.set(x, (int)cy - 3, kBrass[4]); c.set(x, (int)cy + 5, kBrass[1]); }
      break;
    }
    case kTWings: {   // a pair of wings spread on the board
      for (int s = -1; s <= 1; s += 2)
        for (int i = 0; i < 4; i++)
          tube2(c, V2(cx + s * 1.0f, cy), V2(cx + s * (3.5f + i * 1.0f), cy - 5 + i * 3.0f), 1.0f, 0.5f, F, s < 0 ? 3 : 2);
      ball(c, cx, cy, 1.8f, 2.4f, F, 0.05f);
      break;
    }
    case kTTroll: {   // a troll's craggy head: heavy brow, a great nose, a tusk
      ball(c, cx, cy, 4.8f, 4.6f, F, 0.06f);
      ball(c, cx, cy + 1.5f, 1.6f, 2.2f, F, 0.05f, 1);
      for (int x = (int)cx - 3; x <= (int)cx + 3; x++) c.set(x, (int)cy - 2, F[1]);
      c.set((int)cx - 2, (int)cy - 1, rgba(230, 200, 60)); c.set((int)cx + 2, (int)cy - 1, rgba(230, 200, 60));
      c.set((int)cx - 2, (int)cy + 4, kBone[4]); c.set((int)cx + 2, (int)cy + 4, kBone[3]); c.set((int)cx - 2, (int)cy + 3, kBone[3]);
      if (m == Monster::Blightspawn) for (int i = 0; i < 4; i++) c.set((int)cx - 4 + i * 3, (int)cy - 5 + (i & 1), kBark[1]);
      break;
    }
    case kTGoblin: {
      ball(c, cx, cy, 3.6f, 3.6f, F, 0.06f);
      for (int s = -1; s <= 1; s += 2) tube2(c, V2(cx + s * 3.0f, cy - 1), V2(cx + s * 7.5f, cy - 3.5f), 1.3f, 0.4f, F, s < 0 ? 3 : 2);
      c.set((int)cx - 1, (int)cy - 1, rgba(230, 200, 60)); c.set((int)cx + 1, (int)cy - 1, rgba(230, 200, 60));
      c.set((int)cx, (int)cy + 1, F[1]);
      for (int x = (int)cx - 1; x <= (int)cx + 1; x++) c.set(x, (int)cy + 3, kInk);
      break;
    }
  }
  // the brass plate under the head
  for (int x = 8; x <= 11; x++) { c.set(x, 17, kBrass[x < 10 ? 4 : 2]); c.set(x, 18, kBrass[1]); }
  outline(c, 0.85f);
  return c;
}

// ================================================================ paintings
namespace {
// a soft brush: a short horizontal stroke with a lit start (painterly texture inside the picture)
void stroke(Canvas& c, int x, int y, int len, uint32_t col, uint32_t hi) {
  for (int i = 0; i < len; i++) c.set(x + i, y, i == 0 ? hi : col);
}
}  // namespace

Canvas paintingSprite(const PaintingSpec& s) {
  Canvas c(PAINTING_W, PAINTING_H);
  const uint32_t skyC = s.sky ? opaque(s.sky) : rgba(120, 160, 210);
  const uint32_t landC = s.land ? opaque(s.land) : rgba(80, 130, 70);
  const uint32_t acc = s.accent ? opaque(s.accent) : rgba(200, 80, 60);
  const Ramp Sky = ramp(skyC, 0.6f), Land = ramp(landC, 0.9f), Acc = ramp(acc, 0.9f);
  const int X0 = 3, X1 = PAINTING_W - 4, Y0 = 3, Y1 = PAINTING_H - 4;
  const uint32_t sd = s.seed * 2654435761u + s.kind * 977u;
  const int horizon = Y0 + 9 + (int)(sd % 3) - (s.kind == 2 ? 2 : 0) + (s.kind == 3 ? 1 : 0);
  // the sky: lighter toward the horizon, brush streaks; a sun or moon
  for (int y = Y0; y <= Y1; y++)
    for (int x = X0; x <= X1; x++) {
      const float t = (float)(y - Y0) / std::max(1, horizon - Y0);
      int k = t < 0.35f ? 2 : (t < 0.75f ? 3 : 4);
      if (bayer(x, y) < (t * 3 - std::floor(t * 3)) * 0.5f) k = std::min(4, k + 1);
      c.set(x, y, Sky[k]);
    }
  for (int i = 0; i < 4; i++) stroke(c, X0 + 2 + (int)(hash3(i, 1, sd) % 18), Y0 + 1 + (int)(hash3(i, 2, sd) % 5), 3 + (int)(hash3(i, 3, sd) % 4), Sky[4], kWhite);
  const int sunX = X0 + 4 + (int)(sd % 18);
  c.disc(sunX, Y0 + 3, 1, s.kind == 5 ? mixc(acc, kWhite, 0.4f) : rgba(252, 236, 170));
  // the land below the horizon (overridden by each kind)
  for (int y = horizon; y <= Y1; y++)
    for (int x = X0; x <= X1; x++) {
      const float t = (float)(y - horizon) / std::max(1, Y1 - horizon);
      int k = t < 0.3f ? 3 : (t < 0.7f ? 2 : 1);
      if ((hash3(x / 3, y, sd) % 5) == 0) k = std::min(4, k + 1);
      c.set(x, y, Land[k]);
    }
  switch (s.kind) {
    case 0: {   // a city: walls and roofs on a rise, a tower, its lights
      const Ramp Wl = ramp(mixc(landC, rgba(196, 182, 160), 0.65f), 1.2f);
      const Ramp Rf = ramp(mixc(acc, rgba(160, 70, 50), 0.4f));
      int x = X0 + 1;
      while (x < X1 - 1) {
        const int w = 3 + (int)(hash3(x, 5, sd) % 3), h = 3 + (int)(hash3(x, 6, sd) % 4);
        for (int y = horizon - h; y <= horizon + 1; y++) for (int i = 0; i < w && x + i <= X1; i++) c.set(x + i, y, Wl[i == 0 ? 3 : 2]);
        for (int i = -1; i <= w && x + i <= X1; i++) { c.set(x + i, horizon - h - 1, Rf[2]); }
        c.set(x + w / 2, horizon - h - 2, Rf[3]);
        if (hash3(x, 7, sd) % 2) c.set(x + 1, horizon - h + 2, rgba(250, 220, 120));
        x += w + 1;
      }
      const int tx = X0 + 8 + (int)(sd % 12);
      for (int y = horizon - 10; y <= horizon; y++) { c.set(tx, y, Wl[3]); c.set(tx + 1, y, Wl[1]); }
      c.set(tx, horizon - 11, Rf[3]); c.set(tx + 1, horizon - 11, Rf[2]); c.set(tx, horizon - 12, Rf[4]);
      for (int x2 = X0; x2 <= X1; x2++) c.set(x2, horizon + 2, Wl[1]);   // the town wall
      break;
    }
    case 1: {   // a wonder: a great monument in the accent colour, glowing
      const int mx = X0 + (X1 - X0) / 2 + (int)(sd % 5) - 2, base = horizon + 2;
      const int form = (int)(sd >> 3) % 3;
      for (int y = Y0 + 2; y <= base; y++) {
        const int half = form == 0 ? (y - Y0 - 1) * 2 / 3 : (form == 1 ? 2 + (y > base - 4 ? 3 : 0) : std::max(1, 5 - std::abs(y - (Y0 + 7)) / 2));
        for (int x = mx - half; x <= mx + half; x++) if (x >= X0 && x <= X1) c.set(x, y, Acc[x < mx ? 3 : (x == mx ? 2 : 1)]);
      }
      for (int x = mx - 1; x <= mx + 1; x++) c.set(x, Y0 + 1, kGlow[4]);
      break;
    }
    case 2: {   // a peak: snow-capped mountains lit on the left, foothills
      const Ramp Rk = ramp(mixc(landC, rgba(120, 120, 140), 0.6f));
      for (int m = 0; m < 2; m++) {
        const int px = X0 + 8 + m * 10 + (int)(hash3(m, 1, sd) % 4), pt = Y0 + 1 + m * 3;
        for (int y = pt; y <= horizon + 2; y++) {
          const int half = (y - pt) * 3 / 2;
          for (int x = px - half; x <= px + half; x++) {
            if (x < X0 || x > X1) continue;
            const bool snow = y < pt + 3 + (int)(hash3(x, y, sd) % 2);
            c.set(x, y, snow ? (x <= px ? kWhite : kSnow[2]) : Rk[x < px ? 3 : (m ? 1 : 2)]);
          }
        }
      }
      break;
    }
    case 3: {   // the sea: waves in strokes, a sail, the sun's road on the water
      const Ramp Sea = ramp(mixc(skyC, rgba(40, 90, 150), 0.55f));
      for (int y = horizon; y <= Y1; y++)
        for (int x = X0; x <= X1; x++) {
          int k = y == horizon ? 3 : 2;
          if (((x + y * 3 + (int)sd) % 7) == 0) k = 4;
          if (((x * 2 + y) % 9) == 0) k = 1;
          c.set(x, y, Sea[k]);
        }
      for (int y = horizon + 1; y <= Y1; y += 2) c.set(sunX + ((y / 2) & 1), y, rgba(250, 236, 170));
      const int bx = X0 + 4 + (int)((sd >> 5) % 14), by = horizon + 4;
      for (int x = bx; x <= bx + 4; x++) c.set(x, by, kWood[1]);
      for (int y = by - 5; y < by; y++) for (int x = bx + 1; x <= bx + 1 + (y - (by - 5)) / 2; x++) c.set(x, y, kWhite);
      break;
    }
    case 4: {   // a forest: rows of trees, darker behind, a path into it
      for (int row = 0; row < 3; row++) {
        const Ramp Tr = ramp(mixc(landC, rgba(30, 70, 50), 0.5f - row * 0.15f));
        for (int x = X0 - 1 + row; x <= X1; x += 4) {
          const int ty = horizon - 4 + row * 3 + (int)(hash3(x, row, sd) % 2);
          const bool pine = (hash3(x, row + 7, sd) % 2) != 0;
          for (int y = ty; y <= ty + 6; y++) {
            const int half = pine ? (y - ty) / 2 : (y - ty < 4 ? 2 : 1);
            for (int i = -half; i <= half; i++) if (x + i >= X0 && x + i <= X1) c.set(x + i, y, Tr[i < 0 ? 3 : (i == 0 ? 2 : 1)]);
          }
        }
      }
      for (int y = Y1 - 3; y <= Y1; y++) for (int x = X0 + 12 - (y - Y1 + 3); x <= X0 + 15 + (y - Y1 + 3); x++) c.set(x, y, kEarth[3]);
      break;
    }
    default: {   // a ruin: broken columns and an arch against a dusk sky, ivy
      const Ramp Rn = ramp(mixc(landC, rgba(170, 160, 150), 0.6f));
      for (int k = 0; k < 4; k++) {
        const int x = X0 + 3 + k * 6, top = horizon - 3 - (int)(hash3(k, 1, sd) % 7);
        for (int y = top; y <= horizon + 1; y++) { c.set(x, y, Rn[3]); c.set(x + 1, y, Rn[1]); }
        c.set(x - 1, top, Rn[4]); c.set(x + 2, top, Rn[2]);
        if (k == 1) { for (int i = 0; i < 6; i++) c.set(x + 1 + i, top + (i == 0 || i == 5 ? 1 : 0), Rn[2]); }
        if (hash3(k, 2, sd) % 2) c.set(x, top + 3, kMoss[3]);
      }
      break;
    }
  }
  // the gilt frame: lit on its top and left, a bevel, mitred corners
  for (int y = 0; y < PAINTING_H; y++)
    for (int x = 0; x < PAINTING_W; x++) {
      const int d = std::min(std::min(x, y), std::min(PAINTING_W - 1 - x, PAINTING_H - 1 - y));
      if (d > 2) continue;
      const bool litSide = (y < PAINTING_H - 1 - y && y <= x && y <= PAINTING_W - 1 - x) || (x <= y && x <= PAINTING_H - 1 - y && x < PAINTING_W - 1 - x);
      int k = d == 0 ? (litSide ? 2 : 0) : (d == 1 ? (litSide ? 4 : 1) : (litSide ? 3 : 2));
      c.set(x, y, kGold[k]);
    }
  return c;
}

// ================================================================ the FOR SALE sign
namespace {
const char* glyph(char ch) {   // 3 x 5 capitals for the board
  switch (ch) {
    case 'F': return "####..##.#..#..";
    case 'O': return "####.##.##.####";
    case 'R': return "##.#.###.#.##.#";
    case 'S': return "####..###..####";
    case 'A': return ".#.#.#####.##.#";
    case 'L': return "#..#..#..#..###";
    case 'E': return "####..####..###";
    default: return nullptr;
  }
}
void tinyText(Canvas& c, int x, int y, const char* s, uint32_t col) {
  for (; *s; s++) {
    const char* g = glyph(*s);
    if (g) for (int r = 0; r < 5; r++) for (int k = 0; k < 3; k++) if (g[r * 3 + k] == '#') c.set(x + k, y + r, col);
    x += 4;
  }
}
}  // namespace

int homePropW(Prop) { return 24; }
int homePropH(Prop) { return 32; }
int homePropFrames(Prop) { return 1; }
void paintHomeProp(Canvas& c, Prop p, int frame) {
  (void)frame;
  if (p != Prop::ForSaleSign) return;
  const int g = c.h - 4;
  // the post driven into the ground, a bracket arm, the board hanging on two rings
  for (int y = 4; y <= g; y++) { c.set(4, y, kWood[3]); c.set(5, y, kWood[1]); }
  c.set(4, 3, kWood[4]); c.set(5, 3, kWood[2]);
  for (int x = 4; x <= 21; x++) { c.set(x, 6, kWood[3]); c.set(x, 7, kWood[1]); }
  c.set(6, 8, kWood[2]); c.set(7, 9, kWood[1]); c.set(6, 9, kWood[2]);   // the brace
  for (int s = 0; s < 2; s++) { const int rx = 9 + s * 10; c.set(rx, 8, kIron[3]); c.set(rx, 9, kIron[1]); }
  const int bx0 = 7, bx1 = 22, by0 = 10, by1 = 23;
  for (int y = by0; y <= by1; y++)
    for (int x = bx0; x <= bx1; x++) {
      int k = 3;
      if (x == bx0 || y == by0) k = 4;
      if (x == bx1 || y == by1) k = 1;
      if ((y - by0) % 4 == 0 && y != by0) k = 2;   // the board's planks
      c.set(x, y, kCloth[k]);
    }
  tinyText(c, bx0 + 3, by0 + 2, "FOR", kRed[1]);
  tinyText(c, bx0 + 1, by0 + 8, "SALE", kRed[1]);
  // a stake of grass round the post's foot is the ground's; the post's foot darkens into the earth
  c.set(4, g, kWood[2]); c.set(5, g, kWood[0]);
}

}  // namespace art
