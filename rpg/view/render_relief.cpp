// M11 "High Ground": what of the relief moves or stands up, drawn live over the baked terrain (View's RELIEF block,
// rpg/view/view.h; VISION_PLAN 11.3, 13 M11, 15.32). FALLS lane.
//
// Hooks (render.cpp / hud.cpp call them):
//   reliefUpdate     update, after seaUpdate                   drawReliefWater  drawWorld, after the terrain, before
//                                                                               drawSeaWater (the falls over the faces)
//   reliefShift      Ev::WindowShift                            reliefSorted     the sorted pass's relief drawables
//   drawReliefSorted Drawable kind 11                           drawReliefOver   View::draw after the weather, before
//                                                                               drawSeaOver (cloud shadows, haze)
//
// What is drawn:
//   - WATERFALLS. A fall is a Map::HEIGHT_FALL tile (water with HEIGHT_CLIFF on the LOWER side of a level step) whose
//     higher neighbour is water too (the river or a plateau lake it spills from). Each fall tile is analysed once
//     (cached by its global tile, so window shifts cost nothing) and painted into a strip of 6 frames: the falling sheet
//     shaped to the face the terrain draws (the same smooth level field, so a slanted contour gets a slanted lip; 12 px a
//     level), streaks running down and speeding up, a glassy curl at the lip, the sheet whitening as it falls, a
//     churning foam pool and spray at the foot, foam trails drifting downstream, wet dark rock beside it. Columns use
//     the GLOBAL pixel for their streaks, so a wide fall is one continuous curtain across tiles; every run of fall tiles
//     shares a frame phase of its own, so no two falls move in lockstep. A fall from the west or east is a side cascade
//     (a white chute over the side wall); a fall to the north (the higher ground south of it) spills over the rim and
//     out of sight, its spray rising behind the lip. Frozen in the cold (ice ground, snowfields, glaciers): ice columns
//     and icicles, a frozen splash mound, no motion and no roar.
//   - MIST: pooled puffs (no allocation per frame) rising from each fall's foot and drifting with the wind
//     (Sea::Live::wind, updated on land too); in the morning sun a faint rainbow glints in the spray of a big fall. The
//     spray and mist of a fall are one sorted drawable at its foot (Drawable kind 11): the hero in front draws over it.
//   - THE ROAR: the nearest fall's distance and side, eased (no pops on a window shift), muffled indoors, its size from
//     the fall's height and width, to Audio::setFalls.
//   - OVER THE LAND (after the weather): slow cloud shadows drifting with the wind, and a light haze over the highest
//     ground; by day only, faded out at dusk.
// Night: the falls are lit by the world's lighting pass like everything else, their foam toned down to cool moonlit
// highlights.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include "engine/audio.h"
#include "engine/pix.h"
#include "rpg/art/art_relief.h"
#include "rpg/sim/game.h"
#include "rpg/sim/sea.h"
#include "rpg/view/view.h"

