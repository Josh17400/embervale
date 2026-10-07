// EMBERVALE art, M3b FORTIFICATIONS lane: the gate forms that are not the masonry gatehouse (art_walls.cpp paints the
// drum-tower, square-tower and pylon gatehouses): the fjords' and the steppe's timber gate tower, the highlands' cut
// through the bank under a bridge-gate, the thorn walls' hedge gap, the dune folk's iwan, the jade kingdoms' gate
// pavilion and moon gate, the high elves' white arch and the wood elves' living arch.
//
// The contract is gateHouse's (art_building.h): canvas GATE_CW x GATE_CH, the passage's left tile (gx, gy) with its
// top-left corner at (GATE_OX, GATE_OY); the passage is tiles gx..gx+2 (ground x 0..47), the flanking wall tiles gx-1
// and gx+3 (x -16..-1 and 48..63) are painted by the gate: as the wall's own pieces (wallTile with the same parts, so
// the joins with the runs arriving from gx-2 and gx+4 are the wall's own joins) under whatever the gate builds on them.
// Everything is a height field over the ground rendered in the game's oblique (screen row = y - z), lit from the
// top-left, with faces that can look under an eave (kSkip: nothing there, the ground or the wall behind shows).
#include "rpg/art/art_internal.h"
#include "rpg/art/art_heraldry.h"
#include "rpg/art/art_parts.h"

namespace art {

namespace {

constexpr uint32_t kSkip = 0x00000001u;   // a face pixel that is not there (alpha 0: never a real colour)
constexpr uint32_t kClear = 0x00000002u;  // an opening: clears what the flanking wall pieces drew there

const Ramp kGLog = ramp5(rgba(46, 30, 34), rgba(78, 52, 42), rgba(116, 80, 56), rgba(154, 112, 76), rgba(190, 150, 104));
const Ramp kGAdobe = ramp5(rgba(120, 78, 60), rgba(170, 120, 84), rgba(208, 164, 112), rgba(230, 196, 142), rgba(246, 224, 180));
const Ramp kGWhite = ramp5(rgba(124, 132, 156), rgba(180, 190, 208), rgba(222, 228, 236), rgba(240, 244, 248), rgba(255, 255, 255));
const Ramp kGPlaster = ramp5(rgba(140, 132, 128), rgba(196, 190, 180), rgba(230, 226, 214), rgba(242, 240, 230), rgba(252, 250, 244));
const Ramp kGJadeBrick = ramp5(rgba(58, 62, 74), rgba(92, 98, 110), rgba(124, 130, 140), rgba(150, 156, 164), rgba(182, 186, 190));
const Ramp kGJadeTile = ramp5(rgba(20, 58, 48), rgba(34, 92, 70), rgba(52, 128, 92), rgba(84, 166, 116), rgba(146, 210, 160));
const Ramp kGLacquer = ramp5(rgba(70, 18, 22), rgba(118, 28, 30), rgba(160, 40, 38), rgba(196, 64, 50), rgba(226, 104, 76));
const Ramp kGThorn = ramp5(rgba(20, 40, 36), rgba(30, 64, 44), rgba(48, 92, 50), rgba(78, 124, 58), rgba(124, 160, 76));
const Ramp kGLeaf = ramp5(rgba(24, 52, 46), rgba(38, 84, 54), rgba(62, 120, 58), rgba(102, 158, 64), rgba(156, 196, 86));
const Ramp kGGrass = ramp5(rgba(30, 60, 46), rgba(46, 94, 54), rgba(74, 130, 58), rgba(112, 162, 66), rgba(160, 196, 92));
const Ramp kGEarth = ramp5(rgba(54, 40, 40), rgba(88, 64, 50), rgba(120, 92, 64), rgba(150, 120, 84), rgba(178, 150, 108));
const Ramp kGTeal = ramp5(rgba(24, 70, 90), rgba(36, 104, 124), rgba(58, 144, 156), rgba(104, 190, 192), rgba(176, 232, 226));
const Ramp kGSlate = ramp5(rgba(34, 40, 76), rgba(50, 64, 112), rgba(74, 96, 154), rgba(108, 134, 190), rgba(158, 182, 222));
const Ramp kGShingle = ramp5(rgba(54, 34, 38), rgba(88, 56, 46), rgba(124, 82, 58), rgba(158, 112, 74), rgba(192, 148, 100));
const Ramp kGHide = ramp5(rgba(78, 52, 40), rgba(122, 88, 62), rgba(164, 126, 86), rgba(196, 162, 114), rgba(222, 196, 150));
const Ramp kGReed = ramp5(rgba(84, 66, 42), rgba(128, 104, 58), rgba(170, 144, 80), rgba(204, 180, 108), rgba(230, 212, 150));
const Ramp kGBark = ramp5(rgba(46, 30, 40), rgba(74, 46, 42), rgba(106, 70, 50), rgba(140, 98, 64), rgba(170, 128, 84));
const Ramp kGStoneRing = ramp5(rgba(62, 64, 78), rgba(98, 100, 112), rgba(140, 142, 150), rgba(176, 178, 182), rgba(212, 212, 208));
const Ramp kGElfGlow = ramp5(rgba(40, 110, 120), rgba(70, 170, 170), rgba(130, 220, 200), rgba(196, 248, 226), rgba(240, 255, 246));

// the oblique height-field renderer (as art_walls.cpp's), the ground's (0, 0) at canvas (GATE_OX, GATE_OY)
template <class ZF, class TOP, class FACE>
void field(Canvas& c, int fx0, int fx1, int fy0, int fy1, ZF&& zf, TOP&& top, FACE&& face) {
  const int ox = GATE_OX, oy = GATE_OY;
  for (int fx = fx0; fx <= fx1; fx++)
    for (int fy = fy0; fy <= fy1; fy++) {
      const int z = zf(fx, fy);
      if (z <= 0) continue;
      const int x = ox + fx, row = oy + fy - z;
      auto put = [&](int yy, uint32_t col) {
        if (col == kSkip || x < 0 || yy < 0 || x >= c.w || yy >= c.h) return;
        c.set(x, yy, col);
      };
      put(row, top(fx, fy, z));
      const int zn = std::max(0, zf(fx, fy + 1));
      const int rowN = oy + fy + 1 - zn;
      for (int r = row + 1, v = 0; r < rowN; r++, v++) {
        const int h = z - 1 - v;
        if (h < 0) break;
        put(r, face(fx, fy, z, h, v, zn));
      }
    }
}

// the structure over the flanking wall pieces: its own pixels win, its outline only where nothing else is drawn (so
// it joins the wall without a dark seam)
void composeOver(Canvas& dst, const Canvas& s) {
  Canvas lined = s;
  outline(lined, 0.9f);
  for (int y = 0; y < dst.h; y++)
    for (int x = 0; x < dst.w; x++) {
      const uint32_t p = s.get(x, y);
      if (p == kClear) { dst.set(x, y, 0); continue; }
      if (chA(p)) { dst.set(x, y, p); continue; }
      if (s.get(x, y - 1) == kClear) continue;   // no ink line on the ground under an opening
      const uint32_t o = lined.get(x, y);
      if (chA(o) && !chA(dst.get(x, y))) dst.set(x, y, o);
    }
}

// a flanking wall tile (0 west at gx-1, 1 east at gx+3) as the wall's own piece. joined: the wall runs on into the
// passage under the gate's structure (else it ends square at the passage, its end in the wall's own form)
void flankPiece(Canvas& c, int side, bool joined, const bld::FortParts& f, uint32_t seed) {
  uint32_t k = side == 0 ? WALL_BIT_W : WALL_BIT_E;
  if (joined) k |= side == 0 ? WALL_BIT_E : WALL_BIT_W;
  k |= ((seed >> (side * 3)) & 3u) << WALL_VAR_SHIFT;
  const Canvas p = wallTile(k, f);
  const int x0 = side == 0 ? -16 : 48;
  blit(c, p, GATE_OX + x0 - WALL_OX, GATE_OY - WALL_OY);
}

// a hanging banner (w x h) in the arms' colours with its charge, its top at (x, y) (canvas px)
void hangBanner(Canvas& c, int x, int y, int w, int h, const Ramp& BF, const Ramp& BT, int emblem, bool kc, bool tails) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) {
      if (tails && j >= h - 2 && (i == w / 2 || i == w / 2 - 1 + (w & 1))) continue;
      int k = i == 0 ? 3 : (i == w - 1 ? 1 : 2);
      if (j == 0) k = 1;
      uint32_t col = BF[k];
      if (kc && (i == 0 || i == w - 1)) col = BT[i == 0 ? 2 : 1];
      if (kc && w >= 5 && heraldry::chargeAt(emblem, 5, i - (w - 5) / 2, j - 3)) col = BT[std::max(1, heraldry::chargeShade(emblem, 5, i - (w - 5) / 2, j - 3) - (i >= w - 2 ? 1 : 0))];
      c.set(x + i, y + j, col);
    }
  hline(c, x - 1, x + w, y - 1, kGold[3]);
}

