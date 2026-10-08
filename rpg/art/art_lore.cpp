// EMBERVALE art (M4 Banners): the realm's voice and the lore of fallen places: NoticeBoard .. LostJournal in
// art_props.h. STORY lane.
// Painted in the shared language of rpg/art: the 3/4 top-down view (we see the tops of things and their south faces; a
// thing turned a little shows a sliver of its shaded east side), light from the top-left (lit tops and west edges,
// shaded east ones), hue-shifted ramps from art_internal.h, a soft cast shadow thrown down-right onto the ground, grass
// or grit anchoring each piece. The canvas sizes are the art's own; the tile footprints are frozen (m4Footprint: a
// prop stands on the bottom-centre tile of its footprint and its sprite is drawn bottom-centred on that tile).
//   NoticeBoard     a roofed timber board on two posts, pinned with notices (a wanted face, a sealed proclamation,
//                   torn and curling bills), nails glinting; 28 x 40
//   Inscription     a weathered standing slab, its face cut with lines of a dead script (chiselled grooves catch the
//                   light on their far edges), lichen and moss, a chipped crown, rubble at its foot; 18 x 34
//   ToppledStatue   a ruler's statue fallen off its cracked plinth, face up in the grass, the crowned head broken off
//                   and rolled aside; 3 tiles wide; 52 x 28
//   Mural           a fresco on a ruin's wall: a crowned figure under banners, faded, cracked, the plaster flaking to
//                   bare stone, its flakes on the floor below; hangs on the wall like a tapestry; 16 x 34
//   NamedGrave      a carved headstone with a name and a year, its grave a low mound in a stone kerb, dead flowers; 18 x 26
//   LostJournal     a satchel spilled on the floor: an open journal, its pages ruled with ink, a quill; 18 x 12
#include <algorithm>
#include <cstdlib>
#include "rpg/art/art_internal.h"

namespace art {
namespace {

struct LoreInfo { uint8_t w, h, frames; };
// NoticeBoard, Inscription, ToppledStatue, Mural, NamedGrave, LostJournal
const LoreInfo kLore[] = {{28, 40, 1}, {18, 34, 1}, {52, 28, 1}, {16, 34, 1}, {18, 26, 1}, {18, 12, 1}};
constexpr int kLoreN = (int)(sizeof(kLore) / sizeof(kLore[0]));
static_assert(kLoreN == (int)Prop::LostJournal + 1 - (int)Prop::NoticeBoard, "a size for every M4 lore prop");
inline const LoreInfo& li(Prop p) { return kLore[std::clamp((int)p - (int)Prop::NoticeBoard, 0, kLoreN - 1)]; }

// ---------------------------------------------------------------- shared bits
const Ramp kParch = ramp5(rgba(120, 92, 70), rgba(172, 146, 110), rgba(212, 192, 150), rgba(234, 220, 184), rgba(248, 240, 214));
const Ramp kEarth = ramp5(rgba(44, 30, 34), rgba(70, 48, 42), rgba(98, 70, 52), rgba(128, 96, 68), rgba(156, 124, 88));
const Ramp kPlaster = ramp5(rgba(110, 94, 92), rgba(160, 144, 130), rgba(198, 184, 160), rgba(222, 210, 186), rgba(240, 232, 210));
const Ramp kShingle = ramp5(rgba(46, 32, 40), rgba(74, 50, 46), rgba(104, 72, 54), rgba(134, 98, 68), rgba(166, 128, 88));
const uint32_t kInkCol = rgba(52, 40, 52);

// grass blades at the foot of a thing (anchors it to the ground)
void tufts(Canvas& c, int x0, int x1, int y, uint32_t seed, float density = 0.45f) {
  for (int x = x0; x <= x1; x++) {
    if (hashf(x, y, seed) > density || solid(c, x, y)) continue;
    const int h = 1 + (int)(hash3(x, 1, seed) % 3);
    for (int j = 0; j < h; j++) {
      const int xx = x + ((j == h - 1 && (x & 1)) ? 1 : 0);
      if (!solid(c, xx, y - j)) c.set(xx, y - j, kLeaf[j == h - 1 ? 3 : (j == 0 ? 1 : 2)]);
    }
  }
}
// the soft cast shadow on the ground (only where nothing stands): light from the top-left, so it falls down-right
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
// weathering: speckle a ramp-painted area darker / lighter by noise (only on pixels painted with the ramp's middle)
void weather(Canvas& c, int x0, int y0, int x1, int y1, const Ramp& R, uint32_t seed, float amount) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      const uint32_t p = c.get(x, y);
      if (!chA(p)) continue;
      int k = -1;
      for (int i = 0; i < 5; i++) if (p == R[i]) k = i;
      if (k < 1 || k > 3) continue;
      const float n = vnoise(x * 0.55f, y * 0.55f, seed) + (hashf(x, y, seed + 3) - 0.5f) * 0.35f;
      if (n > 1.0f - amount * 0.5f) c.set(x, y, R[k + 1]);
      else if (n < amount * 0.45f) c.set(x, y, R[k - 1]);
    }
}
// moss: soft green clumps where noise says so, inside a box, only over painted stone
void moss(Canvas& c, int x0, int y0, int x1, int y1, uint32_t seed, float cover) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      if (!solid(c, x, y)) continue;
      const float n = vnoise(x * 0.42f, y * 0.42f, seed) * 0.7f + hashf(x, y, seed + 9) * 0.3f;
      if (n < 1.0f - cover) continue;
      const bool topLit = !solid(c, x, y - 1) || !solid(c, x - 1, y);
      c.set(x, y, kMoss[n > 1.0f - cover * 0.45f ? (topLit ? 3 : 2) : 1]);
    }
}
// a chiselled groove: dark in the cut, the far (lower-right) lip catching the light from the top-left
void groove(Canvas& c, int x, int y, const Ramp& R) {
  c.set(x, y, R[0]);
  if (chA(c.get(x + 1, y)) && c.get(x + 1, y) != R[0]) c.set(x + 1, y, R[3]);
  if (chA(c.get(x, y + 1)) && c.get(x, y + 1) != R[0]) c.set(x, y + 1, R[4]);
}
// one rune of a dead script (3 wide, 4 high), from a seed: a stem and a branch or two
void rune(Canvas& c, int x, int y, uint32_t s, const Ramp& R) {
  const int stem = (int)(s % 3);
  for (int j = 0; j < 4; j++) groove(c, x + stem, y + j, R);
  const int kind = (int)((s >> 3) % 5);
  switch (kind) {
    case 0: groove(c, x + (stem == 0 ? 1 : stem - 1), y, R); groove(c, x + (stem == 2 ? 1 : stem + 1), y + 1, R); break;
    case 1: groove(c, x + (stem == 0 ? 1 : 0), y + 1, R); groove(c, x + (stem == 0 ? 2 : 2), y + 2, R); break;
    case 2: groove(c, x + (stem == 2 ? 1 : 2), y, R); groove(c, x + (stem == 2 ? 0 : 2), y + 2, R); break;
    case 3: groove(c, x + (stem == 1 ? 0 : 1), y + 1, R); groove(c, x + (stem == 1 ? 2 : 1), y + 1, R); break;
    default: groove(c, x + (stem == 0 ? 1 : stem - 1), y + 2, R); break;
  }
}

