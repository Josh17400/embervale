// EMBERVALE art: item icons. See rpg/art.h for the contract and rpg/art/art_internal.h for the shared helpers.
#include "rpg/art/art_internal.h"
#include "rpg/art/doll/doll.h"
#include "rpg/art/art_rig.h"
#include "rpg/art/doll/doll_envelope.h"
#include "rpg/core/fingerprint.h"
#include <cstring>
#include <initializer_list>

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
// (P4 ART, 6e; the W7 testers: "the body icon is a grey-white vest for both leather and bronze") every body icon wears its
// MATERIAL: a leather gambeson is quilted leather laced up the front in rawhide (never the maker's cloth, so the bag, the shop
// and the worn coat agree: D116), a brigandine is a coat of plates in its metal with the maker's cloth at the shoulders and
// the plate seams (as the doll's D114 re-cut); a padded coat of no leather keeps the maker's cloth
void bodyIcon(Canvas& c, int form, const Ramp& M, const Ramp& acc, bool leather) {
  // (rig 2 only: rig 1's icons stay as they were, art_hash --rig=1)
  const bool p4 = rigVersion() >= 2;
  const bool leatherCoat = p4 && form == 1 && leather, plates = p4 && form == 6 && !leather;
  const Ramp& base = leatherCoat ? kLeather : (form == 1 || form == 6) ? acc : (form == 2 ? kLeather : M);
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
        case 5:   // laced plates: (M8b fix) 3 x 3 plates with a lit top, a shadowed seam and one lace knot each, never a
                  // full row of alternating lace pixels (it read as a red-and-yellow checker on alloy pieces)
          if (ry > 0 && ry % 3 == 0) { if (x % 3 == 1) { c.set(x, y, mix(acc[1], base[1], 0.45f)); continue; } k--; }
          else if (x % 3 == 0) k--;
          else if (ry % 3 == 1) k++;
          break;
        case 6:
          if (plates) {   // (D117) courses of plates in the metal, the shoulders too: the maker's cloth in the seam between
            // two courses (half cloth, half the plate's shadow), the plate's lit top edge with a rivet head every third pixel,
            // its body
            const int t = ry % 3;
            if (ry > 0 && t == 0 && torso) { c.set(x, y, mix(acc[std::clamp(k - 1, 0, 4)], M[std::clamp(k - 1, 0, 4)], 0.5f)); continue; }
            else if (t == 1) { if (x % 3 == 2) { c.set(x, y, M[std::max(0, k - 2)]); continue; } k++; }
            c.set(x, y, M[std::clamp(k, 0, 4)]);
            continue;
          }
          if (ry > 0 && (ry % 3) == 1 && ((x + ry) & 1)) { c.set(x, y, M[4]); continue; }   // rivets
          break;
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
  } else if (leatherCoat) {   // (D116) the leather gambeson: laced up the front in rawhide (the lit cross a step over the
    // hide's lit plane), a dark belt with its buckle
    static const uint32_t kLace[2] = {rgba(238, 222, 178), rgba(192, 164, 116)};
    for (int y = 5; y <= 10; y++) {
      const bool a = (y & 1) != 0;
      c.set(7, y, a ? kLace[0] : kLeather[0]);
      c.set(8, y, a ? kLeather[0] : kLace[1]);
    }
    for (int x = 4; x <= 11; x++) c.set(x, 11, kLeather[x < 7 ? 1 : 0]);
    c.set(7, 11, kBrass[4]); c.set(8, 11, kBrass[2]);
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
      else if (sheen == 6) {   // (M8b fix) lit seam lines under the plates, not every other row
        if (k == 0) {
          const int ku = idx(src.get(x, y - 1)), kl = idx(src.get(x - 1, y));
          if (ku >= 2 || kl >= 2) { c.set(x, y, mix(lighten(A[3], 0.6f), A[0], 0.35f)); continue; }
        }
      }
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
  // (M8b fix) at 16 px every dot lattice is noise when several stack (an alloy Legendary carried studs, rivets, etching
  // and filigree at once: confetti). The rim may be gilded; the face carries ONE motif (filigree > studs > rivets >
  // etching), sparse and set on the lit plates only, so the piece's shape and shading always read first.
  const uint16_t face = (o & 512) ? 512 : (o & 16) ? 16 : (o & 256) ? 256 : (o & 32) ? 32 : 0;
  auto kOf = [&](int x, int y) { const uint32_t v = src.get(x, y); for (int k = 0; k < 5; k++) if (A[k] == v) return k; return -1; };
  for (int y = 0; y < ICON; y++)
    for (int x = 0; x < ICON; x++) {
      if (!isA(x, y)) continue;
      const bool edge = !(src.get(x - 1, y) >> 24) || !(src.get(x, y - 1) >> 24) || !(src.get(x + 1, y) >> 24) || !(src.get(x, y + 1) >> 24);
      if ((o & 4) && edge) { c.set(x, y, kGold[(!(src.get(x - 1, y) >> 24) || !(src.get(x, y - 1) >> 24)) ? 4 : 2]); continue; }
      if (edge || !face) continue;
      const int k = kOf(x, y);
      // never on the shadow steps, and only on a flat plate (all four neighbours the same step): a textured form (laced
      // plates, scales, rings) already carries its own pattern and a motif over it is speckle
      if (k < 2 || kOf(x - 1, y) != k || kOf(x + 1, y) != k || kOf(x, y - 1) != k || kOf(x, y + 1) != k) continue;
      if (face == 512 && ((x + y * 3) % 11) == 0) c.set(x, y, kGold[k >= 3 ? 4 : 3]);
      else if (face == 16 && (x % 4) == 1 && (y % 4) == 2) c.set(x, y, kBrass[4]);
      else if (face == 256 && (x % 5) == 2 && (y % 4) == 1) c.set(x, y, mix(A[4], kWhite, 0.5f));
      else if (face == 32 && ((x * 2 + y) % 9) == 0 && k == 2) c.set(x, y, A[3]);
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
  if (rarity >= 5) {   // (M8b) a Mythic's flame glint: a hot white-yellow heart, orange arms, an ember tongue above
    c.set(bx, by, rgba(255, 246, 200));
    for (int d = 0; d < 4; d++) {
      const int nx = bx + (d == 0) - (d == 1), ny = by + (d == 2) - (d == 3);
      if (c.get(nx, ny) >> 24) c.set(nx, ny, mix(c.get(nx, ny), d == 3 ? rgba(255, 200, 90) : rgba(255, 120, 40), 0.75f));
    }
    if (by >= 2 && (c.get(bx, by - 2) >> 24)) c.set(bx, by - 2, mix(c.get(bx, by - 2), rgba(232, 60, 28), 0.6f));
    return;
  }
  const uint32_t g = rarity >= 4 ? rgba(255, 226, 140) : rgba(220, 236, 255);
  c.set(bx, by, kWhite);
  for (int d = 0; d < 4; d++) {
    const int nx = bx + (d == 0) - (d == 1), ny = by + (d == 2) - (d == 3);
    if (c.get(nx, ny) >> 24) c.set(nx, ny, mix(c.get(nx, ny), g, 0.7f));
  }
}

// ---------------------------------------------------------------- P5: the maker people's fingerprint (docs/FIX_BATCH_P5.md 3.3, 4.5)
// One pass over a painted armour icon (rig 2, FIX_FP, a maker people), after the piece and its sheen and before the ornament:
// the people's dye band (fp::row: the doll's own table, ramped by the doll's rampA3, so the bag and the hero agree) on the
// piece's rims, seams and cuffs, and its motif as a glyph on the chest, climbing with the look tier as the doll's finish
// does: thread at T1-T2, engraving at T3-T4, gold at T5-T6. Data, not code: kFpGlyphs (a motif -> its drawings), kFpMarks
// (a row per Icon and form: where the band lies and on which tiers), kFpChest (where the glyph sits). Nothing here reads
// the people but the table, so a new people is a new fp::row and a new form a new mark row.

// the motif glyphs (MARKS's marks at icon scale; STYLE_P2 27.3: a shape, every thread pixel touching another, never a lone
// dot): 'a' the dye, 'b' the second colour (the dye where the band is solid), '.' the plate. 4x4 on an open chest, 3x3 where
// a laced or seamed front leaves only a breast
struct FpGlyph { const char* motif; const char* g4; const char* g3; };
const FpGlyph kFpGlyphs[] = {
    {"knot",   ".aa." "a..b" "b..a" ".aa.",  ".a." "a.b" ".a."},    // Fjordfolk: a knotwork ring
    {"plaid",  ".a.." "bbbb" ".a.." ".a..",  ".a." "bab" ".a."},    // Highland: the tartan sett, a check of two threads
    {"oak",    ".a.." "aaa." ".aaa" "..b.",  "aa." "aaa" ".b."},    // Heartland: an oak leaf on its stem
    {"laurel", "a..a" "a..a" "a..a" ".bb.",  "a.a" "a.a" ".b."},    // Imperial: the laurel wreath
    {"arab",   ".aaa" ".a.." "..a." "bbb.",  "aa." ".a." ".bb"},    // Dune: an arabesque scroll
    {"horse",  "aa.." ".aaa" ".a.a" ".b.b",  "a.." "aaa" "b.b"},    // Steppe: a running horse
    {"reed",   "...a" ".a.a" ".a.a" "bbbb",  "..a" "a.a" "bbb"},    // Marsh: reeds bound at the foot
    {"clouds", ".aa." "a..a" "a.aa" ".b..",  ".aa" "a.a" "bb."},    // Jade: a cloud scroll
    {"wave",   ".a.." "a.a." "...a" "..bb",  ".a." "a.a" "..b"},    // River: a breaking wave
    {"ray",    "a..a" ".ab." ".ba." "a..a",  "a.a" ".b." "a.a"},    // Sun Temple: the sun and its rays
    {"vein",   "..aa" ".a.a" "a.b." "ab..",  ".aa" "a.a" "b.."},    // Sylvan: a leaf and its vein
    {"star",   ".a.." "aba." ".a.." "....",  ".a." "aba" ".a."},    // Starspire: a four-point star
};
const FpGlyph* fpGlyph(const char* motif) {
  for (const FpGlyph& g : kFpGlyphs) if (!std::strcmp(g.motif, motif)) return &g;
  return nullptr;
}

// where a band lies: the opaque pixels of a rect (All), those on the piece's edge (Edge: a clear 4-neighbour), or the pixel
// above each column's lowest (AboveBottom: a hem's inner row, so the outline keeps the material's dark); a pattern thins it
// (x % xm == xr, y % ym == yr, and (x + y) % dm < dr: a lashed binding, runs along the edge, never a checker)
enum class FpAt : uint8_t { All, Edge, AboveBottom };
struct FpMark {
  Icon icon; int8_t form;            // IconLook::form (-1: every form)
  uint16_t mats;                     // bit m: drawn on a piece of Mat m (IconLook::mat; 0: every material)
  uint8_t tiers;                     // bit t: drawn at look tier t (1-6)
  FpAt at; int8_t x0, y0, x1, y1;
  uint8_t xm, xr, ym, yr, dm, dr;
};
constexpr uint8_t kT12 = 0x06, kT35 = 0x38, kT15 = 0x3E, kT14 = 0x1E;
// leather pieces carry the least thread (D117: a leather coat's, cap's, gloves' and boots' mean stays a brown, FIX_BATCH_P5 3.5)
constexpr uint16_t kAnyM = 0, kLeatherM = 1u << 1, kHardM = (uint16_t)~(1u << 1);
const FpMark kFpMarks[] = {
    // body: the form's own seams (its N rows: laces, stitches, the shadow between courses) in thread from T1 to T5. Never
    // a whole rim at 16 px: a dyed hem or collar pulled a people's bronze and iron coats together by their means (D117), a
    // shadow seam is dark on every metal, so its thread moves no material read
    {Icon::Armor, 1, kAnyM, kT15, FpAt::All,   7, 5, 7, 10,   0, 0, 2, 0, 0, 0},   // the gambeson's lacing: the dye threaded
    {Icon::Armor, 1, kAnyM, kT15, FpAt::All,   8, 5, 8, 10,   0, 0, 2, 1, 0, 0},   // between D116's rawhide crossings
    {Icon::Armor, 2, kAnyM, kT15, FpAt::All,   7, 5, 7, 13,   0, 0, 2, 1, 0, 0},   // the jerkin's front seam
    {Icon::Armor, 3, kAnyM, kT15, FpAt::All,   4, 11, 6, 11,  0, 0, 0, 0, 0, 0},   // the mail shirt's belt, by its buckle
    {Icon::Armor, 3, kAnyM, kT15, FpAt::All,   9, 11, 11, 11, 0, 0, 0, 0, 0, 0},
    {Icon::Armor, 4, kAnyM, kT15, FpAt::All,   4, 12, 11, 12, 0, 0, 0, 0, 0, 0},   // the scales' lowest course
    {Icon::Armor, 5, kAnyM, kT15, FpAt::All,   4, 9, 11, 12,  3, 1, 3, 0, 0, 0},   // the lamellar's lower lace knots
    {Icon::Armor, 6, kAnyM, kT12, FpAt::All,   5, 12, 9, 12,  0, 0, 0, 0, 0, 0},   // the brigandine's seam: its middle at T1-T2
    {Icon::Armor, 6, kAnyM, kT35, FpAt::All,   4, 12, 11, 12, 0, 0, 0, 0, 0, 0},   // and whole from T3
    {Icon::Armor, 7, kAnyM, kT15, FpAt::All,   4, 12, 11, 12, 0, 0, 0, 0, 0, 0},   // the plate's fauld line
    {Icon::Armor, 8, kAnyM, kT15, FpAt::All,   4, 13, 11, 13, 0, 0, 0, 0, 0, 0},   // the leaves' lowest course
    // helm: the brow band (the rim at T1-T2, its lower row the lining at T3-T5: one row at 16 px)
    {Icon::Helmet, 1, kAnyM, kT15, FpAt::All,  4, 9, 11, 9,   0, 0, 0, 0, 0, 0},
    {Icon::Helmet, 2, kAnyM, kT15, FpAt::All,  4, 9, 11, 9,   0, 0, 0, 0, 0, 0},
    {Icon::Helmet, 3, kAnyM, kT15, FpAt::All,  4, 13, 11, 13, 0, 0, 0, 0, 0, 0},
    {Icon::Helmet, 4, kAnyM, kT15, FpAt::All,  3, 10, 12, 10, 0, 0, 0, 0, 0, 0},
    {Icon::Helmet, 5, kAnyM, kT15, FpAt::All,  4, 11, 11, 11, 0, 0, 0, 0, 0, 0},
    {Icon::Helmet, 6, kAnyM, kT15, FpAt::All,  4, 11, 11, 11, 0, 0, 0, 0, 0, 0},
    {Icon::Helmet, 7, kAnyM, kT15, FpAt::All,  4, 8, 11, 8,   0, 0, 0, 0, 0, 0},
    {Icon::Helmet, 8, kAnyM, kT15, FpAt::All,  4, 8, 11, 8,   0, 0, 0, 0, 0, 0},
    {Icon::Helmet, 9, kAnyM, kT15, FpAt::All,  4, 9, 11, 9,   0, 0, 0, 0, 0, 0},
    {Icon::Helmet, 10, kAnyM, kT15, FpAt::All, 4, 11, 11, 11, 0, 0, 0, 0, 0, 0},
    // shield: the binding lashed over the board's edge, runs of 2 every 8 px (a lashing wraps the edge; the kingdom's field and
    // device stay as painted), T1-T5
    {Icon::Shield, -1, kAnyM, kT15, FpAt::Edge, 0, 0, 15, 15, 0, 0, 0, 0, 8, 2},   // (fix r1: every 8 px, so the lashing and its ring hold 15 %)
    // gloves: the cuff's middle row (leather), and its lit top row on a gauntlet; boots: each boot's cuff (its lower row;
    // the top row too on a hard boot), the far boot's, then the near one's; T1-T5
    {Icon::Gloves, -1, kAnyM, kT15, FpAt::All,  4, 13, 11, 13, 0, 0, 0, 0, 0, 0},
    {Icon::Gloves, -1, kHardM, kT15, FpAt::All, 4, 12, 11, 12, 0, 0, 0, 0, 0, 0},
    {Icon::Boots, -1, kAnyM, kT15, FpAt::All,   3, 1, 6, 1,    0, 0, 0, 0, 0, 0},
    {Icon::Boots, -1, kAnyM, kT15, FpAt::All,   6, 3, 9, 3,    0, 0, 0, 0, 0, 0},
    {Icon::Boots, -1, kHardM, kT15, FpAt::All,  3, 0, 6, 0,    0, 0, 0, 0, 0, 0},
    {Icon::Boots, -1, kHardM, kT15, FpAt::All,  8, 2, 9, 2,    0, 0, 0, 0, 0, 0},
    // cloak: the hem's inner row through T4 (a cloak's border keeps its thread longer: FIX_BATCH_P5 3.3), the hood's rim T1-T5
    {Icon::Cloak, -1, kAnyM, kT14, FpAt::AboveBottom, 0, 9, 15, 15, 0, 0, 0, 0, 0, 0},
    {Icon::Cloak, -1, kAnyM, kT14, FpAt::All,         6, 5, 10, 5,  0, 0, 0, 0, 0, 0},
    {Icon::Cloak, -1, kAnyM, 0x20, FpAt::All,         5, 5, 11, 5,  0, 0, 0, 0, 0, 0},   // T5: the whole rim (no hem)
};
// where the glyph sits: the 4x4 on an open chest, the 3x3 on the lit breast beside a laced or seamed front; on a cloak the
// clasp becomes the people's brooch at T5-T6 (gold)
struct FpChest { Icon icon; int8_t form; uint16_t mats; uint8_t tiers; int8_t x, y; bool small; };
const FpChest kFpChest[] = {
    // a leather gambeson's mark is its laced front at T1-T2 (a stitched glyph on its lit breast pulled D117's browns grey)
    {Icon::Armor, 1, kLeatherM, 0x78, 4, 6, true}, {Icon::Armor, 1, kHardM, 0x7E, 4, 6, true},
    {Icon::Armor, 2, kAnyM, 0x7E, 4, 6, true},     {Icon::Armor, 7, kAnyM, 0x7E, 4, 6, true},
    {Icon::Armor, 6, kAnyM, 0x78, 6, 6, false},    // a coat of plates: its seam at T1-T2, the engraved mark from T3
    {Icon::Armor, -1, kAnyM, 0x7E, 6, 6, false},
    {Icon::Cloak, -1, kAnyM, 0x60, 6, 6, true},
};

// the drawn step of a painted pixel: its index in the ramps it was painted from, else by its brightness
int fpStep(uint32_t v, std::initializer_list<const Ramp*> rs) {
  for (const Ramp* r : rs)
    for (int k = 0; k < 5; k++) if ((*r)[k] == v) return k;
  const float l = luma(v);
  return l < 0.22f ? 0 : l < 0.40f ? 1 : l < 0.62f ? 2 : l < 0.80f ? 3 : 4;
}
Ramp fpRamp(uint32_t rgb) {
  uint32_t k[5];
  doll::dyeRamp(rgba((int)(rgb >> 16 & 255), (int)(rgb >> 8 & 255), (int)(rgb & 255)), k);   // (P5 fix r2) the doll's own dye ramp
  return ramp5(k[0], k[1], k[2], k[3], k[4]);
}

// ---------------------------------------------------------------- P5b: the people's signature (docs/STYLE_P5B.md 7, 9.3 S5)
// The bag says what the doll says: a signed people's helm is its head class at its tier (section 4: the jingasa, the kabuto,
// the kuwagata; the knob cap, the transverse crest, the Attic crest; the fleece-browed cap, the spangenhelm on its aventail,
// the gilt horns), its shield the people's form in its dominant (T5-T6 gilt), its coat the colour landing (the lacing, the
// obi, the pteruges, the woad laces), its cloak the dominant. Hand-placed 16 px grids; the palette by character:
//   k ink, 0-4 the piece's metal, a-e the dominant, f-j the accent, p-t the leather, u-y the gilt, A-E the fleece, # the face
void paintIcon(Canvas& c, Icon ic, uint32_t tint, const Ramp* over);   // (below)
struct SigPal { const Ramp *M, *dom, *acc; };
void sigGrid(Canvas& c, const char* const* rows, int n, const SigPal& P) {
  static const uint32_t kFace = rgba(64, 44, 52);
  for (int y = 0; y < n && y < ICON; y++)
    for (int x = 0; x < ICON && rows[y][x]; x++) {
      const char ch = rows[y][x];
      uint32_t v = 0;
      if (ch == 'k') v = kInk;
      else if (ch == '#') v = kFace;
      else if (ch >= '0' && ch <= '4') v = (*P.M)[ch - '0'];
      else if (ch >= 'a' && ch <= 'e') v = (*P.dom)[ch - 'a'];
      else if (ch >= 'f' && ch <= 'j') v = (*P.acc)[ch - 'f'];
      else if (ch >= 'p' && ch <= 't') v = kLeather[ch - 'p'];
      else if (ch >= 'u' && ch <= 'y') v = kGold[ch - 'u'];
      else if (ch >= 'A' && ch <= 'E') v = kFurIcon[ch - 'A'];
      if (v) c.set(x, y, v);
    }
}
// the heads: [people 0 jade, 1 imperial, 2 fjordfolk][band 0 T1, 1 T2-T4, 2 T5-T6]
const char* const kSigHelm[3][3][16] = {
  {  // Jade: the jingasa (black lacquer, the vermilion mon); the kabuto, its shikoro laced in vermilion; the gilt kuwagata
    {"................", "................", "................", "................",
     ".......jh.......", "......jjhg......", ".....jihhgg.....", "....jiihhggg....",
     "...jiihcdhggg...", "..jiiihdehgggg..", ".jiiiihhhhggggg.", "jiiiiihhhhhggggf",
     ".ggggggggggfffff", "................", "................", "................"},
    {"................", "................", ".....3....3.....", "......3..3......",
     "......k44k......", ".....k44332k....", "....k4433221k...", "....k3333221k...",
     "...dcccccccbbb..", "..k4332222211k..", ".dccc######bbbb.", "k4332######2211k",
     "dccc.######.bbbb", "k32..######..21k", "................", "................"},
    {"..v..........v..", "..wv........vw..", "...xv......vx...", "....xwv..vwx....",
     "......yxxw......", ".....k44332k....", "....k4433221k...", "....k3333221k...",
     "...dcccccccbbb..", "..k4332222211k..", ".dccc######bbbb.", "k4332######2211k",
     "dccc.######.bbbb", "k32..######..21k", "................", "................"},
  },
  {  // Imperial: the bronze knob cap and its cheek guards; the crested helm, a transverse crimson crest; the Attic, its tall crest
    {"................", "................", ".......xw.......", "......kyxk......",
     ".....kyxxwk.....", "....kyxxwwvk....", "....kxxwwvvk....", "...kyxwwwwvvk...",
     "...kxwwwwvvvk...", "...kbccccccbk...", "...kxk####kvk...", "...kxk####kvk...",
     "...kwk####kvk...", "....k......k....", "................", "................"},
    {"................", "..c.d.e.d.c.b...", ".bdcedeedcdcbb..", ".bcdddedddccbb..",
     "..aabbbbbbbaa...", ".....k4433k.....", "....k443322k....", "....k433222k....",
     "...k43322211k...", "...kkxxxxxxkk...", "...k4k####k1k...", "...k3k####k1k...",
     "...k3k####k1k...", "....k......k....", "................", "................"},
    {"......cdec......", ".....cdeedcb....", ".....cdddccb....", "......cdccb.....",
     "......kyxk......", ".....k4433k.....", "....k443322k....", "....k433222k....",
     "...k43322211k...", "...kyxxxxxwwk...", "...kyk####kvk...", "...kxk####kvk...",
     "...kxk####kvk...", "....k......k....", "................", "................"},
  },
  {  // Fjordfolk: the leather cap, its fleece brow; the spangenhelm, its nasal and mail aventail; the gilt horns, the comb
    {"................", "................", "................", "................",
     "......ksrrk.....", ".....ksrrqqk....", "....ksrrrqqpk...", "....ksrrqqqpk...",
     "...ksrcrdqcqpk..", "..EDECEDCDCBCBA.", "..DCDCBCBCBABAA.", "...BABABAAAAAA..",
     "................", "................", "................", "................"},
    {"................", ".......k........", "......k4k.......", ".....k443k......",
     ".....k4432k.....", "....k443322k....", "....k443322k....", "...k44333221k...",
     "...kbccccccbk...", "..k31##4##13k...", "..k13##3##31k...", "..k31#####13k...",
     "..k3131313131k..", "...kkkkkkkkkk...", "................", "................"},
    {"................", ".x..............", "xy....4334....yx", "vx...k4433k...xv",
     ".vw.k443322k.wv.", "..vwk443322kwv..", "...k44333221k...", "...kbccccccbk...",
     "..k31##4##13k...", "..k13##3##31k...", "..k31#####13k...", "..k3131313131k..",
     "...kkkkkkkkkk...", "................", "................", "................"},
  },
};
// (P5b area A) the area's heads in the bag, as worn (STYLE_P5B 4 and 7): the Highland's bonnet (T1; T2 with the clan badge:
// a saffron brooch and the eagle feather), the kettle under its fur band and feather (T3; T4 the feather moved, a finial);
// the Heartland's chapel (T1: boiled leather, the hatband in heraldic red and white; T2-T6 the hero's road: the classic
// painter); the Fjordfolk's T4 spectacle helm (the owner's fix); the Imperial's T2 cap and its crimson tuft. Same palette
// characters as kSigHelm; nullptr: the slice's grid or the classic painter
const char* const kSigHelmHighland[4][16] = {
  {"................", "................", "................", ".......hj.......", "......kghk......", "...kkkdeddkkk...",
   "..kdeeddccccbbk.", ".kdeddcccccccbbk", "kdedccccccccbbak", ".kbbbbbbbbbbbak.", "..kiaiaiaiaiak..", "..kkkkkkkkkkkk..",
   "................", "................", "................", "................"},
  {"................", ".............e..", "............ed..", ".......hj...dc..", "......kghk..dc..", "...kkkdeddkkdck.",
   "..kdeeddccccbbk.", ".kdeddcccccccbbk", "kdedccccccccbbak", ".kbbbbbbbbbbbak.", "..kiaiaiaiijhak.", "..kkkkkkkkkkkk..",
   "................", "................", "................", "................"},
  {"..............e.", ".............ed.", "......kkkk..dc..", ".....k4433kdc...", "....k443322k....", "....k433222k....",
   "...kqrqsqrqsqk..", ".k443333322221k.", "..k1111111111k..", "...k3k####k1k...", "...k4k####k2k...", "...k3k####k1k...",
   "...k3kk##kk1k...", "...kkk....kkk...", "................", "................"},
  {".e..............", ".de.....kk......", "..dc..kk44kk....", "...cdk443322k...", "....k443322k....", "....k433222k....",
   "...kqrqsqrqsqk..", ".k443333322221k.", "..k1111111111k..", "...k3k####k1k...", "...k4k####k2k...", "...k3k####k1k...",
   "...k3kk##kk1k...", "...kkk....kkk...", "................", "................"},
};
const char* const kSigHelmChapel[16] = {
  "................", "................", "................", "................", "......kkkk......", ".....ksrrqk.....",
  "....ksrrrqqk....", "....ksrrqqqk....", "...kdidiidiidk..", "kssrrrrrqqqqqqpk", ".kppppppppppppk.", "................",
  "................", "................", "................", "................"};
const char* const kSigHelmSpectacle[16] = {
  "................", ".......kk.......", "......k43k......", ".....k4432k.....", "....k443322k....", "...k44433221k...",
  "...k44332211k...", "...k44433221k...", "..k31k#k3k#k11k.", "..k31kk43kk11k..", "..k313k##k313k..", "..k131313131313k",
  "...kkkkkkkkkkkk.", "................", "................", "................"};
const char* const kSigHelmTuftCap[16] = {   // (fix r1) the Montefortino: its tuft, the hinged cheek guards in the legion's brass
  "................", "................", ".......dc.......", "......kedk......", ".......kk.......", ".....ksrrk......",
  "....ksrrrqk.....", "...ksrrrqqqk....", "...ksrrqqqpk....", "..kbcbcbcbcbk...", "..kxkkkkkkkvk...", "..kxk.....kvk...",
  "..kwk.....kvk...", "...k.......k....", "................", "................"};
// (P5b fix r1) the slice peoples' T2 heads as worn (p5b_sig_fr1.py): the Jade great kasa (plaited straw, the vermilion binding
// and chin cords, a lacquer knob) and the Fjordfolk spangen cap (a leather cone on its riveted iron band)
const char* const kSigHelmKasa[16] = {
  "................", "................", "................", "................", ".......kk.......", "......kjfk......",
  ".....ksrrqk.....", "....ksrrrqqk....", "...ksrrqrqqpk...", "..ksrrrqqqqqpk..", ".kssrrqrqqqqppk.", "kdddddccccccbbbk",
  ".kkkkkkkkkkkkkk.", "...kbk....kbk...", "...kak....kak...", "................"};
const char* const kSigHelmSpangenCap[16] = {
  "................", "................", "................", ".......kk.......", "......ksrk......", ".....ksrrqk.....",
  "....ksrrrqqk....", "...ksrrrqqqpk...", "...ksrrqqqqpk...", "..kEDkEDkCBkBAk.", "..kDCkDCkBAkAAk.", "..kpk......kpk..",
  "..kpk......kpk..", "...k........k...", "................", "................"};
const char* const* sigHelmA(int people, int t) {
  const char* n = fp::row(people).name;
  if (!std::strcmp(n, "highland")) return t >= 1 && t <= 4 ? kSigHelmHighland[t - 1] : nullptr;
  if (!std::strcmp(n, "heartland")) return t == 1 ? kSigHelmChapel : nullptr;
  if (!std::strcmp(n, "fjordfolk") && t == 4) return kSigHelmSpectacle;
  if (!std::strcmp(n, "imperial") && t == 2) return kSigHelmTuftCap;
  if (!std::strcmp(n, "jade") && t == 2) return kSigHelmKasa;            // (fix r1)
  if (!std::strcmp(n, "fjordfolk") && t == 2) return kSigHelmSpangenCap;   // (fix r1)
  return nullptr;
}
bool sigAreaA(int people) { const char* n = fp::row(people).name; return !std::strcmp(n, "highland") || !std::strcmp(n, "heartland"); }
int sigIndex(int people) {   // the slice's three peoples (STYLE_P5B 9.1); -1 every other
  const char* n = fp::row(people).name;
  return !std::strcmp(n, "jade") ? 0 : !std::strcmp(n, "imperial") ? 1 : !std::strcmp(n, "fjordfolk") ? 2
       : !std::strcmp(n, "highland") ? 3 : !std::strcmp(n, "heartland") ? 4 : -1;   // (P5b area A: 3, 4)
}
const fp::Sig* sigOf(const IconLook& l) {
  if (rigVersion() < 2 || !l.people || !doll::fixOn(doll::FIX_SIG) || !doll::fixOn(doll::FIX_FP)) return nullptr;
  return fp::sig(l.people);
}
// (P5b fix r2, judge r2 N2: the three T5 cloaks were one hooded form in three colours, Jade and Fjordfolk 15 px apart in
// greyscale) a signed cloak is its people's own garment, as the worn one: the Jade's HORO from T3, a balloon of vermilion
// cloth hung from a lacquered yoke (the round silhouette, the gores' seams on the frame's ribs, the black mon), a plain
// cloak below T3; the
// Imperial PALUDAMENTUM, a crimson rectangle hung square from the shoulders and pinned on the right one by a gilt fibula,
// its folds falling from the pin, a half cape to the hip at T3-T4, the legate's cloak to the heel with its gilt hem from
// T5; the Fjordfolk CLOAK, woad under a fur collar (the mantle's roll), open down the front on its bleached-wool lining,
// a wool hem, an iron ring pin (gilt from T5), a cape to the hip at T3-T4 and to the heel from T5. The light from the top
// left, as every icon (ac: the accent ramp, the Jade's lacquer; M the piece's metal)
void sigCloakIcon(Canvas& c, int si, int t, const Ramp& dom, const Ramp& ac, const Ramp& M) {
  const bool tall = t >= 5;
  auto put = [&](int x, int y, uint32_t v) { if (x >= 0 && y >= 0 && x < ICON && y < ICON) c.set(x, y, v); };
  if (si == 0 && t >= 3) {   // the horo: a cape that fills with air, narrow at the lacquered yoke, a full belly, the ribs
    static const float kHalf[16] = {0, 0, 2.6f, 3.4f, 4.4f, 5.3f, 6.0f, 6.5f, 6.8f, 6.9f, 6.8f, 6.5f, 6.0f, 5.2f, 4.0f, 2.2f};
    for (int y = 2; y < ICON; y++)
      for (int x = 0; x < ICON; x++) {
        const float dx = x + 0.5f - 8.0f, hw = kHalf[y];
        if (std::fabs(dx) > hw) continue;
        const float u = dx / std::max(1.0f, hw), v = (y - 9.0f) / 7.0f;   // across the belly, down it
        int k = 2;
        if (u + v < -0.55f) k = 3;
        if (u + v < -1.05f) k = 4;
        if (u + v > 0.55f) k = 1;
        if (y >= 4 && y <= 14 && std::fabs(std::fabs(dx) - 0.5f * hw) < 0.5f) k = k >= 3 ? 1 : 0;   // the gores' seams on the ribs
        if (y == 3) k = std::max(1, k - 1);   // the shade under the yoke
        if (y == 15) k = std::max(0, k - 1);
        put(x, y, dom[k]);
      }
    for (int x = 4; x <= 11; x++) put(x, 2, ac[x <= 6 ? 3 : x >= 10 ? 1 : 2]);   // the yoke, lacquered
    put(4, 3, ac[2]); put(11, 3, ac[1]);                                         // its ends, hooked to the shoulders
    put(8, 6, ac[1]); put(7, 7, ac[1]); put(9, 7, ac[1]); put(8, 8, ac[1]);      // the mon: a black ring of four
    put(8, 7, dom[3]);
    return;
  }
  if (si == 1) {   // the paludamentum: square from the shoulders, flaring a little, pinned on the right shoulder, folds from the pin
    const int hem = tall ? 14 : 11;
    for (int y = 2; y <= hem; y++) {
      const int x0 = y <= 3 ? 3 : y >= hem - 2 ? 1 : 2, x1 = y <= 3 ? 12 : y >= hem - 2 ? 14 : 13;
      for (int x = x0; x <= x1; x++) {
        if (y <= 3 && x >= 6 && x <= 9 && !(y == 3 && (x == 6 || x == 9))) continue;   // the neck
        if (!tall && y > hem - (x - 1) / 4) continue;                                     // the half cape's hem rises to the left hip
        int k = x <= 4 ? 3 : x >= 11 ? 1 : 2;
        if (y == 2) k = 4;
        const int f = (x - 3) - (y - 3) / 2;   // the folds fall from the pin, down and to the right
        if (y >= 4 && (f == 3 || f == 7)) k = std::max(1, k - 1);
        if (y >= 4 && (f == 2 || f == 6)) k = std::min(4, k + 1);
        if (y == hem) k = std::max(0, k - 1);
        put(x, y, dom[k]);
      }
    }
    if (tall) for (int x = 1; x <= 14; x++) put(x, 14, kGold[x <= 4 ? 3 : x >= 11 ? 1 : 2]);   // the legate's gilt hem
    put(3, 3, kGold[4]); put(4, 3, kGold[3]); put(3, 4, kGold[2]); put(4, 4, kGold[1]);       // the fibula
    return;
  }
  if (si == 2) {   // the Fjordfolk cloak: woad under a fur collar, open on its wool lining, a ring pin
    const int hem = tall ? 14 : 11, top = tall ? 1 : 2;
    for (int y = 4; y <= hem; y++) {
      const int x0 = y >= hem - 3 ? 2 : 3, x1 = y >= hem - 3 ? 13 : 12;   // a little flare to the hem
      for (int x = x0; x <= x1; x++) {
        int k = x <= 4 ? 3 : x >= 11 ? 1 : 2;
        if ((x == 5 || x == 10) && y >= 6) k = std::max(1, k - 1);   // two long folds
        if (y == hem) k = std::max(0, k - 1);
        put(x, y, dom[k]);
      }
    }
    for (int y = 5; y <= hem; y++) { put(7, y, ac[3]); put(8, y, ac[1]); }   // the open front: the wool lining's edges
    for (int x = 2; x <= 13; x++) if (x < 7 || x > 8) put(x, hem - 1, ac[x <= 4 ? 3 : x >= 11 ? 1 : 2]);   // the wool hem
    for (int y = top; y <= 4; y++)   // the fur collar, lumpy along its top
      for (int x = 2; x <= 13; x++) {
        if (y == top && (x % 3 == 2 || x == 2 || x == 13)) continue;
        if (y == 4 && (x == 2 || x == 13)) continue;
        int k = x <= 5 ? 3 : x >= 11 ? 1 : 2;
        if (y == top) k++;
        if (y == 4) k--;
        if ((x + y) % 4 == 0) k--;   // the fleece's tufts
        put(x, y, kFurIcon[std::clamp(k, 0, 4)]);
      }
    const Ramp& pin = tall ? kGold : M;   // the ring pin on the right of the collar
    put(10, 4, pin[4]); put(11, 4, pin[2]); put(10, 5, pin[2]); put(11, 5, pin[1]); put(11, 6, pin[3]);
    return;
  }
  paintIcon(c, Icon::Cloak, dom[2], nullptr);
}
// (P5b fix r2, judge r2 N2: the three T1 body icons were one brown jerkin with coloured lace dots, 11-17 grey-distinct px
// apart, while the worn T1 bodies wear the layer) a signed coat carries what the worn coat shows, in place of P5's thread
// marks and inside FIX_BATCH_P5 3.5's share (a leather coat's changed px at most a tenth of it, the outline ring counted: so
// the layer keeps off the silhouette's edge, where a changed pixel recolours the ring beside it), each pixel parted from the
// coat under it by value so it reads in greyscale on any form and material: the Jade OBI through T4 (two rows of vermilion
// round the waist bound by a black lacquered cord, its knotted ends on the right hip), the Imperial FOCALE at T1-T2 (the
// crimson scarf under the throat and up both sides of the neck, knotted on the chest; the apron's straps studded in gilt at
// the hem: the people's pair, and the gold keeps the coat's mean a brown), and the Fjordfolk
// jerkin's SPLIT HEM at T1-T2 (the ancestor of the hirdman's split mail skirt, STYLE_P5B 5: the hem turned back in dark
// hide over a split at the front) laced in woad. The brown stays the coat's (D117). Returns true when it drew the layer
// (the caller then skips P5's marks)
bool sigBodyLayer(Canvas& c, const IconLook& l) {
  const fp::Sig* g = sigOf(l);
  const int si = sigIndex(l.people);
  if (!g || si < 0 || l.icon != Icon::Armor || l.tier < 1) return false;
  const int t = std::clamp((int)l.tier, 1, 6);
  const fp::SigTier& st = g->t[t];
  const Ramp dom = fpRamp(g->dom);
  static const Ramp kLacquer = ramp5(rgba(14, 12, 16), rgba(28, 26, 30), rgba(44, 40, 48), rgba(74, 68, 80), rgba(138, 132, 148));
  auto op = [&](int x, int y) { return x >= 0 && y >= 0 && x < ICON && y < ICON && (c.get(x, y) >> 24) != 0; };
  // the layer's step k, or the nearest step (shade first) that parts from the coat under it by 30 luma
  auto lay = [&](int x, int y, const Ramp& R, int k) {
    if (!op(x, y)) return;
    const float b = luma(c.get(x, y));
    int best = k;
    if (std::fabs(luma(R[k]) - b) < 0.12f)
      for (int d : {-1, 1, -2, 2}) {
        const int q = k + d;
        if (q >= 0 && q <= 4 && std::fabs(luma(R[q]) - b) >= 0.12f) { best = q; break; }
      }
    c.set(x, y, R[best]);
  };
  if (si == 3 && st.layer && !std::strcmp(st.layer, "sash")) {   // (P5b area A) the Highland plaid over the left shoulder
    // a band from the figure's left shoulder (the icon's right) down to the right hip, off the outline ring and inside FIX_BATCH_P5
    // 3.5's tenth of the coat (icons_gallery --check): one pixel a row, two where it crosses the chest, the sett's saffron thread
    for (int y = 4; y <= 11; y++) {
      const int x = 10 - (y - 4) * 5 / 7;
      lay(x, y, dom, y <= 6 ? 3 : 2);
      if (y == 6 || y == 8) lay(x - 1, y, dom, 1);
      if (y == 5 || y == 9) lay(x, y, fpRamp(g->acc), 3);
    }
    return true;
  }
  if (st.layer && !std::strcmp(st.layer, "sash")) {   // the obi
    for (int x = 5; x <= 10; x++) {
      lay(x, 10, dom, x <= 6 ? 3 : 2);
      lay(x, 11, dom, x <= 6 ? 2 : 1);
    }
    lay(9, 10, kLacquer, 1); lay(9, 11, kLacquer, 2); lay(9, 12, kLacquer, 1);   // the black lacquered cord that binds it
    lay(10, 12, dom, 2); lay(10, 13, dom, 1);           // the knotted ends
    return true;
  }
  if (st.layer && !std::strcmp(st.layer, "scarf")) {   // the focale, and the apron's gilt studs
    lay(5, 4, dom, 3); lay(10, 4, dom, 1);
    for (int x = 5; x <= 10; x++) lay(x, 5, dom, x <= 6 ? 3 : x >= 9 ? 1 : 2);
    lay(7, 6, dom, 2); lay(8, 6, dom, 1);
    const Ramp acc = fpRamp(g->acc);
    for (int x : {6, 9}) lay(x, 13, acc, x <= 6 ? 3 : 2);
    return true;
  }
  if (t <= 2 && g->t[3].skirt && !std::strcmp(g->t[3].skirt, "hauberk")) {   // the split hem, the woad laces
    for (int x = 5; x <= 10; x++) lay(x, 12, kLeather, x <= 6 ? 1 : 0);
    for (int y = 13; y <= 14; y++) { c.set(7, y, 0); c.set(8, y, 0); }
    lay(8, 7, dom, 3); lay(7, 8, dom, 2); lay(8, 9, dom, 3);
    return true;
  }
  return false;
}
// ---------------------------------------------------------------- P5b area C: River, Sun Temple, Sylvan, Starspire
// The bag says what the doll says (STYLE_P5B 7): the helm by its head class at its band (the chaperon, the chapel, the plumed
// morion, its gilt legend; the feathered band, the fanned cap, the jaguar, the sun crown over the hood; the leaf circlet, the leaf
// crown, the leaf-winged helm, its gilt veins; the deep hood, the pointed cowl and its clasp, the star crown over the hood, its
// gilt spires), the shield its people's form, field and device (River per pale lapis and linen, the Sun Temple chimalli ringed
// and fringed in yellow, the Sylvan leaf veined in forest, the Starspire kite under a white star), the cloak the dominant
int sigIndexC(int people) {
  const char* n = fp::row(people).name;
  return !std::strcmp(n, "river") ? 0 : !std::strcmp(n, "suntemple") ? 1 : !std::strcmp(n, "sylvan") ? 2 : !std::strcmp(n, "starspire") ? 3 : -1;
}
// [river, suntemple, sylvan, starspire][band 0 T1, 1 T2, 2 T3-T4, 3 T5, 4 T6] (area C; P5b production 2: T5 and T6 their own
// heads, tools/doll/p5b_sig_C2.py: the plumed captain's morion and its grand plume; the jaguar under the sunburst and the
// eagle under the full fan; the leaf-crested open helm and its spray; the gothic ogive under its star and the star arch)
const char* const kSigHelmC[4][5][16] = {
  {  // river
    {"................", "................", "......kkkk......", "....kkjjiikk....", "...kjjjiihhhgk..", "..kjjiiihhhgggk.", "..kiiiihhhgggffk", "..keeddcccbbaak.", "..kcccbbbaaaaak.", "...kkkkkkkkkkgk.", ".............kgk", "............kgfk", "............kfk.", ".............k..", "................", "................"},
    {"................", "................", "................", "......kkkk......", ".....k4433k.....", "....k443322k....", "....k433221k....", "....kddccbak....", ".kkkk3322111kkk.", "k443333222211110", ".kkkkkkkkkkkkkk.", "................", "................", "................", "................", "................"},
    {"........kk......", ".......kdck.kk..", "......kedcbkck..", "......kdccbabk..", ".......kk4kkk...", "......k443k.....", ".....k44332k....", "....k4433221k...", "...k443322211k..", "k4kk433222111kk1", ".k443332222111k.", "..kkkkkkkkkkkk..", "................", "................", "................", "................"},
    {"........kjjk....", "......kkjiihk...", ".....kxwk.khhk..", ".....k4k....kgk.", "....k432k...kgk.", "...k44322k..kgk.", "..k4433221k..k..", ".k443332211k....", "kk44333222111kk.", "kyxxxwwwwvvvvuuk", ".kkk########kkk.", "....k######k....", ".....kkkkkk.....", "................", "................", "................"},
    {"......kjjjjk....", "....kkjiiiihk...", "...kcdk.kkhhhk..", "...kdk.....khgk.", "...kk4k.....kgk.", "....k432k...kgfk", "...k44322k..kgfk", "..k4433221k..kfk", ".k443e332211k.k.", "kk44333222111kk.", "kyxxxwwwwvvvvuuk", ".kkk########kkk.", "....k######k....", ".....kkkkkk.....", "................", "................"},
  },
  {  // suntemple
    {"................", "..........kk....", ".........kjik...", ".........kiik...", ".........khhk...", "..........kk....", "..kkkkkkkkkkkk..", ".kdeeddddcccbbk.", ".kccbbbbbbbaaak.", "..kkkkkkkkkkkk..", "................", "................", "................", "................", "................", "................"},
    {".......kk.......", "..kk..kjik..kk..", "..kik.kiik.kihk.", "...khk.kik.khk..", "....kk.kk.kk....", ".....kkkkkkk....", "....kdeeddcck...", "...kdedcdcdcbk..", "..kdeddcddcdcbk.", "..kddcddcddcbbak", "..kccccccccbbaak", "...kkkkkkkkkkkk.", "................", "................", "................", "................"},
    {"................", "...kk......kk...", "..kjhk....khgk..", "..khjkkkkkkhgk..", ".kjjhhhhhhhggfk.", ".kjhkhhhkhhgkfk.", ".khhdkhhhhkdgfk.", ".khhhhiikhhggfk.", ".khgk4kkkk4kgfk.", ".kgk########kfk.", ".kgk########kfk.", ".khgk######kgfk.", "..kggkkkkkkgfk..", "...kkkkkkkkkk...", "................", "................"},
    {"j.j.j.i.i.h.h.h.", "kddcdcdcdccbcbbk", "..kjhk....khgk..", "..khjkkkkkkhgk..", ".kjjhhhhhhhggfk.", ".kjhkhhhkhhgkfk.", ".khhdkhhhhkdgfk.", ".khhhhiikhhggfk.", ".khgk4kkkk4kgfk.", ".kgk########kfk.", ".kgk########kfk.", ".khgk######kgfk.", "..kggkkkkkkgfk..", "...kkkkkkkkkk...", "................", "................"},
    {"j.j.j.j.i.i.h.h.", "kddcdcdcdcdcbcbk", "kjiijiiihihhghgk", "...kddddcccbk...", "..kdedddccccbk..", ".kdekjddcckjcbk.", ".kddkkjihhkkcbk.", ".kddkjiihhgkcbk.", ".kdk.kjihgk.kbk.", ".kdk##kggk##kbk.", ".kdk########kbk.", ".kdk########kbk.", "..kdk######kbk..", "...kjjiihhggk...", "....kkkkkkkk....", "................"},
  },
  {  // sylvan
    {"................", "................", "................", "................", "..k...k...k...k.", ".kdk.kdk.kdk.kck", ".kcdkkcdkkcbkkbk", ".kiihhhhgggfffk.", "..kkkkkkkkkkkk..", "................", "................", "................", "................", "................", "................", "................"},
    {"................", ".......kk.......", "...k...kdk...k..", "..kdk.kddk..kck.", "..kdckkdcck.kcbk", ".kddckdccbkkcbbk", ".kiihhhhgggfffk.", "..kkkkkkkkkkkk..", "................", "................", "................", "................", "................", "................", "................", "................"},
    {"................", "k..............k", "kdk..........kbk", ".kdk..kkkk..kbk.", ".kddkk4433kkbbk.", "..kdk443322kbk..", "..kk44332221kk..", "...k43322211k...", "...k33222111k...", "...kkkkkkkkkk...", "...k1k....k1k...", "................", "................", "................", "................", "................"},
    {"......kddk......", ".....kdecbk.....", "......kdck......", "k.....kkkk.....k", "kdk..k4433k..kbk", ".kdkk443322kkbk.", ".kddk4332221kbbk", "..kk43322211kk..", "...kxxwwwvvvk...", "...kfj####hgk...", "...kfj####hgk...", "...kfgj##hggk...", "....kffgggfk....", ".....kkkkkk.....", "................", "................"},
    {"..kk..kddk..kk..", ".kdck.kdecbk.kck", "..kdkkkdckkkbk..", "k...kkkkkkkk...k", "kdk..k4433k..kbk", ".kdkk443322kkbk.", ".kddk4332221kbbk", "..kk43322211kk..", "...kyxxwwwvuk...", "...kfj#yy#hgk...", "...kfj####hgk...", "...kfgj##hggk...", "....kffgggfk....", ".....kkkkkk.....", "................", "................"},
  },
  {  // starspire
    {"................", ".......kk.......", "......kddk......", ".....kdeddk.....", "....kdeedcck....", "...kdeddccccbk..", "...kjjjiiihhk...", "..kdj######hcbk.", "..kdj######hcbk.", "..kdj######hcbk.", "..kddj####hccbk.", ".kdddcjjjjccbbak", ".kddcccccccbbaak", "..kkkkkkkkkkkkk.", "................", "................"},
    {".......kk.......", "......kddk......", "......kedk......", ".....kdedck.....", "....kdedccck....", "...kdeddcccbk...", "...kjjjiiihhk...", "..kdj######hcbk.", "..kdj######hcbk.", "..kddj####hccbk.", ".kdddcckjkccbbak", ".kddcckjjjkcbaak", "..kkkkkkjkkkkkk.", "................", "................", "................"},
    {"................", "...k..k.k..k....", "..k4k.k4k.k3k...", "..k43kk43kk32k..", "..k443333222k...", "..kkkkkkkkkkk...", "..kdj######hck..", "..kdj######hck..", "..kddj####hcck..", ".kdddcjjjjccbbk.", ".kddcccccccbbak.", "..kkkkkkkkkkkk..", "................", "................", "................", "................"},
    {"......kjk.......", ".....kjjik......", "......kik.......", "......k4k.......", ".....k432k......", "....k44322k.....", "...k4434321k....", "..k443433221k...", "..kyyxxxwwvuk...", "..kdj######hck..", "..kdj######hck..", "..kddj####hcck..", ".kdddcjjjjccbbk.", ".kddcccccccbbak.", "..kkkkkkkkkkkk..", "................"},
    {"j.....kjk.....j.", ".....kjjik......", ".j....kik....j..", "......k4k.......", "j....k432k....j.", "....k44322k.....", "...k4434321k....", "..k443433221k...", "..kyyxxxwwvuk...", "..kdj######hck..", "..kdj######hck..", "..kddj####hcck..", ".kdddcjjjjccbbk.", ".kddcccccccbbak.", "..kkkkkkkkkkkk..", "................"},
  },
};
bool sigIconPieceC(Canvas& c, const IconLook& l, const Ramp& M) {
  const fp::Sig* g = sigOf(l);
  const int si = sigIndexC(l.people);
  if (!g || si < 0) return false;
  const Ramp dom = fpRamp(g->dom), acc = fpRamp(g->acc);
  const int t = std::clamp((int)l.tier, 1, 6);
  const int band = t <= 1 ? 0 : t == 2 ? 1 : t <= 4 ? 2 : t == 5 ? 3 : 4;
  auto stepOf = [&](uint32_t v, const Ramp& R) { for (int i = 0; i < 5; i++) if (v == R[i]) return i; return -1; };
  switch (l.icon) {
    case Icon::Helmet: {
      const SigPal P{&M, &dom, &acc};
      sigGrid(c, kSigHelmC[si][band], 16, P);
      return true;
    }
    case Icon::Shield: {
      const int form = fp::sigShieldForm(g, l.form);   // (fix r1)
      const bool gilt = t >= 5;
      shieldIcon(c, form, gilt ? kGold : t <= 1 ? dom : (l.mat == 1 ? kLeather : M), t <= 1 ? kLeather : dom, gilt);
      if (t < 2) return true;
      for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
          const uint32_t v = c.get(x, y);
          const int k = (v >> 24) ? stepOf(v, dom) : -1;
          if (k < 0) continue;
          if (si == 0 && x >= 8) c.set(x, y, acc[std::min(4, k + 1)]);                           // River: per pale, lapis on the sinister
          if (si == 1) {                                                                        // Sun Temple: a yellow ring, fringed
            const float d = std::sqrt((x + 0.5f - 8) * (x + 0.5f - 8) + (y + 0.5f - 8) * (y + 0.5f - 8));
            if (std::fabs(d - 3.6f) < 0.6f || (y >= 12 && (x & 1))) c.set(x, y, acc[std::min(4, k + 1)]);
          }
          if (si == 2 && (x == 7 || x == 8) && y >= 3 && y <= 13) c.set(x, y, acc[x == 7 ? 3 : 2]);   // Sylvan: the leaf's forest vein
        }
      if (si == 3) {   // Starspire: a four-point star in star white on the upper field
        const int sx = 8, sy = 5;
        for (int d = -2; d <= 2; d++) { c.set(sx + d, sy, acc[d == 0 ? 4 : 3]); c.set(sx, sy + d, acc[d == 0 ? 4 : 3]); }
      }
      return true;
    }
    case Icon::Cloak:
      paintIcon(c, Icon::Cloak, dom[2], nullptr);
      if (si == 3) { c.set(8, 7, acc[4]); c.set(7, 7, acc[3]); c.set(9, 7, acc[3]); c.set(8, 6, acc[3]); c.set(8, 8, acc[3]); }   // the star
      if (si == 1) for (int x = 4; x <= 11; x += 2) c.set(x, 13, acc[3]);                       // the feathered hem
      return true;
    default: return false;
  }
}

