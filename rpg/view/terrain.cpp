// Terrain painting: every ground pixel is computed procedurally and baked into 32x32-tile chunk textures.
// Natural terrains get organic, domain-warped borders; built surfaces (roads, floors) stay crisp.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <thread>
#include <utility>
#include <vector>
#include "rpg/view/view.h"

namespace {
constexpr int CH = 32;   // tiles per chunk side

// No bake threads: the web build, or a desktop run with EMB_WEBSIM=1 (the web's bake and arrival path measured on
// desktop: the incremental pump on the main thread, building sprites painted inline)
bool singleThread() {
#ifdef __EMSCRIPTEN__
  return true;
#else
  static const bool on = std::getenv("EMB_WEBSIM") != nullptr;
  return on;
#endif
}

inline uint32_t C(int r, int g, int b, int a = 255) { return rgba(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255), a); }
inline uint32_t lerpc(uint32_t a, uint32_t b, float t) {
  int ar = a & 255, ag = (a >> 8) & 255, ab = (a >> 16) & 255, aa = (a >> 24) & 255;
  int br = b & 255, bg = (b >> 8) & 255, bb = (b >> 16) & 255, ba = (b >> 24) & 255;
  return C((int)(ar + (br - ar) * t), (int)(ag + (bg - ag) * t), (int)(ab + (bb - ab) * t), (int)(aa + (ba - aa) * t));
}
inline uint32_t mul(uint32_t c, float k) {
  return C((int)((c & 255) * k), (int)(((c >> 8) & 255) * k), (int)(((c >> 16) & 255) * k), (int)(c >> 24));
}
inline uint32_t pick3(float n, uint32_t a, uint32_t b, uint32_t c) { return n < 0.40f ? a : (n < 0.62f ? b : c); }
const float kBayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
// smooth large-scale tone field, ordered-dithered at the band edges (classic 16-bit look)
inline float toneField(int px, int py, uint32_t seed) {
  float n = vnoise(px / 13.0f, py / 13.0f, seed) * 0.72f + vnoise(px / 4.5f, py / 4.5f, seed + 1) * 0.28f;
  return n + (kBayer[(py & 3) * 4 + (px & 3)] / 16.0f - 0.5f) * 0.10f;
}
// small grass tufts: a dark root pixel with two lit blade tips, one per 6x6 cell at most
inline int tuft(int px, int py, uint32_t seed, float density) {
  int cx = px / 6, cy = py / 6;
  uint32_t h = hash2(cx, cy, seed);
  if ((h & 1023) / 1024.0f > density) return 0;
  int hx = cx * 6 + 1 + (int)((h >> 10) % 4), hy = cy * 6 + 2 + (int)((h >> 14) % 3);
  if (px == hx && py == hy) return 1;
  if ((px == hx - 1 || px == hx + 1) && py == hy - 1) return 2;
  if (px == hx && py == hy - 2 && (h >> 20) & 1) return 2;
  return 0;
}

bool soft(Ground g) {
  switch (g) {
    case Ground::Grass: case Ground::Meadow: case Ground::ForestFloor: case Ground::Autumn: case Ground::Tundra: case Ground::Snow:
    case Ground::Sand: case Ground::Swamp: case Ground::Dirt: case Ground::Water: case Ground::DeepWater: case Ground::CaveFloor: case Ground::Ice:
      return true;
    default: return false;
  }
}
bool natural(Ground g) { return soft(g) || g == Ground::Rock || g == Ground::CaveWall; }
// soft land that blends into its neighbours across a dithered ecotone (not water, not built or worked ground)
bool ecoGround(Ground g) {
  switch (g) {
    case Ground::Grass: case Ground::Meadow: case Ground::ForestFloor: case Ground::Autumn: case Ground::Tundra: case Ground::Snow:
    case Ground::Sand: case Ground::Swamp:
      return true;
    default: return false;
  }
}
inline float bayer(int px, int py) { return kBayer[(py & 3) * 4 + (px & 3)] / 16.0f; }
// (M2 fixer) where rock meets the land out on the overworld, the outline is looked up through this smooth warp: a
// small swing plus a broader, rounder one, so a crag's outline never steps along the tile grid. Its cast shadow uses
// the same warp, so the two agree.
inline void rockWarp(int qx, int qy, float& wx, float& wy) {
  wx = qx + (vnoise(qx / 7.0f, qy / 7.0f, 11) - 0.5f) * 5.0f + (vnoise(qx / 15.0f, qy / 15.0f, 15) - 0.5f) * 9.0f;
  wy = qy + (vnoise(qx / 7.0f, qy / 7.0f, 23) - 0.5f) * 5.0f + (vnoise(qx / 15.0f, qy / 15.0f, 27) - 0.5f) * 7.0f;
}

// The rock of a cliff face by biome (VISION_PLAN 11): lip (lit top edge), hi, mid, lo, dark (the foot).
struct RockPal { uint32_t lip, hi, mid, lo, dark; bool cold; };
RockPal rockPal(Biome b) {
  switch (b) {
    case Biome::Desert: return {C(232, 196, 142), C(206, 160, 108), C(180, 132, 88), C(146, 102, 70), C(102, 70, 54), false};
    case Biome::Snow: case Biome::Taiga: case Biome::Mountain:
      return {C(222, 228, 238), C(150, 156, 172), C(122, 126, 144), C(94, 96, 114), C(62, 62, 80), true};
    case Biome::Swamp: return {C(150, 150, 118), C(116, 118, 92), C(94, 96, 76), C(72, 74, 60), C(46, 48, 42), false};
    case Biome::Autumn: return {C(190, 168, 136), C(150, 128, 104), C(124, 104, 86), C(98, 82, 70), C(64, 54, 50), false};
    default: return {C(184, 172, 150), C(146, 136, 120), C(120, 110, 98), C(94, 86, 78), C(62, 56, 54), false};
  }
}
bool isWall(Ground g) { return g == Ground::Rock || g == Ground::CaveWall || g == Ground::InteriorWall || g == Ground::Void; }

// (M1) A bridge the road generator laid on a diagonal comes out as a 4-connected staircase of Bridge tiles (two per
// row). Tile by tile it drew as square plank blocks touching at their corners. Here such a staircase is read as one
// straight deck: the band's centre line is the mean diagonal of the bridge cells nearby, the deck is DECK_HW px either
// side of it (it reaches into the water tiles beside the staircase), with planks laid across it, rails and posts
// along both edges, and its shadow cast down-right onto the water. Returns false where this is not a diagonal bridge.
template <class TM>
bool diagBridgePixel(const TM& m, int px, int py, Ground real, uint32_t& out) {
  const int tx = px >> 4, ty = py >> 4;
  auto isB = [&](int x, int y) { return m.at(x, y) == Ground::Bridge; };
  if (real != Ground::Bridge) {
    bool near = false;
    for (int oy = -1; oy <= 1 && !near; oy++)
      for (int ox = -1; ox <= 1; ox++) if (isB(tx + ox, ty + oy)) { near = true; break; }
    if (!near) return false;
  }
  // the bridge cells around: their spread says whether this stretch runs on a diagonal
  int n = 0;
  float mx = 0, my = 0;
  int cxs[49], cys[49];
  for (int oy = -3; oy <= 3; oy++)
    for (int ox = -3; ox <= 3; ox++)
      if (isB(tx + ox, ty + oy)) { cxs[n] = tx + ox; cys[n] = ty + oy; mx += (float)ox; my += (float)oy; n++; }
  if (n < 4) return false;
  mx /= (float)n; my /= (float)n;
  float sxx = 0, syy = 0, sxy = 0;
  for (int k = 0; k < n; k++) {
    const float dx = (float)(cxs[k] - tx) - mx, dy = (float)(cys[k] - ty) - my;
    sxx += dx * dx; syy += dy * dy; sxy += dx * dy;
  }
  if (std::fabs(sxy) < 0.55f * std::sqrt(sxx * syy) || std::min(sxx, syy) < 0.35f * std::max(sxx, syy)) return false;
  const bool down = sxy > 0;   // true: the deck runs north-west to south-east
  // k: the coordinate across the deck, a: along it (tile units); a cell's centre and its along-span
  auto across = [&](float x, float y) { return down ? (x - y) / 16.0f : (x + y) / 16.0f; };
  auto along = [&](float x, float y) { return down ? (x + y) / 16.0f : (x - y) / 16.0f; };
  float kc = 0, amin = 1e9f, amax = -1e9f;
  for (int k = 0; k < n; k++) {
    const float cx = cxs[k] * 16.0f + 8.0f, cy = cys[k] * 16.0f + 8.0f;
    kc += across(cx, cy);
    amin = std::min(amin, along(cx, cy) - 1.0f);
    amax = std::max(amax, along(cx, cy) + 1.0f);
  }
  kc /= (float)n;
  const float S = 16.0f * 0.70710678f;   // tile units on a diagonal -> px
  constexpr float DECK_HW = 13.0f;   // (most of the walkable staircase lies under the deck)
  const float d = (across(px + 0.5f, py + 0.5f) - kc) * S, a = along(px + 0.5f, py + 0.5f);
  const bool inSpan = real == Ground::Bridge || (a > amin && a < amax);
  if (inSpan && std::fabs(d) <= DECK_HW) {
    const float ad = std::fabs(d);
    const int ap = down ? px + py : px - py;   // along the deck in px steps (planks lie across it)
    if (ad > DECK_HW - 2.5f) {
      // the rails: a dark outer edge, a lit top; posts every 14 px
      const bool post = (((ap % 14) + 14) % 14) < 3;
      out = ad > DECK_HW - 1.0f ? C(54, 36, 24) : post ? C(150, 110, 70) : C(118, 82, 50);
      if (d > 0 && ad > DECK_HW - 1.0f) out = C(44, 30, 22);
      return true;
    }
    const bool gap = ((ap % 5) + 5) % 5 == 0;
    uint32_t c = gap ? C(70, 46, 30) : lerpc(C(140, 96, 56), C(160, 112, 66), hashf(ap / 5, 0, 261));
    if (!gap && hashf(ap / 5, (int)std::floor(d / 6.0f), 263) < 0.12f) c = mul(c, 0.9f);   // a worn plank end
    if (ad > DECK_HW - 4.0f) c = mul(c, d > 0 ? 1.06f : 0.9f);
    out = c;
    return true;
  }
  if (real == Ground::Road) return false;   // (the deck runs on over the road's head; the rest is road)
  // its shadow on the water, cast down-right
  const float ds = (across(px - 3 + 0.5f, py - 4 + 0.5f) - kc) * S, as = along(px - 3 + 0.5f, py - 4 + 0.5f);
  if (std::fabs(ds) <= DECK_HW && as > amin && as < amax && (groundWater(real) || real == Ground::Bridge)) {
    out = C(18, 40, 76, 200);
    return true;
  }
  if (real == Ground::Bridge) { out = C(48, 112, 168, 130); return true; }   // open water beside the deck
  return false;
}

// M2 ecotones (VISION_PLAN 11.6). The generator writes a blend byte per tile (Map::blend: bits 0-3 the neighbouring
// biome, bits 4-7 its weight 0..8 of 16). Here every pixel reads the four tile centres around it, blends their biome
// mixes bilinearly (so the ramp has no tile steps and no straight seams) and picks one biome's ground with a
// threshold made of two octaves of value noise and a little 4x4 Bayer order: the two grounds interlock in organic
// patches whose edges are dithered, instead of a 50 % checkerboard. Pure functions of global pixels and the snapshot
// (blend and biomes lie within the bake margin), so chunks meet seamlessly. A map without blend bytes (the generator
// not writing them yet) gets the same ramps derived from the biome grid: the distance to the other biome, 4 tiles out.
inline Ground ecoGroundOf(Biome b) {
  switch (b) {
    case Biome::Plains: return Ground::Grass;
    case Biome::Forest: return Ground::ForestFloor;
    case Biome::Autumn: return Ground::Autumn;
    case Biome::Taiga: return Ground::Tundra;
    case Biome::Snow: return Ground::Snow;
    case Biome::Desert: case Biome::Beach: return Ground::Sand;
    case Biome::Swamp: return Ground::Swamp;
    default: return Ground::Void;   // ocean, mountain: no ecotone
  }
}
template <class TM>
uint8_t derivedBlend(const TM& m, int tx, int ty) {
  // a small direct-mapped cache (bakes run on worker threads; a pixel row reads the same few tiles over and over)
  struct E { const void* map; int ox, oy, tx, ty; uint8_t v; };
  thread_local E cache[64] = {};
  E& e = cache[((unsigned)tx * 7u + (unsigned)ty * 13u) & 63u];
  if (e.map == (const void*)m.m && e.ox == m.ox && e.oy == m.oy && e.tx == tx && e.ty == ty) return e.v;
  const Biome own = m.biomeAt(tx, ty);
  uint8_t v = 0;
  if (ecoGroundOf(own) != Ground::Void) {
    for (int r = 1; r <= 4 && !v; r++)
      for (int oy = -r; oy <= r && !v; oy++)
        for (int ox = -r; ox <= r; ox++) {
          if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
          const Biome b = m.biomeAt(tx + ox, ty + oy);
          if (b == own || ecoGroundOf(b) == Ground::Void || !natural(m.at(tx + ox, ty + oy))) continue;
          const float dist = std::sqrt((float)(ox * ox + oy * oy));
          const int w = std::clamp((int)std::lround(8.0f * (4.6f - dist) / 4.1f), 0, 8);
          if (w > 0) v = (uint8_t)((w << 4) | (int)b);
          break;
        }
  }
  e = E{(const void*)m.m, m.ox, m.oy, tx, ty, v};
  return v;
}
// the ground a pixel of an ecotone shows (Ground::Void: no ecotone here)
template <class TM>
Ground ecotonePixel(const TM& m, int px, int py, Ground g) {
  const float fx = (px - 7.5f) / 16.0f, fy = (py - 7.5f) / 16.0f;
  const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
  const float ax = fx - ix, ay = fy - iy;
  Biome bs[8];
  float ws[8];
  int n = 0;
  bool any = false;
  auto add = [&](Biome b, float w) {
    if (w <= 0) return;
    for (int k = 0; k < n; k++) if (bs[k] == b) { ws[k] += w; return; }
    if (n < 8) { bs[n] = b; ws[n] = w; n++; }
  };
  for (int c = 0; c < 4; c++) {
    const int x = ix + (c & 1), y = iy + (c >> 1);
    const float cw = ((c & 1) ? ax : 1 - ax) * ((c >> 1) ? ay : 1 - ay);
    const Biome own = m.biomeAt(x, y);
    const uint8_t bl = m.ecoDerive ? derivedBlend(m, x, y) : m.blendAt(x, y);
    const int w = std::min(8, bl >> 4);
    const Biome other = (Biome)(bl & 15);
    if (w > 0 && ecoGroundOf(other) != Ground::Void && (int)other < (int)Biome::COUNT) {
      any = true;
      add(own, cw * (1 - w / 16.0f));
      add(other, cw * (w / 16.0f));
    } else add(own, cw);
  }
  if (!any) return Ground::Void;
  // the threshold: broad patches, finer fringes, ordered dither only right at their edges
  float t = 0.5f + (vnoise(px / 9.0f, py / 9.0f, 861) - 0.5f) * 1.25f + (vnoise(px / 3.5f, py / 3.5f, 863) - 0.5f) * 0.45f + (bayer(px, py) - 0.5f) * 0.22f;
  t = std::clamp(t, 0.0f, 0.999f);
  // a stable order (by biome) so neighbouring pixels agree on which biome a threshold falls in
  for (int a = 1; a < n; a++)
    for (int b = a; b > 0 && (int)bs[b] < (int)bs[b - 1]; b--) { std::swap(bs[b], bs[b - 1]); std::swap(ws[b], ws[b - 1]); }
  float sum = 0;
  for (int k = 0; k < n; k++) sum += ws[k];
  float acc = 0;
  Biome pick = bs[n - 1];
  for (int k = 0; k < n; k++) {
    acc += ws[k] / sum;
    if (t < acc) { pick = bs[k]; break; }
  }
  const Ground pg = ecoGroundOf(pick);
  if (pg == Ground::Void) return Ground::Void;
  // the pixel's own tile keeps its own variant of its biome's ground (a meadow stays a meadow)
  if (pick == m.biomeAt(px >> 4, py >> 4) && ecoGround(g)) return g;
  return pg;
}

