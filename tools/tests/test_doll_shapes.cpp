// rpg_test --forge-solve, --forge-shapes (P2c forge round 2, FORGE-DOLL; docs/P2C_CONTRACTS.md 3c.1, STYLE_P2 28.3-28.5): the
// silhouette's shape table.
//
//   rpg_test --forge-solve [--pool P] [--estimate] [--keep] [--out FILE]
//       solves kShapeTable (12 peoples x 36 schools x 3 ranks) on the measured 1x outlines (every helm family on the man's
//       head; every skirt x mass x layer on each people's T4 man kit; idle D, idle R and the walk's passing frame R; grey,
//       no held items; split at the neck) or, with --estimate or no pool drawing the kit, the design estimates in doll_forge.h;
//       writes the .inc (default rpg/sim/gear_forge_shapes.inc) and prints the rules' log. Rules 1-4 are hard. --keep (forge
//       fix i1) warm-starts every round from the compiled table, so a re-solve after an art change moves only the cells the
//       rules and the measured twins ask for (the alloys keep their helm families, skirts and layers where they can)
//   rpg_test --forge-shapes [--people a,b] [--seeds A..B] [--check] [--pool P]
//       every kit culture's alloys (seeds A..B, default 1..3): school, key, details and the smith's line; --check re-checks
//       rules 1-4 on the compiled table (C16d's data rules) and C16u over the board set and worlds 1..6
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "rpg/art.h"
#include "rpg/art/doll/doll.h"
#include "rpg/art/doll/doll_forge.h"
#include "rpg/art/doll/doll_internal.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/gear_forge.h"
#include "rpg/sim/gear_ladder.h"
#include "tools/tests/tests.h"

namespace dollshapes {

namespace F = art::doll::forge;
namespace doll = art::doll;

const char* const kPeople[12] = {"fjordfolk", "highland", "heartland", "imperial", "dune", "steppe", "marsh", "jade", "river", "suntemple",
                                 "sylvan", "starspire"};

std::string fmt(const char* f, ...) {
  char b[1024];
  va_list a;
  va_start(a, f);
  std::vsnprintf(b, sizeof b, f, a);
  va_end(a);
  return b;
}
bool range(const char* s, int& a, int& b) {
  if (std::sscanf(s, "%d..%d", &a, &b) == 2) return true;
  if (std::sscanf(s, "%d", &a) == 1) { b = a; return true; }
  return false;
}
bool loadPool(const char* path) {
  std::string err;
  if (!doll::loadPoolFile(path, &err)) { std::printf("FAIL: --pool %s\n", err.c_str()); return false; }
  return true;
}

std::vector<std::string> tableCheck();   // rules 1-4 on the compiled table, with the model it was solved on (below)
}  // namespace dollshapes

// the smith's line (PLAN "Delight"): "DRAGONSTEEL: horned kabuto, split skirt, drop shoulders, sash; wide sleeves, scale bib"
std::string smithLine(const cult::Culture& c, int idx) {
  namespace F = art::doll::forge;
  const gear::Shape s = gear::alloyShape(c, idx);
  const gear::ForgeStyle st = gear::alloyStyle(c, idx);
  const gear::AlloyDetails d = gear::alloyDetails(c, idx, s);
  std::string o = c.arms.alloys[(size_t)idx].name + ": ";
  std::string helm = F::shapeName(F::SX_HELM, s.helm);
  if (st.crest) helm = std::string(F::valueName(F::AX_CREST, st.crest)) + "-crested " + helm;
  o += helm + ", " + F::shapeName(F::SX_SKIRT, s.skirt) + " skirt, ";
  if (s.mass == F::MS_NONE) o += "no shoulders";
  else o += std::string(F::valueName(F::AX_SHOULDER, st.shoulder)) + " " + F::shapeName(F::SX_MASS, s.mass) + " shoulders";
  o += s.layer == F::LY_NONE ? std::string("") : std::string(", ") + F::shapeName(F::SX_LAYER, s.layer);
  o += std::string("; ") + F::shapeName(F::SX_ARM, s.arm) + " arms, " + F::shapeName(F::SX_MOTIF, s.motif) + " chest";
  if (st.collar) o += std::string(", ") + F::valueName(F::AX_COLLAR, st.collar) + " collar";
  if (!d.root.empty()) o += " (root " + d.root + ")";
  return o;
}

namespace dollshapes {

// ---------------------------------------------------------------- --forge-shapes
int shapesCmd(int argc, char** argv) {
  int s0 = 1, s1 = 3;
  bool check = false;
  std::set<int> peoples;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) range(argv[++i], s0, s1);
    else if (!std::strcmp(argv[i], "--check")) check = true;
    else if (!std::strcmp(argv[i], "--pool") && i + 1 < argc) { if (!loadPool(argv[++i])) return 1; }
    else if (!std::strcmp(argv[i], "--people") && i + 1 < argc) {
      std::string v = argv[++i];
      size_t a = 0;
      while (a <= v.size()) {
        size_t b = v.find(',', a);
        if (b == std::string::npos) b = v.size();
        cult::Archetype ar;
        if (gear::peopleOf(v.substr(a, b - a), ar)) peoples.insert((int)ar);
        a = b + 1;
      }
    }
  }
  std::printf("shape table: pool hash %016llX (%s)\n", (unsigned long long)gear::shapeTablePoolHash(),
              gear::shapeTablePoolHash() ? "measured" : "design estimates");
  for (int p = 0; p < 12; p++) {
    if (!peoples.empty() && !peoples.count(p)) continue;
    for (int sd = s0; sd <= s1; sd++) {
      const cult::Culture& c = gear::kitCulture((cult::Archetype)p, (uint32_t)sd);
      for (int i = 0; i < (int)c.arms.alloys.size(); i++) {
        const gear::Shape s = gear::alloyShape(c, i);
        const gear::ForgeStyle st = gear::alloyStyle(c, i);
        std::printf("%-9s s%-2d %d %-13s school %2d rank %d | %-38s | sh %-7s cr %-6s co %-7s | %s\n", kPeople[p], sd, i + 1,
                    c.arms.alloys[(size_t)i].name.c_str(), gear::forgeSchool6(c), gear::shapeRank(i), gear::describeShape(s).c_str(),
                    st.shoulder ? F::valueName(F::AX_SHOULDER, st.shoulder) : "-", st.crest ? F::valueName(F::AX_CREST, st.crest) : "-",
                    st.collar ? F::valueName(F::AX_COLLAR, st.collar) : "-", smithLine(c, i).c_str());
      }
    }
  }
  if (!check) return 0;
  const std::vector<std::string> bad = tableCheck();
  for (size_t i = 0; i < bad.size() && i < 40; i++) std::printf("  %s\n", bad[i].c_str());
  std::printf("forge-shapes --check: %zu failure(s) (rules 1-4)\n", bad.size());
  return bad.empty() ? 0 : 1;
}

}  // namespace dollshapes


