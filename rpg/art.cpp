// EMBERVALE procedural sprite art. See art.h for the contract (sheet layouts, sizes, anchors).
//
// Everything here is painted pixel by pixel from code. The file is organised as:
//   1. colour math, palette ramps and the shared palette
//   2. canvas primitives (lines, ellipses, polygons, shaded spheres, outline pass, noise, dithering)
//   3. humans (and the humanoid monsters that share the rig)
//   4. monsters
//   5. props
//   6. buildings, city walls and the gate
//   7. item icons
//   8. effects
#include "rpg/art.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace art {

// =====================================================================================================
// 1. colour math
// =====================================================================================================

static inline int chR(uint32_t c) { return (int)(c & 255); }
static inline int chG(uint32_t c) { return (int)((c >> 8) & 255); }
static inline int chB(uint32_t c) { return (int)((c >> 16) & 255); }
static inline int chA(uint32_t c) { return (int)((c >> 24) & 255); }
static inline int clamp255(float v) { return v < 0 ? 0 : (v > 255 ? 255 : (int)(v + 0.5f)); }

uint32_t shade(uint32_t c, float k) {
  return rgba(clamp255(chR(c) * k), clamp255(chG(c) * k), clamp255(chB(c) * k), chA(c));
}

uint32_t mix(uint32_t a, uint32_t b, float t) {
  t = t < 0 ? 0 : (t > 1 ? 1 : t);
  return rgba(clamp255(chR(a) + (chR(b) - chR(a)) * t), clamp255(chG(a) + (chG(b) - chG(a)) * t),
              clamp255(chB(a) + (chB(b) - chB(a)) * t), chA(a));
}

namespace {

// ---- palette anchors: shadows drift toward blue-violet, highlights toward warm yellow
const uint32_t kShadowHue = rgba(48, 34, 92);
const uint32_t kLightHue = rgba(255, 238, 168);
const uint32_t kInk = rgba(30, 22, 42);          // darkest "black" in the palette (never pure black)
const uint32_t kEye = rgba(36, 26, 48);
const uint32_t kWhite = rgba(250, 246, 232);     // warm paper white

inline uint32_t withA(uint32_t c, int a) { return (c & 0x00FFFFFFu) | ((uint32_t)(a & 255) << 24); }
inline uint32_t opaque(uint32_t c) { return withA(c, 255); }

// amount 0..1: hue-shifted darkening / lightening
uint32_t darken(uint32_t c, float k) { return mix(shade(c, 1.0f - 0.52f * k), kShadowHue, 0.30f * k); }
uint32_t lighten(uint32_t c, float k) { return mix(shade(c, 1.0f + 0.30f * k), kLightHue, 0.32f * k); }

// 5-step ramp: 0 deepest shadow, 1 shadow, 2 base, 3 light, 4 highlight
struct Ramp {
  uint32_t c[5];
  uint32_t operator[](int i) const { return c[i < 0 ? 0 : (i > 4 ? 4 : i)]; }
};
Ramp ramp(uint32_t base, float spread = 1.0f) {
  Ramp r;
  base = opaque(base);
  r.c[0] = darken(base, 0.95f * spread);
  r.c[1] = darken(base, 0.48f * spread);
  r.c[2] = base;
  r.c[3] = lighten(base, 0.45f * spread);
  r.c[4] = lighten(base, 0.95f * spread);
  return r;
}
// explicit ramp from hand-picked colours
Ramp ramp5(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) { return Ramp{{a, b, c, d, e}}; }

// ---- shared palette ramps (hand tuned so everything sits together)
const Ramp kLeaf = ramp5(rgba(28, 56, 58), rgba(44, 92, 60), rgba(72, 132, 58), rgba(118, 170, 64), rgba(178, 210, 92));
const Ramp kLeafDark = ramp5(rgba(22, 44, 52), rgba(34, 72, 58), rgba(52, 104, 60), rgba(86, 138, 62), rgba(132, 176, 76));
const Ramp kPine = ramp5(rgba(20, 42, 50), rgba(28, 66, 60), rgba(42, 96, 66), rgba(70, 128, 72), rgba(116, 162, 86));
const Ramp kBirchLeaf = ramp5(rgba(46, 82, 52), rgba(78, 124, 56), rgba(122, 166, 64), rgba(170, 200, 82), rgba(220, 230, 120));
const Ramp kAutumn = ramp5(rgba(92, 36, 44), rgba(150, 58, 40), rgba(204, 100, 44), rgba(232, 150, 60), rgba(250, 206, 104));
const Ramp kWillow = ramp5(rgba(30, 64, 60), rgba(52, 104, 68), rgba(88, 146, 70), rgba(136, 182, 82), rgba(190, 216, 112));
const Ramp kBark = ramp5(rgba(46, 30, 40), rgba(74, 46, 42), rgba(106, 70, 50), rgba(140, 98, 64), rgba(170, 128, 84));
const Ramp kWood = ramp5(rgba(62, 36, 38), rgba(100, 60, 44), rgba(142, 92, 56), rgba(182, 128, 74), rgba(214, 170, 108));
const Ramp kWoodDark = ramp5(rgba(40, 26, 34), rgba(64, 40, 40), rgba(90, 58, 46), rgba(120, 82, 58), rgba(150, 110, 74));
const Ramp kStone = ramp5(rgba(50, 50, 72), rgba(82, 84, 104), rgba(118, 120, 134), rgba(158, 160, 164), rgba(200, 200, 192));
const Ramp kStoneWarm = ramp5(rgba(64, 52, 66), rgba(102, 88, 94), rgba(146, 130, 122), rgba(186, 170, 150), rgba(222, 210, 184));
const Ramp kMoss = ramp5(rgba(36, 62, 50), rgba(58, 96, 52), rgba(92, 132, 56), rgba(136, 168, 70), rgba(180, 200, 96));
const Ramp kSnow = ramp5(rgba(120, 138, 178), rgba(162, 184, 214), rgba(206, 222, 238), rgba(234, 242, 248), rgba(255, 255, 255));
const Ramp kIron = ramp5(rgba(44, 46, 62), rgba(76, 80, 98), rgba(116, 122, 138), rgba(162, 168, 180), rgba(214, 220, 226));
const Ramp kGold = ramp5(rgba(98, 54, 34), rgba(160, 98, 34), rgba(214, 156, 46), rgba(244, 204, 82), rgba(255, 244, 168));
const Ramp kBrass = ramp5(rgba(84, 52, 36), rgba(134, 90, 44), rgba(182, 136, 60), rgba(216, 180, 86), rgba(246, 226, 140));
const Ramp kLeather = ramp5(rgba(56, 32, 34), rgba(92, 54, 40), rgba(132, 82, 50), rgba(170, 116, 68), rgba(204, 154, 94));
const Ramp kThatch = ramp5(rgba(92, 62, 42), rgba(140, 100, 50), rgba(186, 142, 66), rgba(218, 184, 94), rgba(240, 216, 140));
const Ramp kFire = ramp5(rgba(150, 34, 34), rgba(222, 76, 32), rgba(250, 146, 44), rgba(255, 210, 86), rgba(255, 248, 196));
const Ramp kWater = ramp5(rgba(34, 50, 98), rgba(46, 86, 142), rgba(66, 132, 184), rgba(112, 184, 214), rgba(196, 236, 244));
const Ramp kBone = ramp5(rgba(92, 78, 82), rgba(150, 136, 124), rgba(204, 192, 166), rgba(232, 224, 196), rgba(252, 248, 228));
const Ramp kCloth = ramp5(rgba(98, 76, 68), rgba(150, 126, 104), rgba(196, 174, 140), rgba(224, 208, 172), rgba(244, 234, 206));
const Ramp kGlow = ramp5(rgba(188, 102, 40), rgba(232, 150, 52), rgba(252, 200, 84), rgba(255, 230, 140), rgba(255, 250, 214));
const Ramp kBrick = ramp5(rgba(74, 36, 42), rgba(116, 54, 46), rgba(156, 80, 58), rgba(188, 112, 76), rgba(214, 150, 104));
const Ramp kSand = ramp5(rgba(124, 92, 70), rgba(172, 134, 90), rgba(212, 178, 116), rgba(234, 208, 146), rgba(248, 232, 186));
const Ramp kCrystal = ramp5(rgba(46, 46, 120), rgba(70, 92, 186), rgba(98, 156, 228), rgba(156, 214, 246), rgba(236, 252, 255));
const Ramp kPurple = ramp5(rgba(40, 24, 56), rgba(68, 38, 86), rgba(104, 60, 120), rgba(148, 96, 160), rgba(196, 150, 200));
const Ramp kRed = ramp5(rgba(76, 22, 40), rgba(124, 32, 42), rgba(176, 50, 46), rgba(214, 90, 62), rgba(244, 150, 100));

// =====================================================================================================
// 2. canvas primitives
// =====================================================================================================

inline bool solid(const Canvas& c, int x, int y) { return chA(c.get(x, y)) != 0; }

// deterministic hashing / noise
inline uint32_t hash3(int x, int y, uint32_t s) {
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + s * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
inline float hashf(int x, int y, uint32_t s) { return (hash3(x, y, s) & 0xFFFFFF) / 16777216.0f; }
float vnoise(float x, float y, uint32_t s) {
  int xi = (int)std::floor(x), yi = (int)std::floor(y);
  float fx = x - xi, fy = y - yi;
  fx = fx * fx * (3 - 2 * fx);
  fy = fy * fy * (3 - 2 * fy);
  float a = hashf(xi, yi, s), b = hashf(xi + 1, yi, s), c = hashf(xi, yi + 1, s), d = hashf(xi + 1, yi + 1, s);
  return lerpf(lerpf(a, b, fx), lerpf(c, d, fx), fy);
}
float fbm(float x, float y, uint32_t s) { return vnoise(x, y, s) * 0.6f + vnoise(x * 2.1f, y * 2.1f, s + 17) * 0.4f; }

// 4x4 ordered dither threshold in (0,1)
inline float bayer(int x, int y) {
  static const int m[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  return (m[y & 3][x & 3] + 0.5f) / 16.0f;
}

inline void dot(Canvas& c, int x, int y, uint32_t col) { c.set(x, y, col); }
// only paints where something already exists (for details layered on a shape)
inline void dotOn(Canvas& c, int x, int y, uint32_t col) { if (solid(c, x, y)) c.set(x, y, col); }
void hline(Canvas& c, int x0, int x1, int y, uint32_t col) { if (x0 > x1) std::swap(x0, x1); for (int x = x0; x <= x1; x++) c.set(x, y, col); }
void vline(Canvas& c, int x, int y0, int y1, uint32_t col) { if (y0 > y1) std::swap(y0, y1); for (int y = y0; y <= y1; y++) c.set(x, y, col); }
void box(Canvas& c, int x0, int y0, int x1, int y1, uint32_t col) {
  if (x0 > x1) std::swap(x0, x1);
  if (y0 > y1) std::swap(y0, y1);
  for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) c.set(x, y, col);
}
// box shaded by a ramp: left column light, right column shadow, bottom row shadow, base inside
void shadedBox(Canvas& c, int x0, int y0, int x1, int y1, const Ramp& r, int base = 2) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      int k = base;
      if (x == x0 && x1 > x0) k = base + 1;
      if (x == x1 && x1 > x0) k = base - 1;
      if (y == y1 && y1 > y0) k = std::min(k, base - 1);
      c.set(x, y, r[k]);
    }
}
void line(Canvas& c, int x0, int y0, int x1, int y1, uint32_t col) {
  int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1, e = dx + dy;
  for (;;) {
    c.set(x0, y0, col);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * e;
    if (e2 >= dy) { e += dy; x0 += sx; }
    if (e2 <= dx) { e += dx; y0 += sy; }
  }
}
// thick line by stamping a small square brush
void thickLine(Canvas& c, float x0, float y0, float x1, float y1, float w, uint32_t col) {
  float L = std::max(1.0f, std::hypot(x1 - x0, y1 - y0));
  int n = (int)std::ceil(L * 2);
  float r = w * 0.5f;
  for (int i = 0; i <= n; i++) {
    float t = (float)i / n, x = lerpf(x0, x1, t), y = lerpf(y0, y1, t);
    for (int yy = (int)std::floor(y - r); yy <= (int)std::ceil(y + r); yy++)
      for (int xx = (int)std::floor(x - r); xx <= (int)std::ceil(x + r); xx++) {
        float ddx = xx + 0.5f - x, ddy = yy + 0.5f - y;
        if (ddx * ddx + ddy * ddy <= r * r + 0.15f) c.set(xx, yy, col);
      }
  }
}
// filled ellipse with float centre/radii (pixel centres tested)
void ellipse(Canvas& c, double cxd, double cyd, double rxd, double ryd, uint32_t col) {
  const float cx = (float)cxd, cy = (float)cyd, rx = (float)rxd, ry = (float)ryd;
  for (int y = (int)std::floor(cy - ry - 1); y <= (int)std::ceil(cy + ry + 1); y++)
    for (int x = (int)std::floor(cx - rx - 1); x <= (int)std::ceil(cx + rx + 1); x++) {
      float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
      if (dx * dx + dy * dy <= 1.0f) c.set(x, y, col);
    }
}
// scanline polygon fill
void poly(Canvas& c, const std::vector<Vec2>& p, uint32_t col) {
  if (p.size() < 3) return;
  float minY = 1e9f, maxY = -1e9f;
  for (auto& v : p) { minY = std::min(minY, v.y); maxY = std::max(maxY, v.y); }
  for (int y = (int)std::floor(minY); y <= (int)std::ceil(maxY); y++) {
    float sy = y + 0.5f;
    std::vector<float> xs;
    for (size_t i = 0; i < p.size(); i++) {
      Vec2 a = p[i], b = p[(i + 1) % p.size()];
      if ((a.y <= sy && b.y > sy) || (b.y <= sy && a.y > sy)) xs.push_back(a.x + (sy - a.y) / (b.y - a.y) * (b.x - a.x));
    }
    std::sort(xs.begin(), xs.end());
    for (size_t i = 0; i + 1 < xs.size(); i += 2)
      for (int x = (int)std::ceil(xs[i] - 0.5f); x <= (int)std::floor(xs[i + 1] - 0.5f); x++) c.set(x, y, col);
  }
}

// light comes from the top-left, slightly toward the viewer
constexpr float kLx = -0.56f, kLy = -0.62f, kLz = 0.55f;
inline float lightAt(float nx, float ny) {
  float nz = std::sqrt(std::max(0.0f, 1.0f - nx * nx - ny * ny));
  return nx * kLx + ny * kLy + nz * kLz;
}
// map a light value (-1..1) to a ramp index; a little dither softens the band edges
inline int lightIndex(float l, int x, int y, float dither = 0.10f) {
  l += (bayer(x, y) - 0.5f) * dither;
  if (l > 0.86f) return 4;
  if (l > 0.62f) return 3;
  if (l > 0.16f) return 2;
  if (l > -0.24f) return 1;
  return 0;
}
// shaded ellipsoid: classic pixel sphere with highlight top-left and bounce-free core shadow
void ball(Canvas& c, double cxd, double cyd, double rxd, double ryd, const Ramp& r, float dither = 0.10f, int bias = 0) {
  const float cx = (float)cxd, cy = (float)cyd, rx = (float)rxd, ry = (float)ryd;
  for (int y = (int)std::floor(cy - ry - 1); y <= (int)std::ceil(cy + ry + 1); y++)
    for (int x = (int)std::floor(cx - rx - 1); x <= (int)std::ceil(cx + rx + 1); x++) {
      float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
      float d2 = dx * dx + dy * dy;
      if (d2 > 1.0f) continue;
      c.set(x, y, r[lightIndex(lightAt(dx * 0.92f, dy * 0.92f), x, y, dither) + bias]);
    }
}
// vertical cylinder shading across x (left lit)
void cylinder(Canvas& c, int x0, int y0, int x1, int y1, const Ramp& r, int bias = 0) {
  int w = x1 - x0 + 1;
  for (int x = x0; x <= x1; x++) {
    float t = (x - x0 + 0.5f) / w * 2 - 1;  // -1..1
    int k = lightIndex(lightAt(t * 0.95f, 0), x, 0, 0) + bias;
    for (int y = y0; y <= y1; y++) c.set(x, y, r[k]);
  }
}

// alpha-over composite of src onto dst at (x,y)
void blit(Canvas& dst, const Canvas& src, int x, int y, bool flip = false) {
  for (int j = 0; j < src.h; j++)
    for (int i = 0; i < src.w; i++) {
      uint32_t p = src.get(flip ? src.w - 1 - i : i, j);
      int a = chA(p);
      if (!a) continue;
      if (a == 255) { dst.set(x + i, y + j, p); continue; }
      uint32_t d = dst.get(x + i, y + j);
      if (!chA(d)) { dst.set(x + i, y + j, p); continue; }
      uint32_t m = mix(d, p, a / 255.0f);
      dst.set(x + i, y + j, withA(m, std::max(a, chA(d))));
    }
}

// outline colour for an edge pixel next to colour n: a deep, hue-shifted version of the neighbour
inline uint32_t outlineOf(uint32_t n, float strength) {
  uint32_t o = mix(shade(opaque(n), 0.36f), kInk, 0.45f);
  return mix(opaque(n), o, strength);
}
// 1px outline around everything opaque. Uses the neighbour's own colour (selective outline), with the
// upper-left edge a touch lighter than the lower-right so silhouettes read as lit from the top-left.
void outline(Canvas& c, float strength = 1.0f) {
  Canvas src = c;
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      if (solid(src, x, y)) continue;
      // prefer the neighbour below/right (their colours are darker), fall back to above/left
      static const int nx[4] = {0, -1, 1, 0}, ny[4] = {1, 0, 0, -1};
      int found = -1;
      for (int k = 0; k < 4; k++)
        if (chA(src.get(x + nx[k], y + ny[k])) > 96) { found = k; break; }
      if (found < 0) continue;
      uint32_t n = src.get(x + nx[found], y + ny[found]);
      float s = strength;
      if (found == 0 || found == 2) s *= 0.88f;  // pixel above/left of shape: lit side
      c.set(x, y, outlineOf(n, s));
    }
}
// darkens the inner edge of a shape against transparency (used for a soft rim instead of a full outline)
void innerRim(Canvas& c, float k) {
  Canvas src = c;
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      if (!solid(src, x, y)) continue;
      if (!solid(src, x + 1, y) || !solid(src, x, y + 1)) c.set(x, y, darken(src.get(x, y), k));
    }
}
// recolour every opaque pixel by mapping its luminance onto a ramp (used for palette swaps)
inline float luma(uint32_t c) { return (chR(c) * 0.3f + chG(c) * 0.55f + chB(c) * 0.15f) / 255.0f; }

// copies a frame into a sheet cell
void place(Canvas& sheet, const Canvas& cell, int col, int row) {
  for (int y = 0; y < cell.h; y++)
    for (int x = 0; x < cell.w; x++) sheet.set(col * cell.w + x, row * cell.h + y, cell.get(x, y));
}

}  // namespace

// =====================================================================================================
// 3. humans
// =====================================================================================================
namespace {

enum Facing { kDown = 0, kUp = 1, kSide = 2 };

// Per-frame body pose. Front/back views use A/B = screen left/right; the side view uses A = near, B = far.
struct Pose {
  int bob = 0, lean = 0;
  int liftA = 0, liftB = 0;    // foot lift (px)
  int stepA = 0, stepB = 0;    // side view: foot x offset from the hip
  int swingA = 0, swingB = 0;  // front/back: hand y offset; side: hand x offset
  int atk = 0;                 // 1 wind-up, 2 strike
  bool hurt = false;
};

Pose humanPose(int facing, int frame) {
  Pose p;
  if (frame >= 1 && frame <= 4) {
    int i = frame - 1;
    if (facing == kSide) {
      static const int sN[4] = {3, 0, -3, 1}, sF[4] = {-3, 1, 3, 0};
      static const int lN[4] = {0, 0, 0, 2}, lF[4] = {0, 2, 0, 0};
      static const int bob[4] = {0, -1, 0, -1}, hN[4] = {-2, 0, 2, 0};
      p.stepA = sN[i]; p.stepB = sF[i]; p.liftA = lN[i]; p.liftB = lF[i];
      p.bob = bob[i]; p.swingA = hN[i]; p.swingB = -hN[i];
    } else {
      static const int lL[4] = {2, 0, 0, 1}, lR[4] = {0, 1, 2, 0};
      static const int bob[4] = {-1, 0, -1, 0}, sL[4] = {-1, -1, 1, 1};
      p.liftA = lL[i]; p.liftB = lR[i]; p.bob = bob[i]; p.swingA = sL[i]; p.swingB = -sL[i];
    }
  } else if (frame == 5) {
    p.atk = 1;
    if (facing == kSide) { p.lean = -1; p.stepA = 2; p.stepB = -2; }
  } else if (frame == 6) {
    p.atk = 2;
    if (facing == kSide) { p.lean = 1; p.stepA = 3; p.stepB = -3; }
  } else if (frame == 7) {
    p.hurt = true;
    p.lean = -1;
    p.swingA = -1; p.swingB = -1;
  }
  return p;
}

// ---- head overlay maps. 12 columns starting at x = 2, rows starting at y = hy - 3 (hy = head top).
// digits = ramp index (0 darkest .. 4 highlight), 'e' = eye, 'g' = accent, 'v' = visor, 'b' = leather tie.
using Map = const char* const*;
constexpr int kMapRows = 17;

const char* const kSkinDown[kMapRows] = {
  nullptr, nullptr, nullptr,
  "...333332...",
  "..33333322..",
  "..33222221..",
  "..32222221..",
  "..32e22e21..",
  "..22e22e21..",
  "..12222211..",
  "...111111...",
};
const char* const kSkinUp[kMapRows] = {
  nullptr, nullptr, nullptr,
  "...333221...",
  "..33222211..",
  "..32222211..",
  "..22222211..",
  "..22222211..",
  "..22222111..",
  "..12221111..",
  "...111111...",
};
const char* const kSkinSide[kMapRows] = {
  nullptr, nullptr, nullptr,
  "...333322...",
  "..33333222..",
  "..33322222..",
  "..32222222..",
  "..322122e2..",
  "..222122e22.",
  "..12222222..",
  "...1122221..",
};

const char* const kHairShortD[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443332..",
  "..23333221..",
  "..22.2212.1.",
  "..1......1..",
};
const char* const kHairShortU[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443321..",
  "..23333221..",
  "..23322211..",
  "..22222211..",
  "..22221111..",
  "..12211110..",
  "...111110...",
};
const char* const kHairShortS[kMapRows] = {
  nullptr, nullptr,
  "...23332....",
  "..2344332...",
  "..23333321..",
  "..2332221.1.",
  "..2221......",
  "..221.......",
  "..21........",
  "...1........",
};
const char* const kHairLongD[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443332..",
  ".2233332211.",
  ".232.2212.1.",
  ".22......11.",
  ".22......11.",
  ".21......10.",
  ".21......10.",
  ".11......10.",
  ".1........0.",
};
const char* const kHairLongU[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443321..",
  ".2233332211.",
  ".2333222211.",
  ".2322222111.",
  ".2222221111.",
  ".2222211110.",
  ".1222111110.",
  "..12211110..",
  "..11111100..",
  "...1.1.0....",
};
const char* const kHairLongS[kMapRows] = {
  nullptr, nullptr,
  "...23332....",
  "..2344332...",
  ".223333321..",
  ".22332221.1.",
  ".22221......",
  ".2221.......",
  ".2221.......",
  ".2211.......",
  ".1211.......",
  ".111........",
  "..1.........",
};
const char* const kHairPonyD[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443332..",
  "..23333221..",
  "..22.2212.11",
  "..1......111",
  ".........11.",
  ".........11.",
  "..........1.",
};
const char* const kHairPonyU[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443321..",
  "..23333221..",
  "..23322211..",
  "..22222211..",
  "..222bb111..",
  "..12233110..",
  "...123210...",
  ".....221....",
  ".....221....",
  ".....211....",
  "......1.....",
};
const char* const kHairPonyS[kMapRows] = {
  nullptr, nullptr,
  "...23332....",
  "..2344332...",
  "..23333321..",
  ".b2332221.1.",
  "1232221.....",
  "2221221.....",
  "221.21......",
  "21...1......",
  "1...........",
};
const char* const kHairMohawkD[kMapRows] = {
  ".....43.....",
  "....3432....",
  "....3321....",
  "....3221....",
  ".....21.....",
};
const char* const kHairMohawkU[kMapRows] = {
  ".....43.....",
  "....3432....",
  "....3321....",
  "....3221....",
  "....3221....",
  "....2211....",
  "....2211....",
  ".....11.....",
};
const char* const kHairMohawkS[kMapRows] = {
  nullptr,
  "......443...",
  "...3443321..",
  "..2332221...",
  "..21........",
};
const char* const kHairBraidD[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443332..",
  "..23332221..",
  "..2232.211..",
  "..3......2..",
  "..1......1..",
  "..3......2..",
  "..1......1..",
  "..3......2..",
  "..1......1..",
  "..b......b..",
};
const char* const kHairBraidU[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443321..",
  "..23333221..",
  "..23322211..",
  "..22222211..",
  "..22221111..",
  "..12211110..",
  "...321120...",
  "...2...1....",
  "...3...2....",
  "...1...1....",
  "...b...b....",
};
const char* const kHairBraidS[kMapRows] = {
  nullptr, nullptr,
  "...23332....",
  "..2344332...",
  "..23333321..",
  "..2332221.1.",
  "..2221......",
  "..221.......",
  "..213.......",
  "...21.......",
  "...31.......",
  "...21.......",
  "...b........",
};

// cloth hood, face opening left free
const char* const kHoodD[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443332..",
  ".2334333221.",
  ".2310000011.",
  ".22......11.",
  ".22......11.",
  ".21......10.",
  ".221....110.",
  "..22111110..",
};
const char* const kHoodU[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443321..",
  ".2334332211.",
  ".2333222111.",
  ".2332222111.",
  ".2322221110.",
  ".2222211110.",
  ".1222111110.",
  "..12211110..",
  "...1111.....",
};
const char* const kHoodS[kMapRows] = {
  nullptr, nullptr,
  "...23332....",
  "..2344332...",
  ".233333321..",
  ".2333322001.",
  ".233321.....",
  ".23321......",
  ".22221......",
  ".122211.....",
  "..11111.....",
};

const char* const kBeardD[kMapRows] = {
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  "..2......1..",
  "..23322211..",
  "..23222111..",
  "...222110...",
  "....2110....",
};
const char* const kBeardS[kMapRows] = {
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  ".....2......",
  ".....22232..",
  "......32221.",
  "......2211..",
  ".......10...",
};

const char* const kCapD[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443332..",
  "..23333221..",
  ".1222222211.",
};
const char* const kCapU[kMapRows] = {
  nullptr, nullptr,
  "...233321...",
  "..23443321..",
  "..23333221..",
  "..23222211..",
  ".1222222211.",
};
const char* const kCapS[kMapRows] = {
  nullptr, nullptr,
  "...23332....",
  "..2344332...",
  "..23333321..",
  ".122222221..",
};
const char* const kIronHelmD[kMapRows] = {
  nullptr,
  "....2332....",
  "...234432...",
  "..23444332..",
  "..23333221..",
  ".1221221211.",
  "..1..21..1..",
  ".....21.....",
};
const char* const kIronHelmU[kMapRows] = {
  nullptr,
  "....2332....",
  "...234432...",
  "..23444322..",
  "..23333221..",
  "..23322211..",
  ".1222222111.",
  "..1......1..",
};
const char* const kIronHelmS[kMapRows] = {
  nullptr,
  "....233.....",
  "...23443....",
  "..2344332...",
  "..23333321..",
  ".122222221..",
  "..2211...2..",
  "..211....1..",
};
const char* const kGreatHelmD[kMapRows] = {
  nullptr,
  "....3442....",
  "...344332...",
  "..34443322..",
  "..33433221..",
  "..33322221..",
  "..3vvvvvv1..",
  "..32212211..",
  "..32212211..",
  "..21111110..",
  "...111110...",
};
const char* const kGreatHelmU[kMapRows] = {
  nullptr,
  "....3442....",
  "...344332...",
  "..34443322..",
  "..33433221..",
  "..33322221..",
  "..33222211..",
  "..32222211..",
  "..22222111..",
  "..21111110..",
  "...111110...",
};
const char* const kGreatHelmS[kMapRows] = {
  nullptr,
  "....3442....",
  "...344332...",
  "..34443322..",
  "..33433221..",
  "..333222221.",
  "..3322vvvv1.",
  "..32222212..",
  "..3222221...",
  "..2111110...",
  "...11110....",
};
const char* const kElvenHelmD[kMapRows] = {
  "1..........1",
  "21..2332..12",
  "321234432123",
  ".3234g4332..",
  "..23333221..",
  "..2.2221.1..",
  "..1......1..",
};
const char* const kElvenHelmU[kMapRows] = {
  "1..........1",
  "21..2332..12",
  "321234432123",
  "..23444322..",
  "..23333221..",
  "..23322211..",
  "..22222211..",
  "..1......1..",
};
const char* const kElvenHelmS[kMapRows] = {
  "1...........",
  "21..233.....",
  "3212344g....",
  ".32344332...",
  "..23333321..",
  "..2332221...",
  "..221.......",
  "..21........",
};
const char* const kEbonyHelmD[kMapRows] = {
  ".3........3.",
  ".2........2.",
  ".22.2332.21.",
  "..22344321..",
  "..23444332..",
  "..23333221..",
  "..2vgvvgv1..",
  "..22222211..",
  "..21122110..",
  "...111110...",
};
const char* const kEbonyHelmU[kMapRows] = {
  ".3........3.",
  ".2........2.",
  ".22.2332.21.",
  "..22344321..",
  "..23444322..",
  "..23333221..",
  "..23322211..",
  "..22222211..",
  "..21111110..",
  "...111110...",
};
const char* const kEbonyHelmS[kMapRows] = {
  "..3.........",
  "..2.........",
  "..22233.....",
  "...234432...",
  "..23444332..",
  "..233333221.",
  "..2322vvg1..",
  "..3222221...",
  "..2111110...",
  "...11110....",
};
const char* const kKettleD[kMapRows] = {
  nullptr,
  "....2332....",
  "...234432...",
  "..23444332..",
  "..23333221..",
  "122222222110",
};
const char* const kKettleU[kMapRows] = {
  nullptr,
  "....2332....",
  "...234432...",
  "..23444322..",
  "..23333221..",
  "..23322211..",
  "122222221110",
};
const char* const kKettleS[kMapRows] = {
  nullptr,
  "....233.....",
  "...23443....",
  "..2344332...",
  "..23333321..",
  "12222222210.",
};

void drawMap(Canvas& c, Map m, int ox, int oy, const Ramp& r, uint32_t eye = kEye, uint32_t accent = 0, uint32_t visor = 0) {
  for (int row = 0; row < kMapRows; row++) {
    const char* s = m[row];
    if (!s) continue;
    for (int col = 0; s[col]; col++) {
      char ch = s[col];
      int x = ox + col, y = oy + row;
      if (ch >= '0' && ch <= '4') c.set(x, y, r[ch - '0']);
      else if (ch == 'e') c.set(x, y, eye);
      else if (ch == 'g' && accent) c.set(x, y, accent);
      else if (ch == 'v') c.set(x, y, visor ? visor : r[0]);
      else if (ch == 'b') c.set(x, y, kLeather[1]);
    }
  }
}

const Ramp kSteelArmor = ramp5(rgba(58, 64, 88), rgba(104, 116, 140), rgba(160, 172, 190), rgba(206, 216, 228), rgba(248, 252, 255));
const Ramp kElvenArmor = ramp5(rgba(92, 70, 40), rgba(150, 120, 46), rgba(204, 176, 70), rgba(232, 214, 112), rgba(252, 244, 184));
const Ramp kEbonyArmor = ramp5(rgba(22, 16, 30), rgba(38, 28, 50), rgba(58, 42, 74), rgba(92, 64, 112), rgba(146, 104, 168));
const Ramp kChainMail = ramp5(rgba(46, 48, 64), rgba(84, 88, 104), rgba(128, 132, 146), rgba(170, 174, 184), rgba(214, 218, 222));
const Ramp kRagCloth = ramp5(rgba(52, 44, 46), rgba(78, 70, 60), rgba(108, 98, 74), rgba(136, 126, 92), rgba(164, 154, 116));

struct Rig {
  Ramp skin, hair, top, sleeve, leg, boot, trim, metal, cape, hood, belt;
  uint32_t accent = 0;
};

struct HumanPainter {
  Canvas& c;
  const HumanLook& L;
  Rig R;
  Pose P;
  int facing = kDown;
  int hy = 3, ty = 11, hip = 17;
  static constexpr int kGround = 22;

  HumanPainter(Canvas& c_, const HumanLook& l, int f, const Pose& p) : c(c_), L(l), P(p), facing(f) {
    hy = 3 + P.bob;
    ty = hy + 8;
    hip = ty + 6;
    buildRig();
  }

  bool skirt() const { return L.outfit == Outfit::Dress || L.outfit == Outfit::Robe; }
  bool metalBody() const { return L.outfit == Outfit::Plate || L.outfit == Outfit::Elven || L.outfit == Outfit::Ebony; }
  bool isStaff() const { return L.weapon == 4; }
  bool heavyHead() const { return L.weapon == 2 || L.weapon == 6; }

  void buildRig() {
    R.skin = ramp(L.skin, 0.8f);
    R.hair = ramp(L.hairColor);
    R.top = ramp(L.topColor);
    R.sleeve = R.top;
    R.leg = ramp(L.bottomColor);
    R.boot = ramp5(kLeather[0], kLeather[0], kLeather[1], kLeather[2], kLeather[3]);
    R.trim = ramp(L.tabardColor);
    R.cape = ramp(L.tabardColor);
    R.metal = ramp(L.weaponColor, 1.1f);
    R.belt = kLeather;
    R.hood = L.outfit == Outfit::Robe ? R.top : ramp(shade(L.bottomColor, 0.95f));
    switch (L.outfit) {
      case Outfit::Leather: R.top = kLeather; R.sleeve = R.skin; break;
      case Outfit::Chain: R.top = kChainMail; R.sleeve = kChainMail; R.leg = ramp(shade(L.bottomColor, 0.85f)); break;
      case Outfit::Plate:
        R.top = kSteelArmor; R.sleeve = kSteelArmor; R.leg = kSteelArmor;
        R.boot = ramp5(kSteelArmor[0], kSteelArmor[0], kSteelArmor[1], kSteelArmor[2], kSteelArmor[3]);
        break;
      case Outfit::Elven:
        R.top = kElvenArmor; R.sleeve = kElvenArmor;
        R.leg = ramp5(rgba(30, 56, 50), rgba(46, 86, 60), rgba(70, 120, 70), rgba(108, 156, 84), rgba(156, 192, 110));
        R.boot = ramp5(kElvenArmor[0], kElvenArmor[0], kElvenArmor[1], kElvenArmor[2], kElvenArmor[3]);
        R.accent = rgba(96, 210, 120);
        break;
      case Outfit::Ebony:
        R.top = kEbonyArmor; R.sleeve = kEbonyArmor; R.leg = kEbonyArmor; R.boot = kEbonyArmor;
        R.accent = rgba(220, 60, 120);
        break;
      case Outfit::Guard: R.top = R.trim; R.sleeve = kChainMail; R.leg = ramp(shade(L.bottomColor, 0.85f)); break;
      case Outfit::Rags: R.top = kRagCloth; R.sleeve = R.skin; R.leg = kRagCloth; R.boot = R.skin; break;
      default: break;
    }
  }

  // ---------------------------------------------------------------------- legs
  void legsFront() {
    if (skirt()) return;
    const Ramp& lg = R.leg;
    for (int x = 5; x <= 10; x++) c.set(x, hip, lg[x == 5 ? 3 : (x == 10 ? 1 : 2)]);
    int lifts[2] = {P.liftA, P.liftB};
    bool bare = L.outfit == Outfit::Rags;
    for (int side = 0; side < 2; side++) {
      int x0 = side == 0 ? 5 : 8;
      int foot = kGround - lifts[side];
      for (int y = hip + 1; y <= foot; y++) {
        bool boot = y >= foot - 1;
        bool shin = bare && !boot && y > foot - 4;
        const Ramp& r = boot ? R.boot : (shin ? R.skin : lg);
        int kL = side == 0 ? 3 : 2;
        c.set(x0, y, r[kL]); c.set(x0 + 1, y, r[2]); c.set(x0 + 2, y, r[1]);
        if (boot && y == foot - 1) c.set(x0, y, r[kL + 1]);
      }
      if (side == 0) for (int y = hip + 2; y <= foot; y++) c.set(7, y, (y >= foot - 1 ? R.boot : lg)[0]);
      if (L.outfit == Outfit::Plate || L.outfit == Outfit::Elven) {   // knee plates
        int ky = hip + 2;
        if (ky < foot - 1) { c.set(x0, ky, R.leg[4]); c.set(x0 + 1, ky, R.leg[3]); }
      }
    }
  }

  void legsSide() {
    if (skirt()) return;
    struct Leg { int step, lift, bias; } legs[2] = {{P.stepB, P.liftB, -1}, {P.stepA, P.liftA, 0}};
    for (auto& g : legs) {
      int foot = kGround - g.lift;
      int hipX = 7 + P.lean;
      for (int y = hip; y <= foot; y++) {
        float t = (float)(y - hip) / std::max(1, kGround - hip);
        int cx = hipX + (int)std::lround(g.step * t) - (g.lift > 0 && y > hip + 2 ? 1 : 0);
        bool boot = y >= foot - 1;
        bool shin = L.outfit == Outfit::Rags && !boot && y > foot - 4;
        const Ramp& r = boot ? R.boot : (shin ? R.skin : R.leg);
        c.set(cx, y, r[3 + g.bias]); c.set(cx + 1, y, r[2 + g.bias]); c.set(cx + 2, y, r[1 + g.bias]);
        if (boot && y == foot) c.set(cx + 3, y, r[1 + g.bias]);
      }
    }
  }