// (M2 fixer) A rock outcrop down on the low land (a cave's crag, a knoll): a heap of broken blocks under the top-left
// light, not the massif's broad ridged smear. The body is a dome from the blurred rock mask (deep inside 1, about 0.5 at
// the outline), so the whole heap is lit on its north-west flank and falls into shade to the south-east; on it sit
// jittered blocks (Voronoi cells about 11 px across), each tilted its own way, bevelled (a lit lip where a crack lies
// to its upper left, a dark one where it lies to the lower right) and parted by dark cracks. Hard quantised tones with
// a whisper of dither, moss in the cracks of the temperate lands, snow on the lit tops in the cold.
template <class TM>
uint32_t outcropPixel(const TM& m, int px, int py, int sx, int sy, const RockPal& P, float cold, float hot) {
  // the blurred rock mask at the 4 x 4 tile centres round the sample, then a bilinear dome between them
  const int ix0 = (int)std::floor((sx - 8) / 16.0f) - 1, iy0 = (int)std::floor((sy - 8) / 16.0f) - 1;
  float Bm[4][4];
  for (int j = 0; j < 4; j++)
    for (int i = 0; i < 4; i++) {
      float s = 0;
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
          if (m.at(ix0 + i + dx, iy0 + j + dy) == Ground::Rock) s += (dx == 0 && dy == 0) ? 4.0f : (dx == 0 || dy == 0) ? 2.0f : 1.0f;
      Bm[j][i] = s / 16.0f;
    }
  auto dome = [&](float x, float y) {
    const float fx = std::clamp((x - 8.0f) / 16.0f - ix0, 0.0f, 2.999f), fy = std::clamp((y - 8.0f) / 16.0f - iy0, 0.0f, 2.999f);
    const int i = (int)fx, j = (int)fy;
    const float ax = fx - i, ay = fy - j;
    return (Bm[j][i] * (1 - ax) + Bm[j][i + 1] * ax) * (1 - ay) + (Bm[j + 1][i] * (1 - ax) + Bm[j + 1][i + 1] * ax) * ay;
  };
  const float D = dome((float)sx, (float)sy);
  // the heap's own light: rising toward the south-east = facing the top-left light
  float lit = 0.50f + (dome(sx + 4.0f, sy + 4.0f) - dome(sx - 4.0f, sy - 4.0f)) * 3.4f + (D - 0.75f) * 0.35f;
  // (M2 fixer round 3, review: "the cave mound reads as paving laid on grass") the heap as one raised mass: how far
  // the rock runs toward the light (up-left) against away from it (down-right). Near its north-west edge the heap
  // rises toward the light and is lit, past its crest it falls away into shade; a soft bright crest between
  float hUL = 0, hDR = 0;
  {
    auto rockPx = [&](int qx, int qy) { return m.at((int)std::floor(qx / 16.0f), (int)std::floor(qy / 16.0f)) == Ground::Rock; };
    for (int k = 1; k <= 16; k++) { if (!rockPx(sx - k * 3, sy - k * 2)) break; hUL = (float)k; }
    for (int k = 1; k <= 16; k++) { if (!rockPx(sx + k * 3, sy + k * 2)) break; hDR = (float)k; }
    const float side = (hDR - hUL) / (hDR + hUL + 2.0f);   // +1 the lit north-west flank .. -1 the shaded south-east
    lit += side * 0.42f;
    if (std::fabs(side) < 0.12f && hUL + hDR > 8.0f) lit += 0.06f;   // the crest
  }
  // the blocks: the nearest and second nearest of jittered points on an 11-px grid (in pixel space: the blocks stay
  // put however the outline is warped)
  const int G = 18;   // (M2 fixer round 3: larger rocks, fewer cracks: 14 px cells read as flagstones)
  const int gx = (int)std::floor(px / (float)G), gy = (int)std::floor(py / (float)G);
  float d1 = 1e9f, d2 = 1e9f;
  int c1x = 0, c1y = 0;
  float p1x = 0, p1y = 0, p2x = 0, p2y = 0;
  for (int oy = -1; oy <= 1; oy++)
    for (int ox = -1; ox <= 1; ox++) {
      const int cx = gx + ox, cy = gy + oy;
      const uint32_t h = hash2(cx, cy, 931);
      const float fx = cx * (float)G + 1.5f + (h & 255) / 255.0f * (G - 3), fy = cy * (float)G + 1.5f + ((h >> 8) & 255) / 255.0f * (G - 3);
      const float d = std::sqrt((px + 0.5f - fx) * (px + 0.5f - fx) + (py + 0.5f - fy) * (py + 0.5f - fy) * 1.35f);   // (squat blocks: 3/4 view)
      if (d < d1) { d2 = d1; p2x = p1x; p2y = p1y; d1 = d; c1x = cx; c1y = cy; p1x = fx; p1y = fy; }
      else if (d < d2) { d2 = d; p2x = fx; p2y = fy; }
    }
  const uint32_t ch = hash2(c1x, c1y, 937);
  // each block's tilt (toward or away from the light) and a gentle roundness within it (its upper left catches more)
  lit += (((ch >> 4) & 255) / 255.0f - 0.5f) * 0.22f;
  lit += -((px + 0.5f - p1x) + (py + 0.5f - p1y)) / (float)G * 0.5f;   // a rounded block: lit upper left, shaded lower right
  float gap = d2 - d1;
  // some neighbouring blocks are one rock (no crack between them, only the turn of the surface)
  const bool fused = ((ch ^ hash2((int)std::floor(p2x), (int)std::floor(p2y), 951)) & 3) <= 1;
  if (fused) gap += 3.0f;
  // where the parting crack lies: upper left of this pixel (the block's lit lip) or lower right (its shaded edge)
  const float toward = (p2x - p1x) + (p2y - p1y);
  if (gap < 2.6f && gap >= 1.1f) lit += toward < 0 ? 0.26f : -0.20f;
  lit += (bayer(px, py) - 0.5f) * 0.05f;
  uint32_t c;
  if (gap < 1.1f) c = toward < 0 ? P.dark : P.lo;   // the crack (its far side catches a little light)
  else if (lit < 0.24f) c = P.dark;
  else if (lit < 0.42f) c = P.lo;
  else if (lit < 0.60f) c = P.mid;
  else if (lit < 0.80f) c = P.hi;
  else c = P.lip;
  // chips and grit
  const float sp = hashf(px, py, 939);
  if (gap >= 1.1f && sp < 0.025f) c = mul(c, 0.84f);
  // moss and grass in the cracks and on the shaded sides (temperate lands), not on the lit tops
  const float wet = 1.0f - std::min(1.0f, cold + hot);
  if (wet > 0.3f && gap < 1.8f && lit < 0.62f && vnoise(px / 9.0f, py / 9.0f, 941) > 0.70f - 0.06f * wet)
    c = lit < 0.40f ? C(58, 80, 46) : vnoise(px / 2.5f, py / 2.5f, 943) > 0.5f ? C(96, 128, 58) : C(74, 104, 52);
  // snow on the lit tops in the cold (the cold north keeps it on low crags too)
  if (cold > 0.2f) {
    const float sv = D * 0.6f + (lit - 0.5f) * 0.5f + (vnoise(px / 7.0f, py / 7.0f, 945) - 0.5f) * 0.2f;
    if (sv > 0.80f - 0.30f * std::min(1.0f, cold) && gap >= 1.1f) c = lit < 0.5f ? C(186, 198, 222) : lit < 0.7f ? C(222, 230, 242) : C(244, 247, 252);
  }
  // the outline: a lit rim where the heap meets the land to the north and west, a dark edge on the east
  {
    auto rockAt = [&](int qx, int qy) { return m.at(qx >> 4, qy >> 4) == Ground::Rock; };
    int dW = 0, dN = 0, dE = 0;
    for (int k = 1; k <= 2 && !dW; k++) if (!rockAt(sx - k, sy)) dW = k;
    for (int k = 1; k <= 2 && !dN; k++) if (!rockAt(sx, sy - k)) dN = k;
    for (int k = 1; k <= 3 && !dE; k++) if (!rockAt(sx + k, sy)) dE = k;
    if (dE == 1) c = P.dark;
    else if (dE) c = lerpc(c, P.lo, 0.5f);
    else if (dW == 1 || dN == 1) c = P.lip;
    else if (dW || dN) c = lerpc(c, P.hi, 0.5f);
  }
  // the heap's south face where it stands on the land's own level (a cliff below is reliefPixel's): 12 px of broken
  // rock, a lit lip, fissured blocks darkening to the foot and a crisp line where it meets the ground
  const int ftx = sx >> 4;
  const int lv = m.heightAt(ftx, sy >> 4);
  // (M2 fixer round 2) broken, irregular rock: the face's height wanders (9-13 px), it is made of slabs of different
  // sizes (a jittered grid squashed flat, each slab tilted toward or away from the light), parted by dark cracks, lit
  // along the top: the regular 6-px columns read as the slats of a wooden palisade
  // (M2 fixer round 3) lower toward the heap's shoulders, so its widest row reads as the slope's foot, not a ledge
  const int FHt = std::max(4, (int)std::lround(4.0f + std::min(1.0f, (hUL + hDR) / 10.0f) * 5.0f + vnoise(px / 9.0f, 0.5f, 953) * 4.0f));
  for (int k = 1; k <= FHt; k++) {
    const int qy = (sy + k) >> 4;
    if (qy == (sy >> 4)) continue;
    const Ground below = m.at(ftx, qy);
    if (below == Ground::Rock || m.heightAt(ftx, qy) < lv) break;
    const float t = (FHt - k) / (float)(FHt - 1);   // 0 at the lip .. 1 at the foot
    // the slabs: nearest of jittered points on a 9 x 5 grid in (x, depth down the face)
    const float fx = (float)px, fy = (float)(FHt - k) + (float)(sy >> 4) * 13.0f;
    const int gx0 = (int)std::floor(fx / 9.0f), gy0 = (int)std::floor(fy / 5.0f);
    float d1 = 1e9f, d2 = 1e9f;
    uint32_t sh = 0;
    for (int oy = -1; oy <= 1; oy++)
      for (int ox = -1; ox <= 1; ox++) {
        const uint32_t hh = hash2(gx0 + ox, gy0 + oy, 957);
        const float cx = (gx0 + ox + 0.15f + (hh & 255) / 255.0f * 0.7f) * 9.0f, cy = (gy0 + oy + 0.2f + ((hh >> 8) & 255) / 255.0f * 0.6f) * 5.0f;
        const float d = std::sqrt((fx + 0.5f - cx) * (fx + 0.5f - cx) + (fy + 0.5f - cy) * (fy + 0.5f - cy) * 2.2f);
        if (d < d1) { d2 = d1; d1 = d; sh = hh; } else if (d < d2) d2 = d;
      }
    const float l = 0.78f - t * 0.62f + (((sh >> 16) & 255) / 255.0f - 0.5f) * 0.30f + (bayer(px, k) - 0.5f) * 0.06f;
    uint32_t f = l > 0.70f ? P.hi : l > 0.46f ? P.mid : l > 0.24f ? P.lo : P.dark;
    if (d2 - d1 < 1.0f) f = t < 0.3f ? P.lo : P.dark;               // the cracks between slabs
    else if (d2 - d1 < 1.9f && ((sh >> 24) & 1)) f = mul(f, 1.08f);  // a slab's lit edge
    if (k == FHt) f = P.lip;                                         // the lip
    else if (k == FHt - 1 && f != P.dark) f = lerpc(f, P.lip, 0.5f);
    if (k == 1) f = mul(P.dark, 0.85f);                              // the foot line
    c = f;
    break;
  }
  return c;
}

// (M1) A mountain massif seen from above: the endless generator fills a range with solid Rock at the top relief
// levels, so its inside is one flat tile class. Paint it as a range: ridged noise gives crests and gullies, lit on
// their north-west slopes and shaded on the south-east ones (the 3/4 top-left light), quantised to a few rock tones
// with an ordered dither, strata along the contours, scree speckle, and snow on the crests and lit high slopes (more
// in the cold north, little in hot lands). The tile's climate comes from the land around the range.
template <class TM>
uint32_t mountainPixel(const TM& m, int px, int py, int sx, int sy) {
  // the climate of the land around (cold: snow / taiga, hot: desert), blended between tile centres so it never steps
  auto climate = [&](int x, int y, float& cd, float& ht) {
    static const int odx[4] = {6, -6, 0, 0}, ody[4] = {0, 0, 6, -6};
    for (int k = 0; k < 4; k++) {
      const Biome b = m.biomeAt(x + odx[k], y + ody[k]);
      if (b == Biome::Snow || b == Biome::Taiga) cd += 0.25f;
      else if (b == Biome::Desert) ht += 0.25f;
    }
  };
  float cold = 0, hot = 0;
  {
    const float fx = (px - 7.5f) / 16.0f, fy = (py - 7.5f) / 16.0f;
    const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    const float ax = fx - ix, ay = fy - iy;
    float c00 = 0, h00 = 0, c10 = 0, h10 = 0, c01 = 0, h01 = 0, c11 = 0, h11 = 0;
    climate(ix, iy, c00, h00); climate(ix + 1, iy, c10, h10); climate(ix, iy + 1, c01, h01); climate(ix + 1, iy + 1, c11, h11);
    cold = (c00 * (1 - ax) + c10 * ax) * (1 - ay) + (c01 * (1 - ax) + c11 * ax) * ay;
    hot = (h00 * (1 - ax) + h10 * ax) * (1 - ay) + (h01 * (1 - ax) + h11 * ax) * ay;
  }
  auto ridge = [](float v) { v = 1.0f - std::fabs(v * 2.0f - 1.0f); return v * v; };
  auto H2 = [&](float x, float y) {   // the broad shape: crests and gullies (two octaves, warped and turned off the grid)
    const float wx = x + (vnoise(x / 61.0f, y / 61.0f, 913) - 0.5f) * 56.0f, wy = y + (vnoise(x / 61.0f, y / 61.0f, 915) - 0.5f) * 56.0f;
    const float u = wx * 0.8f - wy * 0.6f, v = wx * 0.6f + wy * 0.8f;
    return ridge(vnoise(wx / 92.0f, wy / 92.0f, 901)) * 0.7f + ridge(vnoise(u / 37.0f, v / 37.0f, 903)) * 0.3f;
  };
  const float h0 = H2((float)px, (float)py) * 0.9f + vnoise(px / 8.0f, py / 8.0f, 905) * 0.1f;
  // light from the top-left: a slope that rises away from the light faces it
  const float slope = H2(px + 2.0f, py + 2.0f) - H2(px - 2.0f, py - 2.0f);
  float lit = 0.5f + slope * 11.0f + (h0 - 0.5f) * 0.3f;
  lit += (bayer(px, py) - 0.5f) * 0.16f;
  const RockPal P0 = rockPal(Biome::Plains), PC = rockPal(Biome::Mountain), PH = rockPal(Biome::Desert);
  auto mixPal = [&](uint32_t a, uint32_t cc, uint32_t hh) { return lerpc(lerpc(a, cc, std::min(1.0f, cold)), hh, std::min(1.0f, hot)); };
  const RockPal P = {mixPal(P0.lip, PC.lip, PH.lip), mixPal(P0.hi, PC.hi, PH.hi), mixPal(P0.mid, PC.mid, PH.mid), mixPal(P0.lo, PC.lo, PH.lo),
                     mixPal(P0.dark, PC.dark, PH.dark), false};
  // a crag stamped on the land (a cave's, a lair's: its tiles keep the land's biome, a range's rock is Mountain): an
  // outcrop of broken blocks (the massif's broad ridges smeared over a small heap)
  if (m.biomeAt(sx >> 4, sy >> 4) != Biome::Mountain) return outcropPixel(m, px, py, sx, sy, P, cold, hot);
  uint32_t c;
  if (lit < 0.22f) c = P.dark;
  else if (lit < 0.40f) c = P.lo;
  else if (lit < 0.60f) c = P.mid;
  else if (lit < 0.80f) c = P.hi;
  else c = lerpc(P.hi, P.lip, 0.55f);
  // strata: thin darker bands along the contours, broken up
  const float band = h0 * 11.0f - std::floor(h0 * 11.0f);
  if (band < 0.07f && vnoise(px / 5.0f, py / 5.0f, 907) > 0.35f) c = mul(c, 0.82f);
  // scree and chips
  const float sp = hashf(px, py, 909);
  if (sp < 0.035f) c = mul(c, 0.78f);
  else if (sp > 0.975f) c = mul(c, 1.14f);
  // snow on the crests and the lit high slopes
  float snowAt = 0.66f - 0.40f * cold + 1.0f * hot;
  const float sv = h0 + (lit - 0.5f) * 0.22f + (vnoise(px / 9.0f, py / 9.0f, 911) - 0.5f) * 0.16f + (bayer(px + 1, py + 2) - 0.5f) * 0.06f;
  // a crag or knoll down on the low land (a cave's rock) holds no snow outside the cold north
  if (m.heightAt(sx >> 4, sy >> 4) < 5) snowAt += 0.5f * (1.0f - std::min(1.0f, cold));
  if (sv > snowAt) {
    c = lit < 0.40f ? C(178, 190, 216) : lit < 0.62f ? C(222, 230, 242) : C(244, 247, 252);
    if (sv < snowAt + 0.025f) c = mul(c, 0.92f);   // the snow's thin edge over the rock
  }
  // (M1 round 3) the outcrop's own volume at its edges (a crag on open land read as a flat pasted shape): a lit rim
  // where it meets the land to the north and west, a shaded flank falling to a dark outline on the east, so it rises
  // out of the grass under the top-left light
  {
    auto rockAt = [&](int qx, int qy) { return m.at(qx >> 4, qy >> 4) == Ground::Rock; };
    int dW = 0, dN = 0, dE = 0;
    for (int k = 1; k <= 3 && !dW; k++) if (!rockAt(sx - k, sy)) dW = k;
    for (int k = 1; k <= 3 && !dN; k++) if (!rockAt(sx, sy - k)) dN = k;
    for (int k = 1; k <= 6 && !dE; k++) if (!rockAt(sx + k, sy)) dE = k;
    if (dE == 1) c = P.dark;
    else if (dE) c = lerpc(c, P.lo, 0.75f - dE * 0.08f);
    else if ((dW && dW <= 1) || (dN && dN <= 1)) c = P.lip;
    else if (dW || dN) c = lerpc(c, P.hi, 0.6f);
  }
  // a rock mass standing on the same level as the land south of it (a crag, a cave's knoll) shows a short south face
  // there, lit lip to dark foot (where the land drops a level, reliefPixel draws the real cliff below instead)
  const int ftx = sx >> 4;
  const int lv = m.heightAt(ftx, sy >> 4);
  for (int k = 1; k <= 7; k++) {
    const int qy = (sy + k) >> 4;
    if (qy == (sy >> 4)) continue;
    const Ground below = m.at(ftx, qy);
    if (below == Ground::Rock || m.heightAt(ftx, qy) < lv) break;
    // k px above the rock's south edge: a face 7 px tall
    const float t = (7 - k) / 6.0f;   // 0 at the lip .. 1 at the foot
    uint32_t f = lerpc(P.hi, P.dark, t);
    if (((px + (int)(hash2(px / 3, qy, 917) & 1)) % 3) == 0) f = mul(f, 0.86f);   // fissures
    if (k == 7) f = P.lip;
    c = f;
    break;
  }
  return c;
}
}  // namespace

// Terrace height of a rock tile (0 = not rock). Limited by the distance to open ground, so edges always step down one level at a time.
int View::rockLevel(const TMap& m, int tx, int ty) {
  Ground g = m.at(tx, ty);
  if (g != Ground::Rock && g != Ground::CaveWall && !(g == Ground::Void && m.kind != MapKind::Overworld)) return 0;
  if (m.kind != MapKind::Overworld) return 1;
  if (m.relief()) return 1;   // (M1) the generator's levels carry the relief: reliefPixel draws the faces
  int want = 1 + (int)(vnoise(tx / 13.0f, ty / 13.0f, 501) * 3.3f);
  if (want <= 1) return 1;
  for (int r = 1; r < want; r++)
    for (int oy = -r; oy <= r; oy++)
      for (int ox = -r; ox <= r; ox++) {
        if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
        Ground n = m.at(tx + ox, ty + oy);
        if (n != Ground::Rock && n != Ground::Void) return r;
      }
  return want;
}