// a paper / glass lantern hanging at canvas (x, y): its cap, glowing body and tassel
void lantern(Canvas& c, int x, int y, const Ramp& L) {
  c.set(x, y - 1, kIron[1]);
  hline(c, x - 1, x + 1, y, kGLacquer[0]);
  for (int j = 1; j <= 3; j++) { c.set(x - 1, y + j, L[3]); c.set(x, y + j, L[4]); c.set(x + 1, y + j, L[2]); }
  hline(c, x - 1, x + 1, y + 4, kGLacquer[0]);
  c.set(x, y + 5, kGold[2]);
}

// a pointed (Persian / Gothic) arch's soffit height over x in [x0, x1]: springing at hs, apex ha
int pointedArch(float x, float x0, float x1, int hs, int ha) {
  const float u = (x - (x0 + x1) * 0.5f) / ((x1 - x0) * 0.5f);
  if (std::fabs(u) > 1.0f) return -1;
  return hs + (int)std::lround((ha - hs) * std::pow(std::max(0.0f, 1 - std::fabs(u)), 0.55f));
}

// ================================================================================== the timber gate tower (fjord, steppe)
// A blockhouse of horizontal logs straddling the road, an open gallery under a steep gabled roof (shingles in the
// fjords, hides on the steppe, reed in the marsh), a square gateway with its two leaves standing open; the palisade
// runs into it on both sides. Fjord capitals carve dragon heads on the ridge ends; steppe gates fly horse-tail standards.
Canvas timberGate(uint32_t seed, const Ramp& BF, const Ramp& BT, int emblem, bool kc, const bld::FortParts& F) {
  Canvas c(GATE_CW, GATE_CH);
  flankPiece(c, 0, true, F, seed);
  flankPiece(c, 1, true, F, seed);
  const int cul = (int)F.culture - 1;
  const Ramp& RF = cul == 5 ? kGHide : (cul == 6 ? kGReed : kGShingle);
  const int wallH = wallWalkHeight(F);
  const int zb = wallH + 13, zg = zb + 7;            // the log body's top, the gallery's top (the eaves)
  const int bx0 = -5, bx1 = 52, by0 = -1, by1 = 16;  // the body
  const int rx0 = bx0 - 2, rx1 = bx1 + 2, ry0 = by0 - 2, ry1 = by1 + 2;   // the roof, overhanging
  const float ridgeY = (ry0 + ry1) * 0.5f, half = (ry1 - ry0) * 0.5f + 0.5f;
  const int ax0 = 10, ax1 = 37, aH = 22;              // the gateway
  Canvas s(GATE_CW, GATE_CH);
  auto Z = [&](int gx, int gy) -> int {
    if (gx < rx0 || gx > rx1 || gy < ry0 || gy > ry1) return 0;
    return zg + (int)std::lround((half - std::fabs(gy + 0.5f - ridgeY)) * 1.15f);
  };
  auto top = [&](int gx, int gy, int z) -> uint32_t {
    const float dy = gy + 0.5f - ridgeY;
    int k = dy < -0.6f ? (dy > -half * 0.55f ? 4 : 3) : (dy > 0.6f ? (dy < half * 0.5f ? 2 : 1) : 4);   // the north slope toward the light, the ridge
    const int row = (int)std::floor(std::fabs(dy));
    if (row % 3 == 2 && std::fabs(dy) > 0.6f) k = std::max(0, k - 1);   // courses
    if (cul != 5 && ((gx + (row & 1) * 2 + 64) % 4 == 0) && row % 3 != 2) k = std::max(0, k - 1);   // butt joints
    if (cul == 5 && (gx + 64) % 9 == 0) k = std::max(0, k - 1);   // the hides' seams
    if (gx == rx0 || gx == rx1) k = std::max(0, k - 1);   // the bargeboards' shade
    if (Z(gx - 1, gy - 1) > z + 1) k = std::max(0, k - 1);
    (void)z;
    return RF[k];
  };
  auto face = [&](int gx, int gy, int z, int h, int v, int zn) -> uint32_t {
    if (zn > 0 && zn >= z - 3 && h >= zn) return top(gx, gy, z);   // a step down a slope: part of the roof's surface
    (void)gy; (void)zn;
    if (h >= zg - 1 || v <= 1) return RF[v == 0 ? 3 : 1];             // the eave's edge
    if (gx < bx0 || gx > bx1) return kSkip;                          // under the overhang at the gable ends
    if (h >= zb) {                                                    // the open gallery
      const bool post = gx <= bx0 + 1 || gx >= bx1 - 1 || ((gx - bx0) % 9) <= 1;
      if (post) return kGLog[((gx - bx0) % 9) == 0 || gx == bx0 ? 3 : 2];
      if (h == zb + 2) return kWood[3];                              // the rail
      if (h == zb + 1) return kWood[1];
      return h >= zg - 3 ? rgba(20, 14, 24) : rgba(40, 30, 38);
    }
    if (h == zb - 1) return kWood[2];                                 // the gallery's floor beam
    if (h == zb - 2) return kWood[0];
    // the gateway: a lintel, braces, the dark passage (its lower part open to the road)
    if (gx >= ax0 && gx <= ax1) {
      if (h < aH - 6) return kClear;
      if (h < aH) return mix(rgba(30, 24, 40), rgba(48, 38, 54), (h - (aH - 6)) / 6.0f);
      if (h < aH + 3) return kWood[h == aH + 2 ? 4 : (h == aH ? 1 : 3)];
    }
    if ((gx == ax0 - 1 || gx == ax1 + 1) && h < aH + 3) return kWood[gx < 24 ? 4 : 1];   // the posts
    if ((gx == ax0 - 2 || gx == ax1 + 2) && h < aH + 3) return kWood[gx < 24 ? 3 : 0];
    // horizontal logs, each lit on its upper edge, notched ends at the corners
    const int lh = h % 3;
    int k = lh == 2 ? 3 : (lh == 0 ? 1 : 2);
    if ((gx <= bx0 + 1 || gx >= bx1 - 1) && lh != 0) k = std::min(4, k + 1);
    if (hash3(gx / 7, h / 3, 911u + seed) % 7 == 0 && lh == 1) k = std::max(1, k - 1);
    if (h < 2) k = 0;
    return kGLog[k];
  };
  field(s, rx0, rx1, ry0 - 1, ry1 + 1, Z, top, face);
  // the gate's leaves, standing open against the gateway's sides
  for (int side = 0; side < 2; side++)
    for (int i = 0; i < 4; i++) {
      const int x = GATE_OX + (side == 0 ? ax0 + i : ax1 - i);
      for (int h = 0; h < aH - 1 - i; h++) s.set(x, GATE_OY + by1 - h, kWood[(i % 2 == 0) ? 2 : 3 - (h % 5 == 0 ? 2 : 0)]);
    }
  // the finery: carved dragon heads on the ridge ends (fjords), horse-tail standards (steppe), the arms on the gallery
  const int rr = GATE_OY + (int)ridgeY - Z(rx0, (int)ridgeY);
  if (cul == 0) {
    for (int side = 0; side < 2; side++) {
      const int x = GATE_OX + (side == 0 ? rx0 : rx1);
      const int dir = side == 0 ? -1 : 1;
      for (int k = 0; k < 6; k++) { s.set(x + dir * (k / 2), rr - k, kGLog[k < 3 ? 2 : 3]); s.set(x + dir * (k / 2) + dir, rr - k, kGLog[1]); }
      s.set(x + dir * 3, rr - 6, kGLog[3]); s.set(x + dir * 4, rr - 5, kGLog[2]); s.set(x + dir * 2, rr - 7, rgba(196, 56, 40));
      s.set(x + dir * 4, rr - 4, rgba(232, 200, 90));   // the eye
    }
  } else if (cul == 5) {
    for (int side = 0; side < 2; side++) {
      const int x = GATE_OX + (side == 0 ? bx0 + 1 : bx1 - 1);
      const int y0 = GATE_OY + by1 - zg - 14;
      vline(c, x, y0, y0 + 12, kWood[1]);
      c.set(x, y0 - 1, kGold[4]);
      for (int j = 0; j < 7; j++) for (int i = -1; i <= 1; i++) if (i != 0 || j > 4) s.set(x + i, y0 + 2 + j, (j % 2) ? rgba(36, 30, 34) : rgba(64, 54, 58));
      vline(s, x, y0, y0 + 12, kWood[1]);
    }
  }
  // the arms hung from the gallery rail, between the posts
  for (int b = 0; b < 2; b++) {
    const int x = GATE_OX + (b == 0 ? bx0 + 11 : bx1 - 16);
    hangBanner(s, x, GATE_OY + ry1 - zb - 1, 6, 11, BF, BT, emblem, kc, true);
  }
  composeOver(c, s);
  return c;
}

