// EMBERVALE art, M5 "Hearth and Hall": poses, seats and beds, speech bubbles, festival dressing, lamplight
// (rpg/art/art_life.h). ART lane. The village animals are painted in rpg/art/art_critters.cpp.
//
// The poses are painted on the human rig itself (rpg/art/art_human.cpp PostureRig): the head, the torso and the
// garment are the look's own (every outfit, culture cut, headwear and people), the legs and arms are the pose's: the
// knees apart on a chair seen from above, cross-legged on a cushion, a mug or bread to the mouth, the smith's hammer
// arc, the broom's sweep, the bard's instrument in the culture's style. The sleeper is fitted to each kind of bed: the
// head on the pillow, the body a soft mound under the bed's own quilt. Same camera (high 3/4 top-down), top-left
// light, 1 px darkened-neighbour outline and palette as every other sprite.
#include "rpg/art/art_internal.h"

namespace art {

// art_human.cpp: the sleeper's head and shoulders (hats off, eyes shut) in a 16-wide column at x offset ox
void paintSleeperHead(Canvas& c, const HumanLook& look, int ox, int headTop, int frame);

// ================================================================ postures
PostureInfo postureInfo(Posture p) {
  PostureInfo i;
  switch (p) {
    case Posture::Sit: case Posture::SitFloor: i.frames = 1; i.fps = 1; i.seated = true; break;
    case Posture::SitEat: case Posture::SitDrink: case Posture::SitFloorEat: case Posture::SitFloorDrink:
      i.frames = 2; i.fps = 1.2f; i.seated = true; break;
    case Posture::Eat: case Posture::Drink: i.frames = 2; i.fps = 1.2f; break;
    case Posture::Cheer: i.frames = 2; i.fps = 2.5f; break;
    case Posture::Hammer: case Posture::Chop: i.frames = 2; i.fps = 2.5f; break;
    case Posture::Hoe: i.frames = 2; i.fps = 1.6f; break;
    case Posture::Sweep: i.frames = 2; i.fps = 2.0f; break;
    case Posture::Stir: i.frames = 4; i.fps = 4.0f; break;
    case Posture::Carry: case Posture::Play: i.frames = 4; i.fps = 8; break;
    case Posture::Dance: i.frames = 4; i.fps = 5; break;
    case Posture::Fish: i.frames = 2; i.fps = 0.8f; break;
    case Posture::Sleep: i.frames = 2; i.fps = 0.5f; i.lying = true; i.oneRow = true; break;
    case Posture::Wave: i.frames = 2; i.fps = 4; break;
    case Posture::Lute: case Posture::Drum: case Posture::Flute: i.frames = 2; i.fps = 3; break;
    case Posture::Pray: i.frames = 2; i.fps = 0.6f; break;
    case Posture::Beg: i.frames = 2; i.fps = 1.2f; break;
    case Posture::Lamp: i.frames = 2; i.fps = 1.5f; break;
    case Posture::Read: i.frames = 2; i.fps = 0.4f; break;
    default: break;
  }
  return i;
}

namespace {
// the soft mound of a body under a blanket, as light and shade over whatever cloth is below (alpha only), in a
// vertical column: rows y0..y1, centred on cx, the shoulders hw wide tapering to the feet
void bodyMound(Canvas& c, float cx, int y0, int y1, float hw, int frame) {
  const uint32_t lit = withA(kLightHue, 0), dark = withA(kShadowHue, 0);
  const int n = std::max(3, y1 - y0 + 1);
  for (int y = y0; y <= y1; y++) {
    const float t = (y - y0 + 0.5f) / n;                       // 0 shoulders .. 1 feet
    float w = hw * (1.0f - 0.32f * t);
    if (t < 0.12f) w *= 0.86f + t;                              // the shoulders round off under the edge
    const bool chest = t > 0.08f && t < 0.38f;
    for (int x = (int)std::floor(cx - w - 1); x <= (int)std::ceil(cx + w + 1); x++) {
      const float u = (x + 0.5f - cx) / w;                      // -1 west edge .. 1 east edge
      int a = 0;
      uint32_t col = 0;
      if (u < -1.0f || u > 1.25f) continue;
      if (u > 1.0f) { col = dark; a = 110; }                    // the blanket falls away on the shaded side
      else if (u > 0.55f) { col = dark; a = 64 + (int)((u - 0.55f) * 90); }
      else if (u < -0.62f) { col = lit; a = 84; }               // the lit flank
      else if (u < -0.2f && chest) { col = lit; a = (frame & 1) ? 76 : 50; }   // the chest rises and falls
      if (t > 0.86f && std::fabs(u) < 0.95f) {                  // the feet: two little humps, their tips lit
        const bool tip = y == y1 - 1 && (std::fabs(u + 0.45f) < 0.25f || std::fabs(u - 0.35f) < 0.25f);
        if (tip) { col = lit; a = 96; }
        else if (y == y1) { col = dark; a = 96; }
      }
      if (a) {
        const uint32_t under = c.get(x, y);
        if (chA(under) == 255 || !chA(under)) c.set(x, y, withA(col, a));
      }
    }
  }
}
// composite an alpha-only layer onto another alpha-only layer (keeps the stronger)
void overAlpha(Canvas& dst, const Canvas& src, int ox, int oy) {
  for (int y = 0; y < src.h; y++)
    for (int x = 0; x < src.w; x++) {
      const uint32_t p = src.get(x, y);
      if (!chA(p)) continue;
      const uint32_t d = dst.get(ox + x, oy + y);
      if (chA(p) == 255 || chA(p) >= chA(d)) dst.set(ox + x, oy + y, p);
    }
}
// the head layer outlined on its own (the pillow round it darkens: the dent the head makes)
Canvas outlinedHead(const HumanLook& look, int w, int h, int ox, int headTop, int frame) {
  Canvas hd(w, h);
  paintSleeperHead(hd, look, ox, headTop, frame);
  outline(hd, 0.85f);
  return hd;
}
}  // namespace

Canvas humanPostureCellRaw(const HumanLook& look, Posture p, uint8_t variant, int row, int frame);

Canvas humanPostureSheet(const HumanLook& look, Posture p) { return humanPostureSheet(look, p, 0); }

Canvas humanPostureSheet(const HumanLook& look, Posture p, uint8_t variant) {
  Canvas out(HUMAN_W * POSTURE_FRAMES, HUMAN_H * 3);
  if (p == Posture::Sleep) {
    // the generic sleeper (the two-tile bed's fit; see bedFit / sleeperSprite for every berth): the head on the pillow at
    // the top of the cell, the body a mound under the quilt below it
    for (int f = 0; f < POSTURE_FRAMES; f++) {
      Canvas cell(HUMAN_W, HUMAN_H);
      bodyMound(cell, 8.0f, 11, 22, 5.0f, f);
      const Canvas hd = outlinedHead(look, HUMAN_W, HUMAN_H, 0, 2, f);
      blit(cell, hd, 0, 0);
      for (int row = 0; row < 3; row++) place(out, cell, f, row);
    }
    return out;
  }
  const int nf = std::clamp((int)postureInfo(p).frames, 1, POSTURE_FRAMES);
  for (int row = 0; row < 3; row++)
    for (int f = 0; f < POSTURE_FRAMES; f++) {
      Canvas cell = humanPostureCellRaw(look, p, variant, row, f % nf);
      outline(cell);
      place(out, cell, f, row);
    }
  return out;
}

Posture bardPosture(const MusicStyle& s) {
  switch (s.lead) {
    case LeadInst::Flute: case LeadInst::Pipes: case LeadInst::Reed: case LeadInst::Horn: case LeadInst::Brass:
    case LeadInst::Bells: case LeadInst::Voice:
      return Posture::Flute;
    case LeadInst::Marimba: return Posture::Drum;
    default: return Posture::Lute;
  }
}
uint8_t bardVariant(const MusicStyle& s) {
  if (s.lead == LeadInst::Marimba) return (uint8_t)PercKind::Wood;
  return (uint8_t)s.lead;
}

// ================================================================ seats
Posture seatedPosture(Prop seat, Posture p) {
  const bool floor = seat == Prop::Cushion || seat == Prop::SleepingMat || seat == Prop::Bedroll || seat == Prop::Rug || seat == Prop::COUNT ||
                     seat == Prop::LowTable;
  switch (p) {
    case Posture::Sit: case Posture::SitFloor: return floor ? Posture::SitFloor : Posture::Sit;
    case Posture::SitEat: case Posture::SitFloorEat: return floor ? Posture::SitFloorEat : Posture::SitEat;
    case Posture::SitDrink: case Posture::SitFloorDrink: return floor ? Posture::SitFloorDrink : Posture::SitDrink;
    default: return p;
  }
}

SeatFit seatFit(Prop seat, int kit, int facing) {
  (void)kit;
  SeatFit f;
  const int fc = facing == 3 ? 2 : facing;   // left mirrors right
  f.front = true;
  switch (seat) {
    case Prop::Chair:   // the back to the north: a sitter faces down (to its table) or sideways, never into the back
      if (fc == 1) return f;
      f.ok = true; f.seatY = 9; f.ay = fc == 0 ? -2 : -3;
      break;
    case Prop::Stool:
      f.ok = true; f.seatY = 8; f.ay = fc == 0 ? -2 : (fc == 1 ? -3 : -4);
      break;
    case Prop::Bench:
      f.ok = true; f.seatY = 8; f.ay = fc == 0 ? -2 : (fc == 1 ? -3 : -4);
      break;
    case Prop::Throne:
      if (fc != 0) return f;
      f.ok = true; f.seatY = 4; f.ay = -7;
      break;
    case Prop::Cushion:
      f.ok = true; f.seatY = 9; f.ay = (int8_t)(fc == 0 ? -6 : (fc == 1 ? 1 : -4)); f.sit = Posture::SitFloor;   // (M5 fixer r2: see seatAt)
      break;
    case Prop::SleepingMat: case Prop::Bedroll: case Prop::Rug: case Prop::LowTable: case Prop::COUNT:
      f.ok = seat != Prop::LowTable; f.seatY = 12; f.ay = -3; f.sit = Posture::SitFloor;
      break;
    default: break;
  }
  return f;
}

// ================================================================ berths and the sleeper
namespace {
// the culture bed kits (art_culture_furniture.cpp bedKit): the head piece's kind and how low the bed sits
bool kitBed(int arch) { return arch == 4 || arch == 5 || arch == 6 || arch == 7 || arch == 9 || arch == 10 || arch == 11; }
void kitGeom(int arch, int H, int& pillowTop, int& quiltEnd) {
  int headKind = 0, low = 0;
  switch (arch) {
    case 7: headKind = 1; low = 1; break;   // jade
    case 6: headKind = 0; low = 2; break;   // marsh
    case 9: headKind = 0; low = 2; break;   // sun temple
    case 10: headKind = 3; low = 1; break;  // sylvan
    case 5: headKind = 0; low = 1; break;   // steppe
    case 4: headKind = 4; low = 2; break;   // dune
    case 11: headKind = 2; low = 0; break;  // starspire
    default: break;
  }
  const int head = H >= 48 ? 6 : 2;
  const int hb1 = head + (headKind == 0 ? 4 : 10);
  pillowTop = hb1 + 1;
  const int frontH = low == 2 ? 3 : (low == 1 ? 4 : 5);
  quiltEnd = H - 1 - frontH;
}
int bedrollH() { return propH(Prop::Bedroll); }
}  // namespace

BedFit bedFit(Berth b, int kit) {
  BedFit f;
  auto vertical = [&](int w, int h, int headTop, int quiltEnd, int anchorBottom) {
    f.w = (int8_t)w; f.h = (int8_t)h;
    f.headX = (int8_t)(w / 2); f.headY = (int8_t)(headTop + 4);
    f.len = (int8_t)std::max(0, quiltEnd - (headTop + 9));
    // the generic cell's face top is its row 2; the cell's top-left is (p.x - 8, p.y - 22)
    const int cellTop = headTop - 2;                            // in the berth canvas
    f.ay = (int8_t)(cellTop + 22 - (h - anchorBottom));         // p.y relative to the anchor tile's bottom
    f.ax = 0;
  };
  switch (b) {
    case Berth::Bed:
      if (kitBed(kit)) { int pt, qe; kitGeom(kit, 32, pt, qe); vertical(20, 32, pt - 2, qe, 0); }
      else vertical(20, 32, 10, 25, 0);
      break;
    case Berth::LongBed:
      if (kitBed(kit)) { int pt, qe; kitGeom(kit, 48, pt, qe); vertical(20, 48, pt - 2, qe, 0); }
      else vertical(20, 48, 17, 41, 0);
      break;
    case Berth::BunkHigh: vertical(20, 40, 2, 19, 0); break;
    case Berth::BunkLow: vertical(20, 40, 18, 33, 0); break;
    case Berth::LongHammock: vertical(20, 48, 21, 36, 0); break;
    case Berth::LongMat: vertical(20, 48, 20, 44, 0); break;
    case Berth::Hammock: f.w = 28; f.h = 22; f.across = true; f.headX = 8; f.headY = 9; f.len = 12; break;
    case Berth::Mat: f.w = 24; f.h = 14; f.across = true; f.headX = 8; f.headY = 11; f.len = 9; break;
    case Berth::Bedroll: { const int H = bedrollH(); f.w = (int8_t)propW(Prop::Bedroll); f.h = (int8_t)H; f.across = true; f.headX = 4; f.headY = (int8_t)(H - 6); f.len = 9; break; }
    case Berth::Ground: f.w = 24; f.h = 14; f.across = true; f.headX = 5; f.headY = 8; f.len = 12; break;
    default: break;
  }
  return f;
}

Canvas berthSprite(Berth b, int kit, int variant) {
  switch (b) {
    case Berth::Bed: return kitBed(kit) ? cultureInteriorPiece(kit, Prop::Bed, 0) : propSprite(Prop::Bed);
    case Berth::LongBed:
      return kitBed(kit) ? cultureInteriorPiece(kit, Prop::Bed, 1) : interiorPiece(pieceKey(Piece::Styled, 0, 4, variant & 3));
    case Berth::BunkLow: case Berth::BunkHigh: return propSprite(Prop::BunkBed);
    case Berth::Hammock: return propSprite(Prop::Hammock);
    case Berth::LongHammock: return interiorPiece(pieceKey(Piece::Styled, 0, 6, variant & 3));
    case Berth::Mat: return propSprite(Prop::SleepingMat);
    case Berth::LongMat: return interiorPiece(pieceKey(Piece::Styled, 0, 7, variant & 3));
    case Berth::Bedroll: { const Canvas s = propSprite(Prop::Bedroll); Canvas c(propW(Prop::Bedroll), propH(Prop::Bedroll)); for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) c.set(x, y, s.get(x, y)); return c; }
    default: return Canvas(24, 14);
  }
}