  // ---------------------------------------------------------------------- torso
  void torsoFront(bool back) {
    const Ramp& t = R.top;
    static const int colK[6] = {1, 3, 2, 2, 2, 1};
    for (int y = ty; y < hip; y++)
      for (int x = 5; x <= 10; x++) {
        int k = colK[x - 5];
        if (y == ty && x < 10) k = std::min(4, k + 1);
        c.set(x, y, t[k]);
      }
    switch (L.outfit) {
      case Outfit::Tunic:
        if (!back) { c.set(7, ty, R.skin[1]); c.set(8, ty, R.skin[1]); c.set(7, ty + 1, R.top[1]); }
        beltRow(5, 10, ty + 4);
        break;
      case Outfit::Dress:
        if (!back) { c.set(6, ty, R.skin[2]); c.set(7, ty, R.skin[2]); c.set(8, ty, R.skin[1]); }
        for (int x = 5; x <= 10; x++) c.set(x, ty + 3, R.trim[x < 8 ? 2 : 1]);
        break;
      case Outfit::Robe:
        if (!back) {
          for (int y = ty; y < hip; y++) { c.set(7, y, R.trim[3]); c.set(8, y, R.trim[2]); }
          c.set(7, ty, R.trim[4]);
        }
        for (int x = 5; x <= 10; x++) c.set(x, ty + 4, R.trim[x < 8 ? 1 : 0]);
        break;
      case Outfit::Leather:
        for (int x = 5; x <= 10; x++) c.set(x, ty, kLeather[x < 9 ? 3 : 2]);
        if (!back) {
          line(c, 5, ty + 1, 9, ty + 4, kLeather[0]);
          c.set(6, ty + 2, kBrass[4]); c.set(9, ty + 1, kBrass[3]); c.set(8, ty + 3, kBrass[3]);
        } else {
          line(c, 9, ty + 1, 5, ty + 4, kLeather[0]);
        }
        beltRow(5, 10, ty + 4);
        break;
      case Outfit::Chain:
        chainTexture(5, ty, 10, hip - 1);
        beltRow(5, 10, ty + 4);
        break;
      case Outfit::Plate:
        if (!back) { c.set(6, ty + 1, R.top[4]); c.set(6, ty + 2, R.top[4]); c.set(9, ty + 3, R.top[1]); }
        for (int x = 5; x <= 10; x++) c.set(x, ty + 4, R.top[x < 8 ? 1 : 0]);
        break;
      case Outfit::Elven:
        if (!back) { c.set(7, ty + 1, R.accent); c.set(8, ty + 1, shade(R.accent, 0.7f)); c.set(7, ty + 2, shade(R.accent, 0.8f)); }
        for (int x = 5; x <= 10; x++) c.set(x, ty + 4, rgba(70, 120, 70));
        break;
      case Outfit::Ebony:
        if (!back) { c.set(7, ty + 2, R.accent); c.set(8, ty + 2, shade(R.accent, 0.6f)); }
        c.set(6, ty + 1, R.top[4]);
        for (int x = 5; x <= 10; x++) c.set(x, ty + 4, R.top[0]);
        break;
      case Outfit::Guard:
        chainTexture(5, ty, 10, ty);
        c.set(5, ty + 1, R.sleeve[1]); c.set(10, ty + 1, R.sleeve[0]);
        if (!back) { c.set(7, ty + 2, kGold[3]); c.set(8, ty + 2, kGold[2]); c.set(7, ty + 3, kGold[2]); c.set(8, ty + 1, kGold[3]); }
        beltRow(5, 10, ty + 4);
        break;
      case Outfit::Rags:
        if (!back) { c.set(7, ty, R.skin[1]); c.set(8, ty + 1, R.top[0]); c.set(6, ty + 3, R.top[4]); }
        c.set(10, ty + 2, R.top[0]);
        break;
      default: break;
    }
  }

  void torsoSide() {
    const Ramp& t = R.top;
    int ox = P.lean;
    static const int colK[5] = {3, 2, 2, 1, 1};
    for (int y = ty; y < hip; y++)
      for (int x = 6; x <= 10; x++) {
        int k = colK[x - 6];
        if (y == ty && x < 10) k = std::min(4, k + 1);
        c.set(x + ox, y, t[k]);
      }
    switch (L.outfit) {
      case Outfit::Tunic: c.set(10 + ox, ty, R.skin[1]); beltRow(6 + ox, 10 + ox, ty + 4); break;
      case Outfit::Dress:
        c.set(10 + ox, ty, R.skin[2]);
        for (int x = 6; x <= 10; x++) c.set(x + ox, ty + 3, R.trim[x < 9 ? 2 : 1]);
        break;
      case Outfit::Robe:
        for (int y = ty; y < hip; y++) c.set(10 + ox, y, R.trim[2]);
        for (int x = 6; x <= 10; x++) c.set(x + ox, ty + 4, R.trim[1]);
        break;
      case Outfit::Leather:
        for (int x = 6; x <= 10; x++) c.set(x + ox, ty, kLeather[3]);
        c.set(9 + ox, ty + 2, kBrass[4]);
        beltRow(6 + ox, 10 + ox, ty + 4);
        break;
      case Outfit::Chain: chainTexture(6 + ox, ty, 10 + ox, hip - 1); beltRow(6 + ox, 10 + ox, ty + 4); break;
      case Outfit::Plate:
        c.set(9 + ox, ty + 1, R.top[4]);
        for (int x = 6; x <= 10; x++) c.set(x + ox, ty + 4, R.top[1]);
        break;
      case Outfit::Elven:
        c.set(10 + ox, ty + 1, R.accent);
        for (int x = 6; x <= 10; x++) c.set(x + ox, ty + 4, rgba(70, 120, 70));
        break;
      case Outfit::Ebony: c.set(10 + ox, ty + 2, R.accent); c.set(8 + ox, ty + 1, R.top[4]); break;
      case Outfit::Guard:
        chainTexture(6 + ox, ty, 7 + ox, hip - 1);
        c.set(9 + ox, ty + 2, kGold[3]);
        beltRow(6 + ox, 10 + ox, ty + 4);
        break;
      case Outfit::Rags: c.set(10 + ox, ty, R.skin[1]); c.set(7 + ox, ty + 3, R.top[0]); break;
      default: break;
    }
  }

  void beltRow(int x0, int x1, int y) {
    for (int x = x0; x <= x1; x++) c.set(x, y, R.belt[x < x1 - 1 ? 1 : 0]);
    if (facing == kDown) { c.set(7, y, kBrass[3]); c.set(8, y, kBrass[2]); }
    else if (facing == kSide) c.set(x1 - 1, y, kBrass[3]);
  }
  void chainTexture(int x0, int y0, int x1, int y1) {
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) {
        if (!solid(c, x, y)) continue;
        int k = (x == x0) ? 3 : (x == x1 ? 1 : 2);
        if (((x + y) & 1) == 0) k -= 1;
        c.set(x, y, kChainMail[std::max(0, k)]);
      }
  }

  // long skirt (dress / robe), the hem sways with the walk
  void skirtFront() {
    if (!skirt()) return;
    const Ramp& r = R.top;
    int sway = (P.liftA > 0) ? -1 : (P.liftB > 0 ? 1 : 0);
    bool robe = L.outfit == Outfit::Robe;
    int bottom = robe ? kGround : kGround - 1;
    for (int y = hip; y <= bottom; y++) {
      int grow = (y - hip + 1) / 2;
      int x0 = 5 - std::min(2, grow), x1 = 10 + std::min(2, grow);
      if (y >= bottom - 1) { x0 += sway; x1 += sway; }
      for (int x = x0; x <= x1; x++) {
        int k = 2;
        if (x == x0) k = 3;
        else if (x >= x1 - 1) k = 1;
        if ((x == 7 || x == 8) && y > hip + 1 && ((y + x) & 1)) k = 1;
        if (y == bottom) k = std::max(0, k - 1);
        c.set(x, y, r[k]);
      }
      if (robe && facing == kDown) { c.set(7, y, R.trim[3]); c.set(8, y, R.trim[2]); }
    }
    if (robe)
      for (int x = 0; x < 16; x++) if (solid(c, x, bottom)) c.set(x, bottom, R.trim[x < 8 ? 2 : 1]);
    if (!robe) {   // shoes peeking out
      c.set(6, kGround, R.boot[P.liftA ? 1 : 2]); c.set(7, kGround, R.boot[1]);
      c.set(8, kGround, R.boot[1]); c.set(9, kGround, R.boot[P.liftB ? 0 : 1]);
    }
  }
  void skirtSide() {
    if (!skirt()) return;
    const Ramp& r = R.top;
    int sway = P.stepA > 0 ? 1 : (P.stepA < 0 ? -1 : 0);
    bool robe = L.outfit == Outfit::Robe;
    int bottom = robe ? kGround : kGround - 1;
    for (int y = hip; y <= bottom; y++) {
      int grow = (y - hip + 1) / 2;
      int x0 = 6 - std::min(2, grow) + P.lean, x1 = 10 + std::min(2, grow) + P.lean;
      if (y >= bottom - 1) { x0 += sway; x1 += sway; }
      for (int x = x0; x <= x1; x++) {
        int k = x == x0 + 1 ? 3 : (x >= x1 - 1 ? 1 : 2);
        if (y == bottom) k = std::max(0, k - 1);
        c.set(x, y, r[k]);
      }
    }
    if (robe) for (int x = 0; x < 16; x++) if (solid(c, x, bottom)) c.set(x, bottom, R.trim[1]);
    if (!robe) { c.set(9 + sway, kGround, R.boot[1]); c.set(10 + sway, kGround, R.boot[0]); c.set(7 - sway, kGround, R.boot[0]); }
  }

  // ---------------------------------------------------------------------- capes
  void capeBack() {
    if (!L.cape) return;
    int sway = P.liftA > 0 ? -1 : (P.liftB > 0 ? 1 : 0);
    for (int y = ty; y <= kGround - 2; y++) {
      int grow = (y - ty) / 3;
      int x0 = 4 - std::min(1, grow), x1 = 11 + std::min(1, grow);
      if (y > hip) { x0 += sway; x1 += sway; }
      for (int x = x0; x <= x1; x++) {
        int k = 2;
        if (x == x0) k = 3;
        if (x >= x1 - 1) k = 1;
        if ((x == 6 || x == 9) && y > ty + 2) k = 1;
        if (y == kGround - 2) k = std::max(0, k - 1);
        c.set(x, y, R.cape[k]);
      }
    }
    for (int x = 4; x <= 11; x++) c.set(x, ty, R.cape[3]);
  }
  void capeFrontSliver() {
    if (!L.cape) return;
    for (int y = ty + 1; y <= kGround - 3; y++) {
      c.set(2, y, R.cape[1]); c.set(13, y, R.cape[0]);
      if (y > ty + 3) { c.set(3, y, R.cape[2]); c.set(12, y, R.cape[1]); }
    }
    for (int x = 4; x <= 11; x++) for (int y = hip; y <= kGround - 3; y++) c.set(x, y, R.cape[0]);
  }
  void capeSide() {
    if (!L.cape) return;
    int flow = (P.stepA != 0 || P.liftA || P.liftB) ? 1 : 0;
    int ox = P.lean;
    for (int y = ty; y <= kGround - 2; y++) {
      int back = (y - ty) / 3 + flow * ((y - ty) / 4);
      int x0 = 5 - back + ox, x1 = 6 + ox;
      for (int x = x0; x <= x1; x++) c.set(x, y, R.cape[x == x0 ? 1 : 2]);
    }
  }

  // ---------------------------------------------------------------------- arms
  const Ramp& handRamp() const { return metalBody() ? R.sleeve : R.skin; }

  // front / back arm hanging at column x0 (2 wide)
  void armFront(int x0, int swing, bool lit) {
    const Ramp& s = R.sleeve;
    int handY = ty + 5 + swing;
    int kA = lit ? 3 : 2, kB = lit ? 2 : 1;
    for (int y = ty + 1; y < handY; y++) { c.set(x0, y, s[kA]); c.set(x0 + 1, y, s[kB]); }
    c.set(lit ? x0 + 1 : x0, ty, s[kA]);
    if (metalBody()) pauldron(lit ? x0 - 1 : x0, lit);
    if (L.outfit == Outfit::Leather || L.outfit == Outfit::Robe) {
      const Ramp& b = L.outfit == Outfit::Leather ? kLeather : R.trim;
      c.set(x0, handY - 1, b[kA]); c.set(x0 + 1, handY - 1, b[kB]);
    }
    handPx(x0, handY, lit);
  }
  void pauldron(int px, bool lit) {
    const Ramp& s = R.sleeve;
    c.set(px, ty, s[4]); c.set(px + 1, ty, s[3]); c.set(px + 2, ty, s[2]);
    c.set(px, ty + 1, s[2]); c.set(px + 1, ty + 1, s[2]); c.set(px + 2, ty + 1, s[1]);
    if (L.outfit == Outfit::Ebony) c.set(lit ? px : px + 2, ty - 1, s[3]);
  }
  void handPx(int x0, int y, bool lit) {
    const Ramp& h = handRamp();
    c.set(x0, y, h[lit ? 3 : 2]); c.set(x0 + 1, y, h[lit ? 2 : 1]);
  }
  // arm as a 2px stroke from the shoulder to the hand (any view)
  void armLine(int x0, int y0, int x1, int y1, int bias = 0) {
    const Ramp& s = R.sleeve;
    int n = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
    for (int i = 0; i <= n; i++) {
      float t = n ? (float)i / n : 0;
      int x = x0 + (int)std::lround((x1 - x0) * t), y = y0 + (int)std::lround((y1 - y0) * t);
      c.set(x - 1, y, s[3 + bias]); c.set(x, y, s[2 + bias]);
    }
  }
  void handAt(int x, int y, int bias = 0) {
    const Ramp& h = handRamp();
    c.set(x, y, h[3 + bias]); c.set(x - 1, y, h[2 + bias]);
  }
  void armSide(bool nearArm, int swing) {
    int bias = nearArm ? 0 : -1;
    int sx = 8 + P.lean;
    armLine(sx, ty + 1, sx + swing, ty + 4, bias);
    if (nearArm && metalBody()) pauldron(sx - 2, true);
    handAt(sx + swing, ty + 5, bias);
  }

  // ---------------------------------------------------------------------- weapons
  // (hx,hy2) = hand pixel, (dx,dy) = 8-way direction the weapon points, side flips the perpendicular.
  void weapon(int hx, int hy2, int dx, int dy, int side) {
    int w = L.weapon;
    if (w == 0) return;
    const Ramp& m = R.metal;
    int px = -dy, py = dx;
    if (side < 0) { px = -px; py = -py; }
    bool diag = dx != 0 && dy != 0;
    auto put = [&](int t, int o, uint32_t col) { c.set(hx + dx * t + px * o, hy2 + dy * t + py * o, col); };
    // heads on a diagonal: also fill the neighbouring pixel so the shape stays solid (no checkerboard)
    auto head = [&](int t, int o, uint32_t col) {
      put(t, o, col);
      if (diag && o != 0) c.set(hx + dx * t + px * o, hy2 + dy * t + py * o - py * (o > 0 ? 1 : -1), col);
    };
    switch (w) {
      case 1:   // sword
        put(-1, 0, kBrass[3]);
        put(1, 0, kBrass[2]); put(1, 1, kBrass[3]); put(1, -1, kBrass[1]);
        for (int t = 2; t <= 7; t++) {
          put(t, 0, t == 7 ? m[4] : m[3]);
          if (!diag && t < 7) put(t, 1, m[1]);
        }
        if (diag) for (int t = 2; t <= 6; t++) c.set(hx + dx * t + (dx > 0 ? -1 : 1) * (dy > 0 ? 0 : 0), hy2 + dy * t + 1, m[1]);
        break;
      case 5:   // dagger
        put(1, 0, kBrass[2]); put(1, 1, kBrass[3]);
        for (int t = 2; t <= 4; t++) put(t, 0, t == 4 ? m[4] : m[3]);
        if (!diag) { put(2, 1, m[1]); put(3, 1, m[1]); }
        break;
      case 2:   // axe
        for (int t = -1; t <= 6; t++) put(t, 0, kWood[t < 3 ? 2 : 1]);
        head(4, 1, m[1]); head(5, 1, m[1]); head(6, 1, m[0]);
        head(3, 2, m[2]); head(4, 2, m[2]); head(5, 2, m[2]); head(6, 2, m[1]); head(7, 2, m[1]);
        head(3, 3, m[4]); head(4, 3, m[3]); head(5, 3, m[3]); head(6, 3, m[3]); head(7, 3, m[2]);
        put(5, -1, m[1]);
        break;
      case 6:   // hammer / pick
        for (int t = -1; t <= 5; t++) put(t, 0, kWood[t < 3 ? 2 : 1]);
        for (int o = -2; o <= 2; o++) { head(6, o, m[o < 0 ? 2 : 1]); head(7, o, m[o < 0 ? 1 : 0]); }
        head(6, -2, m[3]); head(6, -1, m[3]);
        break;
      case 4: {  // staff with a glowing gem
        for (int t = -7; t <= 6; t++) put(t, 0, kWood[(t & 3) == 0 ? 1 : 2]);
        put(6, 1, kWood[1]); put(6, -1, kWood[1]);
        Ramp gr = ramp(mix(rgba(110, 196, 255), L.weaponColor, 0.2f));
        put(7, 0, gr[3]); put(8, 0, gr[4]); put(7, 1, gr[2]); put(8, 1, gr[2]); put(7, -1, gr[3]); put(8, -1, gr[3]); put(9, 0, gr[2]);
        if (P.atk == 2) { put(10, 0, kWhite); put(8, 2, gr[4]); put(8, -2, gr[4]); }
        break;
      }
      case 3: {  // bow: limbs perpendicular to the aim, string behind
        bool drawn = P.atk == 1;
        Vec2 a = norm(Vec2{(float)dx, (float)dy}), pp = norm(Vec2{(float)-dy, (float)dx});
        Vec2 h{(float)hx, (float)hy2}, e0, e1;
        for (int s = -5; s <= 5; s++) {
          float bulge = 1.6f - s * s / 14.0f;
          Vec2 q = h + pp * (float)s + a * bulge;
          c.set((int)std::lround(q.x), (int)std::lround(q.y), kWood[std::abs(s) > 3 ? 1 : (s < 0 ? 3 : 2)]);
          if (s == -5) e0 = q;
          if (s == 5) e1 = q;
        }
        Vec2 back = h - a * (drawn ? 3.0f : 0.2f);
        int bx = (int)std::lround(back.x), by = (int)std::lround(back.y);
        line(c, (int)std::lround(e0.x), (int)std::lround(e0.y), bx, by, kCloth[3]);
        line(c, bx, by, (int)std::lround(e1.x), (int)std::lround(e1.y), kCloth[3]);
        if (drawn)
          for (int t = -3; t <= 3; t++) {
            Vec2 q = h + a * (float)t;
            c.set((int)std::lround(q.x), (int)std::lround(q.y), t == 3 ? m[4] : kWood[3]);
          }
        break;
      }
      default: break;
    }
  }

  void shield(int x0, int y0, bool edgeOn) {
    if (!L.shield) return;
    const Ramp& rim = R.metal;
    if (edgeOn) {
      for (int y = 0; y < 7; y++) { c.set(x0, y0 + y, rim[2]); c.set(x0 + 1, y0 + y, kWood[y < 3 ? 2 : 1]); }
      c.set(x0, y0, rim[3]); c.set(x0 + 1, y0 + 6, rim[1]);
      return;
    }
    static const char* sh[7] = {".2332.", "234432", "234432", "233321", "233221", "122210", ".1110."};
    for (int y = 0; y < 7; y++)
      for (int x = 0; x < 6; x++) {
        char ch = sh[y][x];
        if (ch == '.') continue;
        int k = ch - '0';
        bool edge = y == 0 || y == 6 || x == 0 || x == 5 || sh[y][x - 1] == '.' || sh[y][x + 1] == '.';
        c.set(x0 + x, y0 + y, edge ? rim[std::max(0, k - 1)] : (x < 3 ? R.trim[k] : kWood[k]));
      }
    c.set(x0 + 2, y0 + 3, rim[4]); c.set(x0 + 3, y0 + 3, rim[2]);
  }

  // ---------------------------------------------------------------------- head
  Map hairMap() const {
    int f = facing;
    switch (L.hair) {
      case Hair::Short: return f == kDown ? kHairShortD : (f == kUp ? kHairShortU : kHairShortS);
      case Hair::Long: return f == kDown ? kHairLongD : (f == kUp ? kHairLongU : kHairLongS);
      case Hair::Ponytail: return f == kDown ? kHairPonyD : (f == kUp ? kHairPonyU : kHairPonyS);
      case Hair::Mohawk: return f == kDown ? kHairMohawkD : (f == kUp ? kHairMohawkU : kHairMohawkS);
      case Hair::Braids: return f == kDown ? kHairBraidD : (f == kUp ? kHairBraidU : kHairBraidS);
      default: return nullptr;
    }
  }
  Map helmetMap(uint32_t& accent, const Ramp*& rp, uint32_t& visor) const {
    int f = facing;
    visor = 0; accent = 0;
    switch (L.outfit) {
      case Outfit::Chain: rp = &kIron; return f == kDown ? kIronHelmD : (f == kUp ? kIronHelmU : kIronHelmS);
      case Outfit::Guard: rp = &kIron; return f == kDown ? kKettleD : (f == kUp ? kKettleU : kKettleS);
      case Outfit::Plate: rp = &kSteelArmor; visor = kInk; return f == kDown ? kGreatHelmD : (f == kUp ? kGreatHelmU : kGreatHelmS);
      case Outfit::Elven: rp = &kElvenArmor; accent = rgba(96, 210, 120); return f == kDown ? kElvenHelmD : (f == kUp ? kElvenHelmU : kElvenHelmS);
      case Outfit::Ebony: rp = &kEbonyArmor; visor = rgba(20, 12, 24); accent = rgba(240, 70, 120); return f == kDown ? kEbonyHelmD : (f == kUp ? kEbonyHelmU : kEbonyHelmS);
      default: rp = &kLeather; return f == kDown ? kCapD : (f == kUp ? kCapU : kCapS);
    }
  }

  void head() {
    int ox = 2 + (facing == kSide ? P.lean : 0), oy = hy - 3;
    Map skin = facing == kDown ? kSkinDown : (facing == kUp ? kSkinUp : kSkinSide);
    drawMap(c, skin, ox, oy, R.skin, kEye);
    if (P.hurt && facing != kUp) {   // squeezed eyes + open mouth
      uint32_t mouth = rgba(120, 40, 56);
      if (facing == kDown) {
        c.set(6, hy + 4, R.skin[2]); c.set(9, hy + 4, R.skin[2]);
        c.set(5, hy + 4, kEye); c.set(6, hy + 5, kEye); c.set(9, hy + 5, kEye); c.set(10, hy + 4, kEye);
        c.set(7, hy + 6, mouth); c.set(8, hy + 6, mouth);
      } else {
        c.set(10 + P.lean, hy + 4, R.skin[2]); c.set(11 + P.lean, hy + 4, kEye); c.set(10 + P.lean, hy + 5, kEye);
        c.set(11 + P.lean, hy + 6, mouth);
      }
    }
    bool covered = L.helmet || L.hood;
    if (L.hair == Hair::Bald && !covered) { c.set(ox + 3, oy + 4, R.skin[4]); c.set(ox + 4, oy + 4, R.skin[4]); }
    if (L.hair == Hair::Mohawk && !covered && facing != kSide)
      for (int y = hy; y < hy + 3; y++) for (int x = 4; x <= 11; x++)
        if (((x + y) & 1) && solid(c, x, y) && (x < 6 || x > 9)) c.set(x, y, mix(c.get(x, y), R.hair[1], 0.4f));
    if (L.beard) {
      if (facing == kDown) drawMap(c, kBeardD, ox, oy, R.hair);
      else if (facing == kSide) drawMap(c, kBeardS, ox, oy, R.hair);
    }
    if (L.helmet) {
      uint32_t accent, visor;
      const Ramp* rp;
      Map hm = helmetMap(accent, rp, visor);
      if (L.hair == Hair::Long || L.hair == Hair::Braids || L.hair == Hair::Ponytail) {   // long hair flows out below
        Canvas tmp(c.w, c.h);
        drawMap(tmp, hairMap(), ox, oy, R.hair);
        for (int y = hy + 4; y < c.h; y++)
          for (int x = 0; x < c.w; x++)
            if (solid(tmp, x, y) && !(facing == kDown && x >= 5 && x <= 10)) c.set(x, y, tmp.get(x, y));
      }
      drawMap(c, hm, ox, oy, *rp, kEye, accent, visor);
    } else if (L.hood) {
      drawMap(c, facing == kDown ? kHoodD : (facing == kUp ? kHoodU : kHoodS), ox, oy, R.hood);
      if (facing == kDown) for (int x = 5; x <= 10; x++) c.set(x, hy + 3, R.skin[0]);
    } else if (Map hm = hairMap()) {
      drawMap(c, hm, ox, oy, R.hair);
    }
  }

  // ---------------------------------------------------------------------- composition
  void paint() {
    if (facing == kDown) paintDown();
    else if (facing == kUp) paintUp();
    else paintSide();
  }

  void paintDown() {
    capeFrontSliver();
    legsFront();
    torsoFront(false);
    skirtFront();
    armFront(11, P.swingB, false);   // off hand (screen right)
    head();
    bool bow = L.weapon == 3;
    if (P.atk == 1) {   // weapon raised high beside the head
      int hx = 3, hy2 = ty - 3;
      armLine(4, ty + 1, 4, hy2 + 1);
      if (bow) weapon(7, ty + 3, 0, 1, 1);
      else weapon(hx, hy2, 0, -1, -1);
      handPx(3, hy2, true);
    } else if (P.atk == 2) {   // swung down across the body
      int hx = 6, hy2 = ty + 4;
      armLine(5, ty + 1, 6, ty + 3);
      if (bow) weapon(7, ty + 4, 0, 1, 1);
      else weapon(hx, hy2, 1, 1, 1);
      handPx(5, hy2, true);
    } else {
      armFront(3, P.swingA, true);
      int hy2 = ty + 5 + P.swingA;
      if (bow) weapon(3, hy2, -1, 0, 1);
      else if (isStaff()) weapon(3, hy2, 0, -1, -1);
      else weapon(3, hy2, 0, 1, heavyHead() ? 1 : -1);
      handPx(3, hy2, true);
    }
    shield(10, ty + 1, false);
  }

  void paintUp() {
    legsFront();
    torsoFront(true);
    skirtFront();
    int hx = 11, hy2 = ty + 5 + P.swingB, dx = 0, dy = isStaff() ? -1 : 1;
    if (L.weapon == 3) { dx = 1; dy = 0; }
    if (P.atk == 1) { hx = 12; hy2 = ty - 2; dx = 0; dy = -1; }
    if (P.atk == 2) { hx = 10; hy2 = ty - 3; dx = -1; dy = -1; if (L.weapon == 3) { hx = 9; dx = 0; } }
    if (P.atk) weapon(hx, hy2, dx, dy, 1);   // raised weapon is beyond the head
    armFront(3, P.swingA, true);
    capeBack();
    if (L.shield) shield(5, ty + 1, false);
    head();
    if (P.atk) {
      armLine(12, ty + 1, hx + 1, hy2 + 1, -1);
      c.set(hx, hy2, handRamp()[2]); c.set(hx + 1, hy2, handRamp()[1]);
    } else {
      armFront(11, P.swingB, false);
      weapon(12, hy2, dx, dy, heavyHead() ? -1 : 1);
      handPx(11, hy2, false);
    }
  }

  void paintSide() {
    capeSide();
    armSide(false, P.atk ? 1 : P.swingB);   // far arm
    legsSide();
    torsoSide();
    skirtSide();
    if (L.shield) shield(11 + P.lean, ty + 1, true);
    head();
    int sx = 8 + P.lean;
    if (P.atk == 1) {
      if (L.weapon == 3) {   // bow drawn: bow arm forward, string hand back at the chest
        weapon(12 + P.lean, ty + 3, 1, 0, 1);
        armLine(sx, ty + 1, 11 + P.lean, ty + 3);
        handAt(9 + P.lean, ty + 3);
      } else {
        int hx = 5 + P.lean, hy2 = ty - 1;
        if (heavyHead()) weapon(hx, hy2, 0, -1, -1);   // axe / hammer hoisted behind the head
        else weapon(hx, hy2, -1, -1, 1);
        armLine(sx, ty + 1, hx + 1, hy2 + 1);
        handAt(hx, hy2);
      }
    } else if (P.atk == 2) {
      int hx = 12 + P.lean, hy2 = ty + 3;
      if (L.weapon == 3) weapon(hx, hy2, 1, 0, 1);
      else if (L.weapon == 5 || L.weapon == 4) weapon(hx, hy2, 1, 0, -1);
      else if (heavyHead()) weapon(hx, hy2, 0, 1, -1);   // chop finished low in front
      else weapon(hx, hy2, 1, 1, -1);
      armLine(sx, ty + 1, hx - 1, hy2);
      handAt(hx, hy2);
    } else {
      int hx = sx + P.swingA, hy2 = ty + 5;
      armSide(true, P.swingA);
      if (L.weapon == 3) weapon(hx, hy2, 1, 0, 1);
      else if (isStaff()) weapon(hx, hy2, 0, -1, -1);
      else if (heavyHead()) weapon(hx, hy2, 0, 1, 1);
      else weapon(hx, hy2, 1, -1, 1);   // blade raised in a ready stance
      handAt(hx, hy2);
    }
  }
};

}  // namespace

Canvas humanSheet(const HumanLook& look) {
  Canvas sheet(HUMAN_W * HUMAN_FRAMES, HUMAN_H * 3);
  for (int row = 0; row < 3; row++)
    for (int f = 0; f < HUMAN_FRAMES; f++) {
      Canvas cell(HUMAN_W, HUMAN_H);
      HumanPainter hp(cell, look, row, humanPose(row, f));
      hp.paint();
      outline(cell);
      place(sheet, cell, f, row);
    }
  return sheet;
}

// =====================================================================================================
// 4. monsters
// =====================================================================================================
namespace {

// shaded tapered capsule from a (radius ra) to b (radius rb): limbs, necks, tails
void capsule(Canvas& c, Vec2 a, Vec2 b, float ra, float rb, const Ramp& r, int bias = 0, float dither = 0.08f) {
  Vec2 ab = b - a;
  float L2 = std::max(1e-4f, len2(ab));
  float maxR = std::max(ra, rb);
  int x0 = (int)std::floor(std::min(a.x, b.x) - maxR - 1), x1 = (int)std::ceil(std::max(a.x, b.x) + maxR + 1);
  int y0 = (int)std::floor(std::min(a.y, b.y) - maxR - 1), y1 = (int)std::ceil(std::max(a.y, b.y) + maxR + 1);
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      Vec2 p{x + 0.5f, y + 0.5f};
      float t = clampf(dot(p - a, ab) / L2, 0, 1);
      Vec2 q = a + ab * t;
      float rr = lerpf(ra, rb, t);
      Vec2 d = p - q;
      float dl = len(d);
      if (dl > rr) continue;
      Vec2 n = rr > 0.01f ? d * (1.0f / rr) : Vec2{};
      c.set(x, y, r[lightIndex(lightAt(n.x * 0.9f, n.y * 0.9f), x, y, dither) + bias]);
    }
}
// ball with a bit of per-pixel noise in the light (fur, scales, rock)
void furBall(Canvas& c, float cx, float cy, float rx, float ry, const Ramp& r, float noise, uint32_t seed, int bias = 0) {
  for (int y = (int)std::floor(cy - ry - 1); y <= (int)std::ceil(cy + ry + 1); y++)
    for (int x = (int)std::floor(cx - rx - 1); x <= (int)std::ceil(cx + rx + 1); x++) {
      float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
      if (dx * dx + dy * dy > 1.0f) continue;
      float l = lightAt(dx * 0.92f, dy * 0.92f) + (hashf(x / 2, y, seed) - 0.5f) * noise;
      c.set(x, y, r[lightIndex(l, x, y, 0.08f) + bias]);
    }
}
inline Vec2 V(double x, double y) { return Vec2{(float)x, (float)y}; }
inline void px(Canvas& c, Vec2 p, uint32_t col) { c.set((int)std::floor(p.x), (int)std::floor(p.y), col); }

// rotate a canvas 90 degrees counter-clockwise (head of a standing figure ends up on the left)
Canvas rotCCW(const Canvas& s) {
  Canvas d(s.h, s.w);
  for (int y = 0; y < s.h; y++)
    for (int x = 0; x < s.w; x++) d.set(y, s.w - 1 - x, s.get(x, y));
  return d;
}
// bounding box of opaque pixels
void bounds(const Canvas& c, int& x0, int& y0, int& x1, int& y1) {
  x0 = c.w; y0 = c.h; x1 = -1; y1 = -1;
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++)
      if (solid(c, x, y)) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); }
}

// Paints a part on its own layer, then composites it with a soft contour wherever it overlaps what is
// already painted. Overlapping limbs and heads stay readable; the outer silhouette is left to outline().
template <class F>
void layered(Canvas& c, F&& draw, float strength = 0.7f) {
  Canvas part(c.w, c.h);
  draw(part);
  static const int nx[4] = {0, -1, 1, 0}, ny[4] = {1, 0, 0, -1};
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      uint32_t p = part.get(x, y);
      if (chA(p)) { c.set(x, y, p); continue; }
      if (!solid(c, x, y)) continue;
      for (int k = 0; k < 4; k++) {
        uint32_t n = part.get(x + nx[k], y + ny[k]);
        if (chA(n) > 96) { c.set(x, y, mix(c.get(x, y), outlineOf(n, 1.0f), strength)); break; }
      }
    }
}

// ------------------------------------------------------------------ quadrupeds (wolf, boar, bear)
struct Quad {
  Ramp fur, belly;
  uint32_t eye, nose;
  float bodyLen, bodyR, legLen, legW, headR, snout;
  int ears;      // 0 pointy, 1 round
  int tail;      // 0 bushy, 1 tuft, 2 stub
  bool tusks = false, mane = false, hump = false, saddle = false;
  uint32_t seed = 1;
};

void quadruped(Canvas& c, const Quad& q, int frame) {
  const int H = c.h;
  const float ground = H - 2.0f;
  const bool dead = frame == 7;
  const float cycle = (float)(frame & 3) * (PI * 0.5f);
  float bx = c.w * 0.44f, by = ground - q.legLen - q.bodyR * 0.55f;
  float headDX = 0, headDY = 0, jaw = 0;
  bool eyeShut = false;
  if (frame < 4) by += (frame & 1) ? -1.0f : 0.0f;
  if (frame == 4) { bx -= 2; by += 1; headDY = 2; headDX = -1; }
  if (frame == 5) { bx += 3; headDX = 2; jaw = 2.5f; }
  if (frame == 6) { bx -= 1; headDY = -2; eyeShut = true; jaw = 1; }
  if (dead) { by = ground - q.bodyR * 0.75f; headDY = q.bodyR * 0.5f + 1; headDX = 1; eyeShut = true; jaw = 1; }

  const Ramp& F = q.fur;
  const float hump = q.hump ? 2.0f : 0.0f;
  const float front = bx + q.bodyLen * 0.5f, back = bx - q.bodyLen * 0.5f;
  struct LegSpec { float ax, phase; bool nearSide, frontLeg; };
  const LegSpec legs[4] = {{back + 1.5f, cycle, false, false}, {front - 1.0f, cycle + PI, false, true},
                           {back + 0.5f, cycle + PI, true, false}, {front - 2.0f, cycle, true, true}};
  auto drawLeg = [&](Canvas& t, const LegSpec& l) {
    int bias = l.nearSide ? 0 : -1;
    Vec2 hip{l.ax, by + q.bodyR * 0.3f};
    if (dead) {
      Vec2 foot{l.ax + q.legLen * 0.9f + (l.nearSide ? 1 : -1), ground - 1 - (l.nearSide ? 0 : 2)};
      capsule(t, hip, foot, q.legW * 0.6f, q.legW * 0.45f, F, bias);
      return;
    }
    float fx = l.ax + std::cos(l.phase) * 2.6f;
    float fy = ground - std::max(0.0f, std::sin(l.phase)) * 2.0f;
    if (frame == 4) fx -= l.frontLeg ? 1 : 2;
    if (frame == 5) fx += l.frontLeg ? 3 : -2;
    if (frame == 4 && l.frontLeg && q.hump) fy -= 3;   // the bear raises a paw
    Vec2 foot{fx, fy};
    Vec2 knee = (hip + foot) * 0.5f + Vec2{l.frontLeg ? -0.6f : 1.2f, 0};
    capsule(t, hip, knee, q.legW * 0.62f, q.legW * 0.5f, F, bias);
    capsule(t, knee, foot, q.legW * 0.5f, q.legW * 0.42f, F, bias);
    int px0 = (int)std::floor(foot.x), py0 = (int)std::floor(foot.y);
    t.set(px0, py0, F[1 + bias]); t.set(px0 + 1, py0, F[1 + bias]);
    if (q.legW >= 3) t.set(px0 - 1, py0, F[1 + bias]);
    if (q.hump || q.tusks) t.set(px0 + 1, py0, q.nose);   // claws / hooves
  };
  // far legs
  drawLeg(c, legs[0]);
  drawLeg(c, legs[1]);
  // tail
  Vec2 tb{back - 0.5f, by - q.bodyR * 0.4f};
  float wag = frame < 4 ? std::sin(cycle) : 0.0f;
  layered(c, [&](Canvas& t) {
    if (q.tail == 0) {
      Vec2 tm = tb + Vec2{-3.0f, 1.0f + wag}, tt = tb + Vec2{-6.0f, 4.0f + wag};
      if (dead) { tm = tb + Vec2{-3, 2}; tt = tb + Vec2{-7, 3}; }
      capsule(t, tb, tm, 1.6f, 2.2f, F, 0);
      capsule(t, tm, tt, 2.2f, 1.0f, F, 0);
      px(t, tt + Vec2{-0.5f, 0.2f}, q.belly[3]);
    } else if (q.tail == 1) {
      Vec2 tt = tb + Vec2{-3.0f, 3.0f + wag * 0.5f};
      capsule(t, tb, tt, 0.8f, 0.6f, F, -1);
      px(t, tt, F[0]);
      px(t, tt + Vec2{0, 1}, F[0]);
    } else {
      ball(t, tb.x, tb.y + 1, 1.6f, 1.4f, F);
    }
  }, 0.5f);
  // body: haunch, barrel, shoulders
  furBall(c, back + q.bodyR * 0.75f, by, q.bodyR * 0.95f, q.bodyR * 0.9f, F, 0.2f, q.seed);
  furBall(c, bx + 0.5f, by + 0.3f, q.bodyLen * 0.5f, q.bodyR * 0.88f, F, 0.2f, q.seed + 1);
  furBall(c, front - q.bodyR * 0.55f, by - hump * 0.5f, q.bodyR, q.bodyR + hump * 0.5f, F, 0.2f, q.seed + 2);
  // lighter underside: the lowest two rows of the body, and a pale chest
  for (int x = (int)(back + 1); x <= (int)(front + 1); x++) {
    int yb = -1;
    for (int y = H - 1; y >= 0; y--) if (solid(c, x, y) && y < by + q.bodyR + 1.5f) { yb = y; break; }
    if (yb > by) { c.set(x, yb, q.belly[1]); c.set(x, yb - 1, q.belly[(x & 1) ? 2 : 1]); }
  }
  if (!q.tusks) ellipse(c, front + 0.5f, by + q.bodyR * 0.25f, q.bodyR * 0.45f, q.bodyR * 0.55f, q.belly[2]);
  // darker saddle along the back (wolf), bristly ridge (boar)
  for (int x = (int)back; x <= (int)front + 1; x++) {
    for (int y = 0; y < H; y++)
      if (solid(c, x, y)) {
        if (q.saddle) for (int k = 1; k <= 2; k++) if (((x + k) & 1) || k == 1) c.set(x, y + k, F[1]);
        if (hash3(x, 0, q.seed) % 3 == 0) c.set(x, y + 1, F[3]);    // fur tufts catching the light
        if (q.mane) { if (x & 1) c.set(x, y - 1, F[1]); c.set(x, y, F[1]); }
        break;
      }
  }
  // near legs
  layered(c, [&](Canvas& t) { drawLeg(t, legs[2]); }, 0.6f);
  layered(c, [&](Canvas& t) { drawLeg(t, legs[3]); }, 0.6f);
  // head (own layer so the jawline and cheek read against the shoulder)
  Vec2 neck{front - 0.5f, by - q.bodyR * 0.45f - hump * 0.6f};
  Vec2 hc{front + q.headR * 0.6f + headDX, by - q.bodyR * 0.75f - hump * 0.4f + headDY};
  if (q.tusks) hc.y += 2.0f;   // the boar carries its head low
  capsule(c, neck, hc, q.bodyR * 0.72f, q.headR * 0.85f, F, 0);
  layered(c, [&](Canvas& t) {
    furBall(t, hc.x, hc.y, q.headR, q.headR * 0.92f, F, 0.15f, q.seed + 3);
    Vec2 sn0 = hc + Vec2{q.headR * 0.5f, q.headR * 0.25f}, sn1 = sn0 + Vec2{q.snout, q.headR * 0.18f};
    if (jaw > 0) {   // open jaw: lower jaw drops, teeth + red mouth
      Vec2 j1 = sn0 + Vec2{q.snout * 0.85f, jaw + 1.0f};
      capsule(t, sn0 + Vec2{0, 1}, j1, q.headR * 0.4f, q.headR * 0.28f, q.belly, -1);
      for (int k = 0; k < (int)q.snout; k++) px(t, sn0 + Vec2{(float)k + 0.5f, q.headR * 0.42f + 0.6f}, rgba(120, 30, 50));
      for (int k = 1; k < (int)q.snout; k += 2) px(t, sn0 + Vec2{(float)k + 0.5f, q.headR * 0.42f}, kWhite);
    }
    capsule(t, sn0, sn1, q.headR * 0.55f, q.headR * 0.42f, F, 0);
    // pale muzzle underside
    for (int k = 0; k <= (int)q.snout; k++) px(t, sn0 + Vec2{(float)k, q.headR * 0.45f}, q.belly[jaw > 0 ? 1 : 3]);
    Vec2 nose = sn1 + Vec2{q.headR * 0.3f, -q.headR * 0.2f};
    px(t, nose, q.nose);
    px(t, nose + Vec2{0, 1}, shade(q.nose, 0.8f));
    px(t, nose + Vec2{-1, -0.2f}, mix(q.nose, F[2], 0.5f));
    if (q.tusks) {
      Vec2 t0 = sn1 + Vec2{-1.0f, q.headR * 0.3f};
      px(t, t0, kBone[3]); px(t, t0 + Vec2{0.6f, -1}, kBone[4]); px(t, t0 + Vec2{1.4f, -1.8f}, kBone[4]);
    }
    Vec2 ear = hc + Vec2{-q.headR * 0.3f, -q.headR * 0.9f};
    if (q.ears == 0) {
      poly(t, {ear + Vec2{-1.8f, 1.5f}, ear + Vec2{0.2f, -3.0f}, ear + Vec2{1.6f, 1.2f}}, F[2]);
      px(t, ear + Vec2{0.0f, -0.2f}, F[0]);
      px(t, ear + Vec2{-0.6f, -1.6f}, F[4]);
    } else {
      ball(t, ear.x, ear.y + 0.6f, 1.6f, 1.5f, F);
      px(t, ear + Vec2{0.1f, 0.6f}, F[0]);
    }
    // eye with a bright glint so it reads on dark fur
    Vec2 e = hc + Vec2{q.headR * 0.35f, -q.headR * 0.2f};
    if (eyeShut) { px(t, e, F[0]); px(t, e + Vec2{-1, 0}, F[0]); }
    else { px(t, e, q.eye); px(t, e + Vec2{-1, 0}, kInk); px(t, e + Vec2{-1, -1}, F[3]); }
    if (dead) { px(t, e + Vec2{0, -1}, F[0]); px(t, e + Vec2{-1, 1}, F[0]); }
  }, 0.65f);
}

