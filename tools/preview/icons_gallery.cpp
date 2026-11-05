// Icons gallery (P4 ART, item 6e; docs/FIX_BATCH_P4.md 9.5): the body-armour icons by material, as the pack and the shop
// draw them (rpg/sim/gear_look.cpp gearIcon -> rpg/art/art_items.cpp itemIconLook), rig 2.
//   icons_gallery <outDir>   writes icons_body.png (4x) and icons_body_1x.png: a row per maker (no maker, then every
//                            archetype), a column per piece: the leather GAMBESON (padded), the leather JERKIN, the people's
//                            own leather coat, bronze, the bronze BRIGANDINE, iron, steel, the people's alloy; then the leather
//                            CAP, GLOVES and BOOTS beside them
//   icons_gallery --check    every maker's leather, bronze, iron and steel body icons (each its people's own form) differ
//                            pairwise by >= 30 in mean RGB (opaque pixels), and its leather gambeson from its bronze
//                            brigandine (the other forced-form pairs are listed as notes);
//                            the leather coats', gloves' and boots' mean colour is a brown (hue 15-45 deg, saturation >= 0.25;
//                            the cap wears its people's cloth or plume, so it is shown, not measured); non-zero on failure
// (P5 ICONS, docs/FIX_BATCH_P5.md 3.3, 3.5, 5.4) the maker people's fingerprint in 16 px, over the P5 audit's 36 columns (every
// body form and material T1-T5, helms, shields, gloves, boots, cloaks) x the 13 makers (none, then gear::kitCulture(people,
// seed) for the 12 peoples, as the gates make them):
//   icons_gallery <outDir> [--seeds A..B]
//                            also writes icons_fp_seed<N>.png and icons_fp_seed<N>_grey.png (3x): BEFORE (FIX_FP off) above
//                            AFTER, a row per maker, a column per piece, each people's dye swatch at the row's end; a red frame
//                            marks a twin pair, amber a colour-only pair (the audit's measure)
//   icons_gallery --check [--seeds A..B] [--verbose]   (default 1..3; --verbose prints every leather mean as info lines) the D117 lines above, then per seed and column: 0 twin pairs (shape,
//                            greyscale and palette all within max(3, area / 40) px of another maker's icon), every maker's icon
//                            >= 3 px of its own dye family among the pixels the fingerprint drew (hue within 15 deg of the row's
//                            dye or dye2, chroma >= 20; a T6 piece wears the ladder's gold, not thread, and is exempt), and the
//                            fingerprint's share of the icon's opaque pixels (every pixel FIX_FP changed, the outline ring
//                            that takes their colour included: the eye sees it; fix r1, the rule rpg_test --fingerprint F4
//                            counts by) under FIX_BATCH_P5 3.5's caps (a leather body or cap 10 %, any other body, helm,
//                            shield or cloak 15 %, gloves and boots 20 %); the ring's own part is printed beside
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "rpg/art/art_rig.h"
#include "rpg/art/doll/doll.h"
#include "rpg/art/doll/doll_envelope.h"
#include "rpg/core/fingerprint.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/gear_ladder.h"
#include "rpg/sim/gear_look.h"
#include "tools/preview/preview_util.h"