// ---------------------------------------------------------------- the notice board (28 x 40, 1 x 1, solid)
// Two squared posts carry a planked board under a little shingled roof; the board's face is to the south (the viewer)
// and the roof's top plane is seen from above. Notices of every age are pinned on it.
void noticeBoard(Canvas& c) {
  const int base = c.h - 3;               // the ground line under the posts
  const int bx0 = 3, bx1 = 24, by0 = 13, by1 = 29;
  Canvas b(c.w, c.h);
  // posts (squared timber: lit west face, shaded east face), sunk into the ground
  for (int px : {bx0 + 1, bx1 - 2})
    for (int y = by0 - 2; y <= base; y++) {
      b.set(px, y, kWood[y > base - 2 ? 2 : 3]);
      b.set(px + 1, y, kWood[y > base - 2 ? 0 : 1]);
    }
  // the board: three broad planks, a frame lip on top, its east edge in shade, its underside dark
  for (int y = by0; y <= by1; y++)
    for (int x = bx0; x <= bx1; x++) {
      const int py = (y - by0) % 6;
      int k = 2;
      if (py == 0) k = 3;                          // each plank's upper edge catches the light
      if (py == 5) k = 1;                          // the seam below it
      if (x == bx0) k = std::min(4, k + 1);        // the lit west edge
      if (x >= bx1 - 1) k = std::max(0, k - 1);    // the shaded east edge
      if (y == by1) k = 0;
      b.set(x, y, kWood[k]);
    }
  // grain and knots
  for (int y = by0 + 1; y < by1; y++)
    for (int x = bx0 + 1; x < bx1 - 1; x++)
      if (vnoise(x * 0.9f, y * 0.25f, 77) > 0.74f && b.get(x, y) == kWood[2]) b.set(x, y, kWood[1]);
  b.set(bx0 + 15, by0 + 9, kWoodDark[1]); b.set(bx0 + 16, by0 + 9, kWoodDark[2]);
  // the board's thickness: a sliver of its east side below the roof
  for (int y = by0; y <= by1; y++) b.set(bx1 + 1, y, kWoodDark[y == by0 ? 2 : 1]);
  // the roof: a shingled lean-to over the board; its top plane seen from above, the fascia below it, a deep eave
  const int ry0 = 3, ry1 = 10;                     // back ridge row .. front eave row
  for (int y = ry0; y <= ry1; y++) {
    const float t = (float)(y - ry0) / (ry1 - ry0);
    const int x0 = (int)std::lround(4 - t * 3.0f), x1 = (int)std::lround(23 + t * 3.0f);
    for (int x = x0; x <= x1; x++) {
      const int row = (y - ry0) / 2;
      const bool seam = (y - ry0) % 2 == 1 && ((x + row * 2) % 4 == 0);
      int k = t < 0.25f ? 4 : 3;                   // the back of the roof faces the sky: brightest
      if (x < x0 + 3) k = std::min(4, k + 1);       // the lit west end
      if (x > x1 - 3) k = std::max(1, k - 1);       // the shaded east end
      if ((y - ry0) % 2 == 1) k = std::max(0, k - 1);   // each course's lower lip
      if (seam) k = 0;
      b.set(x, y, kShingle[k]);
    }
  }
  for (int x = 1; x <= 26; x++) { b.set(x, ry1 + 1, kWoodDark[x < 4 ? 3 : 2]); b.set(x, ry1 + 2, kWoodDark[0]); }   // fascia + its shadow line
  // the eave's shadow across the top of the board
  for (int x = bx0; x <= bx1; x++) { b.set(x, by0, darken(b.get(x, by0), 0.55f)); if (x > bx0 + 2) b.set(x, by0 + 1, darken(b.get(x, by0 + 1), 0.25f)); }
  // ---- the notices
  auto paper = [&](int x0, int y0, int w, int h, int age, uint32_t seed, int curl) {
    for (int y = y0; y < y0 + h; y++)
      for (int x = x0; x < x0 + w; x++) {
        if (curl == 1 && x >= x0 + w - 2 && y >= y0 + h - 2 && (x - (x0 + w - 2)) + (y - (y0 + h - 2)) >= 1) continue;   // a curled corner
        if (curl == 2 && y == y0 + h - 1 && hash3(x, y, seed) % 2 == 0) continue;                                        // a torn foot
        int k = 3 - age;
        if (x == x0 || y == y0) k = std::min(4, k + 1);
        if (x == x0 + w - 1) k = std::max(0, k - 1);
        b.set(x, y, kParch[std::clamp(k, 0, 4)]);
      }
    if (curl == 1) { b.set(x0 + w - 2, y0 + h - 2, kParch[1]); b.set(x0 + w - 1, y0 + h - 2, kParch[4]); }
    // its little shadow on the board (right and below)
    for (int y = y0 + 1; y <= y0 + h; y++) if (!(curl == 1 && y >= y0 + h - 1)) b.set(x0 + w, y, darken(b.get(x0 + w, y), 0.45f));
    for (int x = x0 + 1; x < x0 + w; x++) if (curl != 2) b.set(x, y0 + h, darken(b.get(x, y0 + h), 0.35f));
    b.set(x0 + w / 2, y0, kIron[4]);   // the nail
  };
  // a wanted bill: a sketched face
  paper(bx0 + 2, by0 + 3, 7, 9, 0, 11, 0);
  ellipse(b, bx0 + 5.5f, by0 + 6.5f, 1.8f, 2.2f, kParch[1]);
  b.set(bx0 + 5, by0 + 6, kInkCol); b.set(bx0 + 6, by0 + 6, kInkCol); b.set(bx0 + 5, by0 + 8, kParch[0]); b.set(bx0 + 6, by0 + 8, kParch[0]);
  hline(b, bx0 + 3, bx0 + 7, by0 + 10, kInkCol);
  // the sealed proclamation, in the crown's red wax
  paper(bx0 + 10, by0 + 2, 6, 10, 0, 12, 0);
  for (int l = 0; l < 3; l++) for (int x = bx0 + 11; x < bx0 + 15; x += 1) if (hash3(x, l, 5) % 4) b.set(x, by0 + 4 + l * 2, kParch[1]);
  ellipse(b, bx0 + 13.0f, by0 + 10.0f, 1.4f, 1.2f, kRed[2]);
  b.set(bx0 + 12, by0 + 9, kRed[4]); b.set(bx0 + 13, by0 + 11, kRed[0]);
  hline(b, bx0 + 12, bx0 + 14, by0 + 12, kRed[1]); b.set(bx0 + 12, by0 + 13, kRed[1]); b.set(bx0 + 14, by0 + 13, kRed[1]);   // ribbon tails
  // an old bill, yellowed and curling
  paper(bx0 + 17, by0 + 4, 4, 7, 1, 13, 1);
  for (int l = 0; l < 3; l++) b.set(bx0 + 18 + (l & 1), by0 + 6 + l * 2, kParch[0]);
  // two scraps low on the board, one torn
  paper(bx0 + 4, by0 + 13, 5, 3, 1, 14, 2);
  paper(bx0 + 12, by0 + 14, 6, 2, 0, 15, 0);
  for (int x = bx0 + 13; x < bx0 + 17; x++) if (x & 1) b.set(x, by0 + 15, kParch[1]);
  outline(b, 0.7f);
  // stones at the posts' feet and grass
  for (int px : {bx0 + 1, bx1 - 2}) { ball(b, px - 0.5f, base + 0.5f, 1.5f, 1.0f, kStone, 0.0f); ball(b, px + 2.5f, base + 0.8f, 1.2f, 0.9f, kStone, 0.0f); }
  tufts(b, bx0 - 1, bx1 + 2, base + 1, 41, 0.5f);
  // the cast shadow: the board and its roof thrown down-right onto the ground
  castShadow(c, {V(bx0 + 1, base), V(bx1 + 1, base), V(c.w - 1, base + 2), V(bx0 + 6, base + 2)}, 70);
  blit(c, b, 0, 0);
}

