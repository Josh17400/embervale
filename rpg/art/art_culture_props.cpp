// M3 culture-styled props (rpg/art/art_culture.h propSprite(Prop, PropStyle)), architecture lane: the small things of
// a settlement in its culture's idiom (VISION_PLAN 5.5): fences, wells, lamps, benches, the centrepiece and statues,
// shrines, fire bowls and fountains. Same canvas, anchor and frames as the classic prop; the classic style (and any prop
// without variants) is exactly propSprite(p). (M3b: the culture furniture moved to art_culture_furniture.cpp.)
// Everything in the game's high 3/4 view, lit from the top-left.
#include <cmath>
#include <vector>
#include "rpg/art/art_internal.h"
#include "rpg/art/art_heraldry.h"
#include "rpg/art/art_parts.h"

namespace art {

namespace {

const Ramp kJadeTileP = ramp5(rgba(14, 50, 56), rgba(24, 88, 80), rgba(40, 132, 104), rgba(88, 180, 132), rgba(178, 230, 182));
const Ramp kGlowElfP = ramp5(rgba(40, 110, 120), rgba(70, 170, 170), rgba(130, 220, 200), rgba(196, 248, 226), rgba(240, 255, 246));

enum Cul : int { CU_NONE, CU_FJORD, CU_HIGHLAND, CU_HEART, CU_IMPERIAL, CU_DUNE, CU_STEPPE, CU_MARSH, CU_JADE, CU_RIVER, CU_SUN, CU_SYLVAN, CU_STAR };

struct Mat {
  Ramp wood, stone, metal, cloth;
  int cul;
};
Mat matOf(const PropStyle& st) {
  Mat m;
  m.wood = st.wood ? ramp(opaque(st.wood)) : kWood;
  m.stone = st.stone ? ramp(opaque(st.stone), 0.85f) : kStone;
  m.metal = st.metal ? ramp(opaque(st.metal)) : kIron;
  m.cloth = st.cloth ? ramp(opaque(st.cloth)) : kRed;
  m.cul = st.culture;
  return m;
}

// a small flame (frame 0..3) standing on (cx, base), h tall
void smallFlame(Canvas& c, float cx, float base, float h, int frame) {
  for (int y = 0; y < (int)h; y++) {
    const float t = y / h;
    const float wob = std::sin(t * 5.0f + frame * 1.7f) * 0.8f * t;
    const float half = (1 - t) * (1 - t) * 2.6f + 0.4f;
    for (int x = (int)std::floor(cx - half + wob); x <= (int)std::ceil(cx + half + wob); x++) {
      const float d = std::fabs(x + 0.5f - cx - wob) / std::max(0.6f, half);
      if (d > 1.0f) continue;
      int k = t < 0.3f ? (d < 0.5f ? 4 : 3) : (t < 0.65f ? (d < 0.4f ? 3 : 2) : 1);
      c.set(x, (int)(base - 1 - y), kFire[k]);
    }
  }
}

// a 3/4 box: a top face (lit) depth `topH` rows, a front face `frontH` rows, x0..x0+w-1, its top-left at (x0, y0)
void box34(Canvas& c, int x0, int y0, int w, int topH, int frontH, const Ramp& R) {
  for (int y = 0; y < topH; y++)
    for (int x = 0; x < w; x++) c.set(x0 + x, y0 + y, R[(x == 0 || y == 0) ? 4 : (x == w - 1 ? 2 : 3)]);
  for (int y = 0; y < frontH; y++)
    for (int x = 0; x < w; x++) c.set(x0 + x, y0 + topH + y, R[(x == 0) ? 3 : (x == w - 1 || y == frontH - 1 ? 1 : 2)]);
}

// ---------------------------------------------------------------- fences (seamless along a run)
void fenceStyled(Canvas& c, bool vertical, Fence f, const Mat& M) {
  const int w = c.w, base = c.h - 2;
  if (!vertical) {
    switch (f) {
      case Fence::Wattle: {   // stakes every 4 px, hazel woven between them
        for (int x = 0; x < w; x++)
          for (int y = base - 9; y <= base; y++) {
            const bool stake = (x % 4) == 1;
            if (stake) { c.set(x, y, M.wood[y == base - 9 ? 4 : 2]); continue; }
            const int band = (y - (base - 9)) / 2;
            const bool over = ((x / 4) + band) & 1;
            if (y == base - 9 && (x % 4) != 3) continue;   // a ragged top
            c.set(x, y, over ? M.wood[(y % 2) ? 3 : 2] : M.wood[(y % 2) ? 1 : 0]);
          }
        for (int x = 1; x < w; x += 4) c.set(x, base - 10, M.wood[4]);
        break;
      }
      case Fence::StoneDyke: {   // a dry-stone dyke: the coping stones on top (seen from above), coursed stones below
        for (int x = 0; x < w; x++) {
          const uint32_t hs = hash3(x / 3, 7, 41u);
          for (int y = base - 9; y <= base - 7; y++) c.set(x, y, M.stone[(y == base - 9) ? 4 : (hs % 3 == 0 ? 2 : 3)]);   // copes on edge
          if ((x + 1) % 3 == 0) c.set(x, base - 9, M.stone[2]);
          for (int y = base - 6; y <= base; y++) {
            const int row = (y - (base - 6)) / 2, bx = x + (row & 1) * 2;
            const bool joint = (y - (base - 6)) % 2 == 1 || bx % 4 == 0;
            c.set(x, y, joint ? M.stone[1] : M.stone[(hash3(bx / 4, row, 43u) % 3 == 0) ? 3 : 2]);
          }
        }
        break;
      }
      case Fence::Bamboo: {   // bamboo poles side by side, nodes ringed, two lashed cross-bars
        for (int x = 0; x < w; x++) {
          const int q = x % 3;
          if (q == 2) continue;
          const int top = base - 11 + (int)(hash3(x / 3, 1, 47u) % 2);
          for (int y = top; y <= base; y++) {
            const bool node = (y - top) % 5 == 2;
            c.set(x, y, node ? rgba(120, 130, 60) : (q == 0 ? rgba(188, 196, 104) : rgba(140, 150, 70)));
          }
          c.set(x, top, rgba(220, 222, 150));
        }
        for (int x = 0; x < w; x++) { c.set(x, base - 8, rgba(126, 96, 52)); c.set(x, base - 3, rgba(126, 96, 52)); }
        break;
      }
      case Fence::Rope: {   // one post a tile, two ropes sagging from post to post (the curve runs on into the next tile)
        for (int y = base - 11; y <= base; y++) { c.set(1, y, M.wood[3]); c.set(2, y, M.wood[1]); }
        c.set(1, base - 11, M.wood[4]); c.set(2, base - 11, M.wood[2]);
        for (int r = 0; r < 2; r++)
          for (int x = 0; x < w; x++) {
            const float t = ((x + 16 - 2) % 16) / 16.0f;
            const int y = base - 9 + r * 5 + (int)std::lround(std::sin(t * 3.14159f) * 2.0f);
            if (x == 1 || x == 2) continue;
            c.set(x, y, kCloth[(x & 1) ? 3 : 2]);
          }
        break;
      }
      case Fence::Hedge: {   // a low clipped hedge: its lit top, its leafy side
        for (int x = 0; x < w; x++) {
          for (int y = base - 9; y <= base - 6; y++) {
            const uint32_t h = hash3(x, y, 49u);
            c.set(x, y, kLeaf[y == base - 9 ? (h % 3 ? 4 : 3) : (h % 4 == 0 ? 2 : 3)]);
          }
          for (int y = base - 5; y <= base; y++) {
            const uint32_t h = hash3(x, y, 51u);
            c.set(x, y, kLeaf[(h % 5 == 0) ? 1 : (y > base - 2 ? 1 : 2)]);
          }
        }
        break;
      }
      default: break;
    }
  } else {
    const int cx = w / 2;
    switch (f) {
      case Fence::Wattle:
        for (int y = 0; y <= base; y++) {
          for (int x = cx - 2; x <= cx + 1; x++) c.set(x, y, M.wood[((y / 2 + x) & 1) ? 3 : 1]);
          if (y % 4 == 1) { c.set(cx - 2, y, M.wood[4]); c.set(cx + 1, y, M.wood[0]); }
        }
        break;
      case Fence::StoneDyke:
        for (int y = 0; y <= base; y++)
          for (int x = cx - 3; x <= cx + 2; x++) {
            const bool joint = (y % 3) == 0 || ((x + (y / 3) * 2) % 4) == 0;
            c.set(x, y, M.stone[x == cx - 3 ? 4 : (x == cx + 2 ? 1 : (joint ? 2 : 3))]);
          }
        for (int y = base - 5; y <= base; y++) for (int x = cx - 3; x <= cx + 2; x++) c.set(x, y, M.stone[(x == cx - 3) ? 3 : ((y % 2) ? 1 : 2)]);
        break;
      case Fence::Bamboo:
        // (M7 integration) the canes seen end on along the run: a round band as thick as the wattle's, lit from the
        // left, cut tops every 3 px, a node every 5, lashing ties every 8, the east side in shade (was a 2 px line)
        for (int y = 0; y <= base; y++) {
          const bool node = y % 5 == 2, tie = y % 8 == 4, cut = y % 3 == 0;
          c.set(cx - 2, y, tie ? rgba(150, 116, 66) : node ? rgba(150, 160, 80) : rgba(176, 186, 98));
          c.set(cx - 1, y, tie ? rgba(126, 96, 52) : node ? rgba(130, 140, 66) : cut ? rgba(224, 226, 154) : rgba(196, 204, 110));
          c.set(cx, y, tie ? rgba(110, 82, 44) : node ? rgba(106, 114, 52) : rgba(146, 156, 74));
          c.set(cx + 1, y, tie ? rgba(92, 68, 38) : rgba(104, 112, 50));
          c.set(cx + 2, y, rgba(70, 76, 34));
        }
        break;
      case Fence::Rope:
        for (int y = base - 11; y <= base; y++) { c.set(cx - 1, y, M.wood[3]); c.set(cx, y, M.wood[1]); }
        for (int y = 0; y < base - 11; y++) c.set(cx, y, kCloth[(y & 1) ? 3 : 2]);
        break;
      case Fence::Hedge:
        for (int y = 0; y <= base; y++)
          for (int x = cx - 3; x <= cx + 2; x++) {
            const uint32_t h = hash3(x, y, 53u);
            c.set(x, y, kLeaf[x == cx - 3 ? 4 : (x == cx + 2 ? 2 : (h % 4 == 0 ? 2 : 3))]);
          }
        for (int y = base - 5; y <= base; y++) for (int x = cx - 3; x <= cx + 2; x++) c.set(x, y, kLeaf[(y > base - 2) ? 1 : 2]);
        break;
      default: break;
    }
  }
}

// ---------------------------------------------------------------- wells
void wellStyled(Canvas& c, int kind, const Mat& M) {
  const float cx = c.w * 0.5f;
  const int base = c.h - 2;
  auto ring = [&](float rx, float ry, float hgt, const Ramp& S, bool tiles) {
    ellipse(c, cx, base - hgt, rx, ry, S[3]);
    ellipse(c, cx, base - hgt, rx - 3, ry - 1.5f, kWater[0]);
    ellipse(c, cx + 1, base - hgt + 0.5f, rx - 5.5f, std::max(0.8f, ry - 3.0f), kWater[1]);
    c.set((int)cx - 2, (int)(base - hgt), kWater[3]);
    for (int y = (int)(base - hgt); y <= base; y++)
      for (int x = (int)(cx - rx); x <= (int)(cx + rx); x++) {
        const float dx = (x + 0.5f - cx) / rx;
        const float yy = base - hgt + std::sqrt(std::max(0.0f, 1 - dx * dx)) * ry;
        if (y < yy) continue;
        int k = lightIndex(lightAt(dx * 0.85f, 0.1f), x, y, 0);
        const int row = (int)(y - yy) / 3;
        if (((x + row * 3) % 5) == 0 || (int)(y - yy) % 3 == 0) k = std::max(0, k - 1);
        uint32_t col = S[k];
        if (tiles && (int)(y - yy) >= 1 && (int)(y - yy) <= 4) {   // a band of glazed tiles
          const bool star = ((x + (int)(y - yy)) % 4) == 0;
          col = star ? (M.cloth[3]) : mix(kJadeTileP[std::clamp(k, 1, 4)], S[k], 0.15f);
        }
        c.set(x, y, col);
      }
  };
  switch (kind) {
    case 1: {   // an open stone ring, a bucket standing on its rim, the rope coiled
      ring(11.5f, 4.5f, 8, M.stone, false);
      box(c, (int)cx + 6, base - 14, (int)cx + 9, base - 11, M.wood[2]);
      hline(c, (int)cx + 6, (int)cx + 9, base - 14, M.metal[3]);
      c.set((int)cx + 9, base - 12, M.wood[0]);
      ellipse(c, cx - 7, base - 12.5f, 2.5f, 1.2f, kCloth[2]);
      break;
    }
    case 2: {   // a shadoof: a low ring and a sweep pole on a post, a bucket on its long arm, a stone on its short
      ring(9.5f, 3.6f, 6, M.stone, false);
      const int px = 3;
      for (int y = 10; y <= base - 2; y++) { c.set(px, y, M.wood[3]); c.set(px + 1, y, M.wood[1]); }
      thickLine(c, 0.5f, 18.0f, 26.5f, 4.0f, 1.6f, M.wood[2]);
      line(c, 1, 18, 26, 4, M.wood[3]);
      ball(c, 2.0f, 18.5f, 2.6f, 2.2f, M.stone);
      vline(c, 25, 5, base - 13, kCloth[1]);
      box(c, 24, base - 14, 27, base - 11, M.wood[2]);
      hline(c, 24, 27, base - 14, M.metal[3]);
      break;
    }
    case 3: {   // a carved spring basin: a stone trough under a carved wall slab, water pouring from a spout
      box34(c, (int)cx - 11, base - 18, 22, 4, 14, M.stone);   // the back slab
      for (int y = base - 16; y < base - 7; y++) for (int x = (int)cx - 8; x <= (int)cx + 7; x++) if ((x + y) % 5 == 0) c.set(x, y, M.stone[1]);   // carving
      ball(c, cx, base - 11.0f, 2.2f, 2.0f, M.metal);   // the spout's head
      box34(c, (int)cx - 13, base - 8, 26, 4, 6, M.stone);   // the basin
      for (int x = (int)cx - 11; x <= (int)cx + 10; x++) for (int y = base - 7; y <= base - 6; y++) c.set(x, y, kWater[(x & 2) ? 2 : 1]);
      for (int y = base - 10; y <= base - 7; y++) c.set((int)cx, y, kWater[4]);
      break;
    }
    case 4: {   // a tiled cistern under a small dome on four posts
      ring(12.0f, 4.5f, 8, M.stone, true);
      for (int s = 0; s < 2; s++) {
        const int x = s ? (int)cx + 10 : (int)cx - 11;
        for (int y = 8; y <= base - 10; y++) { c.set(x, y, M.stone[s ? 2 : 4]); c.set(x + 1, y, M.stone[s ? 1 : 3]); }
      }
      for (int y = 0; y < 9; y++)
        for (int x = -12; x <= 12; x++) {
          const float dx = x / 12.5f, dy = (y - 8) / 8.0f;
          if (dx * dx + dy * dy > 1) continue;
          c.set((int)cx + x, y + 1, kJadeTileP[lightIndex(lightAt(dx * 0.8f, dy * 0.6f), x, y, 0)]);
        }
      hline(c, (int)cx - 12, (int)cx + 12, 9, M.stone[1]);
      c.set((int)cx, 0, kGold[4]);
      break;
    }
    default: break;
  }
}

// ---------------------------------------------------------------- lamps
void lampStyled(Canvas& c, int kind, const Mat& M) {
  const int cx = c.w / 2, base = c.h - 2;
  switch (kind) {
    case 1: {   // a paper lantern on a pole: the lantern hung from the arm
      const Ramp L = ramp(mix(M.cloth[2], rgba(244, 196, 120), 0.55f));   // paper glowing warm over the culture's dye
      for (int y = 4; y <= base; y++) { c.set(cx - 2, y, M.wood[3]); c.set(cx - 1, y, M.wood[1]); }
      hline(c, cx - 2, cx + 3, 4, M.wood[2]);
      c.set(cx + 3, 5, M.wood[1]);
      for (int j = 0; j < 7; j++)
        for (int i = -2; i <= 2; i++) {
          if ((j == 0 || j == 6) && std::abs(i) == 2) continue;
          const bool cap = j == 0 || j == 6;
          c.set(cx + 3 + i, 6 + j, cap ? kWoodDark[1] : (j == 3 ? L[4] : L[i < 0 ? 4 : (i > 0 ? 2 : 3)]));
        }
      c.set(cx + 3, 13, L[1]);
      break;
    }
    case 2: {   // a fire bowl on a tall iron stand
      for (int y = 10; y <= base; y++) c.set(cx, y, M.metal[2]);
      line(c, cx, base - 4, cx - 3, base, M.metal[2]);
      line(c, cx, base - 4, cx + 3, base, M.metal[1]);
      for (int y = 0; y < 3; y++) for (int x = cx - 4 + y; x <= cx + 3 - y; x++) c.set(x, 7 + y, M.metal[x < cx ? 3 : 1]);
      hline(c, cx - 4, cx + 3, 7, M.metal[4]);
      smallFlame(c, cx - 0.5f, 7, 7, 1);
      break;
    }
    case 3: {   // a stone lantern: foot, shaft, a light box with a glowing window, a hat and its knob
      box34(c, cx - 4, base - 3, 8, 1, 3, M.stone);
      for (int y = base - 12; y < base - 3; y++) { c.set(cx - 2, y, M.stone[3]); c.set(cx - 1, y, M.stone[2]); c.set(cx, y, M.stone[2]); c.set(cx + 1, y, M.stone[1]); }
      box34(c, cx - 4, base - 15, 8, 1, 2, M.stone);
      box34(c, cx - 3, base - 21, 6, 0, 6, M.stone);
      for (int y = base - 20; y < base - 16; y++) for (int x = cx - 1; x <= cx; x++) c.set(x, y, kGlow[y == base - 20 ? 4 : 3]);
      for (int y = 0; y < 4; y++) for (int x = cx - 5 + y; x <= cx + 4 - y; x++) c.set(x, base - 22 - y, M.stone[y == 0 ? 2 : (x < cx ? 4 : 3)]);
      c.set(cx, base - 26, M.stone[4]); c.set(cx - 1, base - 26, M.stone[3]);
      break;
    }
    case 4: {   // an elven glow-orb: a slender silver stem curling up, the orb of soft light in its crook
      for (int y = 8; y <= base; y++) {
        const int x = cx + (int)std::lround(std::sin(y * 0.18f) * 1.2f);
        c.set(x, y, rgba(214, 220, 228)); c.set(x + 1, y, rgba(140, 150, 170));
      }
      for (int k = 0; k < 6; k++) c.set(cx + 1 + (k > 2 ? 1 : 0), 8 - k, rgba(214, 220, 228));
      ball(c, cx - 1.0f, 6.5f, 3.0f, 3.0f, kGlowElfP);
      c.set(cx - 2, 5, kWhite);
      break;
    }
    case 5: {   // a torch on a post
      for (int y = 8; y <= base; y++) { c.set(cx - 1, y, M.wood[3]); c.set(cx, y, M.wood[1]); }
      hline(c, cx - 2, cx + 1, 8, M.metal[2]);
      for (int y = 5; y < 8; y++) { c.set(cx - 1, y, kWoodDark[2]); c.set(cx, y, kWoodDark[1]); }
      smallFlame(c, cx - 0.5f, 5, 6, 2);
      break;
    }
    default: break;
  }
}

// ---------------------------------------------------------------- benches
void benchStyled(Canvas& c, int kind, const Mat& M) {
  const int w = c.w, base = c.h - 1;
  switch (kind) {
    case 1:   // a stone slab on two blocks
      box34(c, 1, base - 7, w - 2, 3, 2, M.stone);
      for (int s = 0; s < 2; s++) box34(c, s ? w - 5 : 2, base - 2, 3, 0, 3, M.stone);
      break;
    case 2: {   // (fixer M4 r3, review: "an unidentified purple-and-white crate in the old kingdom's purple") a majlis
                // divan that reads as a seat: a dark carved frame with arms at both ends and a raised back, a striped
                // cushion lit from the top-left in the dune's own madder and teal (never a kingdom's colours), carved
                // slots on its shaded front, and its shadow cast down-right on the ground
      const Ramp F = kWoodDark;
      const Ramp T = ramp(rgba(52, 132, 138));     // dune teal (the towns' trim)
      const Ramp R = ramp(rgba(176, 82, 52));      // madder
      const int top = base - 10;                   // the back's top row
      // (its cast shadow is laid down-right after the outline: divanShadow)
      // the back: a carved rail with a bolster before it
      for (int x = 1; x < w - 2; x++) { c.set(x, top, F[x < 4 ? 4 : 3]); c.set(x, top + 1, F[2]); }
      for (int x = 2; x < w - 3; x++) c.set(x, top + 2, x < 5 ? R[4] : R[3]);
      for (int x = 2; x < w - 3; x++) c.set(x, top + 3, R[1]);
      // the seat cushion: stripes running front to back, lit top rows
      for (int y = top + 4; y <= top + 6; y++)
        for (int x = 2; x < w - 3; x++) {
          const int band = (x / 2) % 3;
          const Ramp& B = band == 0 ? M.cloth : (band == 1 ? T : M.cloth);
          int k = y == top + 4 ? 4 : 3;
          if (x == w - 4) k = 2;
          c.set(x, y, band == 1 ? T[k - 1] : B[k]);
        }
      // the arms: blocky ends rising above the seat, lit on the left face
      for (int y = top + 1; y <= base - 2; y++) {
        c.set(0, y, F[y == top + 1 ? 4 : 3]); c.set(1, y, F[y == top + 1 ? 4 : 2]);
        c.set(w - 3, y, F[y == top + 1 ? 3 : 2]); c.set(w - 2, y, F[1]);
      }
      // the front of the frame: shaded, carved with slots, a gold tassel at each arm
      for (int x = 2; x < w - 3; x++) {
        c.set(x, top + 7, F[2]);
        c.set(x, base - 2, F[1]);
      }
      for (int x = 3; x < w - 4; x += 3) c.set(x, top + 7, F[0]);
      c.set(1, top + 4, kGold[4]); c.set(w - 3, top + 4, kGold[2]);
      break;
    }
    case 3: {   // a half log on two stubs
      for (int y = base - 6; y <= base - 3; y++)
        for (int x = 1; x < w - 1; x++) c.set(x, y, y == base - 6 ? M.wood[4] : (y == base - 5 ? M.wood[3] : kBark[(x % 4) ? 2 : 1]));
      ellipse(c, 1.5f, base - 4.5f, 1.4f, 1.8f, M.wood[3]);
      for (int s = 0; s < 2; s++) box(c, s ? w - 5 : 3, base - 2, s ? w - 4 : 4, base, kBark[s ? 1 : 3]);
      break;
    }
    default: break;
  }
}

// the divan's cast shadow (down-right, translucent), laid on the empty pixels after the outline so it gets none
void divanShadow(Canvas& c) {
  const int w = c.w, base = c.h - 1;
  auto sh = [&](int x, int y, int a) { if (x >= 0 && x < w && y >= 0 && y < c.h && !chA(c.get(x, y))) c.set(x, y, rgba(20, 14, 30, a)); };
  for (int x = 2; x < w; x++) { sh(x, base - 1, 100); sh(x + 1, base, 56); }
  for (int y = base - 7; y < base - 1; y++) sh(w - 1, y, 80);
}

// ---------------------------------------------------------------- statues, obelisks, totems
void statueStyled(Canvas& c, const PropStyle& st, const Mat& M) {
  const int cx = c.w / 2, base = c.h - 2;
  const int cul = st.culture;
  const bool obelisk = st.centre == 5 || cul == CU_SUN || cul == CU_DUNE || cul == CU_STAR;
  const bool totem = !obelisk && (cul == CU_FJORD || cul == CU_STEPPE || cul == CU_MARSH || cul == CU_SYLVAN);
  if (obelisk) {   // a stepped plinth, a tapering shaft carved with the culture's glyph, a gilded pyramidion
    box34(c, cx - 8, base - 5, 16, 2, 4, M.stone);
    box34(c, cx - 6, base - 9, 12, 2, 3, M.stone);
    for (int y = 3; y < base - 8; y++) {
      const float t = (y - 3) / (float)(base - 11);
      const int half = (int)std::lround(2.5f + t * 2.0f);
      for (int x = cx - half; x < cx + half; x++) c.set(x, y, M.stone[x == cx - half ? 4 : (x >= cx ? (x == cx + half - 1 ? 1 : 2) : 3)]);
    }
    for (int y = 0; y < 4; y++) for (int x = cx - 3 + y; x < cx + 3 - y; x++) c.set(x, y + 0, kGold[x < cx ? 4 : 2]);
    Canvas g = glyphSprite(0x0BE115u + (uint32_t)cul * 977u, cul % 4, M.stone[1], 1);
    blit(c, g, cx - 4, 10);
    return;
  }
  if (totem) {   // a carved pole: stacked faces in the culture's colours, wings spread at the top
    const Ramp& W = M.wood;
    for (int y = 6; y <= base; y++)
      for (int x = cx - 3; x <= cx + 2; x++) c.set(x, y, W[x == cx - 3 ? 4 : (x == cx + 2 ? 1 : (x < cx ? 3 : 2))]);
    for (int k = 0; k < 3; k++) {
      const int fy = 9 + k * 8;
      hline(c, cx - 3, cx + 2, fy + 6, W[0]);
      c.set(cx - 2, fy + 2, kInk); c.set(cx + 1, fy + 2, kInk);       // eyes
      c.set(cx - 2, fy + 1, M.cloth[3]); c.set(cx + 1, fy + 1, M.cloth[3]);
      hline(c, cx - 1, cx, fy + 4, M.cloth[k == 1 ? 2 : 1]);            // a painted mouth
    }
    for (int i = 0; i < 6; i++) { c.set(cx - 4 - i, 6 + i / 2, W[3]); c.set(cx + 3 + i, 6 + i / 2, W[1]); c.set(cx - 4 - i, 7 + i / 2, M.cloth[2]); c.set(cx + 3 + i, 7 + i / 2, M.cloth[1]); }
    for (int x = cx - 2; x <= cx + 1; x++) c.set(x, 5, W[4]);
    return;
  }
  // a figure on a plinth, in the culture's stone (or bronze)
  box34(c, cx - 8, base - 9, 16, 3, 7, M.stone);
  HumanLook l;
  l.outfit = Outfit::Plate; l.helmet = cul != CU_JADE && cul != CU_IMPERIAL; l.cape = true; l.weapon = 1;
  Canvas fig = humanFigureStill(l, 0);
  const bool bronze = cul == CU_RIVER || cul == CU_JADE;
  const Ramp S = bronze ? (cul == CU_JADE ? ramp(rgba(70, 150, 120)) : kBrass) : M.stone;
  for (auto& p : fig.px)
    if (chA(p)) { float lu = luma(p); p = S[std::clamp((int)(lu * 5.2f), 0, 4)]; }
  blit(c, fig, cx - 8, base - 9 - HUMAN_H + 1);
}

// ---------------------------------------------------------------- shrines
void shrineStyled(Canvas& c, int cul, const Mat& M) {
  const int cx = c.w / 2, base = c.h - 2;
  switch (cul) {
    case CU_JADE: {   // a little pagoda shrine: a stone base, red posts, a glazed roof with upswept corners
      box34(c, cx - 9, base - 5, 18, 2, 4, M.stone);
      for (int s = 0; s < 2; s++) { const int x = s ? cx + 6 : cx - 7; for (int y = 12; y < base - 5; y++) { c.set(x, y, kRed[s ? 1 : 3]); c.set(x + 1, y, kRed[s ? 0 : 2]); } }
      for (int y = 12; y < base - 5; y++) for (int x = cx - 5; x <= cx + 5; x++) c.set(x, y, rgba(46, 30, 40));
      ball(c, cx - 0.5f, base - 9.5f, 2.0f, 2.4f, kGold);   // the image within
      for (int y = 0; y < 9; y++) {
        const int half = 5 + y + (y > 6 ? 2 : 0);
        for (int x = cx - half; x <= cx + half - 1; x++) c.set(x, 3 + y, kJadeTileP[y < 2 ? 4 : (x < cx ? 3 : 2)]);
      }
      c.set(cx - 13, 9, kJadeTileP[4]); c.set(cx + 12, 9, kJadeTileP[2]); c.set(cx - 14, 8, kJadeTileP[4]); c.set(cx + 13, 8, kJadeTileP[2]);
      hline(c, cx - 12, cx + 11, 12, kJadeTileP[0]);
      c.set(cx, 1, kGold[4]); c.set(cx, 2, kGold[2]);
      break;
    }
    case CU_HIGHLAND: case CU_STEPPE: {   // a cairn of piled stones, a pole on top with ribbons / a horse-tail standard
      for (int k = 0; k < 9; k++) {
        const float sx = cx - 7 + (k % 4) * 4.5f + (k / 4) * 2.0f, sy = base - 2 - (k / 4) * 4.0f;
        if (k == 8) rock(c, (float)cx, base - 13.0f, 3.5f, 3.0f, M.stone, 91u + (uint32_t)k, 4);
        else rock(c, sx, sy, 3.2f, 2.8f, M.stone, 91u + (uint32_t)k, 4);
      }
      for (int y = 2; y < base - 14; y++) c.set(cx, y, M.wood[2]);
      if (cul == CU_STEPPE) {
        for (int y = 3; y < 12; y++)
          for (int x = cx - 2 + (y > 7); x <= cx + 2 - (y > 7); x++)
            if (x != cx) c.set(x, y, (y % 2) ? rgba(40, 34, 40) : rgba(230, 226, 214));
      } else for (int k = 0; k < 4; k++) { c.set(cx + 1 + k, 4 + k, M.cloth[3]); c.set(cx - 1 - k, 6 + k, kCloth[3]); }
      break;
    }
    case CU_SYLVAN: {   // a mossy stump grown into an arch, a glow-orb resting in its hollow
      for (int y = 6; y <= base; y++) {
        const int half = 9 - (y < 12 ? (12 - y) : 0) / 2;
        for (int x = cx - half; x <= cx + half - 1; x++) {
          const bool hollow = y > 13 && y < base - 1 && std::abs(x - cx) < 4;
          c.set(x, y, hollow ? rgba(30, 30, 38) : kBark[x < cx - half + 2 ? 4 : (x > cx + half - 3 ? 1 : ((x + y) % 5 == 0 ? 2 : 3))]);
        }
      }
      for (int x = cx - 9; x <= cx + 8; x++) if (hash3(x, 1, 7u) % 3) c.set(x, 6, kMoss[3]);
      ball(c, cx - 0.5f, base - 6.0f, 2.6f, 2.6f, kGlowElfP);
      break;
    }
    case CU_SUN: case CU_DUNE: {   // a stepped fire altar, flames on its top
      box34(c, cx - 10, base - 6, 20, 2, 5, M.stone);
      box34(c, cx - 7, base - 11, 14, 2, 4, M.stone);
      box34(c, cx - 4, base - 15, 8, 2, 3, M.stone);
      for (int x = cx - 7; x < cx + 7; x++) if (x % 3 == 0) c.set(x, base - 8, M.cloth[3]);
      smallFlame(c, cx - 0.5f, base - 14, 9, 0);
      break;
    }
    case CU_MARSH: {   // a spirit house on a post: a little thatched house with offerings
      for (int y = 16; y <= base; y++) { c.set(cx - 1, y, M.wood[3]); c.set(cx, y, M.wood[1]); }
      box34(c, cx - 7, 13, 14, 2, 1, M.wood);
      for (int y = 8; y < 13; y++) for (int x = cx - 5; x <= cx + 4; x++) c.set(x, y, (std::abs(x - cx) < 2 && y > 9) ? rgba(30, 24, 34) : M.wood[x == cx - 5 ? 3 : 2]);
      for (int y = 1; y < 9; y++) { const int half = 2 + y; for (int x = cx - half; x < cx + half; x++) c.set(x, y, kThatch[y < 3 ? 4 : (x < cx ? 3 : 1)]); }
      c.set(cx - 4, 12, kGlow[4]); c.set(cx + 3, 12, rgba(220, 70, 80));
      break;
    }
    case CU_FJORD: {   // a rune stone: a tall slab, the carved runes painted red
      for (int y = 3; y <= base; y++) {
        const int half = 6 - (y < 7 ? (7 - y) : 0);
        for (int x = cx - half; x < cx + half; x++) c.set(x, y, M.stone[x == cx - half ? 4 : (x == cx + half - 1 ? 1 : ((x + y * 3) % 7 == 0 ? 2 : 3))]);
      }
      Canvas g = glyphSprite(0xF0D5u, 0, rgba(190, 50, 40), 1);
      blit(c, g, cx - 4, 8);
      Canvas g2 = glyphSprite(0xF0D7u, 0, rgba(190, 50, 40), 1);
      blit(c, g2, cx - 4, 16);
      break;
    }
    default: {   // a pale marble aedicule: two columns, a pediment, a gilded image (imperial, river, high elves)
      box34(c, cx - 10, base - 6, 20, 2, 4, M.stone);
      for (int px2 : {cx - 9, cx + 6}) cylinder(c, px2, 11, px2 + 2, base - 7, M.stone);
      for (int y = 3; y <= 10; y++) {
        const int half = 3 + (y - 3) * 1;
        for (int x = cx - half; x <= cx + half - 1; x++) c.set(x, y, M.stone[y < 5 ? 4 : (y < 9 ? 3 : 1)]);
      }
      hline(c, cx - 12, cx + 11, 11, M.stone[0]);
      ball(c, cx - 0.5f, base - 12.0f, 2.5f, 3.0f, cul == CU_STAR ? kGlowElfP : kGold);
      for (int i = 0; i < 3; i++) c.set(cx - 5 + i * 4, base - 7, kGlow[4]);
      break;
    }
  }
}

// ---------------------------------------------------------------- fire bowls
void brazierStyled(Canvas& c, int kind, int frame, const Mat& M) {
  const int cx = c.w / 2, base = c.h - 1;
  static const float hs[4] = {8.0f, 9.5f, 7.5f, 9.0f};
  switch (kind) {
    case 1: {   // a stone bowl on a pedestal
      box34(c, cx - 4, base - 4, 8, 1, 3, M.stone);
      for (int y = base - 9; y < base - 4; y++) { c.set(cx - 2, y, M.stone[3]); c.set(cx - 1, y, M.stone[2]); c.set(cx, y, M.stone[2]); c.set(cx + 1, y, M.stone[1]); }
      for (int y = 0; y < 3; y++) for (int x = cx - 6 + y; x < cx + 6 - y; x++) c.set(x, base - 12 + y, M.stone[x < cx - 2 ? 4 : (x < cx + 2 ? 3 : 1)]);
      hline(c, cx - 6, cx + 5, base - 12, M.stone[4]);
      for (int x = cx - 5; x < cx + 5; x++) c.set(x, base - 13, kFire[1 + ((x + frame) & 1)]);
      smallFlame(c, cx - 0.5f, base - 13.0f, hs[frame & 3], frame);
      break;
    }
    case 2: {   // a ring of stones round a fire on the ground
      for (int k = 0; k < 7; k++) {
        const float a = k / 7.0f * 6.2832f;
        rock(c, cx + std::cos(a) * 6.0f, base - 3 + std::sin(a) * 2.2f, 2.2f, 1.8f, M.stone, 31u + (uint32_t)k, 3);
      }
      for (int x = cx - 3; x <= cx + 2; x++) c.set(x, base - 3, kWoodDark[(x & 1) ? 1 : 2]);
      smallFlame(c, cx - 0.5f, base - 3.0f, hs[frame & 3] + 1, frame);
      break;
    }
    case 3: {   // a bronze cauldron-bowl on three legs
      for (int s = -1; s <= 1; s++) line(c, cx + s * 2, base - 6, cx + s * 5, base, kBrass[s < 0 ? 3 : (s > 0 ? 1 : 2)]);
      for (int y = 0; y < 5; y++) for (int x = cx - 6 + y / 2; x < cx + 6 - y / 2; x++) c.set(x, base - 11 + y, kBrass[x < cx - 2 ? 4 : (x < cx + 2 ? 3 : 1)]);
      hline(c, cx - 6, cx + 5, base - 11, kBrass[4]);
      for (int x = cx - 5; x < cx + 5; x++) c.set(x, base - 12, kFire[1 + ((x + frame) & 1)]);
      smallFlame(c, cx - 0.5f, base - 12.0f, hs[frame & 3], frame);
      break;
    }
    default: break;
  }
}

// ---- (M7 fix) the joined yard walls and fences (dykePiece, hedgePiece, fenceJoinPiece): the footprint of a piece
// across its run (ground px a0..a1), its arms to the neighbours it joins (beyond the tile edges) and, toward a gateway,
// to its own tile edge less `gateShort` px (where the gatepost stands)
bool fenceIn(int gx, int gy, int a0, int a1, int mask, int gates, int gateShort = 0) {
  const bool cx = gx >= a0 && gx <= a1, cy = gy >= a0 && gy <= a1;
  if (cx && cy) return true;
  if (cx && gy < a0) return ((mask & 1) && gy >= -16) || ((gates & 1) && gy >= gateShort);
  if (cx && gy > a1) return ((mask & 4) && gy <= 31) || ((gates & 4) && gy <= 15 - gateShort);
  if (cy && gx < a0) return ((mask & 8) && gx >= -16) || ((gates & 8) && gx >= gateShort);
  if (cy && gx > a1) return ((mask & 2) && gx <= 31) || ((gates & 2) && gx <= 15 - gateShort);
  return false;
}
// a post (a stone pier with `stone`, else a squared timber post with a cap) at each end of the run named by gates (at
// the tile edge: the gateway's jamb) and ends (the run stops here: at the footprint's end). hp px tall, its ground rect
// grown `grow` px across the run beyond a0..a1. Its top lit from the north-west, its south face toward the camera.
void fencePosts(Canvas& c, int a0, int a1, int gates, int ends, int OY, int hp, int grow, const Ramp& R, bool stone) {
  for (int d = 0; d < 4; d++) {
    const int bit = 1 << d;   // 1 north, 2 east, 4 south, 8 west
    if (!((gates | ends) & bit)) continue;
    const bool edge = (gates & bit) != 0;
    const int len = stone ? 4 : 3;
    const int c0 = a0 - 1 - grow / 3, c1 = a1 + 1 + grow / 3;
    int x0, x1, y0, y1;
    if (bit == 1) { x0 = c0; x1 = c1; y0 = edge ? 0 : a0; y1 = y0 + len - 1; }
    else if (bit == 4) { x0 = c0; x1 = c1; y1 = edge ? 15 : a1; y0 = y1 - len + 1; }
    else if (bit == 8) { y0 = c0; y1 = c1; x0 = edge ? 0 : a0; x1 = x0 + len - 1; }
    else { y0 = c0; y1 = c1; x1 = edge ? 15 : a1; x0 = x1 - len + 1; }
    if (!stone && (bit == 1 || bit == 4)) { x0 = (a0 + a1) / 2 - 1; x1 = x0 + 2; }
    for (int gy = y0; gy <= y1; gy++)
      for (int gx = x0; gx <= x1; gx++) {
        if (gx < 0 || gx >= c.w) continue;
        const int ty = gy + OY - hp;
        int k = gx == x0 ? 4 : (gx == x1 ? 2 : 3);
        if (gy == y0 && gx != x1) k = 4;
        if (!stone && gy > y0 && gy < y1 && gx > x0 && gx < x1) k = 2;   // the post's cut top, darker at its heart
        if (ty >= 0 && ty < c.h) c.set(gx, ty, R[k]);
        if (gy != y1) continue;
        for (int h = hp - 1; h >= 0; h--) {   // the south face
          const int y = gy + OY - h;
          if (y < 0 || y >= c.h) continue;
          int kk = gx == x0 ? 3 : (gx == x1 ? 1 : 2);
          if (h == hp - 1) kk = 3;                                     // the cap's lip
          if (stone && (hp - 1 - h) % 3 == 0 && h < hp - 1) kk = 1;   // the pier's courses
          if (stone && h == hp - 2) kk = 4;                           // the cap stone's lit edge
          if (!stone && h == hp - 3) kk = 0;                          // the groove under the post's cap
          if (h == 0) kk = std::min(kk, 1);
          c.set(gx, y, R[std::clamp(kk, 0, 4)]);
        }
      }
  }
}
// the cast shadow on the ground to the south-east (the light from the top left), `reach` px, where nothing is drawn
void fenceShadow(Canvas& c, int a0, int a1, int mask, int gates, int OY, int reach, int alpha) {
  for (int gy = 0; gy < 16; gy++)
    for (int gx = 0; gx < 16; gx++) {
      if (fenceIn(gx, gy, a0, a1, mask, gates) || chA(c.get(gx, gy + OY)) != 0) continue;
      bool sh = false;
      for (int r = 1; r <= reach && !sh; r++)
        sh = fenceIn(gx - r, gy, a0, a1, mask, gates) || fenceIn(gx, gy - r, a0, a1, mask, gates) || fenceIn(gx - r, gy - r, a0, a1, mask, gates) ||
             (r > 1 && fenceIn(gx - r + 1, gy - r, a0, a1, mask, gates));
      if (sh) c.set(gx, gy + OY, rgba(16, 22, 12, alpha));
    }
}

}  // namespace

Canvas dykePiece(int bits, const PropStyle& st) {
  const Mat M = matOf(st);
  const Ramp& S = M.stone;
  const int mask = bits & 15, gates = (bits >> 4) & 15, ends = (bits >> 8) & 15;
  const int W = 16, H = 28, OY = 12, HT = 7;  // canvas y = ground y + OY; the wall stands HT px tall (room above for the piers)
  const int a0 = 5, a1 = 10;                  // the wall's width across its run (ground px a0..a1)
  auto in = [&](int gx, int gy) -> bool {     // the footprint, with the neighbours' arms beyond the tile edges
    return fenceIn(gx, gy, a0, a1, mask, gates);
  };
  Canvas c(W, H);
  // ground rows from back to front: the coping (top) of each footprint pixel, then its south face where it ends
  for (int gy = -HT; gy < 16; gy++)
    for (int gx = 0; gx < W; gx++) {
      if (!in(gx, gy)) continue;
      // the coping: flat stones set on edge across the wall, a joint every 3 px along the run, lit on the west rim
      const bool runNS = !(gy >= a0 && gy <= a1) || ((mask & 5) && !(mask & 10));
      const int along = runNS ? gy : gx;
      // copes 2-4 px long at irregular joints (a hash of the position along the run: seamless from tile to tile),
      // each lit on its near (north/west) end and shaded at its far end, some a shade darker than the rest
      auto jointAt = [&](int a) { return a % 16 == 0 || hash3(((a % 16) + 16) % 16, 3, 151u) % 3 == 0; };
      int k = (hash3(((along % 16) + 16) % 16 / 2, 5, 141u) % 4 == 0) ? 2 : 3;
      if (jointAt(along)) k = 1;                                          // the joints between copes
      else if (jointAt(along - 1)) k = 4;                                 // the lit end of a cope
      else if (jointAt(along + 1)) k = std::max(1, k - 1);
      if (!in(gx - 1, gy)) k = std::max(k, 3) + (k == 1 ? 0 : 1);         // the lit west rim
      else if (!in(gx + 1, gy)) k = std::min(k, 2);                       // the shaded east rim
      k = std::clamp(k, 0, 4);
      const int ty = gy + OY - HT;
      if (ty >= 0 && ty < H) c.set(gx, ty, S[k]);
      if (!in(gx, gy + 1)) {   // the face toward the camera: coursed stones, darker toward the foot
        for (int h = HT - 1; h >= 0; h--) {
          const int y = gy + OY - h;
          if (y < 0 || y >= H || gy - h + HT < 0) continue;
          const int row = (HT - 1 - h) / 2;                              // courses 2 px tall from the top
          const int bx = gx + (row & 1) * 2;
          const bool joint = ((HT - 1 - h) % 2 == 1) || ((bx + 32) % 4 == 0);
          int kk = joint ? 1 : ((hash3((bx + 32) / 4, row, 143u) % 3 == 0) ? 3 : 2);
          if (h == HT - 1) kk = 3;                                        // the coping's lip catches the light
          if (h == 0) kk = std::min(kk, 1);                               // the foot, in the grass
          if (!in(gx - 1, gy) && h < HT - 1) kk = std::min(4, kk + 1);    // the wall's lit west end
          c.set(gx, y, S[std::clamp(kk, 0, 4)]);
        }
      }
    }
  // (M7 fix) a stone pier where the dyke ends or frames a gateway (not a flat cut)
  fencePosts(c, a0, a1, gates, ends, OY, HT + 3, 3, S, true);
  outline(c);
  // the cast shadow on the ground, to the south-east (light from the top left), where nothing else is drawn
  fenceShadow(c, a0, a1, mask, gates, OY, 4, 96);
  return c;
}

Canvas hedgePiece(int bits, const PropStyle& st, bool snow) {
  const Mat M = matOf(st);
  const int mask = bits & 15, gates = (bits >> 4) & 15;
  const int W = 16, H = 28, OY = 12, HT = 7;  // canvas y = ground y + OY; the hedge stands HT px tall
  const int a0 = 4, a1 = 11;                  // its width across the run (ground px a0..a1)
  // (a gateway's side stops a little short of the tile edge: the timber gatepost stands there)
  auto in = [&](int gx, int gy) -> bool { return fenceIn(gx, gy, a0, a1, mask, gates, 3); };
  // seamless leaf clumps: a hash of the position modulo the tile, so a run reads as one hedge from tile to tile
  auto leaf = [&](int gx, int gy, uint32_t s) { return hash3(((gx % 16) + 16) % 16 / 2, ((gy % 16) + 16) % 16 / 2, s) ^ hash3(gx & 15, gy & 15, s + 1u); };
  const Ramp& L = kLeaf;
  const Ramp& D = kLeafDark;
  Canvas c(W, H);
  for (int gy = -HT; gy < 16; gy++)
    for (int gx = 0; gx < W; gx++) {
      if (!in(gx, gy)) continue;
      // the top: clumps of leaves, lit toward the north-west rims, rounding down into shade at the south and east rims
      const bool rimW = !in(gx - 1, gy), rimN = !in(gx, gy - 1), rimE = !in(gx + 1, gy), rimS = !in(gx, gy + 1);
      const uint32_t h = leaf(gx, gy, 211u);
      int k = (h % 5 == 0) ? 2 : 3;
      if (h % 7 == 0) k = 4;
      if (rimW || rimN) k = std::min(4, k + 1);
      if (rimE) k = std::max(1, k - 1);
      if (rimS) k = 2;
      const int ty = gy + OY - HT;
      uint32_t top = L[std::clamp(k, 0, 4)];
      if (snow) {
        int sk = (rimW || rimN) ? 4 : 3;
        if (rimE || rimS) sk = 2;
        if (h % 6 == 0) sk = std::max(1, sk - 1);
        top = (h % 5 == 0 && !rimN && !rimW) ? D[(h >> 4) % 2 ? 2 : 3] : kSnow[sk];   // leaves through the snow
      }
      if (ty >= 0 && ty < H) c.set(gx, ty, top);
      if (rimS) {   // the face toward the camera: leaves in shade, darker toward the foot, ragged at the lip
        for (int hh = HT - 1; hh >= 0; hh--) {
          const int y = gy + OY - hh;
          if (y < 0 || y >= H || gy - hh + HT < 0) continue;
          const uint32_t q = leaf(gx, gy * 3 + hh, 223u);
          const int depth = HT - 1 - hh;
          int kk = depth <= 1 ? 2 : (depth <= 4 ? ((q % 3 == 0) ? 1 : 2) : 1);
          if (q % 9 == 0) kk = std::min(3, kk + 1);
          if (hh == 0) kk = 0;
          if (rimW && hh > 0) kk = std::min(3, kk + 1);   // the lit west end
          uint32_t col = (!snow && depth <= 1 && (q & 1)) ? L[kk] : D[std::clamp(snow ? kk : kk + 1, 0, 4)];
          if (snow) {
            const int drip = 1 + (int)(leaf(gx, 5, 227u) % 3u);   // the snow over the lip, drip by drip
            if (depth < drip) col = kSnow[depth == 0 ? 3 : 2];
          }
          c.set(gx, y, col);
        }
      }
    }
  // (M7 fix) a gateway through a hedge is hung on two timber gateposts at its ends
  (void)M;
  fencePosts(c, a0 + 2, a1 - 2, gates, 0, OY, HT + 4, 0, kWood, false);
  outline(c, 0.85f);
  fenceShadow(c, a0, a1, mask, gates, OY, 4, 104);
  return c;
}

Canvas fieldGatePiece(bool ns) {
  const int W = ns ? 24 : 16, H = 28, OY = 12;   // (a north-south gateway's leaf swings out over the next tile east)
  Canvas c(W, H);
  // the leaf in ground px: from its hinge along (dx, dy), 13 px long; rails at three heights, a stile at each end and a
  // brace from the foot of the hinge stile to the head of the swinging one
  const float hx = ns ? 7.5f : 0.5f, hy = ns ? 0.5f : 7.5f;
  const float dx = ns ? 0.86f : 0.82f, dy = ns ? 0.51f : 0.57f;   // swung wide enough that its rails read apart
  const float len = ns ? 13.5f : 14.5f;
  const int HT = 8;
  std::vector<uint8_t> drawn((size_t)W * H, 0);
  auto put = [&](float gx, float gy, int h, uint32_t col) {
    const int x = (int)std::floor(gx), y = (int)std::floor(gy) + OY - h;
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    c.set(x, y, col);
    drawn[(size_t)y * W + x] = 1;
  };
  // the shadow first (on the ground, down-right of the leaf; the light from the top left), then the wood over it
  for (float t = 0; t <= len; t += 0.5f)
    for (int h = 1; h <= HT; h += 2) {
      const float gx = hx + dx * t + h * 0.45f, gy = hy + dy * t + h * 0.2f;
      const int x = (int)std::floor(gx), y = (int)std::floor(gy) + OY;
      if (x >= 0 && x < W && y >= 0 && y < H && !chA(c.get(x, y))) c.set(x, y, rgba(16, 22, 12, 92));
    }
  const Ramp& R = kWood;
  for (int h = 0; h <= HT; h++) {   // the stiles (the hinge stile stout, a lit west face)
    put(hx + dx * 0.0f, hy, h, R[h == HT ? 4 : 3]);
    put(hx + dx * 1.0f, hy + dy * 1.0f, h, R[h == HT ? 3 : 1]);
    put(hx + dx * len, hy + dy * len, h, R[h == HT ? 4 : 2]);
  }
  // three single-pixel rails with the ground showing between them (thicker ones read as a solid board)
  const int rails[3] = {1, 4, HT - 1};
  for (int r = 0; r < 3; r++)
    for (float t = 1.0f; t < len; t += 0.34f) put(hx + dx * t, hy + dy * t, rails[r] + (r == 2 ? 1 : 0), R[r == 2 ? 4 : 3]);
  for (float t = 1.0f; t < len; t += 0.3f) {   // the brace, rising from the hinge's foot to the far stile's head
    const int h = 1 + (int)std::lround((t - 1.0f) / (len - 1.0f) * (HT - 3));
    put(hx + dx * t, hy + dy * t, h + 1, R[2]);
  }
  // the outline round the wood
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (drawn[(size_t)y * W + x]) continue;
      bool nb = false;
      for (int k = 0; k < 2 && !nb; k++) {   // a dark edge below and east of the wood (the light from the top left)
        const int qx = x - (k == 0), qy = y - (k == 1);
        nb = qx >= 0 && qy >= 0 && qx < W && qy < H && drawn[(size_t)qy * W + qx];
      }
      if (nb) c.set(x, y, rgba(40, 24, 26, 255));
    }
  return c;
}

