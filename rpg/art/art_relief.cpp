// M11 "High Ground": the relief props (rpg/art/art_relief.h). FALLS lane.
//
// Painted in the game's high 3/4 view with the light from the top-left: lit tops and west faces, mid-tone south
// faces, shaded east faces, contact shade where a thing meets what it stands on, the hue-shifted ramps of
// art_internal.h. propSprite outlines every cell; the view gives each its ground shadow (prop_traits.cpp propShadow).
//   SummitShrine  an old open shrine on the summit: a two-tier stepped dais of weathered flagstones, four squat pillars
//                 under a heavy stone roof with a ridge and finials, a worn idol within, an altar before it with
//                 offerings (bowls, a red cloth, coins, a candle), and two braziers on the front corners whose fire
//                 flickers over 4 frames (prop_traits.cpp lights them: the shrine is a light). Lichen and cracks.
//   Cairn         a waymarker of six flat stones stacked with a slight lean, each its own grey, lichen, grass at the foot
//   PrayerFlags   two leaning poles with stone footings and a sagging line of faded cloth flags (blue, white, red,
//                 green, yellow), each flag fluttering on its own phase over 4 frames
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>
#include "rpg/art/art_internal.h"
#include "rpg/art/art_relief.h"

namespace art {

namespace {
inline uint32_t hash32u(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
  return x;
}
struct Info { int w, h, frames; };
Info reliefInfo(Prop p) {
  switch (p) {
    case Prop::SummitShrine: return {48, 58, 4};
    case Prop::Cairn: return {16, 22, 1};
    case Prop::PrayerFlags: return {36, 30, 4};
    default: return {16, 16, 1};
  }
}

// weathered summit stone: a cool grey with warm lights (the cliffs' rock), and its moss / lichen
const Ramp kSum = ramp5(rgba(46, 44, 64), rgba(78, 76, 96), rgba(116, 112, 124), rgba(156, 150, 150), rgba(198, 192, 180));

// a block seen in the 3/4 view: its top (lit), its south face (mid), its east face (shade). x0..x1 the front's span,
// yTop the top's back edge, yFront the top's front edge (= the face's top), yBase the face's foot; depth px of the
// east face shown (the block's side), flagstones on the top, courses on the face.
void block(Canvas& c, int x0, int x1, int yTop, int yFront, int yBase, int side, const Ramp& R, uint32_t seed, bool flags) {
  for (int y = yTop; y < yFront; y++)
    for (int x = x0; x <= x1 + side; x++) {
      // the top: a parallelogram leaning right as it goes back (the east side shows)
      const int back = (yFront - y) * side / std::max(1, yFront - yTop);
      if (x < x0 + back || x > x1 + back) continue;
      int k = 3;
      if (y == yTop) k = 4;
      if (flags) {   // flagstones: joints every few px, slabs shading a step apart
        const int fx = (x - back - x0), fy = y - yTop;
        const int row = fy / 4, col = (fx + (row & 1) * 3) / 6;
        if (fy % 4 == 3 || (fx + (row & 1) * 3) % 6 == 5) k = 2;
        else if (hashf(col, row, seed) < 0.3f) k = 4;
      }
      if (hashf(x, y, seed + 1) < 0.05f) k = std::max(1, k - 1);
      c.set(x, y, R[k]);
    }
  for (int y = yFront; y <= yBase; y++) {
    for (int x = x0; x <= x1; x++) {   // the south face
      int k = 2;
      if (y == yFront) k = 3;                                  // its lit arris
      else if (y == yBase) k = 1;                              // the contact line
      else if (((y - yFront) % 4) == 0 && hashf(x / 5, y, seed + 2) < 0.7f) k = 1;   // courses
      else if (((x + (y - yFront) / 4 * 3) % 7) == 0) k = 1;   // joints
      if (hashf(x, y, seed + 3) < 0.06f) k = std::min(3, k + 1);
      c.set(x, y, R[k]);
    }
    for (int i = 1; i <= side; i++) {   // the east face, receding up-right
      const int yy = y - i * (yFront - yTop) / std::max(1, side) / 2;
      if (yy < yTop + 1) continue;
      c.set(x1 + i, yy, R[i == side ? 0 : 1]);
    }
  }
}

void mossOn(Canvas& c, int x0, int y0, int x1, int y1, uint32_t seed, float amount) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      if (!solid(c, x, y)) continue;
      const float n = vnoise(x / 2.4f, y / 2.4f, seed);
      if (n < 1.0f - amount) continue;
      c.set(x, y, kMoss[n > 1.0f - amount * 0.4f ? 3 : 2]);
    }
}

