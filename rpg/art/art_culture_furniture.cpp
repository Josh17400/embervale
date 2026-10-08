// M3 culture furniture and the peoples' own furniture indoors (split out of art_culture_props.cpp in M3b phase A so the
// INTERIORS lane owns the indoor kit and the FORTIFICATIONS & GROUND lane the street furniture): the M3 culture pieces
// (Cushion, LowTable, Hammock, SleepingMat) for art_props.cpp paintProp, and cultureInteriorPiece (hearths, shelves,
// beds, chests, wall decor in each people's idiom). Everything in the game's high 3/4 view, lit from the top-left.
#include "rpg/art/art_internal.h"
#include "rpg/art/art_heraldry.h"

namespace art {

namespace {
const Ramp kJadeTileP = ramp5(rgba(14, 50, 56), rgba(24, 88, 80), rgba(40, 132, 104), rgba(88, 180, 132), rgba(178, 230, 182));
const Ramp kGlowElfP = ramp5(rgba(40, 110, 120), rgba(70, 170, 170), rgba(130, 220, 200), rgba(196, 248, 226), rgba(240, 255, 246));
void box34(Canvas& c, int x0, int y0, int w, int topH, int frontH, const Ramp& R) {
  for (int y = 0; y < topH; y++)
    for (int x = 0; x < w; x++) c.set(x0 + x, y0 + y, R[(x == 0 || y == 0) ? 4 : (x == w - 1 ? 2 : 3)]);
  for (int y = 0; y < frontH; y++)
    for (int x = 0; x < w; x++) c.set(x0 + x, y0 + topH + y, R[(x == 0) ? 3 : (x == w - 1 || y == frontH - 1 ? 1 : 2)]);
}
}  // namespace

// ---------------------------------------------------------------- M3 culture furniture (art_props.cpp paintProp)
// in 3/4 view, lit from the top-left; walk-over pieces (cushions, mats) stay low
void paintCultureFurniture(Canvas& c, Prop p, int frame) {
  (void)frame;
  const int w = c.w, base = c.h - 1;
  switch (p) {
    case Prop::Cushion: {   // a plump floor cushion, its top seen from above, a tassel at each corner, a smaller one leaning on it
      const Ramp C = ramp(rgba(170, 60, 56));
      for (int y = base - 8; y <= base - 1; y++)
        for (int x = 2; x < w - 2; x++) {
          const bool topF = y < base - 3;
          const float dx = (x + 0.5f - w * 0.5f) / (w * 0.5f - 2), dy = (y + 0.5f - (base - 6)) / 3.0f;
          int k = topF ? (dx + dy < -0.4f ? 4 : (dx + dy < 0.5f ? 3 : 2)) : (x < 4 ? 2 : 1);
          if ((x + y) % 4 == 0 && topF) k = std::max(1, k - 1);
          c.set(x, y, C[k]);
        }
      c.set(2, base - 8, kGold[4]); c.set(w - 3, base - 8, kGold[3]); c.set(2, base - 1, kGold[2]); c.set(w - 3, base - 1, kGold[1]);
      for (int x = 5; x < w - 5; x++) c.set(x, base - 6, kGold[3]);   // a woven band
      break;
    }
    case Prop::LowTable: {   // a low table: a lacquered top seen from above, short legs, a tray with a teapot and cups
      box34(c, 1, base - 9, w - 2, 5, 3, kWood);
      for (int s = 0; s < 2; s++) box(c, s ? w - 4 : 2, base - 1, s ? w - 3 : 3, base, kWoodDark[s ? 1 : 2]);
      for (int x = 5; x < w - 5; x++) for (int y = base - 8; y <= base - 6; y++) c.set(x, y, kBrass[y == base - 8 ? 4 : 3]);
      ball(c, w * 0.5f - 0.5f, base - 9.0f, 2.2f, 1.8f, kBone);
      c.set(w / 2 + 2, base - 9, kBone[2]);
      for (int s = 0; s < 2; s++) { c.set(s ? w - 7 : 6, base - 8, kWhite); c.set(s ? w - 7 : 6, base - 7, kBone[2]); }
      break;
    }
    case Prop::Hammock: {   // a hammock slung between two posts, its cloth sagging, a striped weave
      for (int s = 0; s < 2; s++) {
        const int x = s ? w - 3 : 1;
        for (int y = 2; y <= base; y++) { c.set(x, y, kWood[s ? 1 : 3]); c.set(x + 1, y, kWood[s ? 0 : 2]); }
        c.set(x, 2, kWood[4]);
      }
      for (int x = 3; x < w - 3; x++) {
        const float t = (x - 3) / (float)(w - 7);
        const int sag = (int)std::lround(std::sin(t * 3.14159f) * 7.0f);
        for (int k = 0; k < 4; k++) {
          const int y = 6 + sag + k;
          const bool stripe = ((x / 3) & 1) != 0;
          c.set(x, y, stripe ? (k == 0 ? rgba(236, 214, 160) : rgba(206, 180, 120)) : (k == 0 ? rgba(214, 104, 72) : rgba(176, 72, 56)));
        }
        if (x % 4 == 0) c.set(x, 6 + sag + 4, kCloth[1]);   // fringe
      }
      line(c, 2, 4, 4, 6, kCloth[2]); line(c, w - 3, 4, w - 5, 6, kCloth[2]);
      break;
    }
    case Prop::SleepingMat: {   // a reed mat rolled out, a bolster at its head, a folded blanket
      for (int y = base - 7; y <= base; y++)
        for (int x = 1; x < w - 1; x++) c.set(x, y, (y % 2) ? kThatch[2] : kThatch[3]);
      for (int x = 1; x < w - 1; x++) c.set(x, base, kThatch[1]);
      ball(c, 4.5f, base - 4.0f, 2.6f, 3.0f, kCloth);
      for (int y = base - 6; y <= base - 1; y++) for (int x = w - 12; x < w - 3; x++) c.set(x, y, ((x + y) % 3 == 0) ? rgba(60, 90, 140) : (y == base - 6 ? rgba(110, 140, 190) : rgba(80, 110, 160)));
      break;
    }
    default: break;
  }
}

// ================================================================ M3 fixer: the peoples' own furniture indoors
// (Piece::Culture, rpg/sim/prop_rules.cpp interiorPropKey). A house of the jade kingdoms, the marsh, the sun temples,
// the elves, the steppe, the dunes and the star cities is furnished in its own idiom instead of the heartland cottage
// kit: its hearth (a brick kang stove, a clay fire pit under a reed smoke hood, an adobe niche with a griddle, a
// hearth grown of living roots, an iron yurt stove, a brazier under a keyhole niche, a marble hearth of cold blue
// fire), its shelves, beds and chests. Same canvas, anchor and frames as the classic prop, 3/4 view, top-left light.
namespace {
enum KitArch : int { K_FJORD, K_HIGHLAND, K_HEART, K_IMPERIAL, K_DUNE, K_STEPPE, K_MARSH, K_JADE, K_RIVER, K_SUN, K_SYLVAN, K_STAR };
const Ramp kLacquer = ramp5(rgba(54, 16, 26), rgba(104, 24, 30), rgba(150, 38, 36), rgba(190, 64, 48), rgba(226, 112, 80));
const Ramp kEbony = ramp5(rgba(18, 14, 22), rgba(32, 24, 32), rgba(50, 38, 44), rgba(72, 56, 60), rgba(100, 82, 82));
const Ramp kBamboo = ramp5(rgba(84, 70, 40), rgba(128, 112, 60), rgba(170, 152, 84), rgba(204, 188, 112), rgba(232, 220, 150));
const Ramp kReed = ramp5(rgba(78, 62, 40), rgba(120, 98, 56), rgba(160, 134, 76), rgba(192, 168, 100), rgba(222, 202, 138));
const Ramp kPlaster = ramp5(rgba(150, 128, 104), rgba(196, 174, 146), rgba(226, 210, 182), rgba(242, 232, 210), rgba(252, 248, 236));
const Ramp kAdobeR = ramp5(rgba(110, 66, 50), rgba(156, 98, 66), rgba(196, 136, 90), rgba(222, 170, 116), rgba(240, 204, 150));
const Ramp kTerra = ramp5(rgba(96, 40, 30), rgba(140, 62, 40), rgba(180, 88, 54), rgba(210, 120, 74), rgba(234, 160, 106));
const Ramp kTurq = ramp5(rgba(20, 70, 76), rgba(30, 110, 110), rgba(52, 152, 144), rgba(94, 196, 178), rgba(160, 232, 214));
const Ramp kLiveBark = ramp5(rgba(40, 34, 30), rgba(70, 58, 44), rgba(104, 88, 62), rgba(140, 122, 84), rgba(178, 160, 112));
const Ramp kOrangeP = ramp5(rgba(98, 36, 26), rgba(156, 62, 32), rgba(204, 100, 40), rgba(232, 146, 58), rgba(250, 196, 102));
const Ramp kCedar = ramp5(rgba(60, 30, 24), rgba(98, 52, 34), rgba(138, 80, 48), rgba(174, 114, 66), rgba(204, 152, 96));
const Ramp kMarble = ramp5(rgba(124, 128, 148), rgba(176, 182, 198), rgba(214, 218, 228), rgba(234, 236, 242), rgba(250, 250, 252));
const Ramp kSilver = ramp5(rgba(70, 78, 100), rgba(116, 126, 148), rgba(164, 174, 192), rgba(206, 214, 226), rgba(240, 244, 250));
const Ramp kIndigo = ramp5(rgba(24, 26, 64), rgba(36, 44, 104), rgba(56, 70, 144), rgba(88, 108, 184), rgba(140, 160, 220));
const Ramp kFelt = ramp5(rgba(86, 24, 30), rgba(132, 36, 38), rgba(176, 56, 44), rgba(208, 92, 58), rgba(234, 140, 92));
const Ramp kSilkTeal = ramp5(rgba(16, 60, 66), rgba(26, 96, 96), rgba(44, 134, 124), rgba(82, 174, 152), rgba(150, 214, 190));
const Ramp kLeafBed = ramp5(rgba(26, 58, 40), rgba(40, 92, 48), rgba(64, 128, 56), rgba(104, 164, 66), rgba(160, 200, 90));
const Ramp kSkyCloth = ramp5(rgba(54, 70, 112), rgba(84, 108, 156), rgba(124, 150, 196), rgba(170, 192, 226), rgba(218, 230, 246));
const Ramp kSpirit = ramp5(rgba(40, 110, 90), rgba(70, 170, 120), rgba(140, 220, 150), rgba(206, 246, 176), rgba(250, 255, 226));
const Ramp kColdFire = ramp5(rgba(40, 70, 170), rgba(70, 120, 220), rgba(120, 180, 246), rgba(190, 226, 255), rgba(244, 252, 255));
const Ramp kBrickGrey = ramp5(rgba(54, 58, 70), rgba(84, 90, 102), rgba(112, 118, 128), rgba(140, 146, 152), rgba(172, 176, 178));


// a flame in a ramp (fire, the elves' spirit fire, the star folk's cold fire), frame 0..3, standing on (cx, base)
void kitFlame(Canvas& c, float cx, float base, float h, float w, int frame, const Ramp& F) {
  for (int layer = 0; layer < 3; layer++) {
    const float s = 1.0f - layer * 0.28f, fh = h * s, fw = w * s;
    for (int y = (int)(base - fh - 1); y <= (int)base; y++) {
      const float t = (base - y) / fh;
      if (t < 0 || t > 1) continue;
      const float sway = std::sin(t * 3.2f + frame * 1.7f) * t * w * 0.3f;
      const float half = fw * 0.5f * std::sqrt(std::max(0.0f, 1 - t)) * (t < 0.2f ? 0.6f + t * 2 : 1.0f);
      for (int x = (int)std::floor(cx + sway - half); x <= (int)std::ceil(cx + sway + half) - 1; x++) c.set(x, y, F[layer + 2]);
    }
  }
  const uint32_t hs = hash3(frame, 3, 77u);
  c.set((int)cx - 2 + (int)(hs % 5), (int)(base - h - 1 - (hs >> 8) % 3), F[4]);
}
// a soft ellipse of light over what is already painted (firelight on what is round a hearth)
void glowOn(Canvas& c, float cx, float cy, float rx, float ry, uint32_t col, float k) {
  for (int y = (int)(cy - ry); y <= (int)(cy + ry); y++)
    for (int x = (int)(cx - rx); x <= (int)(cx + rx); x++) {
      const float d = std::hypot((x + 0.5f - cx) / rx, (y + 0.5f - cy) / ry);
      if (d >= 1.0f || !chA(c.get(x, y))) continue;
      c.set(x, y, mix(c.get(x, y), col, k * (1 - d)));
    }
}
// coursed bricks or blocks on a face, lit at each course's top and down the left edge
void coursed(Canvas& c, int x0, int y0, int x1, int y1, const Ramp& R, int bw, int bh, uint32_t seed) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      const int row = (y - y0) / bh, sh = (row & 1) * (bw / 2);
      const bool mortar = (y - y0) % bh == bh - 1 || ((x - x0 + sh) % bw) == 0;
      int k = mortar ? 1 : 2;
      if (!mortar && hash3((x - x0 + sh) / bw, row, seed) % 4 == 0) k = 3;
      if (!mortar && (y - y0) % bh == 0) k = std::min(4, k + 1);
      if (x == x0) k = std::min(4, k + 1);
      if (x == x1) k = std::max(0, k - 1);
      c.set(x, y, R[k]);
    }
}