// ================================================================================== the earthwork cut (highland ramparts)
// The bank ends either side of the road in its own sloped ends; a timber bridge-gate spans the cut: four tall posts, a
// deck at the bank's crest with a rail and a stake breastwork, its two leaves open under it, the arms on a pole.
Canvas earthworkGate(uint32_t seed, const Ramp& BF, const Ramp& BT, int emblem, bool kc, const bld::FortParts& F) {
  Canvas c(GATE_CW, GATE_CH);
  flankPiece(c, 0, false, F, seed);
  flankPiece(c, 1, false, F, seed);
  const int wallH = wallWalkHeight(F);
  const int zd = wallH + 1;                       // the deck
  Canvas s(GATE_CW, GATE_CH);
  auto post = [&](int gx, int gy) { return (gx == -3 || gx == -2 || gx == 49 || gx == 50) && (gy == 2 || gy == 3 || gy == 12 || gy == 13); };
  auto Z = [&](int gx, int gy) -> int {
    if (post(gx, gy)) return zd + 9;
    if (gx >= -4 && gx <= 51 && gy >= 3 && gy <= 12) {
      if ((gy == 3 || gy == 12) && ((gx + 64) % 3 == 1)) return zd + 5;   // the stakes along the deck's edges
      return zd;
    }
    return 0;
  };
  auto top = [&](int gx, int gy, int z) -> uint32_t {
    if (z > zd + 6) return kWood[4];
    if (z > zd) return kWood[(gx & 1) ? 3 : 4];
    int k = ((gx + 64) % 4 == 0) ? 1 : 2;   // planks across the road
    if (hash3(gx / 4, gy, 77u + seed) % 5 == 0) k = 3;
    if (gy == 3) k = 3;
    return kWood[k];
  };
  auto face = [&](int gx, int gy, int z, int h, int v, int zn) -> uint32_t {
    (void)gy; (void)zn;
    if (z > zd + 6) return kWood[gx == -3 || gx == 49 ? 3 : 1];               // the posts
    if (z > zd) return kWood[v == 0 ? 3 : 2];                                   // a stake
    if (h >= zd - 3) return kWood[v == 0 ? 3 : (h == zd - 3 ? 0 : 2)];          // the deck's edge beam
    if (h >= zd - 6 && ((gx >= 3 && gx <= 6) || (gx >= 41 && gx <= 44))) return kWood[1];   // braces
    return kSkip;                                                               // the road under the deck
  };
  field(s, -6, 54, 0, 16, Z, top, face);
  // the leaves standing open against the bank ends
  for (int side = 0; side < 2; side++)
    for (int i = 0; i < 4; i++) {
      const int x = GATE_OX + (side == 0 ? i : 47 - i);
      for (int h = 0; h < zd - 5 - i; h++) s.set(x, GATE_OY + 14 - h, kWood[(i % 2 == 0) ? 2 : 3 - (h % 5 == 0 ? 2 : 0)]);
    }
  // the arms on a pole at the middle of the deck
  vline(s, GATE_OX + 24, GATE_OY + 8 - zd - 14, GATE_OY + 8 - zd, kWood[1]);
  s.set(GATE_OX + 24, GATE_OY + 8 - zd - 15, kGold[4]);
  hangBanner(s, GATE_OX + 25, GATE_OY + 8 - zd - 13, 6, 10, BF, BT, emblem, kc, true);
  composeOver(c, s);
  return c;
}

