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
//     --slice   (P5b, STYLE_P5B 9.2) the style slice's figures one PNG each (slice_<people>_T<t>_<m|w>_s<seed>_<D|R>[_ns|_walk|_head])
//               and its kits' icons (slice_icon_<people>_T<t>_<piece>_s<seed>), for tools/doll/p5b_pack.py
//     --figs F  (P5b) one PNG per facing for each line `name who facings spec` of F (the look-and-adjust loop)
// Writes <outDir>/report_<tag>.txt (every column's pairs, the twins and colour-only pairs named with their three measures
// and the pixel they differ most at, the dye shares) and the sheets (<= 8000 px a side).
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

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
  bool slice = false;      // (P5b) --slice
  std::string figs;        // (P5b) --figs FILE
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
void swatchSheet(const Opts& o, uint32_t seed) {
  const int rowH = 96, lab = 70, top = 14;
  Sheet sh(lab + 520, top + 12 * rowH + 4, kBg2);
  sh.label(lab, 2, "DYE   DYE2  BAND (X+Y)     T1 COAT WORN 1X, 2X      T5 MASTER 2X     ICONS T1 COAT, T2 GLOVES, T3 STEEL");
  const Col master{"", G_BODY, ItemKind::Armor, kAllT5, 0}, coat{"", G_BODY, ItemKind::Armor, kLeaT1, 0}, glov{"", G_GLOVES, ItemKind::Gloves, kLeaT2, 0}, steel{"", G_BODY, ItemKind::Armor, kSteT3, 0};
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
    const Canvas mk = pieceCrop(seed, p, coat, kWhole), fil = pieceCrop(seed, p, master, kWhole);
    if (mk.w) { sh.blit(mk, lab + 156, y0); sh.blit(mk, lab + 196, y0, 2); }
    
    if (fil.w) sh.blit(fil, lab + 300, y0, 2);
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

// ---------------------------------------------------------------- (P5b, STYLE_P5B 9.2) the style slice, figure by figure
// --slice: every slice people's (Jade, Imperial, Fjordfolk) T1/T3/T5 ladder kit (sword, the seed), man and woman, facings D
// and R, idle frame 0 and the walk's passing frame; each figure its own 1x PNG on a clear 64x64 canvas, the pivot at (32, 60)
// (so the pack aligns them), with and without the shield; every kit piece's 16 px icon; measure lines (S1 masks, S2 shares).
// --figs FILE: the same for free lines `name who facing spec` (who man|woman, facing D|R|U|L; the look-and-adjust loop).
constexpr int kFigW = 64, kFigH = 64, kFigPx = 32, kFigPy = 60;
Canvas figCanvas(const Fig& f) {
  Canvas c(kFigW, kFigH);
  for (int y = 0; y < f.full.h; y++)
    for (int x = 0; x < f.full.w; x++) {
      const uint32_t v = f.full.get(x, y);
      if (v >> 24) c.set(kFigPx - f.px + x, kFigPy - f.py + y, v);
    }
  return c;
}
int facingOf(char c) { return c == 'U' ? 1 : c == 'R' ? 2 : c == 'L' ? 3 : 0; }
void saveFig(const Opts& o, const std::string& name, const art::HumanLook& L, int facing, art::Clip clip = art::Clip::Idle, int frame = 0) {
  const Fig f = figure(L, facing, kWhole, nullptr, clip, frame);
  savePng(figCanvas(f), o.out + "/" + name + ".png", 1);
}
int figsRun(const Opts& o, const std::string& file) {
  FILE* fp = std::fopen(file.c_str(), "r");
  if (!fp) { std::printf("cannot read %s\n", file.c_str()); return 2; }
  char line[2048];
  int n = 0;
  while (std::fgets(line, sizeof line, fp)) {
    char name[256], who[32], fc[8];
    int off = 0;
    if (line[0] == '#' || std::sscanf(line, "%255s %31s %7s %n", name, who, fc, &off) < 3) continue;
    std::string spec = line + off;
    while (!spec.empty() && (spec.back() == '\n' || spec.back() == '\r' || spec.back() == ' ')) spec.pop_back();
    const FpLook fl{name, who, "-", spec};
    std::string bad;
    const art::HumanLook L = lookOf(fl, o.seedA, &bad);
    if (!bad.empty()) std::printf("%s: bad spec %s\n", name, bad.c_str());
    for (const char* f = fc; *f; f++) {
      saveFig(o, std::string(name) + "_" + *f, L, facingOf(*f));
      if (std::getenv("EMB_FIGS_DUMP")) {   // the composed frame's slot and step per pixel (the look-and-adjust loop's probe)
        const dl::DollRecipe r = dl::resolve(L);
        const dl::FrameProbe p = dl::probeFrame(r, art::RigPart::Loco, facingOf(*f), art::Clip::Idle, 0, 0, true);
        FILE* df = std::fopen((o.out + "/" + name + "_" + *f + "_codes.txt").c_str(), "w");
        if (df && p.ok) {
          for (int y = 0; y < p.h; y++) {
            for (int x = 0; x < p.w; x++) {
              const uint8_t c = p.code[(size_t)(y * p.w + x)];
              std::fprintf(df, "%s", c < 2 ? " .." : (" " + std::string(1, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"[(int)dl::codeSlot(c) % 52]) + std::to_string(dl::codeStep(c))).c_str());
            }
            std::fprintf(df, "\n");
          }
          std::fprintf(df, "flags:");
          for (const std::string& fl : r.flags) std::fprintf(df, " %s", fl.c_str());
          std::fprintf(df, "\nfin shield:");
          for (int k = 0; k < 5; k++) std::fprintf(df, " %d", r.fin[2][k]);
          std::fprintf(df, "\nfin helm:");
          for (int k = 0; k < 5; k++) std::fprintf(df, " %d", r.fin[1][k]);
          std::fprintf(df, "\nparts:");
          for (int pi : r.parts) std::fprintf(df, " %s", dl::partName(pi));
          std::fprintf(df, "\n");
        }
        if (df) std::fclose(df);
      }
    }
    n++;
  }
  std::fclose(fp);
  std::printf("figs: %d looks -> %s\n", n, o.out.c_str());
  return 0;
}
// (P5b, S2) the people's share of a figure, idle D: its dye slots and the cloth the signature dresses in the people's pair
// (the layer's Accent, the cloak, the crest's Plume, the shield's Field, a soft head's or a T1 cap's Head), steps 1-4, over its
// opaque px
void sliceShare(FILE* mf, const char* tag, const art::HumanLook& L) {
  const dl::DollRecipe r = dl::resolve(L);
  const dl::FrameProbe p = dl::probeFrame(r, art::RigPart::Loco, 0, art::Clip::Idle, 0, 0, true);
  if (!p.ok || !mf) return;
  bool body = false, helm = false, shield = false, cloak = false;
  for (const std::string& f : r.flags) {
    body |= f.rfind("sigbody-", 0) == 0; helm |= f.rfind("sighelm-", 0) == 0;
    shield |= f.rfind("sigshield-", 0) == 0; cloak |= f.rfind("sigcloak-", 0) == 0;
  }
  int area = 0, dye = 0, cloth = 0;
  for (size_t k = 0; k < p.code.size(); k++) {
    const uint8_t c = p.code[k];
    if (c < 2) continue;
    area++;
    const dl::Slot sl = dl::codeSlot(c);
    if (dl::codeStep(c) == 0) continue;   // (the outline ring is the figure's ink, whatever its slot)
    if (dl::isDyeSlot(sl)) { dye++; continue; }
    if ((body && sl == dl::Slot::Accent) || (cloak && sl == dl::Slot::Cloak) || (helm && (sl == dl::Slot::Plume || sl == dl::Slot::Head)) ||
        (shield && sl == dl::Slot::Field))
      cloth++;
  }
  std::fprintf(mf, "share %s area %d dye %d cloth %d total %.1f%%\n", tag, area, dye, cloth, area ? 100.0 * (dye + cloth) / area : 0.0);
}
// (P5b fix r1) the head alone for the slice's S1 head line: the helm parts and every hat the figure wears through the guise
// path (a T1-T2 soft head: the jingasa, a hood, a band, a circlet; headwear/cloth/**), never a collar or the hair. G_HELM
// (fingerprint_audit.h, the P5 audit's helm piece) counts headwear/helm/** only, so a cloth head measured 0 px
Canvas headCanvas(const art::HumanLook& L, int facing) {
  Canvas hc(kFigW, kFigH);
  const dl::DollRecipe r = dl::resolve(L);
  const dl::FrameProbe p = dl::probeFrame(r, art::RigPart::Loco, facing, art::Clip::Idle, 0, 0, true);
  if (!p.ok) return hc;
  for (int y = 0; y < p.h; y++)
    for (int x = 0; x < p.w; x++) {
      const int k = y * p.w + x;
      const uint8_t c = p.code[(size_t)k];
      const int o = p.owner[(size_t)k];
      if (c < 2 || o < 0 || o >= (int)p.items.size()) continue;
      const auto& it = p.items[(size_t)o];
      const bool hat = it.has(dl::Role::Helm) || (it.has(dl::Role::Headwear) && it.part.find("headwear/collar/") == std::string::npos);
      if (hat) hc.set(kFigPx - p.px + x, kFigPy - p.py + y, codeColour(r, c));
    }
  return hc;
}
// (P5b production) EMB_SLICE_PEOPLE=dune,steppe,... and EMB_SLICE_TIERS=1234 pick other peoples and tiers (the areas' gates);
// unset: the style slice's three at T1, T3 and T5
std::vector<std::string> slicePeople() {
  std::vector<std::string> v;
  const char* e = std::getenv("EMB_SLICE_PEOPLE");
  std::string s = e && *e ? e : "jade,imperial,fjordfolk";
  for (size_t a = 0; a <= s.size();) {
    const size_t b = s.find(',', a);
    if (b != a) v.push_back(s.substr(a, b == std::string::npos ? std::string::npos : b - a));
    if (b == std::string::npos) break;
    a = b + 1;
  }
  return v;
}
std::vector<int> sliceTiers() {
  std::vector<int> v;
  const char* e = std::getenv("EMB_SLICE_TIERS");
  for (const char* c = e && *e ? e : "135"; *c; c++) if (*c >= '1' && *c <= '6') v.push_back(*c - '0');
  return v;
}
int sliceRun(const Opts& o) {
  const std::vector<std::string> kPeople = slicePeople();
  const std::vector<int> kTiers = sliceTiers();
  FpScope scope(!o.before);
  FILE* mf = std::fopen((o.out + "/slice_shares.txt").c_str(), "w");
  for (uint32_t seed = o.seedA; seed <= o.seedB; seed++)
    for (const std::string& ps : kPeople)
      for (int t : kTiers)
        for (int body = 0; body < 2; body++) {
          const char* p = ps.c_str();
          char spec[160], base[96];
          std::snprintf(spec, sizeof spec, "%skit=%s:%d:%u", body ? "heroine=1," : "hero=1,", p, t, seed);
          std::snprintf(base, sizeof base, "slice_%s_T%d_%s_s%u", p, t, body ? "w" : "m", seed);
          for (int ns = 0; ns < 2; ns++) {
            // (P5b fix r1) truly without the shield: the kit sets the shield's form and style, and doll_resolve's shieldOf() reads
            // any of shield, shieldStyle and shieldForm, so shield=0 alone drew it; shieldForm=9 is ShieldForm::None + 1
            const FpLook fl{base, body ? "woman" : "man", p, std::string(spec) + (ns ? ",shield=0,shieldForm=9,shieldStyle=0" : "")};
            const art::HumanLook L = lookOf(fl, seed);
            if (!ns) sliceShare(mf, base, L);
            for (int f : {0, 2}) {
              const std::string nm = std::string(base) + (f ? "_R" : "_D") + (ns ? "_ns" : "");
              saveFig(o, nm, L, f);
              if (!ns) {
                // the walk's passing frame (frame 2 of the LOCO walk) and the head alone (the helm and headwear parts)
                saveFig(o, nm + "_walk", L, f, art::Clip::Walk, 2);
                savePng(headCanvas(L, f), o.out + "/" + nm + "_head.png", 1);
              }
            }
          }
          if (body) continue;
          // the kit's pieces as icons (helm body shield gloves boots cloak)
          cult::Archetype a{};
          gear::peopleOf(p, a);
          const gear::LadderKit K = gear::ladderKit(a, t, 0, "", -1, seed);
          const auto look = K.lookup();
          const std::pair<const char*, std::pair<bool, const Item*>> pieces[6] = {
              {"helm", {K.hasHelmet, &K.helmet}}, {"body", {K.hasArmor, &K.armor}}, {"shield", {K.shieldMade, &K.shield}},
              {"gloves", {K.hasGloves, &K.gloves}}, {"boots", {K.hasBoots, &K.boots}}, {"cloak", {K.hasCloak, &K.cloak}}};
          for (const auto& pc : pieces) {
            if (!pc.second.first) continue;
            const Item& it = *pc.second.second;
            const cult::Culture* mk = it.culture ? look(it.culture) : nullptr;
            savePng(art::itemIconLook(gearIcon(it, mk)), o.out + "/slice_icon_" + p + "_T" + std::to_string(t) + "_" + pc.first + "_s" + std::to_string(seed) + ".png", 1);
          }
        }
  if (mf) std::fclose(mf);
  std::printf("slice -> %s\n", o.out.c_str());
  return 0;
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
    else if (!std::strcmp(a, "--slice")) o.slice = true;
    else if (!std::strcmp(a, "--figs") && i + 1 < argc) o.figs = argv[++i];
    else if (!std::strcmp(a, "--pool") && i + 1 < argc) {   // (P5b production) a pool file (pxc --bin): the look-and-adjust loop
      std::string err;
      if (!dl::loadPoolFile(argv[++i], &err)) { std::printf("fingerprint_gallery: --pool: %s\n", err.c_str()); return 1; }
    }
    else { std::printf("fingerprint_gallery: unknown arg %s\n", a); return 2; }
  }
  art::RigScope rig(2);
  if (!o.figs.empty()) return figsRun(o, o.figs);
  if (o.slice) return sliceRun(o);
  int rc = 0;
  for (uint32_t s = o.seedA; s <= o.seedB; s++) rc |= runSeed(o, s);
  return rc;
}