Canvas sleeperSprite(const HumanLook& look, Berth b, int kit, int frame) {
  const BedFit f = bedFit(b, kit);
  Canvas out(std::max<int>(1, f.w), std::max<int>(1, f.h));
  if (!f.across) {
    const int headTop = f.headY - 4, ox = f.w / 2 - 8;
    const int y0 = headTop + 9, y1 = y0 + std::max<int>(3, f.len) - 1;
    bodyMound(out, f.w * 0.5f, y0, y1, b == Berth::LongHammock ? 4.0f : 5.2f, frame);
    if (b != Berth::BunkLow) blit(out, outlinedHead(look, f.w, f.h, ox, headTop, frame), 0, 0);
    return out;
  }
  // across: the sleeper painted head-up in a strip, then turned (transposed: the head west, the lit side still up)
  const int L = 8 + 1 + f.len + 2;
  Canvas strip(16, L + 2);
  if (b == Berth::Ground) {   // its own blanket: the cloak, or a drab wool
    const Ramp Bl = ramp(look.cloak ? look.cloakColor : rgba(112, 96, 78));
    for (int y = 10; y <= L; y++)
      for (int x = 3; x <= 12; x++) {
        const float t = (y - 10.0f) / (L - 10.0f);
        const float w = 5.0f * (1.0f - 0.25f * t);
        const float u = (x + 0.5f - 8.0f) / w;
        if (std::fabs(u) > 1.0f) continue;
        int k = u < -0.5f ? 3 : (u > 0.45f ? 1 : 2);
        if (y == 10) k = 4;
        if (y == L) k = std::max(0, k - 1);
        if (((x + y) % 5) == 0 && k > 1) k--;   // a coarse weave
        strip.set(x, y, Bl[k]);
      }
    outline(strip, 0.9f);
  }
  bodyMound(strip, 8.0f, 10, L - 1, 4.6f, frame);
  blit(strip, outlinedHead(look, strip.w, strip.h, 0, 2, frame), 0, 0);
  // transpose into the berth canvas, following a hammock's sag
  const int cx = f.headX - 6, cy = f.headY - 8;
  for (int sy = 0; sy < strip.h; sy++)
    for (int sx = 0; sx < strip.w; sx++) {
      const uint32_t p = strip.get(sx, sy);
      if (!chA(p)) continue;
      const int x = cx + sy;
      int y = cy + sx;
      if (b == Berth::Hammock) {
        const float t = std::clamp((x - 3) / (float)(f.w - 7), 0.0f, 1.0f);
        y += (int)std::lround(std::sin(t * 3.14159f) * 7.0f) - 4;
      }
      out.set(x, y, p);
    }
  return out;
}