// ================================================================================== the hedge gap (thorn walls)
// The hedge ends in rounded bushes either side; two carved gate posts (horse skulls on the steppe, painted spirit
// faces in the marsh), a wicker gate standing open, a cord strung between the posts hung with charms or prayer flags.
Canvas hedgeGap(uint32_t seed, const Ramp& BF, const Ramp& BT, int emblem, bool kc, const bld::FortParts& F) {
  Canvas c(GATE_CW, GATE_CH);
  flankPiece(c, 0, false, F, seed);
  flankPiece(c, 1, false, F, seed);
  const int cul = (int)F.culture - 1;
  Canvas s(GATE_CW, GATE_CH);
  const int ph = 26;
  for (int side = 0; side < 2; side++) {
    const int x = GATE_OX + (side == 0 ? 1 : 45), yb = GATE_OY + 13;
    for (int h = 0; h < ph; h++) {
      const int y = yb - h;
      s.set(x, y, kWood[3]); s.set(x + 1, y, kWood[2]); s.set(x + 2, y, kWood[1]);
      if (cul == 6 && h > ph - 10 && h % 4 == 1) { s.set(x, y, rgba(200, 70, 60)); s.set(x + 1, y, rgba(230, 190, 80)); }   // painted bands
    }
    hline(s, x - 1, x + 3, yb - ph, kWood[4]);
    if (cul == 5) {   // a horse skull on the post
      const int y = yb - ph - 4;
      for (int j = 0; j < 4; j++) for (int i = -1; i <= 3; i++) if (!(j == 3 && (i == -1 || i == 3))) s.set(x + i, y + j, kBone[(i < 1) ? 4 : 3]);
      s.set(x, y + 1, kInk); s.set(x + 2, y + 1, kInk);
    } else {   // a carved spirit face
      s.set(x, yb - ph + 4, kInk); s.set(x + 2, yb - ph + 4, kInk); hline(s, x, x + 2, yb - ph + 7, rgba(170, 50, 40));
    }
    // the wicker leaf, open toward the hedge
    for (int i = 0; i < 9; i++) {
      const int lx = side == 0 ? x + 3 + i : x - 1 - i;
      for (int h = 1; h < 14 - (i > 6 ? i - 6 : 0); h++) {
        const bool over = ((i / 2) + (h / 2)) & 1;
        s.set(lx, yb + 1 - h - i / 3, over ? kWood[(h % 2) ? 3 : 2] : kWood[(h % 2) ? 1 : 0]);
      }
    }
  }
  // the cord with its charms or flags
  for (int x = GATE_OX + 3; x <= GATE_OX + 45; x++) {
    const float t = (x - GATE_OX - 3) / 42.0f;
    const int y = GATE_OY + 13 - ph + 2 + (int)std::lround(std::sin(t * 3.14159f) * 5.0f);
    s.set(x, y, kCloth[1]);
    if ((x - GATE_OX) % 5 == 0) {
      static const uint32_t flags[5] = {rgba(52, 92, 168), rgba(236, 232, 220), rgba(196, 56, 46), rgba(56, 140, 80), rgba(232, 196, 70)};
      const uint32_t col = cul == 5 ? flags[((x - GATE_OX) / 5) % 5] : (((x - GATE_OX) / 5) % 2 ? kBone[3] : rgba(150, 110, 70));
      for (int j = 1; j <= 3; j++) { s.set(x, y + j, col); s.set(x + 1, y + j, cul == 5 ? mix(col, kInk, 0.25f) : col); }
    }
  }
  (void)BF; (void)BT; (void)emblem; (void)kc;
  composeOver(c, s);
  return c;
}