// ------------------------------------------------------------------ slime
void slime(Canvas& c, int frame) {
  const Ramp G = ramp5(rgba(34, 82, 70), rgba(52, 132, 76), rgba(94, 186, 82), rgba(150, 222, 104), rgba(226, 250, 186));
  const int W = c.w;
  const float ground = c.h - 1.5f;
  static const float sx[8] = {7.0f, 6.2f, 5.6f, 6.4f, 8.2f, 5.0f, 6.8f, 8.6f};
  static const float sy[8] = {5.2f, 6.0f, 6.8f, 5.8f, 3.8f, 7.6f, 5.4f, 2.2f};
  static const float ox[8] = {0, 0, 0.5f, 0.5f, -1, 2.0f, -1, 0};
  float rx = sx[frame], ry = sy[frame], cx = W * 0.5f + ox[frame], cy = ground - ry;
  // body: dome (flat bottom)
  for (int y = (int)(cy - ry - 1); y <= (int)ground; y++)
    for (int x = (int)(cx - rx - 1); x <= (int)(cx + rx + 1); x++) {
      float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
      if (dy > 0) dy *= 0.55f;   // bottom flattens out
      if (dx * dx + dy * dy > 1) continue;
      float l = lightAt(dx * 0.9f, std::min(0.9f, dy * 0.9f));
      int k = lightIndex(l, x, y, 0.12f);
      if (y >= (int)ground - 1) k = std::max(0, k - 1);
      c.set(x, y, withA(G[k], 236));
    }
  // inner core + bubbles
  if (frame != 7) {
    ellipse(c, cx + 0.5f, cy + ry * 0.35f, rx * 0.45f, ry * 0.3f, withA(G[1], 236));
    c.set((int)(cx - rx * 0.3f), (int)(cy + ry * 0.2f), withA(G[3], 236));
    c.set((int)(cx + rx * 0.45f), (int)(cy + ry * 0.45f), withA(G[3], 236));
  }
  // gloss
  int gx = (int)(cx - rx * 0.5f), gy = (int)(cy - ry * 0.55f);
  c.set(gx, gy, kWhite); c.set(gx + 1, gy, G[4]); c.set(gx, gy + 1, G[4]);
  // face
  int ey = (int)(cy - ry * 0.05f), ex = (int)(cx + rx * 0.15f);
  if (frame == 6 || frame == 7) {
    c.set(ex, ey, kInk); c.set(ex + 3, ey, kInk); c.set(ex + 1, ey + 1, kInk); c.set(ex + 4, ey + 1, kInk);
  } else {
    c.set(ex, ey, kInk); c.set(ex, ey + 1, kInk); c.set(ex + 3, ey, kInk); c.set(ex + 3, ey + 1, kInk);
    c.set(ex, ey, rgba(70, 60, 90));
    if (frame == 5) { c.set(ex + 1, ey + 3, kInk); c.set(ex + 2, ey + 3, kInk); c.set(ex + 1, ey + 4, rgba(150, 40, 60)); }
  }
  if (frame == 7)   // spilled droplets
    { c.set((int)(cx - rx - 2), (int)ground, G[2]); c.set((int)(cx + rx + 2), (int)ground, G[1]); }
}

// ------------------------------------------------------------------ spider
void spider(Canvas& c, int frame, bool frost) {
  const Ramp B = frost ? ramp5(rgba(60, 76, 120), rgba(102, 130, 176), rgba(156, 190, 220), rgba(206, 228, 244), rgba(250, 254, 255))
                       : ramp5(rgba(30, 24, 36), rgba(54, 40, 56), rgba(84, 62, 72), rgba(122, 94, 94), rgba(168, 136, 120));
  const Ramp Lg = frost ? ramp5(rgba(46, 58, 100), rgba(80, 104, 156), rgba(126, 156, 200), rgba(176, 204, 232), rgba(232, 246, 255))
                        : ramp5(rgba(26, 20, 32), rgba(46, 34, 48), rgba(72, 54, 62), rgba(104, 82, 84), rgba(140, 114, 106));
  const uint32_t eyeC = frost ? rgba(120, 230, 255) : rgba(250, 70, 60);
  const float ground = c.h - 2.0f;
  const bool dead = frame == 7;
  float bx = c.w * 0.40f, by = ground - 7.0f;
  if (frame < 4 && (frame & 1)) by -= 1;
  if (frame == 4) { bx -= 2; by -= 1; }
  if (frame == 5) bx += 3;
  if (frame == 6) bx -= 1;
  if (dead) by = ground - 3.5f;
  const Vec2 ceph{bx + 6.0f, by + 0.5f};
  // one leg: hip on the cephalothorax, knee arched high, foot on the ground
  auto leg = [&](Canvas& t, int side, int i) {
    int bias = side == 0 ? -1 : 0;
    float ph = (float)frame * PI * 0.5f + (i & 1) * PI + side * PI;
    Vec2 hip = ceph + Vec2{-2.0f + i * 1.2f, 0.5f};
    float spread = (i - 1.5f) * 4.6f + (side == 0 ? -1.2f : 1.2f);
    float stepX = frame < 4 ? std::cos(ph) * 1.4f : 0.0f;
    float lift = frame < 4 ? std::max(0.0f, std::sin(ph)) * 2.0f : 0.0f;
    Vec2 knee = hip + Vec2{spread * 0.62f + stepX * 0.5f, -6.5f - lift * 0.5f + (side == 0 ? -1.0f : 0.0f)};
    Vec2 foot{hip.x + spread * 1.5f + stepX, ground - lift};
    if (frame == 4 && i == 3) { knee = hip + Vec2{3, -8}; foot = hip + Vec2{7, -6}; }   // front legs raised to strike
    if (frame == 4 && i == 2) { knee = hip + Vec2{2, -7}; foot = hip + Vec2{6, -3}; }
    if (frame == 5 && i == 3) { knee = hip + Vec2{5, -4}; foot = hip + Vec2{9, 1}; }
    if (dead) { knee = hip + Vec2{spread * 0.35f, -4.5f}; foot = knee + Vec2{spread > 0 ? -1.5f : 1.5f, -1.5f}; }
    capsule(t, hip, knee, 0.8f, 0.6f, Lg, bias + 1);
    capsule(t, knee, foot, 0.6f, 0.45f, Lg, bias);
    px(t, knee + Vec2{-0.4f, -0.6f}, Lg[3 + bias]);
    px(t, foot, Lg[0]);
  };
  for (int i = 0; i < 4; i++) leg(c, 0, i);   // far legs behind the body
  // abdomen with a marking, cephalothorax
  furBall(c, bx - 0.5f, by - 1.0f, 5.2f, 4.2f, B, 0.18f, 7);
  const uint32_t mk = frost ? rgba(130, 230, 255) : rgba(214, 74, 52);
  const int ax = (int)bx - 2, ay = (int)by - 4;
  c.set(ax, ay, mk); c.set(ax + 1, ay, mk); c.set(ax, ay + 1, shade(mk, 0.8f)); c.set(ax + 1, ay + 1, shade(mk, 0.7f));
  c.set(ax - 2, ay + 2, shade(mk, 0.75f)); c.set(ax + 3, ay + 2, shade(mk, 0.7f)); c.set(ax, ay + 3, shade(mk, 0.6f));
  if (frost) {   // ice crystals growing on the back
    for (int k = 0; k < 3; k++) {
      int x = ax - 2 + k * 3, y = ay - 2 - (k == 1);
      c.set(x, y, kSnow[4]); c.set(x, y - 1, kSnow[3]); c.set(x + 1, y, kCrystal[3]);
    }
  }
  layered(c, [&](Canvas& t) { furBall(t, ceph.x, ceph.y, 2.8f, 2.4f, B, 0.1f, 8); }, 0.6f);
  for (int i = 0; i < 4; i++) layered(c, [&](Canvas& t) { leg(t, 1, i); }, 0.55f);
  // eyes + fangs
  int ex = (int)(ceph.x + 2), ey = (int)(ceph.y - 1);
  if (dead || frame == 6) { c.set(ex, ey, B[0]); c.set(ex - 1, ey, B[0]); }
  else { c.set(ex, ey, eyeC); c.set(ex - 1, ey, eyeC); c.set(ex, ey - 1, shade(eyeC, 0.7f)); c.set(ex - 2, ey, shade(eyeC, 0.6f)); c.set(ex + 1, ey, kWhite); }
  c.set(ex + 1, ey + 2, kBone[3]); c.set(ex + 2, ey + 2, kBone[2]);
  if (frame == 5) { c.set(ex + 2, ey + 3, kBone[4]); c.set(ex + 3, ey + 3, kBone[2]); }
}

// ------------------------------------------------------------------ bat
void bat(Canvas& c, int frame) {
  const Ramp B = ramp5(rgba(30, 22, 40), rgba(54, 38, 62), rgba(84, 60, 86), rgba(120, 88, 108), rgba(160, 124, 136));
  const Ramp M = ramp5(rgba(44, 26, 48), rgba(74, 40, 66), rgba(108, 60, 84), rgba(144, 86, 100), rgba(180, 120, 120));
  bool dead = frame == 7;
  float cx = c.w * 0.5f, cy = c.h * 0.45f;
  static const float bobs[8] = {0, 1, 2, 1, -1, 2, 0, 0};
  cy += bobs[frame];
  if (frame == 5) cx += 3;
  if (dead) cy = c.h - 4.0f;
  // wing pose: angle of the wing tip (radians up from horizontal)
  static const float wingA[8] = {0.8f, 0.2f, -0.7f, 0.2f, 0.9f, -0.4f, 0.6f, -0.1f};
  float a = wingA[frame];
  auto wing = [&](int dir, int bias) {
    float span = dead ? 6.5f : 9.0f;
    Vec2 root{cx + dir * 1.5f, cy};
    Vec2 tip = root + Vec2{dir * span * std::cos(a), -span * std::sin(a)};
    Vec2 elbow = root + Vec2{dir * span * 0.45f * std::cos(a * 0.7f), -span * 0.55f * std::sin(a) - 1.5f};
    Vec2 lower = root + Vec2{dir * span * 0.55f, 3.0f - std::sin(a) * 1.5f};
    if (dead) { tip = root + Vec2{dir * span, 1}; elbow = root + Vec2{dir * span * 0.5f, -2}; lower = root + Vec2{dir * span * 0.6f, 2}; }
    // membrane with scalloped trailing edge
    poly(c, {root + Vec2{0, -1}, elbow, tip, lower * 0.5f + tip * 0.5f + Vec2{0, 1.5f}, lower, root + Vec2{0, 2}}, M[2 + bias]);
    poly(c, {root, elbow, lower}, M[1 + bias]);
    line(c, (int)root.x, (int)root.y - 1, (int)elbow.x, (int)elbow.y, B[3 + bias]);
    line(c, (int)elbow.x, (int)elbow.y, (int)tip.x, (int)tip.y, B[2 + bias]);
    line(c, (int)elbow.x, (int)elbow.y, (int)(lower.x * 0.5f + tip.x * 0.5f), (int)(lower.y * 0.5f + tip.y * 0.5f + 1), B[1 + bias]);
    px(c, elbow + Vec2{0, -1}, B[4 + bias]);
  };
  wing(-1, -1);
  // body + head
  furBall(c, cx, cy + 1, 3.0f, 3.4f, B, 0.2f, 11);
  furBall(c, cx + 1, cy - 2.5f, 2.6f, 2.3f, B, 0.1f, 12);
  // ears
  c.set((int)cx - 1, (int)cy - 6, B[3]); c.set((int)cx - 1, (int)cy - 5, B[2]);
  c.set((int)cx + 2, (int)cy - 6, B[2]); c.set((int)cx + 2, (int)cy - 5, B[1]);
  wing(1, 0);
  // face
  int ex = (int)cx + 2, ey = (int)cy - 3;
  if (frame == 6 || dead) { c.set(ex, ey, B[0]); c.set(ex - 2, ey, B[0]); }
  else { c.set(ex, ey, rgba(255, 70, 70)); c.set(ex - 2, ey, rgba(220, 50, 60)); }
  c.set(ex - 1, ey + 2, kWhite);
  if (frame == 5) c.set(ex + 1, ey + 2, kWhite);
  // feet
  c.set((int)cx - 1, (int)cy + 5, B[1]); c.set((int)cx + 1, (int)cy + 5, B[1]);
}

// ------------------------------------------------------------------ humanoid monsters via the human rig
enum Species { kHuman = 0, kSkeletonSp, kDraugrSp, kGoblinSp };

// skeleton: draw the posed figure in bones (side view), reusing the human pose numbers
void skeletonFigure(Canvas& c, const Pose& P, int frame) {
  const Ramp& Bn = kBone;
  int hy = 3 + P.bob, ty = hy + 8, hip = ty + 6, ox = P.lean;
  const int ground = 22;
  // far leg + far arm (darker)
  auto leg = [&](int step, int lift, int bias) {
    int foot = ground - lift;
    Vec2 h{7.5f + ox, (float)hip + 0.5f}, f{7.5f + step + 0.0f, (float)foot + 0.5f};
    Vec2 k = (h + f) * 0.5f + Vec2{1.0f, 0};
    capsule(c, h, k, 0.7f, 0.6f, Bn, bias);
    capsule(c, k, f, 0.6f, 0.5f, Bn, bias);
    c.set((int)k.x, (int)k.y, Bn[3 + bias]);
    c.set((int)f.x, foot, Bn[2 + bias]); c.set((int)f.x + 1, foot, Bn[1 + bias]); c.set((int)f.x + 2, foot, Bn[1 + bias]);
  };
  auto arm = [&](Vec2 hand, int bias) {
    Vec2 s{8.0f + ox, (float)ty + 1.5f};
    Vec2 e = (s + hand) * 0.5f + Vec2{-0.8f, 0.6f};
    capsule(c, s, e, 0.6f, 0.55f, Bn, bias);
    capsule(c, e, hand, 0.55f, 0.5f, Bn, bias);
    c.set((int)hand.x, (int)hand.y, Bn[3 + bias]);
  };
  arm(V(8.0f + ox + (P.atk ? 1 : P.swingB), (float)ty + 5.5f), -1);
  leg(P.stepB, P.liftB, -1);
  // spine, ribs, pelvis
  for (int y = ty; y <= hip; y++) c.set(7 + ox, y, Bn[1]);
  for (int r = 0; r < 4; r++) {
    int y = ty + 1 + r;
    int w = r < 3 ? 4 : 3;
    for (int x = 0; x < w; x++) c.set(8 + ox + x - (r == 0 ? 1 : 0), y, (r & 1) ? Bn[2] : Bn[3]);
    c.set(8 + ox + w - 1, y, Bn[1]);
  }
  for (int x = 6; x <= 9; x++) c.set(x + ox, hip, Bn[x < 8 ? 3 : 2]);
  leg(P.stepA, P.liftA, 0);
  // skull
  static const char* skull[8] = {
    "..2333..",
    ".233443.",
    "2333443.",
    "233300 2",
    "23330023",
    ".2233322",
    "..21212.",
    "..1222..",
  };
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++) {
      char ch = skull[y][x];
      if (ch >= '0' && ch <= '4') c.set(4 + ox + x, hy + y, ch == '0' ? kInk : Bn[ch - '0']);
    }
  if (frame != 6) c.set(9 + ox, hy + 4, rgba(255, 90, 60));   // ember in the eye socket
  (void)frame;
}

}  // namespace

// The HumanPainter gets a few species hooks via these helpers (kept outside the class for clarity).
namespace {

Canvas humanoidCell(Species sp, int frame) {
  // the rig paints into 16x24; the monster cell is 24x24 with the figure centred
  HumanLook L;
  Pose P = frame == 7 ? Pose{} : humanPose(kSide, frame + 1);   // monster frames 0-6 = human frames 1-7
  Canvas fig(HUMAN_W, HUMAN_H);
  switch (sp) {
    case kSkeletonSp: {
      L.weapon = 1; L.weaponColor = rgba(150, 128, 104); L.shield = true; L.tabardColor = rgba(110, 80, 60);
      HumanPainter hp(fig, L, kSide, P);
      hp.R.sleeve = kBone; hp.R.skin = kBone;
      skeletonFigure(fig, P, frame);
      if (L.shield) hp.shield(11 + P.lean, hp.ty + 1, true);
      // weapon hand
      int sx = 8 + P.lean;
      if (P.atk == 1) { hp.weapon(5 + P.lean, hp.ty - 1, -1, -1, 1); capsule(fig, V(sx, hp.ty + 1.5f), V(6.0f + P.lean, hp.ty + 0.0f), 0.6f, 0.5f, kBone); }
      else if (P.atk == 2) { hp.weapon(12 + P.lean, hp.ty + 3, 1, 1, -1); capsule(fig, V(sx, hp.ty + 1.5f), V(11.5f + P.lean, hp.ty + 3.5f), 0.6f, 0.5f, kBone); }
      else { hp.weapon(sx + P.swingA, hp.ty + 5, 1, -1, 1); capsule(fig, V(sx, hp.ty + 1.5f), V(sx + P.swingA + 0.5f, hp.ty + 5.5f), 0.6f, 0.5f, kBone); }
      break;
    }
    case kDraugrSp: {
      L.skin = rgba(120, 132, 118); L.hairColor = rgba(170, 170, 160); L.hair = Hair::Long; L.beard = true;
      L.outfit = Outfit::Chain; L.helmet = true; L.bottomColor = rgba(60, 54, 50); L.topColor = rgba(70, 70, 64);
      L.weapon = 2; L.weaponColor = rgba(118, 140, 120);
      HumanPainter hp(fig, L, kSide, P);
      hp.R.top = ramp5(rgba(36, 36, 44), rgba(58, 58, 64), rgba(84, 82, 82), rgba(112, 108, 100), rgba(140, 136, 124));
      hp.R.sleeve = hp.R.top;
      hp.paint();
      // horns on the helm + glowing eyes
      int hy = hp.hy;
      fig.set(4 + P.lean, hy - 1, kBone[3]); fig.set(3 + P.lean, hy - 2, kBone[4]); fig.set(4 + P.lean, hy - 2, kBone[2]);
      fig.set(9 + P.lean, hy - 2, kBone[3]); fig.set(10 + P.lean, hy - 3, kBone[4]);
      fig.set(10 + P.lean, hy + 4, rgba(140, 230, 255)); fig.set(10 + P.lean, hy + 5, rgba(70, 150, 200));
      break;
    }
    case kGoblinSp: {
      L.skin = rgba(124, 160, 72); L.hair = Hair::Mohawk; L.hairColor = rgba(60, 40, 40);
      L.outfit = Outfit::Rags; L.weapon = 5; L.weaponColor = rgba(170, 160, 150);
      HumanPainter hp(fig, L, kSide, P);
      hp.R.top = kLeather; hp.R.leg = kLeather;
      hp.paint();
      int hy = hp.hy, ox = P.lean;
      // big ears swept back, hooked nose, yellow eyes
      Ramp sk = ramp(L.skin, 0.8f);
      fig.set(5 + ox, hy + 3, sk[3]); fig.set(4 + ox, hy + 2, sk[3]); fig.set(3 + ox, hy + 1, sk[2]); fig.set(6 + ox, hy + 4, sk[2]);
      fig.set(5 + ox, hy + 4, sk[1]); fig.set(4 + ox, hy + 3, sk[1]);
      fig.set(12 + ox, hy + 5, sk[2]); fig.set(12 + ox, hy + 6, sk[1]);
      fig.set(10 + ox, hy + 4, rgba(250, 210, 60));
      // shorten: drop rows so the goblin stands ~4px shorter
      Canvas s(HUMAN_W, HUMAN_H);
      int drop[3] = {hp.ty + 2, hp.hip + 1, hp.hip + 3};
      int dy = 0;
      for (int y = HUMAN_H - 1; y >= 0; y--) {
        bool skip = false;
        for (int d : drop) if (y == d) skip = true;
        if (skip) { dy++; continue; }
        for (int x = 0; x < HUMAN_W; x++) s.set(x, y + dy, fig.get(x, y));
      }
      fig = s;
      break;
    }
    default: break;
  }
  Canvas cell(24, 24);
  if (frame == 7) {
    // dead: the figure lies on its back
    if (sp == kSkeletonSp) {
      // a scattered heap of bones
      const Ramp& Bn = kBone;
      capsule(cell, V(5, 21), V(12, 20), 0.7f, 0.7f, Bn);
      capsule(cell, V(9, 22), V(16, 22.5f), 0.7f, 0.7f, Bn);
      capsule(cell, V(14, 20), V(19, 18), 0.6f, 0.6f, Bn, -1);
      for (int r = 0; r < 3; r++) hline(cell, 10, 14, 17 + r * 1, Bn[r & 1 ? 2 : 3]);
      ball(cell, 6, 18, 3.0f, 2.6f, Bn);
      cell.set(6, 18, kInk); cell.set(7, 18, kInk); cell.set(5, 20, Bn[1]); cell.set(7, 20, Bn[1]);
      Canvas sw(HUMAN_W, HUMAN_H);
      HumanLook sl; sl.weapon = 1; sl.weaponColor = rgba(150, 128, 104);
      HumanPainter hp(sw, sl, kSide, Pose{});
      hp.weapon(14, 21, 1, 0, -1);
      blit(cell, sw, 2, 0);
    } else {
      Canvas r = rotCCW(fig);
      int x0, y0, x1, y1;
      bounds(r, x0, y0, x1, y1);
      Canvas t(x1 - x0 + 1, y1 - y0 + 1);
      for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) t.set(x - x0, y - y0, r.get(x, y));
      blit(cell, t, (24 - t.w) / 2, 22 - t.h + 1);
    }
  } else {
    blit(cell, fig, 4, 0);
  }
  return cell;
}

// ------------------------------------------------------------------ troll
void troll(Canvas& c, int frame) {
  // frost troll: shaggy pale fur over slate-dark hide, long knuckle-dragging arms, small low head
  const Ramp F = ramp5(rgba(70, 76, 100), rgba(118, 126, 142), rgba(170, 176, 182), rgba(206, 210, 208), rgba(236, 238, 230));
  const Ramp S = ramp5(rgba(34, 32, 46), rgba(56, 52, 66), rgba(84, 78, 88), rgba(114, 106, 110), rgba(146, 138, 134));
  const float ground = c.h - 2.0f;
  const bool dead = frame == 7;
  float bx = c.w * 0.42f, by = ground - 16.0f;
  if (frame < 4) by += (frame & 1) ? -1.0f : 0.0f;
  if (frame == 4) { bx -= 1; by -= 1; }
  if (frame == 5) { bx += 2; by += 1; }
  if (frame == 6) bx -= 1;
  const float ph = frame * PI * 0.5f;
  if (dead) {
    // sprawled face down
    capsule(c, V(bx - 9, ground - 2), V(bx - 14, ground - 1), 2.2f, 1.8f, S, -1);
    furBall(c, bx - 1, ground - 5, 11.0f, 5.0f, F, 0.35f, 21);
    capsule(c, V(bx + 6, ground - 2), V(bx + 13, ground - 1), 2.2f, 2.0f, F, 0);
    ball(c, bx + 14, ground - 2.5f, 2.6f, 2.0f, S);
    ball(c, bx + 10, ground - 4, 3.6f, 3.0f, S);
    c.set((int)bx + 11, (int)ground - 5, S[0]); c.set((int)bx + 12, (int)ground - 4, kBone[3]);
    return;
  }
  auto legAt = [&](Canvas& t, float phase, int bias) {
    Vec2 h{bx - 2.0f, by + 7.0f};
    Vec2 f{bx - 1.0f + std::cos(phase) * 3.0f, ground - std::max(0.0f, std::sin(phase)) * 2.0f};
    if (frame == 4) f.x -= 2;
    if (frame == 5) f.x += 2;
    Vec2 k = (h + f) * 0.5f + Vec2{2.0f, -0.5f};
    capsule(t, h, k, 3.2f, 2.6f, F, bias);
    capsule(t, k, f, 2.4f, 1.9f, S, bias);
    hline(t, (int)f.x - 2, (int)f.x + 3, (int)f.y, S[1 + bias]);
    t.set((int)f.x + 3, (int)f.y, kBone[2 + bias]);
  };
  auto armAt = [&](Canvas& t, Vec2 hand, int bias) {
    Vec2 s{bx + 4.0f, by - 5.0f};
    Vec2 e = (s + hand) * 0.5f + Vec2{-1.5f, 0.5f};
    capsule(t, s, e, 3.0f, 2.6f, F, bias);
    capsule(t, e, hand, 2.4f, 2.0f, S, bias);
    furBall(t, hand.x, hand.y, 2.8f, 2.5f, S, 0.15f, 22, bias);
    t.set((int)hand.x + 2, (int)hand.y + 2, kBone[3 + bias]); t.set((int)hand.x + 3, (int)hand.y + 1, kBone[2 + bias]);
  };
  Vec2 farHand{bx + 5 - std::cos(ph) * 3.0f, ground - 3.0f};
  Vec2 nearHand{bx + 8 + std::cos(ph) * 3.0f, ground - 3.5f};
  if (frame == 4) { nearHand = V(bx - 5, by - 13); farHand = V(bx + 4, ground - 5); }
  if (frame == 5) { nearHand = V(bx + 15, ground - 2.5f); farHand = V(bx + 7, ground - 5); }
  if (frame == 6) nearHand = V(bx + 3, by - 9);
  armAt(c, farHand, -1);
  legAt(c, ph + PI, -1);
  // hunched torso: belly, chest, shaggy shoulders
  furBall(c, bx, by + 1, 8.5f, 8.5f, F, 0.4f, 23);
  furBall(c, bx + 3.5f, by - 4.0f, 6.5f, 5.5f, F, 0.4f, 24);
  ellipse(c, bx + 3.0f, by + 4.5f, 4.0f, 3.5f, S[2]);       // bare belly hide
  ellipse(c, bx + 2.5f, by + 4.0f, 3.0f, 2.5f, S[3]);
  for (int x = (int)bx - 8; x <= (int)bx + 8; x += 2)        // fur tufts along the back
    for (int y = 0; y < c.h; y++) if (solid(c, x, y)) { c.set(x, y - 1, F[3]); c.set(x + 1, y, F[4]); break; }
  layered(c, [&](Canvas& t) { legAt(t, ph, 0); }, 0.6f);
  // head, low and forward
  Vec2 hc{bx + 9.0f, by - 3.0f + (frame == 6 ? -2.0f : 0.0f) + (frame == 5 ? 2.0f : 0.0f)};
  layered(c, [&](Canvas& t) {
    ball(t, hc.x, hc.y, 4.0f, 3.6f, S);
    ball(t, hc.x + 2.6f, hc.y + 1.4f, 2.4f, 1.9f, S);
    furBall(t, hc.x - 1.0f, hc.y - 2.5f, 3.6f, 1.8f, F, 0.3f, 25);    // fur cap
    hline(t, (int)hc.x, (int)hc.x + 3, (int)hc.y - 2, S[0]);          // heavy brow
    if (frame == 6) t.set((int)hc.x + 2, (int)hc.y - 1, S[0]);
    else { t.set((int)hc.x + 2, (int)hc.y - 1, rgba(255, 196, 70)); t.set((int)hc.x + 1, (int)hc.y - 1, kInk); t.set((int)hc.x - 1, (int)hc.y - 3, rgba(255, 196, 70)); }
    t.set((int)hc.x + 3, (int)hc.y + 2, kBone[4]); t.set((int)hc.x + 3, (int)hc.y + 1, kBone[3]); t.set((int)hc.x + 5, (int)hc.y + 2, kBone[3]);
    hline(t, (int)hc.x + 2, (int)hc.x + 4, (int)hc.y + 3, frame == 5 ? rgba(130, 40, 56) : S[0]);
  }, 0.6f);
  layered(c, [&](Canvas& t) { armAt(t, nearHand, 0); }, 0.65f);
}

// ------------------------------------------------------------------ wraith
void wraith(Canvas& c, int frame) {
  const Ramp K = ramp5(rgba(22, 18, 42), rgba(40, 34, 72), rgba(66, 60, 110), rgba(104, 100, 150), rgba(150, 150, 196));
  bool dead = frame == 7;
  static const float bob[8] = {0, -1, -2, -1, -2, 0, -1, 0};
  float cx = c.w * 0.5f, top = 4.0f + bob[frame];
  if (frame == 5) cx += 3;
  if (frame == 6) cx -= 1;
  if (dead) {
    // a crumpled empty cloak and a fading wisp
    for (int y = 0; y < 6; y++)
      for (int x = -8; x <= 8; x++) {
        float e = (float)(x * x) / 64.0f + (float)((y - 5) * (y - 5)) / 30.0f;
        if (e > 1) continue;
        int k = y < 2 ? 3 : (y < 4 ? 2 : 1);
        if (x > 3) k--;
        c.set((int)cx + x, c.h - 3 - 5 + y, withA(K[k], 220));
      }
    c.set((int)cx - 1, c.h - 12, withA(K[4], 140)); c.set((int)cx, c.h - 14, withA(K[3], 110));
    return;
  }
  // tattered cloak: a bell shape whose hem flutters
  int h = c.h - 6;
  for (int y = 0; y < h; y++) {
    float t = (float)y / h;
    float half = 2.8f + t * 6.0f;
    if (y < 7) half = 3.5f + std::sin(t * PI * 2.2f) * 0.6f;
    for (int x = (int)(cx - half); x <= (int)(cx + half); x++) {
      float fx = (x + 0.5f - cx) / half;
      int k = fx < -0.5f ? 3 : (fx < 0.35f ? 2 : 1);
      if (y < 3) k++;
      // ragged hem
      int tear = (int)(hash3(x, frame & 3, 31) % 4);
      if (y > h - 1 - tear) continue;
      int alpha = 230 - std::max(0, y - (h - 8)) * 18;
      c.set(x, (int)top + y, withA(K[k], alpha));
    }
  }
  // hood opening + eyes
  for (int y = 2; y < 7; y++)
    for (int x = -2; x <= 2; x++)
      if (x * x + (y - 4) * (y - 4) <= 5) c.set((int)cx + 1 + x, (int)top + y, rgba(10, 8, 20));
  uint32_t eye = rgba(120, 240, 220);
  if (frame != 6) { c.set((int)cx + 1, (int)top + 4, eye); c.set((int)cx + 3, (int)top + 4, eye); c.set((int)cx + 2, (int)top + 5, shade(eye, 0.5f)); }
  // skeletal hands
  float ay = top + 10, ax = cx + 5;
  if (frame == 4) { ay -= 5; ax -= 1; }
  if (frame == 5) { ay -= 1; ax += 4; }
  capsule(c, V(cx + 3, top + 8), V(ax, ay), 1.4f, 1.0f, K, 0);
  c.set((int)ax + 1, (int)ay, kBone[3]); c.set((int)ax + 2, (int)ay - 1, kBone[4]); c.set((int)ax + 2, (int)ay + 1, kBone[3]); c.set((int)ax + 1, (int)ay + 2, kBone[2]);
}

// ------------------------------------------------------------------ mudcrab
void mudcrab(Canvas& c, int frame) {
  const Ramp S = ramp5(rgba(60, 36, 40), rgba(104, 60, 48), rgba(150, 92, 60), rgba(190, 132, 80), rgba(226, 178, 118));
  const Ramp Lg = ramp5(rgba(70, 38, 42), rgba(120, 64, 50), rgba(170, 100, 66), rgba(206, 140, 88), rgba(236, 186, 128));
  const float ground = c.h - 2.0f;
  const bool dead = frame == 7;
  float cx = c.w * 0.42f, cy = ground - 4.5f;
  if (frame == 5) cx += 2;
  if (frame == 4) cx -= 1;
  if (frame < 4 && (frame & 1)) cy -= 0.5f;
  if (dead) cy = ground - 3.0f;
  // walking legs, three a side
  auto legs = [&](Canvas& t, int side) {
    int bias = side ? 0 : -1;
    for (int i = 0; i < 3; i++) {
      float ph = frame * PI * 0.5f + i * 2.1f + side * PI;
      float lx = cx - 4.0f + i * 3.2f + (side ? 0.6f : -0.6f);
      Vec2 hip{lx, cy + 1};
      Vec2 knee{lx - 2.0f + (frame < 4 ? std::cos(ph) : 0), cy - 1.0f};
      Vec2 foot{lx - 3.0f + (frame < 4 ? std::cos(ph) * 1.5f : 0), ground - (frame < 4 ? std::max(0.0f, std::sin(ph)) : 0)};
      if (dead) { knee = V(lx, cy - 4); foot = V(lx + 1, cy - 6); }
      capsule(t, hip, knee, 0.7f, 0.6f, Lg, bias);
      capsule(t, knee, foot, 0.6f, 0.4f, Lg, bias);
    }
  };
  // big pincer: arm, palm, fixed finger and moving finger (open on the wind-up)
  auto claw = [&](Canvas& t, int bias, float raise, bool open, float reach) {
    Vec2 s{cx + 5, cy};
    Vec2 e{cx + 8, cy - 1.5f - raise};
    Vec2 palm{cx + 10.5f + reach, cy - 2.5f - raise * 1.3f};
    capsule(t, s, e, 1.0f, 0.9f, Lg, bias);
    capsule(t, e, palm, 1.1f, 2.0f, Lg, bias);
    float gap = open ? 1.8f : 0.4f;
    capsule(t, palm + Vec2{1, -0.8f}, palm + Vec2{4.5f, -1.2f - gap}, 1.2f, 0.5f, Lg, bias);   // upper finger
    capsule(t, palm + Vec2{1, 0.9f}, palm + Vec2{4.0f, 0.8f + gap * 0.5f}, 0.9f, 0.4f, Lg, bias - 1);
    px(t, palm + Vec2{4.5f, -1.4f - gap}, Lg[4 + bias]);
  };
  const float raise = frame == 4 ? 3.0f : (frame == 5 ? 0.0f : 1.0f);
  const float reach = frame == 5 ? 2.5f : 0.0f;
  legs(c, 0);
  if (!dead) claw(c, -1, raise + 1.5f, frame == 4, reach * 0.5f);
  // shell: broad flat carapace with a lit rim, ridges and spikes
  layered(c, [&](Canvas& t) {
    for (int y = (int)(cy - 5); y <= (int)(cy + 2); y++)
      for (int x = (int)(cx - 8); x <= (int)(cx + 8); x++) {
        float dx = (x + 0.5f - cx) / 8.0f, dy = (y + 0.5f - cy) / (y < cy ? 5.0f : 2.4f);
        if (dx * dx + dy * dy > 1) continue;
        int k = lightIndex(lightAt(dx * 0.9f, std::clamp(dy * 0.9f, -0.9f, 0.9f)) + (hashf(x, y, 3) - 0.5f) * 0.2f, x, y, 0.1f);
        if (dead) k = std::max(0, k - 1);
        t.set(x, y, S[k]);
      }
    for (int x = (int)cx - 6; x <= (int)cx + 6; x += 3) {   // segment ridges
      for (int y = 0; y < c.h; y++) if (solid(t, x, y)) { t.set(x, y - 1, S[3]); for (int k = 1; k < 4; k++) t.set(x, y + k, S[1]); break; }
    }
    hline(t, (int)cx - 7, (int)cx + 7, (int)cy + 1, S[0]);
    hline(t, (int)cx - 6, (int)cx + 6, (int)cy, S[3]);       // lit lip of the shell
  }, 0.5f);
  legs(c, 1);
  if (!dead) {
    int ex = (int)cx + 4, ey = (int)cy - 6 - (frame & 1);
    vline(c, ex, ey + 1, ey + 2, Lg[2]); vline(c, ex + 2, ey + 1, ey + 2, Lg[1]);
    c.set(ex, ey, frame == 6 ? Lg[0] : kInk); c.set(ex + 2, ey, frame == 6 ? Lg[0] : kInk);
    layered(c, [&](Canvas& t) { claw(t, 0, raise, frame == 4, reach); }, 0.6f);
  }
}

