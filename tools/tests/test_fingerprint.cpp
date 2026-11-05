// rpg_test --fingerprint [--seeds A..B] [--report] [--gates] [--selftest] (P5 GATES, docs/FIX_BATCH_P5.md 7): every people's
// fingerprint on its armour, measured. Over fp::kRows, the gallery's columns (tools/preview/fingerprint_audit.h: every body,
// helm and shield form x material x tier the shops make, gloves, boots, cloaks) and tools/doll/fp_looks.txt (the census and
// the kits):
//   F1 table      primaries >= dE 25 apart; two-tone signatures >= 25; primaries >= 20 from every leather, bronze, iron,
//                 steel, gilded and brass step 1-3 (doll_ramps.cpp's tables); dye2 >= 20 from its dye; 12 motifs, all
//                 different, each drawn in finish.mark and finish.filigree
//   F2 worn       no twin pair in any column (four views, tolerance max(2, area / 40)); colour-only <= 25 % of a T1-T3 column
//   F3 icons      no twin pair (tolerance max(3, area / 40))
//   F4 material   dye shares under FIX_BATCH_P5 3.5's caps (worn and icons); the worn leather coats and cap keep their mean
//                 hue in 15-45 and their saturation >= the pre-P5 value - 0.10; icons_gallery --check (D117) passes
//   F5 agreement  every T1-T4 piece of a people: the worn D view holds >= 2 dye px in the row's dye family, the icon >= 3
//   F6 additive   FIX_FP on vs off (the columns' looks at the first seed, the fp_looks lines at every seed; idle and walk,
//                 four facings): identical opaque masks; every differing pixel a dye-slot pixel or inside P (mark, motif)
//   F7 classic    makers 0 (culture-0 pieces, the A3 hero, the T1 floor) and people-0 icons pixel-identical on vs off
//   F8 census     every worn armour slot of the 12 peoples' watch, soldiers, bandits, captains and kings carries the arms
//                 culture's people; two peoples' watch at one rank never twins (their armour pixels, D and R)
// --gates: any FAIL exits 1 (else the numbers only). --report: every column's pairs and shares. --selftest: an injected twin,
// a dye over its cap, a moved outline pixel and a census armour slot with no maker must each FAIL.
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "tools/preview/fingerprint_audit.h"
#include "tools/tests/tests.h"