// ================================================================================== the iwan (dune)
// A tall rectangular pishtaq straddling the road: a frame of turquoise and cobalt tile bands round a deep niche whose
// pointed vault rises over the pointed doorway, honeycomb cells in the niche's head, a band of script across the top,
// stepped crenels; two drum towers with pointed merlons and small glazed domes flank it on the wall.
Canvas iwanGate(uint32_t seed, const Ramp& BF, const Ramp& BT, int emblem, bool kc, const bld::FortParts& F) {
  Canvas c(GATE_CW, GATE_CH);
  flankPiece(c, 0, true, F, seed);
  flankPiece(c, 1, true, F, seed);
  const Ramp TR = F.trim ? ramp(opaque(F.trim), 0.9f) : kGTeal;
  const Ramp& R = kGAdobe;
  const int wallH = wallWalkHeight(F);
  const int ZP = wallH + 22, ZT = wallH + 15;   // the pishtaq's top, the drums' crest
  const int px0 = -3, px1 = 50, py0 = 3, py1 = 15;
  const int t0x = -9, t1x = 56, tcy = 8, TRr = 11;
  static const int alm[6] = {0, 2, 4, 4, 2, 0};
  Canvas s(GATE_CW, GATE_CH);
  auto drum = [&](int gx, int gy, int cx, int& part) -> int {
    const float d = std::hypot(gx + 0.5f - cx, gy + 0.5f - tcy);
    if (d > TRr) return 0;
    if (d <= 5.0f) { part = 3; return ZT + 2 + (int)std::lround(std::sqrt(std::max(0.0f, 25.0f - d * d)) * 1.2f) + (d < 1.0f ? 3 : 0); }
    if (d > TRr - 2.2f) {
      part = 2;
      const float a = (std::atan2(gy + 0.5f - tcy, gx + 0.5f - cx) + PI) / TAU * 12.0f + 0.25f, fr = a - std::floor(a);
      return ZT + (((int)std::floor(a) & 1) ? 1 : (fr > 0.3f && fr < 0.7f ? 5 : 3));
    }
    part = 1;
    return ZT;
  };
  auto Z = [&](int gx, int gy) -> int {
    int part = 0, z = 0;
    if (gx >= px0 && gx <= px1 && gy >= py0 && gy <= py1) {
      const bool rim = gy <= py0 + 1 || gy >= py1 - 1 || gx <= px0 + 1 || gx >= px1 - 1;
      z = ZP + (rim ? 2 + alm[(((gy <= py0 + 1 || gy >= py1 - 1) ? gx : gy) % 6 + 6) % 6] : 0);
    }
    return std::max(z, std::max(drum(gx, gy, t0x, part), drum(gx, gy, t1x, part)));
  };
  auto which = [&](int gx, int gy, int& part) {
    part = 0;
    const int zp = (gx >= px0 && gx <= px1 && gy >= py0 && gy <= py1) ? ZP : 0;
    int p0 = 0, p1 = 0;
    const int z0 = drum(gx, gy, t0x, p0), z1 = drum(gx, gy, t1x, p1);
    if (zp && zp + 6 >= std::max(z0, z1)) return 2;
    if (z0 >= z1 && z0) { part = p0; return 0; }
    if (z1) { part = p1; return 1; }
    return 2;
  };
  auto top = [&](int gx, int gy, int z) -> uint32_t {
    int part = 0;
    const int w = which(gx, gy, part);
    if (w < 2) {
      const int cx = w == 0 ? t0x : t1x;
      const float dx = gx + 0.5f - cx, dy = gy + 0.5f - tcy;
      if (part == 3) {
        if (std::hypot(dx, dy) < 1.0f) return kGold[4];
        const int k = lightIndex(lightAt(std::clamp(dx / 5.2f, -0.95f, 0.95f) * 0.9f, std::clamp(dy / 5.2f, -0.95f, 0.95f) * 0.9f), gx, gy, 0.08f);
        return TR[std::clamp(k, 1, 4)];
      }
      if (part == 2) return R[z > ZT + 2 ? 4 : 3];
      return R[(gx + gy) % 4 == 0 ? 2 : 3];
    }
    if (z > ZP) return R[z > ZP + 3 ? 4 : 3];
    int k = (gx + gy * 3) % 7 == 0 ? 2 : 3;
    if (Z(gx - 1, gy - 1) > z + 1) k = 2;
    return R[k];
  };
  auto face = [&](int gx, int gy, int z, int h, int v, int zn) -> uint32_t {
    (void)gy;
    int part = 0;
    const int w = which(gx, gy, part);
    if (w < 2) {   // a drum: cylinder light, a glazed band under the crest, beam ends, a slit
      const int cx = w == 0 ? t0x : t1x;
      const float u = (gx + 0.5f - cx) / (part == 3 ? 5.0f : (float)TRr);
      int base = std::clamp(lightIndex(lightAt(std::clamp(u, -0.95f, 0.95f) * 0.95f, 0.15f), gx, h, 0.12f), 1, 3);
      if (part == 3 && h > ZT) return TR[std::clamp(base + 1, 1, 4)];
      if (v == 0 || (z > ZT && h >= ZT - 1)) return R[std::min(4, base + 1)];
      if (h == ZT - 2) return R[base - 1];
      if (h >= ZT - 5 && h <= ZT - 4) return TR[base];
      if (h == ZT - 7 && ((gx + 64) % 4) == 1) return kWood[1];
      if (zn == 0 && std::abs(gx - cx) == 0 && h >= 12 && h <= 18) return kInk;
      if (h < 3) return R[base - 1];
      return R[std::clamp(base + ((hash3(gx / 3, h / 3, 19u) % 7) == 0 ? -1 : 0), 0, 4)];
    }
    if (zn > 0) return R[v == 0 ? 3 : 1];
    // the pishtaq's south face
    const int x = gx;
    if (v == 0 || h >= ZP + 1) return R[std::min(4, (x < 0 ? 4 : 3))];                   // the crenels and the lip
    if (h == ZP) return R[1];
    const bool fx = x <= px0 + 3 || x >= px1 - 3;                                          // the frame's sides
    if (h >= ZP - 4) {                                                                       // the band of script
      if (h == ZP - 4 || h == ZP - 1) return TR[1];
      return ((x * 7 + h * 3) % 5 == 0 || ((x + h) % 4 == 0 && h == ZP - 2)) ? kGWhite[3] : rgba(28, 54, 112);
    }
    if (fx) {                                                                                // the tiled frame
      const int q = (x + h + 64) % 4;
      return x <= px0 + 1 || x >= px1 - 1 ? R[x < 24 ? 3 : 1] : (q == 0 ? TR[4] : (q == 2 ? rgba(28, 54, 112) : TR[2]));
    }
    // the niche: its pointed head (a recess in shade), honeycomb cells, the doorway
    const int nTop = pointedArch(x + 0.5f, px0 + 4.0f, px1 - 3.0f, ZP - 18, ZP - 6);
    const int dTop = pointedArch(x + 0.5f, 12.0f, 36.0f, 14, 24);
    if (nTop < 0 || h >= nTop + 2) {                                                         // the frame's spandrels
      if (h >= nTop && nTop >= 0) return TR[3];
      const bool star = ((x + 64) % 6 == 3 && (h % 6) == 3);
      return star ? TR[4] : ((x + h) % 3 == 0 ? TR[1] : TR[2]);
    }
    if (h >= nTop) return TR[4];                                                             // the niche's lit rim
    if (dTop >= 0 && h < dTop) {                                                             // the doorway
      if (h < dTop - 6) return kClear;
      return mix(rgba(30, 22, 36), rgba(52, 40, 50), (h - (dTop - 6)) / 6.0f);
    }
    if (dTop >= 0 && h < dTop + 2) return R[4];                                              // its lit arch ring
    if (h >= ZP - 14) {                                                                      // muqarnas cells in the head
      const int row = (ZP - 14 - h) / 3, col = (x + (row & 1) * 2 + 64) / 4;
      const bool lit = ((h % 3) == 0) || (((x + (row & 1) * 2 + 64) % 4) == 0);
      return lit ? R[3] : ((col + row) % 2 ? R[1] : TR[1]);
    }
    const int k = (x < 10 || x > 37) ? 2 : 1;                                                // the recess in shade
    return R[std::clamp(k + ((hash3(x / 3, h / 3, 23u) % 9) == 0 ? -1 : 0), 0, 4)];
  };
  field(s, -22, 70, -4, 21, Z, top, face);
  // the arms on the drums (long banners), lamps either side of the doorway
  for (int side = 0; side < 2; side++) {
    const int cx = side == 0 ? t0x : t1x;
    hangBanner(s, GATE_OX + cx - 3, GATE_OY + 15 - (ZT - 7), 6, 15, BF, BT, emblem, kc, true);
    const int lx = GATE_OX + (side == 0 ? 9 : 38), ly = GATE_OY + 15 - 20;
    lantern(s, lx, ly, ramp(rgba(244, 196, 110)));
  }
  composeOver(c, s);
  return c;
}

