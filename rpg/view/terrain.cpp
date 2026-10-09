// Terrain painting: every ground pixel is computed procedurally and baked into 32x32-tile chunk textures.
// Natural terrains get organic, domain-warped borders; built surfaces (roads, floors) stay crisp.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <mutex>
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
// (M3c) lava too: an ash field's rift meets the land along an organic edge, not a tile's square
bool natural(Ground g) { return soft(g) || g == Ground::Rock || g == Ground::CaveWall || g == Ground::Lava; }
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
  // (M3 fixer) a stilt town's boardwalk is not a river bridge: its staircases stay square platforms (boardwalkPixel)
  auto isB = [&](int x, int y) { return m.at(x, y) == Ground::Bridge && (m.blendAt(x, y) >> 4) != (Map::BOARDWALK_MARK >> 4); };
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

// (M4, owner carry-over: "marsh boardwalks still staircase on diagonals") a boardwalk (Map::BOARDWALK_MARK: a stilt
// town's walks, and the wild marsh's road boardwalks the chunk generator marks) laid on a diagonal is a 4-connected
// staircase of tiles; drawn tile by tile it was a chain of square platforms touching at their corners. Like the river
// bridges above, such a stretch is read as one straight walk: the band's centre is the mean diagonal of the boardwalk
// cells nearby, BW_HW px either side of it, its planks laid across it in the marsh boardwalk's weathered boards, an edge
// beam along both sides with a post head every 16 px, its south face (fascia and posts) and its shadow on the marsh
// below it. Where the stretch turns or ends the square platforms take over (the band ends a tile past the last cell).
struct BwDiag { bool diag = false, down = false; float kc = 0, amin = 0, amax = 0; };
template <class TM>
BwDiag boardwalkDiagAt(const TM& m, int tx, int ty) {
  struct Ent { const void* m; int tx, ty; BwDiag d; };
  thread_local Ent cache[64];
  Ent& e = cache[(unsigned)(tx * 7 + ty * 13) & 63u];
  if (e.m == (const void*)&m && e.tx == tx && e.ty == ty) return e.d;
  BwDiag r;
  auto isW = [&](int x, int y) { return m.at(x, y) == Ground::Bridge && (m.blendAt(x, y) >> 4) == (Map::BOARDWALK_MARK >> 4); };
  if (!isW(tx, ty)) return r;   // (only a boardwalk tile has a stretch)
  int n = 0;
  float mx = 0, my = 0;
  int cxs[49], cys[49];
  for (int oy = -3; oy <= 3; oy++)
    for (int ox = -3; ox <= 3; ox++)
      if (isW(tx + ox, ty + oy)) { cxs[n] = tx + ox; cys[n] = ty + oy; mx += (float)ox; my += (float)oy; n++; }
  if (n >= 4) {
    mx /= (float)n; my /= (float)n;
    float sxx = 0, syy = 0, sxy = 0;
    for (int k = 0; k < n; k++) {
      const float dx = (float)(cxs[k] - tx) - mx, dy = (float)(cys[k] - ty) - my;
      sxx += dx * dx; syy += dy * dy; sxy += dx * dy;
    }
    // a run along one diagonal, not a broad deck (a plaza of planks has no strong diagonal spread)
    if (std::fabs(sxy) >= 0.62f * std::sqrt(sxx * syy) && std::min(sxx, syy) >= 0.35f * std::max(sxx, syy)) {
      r.diag = true;
      r.down = sxy > 0;
      r.amin = 1e9f; r.amax = -1e9f;
      for (int k = 0; k < n; k++) {
        const float cx = cxs[k] * 16.0f + 8.0f, cy = cys[k] * 16.0f + 8.0f;
        r.kc += r.down ? (cx - cy) / 16.0f : (cx + cy) / 16.0f;
        const float a = r.down ? (cx + cy) / 16.0f : (cx - cy) / 16.0f;
        r.amin = std::min(r.amin, a - 1.0f);
        r.amax = std::max(r.amax, a + 1.0f);
      }
      r.kc /= (float)n;
    }
  }
  e.m = (const void*)&m; e.tx = tx; e.ty = ty; e.d = r;
  return r;
}
// 1: plank deck (out = its colour), 2: the walk's south face / shadow on the marsh (out), 0: not part of a diagonal
// walk. `openWater` (set with 0 on a boardwalk tile outside the band): draw the marsh there instead of planks.
template <class TM>
int diagBoardwalkPixel(const TM& m, int px, int py, Ground real, uint32_t& out, bool& openWater) {
  openWater = false;
  const int tx = px >> 4, ty = py >> 4;
  auto isW = [&](int x, int y) { return m.at(x, y) == Ground::Bridge && (m.blendAt(x, y) >> 4) == (Map::BOARDWALK_MARK >> 4); };
  // the stretch this pixel may belong to: its own tile's, else a boardwalk tile's beside it (the band reaches into
  // the water beside the staircase), the nearest first
  BwDiag D;
  bool found = false;
  if (isW(tx, ty)) { D = boardwalkDiagAt(m, tx, ty); found = D.diag; if (!found) return 0; }
  else {
    static const int ord[8][2] = {{0, -1}, {-1, 0}, {1, 0}, {0, 1}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    for (int k = 0; k < 8 && !found; k++)
      if (isW(tx + ord[k][0], ty + ord[k][1])) { D = boardwalkDiagAt(m, tx + ord[k][0], ty + ord[k][1]); found = D.diag; }
    if (!found) return 0;
  }
  constexpr float BW_HW = 12.5f;
  const float S = 16.0f * 0.70710678f;
  auto acrossF = [&](float x, float y) { return ((D.down ? (x - y) : (x + y)) / 16.0f - D.kc) * S; };
  auto alongF = [&](float x, float y) { return (D.down ? (x + y) : (x - y)) / 16.0f; };
  auto inBand = [&](int qx, int qy) {
    const float d = acrossF(qx + 0.5f, qy + 0.5f), a = alongF(qx + 0.5f, qy + 0.5f);
    return std::fabs(d) <= BW_HW && a > D.amin && a < D.amax;
  };
  const float d = acrossF(px + 0.5f, py + 0.5f), ad = std::fabs(d);
  const bool onDeckTile = real == Ground::Bridge;
  const float a = alongF(px + 0.5f, py + 0.5f);
  if (ad <= BW_HW && (onDeckTile || (a > D.amin && a < D.amax)) && real != Ground::Road) {
    const int ap = D.down ? px + py : px - py;   // along the walk in px steps (the planks lie across it)
    if (ad > BW_HW - 2.5f) {
      // the edge beams: a dark outer line, a lit top on the side facing the light (north-west), post heads every 16 px
      const int q = ((ap % 16) + 16) % 16;
      const bool post = q < 3;
      const bool litSide = D.down ? d > 0 : d < 0;   // the side toward the top-left light
      if (ad > BW_HW - 1.0f) out = C(56, 40, 30);
      else if (post) out = q == 0 ? C(176, 136, 90) : C(132, 98, 64);
      else out = litSide ? C(168, 130, 86) : C(112, 82, 54);
      return 1;
    }
    const int row = ((ap / 5) % 4096 + 4096) % 4096;
    const bool gap = ((ap % 5) + 5) % 5 == 0;
    const int alongPlank = (int)std::floor(d + 32.0f);
    const int joff = (int)(hash2(row, 0, 1411) % 11u);
    const bool butt = ((alongPlank + joff) % 11) == 0;
    uint32_t c = gap ? C(58, 42, 30) : lerpc(C(124, 96, 64), C(150, 118, 80), hashf(row, (alongPlank + joff) / 11, 1413));
    if (!gap && ((ap % 5) + 5) % 5 == 1) c = mul(c, 1.08f);   // each plank's lit edge
    if (butt && !gap) c = C(84, 62, 42);
    if (!gap && vnoise(px / 9.0f, py / 9.0f, 1415) > 0.68f) c = lerpc(c, C(132, 132, 116), 0.3f);   // silvered by the damp
    out = c;
    return 1;
  }
  // below the walk: its south face (the deck's edge seen from the front, 2 px), its posts down into the marsh, then its
  // shade down-right (the light from the top-left)
  if (inBand(px, py - 1) || inBand(px, py - 2)) {
    out = inBand(px, py - 1) ? C(84, 60, 40) : C(60, 44, 32);
    return 2;
  }
  if (inBand(px, py - 3) || inBand(px, py - 6)) {
    const int ap = D.down ? px + py : px - py;
    const int q = ((ap % 16) + 16) % 16;
    if ((q == 1 || q == 2) && inBand(px, py - 6)) { out = q == 1 ? C(108, 80, 54) : C(66, 48, 34); return 2; }
  }
  if (onDeckTile) openWater = true;   // the staircase's corner outside the walk: the marsh shows
  if (inBand(px - 3, py - 4) || inBand(px - 2, py - 7)) {
    out = 0xFFFFFFFFu;   // (the caller darkens what it draws there)
    return 3;
  }
  return 0;
}

// (M4) the diagonal boardwalk's deck covers this tile's centre (the view leaves the walk-through covers there undrawn)
bool boardwalkCoversTileImpl(const Map& m, int tx, int ty) {
  if (!m.in(tx, ty) || m.at(tx, ty) == Ground::Bridge) return false;
  bool near = false;
  for (int oy = -1; oy <= 1 && !near; oy++)
    for (int ox = -1; ox <= 1; ox++)
      if (m.at(tx + ox, ty + oy) == Ground::Bridge && (m.blendAt(tx + ox, ty + oy) >> 4) == (Map::BOARDWALK_MARK >> 4)) { near = true; break; }
  if (!near) return false;
  uint32_t o = 0;
  bool ow = false;
  return diagBoardwalkPixel(m, tx * 16 + 8, ty * 16 + 10, m.at(tx, ty), o, ow) == 1;
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
// M3c Wildlands (LAND lane): the biome proper of a tile and the one it blends toward, read straight off the snapshot
// (a map without the eco layer gives its family's classic eco)
template <class TM>
inline Eco ecoT(const TM& m, int x, int y) { return m.m->ecoAt(x - m.ox, y - m.oy); }
template <class TM>
inline Eco ecoNbT(const TM& m, int x, int y) { return m.m->ecoNbAt(x - m.ox, y - m.oy); }
// the ground a pixel of an ecotone shows (Ground::Void: no ecotone here). (M3c) The blend is between BIOMES PROPER, not
// families: two ecos of one family on the same ground (a meadow meeting a savanna, a birch wood a dark forest) interlock
// the same way two families do. outE: the eco the pixel shows.
template <class TM>
Ground ecotonePixel(const TM& m, int px, int py, Ground g, Eco& outE) {
  // (fixer M4 r2, review: "heath meets lush meadow along an almost vertical hard line") the blend is read through a
  // broad, slow warp (about a tile and a half each way over four tiles), so a boundary the eco map lays along a tile
  // column or row for a long way meanders like a natural edge instead of ruling a straight line across the land
  const float wpx = px + (vnoise(px / 64.0f, py / 64.0f, 871) - 0.5f) * 48.0f;
  const float wpy = py + (vnoise(px / 64.0f, py / 64.0f, 873) - 0.5f) * 48.0f;
  const float fx = (wpx - 7.5f) / 16.0f, fy = (wpy - 7.5f) / 16.0f;
  const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
  const float ax = fx - ix, ay = fy - iy;
  Eco es[8];
  float ws[8];
  int n = 0;
  bool any = false;
  auto add = [&](Eco e, float w) {
    if (w <= 0) return;
    for (int k = 0; k < n; k++) if (es[k] == e) { ws[k] += w; return; }
    if (n < 8) { es[n] = e; ws[n] = w; n++; }
  };
  for (int c = 0; c < 4; c++) {
    const int x = ix + (c & 1), y = iy + (c >> 1);
    const float cw = ((c & 1) ? ax : 1 - ax) * ((c >> 1) ? ay : 1 - ay);
    const Eco own = ecoT(m, x, y);
    const uint8_t bl = m.ecoDerive ? derivedBlend(m, x, y) : m.blendAt(x, y);
    const int w = (bl >> 4) <= 8 ? bl >> 4 : 0;   // (M3 fixer: above 8 a settlement's paving mark, Map::PAVE_MARK)
    if (w > 0) {
      const Biome of = (Biome)(bl & 15);
      Eco nb = ecoNbT(m, x, y);
      // a map whose blend names another family but carries no eco layer: that family's classic eco
      if (nb == own && (int)of < (int)Biome::COUNT && of != ecoFamily(own)) nb = ecoOfFamily(of);
      if (nb != own && ecoGroundOf(ecoFamily(nb)) != Ground::Void) {
        any = true;
        add(own, cw * (1 - w / 16.0f));
        add(nb, cw * (w / 16.0f));
        continue;
      }
    }
    add(own, cw);
  }
  if (!any) return Ground::Void;
  // the threshold: broad patches, finer fringes, ordered dither only right at their edges
  // (M3c fixer round 2, review: "hard checkerboard grass decals on savanna ... edged in coarse 50% checker dither, like
  // spilled paint") the fringe is a wobbling line with only a 1-2 px dither seam (the ordered dither was a quarter of
  // the threshold's swing, so every patch edge was a band of checkerboard), and the patches are broader and softer
  float t = 0.5f + (vnoise(px / 12.0f, py / 12.0f, 861) - 0.5f) * 1.25f + (vnoise(px / 4.0f, py / 4.0f, 863) - 0.5f) * 0.34f + (bayer(px, py) - 0.5f) * 0.07f;
  t = std::clamp(t, 0.0f, 0.999f);
  // a stable order (by eco) so neighbouring pixels agree on which eco a threshold falls in
  for (int a = 1; a < n; a++)
    for (int b = a; b > 0 && (int)es[b] < (int)es[b - 1]; b--) { std::swap(es[b], es[b - 1]); std::swap(ws[b], ws[b - 1]); }
  float sum = 0;
  for (int k = 0; k < n; k++) sum += ws[k];
  float acc = 0;
  Eco pick = es[n - 1];
  for (int k = 0; k < n; k++) {
    acc += ws[k] / sum;
    if (t < acc) { pick = es[k]; break; }
  }
  const Ground pg = ecoGroundOf(ecoFamily(pick));
  if (pg == Ground::Void) return Ground::Void;
  outE = pick;
  // the pixel's own tile keeps its own variant of its biome's ground (a meadow stays a meadow)
  if (pick == ecoT(m, px >> 4, py >> 4) && ecoGround(g)) return g;
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
  // (M3c fixer round 2, review: "a flat random-tone mosaic") the heap's own light, before each block's tilt: the snow
  // and its shading follow it, so a cold crag is a snow-capped mass lit from the top-left, not a random patchwork
  const float litHeap = lit;
  // each block's tilt (toward or away from the light) and a gentle roundness within it (its upper left catches more)
  lit += (((ch >> 4) & 255) / 255.0f - 0.5f) * (cold > 0.2f ? 0.10f : 0.22f);
  lit += -((px + 0.5f - p1x) + (py + 0.5f - p1y)) / (float)G * (cold > 0.2f ? 0.32f : 0.5f);   // a rounded block: lit upper left, shaded lower right
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
    const float sv = D * 0.6f + (litHeap - 0.5f) * 0.5f + (vnoise(px / 7.0f, py / 7.0f, 945) - 0.5f) * 0.2f;
    const float sl = litHeap * 0.7f + lit * 0.3f + (bayer(px, py) - 0.5f) * 0.08f;
    if (sv > 0.80f - 0.30f * std::min(1.0f, cold) && gap >= 1.1f) c = sl < 0.40f ? C(176, 190, 218) : sl < 0.55f ? C(204, 214, 234) : sl < 0.70f ? C(228, 235, 246) : C(246, 249, 253);
    else if (c == P.dark && gap >= 1.1f) c = P.lo;   // bare rock among the snow: a tone off black
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

// M3c: the rock of a cliff, crag or massif by biome proper (lip, hi, mid, lo, dark): red banded sandstone in the
// badlands, white chalk under the downs, black basalt in the ash fields, blue ice on a glacier...
RockPal rockPalE(Eco e) {
  switch (e) {
    case Eco::Badlands: return {C(238, 172, 122), C(212, 126, 84), C(182, 98, 66), C(142, 72, 54), C(98, 50, 46), false};
    case Eco::PetrifiedForest: return {C(222, 182, 152), C(186, 140, 114), C(156, 112, 94), C(124, 88, 78), C(84, 60, 58), false};
    case Eco::ChalkDowns: return {C(250, 248, 240), C(228, 224, 210), C(202, 198, 184), C(166, 164, 154), C(112, 112, 112), false};
    case Eco::AshFields: return {C(126, 120, 126), C(84, 80, 88), C(62, 58, 66), C(44, 42, 50), C(26, 24, 32), false};
    case Eco::Glacier: case Eco::FrozenLakes: return {C(242, 250, 255), C(182, 214, 240), C(142, 182, 222), C(102, 142, 194), C(64, 94, 150), true};
    case Eco::CrystalBarrens: return {C(222, 212, 242), C(176, 162, 210), C(146, 130, 186), C(112, 98, 156), C(72, 62, 112), false};
    case Eco::Blight: return {C(148, 132, 144), C(112, 98, 112), C(90, 78, 92), C(68, 58, 72), C(42, 36, 48), false};
    case Eco::SeaCliffs: return {C(196, 200, 204), C(146, 152, 160), C(118, 124, 134), C(88, 94, 106), C(54, 58, 72), false};
    case Eco::Heath: case Eco::StonePlains: case Eco::Tundra:
      return {C(198, 192, 182), C(156, 152, 146), C(128, 124, 120), C(100, 96, 96), C(62, 60, 66), false};
    case Eco::Jungle: case Eco::GiantForest: case Eco::MushroomForest: case Eco::DarkForest:
      return {C(160, 160, 132), C(118, 122, 100), C(94, 98, 82), C(70, 74, 64), C(42, 46, 44), false};
    case Eco::Mountain: return rockPal(Biome::Plains);   // (a range's own grey, as before M3c)
    default: return rockPal(ecoFamily(e));
  }
}
// M3c: the climate of the land round a massif or crag tile, read from 4 tiles 6 out (cold: snow / taiga, hot: desert)
// and the rock it is made of (the mean of their biomes' rock), cached per tile (bakes run on worker threads)
struct ClimPal { float col[5][3]; float cold, hot; };
template <class TM>
const ClimPal& climPal(const TM& m, int x, int y) {
  struct CE { const void* map; int ox, oy, x, y; ClimPal v; };
  thread_local CE cache[64] = {};
  CE& e = cache[((unsigned)x * 7u + (unsigned)y * 13u) & 63u];
  if (e.map == (const void*)m.m && e.ox == m.ox && e.oy == m.oy && e.x == x && e.y == y) return e.v;
  ClimPal v{};
  static const int odx[4] = {6, -6, 0, 0}, ody[4] = {0, 0, 6, -6};
  for (int k = 0; k < 4; k++) {
    const Biome b = m.biomeAt(x + odx[k], y + ody[k]);
    if (b == Biome::Snow || b == Biome::Taiga) v.cold += 0.25f;
    else if (b == Biome::Desert) v.hot += 0.25f;
    const RockPal P = rockPalE(ecoT(m, x + odx[k], y + ody[k]));
    const uint32_t cs[5] = {P.lip, P.hi, P.mid, P.lo, P.dark};
    for (int i = 0; i < 5; i++) {
      v.col[i][0] += (float)(cs[i] & 255) * 0.25f;
      v.col[i][1] += (float)((cs[i] >> 8) & 255) * 0.25f;
      v.col[i][2] += (float)((cs[i] >> 16) & 255) * 0.25f;
    }
  }
  e.map = (const void*)m.m; e.ox = m.ox; e.oy = m.oy; e.x = x; e.y = y; e.v = v;
  return e.v;
}
// the water off a coral strand: within two tiles of one (cached per tile)
template <class TM>
bool coralNear(const TM& m, int x, int y) {
  struct CE { const void* map; int ox, oy, x, y; bool v; };
  thread_local CE cache[64] = {};
  CE& e = cache[((unsigned)x * 7u + (unsigned)y * 13u) & 63u];
  if (e.map == (const void*)m.m && e.ox == m.ox && e.oy == m.oy && e.x == x && e.y == y) return e.v;
  bool v = false;
  for (int oy = -2; oy <= 2 && !v; oy += 2)
    for (int ox = -2; ox <= 2 && !v; ox += 2) v = ecoT(m, x + ox, y + oy) == Eco::CoralCoast || ecoNbT(m, x + ox, y + oy) == Eco::CoralCoast;
  e.map = (const void*)m.m; e.ox = m.ox; e.oy = m.oy; e.x = x; e.y = y; e.v = v;
  return v;
}

// (M1) A mountain massif seen from above: the endless generator fills a range with solid Rock at the top relief
// levels, so its inside is one flat tile class. Paint it as a range: ridged noise gives crests and gullies, lit on
// their north-west slopes and shaded on the south-east ones (the 3/4 top-left light), quantised to a few rock tones
// with an ordered dither, strata along the contours, scree speckle, and snow on the crests and lit high slopes (more
// in the cold north, little in hot lands). The tile's climate comes from the land around the range.
template <class TM>
uint32_t mountainPixel(const TM& m, int px, int py, int sx, int sy) {
  // the climate of the land around (cold: snow / taiga, hot: desert) and (M3c) the rock its biomes are made of,
  // blended between tile centres so it never steps
  float cold = 0, hot = 0;
  RockPal P;
  {
    const float fx = (px - 7.5f) / 16.0f, fy = (py - 7.5f) / 16.0f;
    const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    const float ax = fx - ix, ay = fy - iy;
    const ClimPal* q[4] = {&climPal(m, ix, iy), &climPal(m, ix + 1, iy), &climPal(m, ix, iy + 1), &climPal(m, ix + 1, iy + 1)};
    const float w[4] = {(1 - ax) * (1 - ay), ax * (1 - ay), (1 - ax) * ay, ax * ay};
    float col[5][3] = {};
    for (int k = 0; k < 4; k++) {
      cold += q[k]->cold * w[k];
      hot += q[k]->hot * w[k];
      for (int i = 0; i < 5; i++)
        for (int j = 0; j < 3; j++) col[i][j] += q[k]->col[i][j] * w[k];
    }
    uint32_t cs[5];
    for (int i = 0; i < 5; i++) cs[i] = C((int)(col[i][0] + 0.5f), (int)(col[i][1] + 0.5f), (int)(col[i][2] + 0.5f));
    P = {cs[0], cs[1], cs[2], cs[3], cs[4], false};
  }
  auto ridge = [](float v) { v = 1.0f - std::fabs(v * 2.0f - 1.0f); return v * v; };
  auto H2 = [&](float x, float y) {   // the broad shape: crests and gullies (two octaves, warped and turned off the grid)
    const float wx = x + (vnoise(x / 61.0f, y / 61.0f, 913) - 0.5f) * 56.0f, wy = y + (vnoise(x / 61.0f, y / 61.0f, 915) - 0.5f) * 56.0f;
    const float u = wx * 0.8f - wy * 0.6f, v = wx * 0.6f + wy * 0.8f;
    return ridge(vnoise(wx / 92.0f, wy / 92.0f, 901)) * 0.7f + ridge(vnoise(u / 37.0f, v / 37.0f, 903)) * 0.3f;
  };
  // a crag stamped on the land (a cave's, a lair's: its tiles keep the land's biome, a range's rock is Mountain): an
  // outcrop of broken blocks (the massif's broad ridges smeared over a small heap)
  if (m.biomeAt(sx >> 4, sy >> 4) != Biome::Mountain) return outcropPixel(m, px, py, sx, sy, P, cold, hot);
  // (M3c fixer, review: "smooth airbrushed gradients, not pixel art") the massif is cut into facets: jittered cells
  // about 15 px across (squat in the 3/4 view), each one flat plane of rock lit by the broad ridge field's slope at its
  // centre plus its own tilt, so the crests and gullies read through clusters of hard-edged tones; neighbouring facets
  // part along a crack with a lit lip on the side toward the light and a dark edge away from it (some pairs are one
  // rock, fused, so no cobble grid shows). Snow lies facet by facet, its rim broken along the cracks.
  const int G = 19;
  const int gx = (int)std::floor(px / (float)G), gy = (int)std::floor(py / (float)G);
  float d1 = 1e9f, d2 = 1e9f;
  int c1x = 0, c1y = 0;
  float p1x = 0, p1y = 0, p2x = 0, p2y = 0;
  for (int oy = -1; oy <= 1; oy++)
    for (int ox = -1; ox <= 1; ox++) {
      const int cx = gx + ox, cy = gy + oy;
      const uint32_t hh = hash2(cx, cy, 961);
      const float fx = cx * (float)G + 1.5f + (hh & 255) / 255.0f * (G - 3), fy = cy * (float)G + 1.5f + ((hh >> 8) & 255) / 255.0f * (G - 3);
      const float d = std::sqrt((px + 0.5f - fx) * (px + 0.5f - fx) + (py + 0.5f - fy) * (py + 0.5f - fy) * 1.45f);
      if (d < d1) { d2 = d1; p2x = p1x; p2y = p1y; d1 = d; c1x = cx; c1y = cy; p1x = fx; p1y = fy; }
      else if (d < d2) { d2 = d; p2x = fx; p2y = fy; }
    }
  const uint32_t ch = hash2(c1x, c1y, 967);
  const float hc = H2(p1x, p1y);
  const float slope = H2(p1x + 3.0f, p1y + 3.0f) - H2(p1x - 3.0f, p1y - 3.0f);
  // (M3c fixer round 2, review: "mountain rock masses in cold lands look like a flat random-tone mosaic, with no light
  // and no cliff depth") the light is first the MASSIF's: how far the rock runs toward the light (up-left) against away
  // from it (down-right), so the whole mass is lit on its north-west flank and falls into shade on its south-east one
  // (as the outcrops are); the ridges' slope only modulates that (its gain was so high that neighbouring facets jumped
  // from the lip to the darkest step), and the facet's own tilt is a whisper
  float side = 0;
  {
    auto rockPx = [&](int qx, int qy) { return m.at(qx >> 4, qy >> 4) == Ground::Rock; };
    float hUL = 0, hDR = 0;
    for (int k = 1; k <= 14; k++) { if (!rockPx(px - k * 3, py - k * 2)) break; hUL = (float)k; }
    for (int k = 1; k <= 14; k++) { if (!rockPx(px + k * 3, py + k * 2)) break; hDR = (float)k; }
    side = (hDR - hUL) / (hDR + hUL + 2.0f);   // +1 the lit north-west flank .. -1 the shaded south-east
  }
  const float litBase = 0.52f + std::clamp(slope * 4.0f, -0.30f, 0.30f) + (hc - 0.5f) * 0.20f + side * 0.30f;
  float lit = litBase;
  lit += (((ch >> 4) & 255) / 255.0f - 0.5f) * 0.06f;                         // the facet's own tilt
  lit += -((px + 0.5f - p1x) + (py + 0.5f - p1y)) / (float)G * 0.22f;          // a turn across it (lit up-left)
  float gap = d2 - d1;
  const bool fused = ((ch ^ hash2((int)std::floor(p2x), (int)std::floor(p2y), 971)) & 3) <= 1;
  if (fused) gap += 3.0f;
  const float toward = (p2x - p1x) + (p2y - p1y);   // < 0: the crack lies up-left of the pixel (the facet's lit lip)
  if (gap < 2.4f && gap >= 1.0f) lit += toward < 0 ? 0.24f : -0.18f;
  lit += (bayer(px, py) - 0.5f) * 0.04f;
  const float h0 = hc;
  uint32_t c;
  if (gap < 1.0f && toward > 0) c = lit < 0.5f ? P.dark : P.lo;   // the crack: dark on a facet's shaded lower-right edge only
  else if (lit < 0.24f) c = P.dark;
  else if (lit < 0.42f) c = P.lo;
  else if (lit < 0.60f) c = P.mid;
  else if (lit < 0.80f) c = P.hi;
  else c = P.lip;
  // strata: a darker band across some facets along the contours
  if (gap >= 1.0f && ((ch >> 12) & 7) == 0) {
    const float band = (py + 0.5f - p1y) + (px + 0.5f - p1x) * 0.35f;
    if (band > 1.0f && band < 2.6f) c = c == P.lip ? P.hi : c == P.hi ? P.mid : c == P.mid ? P.lo : P.dark;
  }
  // scree and chips
  const float sp = hashf(px, py, 909);
  if (gap >= 1.0f && sp < 0.025f) c = mul(c, 0.80f);
  // snow on the crests and the lit high slopes, facet by facet
  float snowAt = 0.66f - 0.40f * cold + 1.0f * hot;
  // (fixer r2) the snow follows the crests and the lit flank (the facet's random tilt no longer decides it, which made
  // a random black-and-white patchwork), and it is shaded by the massif's light: white on the north-west flank, a cool
  // blue shade on the south-east one, so the snowfield shows the mountain's volume
  const float sv = h0 + (litBase - 0.5f) * 0.30f + (((ch >> 20) & 255) / 255.0f - 0.5f) * 0.04f;
  // a crag or knoll down on the low land (a cave's rock) holds no snow outside the cold north
  if (m.heightAt(sx >> 4, sy >> 4) < 5) snowAt += 0.5f * (1.0f - std::min(1.0f, cold));
  if (sv > snowAt) {
    // the snow lies smooth over the facets (a crack net drawn through it read as stained glass): only the massif's
    // light and a soft turn, and a blue crack only where a few facets' shaded edges break through
    const float sl = litBase - ((px + 0.5f - p1x) + (py + 0.5f - p1y)) / (float)G * 0.06f + (bayer(px, py) - 0.5f) * 0.10f;
    if (gap < 1.0f && toward > 0 && (ch & 3) == 0) c = C(178, 190, 216);
    else c = sl < 0.38f ? C(168, 182, 212) : sl < 0.52f ? C(196, 208, 230) : sl < 0.68f ? C(224, 232, 244) : C(246, 249, 253);
  } else if (cold > 0.3f && c == P.dark && gap >= 1.0f) {
    c = P.lo;   // bare rock among the snow: its shade stays a tone off black (the near-black facets read as holes)
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
  // (fixer r2) 10-14 px tall (the mass's thickness), its height wandering, cut by fissures into columns lit on their
  // west side, the snow's rim hanging over the lip in the cold
  const int FHm = 10 + (int)(vnoise(px / 8.0f, 1.5f, 919) * 5.0f);
  for (int k = 1; k <= FHm; k++) {
    const int qy = (sy + k) >> 4;
    if (qy == (sy >> 4)) continue;
    const Ground below = m.at(ftx, qy);
    if (below == Ground::Rock || m.heightAt(ftx, qy) < lv) break;
    const float t = (FHm - k) / (float)(FHm - 1);   // 0 at the lip .. 1 at the foot
    const int cw = 4 + (int)(hash2(px / 5, 1, 921) % 3), cxp = ((px % cw) + cw) % cw;
    float l = 0.82f - t * 0.70f + (((hash2(px / cw, qy, 917) >> 8) & 255) / 255.0f - 0.5f) * 0.16f + (bayer(px, k) - 0.5f) * 0.06f;
    if (cxp == 0) l -= 0.26f;          // a fissure
    else if (cxp == 1) l += 0.12f;     // a column's lit west edge
    uint32_t f = l > 0.70f ? P.hi : l > 0.48f ? P.mid : l > 0.26f ? P.lo : P.dark;
    if (k == FHm) f = cold > 0.4f ? C(240, 245, 252) : P.lip;
    else if (k == FHm - 1) f = cold > 0.4f ? C(196, 208, 230) : lerpc(f, P.lip, 0.5f);
    else if (k == 1) f = mul(P.dark, 0.85f);
    c = f;
    break;
  }
  return c;
}

// ---- (M3 fixer) the cultures' paving (cult::TownStyle::paving, written on a settlement's tiles as Map::PAVE_MARK).
// Every people lays its squares and streets in its own material, so the first view of a capital is its own: sun-baked
// brick for the dune folk, beaten earth for the steppe and the highlands, plank decks in the marsh, moss-grown flags
// under the elves, pale dressed stone in the star cities, fired terracotta round the sun temples, blue-grey brick in a
// basket weave in the jade kingdoms, red brick along the rivers, travertine slabs and basalt roads in the empire.
// 0 (heartland cobbles) keeps the classic look. Pure function of global pixels (bakes run on worker threads).
inline int fmod_(int a, int b) { return ((a % b) + b) % b; }
inline int fdiv_(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
// ---- (M3 fixer) a stilt town's boardwalks (Map::BOARDWALK_MARK on its Bridge tiles over the marsh): one walkway of
// square plank platforms on posts. Every platform of a stretch lays its planks the same way (the way the boardwalk
// runs most, read over a 5x5 window, so a diagonal staircase of platforms keeps one direction), an edge beam runs
// along every side open to the marsh with a post head at each corner, and a plank end where it lands on dry ground.
// Its fascia, posts and shadow fall on the marsh south and east of it (boardwalkUnder).
template <class TM>
bool isBoardwalk(const TM& m, int x, int y) { return m.at(x, y) == Ground::Bridge && (m.blendAt(x, y) >> 4) == (Map::BOARDWALK_MARK >> 4); }
template <class TM>
uint32_t boardwalkPixel(const TM& m, int px, int py) {
  const int tx = px >> 4, ty = py >> 4, lx = px & 15, ly = py & 15;
  int hl = 0, vl = 0;
  for (int oy = -2; oy <= 2; oy++)
    for (int ox = -2; ox <= 2; ox++) {
      if (!isBoardwalk(m, tx + ox, ty + oy)) continue;
      if (isBoardwalk(m, tx + ox + 1, ty + oy)) hl++;
      if (isBoardwalk(m, tx + ox, ty + oy + 1)) vl++;
    }
  const bool eastWest = hl >= vl;   // the way you walk: planks lie across it
  auto wetAt = [&](int x, int y) { const Ground q = m.at(x, y); return groundWater(q) || q == Ground::Swamp || q == Ground::Void; };
  const int a = eastWest ? px : py, along = eastWest ? py : px;
  const int row = ((a >> 2) % 4096 + 4096) % 4096;
  const int joff = (int)(hash2(row, 0, 1411) % 16u);
  const bool gap = (a & 3) == 0, butt = ((along + joff) & 15) == 0;
  uint32_t c = gap ? C(58, 42, 30) : lerpc(C(124, 96, 64), C(150, 118, 80), hashf(row, (along + joff) >> 4, 1413));
  if (!gap && (a & 3) == 1) c = mul(c, 1.08f);   // each plank's lit edge
  if (butt && !gap) c = C(84, 62, 42);
  if (!gap && vnoise(px / 9.0f, py / 9.0f, 1415) > 0.68f) c = lerpc(c, C(132, 132, 116), 0.3f);   // silvered by the damp
  // the open sides: an edge beam (dark outer line, lit top), a post head at each corner
  const bool oN = wetAt(tx, ty - 1), oS = wetAt(tx, ty + 1), oW = wetAt(tx - 1, ty), oE = wetAt(tx + 1, ty);
  auto beam = [&](int d) -> uint32_t { return d == 0 ? C(56, 40, 30) : (d == 1 ? C(168, 130, 86) : C(112, 82, 54)); };
  int d = 99;
  if (oN) d = std::min(d, ly);
  if (oS) d = std::min(d, 15 - ly);
  if (oW) d = std::min(d, lx);
  if (oE) d = std::min(d, 15 - lx);
  if (d <= 2) c = beam(d);
  const bool cornerX = lx <= 2 || lx >= 13, cornerY = ly <= 2 || ly >= 13;
  if (cornerX && cornerY && ((lx <= 2 ? oW : oE) || (ly <= 2 ? oN : oS))) {
    const int cx = lx <= 2 ? 1 : 14, cy = ly <= 2 ? 1 : 14;
    const int dx = lx - cx, dy = ly - cy;
    if (dx * dx + dy * dy <= 2) c = (dx <= 0 && dy <= 0) ? C(150, 112, 72) : C(70, 50, 34);   // the post's top, lit up-left
  }
  // where it lands on dry ground: the last plank ends in a dark joint against the bank
  auto landAt = [&](int x, int y) { const Ground q = m.at(x, y); return q != Ground::Bridge && !wetAt(x, y) && !groundSolid(q); };
  if ((eastWest && ((landAt(tx - 1, ty) && lx == 0) || (landAt(tx + 1, ty) && lx == 15))) ||
      (!eastWest && ((landAt(tx, ty - 1) && ly == 0) || (landAt(tx, ty + 1) && ly == 15))))
    c = C(70, 52, 36);
  return c;
}
// under a boardwalk: on the marsh just south of a platform its fascia and posts, and its shadow down-right (0: none)
template <class TM>
uint32_t boardwalkUnder(const TM& m, int px, int py, uint32_t c) {
  const int tx = px >> 4, ty = py >> 4, lx = px & 15, ly = py & 15;
  if (isBoardwalk(m, tx, ty - 1)) {
    if (ly <= 2) return ly == 0 ? C(110, 80, 52) : (ly == 1 ? C(84, 60, 40) : C(60, 44, 32));   // the deck's south face
    const int col = lx;
    if ((col == 1 || col == 2 || col == 13 || col == 14) && ly <= 8) {   // the posts going down into the marsh
      if (ly == 8) return C(150, 180, 170);                                // a ripple at the foot
      return col == 1 || col == 13 ? C(108, 80, 54) : C(66, 48, 34);
    }
    if (ly <= 6) return mul(c, 0.62f);                                     // in the deck's shade
  }
  if (isBoardwalk(m, tx - 1, ty) && lx <= 3 && !isBoardwalk(m, tx, ty)) return mul(c, 0.72f);   // its shadow to the east
  if (isBoardwalk(m, tx - 1, ty - 1) && lx <= 3 && ly <= 6) return mul(c, 0.75f);
  return 0;
}

// ---- (M3b fixer) a seat's formal water (Map::POOL_MARK on the pools, canals and rills its grounds lay by hand): a
// sunk basin kerbed in cut stone of the people's paving, never a pond's organic bank. Seen in the high 3/4 view with
// the light from the top left: the kerb's top lit along its outer edge, a dark joint every few stones; on the far
// (north) side the inner face of the basin shows below the kerb, in shade; the kerbs to the north and west cast their
// shadow down-right onto the water; the water itself deep and still, the sky glinting in it along the near side.
template <class TM>
bool isPoolT(const TM& m, int x, int y) { return groundWater(m.at(x, y)) && (m.blendAt(x, y) >> 4) == (Map::POOL_MARK >> 4); }
template <class TM>
uint32_t poolPixel(const TM& m, int px, int py) {
  const int tx = px >> 4, ty = py >> 4, lx = px & 15, ly = py & 15;
  // the kerb's stone by the people's paving: lit top, mid, shade, joint
  static const uint8_t pal[10][3] = {
      {178, 172, 160}, {170, 168, 160}, {176, 160, 128}, {214, 186, 136}, {150, 120, 84},
      {150, 156, 140}, {226, 222, 210}, {196, 132, 92}, {120, 128, 140}, {176, 96, 72}};
  const int pv = std::clamp(m.blendAt(tx, ty) & 15, 0, 9);
  const uint32_t lit = C(pal[pv][0] + 22, pal[pv][1] + 22, pal[pv][2] + 20), mid = C(pal[pv][0], pal[pv][1], pal[pv][2]);
  const uint32_t shade = mul(mid, 0.66f), joint = mul(mid, 0.48f);
  auto open = [&](int x, int y) { return !isPoolT(m, x, y) && m.at(x, y) != Ground::Bridge; };
  const bool oN = open(tx, ty - 1), oS = open(tx, ty + 1), oW = open(tx - 1, ty), oE = open(tx + 1, ty);
  const int K = 3;   // the kerb's width
  int dN = oN ? ly : 99, dS = oS ? 15 - ly : 99, dW = oW ? lx : 99, dE = oE ? 15 - lx : 99;
  // the inner corners (the basin turns where both sides run on but the diagonal is land)
  if (!oN && !oW && open(tx - 1, ty - 1) && lx < K && ly < K) { dN = ly; dW = lx; }
  if (!oN && !oE && open(tx + 1, ty - 1) && lx > 15 - K && ly < K) { dN = ly; dE = 15 - lx; }
  if (!oS && !oW && open(tx - 1, ty + 1) && lx < K && ly > 15 - K) { dS = 15 - ly; dW = lx; }
  if (!oS && !oE && open(tx + 1, ty + 1) && lx > 15 - K && ly > 15 - K) { dS = 15 - ly; dE = 15 - lx; }
  const int d = std::min(std::min(dN, dS), std::min(dW, dE));
  if (d < K) {
    const bool alongX = (d == dN || d == dS);
    const int run = alongX ? px : py;
    if (((run + (alongX ? ty * 5 : tx * 5)) % 11 + 11) % 11 == 0) return joint;   // the joints between the stones
    if (d == 0) return (d == dN || d == dW) ? lit : shade;    // the outer arris: lit on the sides the light falls on
    if (d == K - 1) return (d == dS || d == dE) ? lit : mid;  // the lip over the water
    return hashf(px >> 1, py >> 1, 2203) < 0.12f ? mul(mid, 0.92f) : mid;
  }
  // the far side's inner face below its kerb (seen: it faces the viewer), in shade, with one course line
  if (dN < K + 3) return dN == K + 2 ? mul(shade, 0.8f) : (dN == K + 1 && ((px / 7) & 1) ? mul(shade, 0.9f) : shade);
  // the water: deep and still; darker in the kerbs' shadow (north and west), the sky glinting along the near side
  uint32_t w = C(38, 92, 112, 214);
  const bool shadowed = dN < K + 6 || dW < K + 3;
  if (shadowed) w = C(26, 64, 84, 222);
  if (!shadowed && dS < 5 && ((px + py / 2) % 9) < 4) w = C(76, 140, 160, 206);   // the sky in it, toward the viewer
  const float g = hashf(px / 4, py, 2207);
  if (!shadowed && g < 0.05f) w = C(168, 214, 226, 220);                                // a glint on the stillness
  return w;
}

// the paving mark of a tile (-1: none, the classic look); a boardwalk mark reads as its culture's paving too
template <class TM>
int paveMatAt(const TM& m, int tx, int ty) {
  const uint8_t b = m.blendAt(tx, ty);
  const int hi = b >> 4;
  if (hi == (Map::PAVE_MARK >> 4) || hi == (Map::BOARDWALK_MARK >> 4)) return b & 15;
  return -1;
}
// the paving at a pixel: its tile's, looked up through a gentle warp so where a town's streets meet the plain roads of
// the land the materials part along a wobbling line, not a tile edge
template <class TM>
int paveMatPx(const TM& m, int px, int py) {
  const float wx = px + (vnoise(px / 9.0f, py / 9.0f, 1401) - 0.5f) * 12.0f, wy = py + (vnoise(px / 9.0f, py / 9.0f, 1403) - 0.5f) * 12.0f;
  const int t = paveMatAt(m, (int)std::floor(wx / 16), (int)std::floor(wy / 16));
  return t >= 0 ? t : paveMatAt(m, px >> 4, py >> 4);
}
// ---- (M3b forts) the bonds the cultures lay their paving in (bld::paveBond): never a visible regular grid at 1x (owner).
// A jittered cell pattern (Voronoi) for crazy paving and hexagonal flags: the stone's id, how far the pixel is from the
// nearest joint (px), and which side of its stone's centre it lies on (for the top-left bevel)
struct PaveCell { uint32_t id = 0; float edge = 99.0f; float dx = 0, dy = 0; };
PaveCell paveCell(int px, int py, int cs, float jitter, uint32_t seed, bool hex) {
  PaveCell r;
  const int gy = fdiv_(py, cs);
  float best = 1e9f, second = 1e9f;
  for (int oy = -1; oy <= 1; oy++) {
    const int cy = gy + oy;
    const int shift = hex ? (cy & 1) * cs / 2 : 0;
    const int gx = fdiv_(px - shift, cs);
    for (int ox = -1; ox <= 1; ox++) {
      const int cx = gx + ox;
      const float fx = cx * cs + shift + cs * 0.5f + (hashf(cx, cy, seed) - 0.5f) * jitter * cs;
      const float fy = cy * cs + cs * 0.5f + (hashf(cx, cy, seed + 1) - 0.5f) * jitter * cs;
      const float ddx = px + 0.5f - fx, ddy = py + 0.5f - fy, d = ddx * ddx + ddy * ddy;
      if (d < best) { second = best; best = d; r.id = hash2(cx, cy, seed + 2); r.dx = ddx; r.dy = ddy; }
      else if (d < second) second = d;
    }
  }
  r.edge = std::sqrt(second) - std::sqrt(best);
  return r;
}
// stones along a course with jittered joints (constant time): the pixel's place in its stone, the stone's length
struct Ashlar { uint32_t id = 0; int in = 0, iy = 0, sw = 1, rh = 1; };
void jointsAlong(Ashlar& a, int u, int course, uint32_t seed, int P, int jit) {
  const int v = u + (int)(hash2(course, 7, seed) % (uint32_t)P);
  auto jx = [&](int kk) { return kk * P + (int)(hash2(course, kk, seed + 3) % (uint32_t)(2 * jit + 1)) - jit; };
  int k = fdiv_(v, P), lo = jx(k), hi = jx(k + 1);
  if (v < lo) { hi = lo; k--; lo = jx(k); }
  else if (v >= hi) { k++; lo = hi; hi = jx(k + 1); }
  a.in = v - lo;
  a.sw = std::max(2, hi - lo);
  a.id = hash2(course, k, seed + 5);
}
// irregular ashlar: courses of 6..11 px (four to a 34 px band, each band its own heights), stones about P px long
Ashlar ashlarAt(int px, int py, uint32_t seed, int P, int jit) {
  Ashlar a;
  const int band = fdiv_(py, 34), by = fmod_(py, 34);
  int h[4], sum = 0;
  for (int i = 0; i < 4; i++) { h[i] = 6 + (int)(hash2(band, i, seed) % 5u); sum += h[i]; }
  h[3] += 34 - sum;
  int row = 0, y0 = 0;
  while (row < 3 && by >= y0 + h[row]) { y0 += h[row]; row++; }
  a.rh = std::max(3, h[row]);
  a.iy = by - y0;
  jointsAlong(a, px, band * 4 + row, seed, P, jit);
  return a;
}
// running bond with bricks of varying length (a street's setts and bricks): course height rh
Ashlar runningAt(int px, int py, uint32_t seed, int rh, int P, int jit) {
  Ashlar a;
  a.rh = rh;
  a.iy = fmod_(py, rh);
  jointsAlong(a, px, fdiv_(py, rh), seed, P, jit);
  return a;
}
// a kerb stone along a street's edge (e: how close to the edge, 0..1): long dressed stones with a lit top and a dark
// foot in the stone col. 0: not on the kerb
uint32_t kerbPixel(int px, int py, float e, uint32_t col) {
  if (e < 0.80f) return 0;
  uint32_t c = mul(col, 0.94f + hashf(fdiv_(px + py * 3, 11), 3, 1441) * 0.12f);
  if (e > 0.95f) c = mul(col, 0.70f);                 // its outer edge: the joint with the ground
  else if (e < 0.84f) c = mul(col, 1.08f);            // its lit inner arris
  if (fmod_(px + py * 3, 11) == 0) c = mul(col, 0.80f);   // the joints between the stones
  return c;
}
// herringbone: bricks of two cells (cs px) at right angles in a zigzag (the river towns' squares)
struct Herring { uint32_t id = 0; bool joint = false; int bev = 0; };
Herring herringAt(int px, int py, int cs, uint32_t seed) {
  const int i = fdiv_(px, cs), j = fdiv_(py, cs), lx = fmod_(px, cs), ly = fmod_(py, cs);
  static const int off[4][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 1}, {0, 2, 1}};   // the cell's offset in its brick, vertical
  for (int k = 0; k < 4; k++) {
    const int ii = i - off[k][0], jj = j - off[k][1];
    const int pp = ii + jj, qq = ii - jj;
    if ((pp & 1) || fmod_(qq, 4) != 0) continue;
    Herring r;
    r.id = hash2(pp, qq * 3 + off[k][2], seed);
    const bool vert = off[k][2] != 0, first = k == 0 || k == 2;
    r.joint = vert ? (lx == 0 || (first && ly == 0)) : (ly == 0 || (first && lx == 0));
    if (vert) r.bev = (lx == 1 || (first && ly == 1)) ? 1 : ((lx == cs - 1 || (!first && ly == cs - 1)) ? -1 : 0);
    else r.bev = (ly == 1 || (first && lx == 1)) ? 1 : ((ly == cs - 1 || (!first && lx == cs - 1)) ? -1 : 0);
    return r;
  }
  return Herring{};
}
// one pixel of paving in material mat (1..9). road: a street (narrower, worn at its edges: e is how close the pixel is
// to the edge, 0..1); else a square (paveV >= 0 near its rim). n: the broad tone field, h: a per-pixel hash.
uint32_t pavePixel(int mat, bool road, int px, int py, float e, float paveV, float h, float n) {
  uint32_t c = 0;
  const float broad = vnoise(px / 40.0f, py / 40.0f, 1227);
  const float wear = vnoise(px / 7.0f, py / 7.0f, 1211) * 0.8f + e * 0.75f;
  const bool rim = paveV >= 0 && paveV < 0.6f;
  switch (mat) {
    case 1: {
      if (road) {
        // the empire's roads: polygonal basalt blocks, tightly fitted, dark joints, kerbed with pale stone
        const int cs = 7;
        const int gx = fdiv_(px, cs), gy = fdiv_(py, cs);
        float best = 1e9f, second = 1e9f;
        uint32_t bid = 0;
        for (int oy = -1; oy <= 1; oy++)
          for (int ox = -1; ox <= 1; ox++) {
            const int cx = gx + ox, cy = gy + oy;
            const float fx = cx * cs + 1.5f + hashf(cx, cy, 1301) * (cs - 3), fy = cy * cs + 1.5f + hashf(cx, cy, 1303) * (cs - 3);
            const float d = (px + 0.5f - fx) * (px + 0.5f - fx) + (py + 0.5f - fy) * (py + 0.5f - fy);
            if (d < best) { second = best; best = d; bid = hash2(cx, cy, 1305); }
            else if (d < second) second = d;
          }
        const float edgeD = std::sqrt(second) - std::sqrt(best);
        const float t = (bid >> 8) * (1.0f / 16777216.0f);
        c = lerpc(C(112, 108, 106), C(140, 134, 128), t);   // (M3 fixer round 2: warmer, lighter basalt by the travertine)
        if (edgeD < 0.9f) c = C(76, 70, 66);
        else if (edgeD < 1.8f) c = mul(c, 1.10f);   // the block's worn, lit rim
        if (e > 0.62f) c = lerpc(C(196, 188, 168), C(214, 206, 186), hashf(fdiv_(px, 8), fdiv_(py, 8), 1307));   // kerb
        if (e > 0.62f && (fmod_(px, 8) == 0 || fmod_(py, 8) == 0)) c = C(150, 142, 124);
        if (wear > 0.86f) c = lerpc(c, C(150, 128, 96), 0.5f);
      } else {
        // the forum: big travertine slabs in irregular courses (M3b: no two courses alike), crisp joints, fine pitting,
        // a few slabs of darker marble
        const Ashlar sl = ashlarAt(px, py, 1311, 19, 6);
        const int in = sl.in, iy = sl.iy;
        const float tone = (sl.id >> 8) * (1.0f / 16777216.0f);
        c = lerpc(C(204, 192, 164), C(226, 216, 190), tone);
        if ((sl.id % 13u) == 0) c = lerpc(C(168, 172, 176), C(186, 190, 192), tone);   // a grey marble slab
        if (in == 0 || iy == 0) c = C(150, 138, 114);
        else if (iy == 1 || in == 1) c = mul(c, 1.05f);
        else if (iy == sl.rh - 1 || in == sl.sw - 1) c = mul(c, 0.94f);
        if (h < 0.05f && in > 0 && iy > 0) c = mul(c, 0.90f);   // pits in the stone
        c = mul(c, 0.95f + broad * 0.10f);
        if (rim) c = (in == 0 || iy == 0) ? C(132, 112, 86) : lerpc(c, C(186, 168, 134), 0.35f);
      }
      break;
    }
    case 2: {
      // beaten earth: warm tan, trodden darker in broad patches, small stones and grit, a stray tuft at the edges
      c = pick3(n, C(150, 120, 84), C(162, 132, 92), C(172, 142, 100));
      const float tr = vnoise(px / 14.0f, py / 14.0f, 1321);
      if (tr > 0.62f) c = mul(c, 0.90f);
      if (tr < 0.25f) c = lerpc(c, C(184, 158, 118), 0.4f);
      const uint32_t ph = hash2(fdiv_(px, 5), fdiv_(py, 5), 1323);
      const int sx = (int)(ph % 5u), sy = (int)((ph >> 3) % 5u);
      if ((ph >> 8) % 7u == 0 && fmod_(px, 5) == sx && fmod_(py, 5) == sy) c = C(196, 186, 168);           // a pebble
      if ((ph >> 8) % 7u == 0 && fmod_(px, 5) == sx && fmod_(py, 5) == sy + 1) c = C(110, 90, 68);         // its shade
      if (h < 0.04f) c = mul(c, 0.86f);
      if ((road ? e > 0.55f : rim) && vnoise(px / 4.0f, py / 4.0f, 1325) > 0.62f) c = lerpc(c, C(98, 132, 62), 0.6f);   // grass
      break;
    }
    case 3: {
      // the dune folk (M3b): sun-baked brick along the streets in running courses of bricks of their own lengths, big
      // sandstone slabs on the squares in irregular courses; sandy joints, each stone lit on its upper left, sand
      // drifting in, the streets kerbed in pale sandstone
      const Ashlar sl = road ? runningAt(px, py, 1331, 4, 8, 2) : ashlarAt(px, py, 1331, 15, 5);
      const bool line = sl.in == 0 || sl.iy == 0;
      const float t = (sl.id >> 8) * (1.0f / 16777216.0f);
      c = line ? C(170, 130, 88) : lerpc(C(196, 146, 98), C(218, 172, 118), t);
      if (!line && (sl.in == 1 || sl.iy == 1)) c = mul(c, 1.06f);
      else if (!line && (sl.in == sl.sw - 1 || sl.iy == sl.rh - 1)) c = mul(c, 0.92f);
      if (!line && !road && (sl.id % 17u) == 0 && ((px - py) & 3) == 0) c = mul(c, 0.8f);   // a crack
      if (!line && h < 0.05f) c = mul(c, 0.93f);
      c = mul(c, 0.95f + broad * 0.10f);
      const float sand = vnoise(px / 9.0f, py / 9.0f, 1333) + (road ? e * 0.5f : (rim ? 0.4f : 0.0f));
      if (sand > 0.80f) c = lerpc(c, C(226, 200, 150), 0.75f);   // drifted sand
      if (road) if (const uint32_t k = kerbPixel(px, py, e, C(222, 196, 150))) c = k;
      break;
    }
    case 4: {
      // a plank deck laid on the ground: planks east-west, staggered butt joints, nail heads, weathered grey-brown
      const int row = fdiv_(py, 4), joff = (int)(hash2(row, 0, 1341) % 24);
      const bool gap = fmod_(py, 4) == 0, butt = fmod_(px + joff, 24) == 0;
      const float t = hashf(fdiv_(px + joff, 24), row, 1343);
      c = gap ? C(62, 48, 36) : lerpc(C(128, 104, 76), C(152, 126, 92), t);
      // (fixer r2) not hundreds of identical boards: a few newer, paler boards let in, some dark tarred or rain-soaked
      // ones, and broad patches of weathering across the deck
      if (!gap) {
        const uint32_t bh = hash2(fdiv_(px + joff, 24), row, 1347);
        if (bh % 23u == 0) c = lerpc(C(176, 146, 104), C(190, 160, 116), t);
        else if (bh % 29u == 1) c = lerpc(C(92, 74, 56), C(106, 86, 64), t);
        c = mul(c, 0.92f + vnoise(px / 30.0f, py / 22.0f, 1349) * 0.16f);
      }
      if (butt) c = C(78, 60, 44);
      if (!gap && fmod_(py, 4) == 1) c = mul(c, 1.08f);          // the plank's lit edge
      if (!gap && (fmod_(px + joff, 24) == 2 || fmod_(px + joff, 24) == 21) && fmod_(py, 4) == 2) c = C(70, 66, 64);   // nails
      if (!gap && vnoise(px / 10.0f, py / 3.0f, 1345) > 0.7f) c = lerpc(c, C(140, 140, 128), 0.35f);   // silvered by rain
      if ((road ? e > 0.7f : rim) && fmod_(px + py, 7) == 0) c = mul(c, 0.82f);
      break;
    }
    case 5: {
      // the elves' moss-grown flags: rounded irregular stones, deep moss in every joint, moss creeping over some stones
      const int cs = 9;
      const int gx = fdiv_(px, cs), gy = fdiv_(py, cs);
      float best = 1e9f, second = 1e9f;
      uint32_t bid = 0;
      int bcx = 0, bcy = 0;
      for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++) {
          const int cx = gx + ox, cy = gy + oy;
          const float fx = cx * cs + 2.0f + hashf(cx, cy, 1351) * (cs - 4), fy = cy * cs + 2.0f + hashf(cx, cy, 1353) * (cs - 4);
          const float d = (px + 0.5f - fx) * (px + 0.5f - fx) + (py + 0.5f - fy) * (py + 0.5f - fy);
          if (d < best) { second = best; best = d; bid = hash2(cx, cy, 1355); bcx = (int)fx; bcy = (int)fy; }
          else if (d < second) second = d;
        }
      const float edgeD = std::sqrt(second) - std::sqrt(best);
      const float t = (bid >> 8) * (1.0f / 16777216.0f);
      const float mossy = vnoise(px / 12.0f, py / 12.0f, 1357) + (road ? e * 0.7f : (rim ? 0.35f : 0.0f));
      if (edgeD < (mossy > 0.55f ? 2.6f : 1.6f)) {
        c = lerpc(C(58, 92, 44), C(84, 122, 56), hashf(px, py, 1359));   // the moss in the joints
        if (h < 0.012f) c = C(232, 228, 160);                             // a tiny pale flower
      } else {
        c = lerpc(C(146, 152, 136), C(172, 176, 160), t);
        if (px < bcx && py < bcy) c = mul(c, 1.06f);                      // lit on the stone's upper left
        else if (px > bcx + 1 && py > bcy + 1) c = mul(c, 0.92f);
        if (mossy > 0.78f) c = lerpc(c, C(92, 128, 60), 0.6f);           // moss over the stone
      }
      break;
    }
    case 6: {
      // the star cities: pale dressed limestone. (M3b) The squares in hexagonal flags, each its own cool tone with a
      // lit upper-left bevel and a shaded foot, fine grey joints with frost settled in them, a blue star inlaid in one
      // in thirteen; the streets in setts of their own lengths, kerbed in white stone
      if (road) {
        const Ashlar sl = runningAt(px, py, 1361, 6, 9, 2);
        const bool line = sl.in == 0 || sl.iy == 0;
        const float t = (sl.id >> 8) * (1.0f / 16777216.0f);
        c = line ? C(150, 150, 156) : lerpc(C(198, 198, 202), C(226, 224, 224), t);
        if (!line && (sl.iy == 1 || sl.in == 1)) c = mul(c, 1.04f);
        else if (!line && (sl.iy == sl.rh - 1 || sl.in == sl.sw - 1)) c = mul(c, 0.90f);
        if (line && vnoise(px / 8.0f, py / 8.0f, 1365) > 0.58f) c = C(230, 234, 242);   // frost in the joint
      } else {
        const PaveCell sl = paveCell(px, py, 11, 0.55f, 1361, true);
        const float t = (sl.id >> 8) * (1.0f / 16777216.0f);
        if (sl.edge < 1.0f) {
          c = C(150, 150, 156);
          if (vnoise(px / 8.0f, py / 8.0f, 1365) > 0.58f) c = C(230, 234, 242);
        } else {
          c = lerpc(C(198, 198, 202), C(226, 224, 224), t);
          if ((sl.id % 9u) == 0) c = lerpc(C(178, 186, 200), C(194, 200, 212), t);   // a blue-grey stone
          if (sl.edge < 2.0f) c = mul(c, (sl.dx + sl.dy) < 0 ? 1.05f : 0.90f);      // the bevel
          if (h < 0.04f) c = mul(c, 0.93f);
          if (sl.id % 31u == 0) {   // the inlaid star
            const int dx = std::abs((int)std::lround(sl.dx)), dy = std::abs((int)std::lround(sl.dy));
            if (dx + dy <= 1) c = C(214, 222, 240);
            else if ((dx == 0 && dy <= 3) || (dy == 0 && dx <= 3)) c = C(84, 112, 176);
            else if (dx == 1 && dy == 1) c = C(110, 136, 192);
          }
        }
      }
      c = mul(c, 0.95f + broad * 0.09f);
      if (wear > 0.84f) c = lerpc(c, C(176, 178, 186), 0.35f);   // trodden smooth and grey
      if (!road && rim) c = lerpc(c, C(232, 236, 244), 0.45f);   // snow drifting in at the edges
      if (road) if (const uint32_t k = kerbPixel(px, py, e, C(232, 234, 240))) c = k;
      break;
    }
    case 7: {
      // the sun temples' fired terracotta (M3b, owner: "the sun-temple paving still reads as a regular grid at 1x"):
      // the squares in crazy paving of big irregular terracotta flags, no two alike, bedded in lime; sun-bleached in
      // broad drifts, flags lost where the bedding shows through, a turquoise- or ochre-glazed flag here and there; the
      // streets in running courses of tiles of their own lengths, kerbed in limestone; the squares' rims a kerb of
      // carved limestone (a step-fret)
      if (!road && rim) {
        const int k = fmod_(px + py / 4, 12), q = fmod_(py, 6);
        c = (q == 0 || q == 5) ? C(156, 140, 112) : C(206, 194, 166);
        if (q >= 2 && q <= 3 && (k == 2 || k == 3 || k == 8 || k == 9)) c = C(180, 162, 130);
        if (q == 1) c = mul(c, 1.04f);
        if (h < 0.05f) c = mul(c, 0.92f);
        break;
      }
      bool line;
      float t;
      uint32_t id;
      int bev = 0;
      if (road) {
        const Ashlar sl = runningAt(px, py, 1371, 5, 7, 2);
        line = sl.in == 0 || sl.iy == 0;
        t = (sl.id >> 8) * (1.0f / 16777216.0f);
        id = sl.id;
        bev = (sl.in == 1 || sl.iy == 1) ? 1 : ((sl.in == sl.sw - 1 || sl.iy == sl.rh - 1) ? -1 : 0);
      } else {
        const PaveCell sl = paveCell(px, py, 10, 0.80f, 1371, false);
        line = sl.edge < 1.0f;
        t = (sl.id >> 8) * (1.0f / 16777216.0f);
        id = sl.id;
        bev = sl.edge < 2.2f ? ((sl.dx + sl.dy) < 0 ? 1 : -1) : 0;
      }
      c = line ? C(166, 128, 96) : lerpc(C(150, 80, 56), C(186, 108, 72), t);
      if (!line) {
        c = mul(c, 0.88f + broad * 0.22f);                                                     // trodden in broad patches
        const float bleach = vnoise(px / 56.0f, py / 56.0f, 1377);
        if (bleach > 0.55f) c = lerpc(c, C(204, 150, 104), (bleach - 0.55f) * 1.1f);          // sun-bleached drifts
        if (bev > 0) c = mul(c, 1.07f);
        else if (bev < 0) c = mul(c, 0.92f);
        if (h < 0.04f) c = mul(c, 0.9f);
        if (!road && id % 61u == 0) c = lerpc(C(70, 120, 112), C(88, 140, 128), t);           // a turquoise-glazed flag, worn
        else if (!road && id % 89u == 0) c = lerpc(C(190, 150, 84), C(204, 168, 100), t);     // an ochre one
      }
      // flags lost: the lime bedding shows through, gritty, the broken edges round it darker
      const float lost = vnoise(px / 11.0f, py / 11.0f, 1375) + (road ? e * 0.25f : 0.0f);
      if (lost > 0.87f) c = lerpc(C(170, 146, 112), C(188, 164, 128), h);
      else if (lost > 0.845f) c = mul(c, 0.84f);
      if (road && wear > 0.80f) c = lerpc(c, C(176, 140, 100), 0.6f);
      if (road) if (const uint32_t k = kerbPixel(px, py, e, C(206, 194, 166))) c = k;
      break;
    }
    case 8: {
      // the jade kingdoms: blue-grey fired brick. (M3b) The squares in a basket weave whose rows of squares are set off
      // against each other (no line runs on across the square), every brick its own firing, the joints only a shade
      // darker; the streets in running courses of bricks of their own lengths, kerbed in grey granite
      bool line;
      uint32_t id;
      if (road) {
        const Ashlar sl = runningAt(px, py, 1381, 4, 10, 2);
        line = sl.in == 0 || sl.iy == 0;
        id = sl.id;
      } else {
        const int row = fdiv_(py, 8), shift = (int)(hash2(row, 0, 1381) % 8u);
        const int u = px + shift, bx = fdiv_(u, 8), lx = fmod_(u, 8), ly = fmod_(py, 8);
        const bool hz = ((bx + row) & 1) == 0;
        line = lx == 0 || ly == 0 || (hz ? ly == 4 : lx == 4);
        id = hash2(bx * 2 + (hz ? (ly >= 4) : (lx >= 4)), row, 1385);
      }
      const float t = (id >> 8) * (1.0f / 16777216.0f);
      c = line ? C(94, 100, 110) : lerpc(C(114, 122, 132), C(146, 152, 158), t);
      if (!line && (id % 14u) == 0) c = mul(c, 0.86f);   // a darker, harder-fired brick here and there
      if (!line && h < 0.05f) c = mul(c, 0.9f);
      c = mul(c, 0.94f + broad * 0.12f);
      if (line && vnoise(px / 9.0f, py / 9.0f, 1383) > 0.72f) c = lerpc(c, C(70, 100, 62), 0.5f);   // moss in the joints
      if (road ? wear > 0.85f : rim) c = lerpc(c, C(120, 112, 98), 0.4f);
      if (road) if (const uint32_t k = kerbPixel(px, py, e, C(150, 154, 160))) c = k;
      break;
    }
    case 9: {
      // the river towns: red brick. (M3b) The squares in herringbone, the streets in running courses of bricks of their
      // own lengths, kerbed in grey stone; pale sandy joints, an overfired brick here and there
      bool line;
      uint32_t id;
      int bev = 0;
      if (road) {
        const Ashlar sl = runningAt(px, py, 1391, 4, 8, 1);
        line = sl.in == 0 || sl.iy == 0;
        id = sl.id;
        bev = sl.iy == 1 ? 1 : (sl.iy == sl.rh - 1 ? -1 : 0);
      } else {
        const Herring hb = herringAt(px, py, 4, 1391);
        line = hb.joint;
        id = hb.id;
        bev = hb.bev;
      }
      const float t = (id >> 8) * (1.0f / 16777216.0f);
      c = line ? C(176, 158, 132) : lerpc(C(146, 66, 52), C(176, 90, 66), t);
      if (!line && (id % 10u) == 0) c = mul(c, 0.82f);   // an overfired brick
      if (!line && bev) c = mul(c, bev > 0 ? 1.06f : 0.93f);
      if (!line && h < 0.04f) c = mul(c, 0.9f);
      c = mul(c, 0.95f + broad * 0.10f);
      if (road ? wear > 0.82f : rim) c = lerpc(c, C(150, 124, 92), 0.45f);
      if (road) if (const uint32_t k = kerbPixel(px, py, e, C(150, 148, 146))) c = k;
      break;
    }
    default: return 0;
  }
  return c;
}
// (M3 fixer round 2) the seam between a settlement's street (Road) and its square (Plaza): the tile type is looked up
// through a gentle warp so the line between the two pavings wobbles instead of following tile edges. onRoad: the pixel's
// tile is Road. sq = 1: the pixel belongs to the other paving; sq = 2 (on its own tile's road) or 3 (on a square tile
// that the seam gives to the road): a kerb pixel, returned.
template <class TM>
uint32_t streetSquareSeam(const TM& m, int px, int py, int pm, bool onRoad, int& sq) {
  sq = 0;
  const Ground other = onRoad ? Ground::Plaza : Ground::Road;
  const int tx = px >> 4, ty = py >> 4;
  // only near a tile of the other kind (cheap reject)
  bool near = false;
  for (int oy = -1; oy <= 1 && !near; oy++)
    for (int ox = -1; ox <= 1; ox++) if (m.at(tx + ox, ty + oy) == other) { near = true; break; }
  if (!near) return 0;
  auto warpedAt = [&](int x, int y) {
    const float wx = x + (vnoise(x / 11.0f, y / 11.0f, 1431) - 0.5f) * 14.0f, wy = y + (vnoise(x / 11.0f, y / 11.0f, 1433) - 0.5f) * 14.0f;
    return m.at((int)std::floor(wx / 16), (int)std::floor(wy / 16));
  };
  const Ground w = warpedAt(px, py);
  if (w == other) sq = 1;
  const bool roadSide = onRoad ? w != Ground::Plaza : w == Ground::Road;
  if (!roadSide || pm != 1) return 0;
  // the empire's kerb: pale dressed stones along the road's side of the seam, 3 px wide
  for (int k = 1; k <= 3; k++) {
    if (warpedAt(px + k, py) == Ground::Plaza || warpedAt(px - k, py) == Ground::Plaza || warpedAt(px, py + k) == Ground::Plaza ||
        warpedAt(px, py - k) == Ground::Plaza) {
      sq = sq == 1 ? 3 : 2;
      uint32_t c = lerpc(C(196, 188, 168), C(214, 206, 186), hashf(fdiv_(px, 6), fdiv_(py, 6), 1435));
      if (k == 1) c = mul(c, 1.04f);
      if (k == 3) c = C(120, 112, 100);   // its foot against the basalt
      if (fmod_(px + py, 7) == 0) c = C(160, 150, 130);
      return c;
    }
  }
  return 0;
}

// =====================================================================================================================
// M3c Wildlands (LAND lane): the ground of every biome proper (rpg/world/biomes.h Eco). The classic ecos (meadows,
// forest, autumn woods, taiga, snowfields, reed marsh, the sandy shore, the sea) keep the classic grounds in
// View::groundPixel; every other biome paints its own here: three tones picked from the shared tone field (the 16-bit
// banding of the classic grounds), one or two cheap features (value noise and hashes only: the phone's bake budget)
// and its own speckle. Pure functions of the global pixel (chunks meet seamlessly).
// =====================================================================================================================
// a jittered cell of small round stones seen from above in the 3/4 view (at most one per S px cell, wholly inside it,
// a little flattened): 0 none, 1 its shaded lower-right, 2 its body, 3 its lit upper-left, 4 its contact shadow on the
// ground below-right of it. id: the cell's hash (a colour pick)
inline int stoneAt(int px, int py, int S, uint32_t seed, float prob, float rmin, float rmax, uint32_t* id = nullptr) {
  const int cx = fdiv_(px, S), cy = fdiv_(py, S);
  const uint32_t hh = hash2(cx, cy, seed);
  if ((float)(hh & 1023) / 1024.0f >= prob) return 0;
  const float r = rmin + (float)((hh >> 10) & 255) / 255.0f * (rmax - rmin);
  const float room = std::max(0.0f, S * 0.5f - r - 1.4f);
  const float ox = cx * S + S * 0.5f + ((float)((hh >> 18) & 31) / 31.0f - 0.5f) * 2.0f * room;
  const float oy = cy * S + S * 0.5f + ((float)((hh >> 23) & 31) / 31.0f - 0.5f) * 2.0f * room;
  const float dx = px + 0.5f - ox, dy = (py + 0.5f - oy) * 1.3f;
  if (id) *id = hh;
  const float r2 = r * r;
  if (dx * dx + dy * dy <= r2) {
    const float l = -(dx + dy * 1.1f) / r;
    return l > 0.55f ? 3 : (l < -0.5f ? 1 : 2);
  }
  const float qx = dx - 1.2f, qy = dy - 1.7f;
  return qx * qx + qy * qy <= r2 ? 4 : 0;
}
// a thin winding line along a value-noise contour (cracks, roots, veins, paths): 0 none, 1 its core, 2 the rim on its
// lit side, 3 the rim on its shaded side (so a groove or a raised vein reads in relief under the top-left light)
inline int ridgeLine(float v, float w) {
  const float d = v - 0.5f, a = std::fabs(d);
  if (a < w) return 1;
  if (a < w * 2.2f) return d < 0 ? 2 : 3;
  return 0;
}
// pebbles laid at random (a jittered cell pattern, the nearest of the 3x3 cells' seeds, so no grid shows): the pixel's
// distance (px) to its pebble's centre and its offset from it, and the pebble's hash (its size and colour)
struct Pebble { float d = 99.0f, dx = 0, dy = 0; uint32_t id = 0; };
inline Pebble pebbleAt(int px, int py, int S, uint32_t seed) {
  const int cx0 = fdiv_(px, S), cy0 = fdiv_(py, S);
  Pebble b;
  float bd = 1e9f;
  for (int oy = -1; oy <= 1; oy++)
    for (int ox = -1; ox <= 1; ox++) {
      const uint32_t hh = hash2(cx0 + ox, cy0 + oy, seed);
      const float sx = (cx0 + ox + 0.15f + (float)(hh & 255) / 255.0f * 0.7f) * S, sy = (cy0 + oy + 0.15f + (float)((hh >> 8) & 255) / 255.0f * 0.7f) * S;
      const float ddx = px + 0.5f - sx, ddy = (py + 0.5f - sy) * 1.3f, d = ddx * ddx + ddy * ddy;
      if (d < bd) { bd = d; b.dx = ddx; b.dy = ddy; b.id = hh; }
    }
  b.d = std::sqrt(bd);
  return b;
}
// a pebble's pixel (0 none, 1 shaded lower-right, 2 body, 3 lit upper-left, 4 its contact shadow) for a pebble of radius r
inline int pebbleShade(const Pebble& q, float r) {
  if (q.d <= r) {
    const float l = -(q.dx + q.dy * 1.1f) / std::max(0.5f, r);
    return l > 0.5f ? 3 : (l < -0.45f ? 1 : 2);
  }
  const float sx = q.dx - 1.1f, sy = q.dy - 1.6f;
  return sx * sx + sy * sy <= r * r ? 4 : 0;
}
// grass tufts in a biome's colours: a dark root pixel, lit blade tips
inline uint32_t tufted(uint32_t c, int px, int py, uint32_t seed, float dens, float dark, float lit) {
  const int tf = tuft(px, py, seed, dens);
  return tf == 1 ? mul(c, dark) : tf == 2 ? mul(c, lit) : c;
}
// small flowers (one per 5 px cell at most): a bright petal, its shaded side and a dark leaf under it. pal: n colours
inline bool flowered(uint32_t& c, int px, int py, uint32_t seed, float dens, const uint32_t* pal, int npal) {
  const int cx = fdiv_(px, 5), cy = fdiv_(py, 5);
  const uint32_t hh = hash2(cx, cy, seed);
  if ((float)(hh & 1023) / 1024.0f >= dens) return false;
  const int fx = cx * 5 + 1 + (int)((hh >> 10) % 3), fy = cy * 5 + 1 + (int)((hh >> 13) % 3);
  const uint32_t pc = pal[(hh >> 16) % (uint32_t)npal];
  if (px == fx && py == fy) { c = pc; return true; }
  if (px == fx + 1 && py == fy) { c = mul(pc, 0.78f); return true; }
  if (px == fx && py == fy + 1) { c = mul(c, 0.70f); return true; }
  return false;
}

// the ground of biome e on ground g at a global pixel (n: the tone field, h: a per-pixel hash). false: paint the classic
// ground of g (the classic ecos, and snow or marsh lying in a biome that has no look of its own for it)
bool ecoPixel(Eco e, Ground g, int px, int py, float n, float h, uint32_t& c) {
  if (g == Ground::Snow && e != Eco::Tundra && e != Eco::Glacier && e != Eco::FrozenLakes && e != Eco::SeaCliffs && e != Eco::Shingle) return false;
  if (g == Ground::Swamp && ecoFamily(e) != Biome::Swamp) return false;
  const bool lush = g == Ground::Meadow;   // a meadow patch in a grassland: a greener, flowered spot
  switch (e) {
    // ---- coasts
    case Eco::SeaCliffs: {   // salt-bleached clifftop turf, the bedrock breaking through, sea pinks
      if (g == Ground::Snow) {   // (M3c fixer) a cold clifftop: wind-scoured snow, the dark bedrock showing through in slabs
        c = pick3(n, C(214, 222, 236), C(228, 234, 244), C(240, 244, 250));
        const float r = vnoise(px / 13.0f, py / 13.0f, 1613) + (vnoise(px / 4.0f, py / 4.0f, 1619) - 0.5f) * 0.12f;
        if (r > 0.66f) {
          const float l = -((vnoise((px + 2) / 13.0f, (py + 2) / 13.0f, 1613) - vnoise((px - 2) / 13.0f, (py - 2) / 13.0f, 1613))) * 9.0f;
          c = r < 0.69f ? C(96, 102, 116) : l > 0.15f ? C(150, 156, 168) : l < -0.15f ? C(92, 98, 112) : C(120, 126, 140);
          if (r > 0.69f && hashf(px / 3, py, 1615) < 0.05f) c = C(78, 84, 98);   // the slab's cracks
        } else if (h > 0.992f) c = C(196, 206, 224);
        return true;
      }
      c = pick3(n, C(98, 134, 88), C(110, 146, 94), C(124, 156, 100));
      c = mul(c, 0.94f + vnoise(px / 34.0f, py / 34.0f, 1611) * 0.12f);
      const float r = vnoise(px / 15.0f, py / 15.0f, 1613) + (bayer(px, py) - 0.5f) * 0.10f;
      if (r > 0.70f) {
        c = r > 0.75f ? pick3(n, C(132, 136, 140), C(146, 150, 152), C(160, 162, 162)) : C(96, 100, 104);
        if (r > 0.75f && hashf(px / 3, py, 1615) < 0.06f) c = C(84, 88, 94);   // the slab's cracks
        else if (r > 0.75f && h > 0.985f) c = C(196, 190, 120);                // yellow lichen
      } else {
        c = tufted(c, px, py, 1617, 0.5f, 0.76f, 1.16f);
        if (h > 0.993f) c = C(234, 132, 170);
        else if (h > 0.990f) c = C(250, 214, 228);
      }
      return true;
    }
    case Eco::Shingle: {   // rounded flint and slate pebbles, packed, sea-grey grit in the gaps
      const Pebble q = pebbleAt(px, py, 6, 1621);
      const int s = pebbleShade(q, 1.7f + (float)((q.id >> 16) & 255) / 255.0f * 1.6f);
      const uint32_t id = q.id;
      static const uint32_t pb[6] = {C(150, 146, 140), C(170, 164, 154), C(128, 126, 126), C(188, 180, 166), C(112, 114, 120), C(160, 150, 136)};
      const uint32_t base = mul(pb[(id >> 28) % 6], 0.95f + n * 0.1f);
      if (g == Ground::Snow) {   // (M3c fixer) a cold shore: the same pebbles, frost-grey, snow drifted in the gaps and on their tops
        const uint32_t cb = lerpc(base, C(120, 128, 146), 0.35f);
        if (s == 0) c = pick3(n, C(212, 220, 234), C(226, 232, 242), C(238, 242, 250));
        else if (s == 4) c = C(160, 170, 192);
        else c = s == 3 ? C(240, 244, 250) : s == 1 ? mul(cb, 0.72f) : cb;
        return true;
      }
      if (s == 0) c = pick3(n, C(118, 112, 102), C(128, 122, 110), C(138, 132, 118));
      else if (s == 4) c = C(84, 80, 80);
      else c = s == 3 ? lerpc(base, C(240, 236, 226), 0.35f) : s == 1 ? mul(base, 0.74f) : base;
      if (h < 0.003f) c = C(240, 236, 226);   // a shell
      return true;
    }
    case Eco::CoralCoast: {   // white coral sand, broken coral and shells on it
      c = pick3(n, C(234, 226, 204), C(242, 236, 216), C(250, 246, 230));
      const float rip = std::sin(px * 0.31f + py * 0.1f + vnoise(px / 16.0f, py / 16.0f, 171) * 8);
      if (rip > 0.82f) c = mul(c, 0.95f);
      if (h < 0.008f) c = C(236, 136, 132);
      else if (h < 0.013f) c = C(248, 184, 156);
      else if (h < 0.016f) c = C(206, 192, 170);
      else if (h > 0.995f) c = C(255, 253, 246);
      return true;
    }
    // ---- grasslands
    case Eco::FlowerMeadow: {
      c = pick3(n, C(102, 164, 72), C(116, 176, 80), C(130, 186, 88));
      c = mul(c, 0.94f + vnoise(px / 40.0f, py / 40.0f, 141) * 0.12f);
      c = tufted(c, px, py, 401, 0.5f, 0.74f, 1.16f);
      static const uint32_t pal[6] = {C(250, 250, 238), C(250, 220, 84), C(240, 128, 168), C(164, 150, 250), C(232, 76, 64), C(250, 180, 80)};
      const float clump = vnoise(px / 18.0f, py / 18.0f, 1625);
      flowered(c, px, py, 1627, 0.10f + clump * clump * 0.55f, pal, 6);
      return true;
    }
    case Eco::Prairie: {   // tall grass combed by the wind: long light and dark streaks the way it blows
      c = pick3(n, C(146, 148, 70), C(160, 158, 78), C(174, 170, 88));
      c = mul(c, 0.94f + vnoise(px / 44.0f, py / 44.0f, 1631) * 0.12f);
      const float u = px * 0.8f + py * 0.6f, v = py * 0.8f - px * 0.6f;
      const float st = vnoise(u / 30.0f, v / 3.2f, 1633);
      if (st > 0.66f) c = lerpc(c, C(208, 198, 114), std::min(1.0f, (st - 0.66f) * 2.4f));
      else if (st < 0.32f) c = mul(c, 0.86f + st * 0.3f);
      c = tufted(c, px, py, 1635, 0.62f, 0.74f, 1.18f);
      if (lush) c = lerpc(c, C(120, 158, 72), 0.25f);
      if (h > 0.996f) c = C(232, 208, 92);
      return true;
    }
    case Eco::Steppe: {   // pale dry grass and gravel
      c = pick3(n, C(150, 148, 98), C(162, 158, 106), C(174, 168, 114));
      const float gv = vnoise(px / 24.0f, py / 24.0f, 1641) + (bayer(px, py) - 0.5f) * 0.10f;
      if (gv < 0.32f) {
        c = lerpc(c, pick3(n, C(156, 146, 126), C(168, 158, 138), C(178, 168, 148)), std::min(1.0f, (0.32f - gv) * 8.0f));
        const Pebble q = pebbleAt(px, py, 5, 1643);
        const int s = pebbleShade(q, 0.6f + (float)((q.id >> 16) & 255) / 255.0f * 0.8f);
        if (s == 4) c = mul(c, 0.86f);
        else if (s) c = s == 3 ? C(204, 196, 178) : s == 1 ? C(112, 106, 100) : ((q.id >> 28) & 1 ? C(146, 138, 126) : C(176, 166, 150));
      } else {
        c = tufted(c, px, py, 1645, gv < 0.4f ? 0.25f : 0.45f, 0.78f, 1.16f);
        if (h < 0.012f) c = C(132, 124, 112);
      }
      if (lush) c = lerpc(c, C(130, 150, 88), 0.22f);
      return true;
    }
    case Eco::Savanna: {   // golden grass in tussocks, bare red earth between them
      c = pick3(n, C(166, 146, 68), C(182, 162, 78), C(198, 178, 90));
      c = mul(c, 0.93f + vnoise(px / 40.0f, py / 40.0f, 141) * 0.14f);
      const float dry = vnoise(px / 22.0f, py / 22.0f, 1601) + (bayer(px, py) - 0.5f) * 0.08f;
      if (dry > 0.66f) {
        c = pick3(n, C(170, 122, 78), C(182, 134, 86), C(192, 146, 96));
        if (h < 0.05f) c = C(148, 102, 70);
        else if (h > 0.985f) c = C(210, 172, 122);
      } else {
        c = tufted(c, px, py, 1603, dry > 0.58f ? 0.25f : 0.55f, 0.72f, 1.18f);
        if (h > 0.996f) c = C(232, 214, 130);
      }
      if (lush) c = lerpc(c, C(150, 158, 74), 0.25f);
      return true;
    }
    case Eco::Heath: {   // heather in purple-brown tussocks on the moor, dark peat between
      // (M4, owner carry-over: "heath ground cover reads as flat green blobs with stepped edges") the moor is a mat of
      // heather tussocks: low rounded mounds of wiry purple-brown, each lit on its top-left and shaded at its foot to
      // the bottom-right, the bloom thick on the crowns; between them dark peaty earth and a little moss; peat hags as
      // wet black-brown hollows. Every edge is dithered (a 4x4 order over a soft threshold), never a hard blot.
      const float hz = vnoise(px / 26.0f, py / 26.0f, 1653) + (vnoise(px / 6.0f, py / 6.0f, 1657) - 0.5f) * 0.05f;
      const float dz = (bayer(px, py) - 0.5f) * 0.035f;
      if (hz + dz < 0.15f) {   // a peat hag: dark wet earth, a sheen of water in its deepest part
        c = pick3(n, C(62, 48, 42), C(70, 54, 46), C(78, 60, 50));
        if (hz < 0.11f) c = lerpc(c, C(70, 78, 92), 0.35f + (hz < 0.09f ? 0.2f : 0.0f));
        if (hz < 0.10f && h > 0.985f) c = C(150, 158, 166);
        return true;
      }
      // the tussocks: a clump field at two scales; inside a clump the heather, between them the earth
      const float v = vnoise(px / 7.0f, py / 7.0f, 1651) * 0.75f + vnoise(px / 3.0f, py / 3.0f, 1659) * 0.25f;
      // (fixer M4 r2, review: "still noisy camo") the heather covers most of the moor: the earth shows in narrower gaps
      const float edge = 0.40f - std::min(0.10f, (hz - 0.15f) * 0.25f) + (bayer(px, py) - 0.5f) * 0.06f;
      if (v < edge) {
        // the earth between the clumps: peaty brown, moss here and there, in the clumps' shade on their bottom-right
        c = pick3(n, C(80, 64, 56), C(86, 69, 60), C(92, 74, 63));   // (fixer M4 r2: nearer the heather's own value)
        if (vnoise(px / 9.0f, py / 9.0f, 1661) > 0.62f) c = lerpc(c, C(92, 108, 62), 0.35f);
        const float up = vnoise((px - 2) / 7.0f, (py - 2) / 7.0f, 1651) * 0.75f + vnoise((px - 2) / 3.0f, (py - 2) / 3.0f, 1659) * 0.25f;
        if (up >= edge) c = mul(c, 0.86f);   // a clump just up-left: its shadow
        if (h > 0.993f) c = C(150, 140, 120);   // a pale pebble
        if (lush) c = lerpc(c, C(96, 118, 66), 0.25f);
        return true;
      }
      // a clump: shaded by its slope to the top-left light, the bloom on its crown, wiry dark stems at its rim
      const float vx = vnoise((px + 1) / 7.0f, py / 7.0f, 1651) - vnoise((px - 1) / 7.0f, py / 7.0f, 1651);
      const float vy = vnoise(px / 7.0f, (py + 1) / 7.0f, 1651) - vnoise(px / 7.0f, (py - 1) / 7.0f, 1651);
      // (fixer M4 r2) a gentler relief and a narrower ramp: soft rounded mats, not a high-contrast polygon pattern
      const float lit = std::clamp(0.45f + (vx + vy) * 3.0f + (v - edge) * 0.8f + (bayer(px + 1, py + 2) - 0.5f) * 0.18f, 0.0f, 1.0f);
      static const uint32_t ramp[5] = {C(64, 47, 54), C(78, 57, 65), C(92, 67, 77), C(106, 78, 89), C(122, 91, 103)};
      int k = (int)(lit * 4.99f);
      if (v - edge < 0.025f) k = std::min(k, 1);   // the rim: dark stems
      c = ramp[std::clamp(k, 0, 4)];
      // brown and olive in the older growth, the bloom brightest on the crowns
      if (vnoise(px / 11.0f, py / 11.0f, 1663) > 0.55f) c = lerpc(c, C(104, 86, 60), 0.4f);
      if (k >= 2 && h > 0.90f) c = h > 0.98f ? C(186, 138, 176) : C(150, 104, 140);
      if (lush) c = lerpc(c, C(110, 130, 76), 0.2f);
      return true;
    }
    case Eco::ChalkDowns: {   // short bright turf, white chalk paths and scars where it wears through
      c = pick3(n, C(98, 162, 74), C(110, 174, 80), C(122, 184, 88));
      c = mul(c, 0.95f + vnoise(px / 40.0f, py / 40.0f, 141) * 0.10f);
      c = tufted(c, px, py, 1665, 0.42f, 0.78f, 1.14f);
      // a few worn tracks of white chalk wandering over the downs (only where the broad mask lets them run)
      const int r = vnoise(px / 110.0f, py / 110.0f, 1669) > 0.56f ? ridgeLine(vnoise(px / 52.0f, py / 52.0f, 1661), 0.009f) : 0;
      if (r == 1) c = h < 0.10f ? C(212, 208, 192) : C(236, 234, 222);
      else if (r) c = r == 2 ? C(170, 196, 136) : C(86, 136, 66);
      else {
        // chalk scars where the turf has slipped: white with flints, a shaded lip of turf round them
        const float ch = vnoise(px / 17.0f, py / 17.0f, 1663) + (bayer(px, py) - 0.5f) * 0.03f;
        if (ch > 0.835f) c = h < 0.04f ? C(82, 82, 92) : (ch > 0.86f ? C(240, 238, 228) : C(216, 214, 198));
        else if (ch > 0.815f) c = mul(c, 0.84f);
      }
      if (lush) { static const uint32_t pal[3] = {C(250, 250, 238), C(246, 214, 80), C(190, 140, 230)}; flowered(c, px, py, 1667, 0.12f, pal, 3); }
      return true;
    }
    case Eco::AlpineMeadow: {   // cool turf full of tiny flowers, grey stones
      c = pick3(n, C(88, 150, 86), C(100, 162, 94), C(112, 172, 102));
      c = tufted(c, px, py, 1671, 0.45f, 0.76f, 1.15f);
      uint32_t id = 0;
      const int s = stoneAt(px, py, 13, 1673, 0.18f, 1.6f, 3.0f, &id);
      if (s == 4) c = mul(c, 0.70f);
      else if (s) c = s == 3 ? C(196, 196, 190) : s == 1 ? C(108, 108, 112) : C(152, 152, 150);
      else {
        static const uint32_t pal[5] = {C(92, 112, 232), C(246, 246, 238), C(246, 214, 80), C(214, 92, 170), C(130, 170, 250)};
        flowered(c, px, py, 1675, lush ? 0.38f : 0.22f, pal, 5);
      }
      return true;
    }
    case Eco::StonePlains: {   // muted grass over old flat stones, half buried and lichened
      c = pick3(n, C(98, 138, 76), C(108, 148, 82), C(118, 158, 88));
      c = mul(c, 0.94f + vnoise(px / 40.0f, py / 40.0f, 141) * 0.12f);
      c = tufted(c, px, py, 1681, 0.45f, 0.76f, 1.15f);
      uint32_t id = 0;
      const int s = stoneAt(px, py, 15, 1683, 0.24f, 2.6f, 4.6f, &id);
      if (s == 4) c = mul(c, 0.72f);
      else if (s) {
        c = s == 3 ? C(184, 182, 174) : s == 1 ? C(104, 102, 104) : C(146, 144, 138);
        if (s == 2 && h > 0.90f) c = (id >> 30) ? C(190, 184, 110) : C(140, 146, 90);
      } else if (h < 0.008f) c = C(150, 150, 142);
      if (lush) c = lerpc(c, C(110, 160, 84), 0.2f);
      return true;
    }
    case Eco::LakeDistrict: {   // lush blue-green grass, damp hollows with rushes
      c = pick3(n, C(72, 144, 82), C(84, 156, 90), C(96, 168, 96));
      c = mul(c, 0.94f + vnoise(px / 40.0f, py / 40.0f, 141) * 0.12f);
      const float d = vnoise(px / 20.0f, py / 20.0f, 1691);
      if (d < 0.26f) c = lerpc(c, C(64, 112, 90), 0.45f);
      c = tufted(c, px, py, 1693, d < 0.3f ? 0.6f : 0.45f, 0.74f, 1.16f);
      if (lush) { static const uint32_t pal[3] = {C(250, 250, 238), C(246, 214, 80), C(170, 150, 250)}; flowered(c, px, py, 1695, 0.10f, pal, 3); }
      return true;
    }
    // ---- woods
    case Eco::BirchWood: {   // light litter: yellow leaves, curls of white bark
      c = pick3(n, C(90, 122, 62), C(102, 134, 66), C(114, 144, 72));
      c = mul(c, 0.92f + vnoise(px / 30.0f, py / 30.0f, 151) * 0.16f);
      if (vnoise(px / 12.0f, py / 12.0f, 1701) > 0.68f) c = lerpc(c, C(168, 150, 78), 0.35f);
      c = tufted(c, px, py, 1703, 0.45f, 0.74f, 1.15f);
      if (h < 0.030f) c = C(198, 178, 84);
      else if (h < 0.045f) c = C(158, 126, 62);
      else if (h > 0.995f) c = C(228, 224, 212);
      return true;
    }
    case Eco::GiantForest: {   // deep moss over great roots
      c = pick3(n, C(38, 80, 46), C(46, 92, 50), C(54, 104, 54));
      const float mv = vnoise(px / 7.0f, py / 7.0f, 1711);
      if (mv > 0.64f) c = mv > 0.74f ? C(96, 146, 66) : C(72, 124, 58);
      // roots: short winding runs (a broad mask breaks the contour lines up), tapering where the mask fades
      const float rm = vnoise(px / 26.0f, py / 26.0f, 1715);
      const int r = rm > 0.52f ? ridgeLine(vnoise(px / 22.0f, py / 22.0f, 1713), 0.012f + (rm - 0.52f) * 0.06f) : 0;
      if (r == 1) c = C(84, 62, 44);
      else if (r == 2) c = C(116, 90, 60);
      else if (r == 3) c = C(30, 44, 34);
      if (!r && h < 0.010f) c = C(112, 86, 52);
      return true;
    }
    case Eco::DarkForest: {   // near-black litter, roots, rot and pale fungus
      c = pick3(n, C(48, 50, 44), C(56, 58, 50), C(64, 64, 54));
      c = mul(c, 0.90f + vnoise(px / 24.0f, py / 24.0f, 1721) * 0.20f);
      const float rm = vnoise(px / 24.0f, py / 24.0f, 1727);
      const int r = rm > 0.55f ? ridgeLine(vnoise(px / 20.0f, py / 20.0f, 1723), 0.010f + (rm - 0.55f) * 0.06f) : 0;
      if (r == 1) c = C(32, 30, 32);
      else if (r == 2) c = C(84, 74, 64);
      else {
        c = tufted(c, px, py, 1725, 0.2f, 0.8f, 1.2f);
        if (h < 0.05f) c = C(72, 58, 48);
        else if (h < 0.07f) c = C(60, 46, 62);
        else if (h > 0.996f) c = C(170, 168, 146);
      }
      return true;
    }
    case Eco::BlossomGrove: {   // soft grass under a carpet of fallen petals
      c = pick3(n, C(100, 160, 84), C(112, 170, 90), C(124, 180, 96));
      c = tufted(c, px, py, 1731, 0.45f, 0.76f, 1.15f);
      const float carpet = vnoise(px / 16.0f, py / 16.0f, 1733);
      const float dens = carpet * carpet * 0.55f;
      if (h < dens) c = h < dens * 0.35f ? C(252, 228, 236) : h < dens * 0.75f ? C(244, 182, 206) : C(220, 140, 176);
      return true;
    }
    case Eco::BambooForest: {   // dry yellow-green floor strewn with long slanted bamboo leaves
      c = pick3(n, C(134, 138, 72), C(148, 150, 78), C(162, 162, 86));
      c = mul(c, 0.94f + vnoise(px / 30.0f, py / 30.0f, 151) * 0.12f);
      const int lxp = px - (py >> 1);
      const int cx = fdiv_(lxp, 6), in = lxp - cx * 6;
      if (in >= 1 && in <= 4) {
        const uint32_t lh = hash2(cx, fdiv_(py, 2), 1741);
        if ((py & 1) == 0 && (lh & 7) < 3) {
          static const uint32_t lc[3] = {C(198, 186, 104), C(118, 138, 62), C(152, 122, 70)};
          c = lc[(lh >> 3) % 3];
          if (in == 4) c = mul(c, 0.86f);
        } else if ((py & 1) == 1 && (lh & 7) < 3) c = mul(c, 0.84f);   // its shadow on the row below
      }
      return true;
    }
    case Eco::Jungle: {   // lush dark loam, roots, fallen leaves and flowers
      c = pick3(n, C(42, 78, 44), C(50, 90, 48), C(58, 102, 52));
      if (vnoise(px / 9.0f, py / 9.0f, 1751) < 0.30f) c = lerpc(c, C(76, 58, 40), 0.55f);
      const int r = ridgeLine(vnoise(px / 18.0f, py / 18.0f, 1753), 0.020f);
      if (r == 1) c = C(84, 62, 42);
      else if (r == 2) c = C(118, 90, 56);
      else if (r == 3) c = C(28, 42, 30);
      else {
        c = tufted(c, px, py, 1755, 0.55f, 0.72f, 1.22f);
        if (h > 0.985f) c = C(98, 166, 62);
        else if (h < 0.006f) c = C(206, 70, 60);
      }
      return true;
    }
    case Eco::MushroomForest: {   // violet mycelium threads through dark humus, pale spores
      c = pick3(n, C(64, 50, 70), C(74, 58, 80), C(84, 66, 90));
      c = mul(c, 0.92f + vnoise(px / 30.0f, py / 30.0f, 151) * 0.16f);
      const int r = ridgeLine(vnoise(px / 9.0f, py / 9.0f, 1761), 0.017f);
      if (r == 1) c = C(192, 170, 214);
      else if (r == 2) c = C(132, 112, 152);
      else if (r == 3) c = C(52, 40, 60);
      else if (std::fabs(vnoise(px / 5.0f, py / 5.0f, 1763) - 0.5f) < 0.012f) c = C(132, 210, 204);   // the glowing threads
      else if (h > 0.993f) c = C(228, 208, 248);
      return true;
    }
    case Eco::Silverwood: {   // silver moss on pale green, glints
      c = pick3(n, C(106, 136, 118), C(120, 150, 130), C(134, 162, 142));
      const float mv = vnoise(px / 8.0f, py / 8.0f, 1771);
      if (mv > 0.70f) c = mv > 0.77f ? C(156, 180, 166) : C(140, 166, 152);
      else c = tufted(c, px, py, 1773, 0.4f, 0.78f, 1.14f);
      if (h > 0.996f) c = C(246, 252, 250);
      return true;
    }
    // ---- cold
    case Eco::TaigaBog: {   // sphagnum in green, ochre and red cushions, dark pools, cotton-grass
      const float mv = vnoise(px / 6.0f, py / 6.0f, 1781);
      c = mv < 0.42f ? C(104, 122, 70) : mv < 0.62f ? C(148, 136, 80) : C(136, 92, 74);
      c = mul(c, 0.90f + n * 0.18f);
      const float p = vnoise(px / 11.0f, py / 11.0f, 1783) + (bayer(px, py) - 0.5f) * 0.06f;
      if (p > 0.76f) c = p > 0.82f && h < 0.10f ? C(96, 118, 130) : C(36, 48, 56);
      else if (p > 0.72f) c = C(70, 70, 50);
      else if (h > 0.993f) c = C(244, 244, 236);
      return true;
    }
    case Eco::Tundra: {   // lichen and moss broken into frost polygons, snow lying in the hollows
      c = pick3(n, C(124, 130, 102), C(136, 140, 110), C(148, 150, 118));
      const int r1 = ridgeLine(vnoise(px / 15.0f, py / 15.0f, 1791), 0.016f);
      const int r = r1 ? r1 : ridgeLine(vnoise((px + py) / 21.0f, (py - px) / 21.0f, 1793), 0.014f);
      const float mo = vnoise(px / 9.0f, py / 9.0f, 1797);   // moss and lichen mottling the polygons
      if (mo > 0.66f) c = lerpc(c, C(112, 132, 84), 0.5f);
      else if (mo < 0.30f) c = lerpc(c, C(164, 150, 102), 0.4f);
      if (r == 1) c = mul(c, 0.80f);
      else if (r == 2) c = mul(c, 1.10f);
      else if (r == 3) c = mul(c, 0.90f);
      else if (h < 0.040f) c = C(190, 186, 118);
      else if (h < 0.055f) c = C(170, 112, 74);
      else if (h > 0.990f) c = C(214, 216, 208);
      if (g == Ground::Snow) {
        const float sv = vnoise(px / 20.0f, py / 20.0f, 1795) + (bayer(px, py) - 0.5f) * 0.10f;
        if (sv > 0.64f) c = sv > 0.69f ? pick3(n, C(220, 228, 240), C(230, 236, 246), C(240, 244, 250)) : C(196, 206, 214);
      }
      return true;
    }
    case Eco::Glacier: {   // blue-white ice under wind-cut snow, crevasses (their lit far wall, their dark depth)
      c = pick3(n, C(194, 214, 236), C(206, 224, 242), C(218, 232, 248));
      // (M3c fixer, review: "hard horizontal light bars lined up with the tiles") the streaks and crevasses are read
      // through a warped, turned frame, so the wind's drifts run aslant and wander instead of stacking in grid bars
      const float wx = px + (vnoise(px / 37.0f, py / 37.0f, 1807) - 0.5f) * 26.0f, wy = py + (vnoise(px / 37.0f, py / 37.0f, 1809) - 0.5f) * 26.0f;
      const float u = wx * 0.92f + wy * 0.38f, v = wy * 0.92f - wx * 0.38f;
      if (vnoise(wx / 24.0f, wy / 24.0f, 1801) > 0.70f) c = C(156, 194, 228);
      if (vnoise(u / 30.0f, v / 6.0f, 1803) + (bayer(px, py) - 0.5f) * 0.03f > 0.66f) c = C(234, 242, 252);
      const float d = vnoise(u / 44.0f, v / 12.0f, 1805) - 0.5f;
      if (std::fabs(d) < 0.012f) c = C(50, 84, 146);
      else if (d >= 0.012f && d < 0.032f) c = C(112, 154, 208);
      else if (d <= -0.012f && d > -0.026f) c = C(244, 250, 255);
      return true;
    }
    case Eco::FrozenLakes: {   // the snow between the frozen lakes, laid in drifts by the wind, dead grass poking through
      c = pick3(n, C(212, 224, 238), C(224, 232, 244), C(236, 242, 250));
      const float dv = vnoise(px / 36.0f, py / 10.0f, 1811);
      if (dv > 0.64f) c = C(246, 249, 253);
      else if (dv < 0.28f) c = C(204, 218, 236);
      const int tf = tuft(px, py, 1813, 0.14f);
      if (tf == 1) c = C(122, 118, 100); else if (tf == 2) c = C(168, 160, 128);
      return true;
    }
    // ---- wet
    case Eco::PeatBog: {   // dark peat, moss cushions, black pools glinting with the sky
      c = pick3(n, C(60, 48, 40), C(68, 54, 44), C(78, 62, 48));
      const float mv = vnoise(px / 9.0f, py / 9.0f, 1821);
      if (mv > 0.66f) c = mv > 0.74f ? C(100, 104, 58) : C(84, 86, 52);
      const float p = vnoise(px / 8.0f, py / 8.0f, 1823) + (bayer(px, py) - 0.5f) * 0.04f;
      if (p > 0.77f) c = (p > 0.81f && hashf(px >> 1, py, 1825) < 0.08f) ? C(118, 132, 142) : C(26, 30, 36);
      else if (p > 0.745f) c = C(48, 40, 34);
      else if (h > 0.995f) c = C(240, 240, 232);
      return true;
    }
    case Eco::Mangrove: {   // grey-brown tidal mud, wet sheen, root arcs, crab holes
      c = pick3(n, C(96, 82, 62), C(106, 92, 68), C(116, 100, 74));
      const float s = vnoise(px / 11.0f, py / 11.0f, 1831);
      if (s > 0.72f) c = h > 0.97f ? C(170, 190, 186) : C(92, 110, 106);
      else if (s > 0.65f) c = C(120, 116, 98);
      const int r = ridgeLine(vnoise(px / 12.0f, py / 12.0f, 1833), 0.020f);
      if (r == 1) c = C(70, 56, 44);
      else if (r == 2) c = C(110, 92, 70);
      if (!r && h < 0.008f) c = C(46, 38, 34);
      return true;
    }
    case Eco::FloodedForest: {   // dark mud under a film of still water, leaves floating on it
      c = pick3(n, C(58, 74, 54), C(66, 84, 58), C(74, 92, 62));
      const float w = vnoise(px / 13.0f, py / 13.0f, 1841) + (bayer(px, py) - 0.5f) * 0.08f;
      if (w > 0.63f) {
        c = w > 0.71f ? C(64, 98, 98) : C(62, 86, 80);
        if (h < 0.05f) c = C(112, 122, 60);
        else if (h > 0.992f) c = C(150, 180, 184);
      } else if (h < 0.02f) c = C(104, 84, 52);
      return true;
    }
    // ---- dry
    case Eco::Dunes: {   // a dune field: broad crests, windward faces in the light, slip faces in shade, wind ripples
      const float d0 = vnoise((px - 3) / 58.0f, (py - 3) / 34.0f, 1851), d1 = vnoise((px + 3) / 58.0f, (py + 3) / 34.0f, 1851);
      const float sl = (d1 - d0) * 9.0f;   // > 0: the face turned to the light (rising toward the lower right)
      c = pick3(n + sl * 0.55f, C(206, 178, 122), C(222, 198, 140), C(236, 214, 158));
      if (sl < -0.30f) c = mul(c, 0.88f);
      else if (sl > 0.35f) {
        const float rip = std::sin(px * 0.35f + py * 0.12f + vnoise(px / 16.0f, py / 16.0f, 171) * 8);
        if (rip > 0.8f) c = mul(c, 0.94f);
      }
      if (h < 0.005f) c = C(180, 156, 112);
      return true;
    }
    case Eco::StonyDesert: {   // reg: a pavement of wind-polished pebbles, desert varnish
      c = pick3(n, C(190, 164, 122), C(200, 174, 130), C(210, 184, 138));
      const Pebble q = pebbleAt(px, py, 5, 1861);
      const uint32_t id = q.id;
      const int s = pebbleShade(q, ((id >> 16) & 7) == 0 ? 2.2f : 0.7f + (float)((id >> 19) & 255) / 255.0f * 0.9f);
      static const uint32_t pb[4] = {C(140, 108, 86), C(118, 94, 80), C(168, 138, 108), C(96, 78, 70)};
      const uint32_t base = pb[(id >> 28) & 3];
      if (s == 4) c = mul(c, 0.82f);
      else if (s) c = s == 3 ? lerpc(base, C(240, 220, 180), 0.4f) : s == 1 ? mul(base, 0.76f) : base;
      return true;
    }
    case Eco::Badlands: {   // banded red strata laid bare by erosion
      const float yy = py + (vnoise(px / 46.0f, py / 46.0f, 1871) - 0.5f) * 44.0f + (vnoise(px / 11.0f, py / 11.0f, 1873) - 0.5f) * 4.0f;
      const int band = (int)std::floor(yy / 6.0f);
      static const uint32_t BAND[6] = {C(176, 94, 62), C(196, 116, 74), C(214, 142, 94), C(226, 184, 134), C(160, 80, 58), C(204, 128, 84)};
      c = mul(BAND[hash2(band, 0, 1875) % 6], 0.94f + n * 0.12f);
      if (h < 0.02f) c = mul(c, 0.80f);
      else if (h > 0.99f) c = mul(c, 1.12f);
      return true;
    }
    case Eco::SaltFlats: {   // a white salt crust in raised polygons
      c = pick3(n, C(226, 224, 216), C(234, 232, 224), C(242, 240, 234));
      if (vnoise(px / 40.0f, py / 40.0f, 1881) < 0.30f) c = lerpc(c, C(204, 198, 186), 0.45f);
      const int r1 = ridgeLine(vnoise(px / 9.0f, py / 9.0f, 1883), 0.020f);
      const int r = r1 ? r1 : ridgeLine(vnoise((px + py) / 12.7f, (py - px) / 12.7f, 1885), 0.018f);
      if (r == 1) c = C(250, 250, 246);
      else if (r == 2) c = C(255, 255, 252);
      else if (r == 3) c = C(196, 194, 186);
      return true;
    }
    case Eco::Scrubland: {   // dusty earth, patches of dry grass, stones
      c = pick3(n, C(168, 146, 102), C(178, 156, 110), C(188, 166, 118));
      const float gv = vnoise(px / 14.0f, py / 14.0f, 1891) + (bayer(px, py) - 0.5f) * 0.10f;
      if (gv > 0.58f) { c = lerpc(c, C(132, 132, 76), 0.55f); c = tufted(c, px, py, 1893, 0.5f, 0.76f, 1.18f); }
      uint32_t id = 0;
      const int s = stoneAt(px, py, 9, 1895, 0.28f, 1.0f, 1.8f, &id);
      if (s == 4) c = mul(c, 0.80f);
      else if (s) c = s == 3 ? C(206, 186, 150) : s == 1 ? C(110, 92, 74) : C(150, 128, 100);
      return true;
    }
    case Eco::Oasis: {   // green grass round the water, the desert sand breaking in
      c = pick3(n, C(80, 148, 78), C(92, 160, 84), C(104, 170, 90));
      c = tufted(c, px, py, 1901, 0.5f, 0.74f, 1.18f);
      const float sv = vnoise(px / 18.0f, py / 18.0f, 1903) + (bayer(px, py) - 0.5f) * 0.12f;
      if (sv < 0.30f) c = pick3(n, C(214, 190, 134), C(224, 202, 146), C(232, 212, 158));
      else if (sv < 0.36f && h < 0.5f) c = C(176, 170, 104);
      return true;
    }
    // ---- wondrous
    case Eco::AshFields: {   // black cinder and ash, glowing cracks, sulphur stains
      c = pick3(n, C(50, 46, 48), C(58, 54, 54), C(68, 62, 60));
      if (vnoise(px / 30.0f, py / 30.0f, 1911) > 0.80f && h < 0.35f) c = (h < 0.12f ? C(226, 206, 92) : C(170, 152, 70));   // a sulphur crust, speckled
      // (fixer r2) the glowing veins taper out where their field fades (a hard cut there ended them in blunt stubs)
      const float vm = std::clamp((vnoise(px / 44.0f, py / 44.0f, 1917) - 0.44f) / 0.12f, 0.0f, 1.0f);
      const int r = vm > 0.0f ? ridgeLine(vnoise(px / 12.0f, py / 12.0f, 1913), 0.012f * vm) : 0;
      if (r == 1) c = (hash2(px >> 2, py >> 2, 1915) & 3) ? C(234, 98, 34) : C(255, 176, 70);
      else if (r == 2) c = C(124, 46, 32);
      else if (r == 3) c = C(28, 24, 28);
      else if (h < 0.05f) c = C(88, 80, 76);
      else if (h > 0.992f) c = C(122, 110, 100);
      return true;
    }
    case Eco::CrystalBarrens: {   // violet sand, crystal shards, glints
      c = pick3(n, C(152, 142, 182), C(166, 154, 194), C(178, 166, 204));
      c = mul(c, 0.90f + vnoise(px / 36.0f, py / 36.0f, 1921) * 0.18f);
      if (std::sin(px * 0.33f + py * 0.14f + vnoise(px / 16.0f, py / 16.0f, 171) * 8) > 0.82f) c = mul(c, 0.93f);   // wind ripples
      uint32_t id = 0;
      const int s = stoneAt(px, py, 7, 1923, 0.22f, 0.8f, 1.4f, &id);
      if (s == 4) c = mul(c, 0.80f);
      else if (s) c = s == 3 ? C(214, 206, 250) : s == 1 ? C(92, 76, 150) : C(130, 110, 196);
      else if (h > 0.994f) c = C(248, 252, 255);
      else if (h > 0.990f) c = C(176, 232, 250);
      return true;
    }
    case Eco::PetrifiedForest: {   // faintly banded red-grey ground strewn with chips of stone wood
      c = pick3(n, C(178, 142, 112), C(188, 152, 120), C(198, 162, 128));
      const int band = (int)std::floor((py + (vnoise(px / 40.0f, py / 40.0f, 1931) - 0.5f) * 36.0f) / 7.0f);
      if (hash2(band, 0, 1933) & 1) c = mul(c, 0.94f);
      uint32_t id = 0;
      const int s = stoneAt(px, py, 8, 1935, 0.30f, 0.9f, 1.6f, &id);
      static const uint32_t pb[3] = {C(150, 86, 76), C(126, 122, 132), C(214, 172, 110)};
      const uint32_t base = pb[(id >> 28) % 3];
      if (s == 4) c = mul(c, 0.82f);
      else if (s) c = s == 3 ? lerpc(base, C(250, 236, 210), 0.4f) : s == 1 ? mul(base, 0.72f) : base;
      return true;
    }
    case Eco::Blight: {   // grey-purple dead soil, dark veins with a sickly sheen
      c = pick3(n, C(84, 72, 80), C(92, 78, 86), C(100, 86, 94));
      c = mul(c, 0.92f + vnoise(px / 30.0f, py / 30.0f, 151) * 0.16f);
      if (vnoise(px / 22.0f, py / 22.0f, 1941) > 0.70f) c = C(70, 60, 64);
      const int r = vnoise(px / 40.0f, py / 40.0f, 1947) > 0.40f ? ridgeLine(vnoise(px / 10.0f, py / 10.0f, 1943), 0.015f) : 0;
      if (r == 1) c = C(46, 30, 50);
      else if (r == 2) c = C(128, 70, 110);
      else if (r == 3) c = C(60, 46, 62);
      else {
        const int tf = tuft(px, py, 1945, 0.18f);
        if (tf == 1) c = C(66, 58, 58); else if (tf == 2) c = C(124, 112, 100);
        else if (h > 0.996f) c = C(222, 216, 198);
      }
      return true;
    }
    default: return false;   // the classic ecos (and the sea, the mountains): the classic grounds
  }
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
  // (M3b fixer) a seat's formal water: a kerbed basin, no organic bank
  if (m.kind == MapKind::Overworld && groundWater(real) && isPoolT(m, tx, ty)) return poolPixel(m, px, py);
  // (M4) a diagonal boardwalk: one straight walk over the staircase of its tiles (diagBoardwalkPixel)
  bool bwShade = false;
  if (m.kind == MapKind::Overworld && (real == Ground::Bridge || groundWater(real) || real == Ground::Swamp)) {
    uint32_t o;
    bool openW = false;
    const int r = diagBoardwalkPixel(m, px, py, real, o, openW);
    if (r == 1 || r == 2) return o;
    bwShade = r == 3;
    if (openW) {   // the staircase's corner beside the walk: the marsh round it shows there
      real = Ground::Water;
      for (int k = 0; k < 4; k++) {
        static const int dx4[4] = {0, -1, 1, 0}, dy4[4] = {-1, 0, 0, 1};
        const Ground q = m.at(tx + dx4[k], ty + dy4[k]);
        if (q == Ground::Swamp || groundWater(q)) { real = q; break; }
      }
    }
  }
  if (m.kind == MapKind::Overworld && (real == Ground::Bridge || groundWater(real) || real == Ground::Road)) {
    uint32_t o;
    if (diagBridgePixel(m, px, py, real, o)) return o;
  }
  // organic borders: look the terrain up at a warped position
  Ground g = real;
  int sx = px, sy = py;   // sample position (warped for natural terrain)
  Eco E = Eco::COUNT;     // (M3c) the biome proper the pixel shows (COUNT: its sample tile's, read below)
  // (M3c fixer round 2, review: "lava rifts drawn as separate square tile stamps, veins cut off at the tile edges") an
  // ash field's rift is a smooth field over the lava tiles' centres (as the shore is over the water's), wobbled by two
  // octaves of noise: a lone lava tile is a rounded pool, a run of them one crack whose width swells and pinches, and a
  // crack stepping diagonally stays one channel through the shared corner. lavaV: the field (-1 away from lava).
  float lavaV = -1;
  Ground lavaLand = Ground::Sand;
  if (m.kind == MapKind::Overworld && (real == Ground::Lava || (natural(real) && !groundWater(real) && real != Ground::Rock))) {
    bool nearLava = real == Ground::Lava;
    for (int oy = -1; oy <= 1 && !nearLava; oy++)
      for (int ox = -1; ox <= 1 && !nearLava; ox++) nearLava = m.at(tx + ox, ty + oy) == Ground::Lava;
    if (nearLava) {
      const float fx = (px - 7.5f) / 16.0f, fy = (py - 7.5f) / 16.0f;
      const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
      const float ax = fx - ix, ay = fy - iy;
      auto lv = [&](int x, int y) { return m.at(x, y) == Ground::Lava ? 1.0f : 0.0f; };
      const float w00 = lv(ix, iy), w10 = lv(ix + 1, iy), w01 = lv(ix, iy + 1), w11 = lv(ix + 1, iy + 1);
      float v = (w00 * (1 - ax) + w10 * ax) * (1 - ay) + (w01 * (1 - ax) + w11 * ax) * ay;
      if (w00 == w11 && w10 == w01 && w00 != w10) {
        const float dd = w00 > 0.5f ? std::fabs(ax - ay) : std::fabs(ax + ay - 1.0f);
        v = std::max(v, 1.0f - dd * 0.9f);
      }
      v += (vnoise(px / 5.0f, py / 5.0f, 1971) - 0.5f) * 0.30f + (vnoise(px / 14.0f, py / 14.0f, 1973) - 0.5f) * 0.24f;
      lavaV = v;
      // the land the rift cuts: the nearest tile round it that is not lava
      float bd = 1e9f;
      for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++) {
          const Ground q = m.at(tx + ox, ty + oy);
          if (q == Ground::Lava || !natural(q) || groundWater(q) || q == Ground::Rock) continue;
          const float ddx = (tx + ox) * 16 + 7.5f - px, ddy = (ty + oy) * 16 + 7.5f - py, d = ddx * ddx + ddy * ddy;
          if (d < bd) { bd = d; lavaLand = q; }
        }
    }
  }
  if (lavaV > 0.5f) g = Ground::Lava;
  else if (natural(real)) {
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
    if (m.kind == MapKind::Overworld && isPoolT(m, (int)std::floor(wx / 16), (int)std::floor(wy / 16))) { /* a kerbed basin: its edge is the kerb */ }
    else if (natural(w) && !(isWall(w) != isWall(real) && m.kind != MapKind::Overworld)) { g = w; sx = (int)std::floor(wx); sy = (int)std::floor(wy); }
    // (M2 fixer round 2) a patch of earth in paving (a ruin's floor, a square's worn spot) meets the stones along the
    // same wobbling line, not a tile's square edge
    else if (m.kind == MapKind::Overworld && real == Ground::Dirt && (w == Ground::Plaza || w == Ground::StoneFloor)) g = w;
    // (M1, VISION_PLAN 11.6) ecotones: soft land fades into the neighbouring soft land over a dithered band a few tiles
    // wide (a broad low-frequency warp plus an ordered dither) instead of meeting it along one wobbly line
    // (M3c) between biomes proper, not only families
    Eco ee = Eco::COUNT;
    const Ground eco = m.kind == MapKind::Overworld && ecoGround(g) ? ecotonePixel(m, px, py, g, ee) : Ground::Void;
    if (eco != Ground::Void) {
      E = ee;
      if (eco != g) { g = eco; sx = px; sy = py; }
    } else if (m.kind == MapKind::Overworld && ecoGround(g)) {
      // (fixer r2: a gentler ordered dither, 8 px of jitter instead of 18: the wide checkerboard fringe read as spilled
      // paint; the finer noise carries the edge instead)
      const float ex = px + (vnoise(px / 52.0f, py / 52.0f, 811) - 0.5f) * 64.0f + (bayer(px, py) - 0.5f) * 8.0f + (vnoise(px / 5.0f, py / 5.0f, 813) - 0.5f) * 14.0f;
      const float ey = py + (vnoise(px / 52.0f, py / 52.0f, 817) - 0.5f) * 64.0f + (bayer(px + 2, py + 1) - 0.5f) * 8.0f + (vnoise(px / 5.0f, py / 5.0f, 819) - 0.5f) * 14.0f;
      int etx = (int)std::floor(ex / 16), ety = (int)std::floor(ey / 16);
      // (M4, owner carry-over) the heath and the grass meet along a tight ragged line: the broad warp threw islands of
      // flat bright turf deep into the dark heather (and of heather into the turf)
      if ((ecoT(m, sx >> 4, sy >> 4) == Eco::Heath) != (ecoT(m, etx, ety) == Eco::Heath)) {
        // (fixer M4 r2, review: "an almost vertical hard line") two octaves: a broad meander (so the edge never follows a
        // tile column for long) under the tight ragged fringe; still no islands thrown deep across it
        const float fx = px + (vnoise(px / 30.0f, py / 30.0f, 825) - 0.5f) * 34.0f + (vnoise(px / 9.0f, py / 9.0f, 821) - 0.5f) * 14.0f + (bayer(px, py) - 0.5f) * 3.0f;
        const float fy = py + (vnoise(px / 30.0f, py / 30.0f, 827) - 0.5f) * 34.0f + (vnoise(px / 9.0f, py / 9.0f, 823) - 0.5f) * 14.0f + (bayer(px + 2, py + 1) - 0.5f) * 3.0f;
        etx = (int)std::floor(fx / 16); ety = (int)std::floor(fy / 16);
      }
      const Ground e = m.at(etx, ety);
      if (ecoGround(e)) {
        const Eco eE = ecoT(m, etx, ety);
        if (e != g || eE != ecoT(m, sx >> 4, sy >> 4)) { g = e; E = eE; sx = px; sy = py; }
      }
    }
  } else if (m.kind == MapKind::Overworld && (real == Ground::Plaza || real == Ground::StoneFloor)) {
    // (M2 fixer round 2) paving gives way to the earth, the road or the other paving beside it along a wobbling edge
    // (two octaves), so its patches and its ends are not stepped tile rectangles
    const float wx = px + (vnoise(px / 7.0f, py / 7.0f, 11) - 0.5f) * 7.0f + (vnoise(px / 3.0f, py / 3.0f, 41) - 0.5f) * 2.0f;
    const float wy = py + (vnoise(px / 7.0f, py / 7.0f, 23) - 0.5f) * 7.0f + (vnoise(px / 3.0f, py / 3.0f, 43) - 0.5f) * 2.0f;
    const Ground w = m.at((int)std::floor(wx / 16), (int)std::floor(wy / 16));
    if (w != real && (w == Ground::Dirt || w == Ground::Road || w == Ground::Plaza || w == Ground::StoneFloor)) g = w;
  } else if ((real == Ground::Road || real == Ground::Farmland) && !(m.kind == MapKind::Overworld && paveMatAt(m, tx, ty) == 4)) {
    // roads fray a little at their edges into the surrounding soft ground (fixer r2: a plank deck is cut square)
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
  // (fixer r2) outside the rift's smooth edge the land shows, whatever tile the warp looked up
  if (lavaV >= 0 && lavaV <= 0.5f && g == Ground::Lava) { g = lavaLand; sx = px; sy = py; }
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
      if (m.kind == MapKind::Overworld && isPoolT(m, x, y)) return 0.0f;   // (a kerbed basin keeps its own edge)
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
        int lt = 0;
        for (int oy = -1; oy <= 1; oy++)
          for (int ox = -1; ox <= 1; ox++) {
            Ground q = m.at(tx + ox, ty + oy);
            if (!soft(q) || groundWater(q)) continue;
            float ddx = (tx + ox) * 16 + 7.5f - px, ddy = (ty + oy) * 16 + 7.5f - py;
            float d = ddx * ddx + ddy * ddy;
            if (d < bd) { bd = d; land = q; lt = (oy + 1) * 3 + ox + 1; }
          }
        if (land != Ground::Void) { g = land; sx = px; sy = py; E = ecoT(m, tx + lt % 3 - 1, ty + lt / 3 - 1); }
      }
    }
  }
  // (M3c fixer, review: "sea cliffs have no cliffs") where a sea-cliff coast meets the water, the land stands high over
  // it: on the water south of the coast a rock face drops 17 px from a lit lip to a dark foot (fissured strata in the
  // cliff's own rock), the surf breaking white along its foot; the clifftop carries a bright lip over the drop; a coast
  // facing north, east or west shows its top edge as a dark drop line with the shadow on the water below it. The face
  // follows the organic shoreline (the same smoothed wetness field), so it wanders like the coast.
  if (m.kind == MapKind::Overworld && soft(real) && real != Ground::CaveFloor) {
    auto cliffLand = [&](int x, int y) { const Ground q = m.at(x, y); return !groundWater(q) && q != Ground::Void && q != Ground::Bridge && ecoT(m, x, y) == Eco::SeaCliffs; };
    // (fixer r2: the face is up to 44 px tall, so the tiles up to 3 rows below the clifftop carry it)
    bool nearCliff = cliffLand(tx, ty + 1);
    for (int oy = -3; oy <= 0 && !nearCliff; oy++)
      for (int ox = -1; ox <= 1 && !nearCliff; ox++) nearCliff = cliffLand(tx + ox, ty + oy);
    if (nearCliff) {
      auto wetT = [&](int x, int y) -> float {
        const Ground q = m.at(x, y);
        if (q == Ground::Void) return 1.0f;
        if (isPoolT(m, x, y)) return 0.0f;
        return groundWater(q) || q == Ground::Bridge ? 1.0f : 0.0f;
      };
      auto field = [&](int qx, int qy) -> float {
        const float fx = (qx - 7.5f) / 16.0f, fy = (qy - 7.5f) / 16.0f;
        const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
        const float ax = fx - ix, ay = fy - iy;
        const float w00 = wetT(ix, iy), w10 = wetT(ix + 1, iy), w01 = wetT(ix, iy + 1), w11 = wetT(ix + 1, iy + 1);
        if (w00 == w10 && w00 == w01 && w00 == w11) return w00;
        float v = (w00 * (1 - ax) + w10 * ax) * (1 - ay) + (w01 * (1 - ax) + w11 * ax) * ay;
        if (w00 == w11 && w10 == w01 && w00 != w10) {
          const float dd = w00 > 0.5f ? std::fabs(ax - ay) : std::fabs(ax + ay - 1.0f);
          v = std::max(v, 1.0f - dd * 0.9f);
        }
        return v + (vnoise(qx / 6.0f, qy / 6.0f, 351) - 0.5f) * 0.30f + (vnoise(qx / 19.0f, qy / 19.0f, 353) - 0.5f) * 0.26f;
      };
      const RockPal R = rockPalE(Eco::SeaCliffs);
      const bool hereWet = field(px, py) > 0.5f;
      // (M3c fixer round 2, review: "Sea Cliffs biome has no cliffs ... reads as a grassy lakeshore") the face is two
      // tiles tall and its height wanders along the coast (buttresses stand out, bays fall back), so the drop reads as
      // a cliff and not a kerb; the rock is cut into vertical columns between deep fissures, each column lit on its
      // west side and shaded on its east (top-left light), banded by strata; the turf hangs over the lip in tufts; the
      // foot sinks into a dark wet band, the surf breaks white in broken lines, and the cliff's shadow lies on the sea
      // below it. The east and west coasts show the cliff's side face, 7 px wide.
      const int FH = 26 + (int)(vnoise(px / 9.0f, 3.5f, 1643) * 12.0f);   // 26 .. 37 px
      if (hereWet) {
        int up = 0;
        for (int k = 1; k <= FH + 7; k++)
          if (field(px, py - k) <= 0.5f) { up = k; break; }
        if (up > 0 && cliffLand(px >> 4, (py - up) >> 4)) {
          if (up <= FH) {
            const float t = (up - 1) / (float)(FH - 1);   // 0 at the lip .. 1 at the foot
            // columns 4-7 px wide between fissures, wandering a little down the face
            const int sway = (int)(vnoise(px / 7.0f, up / 9.0f, 1645) * 3.0f);
            const int cw = 4 + (int)(hash2((px + sway) / 6, 0, 1647) % 4);
            const int cx = ((px + sway) % cw + cw) % cw;
            // strata: courses 4-6 px deep, each its own tone
            const int course = (up + (int)(vnoise(px / 11.0f, 0.5f, 1631) * 5.0f)) / 5;
            const uint32_t sh = hash2((px + sway) / cw, course, 1633);
            float l = 0.86f - t * 0.62f + (((sh >> 8) & 255) / 255.0f - 0.5f) * 0.20f + (bayer(px, up) - 0.5f) * 0.06f;
            if (cx == 0) l -= 0.34f;                     // the fissure (deep shade)
            else if (cx == 1) l += 0.16f;                // the column's lit west edge
            else if (cx == cw - 1) l -= 0.12f;           // its shaded east edge
            if ((up + (int)(hash2((px + sway) / cw, 7, 1649) % 5)) % 6 == 0) l -= 0.12f;   // a bedding seam
            uint32_t f = l > 0.70f ? R.hi : l > 0.48f ? R.mid : l > 0.28f ? R.lo : R.dark;
            // the lip: turf hanging over the edge in tufts, then the lit rock rim
            const int tuft = 1 + (int)(hash2(px / 2, 3, 1651) % 3);
            if (up <= tuft) f = up == 1 ? C(92, 120, 70) : C(70, 96, 58);
            else if (up <= tuft + 1) f = R.lip;
            else if (up >= FH - 2) f = mul(R.dark, 0.80f);              // the wet foot
            else if (t > 0.72f) f = lerpc(f, mul(R.dark, 0.9f), (t - 0.72f) * 1.6f);
            if (t > 0.6f && hashf(px, up, 1635) < 0.08f) f = C(62, 84, 66);   // weed at the foot
            return f;
          }
          // below the foot: the surf breaking in broken lines, then the cliff's shadow on the sea
          const int below = up - FH;
          const float foam = vnoise(px / 3.0f, py / 2.0f, 1637);
          if (below <= 2 && foam > 0.25f) return C(232, 244, 250, 235);
          if (below <= 4 && foam > 0.55f) return C(196, 222, 236, 215);
          if (below <= 7) return C(14, 34, 58, below <= 5 ? 120 : 70);
        }
        // a coast running north-south: the cliff's side face over the water, lit where it faces west (toward the light),
        // in shade where it faces east, a dark crack line every few px down it, the surf at its foot
        for (int k = 1; k <= 9; k++) {
          const bool eDry = field(px + k, py) <= 0.5f && cliffLand((px + k) >> 4, py >> 4);
          const bool wDry = field(px - k, py) <= 0.5f && cliffLand((px - k) >> 4, py >> 4);
          if (!eDry && !wDry) continue;
          if (k >= 8) return vnoise(px / 2.0f, py / 3.0f, 1639) > 0.35f ? C(226, 240, 248, 225) : C(160, 200, 220, 200);
          // wDry: land to the west, the face looks east (shade); eDry: the face looks west (lit)
          const uint32_t s = eDry ? (k <= 2 ? R.hi : k <= 5 ? R.mid : R.lo) : (k <= 1 ? R.mid : k <= 4 ? R.lo : R.dark);
          return ((py + (int)(hash2(px / 3, py / 5, 1641) & 3)) % 6) == 0 ? mul(s, 0.80f) : s;
        }
      } else if (cliffLand(px >> 4, py >> 4)) {
        // the clifftop: a bright lip over the drop to the south, a dark edge where the coast faces another way
        if (field(px, py + 1) > 0.5f) return R.lip;
        if (field(px, py + 2) > 0.5f) return lerpc(R.hi, R.lip, 0.5f);
        // a coast facing north: the cliff's top edge stands over the water, a dark outline and a lit rim of rock
        if (field(px, py - 1) > 0.5f || field(px - 1, py) > 0.5f || field(px + 1, py) > 0.5f) return R.dark;
        if (field(px, py - 2) > 0.5f || field(px, py - 3) > 0.5f) return field(px, py - 2) > 0.5f ? R.lip : R.hi;
        if (field(px - 2, py) > 0.5f) return R.hi;
        if (field(px + 2, py) > 0.5f) return R.lo;
      }
    }
  }
  // paved ground (roads, squares) meets soft ground along a smoothed edge too: streets widen and narrow in soft
  // curves instead of whole-tile steps. paveV is the field (-1 away from an edge).
  float paveV = -1;
  // (M3c fixer round 2, review: "Fjordfolk towns: the whole plaza is one flat plank deck with no edge, cut by ragged snow
  // blobs") a people that decks its streets and squares in planks (paving 4) BUILDS them: the deck keeps its tiles'
  // straight edges (no wobbling field), and deckEdge below gives it a rim beam, a front face and a cast shadow
  const bool deckTown = m.kind == MapKind::Overworld && paveMatAt(m, tx, ty) == 4;
  if (m.kind == MapKind::Overworld && !deckTown && (real == Ground::Road || real == Ground::Plaza || (soft(real) && !groundWater(real)))) {
    // (M6 finish fixer, review: "steppe village paths are blocky staircase rectangles") a village's trodden earth paths
    // (Ground::Dirt) take the same organic edge as the paved streets: on grass they were cut tile by tile
    auto pavedG = [](Ground q) { return q == Ground::Road || q == Ground::Plaza || q == Ground::Dirt; };
    auto paved = [&](int x, int y) -> float { Ground q = m.at(x, y); return pavedG(q) || q == Ground::Bridge ? 1.0f : 0.0f; };
    float fx = (px - 7.5f) / 16.0f, fy = (py - 7.5f) / 16.0f;
    int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    float ax = fx - ix, ay = fy - iy;
    float p00 = paved(ix, iy), p10 = paved(ix + 1, iy), p01 = paved(ix, iy + 1), p11 = paved(ix + 1, iy + 1);
    if (!(p00 == p10 && p00 == p01 && p00 == p11)) {
      float v = (p00 * (1 - ax) + p10 * ax) * (1 - ay) + (p01 * (1 - ax) + p11 * ax) * ay;
      // (M3c fixer round 2, review: "detached paving fragments float in the snow round the Sylvan paths") a path stepping
      // diagonally (paved tiles touching only at a corner) pinched to 0.5 at the shared corner, so the noise cut it into
      // a chain of separate islands of a few stones: keep one band along the paved diagonal (as the streams do), and a
      // gentler fine wobble, so the edge frays without throwing off islands
      if (p00 == p11 && p10 == p01 && p00 != p10) {
        const float dd = p00 > 0.5f ? std::fabs(ax - ay) : std::fabs(ax + ay - 1.0f);
        v = std::max(v, 1.0f - dd * 0.9f);
      }
      v += (vnoise(px / 5.0f, py / 5.0f, 391) - 0.5f) * 0.16f + (vnoise(px / 13.0f, py / 13.0f, 393) - 0.5f) * 0.18f;
      paveV = v;
      bool pv = v > 0.5f;
      bool isPaved = pavedG(g);
      if (pv != isPaved) {
        // take the ground of the nearest tile of the other kind
        Ground want = Ground::Void;
        float bd = 1e9f;
        for (int oy = -1; oy <= 1; oy++)
          for (int ox = -1; ox <= 1; ox++) {
            Ground q = m.at(tx + ox, ty + oy);
            bool qp = pavedG(q);
            if (pv ? !qp : (!soft(q) || groundWater(q))) continue;
            float ddx = (tx + ox) * 16 + 7.5f - px, ddy = (ty + oy) * 16 + 7.5f - py;
            float d = ddx * ddx + ddy * ddy;
            if (d < bd) { bd = d; want = q; E = ecoT(m, tx + ox, ty + oy); }
          }
        if (want != Ground::Void) { g = want; sx = px; sy = py; }
      }
    }
  }
  float n = toneField(px, py, 101);
  float h = hashf(px, py, 13);
  int lx = px & 15, ly = py & 15;
  uint32_t c = 0;
  // (M3c) the biome proper's own ground (the classic ecos fall through to the classic grounds below)
  if (E == Eco::COUNT) E = ecoT(m, sx >> 4, sy >> 4);
  // (EMB_BAKE_CLASSIC=1: the classic family grounds only, the A side of the bake-cost A/B in m3c_land_perf.txt)
  static const bool classicOnly = std::getenv("EMB_BAKE_CLASSIC") != nullptr;
  const bool ecoDone = !classicOnly && m.kind == MapKind::Overworld && ecoGround(g) && ecoPixel(E, g, px, py, n, h, c);
  if (!ecoDone) switch (g) {
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
        // (M3c) a track across the wild takes the earth of its biome: red in the badlands, black cinder in the ash
        // fields, pale and dusty in the dry lands, grey-violet in the blight
        if (E == Eco::Badlands || E == Eco::PetrifiedForest) c = lerpc(c, C(180, 104, 70), 0.55f);
        else if (E == Eco::AshFields) c = lerpc(c, C(64, 58, 58), 0.7f);
        else if (E == Eco::Blight) c = lerpc(c, C(96, 82, 90), 0.6f);
        else if (E == Eco::CrystalBarrens) c = lerpc(c, C(150, 138, 170), 0.5f);
        else if (E == Eco::SaltFlats) c = lerpc(c, C(206, 200, 186), 0.6f);
        else if (bb == Biome::Desert || E == Eco::Savanna || E == Eco::Steppe) c = lerpc(c, C(184, 154, 112), 0.4f);
        else if (E == Eco::PeatBog || E == Eco::DarkForest) c = lerpc(c, C(70, 56, 46), 0.5f);
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
      // (M3 fixer) a settlement's streets in its people's paving
      if (m.kind == MapKind::Overworld) {
        const int pm = paveMatPx(m, px, py);
        if (pm > 0) {
          // (M3 fixer round 2) where a street opens onto its square the two pavings part along a wobbling line, not a
          // tile edge, and the empire kerbs its basalt road in pale stone there
          int sq = 0;
          const uint32_t sp = streetSquareSeam(m, px, py, pm, true, sq);
          if (sq == 1) { const uint32_t pc = pavePixel(pm, false, px, py, 0.0f, -1.0f, h, n); if (pc) { c = pc; break; } }
          if (sq == 2 && sp) { c = sp; break; }
          const uint32_t pc = pavePixel(pm, true, px, py, e, paveV, h, n);
          if (pc) { c = pc; break; }
        }
      }
      float wear = vnoise(px / 7.0f, py / 7.0f, 211) * 0.8f + e * 0.75f;
      if (wear > 0.72f) c = earth;
      else if (wear > 0.64f && mortar) c = earth;
      break;
    }
    case Ground::Plaza: {
      // (M3 fixer) a settlement's squares in its people's paving (Map::PAVE_MARK); 0 (heartland) keeps the flags below
      if (m.kind == MapKind::Overworld) {
        const int pm = paveMatPx(m, px, py);
        if (pm > 0) {
          int sq = 0;
          const uint32_t kc = streetSquareSeam(m, px, py, pm, false, sq);
          if (sq == 3 && kc) { c = kc; break; }
          if (sq == 1) { const uint32_t pc = pavePixel(pm, true, px, py, 0.0f, -1.0f, h, n); if (pc) { c = pc; break; } }
          const uint32_t pc = pavePixel(pm, false, px, py, 0.0f, paveV, h, n);
          if (pc) { c = pc; break; }
        }
      }
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
      if (m.kind == MapKind::Overworld && isBoardwalk(m, tx, ty)) { c = boardwalkPixel(m, px, py); break; }   // (M3 fixer)
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
      // (M3b forts) a settlement's bridge in its culture's form (bld::bridgeOfPaving, from the paving it lays): a stone
      // arch's deck of setts between parapets, a causeway of great slabs with a low kerb, the elves' white bridge with a
      // pale balustrade, the jade kingdoms' red-railed bridge, the wood elves' deck grown over with moss between living
      // rails; the plank bridge stays the planks above
      {
        const int bpm = m.kind == MapKind::Overworld ? paveMatAt(m, tx, ty) : -1;
        const bld::BridgeForm bf = bpm > 0 ? bld::bridgeOfPaving(bpm) : bld::BridgeForm::Planks;
        int ee = (sideA && e < 3) ? e : ((sideB && e > 12) ? 15 - e : 99);   // px from an open side
        // (M3b fixer round 3) a town's stone bridge keeps its parapet wherever the river meets its deck, on any side:
        // a river crossing a square on a slant reached the deck's corner where the street joins it, the rail rule
        // above left it railless, and the water seemed to stop dead at a slab of paving
        if (bf != bld::BridgeForm::Planks) {
          if (groundWater(m.at(tx - 1, ty))) ee = std::min(ee, lx);
          if (groundWater(m.at(tx + 1, ty))) ee = std::min(ee, 15 - lx);
          if (groundWater(m.at(tx, ty - 1))) ee = std::min(ee, ly);
          if (groundWater(m.at(tx, ty + 1))) ee = std::min(ee, 15 - ly);
        }
        switch (bf) {
          case bld::BridgeForm::StoneArch: case bld::BridgeForm::Causeway: {
            static const uint32_t stoneOf[10] = {C(150, 146, 140), C(204, 192, 164), C(150, 120, 84), C(214, 176, 124), C(130, 104, 76),
                                                 C(146, 152, 136), C(222, 222, 226), C(206, 194, 166), C(142, 148, 156), C(156, 82, 62)};
            const uint32_t st = stoneOf[std::clamp(bpm, 0, 9)];
            const bool cause = bf == bld::BridgeForm::Causeway;
            const Ashlar sl = cause ? ashlarAt(eastWest ? py : px, eastWest ? px : py, 1501, 14, 4) : runningAt(eastWest ? py : px, eastWest ? px : py, 1503, 5, 6, 1);
            c = (sl.in == 0 || sl.iy == 0) ? mul(st, 0.74f) : mul(st, 0.92f + ((sl.id >> 8) & 255) / 255.0f * 0.14f);
            if (sl.in == 1 || sl.iy == 1) c = mul(c, 1.05f);
            if (ee <= (cause ? 1 : 3)) {   // the parapet (a causeway: a low kerb): its outer face dark, its coping lit
              const bool joint = ((along % 9) + 9) % 9 == 0;
              c = ee == 0 ? mul(st, 0.52f) : (ee == 1 ? mul(st, joint ? 0.86f : 1.12f) : mul(st, joint ? 0.78f : 1.0f));
              if (ee == 3) c = mul(st, 0.70f);   // the parapet's shadow on the deck
            }
            break;
          }
          case bld::BridgeForm::MoonArch: {   // white stone, a pale balustrade on small posts
            const Ashlar sl = runningAt(eastWest ? py : px, eastWest ? px : py, 1505, 6, 9, 2);
            c = (sl.in == 0 || sl.iy == 0) ? C(176, 180, 190) : mul(C(224, 226, 232), 0.94f + ((sl.id >> 8) & 255) / 255.0f * 0.08f);
            if (ee <= 2) {
              const bool postP = ((along % 7) + 7) % 7 < 2;
              c = ee == 0 ? C(120, 128, 150) : (postP ? C(246, 248, 252) : (ee == 1 ? C(196, 204, 222) : C(150, 156, 176)));
            }
            break;
          }
          case bld::BridgeForm::Covered: {   // the planks between red lacquered rails on black posts
            if (ee <= 2) {
              const bool postP = ((along % 8) + 8) % 8 < 2;
              c = ee == 0 ? C(60, 20, 24) : (postP ? C(40, 30, 34) : (ee == 1 ? C(196, 60, 48) : C(120, 30, 32)));
            }
            break;
          }
          case bld::BridgeForm::Living: {   // moss on the planks, living rails in leaf
            if (vnoise(px / 6.0f, py / 6.0f, 1507) > 0.62f) c = lerpc(c, C(92, 128, 60), 0.55f);
            if (ee <= 2) c = ee == 0 ? C(40, 52, 36) : (hash2(px, py, 1509) % 3 == 0 ? C(120, 166, 70) : (ee == 1 ? C(116, 96, 68) : C(70, 100, 52)));
            break;
          }
          default: break;
        }
      }
      // (M3, owner carry-over 5) stone abutments where the deck lands on a bank: the last 6 px of the deck at each end
      // that meets dry land are dressed stone courses (lit on their west / north faces), a step up from the planks with
      // a dark joint, and a low parapet stone at each corner where the rails end
      if (m.kind == MapKind::Overworld) {
        auto land = [&](int x, int y) { const Ground q = m.at(x, y); return q != Ground::Bridge && !groundWater(q) && q != Ground::Void; };
        const int across = eastWest ? ly : lx;
        int endD = 99;   // px from the end of the deck that sits on land
        if (eastWest) { if (land(tx - 1, ty)) endD = lx; if (land(tx + 1, ty)) endD = std::min(endD, 15 - lx); }
        else { if (land(tx, ty - 1)) endD = ly; if (land(tx, ty + 1)) endD = std::min(endD, 15 - ly); }
        if (endD < 6) {
          const int course = across / 4, run = eastWest ? px : py;
          uint32_t s = lerpc(C(156, 150, 140), C(176, 170, 156), hashf(((run + course * 3) / 6), course, 271));
          if (across % 4 == 3 || ((run + course * 3) % 6 + 6) % 6 == 0) s = C(112, 104, 104);   // mortar joints
          else if (across % 4 == 0) s = mul(s, 1.1f);                                         // each course's lit edge
          if (endD == 5) s = C(70, 54, 46);                              // the joint where the planks meet the stone
          else if (endD == 4) s = mul(s, 1.12f);
          const bool corner = (across < 3 && sideA) || (across > 12 && sideB);
          if (corner) s = across == 0 || across == 15 ? C(78, 72, 80) : C(184, 178, 166);   // the parapet stones
          c = s;
        }
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
      } else if (m.kind == MapKind::Interior) {
        c = C(0, 0, 0);   // (M3 fixer) beyond the room's walls: the same flat black as past the map's edge (it showed
                          // as faint lighter rectangles round the room)
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
      // (M3c) off a coral strand the water runs turquoise over the white sand (a smooth field between tile centres)
      if (m.kind == MapKind::Overworld) {
        const float fx = (px - 7.5f) / 16.0f, fy = (py - 7.5f) / 16.0f;
        const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
        const float ax = fx - ix, ay = fy - iy;
        const float c00 = coralNear(m, ix, iy), c10 = coralNear(m, ix + 1, iy), c01 = coralNear(m, ix, iy + 1), c11 = coralNear(m, ix + 1, iy + 1);
        const float cv = (c00 * (1 - ax) + c10 * ax) * (1 - ay) + (c01 * (1 - ax) + c11 * ax) * ay;
        if (cv > 0) {
          const int a = (int)(c >> 24);
          c = lerpc(c, deep ? C(30, 128, 166, a) : shore > 0 ? C(128, 228, 216, a) : C(56, 196, 200, a), cv * 0.85f);
        }
      }
      if (shore > 0 && shoreV < 0 && hashf(px / 2, py / 2, 341) < 0.5f) c = C(220, 240, 250, 210);
      // (M1 round 3) a bridge's deck throws its shadow down-right onto the water under it
      if (m.kind == MapKind::Overworld && (m.at((px - 2) >> 4, (py - 4) >> 4) == Ground::Bridge || m.at((px - 1) >> 4, (py - 2) >> 4) == Ground::Bridge))
        c = C(18, 30, 62, 200);
      // (M3b fixer round 3) where the river runs in under a town's stone span from the side (a deck east or west of
      // the water), the shade of the arch's mouth darkens the water for a few px before it: the river visibly passes
      // under the bridge instead of stopping at the paving
      if (m.kind == MapKind::Overworld) {
        int dEdge = 99;
        if (m.at(tx + 1, ty) == Ground::Bridge && !isBoardwalk(m, tx + 1, ty) && paveMatAt(m, tx + 1, ty) > 0) dEdge = 15 - lx;
        if (m.at(tx - 1, ty) == Ground::Bridge && !isBoardwalk(m, tx - 1, ty) && paveMatAt(m, tx - 1, ty) > 0) dEdge = std::min(dEdge, lx);
        if (dEdge < 5) c = dEdge < 2 ? C(10, 18, 40, 220) : (dEdge < 4 ? C(18, 34, 70, 205) : C(28, 56, 104, 190));
      }
      // (M3, owner carry-over 5) under an east-west deck the viewer sees its south face: the timber beam along the
      // deck's edge, and stone piers standing in the river every few paces (lit west side, a ripple at the foot)
      if (m.kind == MapKind::Overworld && m.at(tx, ty - 1) == Ground::Bridge && !isBoardwalk(m, tx, ty - 1) &&
          (m.at(tx - 1, ty - 1) == Ground::Bridge || m.at(tx + 1, ty - 1) == Ground::Bridge) && m.at(tx, ty - 2) != Ground::Bridge) {
        if (ly == 0) c = C(118, 80, 48);
        else if (ly == 1) c = C(66, 42, 30);
        else {
          const int col = ((px % 40) + 40) % 40;
          if (col >= 17 && col <= 23 && ly <= 8) {
            const int k = col - 17;
            uint32_t s = k <= 1 ? C(176, 170, 160) : (k >= 5 ? C(92, 88, 96) : C(140, 134, 128));
            if (ly == 2) s = C(60, 50, 50);                  // in the shade of the deck
            if (ly % 3 == 1 && k > 1 && k < 6) s = mul(s, 0.86f);   // courses
            if (ly == 8) s = (k & 1) ? C(220, 238, 248) : C(150, 190, 214);   // the river breaks on it
            c = s;
          }
        }
      }
      break;
    }
    case Ground::Ice: {
      // (M3c) a frozen lake: clear blue ice, the deep water showing through in darker patches, white cracks with a dark
      // hairline on their shaded side, and snow laid over it in wind-cut drifts (a glacier's ice is bluer and cut by
      // crevasses instead)
      const bool glacier = m.kind == MapKind::Overworld && E == Eco::Glacier;
      c = glacier ? pick3(n, C(132, 178, 222), C(150, 192, 230), C(168, 206, 238)) : pick3(n, C(150, 194, 226), C(166, 206, 234), C(182, 218, 242));
      c = mul(c, 0.92f + vnoise(px / 18.0f, py / 18.0f, 1951) * 0.14f);
      if (glacier) {
        // (M3c fixer round 3, review: "cracked blobby ice meets the flowing crevasse texture along a straight line ...
        // the whole field is very busy, camouflage-like") a glacier's bare ice carries the SAME crevasse field as the
        // snow over it (ecoPixel Eco::Glacier: the warped, turned frame, seeds 1805..1809), so a crevasse runs on
        // unbroken from the snow onto the ice and the two grounds read as one ice sheet; the frozen-lake cell cracks
        // and the extra drifts are left to the lakes (two patterns laid over each other made the camouflage)
        const float wx = px + (vnoise(px / 37.0f, py / 37.0f, 1807) - 0.5f) * 26.0f, wy = py + (vnoise(px / 37.0f, py / 37.0f, 1809) - 0.5f) * 26.0f;
        const float u = wx * 0.92f + wy * 0.38f, v = wy * 0.92f - wx * 0.38f;
        const float d = vnoise(u / 44.0f, v / 12.0f, 1805) - 0.5f;
        if (std::fabs(d) < 0.012f) c = C(44, 76, 136);
        else if (d >= 0.012f && d < 0.032f) c = mul(c, 0.82f);
        else if (d <= -0.012f && d > -0.026f) c = C(236, 246, 255);
        if (h < 0.006f) c = C(255, 255, 255);
        break;
      }
      const int r = ridgeLine(vnoise(px / 13.0f, py / 13.0f, 1953), 0.016f);
      if (r == 1) c = C(238, 248, 255);
      else if (r == 3) c = mul(c, 0.82f);
      if (m.kind == MapKind::Overworld) {
        // (M3c fixer) the drifts read through a warped, turned frame (on the raw lattice they lay in rows of lozenges)
        const float wx = px + (vnoise(px / 41.0f, py / 41.0f, 1957) - 0.5f) * 30.0f, wy = py + (vnoise(px / 41.0f, py / 41.0f, 1959) - 0.5f) * 30.0f;
        const float d = vnoise((wx * 0.9f + wy * 0.44f) / 34.0f, (wy * 0.9f - wx * 0.44f) / 13.0f, 1955) + (bayer(px, py) - 0.5f) * 0.06f;
        if (d > (glacier ? 0.62f : 0.68f)) c = d > (glacier ? 0.68f : 0.74f) ? C(236, 242, 250) : C(208, 222, 240);
      }
      if (h < 0.012f) c = C(255, 255, 255);
      break;
    }
    case Ground::Lava: {
      // (M3c) an ash field's rift: molten rock under a cooling black crust, plates parted by glowing seams that widen
      // where the melt runs open, embers flecking the crust
      // (fixer r2) its depth into the rift (the smooth field above): a black lip of chilled rock along the edge, then
      // the glowing rim, the melt opening wider toward the heart of the crack
      const float dep = lavaV >= 0 ? std::clamp((lavaV - 0.5f) / 0.40f, 0.0f, 1.0f) : 1.0f;
      if (dep < 0.10f) { c = C(30, 22, 26); break; }
      if (dep < 0.24f) { c = (bayer(px, py) < (dep - 0.10f) / 0.14f) ? C(150, 54, 32) : C(96, 34, 28); break; }
      const float run = vnoise(px / 30.0f, py / 30.0f, 1961);
      const float f = std::fabs(vnoise(px / 9.0f, py / 9.0f, 1963) - 0.5f);
      const float open = 0.04f + run * 0.16f + dep * 0.12f;
      if (f < open) c = f < open * 0.45f ? C(255, 220, 110) : (f < open * 0.75f ? C(252, 156, 48) : C(214, 84, 30));
      else {
        c = pick3(n, C(40, 28, 32), C(54, 36, 36), C(68, 44, 40));
        if (f < open + 0.02f) c = C(116, 40, 30);   // the crust's hot edge
        else if (h < 0.03f) c = C(150, 54, 32);
      }
      break;
    }
    default:
      c = C(0, 0, 0);
      break;
  }
  // (fixer r2) the plank deck's thickness: a dark joint and a rim beam along its edge (lit where it faces the light,
  // north and west), the deck's front face on the ground below its south edge, and its shadow on the ground south and
  // east of it (top-left light)
  if (deckTown) {
    auto deck = [&](int x, int y) { const Ground q = m.at(x, y); return q == Ground::Road || q == Ground::Plaza; };
    const int lx2 = px & 15, ly2 = py & 15;
    if (deck(tx, ty) && (g == Ground::Road || g == Ground::Plaza)) {
      const int dN = deck(tx, ty - 1) ? 99 : ly2, dS = deck(tx, ty + 1) ? 99 : 15 - ly2, dW = deck(tx - 1, ty) ? 99 : lx2, dE = deck(tx + 1, ty) ? 99 : 15 - lx2;
      const int d = std::min(std::min(dN, dS), std::min(dW, dE));
      if (d <= 3) {
        const bool lit = d == dN || d == dW;
        const bool alongX = d == dN || d == dS;
        const int run = alongX ? px : py;
        if (d == 0) c = lit ? C(150, 122, 88) : C(70, 52, 40);
        else if (d == 3) c = C(58, 44, 34);                                              // the joint inside the beam
        else c = ((run % 19 + 19) % 19 == 0) ? C(66, 50, 38) : (lit ? (d == 1 ? C(170, 140, 100) : C(146, 118, 84)) : (d == 1 ? C(116, 92, 66) : C(104, 82, 60)));
        if (d >= 1 && d <= 2 && ((run % 19 + 19) % 19) == 9 && ((alongX ? py : px) & 1)) c = C(74, 70, 66);   // a nail head
      }
    } else if (!deck(tx, ty) && !groundWater(real)) {
      // below the south edge: the deck's front face (3 px of plank ends), then its shadow; east of it: the shadow
      const bool northDeck = deck(tx, ty - 1), westDeck = deck(tx - 1, ty), nwDeck = deck(tx - 1, ty - 1);
      if (northDeck && ly2 < 3) c = ly2 == 2 ? C(48, 36, 30) : ((px % 5 + 5) % 5 == 0 ? C(60, 46, 36) : C(92, 70, 52));
      else if ((northDeck && ly2 < 7) || (westDeck && lx2 < 4) || (nwDeck && lx2 < 4 && ly2 < 7)) c = lerpc(mul(c, 0.70f), C(48, 34, 92, (int)(c >> 24)), 0.14f);
    }
  }
  // (M3 fixer) the marsh under and beside a boardwalk: its south face, its posts and its shade
  if (m.kind == MapKind::Overworld && (groundWater(g) || g == Ground::Swamp) && real != Ground::Bridge && !bwShade &&
      !boardwalkDiagAt(m, tx, ty - 1).diag && !boardwalkDiagAt(m, tx - 1, ty).diag && !boardwalkDiagAt(m, tx - 1, ty - 1).diag) {
    const uint32_t u = boardwalkUnder(m, px, py, c);
    if (u) c = u;   // (the beam and posts are opaque; the shade keeps the water's own alpha)
  }
  if (bwShade) c = lerpc(mul(c, 0.66f), C(30, 26, 60, (int)(c >> 24)), 0.12f);   // (M4) a diagonal walk's shade
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
    // (M3 fixer) only a north-south run's own tiles (a wall neighbour above or below, none beside): a diagonal run's
    // staircase tiles threw a square block of shade each, a stepped chain of squares beside the slanted wall (its
    // slanted shadow comes from bakeArchShadows, which follows the art's own outline)
    auto nsRun = [&](int wx, int wy) {
      return m.wallAt(wx, wy) && (m.wallAt(wx, wy - 1) || m.wallAt(wx, wy + 1)) && !m.wallAt(wx - 1, wy) && !m.wallAt(wx + 1, wy);
    };
    for (int k = 1; k <= 10 && !dw; k++) if (nsRun((px - k) >> 4, ty) && m.wallAt((px - k) >> 4, (py - 6) >> 4)) dw = k;
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
// (M3c) style: 0 rock columns (or the bank's earth layers), 1 banded strata (badlands, petrified forest), 2 blue ice
// (glacier, frozen lakes), 3 black basalt in narrow columns (ash fields), 4 white chalk with flint bands (chalk downs)
struct FacePal { RockPal p; bool bank; int style = 0; };
FacePal facePal(Biome b, int level) {
  switch (b) {
    case Biome::Plains: case Biome::Forest: case Biome::Autumn: case Biome::Beach:
      if (level <= 3) return {{C(206, 178, 126), C(170, 132, 88), C(140, 104, 70), C(108, 78, 56), C(70, 50, 44), false}, true};
      return {{C(198, 188, 164), C(156, 146, 128), C(128, 118, 104), C(98, 90, 82), C(62, 56, 56), false}, false};
    default: return {rockPal(b), false};
  }
}
// (M3c) the face of a step by biome proper: the wondrous and rocky lands show their own rock, the rest their family's
FacePal facePalE(Eco e, Biome b, int level) {
  switch (e) {
    case Eco::Badlands: case Eco::PetrifiedForest: return {rockPalE(e), false, 1};
    case Eco::Glacier: case Eco::FrozenLakes: return {rockPalE(e), false, 2};
    case Eco::AshFields: return {rockPalE(e), false, 3};
    case Eco::ChalkDowns: return {rockPalE(e), false, 4};
    case Eco::CrystalBarrens: case Eco::Blight: case Eco::SeaCliffs: return {rockPalE(e), false, 0};
    case Eco::Heath: case Eco::StonePlains: case Eco::Tundra: if (level >= 2) return {rockPalE(e), false, 0}; break;
    default: break;
  }
  return facePal(b, level);
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
  const FacePal FP = facePalE(ecoT(m, tx, ty), bio, l);
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
    // (M3c fixer round 3, review: "snow cover on city paving is a flat untextured white fill with stair-stepped edges")
    // paving and trodden earth are no longer cut out tile by tile (their square holes were the staircase): they take the
    // cover too, but swept: the threshold rises smoothly with a paving field blended between tile centres, so the snow
    // thins out across the kerb onto the cobbles in a ragged drift line, and the cover itself is drawn with wind-cut
    // drifts, blue hollows and a glitter of ice
    if (g != Ground::Bridge && g != Ground::StoneFloor && !snowy) {
      auto snowV = [&](int x, int y) -> float {
        const Biome b = m.biomeAt(x, y);
        int at = b == Biome::Mountain ? 5 : (b == Biome::Snow || b == Biome::Taiga) ? 4 : 99;
        if (at == 4) {   // (M3c) the open tundra and the bogs keep their lichen and moss up to the high ground
          const Eco e = ecoT(m, x, y);
          if (e == Eco::Tundra || e == Eco::TaigaBog) at = 6;
        }
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
        auto sweptT = [&](int x, int y) -> float {
          const Ground q = m.at(x, y);
          return q == Ground::Road || q == Ground::Plaza || q == Ground::Bridge ? 1.0f : (q == Ground::Dirt || q == Ground::Farmland) ? 0.6f : 0.0f;
        };
        float swept;
        {
          const float fx = (px - 7.5f) / 16.0f, fy = (py - 7.5f) / 16.0f;
          const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
          const float ax = fx - ix, ay = fy - iy;
          swept = (sweptT(ix, iy) * (1 - ax) + sweptT(ix + 1, iy) * ax) * (1 - ay) + (sweptT(ix, iy + 1) * (1 - ax) + sweptT(ix + 1, iy + 1) * ax) * ay;
        }
        // (M6 finish fixer, review: "snow drift blobs lie on the paving of a green, rainy imperial capital and turn into
        // dark-blue patches at night") a mountain town's snowline cover is the high ground's, not the season's: where
        // the land itself is not cold (a Mountain tile, not the snow lands or the taiga) the townsfolk keep their streets
        // and squares swept, so the cover stops at the kerb in the same ragged swept line instead of lying in blobs on
        // the flags while the gardens beside them are green; in the truly cold lands the streets are swept too, only
        // the deepest cover (the high ground's) lies on the flags
        const Biome hb = m.biomeAt(tx, ty);
        const bool coldLand = hb == Biome::Snow || hb == Biome::Taiga;
        const float th = 0.5f + swept * (coldLand ? 0.95f : 2.5f) + (bayer(px, py) - 0.5f) * 0.12f;
        if (k > th) {
          // the cover: soft wind-cut drifts aslant (lit crests, blue hollows), a glitter of ice, and a shaded rim where
          // it thins out
          const float dr = vnoise((px * 0.92f + py * 0.38f) / 30.0f, (py * 0.92f - px * 0.38f) / 9.0f, 837);
          c = dr > 0.62f ? C(246, 249, 254) : dr < 0.34f ? C(214, 224, 240) : (vnoise(px / 6.0f, py / 6.0f, 833) < 0.5f ? C(230, 236, 246) : C(236, 241, 250));
          if (dr > 0.58f && dr <= 0.62f) c = C(222, 230, 244);   // the drift's shaded lee under its crest
          if (hashf(px, py, 839) < 0.012f) c = C(255, 255, 255);
          if (k < th + 0.05f) c = C(196, 206, 224);              // the thin, trodden edge of the cover
          else if (k < th + 0.10f) c = lerpc(c, C(206, 216, 234), 0.6f);
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
  // (M3c fixer round 3, review: "a brown cliff ribbon runs diagonally through the middle of the lake, with water on both
  // sides") one sheet of water may lie over tiles of two levels (a lake district's lake across a relief step): water
  // has no cliff in it. A water pixel whose every neighbouring tile of another level is water too draws no face, rim or
  // lip: the lake reads as one surface (a step to dry land keeps its cliff, on the land's side)
  if (water && groundWater(m.at(tx, ty))) {
    bool allWet = true;
    for (int oy = -2; oy <= 1 && allWet; oy++)
      for (int ox = -1; ox <= 1; ox++) {
        if ((int)lc.lv[(2 + oy) * 6 + 2 + ox] == l) continue;
        if (!groundWater(m.at(tx + ox, ty + oy))) { allWet = false; break; }
      }
    if (allWet) return c;
  }
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
    // (M3c) the biomes' own rock faces
    if (FP.style == 1) {   // banded strata: red, ochre and cream layers, fluted by the rain
      const int yy = py + (int)std::lround((vnoise(px / 13.0f, py / 6.0f, 921) - 0.5f) * 4.0f);
      const int band = yy >= 0 ? yy / 3 : (yy - 2) / 3;
      static const uint32_t BAND[5] = {C(226, 184, 134), C(196, 112, 72), C(170, 88, 60), C(214, 146, 96), C(150, 74, 56)};
      r = lerpc(r, mul(BAND[hash2(band, 0, 923) % 5], 0.86f + (1 - t) * 0.22f), 0.55f);
      const int fl = px + (int)(hash2(px / 4, band / 3, 925) % 2);
      if (fl % 5 == 0) r = mul(r, 0.84f);          // the rain's flutes
      else if (fl % 5 == 1) r = mul(r, 1.06f);
      return r;
    }
    if (FP.style == 2) {   // ice: smooth blue, vertical melt streaks, glints, a few dark cracks
      if (hash2(px / 2, 0, 927) % 4 == 0) r = mul(r, 0.92f);
      if (hashf(px, py, 929) < 0.025f) r = C(250, 254, 255);
      else if (((px * 3 + py * 2) % 23) == 0 && hashf(px / 5, py / 9, 931) < 0.3f) r = lerpc(r, P.dark, 0.6f);
      return r;
    }
    if (FP.style == 4) {   // chalk: soft white beds, a band of black flint nodules here and there
      const int band = py >= 0 ? py / 4 : (py - 3) / 4;
      r = mul(r, 0.95f + hashf(band, px / 23, 933) * 0.08f);
      if (band % 3 == 0 && hashf(px / 2, band, 935) < 0.35f) r = C(70, 70, 82);
      return r;
    }
    const int u = px + (int)std::lround((vnoise(px * 0.21f, py * 0.17f, 893) - 0.5f) * 5.0f);
    const int cw = FP.style == 3 ? 4 : 6;   // (basalt stands in narrow columns)
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
    if (cover && ds <= RH) {
      // (M3, owner carry-over 6) a snowfield's north-facing drop read as a thin rim: the plateau's snow ends in a
      // bright cornice with a cool underside, then a short band of its rock face in shadow (it faces away from the
      // light), a little snow caught on its ledges and a crisp foot, so the step reads as rock under snow like every other cliff
      const int lipTop = RH - 6;
      if (ds <= lipTop) { out = regrade(c, up); opaque = !water; }
      else if (ds == lipTop + 1) { out = C(252, 253, 255); opaque = true; }
      else if (ds == lipTop + 2) { out = lerpc(regrade(c, up), C(170, 186, 222), 0.45f); opaque = true; }
      else if (ds < RH) {
        const float t = 0.55f + 0.4f * (float)(ds - lipTop - 3) / 3.0f;
        uint32_t r = mul(faceRock(t, ds), 0.74f);
        if (ds == RH - 1) r = mul(r, 0.85f);                                     // into the shade at its foot
        else if (ds == lipTop + 3 && (hash2(px >> 1, py >> 4, 871) % 5) == 0) r = C(196, 210, 236);   // snow caught on a ledge
        out = r; opaque = true;
      } else { out = outline; opaque = true; }
    } else if (ds <= RH - 3) { out = regrade(c, up); opaque = !water; }
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
  // (M3c LAND) EMB_BAKEBENCH=1: every bake runs alone (one at a time, so the bakers never share the CPU) and prints its
  // time with the chunk's commonest biome ("bakebench: <eco key> <ms>"); the phone budget check (tools/scripts/m3c_land_perf.txt)
  static const bool bench = std::getenv("EMB_BAKEBENCH") != nullptr;
  if (bench) {
    static std::mutex benchMu;
    std::lock_guard<std::mutex> lk(benchMu);
    const auto t0 = std::chrono::steady_clock::now();
    c = Canvas(CH * 16, CH * 16);
    bakeRows(j, c, 0, c.h);
    bakeFinish(j, c);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    int cnt[(int)Eco::COUNT] = {};
    for (int y = kBakeMargin; y < kBakeMargin + CH; y += 2)
      for (int x = kBakeMargin; x < kBakeMargin + CH; x += 2) cnt[(int)j.map->ecoAt(x, y)]++;
    const int best = (int)(std::max_element(cnt, cnt + (int)Eco::COUNT) - cnt);
    std::printf("bakebench: %s %.2f\n", ecoInfo((Eco)best).key, ms);
    std::fflush(stdout);
    return;
  }
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
    // (M3b forts) the shadow of the building's blueprint: every volume (the body, wings, towers, porches, a compound's
    // tents and enclosure walls) cast from its own footprint (a box, a round, an octagon) and its own height, so a
    // yurt casts a round shadow, an L its L, a tower a long one, a compound's low wall a short one; all to the lower
    // right (light from the top-left), a contact shade along the foot of the south faces. The volumes are rasterised
    // once into a map of shadow lengths over the building's box, then each ground pixel looks up-left along the light.
    const bld::Blueprint bp = bldgBlueprint(b);
    const int hx0 = fx - art::BLDG_PAD_X, hy0 = fy - 4, hw = fw + 2 * art::BLDG_PAD_X, hh = fh + 4 + art::BLDG_PAD_B;
    std::vector<uint8_t> hm((size_t)hw * hh, 0);   // per footprint pixel: 1 + the shadow length (px) of its tallest volume
    int Lmax = 0;
    for (const bld::Volume& v : bp.vols) {
      if (v.role == bld::VolRole::Chimney) continue;
      const int dep = std::max(1, (int)v.y1 - (int)v.y0), wid = std::max(1, (int)v.x1 - (int)v.x0);
      int roofH;
      switch (v.roof) {
        case art::RoofShape::FlatParapet: roofH = 3; break;
        case art::RoofShape::Dome: case art::RoofShape::Onion: roofH = std::min(wid, dep) / 2; break;
        case art::RoofShape::Conical: case art::RoofShape::Tent: roofH = std::min(wid, dep) / 2 + 2; break;
        case art::RoofShape::Spire: roofH = std::min(wid, dep); break;
        default: roofH = 4 + (int)v.pitch * std::min(wid, dep) / 8; break;
      }
      if (v.role == bld::VolRole::Enclosure) roofH = 0;
      const int top = (int)v.z0 + (int)v.wallH + roofH;
      const int L = std::clamp(top / 4, v.role == bld::VolRole::Enclosure ? 2 : 4, 16);
      Lmax = std::max(Lmax, L);
      const float rx = wid * 0.5f, ry = dep * 0.5f, cxv = v.x0 + rx, cyv = v.y0 + ry;
      for (int y = std::max(0, (int)v.y0 + fy - hy0); y < std::min(hh, (int)v.y1 + fy - hy0); y++)
        for (int x = std::max(0, (int)v.x0 + fx - hx0); x < std::min(hw, (int)v.x1 + fx - hx0); x++) {
          if (v.shape != bld::VolShape::Box) {
            const float dx = (x + hx0 - fx + 0.5f - cxv) / rx, dy = (y + hy0 - fy + 0.5f - cyv) / ry;
            if (v.shape == bld::VolShape::Round ? dx * dx + dy * dy > 1.0f : std::fabs(dx) + std::fabs(dy) > 1.45f) continue;
          }
          uint8_t& q = hm[(size_t)y * hw + x];
          q = (uint8_t)std::max<int>(q, L + 1);
        }
    }
    if (!Lmax) {   // (a blueprint always has its body; the footprint as a fallback)
      Lmax = std::clamp(art::buildingHeight(bp) / 4, 6, 14);
      for (int y = fy - hy0; y < fy - hy0 + fh; y++)
        for (int x = fx - hx0; x < fx - hx0 + fw; x++) hm[(size_t)y * hw + x] = (uint8_t)(Lmax + 1);
    }
    auto H = [&](int x, int y) -> int {
      x -= hx0; y -= hy0;
      return x < 0 || y < 0 || x >= hw || y >= hh ? 0 : hm[(size_t)y * hw + x];
    };
    auto inFoot = [&](int x, int y) { return H(x, y) > 0; };
    // 1 cast: something tall enough lies up-left along the light; 2 contact: just south of a south face
    auto castAt = [&](int px, int py) -> int {
      if (inFoot(px, py - 1) || inFoot(px, py - 2)) return 2;
      for (int d = 1; d <= Lmax; d++) {
        const int q = H(px - d, py - (d * 3 + 2) / 5);
        if (q > d) return 1;
      }
      return 0;
    };
    const int LyMax = (Lmax * 3 + 2) / 5;
    for (int py = std::max(y0, hy0); py < std::min(y1, hy0 + hh + LyMax + 3); py++)
      for (int px = std::max(x0, hx0); px < std::min(x1, hx0 + hw + Lmax + 3); px++) {
        if (inFoot(px, py)) continue;   // under the building itself
        int l = castAt(px, py);
        // (M2 fixer round 2) a soft edge: the shadow's outer two pixels are a dithered penumbra, not a hard ruled line
        if (l == 1 && castAt(px + 2, py + 2) == 0 && !inFoot(px + 2, py + 2)) l = 3;
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
  if (!m.eco.empty()) sub->eco.assign(n, 0);       // (M3c) the biome proper and the eco it blends toward
  if (!m.ecoNb.empty()) sub->ecoNb.assign(n, 0);
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
    if (!m.eco.empty()) std::copy_n(m.eco.begin() + si, len, sub->eco.begin() + di);
    if (!m.ecoNb.empty()) std::copy_n(m.ecoNb.begin() + si, len, sub->ecoNb.begin() + di);
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
    if (jobs_[i].mapId == mapId || (prepMapId_ && jobs_[i].mapId == prepMapId_)) { i++; continue; }   // (M3c: the door ahead's interior too)
    // forget it was pending too, or coming back to that map would wait forever and bake inline (a hitch)
    uint64_t k = jobs_[i].key;
    pending_.erase(std::remove(pending_.begin(), pending_.end(), k), pending_.end());
    jobs_.erase(jobs_.begin() + i);
  }
  // finished bakes nobody will collect (another map, or a chunk already baked inline) are ~1 MB each: drop them
  for (size_t i = 0; i < done_.size();) {
    bool stale = done_[i].mapId != mapId && !(prepMapId_ && done_[i].mapId == prepMapId_);
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
          if (!wk || wallTiles_.count(wallTexKey(wk))) continue;
          if (paintOver()) { ready = false; over = true; break; }
          wallTileTex(wk);
        }
    } else if (!m.wall.empty() && arrival_.frames < 2) ready = false;   // drawWorld lays the wall keys out this frame
  }
  // the people the arrival shows: their character sheets painted ahead (a city square wakes thirty at once)
  const auto th = SClock::now();
  for (const Actor& a : g.actors) {
    if (!a.human || a.p.x < cam.x - 96 || a.p.x > cam.x + Pix::W + 96 || a.p.y < cam.y - 96 || a.p.y > cam.y + Pix::H + 128) continue;
    const bool child = a.role == Role::Child;   // (M5) children's sheets are keyed apart (render.cpp childBody)
    if (humans_.count(a.look.key() ^ (child ? 0xC41D0000C41Dull : 0))) continue;
    if (st ? spent() > kArriveBudget * 0.75 : spent() > 8.0) { ready = false; break; }
    humanTex(a.look, child);
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

// (M4) the diagonal boardwalk test for render.cpp (the painters above are file-local)
bool boardwalkCoversTile(const Map& m, int tx, int ty) { return boardwalkCoversTileImpl(m, tx, ty); }
