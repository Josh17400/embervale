// M12 "Rise and Fall": the rise props and the view's rise pieces (rpg/art/art_rise.h). VIEW lane.
//   LordBanner     a tall standard: a turned pole with a gilded finial on a stepped stone plinth, a crossbar with gilt
//                  caps, a long swallow-tailed banner hanging in front of the pole (neutral linen: the view swaps in
//                  lordBannerSprite in its owner's arms), waving over 4 frames, a gold fringe at its hem
//   Watchtower     a timber tower on four splayed, cross-braced legs, a plank platform with a rail, a shingled hip roof
//                  on four posts, a ladder, a hanging lantern; painted as a height field (every ground point at its
//                  height lands on row y - z, as the building painter projects), lit from the top-left
//   rankBadge, warlordStandard, trophyPike, resettlerStores, clawScar: the pieces the view draws (art_rise.h)
// Shared language (art_internal.h): hue-shifted ramps, top-left light, a selective outline, soft cast shadows thrown to
// the lower right, grass at the foot of standing things.
#include <algorithm>
#include <cmath>
#include "rpg/art/art_internal.h"
#include "rpg/art/art_culture.h"
#include "rpg/art/art_heraldry.h"
#include "rpg/art/art_rise.h"

namespace art {

namespace {
struct Info { int w, h, frames; };
Info riseInfo(Prop p) {
  switch (p) {
    case Prop::LordBanner: return {20, 48, 4};
    case Prop::Watchtower: return {36, 78, 1};
    default: return {16, 16, 1};
  }
}

const Ramp kRiseCloth = ramp5(rgba(92, 80, 84), rgba(150, 136, 128), rgba(198, 184, 160), rgba(226, 214, 188), rgba(246, 238, 214));
const Ramp kRiseTrim = ramp5(rgba(70, 30, 36), rgba(112, 42, 40), rgba(150, 60, 46), rgba(186, 92, 62), rgba(220, 140, 96));
const Ramp kPlinth = ramp5(rgba(56, 52, 70), rgba(92, 90, 106), rgba(132, 128, 136), rgba(172, 168, 166), rgba(208, 204, 194));
const Ramp kShingle = ramp5(rgba(44, 30, 40), rgba(70, 46, 46), rgba(100, 66, 52), rgba(134, 92, 64), rgba(166, 122, 82));
const Ramp kSoilR = ramp5(rgba(44, 28, 34), rgba(72, 46, 40), rgba(102, 70, 50), rgba(134, 98, 66), rgba(166, 128, 88));

// which tincture of a division a point of the field shows (the heraldry painter's divisions: art_culture.cpp)
int divisionAt(int div, float u, float v) {
  switch (div) {
    case 1: return u >= 0.5f;
    case 2: return v >= 0.5f;
    case 3: return (u >= 0.5f) != (v >= 0.5f);
    case 4: { const float c = 0.78f - std::fabs(u - 0.5f) * 1.1f; return v >= c && v <= c + 0.26f; }
    case 5: return std::fabs(u - v) < 0.19f;
    case 6: return std::fabs(u - v) < 0.12f || std::fabs(u + v - 1) < 0.12f;
    case 7: return u < 0.16f || u > 0.84f || v < 0.12f || v > 0.9f;
    default: return 0;
  }
}
// the charge as a 7 x 7 mask (an emblem, or a glyph one px a cell)
struct Mask7 {
  bool m[7][7] = {};
  bool at(int i, int j) const { return i >= 0 && j >= 0 && i < 7 && j < 7 && m[j][i]; }
  bool any() const { for (auto& r : m) for (bool b : r) if (b) return true; return false; }
};
Mask7 chargeMask(const cult::Heraldry& h) {
  Mask7 c;
  if (h.chargeKind == 1) {
    Canvas g = glyphSprite(h.glyphSeed, (int)(h.glyphSeed % 4), rgba(255, 255, 255), 1);
    for (int j = 0; j < 7; j++)
      for (int i = 0; i < 7; i++) { const uint32_t p = g.get(i, j); c.m[j][i] = chA(p) > 0 && chR(p) > 200; }
  } else
    for (int j = 0; j < 7; j++)
      for (int i = 0; i < 7; i++) c.m[j][i] = heraldry::chargeAt(h.emblem, 7, i, j);
  return c;
}
int maskShade(const Mask7& c, int i, int j) {
  if (!c.at(i, j - 1) || !c.at(i - 1, j)) return 4;
  if (!c.at(i, j + 1) || !c.at(i + 1, j)) return 2;
  return 3;
}

// the soft cast shadow on the ground (only where nothing stands; the rim dithered so it melts into the grass)
void castShadow(Canvas& c, const std::vector<Vec2>& pts, int alpha) {
  Canvas s(c.w, c.h);
  poly(s, pts, rgba(255, 255, 255));
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      if (!solid(s, x, y) || solid(c, x, y)) continue;
      int n = 0;
      for (int k = 0; k < 4; k++) n += solid(s, x + (k == 0) - (k == 1), y + (k == 2) - (k == 3)) ? 1 : 0;
      const int a = n < 4 ? (bayer(x, y) < 0.5f ? alpha / 2 : 0) : alpha;
      if (a) c.set(x, y, withA(kShadowHue, a));
    }
}
// grass blades at the foot of a thing
void tuftsAt(Canvas& c, int x0, int x1, int y, uint32_t seed, float density = 0.45f) {
  for (int x = x0; x <= x1; x++) {
    if (hashf(x, y, seed) > density || solid(c, x, y)) continue;
    const int h = 1 + (int)(hash3(x, 1, seed) % 3);
    for (int j = 0; j < h; j++) c.set(x + ((j == h - 1 && (x & 1)) ? 1 : 0), y - j, kLeaf[j == h - 1 ? 3 : (j == 0 ? 1 : 2)]);
  }
}

// ---------------------------------------------------------------- the lord's standard
// One frame of the standard: the plinth, the pole, the crossbar and the banner (field F / F2 by `div`, trim T, the
// charge mask when given). The cloth hangs in front of the pole from the crossbar and sways at its tail.
void standardFrame(Canvas& c, int f, const Ramp& F, const Ramp& F2, const Ramp& T, int div, const Mask7* charge, uint32_t seed) {
  const int W = c.w, H = c.h, px = W / 2 - 1;   // the pole: px lit, px + 1 shaded
  const int base = H - 1;
  Canvas body(W, H);
  // the stepped stone plinth (3/4: a lit top, a mid-tone south face, the east end in shade)
  {
    const int y0 = base - 5;
    for (int x = px - 4; x <= px + 5; x++) {
      const bool east = x >= px + 4;
      body.set(x, y0, kPlinth[x <= px - 3 ? 4 : 3]);                // the top's front lip
      body.set(x, y0 + 1, kPlinth[east ? 2 : 3]);
      for (int y = y0 + 2; y <= base - 1; y++) body.set(x, y, kPlinth[east ? 1 : (y == base - 1 ? 1 : 2)]);
    }
    for (int x = px - 2; x <= px + 3; x++) { body.set(x, y0 - 2, kPlinth[x <= px ? 4 : 3]); body.set(x, y0 - 1, kPlinth[x >= px + 3 ? 1 : 2]); }   // the upper step
    body.set(px - 4, base - 1, kPlinth[1]); body.set(px + 5, base - 1, kPlinth[0]);
    for (int x = px - 3; x <= px + 4; x += 3) body.set(x, y0 + 3, kPlinth[1]);   // mortar joints on the face
  }
  // the pole: turned wood, lit on its west edge; iron bands at the foot and under the crossbar
  const int poleTop = 3;
  for (int y = poleTop; y <= base - 7; y++) { body.set(px, y, kWood[(y % 7 == 0) ? 3 : 3]); body.set(px + 1, y, kWood[1]); }
  body.set(px, base - 8, kIron[3]); body.set(px + 1, base - 8, kIron[1]);
  body.set(px, 9, kIron[3]); body.set(px + 1, 9, kIron[1]);
  // the finial: a gilded spear-point over a ball
  body.set(px, 0, kGold[4]); body.set(px + 1, 0, kGold[2]);
  body.set(px, 1, kGold[4]); body.set(px + 1, 1, kGold[2]);
  body.set(px - 1, 2, kGold[3]); body.set(px, 2, kGold[4]); body.set(px + 1, 2, kGold[2]); body.set(px + 2, 2, kGold[1]);
  // the crossbar with gilt end caps (it carries the banner)
  const int cy = 5, cx0 = px - 7, cx1 = px + 8;
  for (int x = cx0; x <= cx1; x++) { body.set(x, cy, kWood[x < px ? 4 : 3]); body.set(x, cy + 1, kWood[1]); }
  body.set(cx0 - 1, cy, kGold[4]); body.set(cx0 - 1, cy + 1, kGold[2]);
  body.set(cx1 + 1, cy, kGold[3]); body.set(cx1 + 1, cy + 1, kGold[1]);
  // the banner: rows y0..y1 hanging from the crossbar, x0..x1 wide; the tail sways with the frame
  const int x0 = px - 6, x1 = px + 8, y0 = cy + 2, y1 = 35;
  const int cw = x1 - x0, ch = y1 - y0;
  const float ph = f * 1.5708f + (float)(seed % 5) * 0.3f;
  Canvas cloth(W, H);
  for (int y = y0; y < y1; y++) {
    const float t = (float)(y - y0) / (float)ch;
    const int off = (int)std::lround(std::sin(t * 2.6f + ph) * 2.0f * t * t);   // the tail swings, the head hangs still
    for (int x = x0; x < x1; x++) {
      const int i = x - x0;
      // the swallow tail: a notch up the middle of the last 7 rows
      const int mid2 = cw - 1;   // (centre * 2)
      if (y >= y1 - 7 && std::abs(2 * i - mid2) < 2 * (y - (y1 - 8))) continue;
      // the folds ripple across the cloth and travel with the frame; the lit west edge, the shaded east edge
      const float wave = std::sin(i * 0.95f - ph * 1.2f + t * 2.4f);
      int k = 2;
      if (wave > 0.45f) k = 3;
      if (wave < -0.45f) k = 1;
      if (i == 0) k = std::min(4, k + 1);
      if (i == cw - 1) k = std::max(0, k - 1);
      if (t > 0.75f && wave < 0) k = std::max(0, k - 1);    // the tail twists into its own shade
      const float u = (i + 0.5f) / cw, v = (y - y0 + 0.5f) / (float)ch;
      uint32_t col = divisionAt(div, u, v) ? F2[k] : F[k];
      // the hem: a trim band across the head, the trim down both edges
      if (y <= y0 + 1) col = T[y == y0 ? 3 : 2];
      else if (div != 7 && (i == 0 || i == cw - 1)) col = T[std::clamp(k, 1, 3)];
      // the charge, centred in the upper half
      if (charge) {
        const int ci = i - (cw - 7) / 2, cj = y - (y0 + 5);
        if (charge->at(ci, cj)) col = T[std::clamp(maskShade(*charge, ci, cj) + (k - 2), 1, 4)];
      }
      cloth.set(x + off, y, col);
    }
    // a gold fringe under the lowest rows of the tails
    if (y == y1 - 1)
      for (int x = x0; x < x1; x++) {
        const int i = x - x0;
        if (std::abs(2 * i - (cw - 1)) < 2 * (y - (y1 - 8))) continue;
        if (((x + f) & 1) == 0) cloth.set(x + off, y + 1, kGold[3]);
      }
  }
  // the banner's shade on the pole below it (the cloth hangs in front) and its outline
  outline(cloth);
  outline(body);
  blit(body, cloth, 0, 0);
  // the cast shadow of the plinth and the hanging banner's foot on the ground (to the lower right)
  castShadow(c, {V(px + 5, base), V(px + 10, base - 3), V(W - 1, base - 3), V(W - 1, base), V(px + 6, base + 1)}, 60);
  blit(c, body, 0, 0);
  tuftsAt(c, px - 6, px + 7, base, seed + 7, 0.35f);
}

void lordBanner(Canvas& c, int f) {
  standardFrame(c, f, kRiseCloth, kRiseCloth, kRiseTrim, 0, nullptr, 3);
}

// ---------------------------------------------------------------- the watchtower
// A height field: every ground point (gx, gy) at height z lands on row gy - z. The tower stands bottom-centred on its
// tile (x W / 2, ground row H - 1): four splayed legs, a plank deck at height 34 walled by a waist-high plank parapet, a
// dark lookout under the eaves (a lantern in it), a steep shingled pyramid roof (eaves at 48, the peak at 64).
void watchtower(Canvas& c) {
  const int W = c.w, H = c.h, cxI = W / 2;
  const float cx = (float)cxI;
  const int base = H - 1;
  Canvas body(W, H);
  const float zDeck = 34, zPar = 41, zEave = 48, zPeak = 64;
  const float gF = base - 2.0f, gB = base - 10.0f;          // ground rows of the front and back feet
  const float dF = base - 4.0f, dB = base - 12.0f;          // the deck's front and back edges on the ground
  const float dx0 = cx - 10, dx1 = cx + 9;                  // the deck's west and east edges
  auto row = [](float gy, float z) { return (int)std::floor(gy - z); };
  // ---- the legs: round poles, wide at the foot, gathered under the deck
  struct Leg { float fx, fy, tx, ty; bool back, west; };
  const Leg legs[4] = {{cx - 11.5f, gB, dx0 + 2.0f, dB, true, true}, {cx + 10.5f, gB, dx1 - 2.0f, dB, true, false},
                       {cx - 14.0f, gF, dx0 + 1.0f, dF, false, true}, {cx + 13.0f, gF, dx1 - 1.0f, dF, false, false}};
  auto legAt = [&](const Leg& l, float t, int& x, int& y) {   // t 0 at the foot .. 1 at the deck
    x = (int)std::lround(l.fx + (l.tx - l.fx) * t - 0.5f);
    y = (int)std::lround(l.fy + (l.ty - zDeck - l.fy) * t);
  };
  auto legLine = [&](const Leg& l) {
    const int n = (int)std::ceil(zDeck + 2);
    for (int s = 0; s <= n; s++) {
      int x, y;
      legAt(l, (float)s / n, x, y);
      const Ramp& R = l.back ? kWoodDark : kWood;
      body.set(x, y, R[l.west ? 3 : 2]);
      body.set(x + 1, y, R[l.west ? 2 : 1]);
    }
    int x, y;
    legAt(l, 0, x, y);
    body.set(x - 1, y + 1, kPlinth[3]); body.set(x, y + 1, kPlinth[4]); body.set(x + 1, y + 1, kPlinth[2]); body.set(x + 2, y + 1, kPlinth[1]);
    body.set(x, y + 2, kPlinth[1]); body.set(x + 1, y + 2, kPlinth[0]);
  };
  auto tie = [&](const Leg& a, const Leg& b, float t, const Ramp& R, int k) {   // a horizontal tie between two legs
    int xa, ya, xb, yb;
    legAt(a, t, xa, ya); legAt(b, t, xb, yb);
    line(body, xa + 1, ya, xb, yb, R[k]);
  };
  legLine(legs[0]); legLine(legs[1]);
  {   // the back storey's X-brace, dark behind the front legs
    int xa, ya, xb, yb, xc, yc, xd, yd;
    legAt(legs[0], 0.12f, xa, ya); legAt(legs[1], 0.62f, xb, yb); legAt(legs[1], 0.12f, xc, yc); legAt(legs[0], 0.62f, xd, yd);
    line(body, xa + 1, ya, xb, yb, kWoodDark[1]); line(body, xc, yc, xd + 1, yd, kWoodDark[1]);
  }
  tie(legs[0], legs[1], 0.62f, kWoodDark, 2);
  legLine(legs[2]); legLine(legs[3]);
  tie(legs[2], legs[0], 0.5f, kWood, 2); tie(legs[3], legs[1], 0.5f, kWood, 1);   // the side ties, running back
  tie(legs[2], legs[3], 0.5f, kWood, 3);                                          // the front tie
  // ---- the deck's underside and its front beam (seen from below the parapet: plank ends over two joists)
  for (int x = (int)dx0 - 1; x <= (int)dx1 + 1; x++) {
    const int r0 = row(dF, zDeck);
    body.set(x, r0, kWood[x > dx1 - 1 ? 2 : 3]);
    body.set(x, r0 + 1, kWood[(x % 3 == 0) ? 0 : 1]);
    body.set(x, r0 + 2, kWoodDark[1]);
  }
  // ---- the lookout: dark under the roof between the corner posts, the back wall's planks just catching the lantern
  const int rParF = row(dF, zPar), rEaveF = row(dF + 2.5f, zEave);
  for (int y = rEaveF; y < rParF; y++)
    for (int x = (int)dx0; x <= (int)dx1; x++) {
      const bool glow = std::abs(x - (cxI + 3)) + std::abs(y - (rEaveF + 3)) < 4;
      body.set(x, y, glow ? kWoodDark[2] : ((x + y) % 4 == 0 ? kWoodDark[1] : kWoodDark[0]));
    }
  // ---- the parapet: vertical planks from the deck to waist height, the top rail lit, the east end in shade
  const int rDeckF = row(dF, zDeck);
  for (int x = (int)dx0 - 1; x <= (int)dx1 + 1; x++) {
    const int plank = (x - (int)dx0 + 1) % 3;
    for (int y = rParF; y < rDeckF; y++) {
      int k = plank == 0 ? 1 : (plank == 1 ? 3 : 2);
      if (x >= (int)dx1) k = std::min(k, 1);
      if (x <= (int)dx0) k = std::max(k, 3);
      if (y == rDeckF - 1) k = std::max(0, k - 1);
      if (hashf(x, y, 0x77u) < 0.06f) k = std::max(0, k - 1);   // knots
      body.set(x, y, kWood[k]);
    }
    body.set(x, rParF - 1, kWood[x < cxI ? 4 : 3]);   // the rail's lit top
    body.set(x, rParF, kWood[x >= (int)dx1 ? 1 : 2]);
  }
  // the corner posts rising from the parapet to the eaves (the west one lit)
  for (int y = rEaveF; y < rParF; y++) {
    body.set((int)dx0 - 1, y, kWood[3]); body.set((int)dx0, y, kWood[2]);
    body.set((int)dx1, y, kWood[1]); body.set((int)dx1 + 1, y, kWood[0]);
  }
  // the lantern hanging in the lookout
  {
    const int lx = cxI + 3, ly = rEaveF + 2;
    body.set(lx, ly - 1, kIron[2]);
    body.set(lx - 1, ly, kIron[2]); body.set(lx, ly, kIron[3]); body.set(lx + 1, ly, kIron[1]);
    body.set(lx - 1, ly + 1, kGlow[3]); body.set(lx, ly + 1, kGlow[4]); body.set(lx + 1, ly + 1, kGlow[2]);
    body.set(lx - 1, ly + 2, kGlow[2]); body.set(lx, ly + 2, kGlow[3]); body.set(lx + 1, ly + 2, kGlow[1]);
    body.set(lx, ly + 3, kIron[1]);
  }
  // ---- the roof: a steep pyramid of shingles, eaves 2.5 px out all round, painted back to front
  const float rx0 = dx0 - 3.0f, rx1 = dx1 + 3.0f, ry0 = dB - 2.5f, ry1 = dF + 2.5f;
  const float rcx = (rx0 + rx1) * 0.5f, rcy = (ry0 + ry1) * 0.5f, hw = (rx1 - rx0) * 0.5f, hd = (ry1 - ry0) * 0.5f;
  Canvas roof(W, H);
  for (float gy = ry0; gy <= ry1; gy += 0.5f)
    for (int x = (int)std::floor(rx0); x <= (int)std::ceil(rx1); x++) {
      const float gx = x + 0.5f;
      const float ux = std::fabs(gx - rcx) / hw, uy = std::fabs(gy - rcy) / hd;
      const float u = std::max(ux, uy);
      if (u > 1) continue;
      const float z = zEave + (zPeak - zEave) * (1 - u);
      const int r = row(gy, z);
      int k;
      if (uy >= ux) k = gy > rcy ? 2 : 3;          // the south face mid-tone, the north face (a sliver) catching the light
      else k = gx < rcx ? 4 : 1;                   // the west face lit, the east in shade
      // shingle courses every 3 units of height; the tabs offset course to course; the hips (face edges) a lighter line
      const float course = std::fmod(z - zEave + 300.0f, 3.0f);
      if (course < 0.7f) k = std::max(0, k - 1);
      else if (((x + (int)((z - zEave) / 3.0f) * 2) % 4) == 0 && k > 1 && k < 4) k--;
      if (std::fabs(ux - uy) < 0.035f && u < 0.95f) k = std::min(4, k + 1);
      if (u > 0.95f) k = std::max(0, k - 1);       // the eave's edge
      roof.set(x, r, kShingle[k]);
    }
  // the fascia: a dark board under the front eave, the roof's shade falling on the lookout
  for (int x = (int)std::floor(rx0) + 1; x <= (int)std::ceil(rx1) - 1; x++) roof.set(x, row(ry1, zEave) + 1, kWoodDark[0]);
  // a finial and a small pennant at the peak (rust, flying east)
  {
    const int sx = (int)std::floor(rcx), sy = row(rcy, zPeak);
    for (int y = sy - 7; y <= sy; y++) roof.set(sx, y, kWood[3]);
    roof.set(sx, sy - 8, kGold[4]);
    for (int i = 0; i < 7; i++) {
      const int half = i < 4 ? 1 : 0;
      for (int j = -half; j <= half; j++) roof.set(sx + 1 + i, sy - 6 + j + (i > 3 ? 1 : 0), kRiseTrim[j < 0 ? 3 : 2]);
    }
  }
  // ---- the ladder, leaning out from the deck's front to the ground a little west of the middle
  {
    const int topY = rDeckF + 2, footY = base - 1;
    for (int y = topY; y <= footY; y++) {
      const float t = (float)(y - topY) / (footY - topY);
      const int lean = (int)std::lround(t * 3.0f);
      const int l = cxI - 4 + lean - 1, r = cxI + 1 + lean - 1;
      body.set(l, y, kWood[3]);
      body.set(r, y, kWood[1]);
      if ((y - topY) % 4 == 3) for (int x = l + 1; x < r; x++) body.set(x, y, kWood[x < cxI - 1 ? 4 : 3]);
    }
  }
  outline(body);
  outline(roof);
  blit(body, roof, 0, 0);
  // the cast shadow: the legs and the deck's shade thrown to the lower right
  castShadow(c, {V(cx - 13, gB + 2), V(cx + 6, gB - 2), V(W - 1, gB - 3), V(W - 1, base), V(cx - 11, base)}, 52);
  blit(c, body, 0, 0);
  tuftsAt(c, 2, W - 3, base, 41, 0.4f);
}
}  // namespace