// ================================================================================== the gate pavilion (jade cities)
// A brick gate base with a round-arched tunnel, and on it a timber pavilion of red columns and lattice under a double-
// eaved hip-and-gable roof of glazed tiles, its corners swept up, beasts on the ridge's ends, a blue name board under the
// upper eave, red lanterns hanging from the lower one.
Canvas paifangGate(uint32_t seed, const Ramp& BF, const Ramp& BT, int emblem, bool kc, const bld::FortParts& F) {
  Canvas c(GATE_CW, GATE_CH);
  flankPiece(c, 0, true, F, seed);
  flankPiece(c, 1, true, F, seed);
  const Ramp T = F.roof ? ramp(opaque(F.roof), 0.9f) : kGJadeTile;
  const int wallH = wallWalkHeight(F);
  const int ZB = wallH + 4, ZC = ZB + 11;   // the brick base's top, the pavilion's eaves
  const int bx0 = -4, bx1 = 51;
  const int rx0 = -9, rx1 = 56, ry0 = -4, ry1 = 19;
  const float ry = (ry0 + ry1) * 0.5f;
  Canvas s(GATE_CW, GATE_CH);
  auto Z = [&](int gx, int gy) -> int {
    if (gx < rx0 || gx > rx1 || gy < ry0 || gy > ry1) return 0;
    const float d = std::fabs(gy + 0.5f - ry), e = (float)std::min(gx - rx0, rx1 - gx);   // across the slope, from the gable end
    int z;
    if (d > 7.0f) z = ZC + (int)std::lround((12.0f - d) * 0.6f);                       // the lower eave
    else z = ZC + 5 + (int)std::lround((7.5f - d) * 1.45f);                            // the upper roof
    if (e < 4.0f && d > 7.0f) z += (int)std::lround((4.0f - e) * 0.9f);                  // the swept-up corners
    if (e < 3.0f && d <= 1.0f) z += 4;                                                  // the ridge beasts
    return z;
  };
  auto top = [&](int gx, int gy, int z) -> uint32_t {
    const float dy = gy + 0.5f - ry, d = std::fabs(dy);
    const int e = std::min(gx - rx0, rx1 - gx);
    if (e < 3 && d <= 1.0f) return gx < 24 ? kGold[4] : kGold[2];
    if (d <= 1.0f) return T[4];                                                       // the ridge
    int k = dy < 0 ? 3 : 2;
    if (d > 7.0f) k = dy < 0 ? 4 : 2;                                                 // the lower eave
    if (((int)std::floor(gx + 64.0f)) % 3 == 0) k = std::max(0, k - 1);               // rows of round tiles down the slope
    if (std::fabs(d - 7.0f) < 0.6f) return T[0];                                      // the step between the eaves
    if (e <= 1) k = std::max(0, k - 1);
    (void)z;
    return T[k];
  };
  auto face = [&](int gx, int gy, int z, int h, int v, int zn) -> uint32_t {
    if (zn > 0 && zn >= z - 3 && h >= zn) return top(gx, gy, z);   // a step down a slope: part of the roof's surface
    (void)gy; (void)zn;
    if (v <= 1 && h >= ZC - 1) return T[v == 0 ? 4 : 1];                              // the eave's tile ends
    if (h >= ZC - 1) return T[1];
    if (gx < bx0 || gx > bx1) return kSkip;                                          // under the swept corners
    if (h >= ZB) {                                                                     // the pavilion under its eaves
      if (h >= ZC - 2) return rgba(40, 26, 30);                                       // the eave's deep shade
      const bool col = gx == bx0 || gx == bx0 + 1 || gx == bx1 || gx == bx1 - 1 || std::abs(gx - 15) <= 0 || std::abs(gx - 32) <= 0 || gx == 16 || gx == 33;
      if (col) return kGLacquer[(gx == bx0 || gx == 15 || gx == 32) ? 3 : 2];
      if (gx >= 18 && gx <= 29 && h >= ZC - 7 && h <= ZC - 3) {                        // the name board
        if (h == ZC - 7 || h == ZC - 3 || gx == 18 || gx == 29) return kGold[2];
        return ((gx + h) % 3 == 0 && gx > 19 && gx < 28) ? kGold[4] : rgba(30, 50, 110);
      }
      if (h == ZB) return kGLacquer[0];
      if (h == ZB + 1 || h == ZB + 5) return kGLacquer[2];                             // rails
      return ((gx + h) % 3 == 0 || (gx - h + 99) % 3 == 0) ? kGLacquer[1] : rgba(56, 34, 40);   // lattice
    }
    // the brick base and its tunnel
    const float u = (gx + 0.5f - 24.0f) / 11.5f;
    const int at = std::fabs(u) <= 1.0f ? 12 + (int)std::lround(9.0f * std::sqrt(std::max(0.0f, 1 - u * u))) : -1;
    if (at >= 0 && h < at) {
      if (h < at - 6) return kClear;
      return mix(rgba(26, 22, 34), rgba(46, 40, 52), (h - (at - 6)) / 6.0f);
    }
    if (at >= 0 && h < at + 2) return kGStoneRing[h == at ? 2 : 4];                  // the arch's dressed ring
    if (h >= ZB - 3) return kGLacquer[h == ZB - 1 ? 3 : 2];                          // a lacquered band
    if (h < 3) return kGStoneRing[1];
    const int row = h / 3, hh = h % 3, bx = gx + (row & 1) * 3;
    int k = (hh == 0 || ((bx % 6) + 6) % 6 == 0) ? 1 : (hh == 2 ? 3 : 2);
    if (gx < bx0 + 3) k = std::min(4, k + 1);
    return kGJadeBrick[k];
  };
  field(s, rx0, rx1, ry0 - 1, ry1 + 1, Z, top, face);
  // red lanterns from the lower eave, the arms on two long banners down the base
  const Ramp L = ramp(rgba(214, 60, 46));
  for (int lx : {4, 43}) lantern(s, GATE_OX + lx, GATE_OY + ry1 - ZC + 2, L);
  for (int side = 0; side < 2; side++)
    hangBanner(s, GATE_OX + (side == 0 ? 2 : 40), GATE_OY + 15 - (ZB - 2), 5, 13, BF, BT, emblem, kc, false);
  composeOver(c, s);
  return c;
}

// ================================================================================== the moon gate (jade towns)
// A white garden wall set into the brick wall over the road, a round opening ringed in grey stone, leak windows of
// lattice either side, a hood of green glazed tiles along its top.
Canvas moonGate(uint32_t seed, const Ramp& BF, const Ramp& BT, int emblem, bool kc, const bld::FortParts& F) {
  Canvas c(GATE_CW, GATE_CH);
  flankPiece(c, 0, true, F, seed);
  flankPiece(c, 1, true, F, seed);
  const Ramp T = F.roof ? ramp(opaque(F.roof), 0.9f) : kGJadeTile;
  const int wallH = wallWalkHeight(F);
  const int ZW = wallH + 6;
  const int x0 = -3, x1 = 50, y0 = 2, y1 = 13;
  const float mcx = 24.0f, mcy = 12.0f, mr = 11.5f;
  Canvas s(GATE_CW, GATE_CH);
  auto Z = [&](int gx, int gy) -> int {
    if (gx < x0 || gx > x1 || gy < y0 || gy > y1) return 0;
    const int d = std::min(gy - y0, y1 - gy);
    return ZW - 3 + std::min(3, d);   // a rounded hood
  };
  auto top = [&](int gx, int gy, int z) -> uint32_t {
    const int d = std::min(gy - y0, y1 - gy);
    if (d >= 3) return T[4];
    int k = gy - y0 < y1 - gy ? 3 : 2;
    if ((gx + 64) % 3 == 0) k = std::max(0, k - 1);
    (void)z;
    return T[k];
  };
  auto face = [&](int gx, int gy, int z, int h, int v, int zn) -> uint32_t {
    if (zn > 0 && zn >= z - 3 && h >= zn) return top(gx, gy, z);   // a step down a slope: part of the roof's surface
    (void)gy; (void)z; (void)zn;
    if (v == 0 || h >= ZW - 3) return T[v == 0 ? 4 : 1];
    if (h == ZW - 4) return kGPlaster[0];                                   // the hood's shade
    const float dx = gx + 0.5f - mcx, dh = h + 0.5f - mcy, r = std::sqrt(dx * dx + dh * dh);
    if (r < mr - 1.6f) return kClear;                            // the opening
    if (r < mr + 0.6f) {                                                    // the stone ring, lit on its upper left
      const float a = std::atan2(dh, dx);
      return kGStoneRing[(a > 0.6f && a < 2.9f) ? 4 : ((a < -0.6f && a > -2.6f) ? 1 : 3)];
    }
    for (int wx : {6, 41}) {                                                // the leak windows
      const float ex = gx + 0.5f - wx, eh = h + 0.5f - 14.0f;
      if (ex * ex / 16.0f + eh * eh / 20.0f < 1.0f) {
        if (ex * ex / 9.0f + eh * eh / 13.0f > 1.0f) return kGStoneRing[2];
        return ((gx + h) % 2 == 0) ? kGStoneRing[3] : rgba(44, 58, 52);
      }
    }
    if (h < 2) return kGStoneRing[1];
    int k = 3;
    if (gx <= x0 + 1) k = 4;
    if (hash3(gx / 4, h / 3, 61u + seed) % 13 == 0) k = 2;
    return kGPlaster[k];
  };
  field(s, x0, x1, y0 - 1, y1 + 1, Z, top, face);
  // a small plaque over the opening in the arms' colours
  const int px = GATE_OX + 21, py = GATE_OY + y1 - (int)(mcy + mr) - 6;
  for (int j = 0; j < 4; j++) for (int i = 0; i < 6; i++) s.set(px + i, py + j, (i == 0 || j == 0) ? BF[3] : (i == 5 || j == 3 ? BF[1] : BF[2]));
  if (kc) for (int i = 1; i < 5; i++) s.set(px + i, py + 1 + (i & 1), BT[3]);
  (void)emblem;
  composeOver(c, s);
  return c;
}