// ------------------------------------------------------------------ sandworm
void sandworm(Canvas& c, int frame) {
  const Ramp W = ramp5(rgba(80, 48, 52), rgba(132, 80, 64), rgba(184, 122, 84), rgba(218, 166, 110), rgba(242, 208, 150));
  const float ground = c.h - 2.0f;
  bool dead = frame == 7;
  // spine curve rising out of a sand mound
  float sway = frame < 4 ? std::sin(frame * PI * 0.5f) * 2.0f : 0.0f;
  Vec2 base{c.w * 0.4f, ground - 2};
  Vec2 tip{c.w * 0.45f + sway, 6.0f};
  float lean = 0;
  if (frame == 4) { tip = V(c.w * 0.3f, 4.0f); lean = -3; }
  if (frame == 5) { tip = V(c.w * 0.72f, 11.0f); lean = 4; }
  if (frame == 6) { tip = V(c.w * 0.38f, 8.0f); }
  if (dead) { base = V(c.w * 0.2f, ground - 2); tip = V(c.w * 0.85f, ground - 3); }
  Vec2 mid = (base + tip) * 0.5f + Vec2{-2.0f - lean * 0.3f + sway * 0.5f, 1};
  if (dead) mid = (base + tip) * 0.5f + Vec2{0, -2};
  // sand mound
  if (!dead)
    for (int y = 0; y < 4; y++)
      for (int x = -9 + y; x <= 9 - y; x++) c.set((int)base.x + x, (int)ground - y, kSand[y == 3 ? 3 : (x < 0 ? 2 : 1)]);
  // segments: balls along a quadratic curve, tail first
  const int N = 9;
  for (int i = 0; i <= N; i++) {
    float t = (float)i / N;
    Vec2 p = base * ((1 - t) * (1 - t)) + mid * (2 * (1 - t) * t) + tip * (t * t);
    float r = lerpf(5.0f, 3.8f, t);
    ball(c, p.x, p.y, r, r * 0.9f, W);
    // segment ring
    for (int k = -2; k <= 2; k++) c.set((int)(p.x + k), (int)(p.y + r * 0.6f), W[1]);
  }
  // head: a flared, armoured hood around a round maw ringed with teeth
  Vec2 h = tip + Vec2{1.5f, 0};
  ball(c, h.x - 0.5f, h.y, 5.0f, 4.8f, W);
  ball(c, h.x - 2.5f, h.y - 2.5f, 2.6f, 2.2f, W, 0.1f, 1);    // brow plate
  if (!dead) {
    const bool open = frame == 4 || frame == 5 || (frame & 1) == 0;
    const float ry = open ? 3.4f : 2.0f, rx = open ? 2.0f : 1.4f;
    const float mx = h.x + 3.2f, my = h.y + 0.5f;
    ellipse(c, mx, my, rx + 1.0f, ry + 1.0f, W[4]);                           // lip
    ellipse(c, mx + 0.4f, my, rx + 0.3f, ry + 0.3f, rgba(150, 40, 60));       // gums
    ellipse(c, mx + 0.8f, my, rx - 0.5f, ry - 0.6f, rgba(50, 14, 28));        // throat
    for (int k = 0; k < 7; k++) {   // teeth pointing inward
      float a2 = -PI * 0.5f + k * PI / 6.0f;
      c.set((int)(mx + 0.4f + std::cos(a2) * (rx + 0.3f) * 0.8f), (int)(my + std::sin(a2) * (ry + 0.3f)), kBone[4 - (k & 1)]);
    }
    if (frame == 5) c.set((int)mx + 1, (int)my, rgba(240, 90, 100));
  }
  // dorsal plates
  for (int i = 2; i < N; i += 2) {
    float t = (float)i / N;
    Vec2 p = base * ((1 - t) * (1 - t)) + mid * (2 * (1 - t) * t) + tip * (t * t);
    c.set((int)p.x - 3, (int)p.y - 1, W[4]);
  }
}

// ------------------------------------------------------------------ dragon (boss)
void dragon(Canvas& c, int frame) {
  // ember dragon: crimson scales, gold belly plates, wine-red wing membranes, bone horns and claws
  const Ramp R = ramp5(rgba(56, 18, 40), rgba(106, 28, 44), rgba(160, 44, 44), rgba(206, 84, 54), rgba(242, 146, 86));
  const Ramp Bl = ramp5(rgba(124, 66, 40), rgba(180, 112, 48), rgba(224, 162, 70), rgba(246, 206, 110), rgba(255, 238, 170));
  const Ramp Mb = ramp5(rgba(54, 18, 42), rgba(92, 28, 54), rgba(134, 44, 62), rgba(176, 74, 74), rgba(214, 118, 96));
  const Ramp& Hn = kBone;
  const float ground = c.h - 2.0f;
  const bool dead = frame == 7;
  float bx = 30, by = ground - 15;
  static const float bob[8] = {0, -1, -2, -1, -1, 1, 0, 0};
  by += bob[frame];
  if (frame == 5) bx += 2;
  if (frame == 6) bx -= 2;
  if (dead) by = ground - 8;

  // wing poses: elbow, wrist and four finger tips relative to the shoulder root
  struct WingPose { Vec2 elbow, wrist, tips[4]; };
  static const WingPose kUp = {{-6, -11}, {-2, -25}, {{-15, -27}, {-24, -20}, {-28, -10}, {-22, 0}}};
  static const WingPose kMid = {{-8, -7}, {-15, -16}, {{-33, -17}, {-36, -7}, {-31, 2}, {-20, 6}}};
  static const WingPose kDown = {{-8, -2}, {-16, 3}, {{-31, 4}, {-30, 12}, {-23, 17}, {-13, 14}}};
  static const WingPose kDead = {{-7, 1}, {-15, 6}, {{-27, 10}, {-24, 13}, {-18, 14}, {-11, 13}}};
  const WingPose* poses[8] = {&kUp, &kMid, &kDown, &kMid, &kUp, &kDown, &kMid, &kDead};
  const WingPose& wp = *poses[frame];

  auto wing = [&](Canvas& t, Vec2 root, int bias, float scale) {
    auto P = [&](Vec2 v) { return root + v * scale; };
    Vec2 elbow = P(wp.elbow), wrist = P(wp.wrist), tips[4];
    for (int i = 0; i < 4; i++) tips[i] = P(wp.tips[i]);
    Vec2 trail = P(Vec2{-9, 7}), body = root + Vec2{2, 3};
    // membrane panels between the fingers; trailing edges scalloped toward the wrist
    Vec2 sc[4];
    for (int i = 0; i < 3; i++) sc[i] = (tips[i] + tips[i + 1]) * 0.5f + (wrist - (tips[i] + tips[i + 1]) * 0.5f) * 0.22f;
    sc[3] = (tips[3] + trail) * 0.5f + (wrist - (tips[3] + trail) * 0.5f) * 0.18f;
    poly(t, {root, elbow, wrist, tips[0]}, Mb[3 + bias]);
    for (int i = 0; i < 3; i++) {
      poly(t, {wrist, tips[i], sc[i]}, Mb[(i & 1 ? 2 : 3) + bias]);
      poly(t, {wrist, sc[i], tips[i + 1]}, Mb[(i & 1 ? 3 : 2) + bias]);
    }
    poly(t, {wrist, tips[3], sc[3], trail}, Mb[1 + bias]);
    poly(t, {root, wrist, trail, body}, Mb[1 + bias]);
    poly(t, {root, elbow, wrist}, Mb[2 + bias]);
    // finger bones with lit veins
    for (int i = 0; i < 4; i++) {
      line(t, (int)wrist.x, (int)wrist.y, (int)tips[i].x, (int)tips[i].y, R[1 + bias]);
      line(t, (int)wrist.x, (int)wrist.y - 1, (int)tips[i].x, (int)tips[i].y - 1, R[3 + bias]);
    }
    capsule(t, root, elbow, 1.9f, 1.4f, R, bias);
    capsule(t, elbow, wrist, 1.4f, 1.0f, R, bias);
    px(t, wrist + Vec2{0, -1.5f}, Hn[3]);
    px(t, wrist + Vec2{1, -2.5f}, Hn[4]);   // wrist talon
  };

  Vec2 shoulder{bx + 6, by - 6};
  // far wing behind everything
  wing(c, shoulder + Vec2{4, -1}, -1, 0.82f);
  // tail: tapering chain sweeping back, plated ridge, spade tip
  float sw = frame < 4 ? std::sin(frame * PI * 0.5f) * 1.5f : 0;
  Vec2 t0{bx - 10, by + 2}, t1{bx - 19, by + 7 + sw}, t2{bx - 26, by + 2 - sw}, t3{bx - 30, by - 5};
  if (dead) { t1 = V(bx - 18, ground - 3); t2 = V(bx - 26, ground - 2); t3 = V(bx - 31, ground - 4); }
  const int TN = 16;
  for (int i = TN; i >= 0; i--) {
    float t = (float)i / TN;
    float u = 1 - t;
    Vec2 p = t0 * (u * u * u) + t1 * (3 * u * u * t) + t2 * (3 * u * t * t) + t3 * (t * t * t);
    float r = lerpf(5.0f, 1.2f, t);
    ball(c, p.x, p.y, r, r * 0.95f, R);
    c.set((int)(p.x + r * 0.2f), (int)(p.y + r * 0.8f), Bl[2]);
    if (i % 2 == 0 && i > 0 && i < TN) { c.set((int)p.x, (int)(p.y - r), R[4]); c.set((int)p.x, (int)(p.y - r - 1), Hn[2]); }
  }
  poly(c, {t3 + Vec2{1, 0}, t3 + Vec2{-4, -4}, t3 + Vec2{-2, 0}, t3 + Vec2{-4, 3}}, R[2]);
  px(c, t3 + Vec2{-3, -3}, R[4]);
  // legs
  const float ph = frame * PI * 0.5f;
  auto leg = [&](Canvas& t, Vec2 hip, float phase, int bias, bool hind) {
    Vec2 foot{hip.x + (frame < 4 ? std::cos(phase) * 2.0f : 0) + (hind ? -1 : 2), ground - (frame < 4 ? std::max(0.0f, std::sin(phase)) * 1.5f : 0)};
    if (dead) foot = hip + Vec2{6, 3};
    Vec2 knee = hind ? (hip + foot) * 0.5f + Vec2{4, -1} : (hip + foot) * 0.5f + Vec2{-1, 0};
    capsule(t, hip, knee, hind ? 4.4f : 2.6f, hind ? 2.6f : 2.0f, R, bias);
    capsule(t, knee, foot, hind ? 2.2f : 1.8f, 1.6f, R, bias);
    int fx = (int)foot.x, fy = (int)foot.y;
    hline(t, fx - 1, fx + 1, fy, R[1 + bias]);
    t.set(fx + 2, fy, Hn[2 + bias]); t.set(fx + 3, fy, Hn[3 + bias]); t.set(fx - 2, fy, Hn[2 + bias]);
  };
  leg(c, V(bx - 6, by + 3), ph + PI, -1, true);
  leg(c, V(bx + 9, by + 4), ph, -1, false);
  // body with belly plates and a spiked ridge
  furBall(c, bx - 3, by + 1, 12.5f, 8.6f, R, 0.2f, 41);
  furBall(c, bx + 7, by - 1, 8.5f, 8.0f, R, 0.2f, 42);
  for (int x = (int)bx - 12; x <= (int)bx + 13; x++) {
    int yb = -1;
    for (int y = c.h - 1; y > 0; y--) if (solid(c, x, y) && y < by + 10) { yb = y; break; }
    if (yb < 0) continue;
    for (int k = 0; k < 3; k++) {
      int k2 = k == 0 ? 1 : (k == 1 ? 2 : 3);
      if (x % 4 == 0) k2--;   // plate seams
      c.set(x, yb - k, Bl[std::max(0, k2)]);
    }
  }
  for (int x = (int)bx - 12; x <= (int)bx + 10; x += 3)
    for (int y = 0; y < c.h; y++)
      if (solid(c, x, y)) { c.set(x, y - 1, Hn[2]); c.set(x - 1, y, Hn[1]); c.set(x, y - 2, Hn[3]); break; }
  layered(c, [&](Canvas& t) { leg(t, V(bx - 5, by + 4), ph, 0, true); }, 0.6f);
  // neck + head pose
  Vec2 n0{bx + 11, by - 3};
  Vec2 head{bx + 25, by - 15};
  float jaw = 0;
  bool eyeShut = false, fire = false;
  if (frame < 4) head.y += (frame & 1) ? 1.0f : 0.0f;
  if (frame == 4) { head = V(bx + 17, by - 20); jaw = 1.0f; }
  if (frame == 5) { head = V(bx + 19, by - 9); jaw = 4.0f; fire = true; }
  if (frame == 6) { head = V(bx + 20, by - 19); eyeShut = true; jaw = 1.5f; }
  if (dead) { head = V(bx + 26, ground - 4); eyeShut = true; jaw = 0.5f; }
  Vec2 nc = (n0 + head) * 0.5f + (dead ? Vec2{0, 2} : Vec2{-4, -2});
  layered(c, [&](Canvas& t) {
    const int NN = 10;
    for (int i = 0; i <= NN; i++) {
      float s = (float)i / NN;
      Vec2 p = n0 * ((1 - s) * (1 - s)) + nc * (2 * (1 - s) * s) + head * (s * s);
      float r = lerpf(5.4f, 3.2f, s);
      ball(t, p.x, p.y, r, r, R);
      t.set((int)(p.x + r * 0.5f), (int)(p.y + r * 0.7f), Bl[2]);
      t.set((int)(p.x + r * 0.2f), (int)(p.y + r * 0.85f), Bl[1]);
      if (i % 2 == 1) { t.set((int)(p.x - r * 0.6f), (int)(p.y - r * 0.75f), Hn[3]); t.set((int)(p.x - r * 0.6f) - 1, (int)(p.y - r * 0.75f) - 1, Hn[2]); }
    }
  }, 0.45f);
  layered(c, [&](Canvas& t) {
    // skull + long snout + lower jaw
    ball(t, head.x, head.y, 5.0f, 4.2f, R);
    capsule(t, head + Vec2{2, 0.5f}, head + Vec2{10, 1.5f}, 3.2f, 2.2f, R);
    if (jaw > 0) {
      capsule(t, head + Vec2{1, 2.5f}, head + Vec2{8.5f, 2.5f + jaw}, 2.1f, 1.3f, R, -1);
      for (int k = 2; k <= 9; k++) t.set((int)head.x + k, (int)(head.y + 3.0f + jaw * (k - 1) / 8.0f) - 1, rgba(110, 24, 40));
      for (int k = 3; k <= 9; k += 2) { t.set((int)head.x + k, (int)head.y + 3, kWhite); t.set((int)head.x + k, (int)(head.y + 2.5f + jaw * (k - 1) / 8.0f), kWhite); }
    } else {
      hline(t, (int)head.x + 2, (int)head.x + 10, (int)head.y + 3, R[0]);
      t.set((int)head.x + 8, (int)head.y + 3, kWhite); t.set((int)head.x + 5, (int)head.y + 3, kWhite);
    }
    t.set((int)head.x + 11, (int)head.y + 1, R[0]);   // nostril
    hline(t, (int)head.x + 4, (int)head.x + 10, (int)head.y - 1, R[4]);   // snout ridge highlight
    // brow, swept horns, cheek frill
    hline(t, (int)head.x - 1, (int)head.x + 4, (int)head.y - 3, R[1]);
    capsule(t, head + Vec2{-2, -2}, head + Vec2{-10, -6}, 1.5f, 0.5f, Hn, 0);
    capsule(t, head + Vec2{-1, -3}, head + Vec2{-6, -10}, 1.3f, 0.4f, Hn, 0);
    t.set((int)head.x - 4, (int)head.y + 1, R[3]); t.set((int)head.x - 5, (int)head.y + 2, R[2]); t.set((int)head.x - 6, (int)head.y + 3, R[1]);
    if (eyeShut) hline(t, (int)head.x, (int)head.x + 2, (int)head.y - 1, R[0]);
    else {
      t.set((int)head.x + 1, (int)head.y - 1, rgba(255, 220, 80)); t.set((int)head.x + 2, (int)head.y - 1, rgba(255, 250, 200));
      t.set((int)head.x + 1, (int)head.y - 2, R[0]); t.set((int)head.x, (int)head.y - 1, R[0]);
    }
    if (frame == 4)   // fire kindling in the throat
      for (int i = 0; i < 4; i++) t.set((int)head.x - 2 - i, (int)head.y + 4 + i, kFire[3 - (i & 1)]);
  }, 0.6f);
  layered(c, [&](Canvas& t) { leg(t, V(bx + 9, by + 3), ph + PI, 0, false); }, 0.6f);
  // near wing in front
  layered(c, [&](Canvas& t) { wing(t, shoulder, 0, 1.0f); }, 0.55f);
  // fire breath on the strike: a widening, flickering cone
  if (fire) {
    Vec2 m = head + Vec2{11, 3};
    for (int i = 0; i < 40; i++) {
      float t = i / 39.0f;
      Vec2 p = m + Vec2{t * (c.w - m.x - 3), t * 7.0f + std::sin(t * 9.0f) * 1.2f};
      float r = 1.4f + t * 6.0f;
      for (int y = (int)(p.y - r); y <= (int)(p.y + r); y++)
        for (int x = (int)(p.x - r); x <= (int)(p.x + r); x++) {
          float d = std::hypot(x + 0.5f - p.x, y + 0.5f - p.y) / r + (hashf(x, y, 61) - 0.5f) * 0.25f;
          if (d > 1) continue;
          int k = d < 0.35f ? 4 : (d < 0.7f ? 3 : 2);
          if (t > 0.75f) k--;
          uint32_t cur = c.get(x, y);
          bool hotter = false;
          for (int q = k + 1; q <= 4; q++) if (cur == kFire[q]) hotter = true;
          if (!hotter) c.set(x, y, kFire[k]);
        }
    }
  }
}

}  // namespace

int monsterCellW(Monster m) {
  switch (m) {
    case Monster::Wolf: case Monster::IceWolf: return 32;
    case Monster::Boar: return 28;
    case Monster::Bear: return 36;
    case Monster::Slime: return 20;
    case Monster::Spider: case Monster::FrostSpider: return 28;
    case Monster::Bat: return 24;
    case Monster::Skeleton: case Monster::Draugr: case Monster::Goblin: return 24;
    case Monster::Troll: return 36;
    case Monster::Wraith: return 24;
    case Monster::Mudcrab: return 26;
    case Monster::Sandworm: return 28;
    case Monster::Dragon: return 80;
    default: return 24;
  }
}
int monsterCellH(Monster m) {
  switch (m) {
    case Monster::Wolf: case Monster::IceWolf: return 22;
    case Monster::Boar: return 20;
    case Monster::Bear: return 28;
    case Monster::Slime: return 16;
    case Monster::Spider: case Monster::FrostSpider: return 18;
    case Monster::Bat: return 20;
    case Monster::Skeleton: case Monster::Draugr: case Monster::Goblin: return 24;
    case Monster::Troll: return 34;
    case Monster::Wraith: return 28;
    case Monster::Mudcrab: return 16;
    case Monster::Sandworm: return 30;
    case Monster::Dragon: return 56;
    default: return 24;
  }
}

Canvas monsterSheet(Monster m) {
  const int cw = monsterCellW(m), chh = monsterCellH(m);
  Canvas sheet(cw * MONSTER_FRAMES, chh);
  for (int f = 0; f < MONSTER_FRAMES; f++) {
    Canvas cell(cw, chh);
    switch (m) {
      case Monster::Wolf: case Monster::IceWolf: {
        Quad q;
        bool ice = m == Monster::IceWolf;
        q.fur = ice ? ramp5(rgba(92, 108, 150), rgba(146, 166, 200), rgba(198, 214, 232), rgba(230, 240, 248), rgba(255, 255, 255))
                    : ramp5(rgba(44, 40, 60), rgba(78, 74, 90), rgba(118, 112, 118), rgba(158, 150, 146), rgba(198, 190, 176));
        q.belly = ice ? kSnow : ramp5(rgba(110, 96, 96), rgba(160, 146, 132), rgba(204, 192, 170), rgba(226, 218, 196), rgba(244, 238, 220));
        q.eye = ice ? rgba(110, 220, 255) : rgba(250, 200, 60);
        q.nose = kInk;
        q.bodyLen = 13; q.bodyR = 4.2f; q.legLen = 7; q.legW = 2.4f; q.headR = 3.4f; q.snout = 4.0f;
        q.ears = 0; q.tail = 0; q.saddle = true; q.seed = ice ? 7 : 3;
        quadruped(cell, q, f);
        break;
      }
      case Monster::Boar: {
        Quad q;
        q.fur = ramp5(rgba(46, 30, 38), rgba(78, 50, 46), rgba(112, 74, 56), rgba(148, 104, 72), rgba(182, 140, 98));
        q.belly = ramp5(rgba(80, 56, 52), rgba(122, 88, 70), rgba(160, 120, 92), rgba(190, 152, 116), rgba(214, 184, 146));
        q.eye = rgba(240, 90, 60); q.nose = rgba(196, 120, 120);
        q.bodyLen = 12; q.bodyR = 5.0f; q.legLen = 4.5f; q.legW = 2.4f; q.headR = 3.6f; q.snout = 3.0f;
        q.ears = 0; q.tail = 1; q.tusks = true; q.mane = true; q.seed = 5;
        quadruped(cell, q, f);
        break;
      }
      case Monster::Bear: {
        Quad q;
        q.fur = ramp5(rgba(46, 28, 36), rgba(80, 46, 40), rgba(120, 72, 48), rgba(158, 104, 64), rgba(194, 142, 92));
        q.belly = ramp5(rgba(80, 50, 44), rgba(118, 78, 56), rgba(150, 104, 70), rgba(180, 134, 90), rgba(206, 166, 116));
        q.eye = rgba(40, 20, 24); q.nose = kInk;
        q.bodyLen = 16; q.bodyR = 6.5f; q.legLen = 6.5f; q.legW = 3.6f; q.headR = 4.4f; q.snout = 3.0f;
        q.ears = 1; q.tail = 2; q.hump = true; q.seed = 9;
        quadruped(cell, q, f);
        break;
      }
      case Monster::Slime: slime(cell, f); break;
      case Monster::Spider: spider(cell, f, false); break;
      case Monster::FrostSpider: spider(cell, f, true); break;
      case Monster::Bat: bat(cell, f); break;
      case Monster::Skeleton: cell = humanoidCell(kSkeletonSp, f); break;
      case Monster::Draugr: cell = humanoidCell(kDraugrSp, f); break;
      case Monster::Goblin: cell = humanoidCell(kGoblinSp, f); break;
      case Monster::Troll: troll(cell, f); break;
      case Monster::Wraith: wraith(cell, f); break;
      case Monster::Mudcrab: mudcrab(cell, f); break;
      case Monster::Sandworm: sandworm(cell, f); break;
      case Monster::Dragon: dragon(cell, f); break;
      default: break;
    }
    outline(cell);
    place(sheet, cell, f, 0);
  }
  return sheet;
}

// =====================================================================================================
// 5. props
// =====================================================================================================
namespace {

// ---- nature building blocks ------------------------------------------------------------------------
struct Blob { float x, y, r; };

// Lush leaf canopy made of overlapping clumps. Clumps are painted back (top) to front (bottom); each has
// its own lit top-left and shaded underside, a scalloped edge, clumpy leaf texture and sparse highlights.
void canopy(Canvas& c, std::vector<Blob> blobs, const Ramp& R, uint32_t seed, float squash = 0.88f) {
  std::stable_sort(blobs.begin(), blobs.end(), [](const Blob& a, const Blob& b) { return a.y < b.y; });
  float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
  for (auto& b : blobs) { minX = std::min(minX, b.x - b.r); maxX = std::max(maxX, b.x + b.r); minY = std::min(minY, b.y - b.r); maxY = std::max(maxY, b.y + b.r); }
  float gcx = (minX + maxX) * 0.5f, gcy = (minY + maxY) * 0.5f, gw = std::max(1.0f, maxX - minX), gh = std::max(1.0f, maxY - minY);
  int bi = 0;
  for (auto& b : blobs) {
    float ph = hashf(bi, 3, seed) * TAU, ph2 = hashf(bi, 5, seed) * TAU;
    bi++;
    float ry = b.r * squash;
    for (int y = (int)std::floor(b.y - ry - 2); y <= (int)std::ceil(b.y + ry + 2); y++)
      for (int x = (int)std::floor(b.x - b.r - 2); x <= (int)std::ceil(b.x + b.r + 2); x++) {
        float dx = (x + 0.5f - b.x) / b.r, dy = (y + 0.5f - b.y) / ry;
        float d = std::sqrt(dx * dx + dy * dy);
        float ang = std::atan2(dy, dx);
        float edge = 1.0f + 0.09f * std::sin(ang * 6 + ph) + 0.05f * std::sin(ang * 11 + ph2);
        if (d > edge) continue;
        float nx = dx / edge, ny = dy / edge;
        float l = lightAt(nx * 0.82f, ny * 0.82f);
        l += ((gcx - (x + 0.5f)) / gw + (gcy - (y + 0.5f)) / gh) * 0.35f;     // whole-crown light
        l += (vnoise(x * 0.55f, y * 0.55f, seed) - 0.5f) * 0.55f;            // clumpy leaves
        int k = lightIndex(l, x, y, 0.12f);
        if (d > edge - 0.12f && ny > 0.2f) k = std::max(0, k - 1);              // dark rim on the underside
        c.set(x, y, R[k]);
      }
  }
  // sparkle of sunlit leaves on the light side
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      uint32_t p = c.get(x, y);
      if (p != R[3]) continue;
      uint32_t h = hash3(x, y, seed + 99);
      if (h % 9 == 0) { c.set(x, y, R[4]); if (h % 2) c.set(x + 1, y, R[4]); }
      else if (h % 13 == 0 && c.get(x, y + 1) == R[3]) c.set(x, y + 1, R[2]);
    }
}

// trunk: vertical cylinder in bark with streaks, flared roots at the base
void trunk(Canvas& c, float cx, int baseY, int topY, float w, const Ramp& R, uint32_t seed) {
  for (int y = topY; y <= baseY; y++) {
    float t = (float)(y - topY) / std::max(1, baseY - topY);
    float hw = w * 0.5f + (t > 0.75f ? (t - 0.75f) * 7.0f : 0.0f);
    int x0 = (int)std::floor(cx - hw), x1 = (int)std::ceil(cx + hw) - 1;
    for (int x = x0; x <= x1; x++) {
      float u = (x + 0.5f - cx) / std::max(0.5f, hw);
      int k = lightIndex(lightAt(u * 0.9f, 0) + (hashf(x, y / 3, seed) - 0.5f) * 0.25f, x, y, 0.0f);
      if (hash3(x, y / 2, seed) % 7 == 0) k = std::max(0, k - 1);   // bark streaks
      c.set(x, y, R[k]);
    }
  }
}

// chiselled rock: noisy silhouette, flat-shaded facets (Voronoi), crisp facet edges
void rock(Canvas& c, float cx, float cy, float rx, float ry, const Ramp& R, uint32_t seed, int facets = 7) {
  std::vector<Vec2> seeds;
  for (int i = 0; i < facets; i++) {
    float a = hashf(i, 1, seed) * TAU, r = std::sqrt(hashf(i, 2, seed)) * 0.85f;
    seeds.push_back({std::cos(a) * r, std::sin(a) * r * 0.9f - 0.15f});
  }
  for (int y = (int)std::floor(cy - ry - 1); y <= (int)std::ceil(cy + ry); y++)
    for (int x = (int)std::floor(cx - rx - 1); x <= (int)std::ceil(cx + rx + 1); x++) {
      float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
      float ang = std::atan2(dy, dx);
      float edge = 1.0f + 0.10f * std::sin(ang * 3 + seed) + 0.06f * std::sin(ang * 7 + seed * 2.0f);
      if (dy > 0.55f) edge *= 1.0f + (dy - 0.55f);   // flattened base sits on the ground
      if (dx * dx + dy * dy > edge * edge || dy > 0.98f) continue;
      int best = 0, second = 0;
      float bd = 1e9f, sd = 1e9f;
      for (int i = 0; i < facets; i++) {
        float d = len2(Vec2{dx, dy} - seeds[i]);
        if (d < bd) { sd = bd; second = best; bd = d; best = i; }
        else if (d < sd) { sd = d; second = i; }
      }
      (void)second;
      Vec2 n = seeds[best] * 1.05f;
      float l = lightAt(std::clamp(n.x, -0.95f, 0.95f), std::clamp(n.y - 0.25f, -0.95f, 0.95f));
      int k = lightIndex(l, x, y, 0.0f);
      if (std::sqrt(sd) - std::sqrt(bd) < 0.07f) k = std::max(0, k - 1);   // facet seam
      if (dy > 0.7f) k = std::max(0, k - 1);
      c.set(x, y, R[k]);
    }
}

// little clump of grass blades at the base of things (anchors props visually)
void grassTuft(Canvas& c, int x, int y, uint32_t seed, int n = 4) {
  for (int i = 0; i < n; i++) {
    int bx = x + i * 2 - n + (int)(hash3(i, 0, seed) % 2);
    int h = 2 + (int)(hash3(i, 1, seed) % 3);
    for (int j = 0; j < h; j++) c.set(bx + (j == h - 1 && (i & 1) ? 1 : 0), y - j, kLeaf[j == h - 1 ? 3 : 2]);
  }
}

// lays a blanket of snow on the upward-facing top of a shape (gentle slopes only, so sides stay bare)
void snowCap(Canvas& c, int depth, uint32_t seed) {
  std::vector<int> top(c.w, -1);
  for (int x = 0; x < c.w; x++)
    for (int y = 0; y < c.h; y++) if (solid(c, x, y)) { top[x] = y; break; }
  for (int x = 0; x < c.w; x++) {
    if (top[x] < 0) continue;
    int l = x > 0 && top[x - 1] >= 0 ? top[x - 1] : top[x] + 9, r = x + 1 < c.w && top[x + 1] >= 0 ? top[x + 1] : top[x] + 9;
    int slope = std::max(std::abs(l - top[x]), std::abs(r - top[x]));
    if (slope > 2) continue;
    int d = depth + (int)(hash3(x, 0, seed) % 2) - (slope == 2 ? 1 : 0);
    for (int k = 0; k < d && solid(c, x, top[x] + k); k++) c.set(x, top[x] + k, kSnow[k == 0 ? 4 : (k == d - 1 ? 2 : 3)]);
  }
}

// ---- trees ------------------------------------------------------------------------------------------
void oakTree(Canvas& c, const Ramp& leaf, uint32_t seed, int variant) {
  const int W = c.w, H = c.h;
  float cx = W * 0.5f;
  trunk(c, cx, H - 2, H / 2, 6.0f, kBark, seed);
  // a couple of branches reaching into the crown
  thickLine(c, cx - 1, H * 0.62f, cx - 7, H * 0.42f, 2.0f, kBark[1]);
  thickLine(c, cx + 1, H * 0.6f, cx + 8, H * 0.44f, 2.0f, kBark[1]);
  std::vector<Blob> b;
  if (variant == 0) {
    b = {{cx, 11, 9}, {cx - 9, 15, 8}, {cx + 9, 14, 8}, {cx - 13, 22, 6.5f}, {cx + 13, 22, 6.5f},
         {cx - 5, 22, 9}, {cx + 5, 23, 9}, {cx, 28, 8}, {cx - 10, 29, 6}, {cx + 10, 29, 6}, {cx, 17, 8}};
  } else if (variant == 1) {
    b = {{cx + 1, 10, 8}, {cx - 8, 14, 7}, {cx + 9, 15, 7.5f}, {cx - 11, 22, 6}, {cx + 11, 23, 6},
         {cx - 4, 21, 8.5f}, {cx + 5, 22, 8}, {cx - 1, 27, 7.5f}, {cx + 8, 28, 5}};
  } else {
    b = {{cx - 1, 11, 9}, {cx - 10, 16, 7.5f}, {cx + 9, 13, 8}, {cx - 12, 24, 6}, {cx + 13, 21, 6.5f},
         {cx - 5, 23, 9}, {cx + 6, 22, 8.5f}, {cx, 29, 7.5f}, {cx - 9, 30, 5.5f}, {cx + 10, 28, 5.5f}};
  }
  canopy(c, b, leaf, seed);
  // shadow the trunk right under the crown
  for (int x = 0; x < W; x++)
    for (int y = (int)(H * 0.62f); y < (int)(H * 0.72f); y++)
      if (c.get(x, y) == kBark[2] || c.get(x, y) == kBark[3]) c.set(x, y, kBark[1]);
  grassTuft(c, (int)cx - 4, H - 2, seed, 3);
  grassTuft(c, (int)cx + 5, H - 2, seed + 1, 2);
}

void pineTree(Canvas& c, const Ramp& P, uint32_t seed, int tiers, bool snow) {
  const int W = c.w, H = c.h;
  float cx = W * 0.5f;
  trunk(c, cx, H - 2, H - 9, 4.0f, kBark, seed);
  float top = 2, bottom = H - 7.0f;
  for (int t = 0; t < tiers; t++) {
    float f = (float)t / (tiers - 1);
    float ty0 = top + (bottom - top) * f * 0.78f;
    float ty1 = ty0 + (bottom - top) * (0.30f + 0.06f * f);
    float hw = 3.0f + f * (W * 0.5f - 4.0f);
    for (int y = (int)ty0; y <= (int)ty1; y++) {
      float u = (y - ty0) / std::max(1.0f, ty1 - ty0);
      float half = hw * (0.15f + 0.85f * u);
      // jagged lower edge
      for (int x = (int)(cx - half - 1); x <= (int)(cx + half + 1); x++) {
        float rel = (x + 0.5f - cx) / std::max(1.0f, half);
        if (std::fabs(rel) > 1.0f) continue;
        float jag = (float)(hash3(x, t, seed) % 3);
        if (y > ty1 - jag - std::fabs(rel) * 1.5f) continue;
        float l = lightAt(rel * 0.8f, (u - 0.5f) * 0.9f) + (hashf(x, y, seed) - 0.5f) * 0.3f;
        int k = lightIndex(l, x, y, 0.15f);
        if (((x + y * 2) % 5) == 0 && k > 0) k--;                 // needle strokes
        if (y > ty1 - jag - std::fabs(rel) * 1.5f - 1.5f) k = std::max(0, k - 1);
        // snow settles on the upper part of every tier, its lower edge ragged
        if (snow && u < (t == 0 ? 0.12f : 0.26f) + 0.25f * hashf(x / 2, t, seed) - std::fabs(rel) * 0.15f) {
          c.set(x, y, kSnow[std::clamp(k + 1, 1, 4)]);
          continue;
        }
        c.set(x, y, P[k]);
      }
    }
  }
  c.set((int)cx, 1, P[3]);
  if (snow) {
    for (int x = (int)cx - 5; x <= (int)cx + 5; x++) c.set(x, H - 2, kSnow[(x & 1) ? 3 : 2]);
  }
}

void birchTree(Canvas& c, uint32_t seed) {
  const int W = c.w, H = c.h;
  float cx = W * 0.5f;
  const Ramp Bw = ramp5(rgba(90, 90, 108), rgba(160, 160, 166), rgba(214, 212, 204), rgba(236, 234, 224), rgba(252, 250, 242));
  trunk(c, cx, H - 2, H / 3, 4.0f, Bw, seed);
  for (int y = H / 3; y < H - 2; y++)   // black bark marks
    if (hash3(0, y, seed) % 4 == 0) { int x = (int)cx - 2 + (int)(hash3(1, y, seed) % 3); c.set(x, y, kInk); c.set(x + 1, y, kBark[0]); }
  thickLine(c, cx, H * 0.5f, cx - 6, H * 0.36f, 1.4f, Bw[2]);
  thickLine(c, cx, H * 0.46f, cx + 5, H * 0.34f, 1.4f, Bw[1]);
  std::vector<Blob> b = {{cx, 9, 6.5f}, {cx - 6, 13, 6}, {cx + 6, 12, 6}, {cx - 8, 20, 5}, {cx + 8, 19, 5.5f},
                         {cx - 2, 18, 6.5f}, {cx + 3, 24, 5.5f}, {cx - 5, 25, 4.5f}};
  canopy(c, b, kBirchLeaf, seed, 0.95f);
  grassTuft(c, (int)cx - 3, H - 2, seed, 3);
}

void deadTree(Canvas& c, uint32_t seed) {
  const int W = c.w, H = c.h;
  const Ramp D = ramp5(rgba(40, 32, 40), rgba(70, 56, 58), rgba(104, 86, 78), rgba(136, 116, 98), rgba(166, 146, 122));
  float cx = W * 0.5f;
  trunk(c, cx, H - 2, H / 2, 5.0f, D, seed);
  struct Br { Vec2 a; float ang, len, w; int depth; };
  std::vector<Br> st = {{{cx, H * 0.55f}, -1.62f, 13.0f, 3.2f, 0}};
  while (!st.empty()) {
    Br b = st.back();
    st.pop_back();
    Vec2 e = b.a + Vec2{std::cos(b.ang), std::sin(b.ang)} * b.len;
    capsule(c, b.a, e, b.w * 0.5f, b.w * 0.36f, D, 0, 0);
    if (b.depth < 3) {
      float s = 0.45f + hashf(b.depth, (int)b.len, seed) * 0.3f;
      st.push_back({e, b.ang - s, b.len * 0.66f, b.w * 0.7f, b.depth + 1});
      st.push_back({e, b.ang + s * 0.9f, b.len * 0.6f, b.w * 0.66f, b.depth + 1});
      if (b.depth == 0) st.push_back({b.a + (e - b.a) * 0.45f, b.ang + 0.9f, b.len * 0.55f, b.w * 0.6f, 2});
    }
  }
  // knot hole
  c.set((int)cx, H - 12, kInk); c.set((int)cx, H - 11, D[0]);
  grassTuft(c, (int)cx - 4, H - 2, seed, 2);
}

void willowTree(Canvas& c, uint32_t seed) {
  const int W = c.w, H = c.h;
  float cx = W * 0.5f;
  trunk(c, cx, H - 2, H / 2, 7.0f, kBark, seed);
  std::vector<Blob> b = {{cx, 11, 10}, {cx - 10, 15, 8}, {cx + 10, 15, 8}, {cx - 4, 17, 9}, {cx + 5, 18, 9}};
  canopy(c, b, kWillow, seed, 0.8f);
  // hanging fronds
  for (int x = 2; x < W - 2; x++) {
    int top = -1;
    for (int y = 0; y < H; y++) if (solid(c, x, y) && chA(c.get(x, y)) && c.get(x, y) != kBark[2]) { top = y; break; }
    if (top < 0 || (x % 2 && hash3(x, 0, seed) % 3)) continue;
    int startY = 0;
    for (int y = H - 1; y >= 0; y--) { uint32_t p = c.get(x, y); bool leaf = false; for (int k = 0; k < 5; k++) if (p == kWillow[k]) leaf = true; if (leaf) { startY = y; break; } }
    int len = 8 + (int)(hash3(x, 1, seed) % 12);
    if (std::fabs(x - cx) < 4) len /= 3;
    for (int j = 0; j < len && startY + j < H - 3; j++) {
      int k = j < 3 ? 2 : (j < len - 2 ? 1 : 2);
      if (j % 4 == 1) k = 3;
      c.set(x, startY + j, kWillow[k]);
    }
  }
  grassTuft(c, (int)cx - 4, H - 2, seed, 3);
}