namespace {

using namespace fpa;

struct Ctx {
  bool report = false, gates = false, quiet = false;
  uint64_t seedA = 1, seedB = 1;
  int fails = 0;
  std::string exeDir;
};
// a line's verdict: "F2 worn ... PASS|FAIL"; returns 1 on a FAIL
int verdict(Ctx& c, const char* line, bool ok, const std::string& detail) {
  if (!c.quiet) std::printf("%-14s %s  %s\n", line, ok ? "PASS" : "FAIL", detail.c_str());
  if (!ok) c.fails++;
  return ok ? 0 : 1;
}
std::string fmt(const char* f, ...) {
  char b[512];
  va_list ap;
  va_start(ap, f);
  std::vsnprintf(b, sizeof b, f, ap);
  va_end(ap);
  return b;
}

// ================================================================ F1 the table
// steps 1-3 of the material ramps as doll_ramps.cpp holds them (kLeather[6], kMetalKind 2-5, kTrim[0]: the brass)
std::vector<std::pair<std::string, uint32_t>> materialSteps() {
  std::vector<std::pair<std::string, uint32_t>> out;
  std::ifstream f(std::string(EMB_SOURCE_DIR) + "/rpg/art/doll/doll_ramps.cpp");
  std::stringstream ss;
  ss << f.rdbuf();
  const std::string src = ss.str();
  auto block = [&](const char* head) -> std::vector<uint32_t> {
    std::vector<uint32_t> v;
    const size_t a = src.find(head);
    if (a == std::string::npos) return v;
    const size_t b = src.find("\n};", a);
    const std::string body = src.substr(a, b - a);
    static const std::regex hx("0x([0-9A-Fa-f]{6})\\b");
    for (std::sregex_iterator i(body.begin(), body.end(), hx), e; i != e; ++i) v.push_back((uint32_t)std::strtoul((*i)[1].str().c_str(), nullptr, 16));
    return v;
  };
  const std::vector<uint32_t> L = block("const uint32_t kLeather[6][5] = {"), T = block("const uint32_t kTrim[4][5] = {");
  for (int i = 0; i < 6 && (size_t)(i * 5 + 3) < L.size(); i++)
    for (int s = 1; s <= 3; s++) out.push_back({"leather" + std::to_string(i) + "." + std::to_string(s), L[(size_t)(i * 5 + s)]});
  // kMetalKind: one {0x..} row a kind, comments after it; the first five hexes of each row
  {
    const size_t a = src.find("const uint32_t kMetalKind[12][5] = {");
    const size_t b = a == std::string::npos ? a : src.find("\n};", a);
    std::vector<std::vector<uint32_t>> rows;
    if (a != std::string::npos) {
      std::istringstream in(src.substr(a, b - a));
      std::string ln;
      static const std::regex hx("0x([0-9A-Fa-f]{6})\\b");
      while (std::getline(in, ln)) {
        const size_t p = ln.find_first_not_of(" \t");
        if (p == std::string::npos || ln.compare(p, 3, "{0x") != 0) continue;
        std::vector<uint32_t> r;
        for (std::sregex_iterator i(ln.begin(), ln.end(), hx), e; i != e && r.size() < 5; ++i) r.push_back((uint32_t)std::strtoul((*i)[1].str().c_str(), nullptr, 16));
        rows.push_back(r);
      }
    }
    static const struct { const char* n; int k; } kKinds[] = {{"bronze", 2}, {"iron", 3}, {"steel", 4}, {"gilded", 5}};
    for (const auto& k : kKinds)
      if ((size_t)k.k < rows.size() && rows[(size_t)k.k].size() == 5)
        for (int s = 1; s <= 3; s++) out.push_back({std::string(k.n) + "." + std::to_string(s), rows[(size_t)k.k][(size_t)s]});
  }
  for (int s = 1; s <= 3 && T.size() >= 5; s++) out.push_back({"brass." + std::to_string(s), T[(size_t)s]});
  return out;
}
// the keys the finish maps draw for a role (`role overlay finish.mark` / `finish.filigree`): every `key <k>` of the
// rpg/art/doll/maps/finish/*.pxm files serving it (the authored source; pxc.mjs --check keeps the pool equal to it)
std::set<std::string> finishKeys(const std::string& role) {
  std::set<std::string> keys;
  namespace fs = std::filesystem;
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(std::string(EMB_SOURCE_DIR) + "/rpg/art/doll/maps/finish", ec)) {
    if (e.path().extension() != ".pxm") continue;
    std::ifstream f(e.path());
    std::string ln, all;
    bool serves = false;
    std::set<std::string> ks;
    static const std::regex key(" key ([A-Za-z]+[0-9]+x[0-9]+)( |$)");
    while (std::getline(f, ln)) {
      if (ln.rfind("role ", 0) == 0 && ln.find(role) != std::string::npos) serves = true;
      std::smatch m;
      if (ln.rfind("view ", 0) == 0 && std::regex_search(ln, m, key)) ks.insert(m[1].str());
    }
    if (serves) keys.insert(ks.begin(), ks.end());
  }
  return keys;
}
bool motifIn(const std::set<std::string>& keys, const std::string& motif) {
  for (const std::string& k : keys)
    if (k.size() > motif.size() && k.compare(0, motif.size(), motif) == 0 && std::isdigit((unsigned char)k[motif.size()])) return true;
  return false;
}
int lineF1(Ctx& c) {
  if (!haveTable()) return verdict(c, "F1 table", false, "rpg/core/fingerprint.h absent (wave 1 not merged)");
  auto D = [](uint32_t a, uint32_t b) { return dE(hexc(a), hexc(b)); };
  double prim = 1e9, sig = 1e9, mat = 1e9, own = 1e9;
  std::string wp, ws, wm, wo;
  const auto mats = materialSteps();
  std::vector<std::string> motifs;
  bool bands = true;
  for (int a = 1; a < NM; a++) {
    const Dye x = dyeOf(a);
    motifs.push_back(x.motif);
    bands &= x.run >= 1 && x.run <= x.period && x.period <= 6 && (x.dye2 || x.run == x.period);
    if (x.dye2 && D(x.dye, x.dye2) < own) { own = D(x.dye, x.dye2); wo = x.name; }
    for (const auto& [n, v] : mats) if (D(x.dye, v) < mat) { mat = D(x.dye, v); wm = std::string(x.name) + "/" + n; }
    for (int b = a + 1; b < NM; b++) {
      const Dye y = dyeOf(b);
      if (D(x.dye, y.dye) < prim) { prim = D(x.dye, y.dye); wp = std::string(x.name) + "/" + y.name; }
      // the signature distance (p5_marks.py --table): each colour's nearest in the other's pair, the worse of both ways
      auto one = [&](const Dye& p, const Dye& q) {
        double m = 0;
        for (uint32_t u : {p.dye, p.dye2 ? p.dye2 : p.dye}) m = std::max(m, std::min(D(u, q.dye), D(u, q.dye2 ? q.dye2 : q.dye)));
        return m;
      };
      const double s = std::max(one(x, y), one(y, x));
      if (s < sig) { sig = s; ws = std::string(x.name) + "/" + y.name; }
    }
  }
  int bad = 0;
  bad += verdict(c, "F1 table", prim >= 25.0, fmt("primaries pairwise min dE %.1f (%s) >= 25", prim, wp.c_str()));
  bad += verdict(c, "F1 table", sig >= 25.0, fmt("two-tone signatures min dE %.1f (%s) >= 25", sig, ws.c_str()));
  bad += verdict(c, "F1 table", mats.size() == 33 && mat >= 20.0, fmt("primary vs %zu material steps min dE %.1f (%s) >= 20", mats.size(), mat, wm.c_str()));
  bad += verdict(c, "F1 table", own >= 20.0, fmt("dye2 vs its dye min dE %.1f (%s) >= 20", own, wo.c_str()));
  bad += verdict(c, "F1 table", bands, "bands 1 <= run <= period <= 6");
  std::vector<std::string> u = motifs;
  std::sort(u.begin(), u.end());
  const bool distinct = std::unique(u.begin(), u.end()) == u.end() && motifs.size() == 12;
  std::string missing;
  const std::set<std::string> markKeys = finishKeys("finish.mark"), filKeys = finishKeys("finish.filigree");
  for (const std::string& m : motifs) {
    if (!motifIn(markKeys, m)) missing += " mark:" + m;
    if (!motifIn(filKeys, m)) missing += " filigree:" + m;
  }
  bad += verdict(c, "F1 motifs", distinct && missing.empty(), fmt("12 motifs %s, each drawn in finish.mark and finish.filigree%s%s", distinct ? "all different" : "NOT all different",
                                                                      missing.empty() ? "" : "; missing", missing.c_str()));
  return bad;
}