// ================================================================ lamplight
Canvas lampFlame(uint32_t tint) {
  Canvas c(LAMP_FLAME_W * LAMP_FLAME_FRAMES, LAMP_FLAME_H);
  const Ramp F = tint ? ramp(tint) : kFire;
  for (int f = 0; f < LAMP_FLAME_FRAMES; f++) {
    const int ox = f * LAMP_FLAME_W;
    const float cx = ox + LAMP_FLAME_W * 0.5f, base = LAMP_FLAME_H - 1.0f;
    // the halo: a soft warm disc round the flame (alpha; the view's light pass does the rest)
    for (int y = 0; y < LAMP_FLAME_H; y++)
      for (int x = ox; x < ox + LAMP_FLAME_W; x++) {
        const float d = std::hypot((x + 0.5f - cx) / 4.4f, (y + 0.5f - (base - 3.0f)) / 5.2f);
        if (d < 1.0f) c.set(x, y, withA(F[3], (int)(70 * (1.0f - d) * (1.0f - d)) + 10));
      }
    // the flame: a teardrop that leans and stretches frame to frame
    static const float lean[4] = {0.0f, 0.5f, 0.1f, -0.4f}, tall[4] = {4.6f, 5.2f, 4.2f, 5.0f};
    for (int y = 0; y < LAMP_FLAME_H; y++) {
      const float t = (base - y) / tall[f];
      if (t < 0 || t > 1) continue;
      const float half = 1.5f * std::sqrt(std::max(0.0f, 1.0f - t)) * (t < 0.25f ? 0.7f + t * 1.2f : 1.0f);
      const float mx = cx + lean[f] * t;
      for (int x = (int)std::floor(mx - half); x <= (int)std::ceil(mx + half) - 1; x++) {
        const float u = std::fabs(x + 0.5f - mx) / std::max(0.5f, half);
        c.set(x, y, u < 0.45f && t < 0.6f ? F[4] : (u < 0.8f ? F[3] : F[2]));
      }
    }
    c.set((int)cx, (int)base, F[4]);
  }
  return c;
}
void lampHead(int& x, int& y) { x = 6; y = 7; }   // Prop::Lamppost: the glass of its lantern, rows 3..7 over x 4..7

