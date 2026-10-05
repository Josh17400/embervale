// EMBERVALE art internals: the shared palette ramps and canvas primitives used by every rpg/art/*.cpp painter.
// Not part of the public API (that is rpg/art.h). Everything here is in an unnamed namespace, so each painter file
// gets its own copy; keep it to small helpers and constants.
//
// Art files (one owner each during a milestone):
//   art_core.cpp      shade(), mix()
//   art_human.cpp     humans (player, NPCs, bandits) and the humanoid monsters that share the rig
//   art_monsters.cpp  monsters
//   art_props.cpp     props (nature, camp/town, dungeon, interior furniture)
//   art_building.cpp  buildings, roofs, city walls and the gate
//   art_items.cpp     item icons
//   art_fx.cpp        effects
#pragma once
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "rpg/art.h"

namespace art {

static inline int chR(uint32_t c) { return (int)(c & 255); }
static inline int chG(uint32_t c) { return (int)((c >> 8) & 255); }
static inline int chB(uint32_t c) { return (int)((c >> 16) & 255); }
static inline int chA(uint32_t c) { return (int)((c >> 24) & 255); }
static inline int clamp255(float v) { return v < 0 ? 0 : (v > 255 ? 255 : (int)(v + 0.5f)); }

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


// ---- shared shape helpers (monsters and the humanoid rig) ----------------------------------------------
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

}  // namespace

// humanoid monsters (skeleton, draugr, goblin) are painted by the human rig in art_human.cpp
enum Species { kHuman = 0, kSkeletonSp, kDraugrSp, kGoblinSp };
Canvas humanoidCell(Species sp, int frame);
// one standing human figure (HUMAN_W x HUMAN_H, idle pose, not outlined): statues and other props reuse the rig
Canvas humanFigureStill(const HumanLook& look, int facing);

}  // namespace art
