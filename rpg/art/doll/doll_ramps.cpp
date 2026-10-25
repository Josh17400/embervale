// EMBERVALE art: the doll's ramps (DOLL). STYLE_P2 section 6: every material a 5-step hue-shifted ramp (0 outline,
// 1 shadow, 2 base, 3 light, 4 highlight). A3's 14 hand ramps are the generator's calibration set (gate: delta E 6 per
// step, CIE76; rpg_test --doll --gates measures it) and, with A3's villager swaps (docs/ref/style_a3/palettes.json), the
// first entries of the curated tables.
//
// The generator (rampGenerate) works in CIE Lab in two layers:
//   1. a smooth model per material and step, fitted by least squares on every A3 hand ramp of that material (the 14 and
//      the swaps; leather, metal, skin and hair regularised toward cloth): L' = L + dL + sL (L - 60); (a', b') = kc *
//      rot(theta) (a, b) + (ta, tb). Shadows turn toward violet (ta >= 0, tb <= 0), lights toward yellow (ta >= -2).
//   2. A3's own residuals: each hand ramp's step minus the model's, added back weighted by a Gaussian of the Lab distance
//      between the base and that ramp's base (sigma 8, prior weight 0.05). Near an A3 colour the generator gives A3's hand
//      choices back; far from every A3 colour it is the smooth model. (Leave-one-out, the model alone predicts an unseen
//      A3 ramp within dE 12 per step: the hand ramps vary that much; tools: the fit is reproducible from the A3 data.)
//   Then the order of lightness is enforced (a step never crosses its neighbour; dark bases compress toward a floor and
//   never reach #000000) and step 0 is pushed down until its luma is 70 or less and 40 below the base's where it can be.
//
// The curated tables (rampsOf): skin from A3's four hand skins (hero, woman, traveller, Mira) interpolated by lightness
// (the culture's 12 steps and the creator's 8 all land on it), with the mouth, lip and blush interpolated alongside; elf
// tints; hair by family (black, dark brown, brown, auburn, copper, wheat, flax, platinum, grey, plum: A3's six hand hair
// ramps and four in their manner), elder greys; the creator's six eye colours as iris + glint pairs; six leathers; four
// trim metals (brass, gold, silver, bronze). Everything else (cloth, headwear, cloaks, patterns) is the generator's.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include "rpg/art/doll/doll.h"
#include "rpg/art/doll/doll_forge.h"