// a brazier: a stone pedestal with a bronze bowl and its fire (frame f flickers)
void brazier(Canvas& c, int cx, int by, int f) {
  // the pedestal
  for (int y = by - 7; y <= by; y++)
    for (int x = cx - 2; x <= cx + 2; x++) c.set(x, y, kSum[x == cx - 2 ? 4 : x == cx + 2 ? 1 : (y == by ? 1 : 3)]);
  for (int x = cx - 3; x <= cx + 3; x++) c.set(x, by, kSum[x == cx + 3 ? 0 : 1]);   // its plinth
  // the bowl: bronze, lit on the left, coals within
  for (int x = cx - 4; x <= cx + 4; x++) {
    c.set(x, by - 8, kBrass[x <= cx - 2 ? 4 : x >= cx + 3 ? 1 : 3]);
    c.set(x, by - 9, kBrass[x <= cx - 2 ? 3 : x >= cx + 3 ? 0 : 2]);
  }
  for (int x = cx - 3; x <= cx + 3; x++) c.set(x, by - 10, (x + f) % 3 == 0 ? kFire[3] : kFire[1]);
  c.set(cx - 4, by - 10, kBrass[2]);
  c.set(cx + 4, by - 10, kBrass[1]);
  // the fire: tongues of three heights shifting frame to frame
  static const int TH[4][5] = {{2, 5, 7, 4, 2}, {3, 6, 5, 6, 2}, {2, 4, 8, 5, 3}, {3, 7, 6, 4, 2}};
  for (int i = 0; i < 5; i++) {
    const int x = cx - 2 + i, th = TH[f & 3][i];
    for (int j = 0; j < th; j++) {
      const float t = (float)j / (float)std::max(1, th - 1);
      const int k = t < 0.25f ? 4 : t < 0.55f ? 3 : t < 0.8f ? 2 : 1;
      c.set(x, by - 11 - j, kFire[k]);
    }
  }
  if (f & 1) c.set(cx + (f == 1 ? 1 : -1), by - 20, kFire[2]);   // a spark
}