int risePropW(Prop p) { return riseInfo(p).w; }
int risePropH(Prop p) { return riseInfo(p).h; }
int risePropFrames(Prop p) { return riseInfo(p).frames; }

void paintRiseProp(Canvas& c, Prop p, int frame) {
  switch (p) {
    case Prop::LordBanner: lordBanner(c, frame); break;
    case Prop::Watchtower: watchtower(c); break;
    default: break;
  }
}

// ---------------------------------------------------------------- the view's pieces
Canvas lordBannerSprite(const cult::Heraldry& h) {
  const int w = risePropW(Prop::LordBanner), hh = risePropH(Prop::LordBanner), n = risePropFrames(Prop::LordBanner);
  Canvas sheet(w * n, hh);
  const uint32_t field = h.field ? h.field : rgba(150, 40, 40);
  const uint32_t second = h.field2 ? h.field2 : (h.charge ? h.charge : rgba(232, 200, 90));
  const uint32_t trim = h.charge ? h.charge : rgba(232, 200, 90);
  const Ramp F = ramp(opaque(field), 0.9f), F2 = ramp(opaque(second), 0.9f), T = ramp(opaque(trim), 0.8f);
  const Mask7 ch = chargeMask(h);
  for (int f = 0; f < n; f++) {
    Canvas c(w, hh);
    standardFrame(c, f, F, F2, T, h.division, ch.any() ? &ch : nullptr, (uint32_t)(h.field ^ h.charge));
    place(sheet, c, f, 0);
  }
  return sheet;
}

