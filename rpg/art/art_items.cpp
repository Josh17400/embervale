// EMBERVALE art: item icons. See rpg/art.h for the contract and rpg/art/art_internal.h for the shared helpers.
#include "rpg/art/art_internal.h"

namespace art {

// =====================================================================================================
// 7. item icons (16x16, outlined, top-left light). Weapons point up-right like a classic inventory.
// =====================================================================================================
namespace {

// diagonal blade from (x,y) going up-right for len pixels; width 1..3
void diagBlade(Canvas& c, int x, int y, int len, int width, const Ramp& m) {
  for (int i = 0; i < len; i++) {
    int px2 = x + i, py = y - i;
    bool tip = i == len - 1;
    c.set(px2, py, tip ? m[4] : m[3]);
    if (!tip) c.set(px2 + 1, py, m[1]);
    if (width >= 2 && !tip) c.set(px2, py - 1, m[4]);
    if (width >= 3 && i < len - 2) { c.set(px2 + 1, py + 1, m[0]); c.set(px2 + 2, py, m[1]); }
  }
}
void diagHandle(Canvas& c, int x, int y, int len, const Ramp& r) {
  for (int i = 0; i < len; i++) { c.set(x + i, y - i, r[(i & 1) ? 2 : 3]); c.set(x + i + 1, y - i, r[1]); }
}

void potion(Canvas& c, const Ramp& liquid) {
  // round flask: glass rim, coloured liquid, cork, glints
  ball(c, 8, 10.5f, 5.0f, 4.8f, liquid);
  for (int y = 5; y <= 7; y++) { c.set(7, y, kCrystal[4]); c.set(8, y, kCrystal[3]); }
  for (int y = 6; y <= 9; y++)
    for (int x = 3; x <= 13; x++)
      if (solid(c, x, y) && y < 8) c.set(x, y, withA(mix(kCrystal[3], liquid[2], 0.2f), 255));
  box(c, 6, 2, 9, 4, kWood[3]); hline(c, 6, 9, 2, kWood[4]); c.set(9, 3, kWood[1]); c.set(9, 4, kWood[1]);
  hline(c, 6, 9, 5, kCrystal[2]);
  c.set(5, 10, kWhite); c.set(5, 9, kWhite); c.set(6, 8, liquid[4]);
}

// ---------------------------------------------------------------- M6 Steel: culture arms icons (IconLook)
const Ramp kFurIcon = ramp5(rgba(84, 70, 70), rgba(134, 120, 112), rgba(180, 168, 152), rgba(214, 206, 188), rgba(242, 238, 224));
// Blades and polearm heads are rasterised in the blade's own frame: u along the blade (from the hilt toward the tip,
// up-right on the icon), v across it (+ toward the lower-right, the shaded edge). A profile gives each u its lo..hi.
struct BladeFrame {
  float x0, y0;   // the blade's root on the icon
  float u(int x, int y) const { return ((x + 0.5f - x0) - (y + 0.5f - y0)) * 0.70710678f; }
  float v(int x, int y) const { return ((x + 0.5f - x0) + (y + 0.5f - y0)) * 0.70710678f; }
};
// paint every pixel whose (u, v) lies in the profile: lit on the upper-left edge, a fuller line, dark on the lower-right
template <class Prof>
void rasterBlade(Canvas& c, const BladeFrame& F, float len, const Ramp& m, Prof prof) {
  for (int y = 0; y < ICON; y++)
    for (int x = 0; x < ICON; x++) {
      const float u = F.u(x, y), v = F.v(x, y);
      if (u < 0 || u > len) continue;
      float lo, hi;
      prof(u, lo, hi);
      if (v < lo || v > hi) continue;
      const float t = (hi - lo) > 0.01f ? (v - lo) / (hi - lo) : 0.5f;
      int k = t < 0.34f ? 4 : (t < 0.6f ? 3 : (t < 0.82f ? 2 : 1));
      if (u > len - 1.2f) k = std::min(4, k + 1);   // the point catches the light
      c.set(x, y, m[k]);
    }
}
void hiltIcon(Canvas& c, int gx, int gy, int guard, const Ramp& grip, const Ramp& gold) {
  // a guard across the blade's root (gx, gy) and the grip running down-left to a pommel
  for (int i = -guard; i <= guard; i++) c.set(gx + i, gy + i, gold[i < 0 ? 4 : (i == 0 ? 3 : 1)]);
  for (int i = 1; i <= 2; i++) { c.set(gx - i, gy + i, grip[(i & 1) ? 3 : 2]); c.set(gx - i + 1, gy + i, grip[1]); }
  c.set(gx - 3, gy + 3, gold[3]); c.set(gx - 4, gy + 4, gold[1]);
}
// cult::Blade + 1: 1 straight, 2 leaf, 3 falchion, 4 scimitar, 5 khopesh, 6 wavy, 7 broad, 8 curved, 9 glaive
void bladeIcon(Canvas& c, Icon ic, int form, const Ramp& m, const Ramp& grip, const Ramp& gold) {
  const bool great = ic == Icon::Greatsword, dag = ic == Icon::Dagger;
  const float len = great ? 12.5f : (dag ? 6.5f : 10.0f);
  const float w = great ? 1.5f : 1.1f;
  const BladeFrame F{great ? 5.0f : (dag ? 7.0f : 6.0f), great ? 11.0f : (dag ? 9.0f : 10.0f)};
  auto prof = [&](float u, float& lo, float& hi) {
    const float t = u / len;
    float a = -w, b = w, off = 0;
    switch (form) {
      case 2: a = -w * (0.8f + 0.9f * std::sin(t * PI * 0.95f)); b = -a; break;                      // leaf
      case 3: a = -w * 0.8f; b = w * (0.9f + 1.4f * t); if (t > 0.85f) a += (t - 0.85f) * 8; break;    // falchion
      case 4: off = 3.2f * t * t; a = -w * 0.8f; b = w * 1.3f; break;                                 // scimitar
      case 5: if (t < 0.45f) { a = -0.7f; b = 0.7f; } else { off = (t - 0.45f) * 7.0f; a = -1.2f; b = 1.0f; } break;   // khopesh
      case 6: off = 0.9f * std::sin(t * PI * 4.0f); break;                                             // wavy
      case 7: a = -w * 1.7f; b = w * 1.7f; break;                                                      // broad
      case 8: off = 1.4f * t * t; a = -w * 0.7f; b = w * 0.9f; break;                                  // curved
      default: break;
    }
    if (t > 0.86f && form != 3 && form != 5) { const float pt = 1 - (t - 0.86f) / 0.14f; a *= pt; b *= pt; }   // the point
    lo = a + off; hi = b + off;
  };
  if (form == 9) {   // a glaive: a long hafted single edge
    diagHandle(c, 1, 14, 7, kWood);
    rasterBlade(c, BladeFrame{8.0f, 7.0f}, 7.5f, m, [&](float u, float& lo, float& hi) { lo = -0.7f; hi = 1.4f + 0.4f * std::sin(u * 0.45f); if (u > 6) { lo *= (7.5f - u) / 1.5f; hi *= (7.5f - u) / 1.5f; } });
    c.set(7, 8, gold[3]); c.set(8, 8, gold[1]);
    return;
  }
  rasterBlade(c, F, len, m, prof);
  if (form == 8) {   // a long wrapped grip, a small round guard
    const int gx = (int)F.x0, gy = (int)F.y0;
    c.set(gx - 1, gy, gold[4]); c.set(gx, gy + 1, gold[2]); c.set(gx - 1, gy + 1, gold[3]);
    for (int i = 1; i <= 4; i++) { c.set(gx - 1 - i, gy + 1 + i - 1, grip[(i & 1) ? 3 : 1]); c.set(gx - i, gy + 1 + i - 1, grip[(i & 1) ? 1 : 2]); }
    return;
  }
  hiltIcon(c, (int)F.x0 - 1, (int)F.y0, great ? 3 : (dag ? 1 : 2), grip, gold);
}
// cult::Polearm + 1: 1 spear, 2 glaive, 3 halberd (0: the plain spear)
void spearIcon(Canvas& c, int form, const Ramp& m, uint32_t accent, uint16_t orn) {
  diagHandle(c, 1, 14, 9, kWood);
  const Ramp& ring = (orn & 4) ? kGold : kIron;
  c.set(9, 6, ring[3]); c.set(10, 6, ring[1]); c.set(10, 5, ring[2]);
  const BladeFrame F{10.5f, 5.5f};
  switch (form) {
    case 2:   // glaive: one long edge swelling to the lower right
      rasterBlade(c, F, 6.0f, m, [&](float u, float& lo, float& hi) { lo = -0.6f; hi = 0.6f + 1.6f * std::sin(std::min(1.0f, u / 6.0f) * PI * 0.85f); });
      break;
    case 3:   // halberd: an axe blade below, a spike up, a hook behind
      rasterBlade(c, F, 5.0f, m, [&](float u, float& lo, float& hi) { lo = -0.6f; hi = 0.6f; if (u > 4) { lo *= (5 - u); hi *= (5 - u); } });
      for (int y = 0; y < ICON; y++)
        for (int x = 0; x < ICON; x++) {
          const float u = F.u(x, y) + 1.5f, v = F.v(x, y);
          if (u >= 0 && u <= 3.2f && v > 0.5f && v < 4.2f - std::fabs(u - 1.6f) * 0.5f) c.set(x, y, m[v < 1.6f ? 2 : (v > 3.2f ? 4 : 3)]);
          if (u >= 0.5f && u <= 1.6f && v < -0.5f && v > -2.2f + u * 0.4f) c.set(x, y, m[1]);
        }
      break;
    default:   // spear: a leaf-bladed head with a midrib
      rasterBlade(c, F, 5.5f, m, [&](float u, float& lo, float& hi) { const float t = u / 5.5f; const float w = 1.6f * std::sin(std::min(1.0f, t * 1.25f) * PI * 0.8f) * (1 - t * 0.55f) + 0.25f; lo = -w; hi = w; });
      break;
  }
  if (accent) { c.set(8, 8, accent); c.set(8, 9, shade(accent, 0.7f)); c.set(7, 9, shade(accent, 0.85f)); }   // a tassel
}
// cult::BowKind + 1: 1 self, 2 recurve, 3 composite, 4 crossbow, 5 longbow
void bowIcon(Canvas& c, int form, uint32_t accent, uint16_t orn) {
  static const Ramp kHorn = ramp5(rgba(60, 36, 30), rgba(104, 70, 48), rgba(150, 112, 72), rgba(196, 166, 120), rgba(232, 214, 176));
  const Ramp& gripR = (orn & 4) ? kGold : kLeather;
  if (form == 4) {   // crossbow: the stock along the diagonal, the steel prod across its nose, the string spanned back
    for (int i = 0; i <= 8; i++) { c.set(3 + i, 12 - i, kWood[(i & 1) ? 3 : 2]); c.set(4 + i, 12 - i, kWood[1]); }
    c.set(2, 13, kWood[2]); c.set(3, 13, kWood[1]); c.set(2, 14, kWood[0]);
    line(c, 7, 3, 9, 7, kCloth[3]); line(c, 9, 7, 13, 9, kCloth[3]);   // the string, behind the prod
    const int px0[8] = {6, 7, 8, 9, 10, 11, 12, 13}, py0[8] = {1, 1, 2, 3, 4, 5, 7, 8};
    for (int i = 0; i < 8; i++) { c.set(px0[i], py0[i], kIron[i < 3 ? 4 : (i < 6 ? 3 : 1)]); c.set(px0[i], py0[i] + 1, kIron[i < 4 ? 2 : 0]); }
    c.set(11, 4, kIron[4]); c.set(12, 4, kWood[3]);   // the nose and its stirrup
    c.set(7, 9, kIron[2]); c.set(6, 10, kIron[3]);    // the trigger
    return;
  }
  const bool longb = form == 5, comp = form == 3, rec = form == 2;
  const Ramp& W = comp ? kHorn : (rec ? kWoodDark : kWood);
  const float a0 = longb ? 0.5f : (comp ? 3.5f : 2.0f), a1 = longb ? 14.5f : (comp ? 12.5f : 13.0f);
  const float bend = longb ? 2.0f : (comp ? 3.4f : 3.0f);
  for (int i = 0; i <= 16; i++) {
    const float t = i / 16.0f;
    float x = a0 + t * (a1 - a0) - std::sin(t * PI) * bend, y = (15 - a0) - t * (a1 - a0) - std::sin(t * PI) * bend;
    if ((rec || comp) && (t < 0.1f || t > 0.9f)) { x += 1.0f; y += 1.0f; }   // the recurved tips flick back
    const int xi = (int)std::lround(x), yi = (int)std::lround(y);
    uint32_t col = W[t < 0.5f ? 3 : 2];
    if (t > 0.42f && t < 0.58f) col = gripR[t < 0.5f ? 3 : 2];
    if (comp && (std::fabs(t - 0.3f) < 0.04f || std::fabs(t - 0.7f) < 0.04f)) col = accent ? accent : kRed[2];
    c.set(xi, yi, col);
    c.set(xi + 1, yi, W[1]);
  }
  const int s0 = (int)std::lround(a0 + ((rec || comp) ? 1.0f : 0.0f)), s1 = (int)std::lround(a1 + ((rec || comp) ? 1.0f : 0.0f));
  line(c, s0, 15 - (int)std::lround(a0) + ((rec || comp) ? 1 : 0), s1, 15 - (int)std::lround(a1) + ((rec || comp) ? 1 : 0), kCloth[4]);
}
void ingotIcon(Canvas& c, const Ramp& I) {
  // a cast bar in the 3/4 view: the lit top face, the front face, the shaded end, a stamp
  for (int y = 5; y <= 8; y++) {
    const int x0 = 4 + (8 - y), x1 = 13 + (8 - y) - 1;
    for (int x = x0; x <= x1; x++) c.set(x, y, I[y == 5 ? 4 : 3]);
  }
  for (int y = 9; y <= 12; y++)
    for (int x = 2; x <= 11; x++) c.set(x, y, I[y == 12 ? 1 : (x < 4 ? 3 : 2)]);
  for (int y = 9; y <= 12; y++)
    for (int x = 12; x <= 15 - (y - 8); x++) c.set(x, y, I[y == 9 ? 2 : 1]);
  for (int y = 5; y <= 8; y++) c.set(13 + (8 - y), y, I[2]);
  c.set(7, 10, I[1]); c.set(8, 10, I[1]); c.set(7, 11, I[1]);   // the smith's stamp
  c.set(6, 6, kWhite);
}
// cult::HelmForm + 1: 1 nasal, 2 kettle, 3 great helm, 4 spangen, 5 horned, 6 plumed, 7 aventail, 8 masked, 9 crested,
// 10 winged. A 3/4 view from the front-left; acc = the plume / crest colour, trim = ribs and bands
void helmIcon(Canvas& c, int form, const Ramp& M, const Ramp& acc, const Ramp& trim) {
  auto dome = [&](float cx, float cy, float rx, float ry, int yCut) {
    for (int y = 0; y < ICON; y++)
      for (int x = 0; x < ICON; x++) {
        const float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
        if (y > yCut || dx * dx + dy * dy > 1) continue;
        c.set(x, y, M[lightIndex(lightAt(dx * 0.85f, dy * 0.85f), x, y, 0.08f)]);
      }
  };
  auto band = [&](int y, int x0, int x1, const Ramp& r) { for (int x = x0; x <= x1; x++) c.set(x, y, r[x < 7 ? 4 : (x > 10 ? 1 : 3)]); };
  auto face = [&](int y0, int y1, int x0, int x1) {   // the shadowed opening for the face
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) c.set(x, y, y == y0 ? kInk : rgba(64, 44, 52));
  };
  switch (form) {
    case 1:   // nasal: a conical dome, a brow band, the nose guard
      dome(8, 8.5f, 5.5f, 6.5f, 9);
      c.set(8, 1, M[4]); c.set(7, 2, M[4]); c.set(8, 2, M[3]);
      face(10, 13, 4, 11);
      for (int x = 3; x <= 12; x++) c.set(x, 10, M[x < 6 ? 4 : (x > 10 ? 1 : 2)]);
      band(9, 3, 12, trim);
      for (int y = 10; y <= 13; y++) { c.set(7, y, M[3]); c.set(8, y, M[1]); }
      break;
    case 2:   // kettle: a round crown and a broad brim
      dome(8, 8, 5, 5, 9);
      for (int x = 1; x <= 14; x++) { c.set(x, 10, M[x < 5 ? 4 : (x > 11 ? 2 : 3)]); c.set(x, 11, M[x < 4 ? 2 : 1]); }
      c.set(1, 11, M[1]); c.set(14, 11, M[0]);
      face(12, 13, 5, 10);
      band(9, 3, 12, trim);
      break;
    case 3:   // great helm: a flat-topped barrel, the eye slit and breaths
      for (int y = 2; y <= 14; y++)
        for (int x = 3; x <= 12; x++) c.set(x, y, M[y == 2 ? 4 : (x < 5 ? 4 : (x < 8 ? 3 : (x < 11 ? 2 : 1)))]);
      c.set(3, 2, 0); c.set(12, 2, 0);
      for (int x = 4; x <= 11; x++) if (x != 8) c.set(x, 6, kInk);
      for (int y = 5; y <= 12; y++) { c.set(8, y, M[4]); }
      for (int i = 0; i < 3; i++) { c.set(10, 9 + i * 2, kInk); c.set(11, 9 + i * 2, kInk); }
      for (int x = 3; x <= 12; x++) c.set(x, 14, M[1]);
      break;
    case 4:   // spangen: a segmented dome with gilt ribs and spectacle guards
      dome(8, 8.5f, 5.5f, 6.0f, 10);
      for (int y = 3; y <= 10; y++) { c.set(8, y, trim[3]); }
      for (int y = 5; y <= 10; y++) { c.set(4 + (y > 7), y, trim[4]); c.set(12 - (y > 7), y, trim[2]); }
      band(10, 2, 13, trim);
      face(11, 13, 4, 11);
      for (int x : {5, 6, 9, 10}) c.set(x, 11, M[3]);
      c.set(7, 12, M[2]); c.set(8, 12, M[2]); c.set(7, 13, M[3]); c.set(8, 13, M[1]);
      break;
    case 5:   // horned: a round dome and two bone horns sweeping up and out
      dome(8, 9, 5, 5, 11);
      band(11, 3, 12, trim);
      face(12, 13, 5, 10);
      for (int i = 0; i < 5; i++) {
        c.set(3 - i / 2, 7 - i, kBone[4 - i / 2]); c.set(4 - i / 2, 7 - i, kBone[2]);
        c.set(12 + i / 2, 7 - i, kBone[3 - i / 2]); c.set(13 + i / 2, 7 - i, kBone[1]);
      }
      break;
    case 6:   // plumed: a round dome, a tall plume sweeping back
      dome(8, 9.5f, 5, 5, 12);
      band(11, 3, 12, trim);
      face(12, 13, 5, 10);
      for (int y = 1; y <= 6; y++)
        for (int x = 6 + (6 - y) / 2; x <= 9 + (6 - y); x++) c.set(x, y, acc[(x + y) & 1 ? 3 : (x < 9 ? 4 : 2)]);
      c.set(13, 1, acc[1]); c.set(14, 2, acc[1]);
      break;
    case 7:   // conical with an aventail: a spiked cone, a mail curtain to the shoulders
      dome(8, 8, 5, 5.5f, 8);
      for (int y = 0; y <= 3; y++) { c.set(8, y, M[4 - (y == 0)]); if (y >= 2) c.set(7, y, M[4]); if (y >= 2) c.set(9, y, M[2]); }
      band(8, 3, 12, trim);
      for (int y = 9; y <= 14; y++)
        for (int x = 2 + (y > 12); x <= 13 - (y > 12); x++) {
          if (y <= 11 && x >= 5 && x <= 10) { c.set(x, y, rgba(64, 44, 52)); continue; }
          c.set(x, y, M[((x + y) & 1) ? 1 : (x < 7 ? 3 : 2)]);
        }
      break;
    case 8:   // masked: a dome and a full face mask with eye holes and a grim mouth
      dome(8, 8, 5.5f, 5.5f, 8);
      for (int y = 8; y <= 14; y++)
        for (int x = 3 + (y > 12); x <= 12 - (y > 12); x++) c.set(x, y, M[x < 6 ? 4 : (x < 9 ? 3 : (x < 11 ? 2 : 1))]);
      c.set(5, 10, kInk); c.set(6, 10, kInk); c.set(9, 10, kInk); c.set(10, 10, kInk);
      for (int x = 6; x <= 9; x++) c.set(x, 13, kInk);
      c.set(7, 11, M[4]); c.set(7, 12, M[3]);
      band(8, 3, 12, trim);
      c.set(4, 9, acc[3]); c.set(11, 9, acc[2]);
      break;
    case 9:   // crested: a crest running front to back over the crown, cheek guards
      dome(8, 9, 5, 5.5f, 11);
      for (int x = 4; x <= 12; x++) for (int y = 1; y <= 3 + (x < 6 || x > 10); y++) c.set(x, y + (x - 8) / 3, acc[((x + y) & 1) ? 3 : (x < 8 ? 4 : 2)]);
      face(10, 13, 6, 10);
      for (int y = 10; y <= 13; y++) { c.set(4, y, M[4]); c.set(5, y, M[3]); c.set(11, y, M[1]); }
      band(9, 3, 12, trim);
      break;
    default:   // winged: a round dome, white wings swept up on each side
      dome(8, 9, 5, 5, 11);
      band(11, 3, 12, trim);
      face(12, 13, 5, 10);
      for (int i = 0; i < 5; i++) {
        for (int k = 0; k <= i / 2; k++) { c.set(2 - k + 0, 8 - i, kCloth[4 - k]); c.set(13 + k, 8 - i, kCloth[3 - k]); }
      }
      c.set(1, 4, kCloth[4]); c.set(14, 4, kCloth[2]);
      break;
  }
}
// cult::BodyArm + 1: 1 padded, 2 leather, 3 mail, 4 scale, 5 lamellar, 6 brigandine, 7 plate, 8 leaf
void bodyIcon(Canvas& c, int form, const Ramp& M, const Ramp& acc) {
  const Ramp& base = form == 1 || form == 6 ? acc : (form == 2 ? kLeather : M);
  for (int y = 2; y <= 14; y++)
    for (int x = 1; x <= 14; x++) {
      const bool shoulders = y <= 5 && (x <= 4 || x >= 11) && y >= 2 && !(y == 2 && (x == 1 || x == 14));
      const bool torso = x >= 4 && x <= 11 && y >= 3;
      const bool neck = y < 5 && x >= 6 && x <= 9;
      if (!(shoulders || torso) || neck) continue;
      int k = x < 6 ? 3 : (x > 10 ? 1 : 2);
      if (shoulders && y == 2) k = 4;
      if (y == 14) k = std::max(0, k - 1);
      const int ry = y - 3;
      switch (form) {
        case 1: if ((x & 1) == 0 && y > 4) k--; else if (ry % 3 == 0) k--; break;                         // quilting
        case 2: break;
        case 3: { const int q = (x + 2 * y) & 3; if (q == 0) k--; else if (q == 2) k++; break; }            // mail rings
        case 4: { const int ph = (ry >> 1) & 1; k += (ry & 1) == 0 ? (((x + ph) & 1) ? 1 : 0) : (((x + ph) & 1) ? -1 : 0); break; }   // scales
        case 5: if (ry > 0 && ry % 3 == 0) { c.set(x, y, acc[(x & 1) ? 3 : 1]); continue; } if (x & 1) k--; break;   // laced plates
        case 6: if (ry > 0 && (ry % 3) == 1 && ((x + ry) & 1)) { c.set(x, y, M[4]); continue; } break;     // rivets
        case 7: if (y >= 12 && y <= 13 && ((y - 12) == 0)) k--; break;                                     // faulds
        case 8: { const int ph = (ry / 2) & 1, m = (x + ph * 2) % 4; k += (ry & 1) == 0 ? (m == 1 ? 1 : (m == 3 ? -1 : 0)) : (m == 2 ? -1 : 0); break; }
        default: break;
      }
      c.set(x, y, base[std::clamp(k, 0, 4)]);
    }
  if (form == 7) {   // plate: the breastplate's ridge and its curved light
    for (int y = 5; y <= 11; y++) { c.set(7, y, M[4]); c.set(8, y, M[2]); }
    c.set(5, 6, M[4]); c.set(5, 7, M[4]); c.set(6, 5, M[4]);
    for (int x = 4; x <= 11; x++) c.set(x, 12, M[1]);
  } else if (form == 2) {   // leather: a stitched seam down the front, a buckle
    for (int y = 5; y <= 13; y += 2) c.set(7, y, kLeather[0]);
    c.set(6, 4, kBrass[4]); c.set(9, 4, kBrass[3]);
  } else if (form == 6 || form == 1) {   // a belt over the cloth
    for (int x = 4; x <= 11; x++) c.set(x, 11, kLeather[x < 7 ? 2 : 1]);
    c.set(7, 11, kBrass[4]);
  } else if (form == 8) {
    c.set(7, 6, rgba(110, 200, 120)); c.set(8, 6, rgba(70, 150, 90));
  } else {
    for (int x = 4; x <= 11; x++) c.set(x, 11, kLeather[x < 7 ? 2 : 1]);
    c.set(7, 11, kGold[3]); c.set(8, 11, kGold[2]);
  }
}
// cult::ShieldForm + 1: 1 round, 2 kite, 3 heater, 4 tower, 5 crescent, 6 oval, 7 buckler, 8 leaf
void shieldIcon(Canvas& c, int form, const Ramp& M, const Ramp& F, bool gilt) {
  auto inside = [&](float x, float y, float& edge) -> bool {
    const float dx = x - 8, dy = y - 8;
    float d = 2;
    switch (form) {
      case 1: d = std::sqrt(dx * dx + dy * dy) / 6.6f; break;
      case 2: d = y < 7 ? std::sqrt(dx * dx + (y - 7) * (y - 7) * 1.6f) / 5.6f : std::fabs(dx) / (5.6f * (1 - (y - 7) / 8.5f)); if (y > 15) d = 2; break;
      case 3: d = y < 8 ? std::fabs(dx) / 6.0f : std::fabs(dx) / (6.0f * std::sqrt(std::max(0.0f, 1 - (y - 8) / 7.0f))); if (y < 1.0f) d = 2; break;
      case 4: d = std::max(std::fabs(dx) / 5.3f, std::fabs(dy) / 7.0f); break;
      case 5: { d = std::sqrt(dx * dx + dy * dy) / 6.6f; const float cy = -1.0f; if (std::sqrt(dx * dx + (y - cy) * (y - cy)) < 6.2f) d = 2; break; }
      case 6: d = std::sqrt(dx * dx / (4.8f * 4.8f) + dy * dy / (7.0f * 7.0f)); break;
      case 7: d = std::sqrt(dx * dx + dy * dy) / 4.8f; break;
      default: d = std::sqrt(dx * dx / (5.0f * 5.0f * (1 - dy * dy / 64.0f) + 0.01f) + dy * dy / 56.0f); break;
    }
    edge = d;
    return d <= 1.0f;
  };
  for (int y = 0; y < ICON; y++)
    for (int x = 0; x < ICON; x++) {
      float d;
      if (!inside(x + 0.5f, y + 0.5f, d)) continue;
      const bool left = x < 8, up = y < 8;
      const bool rim = d > (form == 7 ? 0.62f : 0.8f);
      int k = left ? (up ? 4 : 3) : (up ? 2 : 1);
      if (rim) { c.set(x, y, M[std::clamp(k, 0, 4)]); continue; }
      int kf = left ? 3 : 2;
      if (x > 10 || y > 11) kf--;
      c.set(x, y, F[kf]);
    }
  const Ramp& D = gilt ? kGold : M;
  if (form == 1 || form == 7 || form == 6) {   // the boss
    c.set(7, 7, D[4]); c.set(8, 7, D[3]); c.set(7, 8, D[3]); c.set(8, 8, D[1]);
  } else if (form == 8) {   // the leaf's midrib
    for (int y = 3; y <= 13; y++) c.set(8, y, D[y < 8 ? 4 : 2]);
  } else {   // a device: a cross / chevron in gilt
    for (int y = 4; y <= 10; y++) c.set(8, y, D[3]);
    for (int x = 6; x <= 10; x++) c.set(x, 6, D[x < 8 ? 4 : 2]);
  }
}

// the sheen's pattern on an icon's metal pixels (the ramp carries the rest)
void iconSheen(Canvas& c, const Ramp& A, int sheen) {
  if (sheen != 2 && sheen != 4 && sheen != 6 && sheen != 8) return;
  const Canvas src = c;
  auto idx = [&](uint32_t v) { for (int k = 0; k < 5; k++) if (A[k] == v) return k; return -1; };
  for (int y = 0; y < ICON; y++)
    for (int x = 0; x < ICON; x++) {
      const int k = idx(src.get(x, y));
      if (k < 0) continue;
      int nk = k;
      if (sheen == 4) { const int b = (x + 2 * y + ((x >> 2) & 1)) % 5; nk = b == 0 ? k - 1 : (b == 2 ? k + 1 : k); }
      else if (sheen == 6) { if (k == 0 && (y & 1) == 0) { c.set(x, y, lighten(A[3], 0.6f)); continue; } }
      else if (sheen == 8) { if (k == 3 && ((x + y) & 3) == 0) nk = 4; }
      else if (sheen == 2) { if (k == 4 && (idx(src.get(x - 1, y)) < 0 || idx(src.get(x, y - 1)) < 0)) { c.set(x, y, mix(A[4], kWhite, 0.6f)); continue; } }
      if (nk != k) c.set(x, y, A[std::clamp(nk, 0, 4)]);
    }
}
// ornament: gilt rims, studs, fur trim, etching, filigree (cult::ARM_* bits) over the piece's metal
void iconOrnament(Canvas& c, const Ramp& A, uint16_t o, Icon ic) {
  if (!o) return;
  const Canvas src = c;
  auto isA = [&](int x, int y) { const uint32_t v = src.get(x, y); for (int k = 0; k < 5; k++) if (A[k] == v) return true; return false; };
  for (int y = 0; y < ICON; y++)
    for (int x = 0; x < ICON; x++) {
      if (!isA(x, y)) continue;
      const bool edge = !(src.get(x - 1, y) >> 24) || !(src.get(x, y - 1) >> 24) || !(src.get(x + 1, y) >> 24) || !(src.get(x, y + 1) >> 24);
      if ((o & 4) && edge) c.set(x, y, kGold[(!(src.get(x - 1, y) >> 24) || !(src.get(x, y - 1) >> 24)) ? 4 : 2]);
      else if ((o & 16) && !edge && (x % 3) == 1 && (y % 3) == 1) c.set(x, y, kBrass[4]);
      else if ((o & 256) && !edge && (x % 4) == 2 && (y % 4) == 0) c.set(x, y, kWhite);
      else if ((o & 32) && !edge && ((x * 2 + y) % 7) == 0) c.set(x, y, A[4]);
      else if ((o & 512) && !edge && ((x + y * 3) % 9) == 0) c.set(x, y, kGold[4]);
    }
  if ((o & 2) && (ic == Icon::Armor || ic == Icon::Helmet || ic == Icon::Cloak)) {   // fur trim along the bottom edge / collar
    const int fy = ic == Icon::Armor ? 3 : 12;
    for (int x = 2; x <= 13; x++)
      if (src.get(x, fy) >> 24) c.set(x, fy, kFurIcon[1 + (int)(hash3(x, fy, 5) % 3u)]);
  }
}
// epic and legendary pieces glint: a four-point star on the brightest pixel nearest the top-left
void iconGlint(Canvas& c, int rarity) {
  if (rarity < 3) return;
  int bx = -1, by = -1;
  float best = -1;
  for (int y = 1; y < ICON - 1; y++)
    for (int x = 1; x < ICON - 1; x++) {
      const uint32_t v = c.get(x, y);
      if (!(v >> 24)) continue;
      const float s = luma(v) - (x + y) * 0.02f;
      if (s > best) { best = s; bx = x; by = y; }
    }
  if (bx < 0) return;
  const uint32_t g = rarity >= 4 ? rgba(255, 226, 140) : rgba(220, 236, 255);
  c.set(bx, by, kWhite);
  for (int d = 0; d < 4; d++) {
    const int nx = bx + (d == 0) - (d == 1), ny = by + (d == 2) - (d == 3);
    if (c.get(nx, ny) >> 24) c.set(nx, ny, mix(c.get(nx, ny), g, 0.7f));
  }
}

void paintIcon(Canvas& c, Icon ic, uint32_t tint, const Ramp* over = nullptr) {
  const bool tinted = tint != 0 || over;
  const Ramp M = over ? *over : (tinted ? ramp(tint, 1.1f) : kIron);     // metal / main material
  switch (ic) {
    case Icon::Sword:
      diagBlade(c, 6, 9, 8, 2, M);
      line(c, 3, 8, 7, 12, kBrass[3]); c.set(4, 9, kBrass[4]); c.set(6, 11, kBrass[1]);   // guard
      diagHandle(c, 3, 12, 2, kLeather);
      c.set(2, 13, kBrass[3]); c.set(1, 14, kBrass[2]);
      break;
    case Icon::Greatsword:
      diagBlade(c, 5, 10, 10, 3, M);
      line(c, 1, 8, 7, 14, kBrass[3]); line(c, 2, 8, 7, 13, kBrass[4]); c.set(7, 14, kBrass[1]);
      diagHandle(c, 2, 13, 2, kLeather);
      c.set(1, 14, kBrass[3]); c.set(0, 15, kBrass[2]);
      break;
    case Icon::Dagger:
      diagBlade(c, 7, 8, 5, 2, M);
      line(c, 5, 8, 7, 10, kBrass[3]);
      diagHandle(c, 4, 11, 3, kLeather);
      c.set(3, 12, kBrass[3]);
      break;
    case Icon::Axe:
      diagHandle(c, 2, 14, 10, kWood);
      // crescent head on the upper end
      for (int y = 1; y <= 8; y++)
        for (int x = 6; x <= 14; x++) {
          float dx = x - 13.5f, dy = y - 1.0f;
          float d = std::sqrt(dx * dx + dy * dy);
          float d2 = std::sqrt((x - 15.5f) * (x - 15.5f) + (y + 1.5f) * (y + 1.5f));
          if (d < 7.2f && d2 > 4.2f && x + y > 12) c.set(x, y, M[d > 6.0f ? 4 : (d > 4.6f ? 3 : 2)]);
        }
      c.set(12, 4, M[1]); c.set(11, 4, M[1]);
      break;
    case Icon::Mace:
      diagHandle(c, 2, 14, 8, kWood);
      c.set(2, 14, kLeather[2]); c.set(3, 13, kLeather[3]);
      ball(c, 11.5f, 4.5f, 3.6f, 3.6f, M);
      for (int a = 0; a < 6; a++) {
        float ang = a * TAU / 6 + 0.3f;
        c.set((int)std::lround(11.5f + std::cos(ang) * 4.6f - 0.5f), (int)std::lround(4.5f + std::sin(ang) * 4.6f - 0.5f), M[4]);
      }
      break;
    case Icon::Bow: {
      for (int i = 0; i <= 12; i++) {
        float t = i / 12.0f;
        int x = (int)std::lround(2 + t * 11 - std::sin(t * PI) * 3.0f), y = (int)std::lround(13 - t * 11 - std::sin(t * PI) * 3.0f);
        c.set(x, y, kWood[t < 0.5f ? 3 : 2]); c.set(x + 1, y, kWood[1]);
      }
      line(c, 2, 13, 13, 2, kCloth[4]);
      c.set(7, 8, kLeather[3]); c.set(8, 7, kLeather[2]); c.set(8, 8, kLeather[1]);
      if (tinted) { c.set(2, 12, M[3]); c.set(12, 2, M[3]); }
      break;
    }
    case Icon::Staff:
      diagHandle(c, 1, 14, 10, kWood);
      ball(c, 12.5f, 3.5f, 2.8f, 2.8f, tinted ? M : kCrystal);
      c.set(12, 2, kWhite);
      c.set(10, 6, kGold[3]); c.set(11, 6, kGold[2]); c.set(10, 5, kGold[4]);
      break;
    case Icon::Arrows:
      for (int k = 0; k < 3; k++) {
        int ox = k * 3 - 3, oy = k * 1 - 1;
        line(c, 3 + ox + 2, 13 + oy, 12 + ox + 2, 4 + oy, kWood[3]);
        c.set(13 + ox + 2, 3 + oy, M[4]); c.set(12 + ox + 2, 3 + oy, M[2]); c.set(13 + ox + 2, 4 + oy, M[2]);
        c.set(3 + ox + 2, 12 + oy, kRed[3]); c.set(4 + ox + 2, 13 + oy, kRed[2]); c.set(3 + ox + 2, 14 + oy, kCloth[4]);
      }
      break;
    case Icon::Shield: {
      // heater shield: metal rim, painted field with a chevron
      for (int y = 1; y <= 14; y++)
        for (int x = 2; x <= 13; x++) {
          float dx = (x + 0.5f - 8) / 6.0f;
          float bottom = y > 8 ? (y - 8) / 6.5f : 0;
          if (std::fabs(dx) > 1.0f - bottom * bottom) continue;
          bool rim = std::fabs(dx) > 0.82f - bottom * bottom || y == 1 || (y > 12);
          int k = x < 8 ? 3 : 2;
          if (x > 11 || y > 11) k--;
          c.set(x, y, rim ? M[k + (x < 8 ? 1 : 0)] : kRed[k]);
        }
      for (int i = 0; i < 4; i++) { c.set(5 + i, 5 + i, kGold[3]); c.set(10 - i, 5 + i, kGold[2]); }
      c.set(4, 2, kWhite);
      break;
    }
    case Icon::Helmet:
      for (int y = 2; y <= 13; y++)
        for (int x = 2; x <= 13; x++) {
          float dx = (x + 0.5f - 8) / 5.6f, dy = (y + 0.5f - 8) / 6.0f;
          if (y < 8 ? (dx * dx + dy * dy > 1) : std::fabs(dx) > 1) continue;
          c.set(x, y, M[lightIndex(lightAt(dx * 0.85f, std::min(0.6f, dy) * 0.85f), x, y, 0.1f)]);
        }
      hline(c, 4, 11, 8, kInk); hline(c, 4, 11, 9, M[0]);   // visor slit
      vline(c, 7, 9, 13, M[3]); vline(c, 8, 9, 13, M[1]);   // nasal guard
      vline(c, 8, 1, 3, kRed[3]); c.set(9, 1, kRed[2]); c.set(9, 2, kRed[1]);
      break;
    case Icon::Armor:
      for (int y = 2; y <= 14; y++)
        for (int x = 1; x <= 14; x++) {
          bool shoulders = y <= 5 && (x <= 4 || x >= 11) && y >= 2;
          bool torso = x >= 4 && x <= 11 && y >= 3;
          bool neck = y < 5 && x >= 6 && x <= 9;
          if (!(shoulders || torso) || neck) continue;
          int k = x < 6 ? 3 : (x > 10 ? 1 : 2);
          if (shoulders && y == 2) k = 4;
          c.set(x, y, M[k]);
        }
      vline(c, 7, 6, 13, M[3]); vline(c, 8, 6, 13, M[1]);
      hline(c, 4, 11, 11, kLeather[1]); c.set(7, 11, kGold[3]); c.set(8, 11, kGold[2]);
      c.set(5, 6, M[4]);
      break;
    case Icon::Boots: {
      // a pair in profile, toes to the right: the far boot sits up-left in shadow; folded cuff, heel, dark sole
      const Ramp& B = tinted ? M : kLeather;
      for (int b = 0; b < 2; b++) {
        int ox = b ? 0 : -3, oy = b ? 0 : -2, bias = b ? 0 : -1;
        std::vector<Vec2> shape = {{5.0f + ox, 2.5f + oy}, {10.0f + ox, 2.5f + oy}, {10.0f + ox, 8.5f + oy}, {12.5f + ox, 9.5f + oy},
                                   {14.5f + ox, 11.0f + oy}, {14.5f + ox, 14.0f + oy}, {4.5f + ox, 14.0f + oy}, {4.5f + ox, 9.0f + oy}};
        Canvas part(ICON, ICON);
        poly(part, shape, B[2]);
        for (int y = 0; y < ICON; y++)
          for (int x = 0; x < ICON; x++) {
            if (!solid(part, x, y)) continue;
            int lx = x - ox, ly = y - oy, k = 2;
            if (lx <= 5) k = 3;
            if (lx >= 9 && ly < 9) k = 1;
            if (ly <= 3) k = lx <= 6 ? 4 : 3;              // the folded cuff catches the light
            if (ly == 4) k = 1;                             // ...and shades the shaft below it
            if (ly >= 10 && lx >= 11 && ly <= 11) k = 3;    // toe cap
            if (ly == 13) k = 0;                            // sole
            if (ly == 12 && lx >= 5 && lx <= 7) k = 1;      // heel
            if (tinted && (ly == 6 || ly == 8) && lx >= 5 && lx <= 9) k = std::max(0, k - 2);   // greave plates
            c.set(x, y, B[std::clamp(k + bias, 0, 4)]);
          }
      }
      break;
    }
    case Icon::Cloak: {
      // a hooded cloak hanging from its clasp: a shadowed opening and two folds; tint = the cloth
      const Ramp K = tinted ? ramp(tint) : ramp(rgba(120, 44, 40));
      poly(c, {{5.5f, 2.0f}, {10.5f, 2.0f}, {11.5f, 5.0f}, {14.5f, 14.5f}, {11.0f, 13.5f}, {8.0f, 15.0f}, {5.0f, 13.5f}, {1.5f, 14.5f}, {4.5f, 5.0f}}, K[2]);
      for (int y = 0; y < ICON; y++)
        for (int x = 0; x < ICON; x++) {
          if (!solid(c, x, y)) continue;
          int k = 2;
          if (x <= 4 || (x == 5 && y < 9)) k = 3;
          if (x >= 11) k = 1;
          if (y > 6 && (x == 6 || x == 10)) k = 1;   // folds
          if (y > 6 && (x == 5 || x == 9)) k = std::min(4, k + 1);
          if (y <= 3) k = x <= 7 ? 4 : 3;           // the hood's crown
          c.set(x, y, K[k]);
        }
      poly(c, {{7.0f, 8.0f}, {9.0f, 8.0f}, {10.0f, 14.0f}, {8.0f, 15.0f}, {6.0f, 14.0f}}, K[0]);   // the opening
      hline(c, 6, 10, 5, K[1]);                                                                   // hood rim
      c.set(7, 6, kGold[4]); c.set(8, 6, kGold[2]); c.set(7, 7, kGold[1]);                       // clasp
      break;
    }
    case Icon::Gloves: {
      // gauntlet: four fingers, thumb, knuckle plate, leather cuff
      const Ramp& G = M;
      for (int f = 0; f < 4; f++) {
        int x = 5 + f * 2, top = f == 0 || f == 3 ? 4 : 2;
        for (int y = top; y <= 7; y++) c.set(x, y, G[y == top ? 4 : (f < 2 ? 3 : 2)]);
        c.set(x, 5, G[1]);
      }
      for (int y = 7; y <= 11; y++) for (int x = 4; x <= 11; x++) c.set(x, y, G[x < 6 ? 3 : (x > 9 ? 1 : 2)]);
      hline(c, 4, 11, 7, G[4]);
      for (int k = 0; k < 3; k++) { c.set(3 - k / 2, 8 + k, G[3]); c.set(2, 7 + k, G[2]); }
      for (int y = 12; y <= 14; y++) for (int x = 3; x <= 12; x++) c.set(x, y, kLeather[y == 12 ? 4 : (x < 6 ? 3 : 2)]);
      break;
    }
    case Icon::Ring: {
      const Ramp& Rg = tinted ? M : kGold;
      for (int a = 0; a < 48; a++) {
        float ang = a / 48.0f * TAU;
        int x = (int)std::lround(8 + std::cos(ang) * 4.5f - 0.5f), y = (int)std::lround(9.5f + std::sin(ang) * 3.6f - 0.5f);
        int k = lightIndex(lightAt(std::cos(ang) * 0.8f, std::sin(ang) * 0.8f), x, y, 0);
        c.set(x, y, Rg[k]);
        c.set(x, y + 1, Rg[std::max(0, k - 1)]);
      }
      ball(c, 8, 4.5f, 2.4f, 2.2f, ramp(rgba(220, 50, 80)));
      c.set(7, 3, kWhite);
      break;
    }
    case Icon::Amulet: {
      const Ramp& Rg = tinted ? M : kGold;
      for (int i = 0; i <= 10; i++) {
        float t = i / 10.0f;
        int x = (int)std::lround(3 + t * 10), y = (int)std::lround(2 + std::sin(t * PI) * 5);
        c.set(x, y, Rg[(i & 1) ? 2 : 4]);
      }
      ball(c, 8, 11, 3.4f, 3.6f, Rg);
      ball(c, 8, 11, 2.0f, 2.2f, kCrystal);
      c.set(7, 10, kWhite);
      break;
    }
    case Icon::PotionRed: potion(c, tinted ? M : ramp5(rgba(96, 18, 40), rgba(156, 28, 44), rgba(214, 50, 52), rgba(244, 96, 80), rgba(255, 170, 140))); break;
    case Icon::PotionBlue: potion(c, tinted ? M : ramp5(rgba(24, 30, 96), rgba(36, 60, 160), rgba(56, 104, 214), rgba(100, 156, 244), rgba(180, 216, 255))); break;
    case Icon::PotionGreen: potion(c, tinted ? M : ramp5(rgba(20, 70, 48), rgba(34, 120, 56), rgba(64, 176, 70), rgba(120, 220, 96), rgba(200, 250, 170))); break;
    case Icon::Bread: {
      const Ramp Br = ramp5(rgba(110, 54, 34), rgba(166, 92, 44), rgba(212, 140, 64), rgba(236, 186, 104), rgba(250, 226, 160));
      ball(c, 8, 9, 6.5f, 4.2f, Br);
      for (int k = 0; k < 3; k++) { c.set(5 + k * 3, 7, Br[4]); c.set(6 + k * 3, 8, Br[1]); c.set(6 + k * 3, 6, Br[4]); }
      break;
    }
    case Icon::Meat: {
      const Ramp Mt = ramp5(rgba(90, 24, 34), rgba(146, 46, 42), rgba(192, 82, 60), rgba(222, 124, 88), rgba(244, 172, 136));
      ball(c, 6.5f, 7.5f, 5.0f, 4.6f, Mt);
      capsule(c, V(9, 10), V(13, 13), 1.2f, 1.0f, kBone);
      ball(c, 13.5f, 13, 1.6f, 1.4f, kBone); ball(c, 12.8f, 14.2f, 1.3f, 1.2f, kBone);
      c.set(4, 5, Mt[4]); c.set(5, 5, Mt[4]); c.set(7, 9, Mt[1]);
      break;
    }
    case Icon::Apple: {
      const Ramp Ap = tinted ? M : kRed;
      ball(c, 8, 9.5f, 5.2f, 4.9f, Ap);
      c.set(8, 5, Ap[0]); vline(c, 8, 2, 4, kWood[1]);
      c.set(9, 3, kLeaf[3]); c.set(10, 3, kLeaf[3]); c.set(11, 2, kLeaf[2]); c.set(10, 2, kLeaf[4]);
      c.set(5, 7, kWhite); c.set(5, 8, Ap[4]);
      break;
    }
    case Icon::Cheese: {
      const Ramp Ch = ramp5(rgba(150, 100, 30), rgba(206, 150, 40), rgba(240, 196, 70), rgba(252, 226, 116), rgba(255, 246, 180));
      poly(c, {{1.5f, 11.5f}, {13.5f, 5.0f}, {14.5f, 11.5f}, {14.5f, 14.0f}, {1.5f, 14.0f}}, Ch[2]);
      poly(c, {{1.5f, 11.5f}, {13.5f, 5.0f}, {14.5f, 11.5f}}, Ch[4]);
      hline(c, 2, 14, 12, Ch[3]);
      c.set(5, 13, Ch[0]); c.set(10, 13, Ch[0]); c.set(11, 13, Ch[1]); c.set(8, 10, Ch[1]); c.set(12, 9, Ch[1]);
      break;
    }
    case Icon::Gold: {
      const Ramp& G = tinted ? M : kGold;
      const float cs[6][2] = {{5, 12}, {10, 12}, {7.5f, 9.5f}, {12, 9}, {4, 9}, {8, 6.5f}};
      for (auto& p : cs) {
        ellipse(c, p[0], p[1], 3.2f, 1.8f, G[1]);
        ellipse(c, p[0], p[1] - 0.6f, 3.0f, 1.5f, G[3]);
        c.set((int)p[0] - 1, (int)p[1] - 1, G[4]);
      }
      break;
    }
    case Icon::Gem: {
      const Ramp& Gm = tinted ? M : ramp5(rgba(30, 50, 110), rgba(36, 100, 180), rgba(60, 160, 230), rgba(130, 214, 250), rgba(230, 252, 255));
      poly(c, {{8, 2}, {14, 7}, {8, 15}, {2, 7}}, Gm[2]);
      poly(c, {{8, 2}, {8, 7}, {2, 7}}, Gm[4]);
      poly(c, {{8, 2}, {14, 7}, {8, 7}}, Gm[3]);
      poly(c, {{2, 7}, {8, 7}, {8, 15}}, Gm[2]);
      poly(c, {{8, 7}, {14, 7}, {8, 15}}, Gm[1]);
      c.set(5, 5, kWhite);
      break;
    }
    case Icon::Key: {
      const Ramp& K = tinted ? M : kBrass;
      for (int a = 0; a < 24; a++) {
        float ang = a / 24.0f * TAU;
        c.set((int)std::lround(4.5f + std::cos(ang) * 2.6f), (int)std::lround(4.5f + std::sin(ang) * 2.6f), K[std::sin(ang) < 0 ? 4 : 2]);
      }
      line(c, 6, 6, 13, 13, K[3]); line(c, 7, 6, 13, 12, K[1]);
      c.set(11, 13, K[2]); c.set(10, 14, K[2]); c.set(13, 11, K[2]); c.set(14, 10, K[1]);
      break;
    }
    case Icon::Scroll:
      for (int y = 3; y <= 12; y++) for (int x = 3; x <= 12; x++) c.set(x, y, kCloth[x < 5 ? 4 : (x > 10 ? 2 : 3)]);
      for (int x = 2; x <= 13; x++) { c.set(x, 2, kCloth[4]); c.set(x, 3, kCloth[2]); c.set(x, 13, kCloth[2]); c.set(x, 12, kCloth[3]); }
      for (int y = 5; y <= 10; y += 2) hline(c, 5, 10 - (y % 4 == 1 ? 2 : 0), y, kCloth[0]);
      vline(c, 8, 1, 14, kRed[2]); c.set(9, 13, kRed[1]); c.set(7, 14, kRed[3]);
      break;
    case Icon::Book: {
      const Ramp Bk = tinted ? M : ramp5(rgba(56, 22, 34), rgba(90, 30, 40), rgba(130, 44, 48), rgba(170, 70, 60), rgba(206, 110, 86));
      for (int y = 2; y <= 13; y++) for (int x = 3; x <= 12; x++) c.set(x, y, Bk[x == 3 ? 1 : (x < 6 ? 3 : 2)]);
      for (int y = 3; y <= 13; y++) c.set(13, y, kCloth[(y & 1) ? 3 : 2]);
      hline(c, 4, 13, 14, kCloth[2]);
      vline(c, 4, 2, 13, Bk[0]);
      box(c, 7, 6, 10, 9, kGold[2]); c.set(7, 6, kGold[4]); c.set(8, 7, kGold[1]);
      break;
    }
    case Icon::Map:
      poly(c, {{1, 3}, {5, 2}, {10, 3}, {15, 2}, {15, 13}, {10, 14}, {5, 13}, {1, 14}}, kCloth[3]);
      poly(c, {{5, 2}, {10, 3}, {10, 14}, {5, 13}}, kCloth[2]);
      line(c, 3, 11, 6, 8, kRed[2]); c.set(8, 7, kRed[2]); c.set(9, 6, kRed[2]); c.set(10, 7, kRed[2]);
      c.set(12, 5, kRed[1]); c.set(13, 6, kRed[1]); c.set(12, 6, kRed[3]); c.set(13, 5, kRed[3]);
      c.set(3, 5, kLeaf[2]); c.set(4, 5, kLeaf[1]); c.set(3, 4, kLeaf[3]);
      break;
    case Icon::Pelt: {
      const Ramp Pl = ramp5(rgba(64, 40, 38), rgba(104, 68, 50), rgba(146, 102, 70), rgba(182, 140, 96), rgba(214, 180, 130));
      furBall(c, 8, 8, 5.5f, 5.0f, Pl, 0.5f, 3);
      for (int k : {-1, 1}) { capsule(c, V(8 + k * 4, 5), V(8 + k * 7, 2), 1.4f, 0.8f, Pl); capsule(c, V(8 + k * 4, 11), V(8 + k * 7, 14), 1.4f, 0.8f, Pl); }
      capsule(c, V(8, 12), V(9, 15), 1.0f, 0.5f, Pl, -1);
      break;
    }
    case Icon::Bone:
      capsule(c, V(4, 12), V(12, 4), 1.3f, 1.3f, kBone);
      ball(c, 3, 11.5f, 1.8f, 1.8f, kBone); ball(c, 4.5f, 13, 1.8f, 1.8f, kBone);
      ball(c, 11.5f, 3, 1.8f, 1.8f, kBone); ball(c, 13, 4.5f, 1.8f, 1.8f, kBone);
      break;
    case Icon::Ore:
      rock(c, 8, 9, 6.5f, 5.5f, kStone, 41, 5);
      for (int i = 0; i < 6; i++) {
        int x = 4 + (int)(hash3(i, 0, 9) % 8), y = 6 + (int)(hash3(i, 1, 9) % 6);
        if (solid(c, x, y)) { c.set(x, y, (tinted ? M : kGold)[4]); c.set(x + 1, y, (tinted ? M : kGold)[2]); }
      }
      break;
    case Icon::Herb:
      vline(c, 8, 6, 14, kLeaf[1]);
      for (int k = 0; k < 3; k++) {
        int y = 4 + k * 3;
        ellipse(c, 5.5f, y + 1.0f, 2.5f, 1.4f, (tinted ? M : kLeaf)[3 - (k & 1)]);
        ellipse(c, 10.5f, y + 2.0f, 2.5f, 1.4f, (tinted ? M : kLeaf)[2]);
      }
      c.set(8, 2, rgba(240, 220, 250)); c.set(7, 3, rgba(200, 150, 230)); c.set(9, 3, rgba(200, 150, 230));
      break;
    case Icon::Sigil: {
      const Ramp& Sg = tinted ? M : ramp5(rgba(40, 30, 90), rgba(70, 50, 150), rgba(110, 90, 210), rgba(160, 150, 240), rgba(230, 230, 255));
      ball(c, 8, 8, 6.5f, 6.5f, kStone);
      for (int a = 0; a < 5; a++) {   // glowing pentagram-like rune
        float a0 = -PI / 2 + a * TAU / 5, a1 = -PI / 2 + (a + 2) * TAU / 5;
        line(c, (int)std::lround(8 + std::cos(a0) * 4.5f - 0.5f), (int)std::lround(8 + std::sin(a0) * 4.5f - 0.5f),
             (int)std::lround(8 + std::cos(a1) * 4.5f - 0.5f), (int)std::lround(8 + std::sin(a1) * 4.5f - 0.5f), Sg[3]);
      }
      c.set(7, 7, Sg[4]); c.set(8, 8, Sg[4]);
      break;
    }
    case Icon::Letter:
      for (int y = 4; y <= 12; y++) for (int x = 1; x <= 14; x++) c.set(x, y, kCloth[x < 3 ? 4 : 3]);
      line(c, 1, 4, 8, 9, kCloth[1]); line(c, 14, 4, 8, 9, kCloth[1]);
      line(c, 1, 12, 6, 8, kCloth[2]); line(c, 14, 12, 10, 8, kCloth[2]);
      ball(c, 8, 9.5f, 1.9f, 1.8f, kRed); c.set(7, 9, kRed[4]);
      break;
    case Icon::Crown: {
      const Ramp& G = tinted ? M : kGold;
      for (int y = 8; y <= 13; y++) for (int x = 2; x <= 13; x++) c.set(x, y, G[x < 5 ? 4 : (x > 11 ? 2 : 3)]);
      for (int p = 0; p < 3; p++) {
        int px2 = 3 + p * 5;
        for (int y = 3; y < 8; y++) { int half = (y - 3) / 2; for (int x = px2 - half; x <= px2 + half; x++) c.set(x, y, G[x <= px2 ? 3 : 2]); }
        c.set(px2, 2, G[4]);
      }
      hline(c, 2, 13, 12, G[1]); hline(c, 2, 13, 13, G[0]);
      c.set(5, 10, rgba(220, 50, 70)); c.set(8, 10, rgba(80, 160, 240)); c.set(11, 10, rgba(80, 200, 110));
      c.set(4, 9, kWhite);
      break;
    }
    // ---- M6 Steel (phase A stand-ins; the ARMS lane paints them properly)
    case Icon::Spear: spearIcon(c, 1, M, 0, 0); break;
    case Icon::Ingot: ingotIcon(c, tinted ? M : kIron); break;
    default: break;
  }
}

}  // namespace