namespace {

struct Col { const char* tag; ItemKind kind; Mat mat; int form; int ilvl; };
const Col kCols[] = {
    {"GAMBESON", ItemKind::Armor, Mat::Leather, 1, 2},  {"JERKIN", ItemKind::Armor, Mat::Leather, 2, 2},
    {"LEATHER", ItemKind::Armor, Mat::Leather, 0, 2},   {"BRONZE", ItemKind::Armor, Mat::Bronze, 0, 4},
    {"BRIG", ItemKind::Armor, Mat::Bronze, 6, 4},       {"IRON", ItemKind::Armor, Mat::Iron, 0, 8},
    {"STEEL", ItemKind::Armor, Mat::Steel, 0, 14},      {"ALLOY", ItemKind::Armor, Mat::Alloy, 0, 30},
    {"CAP", ItemKind::Helmet, Mat::Leather, 0, 2},      {"GLOVES", ItemKind::Gloves, Mat::Leather, 0, 2},
    {"BOOTS", ItemKind::Boots, Mat::Leather, 0, 2},
};
constexpr int kNCols = (int)(sizeof(kCols) / sizeof(kCols[0]));

const cult::Culture* makerOf(int row) {   // row 0: no maker (the heartland's classic pieces); 1..: an archetype each
  static std::vector<cult::Culture> cs;
  if (cs.empty())
    for (int a = 0; a < (int)cult::Archetype::COUNT; a++) cs.push_back(cult::Atlas::make((cult::Archetype)a, 0x51C0u + (uint32_t)a * 7919u));
  return row == 0 ? nullptr : &cs[(size_t)row - 1];
}
int rows() { return 1 + (int)cult::Archetype::COUNT; }

Canvas iconOf(int row, const Col& c) {
  const cult::Culture* m = makerOf(row);
  Rng r(0x1C0u + (uint32_t)row * 131u);
  Item it = gear::makeGearC(r, c.kind, 0, c.ilvl, Rarity::Common, m, c.mat, c.mat == Mat::Alloy ? 1 : 0);
  if (c.mat == Mat::Alloy && it.mat != Mat::Alloy) return Canvas(art::ICON, art::ICON);   // a people with no alloy
  if (c.form) { it.form = (uint8_t)c.form; it.name = gear::gearName(it, m); }
  return art::itemIconLook(gearIcon(it, m));
}

void meanRgb(const Canvas& c, double out[3]) {
  double s[3] = {0, 0, 0};
  int n = 0;
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      const uint32_t v = c.get(x, y);
      if (!(v >> 24)) continue;
      for (int i = 0; i < 3; i++) s[i] += (double)((v >> (8 * i)) & 255u);
      n++;
    }
  for (int i = 0; i < 3; i++) out[i] = n ? s[i] / n : 0;
}
double distRgb(const double a[3], const double b[3]) { return std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2])); }
void hueSat(const double c[3], double& h, double& s) {
  const double r = c[0] / 255, g = c[1] / 255, b = c[2] / 255, mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b)), d = mx - mn;
  const double l = (mx + mn) / 2;
  s = d < 1e-6 ? 0 : d / (1 - std::fabs(2 * l - 1));
  if (d < 1e-6) { h = 0; return; }
  if (mx == r) h = 60 * std::fmod((g - b) / d + 6, 6.0);
  else if (mx == g) h = 60 * ((b - r) / d + 2);
  else h = 60 * ((r - g) / d + 4);
}

bool g_verbose = false;   // --verbose: every leather mean the D117 lines measure, not only the failing ones
int checkD117() {
  art::RigScope rig(2);
  int fails = 0, pairs = 0;
  double worst = 1e9;
  for (int row = 0; row < rows(); row++) {
    const char* who = row == 0 ? "none" : cult::archetypeName((cult::Archetype)(row - 1));
    // the people's own body form per material (columns 2 3 5 6), and the gambeson and the brigandine (0 4)
    const int pick[6] = {2, 3, 5, 6, 0, 4};
    double m[6][3];
    for (int i = 0; i < 6; i++) meanRgb(iconOf(row, kCols[pick[i]]), m[i]);
    for (int i = 0; i < 6; i++)
      for (int j = i + 1; j < 6; j++) {
        if ((i == 0 && j == 4) || (i == 1 && j == 5)) continue;   // the same material (leather, bronze) in two forms
        const double d = distRgb(m[i], m[j]);
        // gated: the people's own leather, bronze, iron and steel coats pairwise (what a shelf sells), and the W7 testers'
        // pair (the leather gambeson and the bronze brigandine); the other forced-form pairs are listed (a people's gilt
        // rim on its iron coat carries its mean toward bronze's while the two read apart: shown on the sheet)
        const bool gated = j < 4 || (i == 4 && j == 5);
        if (gated) { pairs++; worst = std::min(worst, d); }
        if (g_verbose && gated) std::printf("info: %s: %s vs %s mean RGB %.1f apart\n", who, kCols[pick[i]].tag, kCols[pick[j]].tag, d);
        if (d < 30.0) {
          std::printf("%s: %s: %s vs %s mean RGB %.1f apart (< 30)\n", gated ? "FAIL" : "note", who, kCols[pick[i]].tag, kCols[pick[j]].tag, d);
          if (gated) fails++;
        }
      }
    for (int c : {0, 1, 2, 9, 10}) {
      double mc[3], h, s;
      meanRgb(iconOf(row, kCols[c]), mc);
      hueSat(mc, h, s);
      if (g_verbose) std::printf("info: %s: the leather %s reads hue %.0f sat %.2f\n", who, kCols[c].tag, h, s);
      if (h < 15 || h > 45 || s < 0.25) {
        std::printf("FAIL: %s: the leather %s reads hue %.0f sat %.2f (want a brown: hue 15-45, sat >= 0.25)\n", who, kCols[c].tag, h, s);
        fails++;
      }
    }
  }
  std::printf("icons_gallery --check: %d makers, %d material pairs, the closest %.1f apart; %d failure(s)\n", rows(), pairs, worst, fails);
  return fails;
}

