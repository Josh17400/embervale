// EMBERVALE art: the rig-2 body-holders (P2a HOLDERS), moved out of art_props.cpp (SOLID 5.1). Their sizes and the
// berth geometry are art_props.cpp's holderSize2 / berth2; the shared heights are art_holders2.h's.
#include "rpg/art/art_prop_family.h"
#include "rpg/art/art_holders2.h"

namespace art {

// =====================================================================================================
// (P2a HOLDERS) rig 2: the body-holders fitted to the 32 px v2 figure (STYLE_P2 1 and 16; heights in art_holders2.h).
// Rig 1 never reaches this section (paintProp / propW / propH route here only when rigVersion() >= 2), so today's
// pixels never move (H1). Same 3/4 conventions as the furniture above: a lit top with a bright front lip, a front face
// lit at its left column and shaded at its right, a dark foot row; light from the top-left.
// =====================================================================================================
namespace h2 {
using namespace holders;
// a straight 2 px leg (lit column, shade column), standing on row y1; back legs a step darker
void leg2(Canvas& c, int x, int y0, int y1, const Ramp& R, bool back) {
  for (int y = y0; y <= y1; y++) { c.set(x, y, R[back ? 1 : 3]); c.set(x + 1, y, R[back ? 0 : 1]); }
  if (!back) { c.set(x, y1, R[1]); c.set(x + 1, y1, R[0]); }
}
// a seat seen from above: rows y0..lip-1 of top, the lip row lit, an apron of `apron` rows under it
void seatSlab(Canvas& c, int x0, int x1, int y0, int lip, int apron, const Ramp& R, bool leftEnd = true, bool rightEnd = true) {
  for (int y = y0; y <= lip; y++)
    for (int x = x0; x <= x1; x++) {
      int k = 3;
      if (y == y0) k = 2;                                   // the far edge, under the back's shade
      if (y == lip) k = 4;
      if (leftEnd && x == x0) k = std::min(4, k + 1);
      if (rightEnd && x == x1) k = std::max(1, k - 1);
      c.set(x, y, R[k]);
    }
  for (int y = lip + 1; y <= lip + apron; y++)
    for (int x = x0; x <= x1; x++) {
      int k = y == lip + 1 ? 1 : 2;
      if (y == lip + apron && apron > 1) k = 1;
      if (leftEnd && x == x0) k = 2;
      if (rightEnd && x == x1) k = 0;
      c.set(x, y, R[k]);
    }
}

// ---- seats
void chair(Canvas& c) {   // 14 x 24: a ladder-back chair, the back to the north
  const Ramp& R = kWood;
  const int W = c.w, F = c.h - 1, lip = F - kSeat, st = lip - kSeatDepth;
  // the back: two posts with finials, a curved top rail, a middle rail and a splat; 9 rows over the seat whatever its
  // height (P2b: the seat at A3's knee leaves the canvas's top rows clear)
  const int t = std::max(0, st - 9);
  for (int y = t + 1; y < st; y++) { c.set(1, y, R[3]); c.set(2, y, R[2]); c.set(W - 3, y, R[2]); c.set(W - 2, y, R[1]); }
  c.set(1, t, R[4]); c.set(2, t, R[3]); c.set(W - 3, t, R[3]); c.set(W - 2, t, R[2]);
  for (int x = 3; x <= W - 4; x++) {
    const int lift = (x >= 5 && x <= W - 6) ? 1 : 0;   // the top rail bows up between the posts
    c.set(x, t + 2 - lift, R[4]); c.set(x, t + 3 - lift, R[3]); c.set(x, t + 4 - lift, R[1]);
    c.set(x, st - 3, R[3]); c.set(x, st - 2, R[1]);
  }
  for (int y = t + 4; y <= st - 4; y++) { c.set(W / 2 - 1, y, R[3]); c.set(W / 2, y, R[1]); }
  // the seat and its legs, a stretcher between the front legs where they are long enough for one
  seatSlab(c, 1, W - 2, st, lip, 2, R);
  leg2(c, 3, lip + 3, F - 3, R, true);
  leg2(c, W - 5, lip + 3, F - 3, R, true);
  leg2(c, 1, lip + 3, F, R, false);
  leg2(c, W - 3, lip + 3, F, R, false);
  if (F - 4 > lip + 3) hline(c, 3, W - 4, F - 4, R[2]);
}

void stool(Canvas& c) {   // 14 x 16: a round three-legged stool
  const Ramp& R = kWood;
  const int cx = c.w / 2, F = c.h - 1, lip = F - kSeat;
  line(c, cx - 3, lip + 2, cx - 5, F, R[3]); line(c, cx - 2, lip + 2, cx - 4, F, R[2]);
  line(c, cx + 2, lip + 2, cx + 4, F, R[1]); line(c, cx + 1, lip + 2, cx + 3, F, R[2]);
  vline(c, cx, lip + 2, F - 2, R[1]); vline(c, cx - 1, lip + 2, F - 2, R[0]);
  if (F - 4 > lip + 2) hline(c, cx - 4, cx + 3, F - 4, R[1]);    // the rung (where the legs are long enough)
  ellipse(c, cx - 0.5, lip - 1.0, 5.6, 2.6, R[2]);              // the seat, thick, lit from the top-left
  ellipse(c, cx - 0.5, lip - 1.6, 5.2, 2.0, R[3]);
  ellipse(c, cx - 1.2, lip - 2.0, 3.4, 1.2, R[4]);
  hline(c, cx - 5, cx + 4, lip + 1, R[1]);
  hline(c, cx - 4, cx + 3, lip + 2, R[0]);
  c.set(cx - 6, lip, R[3]);
}

void bench(Canvas& c) {   // 16 x 16 a segment: a long plank seat on trestles; joins its neighbours
  const Ramp& R = kWood;
  const int W = c.w, F = c.h - 1, lip = F - kSeat, st = lip - kSeatDepth + 1;
  for (int y = st; y <= lip; y++)
    for (int x = 0; x < W; x++) {
      int k = y == lip ? 4 : 3;
      if (y == st) k = 2;
      if ((x % 7) == 3 && y < lip && y > st) k = 2;            // board ends staggered along the seat
      c.set(x, y, R[k]);
    }
  for (int x = 0; x < W; x++) { c.set(x, lip + 1, R[1]); c.set(x, lip + 2, R[2]); c.set(x, lip + 3, R[0]); }
  for (int lx : {2, W - 4}) {
    leg2(c, lx, lip + 4, F, R, false);
    c.set(lx - 1, F, R[1]); c.set(lx + 2, F, R[0]);              // the trestle's splayed foot
  }
}

void throne(Canvas& c) {   // 28 x 44: a gilt throne with a red back on a two-step dais
  const int W = c.w, F = c.h - 1, lip = F - kThroneSeat, st = lip - 4;
  // the dais
  for (int y = F - 2; y <= F; y++)
    for (int x = 0; x < W; x++) {
      int k = y == F - 2 ? 4 : (y == F ? 1 : 2);
      if (x == 0) k = std::min(4, k + 1);
      if (x == W - 1) k = std::max(0, k - 1);
      c.set(x, y, kStoneWarm[k]);
    }
  // the tall back: a gilt frame round a red panel, crest spikes on top
  for (int y = 3; y < st; y++)
    for (int x = 3; x < W - 3; x++) {
      int k = x < 5 ? 3 : (x > W - 6 ? 1 : 2);
      if (y == 3) k = 4;
      c.set(x, y, kGold[k]);
    }
  for (int y = 6; y < st - 1; y++)
    for (int x = 6; x < W - 6; x++) {
      float dx = (x + 0.5f - W * 0.5f) / (W * 0.5f - 6.0f);
      int k = x < 9 ? 3 : 2;
      if (y < 9 && dx * dx > 1.0f - (y - 6) * 0.3f) { c.set(x, y, kGold[2]); continue; }   // the panel's arched head
      if (y == st - 2) k = 1;
      c.set(x, y, kRed[k]);
    }
  for (int i = 0; i < 4; i++) { const int x = 4 + i * 6 + (i >= 2 ? 1 : 0); c.set(x, 2, kGold[4]); c.set(x + 1, 2, kGold[2]); c.set(x, 1, kGold[3]); }
  c.set(W / 2 - 1, 0, kGold[4]); c.set(W / 2, 0, kGold[3]); c.set(W / 2 - 1, 1, kGold[4]); c.set(W / 2, 1, kGold[2]);
  ball(c, W / 2.0 - 0.5, 11.5, 2.2, 2.2, kGold);                 // a boss on the panel
  // the seat (a red cushion on a gilt box), the arms
  for (int y = st; y <= lip; y++)
    for (int x = 2; x <= W - 3; x++) {
      int k = y == lip ? 4 : 3;
      if (x == 2) k = 4;
      if (x == W - 3) k = 2;
      c.set(x, y, kGold[k]);
    }
  for (int y = st; y < lip; y++) for (int x = 6; x <= W - 7; x++) c.set(x, y, kRed[y == st ? 2 : (x < 9 ? 4 : 3)]);
  for (int y = lip + 1; y <= F - 3; y++)
    for (int x = 2; x <= W - 3; x++) {
      int k = 2;
      if (y == lip + 1) k = 1;
      if (x == 2) k = 3;
      if (x >= W - 4) k = 1;
      if ((x == 6 || x == W - 7) && y > lip + 2) k = std::max(0, k - 1);   // the box's panels
      if (y == F - 3) k = 0;
      c.set(x, y, kGold[k]);
    }
  for (int ax : {0, W - 5}) {
    box(c, ax, st - 6, ax + 4, lip + 1, kGold[ax == 0 ? 3 : 1]);
    hline(c, ax, ax + 4, st - 6, kGold[4]);
    c.set(ax + (ax == 0 ? 0 : 4), st - 6, kGold[ax == 0 ? 4 : 2]);
    ball(c, ax + 2.0, st - 6.5, 2.2, 1.6, kGold);               // a lion's-head knob on each arm
  }
}

// ---- tables and counters: the top's front lip kTable / kCounter above the floor row
void tableAt(Canvas& c, int x0, int x1, bool leftEnd, bool rightEnd, int& top) {
  const Ramp& R = kWood;
  const int F = c.h - 1, lip = F - kTable;
  top = lip - 7;   // 8 rows of top (the last is the lit lip)
  if (leftEnd) leg2(c, x0 + 2, top + 9, F - 3, R, true);
  if (rightEnd) leg2(c, x1 - 3, top + 9, F - 3, R, true);
  tableTop(c, x0, x1, top, 8, leftEnd, rightEnd, R);
  if (leftEnd) leg2(c, x0, top + 10, F, R, false);
  if (rightEnd) leg2(c, x1 - 1, top + 10, F, R, false);
  if (leftEnd && rightEnd) hline(c, x0 + 2, x1 - 2, F - 5, R[1]);   // a stretcher between the front legs
}
void tableSmall(Canvas& c, int kind) {   // 16 x 26
  int top;
  tableAt(c, 1, 14, true, true, top);
  if (kind == 1) {
    plateAt(c, 5, top + 5, rgba(170, 96, 52));
    mugAt(c, 10, top + 5, true);
    breadAt(c, 11, top + 2);
    c.set(4, top + 2, kGold[4]); c.set(5, top + 2, kGold[3]); c.set(4, top + 3, kGold[2]); c.set(5, top + 3, kGold[2]);
  } else if (kind == 2) {
    for (int x = 3; x <= 9; x++) { c.set(x, top + 3, kWhite); c.set(x, top + 4, x == 6 ? kCloth[2] : kCloth[4]); c.set(x, top + 5, kCloth[3]); }
    c.set(6, top + 3, kCloth[2]); c.set(4, top + 4, kCloth[1]); c.set(8, top + 4, kCloth[1]);
    hline(c, 3, 9, top + 6, kRed[1]);
    candleAt(c, 11, top + 5, 4);
    c.set(12, top + 6, kBrass[2]); c.set(10, top + 6, kBrass[3]);
    c.set(2, top + 2, kInk); c.set(3, top + 2, kSoot[2]); line(c, 3, top + 1, 1, top - 2, kWhite);
  }
}
void tableSeg(Canvas& c, int part) {   // 16 x 26, 0 left, 1 middle, 2 right
  int top;
  tableAt(c, part == 0 ? 1 : 0, part == 2 ? 14 : 15, part == 0, part == 2, top);
  if (part == 1) {
    plateAt(c, 7, top + 5, rgba(196, 120, 64));
    mugAt(c, 12, top + 5, (hash3(part, 0, 5) & 1) != 0);
    c.set(3, top + 4, kPewter[3]); c.set(3, top + 5, kPewter[2]);
  } else if (part == 0) {
    candleAt(c, 5, top + 4, 4);
    c.set(4, top + 5, kBrass[3]); c.set(6, top + 5, kBrass[1]);
    breadAt(c, 11, top + 5);
  } else {
    jugAt(c, 5, top + 5);
    c.set(9, top + 4, kGold[4]); c.set(10, top + 4, kGold[3]); c.set(9, top + 5, kGold[2]); c.set(10, top + 5, kGold[2]); c.set(11, top + 5, kGold[1]);
  }
}
void table(Canvas& c) {   // 32 x 26: the classic two-segment table
  Canvas l(16, c.h), r(16, c.h);
  tableSeg(l, 0); tableSeg(r, 2);
  blit(c, l, 0, 0); blit(c, r, 16, 0);
}
void counterSeg(Canvas& c, int part, int goods) {   // 16 x 30
  const Ramp& R = kWood;
  const int F = c.h - 1, lip = F - kCounter, depth = 5, top = lip - depth + 1, fh = F - lip;
  const int x0 = part == 0 ? 1 : 0, x1 = part == 2 ? 14 : 15;
  for (int y = top; y <= lip; y++)
    for (int x = x0; x <= x1; x++) {
      int k = y == lip ? 4 : 3;
      if (y == top + 2) k = 2;
      if (part == 0 && x == x0) k = 4;
      if (part == 2 && x == x1) k = 2;
      c.set(x, y, kWoodDark[std::min(4, k + 1)]);
    }
  for (int y = lip + 1; y <= F; y++)
    for (int x = x0; x <= x1; x++) {
      const int ly = y - (lip + 1);
      int k = 2;
      const int px2 = (x + 4) % 8;                                // raised panels, 8 px apart
      const bool rail = ly == 0 || ly == 1 || ly >= fh - 3 || ly == fh / 2;
      if (!rail) {
        if (px2 == 0) k = 1;
        else if (px2 == 1) k = 3;
        else if (ly == 2 || ly == fh / 2 + 1) k = 1;
        else if (ly == fh - 4 || ly == fh / 2 - 1) k = 3;
      } else k = ly == 0 ? 1 : (ly == 1 ? 3 : (ly == fh - 1 ? 0 : 2));
      if (part == 0 && x == x0) k = 3;
      if (part == 2 && x == x1) k = std::min(k, 1);
      c.set(x, y, R[k]);
    }
  if (goods >= 0) { shopCounterGoods(c, top, part, goods); return; }
  if (part == 1) {
    mugAt(c, 3, top + 3, true);
    mugAt(c, 9, top + 3, false);
    c.set(14, top + 3, kGold[4]); c.set(13, top + 3, kGold[3]);
  } else if (part == 0) {
    for (int y = top - 3; y <= top + 3; y++) { c.set(6, y, kGlassG[y < top - 1 ? 2 : 3]); c.set(7, y, kGlassG[1]); }
    c.set(6, top - 4, kWood[2]); c.set(6, top, kGlassG[4]);
  } else {
    ball(c, 8.0f, top + 1.5f, 3.0f, 2.2f, kBrass);
    c.set(7, top - 1, kBrass[4]); vline(c, 8, top - 2, top - 1, kBrass[2]);
  }
}
void counter(Canvas& c) {   // 32 x 30: the classic counter, two segments
  Canvas l(16, c.h), r(16, c.h);
  counterSeg(l, 0, -1); counterSeg(r, 2, -1);
  blit(c, l, 0, 0); blit(c, r, 16, 0);
}

// ---- beds: the classic bed 24 x 40 (the headboard rises 8 px over its 32 px), the long bed 24 x 48, the bunk 24 x 60
// One painter for every frame of bed: posts and an arched headboard at rows hb0..hb1, the pillow, the turned-down sheet,
// the quilt down to qy1, the quilt draping over the foot rail, the foot rail and its posts in the bottom rows.
void bed2Paint(Canvas& c, const Ramp& Q, int yOff, int H, int hb0, int hb1, bool footRail = true) {
  const int W = c.w;
  const Ramp& R = kWood;
  for (int y = hb0; y <= hb1; y++)
    for (int x = 2; x <= W - 3; x++) {
      const float dx = (x + 0.5f - W * 0.5f) / (W * 0.5f - 2);
      const int arch = hb0 + (int)(dx * dx * 3.0f);
      if (y < arch) continue;
      int k = 2;
      if (y == arch) k = 4;
      else if (y == arch + 1) k = 3;
      const bool inset = x >= 5 && x <= W - 6 && y >= arch + 3 && y <= hb1 - 2;
      if (inset) k = (x == 5 || y == arch + 3) ? 1 : (x == W - 6 ? 3 : 2);
      if (x >= W - 4 && !inset) k = std::max(0, k - 1);
      if (y == hb1) k = 1;
      c.set(x, y + yOff, R[k]);
    }
  for (int side = 0; side < 2; side++) {
    const int x0 = side ? W - 2 : 0;
    for (int y = hb0; y <= hb1 + 3; y++) { c.set(x0, y + yOff, R[side ? 2 : 4]); c.set(x0 + 1, y + yOff, R[side ? 1 : 2]); }
    c.set(x0, hb0 - 1 + yOff, R[side ? 3 : 4]); c.set(x0 + 1, hb0 - 1 + yOff, R[side ? 2 : 3]);
    c.set(x0, hb0 - 2 + yOff, R[3]); c.set(x0 + 1, hb0 - 2 + yOff, R[2]);
  }
  // the mattress: a plump pillow 6 rows deep, the sheet turned down 2 rows, the quilt
  const int py0 = hb1 + 1, py1 = hb1 + 6, sy = py1 + 1, qy0 = sy + 2, qy1 = H - (footRail ? 8 : 3);
  for (int y = py0; y <= qy1 + 1; y++)
    for (int x = 1; x <= W - 2; x++) {
      uint32_t col;
      if (y <= py1) {
        const float dx = (x + 0.5f - W * 0.5f) / (W * 0.5f - 2.5f), dy = (y - (py0 + py1) * 0.5f) / ((py1 - py0) * 0.5f + 0.6f);
        if (dx * dx * dx * dx + dy * dy > 1.0f) col = kCloth[2];
        else {
          const int k = lightIndex(lightAt(dx * 0.55f, dy * 0.9f), x, y, 0.0f);
          col = k >= 3 ? kWhite : (k == 2 ? kCloth[4] : kCloth[3]);
          if (y == py1 && x > 3 && x < W - 4) col = kCloth[2];
        }
      } else if (y <= sy + 1) {
        col = y == sy ? kWhite : kCloth[3];
        if (x == 1) col = kCloth[4];
        if (x >= W - 3) col = kCloth[2];
      } else {
        const float fold = std::sin((x - 1) * 0.8f + (y - qy0) * 0.22f) * 0.6f + std::sin((y - qy0) * 0.6f) * 0.3f;
        int k = 2 + (fold > 0.45f ? 1 : (fold < -0.5f ? -1 : 0));
        if (x <= 2) k = std::min(4, k + 1);
        if (x >= W - 3) k = std::max(0, k - 1);
        if (y == qy0) k = std::max(0, k - 1);
        if (y >= qy1) k = std::max(0, k - 1);
        if ((x == 3 || x == W - 4) && y > qy0) k = std::max(0, k - 1);
        col = Q[std::clamp(k, 0, 4)];
      }
      c.set(x, y + yOff, col);
    }
  for (int y = hb1 + 1; y <= H - (footRail ? 7 : 2); y++) { c.set(0, y + yOff, R[3]); c.set(W - 1, y + yOff, R[1]); }
  if (!footRail) return;
  for (int x = 1; x <= W - 2; x++) { c.set(x, H - 7 + yOff, Q[1]); c.set(x, H - 6 + yOff, Q[0]); }
  for (int y = H - 5; y <= H - 1; y++)
    for (int x = 0; x < W; x++) {
      int k = y == H - 5 ? 3 : (y == H - 1 ? 0 : 2);
      if (y == H - 3) k = 1;                                      // a moulding along the foot board
      if (x <= 1) k = std::min(4, k + 1);
      if (x >= W - 2) k = std::max(0, k - 1);
      c.set(x, y + yOff, R[k]);
    }
  for (int side = 0; side < 2; side++) {
    const int x0 = side ? W - 2 : 0;
    for (int y = H - 9; y <= H - 1; y++) { c.set(x0, y + yOff, R[side ? 2 : 4]); c.set(x0 + 1, y + yOff, R[side ? 1 : 3]); }
    c.set(x0, H - 10 + yOff, R[side ? 3 : 4]); c.set(x0 + 1, H - 10 + yOff, R[side ? 2 : 3]);
  }
}
void bed(Canvas& c) { bed2Paint(c, ramp(rgba(58, 92, 150)), 0, c.h, kBedHb0, kBedHb1); }
Canvas longBed(int variant) {
  static const uint32_t quilt[4] = {rgba(58, 92, 150), rgba(150, 52, 50), rgba(64, 120, 76), rgba(176, 128, 52)};
  Canvas c(24, 48);
  bed2Paint(c, ramp(quilt[variant & 3]), 0, 48, kLongHb0, kLongHb1);
  for (int y = 24; y < 47; y++) if (!chA(c.get(23, y))) c.set(23, y, withA(kShadowCol, 90));
  outline(c);
  return c;
}
// the bunk: the lower bunk an ordinary bed in the bottom 40 rows; the upper deck on four tall posts over its head end
void bunkBed(Canvas& c) {
  const int W = c.w, H = c.h;
  bed2Paint(c, ramp(rgba(64, 120, 76)), H - 40, 40, kBedHb0, kBedHb1);
  const int d0 = kBunkUp0, d1 = kBunkUp1;
  for (int y = d0; y <= d1; y++)
    for (int x = 2; x <= W - 3; x++) {
      uint32_t col;
      if (y <= d0 + 5) {
        const float dx = (x + 0.5f - W * 0.5f) / (W * 0.5f - 3), dy = (y - d0 - 2.5f) / 2.9f;
        col = dx * dx * dx * dx + dy * dy > 1.0f ? kCloth[2] : (dy < 0 ? kWhite : kCloth[4]);
      } else if (y <= d0 + 7) {
        col = y == d0 + 6 ? kWhite : kCloth[3];
      } else {
        const bool patch = (((x - 2) / 5) + ((y - d0 - 8) / 5)) % 2 == 0;   // a patchwork quilt
        int k = patch ? 2 : 3;
        if (x == 2) k++;
        if (x >= W - 4) k--;
        if (y == d0 + 8) k--;
        if (((x - 2) % 5 == 0 || (y - d0 - 8) % 5 == 0) && x > 2) k--;
        col = patch ? kRed[std::clamp(k, 0, 4)] : kCloth[std::clamp(k, 0, 4)];
      }
      c.set(x, y, col);
    }
  for (int x = 1; x <= W - 2; x++) { c.set(x, d1 + 1, kWood[4]); c.set(x, d1 + 2, kWood[2]); c.set(x, d1 + 3, kWood[1]); }
  for (int y = d1 + 4; y <= d1 + 9; y++)                          // the deck's shade over the lower pillow
    for (int x = 2; x <= W - 3; x++) c.set(x, y, darken(c.get(x, y), 0.55f - (y - d1 - 4) * 0.09f));
  for (int side = 0; side < 2; side++) {
    const int x0 = side ? W - 2 : 0;
    for (int y = 0; y < H; y++) { c.set(x0, y, kWood[side ? 2 : 4]); c.set(x0 + 1, y, kWood[side ? 0 : 2]); }
    c.set(x0, 0, kWood[4]); c.set(x0 + 1, 0, kWood[3]);
  }
  for (int x = 2; x <= W - 3; x++) { c.set(x, 1, kWood[4]); c.set(x, 2, kWood[2]); }
  for (int y = d1 + 2; y <= H - 4; y++) { c.set(W - 7, y, kWood[3]); c.set(W - 3, y, kWood[1]); }   // the ladder
  for (int y = d1 + 5; y <= H - 5; y += kLadderRung) { hline(c, W - 6, W - 4, y, kWood[3]); c.set(W - 4, y + 1, kWood[1]); }
}

// ---- out of doors
void fence(Canvas& c, bool vertical) {   // 16 x 22 (H) / 16 x 32 (V): posts kFence tall, two rails at knee and hip
  const Ramp& R = kWood;
  const int base = c.h - 2, top = base - kFence + 1;
  if (!vertical) {
    for (int px2 : {1, c.w - 3}) {
      for (int y = top; y <= base; y++) { c.set(px2, y, R[3]); c.set(px2 + 1, y, R[1]); }
      c.set(px2, top, R[4]); c.set(px2 + 1, top, R[2]);
      c.set(px2, top + 1, R[4]);
    }
    for (int ry : {top + 2, base - 6})
      for (int x = 0; x < c.w; x++) {
        if (x == 1 || x == 2 || x == c.w - 3 || x == c.w - 2) continue;
        c.set(x, ry, R[(x * 7 + ry) % 11 == 0 ? 2 : 3]); c.set(x, ry + 1, R[1]);
      }
    grassTuft(c, 3, base, 37, 3);
  } else {
    // a run going north: the two rails seen end-on rise as a pair of planks from the post to the canvas top (the run's
    // next tile north carries on from there)
    const int cx = c.w / 2;
    for (int y = 0; y < base - 6; y++) {
      c.set(cx - 2, y, R[3]); c.set(cx - 1, y, R[2]);
      c.set(cx + 1, y, R[2]); c.set(cx + 2, y, R[1]);
      if (y % 8 == 5) { c.set(cx - 2, y, R[4]); c.set(cx + 1, y, R[3]); }
    }
    for (int y = top; y <= base; y++) { c.set(cx - 1, y, R[3]); c.set(cx, y, R[2]); c.set(cx + 1, y, R[1]); }
    hline(c, cx - 1, cx + 1, top, R[4]);
    grassTuft(c, cx - 2, base, 31, 3);
  }
}
void ladder(Canvas& c) {   // 16 x 34: rungs every kLadderRung px, a cave exit's light from above
  const Ramp& R = kWood;
  for (int y = 0; y < c.h - 1; y++) { c.set(3, y, R[3]); c.set(4, y, R[1]); c.set(11, y, R[2]); c.set(12, y, R[0]); }
  for (int y = 3; y < c.h - 2; y += kLadderRung) { hline(c, 5, 10, y, R[3]); hline(c, 5, 10, y + 1, R[1]); c.set(5, y, R[4]); }
  for (int x = 5; x <= 10; x++) { c.set(x, 0, withA(kGlow[4], 160)); c.set(x, 1, withA(kGlow[3], 90)); }
  c.set(3, c.h - 1, R[1]); c.set(4, c.h - 1, R[0]); c.set(11, c.h - 1, R[1]); c.set(12, c.h - 1, R[0]);
}
void gravestone(Canvas& c) {   // 16 x 20: today's headstone x 1.2 (kGraveMarker of stone)
  const int cx = c.w / 2, base = c.h - 2, top = base - kGraveMarker + 1;
  for (int y = top; y <= base; y++)
    for (int x = cx - 6; x <= cx + 5; x++) {
      const int j = y - top;
      if (j < 4) { const float dx = (x + 0.5f - cx) / 6.0f; if (dx * dx + (4 - j) * (4 - j) / 16.0f > 1.0f) continue; }
      int k = x == cx - 6 ? 3 : (x >= cx + 4 ? 1 : 2);
      if (j == 0) k = 3;
      if (x == cx - 5 && j > 0 && j < 6) k = 3;
      c.set(x, y, kStone[k]);
    }
  vline(c, cx, top + 4, top + 11, kStone[0]); hline(c, cx - 2, cx + 2, top + 6, kStone[0]);   // the engraved cross
  vline(c, cx + 1, top + 5, top + 11, kStone[3]); c.set(cx + 3, top + 7, kStone[3]);
  c.set(cx - 5, top + 3, kMoss[2]); c.set(cx - 5, top + 4, kMoss[1]); c.set(cx - 4, top + 3, kMoss[3]);
  c.set(cx + 4, base - 1, kMoss[2]); c.set(cx + 3, base, kMoss[1]);
  grassTuft(c, cx - 5, base, 21, 4);
}

}  // namespace h2

}  // namespace art
