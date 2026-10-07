// EMBERVALE art, M3b FORTIFICATIONS & GROUND lane: the culture's street furniture beyond art_culture_props.cpp's
// (VISION_PLAN 15.14: every built prop is drawn from the culture's parts): signposts, tents, the old market stall,
// banners, gravestones by the burial custom, market crosses, the standing stones' variants, and the dispatch to the
// work yards (art_market.cpp), the wayside props (art_wild.cpp) and the market's tables and cloths.
// Same canvas, anchor and frames as the classic prop; high 3/4 view, light from the top-left.
#include "rpg/art/art_internal.h"
#include "rpg/art/art_parts.h"

namespace art {

namespace {

enum : int { A_FJORD, A_HIGHLAND, A_HEART, A_IMPERIAL, A_DUNE, A_STEPPE, A_MARSH, A_JADE, A_RIVER, A_SUN, A_SYLVAN, A_STAR };

// the archetypes' own materials (the PropStyle's cloths and accent override the cloths)
struct Base { uint32_t wood, woodDark, stone, roof, cloth, cloth2, metal; };
const Base kBase[12] = {
    {rgba(104, 70, 48), rgba(62, 42, 36), rgba(118, 116, 122), rgba(98, 116, 62), rgba(176, 48, 40), rgba(228, 190, 80), rgba(124, 128, 140)},     // fjord: tarred timber, sod
    {rgba(128, 104, 80), rgba(82, 66, 54), rgba(112, 112, 120), rgba(150, 118, 80), rgba(46, 92, 70), rgba(140, 40, 40), rgba(120, 124, 132)},    // highland: weathered, heather thatch
    {rgba(142, 92, 56), rgba(90, 58, 46), rgba(118, 120, 134), rgba(186, 142, 66), rgba(176, 50, 46), rgba(196, 174, 140), rgba(116, 122, 138)},   // heartland (the classic)
    {rgba(150, 98, 58), rgba(96, 62, 44), rgba(200, 184, 156), rgba(178, 82, 54), rgba(150, 44, 38), rgba(226, 190, 104), rgba(182, 136, 60)},    // imperial: travertine, terracotta
    {rgba(176, 140, 96), rgba(120, 90, 62), rgba(214, 176, 124), rgba(196, 170, 108), rgba(40, 110, 150), rgba(214, 160, 60), rgba(196, 150, 70)},  // dune: palm wood, sandstone
    {rgba(150, 112, 72), rgba(96, 70, 50), rgba(140, 126, 110), rgba(214, 200, 170), rgba(196, 70, 40), rgba(44, 84, 168), rgba(150, 140, 120)},   // steppe: birch, felt
    {rgba(168, 150, 84), rgba(104, 90, 52), rgba(110, 104, 92), rgba(170, 144, 80), rgba(206, 128, 46), rgba(70, 120, 90), rgba(110, 112, 100)},   // marsh: bamboo, reed
    {rgba(150, 40, 36), rgba(70, 40, 36), rgba(124, 130, 140), rgba(52, 128, 92), rgba(176, 40, 40), rgba(232, 190, 80), rgba(182, 136, 60)},     // jade: red lacquer, green tile
    {rgba(120, 84, 58), rgba(74, 52, 40), rgba(160, 88, 66), rgba(74, 86, 100), rgba(40, 96, 72), rgba(236, 232, 220), rgba(140, 140, 150)},      // river: brick, slate
    {rgba(110, 74, 52), rgba(66, 44, 36), rgba(222, 194, 150), rgba(190, 160, 90), rgba(40, 140, 136), rgba(200, 60, 40), rgba(196, 150, 70)},     // sun temple: lime, palm thatch
    {rgba(150, 136, 110), rgba(90, 84, 66), rgba(128, 140, 116), rgba(88, 130, 64), rgba(90, 160, 120), rgba(232, 214, 140), rgba(176, 182, 168)},  // sylvan: silvered living wood, leaves
    {rgba(178, 174, 166), rgba(118, 120, 132), rgba(224, 228, 236), rgba(74, 96, 154), rgba(60, 90, 170), rgba(220, 226, 240), rgba(200, 206, 220)},  // starspire: pale ash, white stone
};

struct M {
  int a;
  Ramp wood, dark, stone, roof, cloth, cloth2, metal;
};
M mOf(const PartLook& L) {
  M m;
  m.a = L.arch;
  if (L.arch < 0) { m.wood = kWood; m.dark = kWoodDark; m.stone = kStone; m.roof = kThatch; m.cloth = kRed; m.cloth2 = kCloth; m.metal = kIron; return m; }
  m.wood = ramp(opaque(L.wood)); m.dark = ramp(opaque(L.woodDark)); m.stone = ramp(opaque(L.stone), 0.85f);
  m.roof = ramp(opaque(L.roof), 0.85f); m.cloth = ramp(opaque(L.cloth)); m.cloth2 = ramp(opaque(L.cloth2)); m.metal = ramp(opaque(L.metal));
  return m;
}

void tuft(Canvas& c, int x0, int x1, int y, uint32_t seed) {
  for (int x = x0; x <= x1; x++) {
    if (hashf(x, y, seed) > 0.45f) continue;
    const int h = 1 + (int)(hash3(x, 1, seed) % 3);
    for (int j = 0; j < h; j++) c.set(x + ((j == h - 1 && (x & 1)) ? 1 : 0), y - j, kLeaf[j == h - 1 ? 3 : (j == 0 ? 1 : 2)]);
  }
}
// a vertical round post / pole (x0..x0+w-1) lit on its west side
void post(Canvas& c, int x0, int w, int y0, int y1, const Ramp& R) {
  for (int y = y0; y <= y1; y++)
    for (int i = 0; i < w; i++) c.set(x0 + i, y, R[w == 1 ? 2 : (i == 0 ? 3 : (i == w - 1 ? 1 : 2))]);
}

// ---------------------------------------------------------------- signposts (16 x 22)
void signpost(Canvas& c, const M& m) {
  const int cx = c.w / 2, by = c.h - 1;
  auto arrow = [&](int y, int dir, int len, const Ramp& R, uint32_t ink) {
    const int x0 = dir > 0 ? cx - 2 : cx - len + 1;
    for (int j = 0; j < 4; j++)
      for (int i = 0; i < len; i++) {
        const bool tip = dir > 0 ? i == len - 1 : i == 0;
        if (tip && (j == 0 || j == 3)) continue;
        c.set(x0 + i, y + j, R[j == 0 ? 4 : (j == 3 ? 1 : 3)]);
      }
    for (int i = 2; i < len - 3; i += 2) c.set(x0 + i + (dir < 0 ? 1 : 0), y + 1 + (i % 4 == 0), ink);
  };
  switch (m.a) {
    case A_FJORD: case A_HIGHLAND: {   // a carved post with a knotwork head and one rune board (highland: in a cairn)
      post(c, cx - 2, 3, 4, by, m.dark);
      for (int y = 1; y < 5; y++) for (int x = cx - 2; x <= cx; x++) c.set(x, y, m.dark[((x + y) & 1) ? 3 : 2]);   // the knot
      c.set(cx - 1, 0, m.dark[4]);
      arrow(7, 1, 10, m.wood, rgba(176, 48, 40));
      if (m.a == A_HIGHLAND)
        for (int k = 0; k < 5; k++) rock(c, cx - 4.0f + (k % 3) * 4.0f, by - 1.5f - (k / 3) * 2.5f, 2.6f, 2.0f, m.stone, 33u + (uint32_t)k, 4);
      break;
    }
    case A_IMPERIAL: {   // a milestone: a short column on a plinth, a bronze cap, the miles cut in a band
      for (int y = by - 3; y <= by; y++) for (int x = cx - 5; x <= cx + 4; x++) c.set(x, y, m.stone[y == by - 3 ? 4 : (x == cx + 4 ? 1 : 2)]);
      cylinder(c, cx - 3, 6, cx + 2, by - 4, m.stone);
      ellipse(c, cx - 0.5f, 6.0f, 3.2f, 1.4f, m.stone[4]);
      for (int x = cx - 3; x <= cx + 2; x++) { c.set(x, 10, m.stone[1]); if ((x & 1) && x < cx + 2) c.set(x, 12, m.stone[0]); c.set(x, 14, m.stone[1]); }
      ball(c, cx - 0.5f, 4.5f, 2.4f, 1.8f, kBrass);
      break;
    }
    case A_DUNE: case A_SUN: {   // a stele: a tapering slab, a painted band, a carved sign
      for (int y = 3; y <= by; y++) {
        const int half = 3 + (y - 3) / 8;
        for (int x = cx - half; x < cx + half; x++) c.set(x, y, m.stone[x == cx - half ? 4 : (x == cx + half - 1 ? 1 : ((x + y * 3) % 9 == 0 ? 2 : 3))]);
      }
      for (int x = cx - 3; x < cx + 3; x++) { c.set(x, 2, m.stone[4]); c.set(x, 8, m.cloth[3]); c.set(x, 9, m.cloth[1]); }
      c.set(cx - 1, 1, m.stone[4]); c.set(cx, 1, m.stone[3]);
      const Canvas g = glyphSprite(0x51A0u + (uint32_t)m.a * 77u, m.a == A_SUN ? 2 : 1, m.stone[0], 1);
      blit(c, g, cx - 4, 10);
      break;
    }
    case A_STEPPE: {   // a post crowned with a horse skull, blue and white silk ribbons tied round it
      post(c, cx - 1, 2, 6, by, m.wood);
      for (int j = 0; j < 4; j++) for (int i = -2; i <= 2; i++) if (!(j == 3 && std::abs(i) == 2)) c.set(cx + i, 2 + j, kBone[i < 0 ? 4 : 3]);
      c.set(cx - 1, 3, kInk); c.set(cx + 1, 3, kInk);
      for (int k = 0; k < 3; k++) {
        const uint32_t col = k == 1 ? rgba(236, 232, 220) : rgba(52, 110, 196);
        for (int j = 0; j < 6; j++) { c.set(cx + 1 + (j + k) / 3, 8 + k * 2 + j, col); }
      }
      break;
    }
    case A_MARSH: {   // a bamboo pole, a plank sign hung from it on cords, a charm of shells
      for (int y = 2; y <= by; y++) { c.set(cx - 1, y, (y % 5 == 2) ? m.wood[1] : m.wood[3]); c.set(cx, y, (y % 5 == 2) ? m.wood[0] : m.wood[2]); }
      hline(c, cx - 1, cx + 6, 4, m.wood[2]);
      for (int y = 5; y <= 7; y++) { c.set(cx + 1, y, kCloth[1]); c.set(cx + 6, y, kCloth[1]); }
      for (int y = 8; y <= 12; y++) for (int x = cx; x <= cx + 7; x++) c.set(x, y, m.dark[y == 8 ? 3 : (y == 12 ? 1 : 2)]);
      for (int x = cx + 1; x <= cx + 6; x += 2) c.set(x, 10, m.cloth[3]);
      c.set(cx - 3, 6, kBone[4]); c.set(cx - 3, 7, kBone[3]); c.set(cx - 2, 5, kCloth[1]);
      break;
    }
    case A_JADE: {   // a red pole, a long board of gold characters under a little tiled cap
      post(c, cx - 1, 2, 3, by, m.wood);
      for (int y = 4; y <= 15; y++) for (int x = cx - 4; x <= cx + 1; x++) c.set(x, y, (x == cx - 4 || x == cx + 1 || y == 4 || y == 15) ? kGold[2] : rgba(36, 46, 70));
      for (int y = 6; y <= 13; y += 2) { c.set(cx - 2, y, kGold[4]); c.set(cx - 1, y, kGold[3]); if (y % 4 == 2) c.set(cx - 3, y + 1, kGold[3]); }
      const Ramp T = ramp(rgba(52, 128, 92));
      for (int x = cx - 6; x <= cx + 3; x++) { c.set(x, 2, T[3]); c.set(x, 3, T[1]); }
      c.set(cx - 7, 1, T[3]); c.set(cx + 4, 1, T[2]);
      break;
    }
    case A_SYLVAN: {   // a living sapling trained into a post, a leaf-shaped board, leaves at its crown
      post(c, cx - 1, 2, 5, by, ramp(rgba(126, 112, 90)));
      for (int k = 0; k < 7; k++) c.set(cx - 3 + (k * 5) % 7, 2 + (k * 3) % 4, kLeaf[2 + (k & 1)]);
      for (int i = 0; i < 9; i++) {
        const int hh = i < 2 ? 1 : (i > 6 ? 1 : 2);
        for (int j = -hh; j <= hh; j++) c.set(cx + 1 + i, 9 + j, kLeaf[j < 0 ? 3 : 2]);
      }
      hline(c, cx + 1, cx + 8, 9, kLeaf[1]);
      break;
    }
    case A_STAR: {   // a slender white post, a silver arrow, a glowing crystal on top
      post(c, cx - 1, 2, 5, by, m.stone);
      for (int i = 0; i < 9; i++) { c.set(cx - 1 + i, 8, kIron[4]); c.set(cx - 1 + i, 9, kIron[2]); }
      c.set(cx + 8, 7, kIron[3]); c.set(cx + 8, 10, kIron[1]); c.set(cx + 9, 8, kIron[3]); c.set(cx + 9, 9, kIron[2]);
      const Ramp G = ramp5(rgba(40, 110, 120), rgba(70, 170, 170), rgba(130, 220, 200), rgba(196, 248, 226), rgba(240, 255, 246));
      for (int j = 0; j < 4; j++) { c.set(cx - 1, 1 + j, G[4 - j / 2]); c.set(cx, 1 + j, G[3 - j / 2]); }
      break;
    }
    case A_RIVER: {   // the classic finger post, its boards painted white with a green rim
      post(c, cx - 1, 2, 4, by, m.wood);
      const Ramp W = ramp(rgba(226, 224, 210));
      arrow(3, 1, 11, W, m.cloth[1]);
      arrow(9, -1, 10, W, m.cloth[1]);
      break;
    }
    default: {
      post(c, cx - 1, 2, 4, by, m.wood);
      arrow(3, 1, 11, m.wood, m.wood[0]);
      arrow(9, -1, 10, m.wood, m.wood[0]);
      break;
    }
  }
  tuft(c, cx - 4, cx + 3, by, 9u + (uint32_t)m.a);
}

// ---------------------------------------------------------------- tents (36 x 28)
void tentStyled(Canvas& c, const M& m, const PropStyle& st) {
  const float cx = c.w * 0.5f;
  const int base = c.h - 2;
  const Ramp CL = st.awningA ? ramp(opaque(st.awningA)) : m.cloth, CL2 = st.awningB ? ramp(opaque(st.awningB)) : m.cloth2;
  switch (m.a) {
    case A_STEPPE: {   // a yurt: a felt drum under a low dome, a painted door, rope bands, the crown ring
      const Ramp F = ramp(rgba(226, 214, 190), 0.8f);
      const float rx = 15.0f, top = base - 9.0f;
      for (int y = (int)top; y <= base; y++)
        for (int x = (int)(cx - rx); x <= (int)(cx + rx); x++) {
          const float dx = (x + 0.5f - cx) / rx;
          if (std::fabs(dx) > 1) continue;
          const float yy = top + std::sqrt(std::max(0.0f, 1 - dx * dx)) * 3.5f;
          if (y < yy) continue;
          int k = std::clamp(lightIndex(lightAt(dx * 0.9f, 0.1f), x, y, 0.05f), 1, 3);
          if ((int)(y - yy) == 2 || (int)(y - yy) == 6) { c.set(x, y, CL[k]); continue; }   // the rope bands
          c.set(x, y, F[k]);
        }
      for (int y = 0; y < 10; y++)   // the dome
        for (int x = -15; x <= 15; x++) {
          const float dx = x / 15.5f, dy = (y - 9.5f) / 9.5f;
          if (dx * dx + dy * dy > 1) continue;
          int k = lightIndex(lightAt(dx * 0.8f, dy * 0.7f), x, y, 0.06f);
          if ((x + 32) % 6 == 0 && y > 3) k = std::max(0, k - 1);   // the roof poles under the felt
          c.set((int)cx + x, (int)top - 9 + y + 3, F[std::clamp(k, 1, 4)]);
        }
      ellipse(c, cx, top - 5.0f, 3.0f, 1.2f, m.wood[1]);
      ellipse(c, cx, top - 5.4f, 2.0f, 0.7f, m.wood[3]);
      for (int y = base - 7; y <= base; y++) for (int x = (int)cx - 3; x <= (int)cx + 2; x++)
        c.set(x, y, (x == (int)cx - 3 || x == (int)cx + 2 || y == base - 7) ? kGold[2] : CL[(x + y) % 3 == 0 ? 3 : 2]);
      break;
    }
    case A_FJORD: {   // an A-frame of striped wool over crossed gable poles carved into dragon heads
      const int top = 6;
      for (int y = top; y <= base; y++) {
        const float t = (float)(y - top) / (base - top);
        const int half = (int)(1 + t * (c.w * 0.5f - 3));
        for (int x = (int)cx - half; x <= (int)cx + half; x++) {
          const bool stripe = ((x + 64) / 3) % 2 == 0;
          int k = x < cx ? 3 : 1;
          if (x == (int)cx - half) k = 2;
          c.set(x, y, stripe ? CL[k] : kCloth[k + 1 > 4 ? 4 : k + 1]);
        }
      }
      for (int y = top + 9; y <= base; y++) {
        const int half = (int)((y - top - 9) * 0.45f);
        for (int x = (int)cx - half; x <= (int)cx + half; x++) c.set(x, y, y > base - 2 ? kInk : rgba(46, 34, 44));
      }
      for (int k = 0; k < 7; k++) { c.set((int)cx - 1 - k, top - 1 - k / 2, m.dark[3]); c.set((int)cx + k, top - 1 - k / 2, m.dark[2]); }
      c.set((int)cx - 8, top - 5, m.dark[3]); c.set((int)cx - 9, top - 4, CL[3]); c.set((int)cx + 7, top - 5, m.dark[2]); c.set((int)cx + 8, top - 4, CL[3]);
      hline(c, (int)cx - 15, (int)cx + 15, base, kCloth[0]);
      break;
    }
    case A_MARSH: case A_SYLVAN: {   // a cone of hides / leaf-painted cloth on poles crossing at the top
      const int top = 4;
      const Ramp H = m.a == A_SYLVAN ? ramp(rgba(120, 156, 96)) : ramp(rgba(176, 148, 104));
      for (int y = top; y <= base; y++) {
        const float t = (float)(y - top) / (base - top);
        const int half = (int)(1 + t * 13.0f);
        for (int x = (int)cx - half; x <= (int)cx + half; x++) {
          const float u = (x + 0.5f - cx) / std::max(1, half);
          int k = u < -0.35f ? 3 : (u < 0.35f ? 2 : 1);
          if (y > base - 5 && ((x + 64) % 5 == 0)) k = std::max(0, k - 1);
          uint32_t col = H[k];
          if (m.a == A_SYLVAN && hash3(x / 2, y / 2, 7u) % 9 == 0) col = kLeaf[k + 1];
          if (y == top + 8 || y == top + 9) col = CL[k + (y & 1)];   // a painted band
          c.set(x, y, col);
        }
      }
      for (int y = top + 10; y <= base; y++) { const int half = (int)((y - top - 10) * 0.35f); for (int x = (int)cx - half; x <= (int)cx + half; x++) c.set(x, y, rgba(40, 30, 38)); }
      for (int k = 0; k < 5; k++) { c.set((int)cx - 1 - k / 2, top - 1 - k, m.wood[3]); c.set((int)cx + 1 + k / 2, top - 1 - k, m.wood[1]); }
      break;
    }
    case A_HEART: case A_RIVER: case A_HIGHLAND: case -1: {   // the classic A-frame in the culture's canvas
      const int top = 3;
      const Ramp Cv = m.a == A_HIGHLAND ? ramp(rgba(150, 140, 110)) : kCloth;
      for (int y = top; y <= base; y++) {
        const float t = (float)(y - top) / (base - top);
        const int half = (int)(1 + t * (c.w * 0.5f - 2));
        for (int x = (int)cx - half; x <= (int)cx + half; x++) {
          int k = x < cx ? 3 : 1;
          if (x == (int)cx - half) k = 2;
          if ((x + y) % 6 == 0) k = std::max(0, k - 1);
          if (m.a == A_HIGHLAND && (((x + 64) / 2 + y / 3) % 4 == 0)) { c.set(x, y, CL[k]); continue; }   // a tartan check
          c.set(x, y, Cv[k]);
        }
      }
      for (int y = top + 8; y <= base; y++) {
        const int half = (int)((y - top - 8) * 0.45f);
        for (int x = (int)cx - half; x <= (int)cx + half; x++) c.set(x, y, y > base - 2 ? kInk : rgba(46, 34, 44));
      }
      vline(c, (int)cx, top - 2, top + 1, m.wood[2]);
      hline(c, (int)cx - 14, (int)cx + 14, base, Cv[0]);
      break;
    }
    default: {   // a pavilion: cloth walls, a peaked roof with a scalloped valance, a finial
      const bool jade = m.a == A_JADE;
      const int wallTop = base - 9;
      for (int y = wallTop; y <= base; y++)
        for (int x = (int)cx - 13; x <= (int)cx + 12; x++) {
          const bool stripe = !jade && ((x + 64) / 3) % 2 == 0;
          int k = x < (int)cx - 9 ? 3 : (x > (int)cx + 8 ? 1 : 2);
          c.set(x, y, stripe ? CL2[k] : CL[k]);
        }
      for (int y = base - 7; y <= base; y++) for (int x = (int)cx - 3; x <= (int)cx + 2; x++) c.set(x, y, rgba(44, 32, 42));   // the open flap
      c.set((int)cx - 4, base - 7, CL[4]); c.set((int)cx + 3, base - 7, CL[0]);
      const int roofTop = 2;
      for (int y = roofTop; y < wallTop; y++) {
        const float t = (float)(y - roofTop) / (wallTop - roofTop);
        const int half = (int)(2 + t * 13.0f + (jade && t > 0.75f ? 2 : 0));
        for (int x = (int)cx - half; x <= (int)cx + half - 1; x++) {
          int k = x < (int)cx ? 3 : 2;
          if (x < (int)cx - half + 2) k = 4;
          if (!jade && ((x + 64) / 3) % 2 == 0) { c.set(x, y, CL2[k]); continue; }
          c.set(x, y, CL[k]);
        }
      }
      for (int x = (int)cx - 15; x <= (int)cx + 14; x++) if (((x + 64) % 4) != 3) { c.set(x, wallTop, CL[1]); if ((x + 64) % 4 == 1) c.set(x, wallTop + 1, CL[1]); }   // the valance
      if (jade) { c.set((int)cx - 17, wallTop - 3, CL[3]); c.set((int)cx + 16, wallTop - 3, CL[1]); }
      vline(c, (int)cx, 0, roofTop, kGold[3]); c.set((int)cx, 0, kGold[4]);
      break;
    }
  }
}

// ---------------------------------------------------------------- the old market stall (40 x 36)
void stallStyled(Canvas& c, const M& m, const PropStyle& st) {
  const int W = c.w, base = c.h - 2;
  const Ramp A = st.awningA ? ramp(opaque(st.awningA)) : m.cloth, B = st.awningB ? ramp(opaque(st.awningB)) : m.cloth2;
  for (int y = 10; y <= base; y++) { c.set(2, y, m.wood[3]); c.set(3, y, m.wood[1]); c.set(W - 4, y, m.wood[2]); c.set(W - 3, y, m.wood[0]); }
  // the counter: boards, a lit top edge
  for (int y = base - 11; y <= base - 3; y++)
    for (int x = 1; x < W - 1; x++) {
      int k = y < base - 8 ? (y == base - 11 ? 4 : 3) : ((x % 7 == 0) ? 1 : 2);
      if (x == 1) k = std::min(4, k + 1);
      if (x == W - 2) k = std::max(0, k - 1);
      c.set(x, y, m.wood[k]);
    }
  const uint32_t goods[4] = {rgba(220, 60, 50), rgba(250, 180, 60), rgba(120, 180, 70), rgba(160, 90, 160)};
  for (int i = 0; i < 6; i++) { const Ramp g = ramp(goods[(i + m.a + 4) % 4], 0.8f); ball(c, 6 + i * 5.3f, base - 11.5f, 2.0f, 1.6f, g); ball(c, 7.5f + i * 5.3f, base - 12.5f, 1.4f, 1.2f, g); }
  // the awning in the culture's kind
  const int kind = st.awning;
  for (int y = 1; y <= 10; y++)
    for (int x = 0; x < W; x++) {
      const int k = y < 3 ? 4 : (y < 8 ? 3 : 2);
      uint32_t col;
      switch (kind) {
        case 1: col = A[y == 10 ? 1 : k]; break;                                                    // plain dyed cloth
        case 2: col = ramp(rgba(198, 170, 108))[(x + y) % 3 == 0 ? k - 1 : k]; break;              // reed matting
        case 3: col = m.roof[((y % 3) == 0) ? k - 2 : k - 1]; break;                                // a tiled lean-to
        case 4: col = (((x / 4) & 1) ? B : A)[k]; break;                                            // silk
        case 5: col = ramp(rgba(150, 104, 68))[(hash3(x / 5, y / 4, 3u) % 3 == 0) ? k - 1 : k]; break;   // hide
        default: col = (((x / 4) & 1) ? B : A)[k]; break;                                           // striped canvas
      }
      if (y == 10) col = mix(col, kInk, 0.35f);
      c.set(x, y, col);
    }
  for (int x = 0; x < W; x++) {
    if (kind == 3) { c.set(x, 11, m.roof[0]); continue; }
    if ((x % 4) == 1 || (x % 4) == 2) c.set(x, 11, (((x / 4) & 1) ? B : A)[1]);
    if (kind == 4 && x % 8 == 3) { c.set(x, 12, kGold[3]); c.set(x, 13, kGold[2]); }   // tassels
  }
}

// ---------------------------------------------------------------- banners (14 x 32, 4 frames)
void bannerStyled(Canvas& c, int frame, const M& m, const PropStyle& st) {
  const int base = c.h - 1;
  const Ramp F = st.awningA ? ramp(opaque(st.awningA)) : m.cloth, T = st.awningB ? ramp(opaque(st.awningB)) : m.cloth2;
  auto pole = [&](const Ramp& R) { for (int y = 2; y <= base; y++) { c.set(2, y, R[3]); c.set(3, y, R[1]); } };
  if (m.a == A_STEPPE) {   // a tug: a pole with a trident head and a ring of horse-tail hair streaming down
    pole(m.wood);
    c.set(2, 0, kIron[4]); c.set(1, 1, kIron[3]); c.set(4, 1, kIron[2]); c.set(2, 1, kIron[3]); c.set(3, 1, kIron[2]);
    for (int y = 4; y < 20; y++) {
      const int sway = (int)std::lround(std::sin(y * 0.4f + frame * 1.5f) * (y - 4) * 0.08f);
      for (int x = 0; x <= 6; x++) {
        if (x >= 2 && x <= 3 && y < 8) continue;
        const int hx = x + sway + (x > 3 ? 1 : 0);
        if ((x + y) % 2 == 0 || y < 9) c.set(hx, y, ((x + y / 3) % 3 == 0) ? rgba(84, 72, 70) : rgba(34, 28, 32));
      }
    }
    hline(c, 1, 4, 4, kGold[3]);
    return;
  }
  pole(m.a == A_STAR ? m.stone : m.wood);
  // the finial
  switch (m.a) {
    case A_IMPERIAL: ball(c, 2.5, 1.0, 1.8, 1.4, kGold); break;
    case A_DUNE: c.set(1, 0, kGold[4]); c.set(2, 1, kGold[3]); c.set(3, 0, kGold[3]); break;   // a crescent
    case A_STAR: c.set(2, 0, rgba(240, 255, 246)); c.set(3, 1, rgba(130, 220, 200)); break;
    case A_FJORD: c.set(2, 1, m.dark[3]); c.set(3, 0, m.dark[3]); c.set(4, 0, rgba(196, 56, 40)); break;
    default: c.set(2, 1, kGold[4]); c.set(3, 1, kGold[2]); c.set(2, 0, kGold[3]); break;
  }
  hline(c, 3, c.w - 2, 3, m.wood[2]);
  // the cloth's shape by culture: imperial vexillum (square, fringed), jade streamer (long, bordered), fjord raven
  // pennant (a triangle), sylvan leaf, the rest swallow-tailed
  const int y0 = 4, y1 = m.a == A_JADE ? 28 : (m.a == A_IMPERIAL ? 18 : 24);
  for (int y = y0; y < y1; y++) {
    const float t = (y - y0) / (float)(y1 - y0);
    const int off = (int)std::lround(std::sin(t * 4.0f + frame * 1.6f) * 1.2f * t);
    for (int x = 4; x < c.w - 1; x++) {
      const int i = x - 4, cw = c.w - 5;
      bool cut = false;
      switch (m.a) {
        case A_FJORD: cut = i > (int)((1 - t) * cw + 0.5f); break;
        case A_SYLVAN: { const float half = cw * 0.5f * std::sin(t * 3.14159f) * 1.1f + 0.5f; cut = std::fabs(i + 0.5f - cw * 0.5f) > half; break; }
        case A_IMPERIAL: case A_JADE: break;
        default: { const int mid = cw / 2; cut = y > y1 - 5 && std::abs(i - mid) < (y - (y1 - 5)); break; }
      }
      if (cut) continue;
      const float wave = std::sin(i * 0.9f + frame * 1.6f + t * 2);
      int k = wave > 0.5f ? 3 : (wave < -0.5f ? 1 : 2);
      uint32_t col = F[k];
      if (m.a == A_JADE && (i == 0 || i == cw - 1 || y == y0)) col = kInk;
      else if (m.a == A_IMPERIAL && (y == y1 - 1)) col = (i & 1) ? kGold[3] : kGold[1];
      else if (i == 0 || y == y0) col = T[std::clamp(k + 1, 1, 4)];
      if (m.a == A_SYLVAN && std::abs(i - cw / 2) == 0) col = F[k + 1 > 4 ? 4 : k + 1];   // the leaf's vein
      c.set(x + off, y, col);
    }
  }
  // the device: a glyph-like mark in the second colour
  const int ex = (4 + c.w - 2) / 2;
  if (m.a == A_STAR) { c.set(ex, 9, T[4]); c.set(ex - 1, 10, T[3]); c.set(ex + 1, 10, T[3]); c.set(ex, 11, T[4]); c.set(ex, 10, kWhite); }
  else if (m.a == A_JADE) { for (int y = 8; y <= 22; y += 3) { c.set(ex, y, kGold[4]); c.set(ex - 1, y + 1, kGold[3]); } }
  else if (m.a != A_FJORD) { c.set(ex, 9, T[4]); c.set(ex - 1, 10, T[3]); c.set(ex + 1, 10, T[2]); c.set(ex, 10, T[3]); c.set(ex, 11, T[2]); c.set(ex, 12, T[1]); }
  else { c.set(6, 8, kInk); c.set(7, 8, kInk); c.set(7, 9, kInk); c.set(8, 9, kInk); }   // the raven
}

// ---------------------------------------------------------------- gravestones (14 x 16), by the burial custom
void gravestoneStyled(Canvas& c, const M& m) {
  const int cx = c.w / 2, base = c.h - 2;
  const Ramp& S = m.stone;
  auto slab = [&](int hw, int topY, bool round) {
    for (int y = topY; y <= base; y++)
      for (int x = cx - hw; x < cx + hw; x++) {
        const int j = y - topY;
        if (round && j < 3) { const float dx = (x + 0.5f - cx) / hw; if (dx * dx + (3 - j) * (3 - j) / 9.0f > 1.0f) continue; }
        int k = x == cx - hw ? 3 : (x >= cx + hw - 2 ? 1 : 2);
        if (j == 0) k = 3;
        c.set(x, y, S[k]);
      }
  };
  switch (m.a) {
    case A_FJORD: {   // a rune stone: a tall slab, the runes painted red
      slab(4, 1, true);
      for (int y = 4; y < base - 2; y += 2) { c.set(cx - 2, y, rgba(176, 48, 40)); c.set(cx - 1 + (y % 4 == 0), y + 1, rgba(176, 48, 40)); c.set(cx + 1, y, rgba(150, 40, 36)); }
      break;
    }
    case A_HIGHLAND: {   // a cairn heaped over the grave, a small cross slab set in it
      for (int k = 0; k < 6; k++) rock(c, cx - 4.0f + (k % 3) * 4.0f, base - 1.5f - (k / 3) * 3.0f, 2.8f, 2.2f, S, 51u + (uint32_t)k, 4);
      for (int y = 2; y <= 8; y++) c.set(cx, y, S[3]);
      hline(c, cx - 2, cx + 2, 4, S[3]);
      c.set(cx + 1, 3, S[1]); c.set(cx + 1, 5, S[1]);
      break;
    }
    case A_IMPERIAL: case A_DUNE: case A_SUN: case A_STAR: {   // a stele: a tall tapering slab, a carved band or a sign
      for (int y = 1; y <= base; y++) {
        const int half = 3 + (y > 6 ? 1 : 0);
        for (int x = cx - half; x < cx + half; x++) c.set(x, y, S[x == cx - half ? 4 : (x == cx + half - 1 ? 1 : 3)]);
      }
      if (m.a == A_IMPERIAL) { for (int x = cx - 4; x < cx + 4; x++) c.set(x, 1, S[4]); c.set(cx - 1, 0, S[4]); c.set(cx, 0, S[3]); hline(c, cx - 2, cx + 1, 5, S[1]); hline(c, cx - 2, cx + 1, 7, S[1]); }
      if (m.a == A_DUNE) { ball(c, cx - 0.5f, 1.5f, 2.6f, 1.8f, m.cloth); hline(c, cx - 3, cx + 2, 3, m.cloth[1]); }   // a turban-capped stone
      if (m.a == A_SUN) { hline(c, cx - 4, cx + 3, 5, m.cloth2[2]); hline(c, cx - 4, cx + 3, 6, m.cloth2[1]); c.set(cx - 1, 9, S[0]); c.set(cx, 9, S[0]); }
      if (m.a == A_STAR) { c.set(cx - 1, 6, kGlow[4]); c.set(cx, 5, rgba(196, 248, 226)); c.set(cx, 7, rgba(130, 220, 200)); c.set(cx + 1, 6, rgba(130, 220, 200)); }
      break;
    }
    case A_STEPPE: {   // a balbal: a rough stone figure, a face pecked into it, standing over a kurgan's ring
      for (int k = 0; k < 6; k++) { const float a = k / 6.0f * 6.2832f; rock(c, cx + std::cos(a) * 5.0f, base - 1.5f + std::sin(a) * 1.5f, 1.6f, 1.3f, S, 61u + (uint32_t)k, 3); }
      for (int y = 2; y <= base - 2; y++) { const int half = y < 5 ? 2 : 3; for (int x = cx - half; x < cx + half; x++) c.set(x, y, S[x == cx - half ? 4 : (x == cx + half - 1 ? 1 : 2)]); }
      c.set(cx - 1, 3, S[0]); c.set(cx + 1, 3, S[0]); hline(c, cx - 1, cx, 5, S[1]); hline(c, cx - 3, cx + 2, 7, S[1]);
      break;
    }
    case A_MARSH: case A_SYLVAN: {   // a carved wooden post (sylvan: a sapling planted on the grave, a leaf crown)
      for (int y = 2; y <= base; y++) { c.set(cx - 1, y, m.wood[3]); c.set(cx, y, m.wood[2]); c.set(cx + 1, y, m.wood[1]); }
      if (m.a == A_MARSH) { hline(c, cx - 3, cx + 3, 4, m.wood[3]); c.set(cx - 3, 3, m.wood[4]); c.set(cx + 3, 3, m.wood[2]); c.set(cx, 7, m.cloth[3]); c.set(cx, 9, m.cloth2[3]); }
      else for (int k = 0; k < 9; k++) c.set(cx - 3 + (k * 4) % 7, 1 + (k * 3) % 4, kLeaf[2 + (k & 1)]);
      for (int x = cx - 4; x <= cx + 3; x++) c.set(x, base, rgba(96, 70, 52));
      break;
    }
    case A_JADE: {   // a stone tablet under a little tiled cap, characters cut in gold
      slab(4, 4, false);
      const Ramp T = ramp(rgba(52, 128, 92));
      for (int x = cx - 6; x <= cx + 5; x++) { c.set(x, 2, T[3]); c.set(x, 3, T[1]); }
      c.set(cx - 7, 1, T[3]); c.set(cx + 6, 1, T[2]);
      for (int y = 6; y < base - 1; y += 2) c.set(cx - 1, y, kGold[3]);
      break;
    }
    default: {   // a rounded headstone with a carved cross (the classic, in the culture's stone)
      slab(5, 2, true);
      vline(c, cx, 5, 10, S[0]); hline(c, cx - 2, cx + 1, 7, S[0]);
      vline(c, cx + 1, 6, 10, S[3]);
      c.set(cx - 4, 4, kMoss[2]); c.set(cx + 3, base - 1, kMoss[2]);
      break;
    }
  }
  tuft(c, cx - 4, cx + 3, base + 1, 21u + (uint32_t)m.a);
}

// ---------------------------------------------------------------- market crosses (32 x 60): a market's centrepiece
void marketCrossStyled(Canvas& c, const M& m) {
  const int cx = c.w / 2, b = c.h - 2;
  const Ramp& S = m.stone;
  // a stepped base (round for most, square for the empire and the sun temples)
  auto steps = [&](bool square) -> int {
    int top = b;
    const int rx[3] = {13, 10, 7};
    for (int s = 0; s < 3; s++) {
      const int h = s == 2 ? 3 : 4, ry = std::max(2, rx[s] / 3);
      for (int y = top - h; y <= top; y++)
        for (int x = cx - rx[s]; x <= cx + rx[s]; x++) {
          const float u = (x - cx) / (float)rx[s];
          int k = square ? (x == cx - rx[s] ? 3 : (x == cx + rx[s] ? 1 : 2)) : (u < -0.4f ? 3 : (u < 0.3f ? 2 : 1));
          if (y == top) k = 0;
          c.set(x, y, S[k]);
        }
      if (square) for (int y = top - h - ry * 2; y < top - h; y++) for (int x = cx - rx[s]; x <= cx + rx[s]; x++) c.set(x, y, S[x < cx - rx[s] + 2 || y == top - h - ry * 2 ? 4 : 3]);
      else { ellipse(c, cx + 0.5, top - h, rx[s] + 0.5, ry, S[3]); ellipse(c, cx - 1.5, top - h - 0.5, rx[s] * 0.6, ry * 0.5, S[4]); }
      top -= h + (square ? ry * 2 : ry - 1);
    }
    return top;
  };
  switch (m.a) {
    case A_IMPERIAL: {   // a column with a gilded eagle
      const int top = steps(true);
      cylinder(c, cx - 3, 14, cx + 2, top, S);
      for (int y = 14; y <= top; y += 4) c.set(cx - 2, y, S[2]);
      hline(c, cx - 5, cx + 4, 13, S[4]); hline(c, cx - 5, cx + 4, 14, S[1]);
      ball(c, cx - 0.5f, 9.0f, 2.6f, 2.6f, kGold);
      for (int i = 0; i < 6; i++) { c.set(cx - 2 - i, 8 - i / 2, kGold[3]); c.set(cx + 1 + i, 8 - i / 2, kGold[2]); }
      c.set(cx, 5, kGold[4]); c.set(cx - 1, 6, kGold[3]);
      break;
    }
    case A_HIGHLAND: {   // a mercat cross: an octagonal shaft, a unicorn on the capital
      const int top = steps(false);
      for (int y = 16; y <= top; y++) { c.set(cx - 2, y, S[4]); c.set(cx - 1, y, S[3]); c.set(cx, y, S[2]); c.set(cx + 1, y, S[1]); }
      hline(c, cx - 4, cx + 3, 15, S[4]); hline(c, cx - 4, cx + 3, 16, S[1]);
      const Ramp U = ramp(rgba(226, 222, 210));
      for (int x = cx - 4; x <= cx + 3; x++) for (int y = 9; y <= 12; y++) c.set(x, y, U[x < cx ? 3 : 2]);
      for (int y = 12; y <= 14; y++) { c.set(cx - 4, y, U[2]); c.set(cx + 2, y, U[1]); }
      for (int y = 5; y <= 9; y++) { c.set(cx - 5, y, U[3]); c.set(cx - 4, y, U[2]); }
      c.set(cx - 6, 3, kGold[4]); c.set(cx - 6, 4, kGold[3]); c.set(cx - 5, 5, kGold[2]);   // the horn
      break;
    }
    case A_DUNE: {   // a tiled column with a lantern head
      const int top = steps(false);
      const Ramp TL = ramp(rgba(40, 118, 156));
      for (int y = 14; y <= top; y++) for (int x = cx - 3; x <= cx + 2; x++) c.set(x, y, (y % 6 < 3) ? TL[x < cx ? 3 : 2] : S[x < cx ? 3 : 2]);
      for (int y = 6; y <= 13; y++) for (int x = cx - 4; x <= cx + 3; x++) c.set(x, y, (x == cx - 4 || x == cx + 3 || y == 6) ? kBrass[2] : (y > 8 ? kGlow[3] : kBrass[3]));
      for (int k = 0; k < 4; k++) hline(c, cx - 3 + k, cx + 2 - k, 5 - k, kBrass[3]);
      c.set(cx, 1, kGold[4]);
      break;
    }
    case A_JADE: {   // a stone lantern tower: stacked stone, a light box, a curved cap
      const int top = steps(false);
      for (int y = 22; y <= top; y++) for (int x = cx - 2; x <= cx + 1; x++) c.set(x, y, S[x == cx - 2 ? 4 : (x == cx + 1 ? 1 : 3)]);
      for (int y = 18; y <= 21; y++) for (int x = cx - 6; x <= cx + 5; x++) c.set(x, y, S[y == 18 ? 4 : 2]);
      for (int y = 10; y <= 17; y++) for (int x = cx - 4; x <= cx + 3; x++) c.set(x, y, (x >= cx - 2 && x <= cx + 1 && y > 11 && y < 16) ? kGlow[y < 14 ? 4 : 3] : S[x == cx - 4 ? 4 : 2]);
      for (int k = 0; k < 4; k++) hline(c, cx - 7 + k, cx + 6 - k, 9 - k, S[k == 0 ? 2 : 4 - (k & 1)]);
      c.set(cx - 8, 8, S[4]); c.set(cx + 7, 8, S[2]);
      ball(c, cx - 0.5f, 4.0f, 1.6f, 1.6f, S);
      break;
    }
    case A_FJORD: case A_STEPPE: case A_MARSH: case A_SYLVAN: {   // a carved pole: totem faces, a crest (horse tails, wings, a crown of leaves)
      for (int k = 0; k < 7; k++) rock(c, cx - 9.0f + (k % 4) * 6.0f, b - 2.0f - (k / 4) * 3.0f, 3.4f, 2.6f, S, 71u + (uint32_t)k, 4);
      const Ramp& W = m.a == A_SYLVAN ? m.wood : m.dark;
      for (int y = 10; y <= b - 4; y++) for (int x = cx - 3; x <= cx + 2; x++) c.set(x, y, W[x == cx - 3 ? 4 : (x == cx + 2 ? 1 : (x < cx ? 3 : 2))]);
      for (int f = 0; f < 4; f++) {
        const int fy = 14 + f * 9;
        hline(c, cx - 3, cx + 2, fy + 7, W[0]);
        c.set(cx - 2, fy + 2, kInk); c.set(cx + 1, fy + 2, kInk);
        c.set(cx - 2, fy + 1, m.cloth[3]); c.set(cx + 1, fy + 1, m.cloth[3]);
        hline(c, cx - 1, cx, fy + 4, m.cloth2[f & 1 ? 2 : 3]);
      }
      if (m.a == A_STEPPE) for (int i = -4; i <= 4; i++) for (int y = 3; y < 10; y++) if ((i + y) & 1) c.set(cx + i, y + std::abs(i) / 2, (y % 3) ? rgba(36, 30, 34) : rgba(84, 72, 70));
      else if (m.a == A_SYLVAN) ball(c, cx - 0.5f, 6.0f, 7.0f, 5.0f, kLeaf);
      else for (int i = 0; i < 8; i++) { c.set(cx - 4 - i, 9 - i / 2, W[3]); c.set(cx + 3 + i, 9 - i / 2, W[1]); c.set(cx - 4 - i, 10 - i / 2, m.cloth[2]); c.set(cx + 3 + i, 10 - i / 2, m.cloth[1]); }
      break;
    }
    case A_SUN: {   // a carved stela: a great slab, a face and a sun disc cut into it, painted red
      const int top = steps(true);
      for (int y = 6; y <= top; y++) for (int x = cx - 6; x <= cx + 5; x++) c.set(x, y, S[x == cx - 6 ? 4 : (x == cx + 5 ? 1 : ((x + y * 2) % 11 == 0 ? 2 : 3))]);
      for (int x = cx - 6; x <= cx + 5; x++) c.set(x, 5, S[4]);
      ellipse(c, cx - 0.5f, 13.0f, 4.0f, 4.0f, m.cloth2[2]);
      ellipse(c, cx - 0.5f, 13.0f, 2.5f, 2.5f, S[2]);
      c.set(cx - 2, 22, S[0]); c.set(cx + 1, 22, S[0]); hline(c, cx - 2, cx + 1, 26, m.cloth2[1]);
      for (int y = 30; y <= top - 2; y += 3) hline(c, cx - 4, cx + 3, y, S[1]);
      break;
    }
    case A_STAR: {   // a crystal obelisk on a white plinth, glowing at its tip
      const int top = steps(true);
      for (int y = 6; y <= top; y++) {
        const float t = (y - 6) / (float)(top - 6);
        const int half = (int)std::lround(1.5f + t * 3.0f);
        for (int x = cx - half; x < cx + half; x++) c.set(x, y, S[x == cx - half ? 4 : (x >= cx ? 2 : 3)]);
      }
      const Ramp G = ramp5(rgba(40, 110, 120), rgba(70, 170, 170), rgba(130, 220, 200), rgba(196, 248, 226), rgba(240, 255, 246));
      for (int y = 0; y < 7; y++) for (int x = cx - 1 - (y > 2 ? 1 : 0); x <= cx + (y > 2 ? 1 : 0) - 1 + 1; x++) c.set(x, y, G[y < 3 ? 4 : (x < cx ? 3 : 2)]);
      break;
    }
    default: {   // the classic cross, a weathervane in the river towns
      const int top = steps(false);
      const int knop = (top + 12) / 2;
      for (int y = 12; y <= top; y++) { const int hw = y > knop ? 3 : 2; for (int x = cx - hw; x <= cx + hw - 1; x++) { const float u = (x - cx + 0.5f) / hw; c.set(x, y, S[u < -0.3f ? 4 : (u < 0.4f ? 3 : 1)]); } }
      hline(c, cx - 4, cx + 3, knop, S[4]); hline(c, cx - 4, cx + 3, knop + 1, S[1]);
      if (m.a == A_RIVER) {
        vline(c, cx, 2, 12, kIron[2]);
        for (int x = cx - 5; x <= cx + 5; x++) c.set(x, 6, kGold[x < cx ? 4 : 2]);
        c.set(cx - 6, 5, kGold[3]); c.set(cx + 5, 5, kGold[2]); c.set(cx + 6, 6, kGold[3]);
        ball(c, cx - 0.5f, 10.0f, 1.5f, 1.5f, kGold);
      } else {
        for (int x = cx - 7; x <= cx + 6; x++) { c.set(x, 8, S[4]); c.set(x, 9, S[3]); c.set(x, 10, S[1]); }
        for (int y = 2; y <= 12; y++) { c.set(cx - 1, y, S[4]); c.set(cx, y, S[2]); }
      }
      break;
    }
  }
  for (int i = 0; i < 6; i++) c.set(cx - 12 + i * 5, b - 1 - (i % 2), kMoss[2 + (i % 2)]);
}

// ---------------------------------------------------------------- the work yards: the classic models in the culture's materials
// Every pixel of a classic material ramp (timber, dark timber, thatch, stone, the red cloth) takes the culture's colour
// at the same step of its ramp; near matches (a ramp colour a painter darkened a little) keep their offset.
struct Remap { const Ramp* from; Ramp to; };
void remap(Canvas& c, const std::vector<Remap>& rs) {
  for (uint32_t& p : c.px) {
    if (!chA(p)) continue;
    int best = -1, bk = 0, bd = 1 << 30;
    for (int i = 0; i < (int)rs.size(); i++)
      for (int k = 0; k < 5; k++) {
        const uint32_t q = rs[(size_t)i].from->c[k];
        const int d = std::abs(chR(p) - chR(q)) + std::abs(chG(p) - chG(q)) + std::abs(chB(p) - chB(q));
        if (d < bd) { bd = d; best = i; bk = k; }
      }
    if (best < 0 || bd > 21) continue;
    const uint32_t q = rs[(size_t)best].from->c[bk], t = rs[(size_t)best].to[bk];
    const int r = std::clamp(chR(t) + chR(p) - chR(q), 0, 255), g = std::clamp(chG(t) + chG(p) - chG(q), 0, 255), b = std::clamp(chB(t) + chB(p) - chB(q), 0, 255);
    p = (p & 0xFF000000u) | (uint32_t)r | (uint32_t)g << 8 | (uint32_t)b << 16;
  }
}
std::vector<Remap> yardRemaps(const M& m) {
  return {{&kWood, m.wood}, {&kWoodDark, m.dark}, {&kThatch, m.roof}, {&kStone, m.stone}, {&kStoneWarm, m.stone}, {&kRed, m.cloth}};
}

}  // namespace

Canvas economyPropStyled(Prop p, const PartLook& L) {
  Canvas c = propSprite(p);
  if (L.arch < 0 || L.arch == A_HEART) return c;   // the heartland's yards are the classic ones
  std::vector<Remap> rs = yardRemaps(mOf(L));
  if (p == Prop::MineEntrance || p == Prop::MineHill) rs.erase(rs.begin() + 3, rs.begin() + 5);   // (the hill's own rock)
  remap(c, rs);
  return c;
}
Canvas wildPropStyled(Prop p, const PartLook& L) {
  if (p == Prop::GraveCairn && L.arch >= 0) return graveCairnSprite(L.arch, L.arch == A_HEART ? 0 : L.stone, L.cloth2);
  Canvas c = propSprite(p);
  if (L.arch < 0 || L.arch == A_HEART) return c;
  const M m = mOf(L);
  std::vector<Remap> rs = yardRemaps(m);
  if (p == Prop::TollPost) {   // the boom's stripes in the culture's two colours
    static const Ramp boomA = ramp5(rgba(206, 60, 52), rgba(232, 96, 76), rgba(206, 60, 52), rgba(206, 60, 52), rgba(232, 96, 76));
    (void)boomA;
    for (uint32_t& q : c.px) {
      if (!chA(q)) continue;
      const uint32_t o = q | 0xFF000000u;
      if (o == rgba(206, 60, 52)) q = m.cloth[2];
      else if (o == rgba(232, 96, 76)) q = m.cloth[3];
      else if (o == rgba(236, 228, 210)) q = m.cloth2[3];
      else if (o == kWhite) q = m.cloth2[4];
    }
  }
  remap(c, rs);
  return c;
}
Canvas marketTableStyled(int goods, int shade, bool closed, const PropStyle& st) {
  Canvas c = marketTable(goods, shade, closed);
  const PartLook L = partLook(st);
  if (L.arch < 0) return c;
  const M m = mOf(L);
  static const uint32_t shades[4] = {0, rgba(226, 214, 184), rgba(176, 62, 54), rgba(64, 120, 92)};
  const uint32_t sh = shades[((shade % 4) + 4) % 4];
  std::vector<Remap> rs = {{&kWood, m.wood}, {&kWoodDark, m.dark}};
  Ramp shR, trR;
  if (sh) {
    shR = ramp(sh, 0.75f);
    trR = ramp(shade == 1 ? rgba(176, 62, 54) : rgba(232, 222, 190), 0.7f);
    rs.push_back({&shR, ramp(st.awningA ? opaque(st.awningA) : L.cloth, 0.75f)});
    rs.push_back({&trR, ramp(st.awningB ? opaque(st.awningB) : L.cloth2, 0.7f)});
  }
  remap(c, rs);
  return c;
}
Canvas groundClothStyled(int goods, int cloth, bool closed, const PropStyle& st) {
  Canvas c = groundCloth(goods, cloth, closed);
  const PartLook L = partLook(st);
  if (L.arch < 0) return c;
  static const uint32_t cols[4] = {rgba(170, 56, 50), rgba(58, 74, 140), rgba(200, 150, 60), rgba(70, 120, 84)};
  const Ramp R = ramp(cols[((cloth % 4) + 4) % 4], 0.7f);
  // the culture's weave: its cloth, its accent, the two mixed by the cloth index so a row of cloths is not one colour
  const uint32_t to = (cloth & 1) ? L.cloth2 : L.cloth;
  remap(c, {{&R, ramp(mix(to, cols[((cloth % 4) + 4) % 4], 0.2f), 0.7f)}, {&kWood, ramp(opaque(L.wood))}});
  return c;
}
Canvas mineHillStyled(int variant, int land, const PropStyle& st) {
  Canvas c = mineHill(variant, land);
  const PartLook L = partLook(st);
  if (L.arch < 0) return c;
  const M m = mOf(L);
  remap(c, {{&kWood, m.wood}, {&kWoodDark, m.dark}});
  return c;
}

PartLook partLook(const PropStyle& st) {
  PartLook L;
  L.arch = (int)st.culture - 1;
  if (L.arch < 0 || L.arch >= 12) { L.arch = -1; return L; }
  const Base& B = kBase[L.arch];
  L.wood = B.wood; L.woodDark = B.woodDark; L.stone = B.stone; L.roof = B.roof; L.metal = B.metal;
  L.cloth = st.cloth ? mix(opaque(st.cloth), B.cloth, 0.35f) : B.cloth;
  L.cloth2 = st.awningA ? mix(opaque(st.awningA), B.cloth2, 0.35f) : B.cloth2;
  if (st.stone) L.stone = mix(B.stone, opaque(st.stone), 0.3f);
  return L;
}

bool builtPropStyled(Prop p) {
  switch (p) {
    case Prop::Signpost: case Prop::TollPost: case Prop::Tent: case Prop::MarketStall: case Prop::Banner: case Prop::Gravestone:
    case Prop::GraveCairn: case Prop::MarketCross: case Prop::DryingRack: case Prop::HideRack: case Prop::MineEntrance:
    case Prop::Trough: case Prop::WaterWheel: case Prop::PenShelter: case Prop::MineHill: case Prop::FishingShack: case Prop::HerbBed:
    case Prop::MarketTable: case Prop::GroundCloth: case Prop::StandingStone:
      return true;
    default: return false;
  }
}

bool builtPropSprite(Prop p, const PropStyle& st, Canvas& out) {
  if (st.classic() || !builtPropStyled(p)) return false;
  const PartLook L = partLook(st);
  if (L.arch < 0) return false;
  switch (p) {
    case Prop::TollPost: case Prop::GraveCairn: case Prop::FishingShack: case Prop::HerbBed: out = wildPropStyled(p, L); return true;
    case Prop::DryingRack: case Prop::HideRack: case Prop::MineEntrance: case Prop::Trough: case Prop::WaterWheel: case Prop::PenShelter:
    case Prop::MineHill:
      out = economyPropStyled(p, L);
      return true;
    case Prop::StandingStone: out = standingStoneVariant(0, bld::standingStoneParts(st.culture, 0)); return true;
    case Prop::MarketTable: {
      Canvas c(propW(p), propH(p));
      blit(c, marketTableStyled(0, 1, false, st), -4 + 16, -46 + 28);
      out = c;
      return true;
    }
    case Prop::GroundCloth: {
      Canvas c(propW(p), propH(p));
      blit(c, groundClothStyled(0, 0, false, st), -4 + 16, -14 + 4);
      out = c;
      return true;
    }
    default: break;
  }
  const M m = mOf(L);
  const int w = propW(p), h = propH(p), n = propFrames(p);
  Canvas sheet(w * n, h);
  for (int f = 0; f < n; f++) {
    Canvas cell(w, h);
    switch (p) {
      case Prop::Signpost: signpost(cell, m); break;
      case Prop::Tent: tentStyled(cell, m, st); break;
      case Prop::MarketStall: stallStyled(cell, m, st); break;
      case Prop::Banner: bannerStyled(cell, f, m, st); break;
      case Prop::Gravestone: gravestoneStyled(cell, m); break;
      case Prop::MarketCross: marketCrossStyled(cell, m); break;
      default: break;
    }
    outline(cell);
    place(sheet, cell, f, 0);
  }
  out = sheet;
  return true;
}

}  // namespace art