void palmTree(Canvas& c, uint32_t seed) {
  const int W = c.w, H = c.h;
  const Ramp Pt = ramp5(rgba(70, 48, 44), rgba(118, 84, 60), rgba(162, 122, 80), rgba(196, 160, 106), rgba(224, 196, 140));
  const Ramp Pl = ramp5(rgba(26, 64, 50), rgba(40, 104, 56), rgba(70, 148, 60), rgba(124, 186, 72), rgba(184, 220, 104));
  // curved trunk of stacked rings
  Vec2 base{W * 0.42f, (float)H - 2}, topP{W * 0.56f, 14.0f};
  for (int i = 0; i <= 16; i++) {
    float t = i / 16.0f;
    Vec2 p = base * (1 - t) + topP * t + Vec2{std::sin(t * PI) * -3.0f, 0};
    float r = lerpf(2.6f, 1.8f, t);
    ellipse(c, p.x, p.y, r, 1.6f, Pt[2]);
    for (int x = (int)(p.x - r); x <= (int)(p.x + r); x++) {
      float u = (x + 0.5f - p.x) / r;
      c.set(x, (int)p.y, Pt[lightIndex(lightAt(u * 0.9f, 0), x, 0, 0)]);
      if (i % 2 == 0) c.set(x, (int)p.y + 1, Pt[1]);
    }
  }
  // fronds
  Vec2 crown = topP + Vec2{0, -1};
  for (int f = 0; f < 7; f++) {
    float a = -PI * 0.5f + (f - 3) * 0.52f + (hashf(f, 0, seed) - 0.5f) * 0.2f;
    float L = 13.0f - std::fabs(f - 3.0f) * 0.4f;
    Vec2 prev = crown;
    int bias = (f < 3) ? 0 : (f == 3 ? 1 : -1);
    for (int s = 1; s <= 12; s++) {
      float t = s / 12.0f;
      Vec2 p = crown + Vec2{std::cos(a) * L * t, std::sin(a) * L * t + t * t * 11.0f};
      line(c, (int)prev.x, (int)prev.y, (int)p.x, (int)p.y, Pl[2 + bias]);
      // leaflets hanging below the rib
      int ll = (int)(3.5f * (1 - t * 0.6f));
      for (int k = 1; k <= ll; k++) {
        c.set((int)p.x + (s & 1), (int)p.y + k, Pl[(k == 1 ? 3 : 1) + bias]);
        if (s % 3 == 0) c.set((int)p.x - 1, (int)p.y + k, Pl[1 + bias]);
      }
      prev = p;
    }
  }
  // coconuts
  ball(c, crown.x - 1.5f, crown.y + 2.5f, 1.6f, 1.6f, kWoodDark);
  ball(c, crown.x + 1.5f, crown.y + 3.0f, 1.6f, 1.6f, kWoodDark);
}

// ---- plants -----------------------------------------------------------------------------------------
void bush(Canvas& c, const Ramp& leaf, uint32_t seed) {
  float cx = c.w * 0.5f, by = c.h - 2.0f;
  std::vector<Blob> b = {{cx - 4, by - 6, 5}, {cx + 4, by - 6, 5}, {cx, by - 9, 5.5f}, {cx - 1, by - 4, 5.5f}, {cx + 5, by - 3, 3.5f}, {cx - 6, by - 3, 3.5f}};
  canopy(c, b, leaf, seed, 0.85f);
}

void flowers(Canvas& c, int kind, uint32_t seed) {
  static const uint32_t petals[3][3] = {
    {rgba(232, 70, 60), rgba(250, 200, 70), rgba(250, 130, 60)},
    {rgba(110, 150, 240), rgba(250, 248, 240), rgba(170, 200, 255)},
    {rgba(190, 100, 210), rgba(250, 150, 190), rgba(240, 220, 120)}};
  // leaves first
  for (int i = 0; i < 7; i++) {
    int x = 2 + (int)(hash3(i, 0, seed) % 12), y = c.h - 2 - (int)(hash3(i, 1, seed) % 3);
    c.set(x, y, kLeaf[2]); c.set(x + 1, y, kLeaf[1]); c.set(x, y - 1, kLeaf[3]);
  }
  const int pos[6][2] = {{3, 6}, {8, 4}, {12, 7}, {6, 9}, {11, 2}, {2, 2}};
  for (int i = 0; i < 6; i++) {
    int x = pos[i][0] + (int)(hash3(i, 2, seed) % 2), y = pos[i][1] + 1;
    uint32_t pc = petals[kind][i % 3];
    Ramp pr = ramp(pc, 0.8f);
    vline(c, x, y + 1, c.h - 2, kLeaf[1 + (i & 1)]);
    c.set(x, y - 1, pr[3]); c.set(x - 1, y, pr[2]); c.set(x + 1, y, pr[1]); c.set(x, y + 1, pr[1]);
    c.set(x, y, kind == 1 && (i % 3) == 1 ? rgba(250, 210, 80) : pr[4]);
    if (kind == 1 && (i % 3) != 1) c.set(x, y, rgba(250, 230, 120));
  }
}

void tallGrass(Canvas& c, uint32_t seed) {
  for (int i = 0; i < 9; i++) {
    float x = 1.5f + i * 1.6f;
    int h = 7 + (int)(hash3(i, 0, seed) % 6);
    float lean = (hashf(i, 1, seed) - 0.5f) * 4.0f;
    for (int j = 0; j < h; j++) {
      float t = (float)j / h;
      int px2 = (int)(x + lean * t * t);
      int k = t < 0.3f ? 1 : (t < 0.75f ? 2 : 3);
      if (i % 3 == 0) k = std::max(0, k - 1);
      c.set(px2, c.h - 2 - j, kLeaf[k]);
      if (j < 3) c.set(px2 + 1, c.h - 2 - j, kLeaf[1]);
    }
  }
}

void reeds(Canvas& c, uint32_t seed) {
  const Ramp Rd = ramp5(rgba(40, 70, 52), rgba(68, 108, 58), rgba(104, 146, 64), rgba(150, 180, 82), rgba(196, 212, 120));
  for (int i = 0; i < 7; i++) {
    int x = 2 + i * 2, h = 10 + (int)(hash3(i, 0, seed) % 7);
    int lean = (int)(hash3(i, 1, seed) % 3) - 1;
    for (int j = 0; j < h; j++) c.set(x + (j > h * 2 / 3 ? lean : 0), c.h - 2 - j, Rd[j > h / 2 ? 3 : 2]);
    if (i % 2 == 0) {   // cattail
      int tx = x + lean, ty = c.h - 2 - h;
      for (int k = 0; k < 3; k++) { c.set(tx, ty + k, kWoodDark[3 - (k == 2)]); c.set(tx + 1, ty + k, kWoodDark[2]); }
      c.set(tx, ty - 1, Rd[3]);
    }
  }
  // water ripple at the base
  for (int x = 1; x < c.w - 1; x++) if (x % 3) c.set(x, c.h - 1, withA(kWater[3], 180));
}

void cactus(Canvas& c) {
  const Ramp Cg = ramp5(rgba(30, 70, 56), rgba(50, 108, 62), rgba(80, 148, 70), rgba(126, 182, 86), rgba(176, 214, 120));
  int cx = c.w / 2;
  auto column = [&](int x0, int y0, int x1, int y1) {
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) {
        float u = (x + 0.5f - (x0 + x1 + 1) * 0.5f) / ((x1 - x0 + 1) * 0.5f);
        int k = lightIndex(lightAt(u * 0.9f, y == y0 ? -0.6f : 0), x, y, 0);
        if ((x - x0) % 2 == 1 && k > 1) k--;   // ribs
        c.set(x, y, Cg[k]);
      }
    for (int x = x0; x <= x1; x++) c.set(x, y0 - 1, (x == x0 || x == x1) ? 0 : Cg[3]);
  };
  column(cx - 2, 3, cx + 2, c.h - 2);
  column(cx - 7, 9, cx - 5, 15); hline(c, cx - 6, cx - 3, 15, Cg[2]); hline(c, cx - 7, cx - 3, 16, Cg[1]);
  column(cx + 5, 6, cx + 7, 12); hline(c, cx + 3, cx + 6, 12, Cg[2]); hline(c, cx + 3, cx + 7, 13, Cg[1]);
  for (int y = 5; y < c.h - 3; y += 3) { c.set(cx - 3, y, kBone[4]); c.set(cx + 3, y + 1, kBone[3]); }
  c.set(cx, 1, rgba(250, 120, 160)); c.set(cx - 1, 2, rgba(230, 90, 140)); c.set(cx + 1, 2, rgba(230, 90, 140));
}

void mushrooms(Canvas& c) {
  const Ramp Cap = ramp5(rgba(96, 26, 40), rgba(156, 40, 44), rgba(210, 62, 52), rgba(236, 110, 80), rgba(252, 170, 130));
  auto shroom = [&](int x, int y, int r, const Ramp& cap) {
    int stemH = r + 1;
    for (int j = 0; j < stemH; j++) { c.set(x - 1, y - j, kBone[3]); c.set(x, y - j, kBone[2]); }
    int top = y - stemH - r + 1;
    for (int j = 0; j < r; j++)
      for (int i = -r; i <= r; i++) {
        if (i * i + (r - j) * (r - j) * 2 > r * r * 2 + r) continue;
        float u = (float)i / r;
        c.set(x + i, top + j, cap[lightIndex(lightAt(u * 0.8f, (float)(j - r) / r * 0.8f), x + i, top + j, 0.1f)]);
      }
    hline(c, x - r + 1, x + r - 1, top + r - 1, cap[0]);
    if (r >= 3) { c.set(x - 1, top + 1, kWhite); c.set(x + 2, top + 2, kWhite); c.set(x - r + 1, top + 2, kBone[3]); }
  };
  const Ramp Brown = ramp5(rgba(70, 44, 40), rgba(110, 72, 50), rgba(150, 104, 66), rgba(186, 140, 90), rgba(214, 176, 124));
  shroom(11, c.h - 2, 2, Brown);
  shroom(5, c.h - 2, 4, Cap);
  shroom(9, c.h - 2, 3, Cap);
}

void lilyPad(Canvas& c) {
  auto pad = [&](float x, float y, float r, float notch) {
    for (int j = (int)(y - r); j <= (int)(y + r); j++)
      for (int i = (int)(x - r); i <= (int)(x + r); i++) {
        float dx = (i + 0.5f - x) / r, dy = (j + 0.5f - y) / (r * 0.6f);
        if (dx * dx + dy * dy > 1) continue;
        float a = std::atan2(dy, dx);
        if (std::fabs(a - notch) < 0.35f) continue;
        int k = lightIndex(lightAt(dx * 0.6f, dy * 0.6f), i, j, 0.1f);
        c.set(i, j, kLeaf[std::max(1, k)]);
      }
  };
  pad(6, 6, 5.5f, 0.6f);
  pad(12, 4, 3.5f, 2.4f);
  pad(12, 8, 2.8f, -1.0f);
  // flower
  int fx = 6, fy = 4;
  c.set(fx, fy - 1, rgba(252, 220, 230)); c.set(fx - 1, fy, rgba(240, 150, 180)); c.set(fx + 1, fy, rgba(230, 130, 170));
  c.set(fx, fy, rgba(250, 230, 120)); c.set(fx, fy + 1, rgba(220, 120, 160));
}

void fern(Canvas& c, uint32_t seed) {
  // arching fronds with alternating leaflets; outer fronds darker, the central ones catch the light
  Vec2 base{c.w * 0.5f, (float)c.h - 2};
  const int order[7] = {0, 6, 1, 5, 2, 4, 3};
  for (int oi = 0; oi < 7; oi++) {
    int f = order[oi];
    float a = PI + 0.3f + f * (PI - 0.6f) / 6.0f;
    float L = 7.0f + (f == 3 ? 1.5f : (f % 2) * 1.0f) + hashf(f, 0, seed);
    int bias = (f == 2 || f == 3 || f == 4) ? 1 : (f == 0 || f == 6 ? -1 : 0);
    Vec2 prev = base;
    for (int s2 = 1; s2 <= 10; s2++) {
      float t = s2 / 10.0f;
      Vec2 p = base + Vec2{std::cos(a) * L * t, std::sin(a) * L * t + t * t * 5.0f};
      line(c, (int)prev.x, (int)prev.y, (int)p.x, (int)p.y, kLeaf[1 + bias]);
      if (s2 >= 2 && s2 <= 9 && (s2 & 1) == 0) {
        float pa = a + PI * 0.5f;
        float ll = 2.2f * (1.0f - t * 0.5f);
        c.set((int)(p.x + std::cos(pa) * ll), (int)(p.y + std::sin(pa) * ll), kLeaf[3 + bias]);
        c.set((int)(p.x + std::cos(pa) * ll * 0.5f), (int)(p.y + std::sin(pa) * ll * 0.5f), kLeaf[2 + bias]);
        c.set((int)(p.x - std::cos(pa) * ll), (int)(p.y - std::sin(pa) * ll), kLeaf[2 + bias]);
        c.set((int)(p.x - std::cos(pa) * ll * 0.5f), (int)(p.y - std::sin(pa) * ll * 0.5f), kLeaf[1 + bias]);
      }
      prev = p;
    }
  }
}

// ---- camp / town ------------------------------------------------------------------------------------
// box seen in 3/4 view: top face (lighter) + front face
void plankBox(Canvas& c, int x0, int y0, int w, int topH, int frontH, const Ramp& R) {
  for (int y = 0; y < topH; y++)
    for (int x = 0; x < w; x++) {
      int k = (x == 0) ? 4 : (x == w - 1 ? 2 : 3);
      if (y == 0) k = std::min(4, k + 1);
      c.set(x0 + x, y0 + y, R[k]);
    }
  for (int y = 0; y < frontH; y++)
    for (int x = 0; x < w; x++) {
      int k = (x == 0) ? 2 : (x >= w - 2 ? 1 : 2);
      if (y == frontH - 1) k = 0;
      if (y == 0) k = 1;
      c.set(x0 + x, y0 + topH + y, R[k]);
    }
}

void chest(Canvas& c, bool open) {
  const Ramp& Wd = kWood;
  int x0 = 1, w = c.w - 2;
  int base = c.h - 2;
  // body front
  int bodyTop = base - 7;
  for (int y = bodyTop; y <= base; y++)
    for (int x = x0; x < x0 + w; x++) {
      int k = x == x0 ? 3 : (x >= x0 + w - 2 ? 1 : 2);
      if ((y - bodyTop) % 3 == 2) k = std::max(0, k - 1);   // planks
      if (y == base) k = 0;
      c.set(x, y, Wd[k]);
    }
  // iron bands + lock
  for (int y = bodyTop; y <= base; y++) { c.set(x0 + 2, y, kIron[2]); c.set(x0 + w - 3, y, kIron[1]); }
  if (!open) {
    // rounded lid
    for (int y = bodyTop - 5; y < bodyTop; y++)
      for (int x = x0; x < x0 + w; x++) {
        int j = y - (bodyTop - 5);
        if (j == 0 && (x == x0 || x == x0 + w - 1)) continue;
        int k = j <= 1 ? 4 : (j == 4 ? 1 : 3);
        if (x == x0 + w - 1) k--;
        c.set(x, y, Wd[k]);
      }
    hline(c, x0, x0 + w - 1, bodyTop, Wd[0]);
    for (int y = bodyTop - 5; y < bodyTop; y++) { c.set(x0 + 2, y, kIron[3]); c.set(x0 + w - 3, y, kIron[2]); }
    c.set(x0 + 2, bodyTop - 5, kIron[4]);
    box(c, x0 + w / 2 - 1, bodyTop - 1, x0 + w / 2, bodyTop + 1, kGold[3]);
    c.set(x0 + w / 2, bodyTop + 1, kGold[1]); c.set(x0 + w / 2 - 1, bodyTop + 2, kGold[1]);
  } else {
    // open lid tilted back, gold heap inside
    for (int y = bodyTop - 10; y < bodyTop - 3; y++)
      for (int x = x0; x < x0 + w; x++) {
        int j = y - (bodyTop - 10);
        int k = j == 0 ? 3 : 1;
        if (x == x0 || x == x0 + w - 1) k = 0;
        c.set(x, y, Wd[k]);
      }
    for (int y = bodyTop - 10; y < bodyTop - 3; y++) { c.set(x0 + 2, y, kIron[1]); c.set(x0 + w - 3, y, kIron[0]); }
    // inside rim (dark) + gold
    for (int y = bodyTop - 3; y < bodyTop; y++)
      for (int x = x0; x < x0 + w; x++) c.set(x, y, (x == x0 || x == x0 + w - 1) ? Wd[2] : Wd[0]);
    for (int x = x0 + 2; x < x0 + w - 2; x++) {
      int hgt = 1 + (int)(hash3(x, 0, 5) % 2);
      for (int k = 0; k < hgt; k++) c.set(x, bodyTop - 2 - k, kGold[(x + k) % 3 == 0 ? 4 : 3]);
    }
    c.set(x0 + 4, bodyTop - 4, kWhite); c.set(x0 + w - 5, bodyTop - 3, rgba(120, 220, 255));
    hline(c, x0, x0 + w - 1, bodyTop, Wd[3]);
    box(c, x0 + w / 2 - 1, bodyTop + 1, x0 + w / 2, bodyTop + 2, kGold[2]);
  }
}

void barrel(Canvas& c, int x0, int y0, int w, int h) {
  // staves with a bulge, iron hoops, elliptical top
  float cx = x0 + w * 0.5f;
  for (int y = y0 + 2; y < y0 + h; y++) {
    float t = (float)(y - y0 - 2) / (h - 3);
    float bulge = 1.0f - (t - 0.5f) * (t - 0.5f) * 1.2f;
    float hw = w * 0.5f * (0.86f + 0.14f * bulge);
    for (int x = (int)std::floor(cx - hw); x < (int)std::ceil(cx + hw); x++) {
      float u = (x + 0.5f - cx) / hw;
      int k = lightIndex(lightAt(u * 0.9f, 0), x, y, 0);
      if ((x - x0) % 3 == 0) k = std::max(0, k - 1);   // stave seams
      c.set(x, y, kWood[k]);
    }
  }
  for (int hy : {y0 + 4, y0 + h - 4})
    for (int x = x0; x < x0 + w; x++) if (solid(c, x, hy)) c.set(x, hy, kIron[x < cx - 2 ? 3 : (x < cx + 2 ? 2 : 1)]);
  ellipse(c, cx, y0 + 2.5f, w * 0.43f, 2.0f, kWood[3]);
  ellipse(c, cx, y0 + 2.5f, w * 0.30f, 1.2f, kWood[2]);
  hline(c, (int)(cx - w * 0.43f), (int)(cx + w * 0.43f) - 1, y0 + 4, kWood[1]);
  c.set((int)cx - 2, y0 + 2, kWood[4]);
}

void crate(Canvas& c) {
  int x0 = 1, w = c.w - 2, topH = 5, frontH = c.h - 2 - topH;
  plankBox(c, x0, 1, w, topH, frontH, kWood);
  for (int y = 1; y < 1 + topH; y++) c.set(x0 + w / 2, y, kWood[2]);
  int fy = 1 + topH;
  // frame + cross brace
  for (int y = fy; y < fy + frontH; y++) { c.set(x0, y, kWood[3]); c.set(x0 + 1, y, kWood[3]); c.set(x0 + w - 1, y, kWood[1]); c.set(x0 + w - 2, y, kWood[1]); }
  for (int x = x0; x < x0 + w; x++) { c.set(x, fy, kWood[3]); c.set(x, fy + frontH - 2, kWood[2]); }
  line(c, x0 + 2, fy + 1, x0 + w - 3, fy + frontH - 3, kWood[3]);
  line(c, x0 + 2, fy + 2, x0 + w - 4, fy + frontH - 3, kWood[1]);
  for (int x = x0 + 2; x < x0 + w - 2; x++) if ((x & 3) == 0) c.set(x, fy + 1, kIron[2]);
}

void flame(Canvas& c, double cxd, double based, double hd, double wd, int frame, uint32_t seed) {
  const float cx = (float)cxd, base = (float)based, h = (float)hd, w = (float)wd;
  // teardrop flame with flicker; layered from outer red to white-hot core
  for (int layer = 0; layer < 4; layer++) {
    float s = 1.0f - layer * 0.22f;
    float fh = h * s, fw = w * s;
    for (int y = (int)(base - fh - 1); y <= (int)base; y++) {
      float t = (base - y) / fh;   // 0 bottom .. 1 tip
      if (t < 0 || t > 1) continue;
      float sway = std::sin(t * 3.2f + frame * 1.7f + seed) * t * w * 0.35f;
      float half = fw * 0.5f * std::sqrt(std::max(0.0f, 1 - t)) * (t < 0.2f ? 0.6f + t * 2 : 1.0f);
      for (int x = (int)std::floor(cx + sway - half); x <= (int)std::ceil(cx + sway + half) - 1; x++)
        c.set(x, y, kFire[layer + 1]);
    }
  }
  // sparks
  for (int i = 0; i < 2; i++) {
    uint32_t hsh = hash3(frame, i, seed);
    int sx = (int)cx - 2 + (int)(hsh % 5), sy = (int)(base - h - 1 - (hsh >> 8) % 4);
    c.set(sx, sy, kFire[3 + (int)(hsh >> 4) % 2]);
  }
}

void torch(Canvas& c, int frame) {
  int cx = c.w / 2;
  // pole + bracket
  for (int y = 9; y < c.h - 1; y++) { c.set(cx - 1, y, kWood[3]); c.set(cx, y, kWood[1]); }
  box(c, cx - 2, 8, cx + 1, 10, kIron[2]);
  hline(c, cx - 2, cx + 1, 8, kIron[3]);
  c.set(cx - 1, c.h - 1, kWood[0]); c.set(cx, c.h - 1, kWood[0]);
  static const float hs[4] = {7.0f, 8.0f, 6.5f, 7.5f};
  flame(c, cx, 8.0f, hs[frame], 5.0f, frame, 3);
}

void campfire(Canvas& c, int frame) {
  float cx = c.w * 0.5f, by = c.h - 3.0f;
  // ring of stones
  for (int i = 0; i < 9; i++) {
    float a = PI * (0.05f + i / 8.0f * 0.9f);
    float sx = cx + std::cos(a) * 7.5f, sy = by - 1 + std::sin(a) * 2.0f;
    ball(c, sx, sy, 2.0f, 1.6f, kStone);
  }
  // logs crossed
  capsule(c, V(cx - 6, by - 1), V(cx + 5, by - 3), 1.4f, 1.2f, kWood);
  capsule(c, V(cx + 6, by - 1), V(cx - 5, by - 3), 1.4f, 1.2f, kWoodDark);
  c.set((int)cx + 5, (int)by - 3, kWood[4]); c.set((int)cx - 6, (int)by - 1, kWood[4]);
  // embers
  for (int x = (int)cx - 3; x <= (int)cx + 3; x++) c.set(x, (int)by - 2, kFire[(x + frame) % 3 == 0 ? 3 : 1]);
  static const float hs[4] = {11.0f, 13.0f, 10.5f, 12.5f};
  flame(c, cx, by - 2.0f, hs[frame], 9.0f, frame, 11);
  // stones in front of the fire
  for (int i = 0; i < 3; i++) ball(c, cx - 4 + i * 4, by + 1, 2.1f, 1.5f, kStone);
}

void signpost(Canvas& c) {
  int cx = c.w / 2;
  for (int y = 4; y < c.h - 1; y++) { c.set(cx - 1, y, kWood[3]); c.set(cx, y, kWood[1]); }
  // two arrow boards
  auto board = [&](int y, int dir, int len) {
    int x0 = dir > 0 ? cx - 2 : cx - len + 1;
    for (int j = 0; j < 4; j++)
      for (int i = 0; i < len; i++) {
        int x = x0 + i;
        bool tipZone = dir > 0 ? (i >= len - 2) : (i <= 1);
        if (tipZone && (j == 0 || j == 3) && ((dir > 0 && i == len - 1) || (dir < 0 && i == 0))) continue;
        int k = j == 0 ? 4 : (j == 3 ? 1 : 3);
        c.set(x, y + j, kWood[k]);
      }
    // writing marks
    for (int i = 2; i < len - 3; i += 2) c.set(x0 + i + (dir < 0 ? 1 : 0), y + 1 + (i % 4 == 0), kWood[0]);
  };
  board(3, 1, 11);
  board(9, -1, 10);
  c.set(cx - 1, c.h - 1, kWood[0]);
  grassTuft(c, cx - 2, c.h - 1, 9, 3);
}

void well(Canvas& c) {
  float cx = c.w * 0.5f;
  int base = c.h - 2;
  // stone ring: back rim, water, front wall
  ellipse(c, cx, base - 8, 11.5f, 4.5f, kStone[3]);
  ellipse(c, cx, base - 8, 8.5f, 3.0f, kWater[0]);
  ellipse(c, cx + 1, base - 7.5f, 6.0f, 1.6f, kWater[1]);
  c.set((int)cx - 2, base - 8, kWater[3]); c.set((int)cx - 1, base - 8, kWater[3]);
  for (int y = base - 8; y <= base; y++)
    for (int x = (int)(cx - 11.5f); x <= (int)(cx + 11.5f); x++) {
      float dx = (x + 0.5f - cx) / 11.5f;
      float yy = base - 8 + std::sqrt(std::max(0.0f, 1 - dx * dx)) * 4.5f;
      if (y < yy) continue;
      int row = (int)(y - yy) / 3;
      int brick = (x + row * 3) / 5;
      int k = lightIndex(lightAt(dx * 0.85f, 0.1f), x, y, 0);
      if (((x + row * 3) % 5) == 0 || (int)(y - yy) % 3 == 0) k = std::max(0, k - 1);
      (void)brick;
      c.set(x, y, kStone[k]);
    }
  // posts + roof
  for (int y = 6; y <= base - 8; y++) { c.set((int)cx - 10, y, kWood[3]); c.set((int)cx - 9, y, kWood[1]); c.set((int)cx + 9, y, kWood[2]); c.set((int)cx + 10, y, kWood[0]); }
  hline(c, (int)cx - 9, (int)cx + 9, 8, kWood[2]);   // crossbar
  vline(c, (int)cx, 9, base - 13, kCloth[2]);           // rope
  box(c, (int)cx - 1, base - 13, (int)cx + 1, base - 11, kWood[2]);   // bucket
  hline(c, (int)cx - 1, (int)cx + 1, base - 13, kIron[3]);
  for (int y = 0; y < 7; y++)
    for (int x = -13 + (6 - y); x <= 13 - (6 - y); x++) {
      int k = y < 2 ? 4 : (y < 5 ? 3 : 1);
      if ((x + 13) % 3 == 0 && y > 1) k--;
      c.set((int)cx + x, y + 1, kBrick[k]);
    }
  hline(c, (int)cx - 13, (int)cx + 13, 7, kBrick[0]);
}

void fence(Canvas& c, bool vertical) {
  if (!vertical) {
    int base = c.h - 2;
    for (int px2 : {1, c.w - 3}) {
      for (int y = base - 11; y <= base; y++) { c.set(px2, y, kWood[3]); c.set(px2 + 1, y, kWood[1]); }
      c.set(px2, base - 11, kWood[4]); c.set(px2 + 1, base - 11, kWood[2]);
    }
    for (int ry : {base - 9, base - 4})
      for (int x = 0; x < c.w; x++) {
        if (x == 1 || x == 2 || x == c.w - 3 || x == c.w - 2) continue;
        c.set(x, ry, kWood[3]); c.set(x, ry + 1, kWood[1]);
      }
  } else {
    // a run going north: two rails seen end-on stack into a pair of planks rising from the post
    int cx = c.w / 2, base = c.h - 2;
    for (int y = 0; y < base - 6; y++) {
      c.set(cx - 2, y, kWood[3]); c.set(cx - 1, y, kWood[2]);
      c.set(cx + 1, y, kWood[2]); c.set(cx + 2, y, kWood[1]);
      if (y % 6 == 3) { c.set(cx - 2, y, kWood[4]); c.set(cx + 1, y, kWood[3]); }   // plank joints catch light
    }
    for (int y = base - 12; y <= base; y++) { c.set(cx - 1, y, kWood[3]); c.set(cx, y, kWood[2]); c.set(cx + 1, y, kWood[1]); }
    hline(c, cx - 1, cx + 1, base - 12, kWood[4]);
    grassTuft(c, cx - 2, base, 31, 3);
  }
}

void lamppost(Canvas& c) {
  int cx = c.w / 2;
  for (int y = 9; y < c.h - 2; y++) { c.set(cx - 1, y, kIron[2]); c.set(cx, y, kIron[1]); }
  box(c, cx - 2, c.h - 3, cx + 1, c.h - 2, kIron[1]);
  hline(c, cx - 2, cx + 1, c.h - 3, kIron[2]);
  // lantern
  hline(c, cx - 3, cx + 2, 2, kIron[2]); hline(c, cx - 2, cx + 1, 1, kIron[3]);
  for (int y = 3; y <= 7; y++) {
    c.set(cx - 3, y, kIron[1]); c.set(cx + 2, y, kIron[0]);
    for (int x = cx - 2; x <= cx + 1; x++) c.set(x, y, kGlow[y < 5 ? 4 : 3]);
  }
  c.set(cx - 1, 5, kWhite);
  hline(c, cx - 3, cx + 2, 8, kIron[1]);
}

void tent(Canvas& c) {
  const Ramp Cv = kCloth;
  float cx = c.w * 0.5f;
  int base = c.h - 2, top = 3;
  // A-frame: left face lit, right face shaded, dark triangular opening at the front
  for (int y = top; y <= base; y++) {
    float t = (float)(y - top) / (base - top);
    int half = (int)(1 + t * (c.w * 0.5f - 2));
    for (int x = (int)cx - half; x <= (int)cx + half; x++) {
      int k = x < cx ? 3 : 1;
      if (x == (int)cx - half) k = 2;
      if ((x + y) % 6 == 0) k = std::max(0, k - 1);    // fabric seams
      c.set(x, y, Cv[k]);
    }
  }
  for (int y = top + 8; y <= base; y++) {
    int half = (int)((y - top - 8) * 0.45f);
    for (int x = (int)cx - half; x <= (int)cx + half; x++) c.set(x, y, y > base - 2 ? kInk : rgba(46, 34, 44));
    c.set((int)cx - half - 1, y, Cv[4]);
  }
  vline(c, (int)cx, top - 2, top + 1, kWood[2]);
  // guy ropes + pegs
  line(c, (int)cx - 3, top + 4, 0, base, Cv[1]);
  line(c, (int)cx + 3, top + 4, c.w - 1, base, Cv[0]);
  hline(c, (int)cx - 14, (int)cx + 14, base, Cv[0]);
}

void marketStall(Canvas& c) {
  int W = c.w, base = c.h - 2;
  // posts
  for (int y = 10; y <= base; y++) { c.set(2, y, kWood[3]); c.set(3, y, kWood[1]); c.set(W - 4, y, kWood[2]); c.set(W - 3, y, kWood[0]); }
  // counter with goods
  plankBox(c, 1, base - 11, W - 2, 4, 8, kWood);
  const uint32_t goods[4] = {rgba(220, 60, 50), rgba(250, 180, 60), rgba(120, 180, 70), rgba(160, 90, 160)};
  for (int i = 0; i < 6; i++) {
    float gx = 6 + i * 5.3f, gy = base - 11.5f;
    Ramp g = ramp(goods[i % 4], 0.8f);
    ball(c, gx, gy, 2.0f, 1.6f, g);
    ball(c, gx + 1.5f, gy - 1, 1.4f, 1.2f, g);
  }
  // striped awning
  for (int y = 1; y <= 10; y++)
    for (int x = 0; x < W; x++) {
      bool red = ((x / 4) & 1) == 0;
      Ramp r = red ? kRed : kCloth;
      int k = y < 3 ? 4 : (y < 8 ? 3 : 2);
      if (y == 10) k = 1;
      c.set(x, y, r[k]);
    }
  // scalloped edge
  for (int x = 0; x < W; x++) if ((x % 4) == 1 || (x % 4) == 2) c.set(x, 11, ((x / 4) & 1) == 0 ? kRed[1] : kCloth[1]);
}

void haystack(Canvas& c) {
  float cx = c.w * 0.5f;
  int base = c.h - 2;
  for (int y = 1; y <= base; y++)
    for (int x = 0; x < c.w; x++) {
      float dx = (x + 0.5f - cx) / (c.w * 0.48f), dy = (y + 0.5f - base) / (base - 1.0f);
      if (dx * dx + dy * dy > 1) continue;
      float l = lightAt(dx * 0.85f, dy * 0.85f) + (hashf(x, y / 2, 3) - 0.5f) * 0.3f;
      int k = lightIndex(l, x, y, 0.12f);
      if (hash3(x, y, 4) % 5 == 0) k = std::max(0, k - 1);   // straw strands
      c.set(x, y, kThatch[k]);
    }
  for (int i = 0; i < 9; i++) {   // stray straws
    int x = 2 + (int)(hash3(i, 0, 7) % (c.w - 4)), y = 3 + (int)(hash3(i, 1, 7) % (base - 4));
    if (solid(c, x, y)) { c.set(x, y, kThatch[4]); c.set(x + 1, y - 1, kThatch[3]); }
  }
}

void anvil(Canvas& c) {
  int cx = c.w / 2, base = c.h - 2;
  // stump
  for (int y = base - 5; y <= base; y++)
    for (int x = cx - 5; x <= cx + 4; x++) c.set(x, y, kWood[x < cx - 2 ? 3 : (x > cx + 2 ? 1 : 2)]);
  hline(c, cx - 5, cx + 4, base - 5, kWood[4]);
  // anvil body: horn to the right
  box(c, cx - 3, base - 8, cx + 2, base - 6, kIron[1]);
  box(c, cx - 6, base - 11, cx + 4, base - 9, kIron[2]);
  hline(c, cx - 6, cx + 4, base - 11, kIron[4]);
  hline(c, cx - 5, cx + 3, base - 10, kIron[3]);
  c.set(cx + 5, base - 10, kIron[2]); c.set(cx + 6, base - 10, kIron[2]); c.set(cx + 7, base - 9, kIron[1]); c.set(cx + 5, base - 11, kIron[3]);
  box(c, cx - 4, base - 6, cx + 3, base - 5, kIron[1]);
}

void cart(Canvas& c) {
  int base = c.h - 2;
  // bed with sides and a load of sacks
  plankBox(c, 3, base - 15, c.w - 8, 4, 7, kWood);
  for (int x = 3; x < c.w - 5; x += 4) vline(c, x, base - 11, base - 5, kWood[1]);
  for (int i = 0; i < 3; i++) ball(c, 9 + i * 7.0f, base - 16.0f, 3.6f, 3.0f, kCloth);
  c.set(9, base - 19, kCloth[4]);
  // shafts to the right
  line(c, c.w - 5, base - 8, c.w - 1, base - 6, kWood[2]);
  // wheels
  for (int wx : {9, c.w - 13}) {
    for (int y = -5; y <= 5; y++)
      for (int x = -5; x <= 5; x++) {
        int d2 = x * x + y * y;
        if (d2 > 26) continue;
        if (d2 >= 15) c.set(wx + x, base - 4 + y, kWood[d2 > 22 ? 1 : 3]);
        else if (x == 0 || y == 0 || x == y || x == -y) c.set(wx + x, base - 4 + y, kWood[2]);
      }
    c.set(wx, base - 4, kIron[3]);
  }
}

void fountain(Canvas& c, int frame) {
  float cx = c.w * 0.5f;
  int base = c.h - 2;
  // basin: back rim, water, front wall
  ellipse(c, cx, base - 9, 19.0f, 6.5f, kStone[3]);
  ellipse(c, cx, base - 9, 16.0f, 5.0f, kWater[1]);
  for (int i = 0; i < 6; i++) {   // ripples move outward per frame
    float r = 3.0f + ((i * 3 + frame * 2) % 13);
    for (int a = 0; a < 40; a++) {
      float ang = a / 40.0f * TAU;
      int x = (int)(cx + std::cos(ang) * r * 1.3f), y = (int)(base - 9 + std::sin(ang) * r * 0.38f);
      if (c.get(x, y) == kWater[1] && (a + i) % 3 == 0) c.set(x, y, kWater[2]);
    }
  }
  for (int y = base - 9; y <= base; y++)
    for (int x = (int)(cx - 19); x <= (int)(cx + 19); x++) {
      float dx = (x + 0.5f - cx) / 19.0f;
      float yy = base - 9 + std::sqrt(std::max(0.0f, 1 - dx * dx)) * 6.5f - 1;
      if (y < yy) continue;
      int k = lightIndex(lightAt(dx * 0.85f, 0.1f), x, y, 0);
      if (y - (int)yy == 0) k = 4;
      if ((x % 6) == 0 && y - yy > 1) k = std::max(0, k - 1);
      c.set(x, y, kStone[k]);
    }
  // central pillar + upper bowl
  cylinder(c, (int)cx - 2, base - 22, (int)cx + 1, base - 8, kStone);
  ellipse(c, cx, base - 21, 6.0f, 2.0f, kStone[3]);
  ellipse(c, cx, base - 21, 4.5f, 1.2f, kWater[2]);
  hline(c, (int)cx - 5, (int)cx + 4, base - 19, kStone[1]);
  // spout + falling arcs
  vline(c, (int)cx, base - 27 + (frame & 1), base - 22, kWater[4]);
  for (int side = -1; side <= 1; side += 2)
    for (int i = 0; i < 9; i++) {
      float t = i / 8.0f;
      int x = (int)(cx + side * (2 + t * 7)), y = (int)(base - 26 + t * t * 14 - (1 - t) * 2);
      if ((i + frame) % 4 == 0) continue;
      c.set(x, y, kWater[(i + frame) % 2 ? 4 : 3]);
    }
  for (int side = -1; side <= 1; side += 2) { c.set((int)cx + side * 9, base - 11 + (frame % 2), kWater[4]); c.set((int)cx + side * 10, base - 12, kWhite); }
}

void statue(Canvas& c) {
  int cx = c.w / 2, base = c.h - 2;
  plankBox(c, cx - 8, base - 9, 16, 3, 7, kStone);
  // stone knight leaning on a sword
  const Ramp& S = kStoneWarm;
  HumanLook l;
  l.outfit = Outfit::Plate; l.helmet = true; l.cape = true; l.weapon = 1;
  Canvas fig(HUMAN_W, HUMAN_H);
  HumanPainter hp(fig, l, kDown, Pose{});
  hp.paint();
  // recolour the figure to stone by luminance
  for (auto& p : fig.px)
    if (chA(p)) { float lu = luma(p); p = S[std::clamp((int)(lu * 5.2f), 0, 4)]; }
  blit(c, fig, cx - 8, base - 9 - HUMAN_H + 1);
}

void banner(Canvas& c, int frame) {
  int base = c.h - 1;
  for (int y = 2; y <= base; y++) { c.set(2, y, kWood[3]); c.set(3, y, kWood[1]); }
  c.set(2, 1, kGold[4]); c.set(3, 1, kGold[2]); c.set(2, 0, kGold[3]);
  hline(c, 3, c.w - 2, 3, kWood[2]);
  // cloth hanging from the crossbar, rippling
  for (int y = 4; y < 22; y++) {
    float t = (y - 4) / 18.0f;
    int off = (int)std::lround(std::sin(t * 4.0f + frame * 1.6f) * 1.2f * t);
    for (int x = 4; x < c.w - 1; x++) {
      int k = 2;
      float wave = std::sin((x - 4) * 0.9f + frame * 1.6f + t * 2);
      if (wave > 0.5f) k = 3;
      if (wave < -0.5f) k = 1;
      int yy = y;
      if (y > 18) {   // swallow-tail end
        int mid = (4 + c.w - 2) / 2;
        if (std::abs(x - mid) < (y - 18)) continue;
      }
      c.set(x + off, yy, kRed[k]);
    }
  }
  // emblem
  int ex = (4 + c.w - 2) / 2;
  c.set(ex, 9, kGold[4]); c.set(ex - 1, 10, kGold[3]); c.set(ex + 1, 10, kGold[2]); c.set(ex, 10, kGold[3]); c.set(ex, 11, kGold[2]); c.set(ex, 12, kGold[1]);
}