// ================================================================ F2-F5 over the columns
struct Caps { int worn, icon; };
Caps capsOf(const Col& col, int tier) {
  int w = 15;
  switch (col.g) {
    case G_BODY: w = tier <= 2 ? 15 : 20; break;
    case G_HELM: w = 25; break;
    case G_SHIELD: w = 30; break;
    case G_GLOVES: w = 40; break;
    case G_BOOTS: w = 20; break;
    default: w = 15; break;
  }
  const bool leatherBodyOrCap = col.mt.mat == Mat::Leather && (col.g == G_BODY || col.g == G_HELM);
  const int i = leatherBodyOrCap ? 10 : (col.g == G_GLOVES || col.g == G_BOOTS) ? 20 : 15;
  return {w, i};
}
struct ColTotals { int twins = 0, colourCols = 0, iconTwins = 0, shareBad = 0, iconShareBad = 0, f5Bad = 0, f5Checked = 0; std::vector<std::string> notes; };
// one column's F2-F5 numbers into the totals (seed for the notes)
void judgeColumn(const ColumnRun& r, uint64_t seed, ColTotals& t, bool report) {
  t.twins += r.worn.twin;
  t.iconTwins += r.icons.twin;
  if (r.tier >= 1 && r.tier <= 3 && r.worn.pairs && r.worn.colour * 4 > r.worn.pairs) {
    t.colourCols++;
    t.notes.push_back(fmt("s%llu %s: colour-only %d of %d pairs", (unsigned long long)seed, r.col.tag.c_str(), r.worn.colour, r.worn.pairs));
  }
  for (const PairNote& n : r.worn.notes) if (n.v == V_TWIN || report) t.notes.push_back(fmt("s%llu %s worn %s", (unsigned long long)seed, r.col.tag.c_str(), noteText(n, wornViewNames()).c_str()));
  for (const PairNote& n : r.icons.notes) if (n.v == V_TWIN || report) t.notes.push_back(fmt("s%llu %s icon %s", (unsigned long long)seed, r.col.tag.c_str(), noteText(n, nullptr).c_str()));
  for (int m = 1; m < NM; m++) {
    const Cell& e = r.cell[m];
    if (!e.have) continue;
    const Caps cap = capsOf(r.col, e.tier);
    if (e.maxShare > cap.worn) { t.shareBad++; t.notes.push_back(fmt("s%llu %s %s: worn dye share %d%% > %d%%", (unsigned long long)seed, r.col.tag.c_str(), makerShort(m), e.maxShare, cap.worn)); }
    // (P5b, STYLE_P5B 8.4 and 8.6, decision 3) a signed people's head, shield and cloak are the people's own drawing and cloth
    // (the head class, the field, the cloak: only the figure's 30 % cap holds them, S2); its body, helm, shield and cloak carry
    // the people by identity (the head family, the axes, the shield's form, the cloak's dominant): F5 accepts them by it
    const bool sig = fp::sig(m) && dl::fixOn(dl::FIX_SIG) && dl::fixOn(dl::FIX_FP);
    const bool sigCloth = sig && (r.col.g == G_HELM || r.col.g == G_SHIELD || r.col.g == G_CLOAK);
    const bool sigIdentity = sig && r.col.g != G_GLOVES && r.col.g != G_BOOTS;
    const int is = e.iconArea ? 100 * e.iconChanged / e.iconArea : 0;
    if (is > cap.icon && !sigCloth) { t.iconShareBad++; t.notes.push_back(fmt("s%llu %s %s: icon share %d%% > %d%%", (unsigned long long)seed, r.col.tag.c_str(), makerShort(m), is, cap.icon)); }
    if (e.tier >= 1 && e.tier <= 4) {
      t.f5Checked++;
      if (!sigIdentity && (e.fig[0].dyeFamily < 2 || e.iconFamily < 3)) {
        t.f5Bad++;
        if (t.f5Bad <= 40 || report) t.notes.push_back(fmt("s%llu %s %s: F5 worn D %d dye-family px (>= 2), icon %d (>= 3)", (unsigned long long)seed, r.col.tag.c_str(), makerShort(m), e.fig[0].dyeFamily, e.iconFamily));
      }
    }
  }
}
// the worn leather means (F4): the leather coats and the cap, man facing D, whole piece, against the pre-P5 look
int leatherMeans(Ctx& c, const Col& col, uint64_t seed, const ColumnRun& on) {
  const ColumnRun off = runColumn((uint32_t)seed, col, false);
  int bad = 0;
  double hLo = 999, hHi = -1, worstDrop = -9;
  std::string who;
  for (int m = 1; m < NM; m++) {
    if (!on.cell[m].have || !off.cell[m].have) continue;
    const HueSat a = meanHueSat(on.cell[m].fig[0].piece.px.begin(), on.cell[m].fig[0].piece.px.end());
    const HueSat b = meanHueSat(off.cell[m].fig[0].piece.px.begin(), off.cell[m].fig[0].piece.px.end());
    hLo = std::min(hLo, a.hue); hHi = std::max(hHi, a.hue);
    if (b.sat - a.sat > worstDrop) { worstDrop = b.sat - a.sat; who = makerShort(m); }
    if (a.hue < 15 || a.hue > 45 || a.sat < b.sat - 0.10) bad++;
  }
  verdict(c, "F4 leather", bad == 0, fmt("s%llu %s: mean hue %.0f..%.0f (15-45), sat drop <= %.2f (%s; <= 0.10)", (unsigned long long)seed, col.tag.c_str(), hLo, hHi, worstDrop, who.c_str()));
  return bad ? 1 : 0;
}
bool isLeatherMeanColumn(const Col& col) {
  return col.mt.mat == Mat::Leather && ((col.g == G_BODY && col.mt.tier == 1 && col.form >= 1 && col.form <= 2) || (col.g == G_HELM && col.form == 0));
}
// D117: icons_gallery --check (the materials read first), run beside this exe
int iconsGalleryCheck(Ctx& c) {
  const std::string exe = c.exeDir + "icons_gallery.exe";
  FILE* f = std::fopen(exe.c_str(), "rb");
  if (!f) return verdict(c, "F4 D117", false, "icons_gallery.exe not found beside rpg_test");
  std::fclose(f);
#ifdef _WIN32
  FILE* p = _popen(("\"\"" + exe + "\" --check 2>&1\"").c_str(), "r");
#else
  FILE* p = popen(("\"" + exe + "\" --check 2>&1").c_str(), "r");
#endif
  if (!p) return verdict(c, "F4 D117", false, "cannot run icons_gallery --check");
  char line[1024];
  int fl = 0, n = 0;
  std::string first;
  while (std::fgets(line, sizeof line, p)) {
    n++;
    if (std::strstr(line, "FAIL")) { if (!fl++) first = line; }
  }
#ifdef _WIN32
  const int rc = _pclose(p);
#else
  const int rc = pclose(p);
#endif
  while (!first.empty() && (first.back() == '\n' || first.back() == '\r')) first.pop_back();
  return verdict(c, "F4 D117", rc == 0 && fl == 0, fmt("icons_gallery --check rc %d, %d lines, %d FAIL%s%s", rc, n, fl, fl ? ": " : "", first.c_str()));
}
int linesF2toF5(Ctx& c) {
  ColTotals t;
  int leatherBad = 0, cols = 0;
  for (uint64_t s = c.seedA; s <= c.seedB; s++)
    for (const Col& col : columns()) {
      const ColumnRun r = runColumn((uint32_t)s, col, true);
      cols++;
      judgeColumn(r, s, t, c.report);
      if (c.report) std::printf("  s%llu %-30s T%d worn twin %2d colour %2d of %2d | icon twin %2d colour %2d\n", (unsigned long long)s, col.tag.c_str(), r.tier, r.worn.twin, r.worn.colour,
                                r.worn.pairs, r.icons.twin, r.icons.colour);
      if (isLeatherMeanColumn(col)) leatherBad += leatherMeans(c, col, s, r);
    }
  for (const std::string& n : t.notes) std::printf("    %s\n", n.c_str());
  int bad = 0;
  bad += verdict(c, "F2 worn", t.twins == 0, fmt("%d columns x seeds: %d twin pairs (four views, tol max(2, area/40))", cols, t.twins));
  bad += verdict(c, "F2 worn", t.colourCols == 0, fmt("%d T1-T3 columns over 25 %% colour-only pairs", t.colourCols));
  bad += verdict(c, "F3 icons", t.iconTwins == 0, fmt("%d twin icon pairs (tol max(3, area/40))", t.iconTwins));
  bad += verdict(c, "F4 shares", t.shareBad == 0 && t.iconShareBad == 0, fmt("%d worn and %d icon dye shares over their caps", t.shareBad, t.iconShareBad));
  bad += leatherBad;
  bad += verdict(c, "F5 agreement", t.f5Bad == 0, fmt("%d of %d T1-T4 pieces short of their dye family (worn D >= 2 px, icon >= 3 px)", t.f5Bad, t.f5Checked));
  return bad;
}