// ---- hearths (48x64, frames 0..3; the wall's foot at row 48, the floor below) ----------------------------------
void hearthKit(Canvas& c, int a, int frame) {
  const int H = c.h, foot = H - 16;
  switch (a) {
    case K_JADE: {
      // a stovepipe up the wall, a red paper charm, the brick kang stove with an iron wok set in its top, the fire's
      // mouth glowing in its front, a teapot keeping warm on its corner
      for (int y = 2; y <= 32; y++) {
        c.set(31, y, kIron[3]); c.set(32, y, kIron[2]); c.set(33, y, kIron[1]);
        if (y % 9 == 0) hline(c, 30, 34, y, kIron[4]);
      }
      for (int y = 8; y <= 19; y++)
        for (int x = 8; x <= 19; x++) {
          const float d = std::fabs(x - 13.5f) + std::fabs(y - 13.5f);
          if (d > 6.0f) continue;
          c.set(x, y, kRed[d > 5.0f ? 1 : 3]);
        }
      hline(c, 12, 15, 12, kGold[4]); vline(c, 13, 12, 16, kGold[3]); hline(c, 12, 15, 15, kGold[3]);
      const int top = 30, front = 36, bottom = foot + 9;
      for (int y = top; y < front; y++)
        for (int x = 5; x <= 42; x++) c.set(x, y, kBrickGrey[(x == 5 || y == top) ? 4 : (x == 42 ? 2 : 3)]);
      for (int y = 30; y <= 36; y++)
        for (int x = 9; x <= 26; x++) {
          const float d = std::hypot((x + 0.5f - 17.5f) / 8.0f, (y + 0.5f - 33.2f) / 2.9f);
          if (d > 1.0f) continue;
          c.set(x, y, d > 0.86f ? kInk : kIron[d < 0.45f ? 0 : (x < 16 ? 2 : 1)]);
        }
      hline(c, 13, 21, 32, kIron[3]);
      coursed(c, 5, front, 42, bottom - 2, kBrickGrey, 6, 3, 61u);
      for (int y = front + 8; y <= bottom - 3; y++)
        for (int x = 15; x <= 28; x++) {
          const float dx = (x + 0.5f - 21.5f) / 7.0f;
          if (y < front + 8 + dx * dx * 3.0f) continue;
          c.set(x, y, y > bottom - 6 ? kFire[1] : rgba(30, 18, 26));
        }
      kitFlame(c, 21.5f, (float)bottom - 4, 6.5f + (frame & 1), 9.0f, frame, kFire);
      for (int x = 4; x <= 43; x++) { c.set(x, bottom - 1, kStone[3]); c.set(x, bottom, kStone[1]); }
      glowOn(c, 21.5f, (float)bottom - 4, 14, 8, kFire[3], 0.35f);
      ball(c, 37.5, 28.5, 3.2, 2.6, kSilkTeal);
      c.set(41, 28, kSilkTeal[1]); c.set(42, 27, kSilkTeal[2]); c.set(37, 25, kSilkTeal[4]); c.set(36, 28, kWhite);
      break;
    }
    case K_MARSH: {
      // a smoke hood of bound reeds over a raised clay fire ring, a pot on a chain, logs and the fire
      for (int y = 4; y <= 30; y++) {
        const float t = (y - 4) / 26.0f;
        const int half = 5 + (int)std::lround(t * 13.0f);
        for (int x = 24 - half; x <= 23 + half; x++) {
          int k = ((x + (y / 3)) % 3 == 0) ? 1 : 2;
          if (x == 24 - half) k = 3;
          if (x >= 22 + half) k = 1;
          if (y % 6 == 0) k = std::max(0, k - 1);   // the bindings
          if (y == 30) k = 0;
          c.set(x, y, kReed[k]);
        }
      }
      box(c, 21, 1, 26, 4, kInk);   // the smoke hole
      for (int y = 31; y <= 40; y++) c.set(24, y, (y & 1) ? kIron[2] : kIron[3]);
      ball(c, 24.0, 43.5, 5.0, 3.6, kTerra);
      hline(c, 20, 28, 40, kTerra[1]); c.set(21, 42, kTerra[4]);
      for (int y = foot; y <= H - 2; y++)
        for (int x = 3; x <= 44; x++) {
          const float d = std::hypot((x + 0.5f - 24.0f) / 20.0f, (y + 0.5f - (foot + 7.0f)) / 6.5f);
          if (d > 1.0f) continue;
          if (d > 0.72f) {
            const bool lit = (x < 24 && y < foot + 7);
            c.set(x, y, kAdobeR[d > 0.92f ? 1 : (lit ? 4 : 2)]);
          } else c.set(x, y, d < 0.5f ? kFire[1] : rgba(56, 34, 30));
        }
      capsule(c, V(14, foot + 8), V(33, foot + 6), 1.6f, 1.6f, kWood);
      capsule(c, V(16, foot + 5), V(31, foot + 9), 1.3f, 1.3f, kWoodDark);
      kitFlame(c, 24.0f, (float)foot + 7, 9.0f + (frame & 1), 10.0f, frame, kFire);
      glowOn(c, 24.0f, (float)foot + 5, 20, 12, kFire[3], 0.30f);
      break;
    }
    case K_SUN: {
      // a whitewashed adobe mass with a rounded top and a painted band (red, a turquoise step-fret), the hearth niche
      // in it, a clay griddle (comal) on three stones over the fire, a water jar beside it
      for (int y = 10; y <= foot + 6; y++)
        for (int x = 4; x <= 43; x++) {
          const float dx = (x + 0.5f - 24.0f) / 20.0f;
          if (y < 10 + dx * dx * 8.0f) continue;
          int k = x < 8 ? 4 : (x > 40 ? 2 : 3);
          if (hash3(x / 2, y / 2, 91u) % 13 == 0) k = std::max(1, k - 1);
          c.set(x, y, kPlaster[k]);
        }
      for (int x = 6; x <= 41; x++) {
        for (int y = 20; y <= 24; y++) c.set(x, y, kTerra[y == 20 ? 3 : 2]);
        const int q = (x - 6) % 8;
        if (q <= 1) c.set(x, 22, kTurq[3]);
        if (q >= 1 && q <= 3) c.set(x, 21, kTurq[3]);
        if (q == 4 || q == 5) c.set(x, 23, kTurq[2]);
      }
      for (int y = 30; y <= foot + 4; y++)
        for (int x = 13; x <= 34; x++) {
          const float dx = (x + 0.5f - 24.0f) / 11.0f;
          if (y < 30 + dx * dx * 6.0f) continue;
          c.set(x, y, y < 36 ? rgba(44, 30, 32) : rgba(30, 20, 24));
        }
      for (int x = 13; x <= 34; x++) { const float dx = (x + 0.5f - 24.0f) / 11.0f; c.set(x, (int)(30 + dx * dx * 6.0f) - 1, kPlaster[1]); }
      kitFlame(c, 24.0f, (float)foot + 3, 6.0f + (frame & 1), 9.0f, frame, kFire);
      for (int s = 0; s < 3; s++) ball(c, 17.0 + s * 7, foot + 2.0, 2.0, 1.6, kStone);
      for (int x = 15; x <= 33; x++) { c.set(x, foot - 4, kEbony[3]); c.set(x, foot - 3, kEbony[1]); }   // the comal
      hline(c, 19, 22, foot - 5, kCloth[4]); hline(c, 25, 28, foot - 5, kCloth[3]);                     // flatbreads on it
      glowOn(c, 24.0f, (float)foot, 14, 9, kFire[3], 0.35f);
      for (int y = foot + 7; y <= foot + 11; y++) for (int x = 3; x <= 44; x++) c.set(x, y, kPlaster[y == foot + 7 ? 4 : (y == foot + 11 ? 1 : 2)]);
      ball(c, 40.5, foot + 1.0, 3.6, 4.2, kTerra);
      hline(c, 38, 43, foot - 3, kTerra[4]); hline(c, 39, 42, foot - 2, kInk);
      break;
    }
    case K_SYLVAN: {
      // two living roots grow out of the wall and arch over an alcove; a stone bowl of spirit fire glows in it, moss on
      // the roots, small glowing caps and leaves
      for (int y = 4; y <= foot + 6; y++)
        for (int x = 4; x <= 43; x++) {
          const float dx = (x + 0.5f - 24.0f) / 20.0f, dy = (y - (foot + 6.0f)) / (foot + 2.0f);
          const float r = std::sqrt(dx * dx + dy * dy);
          if (r > 1.0f || r < 0.70f) continue;
          int k = 2;
          if (dx < -0.2f) k = 3;
          if (dx > 0.3f) k = 1;
          if (r > 0.96f || r < 0.73f) k = std::max(0, k - 1);
          if (((x + y * 2) % 7) == 0) k = std::max(0, k - 1);   // bark furrows
          c.set(x, y, kLiveBark[k]);
          if (dy < -0.55f && hash3(x / 2, y / 2, 33u) % 3 == 0) c.set(x, y, kMoss[(x < 24) ? 3 : 2]);
        }
      for (int y = 14; y <= foot + 5; y++)
        for (int x = 9; x <= 38; x++) {
          const float dx = (x + 0.5f - 24.0f) / 20.0f, dy = (y - (foot + 6.0f)) / (foot + 2.0f);
          if (std::sqrt(dx * dx + dy * dy) >= 0.70f) continue;
          c.set(x, y, mix(rgba(20, 40, 34), rgba(34, 60, 46), (y - 14) / 40.0f));
        }
      for (int y = foot; y <= foot + 4; y++)
        for (int x = 15; x <= 32; x++) {
          const float d = std::hypot((x + 0.5f - 24.0f) / 9.0f, (y - (foot - 1.0f)) / 5.0f);
          if (d > 1.0f) continue;
          c.set(x, y, kStone[x < 22 ? 3 : (x > 28 ? 1 : 2)]);
        }
      hline(c, 15, 32, foot - 1, kStone[4]);
      kitFlame(c, 24.0f, (float)foot - 1, 10.0f + (frame & 1), 9.0f, frame, kSpirit);
      glowOn(c, 24.0f, (float)foot - 6, 18, 16, kSpirit[3], 0.35f);
      static const int gx[4] = {8, 12, 38, 35}, gy[4] = {36, 26, 30, 40};
      for (int i = 0; i < 4; i++) { c.set(gx[i], gy[i], kGlowElfP[4]); c.set(gx[i] + 1, gy[i], kGlowElfP[3]); c.set(gx[i], gy[i] + 1, kLiveBark[1]); }
      for (int i = 0; i < 6; i++) {
        const int lx = 10 + i * 6, ly = 6 + (i % 3) * 3;
        c.set(lx, ly, kLeaf[3]); c.set(lx + 1, ly, kLeaf[2]); c.set(lx, ly + 1, kLeaf[1]);
      }
      for (int y = foot + 7; y <= foot + 11; y++) for (int x = 8; x <= 39; x++) c.set(x, y, (y == foot + 7) ? kMoss[3] : kStone[y == foot + 11 ? 1 : 2]);
      break;
    }
    case K_STEPPE: {
      // the yurt's iron stove: a black box on legs, its pipe straight up, a brass kettle on top, the fire behind a
      // grate, a felt mat in front
      for (int y = 0; y <= 30; y++) {
        c.set(22, y, kIron[3]); c.set(23, y, kIron[2]); c.set(24, y, kIron[1]); c.set(25, y, kIron[0]);
        if (y % 10 == 4) hline(c, 21, 26, y, kIron[4]);
      }
      box34(c, 10, 29, 28, 5, 14, kEbony);
      for (int x = 10; x <= 37; x++) c.set(x, 29, kIron[3]);
      for (int y = 37; y <= 44; y++)
        for (int x = 16; x <= 31; x++) c.set(x, y, y > 41 ? kFire[2] : kFire[1]);
      kitFlame(c, 24.0f, 44.0f, 5.0f + (frame & 1), 10.0f, frame, kFire);
      for (int x = 16; x <= 31; x += 3) vline(c, x, 37, 44, kIron[2]);
      for (int lx : {11, 36}) vline(c, lx, 48, foot + 2, kIron[1]);
      ball(c, 31.5, 26.0, 4.0, 3.0, kBrass);
      c.set(35, 24, kBrass[2]); c.set(36, 23, kBrass[3]); hline(c, 29, 33, 22, kBrass[4]);
      glowOn(c, 24.0f, 44.0f, 16, 10, kFire[3], 0.25f);
      for (int y = foot + 4; y <= H - 3; y++)
        for (int x = 4; x <= 43; x++) {
          const bool border = y == foot + 4 || y == H - 3 || x == 4 || x == 43;
          int k = 2;
          if (!border && ((x - 4) % 10 == 5) && (y - foot) % 4 == 2) k = 4;
          c.set(x, y, border ? kCloth[4] : kFelt[k]);
        }
      break;
    }
    case K_DUNE: {
      // a keyhole niche framed in blue-and-white tiles, a brass lamp hanging in it, a brass brazier of coals before it
      for (int y = 4; y <= foot - 2; y++)
        for (int x = 8; x <= 39; x++) {
          const float dx = (x + 0.5f - 24.0f) / 16.0f, dy = (y - 18.0f) / 14.0f;
          const bool inKey = (dx * dx + dy * dy <= 1.0f && y <= 24) || (y > 18 && std::fabs(dx) < 0.62f);
          if (!inKey) continue;
          const float ix = (x + 0.5f - 24.0f) / 13.0f, iy = (y - 18.0f) / 11.0f;
          const bool inner = (ix * ix + iy * iy <= 1.0f && y <= 24) || (y > 18 && std::fabs(ix) < 0.55f && y < foot - 2);
          if (inner) c.set(x, y, mix(rgba(40, 26, 30), rgba(56, 36, 34), (y - 4) / 40.0f));
          else c.set(x, y, ((x + y) & 3) == 0 ? kWhite : (((x / 2 + y / 2) & 1) ? kIndigo[3] : kTurq[3]));
        }
      for (int y = 8; y <= 18; y++) c.set(24, y, kBrass[2]);
      ball(c, 24.0, 21.0, 3.0, 2.4, kBrass); c.set(23, 23, kGlow[4]); c.set(24, 24, kGlow[3]);
      for (int lx : {17, 24, 31}) for (int y = foot - 4; y <= foot + 4; y++) c.set(lx, y, kBrass[lx < 24 ? 3 : 1]);
      for (int y = foot - 10; y <= foot - 4; y++)
        for (int x = 12; x <= 35; x++) {
          const float d = std::hypot((x + 0.5f - 24.0f) / 12.0f, (y - (foot - 10.0f)) / 6.5f);
          if (d > 1.0f) continue;
          c.set(x, y, kBrass[x < 20 ? 4 : (x > 30 ? 1 : 2)]);
        }
      for (int x = 13; x <= 34; x++) { c.set(x, foot - 10, kBrass[4]); c.set(x, foot - 11, (x + frame) % 3 ? kFire[2] : kFire[3]); }
      kitFlame(c, 24.0f, (float)foot - 11, 6.0f + (frame & 1), 12.0f, frame, kFire);
      glowOn(c, 24.0f, (float)foot - 12, 16, 10, kFire[3], 0.3f);
      for (int y = foot + 6; y <= foot + 11; y++) for (int x = 6; x <= 41; x++) c.set(x, y, (((x - 6) / 4 + (y - foot) / 3) & 1) ? kTurq[2] : kWhite);
      for (int x = 6; x <= 41; x++) c.set(x, foot + 11, kIndigo[1]);
      break;
    }
    case K_STAR: {
      // a pale marble hearth under a pointed arch, a silver star inlaid over it, a cold blue fire burning without wood
      for (int y = 2; y <= foot + 6; y++)
        for (int x = 7; x <= 40; x++) {
          const float dx = std::fabs(x + 0.5f - 24.0f) / 17.0f;
          if (y < 2 + std::pow(dx, 0.6f) * 16.0f) continue;
          int k = x < 10 ? 4 : (x > 37 ? 2 : 3);
          if (((y / 6) & 1) == 0 && (x % 12) == 0) k = 2;
          c.set(x, y, kMarble[k]);
        }
      for (int y = 22; y <= foot + 4; y++)
        for (int x = 15; x <= 32; x++) {
          const float dx = std::fabs(x + 0.5f - 24.0f) / 9.0f;
          if (y < 22 + std::pow(dx, 0.6f) * 9.0f) continue;
          c.set(x, y, mix(rgba(30, 34, 60), rgba(20, 22, 44), (y - 22) / 26.0f));
        }
      for (int k = -3; k <= 3; k++) { c.set(24 + k, 12, kSilver[4 - std::abs(k) / 2]); c.set(24, 12 + k, kSilver[4 - std::abs(k) / 2]); }
      c.set(23, 11, kSilver[3]); c.set(25, 13, kSilver[2]); c.set(23, 13, kSilver[3]); c.set(25, 11, kSilver[3]);
      kitFlame(c, 24.0f, (float)foot + 3, 12.0f + (frame & 1), 9.0f, frame, kColdFire);
      glowOn(c, 24.0f, (float)foot - 6, 18, 18, kColdFire[3], 0.30f);
      for (int y = foot + 7; y <= foot + 11; y++) for (int x = 5; x <= 42; x++) c.set(x, y, kMarble[y == foot + 7 ? 4 : (y == foot + 11 ? 1 : 3)]);
      break;
    }
    default: break;
  }
}