// a signed helm, shield, coat or cloak painted whole (true); false: the classic painter
bool sigIconPiece(Canvas& c, const IconLook& l, const Ramp& M) {
  const fp::Sig* g = sigOf(l);
  if (sigIconPieceC(c, l, M)) return true;   // (P5b area C)
  const int si = sigIndex(l.people);
  if (!g || si < 0) return false;
  const Ramp dom = fpRamp(g->dom), acc = fpRamp(g->acc);
  const int t = std::clamp((int)l.tier, 1, 6);
  const int band = t <= 1 ? 0 : t <= 4 ? 1 : 2;
  static const Ramp kLacquer = ramp5(rgba(14, 12, 16), rgba(28, 26, 30), rgba(44, 40, 48), rgba(74, 68, 80), rgba(138, 132, 148));
  switch (l.icon) {
    case Icon::Helmet: {
      const SigPal P{&M, &dom, si == 0 ? &kLacquer : &acc};
      if (const char* const* a = sigHelmA(l.people, t)) { sigGrid(c, a, 16, P); return true; }   // (P5b area A)
      if (si > 2) return false;   // (P5b area A) a tier the people leaves to the classic painter (the hero's road, T5-T6)
      sigGrid(c, kSigHelm[si][band], 16, P);
      return true;
    }
    case Icon::Shield: {
      if (!g->cloth) return false;   // (P5b area A) the Heartland's shield keeps P5's painter
      const int form = fp::sigShieldForm(g, l.form);   // (fix r1)
      const bool gilt = t >= 5;
      // T1 plain boards bound in the dominant; T2-T4 the field in the dominant, the rim the piece's own; T5-T6 a gilt rim
      shieldIcon(c, form, gilt ? kGold : t <= 1 ? dom : (l.mat == 1 ? kLeather : M), t <= 1 ? kLeather : dom, gilt);
      if (si == 2 && t >= 2 && t <= 4)   // the Fjordfolk's quartered round: the wool in the upper-left and lower-right quarters
        for (int y = 2; y < 14; y++)
          for (int x = 2; x < 14; x++) {
            const uint32_t v = c.get(x, y);
            if (!(v >> 24)) continue;
            const bool q = (x < 8) == (y < 8);
            int k = -1;
            for (int i = 0; i < 5; i++) if (v == dom[i]) k = i;
            if (q && k >= 0) c.set(x, y, acc[std::min(4, k + 1)]);
          }
      if (si == 3 && t >= 2)   // (P5b area A) the Highland targe's studs round its boss: saffron at T2, brass from T3
        for (const auto& q : {std::pair<int, int>{8, 3}, {4, 7}, {12, 8}, {8, 12}, {5, 4}, {11, 4}, {5, 11}, {11, 11}})
          if ((c.get(q.first, q.second) >> 24) != 0) c.set(q.first, q.second, t >= 3 ? kGold[3] : acc[3]);
      if (si == 1 && t >= 2)   // the scutum's device: a gilt thunderbolt down its middle
        for (int y = 4; y <= 11; y++) c.set(8 + ((y & 2) ? 1 : -1) * ((y % 4) == 1), y, kGold[y < 8 ? 4 : 3]);
      return true;
    }
    case Icon::Cloak:
      if (!g->cloth) return false;   // (P5b area A) the Heartland's cloak keeps P5's painter
      sigCloakIcon(c, si, t, dom, si == 0 ? kLacquer : acc, M);
      return true;
    default: return false;
  }
}