// ================================================================ F6 / F7 frames on vs off
struct FrameDiff { int frames = 0, maskPx = 0, foreignPx = 0, changedPx = 0; std::string first; };
struct Probed { dl::DollRecipe r; std::vector<dl::FrameProbe> p; };
constexpr int kProbeFrames = 8;   // four facings x (idle 0, walk 1)
Probed probeAll(const art::HumanLook& L, bool on) {
  FpScope scope(on);
  Probed o;
  o.r = dl::resolve(L);
  for (int f = 0; f < 4; f++)
    for (int k = 0; k < 2; k++) o.p.push_back(dl::probeFrame(o.r, art::RigPart::Loco, f, k ? art::Clip::Walk : art::Clip::Idle, 0, k, true));
  return o;
}
// one frame: same mask; a differing pixel is allowed only as a dye-slot pixel or inside P (strict: none allowed)
void diffFrame(const dl::FrameProbe& a, const dl::DollRecipe& ra, const dl::FrameProbe& b, const dl::DollRecipe& rb, bool strict, const std::string& tag, FrameDiff& d) {
  if (!a.ok && !b.ok) return;
  d.frames++;
  if (a.ok != b.ok || a.w != b.w || a.h != b.h || a.px != b.px || a.py != b.py) { d.maskPx += 1000; if (d.first.empty()) d.first = tag + ": frame size or pivot moved"; return; }
  for (size_t k = 0; k < a.code.size(); k++) {
    const bool oa = a.code[k] >= 2, ob = b.code[k] >= 2;
    if (oa != ob) { d.maskPx++; if (d.first.empty()) d.first = fmt("%s: mask differs at %d,%d", tag.c_str(), (int)(k % (size_t)a.w), (int)(k / (size_t)a.w)); continue; }
    if (!oa) continue;
    const uint32_t ca = codeColour(ra, a.code[k]), cb = codeColour(rb, b.code[k]);
    if (ca == cb) continue;
    d.changedPx++;
    const bool ok = !strict && (dyeCode(a.code[k]) || a.region[k] == (uint8_t)dl::Region::Panel || b.region[k] == (uint8_t)dl::Region::Panel);
    if (!ok) { d.foreignPx++; if (d.first.empty()) d.first = fmt("%s: px %d,%d %s%s step %d -> %s step %d", tag.c_str(), (int)(k % (size_t)a.w), (int)(k / (size_t)a.w), strict ? "(classic) " : "",
                                                                    dl::slotName(dl::codeSlot(b.code[k])), dl::codeStep(b.code[k]), dl::slotName(dl::codeSlot(a.code[k])), dl::codeStep(a.code[k])); }
  }
}
// (P5b) F6 is P5's own rule (the fingerprint adds colour, never an outline): it holds with FIX_SIG off, whose moves are the
// people's signature by design (STYLE_P5B 8.3: maker pieces wear new added views and the people's parts); F7 (maker-less and
// people-0 looks untouched) runs with every rule on, the signature's included
struct SigOff {
  uint32_t saved;
  explicit SigOff(bool off) : saved(dl::fixRules()) { if (off) dl::fixRules() = saved & ~(uint32_t)dl::FIX_SIG; }
  ~SigOff() { dl::fixRules() = saved; }
};
void diffLook(const art::HumanLook& L, bool strict, const std::string& tag, FrameDiff& d) {
  SigOff sig(!strict);
  const Probed on = probeAll(L, true), off = probeAll(L, false);
  static const char* const kF[kProbeFrames] = {"D idle", "D walk", "U idle", "U walk", "R idle", "R walk", "L idle", "L walk"};
  for (int i = 0; i < kProbeFrames; i++) diffFrame(on.p[(size_t)i], on.r, off.p[(size_t)i], off.r, strict, tag + " " + kF[i], d);
}
std::string fpLooksPath() { return std::string(EMB_SOURCE_DIR) + "/tools/doll/fp_looks.txt"; }
int linesF6F7(Ctx& c) {
  FrameDiff add, classic;
  int iconsOn = 0, iconsBad = 0;
  std::string firstIcon;
  for (const Col& col : columns()) {   // the columns' looks at the first seed
    const ColumnRun r = runColumn((uint32_t)c.seedA, col, true);
    for (int m = 0; m < NM; m++) {
      const Cell& e = r.cell[m];
      if (!e.have) continue;
      for (int body = 0; body < 2; body++) diffLook(e.look[body], m == 0, col.tag + " " + makerShort(m) + (body ? " woman" : " man"), m == 0 ? classic : add);
      if (m == 0) { iconsOn++; if (e.icon.px != e.iconBefore.px) { iconsBad++; if (firstIcon.empty()) firstIcon = col.tag; } }
    }
  }
  const std::vector<FpLook> looks = readFpLooks(fpLooksPath());
  for (uint64_t s = c.seedA; s <= c.seedB; s++)
    for (const FpLook& l : looks) {
      const art::HumanLook L = lookOf(l, (uint32_t)s);
      const bool none = l.people == "-";
      diffLook(L, none, fmt("s%llu %s", (unsigned long long)s, l.tag.c_str()), none ? classic : add);
    }
  for (int body = 0; body < 2; body++) diffLook(shirtLook(body), true, body ? "A3 woman" : "A3 man", classic);
  int bad = 0;
  bad += verdict(c, "F6 additive", !looks.empty() && add.maskPx == 0 && add.foreignPx == 0,
                 fmt("%d frames (+%zu fp_looks lines x seeds): %d changed px, %d mask px, %d outside the dye slots and P%s%s", add.frames, looks.size(), add.changedPx, add.maskPx, add.foreignPx,
                     add.first.empty() ? "" : "; first: ", add.first.c_str()));
  bad += verdict(c, "F7 classic", classic.maskPx == 0 && classic.foreignPx == 0 && iconsBad == 0,
                 fmt("%d maker-less frames: %d px differ; %d of %d people-0 icons differ%s%s", classic.frames, classic.foreignPx + classic.maskPx, iconsBad, iconsOn,
                     (classic.first + firstIcon).empty() ? "" : "; first: ", (classic.first.empty() ? firstIcon : classic.first).c_str()));
  return bad;
}

