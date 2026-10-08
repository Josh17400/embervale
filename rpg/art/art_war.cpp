// EMBERVALE art (M4 Banners): the war made visible: WarTent .. Barricade in art_props.h. WARDS lane.
// Painted in the shared language of rpg/art: the 3/4 top-down view (a ground point at depth y and height z lands on
// screen row y - z, as the building painter projects), light from the top-left (lit west faces and slopes, shaded east
// ones, south faces mid-tone), hue-shifted ramps from art_internal.h, a soft cast shadow thrown down-right onto the
// ground, grass anchoring each piece. Kingdom cloth comes in through warPropSprite(p, field, trim): the soldier's tent
// and the command pavilion in the attacker's field with trim, pennants and the banner; the checkpoint's barrier pole
// striped in the owner's colours. The canvas sizes are the art's own; the tile FOOTPRINTS are frozen (m4Footprint: the
// prop stands on the bottom-centre tile of its footprint and its sprite is drawn bottom-centred on that tile).
#include <cstdlib>
#include "rpg/art/art_internal.h"

namespace art {
namespace {

struct WarInfo { uint8_t w, h, frames; };
// WarTent, CommandTent, Catapult, Palisade, RefugeeTent, Rubble, Ash, Scaffold, Barricade
const WarInfo kWar[] = {{60, 58, 1}, {64, 70, 1}, {64, 56, 1}, {16, 26, 1}, {56, 42, 1}, {28, 24, 1}, {16, 16, 1}, {20, 46, 1}, {28, 26, 1}};
constexpr int kWarN = (int)(sizeof(kWar) / sizeof(kWar[0]));
static_assert(kWarN == (int)Prop::Barricade + 1 - (int)Prop::WarTent, "a size for every M4 war prop");
inline const WarInfo& wi(Prop p) { return kWar[std::clamp((int)p - (int)Prop::WarTent, 0, kWarN - 1)]; }

// ---------------------------------------------------------------- shared bits
const Ramp kSoil = ramp5(rgba(44, 28, 34), rgba(72, 46, 40), rgba(102, 70, 50), rgba(134, 98, 66), rgba(166, 128, 88));
// the cloth ramp of a kingdom colour (or the neutral linen of an unowned camp), hue-shifted like every ramp
Ramp clothOf(uint32_t field, const Ramp& fallback) { return field ? ramp(opaque(field), 0.9f) : fallback; }
// a trim colour that reads against the field (a dark field gets a light trim and the reverse)
Ramp trimOf(uint32_t field, uint32_t trim) {
  if (trim) return ramp(opaque(trim), 0.8f);
  if (!field) return kRed;
  return luma(field) > 0.45f ? ramp(rgba(70, 40, 40), 0.8f) : ramp(rgba(226, 204, 150), 0.7f);
}
// grass blades at the foot of a thing (anchors it to the ground)
void tufts(Canvas& c, int x0, int x1, int y, uint32_t seed, float density = 0.45f) {
  for (int x = x0; x <= x1; x++) {
    if (hashf(x, y, seed) > density || solid(c, x, y)) continue;
    const int h = 1 + (int)(hash3(x, 1, seed) % 3);
    for (int j = 0; j < h; j++) c.set(x + ((j == h - 1 && (x & 1)) ? 1 : 0), y - j, kLeaf[j == h - 1 ? 3 : (j == 0 ? 1 : 2)]);
  }
}
// the soft cast shadow on the ground (painted only where nothing stands, after the outline): the light comes from the
// top-left, so a thing's shadow falls to its lower right
void groundShadow(Canvas& c, const std::vector<Vec2>& pts, int alpha) {
  Canvas s(c.w, c.h);
  poly(s, pts, rgba(255, 255, 255));
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      if (!solid(s, x, y) || solid(c, x, y)) continue;
      // soften the rim: the outer pixel ring lighter (a dither so it melts into the grass)
      int n = 0;
      for (int k = 0; k < 4; k++) n += solid(s, x + (k == 0) - (k == 1), y + (k == 2) - (k == 3)) ? 1 : 0;
      const int a = n < 4 ? (bayer(x, y) < 0.5f ? alpha / 2 : 0) : alpha;
      if (a) c.set(x, y, withA(kShadowHue, a));
    }
}
// a guy rope from (x0, y0) down to a peg at (x1, y1), with a slight sag; the peg a stub of wood
void guyRope(Canvas& c, float x0, float y0, float x1, float y1, uint32_t rope) {
  const int n = (int)std::ceil(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0)));
  for (int i = 0; i <= n; i++) {
    const float t = n ? (float)i / n : 0;
    const float sag = 1.6f * std::sin(t * 3.14159f);
    const int x = (int)std::lround(x0 + (x1 - x0) * t), y = (int)std::lround(y0 + (y1 - y0) * t + sag);
    if (!solid(c, x, y)) c.set(x, y, rope);
  }
  c.set((int)std::lround(x1), (int)std::lround(y1), kWood[1]);
  c.set((int)std::lround(x1), (int)std::lround(y1) - 1, kWood[3]);
}
// a small swallow-tailed pennant flying east from a pole top (field with a trim stripe)
void pennant(Canvas& c, int px, int py, const Ramp& F, const Ramp& T, int len, uint32_t seed) {
  for (int i = 0; i < len; i++) {
    const float t = (float)i / len;
    const int wave = (int)std::lround(std::sin(t * 4.2f + seed % 7) * 0.9f);
    const int half = std::max(1, (int)std::lround(2.4f * (1 - t * 0.55f)));
    for (int j = -half; j <= half; j++) {
      if (i > len - 3 && std::abs(j) < 1) continue;   // the swallow tail
      int k = j < 0 ? 3 : 2;
      if (wave > 0 && j > 0) k = 1;
      c.set(px + 1 + i, py + half + j + wave, (j == 0 && i < len - 2) ? T[k] : F[k]);
    }
  }
}