// ======================================================================== (forge round 2) the gates C16 o m n d u, C17's visor, C18
namespace dollshapes {

struct Line {
  std::string id;
  int checks = 0, fails = 0;
  std::string first;
  bool report = false;
  void ok() { checks++; }
  void bad(const std::string& why) {
    checks++;
    fails++;
    if (first.empty()) first = why;
    if (report && (fails <= 12 || std::getenv("FORGE_ALL"))) std::printf("    %s: %s\n", id.c_str(), why.c_str());
  }
  std::string text() const {
    return fmt("%s %-24s %4d checks, %4d failed%s%s", fails ? "FAIL:" : "ok   ", id.c_str(), checks, fails, fails ? "; first: " : "", first.c_str());
  }
};

art::HumanLook a3Base(int who) {
  auto hexc = [](uint32_t rgb) { return 0xFF000000u | (rgb & 0xFF) << 16 | (rgb & 0xFF00) | (rgb >> 16 & 0xFF); };
  art::HumanLook L;
  if (who == 0) {
    L.body = 0; L.hair = art::Hair::Short; L.hairVar = 2; L.neck = 1; L.weapon = 1; L.outfit = art::Outfit::Tunic;
    L.skin = hexc(0xE9A077); L.hairColor = hexc(0xAA4A2C); L.topColor = hexc(0x28728C); L.bottomColor = hexc(0x4F4B6B); L.tabardColor = hexc(0xC83E38);
    L.eyeColor = hexc(0x3A78B0);
  } else {
    L.body = 1; L.hair = art::Hair::Braids; L.hairVar = 0; L.outfit = art::Outfit::Dress;
    L.skin = hexc(0xEEAA86); L.hairColor = hexc(0xC79346); L.topColor = hexc(0x922F60); L.tabardColor = hexc(0x28728C);
    L.eyeColor = hexc(0x3F8A5A);
  }
  return L;
}
// an alloy's outline on its people's kit (T4; the masterwork T5), the round-1 body when roundOne (the guise, no silhouette)
doll::Outline alloyOutline(const cult::Culture& c, int i, bool roundOne = false) {
  art::HumanLook L = gear::alloyKitLook(a3Base(0), c, i);
  if (roundOne) {
    L.forgeShape = 0;
    L.forgeGuise = gear::alloyGuise(c, i);
    const uint64_t pk = gear::packForge(gear::alloyPick(c, i).style);
    for (uint64_t& f : L.forge) if (f) f = pk;
  }
  return doll::outlineOf(doll::resolve(L));
}
// a material style of a people on the same kit (its own form; STYLE_P2 28.4's hero on it)
doll::Outline materialOutline(const cult::Culture& c, int base) {
  const art::HumanLook L0 = gear::alloyKitLook(a3Base(0), c, 0);
  art::HumanLook L = L0;
  static const char* const kMat[F::B_COUNT] = {"", "cloth", "leather", "bronze", "iron", "steel", "gilded", "obsidian", "ember", "starmetal", "mythic"};
  if (base == F::B_MYTHIC) L.mythic = 1;
  doll::applyLookSpec(L, std::string("mat=") + kMat[base]);
  if (base >= F::B_TANNER && base <= F::B_ARMOURER && c.archetype != cult::Archetype::Heartland) L.forgeKit = 0;   // the people's own forms
  return doll::outlineOf(doll::resolve(L));
}

// C16o's pair rule for two alloys of a culture: body OD 0.08 or more with no frame under 0.05, head OD 0.10 or more
std::string cultureRule(const doll::OutlineOD& d) {
  if (d.body < 0.08 || d.bodyMin < 0.05 || d.head < 0.10) return fmt("body OD %.3f (0.08), smallest frame %.3f (0.05), head OD %.3f (0.10)", d.body, d.bodyMin, d.head);
  return "";
}
std::string materialRule(const doll::OutlineOD& d) {
  if (d.body >= 0.06 || (d.head >= 0.15 && d.body >= 0.04)) return "";
  return fmt("body OD %.3f (0.06), head OD %.3f (0.15 with body 0.04)", d.body, d.head);
}

// C16n's 1 px features on a probe: gear pixels whose step is above or below all four same-slot neighbours (glints aside)
double noiseShare(const doll::FrameProbe& p, int* gearPx = nullptr) {
  int gear = 0, feat = 0;
  auto isGear = [&](int k) {
    const int o = p.owner[(size_t)k];
    if (o < 0 || p.code[(size_t)k] < 2) return false;
    const auto& it = p.items[(size_t)o];
    return it.has(doll::Role::Armour) || it.has(doll::Role::Helm) || it.has(doll::Role::Pauldron) || it.has(doll::Role::Overlay);
  };
  for (int y = 1; y + 1 < p.h; y++)
    for (int x = 1; x + 1 < p.w; x++) {
      const int k = y * p.w + x;
      if (!isGear(k)) continue;
      gear++;
      const uint8_t c = p.code[(size_t)k];
      const doll::Slot s = doll::codeSlot(c);
      if (!doll::greySlot(s)) continue;
      const int st = doll::codeStep(c);
      if (st == 4 || st == 0) continue;   // glints, the outline and seam lines
      int above = 0, below = 0, n = 0;
      // its same-slot gear neighbours (the outline and seam lines, step 0, are lines, not a plane's step: aside)
      for (int k2 : {k - 1, k + 1, k - p.w, k + p.w}) {
        const uint8_t c2 = p.code[(size_t)k2];
        if (c2 < 2 || doll::codeSlot(c2) != s || !isGear(k2) || doll::codeStep(c2) == 0) continue;
        n++;
        if (st > doll::codeStep(c2)) above++;
        if (st < doll::codeStep(c2)) below++;
      }
      if (n >= 3 && (above == n || below == n)) feat++;
    }
  if (gearPx) *gearPx = gear;
  return gear ? (double)feat / gear : 0.0;
}

// C16m's torso box: the pixels of the torso armour and the motif on the idle D frame (grey ramps), luma blurred 2 x 2
struct MotifRead { double range = 0, mean = 0; int px = 0, w = 0, h = 0, pivX = 0, pivY = 0; std::vector<double> map; };
MotifRead motifRead(const doll::DollRecipe& rIn) {
  doll::DollRecipe r = rIn;
  r.ramps = doll::greyRamps(rIn.ramps);
  const doll::FrameProbe p = doll::probeFrame(r, art::RigPart::Loco, 0, art::Clip::Idle, 0, 0, true);
  MotifRead m;
  if (!p.ok) return m;
  std::vector<double> lum((size_t)p.w * p.h, -1);
  for (int y = 0; y < p.h; y++)
    for (int x = 0; x < p.w; x++) {
      const size_t k = (size_t)y * p.w + x;
      const int o = p.owner[k];
      if (o < 0 || p.code[k] < 2 || p.items[(size_t)o].layer != doll::Layer::Torso) continue;
      if (!p.items[(size_t)o].has(doll::Role::Armour) && !p.items[(size_t)o].has(doll::Role::Overlay)) continue;
      const uint32_t c = r.ramps.c[(int)doll::codeSlot(p.code[k])][doll::codeStep(p.code[k])];
      lum[k] = (c & 255) * 0.299 + (c >> 8 & 255) * 0.587 + (c >> 16 & 255) * 0.114;
    }
  std::vector<double> v;
  for (int y = 0; y + 1 < p.h; y++)
    for (int x = 0; x + 1 < p.w; x++) {
      const size_t k = (size_t)y * p.w + x;
      const double a = lum[k], b = lum[k + 1], c = lum[k + p.w], d = lum[k + p.w + 1];
      if (a < 0 || b < 0 || c < 0 || d < 0) continue;
      v.push_back((a + b + c + d) / 4.0);
    }
  // the blurred map itself, on the frame's pivot (C16m's per-pixel difference)
  m.w = p.w; m.h = p.h; m.pivX = p.px; m.pivY = p.py;
  m.map.assign((size_t)p.w * p.h, -1.0);
  for (int y = 0; y + 1 < p.h; y++)
    for (int x = 0; x + 1 < p.w; x++) {
      const size_t k = (size_t)y * p.w + x;
      const double a = lum[k], b = lum[k + 1], c = lum[k + p.w], d = lum[k + p.w + 1];
      if (a >= 0 && b >= 0 && c >= 0 && d >= 0) m.map[k] = (a + b + c + d) / 4.0;
    }
  if (v.size() < 8) return m;
  std::sort(v.begin(), v.end());
  m.range = v[v.size() * 9 / 10] - v[v.size() / 10];
  double s = 0;
  for (double x : v) s += x;
  m.mean = s / v.size();
  m.px = (int)v.size();
  return m;
}

// C16m's per-pixel reading: the mean |a - b| over the torso pixels both blurred maps hold, aligned on the pivot (fewer than 8
// shared: the means' difference)
double motifMad(const MotifRead& a, const MotifRead& b) {
  double s = 0;
  int n = 0;
  for (int y = 0; y < a.h; y++)
    for (int x = 0; x < a.w; x++) {
      const double va = a.map[(size_t)y * a.w + x];
      if (va < 0) continue;
      const int bx = x - a.pivX + b.pivX, by = y - a.pivY + b.pivY;
      if (bx < 0 || by < 0 || bx >= b.w || by >= b.h) continue;
      const double vb = b.map[(size_t)by * b.w + bx];
      if (vb < 0) continue;
      s += std::fabs(va - vb);
      n++;
    }
  return std::max(n >= 8 ? s / n : 0.0, std::fabs(a.mean - b.mean));   // (either reading: the means apart, or the pixels)
}

// C17's visor term: a closed helm shows a band of step 0-1 pixels 2 px or more tall across the eye row (idle D)
int visorRows(const doll::FrameProbe& p) {
  int rows = 0, best = 0;
  for (int dy = 5; dy <= 12; dy++) {
    const int y = p.headY + dy;
    int dark = 0;
    for (int x = p.headX + 3; x <= p.headX + 12; x++) {
      const int k = p.at(x, y);
      if (k < 0 || p.code[(size_t)k] < 2) continue;
      const int o = p.owner[(size_t)k];
      if (o < 0 || !p.items[(size_t)o].has(doll::Role::Helm)) continue;
      if (doll::codeStep(p.code[(size_t)k]) <= 1 && doll::greySlot(doll::codeSlot(p.code[(size_t)k]))) dark++;
    }
    rows = dark >= 5 ? rows + 1 : 0;
    best = std::max(best, rows);
  }
  return best;
}
bool visorBand(const doll::DollRecipe& r, std::string* why) {
  const doll::FrameProbe p = doll::probeFrame(r, art::RigPart::Loco, 0, art::Clip::Idle, 0, 0, true);
  if (!p.ok) { if (why) *why = "no idle D"; return false; }
  const int best = visorRows(p);
  if (best < 2 && why) *why = fmt("the visor band %d rows tall (2)", best);
  return best >= 2;
}

}  // namespace dollshapes