Canvas rankBadge(int rank, uint32_t field, uint32_t trim, int size) {
  size = std::clamp(size, 9, 32);
  const int W = size, H = (int)std::lround(size * 1.2f);
  Canvas c(W, H);
  const Ramp F = ramp(opaque(field ? field : rgba(150, 40, 40)), 0.9f);
  const Ramp G = kGold;
  const Ramp T = trim ? ramp(opaque(trim), 0.8f) : kGold;
  // the heater shield: straight top, straight sides to 55 %, curved flanks to the point
  const float w = W - 2.0f, hgt = H - 2.0f;
  auto inside = [&](float x, float y) {
    if (y < 0 || y > hgt) return false;
    const float u = std::fabs(x - w * 0.5f) / (w * 0.5f), v = y / hgt;
    if (v < 0.55f) return u <= 1.0f;
    const float t = (v - 0.55f) / 0.45f;
    return u <= std::sqrt(std::max(0.0f, 1.0f - t * t));
  };
  // the field, lit across the upper left, the point in shade; a gilded rim
  for (int y = 0; y < H - 1; y++)
    for (int x = 0; x < W - 1; x++) {
      const float fx = x + 0.5f, fy = y + 0.5f;
      if (!inside(fx, fy)) continue;
      const bool rim = !inside(fx - 1, fy) || !inside(fx + 1, fy) || !inside(fx, fy - 1) || !inside(fx, fy + 1);
      const float l = (1 - fx / w) * 0.5f + (1 - fy / hgt) * 0.5f;
      int k = l > 0.72f ? 3 : (l > 0.4f ? 2 : 1);
      uint32_t col = F[k];
      if (rim) col = G[fx < w * 0.5f && fy < hgt * 0.6f ? 4 : (fy > hgt * 0.75f ? 1 : 2)];
      c.set(x + 1, y + 1, col);
    }
  // the marks, in gold over the field (unit u = one shield-width fraction, scaled to the size)
  const float s = W / 11.0f;   // pixels per design unit (11 at the HUD's size)
  const int mx = W / 2;        // centre column (the marks are drawn symmetric round it)
  auto px2 = [&](int x, int y, int k) { if (x >= 1 && y >= 1 && x < W - 1 && y < H - 1 && inside(x - 0.5f, y - 0.5f)) c.set(x, y, G[k]); };
  auto chevron = [&](int topY) {   // a ^ chevron: its arms 1 px thick under 16 px (2 above), a gap between chevrons
    const int th = W >= 16 ? 2 : 1;
    const int half = std::max(2, (int)std::lround(3.0f * s));
    for (int i = -half; i <= half; i++) {
      const int y = topY + (std::abs(i) * 2 + 1) / 3;
      for (int t = 0; t < th; t++) px2(mx + i, y + t, t == 0 ? (i <= 0 ? 4 : 3) : 2);
    }
  };
  auto star = [&](float cyf, float r) {
    const int cxp = mx, cyp = (int)std::lround(cyf);
    const int R = std::max(1, (int)std::lround(r));
    for (int d = -R; d <= R; d++) { px2(cxp + d, cyp, d <= 0 ? 4 : 3); px2(cxp, cyp + d, d <= 0 ? 4 : 2); }
    if (R >= 2) { px2(cxp - 1, cyp - 1, 4); px2(cxp + 1, cyp - 1, 3); px2(cxp - 1, cyp + 1, 3); px2(cxp + 1, cyp + 1, 2); }
  };
  switch (rank) {
    case 1: chevron((int)std::lround(H * 0.36f)); break;
    case 2: {
      const int step = std::max(3, (int)std::lround(2.8f * s));
      const int y0 = std::max(2, (int)std::lround(H * 0.18f));
      for (int k = 0; k < 3; k++) chevron(y0 + k * step);
      break;
    }
    case 3: {   // a sash from the upper left to the lower right in the trim, a star above it
      const int th = std::max(2, (int)std::lround(1.8f * s));
      for (int y = 1; y < H - 1; y++)
        for (int x = 1; x < W - 1; x++) {
          const float d = (x - W * 0.18f) - (y - H * 0.30f) * (W / (float)H) * 1.15f;
          if (d >= 0 && d < th && inside(x - 0.5f, y - 0.5f)) c.set(x, y, T[d < 1 ? 4 : (d >= th - 1 ? 1 : 3)]);
        }
      star(H * 0.62f, 1.6f * s);
      break;
    }
    case 4: {   // crossed batons under a crown
      const int n = (int)std::lround(5.0f * s);
      for (int i = -n; i <= n; i++) {
        const int y = (int)std::lround(H * 0.62f + i * 0.9f);
        px2(mx + i, y, i < 0 ? 4 : 2);
        px2(mx - i, y, i < 0 ? 3 : 2);
      }
      px2(mx - n, (int)std::lround(H * 0.62f - n * 0.9f), 4); px2(mx + n, (int)std::lround(H * 0.62f - n * 0.9f), 3);
      const int cyp = (int)std::lround(H * 0.26f), cw = (int)std::lround(3.0f * s);
      for (int i = -cw; i <= cw; i++) { px2(mx + i, cyp + 1, i <= 0 ? 4 : 3); px2(mx + i, cyp + 2, 2); }
      for (int i = -cw; i <= cw; i += std::max(1, cw)) px2(mx + i, cyp, 4);
      break;
    }
    default: break;
  }
  outline(c);
  return c;
}