uint32_t View::groundPixel(const TMap& m, int px, int py) {
  int tx = px >> 4, ty = py >> 4;
  Ground real = m.at(tx, ty);
  if (real == Ground::Void && m.kind == MapKind::Overworld) real = Ground::DeepWater;
  if (m.kind == MapKind::Overworld && (real == Ground::Bridge || groundWater(real) || real == Ground::Road)) {
    uint32_t o;
    if (diagBridgePixel(m, px, py, real, o)) return o;
  }
  // organic borders: look the terrain up at a warped position
  Ground g = real;
  int sx = px, sy = py;   // sample position (warped for natural terrain)
  if (natural(real)) {
    float amp = (real == Ground::Rock || real == Ground::CaveWall) ? 5.0f : 7.0f;
    float wx = px + (vnoise(px / 7.0f, py / 7.0f, 11) - 0.5f) * amp + (hashf(px, py, 3) - 0.5f) * 1.5f;
    float wy = py + (vnoise(px / 7.0f, py / 7.0f, 23) - 0.5f) * amp + (hashf(px, py, 5) - 0.5f) * 1.5f;
    Ground w = m.at((int)std::floor(wx / 16), (int)std::floor(wy / 16));
    if (m.kind == MapKind::Overworld) {
      // a rock outcrop's outline out on the land: the smooth rockWarp alone decides it (the per-pixel jitter made rock
      // edges a fuzzy smudge, and the small swing alone left tile-stepped cuts)
      float bx, by;
      rockWarp(px, py, bx, by);
      const Ground wb = m.at((int)std::floor(bx / 16), (int)std::floor(by / 16));
      if ((wb == Ground::Rock) != (real == Ground::Rock) || (w == Ground::Rock) != (real == Ground::Rock)) { wx = bx; wy = by; w = wb; }
    } else if ((w == Ground::Rock) != (real == Ground::Rock)) {
      wx = px + (vnoise(px / 7.0f, py / 7.0f, 11) - 0.5f) * amp;
      wy = py + (vnoise(px / 7.0f, py / 7.0f, 23) - 0.5f) * amp;
      w = m.at((int)std::floor(wx / 16), (int)std::floor(wy / 16));
    }
    if (w == Ground::Void && m.kind == MapKind::Overworld) w = Ground::DeepWater;
    if (natural(w) && !(isWall(w) != isWall(real) && m.kind != MapKind::Overworld)) { g = w; sx = (int)std::floor(wx); sy = (int)std::floor(wy); }
    // (M2 fixer round 2) a patch of earth in paving (a ruin's floor, a square's worn spot) meets the stones along the
    // same wobbling line, not a tile's square edge
    else if (m.kind == MapKind::Overworld && real == Ground::Dirt && (w == Ground::Plaza || w == Ground::StoneFloor)) g = w;
    // (M1, VISION_PLAN 11.6) ecotones: soft land fades into the neighbouring soft land over a dithered band a few tiles
    // wide (a broad low-frequency warp plus an ordered dither) instead of meeting it along one wobbly line
    const Ground eco = m.kind == MapKind::Overworld && ecoGround(g) ? ecotonePixel(m, px, py, g) : Ground::Void;
    if (eco != Ground::Void) {
      if (eco != g) { g = eco; sx = px; sy = py; }
    } else if (m.kind == MapKind::Overworld && ecoGround(g)) {
      const float ex = px + (vnoise(px / 52.0f, py / 52.0f, 811) - 0.5f) * 64.0f + (bayer(px, py) - 0.5f) * 18.0f + (vnoise(px / 5.0f, py / 5.0f, 813) - 0.5f) * 10.0f;
      const float ey = py + (vnoise(px / 52.0f, py / 52.0f, 817) - 0.5f) * 64.0f + (bayer(px + 2, py + 1) - 0.5f) * 18.0f + (vnoise(px / 5.0f, py / 5.0f, 819) - 0.5f) * 10.0f;
      const Ground e = m.at((int)std::floor(ex / 16), (int)std::floor(ey / 16));
      if (e != g && ecoGround(e)) { g = e; sx = px; sy = py; }
    }
  } else if (m.kind == MapKind::Overworld && (real == Ground::Plaza || real == Ground::StoneFloor)) {
    // (M2 fixer round 2) paving gives way to the earth, the road or the other paving beside it along a wobbling edge
    // (two octaves), so its patches and its ends are not stepped tile rectangles
    const float wx = px + (vnoise(px / 7.0f, py / 7.0f, 11) - 0.5f) * 7.0f + (vnoise(px / 3.0f, py / 3.0f, 41) - 0.5f) * 2.0f;
    const float wy = py + (vnoise(px / 7.0f, py / 7.0f, 23) - 0.5f) * 7.0f + (vnoise(px / 3.0f, py / 3.0f, 43) - 0.5f) * 2.0f;
    const Ground w = m.at((int)std::floor(wx / 16), (int)std::floor(wy / 16));
    if (w != real && (w == Ground::Dirt || w == Ground::Road || w == Ground::Plaza || w == Ground::StoneFloor)) g = w;
  } else if (real == Ground::Road || real == Ground::Farmland) {
    // roads fray a little at their edges into the surrounding soft ground
    float wx = px + (vnoise(px / 5.0f, py / 5.0f, 31) - 0.5f) * 4.0f, wy = py + (vnoise(px / 5.0f, py / 5.0f, 37) - 0.5f) * 4.0f;
    Ground w = m.at((int)std::floor(wx / 16), (int)std::floor(wy / 16));
    if (soft(w) && !groundWater(w)) g = w;
    // (M2 fixer round 2) a street meets a square's flags along the same wobbling line the flags use (paving.. above)
    else if (m.kind == MapKind::Overworld && real == Ground::Road) {
      const float vx = px + (vnoise(px / 7.0f, py / 7.0f, 11) - 0.5f) * 7.0f + (vnoise(px / 3.0f, py / 3.0f, 41) - 0.5f) * 2.0f;
      const float vy = py + (vnoise(px / 7.0f, py / 7.0f, 23) - 0.5f) * 7.0f + (vnoise(px / 3.0f, py / 3.0f, 43) - 0.5f) * 2.0f;
      const Ground v = m.at((int)std::floor(vx / 16), (int)std::floor(vy / 16));
      if (v == Ground::Plaza || v == Ground::StoneFloor) g = v;
    }
  }
  // ruins and crypts (M0 round 3): the built rooms and corridors are walled in dressed stone on every side. A wall
  // tile touching the floor gets a masonry face where the floor lies south of it (lit coping lip, block courses
  // darkening to the foot), a coping band along the wall top on every edge that meets the floor (west, east, north
  // and the corners), and the floor takes the west wall's cast shadow (light from the top-left). Before, the brick
  // floor ended in a straight cut against cave rock with only a brown earthen strip along some north edges.
  if (m.kind == MapKind::Ruin && (real == Ground::CaveWall || real == Ground::StoneFloor)) {
    auto fl = [&](int x, int y) { Ground q = m.at(x, y); return !isWall(q); };
    const int lx = px & 15, ly = py & 15;
    if (real == Ground::StoneFloor) {
      int row = py / 8, shift = (row & 1) * 4, col = (px + shift) / 8;
      bool line = ((px + shift) % 8 == 0) || (py % 8 == 0);
      uint32_t c = line ? C(58, 56, 62) : lerpc(C(96, 94, 104), C(116, 112, 122), hashf(col, row, 231));
      if (!line && hashf(px, py, 13) < 0.03f) c = C(80, 78, 86);
      // the west wall's cast shadow (a cool band), and the corner where it meets the north wall
      int sw = !fl(tx - 1, ty) ? 4 - lx : (!fl(tx - 1, ty - 1) && !fl(tx, ty - 1) ? 0 : -1);
      if (sw > 0) c = lerpc(mul(c, 0.66f + (4 - sw) * 0.06f), C(40, 30, 70), 0.18f);
      return c;
    }
    const bool fS = fl(tx, ty + 1), fN = fl(tx, ty - 1), fW = fl(tx - 1, ty), fE = fl(tx + 1, ty);
    const bool fNW = fl(tx - 1, ty - 1), fNE = fl(tx + 1, ty - 1), fSW = fl(tx - 1, ty + 1), fSE = fl(tx + 1, ty + 1);
    if (fS || fN || fW || fE || fNW || fNE || fSW || fSE) {
      const int FH = 12, RB = 6;   // face height, coping band width
      // the masonry face of a wall with the floor south of it
      if (fS && ly >= 16 - FH) {
        const int k = ly - (16 - FH);          // 0 at the lip .. FH-1 at the foot
        const int crs = (k + 1) / 4, kk = (k + 1) % 4;
        const int off = (crs & 1) * 5;
        const bool joint = kk == 0 || ((px + off) % 10 == 0);
        float t = k / (float)(FH - 1);
        uint32_t c = lerpc(C(132, 126, 136), C(72, 68, 80), t);
        c = mul(c, 0.94f + hashf((px + off) / 10, crs + ty * 7, 611) * 0.12f);
        if (joint) c = mul(c, 0.62f);
        else if (kk == 1) c = mul(c, 1.08f);                                    // each block's lit upper edge
        if (k == 0) c = C(178, 170, 176);                                       // the coping's lit lip
        if (k >= FH - 2) c = mul(c, 0.72f);                                     // contact shade at the foot
        // the face ends cleanly where the wall turns: a lit edge at its west end, a shaded one at its east end
        if (lx == 0 && !fl(tx - 1, ty) && fl(tx - 1, ty + 1) == false) c = mul(c, 1.18f);
        if (lx == 0 && fW) c = mul(c, 1.25f);
        if (lx == 15 && fE) c = mul(c, 0.7f);
        if (hashf(px, py, 613) < 0.02f) c = mul(c, 0.85f);                      // chips
        return c;
      }
      // the coping on the wall top along every edge that meets the floor
      int dEdge = 99;   // px from the nearest floor-side edge of this tile's top
      bool lit = false;
      if (fS) { int d = (16 - FH) - 1 - ly; if (d < dEdge) { dEdge = d; lit = false; } }
      if (fN) { if (ly < dEdge) { dEdge = ly; lit = true; } }
      if (fW) { if (lx < dEdge) { dEdge = lx; lit = true; } }
      if (fE) { if (15 - lx < dEdge) { dEdge = 15 - lx; lit = false; } }
      if (fNW && !fN && !fW) { int d = std::max(lx, ly); if (d < dEdge) { dEdge = d; lit = true; } }
      if (fNE && !fN && !fE) { int d = std::max(15 - lx, ly); if (d < dEdge) { dEdge = d; lit = false; } }
      // (M2 fixer round 3) a room's top corners: the side wall's coping runs the corner tile's full height, up to the
      // north wall's top band (which a corner cut short left a notch of bare rock beside the north wall's face)
      if (fSW && !fS && !fW) { int d = lx; if (ly <= 15 - FH) d = std::min(d, 15 - FH - ly); if (d < dEdge) { dEdge = d; lit = true; } }
      if (fSE && !fS && !fE) { int d = 15 - lx; if (ly <= 15 - FH) d = std::min(d, 15 - FH - ly); if (d < dEdge) { dEdge = d; lit = false; } }
      if (dEdge < RB) {
        // dressed coping stones 8 px long, a joint between them, lit on the floor-facing rim (west and north)
        bool alongX = (fN || fS) && !(fW || fE);
        int a = alongX ? px : py;
        uint32_t c = lerpc(C(120, 114, 126), C(136, 130, 140), hashf(a / 8, (alongX ? py : px) / 16, 617));
        if (a % 8 == 0) c = mul(c, 0.7f);
        if (dEdge == 0) c = lit ? C(164, 158, 166) : C(66, 62, 72);   // the rim over the floor
        else if (dEdge == 1) c = mul(c, lit ? 1.08f : 0.9f);
        else if (dEdge >= RB - 1) c = C(40, 36, 44);                  // the back edge, where the rock mass begins
        return c;
      }
      // the rest of a wall tile beside a room: the dark top of the wall mass (no face, no cave rim)
      uint32_t c = lerpc(C(50, 43, 45), C(60, 52, 52), toneField(px, py, 107));   // as the cave rock beyond
      c = mul(c, 0.9f + vnoise(px / 9.0f, py / 9.0f, 371) * 0.22f);
      int tf = tuft(px, py, 377, 0.35f);
      if (tf == 1) c = mul(c, 0.75f); else if (tf == 2) c = mul(c, 1.2f);
      return c;
    }
  }
  // caves: the rock is a raised mass with a rounded, wandering outline (a smoothed wall field, like the shorelines),
  // a dark rubbly top lit along its north-west rim, and a tall south face lit at the lip and falling into shadow at
  // its foot, where the floor gets a contact shade. Movement stays on the tile grid; only the look is organic.
  if (m.kind != MapKind::Overworld && (real == Ground::CaveWall || real == Ground::CaveFloor)) {
    auto wallT = [&](int x, int y) -> float {
      Ground q = m.at(x, y);
      return (q == Ground::CaveWall || q == Ground::Rock || q == Ground::Void) ? 1.0f : 0.0f;
    };
    auto F = [&](int x, int y) -> float {
      float fx = (x - 7.5f) / 16.0f, fy = (y - 7.5f) / 16.0f;
      int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
      float ax = fx - ix, ay = fy - iy;
      float a = wallT(ix, iy), b = wallT(ix + 1, iy), c0 = wallT(ix, iy + 1), d = wallT(ix + 1, iy + 1);
      if (a == b && a == c0 && a == d) return a;
      float v = (a * (1 - ax) + b * ax) * (1 - ay) + (c0 * (1 - ax) + d * ax) * ay;
      return v + (vnoise(x / 5.0f, y / 5.0f, 361) - 0.5f) * 0.34f + (vnoise(x / 15.0f, y / 15.0f, 363) - 0.5f) * 0.24f;
    };
    const int FH = 12;   // face height, px
    float n2 = toneField(px, py, 107);
    float h2 = hashf(px, py, 17);
    if (F(px, py) > 0.5f) {
      int k = 0;   // px above the foot of the face (0 = not a face pixel)
      for (int j = 1; j <= FH; j++) if (F(px, py + j) <= 0.5f) { k = j; break; }
      if (k) {
        float t = (k - 1) / (float)(FH - 1);   // 0 at the foot, 1 at the lip
        uint32_t c = lerpc(C(64, 52, 50), C(124, 106, 94), t);
        int col = px / 3 + (int)(hash2(px / 3, py / 16, 281) % 2);
        if ((hash2(col, py / 16, 283) & 3) == 0) c = mul(c, 0.82f);                  // vertical fissures
        if (((py + (int)(hash2(px / 5, 0, 285) % 3)) % 4) == 0) c = mul(c, 0.9f);    // strata
        if (k == FH || F(px, py - 1) <= 0.5f) c = C(150, 132, 116);                  // lit lip
        if (k <= 2) c = mul(c, 0.78f);
        if (h2 < 0.03f) c = mul(c, 1.15f);
        return c;
      }
      // the top of the rock: dark rubble, lit along the rim that faces the light (north and west)
      uint32_t c = lerpc(C(40, 34, 38), C(49, 42, 44), n2);
      c = mul(c, 0.9f + vnoise(px / 9.0f, py / 9.0f, 371) * 0.22f);
      int tf = tuft(px, py, 377, 0.35f);
      if (tf == 1) c = mul(c, 0.72f); else if (tf == 2) c = mul(c, 1.25f);
      // lumps of rock: each lit on its upper-left, shaded on its lower-right
      float lu = vnoise(px / 4.5f, py / 4.5f, 381), ld = vnoise((px + 2) / 4.5f, (py + 2) / 4.5f, 381);
      if (lu > 0.66f && ld <= 0.66f) c = mul(c, 1.3f);
      else if (lu <= 0.66f && ld > 0.66f) c = mul(c, 0.72f);
      if (F(px, py - 1) <= 0.5f || F(px - 1, py) <= 0.5f) c = C(112, 98, 90);
      else if (F(px, py - 2) <= 0.5f || F(px - 2, py) <= 0.5f) c = mul(c, 1.35f);
      return c;
    }
    // floor: the cave floor, darker in the contact shade at the foot of a face. (M1: kept clearly lighter than the
    // rock tops, so the cave's shape reads on a phone in daylight, as the 16-bit games kept their cave floors)
    uint32_t c = pick3(n2, C(108, 95, 82), C(118, 104, 90), C(128, 113, 98));
    if (h2 < 0.015f) c = C(146, 131, 114);
    else if (h2 < 0.03f) c = mul(c, 0.8f);
    if (F(px, py - 1) > 0.5f) c = mul(c, 0.6f);
    else if (F(px, py - 2) > 0.5f) c = mul(c, 0.7f);
    else if (F(px, py - 3) > 0.5f) c = mul(c, 0.8f);
    else if (F(px, py - 4) > 0.5f || F(px - 2, py - 2) > 0.5f) c = mul(c, 0.9f);
    return c;
  }
  // organic shorelines: whether a pixel is water follows a smoothed field (the tile grid's wetness, bilinear between
  // tile centres, plus two octaves of noise) instead of the tile staircase, so ponds, rivers and coasts have rounded,
  // wandering banks. shoreV is that field (0 dry .. 1 wet), -1 away from any shore.
  float shoreV = -1;
  if (soft(real) && real != Ground::CaveFloor) {
    float fx = (px - 7.5f) / 16.0f, fy = (py - 7.5f) / 16.0f;
    int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    float ax = fx - ix, ay = fy - iy;
    auto wet = [&](int x, int y) -> float {
      Ground q = m.at(x, y);
      if (q == Ground::Void) return m.kind == MapKind::Overworld ? 1.0f : 0.0f;
      return groundWater(q) || q == Ground::Bridge ? 1.0f : 0.0f;
    };
    float w00 = wet(ix, iy), w10 = wet(ix + 1, iy), w01 = wet(ix, iy + 1), w11 = wet(ix + 1, iy + 1);
    if (!(w00 == w10 && w00 == w01 && w00 == w11)) {
      float v = (w00 * (1 - ax) + w10 * ax) * (1 - ay) + (w01 * (1 - ax) + w11 * ax) * ay;
      // a stream stepping diagonally (wet cells touching only at a corner): the bilinear field pinches to 0.5 at the
      // shared corner, so the noise cut the channel into a string of beads. Keep a channel along the wet diagonal.
      if (w00 == w11 && w10 == w01 && w00 != w10) {
        const float dd = w00 > 0.5f ? std::fabs(ax - ay) : std::fabs(ax + ay - 1.0f);
        v = std::max(v, 1.0f - dd * 0.9f);
      }
      v += (vnoise(px / 6.0f, py / 6.0f, 351) - 0.5f) * 0.30f + (vnoise(px / 19.0f, py / 19.0f, 353) - 0.5f) * 0.26f;
      shoreV = v;
      bool wetPix = v > 0.5f;
      if (wetPix && !groundWater(g)) g = Ground::Water;
      else if (!wetPix && groundWater(g)) {
        // dry: take the nearest land tile's ground (never a road: built ground keeps its own edge)
        Ground land = Ground::Void;
        float bd = 1e9f;
        for (int oy = -1; oy <= 1; oy++)
          for (int ox = -1; ox <= 1; ox++) {
            Ground q = m.at(tx + ox, ty + oy);
            if (!soft(q) || groundWater(q)) continue;
            float ddx = (tx + ox) * 16 + 7.5f - px, ddy = (ty + oy) * 16 + 7.5f - py;
            float d = ddx * ddx + ddy * ddy;
            if (d < bd) { bd = d; land = q; }
          }
        if (land != Ground::Void) { g = land; sx = px; sy = py; }
      }
    }
  }
  // paved ground (roads, squares) meets soft ground along a smoothed edge too: streets widen and narrow in soft
  // curves instead of whole-tile steps. paveV is the field (-1 away from an edge).
  float paveV = -1;
  if (m.kind == MapKind::Overworld && (real == Ground::Road || real == Ground::Plaza || (soft(real) && !groundWater(real)))) {
    auto paved = [&](int x, int y) -> float { Ground q = m.at(x, y); return q == Ground::Road || q == Ground::Plaza || q == Ground::Bridge ? 1.0f : 0.0f; };
    float fx = (px - 7.5f) / 16.0f, fy = (py - 7.5f) / 16.0f;
    int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    float ax = fx - ix, ay = fy - iy;
    float p00 = paved(ix, iy), p10 = paved(ix + 1, iy), p01 = paved(ix, iy + 1), p11 = paved(ix + 1, iy + 1);
    if (!(p00 == p10 && p00 == p01 && p00 == p11)) {
      float v = (p00 * (1 - ax) + p10 * ax) * (1 - ay) + (p01 * (1 - ax) + p11 * ax) * ay;
      v += (vnoise(px / 5.0f, py / 5.0f, 391) - 0.5f) * 0.26f + (vnoise(px / 13.0f, py / 13.0f, 393) - 0.5f) * 0.18f;
      paveV = v;
      bool pv = v > 0.5f;
      bool isPaved = g == Ground::Road || g == Ground::Plaza;
      if (pv != isPaved) {
        // take the ground of the nearest tile of the other kind
        Ground want = Ground::Void;
        float bd = 1e9f;
        for (int oy = -1; oy <= 1; oy++)
          for (int ox = -1; ox <= 1; ox++) {
            Ground q = m.at(tx + ox, ty + oy);
            bool qp = q == Ground::Road || q == Ground::Plaza;
            if (pv ? !qp : (!soft(q) || groundWater(q))) continue;
            float ddx = (tx + ox) * 16 + 7.5f - px, ddy = (ty + oy) * 16 + 7.5f - py;
            float d = ddx * ddx + ddy * ddy;
            if (d < bd) { bd = d; want = q; }
          }
        if (want != Ground::Void) { g = want; sx = px; sy = py; }
      }
    }
  }
  float n = toneField(px, py, 101);
  float h = hashf(px, py, 13);
  int lx = px & 15, ly = py & 15;
  uint32_t c = 0;
  switch (g) {
    case Ground::Grass:
    case Ground::Meadow: {
      bool meadow = g == Ground::Meadow;
      uint32_t a = meadow ? C(104, 168, 72) : C(78, 144, 62), b = meadow ? C(120, 182, 80) : C(92, 160, 68), cc = meadow ? C(136, 192, 88) : C(106, 172, 76);
      c = pick3(n, a, b, cc);
      float big = vnoise(px / 40.0f, py / 40.0f, 141);
      c = mul(c, 0.94f + big * 0.12f);
      int tf = tuft(px, py, 401, meadow ? 0.55f : 0.45f);
      if (tf == 1) c = mul(c, 0.74f);
      else if (tf == 2) c = mul(c, 1.16f);
      if (meadow && h > 0.992f) { static const uint32_t fl[] = {C(250, 250, 240), C(250, 220, 90), C(240, 130, 170), C(160, 150, 250)}; c = fl[hash2(px, py, 9) & 3]; }
      break;
    }
    case Ground::ForestFloor:
      c = pick3(n, C(50, 96, 50), C(58, 108, 54), C(66, 118, 58));
      c = mul(c, 0.9f + vnoise(px / 30.0f, py / 30.0f, 151) * 0.18f);
      { int tf = tuft(px, py, 411, 0.5f); if (tf == 1) c = mul(c, 0.72f); else if (tf == 2) c = mul(c, 1.14f); }
      if (h < 0.012f) c = C(110, 84, 50);
      break;
    case Ground::Autumn:
      c = pick3(n, C(152, 98, 50), C(168, 112, 56), C(184, 128, 64));
      if (h < 0.02f) c = C(200, 70, 40);
      else if (h < 0.04f) c = C(226, 176, 76);
      { int tf = tuft(px, py, 421, 0.35f); if (tf == 1) c = mul(c, 0.75f); else if (tf == 2) c = C(214, 150, 60); }
      break;
    case Ground::Tundra:
      c = pick3(n, C(100, 120, 92), C(112, 132, 100), C(124, 142, 108));
      if (h < 0.01f) c = C(150, 150, 146);
      { int tf = tuft(px, py, 431, 0.4f); if (tf == 1) c = mul(c, 0.78f); else if (tf == 2) c = mul(c, 1.12f); }
      break;
    case Ground::Snow:
      c = pick3(n, C(214, 224, 238), C(226, 234, 244), C(238, 243, 250));
      c = mul(c, 0.95f + vnoise(px / 26.0f, py / 26.0f, 161) * 0.06f);
      if (h > 0.996f) c = C(255, 255, 255);
      break;
    case Ground::Sand: {
      float rip = std::sin(px * 0.35f + py * 0.12f + vnoise(px / 16.0f, py / 16.0f, 171) * 8);
      c = pick3(n, C(214, 190, 134), C(224, 202, 146), C(232, 212, 158));
      if (rip > 0.8f) c = mul(c, 0.93f);
      if (h < 0.008f) c = C(180, 156, 112);
      break;
    }
    case Ground::Swamp:
      c = pick3(n, C(66, 82, 52), C(74, 92, 56), C(84, 100, 60));
      if (vnoise(px / 6.0f, py / 6.0f, 181) > 0.74f) c = C(58, 74, 66);
      if (h < 0.05f) c = C(100, 120, 60);
      break;
    case Ground::Dirt:
      c = pick3(n, C(132, 100, 66), C(144, 112, 74), C(156, 124, 82));
      if (h < 0.015f) c = C(172, 152, 122);
      else if (h < 0.03f) c = mul(c, 0.8f);
      // (M1 round 3) bare earth in the frozen north is cold, dark and flecked with snow (a warm tan patch on a
      // snowfield read as a pasted decal: the dragon's eyrie, tracks over the tundra)
      if (m.kind == MapKind::Overworld) {
        const Biome bb = m.biomeAt(tx, ty);
        if (bb == Biome::Snow || bb == Biome::Mountain) {
          c = pick3(n, C(92, 82, 82), C(104, 93, 90), C(118, 106, 100));
          if (h < 0.05f) c = C(226, 232, 242);
          else if (h < 0.08f) c = mul(c, 0.78f);
        }
      }
      break;
    case Ground::Farmland: {
      bool ridge = (py % 4) < 2;
      c = ridge ? C(128, 94, 60) : C(98, 70, 46);
      if (ridge && (hash2(px / 3, py / 4, 191) & 3) == 0) c = C(96, 150, 60);
      if (ridge && h < 0.08f) c = C(148, 112, 74);
      break;
    }
    case Ground::Road: {
      // worn cobbles: irregular stones with soft mortar, packed earth showing through toward the edges
      int row = py / 5;
      int shift = (row & 1) * 3;
      int col = (px + shift) / 6;
      bool mortar = ((px + shift) % 6 == 0) || (py % 5 == 0);
      uint32_t stone = lerpc(C(164, 152, 130), C(184, 172, 148), hashf(col, row, 201));
      uint32_t earth = lerpc(C(150, 124, 90), C(162, 136, 98), n);
      c = mortar ? lerpc(stone, C(130, 116, 96), 0.55f) : stone;
      if (!mortar && ((px + shift) % 6 == 1 || py % 5 == 1)) c = mul(c, 1.05f);
      // how close to the road's edge is this pixel?
      bool edgeL = m.at(tx - 1, ty) != Ground::Road && m.at(tx - 1, ty) != Ground::Plaza && m.at(tx - 1, ty) != Ground::Bridge;
      bool edgeR = m.at(tx + 1, ty) != Ground::Road && m.at(tx + 1, ty) != Ground::Plaza && m.at(tx + 1, ty) != Ground::Bridge;
      bool edgeU = m.at(tx, ty - 1) != Ground::Road && m.at(tx, ty - 1) != Ground::Plaza && m.at(tx, ty - 1) != Ground::Bridge;
      bool edgeD = m.at(tx, ty + 1) != Ground::Road && m.at(tx, ty + 1) != Ground::Plaza && m.at(tx, ty + 1) != Ground::Bridge;
      float e = 0;
      if (edgeL) e = std::max(e, 1.0f - lx / 6.0f);
      if (edgeR) e = std::max(e, 1.0f - (15 - lx) / 6.0f);
      if (edgeU) e = std::max(e, 1.0f - ly / 6.0f);
      if (edgeD) e = std::max(e, 1.0f - (15 - ly) / 6.0f);
      if (paveV >= 0) e = std::clamp((0.66f - paveV) / 0.16f, 0.0f, 1.0f);   // the smoothed edge, not the tile's
      float wear = vnoise(px / 7.0f, py / 7.0f, 211) * 0.8f + e * 0.75f;
      if (wear > 0.72f) c = earth;
      else if (wear > 0.64f && mortar) c = earth;
      break;
    }
    case Ground::Plaza: {
      // (M2 fixer round 2) laid flags, not one regular brick grid: courses of three rows share a stone length (7-10 px),
      // each row starts its joints where it likes, every flag has its own tone and a faint bevel (lit top edge, shaded
      // foot), broad patches of warmer and greyer stone and of wear, moss in the joints here and there, a cracked flag
      const int row = py / 8, fwid = 7 + (int)(hash2(py / 24, 0, 223) % 4);
      const int shift = (int)(hash2(row, 0, 225) % (uint32_t)fwid);
      const int u = px + shift, col = u / fwid, in = u % fwid, iy = py % 8;
      const bool line = in == 0 || iy == 0;
      const float tone = hashf(col, row, 221);
      c = line ? C(112, 110, 112) : lerpc(C(158, 156, 154), C(186, 182, 176), tone);
      if (!line) {
        if (iy == 1 || in == 1) c = mul(c, 1.05f);             // the flag's lit edge (top-left light)
        else if (iy == 7 || in == fwid - 1) c = mul(c, 0.93f);  // its shaded foot
        if (h < 0.04f) c = mul(c, 0.88f);
        if (hashf(col, row, 229) < 0.035f && ((px - py + 64) % 6) == 0) c = mul(c, 0.72f);   // a cracked flag
      }
      const float broad = vnoise(px / 46.0f, py / 46.0f, 227);
      c = mul(c, 0.94f + broad * 0.12f);
      c = lerpc(c, C(178, 160, 128), std::max(0.0f, vnoise(px / 70.0f, py / 70.0f, 233) - 0.55f) * 0.5f);   // warmer stone
      if (line && vnoise(px / 11.0f, py / 11.0f, 231) > 0.70f) c = lerpc(c, C(78, 98, 58), 0.55f);       // moss in the joints
      // the square's rim: worn flags with earth in the joints, fading into the ground around it
      if (paveV >= 0 && paveV < 0.6f) c = line ? C(140, 116, 86) : lerpc(c, C(170, 150, 120), 0.35f);
      break;
    }
    case Ground::StoneFloor: {
      int row = py / 8, shift = (row & 1) * 4, col = (px + shift) / 8;
      bool line = ((px + shift) % 8 == 0) || (py % 8 == 0);
      c = line ? C(58, 56, 62) : lerpc(C(96, 94, 104), C(116, 112, 122), hashf(col, row, 231));
      if (!line && h < 0.03f) c = C(80, 78, 86);
      break;
    }
    case Ground::WoodFloor: {
      int row = py / 4;
      int seamOff = (int)(hash2(row, 0, 241) % 24);
      bool seam = (py % 4 == 0) || ((px + seamOff) % 24 == 0);
      c = seam ? C(76, 50, 32) : lerpc(C(138, 94, 58), C(158, 110, 66), hashf((px + seamOff) / 24, row, 251));
      if (!seam && h < 0.06f) c = mul(c, 0.86f);   // grain
      break;
    }
    case Ground::Bridge: {
      // one plank direction per bridge: it runs the way its deck is longest (the way you cross), the planks lie across
      // it, and the rails run along both open sides. Deciding per tile from the water beside it made L-shaped decks
      // with planks turning at the river's edge.
      auto isB = [&](int x, int y) { return m.at(x, y) == Ground::Bridge; };
      int hl = 1, vl = 1;
      for (int k = 1; k < 8 && isB(tx - k, ty); k++) hl++;
      for (int k = 1; k < 8 && isB(tx + k, ty); k++) hl++;
      for (int k = 1; k < 8 && isB(tx, ty - k); k++) vl++;
      for (int k = 1; k < 8 && isB(tx, ty + k); k++) vl++;
      // the water beside the deck says it plainest (water north or south: you cross east-west); where water lies on
      // both axes (a deck's corner over a bend), the longer run decides
      const bool wNS = groundWater(m.at(tx, ty - 1)) || groundWater(m.at(tx, ty + 1));
      const bool wEW = groundWater(m.at(tx - 1, ty)) || groundWater(m.at(tx + 1, ty));
      bool eastWest = wNS != wEW ? wNS : (hl != vl ? hl > vl : true);
      // (M1 round 3) the whole deck decides, not each tile: a road's bridge that jogs a tile sideways halfway over
      // came out as two blocks with their planks turned against each other. The deck's extent (flood over its tiles,
      // cached per tile: bakes run on worker threads) gives the way across.
      {
        struct DeckCache { const void* map = nullptr; int ox = 0, oy = 0, tx = INT32_MIN, ty = INT32_MIN; int dir = 0; };
        thread_local DeckCache dc;
        if (dc.map != (const void*)m.m || dc.ox != m.ox || dc.oy != m.oy || dc.tx != tx || dc.ty != ty) {
          dc.map = (const void*)m.m; dc.ox = m.ox; dc.oy = m.oy; dc.tx = tx; dc.ty = ty;
          int x0 = tx, x1 = tx, y0 = ty, y1 = ty;
          std::vector<std::pair<int, int>> todo{{tx, ty}}, seen{{tx, ty}};
          while (!todo.empty() && seen.size() < 64) {
            const auto q = todo.back();
            todo.pop_back();
            static const int ddx[4] = {1, -1, 0, 0}, ddy[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; k++) {
              const int nx = q.first + ddx[k], ny = q.second + ddy[k];
              if (!isB(nx, ny) || std::abs(nx - tx) > 12 || std::abs(ny - ty) > 12) continue;
              if (std::find(seen.begin(), seen.end(), std::make_pair(nx, ny)) != seen.end()) continue;
              seen.push_back({nx, ny});
              todo.push_back({nx, ny});
              x0 = std::min(x0, nx); x1 = std::max(x1, nx); y0 = std::min(y0, ny); y1 = std::max(y1, ny);
            }
          }
          dc.dir = (x1 - x0) > (y1 - y0) ? 1 : (y1 - y0) > (x1 - x0) ? 2 : 0;
        }
        if (dc.dir) eastWest = dc.dir == 1;
      }
      int a = eastWest ? px : py;   // across the planks
      bool gap = ((a % 4) + 4) % 4 == 0;
      c = gap ? C(70, 46, 30) : lerpc(C(140, 96, 56), C(160, 112, 66), hashf(a / 4, 0, 261));
      if (!gap && hashf(a / 4, (eastWest ? py : px) / 6, 263) < 0.12f) c = mul(c, 0.9f);   // a worn plank end
      // rails along the open sides (not where the deck continues into a wider bridge)
      // (M2 fixer round 2) and not where a road or path joins the deck from the side (a road crossing on a slant meets
      // its bridge at a corner: a rail across the join read as the road running into a fence)
      auto joins = [&](int x, int y) { const Ground q = m.at(x, y); return q == Ground::Bridge || q == Ground::Road || q == Ground::Dirt || q == Ground::Plaza; };
      bool sideA = eastWest ? !joins(tx, ty - 1) : !joins(tx - 1, ty), sideB = eastWest ? !joins(tx, ty + 1) : !joins(tx + 1, ty);
      // (M1 round 3) a rail on posts: a dark outer edge, the lit rail with a post every 6 px, and the rail's shadow on
      // the planks inside it
      const int e = eastWest ? ly : lx, along = eastWest ? px : py;
      if ((sideA && e < 3) || (sideB && e > 12)) {
        const int ee = (sideA && e < 3) ? e : 15 - e;
        const bool post = ((along % 6) + 6) % 6 < 2;
        if (ee == 0) c = C(52, 34, 24);
        else if (ee == 1) c = post ? C(164, 116, 68) : C(126, 86, 52);
        else c = post ? C(92, 60, 38) : mul(c, 0.80f);
      }
      break;
    }
    case Ground::Rock:
    case Ground::CaveWall: {
      if (g == Ground::Rock && m.kind == MapKind::Overworld && m.relief()) { c = mountainPixel(m, px, py, sx, sy); break; }
      // terraced cliffs: each rock tile has a height level; a tile whose southern neighbour is lower shows a cliff face
      bool cave = g == Ground::CaveWall;
      int gx = sx >> 4, gy = sy >> 4, sl = sx & 15, slx = sy & 15;
      (void)sl;
      int L = rockLevel(m, gx, gy), Lb = rockLevel(m, gx, gy + 1), La = rockLevel(m, gx, gy - 1);
      int faceH = cave ? 11 : 11;
      bool face = Lb < L && slx >= 16 - faceH;
      bool cold = !cave && (m.biomeAt(gx, gy) == Biome::Snow || m.biomeAt(gx, gy + 2) == Biome::Snow || m.biomeAt(gx, gy - 2) == Biome::Snow);
      if (face) {
        float k = (slx - (16 - faceH)) / (float)faceH;
        uint32_t base = cave ? C(78, 64, 58) : lerpc(C(98, 86, 82), C(112, 100, 92), (L - 1) / 3.0f);
        c = mul(base, 1.05f - k * 0.38f);
        int col = sx / 3 + (int)(hash2(sx / 3, gy, 281) % 2);
        if ((hash2(col, gy, 283) & 3) == 0) c = mul(c, 0.84f);          // vertical fissures
        if ((sx % 3) == 0) c = mul(c, 0.93f);
        if (slx == 16 - faceH) c = mul(c, 1.32f);                       // lit lip at the top of the face
        else if (slx == 16 - faceH + 1) c = mul(c, 1.12f);
        if (slx >= 14) c = mul(c, 0.72f);                                // contact shadow at the base
        if (cold && slx <= 16 - faceH + 1) c = C(236, 240, 248);         // snow overhang
      } else if (!cave) {
        // the plateau: broken stone slabs (a jittered cell pattern). Each slab is lit along its north-west edge and
        // shaded along its south-east one, with dark cracks between slabs, so the rock reads as rock at 1x and not
        // as a flat grey decal; higher terraces are a little lighter.
        const int CS = 13;
        int cx0 = (int)std::floor(sx / (float)CS), cy0 = (int)std::floor(sy / (float)CS);
        float d1 = 1e9f, d2 = 1e9f, fdx = 0, fdy = 0;
        uint32_t cellH = 0;
        for (int oy = -1; oy <= 1; oy++)
          for (int ox = -1; ox <= 1; ox++) {
            int ccx = cx0 + ox, ccy = cy0 + oy;
            uint32_t hh = hash2(ccx, ccy, 307);
            float fx = (ccx + 0.2f + (hh & 255) / 255.0f * 0.6f) * CS, fy = (ccy + 0.2f + ((hh >> 8) & 255) / 255.0f * 0.6f) * CS;
            float ddx = sx + 0.5f - fx, ddy = sy + 0.5f - fy, d = ddx * ddx + ddy * ddy;
            if (d < d1) { d2 = d1; d1 = d; fdx = ddx; fdy = ddy; cellH = hh; }
            else if (d < d2) d2 = d;
          }
        float edge = std::sqrt(d2) - std::sqrt(d1);   // distance to the crack, roughly
        float lvl = (L - 1) / 3.0f;
        uint32_t base = lerpc(C(128, 121, 110), C(150, 143, 130), lvl);
        c = mul(base, 0.93f + ((cellH >> 16) & 15) / 15.0f * 0.12f);
        float lean = (fdx + fdy) / (float)CS;          // < 0: the slab's north-west side, > 0: south-east
        if (lean < -0.55f) c = mul(c, 1.10f);
        else if (lean > 0.55f) c = mul(c, 0.90f);
        if (edge < 0.9f) c = mul(c, 0.70f);                  // crack
        else if (edge < 1.8f && lean > 0) c = mul(c, 0.84f); // the crack's shaded lip
        else if (edge < 1.8f && lean <= 0) c = mul(c, 1.06f);
        int tf = tuft(sx, sy, 297, 0.18f);
        if (tf == 1) c = mul(c, 0.8f);
        if (L == 1 && m.kind == MapKind::Overworld && vnoise(sx / 9.0f, sy / 9.0f, 299) > 0.74f && edge > 1.2f) c = lerpc(c, C(104, 128, 80), 0.35f);   // moss on the low ledges
        if (cold && (L >= 2 || vnoise(sx / 12.0f, sy / 12.0f, 301) > 0.45f)) {
          c = n < 0.5f ? C(228, 234, 244) : C(240, 244, 250);
          if (edge < 0.9f) c = C(196, 206, 224);
        }
        if (La < L && slx < 2) c = mul(c, 1.2f);    // rim where the terrace above ends
      } else {
        // calm plateau surface, lighter with height
        uint32_t lo = cave ? C(44, 37, 35) : C(126, 119, 109), hi = cave ? C(54, 46, 42) : C(134, 127, 116);
        float lvl = (L - 1) / 3.0f;
        c = n < 0.5f ? lo : hi;
        c = mul(c, 0.94f + lvl * 0.16f + (vnoise(sx / 30.0f, sy / 30.0f, 291) - 0.5f) * 0.05f);
        int tf = tuft(sx, sy, 297, 0.25f);
        if (tf == 1) c = mul(c, 0.78f); else if (tf == 2) c = mul(c, 1.12f);      // pebbles
        if (!cave && L == 1 && m.kind == MapKind::Overworld && vnoise(sx / 9.0f, sy / 9.0f, 299) > 0.74f) c = lerpc(c, C(104, 128, 80), 0.35f);   // moss on the low ledges
        if (cold && (L >= 2 || vnoise(sx / 12.0f, sy / 12.0f, 301) > 0.45f)) {
          c = n < 0.5f ? C(228, 234, 244) : C(240, 244, 250);
          if (tf == 1) c = C(206, 214, 230);
        }
        if (La < L && slx < 2) c = mul(c, 1.18f);    // rim where the terrace above ends
        if (cave) c = mul(c, 0.75f + vnoise(sx / 10.0f, sy / 10.0f, 311) * 0.2f);
      }
      break;
    }
    case Ground::CaveFloor:
      c = pick3(n, C(70, 62, 58), C(78, 70, 64), C(86, 76, 70));
      if (h < 0.015f) c = C(104, 94, 86);
      else if (h < 0.03f) c = mul(c, 0.8f);
      break;
    case Ground::InteriorWall: {
      Ground below = m.at(tx, ty + 1);
      bool stone = below == Ground::StoneFloor || m.at(tx, ty + 2) == Ground::StoneFloor;
      if (below != Ground::InteriorWall && below != Ground::Void && ty > 0) {
        if (stone) {
          int row = py / 5, shift = (row & 1) * 5;
          bool line = ((px + shift) % 10 == 0) || (py % 5 == 0);
          c = line ? C(60, 58, 64) : lerpc(C(118, 114, 120), C(134, 128, 134), hashf((px + shift) / 10, row, 321));
        } else {
          bool seam = px % 6 == 0;
          c = seam ? C(70, 44, 28) : lerpc(C(128, 84, 50), C(146, 98, 58), hashf(px / 6, 0, 331));
          if (ly % 8 == 7) c = mul(c, 0.8f);
        }
        if (ly < 2) c = C(60, 40, 30);
        if (ly == 15) c = mul(c, 0.55f);
      } else {
        c = C(34, 26, 24);
        if (h < 0.1f) c = C(42, 32, 28);
      }
      break;
    }
    case Ground::Water:
    case Ground::DeepWater: {
      // semi-transparent tint over the animated water layer drawn underneath
      bool deep = g == Ground::DeepWater;
      float shore = 0;
      if (shoreV >= 0) {
        // the smoothed bank: a band of shallows, a broken line of foam right at the edge
        if (shoreV < 0.66f) shore = 1;
        if (shoreV < 0.57f && hashf(px / 2, py / 2, 341) < 0.62f) { c = C(220, 240, 250, 210); break; }
      } else
        for (int k = 0; k < 4; k++) {
          static const int dx[4] = {3, -3, 0, 0}, dy[4] = {0, 0, 3, -3};
          Ground o = m.at((px + dx[k]) >> 4, (py + dy[k]) >> 4);
          if (o != Ground::Void && !groundWater(o) && o != Ground::Bridge && !soft(o)) shore = 1;   // built edges (quays, roads)
        }
      c = deep ? C(28, 58, 118, 190) : C(48, 112, 168, 130);
      if (m.kind == MapKind::Overworld && shore == 0) {
        // (M1) deep water shades into the shallows along a smoothed, dithered field instead of tile steps
        auto dp = [&](int x, int y) -> float { Ground q = m.at(x, y); return q == Ground::DeepWater || q == Ground::Void ? 1.0f : 0.0f; };
        float fx = (px - 7.5f) / 16.0f, fy = (py - 7.5f) / 16.0f;
        int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
        float ax = fx - ix, ay = fy - iy;
        float d00 = dp(ix, iy), d10 = dp(ix + 1, iy), d01 = dp(ix, iy + 1), d11 = dp(ix + 1, iy + 1);
        if (!(d00 == d10 && d00 == d01 && d00 == d11)) {
          float v = (d00 * (1 - ax) + d10 * ax) * (1 - ay) + (d01 * (1 - ax) + d11 * ax) * ay;
          v += (vnoise(px / 9.0f, py / 9.0f, 821) - 0.5f) * 0.36f + (bayer(px, py) - 0.5f) * 0.22f;
          c = v > 0.62f ? C(28, 58, 118, 190) : v > 0.42f ? C(38, 84, 142, 162) : C(48, 112, 168, 130);
        }
      }
      if (m.kind != MapKind::Overworld) c = C(30, 60, 90, 170);
      if (shore > 0 && !deep) c = C(110, 170, 200, 140);
      if (shore > 0 && shoreV < 0 && hashf(px / 2, py / 2, 341) < 0.5f) c = C(220, 240, 250, 210);
      // (M1 round 3) a bridge's deck throws its shadow down-right onto the water under it
      if (m.kind == MapKind::Overworld && (m.at((px - 2) >> 4, (py - 4) >> 4) == Ground::Bridge || m.at((px - 1) >> 4, (py - 2) >> 4) == Ground::Bridge))
        c = C(18, 30, 62, 200);
      break;
    }
    case Ground::Ice:
      c = pick3(n, C(170, 210, 236), C(186, 222, 242), C(200, 232, 248));
      if (h < 0.02f) c = C(255, 255, 255);
      break;
    case Ground::Lava:
      c = pick3(n, C(220, 80, 20), C(250, 130, 30), C(255, 190, 60));
      break;
    default:
      c = C(0, 0, 0);
      break;
  }
  // (M1 round 3) a crag or massif casts its shadow down-right onto the land beside it (looked up through the same
  // smooth warp that shapes the rock's outline, so the shadow follows it)
  if (m.kind == MapKind::Overworld && m.relief() && g != Ground::Rock && !groundWater(g) &&
      (m.at((px - 13) >> 4, (py - 10) >> 4) == Ground::Rock || m.at((px - 13) >> 4, (py + 6) >> 4) == Ground::Rock ||
       m.at((px + 3) >> 4, (py - 10) >> 4) == Ground::Rock || m.at((px + 3) >> 4, (py + 6) >> 4) == Ground::Rock)) {
    auto rockW = [&](int qx, int qy) {
      float wx, wy;
      rockWarp(qx, qy, wx, wy);
      return m.at((int)std::floor(wx / 16), (int)std::floor(wy / 16)) == Ground::Rock;
    };
    if (rockW(px - 4, py - 3) || rockW(px - 6, py - 1)) c = lerpc(mul(c, 0.68f), C(48, 34, 92, (int)(c >> 24)), 0.16f);
  }
  // (M1 round 3) a city wall casts its shadow east onto the ground at its foot (top-left light): a north-south run
  // shows no face to the viewer, and without the shadow it read as a flat paved strip
  if (m.kind == MapKind::Overworld && !m.wallAt(tx, ty)) {
    int dw = 0;
    for (int k = 1; k <= 10 && !dw; k++) if (m.wallAt((px - k) >> 4, ty) && m.wallAt((px - k) >> 4, (py - 6) >> 4)) dw = k;
    if (dw) c = lerpc(mul(c, 0.56f + dw * 0.025f), C(48, 34, 92, (int)(c >> 24)), 0.18f);
  }
  // ledge shading: soft ground sitting above water gets a dark lip (the bank seen from above, lit from the top-left)
  if (soft(g) && !groundWater(g) && g != Ground::CaveFloor) {
    if (shoreV >= 0) {
      if (shoreV > 0.40f) c = mul(c, 0.72f);        // the damp bank right at the water
      else if (shoreV > 0.33f) c = mul(c, 0.86f);
    } else {
      Ground b1 = m.at(px >> 4, (py + 2) >> 4);
      if (groundWater(b1) && ((py + 2) >> 4) != (py >> 4)) c = mul(c, 0.7f);
    }
  }
  if (m.kind == MapKind::Overworld && m.relief()) c = reliefPixel(m, px, py, c, g);
  return c;
}