// ================================================================ F8 the census
// the census lines' armour slots against their people (first failure text in *why)
int censusMakers(const art::HumanLook& L, int people, std::string* why) {
  int bad = 0;
  for (int s : armourSlotsWorn(L)) {
    const int m = makerAt(L, s);
    if (m != people) { bad++; if (why && why->empty()) *why = fmt("%s maker %d, want %d", tierSlotName(s), m, people); }
  }
  return bad;
}
int lineF8(Ctx& c) {
  const std::vector<FpLook> looks = readFpLooks(fpLooksPath());
  int lines = 0, slots = 0, bad = 0, twins = 0, groups = 0;
  std::string first, firstTwin;
  for (uint64_t s = c.seedA; s <= c.seedB; s++) {
    std::map<std::string, std::vector<std::pair<int, std::vector<Img>>>> watch;   // rank tag prefix -> (people, views)
    for (const FpLook& l : looks) {
      if (l.spec.rfind("npc=", 0) != 0 || l.people == "-") continue;
      const art::HumanLook L = lookOf(l, (uint32_t)s);
      const int p = peopleIndex(l.people);
      lines++;
      slots += (int)armourSlotsWorn(L).size();
      std::string why;
      if (censusMakers(L, p, &why)) { bad++; if (first.empty()) first = fmt("s%llu %s: %s", (unsigned long long)s, l.tag.c_str(), why.c_str()); }
      const size_t us = l.tag.find('_');
      if (l.tag.rfind("watch", 0) == 0 && us != std::string::npos) {
        FpScope on(true);
        watch[l.tag.substr(0, us)].push_back({p, {figure(L, 0, kAllArmour).piece, figure(L, 2, kAllArmour).piece}});
      }
    }
    for (auto& [rank, v] : watch) {
      groups++;
      for (size_t a = 0; a < v.size(); a++)
        for (size_t b = a + 1; b < v.size(); b++) {
          std::vector<Cmp> cs;
          int area = 0;
          for (size_t i = 0; i < 2; i++) { cs.push_back(compare(v[a].second[i], v[b].second[i])); area = std::max({area, (int)v[a].second[i].px.size(), (int)v[b].second[i].px.size()}); }
          if (verdictOf(cs, area, 2) == V_TWIN) { twins++; if (firstTwin.empty()) firstTwin = fmt("s%llu %s %s=%s", (unsigned long long)s, rank.c_str(), makerShort(v[a].first), makerShort(v[b].first)); }
        }
    }
  }
  int r = 0;
  r += verdict(c, "F8 census", lines > 0 && bad == 0 && slots > 0, fmt("%d census lines x seeds, %d armour slots: %d lines with a slot not the arms culture's%s%s", lines, slots, bad, first.empty() ? "" : "; first: ", first.c_str()));
  r += verdict(c, "F8 watch", groups > 0 && twins == 0, fmt("%d watch ranks x seeds: %d twin pairs of peoples%s%s", groups, twins, firstTwin.empty() ? "" : "; first: ", firstTwin.c_str()));
  return r;
}