Canvas fenceJoinPiece(int bits, const PropStyle& st) {
  const Mat M = matOf(st);
  const int mask = bits & 15, gates = (bits >> 4) & 15, ends = (bits >> 8) & 15;
  const Fence f = st.fence;
  const bool bamboo = f == Fence::Bamboo, rope = f == Fence::Rope;
  const int HT = bamboo ? 11 : (rope ? 10 : 9);   // how tall it stands
  const int W = 16, OY = HT + 5, H = 16 + OY;
  const int a0 = rope ? 7 : 6, a1 = rope ? 8 : 9;   // its thickness across the run (a palisade of canes, a woven hurdle)
  auto in = [&](int gx, int gy) -> bool { return fenceIn(gx, gy, a0, a1, mask, gates); };
  // the canes' colours: a lit cut end, the cane, its shade, the dark between two canes, the lashing
  const uint32_t bCut = rgba(226, 228, 158), bLit = rgba(194, 202, 108), bMid = rgba(150, 160, 76), bShade = rgba(110, 118, 54),
                 bGap = rgba(72, 78, 36), bTie = rgba(128, 96, 52), bTieD = rgba(92, 66, 36);
  Canvas c(W, H);
  for (int gy = -HT; gy < 16; gy++)
    for (int gx = 0; gx < W; gx++) {
      if (!in(gx, gy)) continue;
      const bool runNS = !(gy >= a0 && gy <= a1) || (((mask | gates) & 5) && !((mask | gates) & 10));
      const int along = (runNS ? gy : gx) + 32;
      const bool rimW = !in(gx - 1, gy), rimE = !in(gx + 1, gy), rimS = !in(gx, gy + 1);
      // ---- the top, seen from above
      uint32_t top = 0;
      if (bamboo) {
        // the canes' cut ends side by side along the run: a ring of light, the hollow, the dark between canes
        const int q = along % 3;
        top = q == 0 ? bCut : (q == 1 ? bLit : bGap);
        if (rimW && q != 2) top = bCut;
        if (rimE && q != 2) top = bShade;
      } else if (rope) {
        if (along % 8 == 2 || along % 8 == 3) top = M.wood[along % 8 == 2 ? 4 : 2];   // the post heads
        else top = kCloth[(along & 1) ? 3 : 4];                                         // the rope's top
      } else {   // wattle: the stake heads above the weave's top edge
        const bool stake = along % 4 == 1;
        int k = stake ? 4 : (((along / 2) & 1) ? 3 : 2);
        if (rimW) k = std::min(4, k + 1);
        if (rimE) k = std::max(1, k - 1);
        top = M.wood[k];
      }
      const int ty = gy + OY - HT;
      if (ty >= 0 && ty < H) c.set(gx, ty, top);
      // ---- the face toward the camera (where the run ends to the south: an E-W run's whole front)
      if (!rimS) continue;
      for (int h = HT - 1; h >= 0; h--) {
        const int y = gy + OY - h;
        if (y < 0 || y >= H) continue;
        const int depth = HT - 1 - h;
        uint32_t col = 0;
        if (bamboo) {
          const int q = (gx + 30) % 3;
          col = q == 0 ? bLit : (q == 1 ? bMid : bGap);
          if (depth % 5 == 3 && q != 2) col = q == 0 ? bMid : bShade;   // the nodes
          if (h == 3 || h == HT - 3) col = (gx & 1) ? bTie : bTieD;      // two lashed rails
          if (h == 0) col = bGap;
          if (rimW && q != 2 && h != 3 && h != HT - 3) col = bCut;
        } else if (rope) {
          const bool post = along % 8 == 2 || along % 8 == 3;
          if (post) col = M.wood[along % 8 == 2 ? (depth == 0 ? 4 : 3) : 1];
          else {
            const float t = (float)(((along - 3) % 8 + 8) % 8) / 8.0f;
            const int sag = (int)std::lround(std::sin(t * 3.14159f) * 2.0f);
            if (depth == 2 + sag || depth == 6 + sag) col = kCloth[depth < 5 ? 3 : 2];
          }
        } else {
          const bool stake = along % 4 == 1;
          const int band = depth / 2;
          const bool over = ((along / 4) + band) & 1;
          col = stake ? M.wood[depth == 0 ? 4 : 2] : (over ? M.wood[(depth % 2) ? 2 : 3] : M.wood[(depth % 2) ? 0 : 1]);
          if (h == 0) col = M.wood[0];
          if (rimW && !stake) col = M.wood[3];
        }
        if (col) c.set(gx, y, col);
      }
    }
  // posts where the run ends or frames a gateway
  const Ramp caneR = ramp5(bGap, bShade, bMid, bLit, bCut);   // a bamboo fence's posts are thick canes
  fencePosts(c, rope ? a0 - 1 : a0, rope ? a1 + 1 : a1, gates, ends, OY, HT + 3, 0, bamboo ? caneR : M.wood, false);
  outline(c, rope ? 0.55f : 0.85f);
  fenceShadow(c, a0, a1, mask, gates, OY, rope ? 2 : 4, rope ? 56 : 92);
  return c;
}