// (P5b production, area B) Dune, Steppe and Marsh: their own grids and painters, beside the slice's
#include "rpg/art/sig_icons_B.inc"

void iconFingerprint(Canvas& c, const IconLook& l, const Ramp& M, const Ramp& acc) {
  const fp::Row& row = fp::row(l.people);
  if (!row.period || l.tier < 1 || l.tier > 6) return;
  const uint8_t tbit = (uint8_t)(1u << l.tier);
  // (P5b) a signed people's band in its dominant and accent (the doll's dyeRamps reads the same pair)
  const fp::Sig* sg = sigOf(l);
  if (sg && !sg->cloth) sg = nullptr;   // (P5b area A) a people whose cloth keeps P5's (the Heartland) keeps P5's band
  const uint32_t dy = sg ? sg->dom : row.dye, dy2 = sg ? sg->acc : row.dye2;
  const Ramp D = fpRamp(dy), D2 = dy2 ? fpRamp(dy2) : D;
  auto band = [&](int x, int y) -> const Ramp& { return ((x + y) % row.period) < row.run ? D : D2; };
  const Canvas src = c;
  auto op = [&](int x, int y) { return x >= 0 && y >= 0 && x < ICON && y < ICON && (src.get(x, y) >> 24) != 0; };
  auto edge = [&](int x, int y) { return op(x, y) && (!op(x - 1, y) || !op(x + 1, y) || !op(x, y - 1) || !op(x, y + 1)); };
  const bool leatherPiece = l.mat == 1;   // Mat::Leather
  auto inM = [&](uint32_t v) { for (int k = 0; k < 5; k++) if (M[k] == v) return true; return false; };
  auto dye = [&](int x, int y, const Ramp& R) {
    // the drawn step carries (shadow 1, else the dye's own 2: the thread keeps its hue on a lit rim), never a glint; on
    // leather the dye's own step, a dark thread on a boot's lit cuff (the hide keeps its browns: D117)
    const int k = leatherPiece ? (l.icon == Icon::Boots ? 1 : 2) : std::clamp(fpStep(src.get(x, y), {&M, &acc, &kLeather}), 1, 2);
    c.set(x, y, R[k]);
  };
  for (const FpMark& m : kFpMarks) {
    if (m.icon != l.icon || (m.form >= 0 && m.form != l.form) || !(m.tiers & tbit) || (m.mats && !(m.mats & (1u << l.mat)))) continue;
    for (int x = m.x0; x <= m.x1; x++) {
      int lowest = -1;
      for (int y = m.y0; y <= m.y1; y++) {
        if (!op(x, y)) continue;
        if (m.xm > 1 && x % m.xm != m.xr) continue;
        if (m.ym > 1 && y % m.ym != m.yr) continue;
        if (m.dm > 1 && (x + y) % m.dm >= m.dr) continue;
        bool hit = true;
        switch (m.at) {
          case FpAt::All: break;
          case FpAt::Edge: hit = edge(x, y); break;
          case FpAt::AboveBottom: hit = false; lowest = y; break;
        }
        if (hit) dye(x, y, band(x, y));
      }
      if (m.at == FpAt::AboveBottom && lowest > m.y0 && op(x, lowest - 1)) dye(x, lowest - 1, band(x, lowest - 1));   // a hem's inner row
    }
  }
  // the glyph: thread at T1-T2 on leather (a dark thread, so the hide keeps its browns: D117) and cloth (one step over the
  // plate), Ink relief one step under the plate on metal and at T3-T4 (engraved), gold at T5-T6 (the filigree's language:
  // the lit point top-left)
  const FpGlyph* g = fpGlyph(row.motif);
  if (!g) return;
  const bool metal = l.mat != 1 && l.mat != 6 && !(l.icon == Icon::Armor && l.form == 1);   // Mat::Leather, Mat::Cloth, a padded coat
  for (const FpChest& ch : kFpChest) {
    if (ch.icon != l.icon || (ch.form >= 0 && ch.form != l.form) || (ch.mats && !(ch.mats & (1u << l.mat)))) continue;
    if (!(ch.tiers & tbit)) break;   // the first row for the piece decides
    const char* cells = ch.small ? g->g3 : g->g4;
    const int n = ch.small ? 3 : 4;
    bool first = true;
    for (int j = 0; j < n; j++)
      for (int i = 0; i < n; i++) {
        const char cc = cells[j * n + i];
        const int x = ch.x + i, y = ch.y + j;
        if (cc == '.' || !op(x, y)) continue;
        const uint32_t v = src.get(x, y);
        const int k = fpStep(v, {&M, &acc, &kLeather});
        if (l.tier >= 5) c.set(x, y, kGold[first ? 4 : 3]);
        else if (l.tier >= 3 || metal) c.set(x, y, inM(v) ? M[std::max(0, k - 1)] : mix(v, kInk, 0.35f));
        else c.set(x, y, (cc == 'b' ? D2 : D)[leatherPiece ? 1 : std::clamp(k + 1, 2, 3)]);
        first = false;
      }
    break;
  }
}