// ---------------------------------------------------------------- the inscription (18 x 34, 1 x 1, solid)
void inscription(Canvas& c) {
  const int base = c.h - 4;
  const int x0 = 3, x1 = 12, top = 5;
  Canvas b(c.w, c.h);
  const Ramp& R = kStone;
  // the slab's top face (seen from above, brightest), its crown chipped away on the east
  for (int y = top - 2; y < top; y++)
    for (int x = x0 + (top - y) - 1; x <= x1 + 1 - (top - y); x++) b.set(x, y, R[4]);
  // the front face: lit west edge, shaded east edge, darker toward the foot (earth splash)
  for (int y = top; y <= base; y++)
    for (int x = x0; x <= x1; x++) {
      if (y < top + 3 && x > x1 - (top + 3 - y) * 2 + 1) continue;   // the broken crown
      int k = 2;
      if (x <= x0 + 1) k = 3;
      if (x >= x1) k = 1;
      if (y >= base - 2) k = std::max(0, k - 1);
      b.set(x, y, R[k]);
    }
  // the break: a ragged lit top surface along the missing corner
  for (int i = 0; i < 3; i++) { b.set(x1 - 5 + i * 2, top + i, R[4]); b.set(x1 - 4 + i * 2, top + i, R[3]); }
  // the slab's thickness: a sliver of its shaded east side (it stands a little turned)
  for (int y = top + 3; y <= base; y++) { b.set(x1 + 1, y - 1, R[1]); b.set(x1 + 2, y, R[0]); }
  weather(b, x0, top, x1, base, R, 913, 0.55f);
  // the dead script: five lines of runes, cut deep
  for (int l = 0; l < 5; l++)
    for (int g = 0; g < 2; g++) {
      const int rx = x0 + 1 + g * 4, ry = top + 4 + l * 5;
      if (ry + 4 > base - 2) continue;
      if (l == 0 && g == 1) continue;   // (the broken corner took the first line's end)
      rune(b, rx, ry, hash3(l, g, 4242), R);
    }
  // a crack down the face
  int cx = x0 + 6;
  for (int y = top + 6; y < base - 3; y++) { if (hash3(cx, y, 7) % 4 == 0) cx += (hash3(y, cx, 8) & 1) ? 1 : -1; cx = std::clamp(cx, x0 + 2, x1 - 1); b.set(cx, y, R[0]); }
  moss(b, x0, base - 5, x1 + 2, base, 515, 0.55f);
  moss(b, x0, top - 2, x0 + 4, top + 4, 516, 0.35f);
  for (int i = 0; i < 6; i++) b.set(x0 + 1 + (int)(hash3(i, 1, 99) % 9), top + 2 + (int)(hash3(i, 2, 99) % 20), rgba(196, 190, 120));   // lichen
  outline(b, 0.75f);
  // rubble and grass at its foot
  ball(b, x0 - 1.0f, base + 1.0f, 1.6f, 1.1f, R, 0.0f);
  ball(b, x1 + 2.0f, base + 1.2f, 1.3f, 1.0f, kStoneWarm, 0.0f);
  ball(b, x0 + 4.0f, base + 2.0f, 1.0f, 0.8f, R, 0.0f);
  tufts(b, x0 - 2, x1 + 3, base + 2, 61, 0.55f);
  castShadow(c, {V(x0 + 1, base + 1), V(x1 + 2, base + 1), V(c.w - 1, base + 3), V(x0 + 5, base + 3)}, 74);
  blit(c, b, 0, 0);
}