namespace {
constexpr int NF = art::FALL_FRAMES;   // frames of a flowing fall (streaks move 2 px a frame over a 12 px period)
constexpr float FPS = 12.0f;     // its frame rate
constexpr int MAX_PUFFS = 72;
constexpr int MAX_RUNS = 24;
constexpr int BAKES_PER_FRAME = 3;

inline uint32_t hh(int a, int b, uint32_t s) { return hash32((uint32_t)a * 73856093u ^ (uint32_t)b * 19349663u ^ s * 83492791u); }
inline float hf(int a, int b, uint32_t s) { return (float)(hh(a, b, s) & 0xFFFFFF) / 16777216.0f; }
inline int pmod(int a, int m) { const int r = a % m; return r < 0 ? r + m : r; }
inline uint64_t gkey(int32_t gx, int32_t gy) { return ((uint64_t)(uint32_t)gx << 32) | (uint32_t)gy; }

inline uint32_t withA(uint32_t c, float a) { return (c & 0x00FFFFFFu) | ((uint32_t)std::clamp((int)(a * 255.0f), 0, 255) << 24); }

// the level field the terrain draws its faces from (terrain.cpp reliefPixel's F: tile levels bilinear between tile
// centres, rounded), at a window pixel
int levelField(const Map& m, int px, int py) {
  const float fx = (px - 8) / 16.0f, fy = (py - 8) / 16.0f;
  const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
  const float ax = fx - ix, ay = fy - iy;
  const float v = ((float)m.heightAt(ix, iy) * (1 - ax) + (float)m.heightAt(ix + 1, iy) * ax) * (1 - ay) +
                  ((float)m.heightAt(ix, iy + 1) * (1 - ax) + (float)m.heightAt(ix + 1, iy + 1) * ax) * ay;
  return (int)std::floor(v + 0.5f);
}
bool wetTile(const Map& m, int tx, int ty) {
  const Ground q = m.at(tx, ty);
  return groundWater(q) || q == Ground::Ice;
}
// the organic shoreline (terrain.cpp's, as seaShoreField; ice counts as water here): is this window pixel water?
bool wetPx(const Map& m, int gox, int goy, int px, int py) {
  const float fx = (px - 7.5f) / 16.0f, fy = (py - 7.5f) / 16.0f;
  const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
  const float ax = fx - ix, ay = fy - iy;
  auto wet = [&](int x, int y) -> float {
    const Ground q = m.at(x, y);
    if (q == Ground::Void) return 1.0f;
    if (groundWater(q) && (m.blendAt(x, y) >> 4) == (Map::POOL_MARK >> 4)) return 0.0f;
    return groundWater(q) || q == Ground::Ice || q == Ground::Bridge ? 1.0f : 0.0f;
  };
  const float w00 = wet(ix, iy), w10 = wet(ix + 1, iy), w01 = wet(ix, iy + 1), w11 = wet(ix + 1, iy + 1);
  if (w00 == w10 && w00 == w01 && w00 == w11) return w00 > 0.5f;
  float v = (w00 * (1 - ax) + w10 * ax) * (1 - ay) + (w01 * (1 - ax) + w11 * ax) * ay;
  if (w00 == w11 && w10 == w01 && w00 != w10) {
    const float dd = w00 > 0.5f ? std::fabs(ax - ay) : std::fabs(ax + ay - 1.0f);
    v = std::max(v, 1.0f - dd * 0.9f);
  }
  const int gx = px + gox * 16, gy = py + goy * 16;
  v += (vnoise(gx / 6.0f, gy / 6.0f, 351) - 0.5f) * 0.30f + (vnoise(gx / 19.0f, gy / 19.0f, 353) - 0.5f) * 0.26f;
  return v > 0.5f;
}

// which way a fall tile's water comes from: 0 the north (down the face toward the viewer), 1 the west, 2 the east
// (a side cascade), 3 the south (over the rim, away from the viewer); -1 no water above it (a river at a cliff's foot)
int fallDir(const Map& m, int tx, int ty) {
  if (!m.fallAt(tx, ty)) return -1;
  const int l = m.heightAt(tx, ty);
  if (m.heightAt(tx, ty - 1) > l && wetTile(m, tx, ty - 1)) return 0;
  const bool w = m.heightAt(tx - 1, ty) > l && wetTile(m, tx - 1, ty), e = m.heightAt(tx + 1, ty) > l && wetTile(m, tx + 1, ty);
  if (w) return 1;
  if (e) return 2;
  if (m.heightAt(tx, ty + 1) > l && wetTile(m, tx, ty + 1)) return 3;
  return -1;
}
int upTile(int dir, int tx, int ty, int& ux, int& uy) {
  ux = tx + (dir == 1 ? -1 : dir == 2 ? 1 : 0);
  uy = ty + (dir == 0 ? -1 : dir == 3 ? 1 : 0);
  return 0;
}
bool frozenAt(const Map& m, int tx, int ty, int dir) {
  int ux, uy;
  upTile(dir, tx, ty, ux, uy);
  if (m.at(tx, ty) == Ground::Ice || m.at(ux, uy) == Ground::Ice) return true;
  const Eco e = m.ecoAt(tx, ty);
  return e == Eco::Glacier || e == Eco::FrozenLakes || e == Eco::SnowField || m.biomeAt(tx, ty) == Biome::Snow;
}

// soft pixel puffs and cloud shadows: stepped alpha (pixel art, not airbrush), ragged by noise
Canvas puffCanvas(int r, uint32_t seed) {
  Canvas c(2 * r + 2, 2 * r + 2);
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      const float dx = (x + 0.5f - (r + 1)) / r, dy = (y + 0.5f - (r + 1)) / (r * 0.85f);
      const float d = std::sqrt(dx * dx + dy * dy) + (vnoise(x / 2.2f, y / 2.2f, seed) - 0.5f) * 0.45f;
      if (d >= 1.0f) continue;
      const float a = d < 0.45f ? 0.85f : d < 0.75f ? 0.55f : 0.28f;
      const int k = d < 0.45f && dy < -0.1f ? 255 : d < 0.75f ? 244 : 232;   // a lit crown
      c.set(x, y, rgba(k - 4, k, 255, (int)(a * 255)));
    }
  return c;
}
Canvas cloudCanvas(int w, int h, uint32_t seed) {
  Canvas c(w, h);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      const float dx = (x + 0.5f - w * 0.5f) / (w * 0.5f), dy = (y + 0.5f - h * 0.5f) / (h * 0.5f);
      const float r = std::sqrt(dx * dx + dy * dy);
      const float n = fbm(x / 34.0f, y / 30.0f, seed, 3) - r * 0.70f;
      if (n < 0.04f) continue;
      const float a = n > 0.16f ? 1.0f : n > 0.09f ? 0.66f : 0.33f;   // three soft steps to the edge
      c.set(x, y, rgba(22, 20, 52, (int)(a * 255)));
    }
  return c;
}
Canvas rainbowCanvas() {
  Canvas c(34, 12);
  static const uint32_t B[6] = {rgba(255, 70, 60), rgba(255, 160, 50), rgba(255, 240, 80), rgba(80, 230, 100), rgba(70, 140, 255), rgba(150, 80, 230)};
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      const float dx = x + 0.5f - 17.0f, dy = (y + 0.5f - 13.0f) * 1.25f;
      const float r = std::sqrt(dx * dx + dy * dy);
      const int band = (int)std::floor(16.5f - r);
      if (band < 0 || band > 5) continue;
      const float fade = std::clamp((13.0f - y) / 6.0f, 0.0f, 1.0f) * 0.9f;   // the arc fades into the spray at its feet
      c.set(x, y, withA(B[band], fade));
    }
  return c;
}
}  // namespace

