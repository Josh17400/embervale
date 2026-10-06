// M3 culture-styled props (rpg/art/art_culture.h propSprite(Prop, PropStyle)), architecture lane: the small things of
// a settlement in its culture's idiom (VISION_PLAN 5.5): fences, wells, lamps, benches, the centrepiece and statues,
// shrines, fire bowls and fountains. Same canvas, anchor and frames as the classic prop; the classic style (and any prop
// without variants) is exactly propSprite(p). Also the M3 culture furniture (Cushion, LowTable, Hammock, SleepingMat)
// for art_props.cpp. Everything in the game's high 3/4 view, lit from the top-left.
#include "rpg/art/art_internal.h"
#include "rpg/art/art_heraldry.h"

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
        for (int y = 0; y <= base; y++) {
          c.set(cx - 1, y, (y % 5 == 2) ? rgba(120, 130, 60) : rgba(188, 196, 104));
          c.set(cx, y, (y % 5 == 2) ? rgba(100, 108, 50) : rgba(140, 150, 70));
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
    case 2: {   // cushions on a rug
      for (int y = base - 5; y <= base; y++) for (int x = 0; x < w; x++) c.set(x, y, ((x + y) % 4 == 0) ? M.cloth[1] : (y == base - 5 || y == base ? M.cloth[3] : M.cloth[2]));
      for (int s = 0; s < 2; s++) ball(c, s ? w - 5.0f : 4.5f, base - 4.5f, 3.6f, 2.8f, s ? kPurple : M.cloth);
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

}  // namespace

Canvas dykePiece(int mask, const PropStyle& st) {
  const Mat M = matOf(st);
  const Ramp& S = M.stone;
  const int W = 16, H = 24, OY = 8, HT = 7;   // canvas y = ground y + OY; the wall stands HT px tall
  const int a0 = 5, a1 = 10;                  // the wall's width across its run (ground px a0..a1)
  auto in = [&](int gx, int gy) -> bool {     // the footprint, with the neighbours' arms beyond the tile edges
    const bool cx = gx >= a0 && gx <= a1, cy = gy >= a0 && gy <= a1;
    if (cx && cy) return true;
    if (cx && gy < a0) return (mask & 1) != 0 && gy >= -16;
    if (cx && gy > a1) return (mask & 4) != 0 && gy <= 31;
    if (cy && gx < a0) return (mask & 8) != 0 && gx >= -16;
    if (cy && gx > a1) return (mask & 2) != 0 && gx <= 31;
    return false;
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
  outline(c);
  // the cast shadow on the ground, to the south-east (light from the top left), where nothing else is drawn
  for (int gy = 0; gy < 16; gy++)
    for (int gx = 0; gx < W; gx++) {
      if (in(gx, gy) || chA(c.get(gx, gy + OY)) != 0) continue;
      if (in(gx - 1, gy) || in(gx - 2, gy) || in(gx - 3, gy - 1) || in(gx, gy - 1) || in(gx - 1, gy - 1) || in(gx - 2, gy - 2))
        c.set(gx, gy + OY, rgba(16, 22, 12, 72));
    }
  return c;
}

bool propStyled(Prop p) {
  switch (p) {
    case Prop::Well: case Prop::Lamppost: case Prop::Bench: case Prop::FenceH: case Prop::FenceV: case Prop::Fountain:
    case Prop::Statue: case Prop::Shrine: case Prop::Brazier: case Prop::MarketStall:
      return true;
    default: return isStall(p);
  }
}

Canvas propSprite(Prop p, const PropStyle& st) {
  if (st.classic() || !propStyled(p)) return propSprite(p);
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
    place(sheet, cell, f, 0);
  }
  return sheet;
}

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
  if (a == K_MARSH) {   // the marsh folk keep their things in lidded baskets stacked on one another
    const int n = p == Prop::Dresser ? 2 : 3;
    int y = base;
    for (int i = 0; i < n; i++) {
      const int h = 9, w = W - 2 - (i & 1) * 2, x0 = 1 + (i & 1);
      for (int yy = y - h + 1; yy <= y; yy++)
        for (int x = x0; x < x0 + w; x++) {
          int k = ((x + yy * 2) % 3 == 0) ? 1 : 2;
          if (x == x0) k = 3;
          if (x == x0 + w - 1) k = 1;
          if (yy == y - h + 1) k = 4;
          c.set(x, yy, kReed[k]);
        }
      hline(c, x0, x0 + w - 1, y - h + 3, kReed[0]);   // the lid's rim
      y -= h;
    }
    c.set(W / 2, y + 2, kBamboo[4]);
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