// (forge round 2) the drawn gates in rpg_test --doll --gates (test_doll_forge.cpp's dollForgeGatesImpl calls it): C16o, C16m,
// C16n, the visor term and their selftests. Returns the failing lines; appends the printed lines
int dollShapeGates(bool report, std::vector<std::string>& printed, const std::vector<std::pair<std::string, art::HumanLook>>& forgeLooks) {
  using namespace dollshapes;
  Line o{"C16o outline"}, mo{"C16m motif"}, no{"C16n noise"}, vi{"C17 visor"}, st{"C16o n visor selftests"};
  for (Line* l : {&o, &mo, &no, &vi, &st}) l->report = report;
  // o: a culture's alloys pairwise (every people, seeds 1..8) and each against its people's materials
  const int s1 = std::getenv("FORGE_SEEDS") ? std::atoi(std::getenv("FORGE_SEEDS")) : 8;
  for (int pe = 0; pe < 12; pe++)
    for (int sd = 1; sd <= s1; sd++) {
      const cult::Culture& c = gear::kitCulture((cult::Archetype)pe, (uint32_t)sd);
      const int n = (int)c.arms.alloys.size();
      std::vector<doll::Outline> ol;
      for (int i = 0; i < n; i++) ol.push_back(alloyOutline(c, i));
      for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
          const std::string why = cultureRule(doll::outlineDistance(ol[(size_t)i], ol[(size_t)j]));
          if (!why.empty()) o.bad(fmt("%s s%d %s vs %s: %s", kPeople[pe], sd, c.arms.alloys[(size_t)i].name.c_str(), c.arms.alloys[(size_t)j].name.c_str(), why.c_str()));
          else o.ok();
        }
      if (sd > 3) continue;   // (the materials on the board set's kits)
      for (int b = F::B_HOMESPUN; b < F::B_COUNT; b++) {
        const doll::Outline mt = materialOutline(c, b);
        for (int i = 0; i < n; i++) {
          const std::string why = materialRule(doll::outlineDistance(ol[(size_t)i], mt));
          if (!why.empty()) o.bad(fmt("%s s%d %s vs %s: %s", kPeople[pe], sd, c.arms.alloys[(size_t)i].name.c_str(), F::valueName(F::AX_BASE, b), why.c_str()));
          else o.ok();
        }
      }
      // m: a motif other than PLAIN keeps a luma range of 40 or more in the torso box; two alloys whose motifs differ, 10 apart
      std::vector<MotifRead> mr;
      std::vector<int> motif;
      for (int i = 0; i < n; i++) {
        const doll::DollRecipe r = doll::resolve(gear::alloyKitLook(a3Base(0), c, i));
        mr.push_back(motifRead(r));
        motif.push_back(r.shape[F::SX_MOTIF]);
        if (r.shape[F::SX_MOTIF] && r.shape[F::SX_MOTIF] != F::MO_PLAIN) {
          if (mr.back().range < 40.0) mo.bad(fmt("%s s%d %s %s: luma range %.0f (40)", kPeople[pe], sd, c.arms.alloys[(size_t)i].name.c_str(), F::shapeName(F::SX_MOTIF, motif.back()), mr.back().range));
          else mo.ok();
        }
      }
      for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
          if (motif[(size_t)i] && motif[(size_t)j] && motif[(size_t)i] != motif[(size_t)j]) {
            // (silhouettes i2; PLAN C16 m "differ by 10 luma or more (mean) there") the mean of the per-pixel differences over
            // the torso box both figures share (aligned on the pivot); two equal means of two different motifs still part
            const double mad = motifMad(mr[(size_t)i], mr[(size_t)j]);
            if (mad < 10.0)
              mo.bad(fmt("%s s%d %s vs %s: torso box %.1f luma apart per pixel (10; means %.0f and %.0f)", kPeople[pe], sd, c.arms.alloys[(size_t)i].name.c_str(), c.arms.alloys[(size_t)j].name.c_str(), mad, mr[(size_t)i].mean, mr[(size_t)j].mean));
            else mo.ok();
          }
    }
  // the four fixed hero pairs (o): ARMOURER'S vs STARFALL body 0.06 and head 0.15; FURNACE vs WROUGHT and ARMOURER'S head 0.15
  {
    const cult::Culture& hc = gear::kitCulture(cult::Archetype::Heartland, 7);
    const doll::Outline arm = materialOutline(hc, F::B_ARMOURER), sta = materialOutline(hc, F::B_STARFALL), fur = materialOutline(hc, F::B_FURNACE),
                        wro = materialOutline(hc, F::B_WROUGHT);
    const doll::OutlineOD as = doll::outlineDistance(arm, sta), fw = doll::outlineDistance(fur, wro), fa = doll::outlineDistance(fur, arm);
    if (as.body < 0.06 || as.head < 0.15) o.bad(fmt("ARMOURER'S vs STARFALL: body %.3f (0.06), head %.3f (0.15)", as.body, as.head)); else o.ok();
    if (fw.head < 0.15) o.bad(fmt("FURNACE vs WROUGHT: head %.3f (0.15)", fw.head)); else o.ok();
    if (fa.head < 0.15) o.bad(fmt("FURNACE vs ARMOURER'S: head %.3f (0.15)", fa.head)); else o.ok();
  }
  // n: every forge look's idle frames: 1 px features 3 % or fewer of the gear pixels; visor: every closed helm
  for (const auto& fl : forgeLooks) {
    const doll::DollRecipe r = doll::resolve(fl.second);
    for (int fc : {0, 2}) {
      doll::DollRecipe g = r;
      g.ramps = doll::greyRamps(r.ramps);
      const doll::FrameProbe p = doll::probeFrame(g, art::RigPart::Loco, fc, art::Clip::Idle, 0, 0, true);
      if (!p.ok) continue;
      int gp = 0;
      const double sh = noiseShare(p, &gp);
      if (gp >= 40 && sh > 0.03) no.bad(fmt("%s %c: 1 px features %.1f %% of %d gear px (3)", fl.first.c_str(), "DURL"[fc], sh * 100, gp)); else no.ok();
    }
    if (r.shape[F::SX_HELM] && F::helmClosed(r.shape[F::SX_HELM])) {
      std::string why;
      if (!visorBand(r, &why)) vi.bad(fl.first + ": " + why); else vi.ok();
    }
  }
  // every closed family on the board set's alloys
  for (const gear::BoardCulture& bc : gear::forgeBoardSet()) {
    const cult::Culture& c = gear::kitCulture((cult::Archetype)bc.people, bc.seed);
    for (int i = 0; i < (int)c.arms.alloys.size(); i++) {
      const doll::DollRecipe r = doll::resolve(gear::alloyKitLook(a3Base(0), c, i));
      if (!r.shape[F::SX_HELM] || !F::helmClosed(r.shape[F::SX_HELM])) continue;
      std::string why;
      if (!visorBand(r, &why)) vi.bad(fmt("%s s%u %s (%s): %s", kPeople[bc.people], bc.seed, c.arms.alloys[(size_t)i].name.c_str(), F::shapeName(F::SX_HELM, r.shape[F::SX_HELM]), why.c_str()));
      else vi.ok();
    }
  }
  // selftests: the round-1 Highland alloys FAIL C16o; a checker tile on a plate torso FAILs C16n; a closed helm without its band
  // FAILs the visor term
  int caught = 0, tries = 0;
  {
    tries++;
    const cult::Culture& c = gear::kitCulture(cult::Archetype::Highland, 1);
    bool fail = false;
    for (int i = 0; i < (int)c.arms.alloys.size(); i++)
      for (int j = i + 1; j < (int)c.arms.alloys.size(); j++)
        if (!cultureRule(doll::outlineDistance(alloyOutline(c, i, true), alloyOutline(c, j, true))).empty()) fail = true;
    if (fail) caught++;
    else printed.push_back("  selftest MISSED: the round-1 Highland alloys pass C16o");
  }
  {
    tries++;
    art::HumanLook L = a3Base(0);
    doll::applyLookSpec(L, "hero=1,kit=heartland:4:7,mat=steel,palette=look");
    doll::DollRecipe r = doll::resolve(L);
    r.ramps = doll::greyRamps(r.ramps);
    doll::FrameProbe p = doll::probeFrame(r, art::RigPart::Loco, 0, art::Clip::Idle, 0, 0, true);
    for (int y = 0; y < p.h; y++)
      for (int x = 0; x < p.w; x++) {
        const size_t k = (size_t)y * p.w + x;
        const int ow = p.owner[k];
        if (ow < 0 || p.code[k] < 2 || !p.items[(size_t)ow].has(doll::Role::Armour)) continue;   // a checker over the whole armour
        if (doll::codeStep(p.code[k]) == 0) continue;
        p.code[k] = doll::code(doll::codeSlot(p.code[k]), ((x + y) & 1) ? 3 : 1);
      }
    int cgp = 0;
    const double csh = noiseShare(p, &cgp);
    if (report) std::printf("    C16n selftest: the checker %.1f %% of %d gear px (probe %s)\n", csh * 100, cgp, p.ok ? "ok" : "none");
    if (csh > 0.03) caught++;
    else printed.push_back("  selftest MISSED: a checker tile passes C16n");
  }
  {
    tries++;
    // a sage closed helm without its band: a great helm's dark pixels in the eye rows lifted to step 2
    art::HumanLook L = a3Base(0);
    doll::applyLookSpec(L, "kit=jade:4:2,alloy=3,palette=look,shape=helm:great");
    doll::DollRecipe r = doll::resolve(L);
    std::string why;
    const bool had = visorBand(r, &why);
    doll::FrameProbe p = doll::probeFrame(r, art::RigPart::Loco, 0, art::Clip::Idle, 0, 0, true);
    for (int y = p.headY + 4; y <= p.headY + 13; y++)   // the band removed: every helm pixel of the eye rows at step 2
      for (int x = p.headX; x <= p.headX + 15; x++) {
        const int k = p.at(x, y);
        if (k < 0 || p.code[(size_t)k] < 2) continue;
        const int ow = p.owner[(size_t)k];
        if (ow >= 0 && p.items[(size_t)ow].has(doll::Role::Helm) && doll::codeStep(p.code[(size_t)k]) <= 1)
          p.code[(size_t)k] = doll::code(doll::codeSlot(p.code[(size_t)k]), 2);
      }
    const int best = visorRows(p);
    if (best < 2) caught++;
    else printed.push_back("  selftest MISSED: a closed helm without its band passes the visor term");
    if (report) std::printf("    visor selftest: the drawn great helm %s its band (%s)\n", had ? "shows" : "lacks", why.c_str());
  }
  if (caught == tries) st.ok(); else st.bad(fmt("%d of %d caught", caught, tries));
  printed.push_back(fmt("  C16o C16n visor selftest: %s", caught == tries ? "ALL CAUGHT" : "MISSED"));
  int fails = 0;
  for (Line* l : {&o, &mo, &no, &vi, &st}) { printed.push_back(l->text()); fails += l->fails ? 1 : 0; }
  return fails;
}