void text(Canvas& b, int x, int y, const std::string& s, uint32_t k) {
  for (char ch : s) {
    const int gi = pv::font5x7::glyphIndex(ch);
    if (gi >= 0)
      for (int r = 0; r < 7; r++)
        for (int q = 0; q < 5; q++)
          if (pv::font5x7::kFont[gi].rows[r] & (0x10 >> q)) b.set(x + q, y + r, k);
    x += 6;
  }
}

Canvas sheet() {
  art::RigScope rig(2);
  const int cell = 20, lab = 64, top = 12;
  Canvas b(lab + kNCols * cell + 4, top + rows() * cell + 4);
  for (int y = 0; y < b.h; y++)
    for (int x = 0; x < b.w; x++) b.set(x, y, rgba(36, 32, 40));
  for (int c = 0; c < kNCols; c++) text(b, lab + c * cell + 1, 2, std::string(kCols[c].tag).substr(0, 3), rgba(220, 210, 190));
  for (int row = 0; row < rows(); row++) {
    std::string who = row == 0 ? "NONE" : cult::archetypeName((cult::Archetype)(row - 1));
    for (char& ch : who) ch = (char)std::toupper((unsigned char)ch);
    text(b, 2, top + row * cell + 6, who.substr(0, 10), rgba(220, 210, 190));
    for (int c = 0; c < kNCols; c++) {
      const Canvas ic = iconOf(row, kCols[c]);
      const int ox = lab + c * cell + 2, oy = top + row * cell + 2;
      for (int y = 0; y < ic.h; y++)
        for (int x = 0; x < ic.w; x++) {
          const uint32_t v = ic.get(x, y);
          if (v >> 24) b.set(ox + x, oy + y, v);
        }
    }
  }
  return b;
}