struct View::ReliefViewState {
  ReliefViewStats last, cur;
  // ---- the fall tiles seen (by global tile)
  struct FallTile {
    int8_t dir = -1;           // fallDir
    uint8_t drop = 1;          // levels it falls
    bool frozen = false;
    bool baked = false;
    int16_t ox = 0, oy = 0;    // the frame's top-left, from the tile's top-left (px)
    int16_t w = 0, h = 0;      // one frame
    int8_t frames = 1;
    Tex tex;                   // the frames side by side
    // the foot (spray, mist, the roar), from the tile's top-left
    int16_t fx0 = 0, fx1 = 0, fy0 = 0, fy1 = 0;
    bool any = false;          // a column (or row) of the tile carries water over the step
    uint64_t run = 0;          // the run of fall tiles it belongs to (its anchor's global tile)
    uint8_t runW = 1;          // the run's tiles
    float used = 0;
  };
  std::unordered_map<uint64_t, FallTile> tiles;
  const Map* map = nullptr;
  uint64_t seed = 0;          // (a new game on the same map object: the cache is the old world's)
  // ---- this frame's runs on screen (window px)
  struct Run { uint64_t key = 0; float x0 = 0, x1 = 0, y0 = 0, y1 = 0; int8_t dir = 0; uint8_t drop = 1; bool frozen = false; int n = 0; float acc = 0; };
  Run runs[MAX_RUNS];
  int nRuns = 0;
  // ---- the mist (window px), pooled
  struct Puff { float x = 0, y = 0, vx = 0, vy = 0, age = 0, life = 0; uint64_t run = 0; uint8_t size = 0; bool on = false; };
  Puff puffs[MAX_PUFFS];
  uint32_t rng = 0x9E3779B9u;
  // ---- the textures
  Tex puff[3], cloud[3], haze, rainbow;
  bool texBaked = false;
  // ---- the roar
  float roar = 0, pan = 0, size = 0.5f, outdoorRoar = 0;
  float frand() { rng = hash32(rng + 0x6D2B79F5u); return (float)(rng & 0xFFFFFF) / 16777216.0f; }
};

struct ReliefViewOps {
  static View::ReliefViewState& st(View& v) {
    if (!v.reliefView_) v.reliefView_ = std::make_shared<View::ReliefViewState>();
    return *v.reliefView_;
  }
};

namespace {
struct ReliefTimer {
  double& acc;
  std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
  explicit ReliefTimer(double& a) : acc(a) {}
  ~ReliefTimer() { acc += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); }
};

}  // namespace

View::ReliefViewStats View::reliefViewStats() const { return reliefView_ ? reliefView_->last : ReliefViewStats(); }

// (relief_script.cpp: gotofall)
int reliefFallDir(const Map& m, int tx, int ty) { return fallDir(m, tx, ty); }
bool reliefFallFrozen(const Map& m, int tx, int ty, int dir) { return frozenAt(m, tx, ty, dir); }