// ================================================================ speech bubbles
// 11x10: a rounded white frame (lit from the top-left: a warm paper face, a cool shade along its lower-right rim), the
// tail at the bottom centre; the glyph a crisp 7x5 icon in its own colours
Canvas bubbleSprite(Bubble b) {
  Canvas c(11, 10);
  if (b == Bubble::None || (int)b >= (int)Bubble::COUNT) return c;
  const uint32_t rim = rgba(70, 58, 86), paper = kWhite, shade = rgba(214, 206, 214);
  for (int y = 0; y <= 7; y++)
    for (int x = 0; x <= 10; x++) {
      const bool corner = (x == 0 || x == 10) && (y == 0 || y == 7);
      if (corner) continue;
      const bool edge = x == 0 || x == 10 || y == 0 || y == 7 || ((x == 1 || x == 9) && (y == 0 || y == 7));
      c.set(x, y, edge ? rim : paper);
    }
  c.set(1, 1, rim); c.set(9, 1, rim); c.set(1, 6, rim); c.set(9, 6, rim);   // rounder corners
  for (int x = 2; x <= 8; x++) c.set(x, 6, shade);
  for (int y = 2; y <= 5; y++) c.set(9, y, shade);
  c.set(2, 1, rgba(255, 252, 240));
  // the tail
  c.set(4, 7, paper); c.set(5, 7, paper); c.set(6, 7, shade);
  c.set(3, 7, rim); c.set(7, 7, rim); c.set(4, 8, rim); c.set(5, 8, paper); c.set(6, 8, rim); c.set(5, 9, rim);
  // glyphs: rows of 7 chars over x 2..8, y 1..5. '#' ink, 'r' red, 'R' light red, 'y' gold, 'Y' light gold, 'b' blue,
  // 'B' light blue, 'w' wood, 'W' light wood, 'f' foam, 'g' grey, 'k' pale highlight
  static const char* const kG[(int)Bubble::COUNT][5] = {
      {".......", ".......", ".......", ".......", "......."},                       // None
      {".......", ".......", "#.#.#..", ".......", "......."},                       // Talk "..."
      {"...r...", "...r...", "...r...", ".......", "...r..."},                       // Exclaim
      {"..#####", "..#...#", ".##..##", "###.###", ".#...#."},                       // Note (two beamed quavers)
      {".ffff..", ".WWWww.", ".WWWw.w", ".WWWww.", ".wwww.."},                       // Mug
      {".rr.rr.", "rRrrrrr", "rrrrrrr", ".rrrrr.", "..rrr.."},                       // Heart
      {".WWWW..", "WWWWWW.", "WwWwWw.", "wwwwww.", "......."},                       // Bread
      {"####...", "..#....", ".#.###.", "####.#.", "....###"},                       // Zzz
      {"..yyy..", ".yYYyy.", ".yYyyy.", ".yyyyy.", "..yyy.."},                       // Coin
      {"..r.r..", ".rr.rr.", ".......", ".rr.rr.", "..r.r.."},                       // Anger (the vein mark)
      {"...B...", "..BbB..", "..bbb..", "..bbb..", "...b..."},                       // Tear
      {"..###..", ".#...#.", "....#..", "...#...", "...#..."},                       // Question
  };
  const uint32_t ink = rgba(44, 34, 60);
  for (int r = 0; r < 5; r++)
    for (int k = 0; k < 7; k++) {
      uint32_t col = 0;
      switch (kG[(int)b][r][k]) {
        case '#': col = ink; break;
        case 'r': col = rgba(204, 48, 58); break;
        case 'R': col = rgba(250, 150, 150); break;
        case 'y': col = kGold[2]; break;
        case 'Y': col = kGold[4]; break;
        case 'b': col = rgba(70, 130, 210); break;
        case 'B': col = rgba(170, 214, 250); break;
        case 'w': col = kWood[1]; break;
        case 'W': col = kSand[3]; break;
        case 'f': col = rgba(250, 250, 240); break;
        case 'g': col = rgba(140, 132, 150); break;
        default: break;
      }
      if (col) c.set(2 + k, 1 + r, col);
    }
  if (b == Bubble::Mug) { c.set(3, 2, kSand[4]); c.set(3, 3, kSand[4]); }
  if (b == Bubble::Bread) { c.set(3, 1, kSand[4]); c.set(2, 2, kSand[4]); }
  return c;
}

