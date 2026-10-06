// M3 culture-styled art (rpg/art/art_culture.h), architecture lane: heraldry (banners, shields, glyphs). The styled
// gatehouse lives with the walls (art_walls.cpp), the incremental building paint with the buildings (art_building.cpp),
// the culture-styled props in art_culture_props.cpp.
#include "rpg/art/art_internal.h"
#include "rpg/art/art_heraldry.h"

namespace art {

namespace {

// which tincture of a division a point of the field shows: 0 the field, 1 the second tincture (u, v in 0..1)
int division(int div, float u, float v) {
  switch (div) {
    case 1: return u >= 0.5f;                                     // per pale
    case 2: return v >= 0.5f;                                     // per fess
    case 3: return (u >= 0.5f) != (v >= 0.5f);                    // quarterly
    case 4: {                                                     // a chevron
      const float c = 0.78f - std::fabs(u - 0.5f) * 1.1f;
      return v >= c && v <= c + 0.26f;
    }
    case 5: return std::fabs(u - v) < 0.19f;                      // a bend
    case 6: return std::fabs(u - v) < 0.12f || std::fabs(u + v - 1) < 0.12f;   // a saltire
    case 7: return u < 0.16f || u > 0.84f || v < 0.12f || v > 0.9f;            // a bordure
    default: return 0;
  }
}

// the charge as a 7 x 7 mask: an emblem (art_heraldry.h) or a glyph (glyphSprite, one px a cell)
struct Charge {
  bool m[7][7] = {};
  bool at(int i, int j) const { return i >= 0 && j >= 0 && i < 7 && j < 7 && m[j][i]; }
};
Charge chargeOf(const cult::Heraldry& h) {
  Charge c;
  if (h.chargeKind == 1) {
    Canvas g = glyphSprite(h.glyphSeed, (int)(h.glyphSeed % 4), rgba(255, 255, 255), 1);
    // the glyph's strokes (not its dark outline) on the 7 x 7 grid
    for (int j = 0; j < 7; j++)
      for (int i = 0; i < 7; i++) {
        const uint32_t p = g.get(i, j);
        c.m[j][i] = chA(p) > 0 && chR(p) > 200;
      }
  } else
    for (int j = 0; j < 7; j++)
      for (int i = 0; i < 7; i++) c.m[j][i] = heraldry::chargeAt(h.emblem, 7, i, j);
  return c;
}
int chargeShadeOf(const Charge& c, int i, int j) {
  bool up = !c.at(i, j - 1), left = !c.at(i - 1, j), down = !c.at(i, j + 1), right = !c.at(i + 1, j);
  if (up || left) return 4;
  if (down || right) return 2;
  return 3;
}
uint32_t secondTincture(const cult::Heraldry& h) {
  if (h.field2) return h.field2;
  return h.charge ? h.charge : rgba(232, 200, 90);
}

}  // namespace

// A standing banner on a pole with the full arms: the plain banner's pole, crossbar, canvas, frames and ripple; the cloth
// cut to the banner's shape (hanging square, swallow-tail, pennant, gonfalon), divided, charged, its edges trimmed in
// the charge's colour, lit from the top-left and shaded in the folds of its ripple.
Canvas bannerSprite(const cult::Heraldry& h) {
  if (h.empty()) return propSprite(Prop::Banner);
  const int w = propW(Prop::Banner), hh = propH(Prop::Banner), n = propFrames(Prop::Banner);
  const Ramp F = ramp(opaque(h.field)), F2 = ramp(opaque(secondTincture(h))), T = ramp(opaque(h.charge ? h.charge : rgba(232, 200, 90)));
  const Charge ch = chargeOf(h);
  Canvas sheet(w * n, hh);
  for (int f = 0; f < n; f++) {
    Canvas c(w, hh);
    const int base = hh - 1;
    for (int y = 2; y <= base; y++) { c.set(2, y, kWood[3]); c.set(3, y, kWood[1]); }
    c.set(2, 1, kGold[4]); c.set(3, 1, kGold[2]); c.set(2, 0, kGold[3]);
    c.set(1, base, kWood[2]); c.set(4, base, kWood[1]);
    hline(c, 3, w - 2, 3, kWood[2]);
    c.set(w - 2, 3, kGold[3]);
    const int x0 = 4, x1 = w - 1, y0 = 4, y1 = 25;
    const int cw = x1 - x0;
    for (int y = y0; y < y1; y++) {
      const float t = (y - y0) / (float)(y1 - y0);
      const int off = (int)std::lround(std::sin(t * 4.0f + f * 1.6f) * 1.1f * t);
      for (int x = x0; x < x1; x++) {
        const int i = x - x0;
        // the shape of the fly
        bool cut = false;
        switch (h.shape) {
          case 1: { const int mid = (x0 + x1 - 1) / 2; cut = y > y1 - 6 && std::abs(x - mid) < (y - (y1 - 6)); break; }   // swallow-tail
          case 2: { const float half = cw * 0.5f * (1 - std::pow(t, 1.6f) * 0.92f); cut = std::fabs(i + 0.5f - cw * 0.5f) > half; break; }   // pennant
          case 3: { if (y > y1 - 6) { const int q = i % 3; cut = (y - (y1 - 6)) > (q == 1 ? 6 : (q == 0 ? 2 : 3)); } break; }   // gonfalon tails
          default: cut = y == y1 - 1 && (i == 0 || i == cw - 1); break;   // a hanging square, its corners eased
        }
        if (cut) continue;
        const float wave = std::sin(i * 0.9f + f * 1.6f + t * 2);
        int k = 2;
        if (wave > 0.5f) k = 3;
        if (wave < -0.5f) k = 1;
        if (x == x0) k = std::min(4, k + 1);
        const float u = (i + 0.5f) / cw, v = (y - y0 + 0.5f) / (float)(y1 - y0);
        uint32_t col = division(h.division, u, v) ? F2[k] : F[k];
        const bool edge = x == x0 || x == x1 - 1 || y == y0 || y == y0 + 1;
        if (edge && h.division != 7) col = T[y == y0 ? 3 : std::clamp(k, 1, 3)];
        const int ci = i - (cw - 7) / 2, cj = y - (y0 + 4);
        if (ch.at(ci, cj)) col = T[std::clamp(chargeShadeOf(ch, ci, cj) + (k - 2), 1, 4)];
        c.set(x + off, y, col);
      }
    }
    outline(c);
    place(sheet, c, f, 0);
  }
  return sheet;
}

// The arms on a heater shield, size x size: a gilded rim, the division and the charge scaled to the shield, lit from
// the top-left (a highlight across the upper left, the point in shade), a dark outline
Canvas shieldArms(const cult::Heraldry& h, int size) {
  size = std::clamp(size, 8, 32);
  Canvas c(size, size);
  const uint32_t fieldC = h.field ? h.field : rgba(150, 40, 40);
  const Ramp F = ramp(opaque(fieldC)), F2 = ramp(opaque(secondTincture(h))), T = ramp(opaque(h.charge ? h.charge : rgba(232, 200, 90)));
  const Charge ch = chargeOf(h);
  const float W = (float)size - 2, Hh = (float)size - 1;
  auto inside = [&](float x, float y) {   // the heater: a slightly dished top, straight sides, curved flanks to the point
    if (y < 0.5f || y > Hh) return false;
    const float u = (x - 1) / W;            // 0..1 across
    if (u < 0 || u > 1) return false;
    const float v = y / Hh;
    if (v < 0.05f + 0.04f * std::fabs(u - 0.5f) * 2) return false;
    if (v < 0.55f) return true;
    const float t = (v - 0.55f) / 0.45f;   // 0..1 down the flanks
    const float half = 0.5f * std::sqrt(std::max(0.0f, 1 - t * t * 0.98f));
    return std::fabs(u - 0.5f) <= half;
  };
  const int s = std::max(1, (size - 4) / 9);   // charge px per cell
  const int cx0 = (size - 7 * s) / 2, cy0 = (int)std::lround(size * 0.22f);
  for (int y = 0; y < size; y++)
    for (int x = 0; x < size; x++) {
      if (!inside(x + 0.5f, y + 0.5f)) continue;
      const bool rim = !inside(x - 0.5f, y + 0.5f) || !inside(x + 1.5f, y + 0.5f) || !inside(x + 0.5f, y - 0.5f) || !inside(x + 0.5f, y + 1.5f);
      const float u = (x + 0.5f - 1) / W, v = (y + 0.5f) / Hh;
      // light: across the face from the upper left, a soft highlight band, the point in shade
      const float l = (0.5f - u) * 0.9f + (0.4f - v) * 0.8f;
      int k = l > 0.35f ? 3 : (l > -0.25f ? 2 : 1);
      if (rim) { c.set(x, y, kGold[(u < 0.5f && v < 0.6f) ? 4 : (u > 0.6f || v > 0.8f ? 1 : 2)]); continue; }
      uint32_t col = division(h.division, u, v) ? F2[k] : F[k];
      const int ci = (x - cx0) / s, cj = (y - cy0) / s;
      if (x >= cx0 && y >= cy0 && ch.at(ci, cj)) col = T[std::clamp(chargeShadeOf(ch, ci, cj) - (k < 2 ? 1 : 0), 1, 4)];
      c.set(x, y, col);
    }
  outline(c, 0.9f);
  return c;
}

// A glyph: strokes between the nodes of a 5 x 5 grid, mirrored left to right (and sometimes top to bottom), in one of
// four styles; drawn at `cell` px a grid step in col with a darker outline. 0 runic: straight strokes along the grid and
// its diagonals, a stave down the middle; 1 flowing: quarter-circle arcs between nodes; 2 geometric: rings, dots and a
// cross; 3 knotwork: an interlaced loop with over-under breaks.
Canvas glyphSprite(uint32_t seed, int style, uint32_t col, int cell) {
  cell = std::max(1, cell);
  const int n = 5 * cell + 2;
  Canvas c(n, n);
  uint32_t hs = seed * 2654435761u + 0x9E3779B9u;
  auto rnd = [&](int m) { hs ^= hs << 13; hs ^= hs >> 17; hs ^= hs << 5; return (int)(hs % (uint32_t)m); };
  // the strokes on a 5 x 5 boolean grid of cells (at cell size 1 the grid is the picture)
  bool g[5][5] = {};
  auto mark = [&](int x, int y) { if (x >= 0 && y >= 0 && x < 5 && y < 5) { g[y][x] = true; g[y][4 - x] = true; } };
  auto stroke = [&](int xa, int ya, int xb, int yb) {
    const int steps = std::max(std::abs(xb - xa), std::abs(yb - ya));
    for (int k = 0; k <= steps; k++) mark(xa + (steps ? (xb - xa) * k / steps : 0), ya + (steps ? (yb - ya) * k / steps : 0));
  };
  style = ((style % 4) + 4) % 4;
  const bool vmirror = rnd(3) == 0;
  switch (style) {
    case 0: {   // runic: a stave and two or three twigs
      if (rnd(4) != 0) stroke(2, 0, 2, 4);
      const int twigs = 2 + rnd(2);
      for (int k = 0; k < twigs; k++) {
        const int y = rnd(5), y2 = std::clamp(y + rnd(3) - 1, 0, 4);
        stroke(2, y, rnd(2), y2);
      }
      break;
    }
    case 1: {   // flowing: a hook or an S of arcs
      const int r = 1 + rnd(2);
      for (int a = 0; a <= 8; a++) {
        const float t = a / 8.0f * 3.14159f;
        mark(2 - (int)std::lround(std::sin(t) * r), (int)std::lround(2 - std::cos(t) * 2));
      }
      if (rnd(2)) stroke(0, 4, 2, 4);
      if (rnd(2)) mark(2, 2);
      break;
    }
    case 2: {   // geometric: a ring, a dot in it, rays
      for (int a = 0; a < 12; a++) {
        const float t = a / 12.0f * 6.2832f;
        mark(2 + (int)std::lround(std::cos(t) * 1.6f), 2 + (int)std::lround(std::sin(t) * 1.6f));
      }
      if (rnd(2)) mark(2, 2);
      if (rnd(2)) { mark(2, 0); mark(2, 4); mark(0, 2); }
      break;
    }
    default: {   // knotwork: a square loop crossed by a diagonal pair, broken where one passes under
      stroke(1, 0, 1, 4); stroke(0, 1, 4, 1); stroke(0, 3, 4, 3);
      if (rnd(2)) stroke(0, 0, 2, 2);
      break;
    }
  }
  if (vmirror)
    for (int y = 0; y < 2; y++)
      for (int x = 0; x < 5; x++) g[4 - y][x] = g[y][x] = g[y][x] || g[4 - y][x];
  // a glyph needs body: at least five cells
  int cnt = 0;
  for (auto& r : g) for (bool b : r) cnt += b;
  if (cnt < 5) { stroke(2, 0, 2, 4); stroke(1, 1, 2, 2); }
  const uint32_t dark = mix(shade(opaque(col), 0.45f), kInk, 0.4f);
  for (int y = 0; y < 5; y++)
    for (int x = 0; x < 5; x++) {
      if (!g[y][x]) continue;
      for (int yy = 0; yy < cell; yy++)
        for (int xx = 0; xx < cell; xx++) {
          // knotwork: a break where the stroke passes under (a darker pixel at the crossing)
          const bool under = style == 3 && cell > 1 && ((x + y) & 1) && xx == 0 && yy == 0 && g[y][(x + 1) % 5];
          c.set(1 + x * cell + xx, 1 + y * cell + yy, under ? dark : col);
        }
    }
  // the outline (darker), only outside the strokes
  Canvas src = c;
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++) {
      if (chA(src.get(x, y))) continue;
      bool near = false;
      for (int k = 0; k < 4 && !near; k++) {
        static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        near = chA(src.get(x + dx[k], y + dy[k])) && src.get(x + dx[k], y + dy[k]) == opaque(col);
      }
      if (near && cell > 1) c.set(x, y, dark);
    }
  return c;
}

}  // namespace art