// ---------------------------------------------------------------- the toppled statue (52 x 28, 3 x 1, solid)
void toppledStatue(Canvas& c) {
  const int base = c.h - 4;
  Canvas b(c.w, c.h);
  const Ramp& S = kStoneWarm;
  // the plinth, still standing at the west end: a squat block, its top lit, cracked, its front in mid tone
  const int px0 = 1, px1 = 11, ptop = base - 11;
  for (int y = ptop; y <= base; y++)
    for (int x = px0; x <= px1; x++) {
      int k = y < ptop + 4 ? 4 : 2;                       // the top face, then the front face
      if (y >= ptop + 4 && x <= px0 + 1) k = 3;
      if (x >= px1 - 1) k = std::max(0, k - 2);
      if (y == ptop + 4) k = 1;                            // the top's front lip
      if (y == base) k = 0;
      b.set(x, y, S[k]);
    }
  // a moulding band on the plinth front, and the feet that stayed on it, sheared off at the ankle
  for (int x = px0; x <= px1; x++) { b.set(x, ptop + 7, S[1]); b.set(x, ptop + 6, S[3]); }
  box(b, px0 + 3, ptop + 1, px0 + 4, ptop + 2, S[1]); box(b, px0 + 6, ptop + 1, px0 + 7, ptop + 2, S[1]);
  b.set(px0 + 3, ptop, S[3]); b.set(px0 + 6, ptop, S[3]);
  line(b, px0 + 2, ptop + 2, px0 + 8, ptop + 3, S[1]);   // a crack across the top
  // the body, fallen on its back with its head to the east (we look down on its front): robed legs side by side, the
  // robe widening to the girdle, the chest with the arms crossed on a sword, the neck snapped
  const float my = base - 6.0f;                           // the body's mid line
  layered(b, [&](Canvas& L) {                             // the feet, poking from the hem toward the plinth
    ball(L, 14.0f, my - 2.4f, 1.8f, 1.5f, S, 0.0f, 1);
    ball(L, 14.0f, my + 2.4f, 1.8f, 1.5f, S, 0.0f, 1);
  });
  layered(b, [&](Canvas& L) {                             // the robe: a long trapezoid, lit along its north side
    for (int x = 15; x <= 30; x++) {
      const float half = 3.6f + (x - 15) * 0.17f;
      for (int y = (int)std::floor(my - half); y <= (int)std::ceil(my + half); y++) {
        const float v = (y - my) / half;
        int k = v < -0.55f ? 4 : v < 0.1f ? 3 : v < 0.7f ? 2 : 1;
        if ((x % 5) == 2 && v > -0.6f) k = std::max(0, k - 1);   // the folds run along the body
        L.set(x, y, S[k]);
      }
    }
    for (int y = (int)(my - 6); y <= (int)(my + 6); y++) { if (solid(L, 30, y)) L.set(30, y, S[1]); if (solid(L, 29, y)) L.set(29, y, S[3]); }
  });
  layered(b, [&](Canvas& L) { capsule(L, V(31, my), V(36, my), 5.4f, 4.8f, S, 1); });   // the chest
  layered(b, [&](Canvas& L) {                             // the arms, crossed on the chest, and the sword under them
    thickLine(L, 23, my, 37, my, 1.2f, kStone[2]);
    hline(L, 23, 37, (int)my - 1, kStone[3]);
    capsule(L, V(31, my - 4.5f), V(35, my + 1.0f), 1.5f, 1.4f, S, 1);
    capsule(L, V(31, my + 4.5f), V(35, my - 1.0f), 1.5f, 1.4f, S, 1);
    box(L, 36, (int)my - 2, 37, (int)my + 2, kBrass[1]); L.set(36, (int)my - 2, kBrass[3]);
  });
  ellipse(b, 38.0f, my, 1.4f, 2.0f, S[4]); b.set(39, (int)my, S[2]);   // the snapped neck: raw, lighter stone
  weather(b, 13, base - 14, 40, base, S, 717, 0.45f);
  moss(b, 13, base - 3, 40, base, 718, 0.4f);
  // the head, rolled away to the east, face up: the crown's points toward the east, the face worn, the nose gone
  Canvas head(c.w, c.h);
  ball(head, 45.5f, base - 5.0f, 4.0f, 3.6f, S, 0.04f, 1);
  for (int y = base - 9; y <= base - 1; y++) {            // the crown along the top of the head (its east side)
    head.set(49, y, kBrass[y < base - 5 ? 3 : 2]);
    if (y % 2 == 0) head.set(50, y, kBrass[y < base - 5 ? 4 : 2]);
  }
  head.set(46, base - 7, S[0]); head.set(46, base - 3, S[0]);   // the eyes (it lies face up: they sit one above the other)
  head.set(44, base - 5, S[4]); head.set(43, base - 5, S[1]);   // the broken nose, the mouth
  outline(head, 0.8f);
  blit(b, head, 0, 0);
  // chips of stone in the grass
  ball(b, 40.0f, base + 1.0f, 1.2f, 0.8f, S, 0.0f);
  ball(b, 13.0f, base + 1.5f, 1.4f, 0.9f, S, 0.0f);
  b.set(38, base + 2, S[3]); b.set(50, base + 1, S[2]);
  moss(b, px0, base - 4, px1, base, 719, 0.4f);
  outline(b, 0.5f);
  tufts(b, 0, c.w - 1, base + 1, 81, 0.55f);
  tufts(b, 12, 40, base - 1, 82, 0.25f);
  castShadow(c, {V(2, base + 1), V(12, base + 1), V(14, base), V(40, base), V(42, base - 1), V(50, base - 1), V(51, base + 2), V(6, base + 3)}, 72);
  blit(c, b, 0, 0);
}

