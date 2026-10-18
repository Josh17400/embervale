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
#include <string>
#include <utility>
#include <vector>
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"
#include "rpg/world/urban.h"

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
// (M11 fixer r2) a glacier's crevasse fields: 0 outside, rising to 1 inside, in the glacier's turned frame (u, v)
inline float crevasseW(float u, float v) {
  const float k = vnoise(u / 640.0f, v / 300.0f, 1815) * 0.8f + vnoise(u / 160.0f, v / 90.0f, 1817) * 0.2f;   // (px: fields 20-40 tiles across)
  return k < 0.62f ? 0.0f : std::min(1.0f, (k - 0.62f) / 0.08f);
}
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
  // (fixer M6b r1) the cells of THIS walk only: the boardwalk cells 4-connected to the tile within 4 steps (and the
  // 7 x 7 block). A stilt village's walks fan out close together; counting every boardwalk cell of the block mixed two
  // or three runs, the spread showed no diagonal, and the runs fell back to square decks stepping in L-shaped
  // staircases beside the slanted ones
  {
    uint8_t seen[7][7] = {};
    int qx[49], qy[49], qd[49], qh = 0, qt = 0;
    seen[3][3] = 1;
    qx[qt] = 0; qy[qt] = 0; qd[qt] = 0; qt++;
    while (qh < qt) {
      const int ox = qx[qh], oy = qy[qh], dd = qd[qh];
      qh++;
      cxs[n] = tx + ox; cys[n] = ty + oy; mx += (float)ox; my += (float)oy; n++;
      if (dd >= 4) continue;
      static const int nb[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
      for (const auto& v : nb) {
        const int nx = ox + v[0], ny = oy + v[1];
        if (nx < -3 || nx > 3 || ny < -3 || ny > 3 || seen[ny + 3][nx + 3]) continue;
        seen[ny + 3][nx + 3] = 1;
        if (!isW(tx + nx, ty + ny)) continue;
        qx[qt] = nx; qy[qt] = ny; qd[qt] = dd + 1; qt++;
      }
    }
  }
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

// (M11 TERRAIN) the material of a face by the land (VISION_PLAN 11.2). style:
//   0 granite / grey rock: jointed columns of varied width, lit on their west edge, a dark crack on the east, ledges
//   1 banded strata (badlands, petrified forest): red, ochre and cream beds fluted by the rain
//   2 ice (glacier, frozen lakes): smooth blue, melt streaks, glints, a few cracks
//   3 basalt (ash fields): narrow black columns
//   4 chalk (chalk downs): soft white beds, flint bands
//   5 dark mossy rock (the marsh, the deep woods): wet dark rock, moss cushions, drips
//   6 sandstone (the desert): thick cross-bedded beds with lit tops, rounded joints
//   7 earth bank (temperate low ground): soil with rills, roots and bedded stones under a hanging turf
struct FacePal { RockPal p; int style = 0; };
const RockPal kBankPal = {C(176, 140, 96), C(148, 112, 76), C(122, 90, 62), C(94, 68, 50), C(60, 44, 40), false};
const RockPal kGranitePal = {C(214, 212, 204), C(170, 168, 162), C(138, 136, 136), C(106, 104, 110), C(66, 64, 74), false};
const RockPal kSandPal = {C(244, 212, 156), C(222, 176, 116), C(194, 144, 94), C(156, 108, 74), C(108, 72, 56), false};
const RockPal kMarshPal = {C(134, 138, 106), C(102, 106, 84), C(80, 86, 68), C(60, 66, 54), C(36, 42, 38), false};
FacePal facePal(Biome b, int level) {
  switch (b) {
    case Biome::Plains: case Biome::Forest: case Biome::Autumn: case Biome::Beach:
      return level <= 3 ? FacePal{kBankPal, 7} : FacePal{kGranitePal, 0};
    case Biome::Desert: return {kSandPal, 6};
    case Biome::Swamp: return {kMarshPal, 5};
    default: return {rockPal(b), 0};   // the snow lands, the taiga, the mountains: cold grey rock
  }
}
// (M3c) the face of a step by biome proper: the wondrous and rocky lands show their own rock, the rest their family's
FacePal facePalE(Eco e, Biome b, int level) {
  switch (e) {
    case Eco::Badlands: case Eco::PetrifiedForest: return {rockPalE(e), 1};
    case Eco::Glacier: case Eco::FrozenLakes: return {rockPalE(e), 2};
    case Eco::AshFields: return {rockPalE(e), 3};
    case Eco::ChalkDowns: return {rockPalE(e), 4};
    case Eco::Jungle: case Eco::GiantForest: case Eco::MushroomForest: case Eco::DarkForest: return {rockPalE(e), 5};
    case Eco::CrystalBarrens: case Eco::Blight: case Eco::SeaCliffs: return {rockPalE(e), 0};
    case Eco::Heath: case Eco::StonePlains: case Eco::Tundra: if (level >= 2) return {rockPalE(e), 0}; break;
    case Eco::Mountain: case Eco::AlpineMeadow: return {kGranitePal, 0};
    default: break;
  }
  return facePal(b, level);
}
// a face's material at depth t (0 the lip .. 1 the foot), d px below its lip; cold: snow caught on its ledges
uint32_t faceMat(const FacePal& FP, const RockPal& P, float t, int d, int px, int py, bool cold) {
  uint32_t r = t < 0.5f ? lerpc(P.hi, P.mid, t * 2) : lerpc(P.mid, P.lo, (t - 0.5f) * 2);
  switch (FP.style) {
    case 7: {   // an earth bank: rills down the soil, faint layers, bedded stones, roots hanging from the turf
      const float rill = vnoise(px / 2.2f, py / 11.0f, 905);
      r = mul(r, 0.90f + rill * 0.18f);
      const int yy = py + (int)std::lround((vnoise(px / 13.0f, py / 6.0f, 907) - 0.5f) * 4.0f);
      const int band = yy >= 0 ? yy / 4 : (yy - 3) / 4;
      r = mul(r, 0.96f + hashf(band, px / 23, 909) * 0.07f);
      const float s0 = vnoise(px / 2.6f, py / 2.6f, 911), s1 = vnoise(px / 2.6f, (py - 1) / 2.6f, 911);
      if (s0 > 0.84f) r = s1 > 0.80f ? lerpc(C(150, 142, 128), P.lo, 0.35f) : lerpc(C(184, 176, 160), r, 0.25f);   // a stone: lit top, its shade under
      const uint32_t rh = hash2(px, 0, 913);
      const int rl = 3 + (int)((rh >> 8) % 6);
      if ((rh % 7) == 0 && d >= 2 && d <= rl) r = d == rl ? mul(r, 0.82f) : mul(lerpc(r, C(70, 50, 38), 0.6f), 0.9f);   // a root
      if (hashf(px, py, 863) < 0.03f) r = mul(r, 1.10f);
      return r;
    }
    case 1: {   // banded strata, fluted by the rain
      const int yy = py + (int)std::lround((vnoise(px / 13.0f, py / 6.0f, 921) - 0.5f) * 4.0f);
      const int band = yy >= 0 ? yy / 3 : (yy - 2) / 3;
      static const uint32_t BAND[5] = {C(226, 184, 134), C(196, 112, 72), C(170, 88, 60), C(214, 146, 96), C(150, 74, 56)};
      r = lerpc(r, mul(BAND[hash2(band, 0, 923) % 5], 0.86f + (1 - t) * 0.22f), 0.55f);
      const float fl = px + (vnoise(px / 7.0f, py / 9.0f, 925) - 0.5f) * 5.0f;   // the rain's flutes, wandering
      const float fw = 4.0f + (float)(hash2((int)std::floor(fl / 6.0f), 0, 926) % 4);
      const float fp = fl - std::floor(fl / fw) * fw;
      if (fp < 1.0f) r = mul(r, 0.84f);
      else if (fp < 2.0f) r = mul(r, 1.07f);
      return r;
    }
    case 2: {   // ice
      if (hash2(px / 2, 0, 927) % 4 == 0) r = mul(r, 0.92f);
      if (hashf(px, py, 929) < 0.025f) r = C(250, 254, 255);
      else if (((px * 3 + py * 2) % 23) == 0 && hashf(px / 5, py / 9, 931) < 0.3f) r = lerpc(r, P.dark, 0.6f);
      return r;
    }
    case 4: {   // chalk
      const int band = py >= 0 ? py / 4 : (py - 3) / 4;
      r = mul(r, 0.95f + hashf(band, px / 23, 933) * 0.08f);
      r = mul(r, 0.94f + vnoise(px / 3.0f, py / 10.0f, 936) * 0.10f);   // rain-washed runnels
      // flint nodules: dark lumps strung along a few beds, lit on top
      const int fy = (int)std::floor((py + (vnoise(px / 19.0f, 0.5f, 934) - 0.5f) * 4.0f) / 9.0f);
      const float fv = vnoise(px / 2.2f, py / 1.6f, 935);
      if ((fy % 2) == 0 && fv > 0.78f && hashf(px / 6, fy, 937) < 0.6f) r = vnoise(px / 2.2f, (py - 1) / 1.6f, 935) > 0.78f ? C(66, 66, 80) : C(120, 120, 134);
      return r;
    }
    case 6: {   // sandstone: soft beds of varied thickness, weathered back unevenly, a few long cracks
      // the beds wander and swell (their boundaries follow a slow wave), each its own warm tone
      const float yy = py + (vnoise(px / 23.0f, py / 11.0f, 941) - 0.5f) * 5.0f + (vnoise(px / 61.0f, 0.5f, 942) - 0.5f) * 6.0f;
      const int bed = (int)std::floor(yy / 5.0f);
      const float in = yy - bed * 5.0f;
      static const uint32_t BED[5] = {C(232, 188, 128), C(214, 160, 104), C(240, 204, 146), C(204, 150, 98), C(224, 176, 118)};
      r = lerpc(r, mul(BED[hash2(bed, 0, 943) % 5], 0.84f + (1 - t) * 0.2f), 0.5f);
      const float wx = vnoise(px / 9.0f, (float)bed, 944);   // how far this stretch of the bed is weathered back
      if (in < 1.0f && wx > 0.45f) r = mul(r, 1.10f);                    // a bed's rounded, lit top
      else if (in > 4.0f && wx < 0.55f) r = mul(r, 0.84f);               // the shade where the bed above overhangs
      if (vnoise(px / 2.5f, py / 2.0f, 945) > 0.80f) r = mul(r, 0.90f);  // honeycomb pits
      // long cracks down through several beds, wandering, their west lip lit
      const float cx = px + (vnoise(px / 13.0f, py / 6.0f, 946) - 0.5f) * 4.0f;
      const int cc = (int)std::floor(cx / 17.0f);
      const float off = 3.0f + (float)(hash2(cc, 0, 947) % 11);
      const float dxc = cx - cc * 17.0f - off;
      if (hashf(cc, bed / 3, 949) < 0.55f) {
        if (dxc >= 0.0f && dxc < 1.0f) r = mul(r, 0.66f);
        else if (dxc >= -1.0f && dxc < 0.0f) r = mul(r, 1.08f);
      }
      if (cold && in < 1.0f && wx > 0.45f) r = C(232, 238, 248);
      return r;
    }
    default: break;
  }
  // rock in columns: granite (0), basalt (3, narrow), mossy (5)
  const int u = px + (int)std::lround((vnoise(px * 0.21f, py * 0.17f, 893) - 0.5f) * 4.0f);
  const int cw0 = FP.style == 3 ? 4 : 6;
  const int cs = u >= 0 ? u / cw0 : (u - cw0 + 1) / cw0;
  const int cw = FP.style == 3 ? cw0 : cw0 + (int)(hash2(cs, 0, 891) % 3) - 1;   // varied widths (5..7)
  const int in = ((u - cs * cw0) % cw + cw) % cw;
  r = mul(r, 0.88f + hashf(cs, py / 13, 895) * 0.22f);
  if (in == 0) r = mul(r, 1.16f);                                                  // the column's lit west edge
  else if (in == cw - 1) r = mul(r, 0.64f);                                        // the crack east of it
  else if (in == cw - 2 && hashf(cs, py / 3, 899) < 0.4f) r = mul(r, 0.86f);
  const int ledge = py + (int)(hash2(cs, 0, 897) % 7);
  const bool ledgeTop = ledge % 7 == 1 && hashf(cs, ledge / 7, 901) < 0.22f;
  if (ledge % 7 == 0 && hashf(cs, ledge / 7, 901) < 0.22f) r = mul(r, 0.74f);     // a joint across the column
  else if (ledgeTop) r = mul(r, 1.10f);                                             // the little ledge under it
  r = mul(r, 0.92f + vnoise(px / 3.0f, py / 7.0f, 903) * 0.16f);
  if (FP.style == 5) {   // moss cushions, thickest near the top, and dark drips
    const float mv = vnoise(px / 5.0f, py / 3.5f, 955) + (1.0f - t) * 0.18f;
    if (mv > 0.70f) r = mv > 0.80f ? C(104, 132, 62) : (in == 0 ? C(88, 116, 56) : C(66, 92, 48));
    else if ((hash2(px, 0, 957) % 9) == 0 && hashf(px, py / 4, 959) < 0.7f) r = mul(r, 0.78f);
  }
  if (cold && (ledgeTop || (in == 0 && hashf(px, py, 961) < 0.05f))) r = C(226, 234, 246);   // snow caught on a ledge
  if (hashf(px, py, 863) < 0.02f) r = mul(r, 1.12f);
  return r;
}
// the aerial grade (VISION_PLAN 11.3): each level about 4 % lighter and 5 % less saturated than the one below, the high
// ones (5+) hazed a little blue; level 0 keeps the lowland's tone. Snow and ice, already as bright as paint goes, are
// shaded darker and cooler on the lower levels instead (a snowfield's steps read as steps).
inline uint32_t aerialGrade(uint32_t col, float lv) {
  const float r = (float)(col & 255), g = (float)((col >> 8) & 255), b = (float)((col >> 16) & 255);
  const float y = r * 0.3f + g * 0.55f + b * 0.15f;
  const float ds = std::min(0.36f, 0.05f * lv), k = 0.86f + 0.046f * lv, hz = std::clamp((lv - 4.0f) * 0.045f, 0.0f, 0.13f);
  const float r1 = (r + (y - r) * ds) * k, g1 = (g + (y - g) * ds) * k, b1 = (b + (y - b) * ds) * k;
  return C((int)(r1 + (200.0f - r1) * hz), (int)(g1 + (212.0f - g1) * hz), (int)(b1 + (236.0f - b1) * hz), (int)(col >> 24));
}
// the same grade on a face's or a stair's own colours (painted for the middle levels): gentler, so high rock stays rock
inline uint32_t aerialFace(uint32_t col, float lv) {
  const float r = (float)(col & 255), g = (float)((col >> 8) & 255), b = (float)((col >> 16) & 255);
  const float y = r * 0.3f + g * 0.55f + b * 0.15f;
  const float ds = std::min(0.30f, 0.04f * lv), k = 0.94f + 0.03f * lv, hz = std::clamp((lv - 4.0f) * 0.045f, 0.0f, 0.13f);
  const float r1 = (r + (y - r) * ds) * k, g1 = (g + (y - g) * ds) * k, b1 = (b + (y - b) * ds) * k;
  return C((int)(r1 + (200.0f - r1) * hz), (int)(g1 + (212.0f - g1) * hz), (int)(b1 + (236.0f - b1) * hz), (int)(col >> 24));
}
inline RockPal gradePal(const RockPal& P, float lv) {
  return {aerialFace(P.lip, lv), aerialFace(P.hi, lv), aerialFace(P.mid, lv), aerialFace(P.lo, lv), aerialFace(P.dark, lv), P.cold};
}
inline uint32_t aerialSnow(uint32_t col, float lv) {
  // (M11 fixer r1) every level its own shade up to the top (levels 5..7 were all one white): each level about 5 % darker
  // and a little bluer than the one above it, the top tier the brightest
  // (M11 fixer r2) the top tiers 7 % apart (the summit crown and the crests over the high snowfields read as higher
  // ground at a glance), the low ones 4 % as before
  const float q = std::min(lv, 7.0f), k = q >= 5.0f ? 1.0f - 0.07f * (7.0f - q) : 0.86f - 0.04f * (5.0f - q);
  const uint32_t s = mul(col, k);
  return lerpc(s, C(150, 168, 214, (int)(s >> 24)), 0.05f * std::max(0.0f, 7.0f - lv));
}
inline bool turfGround(Ground g) {
  return g == Ground::Grass || g == Ground::Meadow || g == Ground::ForestFloor || g == Ground::Autumn || g == Ground::Swamp ||
         g == Ground::Tundra || g == Ground::Farmland;
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
  // (M11 TERRAIN, VISION_PLAN 11.2: "rock is drawn as stacked cliffs, never as a flat grey area") the massif is cut into
  // tiers: a height field (the broad crests and gullies, the relief levels and the depth into the mass) is sliced into
  // steps about a tile and a half apart. Every step down to the south shows a rock face (jointed columns, a lit lip or
  // a snow cornice, a dark foot) with its cast shadow on the ledge below; a step down to the east or west a narrow lip
  // (lit facing west, in shade facing east); the ledges between are scree: gravel and fallen stones under the top-left
  // light, a fan of loose rubble under each face, snow lying on the ledges of the cold or high ranges. The field is
  // sampled on a 4-px lattice cached per thread (bakes run on worker threads) and blended between its points.
  struct MCell { int32_t kx = INT32_MIN, ky = 0; float h = 0, side = 0; };
  struct MCache { const void* map = nullptr; int ox = 0, oy = 0; MCell c[64 * 64]; };
  thread_local MCache mc;
  if (mc.map != (const void*)m.m || mc.ox != m.ox || mc.oy != m.oy) {
    mc.map = (const void*)m.m; mc.ox = m.ox; mc.oy = m.oy;
    for (MCell& e : mc.c) e.kx = INT32_MIN;
  }
  auto rockT = [&](int x, int y) { return m.at(x, y) == Ground::Rock; };
  auto cell = [&](int X, int Y) -> const MCell& {
    MCell& e = mc.c[((Y & 63) << 6) | (X & 63)];
    if (e.kx == X && e.ky == Y) return e;
    e.kx = X; e.ky = Y;
    const int x = X * 4, y = Y * 4;
    const float fx = (x - 8) / 16.0f, fy = (y - 8) / 16.0f;
    const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    const float ax = fx - ix, ay = fy - iy;
    const float lv = ((float)m.heightAt(ix, iy) * (1 - ax) + (float)m.heightAt(ix + 1, iy) * ax) * (1 - ay) +
                     ((float)m.heightAt(ix, iy + 1) * (1 - ax) + (float)m.heightAt(ix + 1, iy + 1) * ax) * ay;
    auto blur = [&](int tx, int ty) {
      float s = 0;
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) if (rockT(tx + dx, ty + dy)) s += (dx == 0 && dy == 0) ? 4.0f : (dx == 0 || dy == 0) ? 2.0f : 1.0f;
      return s / 16.0f;
    };
    const float dome = (blur(ix, iy) * (1 - ax) + blur(ix + 1, iy) * ax) * (1 - ay) + (blur(ix, iy + 1) * (1 - ax) + blur(ix + 1, iy + 1) * ax) * ay;
    e.h = 2.6f * H2((float)x, (float)y) + 0.9f * lv + 2.4f * dome + 0.6f * vnoise(x / 23.0f, y / 19.0f, 985);
    // the massif's own light: how far the rock runs toward the light (up-left) against away from it (down-right)
    float hUL = 0, hDR = 0;
    for (int k = 1; k <= 12; k++) { if (!rockT((x - k * 3) >> 4, (y - k * 2) >> 4)) break; hUL = (float)k; }
    for (int k = 1; k <= 12; k++) { if (!rockT((x + k * 3) >> 4, (y + k * 2) >> 4)) break; hDR = (float)k; }
    e.side = (hDR - hUL) / (hDR + hUL + 2.0f);
    return e;
  };
  auto field = [&](int x, int y, float* side = nullptr) {
    const int X = x >> 2, Y = y >> 2;
    const float ax = (x & 3) / 4.0f, ay = (y & 3) / 4.0f;
    const MCell &a = cell(X, Y), &b = cell(X + 1, Y), &c2 = cell(X, Y + 1), &d2 = cell(X + 1, Y + 1);
    if (side) *side = (a.side * (1 - ax) + b.side * ax) * (1 - ay) + (c2.side * (1 - ax) + d2.side * ax) * ay;
    return (a.h * (1 - ax) + b.h * ax) * (1 - ay) + (c2.h * (1 - ax) + d2.h * ax) * ay;
  };
  auto tier = [&](int x, int y) { return (int)std::floor(field(x, y)); };
  float side = 0;
  const float hv = field(px, py, &side);
  const int T = (int)std::floor(hv);
  const bool snowy = cold > 0.35f;
  const FacePal FPm{P, 0};
  // how high the snow lies: the cold north holds it low, hot lands keep their rock bare
  const float snowAt = 4.6f - 2.6f * std::min(1.0f, cold) + 4.0f * hot;
  const int FHt = 8 + (int)(vnoise(px / 6.0f, 3.5f, 987) * 4.0f);   // a tier's face, 8-11 px
  int d = 0, topT = T;
  for (int k = 1; k <= FHt + 9 && !d; k++) {
    const int tk = tier(px, py - k);
    if (tk > T) { d = k; topT = std::max(tk, tier(px, py - k - 4)); }
  }
  const int fh = topT - T >= 2 ? FHt + 6 : FHt;
  uint32_t c;
  if (d && d <= fh) {
    // a face: jointed rock darkening to its foot, a lit lip (a snow cornice where the ledge above holds snow)
    const float t = (d - 1) / (float)std::max(1, fh - 1);
    const bool snowAbove = (float)topT > snowAt + (vnoise(px / 9.0f, 0.5f, 989) - 0.5f) * 1.2f;
    c = faceMat(FPm, P, t, d, px, py, snowy || snowAbove);
    if (d == 1) c = snowAbove ? C(246, 249, 253) : P.lip;
    else if (d == 2) c = snowAbove ? C(204, 214, 234) : lerpc(P.lip, P.hi, 0.5f);
    else if (snowAbove && d == 3 && (hash2(px, 0, 991) % 3) == 0) c = C(214, 224, 242);   // icicles of the cornice
    if (d >= fh - 1) c = mul(c, 0.80f);
    if (d == fh) c = lerpc(P.dark, C(30, 24, 34), 0.35f);
  } else {
    // a ledge of scree, lit by the massif's light (its north-west flank catches the sun, its south-east one is in shade)
    float lit = 0.56f + side * 0.26f + (vnoise(px / 13.0f, py / 11.0f, 993) - 0.5f) * 0.18f + (bayer(px, py) - 0.5f) * 0.10f;
    if (d && d <= fh + 3) lit -= 0.30f - (d - fh - 1) * 0.08f;                // the face's cast shadow on the ledge
    const bool fan = d && d > fh + 3 && d <= fh + 9;                          // the rubble fan under a face
    c = lit < 0.30f ? P.lo : lit < 0.50f ? P.mid : lit < 0.72f ? P.hi : lerpc(P.hi, P.lip, 0.5f);
    // gravel and fallen stones: a lit top, a dark underside
    const uint32_t gh = hash2(px / 2, py / 2, 995);
    if ((gh & 31) < (fan ? 7u : 3u)) c = (py & 1) ? mul(P.lo, 0.9f) : lerpc(P.hi, P.lip, 0.6f);
    else if (hashf(px, py, 997) < 0.05f) c = mul(c, 0.84f);
    const float s0 = vnoise(px / 3.0f, py / 2.5f, 999), s1 = vnoise(px / 3.0f, (py - 1) / 2.5f, 999);
    if (s0 > 0.82f && !(d && d <= fh + 3)) c = s1 > 0.82f ? lerpc(P.lo, P.dark, 0.4f) : P.lip;   // a boulder
    // side lips: a higher tier to the west shows its east-facing lip (in shade), one to the east its lit west lip
    const int tW = tier(px - 1, py), tE = tier(px + 1, py), tW2 = tier(px - 2, py);
    if (tW > T || tW2 > T) c = tW > T ? lerpc(P.lo, P.dark, 0.5f) : P.lo;
    else if (tE > T) c = P.lip;
    else if (tier(px + 2, py) > T) c = P.hi;
    // snow on the ledges of the high and cold ranges: smooth drifts shaded by the massif's light
    const float sv = (float)T + (side * 0.8f) + (vnoise(px / 17.0f, py / 13.0f, 1001) - 0.5f) * 1.4f;
    if (sv > snowAt && !(tW > T || tW2 > T)) {
      const float sl = lit + 0.08f;
      c = sl < 0.36f ? C(168, 182, 212) : sl < 0.52f ? C(196, 208, 230) : sl < 0.70f ? C(226, 233, 245) : C(246, 249, 253);
      if (hashf(px, py, 1003) < 0.01f) c = C(255, 255, 255);
      if (fan && (gh & 31) < 3u) c = mul(P.lo, 0.95f);                        // a stone through the snow
    }
  }
  (void)hv;
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
uint32_t pavePixel(int mat, bool road, int px, int py, float e, float paveV, float h, float n);
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
  bool eastWest = hl >= vl;   // the way you walk: planks lie across it
  // (M7c fixer r3, review: "planks running in alternating directions") in a plank city (its fabric within 3 tiles) the
  // boardwalk is the city's deck run on over the marsh: the deck's boards, laid its way (a village keeps its walks)
  bool cityDeck = false;
  if (!m.m->urb.empty())
    for (int oy = -3; oy <= 3 && !cityDeck; oy++)
      for (int ox = -3; ox <= 3; ox++) if (m.urbAt(tx + ox, ty + oy)) { cityDeck = true; break; }
  if (cityDeck) eastWest = false;
  auto wetAt = [&](int x, int y) { const Ground q = m.at(x, y); return groundWater(q) || q == Ground::Swamp || q == Ground::Void; };
  const int a = eastWest ? px : py, along = eastWest ? py : px;
  const int row = ((a >> 2) % 4096 + 4096) % 4096;
  const int joff = (int)(hash2(row, 0, 1411) % 16u);
  const bool gap = (a & 3) == 0, butt = ((along + joff) & 15) == 0;
  uint32_t c = gap ? C(58, 42, 30) : lerpc(C(124, 96, 64), C(150, 118, 80), hashf(row, (along + joff) >> 4, 1413));
  if (!gap && (a & 3) == 1) c = mul(c, 1.08f);   // each plank's lit edge
  if (butt && !gap) c = C(84, 62, 42);
  if (!gap && vnoise(px / 9.0f, py / 9.0f, 1415) > 0.68f) c = lerpc(c, C(132, 132, 116), 0.3f);   // silvered by the damp
  if (cityDeck) c = pavePixel(4, true, px, py, 0.0f, -1.0f, hashf(px, py, 1417), 0.5f);
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
  auto landAt = [&](int x, int y) { const Ground q = m.at(x, y); return q != Ground::Bridge && !wetAt(x, y) && !groundSolid(q) && !(cityDeck && (q == Ground::Road || q == Ground::Plaza)); };
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
  const int pv0 = m.blendAt(tx, ty) & 15;   // (M7c fixer: the fjord city's 10 kerbs in fieldstone, the river city's 11 in brick)
  const int pv = std::clamp(pv0 == 10 ? 2 : (pv0 == 11 ? 9 : pv0), 0, 9);
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
        // (fixer M8 r3, review: "pavers about the size of a head, random blue-grey tiles, flat") slabs of a person's stride, not a head's height; each slab dished a little (its lit top-left lip, its
        // shaded foot two pixels deep), a rare warm-grey marble one instead of blue tiles, broad wear trodden darker
        // down the middle of the square; at the rim a dark worn edge stone where the square meets the ground
        const Ashlar sl = ashlarAt(px, py, 1311, 13, 4);
        const int in = sl.in, iy = sl.iy;
        const float tone = (sl.id >> 8) * (1.0f / 16777216.0f);
        c = lerpc(C(196, 184, 156), C(220, 208, 180), tone);
        if ((sl.id % 29u) == 0) c = lerpc(C(176, 170, 160), C(190, 184, 172), tone);   // a weathered marble slab
        const bool jt = in == 0 || iy == 0;
        if (jt) c = C(142, 128, 104);
        else if (iy == 1 || in == 1) c = mul(c, 1.05f);
        else if (iy == sl.rh - 1 || in == sl.sw - 1) c = mul(c, 0.90f);
        else if (iy == sl.rh - 2 && sl.rh > 4) c = mul(c, 0.96f);
        if (h < 0.05f && !jt) c = mul(c, 0.90f);   // pits in the stone
        c = mul(c, 0.92f + broad * 0.12f);
        c = lerpc(c, C(170, 150, 120), std::max(0.0f, vnoise(px / 24.0f, py / 24.0f, 1313) - 0.6f) * 0.6f);   // trodden wear
        if (rim) {
          if (paveV < 0.53f) c = jt ? C(96, 82, 62) : lerpc(C(150, 132, 104), C(164, 146, 116), tone);   // the worn edge stone
          else c = jt ? C(124, 104, 78) : mul(lerpc(c, C(184, 164, 130), 0.4f), 0.94f);                // earth in the joints
        }
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
    case 10: {
      // (M7c fixer) the fjord's timber-walk city (town_dress: a planned city of a plank-paving people on dry land) off
      // its street fabric: yards and doorsteps of trodden grey shore gravel, cooler and paler than the log walls, with
      // flat stones bedded in it and grass at the edges (the walks themselves are planks, the streets setts: the fabric)
      c = pick3(n, C(132, 128, 118), C(144, 140, 128), C(156, 150, 136));
      const float tr = vnoise(px / 13.0f, py / 13.0f, 1361);
      if (tr > 0.64f) c = lerpc(c, C(150, 126, 94), 0.35f);   // trodden earth showing through
      if (tr < 0.22f) c = mul(c, 1.05f);
      const PaveCell s = paveCell(px, py, 9, 0.8f, 1363, false);
      if ((s.id % 7u) == 0 && s.edge > 1.2f) c = mul(lerpc(c, C(170, 168, 160), 0.6f), s.dx + s.dy < 0 ? 1.06f : 0.94f);   // a bedded stone
      if (h < 0.10f) c = mul(c, 0.86f);
      else if (h > 0.93f) c = mul(c, 1.10f);   // grit
      if ((road ? e > 0.6f : rim) && vnoise(px / 4.0f, py / 4.0f, 1365) > 0.6f) c = lerpc(c, C(96, 126, 64), 0.6f);   // grass
      break;
    }
    case 11: {
      // (M7c fixer, review: "the paving is a large featureless brick field"; "block interiors paved in the same red brick
      // as the house walls") the river people's planned city (town_dress: paving 9 in a planned city) off its street
      // fabric: its yards, doorsteps and back courts of trodden earth with a scatter of cobbles and broken brick, so the
      // brick streets, walks and houses read against the ground (the fabric itself draws as the river towns' 9)
      c = pick3(n, C(138, 118, 92), C(148, 128, 98), C(158, 138, 106));
      const float tr = vnoise(px / 12.0f, py / 12.0f, 1371);
      if (tr > 0.64f) c = mul(c, 0.91f);
      if (tr < 0.24f) c = lerpc(c, C(170, 154, 124), 0.35f);
      const PaveCell s = paveCell(px, py, 5, 0.8f, 1373, false);
      if (s.edge > 1.6f && s.dx * s.dx + s.dy * s.dy < 2.6f) {
        const uint32_t k = s.id % 19u;
        if (k == 0 || k == 7) c = mul(C(156, 150, 140), s.dx + s.dy < 0 ? 1.06f : 0.90f);   // a bedded cobble
        else if (k == 3) c = mul(C(144, 88, 68), s.dx + s.dy < 0 ? 1.04f : 0.90f);        // a brick end
      }
      if (h < 0.05f) c = mul(c, 0.86f);
      if ((road ? e > 0.6f : rim) && vnoise(px / 4.0f, py / 4.0f, 1375) > 0.62f) c = lerpc(c, C(98, 128, 62), 0.6f);   // grass
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
    case Eco::Mountain: {   // (M11 TERRAIN) the mountains' open high ground (macro.cpp groundFor): scree and stony turf
      if (g == Ground::Tundra) {
        // scree: angular grey rubble streaked down the fall line in fans, the bigger stones lit on top and shaded
        // under, grit, old stones yellow with lichen, a little grass in the hollows
        c = pick3(n, C(126, 122, 116), C(138, 134, 126), C(150, 146, 138));
        c = mul(c, 0.92f + vnoise(px / 30.0f, py / 30.0f, 1701) * 0.16f);
        c = mul(c, 0.93f + vnoise(px / 8.0f, py / 24.0f, 1703) * 0.13f);
        uint32_t id = 0;
        const int s = stoneAt(px, py, 9, 1705, 0.34f, 1.4f, 3.0f, &id);
        if (s == 4) c = mul(c, 0.70f);
        else if (s) c = s == 3 ? C(190, 186, 178) : s == 1 ? C(90, 88, 92) : C(152, 148, 142);
        else if (h > 0.97f) c = mul(c, 0.82f);
        else if (h < 0.006f) c = C(176, 172, 108);
        if (!s && vnoise(px / 14.0f, py / 14.0f, 1707) > 0.72f) c = tufted(lerpc(c, C(108, 130, 82), 0.55f), px, py, 1709, 0.4f, 0.8f, 1.12f);
        return true;
      }
      if (g == Ground::Grass) {   // the high turf: short, cool, stony, a few flowers
        c = pick3(n, C(96, 140, 86), C(106, 150, 92), C(118, 160, 100));
        c = tufted(c, px, py, 1711, 0.42f, 0.78f, 1.14f);
        uint32_t id = 0;
        const int s = stoneAt(px, py, 15, 1713, 0.22f, 1.6f, 3.4f, &id);
        if (s == 4) c = mul(c, 0.70f);
        else if (s) c = s == 3 ? C(196, 194, 188) : s == 1 ? C(104, 104, 108) : C(150, 150, 148);
        else {
          static const uint32_t pal[3] = {C(246, 246, 238), C(130, 170, 250), C(246, 214, 80)};
          flowered(c, px, py, 1715, 0.08f, pal, 3);
        }
        return true;
      }
      return false;
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
      // (M11 fixer r2) crevasses only in crevasse fields (a slow mask, about a quarter of the ice), each one tapering
      // to a point where the field ends: the noise's isoline drawn everywhere ran as thin dark contour lines over the
      // whole glacier, and read as relief that was not there
      const float cw = crevasseW(u, v);
      if (cw > 0) {
        const float d = vnoise(u / 44.0f, v / 12.0f, 1805) - 0.5f;
        if (std::fabs(d) < 0.012f * cw) c = C(50, 84, 146);
        else if (d >= 0.012f * cw && d < 0.032f * cw) c = C(112, 154, 208);
        else if (d <= -0.012f * cw && d > -0.026f * cw) c = C(244, 250, 255);
      }
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

// =====================================================================================================================
// M7b "Cities" (CITYART lane, VISION_PLAN 15.23): the urban fabric (Map::urb, rpg/world/urban.h) painted in the 3/4 view
// with the top-left light, in the material of the culture's paving (Map::PAVE_MARK). Drawn ONLY where a tile's class is
// not Urb::None (never in a village: rpg_test --villages), so every other ground keeps its pixels.
//  - carriageways: avenues and streets in the people's best setts, lanes in cobbles, alleys worn; the upkeep (0..3) wears
//    them from even swept setts (3) through cobbles with grit in the joints (2) and worn setts with stones lost (1) to
//    patched dirt and broken stones (0);
//  - sidewalks: raised flagged walks; where one meets a lower carriageway its kerb shows: to the SOUTH a lit arris over a
//    2 px vertical face and a dark foot (the face the camera sees), to the north and west a lit arris, to the east a
//    shaded one with the kerb's cast shadow on the road beside it; corner stones round where a kerb turns, the bands run
//    on into the inner corners (no gap where a run steps on a curve or a diagonal);
//  - gutters: a channel of dark, water-stained stones with an iron grate where it meets a square;
//  - squares and courts: the people's square paving framed by a border course of long kerb stones, a big square banded
//    two tiles in; closes: flags round kerbed lawn beds; parks: lawn and soft-edged gravel paths; gardens: kitchen beds
//    and orchard grass; quays: cut stone, a coping lit along the water and the face below it, mooring posts.
// Pure functions of the global pixel and the snapshot (bakes run on worker threads), so chunks meet seamlessly.
// =====================================================================================================================
using ew::Urb;
template <class TM>
inline Urb ucls(const TM& m, int x, int y) { return ew::urbClass(m.urbAt(x, y)); }
// the culture's stones for the fabric: flags (walks, closes), kerbs (and border courses, copings), setts (carriageways)
struct UrbPal { uint32_t flag, kerb, sett; };
UrbPal urbPal(int pm) {
  switch (pm) {
    case 1: return {C(206, 196, 170), C(222, 214, 192), C(126, 122, 118)};   // the empire: travertine, basalt roads
    case 2: return {C(170, 156, 128), C(184, 172, 148), C(140, 128, 108)};   // the earth-paving peoples: fieldstone
    case 3: return {C(214, 182, 134), C(228, 204, 158), C(202, 158, 108)};   // the dune folk: sandstone
    case 4: return {C(142, 114, 82), C(112, 86, 60), C(132, 106, 76)};       // the marsh: planks, a timber kerb beam
    case 5: return {C(156, 162, 146), C(178, 182, 166), C(142, 148, 132)};   // the elves: grey-green flags
    case 6: return {C(212, 212, 216), C(234, 236, 242), C(192, 192, 198)};   // the star cities: white limestone
    case 7: return {C(206, 190, 156), C(216, 204, 176), C(178, 108, 76)};    // the sun temples: limestone, terracotta
    case 8: return {C(150, 156, 162), C(170, 174, 180), C(126, 132, 142)};   // the jade kingdoms: grey granite
    case 9: return {C(170, 162, 152), C(180, 178, 174), C(162, 84, 64)};     // the river towns: grey stone, red brick
    case 10: return {C(158, 156, 148), C(98, 76, 54), C(122, 122, 120)};     // the fjord city: plank walks on a tarred
                                                                             // timber kerb, grey fieldstone setts
    default: return {C(182, 172, 154), C(196, 190, 178), C(138, 138, 142)};  // the heartland: warm flags, grey setts
  }
}
inline uint32_t urbEarth(int px, int py) {
  const float t = vnoise(px / 13.0f, py / 13.0f, 2701);
  uint32_t c = pick3(t + (bayer(px, py) - 0.5f) * 0.1f, C(138, 110, 78), C(150, 120, 86), C(160, 130, 94));
  if (hashf(px, py, 2703) < 0.05f) c = mul(c, 0.86f);
  return c;
}
// setts in running bond (course rh px, stones about P px): the stone's bevel lit on its upper left; joint: a joint pixel
inline uint32_t settPx(int px, int py, uint32_t col, int rh, int P, uint32_t seed, bool& joint) {
  const Ashlar a = runningAt(px, py, seed, rh, P, 2);
  joint = a.in == 0 || a.iy == 0;
  if (joint) return mul(col, 0.64f);
  uint32_t c = mul(col, 0.90f + ((a.id >> 8) & 255) / 255.0f * 0.18f);
  if (a.iy == 1 || a.in == 1) c = mul(c, 1.07f);
  else if (a.iy == a.rh - 1 || a.in == a.sw - 1) c = mul(c, 0.90f);
  return c;
}
// an avenue's setts laid in segmental arcs (fans): each course a shallow arc 24 px across, the stones of a course
// staggered along it, lit on their upper left; the arcs read at 1x as the even, planned paving of a main avenue
inline uint32_t fanPx(int px, int py, uint32_t col, uint32_t seed, bool& joint) {
  const int A = 24;
  const int u = fmod_(px, A), ai = fdiv_(px, A);
  const float du = (u + 0.5f - A * 0.5f) / (A * 0.5f);
  const float ph = py + du * du * 7.0f;              // the arc: courses bow toward the viewer at the fan's edges
  const int crs = (int)std::floor(ph / 4.0f);
  const float in = ph - crs * 4.0f;
  const int along = px + (crs & 1) * 3 + (int)(hash2(crs, ai, seed) % 3u);
  const int sj = fmod_(along, 5);
  const bool fanEdge = u == 0;
  joint = in < 1.0f || sj == 0 || fanEdge;
  if (joint) return mul(col, fanEdge ? 0.80f : 0.78f);   // (the fans meet in cusps: no darker seam line down the street)
  const uint32_t id = hash2(fdiv_(along, 5), crs, seed + 1);
  uint32_t c = mul(col, 0.90f + ((id >> 8) & 255) / 255.0f * 0.16f);
  if (in < 2.0f || sj == 1) c = mul(c, 1.06f);
  else if (in > 3.2f || sj == 4) c = mul(c, 0.92f);
  return c;
}
// rounded cobbles (a jittered cell per stone): lit upper left, shaded lower right, dark joints
inline uint32_t cobblePx(int px, int py, uint32_t col, uint32_t seed, bool& joint) {
  const PaveCell s = paveCell(px, py, 5, 0.75f, seed, false);
  joint = s.edge < 1.0f;
  if (joint) return mul(col, 0.58f);
  uint32_t c = mul(col, 0.88f + ((s.id >> 8) & 255) / 255.0f * 0.20f);
  if (s.edge < 1.9f) c = mul(c, (s.dx + s.dy) < 0 ? 1.08f : 0.86f);
  return c;
}
// big flags in irregular courses (walks, closes, quays)
inline uint32_t flagPx(int px, int py, uint32_t col, uint32_t seed, int P, bool& joint) {
  const Ashlar a = ashlarAt(px, py, seed, P, 4);
  joint = a.in == 0 || a.iy == 0;
  if (joint) return mul(col, 0.70f);
  uint32_t c = mul(col, 0.93f + ((a.id >> 8) & 255) / 255.0f * 0.12f);
  if (a.iy == 1 || a.in == 1) c = mul(c, 1.05f);
  else if (a.iy == a.rh - 1 || a.in == a.sw - 1) c = mul(c, 0.93f);
  return c;
}
// the wear of the upkeep over a paved pixel
inline uint32_t urbWear(uint32_t c, int up, int px, int py, bool joint) {
  if (up >= 3) return c;
  const float w = vnoise(px / 9.0f, py / 9.0f, 2711), h = hashf(px, py, 2713);
  if (up == 2) {
    if (joint && w > 0.55f) c = lerpc(c, C(132, 112, 84), 0.4f);    // grit in the joints
    return c;
  }
  if (up == 1) {
    if (w > 0.74f) return h < 0.12f ? C(170, 160, 144) : urbEarth(px, py);   // stones lost: the bedding shows
    if (joint) c = h < 0.18f ? C(88, 120, 58) : lerpc(c, C(126, 104, 76), 0.6f);   // earth and weeds in the joints
    return mul(c, 0.95f);
  }
  // 0: patched dirt with broken stones; ruts along the way
  if (w > 0.40f) {
    uint32_t e = urbEarth(px, py);
    if (vnoise(px / 3.0f, py / 11.0f, 2717) > 0.78f) e = mul(e, 0.84f);
    if (h < 0.03f) e = C(176, 168, 150);
    return e;
  }
  if (joint) return lerpc(c, C(110, 90, 66), 0.7f);
  return h < 0.08f ? mul(c, 0.78f) : mul(c, 0.92f);   // cracked
}
// a carriageway pixel of class u
inline uint32_t carriagePx(int pm, Urb u, int up, int px, int py) {
  const UrbPal P = urbPal(pm);
  bool joint = false;
  uint32_t c;
  if (u == Urb::Alley) up = std::min(up, 1);
  const float n = toneField(px, py, 2721), h = hashf(px, py, 2723);
  if (pm == 4) c = pavePixel(4, true, px, py, 0.0f, -1.0f, h, n);
  else if ((u == Urb::Avenue || u == Urb::Street) && (pm == 9 || pm == 8)) {
    // (fixer M7b r2, review: "red-brick cities: the road, the plots and the house walls are all the same brick") the
    // river towns lay their carriageways in dark grey setts: the street reads against the brick of the plots and walls
    // ((M7c fixer, review: "jade city: carriageway, pavement and plaza share one grey brick") the jade kingdoms' roads
    // likewise: dark slate-blue granite setts between the pale granite walks and the squares' herringbone)
    const uint32_t gs = pm == 8 ? C(98, 104, 116) : C(116, 112, 114);
    if (u == Urb::Avenue) c = fanPx(px, py, gs, 2735, joint);
    else { c = settPx(px, py, gs, 4, 5, 2731, joint); if (joint) c = mul(gs, 0.76f); }
    c = mul(c, 0.96f + vnoise(px / 40.0f, py / 40.0f, 2733) * 0.08f);
  }
  else if (u == Urb::Avenue || u == Urb::Street) {
    if (pm == 1 || pm == 3 || pm == 6 || pm == 7 || pm == 8 || pm == 9) {
      c = pavePixel(pm, true, px, py, 0.0f, -1.0f, h, n);
      joint = false;
    } else {
      if (u == Urb::Avenue) c = fanPx(px, py, P.sett, 2735, joint);
      else {
        c = settPx(px, py, P.sett, 4, 5, 2731, joint);
        if (joint) c = mul(P.sett, 0.76f);   // (fine, shallow joints: a sett floor, not a wall's courses)
      }
    }
    c = mul(c, 0.96f + vnoise(px / 40.0f, py / 40.0f, 2733) * 0.08f);
  } else {
    if (pm == 3 || pm == 7 || pm == 9) c = pavePixel(pm, true, px, py, 0.0f, -1.0f, h, n);
    else if (pm == 8) c = cobblePx(px, py, C(108, 114, 124), 2741, joint);   // ((M7c fixer) the jade lanes: dark cobbles)
    else c = cobblePx(px, py, P.sett, 2741, joint);
    c = mul(c, 0.94f);
  }
  return urbWear(c, up, px, py, joint);
}
// a kerb stone pixel: side 0 south (the face), 1 north, 2 west, 3 east; d px from the low side's edge; along: the
// coordinate along the kerb (joints between the long stones)
inline uint32_t kerbPx(const UrbPal& P, int side, int d, int along, int up, uint32_t flagC) {
  const int sj = fmod_(along, 12);
  const uint32_t stone = mul(P.kerb, 0.95f + ((hash2(fdiv_(along, 12), side, 2751) >> 8) & 255) / 255.0f * 0.10f);
  const bool jt = sj == 0;
  if (side == 0) {
    switch (d) {
      case 0: return mul(P.kerb, 0.40f);                                       // the foot (contact shade)
      case 1: return mul(stone, jt ? 0.46f : 0.56f);                           // the face, lower course
      case 2: return mul(stone, jt ? 0.52f : (up <= 1 && hashf(along, 3, 2753) < 0.2f ? 0.58f : 0.68f));   // the face
      case 3: return jt ? mul(stone, 0.80f) : mul(stone, 1.07f);               // the lit arris
      case 4: return jt ? mul(stone, 0.74f) : mul(stone, 0.96f);               // the kerb's top
      default: return mul(flagC, 0.70f);                                       // the joint to the flags
    }
  }
  const bool lit = side != 3;
  switch (d) {
    case 0: return mul(P.kerb, lit ? 0.62f : 0.46f);                           // the step down, seen from above
    case 1: return jt ? mul(stone, 0.78f) : mul(stone, lit ? 1.07f : 0.80f);   // the arris (lit west / north, shaded east)
    case 2: return jt ? mul(stone, 0.74f) : mul(stone, 0.96f);
    default: return mul(flagC, 0.72f);
  }
}
// (M7c fixer r3, review: "a shadowed ledge seam where they meet, so one street reads as a quilt of separate rafts") in
// a plank town the walk is the same deck one board higher than the carriageway: a step a board thick (a dark foot, the
// board's edge, its lit arris), not a timber kerb beam
inline uint32_t deckStepPx(int side, int d, uint32_t fl) {
  if (side == 0) return d == 0 ? mul(fl, 0.50f) : (d == 1 ? mul(fl, 0.70f) : (d == 2 ? mul(fl, 1.10f) : fl));
  const bool lit = side != 3;
  return d == 0 ? mul(fl, lit ? 0.72f : 0.60f) : (d == 1 ? mul(fl, lit ? 1.08f : 0.88f) : fl);
}
// (M7c fixer r3, review: "rough-cobble footpaths cut through the kerbs in jade cities: where a doorway path meets a
// kerbed street it should end in a dropped kerb") a lane tile that crosses the line of a walk (walk on both its sides,
// the kerbed carriageway at one end) is laid as the walk with a dropped kerb at the street: the walk's flags and kerb
// line run on across the lane's mouth, the kerb stone there laid flush, and the lane begins behind the walk
template <class TM>
inline bool droppedLane(const TM& m, int x, int y) {
  const Urb u = ucls(m, x, y);
  if (u != Urb::Lane && u != Urb::Alley) return false;
  auto walk = [&](int a, int b) { return ucls(m, a, b) == Urb::Sidewalk; };
  auto carr = [&](int a, int b) { const Urb q = ucls(m, a, b); return q == Urb::Avenue || q == Urb::Street || q == Urb::Gutter; };
  if (walk(x - 1, y) && walk(x + 1, y) && (carr(x, y - 1) != carr(x, y + 1))) return true;
  if (walk(x, y - 1) && walk(x, y + 1) && (carr(x - 1, y) != carr(x + 1, y))) return true;
  return false;
}
// what a raised walk counts as lower than (its kerb shows there)
template <class TM>
inline bool urbLow(const TM& m, int x, int y) {
  if (droppedLane(m, x, y)) return false;
  const Urb u = ucls(m, x, y);
  if (ew::urbCarriage(u) || u == Urb::Gutter) return true;
  if (u != Urb::None) return false;
  const Ground q = m.at(x, y);
  // (fixer M7b r3, review: "a partial second kerb lip inside the poor quarter's walk") a patch of bare dirt beside a
  // walk is open ground like the grass round it, not a street: no kerb face along a few tiles of it
  return q == Ground::Road || q == Ground::Bridge;   // a street outside the plan, a bridge's deck
}
template <class TM>
inline bool urbRaised(const TM& m, int x, int y) { return ucls(m, x, y) == Urb::Sidewalk; }
template <class TM>
inline bool urbPaved(const TM& m, int x, int y) {   // squares, closes and courts: one paved floor (their border course)
  const Urb u = ucls(m, x, y);
  return u == Urb::Square || u == Urb::Close || u == Urb::Court;
}
// the class a neighbouring carriageway has (for a gutter's or a corner's carriage pixel)
template <class TM>
inline Urb nearCarriage(const TM& m, int tx, int ty) {
  static const int DX[8] = {0, 1, 0, -1, 1, 1, -1, -1}, DY[8] = {1, 0, -1, 0, 1, -1, 1, -1};
  for (int k = 0; k < 8; k++) { const Urb u = ucls(m, tx + DX[k], ty + DY[k]); if (ew::urbCarriage(u)) return u; }
  return Urb::Street;
}
// the paving mark of the fabric at a tile (the culture's, through its own tile or a neighbour's)
template <class TM>
inline int urbMat(const TM& m, int tx, int ty) {
  int pm = paveMatAt(m, tx, ty);
  if (pm < 0) for (int k = 0; k < 4 && pm < 0; k++) pm = paveMatAt(m, tx + (k == 0) - (k == 1), ty + (k == 2) - (k == 3));
  pm = std::clamp(pm, 0, 11);
  return pm == 11 ? 9 : pm;   // (M7c fixer: the river city's fabric is the river towns')
}
// distance in px from the pixel to the edge of the run of tiles that pass `in` (looked up to 3 tiles each way), and
// the side it is nearest (0 S, 1 N, 2 W, 3 E)
template <class TM, class F>
inline int edgeDist(const TM&, int tx, int ty, int lx, int ly, F in, int& side) {
  int best = 99;
  side = 0;
  auto run = [&](int dx, int dy, int base, int sd) {
    for (int k = 1; k <= 3; k++) {
      if (!in(tx + dx * k, ty + dy * k)) { const int d = base + (k - 1) * 16; if (d < best) { best = d; side = sd; } return; }
    }
  };
  run(0, 1, 15 - ly, 0);
  run(0, -1, ly, 1);
  run(-1, 0, lx, 2);
  run(1, 0, 15 - lx, 3);
  return best;
}
// lawn (parks, closes' beds, gardens)
inline uint32_t lawnPx(int px, int py, bool dark, Biome b) {
  const float n = toneField(px, py, 2761);
  uint32_t c = dark ? pick3(n, C(70, 122, 50), C(78, 132, 54), C(88, 142, 60)) : pick3(n, C(84, 142, 58), C(94, 154, 64), C(104, 164, 70));
  const int tf = tuft(px, py, 2763, 0.45f);
  if (tf == 1) c = mul(c, 0.78f); else if (tf == 2) c = mul(c, 1.14f);
  const float h = hashf(px, py, 2765);
  if (!dark && h < 0.006f) c = C(238, 236, 220);        // a daisy
  else if (!dark && h < 0.010f) c = C(232, 206, 84);    // a buttercup
  if (b == Biome::Snow) c = lerpc(c, C(226, 232, 242), 0.62f);
  else if (b == Biome::Desert) c = lerpc(c, C(150, 150, 84), 0.25f);
  return c;
}
inline uint32_t gravelPx(int px, int py) {
  const float n = toneField(px, py, 2771), h = hashf(px, py, 2773);
  // (fixer M7b r2, review: "the park paths are wobbly, speckled noise bands") a raked gravel: a soft tone, a few pebbles
  uint32_t c = lerpc(C(180, 166, 140), C(196, 184, 158), std::clamp(n, 0.0f, 1.0f));
  if (h < 0.035f) c = C(212, 202, 180);
  else if (h > 0.975f) c = C(150, 138, 116);
  return c;
}
template <class TM>
bool closeBed(const TM& m, int x, int y) {
  if (ucls(m, x, y) != Urb::Close) return false;
  if (fmod_(x, 3) == 0 || fmod_(y, 3) == 0) return false;
  for (int oy = -1; oy <= 1; oy++)
    for (int ox = -1; ox <= 1; ox++) if (ucls(m, x + ox, y + oy) != Urb::Close) return false;
  return true;
}
// the face of a quay (or any paved edge) on the water tile below it: ly 0..4 (0: none)
template <class TM>
uint32_t quayFacePx(const TM& m, int px, int py) {
  const int tx = px >> 4, ty = py >> 4, ly = py & 15;
  const Urb n = ucls(m, tx, ty - 1);
  if (n == Urb::None || ew::urbGreen(n) || ly > 4) return 0;
  const UrbPal P = urbPal(urbMat(m, tx, ty - 1));
  const uint32_t st = mul(P.kerb, 0.92f + ((hash2(fdiv_(px + (ly > 1 ? 5 : 0), 9), ty, 2781) >> 8) & 255) / 255.0f * 0.10f);
  const bool jt = fmod_(px + (ly > 1 ? 5 : 0), 9) == 0;
  switch (ly) {
    case 0: return jt ? mul(st, 0.56f) : mul(st, 0.74f);
    case 1: return mul(st, 0.50f);
    case 2: return jt ? mul(st, 0.44f) : mul(st, 0.62f);
    case 3: return jt ? mul(st, 0.40f) : (hashf(px, 3, 2783) < 0.3f ? lerpc(mul(st, 0.5f), C(60, 90, 60), 0.4f) : mul(st, 0.54f));   // weed at the waterline
    default: return C(34, 48, 56, 230);   // the wet line
  }
}
// (fixer M7b r1, review: parks) a park's pond: the water's field blended between tile centres (a staircase's diagonal
// kept, a wobble of noise), so its edge meanders and its corners round off instead of following the tile grid. Land
// pixels of the pond's own tiles and the lawn beside it take a bank of wet earth and pebbles.
template <class TM>
inline float pondField(const TM& m, int px, int py) {
  auto wt = [&](int x, int y) -> float { return groundWater(m.at(x, y)) || m.at(x, y) == Ground::Ice ? 1.0f : 0.0f; };
  auto bil = [&](float qx, float qy) {
    const float fx = (qx - 7.5f) / 16.0f, fy = (qy - 7.5f) / 16.0f;
    const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    const float ax = fx - ix, ay = fy - iy;
    const float w00 = wt(ix, iy), w10 = wt(ix + 1, iy), w01 = wt(ix, iy + 1), w11 = wt(ix + 1, iy + 1);
    float v = (w00 * (1 - ax) + w10 * ax) * (1 - ay) + (w01 * (1 - ax) + w11 * ax) * ay;
    if (w00 == w11 && w10 == w01 && w00 != w10) v = std::max(v, 1.0f - (w00 > 0.5f ? std::fabs(ax - ay) : std::fabs(ax + ay - 1.0f)) * 0.9f);
    return v;
  };
  // (a box of four samples half a tile apart: the corners round off over most of a tile)
  const float v = (bil(px - 6.0f, py - 6.0f) + bil(px + 6.0f, py - 6.0f) + bil(px - 6.0f, py + 6.0f) + bil(px + 6.0f, py + 6.0f)) * 0.25f;
  return v + (vnoise(px / 11.0f, py / 11.0f, 2861) - 0.5f) * 0.42f + (vnoise(px / 3.0f, py / 3.0f, 2863) - 0.5f) * 0.08f;
}
inline uint32_t pondBankPx(int px, int py, float v, Biome b) {
  // v: the pond field (about 0.38 .. 0.62 on the bank; the water's side darker and wetter)
  const float h = hashf(px, py, 2865);
  uint32_t c = v > 0.54f ? C(92, 78, 56) : v > 0.47f ? C(112, 94, 66) : C(126, 108, 76);
  if (h < 0.10f) c = C(150, 140, 120);                    // a pebble
  else if (h > 0.93f) c = C(76, 64, 46);
  if (v > 0.58f) c = lerpc(c, C(60, 86, 96), 0.35f);      // wet at the waterline
  if (b == Biome::Snow) c = lerpc(c, C(226, 232, 242), 0.55f);
  return c;
}
// (fixer M7b r1) the drain's channel near a gutter tile: cd the distance (px) from the pixel to the channel's centre
// line, cs > 0 on its north / west (lit) side, cal the distance along it (for the joints). Nodes: each gutter tile's
// centre averaged with the gutter tiles in its 3x3 (a two-tile band's middle, a staircase's diagonal); the line: the
// links between the nodes of 8-neighbouring gutter tiles round the pixel's tile. Cached per tile and thread.
template <class TM>
inline void gutterChannel(const TM& m, int tx, int ty, int px, int py, float& cd, float& cs, float& cal, bool* slant = nullptr) {
  struct Seg { float ax, ay, bx, by; };
  struct Cache { const void* map = nullptr; int ox = 0, oy = 0, tx = INT32_MIN, ty = INT32_MIN; int n = 0; Seg s[40]; };
  thread_local Cache cc;
  if (cc.map != (const void*)m.m || cc.ox != m.ox || cc.oy != m.oy || cc.tx != tx || cc.ty != ty) {
    cc.map = (const void*)m.m; cc.ox = m.ox; cc.oy = m.oy; cc.tx = tx; cc.ty = ty; cc.n = 0;
    // ((M7c fixer, review: "the gutter of the diagonal street is a chain of 1-tile stair steps") the nodes are smoothed
    // twice: each the mean of the gutter tiles round it, then the mean of its gutter neighbours' nodes, so a staircase
    // of tiles at any slope becomes one straight channel (a straight run and a bend keep their line)
    bool G9[9][9];
    for (int j = 0; j < 9; j++)
      for (int i = 0; i < 9; i++) G9[j][i] = ucls(m, tx - 4 + i, ty - 4 + j) == Urb::Gutter;
    bool g[7][7];
    for (int j = 0; j < 7; j++)
      for (int i = 0; i < 7; i++) g[j][i] = G9[j + 1][i + 1];
    float n1x[7][7], n1y[7][7];
    for (int j = 0; j < 7; j++)
      for (int i = 0; i < 7; i++) {
        float sx = 0, sy = 0; int k = 0;
        for (int oy = -1; oy <= 1; oy++)
          for (int ox = -1; ox <= 1; ox++)
            if (G9[j + 1 + oy][i + 1 + ox]) { sx += (float)ox; sy += (float)oy; k++; }
        n1x[j][i] = (tx - 3 + i) * 16.0f + 7.5f + (k ? sx / (float)k : 0.0f) * 16.0f;
        n1y[j][i] = (ty - 3 + j) * 16.0f + 7.5f + (k ? sy / (float)k : 0.0f) * 16.0f;
      }
    float nx[5][5], ny[5][5];
    for (int j = 0; j < 5; j++)
      for (int i = 0; i < 5; i++) {
        float sx = n1x[j + 1][i + 1], sy = n1y[j + 1][i + 1];
        int k = 1;
        if (g[j + 1][i + 1])
          for (int oy = -1; oy <= 1; oy++)
            for (int ox = -1; ox <= 1; ox++)
              if ((ox || oy) && g[j + 1 + oy][i + 1 + ox]) { sx += n1x[j + 1 + oy][i + 1 + ox]; sy += n1y[j + 1 + oy][i + 1 + ox]; k++; }
        nx[j][i] = sx / (float)k;
        ny[j][i] = sy / (float)k;
      }
    // links among the 3x3 round the tile and out to their 8 neighbours (each pair once)
    for (int j = 1; j <= 3; j++)
      for (int i = 1; i <= 3; i++) {
        if (!g[j + 1][i + 1]) continue;
        for (int oy = -1; oy <= 1; oy++)
          for (int ox = -1; ox <= 1; ox++) {
            if (!ox && !oy) continue;
            const int i2 = i + ox, j2 = j + oy;
            if (!g[j2 + 1][i2 + 1]) continue;
            const bool inner2 = i2 >= 1 && i2 <= 3 && j2 >= 1 && j2 <= 3;
            if (inner2 && (j2 * 5 + i2) < (j * 5 + i)) continue;   // (counted from the other end)
            // a diagonal link only where no orthogonal pair already joins the two (a staircase's corner keeps one path)
            if (ox && oy && (g[j + 1][i2 + 1] || g[j2 + 1][i + 1])) continue;
            if (cc.n < 40) cc.s[cc.n++] = Seg{nx[j][i], ny[j][i], nx[j2][i2], ny[j2][i2]};
          }
      }
    if (cc.n == 0 && g[3][3]) cc.s[cc.n++] = Seg{nx[2][2] - 8.0f, ny[2][2], nx[2][2] + 8.0f, ny[2][2]};   // a lone tile
  }
  cd = 99.0f; cs = 1.0f; cal = (float)px;
  const float qx = px + 0.5f, qy = py + 0.5f;
  for (int k = 0; k < cc.n; k++) {
    const Seg& S = cc.s[k];
    const float dx = S.bx - S.ax, dy = S.by - S.ay, L2 = dx * dx + dy * dy;
    float t = L2 > 0.01f ? ((qx - S.ax) * dx + (qy - S.ay) * dy) / L2 : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    const float cx = S.ax + dx * t, cy = S.ay + dy * t;
    const float ex = qx - cx, ey = qy - cy;
    const float d = std::sqrt(ex * ex + ey * ey);
    if (d < cd) {
      cd = d;
      // the side: the normal that points up / left is the lit wall's side
      float nX = -dy, nY = dx;
      if (nX + nY > 0) { nX = -nX; nY = -nY; }
      cs = (ex * nX + ey * nY) >= 0 ? 1.0f : -1.0f;
      cal = (std::fabs(dx) < 0.01f || std::fabs(dy) < 0.01f) ? (std::fabs(dx) >= std::fabs(dy) ? qx : qy) : (qx * std::fabs(dx) + qy * std::fabs(dy) * (dx * dy > 0 ? 1.0f : -1.0f)) / std::sqrt(L2);   // (fixer M7b r2: joints across a slanting channel)
      if (slant) *slant = !(std::fabs(dx) < 0.01f || std::fabs(dy) < 0.01f);
    }
  }
}
// the drain's channel at a pixel cd px from its line (cs: which side, cal: along it): its wet floor and its two lips (0:
// not on the channel). (M7c fixer r3, review: "diagonal avenue gutters still draw as a stair-stepped dashed strip") on
// a slant the channel is one smooth wet runnel shaded across its width, no joints across it (joints laid across a
// slanting line broke it into a chain of square blocks on the pixel grid)
inline uint32_t chanPx(uint32_t chanS, float cd, float cs, float cal, bool slant, int px, int py) {
  if (cd <= 2.5f) {
    const bool cj = !slant && fmod_((int)std::floor(cal), 4) == 0;
    uint32_t c = mul(chanS, cj ? 0.42f : (slant ? (cd < 1.3f ? 0.54f : 0.64f) : 0.60f));
    c = lerpc(c, C(56, 72, 80), 0.28f);                                                   // wet
    if (vnoise(px / 7.0f, py / 7.0f, 2801) > 0.62f) c = lerpc(c, C(62, 82, 56), 0.35f);   // water-stained, mossy
    if (!cj && cs > 1.0f && hashf(px, py, 2803) < 0.22f) c = lerpc(c, C(150, 172, 184), 0.35f);   // a glint
    return c;
  }
  if (cd <= 4.0f) {
    uint32_t c = mul(chanS, cs > 0 ? 1.02f : 0.74f);                                      // the channel's sides
    if (!slant && fmod_((int)std::floor(cal), 6) == 0) c = mul(c, 0.8f);
    return c;
  }
  return 0;
}
// (fixer M7b r2, review: "paving edges are tile-aligned staircases", "the diagonal avenue's sidewalks are a sawtooth of
// square kerb blocks") marching squares over the tile grid. The quad of four tiles round a tile corner: when one of them
// is the odd one out (three against one; or two against two across the diagonal, where `win` says which set joins up),
// its corner is cut on the 45-degree line between the middles of its two sides: a staircase of tiles becomes one
// straight slant, its steps' corners given to the run. msKind: 0 a quad that is uniform or split along a tile edge, 1
// cut (3:1 or a saddle).
template <class F>
inline int msKind(F mem, int cxI, int cyI) {
  const bool a = mem(cxI - 1, cyI - 1), b = mem(cxI, cyI - 1), c = mem(cxI - 1, cyI), d = mem(cxI, cyI);
  const int n = (int)a + (int)b + (int)c + (int)d;
  if (n == 0 || n == 4) return 0;
  if (n == 2 && ((a == b) || (a == c))) return 0;   // split along a tile edge
  return 1;
}
// msOdd: -1 not cut, 0 / 1 the odd corner's value (3:1), 2 a saddle
template <class F>
inline int msOdd(F mem, int cxI, int cyI) {
  const bool a = mem(cxI - 1, cyI - 1), b = mem(cxI, cyI - 1), c = mem(cxI - 1, cyI), d = mem(cxI, cyI);
  const int n = (int)a + (int)b + (int)c + (int)d;
  if (n == 0 || n == 4) return -1;
  if (n == 1) return 1;
  if (n == 3) return 0;
  return (a == b || a == c) ? -1 : 2;
}
// a cut quad is a staircase's step when a quad beside it is cut the other way round (the slant runs on through it); a
// junction's corners (cut the same way, or alone) are not
template <class F>
inline bool msStep(F mem, int cxI, int cyI) {
  const int k = msOdd(mem, cxI, cyI);
  if (k < 0) return false;
  static const int DX[4] = {-1, 1, 0, 0}, DY[4] = {0, 0, -1, 1};
  for (int i = 0; i < 4; i++) {
    const int q = msOdd(mem, cxI + DX[i], cyI + DY[i]);
    if (q < 0) continue;
    if (q == 2 || k == 2 || q != k) return true;
  }
  return false;
}
// the tile the point (qx, qy) (pixel coordinates, a pixel's centre at px + 0.5) belongs to: its own, or its quad's
// neighbour along the nearer side where its own tile's corner is cut. needStep: cut only a step of a staircase (a quad
// next to it along the cut is cut too), never a lone corner (a junction keeps its square corner and corner stone).
template <class F>
inline bool msCut(F mem, float qx, float qy, bool needStep, bool win, int& otx, int& oty) {
  const float fx = qx / 16.0f, fy = qy / 16.0f;
  const int cxI = (int)std::floor(fx + 0.5f), cyI = (int)std::floor(fy + 0.5f);
  const float u = fx - (float)cxI, v = fy - (float)cyI;
  if (std::fabs(u) + std::fabs(v) >= 0.5f) return false;
  const int sx = u < 0 ? -1 : 0, sy = v < 0 ? -1 : 0;   // own tile = (cxI + sx, cyI + sy)
  const int ox = cxI + sx, oy = cyI + sy, hx = cxI - 1 - sx, vy = cyI - 1 - sy;
  const bool mo = mem(ox, oy), mh = mem(hx, oy), mv = mem(ox, vy), md = mem(hx, vy);
  bool cut = false;
  if (mh == mv && mh != mo) cut = md == mh || mo != win;   // 3:1 with this tile the odd one, or a saddle it loses
  if (!cut) return false;
  if (needStep && !msStep(mem, cxI, cyI)) return false;
  if (std::fabs(u) < std::fabs(v)) { otx = hx; oty = oy; } else { otx = ox; oty = vy; }
  return true;
}
// the kerb line round a walk tile where a staircase is cut (the walk / carriage edge as marching squares lay it): the
// segments of the four quads at the tile's corners, each with its normal toward the low side; the nearest to a pixel.
// Returns false where no quad round the tile is a cut step (the straight kerbs and corner stones draw as before).
template <class TM>
inline bool kerbLine(const TM& m, int tx, int ty, int px, int py, float& dist, float& nX, float& nY) {
  struct Seg { float ax, ay, bx, by, nx, ny; };
  struct Cache { const void* map = nullptr; int ox = 0, oy = 0, tx = INT32_MIN, ty = INT32_MIN; int n = 0; bool any = false; Seg s[16]; };
  thread_local Cache cc;
  auto low = [&](int x, int y) {
    const Urb u = ucls(m, x, y);
    return ew::urbCarriage(u) || u == Urb::Gutter;
  };
  if (cc.map != (const void*)m.m || cc.ox != m.ox || cc.oy != m.oy || cc.tx != tx || cc.ty != ty) {
    cc.map = (const void*)m.m; cc.ox = m.ox; cc.oy = m.oy; cc.tx = tx; cc.ty = ty; cc.n = 0; cc.any = false;
    for (int j = 0; j <= 1; j++)
      for (int i = 0; i <= 1; i++) {
        const int cxI = tx + i, cyI = ty + j;
        const bool a = low(cxI - 1, cyI - 1), b = low(cxI, cyI - 1), c = low(cxI - 1, cyI), d = low(cxI, cyI);
        const int nl = (int)a + (int)b + (int)c + (int)d;
        if (nl == 0 || nl == 4) continue;
        const float X = cxI * 16.0f, Y = cyI * 16.0f;   // the corner point
        const float T[2] = {X, Y - 8.0f}, B[2] = {X, Y + 8.0f}, L[2] = {X - 8.0f, Y}, R[2] = {X + 8.0f, Y};
        auto add = [&](const float* p, const float* q, float nx, float ny) {
          const float k = 1.0f / std::sqrt(nx * nx + ny * ny);
          if (cc.n < 16) cc.s[cc.n++] = Seg{p[0], p[1], q[0], q[1], nx * k, ny * k};
        };
        const bool step = msStep(low, cxI, cyI);
        if (nl == 2 && a == b) { add(L, R, 0.0f, c ? 1.0f : -1.0f); continue; }   // a straight east-west kerb
        if (nl == 2 && a == c) { add(T, B, b ? 1.0f : -1.0f, 0.0f); continue; }   // a straight north-south kerb
        // the odd corner(s): cut on the slant where a staircase steps; a lone corner keeps two straight half-kerbs
        auto corner = [&](bool val, int k) {   // k: 0 a (TL), 1 b (TR), 2 c (BL), 3 d (BR); val: is that corner low
          const float* E1 = (k == 0 || k == 1) ? T : B;   // its side along x = X
          const float* E2 = (k == 0 || k == 2) ? L : R;   // its side along y = Y
          const float dx = (k == 1 || k == 3) ? 1.0f : -1.0f, dy = (k == 2 || k == 3) ? 1.0f : -1.0f;   // toward that corner's tile
          const float s = val ? 1.0f : -1.0f;   // the normal points toward low
          if (step) { cc.any = true; add(E1, E2, dx * s, dy * s); }
          else {
            const float P[2] = {X, Y};
            add(E1, P, dx * s, 0.0f);
            add(P, E2, 0.0f, dy * s);
          }
        };
        if (nl == 1 || nl == 3) {
          const bool odd = nl == 1;   // the odd corner's value
          const int k = a == odd ? 0 : b == odd ? 1 : c == odd ? 2 : 3;
          corner(odd, k);
        } else {   // a saddle: the low tiles join (both walk corners cut)
          if (!a) corner(false, 0);
          if (!b) corner(false, 1);
          if (!c) corner(false, 2);
          if (!d) corner(false, 3);
        }
      }
  }
  if (!cc.any) return false;
  dist = 99.0f; nX = 0.0f; nY = 1.0f;
  const float qx = px + 0.5f, qy = py + 0.5f;
  for (int k = 0; k < cc.n; k++) {
    const Seg& S = cc.s[k];
    const float dx = S.bx - S.ax, dy = S.by - S.ay, L2 = dx * dx + dy * dy;
    float t = L2 > 0.01f ? ((qx - S.ax) * dx + (qy - S.ay) * dy) / L2 : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    const float ex = qx - (S.ax + dx * t), ey = qy - (S.ay + dy * t);
    const float d = std::sqrt(ex * ex + ey * ey);
    if (d < dist) { dist = d; nX = S.nx; nY = S.ny; }
  }
  return true;
}
// (M7c ART, VISION_PLAN 15.M7c: "grid sections may tilt", "a few run diagonal") the staircase edge of a set of tiles
// drawn as one straight line on ANY slope, not only 1:1 (msCut / kerbLine cut 1:1 steps; a 1:2 or 1:3 tilted street
// came out as a sawtooth of short slants and flats). The set's boundary within 5 tiles of the tile is traced as chains
// of unit edges (the set on the left), compressed into straight runs; a corner is a STAIR corner when a one-tile run
// beside it turns the other way at its far end (a step of a staircase of any tread length, a one-tile jog), else a
// built corner kept square (a block's corner, a T's mouth, a walk's end). The line runs through the middles of the runs
// at stair corners (at most two tiles from the corner) and through the corner itself elsewhere, so a regular staircase
// of treads n and risers 1 becomes one straight line of slope 1:n, and junction corners keep their corner stones.
// Saddles (two set tiles touching at a corner): `win` joins the set's tiles. Cached per tile and call site.
struct StairCache {
  const void* map = nullptr;
  int ox = 0, oy = 0, tx = INT32_MIN, ty = INT32_MIN;
  bool any = false;
  int np = 0, nc = 0;
  float px[192], py[192];
  int cs[24], ce[24];   // chain c: points [cs, ce)
  bool closed[24];
};
template <class TM, class F>
bool stairEdge(const TM& m, F mem, bool win, int tx, int ty, float qx, float qy, StairCache& cc, float& sd, float& nX, float& nY) {
  constexpr int R = 5, N = 2 * R + 1;
  if (cc.map != (const void*)m.m || cc.ox != m.ox || cc.oy != m.oy || cc.tx != tx || cc.ty != ty) {
    cc.map = (const void*)m.m; cc.ox = m.ox; cc.oy = m.oy; cc.tx = tx; cc.ty = ty;
    cc.any = false; cc.np = 0; cc.nc = 0;
    bool in[N][N];
    for (int j = 0; j < N; j++)
      for (int i = 0; i < N; i++) in[j][i] = mem(tx - R + i, ty - R + j);
    // directed unit edges (0 E, 1 S, 2 W, 3 N) from each lattice vertex (N + 1 by N + 1), the set on their left
    uint8_t outN[N + 1][N + 1] = {}, outD[N + 1][N + 1][2] = {}, inN[N + 1][N + 1] = {};
    bool used[N + 1][N + 1][4] = {};
    auto addE = [&](int vx, int vy, int d) {
      if (outN[vy][vx] < 2) outD[vy][vx][outN[vy][vx]++] = (uint8_t)d;
      static const int DX[4] = {1, 0, -1, 0}, DY[4] = {0, 1, 0, -1};
      inN[vy + DY[d]][vx + DX[d]]++;
    };
    for (int vj = 1; vj < N; vj++)
      for (int i = 0; i < N; i++)
        if (in[vj - 1][i] != in[vj][i]) { if (in[vj - 1][i]) addE(i, vj, 0); else addE(i + 1, vj, 2); }
    for (int vi = 1; vi < N; vi++)
      for (int j = 0; j < N; j++)
        if (in[j][vi - 1] != in[j][vi]) { if (in[j][vi]) addE(vi, j, 1); else addE(vi, j + 1, 3); }
    static const int DX[4] = {1, 0, -1, 0}, DY[4] = {0, 1, 0, -1};
    auto pick = [&](int vx, int vy, int din) -> int {   // the outgoing edge taken after arriving along din (-1: none)
      int best = -1;
      for (int k = 0; k < outN[vy][vx]; k++) {
        const int d = outD[vy][vx][k];
        if (used[vy][vx][d]) continue;
        if (best < 0) best = d;
        if (din >= 0 && outN[vy][vx] == 2 && d == (win ? (din + 1) & 3 : (din + 3) & 3)) best = d;
      }
      return best;
    };
    // trace: chains that enter from the window's border first, then the closed loops
    for (int pass = 0; pass < 2; pass++)
      for (int sy = 0; sy <= N; sy++)
        for (int sx = 0; sx <= N; sx++) {
          if (pass == 0 && inN[sy][sx] != 0) continue;
          for (;;) {
            int d0 = pick(sx, sy, -1);
            if (d0 < 0 || cc.nc >= 24) break;
            // the runs of this chain
            int runD[96], runL[96], nr = 0;
            int vx = sx, vy = sy, d = d0;
            bool closedLoop = false;
            while (d >= 0) {
              used[vy][vx][d] = true;
              if (nr > 0 && runD[nr - 1] == d) runL[nr - 1]++;
              else if (nr < 96) { runD[nr] = d; runL[nr] = 1; nr++; }
              vx += DX[d]; vy += DY[d];
              if (vx == sx && vy == sy) { closedLoop = true; break; }
              d = pick(vx, vy, d);
            }
            if (nr == 0) break;
            int s0x = sx, s0y = sy;
            if (closedLoop && nr > 1 && runD[0] == runD[nr - 1]) {   // a loop: start it at a corner
              runL[0] += runL[nr - 1];
              s0x -= DX[runD[0]] * runL[nr - 1]; s0y -= DY[runD[0]] * runL[nr - 1];
              nr--;
            }
            // the corners (corner k: between run k and run k + 1, wrapping on a loop); turn +1 right, -1 left
            auto turnAt = [&](int k) -> int {   // k in [0, nr) for a loop, [0, nr - 1) for an open chain; else 0
              if (!closedLoop && (k < 0 || k >= nr - 1)) return 0;
              const int a = runD[((k % nr) + nr) % nr], b = runD[(((k + 1) % nr) + nr) % nr];
              return b == ((a + 1) & 3) ? 1 : (b == ((a + 3) & 3) ? -1 : 0);
            };
            auto partial = [&](int k) { return !closedLoop && (k == 0 || k == nr - 1); };
            auto stair = [&](int k) {   // corner k is a step
              const int t = turnAt(k);
              if (!t) return false;
              const int a = ((k % nr) + nr) % nr, b = (((k + 1) % nr) + nr) % nr;
              if (runL[a] == 1 && !partial(a) && turnAt(k - 1) == -t) return true;
              if (runL[b] == 1 && !partial(b) && turnAt(k + 1) == -t) return true;
              return false;
            };
            const float X0 = (tx - R) * 16.0f, Y0 = (ty - R) * 16.0f;
            int cx = s0x, cy = s0y;
            const int first = cc.np;
            auto push = [&](float x, float y) {
              if (cc.np > first && std::fabs(cc.px[cc.np - 1] - x) < 0.01f && std::fabs(cc.py[cc.np - 1] - y) < 0.01f) return;
              if (cc.np < 192) { cc.px[cc.np] = x; cc.py[cc.np] = y; cc.np++; }
            };
            for (int k = 0; k < nr; k++) {
              const float sxp = X0 + cx * 16.0f, syp = Y0 + cy * 16.0f;
              const float ux = (float)DX[runD[k]], uy = (float)DY[runD[k]];
              const float L = runL[k] * 16.0f, h = std::min(L * 0.5f, 32.0f);
              const bool st0 = (closedLoop || k > 0) && stair(k - 1), st1 = (closedLoop || k < nr - 1) && stair(k);
              if (st0 || st1) {
                const int vxk = cx, vyk = cy;
                if (vxk >= R - 1 && vxk <= R + 2 && vyk >= R - 1 && vyk <= R + 2) cc.any = true;
                const int ex = cx + DX[runD[k]] * runL[k], ey = cy + DY[runD[k]] * runL[k];
                if (ex >= R - 1 && ex <= R + 2 && ey >= R - 1 && ey <= R + 2) cc.any = true;
              }
              if (!st0) push(sxp, syp);
              else push(sxp + ux * h, syp + uy * h);
              if (st1 && (!st0 || L - h > h + 0.01f)) push(sxp + ux * (L - h), syp + uy * (L - h));
              cx += DX[runD[k]] * runL[k]; cy += DY[runD[k]] * runL[k];
              if (!closedLoop && k == nr - 1) push(X0 + cx * 16.0f, Y0 + cy * 16.0f);
            }
            cc.cs[cc.nc] = first; cc.ce[cc.nc] = cc.np; cc.closed[cc.nc] = closedLoop; cc.nc++;
          }
        }
  }
  if (!cc.any) return false;
  // the nearest segment; the side by the segment's left normal (at a shared end point: both segments' sides, joined by
  // the turn: a convex corner of the set is inside both, a concave one inside either)
  float best = 1e9f;
  int bc = -1, bs = -1;
  float bt = 0;
  for (int c2 = 0; c2 < cc.nc; c2++) {
    const int a0 = cc.cs[c2], a1 = cc.ce[c2], n = a1 - a0;
    const int segs = cc.closed[c2] ? n : n - 1;
    for (int s = 0; s < segs; s++) {
      const int i0 = a0 + s, i1 = a0 + (s + 1) % n;
      const float ax = cc.px[i0], ay = cc.py[i0], dx = cc.px[i1] - ax, dy = cc.py[i1] - ay, L2 = dx * dx + dy * dy;
      float t = L2 > 1e-4f ? ((qx - ax) * dx + (qy - ay) * dy) / L2 : 0.0f;
      t = std::clamp(t, 0.0f, 1.0f);
      const float ex = qx - (ax + dx * t), ey = qy - (ay + dy * t), d2 = ex * ex + ey * ey;
      if (d2 < best) { best = d2; bc = c2; bs = s; bt = t; }
    }
  }
  if (bc < 0) return false;
  const int a0 = cc.cs[bc], n = cc.ce[bc] - a0;
  auto segDir = [&](int s, float& dx, float& dy) {
    const int i0 = a0 + ((s % n) + n) % n, i1 = a0 + (((s + 1) % n) + n) % n;
    dx = cc.px[i1] - cc.px[i0]; dy = cc.py[i1] - cc.py[i0];
    const float l = std::sqrt(dx * dx + dy * dy);
    if (l > 1e-4f) { dx /= l; dy /= l; }
  };
  auto sideOf = [&](int s) {   // + on the segment's left (the set's side)
    float dx, dy;
    segDir(s, dx, dy);
    const int i0 = a0 + ((s % n) + n) % n;
    return (qx - cc.px[i0]) * dy - (qy - cc.py[i0]) * dx;
  };
  float dx, dy;
  segDir(bs, dx, dy);
  nX = dy; nY = -dx;
  const float d = std::sqrt(best);
  bool inside = sideOf(bs) > 0;
  const int segs = cc.closed[bc] ? n : n - 1;
  int other = -1;
  if (bt <= 0.0f && (cc.closed[bc] || bs > 0)) other = bs - 1;
  else if (bt >= 1.0f && (cc.closed[bc] || bs < segs - 1)) other = bs + 1;
  if (other >= 0 || (other < 0 && cc.closed[bc] && (bt <= 0.0f || bt >= 1.0f))) {
    if (other < 0) other = bt <= 0.0f ? bs - 1 : bs + 1;
    float d1x, d1y, d2x, d2y;
    const int s1 = bt <= 0.0f ? other : bs, s2 = bt <= 0.0f ? bs : other;
    segDir(s1, d1x, d1y); segDir(s2, d2x, d2y);
    const bool leftTurn = d1x * d2y - d1y * d2x < 0;   // (y down: a left turn bends toward the set: a convex corner of it)
    const bool a = sideOf(s1) > 0, b = sideOf(s2) > 0;
    inside = leftTurn ? (a && b) : (a || b);
    // the normal of a corner: the mean of its two segments'
    nX = (d1y + d2y) * 0.5f; nY = -(d1x + d2x) * 0.5f;
    const float l = std::sqrt(nX * nX + nY * nY);
    if (l > 1e-4f) { nX /= l; nY /= l; }
  }
  sd = inside ? d : -d;
  return true;
}
// one pixel of the fabric (tile class u from byte ub)
template <class TM>
uint32_t urbPixel(const TM& m, int px, int py, uint8_t ub, int tx, int ty) {
  // (tx, ty): the tile whose class paints the pixel (its own, or a neighbour's where a soft edge wobbles over the line;
  // lx / ly may then lie a few px outside 0..15)
  const int lx = px - tx * 16, ly = py - ty * 16;
  const Urb u = ew::urbClass(ub);
  const int up = ew::urbUpkeep(ub);
  const int pm = urbMat(m, tx, ty);
  const UrbPal P = urbPal(pm);
  bool joint = false;
  uint32_t c = 0;
  auto water = [&](int x, int y) { return groundWater(m.at(x, y)); };
  // EMB_URBDBG=1: each class in a flat colour (judging what the plan put where)
  static const bool dbg = std::getenv("EMB_URBDBG") != nullptr;
  if (dbg) {
    static const uint32_t k[14] = {C(0, 0, 0), C(200, 60, 60), C(220, 140, 60), C(200, 200, 60), C(120, 90, 50), C(240, 240, 240), C(40, 60, 200),
                                   C(160, 60, 200), C(60, 200, 200), C(60, 180, 60), C(200, 220, 160), C(20, 100, 20), C(255, 120, 200), C(30, 30, 120)};
    return ((lx == 0 || ly == 0) ? mul(k[(int)u], 0.8f) : k[(int)u]);
  }
  // (fixer M7b r2, review: "the diagonal avenue's sidewalks are a sawtooth staircase of square kerb blocks") where the
  // walk and its carriageway step on a slant, the kerb runs on the slant: the tiles' staircase is cut by marching
  // squares (msCut / kerbLine), the walk's flags and its kerb (the face where the road lies south of it, the lit or the
  // shaded arris where it lies north, west or east) laid along the cut line, the road's setts below it
  bool handled = false;
  if ((u == Urb::Sidewalk || ew::urbCarriage(u)) && (px >> 4) == tx && (py >> 4) == ty && !droppedLane(m, tx, ty)) {
    float kd = 99.0f, nX = 0.0f, nY = 1.0f, sd = 0.0f;
    auto lowF = [&](int x, int y) { const Urb q = ucls(m, x, y); return (ew::urbCarriage(q) || q == Urb::Gutter) && !droppedLane(m, x, y); };
    // (M7c ART) the walk / carriageway edge on any slope (stairEdge); a carriageway tile is cut only where a walk is
    // beside it (a lane against the open ground keeps its soft edge)
    bool walkNear = u == Urb::Sidewalk;
    for (int oy = -1; oy <= 1 && !walkNear; oy++)
      for (int ox = -1; ox <= 1; ox++) if (urbRaised(m, tx + ox, ty + oy)) { walkNear = true; break; }
    thread_local StairCache kerbSC;
    if (walkNear && stairEdge(m, lowF, true, tx, ty, px + 0.5f, py + 0.5f, kerbSC, sd, nX, nY)) {
      handled = true;
      kd = std::fabs(sd);
      const bool isLow = sd > 0.0f;
      int ctx = (int)std::floor((px + 0.5f + nX * (kd + 1.0f)) / 16.0f), cty = (int)std::floor((py + 0.5f + nY * (kd + 1.0f)) / 16.0f);
      if (isLow) {
        const Urb cq = ucls(m, ctx, cty);
        const Urb cu = ew::urbCarriage(u) ? u : (ew::urbCarriage(cq) ? cq : nearCarriage(m, tx, ty));
        c = carriagePx(pm, cu, up, px, py);
        if (kd < 2.0f && (nY > 0.3f || nX > 0.3f)) c = mul(c, kd < 1.0f ? 0.72f : 0.87f);   // the kerb's cast shadow
      } else {
        uint32_t fl = (pm == 4 || pm == 10) ? pavePixel(4, true, px, py, 0.0f, -1.0f, hashf(px, py, 2791), 0.5f)
                              : flagPx(px, py, P.flag, 2793, up >= 3 ? 14 : 11, joint);
        fl = urbWear(fl, up, px, py, joint);
        c = fl;
        int side, dd, lim;
        if (nY >= 0.38f) { side = 0; dd = (int)std::floor(kd / nY); lim = 6; }
        else if (nY <= -0.75f) { side = 1; dd = (int)kd; lim = 4; }
        else { side = nX < 0 ? 2 : 3; dd = (int)kd; lim = 4; }
        if (dd < lim) c = pm == 4 ? deckStepPx(side, dd, fl) : kerbPx(P, side, dd, side <= 1 ? px : py, up, fl);
      }
    }
  }
  if (!handled && droppedLane(m, tx, ty) && (px >> 4) == tx && (py >> 4) == ty && pm != 4) {
    uint32_t fl = pm == 10 ? pavePixel(4, true, px, py, 0.0f, -1.0f, hashf(px, py, 2791), 0.5f) : flagPx(px, py, P.flag, 2793, up >= 3 ? 14 : 11, joint);
    c = urbWear(fl, up, px, py, joint);
    auto carr = [&](int a, int b) { const Urb q = ucls(m, a, b); return q == Urb::Avenue || q == Urb::Street || q == Urb::Gutter; };
    int d = 99, side = -1;
    if (carr(tx, ty + 1)) { d = 15 - ly; side = 0; }
    else if (carr(tx, ty - 1)) { d = ly; side = 1; }
    else if (carr(tx - 1, ty)) { d = lx; side = 2; }
    else if (carr(tx + 1, ty)) { d = 15 - lx; side = 3; }
    if (side >= 0 && d <= 3) {   // the dropped kerb: the kerb stone laid flush, a fine joint, a lit chamfer
      const int along = side <= 1 ? px : py;
      const uint32_t st = mul(P.kerb, 0.95f + ((hash2(fdiv_(along, 12), side, 2751) >> 8) & 255) / 255.0f * 0.10f);
      c = d == 0 ? mul(st, 0.62f) : (d == 1 ? mul(st, side == 3 ? 0.86f : 1.04f) : (d == 2 ? mul(st, 0.94f) : mul(fl, 0.72f)));
      if (fmod_(along, 12) == 0) c = mul(c, 0.82f);
    }
    // the walk's own kerb on its far side runs on across the mouth too, a step down to the lane behind it
    if (side >= 0) {
      const int os = side == 0 ? 1 : (side == 1 ? 0 : (side == 2 ? 3 : 2));
      const int ox = os == 2 ? -1 : (os == 3 ? 1 : 0), oy = os == 0 ? 1 : (os == 1 ? -1 : 0);
      const int od = os == 0 ? 15 - ly : (os == 1 ? ly : (os == 2 ? lx : 15 - lx));
      if (urbLow(m, tx + ox, ty + oy) && od < (os == 0 ? 6 : 4)) c = kerbPx(P, os, od, os <= 1 ? px : py, up, fl);
    }
    return c;
  }
  if (!handled) switch (u) {
    case Urb::Avenue: case Urb::Street: case Urb::Lane: case Urb::Alley: {
      c = carriagePx(pm, u, up, px, py);
      // (M7c fixer r3, review: "diagonal avenue gutters still draw as a stair-stepped dashed strip", "the centre gutter
      // of a curved branch breaks into separate dark segments") the drain's channel is one smooth line through the
      // staircase of its tiles (gutterChannel), so on a slant or a bend it crosses the carriageway tiles beside the
      // staircase too: they draw their part of it, and the channel runs on unbroken
      {
        bool gNear = false;
        for (int oy = -1; oy <= 1 && !gNear; oy++)
          for (int ox = -1; ox <= 1; ox++) if ((ox || oy) && ucls(m, tx + ox, ty + oy) == Urb::Gutter) { gNear = true; break; }
        if (gNear && (px >> 4) == tx && (py >> 4) == ty) {
          float cd, cs, cal;
          bool sl = false;
          gutterChannel(m, tx, ty, px, py, cd, cs, cal, &sl);
          if (const uint32_t k = chanPx(pm == 9 ? C(116, 112, 114) : P.sett, cd, cs, cal, sl, px, py)) { c = k; break; }
        }
      }
      // the raised walks beside it: the kerb's cast shadow (light from the top-left: on the road south of a walk, under
      // its face, and east of a walk's east kerb)
      const bool rN = urbRaised(m, tx, ty - 1), rW = urbRaised(m, tx - 1, ty);
      if (rN && ly <= 1) c = mul(c, ly == 0 ? 0.70f : 0.86f);
      if (rW && lx <= 1) c = mul(c, lx == 0 ? 0.74f : 0.88f);
      if (!rN && !rW && urbRaised(m, tx - 1, ty - 1) && lx <= 1 && ly <= 1) c = mul(c, 0.80f);
      break;
    }
    case Urb::Sidewalk: {
      // the walk's own flags (planks in the marsh)
      uint32_t fl = (pm == 4 || pm == 10) ? pavePixel(4, true, px, py, 0.0f, -1.0f, hashf(px, py, 2791), 0.5f)
                            : flagPx(px, py, P.flag, 2793, up >= 3 ? 14 : 11, joint);
      fl = urbWear(fl, up, px, py, joint);
      c = fl;
      const bool lS = urbLow(m, tx, ty + 1), lN = urbLow(m, tx, ty - 1), lW = urbLow(m, tx - 1, ty), lE = urbLow(m, tx + 1, ty);
      int side = -1, d = 99;
      auto cand = [&](int sd, int dd, int lim) { if (dd < lim && dd < d) { d = dd; side = sd; } };
      if (lS) cand(0, 15 - ly, 6);
      if (lE) cand(3, 15 - lx, 4);
      if (lW) cand(2, lx, 4);
      if (lN) cand(1, ly, 4);
      // inner corners: the neighbours' bands run on into this tile's corner (no notch where the run steps)
      if (side < 0) {
        if (!lS && !lE && urbLow(m, tx + 1, ty + 1) && ly >= 10 && 15 - lx < 4) { side = 3; d = 15 - lx; }
        else if (!lS && !lW && urbLow(m, tx - 1, ty + 1) && ly >= 10 && lx < 4) { side = 2; d = lx; }
        else if (!lN && !lE && urbLow(m, tx + 1, ty - 1) && ly < 4 && 15 - lx < 4) { const int dE = 15 - lx, dN = ly; side = dE >= dN ? 3 : 1; d = std::max(dE, dN); }
        else if (!lN && !lW && urbLow(m, tx - 1, ty - 1) && ly < 4 && lx < 4) { const int dW = lx, dN = ly; side = dW >= dN ? 2 : 1; d = std::max(dW, dN); }
      }
      // outer corners: a corner stone, rounded where the kerb turns (radius 4)
      bool outside = false;
      if (lS && lE && lx >= 11 && ly >= 11) { const float dx = lx - 11.0f, dy = ly - 11.0f; outside = dx * dx + dy * dy > 18.0f; }
      if (lS && lW && lx <= 4 && ly >= 11) { const float dx = 4.0f - lx, dy = ly - 11.0f; outside = dx * dx + dy * dy > 18.0f; }
      if (lN && lE && lx >= 11 && ly <= 4) { const float dx = lx - 11.0f, dy = 4.0f - ly; outside = dx * dx + dy * dy > 18.0f; }
      if (lN && lW && lx <= 4 && ly <= 4) { const float dx = 4.0f - lx, dy = 4.0f - ly; outside = dx * dx + dy * dy > 18.0f; }
      if (outside) {   // the carriageway round the corner stone, in the corner's shade
        c = mul(carriagePx(pm, nearCarriage(m, tx, ty), up, px, py), (lS && ly >= 13) || lE ? 0.62f : 0.80f);
        break;
      }
      if (side >= 0) {
        const int along = (side <= 1) ? px : py;
        c = pm == 4 ? deckStepPx(side, d, fl) : kerbPx(P, side, d, along, up, fl);
        // a corner stone: where two kerbs meet at an outer corner, one squarer stone (a joint across each band)
        if (((lS || lN) && (lE || lW)) && ((side <= 1 && (lx == (lE ? 10 : 5))) || (side >= 2 && ly == (lS ? 9 : 5))) && d >= 1) c = mul(c, 0.78f);
        if (up == 0 && hashf(px >> 1, py >> 1, 2795) < 0.08f) c = mul(c, 0.84f);   // chipped
      }
      break;
    }
    case Urb::Gutter: {
      // the channel runs the way the gutter tiles do (else along the street beside it)
      // (integration) a drain band two tiles thick (a wide avenue's centre) ran a channel down every tile across the
      // street: the way is the longer run, and the one channel sits in the middle of the band's width
      auto isG = [&](int x, int y) { return ucls(m, x, y) == Urb::Gutter; };
      auto run = [&](int dx, int dy) { int n = 0; while (n < 4 && isG(tx + dx * (n + 1), ty + dy * (n + 1))) n++; return n; };
      const int rW = run(-1, 0), rE = run(1, 0), rN = run(0, -1), rS = run(0, 1);
      bool horiz = rW + rE > rN + rS;
      if (rW + rE == rN + rS) horiz = rW + rE > 0 ? true : (ew::urbCarriage(ucls(m, tx - 1, ty)) || ew::urbCarriage(ucls(m, tx + 1, ty)));
      const int n0 = std::min(horiz ? rN : rW, 2), n1 = std::min(horiz ? rS : rE, 2);
      const int a = (horiz ? ly : lx) + 16 * n0, along = horiz ? px : py;
      const float ac = a - (16.0f * (n0 + n1 + 1) - 1.0f) * 0.5f;
      c = carriagePx(pm, nearCarriage(m, tx, ty), up, px, py);
      // (fixer M7b r1, review: "the central drain breaks into disconnected zig-zag slots at junctions and kinks") the
      // channel is one line through the band: each gutter tile's node is the mean of the gutter tiles round it (the
      // middle of a two-tile band, a staircase's diagonal smoothed), and the channel follows the links between
      // neighbouring nodes, so it runs on unbroken through bends, chamfers and junctions
      float cd, cs, cal;
      bool gSlant = false;
      gutterChannel(m, tx, ty, px, py, cd, cs, cal, &gSlant);
      // (M7c ART) where the drain stops in the street (a block street crossing the avenue, the abutment of a bridge, the
      // walk at a dead end) it ends in an iron grate as it does at a square, not in a cut-off slot
      auto gEnd = [&](int x, int y, int qx2, int qy2) { return !isG(x, y) && !isG(x + qx2, y + qy2) && !isG(x - qx2, y - qy2); };
      const bool gEndA = horiz ? gEnd(tx - 1, ty, 0, 1) : gEnd(tx, ty - 1, 1, 0);
      const bool gEndB = horiz ? gEnd(tx + 1, ty, 0, 1) : gEnd(tx, ty + 1, 1, 0);
      {
        // (fixer M7b r2, review: "the grate sits a few pixels from where the channel ends") at a square the channel runs
        // straight on along the band's middle into its grate
        const bool sA = gEndA || (horiz ? urbPaved(m, tx - 1, ty) : urbPaved(m, tx, ty - 1)), sB = gEndB || (horiz ? urbPaved(m, tx + 1, ty) : urbPaved(m, tx, ty + 1));
        const int al0 = horiz ? lx : ly;
        if ((sA && !sB && al0 <= 12) || (sB && !sA && al0 >= 3)) {
          const float st = std::fabs(ac);
          if (st < cd) { cd = st; cs = ac < 0 ? 1.0f : -1.0f; cal = (float)along; gSlant = false; }
        }
      }
      const uint32_t chanS = pm == 9 ? C(116, 112, 114) : P.sett;   // (fixer M7b r2: the river towns' grey setts)
      if (const uint32_t k = chanPx(chanS, cd, cs, cal, gSlant && cd < 90.0f, px, py)) c = k;
      // an iron grate where the channel meets a square
      const bool sqA = gEndA || (horiz ? urbPaved(m, tx - 1, ty) : urbPaved(m, tx, ty - 1));
      const bool sqB = gEndB || (horiz ? urbPaved(m, tx + 1, ty) : urbPaved(m, tx, ty + 1));
      const int al = horiz ? lx : ly;
      if ((sqA && al <= 9) || (sqB && al >= 6)) {
        if (std::fabs(ac) <= 4.0f && al >= (sqA ? 1 : 6) && al <= (sqA ? 9 : 14)) {
          const bool frame = std::fabs(ac) >= 3.5f || al == (sqA ? 1 : 6) || al == (sqA ? 9 : 14);
          if (frame) c = (ac < 0 || al == (sqA ? 1 : 6)) ? C(96, 94, 100) : C(38, 36, 42);
          else c = ((along & 1) == 0) ? C(76, 76, 84) : C(16, 16, 20);
        }
      }
      break;
    }
    case Urb::Square: case Urb::Court: case Urb::Close: {
      const float n = toneField(px, py, 2811), h = hashf(px, py, 2813);
      if (u == Urb::Close) c = flagPx(px, py, P.flag, 2815, 13, joint);
      else if ((pm == 4 && u == Urb::Square) || (pm == 10 && (u == Urb::Square || u == Urb::Court))) {
        // (fixer M7b r1, review: "the fjord city is one unbroken plank boardwalk, squares included") the plank towns
        // lay their market and their squares in weathered grey stone: the boardwalks lead into a paved square
        c = flagPx(px, py, C(152, 150, 140), 2818, 12, joint);
        if (joint && vnoise(px / 9.0f, py / 9.0f, 2820) > 0.45f) c = C(78, 104, 64);   // moss in the joints
        else if (!joint && h < 0.05f) c = mul(c, 0.88f);
      }
      else if (pm > 0 && (pm != 2 || up < 3)) c = pavePixel(pm, false, px, py, 0.0f, -1.0f, h, n);   // (the people's own: the elves' mossy flags, the steppe's beaten earth)
      else {
        c = flagPx(px, py, pm == 5 ? P.flag : P.flag, 2817, 10, joint);
        if (pm == 5 && joint && vnoise(px / 9.0f, py / 9.0f, 2819) > 0.4f) c = C(70, 108, 52);   // moss in the joints
      }
      c = urbWear(c, std::max(up, 1), px, py, joint);
      // the closes' lawn beds, each kerbed round in stone
      if (u == Urb::Close && closeBed(m, tx, ty)) {
        const bool bN = closeBed(m, tx, ty - 1), bS = closeBed(m, tx, ty + 1), bW = closeBed(m, tx - 1, ty), bE = closeBed(m, tx + 1, ty);
        const int dN = bN ? 99 : ly, dS = bS ? 99 : 15 - ly, dW = bW ? 99 : lx, dE = bE ? 99 : 15 - lx;
        // (fixer M7b r1, review: "well-square greenery is flat turf rectangles") a raised bed in 3/4 view: a dressed
        // stone kerb round it, its top lit, its south face in shade with a dark foot on the flags, the lawn inside a
        // touch lower (the kerb's shadow along its north and west), tufts and a few flowers
        const uint32_t kr = mul(P.kerb, 0.96f + ((hash2(fdiv_(px, 9), fdiv_(py, 9), 2823) >> 8) & 255) / 255.0f * 0.08f);
        if (dS <= 2) c = dS == 0 ? mul(kr, 0.56f) : (dS == 1 ? mul(kr, 0.76f) : mul(kr, 1.04f));   // the face, then the top
        else if (dN <= 1) c = dN == 0 ? mul(kr, 0.84f) : mul(kr, 1.14f);
        else if (dW <= 1) c = dW == 0 ? mul(kr, 0.84f) : mul(kr, 1.10f);
        else if (dE <= 1) c = dE == 0 ? mul(kr, 0.70f) : mul(kr, 0.90f);
        else {
          c = lawnPx(px, py, false, m.biomeAt(tx, ty));
          if (dN == 2 || dW == 2) c = mul(c, 0.74f);        // the kerb's shadow on the lawn
          else if (dN == 3 || dW == 3) c = mul(c, 0.90f);
          const float fh = hashf(px >> 1, py >> 1, 2825);
          if (fh < 0.012f) c = (hash2(px, py, 2827) & 1) ? C(236, 210, 90) : C(226, 120, 150);   // a flower
        }
        if ((dE <= 1 || dS <= 2) && (fmod_(dE <= 1 ? py : px, 9) == 0) && !(dE > 1 && dS > 2)) c = mul(c, 0.82f);   // the joints
        break;
      }
      // the border course of long kerb stones round the paved floor
      int sd = 0;
      // ((fixer M7b r1) paving the plan left unclassed - the town's own plaza flags round a square - is the same floor)
      const int e = edgeDist(m, tx, ty, lx, ly, [&](int x, int y) {
        const Urb q = ucls(m, x, y);
        if (q == Urb::None) { const Ground gq = m.at(x, y); return gq == Ground::Plaza || gq == Ground::StoneFloor || gq == Ground::Road; }
        return urbPaved(m, x, y) || ew::urbCarriage(q) || urbRaised(m, x, y) || q == Urb::Gutter; }, sd);
      const int along = sd <= 1 ? px : py;
      // (fixer M7b r1) the border course runs only where the paved floor meets something that is not street or paving
      // (a house, the open ground): a square meeting a court or a close is one floor, no stroke between them
      if (e < 4) {
        const int dd = e;
        const uint32_t st = mul(P.kerb, 0.93f + ((hash2(fdiv_(along, 14), sd + ty * 3 + tx * 7, 2821) >> 8) & 255) / 255.0f * 0.10f);
        const bool jt = fmod_(along, 14) == 0;
        if (dd == 0 || jt) c = mul(st, 0.70f);
        else if (dd == 1) c = mul(st, (sd == 1 || sd == 2) ? 1.10f : 0.92f);
        else c = st;
      }
      // (fixer M7b r1, review: "squares covered in stray light kerb-band outline fragments") no inner band two tiles in:
      // it followed every step of a square's irregular edge and broke into L and staircase strokes across the flags
      // (fixer M7b r2, review: "where an avenue opens into a square the road is cut off square, the kerbs just stop") the
      // square stands at the walks' height: across the mouth of a carriageway its edge is the kerb's return (the face
      // where the road lies south of it, the arris to the north, west and east), so the walks' kerbs turn the corner
      // and run on across the mouth as one line
      {
        auto lowQ = [&](int x, int y) { const Urb q = ucls(m, x, y); return ew::urbCarriage(q) || q == Urb::Gutter; };
        int side = -1, d = 99;
        auto cand = [&](int sd, int dd, int lim) { if (dd < lim && dd < d) { d = dd; side = sd; } };
        if (lowQ(tx, ty + 1)) cand(0, 15 - ly, 6);
        if (lowQ(tx + 1, ty)) cand(3, 15 - lx, 4);
        if (lowQ(tx - 1, ty)) cand(2, lx, 4);
        if (lowQ(tx, ty - 1)) cand(1, ly, 4);
        // (only where the carriageway is a kerbed one: a walk beside it near by; a town's flush street meets its square
        // level)
        bool kerbed = false;
        const bool nearLow = lowQ(tx, ty + 1) || lowQ(tx + 1, ty) || lowQ(tx - 1, ty) || lowQ(tx, ty - 1);
        for (int oy = -2; oy <= 2 && nearLow && !kerbed; oy++)
          for (int ox = -2; ox <= 2; ox++) if (urbRaised(m, tx + ox, ty + oy)) { kerbed = true; break; }
        if (side >= 0 && kerbed) c = pm == 4 ? deckStepPx(side, d, c) : kerbPx(P, side, d, side <= 1 ? px : py, up, c);
        // (fixer M7b r3, review: "avenues stop at squares with a hard straight cut and no edge treatment") behind the
        // kerb's return a threshold course: a band of setts laid across the mouth, edged by a dark joint toward the
        // square's flags, so the avenue enters its square through an edging as a commercial tileset would lay it
        if (kerbed && side < 0) {
          int ts = -1, td = 99;
          auto tc = [&](int sd, int dd, int lim) { if (dd >= lim && dd < lim + 6 && dd < td) { td = dd - lim; ts = sd; } };
          if (lowQ(tx, ty + 1)) tc(0, 15 - ly, 6);
          if (lowQ(tx + 1, ty)) tc(3, 15 - lx, 4);
          if (lowQ(tx - 1, ty)) tc(2, lx, 4);
          if (lowQ(tx, ty - 1)) tc(1, ly, 4);
          if (ts >= 0) {
            const int al = ts <= 1 ? px : py;
            const uint32_t st = mul(lerpc(P.kerb, P.sett, 0.35f), 0.92f + ((hash2(fdiv_(al, 5), ts + tx * 5 + ty * 3, 2861) >> 8) & 255) / 255.0f * 0.12f);
            const bool jt = fmod_(al, 5) == 0;
            if (td == 5) c = mul(st, 0.62f);                          // the joint along the flags
            else if (jt) c = mul(st, 0.70f);                          // the joints between the setts
            else if (td == 0) c = mul(st, 0.84f);                     // in the kerb's lee
            else if (td == 1 && (ts == 1 || ts == 2)) c = mul(st, 1.06f);
            else c = st;
            c = urbWear(c, std::max(up, 1), px, py, jt);
          }
        }
      }
      break;
    }
    case Urb::Park: case Urb::ParkPath: case Urb::Garden: {
      const Biome b = m.biomeAt(tx, ty);
      if (u == Urb::Garden) {
        // kitchen beds in blocks of 3x3 tiles (one block in three an orchard's grass), edged in planks
        const int bx = fdiv_(tx, 3), by = fdiv_(ty, 3);
        const bool beds = hash2(bx, by, 2831) % 3u != 0;
        if (beds) {
          const bool eW = ucls(m, tx - 1, ty) != Urb::Garden || fmod_(tx, 3) == 0, eE = ucls(m, tx + 1, ty) != Urb::Garden || fmod_(tx, 3) == 2;
          const bool eN = ucls(m, tx, ty - 1) != Urb::Garden || fmod_(ty, 3) == 0, eS = ucls(m, tx, ty + 1) != Urb::Garden || fmod_(ty, 3) == 2;
          const int dd = std::min(std::min(eW ? lx : 99, eE ? 15 - lx : 99), std::min(eN ? ly : 99, eS ? 15 - ly : 99));
          if (dd <= 1) { c = dd == 0 ? C(96, 72, 46) : ((eN && ly == 1) || (eW && lx == 1) ? C(156, 120, 78) : C(118, 88, 56)); break; }
          if (dd == 2) { c = gravelPx(px, py); break; }
          const int row = fmod_(py, 5);
          const uint32_t hh = hash2(px >> 1, py / 5, 2833 + (uint32_t)(bx * 7 + by));
          if (row <= 1) c = row == 0 ? C(82, 58, 40) : C(98, 70, 46);                       // the tilled furrow
          else {
            c = (hh % 3u) == 0 ? C(74, 124, 52) : ((hh % 3u) == 1 ? C(92, 146, 60) : C(108, 72, 48));   // the crop
            if (row == 2 && (hh & 4u)) c = C(126, 174, 74);                                  // a lit leaf
            if ((hh % 23u) == 0) c = (bx + by) & 1 ? C(214, 120, 60) : C(196, 70, 64);       // a ripe squash / a beet top
          }
          if (b == Biome::Snow) c = lerpc(c, C(226, 232, 242), 0.55f);
          break;
        }
        c = lawnPx(px, py, false, b);   // (an orchard's grass: the park's lawn, its trees and the walls make the difference)
        break;
      }
      // (fixer M7b r1) the bank of a pond beside the lawn
      if (u == Urb::Park) {
        bool wetNear = false;
        for (int oy = -1; oy <= 1 && !wetNear; oy++)
          for (int ox = -1; ox <= 1; ox++) if (groundWater(m.at(tx + ox, ty + oy)) || m.at(tx + ox, ty + oy) == Ground::Ice) { wetNear = true; break; }
        if (wetNear) {
          const float pv = pondField(m, px, py);
          if (pv > 0.38f) { c = pondBankPx(px, py, pv, b); break; }
          if (pv > 0.32f) { c = mul(lawnPx(px, py, true, b), 0.86f); break; }   // the lawn's damp lip
        }
      }
      // a park: lawn, and gravel paths whose edges follow a smooth field over the tile centres (no square steps)
      auto pathT = [&](int x, int y) -> float {
        const Urb q = ucls(m, x, y);
        return q == Urb::ParkPath ? 1.0f : 0.0f;
      };
      const float fx = (px - 7.5f) / 16.0f, fy = (py - 7.5f) / 16.0f;
      const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
      const float ax = fx - ix, ay = fy - iy;
      const float w00 = pathT(ix, iy), w10 = pathT(ix + 1, iy), w01 = pathT(ix, iy + 1), w11 = pathT(ix + 1, iy + 1);
      float v = (w00 * (1 - ax) + w10 * ax) * (1 - ay) + (w01 * (1 - ax) + w11 * ax) * ay;
      if (w00 == w11 && w10 == w01 && w00 != w10) v = std::max(v, 1.0f - (w00 > 0.5f ? std::fabs(ax - ay) : std::fabs(ax + ay - 1.0f)) * 0.9f);
      v += (vnoise(px / 9.0f, py / 9.0f, 2841) - 0.5f) * 0.12f;   // (fixer M7b r2: an even width, a soft edge)
      if (v > 0.5f) {
        c = gravelPx(px, py);
        if (v < 0.56f) c = mul(c, 0.84f);                     // the grass edge's shade on the gravel
      } else {
        c = lawnPx(px, py, false, b);
        if (v > 0.44f) c = mul(c, (tuft(px, py, 2843, 0.9f) ? 1.12f : 0.90f));   // the lawn's ragged lip
      }
      break;
    }
    case Urb::Quay: {
      c = flagPx(px, py, mul(P.kerb, 0.92f), 2851, 18, joint);
      c = urbWear(c, std::max(up, 1), px, py, joint);
      const bool wS = water(tx, ty + 1), wN = water(tx, ty - 1), wW = water(tx - 1, ty), wE = water(tx + 1, ty);
      int d = 99, side = -1;
      if (wS && 15 - ly < 4) { d = 15 - ly; side = 0; }
      if (wN && ly < d && ly < 4) { d = ly; side = 1; }
      if (wW && lx < d && lx < 4) { d = lx; side = 2; }
      if (wE && 15 - lx < d && 15 - lx < 4) { d = 15 - lx; side = 3; }
      if (side >= 0) {   // the coping: long dressed stones lit along the water's edge
        const int along = side <= 1 ? px : py;
        const uint32_t st = mul(P.kerb, 0.96f + ((hash2(fdiv_(along, 16), side, 2853) >> 8) & 255) / 255.0f * 0.08f);
        const bool jt = fmod_(along, 16) == 0;
        if (d == 0) c = side == 0 ? mul(st, 0.86f) : mul(st, 0.60f);
        else if (d == 1) c = jt ? mul(st, 0.74f) : mul(st, side == 3 ? 0.86f : 1.14f);
        else if (d == 2) c = jt ? mul(st, 0.74f) : st;
        else c = mul(c, 0.78f);
      }
      // mooring posts on the coping, one quay tile in three along the water
      if (wS && hash2(tx, ty, 2855) % 3u == 0 && lx >= 6 && lx <= 9 && ly >= 6 && ly <= 11) {
        const int qx = lx - 6, qy = ly - 6;
        if (qy == 0) c = qx == 0 ? C(120, 116, 112) : C(92, 88, 86);                     // the post's cap, lit on the left
        else if (qy <= 4) c = qx == 0 ? C(84, 76, 70) : (qx == 3 ? C(36, 32, 32) : C(60, 54, 50));
        else c = mul(c, 0.6f);                                                             // its foot's shade
        if (qy == 2 && qx >= 1 && qx <= 2) c = C(150, 128, 90);                            // a turn of rope
      } else if (wS && hash2(tx, ty, 2855) % 3u == 0 && lx >= 10 && lx <= 11 && ly >= 7 && ly <= 11) c = mul(c, 0.72f);   // its shadow
      break;
    }
    default: return 0;
  }
  // (M7c ART, "crossings where a block street meets an avenue") where a line of walk runs on across a carriageway (the
  // walks of a block street either side of the avenue it crosses), a crossing course of big pale slabs is laid across
  // the road, level with it, joints across the way of the walk, a lit leading edge and a dark joint to the setts on
  // either side; the drain passes under it. Avenues and streets only (a lane is crossed anywhere).
  if (!handled && (u == Urb::Avenue || u == Urb::Street || u == Urb::Gutter) && (px >> 4) == tx && (py >> 4) == ty) {
    auto carrT = [&](int x, int y) { const Urb q = ucls(m, x, y); return ew::urbCarriage(q) || q == Urb::Gutter; };
    auto walkT = [&](int x, int y) { const Ground q = m.at(x, y); return ucls(m, x, y) == Urb::Sidewalk && !groundWater(q) && q != Ground::Bridge; };
    auto across = [&](int dx, int dy) {   // the walk line runs along (dx, dy) across this carriageway
      int a = 1, b = 1;
      // (M7c fixer, review: "an orphan pavement slab floating in the carriageway" at a diagonal junction's mouth) every
      // tile of a crossing sees the same run (up to five tiles each way, five across in all), so a crossing is laid
      // from kerb to kerb or not at all: with four each way the run's end tile saw too far and dropped out
      while (a <= 5 && carrT(tx - dx * a, ty - dy * a)) a++;
      while (b <= 5 && carrT(tx + dx * b, ty + dy * b)) b++;
      if (a > 5 || b > 5 || a + b - 1 > 5) return false;
      // and only over a straight stretch of carriageway: the road runs on either side of the crossing's whole width
      // (at a junction's mouth or a slanting kerb the walks of a crossing street meet no straight band to lay it on)
      for (int k = -(a - 1); k <= b - 1; k++) {
        const int qx = tx + dx * k, qy = ty + dy * k;
        if (!carrT(qx + dy, qy + dx) || !carrT(qx - dy, qy - dx)) return false;
      }
      return walkT(tx - dx * a, ty - dy * a) && walkT(tx - dx * (a + 1), ty - dy * (a + 1)) && walkT(tx + dx * b, ty + dy * b) &&
             walkT(tx + dx * (b + 1), ty + dy * (b + 1));
    };
    const bool cxing = across(1, 0), cying = !cxing && across(0, 1);
    if (cxing || cying) {
      const Urb cu = ew::urbCarriage(u) ? u : nearCarriage(m, tx, ty);
      if (cu == Urb::Avenue || cu == Urb::Street) {
        const int w = cxing ? ly : lx;          // across the band (0..15)
        const int al = cxing ? px : py;         // along the way of the walk
        if (w >= 2 && w <= 13) {
          const int sj = fmod_(al, 8);
          const uint32_t st = mul(lerpc(P.flag, P.sett, 0.30f), 0.94f + ((hash2(fdiv_(al, 8), (cxing ? ty : tx), 2871) >> 8) & 255) / 255.0f * 0.10f);
          bool jt = sj == 0 || w == 7 + (fdiv_(al, 8) & 1);   // the slabs: two courses, staggered
          uint32_t k = jt ? mul(st, 0.72f) : st;
          if (!jt && (sj == 1 || w == 2 || w == 8 + (fdiv_(al, 8) & 1))) k = mul(k, 1.07f);   // lit upper / left edges
          else if (!jt && (sj == 7 || w == 13)) k = mul(k, 0.90f);
          c = urbWear(k, std::max(1, up), px, py, jt);
        } else if (w == 1 || w == 14) c = mul(c, 0.70f);   // the joint to the setts
      }
    }
  }
  // a paved tile that meets open water (not a quay): a coping band along it (the face shows on the water below)
  if (u != Urb::Quay && !ew::urbGreen(u) && c) {
    if (water(tx, ty + 1) && ly >= 14) c = ly == 15 ? mul(P.kerb, 0.86f) : mul(P.kerb, 1.10f);
    else if (water(tx - 1, ty) && lx <= 1) c = lx == 0 ? mul(P.kerb, 0.62f) : mul(P.kerb, 1.12f);
    else if (water(tx + 1, ty) && lx >= 14) c = lx == 15 ? mul(P.kerb, 0.50f) : mul(P.kerb, 0.86f);
    else if (water(tx, ty - 1) && ly <= 1) c = ly == 0 ? mul(P.kerb, 0.62f) : mul(P.kerb, 1.12f);
  }
  return c;
}
// the classes whose edge with the open ground (and with each other) wobbles like the land's, instead of following the
// tile grid: lanes, alleys, streets, squares, courts, parks and gardens (a kerbed walk, a close's beds, a quay and a
// gutter keep their built, straight edges)
inline bool urbSoftEdge(Urb u) {
  return u == Urb::Lane || u == Urb::Alley || u == Urb::Avenue || u == Urb::Street || u == Urb::Square || u == Urb::Court || urbGreen(u);
}
// the warped tile a pixel takes its look from at a soft edge (the same warp on both sides of the line)
inline void urbWarp(int px, int py, int& wtx, int& wty) {
  const float wx = px + (vnoise(px / 7.0f, py / 7.0f, 2901) - 0.5f) * 10.0f + (vnoise(px / 3.0f, py / 3.0f, 2903) - 0.5f) * 3.0f;
  const float wy = py + (vnoise(px / 7.0f, py / 7.0f, 2905) - 0.5f) * 10.0f + (vnoise(px / 3.0f, py / 3.0f, 2907) - 0.5f) * 3.0f;
  wtx = (int)std::floor(wx / 16.0f); wty = (int)std::floor(wy / 16.0f);
}
// ((M7c fixer r3, review: "paving-to-dirt transitions in planned cities have torn, sawtooth edges") the fine 3-px
// octave is gone and the swing halved: a planned city's lane meets the open ground along a gently wavering laid edge,
// not a torn, dithered one)
inline void urbWarpF(int px, int py, float& wx, float& wy) {
  wx = px + (vnoise(px / 9.0f, py / 9.0f, 2901) - 0.5f) * 6.0f;
  wy = py + (vnoise(px / 9.0f, py / 9.0f, 2905) - 0.5f) * 6.0f;
}
// the ground under the fabric is painted by it (a surface: not water, a bridge, rock or a wall)
inline bool urbSurface(Ground g) {
  switch (g) {
    case Ground::Road: case Ground::Plaza: case Ground::Dirt: case Ground::StoneFloor: case Ground::Grass: case Ground::Meadow:
    case Ground::ForestFloor: case Ground::Autumn: case Ground::Tundra: case Ground::Snow: case Ground::Sand: case Ground::Farmland:
      return true;
    default: return false;
  }
}
// a tile painted by the fabric
template <class TM>
inline bool urbPainted(const TM& m, int x, int y) { return ucls(m, x, y) != Urb::None && urbSurface(m.at(x, y)); }

// (M7c fixer r3, review: "marsh city boardwalks look like a patchwork of tiles, not continuous decks") a people that
// decks its ways in planks (paving 4) lays ONE deck: its streets, squares, bridges and the city's fabric (walks,
// carriageways, lanes, squares) are one plank floor with one rim beam round its outer edge, never a beam, a face and a
// shadow between two of its own pieces. The deck's edge runs on the slant where its tiles staircase (stairEdge), so a
// diagonal lane is one straight deck, not a zig-zag of tile steps.
template <class TM>
inline bool plankDeckT(const TM& m, int x, int y) {
  const Ground q = m.at(x, y);
  if (q == Ground::Bridge) return true;   // (any bridge: a deck runs on over it; it keeps its own rails)
  const Urb u = ucls(m, x, y);
  if (u != Urb::None && !ew::urbGreen(u) && urbSurface(q)) return urbMat(m, x, y) == 4;
  if (q != Ground::Road && q != Ground::Plaza) return false;
  return paveMatAt(m, x, y) == 4;
}
// a pixel near a plank deck's edge: is it on the deck, how far from the edge (px) and the edge's outward normal
struct DeckEdge { bool in = false; float d = 99.0f, ox = 0.0f, oy = 0.0f; };
template <class TM>
DeckEdge deckEdgeAt(const TM& m, int px, int py) {
  DeckEdge r;
  const int tx = px >> 4, ty = py >> 4, lx = px & 15, ly = py & 15;
  auto mem = [&](int x, int y) { return plankDeckT(m, x, y); };
  const bool here = mem(tx, ty);
  r.in = here;
  thread_local StairCache deckSC;
  float sd = 0.0f, nX = 0.0f, nY = 0.0f;
  if (stairEdge(m, mem, true, tx, ty, px + 0.5f, py + 0.5f, deckSC, sd, nX, nY)) {
    r.in = sd > 0.0f;
    r.d = std::fabs(sd);
    r.ox = -nX; r.oy = -nY;   // (the normal points into the deck)
    return r;
  }
  auto cand = [&](bool edge, int d, float ox, float oy) { if (edge && (float)d < r.d) { r.d = (float)d; r.ox = ox; r.oy = oy; } };
  if (here) {
    cand(!mem(tx, ty - 1), ly, 0.0f, -1.0f);
    cand(!mem(tx, ty + 1), 15 - ly, 0.0f, 1.0f);
    cand(!mem(tx - 1, ty), lx, -1.0f, 0.0f);
    cand(!mem(tx + 1, ty), 15 - lx, 1.0f, 0.0f);
  } else {
    // (only the decks north and west of it matter off the deck: its front face and its shadow fall south and east)
    cand(mem(tx, ty - 1), ly, 0.0f, 1.0f);
    cand(mem(tx - 1, ty), lx, 1.0f, 0.0f);
    if (r.d > 90.0f && mem(tx - 1, ty - 1)) cand(true, std::max(lx, ly), 0.7071f, 0.7071f);
  }
  return r;
}
// the deck's rim beam (on it) or its front face and cast shadow (off it); c unchanged elsewhere
inline uint32_t deckRimPx(const DeckEdge& e, int px, int py, uint32_t c) {
  const int di = (int)std::floor(e.d);
  if (e.in) {
    if (di > 3) return c;
    const bool lit = e.oy < -0.5f || e.ox < -0.5f;   // the rim facing north or west catches the light
    const bool alongX = std::fabs(e.oy) >= std::fabs(e.ox);
    const int run = alongX ? px : py;
    const int rj = ((run % 19) + 19) % 19;
    if (di == 0) return lit ? C(150, 122, 88) : C(70, 52, 40);
    if (di == 3) return C(58, 44, 34);                                              // the joint inside the beam
    uint32_t k = rj == 0 ? C(66, 50, 38) : (lit ? (di == 1 ? C(170, 140, 100) : C(146, 118, 84)) : (di == 1 ? C(116, 92, 66) : C(104, 82, 60)));
    if (rj == 9 && (((alongX ? py : px) & 1) != 0)) k = C(74, 70, 66);              // a nail head
    return k;
  }
  if (e.oy > 0.38f && e.d < 3.0f / std::max(0.38f, e.oy)) {   // below the south edge: the deck's front face (plank ends)
    const int fd = (int)std::floor(e.d * e.oy);
    return fd >= 2 ? C(48, 36, 30) : ((((px + py) % 5 + 5) % 5) == 0 ? C(60, 46, 36) : C(92, 70, 52));
  }
  if ((e.oy > 0.38f && e.d < 7.0f) || (e.ox > 0.38f && e.d < 4.0f)) return lerpc(mul(c, 0.70f), C(48, 34, 92, (int)(c >> 24)), 0.14f);
  return c;
}
// a pixel is in reach of a plank deck's edge (cheap test before deckEdgeAt): a plank town's tile or one beside a deck
template <class TM>
inline bool plankZone(const TM& m, int tx, int ty) {
  const bool here = plankDeckT(m, tx, ty);
  for (int oy = -1; oy <= 1; oy++)
    for (int ox = -1; ox <= 1; ox++)
      if ((ox || oy) && plankDeckT(m, tx + ox, ty + oy) != here) {
        // (a bridge alone is no plank town: some deck of the planking people must be near)
        for (int qy = -1; qy <= 1; qy++)
          for (int qx = -1; qx <= 1; qx++)
            if (plankDeckT(m, tx + qx, ty + qy) && m.at(tx + qx, ty + qy) != Ground::Bridge) return true;
        return false;
      }
  return false;
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
      // (fixer M6b r1) what most of the ground round it is (marsh or water, by the 8 neighbours and the next ring), not
      // the first neighbour found: a corner in a reed bed showed as a square of open water stepping along the walk
      int nSwamp = 0, nWater = 0;
      Ground wq = Ground::Water;
      for (int oy = -2; oy <= 2; oy++)
        for (int ox = -2; ox <= 2; ox++) {
          if (!ox && !oy) continue;
          const Ground q = m.at(tx + ox, ty + oy);
          const int w = (std::abs(ox) <= 1 && std::abs(oy) <= 1) ? 2 : 1;
          if (q == Ground::Swamp) nSwamp += w;
          else if (groundWater(q)) { nWater += w; wq = q; }
        }
      real = nSwamp >= nWater && nSwamp > 0 ? Ground::Swamp : wq;
    }
  }
  if (m.kind == MapKind::Overworld && (real == Ground::Bridge || groundWater(real) || real == Ground::Road)) {
    uint32_t o;
    if (diagBridgePixel(m, px, py, real, o)) return o;
  }
  // (M7b CITYART) a city's urban fabric paints its own tiles (Map::urb, 0 in every village: their ground is untouched);
  // the ground beside it keeps its own tile's look (no warp or shore field reaching into the fabric), so the kerbs,
  // copings and border courses meet it along clean edges
  bool nearUrb = false;
  Ground urbYield = Ground::Void;   // (M7b) a fabric pixel the land's ground takes over at a soft edge
  // (M7c fixer r3) a plank town's one deck (plankDeckT): near its edge the pixel's side of the (slanted) edge line, the
  // rim beam on it and the front face and shadow off it (deckRimPx); the fabric's soft edges stay off its planks
  DeckEdge deckE;
  bool deckZ = false;
  Ground deckYield = Ground::Void;   // a deck tile's pixel the slanted edge gives to the ground beside it
  if (m.kind == MapKind::Overworld && real != Ground::Bridge && plankZone(m, tx, ty)) {
    deckZ = true;
    deckE = deckEdgeAt(m, px, py);
    const bool hereDeck = plankDeckT(m, tx, ty);
    // the tile a step along (sx, sy) of the larger component of the normal (or the other axis)
    auto stepT = [&](float vx, float vy, bool wantDeck, int& ox, int& oy) {
      const int ax = vx > 0.2f ? 1 : (vx < -0.2f ? -1 : 0), ay = vy > 0.2f ? 1 : (vy < -0.2f ? -1 : 0);
      const int c1x = std::fabs(vx) >= std::fabs(vy) ? ax : 0, c1y = std::fabs(vx) >= std::fabs(vy) ? 0 : ay;
      const int c2x = c1x ? 0 : ax, c2y = c1x ? ay : 0;
      if ((c1x || c1y) && plankDeckT(m, tx + c1x, ty + c1y) == wantDeck) { ox = tx + c1x; oy = ty + c1y; return true; }
      if ((c2x || c2y) && plankDeckT(m, tx + c2x, ty + c2y) == wantDeck) { ox = tx + c2x; oy = ty + c2y; return true; }
      if (ax && ay && plankDeckT(m, tx + ax, ty + ay) == wantDeck) { ox = tx + ax; oy = ty + ay; return true; }
      return false;
    };
    if (deckE.in && !hereDeck) {
      // a step of the staircase the deck's slanted edge takes in: the planks of the deck tile beside it
      int dx = tx, dy = ty;
      uint32_t c = 0;
      if (stepT(-deckE.ox, -deckE.oy, true, dx, dy) && urbPainted(m, dx, dy)) c = urbPixel(m, px, py, m.urbAt(dx, dy), dx, dy);
      if (!c) c = pavePixel(4, true, px, py, 0.0f, -1.0f, hashf(px, py, 2791), 0.5f);
      c = deckRimPx(deckE, px, py, c);
      if (m.relief()) c = reliefPixel(m, px, py, c, Ground::Road, true);
      return c;
    }
    if (!deckE.in && hereDeck) {
      int gx = tx, gy = ty;
      if (stepT(deckE.ox, deckE.oy, false, gx, gy)) {
        const Ground q = m.at(gx, gy);
        if (q != Ground::Void && q != Ground::Bridge && !isWall(q)) deckYield = q;
      }
      if (deckYield == Ground::Void) deckE.in = true;   // (nothing to yield to: the deck keeps the pixel)
    }
  }
  if (m.kind == MapKind::Overworld && !m.m->urb.empty()) {
    const uint8_t ub = m.urbAt(tx, ty);
    // the soft edges: a pixel near the line takes its look from the tile the warp points at (the land's ground over a
    // lane's edge, a square's flags into the grass beside it), the same warp seen from both sides
    int otx = tx, oty = ty;
    uint8_t ob = ub;
    const bool mine = ew::urbClass(ub) != ew::Urb::None && urbSurface(real);
    // (cheap test first: the warp reaches 7 px, so only a pixel near its tile's edge whose neighbours that way differ)
    const int elx = px & 15, ely = py & 15, eqx = elx < 8 ? -1 : 1, eqy = ely < 8 ? -1 : 1;
    const bool nearEdge = (elx < 7 || elx > 8 || ely < 7 || ely > 8) &&
                          (m.urbAt(tx + eqx, ty) != ub || m.urbAt(tx, ty + eqy) != ub || m.urbAt(tx + eqx, ty + eqy) != ub);
    // (fixer M7b r2) the fabric as a set (for the marching-squares cut of its staircases against the open ground)
    auto fabT = [&](int x, int y) { return urbPainted(m, x, y); };
    bool walkCut = false, greenCut = false;
    float cutD = 99.0f;   // (M7c) px from the slanted line
    const bool greenHere = mine && ew::urbGreen(ew::urbClass(ub));
    // ((M7c fixer, review: "steppe city: paving meets dirt in a square notched edge") the plan-less beaten earth round a
    // walk (Ground::Road with no fabric class: the steppe's, a town's yards) is open ground for the cut too)
    auto openG = [&](Ground q) { return (soft(q) && !groundWater(q)) || q == Ground::Road; };
    if (!deckZ && nearEdge && ((mine && ew::urbClass(ub) == ew::Urb::Sidewalk) || greenHere || (!ub && openG(real)))) {
      // (fixer M7b r2, review: "the grass beside a slanting walk breaks into hard square tile blocks") the back edge of a
      // walk on the slant: the steps of its staircase cut on the line (the walk's flags into the notch, the ground over
      // the step), no wobble (a built edge). (M7c ART) on any slope (stairEdge over the hard fabric: walks, streets,
      // squares, courts, quays), and against a park's lawn or a garden as against the land's own ground
      auto hardT = [&](int x, int y) { return urbPainted(m, x, y) && !ew::urbGreen(ew::urbClass(m.urbAt(x, y))); };
      thread_local StairCache edgeSC;
      float sd = 0.0f, nX = 0.0f, nY = 0.0f;
      if (stairEdge(m, hardT, true, tx, ty, px + 0.5f, py + 0.5f, edgeSC, sd, nX, nY)) {
        const bool hereHard = hardT(tx, ty);
        if (hereHard != (sd > 0.0f)) {
          // the pixel belongs across the line: the tile beside it that way (the normal points into the hard set)
          const float k = std::fabs(sd) + 1.5f, s = sd > 0.0f ? 1.0f : -1.0f;
          int ctx = (int)std::floor((px + 0.5f + nX * k * s) / 16.0f), cty = (int)std::floor((py + 0.5f + nY * k * s) / 16.0f);
          if (ctx == tx && cty == ty) {   // (the corner of the tile: the 4-neighbour on the far side)
            if (std::fabs(nX) >= std::fabs(nY)) ctx += (nX * s > 0) ? 1 : -1; else cty += (nY * s > 0) ? 1 : -1;
          }
          const uint8_t wb = m.urbAt(ctx, cty);
          const ew::Urb wu = ew::urbClass(wb);
          if (hereHard && ew::urbClass(ub) == ew::Urb::Sidewalk) {
            if (!wb && openG(m.at(ctx, cty))) urbYield = m.at(ctx, cty);
            else if (ew::urbGreen(wu) && urbSurface(m.at(ctx, cty))) { greenCut = true; otx = ctx; oty = cty; ob = wb; cutD = std::fabs(sd); }
          } else if (!hereHard && wu == ew::Urb::Sidewalk && urbSurface(m.at(ctx, cty))) {
            walkCut = true; otx = ctx; oty = cty; ob = wb;
          }
        }
      }
    }
    if (deckYield != Ground::Void && mine) urbYield = deckYield;   // (M7c fixer r3) the plank deck's slanted edge
    if (!deckZ && !walkCut && !greenCut && urbYield == Ground::Void && nearEdge && ((mine && urbSoftEdge(ew::urbClass(ub))) || (!ub && openG(real)))) {
      int wtx, wty;
      float wfx, wfy;
      urbWarpF(px, py, wfx, wfy);
      wtx = (int)std::floor(wfx / 16.0f); wty = (int)std::floor(wfy / 16.0f);
      // (fixer M7b r2, review: "a diagonal cobble lane is a staircase of whole 16-px tiles on bare dirt") the warped
      // point's tile with the staircase's steps cut on the slant, so a slanting lane or square edge wobbles along a
      // straight line instead of a tile staircase
      { int ctx = wtx, cty = wty; if (msCut(fabT, wfx, wfy, false, true, ctx, cty)) { wtx = ctx; wty = cty; } }
      if (wtx != tx || wty != ty) {
        const uint8_t wb = m.urbAt(wtx, wty);
        const ew::Urb wu = ew::urbClass(wb);
        // (fixer M7b r2, review: "the cobble edge against the square's flags is ragged and dithered") two kinds of
        // paving meet along their built edge; only two greens blend
        const bool fabFab = ub != 0 && !(ew::urbGreen(wu) && ew::urbGreen(ew::urbClass(ub)));
        if (wu != ew::Urb::None && urbSurface(m.at(wtx, wty)) && urbSoftEdge(wu) && !fabFab) { otx = wtx; oty = wty; ob = wb; }
        else if (mine && wu == ew::Urb::None && openG(m.at(wtx, wty))) urbYield = m.at(wtx, wty);
      }
    }
    if (walkCut) {
      const UrbPal P = urbPal(urbMat(m, otx, oty));
      bool jt = false;
      const int up = ew::urbUpkeep(ob), pm = urbMat(m, otx, oty);
      uint32_t c = (pm == 4 || pm == 10) ? pavePixel(4, true, px, py, 0.0f, -1.0f, hashf(px, py, 2791), 0.5f) : flagPx(px, py, P.flag, 2793, up >= 3 ? 14 : 11, jt);
      c = urbWear(c, up, px, py, jt);
      if (m.relief()) c = reliefPixel(m, px, py, c, real, true);
      return c;
    }
    if (ew::urbClass(ub) == ew::Urb::Park && (groundWater(real) || real == Ground::Ice)) {
      // (fixer M7b r1) a park's pond: its rounded, meandering edge carved from its own tiles (the bank, the lawn)
      const float pv = pondField(m, px, py);
      if (pv < 0.62f) {
        uint32_t c = pv > 0.38f ? pondBankPx(px, py, pv, m.biomeAt(tx, ty)) : lawnPx(px, py, false, m.biomeAt(tx, ty));
        if (pv > 0.32f && pv <= 0.38f) c = mul(lawnPx(px, py, true, m.biomeAt(tx, ty)), 0.86f);
        if (m.relief()) c = reliefPixel(m, px, py, c, Ground::Meadow, true);
        return c;
      }
    }
    if ((mine || ob != ub) && urbYield == Ground::Void) {
      if (uint32_t c = urbPixel(m, px, py, ob, otx, oty)) {
        if (deckZ && deckE.in && plankDeckT(m, otx, oty)) c = deckRimPx(deckE, px, py, c);   // (M7c fixer r3) the deck's rim
        // (M7c fixer r3) a paved lane's or square's laid edge against the open ground: its last stones sit a touch
        // lower in the earth (a soft contact shade along the wavering line), so the edge reads as laid, not torn
        else if (nearEdge && !deckZ && ob == ub && urbSoftEdge(ew::urbClass(ub)) && !ew::urbGreen(ew::urbClass(ub))) {
          bool openBy = false;
          static const int EX[4] = {2, -2, 0, 0}, EY[4] = {0, 0, 2, -2};
          for (int k = 0; k < 4 && !openBy; k++) {
            float wfx, wfy;
            urbWarpF(px + EX[k], py + EY[k], wfx, wfy);
            const int qx = (int)std::floor(wfx / 16.0f), qy = (int)std::floor(wfy / 16.0f);
            if ((qx != tx || qy != ty) && !m.urbAt(qx, qy) && openG(m.at(qx, qy)) && m.at(qx, qy) != Ground::Road) openBy = true;
          }
          if (openBy) c = mul(c, 0.84f);
        }
        // (M7c) a kitchen garden cut on the slant by a walk: its bed's plank edging runs along the line too
        if (greenCut && ew::urbClass(ob) == ew::Urb::Garden && cutD < 2.0f) c = cutD < 1.0f ? C(96, 72, 46) : C(140, 106, 68);
        auto nsRun = [&](int wx, int wy) {   // a north-south run of city wall casts its shade east (as below)
          return m.wallAt(wx, wy) && (m.wallAt(wx, wy - 1) || m.wallAt(wx, wy + 1)) && !m.wallAt(wx - 1, wy) && !m.wallAt(wx + 1, wy);
        };
        int dw = 0;
        for (int k = 1; k <= 10 && !dw; k++) if (nsRun((px - k) >> 4, ty) && m.wallAt((px - k) >> 4, (py - 6) >> 4)) dw = k;
        if (dw) c = lerpc(mul(c, 0.56f + dw * 0.025f), C(48, 34, 92, (int)(c >> 24)), 0.18f);
        if (m.relief()) {
          // (fixer M7b r1, review: "a stray winding trail drawn across the paved avenues in jade cities") the plan's
          // paving over a walkable step (the land's ramps under a street; no cliff within two tiles) is laid level: no
          // contour lip or stair band wanders across the kerbs, setts and drain
          bool flat = true;
          for (int oy = -2; oy <= 2 && flat; oy++)
            for (int ox = -2; ox <= 2; ox++)
              if (m.heightBits(tx + ox, ty + oy) & Map::HEIGHT_CLIFF) { flat = false; break; }
          c = reliefPixel(m, px, py, c, real, flat);
        }
        return c;
      }
    }
    if (groundWater(real)) if (const uint32_t q = quayFacePx(m, px, py)) return q;
    const int qx = (px & 15) < 8 ? -1 : 1, qy = (py & 15) < 8 ? -1 : 1;
    nearUrb = ub == 0 && (urbPainted(m, tx + qx, ty) || urbPainted(m, tx, ty + qy) || urbPainted(m, tx + qx, ty + qy));
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
    // (fixer M6b r1, review: "a ruin's and a cave's approach: square, tile-aligned dirt patches with hard stair-step
    // edges") bare earth meeting soft land: a smooth field over the four tile centres round the pixel (a lone dirt tile
    // a rounded patch, a staircase of them one wobbling track, an L a curve), roughened by two octaves of noise, instead
    // of the tile's square nudged a few px by the warp
    if (m.kind == MapKind::Overworld && (real == Ground::Dirt || w == Ground::Dirt)) {
      const float fx = (px - 7.5f) / 16.0f, fy = (py - 7.5f) / 16.0f;
      const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
      const float ax = fx - ix, ay = fy - iy;
      const Ground q[4] = {m.at(ix, iy), m.at(ix + 1, iy), m.at(ix, iy + 1), m.at(ix + 1, iy + 1)};
      bool pure = true, anyDirt = false, anyLand = false;
      for (const Ground qq : q) {
        if (qq == Ground::Dirt) anyDirt = true;
        else if (ecoGround(qq) || qq == Ground::Plaza || qq == Ground::StoneFloor) anyLand = true;   // (a ruin's floor too)
        else pure = false;
      }
      if (pure && anyDirt && anyLand) {
        auto dv = [&](int k) { return q[k] == Ground::Dirt ? 1.0f : 0.0f; };
        float v = (dv(0) * (1 - ax) + dv(1) * ax) * (1 - ay) + (dv(2) * (1 - ax) + dv(3) * ax) * ay;
        v += (vnoise(px / 11.0f, py / 11.0f, 2101) - 0.5f) * 0.42f + (vnoise(px / 4.0f, py / 4.0f, 2103) - 0.5f) * 0.16f;
        if (v > 0.5f) { w = Ground::Dirt; wx = (float)px; wy = (float)py; }
        else {
          // the land: the pixel's own tile's, else the nearest land corner's
          Ground land = real;
          if (land == Ground::Dirt) {
            const int near = (ax < 0.5f ? 0 : 1) + (ay < 0.5f ? 0 : 2);
            land = q[near];
            for (int k = 0; k < 4 && land == Ground::Dirt; k++) if (q[k] != Ground::Dirt) land = q[k];
          }
          w = land; wx = (float)px; wy = (float)py;
        }
      }
    }
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
    } else if (m.kind == MapKind::Overworld && ecoGround(g) && ecoGround(real) && [&]() {
      // (fixer M8 r2, review: "the grass patches by the yurts are hard-edged rectangles") two grounds of ONE biome (a
      // village's trodden grass left in its wood's forest floor, a clearing): a smooth field over the four tile centres
      // round a gently meandered point (a lone tile a rounded patch, a staircase one wobbling edge, a long straight run
      // a soft meander), roughened by two octaves of noise. The broad warp below only slid their tile rectangles about.
      const float mx = px + (vnoise(px / 37.0f, py / 37.0f, 2141) - 0.5f) * 22.0f;
      const float my = py + (vnoise(px / 37.0f, py / 37.0f, 2143) - 0.5f) * 22.0f;
      const float fx = (mx - 7.5f) / 16.0f, fy = (my - 7.5f) / 16.0f;
      const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
      const float ax = fx - ix, ay = fy - iy;
      const Eco own = ecoT(m, tx, ty);
      const Ground q[4] = {m.at(ix, iy), m.at(ix + 1, iy), m.at(ix, iy + 1), m.at(ix + 1, iy + 1)};
      Ground A = Ground::Void, B = Ground::Void;
      for (int k = 0; k < 4; k++) {
        if (!ecoGround(q[k]) || ecoT(m, ix + (k & 1), iy + (k >> 1)) != own) return false;
        if (A == Ground::Void || q[k] == A) A = q[k];
        else if (B == Ground::Void || q[k] == B) B = q[k];
        else return false;   // three grounds: the warp's own edges
      }
      if (B == Ground::Void) {   // the meandered point lies wholly in one ground
        if (A == real && A == g) return false;   // (the plain inside of a patch: nothing to decide)
        g = A; sx = px; sy = py; E = own;
        return true;
      }
      if ((int)B < (int)A) std::swap(A, B);
      auto av = [&](int k) { return q[k] == A ? 1.0f : 0.0f; };
      float v = (av(0) * (1 - ax) + av(1) * ax) * (1 - ay) + (av(2) * (1 - ax) + av(3) * ax) * ay;
      if (av(0) == av(3) && av(1) == av(2) && av(0) != av(1)) {   // a diagonal step: one band through the corner
        const float dd = av(0) > 0.5f ? std::fabs(ax - ay) : std::fabs(ax + ay - 1.0f);
        v = std::max(v, 1.0f - dd * 0.9f);
      }
      v += (vnoise(px / 10.0f, py / 10.0f, 2145) - 0.5f) * 0.40f + (vnoise(px / 4.0f, py / 4.0f, 2147) - 0.5f) * 0.18f;
      g = v > 0.5f ? A : B; sx = px; sy = py; E = own;
      return true;
    }()) {
      // (the field above decided the pixel)
    } else if (m.kind == MapKind::Overworld && ecoGround(g)) {
      // (fixer r2: a gentler ordered dither, 8 px of jitter instead of 18: the wide checkerboard fringe read as spilled
      // paint; the finer noise carries the edge instead)
      // ((M7c fixer, review: "the sand/ash and sand/grass edges are a coarse 50% checkerboard dither with stray
      // isolated pixels" round a city's civic ground) the ordered jitter is down to 2 px: the edge is a ragged line
      // drawn by the fine noise, with at most a pixel's dither seam, never a band of checkerboard)
      const float ex = px + (vnoise(px / 52.0f, py / 52.0f, 811) - 0.5f) * 64.0f + (bayer(px, py) - 0.5f) * 2.0f + (vnoise(px / 5.0f, py / 5.0f, 813) - 0.5f) * 14.0f +
                       (vnoise(px / 2.5f, py / 2.5f, 814) - 0.5f) * 4.0f;
      const float ey = py + (vnoise(px / 52.0f, py / 52.0f, 817) - 0.5f) * 64.0f + (bayer(px + 2, py + 1) - 0.5f) * 2.0f + (vnoise(px / 5.0f, py / 5.0f, 819) - 0.5f) * 14.0f +
                       (vnoise(px / 2.5f, py / 2.5f, 820) - 0.5f) * 4.0f;
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
        // (fixer M8 r2) another ground of the SAME biome is the field's above (this broad warp slid its tile squares)
        const bool sameEco = eE == ecoT(m, sx >> 4, sy >> 4);
        if (!sameEco) { g = e; E = eE; sx = px; sy = py; }
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
  if (nearUrb || urbYield != Ground::Void) { g = urbYield != Ground::Void ? urbYield : real; sx = px; sy = py; E = Eco::COUNT; }   // (M7b) the fabric's edge
  if (deckYield != Ground::Void && urbYield == Ground::Void) { g = deckYield; sx = px; sy = py; E = Eco::COUNT; }   // (M7c fixer r3) the deck's slant
  // (M7c fixer r3, review: "brown wedges stick up over the stone edge of a town square") a plank town's lone street
  // tile let into its stone square's lip (the square on three sides of it) is the square's own paving, not an orphan
  // patch of deck
  if (m.kind == MapKind::Overworld && real == Ground::Road && g == Ground::Road && paveMatAt(m, tx, ty) == 4) {
    const int nP = (m.at(tx - 1, ty) == Ground::Plaza) + (m.at(tx + 1, ty) == Ground::Plaza) + (m.at(tx, ty - 1) == Ground::Plaza) + (m.at(tx, ty + 1) == Ground::Plaza);
    if (nP >= 3) g = Ground::Plaza;
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
  if (soft(real) && real != Ground::CaveFloor && !nearUrb) {
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
  if (m.kind == MapKind::Overworld && !deckTown && !deckZ && (real == Ground::Road || real == Ground::Plaza || (soft(real) && !groundWater(real)))) {
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
  // (fixer M7b r2, review: "the sylvan city's ground is a blotchy mint camouflage of big round patches") inside a city
  // the silverwood's big moss blotches fade out: a smooth "in the city" field (each corner of a lattice 8 tiles apart:
  // fabric within 8 tiles of it; blended between the corners, so there is no edge) raises the blotches' threshold;
  // never in a village (its urb is all 0)
  float cityMoss = 0.0f;
  if (E == Eco::Silverwood && m.kind == MapKind::Overworld && !m.m->urb.empty()) {
    auto cityAt = [&](int cxl, int cyl) -> float {   // lattice corner (in units of 8 tiles)
      for (int oy = -8; oy <= 8; oy += 4)
        for (int ox = -8; ox <= 8; ox += 4) if (m.urbAt(cxl * 8 + ox, cyl * 8 + oy)) return 1.0f;
      return 0.0f;
    };
    const float fx = (px - 63.5f) / 128.0f, fy = (py - 63.5f) / 128.0f;
    const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    const float ax = fx - ix, ay = fy - iy;
    cityMoss = (cityAt(ix, iy) * (1 - ax) + cityAt(ix + 1, iy) * ax) * (1 - ay) + (cityAt(ix, iy + 1) * (1 - ax) + cityAt(ix + 1, iy + 1) * ax) * ay;
  }
  // (EMB_BAKE_CLASSIC=1: the classic family grounds only, the A side of the bake-cost A/B in m3c_land_perf.txt)
  static const bool classicOnly = std::getenv("EMB_BAKE_CLASSIC") != nullptr;
  bool ecoDone = false;
  if (cityMoss > 0.0f && !classicOnly && ecoGround(g)) {   // (the silverwood's turf, its moss kept small in a city)
    // (the kept turf: the three flat tones of the wild silverwood become one soft gradient, the pale moss gone)
    const uint32_t wild = pick3(n, C(106, 136, 118), C(120, 150, 130), C(134, 162, 142));
    const uint32_t kept = lerpc(C(108, 140, 116), C(124, 154, 130), std::clamp(n, 0.0f, 1.0f));
    c = (bayer(px, py) < cityMoss * 1.6f - 0.3f) ? kept : wild;
    const float mv = vnoise(px / 8.0f, py / 8.0f, 1771);
    const float th = 0.70f + 0.40f * cityMoss;
    if (mv > th) c = mv > th + 0.07f ? C(156, 180, 166) : C(140, 166, 152);
    else c = tufted(c, px, py, 1773, 0.4f, 0.78f, 1.14f);
    if (h > 0.996f) c = C(246, 252, 250);
    ecoDone = true;
  }
  if (!ecoDone) ecoDone = !classicOnly && m.kind == MapKind::Overworld && ecoGround(g) && ecoPixel(E, g, px, py, n, h, c);
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
      bool plankBridge = false;
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
        struct DeckCache { const void* map = nullptr; int ox = 0, oy = 0, tx = INT32_MIN, ty = INT32_MIN; int dir = 0; bool plank = false; };
        thread_local DeckCache dc;
        if (dc.map != (const void*)m.m || dc.ox != m.ox || dc.oy != m.oy || dc.tx != tx || dc.ty != ty) {
          dc.map = (const void*)m.m; dc.ox = m.ox; dc.oy = m.oy; dc.tx = tx; dc.ty = ty;
          int x0 = tx, x1 = tx, y0 = ty, y1 = ty;
          int wEW = 0, wNS = 0;   // (fixer M7b r2) the water beside the deck's tiles: east / west of them, north / south
          dc.plank = false;
          auto wcount = [&](int x, int y) {
            wEW += groundWater(m.at(x - 1, y)) + groundWater(m.at(x + 1, y));
            wNS += groundWater(m.at(x, y - 1)) + groundWater(m.at(x, y + 1));
            // (M7c fixer r3) a bridge of a plank town's deck (a street of planks runs onto it)
            if (!dc.plank && m.kind == MapKind::Overworld)
              for (int k = 0; k < 4; k++) {
                const int qx = x + (k == 0) - (k == 1), qy = y + (k == 2) - (k == 3);
                if (m.at(qx, qy) != Ground::Bridge && plankDeckT(m, qx, qy)) { dc.plank = true; break; }
              }
          };
          wcount(tx, ty);
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
              wcount(nx, ny);
              x0 = std::min(x0, nx); x1 = std::max(x1, nx); y0 = std::min(y0, ny); y1 = std::max(y1, ny);
            }
          }
          dc.dir = (x1 - x0) > (y1 - y0) ? 1 : (y1 - y0) > (x1 - x0) ? 2 : 0;
          // (fixer M7b r2, review: "an avenue crosses a canal with no parapet") a wide deck (a city avenue's whole
          // width) over a narrow stream: the way across is the one with the water on its flanks, not the longer side
          bool cityDeck = false;
          if (!m.m->urb.empty())
            for (int oy = -3; oy <= 3 && !cityDeck; oy++)
              for (int ox = -3; ox <= 3; ox++) if (m.urbAt(tx + ox, ty + oy)) { cityDeck = true; break; }
          if (cityDeck && (x1 - x0) >= 3 && (y1 - y0) >= 1 && wEW != wNS && std::max(wEW, wNS) >= 2 * std::min(wEW, wNS) + 2)
            dc.dir = wEW > wNS ? 2 : 1;
        }
        if (dc.dir) eastWest = dc.dir == 1;
        plankBridge = dc.plank;
      }
      int a = eastWest ? px : py;   // across the planks
      bool gap = ((a % 4) + 4) % 4 == 0;
      c = gap ? C(70, 46, 30) : lerpc(C(140, 96, 56), C(160, 112, 66), hashf(a / 4, 0, 261));
      if (!gap && hashf(a / 4, (eastWest ? py : px) / 6, 263) < 0.12f) c = mul(c, 0.9f);   // a worn plank end
      // (M7c fixer r3, review: "planks running in alternating directions: horizontal, then vertical, then horizontal
      // again") a plank town's bridge is its deck run on over the water: the same boards, laid the same way
      if (plankBridge) c = pavePixel(4, true, px, py, 0.0f, -1.0f, h, n);
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
        // ((M7c fixer r3) not where a plank town's deck runs on from the bridge: one floor)
        auto land = [&](int x, int y) { const Ground q = m.at(x, y); return q != Ground::Bridge && !groundWater(q) && q != Ground::Void && !(plankBridge && plankDeckT(m, x, y)); };
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
      // (M10 ART) a lighter veil than before, so the sea's moving swell under it (render_sea.cpp seaDrawUnderlay) shows
      // through; the colours are chosen so deep and shallow water still read as they did
      c = deep ? C(24, 52, 111, 160) : C(61, 135, 184, 120);
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
          c = v > 0.62f ? C(24, 52, 111, 160) : v > 0.42f ? C(38, 89, 146, 140) : C(61, 135, 184, 120);
        }
      }
      if (m.kind != MapKind::Overworld) c = C(30, 60, 90, 170);
      if (shore > 0 && !deep) c = C(123, 192, 216, 120);
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
      } else if (m.kind == MapKind::Overworld && m.at(tx, ty - 1) == Ground::Bridge && !isBoardwalk(m, tx, ty - 1)) {
        // (M10 ART) a harbour's pier (a deck more than a tile deep, or one standing out into the sea): its south face
        // over the water, the timber piles under it every few paces, the sea washing round their feet
        if (ly == 0) c = C(140, 100, 62);
        else if (ly <= 2) c = ly == 1 ? C(82, 54, 38) : C(62, 40, 30);
        else {
          const int col = ((px % 12) + 12) % 12;
          if (col <= 2 && ly <= 8) {
            uint32_t s = col == 0 ? C(120, 88, 60) : col == 1 ? C(86, 60, 42) : C(58, 40, 32);
            if (ly == 3) s = C(44, 30, 26);   // in the deck's shade
            if (ly >= 7) s = lerpc(s, C(52, 84, 72), 0.55f);   // weed and wet at the waterline
            if (ly == 8) s = (col & 1) ? C(220, 238, 248) : C(160, 200, 220);
            c = s;
          } else if (ly <= 5) c = lerpc(c, C(14, 26, 54, 230), ly == 3 ? 0.75f : ly == 4 ? 0.5f : 0.3f);   // the shade under the deck
          else if (ly == 8 && (col == 3 || col == 11)) c = C(200, 226, 240, 200);
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
        const float cw = crevasseW(u, v);   // (M11 fixer r2: crevasse fields only, as on the snow)
        const float d = vnoise(u / 44.0f, v / 12.0f, 1805) - 0.5f;
        if (cw > 0 && std::fabs(d) < 0.012f * cw) c = C(44, 76, 136);
        else if (cw > 0 && d >= 0.012f * cw && d < 0.032f * cw) c = mul(c, 0.82f);
        else if (cw > 0 && d <= -0.012f * cw && d > -0.026f * cw) c = C(236, 246, 255);
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
  // (M7c fixer r3) one rim round the whole plank deck (deckEdgeAt / deckRimPx: streets, squares, bridges and the
  // city's fabric are one floor; the edge on the slant where its tiles staircase)
  if (deckZ && (deckE.in ? (g == Ground::Road || g == Ground::Plaza) : !groundWater(g))) c = deckRimPx(deckE, px, py, c);
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
  if (m.kind == MapKind::Overworld && m.relief()) {
    // (fixer M7b r1) a city's own paving beside its planned fabric (the plaza and road flags the plan left unclassed)
    // is laid level over a walkable step like the fabric itself: no stair band wanders across it into the avenue
    bool flat = false;
    if (!m.m->urb.empty() && (real == Ground::Road || real == Ground::Plaza)) {
      bool city = false, cliff = false;
      for (int oy = -2; oy <= 2 && !cliff; oy++)
        for (int ox = -2; ox <= 2; ox++) {
          if (m.heightBits(tx + ox, ty + oy) & Map::HEIGHT_CLIFF) { cliff = true; break; }
          if (m.urbAt(tx + ox, ty + oy)) city = true;
        }
      flat = city && !cliff;
    }
    c = reliefPixel(m, px, py, c, g, flat);
  }
  return c;
}

// Relief (M1, VISION_PLAN 11; M2 readability pass; M11 TERRAIN: 11.2 / 11.3, 15.32): the generator gives every tile a
// level 0..7; the tile on the LOWER side of a step carries a step bit (HEIGHT_CLIFF, HEIGHT_RAMP, HEIGHT_SLOPE; FALL and
// STAIR qualify them). Drawn from a smooth level field, light from the top-left, 3/4 view:
//   higher NORTH  a south-facing face, 12 px for one level; a stacked step (two levels in one tile row) shows 24 px: the
//                 upper face, a ledge with its own lip and tufts, the lower face. A lit lip with the plateau's cover
//                 hanging over it (turf with a dark undercut, a snow cornice, or bare lit rock), the face's material
//                 darkening to a crisp foot line, a 4-px cast shadow on the ground below, and under a stacked face a soft
//                 shadow thrown 6 px to the south-east
//   higher SOUTH  the plateau's top reaches up over this tile (it stands higher, so it shows higher): a 2-px lit rim,
//                 a dark outline and a thin shadow at its foot
//   higher WEST   an east-facing 3-4 px side lip in shade (twice that for two levels), its shadow cast east
//   higher EAST   a west-facing side lip, lit, a contact shade at its foot
//   HEIGHT_SLOPE  a hillside: the step's own band (face, rim or side lip) in turf instead of bare face, graded from
//                 the upper level to the lower, a lit brow, grass strokes, a soft toe; a run that changes between face
//                 and hillside keeps one band and frays from bare earth into turf (M11 fixer r2)
//   ramps         stone stairs (HEIGHT_STAIR, or a road / square) cut into the face band with cheeks and lit noses; an
//                 earth ramp is a trodden path of worn steps through the same band, its sides fraying into the bank
//   HEIGHT_FALL   the face under a waterfall is wet dark rock with a notch in its lip (the FALLS lane draws the water)
// Every level is graded about 4 % lighter and 5 % less saturated than the one below, hazed blue from level 5 (snow,
// which cannot get brighter, is shaded darker and cooler on the lower levels instead). The face material follows the
// land (facePalE): earth banks on temperate low ground, granite higher up, cold grey rock under snow, sandstone in the
// desert, basalt in the ash fields, ice on the glaciers, chalk under the downs, banded strata in the badlands, dark
// mossy rock in the marsh and the deep woods.
uint32_t View::reliefPixel(const TMap& m, int px, int py, uint32_t c, Ground g, bool flat) {
  const int tx = px >> 4, ty = py >> 4, lx = px & 15, ly = py & 15;
  const bool snowy = g == Ground::Snow || g == Ground::Ice;
  const uint8_t bits = m.heightBits(tx, ty);
  const int l = bits & Map::HEIGHT_LEVEL;
  const bool water = groundWater(g) || g == Ground::Void;
  const bool rock = g == Ground::Rock;
  // (M11) a massif's or crag's rock paints its own tiers, faces and light (mountainPixel); the cliff where it stands
  // over lower land is drawn on that land's tiles below. A rock tile that is itself the lower tile of a step (the
  // mountains keep rock only on their stacked faces: chunkgen) is drawn as that face, like every other step
  if (rock && !(bits & Map::HEIGHT_STEP)) return c;
  const bool ramp = (bits & Map::HEIGHT_RAMP) != 0;
  const Biome bio = m.biomeAt(tx, ty);
  FacePal FP = facePalE(ecoT(m, tx, ty), bio, l);
  RockPal P = FP.p;
  auto wob = [&](int a, int b, uint32_t seed, float amp) { return (int)std::lround((vnoise(a / 7.0f, b * 1.37f, seed) - 0.5f) * amp); };
  // ---- the ground's own colour before the grade (c0), and the snow cover: mountains and the cold north hold snow on
  // their high ground. The cover is a field blended between tile centres (each tile's biome and level), looked up
  // through the same broad warp as the ecotones, so its edge meanders and dithers instead of following the tile grid
  // (M3c fixer round 3) paving and trodden earth take the cover too, but swept: the threshold rises smoothly with a
  // paving field blended between tile centres, so the snow thins out across the kerb onto the cobbles in a ragged
  // drift line, and the cover itself is drawn with wind-cut drifts, blue hollows and a glitter of ice
  uint32_t c0 = c;
  bool cover = snowy;
  if (!water && !rock && g != Ground::Bridge && g != Ground::StoneFloor && !snowy) {
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
      // (M6 finish fixer) a mountain town's snowline cover is the high ground's: the townsfolk keep their streets swept
      const Biome hb = m.biomeAt(tx, ty);
      const bool coldLand = hb == Biome::Snow || hb == Biome::Taiga;
      const float th = 0.5f + swept * (coldLand ? 0.95f : 2.5f) + (bayer(px, py) - 0.5f) * 0.12f;
      // (M11 fixer r2) a trail or a trodden floor (dirt) under the snow line is packed snow, not bare earth: the summit
      // trails read as pale strips with grey blobs (the cover broken at random over the earth) and the floor before a
      // summit shrine as a grey smudge. Blended between the tile centres with a ragged edge, so the path meanders.
      float trod = 0;
      if (k > 0.5f) {
        const float fx = (px - 7.5f) / 16.0f, fy = (py - 7.5f) / 16.0f;
        const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
        const float ax = fx - ix, ay = fy - iy;
        auto dt = [&](int x, int y) { return m.at(x, y) == Ground::Dirt ? 1.0f : 0.0f; };
        trod = (dt(ix, iy) * (1 - ax) + dt(ix + 1, iy) * ax) * (1 - ay) + (dt(ix, iy + 1) * (1 - ax) + dt(ix + 1, iy + 1) * ax) * ay;
        trod += (vnoise(px / 4.0f, py / 4.0f, 841) - 0.5f) * 0.4f;
      }
      if (trod > 0.5f) {
        uint32_t s = C(206, 214, 230);
        const float rv = vnoise(px / 3.0f, py / 2.0f, 843);
        if (rv > 0.66f) s = C(188, 198, 218);                                   // trodden ruts
        else if (rv < 0.25f) s = C(218, 225, 238);
        if (hashf(px >> 1, py / 3, 844) < 0.05f) s = C(170, 182, 208);          // footprints
        if (trod < 0.58f) s = C(222, 229, 242);                                 // its soft, kicked-up edge
        c0 = s;
        cover = true;
      } else if (k > th) {
        const float dr = vnoise((px * 0.92f + py * 0.38f) / 30.0f, (py * 0.92f - px * 0.38f) / 9.0f, 837);
        uint32_t s = dr > 0.62f ? C(246, 249, 254) : dr < 0.34f ? C(214, 224, 240) : (vnoise(px / 6.0f, py / 6.0f, 833) < 0.5f ? C(230, 236, 246) : C(236, 241, 250));
        if (dr > 0.58f && dr <= 0.62f) s = C(222, 230, 244);   // the drift's shaded lee under its crest
        if (hashf(px, py, 839) < 0.012f) s = C(255, 255, 255);
        if (k < th + 0.05f) s = C(196, 206, 224);              // the thin, trodden edge of the cover
        else if (k < th + 0.10f) s = lerpc(s, C(206, 216, 234), 0.6f);
        c0 = s;
        cover = true;
      }
    }
  }
  // the ground as it looks on level lv (water and a massif's rock paint their own light: ungraded)
  auto gradeAt = [&](float lv) -> uint32_t {
    if (water || rock) return c0;
    return cover ? aerialSnow(c0, lv) : aerialGrade(c0, lv);
  };
  if (flat) {
    // (fixer M7b r1) a city's paving laid over a walkable level step is one levelled floor: the grade is blended
    // smoothly between the tile centres instead of stepping at a contour drawn across the kerbs, setts and drain
    const float fx = (px - 7.5f) / 16.0f, fy = (py - 7.5f) / 16.0f;
    const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    const float ax = fx - ix, ay = fy - iy;
    const float lv = ((float)m.heightAt(ix, iy) * (1 - ax) + (float)m.heightAt(ix + 1, iy) * ax) * (1 - ay) +
                     ((float)m.heightAt(ix, iy + 1) * (1 - ax) + (float)m.heightAt(ix + 1, iy + 1) * ax) * ay;
    return gradeAt(cover ? (float)l : lv);
  }
  const uint32_t graded = gradeAt((float)l);
  // (M1 round 3) a bridge's deck spans the water at one height: no face, rim or stair is drawn across it
  if (g == Ground::Bridge) return graded;
  const uint32_t alpha = 255u << 24;
  // ---- the levels and step bits of the 6x6 tiles around, cached per thread (bakes run on worker threads)
  // sl: 1 a hillside or an earth ramp (drawn as a rise), 2 any step, 8 an earth ramp
  struct LvCache { const void* map = nullptr; int ox = 0, oy = 0, tx = INT32_MIN, ty = INT32_MIN; uint8_t lv[36], bt[36], sl[36]; bool flat = true, stack = false, anySlope = false, anyRamp = false; };
  thread_local LvCache lc;
  if (lc.map != (const void*)m.m || lc.ox != m.ox || lc.oy != m.oy || lc.tx != tx || lc.ty != ty) {
    lc.map = (const void*)m.m; lc.ox = m.ox; lc.oy = m.oy; lc.tx = tx; lc.ty = ty;
    lc.flat = true; lc.stack = false; lc.anySlope = false; lc.anyRamp = false;
    for (int j = 0; j < 6; j++)
      for (int i = 0; i < 6; i++) {
        uint8_t hb = m.heightBits(tx - 2 + i, ty - 2 + j);
        // (a ramp a road or a square climbs is built stone stairs too)
        if ((hb & Map::HEIGHT_RAMP) && !(hb & Map::HEIGHT_STAIR)) {
          const Ground q = m.at(tx - 2 + i, ty - 2 + j);
          if (q == Ground::Road || q == Ground::Plaza) hb |= Map::HEIGHT_STAIR;
        }
        lc.lv[j * 6 + i] = hb & Map::HEIGHT_LEVEL;
        lc.bt[j * 6 + i] = hb & (Map::HEIGHT_STEP | Map::HEIGHT_STAIR | Map::HEIGHT_FALL);
        if ((int)(hb & Map::HEIGHT_LEVEL) != l) lc.flat = false;
      }
    auto earthRamp = [&](int i, int j) {
      if (i < 0 || j < 0 || i > 5 || j > 5) return false;
      const uint8_t b = lc.bt[j * 6 + i];
      return (b & Map::HEIGHT_RAMP) && !(b & (Map::HEIGHT_STAIR | Map::HEIGHT_FALL));
    };
    for (int j = 0; j < 6; j++)
      for (int i = 0; i < 6; i++) {
        const uint8_t b = lc.bt[j * 6 + i];
        uint8_t f = (b & Map::HEIGHT_STEP) ? 2 : 0;
        // (M11 fixer r1) an earth ramp is a flight of worn steps cut into the face (rampPx), not a gap where the face
        // sinks away: the bank runs on unbroken past it (it read as separate slabs with gaps between)
        if (b & Map::HEIGHT_SLOPE) f |= 1;
        if (earthRamp(i, j)) { f |= 8; lc.anyRamp = true; }
        lc.sl[j * 6 + i] = f;
        if (f & 1) lc.anySlope = true;
      }
    for (int j = 1; j < 6; j++)
      for (int i = 0; i < 6; i++) if (lc.lv[(j - 1) * 6 + i] >= lc.lv[j * 6 + i] + 2) lc.stack = true;
  }
  if (lc.flat) return graded;
  // (M3c fixer round 3) one sheet of water may lie over tiles of two levels (a lake across a relief step): water has no
  // cliff in it, unless it pours over the step (a fall: its wet face is drawn under the falling sheet)
  if (water && groundWater(m.at(tx, ty)) && !(bits & Map::HEIGHT_FALL)) {
    bool allWet = true;
    for (int oy = -2; oy <= 1 && allWet; oy++)
      for (int ox = -1; ox <= 1; ox++) {
        if ((int)lc.lv[(2 + oy) * 6 + 2 + ox] == l) continue;
        if (!groundWater(m.at(tx + ox, ty + oy))) { allWet = false; break; }
      }
    if (allWet) return graded;
  }
  auto LV = [&](int x, int y) -> float { return (float)lc.lv[std::clamp(y, 0, 5) * 6 + std::clamp(x, 0, 5)]; };
  const int bx = (tx - 2) * 16, by = (ty - 2) * 16;
  // ---- level steps, drawn from a smooth level field: VF is the tile levels blended between tile centres, F that
  // rounded, so a step the grid lays as a staircase is drawn as one slanted cliff and corners round off
  auto VF = [&](int qx, int qy) -> float {
    const float fx = (qx - bx - 8) / 16.0f, fy = (qy - by - 8) / 16.0f;
    const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    const float ax = fx - ix, ay = fy - iy;
    return (LV(ix, iy) * (1 - ax) + LV(ix + 1, iy) * ax) * (1 - ay) + (LV(ix, iy + 1) * (1 - ax) + LV(ix + 1, iy + 1) * ax) * ay;
  };
  auto F = [&](int qx, int qy) -> int { return (int)std::floor(VF(qx, qy) + 0.5f); };
  const int own = F(px, py);
  // (M11) stone stairs are cut into the face band (rampPx); an earth ramp is a notch through the face, its worn steps
  // drawn where the face has sunk away (the FACE branch)
  const bool rampHere = ramp && !water;
  const bool stoneStair = (lc.bt[14] & Map::HEIGHT_STAIR) != 0;
  auto regrade = [&](int to) { return to == l ? graded : gradeAt((float)to); };
  uint32_t out = regrade(own);   // (a corner of the next level reaching over)
  bool opaque = false;
  // ---- (M11) hillsides: where the step tiles round a pixel are HEIGHT_SLOPE (or a worn earth ramp), the step is a
  // walkable rise. hs is the hillside tiles' weight against all the step tiles' weight, both blended between the tile
  // centres (continuous everywhere: a hillside running into a cliff fades the cliff in over a tile, so the face grows
  // out of the slope instead of ending in a stub, and an earth ramp is a notch cut through the face). The hillside is
  // the ground graded smoothly from the lower level to the upper one across the step, shaded by which way it faces (the
  // light is top-left: a rise facing south or east falls into shade, one facing north or west catches the light), with
  // a lit brow, a darker toe and hachures down the fall line, so it reads as a rise, never as a line. (An earth ramp's
  // worn steps are drawn where its face has sunk away: the FACE branch below.)
  float hs = 0;
  auto bil = [&](int qx, int qy, int bit) -> float {
    const float fx = (qx - bx - 8) / 16.0f, fy = (qy - by - 8) / 16.0f;
    const int ix = std::clamp((int)std::floor(fx), 0, 4), iy = std::clamp((int)std::floor(fy), 0, 4);
    const float ax = std::clamp(fx - ix, 0.0f, 1.0f), ay = std::clamp(fy - iy, 0.0f, 1.0f);
    auto f = [&](int i, int j) { return (lc.sl[j * 6 + i] & bit) ? 1.0f : 0.0f; };
    return (f(ix, iy) * (1 - ax) + f(ix + 1, iy) * ax) * (1 - ay) + (f(ix, iy + 1) * (1 - ax) + f(ix + 1, iy + 1) * ax) * ay;
  };
  if (lc.anySlope) {
    // (smoothed along and across over about a tile and a half, so a run that changes between hillside and cliff tile by
    // tile dips and rises gently instead of breaking into a dashed line of short faces)
    auto raw = [&](int qx, int qy) { const float sw = bil(qx, qy, 1); return sw > 0 ? sw / (bil(qx, qy, 2) + 0.08f) : 0.0f; };
    // (M11 fixer r1) smoothed mostly ALONG the contour (its tangent, from the level field's gradient) over 2 tiles each
    // way: a bank whose tiles change between hillside and face every few tiles sank and rose in 1-tile wedges (it read
    // as separate logs and triangular mounds); now the face tapers over 3-4 tiles into the rise and back
    float tx0 = -(VF(px, py + 4) - VF(px, py - 4)), ty0 = VF(px + 4, py) - VF(px - 4, py);
    const float tm = std::sqrt(tx0 * tx0 + ty0 * ty0);
    if (tm > 0.002f) { tx0 /= tm; ty0 /= tm; } else { tx0 = 1.0f; ty0 = 0.0f; }
    float r = 2.0f * raw(px, py) + 0.6f * (raw(px + (int)(ty0 * 8), py - (int)(tx0 * 8)) + raw(px - (int)(ty0 * 8), py + (int)(tx0 * 8)));
    float wsum = 3.2f;
    static const float kAlong[4] = {8.0f, 16.0f, 24.0f, 32.0f}, kW[4] = {1.4f, 1.1f, 0.8f, 0.5f};
    for (int k = 0; k < 4; k++) {
      const int ox = (int)std::lround(tx0 * kAlong[k]), oy = (int)std::lround(ty0 * kAlong[k]);
      r += kW[k] * (raw(px + ox, py + oy) + raw(px - ox, py - oy));
      wsum += 2.0f * kW[k];
    }
    r /= wsum;
    hs = std::clamp((r - 0.2f) / 0.5f, 0.0f, 1.0f);
  }
  // (M11 fixer r2) ONE band per step. A bank's face, its hillside stretches and its worn earth ramps share the same
  // band geometry (the lip on the contour, the foot 12 px below it, the rim and the side lips likewise); only the
  // MATERIAL changes along the run: bare face, turf (a walkable grassy rise: hs), or a trodden path (an earth ramp: er).
  // The face used to sink into the hillside (its lip coming down to its foot), and the hillside was the level field
  // graded across the step: wherever the world's bits changed between face, hillside and ramp every few tiles the
  // bank broke into tapering slabs, triangular mounds and see-through bands, a soft diagonal smear where a hillside
  // ran on a diagonal, and an orphan slab where a face run ended. With one band nothing can taper or break: the bare
  // earth frays into the turf through a dithered, noisy edge over the 2-3 tiles hs blends along the contour.
  const bool turfPx = hs > 0 && !water &&
                      hs + (vnoise(px / 5.0f, py / 4.0f, 1203) - 0.5f) * 0.7f + (bayer(px, py) - 0.5f) * 0.12f > 0.5f;
  const float er = lc.anyRamp && !water ? bil(px, py, 8) : 0.0f;
  const bool pathPx = er > 0 && er + (vnoise(px / 3.0f, py / 3.0f, 849) - 0.5f) * 0.5f > 0.62f;
  // the face material, graded for its level: a bank under a rock mass is rock too; a fall's face is wet dark rock
  P = gradePal(P, (float)l + 0.5f);
  const bool wetFace = (bits & Map::HEIGHT_FALL) != 0 || water;
  const uint32_t outline = lerpc(P.dark, C(30, 24, 34), 0.35f);
  auto shade = [&](uint32_t col, float k, float hue) {   // a cast shadow: darker and cooler (the palette's shadow hue)
    return lerpc(mul(col, k), C(48, 34, 92, (int)(col >> 24)), hue);
  };
  // ---- ramps and stairs, cut into the face's own band (the face, side wall or rim found below), so a stair takes the
  //      cliff's slanted shape instead of standing out as a square block. ns: climbing north-south (the steps lie
  //      east-west); a: px into the band from its top, across the steps
  // (M11 fixer r2) a worn earth ramp's treads, one material wherever the ramp climbs (south faces, side drops, rims):
  // trodden soil on an earth bank, trodden grit (warm grey-brown, darker than the rock's lit lip) on rock, packed snow
  // under snow. (Built from the bank's lip and the turf, the side ramps read as pale see-through bands and the rock's
  // ramps as washed-out cream slabs.)
  auto pathEarth = [&]() -> uint32_t {
    if (cover) return lerpc(mul(graded, 0.96f), C(184, 194, 216), 0.30f);
    if (FP.style == 7) return lerpc(lerpc(lerpc(P.hi, P.lip, 0.4f), aerialFace(C(196, 170, 128), (float)l + 0.5f), 0.5f), regrade(l + 1), 0.18f);
    return lerpc(lerpc(P.mid, aerialFace(C(156, 134, 104), (float)l + 0.5f), 0.55f), regrade(l + 1), 0.12f);
  };
  auto rampPx = [&](bool ns, int a) -> uint32_t {
    const int b = ns ? px : py;            // along a step
    const int e = ns ? lx : ly;            // across the ramp within the tile
    const bool cheekA = ns ? (m.heightBits(tx - 1, ty) & Map::HEIGHT_CLIFF) != 0 : (m.heightBits(tx, ty - 1) & Map::HEIGHT_CLIFF) != 0;
    const bool cheekB = ns ? (m.heightBits(tx + 1, ty) & Map::HEIGHT_CLIFF) != 0 : (m.heightBits(tx, ty + 1) & Map::HEIGHT_CLIFF) != 0;
    const float lvA = (float)l + 1.0f - std::min(1.0f, a / 12.0f);   // (stone: the grade climbs with the steps)
    uint32_t r;
    if (!stoneStair) {
      // (M11 fixer r1) a worn earth ramp: treads of packed earth (or snow) in the face's own material, each worn
      // riser a soft shade under a lit nose, the edges wandering, turf (or snow) on the cheeks where the face runs on
      // (a rise of trodden turf: the ground's own colour with the earth showing through on the worn middle, so the
      // ramp reads as a sloping path, not as a slab of the bank's soil)
      const uint32_t earth = pathEarth();
      const float aw = (float)a + (vnoise(b / 5.0f, (float)a / 5.0f, 845) - 0.5f) * 1.6f;
      const int k4 = ((int)std::floor(aw) % 4 + 4) % 4;
      float kk = 1.04f - 0.14f * std::min(1.0f, a / 12.0f);
      if (k4 == 0) kk *= 1.10f; else if (k4 == 3) kk *= cover ? 0.74f : 0.84f;
      if (hashf(px, py, 847) < 0.08f) kk *= 0.88f;
      r = mul(earth, kk);
      if (cover && k4 == 3) r = lerpc(r, C(112, 126, 164), 0.62f);
      else if (cover && k4 == 2) r = lerpc(r, C(150, 164, 200), 0.30f);
      const int ew = 2 + (int)(hash2(b >> 2, a >> 2, 851) % 2);
      if (cheekA && e < ew) r = e == 0 ? mul(P.lo, 0.85f) : mul(P.mid, 0.82f);
      if (cheekB && e > 15 - ew) r = e == 15 ? mul(P.lo, 0.9f) : mul(P.mid, 0.95f);
      r = aerialFace(r, (float)l + 1.0f - std::min(1.0f, a / 12.0f));
    } else {
      // dressed stone steps (4 px each): a lit nose, the tread, the riser in shade, staggered joints; stone cheeks
      // (parapets) where the face runs on beside the stair: the west one's inner face in shade, the east one's lit
      const uint32_t st = lerpc(C(180, 172, 158), P.hi, 0.25f);
      const int k = a & 3, step = a >> 2;
      const int jw = 7, jo = (step * 4) % jw;
      const bool joint = ((b + jo) % jw + jw) % jw == 0;
      if (k == 0) r = mul(st, 1.18f);
      else if (k == 1) r = joint ? mul(st, 0.84f) : st;
      else if (k == 2) r = joint ? mul(st, 0.50f) : mul(st, 0.72f);
      else r = joint ? mul(st, 0.44f) : mul(st, 0.58f);
      if (k != 0 && hashf(px, py, 977) < 0.06f) r = mul(r, 0.92f);
      if (cover && k <= 1) r = lerpc(r, C(236, 241, 250), 0.6f);
      if (cheekA && e < 3) r = e == 0 ? outline : e == 1 ? (cover ? C(240, 244, 252) : mul(st, 1.12f)) : mul(st, 0.62f);
      if (cheekB && e > 12) r = e == 15 ? outline : e == 14 ? (cover ? C(240, 244, 252) : mul(st, 1.14f)) : mul(st, 0.98f);
      r = aerialFace(r, lvA);
    }
    return (r & 0x00FFFFFFu) | alpha;
  };
  // ---- which drop this pixel belongs to. Every direction is looked for (the distance in px to the contour, upward,
  //      downward, west and east); a contour running on a diagonal is found both ways, so one treatment is chosen by
  //      the contour's slope: one clean face, rim or lip per drop, never two bands crossing into stripes.
  const float keep = turfPx ? 0.55f : 1.0f;   // (a turf bank throws a lighter shadow than a bare face)
  const int FH0 = std::clamp(12 + wob(px, 0, 851, 2.0f), 11, 13);   // a one-level face
  // a plateau to the south reaching up; the rims and side lips keep their shape where a drop turns into a rise and
  // fade into it instead (rimA), so they never break into stubs
  const int RH = std::clamp(11 + wob(px, 0, 865, 3.0f), 9, 13);
  // (M2 fixer round 2) the side lip's width follows the contour's slope: full where the drop runs north-south, none where
  // it runs on a diagonal or east-west (the face or rim band covers that), and narrowing smoothly in between
  float sgx = 0, sgy = 0;
  for (int k = -2; k <= 2; k++) {
    const float wgt = k == 0 ? 1.0f : (k == 1 || k == -1) ? 0.75f : 0.4f;
    sgx += wgt * std::fabs(VF(px + 14, py + k * 10) - VF(px - 14, py + k * 10));
    sgy += wgt * std::fabs(VF(px + k * 10, py + 14) - VF(px + k * 10, py - 14));
  }
  const float steep = sgx + sgy > 0.01f ? sgx / (sgx + sgy) : 1.0f;   // 1: the drop runs north-south
  const float sideK = std::clamp((steep - 0.62f) / 0.26f, 0.0f, 1.0f);
  // (M11) the side lips: 3-4 px of the plateau's edge seen side on (its thickness), shaded where it faces east, lit
  // where it faces west, then the shadow it throws east (or a contact shade on the lit side)
  // (M11 fixer r2: on snow the side walls are wider, 5-6 px: a 3-px grey line on white read as a seam, not a drop)
  const int SL0 = (cover ? 5 : 3) + (vnoise(py / 9.0f, 0.5f, 869) > 0.5f ? 1 : 0);
  int SL = (int)std::lround(SL0 * sideK), SLE = (int)std::lround((SL0 + 1) * sideK);
  if (SL < 2) SL = 0;
  if (SLE < 2) SLE = 0;
  // (a drop of two levels or more shows a lip twice as thick: the taller wall's edge)
  if (SL && F(px - 10, py) >= own + 2) SL = std::min(7, SL * 2);
  if (SLE && F(px + 10, py) >= own + 2) SLE = std::min(8, SLE * 2 - 1);
  const int SLR = 8;   // a ramp up a side drop: its steps run 8 px into the low side
  int d = 0, ds = 0, dw = 0, de = 0, up = own;
  for (int k = 1; k <= 22 && !d; k++) if (F(px, py - k) > own) d = k;
  for (int k = 1; k <= RH + 3 && !ds; k++) { const int f2 = F(px, py + k); if (f2 > own) { ds = k; up = f2; } }
  for (int k = 1; k <= std::max(SL + (cover ? 12 : 7), rampHere ? SLR : 0) && !dw; k++) if (F(px - k, py) > own) dw = k;
  for (int k = 1; k <= std::max(SLE + 3, rampHere ? SLR : 0) && !de; k++) if (F(px + k, py) > own) de = k;
  int top = own;
  if (d > 0) top = std::max(F(px, py - d), F(px, py - d - 10));
  // a two-level drop: the lower face of a stack (16 px under its ledge); a face whose foot meets another contour within
  // its height is the upper part of a stack (its ledge is the lower face's lip)
  int above = 0, ledgeAt = 0;
  const bool stackLow = d > 0 && top - own >= 2;
  // (a face sunk below 4 px is gone: only the rise and a light shade at its toe are left, never a dot of outline)
  const int fhF = stackLow ? FH0 + 4 : FH0, fh = fhF;
  if (stackLow)
    for (int j = 1; j <= 12 && !above; j++) if (F(px, py - d - j) > own + 1) above = j;
  if (d > 0 && d <= fhF && !stackLow)
    for (int k = 1; k <= fhF - d + 1 && !ledgeAt; k++) if (F(px, py + k) < own) ledgeAt = k;
  const int shadowF = cover ? (stackLow ? 16 : 11) : 4;   // (fixer r2: longer and deeper on snow, the main cue of height on white)   // (M11 fixer r1) on snow the face's blue shadow reaches further
  const bool vFace = d > 0 && d <= fhF + shadowF, vRim = ds > 0;
  const int vd = vFace && vRim ? std::min(d, ds) : vFace ? d : vRim ? ds : 99;
  const bool hW = dw > 0, hE = de > 0 && de <= SLE + 3;
  const int hd = hW && hE ? std::min(dw, de) : hW ? dw : hE ? de : 99;
  enum { NONE, FACE, RIM, LIPW, LIPE } kind = NONE;
  const bool inFace = vFace && d <= fhF, inRim = vRim && ds <= RH;
  const bool inW = hW && (dw <= SL || (rampHere && dw <= SLR)), inE = hE && (de <= SLE || (rampHere && de <= SLR));
  if (inFace && inRim) kind = d <= ds ? FACE : RIM;
  else if (inFace) kind = FACE;
  else if (inRim) kind = RIM;
  else if (inW && inE) kind = dw <= de ? LIPW : LIPE;
  else if (inW) kind = LIPW;
  else if (inE) kind = LIPE;
  // nothing solid here: the nearest drop's shadow (the same order: a face's, then the rim's, then the side lips')
  else if (vd < 99 && (hd == 99 || vd <= hd + 3)) kind = (vFace && (!vRim || d <= ds)) ? FACE : RIM;
  else if (hd < 99) kind = (hW && (!hE || dw <= de)) ? LIPW : LIPE;
  const uint32_t base0 = out;
  // (M11 fixer r1) a stone stair climbing north is one straight flight across its own tile: its steps run level from
  // cheek to cheek, measured from the face's lip at the tile's middle column (they followed the smoothed contour, so a
  // flight at a bend curled up at both ends like pipes)
  const bool nsStair = rampHere && stoneStair && (lc.lv[1 * 6 + 2] > l) && !water;
  if (nsStair) {
    const int cx = tx * 16 + 8;
    int lipY = INT32_MIN;
    for (int k = 0; k <= 24 && lipY == INT32_MIN; k++) if (F(cx, ty * 16 + 8 - k) > l) lipY = ty * 16 + 8 - k + 1;
    if (lipY != INT32_MIN) {
      const int a = py - lipY, fhS = lc.stack ? FH0 + 4 : FH0;
      if (a >= 0 && a < fhS) return rampPx(true, a);
      if (a >= fhS && a < fhS + 3) out = shade(out, 0.80f + 0.06f * (a - fhS), 0.12f);   // the flight's foot shade
    }
  }
  if (kind == FACE) {
    if (FP.style == 7 && m.at(px >> 4, (py - d - 3) >> 4) == Ground::Rock) { FP = {kGranitePal, 0}; P = gradePal(FP.p, (float)l + 0.5f); }
    if (rampHere && stoneStair && !nsStair && d <= fhF) return rampPx(true, d - 1);
    if (d <= fhF && pathPx && !stackLow) {
      // (M11 fixer r2) a worn earth ramp: a trodden path cut down through the band, treads of packed earth in the
      // bank's own material, each worn riser a soft shade under a lit nose, the lower steps in shade (they face the
      // viewer); its sides fray into the bank through the dithered edge of er instead of ending at the tile's edge
      // (lighter than the bank's face: trodden, sunlit treads, so the way up reads at a glance; a dark cheek where the
      // path meets the face beside it)
      const uint32_t earth = pathEarth();
      const float a = (float)(d - 1) + (vnoise(px / 6.0f, (float)d / 5.0f, 845) - 0.5f) * 1.5f;
      const int st = (int)std::floor(a / 3.0f), k4 = (int)std::floor(a) - st * 3;
      float kk = 1.08f - 0.18f * std::min(1.0f, (float)(d - 1) / (float)std::max(1, fhF - 1));
      if (hashf(px / 4, st, 846) < 0.85f) { if (k4 == 2) kk *= 0.70f; else if (k4 == 0) kk *= 1.12f; }
      if (hashf(px, py, 847) < 0.07f) kk *= 0.88f;   // pebbles in the tread
      uint32_t r = mul(earth, kk);
      if (cover && k4 == 2) r = lerpc(r, C(112, 126, 164), 0.62f);
      else if (cover && k4 == 1) r = lerpc(r, C(150, 164, 200), 0.30f);
      const float ed = er + (vnoise(px / 3.0f, py / 3.0f, 849) - 0.5f) * 0.5f;
      if (ed < 0.70f) r = cover ? lerpc(r, C(120, 134, 170), 0.5f) : lerpc(mul(P.lo, 0.9f), outline, 0.3f);   // the cheek
      out = r; opaque = true;
    } else if (d <= fhF && turfPx && !stackLow && !wetFace) {
      // (M11 fixer r2) a hillside: the same band in turf (or snow), graded from the upper level down to the lower, a lit
      // brow on the contour (where a bare face has its lit lip), darkening down the rise (it faces the viewer, away
      // from the top-left light), grass strokes down the fall line and a soft toe instead of the face's crisp foot
      const float t = (float)(d - 1) / (float)std::max(1, fhF - 1);
      const uint32_t h = gradeAt((float)top - t * (float)std::max(1, top - own));
      float k = 0.82f - 0.14f * t;
      if (d == 1) k = cover ? 1.0f : 1.14f;
      else if (d == 2) k = cover ? 0.97f : 1.02f;
      else if (hashf(px, (py - d + (int)(hash2(px, 0, 1205) % 4)) >> 2, 1207) < 0.32f) k *= cover ? 0.94f : 0.86f;
      else if (!cover && hashf(px, py, 973) < 0.05f) k *= 1.08f;   // a lit blade of grass
      if (d == fhF) k *= 0.80f;            // the soft toe line
      else if (d == fhF - 1) k *= 0.92f;
      uint32_t r = mul(h, k);
      if (cover && k < 1.0f) r = lerpc(r, C(150, 170, 214), std::min(0.5f, (1.0f - k) * 2.4f));
      out = r; opaque = true;
    } else if (d <= fhF) {
      const int d0 = d;
      // depth into the whole wall: a stack's lower face continues under its ledge, its upper part stops at it
      float t;
      if (stackLow) t = (float)(above + d - 1) / (float)std::max(1, above + fh - 1);
      else if (ledgeAt) t = (float)(d - 1) / (float)std::max(1, d - 1 + ledgeAt + 15);
      else t = (float)(d - 1) / (float)std::max(1, fh - 1);
      RockPal Q = P;
      if (wetFace) {   // a fall's face: dark wet rock, sheened in streaks where the spray runs down it
        auto wet = [](uint32_t q) { return lerpc(mul(q, 0.58f), C(34, 52, 74), 0.30f); };
        Q = {wet(P.lip), wet(P.hi), wet(P.mid), wet(P.lo), wet(P.dark), P.cold};
      }
      uint32_t r = faceMat(FP, Q, t, d, px, py, cover && !wetFace);
      if (wetFace && ((px * 5 + (int)(hash2(px, 0, 979) % 7)) % 6) == 0) r = lerpc(r, C(150, 182, 204), 0.35f);
      // the lip: the plateau's cover hanging over it in ragged tufts with a dark undercut beneath (grass), a snow
      // cornice (snow), or a lit edge of bare rock (sand, rock, paving); a stack's ledge carries its own tufts or snow
      const uint32_t lipCover = regrade(top);
      const bool turf = turfGround(g) || (FP.style == 7);
      const int hang = 1 + (int)(hash2(px >> 1, (py - d) >> 4, 877) % 2) + (vnoise(px / 5.0f, (py - d) / 7.0f, 879) > 0.62f ? 1 : 0);
      if (wetFace) {
        // the notch in the lip where the river pours over: the stream's water runs 2-3 px down into the lip
        const bool pour = groundWater(m.at(px >> 4, (py - d - 2) >> 4));
        if (pour && d <= 2 + (int)(hash2(px, 0, 981) % 2)) r = mul(c, 1.10f);
        else if (d == 1) r = lerpc(Q.lip, C(120, 150, 170), 0.3f);
      } else if (cover) {   // a snow cornice: a bright lip, a soft blue underside, icicle drips
        const int cn = 2 + (int)(hash2(px / 3, (py - d) >> 4, 861) % 2);
        if (d == 1) r = C(250, 252, 255);
        else if (d <= cn) r = mul(lipCover, 0.93f);
        else if (d == cn + 1) r = lerpc(r, C(160, 176, 214), 0.5f);
        else if (d <= cn + 3 && (hash2(px, 0, 867) % 5) == 0) r = lerpc(r, C(214, 226, 244), 0.7f);
      } else if (turf && g != Ground::Road && g != Ground::Plaza) {
        if (d == 1) r = mul(lipCover, 1.06f);
        else if (d <= hang) r = mul(lipCover, 0.84f);
        else if (d == hang + 1) r = lerpc(mul(r, 0.62f), C(40, 30, 44), 0.25f);   // the undercut's shadow
      } else {
        if (d == 1) r = P.lip;
        else if (d == 2) r = lerpc(P.lip, P.hi, 0.5f);
      }
      if (!ledgeAt) {
        if (d >= fh - 2) r = mul(r, 0.80f);   // into the shade at the foot
        if (d == fh) r = outline;             // the crisp line where the face meets the ground
      } else if (ledgeAt == 1) {
        r = mul(r, 0.86f);                    // (the row against the ledge below)
      }
      out = r; opaque = true;
      d = d0;
    } else if (!rampHere && !pathPx) {
      // the face's cast shadow: a 4-px band on the ground at its foot, deepest against the face
      const int s = d - fhF - 1;   // 0 .. 3 (0 .. 6 / 9 on snow)
      if (cover) {
        // a cool blue shadow on the snow, deep against the face and fading out with a dithered edge
        const float q = 1.0f - (float)s / (float)shadowF;
        if (q > 0.25f || bayer(px, py) < q * 3.0f) out = shade(out, 1.0f - (0.18f + 0.22f * q) * keep, (0.34f + 0.24f * q) * keep);
      } else
        out = shade(out, 1.0f - (0.44f - s * 0.09f) * keep, (0.22f - s * 0.045f) * keep);
    }
  } else if (kind == RIM) {
    if (rampHere && !nsStair && ds <= RH) return rampPx(true, RH - ds);
    if (turfPx && ds <= RH) {
      // (M11 fixer r2) a hillside facing north: the rise's top seen from above, a crisp lit brow (so up reads from
      // down), a soft shade where it meets the lower ground instead of the face's dark outline
      const uint32_t top2 = regrade(up);
      if (ds <= RH - 3) out = top2;
      else if (ds <= RH - 1) out = cover ? (ds == RH - 1 ? C(252, 253, 255) : mul(top2, 1.0f)) : mul(top2, ds == RH - 1 ? 1.16f : 1.07f);
      else out = cover ? lerpc(regrade(own), C(170, 186, 222), 0.35f) : mul(regrade(own), 0.84f);
      opaque = !water;
    } else if (cover && ds <= RH) {
      // (M3, owner carry-over 6) a snowfield's north-facing drop: the plateau's snow ends in a bright cornice with a
      // cool underside, then a short band of its rock in shadow, snow caught on its ledges and a crisp foot
      const int lipTop = RH - 6;
      if (ds <= lipTop) { out = regrade(up); opaque = !water; }
      else if (ds == lipTop + 1) { out = C(252, 253, 255); opaque = true; }
      else if (ds == lipTop + 2) { out = lerpc(regrade(up), C(170, 186, 222), 0.45f); opaque = true; }
      else if (ds < RH) {
        const float t = 0.55f + 0.4f * (float)(ds - lipTop - 3) / 3.0f;
        uint32_t r = mul(faceMat(FP, P, t, ds, px, py, true), 0.74f);
        if (ds == RH - 1) r = mul(r, 0.85f);
        out = r; opaque = true;
      } else { out = outline; opaque = true; }
    } else if (ds <= RH - 3) { out = regrade(up); opaque = !water; }
    else if (ds <= RH - 1) {   // the rim, catching the light (2 px)
      const uint32_t top2 = regrade(up);
      out = ds == RH - 1 ? lerpc(P.lip, mul(top2, 1.3f), 0.45f) : mul(top2, 1.14f);
      opaque = true;
    } else if (ds == RH) { out = outline; opaque = true; }
    else if (!water) out = shade(out, 1.0f - (0.34f - (ds - RH - 1) * 0.12f) * keep, 0.12f * keep);   // the thin shadow at its foot
  } else if (kind == LIPW) {
    if (rampHere && !nsStair && dw <= SLR) return rampPx(false, dw - 1);
    if (dw <= SL) {   // an east-facing lip in shade: the plateau's brow, then its edge darkening to a crisp foot
      uint32_t r;
      if (turfPx) {   // (fixer r2) a hillside facing east: turf in shade, a soft foot
        const uint32_t u = regrade(F(px - dw - 1, py));
        r = dw == 1 ? u : dw == SL ? mul(regrade(own), 0.84f) : mul(u, 0.86f);
        if (cover && dw > 1) r = lerpc(r, C(150, 170, 214), 0.3f);
      } else if (dw == 1) r = cover ? mul(regrade(F(px - 1, py)), 0.88f) : lerpc(P.mid, P.hi, 0.3f);
      else if (dw == SL) r = outline;
      else r = mul(faceMat(FP, P, 0.6f + 0.3f * (float)(dw - 2) / (float)std::max(1, SL - 2), 6, px, py, false), 0.78f);
      out = r; opaque = true;
    } else if (!water) {
      const int s = dw - SL - 1;   // 0 .. 6 (0 .. 11 on snow): the shadow cast east, fading
      const float qs = 1.0f - (float)s / 12.0f;
      if (cover && (qs > 0.3f || bayer(px, py) < qs * 3.0f)) out = shade(out, 1.0f - (0.10f + 0.24f * qs) * keep, (0.26f + 0.26f * qs) * keep);
      else if (!cover) out = shade(out, 1.0f - (0.40f - s * 0.06f) * keep, (0.20f - s * 0.028f) * keep);
    }
  } else if (kind == LIPE) {
    if (rampHere && !nsStair && de <= SLR) return rampPx(false, SLR - de);
    if (de <= SLE) {   // a west-facing lip, lit from the top-left: bright at the brow, a crisp foot
      uint32_t r;
      if (turfPx) {   // (fixer r2) a hillside facing west: lit turf, a soft foot
        const uint32_t u = regrade(F(px + de + 1, py));
        r = de == 1 ? (cover ? C(252, 253, 255) : mul(u, 1.14f)) : de == SLE ? mul(regrade(own), 0.9f) : mul(u, cover ? 1.0f : 1.06f);
      } else if (de == 1) r = cover ? C(250, 252, 255) : P.lip;
      else if (de == SLE) r = lerpc(outline, P.lo, 0.3f);
      else r = faceMat(FP, P, 0.1f + 0.35f * (float)(de - 2) / (float)std::max(1, SLE - 2), 6, px, py, cover);
      out = r; opaque = true;
    } else if (!water) out = shade(out, 1.0f - (0.30f - (de - SLE - 1) * 0.10f) * keep, 0.10f * keep);   // contact shade at its foot
  }
  // (M2 fixer) the plateau's own edge above a side drop: lit where the land falls away to the west, shaded where it
  // falls away to the east, so the high side reads as high
  if (kind == NONE && !water && !rampHere && keep > 0.5f) {
    if (F(px - 1, py) < own || F(px - 2, py) < own) out = cover ? lerpc(out, C(255, 255, 255), 0.6f) : mul(out, F(px - 1, py) < own ? 1.18f : 1.08f);
    else if (F(px + 1, py) < own) out = mul(out, 0.86f);
  }
  // (M11) a stacked face (two levels or more) throws a soft shadow south-east as well: the light is top-left, so its
  // shadow lies 6 px to the east of the face's foot and runs up to 8 px further south than the contact band
  if (!opaque && !rampHere && !water && lc.stack) {
    int d6 = 0;
    for (int k = 1; k <= 30 && !d6; k++) if (F(px - 6, py - k) > own) d6 = k;
    if (d6 > 0 && F(px - 6, py - d6 - 10) >= own + 2) {
      const int foot = 16, far = foot + 12 - (int)(vnoise(px / 6.0f, 0.5f, 983) * 3.0f);
      if (d6 > foot && d6 <= far) out = shade(out, d6 >= far - 1 ? 0.90f : 0.82f, 0.08f);
    }
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
    if (b.home == 3) continue;   // (M7) the player's building site: stakes and a frame, no house yet to cast a shadow
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

void View::rebakeHomeHouses(const Game& g, const Map& m, uint64_t mapId) {
  if (m.kind != MapKind::Overworld) return;
  std::vector<HomeHouse> now;
  for (const Bldg& b : m.bldgs) {
    if (b.home != 1 && b.home != 3) continue;
    HomeHouse h;
    h.id = b.id; h.home = b.home; h.seed = b.seed;
    h.gx = b.r.x + (chunkEndless_ ? g.world.ox : 0); h.gy = b.r.y + (chunkEndless_ ? g.world.oy : 0);
    h.w = b.r.w; h.h = b.r.h;
    now.push_back(h);
  }
  auto same = [](const HomeHouse& a, const HomeHouse& b) {
    return a.id == b.id && a.home == b.home && a.seed == b.seed && a.gx == b.gx && a.gy == b.gy && a.w == b.w && a.h == b.h;
  };
  if (mapId != homeHousesMap_) {   // another map (or the first frame): its chunks are baked from what it holds now,
    homeHousesMap_ = mapId;        // except those baked before a house was stamped: rebake under every house once
    homeHouses_.clear();
  }
  std::vector<HomeHouse> changed;
  for (const HomeHouse& h : now) {
    bool seen = false;
    for (const HomeHouse& o : homeHouses_) seen = seen || same(h, o);
    if (!seen) changed.push_back(h);
  }
  for (const HomeHouse& o : homeHouses_) {
    bool still = false;
    for (const HomeHouse& h : now) still = still || same(h, o);
    if (!still) changed.push_back(o);
  }
  homeHouses_ = std::move(now);
  if (changed.empty()) return;
  // every chunk the house's footprint or its shadow (two tiles down-right, the sprite's pad) reaches, both its whole
  // and its partial (window-edge) key
  std::vector<uint64_t> keys;
  for (const HomeHouse& h : changed) {
    const int32_t ox = chunkEndless_ ? chunkOX_ * CH : 0, oy = chunkEndless_ ? chunkOY_ * CH : 0;
    const int cx0 = (int)std::floor((h.gx - 2 - ox) / (double)CH), cy0 = (int)std::floor((h.gy - 2 - oy) / (double)CH);
    const int cx1 = (int)std::floor((h.gx + h.w + 3 - ox) / (double)CH), cy1 = (int)std::floor((h.gy + h.h + 3 - oy) / (double)CH);
    for (int cy = cy0; cy <= cy1; cy++)
      for (int cx = cx0; cx <= cx1; cx++) {
        if (cx < 0 || cy < 0 || cx * CH >= m.w || cy * CH >= m.h) continue;
        const uint64_t k = chunkKeyFor(m, mapId, cx, cy) & ~1ull;
        keys.push_back(k);
        keys.push_back(k | 1ull);
      }
  }
  auto hit = [&](uint64_t k) { return std::find(keys.begin(), keys.end(), k) != keys.end(); };
  // a baked chunk stays on screen until its new bake is ready (chunkTex swaps it in: no frame waits on it); bakes
  // queued or finished from the old snapshot are dropped (prefetch / chunkTex queue them again)
  for (Chunk& ch : chunks_) if (hit(ch.key)) ch.stale = true;
  std::lock_guard<std::mutex> lk(mu_);
  for (size_t i = 0; i < jobs_.size();) if (hit(jobs_[i].key)) jobs_.erase(jobs_.begin() + (std::ptrdiff_t)i); else i++;
  for (size_t i = 0; i < done_.size();) if (hit(done_[i].key)) done_.erase(done_.begin() + (std::ptrdiff_t)i); else i++;
  pending_.erase(std::remove_if(pending_.begin(), pending_.end(), hit), pending_.end());
  if (incrOn_ && hit(incrJob_.key)) { incrOn_ = false; incrJob_.map.reset(); }
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
  if (!m.urb.empty()) sub->urb.assign(n, 0);       // (M7b) the urban fabric (kerbs, sidewalks, squares, parks)
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
    if (!m.urb.empty()) std::copy_n(m.urb.begin() + si, len, sub->urb.begin() + di);
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

void View::settleBakes() {
  if (singleThread()) return;
  for (int i = 0; i < 5000; i++) {
    {
      std::lock_guard<std::mutex> lk(mu_);
      bool busy = false;
      for (uint64_t k : pending_) {   // queued, or being painted (pending, not finished)
        bool finished = false;
        for (const BakeDone& d : done_) if (d.key == k) { finished = true; break; }
        if (!finished) { busy = true; break; }
      }
      if (!busy) return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
  int c1x = (int)std::floor((cam.x + viewW() + 256) / 512), c1y = (int)std::floor((cam.y + viewH() + 256) / 512);   // (P2a: the world view)
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
    for (auto& ch : chunks_) if (ch.key == done_[i].key && !ch.stale) stale = true;   // (a stale chunk awaits it)
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
    if (ch.key == key) {
      ch.used = t_;
      if (ch.stale && witness) {   // (Witness) the new bake now, not whenever a baker gets to it
        {
          std::lock_guard<std::mutex> lk(mu_);
          for (size_t i = 0; i < jobs_.size();) if (jobs_[i].key == key) jobs_.erase(jobs_.begin() + (std::ptrdiff_t)i); else i++;
          for (size_t i = 0; i < done_.size();) if (done_[i].key == key) done_.erase(done_.begin() + (std::ptrdiff_t)i); else i++;
          pending_.erase(std::remove(pending_.begin(), pending_.end(), key), pending_.end());
        }
        Canvas c;
        bakeChunk(makeJob(m, mapId, cx, cy), c);
        pix_->destroy(ch.tex);
        ch.tex = pix_->bake(c);
        ch.stale = false;
        return ch.tex;
      }
      if (ch.stale) {   // (M7) to be baked again (a player's house came or changed): swap the new bake in when ready
        std::unique_lock<std::mutex> lk(mu_);
        for (size_t i = 0; i < done_.size(); i++)
          if (done_[i].key == key) {
            BakeDone d = std::move(done_[i]);
            done_.erase(done_.begin() + (std::ptrdiff_t)i);
            pending_.erase(std::remove(pending_.begin(), pending_.end(), key), pending_.end());
            lk.unlock();
            pix_->destroy(ch.tex);
            ch.tex = pix_->bake(d.c);
            ch.stale = false;
            return ch.tex;
          }
        if (std::find(pending_.begin(), pending_.end(), key) == pending_.end()) {
          pending_.push_back(key);
          jobs_.push_back(makeJob(m, mapId, cx, cy));
          lk.unlock();
          cv_.notify_all();
        }
      }
      return ch.tex;
    }
  auto store = [&](Canvas& c) -> Tex {
    Chunk* slot = nullptr;
    // enough for the visible chunks of a wide phone screen and the ring prefetched around them
    const size_t cap = (size_t)std::max(24, 2 * (viewW() / 512 + 3) * (viewH() / 512 + 3));
    if (chunks_.size() < cap) { chunks_.push_back(Chunk()); slot = &chunks_.back(); }
    else {
      slot = &chunks_[0];
      for (auto& ch : chunks_) if (ch.used < slot->used) slot = &ch;
      pix_->destroy(slot->tex);
    }
    slot->tex = pix_->bake(c);
    slot->key = key; slot->used = t_; slot->stale = false;
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
  if (witness) {
    // (Witness) nothing waits on a baker, a paint thread or a budget: the camera sits on the arrival, the draw bakes and
    // paints whatever it shows inline (arriving_ stays off), and the sim ends the arrival after a fixed number of steps
    // (Game::syncStreams), never this view (so travel.viewed is left alone)
    arriving_ = false;
    arrival_ = Arrival();
    snap(g);
    return;
  }
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
  const int VW = viewW(), VH = viewH();   // (P2a) the world view (this runs in update, outside the world layer)
  const int v1x = (int)std::floor((cam.x + VW) / 512), v1y = (int)std::floor((cam.y + VH) / 512);
  // the chunks the fade waits for: desktop, the view and a ring round it (the background bakers are quick); without
  // threads only what the first frames after the fade can show (the view plus 64 px); the ring is baked afterwards by
  // the per-frame pump like any walk's prefetch
  const int n0x = st ? (int)std::floor((cam.x - 64) / 512) : v0x - 1, n0y = st ? (int)std::floor((cam.y - 64) / 512) : v0y - 1;
  const int n1x = st ? (int)std::floor((cam.x + VW + 64) / 512) : v1x + 1, n1y = st ? (int)std::floor((cam.y + VH + 64) / 512) : v1y + 1;
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
    bldgsIn(g, m, std::min(cam.x - 32, pp.x - 720), std::min(cam.y - 32, pp.y - 520), std::max(cam.x + VW + 32, pp.x + 720),
            std::max(cam.y + VH + 32, pp.y + 520), vis);
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
      const int tx1 = std::min(m.w, tx0 + VW / 16 + 7), ty1 = std::min(m.h, ty0 + VH / 16 + 8);
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
    if (!a.human || a.p.x < cam.x - 96 || a.p.x > cam.x + VW + 96 || a.p.y < cam.y - 96 || a.p.y > cam.y + VH + 128) continue;
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

// ---------------------------------------------------------------- (M7b CITYART) test only: close shots of the fabric
//   gotourb <class|corner|kerb|grate|terrace> [n]   stand 2 tiles south of the n-th nearest (default 0) tile of a fabric
//                                                   class (avenue .. quay), an outer kerb corner (a walk with the
//                                                   carriageway south and east of it), a south kerb (a walk with the road
//                                                   south of it), a gutter grate (a gutter beside a square) or a row house;
//                                                   none there: the hero stays put (logged, not a failure)
namespace {
bool cmdGotoUrb(ScriptCtx& c) {
  Game& g = c.game;
  const Map& m = g.world.over;
  if (g.inside || m.urb.empty()) { c.fail("gotourb: no urban fabric here"); return true; }
  const std::string what = c.arg(1);
  const int want = c.arg(2).empty() ? 0 : std::max(0, std::atoi(c.arg(2).c_str()));
  auto cls = [&](int x, int y) { return ew::urbClass(m.urbAt(x, y)); };
  auto low = [&](int x, int y) { const ew::Urb u = cls(x, y); return ew::urbCarriage(u) || u == ew::Urb::Gutter; };
  int kind = -1;
  for (int k = 1; k < (int)ew::Urb::COUNT; k++) if (what == ew::urbName((ew::Urb)k)) kind = k;
  auto match = [&](int x, int y) {
    if (what == "terrace") return false;
    const ew::Urb u = cls(x, y);
    if (what == "corner") return u == ew::Urb::Sidewalk && low(x, y + 1) && (low(x + 1, y) || low(x - 1, y));
    if (what == "kerb") return u == ew::Urb::Sidewalk && low(x, y + 1) && cls(x - 1, y) == ew::Urb::Sidewalk && cls(x + 1, y) == ew::Urb::Sidewalk;
    if (what == "grate") {
      if (u != ew::Urb::Gutter) return false;
      for (int k = 0; k < 4; k++) { const ew::Urb q = cls(x + (k == 0) - (k == 1), y + (k == 2) - (k == 3)); if (q == ew::Urb::Square || q == ew::Urb::Court || q == ew::Urb::Close) return true; }
      return false;
    }
    return kind > 0 && (int)u == kind;
  };
  const int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
  std::vector<std::pair<int, std::pair<int, int>>> hits;
  if (what == "terrace") {
    for (const Bldg& b : m.bldgs) if (b.row) hits.push_back({(b.r.x - px) * (b.r.x - px) + (b.r.y - py) * (b.r.y - py), {b.r.x + b.r.w / 2, b.r.y + b.r.h}});
  } else {
    for (int y = std::max(0, py - 90); y < std::min(m.h, py + 90); y++)
      for (int x = std::max(0, px - 90); x < std::min(m.w, px + 90); x++)
        if (match(x, y)) hits.push_back({(x - px) * (x - px) + (y - py) * (y - py), {x, y}});
  }
  if ((int)hits.size() <= want) { std::printf("gotourb: no %s within 90 tiles (the hero stays)\n", what.c_str()); return true; }   // (not a failure: the plan may have none)
  std::nth_element(hits.begin(), hits.begin() + want, hits.end());
  std::sort(hits.begin(), hits.begin() + want + 1);
  const int x = hits[(size_t)want].second.first, y = hits[(size_t)want].second.second;
  g.pl().p = Vec2(x * (float)TILE + 8, (y + 2) * (float)TILE + 8);
  c.view.snap(g);
  std::printf("gotourb: %s %d at local %d,%d\n", what.c_str(), want, x, y);
  return true;
}
EMB_SCRIPT_CMD("gotourb", "gotourb <class|corner|kerb|grate|terrace> [n]: test only: stand by the n-th nearest piece of the city fabric (M7b CITYART)", cmdGotoUrb);
}  // namespace

// (M7b CITYART, tests) the kerb's face, measured on the painted ground: for each walk tile with a carriageway south of
// it (and walk on both sides, a straight run), the face rows (ly 13..14) are darker than the kerb's top and lit arris
// above them (ly 11..12)
int View::urbKerbCheck(const Game& g, int r, int& faces) {
  faces = 0;
  const Map& m = g.world.over;
  if (g.inside || m.urb.empty()) return 0;
  TMap tm;
  tm.m = &m; tm.ox = g.world.ox; tm.oy = g.world.oy; tm.kind = m.kind;
  auto cls = [&](int x, int y) { return ew::urbClass(m.urbAt(x, y)); };
  const int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
  int bad = 0;
  for (int y = std::max(1, py - r); y < std::min(m.h - 1, py + r); y++)
    for (int x = std::max(1, px - r); x < std::min(m.w - 1, px + r); x++) {
      if (cls(x, y) != ew::Urb::Sidewalk || !urbSurface(m.at(x, y)) || m.bldgAt[(size_t)y * m.w + x] >= 0) continue;
      const ew::Urb s = cls(x, y + 1);
      if (!(ew::urbCarriage(s) || s == ew::Urb::Gutter)) continue;
      if (cls(x - 1, y) != ew::Urb::Sidewalk || cls(x + 1, y) != ew::Urb::Sidewalk) continue;
      // (M7c integration) a straight run: the carriageway south of the neighbours too (where the walk steps along a
      // tilted street the kerb slants across the tile, M7c ART stairEdge, and its face is not on these rows)
      // (two tiles either side: the slant's own tile and the one it eases into)
      {
        bool straightRun = true;
        for (int k = -2; k <= 2 && straightRun; k++) {
          const ew::Urb q = cls(x + k, y + 1);
          if (!(ew::urbCarriage(q) || q == ew::Urb::Gutter) || cls(x + k, y) != ew::Urb::Sidewalk) straightRun = false;
        }
        if (!straightRun) continue;
      }
      const int gx = (x + tm.ox) * 16, gy = (y + tm.oy) * 16;
      float top = 0, face = 0;
      for (int lx = 3; lx < 13; lx++) {
        for (int ly = 11; ly <= 12; ly++) { const uint32_t c = groundPixel(tm, gx + lx, gy + ly); top += (c & 255) * 0.3f + ((c >> 8) & 255) * 0.55f + ((c >> 16) & 255) * 0.15f; }
        for (int ly = 13; ly <= 14; ly++) { const uint32_t c = groundPixel(tm, gx + lx, gy + ly); face += (c & 255) * 0.3f + ((c >> 8) & 255) * 0.55f + ((c >> 16) & 255) * 0.15f; }
      }
      top /= 20.0f; face /= 20.0f;
      faces++;
      if (face > top * 0.80f) {
        if (bad < 4) std::printf("urbKerbCheck: no face at global %d,%d (top %.0f face %.0f; south %d)\n", x + tm.ox, y + tm.oy, top, face, (int)s);
        bad++;
      }
    }
  return bad;
}

namespace {
bool expKerbs(ScriptCtx& c) {
  int faces = 0;
  const int bad = c.view.urbKerbCheck(c.game, 40, faces);
  std::printf("expect kerbs: %d south kerbs within 40 tiles, %d without a face\n", faces, bad);
  if (bad || (!faces && c.arg(2) != "any")) c.fail("expect kerbs: " + std::to_string(bad) + " of " + std::to_string(faces) + " south kerbs show no face");
  return true;
}
EMB_SCRIPT_CMD("expect:kerbs", "expect kerbs [any]: every straight south kerb within 40 tiles shows its face (darker than the walk); any: none is no failure (M7b CITYART)", expKerbs);
}  // namespace

// ---------------------------------------------------------------- (M7c ART) test only: a street-network stage
//   urbstage build [far]      in a city: keep its look (the nearest row house's style, the paving), go to open land
//                             `far` (default 420) tiles south of here and lay a synthetic planned quarter round the
//                             hero: two crossing avenues with central drains, block streets with kerbed walks crossing
//                             them, a mews lane and a lane meeting the walks in T junctions, a 1:1 diagonal and a 1:2
//                             tilted street with their walks, a framed square, a river with quays and bridges, row
//                             houses fronting the streets with corner units (ROW_SIDE_W / _E) and backs on a street
//                             (ROW_BACK). Every junction kind of the M7c network on one stage, judged at street zoom.
//   urbstage go <spot>        stand at a spot of the stage: cross, street, tee, lane, corner, mews, diag, tilt,
//                             square, bridge, quay, back, east, yard
namespace {
struct UrbStage {
  bool have = false, built = false;
  Bldg tmpl;
  int pm = 0;
  Biome biome = Biome::Plains;
  int x0 = 0, y0 = 0;          // the stage's local origin in the window it was built in
  int32_t gox = 0, goy = 0;    // the window's global origin then
};
UrbStage g_stage;
constexpr int kStW = 60, kStH = 44;
enum StK : uint8_t { ST_OPEN, ST_WATER, ST_BRIDGE, ST_CARR, ST_WALK, ST_GUT, ST_SQUARE, ST_PARK, ST_GARDEN, ST_QUAY, ST_HOUSE };
struct StT { StK k = ST_OPEN; Urb u = Urb::None; int up = 3; };

bool cmdUrbStage(ScriptCtx& c) {
  Game& g = c.game;
  const std::string sub = c.arg(1);
  if (sub == "go") {
    if (!g_stage.built) { c.fail("urbstage go: no stage (urbstage build first)"); return true; }
    static const struct { const char* n; int x, y; } spots[] = {
        {"cross", 29, 24}, {"street", 12, 24}, {"tee", 38, 25}, {"lane", 38, 28}, {"corner", 14, 11}, {"mews", 20, 16},
        {"diag", 19, 28}, {"tilt", 53, 27}, {"square", 38, 15}, {"bridge", 4, 23}, {"quay", 6, 12}, {"back", 20, 37},
        {"east", 47, 21}, {"yard", 54, 15}};
    const std::string w = c.arg(2);
    for (const auto& s : spots)
      if (w == s.n) {
        const int lx = g_stage.x0 + s.x + (g_stage.gox - g.world.ox), ly = g_stage.y0 + s.y + (g_stage.goy - g.world.oy);
        g.pl().p = Vec2(lx * (float)TILE + 8, ly * (float)TILE + 8);
        c.view.snap(g);
        std::printf("urbstage go %s: local %d,%d\n", s.n, lx, ly);
        return true;
      }
    c.fail("urbstage go: unknown spot " + w);
    return true;
  }
  if (sub != "build") { c.fail("urbstage build [far] | urbstage go <spot>"); return true; }
  if (g.inside) { c.fail("urbstage: indoors"); return true; }
  {
    // the city's look: its nearest row house (else any city house) and the paving of its walks
    Map& m = g.world.over;
    const int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
    int best = -1, bd = 1 << 30;
    for (int i = 0; i < (int)m.bldgs.size(); i++) {
      const Bldg& b = m.bldgs[(size_t)i];
      if (b.urban < 2 || b.type != art::Building::House || b.home) continue;
      const int d = std::abs(b.r.x - px) + std::abs(b.r.y - py) + (b.row ? 0 : 400);
      if (d < bd) { bd = d; best = i; }
    }
    if (best < 0) { c.fail("urbstage build: no city house here"); return true; }
    g_stage.tmpl = m.bldgs[(size_t)best];
    int pm = -1;
    for (int r = 0; r < 40 && pm < 0; r++)
      for (int oy = -r; oy <= r && pm < 0; oy++)
        for (int ox = -r; ox <= r; ox++) {
          const int x = g_stage.tmpl.r.x + ox, y = g_stage.tmpl.r.y + oy;
          const uint8_t bl = m.blendAt(x, y);
          if ((bl >> 4) == (Map::PAVE_MARK >> 4) && ew::urbClass(m.urbAt(x, y)) != Urb::None) { pm = bl & 15; break; }
        }
    g_stage.pm = std::max(0, pm);
    g_stage.biome = m.biomeAt(g_stage.tmpl.r.x, g_stage.tmpl.r.y);
    g_stage.have = true;
  }
  const int far = c.arg(2).empty() ? 420 : std::max(200, std::atoi(c.arg(2).c_str()));
  g.teleportGlobal(g.world.ox + (int)std::floor(g.pl().p.x / TILE), g.world.oy + (int)std::floor(g.pl().p.y / TILE) + far);
  g.mode = Mode::Play;
  Map& m = g.world.over;
  const int hx = (int)std::floor(g.pl().p.x / TILE), hy = (int)std::floor(g.pl().p.y / TILE);
  const int X0 = hx - 30, Y0 = hy - 22;
  g_stage.x0 = X0; g_stage.y0 = Y0; g_stage.gox = g.world.ox; g_stage.goy = g.world.oy;
  // ---- the plan, in stage tiles
  using K = StK;
  using T = StT;
  std::vector<T> P((size_t)kStW * kStH);
  auto at = [&](int x, int y) -> T* { return (x >= 0 && y >= 0 && x < kStW && y < kStH) ? &P[(size_t)y * kStW + x] : nullptr; };
  auto put = [&](int x, int y, K k, Urb u, int up) { if (T* t = at(x, y)) { t->k = k; t->u = u; t->up = up; } };
  auto rect = [&](int ax, int ay, int bx, int by, K k, Urb u, int up) { for (int y = ay; y <= by; y++) for (int x = ax; x <= bx; x++) put(x, y, k, u, up); };
  auto carr = [&](int x, int y) { const T* t = at(x, y); return t && (t->k == ST_CARR || t->k == ST_GUT); };
  // walks first (the carriageways overwrite them where they cross), then the carriageways
  rect(0, 18, kStW - 1, 18, ST_WALK, Urb::Sidewalk, 3); rect(0, 22, kStW - 1, 22, ST_WALK, Urb::Sidewalk, 3);   // the E-W avenue's walks
  rect(27, 0, 27, kStH - 1, ST_WALK, Urb::Sidewalk, 3); rect(31, 0, 31, kStH - 1, ST_WALK, Urb::Sidewalk, 3);   // the N-S avenue's
  rect(0, 6, kStW - 1, 6, ST_WALK, Urb::Sidewalk, 2); rect(0, 9, kStW - 1, 9, ST_WALK, Urb::Sidewalk, 2);       // block streets
  rect(0, 32, kStW - 1, 32, ST_WALK, Urb::Sidewalk, 1); rect(0, 35, kStW - 1, 35, ST_WALK, Urb::Sidewalk, 1);
  rect(10, 0, 10, kStH - 1, ST_WALK, Urb::Sidewalk, 2); rect(13, 0, 13, kStH - 1, ST_WALK, Urb::Sidewalk, 2);
  rect(45, 0, 45, kStH - 1, ST_WALK, Urb::Sidewalk, 2); rect(48, 0, 48, kStH - 1, ST_WALK, Urb::Sidewalk, 2);
  rect(0, 19, kStW - 1, 21, ST_CARR, Urb::Avenue, 3);
  rect(28, 0, 30, kStH - 1, ST_CARR, Urb::Avenue, 3); rect(29, 0, 29, kStH - 1, ST_GUT, Urb::Gutter, 3);
  rect(0, 20, kStW - 1, 20, ST_GUT, Urb::Gutter, 3);
  rect(0, 7, kStW - 1, 8, ST_CARR, Urb::Street, 2); rect(0, 33, kStW - 1, 34, ST_CARR, Urb::Street, 1);
  rect(11, 0, 12, kStH - 1, ST_CARR, Urb::Street, 2); rect(46, 0, 47, kStH - 1, ST_CARR, Urb::Street, 2);
  rect(28, 19, 30, 21, ST_CARR, Urb::Avenue, 3); rect(29, 19, 29, 21, ST_GUT, Urb::Gutter, 3); rect(28, 20, 30, 20, ST_GUT, Urb::Gutter, 3);
  rect(14, 14, 26, 14, ST_CARR, Urb::Lane, 2);    // a mews lane between two rows of backs
  rect(38, 23, 38, 31, ST_CARR, Urb::Lane, 1);    // a lane off the avenue to the south street
  rect(32, 10, 44, 17, ST_SQUARE, Urb::Square, 3);
  // a 1:1 diagonal street (two tiles wide) across the block x 14..26, y 23..31, kerbed walks either side
  rect(14, 23, 26, 31, ST_PARK, Urb::Park, 3);
  for (int k = 0; k <= 8; k++) { put(14 + k, 31 - k, ST_CARR, Urb::Street, 2); put(15 + k, 31 - k, ST_CARR, Urb::Street, 2); }
  // a 1:2 tilted street across the block x 49..59, y 23..31
  rect(49, 23, kStW - 1, 31, ST_GARDEN, Urb::Garden, 2);
  for (int k = 0; k <= 10; k++) { const int y = 29 - k / 2; put(49 + k, y, ST_CARR, Urb::Street, 2); put(49 + k, y + 1, ST_CARR, Urb::Street, 2); }
  for (int y = 23; y <= 31; y++)
    for (int x = 14; x < kStW; x++) {
      if (x > 26 && x < 49) continue;
      T* t = at(x, y);
      if (!t || t->k == ST_CARR) continue;
      bool nbr = false;
      for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++) {
          if (!(ox || oy) || !carr(x + ox, y + oy)) continue;
          const T* q = at(x + ox, y + oy);
          if (q && q->u == Urb::Street) nbr = true;
        }
      if (nbr) { t->k = ST_WALK; t->u = Urb::Sidewalk; t->up = 2; }
    }
  // the river: water cols 3..5, quays on both banks, bridges where the streets cross
  for (int y = 0; y < kStH; y++) {
    const bool span = (y >= 6 && y <= 9) || (y >= 18 && y <= 22) || (y >= 32 && y <= 35);
    for (int x = 3; x <= 5; x++) put(x, y, span ? ST_BRIDGE : ST_WATER, Urb::None, 3);
    if (!span) { put(2, y, ST_QUAY, Urb::Quay, 2); put(6, y, ST_QUAY, Urb::Quay, 2); }
  }
  rect(7, 10, 9, 13, ST_GARDEN, Urb::Garden, 2);
  rect(7, 23, 9, 31, ST_GARDEN, Urb::Garden, 2);
  // ---- write the tiles
  const int lvl = m.heightAt(hx, hy);
  if (m.urb.empty()) m.urb.assign((size_t)m.w * m.h, 0);
  for (int y = -6; y < kStH + 6; y++)
    for (int x = -6; x < kStW + 6; x++) {
      const int mx = X0 + x, my = Y0 + y;
      if (!m.in(mx, my)) continue;
      const size_t i = (size_t)my * m.w + mx;
      const T* t = at(x, y);
      Ground gr = Ground::Grass;
      uint8_t ub = 0;
      if (t) {
        switch (t->k) {
          case ST_WATER: gr = Ground::Water; break;
          case ST_BRIDGE: gr = Ground::Bridge; break;
          case ST_CARR: case ST_GUT: gr = Ground::Road; break;
          case ST_WALK: case ST_SQUARE: case ST_QUAY: gr = Ground::Plaza; break;
          default: gr = Ground::Grass; break;
        }
        if (t->u != Urb::None) ub = ew::urbPack(t->u, t->up, ew::Quarter::Centre);
      }
      m.ground[i] = (uint8_t)gr;
      if (!m.prop.empty()) m.prop[i] = 0;
      if (!m.deco.empty()) m.deco[i] = 0;
      if (!m.wall.empty()) m.wall[i] = 0;
      if (!m.biome.empty()) m.biome[i] = (uint8_t)g_stage.biome;
      if (!m.eco.empty()) m.eco[i] = (uint8_t)ecoOfFamily(g_stage.biome);
      if (!m.ecoNb.empty()) m.ecoNb[i] = (uint8_t)ecoOfFamily(g_stage.biome);
      if (!m.height.empty()) m.height[i] = (uint8_t)lvl;
      if (!m.blend.empty()) m.blend[i] = t ? (uint8_t)(Map::PAVE_MARK | g_stage.pm) : 0;
      m.urb[i] = ub;
    }
  // buildings standing on the stage (a farm of the land) are moved off the map
  for (Bldg& b : m.bldgs)
    if (b.r.x + b.r.w > X0 - 6 && b.r.x < X0 + kStW + 6 && b.r.y + b.r.h > Y0 - 6 && b.r.y < Y0 + kStH + 6) b.r.x = -100000;
  // ---- the row houses: runs along a front, widths 2..3, the party walls between, the street faces at the ends / back
  auto streetish = [&](int x, int y) { const T* t = at(x, y); return t && (t->k == ST_CARR || t->k == ST_GUT || t->k == ST_WALK || t->k == ST_SQUARE || t->k == ST_QUAY); };
  int nb = 0;
  auto run = [&](int ax, int bx, int y0, int h, bool yard, uint32_t salt) {
    std::vector<int> ws;
    int left = bx - ax + 1;
    uint32_t hs = hash2((int)salt, 77, 9101);
    while (left > 0) {
      int w = 2 + (int)(hs & 1u);
      hs = hash2((int)hs, 0x51, 9103);
      if (left - w == 1) w = left >= 4 ? 2 : 3;
      if (w > left) w = left;
      ws.push_back(w);
      left -= w;
    }
    const uint8_t storeys = (uint8_t)(2 + hash2((int)salt, 3, 9105) % 2u);
    int x = ax;
    for (size_t k = 0; k < ws.size(); k++) {
      const int w = ws[k];
      Bldg b = g_stage.tmpl;
      b.id = 0; b.site = -1; b.home = 0; b.charred = 0;
      b.open = Bldg::Open();
      b.r = IRect{X0 + x, Y0 + y0, w, h};
      b.seed = hash2((int)salt * 131, (int)k * 977 + 5, 9107);
      b.storeys = storeys;
      b.type = (k == 1 && ws.size() >= 4) ? art::Building::Shop : art::Building::House;
      b.variant = (uint8_t)(1 + (k * 3 + salt) % 7);
      uint8_t row = 0;
      if (k > 0) row |= bld::ROW_W;
      if (k + 1 < ws.size()) row |= bld::ROW_E;
      if (k == 0 && streetish(x - 1, y0 + h - 1)) row |= bld::ROW_SIDE_W;
      if (k + 1 == ws.size() && streetish(x + w, y0 + h - 1)) row |= bld::ROW_SIDE_E;
      if (streetish(x + w / 2, y0 - 1)) row |= bld::ROW_BACK;
      if (yard && !(row & bld::ROW_BACK)) row |= bld::ROW_YARD;
      b.row = row;
      for (int yy = y0; yy < y0 + h; yy++) for (int xx = x; xx < x + w; xx++) put(xx, yy, ST_HOUSE, Urb::None, 0);
      m.bldgs.push_back(b);
      nb++;
      x += w;
    }
  };
  run(7, 9, 2, 4, false, 1); run(14, 26, 2, 4, false, 2); run(32, 44, 2, 4, false, 3); run(49, kStW - 1, 2, 4, false, 4);
  run(14, 26, 10, 4, false, 5); run(14, 26, 15, 3, false, 6);        // the mews: backs on the block street and the lane
  run(49, kStW - 1, 13, 5, true, 7);                                  // yards behind
  run(7, 9, 14, 4, false, 8);                                         // a corner house between the garden and a street
  run(32, 37, 23, 3, false, 9); run(39, 44, 23, 3, false, 10);        // backs on the avenue's walk, the lane beside
  run(32, 37, 27, 5, false, 11); run(39, 44, 27, 5, false, 12);
  run(14, 26, 36, 4, false, 13); run(32, 44, 36, 4, false, 14); run(49, kStW - 1, 36, 4, false, 15);   // backs on the south street
  m.rebuildSolid();
  g_stage.built = true;
  g.pl().p = Vec2((X0 + 29) * (float)TILE + 8, (Y0 + 24) * (float)TILE + 8);
  c.view.snap(g);
  std::printf("urbstage: built at local %d,%d (global %d,%d), paving %d, %d row houses\n", X0, Y0, X0 + g.world.ox, Y0 + g.world.oy, g_stage.pm, nb);
  return true;
}
EMB_SCRIPT_CMD("urbstage", "urbstage build [far] | go <spot>: test only: a synthetic M7c street network with row houses (M7c ART)", cmdUrbStage);

// ---------------------------------------------------------------- (M7c ART) test only: the city wall's places
//   wallspot <vertex|diag|gate|water|any> [n]   stand the hero by the nth (nearest first) place of that kind on the
//                                               city wall within 90 tiles: a vertex (the run turns by 30 degrees or
//                                               more: a polygon's corner, a straight run onto a diagonal), a diagonal
//                                               run, a gate whose curtains leave it on the slant, the wall meeting water
// The run's direction at a wall tile is read from the wall tiles on the square ring 4 tiles out (two clusters: the run
// arriving and leaving).
// tilescan <dx0> <dy0> <dx1> <dy1>: debug: print what lies on the tiles of that box round the hero (ground, prop, deco,
// wall, building) (M7c fixer r3)
bool cmdTileScan(ScriptCtx& c) {
  Game& g = c.game;
  const Map& m = g.world.over;
  const int hx = (int)(g.pl().p.x / TILE), hy = (int)(g.pl().p.y / TILE);
  const int x0 = hx + std::atoi(c.arg(1).c_str()), y0 = hy + std::atoi(c.arg(2).c_str()), x1 = hx + std::atoi(c.arg(3).c_str()), y1 = hy + std::atoi(c.arg(4).c_str());
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      if (!m.in(x, y)) continue;
      const size_t i = (size_t)y * m.w + x;
      const int pr = m.prop.empty() ? 0 : m.prop[i], dc = m.deco.empty() ? 0 : m.deco[i], wl = m.wall.empty() ? 0 : m.wall[i];
      const int bi = m.bldgAt.empty() ? -1 : m.bldgAt[i];
      if (!pr && !dc && !wl && bi < 0) continue;
      std::printf("tilescan %d,%d: ground %d prop %d deco %d wall %d bldg %d", x, y, (int)m.ground[i], pr - 1, dc, wl, bi);
      if (bi >= 0 && bi < (int)m.bldgs.size()) { const Bldg& b = m.bldgs[(size_t)bi]; std::printf(" (type %d rect %d,%d %dx%d home %d)", (int)b.type, b.r.x, b.r.y, b.r.w, b.r.h, (int)b.home); }
      std::printf("\n");
    }
  if (c.arg(5) == "h")
    for (int y = y0; y <= y1; y++) {
      std::printf("tilescan h %d:", y);
      for (int x = x0; x <= x1; x++) std::printf(" %2d", (int)m.heightBits(x, y));
      std::printf("\n");
    }
  for (size_t k = 0; k < m.bldgs.size(); k++) {
    const Bldg& b = m.bldgs[k];
    if (b.r.x > x1 || b.r.y > y1 || b.r.x + b.r.w < x0 || b.r.y + b.r.h < y0) continue;
    std::printf("tilescan bldg %zu: type %d rect %d,%d %dx%d home %d site %d\n", k, (int)b.type, b.r.x, b.r.y, b.r.w, b.r.h, (int)b.home, b.site);
  }
  return true;
}
EMB_SCRIPT_CMD("tilescan", "tilescan <dx0> <dy0> <dx1> <dy1>: debug: what lies on the tiles of that box round the hero (M7c fixer r3)", cmdTileScan);
bool cmdWallSpot(ScriptCtx& c) {
  Game& g = c.game;
  const Map& m = g.world.over;
  const std::string what = c.arg(1);
  const int nth = c.arg(2).empty() ? 0 : std::atoi(c.arg(2).c_str());
  const int hx = (int)(g.pl().p.x / TILE), hy = (int)(g.pl().p.y / TILE);
  auto wallAt = [&](int x, int y) { return m.in(x, y) && !m.wall.empty() && m.wall[(size_t)y * m.w + x] != 0; };
  // the two directions of the run at (x, y): false where the ring does not show exactly two runs
  auto dirs = [&](int x, int y, float& ax, float& ay, float& bx, float& by) {
    constexpr int R = 4;
    std::vector<std::pair<int, int>> ring;
    for (int i = -R; i < R; i++) ring.push_back({i, -R});
    for (int j = -R; j < R; j++) ring.push_back({R, j});
    for (int i = R; i > -R; i--) ring.push_back({i, R});
    for (int j = R; j > -R; j--) ring.push_back({-R, j});
    const int n = (int)ring.size();
    int start = -1;
    for (int k = 0; k < n; k++) if (!wallAt(x + ring[(size_t)k].first, y + ring[(size_t)k].second)) { start = k; break; }
    if (start < 0) return false;
    float sx[3] = {0, 0, 0}, sy[3] = {0, 0, 0};
    int cnt[3] = {0, 0, 0}, nc = 0;
    bool in = false;
    for (int k = 1; k <= n; k++) {
      const auto& q = ring[(size_t)((start + k) % n)];
      const bool w = wallAt(x + q.first, y + q.second);
      if (w && !in) { if (nc == 3) return false; nc++; }
      in = w;
      if (w) { sx[nc - 1] += (float)q.first; sy[nc - 1] += (float)q.second; cnt[nc - 1]++; }
    }
    if (nc != 2) return false;
    ax = sx[0] / cnt[0]; ay = sy[0] / cnt[0]; bx = sx[1] / cnt[1]; by = sy[1] / cnt[1];
    return true;
  };
  auto turnDeg = [](float ax, float ay, float bx, float by) {
    const float d = (ax * bx + ay * by) / std::max(1e-3f, std::sqrt((ax * ax + ay * ay) * (bx * bx + by * by)));
    return 180.0f - std::acos(std::clamp(d, -1.0f, 1.0f)) * 57.2958f;   // 0: straight on
  };
  auto slanted = [](float ax, float ay) { const float r = std::fabs(ax) / std::max(1e-3f, std::fabs(ax) + std::fabs(ay)); return r > 0.3f && r < 0.7f; };
  struct Cand { int d, x, y; };
  std::vector<Cand> cs;
  constexpr int RR = 90;
  if (what == "gate") {
    for (const auto& gt : g.world.gates) {
      const int gx = gt.first, gy = gt.second;
      if (std::abs(gx - hx) > RR || std::abs(gy - hy) > RR) continue;
      bool sl = false;
      for (int side = 0; side < 2 && !sl; side++) {
        const int px = side ? gx + 6 : gx - 4;
        float ax, ay, bx, by;
        for (int oy = -2; oy <= 2 && !sl; oy++)
          for (int ox = -1; ox <= 1 && !sl; ox++)
            if (wallAt(px + ox, gy + oy) && dirs(px + ox, gy + oy, ax, ay, bx, by) && (slanted(ax, ay) || slanted(bx, by))) sl = true;
      }
      if (sl) cs.push_back({std::abs(gx - hx) + std::abs(gy - hy), gx + 1, gy});
    }
  } else {
    for (int y = std::max(0, hy - RR); y < std::min(m.h, hy + RR); y++)
      for (int x = std::max(0, hx - RR); x < std::min(m.w, hx + RR); x++) {
        if (!wallAt(x, y)) continue;
        bool ok = what == "any";
        if (what == "water") {
          for (int oy = -1; oy <= 1 && !ok; oy++)
            for (int ox = -1; ox <= 1 && !ok; ox++) ok = m.in(x + ox, y + oy) && groundWater(m.at(x + ox, y + oy));
        } else if (what == "vertex" || what == "diag") {
          float ax, ay, bx, by;
          if (!dirs(x, y, ax, ay, bx, by)) continue;
          const float t = turnDeg(ax, ay, bx, by);
          ok = what == "vertex" ? t >= 30.0f : (t < 12.0f && slanted(ax, ay) && slanted(bx, by));
          for (const auto& gt : g.world.gates)   // (a gate's opening reads as a turn: never a vertex)
            if (ok && std::abs(gt.first + 1 - x) <= 7 && std::abs(gt.second - y) <= 5) ok = false;
        } else if (what != "any") {
          c.fail("wallspot: unknown place " + what + " (vertex|diag|gate|water|any)");
          return true;
        }
        if (ok) cs.push_back({std::abs(x - hx) + std::abs(y - hy), x, y});
      }
  }
  // one place per 12-tile cell (a vertex is a cluster of tiles), nearest first
  std::stable_sort(cs.begin(), cs.end(), [](const Cand& a, const Cand& b) { return a.d < b.d; });
  std::vector<Cand> one;
  for (const Cand& q : cs) {
    bool near = false;
    for (const Cand& o : one) if (std::abs(o.x - q.x) < 12 && std::abs(o.y - q.y) < 12) { near = true; break; }
    if (!near) one.push_back(q);
  }
  if (one.empty()) { c.fail("wallspot: no " + what + " on a wall here"); return true; }
  const Cand p = one[(size_t)nth % one.size()];
  // stand on open ground a few tiles south of it (the wall's face is its south side)
  int tx = p.x, ty = p.y + 3;
  bool found = false;
  for (int r = 0; r <= 8 && !found; r++)
    for (int oy = -r; oy <= r && !found; oy++)
      for (int ox = -r; ox <= r && !found; ox++) {
        const int x = p.x + ox, y = p.y + 3 + oy;
        if (std::max(std::abs(ox), std::abs(oy)) == r && m.in(x, y) && !m.blocked(x, y) && !groundWater(m.at(x, y))) { tx = x; ty = y; found = true; }
      }
  g.pl().p = Vec2(tx * (float)TILE + 8, ty * (float)TILE + 8);
  c.view.snap(g);
  std::printf("wallspot %s %d of %zu: wall %d,%d, hero %d,%d\n", what.c_str(), nth, one.size(), p.x, p.y, tx, ty);
  return true;
}
EMB_SCRIPT_CMD("wallspot", "wallspot <vertex|diag|gate|water|any> [n]: test only: stand by that place of the city wall (M7c ART)", cmdWallSpot);
}  // namespace

// (M10 ART) the organic shoreline's field at a global pixel, exactly as groundPixel's bank follows it (the tile grid's
// wetness bilinear between tile centres, two octaves of noise): 0 dry .. 1 wet, the shore at 0.5; -1 where all four
// tiles round the pixel agree (no shore near). The sea's surf (render_sea.cpp) runs its foam along the 0.5 contour.
float seaShoreField(const Map& m, int gox, int goy, int px, int py) {
  const float fx = (px - 7.5f) / 16.0f, fy = (py - 7.5f) / 16.0f;
  const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
  const float ax = fx - ix, ay = fy - iy;
  auto wet = [&](int x, int y) -> float {
    const Ground q = m.at(x - gox, y - goy);
    if (q == Ground::Void) return 1.0f;
    if (groundWater(q) && (m.blendAt(x - gox, y - goy) >> 4) == (Map::POOL_MARK >> 4)) return 0.0f;
    return groundWater(q) || q == Ground::Bridge ? 1.0f : 0.0f;
  };
  const float w00 = wet(ix, iy), w10 = wet(ix + 1, iy), w01 = wet(ix, iy + 1), w11 = wet(ix + 1, iy + 1);
  if (w00 == w10 && w00 == w01 && w00 == w11) return -1.0f;
  float v = (w00 * (1 - ax) + w10 * ax) * (1 - ay) + (w01 * (1 - ax) + w11 * ax) * ay;
  if (w00 == w11 && w10 == w01 && w00 != w10) {
    const float dd = w00 > 0.5f ? std::fabs(ax - ay) : std::fabs(ax + ay - 1.0f);
    v = std::max(v, 1.0f - dd * 0.9f);
  }
  v += (vnoise(px / 6.0f, py / 6.0f, 351) - 0.5f) * 0.30f + (vnoise(px / 19.0f, py / 19.0f, 353) - 0.5f) * 0.26f;
  return v;
}