// ================================================================ festival dressing
// a cord sagging between two eaves, pennants hung along it: each a little triangle in the town's colours, lit on its
// left edge, shaded on its right, a darker inner fold; every third a lighter cloth; the cord darker where it dips
Canvas festivalBunting(int widthPx, uint32_t a, uint32_t b, uint32_t seed) {
  widthPx = std::max(8, std::min(widthPx, 512));
  const int sag = std::clamp(widthPx / 10, 2, 9);
  Canvas c(widthPx, sag + 9);
  const Ramp A = ramp(a ? a : rgba(196, 60, 52)), B = ramp(b ? b : rgba(236, 196, 96));
  const Ramp Cc = ramp(mix(a ? a : rgba(196, 60, 52), b ? b : rgba(236, 196, 96), 0.5f), 0.6f);
  auto cordY = [&](float x) { const float t = x / (float)(widthPx - 1); return 1.0f + sag * 4.0f * t * (1.0f - t); };
  const int step = 6;
  const int phase = (int)(seed % 3u);
  for (int x0 = 2; x0 + 4 < widthPx; x0 += step) {
    const int k = (x0 / step + phase) % 3;
    const Ramp& R = k == 0 ? A : (k == 1 ? B : Cc);
    const float y0 = cordY(x0 + 2.0f);
    const int yy = (int)std::lround(y0) + 1;
    const int sway = (int)(hash3(x0, 7, seed) % 3u) - 1;   // the wind: a pennant's tip off true
    for (int r = 0; r < 5; r++) {
      const int half = 2 - r / 2;
      const int mid = x0 + 2 + (r >= 3 ? sway : 0);
      for (int x = mid - half; x <= mid + half; x++) {
        int kk = x == mid - half ? 4 : (x == mid + half && half > 0 ? 1 : 3);
        if (r == 0) kk = std::max(kk, 3);
        if (r == 4) kk = 2;
        c.set(x, yy + r, R[kk]);
      }
    }
  }
  // the cord over the pennants' heads
  for (int x = 0; x < widthPx; x++) {
    const int y = (int)std::lround(cordY((float)x));
    c.set(x, y, kWoodDark[x % 4 == 0 ? 3 : 2]);
  }
  outline(c, 0.7f);
  return c;
}