void summitShrine(Canvas& c, int f) {
  const int W = c.w, H = c.h, by = H - 1;
  const uint32_t sd = 9101u;
  // ---- the dais: two tiers of old flagstones (the lower one fills the 3x2 footprint)
  block(c, 1, 43, by - 27, by - 6, by, 3, kSum, sd, true);
  block(c, 7, 37, by - 31, by - 15, by - 11, 3, kSum, sd + 10, true);
  // steps cut into the lower tier's face (the way up from the south), worn in the middle
  for (int s = 0; s < 2; s++) {
    const int y0 = by - 5 + s * 3;
    for (int x = 17 - s * 2; x <= 27 + s * 2; x++) {
      c.set(x, y0, kSum[x < 20 ? 4 : 3]);
      c.set(x, y0 + 1, kSum[2]);
      c.set(x, y0 + 2, kSum[1]);
    }
  }
  // ---- the shrine on the upper tier: four squat pillars and a heavy roof
  const int py0 = by - 44, py1 = by - 16;
  auto pillar = [&](int x, bool front) {
    for (int y = py0 + 4; y <= py1; y++) {
      c.set(x, y, kSum[4]);
      c.set(x + 1, y, kSum[3]);
      c.set(x + 2, y, kSum[2]);
      c.set(x + 3, y, kSum[front ? 1 : 0]);
      if (((y - py0) % 7) == 0) { c.set(x + 1, y, kSum[2]); c.set(x + 2, y, kSum[1]); }   // drums
    }
    for (int i = -1; i <= 4; i++) { c.set(x + i, py1, kSum[i < 1 ? 3 : 1]); c.set(x + i, py0 + 4, kSum[i < 1 ? 4 : 2]); }   // base, capital
  };
  // the back pillars (in the roof's shade)
  for (int y = py0 + 4; y <= py1 - 5; y++) { c.set(15, y, kSum[1]); c.set(16, y, kSum[0]); c.set(31, y, kSum[1]); c.set(32, y, kSum[0]); }
  // the dark within
  for (int y = py0 + 5; y <= py1 - 5; y++)
    for (int x = 17; x <= 30; x++) c.set(x, y, y < py0 + 8 ? kPurple[0] : mix(kSum[0], kPurple[0], 0.5f));
  // ---- the idol: a weathered seated figure, a rounded head and shoulders, its face worn smooth
  {
    const int ix = 23, iy = py1 - 6;
    ellipse(c, ix + 0.5, iy - 2, 5.2, 4.5, kSum[2]);            // the lap / shoulders
    for (int y = iy - 10; y <= iy - 3; y++)
      for (int x = ix - 3; x <= ix + 4; x++) {
        const float dx = (x + 0.5f - (ix + 0.5f)) / 3.8f, dy = (y + 0.5f - (iy - 7.0f)) / 4.2f;
        if (dx * dx + dy * dy > 1.0f) continue;
        c.set(x, y, kSum[lightIndex(lightAt(dx * 0.9f, dy * 0.9f), x, y, 0.08f)]);
      }
    for (int y = iy - 15; y <= iy - 10; y++)                     // the head
      for (int x = ix - 2; x <= ix + 3; x++) {
        const float dx = (x + 0.5f - (ix + 0.5f)) / 2.8f, dy = (y + 0.5f - (iy - 12.5f)) / 3.0f;
        if (dx * dx + dy * dy > 1.0f) continue;
        c.set(x, y, kSum[lightIndex(lightAt(dx * 0.9f, dy * 0.9f), x, y, 0.08f)]);
      }
    c.set(ix, iy - 12, kSum[1]); c.set(ix + 2, iy - 12, kSum[1]);   // the worn eyes
    c.set(ix + 1, iy - 7, kGold[3]);                                // a gilt mark on the breast
    mossOn(c, ix - 4, iy - 4, ix + 5, iy + 2, sd + 21, 0.3f);
  }
  // ---- the altar before it, and its offerings
  for (int y = py1 - 5; y <= py1 + 1; y++)
    for (int x = 18; x <= 29; x++) c.set(x, y, kSum[y == py1 - 5 ? 4 : y == py1 - 4 ? 3 : x == 29 ? 1 : (y == py1 + 1 ? 1 : 2)]);
  for (int x = 20; x <= 24; x++) { c.set(x, py1 - 4, kRed[x == 20 ? 3 : 2]); for (int y = py1 - 3; y <= py1; y++) c.set(x, y, kRed[x == 24 ? 0 : 1]); }   // a red cloth hanging
  c.set(19, py1 - 6, kBrass[3]); c.set(20, py1 - 6, kBrass[2]); c.set(19, py1 - 7, kBrass[4]);   // a bowl
  c.set(26, py1 - 6, kGold[4]); c.set(27, py1 - 6, kGold[2]);                                     // coins
  c.set(28, py1 - 6, kBone[4]); c.set(28, py1 - 7, kBone[3]); c.set(28, py1 - 8, (f & 1) ? kFire[4] : kFire[3]);   // a candle
  // ---- the front pillars
  pillar(10, true);
  pillar(34, true);
  // ---- the roof: a heavy slab with a pitched top, a ridge and finials (front fascia, lit top, shaded east end)
  {
    const int ry0 = py0 - 6, ry1 = py0 + 4;   // ridge .. eave
    for (int y = ry0; y <= ry1; y++) {
      const float t = (float)(y - ry0) / (float)(ry1 - ry0);
      const int half = (int)std::lround(9 + t * 14);
      for (int x = 24 - half; x <= 23 + half; x++) {
        int k = y >= ry1 - 2 ? (y == ry1 ? 1 : 2) : (t < 0.15f ? 4 : 3);   // the fascia below, the lit pitch above
        if (y < ry1 - 2 && x > 23 + half - 3) k = 1;                       // the east hip in shade
        if (y < ry1 - 2 && x < 24 - half + 2) k = 4;
        if (y < ry1 - 2 && ((x + y) % 4 == 0)) k = std::max(1, k - 1);     // the stone tiles' courses
        if (hashf(x, y, sd + 31) < 0.05f) k = std::max(1, k - 1);
        c.set(x, y, kSum[k]);
      }
    }
    for (int x = 15; x <= 32; x++) c.set(x, ry0 - 1, kSum[x < 22 ? 4 : 3]);   // the ridge
    // finials: a little horn at each end of the ridge
    c.set(14, ry0 - 2, kSum[4]); c.set(13, ry0 - 3, kSum[3]); c.set(33, ry0 - 2, kSum[2]); c.set(34, ry0 - 3, kSum[1]);
    // the roof's shadow on the front of the dark within and the pillars' tops
    for (int x = 12; x <= 37; x++) { const uint32_t p = c.get(x, ry1 + 1); if (chA(p)) c.set(x, ry1 + 1, darken(p, 0.5f)); }
    mossOn(c, 2, ry0, W - 3, ry1 - 3, sd + 33, 0.22f);
    for (int x = 2; x < W - 2; x++) if (hashf(x, 3, sd + 35) < 0.18f && solid(c, x, ry1)) c.set(x, ry1 + 1, kSum[1]);   // drips of weather
  }
  // ---- weathering: cracks across the dais, lichen at its foot
  for (int i = 0; i < 6; i++) {
    int x = 4 + (int)(hash3(i, 1, sd) % 38), y = by - 26 + (int)(hash3(i, 2, sd) % 18);
    for (int k = 0; k < 4; k++) { if (solid(c, x, y)) c.set(x, y, kSum[1]); x += (int)(hash3(i, k, sd + 5) % 3) - 1; y++; }
  }
  mossOn(c, 1, by - 8, W - 2, by, sd + 41, 0.30f);
  // ---- the braziers on the lower tier's front corners
  brazier(c, 5, by - 9, f);
  brazier(c, 40, by - 9, (f + 2) & 3);
}