// ---- shelves (28x32, against the back wall) ---------------------------------------------------------------
void shelfKit(Canvas& c, int a) {
  const int W = c.w, H = c.h, base = H - 1;
  switch (a) {
    case K_JADE: {   // a red lacquered cabinet of pigeonholes holding rolled scrolls, a vase on top
      box(c, 1, 6, W - 2, base, kLacquer[2]);
      for (int y = 6; y <= base; y++) { c.set(1, y, kLacquer[4]); c.set(2, y, kLacquer[3]); c.set(W - 3, y, kLacquer[1]); c.set(W - 2, y, kLacquer[0]); }
      hline(c, 0, W - 1, 6, kLacquer[4]); hline(c, 0, W - 1, 7, kLacquer[3]);
      for (int r = 0; r < 3; r++)
        for (int k = 0; k < 3; k++) {
          const int x0 = 4 + k * 7, y0 = 10 + r * 7;
          box(c, x0, y0, x0 + 5, y0 + 5, kEbony[0]);
          for (int s = 0; s < 3; s++) {   // scroll ends
            const int sx = x0 + 1 + (s % 2) * 2, sy = y0 + 1 + s;
            if (hash3(r, k * 3 + s, 5u) % 4 == 0) continue;
            c.set(sx, sy, kBone[4]); c.set(sx + 1, sy, kBone[2]);
            if (s == 1) c.set(sx + 1, sy, ((r + k) & 1) ? kRed[3] : kIndigo[3]);
          }
          hline(c, x0, x0 + 5, y0 + 6, kLacquer[3]);
        }
      ball(c, 21.0, 3.0, 2.6, 2.8, kSilkTeal); c.set(20, 1, kSilkTeal[4]);
      c.set(7, 4, kGold[4]); c.set(8, 4, kGold[2]);
      break;
    }
    case K_MARSH: {   // a pole rack: a fishing net hung to dry, gourds and baskets on two plank shelves
      for (int y = 2; y <= base; y++) {
        c.set(2, y, kBamboo[3]); c.set(3, y, kBamboo[1]); c.set(W - 4, y, kBamboo[3]); c.set(W - 3, y, kBamboo[1]);
        if (y % 7 == 0) { c.set(2, y, kBamboo[4]); c.set(W - 4, y, kBamboo[4]); }
      }
      for (int y = 3; y <= 14; y++)
        for (int x = 4; x <= W - 5; x++)
          if (((x + y) % 4 == 0) || ((x - y + 64) % 4 == 0)) c.set(x, y + (int)std::lround(std::sin(x * 0.4f)), kCloth[(x + y) % 8 == 0 ? 1 : 2]);
      hline(c, 2, W - 3, 2, kBamboo[4]);
      for (int sy : {17, 27}) { hline(c, 1, W - 2, sy, kReed[4]); hline(c, 1, W - 2, sy + 1, kReed[1]); }
      ball(c, 8.0, 14.5, 2.8, 2.6, kThatch); c.set(8, 11, kWood[2]);
      ball(c, 15.0, 15.0, 2.2, 2.0, kBamboo);
      for (int y = 21; y <= 26; y++) for (int x = 5; x <= 12; x++) c.set(x, y, ((x + y) & 1) ? kReed[3] : kReed[2]);
      hline(c, 5, 12, 21, kReed[4]);
      for (int y = 22; y <= 26; y++) for (int x = 16; x <= 22; x++) c.set(x, y, ((x * 2 + y) % 3) ? kReed[2] : kReed[1]);
      c.set(18, 21, kWater[3]); c.set(19, 21, kWater[4]); c.set(20, 21, kWater[2]);   // a fish laid on the basket
      break;
    }
    case K_SUN: {   // a whitewashed wall niche of two arched bays: painted pots, a maize bundle, a folded blanket
      for (int y = 2; y <= base; y++) for (int x = 1; x <= W - 2; x++) c.set(x, y, kPlaster[x < 3 ? 4 : (x > W - 4 ? 2 : 3)]);
      for (int b = 0; b < 2; b++) {
        const int y0 = 5 + b * 13;
        for (int y = y0; y <= y0 + 10; y++)
          for (int x = 4; x <= W - 5; x++) {
            const float dx = (x + 0.5f - W * 0.5f) / (W * 0.5f - 4);
            if (y < y0 + dx * dx * 3.0f) continue;
            c.set(x, y, y > y0 + 8 ? kPlaster[1] : rgba(120, 88, 70));
          }
      }
      ball(c, 9.0, 12.0, 3.0, 3.0, kTerra); hline(c, 7, 11, 11, kEbony[2]); c.set(8, 9, kTerra[4]);
      ball(c, 18.0, 12.5, 2.6, 2.4, kTerra); c.set(17, 12, kTurq[3]); c.set(19, 12, kTurq[3]);
      for (int i = 0; i < 4; i++) { vline(c, 6 + i * 2, 19, 26, kGold[(i & 1) ? 2 : 3]); c.set(6 + i * 2, 19, kLeaf[2]); }
      for (int y = 22; y <= 26; y++) for (int x = 15; x <= 23; x++) c.set(x, y, ((y + x / 3) & 1) ? kTerra[3] : kTurq[2]);
      for (int x = 1; x <= W - 2; x++) c.set(x, base, kPlaster[0]);
      break;
    }
    case K_SYLVAN: {   // shelves grown from a living trunk: branch boards, glowing jars, leaves at the corners
      for (int y = 1; y <= base; y++) for (int x = 1; x <= 4; x++) c.set(x, y, kLiveBark[x == 1 ? 3 : (x == 4 ? 1 : 2)]);
      for (int y = 1; y <= base; y++) for (int x = W - 5; x <= W - 2; x++) c.set(x, y, kLiveBark[x == W - 5 ? 2 : 1]);
      for (int sy : {9, 19, 29}) {
        for (int x = 2; x <= W - 3; x++) { c.set(x, sy, kLiveBark[3]); c.set(x, sy + 1, kLiveBark[1]); }
        c.set(5, sy - 1, kLeaf[3]); c.set(W - 7, sy - 1, kLeaf[2]);
      }
      for (int r = 0; r < 3; r++)
        for (int k = 0; k < 3; k++) {
          if (hash3(r, k, 9u) % 4 == 0) continue;
          const int jx = 7 + k * 6, jy = 8 + r * 10;
          const Ramp& J = ((r + k) % 2) ? kGlowElfP : kSpirit;
          for (int y = jy - 4; y <= jy; y++) for (int x = jx; x <= jx + 3; x++) c.set(x, y, J[(x == jx) ? 4 : (y == jy ? 2 : 3)]);
          c.set(jx + 1, jy - 5, kLiveBark[3]); c.set(jx + 2, jy - 5, kLiveBark[2]);
        }
      for (int i = 0; i < 5; i++) {
        const int lx = 2 + i * 6;
        c.set(lx, 1 + (i & 1), kLeaf[3]); c.set(lx + 1, 1 + (i & 1), kLeaf[2]); c.set(lx, 2 + (i & 1), kLeaf[1]);
      }
      break;
    }
    case K_STEPPE: {   // a stack of painted chests, orange with gold scrolls; a bowl and a horse-head fiddle on top
      for (int k = 0; k < 3; k++) {
        const int y0 = 5 + k * 9, x0 = 1 + (k == 1 ? 1 : 0), w = W - 2 - (k == 1 ? 2 : 0);
        box34(c, x0, y0, w, 2, 7, kOrangeP);
        for (int x = x0 + 2; x < x0 + w - 2; x++)
          if ((x + k) % 5 == 0) { c.set(x, y0 + 4, kGold[4]); c.set(x + 1, y0 + 5, kGold[3]); c.set(x - 1, y0 + 5, kGold[3]); }
        hline(c, x0 + 1, x0 + w - 2, y0 + 8, kOrangeP[0]);
        c.set(x0 + w / 2, y0 + 6, kBrass[4]);
      }
      ball(c, 7.0, 3.0, 3.0, 1.6, kBone);
      line(c, 18, 0, 22, 4, kWood[3]); ball(c, 22.0, 4.0, 2.2, 1.4, kWood); c.set(18, 0, kWood[4]);
      break;
    }
    case K_DUNE: {   // carved cedar with two keyhole niches: a brass lamp, glazed pottery, a rolled rug on top
      for (int y = 3; y <= base; y++) for (int x = 1; x <= W - 2; x++) c.set(x, y, kCedar[x < 3 ? 4 : (x > W - 4 ? 1 : 2)]);
      hline(c, 0, W - 1, 3, kCedar[4]);
      for (int b = 0; b < 2; b++) {
        const float cx = 8.5f + b * 11.0f;
        for (int y = 7; y <= 27; y++)
          for (int x = (int)cx - 5; x <= (int)cx + 5; x++) {
            const float d = std::hypot((x + 0.5f - cx) / 4.5f, (y - 11.0f) / 4.5f);
            if (!((d <= 1.0f && y <= 12) || (y > 11 && std::fabs(x + 0.5f - cx) < 3.0f))) continue;
            c.set(x, y, rgba(40, 22, 24));
          }
      }
      ball(c, 8.5, 25.0, 2.6, 2.2, kBrass); c.set(8, 22, kGlow[4]);
      ball(c, 19.5, 24.5, 2.6, 3.0, kTurq); c.set(19, 21, kIndigo[3]);
      for (int x = 3; x <= W - 4; x++) for (int y = 0; y <= 2; y++) c.set(x, y, ((x / 3) & 1) ? kFelt[y == 0 ? 3 : 2] : kIndigo[y == 0 ? 3 : 2]);
      for (int x = 1; x <= W - 2; x++) c.set(x, base, kCedar[0]);
      break;
    }
    case K_STAR: {   // a pale shelf of slender white wood: silver-bound books, a crystal orb, a silver star crest
      for (int y = 2; y <= base; y++) for (int x = 3; x <= W - 4; x++) c.set(x, y, rgba(46, 52, 78));
      for (int y = 2; y <= base; y++) { c.set(1, y, kMarble[4]); c.set(2, y, kMarble[3]); c.set(W - 3, y, kMarble[2]); c.set(W - 2, y, kMarble[1]); }
      for (int s = 0; s < 3; s++) {
        const int sy = 11 + s * 10;
        hline(c, 1, W - 2, sy, kMarble[4]); hline(c, 1, W - 2, sy + 1, kMarble[2]);
        int x = 4;
        for (int i = 0; x < W - 6 && i < 8; i++) {
          const int h = 5 + (int)(hash3(i, s, 31u) % 3);
          const Ramp& R = (i % 3 == 0) ? kSkyCloth : ((i % 3 == 1) ? kIndigo : kSilver);
          for (int y = sy - h; y < sy; y++) { c.set(x, y, R[3]); c.set(x + 1, y, R[2]); }
          c.set(x, sy - h + 1, kSilver[4]);
          x += 2 + (hash3(i, s, 37u) % 4 == 0 ? 2 : 0);
        }
      }
      ball(c, W - 8.0, 18.0, 2.6, 2.6, kCrystal);
      hline(c, 0, W - 1, 2, kMarble[4]);
      c.set(W / 2, 0, kSilver[4]); c.set(W / 2 - 1, 1, kSilver[3]); c.set(W / 2 + 1, 1, kSilver[3]);
      break;
    }
    default: break;
  }
}