Canvas warlordStandard(uint32_t field, uint32_t trim) {
  const int W = kWarlordStdW, H = kWarlordStdH, n = 4;
  const Ramp F = ramp(opaque(field ? field : rgba(110, 30, 34)), 0.95f);
  const Ramp T = ramp(opaque(trim ? trim : rgba(30, 26, 30)), 0.8f);
  Canvas sheet(W * n, H);
  for (int f = 0; f < n; f++) {
    Canvas c(W, H), body(W, H), cloth(W, H);
    const int base = H - 1, px = W / 2 - 1;
    // a cairn of rough stones round the foot
    rock(body, px + 0.5f, base - 3.0f, 6.0f, 3.2f, kStone, 0x5101u + 3, 7);
    rock(body, px - 4.0f, base - 1.5f, 3.0f, 2.0f, kStone, 0x5101u + 9, 6);
    rock(body, px + 5.5f, base - 1.5f, 2.8f, 1.8f, kStone, 0x5101u + 17, 6);
    // the crooked pole: a bent young trunk, bark-dark, lit on its west edge
    for (int y = 4; y <= base - 4; y++) {
      const int bend = (int)std::lround(std::sin(y * 0.11f) * 1.2f);
      body.set(px + bend, y, kBark[3]);
      body.set(px + 1 + bend, y, kBark[1]);
      if (y % 9 == 3) body.set(px + 2 + bend, y, kBark[2]);   // a lopped twig
    }
    // the crossbar, lashed with rope
    const int cy = 10;
    for (int x = px - 8; x <= px + 9; x++) { body.set(x, cy, kBark[x < px ? 3 : 2]); body.set(x, cy + 1, kBark[1]); }
    body.set(px - 1, cy - 1, kRiseTrim[1]); body.set(px + 2, cy + 2, kRiseTrim[1]);
    // the horned skull at the top: a bone dome, dark eye pits, two curled horns sweeping out and up
    {
      // a ram-horned skull: the cranium a lit dome, deep eye pits, a long snout with teeth; the horns sweep out, curl down
      // and forward (thick at the root, a light ridge along their tops, ribbed)
      const int sx = px + 1, sy = 5;
      ellipse(body, sx, sy - 0.5, 3.4, 2.8, kBone[2]);
      for (int x = sx - 2; x <= sx + 1; x++) body.set(x, sy - 3, kBone[4]);
      body.set(sx - 3, sy - 2, kBone[4]); body.set(sx - 2, sy - 2, kBone[4]); body.set(sx + 2, sy - 2, kBone[2]);
      body.set(sx + 3, sy, kBone[1]); body.set(sx + 3, sy - 1, kBone[1]);
      body.set(sx - 2, sy, kInk); body.set(sx - 1, sy, kEye); body.set(sx + 1, sy, kEye); body.set(sx + 2, sy, kInk);
      body.set(sx - 1, sy + 1, kInk); body.set(sx + 1, sy + 1, kInk);
      for (int y = sy + 2; y <= sy + 4; y++) { body.set(sx - 1, y, kBone[3]); body.set(sx, y, kBone[2]); body.set(sx + 1, y, kBone[1]); }
      body.set(sx - 1, sy + 5, kBone[2]); body.set(sx + 1, sy + 5, kBone[1]);   // the teeth
      // the horns: a spiral path from the skull's crown out to each side (mirrored), 2 px thick
      static const int hp[9][2] = {{3, -2}, {4, -3}, {5, -3}, {6, -2}, {7, -1}, {7, 0}, {7, 1}, {6, 2}, {5, 2}};
      for (int i = 0; i < 9; i++) {
        const int k = (i % 2) ? 3 : 2;
        body.set(sx - hp[i][0], sy + hp[i][1], kBone[k + 1]);
        body.set(sx - hp[i][0], sy + hp[i][1] + 1, kBone[i > 5 ? 1 : 2]);
        body.set(sx + hp[i][0], sy + hp[i][1], kBone[k]);
        body.set(sx + hp[i][0], sy + hp[i][1] + 1, kBone[i > 5 ? 0 : 1]);
      }
      body.set(sx - 4, sy - 1, kBone[3]); body.set(sx + 4, sy - 1, kBone[1]);
    }
    // the ragged banner: torn and holed, hanging from the crossbar, swaying at its tail
    const int x0 = px - 6, x1 = px + 8, y0 = cy + 2, y1 = 38;
    const float ph = f * 1.5708f;
    for (int y = y0; y < y1; y++) {
      const float t = (float)(y - y0) / (y1 - y0);
      const int off = (int)std::lround(std::sin(t * 2.4f + ph) * 2.2f * t * t);
      for (int x = x0; x < x1; x++) {
        const int i = x - x0;
        // torn: each column ends at its own depth (a ragged lower edge), a few holes burnt through
        const int len = (y1 - y0) - (int)(hash3(i, 0, 0x7A11u) % 9) - (i % 3 == 1 ? 3 : 0);
        if (y - y0 > len) continue;
        if (hashf(i, y, 0x404u) < 0.035f && y > y0 + 3) continue;
        const float wave = std::sin(i * 0.9f - ph * 1.1f + t * 2.0f);
        int k = wave > 0.4f ? 3 : (wave < -0.4f ? 1 : 2);
        if (i == 0) k = std::min(4, k + 1);
        if (i == x1 - x0 - 1) k = std::max(0, k - 1);
        uint32_t col = F[k];
        // a crude daub across it in the trim (a slash and a rough eye: the warband's sign), soot at the frayed edge
        const int di = i - 7, dj = y - (y0 + 9);
        if (std::abs(di + dj / 2) <= 1 && dj > -5 && dj < 7) col = T[std::clamp(k, 1, 3)];
        if (std::abs(dj) <= 1 && std::abs(di) <= 3 && !(std::abs(di) <= 1 && dj == 0)) col = T[std::clamp(k + (dj < 0 ? 1 : 0), 1, 4)];
        if (y - y0 > len - 2) col = darken(col, 0.45f);
        cloth.set(x + off, y, col);
      }
    }
    // trophies hung from the crossbar's ends: a small skull on a thong, a bundle of feathers
    {
      const int lx = px - 8, rx = px + 9;
      for (int y = cy + 2; y < cy + 6; y++) body.set(lx, y, kLeather[1]);
      body.set(lx - 1, cy + 6, kBone[3]); body.set(lx, cy + 6, kBone[3]); body.set(lx + 1, cy + 6, kBone[2]);
      body.set(lx - 1, cy + 7, kBone[2]); body.set(lx, cy + 7, kInk); body.set(lx + 1, cy + 7, kBone[1]);
      body.set(lx, cy + 8, kBone[1]);
      const int sw = (f & 1) ? 1 : 0;
      for (int y = cy + 2; y < cy + 9; y++) body.set(rx + (y > cy + 5 ? sw : 0), y, y < cy + 5 ? kLeather[1] : (y & 1 ? kRed[2] : kWhite));
    }
    outline(cloth);
    outline(body);
    blit(body, cloth, 0, 0);
    castShadow(c, {V(px + 4, base), V(px + 9, base - 4), V(W - 1, base - 4), V(W - 1, base), V(px + 5, base + 1)}, 60);
    blit(c, body, 0, 0);
    tuftsAt(c, px - 8, px + 9, base, 0x77u, 0.3f);
    place(sheet, c, f, 0);
  }
  return sheet;
}