// ---------------------------------------------------------------- the soldier's ridge tent (3 x 2)
// The ridge runs north-south (into the picture), so the front gable faces the viewer with its door; the west slope is
// lit, the east slope in shade; the canvas sags a little between the poles; seams run down the slopes, a hem in the
// trim colour along the eaves; the ridge pole carries a pennant; guy ropes to pegs at the corners.
void warTent(Canvas& c, const Ramp& F, const Ramp& T, uint32_t seed) {
  const int W = c.w, cx = W / 2;
  const int yf = c.h - 7;              // the front eave line (ground at the front)
  const int D = 20;                    // depth of the tent on the ground
  const int yb = yf - D;
  const float hw = 17.0f, ridge = 21.0f, eave = 3.0f;
  // the cast shadow first (its shape), so the tent paints over it
  std::vector<Vec2> sh = {V(cx - hw + 2, yf + 1), V(cx + hw + 1, yf + 1), V(cx + hw + 9, yf - 2), V(cx + hw + 9, yb + 4), V(cx + hw, yb - 1)};
  // the body: back to front, every ground row; a pixel is the highest surface there
  auto zAt = [&](float gx, float gy) {
    const float u = std::fabs(gx - cx) / hw;
    if (u > 1) return -1.0f;
    const float t = (gy - yb) / D;
    const float sag = 1.6f * std::sin(t * 3.14159f) * (1 - u);   // the cloth bellies between the poles
    return eave + (ridge - eave) * (1 - u) - sag;
  };
  Canvas body(W, c.h);
  for (float gy = (float)yb; gy <= yf; gy += 0.5f)
    for (int x = (int)(cx - hw); x <= (int)(cx + hw); x++) {
      const float gx = x + 0.5f;
      const float z = zAt(gx, gy);
      if (z < 0) continue;
      const int row = (int)std::floor(gy - z);
      const bool west = gx < cx;
      const float u = std::fabs(gx - cx) / hw;
      // seams down the slopes at fixed depths; the panels belly between them
      const float panel = std::fmod(gy - yb + 100.0f, 6.5f);
      // the lit west slope brightest up by the ridge; the shaded east slope catches a little light at the ridge and is
      // darkest down at its eave
      int k = west ? (u < 0.3f ? 4 : 3) : (u < 0.18f ? 2 : (u > 0.72f ? 0 : 1));
      if (panel < 0.5f) k = std::max(0, k - 1); // a seam
      uint32_t col = F[k];
      // a faint weave: every third pixel of the light slope a hair warmer
      if (((x + row) % 3) == 0 && k >= 2) col = mix(col, F[std::min(4, k + 1)], 0.25f);
      // the hem along the eaves in the trim colour (two pixels above the eave)
      if (z <= eave + 1.4f) col = T[west ? 3 : 1];
      // the ridge binding: the pole under the cloth catches the light along its length
      if (u < 0.07f) col = west ? lighten(F[4], 0.25f) : F[3];
      body.set(x, row, col);
      // the low wall (valance) under the eave on both long sides at the front-most depth only is the gable; the sides'
      // eave edges are seen from above: nothing more
    }
  // the front gable (a vertical triangle facing the viewer, mid-tone) with the door
  const int apexY = (int)std::lround(yf - ridge);
  for (int y = apexY; y <= yf; y++) {
    const float t = (float)(y - apexY) / (float)(yf - apexY);
    const int half = (int)std::lround(t * (hw - 0.5f));
    for (int x = cx - half; x <= cx + half; x++) {
      int k = 2;
      if (y > yf - 2) k = 1;                    // the foot in its own shade
      uint32_t col = F[k];
      if (x == cx - half || x == cx + half) col = F[x < cx ? 1 : 0];   // the gable's edge: the front rafters under the cloth
      if (y >= yf - (int)eave && y <= yf - (int)eave + 1) col = T[2];   // the hem runs across the front
      body.set(x, y, col);
    }
  }
  // the door: a dark slit opening from under the apex, its flaps folded back and tied
  const int dTop = apexY + 6;
  for (int y = dTop; y <= yf; y++) {
    const float t = (float)(y - dTop) / (float)(yf - dTop);
    const int half = (int)std::lround(t * 5.5f);
    for (int x = cx - half; x <= cx + half; x++) {
      const float depth = (float)(y - dTop) / (yf - dTop);
      uint32_t col = mix(kInk, rgba(70, 48, 40), depth * 0.6f);
      if (y >= yf - 1) col = rgba(96, 74, 50);            // straw on the floor inside
      body.set(x, y, col);
    }
    // the folded flaps: a lit fold on the left, a shaded one on the right
    if (y > dTop + 2) {
      body.set(cx - half - 1, y, F[4]); body.set(cx - half - 2, y, F[3]);
      body.set(cx + half + 1, y, F[1]); body.set(cx + half + 2, y, F[2]);
    }
  }
  body.set(cx - 6, yf - 6, T[3]); body.set(cx + 6, yf - 6, T[1]);   // the ties
  // the ridge pole's end above the apex, and its pennant
  for (int y = apexY - 5; y <= apexY; y++) { body.set(cx, y, kWood[y < apexY - 3 ? 3 : 2]); }
  body.set(cx, apexY - 6, kBrass[3]);
  outline(body, 0.85f);
  pennant(body, cx, apexY - 6, F, T, 9, seed);
  // guy ropes from the eave corners and the ridge ends to their pegs
  const uint32_t rope = mix(kCloth[2], kCloth[1], 0.4f);
  Canvas ropes(W, c.h);
  groundShadow(ropes, sh, 70);
  blit(c, ropes, 0, 0);
  // back ropes first (behind the tent)
  guyRope(c, cx - hw + 1, yb - eave, cx - hw - 6, yb - 1, rope);
  guyRope(c, cx + hw - 1, yb - eave, cx + hw + 6, yb - 1, rope);
  guyRope(c, cx, yb - ridge + 1, cx, yb - ridge - 3, rope);
  blit(c, body, 0, 0);
  guyRope(c, cx - hw + 1, yf - eave, cx - hw - 7, yf + 2, rope);
  guyRope(c, cx + hw - 1, yf - eave, cx + hw + 7, yf + 2, rope);
  tufts(c, cx - (int)hw - 2, cx + (int)hw + 4, yf + 2, seed, 0.35f);
}