// ---------------------------------------------------------------- P5: the fingerprint over the audit's columns
constexpr int NM = fp::kPeoples;   // makers: 0 none, 1..12 cult::Archetype + 1
enum Grp { G_BODY, G_HELM, G_SHIELD, G_GLOVES, G_BOOTS, G_CLOAK };
struct FpCol { const char* tag; Grp g; ItemKind kind; Mat mat; int form; int ilvl; Rarity rar; int alloy; };
const FpCol kFpCols[] = {   // the P5 audit's piece list (docs/FIX_BATCH_P5.md 2.1)
    {"gambeson T1", G_BODY, ItemKind::Armor, Mat::Leather, 1, 3, Rarity::Common, 0},
    {"jerkin T1", G_BODY, ItemKind::Armor, Mat::Leather, 2, 3, Rarity::Common, 0},
    {"own-soft T2", G_BODY, ItemKind::Armor, Mat::Leather, 0, 6, Rarity::Rare, 0},
    {"bronze T1", G_BODY, ItemKind::Armor, Mat::Bronze, 0, 4, Rarity::Common, 0},
    {"bronze brig T1", G_BODY, ItemKind::Armor, Mat::Bronze, 6, 4, Rarity::Common, 0},
    {"iron T2", G_BODY, ItemKind::Armor, Mat::Iron, 0, 10, Rarity::Common, 0},
    {"steel T3", G_BODY, ItemKind::Armor, Mat::Steel, 0, 20, Rarity::Common, 0},
    {"mail T3", G_BODY, ItemKind::Armor, Mat::Steel, 3, 20, Rarity::Common, 0},
    {"scale T3", G_BODY, ItemKind::Armor, Mat::Steel, 4, 20, Rarity::Common, 0},
    {"lamellar T3", G_BODY, ItemKind::Armor, Mat::Steel, 5, 20, Rarity::Common, 0},
    {"brig T3", G_BODY, ItemKind::Armor, Mat::Steel, 6, 20, Rarity::Common, 0},
    {"plate T3", G_BODY, ItemKind::Armor, Mat::Steel, 7, 20, Rarity::Common, 0},
    {"leaf T3", G_BODY, ItemKind::Armor, Mat::Steel, 8, 20, Rarity::Common, 0},
    {"alloy T4", G_BODY, ItemKind::Armor, Mat::Alloy, 0, 28, Rarity::Common, 1},
    {"master T5", G_BODY, ItemKind::Armor, Mat::Alloy, 0, 47, Rarity::Rare, 9},
    {"cap T2", G_HELM, ItemKind::Helmet, Mat::Leather, 0, 6, Rarity::Rare, 0},
    {"bronze helm T1", G_HELM, ItemKind::Helmet, Mat::Bronze, 0, 4, Rarity::Common, 0},
    {"iron helm T2", G_HELM, ItemKind::Helmet, Mat::Iron, 0, 10, Rarity::Common, 0},
    {"steel helm T3", G_HELM, ItemKind::Helmet, Mat::Steel, 0, 20, Rarity::Common, 0},
    {"alloy helm T4", G_HELM, ItemKind::Helmet, Mat::Alloy, 0, 28, Rarity::Common, 1},
    {"shield T2", G_SHIELD, ItemKind::Shield, Mat::Leather, 0, 6, Rarity::Rare, 0},
    {"bronze shield T1", G_SHIELD, ItemKind::Shield, Mat::Bronze, 0, 4, Rarity::Common, 0},
    {"iron shield T2", G_SHIELD, ItemKind::Shield, Mat::Iron, 0, 10, Rarity::Common, 0},
    {"steel shield T3", G_SHIELD, ItemKind::Shield, Mat::Steel, 0, 20, Rarity::Common, 0},
    {"alloy shield T4", G_SHIELD, ItemKind::Shield, Mat::Alloy, 0, 28, Rarity::Common, 1},
    {"gloves T2", G_GLOVES, ItemKind::Gloves, Mat::Leather, 0, 6, Rarity::Rare, 0},
    {"iron gloves T2", G_GLOVES, ItemKind::Gloves, Mat::Iron, 0, 10, Rarity::Common, 0},
    {"steel gloves T3", G_GLOVES, ItemKind::Gloves, Mat::Steel, 0, 20, Rarity::Common, 0},
    {"alloy gloves T4", G_GLOVES, ItemKind::Gloves, Mat::Alloy, 0, 28, Rarity::Common, 1},
    {"boots T1", G_BOOTS, ItemKind::Boots, Mat::Leather, 0, 3, Rarity::Common, 0},
    {"boots T2", G_BOOTS, ItemKind::Boots, Mat::Leather, 0, 6, Rarity::Rare, 0},
    {"iron boots T3", G_BOOTS, ItemKind::Boots, Mat::Iron, 0, 10, Rarity::Common, 0},
    {"steel boots T3", G_BOOTS, ItemKind::Boots, Mat::Steel, 0, 20, Rarity::Common, 0},
    {"alloy boots T4", G_BOOTS, ItemKind::Boots, Mat::Alloy, 0, 28, Rarity::Common, 1},
    {"cloak T3", G_CLOAK, ItemKind::Cloak, Mat::Cloth, 0, 20, Rarity::Common, 0},
    {"cloak T5", G_CLOAK, ItemKind::Cloak, Mat::Cloth, 0, 47, Rarity::Rare, 0},
};
constexpr int NC = (int)(sizeof(kFpCols) / sizeof(kFpCols[0]));
const char* kMk[NM] = {"none", "fjord", "highl", "heart", "imper", "dune", "steppe", "marsh", "jade", "river", "sunt", "sylvan", "stars"};