Canvas trophyPike(int variant, uint32_t trim) {
  const int W = kTrophyW, H = kTrophyH, base = H - 1, px = W / 2 - 1;
  Canvas c(W, H), body(W, H);
  const Ramp T = ramp(opaque(trim ? trim : rgba(120, 34, 36)), 0.8f);
  variant &= 3;
  // the shaft, driven into a little mound of earth
  for (int y = 6; y <= base - 1; y++) { body.set(px, y, kWood[3]); body.set(px + 1, y, kWood[1]); }
  for (int x = px - 3; x <= px + 4; x++) { body.set(x, base - 1, kSoilR[x < px ? 3 : 2]); body.set(x, base, kSoilR[1]); }
  body.set(px - 2, base - 2, kSoilR[3]); body.set(px + 3, base - 2, kSoilR[2]);
  // a spear point at the top
  body.set(px, 3, kIron[4]); body.set(px + 1, 3, kIron[2]); body.set(px, 4, kIron[3]); body.set(px + 1, 4, kIron[1]);
  body.set(px, 5, kIron[2]); body.set(px + 1, 5, kIron[1]);
  auto skull = [&](int sx, int sy, bool beast) {
    ellipse(body, sx + 0.5, sy, beast ? 3.0 : 2.4, 2.2, kBone[2]);
    body.set(sx - 1, sy - 2, kBone[4]); body.set(sx, sy - 2, kBone[4]); body.set(sx - 2, sy - 1, kBone[3]);
    body.set(sx - 1, sy, kInk); body.set(sx + 1, sy, kInk);
    body.set(sx, sy + 2, kBone[1]); body.set(sx - 1, sy + 2, kBone[2]); body.set(sx + 1, sy + 2, kBone[1]);
    if (beast) { body.set(sx - 3, sy - 2, kBone[3]); body.set(sx - 4, sy - 3, kBone[4]); body.set(sx + 3, sy - 2, kBone[2]); body.set(sx + 4, sy - 3, kBone[3]); body.set(sx, sy + 3, kBone[1]); }
  };
  switch (variant) {
    case 0:   // a dented helm over a skull
      skull(px, 10, false);
      for (int x = px - 3; x <= px + 3; x++) { body.set(x, 7, kIron[x < px ? 4 : 2]); body.set(x, 8, kIron[x < px ? 3 : 1]); }
      body.set(px - 2, 6, kIron[4]); body.set(px - 1, 6, kIron[4]); body.set(px, 6, kIron[3]); body.set(px + 1, 6, kIron[2]); body.set(px + 2, 6, kIron[2]);
      body.set(px + 1, 7, kIron[0]);   // the dent
      break;
    case 1:   // two skulls
      skull(px, 8, false); skull(px + 1, 14, false);
      break;
    case 2: { // a split round shield and a horn
      ellipse(body, px + 0.5, 12, 4.2, 3.6, T[2]);
      for (int y = 9; y <= 15; y++) body.set(px + 1, y, kWood[1]);
      body.set(px - 2, 10, T[4]); body.set(px - 3, 11, T[3]); body.set(px + 4, 13, T[1]);
      body.set(px + 3, 9, kInk); body.set(px + 2, 10, kInk);      // the split
      for (int i = 0; i < 4; i++) body.set(px - 2 + i, 18 + (i > 1 ? 1 : 0), kBone[i < 2 ? 4 : 2]);
      break;
    }
    default:  // a great beast's horned skull
      skull(px, 9, true);
      break;
  }
  // a strip of the warband's cloth tied under it
  for (int y = 17; y < 24; y++) body.set(px + 2 + (y > 20 ? 1 : 0), y, T[y < 20 ? 3 : 2]);
  outline(body);
  castShadow(c, {V(px + 2, base), V(px + 6, base - 3), V(W - 1, base - 3), V(W - 1, base), V(px + 3, base + 1)}, 56);
  blit(c, body, 0, 0);
  return c;
}