void woodpile(Canvas& c) {
  int base = c.h - 2;
  const int rows[3] = {4, 3, 2};
  for (int r = 0; r < 3; r++)
    for (int i = 0; i < rows[r]; i++) {
      float x = 4.0f + i * 5.2f + r * 2.6f, y = base - 2.5f - r * 4.2f;
      // log end: bark ring + rings
      ellipse(c, x, y, 2.6f, 2.3f, kBark[1]);
      ellipse(c, x - 0.2f, y - 0.2f, 1.8f, 1.6f, kWood[3]);
      c.set((int)x, (int)y, kWood[2]);
      c.set((int)x - 1, (int)y - 1, kWood[4]);
    }
}

void gravestone(Canvas& c) {
  int cx = c.w / 2, base = c.h - 2;
  for (int y = 2; y <= base; y++)
    for (int x = cx - 5; x <= cx + 4; x++) {
      int j = y - 2;
      if (j < 3) { float dx = (x + 0.5f - cx) / 5.0f; if (dx * dx + (3 - j) * (3 - j) / 9.0f > 1.0f) continue; }
      int k = x == cx - 5 ? 3 : (x >= cx + 3 ? 1 : 2);
      if (j == 0) k = 3;
      c.set(x, y, kStone[k]);
    }
  // engraved cross
  vline(c, cx, 5, 10, kStone[0]); hline(c, cx - 2, cx + 1, 7, kStone[0]);
  vline(c, cx + 1, 6, 10, kStone[3]);
  c.set(cx - 4, 4, kMoss[2]); c.set(cx - 4, 5, kMoss[1]); c.set(cx + 3, base - 1, kMoss[2]);
  grassTuft(c, cx - 4, base, 21, 4);
}

void shrine(Canvas& c) {
  int cx = c.w / 2, base = c.h - 2;
  plankBox(c, cx - 10, base - 8, 20, 3, 6, kStone);
  // pillars + roof
  for (int px2 : {cx - 9, cx + 7}) cylinder(c, px2, 10, px2 + 2, base - 8, kStone);
  for (int y = 3; y <= 9; y++) {
    int half = 6 + (y - 3) * 1;
    for (int x = cx - half; x <= cx + half - 1; x++) c.set(x, y, kStone[y < 5 ? 4 : (y < 8 ? 3 : 1)]);
  }
  hline(c, cx - 12, cx + 11, 10, kStone[0]);
  // glowing idol / gem
  ball(c, cx - 0.5f, base - 12.0f, 2.5f, 3.0f, kCrystal);
  c.set(cx - 1, base - 14, kWhite);
  for (int i = 0; i < 3; i++) c.set(cx - 5 + i * 4, base - 9, kGlow[4]);   // candles
  for (int i = 0; i < 3; i++) c.set(cx - 5 + i * 4, base - 8, kCloth[3]);
}

// ---- dungeon ----------------------------------------------------------------------------------------
void caveEntrance(Canvas& c) {
  const int W = c.w, H = c.h;
  rock(c, W * 0.5f, H * 0.52f, W * 0.5f, H * 0.5f, kStone, 31, 11);
  rock(c, W * 0.16f, H * 0.7f, 8.0f, 9.0f, kStone, 32, 5);
  rock(c, W * 0.85f, H * 0.68f, 8.0f, 9.5f, kStone, 33, 5);
  // the dark opening: arched, fading to black inside
  float ox = W * 0.5f, ow = 11.0f, top = H * 0.32f;
  for (int y = (int)top; y < H; y++)
    for (int x = (int)(ox - ow); x <= (int)(ox + ow); x++) {
      float dx = (x + 0.5f - ox) / ow;
      float arch = top + (1 - std::sqrt(std::max(0.0f, 1 - dx * dx))) * 8.0f;
      if (y < arch) continue;
      float depth = (y - arch) / (H - arch);
      uint32_t col = depth < 0.15f ? rgba(40, 32, 50) : rgba(14, 10, 20);
      if (std::fabs(dx) > 0.82f) col = rgba(52, 44, 62);
      c.set(x, y, col);
    }
  // stalactite teeth on the arch
  for (int i = -3; i <= 3; i++) {
    int x = (int)ox + i * 3;
    float dx = (x + 0.5f - ox) / ow;
    int ay = (int)(top + (1 - std::sqrt(std::max(0.0f, 1 - dx * dx))) * 8.0f);
    for (int k = 0; k < 2 + (i & 1); k++) c.set(x, ay + k, kStone[k == 0 ? 2 : 1]);
  }
  // moss tufts on top
  for (int x = 4; x < W - 4; x += 3)
    for (int y = 0; y < H; y++) if (solid(c, x, y)) { if (hash3(x, 0, 3) % 2) { c.set(x, y, kMoss[3]); c.set(x + 1, y, kMoss[2]); c.set(x, y + 1, kMoss[1]); } break; }
}

void ladder(Canvas& c) {
  for (int y = 0; y < c.h - 1; y++) { c.set(3, y, kWood[3]); c.set(4, y, kWood[1]); c.set(11, y, kWood[2]); c.set(12, y, kWood[0]); }
  for (int y = 2; y < c.h - 1; y += 4) { hline(c, 5, 10, y, kWood[3]); hline(c, 5, 10, y + 1, kWood[1]); }
  // light from above
  for (int x = 5; x <= 10; x++) c.set(x, 0, withA(kGlow[4], 160));
}

void stalagmite(Canvas& c) {
  int cx = c.w / 2, base = c.h - 2;
  for (int y = 1; y <= base; y++) {
    float t = (float)(y - 1) / (base - 1);
    float half = 0.6f + t * t * (c.w * 0.45f);
    for (int x = (int)(cx - half); x <= (int)(cx + half); x++) {
      float u = (x + 0.5f - cx) / std::max(0.6f, half);
      int k = lightIndex(lightAt(u * 0.9f, -0.2f) + (hashf(x, y / 3, 5) - 0.5f) * 0.25f, x, y, 0.1f);
      if (y % 5 == 0 && k > 0) k--;
      c.set(x, y, kStoneWarm[k]);
    }
  }
  // small sibling
  for (int y = base - 7; y <= base; y++) {
    float half = (y - (base - 7)) * 0.4f;
    for (int x = (int)(cx + 4 - half); x <= (int)(cx + 4 + half); x++) c.set(x, y, kStoneWarm[x < cx + 4 ? 3 : 1]);
  }
}

void crystalCluster(Canvas& c) {
  int base = c.h - 2;
  struct Sh { float x, h, w, lean; };
  Sh sh[5] = {{8, 19, 3.2f, 0.0f}, {4, 12, 2.6f, -0.35f}, {12, 13, 2.6f, 0.35f}, {6, 8, 2.0f, -0.6f}, {11, 7, 2.0f, 0.6f}};
  for (int i = 4; i >= 0; i--) {
    const Sh& s = sh[i];
    Vec2 b{s.x, (float)base}, t{s.x + s.lean * s.h, base - s.h};
    Vec2 dir = norm(t - b), perp{-dir.y, dir.x};
    std::vector<Vec2> left = {b + perp * -s.w, t + perp * -s.w * 0.7f, t + dir * 2.5f, t, b};
    std::vector<Vec2> right = {b, t, t + dir * 2.5f, t + perp * s.w * 0.7f, b + perp * s.w};
    poly(c, left, kCrystal[3]);
    poly(c, right, kCrystal[1]);
    line(c, (int)b.x, (int)b.y, (int)t.x, (int)t.y, kCrystal[4]);
  }
  c.set(7, 3, kWhite); c.set(3, base - 9, kWhite);
}

void bones(Canvas& c) {
  int base = c.h - 2;
  capsule(c, V(2, base - 1), V(10, base - 3), 0.8f, 0.8f, kBone);
  ball(c, 2, base - 1.5f, 1.3f, 1.3f, kBone); ball(c, 10, base - 3.5f, 1.3f, 1.3f, kBone);
  capsule(c, V(8, base), V(15, base - 1), 0.7f, 0.7f, kBone, -1);
  ball(c, 13, base - 5.0f, 3.0f, 2.6f, kBone);
  c.set(12, base - 5, kInk); c.set(14, base - 5, kInk); c.set(13, base - 3, kBone[1]);
  for (int i = 0; i < 3; i++) c.set(4 + i * 2, base, kBone[3]);
}

void skullPile(Canvas& c) {
  int base = c.h - 2;
  const float sk[6][2] = {{4, 0}, {10, 0}, {16, 0}, {7, -4}, {13, -4}, {10, -8}};
  for (auto& s : sk) {
    float x = s[0], y = base - 3 + s[1];
    ball(c, x, y, 3.0f, 2.8f, kBone);
    c.set((int)x - 1, (int)y, kInk); c.set((int)x + 1, (int)y, kInk);
    c.set((int)x, (int)y + 2, kBone[1]);
    c.set((int)x - 1, (int)y - 2, kBone[4]);
  }
}

void cobweb(Canvas& c) {
  uint32_t silk = withA(rgba(230, 232, 240), 170), silk2 = withA(rgba(210, 214, 228), 120);
  // corner web anchored top-left
  for (int i = 0; i < 5; i++) {
    float a = i / 4.0f * PI * 0.5f;
    line(c, 0, 0, (int)(std::cos(a) * 15), (int)(std::sin(a) * 15), silk);
  }
  for (int r = 4; r <= 14; r += 4)
    for (int i = 0; i < 4; i++) {
      float a0 = i / 4.0f * PI * 0.5f, a1 = (i + 1) / 4.0f * PI * 0.5f;
      float sag = 0.85f;
      int x0 = (int)(std::cos(a0) * r), y0 = (int)(std::sin(a0) * r), x1 = (int)(std::cos(a1) * r), y1 = (int)(std::sin(a1) * r);
      int mx = (int)((x0 + x1) * 0.5f * sag), my = (int)((y0 + y1) * 0.5f * sag);
      line(c, x0, y0, mx, my, silk2);
      line(c, mx, my, x1, y1, silk2);
    }
}

void brazier(Canvas& c, int frame) {
  int cx = c.w / 2, base = c.h - 1;
  // tripod legs
  line(c, cx - 1, base - 7, cx - 5, base, kIron[2]);
  line(c, cx, base - 7, cx + 4, base, kIron[1]);
  vline(c, cx - 1, base - 7, base - 1, kIron[1]);
  // bowl
  for (int y = 0; y < 4; y++) {
    int half = 6 - y;
    for (int x = cx - half; x < cx + half; x++) c.set(x, base - 11 + y, kIron[x < cx - 2 ? 3 : (x < cx + 2 ? 2 : 1)]);
  }
  hline(c, cx - 6, cx + 5, base - 11, kIron[4]);
  for (int x = cx - 5; x < cx + 5; x++) c.set(x, base - 12, kFire[1 + ((x + frame) & 1)]);
  static const float hs[4] = {8.0f, 9.5f, 7.5f, 9.0f};
  flame(c, cx, base - 12.0f, hs[frame], 8.0f, frame, 17);
}

void coffin(Canvas& c) {
  // lying N-S, seen from above-front: hexagonal lid + front face
  int cx = c.w / 2;
  std::vector<Vec2> lid = {{cx - 3.0f, 1}, {cx + 3.0f, 1}, {cx + 7.0f, 7}, {cx + 5.0f, 22}, {cx - 5.0f, 22}, {cx - 7.0f, 7}};
  poly(c, lid, kWoodDark[3]);
  std::vector<Vec2> right = {{cx + 1.0f, 1}, {cx + 3.0f, 1}, {cx + 7.0f, 7}, {cx + 5.0f, 22}, {cx + 1.0f, 22}};
  poly(c, right, kWoodDark[2]);
  for (int y = 22; y < c.h - 1; y++) for (int x = cx - 5; x < cx + 5; x++) c.set(x, y, kWoodDark[x < cx ? 1 : 0]);
  vline(c, cx - 1, 5, 17, kGold[3]); hline(c, cx - 3, cx + 1, 8, kGold[3]); vline(c, cx, 6, 17, kGold[1]);
}

void urn(Canvas& c) {
  const Ramp Cl = ramp5(rgba(84, 40, 40), rgba(132, 66, 48), rgba(178, 98, 62), rgba(210, 138, 86), rgba(234, 178, 120));
  float cx = c.w * 0.5f;
  int base = c.h - 2;
  for (int y = 2; y <= base; y++) {
    float t = (float)(y - 2) / (base - 2);
    float hw = t < 0.2f ? 2.5f : 2.0f + std::sin((t - 0.1f) * PI) * 3.5f;
    if (t > 0.92f) hw = 3.0f;
    for (int x = (int)(cx - hw); x < (int)std::ceil(cx + hw); x++) {
      float u = (x + 0.5f - cx) / hw;
      c.set(x, y, Cl[lightIndex(lightAt(u * 0.9f, (t - 0.5f) * 0.6f), x, y, 0.08f)]);
    }
  }
  ellipse(c, cx, 2.5f, 2.6f, 1.0f, Cl[0]);
  hline(c, (int)cx - 2, (int)cx + 2, 1, Cl[3]);
  for (int x = (int)cx - 4; x <= (int)cx + 4; x++) if (solid(c, x, 8) && x % 2) c.set(x, 8, kStone[4]);   // painted band
  for (int x = (int)cx - 4; x <= (int)cx + 4; x++) if (solid(c, x, 10)) c.set(x, 10, Cl[1]);
}

void ironDoor(Canvas& c) {
  // stone frame + iron-banded door
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      bool frame = x < 2 || x > c.w - 3 || y < 2;
      if (!frame) continue;
      int k = ((x + (y / 3) * 2) % 4 == 0) ? 1 : (x < 2 ? 3 : 2);
      c.set(x, y, kStone[k]);
    }
  for (int y = 2; y < c.h - 1; y++)
    for (int x = 2; x < c.w - 2; x++) {
      int k = ((x - 2) % 3 == 2) ? 0 : 1;
      c.set(x, y, kWoodDark[k + (x < 5 ? 1 : 0)]);
    }
  for (int by : {5, 12, 19}) for (int x = 2; x < c.w - 2; x++) c.set(x, by, kIron[x < 5 ? 3 : 2]);
  for (int by : {5, 12, 19}) for (int x = 3; x < c.w - 2; x += 4) c.set(x, by, kIron[4]);
  c.set(c.w - 5, 13, kIron[4]); c.set(c.w - 5, 14, kIron[1]);
  hline(c, 0, c.w - 1, c.h - 1, kStone[0]);
}

void altar(Canvas& c) {
  int base = c.h - 2;
  plankBox(c, 1, base - 13, c.w - 2, 5, 9, kStone);
  // cloth draped over the top and front
  for (int y = base - 13; y < base - 5; y++)
    for (int x = 7; x < c.w - 7; x++) {
      bool front = y >= base - 8;
      c.set(x, y, front ? kRed[(x < 10 ? 2 : 1)] : kRed[3]);
    }
  for (int x = 7; x < c.w - 7; x += 2) c.set(x, base - 5, kGold[3]);
  // candles + relic
  for (int x : {4, c.w - 6}) { vline(c, x, base - 17, base - 14, kCloth[3]); c.set(x, base - 18, kGlow[4]); c.set(x, base - 19, kFire[3]); }
  ball(c, c.w * 0.5f, base - 15.0f, 2.2f, 2.0f, kGold);
  c.set(c.w / 2 - 1, base - 16, kWhite);
}

// ---- interior furniture -----------------------------------------------------------------------------
void bed(Canvas& c) {
  int W = c.w;
  // headboard
  for (int y = 0; y < 7; y++) for (int x = 0; x < W; x++) c.set(x, y, kWood[y == 0 ? 4 : (x < 2 ? 3 : (x > W - 3 ? 1 : 2))]);
  for (int x = 3; x < W - 3; x += 4) vline(c, x, 2, 5, kWood[1]);
  // mattress, pillow, blanket
  box(c, 1, 7, W - 2, c.h - 4, kCloth[3]);
  for (int y = 8; y < 13; y++) for (int x = 3; x < W - 3; x++) c.set(x, y, y == 8 ? kWhite : (x > W - 6 ? kCloth[2] : kCloth[4]));
  for (int y = 14; y < c.h - 4; y++)
    for (int x = 1; x < W - 1; x++) {
      int k = x < 3 ? 3 : (x > W - 4 ? 1 : 2);
      if (y == 14) k = 4;
      if (((x / 3) + (y / 3)) % 2 == 0 && k == 2) k = 3;   // quilt pattern
      c.set(x, y, ramp(rgba(70, 110, 170))[k]);
    }
  // footboard
  for (int y = c.h - 4; y < c.h - 1; y++) for (int x = 0; x < W; x++) c.set(x, y, kWood[y == c.h - 4 ? 3 : 1]);
}

void table(Canvas& c) {
  int W = c.w, base = c.h - 2;
  plankBox(c, 0, 1, W, 9, 3, kWood);
  for (int x = 4; x < W; x += 8) vline(c, x, 1, 9, kWood[2]);
  for (int lx : {1, W - 4}) for (int y = 13; y <= base; y++) { c.set(lx, y, kWood[2]); c.set(lx + 1, y, kWood[1]); c.set(lx + 2, y, kWood[0]); }
  // mug + plate
  ellipse(c, 10, 5, 3.5f, 2.0f, kBone[4]); ellipse(c, 10, 5, 2.0f, 1.0f, kBone[2]);
  box(c, 20, 3, 22, 6, kWood[1]); hline(c, 20, 22, 3, kCloth[4]); c.set(23, 4, kWood[1]);
}

void chair(Canvas& c) {
  int W = c.w, base = c.h - 2;
  for (int y = 0; y < 8; y++) { c.set(1, y, kWood[3]); c.set(W - 2, y, kWood[1]); }
  for (int y = 1; y < 7; y += 2) hline(c, 2, W - 3, y, kWood[2]);
  plankBox(c, 1, 8, W - 2, 3, 2, kWood);
  for (int y = 13; y <= base; y++) { c.set(1, y, kWood[2]); c.set(W - 2, y, kWood[0]); }
}

void shelfWithJars(Canvas& c) {
  int W = c.w;
  for (int y = 0; y < c.h - 1; y++) { c.set(0, y, kWood[3]); c.set(W - 1, y, kWood[1]); }
  for (int s = 0; s < 3; s++) {
    int sy = 8 + s * 9;
    hline(c, 0, W - 1, sy, kWood[3]); hline(c, 0, W - 1, sy + 1, kWood[1]);
    for (int i = 0; i < 4; i++) {
      int x = 3 + i * 5 + (int)(hash3(i, s, 9) % 2);
      uint32_t col = (i + s) % 3 == 0 ? rgba(120, 170, 200) : ((i + s) % 3 == 1 ? rgba(180, 110, 70) : rgba(130, 170, 90));
      Ramp r = ramp(col, 0.8f);
      int h = 4 + (int)(hash3(i, s, 4) % 3);
      for (int y = sy - h; y < sy; y++) { c.set(x, y, r[3]); c.set(x + 1, y, r[2]); c.set(x + 2, y, r[1]); }
      hline(c, x, x + 2, sy - h - 1, kWood[2]);
      c.set(x, sy - h + 1, r[4]);
    }
  }
}

void fireplace(Canvas& c, int frame) {
  int W = c.w, base = c.h - 2;
  // stone chimney breast
  for (int y = 0; y <= base; y++)
    for (int x = 0; x < W; x++) {
      int row = y / 4;
      bool mortar = (y % 4 == 3) || ((x + (row & 1) * 3) % 6 == 0);
      int k = mortar ? 1 : (x < 3 ? 3 : (x > W - 4 ? 1 : 2));
      if (!mortar && hash3(x / 6, row, 7) % 3 == 0) k++;
      c.set(x, y, kStoneWarm[std::min(k, 4)]);
    }
  // mantel
  for (int x = 0; x < W; x++) { c.set(x, 9, kWood[3]); c.set(x, 10, kWood[2]); c.set(x, 11, kWood[0]); }
  // hearth opening with fire
  for (int y = 13; y <= base; y++)
    for (int x = 6; x < W - 6; x++) {
      float dx = (x + 0.5f - W * 0.5f) / (W * 0.5f - 6);
      if (y < 13 + dx * dx * 4) continue;
      c.set(x, y, y < 16 ? rgba(40, 24, 30) : rgba(22, 14, 22));
    }
  capsule(c, V(10, base - 1), V(W - 10, base - 2), 1.2f, 1.2f, kWood);
  static const float hs[4] = {9.0f, 11.0f, 8.5f, 10.5f};
  flame(c, W * 0.5f - 3, base - 2.0f, hs[frame] * 0.8f, 6.0f, frame, 23);
  flame(c, W * 0.5f + 3, base - 2.0f, hs[(frame + 2) & 3] * 0.7f, 5.0f, frame + 1, 29);
  // warm glow on the stones around the opening
  for (int y = 12; y <= base; y++)
    for (int x = 3; x < W - 3; x++) {
      uint32_t p = c.get(x, y);
      bool stone = false;
      for (int k = 0; k < 5; k++) if (p == kStoneWarm[k]) stone = true;
      if (stone && (x < 8 || x > W - 9)) c.set(x, y, mix(p, kFire[2], 0.25f));
    }
}

void rug(Canvas& c) {
  int W = c.w, H = c.h;
  for (int y = 1; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++) {
      int bx = std::min(x - 1, W - 2 - x), by = std::min(y - 1, H - 2 - y);
      int b = std::min(bx, by);
      uint32_t col;
      if (b == 0) col = kRed[1];
      else if (b == 1) col = kGold[2];
      else if (b == 2) col = kRed[2];
      else {
        int dx = std::abs(x - W / 2), dy = std::abs(y - H / 2);
        col = ((dx + dy * 2) % 6 < 2) ? kGold[3] : (((dx + dy * 2) % 6 < 3) ? kRed[3] : kRed[2]);
        if (dx + dy * 2 < 4) col = kGold[4];
      }
      c.set(x, y, col);
    }
  for (int y = 1; y < H - 1; y += 2) { c.set(0, y, kCloth[3]); c.set(W - 1, y, kCloth[3]); }   // tassels
}

void counter(Canvas& c) {
  int W = c.w;
  plankBox(c, 0, 2, W, 5, c.h - 9, kWood);
  for (int x = 5; x < W - 2; x += 6) vline(c, x, 8, c.h - 4, kWood[1]);
  hline(c, 0, W - 1, 7, kWood[1]);
  // goods on top: scales + coins
  hline(c, 6, 12, 2, kBrass[3]); vline(c, 9, 0, 2, kBrass[2]); c.set(6, 3, kBrass[1]); c.set(12, 3, kBrass[1]);
  c.set(20, 4, kGold[4]); c.set(21, 4, kGold[3]); c.set(20, 5, kGold[2]); c.set(22, 5, kGold[3]);
}

void kegRack(Canvas& c) {
  // a keg lying on a little rack with a brass tap
  int base = c.h - 2;
  for (int x = 1; x < c.w - 1; x++) { c.set(x, base, kWood[1]); c.set(x, base - 1, kWood[2]); }
  for (int y = 2; y <= base - 2; y++)
    for (int x = 1; x < c.w - 1; x++) {
      float v = (y + 0.5f - (base * 0.5f + 0.5f)) / ((base - 3) * 0.5f);
      if (std::fabs(v) > 1) continue;
      int k = lightIndex(lightAt(0, v * 0.9f), x, y, 0);
      if ((y - 2) % 3 == 0) k = std::max(0, k - 1);
      c.set(x, y, kWood[k]);
    }
  for (int y = 2; y <= base - 2; y++) { c.set(3, y, kIron[2]); c.set(c.w - 4, y, kIron[1]); }
  ellipse(c, c.w - 2.5f, base * 0.5f + 0.5f, 1.5f, (base - 3) * 0.5f, kWood[3]);
  c.set(c.w - 1, base / 2 + 1, kBrass[3]); c.set(c.w - 1, base / 2 + 2, kBrass[1]);
}

void plantPot(Canvas& c) {
  int cx = c.w / 2, base = c.h - 2;
  const Ramp Cl = ramp5(rgba(84, 40, 40), rgba(132, 66, 48), rgba(178, 98, 62), rgba(210, 138, 86), rgba(234, 178, 120));
  for (int y = base - 5; y <= base; y++)
    for (int x = cx - 4 + (y - (base - 5)) / 3; x <= cx + 3 - (y - (base - 5)) / 3; x++) c.set(x, y, Cl[x < cx - 1 ? 3 : (x > cx + 1 ? 1 : 2)]);
  hline(c, cx - 5, cx + 4, base - 6, Cl[4]);
  std::vector<Blob> b = {{cx - 2.5f, base - 11.0f, 3.5f}, {cx + 2.5f, base - 11.0f, 3.5f}, {cx + 0.0f, base - 14.0f, 3.5f}, {cx + 0.0f, base - 9.0f, 3.0f}};
  canopy(c, b, kLeaf, 77, 0.9f);
}

void cauldron(Canvas& c, int frame) {
  float cx = c.w * 0.5f;
  int base = c.h - 1;
  // split logs and a low fire under the pot
  capsule(c, V(cx - 7, base - 1), V(cx + 6, base - 1.5f), 1.1f, 1.0f, kWood);
  flame(c, cx - 3, base - 1.0f, 4.0f + (frame & 1), 3.5f, frame, 41);
  flame(c, cx + 3, base - 1.0f, 4.5f - (frame & 1) * 0.5f, 3.5f, frame + 2, 43);
  flame(c, cx, base - 1.0f, 3.0f + ((frame + 1) & 1), 3.0f, frame + 1, 47);
  // iron pot on stubby legs
  const Ramp K = ramp5(rgba(20, 16, 26), rgba(34, 30, 42), rgba(54, 50, 64), rgba(84, 80, 96), rgba(126, 124, 140));
  vline(c, (int)cx - 5, base - 5, base - 3, K[2]); vline(c, (int)cx + 4, base - 5, base - 3, K[1]);
  ball(c, cx, base - 9.0f, 7.5f, 5.5f, K);
  ellipse(c, cx, base - 13.5f, 7.0f, 2.0f, K[3]);
  ellipse(c, cx, base - 13.5f, 5.6f, 1.3f, rgba(110, 200, 90));
  hline(c, (int)cx - 3, (int)cx + 1, base - 14, rgba(160, 236, 120));
  c.set((int)cx - 5, base - 11, K[4]);
  // bubbles + rising steam
  static const int bx[4][2] = {{-3, 1}, {2, -1}, {-1, 3}, {3, -3}};
  for (int i = 0; i < 2; i++) {
    int x = (int)cx + bx[(frame + i * 2) & 3][i], y = base - 14 - ((frame + i) & 1);
    c.set(x, y, rgba(190, 250, 150)); c.set(x + 1, y, rgba(140, 220, 110));
  }
  for (int i = 0; i < 2; i++) c.set((int)cx - 2 + i * 4, 1 + ((frame + i) % 3), withA(rgba(170, 240, 150), 150));
}

void bookshelf(Canvas& c) {
  int W = c.w, H = c.h;
  for (int y = 0; y < H - 1; y++) for (int x = 0; x < W; x++) c.set(x, y, kWoodDark[1]);
  for (int y = 0; y < H - 1; y++) { c.set(0, y, kWood[3]); c.set(1, y, kWood[2]); c.set(W - 2, y, kWood[1]); c.set(W - 1, y, kWood[0]); }
  hline(c, 0, W - 1, 0, kWood[4]);
  const uint32_t spines[6] = {rgba(150, 40, 46), rgba(50, 90, 150), rgba(60, 120, 70), rgba(170, 130, 50), rgba(110, 60, 130), rgba(140, 90, 60)};
  for (int s = 0; s < 3; s++) {
    int top = 2 + s * 10, bottom = top + 8;
    int x = 2;
    int i = 0;
    while (x < W - 3) {
      int w = 2 + (int)(hash3(i, s, 13) % 2);
      int h = 5 + (int)(hash3(i, s, 17) % 3);
      if (x + w > W - 2) break;
      Ramp r = ramp(spines[hash3(i, s, 19) % 6], 0.8f);
      bool lean = hash3(i, s, 23) % 9 == 0;
      for (int y = bottom - h; y < bottom; y++)
        for (int k = 0; k < w; k++) c.set(x + k + (lean && y < bottom - 3 ? 1 : 0), y, r[k == 0 ? 3 : (k == w - 1 ? 1 : 2)]);
      c.set(x, bottom - h + 1, kGold[3]);
      x += w;
      i++;
    }
    hline(c, 0, W - 1, bottom, kWood[3]); hline(c, 0, W - 1, bottom + 1, kWood[1]);
  }
}

void throne(Canvas& c) {
  int W = c.w, base = c.h - 2;
  // tall back
  for (int y = 1; y < 20; y++)
    for (int x = 3; x < W - 3; x++) {
      int k = x < 5 ? 3 : (x > W - 6 ? 1 : 2);
      c.set(x, y, kGold[k]);
    }
  for (int y = 4; y < 19; y++) for (int x = 6; x < W - 6; x++) c.set(x, y, kRed[x < 9 ? 3 : 2]);
  // crest spikes
  for (int i = 0; i < 3; i++) { int x = 5 + i * 7; c.set(x, 0, kGold[4]); c.set(x + 1, 0, kGold[2]); }
  c.set(W / 2 - 1, 7, kGold[4]); c.set(W / 2, 8, kGold[3]); c.set(W / 2 - 1, 9, kGold[3]);
  // seat + arms
  plankBox(c, 1, 19, W - 2, 4, base - 22, kGold);
  for (int y = 20; y < 23; y++) for (int x = 5; x < W - 5; x++) c.set(x, y, kRed[3]);
  for (int ax : {0, W - 4}) { box(c, ax, 15, ax + 3, 18, kGold[ax == 0 ? 3 : 1]); hline(c, ax, ax + 3, 15, kGold[4]); }
}

// prop sizes ----------------------------------------------------------------------------------------
struct PropInfo { uint8_t w, h, frames; };
const PropInfo kPropInfo[(int)Prop::COUNT] = {
  // nature
  {40, 48, 1}, {36, 44, 1}, {26, 46, 1}, {22, 40, 1}, {26, 46, 1}, {28, 44, 1}, {30, 40, 1}, {40, 46, 1}, {32, 44, 1}, {38, 46, 1},
  {20, 16, 1}, {20, 16, 1}, {20, 16, 1}, {28, 22, 1}, {14, 10, 1}, {18, 14, 1}, {18, 14, 1}, {16, 12, 1}, {28, 12, 1},
  {16, 12, 1}, {16, 12, 1}, {16, 12, 1}, {16, 14, 1}, {16, 20, 1}, {16, 24, 1}, {16, 12, 1}, {16, 10, 1}, {18, 14, 1},
  // camp / civilisation
  {16, 14, 1}, {16, 18, 1}, {14, 18, 1}, {16, 18, 1}, {8, 20, 4}, {20, 20, 4}, {16, 22, 1}, {28, 34, 1}, {16, 16, 1}, {16, 24, 1}, {12, 32, 1}, {36, 28, 1},
  {40, 36, 1}, {24, 20, 1}, {18, 14, 1}, {36, 24, 1}, {40, 32, 4}, {20, 36, 1}, {14, 32, 4}, {24, 16, 1}, {14, 16, 1}, {24, 32, 1},
  // dungeon
  {48, 32, 1}, {16, 24, 1}, {14, 24, 1}, {16, 22, 1}, {18, 10, 1}, {20, 14, 1}, {16, 16, 1}, {16, 22, 4}, {16, 28, 1}, {12, 16, 1}, {16, 24, 1}, {32, 22, 1},
  // interior
  {20, 32, 1}, {32, 20, 1}, {12, 16, 1}, {24, 28, 1}, {32, 32, 4}, {32, 20, 1}, {32, 20, 1}, {16, 16, 1}, {12, 18, 1}, {20, 18, 4}, {28, 32, 1}, {24, 32, 1},
};

void paintProp(Canvas& c, Prop p, int frame) {
  switch (p) {
    case Prop::OakTree: oakTree(c, kLeaf, 101, 0); break;
    case Prop::OakTree2: oakTree(c, kLeafDark, 202, 1); break;
    case Prop::AutumnTree: oakTree(c, kAutumn, 303, 2); break;
    case Prop::PineTree: pineTree(c, kPine, 11, 5, false); break;
    case Prop::PineTree2: pineTree(c, kPine, 12, 4, false); break;
    case Prop::SnowPine: pineTree(c, kPine, 13, 5, true); break;
    case Prop::BirchTree: birchTree(c, 21); break;
    case Prop::DeadTree: deadTree(c, 31); break;
    case Prop::WillowTree: willowTree(c, 41); break;
    case Prop::PalmTree: palmTree(c, 51); break;
    case Prop::Bush: bush(c, kLeaf, 61); break;
    case Prop::BerryBush:
      bush(c, kLeafDark, 62);
      for (int i = 0; i < 9; i++) {
        int x = 3 + (int)(hash3(i, 0, 63) % 14), y = 3 + (int)(hash3(i, 1, 63) % 10);
        if (solid(c, x, y) && solid(c, x + 1, y + 1)) { c.set(x, y, rgba(220, 50, 70)); c.set(x + 1, y, rgba(150, 30, 60)); c.set(x, y - 1, rgba(255, 170, 170)); }
      }
      break;
    case Prop::SnowBush: bush(c, kPine, 64); snowCap(c, 2, 65); break;
    case Prop::Boulder: rock(c, c.w * 0.5f, c.h * 0.55f, c.w * 0.46f, c.h * 0.45f, kStone, 71, 8); break;
    case Prop::Rock: rock(c, c.w * 0.5f, c.h * 0.55f, c.w * 0.44f, c.h * 0.42f, kStone, 72, 4); break;
    case Prop::MossRock:
      rock(c, c.w * 0.5f, c.h * 0.55f, c.w * 0.45f, c.h * 0.44f, kStone, 73, 5);
      for (int x = 0; x < c.w; x++)
        for (int y = 0; y < c.h; y++) if (solid(c, x, y)) {
          int d = 2 + (int)(hash3(x, 0, 74) % 3);
          for (int k = 0; k < d; k++) if (solid(c, x, y + k)) c.set(x, y + k, kMoss[k == 0 ? 3 : 2 - (k == d - 1)]);
          break;
        }
      break;
    case Prop::SnowRock: rock(c, c.w * 0.5f, c.h * 0.55f, c.w * 0.45f, c.h * 0.44f, kStone, 75, 5); snowCap(c, 2, 76); break;
    case Prop::Stump: {
      float cx = c.w * 0.5f;
      trunk(c, cx, c.h - 2, 4, 10.0f, kBark, 81);
      ellipse(c, cx, 4.5f, 5.5f, 2.6f, kWood[3]);
      ellipse(c, cx, 4.5f, 3.5f, 1.6f, kWood[2]);
      ellipse(c, cx, 4.5f, 1.5f, 0.8f, kWood[3]);
      c.set((int)cx - 3, 3, kWood[4]);
      grassTuft(c, (int)cx - 5, c.h - 2, 82, 3);
      break;
    }
    case Prop::Log: {
      int y0 = 2, y1 = c.h - 3;
      for (int y = y0; y <= y1; y++)
        for (int x = 1; x < c.w - 4; x++) {
          float v = (y + 0.5f - (y0 + y1 + 1) * 0.5f) / ((y1 - y0 + 1) * 0.5f);
          int k = lightIndex(lightAt(0, v * 0.9f) + (hashf(x / 3, y, 83) - 0.5f) * 0.3f, x, y, 0);
          c.set(x, y, kBark[k]);
        }
      ellipse(c, c.w - 4.5f, (y0 + y1 + 1) * 0.5f, 3.0f, (y1 - y0 + 1) * 0.5f, kWood[3]);
      ellipse(c, c.w - 4.5f, (y0 + y1 + 1) * 0.5f, 1.6f, (y1 - y0 + 1) * 0.3f, kWood[2]);
      c.set(c.w - 5, (y0 + y1) / 2, kWood[4]);
      c.set(8, y0 - 1, kLeaf[3]); c.set(9, y0 - 1, kLeaf[2]); c.set(9, y0 - 2, kLeaf[3]);   // sprout
      break;
    }
    case Prop::Flowers1: flowers(c, 0, 91); break;
    case Prop::Flowers2: flowers(c, 1, 92); break;
    case Prop::Flowers3: flowers(c, 2, 93); break;
    case Prop::TallGrass: tallGrass(c, 94); break;
    case Prop::Reeds: reeds(c, 95); break;
    case Prop::Cactus: cactus(c); break;
    case Prop::Mushrooms: mushrooms(c); break;
    case Prop::LilyPad: lilyPad(c); break;
    case Prop::Fern: fern(c, 96); break;
    case Prop::Chest: chest(c, false); break;
    case Prop::ChestOpen: chest(c, true); break;
    case Prop::Barrel: barrel(c, 0, 0, c.w, c.h - 1); break;
    case Prop::Crate: crate(c); break;
    case Prop::Torch: torch(c, frame); break;
    case Prop::Campfire: campfire(c, frame); break;
    case Prop::Signpost: signpost(c); break;
    case Prop::Well: well(c); break;
    case Prop::FenceH: fence(c, false); break;
    case Prop::FenceV: fence(c, true); break;
    case Prop::Lamppost: lamppost(c); break;
    case Prop::Tent: tent(c); break;
    case Prop::MarketStall: marketStall(c); break;
    case Prop::Haystack: haystack(c); break;
    case Prop::Anvil: anvil(c); break;
    case Prop::Cart: cart(c); break;
    case Prop::Fountain: fountain(c, frame); break;
    case Prop::Statue: statue(c); break;
    case Prop::Banner: banner(c, frame); break;
    case Prop::Woodpile: woodpile(c); break;
    case Prop::Gravestone: gravestone(c); break;
    case Prop::Shrine: shrine(c); break;
    case Prop::CaveEntrance: caveEntrance(c); break;
    case Prop::Ladder: ladder(c); break;
    case Prop::Stalagmite: stalagmite(c); break;
    case Prop::Crystal: crystalCluster(c); break;
    case Prop::Bones: bones(c); break;
    case Prop::SkullPile: skullPile(c); break;
    case Prop::Cobweb: cobweb(c); break;
    case Prop::Brazier: brazier(c, frame); break;
    case Prop::Coffin: coffin(c); break;
    case Prop::Urn: urn(c); break;
    case Prop::IronDoor: ironDoor(c); break;
    case Prop::Altar: altar(c); break;
    case Prop::Bed: bed(c); break;
    case Prop::Table: table(c); break;
    case Prop::Chair: chair(c); break;
    case Prop::Shelf: shelfWithJars(c); break;
    case Prop::Fireplace: fireplace(c, frame); break;
    case Prop::Rug: rug(c); break;
    case Prop::Counter: counter(c); break;
    case Prop::Barrel2: kegRack(c); break;
    case Prop::PlantPot: plantPot(c); break;
    case Prop::Cauldron: cauldron(c, frame); break;
    case Prop::Bookshelf: bookshelf(c); break;
    case Prop::Throne: throne(c); break;
    default: break;
  }
}

}  // namespace

int propW(Prop p) { return (int)p < (int)Prop::COUNT ? kPropInfo[(int)p].w : 16; }
int propH(Prop p) { return (int)p < (int)Prop::COUNT ? kPropInfo[(int)p].h : 16; }
int propFrames(Prop p) { return (int)p < (int)Prop::COUNT ? kPropInfo[(int)p].frames : 1; }