// ---------------------------------------------------------------- (forge round 2) the data gates C16d, C16u and C18
namespace dollshapes {
// C16d's culture rule: the helm families differ and two or more of skirt, mass and over-layer differ
bool cultureKeysApart(const gear::Shape& x, const gear::Shape& y) {
  return x.helm != y.helm && (x.skirt != y.skirt) + (x.mass != y.mass) + (x.layer != y.layer) >= 2;
}
struct AlloyRef { const cult::Culture* c; int idx; int ci = 0, cj = 0; std::string scope; };
std::string refName(const AlloyRef& a) {
  return fmt("%s %s", cult::archetypeName(a.c->archetype), a.c->arms.alloys[(size_t)a.idx].name.c_str());
}
// the board set's alloys
std::vector<AlloyRef> boardAlloys() {
  std::vector<AlloyRef> v;
  for (const gear::BoardCulture& bc : gear::forgeBoardSet()) {
    const cult::Culture& c = gear::kitCulture((cult::Archetype)bc.people, bc.seed);
    for (int i = 0; i < (int)c.arms.alloys.size(); i++) v.push_back(AlloyRef{&c, i, bc.people, (int)bc.seed, "board"});
  }
  return v;
}
// a world's window of families (cells x0..x0+5, y0..y0+5)
std::vector<AlloyRef> windowAlloys(cult::Atlas& at, int x0, int y0) {
  std::vector<AlloyRef> v;
  for (int cj = y0; cj < y0 + 6; cj++)
    for (int ci = x0; ci < x0 + 6; ci++) {
      const cult::Culture& c = at.family(ci, cj);
      for (int i = 0; i < (int)c.arms.alloys.size(); i++) v.push_back(AlloyRef{&c, i, ci, cj, "window"});
    }
  return v;
}
// C16u over a scope: no two alloys share a key
void uniqueKeys(const std::vector<AlloyRef>& as, const std::string& scope, Line& u) {
  std::map<uint32_t, size_t> seen;
  for (size_t i = 0; i < as.size(); i++) {
    const uint32_t k = gear::shapeKey(gear::alloyShape(*as[i].c, as[i].idx));
    if (!k) { u.bad(scope + ": " + refName(as[i]) + " has no key"); continue; }
    auto it = seen.find(k);
    if (it != seen.end()) u.bad(fmt("%s: %s and %s share %s", scope.c_str(), refName(as[it->second]).c_str(), refName(as[i]).c_str(), gear::describeShape(gear::shapeOfKey(k)).c_str()));
    else { seen[k] = i; u.ok(); }
  }
}
}  // namespace dollshapes