// ---------------------------------------------------------------- the command pavilion (3 x 2)
// A round pavilion: a drum wall of vertical cloth panels (field and trim by turns) lit from the left, a scalloped
// valance, a cone of gores rising to a crown where the commander's banner flies; the entrance's flaps tied back on a
// dark inside with a rug's edge; ropes radiate to pegs.
void commandTent(Canvas& c, const Ramp& F, const Ramp& T, uint32_t seed) {
  const int W = c.w, cx = W / 2;
  const float rx = 21.0f, ry = 10.5f;
  const int gy0 = c.h - 8 - (int)ry;      // the ground centre of the drum
  const float wallH = 13.0f, coneH = 20.0f;
  std::vector<Vec2> sh;
  for (int k = 0; k <= 16; k++) {
    const float a = k / 16.0f * 6.2832f;
    sh.push_back(V(cx + 7 + std::cos(a) * (rx + 2), gy0 + 3 + std::sin(a) * (ry + 1)));
  }
  Canvas body(W, c.h);
  // the drum wall
  for (int x = (int)(cx - rx); x <= (int)(cx + rx); x++) {
    const float u = (x + 0.5f - cx) / rx;
    if (std::fabs(u) > 1) continue;
    const float s = std::sqrt(std::max(0.0f, 1 - u * u));
    const int front = (int)std::lround(gy0 + ry * s);
    const int top = (int)std::lround(front - wallH);
    const float l = lightAt(std::clamp(u * 0.95f, -0.95f, 0.95f), 0.05f);
    // panel index by the angle round the drum (eight panels a half turn)
    const float ang = std::asin(std::clamp(u, -1.0f, 1.0f));
    const int panel = (int)std::floor((ang + 1.5708f) / 3.14159f * 8.0f);
    const bool trimP = (panel & 1) != 0;
    const float edge = std::fmod((ang + 1.5708f) / 3.14159f * 8.0f, 1.0f);
    for (int y = top; y <= front; y++) {
      int k = lightIndex(l, x, y, 0.08f);
      if (y > front - 2) k = std::max(0, k - 1);
      if (edge < 0.08f || edge > 0.94f) k = std::max(0, k - 1);   // the seams between panels
      body.set(x, y, trimP ? T[std::min(4, k)] : F[k]);
    }
  }
  // the entrance: a tall opening in the front panel, flaps tied back, the dark inside with a rug's edge
  {
    const int front = gy0 + (int)ry;
    for (int y = front - 11; y <= front; y++) {
      const int half = y < front - 8 ? (y - (front - 11)) + 1 : 4;
      for (int x = cx - half; x <= cx + half; x++) body.set(x, y, y >= front - 1 ? rgba(150, 40, 48) : mix(kInk, rgba(72, 46, 44), (float)(y - (front - 11)) / 12.0f));
      if (y > front - 9) { body.set(cx - half - 1, y, F[4]); body.set(cx + half + 1, y, F[1]); body.set(cx - half - 2, y, F[3]); body.set(cx + half + 2, y, F[2]); }
    }
    body.set(cx - 6, front - 7, T[4]); body.set(cx + 6, front - 7, T[1]);
    for (int x = cx - 3; x <= cx + 3; x++) if ((x & 1) == 0) body.set(x, front, kGold[3]);   // the rug's fringe
  }
  // the cone of gores to the crown (painted over the drum's top edge)
  const float apexX = cx, apexY = gy0 - wallH - coneH;
  const float eaveY = gy0 - wallH;
  for (int y = (int)apexY; y <= (int)(eaveY + ry + 1); y++)
    for (int x = (int)(cx - rx - 3); x <= (int)(cx + rx + 3); x++) {
      // the cone's surface over the eave ellipse: the gauge t (0 the crown, 1 the eave) along the line from the apex
      const float px = x + 0.5f - apexX, py = y + 0.5f - apexY;
      // find t such that (px/t, py/t) lies on the eave ellipse below the apex: (px/(t rx'))^2 + ((py - t coneH)/(t ry'))^2 = 1
      float lo = 0.02f, hi = 1.25f, t = -1;
      for (int it = 0; it < 18; it++) {
        const float m = (lo + hi) * 0.5f;
        const float ex = px / (m * (rx + 2.5f)), ey = (py - m * coneH) / (m * (ry + 1.0f));
        if (ex * ex + ey * ey > 1) lo = m; else hi = m;
      }
      t = hi;
      if (t > 1.0f) continue;
      const float ex = px / (t * (rx + 2.5f)), ey = (py - t * coneH) / (t * (ry + 1.0f));
      if (ey < -0.15f) continue;   // the back of the cone is hidden behind its front
      const float ang = std::atan2(ey, ex);   // round the cone (0 east, pi/2 front, pi west)
      const float u = std::cos(ang);
      float l = lightAt(std::clamp(u * 0.85f, -0.95f, 0.95f), -0.42f + 0.25f * (1 - t));
      l += 0.15f * (1 - t);
      int k = lightIndex(l, x, y, 0.08f);
      const float g = (ang / 3.14159f) * 8.0f;
      const int gore = (int)std::floor(g + 16);
      const float ge = g - std::floor(g);
      if (ge < 0.1f) k = std::max(0, k - 1);
      uint32_t col = (gore & 1) ? T[std::min(4, k)] : F[k];
      // the scalloped valance round the eave: a band in the trim colour with dips between the gores
      const float sc = 1.0f - std::fabs(ge - 0.5f) * 2.0f;
      if (t > 0.88f - 0.05f * sc && t <= 1.0f) col = T[std::max(0, k - (t > 0.97f ? 1 : 0))];
      body.set(x, y, col);
    }
  // the crown: a gilt boss and the banner pole
  const int ax = (int)apexX, ay = (int)apexY;
  body.set(ax, ay, kGold[3]); body.set(ax - 1, ay, kGold[4]); body.set(ax + 1, ay, kGold[2]);
  for (int y = ay - 12; y < ay; y++) { body.set(ax, y, kWood[y < ay - 9 ? 4 : 3]); body.set(ax + 1, y, kWood[1]); }
  body.set(ax, ay - 13, kGold[4]);
  outline(body, 0.85f);
  // the banner: hanging from a crossbar, field with the trim's stripe and a swallowtail, a little wind in it
  {
    const int bx = ax + 2, by = ay - 12, bw = 9, bh = 12;
    for (int x = bx - 1; x <= bx + bw; x++) body.set(x, by, kWood[3]);
    for (int y = 1; y <= bh; y++)
      for (int x = 0; x < bw; x++) {
        const int wave = (int)std::lround(std::sin(y * 0.7f + seed % 5) * 0.6f + x * 0.12f);
        if (y > bh - 3 && std::abs(x - bw / 2) < (y - (bh - 3))) continue;   // the swallowtail
        int k = x < 2 ? 3 : (x > bw - 3 ? 1 : 2);
        uint32_t col = F[k];
        if (x >= bw / 2 - 1 && x <= bw / 2) col = T[k + 1];   // the charge: a pale stripe down the middle
        body.set(bx + x, by + y + wave, col);
      }
    // outline the banner too (it was painted after the first pass)
    Canvas b2 = body;
    outline(b2, 0.6f);
    for (int y = by - 1; y <= by + bh + 3; y++)
      for (int x = bx - 2; x <= bx + bw + 2; x++) if (!solid(body, x, y) && solid(b2, x, y)) body.set(x, y, b2.get(x, y));
  }
  Canvas under(W, c.h);
  groundShadow(under, sh, 72);
  blit(c, under, 0, 0);
  const uint32_t rope = mix(kCloth[2], kCloth[1], 0.4f);
  // ropes from the eave to the pegs, the back ones first
  for (int k = 0; k < 8; k++) {
    const float a = (k + 0.5f) / 8.0f * 6.2832f;
    const float ex = cx + std::cos(a) * (rx + 2), ey = eaveY + std::sin(a) * (ry + 1) + 1;
    const float px2 = cx + std::cos(a) * (rx + 9), py2 = gy0 + std::sin(a) * (ry + 5) + 1;
    if (std::sin(a) < 0) guyRope(c, ex, ey, px2, py2, rope);
  }
  blit(c, body, 0, 0);
  for (int k = 0; k < 8; k++) {
    const float a = (k + 0.5f) / 8.0f * 6.2832f;
    if (std::sin(a) < 0 || std::fabs(std::cos(a)) < 0.4f) continue;   // (none across the door)
    const float ex = cx + std::cos(a) * (rx + 2), ey = eaveY + std::sin(a) * (ry + 1) + 1;
    const float px2 = cx + std::cos(a) * (rx + 8), py2 = gy0 + std::sin(a) * (ry + 4) + 1;
    guyRope(c, ex, ey, px2, py2, rope);
  }
  tufts(c, cx - (int)rx - 4, cx + (int)rx + 6, gy0 + (int)ry + 3, seed, 0.3f);
}