// ================================================================ the selftest: each injected defect must FAIL
int selftest(Ctx& c) {
  Ctx q = c;
  q.quiet = true;
  int missed = 0;
  auto expectFail = [&](const char* what, bool failed) { std::printf("  selftest %-36s %s\n", what, failed ? "FAILs as it must" : "MISSED"); if (!failed) missed++; };
  // 1. an injected twin: maker 2's views replaced by maker 1's
  {
    const ColumnRun r = runColumn(1, columns()[0], true);
    std::vector<std::vector<Img>> v(NM);
    std::vector<int> area(NM, 0);
    std::vector<bool> have(NM, false);
    for (int m = 0; m < NM; m++) {
      const Cell& e = r.cell[m];
      have[(size_t)m] = e.have;
      area[(size_t)m] = e.area;
      for (int i = 0; i < kWornViews; i++) v[(size_t)m].push_back(e.fig[i].piece);
    }
    v[2] = v[1];
    area[2] = area[1];
    have[1] = have[2] = true;
    expectFail("injected twin (F2)", pairsOf(v, area, have, 2).twin >= 1);
  }
  // 2. a dye over its cap: a T1 body's share at cap + 1
  {
    ColumnRun r = runColumn(1, columns()[0], true);
    r.cell[1].maxShare = capsOf(r.col, 1).worn + 1;
    ColTotals t;
    judgeColumn(r, 1, t, false);
    expectFail("dye over its cap (F4)", t.shareBad >= 1);
  }
  // 3. a moved outline pixel: the hero's frame against itself with one transparent pixel beside the outline made opaque
  {
    const Probed on = probeAll(shirtLook(0), true);
    dl::FrameProbe moved = on.p[0];
    bool done = false;
    for (int y = 1; y + 1 < moved.h && !done; y++)
      for (int x = 1; x + 1 < moved.w && !done; x++) {
        const size_t k = (size_t)(y * moved.w + x);
        if (moved.code[k] < 2 && moved.code[k + 1] >= 2) { moved.code[k] = moved.code[k + 1]; done = true; }
      }
    FrameDiff d;
    diffFrame(moved, on.r, on.p[0], on.r, false, "selftest", d);
    FrameDiff clean;
    diffFrame(on.p[0], on.r, on.p[0], on.r, false, "selftest", clean);
    expectFail("moved outline pixel (F6)", done && d.maskPx >= 1 && clean.maskPx == 0);
  }
  // 4. a census armour slot with no maker
  {
    const FpLook l{"watch0_x", "man", "dune", "npc=dune:guard:0:@"};
    art::HumanLook L = lookOf(l, 1);
    const std::vector<int> worn = armourSlotsWorn(L);
    const bool clean = censusMakers(L, peopleIndex("dune"), nullptr) == 0;
    const bool injected = !worn.empty() && setMaker(L, worn[0], 0);
    if (!injected) std::printf("  selftest: HumanLook::makers absent (wave 1 not merged): cannot inject\n");
    expectFail("census slot with no maker (F8)", injected && clean && censusMakers(L, peopleIndex("dune"), nullptr) >= 1);
  }
  return verdict(c, "selftest", missed == 0, fmt("%d of 4 injected defects missed", missed));
}

