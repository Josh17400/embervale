// EMBERVALE art, M7b "Cities" (rpg/art/art_city.h): the city props, CITYART lane. Street furniture, gardens, parks and
// souks in the game's high 3/4 view (a point at ground (x, y) and height z is drawn at (x, y - z)), light from the
// top-left: tops and west ends lit, south faces mid-tone, east ends in shade, cast shadows to the lower right.
// The culture (PropStyle::culture, cult::Archetype + 1) picks the materials and the forms: a heartland garden wall of
// grey ashlar under a lit coping, the jade kingdoms' grey brick under green glazed tiles, the dune folk's whitewashed
// adobe, a fjord runestone for a monument, a jade pavilion with red pillars and upturned eaves, a souk's striped cloth.
#include <algorithm>
#include <cmath>
#include <vector>
#include "rpg/art/art_city.h"
#include "rpg/art/art_internal.h"

namespace art {

namespace {
struct Sz { int w, h, frames; };
Sz cityInfo(Prop p) {
  switch (p) {
    case Prop::Bollard: return {10, 16, 1};
    case Prop::Planter: return {16, 22, 1};
    case Prop::GardenWall: return {16, 28, 1};
    case Prop::Hedge: return {16, 28, 1};
    case Prop::FlowerBed: return {16, 16, 1};
    case Prop::FruitTree: return {28, 36, 1};
    case Prop::Monument: return {24, 64, 1};
    case Prop::Pavilion: return {48, 72, 1};
    case Prop::Canopy: return {16, 40, 1};
    default: return {16, 16, 1};
  }
}

enum Cul : int { CU_NONE, CU_FJORD, CU_HIGHLAND, CU_HEART, CU_IMPERIAL, CU_DUNE, CU_STEPPE, CU_MARSH, CU_JADE, CU_RIVER, CU_SUN, CU_SYLVAN, CU_STAR };

const Ramp kJadeTile = ramp5(rgba(14, 50, 56), rgba(24, 88, 80), rgba(40, 132, 104), rgba(88, 180, 132), rgba(178, 230, 182));
const Ramp kTerracotta = ramp5(rgba(84, 36, 34), rgba(136, 58, 40), rgba(184, 88, 54), rgba(214, 126, 78), rgba(238, 170, 116));
const Ramp kAdobe = ramp5(rgba(120, 90, 74), rgba(176, 140, 108), rgba(214, 186, 148), rgba(236, 214, 178), rgba(250, 238, 212));
const Ramp kWhitewash = ramp5(rgba(120, 118, 132), rgba(176, 176, 184), rgba(214, 214, 214), rgba(236, 234, 228), rgba(252, 250, 242));
const Ramp kEarthWall = ramp5(rgba(80, 58, 46), rgba(122, 92, 66), rgba(156, 124, 88), rgba(184, 154, 112), rgba(212, 186, 142));
const Ramp kLacquer = ramp5(rgba(70, 18, 26), rgba(118, 26, 32), rgba(170, 40, 38), rgba(210, 74, 56), rgba(240, 128, 96));

struct Mat {
  Ramp stone, wood, cloth, metal, cope;
  int cul = CU_NONE;
};
Mat matOf(const PropStyle& st) {
  Mat m;
  m.cul = st.culture;
  m.stone = st.stone ? ramp(opaque(st.stone), 0.85f) : kStone;
  m.wood = st.wood ? ramp(opaque(st.wood)) : kWood;
  m.cloth = st.cloth ? ramp(opaque(st.cloth)) : kRed;
  m.metal = st.metal ? ramp(opaque(st.metal)) : kIron;
  m.cope = m.stone;
  switch (m.cul) {
    case CU_IMPERIAL: if (!st.stone) m.stone = kStoneWarm; m.cope = kTerracotta; break;
    case CU_DUNE: m.stone = kAdobe; m.cope = kAdobe; break;
    case CU_SUN: m.stone = kWhitewash; m.cope = kAdobe; break;
    case CU_STEPPE: m.stone = kEarthWall; m.cope = kEarthWall; break;
    case CU_JADE: if (!st.stone) m.stone = kStone; m.cope = kJadeTile; break;
    case CU_RIVER: m.stone = kBrick; m.cope = st.stone ? ramp(opaque(st.stone), 0.85f) : kStone; break;
    case CU_STAR: m.stone = kWhitewash; m.cope = kWhitewash; break;
    case CU_HEART: case CU_NONE: if (!st.stone) m.stone = kStoneWarm; m.cope = m.stone; break;
    default: break;
  }
  return m;
}
inline uint32_t shadowPx(int a) { return withA(rgba(40, 28, 70), a); }
// a soft ground shadow (an ellipse, darker at its heart), only on empty pixels; alpha <= 90 (the outline skips it)
void groundShadow(Canvas& c, float cx, float cy, float rx, float ry, int a = 84) {
  for (int y = (int)std::floor(cy - ry); y <= (int)std::ceil(cy + ry); y++)
    for (int x = (int)std::floor(cx - rx); x <= (int)std::ceil(cx + rx); x++) {
      const float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry, d = dx * dx + dy * dy;
      if (d > 1.0f || chA(c.get(x, y))) continue;
      if (d > 0.7f && ((x + y) & 1)) continue;   // a dithered penumbra
      c.set(x, y, shadowPx(d < 0.45f ? a : a * 3 / 4));
    }
}
// a 3/4 block: ground plan x0..x1, g0..g1 (canvas rows of the ground, g1 the front edge), height z px. The top (lit at
// its north and west rims), the south face (mid-tone, darker at the foot); the east rim shaded
void block(Canvas& c, int x0, int x1, int g0, int g1, int z, const Ramp& R, int topK = 3, bool courses = false) {
  for (int g = g0; g <= g1; g++)
    for (int x = x0; x <= x1; x++) {
      int k = topK;
      if (g == g0 || x == x0) k = std::min(4, topK + 1);
      else if (x == x1) k = std::max(1, topK - 1);
      c.set(x, g - z, R[k]);
    }
  for (int h = 0; h < z; h++)
    for (int x = x0; x <= x1; x++) {
      int k = h == z - 1 ? 3 : (h <= 1 ? 1 : 2);
      if (x == x0 && h < z - 1) k = std::min(3, k + 1);
      if (x == x1) k = std::max(0, k - 1);
      if (courses && h < z - 1 && h > 0 && (h % 3 == 0 || ((x + (h / 3) * 3) % 6 == 0 && h % 3 != 0))) k = std::max(0, k - 1);
      c.set(x, g1 - h, R[k]);
    }
}

// ---------------------------------------------------------------- joined runs (garden walls; hedges via hedgePiece)
// the run's plan in the tile's ground px (0..15): a band a0..a1 across, out to the joined sides; a pier's square where
// the run stops at a gateway or an end
bool runIn(int gx, int gy, int a0, int a1, int mask) {
  if (gx < 0 || gx > 15 || gy < 0 || gy > 15) return false;
  const bool cx = gx >= a0 && gx <= a1, cy = gy >= a0 && gy <= a1;
  if (cx && cy) return true;
  if (mask == 0) return false;
  if ((mask & 1) && cx && gy < a0) return true;
  if ((mask & 4) && cx && gy > a1) return true;
  if ((mask & 2) && cy && gx > a1) return true;
  if ((mask & 8) && cy && gx < a0) return true;
  return false;
}
Canvas gardenWallPiece(int bits, const Mat& M, bool snow) {
  const int W = 16, H = 28, OY = 12, HT = 10;   // canvas y = ground y + OY - z; the wall stands HT px tall
  const int mask = bits & 15, gates = (bits >> 4) & 15, ends = (bits >> 8) & 15;
  const int a0 = 5, a1 = 10;
  Canvas c(W, H);
  auto in = [&](int gx, int gy) { return runIn(gx, gy, a0, a1, mask); };
  // the piers: at the middle where the run ends or meets a gateway (and a lone pier), a little taller with a cap
  const bool pier = mask == 0 || gates || ends || ((mask & 5) && (mask & 10));   // (a corner or a T gets a pier too)
  const int PH = HT + 3;
  auto hgt = [&](int gx, int gy) { return (pier && gx >= a0 - 1 && gx <= a1 + 1 && gy >= a0 - 1 && gy <= a1 + 1) ? PH : HT; };
  auto inP = [&](int gx, int gy) { return in(gx, gy) || (pier && gx >= a0 - 1 && gx <= a1 + 1 && gy >= a0 - 1 && gy <= a1 + 1); };
  const bool brick = M.cul == CU_RIVER || M.cul == CU_JADE;
  const bool plaster = M.cul == CU_DUNE || M.cul == CU_SUN || M.cul == CU_STAR || M.cul == CU_STEPPE;
  // the cast shadow first (to the lower right on the ground), only where the wall does not stand
  for (int gy = 0; gy < 16; gy++)
    for (int gx = 0; gx < 16; gx++) {
      if (inP(gx, gy)) continue;
      bool sh = false;
      for (int d = 1; d <= 4 && !sh; d++) if (inP(gx - d, gy - (d * 3 + 2) / 5) || inP(gx - d + 1, gy - (d * 3 + 2) / 5 - 1)) sh = true;
      if (sh) c.set(gx, gy + OY, shadowPx(((gx + gy) & 1) ? 70 : 86));
    }
  // back to front so the nearer rows overwrite: the top, then the south face where the plan ends
  for (int gy = 0; gy < 16; gy++)
    for (int gx = 0; gx < 16; gx++) {
      if (!inP(gx, gy)) continue;
      const int z = hgt(gx, gy);
      const bool rimW = !inP(gx - 1, gy) || hgt(gx - 1, gy) < z, rimN = !inP(gx, gy - 1) || hgt(gx, gy - 1) < z;
      const bool rimE = !inP(gx + 1, gy) || hgt(gx + 1, gy) < z, rimS = !inP(gx, gy + 1) || hgt(gx, gy + 1) < z;
      // the coping, seen from above: long stones (or tiles, or a rounded plaster cap), lit along the north-west rim
      const uint32_t h = hash3(gx / 4, gy / 4, 331u);
      int k = 3;
      if (rimW || rimN) k = 4;
      else if (rimE || rimS) k = 2;
      uint32_t top = M.cope[k];
      if (M.cul == CU_JADE || M.cul == CU_IMPERIAL) {   // glazed or fired tiles in rows across the run
        const bool alongX = (mask & 10) || !(mask & 5);
        if (((alongX ? gx : gy) & 1) == 0 && !rimN && !rimW) top = M.cope[std::max(1, k - 1)];
      } else if (!plaster && (((mask & 10) ? gx : gy) % 5) == 0 && !rimW) top = M.cope[std::max(1, k - 2)];   // the joints
      if (snow) top = kSnow[(rimW || rimN) ? 4 : ((rimE || rimS) ? 2 : 3)];
      (void)h;
      c.set(gx, gy + OY - z, top);
      if (!rimS) continue;
      // the south face toward the camera: courses, darker toward the foot, the coping's drip line under its lip
      for (int hh = z - 1; hh >= 0; hh--) {
        const int y = gy + OY - hh;
        int kk;
        if (hh == z - 1) kk = snow ? -1 : 3;              // the coping's front edge
        else if (hh == z - 2) kk = 1;                     // its drip shadow on the face
        else kk = hh <= 1 ? 1 : 2;
        uint32_t col;
        if (kk < 0) col = kSnow[2];
        else if (hh >= z - 2) col = M.cope[kk];
        else {
          const int fh = hh;   // the face's courses
          if (brick) {
            const int crs = fh / 2;
            const bool jt = fh % 2 == 0 || (gx + (crs & 1) * 2) % 4 == 0;
            col = jt ? M.stone[1] : M.stone[(hash3(gx / 4, crs, 337u) % 5 == 0) ? 3 : kk];
          } else if (plaster) {
            col = M.stone[kk];
            if (hash3(gx, fh, 339u) % 11 == 0) col = M.stone[std::max(0, kk - 1)];   // the plaster's wear
            if (fh <= 1) col = M.stone[1];                                           // the splash at the foot
          } else {
            const int crs = fh / 3;
            const bool jt = fh % 3 == 0 || (gx + (crs & 1) * 3 + (int)(hash3(crs, gy, 341u) % 2)) % 6 == 0;
            col = jt ? M.stone[std::max(0, kk - 1)] : M.stone[(hash3(gx / 6, crs, 343u) % 4 == 0) ? std::min(3, kk + 1) : kk];
          }
        }
        if (rimW && hh < z - 1) col = lighten(col, 0.25f);   // the lit west end of the face
        if (rimE && hh < z - 1) col = darken(col, 0.30f);
        c.set(gx, y, col);
      }
    }
  // a pier's cap: a lit capstone (and a ball finial at a gateway)
  if (pier && !snow) {
    for (int x = a0 - 1; x <= a1 + 1; x++) c.set(x, a0 - 1 + OY - PH - 1, M.cope[x == a0 - 1 ? 4 : (x == a1 + 1 ? 2 : 3)]);
    if (gates) {
      const float bx = (a0 + a1) * 0.5f + 0.5f, by = (float)(a0 + OY - PH - 3);
      ball(c, bx, by, 2.2, 2.0, M.cope);
    }
  }
  // ivy over the warm stone walls of the heartland and the elves
  if (M.cul == CU_SYLVAN || M.cul == CU_HEART || M.cul == CU_NONE)
    for (int gx = 0; gx < 16; gx++) {
      const uint32_t h = hash3(gx / 3, M.cul, 347u);
      if (h % 4 != 0) continue;
      for (int y = 0; y < H; y++) {
        if (!chA(c.get(gx, y)) || chA(c.get(gx, y)) < 200) continue;
        const int drop = 2 + (int)(hash3(gx, 1, 349u) % 5);
        for (int k = 0; k < drop && y + k < H; k++)
          if (chA(c.get(gx, y + k)) == 255 && hash3(gx, y + k, 351u) % 3 != 0) c.set(gx, y + k, kLeaf[k == 0 ? 3 : (k < 3 ? 2 : 1)]);
        break;
      }
    }
  outline(c, 0.85f);
  return c;
}

// ---------------------------------------------------------------- the props
void paintBollard(Canvas& c, const Mat& M) {
  const int g = c.h - 2;   // the ground line (the foot's row)
  groundShadow(c, 6.5f, g + 0.5f, 3.6f, 1.6f);
  // a squat round post of dressed stone: lit on the left, a darker band at the foot, a domed cap
  const Ramp& R = M.stone;
  cylinder(c, 2, g - 8, 7, g, R);
  for (int x = 2; x <= 7; x++) { c.set(x, g, R[x <= 3 ? 1 : 0]); c.set(x, g - 1, R[x <= 3 ? 2 : 1]); }   // the foot's plinth
  for (int x = 2; x <= 7; x++) c.set(x, g - 6, R[x <= 3 ? 2 : 1]);   // a groove under the cap
  ball(c, 4.9, g - 8.5, 3.1, 2.4, R);
  if (M.cul == CU_IMPERIAL || M.cul == CU_STAR) c.set(4, g - 10, R[4]);
  if (M.cul == CU_JADE) { c.set(3, g - 4, kIron[3]); c.set(4, g - 4, kIron[2]); c.set(5, g - 4, kIron[1]); }   // an iron ring
}

void paintPlanter(Canvas& c, const Mat& M, int v, bool snow) {
  const int g = c.h - 2;
  groundShadow(c, 9.0f, g + 0.5f, 7.5f, 2.2f);
  const int cul = M.cul;
  const bool pot = cul == CU_DUNE || cul == CU_SUN || cul == CU_JADE || cul == CU_IMPERIAL;
  const bool stone = cul == CU_STAR || cul == CU_SYLVAN || cul == CU_HIGHLAND;
  int soilY;
  if (pot) {
    // a round pot: terracotta (glazed blue and white in the jade kingdoms), a lit rim, swelling body
    const Ramp& R = cul == CU_JADE ? ramp5(rgba(40, 54, 100), rgba(70, 96, 150), rgba(196, 206, 222), rgba(226, 232, 240), rgba(248, 250, 252)) : kTerracotta;
    for (int y = g - 8; y <= g; y++) {
      const float t = (y - (g - 8)) / 8.0f;
      const float half = 4.2f + std::sin(t * 3.0f) * 1.6f - (t > 0.85f ? 1.2f : 0.0f);
      for (int x = (int)std::floor(8 - half); x <= (int)std::ceil(7 + half); x++) {
        const float u = (x + 0.5f - 8.0f) / (half + 0.5f);
        int k = lightIndex(lightAt(u * 0.9f, 0.1f), x, y, 0.05f);
        if (cul == CU_JADE && k >= 2 && ((y == g - 4) || (y == g - 6 && (x & 1)))) k = 1;   // the blue band
        c.set(x, y, R[k]);
      }
    }
    for (int x = 3; x <= 12; x++) c.set(x, g - 9, R[x <= 5 ? 4 : 3]);   // the rim
    soilY = g - 9;
  } else {
    // a box: timber planks with iron corners, or a dressed stone trough
    const Ramp& R = stone ? M.stone : M.wood;
    block(c, 2, 13, g - 3, g, 6, R, 3, stone);
    if (!stone)
      for (int h = 0; h < 5; h++) { c.set(2, g - h, kIron[2]); c.set(13, g - h, kIron[1]); if (h == 2) for (int x = 3; x <= 12; x++) c.set(x, g - h, R[1]); }
    for (int x = 3; x <= 12; x++) for (int y = g - 8; y <= g - 7; y++) c.set(x, y, rgba(70, 50, 38));   // the soil inside
    soilY = g - 8;
  }
  // the planting
  const float cx = 8.0f, cy = soilY - 3.0f;
  switch (v % 4) {
    case 0: ball(c, cx, cy, 5.4, 4.4, kLeafDark); break;   // a clipped box ball
    case 1: {                                              // flowers in a mound of leaves
      ball(c, cx, cy + 1, 5.6, 3.8, kLeaf);
      static const uint32_t fl[4][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
      (void)fl;
      for (int i = 0; i < 12; i++) {
        const int x = 3 + (int)(hash3(i, v, 361u) % 10), y = (int)cy - 2 + (int)(hash3(i, v, 363u) % 5);
        const uint32_t col = (i % 3 == 0) ? kRed[3] : (i % 3 == 1 ? kGold[4] : rgba(236, 232, 244));
        if (chA(c.get(x, y))) { c.set(x, y, col); if (i % 2) c.set(x, y - 1, lighten(col, 0.4f)); }
      }
      break;
    }
    case 2: {                                              // tall spikes of lavender or hollyhock
      for (int i = 0; i < 6; i++) {
        const int x = 4 + i * 2 - (i > 2 ? 0 : 0);
        const int top = (int)cy - 5 + (int)(hash3(i, 7, 365u) % 3);
        for (int y = top; y <= soilY; y++) c.set(x, y, y > top + 3 ? kLeaf[i < 3 ? 3 : 2] : kPurple[i < 3 ? 4 : 3]);   // (lit on the west)
        c.set(x + 1, top + 2, kLeaf[3]);
      }
      break;
    }
    default: {                                             // a little standard bay tree, clipped round on a stem
      for (int y = (int)cy - 1; y <= soilY; y++) { c.set(7, y, kBark[3]); c.set(8, y, kBark[1]); }
      ball(c, cx, cy - 5, 4.6, 4.0, kLeafDark);
      break;
    }
  }
  if (snow) for (int x = 0; x < c.w; x++) for (int y = 0; y < c.h - 3; y++) if (chA(c.get(x, y)) == 255 && !chA(c.get(x, y - 1))) { c.set(x, y, kSnow[3]); break; }
}

void paintFlowerBed(Canvas& c, const Mat& M, int v) {
  const int g = c.h - 2;
  // a kerbed bed: the stone kerb's lit top and short face, dark soil, a mound of flowers in two colours
  const Ramp& R = M.stone;
  for (int y = g - 9; y <= g - 2; y++)
    for (int x = 1; x <= 14; x++) {
      const bool rim = y == g - 9 || x == 1 || x == 14 || y == g - 2;
      c.set(x, y, rim ? R[(y == g - 9 || x == 1) ? 4 : (x == 14 ? 2 : 3)] : rgba(78, 56, 40));
    }
  for (int x = 1; x <= 14; x++) { c.set(x, g - 1, R[x == 1 ? 3 : 2]); c.set(x, g, R[1]); }   // the kerb's face
  static const uint32_t pal[4][3] = {{rgba(214, 60, 58), rgba(250, 210, 90), rgba(250, 150, 110)},
                                     {rgba(150, 90, 200), rgba(244, 240, 248), rgba(196, 150, 230)},
                                     {rgba(80, 110, 214), rgba(240, 150, 190), rgba(150, 180, 240)},
                                     {rgba(240, 130, 40), rgba(250, 220, 80), rgba(255, 180, 90)}};
  const int pv = (v % 4 + 4) % 4;
  for (int y = g - 10; y <= g - 3; y++)
    for (int x = 2; x <= 13; x++) {
      const float dx = (x - 7.5f) / 6.5f, dy = (y - (g - 6.0f)) / 4.2f;
      if (dx * dx + dy * dy > 1.0f) continue;
      const uint32_t h = hash3(x, y, 371u + (uint32_t)pv);
      uint32_t col = (h % 3 == 0) ? kLeaf[dx < 0 && dy < 0 ? 3 : 2] : (h % 3 == 1 ? pal[pv][0] : pal[pv][1]);
      if (dx + dy < -0.6f && h % 3 != 0) col = pal[pv][2];   // the lit side's blooms
      if (dx + dy > 0.8f) col = darken(col, 0.35f);
      c.set(x, y, col);
    }
}

void paintFruitTree(Canvas& c, int kind, bool snow) {
  const int g = c.h - 2, cx = 14;
  // its ground shadow, then the trunk (a short bole, forked), then the crown in clumps lit from the top-left
  for (int y = g - 13; y <= g; y++) { c.set(cx - 1, y, kBark[3]); c.set(cx, y, kBark[2]); c.set(cx + 1, y, kBark[1]); }
  c.set(cx - 2, g, kBark[2]); c.set(cx + 2, g, kBark[1]);   // the root flare
  line(c, cx, g - 10, cx - 4, g - 15, kBark[2]);
  line(c, cx + 1, g - 11, cx + 5, g - 16, kBark[1]);
  const Ramp leaf = kind == FRUIT_ORANGE ? ramp5(rgba(14, 40, 40), rgba(24, 70, 50), rgba(40, 104, 56), rgba(76, 140, 62), rgba(130, 180, 84))
                  : kind == FRUIT_POMEGRANATE ? ramp5(rgba(26, 46, 40), rgba(44, 80, 46), rgba(70, 116, 50), rgba(112, 152, 60), rgba(168, 196, 88))
                  : kind == FRUIT_PEAR ? kBirchLeaf : kLeaf;
  const float rx = kind == FRUIT_PEAR ? 8.5f : 10.5f, ry = kind == FRUIT_PEAR ? 11.0f : 8.5f;
  const float ccy = g - 20.0f - (kind == FRUIT_PEAR ? 2.0f : 0.0f);
  // the crown: one shaded mass, then lit clumps on its upper left (no flat disc)
  ball(c, cx + 0.5, ccy, rx, ry, leaf, 0.12f, -1);
  for (int i = 0; i < 7; i++) {
    const float a = i * 0.9f + (float)(kind) * 0.4f;
    const float qx = cx + 0.5f + std::cos(a) * rx * 0.55f, qy = ccy + std::sin(a) * ry * 0.5f;
    ball(c, qx, qy, rx * 0.42f, ry * 0.42f, leaf, 0.12f, (std::cos(a) + std::sin(a)) < -0.3f ? 1 : 0);
  }
  ball(c, cx - 1.5, ccy - 2.5, rx * 0.45f, ry * 0.4f, leaf, 0.10f, 1);
  // the fruit, hanging in the lit half more than the shade
  const Ramp fruit = kind == FRUIT_ORANGE ? ramp5(rgba(150, 64, 20), rgba(214, 110, 24), rgba(246, 150, 36), rgba(255, 196, 80), rgba(255, 236, 160))
                   : kind == FRUIT_POMEGRANATE ? ramp5(rgba(70, 14, 30), rgba(120, 22, 40), rgba(168, 36, 50), rgba(212, 70, 74), rgba(240, 140, 130))
                   : kind == FRUIT_PEAR ? ramp5(rgba(90, 96, 30), rgba(150, 156, 44), rgba(196, 200, 70), rgba(226, 228, 110), rgba(248, 248, 180))
                   : ramp5(rgba(90, 14, 26), rgba(150, 26, 34), rgba(204, 44, 44), rgba(236, 96, 80), rgba(255, 170, 150));
  const int nFruit = kind == FRUIT_POMEGRANATE ? 7 : 10;
  for (int i = 0; i < nFruit; i++) {
    const uint32_t h = hash3(i, kind, 381u);
    const float fx = cx + 0.5f + ((float)(h % 1000) / 1000.0f - 0.5f) * rx * 1.6f;
    const float fy = ccy + ((float)((h >> 10) % 1000) / 1000.0f - 0.35f) * ry * 1.4f;
    const int x = (int)fx, y = (int)fy;
    const float ux = (fx - cx) / rx, uy = (fy - ccy) / ry;
    if (ux * ux + uy * uy > 0.8f) continue;
    const bool lit = ux + uy < 0.2f;
    c.set(x, y, fruit[lit ? 2 : 1]);
    c.set(x + 1, y, fruit[lit ? 2 : 1]);
    c.set(x, y + 1, fruit[lit ? 1 : 0]);
    c.set(x + 1, y + 1, fruit[0]);
    c.set(x, y, fruit[lit ? 4 : 2]);   // the glint
    if (kind == FRUIT_PEAR) c.set(x + 1, y - 1, fruit[lit ? 2 : 1]);
    if (kind == FRUIT_POMEGRANATE) c.set(x + 1, y - 1, fruit[3]);   // the crown at its top
  }
  if (snow)
    for (int x = 0; x < c.w; x++)
      for (int y = 0; y < g - 12; y++)
        if (chA(c.get(x, y)) == 255) { c.set(x, y, kSnow[4]); if (chA(c.get(x, y + 1)) == 255) c.set(x, y + 1, kSnow[3]); break; }
}

void paintMonument(Canvas& c, const Mat& M, bool snow) {
  const int g = c.h - 2, cx = 12;
  groundShadow(c, cx + 4.0f, g + 0.5f, 11.0f, 3.0f, 80);
  const int cul = M.cul;
  const Ramp& S = M.stone;
  // the stepped plinth: two steps and a die, each a 3/4 block (lit tops, mid-tone faces)
  block(c, cx - 11, cx + 10, g - 4, g, 3, S, 3, false);
  block(c, cx - 8, cx + 7, g - 7, g - 3, 3, S, 3, false);
  if (snow) for (int x = cx - 11; x <= cx + 10; x++) { c.set(x, g - 7, kSnow[3]); if (x >= cx - 8 && x <= cx + 7) c.set(x, g - 10, kSnow[3]); }
  const int dieTop = g - 6 - 9;
  block(c, cx - 5, cx + 4, g - 9, g - 6, 9, S, 3, false);
  for (int x = cx - 3; x <= cx + 2; x++) c.set(x, g - 10, S[1]);   // an inscription's panel
  for (int x = cx - 2; x <= cx + 1; x += 2) c.set(x, g - 11, S[4]);
  const int top = dieTop - 4;   // the shaft stands on the die's top
  switch (cul) {
    case CU_DUNE: case CU_SUN: case CU_STAR: {   // an obelisk: a tapering square shaft, its pyramidion gilded (white and starlit)
      const int H = 36;
      for (int y = 0; y < H; y++) {
        const int half = 4 - (y * 2) / H;
        for (int x = cx - half; x <= cx + half - 1; x++) {
          int k = x < cx ? 3 : 1;
          if (x == cx - half) k = 4;
          if (cul == CU_SUN && (y % 7 == 3) && x > cx - half && x < cx + half - 1) k = 1;   // carved bands
          if (cul == CU_DUNE && (hash3(x, y, 391u) % 13 == 0) && x > cx - half) k = std::max(0, k - 1);   // the glyphs
          c.set(x, top - y, S[k]);
        }
      }
      const Ramp& tip = cul == CU_STAR ? kCrystal : kGold;
      for (int y = 0; y < 5; y++)
        for (int x = cx - 3 + y / 2; x <= cx + 2 - y / 2; x++) c.set(x, top - H - y, tip[x < cx ? 4 : 2]);
      if (cul == CU_STAR) { c.set(cx, top - H - 6, kCrystal[4]); c.set(cx - 1, top - H - 5, kCrystal[3]); }
      break;
    }
    case CU_JADE: {   // a stele: a tall slab under a little roof of green tiles
      for (int y = 0; y < 30; y++)
        for (int x = cx - 4; x <= cx + 3; x++) c.set(x, top - y, S[x == cx - 4 ? 4 : (x >= cx + 2 ? 1 : (y % 5 == 2 && x > cx - 3 && x < cx + 2 ? 2 : 3))]);
      for (int y = 0; y < 5; y++)
        for (int x = cx - 8 + y; x <= cx + 7 - y; x++) c.set(x, top - 30 - y, kJadeTile[y == 0 ? 1 : (x < cx ? 4 : 2)]);
      c.set(cx - 8, top - 31, kJadeTile[3]); c.set(cx + 7, top - 31, kJadeTile[2]);   // the upturned eaves
      break;
    }
    case CU_FJORD: case CU_STEPPE: case CU_HIGHLAND: {   // a carved standing stone: runes in red, a cross ring, a stone figure
      for (int y = 0; y < 30; y++) {
        const float t = y / 30.0f;
        const int half = 4 + (int)(std::sin(t * 2.6f + 0.4f) * 1.3f);
        for (int x = cx - half; x <= cx + half - 1; x++) {
          int k = x < cx - 1 ? 3 : (x > cx + 1 ? 1 : 2);
          if (x == cx - half) k = 4;
          if (y > 26 && std::abs(x - cx) > 2 + (30 - y)) continue;
          c.set(x, top - y, S[k]);
        }
        if (cul == CU_FJORD && y > 3 && y < 26 && (y % 4 == 1)) c.set(cx - 2 + (y / 4) % 3, top - y, kRed[2]);   // the runes
      }
      if (cul == CU_HIGHLAND) {   // a ring over the head of a cross
        for (int a = 0; a < 24; a++) { const float an = a * 0.2618f; c.set(cx + (int)std::lround(std::cos(an) * 5) , top - 24 + (int)std::lround(std::sin(an) * 5), S[an > 3.14f ? 4 : 1]); }
      }
      if (cul == CU_STEPPE) { for (int x = cx - 2; x <= cx + 1; x++) c.set(x, top - 20, S[0]); c.set(cx - 2, top - 24, S[0]); c.set(cx + 1, top - 24, S[0]); }   // the face
      break;
    }
    case CU_MARSH: {   // a carved and painted totem post
      for (int y = 0; y < 34; y++)
        for (int x = cx - 3; x <= cx + 2; x++) {
          int k = x == cx - 3 ? 4 : (x >= cx + 1 ? 1 : 3);
          const int band = y / 7;
          const Ramp& R = band % 3 == 1 ? kRed : (band % 3 == 2 ? kJadeTile : M.wood);
          c.set(x, top - y, (y % 7 == 0) ? M.wood[0] : R[k]);
        }
      for (int x = cx - 6; x <= cx + 5; x++) c.set(x, top - 28, M.wood[x < cx ? 3 : 1]);   // the outspread wings
      break;
    }
    case CU_SYLVAN: {   // a slender white spire wound with a living vine
      for (int y = 0; y < 38; y++) {
        const int half = 3 - y / 14;
        for (int x = cx - half; x <= cx + half - 1; x++) c.set(x, top - y, kWhitewash[x == cx - half ? 4 : (x >= cx ? 2 : 3)]);
        const int vx = cx + (int)std::lround(std::sin(y * 0.5f) * (half + 0.5f)) - (y % 2);
        if (y > 2) c.set(vx, top - y, kLeaf[std::sin(y * 0.5f) < 0 ? 3 : 1]);
      }
      ball(c, cx - 1, top - 40, 2.5, 2.5, kGlow);
      break;
    }
    default: {   // a column: fluted shaft, a capital, a gilded figure (the heartland, the empire, the river towns)
      const int H = 30;
      for (int y = 0; y < H; y++)
        for (int x = cx - 3; x <= cx + 2; x++) {
          const float u = (x + 0.5f - cx) / 3.0f;
          int k = lightIndex(lightAt(u * 0.95f, 0.0f), x, y, 0.0f);
          if ((x - cx + 3) % 2 == 1 && k >= 2) k--;   // the fluting
          c.set(x, top - y, S[k]);
        }
      block(c, cx - 5, cx + 4, top - H - 1, top - H + 1, 2, S, 3, false);   // the capital
      const int fy = top - H - 4;
      const Ramp& F = cul == CU_IMPERIAL ? kGold : (cul == CU_RIVER ? kBrass : kGold);
      // a gilded figure with a raised arm (a hero, a king, an eagle in the empire)
      if (cul == CU_IMPERIAL) {
        ball(c, cx, fy - 2, 2.4, 2.2, F);
        line(c, cx - 5, fy - 5, cx - 1, fy - 2, F[3]); line(c, cx + 4, fy - 5, cx, fy - 2, F[2]);   // the eagle's wings
        line(c, cx - 5, fy - 4, cx - 2, fy - 2, F[4]);
      } else {
        for (int y = 0; y < 8; y++) { c.set(cx - 1, fy - y, F[3]); c.set(cx, fy - y, F[2]); if (y < 6) c.set(cx + 1, fy - y, F[1]); }
        ball(c, cx - 0.2, fy - 9, 1.7, 1.7, F);
        line(c, cx - 2, fy - 6, cx - 4, fy - 11, F[4]);   // the raised arm (a sword's glint at its end)
        c.set(cx - 4, fy - 12, kWhite);
      }
      break;
    }
  }
}

// a pavilion over its 3x2 footprint (48 x 32 ground px: canvas rows 40..71), in the culture's idiom
void paintPavilion(Canvas& c, const Mat& M, bool snow) {
  const int OY = 40;   // canvas y = ground y + OY - z
  const int cul = M.cul;
  const Ramp& S = M.stone;
  const bool tent = cul == CU_STEPPE, dome = cul == CU_DUNE || cul == CU_SUN || cul == CU_IMPERIAL || cul == CU_STAR;
  const bool jade = cul == CU_JADE || cul == CU_MARSH;
  // the ground shadow down-right of the roof
  for (int gy = 10; gy < 32; gy++)
    for (int gx = 8; gx < 48; gx++) {
      if (gx > 44 && gy < 14) continue;
      if (((gx + gy) & 1) && (gx > 45 || gy > 30)) continue;
      c.set(gx, gy + OY, shadowPx(gx > 44 || gy > 29 ? 60 : 82));
    }
  // the platform: a dais of stone (planks in the marsh, felt rugs on a timber floor in the steppe), 4 px high
  const int px0 = 3, px1 = 44, pg0 = 6, pg1 = 29, PZ = 4;
  const Ramp& floorR = (cul == CU_MARSH || tent) ? M.wood : S;
  block(c, px0, px1, pg0 + OY, pg1 + OY, PZ, floorR, 3, !(cul == CU_MARSH || tent));
  // the roof's shade on the floor inside the posts
  for (int gy = pg0 + 1; gy < pg1; gy++)
    for (int gx = px0 + 1; gx < px1; gx++) c.set(gx, gy + OY - PZ, darken(c.get(gx, gy + OY - PZ), gy < pg1 - 3 ? 0.55f : 0.35f));
  // the posts (columns, red lacquered pillars, timber posts, tent poles), back row first
  const Ramp& postR = jade ? kLacquer : (dome ? (cul == CU_IMPERIAL || cul == CU_STAR ? kWhitewash : kAdobe) : M.wood);
  const int PH = 22;   // from the dais to the eaves
  const int postX[3] = {px0 + 3, (px0 + px1) / 2, px1 - 4};
  for (int row = 0; row < 2; row++) {
    const int gyp = row == 0 ? pg0 + 2 : pg1 - 1;
    for (int k = 0; k < 3; k++) {
      if (row == 0 && k == 1) continue;
      const int x = postX[k];
      for (int h = 0; h < PH; h++) {
        const int y = gyp + OY - PZ - h;
        c.set(x, y, postR[3]); c.set(x + 1, y, postR[2]); c.set(x + 2, y, postR[1]);
        if (dome && (h == 0 || h == PH - 1)) { c.set(x - 1, y, postR[3]); c.set(x + 3, y, postR[1]); }   // base and capital
      }
    }
    if (row == 1) {   // a low railing between the front posts (balusters), open at the middle bay (the way in)
      for (int x = postX[0] + 3; x < postX[2]; x++) {
        if (x > postX[1] - 6 && x < postX[1] + 9) continue;
        const int y = gyp + OY - PZ;
        c.set(x, y - 6, postR[3]);
        if (x % 3 == 0) for (int h = 1; h < 6; h++) c.set(x, y - h, postR[2]);
      }
    }
  }
  // the roof: a height field over the plan (eaves at PH above the dais), painted back to front, each point lit by its
  // slope (the light from the top-left); courses where the height crosses each step
  const float rx0 = 0.5f, rx1 = 47.5f, rg0 = 2.5f, rg1 = 31.0f;
  const float rcx = (rx0 + rx1) * 0.5f, rcy = (rg0 + rg1) * 0.5f, hx = (rx1 - rx0) * 0.5f, hy = (rg1 - rg0) * 0.5f;
  const float base = (float)(PZ + PH);
  auto roofZ = [&](float gx, float gy) -> float {
    const float u = (gx - rcx) / hx, v = (gy - rcy) / hy;
    if (std::fabs(u) > 1.0f || std::fabs(v) > 1.0f) return -1.0f;
    if (dome) {
      // (fixer M7b r2, review: "the dome floats free of its posts") the dome rises from a flat roof slab that spans the
      // posts (the slab's corners on the corner posts)
      (void)u; (void)v;
      return base + 2.0f;   // (the dome itself is painted on the slab below, as a lit hemisphere)
    }
    if (tent) {
      const float r = std::sqrt(u * u + v * v);
      if (r > 1.0f) return -1.0f;
      return base + (1.0f - r) * 22.0f;
    }
    // a hip roof, ridge east-west; the jade kingdoms' with its eaves swept up at the corners
    const float ridge = 0.45f;   // the ridge's half-length (of hx)
    const float du = std::max(0.0f, std::fabs(u) - ridge) / (1.0f - ridge);
    const float m = std::max(du, std::fabs(v));
    float z = base + (1.0f - m) * 13.0f;
    if (jade) { const float corner = std::max(0.0f, std::fabs(u) + std::fabs(v) - 1.3f); z += corner * corner * 18.0f + (1.0f - m) * (1.0f - m) * -4.0f; }
    return z;
  };
  const Ramp roofR = tent ? kCloth : (dome ? (cul == CU_DUNE ? kWhitewash : (cul == CU_STAR ? kCrystal : kTerracotta)) : (jade ? kJadeTile : (cul == CU_RIVER || cul == CU_HEART || cul == CU_NONE ? kTerracotta : (cul == CU_SYLVAN || cul == CU_FJORD ? kLeaf : (cul == CU_HIGHLAND ? kThatch : kWoodDark)))));   // (fixer M7b r2: the highland pavilion thatched, the fjord's under turf, not one red-brown roof in every park)
  // (fixer M7b r2, review: "the lower half of the dome is a coarse checkerboard, the grass shows through") where the
  // surface turns toward the camera faster than a pixel a row, the rows between are filled (no see-through gaps)
  int lastY[48];
  for (int i = 0; i < 48; i++) lastY[i] = -1000;
  for (int gy = 2; gy < 32; gy++)
    for (int gx = 0; gx < 48; gx++) {
      const float z = roofZ(gx + 0.5f, gy + 0.5f);
      if (z < 0) continue;
      const float zx = roofZ(gx + 1.5f, gy + 0.5f), zy = roofZ(gx + 0.5f, gy + 1.5f);
      const float dzx = zx < 0 ? z - roofZ(gx - 0.5f, gy + 0.5f) : zx - z, dzy = zy < 0 ? z - roofZ(gx + 0.5f, gy - 0.5f) : zy - z;
      // the surface normal (x right, y toward the camera, z up): -dz/dx, -dz/dy; the light from the top-left
      float nx = -dzx * 0.55f, ny = -dzy * 0.55f;
      const float nl = std::sqrt(nx * nx + ny * ny + 1.0f);
      nx /= nl; ny /= nl;
      int k = lightIndex(lightAt(nx, ny), gx, gy, 0.06f);
      // the courses (tiles, shingles, felt seams): a darker line where the height crosses a step
      const float step = tent ? 5.0f : 3.0f;
      const int course = (int)std::floor(z / step);
      const bool courseLine = course != (int)std::floor((z + std::max(std::fabs(dzx), std::fabs(dzy))) / step);
      if (snow && !dome) k = std::min(4, k + 1);
      uint32_t col = snow && !dome ? kSnow[std::clamp(k, 1, 4)] : roofR[dome ? std::max(0, k - 1) : k];   // (a dome's slab a shade under the dome)
      // the courses (a soft line under each row of tiles or shingles) and the staggered joints between them
      if (courseLine) col = mix(col, roofR[std::max(0, k - 1)], 0.65f);
      else if (!dome && !tent && ((gx + (course & 1) * 2) % 4) == 0 && std::fabs(dzy) >= std::fabs(dzx)) col = mix(col, roofR[std::max(0, k - 1)], 0.35f);
      const int y = (int)std::lround(gy + OY - z);
      if (y >= 0 && y < c.h) c.set(gx, y, col);
      if (lastY[gx] > -1000 && y > lastY[gx] + 1)
        for (int yy = lastY[gx] + 1; yy < y; yy++) if (yy >= 0 && yy < c.h) c.set(gx, yy, col);
      if (lastY[gx] > -1000 && y < lastY[gx] - 1)
        for (int yy = y + 1; yy < lastY[gx]; yy++) if (yy >= 0 && yy < c.h) c.set(gx, yy, col);
      lastY[gx] = y;
      // the eave's fascia at the front edge
      if (roofZ(gx + 0.5f, gy + 1.5f) < 0 && !tent) {
        for (int h = 1; h <= 2; h++) if (y + h < c.h) c.set(gx, y + h, roofR[h == 1 ? 1 : 0]);
      }
    }
  // a finial at the top
  int topY = (int)std::lround(rcy + OY - roofZ(rcx, rcy));
  if (dome) {
    // the dome on its slab: a hemisphere seen from the 3/4 view (its foot an ellipse on the slab), lit from the top-left
    // like every sphere (ball's normal and ramp), a drum ring at its foot
    const float bx = rcx, by = rcy + OY - (base + 2.0f) + 6.0f;   // the foot's centre on the canvas
    const float Rx = 19.0f, Ru = 25.0f, Rd = 8.0f;
    for (int y = (int)(by - Ru - 1); y <= (int)(by + Rd + 1); y++)
      for (int x = (int)(bx - Rx - 1); x <= (int)(bx + Rx + 1); x++) {
        const float dx = (x + 0.5f - bx) / Rx, dyS = y + 0.5f - by, dy = dyS < 0 ? dyS / Ru : dyS / Rd;
        const float e = dx * dx + dy * dy;
        if (e > 1.0f || y < 0 || y >= c.h) continue;
        const float ny = dyS < 0 ? dy : dy * 0.35f;   // (the front of the foot faces the camera, not the ground)
        int k = lightIndex(lightAt(dx * 0.92f, ny * 0.92f), x, y, 0.06f);
        if (dyS >= 0 && e > 0.55f) k = std::max(0, k - 1);              // the drum ring's shade at the foot
        c.set(x, y, roofR[std::clamp(k, 0, 4)]);
      }
    topY = (int)std::lround(by - Ru);
    ball(c, rcx, topY - 2, 1.6, 1.6, kGold); c.set((int)rcx, topY - 4, kGold[4]);
  }
  else if (tent) { for (int y = 0; y < 5; y++) c.set((int)rcx, topY - y, kWood[2]); c.set((int)rcx + 1, topY - 4, kRed[3]); c.set((int)rcx + 2, topY - 3, kRed[2]); }
  else if (jade) { ball(c, rcx - 0.5, topY - 1, 1.8, 1.6, kGold); }
  else for (int y = 0; y < 3; y++) { c.set((int)rcx - 1, topY - y, kIron[3]); }
}

// the souk's canopy: a cloth strung across the street 22 px up (canvas rows 2..17 over the tile's ground rows 24..39),
// panels sewn in stripes, billowing between the ropes; a hem where no canopy runs on to the south, poles at the ends
void paintCanopy(Canvas& c, const Mat& M, const PropStyle& st, int v, int joins) {
  const int OY = 24, Z = 22;
  // the colours: the culture's awning cloths, else the souk's indigo and saffron
  uint32_t a = st.awningA ? st.awningA : (M.cul == CU_JADE ? rgba(186, 40, 40) : (M.cul == CU_SUN ? rgba(196, 80, 44) : (M.cul == CU_STEPPE ? rgba(150, 110, 80) : rgba(52, 70, 150))));
  uint32_t b = st.awningB ? st.awningB : (M.cul == CU_JADE ? rgba(232, 190, 80) : (M.cul == CU_SUN ? rgba(230, 196, 120) : (M.cul == CU_STEPPE ? rgba(222, 204, 170) : rgba(232, 184, 70))));
  const int pv = (v % 4 + 4) % 4;
  if (pv == 1) std::swap(a, b);
  if (pv == 2) b = rgba(236, 228, 208);
  const Ramp A = ramp(opaque(a)), B = ramp(opaque(b));
  // the shade under it, on the ground (and on whoever walks there: the canopy is drawn over them)
  for (int gy = 0; gy < 16; gy++)
    for (int gx = 0; gx < 16; gx++) {
      const bool edge = (!(joins & 4) && gy > 12) || (!(joins & 8) && gx < 3);
      if (edge && ((gx + gy) & 1)) continue;
      c.set(gx, gy + OY, shadowPx(edge ? 46 : 64));
    }
  // the poles at the open ends (west and east), standing at the tile's front corners
  for (int side = 0; side < 2; side++) {
    if (joins & (side == 0 ? 8 : 2)) continue;
    if (joins & 4) continue;   // (the pole stands at the canopy's south edge only)
    const int x = side == 0 ? 1 : 14;
    for (int y = 12 + OY - Z; y <= 15 + OY; y++) { c.set(x, y, M.wood[side == 0 ? 3 : 2]); }
    c.set(x + (side == 0 ? 1 : -1), 15 + OY, M.wood[1]);
  }
  // (fixer M7b r3, review: "the canopies are flat slabs of stripes crossed by scanline-like grey lines, no sag, no
  // lighting, no poles, no shadow") one cloth over the whole bay: no seam or sag restarting at every tile (that ribbed
  // the bay with grey lines), but sewn widths that each belly between their seams, lit on the side toward the
  // top-left light and shaded on the other; the cloth rises to its ridge rope at the bay's north end and falls to a
  // shaded, scalloped hem at its south end on tasselled poles
  const int sw = pv == 3 ? 8 : 4;   // the sewn width (one stripe)
  for (int gy = 0; gy < 16; gy++)
    for (int gx = 0; gx < 16; gx++) {
      // the cloth's height: up at the ridge rope (the bay's north edge), dipping toward the hem (its south edge)
      float lift = 0.0f;
      if (!(joins & 1)) lift += std::max(0.0f, 3.0f - gy) * 0.6f;
      if (!(joins & 4)) lift -= std::max(0.0f, gy - 11.0f) * 0.5f;
      const int y = (int)std::lround(gy + OY - Z - lift);
      const int stripe = (gx / sw) & 1;
      const Ramp& R = stripe ? B : A;
      // each sewn width bellies: its west side faces the light, its east side turns away, a dark seam between widths
      const int f = gx % sw;
      int k = sw == 4 ? (f == 0 ? 3 : (f == 3 ? 1 : 2)) : (f <= 1 ? 3 : (f >= 6 ? 1 : 2));
      if (!(joins & 1) && gy <= 2) k = std::min(4, k + 1);              // the cloth lifting to its ridge catches the light
      if (!(joins & 4) && gy >= 13) k = std::max(0, k - 1);             // and falls into shade toward the hem
      if (gx == 0 && !(joins & 8)) k = std::min(4, k + 1);
      if (gx == 15 && !(joins & 2)) k = std::max(0, k - 1);
      uint32_t col = R[k];
      if (f == sw - 1 && sw == 8) col = R[std::max(0, k - 1)];          // a broad cloth's seam
      if ((gx == 0 && !(joins & 8)) || (gx == 15 && !(joins & 2))) col = shade(R[0], 0.7f);   // the open sides' rim
      if (gy == 0 && !(joins & 1)) col = rgba(110, 86, 60);              // the rope along the north edge
      c.set(gx, y, col);
      if (gy == 15 && !(joins & 4)) {                                    // the hem: a scalloped valance, tassels
        const int drop = 2 + ((gx % 4 == 1 || gx % 4 == 2) ? 1 : 0);
        for (int d = 1; d <= drop; d++) c.set(gx, y + d, R[d == drop ? 0 : 1]);
        if (gx % 4 == 2) { c.set(gx, y + drop + 1, kGold[3]); }
      }
    }
  // the hem's poles: one at every tile's west corner along the open south edge (a bay's valance hangs from a row of
  // them), its shadow falling to the south-east on the street
  if (!(joins & 4)) {
    const int x = 2;
    for (int y = 21 + OY - Z; y <= 15 + OY; y++) c.set(x, y, M.wood[2]);
    c.set(x + 1, 15 + OY, shadowPx(90));
    c.set(x + 2, 15 + OY, shadowPx(60));
  }
}
}  // namespace

int cityPropW(Prop p) { return cityInfo(p).w; }
int cityPropH(Prop p) { return cityInfo(p).h; }
int cityPropFrames(Prop p) { return cityInfo(p).frames; }

int cityPropVariants(Prop p) {
  switch (p) {
    case Prop::FruitTree: return 4;
    case Prop::Planter: return 4;
    case Prop::FlowerBed: return 4;
    case Prop::Canopy: return 4;
    default: return 1;
  }
}

int fruitTreeKind(int biome, uint32_t h) {
  switch (biome) {
    case 8: return (h & 1) ? FRUIT_POMEGRANATE : FRUIT_ORANGE;      // the desert's oases
    case 7: case 1: return (h % 3u) == 0 ? FRUIT_ORANGE : FRUIT_APPLE;   // the warm wet south, the coast
    case 5: case 6: case 9: return FRUIT_APPLE;                      // the hardy apple in the cold
    default: return (h % 3u) == 0 ? FRUIT_PEAR : FRUIT_APPLE;
  }
}

Canvas cityPropStyled(Prop p, const PropStyle& st, int variant, int joins, bool snow) {
  const Sz s = cityInfo(p);
  const Mat M = matOf(st);
  if (p == Prop::Hedge) return hedgePiece(joins, st, snow);         // the culture's clipped hedge, joined as on the land
  if (p == Prop::GardenWall) return gardenWallPiece(joins, M, snow);
  Canvas c(s.w, s.h);
  switch (p) {
    case Prop::Bollard: paintBollard(c, M); break;
    case Prop::Planter: paintPlanter(c, M, variant, snow); break;
    case Prop::FlowerBed: paintFlowerBed(c, M, variant); break;
    case Prop::FruitTree: paintFruitTree(c, ((variant % 4) + 4) % 4, snow); break;
    case Prop::Monument: paintMonument(c, M, snow); break;
    case Prop::Pavilion: paintPavilion(c, M, snow); break;
    case Prop::Canopy: paintCanopy(c, M, st, variant, joins); break;
    default: break;
  }
  // (fixer M7b r3) a canopy's cloth runs on into the next tile's: the outline darkened every tile's top and bottom row
  // into a grey line across the bay (its open edges carry their own dark rim, paintCanopy)
  if (p != Prop::Canopy) outline(c, 0.9f);   // (the shadows' alpha stays under the outline's threshold)
  return c;
}

Canvas cityTreePit(const PropStyle& st) {
  const Mat M = matOf(st);
  const Ramp& S = M.stone;
  Canvas c(16, 10);
  // in the 3/4 view a square pit foreshortens to 14 x 8: the kerb's lit north and west rims, its shaded south and east
  // rims, the grate's iron rings round the trunk's dark soil
  for (int y = 1; y <= 8; y++)
    for (int x = 1; x <= 14; x++) {
      const bool rimN = y == 1, rimS = y == 8, rimW = x == 1, rimE = x == 14;
      uint32_t col;
      if (rimN || rimW) col = S[(rimN && rimW) ? 3 : 4];
      else if (rimS || rimE) col = S[rimS && rimE ? 1 : 2];
      else {
        const float dx = (x - 7.5f) / 6.0f, dy = (y - 4.5f) / 3.4f, r = std::sqrt(dx * dx + dy * dy);
        if (r < 0.32f) col = rgba(62, 46, 36);                                  // the soil at the trunk
        else if (std::fabs(r - 0.62f) < 0.12f || std::fabs(r - 0.95f) < 0.10f) col = (dx + dy < 0) ? kIron[3] : kIron[1];   // the rings
        else if (((x + y) & 1) == 0) col = kIron[2];                             // the bars
        else col = rgba(28, 24, 30);                                             // the dark below
      }
      c.set(x, y, col);
    }
  for (int x = 1; x <= 14; x++) c.set(x, 9, withA(rgba(40, 28, 70), 70));     // the kerb's foot on the paving
  return c;
}

void paintCityProp(Canvas& c, Prop p, int frame) {
  (void)frame;
  const int joins = (p == Prop::GardenWall || p == Prop::Hedge) ? 10 : 0;   // a lone wall or hedge reads as an east-west run
  const Canvas s = cityPropStyled(p, PropStyle{}, 0, joins, false);
  for (int y = 0; y < std::min(c.h, s.h); y++)
    for (int x = 0; x < std::min(c.w, s.w); x++) c.set(x, y, s.get(x, y));
}

}  // namespace art