Canvas propSprite(Prop p) {
  const int w = propW(p), h = propH(p), n = propFrames(p);
  Canvas sheet(w * n, h);
  for (int f = 0; f < n; f++) {
    Canvas cell(w, h);
    paintProp(cell, p, f);
    if (p != Prop::Cobweb && p != Prop::Rug) outline(cell);
    place(sheet, cell, f, 0);
  }
  return sheet;
}

// =====================================================================================================
// 6. buildings, city walls, gate
// =====================================================================================================
namespace {

enum RoofTex { kShingle, kThatchTex, kSlate };

const Ramp kPlaster = ramp5(rgba(150, 128, 112), rgba(196, 174, 146), rgba(228, 212, 180), rgba(242, 232, 206), rgba(252, 248, 230));
const Ramp kBeam = ramp5(rgba(40, 26, 32), rgba(64, 40, 38), rgba(92, 60, 46), rgba(120, 84, 58), rgba(150, 112, 76));
const Ramp kRoofRed = ramp5(rgba(76, 30, 40), rgba(124, 46, 44), rgba(170, 70, 52), rgba(204, 104, 70), rgba(232, 150, 102));
const Ramp kRoofSlate = ramp5(rgba(38, 42, 64), rgba(58, 66, 92), rgba(82, 94, 122), rgba(114, 128, 154), rgba(156, 170, 190));
const Ramp kRoofGreen = ramp5(rgba(30, 56, 52), rgba(44, 86, 62), rgba(64, 120, 72), rgba(98, 152, 86), rgba(150, 190, 116));
const Ramp kRoofBrown = ramp5(rgba(54, 34, 38), rgba(88, 56, 46), rgba(124, 82, 58), rgba(158, 112, 74), rgba(192, 148, 100));
const Ramp kRoofPurple = ramp5(rgba(36, 26, 60), rgba(58, 40, 94), rgba(86, 60, 132), rgba(122, 92, 170), rgba(170, 140, 206));
const Ramp kBarnRed = ramp5(rgba(70, 24, 36), rgba(112, 36, 40), rgba(156, 52, 46), rgba(190, 82, 62), rgba(220, 126, 96));

// texture index offset (-1/0/+1) for a roof pixel at roof-local (x, y)
int roofTexel(RoofTex t, int x, int y, uint32_t seed) {
  switch (t) {
    case kShingle: {
      const int rh = 4, sw = 6;
      int row = y / rh, off = (row & 1) * (sw / 2), col = (x + off) / sw;
      int yy = y % rh, xx = (x + off) % sw;
      int k = 0;
      if (yy == rh - 1) k = -1;
      else if (xx == 0) k = -1;
      else if (yy == 0) k = 1;
      if (hash3(col, row, seed) % 7 == 0 && k == 0) k = -1;   // a few weathered shingles
      if (hash3(col, row, seed + 5) % 11 == 0 && k == 0) k = 1;
      return k;
    }
    case kSlate: {
      const int rh = 3, sw = 4;
      int row = y / rh, off = (row & 1) * 2, col = (x + off) / sw;
      int yy = y % rh, xx = (x + off) % sw;
      int k = 0;
      if (yy == rh - 1 || xx == 0) k = -1;
      if (hash3(col, row, seed) % 6 == 0 && k == 0) k = 1;
      return k;
    }
    default: {   // thatch: layered bundles with wavy lower edges and straw strokes
      const int rh = 5;
      int wave = (int)std::lround(std::sin(x * 0.7f + (y / rh) * 1.3f) * 0.9f);
      int yy = (y + wave + rh * 4) % rh;
      int k = 0;
      if (yy == rh - 1) k = -1;
      else if (yy == 0) k = 1;
      if (hash3(x, y / 2, seed) % 9 == 0) k -= 1;
      if (hash3(x, y / 3, seed + 3) % 13 == 0) k += 1;
      return std::clamp(k, -1, 1);
    }
  }
}

// Hipped roof in 3/4 view. The roof covers the rectangle x0..x1, top..eave; the ridge runs at ridgeY
// between the two ridge ends. Four facets: back (above the ridge, catches the sky), front (faces the
// viewer), lit left hip and shaded right hip triangles.
void hipRoof(Canvas& c, int x0, int x1, int top, int eave, int inset, const Ramp& R, RoofTex tex, uint32_t seed) {
  int ridgeY = top + std::max(2, (eave - top) * 3 / 10);
  float up = (float)std::max(1, ridgeY - top), down = (float)std::max(1, eave - ridgeY);
  for (int y = top; y <= eave; y++)
    for (int x = x0; x <= x1; x++) {
      float reach = inset * std::min((y - top) / up, (eave - y) / down);   // hip triangle half-width at this row
      int base;
      if (x - x0 < reach) base = 3;
      else if (x1 - x < reach) base = 1;
      else base = y < ridgeY ? 3 : 2;
      int k = base + roofTexel(tex, x - x0, y - top, seed);
      c.set(x, y, R[std::clamp(k, 0, 4)]);
    }
  // ridge cap and hip edges
  hline(c, x0 + inset, x1 - inset, ridgeY, R[4]);
  hline(c, x0 + inset, x1 - inset, ridgeY + 1, R[1]);
  if (inset > 0) {
    line(c, x0, top, x0 + inset, ridgeY, R[4]);
    line(c, x0, eave, x0 + inset, ridgeY, R[4]);
    line(c, x1, top, x1 - inset, ridgeY, R[1]);
    line(c, x1, eave, x1 - inset, ridgeY, R[0]);
  }
  hline(c, x0, x1, top, R[3]);
  // eave: fascia board with a dark drip edge
  hline(c, x0, x1, eave, R[0]);
  hline(c, x0 + 1, x1 - 1, eave - 1, R[1]);
}

// warm lit window: frame, glowing panes with a cross mullion, sill; optional shutters + flower box
void window(Canvas& c, int x, int y, int w, int h, const Ramp& frame, uint32_t seed, bool shutters, bool flowers, bool arched = false) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) {
      bool edge = i == 0 || j == 0 || i == w - 1 || j == h - 1;
      if (arched && j == 0 && (i == 0 || i == w - 1)) continue;
      if (edge) { c.set(x + i, y + j, frame[i == 0 || j == 0 ? 1 : 0]); continue; }
      int k = j < h / 2 ? 4 : 3;
      if (i == w - 2 || j == h - 2) k--;
      c.set(x + i, y + j, kGlow[k]);
    }
  vline(c, x + w / 2, y + 1, y + h - 2, frame[2]);
  hline(c, x + 1, x + w - 2, y + h / 2, frame[2]);
  c.set(x + 1, y + 1, kWhite);
  hline(c, x - 1, x + w, y + h, frame[3]);   // sill
  if (shutters) {
    Ramp sh = ramp((seed & 2) ? rgba(60, 100, 140) : rgba(70, 110, 70));
    for (int j = 0; j < h; j++) { c.set(x - 2, y + j, sh[3]); c.set(x - 1, y + j, sh[2]); c.set(x + w, y + j, sh[2]); c.set(x + w + 1, y + j, sh[1]); }
    for (int j = 1; j < h; j += 2) { c.set(x - 2, y + j, sh[1]); c.set(x + w + 1, y + j, sh[0]); }
  }
  if (flowers) {
    hline(c, x - 1, x + w, y + h + 1, kWood[2]);
    hline(c, x - 1, x + w, y + h + 2, kWood[0]);
    for (int i = 0; i < w + 2; i++) {
      uint32_t fc = (i + (int)seed) % 3 == 0 ? rgba(230, 70, 80) : ((i + (int)seed) % 3 == 1 ? rgba(250, 200, 80) : kLeaf[3]);
      c.set(x - 1 + i, y + h, fc);
      if (i % 2) c.set(x - 1 + i, y + h - 1, kLeaf[2]);
    }
  }
}

// plank door with frame and handle; bottom edge at y1
void door(Canvas& c, int cx, int y1, int w, int h, const Ramp& wood, const Ramp& frame, bool arched) {
  int x0 = cx - w / 2;
  for (int j = -1; j < h; j++)
    for (int i = -1; i <= w; i++) {
      int y = y1 - h + 1 + j;
      bool inArch = arched && j < 3 && ((i - w * 0.5f + 0.5f) * (i - w * 0.5f + 0.5f) / (w * w * 0.25f) + (3 - j) * (3 - j) / 9.0f > 1.0f);
      if (inArch) continue;
      if (i == -1 || i == w || j == -1) { c.set(x0 + i, y, frame[i == -1 || j == -1 ? 3 : 1]); continue; }
      int k = (i % 3 == 0) ? 1 : 2;
      if (i == w - 1) k = 1;
      if (j == 0) k = 1;
      c.set(x0 + i, y, wood[k]);
    }
  // iron bands + handle
  for (int i = 0; i < w; i++) { c.set(x0 + i, y1 - h + 4, kIron[1]); c.set(x0 + i, y1 - 3, kIron[1]); }
  c.set(x0 + w - 3, y1 - h / 2, kGold[4]); c.set(x0 + w - 3, y1 - h / 2 + 1, kGold[1]);
  // step stone
  hline(c, x0 - 1, x0 + w, y1, kStone[3]);
}

// stone masonry fill
void masonry(Canvas& c, int x0, int y0, int x1, int y1, const Ramp& S, uint32_t seed, int bh = 4, int bw = 7) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      int row = (y - y0) / bh, off = (row & 1) * (bw / 2), col = (x - x0 + off) / bw;
      bool mortar = (y - y0) % bh == bh - 1 || (x - x0 + off) % bw == 0;
      int k = 2;
      uint32_t hsh = hash3(col, row, seed);
      if (hsh % 5 == 0) k = 3;
      if (hsh % 7 == 1) k = 1;
      if ((y - y0) % bh == 0 && !mortar) k = std::min(4, k + 1);   // lit top edge of each block
      if (mortar) k = 1;
      if (x == x0) k = std::min(4, k + 1);
      if (x == x1) k = std::max(0, k - 1);
      c.set(x, y, S[k]);
    }
}

// timber frame: plaster with dark beams and braces
void timberWall(Canvas& c, int x0, int y0, int x1, int y1, uint32_t seed) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      int k = 2 + ((hash3(x / 2, y / 2, seed) % 9 == 0) ? -1 : 0);
      if (x == x0 + 1) k = 3;
      c.set(x, y, kPlaster[k]);
    }
  auto beamH = [&](int y) { for (int x = x0; x <= x1; x++) { c.set(x, y, kBeam[2]); c.set(x, y + 1, kBeam[1]); } };
  auto beamV = [&](int x) { for (int y = y0; y <= y1; y++) { c.set(x, y, kBeam[3]); c.set(x + 1, y, kBeam[1]); } };
  beamH(y0);
  beamH(y1 - 1);
  beamV(x0);
  beamV(x1 - 1);
  for (int x = x0 + 16; x < x1 - 4; x += 16) {
    beamV(x - 1);
    // diagonal braces either side of the post
    int h = std::min(8, (y1 - y0) / 2);
    line(c, x - 7, y1 - 2, x - 2, y1 - 2 - h, kBeam[2]);
    line(c, x + 2, y1 - 2 - h, x + 7, y1 - 2, kBeam[2]);
  }
}

void plankWall(Canvas& c, int x0, int y0, int x1, int y1, const Ramp& R, uint32_t seed) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      int k = ((x - x0) % 4 == 3) ? 1 : 2;
      if ((x - x0) % 4 == 0) k = 3;
      if (hash3(x / 4, y / 6, seed) % 6 == 0 && k == 2) k = 1;
      if (x == x1) k = 1;
      c.set(x, y, R[k]);
    }
}

void chimney(Canvas& c, int x, int top, int bottom, const Ramp& S, bool glow) {
  for (int y = top; y <= bottom; y++)
    for (int i = 0; i < 6; i++) {
      bool mortar = (y - top) % 3 == 2 || ((i + ((y - top) / 3) * 3) % 6 == 0);
      int k = i < 2 ? 3 : (i > 4 ? 1 : 2);
      if (mortar) k = 1;
      c.set(x + i, y, S[k]);
    }
  hline(c, x - 1, x + 6, top, S[4]);
  hline(c, x - 1, x + 6, top + 1, S[1]);
  if (glow) { c.set(x + 2, top, kFire[3]); c.set(x + 3, top, kFire[4]); c.set(x + 1, top, kFire[2]); }
  else { c.set(x + 2, top, kInk); c.set(x + 3, top, kInk); }
}

void hangingSign(Canvas& c, int x, int y, int icon) {
  // iron bracket out from the wall, board hanging on two chains, painted icon
  hline(c, x, x + 10, y, kIron[2]);
  c.set(x, y + 1, kIron[1]); c.set(x + 1, y + 2, kIron[1]);
  c.set(x + 2, y + 1, kIron[3]); c.set(x + 9, y + 1, kIron[3]);
  for (int j = 0; j < 7; j++)
    for (int i = 0; i < 10; i++) {
      int k = (i == 0 || j == 0) ? 4 : ((i == 9 || j == 6) ? 1 : 3);
      c.set(x + 1 + i, y + 2 + j, kWood[k]);
    }
  int ix = x + 4, iy = y + 4;
  switch (icon) {
    case 0:   // mug of ale
      box(c, ix, iy, ix + 2, iy + 3, kGold[3]); hline(c, ix, ix + 2, iy, kWhite); c.set(ix + 3, iy + 1, kGold[2]); c.set(ix + 3, iy + 2, kGold[2]);
      break;
    case 1:   // anvil
      hline(c, ix - 1, ix + 4, iy + 1, kIron[3]); hline(c, ix, ix + 3, iy + 2, kIron[2]); hline(c, ix + 1, ix + 2, iy + 3, kIron[1]);
      break;
    default:  // coin pouch
      box(c, ix, iy + 1, ix + 3, iy + 3, kLeather[3]); c.set(ix + 1, iy, kLeather[2]); c.set(ix + 2, iy, kLeather[2]); c.set(ix + 1, iy + 2, kGold[4]);
      break;
  }
}

void wallBanner(Canvas& c, int x, int y, int h, const Ramp& cloth) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < 6; i++) {
      if (j >= h - 2 && (i == 2 || i == 3) && j == h - 1) continue;
      if (j == h - 1 && (i == 2 || i == 3)) continue;
      int k = i == 0 ? 3 : (i == 5 ? 1 : 2);
      c.set(x + i, y + j, cloth[k]);
    }
  hline(c, x - 1, x + 6, y, kGold[3]);
  c.set(x + 2, y + 4, kGold[4]); c.set(x + 3, y + 4, kGold[3]); c.set(x + 2, y + 5, kGold[3]); c.set(x + 3, y + 5, kGold[2]);
  c.set(x + 1, y + 5, kGold[2]); c.set(x + 4, y + 5, kGold[1]); c.set(x + 2, y + 6, kGold[2]); c.set(x + 3, y + 6, kGold[1]);
}

void crenellate(Canvas& c, int x0, int x1, int y, const Ramp& S) {
  // parapet with merlons rising 3px above y
  for (int x = x0; x <= x1; x++) {
    bool merlon = ((x - x0) / 3) % 2 == 0;
    int topY = merlon ? y - 3 : y;
    for (int yy = topY; yy <= y + 2; yy++) {
      int k = yy == topY ? 4 : 2;
      if ((x - x0) % 3 == 2 && merlon) k = 1;
      c.set(x, yy, S[k]);
    }
  }
  hline(c, x0, x1, y + 3, S[0]);
}

}  // namespace

namespace {
Canvas paintBuilding(Building b, int wTiles, int hTiles, uint32_t roofColor, uint32_t seed) {
  wTiles = std::max(2, wTiles);
  hTiles = std::max(1, hTiles);
  const int W = wTiles * 16, H = (hTiles + 2) * 16;
  Canvas c(W, H);
  const int doorX = (wTiles / 2) * 16 + 8;   // centre of the door tile column
  const int ground = H - 1;

  // style table -------------------------------------------------------------------------------------
  int wallH = 26;
  RoofTex tex = kShingle;
  Ramp roof = kRoofRed;
  switch (b) {
    case Building::House: tex = (seed & 1) ? kShingle : kThatchTex; roof = (seed & 1) ? kRoofRed : kThatch; break;
    case Building::StoneHouse: tex = kSlate; roof = kRoofSlate; break;
    case Building::Inn: tex = kShingle; roof = kRoofBrown; wallH = 36; break;
    case Building::Smithy: tex = kSlate; roof = kRoofSlate; break;
    case Building::Shop: tex = kShingle; roof = kRoofGreen; break;
    case Building::Temple: tex = kSlate; roof = kRoofSlate; wallH = 30; break;
    case Building::Keep: tex = kSlate; roof = kStone; wallH = std::min(H - 14, 44); break;
    case Building::Tower: tex = kSlate; roof = kRoofPurple; break;
    case Building::Farmhouse: tex = kShingle; roof = kRoofBrown; wallH = 28; break;
    case Building::Hut: tex = kThatchTex; roof = kThatch; wallH = 22; break;
    default: break;
  }
  if (roofColor) roof = ramp(roofColor);
  wallH = std::min(wallH, H - 16);
  const int wallTop = H - wallH;
  const int wx0 = 2, wx1 = W - 3;

  // ---- walls
  switch (b) {
    case Building::House: case Building::Inn: case Building::Shop:
      masonry(c, wx0, ground - 3, wx1, ground, kStone, seed, 2, 5);
      timberWall(c, wx0, wallTop, wx1, ground - 4, seed);
      if (b == Building::Inn) for (int x = wx0; x <= wx1; x++) { c.set(x, wallTop + 15, kBeam[3]); c.set(x, wallTop + 16, kBeam[1]); }
      break;
    case Building::StoneHouse: case Building::Smithy: case Building::Temple:
      masonry(c, wx0, wallTop, wx1, ground, b == Building::Temple ? kStoneWarm : kStone, seed);
      break;
    case Building::Keep: masonry(c, wx0, wallTop, wx1, ground, kStone, seed, 4, 8); break;
    case Building::Farmhouse:
      plankWall(c, wx0, wallTop, wx1, ground, kBarnRed, seed);
      for (int x = wx0; x <= wx1; x++) { c.set(x, wallTop, kCloth[4]); c.set(x, wallTop + 1, kCloth[2]); }
      for (int y = wallTop; y <= ground; y++) { c.set(wx0, y, kCloth[4]); c.set(wx0 + 1, y, kCloth[3]); c.set(wx1, y, kCloth[2]); c.set(wx1 - 1, y, kCloth[3]); }
      break;
    case Building::Hut: plankWall(c, wx0, wallTop, wx1, ground, kWood, seed); break;
    case Building::Tower: break;
    default: break;
  }

  // ---- roof
  int roofTop = 2;
  if (b == Building::Temple) roofTop = std::max(10, H - wallH - 30);   // leave the sky for the steeple
  if (b == Building::Tower) {
    // round tower: stone cylinder in the middle, conical roof; low annexes either side when wide
    int tw = std::min(W - 8, 34), tx0 = W / 2 - tw / 2, tx1 = tx0 + tw - 1;
    int bodyTop = 22;
    if (W - tw > 16) {   // annex wings
      int aTop = ground - 22;
      masonry(c, wx0, aTop, wx1, ground, kStone, seed + 3);
      hipRoof(c, 1, W - 2, aTop - 10, aTop + 1, 4, kRoofSlate, kSlate, seed);
    }
    for (int y = bodyTop; y <= ground; y++)
      for (int x = tx0; x <= tx1; x++) {
        float u = (x + 0.5f - (tx0 + tx1 + 1) * 0.5f) / (tw * 0.5f);
        int row = (y - bodyTop) / 4, off = (row & 1) * 3;
        bool mortar = (y - bodyTop) % 4 == 3 || ((x + off) % 7 == 0);
        int k = lightIndex(lightAt(u * 0.92f, 0), x, y, 0.15f);
        if (mortar) k = std::max(0, k - 1);
        c.set(x, y, kStone[k]);
      }
    // conical roof
    int coneBase = bodyTop + 2, apex = 0;
    for (int y = apex; y <= coneBase; y++) {
      float t = (float)(y - apex) / (coneBase - apex);
      float half = 1 + t * (tw * 0.5f + 3);
      for (int x = (int)(W * 0.5f - half); x <= (int)(W * 0.5f + half - 1); x++) {
        float u = (x + 0.5f - W * 0.5f) / std::max(1.0f, half);
        int k = lightIndex(lightAt(u * 0.9f, -0.3f), x, y, 0.1f) + roofTexel(kSlate, x, y, seed);
        c.set(x, y, roof[std::clamp(k, 0, 4)]);
      }
    }
    hline(c, (int)(W * 0.5f - tw * 0.5f - 3), (int)(W * 0.5f + tw * 0.5f + 2), coneBase, roof[0]);
    c.set(W / 2, 0, kGold[4]); c.set(W / 2 - 1, 0, kGold[3]);
    // arcane windows
    for (int i = 0; i < 2; i++) {
      int wy = bodyTop + 6 + i * 14;
      if (wy + 8 > ground - 20) break;
      int wx = W / 2 - 2;
      for (int j = 0; j < 7; j++) for (int k2 = 0; k2 < 4; k2++) c.set(wx + k2, wy + j, (j == 0 && (k2 == 0 || k2 == 3)) ? kStone[2] : (j < 3 ? rgba(170, 150, 255) : rgba(110, 90, 220)));
      c.set(wx + 1, wy + 1, kWhite);
      hline(c, wx - 1, wx + 4, wy + 7, kStone[4]);
    }
    door(c, doorX, ground, 10, 15, kWoodDark, kStone, true);
    return c;
  }
  if (b == Building::Keep) {
    // flat stone roof deck behind a crenellated parapet, corner turrets with banners
    for (int y = roofTop + 4; y < wallTop; y++)
      for (int x = 4; x <= W - 5; x++) {
        int k = ((x / 5 + y / 4) % 2) ? 3 : 2;
        if ((y % 4) == 0 || (x % 5) == 0) k = 1;
        c.set(x, y, kStone[k]);
      }
    crenellate(c, 4, W - 5, roofTop + 4, kStone);   // back parapet
    // turrets at the corners
    for (int side = 0; side < 2; side++) {
      int tx = side == 0 ? 1 : W - 13;
      masonry(c, tx, roofTop + 6, tx + 11, wallTop + 2, kStone, seed + side, 4, 6);
      crenellate(c, tx, tx + 11, roofTop + 6, kStone);
      wallBanner(c, tx + 3, roofTop + 12, std::min(14, wallTop - roofTop - 14), kRed);
    }
    if (hTiles >= 3 && W >= 64) {   // central donjon with its own slate roof
      int dx0 = W / 2 - 14, dx1 = W / 2 + 13;
      masonry(c, dx0, roofTop + 17, dx1, wallTop - 1, kStone, seed + 11, 4, 7);
      hipRoof(c, dx0 - 2, dx1 + 2, roofTop + 3, roofTop + 18, 6, kRoofSlate, kSlate, seed);
      window(c, W / 2 - 3, roofTop + 21, 6, 7, kStone, seed, false, false, true);
    }
    crenellate(c, 1, W - 2, wallTop, kStone);   // front parapet
    // flag on top
    int fx = W / 2;
    vline(c, fx, 0, roofTop + 6, kWood[2]);
    for (int j = 0; j < 4; j++) for (int i = 1; i < 7 - j / 2; i++) c.set(fx + i, 1 + j, kRed[(i + j) % 3 == 0 ? 3 : 2]);
    // facade: banners either side of a grand arched door, arrow slits
    door(c, doorX, ground, 12, 20, kWoodDark, kStone, true);
    for (int x = 16; x < W - 12; x += 16) {
      if (std::abs(x + 8 - doorX) < 12) continue;
      wallBanner(c, x + 5, wallTop + 6, 16, kRed);
      vline(c, x + 2, wallTop + 26, wallTop + 31, kInk);
    }
    return c;
  }

  int eave = wallTop + 2;
  int inset = std::min((eave - roofTop) / 2, W / 5);
  if (b == Building::Farmhouse) inset = 0;
  hipRoof(c, 1, W - 2, roofTop, eave, inset, roof, tex, seed);
  if (b == Building::Farmhouse) {   // gambrel break line + hay loft door
    int brk = roofTop + (eave - roofTop) * 2 / 5;
    hline(c, 1, W - 2, brk, roof[0]);
    hline(c, 1, W - 2, brk + 1, roof[3]);
    int lx = doorX - 5;
    for (int j = 0; j < 9; j++) for (int i = 0; i < 10; i++) c.set(lx + i, brk - 10 + j, (i == 0 || i == 9 || j == 0) ? kCloth[4] : kWoodDark[1]);
    for (int i = 1; i < 9; i++) c.set(lx + i, brk - 2, kThatch[3 + (i & 1)]);
    for (int i = 1; i < 9; i += 2) c.set(lx + i, brk - 3, kThatch[3]);
  }
  // eave shadow on the wall
  for (int x = wx0; x <= wx1; x++) for (int y = eave + 1; y <= eave + 2; y++) if (solid(c, x, y)) c.set(x, y, darken(c.get(x, y), y == eave + 1 ? 0.9f : 0.45f));

  // chimney / dormer
  int chx = (seed % 2) ? W - 18 : 8;
  if (b == Building::Smithy) chx = W - 14;
  bool hasChimney = b != Building::Temple && b != Building::Hut && b != Building::Farmhouse;
  if (b == Building::Hut && (seed & 2)) hasChimney = true;
  if (hasChimney) {
    int ctop = roofTop + (b == Building::Smithy ? -2 : 2);
    int cbot = roofTop + 10;
    chimney(c, chx, std::max(0, ctop), cbot, b == Building::Smithy ? kStone : kBrick, b == Building::Smithy);
  }
  if (hTiles >= 3 && b != Building::Temple && b != Building::Farmhouse && b != Building::Hut) {   // dormer window on deep roofs
    int dx = (seed & 4) ? W / 2 - 16 : W / 2 + 8;
    if (std::abs(dx + 4 - (chx + 3)) < 10) dx = W - dx - 8;
    int dy = roofTop + (eave - roofTop) / 2 - 6;
    for (int y = dy - 3; y < dy + 10; y++) for (int x = dx - 2; x < dx + 10; x++) {
      int rel = y - (dy - 3);
      if (x - (dx - 2) < 3 - rel || (dx + 9) - x < 3 - rel) continue;
      c.set(x, y, rel < 4 ? roof[3] : kPlaster[2]);
    }
    window(c, dx, dy + 2, 6, 6, kBeam, seed, false, false);
  }

  // ---- facade details per style
  const Ramp& frame = (b == Building::StoneHouse || b == Building::Temple || b == Building::Smithy) ? kStoneWarm : kBeam;
  int winY = ground - 18, winW = 8, winH = 8;
  if (b == Building::Hut) { winY = ground - 14; winW = 6; winH = 6; }
  bool forgeOpen = b == Building::Smithy;
  for (int t = 0; t < wTiles; t++) {
    int cx = t * 16 + 8;
    if (t == wTiles / 2) continue;
    if (forgeOpen && t == wTiles - 1) continue;
    bool sh = hash3(t, 1, seed) % 2 == 0 && b != Building::Temple;
    bool fl = hash3(t, 2, seed) % 3 == 0 && (b == Building::House || b == Building::Inn || b == Building::StoneHouse || b == Building::Shop);
    if (b == Building::Temple) {
      window(c, cx - 3, winY - 6, 6, 12, frame, seed, false, false, true);
      // stained glass colours
      for (int j = 1; j < 11; j++) for (int i = 1; i < 5; i++) {
        if (i == 3 || j == 6) continue;
        static const uint32_t sg[4] = {rgba(220, 70, 80), rgba(80, 130, 230), rgba(250, 200, 80), rgba(90, 190, 120)};
        c.set(cx - 3 + i, winY - 6 + j, sg[(i + j / 3) % 4]);
      }
      continue;
    }
    if (b == Building::Inn && t == wTiles / 2 + 1 && wTiles > 2) { hangingSign(c, cx - 8, wallTop + 18, 0); continue; }
    if (b == Building::Shop && t == wTiles / 2 + 1 && wTiles > 2) { hangingSign(c, cx - 8, wallTop + 6, 2); }
    window(c, cx - winW / 2, winY, winW, winH, frame, seed + t, sh, fl);
    if (b == Building::Inn) window(c, cx - 3, wallTop + 5, 6, 7, frame, seed + t + 9, false, hash3(t, 3, seed) % 2 == 0);
  }
  if (b == Building::Inn) {   // lanterns either side of the door
    for (int s = -1; s <= 1; s += 2) { int lx = doorX + s * 9; c.set(lx, ground - 16, kIron[2]); c.set(lx, ground - 15, kGlow[4]); c.set(lx, ground - 14, kGlow[2]); }
  }
  if (b == Building::Shop) {   // striped awning over the facade
    int ay = wallTop + 3;
    for (int y = ay; y < ay + 6; y++)
      for (int x = wx0; x <= wx1; x++) {
        bool stripe = ((x / 4) & 1) == 0;
        const Ramp& r = stripe ? kRed : kCloth;
        int k = y == ay ? 4 : (y < ay + 4 ? 3 : 2);
        c.set(x, y, r[k]);
      }
    for (int x = wx0; x <= wx1; x++) if (x % 4 == 1 || x % 4 == 2) c.set(x, ay + 6, ((x / 4) & 1) == 0 ? kRed[1] : kCloth[1]);
  }
  if (forgeOpen) {
    // open forge on the last bay: dark opening, glowing hearth, anvil, sign
    int fx0 = (wTiles - 1) * 16 + 1, fx1 = W - 4;
    for (int y = ground - 17; y <= ground; y++) for (int x = fx0; x <= fx1; x++) c.set(x, y, y < ground - 15 ? kBeam[1] : rgba(36, 24, 30));
    for (int x = fx0; x <= fx1; x++) c.set(x, ground - 18, kBeam[3]);
    for (int y = ground - 7; y <= ground; y++) for (int x = fx0 + 1; x <= fx0 + 7; x++) c.set(x, y, kStone[x == fx0 + 1 ? 3 : 1]);
    for (int x = fx0 + 2; x <= fx0 + 6; x++) { c.set(x, ground - 7, kFire[(x & 1) ? 3 : 4]); c.set(x, ground - 8, kFire[2]); }
    for (int y = ground - 16; y < ground - 8; y++) for (int x = fx0 + 2; x <= fx0 + 6; x++) if (c.get(x, y) == rgba(36, 24, 30)) c.set(x, y, mix(rgba(36, 24, 30), kFire[1], 0.35f));
    hline(c, fx1 - 6, fx1 - 1, ground - 4, kIron[3]); hline(c, fx1 - 5, fx1 - 2, ground - 3, kIron[2]); vline(c, fx1 - 4, ground - 2, ground, kIron[1]);
    hangingSign(c, (wTiles / 2) * 16 + 14 > fx0 - 12 ? fx0 - 12 : (wTiles / 2) * 16 + 14, wallTop + 4, 1);
  }
  if (b == Building::Temple) {
    // steeple: square bell tower rising from the ridge with a spire
    int sx = W / 2 - 6;
    masonry(c, sx, roofTop - 8, sx + 11, roofTop + 8, kStoneWarm, seed + 7, 3, 6);
    for (int y = roofTop - 6; y < roofTop + 1; y++) for (int x = sx + 3; x <= sx + 8; x++) c.set(x, y, kInk);
    ball(c, sx + 6.0f, roofTop - 2.5f, 2.0f, 2.2f, kGold);
    for (int y = 0; y < roofTop - 8; y++) {
      float t = (float)y / std::max(1, roofTop - 8);
      int half = (int)(t * 7);
      for (int x = W / 2 - half - 1; x <= W / 2 + half; x++) c.set(x, y, roof[x < W / 2 ? 3 : 1]);
    }
    c.set(W / 2, 0, kGold[4]);
    // rose window above the door
    int rx = doorX, ry = wallTop + 6;
    ellipse(c, rx, ry + 0.5f, 4.5f, 4.5f, kStoneWarm[1]);
    ellipse(c, rx, ry + 0.5f, 3.5f, 3.5f, rgba(80, 130, 230));
    for (int a = 0; a < 6; a++) line(c, rx, ry, rx + (int)std::lround(std::cos(a * 1.047f) * 3), ry + (int)std::lround(std::sin(a * 1.047f) * 3), rgba(220, 70, 80));
    c.set(rx, ry, kGold[4]);
  }
  // door
  bool arch = b == Building::StoneHouse || b == Building::Temple || b == Building::Smithy;
  int dw = b == Building::Hut ? 8 : (b == Building::Temple ? 12 : 10);
  int dh = b == Building::Hut ? 14 : (b == Building::Temple ? 20 : 17);
  if (b == Building::Farmhouse) {
    // big barn door with X bracing
    int x0 = doorX - 7;
    for (int y = ground - 19; y <= ground; y++) for (int x = x0; x < x0 + 14; x++) c.set(x, y, kBarnRed[(x - x0) % 3 == 0 ? 1 : 2]);
    for (int y = ground - 19; y <= ground; y++) { c.set(x0, y, kCloth[4]); c.set(x0 + 13, y, kCloth[2]); c.set(x0 + 7, y, kCloth[3]); }
    hline(c, x0, x0 + 13, ground - 19, kCloth[4]);
    line(c, x0, ground - 19, x0 + 6, ground, kCloth[3]); line(c, x0 + 6, ground - 19, x0, ground, kCloth[3]);
    line(c, x0 + 7, ground - 19, x0 + 13, ground, kCloth[2]); line(c, x0 + 13, ground - 19, x0 + 7, ground, kCloth[2]);
  } else {
    door(c, doorX, ground, dw, dh, b == Building::Temple ? kWood : kWoodDark, frame, arch);
  }
  if (b == Building::Hut) {   // crooked patch planks
    int px2 = (seed % 2) ? 6 : W - 12;
    hline(c, px2, px2 + 5, ground - 9, kWood[4]); hline(c, px2, px2 + 5, ground - 8, kWood[1]);
  }
  return c;
}
}  // namespace

Canvas buildingSprite(Building b, int wTiles, int hTiles, uint32_t roofColor, uint32_t seed) {
  Canvas c = paintBuilding(b, wTiles, hTiles, roofColor, seed);
  outline(c);
  return c;
}

// ---- city wall -------------------------------------------------------------------------------------
namespace {
const Ramp& wallStone() { return kStone; }

void wallFace(Canvas& c, int x, int top, int h, uint32_t seed) {
  // one column of brick face below a top-surface pixel
  const Ramp& S = wallStone();
  for (int j = 0; j < h; j++) {
    int y = top + j;
    int row = j / 4, off = (row & 1) * 4;
    bool mortar = j % 4 == 3 || (x + off) % 8 == 0;
    int k = 2;
    uint32_t hsh = hash3((x + off) / 8, row, seed);
    if (hsh % 5 == 0) k = 3;
    if (hsh % 6 == 1) k = 1;
    if (mortar) k = 1;
    if (j == 0) k = 4;
    if (j == h - 1) k = 0;
    if (j >= h - 3 && !mortar && hsh % 3 == 0) k = 1;   // grime near the ground
    c.set(x, y, S[k]);
  }
}
}  // namespace

Canvas wallPiece(int mask) {
  Canvas c(16, 32);
  const Ramp& S = wallStone();
  bool N = mask & 1, E = mask & 2, Sth = mask & 4, Wst = mask & 8;
  // top surface in the upper 16 rows; the wall body is 10px thick (x 3..12)
  auto top = [&](int x, int y) {
    bool core = x >= 3 && x <= 12 && y >= 3 && y <= 12;
    bool n = N && x >= 3 && x <= 12 && y < 3;
    bool s = Sth && x >= 3 && x <= 12 && y > 12;
    bool e = E && x > 12 && y >= 3 && y <= 12;
    bool w = Wst && x < 3 && y >= 3 && y <= 12;
    return (core || n || s || e || w) && x >= 0 && x < 16 && y >= 0 && y < 16;
  };
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      if (!top(x, y)) continue;
      // walkway flagstones with a lighter parapet rim on exposed edges
      bool rim = !top(x - 1, y) || !top(x + 1, y) || !top(x, y - 1) || !top(x, y + 1);
      bool openEdgeY = (y == 0 && N) || (y == 15 && Sth);
      bool openEdgeX = (x == 0 && Wst) || (x == 15 && E);
      if (openEdgeY && top(x - 1, y) && top(x + 1, y)) rim = false;
      if (openEdgeX && top(x, y - 1) && top(x, y + 1)) rim = false;
      if (openEdgeY || openEdgeX) {
        rim = (!top(x - 1, y) && !(x == 0)) || (!top(x + 1, y) && !(x == 15)) || (!top(x, y - 1) && !(y == 0)) || (!top(x, y + 1) && !(y == 15));
      }
      int k = ((x / 4 + y / 3) % 2) ? 3 : 2;
      if (y % 3 == 0 || x % 4 == 0) k = 2;
      if (rim) k = 4;
      c.set(x, y, S[k]);
    }
  // inner parapet shadow next to the rim
  Canvas t = c;
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++)
      if (solid(t, x, y) && t.get(x, y) != S[4] && t.get(x, y - 1) == S[4] && solid(t, x, y - 1)) c.set(x, y, S[1]);
  // front faces: below every top pixel whose southern neighbour is not wall
  for (int x = 0; x < 16; x++)
    for (int y = 15; y >= 0; y--) {
      if (!top(x, y)) continue;
      if (y == 15 && Sth) break;
      if (top(x, y + 1)) break;
      wallFace(c, x, y + 1, 16, 77);
      // merlons on the front edge
      bool merlon = ((x + 1) / 3) % 2 == 0;
      if (merlon) { c.set(x, y - 1, S[4]); c.set(x, y - 2, S[3]); c.set(x, y, S[2]); }
      break;
    }
  // back merlons along exposed north edges
  for (int x = 0; x < 16; x++)
    for (int y = 0; y < 16; y++) {
      if (!top(x, y)) continue;
      if (y > 0 && !top(x, y - 1) && ((x + 1) / 3) % 2 == 0) { c.set(x, y - 1, S[4]); c.set(x, y - 2, S[3]); }
      break;
    }
  // moss streaks
  for (int x = 0; x < 16; x++)
    if (hash3(x, mask, 5) % 7 == 0)
      for (int y = 31; y > 16; y--) if (solid(c, x, y)) { c.set(x, y, kMoss[1]); c.set(x, y - 1, kMoss[2]); break; }
  outline(c);
  return c;
}