void cairn(Canvas& c) {
  const int W = c.w, by = c.h - 2;
  const float cx = W * 0.5f - 0.5f;
  // six flat stones, wide at the foot, each its own grey, the stack leaning a touch east
  struct S { float w, h; int tint; };
  const S st[6] = {{6.4f, 2.6f, 0}, {5.6f, 2.4f, 1}, {4.8f, 2.2f, 2}, {4.0f, 2.0f, 0}, {3.2f, 1.8f, 1}, {2.2f, 1.6f, 2}};
  const Ramp R1 = ramp5(rgba(52, 48, 66), rgba(88, 84, 100), rgba(128, 122, 130), rgba(168, 160, 156), rgba(206, 200, 186));
  const Ramp R2 = ramp5(rgba(56, 52, 60), rgba(96, 88, 92), rgba(138, 128, 122), rgba(178, 168, 152), rgba(214, 206, 182));
  float y = (float)by - st[0].h;
  for (int i = 0; i < 6; i++) {
    const float x = cx + i * 0.35f + (hashf(i, 0, 9301) - 0.5f) * 0.9f;
    const Ramp& R = st[i].tint == 1 ? R2 : st[i].tint == 2 ? kSum : R1;
    ball(c, x, y, st[i].w, st[i].h, R, 0.08f);
    // the flat top of each stone catches the light
    for (int xx = (int)std::floor(x - st[i].w * 0.6f); xx <= (int)std::ceil(x + st[i].w * 0.2f); xx++) dotOn(c, xx, (int)std::floor(y - st[i].h + 0.6f), R[4]);
    y -= st[i].h * 1.55f;
  }
  // a white pebble on top, lichen, a crack of shade between stones
  c.set((int)cx + 2, (int)y + 1, kBone[4]);
  c.set((int)cx + 2, (int)y + 2, kBone[2]);
  for (int i = 0; i < 7; i++) dotOn(c, (int)cx - 4 + (int)(hash3(i, 3, 9303) % 9), by - 2 - (int)(hash3(i, 4, 9303) % 12), (i & 1) ? rgba(186, 178, 84) : kMoss[3]);
  // grass at the foot
  for (int x = 1; x < W - 1; x++) {
    if (hashf(x, 0, 9305) > 0.55f) continue;
    const int h = 1 + (int)(hash3(x, 1, 9305) % 3);
    for (int j = 0; j < h; j++) c.set(x, by + 1 - j, kLeaf[j == h - 1 ? 3 : 2]);
  }
}