// ---------------------------------------------------------------- the measured model (rpg_test --forge-solve on a pool)
namespace dollshapes {
// the outlines the solver measures: every skirt x mass x layer on each people's T4 man kit (its seed-1 kit culture, the
// warriors' alloy's body, the people's first helm family, close arms, no motif) and every helm family on the Heartland
// kit's man; a value the pool does not draw for the man yet keeps the design estimate for every pair it is in
bool servedFor(doll::Role r, const std::string& v) {
  const auto& P = art::doll::pool::get();
  for (int i : P.partsWithRole(r, v)) if (P.parts[(size_t)i].bodyMask >> (int)doll::BodySet::Man & 1) return true;
  return false;
}
struct Measured {
  std::map<uint32_t, doll::Outline> body;   // people << 24 | skirt << 16 | mass << 8 | layer
  std::map<int, doll::Outline> head;
  std::map<uint32_t, doll::Outline> mats;   // people << 8 | base
  int composes = 0;
  bool drawnValue(int axis, int v) {
    const char* w = F::shapeName(axis, v);
    switch (axis) {
      case F::SX_HELM: return v >= 1 && servedFor(doll::Role::Helm, std::string("fam.") + w);
      case F::SX_SKIRT: return v == F::SK_NONE || servedFor(doll::Role::Overlay, std::string("skirt.") + w);
      case F::SX_MASS: return true;   // the masses are the existing pauldron shapes (and none)
      case F::SX_LAYER:
        return v == F::LY_NONE || v == F::LY_CAPE || v == F::LY_SCARF ||
               servedFor(v == F::LY_HALFCAPE ? doll::Role::Cloak : doll::Role::Overlay, w);
      default: return true;
    }
  }
  const doll::Outline& bodyOf(int p, const gear::Shape& s) {
    const uint32_t k = (uint32_t)p << 24 | (uint32_t)s.skirt << 16 | (uint32_t)s.mass << 8 | s.layer;
    auto it = body.find(k);
    if (it != body.end()) return it->second;
    art::HumanLook L = gear::alloyKitLook(a3Base(0), gear::kitCulture((cult::Archetype)p, 1), 1, 4);
    uint8_t first[8];
    F::shapeMenu(p, F::SX_HELM, first, nullptr, nullptr, 8);
    doll::applyLookSpec(L, fmt("shape=helm:%s+skirt:%s+mass:%s+layer:%s+arm:close+motif:unset", F::shapeName(F::SX_HELM, first[0]), F::shapeName(F::SX_SKIRT, s.skirt),
                               F::shapeName(F::SX_MASS, s.mass), F::shapeName(F::SX_LAYER, s.layer)));
    // the mass in its first shape (the details move with the table: the measured outline must not)
    uint8_t ms[5];
    const int nm = F::massShapes(s.mass, ms);
    doll::applyLookSpec(L, nm ? fmt("style=shoulder:%s", F::valueName(F::AX_SHOULDER, ms[0])) : std::string("style=shoulder:none"));
    composes++;
    return body[k] = doll::outlineOf(doll::resolve(L));
  }
  const doll::Outline& headOf(int h) {
    auto it = head.find(h);
    if (it != head.end()) return it->second;
    art::HumanLook L = gear::alloyKitLook(a3Base(0), gear::kitCulture(cult::Archetype::Heartland, 1), 1, 4);
    doll::applyLookSpec(L, fmt("shape=helm:%s", F::shapeName(F::SX_HELM, h)));
    composes++;
    return head[h] = doll::outlineOf(doll::resolve(L));
  }
  const doll::Outline& matOf(int p, int base) {
    const uint32_t k = (uint32_t)p << 8 | (uint32_t)base;
    auto it = mats.find(k);
    if (it != mats.end()) return it->second;
    composes++;
    return mats[k] = materialOutline(gear::kitCulture((cult::Archetype)p, 1), base);
  }
  bool drawnBody(const gear::Shape& s) { return drawnValue(F::SX_SKIRT, s.skirt) && drawnValue(F::SX_LAYER, s.layer); }
  // a board-set cell's real figure (its kit culture's alloy of that rank) wearing a key: C18a's own figure
  std::map<std::pair<uint64_t, uint32_t>, doll::Outline> board;
  const doll::Outline* boardOf(int p, int s, int r, const gear::Shape& k) {
    const cult::Culture* c = nullptr;
    for (const gear::BoardCulture& bc : gear::forgeBoardSet())
      if (bc.people == p && gear::forgeSchool6(gear::kitCulture((cult::Archetype)p, bc.seed)) == s) c = &gear::kitCulture((cult::Archetype)p, bc.seed);
    if (!c || r >= (int)c->arms.alloys.size()) return nullptr;
    const auto key = std::make_pair((uint64_t)p << 16 | (uint64_t)s << 8 | (uint64_t)r, gear::shapeKey(k));
    auto it = board.find(key);
    if (it != board.end()) return &it->second;
    art::HumanLook L = gear::alloyKitLook(a3Base(0), *c, r);
    doll::applyLookSpec(L, fmt("shape=helm:%s+skirt:%s+mass:%s+layer:%s", F::shapeName(F::SX_HELM, k.helm), F::shapeName(F::SX_SKIRT, k.skirt),
                               F::shapeName(F::SX_MASS, k.mass), F::shapeName(F::SX_LAYER, k.layer)));
    composes++;
    return &(board[key] = doll::outlineOf(doll::resolve(L)));
  }
};
gear::ShapeModel measuredModel(Measured& M, int* undrawn) {
  gear::ShapeModel est = gear::estimateModel(), m;
  m.measured = true;
  std::map<uint64_t, double> cache;
  auto bodyKey = [](int p, const gear::Shape& s) { return (uint64_t)p << 24 | (uint64_t)s.skirt << 16 | (uint64_t)s.mass << 8 | s.layer; };
  m.body = [&M, est, bodyKey, cache](int p, const gear::Shape& a, const gear::Shape& b) mutable {
    if (!M.drawnBody(a) || !M.drawnBody(b)) return est.body(p, a, b);
    const uint64_t k = bodyKey(p, a) << 32 ^ bodyKey(p, b);
    auto it = cache.find(k);
    if (it != cache.end()) return it->second;
    return cache[k] = doll::outlineDistance(M.bodyOf(p, a), M.bodyOf(p, b)).body;
  };
  m.bodyX = [&M, est, bodyKey](int pa, const gear::Shape& a, int pb, const gear::Shape& b) {
    static std::map<std::pair<uint64_t, uint64_t>, double> cx;
    if (!M.drawnBody(a) || !M.drawnBody(b)) return est.bodyX(pa, a, pb, b);
    const auto k = std::make_pair(bodyKey(pa, a), bodyKey(pb, b));
    auto it = cx.find(k);
    if (it != cx.end()) return it->second;
    return cx[k] = doll::outlineDistance(M.bodyOf(pa, a), M.bodyOf(pb, b)).body;
  };
  m.head = [&M, est](int a, int b) {
    static std::map<std::pair<int, int>, double> ch;
    if (!M.drawnValue(F::SX_HELM, a) || !M.drawnValue(F::SX_HELM, b)) return est.head(a, b);
    const auto k = std::make_pair(std::min(a, b), std::max(a, b));
    auto it = ch.find(k);
    if (it != ch.end()) return it->second;
    return ch[k] = doll::outlineDistance(M.headOf(a), M.headOf(b)).head;
  };
  m.material = [&M, est](int p, const gear::Shape& a, int base) {
    static std::map<std::pair<uint64_t, int>, double> cm;
    if (!M.drawnBody(a)) return est.material(p, a, base);
    const auto k = std::make_pair((uint64_t)p << 24 | (uint64_t)a.skirt << 16 | (uint64_t)a.mass << 8 | a.layer, base);
    auto it = cm.find(k);
    if (it != cm.end()) return it->second;
    return cm[k] = doll::outlineDistance(M.bodyOf(p, a), M.matOf(p, base)).body;
  };
  m.boardBody = [&M, est](int p, int s, int r, const gear::Shape& a, int q, int t, int rr, const gear::Shape& b) {
    if (!M.drawnBody(a) || !M.drawnBody(b)) return est.bodyX(p, a, q, b);
    const doll::Outline* x = M.boardOf(p, s, r, a);
    const doll::Outline* y = M.boardOf(q, t, rr, b);
    if (!x || !y) return 1.0;
    return doll::outlineDistance(*x, *y).body;
  };
  if (undrawn) {
    *undrawn = 0;
    for (int ax : {(int)F::SX_HELM, (int)F::SX_SKIRT, (int)F::SX_LAYER})
      for (int v = 1; v <= (ax == F::SX_HELM ? F::H_ALLOY_LAST : F::shapeCount(ax)); v++) if (!M.drawnValue(ax, v)) (*undrawn)++;
  }
  return m;
}
}  // namespace dollshapes