const cult::Culture* fpMaker(int m, uint32_t seed) { return m == 0 ? nullptr : &gear::kitCulture((cult::Archetype)(m - 1), seed); }
// the piece as the shops make it (the audit's recipe); false: this maker has no such piece (no alloy)
bool fpItem(int m, const FpCol& c, uint32_t seed, Item& out) {
  const cult::Culture* C = fpMaker(m, seed);
  Rng r(0x1C0u + (uint32_t)m * 131u + (uint32_t)c.ilvl * 7u);
  uint8_t al = 0;
  if (c.mat == Mat::Alloy) {
    if (!C || C->arms.alloys.empty()) return false;
    al = (uint8_t)std::min<int>(c.alloy, (int)C->arms.alloys.size());
  }
  Item it = gear::makeGearC(r, c.kind, 0, c.ilvl, c.rar, C, c.mat, al);
  if (c.mat == Mat::Alloy && it.mat != Mat::Alloy) return false;
  if (c.form) { it.form = (uint8_t)c.form; it.name = gear::gearName(it, C); }
  if (c.kind == ItemKind::Cloak && !it.tint && C) it.tint = (C->heraldry.field ? C->heraldry.field : (C->arms.cloth ? C->arms.cloth : 0)) | 0xFF000000u;
  out = it;
  return true;
}
// one icon with the fingerprint on (after) or off (before: FIX_FP cleared, as EMB_FIX_RULES=4095 draws it)
Canvas fpIcon(const Item& it, const cult::Culture* C, bool after) {
  uint32_t& rules = art::doll::fixRules();
  const uint32_t keep = rules;
  if (!after) rules &= ~(uint32_t)art::doll::FIX_FP;
  Canvas c = art::itemIconLook(gearIcon(it, C));
  rules = keep;
  return c;
}

// the audit's measure at 16 px: xor (silhouette px in one and not the other), grey (shared px whose luma differs by 12+),
// distinct (px whose colour lies dE 10+ from every colour of the other's palette)
struct Lab { double L, a, b; };
double lin(double c) { c /= 255.0; return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }
Lab toLab(uint32_t v) {
  const double r = lin(v & 255), g = lin(v >> 8 & 255), b = lin(v >> 16 & 255);
  double X = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047, Y = 0.2126 * r + 0.7152 * g + 0.0722 * b, Z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883;
  auto f = [](double t) { return t > 0.008856 ? std::cbrt(t) : 7.787 * t + 16.0 / 116; };
  X = f(X); Y = f(Y); Z = f(Z);
  return Lab{116 * Y - 16, 500 * (X - Y), 200 * (Y - Z)};
}
double dE(uint32_t a, uint32_t b) {
  static std::map<uint64_t, double> memo;
  const uint64_t k = (uint64_t)(a & 0xFFFFFF) << 24 | (b & 0xFFFFFF);
  auto it = memo.find(k);
  if (it != memo.end()) return it->second;
  const Lab x = toLab(a), y = toLab(b);
  const double d = std::sqrt((x.L - y.L) * (x.L - y.L) + (x.a - y.a) * (x.a - y.a) + (x.b - y.b) * (x.b - y.b));
  memo[k] = d;
  return d;
}
int luma8(uint32_t v) { return (int)std::lround(0.299 * (v & 255) + 0.587 * (v >> 8 & 255) + 0.114 * (v >> 16 & 255)); }
uint32_t greyOf(uint32_t v) { const int l = luma8(v); return (v & 0xFF000000u) | (uint32_t)l << 16 | (uint32_t)l << 8 | (uint32_t)l; }
int opaque(const Canvas& c) { int n = 0; for (uint32_t v : c.px) n += (v >> 24) != 0; return n; }

enum Verdict { V_DISTINCT = 0, V_COLOUR = 1, V_TWIN = 2 };
Verdict verdictOf(const Canvas& A, const Canvas& B) {
  int xorPx = 0, greyPx = 0, distinctPx = 0;
  std::map<uint32_t, int> pa, pb;
  for (int y = 0; y < art::ICON; y++)
    for (int x = 0; x < art::ICON; x++) {
      const uint32_t a = A.get(x, y), b = B.get(x, y);
      const bool oa = (a >> 24) != 0, ob = (b >> 24) != 0;
      if (oa) pa[a]++;
      if (ob) pb[b]++;
      if (oa != ob) xorPx++;
      else if (oa && std::abs(luma8(a) - luma8(b)) >= 12) greyPx++;
    }
  auto far = [&](const std::map<uint32_t, int>& P, const std::map<uint32_t, int>& Q) {
    int n = 0;
    for (auto& [col, cnt] : P) {
      double m = 1e9;
      for (auto& [d, k] : Q) m = std::min(m, dE(col, d));
      if (m >= 10) n += cnt;
    }
    return n;
  };
  distinctPx = far(pa, pb) + far(pb, pa);
  const int tol = std::max(3, std::max(opaque(A), opaque(B)) / 40);   // FIX_BATCH_P5 7 F3: the icons' tolerance
  const bool shapeTwin = xorPx <= tol && greyPx <= tol;
  if (shapeTwin && distinctPx <= tol) return V_TWIN;
  return shapeTwin ? V_COLOUR : V_DISTINCT;
}