void prayerFlags(Canvas& c, int f) {
  const int W = c.w, H = c.h, by = H - 2;
  // two poles, leaning a little apart, on footings of stones
  const int lx = 3, rx = W - 5, top = 4;
  auto pole = [&](int x, int lean) {
    for (int y = top; y <= by; y++) {
      const int xx = x + (by - y) * lean / 26;
      c.set(xx, y, kWood[4]);
      c.set(xx + 1, y, kWood[2]);
    }
    c.set(x + (by - top) * lean / 26, top - 1, kWood[3]);                 // the pole's cap
    ball(c, x + 0.5, by - 0.5, 2.6, 1.6, kSum, 0.08f);                     // its footing
    ball(c, x + 2.0, by + 0.2, 1.8, 1.2, kSum, 0.08f, 0);
  };
  pole(lx, -1);
  pole(rx, 1);
  // the line: a sagging curve between the pole tops, swaying a pixel with the frame
  const float x0 = lx + 1.0f - 1.0f, x1 = rx + 1.0f + 1.0f, y0 = top + 1.0f;
  const float sag = 6.0f + ((f == 1 || f == 2) ? 1.0f : 0.0f);
  auto lineY = [&](float x) { const float t = (x - x0) / (x1 - x0); return y0 + sag * 4.0f * t * (1 - t); };
  for (int x = (int)x0 + 1; x < (int)x1; x++) c.set(x, (int)std::floor(lineY((float)x)), kWood[1]);
  // the flags: blue, white, red, green, yellow, faded by the weather; each a cloth hanging from the line, rippling
  const uint32_t FC[5][3] = {{rgba(60, 92, 168), rgba(88, 126, 200), rgba(120, 160, 222)},
                             {rgba(188, 186, 190), rgba(222, 220, 214), rgba(246, 244, 236)},
                             {rgba(150, 46, 46), rgba(192, 70, 58), rgba(222, 108, 84)},
                             {rgba(52, 118, 70), rgba(78, 152, 84), rgba(118, 186, 106)},
                             {rgba(186, 142, 46), rgba(222, 184, 68), rgba(244, 216, 110)}};
  for (int i = 0; i < 7; i++) {
    const int fx = (int)x0 + 3 + i * 4;
    const int ph = (f + i * 3) & 3;                 // each flag on its own phase
    const int len = 5 + ((ph == 1) ? 1 : 0), lift = ph == 2 ? 1 : 0;
    const uint32_t* col = FC[i % 5];
    for (int j = 1; j <= len - lift; j++) {
      const int sway = (ph == 0 ? (j > 3 ? 1 : 0) : ph == 2 ? (j > 2 ? 1 : 0) : ph == 3 ? -(j > 3 ? 1 : 0) : 0);
      const int yy = (int)std::floor(lineY((float)fx + 1.0f)) + j;
      for (int k = 0; k < 3; k++) {
        const int x = fx + k + sway;
        int t = k == 0 ? 2 : k == 1 ? 1 : 0;      // lit on the left
        if (((j + ph + k) % 3) == 0) t = std::max(0, t - 1);   // the ripple's folds
        c.set(x, yy, col[t]);
      }
    }
    // the frayed tail
    if (hashf(i, f, 9401) < 0.5f) c.set(fx + 1 + (ph == 0 ? 1 : 0), (int)std::floor(lineY((float)fx + 1.0f)) + len - lift + 1, col[0]);
  }
}
}  // namespace

// ---- the waterfalls' frames (render_relief.cpp bakes them per fall tile; relief_gallery previews them)
namespace {
constexpr int NF = FALL_FRAMES;
inline uint32_t hh(int a, int b, uint32_t s) { return hash32u((uint32_t)a * 73856093u ^ (uint32_t)b * 19349663u ^ s * 83492791u); }
inline float hf(int a, int b, uint32_t s) { return (float)(hh(a, b, s) & 0xFFFFFF) / 16777216.0f; }
inline int pmod(int a, int m) { const int r = a % m; return r < 0 ? r + m : r; }
inline uint32_t fa(uint32_t c, float a) { return (c & 0x00FFFFFFu) | ((uint32_t)std::clamp((int)(a * 255.0f), 0, 255) << 24); }
// the falling water's ramp (hue-shifted: shadows to blue-violet, lights to a cool white), beside the river's own
// (50,108,163) so the sheet sits in the stream; and the ice's
const uint32_t WP[7] = {rgba(30, 58, 104), rgba(42, 86, 140), rgba(60, 120, 180), rgba(98, 156, 206), rgba(152, 200, 234), rgba(216, 234, 248), rgba(248, 252, 255)};
const uint32_t IP[6] = {rgba(66, 92, 146), rgba(104, 144, 196), rgba(146, 186, 226), rgba(192, 222, 244), rgba(226, 241, 252), rgba(246, 251, 255)};
}  // namespace

// ------------------------------------------------------------------------------------------------ painting a fall
// The tile's columns (a north or south fall) or rows (a side cascade): where the step's contour crosses them and
// whether water runs over it there.