namespace {
// analyses and paints one fall tile (window tile tx, ty): its spans, its foot, its frames
void buildFall(Pix& P, const Map& m, int gox, int goy, int tx, int ty, View::ReliefViewState::FallTile& F) {
  F.dir = (int8_t)fallDir(m, tx, ty);
  F.any = false;
  if (F.dir < 0) return;
  const int l = m.heightAt(tx, ty);
  int ux, uy;
  upTile(F.dir, tx, ty, ux, uy);
  F.drop = (uint8_t)std::clamp(m.heightAt(ux, uy) - l, 1, 3);
  F.frozen = frozenAt(m, tx, ty, F.dir);
  // one sheet of water over two levels (a lake across a step) shows no face (terrain.cpp: water has no cliff in it
  // where every neighbour of another level is water too): no fall is drawn there either
  {
    bool face = false;
    for (int oy = -2; oy <= 1 && !face; oy++)
      for (int ox = -1; ox <= 1 && !face; ox++)
        if (m.heightAt(tx + ox, ty + oy) != l && !groundWater(m.at(tx + ox, ty + oy))) face = true;
    if (!face) return;
  }
  const int fh = 12 * F.drop;
  const int x0 = tx * 16, y0 = ty * 16;
  art::FallSpan sp[22];
  int lo = 1 << 20, hi = -(1 << 20);
  if (F.dir == 0 || F.dir == 3) {
    for (int i = -3; i <= 18; i++) {
      art::FallSpan& s = sp[i + 3];
      const int x = x0 + i;
      if (F.dir == 0) {   // the first face pixel below the contour
        for (int y = y0 - 14; y <= y0 + 14; y++)
          if (levelField(m, x, y - 1) > l && levelField(m, x, y) == l) { s.at = (int16_t)y; break; }
        if (!s.at) continue;
        // the water under the face (where the terrain paints its wet face), fed from above or pouring out below
        s.wet = wetPx(m, gox, goy, x, s.at + fh / 2) && (wetPx(m, gox, goy, x, s.at - 2) || wetPx(m, gox, goy, x, s.at - 5)) &&
                (wetPx(m, gox, goy, x, s.at + fh + 2) || wetPx(m, gox, goy, x, s.at + fh + 5));
      } else {            // the first pixel of the higher ground to the south
        for (int y = y0 + 2; y <= y0 + 30; y++)
          if (levelField(m, x, y) > l && levelField(m, x, y - 1) == l) { s.at = (int16_t)y; break; }
        if (!s.at) continue;
        s.wet = wetPx(m, gox, goy, x, s.at + 3) && wetPx(m, gox, goy, x, s.at - 14);
      }
      if (s.wet && i >= 0 && i < 16) { F.any = true; lo = std::min(lo, (int)s.at); hi = std::max(hi, (int)s.at); }
    }
  } else {
    for (int j = -3; j <= 18; j++) {
      art::FallSpan& s = sp[j + 3];
      const int y = y0 + j;
      bool found = false;
      for (int x = x0 - 12; x <= x0 + 12; x++) {
        const bool hit = F.dir == 1 ? (levelField(m, x - 1, y) > l && levelField(m, x, y) == l) : (levelField(m, x + 1, y) > l && levelField(m, x, y) == l);
        if (hit) { s.at = (int16_t)x; found = true; if (F.dir == 1) break; }
      }
      if (!found) continue;
      const int sg = F.dir == 1 ? 1 : -1, SW = 6 + 3 * (F.drop - 1);
      s.wet = wetPx(m, gox, goy, s.at - sg * 3, y) && wetPx(m, gox, goy, s.at + sg * (SW + 3), y);
      if (s.wet && j >= 0 && j < 16) { F.any = true; lo = std::min(lo, (int)s.at); hi = std::max(hi, (int)s.at); }
    }
  }
  if (!F.any) return;
  // the frame's box (window px) and the foot
  int bx0, by0, bx1, by1;
  const int churnH = 3 + F.drop * 2;
  if (F.dir == 0) {
    bx0 = x0 - 3; bx1 = x0 + 19; by0 = lo - 4; by1 = hi + fh + churnH + 8;
    F.fx0 = 0; F.fx1 = 16; F.fy0 = (int16_t)((lo + hi) / 2 + fh - y0); F.fy1 = (int16_t)(F.fy0 + churnH);
  } else if (F.dir == 3) {
    bx0 = x0; bx1 = x0 + 16; by0 = lo - 20; by1 = hi + 1;
    F.fx0 = 0; F.fx1 = 16; F.fy0 = (int16_t)((lo + hi) / 2 - 14 - y0); F.fy1 = (int16_t)(F.fy0 + 6);
  } else {
    const int SW = 6 + 3 * (F.drop - 1);
    if (F.dir == 1) { bx0 = lo - 4; bx1 = hi + SW + 8; } else { bx0 = lo - SW - 8; bx1 = hi + 5; }
    by0 = y0 - 3; by1 = y0 + 19;
    const int footX = F.dir == 1 ? (lo + hi) / 2 + SW : (lo + hi) / 2 - SW;
    F.fx0 = (int16_t)(footX - 2 - x0); F.fx1 = (int16_t)(footX + 2 - x0); F.fy0 = 0; F.fy1 = 16;
  }
  F.ox = (int16_t)(bx0 - x0); F.oy = (int16_t)(by0 - y0);
  F.w = (int16_t)(bx1 - bx0); F.h = (int16_t)(by1 - by0);
  F.frames = F.frozen ? 1 : NF;
  Canvas c(F.w * F.frames, F.h);
  for (int f = 0; f < F.frames; f++) {
    if (F.dir == 0) art::paintFallNorth(c, f, bx0, by0, x0, sp, fh, F.drop, F.frozen, gox, goy);
    else if (F.dir == 3) art::paintFallRim(c, f, bx0, by0, x0, sp, F.frozen, gox, goy);
    else art::paintFallSide(c, f, bx0, by0, y0, sp, F.dir, F.drop, F.frozen, gox, goy);
  }
  F.tex = P.bake(c);
  F.baked = true;
}

// the mist puffs, cloud shadows, haze and rainbow (once)
void bakeReliefTex(View::ReliefViewState& S, Pix& P) {
  if (S.texBaked) return;
  S.puff[0] = P.bake(puffCanvas(3, 4201));
  S.puff[1] = P.bake(puffCanvas(5, 4203));
  S.puff[2] = P.bake(puffCanvas(7, 4205));
  S.cloud[0] = P.bake(cloudCanvas(200, 110, 4207));
  S.cloud[1] = P.bake(cloudCanvas(250, 130, 4209));
  S.cloud[2] = P.bake(cloudCanvas(170, 96, 4211));
  S.haze = P.bake(puffCanvas(22, 4213));
  S.rainbow = P.bake(rainbowCanvas());
  S.texBaked = true;
}
// the run a fall tile belongs to: its fall neighbours (8-way) with the same direction, by flood (bounded); its key is
// the westmost-then-northmost tile's, so every tile of a wide fall agrees on it
void runOf(const Map& m, int gox, int goy, int tx, int ty, int dir, uint64_t& key, uint8_t& n) {
  int qx[48], qy[48], cnt = 0, head = 0;
  qx[cnt] = tx; qy[cnt] = ty; cnt++;
  int bx = tx, by = ty;
  while (head < cnt && cnt < 48) {
    const int x = qx[head], y = qy[head];
    head++;
    for (int oy = -1; oy <= 1; oy++)
      for (int ox = -1; ox <= 1; ox++) {
        if (!ox && !oy) continue;
        const int nx = x + ox, ny = y + oy;
        if (std::abs(nx - tx) > 12 || std::abs(ny - ty) > 12) continue;
        bool seen = false;
        for (int k = 0; k < cnt && !seen; k++) seen = qx[k] == nx && qy[k] == ny;
        if (seen || fallDir(m, nx, ny) != dir) continue;
        if (cnt < 48) { qx[cnt] = nx; qy[cnt] = ny; cnt++; }
        if (nx < bx || (nx == bx && ny < by)) { bx = nx; by = ny; }
      }
  }
  key = gkey(bx + gox, by + goy);
  n = (uint8_t)std::min(cnt, 255);
}
}  // namespace