// ================================================================================== the elven arch (starspire)
// A slender white arch block with a pointed arch and a tall carved gable over the road, two slender round towers
// under tall blue cones with gilded finials, teal bands, lancet windows glowing faintly.
Canvas elvenArch(uint32_t seed, const Ramp& BF, const Ramp& BT, int emblem, bool kc, const bld::FortParts& F) {
  Canvas c(GATE_CW, GATE_CH);
  flankPiece(c, 0, true, F, seed);
  flankPiece(c, 1, true, F, seed);
  const Ramp& R = kGWhite;
  const int wallH = wallWalkHeight(F);
  const int ZT = wallH + 18, ZA = wallH + 9;
  const int t0x = -8, t1x = 56, tcy = 8;
  const float TRd = 8.0f;
  Canvas s(GATE_CW, GATE_CH);
  auto tower = [&](int gx, int gy, int cx, int& part) -> int {
    const float d = std::hypot(gx + 0.5f - cx, gy + 0.5f - tcy);
    if (d > TRd + 1.0f) return 0;
    part = d > TRd ? 2 : 1;   // 2: the spire's eave beyond the drum
    return ZT + 1 + (int)std::lround(17.0f * (1 - d / (TRd + 1.0f))) + (d < 0.9f ? 3 : 0);   // a tall straight spire
  };
  auto Z = [&](int gx, int gy) -> int {
    int p = 0, z = 0;
    if (gx >= 0 && gx <= 47 && gy >= 4 && gy <= 12) z = ZA + std::max(0, 7 - (int)std::lround(std::fabs(gx + 0.5f - 24.0f) / 3.4f));
    return std::max(z, std::max(tower(gx, gy, t0x, p), tower(gx, gy, t1x, p)));
  };
  auto towerOf = [&](int gx, int gy, int& part, int& cx) {
    int p0 = 0, p1 = 0;
    const int z0 = tower(gx, gy, t0x, p0), z1 = tower(gx, gy, t1x, p1);
    if (z0) { part = p0; cx = t0x; return true; }
    if (z1) { part = p1; cx = t1x; return true; }
    return false;
  };
  auto top = [&](int gx, int gy, int z) -> uint32_t {
    int part = 0, cx = 0;
    if (towerOf(gx, gy, part, cx)) {
      const float dx = gx + 0.5f - cx, dy = gy + 0.5f - tcy, d = std::hypot(dx, dy);
      if (d < 1.2f) return kGold[4];
      const float l = (-dx * 0.72f - dy * 0.5f) / std::max(1.0f, d);
      int k = l > 0.45f ? 4 : (l > 0.0f ? 3 : (l > -0.45f ? 2 : 1));
      if (((int)std::floor(d)) % 3 == 0) k = std::max(0, k - 1);
      return kGSlate[k];
    }
    if (z > ZA + 1) return R[gx < 24 ? 4 : 3];   // the gable's slopes
    return R[(gx + gy) % 5 == 0 ? 2 : 3];
  };
  auto face = [&](int gx, int gy, int z, int h, int v, int zn) -> uint32_t {
    if (zn > 0 && zn >= z - 3 && h >= zn) return top(gx, gy, z);   // a step down a slope: part of the roof's surface
    (void)gy;
    int part = 0, cx = 0;
    if (towerOf(gx, gy, part, cx)) {
      const float u = (gx + 0.5f - cx) / TRd;
      const int base = std::clamp(lightIndex(lightAt(std::clamp(u, -0.95f, 0.95f) * 0.95f, 0.15f), gx, h, 0.12f), 1, 3);
      if (h > ZT) return kGSlate[std::max(0, base - 1)];                               // the cone's eave
      // (the spire's 1 px eave: its face below is the drum's, or the drum would hide behind it)
      if (h == ZT || h == ZT - 1) return R[0];
      if (std::fabs(u) > 0.9f) return u < 0 ? kGSlate[2] : kGSlate[1];   // the drum's rounded flanks against the white wall
      if (h == ZT - 4 || h == ZT - 5 || h == 9 || h == 10) return kGTeal[base];         // teal bands
      if (zn == 0 && std::fabs(u) < 0.15f && ((h >= ZT - 13 && h <= ZT - 7) || (h >= 14 && h <= 19))) return (h == ZT - 13 || h == 14) ? kGElfGlow[1] : kGElfGlow[2 + (h & 1)];
      if (h < 3) return R[base - 1];
      const int row = h / 5, hh = h % 5, bx = gx + (row & 1) * 4;
      return R[std::clamp((hh == 0 || ((bx % 9) + 9) % 9 == 0) ? base - 1 : base, 0, 4)];
    }
    if (zn > 0) return R[v == 0 ? 3 : 1];
    // the arch block's face: the pointed arch, its moulded ring, a star over the apex, the gable's carved edge
    const int at = pointedArch(gx + 0.5f, 5.0f, 43.0f, 9, 25);
    if (at >= 0 && h < at) {
      if (h < at - 6) return kClear;
      return mix(rgba(28, 30, 50), rgba(56, 60, 84), (h - (at - 6)) / 6.0f);
    }
    if (at >= 0 && h < at + 2) return h == at ? kGTeal[2] : R[4];
    if (v == 0 || (z > ZA && h >= ZA)) return R[v == 0 ? 4 : 3];                         // the gable's lit slopes
    if (h == ZA - 1) return R[1];
    if (std::abs(gx - 24) <= 1 && h >= ZA - 6 && h <= ZA - 3) return (gx == 24 || h == ZA - 5) ? kGElfGlow[4] : kGElfGlow[2];   // a star
    if (h == ZA - 8) return kGTeal[2];
    if (h < 3) return R[1];
    const int row = h / 5, hh = h % 5, bx = gx + (row & 1) * 5;
    return R[(hh == 0 || ((bx % 10) + 10) % 10 == 0) ? 1 : (hh == 4 ? 3 : 2)];
  };
  field(s, -18, 66, -2, 18, Z, top, face);
  // slender pennants from the finials (the arms' colours)
  for (int side = 0; side < 2; side++) {
    const int cx = side == 0 ? t0x : t1x;
    hangBanner(s, GATE_OX + cx - 2, GATE_OY + tcy + (int)TRd - (ZT - 6), 5, 13, BF, BT, emblem, kc, true);
  }
  composeOver(c, s);
  return c;
}