// a fall from the north: the sheet down the face toward the viewer. lips[i] for the columns i = -3..18 of the tile
// (window px y of the first face pixel), wet where water lies above the lip and below the foot.
void paintFallNorth(Canvas& c, int f, int ox, int oy, int x0, const FallSpan* col, int fh, int drop, bool frozen, int gox, int goy) {
  const int FW = frozen ? c.w : c.w / FALL_FRAMES;
  auto set = [&](int x, int y, uint32_t v) {   // window px -> this frame's cell
    const int cx = x - ox, cy = y - oy;
    if (cx < 0 || cy < 0 || cx >= FW || cy >= c.h) return;
    c.set(cx + f * FW, cy, v);
  };
  auto get = [&](int x, int y) -> uint32_t {
    const int cx = x - ox, cy = y - oy;
    if (cx < 0 || cy < 0 || cx >= FW || cy >= c.h) return 0;
    return c.get(cx + f * FW, cy);
  };
  // the valid span inside the tile (the run's ends: lit on the left, shaded on the right)
  for (int i = 0; i < 16; i++) {
    const FallSpan& s = col[i + 3];
    if (!s.wet) continue;
    const int x = x0 + i, gx = x + gox * 16;
    const bool leftEnd = !col[i + 2].wet, rightEnd = !col[i + 4].wet;
    const uint32_t hc = hh(gx, 0, 4111);
    const int lip = s.at;
    if (frozen) {
      // ice: columns 2-4 px wide hanging from the lip, each lit on its west edge and shaded on its east, ending in
      // icicles of their own length; a frozen splash mound swelling at the foot
      const int grp = gx / 3, inG = pmod(gx, 3);
      const uint32_t hg = hh(grp, 0, 4113);
      const int len = fh - (int)(hg % 4);
      for (int d = -2; d <= fh + 4; d++) {
        const int y = lip + d;
        int k;
        if (d < 0) k = d == -1 ? 5 : 4;                       // the frozen lip: a bright rounded cornice
        else if (d <= len) {
          k = inG == 0 ? 4 : inG == 1 ? 3 : 2;
          if (leftEnd && i == 0) k = std::min(5, k + 1);
          if (rightEnd) k = std::max(1, k - 1);
          if (d > fh * 0.6f && hf(gx, y, 4115) < 0.2f) k = std::min(5, k + 1);      // rime
          if (((gx * 3 + y * 2) % 19) == 0 && hf(gx / 4, y / 6, 4117) < 0.4f) k = 0;  // a crack
        } else if (d <= fh) {   // the icicle tips between the columns
          if (inG == 2 || hf(gx, d, 4119) < 0.5f) continue;
          k = 3;
        } else {   // the frozen splash mound
          const int dd = d - fh;
          const float wob = vnoise(gx / 3.0f, 0, 4121) * 2.0f;
          if (dd > 2 + wob) continue;
          k = dd == 0 ? 5 : dd == 1 ? 4 : 3;
        }
        set(x, y, IP[k]);
      }
      // wet-dark rock beside the run's ends: frost instead
      if (leftEnd) for (int d = 1; d < fh; d += 2) set(x - 1, lip + d, fa(IP[4], 0.55f));
      if (rightEnd) for (int d = 1; d < fh; d++) set(x + 1, lip + d, fa(IP[1], 0.35f));
      continue;
    }
    // flowing water. The sheet hangs in RIBBONS three px wide (by the global px: a wide fall is one curtain), each a
    // tone of its own with a darker gap column here and there; chunky streak dashes as wide as their ribbon run down
    // them and stretch as the water speeds up; the lower sheet whitens in bands, not speckle; at the foot a boil of
    // rounded foam blobs heaves frame by frame (smooth noise round a loop, so the 6 frames cycle seamlessly), a ring of
    // foam spreads on the pool, and foam trails drift downstream.
    const int rib = gx >= 0 ? gx / 3 : (gx - 2) / 3, inR = pmod(gx, 3);
    const uint32_t hr = hh(rib, 0, 4111);
    int base = (hr % 3) == 0 ? 3 : 2;
    if (inR == 2 && (hr & 8)) base = 1;                                   // a gap between ribbons
    const int ph = (int)((hr >> 4) % 12), len = 4 + (int)((hr >> 8) % 3);
    const float ang = 6.2831853f * (float)f / (float)FALL_FRAMES;
    auto loopN = [&](float u, float v, uint32_t sd) { return vnoise(u + 1.4f * std::cos(ang), v + 1.4f * std::sin(ang), sd); };
    const int boil = 1 + drop;                                             // the boil's height above the foot
    const int foamTop = fh - boil - (int)std::lround(3.2f * loopN(gx / 2.1f, 3.0f, 4129) * loopN(gx / 5.0f, 5.0f, 4130) * 1.6f);
    const int foamBot = fh + 1 + drop + (int)std::lround(2.4f * loopN(gx / 2.7f, 9.0f, 4131));
    (void)hc;
    for (int d = -3; d <= foamBot + 7; d++) {
      const int y = lip + d;
      const float t = d / (float)fh;
      int k = -1;
      float a = 1.0f;
      if (d == -3) { if (pmod(rib + ph - f, 3) == 0) { k = 3; a = 0.5f; } }   // the river quickening toward the lip
      else if (d == -2) { k = 3; a = 0.85f; }
      else if (d == -1) k = (hr & 48) == 0 ? 5 : 4;                        // the glassy curl's crest, a glint here and there
      else if (d == 0) k = 3;
      else if (d == 1) k = (hr & 3) == 0 ? 2 : 1;                          // the curl's shaded underside
      else if (d < foamTop) {
        k = base;
        const float yy = d + (float)(d * d) / (2.0f * fh);                 // the water speeds up as it falls
        const int sp = pmod((int)yy + ph - f * 2, 12);
        const int ln = len + (t > 0.5f ? 2 : 0);
        if (sp < ln && inR < 2) k = base + (sp == ln - 1 && inR == 1 ? 2 : 1);   // a dash, its leading end the brightest
        if (t > 0.55f) k = std::max(k, base + 1);                          // the lower sheet whitening
        if (t > 0.8f) k = std::max(k, 4);
        if (leftEnd && i == 0) k++;
        if (rightEnd) k--;
        k = std::clamp(k, 0, 5);
      } else if (d <= foamBot) {   // the boil: rounded blobs, lit caps, a soft lower edge
        k = d == foamTop ? 6 : d == foamBot ? 4 : 5;
        if (d > foamTop && d < foamBot && loopN(gx / 2.0f, d / 2.0f, 4133) < 0.32f) k = 4;   // the folds between blobs
      } else {
        const int u = d - foamBot;
        if (u <= 4) {   // the ring of foam spreading on the pool
          const float n = loopN(gx / 3.2f, d / 2.2f, 4135);
          if (n > 0.56f + 0.06f * u) { k = n > 0.70f + 0.06f * u ? 5 : 4; a = 0.9f - 0.1f * u; }
        } else if (pmod(u - f, 6) == 0 && hf(gx / 3, (u - f + 60) / 6, 4137) < 0.4f) { k = 5; a = 0.6f; }   // trails
      }
      if (k >= 0) set(x, y, fa(WP[k], a));
    }
    // the run's ends: wet dark rock beside the sheet, the boil swelling a px or two over the bank
    if (leftEnd)
      for (int d = 2; d < foamTop; d++) if (!get(x - 1, lip + d)) set(x - 1, lip + d, rgba(16, 24, 46, 90));
    if (rightEnd)
      for (int d = 2; d < foamTop; d++) if (!get(x + 1, lip + d)) set(x + 1, lip + d, rgba(16, 24, 46, 110));
    for (int dx = -2; dx <= 2; dx++) {
      if (dx == 0 || (dx < 0 && !leftEnd) || (dx > 0 && !rightEnd)) continue;
      const int top = foamTop + 1 + std::abs(dx), bot = foamBot - std::abs(dx) + 1;
      for (int d = top; d <= bot; d++)
        if (loopN((gx + dx) / 2.0f, d / 2.0f, 4139) > 0.38f) set(x + dx, lip + d, WP[d == top ? 6 : 5]);
    }
    // a few drops of spray flung up over the boil
    for (int d = foamTop - 4; d < foamTop; d++)
      if (hf(gx, lip + d, 4141u + (uint32_t)f * 13u) < 0.035f) set(x, lip + d, fa(WP[6], 0.9f));
  }
}