namespace dollshapes {
// rules 1-4 on the compiled table: measured on the pool when the table was solved on one, else the design estimates
std::vector<std::string> tableCheck() {
  if (!gear::shapeTablePoolHash()) return gear::checkShapeTable(gear::shapeTable(), gear::estimateModel());
  const int was = art::rigVersion();
  art::setRigVersion(2);
  Measured M;
  const gear::ShapeModel m = measuredModel(M, nullptr);
  std::vector<std::string> out = gear::checkShapeTable(gear::shapeTable(), m);
  art::setRigVersion(was);
  return out;
}
}  // namespace dollshapes

// rpg_test --doll-forge's round-2 data lines (test_doll_forge.cpp forgeCmd): C16d over every people at seeds s0..s1 and the
// table's rules 1-4; C16u over the board set and worlds w0..w1 (windows at cells 0..5 and 3..8)
int forgeShapeData(int w0, int w1, int s0, int s1, bool report, bool selftest, std::vector<std::string>& printed) {
  using namespace dollshapes;
  Line d{"C16d silhouette data"}, u{"C16u unique keys"}, st{"C16d u selftests"};
  for (Line* l : {&d, &u, &st}) l->report = report;
  for (const std::string& b : tableCheck()) d.bad("table " + b);
  std::vector<std::vector<std::pair<int, gear::Shape>>> mats(12);
  for (int p = 0; p < 12; p++) mats[(size_t)p] = gear::materialKeys(p);
  for (int p = 0; p < 12; p++)
    for (int sd = s0; sd <= s1; sd++) {
      const cult::Culture& c = gear::kitCulture((cult::Archetype)p, (uint32_t)sd);
      const int n = (int)c.arms.alloys.size();
      std::vector<gear::Shape> sh;
      for (int i = 0; i < n; i++) sh.push_back(gear::alloyShape(c, i));
      for (int i = 0; i < n; i++) {
        for (const auto& m : mats[(size_t)p])
          if (gear::keyAxesApart(sh[(size_t)i], m.second) < 2) {
            d.bad(fmt("%s s%d %s: %s within one axis of %s (%s)", kPeople[p], sd, c.arms.alloys[(size_t)i].name.c_str(), gear::describeShape(sh[(size_t)i]).c_str(),
                      F::valueName(F::AX_BASE, m.first), gear::describeShape(m.second).c_str()));
            goto nextAlloy;
          }
        d.ok();
      nextAlloy:;
        for (int j = i + 1; j < n; j++) {
          const gear::Shape &x = sh[(size_t)i], &y = sh[(size_t)j];
          const int body = (x.skirt != y.skirt) + (x.mass != y.mass) + (x.layer != y.layer);
          if (!cultureKeysApart(x, y))
            d.bad(fmt("%s s%d %s vs %s: helm %s, %d body axes apart", kPeople[p], sd, c.arms.alloys[(size_t)i].name.c_str(), c.arms.alloys[(size_t)j].name.c_str(),
                      x.helm == y.helm ? "shared" : "apart", body));
          else d.ok();
        }
      }
    }
  uniqueKeys(boardAlloys(), "board set", u);
  for (int w = w0; w <= w1; w++) {
    cult::Atlas at((uint64_t)w);
    for (int o : {0, 3}) uniqueKeys(windowAlloys(at, o, o), fmt("world %d window %d", w, o), u);
  }
  if (selftest) {
    int caught = 0;
    Line t{"t"};
    std::vector<AlloyRef> dup = boardAlloys();
    dup.push_back(dup[0]);
    uniqueKeys(dup, "dup", t);
    if (t.fails) caught++;
    // a culture whose three alloys share a key fails C16d's culture rule
    const gear::Shape s = gear::alloyShape(gear::kitCulture(cult::Archetype::Highland, 1), 0);
    if (!cultureKeysApart(s, s)) caught++;
    if (caught == 2) st.ok(); else st.bad(fmt("%d of 2 caught", caught));
    printed.push_back(fmt("  C16d C16u selftest: %s", caught == 2 ? "ALL CAUGHT" : "MISSED"));
  }
  int fails = 0;
  for (Line* l : {&d, &u}) { printed.push_back(l->text()); fails += l->fails ? 1 : 0; }
  if (selftest) { printed.push_back(st.text()); fails += st.fails ? 1 : 0; }
  return fails;
}