// ---------------------------------------------------------------- the catapult (3 x 2)
// A torsion mangonel seen side-on (it throws east or west, so its arm swings across the picture, the clearest way to
// read it from above): a long timber bed on four plank wheels; on each side an A-frame rising to the padded crossbar the
// arm strikes; between them the twisted skein that powers it; the arm cocked back, low, to the rear, its cup holding a
// stone, the winch rope taut to the drum at the tail. Beam tops catch the light; the far frame and wheels sit in shade;
// iron straps at the joints; shot heaped beside it.
void catapult(Canvas& c, const Ramp& F, const Ramp& T, uint32_t seed) {
  (void)T;
  const int W = c.w, base = c.h - 7;
  const int x0 = 8, x1 = W - 12;                 // the bed's ends
  const int frontY = base - 1, backY = base - 9; // ground rows of the near and far sides
  const int bedH = 8;                            // the bed's height off the ground (on the wheels)
  Canvas b(W, c.h);
  // a squared beam: its top face lit, its side face mid, its underside edge dark (th rows from the top)
  auto beam = [&](float ax, float ay, float bx, float by, int th, int bias) {
    const int n = (int)std::ceil(std::max(std::fabs(bx - ax), std::fabs(by - ay)));
    for (int i = 0; i <= n; i++) {
      const float t = n ? (float)i / n : 0;
      const int x = (int)std::lround(ax + (bx - ax) * t), y = (int)std::lround(ay + (by - ay) * t);
      for (int j = 0; j < th; j++) {
        int k = j == 0 ? 4 : (j == th - 1 ? 1 : (j == 1 ? 3 : 2));
        k = std::clamp(k + bias, 0, 4);
        uint32_t col = kWood[k];
        if (j > 0 && ((x * 7 + y * 3 + j) % 13) == 0) col = kWood[std::max(0, k - 1)];   // the grain
        b.set(x, y + j, col);
      }
    }
  };
  // an upright post: lit left column, shaded right
  auto post = [&](float ax, float ay, float bx, float by, int bias) {
    const int n = (int)std::ceil(std::max(std::fabs(bx - ax), std::fabs(by - ay)));
    for (int i = 0; i <= n; i++) {
      const float t = n ? (float)i / n : 0;
      const int x = (int)std::lround(ax + (bx - ax) * t), y = (int)std::lround(ay + (by - ay) * t);
      b.set(x, y, kWood[std::clamp(3 + bias, 0, 4)]); b.set(x + 1, y, kWood[std::clamp(2 + bias, 0, 4)]); b.set(x + 2, y, kWood[std::clamp(1 + bias, 0, 4)]);
    }
  };
  // a plank wheel side-on: a disc, its iron tyre, the plank joints and the hub
  auto wheel = [&](float wx, float wy, float r, int bias) {
    for (int y = (int)(wy - r - 1); y <= (int)(wy + r + 1); y++)
      for (int x = (int)(wx - r - 1); x <= (int)(wx + r + 1); x++) {
        const float dx = (x + 0.5f - wx) / r, dy = (y + 0.5f - wy) / r;
        const float d = dx * dx + dy * dy;
        if (d > 1) continue;
        int k = lightIndex(lightAt(dx * 0.55f, dy * 0.55f), x, y, 0.1f) + bias;
        uint32_t col = kWood[std::clamp(k, 0, 4)];
        if (d > 0.7f) col = kIron[std::clamp(k - (d > 0.88f ? 1 : 0), 0, 4)];
        else if (std::fabs(dy) < 0.09f || std::fabs(dx) < 0.09f) col = kWood[std::clamp(k - 1, 0, 4)];   // the planks' joints
        b.set(x, y, col);
      }
    b.set((int)wx, (int)wy, kIron[4]); b.set((int)wx + 1, (int)wy, kIron[2]); b.set((int)wx, (int)wy + 1, kIron[1]);
  };
  const int crossY = frontY - 33;      // the crossbar on the near A-frame
  const int ax = (x0 + x1) / 2 - 3;    // where the A-frames stand (and the skein)
  // ---- the far side: wheels, bed rail, A-frame (in shade)
  wheel(x0 + 6, backY - 5, 5.5f, -1);
  wheel(x1 - 6, backY - 5, 5.5f, -1);
  beam(x0, backY - bedH - 3, x1, backY - bedH - 3, 4, -1);
  post(ax - 9, backY - bedH - 3, ax - 1, crossY - 7, -1);
  post(ax + 9, backY - bedH - 3, ax + 1, crossY - 7, -2);
  // the crossbar running across between the two frames (seen side-on: the end of a beam, a short dark run)
  for (int y = crossY - 8; y <= crossY; y++) { b.set(ax - 1, y, kWood[3]); b.set(ax, y, kWood[2]); b.set(ax + 1, y, kWood[2]); b.set(ax + 2, y, kWood[1]); }
  // the stop's pad in the kingdom's cloth
  for (int y = crossY - 9; y <= crossY - 6; y++) for (int x = ax - 2; x <= ax + 3; x++) b.set(x, y, F[x < ax ? 4 : (x > ax + 1 ? 2 : 3)]);
  // the cross timbers of the bed (their tops lit)
  for (int k = 0; k < 4; k++) {
    const int xx = x0 + 3 + k * (x1 - x0 - 6) / 3;
    beam((float)xx, (float)(backY - bedH - 2), (float)xx, (float)(frontY - bedH - 3), 3, 0);
  }
  // ---- the skein and the arm, cocked back to the tail, the cup and its stone on the rear cushion
  const int skX = ax, skY = (frontY + backY) / 2 - bedH - 4;
  for (int y = skY - 3; y <= skY + 3; y++)
    for (int x = skX - 3; x <= skX + 4; x++) {
      const float dx = (x + 0.5f - (skX + 0.5f)) / 4.0f, dy = (y + 0.5f - skY) / 3.5f;
      if (dx * dx + dy * dy > 1) continue;
      b.set(x, y, kCloth[((x + y * 2) % 3 == 0) ? 1 : (dx < 0 ? 3 : 2)]);   // the twisted rope
    }
  const float cupX = x1 - 5.0f, cupY = skY - 6.0f;
  {
    const int n = (int)std::ceil(std::hypot(cupX - skX, cupY - skY));
    for (int i = 0; i <= n; i++) {
      const float t = (float)i / n;
      const int x = (int)std::lround(skX + (cupX - skX) * t), y = (int)std::lround(skY + (cupY - skY) * t);
      b.set(x, y - 1, kWood[4]); b.set(x, y, kWood[3]); b.set(x, y + 1, kWood[2]); b.set(x, y + 2, kWood[1]);
      if (i % 8 == 4) { b.set(x, y, kIron[3]); b.set(x, y + 1, kIron[2]); b.set(x, y + 2, kIron[1]); }   // iron bands
    }
    // the cushion block under the cup
    beam(cupX - 3, cupY + 3, cupX + 3, cupY + 3, 3, -1);
    // the cup: a wooden bowl, its rim lit, the stone in it
    for (int y = (int)cupY - 3; y <= (int)cupY + 2; y++)
      for (int x = (int)cupX - 4; x <= (int)cupX + 4; x++) {
        const float dx = (x + 0.5f - cupX) / 4.5f, dy = (y + 0.5f - cupY) / 2.8f;
        if (dx * dx + dy * dy > 1) continue;
        b.set(x, y, dy < -0.3f ? kWoodDark[1] : kWood[dx < -0.2f ? 3 : (dx > 0.4f ? 1 : 2)]);
      }
    ball(b, cupX, cupY - 4, 3.6, 3.4, kStone);
    // the winch rope from the arm to the drum at the tail
    line(b, (int)cupX + 2, (int)cupY + 1, x1 + 3, frontY - bedH - 6, kCloth[1]);
  }
  // ---- the near side: A-frame, bed rail, wheels (in the light)
  post(ax - 10, frontY - bedH - 3, ax - 1, crossY + 1, 0);
  post(ax + 10, frontY - bedH - 3, ax + 1, crossY + 1, -1);
  beam(ax - 6, crossY + 13, ax + 7, crossY + 13, 3, 0);   // the A's tie beam
  b.set(ax, crossY, kIron[3]); b.set(ax + 1, crossY, kIron[2]);
  beam(x0 - 1, frontY - bedH - 3, x1 + 1, frontY - bedH - 3, 4, 0);
  for (int x = x0 + 3; x < x1; x += 10) { b.set(x, frontY - bedH - 2, kIron[3]); b.set(x, frontY - bedH - 1, kIron[2]); b.set(x, frontY - bedH, kIron[1]); }
  // the winch drum and its spokes at the tail
  for (int y = frontY - bedH - 8; y <= frontY - bedH - 3; y++) { b.set(x1 + 2, y, kWood[3]); b.set(x1 + 3, y, kWood[2]); b.set(x1 + 4, y, kWood[1]); }
  thickLine(b, x1 + 3, frontY - bedH - 6, x1 + 7, frontY - bedH - 11, 1.0f, kWood[3]);
  thickLine(b, x1 + 3, frontY - bedH - 6, x1 + 8, frontY - bedH - 3, 1.0f, kWood[2]);
  wheel(x0 + 5, frontY - 6, 6.5f, 0);
  wheel(x1 - 5, frontY - 6, 6.5f, 0);
  outline(b, 0.85f);
  // shot heaped beside it
  Canvas shot(W, c.h);
  ball(shot, 4.5, base + 2, 2.7, 2.4, kStone);
  ball(shot, 8.5, base + 3, 2.7, 2.4, kStone, 0.1f, -1);
  ball(shot, 6.5, base, 2.5, 2.2, kStone, 0.1f, 1);
  outline(shot, 0.7f);
  std::vector<Vec2> sh = {V(x0 - 1, frontY + 1), V(x1 + 6, frontY + 1), V(x1 + 16, backY - 3), V(ax + 18, crossY + 22), V(x0 + 4, backY - 2)};
  Canvas under(W, c.h);
  groundShadow(under, sh, 62);
  blit(c, under, 0, 0);
  blit(c, b, 0, 0);
  blit(c, shot, 0, 0);
  tufts(c, 2, W - 2, base + 4, seed, 0.3f);
}

