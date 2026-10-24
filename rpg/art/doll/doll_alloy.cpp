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
    {"SEA BRONZE", 150, 26, -1},            // BRONZE FJ
    {"PEAT BRONZE", 60, 32, -1},           // BRONZE HI
    {"AMBER BRONZE", 70, 42, 0},          // BRONZE HE
    {"RED BRONZE", 60, 52, 0},            // BRONZE IM
    {"SUN BRONZE", 80, 52, 1},            // BRONZE DU
    {"DARK COPPER", 50, 54, -2},           // BRONZE ST
    {"OLIVE BRONZE", 100, 36, -1},          // BRONZE MA
    {"BLACK BRONZE", 70, 24, -2},          // BRONZE JA
    {"RED COPPER", 50, 54, -1},            // BRONZE RI
    {"GOLD BRONZE", 90, 46, 0},           // BRONZE SU
    {"MOSS BRONZE", 130, 36, 1},           // BRONZE SY
    {"WHITE BRONZE", 70, 28, 1},          // BRONZE SS
    {"PALE BRASS", 100, 34, 1},            // BRASS FJ
    {"OLD BRASS", 90, 42, -1},             // BRASS HI
    {"BRASS", 90, 58, 1},                 // BRASS HE
    {"RED BRASS", 60, 52, 0},             // BRASS IM
    {"SUN BRASS", 80, 62, 0},             // BRASS DU
    {"DARK BRASS", 80, 42, -2},            // BRASS ST
    {"GREEN BRASS", 110, 44, 0},           // BRASS MA
    {"TEMPLE BRASS", 60, 48, -1},          // BRASS JA
    {"LEMON BRASS", 110, 60, 1},           // BRASS RI
    {"ROYAL BRASS", 70, 56, 1},           // BRASS SU
    {"LEAF BRASS", 110, 40, 1},            // BRASS SY
    {"STAR BRASS", 90, 30, 2},            // BRASS SS
    {"RIME IRON", 230, 12, -2},             // IRON FJ
    {"BOG IRON", 120, 6, -2},              // IRON HI
    {"BLUED IRON", 260, 24, 0},            // IRON HE
    {"BLACK IRON", 230, 20, -2},            // IRON IM
    {"GREY IRON", 320, 4, 0},             // IRON DU
    {"VIOLET IRON", 270, 24, -2},           // IRON ST
    {"MOSS IRON", 170, 26, -1},             // IRON MA
    {"INK IRON", 300, 18, -1},              // IRON JA
    {"SLATE IRON", 190, 12, 0},            // IRON RI
    {"NIGHT IRON", 230, 20, 0},            // IRON SU
    {"GREEN IRON", 130, 18, -1},            // IRON SY
    {"DUSK IRON", 240, 18, -1},             // IRON SS
    {"FROST STEEL", 230, 20, 0},           // STEEL1 FJ
    {"COBALT STEEL", 300, 20, -1},          // STEEL1 HI
    {"WHITE STEEL", 230, 10, 1},           // STEEL1 HE
    {"GREY STEEL", 320, 4, 0},            // STEEL1 IM
    {"GREEN STEEL", 130, 18, -1},           // STEEL1 DU
    {"SKY STEEL", 250, 34, 0},             // STEEL1 ST
    {"DARK STEEL", 160, 8, -1},            // STEEL1 MA
    {"TEAL STEEL", 200, 30, 0},            // STEEL1 JA
    {"AQUA STEEL", 210, 28, 1},            // STEEL1 RI
    {"BLACK STEEL", 290, 6, -1},           // STEEL1 SU
    {"JADE STEEL", 170, 26, 1},            // STEEL1 SY
    {"BLUE STEEL", 290, 26, 1},            // STEEL1 SS
    {"VIOLET SILVER", 300, 16, 2},         // SILVER1 FJ
    {"MINT SILVER", 140, 18, 2},           // SILVER1 HI
    {"ICE SILVER", 210, 8, 2},            // SILVER1 HE
    {"WHITE SILVER", 120, 8, 2},          // SILVER1 IM
    {"TEAL SILVER", 220, 18, 1},           // SILVER1 DU
    {"PALE SILVER", 270, 4, 1},           // SILVER1 ST
    {"SKY SILVER", 240, 14, 1},            // SILVER1 MA
    {"BLUE SILVER", 260, 16, 2},           // SILVER1 JA
    {"PEARL SILVER", 340, 4, 2},          // SILVER1 RI
    {"LILAC SILVER", 300, 16, 1},          // SILVER1 SU
    {"JADE SILVER", 180, 18, 2},           // SILVER1 SY
    {"STAR SILVER", 300, 14, 2},           // SILVER1 SS
    {"JADE STEEL", 160, 36, 0},            // STEEL2 FJ
    {"VIOLET STEEL", 300, 36, 1},          // STEEL2 HI
    {"COBALT STEEL", 250, 46, 1},          // STEEL2 HE
    {"PURPLE STEEL", 300, 38, -1},          // STEEL2 IM
    {"TURQUOISE STEEL", 180, 34, 0},       // STEEL2 DU
    {"TEAL STEEL", 220, 28, 1},            // STEEL2 ST
    {"GREEN STEEL", 130, 34, -1},           // STEEL2 MA
    {"DARK JADE STEEL", 160, 30, -1},       // STEEL2 JA
    {"EMERALD STEEL", 130, 42, 0},         // STEEL2 RI
    {"INDIGO STEEL", 270, 30, -1},          // STEEL2 SU
    {"LEAF STEEL", 130, 38, 0},            // STEEL2 SY
    {"BLUE STEEL", 280, 42, 0},            // STEEL2 SS
    {"JADE SILVER", 180, 20, 2},           // SILVER2 FJ
    {"LILAC SILVER", 310, 12, 2},          // SILVER2 HI
    {"MINT SILVER", 140, 24, 2},           // SILVER2 HE
    {"SKY SILVER", 260, 14, 2},            // SILVER2 IM
    {"SEAFOAM SILVER", 150, 12, 2},        // SILVER2 DU
    {"TEAL SILVER", 220, 18, 2},           // SILVER2 ST
    {"GREEN SILVER", 130, 12, 2},          // SILVER2 MA
    {"EMERALD SILVER", 150, 22, 1},        // SILVER2 JA
    {"COBALT SILVER", 290, 22, 2},         // SILVER2 RI
    {"WHITE SILVER", 160, 6, 2},          // SILVER2 SU
    {"AQUA SILVER", 220, 22, 1},           // SILVER2 SY
    {"STAR SILVER", 250, 24, 2},           // SILVER2 SS
    {"SEA GLASS", 180, 32, 0},             // GLASS2 FJ
    {"SEA GLASS", 180, 32, 0},             // GLASS2 HI
    {"SEA GLASS", 190, 32, 0},             // GLASS2 HE
    {"SEA GLASS", 190, 32, 0},             // GLASS2 IM
    {"SEA GLASS", 180, 32, 0},             // GLASS2 DU
    {"SEA GLASS", 180, 32, 0},             // GLASS2 ST
    {"SEA GLASS", 190, 32, 0},             // GLASS2 MA
    {"SEA GLASS", 180, 30, 0},             // GLASS2 JA
    {"SEA GLASS", 190, 32, 0},             // GLASS2 RI
    {"SEA GLASS", 190, 32, 0},             // GLASS2 SU
    {"GREEN GLASS", 150, 42, 0},           // GLASS2 SY
    {"VIOLET GLASS", 280, 40, 0},          // GLASS2 SS
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

}  // namespace doll
}  // namespace art