// Relief (M1, VISION_PLAN 11; M2 readability pass): the generator gives every tile a level 0..7; the tile on the LOWER
// side of a step carries HEIGHT_CLIFF (or HEIGHT_RAMP where a ramp or stair climbs it). Drawn over that tile, light
// from the top-left, 3/4 view:
//   higher NORTH  a south-facing face (the one the viewer sees), 12 px for one level and 16 for two: a lit lip with the
//                 plateau's cover hanging over it, the face's material darkening to a crisp foot line, then a cast
//                 shadow band on the ground below (and a soft shadow thrown south-east under a two-level face)
//   higher SOUTH  the plateau's top reaches up over this tile (it stands higher, so it shows higher): a 2-px lit rim,
//                 a dark outline and a thin shadow on the ground at its foot
//   higher WEST   an east-facing 3-px side lip in shade, the shadow cast east onto the low ground
//   higher EAST   a west-facing 3-px side lip, lit
//   ramps         steps across the climb: stone stairs on roads, worn earth steps elsewhere
// Every level is graded a step brighter than the one below it (snow, which cannot get brighter, is shaded darker on the
// lower levels instead: a snowfield's steps read as steps, not as thin lines), the high ones a little hazier. The face
// material follows the land: earth banks with strata, roots and stones on temperate low ground, granite higher up,
// cold blue rock under snow, sandstone in the desert, dark mossy rock in the marsh.
namespace {
// the material of a face (VISION_PLAN 11.2): bank = soft earth layers instead of rock columns
struct FacePal { RockPal p; bool bank; };
FacePal facePal(Biome b, int level) {
  switch (b) {
    case Biome::Plains: case Biome::Forest: case Biome::Autumn: case Biome::Beach:
      if (level <= 3) return {{C(206, 178, 126), C(170, 132, 88), C(140, 104, 70), C(108, 78, 56), C(70, 50, 44), false}, true};
      return {{C(198, 188, 164), C(156, 146, 128), C(128, 118, 104), C(98, 90, 82), C(62, 56, 56), false}, false};
    default: return {rockPal(b), false};
  }
}
inline int luma8(uint32_t c) { return (int)((c & 255) * 0.3f + ((c >> 8) & 255) * 0.55f + ((c >> 16) & 255) * 0.15f); }
}  // namespace