Canvas resettlerStores(int variant) {
  const int W = kStoresW, H = kStoresH, base = H - 1;
  Canvas c(W, H), body(W, H);
  variant = ((variant % 3) + 3) % 3;
  if (variant == 0) {
    // fresh-cut logs stacked east-west in a pyramid (3, 2, 1): bark along their lengths (lit tops, shaded bellies),
    // the pale cut ends with their rings at the west end, two stakes holding the stack
    auto log = [&](int x0, int x1, int cy) {
      for (int x = x0 + 1; x <= x1; x++) {
        body.set(x, cy - 2, kBark[(x * 5) % 7 == 0 ? 3 : 4]);
        body.set(x, cy - 1, kBark[3]);
        body.set(x, cy, kBark[(x * 3) % 5 == 0 ? 1 : 2]);
        body.set(x, cy + 1, kBark[1]);
      }
      body.set(x1 + 1, cy - 1, kBark[1]); body.set(x1 + 1, cy, kBark[0]);   // the far end, in shade
      // the cut end: a pale disc with its ring and a dark heart
      body.set(x0, cy - 2, kWood[4]); body.set(x0 + 1, cy - 2, kWood[4]);
      body.set(x0 - 1, cy - 1, kWood[4]); body.set(x0, cy - 1, kWood[3]); body.set(x0 + 1, cy - 1, kWood[4]);
      body.set(x0 - 1, cy, kWood[3]); body.set(x0, cy, kWood[1]); body.set(x0 + 1, cy, kWood[3]);
      body.set(x0, cy + 1, kWood[2]); body.set(x0 + 1, cy + 1, kWood[2]);
    };
    log(6, 23, base - 5);      // the back log of the bottom layer
    log(5, 22, base - 8);      // the top log, resting in the groove between
    log(4, 21, base - 2);      // the front log
    for (int y = base - 9; y <= base; y++) { body.set(3, y, kWood[3]); body.set(25, y, kWood[1]); }   // the stakes
  } else if (variant == 1) {
    // bundles of golden thatch reed, bound twice, stacked in a ridge
    auto bundle = [&](int bx, int by, int len) {
      for (int x = bx; x < bx + len; x++)
        for (int y = by - 2; y <= by + 2; y++) {
          const int k = y < by - 1 ? 4 : (y < by + 1 ? 3 : (y == by + 1 ? 2 : 1));
          body.set(x, y, kThatch[(x * 7 + y * 3) % 5 == 0 ? std::max(1, k - 1) : k]);
        }
      for (int y = by - 2; y <= by + 2; y++) { body.set(bx + len / 3, y, kLeather[2]); body.set(bx + 2 * len / 3, y, kLeather[1]); }
      for (int y = by - 1; y <= by + 1; y++) body.set(bx - 1, y, kThatch[y < by ? 4 : 2]);   // the cut butts
    };
    bundle(3, base - 3, 10); bundle(14, base - 3, 10); bundle(8, base - 7, 11);
  } else {
    // a heap of squared new stone and a mortar trough
    for (int r = 0; r < 2; r++)
      for (int i = 0; i < 3 - r; i++) {
        const int bx = 3 + i * 6 + r * 3, by = base - 2 - r * 4;
        for (int x = bx; x < bx + 5; x++) {
          body.set(x, by - 3, kStoneWarm[4]);
          body.set(x, by - 2, kStoneWarm[x == bx + 4 ? 2 : 3]);
          body.set(x, by - 1, kStoneWarm[x == bx + 4 ? 1 : 2]);
          body.set(x, by, kStoneWarm[1]);
        }
      }
    for (int x = 19; x < 27; x++) { body.set(x, base - 4, kWood[3]); for (int y = base - 3; y <= base - 1; y++) body.set(x, y, kWood[x > 25 ? 1 : 2]); }
    for (int x = 20; x < 26; x++) body.set(x, base - 4, rgba(208, 204, 192));   // the mortar
  }
  outline(body);
  castShadow(c, {V(3, base), V(W - 4, base), V(W - 1, base - 3), V(W - 1, base - 7), V(W - 6, base - 9)}, 54);
  blit(c, body, 0, 0);
  tuftsAt(c, 1, W - 2, base, 0x51u + (uint32_t)variant, 0.35f);
  return c;
}