Canvas itemIcon(Icon i, uint32_t tint) {
  Canvas c(ICON, ICON);
  paintIcon(c, i, tint);
  outline(c);
  return c;
}

// M6 Steel: an icon in its maker culture's style and its metal (every field 0 but icon / tint: the classic icon)
Canvas itemIconLook(const IconLook& l) {
  if (!l.tint2 && !l.form && !l.sheen && !l.ornament && !l.accent && !l.rarity && !l.mat) return itemIcon(l.icon, l.tint);
  Canvas c(ICON, ICON);
  const bool leather = l.mat == 1;   // Mat::Leather
  Ramp M = kIron;
  if (l.tint2 || l.sheen) {
    uint32_t k[5];
    metalRampKeys(l.tint, l.tint2, l.sheen, k);
    M = ramp5(k[0], k[1], k[2], k[3], k[4]);
  } else if (leather && (l.icon == Icon::Helmet || l.icon == Icon::Armor || l.icon == Icon::Shield || l.icon == Icon::Gloves || l.icon == Icon::Boots)) {
    M = kLeather;
  } else if (l.tint) {
    M = ramp(l.tint, 1.1f);
  }
  const Ramp acc = l.accent ? ramp(l.accent) : kRed;
  const Ramp trim = (l.ornament & 4) ? kGold : M;
  const Ramp grip = l.accent ? ramp(mix(l.accent, rgba(92, 54, 40), 0.35f)) : kLeather;
  const Ramp& gold = (l.ornament & 4) ? kGold : kBrass;
  bool done = true;
  switch (l.icon) {
    case Icon::Helmet: if (l.form >= 1 && l.form <= 10) helmIcon(c, l.form, M, acc, trim); else done = false; break;
    case Icon::Armor: if (l.form >= 1 && l.form <= 8) bodyIcon(c, l.form, M, l.accent ? acc : ramp(rgba(120, 60, 50))); else done = false; break;
    case Icon::Shield: if (l.form >= 1 && l.form <= 8) shieldIcon(c, l.form, M, acc, (l.ornament & 4) != 0); else done = false; break;
    case Icon::Sword: case Icon::Greatsword: case Icon::Dagger: bladeIcon(c, l.icon, l.form ? l.form : 1, M, grip, gold); break;
    case Icon::Spear: spearIcon(c, l.form, M, l.accent && (l.ornament & (64 | 8 | 1)) ? l.accent : 0, l.ornament); break;
    case Icon::Bow: if (l.form >= 1 && l.form <= 5) bowIcon(c, l.form, l.accent, l.ornament); else done = false; break;
    default: done = false; break;
  }
  if (!done) {
    const bool metalKind = l.icon == Icon::Gloves || l.icon == Icon::Boots || l.icon == Icon::Helmet || l.icon == Icon::Armor ||
                           l.icon == Icon::Shield || l.icon == Icon::Axe || l.icon == Icon::Mace || l.icon == Icon::Ingot;
    paintIcon(c, l.icon, l.tint, metalKind ? &M : nullptr);
  }
  if (l.sheen) iconSheen(c, M, l.sheen);
  iconOrnament(c, M, l.ornament, l.icon);
  iconGlint(c, l.rarity);
  const Canvas raw = c;
  outline(c);
  if (l.rarity >= 4) {   // a legendary's outline warms toward gold
    for (int y = 0; y < ICON; y++)
      for (int x = 0; x < ICON; x++) {
        const uint32_t v = c.get(x, y);
        if ((v >> 24) && !(raw.get(x, y) >> 24)) c.set(x, y, mix(v, rgba(255, 190, 80), 0.45f));
      }
  }
  return c;
}

}  // namespace art