void View::reliefUpdate(Game& g, float dt) {
  ReliefViewState& S = ReliefViewOps::st(*this);
  S.last = S.cur;
  S.cur = ReliefViewStats();
  ReliefTimer tm(S.cur.ms);
  // ---- the mist: rise, drift with the wind, fade; new puffs at each fall's foot (last frame's runs)
  const sea::Sea::Wind& wd = g.sea.live.wind;
  const float wx = std::cos(wd.dir) * (3.0f + 9.0f * wd.speed), wy = std::sin(wd.dir) * (2.0f + 5.0f * wd.speed);
  for (ReliefViewState::Puff& p : S.puffs) {
    if (!p.on) continue;
    p.age += dt;
    if (p.age >= p.life) { p.on = false; continue; }
    p.vx += (wx - p.vx) * std::min(1.0f, dt * 0.9f);
    p.vy += (wy * 0.3f - 2.0f - p.vy) * std::min(1.0f, dt * 0.6f);
    p.x += p.vx * dt;
    p.y += p.vy * dt;
  }
  if (!g.inside && g.mode != Mode::Title) {
    for (int r = 0; r < S.nRuns; r++) {
      ReliefViewState::Run& R = S.runs[r];
      if (R.frozen) continue;
      R.acc += dt * (0.9f + 0.9f * R.drop) * std::min(5, R.n);
      while (R.acc >= 1.0f) {
        R.acc -= 1.0f;
        ReliefViewState::Puff* slot = nullptr;
        for (ReliefViewState::Puff& p : S.puffs) if (!p.on) { slot = &p; break; }
        if (!slot) break;
        ReliefViewState::Puff& p = *slot;
        p.on = true;
        p.run = R.key;
        p.x = R.x0 + (R.x1 - R.x0) * S.frand();
        p.y = R.y0 + (R.y1 - R.y0) * S.frand();
        p.vx = (S.frand() - 0.5f) * 6.0f;
        p.vy = -3.0f - S.frand() * (4.0f + 2.0f * R.drop);
        p.age = 0;
        p.life = 1.4f + S.frand() * (1.2f + 0.4f * R.drop);
        p.size = (uint8_t)std::min(2, (int)(S.frand() * (1.5f + 0.6f * R.drop)));
      }
    }
  }
  // ---- the roar: the nearest flowing fall within 18 tiles, by the distance to its foot and its side
  float target = 0, tpan = 0, tsize = S.size;
  if (!g.inside && g.mode != Mode::Title && !g.world.over.height.empty()) {
    const Map& m = g.world.over;
    const Vec2 p = g.pl().p;
    const int px = (int)std::floor(p.x / 16), py = (int)std::floor(p.y / 16);
    float best = 1e9f;
    int bx = 0, by = 0, bd = -1;
    for (int y = py - 18; y <= py + 18; y++)
      for (int x = px - 18; x <= px + 18; x++) {
        if (!m.fallAt(x, y)) continue;
        const float dx = (x * 16 + 8) - p.x, dy = (y * 16 + 14) - p.y;
        const float d = std::sqrt(dx * dx + dy * dy) / 16.0f;
        if (d >= best) continue;
        const int dir = fallDir(m, x, y);
        if (dir < 0 || frozenAt(m, x, y, dir)) continue;
        best = d; bx = x; by = y; bd = dir;
      }
    if (bd >= 0) {
      int ux, uy;
      upTile(bd, bx, by, ux, uy);
      const int drop = std::clamp(m.heightAt(ux, uy) - m.heightAt(bx, by), 1, 3);
      int width = 0;   // the fall's width: its fall tiles along the step
      for (int k = -4; k <= 4; k++) width += (bd == 0 || bd == 3) ? (fallDir(m, bx + k, by) == bd) : (fallDir(m, bx, by + k) == bd);
      tsize = std::clamp(0.12f + 0.24f * (drop - 1) + 0.10f * (width - 1), 0.0f, 1.0f);
      const float reach = 10.0f + 8.0f * tsize;   // a great fall carries further
      const float k = std::clamp(1.0f - best / reach, 0.0f, 1.0f);
      target = k * k * (0.55f + 0.45f * tsize);
      tpan = std::clamp(((bx * 16 + 8) - p.x) / 120.0f, -1.0f, 1.0f);
    }
    S.outdoorRoar = target;
  } else if (g.inside && g.subSite < 0) {
    target = S.outdoorRoar * 0.3f;   // indoors by a fall: the walls take most of it
  }
  const float ke = 1.0f - std::exp(-dt / 0.45f);
  S.roar += (target - S.roar) * ke;
  S.pan += (tpan - S.pan) * ke;
  S.size += (tsize - S.size) * (1.0f - std::exp(-dt / 1.2f));
  if (S.roar < 0.002f && target == 0) S.roar = 0;
  S.cur.roar = S.roar;
  if (audio_) audio_->setFalls(S.roar, S.pan, S.size);
}

