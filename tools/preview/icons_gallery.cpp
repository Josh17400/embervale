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
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "rpg/art/art_rig.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/gear.h"
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

int check() {
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
        if (d < 30.0) {
          std::printf("%s: %s: %s vs %s mean RGB %.1f apart (< 30)\n", gated ? "FAIL" : "note", who, kCols[pick[i]].tag, kCols[pick[j]].tag, d);
          if (gated) fails++;
        }
      }
    for (int c : {0, 1, 2, 9, 10}) {
      double mc[3], h, s;
      meanRgb(iconOf(row, kCols[c]), mc);
      hueSat(mc, h, s);
      if (h < 15 || h > 45 || s < 0.25) {
        std::printf("FAIL: %s: the leather %s reads hue %.0f sat %.2f (want a brown: hue 15-45, sat >= 0.25)\n", who, kCols[c].tag, h, s);
        fails++;
      }
    }
  }
  std::printf("icons_gallery --check: %d makers, %d material pairs, the closest %.1f apart; %d failure(s)\n", rows(), pairs, worst, fails);
  return fails ? 1 : 0;
}

Canvas sheet() {
  art::RigScope rig(2);
  const int cell = 20, lab = 64, top = 12;
  Canvas b(lab + kNCols * cell + 4, top + rows() * cell + 4);
  for (int y = 0; y < b.h; y++)
    for (int x = 0; x < b.w; x++) b.set(x, y, rgba(36, 32, 40));
  auto text = [&](int x, int y, const std::string& s, uint32_t k) {
    for (char ch : s) {
      const int gi = pv::font5x7::glyphIndex(ch);
      if (gi >= 0)
        for (int r = 0; r < 7; r++)
          for (int q = 0; q < 5; q++)
            if (pv::font5x7::kFont[gi].rows[r] & (0x10 >> q)) b.set(x + q, y + r, k);
      x += 6;
    }
  };
  for (int c = 0; c < kNCols; c++) text(lab + c * cell + 1, 2, std::string(kCols[c].tag).substr(0, 3), rgba(220, 210, 190));
  for (int row = 0; row < rows(); row++) {
    std::string who = row == 0 ? "NONE" : cult::archetypeName((cult::Archetype)(row - 1));
    for (char& ch : who) ch = (char)std::toupper((unsigned char)ch);
    text(2, top + row * cell + 6, who.substr(0, 10), rgba(220, 210, 190));
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

}  // namespace

int main(int argc, char** argv) {
  if (argc > 1 && !std::strcmp(argv[1], "--check")) return check();
  const std::string out = argc > 1 ? argv[1] : ".";
  const Canvas c = sheet();
  savePng(c, out + "/icons_body.png", 4);
  savePng(c, out + "/icons_body_1x.png", 1);
  return 0;
}