// rpg_test --doll-forge --silhouettes: C18 over the board set and worlds w0..w1 (the windows at cells 0..5 and 3..8)
int forgeSilhouettes(int w0, int w1, bool report, bool selftest, std::vector<std::string>& printed) {
  using namespace dollshapes;
  Line a{"C18a no twins"}, b{"C18b culture outline"}, c{"C18c spread"}, st{"C18 selftest"};
  for (Line* l : {&a, &b, &c, &st}) l->report = report;
  std::string out;
  auto twin = [](const doll::OutlineOD& d) { return d.body < 0.05 && d.head < 0.10; };
  struct Scope { std::string name; std::vector<AlloyRef> as; bool window; };
  std::vector<Scope> scopes;
  scopes.push_back(Scope{"board set", boardAlloys(), false});
  std::vector<std::unique_ptr<cult::Atlas>> atlases;
  for (int w = w0; w <= w1; w++) {
    atlases.push_back(std::make_unique<cult::Atlas>((uint64_t)w));
    for (int o : {0, 3}) scopes.push_back(Scope{fmt("world %d window %d", w, o), windowAlloys(*atlases.back(), o, o), true});
  }
  for (Scope& sc : scopes) {
    std::vector<doll::Outline> ol;
    for (const AlloyRef& r : sc.as) ol.push_back(alloyOutline(*r.c, r.idx));
    struct P { size_t i, j; doll::OutlineOD d; double s; };
    std::vector<P> ps;
    int histoB[11] = {}, histoH[11] = {};
    int windowTwins = 0;
    for (size_t i = 0; i < sc.as.size(); i++)
      for (size_t j = i + 1; j < sc.as.size(); j++) {
        const doll::OutlineOD d = doll::outlineDistance(ol[i], ol[j]);
        ps.push_back(P{i, j, d, d.body / 0.05 + d.head / 0.10});
        histoB[std::min(10, (int)(d.body * 50))]++;
        histoH[std::min(10, (int)(d.head * 25))]++;
        const AlloyRef &x = sc.as[i], &y = sc.as[j];
        const bool same = x.c == y.c;
        const int dist = std::max(std::abs(x.ci - y.ci), std::abs(x.cj - y.cj));
        if (same) {
          if (d.body < 0.08) b.bad(fmt("%s: %s vs %s: body OD %.3f (0.08)", sc.name.c_str(), refName(x).c_str(), refName(y).c_str(), d.body)); else b.ok();
        }
        if (!twin(d)) { a.ok(); continue; }
        const std::string what = fmt("%s: %s vs %s: body %.3f head %.3f", sc.name.c_str(), refName(x).c_str(), refName(y).c_str(), d.body, d.head);
        if (!sc.window || same || dist < 3) a.bad(what);
        else if (++windowTwins > 2) a.bad(what + " (a third pair in the world)");
        else { a.ok(); out += "  allowed: " + what + "\n"; }
      }
    // c: no helm family on more than 20 %, no body outline on more than 4 % of the scope's alloys
    std::map<int, int> helms, bodies;
    for (const AlloyRef& r : sc.as) {
      const gear::Shape s = gear::alloyShape(*r.c, r.idx);
      helms[s.helm]++;
      bodies[s.skirt | s.mass << 3 | s.layer << 6]++;
    }
    const double n = (double)sc.as.size();
    for (const auto& kv : helms)
      if (kv.second > 0.20 * n) c.bad(fmt("%s: helm %s on %d of %.0f", sc.name.c_str(), F::shapeName(F::SX_HELM, kv.first), kv.second, n)); else c.ok();
    for (const auto& kv : bodies)
      if (kv.second > std::max(1.0, 0.04 * n)) {
        gear::Shape s; s.skirt = (uint8_t)(kv.first & 7); s.mass = (uint8_t)(kv.first >> 3 & 7); s.layer = (uint8_t)(kv.first >> 6 & 7);
        c.bad(fmt("%s: body %s on %d of %.0f", sc.name.c_str(), gear::describeShape(s).c_str(), kv.second, n));
      } else c.ok();
    std::sort(ps.begin(), ps.end(), [](const P& x, const P& y) { return x.s < y.s; });
    out += fmt("%s: %zu alloys; body OD histogram (0.02 bins)", sc.name.c_str(), sc.as.size());
    for (int k : histoB) out += fmt(" %d", k);
    out += "; head OD histogram (0.04 bins)";
    for (int k : histoH) out += fmt(" %d", k);
    out += "\n";
    for (size_t k = 0; k < ps.size() && k < 10; k++)
      out += fmt("  nearest %zu: %s vs %s: body %.3f (min %.3f) head %.3f\n", k + 1, refName(sc.as[ps[k].i]).c_str(), refName(sc.as[ps[k].j]).c_str(), ps[k].d.body,
                 ps[k].d.bodyMin, ps[k].d.head);
    if (!sc.window)
      for (const AlloyRef& r : sc.as) out += "  key " + refName(r) + ": " + gear::describeShape(gear::alloyShape(*r.c, r.idx)) + " | " + smithLine(*r.c, r.idx) + "\n";
  }
  if (selftest) {
    // the round-1 alloys (the guise forced, no silhouette) FAIL a and b
    const cult::Culture& hc = gear::kitCulture(cult::Archetype::Highland, 1);
    bool fa = false, fb = false;
    for (int i = 0; i < (int)hc.arms.alloys.size(); i++)
      for (int j = i + 1; j < (int)hc.arms.alloys.size(); j++) {
        const doll::OutlineOD d = doll::outlineDistance(alloyOutline(hc, i, true), alloyOutline(hc, j, true));
        if (d.body < 0.08) fb = true;
        if (twin(d) || d.body < 0.05) fa = true;
      }
    if (fa && fb) st.ok(); else st.bad(fmt("round-1 alloys: a %s, b %s", fa ? "caught" : "missed", fb ? "caught" : "missed"));
    printed.push_back(fmt("  C18 selftest: %s", fa && fb ? "ALL CAUGHT" : "MISSED"));
  }
  if (const char* path = std::getenv("FORGE_SIL_OUT")) {
    if (FILE* f = std::fopen(path, "wb")) { std::fputs(out.c_str(), f); std::fclose(f); }
  } else if (report) std::fputs(out.c_str(), stdout);
  int fails = 0;
  for (Line* l : {&a, &b, &c}) { printed.push_back(l->text()); fails += l->fails ? 1 : 0; }
  if (selftest) { printed.push_back(st.text()); fails += st.fails ? 1 : 0; }
  return fails;
}