void View::reliefShift(Vec2 d) {
  if (!reliefView_) return;
  ReliefViewState& S = *reliefView_;
  for (ReliefViewState::Puff& p : S.puffs) if (p.on) { p.x += d.x; p.y += d.y; }
  for (int r = 0; r < S.nRuns; r++) { S.runs[r].x0 += d.x; S.runs[r].x1 += d.x; S.runs[r].y0 += d.y; S.runs[r].y1 += d.y; }
}

void View::drawReliefWater(Game& g, Vec2 cam) {
  ReliefViewState& S = ReliefViewOps::st(*this);
  if (g.inside) { S.nRuns = 0; return; }
  ReliefTimer tm(S.cur.ms);
  Pix& P = *pix_;
  const Map& m = g.world.over;
  if (!!m.height.empty()) { S.nRuns = 0; return; }
  if (S.map != &m || S.seed != g.seed) {   // a new overworld map (or a new world in it): forget the old tiles
    for (auto& kv : S.tiles) if (kv.second.baked) P.destroy(kv.second.tex);
    S.tiles.clear();
    S.map = &m;
    S.seed = g.seed;
  }
  const int gox = g.world.ox, goy = g.world.oy;
  const int tx0 = (int)std::floor(cam.x / 16) - 2, ty0 = (int)std::floor(cam.y / 16) - 3;
  const int tx1 = (int)std::floor((cam.x + Pix::W) / 16) + 2, ty1 = (int)std::floor((cam.y + Pix::H) / 16) + 2;
  const float night = std::clamp(1.0f - g.daylight() * 1.6f, 0.0f, 1.0f);
  // by night the white water is toned down to cool moonlit highlights (the lighting pass darkens it further)
  const Color tint(1.0f - 0.30f * night, 1.0f - 0.24f * night, 1.0f - 0.08f * night, 1.0f);
  const int frameNow = (int)std::floor(t_ * FPS);
  int bakes = 0;
  S.nRuns = 0;
  for (int ty = ty0; ty <= ty1; ty++)
    for (int tx = tx0; tx <= tx1; tx++) {
      if (!m.fallAt(tx, ty)) continue;
      const uint64_t key = gkey(tx + gox, ty + goy);
      auto it = S.tiles.find(key);
      if (it == S.tiles.end()) {
        // (never analysed at the window's edge: the levels beyond it are not loaded, and the cache outlives a shift)
        if (bakes >= BAKES_PER_FRAME || tx < 3 || ty < 3 || tx >= m.w - 3 || ty >= m.h - 3) continue;
        bakes++;
        ReliefViewState::FallTile F;
        buildFall(P, m, gox, goy, tx, ty, F);
        if (F.any) runOf(m, gox, goy, tx, ty, F.dir, F.run, F.runW);
        it = S.tiles.emplace(key, F).first;
      }
      ReliefViewState::FallTile& F = it->second;
      F.used = t_;
      if (!F.any || !F.baked) continue;
      const float sx = (float)(tx * 16 + F.ox) - cam.x, sy = (float)(ty * 16 + F.oy) - cam.y;
      const int fr = F.frames > 1 ? pmod(frameNow + (int)(hash32((uint32_t)F.run ^ (uint32_t)(F.run >> 32)) % NF), NF) : 0;
      P.blitEx(F.tex, fr * F.w, 0, F.w, F.h, std::floor(sx), std::floor(sy), (float)F.w, (float)F.h, false, F.frozen ? Color(1, 1, 1, 1) : tint);
      S.cur.falls++;
      // gather the runs (window px feet)
      const float fx0 = (float)(tx * 16 + F.fx0), fx1 = (float)(tx * 16 + F.fx1), fy0 = (float)(ty * 16 + F.fy0), fy1 = (float)(ty * 16 + F.fy1);
      int r = 0;
      while (r < S.nRuns && S.runs[r].key != F.run) r++;
      if (r == S.nRuns) {
        if (S.nRuns >= MAX_RUNS) continue;
        ReliefViewState::Run& R = S.runs[S.nRuns++];
        const float acc = R.key == F.run ? R.acc : 0.0f;
        R = ReliefViewState::Run();
        R.key = F.run; R.x0 = fx0; R.x1 = fx1; R.y0 = fy0; R.y1 = fy1; R.dir = F.dir; R.drop = F.drop; R.frozen = F.frozen; R.n = 1; R.acc = acc;
      } else {
        ReliefViewState::Run& R = S.runs[r];
        R.x0 = std::min(R.x0, fx0); R.x1 = std::max(R.x1, fx1); R.y0 = std::min(R.y0, fy0); R.y1 = std::max(R.y1, fy1);
        R.drop = std::max(R.drop, F.drop); R.n++;
      }
    }
  // forget tiles long out of sight
  if (S.tiles.size() > 160) {
    for (auto it = S.tiles.begin(); it != S.tiles.end();) {
      if (t_ - it->second.used > 8.0f) { if (it->second.baked) P.destroy(it->second.tex); it = S.tiles.erase(it); }
      else ++it;
    }
  }
}