// ---- beds (20x32 one tile, 20x48 the two-tile bed) -------------------------------------------------------------
void bedKit(Canvas& c, int a) {
  const int W = c.w, H = c.h;
  const bool lng = H >= 48;
  const int head = lng ? 6 : 2;      // where the bed's head piece begins (the wall above the head)
  Ramp F = kWood, Q = kRed, P = kCloth;
  int headKind = 0, pattern = 0, low = 0;
  switch (a) {
    case K_JADE: F = kEbony; Q = kSilkTeal; headKind = 1; pattern = 4; low = 1; break;                    // a canopy panel, silk
    case K_MARSH: F = kBamboo; Q = ramp(rgba(196, 120, 64)); headKind = 0; pattern = 1; low = 2; break;   // a reed mat, stripes
    case K_SUN: F = kPlaster; Q = kTerra; headKind = 0; pattern = 2; low = 2; break;                      // a serape on a bench
    case K_SYLVAN: F = kLiveBark; Q = kLeafBed; headKind = 3; pattern = 3; low = 1; break;               // a branch arch, leaves
    case K_STEPPE: F = kOrangeP; Q = kFelt; headKind = 0; pattern = 5; low = 1; break;                   // painted frame, felt
    case K_DUNE: F = kCedar; Q = kIndigo; headKind = 4; pattern = 5; low = 2; break;                     // a divan, bolsters
    case K_STAR: F = kMarble; Q = kSkyCloth; headKind = 2; pattern = 0; low = 0; break;                  // a crescent
    default: break;
  }
  const int hb1 = head + (headKind == 0 ? 4 : 10);   // the head piece's bottom row
  for (int y = head; y <= hb1; y++)
    for (int x = 1; x <= W - 2; x++) {
      const float dx = (x + 0.5f - W * 0.5f) / (W * 0.5f - 1);
      uint32_t col = 0;
      switch (headKind) {
        case 1: {   // a lacquered frame with a red silk hanging in it
          const bool frame = x <= 2 || x >= W - 3 || y == head || y == head + 1;
          col = frame ? F[(x <= 2 || y == head) ? 4 : 2] : kLacquer[(y - head) % 4 == 0 ? 3 : 2];
          if (!frame && (x == 6 || x == W - 7)) col = kGold[3];
          break;
        }
        case 2: {   // a crescent of pale wood
          const float r = std::hypot(dx, (y - hb1) / (float)(hb1 - head + 1));
          if (r > 1.0f || r < 0.55f) continue;
          col = F[dx < 0 ? 4 : 2];
          break;
        }
        case 3: {   // an arch of living branches, leaves along it
          const float r = std::hypot(dx, (y - hb1) / (float)(hb1 - head + 1));
          if (r > 1.0f || r < 0.72f) continue;
          col = ((x + y) % 4 == 0) ? kLeaf[3] : F[dx < 0 ? 3 : 1];
          break;
        }
        case 4: {   // bolsters against the wall, gold bands
          const float dy = (y - (head + hb1) * 0.5f) / ((hb1 - head) * 0.5f + 0.5f);
          col = Q[std::fabs(dy) > 0.7f ? 1 : (dy < 0 ? 4 : 3)];
          if ((x / 4) % 2 == 0 && std::fabs(dy) < 0.4f) col = kGold[3];
          break;
        }
        default: col = F[(y == head) ? 4 : (x <= 2 ? 3 : 2)]; break;   // a low rail
      }
      c.set(x, y, col);
    }
  if (headKind == 2) { c.set(W / 2, head + 2, kSilver[4]); c.set(W / 2 - 1, head + 3, kSilver[3]); c.set(W / 2 + 1, head + 3, kSilver[3]); c.set(W / 2, head + 4, kSilver[2]); }
  const int top = hb1 + 1, frontH = low == 2 ? 3 : (low == 1 ? 4 : 5), cy1 = H - 1 - frontH;
  for (int y = top; y <= cy1; y++)
    for (int x = 1; x <= W - 2; x++) {
      uint32_t col;
      if (y <= top + 3) {   // the pillow
        const float dx = (x + 0.5f - W * 0.5f) / (W * 0.5f - 2.5f), dy = (y - (top + 1.5f)) / 2.4f;
        if (dx * dx * dx * dx + dy * dy > 1.0f) { c.set(x, y, (a == K_MARSH || a == K_SUN) ? kReed[2] : P[2]); continue; }
        col = (dy < -0.2f && dx < 0.4f) ? kWhite : P[3];
        if (a == K_JADE) col = dy < 0 ? kLacquer[3] : kLacquer[2];   // a firm round pillow of red silk
      } else {
        const int yy = y - top - 4;
        int k = 2;
        if (std::sin(x * 0.9f + yy * 0.25f) * 0.5f > 0.35f) k = 3;
        if (x <= 2) k = std::min(4, k + 1);
        if (x >= W - 3) k = std::max(0, k - 1);
        bool done = false;
        switch (pattern) {
          case 1: if (yy % 6 < 2) k = (k + 2) % 5; break;                                                       // woven stripes
          case 2: { const int band = yy % 9; if (band < 4) { col = band < 2 ? kTurq[3] : kGold[3]; done = true; } break; }   // a serape
          case 3: if (((x * 3 + yy * 5) % 11) == 0) { col = kGold[3]; done = true; } break;                       // veined leaves
          case 4: if (((x + yy) % 6) == 0 || ((x - yy + 60) % 6) == 0) k = std::min(4, k + 1); break;            // a silk lattice
          case 5: if ((yy % 8 == 3) || ((x % 6) == 3 && yy % 8 > 3 && yy % 8 < 7)) { col = kGold[3]; done = true; } break;   // a band
          default: break;
        }
        if (!done) col = Q[std::clamp(yy == 0 ? k - 1 : k, 0, 4)];
      }
      c.set(x, y, col);
    }
  for (int y = cy1 + 1; y <= H - 1; y++)
    for (int x = 0; x < W; x++) {
      int k = y == cy1 + 1 ? 3 : (y == H - 1 ? 0 : 2);
      if (x <= 1) k = std::min(4, k + 1);
      if (x >= W - 2) k = std::max(0, k - 1);
      c.set(x, y, F[k]);
    }
  for (int y = top; y <= cy1; y++) { c.set(0, y, F[3]); c.set(W - 1, y, F[1]); }
}