// the fingerprint's own pixels (those FIX_FP changed), how many lie in the row's dye family, and the share cap
struct FpMeasure { int drawn = 0, ring = 0, family = 0, area = 0; };   // ring: changed outline pixels (the outline takes its
                                                                       // neighbour's colour, darkened: counted apart)
FpMeasure measure(const Canvas& before, const Canvas& after, int people) {
  FpMeasure f;
  const fp::Row& row = fp::row(people);
  auto hc = [](uint32_t rgb, double& h, double& chroma) {   // 0xRRGGBB or rgba: callers pass r, g, b in order
    const double r = rgb >> 16 & 255, g = rgb >> 8 & 255, b = rgb & 255, mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b));
    chroma = mx - mn;
    double c3[3] = {r, g, b}, s;
    hueSat(c3, h, s);
  };
  double h1, c1, h2 = 0, c2 = 0;
  // (P5b) a signed people's band is its dominant and accent (art_items iconFingerprint draws that pair under FIX_SIG)
  const bool sg = row.sig && art::doll::fixOn(art::doll::FIX_SIG) && art::doll::fixOn(art::doll::FIX_FP);
  const uint32_t dye = sg ? row.sig->dom : row.dye, dye2 = sg ? row.sig->acc : row.dye2;
  hc(dye, h1, c1);
  if (dye2) hc(dye2, h2, c2);
  auto near = [](double a, double b) { const double d = std::fabs(a - b); return std::min(d, 360 - d) <= 15; };
  for (int y = 0; y < art::ICON; y++)
    for (int x = 0; x < art::ICON; x++) {
      const uint32_t a = after.get(x, y);
      if (a >> 24) f.area++;
      if (a == before.get(x, y)) continue;
      auto clear = [&](int u, int v) { return u < 0 || v < 0 || u >= art::ICON || v >= art::ICON || !(after.get(u, v) >> 24); };
      if (clear(x - 1, y) || clear(x + 1, y) || clear(x, y - 1) || clear(x, y + 1)) { f.ring++; continue; }   // the outline ring
      f.drawn++;
      double h, ch;
      hc((a & 255) << 16 | (a & 0xFF00) | (a >> 16 & 255), h, ch);
      if (ch >= 20 && (near(h, h1) || (row.dye2 && c2 >= 20 && near(h, h2)))) f.family++;
    }
  return f;
}
double capOf(const FpCol& c) {
  if (c.g == G_GLOVES || c.g == G_BOOTS) return 0.20;
  if ((c.g == G_BODY || c.g == G_HELM) && c.mat == Mat::Leather) return 0.10;
  return 0.15;
}