// ---------------------------------------------------------------- the mural (16 x 34, walked over: hangs on a wall)
// A fresco on the wall face above the tile it stands on (like a tapestry): a crowned figure between two banners under a
// faded sky, the plaster cracked and flaking to the bare stone, its flakes on the floor below.
void mural(Canvas& c) {
  const int y0 = 4, y1 = 21, x0 = 1, x1 = 14;
  Canvas b(c.w, c.h);
  // the plaster panel, its edge eaten away by damp (noise), a little thickness on its lower and east rims
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      const float e = std::min({(float)(x - x0), (float)(x1 - x), (float)(y - y0), (float)(y1 - y)});
      if (e < 2.0f && vnoise(x * 0.7f, y * 0.7f, 303) > 0.35f + e * 0.25f) continue;
      b.set(x, y, kPlaster[3]);
    }
  // the painting, faded: sky band, ground band, two banners, the crowned figure
  const uint32_t sky = mix(rgba(120, 146, 176), kPlaster[3], 0.45f), skyLo = mix(rgba(150, 168, 186), kPlaster[3], 0.45f);
  const uint32_t ground = mix(rgba(168, 128, 70), kPlaster[3], 0.4f);
  const uint32_t robe = mix(rgba(156, 52, 58), kPlaster[3], 0.35f), robeLo = mix(rgba(110, 36, 50), kPlaster[3], 0.3f);
  const uint32_t ban = mix(rgba(52, 76, 140), kPlaster[3], 0.35f), gold = mix(kGold[3], kPlaster[3], 0.25f);
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      if (!solid(b, x, y)) continue;
      if (y < y0 + 6) b.set(x, y, y < y0 + 3 ? sky : skyLo);
      else if (y > y1 - 4) b.set(x, y, ground);
    }
  // banners on poles at both sides
  for (int side = 0; side < 2; side++) {
    const int bx = side ? x1 - 3 : x0 + 2;
    for (int y = y0 + 3; y <= y1 - 3; y++) if (solid(b, bx, y)) b.set(bx, y, kWoodDark[2]);
    for (int y = y0 + 4; y <= y0 + 9; y++)
      for (int x = bx + 1; x <= bx + 2; x++)
        if (solid(b, x - (side ? 3 : 0), y) && !(y == y0 + 9 && x == bx + 1)) b.set(x - (side ? 3 : 0), y, ban);
    if (solid(b, bx + (side ? -1 : 1), y0 + 6)) b.set(bx + (side ? -1 : 1), y0 + 6, gold);
  }
  // the figure: a robe widening to the foot, arms out, a gold crown, a pale face
  const int fx = (x0 + x1) / 2;
  for (int y = y0 + 8; y <= y1 - 3; y++) {
    const int half = 1 + (y - (y0 + 8)) / 3;
    for (int x = fx - half; x <= fx + half; x++) if (solid(b, x, y)) b.set(x, y, x > fx ? robeLo : robe);
  }
  hline(b, fx - 3, fx + 3, y0 + 9, robe);
  ellipse(b, fx + 0.5f, y0 + 6.5f, 1.4f, 1.6f, mix(rgba(226, 190, 160), kPlaster[3], 0.3f));
  b.set(fx - 1, y0 + 4, gold); b.set(fx, y0 + 4, gold); b.set(fx + 1, y0 + 4, gold); b.set(fx, y0 + 3, gold);
  // age: cracks, flaking to bare stone, damp stains, the light falling from the top-left
  for (int i = 0; i < 2; i++) {
    int cx = x0 + 3 + i * 7;
    for (int y = y0 + 1; y <= y1 - 1; y++) {
      if (hash3(cx, y, 31 + i) % 3 == 0) cx += (hash3(y, cx, 32) & 1) ? 1 : -1;
      if (solid(b, cx, y)) b.set(cx, y, darken(b.get(cx, y), 0.55f));
    }
  }
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      if (!solid(b, x, y)) continue;
      const float n = vnoise(x * 0.5f, y * 0.5f, 404);
      if (n > 0.8f) b.set(x, y, kStone[n > 0.88f ? 1 : 2]);                       // flaked to the stone beneath
      else if (n < 0.16f) b.set(x, y, darken(b.get(x, y), 0.3f));                 // a damp stain
      const float lit = ((x1 - x) + (y1 - y)) / (float)((x1 - x0) + (y1 - y0));
      if (lit > 0.82f && hashf(x, y, 405) < 0.5f) b.set(x, y, lighten(b.get(x, y), 0.25f));
    }
  // the panel's thickness: a shaded east rim and lower rim where the plaster stands proud of the wall
  for (int y = y0; y <= y1 + 1; y++)
    for (int x = x0; x <= x1 + 1; x++)
      if (!solid(b, x, y) && (solid(b, x - 1, y) || solid(b, x, y - 1)) && x > x0 && y > y0) b.set(x, y, withA(kShadowHue, 110));
  // flakes of plaster on the floor below
  for (int i = 0; i < 9; i++) {
    const int fx2 = 2 + (int)(hash3(i, 7, 61) % 12), fy2 = c.h - 2 - (int)(hash3(i, 8, 61) % 4);
    b.set(fx2, fy2, kPlaster[(i % 3) + 2]);
    if (i % 3 == 0) b.set(fx2 + 1, fy2, kPlaster[1]);
  }
  blit(c, b, 0, 0);
}