// ---- chests of drawers, wardrobes and cupboards (16 x 30 / 36 / 34) ------------------------------------------
void cabinetKit(Canvas& c, int a, Prop p) {
  const int W = c.w, H = c.h, base = H - 1;
  const int topY = p == Prop::Dresser ? 12 : 2;   // a chest of drawers is low; the others reach up the wall
  Ramp F = kWood;
  switch (a) {
    case K_JADE: F = kLacquer; break;
    case K_SUN: F = kTerra; break;
    case K_SYLVAN: F = kLiveBark; break;
    case K_STEPPE: F = kOrangeP; break;
    case K_DUNE: F = kCedar; break;
    case K_STAR: F = kMarble; break;
    default: break;
  }
  if (a == K_MARSH) {
    // (M3b: they read as beds when they were stacked reed baskets) the marsh folk's cupboards are dark bog-oak frames on
    // short legs, raised off the damp floor, with woven reed doors on pegs; a pot of eels or a net-float on top. The
    // wardrobe is tall with a net hung on its side, the cupboard a hutch with an open shelf of gourds over its doors, the
    // chest of drawers low and wide with lidded crocks on it.
    const Ramp& Fr = kWoodDark;
    const int top = p == Prop::Dresser ? 14 : 3, legs = 2;
    // the frame: posts and rails, its top lit
    for (int y = top; y <= base; y++)
      for (int x = 1; x <= W - 2; x++) {
        const bool post = x <= 2 || x >= W - 3, rail = y <= top + 1 || y >= base - legs - 1;
        if (y > base - legs && !(x <= 2 || x >= W - 3)) continue;   // between the legs: the floor shows
        if (!(post || rail)) continue;
        int k = y == top ? 4 : (x <= 2 ? (x == 1 ? 3 : 2) : (x >= W - 3 ? (x == W - 2 ? 0 : 1) : 2));
        c.set(x, y, Fr[k]);
      }
    // the panels: an open shelf of gourds (the hutch), woven reed doors with a peg each
    int py0 = top + 2;
    if (p == Prop::Cupboard) {
      for (int y = py0; y <= py0 + 7; y++) for (int x = 3; x <= W - 4; x++) c.set(x, y, y == py0 + 7 ? Fr[3] : mix(Fr[0], kInk, 0.3f));
      ball(c, 5.5, py0 + 5.0, 2.0, 2.0, kThatch); ball(c, 10.0, py0 + 5.5, 1.6, 1.6, kLeather);
      py0 += 9;
    }
    const int py1 = base - legs - 2;
    for (int y = py0; y <= py1; y++)
      for (int x = 3; x <= W - 4; x++) {
        if (x == W / 2) { c.set(x, y, Fr[1]); continue; }   // the doors' meeting stile
        const int k = ((x + (y >> 1)) % 3 == 0) ? 1 : ((y & 1) ? 3 : 2);   // the reed weave
        c.set(x, y, kReed[x == 3 || x == W / 2 + 1 ? std::min(4, k + 1) : k]);
      }
    if (py1 > py0 + 2) { c.set(W / 2 - 2, (py0 + py1) / 2, kBone[4]); c.set(W / 2 + 2, (py0 + py1) / 2, kBone[3]); }   // bone pegs
    if (p == Prop::Wardrobe)   // a net hung on its side, a cork float
      for (int y = top + 4; y <= base - 6; y += 2) { c.set(W - 2, y, kCloth[2]); c.set(W - 1, y + 1, kCloth[1]); }
    if (p == Prop::Dresser) { ball(c, 5.0, top - 2.0, 2.4, 2.2, kTerra); ball(c, 11.0, top - 1.5, 2.0, 1.8, kReed); }
    else ball(c, W / 2.0, top - 1.5, 2.0, 1.6, kTerra);
    return;
  }
  box34(c, 1, topY, W - 2, 3, base - topY - 3, F);
  hline(c, 1, W - 2, base, F[0]);
  const int fy0 = topY + 4, fy1 = base - 2;
  if (p == Prop::Dresser) {
    for (int d = 0; d < 3; d++) {
      const int y = fy0 + 1 + d * 4;
      if (y + 2 > fy1) break;
      hline(c, 2, W - 3, y, F[1]); hline(c, 2, W - 3, y + 1, F[3]);
      c.set(W / 2 - 3, y + 2, kBrass[4]); c.set(W / 2 + 2, y + 2, kBrass[3]);
    }
  } else {
    vline(c, W / 2, fy0, fy1, F[0]);
    for (int s = 0; s < 2; s++) {
      const int x0 = s ? W / 2 + 2 : 3, x1 = s ? W - 4 : W / 2 - 2;
      for (int y = fy0 + 2; y <= fy1 - 2; y++) { c.set(x0, y, F[1]); c.set(x1, y, F[3]); }
      hline(c, x0, x1, fy0 + 2, F[1]); hline(c, x0, x1, fy1 - 2, F[3]);
    }
    c.set(W / 2 - 1, (fy0 + fy1) / 2, kBrass[4]); c.set(W / 2 + 1, (fy0 + fy1) / 2, kBrass[3]);
  }
  const int my = (fy0 + fy1) / 2;
  switch (a) {
    case K_JADE:   // black panels and a round brass plate
      for (int y = fy0 + 3; y <= fy1 - 3; y++) { c.set(4, y, kEbony[2]); c.set(W - 5, y, kEbony[2]); }
      ellipse(c, W / 2.0, my, 2.0, 2.0, kBrass[3]); c.set(W / 2 - 1, my - 1, kBrass[4]);
      break;
    case K_SUN:    // a turquoise step-fret band
      for (int x = 2; x <= W - 3; x++) { c.set(x, fy0 + 1, kTurq[3]); c.set(x, fy0 + 2, (x % 4) < 2 ? kTurq[2] : F[2]); }
      break;
    case K_SYLVAN: // a carved leaf on each door, moss on the top
      for (int s = 0; s < 2; s++) { const int lx = s ? W - 6 : 5; for (int k = -2; k <= 2; k++) c.set(lx + (k & 1), my + k, kLeaf[3 - std::abs(k) / 2]); }
      for (int x = 2; x <= W - 3; x += 2) c.set(x, topY, kMoss[3]);
      break;
    case K_STEPPE: // gold scrolls
      for (int x = 3; x <= W - 4; x += 4) { c.set(x, my - 1, kGold[4]); c.set(x + 1, my, kGold[3]); c.set(x, my + 1, kGold[3]); }
      break;
    case K_DUNE:   // a keyhole arch carved in the front
      for (int y = my - 4; y <= my + 4; y++)
        for (int x = W / 2 - 3; x <= W / 2 + 2; x++) {
          const float d = std::hypot((x + 0.5f - W * 0.5f) / 3.0f, (y - (my - 2.0f)) / 2.5f);
          if ((d <= 1.0f && y <= my - 1) || (y > my - 2 && std::fabs(x + 0.5f - W * 0.5f) < 2.0f)) c.set(x, y, F[0]);
        }
      break;
    case K_STAR:   // silver edging and a star
      for (int y = fy0; y <= fy1; y++) { c.set(1, y, kSilver[4]); c.set(W - 2, y, kSilver[2]); }
      c.set(W / 2, my - 3, kSilver[4]); c.set(W / 2 - 1, my - 2, kSilver[3]); c.set(W / 2 + 1, my - 2, kSilver[3]);
      break;
    default: break;
  }
  if (p == Prop::Dresser) {   // something on top: a little vase, a bowl, a glowing jar, a crystal
    switch (a) {
      case K_JADE: ball(c, 5.0, topY - 2.0, 1.8, 2.2, kSilkTeal); break;
      case K_SUN: ball(c, 5.0, topY - 2.0, 2.0, 2.0, kTerra); break;
      case K_SYLVAN: c.set(5, topY - 1, kGlowElfP[4]); c.set(5, topY - 2, kGlowElfP[3]); c.set(6, topY - 1, kGlowElfP[2]); break;
      case K_DUNE: ball(c, 5.0, topY - 1.5, 2.0, 1.4, kBrass); break;
      case K_STAR: ball(c, 5.0, topY - 2.0, 1.8, 1.8, kCrystal); break;
      default: ball(c, 5.0, topY - 1.5, 2.0, 1.4, kBone); break;
    }
  }
}
// ---- wall decor (16x30 painted, set 3 px down in the 16x42 canvas like the classic pieces) ----------------------
// Painting: the people's picture on the wall; Wreath: its small hanging charm
void decorKit(Canvas& t, int a, Prop p) {
  if (p == Prop::Painting) {
    switch (a) {
      case K_JADE: {   // a hanging scroll: an ink landscape on pale silk, a red seal, rollers top and bottom
        hline(t, 3, 12, 4, kEbony[3]); t.set(2, 4, kGold[3]); t.set(13, 4, kGold[2]);
        for (int y = 5; y <= 22; y++) for (int x = 4; x <= 11; x++) t.set(x, y, (x == 4 || x == 11) ? kSilkTeal[2] : kBone[4]);
        for (int x = 5; x <= 10; x++) { const int ridge = 13 - (int)std::lround(3.0f * std::sin(x * 0.9f)); for (int y = ridge; y <= 15; y++) t.set(x, y, y == ridge ? kStone[1] : kStone[3]); }
        for (int x = 6; x <= 9; x++) t.set(x, 18, kStone[3]);
        t.set(9, 8, kStone[2]); t.set(10, 8, kStone[2]);
        t.set(6, 20, kRed[3]); t.set(7, 20, kRed[2]);
        hline(t, 3, 12, 23, kEbony[2]); t.set(2, 23, kGold[2]); t.set(13, 23, kGold[1]);
        break;
      }
      case K_SUN: {    // a woven hanging in bands of zigzags, a fringe
        hline(t, 2, 13, 5, kWood[3]);
        for (int y = 6; y <= 20; y++)
          for (int x = 3; x <= 12; x++) {
            const int band = (y - 6) / 5, z = (x + (y - 6)) % 5;
            const Ramp& B = band == 0 ? kTerra : (band == 1 ? kTurq : kGold);
            t.set(x, y, z == 0 ? kPlaster[4] : B[(x == 3) ? 4 : 2 + ((y & 1) ? 0 : 1)]);
          }
        for (int x = 3; x <= 12; x += 2) { t.set(x, 21, kTerra[2]); t.set(x, 22, kTerra[1]); }
        break;
      }
      case K_SYLVAN: { // a ring of living wood, leaves round it, a pale glowing moth in it
        for (int y = 5; y <= 19; y++)
          for (int x = 1; x <= 14; x++) {
            const float d = std::hypot(x + 0.5f - 8.0f, (y + 0.5f - 12.0f) * 1.1f);
            if (d > 6.8f || d < 5.0f) continue;
            t.set(x, y, kLiveBark[(x < 8 && y < 12) ? 3 : 1]);
          }
        for (int k = 0; k < 6; k++) { const int lx = 3 + k * 2, ly = (k & 1) ? 5 : 19; t.set(lx, ly, kLeaf[3]); t.set(lx + 1, ly, kLeaf[2]); }
        t.set(7, 11, kGlowElfP[4]); t.set(8, 11, kGlowElfP[4]); t.set(6, 12, kGlowElfP[3]); t.set(9, 12, kGlowElfP[3]); t.set(7, 13, kGlowElfP[2]); t.set(8, 13, kGlowElfP[2]);
        break;
      }
      case K_MARSH: {  // a net hung on a peg, a cork float, a dried fish
        t.set(8, 4, kWood[3]); t.set(8, 5, kWood[1]);
        for (int y = 6; y <= 20; y++)
          for (int x = 2; x <= 13; x++) {
            const int half = (y - 6) / 2 + 1;
            if (std::abs(x - 8) > half) continue;
            if (((x + y) % 3 == 0) || ((x - y + 30) % 3 == 0)) t.set(x, y, kCloth[(x + y) % 6 == 0 ? 1 : 2]);
          }
        ball(t, 5.0, 16.0, 1.6, 1.6, kOrangeP);
        for (int y = 9; y <= 17; y++) { t.set(11, y, kStone[3]); t.set(12, y, kStone[2]); }
        t.set(11, 18, kStone[1]); t.set(12, 18, kStone[1]); t.set(11, 9, kWood[2]);
        break;
      }
      case K_STEPPE: { // a felt hanging: red with a cream border and a ram's horn scroll
        for (int y = 5; y <= 19; y++)
          for (int x = 3; x <= 12; x++) {
            const bool border = x == 3 || x == 12 || y == 5 || y == 19;
            t.set(x, y, border ? kCloth[4] : kFelt[(x == 4 || y == 6) ? 3 : 2]);
          }
        for (int k = 0; k < 3; k++) { t.set(6 + k, 10, kCloth[4]); t.set(6, 11 + k, kCloth[4]); t.set(9, 11 + k, kCloth[4]); }
        t.set(5, 9, kCloth[4]); t.set(10, 9, kCloth[4]);
        break;
      }
      case K_DUNE: {   // a little rug hung on the wall: indigo and red, a medallion, tassels
        for (int y = 5; y <= 19; y++)
          for (int x = 3; x <= 12; x++) {
            const bool border = x <= 4 || x >= 11 || y <= 6 || y >= 18;
            const float d = std::fabs(x + 0.5f - 8.0f) + std::fabs(y + 0.5f - 12.0f);
            t.set(x, y, border ? kFelt[(x + y) & 1 ? 2 : 3] : (d < 3.5f ? kGold[3] : kIndigo[2]));
          }
        for (int x = 3; x <= 12; x += 2) t.set(x, 20, kGold[2]);
        break;
      }
      case K_STAR: {   // a star chart: a dark blue disc, silver stars, a constellation's lines
        for (int y = 5; y <= 19; y++)
          for (int x = 1; x <= 14; x++) {
            const float d = std::hypot(x + 0.5f - 8.0f, y + 0.5f - 12.0f);
            if (d > 6.6f) continue;
            t.set(x, y, d > 5.8f ? kSilver[(x < 8) ? 4 : 2] : kIndigo[1]);
          }
        static const int sx[5] = {5, 7, 10, 9, 6}, sy[5] = {9, 11, 10, 14, 15};
        for (int i = 0; i < 4; i++) line(t, sx[i], sy[i], sx[i + 1], sy[i + 1], kIndigo[3]);
        for (int i = 0; i < 5; i++) t.set(sx[i], sy[i], kSilver[4]);
        break;
      }
      default: break;
    }
    return;
  }
  // Wreath: the small charm
  switch (a) {
    case K_JADE:   // a red paper lantern with gold caps and a tassel
      t.set(8, 4, kEbony[2]); t.set(8, 5, kEbony[2]);
      ball(t, 8.0, 11.0, 4.0, 4.6, kLacquer); hline(t, 6, 10, 6, kGold[3]); hline(t, 6, 10, 15, kGold[2]);
      for (int y = 8; y <= 13; y++) t.set(8, y, kLacquer[1]);
      t.set(6, 9, kGlow[3]); t.set(8, 16, kGold[3]); t.set(8, 17, kRed[2]); t.set(8, 18, kRed[1]);
      break;
    case K_SUN:    // a gold sun disc with rays round a turquoise eye
      for (int k = 0; k < 8; k++) {
        const float ang = k * 0.785f;
        t.set(8 + (int)std::lround(std::cos(ang) * 6.0f), 12 + (int)std::lround(std::sin(ang) * 6.0f), kGold[3]);
      }
      ball(t, 8.0, 12.0, 4.0, 4.0, kGold); ball(t, 8.0, 12.0, 1.8, 1.8, kTurq);
      break;
    case K_SYLVAN: // a hoop of flowers and leaves
      for (int k = 0; k < 14; k++) {
        const float ang = k * 0.449f;
        const int x = 8 + (int)std::lround(std::cos(ang) * 5.0f), y = 12 + (int)std::lround(std::sin(ang) * 5.0f);
        t.set(x, y, (k % 3 == 0) ? rgba(240, 214, 230) : kLeaf[(k & 1) ? 3 : 2]);
        if (k % 3 == 0) t.set(x, y + 1, kGold[3]);
      }
      break;
    case K_MARSH:  // a dried gourd on a cord, two heron feathers
      vline(t, 8, 4, 7, kCloth[2]);
      ball(t, 8.0, 12.0, 3.4, 4.2, kBamboo); ball(t, 8.0, 8.5, 1.6, 1.6, kBamboo); t.set(7, 10, kBamboo[4]);
      line(t, 4, 9, 2, 16, kCloth[4]); line(t, 12, 9, 14, 16, kCloth[3]);
      break;
    case K_STEPPE: // a horse-hair standard's tassel on a short staff
      vline(t, 8, 3, 9, kWood[3]); t.set(8, 2, kBrass[4]);
      for (int x = 5; x <= 11; x++) for (int y = 10; y <= 19 - std::abs(x - 8); y++) t.set(x, y, ((x + y) & 1) ? kEbony[3] : kEbony[2]);
      break;
    case K_DUNE:   // a pierced brass lamp hanging from a chain, lit
      for (int y = 3; y <= 7; y++) t.set(8, y, kBrass[2]);
      ball(t, 8.0, 12.0, 3.6, 4.0, kBrass);
      for (int k = 0; k < 4; k++) t.set(6 + (k % 2) * 3, 10 + (k / 2) * 3, kGlow[4]);
      t.set(8, 17, kBrass[3]);
      break;
    case K_STAR:   // a silver crescent moon
      for (int y = 5; y <= 19; y++)
        for (int x = 2; x <= 14; x++) {
          const float d1 = std::hypot(x + 0.5f - 8.0f, y + 0.5f - 12.0f), d2 = std::hypot(x + 0.5f - 10.5f, y + 0.5f - 10.5f);
          if (d1 > 6.0f || d2 < 5.0f) continue;
          t.set(x, y, kSilver[x < 7 ? 4 : 3]);
        }
      break;
    default: break;
  }
}
}  // namespace