Canvas gatePiece() {
  Canvas c(48, 40);
  const Ramp& S = wallStone();
  const int ground = 39;
  // central wall section above the passage
  for (int y = 6; y < 12; y++) for (int x = 12; x < 36; x++) c.set(x, y, S[(x / 4 + y / 3) % 2 ? 3 : 2]);
  for (int x = 12; x < 36; x++) wallFace(c, x, 12, ground - 12 - 0, 91);
  crenellate(c, 12, 35, 9, S);
  // passage: arched opening, dark tunnel, raised portcullis
  int ax0 = 16, ax1 = 31;
  for (int y = 18; y <= ground; y++)
    for (int x = ax0; x <= ax1; x++) {
      float dx = (x + 0.5f - 24.0f) / 8.0f;
      float archY = 18 + (1 - std::sqrt(std::max(0.0f, 1 - dx * dx))) * 6.0f;
      if (y < archY) continue;
      float depth = (y - archY) / (ground - archY);
      uint32_t col = depth < 0.2f ? rgba(46, 36, 50) : rgba(28, 22, 36);
      if (y > ground - 3) col = kStone[1];   // paving visible in the passage
      c.set(x, y, col);
    }
  for (int x = ax0 + 1; x < ax1; x += 2) for (int y = 0; y < 4; y++) {
    float dx = (x + 0.5f - 24.0f) / 8.0f;
    int archY = (int)(18 + (1 - std::sqrt(std::max(0.0f, 1 - dx * dx))) * 6.0f);
    c.set(x, archY + y, y == 3 ? kIron[3] : kIron[1]);
  }
  hline(c, ax0 + 1, ax1 - 1, 24, kIron[2]);
  // arch voussoirs
  for (int a = 0; a <= 16; a++) {
    float t = a / 16.0f * PI;
    int x = (int)std::lround(24 - std::cos(t) * 9), y = (int)std::lround(24 - std::sin(t) * 7);
    c.set(x, y, S[4]); c.set(x, y - 1, S[2]);
  }
  // coat of arms over the arch
  for (int j = 0; j < 5; j++) for (int i = 0; i < 4; i++) c.set(22 + i, 13 + j, (j == 4 && (i == 0 || i == 3)) ? 0 : kRed[i < 2 ? 3 : 2]);
  c.set(23, 14, kGold[4]); c.set(24, 15, kGold[3]);
  // flanking towers
  for (int side = 0; side < 2; side++) {
    int tx0 = side == 0 ? 0 : 34, tx1 = tx0 + 13;
    for (int y = 3; y < 9; y++) for (int x = tx0; x <= tx1; x++) c.set(x, y, S[(x / 4 + y / 3) % 2 ? 3 : 2]);
    for (int x = tx0; x <= tx1; x++) wallFace(c, x, 9, ground - 9 + 1, 93 + side);
    crenellate(c, tx0, tx1, 4, S);
    // arrow slit + banner
    vline(c, tx0 + 6, 14, 19, kInk); vline(c, tx0 + 7, 14, 19, S[0]);
    wallBanner(c, tx0 + 4, 22, 12, kRed);
  }
  outline(c);
  return c;
}

// =====================================================================================================
// 7. item icons (16x16, outlined, top-left light). Weapons point up-right like a classic inventory.
// =====================================================================================================
namespace {

// diagonal blade from (x,y) going up-right for len pixels; width 1..3
void diagBlade(Canvas& c, int x, int y, int len, int width, const Ramp& m) {
  for (int i = 0; i < len; i++) {
    int px2 = x + i, py = y - i;
    bool tip = i == len - 1;
    c.set(px2, py, tip ? m[4] : m[3]);
    if (!tip) c.set(px2 + 1, py, m[1]);
    if (width >= 2 && !tip) c.set(px2, py - 1, m[4]);
    if (width >= 3 && i < len - 2) { c.set(px2 + 1, py + 1, m[0]); c.set(px2 + 2, py, m[1]); }
  }
}
void diagHandle(Canvas& c, int x, int y, int len, const Ramp& r) {
  for (int i = 0; i < len; i++) { c.set(x + i, y - i, r[(i & 1) ? 2 : 3]); c.set(x + i + 1, y - i, r[1]); }
}

void potion(Canvas& c, const Ramp& liquid) {
  // round flask: glass rim, coloured liquid, cork, glints
  ball(c, 8, 10.5f, 5.0f, 4.8f, liquid);
  for (int y = 5; y <= 7; y++) { c.set(7, y, kCrystal[4]); c.set(8, y, kCrystal[3]); }
  for (int y = 6; y <= 9; y++)
    for (int x = 3; x <= 13; x++)
      if (solid(c, x, y) && y < 8) c.set(x, y, withA(mix(kCrystal[3], liquid[2], 0.2f), 255));
  box(c, 6, 2, 9, 4, kWood[3]); hline(c, 6, 9, 2, kWood[4]); c.set(9, 3, kWood[1]); c.set(9, 4, kWood[1]);
  hline(c, 6, 9, 5, kCrystal[2]);
  c.set(5, 10, kWhite); c.set(5, 9, kWhite); c.set(6, 8, liquid[4]);
}

void paintIcon(Canvas& c, Icon ic, uint32_t tint) {
  const bool tinted = tint != 0;
  const Ramp M = tinted ? ramp(tint, 1.1f) : kIron;     // metal / main material
  switch (ic) {
    case Icon::Sword:
      diagBlade(c, 6, 9, 8, 2, M);
      line(c, 3, 8, 7, 12, kBrass[3]); c.set(4, 9, kBrass[4]); c.set(6, 11, kBrass[1]);   // guard
      diagHandle(c, 3, 12, 2, kLeather);
      c.set(2, 13, kBrass[3]); c.set(1, 14, kBrass[2]);
      break;
    case Icon::Greatsword:
      diagBlade(c, 5, 10, 10, 3, M);
      line(c, 1, 8, 7, 14, kBrass[3]); line(c, 2, 8, 7, 13, kBrass[4]); c.set(7, 14, kBrass[1]);
      diagHandle(c, 2, 13, 2, kLeather);
      c.set(1, 14, kBrass[3]); c.set(0, 15, kBrass[2]);
      break;
    case Icon::Dagger:
      diagBlade(c, 7, 8, 5, 2, M);
      line(c, 5, 8, 7, 10, kBrass[3]);
      diagHandle(c, 4, 11, 3, kLeather);
      c.set(3, 12, kBrass[3]);
      break;
    case Icon::Axe:
      diagHandle(c, 2, 14, 10, kWood);
      // crescent head on the upper end
      for (int y = 1; y <= 8; y++)
        for (int x = 6; x <= 14; x++) {
          float dx = x - 13.5f, dy = y - 1.0f;
          float d = std::sqrt(dx * dx + dy * dy);
          float d2 = std::sqrt((x - 15.5f) * (x - 15.5f) + (y + 1.5f) * (y + 1.5f));
          if (d < 7.2f && d2 > 4.2f && x + y > 12) c.set(x, y, M[d > 6.0f ? 4 : (d > 4.6f ? 3 : 2)]);
        }
      c.set(12, 4, M[1]); c.set(11, 4, M[1]);
      break;
    case Icon::Mace:
      diagHandle(c, 2, 14, 8, kWood);
      c.set(2, 14, kLeather[2]); c.set(3, 13, kLeather[3]);
      ball(c, 11.5f, 4.5f, 3.6f, 3.6f, M);
      for (int a = 0; a < 6; a++) {
        float ang = a * TAU / 6 + 0.3f;
        c.set((int)std::lround(11.5f + std::cos(ang) * 4.6f - 0.5f), (int)std::lround(4.5f + std::sin(ang) * 4.6f - 0.5f), M[4]);
      }
      break;
    case Icon::Bow: {
      for (int i = 0; i <= 12; i++) {
        float t = i / 12.0f;
        int x = (int)std::lround(2 + t * 11 - std::sin(t * PI) * 3.0f), y = (int)std::lround(13 - t * 11 - std::sin(t * PI) * 3.0f);
        c.set(x, y, kWood[t < 0.5f ? 3 : 2]); c.set(x + 1, y, kWood[1]);
      }
      line(c, 2, 13, 13, 2, kCloth[4]);
      c.set(7, 8, kLeather[3]); c.set(8, 7, kLeather[2]); c.set(8, 8, kLeather[1]);
      if (tinted) { c.set(2, 12, M[3]); c.set(12, 2, M[3]); }
      break;
    }
    case Icon::Staff:
      diagHandle(c, 1, 14, 10, kWood);
      ball(c, 12.5f, 3.5f, 2.8f, 2.8f, tinted ? M : kCrystal);
      c.set(12, 2, kWhite);
      c.set(10, 6, kGold[3]); c.set(11, 6, kGold[2]); c.set(10, 5, kGold[4]);
      break;
    case Icon::Arrows:
      for (int k = 0; k < 3; k++) {
        int ox = k * 3 - 3, oy = k * 1 - 1;
        line(c, 3 + ox + 2, 13 + oy, 12 + ox + 2, 4 + oy, kWood[3]);
        c.set(13 + ox + 2, 3 + oy, M[4]); c.set(12 + ox + 2, 3 + oy, M[2]); c.set(13 + ox + 2, 4 + oy, M[2]);
        c.set(3 + ox + 2, 12 + oy, kRed[3]); c.set(4 + ox + 2, 13 + oy, kRed[2]); c.set(3 + ox + 2, 14 + oy, kCloth[4]);
      }
      break;
    case Icon::Shield: {
      // heater shield: metal rim, painted field with a chevron
      for (int y = 1; y <= 14; y++)
        for (int x = 2; x <= 13; x++) {
          float dx = (x + 0.5f - 8) / 6.0f;
          float bottom = y > 8 ? (y - 8) / 6.5f : 0;
          if (std::fabs(dx) > 1.0f - bottom * bottom) continue;
          bool rim = std::fabs(dx) > 0.82f - bottom * bottom || y == 1 || (y > 12);
          int k = x < 8 ? 3 : 2;
          if (x > 11 || y > 11) k--;
          c.set(x, y, rim ? M[k + (x < 8 ? 1 : 0)] : kRed[k]);
        }
      for (int i = 0; i < 4; i++) { c.set(5 + i, 5 + i, kGold[3]); c.set(10 - i, 5 + i, kGold[2]); }
      c.set(4, 2, kWhite);
      break;
    }
    case Icon::Helmet:
      for (int y = 2; y <= 13; y++)
        for (int x = 2; x <= 13; x++) {
          float dx = (x + 0.5f - 8) / 5.6f, dy = (y + 0.5f - 8) / 6.0f;
          if (y < 8 ? (dx * dx + dy * dy > 1) : std::fabs(dx) > 1) continue;
          c.set(x, y, M[lightIndex(lightAt(dx * 0.85f, std::min(0.6f, dy) * 0.85f), x, y, 0.1f)]);
        }
      hline(c, 4, 11, 8, kInk); hline(c, 4, 11, 9, M[0]);   // visor slit
      vline(c, 7, 9, 13, M[3]); vline(c, 8, 9, 13, M[1]);   // nasal guard
      vline(c, 8, 1, 3, kRed[3]); c.set(9, 1, kRed[2]); c.set(9, 2, kRed[1]);
      break;
    case Icon::Armor:
      for (int y = 2; y <= 14; y++)
        for (int x = 1; x <= 14; x++) {
          bool shoulders = y <= 5 && (x <= 4 || x >= 11) && y >= 2;
          bool torso = x >= 4 && x <= 11 && y >= 3;
          bool neck = y < 5 && x >= 6 && x <= 9;
          if (!(shoulders || torso) || neck) continue;
          int k = x < 6 ? 3 : (x > 10 ? 1 : 2);
          if (shoulders && y == 2) k = 4;
          c.set(x, y, M[k]);
        }
      vline(c, 7, 6, 13, M[3]); vline(c, 8, 6, 13, M[1]);
      hline(c, 4, 11, 11, kLeather[1]); c.set(7, 11, kGold[3]); c.set(8, 11, kGold[2]);
      c.set(5, 6, M[4]);
      break;
    case Icon::Boots:
      for (int b = 0; b < 2; b++) {
        int ox = b * 6;
        for (int y = 3; y <= 13; y++) for (int x = 2; x <= 5; x++) c.set(x + ox, y, (tinted ? M : kLeather)[x == 2 ? 3 : (x == 5 ? 1 : 2)]);
        for (int x = 2; x <= 8; x++) { c.set(x + ox, 13, (tinted ? M : kLeather)[1]); c.set(x + ox, 12, (tinted ? M : kLeather)[x < 5 ? 2 : 3]); }
        hline(c, 2 + ox, 5 + ox, 3, (tinted ? M : kLeather)[4]);
        hline(c, 2 + ox, 5 + ox, 5, kLeather[0]);
      }
      break;
    case Icon::Gloves: {
      // gauntlet: four fingers, thumb, knuckle plate, leather cuff
      const Ramp& G = M;
      for (int f = 0; f < 4; f++) {
        int x = 5 + f * 2, top = f == 0 || f == 3 ? 4 : 2;
        for (int y = top; y <= 7; y++) c.set(x, y, G[y == top ? 4 : (f < 2 ? 3 : 2)]);
        c.set(x, 5, G[1]);
      }
      for (int y = 7; y <= 11; y++) for (int x = 4; x <= 11; x++) c.set(x, y, G[x < 6 ? 3 : (x > 9 ? 1 : 2)]);
      hline(c, 4, 11, 7, G[4]);
      for (int k = 0; k < 3; k++) { c.set(3 - k / 2, 8 + k, G[3]); c.set(2, 7 + k, G[2]); }
      for (int y = 12; y <= 14; y++) for (int x = 3; x <= 12; x++) c.set(x, y, kLeather[y == 12 ? 4 : (x < 6 ? 3 : 2)]);
      break;
    }
    case Icon::Ring: {
      const Ramp& Rg = tinted ? M : kGold;
      for (int a = 0; a < 48; a++) {
        float ang = a / 48.0f * TAU;
        int x = (int)std::lround(8 + std::cos(ang) * 4.5f - 0.5f), y = (int)std::lround(9.5f + std::sin(ang) * 3.6f - 0.5f);
        int k = lightIndex(lightAt(std::cos(ang) * 0.8f, std::sin(ang) * 0.8f), x, y, 0);
        c.set(x, y, Rg[k]);
        c.set(x, y + 1, Rg[std::max(0, k - 1)]);
      }
      ball(c, 8, 4.5f, 2.4f, 2.2f, ramp(rgba(220, 50, 80)));
      c.set(7, 3, kWhite);
      break;
    }
    case Icon::Amulet: {
      const Ramp& Rg = tinted ? M : kGold;
      for (int i = 0; i <= 10; i++) {
        float t = i / 10.0f;
        int x = (int)std::lround(3 + t * 10), y = (int)std::lround(2 + std::sin(t * PI) * 5);
        c.set(x, y, Rg[(i & 1) ? 2 : 4]);
      }
      ball(c, 8, 11, 3.4f, 3.6f, Rg);
      ball(c, 8, 11, 2.0f, 2.2f, kCrystal);
      c.set(7, 10, kWhite);
      break;
    }
    case Icon::PotionRed: potion(c, tinted ? M : ramp5(rgba(96, 18, 40), rgba(156, 28, 44), rgba(214, 50, 52), rgba(244, 96, 80), rgba(255, 170, 140))); break;
    case Icon::PotionBlue: potion(c, tinted ? M : ramp5(rgba(24, 30, 96), rgba(36, 60, 160), rgba(56, 104, 214), rgba(100, 156, 244), rgba(180, 216, 255))); break;
    case Icon::PotionGreen: potion(c, tinted ? M : ramp5(rgba(20, 70, 48), rgba(34, 120, 56), rgba(64, 176, 70), rgba(120, 220, 96), rgba(200, 250, 170))); break;
    case Icon::Bread: {
      const Ramp Br = ramp5(rgba(110, 54, 34), rgba(166, 92, 44), rgba(212, 140, 64), rgba(236, 186, 104), rgba(250, 226, 160));
      ball(c, 8, 9, 6.5f, 4.2f, Br);
      for (int k = 0; k < 3; k++) { c.set(5 + k * 3, 7, Br[4]); c.set(6 + k * 3, 8, Br[1]); c.set(6 + k * 3, 6, Br[4]); }
      break;
    }
    case Icon::Meat: {
      const Ramp Mt = ramp5(rgba(90, 24, 34), rgba(146, 46, 42), rgba(192, 82, 60), rgba(222, 124, 88), rgba(244, 172, 136));
      ball(c, 6.5f, 7.5f, 5.0f, 4.6f, Mt);
      capsule(c, V(9, 10), V(13, 13), 1.2f, 1.0f, kBone);
      ball(c, 13.5f, 13, 1.6f, 1.4f, kBone); ball(c, 12.8f, 14.2f, 1.3f, 1.2f, kBone);
      c.set(4, 5, Mt[4]); c.set(5, 5, Mt[4]); c.set(7, 9, Mt[1]);
      break;
    }
    case Icon::Apple: {
      const Ramp Ap = tinted ? M : kRed;
      ball(c, 8, 9.5f, 5.2f, 4.9f, Ap);
      c.set(8, 5, Ap[0]); vline(c, 8, 2, 4, kWood[1]);
      c.set(9, 3, kLeaf[3]); c.set(10, 3, kLeaf[3]); c.set(11, 2, kLeaf[2]); c.set(10, 2, kLeaf[4]);
      c.set(5, 7, kWhite); c.set(5, 8, Ap[4]);
      break;
    }
    case Icon::Cheese: {
      const Ramp Ch = ramp5(rgba(150, 100, 30), rgba(206, 150, 40), rgba(240, 196, 70), rgba(252, 226, 116), rgba(255, 246, 180));
      poly(c, {{1.5f, 11.5f}, {13.5f, 5.0f}, {14.5f, 11.5f}, {14.5f, 14.0f}, {1.5f, 14.0f}}, Ch[2]);
      poly(c, {{1.5f, 11.5f}, {13.5f, 5.0f}, {14.5f, 11.5f}}, Ch[4]);
      hline(c, 2, 14, 12, Ch[3]);
      c.set(5, 13, Ch[0]); c.set(10, 13, Ch[0]); c.set(11, 13, Ch[1]); c.set(8, 10, Ch[1]); c.set(12, 9, Ch[1]);
      break;
    }
    case Icon::Gold: {
      const Ramp& G = tinted ? M : kGold;
      const float cs[6][2] = {{5, 12}, {10, 12}, {7.5f, 9.5f}, {12, 9}, {4, 9}, {8, 6.5f}};
      for (auto& p : cs) {
        ellipse(c, p[0], p[1], 3.2f, 1.8f, G[1]);
        ellipse(c, p[0], p[1] - 0.6f, 3.0f, 1.5f, G[3]);
        c.set((int)p[0] - 1, (int)p[1] - 1, G[4]);
      }
      break;
    }
    case Icon::Gem: {
      const Ramp& Gm = tinted ? M : ramp5(rgba(30, 50, 110), rgba(36, 100, 180), rgba(60, 160, 230), rgba(130, 214, 250), rgba(230, 252, 255));
      poly(c, {{8, 2}, {14, 7}, {8, 15}, {2, 7}}, Gm[2]);
      poly(c, {{8, 2}, {8, 7}, {2, 7}}, Gm[4]);
      poly(c, {{8, 2}, {14, 7}, {8, 7}}, Gm[3]);
      poly(c, {{2, 7}, {8, 7}, {8, 15}}, Gm[2]);
      poly(c, {{8, 7}, {14, 7}, {8, 15}}, Gm[1]);
      c.set(5, 5, kWhite);
      break;
    }
    case Icon::Key: {
      const Ramp& K = tinted ? M : kBrass;
      for (int a = 0; a < 24; a++) {
        float ang = a / 24.0f * TAU;
        c.set((int)std::lround(4.5f + std::cos(ang) * 2.6f), (int)std::lround(4.5f + std::sin(ang) * 2.6f), K[std::sin(ang) < 0 ? 4 : 2]);
      }
      line(c, 6, 6, 13, 13, K[3]); line(c, 7, 6, 13, 12, K[1]);
      c.set(11, 13, K[2]); c.set(10, 14, K[2]); c.set(13, 11, K[2]); c.set(14, 10, K[1]);
      break;
    }
    case Icon::Scroll:
      for (int y = 3; y <= 12; y++) for (int x = 3; x <= 12; x++) c.set(x, y, kCloth[x < 5 ? 4 : (x > 10 ? 2 : 3)]);
      for (int x = 2; x <= 13; x++) { c.set(x, 2, kCloth[4]); c.set(x, 3, kCloth[2]); c.set(x, 13, kCloth[2]); c.set(x, 12, kCloth[3]); }
      for (int y = 5; y <= 10; y += 2) hline(c, 5, 10 - (y % 4 == 1 ? 2 : 0), y, kCloth[0]);
      vline(c, 8, 1, 14, kRed[2]); c.set(9, 13, kRed[1]); c.set(7, 14, kRed[3]);
      break;
    case Icon::Book: {
      const Ramp Bk = tinted ? M : ramp5(rgba(56, 22, 34), rgba(90, 30, 40), rgba(130, 44, 48), rgba(170, 70, 60), rgba(206, 110, 86));
      for (int y = 2; y <= 13; y++) for (int x = 3; x <= 12; x++) c.set(x, y, Bk[x == 3 ? 1 : (x < 6 ? 3 : 2)]);
      for (int y = 3; y <= 13; y++) c.set(13, y, kCloth[(y & 1) ? 3 : 2]);
      hline(c, 4, 13, 14, kCloth[2]);
      vline(c, 4, 2, 13, Bk[0]);
      box(c, 7, 6, 10, 9, kGold[2]); c.set(7, 6, kGold[4]); c.set(8, 7, kGold[1]);
      break;
    }
    case Icon::Map:
      poly(c, {{1, 3}, {5, 2}, {10, 3}, {15, 2}, {15, 13}, {10, 14}, {5, 13}, {1, 14}}, kCloth[3]);
      poly(c, {{5, 2}, {10, 3}, {10, 14}, {5, 13}}, kCloth[2]);
      line(c, 3, 11, 6, 8, kRed[2]); c.set(8, 7, kRed[2]); c.set(9, 6, kRed[2]); c.set(10, 7, kRed[2]);
      c.set(12, 5, kRed[1]); c.set(13, 6, kRed[1]); c.set(12, 6, kRed[3]); c.set(13, 5, kRed[3]);
      c.set(3, 5, kLeaf[2]); c.set(4, 5, kLeaf[1]); c.set(3, 4, kLeaf[3]);
      break;
    case Icon::Pelt: {
      const Ramp Pl = ramp5(rgba(64, 40, 38), rgba(104, 68, 50), rgba(146, 102, 70), rgba(182, 140, 96), rgba(214, 180, 130));
      furBall(c, 8, 8, 5.5f, 5.0f, Pl, 0.5f, 3);
      for (int k : {-1, 1}) { capsule(c, V(8 + k * 4, 5), V(8 + k * 7, 2), 1.4f, 0.8f, Pl); capsule(c, V(8 + k * 4, 11), V(8 + k * 7, 14), 1.4f, 0.8f, Pl); }
      capsule(c, V(8, 12), V(9, 15), 1.0f, 0.5f, Pl, -1);
      break;
    }
    case Icon::Bone:
      capsule(c, V(4, 12), V(12, 4), 1.3f, 1.3f, kBone);
      ball(c, 3, 11.5f, 1.8f, 1.8f, kBone); ball(c, 4.5f, 13, 1.8f, 1.8f, kBone);
      ball(c, 11.5f, 3, 1.8f, 1.8f, kBone); ball(c, 13, 4.5f, 1.8f, 1.8f, kBone);
      break;
    case Icon::Ore:
      rock(c, 8, 9, 6.5f, 5.5f, kStone, 41, 5);
      for (int i = 0; i < 6; i++) {
        int x = 4 + (int)(hash3(i, 0, 9) % 8), y = 6 + (int)(hash3(i, 1, 9) % 6);
        if (solid(c, x, y)) { c.set(x, y, (tinted ? M : kGold)[4]); c.set(x + 1, y, (tinted ? M : kGold)[2]); }
      }
      break;
    case Icon::Herb:
      vline(c, 8, 6, 14, kLeaf[1]);
      for (int k = 0; k < 3; k++) {
        int y = 4 + k * 3;
        ellipse(c, 5.5f, y + 1.0f, 2.5f, 1.4f, (tinted ? M : kLeaf)[3 - (k & 1)]);
        ellipse(c, 10.5f, y + 2.0f, 2.5f, 1.4f, (tinted ? M : kLeaf)[2]);
      }
      c.set(8, 2, rgba(240, 220, 250)); c.set(7, 3, rgba(200, 150, 230)); c.set(9, 3, rgba(200, 150, 230));
      break;
    case Icon::Sigil: {
      const Ramp& Sg = tinted ? M : ramp5(rgba(40, 30, 90), rgba(70, 50, 150), rgba(110, 90, 210), rgba(160, 150, 240), rgba(230, 230, 255));
      ball(c, 8, 8, 6.5f, 6.5f, kStone);
      for (int a = 0; a < 5; a++) {   // glowing pentagram-like rune
        float a0 = -PI / 2 + a * TAU / 5, a1 = -PI / 2 + (a + 2) * TAU / 5;
        line(c, (int)std::lround(8 + std::cos(a0) * 4.5f - 0.5f), (int)std::lround(8 + std::sin(a0) * 4.5f - 0.5f),
             (int)std::lround(8 + std::cos(a1) * 4.5f - 0.5f), (int)std::lround(8 + std::sin(a1) * 4.5f - 0.5f), Sg[3]);
      }
      c.set(7, 7, Sg[4]); c.set(8, 8, Sg[4]);
      break;
    }
    case Icon::Letter:
      for (int y = 4; y <= 12; y++) for (int x = 1; x <= 14; x++) c.set(x, y, kCloth[x < 3 ? 4 : 3]);
      line(c, 1, 4, 8, 9, kCloth[1]); line(c, 14, 4, 8, 9, kCloth[1]);
      line(c, 1, 12, 6, 8, kCloth[2]); line(c, 14, 12, 10, 8, kCloth[2]);
      ball(c, 8, 9.5f, 1.9f, 1.8f, kRed); c.set(7, 9, kRed[4]);
      break;
    case Icon::Crown: {
      const Ramp& G = tinted ? M : kGold;
      for (int y = 8; y <= 13; y++) for (int x = 2; x <= 13; x++) c.set(x, y, G[x < 5 ? 4 : (x > 11 ? 2 : 3)]);
      for (int p = 0; p < 3; p++) {
        int px2 = 3 + p * 5;
        for (int y = 3; y < 8; y++) { int half = (y - 3) / 2; for (int x = px2 - half; x <= px2 + half; x++) c.set(x, y, G[x <= px2 ? 3 : 2]); }
        c.set(px2, 2, G[4]);
      }
      hline(c, 2, 13, 12, G[1]); hline(c, 2, 13, 13, G[0]);
      c.set(5, 10, rgba(220, 50, 70)); c.set(8, 10, rgba(80, 160, 240)); c.set(11, 10, rgba(80, 200, 110));
      c.set(4, 9, kWhite);
      break;
    }
    default: break;
  }
}

}  // namespace

Canvas itemIcon(Icon i, uint32_t tint) {
  Canvas c(ICON, ICON);
  paintIcon(c, i, tint);
  outline(c);
  return c;
}

// =====================================================================================================
// 8. effects
// =====================================================================================================
namespace {

const Ramp kSteelFx = ramp5(rgba(90, 110, 160), rgba(150, 176, 214), rgba(200, 220, 240), rgba(232, 242, 252), rgba(255, 255, 255));

// sword arc: a crescent swept clockwise between tail and head angles (radians, 0 = right, +y down),
// rotated by `base`. Bright white at the leading edge, silver-blue and fading toward the tail.
void slashArc(Canvas& c, float cx, float cy, float R, float tail, float head, float width, float base) {
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
      float r = std::sqrt(dx * dx + dy * dy);
      float a = std::atan2(dy, dx) - base;
      while (a < tail - PI) a += TAU;
      while (a > tail + PI) a -= TAU;
      if (a < tail || a > head) continue;
      float t = (a - tail) / std::max(0.01f, head - tail);     // 0 tail .. 1 head
      float w = width * (0.25f + 0.75f * t);
      if (r > R || r < R - w) continue;
      float edge = (R - r) / w;                                  // 0 outer rim .. 1 inner
      int k = t > 0.75f ? 4 : (t > 0.45f ? 3 : (t > 0.2f ? 2 : 1));
      if (edge > 0.6f) k = std::max(0, k - 1);
      int alpha = (int)(150 + 105 * t);
      if (edge < 0.25f && t > 0.3f) k = 4;   // bright cutting edge on the outer rim
      c.set(x, y, withA(kSteelFx[k], alpha));
    }
}

void puff(Canvas& c, float cx, float cy, float r, const Ramp& R, int alpha, uint32_t seed) {
  for (int y = (int)(cy - r - 1); y <= (int)(cy + r + 1); y++)
    for (int x = (int)(cx - r - 1); x <= (int)(cx + r + 1); x++) {
      float dx = (x + 0.5f - cx) / r, dy = (y + 0.5f - cy) / r;
      float d = dx * dx + dy * dy;
      float edge = 1.0f + (hashf(x, y, seed) - 0.5f) * 0.3f;
      if (d > edge) continue;
      int k = lightIndex(lightAt(dx * 0.85f, dy * 0.85f), x, y, 0.15f);
      c.set(x, y, withA(R[k], alpha));
    }
}

void paintFx(Canvas& c, Fx f, int frame) {
  switch (f) {
    case Fx::Slash: case Fx::SlashDown: case Fx::SlashUp: {
      float base = f == Fx::Slash ? 0.0f : (f == Fx::SlashDown ? PI * 0.5f : -PI * 0.5f);
      static const float tails[4] = {-1.9f, -1.7f, -0.6f, 0.5f};
      static const float heads[4] = {-1.1f, 0.1f, 1.1f, 1.5f};
      static const float widths[4] = {3.5f, 6.5f, 6.0f, 3.5f};
      slashArc(c, 10.0f, 12.0f, 11.0f, tails[frame], heads[frame], widths[frame], base);
      if (frame == 1 || frame == 2) {   // glint at the leading tip
        float a = heads[frame] + base;
        int gx = (int)(10.0f + std::cos(a) * 9.5f), gy = (int)(12.0f + std::sin(a) * 9.5f);
        c.set(gx, gy, kWhite); c.set(gx + 1, gy, withA(kWhite, 180)); c.set(gx - 1, gy, withA(kWhite, 180));
        c.set(gx, gy + 1, withA(kWhite, 180)); c.set(gx, gy - 1, withA(kWhite, 180));
      }
      break;
    }
    case Fx::Arrow:
      hline(c, 2, 9, 1, kWood[3]);
      c.set(11, 1, kIron[4]); c.set(10, 0, kIron[3]); c.set(10, 1, kIron[3]); c.set(10, 2, kIron[2]); c.set(9, 1, kIron[2]);
      c.set(0, 0, kCloth[4]); c.set(1, 0, kRed[3]); c.set(0, 2, kCloth[3]); c.set(1, 2, kRed[2]); c.set(2, 0, kRed[3]); c.set(2, 2, kRed[1]);
      c.set(0, 1, kWood[2]);
      break;
    case Fx::ArrowDown:
      vline(c, 1, 2, 9, kWood[3]);
      c.set(1, 11, kIron[4]); c.set(0, 10, kIron[3]); c.set(1, 10, kIron[3]); c.set(2, 10, kIron[2]); c.set(1, 9, kIron[2]);
      c.set(0, 0, kCloth[4]); c.set(0, 1, kRed[3]); c.set(2, 0, kCloth[3]); c.set(2, 1, kRed[2]); c.set(0, 2, kRed[3]); c.set(2, 2, kRed[1]);
      c.set(1, 0, kWood[2]);
      break;
    case Fx::Fireball: {
      // trailing flames to the left, bright core on the right
      for (int i = 0; i < 6; i++) {
        float t = i / 5.0f;
        float x = 7.5f - t * 6.0f, y = 6.0f + std::sin(frame * 1.6f + i * 1.3f) * t * 1.6f;
        float r = 3.6f - t * 2.6f;
        for (int yy = (int)(y - r); yy <= (int)(y + r); yy++)
          for (int xx = (int)(x - r); xx <= (int)(x + r); xx++) {
            float d = std::hypot(xx + 0.5f - x, yy + 0.5f - y) / r;
            if (d > 1) continue;
            int k = d < 0.4f ? 3 : 2;
            if (t > 0.5f) k--;
            c.set(xx, yy, withA(kFire[k], (int)(255 - t * 90)));
          }
      }
      ball(c, 8.0f, 6.0f, 3.4f, 3.4f, ramp5(kFire[1], kFire[2], kFire[3], kFire[4], kWhite), 0.2f, 1);
      c.set(7, 4 + (frame & 1), kWhite);
      break;
    }
    case Fx::Explosion: {
      float cx = 16, cy = 17;
      static const float fireR[6] = {5, 10, 13, 11, 0, 0};
      static const float smokeR[6] = {0, 0, 9, 12, 13, 12};
      static const int smokeA[6] = {0, 0, 200, 210, 160, 90};
      const Ramp Smoke = ramp5(rgba(40, 32, 44), rgba(70, 60, 70), rgba(104, 94, 100), rgba(140, 130, 132), rgba(176, 168, 166));
      if (smokeR[frame] > 0)
        for (int i = 0; i < 6; i++) {
          float a = i * TAU / 6 + frame * 0.3f;
          float d = smokeR[frame] * 0.55f;
          puff(c, cx + std::cos(a) * d, cy + std::sin(a) * d * 0.8f - frame, smokeR[frame] * 0.5f, Smoke, smokeA[frame], 7 + i);
        }
      if (fireR[frame] > 0) {
        float r = fireR[frame];
        for (int y = 0; y < c.h; y++)
          for (int x = 0; x < c.w; x++) {
            float d = std::hypot(x + 0.5f - cx, (y + 0.5f - cy) * 1.1f) / r;
            float n = (vnoise(x * 0.35f, y * 0.35f, 5 + frame) - 0.5f) * 0.5f;
            d += n;
            if (d > 1) continue;
            int k = d < 0.3f ? 4 : (d < 0.55f ? 3 : (d < 0.8f ? 2 : 1));
            if (frame == 3) k = std::max(0, k - 1);
            c.set(x, y, kFire[k]);
          }
      }
      if (frame == 0) for (int a = 0; a < 8; a++) {   // flash rays
        float ang = a * TAU / 8;
        for (int k = 5; k < 9; k++) c.set((int)(cx + std::cos(ang) * k), (int)(cy + std::sin(ang) * k), withA(kFire[4], 220 - k * 15));
      }
      if (frame >= 2 && frame <= 4)   // flying embers
        for (int i = 0; i < 8; i++) {
          float ang = hashf(i, 0, 3) * TAU, dist = 8 + frame * 3.0f + hashf(i, 1, 3) * 3;
          int x = (int)(cx + std::cos(ang) * dist), y = (int)(cy + std::sin(ang) * dist + frame);
          c.set(x, y, kFire[frame == 4 ? 2 : 3]);
        }
      break;
    }
    case Fx::Sparkle: {
      static const int sz[4] = {1, 3, 2, 1};
      static const int al[4] = {200, 255, 230, 140};
      int s = sz[frame];
      uint32_t gold = withA(kGold[4], al[frame]), white = withA(kWhite, al[frame]);
      for (int k = 1; k <= s; k++) { c.set(4 + k, 4, gold); c.set(3 - k + 1 - 1, 4, gold); c.set(4, 4 + k, gold); c.set(4, 4 - k, gold); }
      c.set(4, 4, white);
      if (frame == 1) { c.set(3, 3, withA(kGold[3], 160)); c.set(5, 5, withA(kGold[3], 160)); c.set(5, 3, withA(kGold[3], 160)); c.set(3, 5, withA(kGold[3], 160)); }
      if (frame == 3) { c.set(1, 6, withA(kGold[4], 120)); c.set(6, 1, withA(kGold[4], 100)); }
      break;
    }
    case Fx::Blood: {
      static const float spread[4] = {1.0f, 2.2f, 3.2f, 3.6f};
      static const float fall[4] = {0, 0.5f, 1.5f, 3.0f};
      for (int i = 0; i < 6; i++) {
        float ang = -PI * 0.15f - hashf(i, 0, 11) * PI * 0.7f;
        float d = spread[frame] * (0.6f + hashf(i, 1, 11) * 0.6f);
        int x = (int)(4 + std::cos(ang) * d), y = (int)(4 + std::sin(ang) * d + fall[frame]);
        uint32_t col = frame < 3 ? kRed[(i & 1) ? 2 : 1] : withA(kRed[1], 170);
        c.set(x, y, col);
        if (frame < 2) c.set(x, y + 1, kRed[0]);
      }
      if (frame == 0) { c.set(3, 4, kRed[3]); c.set(4, 4, kRed[2]); c.set(4, 3, kRed[2]); }
      if (frame >= 2) { c.set(3, 7, withA(kRed[1], 200)); c.set(4, 7, withA(kRed[0], 200)); }
      break;
    }
    case Fx::Dust: {
      const Ramp D = ramp5(rgba(120, 104, 90), rgba(156, 140, 118), rgba(190, 176, 150), rgba(214, 204, 180), rgba(236, 230, 210));
      static const float r[4] = {1.6f, 2.3f, 2.8f, 3.0f};
      static const int a[4] = {230, 200, 150, 80};
      puff(c, 3.0f, 5.5f - frame * 0.3f, r[frame] * 0.8f, D, a[frame], 3);
      puff(c, 5.2f, 5.0f - frame * 0.5f, r[frame] * 0.7f, D, a[frame], 4);
      break;
    }
    case Fx::Frost: {
      // spinning ice shard: diamond core + orbiting flakes
      poly(c, {{11.0f, 6.0f}, {6.0f, 3.5f}, {1.5f, 6.0f}, {6.0f, 8.5f}}, kCrystal[3]);
      poly(c, {{11.0f, 6.0f}, {6.0f, 3.5f}, {6.0f, 6.0f}}, kCrystal[4]);
      poly(c, {{6.0f, 6.0f}, {6.0f, 8.5f}, {11.0f, 6.0f}}, kCrystal[2]);
      c.set(9, 5, kWhite);
      for (int i = 0; i < 3; i++) {
        float ang = frame * 0.8f + i * TAU / 3;
        int x = (int)(5 + std::cos(ang) * 4.5f), y = (int)(6 + std::sin(ang) * 4.5f);
        c.set(x, y, withA(kSnow[4], 230));
        if (i == frame % 3) { c.set(x + 1, y, withA(kSnow[3], 160)); c.set(x, y + 1, withA(kSnow[3], 160)); }
      }
      break;
    }
    case Fx::Heal: {
      const Ramp G = ramp5(rgba(30, 110, 60), rgba(60, 170, 80), rgba(110, 220, 110), rgba(170, 250, 160), rgba(236, 255, 220));
      static const int ys[3] = {11, 7, 3};
      for (int i = 0; i < 3; i++) {
        int x = 3 + i * 5, y = ys[(i + frame) % 3] + (frame & 1);
        int alpha = 255 - ((i + frame) % 3) * 70;
        hline(c, x - 1, x + 1, y, withA(G[3], alpha)); vline(c, x, y - 1, y + 1, withA(G[3], alpha));
        c.set(x, y, withA(G[4], alpha));
        c.set(x + 1, y + 1, withA(G[1], alpha));
      }
      for (int i = 0; i < 2; i++) c.set(2 + i * 9 + frame, 14 - frame * 3 + i * 2, withA(G[4], 180));
      break;
    }
    default: break;
  }
}

struct FxInfo { uint8_t w, h, frames; };
const FxInfo kFxInfo[(int)Fx::COUNT] = {
  {24, 24, 4}, {24, 24, 4}, {24, 24, 4}, {12, 3, 1}, {3, 12, 1}, {12, 12, 4}, {32, 32, 6}, {8, 8, 4}, {8, 8, 4}, {8, 8, 4}, {12, 12, 4}, {16, 16, 4},
};

}  // namespace

int fxW(Fx f) { return (int)f < (int)Fx::COUNT ? kFxInfo[(int)f].w : 8; }
int fxH(Fx f) { return (int)f < (int)Fx::COUNT ? kFxInfo[(int)f].h : 8; }
int fxFrames(Fx f) { return (int)f < (int)Fx::COUNT ? kFxInfo[(int)f].frames : 1; }

Canvas fxSprite(Fx f) {
  const int w = fxW(f), h = fxH(f), n = fxFrames(f);
  Canvas sheet(w * n, h);
  for (int i = 0; i < n; i++) {
    Canvas cell(w, h);
    paintFx(cell, f, i);
    place(sheet, cell, i, 0);
  }
  return sheet;
}

}  // namespace art