// ---------------------------------------------------------------- the named grave (18 x 26, 1 x 1, solid)
void namedGrave(Canvas& c) {
  const int base = c.h - 3;
  Canvas b(c.w, c.h);
  const Ramp& R = kStone;
  // the grave: a low mound of earth in a stone kerb, seen from above (its top), the kerb's south face at the foot
  const int gx0 = 2, gx1 = 14, gy0 = base - 9, gy1 = base;
  for (int y = gy0; y <= gy1; y++)
    for (int x = gx0; x <= gx1; x++) {
      const bool kerb = x == gx0 || x == gx1 || y == gy0 || y >= gy1 - 1;
      if (kerb) {
        int k = y >= gy1 - 1 ? (y == gy1 ? 1 : 2) : 3;   // the kerb's top lit, its south face below
        if (x == gx0 && y < gy1 - 1) k = 4;
        if (x == gx1) k = std::max(0, k - 2);
        b.set(x, y, R[k]);
      } else {
        // the mound: lit to the north-west, earth with sparse dead grass
        const float dx = (x - (gx0 + gx1) * 0.5f) / 6.0f, dy = (y - (gy0 + gy1 - 1) * 0.5f) / 4.0f;
        const float l = lightAt(dx * 0.8f, dy * 0.8f - 0.25f);
        b.set(x, y, kEarth[lightIndex(l, x, y, 0.12f)]);
        if (hashf(x, y, 51) < 0.12f) b.set(x, y, rgba(150, 140, 86));
      }
    }
  // the headstone at the head (north) end: rounded, its face to the south, a name and a year cut in it
  const int hx0 = 4, hx1 = 12, htop = 2, hbot = gy0 + 1;
  for (int y = htop; y <= hbot; y++)
    for (int x = hx0; x <= hx1; x++) {
      const float dx = (x + 0.5f - (hx0 + hx1 + 1) * 0.5f) / ((hx1 - hx0 + 1) * 0.5f);
      if (y < htop + 3 && dx * dx + ((htop + 3 - y) / 3.0f) * ((htop + 3 - y) / 3.0f) > 1.0f) continue;
      int k = 2;
      if (x <= hx0 + 1 || y == htop) k = 3;
      if (x == hx0 && y > htop + 2) k = 4;
      if (x == hx1) k = 1;
      b.set(x, y, R[k]);
    }
  for (int y = htop + 3; y <= hbot; y++) b.set(hx1 + 1, y, R[0]);   // its thickness, in shade
  weather(b, hx0, htop, hx1, hbot, R, 222, 0.5f);
  // the name (a line of letters), the year (a shorter line), a little star above
  for (int x = hx0 + 2; x <= hx1 - 2; x++) if (hash3(x, 3, 77) % 4) groove(b, x, htop + 6, R);
  for (int x = hx0 + 3; x <= hx1 - 3; x++) if (hash3(x, 4, 77) % 3) groove(b, x, htop + 9, R);
  groove(b, (hx0 + hx1) / 2, htop + 2, R); groove(b, (hx0 + hx1) / 2 - 1, htop + 3, R); groove(b, (hx0 + hx1) / 2 + 1, htop + 3, R);
  moss(b, hx0, hbot - 3, hx1, hbot, 223, 0.5f);
  // dead flowers laid on the mound: dry stems and a few faded petals
  for (int i = 0; i < 3; i++) {
    const int fx = gx0 + 4 + i * 2, fy = gy0 + 4 + (i & 1);
    b.set(fx, fy, rgba(118, 96, 60)); b.set(fx + 1, fy + 1, rgba(118, 96, 60));
    b.set(fx, fy - 1, i == 1 ? rgba(176, 120, 132) : rgba(196, 170, 112));
  }
  outline(b, 0.7f);
  tufts(b, 0, c.w - 1, base + 1, 91, 0.5f);
  castShadow(c, {V(gx0 + 1, base + 1), V(gx1 + 1, base + 1), V(c.w - 1, base + 2), V(gx0 + 4, base + 2)}, 60);
  castShadow(c, {V(hx1 + 1, htop + 4), V(hx1 + 3, htop + 6), V(hx1 + 3, hbot + 2), V(hx1 + 1, hbot)}, 64);   // the stone's shadow on the mound
  blit(c, b, 0, 0);
}