// a paper lantern on a cord: ribbed, round, a lid and a base ring of lacquered wood, a tassel; glowing from within (the
// light pass does the halo). seed varies the shape: round, tall or a gourd
Canvas festivalLantern(uint32_t color, uint32_t seed) {
  Canvas c(9, 14);
  const Ramp P = ramp(color ? color : rgba(214, 70, 52));
  const int shape = (int)(seed % 3u);
  const float cx = 4.5f, cy = 6.5f;
  const float rx = shape == 1 ? 2.8f : 3.6f, ry = shape == 1 ? 3.8f : 3.2f;
  // the cord
  c.set(4, 0, kWoodDark[2]); c.set(4, 1, kWoodDark[2]);
  // the lid
  for (int x = 3; x <= 5; x++) c.set(x, 2, kWoodDark[x == 3 ? 4 : 2]);
  // the paper body, lit from inside: brightest in the middle, darker toward the ribs at the rim, the left a touch warmer
  for (int y = 3; y <= 10; y++)
    for (int x = 0; x < 9; x++) {
      float dy = (y + 0.5f - cy) / ry;
      if (shape == 2 && y < 5) dy = (y + 0.5f - 4.2f) / 1.6f;   // a gourd's small top bulb
      const float dx = (x + 0.5f - cx) / (shape == 2 && y < 5 ? rx * 0.6f : rx);
      const float d = dx * dx + dy * dy;
      if (d > 1.0f) continue;
      int k = d < 0.25f ? 4 : (d < 0.6f ? 3 : 2);
      if (dx > 0.5f) k = std::max(1, k - 1);
      if ((y - 3) % 2 == 1 && d > 0.3f) k = std::max(1, k - 1);   // the ribs
      c.set(x, y, P[k]);
    }
  // the base ring and the tassel
  for (int x = 3; x <= 5; x++) c.set(x, 11, kWoodDark[x == 3 ? 3 : 1]);
  c.set(4, 12, kGold[3]); c.set(4, 13, kGold[2]);
  outline(c, 0.7f);
  return c;
}

}  // namespace art