// M7 Home: the farm's goods and tools (tint: the crop's or the product's colour; M: the tinted ramp when tinted)
void paintFarmIcon(Canvas& c, Icon ic, bool tinted, const Ramp& M) {
  switch (ic) {
    case Icon::Fish: {   // a fish side-on, nose left: the back dark, the belly pale, a forked tail, a fin and an eye
      const Ramp F = tinted ? M : ramp5(rgba(40, 56, 80), rgba(70, 98, 124), rgba(112, 146, 166), rgba(166, 196, 206), rgba(226, 236, 236));
      ball(c, 7.0f, 8.5f, 5.6f, 3.2f, F, 0.06f);
      poly(c, {{11.0f, 8.5f}, {15.0f, 4.5f}, {15.0f, 12.5f}}, F[2]);   // the tail
      poly(c, {{11.0f, 8.5f}, {15.0f, 4.5f}, {13.6f, 8.5f}}, F[3]);
      c.set(15, 4, F[1]); c.set(15, 13, F[0]); c.set(14, 8, F[1]); c.set(14, 9, F[1]);
      for (int x = 4; x <= 9; x++) c.set(x, 5, F[x < 6 ? 2 : 1]);   // the dark back
      for (int x = 3; x <= 10; x++) c.set(x, 11, F[4]);             // the pale belly
      poly(c, {{6.0f, 5.5f}, {9.5f, 3.0f}, {10.0f, 5.5f}}, F[1]);    // the dorsal fin
      c.set(7, 10, F[1]); c.set(8, 11, F[1]);                         // the pectoral fin
      c.set(3, 7, kInk); c.set(3, 8, F[4]);                           // the eye
      c.set(2, 9, F[0]); c.set(5, 7, F[0]); c.set(5, 9, F[0]);        // the mouth, the gill
      if (tinted) { for (int x = 5; x <= 10; x += 2) c.set(x, 8, F[0]); }   // grill marks on a cooked fish
      break;
    }
    case Icon::Pie: {   // a round pie in its tin from above-front: a domed lattice crust, steam
      const Ramp Cr = ramp5(rgba(110, 58, 30), rgba(170, 100, 46), rgba(214, 148, 70), rgba(238, 190, 108), rgba(252, 228, 164));
      const Ramp& Fl = tinted ? M : ramp5(rgba(80, 26, 30), rgba(128, 44, 40), rgba(170, 70, 52), rgba(204, 104, 70), rgba(230, 150, 110));
      for (int x = 1; x <= 14; x++) for (int y = 11; y <= 13; y++) c.set(x, y, kIron[y == 11 ? 3 : (x < 4 ? 2 : 1)]);   // the tin
      ball(c, 7.5f, 9.5f, 6.6f, 3.6f, Cr, 0.05f);
      for (int k = 0; k < 4; k++) for (int y = 7; y <= 11; y++) { const int x = 3 + k * 3 + (y - 7) / 2; if (x < 14) c.set(x, y, Cr[4]); }   // the lattice
      for (int x = 3; x <= 12; x++) c.set(x, 9, Cr[3]);
      c.set(5, 8, Fl[2]); c.set(8, 10, Fl[2]); c.set(11, 8, Fl[3]); c.set(6, 10, Fl[1]);   // the filling between the strips
      c.set(6, 4, kCloth[4]); c.set(7, 3, kCloth[3]); c.set(9, 4, kCloth[4]); c.set(10, 3, kCloth[3]);   // steam
      break;
    }
    case Icon::Roast: {   // a roast on a platter: the browned joint, a crisp highlight, herbs beside it
      const Ramp R = tinted ? M : ramp5(rgba(76, 30, 24), rgba(122, 56, 32), rgba(166, 90, 44), rgba(204, 130, 66), rgba(238, 182, 110));
      for (int y = 11; y <= 13; y++) for (int x = 1; x <= 14; x++) {   // the platter (an oval rim)
        const float dx = (x + 0.5f - 8.0f) / 7.0f, dy = (y + 0.5f - 12.0f) / 2.2f;
        if (dx * dx + dy * dy <= 1.0f) c.set(x, y, kStoneWarm[y == 11 ? 4 : (y == 13 ? 2 : 3)]);
      }
      ball(c, 7.5f, 8.5f, 5.2f, 3.8f, R, 0.06f);
      for (int x = 5; x <= 9; x++) c.set(x, 6, R[4]);
      c.set(4, 7, R[4]); c.set(6, 9, R[1]); c.set(9, 10, R[0]); c.set(10, 8, R[1]);
      capsule(c, V(11, 7), V(14, 5), 1.0f, 0.8f, kBone);   // the bone end
      c.set(2, 10, kLeaf[3]); c.set(3, 11, kLeaf[2]); c.set(13, 10, kLeaf[3]); c.set(12, 11, kLeaf[2]);   // the herbs
      break;
    }
    case Icon::Cake: {   // a round cake: two sponge layers, cream between, icing running over the top, a berry
      const Ramp Sp = ramp5(rgba(120, 72, 38), rgba(178, 120, 60), rgba(218, 166, 92), rgba(240, 204, 132), rgba(252, 236, 186));
      const Ramp& Ic = tinted ? M : ramp5(rgba(150, 120, 96), rgba(200, 170, 140), rgba(232, 208, 180), rgba(246, 234, 214), rgba(255, 252, 244));
      for (int y = 7; y <= 13; y++) for (int x = 2; x <= 13; x++) c.set(x, y, Sp[x < 4 ? 3 : (x > 11 ? 1 : 2)]);
      for (int x = 2; x <= 13; x++) { c.set(x, 10, Ic[3]); c.set(x, 13, Sp[0]); }
      ball(c, 7.5f, 7.0f, 6.0f, 2.2f, Ic, 0.04f);   // the iced top
      c.set(3, 8, Ic[3]); c.set(6, 8, Ic[2]); c.set(6, 9, Ic[2]); c.set(10, 8, Ic[2]); c.set(12, 8, Ic[3]); c.set(12, 9, Ic[2]);   // drips
      ball(c, 8.0f, 5.0f, 1.3f, 1.2f, kRed);   // the berry
      c.set(8, 3, kLeaf[3]);
      break;
    }
    case Icon::Seeds: {   // a tied cloth pouch, seeds spilling from its mouth
      const Ramp Sd = tinted ? M : ramp5(rgba(96, 62, 34), rgba(150, 102, 50), rgba(196, 146, 72), rgba(226, 190, 112), rgba(246, 226, 166));
      ball(c, 7.5f, 10, 5.0f, 4.6f, kCloth);
      for (int x = 5; x <= 10; x++) c.set(x, 5, kCloth[x < 7 ? 4 : 2]);
      for (int x = 4; x <= 11; x++) c.set(x, 4, kCloth[x < 6 ? 3 : 1]);
      c.set(6, 3, kCloth[3]); c.set(9, 3, kCloth[2]); c.set(7, 2, kCloth[4]); c.set(8, 2, kCloth[3]);
      for (int x = 5; x <= 10; x++) c.set(x, 6, kLeather[x < 8 ? 3 : 1]);   // the drawstring
      c.set(4, 9, kCloth[4]); c.set(5, 13, kCloth[1]);
      static const int sp[5][2] = {{11, 13}, {13, 12}, {12, 14}, {14, 14}, {10, 14}};
      for (auto& q : sp) { c.set(q[0], q[1], Sd[3]); c.set(q[0] + 1, q[1], Sd[1]); }
      c.set(7, 9, Sd[2]); c.set(8, 10, Sd[3]);   // a seed printed on the cloth
      break;
    }
    case Icon::Sheaf: {   // a sheaf of grain: stalks bound at the waist, the ears fanning out on top
      const Ramp& G = tinted ? M : kGold;
      for (int i = -3; i <= 3; i++) {
        const int top = 5 - (i == 0 ? 1 : 0), bx = 8 + i / 2;
        for (int y = top; y <= 14; y++) {
          const float t = (float)(y - top) / (14 - top);
          const int x = (int)std::lround(8 + i * (1.0f - t) * 1.3f + i * 0.4f * (y > 10 ? (y - 10) * 0.3f : 0));
          c.set(x, y, G[(i < 0) ? 3 : (i == 0 ? 2 : 1)]);
        }
        (void)bx;
        // the ear
        const int ex = 8 + (int)std::lround(i * 1.3f), ey = top;
        c.set(ex, ey - 1, G[4]); c.set(ex, ey - 2, G[3]); c.set(ex + (i < 0 ? -1 : 1), ey - 3, G[3]); c.set(ex, ey, G[2]);
        if (i != 0) c.set(ex + (i < 0 ? -1 : 1), ey - 1, G[2]);
      }
      for (int x = 5; x <= 11; x++) { c.set(x, 10, kLeather[x < 8 ? 3 : 2]); c.set(x, 11, kLeather[1]); }   // the band
      break;
    }
    case Icon::Veg: {   // a root (a turnip by default): the round bulb, its tail, a tuft of leaves
      const Ramp Vg = tinted ? M : ramp5(rgba(88, 40, 82), rgba(138, 66, 118), rgba(184, 104, 156), rgba(226, 196, 214), rgba(248, 240, 236));
      for (int i = -1; i <= 1; i++) capsule(c, V(8, 6), V(8 + i * 4, 1.5f), 1.2f, 0.6f, kLeaf, i < 0 ? 1 : 0);
      ball(c, 8, 9.5f, 4.8f, 4.4f, Vg);
      capsule(c, V(8.5f, 13), V(9.5f, 15.2f), 0.9f, 0.4f, Vg, -1);
      c.set(6, 8, Vg[4]); c.set(5, 9, Vg[4]);
      c.set(10, 11, Vg[1]); c.set(7, 12, Vg[1]);
      break;
    }
    case Icon::Fruit: {   // a bunch of grapes on its stem with a vine leaf
      const Ramp& F = tinted ? M : kPurple;
      static const float g[9][2] = {{6, 6}, {9, 6}, {12, 6.5f}, {7.5f, 8.5f}, {10.5f, 8.8f}, {6, 11}, {9, 11}, {7.5f, 13.5f}, {12, 11}};
      for (auto& q : g) ball(c, q[0], q[1], 1.8f, 1.8f, F, 0.05f);
      for (auto& q : g) c.set((int)q[0] - 1, (int)q[1] - 1, F[4]);
      line(c, 9, 4, 10, 1, kWood[2]);
      ellipse(c, 12.5f, 2.8f, 2.6f, 1.6f, kLeaf[3]); c.set(12, 2, kLeaf[4]); c.set(13, 3, kLeaf[1]);
      break;
    }
    case Icon::Egg: {   // eggs in a nest of straw
      const Ramp E = tinted ? M : ramp5(rgba(150, 120, 96), rgba(206, 178, 146), rgba(236, 218, 192), rgba(248, 240, 222), rgba(255, 252, 244));
      for (int x = 1; x <= 14; x++)
        for (int y = 10; y <= 14; y++) {
          const float dx = (x - 7.5f) / 7.0f, dy = (y - 11.5f) / 3.0f;
          if (dx * dx + dy * dy > 1.0f) continue;
          c.set(x, y, kThatch[(hash3(x, y, 21) % 3 == 0) ? 4 : (y > 12 ? 1 : 2)]);
        }
      ball(c, 5.5f, 8.5f, 2.6f, 3.3f, E, 0.05f);
      ball(c, 10.0f, 8.0f, 2.6f, 3.4f, E, 0.05f);
      ball(c, 8.0f, 10.0f, 2.5f, 3.0f, E, 0.05f);
      c.set(7, 8, kWhite); c.set(9, 6, kWhite);
      for (int x = 2; x <= 13; x += 2) c.set(x, 11, kThatch[3]);
      break;
    }
    case Icon::Milk: {   // an earthenware jug, milk at its mouth
      const Ramp J = tinted ? M : ramp5(rgba(92, 52, 40), rgba(148, 86, 58), rgba(190, 124, 80), rgba(222, 164, 110), rgba(244, 204, 150));
      ball(c, 7.5f, 10, 4.8f, 4.6f, J, 0.06f);
      for (int y = 3; y <= 6; y++) for (int x = 5; x <= 10; x++) c.set(x, y, J[x < 7 ? 3 : (x > 9 ? 1 : 2)]);
      for (int x = 4; x <= 11; x++) c.set(x, 2, J[x < 7 ? 4 : 2]);
      c.set(3, 3, J[3]); c.set(4, 3, J[2]);   // the lip
      for (int x = 5; x <= 10; x++) c.set(x, 3, x < 8 ? kWhite : kCloth[4]);   // the milk
      for (int y = 5; y <= 10; y++) c.set(13, y, J[1]);   // the handle
      c.set(12, 4, J[2]); c.set(12, 11, J[1]);
      for (int x = 4; x <= 11; x++) c.set(x, 8, J[1]);   // a band
      c.set(5, 9, J[4]);
      break;
    }
    case Icon::Wool: {   // a skein of wool: a ball of yarn, its loose end
      const Ramp W = tinted ? M : ramp5(rgba(150, 140, 120), rgba(200, 190, 168), rgba(232, 224, 204), rgba(244, 238, 224), rgba(254, 252, 244));
      ball(c, 8, 8.5f, 5.6f, 5.4f, W, 0.05f);
      for (int k = 0; k < 4; k++) {   // the windings
        const float a = 0.6f + k * 0.55f;
        for (int t = -5; t <= 5; t++) {
          const int x = (int)std::lround(8 + t * std::cos(a)), y = (int)std::lround(8.5f + t * std::sin(a) * 0.9f);
          if (solid(c, x, y) && (t & 1)) c.set(x, y, W[1]);
        }
      }
      line(c, 12, 12, 15, 14, W[2]); c.set(14, 14, W[1]);
      c.set(5, 5, W[4]); c.set(6, 5, W[4]);
      break;
    }
    case Icon::Hay: {   // a bale of hay: its lit top, the straw-ends face, two twine bands, stray stalks
      const Ramp H = tinted ? M : ramp5(rgba(140, 104, 40), rgba(190, 150, 56), rgba(224, 188, 82), rgba(240, 214, 120), rgba(252, 238, 172));
      for (int y = 6; y <= 13; y++)
        for (int x = 2; x <= 13; x++) {
          const uint32_t q = hash3(x, y, 31);
          int k = q % 3 == 0 ? 3 : 2;
          if (q % 7 == 0) k = 1;
          if (x == 2) k = 3;
          if (x == 13 || y == 13) k = 1;
          c.set(x, y, H[k]);
        }
      for (int y = 3; y <= 5; y++)
        for (int x = 3 + (5 - y); x <= 14 - (6 - y); x++) c.set(x, y, H[(hash3(x, y, 33) % 3 == 0) ? 3 : 4]);
      for (int y = 3; y <= 13; y++) { c.set(5 + (y < 6 ? 6 - y : 0) / 2, y, kLeather[2]); c.set(10 + (y < 6 ? 6 - y : 0) / 2, y, kLeather[1]); }
      c.set(1, 8, H[3]); c.set(14, 10, H[2]); c.set(1, 11, H[2]); c.set(6, 14, H[2]);   // stray stalks
      break;
    }
    case Icon::Flour: {   // a sack of flour: the open mouth white with flour, a wheat mark
      ball(c, 8, 10, 5.6f, 4.8f, kCloth, 0.05f);
      for (int y = 4; y <= 7; y++) for (int x = 4; x <= 11; x++) c.set(x, y, kCloth[x < 6 ? 4 : (x > 10 ? 2 : 3)]);
      for (int x = 4; x <= 11; x++) c.set(x, 3, x < 8 ? kWhite : rgba(236, 232, 222));
      c.set(5, 2, kWhite); c.set(6, 2, kWhite); c.set(9, 2, rgba(236, 232, 222));
      for (int x = 4; x <= 11; x++) c.set(x, 4, kCloth[1]);   // the turned-down rim
      const Ramp& G = tinted ? M : kGold;
      vline(c, 8, 8, 13, G[2]); c.set(7, 9, G[3]); c.set(9, 9, G[1]); c.set(7, 11, G[3]); c.set(9, 11, G[1]); c.set(8, 7, G[4]);
      break;
    }
    case Icon::Honey: {   // a honey pot with a dipper, honey running over its lip
      const Ramp P = ramp5(rgba(92, 52, 40), rgba(148, 86, 58), rgba(190, 124, 80), rgba(222, 164, 110), rgba(244, 204, 150));
      const Ramp& Hn = tinted ? M : kGold;
      ball(c, 7.5f, 10.5f, 5.2f, 4.4f, P, 0.06f);
      for (int x = 3; x <= 12; x++) { c.set(x, 6, P[x < 6 ? 4 : 2]); c.set(x, 7, Hn[x < 7 ? 4 : 3]); }
      c.set(4, 8, Hn[3]); c.set(4, 9, Hn[2]); c.set(10, 8, Hn[2]);   // drips
      line(c, 9, 7, 13, 1, kWood[2]); c.set(13, 1, kWood[3]);
      c.set(8, 6, Hn[3]); c.set(9, 6, Hn[2]);
      c.set(5, 11, P[4]);
      break;
    }
    case Icon::Meal: {   // a bowl of hot food: the stew's surface, a spoon, steam
      const Ramp& D = tinted ? M : ramp5(rgba(96, 40, 30), rgba(150, 70, 40), rgba(196, 110, 56), rgba(226, 156, 84), rgba(246, 200, 130));
      for (int y = 9; y <= 14; y++)
        for (int x = 1; x <= 14; x++) {
          const float dx = (x + 0.5f - 8.0f) / 7.0f, dy = (y + 0.5f - 9.0f) / 5.5f;
          if (dx * dx + dy * dy > 1.0f || y < 9) continue;
          c.set(x, y, kWood[x < 5 ? 3 : (x > 11 ? 1 : 2)]);
        }
      for (int x = 2; x <= 13; x++) { c.set(x, 8, D[x < 6 ? 4 : 3]); c.set(x, 9, D[2]); }
      c.set(5, 8, kLeaf[3]); c.set(9, 9, kLeaf[2]); c.set(11, 8, rgba(240, 220, 160));
      for (int x = 1; x <= 14; x++) c.set(x, 10, kWood[x < 4 ? 4 : 3]);
      line(c, 10, 8, 14, 3, kWood[3]);   // the spoon
      for (int k = 0; k < 2; k++) { c.set(5 + k * 3, 6 - k, kCloth[4]); c.set(6 + k * 3, 4 - k, kCloth[3]); c.set(5 + k * 3, 3 - k, kCloth[4]); }
      break;
    }
    case Icon::Hoe: {   // a hoe: a long ash handle, the iron blade at its foot
      diagHandle(c, 4, 13, 10, kWood);
      const Ramp& I = tinted ? M : kIron;
      for (int y = 10; y <= 14; y++) for (int x = 1; x <= 4; x++) if (x + (14 - y) / 2 <= 5) c.set(x, y, I[x == 1 ? 4 : (y == 14 ? 1 : 3)]);
      c.set(4, 12, I[2]); c.set(5, 12, kIron[1]);
      break;
    }
    case Icon::WateringCan: {   // a watering can: the body, the long spout and its rose, the handle
      const Ramp& I = tinted ? M : ramp5(rgba(40, 70, 70), rgba(64, 108, 104), rgba(98, 148, 136), rgba(142, 186, 168), rgba(206, 226, 210));
      for (int y = 6; y <= 13; y++) for (int x = 3; x <= 10; x++) c.set(x, y, I[x == 3 ? 4 : (x < 6 ? 3 : (x > 9 ? 1 : 2))]);
      for (int x = 3; x <= 10; x++) { c.set(x, 6, I[4]); c.set(x, 13, I[0]); }
      line(c, 10, 11, 14, 5, I[3]); line(c, 10, 12, 14, 6, I[1]);
      c.set(14, 4, I[4]); c.set(15, 4, I[2]); c.set(15, 5, I[1]);
      for (int x = 4; x <= 9; x++) c.set(x, 3, I[x < 6 ? 3 : 2]);
      c.set(4, 4, I[2]); c.set(4, 5, I[2]); c.set(9, 4, I[1]); c.set(9, 5, I[1]);   // the handle
      c.set(2, 8, I[2]); c.set(2, 9, I[2]); c.set(2, 10, I[1]);
      c.set(13, 7, kWater[4]); c.set(15, 7, kWater[3]); c.set(14, 9, kWater[3]);   // drops
      break;
    }
    case Icon::Sickle: {   // a sickle: the crescent blade, a short handle
      const Ramp& I = tinted ? M : kIron;
      for (int a = 0; a < 26; a++) {
        const float t = (float)a / 25.0f;
        const float ang = -PI * 0.15f - t * PI * 1.05f;
        const float r = 5.6f - t * 0.6f;
        const int x = (int)std::lround(8.5f + std::cos(ang) * r), y = (int)std::lround(7.0f + std::sin(ang) * r);
        c.set(x, y, I[4]);
        c.set((int)std::lround(8.5f + std::cos(ang) * (r - 1.0f)), (int)std::lround(7.0f + std::sin(ang) * (r - 1.0f)), I[t < 0.8f ? 2 : 3]);
      }
      for (int i = 0; i < 4; i++) { c.set(11 + i / 2, 9 + i, kWood[(i & 1) ? 2 : 3]); c.set(12 + i / 2, 9 + i, kWood[1]); }
      c.set(13, 14, kWood[1]);
      break;
    }
    case Icon::Brush: {   // a grooming brush: a wooden back, the bristles, a leather strap over it
      const Ramp& B = tinted ? M : kWood;
      for (int y = 5; y <= 10; y++)
        for (int x = 2; x <= 13; x++) {
          const float dx = (x + 0.5f - 8.0f) / 6.0f, dy = (y + 0.5f - 7.5f) / 3.0f;
          if (dx * dx + dy * dy > 1.0f) continue;
          c.set(x, y, B[y < 7 ? (x < 7 ? 4 : 3) : 2]);
        }
      for (int x = 3; x <= 12; x++) for (int y = 10; y <= 12; y++) c.set(x, y, (x & 1) ? kThatch[1] : kThatch[2]);
      for (int x = 3; x <= 12; x++) c.set(x, 6, kLeather[x < 8 ? 3 : 2]);
      c.set(3, 7, kLeather[1]); c.set(12, 7, kLeather[1]);
      c.set(5, 6, kBrass[4]); c.set(10, 6, kBrass[3]);
      break;
    }
    case Icon::Deed: {   // a deed: a rolled parchment tied with a ribbon, a red wax seal
      for (int y = 4; y <= 11; y++) for (int x = 2; x <= 13; x++) c.set(x, y, kCloth[x < 4 ? 4 : (y > 9 ? 2 : 3)]);
      for (int y = 3; y <= 12; y++) { c.set(1, y, kCloth[(y & 1) ? 3 : 4]); c.set(14, y, kCloth[(y & 1) ? 1 : 2]); }
      for (int y = 3; y <= 12; y++) c.set(2, y, kCloth[1]);
      for (int x = 4; x <= 11; x += 1) if (x % 4) c.set(x, 6, kCloth[0]);
      for (int x = 4; x <= 9; x += 1) if (x % 3) c.set(x, 8, kCloth[0]);
      const Ramp& Sl = tinted ? M : kRed;
      vline(c, 11, 2, 14, kGold[2]); c.set(10, 14, kGold[3]); c.set(12, 14, kGold[1]);
      ball(c, 11, 10.5f, 2.3f, 2.2f, Sl, 0.0f); c.set(10, 9, Sl[4]); c.set(11, 11, Sl[1]);
      break;
    }
    default: break;
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
    // ---- M7 Home: the farm's goods and tools (painted apart, so the classic icons' code is untouched)
    case Icon::Seeds: case Icon::Sheaf: case Icon::Veg: case Icon::Fruit: case Icon::Egg: case Icon::Milk: case Icon::Wool:
    case Icon::Hay: case Icon::Flour: case Icon::Honey: case Icon::Meal: case Icon::Hoe: case Icon::WateringCan:
    case Icon::Sickle: case Icon::Brush: case Icon::Deed:
    case Icon::Fish: case Icon::Pie: case Icon::Roast: case Icon::Cake:
      paintFarmIcon(c, ic, tinted, M);
      break;
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
  if (l.dollMetal >= 2 && l.dollMetal <= 11 && !leather) {   // (alloys i1, rig 2) the doll's own metal ramp
    uint32_t k[5];
    if (l.dollMetal == 10 && l.alloyFam) doll::alloyRamp(l.alloyFam - 1, l.sheen, k);
    else if (l.dollMetal == 10) metalRampKeys(l.tint, l.tint2, l.sheen, k);
    else doll::metalKindRamp(l.dollMetal, k);
    M = ramp5(k[0], k[1], k[2], k[3], k[4]);
  } else if (l.tint2 || l.sheen) {
    uint32_t k[5];
    metalRampKeys(l.tint, l.tint2, l.sheen, k);
    M = ramp5(k[0], k[1], k[2], k[3], k[4]);
  } else if (leather && (l.icon == Icon::Helmet || l.icon == Icon::Armor || l.icon == Icon::Shield || l.icon == Icon::Gloves || l.icon == Icon::Boots)) {
    M = kLeather;
  } else if (l.tint) {
    M = ramp(l.tint, 1.1f);
  }
  // (P4 ART, D117) iron's body armour icon is dark and matte, a step under its ramp (the doll's "iron dark, steel bright"):
  // at the icon's size an iron and a steel coat of one form otherwise met within 13-29 of mean RGB
  // and bronze's a third of a step over its olive body toward the cream glint: the warm gold the doll's bronze plates read as
  // (a bronze coat sat 14-28 from the leather one's browns)
  const bool p4 = rigVersion() >= 2;   // (rig 2 only: rig 1's icons as they were)
  if (p4 && l.icon == Icon::Armor && l.dollMetal == 3 && !leather) {
    M = ramp5(M[0], mix(M[0], M[1], 0.5f), M[1], M[2], M[3]);
  }
  if (p4 && l.icon == Icon::Armor && l.dollMetal == 2 && !leather) M = ramp5(M[0], mix(M[1], M[2], 0.5f), mix(M[2], M[3], 0.4f), M[3], M[4]);
  const Ramp acc = l.accent ? ramp(l.accent) : kRed;
  const Ramp trim = (l.ornament & 4) ? kGold : M;
  const Ramp grip = l.accent ? ramp(mix(l.accent, rgba(92, 54, 40), 0.35f)) : kLeather;
  const Ramp& gold = (l.ornament & 4) ? kGold : kBrass;
  bool done = true;
  const bool sigDone = sigIconPiece(c, l, M) || sigIconPieceB(c, l, M);   // (P5b) a signed people's piece, painted whole
  if (!sigDone) switch (l.icon) {
    case Icon::Helmet: if (l.form >= 1 && l.form <= 10) helmIcon(c, l.form, M, acc, trim); else done = false; break;
    case Icon::Armor: if (l.form >= 1 && l.form <= 8) bodyIcon(c, l.form, M, l.accent ? acc : ramp(rgba(120, 60, 50)), leather); else done = false; break;
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
  const bool sigLayer = !sigDone && (sigBodyLayer(c, l) || sigBodyLayerB(c, l, M));   // (P5b fix r2) a signed coat's layer, in place of P5's marks
  // (P5, FIX_BATCH_P5 4.5) the maker people's fingerprint: rig 2 and FIX_FP only (EMB_FIX_RULES without it, rig 1 and a piece
  // with no maker people paint as before)
  const bool fpKind = l.icon == Icon::Armor || l.icon == Icon::Helmet || l.icon == Icon::Shield || l.icon == Icon::Gloves ||
                      l.icon == Icon::Boots || l.icon == Icon::Cloak;
  if (p4 && fpKind && l.people >= 1 && doll::fixOn(doll::FIX_FP) && !(sigDone && (l.icon == Icon::Helmet || l.icon == Icon::Shield || l.icon == Icon::Cloak)) &&
      !sigLayer)
    iconFingerprint(c, l, M, acc);
  // (P4 ART, D117) a gilt iron coat is gilt at the collar and the shoulders only: a whole gold rim round a dark iron coat read,
  // by its mean, as bronze (Imperial, Sun-Temple, Starspire iron sat 22-27 from their bronze); the gold still marks the maker
  const bool ironGilt = p4 && l.icon == Icon::Armor && l.dollMetal == 3 && !leather && (l.ornament & 4);
  if (ironGilt) {
    const Canvas src = c;
    iconOrnament(c, M, (uint16_t)(l.ornament & ~4u), l.icon);
    for (int y = 0; y <= 5; y++)
      for (int x = 0; x < ICON; x++) {
        if (!(src.get(x, y) >> 24)) continue;
        const bool edge = !(src.get(x - 1, y) >> 24) || !(src.get(x, y - 1) >> 24) || !(src.get(x + 1, y) >> 24);
        if (edge) c.set(x, y, kGold[(!(src.get(x - 1, y) >> 24) || !(src.get(x, y - 1) >> 24)) ? 4 : 2]);
      }
  } else {
    iconOrnament(c, M, l.ornament, l.icon);
  }
  iconGlint(c, l.rarity);
  const Canvas raw = c;
  outline(c);
  if (l.rarity == 4) {   // a legendary's outline warms toward gold
    for (int y = 0; y < ICON; y++)
      for (int x = 0; x < ICON; x++) {
        const uint32_t v = c.get(x, y);
        if ((v >> 24) && !(raw.get(x, y) >> 24)) c.set(x, y, mix(v, rgba(255, 190, 80), 0.45f));
      }
  } else if (l.rarity >= 5) {   // (M8b) a mythic's burns red-orange: hotter along the top, ember-dark along the foot
    for (int y = 0; y < ICON; y++)
      for (int x = 0; x < ICON; x++) {
        const uint32_t v = c.get(x, y);
        if (!(v >> 24) || (raw.get(x, y) >> 24)) continue;
        const float u = (float)y / (float)(ICON - 1);
        const uint32_t fc = u < 0.4f ? rgba(255, 150, 60) : u < 0.75f ? rgba(255, 96, 36) : rgba(196, 44, 24);
        c.set(x, y, mix(v, fc, 0.55f));
      }
  }
  return c;
}

}  // namespace art