Canvas clawScar(int variant) {
  const int W = kClawW, H = kClawH;
  Canvas c(W, H);
  variant &= 3;
  // the burn: a patch of scorched earth round the strike, dithered out at its rim (ground seen from above: a wide ellipse)
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const float ex = (x + 0.5f - W * 0.5f) / (W * 0.48f), ey = (y + 0.5f - H * 0.5f) / (H * 0.46f);
      const float d = ex * ex + ey * ey + (fbm(x * 0.25f, y * 0.25f, 0x5C0u + (uint32_t)variant) - 0.5f) * 0.5f;
      if (d >= 1.0f) continue;
      const float k = 1.0f - d;
      if (bayer(x, y) > k * 1.6f) continue;
      c.set(x, y, withA(k > 0.55f ? rgba(46, 30, 30) : rgba(70, 48, 40), (int)(110 + 110 * std::min(1.0f, k * 1.4f))));
    }
  // the gashes: three or four claw strokes torn through the earth, curving, deepest in the middle, each with a thrown-up
  // lip of raw soil on its upper edge, a black trough and embers still glowing in its heart
  // (M12 fixer) eight rakes: the angle, its sense and the claws' fan differ, so no two scars on a square repeat, and the
  // strokes splay from where the paw struck and bow hard as it dragged (parallel straight strokes read as planks)
  variant &= 7;
  const int n = 3 + (variant & 1);
  static const float angs[8] = {-0.55f, 0.48f, -0.18f, 0.72f, 0.25f, -0.78f, 0.05f, -0.36f};
  const float ang = angs[variant];
  const float bendK = (variant & 4) ? 3.6f : -3.0f;
  const bool fromHead = (variant & 2) != 0;   // which end the paw struck (the strokes splay from it)
  const float dx = std::cos(ang), dy = std::sin(ang) * 0.6f;
  const float nx = -dy, ny = dx;                // across the rake
  const float nl = std::max(0.01f, std::sqrt(nx * nx + ny * ny));
  for (int k = 0; k < n; k++) {
    const float rel = k - (n - 1) * 0.5f;
    const float len = 17.0f - std::fabs(rel) * 3.0f + (float)((variant + k) % 3);
    const float shift = (float)(((variant * 3 + k * 5) % 5) - 2);   // the claws never quite in step
    for (float t = -len; t <= len; t += 0.5f) {
      const float u = t / len, taper = 1.0f - u * u;
      const float spread = 2.5f + 6.5f * ((fromHead ? (u + 1.0f) : (1.0f - u)) * 0.5f);   // tight where it struck, splayed
      const float bend = taper * bendK * 0.5f;
      const float px = W * 0.5f + dx * (t + shift) + nx / nl * rel * spread;
      const float py = H * 0.5f + dy * (t + shift) + ny / nl * rel * spread * 0.6f + bend;
      const int x = (int)std::lround(px), y = (int)std::lround(py);
      const int wpx = taper > 0.55f ? 3 : (taper > 0.2f ? 2 : 1);
      for (int j = 0; j < wpx; j++) c.set(x, y + j, j == 0 && wpx > 1 ? rgba(20, 12, 16) : (j == wpx - 1 ? rgba(66, 30, 28) : rgba(32, 18, 20)));   // the gouge
      if (taper > 0.5f) c.set(x, y - 1, rgba(96, 62, 46));                              // its torn rim (dark soil, never a plank's light edge)
      if (taper > 0.8f && ((x * 7 + k * 3) % 7) == 0) c.set(x, y + 1, rgba(222, 98, 38));   // an ember deep in it
    }
    // soil flung out past the stroke's far end
    for (int e = 0; e < 3; e++) {
      const float t = (fromHead ? 1.0f : -1.0f) * (len + 2.0f + e * 1.5f);
      const int x = (int)std::lround(W * 0.5f + dx * (t + shift) + nx / nl * rel * 9.0f + (e - 1)), y = (int)std::lround(H * 0.5f + dy * (t + shift) + ny / nl * rel * 5.4f + (e & 1));
      c.set(x, y, e == 1 ? rgba(74, 52, 42) : rgba(110, 80, 58));
    }
  }
  return c;
}

}  // namespace art