uint32_t View::reliefPixel(const TMap& m, int px, int py, uint32_t c, Ground g) {
  const int tx = px >> 4, ty = py >> 4, lx = px & 15, ly = py & 15;
  // each level a clear step brighter than the one below (the cue that says "higher" at a glance); snow and ice are
  // already as bright as paint goes, so their lower levels are shaded darker (and cooler) instead
  const bool snowy = g == Ground::Snow || g == Ground::Ice;
  auto gradeOf = [](int lv) { return std::min(1.22f, 0.86f + 0.075f * (float)lv); };
  auto gradeSnow = [](int lv) { return 0.76f + 0.045f * (float)lv; };
  auto grade = [&](int lv) { return snowy ? gradeSnow(lv) : gradeOf(lv); };
  auto coolSnow = [](uint32_t s, int lv) {   // the lower snow lies in a cooler light
    return lerpc(s, C(150, 168, 214, (int)(s >> 24)), 0.05f * (float)std::max(0, 5 - lv));
  };
  const uint8_t bits = m.heightBits(tx, ty);
  const int l = bits & Map::HEIGHT_LEVEL;
  const int lN = m.heightAt(tx, ty - 1), lW = m.heightAt(tx - 1, ty);
  const bool water = groundWater(g) || g == Ground::Void;
  const bool ramp = (bits & Map::HEIGHT_RAMP) != 0;
  const Biome bio = m.biomeAt(tx, ty);
  const FacePal FP = facePal(bio, l);
  RockPal P = FP.p;
  auto wob = [&](int a, int b, uint32_t seed, float amp) { return (int)std::lround((vnoise(a / 7.0f, b * 1.37f, seed) - 0.5f) * amp); };
  // the aerial grade: each level a touch brighter, the high ones a little hazier
  bool snowCover = false;
  if (!water && g != Ground::Rock) {   // (a massif's rock paints its own light and snow: mountainPixel)
    c = mul(c, grade(l));
    if (snowy) c = coolSnow(c, l);
    if (l >= 4) c = lerpc(c, C(206, 214, 228, (int)(c >> 24)), 0.025f * (l - 3));
    // snowline: mountains and the cold north hold snow on their high ground. The snow cover is a field blended
    // between tile centres (each tile's biome and level), looked up through the same broad warp as the ecotones, so
    // its edge meanders and dithers instead of following the tile grid where a cold biome meets a mild one
    if (g != Ground::Road && g != Ground::Plaza && g != Ground::Bridge && g != Ground::StoneFloor && g != Ground::Dirt && g != Ground::Farmland && !snowy) {
      auto snowV = [&](int x, int y) -> float {
        const Biome b = m.biomeAt(x, y);
        const int at = b == Biome::Mountain ? 5 : (b == Biome::Snow || b == Biome::Taiga) ? 4 : 99;
        const int lv = m.heightAt(x, y);
        return lv >= at ? (lv - at + 1) * 0.42f : -0.6f;
      };
      const float wx = px + (vnoise(px / 52.0f, py / 52.0f, 811) - 0.5f) * 40.0f, wy = py + (vnoise(px / 52.0f, py / 52.0f, 817) - 0.5f) * 40.0f;
      auto bil = [&](float qx, float qy, bool& any) {
        const float fx = (qx - 7.5f) / 16.0f, fy = (qy - 7.5f) / 16.0f;
        const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
        const float ax = fx - ix, ay = fy - iy;
        const float s00 = snowV(ix, iy), s10 = snowV(ix + 1, iy), s01 = snowV(ix, iy + 1), s11 = snowV(ix + 1, iy + 1);
        if (std::max({s00, s10, s01, s11}) > 0) any = true;
        return (s00 * (1 - ax) + s10 * ax) * (1 - ay) + (s01 * (1 - ax) + s11 * ax) * ay;
      };
      // a box of four blended samples a tile and a half apart: the cover thins out over two or three tiles
      bool any = false;
      const float sv = (bil(wx - 12, wy - 12, any) + bil(wx + 12, wy - 12, any) + bil(wx - 12, wy + 12, any) + bil(wx + 12, wy + 12, any)) * 0.25f;
      if (any) {
        float k = std::min(sv, 1.0f) + (vnoise(px / 11.0f, py / 11.0f, 831) - 0.5f) * 0.7f + (vnoise(px / 4.0f, py / 4.0f, 835) - 0.5f) * 0.2f;
        const float th = 0.5f + (bayer(px, py) - 0.5f) * 0.25f;
        if (k > th) {
          c = vnoise(px / 6.0f, py / 6.0f, 833) < 0.5f ? C(230, 236, 246) : C(242, 246, 252);
          if (k < th + 0.05f) c = C(204, 212, 228);   // the thin, trodden edge of the cover
          c = coolSnow(mul(c, gradeSnow(l)), l);
          snowCover = true;
        }
      }
    }
  }
  // (M1 round 3) a bridge's deck spans the water at one height: no cliff face, rim or stair is drawn across it (the
  // faces of the banks stop at its rails and pass under it)
  if (g == Ground::Bridge) return c;
  const bool cover = snowy || snowCover;   // the ground here (and the plateau's) is snow: its rim and lip are snow
  const uint32_t alpha = 255u << 24;
  // ---- ramps and stairs: drawn only where the cliff would be (the face, side wall or rim band found below), so a
  //      stair is cut into the cliff's own slanted shape instead of standing out as a square block. ns: climbing
  //      north-south (steps lie east-west); a: the distance (px) into the band, across the steps
  auto rampPx = [&](bool ns, int a) -> uint32_t {
    const int b = ns ? px : py;
    const bool paved = g == Ground::Road || g == Ground::Plaza;
    const int k = a & 3;                          // 4 px per step
    uint32_t r = c;
    if (paved) {
      const int blk = (b + (a / 4) * 5) / 7;
      r = lerpc(C(150, 142, 128), C(172, 164, 148), hashf(blk, a / 4, 841));
      if ((b + (a / 4) * 5) % 7 == 0) r = mul(r, 0.72f);
    }
    const bool upIsNear = ns ? lN > l : lW > l;   // the higher side is north / west: risers face the light less
    if (paved) {
      if (k == 0) r = mul(r, 0.55f);                                  // the riser's shadow
      else if (k == 1) r = mul(r, upIsNear ? 1.16f : 1.10f);          // the worn, lit nose of the step
    } else {
      // (M1 round 3) a worn earth slope cut in shallow steps: each step's lip of packed earth catches the light, its
      // riser falls into shade, grass creeps in at the sides of the path
      // (M2 fixer round 2) cut in the bank's own material and solid: packed-earth treads in the face's palette, each
      // riser a dark line under a lit nose (a timber or stone edging), no grass tint washed through it (the steps
      // read as a faded fence ghosted into the grass)
      const uint32_t earth = cover ? lerpc(mul(c, 0.94f), C(176, 186, 210), 0.35f) : lerpc(P.hi, P.mid, 0.35f);
      const int wob2 = cover ? (int)(hash2(b / 4, a / 4, 845) % 2) : (int)(hash2(b / 9, a / 4, 845) % 7 == 0);   // (a step's edge runs on)
      const int kk = (a + wob2) & 3;
      if (kk == 0) r = cover ? lerpc(mul(earth, 0.62f), C(48, 34, 92), 0.12f) : lerpc(P.dark, C(48, 34, 92), 0.15f);
      else if (kk == 1) r = cover ? mul(earth, 1.14f) : lerpc(P.lip, P.hi, 0.3f + 0.4f * hashf(b / 5, a / 4, 851));
      else r = mul(earth, kk == 2 ? 0.92f : 1.0f);
      if (kk >= 2 && hashf(px, py, 847) < 0.10f) r = mul(r, 0.86f);   // pebbles in the tread
      const int eb = ns ? lx : ly;
      if ((eb == 0 || eb == 15) && hashf(px, py, 849) < 0.40f) r = mul(c, 0.9f);   // grass creeping in at the very edge
    }
    // the stair's cheeks: a strip of rock where the ramp meets a cliff beside it
    const bool cheekA = ns ? (m.heightBits(tx - 1, ty) & Map::HEIGHT_CLIFF) != 0 : (m.heightBits(tx, ty - 1) & Map::HEIGHT_CLIFF) != 0;
    const bool cheekB = ns ? (m.heightBits(tx + 1, ty) & Map::HEIGHT_CLIFF) != 0 : (m.heightBits(tx, ty + 1) & Map::HEIGHT_CLIFF) != 0;
    const int e = ns ? lx : ly;
    // (the bank's face material, not a flat strip: the cheeks read as fence posts)
    // the west cheek faces east, into the shade; the east one faces west, into the light
    if (cheekA && e < 2) r = e == 0 ? P.lo : lerpc(P.lo, P.mid, 0.5f);
    if (cheekB && e > 13) r = e == 15 ? P.mid : lerpc(P.hi, P.lip, 0.3f);
    return (r & 0x00FFFFFFu) | alpha;
  };
  // ---- level steps, drawn from a smooth level field. F is the tile levels blended between tile centres, so a step
  // that the grid lays as a staircase (a contour running on a diagonal) is drawn as one slanted cliff, and corners
  // round off, instead of a row of separate one-tile rock blocks. The tiles still decide what blocks (the lower tile of
  // every step); every drawn face lies over or right next to those tiles. The levels of the 6x6 tiles around are
  // cached per thread (bakes run on worker threads).
  struct LvCache { const void* map = nullptr; int ox = 0, oy = 0, tx = INT32_MIN, ty = INT32_MIN; uint8_t lv[36]; };
  thread_local LvCache lc;
  if (lc.map != (const void*)m.m || lc.ox != m.ox || lc.oy != m.oy || lc.tx != tx || lc.ty != ty) {
    lc.map = (const void*)m.m; lc.ox = m.ox; lc.oy = m.oy; lc.tx = tx; lc.ty = ty;
    for (int j = 0; j < 6; j++)
      for (int i = 0; i < 6; i++) lc.lv[j * 6 + i] = (uint8_t)m.heightAt(tx - 2 + i, ty - 2 + j);
  }
  bool flatHere = true;
  for (int k = 0; k < 36; k++) if (lc.lv[k] != l) { flatHere = false; break; }
  if (flatHere) return c;
  auto LV = [&](int x, int y) -> float {   // tile level, x/y relative to (tx - 2, ty - 2), clamped to the cache
    return (float)lc.lv[std::clamp(y, 0, 5) * 6 + std::clamp(x, 0, 5)];
  };
  const int bx = (tx - 2) * 16, by = (ty - 2) * 16;
  auto F = [&](int qx, int qy) -> int {    // the visible level at a pixel (rounded smooth field)
    const float fx = (qx - bx - 8) / 16.0f, fy = (qy - by - 8) / 16.0f;
    const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    const float ax = fx - ix, ay = fy - iy;
    const float v = (LV(ix, iy) * (1 - ax) + LV(ix + 1, iy) * ax) * (1 - ay) + (LV(ix, iy + 1) * (1 - ax) + LV(ix + 1, iy + 1) * ax) * ay;
    return (int)std::floor(v + 0.5f);
  };
  const int own = F(px, py);
  const bool rampHere = ramp && !water;
  auto regrade = [&](uint32_t col, int to) {   // this pixel's ground as it looks on level `to`
    uint32_t r = mul(col, grade(to) / grade(l));
    if (snowy || cover) r = lerpc(r, C(150, 168, 214, (int)(r >> 24)), 0.05f * (float)(std::max(0, 5 - to) - std::max(0, 5 - l)));
    return r;
  };
  uint32_t out = own == l ? c : regrade(c, own);   // a corner of the next level reaching over
  bool opaque = false;
  // a face's material at depth t (0 the lip .. 1 the foot): rock = irregular vertical columns (a lit left edge, a dark
  // crack on the right, a few ledges across them); bank = soft earth in wavy strata with stones and hanging roots
  bool bank = FP.bank;   // (a bank under a rock mass is rock too)
  auto faceRock = [&](float t, int d) -> uint32_t {
    uint32_t r = t < 0.5f ? lerpc(P.hi, P.mid, t * 2) : lerpc(P.mid, P.lo, (t - 0.5f) * 2);
    if (bank) {
      const int yy = py + (int)std::lround((vnoise(px / 11.0f, py / 5.0f, 905) - 0.5f) * 5.0f);
      const int band = yy >= 0 ? yy / 3 : (yy - 2) / 3;
      r = mul(r, 0.93f + hashf(band, px / 19, 907) * 0.13f);
      if (((yy % 3) + 3) % 3 == 0 && hashf(px / 5, band, 909) < 0.5f) r = mul(r, 0.88f);   // the seams between layers
      // stones bedded in the bank: lit on top, a dark edge underneath
      const float s0 = vnoise(px / 3.2f, py / 2.6f, 911), s1 = vnoise(px / 3.2f, (py + 1) / 2.6f, 911);
      if (s0 > 0.80f) r = s1 > 0.80f ? lerpc(r, P.hi, 0.55f) : mul(lerpc(r, P.mid, 0.4f), 0.70f);
      // roots and grass threads hanging from the lip
      const uint32_t rh = hash2(px, 0, 913);
      if (d >= 3 && (rh % 7) == 0 && d <= 3 + (int)((rh >> 8) % 5)) r = mul(r, 0.66f);
      if (hashf(px, py, 863) < 0.025f) r = mul(r, 1.12f);
      return r;
    }
    const int u = px + (int)std::lround((vnoise(px * 0.21f, py * 0.17f, 893) - 0.5f) * 5.0f);
    const int cw = 6;
    const int col = u >= 0 ? u / cw : (u - cw + 1) / cw;
    const int in = u - col * cw;
    r = mul(r, 0.86f + hashf(col, py / 16, 895) * 0.24f);
    if (in == 0) r = mul(r, 1.14f);
    else if (in == cw - 1) r = mul(r, 0.68f);
    else if (in == cw - 2 && hashf(col, py / 3, 899) < 0.5f) r = mul(r, 0.84f);
    const int ledge = py + (int)(hash2(col, 0, 897) % 7);
    if (ledge % 7 == 0 && hashf(col, ledge / 7, 901) < 0.16f) r = mul(r, 0.78f);
    else if (ledge % 7 == 1 && hashf(col, ledge / 7, 901) < 0.16f) r = mul(r, 1.08f);
    r = mul(r, 0.9f + vnoise(px / 3.0f, py / 7.0f, 903) * 0.2f);   // (M1 round 3) rough rock, not laid courses
    if (hashf(px, py, 863) < 0.02f) r = mul(r, 1.12f);
    return r;
  };
  const uint32_t outline = lerpc(P.dark, C(30, 24, 34), 0.35f);
  auto shade = [&](uint32_t col, float k, float hue) {   // a cast shadow: darker and cooler (the palette's shadow hue)
    return lerpc(mul(col, k), C(48, 34, 92, (int)(col >> 24)), hue);
  };
  // ---- which drop this pixel belongs to. Every direction is looked for (the distance in px to the contour, upward,
  //      downward, west and east); a contour running on a diagonal is found both ways, so one treatment is chosen by
  //      the contour's slope (the nearer way across it, the vertical one unless the contour is steep): one clean face,
  //      rim or lip per drop, never two bands crossing into stripes.
  const int FH = std::clamp(12 + wob(px, 0, 851, 2.0f), 11, 13);   // a one-level face
  const int RH = std::clamp(11 + wob(px, 0, 865, 3.0f), 9, 13);    // how far a plateau to the south reaches up
  // (M2 integration) a side wall: an east / west drop shows a real strip of rock (6 px, wavering), not a hairline
  const int SL0 = std::clamp(6 + wob(py, 0, 869, 2.0f), 5, 7);
  const int SLR = 3;   // a stair keeps its old 3-px cut where it meets a side drop
  // (M2 fixer round 2) the side wall's width follows the contour's slope: full where the drop runs north-south, none
  // where it runs on a diagonal or east-west (the face or rim band covers that), and in between it narrows smoothly, so
  // a staircase contour shows one band and not wedges of wall wherever the grid steps. The slope is read from the
  // unrounded level field over a wide stencil (about a tile and a half), which irons out the grid's steps.
  auto VF = [&](int qx, int qy) -> float {
    const float fx = (qx - bx - 8) / 16.0f, fy = (qy - by - 8) / 16.0f;
    const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    const float ax = fx - ix, ay = fy - iy;
    return (LV(ix, iy) * (1 - ax) + LV(ix + 1, iy) * ax) * (1 - ay) + (LV(ix, iy + 1) * (1 - ax) + LV(ix + 1, iy + 1) * ax) * ay;
  };
  float sgx = 0, sgy = 0;
  for (int k = -2; k <= 2; k++) {   // a 5-sample run along each way (40 px): a one-tile jog in a diagonal stays a diagonal
    const float wgt = k == 0 ? 1.0f : (k == 1 || k == -1) ? 0.75f : 0.4f;
    sgx += wgt * std::fabs(VF(px + 14, py + k * 10) - VF(px - 14, py + k * 10));
    sgy += wgt * std::fabs(VF(px + k * 10, py + 14) - VF(px + k * 10, py - 14));
  }
  const float steep = sgx + sgy > 0.01f ? sgx / (sgx + sgy) : 1.0f;   // 1: the drop runs north-south
  const float sideK = std::clamp((steep - 0.62f) / 0.26f, 0.0f, 1.0f);
  const int SL = (int)std::lround(SL0 * sideK);
  // (M2 fixer) a west-facing wall is the lit one the eye reads a drop by: 2 px wider, so a north-south drop on open
  // grass is a bank and not a path-like ribbon
  const int SLE = (int)std::lround((SL0 + 2) * sideK);
  int d = 0, ds = 0, dw = 0, de = 0, up = own;
  for (int k = 1; k <= FH + 8 && !d; k++) if (F(px, py - k) > own) d = k;
  for (int k = 1; k <= RH + 4 && !ds; k++) { const int f2 = F(px, py + k); if (f2 > own) { ds = k; up = f2; } }
  for (int k = 1; k <= SL + 7 && !dw; k++) if (F(px - k, py) > own) dw = k;
  for (int k = 1; k <= SLE + 4 && !de; k++) if (F(px + k, py) > own) de = k;
  int top = own;
  if (d > 0) for (int k = d; k <= d + 18; k++) top = std::max(top, F(px, py - k));
  const int fh = top - own >= 2 ? FH + 4 : FH;   // a two-level drop: a taller face
  const bool vFace = d > 0 && d <= fh + 4, vRim = ds > 0;
  const int vd = vFace && vRim ? std::min(d, ds) : vFace ? d : vRim ? ds : 99;
  const bool hW = dw > 0, hE = de > 0 && de <= SLE + 4;
  const int hd = hW && hE ? std::min(dw, de) : hW ? dw : hE ? de : 99;
  // (M2 fixer round 2) one continuous band per drop, whatever the contour's slope. The vertical treatments (the face
  // hanging below a higher north, the rim of a higher south) are drawn wherever the pixel lies inside their band; the
  // side walls only fill in where the contour turns steep and the vertical band gets thinner than the wall. The old
  // exclusive choice (sideways only when much nearer) flipped back and forth along a staircase contour and left
  // wedge-shaped shards of side wall with gaps of shadow between them.
  enum { NONE, FACE, RIM, LIPW, LIPE } kind = NONE;
  const bool inFace = vFace && d <= fh, inRim = vRim && ds <= RH;
  const bool inW = hW && dw <= SL, inE = hE && de <= SLE;
  (void)hd;
  if (inFace && inRim) kind = d <= ds ? FACE : RIM;
  else if (inFace) kind = FACE;
  else if (inRim) kind = RIM;
  else if (inW && inE) kind = dw <= de ? LIPW : LIPE;
  else if (inW) kind = LIPW;
  else if (inE) kind = LIPE;
  // nothing solid here: the nearest drop's shadow (the same order: a face's, then the rim's, then the side walls')
  else if (vd < 99 && (hd == 99 || vd <= hd + 3)) kind = (vFace && (!vRim || d <= ds)) ? FACE : RIM;
  else if (hd < 99) kind = (hW && (!hE || dw <= de)) ? LIPW : LIPE;
  if (kind == FACE) {
    if (bank && m.at(px >> 4, (py - d - 3) >> 4) == Ground::Rock) { bank = false; P = rockPal(Biome::Plains); }
    if (d <= fh) {
      const float t = (d - 1) / (float)std::max(1, fh - 1);
      uint32_t r = faceRock(t, d);
      // the lip: a lit edge with the plateau's cover hanging over it in ragged tufts (grass, or a snow cornice)
      const uint32_t lipCover = regrade(c, top);
      const int hang = (int)(hash2(px >> 1, (py - d) >> 4, 877) % 3) + (vnoise(px / 5.0f, (py - d) / 7.0f, 879) > 0.6f ? 1 : 0);
      if (d == 1) r = P.lip;
      else if (d == 2) r = lerpc(P.lip, P.hi, 0.5f);
      if (cover) {   // a snow cornice: a bright lip, a soft blue underside, icicle drips
        const int cn = 2 + (int)(hash2(px / 3, (py - d) >> 4, 861) % 2);
        if (d == 1) r = C(250, 252, 255);
        else if (d <= cn) r = mul(lipCover, 0.93f);
        else if (d == cn + 1) r = lerpc(r, C(160, 176, 214), 0.5f);
        else if (d <= cn + 3 && (hash2(px, 0, 867) % 5) == 0) r = lerpc(r, C(214, 226, 244), 0.7f);
      } else if (d <= hang && g != Ground::Road && g != Ground::Plaza && !water) r = mul(lipCover, 0.80f);
      if (d >= fh - 2) r = mul(r, 0.78f);   // into the shade at the foot
      if (d == fh) r = outline;             // the crisp line where the face meets the ground
      if (rampHere) return rampPx(true, d - 1);
      out = r; opaque = true;
    } else if (!rampHere) {
      // the face's cast shadow: a 4-px band on the ground at its foot, deepest against the face
      const int s = d - fh - 1;   // 0 .. 3
      out = shade(out, 0.58f + s * 0.09f, 0.22f - s * 0.04f);
    }
  } else if (kind == RIM) {
    if (rampHere && ds <= RH) return rampPx(true, RH - ds);
    if (ds <= RH - 3) { out = regrade(c, up); opaque = !water; }
    else if (ds <= RH - 1) {   // the rim, catching the light (2 px)
      const uint32_t top2 = regrade(c, up);
      out = cover ? (ds == RH - 1 ? C(252, 253, 255) : lerpc(top2, C(255, 255, 255), 0.5f))
                  : (ds == RH - 1 ? lerpc(P.lip, mul(top2, 1.3f), 0.45f) : mul(top2, 1.14f));
      opaque = true;
    } else if (ds == RH) { out = cover ? lerpc(outline, C(150, 160, 190), 0.35f) : outline; opaque = true; }
    else if (!water) out = shade(out, 0.64f + (ds - RH - 1) * 0.09f, 0.14f);   // the thin shadow at its foot
  } else if (kind == LIPW) {
    if (rampHere && dw <= SLR + 2) return rampPx(false, dw - 1);
    if (rampHere && dw <= SL) {
    } else if (dw <= SL) {   // an east-facing side wall in shade: darker toward its foot
      const float t = 0.45f + 0.45f * (float)(dw - 1) / (float)std::max(1, SL - 1);
      uint32_t r = mul(faceRock(t, 6), 0.80f);
      if (dw == 1) r = cover ? mul(regrade(c, F(px - 1, py)), 0.90f) : mul(r, 1.18f);   // the plateau's rounded edge
      else if (dw == 2) r = cover ? lerpc(r, C(196, 206, 230), 0.45f) : mul(r, 1.06f);
      if (dw == SL) r = outline;
      out = r; opaque = true;
    } else if (!water) {
      const int s = dw - SL - 1;   // 0 .. 6: the shadow cast east, fading
      out = shade(out, 0.60f + s * 0.055f, 0.20f - s * 0.025f);
    }
  } else if (kind == LIPE) {
    if (rampHere && de <= SLR) return rampPx(false, SLR - de);
    if (rampHere && de <= SLE) {
    } else if (de <= SLE) {   // a west-facing side wall, lit from the top-left: bright at the lip, into shade at the foot
      const float t = 0.08f + 0.50f * (float)(de - 1) / (float)std::max(1, SLE - 1);
      uint32_t r = faceRock(t, 6);
      if (de == SLE - 1 && SLE > 3) r = mul(r, 0.82f);
      if (de == 1) r = cover ? C(250, 252, 255) : P.lip;
      else if (de == 2) r = cover ? lerpc(r, C(214, 224, 244), 0.55f) : lerpc(P.lip, r, 0.5f);
      if (de == SLE) r = outline;
      out = r; opaque = true;
    } else if (!water) out = shade(out, 0.64f + (de - SLE - 1) * 0.08f, 0.12f);   // contact shade at the wall's foot
  }
  // (M2 fixer) the plateau's own edge above a side drop: lit where the land falls away to the west (the top-left
  // light catches the brow), shaded where it falls away to the east, so the high side reads as high
  if (kind == NONE && !water && !rampHere) {
    if (F(px - 1, py) < own || F(px - 2, py) < own) out = cover ? lerpc(out, C(255, 255, 255), 0.6f) : mul(out, F(px - 1, py) < own ? 1.20f : 1.10f);
    else if (F(px + 1, py) < own) out = mul(out, 0.84f);
  }
  // a two-level face throws a soft shadow south-east as well (light from the top-left)
  if (!opaque && !rampHere && !water) {
    bool tall = false;
    for (int k = 1; k <= 6 && !tall; k++) if (F(px - 6, py - FH - 4 - k) >= own + 2) tall = true;
    if (tall && F(px - 6, py) <= own) out = shade(out, 0.84f, 0.08f);
  }
  if (opaque) out = (out & 0x00FFFFFFu) | alpha;   // faces are solid rock over water too
  return out;
}