void View::reliefSorted(Game& g, Vec2 cam, const std::function<void(float, int)>& push) {
  (void)cam;
  if (g.inside || !reliefView_) return;
  ReliefViewState& S = *reliefView_;
  for (int r = 0; r < S.nRuns; r++) if (!S.runs[r].frozen) push(S.runs[r].y1 + 2.0f, r);
}

void View::drawReliefSorted(Game& g, int idx, Vec2 cam) {
  if (!reliefView_) return;
  ReliefViewState& S = *reliefView_;
  if (idx < 0 || idx >= S.nRuns) return;
  ReliefTimer tm(S.cur.ms);
  Pix& P = *pix_;
  bakeReliefTex(S, P);
  const ReliefViewState::Run& R = S.runs[idx];
  const float day = std::clamp(g.daylight() * 1.6f, 0.0f, 1.0f);
  const float lit = 0.55f + 0.45f * day;
  // the mist of this fall (and any puff whose fall went out of sight: with the first run)
  int drawn = 0;
  for (const ReliefViewState::Puff& p : S.puffs) {
    if (!p.on) continue;
    if (p.run != R.key) {
      bool orphan = true;
      for (int r = 0; r < S.nRuns && orphan; r++) orphan = S.runs[r].key != p.run;
      if (!orphan || idx != 0) continue;
    }
    const float u = p.age / p.life;
    const float a = std::sin(u * 3.14159f) * (0.30f + 0.08f * p.size) * (1.0f - 0.25f * u);
    const Tex& t = S.puff[p.size];
    P.blitEx(t, 0, 0, t.w, t.h, std::floor(p.x - cam.x - t.w * 0.5f), std::floor(p.y - cam.y - t.h * 0.5f), (float)t.w, (float)t.h, false,
             Color(lit * 0.96f, lit * 0.98f, lit, a));
    drawn++;
  }
  S.cur.mistPuffs += drawn;
  S.cur.sprays++;
  // a rainbow in the morning sun, in the spray of a big fall seen face on
  const float h = g.hour;
  const float morning = std::clamp((h - 6.5f) / 1.0f, 0.0f, 1.0f) * std::clamp((10.5f - h) / 1.5f, 0.0f, 1.0f);
  if (morning > 0.01f && R.dir == 0 && (R.n >= 2 || R.drop >= 2)) {
    const float cx = (R.x0 + R.x1) * 0.5f, w = (float)S.rainbow.w;
    const float a = morning * 0.55f * (0.75f + 0.25f * std::sin(t_ * 0.7f + (float)(R.key & 255)));
    P.blitEx(S.rainbow, 0, 0, S.rainbow.w, S.rainbow.h, std::floor(cx - cam.x - w * 0.5f), std::floor(R.y0 - cam.y - S.rainbow.h + 3), w, (float)S.rainbow.h,
             false, Color(1, 1, 1, a), 1);
  }
}