// ---------------------------------------------------------------- M3b furniture (art_props.cpp paintProp)
int m3bPropW(Prop p) {
  switch (p) {
    case Prop::FoldScreen: return 24;
    default: return 16;
  }
}
int m3bPropH(Prop p) {
  switch (p) {
    case Prop::FirePitL: case Prop::FirePitM: case Prop::FirePitR: return 26;
    case Prop::Stove: return 46;
    case Prop::TrainingDummy: return 32;
    case Prop::FoldScreen: return 28;
    default: return 16;
  }
}
int m3bPropFrames(Prop p) {
  switch (p) {
    case Prop::FirePitL: case Prop::FirePitM: case Prop::FirePitR: case Prop::Stove: return 4;
    default: return 1;
  }
}

namespace {
// a segment of the long hearth (VISION_PLAN 15.14 mead halls): the tile is the canvas's bottom 16 rows. A trench of
// embers between two kerbs of fieldstone seen from above (the north kerb's top, the trench's far inner side in shade,
// the ember bed, the south kerb's top and its short front face), closed by an end kerb at the hall's ends; low
// flames lick up from the embers (frame 0..3), lit from the top-left like everything else
void firePit(Canvas& c, int part, int frame) {
  const int H = c.h, t0 = H - 16;   // the tile's top row in the canvas
  auto stone = [&](int x, int y, int k) {
    const int bx = (x + (y / 3) * 5) / 5;
    int kk = k + ((hash3(bx, y / 3, 1601) % 3 == 0) ? -1 : 0);
    uint32_t col = kStone[std::clamp(kk, 0, 4)];
    if ((x + (y / 3) * 5) % 5 == 4) col = kStone[std::max(0, kk - 1)];
    return col;
  };
  const bool wEnd = part == 0, eEnd = part == 2;
  for (int y = t0; y < H; y++)
    for (int x = 0; x < 16; x++) {
      const int r = y - t0;   // 0..15 down the tile
      uint32_t col = 0;
      const bool wk = wEnd && x <= 2, ek = eEnd && x >= 13;
      if (r <= 3) col = stone(x, y, r == 0 ? 4 : 3);                          // the north kerb's top
      else if (r >= 11 && r <= 13) col = stone(x, y, r == 11 ? 4 : 3);        // the south kerb's top
      else if (r >= 14) col = stone(x, y, r == 15 ? 0 : 1);                   // its front face
      else if (r <= 5) col = mix(kStone[0], kInk, 0.35f);                      // the trench's far side in shade
      else {
        // (M3b round 3) the trench: grey ash and charcoal (a solid bed of glowing orange read as a strip of lava), the
        // fire burning only where logs are laid across it, the embers glowing round their feet
        // (M5 fixer) the fires burn between the places on the benches (at the segments' joins), never straight above
        // a sitter's head: the near bench's patrons have their backs to the camera and a flame right over a head read
        // as a head on fire (each fire is shared by the two segments either side of its join: each paints its half)
        const float fcA = wEnd ? -99.0f : 0.0f, fcB = eEnd ? -99.0f : 16.0f;
        const float fcx = std::fabs(x + 0.5f - fcA) < std::fabs(x + 0.5f - fcB) ? fcA : fcB;
        const float n = vnoise((x + part * 16) * 0.6f, r * 0.8f, 1603);
        const float glowD = std::fabs(x + 0.5f - fcx) + std::fabs(r - 8.0f) * 0.7f;
        col = n > 0.6f ? mix(kStone[1], kStone[2], 0.4f) : (n > 0.35f ? mix(kStone[0], kStone[1], 0.5f) : mix(kStone[0], kInk, 0.5f));
        if (hash3(x + part * 16, r, 1605) % 9 == 0) col = mix(kStone[2], kStone[3], 0.3f);                  // pale ash flecks
        const float e = vnoise((x + part * 16) * 0.9f, r * 0.9f + frame * 0.45f, 1607);
        if (glowD < 5.5f) {
          const float g = (5.5f - glowD) / 5.5f + (e - 0.5f) * 0.6f;
          if (g > 0.75f) col = kFire[3];
          else if (g > 0.5f) col = kFire[2];
          else if (g > 0.28f) col = mix(kFire[1], kFire[0], 0.3f);
          else if (g > 0.12f) col = mix(kFire[0], kInk, 0.35f);
        } else if (e > 0.82f && r >= 7 && r <= 9) col = mix(kFire[1], kFire[0], 0.5f);                 // a stray ember
        // the logs: two split logs leant together over the fire, their bark dark, their cut ends lit
        for (int lg = -1; lg <= 1; lg += 2) {
          const float lx = fcx + lg * 2.5f - (r - 8.0f) * 0.9f * lg;
          if (std::fabs(x + 0.5f - lx) < 0.9f && r >= 6 && r <= 10) col = (r == 6 || r == 10) ? kFire[2] : (lg < 0 ? kWoodDark[2] : kWoodDark[1]);
        }
      }
      if (wk || ek) col = stone(x, y, (wk && x == 0) || r == 0 ? 4 : (ek && x == 15 ? 1 : 3));
      c.set(x, y, col);
    }
  // the flames: (fix) low licks over the embers, one per segment and set off from the next one's, so the trench reads
  // as a kerbed hearth glowing down the hall and not as a burning strip of floor
  for (int f = 0; f < 2; f++) {   // (the fire at the west join, then the one at the east join: half of each is ours)
    if ((f == 0 && wEnd) || (f == 1 && eEnd)) continue;
    const float cx = (f == 0 ? 0.0f : 16.0f) + (frame % 4 == 1 ? 0.5f : (frame % 4 == 3 ? -0.5f : 0.0f));
    // (M5 fixer r2) low and narrow: the bench's patrons sit before it, and tall tongues between their heads read as a
    // table on fire; a short lick over glowing logs reads as a hearth
    const float h = 3.5f + ((frame * 3 + 1) % 4) * 0.6f, base = (float)(t0 + 9);
    for (int y = (int)(base - h); y <= (int)base; y++)
      for (int x = 0; x < 16; x++) {
        const float u = (base - y) / h;                       // 0 at the embers, 1 at the tip
        const float wdt = 1.5f * (1.0f - u * u) + 0.3f;
        const float sway = std::sin(u * 3.0f + frame * 1.6f) * 0.9f * u;
        const float d = std::fabs(x + 0.5f - cx - sway);
        if (d > wdt) continue;
        const int k = u > 0.75f ? 2 : (d < wdt * 0.4f ? (u < 0.4f ? 4 : 3) : 2);
        c.set(x, y, withA(kFire[k], u > 0.85f ? 170 : 255));
      }
  }
}
// the yurt's stove: an iron firebox on short legs, its door glowing, a kettle on the plate, the flue pipe rising to the
// crown ring (lit on its left flank)
void yurtStove(Canvas& c, int frame) {
  const int H = c.h, base = H - 1;
  const int x0 = 2, x1 = 13, top = base - 13;
  for (int y = top; y <= base - 2; y++)
    for (int x = x0; x <= x1; x++) {
      int k = y <= top + 2 ? (y == top ? 4 : 3) : (x == x0 ? 3 : (x == x1 ? 1 : 2));
      c.set(x, y, kIron[k]);
    }
  hline(c, x0, x1, top + 3, kIron[0]);
  for (int x : {x0 + 1, x1 - 1}) { c.set(x, base - 1, kIron[1]); c.set(x, base, kIron[0]); }   // legs
  // the door: a glowing grate
  const float gl = 0.55f + 0.45f * ((frame % 4 == 1 || frame % 4 == 2) ? 1.0f : 0.6f);
  for (int y = top + 5; y <= base - 4; y++)
    for (int x = x0 + 3; x <= x1 - 3; x++) {
      uint32_t col = (x == x0 + 3 || x == x1 - 3 || y == top + 5 || y == base - 4) ? kIron[0] : mix(kFire[1], kFire[3], gl * hashf(x, y + frame, 1611));
      if ((x - x0) % 2 == 0 && col != kIron[0] && y > top + 5) col = mix(col, kIron[1], 0.5f);   // the grate bars
      c.set(x, y, col);
    }
  // the kettle on the plate
  ball(c, 5.5, top - 1.0, 2.6, 2.0, kBrass);
  c.set(8, top - 2, kBrass[2]); c.set(9, top - 3, kBrass[2]);
  // the flue pipe up to the crown, a collar where its lengths join
  for (int y = 0; y <= top + 1; y++)
    for (int x = 9; x <= 12; x++) c.set(x, y, kIron[x == 9 ? 3 : (x == 12 ? 0 : (x == 10 ? 2 : 1))]);
  for (int y : {top - 10, top - 22}) if (y > 1) hline(c, 8, 13, y, kIron[3]);
}
// a straw dummy on a post: a sacking head, rope bands round the straw body, a crossbar for arms, a dented shield
void trainingDummy(Canvas& c) {
  const int H = c.h, base = H - 1;
  for (int y = 8; y <= base - 1; y++) { c.set(7, y, kWood[3]); c.set(8, y, kWood[1]); }
  hline(c, 4, 11, base, kWood[1]); hline(c, 5, 10, base - 1, kWood[2]);   // the cross-foot
  for (int y = 11; y <= 22; y++)                                          // the straw body
    for (int x = 4; x <= 11; x++) {
      const float d = std::fabs(x - 7.5f) / 4.2f;
      if (d > 1.0f - (y > 20 ? (y - 20) * 0.2f : 0.0f)) continue;
      int k = x <= 5 ? 4 : (x >= 10 ? 1 : ((x + y) % 3 == 0 ? 2 : 3));
      c.set(x, y, kThatch[k]);
    }
  for (int y : {14, 19}) hline(c, 4, 11, y, kLeather[1]);
  hline(c, 1, 14, 12, kWood[2]); hline(c, 1, 14, 13, kWood[1]);          // the arms
  ball(c, 7.5, 7.0, 3.2, 3.0, kCloth);                                     // the head
  c.set(6, 6, kInk); c.set(9, 6, kInk); hline(c, 6, 9, 9, kLeather[1]);   // painted eyes, the neck cord
  for (int y = 13; y <= 20; y++)                                          // the shield on its left arm
    for (int x = 0; x <= 4; x++) {
      if ((x == 0 || x == 4) && (y == 13 || y == 20)) continue;
      c.set(x, y, kWood[x == 0 ? 4 : (x == 4 ? 1 : 2)]);
    }
  c.set(2, 16, kIron[3]); c.set(2, 17, kIron[2]);                          // its boss
}
// a three-leaf folding screen standing in a shallow zigzag: frames of lacquer (or bamboo), paper leaves with a painted
// branch, small feet. The light catches the left leaf, the middle one turns away into shade.
void foldScreen(Canvas& c) {
  const int H = c.h, base = H - 1;
  const Ramp& F = kLacquer;
  for (int leaf = 0; leaf < 3; leaf++) {
    const int lx0 = leaf * 8, lx1 = lx0 + 7;
    const int sk = leaf == 1 ? -2 : 0;   // the middle leaf stands back a step (the zigzag)
    const int y0 = 3 + sk + 2, y1 = base - 1 + sk;
    for (int y = y0; y <= y1; y++)
      for (int x = lx0; x <= lx1; x++) {
        const bool frame = x == lx0 || x == lx1 || y == y0 || y == y1 || y == y0 + 1;
        uint32_t col;
        if (frame) col = F[x == lx0 ? 3 : (x == lx1 ? 1 : 2)];
        else {
          col = leaf == 1 ? kCloth[2] : (leaf == 0 ? kCloth[4] : kCloth[3]);
          // a painted plum branch and blossoms across the leaves
          const int bx = x, by = y - (y0 + 2);
          if (std::abs(by - (12 - (bx * 2) / 5)) <= 0 && bx % 5 != 0) col = kWoodDark[1];
          if ((bx * 7 + by * 3) % 23 == 0 && by > 2 && by < 14) col = kRed[3];
        }
        c.set(x, y, col);
      }
    c.set(lx0 + 1, y1 + 1, F[0]); c.set(lx1 - 1, y1 + 1, F[0]);   // feet
  }
}
}  // namespace