namespace art {
namespace doll {

namespace {

constexpr uint32_t hex(uint32_t rgb) { return 0xFF000000u | (rgb & 0xFF) << 16 | (rgb & 0xFF00) | (rgb >> 16 & 0xFF); }   // #RRGGBB -> rgba

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

// ---------------------------------------------------------------- the hand ramps (A3's 14, then the villager swaps)
struct Hand { const char* name; Material m; uint32_t c[5]; bool gate; };
const std::vector<Hand>& hands() {
  static const std::vector<Hand> t = {
      {"hero hair", Material::Hair, {0x3A1622, 0x742E2C, 0xAA4A2C, 0xD9733A, 0xF5B060}, true},
      {"hero skin", Material::Skin, {0x5E2A36, 0xB8644F, 0xE9A077, 0xF8C597, 0xFFE6C2}, true},
      {"tunic", Material::Cloth, {0x162540, 0x1F4A68, 0x28728C, 0x3E9FA7, 0x8BD8C6}, true},
      {"scarf", Material::Cloth, {0x44122C, 0x87232F, 0xC83E38, 0xF47650, 0}, true},
      {"leather", Material::Leather, {0x2C1621, 0x5B3129, 0x8D5536, 0xBF8249, 0xE7B672}, true},
      {"trousers", Material::Cloth, {0x1C1732, 0x33304E, 0x4F4B6B, 0x777493, 0}, true},
      {"steel", Material::Metal, {0x201C31, 0x525C7A, 0x95A2BC, 0xD5DEEA, 0xFFFFFF}, true},
      {"brass", Material::Metal, {0, 0x704020, 0xCB9034, 0xF8DA80, 0}, true},
      {"woman hair", Material::Hair, {0x3E2418, 0x8C5A2A, 0xC79346, 0xEBC26C, 0xFFEBA8}, true},
      {"woman skin", Material::Skin, {0x5E2C36, 0xC4705A, 0xEEAA86, 0xFCD2AA, 0xFFEAD0}, true},
      {"dress", Material::Cloth, {0x2C1030, 0x5E1D48, 0x922F60, 0xC24F76, 0xEC8E9E}, true},
      {"chemise", Material::Cloth, {0x4B3C55, 0xADA4C0, 0xDCD6E2, 0xFAF6EE, 0}, true},
      {"apron", Material::Cloth, {0x4E3A30, 0xA99070, 0xD9C69E, 0xF3E6C4, 0}, true},
      {"woman leather", Material::Leather, {0x2C1621, 0x5B3129, 0x8D5536, 0xBF8249, 0}, true},
      // the swaps (palettes.json: traveller, moss lad, Mira, Hedda)
      {"traveller hair", Material::Hair, {0x140C18, 0x2C2232, 0x45384C, 0x67566E, 0x94829C}, false},
      {"traveller skin", Material::Skin, {0x46202A, 0x8A4C3A, 0xB97856, 0xD69A72, 0xEEC096}, false},
      {"traveller cloth", Material::Cloth, {0x26100E, 0x54261A, 0x844024, 0xB26634, 0xDA9658}, false},
      {"traveller accent", Material::Cloth, {0x3A2808, 0x7A5814, 0xB88E1E, 0xE8C44A, 0}, false},
      {"moss hair", Material::Hair, {0x3A2A1A, 0x7A5E36, 0xB08E58, 0xD8BC84, 0xF4E2B0}, false},
      {"moss cloth", Material::Cloth, {0x18240E, 0x2E4418, 0x4A6A24, 0x6E9234, 0xA8C860}, false},
      {"moss accent", Material::Cloth, {0x141C3A, 0x24366A, 0x3A5AA4, 0x6E92D8, 0}, false},
      {"mira hair", Material::Hair, {0x150E1B, 0x2F2336, 0x4B394B, 0x6F566B, 0x9A829C}, false},
      {"mira skin", Material::Skin, {0x3E1C20, 0x7C4432, 0xA86A4A, 0xC98C64, 0xE2AE84}, false},
      {"mira cloth", Material::Cloth, {0x101830, 0x1E3062, 0x2E4E94, 0x4A74C2, 0x86A8E2}, false},
      {"hedda hair", Material::Hair, {0x2E0E10, 0x6A1E1A, 0x9E3422, 0xCC5A30, 0xF08E50}, false},
      {"hedda cloth", Material::Cloth, {0x2E1A08, 0x6A4012, 0xA06A1E, 0xCC9A34, 0xF0CC6A}, false},
      {"hedda apron", Material::Cloth, {0x4A4248, 0xA8A2AC, 0xDAD6DC, 0xF6F4F0, 0}, false},
  };
  return t;
}

// the smooth model: per material (Cloth, Leather, Metal, Skin, Hair) and step (0, 1, 3, 4): dL, sL, kc, theta, ta, tb
const double kFit[5][4][6] = {
    /* Cloth   */ {{-41.22274, -0.64433, 0.38187, -0.13736, 5.65362, -7.76672}, {-16.42163, -0.10400, 0.73547, -0.02993, 3.14477, -4.17977},
                   {13.90190, -0.10864, 1.00945, 0.03347, -2.00000, 4.74164}, {38.74912, 0.21530, 0.90667, -0.10813, -2.00000, 16.10471}},
    /* Leather */ {{-42.32171, -0.62222, 0.31374, -0.15628, 5.51187, -10.15596}, {-17.65824, -0.07911, 0.70280, -0.04648, 3.09220, -5.23794},
                   {15.04297, -0.13161, 1.09909, 0.08293, -1.79091, 7.37194}, {38.96120, 0.21103, 0.79840, 0.10919, -2.00000, 17.17834}},
    /* Metal   */ {{-48.99750, -0.69974, 0.37761, -0.12376, 7.83044, -7.62524}, {-27.83639, -0.12984, 0.67629, -0.10771, 4.21392, -7.78824},
                   {22.42787, -0.07397, 0.77622, 0.20139, -2.00000, 4.03927}, {33.32193, 0.17662, 0.93657, -0.07943, -0.17113, 14.29107}},
    /* Skin    */ {{-40.59975, -0.60731, 0.47589, -0.23284, 9.80452, -7.72363}, {-17.63238, -0.14330, 0.92401, -0.10066, 6.90794, -0.00000},
                   {12.20483, -0.07122, 0.82365, 0.13418, -2.00000, 1.41376}, {22.41670, -0.14953, 0.36477, 0.04462, -2.00000, 8.94388}},
    /* Hair    */ {{-43.20662, -0.68084, 0.36634, -0.41698, 0.59938, -2.49478}, {-19.79948, -0.28169, 0.73284, -0.19773, 0.00000, -0.02585},
                   {15.85169, 0.05162, 1.09967, 0.20818, 2.62778, -2.63827}, {30.02418, -0.07267, 0.94965, 0.50756, 5.82509, -3.97898}},
};
constexpr double kSigma = 8.0, kPriorW = 0.05, kL0 = 60.0;
const int kStepOf[4] = {0, 1, 3, 4};

Lab model(const Lab& b, int mat, int k) {
  const double* p = kFit[mat][k];
  const double c = std::cos(p[3]), s = std::sin(p[3]);
  return {b.L + p[0] + p[1] * (b.L - kL0), p[2] * (c * b.a - s * b.b) + p[4], p[2] * (s * b.a + c * b.b) + p[5]};
}
// A3's residuals against the model (computed once from the hand table)
struct Resid { int mat; int k; Lab base; Lab d; };
const std::vector<Resid>& residuals() {
  static const std::vector<Resid> v = [] {
    std::vector<Resid> out;
    for (const Hand& h : hands()) {
      if (!h.c[2]) continue;
      const Lab b = toLab(hex(h.c[2]));
      for (int k = 0; k < 4; k++) {
        const int st = kStepOf[k];
        if (!h.c[st]) continue;
        const Lab t = toLab(hex(h.c[st])), m = model(b, (int)h.m, k);
        out.push_back({(int)h.m, k, b, {t.L - m.L, t.a - m.a, t.b - m.b}});
      }
    }
    return out;
  }();
  return v;
}

void generate(uint32_t base, Material mat, uint32_t out[5]) {
  const int mi = std::min((int)mat, 4);
  const Lab b = toLab(base);
  Lab st[5];
  st[2] = b;
  for (int k = 0; k < 4; k++) {
    Lab l = model(b, mi, k);
    double nL = 0, na = 0, nb = 0, den = kPriorW;
    for (const Resid& r : residuals()) {
      if (r.mat != mi || r.k != k) continue;
      const double w = std::exp(-dist2(b, r.base) / (2 * kSigma * kSigma));
      if (w < 1e-6) continue;
      nL += w * r.d.L; na += w * r.d.a; nb += w * r.d.b; den += w;
    }
    l.L += nL / den; l.a += na / den; l.b += nb / den;
    st[kStepOf[k]] = l;
  }
  // the order of lightness: lights above the base, shadows below; a dark base compresses toward a floor (never black)
  for (int s = 3; s <= 4; s++) if (st[s].L < st[s - 1].L + 4) st[s].L = std::min(st[s - 1].L + 4, 100.0);
  const double L2 = st[2].L;
  double l1 = std::min(st[1].L, L2 - 4), l0 = std::min(st[0].L, l1 - 4);
  if (l0 < 4) l0 = 4;
  if (l1 < l0 + 3) l1 = L2 > l0 + 6 ? (l0 + L2) / 2 : l0 + 3;
  l1 = std::min(l1, L2 - 1);
  l0 = std::max(std::min(l0, l1 - 1), 1.5);
  st[1].L = l1; st[0].L = l0;
  out[2] = base | 0xFF000000u;
  for (int s : {1, 3, 4}) out[s] = fromLab(st[s]) | 0xFF000000u;
  // the outline: luma 70 or less and at least 40 below the base (STYLE_P2 6), as far as the base allows
  Lab l = st[0];
  uint32_t c = fromLab(l);
  const double bl = luma(base);
  for (int it = 0; it < 40 && (luma(c) > 70.0 || bl - luma(c) < 40.0) && l.L > 6; it++) { l.L -= 2; c = fromLab(l); }
  if ((c & 0xFFFFFFu) == 0) c = rgba(4, 2, 8);   // never #000000 (the sole line's Ink is the darkest colour)
  out[0] = c | 0xFF000000u;
}

// a generated ramp, cached (bakes call rampsOf for every look; the generator is pure)
void generateCached(uint32_t base, Material mat, uint32_t out[5]) {
  static std::unordered_map<uint64_t, std::array<uint32_t, 5>> cache;
  const uint64_t k = (uint64_t)(base & 0xFFFFFFu) | (uint64_t)mat << 32;
  auto it = cache.find(k);
  if (it != cache.end()) { for (int i = 0; i < 5; i++) out[i] = it->second[i]; return; }
  generate(base, mat, out);
  if (cache.size() > 8192) cache.clear();
  std::array<uint32_t, 5> a;
  for (int i = 0; i < 5; i++) a[i] = out[i];
  cache.emplace(k, a);
}

const std::vector<A3Ramp>& table() {
  static const std::vector<A3Ramp> t = [] {
    std::vector<A3Ramp> v;
    for (const Hand& h : hands()) {
      if (!h.gate) continue;
      A3Ramp r{h.name, h.m, {}, 0};
      for (int i = 0; i < 5; i++) { r.c[i] = h.c[i] ? hex(h.c[i]) : 0; r.steps += h.c[i] != 0; }
      v.push_back(r);
    }
    return v;
  }();
  return t;
}

// ---------------------------------------------------------------- the curated tables
// skin: A3's four hand skins by lightness, each with its mouth, lip and blush (A3's face values for that skin)
struct SkinAnchor { uint32_t c[5]; uint32_t mouth, lip, blush; };
const SkinAnchor kSkinAnchors[4] = {
    {{0x3E1C20, 0x7C4432, 0xA86A4A, 0xC98C64, 0xE2AE84}, 0x6A2A2C, 0xB85456, 0xC8705C},   // Mira
    {{0x46202A, 0x8A4C3A, 0xB97856, 0xD69A72, 0xEEC096}, 0x7E3238, 0xC45C62, 0xDC846E},   // the traveller (lip, blush between)
    {{0x5E2A36, 0xB8644F, 0xE9A077, 0xF8C597, 0xFFE6C2}, 0x8E3A40, 0xD46470, 0xF0987E},   // the hero
    {{0x5E2C36, 0xC4705A, 0xEEAA86, 0xFCD2AA, 0xFFEAD0}, 0xA0414A, 0xD46470, 0xF49A8A},   // the woman
};
struct SkinRamp { Lab st[5]; Lab mouth, lip, blush; };
SkinRamp anchorLab(int i) {
  SkinRamp r;
  for (int s = 0; s < 5; s++) r.st[s] = toLab(hex(kSkinAnchors[i].c[s]));
  r.mouth = toLab(hex(kSkinAnchors[i].mouth)); r.lip = toLab(hex(kSkinAnchors[i].lip)); r.blush = toLab(hex(kSkinAnchors[i].blush));
  return r;
}
Lab lerp(const Lab& a, const Lab& b, double t) { return {a.L + (b.L - a.L) * t, a.a + (b.a - a.a) * t, a.b + (b.b - a.b) * t}; }
Lab shiftL(Lab x, double d) { x.L = std::clamp(x.L + d, 2.0, 100.0); return x; }
// the curated skin ramp for a skin colour: A3's anchors interpolated by the base's lightness (beyond the lightest and
// the deepest anchors the end anchor moves in lightness, its steps compressed so the deepest never reach black)
SkinRamp curatedSkin(uint32_t skin) {
  const double L = toLab(skin).L;
  SkinRamp a[4];
  for (int i = 0; i < 4; i++) a[i] = anchorLab(i);
  // the anchors' bases by lightness: Mira < traveller < hero < woman
  SkinRamp r;
  if (L <= a[0].st[2].L) {
    const double d = L - a[0].st[2].L;   // negative: deeper than Mira
    const double k = std::clamp(1.0 + d / 80.0, 0.55, 1.0);   // the steps' spread narrows as the base deepens
    for (int s = 0; s < 5; s++) { r.st[s] = a[0].st[s]; r.st[s].L = L + (a[0].st[s].L - a[0].st[2].L) * k; r.st[s].a *= 0.85 + 0.15 * k; r.st[s].b *= 0.85 + 0.15 * k; }
    r.mouth = shiftL(a[0].mouth, d * 0.6); r.lip = shiftL(a[0].lip, d * 0.6); r.blush = shiftL(a[0].blush, d * 0.6);
  } else if (L >= a[3].st[2].L) {
    const double d = L - a[3].st[2].L;   // positive: paler than the woman
    for (int s = 0; s < 5; s++) { r.st[s] = shiftL(a[3].st[s], s <= 1 ? d * 0.5 : d); r.st[s].a *= std::max(0.6, 1.0 - d / 40.0); r.st[s].b *= std::max(0.6, 1.0 - d / 40.0); }
    r.mouth = shiftL(a[3].mouth, d * 0.3); r.lip = shiftL(a[3].lip, d * 0.3); r.blush = shiftL(a[3].blush, d * 0.4);
  } else {
    int i = 0;
    while (i < 2 && L > a[i + 1].st[2].L) i++;
    const double t = (L - a[i].st[2].L) / std::max(1e-6, a[i + 1].st[2].L - a[i].st[2].L);
    for (int s = 0; s < 5; s++) r.st[s] = lerp(a[i].st[s], a[i + 1].st[s], t);
    r.mouth = lerp(a[i].mouth, a[i + 1].mouth, t); r.lip = lerp(a[i].lip, a[i + 1].lip, t); r.blush = lerp(a[i].blush, a[i + 1].blush, t);
  }
  for (int s = 3; s <= 4; s++) if (r.st[s].L < r.st[s - 1].L + 3) r.st[s].L = std::min(100.0, r.st[s - 1].L + 3);
  return r;
}
// a skin colour far from every human skin (a draugr's, a goblin's: P2c) keeps the generator
bool humanSkin(uint32_t skin) {
  const Lab l = toLab(skin);
  const double h = std::atan2(l.b, l.a) * 57.29577951308232, C = std::hypot(l.a, l.b);
  return C >= 8 && C <= 60 && h >= 20 && h <= 85;
}

// hair by family: the key colour a look's hair is matched to (the creator's, the cultures' and v1's colours) and the
// curated ramp; the look's own difference from the key moves the ramp by half (so culture jitter still shows)
struct HairFam { const char* name; uint32_t key; uint32_t c[5]; };
const HairFam kHair[] = {
    {"black", 0x241E1E, {0x150E1B, 0x2F2336, 0x4B394B, 0x6F566B, 0x9A829C}},        // Mira's
    {"blueblack", 0x221E2C, {0x140C18, 0x2C2232, 0x45384C, 0x67566E, 0x94829C}},    // the traveller's
    {"darkbrown", 0x3E2C20, {0x1E1014, 0x3E2620, 0x5E3C2C, 0x84583C, 0xAE7E52}},
    {"brown", 0x6E4C30, {0x2A1618, 0x5A3424, 0x845030, 0xAE7444, 0xD29E62}},
    {"auburn", 0x96462A, {0x2E0E10, 0x6A1E1A, 0x9E3422, 0xCC5A30, 0xF08E50}},       // Hedda's
    {"copper", 0xC0682E, {0x3A1622, 0x742E2C, 0xAA4A2C, 0xD9733A, 0xF5B060}},       // the hero's
    {"wheat", 0xC4A064, {0x3E2418, 0x8C5A2A, 0xC79346, 0xEBC26C, 0xFFEBA8}},        // the woman's
    {"flax", 0xE2C88A, {0x3A2A1A, 0x7A5E36, 0xB08E58, 0xD8BC84, 0xF4E2B0}},         // the moss lad's
    {"platinum", 0xE6E0CC, {0x4A4248, 0x9E96A2, 0xD4CED4, 0xF2EEEA, 0xFFFFFF}},
    {"grey", 0x969496, {0x2C2832, 0x5E5966, 0x8E8A94, 0xBAB6BE, 0xE2E0E4}},
    {"silver", 0xC8C8C8, {0x3A3440, 0x7A7480, 0xB0ACB4, 0xDAD6DC, 0xF6F4F0}},
    {"plum", 0x78326E, {0x24102A, 0x4E2050, 0x78367A, 0xA458A2, 0xCE8AC8}},
};
constexpr int kHairFams = (int)(sizeof(kHair) / sizeof(kHair[0]));
int hairFamily(uint32_t col, double* d2out = nullptr) {
  const Lab l = toLab(col);
  int best = -1;
  double bd = 1e18;
  for (int i = 0; i < kHairFams; i++) {
    const double d = dist2(l, toLab(hex(kHair[i].key)));
    if (d < bd) { bd = d; best = i; }
  }
  if (d2out) *d2out = bd;
  return best;
}
void curatedHair(uint32_t col, uint32_t out[5]) {
  double d2 = 0;
  const int f = hairFamily(col, &d2);
  if (f < 0 || d2 > 30.0 * 30.0) { generateCached(col, Material::Hair, out); return; }
  const Lab l = toLab(col), k = toLab(hex(kHair[f].key));
  const Lab d{(l.L - k.L) * 0.4, (l.a - k.a) * 0.5, (l.b - k.b) * 0.5};
  for (int s = 0; s < 5; s++) {
    Lab x = toLab(hex(kHair[f].c[s]));
    x.L = std::clamp(x.L + d.L, 2.0, 100.0); x.a += d.a; x.b += d.b;
    out[s] = fromLab(x) | 0xFF000000u;
  }
  const double bl = luma(out[2]);
  Lab o = toLab(out[0]);
  for (int it = 0; it < 20 && (luma(out[0]) > 70.0 || bl - luma(out[0]) < 40.0) && o.L > 4; it++) { o.L -= 2; out[0] = fromLab(o) | 0xFF000000u; }
}

// eyes: the creator's six (STYLE_P2 6) as iris + glint (A3's pairs where A3 painted one)
struct EyePair { uint32_t iris, glint; };
const EyePair kEyes[] = {
    {0x603A20, 0xA0704A},   // brown (Mira's glint)
    {0x3C70C4, 0x8CB8EC},   // blue (Hedda's)
    {0x388C50, 0x9FDCA8},   // green (the woman's glint)
    {0x788492, 0xC4CCD6},   // grey
    {0xCE9228, 0xF4D27A},   // amber
    {0x8850B0, 0xC8A0E8},   // violet
    {0x3A78B0, 0x8FC6E8},   // the hero's blue
    {0x3F8A5A, 0x9FDCA8},   // the woman's green
    {0x4A3020, 0x8A6040},   // the traveller's brown
    {0x3A6A3A, 0x88C088},   // the moss lad's green
};
EyePair eyesFor(uint32_t col) {
  const Lab l = toLab(col);
  int best = 0;
  double bd = 1e18;
  for (int i = 0; i < (int)(sizeof(kEyes) / sizeof(kEyes[0])); i++) {
    const double d = dist2(l, toLab(hex(kEyes[i].iris)));
    if (d < bd) { bd = d; best = i; }
  }
  if (bd < 12.0 * 12.0) return {col | 0xFF000000u, hex(kEyes[best].glint)};
  uint32_t g[5];
  generateCached(col, Material::Cloth, g);
  return {col | 0xFF000000u, g[4] ? g[4] : g[3]};
}

// leathers: A3's brown and five more (4 browns, black, red-brown), picked per person
const uint32_t kLeather[6][5] = {
    {0x2C1621, 0x5B3129, 0x8D5536, 0xBF8249, 0xE7B672},   // A3's brown
    {0x1E1018, 0x45261F, 0x6A3E2A, 0x94603A, 0xBE8A5A},   // dark brown
    {0x180D09, 0x7A4A2C, 0xB07A44, 0xD8A866, 0xF2D294},   // tan (MOTION_P2 14: its outline luma 16, with the black's: the
                                                          // boot's edge parts from grass instead of melting into it)
    {0x2A1E22, 0x54403A, 0x7E6450, 0xA88A6C, 0xD0B892},   // grey-brown
    {0x141018, 0x2A2230, 0x443A48, 0x6A5E6C, 0x988C98},   // black
    {0x2E1018, 0x62261E, 0x96402C, 0xC2683E, 0xE8A066},   // red-brown
};
// trim metals: brass (A3's), gold, silver, bronze
const uint32_t kTrim[4][5] = {
    {0x3A1C12, 0x704020, 0xCB9034, 0xF8DA80, 0xFFF6D0},
    {0x3A2410, 0x8A5A14, 0xD8A32C, 0xF6D660, 0xFFF4C0},
    {0x2A2836, 0x6A6E80, 0xB4BAC8, 0xE4E8F0, 0xFFFFFF},
    {0x2E1A10, 0x6E3E1C, 0xA8682E, 0xD49A52, 0xF2CC8A},
};
void setHex(uint32_t dst[5], const uint32_t src[5]) { for (int i = 0; i < 5; i++) dst[i] = hex(src[i]); }

constexpr double kTrouserFloor = 52.0;   // A3's trouser body #33304E (MOTION_P2 14)
constexpr double kTrouserBody = 101.0;   // the lit trouser body over the game's grass (luma 89 + 12; run fix i1)

uint32_t mixHash(uint32_t h, uint32_t v) { h ^= v + 0x9E3779B9u + (h << 6) + (h >> 2); return h; }

// ---- (P2c ladder, contracts 3b; STYLE_P2 25.2, 27.3) the metals a piece is drawn in (HumanLook::metalKind) and their glow
// iron dark and matte, steel A3's, gilded the trim gold, obsidian violet-black, emberforged forge red, starmetal pale
// blue-white, jade green, bronze warm gold-brown; the glow colours: warm gold on gilded and bronze, cold blue-white on
// steel, iron and starmetal, violet on obsidian, ember on emberforged and Mythic, green on jade
const uint32_t kMetalKind[12][5] = {
    {0x201C31, 0x525C7A, 0x95A2BC, 0xD5DEEA, 0xFFFFFF},   // 0 legacy: A3's steel
    {0x2C1621, 0x5B3129, 0x8D5536, 0xBF8249, 0xE7B672},   // 1 leather (A3's)
    {0x24140A, 0x5A3E10, 0x8C6A1E, 0xE8D8A8, 0xFFF6DC},   // (silhouettes i2, the blind panel: "Bronze brown with weak specular, drifts toward leather": the body olive-gold, 8 degrees off the leather's orange-brown)   // (round 2: the body under the skin box, C17)   // (forge C17: the lit plane cream, the light loses the orange)
      // 2 bronze (alloys i2: an orange body and a hard cream glint; i1's peach top read as skin)
    {0x16141A, 0x52525C, 0x7E7C86, 0xC0BCBA, 0xE6E2DA},   // (forge C17: a hard lit plane over the core; iron keeps the warm grey)
      // 3 iron (C14's edge step and body; alloys i3: a DULL lit plane, a mid grey where steel's is near-white: "iron and steel the same grey at 1x")
    {0x201C31, 0x525C7A, 0x95A2BC, 0xD5DEEA, 0xFFFFFF},   // 4 steel (A3's)
    {0x3A2A08, 0x6E5410, 0xC8A020, 0xF6E6A6, 0xFFFAE4},   // (forge C17: a pale gilt plane, the core deeper)
      // 5 gilded (alloys i3: a lemon gold, apart from the bronze's orange at game scale; its body 18 luma under the skin)
    {0x0E2A22, 0x1F5A46, 0x3E9474, 0x7CC8A0, 0xD0F4E0},   // 6 jade
    {0x0A0810, 0x181424, 0x2A2440, 0x82808F, 0xF2F6FF},   // (silhouettes i2: the lit facet violet-cold #82808F (C17: its chroma 60 % of the body's), not i1's grey #868596 that read as dark iron) (forge round 2, STYLE_P2 28.4 KNAPPED: near-black violet glass, cold lit facets, hard white glints)
      // 7 obsidian (alloys i2: black glass, a near-white glint; the violet read as cloth)
    {0x120C0E, 0x2A1614, 0x4A2622, 0x786A5E, 0xFFD8A8},   // (silhouettes i3, the blind panel: "a cold iron stove": the char holds its heat, a red-black body under a warm lit plane) (silhouettes i2: black iron, i1's taupe #887868 lit plane read as clay; the heat is the seams') (forge round 2, STYLE_P2 28.4 FURNACE: soft char, the heat only in the seams' Glow; i3:
      // the face plate as skin: a forge-dark red body, the lit plane a yellow straw heat (C17 keeps its chroma under 60 % of the body: an orange went pink), the glint white-hot)
      // 8 emberforged (alloys i2: redder, a yellow-hot glint, apart from bronze)
    {0x1C2A4C, 0x3E5CA0, 0x8CB2F2, 0xCCE4FF, 0xFFFFFF},   // (forge C17: the core under the crystal plane)
      // 9 starmetal (alloys i1: bluer, it read as steel beside it)
    {0x201C31, 0x525C7A, 0x95A2BC, 0xD5DEEA, 0xFFFFFF},   // 10 alloy: from the tint (this row is the fallback)
    // 11 mythic (ladder i3; i2's crimson-black read as "rust", "a red cap", "a step down" to all five of the blind panel):
    // BLACKENED steel, a cold gunmetal whose highlight runs white-hot at the edge, so the ember runes, the gold trim and the
    // crimson cloak carry the colour and burn on it (dark metal, bright fire: the top reads by contrast, never by a tint)
    {0x0C080E, 0x3C3846, 0x625E70, 0xB0AEB4, 0xFFF0E0},   // (forge C17: the blackened steel's lit edge neutral)
   
};
const uint32_t kGlowOf[12] = {0xBFE4FF, 0xFFC860, 0xFFC860, 0xBFE4FF, 0xBFE4FF, 0xFFD060, 0x7CF0B0, 0xC890FF, 0xFF7A24, 0xD8ECFF, 0xBFE4FF, 0xFF6828};   // (round 2: ember #FF7A24)
// ---- (alloys i1) the culture alloys' metal families: doll_alloy.cpp (in rpg_doll, so the gear ladder links without the painters)
// (ladder i2) the glow is LIGHT: bright and saturated in its own hue (a grey or a tint, e.g. an alloy's pale light key,
// would read as more metal: the panel saw "a few white sparkle pixels"), and it parts from the gold trim it sits beside
// (a warm glow on gold gilding vanishes): then the steel's cold blue-white light
uint32_t glowLight(uint32_t base, bool goldTrim, bool fire = false) {
  Lab g = toLab(base);
  double c = std::hypot(g.a, g.b);
  const double hue = std::atan2(g.b, g.a) * 57.29578;   // gold sits near 70-95
  if (c < 12.0 || (goldTrim && hue > 40.0 && hue < 110.0)) { g = toLab(hex(0x7FD4FF)); c = std::hypot(g.a, g.b); }
  const double want = std::max(c, 48.0);
  g.a *= want / std::max(c, 1e-6); g.b *= want / std::max(c, 1e-6);
  // at rest a deep, saturated light (the in-game breath adds up to 0.85 of it again: a rest colour already near white
  // saturated and the pulse read as nothing, the panel's "brightness within 0.3/255"); the peak lands bright
  g.L = std::clamp(g.L, 54.0, 62.0);
  // (ladder i3) the Mythic's fire rests HOT (amber, L 68): on the blackened steel a deep orange read as rust, never as flame
  if (fire) {
    const Lab amber = toLab(hex(0xFF9A2A));
    g.a = amber.a; g.b = amber.b; g.L = 68.0;
  }
  return fromLab(g) | 0xFF000000u;
}
// a ramp's brightness scaled (RGB, hue kept): the leather's parting from the cloth (C14)
double alloyTint(uint32_t c) { const Lab l = toLab(c); return std::hypot(l.a, l.b); }   // a mid step's chroma
uint32_t scaleRgb(uint32_t c, double k) {
  auto ch = [&](int v) { return (int)std::clamp(std::lround(v * k), 0L, 255L); };
  return rgba(ch(chR(c)), ch(chG(c)), ch(chB(c)));
}

}  // namespace

const std::vector<A3Ramp>& a3Ramps() { return table(); }
void rampGenerate(uint32_t base, Material m, uint32_t out[5]) { generate(base, m, out); }

void metalKindRamp(int kind, uint32_t out[5]) { setHex(out, kMetalKind[std::clamp(kind, 0, 11)]); }
void rampA3(uint32_t base, Material m, uint32_t out[5]) {
  // A3's hand ramps are the curated tables' first entries: their bases give them back exactly
  for (const A3Ramp& r : table())
    if (r.material == m && (r.c[2] & 0xFFFFFFu) == (base & 0xFFFFFFu) && r.steps == 5) {
      for (int i = 0; i < 5; i++) out[i] = r.c[i];
      return;
    }
  generateCached(base, m, out);
  // a hand ramp with fewer drawn steps keeps its drawn ones (the generator fills the rest)
  for (const A3Ramp& r : table())
    if (r.material == m && (r.c[2] & 0xFFFFFFu) == (base & 0xFFFFFFu))
      for (int i = 0; i < 5; i++) if (r.c[i]) out[i] = r.c[i];
}

DollRamps a3Palette(int who) {
  DollRamps R;
  auto set = [&](Slot s, std::initializer_list<uint32_t> c) { int i = 0; for (uint32_t v : c) R.c[(int)s][i++] = v ? hex(v) : 0; };
  auto fill = [&](Slot s, Material m) {   // the steps A3 did not draw come from the generator (never seen in A3's frames)
    uint32_t g[5];
    generateCached(R.c[(int)s][2], m, g);
    for (int i = 0; i < 5; i++) if (!R.c[(int)s][i]) R.c[(int)s][i] = g[i];
  };
  set(Slot::Ink, {0x21152A, 0x21152A, 0x21152A, 0x21152A, 0x21152A});
  set(Slot::Fx, {0x7FA8D8, 0xC9E6F4, 0xFFFDF4, 0xFFFDF4, 0xFFFDF4});
  set(Slot::Leather, {0x2C1621, 0x5B3129, 0x8D5536, 0xBF8249, 0xE7B672});
  set(Slot::Metal, {0x201C31, 0x525C7A, 0x95A2BC, 0xD5DEEA, 0xFFFFFF});
  set(Slot::Metal2, {0, 0x704020, 0xCB9034, 0xF8DA80, 0});
  fill(Slot::Metal2, Material::Metal);
  if (who == 0) {
    set(Slot::Face, {0x24142A, 0xF6EEE3, 0x8E3A40, 0xD46470, 0xF0987E});
    set(Slot::Eyes, {0x1E3A5A, 0x2C5C8C, 0x3A78B0, 0x8FC6E8, 0xC8E4F4});
    set(Slot::Hair, {0x3A1622, 0x742E2C, 0xAA4A2C, 0xD9733A, 0xF5B060});
    set(Slot::Skin, {0x5E2A36, 0xB8644F, 0xE9A077, 0xF8C597, 0xFFE6C2});
    set(Slot::Cloth1, {0x162540, 0x1F4A68, 0x28728C, 0x3E9FA7, 0x8BD8C6});
    set(Slot::Cloth2, {0x1C1732, 0x33304E, 0x4F4B6B, 0x777493, 0});
    set(Slot::Accent, {0x44122C, 0x87232F, 0xC83E38, 0xF47650, 0});
    set(Slot::Linen, {0x4B3C55, 0xADA4C0, 0xDCD6E2, 0xFAF6EE, 0});
    set(Slot::Cloth3, {0x4E3A30, 0xA99070, 0xD9C69E, 0xF3E6C4, 0});
  } else {
    set(Slot::Face, {0x2A1424, 0xF7EFE4, 0xA0414A, 0xD46470, 0xF49A8A});
    set(Slot::Eyes, {0x1E4630, 0x2E6A46, 0x3F8A5A, 0x9FDCA8, 0xD0F0D4});
    set(Slot::Hair, {0x3E2418, 0x8C5A2A, 0xC79346, 0xEBC26C, 0xFFEBA8});
    set(Slot::Skin, {0x5E2C36, 0xC4705A, 0xEEAA86, 0xFCD2AA, 0xFFEAD0});
    set(Slot::Cloth1, {0x2C1030, 0x5E1D48, 0x922F60, 0xC24F76, 0xEC8E9E});
    set(Slot::Cloth2, {0x1C1732, 0x33304E, 0x4F4B6B, 0x777493, 0});
    set(Slot::Accent, {0x162540, 0x1F4A68, 0x28728C, 0x3E9FA7, 0x8BD8C6});   // the ribbon: the hero's teal
    set(Slot::Linen, {0x4B3C55, 0xADA4C0, 0xDCD6E2, 0xFAF6EE, 0});
    set(Slot::Cloth3, {0x4E3A30, 0xA99070, 0xD9C69E, 0xF3E6C4, 0});
  }
  fill(Slot::Cloth2, Material::Cloth);
  fill(Slot::Accent, Material::Cloth);
  fill(Slot::Linen, Material::Cloth);
  fill(Slot::Cloth3, Material::Cloth);
  for (Slot s : {Slot::Head, Slot::Cloak, Slot::Pattern, Slot::Plume, Slot::Field, Slot::Device, Slot::Glow})
    for (int i = 0; i < 5; i++) R.c[(int)s][i] = R.c[(int)Slot::Accent][i];
  for (int i = 0; i < 5; i++) R.c[(int)Slot::Blade][i] = R.c[(int)Slot::Metal][i];   // (P2c ladder) the blade: A3's steel
  return R;
}

DollRamps rampsOf(const HumanLook& L) {
  DollRamps R = a3Palette(L.body == 1 ? 1 : 0);
  // skin: the curated ramp from A3's four skins (elves a shade paler and cooler); the face's mouth, lip and blush with it
  if (humanSkin(L.skin)) {
    SkinRamp s = curatedSkin(L.skin);
    if (L.people == 2)
      for (Lab* x : {&s.st[0], &s.st[1], &s.st[2], &s.st[3], &s.st[4], &s.blush}) { x->L = std::min(100.0, x->L + 2.5); x->a -= 3.0; x->b -= 4.5; }
    for (int i = 0; i < 5; i++) R.c[(int)Slot::Skin][i] = fromLab(s.st[i]) | 0xFF000000u;
    R.c[(int)Slot::Face][2] = fromLab(s.mouth) | 0xFF000000u;
    R.c[(int)Slot::Face][3] = fromLab(s.lip) | 0xFF000000u;
    R.c[(int)Slot::Face][4] = fromLab(s.blush) | 0xFF000000u;
  } else {
    rampA3(L.skin, Material::Skin, R.c[(int)Slot::Skin]);
  }
  // hair: the curated family; elders grey (dark hair to iron grey, fair hair to white)
  uint32_t hairCol = L.hairColor;
  if (L.age == 3) hairCol = toLab(L.hairColor).L < 45 ? hex(0x8E8A94) : hex(0xD8D4D8);
  curatedHair(hairCol, R.c[(int)Slot::Hair]);
  rampA3(L.topColor, Material::Cloth, R.c[(int)Slot::Cloth1]);
  rampA3(L.bottomColor, Material::Cloth, R.c[(int)Slot::Cloth2]);
  // MOTION_P2 14: the trouser-body floor. A trouser body (Cloth2 step 1, A3's leg char '2') darker than A3's own (#33304E,
  // luma 52) reads as a shadow band on grass; its base is lifted (RGB scaled: HSV hue and saturation kept) to the smallest
  // value whose generated step 1 reaches luma 52. Brighter picks are untouched; darker picks all land on the floor, so the
  // mapping stays monotonic. Rig 2 only (appearance.h, rig 1 and A3's palettes never pass through here).
  if (luma(R.c[(int)Slot::Cloth2][1]) < kTrouserFloor) {
    const uint32_t b = (L.bottomColor & 0xFFFFFFu) ? L.bottomColor : rgba(1, 1, 1);   // black: a neutral grey lift
    const int mx = std::max({chR(b), chG(b), chB(b)});
    auto scaled = [&](double k) {
      auto ch = [&](int v) { return (int)std::clamp(std::lround(v * k), 0L, 255L); };
      return rgba(ch(chR(b)), ch(chG(b)), ch(chB(b)));
    };
    double lo = 1.0, hi = 255.0 / mx;
    uint32_t g[5];
    generateCached(scaled(hi), Material::Cloth, g);
    if (luma(g[1]) >= kTrouserFloor) {
      for (int it = 0; it < 24; it++) {
        const double mid = (lo + hi) * 0.5;
        generateCached(scaled(mid), Material::Cloth, g);
        (luma(g[1]) >= kTrouserFloor ? hi : lo) = mid;
      }
    }
    const uint32_t ownOutline = R.c[(int)Slot::Cloth2][0];   // the pick's own step 0, before the lift
    generateCached(scaled(hi), Material::Cloth, R.c[(int)Slot::Cloth2]);
    // the floor is a BODY floor: the outline keeps the pick's own step 0 (darker than the lifted ramp's), so the leg's
    // silhouette ring stays as dark against grass as the pick drew it (STYLE_P2 8: the ring 40 luma or more from the
    // ground). Still monotonic: a darker pick has a darker own outline.
    if (luma(ownOutline) < luma(R.c[(int)Slot::Cloth2][0])) R.c[(int)Slot::Cloth2][0] = ownOutline;
  }
  rampA3(L.tabardColor, Material::Cloth, R.c[(int)Slot::Accent]);
  // (P2c forge i2) HOMESPUN's hood (doll_resolve styleKitOf: the hood in the helm's place): the gambeson's cloth (below)
  const bool homespunHood = !L.headColor && (L.forge[HumanLook::TIER_HELM] & 0xFF) == 1;
  rampA3(L.headColor ? L.headColor : homespunHood ? hex(0x8C7A5C) : L.topColor, Material::Cloth, R.c[(int)Slot::Head]);
  rampA3(L.cloakColor, Material::Cloth, R.c[(int)Slot::Cloak]);
  rampA3(L.patternColor ? L.patternColor : L.tabardColor, Material::Cloth, R.c[(int)Slot::Pattern]);
  rampA3(L.plumeColor ? L.plumeColor : L.tabardColor, Material::Cloth, R.c[(int)Slot::Plume]);
  rampA3(L.shieldField ? L.shieldField : L.tabardColor, Material::Cloth, R.c[(int)Slot::Field]);
  rampA3(L.shieldDevice ? L.shieldDevice : hex(0xCB9034), Material::Metal, R.c[(int)Slot::Device]);
  // leather: one of six per person (a stable pick from the person's colours; A3's brown the most common)
  {
    const uint32_t h = mixHash(mixHash(mixHash(0x1EA7u, L.skin), L.topColor), L.bottomColor);
    static const int pick[10] = {0, 0, 0, 1, 1, 2, 3, 4, 5, 2};
    setHex(R.c[(int)Slot::Leather], kLeather[pick[h % 10]]);
  }
  // (P2b motion prototype, 2026-10-21) the trousers part from the boots: a trouser body (Cloth2 step 2, the lit leg tone)
  // within 28 luma of the boot's and within 40 degrees of its hue (brown trousers over brown boots, the default hero) reads
  // as one dark column, so a walking leg loses its boot top; its base is lifted the floor's way (RGB scaled, hue and
  // saturation kept) to the smallest value whose step 2 sits 28 luma over the boot's and whose step 1 keeps the floor.
  // The outline keeps the pick's own step 0 (the silhouette ring as dark as drawn). Rig 2 only.
  {
    uint32_t* T = R.c[(int)Slot::Cloth2];
    const uint32_t* B = R.c[(int)Slot::Leather];
    const Lab lt = toLab(T[2]), lb = toLab(B[2]);
    double dh = std::abs(std::atan2(lt.b, lt.a) - std::atan2(lb.b, lb.a)) * 57.29578;
    if (dh > 180.0) dh = 360.0 - dh;
    const double want = luma(B[2]) + 28.0;
    if (luma(T[2]) < want && luma(T[2]) > luma(B[2]) - 28.0 && dh < 40.0) {
      const uint32_t b = (L.bottomColor & 0xFFFFFFu) ? L.bottomColor : rgba(1, 1, 1);
      const int mx = std::max({chR(b), chG(b), chB(b), 1});
      auto scaled = [&](double k) {
        auto ch = [&](int v) { return (int)std::clamp(std::lround(v * k), 0L, 255L); };
        return rgba(ch(chR(b)), ch(chG(b)), ch(chB(b)));
      };
      auto good = [&](const uint32_t g[5]) { return luma(g[2]) >= want && luma(g[1]) >= kTrouserFloor; };
      double lo = 1.0, hi = 255.0 / mx;
      uint32_t g[5];
      generateCached(scaled(hi), Material::Cloth, g);
      if (good(g)) {
        for (int it = 0; it < 24; it++) {
          const double mid = (lo + hi) * 0.5;
          generateCached(scaled(mid), Material::Cloth, g);
          (good(g) ? hi : lo) = mid;
        }
        const uint32_t ownOutline = T[0];
        generateCached(scaled(hi), Material::Cloth, T);
        if (luma(ownOutline) < luma(T[0])) T[0] = ownOutline;
      }
    }
  }
  // (P2b run fix i1, 2026-10-22; the owner: the woman's navy trousers "nearly vanish against grass and merge with her dark
  // boots") the trousers' light steps part from the grass: a trouser body (Cloth2 step 2, the lit leg's body; the near leg
  // shows steps 3 and 2, the far leg 2 and 1) under kTrouserBody (the game's grass #4A663A is luma 89: 12 over it) is
  // lifted the floor's way (RGB scaled, hue and saturation kept) to the smallest value whose step 2 reaches it, so the
  // near leg's light step 3 sits 40+ over the grass and the body over the boot's shadow. Darker picks all land on it
  // (monotonic). The outline keeps the pick's own step 0 (the silhouette ring as dark as drawn) and step 1 keeps the
  // floor. Rig 2 only.
  {
    uint32_t* T = R.c[(int)Slot::Cloth2];
    if (luma(T[2]) < kTrouserBody) {
      const uint32_t b = (L.bottomColor & 0xFFFFFFu) ? L.bottomColor : rgba(1, 1, 1);
      const int mx = std::max({chR(b), chG(b), chB(b), 1});
      auto scaled = [&](double k) {
        auto ch = [&](int v) { return (int)std::clamp(std::lround(v * k), 0L, 255L); };
        return rgba(ch(chR(b)), ch(chG(b)), ch(chB(b)));
      };
      auto good = [&](const uint32_t g[5]) { return luma(g[2]) >= kTrouserBody && luma(g[1]) >= kTrouserFloor; };
      double lo = 1.0, hi = 255.0 / mx;
      uint32_t g[5];
      generateCached(scaled(hi), Material::Cloth, g);
      if (good(g)) {
        for (int it = 0; it < 24; it++) {
          const double mid = (lo + hi) * 0.5;
          generateCached(scaled(mid), Material::Cloth, g);
          (good(g) ? hi : lo) = mid;
        }
        const uint32_t ownOutline = T[0];
        generateCached(scaled(hi), Material::Cloth, T);
        if (luma(ownOutline) < luma(T[0])) T[0] = ownOutline;
      }
    }
  }
  // trim: the nearest curated metal to the look's trim colour (brass by default)
  if (L.trimColor) {
    const Lab t = toLab(L.trimColor);
    int best = 0;
    double bd = 1e18;
    for (int i = 0; i < 4; i++) { const double d = dist2(t, toLab(hex(kTrim[i][2]))); if (d < bd) { bd = d; best = i; } }
    if (bd < 30.0 * 30.0) setHex(R.c[(int)Slot::Metal2], kTrim[best]);
    else rampA3(L.trimColor, Material::Metal, R.c[(int)Slot::Metal2]);
  }
  if (L.glowColor) rampA3(L.glowColor, Material::Metal, R.c[(int)Slot::Glow]);
  // (P2c ladder, contracts 3b) the blade's own slot defaults to the armour's metal (a look with no tier: A3's steel)
  for (int i = 0; i < 5; i++) R.c[(int)Slot::Blade][i] = R.c[(int)Slot::Metal][i];
  if (const int top = L.topTier()) {
    // Metal from the alloy's tint (art::metalRampKeys, as the icons), else from the metal kind, else A3's steel
    const int mk = std::clamp((int)L.metalKind, 0, 11);
    auto metalRamp = [&](int kind, uint32_t out[5]) {
      // (P2c forge, D29) a family with no tint (a census officer in his realm's alloy: ladder fields only) draws that family
      if (kind == 10 && (L.armourTint || L.alloyFam)) alloyRamp(L.alloyFam ? L.alloyFam - 1 : alloyFamilyOf(L.armourTint, L.sheen), L.sheen, out);
      else setHex(out, kMetalKind[std::clamp(kind, 0, 11)]);
      for (int i = 0; i < 5; i++) out[i] |= 0xFF000000u;
    };
    // (ladder i2, D20) a heartland high metal (gilded, jade, obsidian, ember, starmetal) draws as itself, not gear_look's band tint
    // (which draws a band-7 STARMETAL piece as ember); an alloy (10) or a look with no kind takes the tint
    if (L.armourTint && (mk == 10 || mk <= 4)) {
      if (mk == 10) alloyRamp(L.alloyFam ? L.alloyFam - 1 : alloyFamilyOf(L.armourTint, L.sheen), L.sheen, R.c[(int)Slot::Metal]);
      else metalRampKeys(L.armourTint, L.armourTint2, L.sheen, R.c[(int)Slot::Metal]);
    } else if (mk == 1) metalRamp(3, R.c[(int)Slot::Metal]);   // (polish i3, LADDER-ARMS: "T2's sword and boss draw in the
    // leather's browns") a leather wearer's metal is his fittings, the boss, the blade: plain iron (the leather has its own slot)
    else if (mk) metalRamp(mk, R.c[(int)Slot::Metal]);
    for (int i = 0; i < 5; i++) R.c[(int)Slot::Metal][i] |= 0xFF000000u;
    // (forge round 2, C17; STYLE_P2 28.8) the read at 1x: warm metals to gold and out of the skin box, greens cold (the alloy
    // model's own rule; first, so the partings from the skin and the hair below see the moved ramp)
    if (L.forge[HumanLook::TIER_BODY] && mk == 10) metalRound2(R.c[(int)Slot::Metal], L.sheen);
    // (alloys i1, C14) an alloy's lit body parts from the skin it touches (a warm copper or brass at the skin's value read
    // as bare arms): its mid step moves darker, the glint and the core kept, until 18 luma apart
    // (P2c forge) every metal (the forge looks wear each band metal on the same face: starmetal's body sat on the skin, steel's
    // on the heroine's wheat hair) parts from the skin and the hair it touches the same way
    // (forge i2) and the Heartland kit's styled steel (C14: fm_steel's body sat 16 luma off the skin, the heroine's on her
    // wheat hair); another people's steel keeps A3's ramp (B8: imp3's moved body split a shared colour, 49 of 48)
    if (mk == 10 || (mk >= 2 && mk != 4) || (mk == 4 && L.forge[HumanLook::TIER_BODY] && L.forgeKit)) {
      uint32_t* M = R.c[(int)Slot::Metal];
      const double sk = luma(R.c[(int)Slot::Skin][2]), hr = luma(R.c[(int)Slot::Hair][2]);
      auto clash = [&]() { return std::fabs(luma(M[2]) - sk) < 18.0 || std::fabs(luma(M[2]) - hr) < 18.0; };
      // the smaller move that clears both (darker, or lighter while the lit plane stays 23 luma over it), else as before
      const uint32_t m0 = M[2];
      int dn = -1, up = -1;
      for (int it = 0; it <= 16 && dn < 0; it++) { if (!clash()) dn = it; else if (luma(M[2]) - luma(M[1]) > 14.0) M[2] = scaleRgb(M[2], 0.95); else break; }
      const uint32_t mDn = M[2];
      M[2] = m0;
      // (a dark sheen's body never lightens past luma 90: C17's blackened tell)
      for (int it = 0; it <= 16 && up < 0; it++) {
        if (!clash()) up = it;
        else if (luma(M[3]) - luma(scaleRgb(M[2], 1.05)) >= 23.0 && (L.sheen != 3 || luma(scaleRgb(M[2], 1.05)) <= 90.0)) M[2] = scaleRgb(M[2], 1.05);
        else break;
      }
      const uint32_t mUp = M[2];
      M[2] = (up >= 0 && (dn < 0 || up < dn)) ? mUp : (dn >= 0 ? mDn : (mk == 10 ? mDn : m0));
      // (C17) the lit plane stays paler in colour than the moved body (60 % of its chroma at most)
      for (int it = 0; it < 20; it++) {
        const Lab b2 = toLab(M[2]);
        Lab b3 = toLab(M[3]);
        const double c2 = std::hypot(b2.a, b2.b), c3 = std::hypot(b3.a, b3.b);
        if (c2 < 6.0 || c3 <= 0.57 * c2) break;
        b3.a *= 0.9; b3.b *= 0.9;
        M[3] = fromLab(b3) | 0xFF000000u;
      }
    }
    // T1 compresses: the metal's and the leather's highlight is their light step (matte, one shine)
    if (L.tier[HumanLook::TIER_BODY] == 1 || (!L.tier[HumanLook::TIER_BODY] && top == 1)) {
      R.c[(int)Slot::Metal][4] = R.c[(int)Slot::Metal][3];
      R.c[(int)Slot::Leather][4] = R.c[(int)Slot::Leather][3];
    }
    // plate is never pale and flat (C14): the lit plane (3) 60 luma or more over the shadow side (1)
    {
      uint32_t* M = R.c[(int)Slot::Metal];
      for (int it = 0; it < 12 && luma(M[3]) - luma(M[1]) < 60.0; it++) { M[1] = scaleRgb(M[1], 0.88); M[0] = scaleRgb(M[0], 0.9); }
    }
    // (P2c forge i2, C17's sheen read; STYLE_P2 28.7) the skin and hair moves above may not undo a sheen's tell: a pale
    // alloy's body stays whitened (step 2 at 150 luma or more, the lit plane 22 over it, the glint over that), a dark one's
    // blackened (step 2 at 90 or less) under a near-white glint (196 or more)
    auto towardWhite = [](uint32_t c, int pct) {
      auto ch = [&](int v) { return v + (255 - v) * pct / 100; };
      return rgba(ch(chR(c)), ch(chG(c)), ch(chB(c)));
    };
    if (mk == 10 && L.sheen == 7) {
      uint32_t* M = R.c[(int)Slot::Metal];
      for (int it = 0; it < 30 && luma(M[2]) < 150.0; it++) M[2] = towardWhite(M[2], 6);   // (whitened: the chroma falls)
      for (int it = 0; it < 30 && luma(M[3]) < luma(M[2]) + 22.0; it++) M[3] = scaleRgb(M[3], 1.04);
      if (luma(M[3]) < luma(M[2]) + 22.0) M[3] = towardWhite(M[3], 50);
      if (luma(M[4]) < luma(M[3]) + 8.0) M[4] = towardWhite(M[3], 60);
    }
    if (mk == 10 && L.sheen == 3) {
      uint32_t* M = R.c[(int)Slot::Metal];
      for (int it = 0; it < 30 && luma(M[2]) > 90.0; it++) M[2] = scaleRgb(M[2], 0.95);
      for (int it = 0; it < 30 && luma(M[1]) > luma(M[2]) - 12.0; it++) M[1] = scaleRgb(M[1], 0.92);
      if (luma(M[4]) < 196.0) M[4] = towardWhite(M[4], 70);
    }
    // (forge i2, C14) the dark sheen's body, pushed under 90, parts from the skin and hair it touches by going darker still
    if (mk == 10 && L.sheen == 3) {
      uint32_t* M = R.c[(int)Slot::Metal];
      const double sk = luma(R.c[(int)Slot::Skin][2]), hr = luma(R.c[(int)Slot::Hair][2]);
      for (int it = 0; it < 20 && (std::fabs(luma(M[2]) - sk) < 18.0 || std::fabs(luma(M[2]) - hr) < 18.0) && luma(M[2]) > 30.0; it++) {
        M[2] = scaleRgb(M[2], 0.93);
        if (luma(M[1]) > luma(M[2]) - 10.0) M[1] = scaleRgb(M[1], 0.93);
      }
    }
    // (P2c forge i2, C17; STYLE_P2 28.8: "warm metals stay 20 dE off the skin") a body or lit plane within 20 dE of the
    // look's skin or A3's lose chroma together (their luma and their ratio kept) until clear: a pale bronze reads as metal
    if (mk >= 2) {
      uint32_t* M = R.c[(int)Slot::Metal];
      const uint32_t skins[2] = {R.c[(int)Slot::Skin][2], hex(0xE9A077)};
      auto near = [&]() {
        for (uint32_t sk : skins) for (int st = 2; st <= 3; st++) if (deltaE76(M[st], sk) < 20.5f) return true;
        return false;
      };
      for (int it = 0; it < 16 && near(); it++)
        for (int st = 2; st <= 3; st++) {
          Lab l = toLab(M[st]);
          l.a *= 0.85; l.b *= 0.85;
          M[st] = fromLab(l) | 0xFF000000u;
        }
      // the lit plane paler in colour than the body (60 % of its chroma at most) after the moves above (C17)
      for (int it = 0; it < 30; it++) {
        const Lab b2 = toLab(M[2]);
        Lab b3 = toLab(M[3]);
        const double c2 = std::hypot(b2.a, b2.b), c3 = std::hypot(b3.a, b3.b);
        if (c2 < 6.0 || c3 <= 0.57 * c2) break;
        b3.a *= 0.9; b3.b *= 0.9;
        M[3] = fromLab(b3) | 0xFF000000u;
      }
    }
    // (forge i2, C14) a styled metal's selout-lit edge (step 1) 40 over the darkest day ground where the dark core allows
    // (C17: step 1 at 45 % of step 3's luma or less): the alloys' cores had sunk to 59-64 luma, their outlines lost in shade
    if (L.forge[HumanLook::TIER_BODY] && mk >= 2) {
      uint32_t* M = R.c[(int)Slot::Metal];
      const double sk = luma(R.c[(int)Slot::Skin][2]), hr = luma(R.c[(int)Slot::Hair][2]);
      // step 1 rises to its cap (45 % of step 3, 12 under step 2, plate's 60 under step 3); where step 3 or step 2 holds it
      // down, that step rises a little (step 3 15 under the glint; step 2 still 18 off the skin and hair, 22 under step 3)
      for (int it = 0; it < 120 && luma(M[1]) < 80.0; it++) {
        const double c3 = 0.45 * luma(M[3]) - 1.0, c2 = luma(M[2]) - 12.0, cp = luma(M[3]) - 61.0;
        const double cap = std::min({c3, c2, cp});
        if (luma(scaleRgb(M[1], 1.01)) <= cap) { M[1] = scaleRgb(M[1], 1.01); continue; }
        if (cap == c3 && luma(scaleRgb(M[3], 1.01)) <= luma(M[4]) - 15.0) { M[3] = scaleRgb(M[3], 1.01); continue; }
        const double l2n = luma(scaleRgb(M[2], 1.01));
        if (cap == c2 && L.sheen != 3 && std::fabs(l2n - sk) >= 18.0 && std::fabs(l2n - hr) >= 18.0 && luma(M[3]) - l2n >= 22.0) {
          M[2] = scaleRgb(M[2], 1.01);
          continue;
        }
        break;
      }
    }
    // (forge round 2) and again after the moves above (the skin's and the hair's partings, the lit edge): the read holds
    if (L.forge[HumanLook::TIER_BODY] && mk == 10) {
      metalRound2(R.c[(int)Slot::Metal], L.sheen);
      // the body still parts from the hair it touches (C14: 18 luma): away from it, the lit plane kept 22 over
      uint32_t* M = R.c[(int)Slot::Metal];
      const double hr = luma(R.c[(int)Slot::Hair][2]), sk = luma(R.c[(int)Slot::Skin][2]);
      // (silhouettes i3) and from the second cloth it touches (C14: the brass lift and the polish moved bodies after its parting)
      const double c2l = luma(R.c[(int)Slot::Cloth2][2]);
      auto clash = [&](uint32_t c) { return std::fabs(luma(c) - hr) < 18.0 || std::fabs(luma(c) - sk) < 18.0 || std::fabs(luma(c) - c2l) < 18.0; };
      if (clash(M[2]))
        for (double k : {L.sheen == 7 ? 1.03 : 0.96, L.sheen == 7 ? 0.96 : 1.03}) {   // darker first (out of the skin box), else lighter; a pale body lighter first
          uint32_t c = M[2], c3 = M[3];
          bool ok = false;
          for (int it = 0; it < 40; it++) {
            c = scaleRgb(c, k);
            if (k > 1.0) while (luma(c3) - luma(c) < 22.0 && luma(c3) < 244.0) c3 = scaleRgb(c3, 1.02);   // the lit plane climbs with it
            if (luma(c3) - luma(c) < 22.0 || luma(c) < luma(M[1]) + 8.0) break;
            if (!clash(c)) { ok = true; break; }
          }
          if (ok) {   // (silhouettes i3) never into the skin box (C17 round 2): a move that breaks the read is not taken
            uint32_t t[5] = {M[0], M[1], c, c3, M[4]};
            if (!metalRead2(t, nullptr) && metalRead2(M, nullptr)) continue;
            M[2] = c; M[3] = c3; break;
          }
        }
      // the lit plane paler in colour than the moved body (C17: 60 % of its chroma at most)
      for (int it = 0; it < 30; it++) {
        const Lab b2 = toLab(M[2]);
        Lab b3 = toLab(M[3]);
        const double c2 = std::hypot(b2.a, b2.b), c3 = std::hypot(b3.a, b3.b);
        if (c2 < 6.0 || c3 <= 0.56 * c2) break;
        b3.a *= 0.9; b3.b *= 0.9;
        M[3] = fromLab(b3) | 0xFF000000u;
      }
    }
    // the blade: its own metal (bladeKind; a Mythic blade white-hot), the armour's when they match
    const int bk = std::clamp((int)L.bladeKind, 0, 11);
    for (int i = 0; i < 5; i++) R.c[(int)Slot::Blade][i] = R.c[(int)Slot::Metal][i];
    if (bk == 11) setHex(R.c[(int)Slot::Blade], kMetalKind[9]);   // white-hot steel (gear_look's Mythic blade)
    else if (bk && bk != mk && bk != 1) metalRamp(bk, R.c[(int)Slot::Blade]);
    // trim: gold at T5-T6 (Metal 2), brass below (A3's)
    int topR = 0;
    for (int s = 0; s < 8; s++) if (L.tier[s]) topR = std::max(topR, std::min(6, L.tier[s] + ((L.rare >> s & 1) ? 1 : 0)));
    if (topR >= 5) setHex(R.c[(int)Slot::Metal2], kTrim[1]);
    // (alloys i2: the blind panel, "every warm metal blends into the gold": a brass, amber or gilded body under gold trim
    // reads as one gold mass) a body metal within a glance of the gold takes the silver trim, so the trim stays a second metal
    if (topR >= 5 && deltaE76(R.c[(int)Slot::Metal][2], hex(kTrim[1][2])) < 26.0f) setHex(R.c[(int)Slot::Metal2], kTrim[2]);
    // (alloys i3, the blind panel: "the gold trim, gold shield and white plume never change: 36 recolours of one knight",
    // "garnet and violet clash with the yellow shield") a culture alloy's T5-T6 trim is its own metal POLISHED: the ramp one
    // step up (the trim's edge is the body's lit plane, its face the glint), so the whole harness reads as that metal; a
    // near-neutral pale steel or silver (mid-step chroma under 9, luma 120 up) keeps the gold: silver and gold, the paladin's
    // pairing (a dark neutral iron under gold read as one gold figure)
    if (topR >= 5 && mk == 10 && (alloyTint(R.c[(int)Slot::Metal][2]) >= 9.0 || luma(R.c[(int)Slot::Metal][2]) < 120.0))
      for (int i = 0; i < 5; i++) R.c[(int)Slot::Metal2][i] = R.c[(int)Slot::Metal][std::min(4, i + 1)];
    // (P2c forge, STYLE_P2 28.7) the iridescent tell: Metal 2 is the alloy's own ramp turned 70 degrees of hue (its value
    // kept), so the row cut's alternate rows (and the trim) shift hue across the plates; read in shape by the rows
    if (mk == 10 && L.sheen == 5) {
      const uint32_t* M = R.c[(int)Slot::Metal];
      // (silhouettes i3, the blind panel: "Amiorsteel's pink and blue stripes read as a painted clown costume") the turn goes
      // the way that lands away from the pinks and reds (an oil sheen: blue to teal, gold to green), at a gentler chroma
      double turn = 110.0;
      {
        const Lab m2 = toLab(M[2]);
        const double h0 = std::atan2(m2.b, m2.a) * 57.29578;
        auto pinkness = [](double h) { h = std::fmod(h + 720.0, 360.0); const double d = std::min(std::fabs(h - 15.0), 360.0 - std::fabs(h - 15.0)); return d; };
        if (pinkness(h0 - 110.0) > pinkness(h0 + 110.0)) turn = -110.0;
      }
      for (int i = 0; i < 5; i++) {
        Lab l = toLab(M[i]);
        const double c = std::hypot(l.a, l.b), h = std::atan2(l.b, l.a) + turn * 0.01745329;   // (INTEGRATE silhouettes i1: 110, so a dark iron or a pale glass reads 40 HSV degrees off)
        const double cc = i >= 1 && i <= 3 ? std::clamp(c * 0.8, 16.0, 22.0) : c;   // a pale silver turns visibly, a vivid one stays gentle
        l.a = cc * std::cos(h); l.b = cc * std::sin(h);
        R.c[(int)Slot::Metal2][i] = fromLab(l) | 0xFF000000u;
      }
    }
    // Glow: the glow colour, else the gem colour, else the metal's own
    {
      const uint32_t g0 = L.glowColor ? L.glowColor : L.gemColor ? L.gemColor : (mk == 10 && L.armourTint ? L.armourTint : hex(kGlowOf[mk]));
      rampA3(glowLight(g0, topR >= 5 && !L.mythic && mk != 11, L.mythic != 0 || mk == 11), Material::Metal, R.c[(int)Slot::Glow]);
    }
    // T6: the shield's device burns in the glow's light (STYLE_P2 27.2: "the device or boss glowing"), drawn (no pulse)
    if (L.tier[HumanLook::TIER_SHIELD] >= 6)
      for (int i = 1; i < 5; i++) R.c[(int)Slot::Device][i] = R.c[(int)Slot::Glow][std::min(4, i)];
    // the T1 shield: plain wood, an unpainted field (the device is the field)
    if (L.tier[HumanLook::TIER_SHIELD] == 1) {
      static const uint32_t kWood[5] = {0x2E1C14, 0x5E3E26, 0x8A6440, 0xB08A5C, 0xD2B07E};
      setHex(R.c[(int)Slot::Field], kWood);
      setHex(R.c[(int)Slot::Device], kWood);
    }
    // C14 value: touching materials part by 18 luma at step 2 (STYLE_P2 27.5). The cloths, the leather, the cloak, the
    // accent and the shield's field move (RGB scaled, hue kept: the trouser lifts' way; the smallest change, darker or lighter,
    // that clears every partner; the outline ring keeps its own step 0 and stays 40 under the base); skin, hair and the
    // metals stay themselves. Rig 2's tiered looks only.
    {
      using S = Slot;
      // (P2c forge i2, C17; STYLE_P2 28.8) a styled figure's leather and cloths stay 15 dE off the metal's body and lit plane
      // (steps 2 and 3), so a bronze cuirass never reads as the jerkin under it nor an iron one as the grey shirt
      bool styled = L.forge[HumanLook::TIER_BODY] != 0;
      auto part = [&](S who, std::initializer_list<S> partners, bool lighterOnly, double floor1 = 0) -> bool {
        uint32_t* T = R.c[(int)who];
        const bool offMetal = styled && (who == S::Leather || who == S::Cloth1 || who == S::Cloth2);
        auto apart = [&](const uint32_t* t) {
          for (S p : partners) if (std::fabs(luma(t[2]) - luma(R.c[(int)p][2])) < 18.0) return false;
          if (offMetal)
            for (int st = 2; st <= 3; st++) if (deltaE76(t[st], R.c[(int)S::Metal][st]) < 16.0f) return false;
          return luma(t[2]) - luma(T[0]) >= 40.0 && luma(t[1]) >= floor1;
        };
        if (apart(T)) return true;
        for (int k = 1; k <= 40; k++)
          for (double dir : {-1.0, 1.0}) {
            if (lighterOnly && dir < 0) continue;
            const double f = 1.0 + dir * 0.025 * k;
            if (f <= 0.2) continue;
            uint32_t t[5];
            t[0] = T[0];
            for (int q = 1; q < 5; q++) t[q] = scaleRgb(T[q], f);
            if (apart(t)) { for (int q = 1; q < 5; q++) T[q] = t[q]; return true; }
          }
        return false;
      };
      if (styled && (deltaE76(R.c[(int)S::Cloth1][2], R.c[(int)S::Metal][2]) < 16.0f || deltaE76(R.c[(int)S::Cloth1][3], R.c[(int)S::Metal][3]) < 16.0f))
        part(S::Cloth1, {S::Hair, S::Metal}, false);
      // (forge i2, C14: the T1 heroine's lifted shirt sat 9 luma off her wheat braids) a styled figure's shirt parts from the
      // hair it touches
      if (styled && std::fabs(luma(R.c[(int)S::Cloth1][2]) - luma(R.c[(int)S::Hair][2])) < 18.0) { if (!part(S::Cloth1, {S::Hair, S::Metal}, false)) part(S::Cloth1, {S::Hair}, false); }
      part(S::Leather, {S::Cloth1, S::Cloth2, S::Hair, S::Metal, S::Skin}, false);   // (i2) and the skin: the archer's jerkin read as bare skin
      // (forge i2: and the metal, where a lighter trouser can clear it too; else as before)
      // (silhouettes i3, C14: a_fj2's trousers 15 luma off the brass they hang under, once the brass climbed) the metal before
      // the leather: darker trousers that clear the metal and the hair come next
      if (!styled || (!part(S::Cloth2, {S::Cloth1, S::Leather, S::Hair, S::Metal}, true, kTrouserFloor) &&
                      !part(S::Cloth2, {S::Cloth1, S::Hair, S::Metal}, false, kTrouserFloor) && !part(S::Cloth2, {S::Leather, S::Hair, S::Metal}, true, kTrouserFloor) &&
                      !part(S::Cloth2, {S::Cloth1, S::Leather, S::Hair}, true, kTrouserFloor) &&
                      !part(S::Cloth2, {S::Cloth1, S::Leather, S::Hair}, false, kTrouserFloor) &&
                      !part(S::Cloth2, {S::Hair}, false, kTrouserFloor) && !part(S::Cloth2, {}, false, 0))) {   // (the dE off the metal kept)
        const bool was = styled;
        styled = false;   // (the dE pass off: the trousers' partners as before)
        part(S::Cloth2, {S::Cloth1, S::Leather, S::Hair}, true, kTrouserFloor);
        styled = was;
      }
      // (forge round 2: a styled alloy's tabard, sash and scarf lie on bare arms and the neck too: and the skin)
      // (silhouettes i2, C14: mr_1's sash 2 luma off the shirt it crosses) the shirt kept before the leather and the hair
      if (!part(S::Accent, {S::Cloth1, S::Metal, S::Hair, S::Leather, S::Skin}, false) && !part(S::Accent, {S::Cloth1, S::Metal, S::Hair, S::Leather}, false) && styled) {
        part(S::Accent, {S::Metal, S::Hair}, false);
        // the shirt it crosses (a sash on a bare-armed alloy) when the hair could not be cleared with it
        if (std::fabs(luma(R.c[(int)S::Accent][2]) - luma(R.c[(int)S::Cloth1][2])) < 18.0 && L.forgeShape && !part(S::Accent, {S::Cloth1, S::Metal, S::Leather}, false))
          part(S::Accent, {S::Metal, S::Hair}, false);
      }   // (forge i2: the tabard off the armour first)
      // (forge i2, C14: the king's and the Jade masterwork's tabard sat 12-17 luma off the cloak, a Jade alloy 2 off it) when
      // no cloak clears all five, the accent moves off the cloak, else the cloak clears the two it lies on most (metal, accent)
      if (!part(S::Cloak, {S::Metal, S::Hair, S::Accent, S::Cloth1, S::Leather}, false))
        if (!part(S::Accent, {S::Cloth1, S::Metal, S::Hair, S::Leather, S::Cloak}, false) ||
            std::fabs(luma(R.c[(int)S::Cloak][2]) - luma(R.c[(int)S::Metal][2])) < 18.0)
          part(S::Cloak, {S::Metal, S::Accent}, false);
      // (INTEGRATE forge i2, C14: HOMESPUN's leather straps lay on the shield's field 2 luma off it) the field parts from the
      // leather too where it can, else from the metal and the accent as before
      if (!part(S::Field, {S::Metal, S::Accent, S::Leather}, false)) part(S::Field, {S::Metal, S::Accent}, false);
    }
    // (ladder i2, C14) the leather's step 1 is the lit edge of a soft kit's silhouette (A3's brown at 58 luma sank into dark
    // ground: lt2's edge 35 over the forest floor): lifted to 82 luma, under its step 2, the outline untouched
    {
      uint32_t* Lr = R.c[(int)Slot::Leather];
      for (int it = 0; it < 20 && luma(Lr[1]) < 82.0 && luma(scaleRgb(Lr[1], 1.05)) < luma(Lr[2]) - 8.0; it++) Lr[1] = scaleRgb(Lr[1], 1.05);
    }
  }
  // eyes: the look's colour as iris + glint; 0 picks by skin and hair (dark: brown; else blue, green or grey by person)
  uint32_t eye = L.eyeColor;
  if (!eye) {
    const double sl = toLab(L.skin).L, hl = toLab(L.hairColor).L;
    const uint32_t h = mixHash(mixHash(0xE7Eu, L.skin), L.hairColor);
    if (sl < 60 || hl < 22) eye = (h & 3) ? hex(0x603A20) : hex(0x4A3020);
    else { static const uint32_t pale[4] = {0x3C70C4, 0x388C50, 0x788492, 0x603A20}; eye = hex(pale[h % 4]); }
  }
  const EyePair ep = eyesFor(eye);
  R.c[(int)Slot::Eyes][2] = ep.iris;
  R.c[(int)Slot::Eyes][3] = ep.glint;
  // (P2c forge i2) HOMESPUN's hood is cut from the gambeson's own cloth (Cloth3): one quilted suit, and no new colours (B8)
  if (homespunHood) for (int i = 0; i < 5; i++) R.c[(int)Slot::Head][i] = R.c[(int)Slot::Cloth3][i];
  // (forge i3) a culture alloy's metal head (the guise's coif, turban helm, jingasa, aventail: headwear shapes) in the armour's metal
  if (L.forgeGuise && L.forge[HumanLook::TIER_HELM] && (L.forge[HumanLook::TIER_HELM] & 0xFF) == 0 &&
      forge::guiseHeadwear(forge::unpackGuise(L.forgeGuise).helm))
    for (int i = 0; i < 5; i++) R.c[(int)Slot::Head][i] = R.c[(int)Slot::Metal][i];
  return R;
}

}  // namespace doll
}  // namespace art