bool propStyled(Prop p) {
  if (builtPropStyled(p)) return true;   // (M3b) the builder's street furniture (art_parts_props.cpp)
  switch (p) {
    case Prop::Well: case Prop::Lamppost: case Prop::Bench: case Prop::FenceH: case Prop::FenceV: case Prop::Fountain:
    case Prop::Statue: case Prop::Shrine: case Prop::Brazier: case Prop::MarketStall:
      return true;
    default: return isStall(p);
  }
}

Canvas propSprite(Prop p, const PropStyle& st) {
  if (st.classic() || !propStyled(p)) return propSprite(p);
  {   // (M3b) the builder's street furniture: signs, tents, banners, graves, crosses, work yards, tables
    Canvas out;
    if (builtPropSprite(p, st, out)) return out;
  }
  const Mat M = matOf(st);
  const int cul = st.culture;
  if (isStall(p)) return marketStallStyled(stallTrade(p), 0, 0, false, StallS, st);
  // which painter, if the style differs from the classic for this prop
  int kind = 0;
  switch (p) {
    case Prop::Well: kind = st.well; break;
    case Prop::Lamppost: kind = st.lamp; break;
    case Prop::Bench: kind = st.bench; break;
    case Prop::FenceH: case Prop::FenceV: kind = st.fence == Fence::Picket ? 0 : 1; break;
    case Prop::Brazier: kind = (cul == CU_IMPERIAL || cul == CU_SUN || cul == CU_STAR || cul == CU_JADE) ? 1 : ((cul == CU_FJORD || cul == CU_HIGHLAND || cul == CU_MARSH) ? 2 : (cul == CU_STEPPE || cul == CU_DUNE ? 3 : 0)); break;
    case Prop::Statue: kind = cul == CU_HEART ? 0 : 1; break;
    case Prop::Shrine: kind = cul == CU_HEART ? 0 : 1; break;
    case Prop::Fountain: kind = (st.stone || cul == CU_DUNE || cul == CU_JADE || cul == CU_SUN) ? 1 : 0; break;
    default: kind = 0; break;
  }
  if (kind == 0) return propSprite(p);
  const int w = propW(p), h = propH(p), n = propFrames(p);
  if (p == Prop::Fountain) {
    // the classic fountain recoloured: the culture's stone, a band of its tiles round the basin's rim
    Canvas s = propSprite(p);
    const Ramp& S = M.stone;
    for (int y = 0; y < s.h; y++)
      for (int x = 0; x < s.w; x++) {
        const uint32_t q = s.get(x, y);
        if (!chA(q)) continue;
        // stone pixels: low saturation; water stays water
        const int r = chR(q), g = chG(q), b = chB(q);
        const int mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b));
        if (mx - mn < 40 && b <= r + 30) {
          const float lu = luma(q);
          s.set(x, y, S[std::clamp((int)(lu * 5.4f), 0, 4)]);
        }
      }
    if (cul == CU_DUNE || cul == CU_JADE || cul == CU_SUN)
      for (int f = 0; f < n; f++)
        for (int x = 2; x < w - 2; x++) {
          const int y = h - 6;
          if (chA(s.get(f * w + x, y))) s.set(f * w + x, y, ((x / 2) & 1) ? (cul == CU_JADE ? kJadeTileP[3] : M.cloth[3]) : kJadeTileP[2]);
        }
    return s;
  }
  Canvas sheet(w * n, h);
  for (int f = 0; f < n; f++) {
    Canvas cell(w, h);
    switch (p) {
      case Prop::Well: wellStyled(cell, kind, M); break;
      case Prop::Lamppost: lampStyled(cell, kind, M); break;
      case Prop::Bench: benchStyled(cell, kind, M); break;
      case Prop::FenceH: fenceStyled(cell, false, st.fence, M); break;
      case Prop::FenceV: fenceStyled(cell, true, st.fence, M); break;
      case Prop::Statue: statueStyled(cell, st, M); break;
      case Prop::Shrine: shrineStyled(cell, cul, M); break;
      case Prop::Brazier: brazierStyled(cell, kind, f, M); break;
      default: break;
    }
    outline(cell);
    if (p == Prop::Bench && kind == 2) divanShadow(cell);
    place(sheet, cell, f, 0);
  }
  return sheet;
}

}  // namespace art