int checkFp(uint32_t s0, uint32_t s1) {
  art::RigScope rig(2);
  if (g_verbose)   // each people's dye ramp as the icons and the doll draw it (rampA3, Cloth): hue and chroma per step
    for (int m = 1; m < NM; m++) {
      const fp::Row& row = fp::row(m);
      for (uint32_t base : {row.dye, row.dye2}) {
        if (!base) continue;
        uint32_t k[5];
        art::doll::rampA3(rgba((int)(base >> 16 & 255), (int)(base >> 8 & 255), (int)(base & 255)), art::doll::Material::Cloth, k);
        std::printf("info: ramp %-6s #%06X:", kMk[m], (unsigned)base);
        for (uint32_t v : k) {
          double c3[3] = {(double)(v & 255), (double)(v >> 8 & 255), (double)(v >> 16 & 255)}, h, s;
          hueSat(c3, h, s);
          const double mx = std::max(c3[0], std::max(c3[1], c3[2])), mn = std::min(c3[0], std::min(c3[1], c3[2]));
          std::printf("  h%3.0f c%3.0f", h, mx - mn);
        }
        std::printf("\n");
      }
    }
  int fails = 0, cols = 0, totPairs = 0, totColour = 0;
  double worstShare = 0;
  for (uint32_t seed = s0; seed <= s1; seed++)
    for (int ci = 0; ci < NC; ci++) {
      const FpCol& c = kFpCols[ci];
      std::vector<Canvas> after(NM, Canvas(art::ICON, art::ICON)), before = after;
      std::vector<bool> have(NM, false);
      std::vector<int> tierOf(NM, 0);
      for (int m = 0; m < NM; m++) {
        Item it;
        if (!fpItem(m, c, seed, it)) continue;
        have[m] = true;
        after[m] = fpIcon(it, fpMaker(m, seed), true);
        before[m] = fpIcon(it, fpMaker(m, seed), false);
        tierOf[m] = gear::lookTier(it, fpMaker(m, seed)).tier;
      }
      int twins = 0, colour = 0, pairs = 0, minFam = 99;
      double maxShare = 0, maxRing = 0;
      std::string twinList;
      for (int a = 0; a < NM; a++)
        for (int b = a + 1; b < NM; b++) {
          if (!have[a] || !have[b]) continue;
          pairs++;
          const Verdict v = verdictOf(after[a], after[b]);
          if (v == V_TWIN) { twins++; twinList += std::string(" ") + kMk[a] + "=" + kMk[b]; }
          else if (v == V_COLOUR) colour++;
        }
      for (int m = 1; m < NM; m++) {
        if (!have[m]) continue;
        const FpMeasure f = measure(before[m], after[m], m);
        // (fix r1) the ring counts (the eye sees it), as F4 counts it: the share is every changed pixel over the opaque ones
        const double share = f.area ? (double)(f.drawn + f.ring) / f.area : 0;
        maxShare = std::max(maxShare, share);
        maxRing = std::max(maxRing, f.area ? (double)f.ring / f.area : 0);
        // (P5b, STYLE_P5B 7 and 8.4) a signed people's helm, shield and cloak icon is the people's own drawing (the head class,
        // the field, the cloak cloth): no P5 share cap (S5 checks the head class, 3 px of the dominant and the greyscale parting)
        const bool sigCloth = fp::row(m).sig && art::doll::fixOn(art::doll::FIX_SIG) && art::doll::fixOn(art::doll::FIX_FP) &&
                              (c.g == G_HELM || c.g == G_SHIELD || c.g == G_CLOAK);
        if (share > capOf(c) + 1e-9 && !sigCloth) {
          std::printf("FAIL: fp seed %u %s %s: the fingerprint takes %d of %d px with the outline ring (%.1f %%, cap %.0f %%)\n", seed, c.tag, kMk[m], f.drawn + f.ring, f.area, share * 100, capOf(c) * 100);
          fails++;
        }
        if (tierOf[m] <= 5) {
          minFam = std::min(minFam, f.family);
          if (f.family < 3) {
            std::printf("FAIL: fp seed %u %s %s (T%d): %d px of its dye family (want >= 3)\n", seed, c.tag, kMk[m], tierOf[m], f.family);
            fails++;
          }
        }
      }
      if (twins) { std::printf("FAIL: fp seed %u %s: %d twin pair(s):%s\n", seed, c.tag, twins, twinList.c_str()); fails++; }
      std::printf("fp seed %u %-17s pairs %2d twin %d colour-only %2d  share max %4.1f %% (cap %2.0f %%; the outline ring's part %4.1f %%)  dye px min %d\n", seed,
                  c.tag, pairs, twins, colour, maxShare * 100, capOf(c) * 100, maxRing * 100, minFam == 99 ? 0 : minFam);
      cols++;
      totPairs += pairs;
      totColour += colour;
      worstShare = std::max(worstShare, maxShare);
    }
  std::printf("icons_gallery --check fingerprint: seeds %u..%u, %d columns, %d pairs, %d colour-only, the largest share %.1f %%; %d failure(s)\n", s0, s1, cols,
              totPairs, totColour, worstShare * 100, fails);
  return fails;
}