namespace dollshapes {
// (INTEGRATE silhouettes i1) C18a on the real figures of the table tableKey reads now (the board set, worlds 1..6's windows):
// the failing pairs' keys, as forgeSilhouettes judges them
std::vector<std::pair<uint32_t, uint32_t>> c18aFailKeys(int w0, int w1) {
  auto twin = [](const doll::OutlineOD& d) { return d.body < 0.05 && d.head < 0.10; };
  // (INTEGRATE silhouettes i2) C18b and C16o too: a culture's own alloys (the board set, and every people's seeds 1..8 as
  // C16o reads them) fed back when they break cultureRule (body 0.08, smallest frame 0.05, head 0.10), not only as twins
  struct Scope { std::vector<AlloyRef> as; bool window; bool culture; };
  std::vector<Scope> scopes;
  scopes.push_back(Scope{boardAlloys(), false, true});
  for (int pe = 0; pe < 12; pe++)
    for (int sd = 1; sd <= 8; sd++) {
      const cult::Culture& c = gear::kitCulture((cult::Archetype)pe, (uint32_t)sd);
      Scope sc{{}, false, true};
      for (int i = 0; i < (int)c.arms.alloys.size(); i++) sc.as.push_back(AlloyRef{&c, i, pe, sd, "seed"});
      scopes.push_back(sc);
    }
  std::vector<std::pair<uint32_t, uint32_t>> out;
  // C16m as dollShapeGates reads it (seeds 1..8): a covered motif (luma range under 40) pairs its key with its people's
  // other alloys, two different motifs under 10 luma apart pair theirs, so the solve moves their cells
  for (size_t si = 1; si < scopes.size(); si++) {
    const Scope& sc = scopes[si];
    const size_t n = sc.as.size();
    std::vector<MotifRead> mr;
    std::vector<int> motif;
    std::vector<uint32_t> keys;
    for (const AlloyRef& r : sc.as) {
      const doll::DollRecipe rr = doll::resolve(gear::alloyKitLook(a3Base(0), *r.c, r.idx));
      mr.push_back(motifRead(rr));
      motif.push_back(rr.shape[F::SX_MOTIF]);
      keys.push_back(gear::shapeKey(gear::alloyShape(*r.c, r.idx)));
    }
    for (size_t i = 0; i < n; i++) {
      if (!motif[i] || motif[i] == F::MO_PLAIN || mr[i].range >= 40.0) continue;
      for (size_t j = 0; j < n; j++) if (j != i) out.push_back({std::min(keys[i], keys[j]), std::max(keys[i], keys[j])});
    }
    for (size_t i = 0; i < n; i++)
      for (size_t j = i + 1; j < n; j++)
        if (motif[i] && motif[j] && motif[i] != motif[j] && motifMad(mr[i], mr[j]) < 10.0) out.push_back({std::min(keys[i], keys[j]), std::max(keys[i], keys[j])});
  }
  std::vector<std::unique_ptr<cult::Atlas>> atlases;
  for (int w = w0; w <= w1; w++) {
    atlases.push_back(std::make_unique<cult::Atlas>((uint64_t)w));
    for (int o : {0, 3}) scopes.push_back(Scope{windowAlloys(*atlases.back(), o, o), true, true});   // (C18b holds a window's same-culture pairs too)
  }
  for (Scope& sc : scopes) {
    std::vector<doll::Outline> ol;
    std::vector<uint32_t> keys;
    for (const AlloyRef& r : sc.as) { ol.push_back(alloyOutline(*r.c, r.idx)); keys.push_back(gear::shapeKey(gear::alloyShape(*r.c, r.idx))); }
    int windowTwins = 0;
    for (size_t i = 0; i < sc.as.size(); i++)
      for (size_t j = i + 1; j < sc.as.size(); j++) {
        const doll::OutlineOD d = doll::outlineDistance(ol[i], ol[j]);
        const AlloyRef &x = sc.as[i], &y = sc.as[j];
        const bool same = x.c == y.c;
        if (sc.culture && same && !twin(d) && !cultureRule(d).empty()) { out.push_back({std::min(keys[i], keys[j]), std::max(keys[i], keys[j])}); continue; }
        if (!twin(d)) continue;
        const int dist = std::max(std::abs(x.ci - y.ci), std::abs(x.cj - y.cj));
        if (!sc.window || same || dist < 3 || ++windowTwins > 2) out.push_back({std::min(keys[i], keys[j]), std::max(keys[i], keys[j])});
      }
  }
  return out;
}
// ---------------------------------------------------------------- --forge-solve
int solveCmd(int argc, char** argv) {
  std::string out = "rpg/sim/gear_forge_shapes.inc";
  bool estimate = false, keep = false;
  std::string poolPath;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
    else if (!std::strcmp(argv[i], "--estimate")) estimate = true;
    else if (!std::strcmp(argv[i], "--keep")) keep = true;
    else if (!std::strcmp(argv[i], "--pool") && i + 1 < argc) { poolPath = argv[++i]; if (!loadPool(poolPath.c_str())) return 1; }
  }
  uint64_t hash = 0;
  {
    const doll::PoolStats ps = doll::poolStats();
    if (!ps.hash.empty()) hash = std::strtoull(ps.hash.c_str(), nullptr, 16);
    if (!poolPath.empty()) {   // a loaded pool: FNV-1a over the file
      hash = 1469598103934665603ull;
      if (FILE* f = std::fopen(poolPath.c_str(), "rb")) {
        int ch;
        while ((ch = std::fgetc(f)) != EOF) { hash ^= (uint64_t)ch; hash *= 1099511628211ull; }
        std::fclose(f);
      }
    }
  }
  const int was = art::rigVersion();
  art::setRigVersion(2);
  Measured M;
  int undrawn = 0;
  gear::ShapeModel m = estimate ? gear::estimateModel() : measuredModel(M, &undrawn);
  std::string how = estimate ? "design estimates (art::doll::forge estBodyOD / estHeadOD)"
                             : fmt("measured 1x outlines on pool %016llX (%d helm, skirt and layer values not drawn yet keep the estimates)", (unsigned long long)hash, undrawn);
  if (estimate) hash = 0;
  static uint32_t keepTab[12][36][3];
  if (keep) { std::memcpy(keepTab, gear::shapeTable(), sizeof keepTab); m.start = keepTab; }
  gear::ShapeSolve S = gear::solveShapes(m);
  // (INTEGRATE silhouettes i1) C18a holds the board set and the worlds' windows hard on the REAL figures (each alloy's own
  // details: its shoulder shape, crest, collar; its people's head), which the model only estimates: measure the solved
  // table's figures, feed the pairs that came out twins back as known twins, and solve again (at most 8 rounds)
  if (!estimate) {
    static std::set<std::pair<uint32_t, uint32_t>> knownPairs;
    m.knownTwin = [](int, const gear::Shape& a, int, const gear::Shape& b) {
      const uint32_t x = gear::shapeKey(a), y = gear::shapeKey(b);
      return knownPairs.count({std::min(x, y), std::max(x, y)}) > 0;
    };
    static uint32_t cand[12][36][3];
    gear::ShapeSolve best = S;
    size_t bestFails = SIZE_MAX;
    for (int round = 1; round <= 24; round++) {
      std::memcpy(cand, S.table, sizeof cand);
      gear::setShapeTableOverride(cand);
      const std::vector<std::pair<uint32_t, uint32_t>> fails = c18aFailKeys(1, 6);
      gear::setShapeTableOverride(nullptr);
      if (fails.size() < bestFails) { bestFails = fails.size(); best = S; }
      size_t added = 0;
      for (const auto& f : fails) added += knownPairs.insert(f).second ? 1 : 0;
      std::printf("  C18a round %d: %zu twin pair(s) on the real figures, %zu new\n", round, fails.size(), added);
      if (fails.empty() || !added || round == 24) break;
      // (silhouettes i2) from round 4 on, start from the best table so far: the passes move the twins' cells, not the world
      static uint32_t startTab[12][36][3];
      std::memcpy(startTab, best.table, sizeof startTab);
      m.start = round >= 3 ? startTab : keep ? keepTab : nullptr;
      S = gear::solveShapes(m);
    }
    S = best;   // the round with the fewest real twins
    std::printf("  C18a: kept the table with %zu twin pair(s)\n", bestFails);
  }
  art::setRigVersion(was);
  std::printf("  %d outlines composed\n", M.composes);
  for (const std::string& l : S.log) std::printf("  %s\n", l.c_str());
  const std::string text = gear::shapeTableInc(S, hash, how);
  std::ofstream f(out, std::ios::binary);
  if (!f) { std::printf("FAIL: cannot write %s\n", out.c_str()); return 1; }
  f << text;
  std::printf("forge-solve: wrote %s (%s); %d hard failure(s)\n", out.c_str(), how.c_str(), S.hardFails);
  return S.hardFails ? 1 : 0;
}

}  // namespace dollshapes

RPG_TEST_CMD("--forge-shapes", "P2c forge round 2: the shape table on the kit cultures [--people a,b] [--seeds A..B] [--check] [--pool f]",
             dollshapes::shapesCmd);
RPG_TEST_CMD("--forge-solve", "P2c forge round 2: solve the shape table [--pool f] [--estimate] [--out FILE]", dollshapes::solveCmd);
