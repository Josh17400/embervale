// Relief gallery (M11 FALLS lane): the summit props (the shrine in its frames, cairns, prayer flags fluttering) and the
// peaks in every shape and land (grey rock, snow, sandstone, the Wildlands' rocks), the great peaks as a range, on an
// alpine board, scaled up for review.
//   relief_gallery <outDir> [props|peaks|range|all]   (default all)
// writes relief_props.png, relief_peaks.png, relief_range.png
#include "tools/preview/preview_util.h"

#include "rpg/art/art_relief.h"

namespace {

// an alpine board: short grass going to scree and snow (roughly the game's high ground)
Canvas alpineBoard(int w, int h, int kind) {
  Canvas c(w, h);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      const uint32_t n = hash2(x / 2, y / 2) % 100, t = hash2(x, y * 7 + 3) % 300;
      uint32_t col;
      if (kind == 1) { col = rgba(206, 220, 236); if (n < 20) col = rgba(190, 206, 228); else if (n > 92) col = rgba(226, 236, 246); }
      else if (kind == 2) { col = rgba(196, 150, 104); if (n < 20) col = rgba(184, 138, 94); else if (n > 92) col = rgba(212, 168, 120); }
      else { col = rgba(108, 140, 76); if (n < 20) col = rgba(98, 128, 72); else if (n > 90) col = rgba(124, 150, 86); if (t == 0) col = rgba(150, 146, 140); }
      c.set(x, y, col);
    }
  return c;
}
void put(Canvas& dst, const Canvas& s, int x, int y, int sx = 0, int sw = -1) {
  if (sw < 0) sw = s.w;
  for (int j = 0; j < s.h; j++)
    for (int i = 0; i < sw; i++) {
      const uint32_t p = s.get(sx + i, j);
      const int a = (int)(p >> 24);
      if (!a) continue;
      if (a == 255) { dst.set(x + i, y + j, p); continue; }
      dst.set(x + i, y + j, art::mix(dst.get(x + i, y + j), p | 0xFF000000u, a / 255.0f));
    }
}
// a soft ground shadow (as the view's propShadow 6: footprint-wide, offset to the lower right)
void shadow(Canvas& dst, int cx, int by, int rw, int rh) {
  for (int y = -rh; y <= rh; y++)
    for (int x = -rw; x <= rw; x++) {
      const float d = (x * x) / (float)(rw * rw) + (y * y) / (float)(rh * rh);
      if (d > 1) continue;
      const int px = cx + x + 2, py = by + y;
      dst.set(px, py, art::mix(dst.get(px, py), rgba(30, 26, 60), 0.28f));
    }
}
void label(Canvas& c, int x, int y, const std::string& s, uint32_t col = rgba(255, 250, 230)) {
  for (char ch : s) {
    const int gi = pv::font5x7::glyphIndex(ch);
    if (gi >= 0)
      for (int r = 0; r < 7; r++)
        for (int k = 0; k < 5; k++)
          if (pv::font5x7::kFont[gi].rows[r] & (0x10 >> k)) { c.set(x + k + 1, y + r + 1, rgba(16, 20, 34)); c.set(x + k, y + r, col); }
    x += 6;
  }
}

void props(const std::string& dir) {
  Canvas B = alpineBoard(420, 250, 0);
  // the shrine in each frame, on the board, then by night (the braziers' glow)
  const Canvas sh = art::propSprite(art::Prop::SummitShrine);
  const int sw = art::propW(art::Prop::SummitShrine), shh = art::propH(art::Prop::SummitShrine), sn = art::propFrames(art::Prop::SummitShrine);
  for (int f = 0; f < sn; f++) {
    shadow(B, 10 + f * 62 + sw / 2, 20 + shh - 10, sw / 2, 10);
    put(B, sh, 10 + f * 62, 20, f * sw, sw);
  }
  label(B, 10, 6, "SUMMIT SHRINE (FRAMES)");
  // cairns: a few side by side, and the prayer flags in their frames
  const Canvas ca = art::propSprite(art::Prop::Cairn);
  for (int i = 0; i < 4; i++) { shadow(B, 14 + i * 22 + ca.w / 2, 120 + ca.h - 2, 6, 3); put(B, ca, 14 + i * 22, 120); }
  label(B, 10, 106, "CAIRNS");
  const Canvas pf = art::propSprite(art::Prop::PrayerFlags);
  const int pw = art::propW(art::Prop::PrayerFlags), pn = art::propFrames(art::Prop::PrayerFlags);
  for (int f = 0; f < pn; f++) put(B, pf, 120 + f * (pw + 8), 116, f * pw, pw);
  label(B, 120, 106, "PRAYER FLAGS (FRAMES)");
  // on snow and on sandstone
  Canvas S = alpineBoard(420, 80, 1), D = alpineBoard(420, 80, 2);
  put(S, sh, 8, 14, 0, sw); put(S, ca, 70, 40); put(S, pf, 92, 30, 0, pw);
  put(D, sh, 8, 14, 0, sw); put(D, ca, 70, 40); put(D, pf, 92, 30, 0, pw);
  put(B, S, 0, 170, 0, 210);
  put(B, D, 210, 170, 0, 210);
  label(B, 10, 172, "ON SNOW");
  label(B, 220, 172, "ON SAND");
  savePng(B, dir + "/relief_props.png", 4);
}