// A chunk is painted from its BakeJob: the snapshot (the chunk and kBakeMargin tiles around it) seen through its
// global origin, in global pixels. Everything a pixel looks at (organic borders, shores, bridges, rock terraces, wall
// and building shadows) lies within the margin, so a chunk comes out the same whichever window it was baked in.
void View::bakeChunk(const BakeJob& j, Canvas& c) {
  c = Canvas(CH * 16, CH * 16);
  bakeRows(j, c, 0, c.h);
  bakeFinish(j, c);
}

void View::bakeRows(const BakeJob& j, Canvas& c, int r0, int r1) {
  TMap tm;
  tm.m = j.map.get(); tm.ox = j.ox; tm.oy = j.oy; tm.kind = j.map->kind; tm.ecoDerive = j.ecoDerive;
  const int x0 = j.gcx * CH * 16, y0 = j.gcy * CH * 16;
  for (int y = r0; y < r1; y++)
    for (int x = 0; x < c.w; x++) c.px[(size_t)y * c.w + x] = groundPixel(tm, x0 + x, y0 + y);
}

namespace {
// cast shadow on the ground (light from the top-left): cool and darker, a deeper contact shade at the foot of walls
inline uint32_t groundShade(uint32_t p, int level) {
  if (level == 3) { uint32_t s = lerpc(p, C(48, 34, 92), 0.16f); return mul(s, 0.90f); }   // the penumbra
  uint32_t s = lerpc(p, C(48, 34, 92), level == 2 ? 0.42f : 0.30f);
  return mul(s, level == 2 ? 0.70f : 0.80f);
}
}  // namespace

