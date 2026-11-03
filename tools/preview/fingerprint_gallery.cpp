// fingerprint_gallery (P5 GATES, docs/FIX_BATCH_P5.md 5.5): the peoples' fingerprint on armour, worn and in the bag, as
// sheets and a report. Every audit column (tools/preview/fingerprint_audit.h: every body, helm and shield form with the
// materials and tiers the shops make, gloves, boots, cloaks) x the 13 makers (none + the 12 peoples), worn by A3's man and
// woman (idle frame 0, facings D and R) and as the 16 px icon. A red frame marks a twin, an amber one a colour-only pair.
//
//   fingerprint_gallery <outDir> [--seeds A..B] [--before] [--grey] [--matrix] [--kits] [--census] [--icons] [--swatches]
//     --before  the pre-P5 look from the same build (FIX_FP off through art::doll::fixRules())
//     --grey    also writes every sheet in greyscale (*_grey.png)
//     --matrix  the worn matrices per piece group (and the icon matrix); --icons the icon matrix alone; --kits every people's
//               T1-T6 ladder kit (man, woman); --census the watch ranks 0-2, a soldier, a bandit, a captain and the king of
//               every people (D and R); --swatches the twelve dyes, bands, marks and icons (swatches.png). None: all of them.
// Writes <outDir>/report_<tag>.txt (every column's pairs, the twins and colour-only pairs named with their three measures
// and the pixel they differ most at, the dye shares) and the sheets (<= 8000 px a side).
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "rpg/art/doll/doll_internal.h"
#include "tools/preview/fingerprint_audit.h"
#include "tools/preview/preview_util.h"