int fingerprintCmd(int argc, char** argv) {
  Ctx c;
  bool self = false;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], c.seedA, c.seedB);
    else if (!std::strcmp(argv[i], "--report")) c.report = true;
    else if (!std::strcmp(argv[i], "--gates")) c.gates = true;
    else if (!std::strcmp(argv[i], "--selftest")) self = true;
  }
  {
    const std::string a0 = argv[0];
    const size_t sl = a0.find_last_of("/\\");
    c.exeDir = sl == std::string::npos ? "" : a0.substr(0, sl + 1);
  }
  art::RigScope rig(2);
  std::printf("rpg_test --fingerprint: seeds %llu..%llu; table %s, dye slots %s, makers %s, FIX_FP bit %u\n", (unsigned long long)c.seedA, (unsigned long long)c.seedB,
              haveTable() ? "in" : "ABSENT", haveDyeSlots() ? "in" : "ABSENT", makerAt(art::HumanLook{}, 0) >= 0 ? "in" : "ABSENT", fixFp());
  if (self) {
    selftest(c);
  } else {
    lineF1(c);
    linesF2toF5(c);
    iconsGalleryCheck(c);
    linesF6F7(c);
    lineF8(c);
  }
  std::printf("fingerprint: %s (%d FAIL lines)\n", c.fails ? "FAIL" : "PASS", c.fails);
  return c.fails && (c.gates || self) ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--fingerprint", "P5: every people's fingerprint on armour (F1-F8) [--seeds A..B] [--report] [--gates] [--selftest]", fingerprintCmd);