void View::drawReliefOver(Game& g, Vec2 cam) {
  if (g.inside) return;
  bakeReliefTex(ReliefViewOps::st(*this), *pix_);
  ReliefViewState& S = *reliefView_;
  if (g.mode == Mode::Title || g.mode == Mode::Creator) return;
  const Map& m = g.world.over;
  if (!!m.height.empty()) return;
  ReliefTimer tm(S.cur.ms);
  const float day = std::clamp((g.daylight() - 0.35f) / 0.4f, 0.0f, 1.0f);
  Pix& P = *pix_;
  // ---- (M11 fixer r1) by night white water still catches the moonlight: a pale cool sheen over each fall's sheet
  // and its foam pool, drawn after the lighting pass (the falls vanished into the dark at 21:00), breathing slowly
  {
    const float night = std::clamp(1.0f - g.daylight() * 1.8f, 0.0f, 1.0f);
    if (night > 0.02f)
      for (int r = 0; r < S.nRuns; r++) {
        const ReliefViewState::Run& R = S.runs[r];
        if (R.frozen) continue;
        const float cx = (R.x0 + R.x1) * 0.5f, w = std::max(8.0f, R.x1 - R.x0);
        const float br = 0.85f + 0.15f * std::sin(t_ * 2.3f + (float)(R.key & 255));
        const int ps = std::min(2, R.drop >= 2 || R.n >= 3 ? 2 : 1);
        const Tex& t = S.puff[ps];
        const float sc = std::max(1.0f, (w + 14.0f) / (float)t.w);
        // the foot's foam pool (brightest) and the falling sheet above it
        const float fw = t.w * sc, fh = t.h * sc * 0.8f;
        P.blitEx(t, 0, 0, t.w, t.h, std::floor(cx - cam.x - fw * 0.5f), std::floor(R.y1 - cam.y - fh * 0.5f), fw, fh, false,
                 Color(0.80f, 0.88f, 1.0f, 0.42f * night * br));
        P.blitEx(t, 0, 0, t.w, t.h, std::floor(cx - cam.x - fw * 0.35f), std::floor(R.y0 - cam.y - fh * 0.3f), fw * 0.7f, fh * 0.6f, false,
                 Color(0.74f, 0.84f, 1.0f, 0.26f * night * br));
      }
  }
  if (day <= 0.01f) return;
  // ---- cloud shadows: a sparse lattice of soft shadows in global px, drifting with the wind (slowly)
  const sea::Sea::Wind& wd = g.sea.live.wind;
  const double drift = (double)t_ * (4.0 + 6.0 * wd.speed);
  const double gcx = (double)g.world.ox * 16.0 + cam.x - std::cos(wd.dir) * drift, gcy = (double)g.world.oy * 16.0 + cam.y - std::sin(wd.dir) * drift;
  constexpr int CW = 320, CH = 200;
  const int cx0 = (int)std::floor(gcx / CW) - 1, cy0 = (int)std::floor(gcy / CH) - 1;
  const int cx1 = (int)std::floor((gcx + Pix::W) / CW) + 1, cy1 = (int)std::floor((gcy + Pix::H) / CH) + 1;
  for (int cy = cy0; cy <= cy1; cy++)
    for (int cx = cx0; cx <= cx1; cx++) {
      const uint32_t h = hh(cx, cy, 4221);
      if ((h & 255) > 80) continue;   // about one cell in three carries a cloud
      const Tex& t = S.cloud[(h >> 8) % 3];
      const double wx = cx * (double)CW + (double)((h >> 12) % (CW - 60)), wy = cy * (double)CH + (double)((h >> 20) % (CH - 40));
      const float sx = (float)(wx - gcx), sy = (float)(wy - gcy);
      if (sx > Pix::W || sy > Pix::H || sx + t.w < 0 || sy + t.h < 0) continue;
      P.blitEx(t, 0, 0, t.w, t.h, std::floor(sx), std::floor(sy), (float)t.w, (float)t.h, false, Color(1, 1, 1, 0.085f * day));
    }
  // ---- haze over the highest ground: soft veils on a 3-tile lattice where the land stands at level 5 or more
  const int tx0 = (int)std::floor(cam.x / 16) - 2, ty0 = (int)std::floor(cam.y / 16) - 2;
  const int tx1 = (int)std::floor((cam.x + Pix::W) / 16) + 2, ty1 = (int)std::floor((cam.y + Pix::H) / 16) + 2;
  const int gox = g.world.ox, goy = g.world.oy;
  for (int ty = ty0 - pmod(ty0 + goy, 3); ty <= ty1; ty += 3)
    for (int tx = tx0 - pmod(tx0 + gox, 3); tx <= tx1; tx += 3) {
      const int lv = m.heightAt(tx, ty);
      if (lv < 6) continue;
      const uint32_t h = hh(tx + gox, ty + goy, 4223);
      const float wob = std::sin(t_ * 0.15f + (float)(h & 1023) * 0.01f);
      const float sx = tx * 16.0f + (float)(h % 16) - 22.0f + wob * 6.0f - cam.x, sy = ty * 16.0f + (float)((h >> 4) % 16) - 22.0f - cam.y;
      const float a = 0.03f * (float)(lv - 5) * day;
      P.blitEx(S.haze, 0, 0, S.haze.w, S.haze.h, std::floor(sx), std::floor(sy), (float)S.haze.w, (float)S.haze.h, false, Color(0.86f, 0.9f, 0.98f, a));
    }
}