// ================================================================================== the living arch (sylvan)
// Two great trees grown either side of the road where the hedge ends, their roots buttressing the ground, their
// branches trained over the road into a pointed arch and their crowns grown together into one canopy; glow-orbs hang
// in it.
Canvas livingArch(uint32_t seed, const Ramp& BF, const Ramp& BT, int emblem, bool kc, const bld::FortParts& F) {
  Canvas c(GATE_CW, GATE_CH);
  flankPiece(c, 0, false, F, seed);
  flankPiece(c, 1, false, F, seed);
  const int wallH = wallWalkHeight(F);
  const int ZC = wallH + 18;
  const float t0 = -7.5f, t1 = 55.5f, tcy = 9.0f;
  Canvas s(GATE_CW, GATE_CH);
  auto canopy = [&](int gx, int gy) -> int {   // the crown: lumpy, highest over the trunks, a saddle over the road
    const float ex = (gx + 0.5f - 24.0f) / 40.0f, ey = (gy + 0.5f - 5.0f) / 11.0f;
    const float n = vnoise(gx / 4.0f, gy / 4.0f, 701u + seed % 97u);
    if (ex * ex + ey * ey > 1.0f - (n - 0.5f) * 0.35f) return 0;
    const float hump = std::max(std::exp(-((gx - t0) * (gx - t0)) / 160.0f), std::exp(-((gx - t1) * (gx - t1)) / 160.0f));
    return ZC + (int)std::lround(4.0f + 7.0f * hump * std::sqrt(std::max(0.0f, 1 - ey * ey)) + n * 4.0f);
  };
  auto Z = [&](int gx, int gy) -> int { return canopy(gx, gy); };
  auto top = [&](int gx, int gy, int z) -> uint32_t {
    const float n = hashf(gx, gy, 703u);
    int k = 3;
    if (Z(gx - 1, gy - 1) > z || Z(gx - 1, gy) > z + 1) k = 2;
    if (Z(gx + 1, gy + 1) < z - 1) k = 2;
    if (Z(gx - 1, gy - 1) < z - 1 && Z(gx, gy - 1) < z) k = 4;
    if (n < 0.12f) k = std::max(1, k - 1);
    if (n > 0.95f) return rgba(236, 226, 160);   // blossom
    return kGLeaf[k];
  };
  auto face = [&](int gx, int gy, int z, int h, int v, int zn) -> uint32_t {
    (void)gy; (void)zn;
    const float n = hashf(gx, h, 705u);
    if (h >= z - 4 || v <= 2) return kGLeaf[std::clamp((v == 0 ? 3 : 2) - (n < 0.2f ? 1 : 0), 0, 4)];   // the crown's flank
    // under the crown: the trunks, the arching branches, the opening
    const float u = (gx + 0.5f - 24.0f) / 31.5f;
    const int at = std::fabs(u) <= 1.0f ? 8 + (int)std::lround(18.0f * std::pow(1 - std::fabs(u), 0.5f)) : 0;   // the arch's soffit
    const float dt = std::min(std::fabs(gx + 0.5f - t0), std::fabs(gx + 0.5f - t1));
    const float tw = 3.6f + (h < 6 ? (6 - h) * 0.9f : 0.0f);   // the trunk, flaring into its roots
    if (dt < tw) {
      const float tu = ((gx + 0.5f) < 24.0f ? gx + 0.5f - t0 : gx + 0.5f - t1) / tw;
      int k = tu < -0.4f ? 3 : (tu > 0.45f ? 1 : 2);
      if (hash3(gx, h / 3, 707u) % 6 == 0) k = std::max(0, k - 1);
      if (h > 4 && hash3(gx / 2, h / 4, 709u) % 11 == 0) return rgba(92, 128, 60);   // moss
      return kGBark[k];
    }
    if (at && h >= at && h < at + 4) return kGBark[h == at ? 1 : (h == at + 3 ? 3 : 2)];   // the trained branches
    if (at && h >= at + 4) return kGLeaf[(n < 0.3f) ? 0 : 1];                                // the crown's shaded underside
    return kSkip;
  };
  field(s, -26, 74, -8, 18, Z, top, face);
  // glow-orbs hanging in the arch
  const Ramp G = kGElfGlow;
  for (int i = 0; i < 3; i++) {
    const int ox = GATE_OX + 10 + i * 14, oy = GATE_OY + 15 - 24 + (i == 1 ? -3 : 0);
    vline(s, ox, oy - 4, oy - 1, kGBark[1]);
    ball(s, ox + 0.5, oy + 1.0, 1.8, 1.8, G);
  }
  (void)BF; (void)BT; (void)emblem; (void)kc;
  composeOver(c, s);
  return c;
}

}  // namespace

// the gate forms art_walls.cpp's gateHouse(seed, arms, parts) hands over (the masonry gatehouses stay there)
Canvas gateHouseForm(uint32_t seed, uint32_t field, uint32_t trim, int emblem, const bld::FortParts& f) {
  const bool kc = field != 0;
  const Ramp BF = kc ? ramp(opaque(field)) : kRed;
  const Ramp BT = kc ? ramp(opaque(trim ? trim : rgba(232, 200, 90))) : kGold;
  switch (f.gate) {
    case bld::GateForm::TimberGate: return timberGate(seed, BF, BT, emblem, kc, f);
    case bld::GateForm::Earthwork: return earthworkGate(seed, BF, BT, emblem, kc, f);
    case bld::GateForm::HedgeGap: return hedgeGap(seed, BF, BT, emblem, kc, f);
    case bld::GateForm::Iwan: return iwanGate(seed, BF, BT, emblem, kc, f);
    case bld::GateForm::Paifang: return paifangGate(seed, BF, BT, emblem, kc, f);
    case bld::GateForm::MoonGate: return moonGate(seed, BF, BT, emblem, kc, f);
    case bld::GateForm::ElvenArch: return elvenArch(seed, BF, BT, emblem, kc, f);
    case bld::GateForm::LivingArch: return livingArch(seed, BF, BT, emblem, kc, f);
    default: return gateHouse(seed, field, trim, emblem, f.wall);
  }
}

}  // namespace art