void peaks(const std::string& dir) {
  const int lands[4] = {0, 1, 2, 3};
  const char* names[4] = {"GREY ROCK", "SNOW", "SANDSTONE", "BADLANDS"};
  const int pw = art::propW(art::Prop::Peak), ph = art::propH(art::Prop::Peak);
  Canvas B(8 * (pw + 4) + 8, 4 * (ph + 14) + 8);
  for (int r = 0; r < 4; r++) {
    Canvas row = alpineBoard(B.w, ph + 14, lands[r] == 1 ? 1 : lands[r] >= 2 ? 2 : 0);
    for (int v = 0; v < 8; v++) {
      const Canvas k = art::peakVariant(v, lands[r]);
      shadow(row, 4 + v * (pw + 4) + pw / 2, 10 + ph - 6, pw / 2 - 2, 7);
      put(row, k, 4 + v * (pw + 4), 10);
    }
    label(row, 4, 1, names[r]);
    put(B, row, 0, 4 + r * (ph + 14));
  }
  savePng(B, dir + "/relief_peaks.png", 3);
}

// a range: great peaks and peaks in a row along a crest, overlapping as the view sorts them (by their base)
void range(const std::string& dir) {
  for (int land = 0; land <= 1; land++) {
    Canvas B = alpineBoard(420, 220, land);
    struct P { int v, x, y; };
    const P ps[] = {{3, 10, 70}, {12, 60, 20}, {5, 150, 78}, {1, 200, 60}, {9, 250, 30}, {6, 350, 84}, {2, 110, 116}, {7, 300, 120}};
    std::vector<P> order(std::begin(ps), std::end(ps));
    auto base = [](const P& p) { return p.y + (p.v >= 8 ? art::propH(art::Prop::GreatPeak) : art::propH(art::Prop::Peak)); };
    std::sort(order.begin(), order.end(), [&](const P& a, const P& b) { return base(a) < base(b); });
    for (const P& p : order) {
      const Canvas k = art::peakVariant(p.v, land);
      shadow(B, p.x + k.w / 2, p.y + k.h - 6, k.w / 2 - 4, 8);
      put(B, k, p.x, p.y);
    }
    label(B, 4, 4, land ? "A RANGE IN THE SNOW" : "A RANGE OF GREY ROCK");
    savePng(B, dir + (land ? "/relief_range_snow.png" : "/relief_range.png"), 3);
  }
}
// ---- the falls over a mock step (the game draws the face from the terrain; here a plain rock face 12 px a level, a
//      river above, a pool below), every kind, then a frame strip
struct FallScene { const char* name; int dir; int drop; int tiles; bool frozen; };
void mockStep(Canvas& B, int px, int py, int pw, int ph, const FallScene& s, int contour, int rx0, int rx1) {
  const bool snow = s.frozen;
  const uint32_t gr = snow ? rgba(214, 226, 240) : rgba(108, 140, 76), gr2 = snow ? rgba(196, 210, 230) : rgba(96, 128, 70);
  const uint32_t wa = snow ? rgba(150, 190, 226) : rgba(50, 108, 163), wa2 = snow ? rgba(176, 208, 236) : rgba(80, 135, 178);
  const int fh = 12 * s.drop;
  for (int y = py; y < py + ph; y++)
    for (int x = px; x < px + pw; x++) {
      uint32_t col = ((hash2(x / 2, y / 2) % 9) == 0) ? gr2 : gr;
      if (s.dir == 0) {
        if (y < contour) { if (x >= rx0 && x < rx1) col = ((x + y * 3) % 11 == 0) ? wa2 : wa; }
        else if (y < contour + fh) {   // the face: rock columns lit on their left
          const int cc = (x - px) % 6;
          col = cc == 0 ? rgba(170, 160, 156) : cc == 5 ? rgba(70, 66, 88) : (y - contour < 2 ? rgba(176, 168, 160) : rgba(118, 112, 124));
          if (x >= rx0 - 1 && x <= rx1) col = art::mix(col, rgba(30, 40, 70), 0.45f);   // the wet face
        } else if (x >= rx0 - 10 && x < rx1 + 10 && y < contour + fh + 30) col = ((x + y * 3) % 11 == 0) ? wa2 : wa;
        else if (y == contour + fh || y == contour + fh + 1) col = art::mix(col, rgba(40, 30, 80), 0.4f);
      } else if (s.dir == 3) {   // the higher ground to the south: its rim's lip 6 px above the step
        if (y >= contour) { if (x >= rx0 && x < rx1) col = wa; else col = art::shade(gr, 1.1f); }
        else if (y >= contour - 6) col = (x >= rx0 && x < rx1) ? wa : art::shade(gr, 1.1f);
        else if (y == contour - 7) col = rgba(196, 190, 176);
        else if (y >= contour - 12) col = (x >= rx0 && x < rx1) ? wa : rgba(84, 80, 100);
        else if (x >= rx0 - 4 && x < rx1 + 4) col = wa;
      } else {   // a side cascade from the west: the river runs east and drops at x = contour (a rock wall 6 px)
        const int SW = 6 + 3 * (s.drop - 1);
        const bool riverRow = y >= rx0 && y < rx1;
        if (x < contour) { if (riverRow) col = wa; }
        else if (x < contour + SW) col = riverRow ? wa : (x == contour ? rgba(84, 80, 100) : rgba(110, 104, 120));
        else if (y >= rx0 - 6 && y < rx1 + 6) col = wa;
      }
      B.set(x, y, col);
    }
}
void falls(const std::string& dir) {
  const FallScene sc[6] = {{"ONE TILE", 0, 1, 1, false}, {"WIDE", 0, 1, 3, false}, {"TALL (2 LEVELS)", 0, 2, 2, false},
                           {"FROZEN", 0, 2, 2, true}, {"SIDE CASCADE", 1, 1, 2, false}, {"OVER THE RIM", 3, 1, 2, false}};
  const int PW = 96, PH = 92;
  Canvas B(3 * PW + 16, 2 * PH + 16 + 70);
  for (int i = 0; i < 6; i++) {
    const FallScene& s = sc[i];
    const int px = 4 + (i % 3) * (PW + 4), py = 4 + (i / 3) * (PH + 4);
    const int contour = py + (s.dir == 3 ? 52 : 34);
    const int tx0 = (px + PW / 2 - s.tiles * 8) / 16;   // the fall's tiles (16 px, the board is the window)
    const int rx0 = s.dir == 1 ? py + 30 : tx0 * 16 + 2, rx1 = s.dir == 1 ? py + 30 + s.tiles * 16 - 4 : (tx0 + s.tiles) * 16 - 2;
    const int cx = px + PW / 2 - 4;
    mockStep(B, px, py, PW, PH, s, s.dir == 1 ? cx : contour, rx0, rx1);
    Canvas c(PW * (s.frozen ? 1 : art::FALL_FRAMES), PH);
    for (int f = 0; f < (s.frozen ? 1 : art::FALL_FRAMES); f++)
      for (int t = 0; t < (s.dir == 1 ? 1 + s.tiles : s.tiles); t++) {
        art::FallSpan sp[22];
        if (s.dir == 1) {
          const int y0 = ((py + 30) / 16 + t) * 16;
          for (int j = -3; j <= 18; j++) { sp[j + 3].at = (int16_t)cx; sp[j + 3].wet = y0 + j >= rx0 && y0 + j < rx1; }
          art::paintFallSide(c, f, px, py, y0, sp, 1, s.drop, s.frozen, 0, 0);
        } else {
          const int x0 = (tx0 + t) * 16;
          for (int k = -3; k <= 18; k++) { sp[k + 3].at = (int16_t)contour; sp[k + 3].wet = x0 + k >= rx0 && x0 + k < rx1; }
          if (s.dir == 0) art::paintFallNorth(c, f, px, py, x0, sp, 12 * s.drop, s.drop, s.frozen, 0, 0);
          else art::paintFallRim(c, f, px, py, x0, sp, s.frozen, 0, 0);
        }
      }
    put(B, c, px, py, 0, PW);
    label(B, px + 2, py + PH - 9, s.name);
  }
  // a frame strip of the tall fall (its sheet only, the 6 frames side by side)
  {
    const FallScene& s = sc[2];
    const int py = 2 * PH + 16, pw = 50;
    for (int f = 0; f < art::FALL_FRAMES; f++) {
      const int px = 4 + f * (pw + 2);
      const int contour = py + 10, x0 = ((px + 9) / 16) * 16;
      mockStep(B, px, py, pw, 64, s, contour, x0 + 2, x0 + 30);
      Canvas c(pw * art::FALL_FRAMES, 64);
      for (int t = 0; t < 2; t++) {
        art::FallSpan sp[22];
        for (int k = -3; k <= 18; k++) { sp[k + 3].at = (int16_t)contour; sp[k + 3].wet = x0 + t * 16 + k >= x0 + 2 && x0 + t * 16 + k < x0 + 30; }
        art::paintFallNorth(c, f, px, py, x0 + t * 16, sp, 24, 2, false, 0, 0);
      }
      put(B, c, px, py, f * pw, pw);
    }
  }
  savePng(B, dir + "/relief_falls.png", 4);
}
}  // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  const std::string what = argc > 2 ? argv[2] : "all";
  if (what == "props" || what == "all") props(dir);
  if (what == "peaks" || what == "all") peaks(dir);
  if (what == "range" || what == "all") range(dir);
  if (what == "falls" || what == "all") falls(dir);
  return 0;
}