// ---------------------------------------------------------------- the stake palisade (1 x 1, joins its neighbours)
// Four sharpened logs across the tile's whole width (so a run of tiles is one continuous fence, and the corner tile a
// run's diagonal step gets joins it without a gap), each a little cylinder lit from the left, of a slightly different
// height and lean, its point shaved pale; they stand in a ridge of thrown-up earth that spans the tile. No band runs
// across them, so a north-south run (tiles stacked up the screen) reads as stakes behind stakes, not as stripes.
void palisade(Canvas& c, uint32_t seed) {
  const int base = c.h - 4;
  static const int hts[4] = {19, 22, 18, 21};
  static const int lean[4] = {0, 1, 0, -1};   // the tip's x offset (a hand-cut fence is never quite straight)
  for (int k = 0; k < 4; k++) {
    const int x0 = k * 4, h = hts[k], top = base - h;
    for (int y = top; y <= base; y++) {
      const float t = (float)(base - y) / (float)h;   // 0 at the foot, 1 at the tip
      const int sh = (int)std::lround(lean[k] * t);
      for (int i = 0; i < 4; i++) {
        const int x = x0 + i + sh;
        if (y < top + 3) {   // the point: shaved pale wood, narrowing
          const int w = y - top;   // 0 at the tip
          if ((w == 0 && i != 1 && i != 2) || (w == 1 && i == 3)) continue;
          c.set(x, y, kWood[i == 0 ? 4 : (i == 1 ? 4 : (i == 2 ? 3 : 2))]);
          continue;
        }
        int kk = i == 0 ? 3 : (i == 1 ? 2 : (i == 2 ? 1 : 0));
        uint32_t col = kBark[kk + (i == 0 ? 1 : 0)];
        if (((y * 3 + k * 5 + i) % 9) == 0 && i > 0) col = kBark[std::max(0, kk - 1)];   // bark knots and cracks
        if (i == 3 && ((y + k) & 1)) col = kBark[0];
        c.set(x, y, col);
      }
    }
    // the shaved ring where the point begins
    c.set(x0 + 1 + (int)std::lround(lean[k] * (1.0f - 3.0f / h)), top + 3, kWood[3]);
  }
  // the earth ridge at the foot (spans the tile so neighbours join), lit on its crest, grass on top here and there
  for (int x = 0; x < 16; x++) {
    const int h = 3 + (int)(hash3(x, 5, seed) % 2);
    for (int y = base - h + 1; y <= base + 3; y++) {
      int k = y <= base - h + 1 ? 3 : (y > base + 1 ? 1 : 2);
      c.set(x, y, kSoil[k]);
    }
    if (hash3(x, 9, seed) % 3 == 0) c.set(x, base - h, kLeaf[2]);
  }
}