// Static shadows of city walls and buildings, baked into the ground so they cost nothing per frame and sit under
// actors and props. Walls: the art's own wall shape (bevels, joins) swept down-right. Buildings: the footprint swept
// down-right by an amount that grows with the building's height, plus a contact shade along the front wall's foot.
// m is the bake snapshot; the chunk's top-left tile in it is (tx0, ty0) (the margin).
static void bakeArchShadows(const Map& m, int tx0, int ty0, Canvas& c) {
  const int x0 = tx0 * 16, y0 = ty0 * 16, x1 = x0 + CH * 16, y1 = y0 + CH * 16;
  std::vector<uint8_t> lv((size_t)c.w * c.h, 0);
  bool any = false;
  if (!m.wall.empty())
    for (int ty = ty0; ty < ty0 + CH; ty++)
      for (int tx = tx0; tx < tx0 + CH; tx++) {
        bool near = false;
        for (int oy = -1; oy <= 0 && !near; oy++)
          for (int ox = -1; ox <= 0; ox++)
            if (m.in(tx + ox, ty + oy) && m.wall[(size_t)(ty + oy) * m.w + tx + ox]) { near = true; break; }
        if (!near) continue;
        for (int y = 0; y < 16; y++)
          for (int x = 0; x < 16; x++) {
            int px = tx * 16 + x, py = ty * 16 + y;
            int l = art::wallShadeAt(m.wall.data(), m.w, m.h, px, py);
            if (l) { lv[(size_t)(py - y0) * c.w + (px - x0)] = (uint8_t)l; any = true; }
          }
      }
  for (const Bldg& b : m.bldgs) {
    int fx = b.r.x * 16, fy = b.r.y * 16, fw = b.r.w * 16, fh = b.r.h * 16;
    if (fx > x1 + 4 || fy > y1 + 4 || fx + fw + 24 < x0 || fy + fh + 16 < y0) continue;
    // (M1) the building's own biome, not the tile's: the same shadow in every window
    art::ArchStyle st = bldgArch(b);
    int hgt = art::buildingHeight(b.type, b.r.w, b.r.h, st, bldgFacts(b));
    int L = std::clamp(hgt / 4, 6, 14), Ly = std::max(3, L * 3 / 5);
    for (int py = std::max(y0, fy); py < std::min(y1, fy + fh + Ly + 2); py++)
      for (int px = std::max(x0, fx); px < std::min(x1, fx + fw + L + 2); px++) {
        if (px < fx + fw && py < fy + fh) continue;   // under the building itself
        int l = 0;
        if (py >= fy + fh && py < fy + fh + 2 && px < fx + fw + 1) l = 2;
        else
          for (int k = 1; k <= 8 && !l; k++) {
            int sx = px - L * k / 8, sy = py - Ly * k / 8;
            if (sx >= fx && sx < fx + fw && sy >= fy && sy < fy + fh) l = 1;
          }
        // (M2 fixer round 2) a soft edge: the shadow's outer two pixels are a dithered penumbra, not a hard ruled line
        if (l == 1) {
          int k2 = 0;   // is the shadow still there two pixels further out (down-right)?
          for (int k = 0; k <= 8; k++) {
            const int sx = px + 2 - L * k / 8, sy = py + 2 - Ly * k / 8;
            if (sx >= fx && sx < fx + fw && sy >= fy && sy < fy + fh) { k2 = 1; break; }
          }
          if (!k2) l = 3;
        }
        if (!l) continue;
        uint8_t& d = lv[(size_t)(py - y0) * c.w + (px - x0)];
        if (l == 3) { if (!d) d = 3; }        // (a penumbra never lightens a full shadow another building casts)
        else if (d == 3 || l > d) d = (uint8_t)l;
        any = true;
      }
  }
  if (!any) return;
  for (size_t i = 0; i < lv.size(); i++) {
    if (!lv[i]) continue;
    int px = x0 + (int)(i % c.w), py = y0 + (int)(i / c.w);
    Ground g = m.at(px >> 4, py >> 4);
    if (groundWater(g) || g == Ground::Void) continue;
    c.px[i] = groundShade(c.px[i], lv[i]);
  }
}

void View::bakeFinish(const BakeJob& j, Canvas& c) {
  const Map& m = *j.map;
  const int tx0 = j.gcx * CH - j.ox, ty0 = j.gcy * CH - j.oy;   // the chunk's top-left tile in the snapshot
  bakeArchShadows(m, tx0, ty0, c);
  // soft ambient occlusion along the base of walls and cliffs
  for (int ty = 0; ty < CH; ty++)
    for (int tx = 0; tx < CH; tx++) {
      int wx = tx0 + tx, wy = ty0 + ty;
      Ground g = m.at(wx, wy);
      if (isWall(g) || groundWater(g)) continue;
      if (g == Ground::CaveFloor && m.kind != MapKind::Overworld) continue;   // caves shade per pixel (groundPixel)
      bool wallAbove = isWall(m.at(wx, wy - 1));
      if (!wallAbove) continue;
      for (int y = 0; y < 5; y++)
        for (int x = 0; x < 16; x++) {
          uint32_t& p = c.px[(size_t)(ty * 16 + y) * c.w + tx * 16 + x];
          p = mul(p, 0.62f + y * 0.075f);
        }
    }
}

// the cache key of local chunk (cx, cy) of map m: the map id, the GLOBAL chunk and whether the chunk is partial (an
// endless window's outer ring: its margin runs past the window, so it is baked again once the window moves on)
// Global chunk coordinates wrap every 2048 chunks (65536 tiles): pixel coordinates stay positive and small enough for
// the float noise to keep its fine detail however far the player walks (one seam line per 65536 tiles).
static int wrapChunk(int c) { return c & 2047; }

uint64_t View::chunkKeyFor(const Map& m, uint64_t mapId, int cx, int cy) const {
  const int gcx = wrapChunk(cx + chunkOX_), gcy = wrapChunk(cy + chunkOY_);
  bool partial = false;
  if (chunkEndless_)
    partial = cx * CH - kBakeMargin < 0 || cy * CH - kBakeMargin < 0 || (cx + 1) * CH + kBakeMargin > m.w || (cy + 1) * CH + kBakeMargin > m.h;
  uint64_t k = mapId * 0x9E3779B97F4A7C15ull;
  k ^= ((uint64_t)(uint32_t)gcx * 0xC2B2AE3D27D4EB4Full) ^ ((uint64_t)(uint32_t)gcy * 0x165667B19E3779F9ull);
  k ^= k >> 29;
  return (k << 1) | (partial ? 1u : 0u);
}

// a snapshot of local chunk (cx, cy) and its margin
View::BakeJob View::makeJob(const Map& m, uint64_t mapId, int cx, int cy) const {
  BakeJob j;
  j.mapId = mapId;
  j.key = chunkKeyFor(m, mapId, cx, cy);
  j.gcx = wrapChunk(cx + chunkOX_); j.gcy = wrapChunk(cy + chunkOY_);
  const int S = CH + 2 * kBakeMargin;
  const int lx0 = cx * CH - kBakeMargin, ly0 = cy * CH - kBakeMargin;   // the snapshot's origin in m
  j.ox = j.gcx * CH - kBakeMargin; j.oy = j.gcy * CH - kBakeMargin;
  auto sub = std::make_shared<Map>();
  sub->kind = m.kind;
  sub->seed = m.seed;
  sub->w = S; sub->h = S;
  const size_t n = (size_t)S * S;
  sub->ground.assign(n, (uint8_t)Ground::Void);
  if (!m.wall.empty()) sub->wall.assign(n, 0);
  if (!m.biome.empty()) sub->biome.assign(n, 0);
  if (!m.height.empty()) sub->height.assign(n, 0);
  if (!m.blend.empty()) sub->blend.assign(n, 0);
  // M2 ecotones: a map whose generator writes no blend bytes gets them derived from its biomes (decided per map, so
  // every chunk of it agrees)
  j.ecoDerive = m.kind == MapKind::Overworld && !m.biome.empty() && std::none_of(m.blend.begin(), m.blend.end(), [](uint8_t v) { return v != 0; });
  for (int y = 0; y < S; y++) {
    const int sy = ly0 + y;
    if (sy < 0 || sy >= m.h) continue;
    const int xa = std::max(0, -lx0), xb = std::min(S, m.w - lx0);
    if (xa >= xb) continue;
    const size_t si = (size_t)sy * m.w + (size_t)(lx0 + xa), di = (size_t)y * S + (size_t)xa;
    const size_t len = (size_t)(xb - xa);
    std::copy_n(m.ground.begin() + si, len, sub->ground.begin() + di);
    if (!m.wall.empty()) std::copy_n(m.wall.begin() + si, len, sub->wall.begin() + di);
    if (!m.biome.empty()) std::copy_n(m.biome.begin() + si, len, sub->biome.begin() + di);
    if (!m.height.empty()) std::copy_n(m.height.begin() + si, len, sub->height.begin() + di);
    if (!m.blend.empty()) std::copy_n(m.blend.begin() + si, len, sub->blend.begin() + di);
  }
  // the buildings whose footprint (or shadow, up to a tile and a half down-right) reaches the snapshot
  for (const Bldg& b : m.bldgs) {
    if (b.r.x + b.r.w + 2 < lx0 || b.r.y + b.r.h + 2 < ly0 || b.r.x > lx0 + S || b.r.y > ly0 + S) continue;
    Bldg c = b;
    c.r.x -= lx0; c.r.y -= ly0;
    sub->bldgs.push_back(c);
  }
  j.map = std::move(sub);
  return j;
}

void View::workerLoop() {
  for (;;) {
    BakeJob job;
    {
      std::unique_lock<std::mutex> lk(mu_);
      cv_.wait(lk, [&] { return quit_ || !jobs_.empty(); });
      if (quit_) return;
      job = jobs_.front();
      jobs_.pop_front();
    }
    BakeDone d;
    d.key = job.key; d.mapId = job.mapId;
    auto t0 = std::chrono::steady_clock::now();
    bakeChunk(job, d.c);
    d.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::lock_guard<std::mutex> lk(mu_);
    done_.push_back(std::move(d));
  }
}

void View::clearChunks() {
  for (auto& ch : chunks_) pix_->destroy(ch.tex);
  chunks_.clear();
  std::lock_guard<std::mutex> lk(mu_);
  jobs_.clear();
  pending_.clear();
}

void View::prefetch(const Map& m, uint64_t mapId, Vec2 cam) {
#ifndef __EMSCRIPTEN__
  if (!worker_.joinable() && !singleThread()) {
    worker_ = std::thread([this] { workerLoop(); });
    const unsigned hw = std::thread::hardware_concurrency();
    for (unsigned i = 1; i < std::min(3u, hw > 2 ? hw - 2 : 1u); i++) workers_.emplace_back([this] { workerLoop(); });
  }
#endif
  int c0x = (int)std::floor((cam.x - 256) / 512), c0y = (int)std::floor((cam.y - 256) / 512);
  int c1x = (int)std::floor((cam.x + Pix::W + 256) / 512), c1y = (int)std::floor((cam.y + Pix::H + 256) / 512);
  std::lock_guard<std::mutex> lk(mu_);
  // keep only jobs for the current map (the global chunk key survives window shifts, so those jobs stay)
  for (size_t i = 0; i < jobs_.size();) {
    if (jobs_[i].mapId == mapId) { i++; continue; }
    // forget it was pending too, or coming back to that map would wait forever and bake inline (a hitch)
    uint64_t k = jobs_[i].key;
    pending_.erase(std::remove(pending_.begin(), pending_.end(), k), pending_.end());
    jobs_.erase(jobs_.begin() + i);
  }
  // finished bakes nobody will collect (another map, or a chunk already baked inline) are ~1 MB each: drop them
  for (size_t i = 0; i < done_.size();) {
    bool stale = done_[i].mapId != mapId;
    for (auto& ch : chunks_) if (ch.key == done_[i].key) stale = true;
    if (stale) {
      uint64_t k = done_[i].key;
      pending_.erase(std::remove(pending_.begin(), pending_.end(), k), pending_.end());
      done_.erase(done_.begin() + i);
    } else i++;
  }
  for (int cy = c0y; cy <= c1y; cy++)
    for (int cx = c0x; cx <= c1x; cx++) {
      if (cx < 0 || cy < 0 || cx * 32 >= m.w || cy * 32 >= m.h) continue;
      const uint64_t k = chunkKeyFor(m, mapId, cx, cy);
      bool have = false;
      for (auto& ch : chunks_) if (ch.key == k) { have = true; break; }
      if (have) continue;
      if (std::find(pending_.begin(), pending_.end(), k) != pending_.end()) continue;
      pending_.push_back(k);
      jobs_.push_back(makeJob(m, mapId, cx, cy));
    }
  cv_.notify_all();
}