// a side cascade (dir 1 from the west, 2 from the east): rows of the tile, the contour x of each, water tumbling over
// the side wall in a white chute, foam boiling at its downstream edge
void paintFallSide(Canvas& c, int f, int ox, int oy, int y0, const FallSpan* row, int dir, int drop, bool frozen, int gox, int goy) {
  const int FW = frozen ? c.w : c.w / FALL_FRAMES;
  auto set = [&](int x, int y, uint32_t v) {
    const int cx = x - ox, cy = y - oy;
    if (cx < 0 || cy < 0 || cx >= FW || cy >= c.h) return;
    c.set(cx + f * FW, cy, v);
  };
  const int sg = dir == 1 ? 1 : -1;            // the way the water flows
  const int SW = 6 + 3 * (drop - 1);           // the chute over the side wall
  const int lit = dir == 2 ? 1 : 0;            // a west-facing wall (from the east) is in the light
  for (int j = 0; j < 16; j++) {
    const FallSpan& s = row[j + 3];
    if (!s.wet) continue;
    const int y = y0 + j, gy = y + goy * 16;
    const bool topEnd = !row[j + 2].wet, botEnd = !row[j + 4].wet;
    const uint32_t hr = hh(0, gy, 4141);
    const int ph = (int)(hr % 12), len = 3 + (int)((hr >> 8) % 4);
    for (int u = -3; u <= SW + 5; u++) {
      const int x = s.at + sg * u;
      int k = -1;
      float a = 1.0f;
      if (frozen) {
        if (u < 0) k = 4;
        else if (u <= SW) k = std::clamp(2 + lit + ((u + (gy / 3)) % 3 == 0 ? 1 : 0) - (botEnd ? 1 : 0), 0, 5);
        else if (u <= SW + 2) k = 4;
        if (k >= 0) set(x, y, IP[k]);
        continue;
      }
      if (u < 0) { if (u == -1) k = 4; else if (u == -2) { k = 3; a = 0.7f; } }
      else if (u <= SW) {
        const float t = u / (float)SW;
        k = 2 + lit;
        const int sp = pmod(u + ph - f * 2, 12);
        if (sp < len) k = std::min(5, k + 1 + (sp == 0 ? 1 : 0));
        if (t > 0.45f && hf(x * 3 + f * 17, gy, 4143) < (t - 0.45f) * 1.6f) k = std::max(k, 5);
        if (topEnd) k = std::min(6, k + 1);
        if (botEnd) k = std::max(0, k - 1);
        if (j > 0 && ((gy + f) % 5) == 0 && hf(gy, u, 4145) < 0.3f) k = std::max(1, k - 1);   // the tumbling's troughs
      } else {
        const int dd = u - SW;
        const float n = 0.5f * vnoise(dd / 1.7f, gy / 2.3f, 4147u + (uint32_t)f) + 0.5f * hf(x, gy, 4149u + (uint32_t)f * 7u);
        const float thr = 0.2f + 0.14f * dd;
        if (n < thr) continue;
        k = n > thr + 0.28f ? 6 : n > thr + 0.1f ? 5 : 4;
      }
      if (k >= 0) set(x, y, fa(WP[k], a));
    }
    if (hf(gy, 7, 4151u + (uint32_t)f) < 0.35f) set(s.at + sg * (SW + 1 + (int)(hr % 3)), y - 1 - (int)((hr >> 4) % 3), fa(WP[6], 0.85f));   // spray
  }
}