// ---------------------------------------------------------------- the refugee lean-to (3 x 2)
// A lean-to of patched blankets: a ridge pole on two forked sticks at the back, the blanket thrown over it and pulled
// forward and down to short stakes at the front, so its quilted face slopes toward the viewer and a dark low mouth opens
// under its hem (a bedroll, a foot inside). The patches are mismatched and faded (one in the field colour: the refuge's
// lord gave them that much), stitched with pale thread; the cloth sags between its supports and drapes in folds; a
// second blanket hangs down the east end as a windbreak; bundles and a pot stand beside it.
void refugeeTent(Canvas& c, uint32_t field, uint32_t seed) {
  const int W = c.w, base = c.h - 5;
  const int x0 = 8, x1 = W - 13;
  const int hemY = base - 4;                     // the hem's row (4 px up on its stakes)
  const int ridgeY = base - 29;                  // the ridge pole's row
  static const uint32_t patchCols[8] = {rgba(124, 92, 70), rgba(104, 110, 120), rgba(150, 122, 84), rgba(96, 112, 84),
                                        rgba(142, 88, 72), rgba(116, 98, 126), rgba(164, 150, 118), rgba(88, 96, 108)};
  // the patches: Voronoi cells over the blanket (u across, v down), so their edges run every which way
  struct Cell { float u, v; uint32_t col; };
  std::vector<Cell> cells;
  for (int k = 0; k < 9; k++) {
    Cell q;
    q.u = hashf(k, 1, seed); q.v = hashf(k, 2, seed);
    q.col = patchCols[hash3(k, 3, seed) % 8];
    cells.push_back(q);
  }
  if (field) cells[2].col = opaque(field);
  Canvas b(W, c.h);
  // the forked poles at the back
  for (int y = ridgeY - 2; y <= base - 12; y++) { b.set(x0, y, kWood[2]); b.set(x0 + 1, y, kWood[1]); b.set(x1, y, kWood[2]); b.set(x1 + 1, y, kWood[1]); }
  b.set(x0 - 1, ridgeY - 3, kWood[3]); b.set(x0 + 2, ridgeY - 3, kWood[2]); b.set(x1 - 1, ridgeY - 3, kWood[3]); b.set(x1 + 2, ridgeY - 3, kWood[2]);
  // the dark mouth under the hem: the shelter's inside, a bedroll's end and a foot
  for (int y = hemY; y <= base; y++)
    for (int x = x0 - 1; x <= x1 + 1; x++) b.set(x, y, mix(rgba(30, 22, 28), rgba(58, 44, 40), (float)(y - hemY) / 5.0f));
  for (int x = x0 + 6; x <= x0 + 13; x++) { b.set(x, base - 1, rgba(120, 104, 128)); b.set(x, base, rgba(90, 76, 98)); }
  b.set(x1 - 7, base, rgba(150, 110, 84)); b.set(x1 - 6, base, rgba(130, 92, 70));
  // the front stakes holding the hem
  for (int y = hemY; y <= base; y++) { b.set(x0 + 1, y, kWood[3]); b.set(x1 - 1, y, kWood[2]); }
  // (fixer M4 r1, review: "flat brown boxes seen straight on") the blanket reads as a slope in the 3/4 view: it splays
  // wider toward the hem (the near, low edge) than at the ridge, it is lit from the ridge down (the cloth turns over the
  // pole into the top-left light, the hem in shade), and the open west end shows the dark wedge of the shelter's inside
  // under it, its lit pole edge in front
  const int span = hemY - 1 - ridgeY;
  for (int y = ridgeY + 2; y <= hemY; y++) {   // the open west end: the dark inside, widening toward the ground
    const float v = std::clamp((float)(y - ridgeY) / (float)span, 0.0f, 1.0f);
    const int xl = x0 - 1 - (int)std::lround(3.0f * v);
    const int xo = xl - (int)std::lround(5.0f * v);
    for (int x = xo; x < xl; x++) b.set(x, y, x == xo ? kWood[2] : mix(rgba(30, 22, 28), rgba(66, 50, 44), v * 0.7f));
  }
  for (int y = ridgeY - 1; y <= hemY + 1; y++) {
    const float v = std::clamp((float)(y - ridgeY) / (float)span, 0.0f, 1.0f);
    const int xl = x0 - 1 - (int)std::lround(3.0f * v), xr = x1 + 1 + (int)std::lround(3.0f * v);
    for (int x = xl; x <= xr; x++) {
      const float u = (float)(x - xl) / (float)std::max(1, xr - xl);
      const int top = ridgeY + (int)std::lround(2.6f * std::sin(u * 3.14159f));                // the sag between the poles
      const int bot = hemY - 1 + (int)std::lround(std::sin(u * 17.0f + seed % 5) * 0.8f);      // the wavy hem
      if (y < top || y > bot) continue;
      if ((x == xl || x == xr) && ((x + y) & 1)) continue;   // the frayed side edges
      const float vv = (float)(y - top) / (float)std::max(1, bot - top);
      float d1 = 9, d2 = 9;
      int c1 = 0;
      for (int k = 0; k < (int)cells.size(); k++) {
        const float du = (u - cells[(size_t)k].u) * 1.3f, dv = vv - cells[(size_t)k].v, d = du * du + dv * dv;
        if (d < d1) { d2 = d1; d1 = d; c1 = k; } else if (d < d2) d2 = d;
      }
      const Ramp R = ramp(cells[(size_t)c1].col, 0.7f);
      float lt = 0.92f - vv * 0.78f - u * 0.16f;
      const float fold = std::sin(u * 21.0f + vv * 2.0f + (float)(seed % 3));
      lt += fold * 0.10f * (0.4f + vv);
      if (y <= top + 1) lt += 0.3f;   // over the pole
      int k = lt > 0.70f ? 4 : (lt > 0.44f ? 3 : (lt > 0.16f ? 2 : 1));
      uint32_t col = R[k];
      const float gap = std::sqrt(d2) - std::sqrt(d1);
      if (gap < 0.03f) col = R[std::max(0, k - 2)];                                 // the seam
      else if (gap < 0.05f && ((x + y) % 3) == 0) col = kCloth[3];                 // a running stitch beside it
      if (y == bot) col = R[0];
      b.set(x, y, col);
    }
  }
  for (int x = x0 - 4; x <= x1 + 4; x++) {
    const float u = (float)(x - (x0 - 4)) / (float)(x1 - x0 + 8);
    if (hash3(x, 7, seed) % 3 == 0) b.set(x, hemY + (int)std::lround(std::sin(u * 17.0f + seed % 5) * 0.8f), kCloth[1]);   // fringe
  }
  // the ridge pole's ends showing past the blanket
  b.set(x0 - 4, ridgeY, kWood[3]); b.set(x0 - 5, ridgeY, kWood[4]); b.set(x1 + 4, ridgeY + 1, kWood[2]); b.set(x1 + 5, ridgeY + 1, kWood[1]);
  // the windbreak down the east end: a darker blanket hanging from the pole's end to the ground
  for (int y = ridgeY + 2; y <= base; y++) {
    const int w = 2 + (y - ridgeY) / 9;
    for (int x = x1 + 4; x < x1 + 4 + w; x++) {
      int k = (x - (x1 + 4)) == 0 ? 2 : 1;
      if (((y + x) % 7) == 0) k = 0;
      b.set(x, y, ramp(rgba(98, 84, 74), 0.7f)[k]);
    }
  }
  outline(b, 0.8f);
  // bundles and a pot beside it
  Canvas side(W, c.h);
  ball(side, x1 + 10, base - 1, 3.6, 3.0, ramp(rgba(140, 116, 84), 0.7f));   // a bundle tied with cord
  vline(side, x1 + 10, base - 4, base + 2, kLeather[1]);
  ball(side, 4, base + 1, 2.6, 2.2, kIron);                                  // the pot
  hline(side, 2, 6, base - 1, kIron[1]);
  outline(side, 0.7f);
  std::vector<Vec2> sh = {V(x0 - 2, base + 1), V(x1 + 8, base + 1), V(x1 + 14, base - 7), V(x1 + 8, ridgeY + 10), V(x0 + 4, ridgeY + 12)};
  Canvas under(W, c.h);
  groundShadow(under, sh, 58);
  blit(c, under, 0, 0);
  blit(c, b, 0, 0);
  blit(c, side, 0, 0);
  tufts(c, 1, W - 2, base + 2, seed, 0.3f);
}