// ---------------------------------------------------------------- the lost journal (18 x 12, walked over, on the floor)
void lostJournal(Canvas& c) {
  Canvas b(c.w, c.h);
  // the satchel, fallen on its side to the west, its flap thrown open
  ball(b, 4.0f, 7.0f, 3.6f, 2.8f, kLeather, 0.05f, 1);
  for (int x = 1; x <= 7; x++) b.set(x, 5 + (x > 4 ? 1 : 0), kLeather[1]);   // the flap's edge
  box(b, 3, 7, 4, 8, kBrass[2]); b.set(3, 7, kBrass[4]);                        // its buckle
  for (int x = 6; x <= 8; x++) b.set(x, 3 - (x - 6) / 2, kLeather[2]);          // the strap
  // the journal, open, its two pages seen from above (a little skewed as it fell), ruled with faded ink
  for (int y = 2; y <= 9; y++)
    for (int x = 8; x <= 16; x++) {
      const int sx = x - (9 - y) / 4;   // the skew
      if (sx < 7 || sx > 16) continue;
      int k = sx < 12 ? 4 : 3;           // the left page faces the light, the right one a shade darker
      if (sx == 12) k = 1;               // the gutter
      if (y == 9) k = 2;
      b.set(sx, y, kParch[k]);
    }
  for (int y = 3; y <= 8; y += 2)
    for (int x = 8; x <= 16; x++) {
      const int sx = x - (9 - y) / 4;
      if (sx == 12 || sx < 8 || sx > 15) continue;
      if (hash3(x, y, 17) % 4) b.set(sx, y, mix(kInkCol, kParch[3], 0.45f));
    }
  hline(b, 7, 16, 10, kLeather[1]);   // the cover's edge under the pages
  // a quill, dropped across the page
  line(b, 13, 1, 16, 4, kParch[4]);
  b.set(16, 4, kInkCol);
  outline(b, 0.6f);
  castShadow(c, {V(1, 9), V(16, 10), V(17, 11), V(3, 11)}, 56);
  blit(c, b, 0, 0);
}

}  // namespace

int lorePropW(Prop p) { return li(p).w; }
int lorePropH(Prop p) { return li(p).h; }
int lorePropFrames(Prop p) { return li(p).frames; }

void paintLoreProp(Canvas& c, Prop p, int frame) {
  (void)frame;
  switch (p) {
    case Prop::NoticeBoard: noticeBoard(c); break;
    case Prop::Inscription: inscription(c); break;
    case Prop::ToppledStatue: toppledStatue(c); break;
    case Prop::Mural: mural(c); break;
    case Prop::NamedGrave: namedGrave(c); break;
    case Prop::LostJournal: lostJournal(c); break;
    default: break;
  }
}

}  // namespace art