namespace {

using namespace fpa;

const uint32_t kBg = rgba(118, 132, 96), kBg2 = rgba(36, 32, 40), kInk = rgba(230, 222, 200);
const uint32_t kTwin = rgba(230, 40, 40), kColourOnly = rgba(240, 170, 40);

struct Opts {
  std::string out = ".";
  uint32_t seedA = 1, seedB = 1;
  bool before = false, grey = false, matrix = false, kits = false, census = false, icons = false, swatches = false;
};

FILE* g_rep = nullptr;
void rep(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  if (g_rep) vfprintf(g_rep, fmt, ap);
  va_end(ap);
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
}

void text(Canvas& b, int x, int y, const std::string& s, uint32_t k) {
  for (char ch : s) {
    const int gi = pv::font5x7::glyphIndex((char)std::toupper((unsigned char)ch));
    if (gi >= 0)
      for (int r = 0; r < 7; r++)
        for (int q = 0; q < 5; q++)
          if (pv::font5x7::kFont[gi].rows[r] & (0x10 >> q)) b.set(x + q, y + r, k);
    x += 6;
  }
}
void frameBox(Canvas& b, int x0, int y0, int w, int h, uint32_t k) {
  for (int x = x0; x < x0 + w; x++) { b.set(x, y0, k); b.set(x, y0 + h - 1, k); }
  for (int y = y0; y < y0 + h; y++) { b.set(x0, y, k); b.set(x0 + w - 1, y, k); }
}
// a sheet and its greyscale twin, drawn together
struct Sheet {
  Canvas c, g;
  Sheet(int w, int h, uint32_t bg) : c(w, h), g(w, h) { for (uint32_t& v : c.px) v = bg; for (uint32_t& v : g.px) v = greyOf(bg); }
  void set(int x, int y, uint32_t v) { c.set(x, y, v); g.set(x, y, greyOf(v)); }
  void label(int x, int y, const std::string& s) { text(c, x, y, s, kInk); text(g, x, y, s, kInk); }
  void box(int x, int y, int w, int h, uint32_t k) { frameBox(c, x, y, w, h, k); frameBox(g, x, y, w, h, k); }
  // a figure's full canvas into a cell, its pivot at the cell's bottom centre (clipped to the cell)
  void figure(const Fig& f, int cx, int cy, int cw, int ch) {
    const int ox = cx + cw / 2 - f.px, oy = cy + ch - 6 - f.py;
    for (int y = 0; y < f.full.h; y++)
      for (int x = 0; x < f.full.w; x++) {
        const uint32_t v = f.full.get(x, y);
        const int X = ox + x, Y = oy + y;
        if ((v >> 24) && X >= cx && X < cx + cw && Y >= cy && Y < cy + ch) set(X, Y, v);
      }
  }
  void blit(const Canvas& s, int x0, int y0, int scale = 1) {
    for (int y = 0; y < s.h * scale; y++)
      for (int x = 0; x < s.w * scale; x++) { const uint32_t v = s.get(x / scale, y / scale); if (v >> 24) set(x0 + x, y0 + y, v); }
  }
  void save(const Opts& o, const std::string& name, int scale) {
    savePng(c, o.out + "/" + name + ".png", scale);
    if (o.grey) savePng(g, o.out + "/" + name + "_grey.png", scale);
  }
};

std::string tagOf(const Opts& o, uint32_t seed) { return std::string(o.before ? "before_" : "") + "seed" + std::to_string(seed); }

// ---------------------------------------------------------------- the report lines of one column
void reportColumn(const ColumnRun& r) {
  int wa = 0;
  for (const Cell& e : r.cell) wa = std::max(wa, e.area);
  int maxShare = 0, maxIcon = 0;
  for (int m = 1; m < NM; m++) {
    const Cell& e = r.cell[m];
    if (!e.have) continue;
    maxShare = std::max(maxShare, e.maxShare);
    if (e.iconArea) maxIcon = std::max(maxIcon, 100 * e.iconChanged / e.iconArea);
  }
  rep("%-30s T%d area %3d  worn: pairs %2d twin %2d colour-only %2d | icons: twin %2d colour-only %2d | dye share worn %2d%% icon %2d%%\n",
      r.col.tag.c_str(), r.tier, wa, r.worn.pairs, r.worn.twin, r.worn.colour, r.icons.twin, r.icons.colour, maxShare, maxIcon);
  for (const PairNote& n : r.worn.notes) rep("    worn %s\n", noteText(n, wornViewNames()).c_str());
  for (const PairNote& n : r.icons.notes) rep("    icon %s\n", noteText(n, nullptr).c_str());
}

// ---------------------------------------------------------------- the worn matrices (a sheet per group, 12 columns a sheet)
void wornSheets(const Opts& o, uint32_t seed, const std::vector<ColumnRun>& runs) {
  const int cw = 40, ch = 52, lab = 44, top = 10, perSheet = 12;
  for (int g = 0; g < G_COUNT; g++) {
    std::vector<const ColumnRun*> cols;
    for (const ColumnRun& r : runs) if (r.col.g == g) cols.push_back(&r);
    for (size_t s0 = 0; s0 < cols.size(); s0 += perSheet) {
      const int n = (int)std::min<size_t>(perSheet, cols.size() - s0);
      Sheet sh(lab + n * kWornViews * cw + 8, top + NM * ch + 4, kBg);
      for (int ci = 0; ci < n; ci++) {
        const ColumnRun& r = *cols[s0 + (size_t)ci];
        sh.label(lab + ci * kWornViews * cw + 2, 1, r.col.tag.substr(r.col.tag.find(' ') + 1));
        for (int m = 0; m < NM; m++) {
          const Cell& e = r.cell[m];
          if (!e.have) continue;
          const Verdict w = worstOf(r.worn, m);
          for (int v = 0; v < kWornViews; v++) {
            const int cx = lab + (ci * kWornViews + v) * cw, cy = top + m * ch;
            sh.figure(e.fig[v], cx, cy, cw, ch);
            if (w != V_DISTINCT) sh.box(cx, cy, cw, ch, w == V_TWIN ? kTwin : kColourOnly);
          }
        }
      }
      for (int m = 0; m < NM; m++) sh.label(2, top + m * ch + ch / 2, makerShort(m));
      sh.save(o, std::string("worn_") + grpName(g) + "_" + std::to_string(s0 / perSheet + 1) + "_" + tagOf(o, seed), 3);
    }
  }
}

// ---------------------------------------------------------------- the icon matrix (rows makers, columns pieces)
void iconSheet(const Opts& o, uint32_t seed, const std::vector<ColumnRun>& runs) {
  const int cell = 20, lab = 44, top = 10;
  Sheet sh(lab + (int)runs.size() * cell + 4, top + NM * cell + 4, kBg2);
  for (size_t ci = 0; ci < runs.size(); ci++) {
    const ColumnRun& r = runs[ci];
    char t[8];
    std::snprintf(t, sizeof t, "%c%d", (char)std::toupper((unsigned char)grpName(r.col.g)[0]), r.tier);
    sh.label(lab + (int)ci * cell + 3, 1, t);
    for (int m = 0; m < NM; m++) {
      const Cell& e = r.cell[m];
      if (!e.have) continue;
      const int ox = lab + (int)ci * cell + 2, oy = top + m * cell + 2;
      sh.blit(e.icon, ox, oy);
      const Verdict w = worstOf(r.icons, m);
      if (w != V_DISTINCT) sh.box(ox - 2, oy - 2, 20, 20, w == V_TWIN ? kTwin : kColourOnly);
    }
  }
  for (int m = 0; m < NM; m++) sh.label(2, top + m * cell + 7, makerShort(m));
  sh.save(o, "icons_matrix_" + tagOf(o, seed), 3);
}

// ---------------------------------------------------------------- whole figures: the kits and the census
void figureSheet(const Opts& o, uint32_t seed, const char* name, const std::vector<std::pair<std::string, std::string>>& colSpecs) {
  // colSpecs: (column label, spec with %s for the people and @ for the seed; a leading "w:" draws the woman, ":R" facing R)
  const int cw = 40, ch = 52, lab = 44, top = 10;
  FpScope scope(!o.before);
  Sheet sh(lab + (int)colSpecs.size() * cw + 4, top + 12 * ch + 4, kBg);
  for (int p = 1; p < NM; p++) {
    for (size_t c = 0; c < colSpecs.size(); c++) {
      std::string spec = colSpecs[c].second;
      const bool woman = spec.rfind("w:", 0) == 0;
      if (woman) spec = spec.substr(2);
      int facing = 0;
      if (spec.size() > 2 && spec.compare(spec.size() - 2, 2, ":R") == 0) { facing = 2; spec.resize(spec.size() - 2); }
      for (size_t i; (i = spec.find("%s")) != std::string::npos;) spec.replace(i, 2, makerName(p));
      const FpLook fl{"x", woman ? "woman" : "man", makerName(p), spec};
      std::string bad;
      const art::HumanLook L = lookOf(fl, seed, &bad);
      if (!bad.empty()) rep("bad spec %s: %s\n", spec.c_str(), bad.c_str());
      sh.figure(figure(L, facing, kWhole), lab + (int)c * cw, top + (p - 1) * ch, cw, ch);
    }
    sh.label(2, top + (p - 1) * ch + ch / 2, makerShort(p));
  }
  for (size_t c = 0; c < colSpecs.size(); c++) sh.label(lab + (int)c * cw + 2, 1, colSpecs[c].first);
  sh.save(o, std::string(name) + "_" + tagOf(o, seed), 3);
}
void kitSheet(const Opts& o, uint32_t seed) {
  std::vector<std::pair<std::string, std::string>> cols;
  for (int body = 0; body < 2; body++)
    for (int t = 1; t <= 6; t++) cols.push_back({(body ? "W" : "M") + std::string("T") + std::to_string(t), std::string(body ? "w:heroine=1," : "hero=1,") + "kit=%s:" + std::to_string(t) + ":@"});
  figureSheet(o, seed, "kits_T1-T6_man_woman", cols);
}
void censusSheet(const Opts& o, uint32_t seed) {
  static const char* const kNpc[7][2] = {{"G0", "guard:0"}, {"G1", "guard:1"}, {"G2", "guard:2"}, {"SO", "soldier:0"}, {"BA", "bandit:0"}, {"CA", "captain:1"}, {"KI", "king:2"}};
  std::vector<std::pair<std::string, std::string>> cols;
  for (const auto& n : kNpc)
    for (int f = 0; f < 2; f++) cols.push_back({std::string(n[0]) + (f ? "R" : "D"), std::string("npc=%s:") + n[1] + ":@" + (f ? ":R" : "")});
  figureSheet(o, seed, "census_guard0-2_soldier_bandit_captain_king_DR", cols);
}

// ---------------------------------------------------------------- the swatches: dyes, bands, marks (1x, 3x) and icons
// a pool view drawn with the people's dye ramps (dye, dye2), the ink dark and every other slot in `other` (the mark on a
// leather, the filigree in gold)
Canvas markView(const dl::pool::ViewRec& v, const Dye& d, uint32_t other, dl::Material om) {
  Canvas c(v.w, v.h);
  uint32_t r1[5], r2[5], lea[5];
  dl::rampA3(hexc(d.dye), dl::Material::Cloth, r1);
  dl::rampA3(hexc(d.dye2 ? d.dye2 : d.dye), dl::Material::Cloth, r2);
  dl::rampA3(other, om, lea);
  const int first = firstDyeSlot<dl::Slot>();
  for (int y = 0; y < v.h; y++)
    for (int x = 0; x < v.w; x++) {
      const uint8_t code = v.cells.empty() ? 0 : v.cells[(size_t)(y * v.w + x)];
      if (code < 2) continue;
      const int s = (int)dl::codeSlot(code), st = dl::codeStep(code);
      uint32_t col = lea[st];
      if (s == (int)dl::Slot::Ink) col = rgba(28, 22, 26);
      else if (s == first || (s >= first + 2 && s < first + 10)) col = r1[std::max(1, st)];
      else if (s == first + 1 || s >= first + 10) col = r2[std::max(1, st)];
      c.set(x, y, col | 0xFF000000u);
    }
  return c;
}
// the largest view of the role's parts whose key starts with the motif (then a digit); empty canvas: none drawn
Canvas motifView(const std::string& role, const Dye& d) {
  const dl::pool::Pool& P = dl::pool::get();
  const dl::pool::ViewRec* best = nullptr;
  for (int pi : P.partsWithRole(dl::Role::Overlay, role)) {
    const dl::pool::PartRec& pr = P.parts[(size_t)pi];
    for (uint32_t vi = pr.firstView; vi < pr.firstView + pr.nViews; vi++) {
      const dl::pool::ViewRec& v = P.views[vi];
      const std::string& k = P.s(v.key);
      const size_t n = std::strlen(d.motif);
      if (k.size() > n && k.compare(0, n, d.motif) == 0 && std::isdigit((unsigned char)k[n]) && (!best || v.w * v.h > best->w * best->h)) best = &v;
    }
  }
  const bool gold = role == "finish.filigree";
  return best ? markView(*best, d, gold ? rgba(200, 160, 32) : rgba(141, 85, 54), gold ? dl::Material::Metal : dl::Material::Leather) : Canvas();
}
void swatchSheet(const Opts& o, uint32_t seed) {
  const int rowH = 56, lab = 70, top = 14;
  Sheet sh(lab + 520, top + 12 * rowH + 4, kBg2);
  sh.label(lab, 2, "DYE   DYE2  BAND (X+Y)     MARK 1X  MARK 3X        FILIGREE 3X     ICONS T1 COAT, T2 GLOVES, T3 STEEL");
  const Col coat{"", G_BODY, ItemKind::Armor, kLeaT1, 0}, glov{"", G_GLOVES, ItemKind::Gloves, kLeaT2, 0}, steel{"", G_BODY, ItemKind::Armor, kSteT3, 0};
  FpScope scope(!o.before);
  for (int p = 1; p < NM; p++) {
    const Dye d = dyeOf(p);
    const int y0 = top + (p - 1) * rowH;
    sh.label(2, y0 + 4, makerName(p));
    sh.label(2, y0 + 14, d.motif);
    if (!haveTable()) { sh.label(lab, y0 + 4, "NO TABLE (rpg/core/fingerprint.h)"); continue; }
    for (int y = 0; y < 24; y++) for (int x = 0; x < 30; x++) { sh.set(lab + x, y0 + y, hexc(d.dye) | 0xFF000000u); if (d.dye2) sh.set(lab + 36 + x, y0 + y, hexc(d.dye2) | 0xFF000000u); }
    for (int y = 0; y < 24; y++)
      for (int x = 0; x < 72; x++) {
        const int ph = d.period > 0 ? (x + y) % d.period : 0;
        sh.set(lab + 72 + x, y0 + y, hexc(ph < d.run || !d.dye2 ? d.dye : d.dye2) | 0xFF000000u);
      }
    const Canvas mk = motifView("finish.mark", d), fil = motifView("finish.filigree", d);
    if (mk.w) { sh.blit(mk, lab + 156, y0); sh.blit(mk, lab + 196, y0, 3); }
    else sh.label(lab + 156, y0 + 4, "NO MARK");
    if (fil.w) sh.blit(fil, lab + 290, y0, 3);
    else sh.label(lab + 290, y0 + 4, "NO FILIGREE");
    int ix = lab + 380;
    for (const Col* c : {&coat, &glov, &steel}) {
      Item it;
      if (!makeItem(seed, p, *c, it)) continue;
      sh.blit(art::itemIconLook(gearIcon(it, maker(p, seed))), ix, y0, 2);
      ix += 40;
    }
  }
  sh.save(o, std::string("swatches") + (o.before ? "_before" : ""), 3);
}

bool parseSeeds(const char* s, uint32_t& a, uint32_t& b) {
  const char* dots = std::strstr(s, "..");
  a = (uint32_t)std::strtoul(s, nullptr, 10);
  b = dots ? (uint32_t)std::strtoul(dots + 2, nullptr, 10) : a;
  return a >= 1 && b >= a;
}

int runSeed(const Opts& o, uint32_t seed) {
  const std::string tag = tagOf(o, seed);
  g_rep = std::fopen((o.out + "/report_" + tag + ".txt").c_str(), "w");
  if (!g_rep) { std::printf("cannot write %s\n", o.out.c_str()); return 2; }
  rep("P5 fingerprint gallery, seed %u%s (kit cultures gear::kitCulture(people, seed)); table %s, dye slots %s, FIX_FP bit %u.\n"
      "Per pair of makers: xor (silhouette px), grey (luma differs by 12+), distinct (px dE 10+ from every colour of the other);\n"
      "twin = all three within max(2, area/40) px (icons max(3, area/40)) in every view; colour-only = shape and grey twins.\n\n",
      seed, o.before ? ", BEFORE (FIX_FP off)" : "", haveTable() ? "rpg/core/fingerprint.h" : "ABSENT", haveDyeSlots() ? "in" : "ABSENT", fixFp());
  const bool all = !o.matrix && !o.kits && !o.census && !o.icons && !o.swatches;
  if (all || o.matrix || o.icons) {
    std::vector<ColumnRun> runs;
    int wt = 0, wc = 0, wp = 0, it = 0, ic = 0;
    for (const Col& c : columns()) {
      runs.push_back(runColumn(seed, c, !o.before));
      reportColumn(runs.back());
      wt += runs.back().worn.twin; wc += runs.back().worn.colour; wp += runs.back().worn.pairs;
      it += runs.back().icons.twin; ic += runs.back().icons.colour;
    }
    rep("\n%zu columns, %d pairs: worn %d twin, %d colour-only; icons %d twin, %d colour-only\n", runs.size(), wp, wt, wc, it, ic);
    if (all || o.matrix) wornSheets(o, seed, runs);
    iconSheet(o, seed, runs);
  }
  if (all || o.kits) kitSheet(o, seed);
  if (all || o.census) censusSheet(o, seed);
  if ((all || o.swatches) && seed == o.seedA) swatchSheet(o, seed);
  std::fclose(g_rep);
  g_rep = nullptr;
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  Opts o;
  if (argc < 2 || argv[1][0] == '-') { std::printf("usage: fingerprint_gallery <outDir> [--seeds A..B] [--before] [--grey] [--matrix|--kits|--census|--icons|--swatches]\n"); return 2; }
  o.out = argv[1];
  for (int i = 2; i < argc; i++) {
    const char* a = argv[i];
    if (!std::strcmp(a, "--seeds") && i + 1 < argc) { if (!parseSeeds(argv[++i], o.seedA, o.seedB)) { std::printf("bad --seeds\n"); return 2; } }
    else if (!std::strcmp(a, "--seed") && i + 1 < argc) o.seedA = o.seedB = (uint32_t)std::strtoul(argv[++i], nullptr, 10);
    else if (!std::strcmp(a, "--before")) o.before = true;
    else if (!std::strcmp(a, "--grey")) o.grey = true;
    else if (!std::strcmp(a, "--matrix")) o.matrix = true;
    else if (!std::strcmp(a, "--kits")) o.kits = true;
    else if (!std::strcmp(a, "--census")) o.census = true;
    else if (!std::strcmp(a, "--icons")) o.icons = true;
    else if (!std::strcmp(a, "--swatches")) o.swatches = true;
    else { std::printf("fingerprint_gallery: unknown arg %s\n", a); return 2; }
  }
  art::RigScope rig(2);
  int rc = 0;
  for (uint32_t s = o.seedA; s <= o.seedB; s++) rc |= runSeed(o, s);
  return rc;
}