void paintInteriorM3b(Canvas& c, Prop p, int frame) {
  switch (p) {
    case Prop::FirePitL: firePit(c, 0, frame); break;
    case Prop::FirePitM: firePit(c, 1, frame); break;
    case Prop::FirePitR: firePit(c, 2, frame); break;
    case Prop::Stove: yurtStove(c, frame); break;
    case Prop::TrainingDummy: trainingDummy(c); break;
    case Prop::FoldScreen: foldScreen(c); break;
    default: break;
  }
}

Canvas cultureInteriorPiece(int arch, Prop p, int variant) {
  if (p == Prop::Bed) {
    Canvas c(20, (variant & 1) ? 48 : 32);
    bedKit(c, arch);
    outline(c);
    return c;
  }
  Canvas c(propW(p), propH(p));
  switch (p) {
    case Prop::Hearth: hearthKit(c, arch, variant & 3); break;
    case Prop::Bookshelf: shelfKit(c, arch); break;
    case Prop::Shelf: {   // the people's shelves cut to the smaller canvas (24x28): the middle of the 28x32 piece
      Canvas big(28, 32);
      shelfKit(big, arch);
      for (int y = 0; y < c.h; y++)
        for (int x = 0; x < c.w; x++) c.set(x, y, big.get(x + 2, y + 4));
      break;
    }
    case Prop::Dresser: case Prop::Wardrobe: case Prop::Cupboard: cabinetKit(c, arch, p); break;
    case Prop::Painting: case Prop::Wreath: { Canvas t(16, 30); decorKit(t, arch, p); blit(c, t, 0, 3); break; }
    default: return propSprite(p);
  }
  outline(c);
  return c;
}

}  // namespace art