// a fall toward the north (the higher ground south of it): the water runs over the rim and drops out of sight behind
// it; its curl is the last thing seen and its spray rises beyond the lip
void paintFallRim(Canvas& c, int f, int ox, int oy, int x0, const FallSpan* col, bool frozen, int gox, int goy) {
  const int FW = frozen ? c.w : c.w / FALL_FRAMES;
  auto set = [&](int x, int y, uint32_t v) {
    const int cx = x - ox, cy = y - oy;
    if (cx < 0 || cy < 0 || cx >= FW || cy >= c.h) return;
    c.set(cx + f * FW, cy, v);
  };
  (void)goy;
  for (int i = 0; i < 16; i++) {
    const FallSpan& s = col[i + 3];
    if (!s.wet) continue;
    const int x = x0 + i, gx = x + gox * 16;
    const uint32_t hc = hh(gx, 0, 4161);
    const int ph = (int)(hc % 12), len = 2 + (int)((hc >> 8) % 3);
    const int lipY = s.at - 6;   // the rim's lip (terrain: a plateau to the south reaches up over its north neighbour)
    for (int d = -12; d <= 5; d++) {
      const int y = lipY + d;   // d < 0 above (north of) the lip, d > 0 the plateau's water
      int k = -1;
      float a = 1.0f;
      if (frozen) {
        if (d == 0) k = 5;
        else if (d > 0 && d <= 2) k = 4;
        else if (d < 0 && d >= -2 && hf(gx, d, 4163) < 0.6f) k = 3;
        if (k >= 0) set(x, y, IP[k]);
        continue;
      }
      if (d > 0) {   // streaks running north (up) into the lip
        k = 3;
        a = 0.35f + 0.1f * (5 - d);
        if (pmod(d + ph + f * 2, 12) < len) { k = 4; a = 0.85f; }
      } else if (d == 0) k = hf(gx, 2, 4165) < 0.2f ? 6 : 5;
      else {   // spray rising from the hidden fall
        const int up = -d;
        const float n = 0.5f * vnoise(gx / 2.4f, (up + f) / 2.0f, 4167u + (uint32_t)f) + 0.5f * hf(gx, y, 4169u + (uint32_t)f * 5u);
        const float thr = 0.25f + 0.07f * up;
        if (n < thr) continue;
        k = n > thr + 0.25f ? 6 : 5;
        a = std::max(0.2f, 0.95f - up * 0.07f);
      }
      if (k >= 0) set(x, y, fa(WP[k], a));
    }
  }
}

int reliefPropW(Prop p) { return reliefInfo(p).w; }
int reliefPropH(Prop p) { return reliefInfo(p).h; }
int reliefPropFrames(Prop p) { return reliefInfo(p).frames; }

void paintReliefProp(Canvas& c, Prop p, int frame) {
  switch (p) {
    case Prop::SummitShrine: summitShrine(c, frame); break;
    case Prop::Cairn: cairn(c); break;
    case Prop::PrayerFlags: prayerFlags(c, frame); break;
    default: break;
  }
}

}  // namespace art
