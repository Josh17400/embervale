// rpg/art/doll/doll_alloy.cpp - (P2c alloys i1) the culture alloys' metal families, the metal test and CIE76 delta E.
// Split from doll_ramps.cpp into the doll's data library (rpg_doll): the gear ladder (rpg/sim/gear_ladder.cpp) picks each
// people's alloy families from these ramps, and save_test links rpg_sim without the painters. Colour helpers are local
// copies of doll_ramps.cpp's (same formulas, so the ramps agree to the bit).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include "rpg/art/doll/doll.h"

namespace art {
namespace doll {

namespace {

struct Lab { double L, a, b; };
int chR(uint32_t c) { return (int)(c & 255u); }
int chG(uint32_t c) { return (int)(c >> 8 & 255u); }
int chB(uint32_t c) { return (int)(c >> 16 & 255u); }
double lin(double c) { c /= 255.0; return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }
double unlin(double c) { const double v = c <= 0.0031308 ? 12.92 * c : 1.055 * std::pow(std::max(c, 0.0), 1 / 2.4) - 0.055; return std::clamp(v * 255.0, 0.0, 255.0); }
Lab toLab(uint32_t c) {
  const double r = lin(chR(c)), g = lin(chG(c)), b = lin(chB(c));
  const double X = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047, Y = 0.2126 * r + 0.7152 * g + 0.0722 * b,
               Z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883;
  auto f = [](double t) { return t > 0.008856 ? std::cbrt(t) : 7.787 * t + 16.0 / 116.0; };
  const double fx = f(X), fy = f(Y), fz = f(Z);
  return {116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz)};
}
uint32_t fromLab(Lab l) {
  const double fy = (l.L + 16) / 116, fx = fy + l.a / 500, fz = fy - l.b / 200;
  auto fi = [](double t) { return t * t * t > 0.008856 ? t * t * t : (t - 16.0 / 116.0) / 7.787; };
  const double X = fi(fx) * 0.95047, Y = fi(fy), Z = fi(fz) * 1.08883;
  const double r = 3.2406 * X - 1.5372 * Y - 0.4986 * Z, g = -0.9689 * X + 1.8758 * Y + 0.0415 * Z, b = 0.0557 * X - 0.2040 * Y + 1.0570 * Z;
  return rgba((int)std::lround(unlin(r)), (int)std::lround(unlin(g)), (int)std::lround(unlin(b)));
}
double luma(uint32_t c) { return chR(c) * 0.299 + chG(c) * 0.587 + chB(c) * 0.114; }
double dist2(const Lab& x, const Lab& y) { return (x.L - y.L) * (x.L - y.L) + (x.a - y.a) * (x.a - y.a) + (x.b - y.b) * (x.b - y.b); }
uint32_t mixHash(uint32_t h, uint32_t v) { h ^= v + 0x9E3779B9u + (h << 6) + (h >> 2); return h; }

// ---- (alloys i1, the owner: "does this fit with all our ores") the culture alloys as METAL FAMILIES. i2-i3's metalize()
// drew every alloy as a faint cast over A3's steel (the panel had read stronger tints as "pink clay", "beige cloth"), so
// the alloys looked like steel and like each other. Clay is a VALUE problem, not a colour one: clay is mid-value and soft,
// metal is a near-black core beside a near-white specular. Each family keeps a clear hue and chroma, and every ramp has
// the metal's value structure: step 0 a dark core (L 8-16), steps 1-3 a wide lit plane (L 24-87), step 4 a hard
// specular (L 92-99) that desaturates toward white (warm metals keep a warm glint). Shadows turn toward violet, lights
// toward yellow (A3's steel turns the same way). The sheen (AlloyLook::sheen) shapes the top and the depth.
struct AlloyFam { const char* name; double h, C; int v; };
// (alloys i3, the blind panel: "Eaglesilver drawn as brass, Thorniron as white silver", "nine near-identical golds", "violet
// matte reads as lavender cloth, the garnets as maroon cloth") the family follows the alloy's NAME: the metal word of its
// name (BRONZE BRASS IRON STEEL SILVER GLASS, cult makeAlloys) and its rank pick a slot, and each people has its own family
// in every slot (kAlloyFam[slot * 12 + archetype]: gear::alloyFamilies). A bronze is warm and orange-brown, a brass yellow,
// an iron dark and cool (never brown: that read as leather), a steel mid and cool, a silver pale, a glass a deep jewel.
// Solved offline (tools/doll/alloys: sim.py is famRamp in Python, opt.py the solver, port.py writes this table): a people's three metals part by delta E 22 on the mid step under their
// own sheens, two peoples' same-slot metals by 13, no lit step reads as skin, pink or brown (the "clay" and "cloth" reads).
// slots: 0 bronze, 1 brass, 2 iron (the everyday metal), 3 steel, 4 silver (the warriors'), 5 steel, 6 silver, 7 glass
// (the masterwork); peoples in cult::Archetype order: Fj Hi He Im Du St Ma Ja Ri Su Sy Ss
const AlloyFam kAlloyFam[] = {
    {"SEA BRONZE", 150, 22, 0},            // BRONZE FJ
    {"PEAT BRONZE", 60, 30, -1},           // BRONZE HI
    {"AMBER BRONZE", 70, 38, 0},          // BRONZE HE
    {"RED BRONZE", 60, 46, 0},            // BRONZE IM
    {"SUN BRONZE", 70, 50, 1},            // BRONZE DU
    {"DARK COPPER", 60, 50, -2},           // BRONZE ST
    {"OLIVE BRONZE", 100, 38, -1},          // BRONZE MA
    {"BLACK BRONZE", 70, 22, -2},          // BRONZE JA
    {"RED COPPER", 50, 54, -1},            // BRONZE RI
    {"GOLD BRONZE", 110, 40, 0},           // BRONZE SU
    {"MOSS BRONZE", 100, 28, -1},           // BRONZE SY
    {"WHITE BRONZE", 80, 22, 1},          // BRONZE SS
    {"PALE BRASS", 90, 32, 1},            // BRASS FJ
    {"OLD BRASS", 90, 38, 0},             // BRASS HI
    {"BRASS", 80, 62, 1},                 // BRASS HE
    {"RED BRASS", 60, 56, 0},             // BRASS IM
    {"SUN BRASS", 80, 58, 0},             // BRASS DU
    {"DARK BRASS", 20, 14, -2},            // BRASS ST
    {"GREEN BRASS", 110, 46, 1},           // BRASS MA
    {"TEMPLE BRASS", 70, 48, -1},          // BRASS JA
    {"LEMON BRASS", 100, 56, 0},           // BRASS RI
    {"ROYAL BRASS", 90, 56, 1},           // BRASS SU
    {"LEAF BRASS", 100, 40, 2},            // BRASS SY
    {"STAR BRASS", 100, 28, 2},            // BRASS SS
    {"RIME IRON", 240, 22, -2},             // IRON FJ
    {"BOG IRON", 120, 8, -2},              // IRON HI
    {"BLUED IRON", 260, 22, 0},            // IRON HE
    {"BLACK IRON", 210, 12, -2},            // IRON IM
    {"GREY IRON", 130, 22, -2},             // IRON DU
    {"VIOLET IRON", 270, 12, -1},           // IRON ST
    {"MOSS IRON", 150, 8, 0},             // IRON MA
    {"INK IRON", 300, 20, -2},              // IRON JA
    {"SLATE IRON", 170, 20, -2},            // IRON RI
    {"NIGHT IRON", 320, 4, 0},            // IRON SU
    {"GREEN IRON", 120, 16, 0},            // IRON SY
    {"DUSK IRON", 340, 4, -2},             // IRON SS
    {"FROST STEEL", 260, 28, 1},           // STEEL1 FJ
    {"COBALT STEEL", 240, 14, 0},          // STEEL1 HI
    {"WHITE STEEL", 190, 16, 0},           // STEEL1 HE
    {"GREY STEEL", 310, 14, 0},            // STEEL1 IM
    {"GREEN STEEL", 120, 8, 1},           // STEEL1 DU
    {"SKY STEEL", 250, 34, -1},             // STEEL1 ST
    {"DARK STEEL", 190, 28, -1},            // STEEL1 MA
    {"TEAL STEEL", 190, 30, 1},            // STEEL1 JA
    {"AQUA STEEL", 220, 34, 1},            // STEEL1 RI
    {"BLACK STEEL", 220, 20, -1},           // STEEL1 SU
    {"JADE STEEL", 130, 32, 1},            // STEEL1 SY
    {"BLUE STEEL", 300, 30, 1},            // STEEL1 SS
    {"VIOLET SILVER", 310, 16, 2},         // SILVER1 FJ
    {"MINT SILVER", 220, 18, 2},           // SILVER1 HI
    {"ICE SILVER", 140, 18, 1},            // SILVER1 HE
    {"WHITE SILVER", 270, 18, 2},          // SILVER1 IM
    {"TEAL SILVER", 130, 4, 2},           // SILVER1 DU
    {"PALE SILVER", 220, 14, 1},           // SILVER1 ST
    {"SKY SILVER", 310, 10, 1},            // SILVER1 MA
    {"BLUE SILVER", 260, 4, 1},           // SILVER1 JA
    {"PEARL SILVER", 180, 18, 1},          // SILVER1 RI
    {"LILAC SILVER", 160, 18, 2},          // SILVER1 SU
    {"JADE SILVER", 300, 18, 1},           // SILVER1 SY
    {"STAR SILVER", 310, 6, 2},           // SILVER1 SS
    {"JADE STEEL", 170, 34, 1},            // STEEL2 FJ
    {"VIOLET STEEL", 290, 38, 1},          // STEEL2 HI
    {"COBALT STEEL", 260, 44, 1},          // STEEL2 HE
    {"PURPLE STEEL", 300, 44, -1},          // STEEL2 IM
    {"TURQUOISE STEEL", 200, 34, 0},       // STEEL2 DU
    {"TEAL STEEL", 240, 38, 1},            // STEEL2 ST
    {"GREEN STEEL", 140, 30, -1},           // STEEL2 MA
    {"DARK JADE STEEL", 150, 42, -1},       // STEEL2 JA
    {"EMERALD STEEL", 130, 44, 1},         // STEEL2 RI
    {"INDIGO STEEL", 280, 26, -1},          // STEEL2 SU
    {"LEAF STEEL", 160, 28, -1},            // STEEL2 SY
    {"BLUE STEEL", 270, 40, -1},            // STEEL2 SS
    {"JADE SILVER", 180, 10, 1},           // SILVER2 FJ
    {"LILAC SILVER", 290, 24, 2},          // SILVER2 HI
    {"MINT SILVER", 230, 8, 2},           // SILVER2 HE
    {"SKY SILVER", 130, 12, 2},            // SILVER2 IM
    {"SEAFOAM SILVER", 190, 24, 2},        // SILVER2 DU
    {"TEAL SILVER", 160, 24, 1},           // SILVER2 ST
    {"GREEN SILVER", 240, 24, 2},          // SILVER2 MA
    {"EMERALD SILVER", 140, 24, 2},        // SILVER2 JA
    {"COBALT SILVER", 270, 14, 1},         // SILVER2 RI
    {"WHITE SILVER", 280, 24, 1},          // SILVER2 SU
    {"AQUA SILVER", 210, 22, 1},           // SILVER2 SY
    {"STAR SILVER", 240, 20, 1},           // SILVER2 SS
    {"ICE GLASS", 220, 30, 0},             // GLASS2 FJ
    {"PEAT GLASS", 140, 38, 0},            // GLASS2 HI
    {"SAPPHIRE GLASS", 250, 42, 0},        // GLASS2 HE
    {"GARNET GLASS", 300, 36, 0},          // GLASS2 IM
    {"AMBER GLASS", 130, 36, -1},           // GLASS2 DU
    {"SKY GLASS", 240, 32, 0},             // GLASS2 ST
    {"BOG GLASS", 130, 46, -1},             // GLASS2 MA
    {"JADE GLASS", 130, 26, 0},            // GLASS2 JA
    {"RIVER GLASS", 180, 30, 0},           // GLASS2 RI
    {"NIGHT GLASS", 300, 28, -1},           // GLASS2 SU
    {"GREEN GLASS", 160, 38, 0},           // GLASS2 SY
    {"VIOLET GLASS", 280, 46, 0},          // GLASS2 SS
};
constexpr int kAlloyFams = (int)(sizeof kAlloyFam / sizeof kAlloyFam[0]);
constexpr int kAlloySlots = 8;
static_assert(kAlloyFams == kAlloySlots * 12, "a family per slot per people");
double hueTurn(double h, double target, double maxDeg) {   // h moved toward target by at most maxDeg (shortest arc)
  double d = std::fmod(target - h + 540.0, 360.0) - 180.0;
  return h + std::clamp(d, -maxDeg, maxDeg);
}
Lab lchLab(double L, double C, double hDeg) { const double r = hDeg * 0.01745329; return {L, C * std::cos(r), C * std::sin(r)}; }
void famRamp(int f, uint8_t sheen, uint32_t out[5]) {
  const AlloyFam& F = kAlloyFam[std::clamp(f, 0, kAlloyFams - 1)];
  // step 1 is the lit edge of the silhouette on the darkest day ground (C14: 40 luma over it), so it never sinks below L 28.
  // (i3: "lavender cloth", "khaki cloth") cloth shades in even steps at one chroma; metal jumps: a saturated, darker body
  // (step 2 at 1.2x chroma) under a hard, pale lit plane (step 3 at half the chroma, 28 L above it) and a near-white glint.
  // Value class 2 is the silvers' (a pale body that still keeps its dark core).
  static const double kLv[5][5] = {{5, 28, 38, 66, 94}, {7, 30, 44, 72, 96}, {9, 33, 52, 80, 98}, {13, 40, 62, 87, 99}, {18, 48, 71, 92, 100}};
  static const double kCf[5] = {0.7, 1.1, 1.2, 0.55, 0.12};
  double L[5], C[5], H[5];
  for (int i = 0; i < 5; i++) { L[i] = kLv[std::clamp(F.v + 2, 0, 4)][i]; C[i] = F.C * kCf[i]; H[i] = F.h; }
  H[0] = hueTurn(F.h, 290, 22); H[1] = hueTurn(F.h, 290, 10); H[3] = hueTurn(F.h, 90, 8); H[4] = hueTurn(F.h, 90, 14);
  // (i2) a red's lights turn harder toward gold and lose chroma, or its lit plane reads as pink cloth (the panel's garnet)
  if (F.h >= 330 || F.h < 40) { H[3] = hueTurn(F.h, 90, 14); H[4] = hueTurn(F.h, 90, 30); C[3] *= 0.8; }
  switch (sheen) {
    case 1:   // matte: a softer top (a shorter shine, still a glint: i2's 0.62 read as leather), a full body
      L[4] = L[3] + (L[4] - L[3]) * 0.85; C[3] *= 1.05; C[4] = C[3] * 0.5; L[0] += 2;
      break;
    case 2:   // bright (polished): a white-hot glint over a deeper body
      L[4] = 99; C[4] = F.C * 0.1; L[3] += 3; L[2] -= 3;
      break;
    case 3: case 6:   // dark (blackened) and glowing (a dark body; the seams carry the glow): a cold rim
      for (int i = 0; i < 4; i++) L[i] = i == 1 ? std::max(31.0, L[i] * 0.84) : L[i] * (i == 3 ? 0.94 : 0.82);
      for (int i = 1; i < 4; i++) C[i] *= 0.85;
      H[4] = hueTurn(F.h, 250, 60); C[4] = 10; L[4] = std::max(L[4] - 2, 90.0);
      break;
    case 5:   // iridescent: the hue turns across the ramp (violet shadow, its own body, a warm-pale light)
      H[0] = F.h - 50; H[1] = F.h - 24; H[3] = F.h + 30; H[4] = F.h + 56;
      for (int i = 0; i < 4; i++) C[i] *= 1.1;
      C[4] = F.C * 0.42;
      break;
    case 7:   // pale: whitened, the body lighter, the colour gentler (still a dark core)
      for (int i = 1; i < 5; i++) L[i] = std::min(99.0, L[i] + 5);
      for (int i = 0; i < 5; i++) C[i] *= 0.75;
      break;
    case 8:   // burnished: rich; a warm metal's lights turn toward gold (i3: a cool one's turned mauve-brown, so it keeps its hue)
      if (std::fabs(std::fmod(F.h - 80.0 + 540.0, 360.0) - 180.0) <= 60.0) { H[3] = hueTurn(F.h, 80, 12); H[4] = hueTurn(F.h, 80, 22); C[4] = F.C * 0.44; }
      else C[4] = F.C * 0.3;
      C[3] *= 1.1; L[4] = 97;
      break;
    default: break;   // banded (satin) and none: the family as drawn
  }
  for (int i = 0; i < 5; i++) out[i] = fromLab(lchLab(L[i], C[i], H[i])) | 0xFF000000u;
  // (i2, C15e) the lit body (steps 2 and 3) keeps off skin: a warm metal at the skin's value and chroma read as "clay" or
  // "bare arms"; it gains chroma and turns its hue away from the skin's (the mid step a little darker) until 22 apart from A3's skin (#E9A077), the glint and core kept
  {
    const Lab skin = toLab(rgba(0xE9, 0xA0, 0x77));
    for (int i = 2; i <= 3; i++)
      for (int it = 0; it < 24 && std::sqrt(dist2(toLab(out[i]), skin)) < 22.0; it++) {
        C[i] *= 1.05; H[i] += H[i] < 55.0 || H[i] > 235.0 ? -2.0 : 2.0; if (i == 2) L[i] -= 0.8;   // the hue turns away from the skin's
        out[i] = fromLab(lchLab(L[i], C[i], H[i])) | 0xFF000000u;
      }
  }
  // the edge step lit to luma 82 (a blue's lightness gives less luma than a gold's: C14's edge on the darkest ground)
  for (int it = 0; it < 30 && luma(out[1]) < 82.0 && L[1] < L[2] - 6; it++) { L[1] += 1; out[1] = fromLab(lchLab(L[1], C[1], H[1])) | 0xFF000000u; }
  // (P2c forge, C17; STYLE_P2 28.8: the alloy panel's clay, cloth and leather) metal reads as metal on the figure: the lit
  // plane (step 3) loses its colour (60 % of the body's chroma at most) and stands hard and pale over the body (22 luma or
  // more) and over the core (step 1 at 45 % of its luma or less: the plane rises first, then the core sinks, never under the
  // edge's luma 82 while the plane can rise); a red's lit plane never pink (turned toward gold); the glint near-white
  auto set = [&](int i) { out[i] = fromLab(lchLab(L[i], C[i], H[i])) | 0xFF000000u; };
  C[3] = std::min(C[3], 0.56 * C[2]);
  set(3);
  auto chromaOf = [](uint32_t c) { const Lab l = toLab(c); return std::hypot(l.a, l.b); };
  auto hueOf = [](uint32_t c) { const Lab l = toLab(c); return std::fmod(std::atan2(l.b, l.a) * 57.29578 + 360.0, 360.0); };
  for (int it = 0; it < 20 && chromaOf(out[3]) >= 13.0 && (hueOf(out[3]) >= 328.0 || hueOf(out[3]) < 32.0); it++) { H[3] = hueTurn(H[3], 60.0, 6.0); C[3] *= 0.94; set(3); }
  for (int it = 0; it < 30 && chromaOf(out[2]) >= 6.0 && chromaOf(out[3]) > 0.57 * chromaOf(out[2]); it++) { C[3] *= 0.92; set(3); }
  for (int it = 0; it < 40 && (luma(out[3]) < luma(out[2]) + 23.0 || luma(out[1]) > 0.44 * luma(out[3])) && L[3] < 96.0; it++) { L[3] += 1; set(3); }
  for (int it = 0; it < 40 && luma(out[1]) > 0.44 * luma(out[3]) && L[1] > 12.0; it++) { L[1] -= 1; set(1); }
  for (int it = 0; it < 30 && luma(out[4]) < 198.0 && L[4] < 100.0; it++) { L[4] += 1; C[4] *= 0.9; set(4); }
  // the dark sheen's tell: a blackened body (step 2 at luma 90 or less) under the near-white glint
  if (sheen == 3) for (int it = 0; it < 40 && luma(out[2]) > 88.0 && L[2] > L[1] + 3.0; it++) { L[2] -= 1; set(2); }
  // the lit plane off the skin too (the body's push above moved the mid step; a pale plane can still sit on the skin)
  {
    const Lab skin = toLab(rgba(0xE9, 0xA0, 0x77));
    for (int it = 0; it < 30 && std::sqrt(dist2(toLab(out[3]), skin)) < 21.0; it++) {
      C[3] *= 0.9; H[3] += H[3] < 55.0 || H[3] > 235.0 ? -3.0 : 3.0; if (L[3] < 96.0) L[3] += 0.5;
      set(3);
    }
  }
}
// the family nearest a light key (its colour against the family's lit step 3; lightness counts a quarter: a key's value
// is the sheen's business, its hue the people's); a grey key (chroma < 8) by its own hash, so it is stable
int famNearest(uint32_t light, uint8_t /*sheen*/, int skipMask = 0) {
  const Lab k = toLab(light | 0xFF000000u);
  if (std::hypot(k.a, k.b) < 8.0) {
    for (int t = 0; t < kAlloyFams; t++) {
      const int f = (int)((mixHash(0xA110u, light & 0xFFFFFFu) + (uint32_t)t) % (uint32_t)kAlloyFams);
      if (f >= 31 || !(skipMask >> f & 1)) return f;
    }
  }
  int best = -1;
  double bd = 1e18;
  for (int f = 0; f < kAlloyFams; f++) {
    if (f < 31 && (skipMask >> f & 1)) continue;
    uint32_t r[5];
    famRamp(f, 0, r);   // the family as drawn (an iridescent or burnished sheen turns its lights: the body keeps the hue)
    const Lab m = toLab(r[3]);
    const double d = 0.25 * (k.L - m.L) * (k.L - m.L) + (k.a - m.a) * (k.a - m.a) + (k.b - m.b) * (k.b - m.b);
    if (d < bd) { bd = d; best = f; }
  }
  return best < 0 ? 0 : best;
}
}  // namespace

float deltaE76(uint32_t a, uint32_t b) { return (float)std::sqrt(dist2(toLab(a), toLab(b))); }

int alloyFamilyCount() { return kAlloyFams; }
int alloyFamilyAt(int slot, int people) { return std::clamp(slot, 0, kAlloySlots - 1) * 12 + std::clamp(people, 0, 11); }
int alloyFamilyMetal(int fam) {
  static const int kMetal[kAlloySlots] = {0, 1, 2, 3, 4, 3, 4, 5};
  return fam >= 0 && fam < kAlloyFams ? kMetal[fam / 12] : -1;
}
const char* alloyFamilyName(int f) { return f >= 0 && f < kAlloyFams ? kAlloyFam[f].name : "?"; }
int alloyFamilyOf(uint32_t light, uint8_t sheen, int skipMask) { return famNearest(light, sheen, skipMask); }
void alloyRamp(int fam, uint8_t sheen, uint32_t out[5]) { famRamp(fam, sheen, out); }
bool metalTest(const uint32_t r[5], std::string* why) {
  char b[160];
  const double l0 = luma(r[0]), l1 = luma(r[1]), l3 = luma(r[3]), l4 = luma(r[4]);
  const char* bad = l4 < 196.0 ? "step 4 (the glint) under luma 196" : l4 - l0 < 165.0 ? "steps 0-4 under 165 luma apart"
                    : l0 > 56.0 ? "step 0 (the core) over luma 56" : l3 - l1 < 60.0 ? "steps 1-3 under 60 luma apart" : nullptr;
  if (bad && why) { std::snprintf(b, sizeof b, "%s (luma %.0f %.0f %.0f %.0f)", bad, l0, l1, l3, l4); *why = b; }
  return !bad;
}

// (P2c forge, C17; STYLE_P2 28.8) metal reads as metal: a near-black core (step 1 at 45 % of step 3's luma or less), a hard
// pale lit plane (step 3 at least 22 luma over step 2, at most 60 % of its chroma), a near-white glint (step 4 luma 196 or
// more); warm metals' lit steps 20 dE off the skin and never pink (a red lit step at chroma 15 or more)
bool metalRead(const uint32_t r[5], uint32_t skin, std::string* why) {
  char b[200];
  const double l1 = luma(r[1]), l2 = luma(r[2]), l3 = luma(r[3]), l4 = luma(r[4]);
  const Lab s2 = toLab(r[2]), s3 = toLab(r[3]), sk = toLab(skin | 0xFF000000u);
  const double c2 = std::hypot(s2.a, s2.b), c3 = std::hypot(s3.a, s3.b);
  const double h3 = std::fmod(std::atan2(s3.b, s3.a) * 57.29578 + 360.0, 360.0);
  const char* bad = nullptr;
  double v = 0;
  if (l1 > 0.45 * l3 + 0.5) { bad = "step 1 over 45 % of step 3's luma (no dark core)"; v = l1 / std::max(1.0, l3); }
  else if (l3 - l2 < 22.0) { bad = "step 3 under 22 luma over step 2 (no hard lit plane)"; v = l3 - l2; }
  else if (c2 >= 6.0 && c3 > 0.6 * c2 + 0.5) { bad = "step 3 over 60 % of step 2's chroma (the light keeps its colour: clay)"; v = c3 / c2; }
  else if (l4 < 196.0) { bad = "step 4 under luma 196 (no glint)"; v = l4; }
  else if (std::sqrt(dist2(s2, sk)) < 20.0 || std::sqrt(dist2(s3, sk)) < 20.0) { bad = "a lit step within 20 dE of the skin"; v = std::min(std::sqrt(dist2(s2, sk)), std::sqrt(dist2(s3, sk))); }
  else if (c3 >= 15.0 && (h3 >= 330.0 || h3 < 30.0)) { bad = "step 3 pink"; v = h3; }
  if (bad && why) { std::snprintf(b, sizeof b, "%s [%.2f] (luma %.0f %.0f %.0f %.0f, chroma %.0f %.0f)", bad, v, l1, l2, l3, l4, c2, c3); *why = b; }
  return !bad;
}

}  // namespace doll
}  // namespace art