void View::bakeVisibleNow(const Map& m, uint64_t mapId, int c0x, int c0y, int c1x, int c1y) {
#ifdef __EMSCRIPTEN__
  (void)m; (void)mapId; (void)c0x; (void)c0y; (void)c1x; (void)c1y;   // no threads: chunkTex bakes them one by one
#else
  if (singleThread()) return;
  std::vector<BakeJob> need;
  {
    std::lock_guard<std::mutex> lk(mu_);
    for (int cy = c0y; cy <= c1y; cy++)
      for (int cx = c0x; cx <= c1x; cx++) {
        if (cx < 0 || cy < 0 || cx * 32 >= m.w || cy * 32 >= m.h) continue;
        const uint64_t k = chunkKeyFor(m, mapId, cx, cy);
        bool have = false;
        for (auto& ch : chunks_) if (ch.key == k) { have = true; break; }
        for (auto& d : done_) if (d.key == k) { have = true; break; }
        // a job the worker already started is still pending without a queue entry: let chunkTex wait it out inline
        if (!have) need.push_back(makeJob(m, mapId, cx, cy));
      }
  }
  if (need.size() < 2) return;
  std::vector<BakeDone> out(need.size());
  std::vector<std::thread> th;
  auto t0 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < need.size(); i++)
    th.emplace_back([this, &need, &out, i] { out[i].key = need[i].key; out[i].mapId = need[i].mapId; bakeChunk(need[i], out[i].c); });
  for (auto& t : th) t.join();
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  perf_.inlineBakes += (int)need.size();
  perf_.bakeMs += ms;
  perf_.worstBakeMs = std::max(perf_.worstBakeMs, ms);
  if (getenv("EMB_TIMING")) printf("%zu chunks baked in parallel in %.1f ms\n", need.size(), ms);
  std::lock_guard<std::mutex> lk(mu_);
  for (auto& d : out) {
    for (size_t i = 0; i < jobs_.size();) if (jobs_[i].key == d.key) jobs_.erase(jobs_.begin() + i); else i++;
    pending_.push_back(d.key);   // chunkTex takes it from done_ and clears this
    d.ms = 0;
    done_.push_back(std::move(d));
  }
#endif
}

Tex View::chunkTex(const Map& m, uint64_t mapId, int cx, int cy) {
  const uint64_t key = chunkKeyFor(m, mapId, cx, cy);
  for (auto& ch : chunks_)
    if (ch.key == key) { ch.used = t_; return ch.tex; }
  auto store = [&](Canvas& c) -> Tex {
    Chunk* slot = nullptr;
    // enough for the visible chunks of a wide phone screen and the ring prefetched around them
    const size_t cap = (size_t)std::max(24, 2 * (Pix::W / 512 + 3) * (Pix::H / 512 + 3));
    if (chunks_.size() < cap) { chunks_.push_back(Chunk()); slot = &chunks_.back(); }
    else {
      slot = &chunks_[0];
      for (auto& ch : chunks_) if (ch.used < slot->used) slot = &ch;
      pix_->destroy(slot->tex);
    }
    slot->tex = pix_->bake(c);
    slot->key = key; slot->used = t_;
    return slot->tex;
  };
  // finished in the background?
  {
    std::unique_lock<std::mutex> lk(mu_);
    for (size_t i = 0; i < done_.size(); i++)
      if (done_[i].key == key) {
        BakeDone d = std::move(done_[i]);
        done_.erase(done_.begin() + i);
        pending_.erase(std::remove(pending_.begin(), pending_.end(), key), pending_.end());
        lk.unlock();
        if (d.ms > 0) perf_.bakes++;   // (0: one of bakeVisibleNow's, counted there)
        perf_.bakeMs += d.ms;
        perf_.worstBakeMs = std::max(perf_.worstBakeMs, d.ms);
        return store(d.c);
      }
  }
  // needed right now (teleport / first frame): bake synchronously. (M2) Not while an arrival bakes behind the fade:
  // travelArrive waits for the bakers instead, so no frame stalls
  if (arriving_) return Tex{};
  Canvas c;
  auto t0 = std::chrono::steady_clock::now();
  bakeChunk(makeJob(m, mapId, cx, cy), c);
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  perf_.inlineBakes++;
  perf_.bakeMs += ms;
  perf_.worstBakeMs = std::max(perf_.worstBakeMs, ms);
  if (getenv("EMB_TIMING")) printf("chunk %d,%d baked inline in %.1f ms\n", cx + chunkOX_, cy + chunkOY_, ms);
  {
    std::lock_guard<std::mutex> lk(mu_);
    for (size_t i = 0; i < jobs_.size();) if (jobs_[i].key == key) jobs_.erase(jobs_.begin() + i); else i++;
    pending_.erase(std::remove(pending_.begin(), pending_.end(), key), pending_.end());
  }
  return store(c);
}

void View::shutdown() {
  {
    std::lock_guard<std::mutex> lk(mu_);
    quit_ = true;
  }
  cv_.notify_all();
  if (worker_.joinable()) worker_.join();
  for (auto& w : workers_) if (w.joinable()) w.join();
  workers_.clear();
}

// Without threads (the web build) the prefetch queue is baked a few rows at a time on the main thread,
// within a per-frame time budget, so walking into new terrain never stalls a frame.
void View::pumpBake(double budgetMs) {
  if (!singleThread()) return;
  auto t0 = std::chrono::steady_clock::now();
  for (;;) {
    if (!incrOn_) {
      std::lock_guard<std::mutex> lk(mu_);
      if (jobs_.empty()) return;
      incrJob_ = jobs_.front();
      jobs_.pop_front();
      incrCanvas_ = Canvas(CH * 16, CH * 16);
      incrRow_ = 0;
      incrOn_ = true;
    }
    // (M2 fixer round 3) four rows a step: a step of 16 rows of a rocky or built chunk ran 3-6 ms past the budget
    int r1 = std::min(incrCanvas_.h, incrRow_ + 4);
    bakeRows(incrJob_, incrCanvas_, incrRow_, r1);
    incrRow_ = r1;
    if (incrRow_ >= incrCanvas_.h) {
      bakeFinish(incrJob_, incrCanvas_);
      BakeDone d;
      d.key = incrJob_.key; d.mapId = incrJob_.mapId; d.c = std::move(incrCanvas_);
      std::lock_guard<std::mutex> lk(mu_);
      done_.push_back(std::move(d));
      incrOn_ = false;
      incrJob_.map.reset();
    }
    if (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() > budgetMs) return;
  }
}

// M2 travel behind the fade (view.h, owner carry-over: the ~180 ms fast-travel hitch). The SIM lane moves the window
// and the player in one cheap step and holds the screen black (Arrive); here, a frame at a time, the camera is snapped
// to the arrival, the terrain chunks it shows plus one ring around them are baked (desktop: the background bakers;
// the web: the incremental pump, about 9 ms a frame), uploaded a few per frame, and the building and wall sprites in
// view are painted within a budget. Meanwhile chunkTex never bakes inline (arriving_). Only when all of it is ready
// does finishTravel() lift the fade, onto a finished picture: no frame stalls and nothing pops in.
void View::travelArrive(Game& g) {
  using SClock = std::chrono::steady_clock;
  auto msSince = [](SClock::time_point t) { return std::chrono::duration<double, std::milli>(SClock::now() - t).count(); };
  auto wallMs = [] { return std::chrono::duration<double, std::milli>(SClock::now().time_since_epoch()).count(); };
  if (!arrival_.on) {
    arrival_ = Arrival();
    arrival_.on = true;
    arrival_.wall0 = wallMs();
  }
  arriving_ = true;
  arrival_.frames++;
  g.travel.viewed = true;
  const bool st = singleThread();
  const auto t0 = SClock::now();
  auto spent = [&] { return msSince(t0); };
  snap(g);
  const Map* mp = nullptr;
  const uint64_t mapId = terrainFrame(g, mp);
  const Map& m = *mp;
  const Vec2 cam(std::floor(cam_.x), std::floor(cam_.y));
  const int v0x = (int)std::floor(cam.x / 512), v0y = (int)std::floor(cam.y / 512);
  const int v1x = (int)std::floor((cam.x + Pix::W) / 512), v1y = (int)std::floor((cam.y + Pix::H) / 512);
  // the chunks the fade waits for: desktop, the view and a ring round it (the background bakers are quick); without
  // threads only what the first frames after the fade can show (the view plus 64 px); the ring is baked afterwards by
  // the per-frame pump like any walk's prefetch
  const int n0x = st ? (int)std::floor((cam.x - 64) / 512) : v0x - 1, n0y = st ? (int)std::floor((cam.y - 64) / 512) : v0y - 1;
  const int n1x = st ? (int)std::floor((cam.x + Pix::W + 64) / 512) : v1x + 1, n1y = st ? (int)std::floor((cam.y + Pix::H + 64) / 512) : v1y + 1;
  auto inMap = [&](int cx, int cy) { return cx >= 0 && cy >= 0 && cx * CH < m.w && cy * CH < m.h; };
  if (st && arrival_.frames == 1) {
    // without threads, bakes still queued for where the player was are dropped, or the pump works through them first
    std::lock_guard<std::mutex> lk(mu_);
    auto far = [&](const BakeJob& j) {
      const int cx = j.gcx - chunkOX_, cy = j.gcy - chunkOY_;
      return j.mapId != mapId || cx < v0x - 1 || cy < v0y - 1 || cx > v1x + 1 || cy > v1y + 1;
    };
    for (size_t i = 0; i < jobs_.size();) {
      if (!far(jobs_[i])) { i++; continue; }
      const uint64_t k = jobs_[i].key;
      pending_.erase(std::remove(pending_.begin(), pending_.end(), k), pending_.end());
      jobs_.erase(jobs_.begin() + (std::ptrdiff_t)i);
    }
    if (incrOn_ && far(incrJob_)) {
      pending_.erase(std::remove(pending_.begin(), pending_.end(), incrJob_.key), pending_.end());
      incrOn_ = false;
      incrJob_.map.reset();
    }
  }
  prefetch(m, mapId, cam);   // (starts the bakers; queues the half-chunk margin drawWorld keeps)
  {
    std::lock_guard<std::mutex> lk(mu_);
    for (int cy = v0y - 1; cy <= v1y + 1; cy++)
      for (int cx = v0x - 1; cx <= v1x + 1; cx++) {
        if (!inMap(cx, cy)) continue;
        const uint64_t k = chunkKeyFor(m, mapId, cx, cy);
        bool have = false;
        for (auto& ch : chunks_) if (ch.key == k) { have = true; break; }
        if (have) continue;
        // the visible ones first (one already queued moves to the front)
        const bool vis = cx >= v0x && cx <= v1x && cy >= v0y && cy <= v1y;
        if (std::find(pending_.begin(), pending_.end(), k) != pending_.end()) {
          if (!vis) continue;
          for (size_t i = 1; i < jobs_.size(); i++)
            if (jobs_[i].key == k) {
              BakeJob j = jobs_[i];
              jobs_.erase(jobs_.begin() + (std::ptrdiff_t)i);
              jobs_.push_front(std::move(j));
              break;
            }
          continue;
        }
        pending_.push_back(k);
        if (vis) jobs_.push_front(makeJob(m, mapId, cx, cy)); else jobs_.push_back(makeJob(m, mapId, cx, cy));
      }
  }
  cv_.notify_all();
  // upload finished chunks (a texture each), a few per frame, visible ones first. (M2 fixer round 2) Without threads
  // every stage draws on one shared budget of about 12 ms a frame (the stages' own 6 + 6 + 4 + 6 ms added up past a
  // frame, and on a phone's wasm each frame ran 2-3x that): uploads, then sprite paints (one only starts while half the
  // budget is left: a palace cannot be split), then character sheets, then the bake pump with what is left.
  constexpr double kArriveBudget = 12.0;
  bool ready = true;
  int uploads = 0;
  const auto tu = SClock::now();
  for (int pass = 0; pass < 2; pass++)
    for (int cy = v0y - 1; cy <= v1y + 1; cy++)
      for (int cx = v0x - 1; cx <= v1x + 1; cx++) {
        if (!inMap(cx, cy)) continue;
        const bool vis = cx >= v0x && cx <= v1x && cy >= v0y && cy <= v1y;
        if (vis != (pass == 0)) continue;
        const bool need = cx >= n0x && cx <= n1x && cy >= n0y && cy <= n1y;
        const uint64_t k = chunkKeyFor(m, mapId, cx, cy);
        bool have = false;
        for (auto& ch : chunks_) if (ch.key == k) { have = true; ch.used = t_; break; }
        if (have) continue;
        bool done = false;
        {
          std::lock_guard<std::mutex> lk(mu_);
          for (auto& dd : done_) if (dd.key == k) { done = true; break; }
        }
        if (done && uploads < 3 && (st ? spent() < kArriveBudget * 0.5 : msSince(tu) < 6.0)) { chunkTex(m, mapId, cx, cy); uploads++; continue; }
        if (need) ready = false;
      }
  // the building and wall sprites the arrival shows, painted ahead within their own budget
  const auto tb = SClock::now();
  auto paintOver = [&] { return st ? spent() > kArriveBudget * 0.5 : spent() > 8.0; };
  (void)tb;
  if (m.kind == MapKind::Overworld || m.kind == MapKind::Interior) {
    static std::vector<int> vis;
    bldgMap_ = &m;
    // the view, and every building drawWorld's one-a-frame prefetch would paint once the fade lifts (within 720 x 520
    // px of the player): a palace there took 25 ms in the first frames after a city arrival
    const Vec2 pp = g.pl().p;
    bldgsIn(g, m, std::min(cam.x - 32, pp.x - 720), std::min(cam.y - 32, pp.y - 520), std::max(cam.x + Pix::W + 32, pp.x + 720),
            std::max(cam.y + Pix::H + 32, pp.y + 520), vis);
    bool painted = false;
#ifndef __EMSCRIPTEN__
    if (!st) {
      painted = true;
      // desktop: the sprites are painted on worker threads (a palace takes 25 ms) and stored here as they finish
      for (size_t i = 0; i < bldgAsync_.size();) {
        if (bldgAsync_[i].wait_for(std::chrono::seconds(0)) != std::future_status::ready) { i++; continue; }
        if (paintOver()) { ready = false; break; }
        BldgPaint p = bldgAsync_[i].get();
        if (!bldgTex_.count(p.key)) { storeBldg(p); bldgUsed_[p.key] = t_; }
        bldgAsync_.erase(bldgAsync_.begin() + (std::ptrdiff_t)i);
        bldgAsyncKeys_.erase(bldgAsyncKeys_.begin() + (std::ptrdiff_t)i);
      }
      for (int bi : vis) {
        const Bldg& b = m.bldgs[(size_t)bi];
        const uint64_t k = bldgKey(m, b, bi);
        if (bldgTex_.count(k)) continue;
        ready = false;
        if (std::find(bldgAsyncKeys_.begin(), bldgAsyncKeys_.end(), k) != bldgAsyncKeys_.end()) continue;
        if (bldgAsync_.size() >= 6) continue;   // a few at a time
        bldgAsync_.push_back(std::async(std::launch::async, [b, k] { return paintBldg(b, k); }));
        bldgAsyncKeys_.push_back(k);
      }
    }
#endif
    if (!painted)
      for (int bi : vis) {
        const Bldg& b = m.bldgs[(size_t)bi];
        if (bldgTex_.count(bldgKey(m, b, bi))) continue;
        if (paintOver()) { ready = false; break; }
        bldgTex(b, bi);
      }
    if (!wallKeys_.empty() && wallKeys_.size() == (size_t)m.w * m.h) {
      const int tx0 = std::max(0, (int)std::floor(cam.x / 16) - 3), ty0 = std::max(0, (int)std::floor(cam.y / 16) - 2);
      const int tx1 = std::min(m.w, tx0 + Pix::W / 16 + 7), ty1 = std::min(m.h, ty0 + Pix::H / 16 + 8);
      bool over = false;
      for (int ty = ty0; ty < ty1 && !over; ty++)
        for (int tx = tx0; tx < tx1; tx++) {
          const uint32_t wk = wallKeys_[(size_t)ty * m.w + tx];
          if (!wk || wallTiles_.count(wk)) continue;
          if (paintOver()) { ready = false; over = true; break; }
          wallTileTex(wk);
        }
    } else if (!m.wall.empty() && arrival_.frames < 2) ready = false;   // drawWorld lays the wall keys out this frame
  }
  // the people the arrival shows: their character sheets painted ahead (a city square wakes thirty at once)
  const auto th = SClock::now();
  for (const Actor& a : g.actors) {
    if (!a.human || a.p.x < cam.x - 96 || a.p.x > cam.x + Pix::W + 96 || a.p.y < cam.y - 96 || a.p.y > cam.y + Pix::H + 128) continue;
    if (humans_.count(a.look.key())) continue;
    if (st ? spent() > kArriveBudget * 0.75 : spent() > 8.0) { ready = false; break; }
    humanTex(a.look);
  }
  // without threads: the incremental bake pump, last, with what is left of about 16 ms (at least 6)
  if (st) pumpBake(std::max(2.0, kArriveBudget - spent()));
  (void)th;
  arrival_.ms += spent();
  if (getenv("EMB_TIMING")) printf("arrival frame %d: %.2f ms\n", arrival_.frames, spent());
  arrival_.t += 1.0f / 60.0f;
  // a safety net, by the clock (a slow phone takes more frames, not more seconds): never hold the black for more
  // than 6 s (a chunk stuck in a queue bakes inline then)
  const double held = wallMs() - arrival_.wall0;
  if ((ready && arrival_.frames >= 2) || held > 6000.0) {
    if (getenv("EMB_TIMING") || getenv("EMB_PERF"))
      printf("arrival: %d frames, %.0f ms held, %.1f ms of view work%s\n", arrival_.frames, held, arrival_.ms, ready ? "" : " (timed out)");
    arriving_ = false;
    arrival_ = Arrival();
    g.finishTravel();
  }
}