// ---------------------------------------------------------------- rubble (1 x 1)
// A heap of fallen stone where a wall stood: squared blocks tumbled every way (each with its lit top, its face, its
// dark side), broken roof tiles, two charred beams jutting out of the heap, soot on the stones, an ember still red in a
// crack; grit spilled round the foot.
void stoneBlock(Canvas& dst, int x, int y, int w, int h, int top, int skew, const Ramp& R, uint32_t seed) {
  Canvas b(dst.w, dst.h);
  // the front face
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) {
      int k = j == h - 1 ? 1 : 2;
      if (hash3(i, j, seed) % 7 == 0) k = std::max(0, k - 1);
      b.set(x + i, y + j, R[k]);
    }
  // the top face (a parallelogram, skewed as the block lies), brightest along its back edge
  for (int r = 1; r <= top; r++) {
    const int off = (skew * r + top / 2) / std::max(1, top);
    for (int i = 0; i < w; i++) b.set(x + i + off, y - r, R[r == top ? 4 : 3]);
  }
  // the turned block's east side, in shade
  for (int s = 0; s < skew; s++)
    for (int j = -top + (skew - s) * top / std::max(1, skew); j < h - s; j++) b.set(x + w + s, y + j, R[s == 0 ? 1 : 0]);
  outline(b, 0.7f);
  blit(dst, b, 0, 0);
}
void rubble(Canvas& c, uint32_t seed) {
  const int W = c.w, base = c.h - 4;
  Canvas b(W, c.h);
  // the charred beams, behind the stones
  thickLine(b, 2, (float)(base - 13), 15, (float)(base - 4), 2.4f, rgba(40, 28, 30));
  thickLine(b, 2.5f, base - 13.8f, 15, base - 4.8f, 0.9f, rgba(84, 60, 50));
  thickLine(b, (float)(W - 3), (float)(base - 15), (float)(W - 11), (float)(base - 3), 2.2f, rgba(36, 26, 28));
  thickLine(b, W - 3.5f, base - 15.6f, (float)(W - 11), base - 3.6f, 0.8f, rgba(78, 56, 48));
  b.set(W - 3, base - 15, kFire[2]);
  outline(b, 0.7f);
  // spilled grit and broken tiles at the foot
  for (int x = 1; x < W - 1; x++)
    for (int y = base - 2; y <= base + 2; y++)
      if (hash3(x, y, seed) % 3 == 0 && std::abs(x - W / 2) < W / 2 - (y - base + 2))
        b.set(x, y, hash3(x, y, seed + 1) % 3 == 0 ? rgba(150, 82, 62) : kStone[1 + (int)(hash3(x, y, seed + 2) % 2)]);
  // the stones, back to front, each tumbled its own way
  stoneBlock(b, 5, base - 9, 6, 4, 3, 1, kStoneWarm, seed + 1);
  stoneBlock(b, 13, base - 10, 7, 4, 3, 2, kStone, seed + 2);
  stoneBlock(b, 9, base - 13, 5, 3, 2, 1, kStone, seed + 3);
  stoneBlock(b, 2, base - 4, 7, 4, 3, 1, kStone, seed + 4);
  stoneBlock(b, 11, base - 4, 6, 4, 2, 2, kStoneWarm, seed + 5);
  stoneBlock(b, 18, base - 5, 5, 4, 2, 1, kStoneWarm, seed + 6);
  // soot on the stones' tops
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < W; x++)
      if (solid(b, x, y) && vnoise(x * 0.4f, y * 0.4f, seed) > 0.64f) b.set(x, y, darken(b.get(x, y), 0.5f));
  b.set(10, base - 5, kFire[2]); b.set(11, base - 5, kFire[1]);   // an ember in a crack
  std::vector<Vec2> sh = {V(2, base + 2), V(W - 2, base + 2), V(W, base - 5), V(W - 6, base - 11)};
  Canvas under(W, c.h);
  groundShadow(under, sh, 58);
  blit(c, under, 0, 0);
  blit(c, b, 0, 0);
}

// ---------------------------------------------------------------- ash (1 x 1, walked over, drawn flat)
// A scorch of ash and cinders that fades into the ground: a soft oval (not a blot), charcoal at its heart, greying
// outward in three steps that break up through an ordered dither at the rim, drifts of pale ash in it, a charred stick
// and an ember's glow. Translucent, so the ground's own texture shows through it.
void ash(Canvas& c, uint32_t seed) {
  const float cx = 8.0f, cy = 8.5f;
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      const float dx = (x + 0.5f - cx) / 7.8f, dy = (y + 0.5f - cy) / 6.4f;
      const float d = std::sqrt(dx * dx + dy * dy);
      const float n = vnoise(x * 0.3f, y * 0.3f, seed) - 0.5f;
      const float v = 1.0f - d + n * 0.35f;   // > 0 inside the scorch, larger toward its heart
      if (v <= 0) continue;
      const float s = v * 3.2f + (bayer(x, y) - 0.5f) * 0.8f;
      const int step = (int)std::floor(s);
      if (step <= 0) continue;   // the dithered rim
      uint32_t col;
      int a;
      if (step >= 3) { col = rgba(40, 34, 40); a = 200; }
      else if (step == 2) { col = rgba(66, 60, 62); a = 160; }
      else { col = rgba(96, 90, 86); a = 110; }
      // pale ash drifted in the scorch
      if (step >= 2 && vnoise(x * 0.7f + 3, y * 0.7f, seed + 5) > 0.68f) { col = rgba(176, 170, 160); a = 170; }
      c.set(x, y, withA(col, a));
    }
  // a charred stick across it and an ember
  c.set(5, 9, withA(rgba(26, 18, 20), 255)); c.set(6, 9, withA(rgba(48, 32, 30), 255)); c.set(7, 10, withA(rgba(26, 18, 20), 255));
  c.set(8, 10, withA(rgba(62, 42, 34), 255));
  c.set(10, 7, withA(kFire[1], 230));
}