// the before | after matrix: BEFORE above AFTER, a row per maker (its dye swatch at the row's end), a column per piece
void fpSheets(const std::string& out, uint32_t seed) {
  art::RigScope rig(2);
  const int cell = 20, lab = 44, top = 12, half = top + NM * cell + 6;
  Canvas b(lab + NC * cell + 30, 2 * half + 2);
  const uint32_t bg = rgba(36, 32, 40), ink = rgba(220, 210, 190), kTwin = rgba(220, 40, 40), kColour = rgba(236, 168, 40);
  for (uint32_t& v : b.px) v = bg;
  auto frame = [&](int x0, int y0, uint32_t k) {
    for (int i = 0; i < cell; i++) { b.set(x0 + i, y0, k); b.set(x0 + i, y0 + cell - 1, k); b.set(x0, y0 + i, k); b.set(x0 + cell - 1, y0 + i, k); }
  };
  for (int pass = 0; pass < 2; pass++) {
    const int oy0 = pass * half;
    text(b, 2, oy0 + 2, pass ? "AFTER" : "BEFORE", pass ? rgba(140, 220, 140) : rgba(200, 170, 170));
    for (int ci = 0; ci < NC; ci++) {
      const FpCol& c = kFpCols[ci];
      std::string t = c.tag;
      text(b, lab + ci * cell + 1, oy0 + 2, t.substr(0, 3), ink);
      std::vector<Canvas> ic(NM, Canvas(art::ICON, art::ICON));
      std::vector<bool> have(NM, false);
      for (int m = 0; m < NM; m++) {
        Item it;
        if (!fpItem(m, c, seed, it)) continue;
        have[m] = true;
        ic[m] = fpIcon(it, fpMaker(m, seed), pass == 1);
      }
      for (int m = 0; m < NM; m++) {
        const int ox = lab + ci * cell, oy = oy0 + top + m * cell;
        for (int y = 0; y < art::ICON; y++)
          for (int x = 0; x < art::ICON; x++) {
            const uint32_t v = ic[m].get(x, y);
            if (v >> 24) b.set(ox + 2 + x, oy + 2 + y, v);
          }
        if (!have[m]) continue;
        Verdict w = V_DISTINCT;
        for (int o = 0; o < NM; o++) if (o != m && have[o]) w = std::max(w, verdictOf(ic[m], ic[o]));
        if (w != V_DISTINCT) frame(ox, oy, w == V_TWIN ? kTwin : kColour);
      }
    }
    for (int m = 0; m < NM; m++) {
      text(b, 2, oy0 + top + m * cell + 7, kMk[m], ink);
      const fp::Row& row = fp::row(m);
      if (!row.period) continue;
      auto rgb = [](uint32_t v) { return rgba((int)(v >> 16 & 255), (int)(v >> 8 & 255), (int)(v & 255)); };
      for (int y = 0; y < 12; y++)
        for (int x = 0; x < 20; x++) {
          const bool second = row.dye2 && ((x + y) % row.period) >= row.run;
          b.set(lab + NC * cell + 6 + x, oy0 + top + m * cell + 4 + y, rgb(second ? row.dye2 : row.dye));
        }
    }
  }
  Canvas g = b;
  for (uint32_t& v : g.px) v = greyOf(v);
  savePng(b, out + "/icons_fp_seed" + std::to_string(seed) + ".png", 3);
  savePng(g, out + "/icons_fp_seed" + std::to_string(seed) + "_grey.png", 3);
}

bool seedsArg(int argc, char** argv, uint32_t& s0, uint32_t& s1) {
  for (int i = 1; i + 1 < argc; i++)
    if (!std::strcmp(argv[i], "--seeds")) {
      unsigned a = 0, b = 0;
      if (std::sscanf(argv[i + 1], "%u..%u", &a, &b) == 2) { s0 = a; s1 = b; return true; }
      if (std::sscanf(argv[i + 1], "%u", &a) == 1) { s0 = s1 = a; return true; }
    }
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  uint32_t s0 = 1, s1 = 3;
  if (argc > 1 && !std::strcmp(argv[1], "--check")) {
    seedsArg(argc, argv, s0, s1);
    for (int i = 2; i < argc; i++) if (!std::strcmp(argv[i], "--verbose")) g_verbose = true;
    const int f = checkD117();
    // the fingerprint draws only with FIX_FP (EMB_FIX_RULES=4095: the pre-P5 icons, the D117 lines alone)
    const int g = art::doll::fixOn(art::doll::FIX_FP) ? checkFp(s0, s1) : 0;
    return f || g ? 1 : 0;
  }
  const std::string out = argc > 1 ? argv[1] : ".";
  const Canvas c = sheet();
  savePng(c, out + "/icons_body.png", 4);
  savePng(c, out + "/icons_body_1x.png", 1);
  s0 = s1 = 1;
  seedsArg(argc, argv, s0, s1);
  for (uint32_t s = s0; s <= s1; s++) fpSheets(out, s);
  return 0;
}