// ---------------------------------------------------------------- scaffold (1 x 1, stands against a wall)
// Timber scaffolding for a house being rebuilt: two poles lashed with ledgers, a diagonal brace, a plank deck halfway
// up with a pail on it, a ladder leaning against the front; fresh pale timber against the soot.
void scaffold(Canvas& c, uint32_t seed) {
  (void)seed;
  const int W = c.w, base = c.h - 2;
  Canvas b(W, c.h);
  auto pole = [&](int x, int top) {
    for (int y = top; y <= base; y++) { b.set(x, y, kWood[3]); b.set(x + 1, y, kWood[1]); }
    b.set(x, top - 1, kWood[4]);
  };
  pole(2, 2);
  pole(W - 5, 4);
  // ledgers and the lashings at the joints
  for (int y = base - 9; y > 4; y -= 11) {
    for (int x = 1; x <= W - 3; x++) { b.set(x, y, kWood[4]); b.set(x, y + 1, kWood[2]); }
    b.set(2, y + 2, kCloth[1]); b.set(W - 5, y + 2, kCloth[1]);
  }
  // the diagonal brace
  line(b, 3, base - 3, W - 5, base - 19, kWood[2]);
  line(b, 3, base - 2, W - 5, base - 18, kWood[1]);
  // the plank deck halfway up, a pail on it
  const int deck = base - 20;
  for (int x = 0; x <= W - 2; x++) { b.set(x, deck, kWood[4]); b.set(x, deck + 1, kWood[3]); b.set(x, deck + 2, kWood[1]); }
  for (int x = 4; x < W - 4; x += 5) b.set(x, deck + 1, kWood[2]);
  ball(b, W - 8, deck - 2, 2.0, 1.8, kWoodDark);
  b.set(W - 8, deck - 4, kIron[2]);
  // the ladder against the front
  for (int y = deck + 3; y <= base; y++) { b.set(6, y, kWood[3]); b.set(10, y, kWood[2]); }
  for (int y = deck + 5; y <= base; y += 4) for (int x = 7; x <= 9; x++) b.set(x, y, kWood[4]);
  outline(b, 0.8f);
  std::vector<Vec2> sh = {V(2, base + 1), V(W - 2, base + 1), V(W + 4, base - 6), V(W, base - 10)};
  Canvas under(W, c.h);
  groundShadow(under, sh, 50);
  blit(c, under, 0, 0);
  blit(c, b, 0, 0);
}

// ---------------------------------------------------------------- the checkpoint's barricade (1 x 1)
// Two X-trestles of sharpened timber holding a barrier pole striped in the owner's colours, a lantern hook on one end;
// the stakes' points pale and fresh, the trestles lit on their west faces.
void barricade(Canvas& c, const Ramp& F, const Ramp& T, uint32_t seed) {
  const int W = c.w, base = c.h - 4;
  Canvas b(W, c.h);
  auto stake = [&](float x0, float y0, float x1, float y1, int bias) {
    thickLine(b, x0, y0, x1, y1, 2.2f, kWood[std::clamp(2 + bias, 0, 4)]);
    thickLine(b, x0 - 0.6f, y0 - 0.6f, x1 - 0.6f, y1 - 0.6f, 0.8f, kWood[std::clamp(3 + bias, 0, 4)]);
    b.set((int)std::lround(x1), (int)std::lround(y1), kWood[4]);   // the shaved point
  };
  // the far trestle, then the pole, then the near one
  stake(4, base, 11, base - 13, -1);
  stake(11, base, 4, base - 13, -1);
  stake(W - 11, base, W - 4, base - 13, -1);
  stake(W - 4, base, W - 11, base - 13, -1);
  // the barrier pole across, painted in bands of the owner's field and trim
  for (int x = 2; x <= W - 3; x++) {
    const bool band = ((x / 4) & 1) != 0;
    const Ramp& R = band ? T : F;
    b.set(x, base - 9, R[4]); b.set(x, base - 8, R[3]); b.set(x, base - 7, R[1]);
  }
  // the near legs of the trestles over the pole
  stake(7, base + 1, 10, base - 6, 0);
  stake(W - 8, base + 1, W - 11, base - 6, 0);
  // a lantern hook on the east end
  b.set(W - 3, base - 10, kIron[2]); b.set(W - 3, base - 11, kIron[3]);
  outline(b, 0.8f);
  std::vector<Vec2> sh = {V(3, base + 1), V(W - 2, base + 1), V(W + 3, base - 4), V(W - 3, base - 8), V(5, base - 6)};
  Canvas under(W, c.h);
  groundShadow(under, sh, 55);
  blit(c, under, 0, 0);
  blit(c, b, 0, 0);
  tufts(c, 1, W - 2, base + 1, seed, 0.3f);
}

}  // namespace

int warPropW(Prop p) { return wi(p).w; }
int warPropH(Prop p) { return wi(p).h; }
int warPropFrames(Prop p) { return wi(p).frames; }

void paintWarProp(Canvas& c, Prop p, int frame) {
  (void)frame;
  Canvas s = warPropSprite(p, 0, 0);
  for (int y = 0; y < c.h && y < s.h; y++)
    for (int x = 0; x < c.w && x < s.w; x++) c.set(x, y, s.get(x, y));
}

Canvas warPropSprite(Prop p, uint32_t field, uint32_t trim) {
  Canvas c(warPropW(p), warPropH(p));
  const uint32_t seed = 0x3A17u + (uint32_t)p * 977u;
  // the neutral camp (no owner): undyed linen with a russet trim
  const Ramp F = clothOf(field, kCloth);
  const Ramp T = trimOf(field, trim);
  switch (p) {
    case Prop::WarTent: warTent(c, F, T, seed); break;
    case Prop::CommandTent: commandTent(c, field ? F : kRed, field ? T : ramp(rgba(226, 204, 150), 0.7f), seed); break;
    case Prop::Catapult: catapult(c, field ? F : kRed, T, seed); break;
    case Prop::Palisade: palisade(c, seed); break;
    case Prop::RefugeeTent: refugeeTent(c, field, seed); break;
    case Prop::Rubble: rubble(c, seed); break;
    case Prop::Ash: ash(c, seed); break;
    case Prop::Scaffold: scaffold(c, seed); break;
    case Prop::Barricade: barricade(c, field ? F : kRed, field ? T : ramp(rgba(230, 220, 196), 0.6f), seed); break;
    default: break;
  }
  return c;
}

}  // namespace art
