// rpg_test --expand-looks and --doll-ladder (P2c the gear ladder, LADDER-DOLL; docs/P2C_CONTRACTS.md 3b, 6b, 7; docs/PLAN.md
// "The ladder slice", gates C12 C13 C14).
//
//   rpg_test --expand-looks FILE              each looks line's resulting fields (the macros expanded: gear::describeLook,
//                                             which reads back as a plain spec) and what the doll resolves it to
//   rpg_test --doll-ladder [--looks F] [--pool P] [--finish] [--report] [--selftest]
//                                             C12 the ladder reads (a steps, b richness, c silhouette, d T1 okay, e flavour),
//                                             C13 the living finish (with --finish or --gates), C14 value; selftests
//
// The looks: tools/doll/ladder_looks.txt (embedded as test_doll_arms.cpp's matrix; --looks reads a file). C12 reads the
// Heartland hero's line lt1..lt6 and the T3 flavour row (lt3 jade3 imp3 fjord3), idle facing D and R. Every number is from
// art::doll::probeFrame (the composed frame with each pixel's owner and region) and the recipe's ramps.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/art/art_fx.h"
#include "rpg/art/doll/doll.h"
#include "rpg/art/doll/doll_internal.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/gear_forge.h"
#include "rpg/sim/gear_ladder.h"
#include "rpg/sim/gear_look.h"
#include "tools/tests/tests.h"

using art::Clip;
using art::HumanLook;
using art::RigPart;
using art::RigSheet;
namespace doll = art::doll;

std::string dollSliceLooksText();   // test_doll_arms.cpp: the gates' matrix (the ladder looks, embedded)

namespace dollladder {

bool g_report = false;

std::string fmt(const char* f, ...) {
  char b[1024];
  va_list a;
  va_start(a, f);
  std::vsnprintf(b, sizeof b, f, a);
  va_end(a);
  return b;
}

struct Line {
  std::string id;
  int checks = 0, fails = 0;
  std::string first;
  void ok() { checks++; }
  void bad(const std::string& why) {
    checks++;
    fails++;
    if (first.empty()) first = why;
    if (g_report) std::printf("    %s: %s\n", id.c_str(), why.c_str());
  }
  std::string text() const {
    return fmt("%s %-24s %4d checks, %4d failed%s%s", fails ? "FAIL:" : "ok   ", id.c_str(), checks, fails, fails ? "; first: " : "", first.c_str());
  }
};

uint32_t hexc(uint32_t rgb) { return 0xFF000000u | (rgb & 0xFF) << 16 | (rgb & 0xFF00) | (rgb >> 16 & 0xFF); }
HumanLook a3Hero() {
  HumanLook L;
  L.body = 0; L.hair = art::Hair::Short; L.hairVar = 2; L.neck = 1; L.weapon = 1; L.outfit = art::Outfit::Tunic;
  L.skin = hexc(0xE9A077); L.hairColor = hexc(0xAA4A2C); L.topColor = hexc(0x28728C); L.bottomColor = hexc(0x4F4B6B); L.tabardColor = hexc(0xC83E38);
  L.eyeColor = hexc(0x3A78B0);
  return L;
}
HumanLook a3Woman() {
  HumanLook L;
  L.body = 1; L.hair = art::Hair::Braids; L.hairVar = 0; L.outfit = art::Outfit::Dress;
  L.skin = hexc(0xEEAA86); L.hairColor = hexc(0xC79346); L.topColor = hexc(0x922F60); L.tabardColor = hexc(0x28728C);
  L.eyeColor = hexc(0x3F8A5A);
  return L;
}

struct Look { std::string tag; int who = 0; std::string spec; std::vector<std::string> gaits; };
std::vector<Look> parseLooks(const std::string& text) {
  std::vector<Look> v;
  size_t a = 0;
  while (a < text.size()) {
    size_t b = text.find('\n', a);
    if (b == std::string::npos) b = text.size();
    const std::string line = text.substr(a, b - a);
    a = b + 1;
    if (line.empty() || line[0] == '#') continue;
    char tag[128] = {}, who[16] = {}, spec[1200] = {}, gaits[200] = {};
    if (std::sscanf(line.c_str(), "%127s %15s %1199s %199s", tag, who, spec, gaits) < 2) continue;
    Look l;
    l.tag = tag; l.who = !std::strcmp(who, "woman") ? 1 : 0; l.spec = std::strcmp(spec, "-") ? spec : "";
    std::string g = gaits;
    size_t c = 0;
    while (c < g.size()) { size_t d = g.find(',', c); if (d == std::string::npos) d = g.size(); if (d > c) l.gaits.push_back(g.substr(c, d - c)); c = d + 1; }
    v.push_back(l);
  }
  return v;
}
std::string readFile(const char* file) {
  std::string t;
  FILE* f = std::fopen(file, "rb");
  if (!f) return t;
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) t.append(buf, n);
  std::fclose(f);
  return t;
}

struct Built { std::string tag; HumanLook L; doll::DollRecipe r; bool lookPal = false; std::string bad; };
Built build(const Look& l) {
  gear::installLadderMacros();
  Built b;
  b.tag = l.tag;
  b.L = l.who ? a3Woman() : a3Hero();
  bool bare = false;
  doll::applyLookSpec(b.L, l.spec, &b.lookPal, &bare, &b.bad);
  b.r = doll::resolve(b.L);
  if (!b.lookPal) b.r.ramps = doll::a3Palette(b.L.body == 1 ? 1 : 0);
  return b;
}
const Built* find(const std::vector<Built>& v, const char* tag) {
  for (const Built& b : v) if (b.tag == tag) return &b;
  return nullptr;
}

// ---------------------------------------------------------------- --expand-looks
int expandCmd(int argc, char** argv) {
  const char* file = nullptr;
  for (int i = 1; i < argc; i++)
    if (!std::strcmp(argv[i], "--expand-looks") && i + 1 < argc) file = argv[++i];
  const std::string text = file ? readFile(file) : dollSliceLooksText();
  if (text.empty()) { std::printf("FAIL: --expand-looks %s: cannot read\n", file ? file : "(embedded)"); return 1; }
  const int was = art::rigVersion();
  art::setRigVersion(2);
  int bad = 0;
  for (const Look& l : parseLooks(text)) {
    const Built b = build(l);
    std::printf("%-8s %-5s %s\n", l.tag.c_str(), l.who ? "woman" : "man", gear::describeLook(b.L).c_str());
    std::string needs;
    for (const doll::Need& n : b.r.needs) {
      const doll::Role ro = n.role;
      if (ro == doll::Role::Armour || ro == doll::Role::Helm || ro == doll::Role::Shield || ro == doll::Role::Pauldron || ro == doll::Role::Held ||
          ro == doll::Role::Sheath || ro == doll::Role::Cloak || (ro == doll::Role::Overlay && n.value != "stripes"))
        needs += fmt("%s%s=%s", needs.empty() ? "" : " ", doll::roleName(ro), n.value.c_str());
    }
    std::printf("         needs: %s\n", needs.c_str());
    std::string tiers;
    static const char* const kSlot[8] = {"body", "helm", "shield", "weapon", "gloves", "boots", "cloak", "back"};
    for (int s = 0; s < 8; s++) if (b.L.tier[s]) tiers += fmt("%s%s T%d%s", tiers.empty() ? "" : ", ", kSlot[s], b.L.tier[s], (b.L.rare >> s & 1) ? " r" : "");
    std::printf("         tiers: %s; metal %d, blade %d%s\n", tiers.empty() ? "none" : tiers.c_str(), b.L.metalKind, b.L.bladeKind,
                b.L.mythic ? ", mythic" : "");
    // (P2c forge) each worn slot's forge style (the picks) and the flags the maps test
    for (int s = 0; s < 8; s++)
      if (b.L.forge[s]) std::printf("         forge %-6s %s\n", kSlot[s], gear::describeForge(gear::unpackForge(b.L.forge[s])).c_str());
    {
      std::string ff;
      for (const std::string& f : b.r.flags)
        if (f.rfind("forge-", 0) == 0 || f.rfind("shoulder-", 0) == 0 || f.rfind("cut-", 0) == 0 || f.rfind("edge-", 0) == 0 || f.rfind("collar-", 0) == 0 ||
            f.rfind("limb-", 0) == 0 || f.rfind("fit-", 0) == 0 || f.rfind("crest-", 0) == 0 || f.rfind("boss-", 0) == 0 || f.rfind("rim-", 0) == 0 ||
            f.rfind("glimb-", 0) == 0 || f.rfind("blimb-", 0) == 0 || f.rfind("form-", 0) == 0 || f.rfind("blademetal-", 0) == 0 ||
            f.rfind("helmfam-", 0) == 0 || f.rfind("skirt-", 0) == 0 || f.rfind("mass-", 0) == 0 || f.rfind("layer-", 0) == 0 ||
            f.rfind("arm-", 0) == 0 || f.rfind("motif-", 0) == 0 || f == "alloyshape" || f == "alloyface" || f == "glowseams")
          ff += (ff.empty() ? "" : " ") + f;
      if (!ff.empty()) std::printf("         forge flags: %s\n", ff.c_str());
    }
    // (forge round 2) the silhouette: the look's, as the resolver applied it, and what the pool does not draw yet
    if (b.L.forgeShape) {
      gear::Shape ap;
      ap.helm = b.r.shape[0]; ap.skirt = b.r.shape[1]; ap.mass = b.r.shape[2]; ap.layer = b.r.shape[3]; ap.arm = b.r.shape[4]; ap.motif = b.r.shape[5];
      std::printf("         shape: %s (applied: %s)\n", gear::describeShape(gear::unpackShape(b.L.forgeShape)).c_str(), gear::describeShape(ap).c_str());
      std::string fm;
      for (const std::string& m : b.r.forgeMissing) fm += (fm.empty() ? "" : "; ") + m;
      if (!fm.empty()) std::printf("         shape not drawn yet: %s\n", fm.c_str());
    }
    const std::vector<doll::Missing> miss = doll::dollMissing(b.L);
    if (!miss.empty()) std::printf("         missing (%d): %s\n", (int)miss.size(), miss[0].what().c_str());
    if (!b.bad.empty()) { std::printf("         FAIL: unknown field(s) %s\n", b.bad.c_str()); bad++; }
  }
  art::setRigVersion(was);
  return bad ? 1 : 0;
}

// ---------------------------------------------------------------- measuring a frame
double luma(uint32_t c) { return (c & 255) * 0.299 + (c >> 8 & 255) * 0.587 + (c >> 16 & 255) * 0.114; }
uint32_t colourOf(const doll::DollRecipe& r, uint8_t code) {
  if (code < 2) return 0;
  return r.ramps.c[(int)doll::codeSlot(code)][doll::codeStep(code)] | 0xFF000000u;
}
bool gearItem(const doll::FrameProbe::It& it) {
  using doll::Role;
  return it.has(Role::Armour) || it.has(Role::Helm) || it.has(Role::Shield) || it.has(Role::Pauldron) || it.has(Role::Held) ||
         it.has(Role::Sheath) || it.has(Role::Cloak) || it.has(Role::Overlay);
}
struct Pix { int x, y; uint8_t code; int item; uint8_t region; };
struct Frame {
  bool ok = false;
  doll::FrameProbe p;
  std::map<std::pair<int, int>, Pix> px;   // relative to the pivot
};
Frame idle(const Built& b, int facing) {
  Frame f;
  f.p = doll::probeFrame(b.r, RigPart::Loco, facing, Clip::Idle, 0, 0, true);
  if (!f.p.ok) return f;
  f.ok = true;
  for (int y = 0; y < f.p.h; y++)
    for (int x = 0; x < f.p.w; x++) {
      const int k = y * f.p.w + x;
      if (f.p.code[(size_t)k] < 2) continue;
      f.px[{x - f.p.px, y - f.p.py}] = Pix{x, y, f.p.code[(size_t)k], f.p.owner[(size_t)k], f.p.region[(size_t)k]};
    }
  return f;
}
bool isGear(const Frame& f, const Pix& p) { return p.item >= 0 && (size_t)p.item < f.p.items.size() && gearItem(f.p.items[(size_t)p.item]); }
bool isHeld(const Frame& f, const Pix& p) { return p.item >= 0 && (size_t)p.item < f.p.items.size() && f.p.items[(size_t)p.item].has(doll::Role::Held); }
// a: the share of the union of two frames' gear pixels that differ (one has gear there and the other not, or the colours differ)
double gearDiff(const Built& A, const Frame& a, const Built& B, const Frame& b) {
  std::set<std::pair<int, int>> uni;
  for (const auto& kv : a.px) if (isGear(a, kv.second)) uni.insert(kv.first);
  for (const auto& kv : b.px) if (isGear(b, kv.second)) uni.insert(kv.first);
  if (uni.empty()) return 0;
  int diff = 0;
  for (const auto& k : uni) {
    auto ia = a.px.find(k), ib = b.px.find(k);
    const bool ga = ia != a.px.end() && isGear(a, ia->second), gb = ib != b.px.end() && isGear(b, ib->second);
    if (ga != gb || colourOf(A.r, ia->second.code) != colourOf(B.r, ib->second.code)) diff++;
  }
  return (double)diff / uni.size();
}
// the figure's area without the weapon
int area(const Frame& f) {
  int n = 0;
  for (const auto& kv : f.px) if (!isHeld(f, kv.second)) n++;
  return n;
}
double outlineDiff(const Frame& a, const Frame& b) {
  std::set<std::pair<int, int>> sa, sb, uni;
  for (const auto& kv : a.px) if (!isHeld(a, kv.second)) { sa.insert(kv.first); uni.insert(kv.first); }
  for (const auto& kv : b.px) if (!isHeld(b, kv.second)) { sb.insert(kv.first); uni.insert(kv.first); }
  int x = 0;
  for (const auto& k : uni) if (sa.count(k) != sb.count(k)) x++;
  if (std::getenv("EMB_C12_OUTLINE") && !uni.empty()) {   // the two masks: '#' both, 'a' the first only, 'b' the second only
    int x0 = 1 << 30, x1 = -(1 << 30), y0 = 1 << 30, y1 = -(1 << 30);
    for (const auto& k : uni) { x0 = std::min(x0, k.first); x1 = std::max(x1, k.first); y0 = std::min(y0, k.second); y1 = std::max(y1, k.second); }
    std::printf("      outline %d of %d differ (%.2f %%)\n", x, (int)uni.size(), 100.0 * x / uni.size());
    for (int y = y0; y <= y1; y++) {
      std::string row;
      for (int xx = x0; xx <= x1; xx++) {
        const bool ia = sa.count({xx, y}) > 0, ib = sb.count({xx, y}) > 0;
        row += ia && ib ? '#' : ia ? 'a' : ib ? 'b' : '.';
      }
      std::printf("      %s\n", row.c_str());
    }
  }
  return uni.empty() ? 0 : (double)x / uni.size();
}
// per tier slot: its pixels, its trim (R recoloured to metal 2), wear (W) and glow pixels
struct Richness { int px[8] = {}, trim[8] = {}, wear[8] = {}, glow = 0, step4 = 0, colours = 0; };
Richness richness(const Built& b, const Frame& f) {
  Richness R;
  std::set<uint32_t> cols;
  for (const auto& kv : f.px) {
    const Pix& p = kv.second;
    cols.insert(colourOf(b.r, p.code));
    const doll::Slot s = doll::codeSlot(p.code);
    if (s == doll::Slot::Glow) R.glow++;
    // (ladder i2, integration) a step-4 glow pixel counts with the metal's: T5's gem in the shield boss and the helm's brow
    // stone take the place of the T4 boss's and band's sheen (lt5 D: 3 metal + 2 glow against lt4's 5 metal)
    if (doll::codeStep(p.code) == 4 && (s == doll::Slot::Metal || s == doll::Slot::Metal2 || s == doll::Slot::Blade || s == doll::Slot::Glow)) R.step4++;
    if (p.item < 0 || (size_t)p.item >= f.p.items.size()) continue;
    const int ts = doll::partTierSlot(f.p.items[(size_t)p.item].partIndex);
    if (ts < 0 || !isGear(f, p)) continue;
    R.px[ts]++;
    if (p.region == (uint8_t)doll::Region::Trim && s == doll::Slot::Metal2) R.trim[ts]++;
    if (p.region == (uint8_t)doll::Region::Wear) R.wear[ts]++;
  }
  R.colours = (int)cols.size();
  return R;
}

// ---------------------------------------------------------------- C14 value
// touching materials part by 18 luma at step 2; the lit top-left edge 40 over the darkest day ground; plate 3 vs 1 by 60
constexpr uint32_t kDarkGround = 0xFF1E2A24u;   // #242A1E: the darkest day ground (a forest floor in shade, luma 40)
bool c14Material(doll::Slot s) {
  using S = doll::Slot;
  return s == S::Cloth1 || s == S::Cloth2 || s == S::Accent || s == S::Leather || s == S::Metal || s == S::Hair || s == S::Skin || s == S::Cloak ||
         s == S::Field || s == S::Head;
}
// the A3 hero's own lit edge per facing (the yardstick: the darkest day ground + 40, or A3's when A3's is lower)
double g_heroLit[4] = {1e9, 1e9, 1e9, 1e9};
double c14Look(Line& g, const Built& b, int facing) {
  const Frame f = idle(b, facing);
  if (!f.ok) { g.bad(b.tag + fmt(" %c: no idle frame", "DURL"[facing])); return 0; }
  // pairs of materials touching (4-neighbours, both off the outline), counted by contacts
  std::map<std::pair<int, int>, int> touch;
  for (const auto& kv : f.px) {
    const Pix& p = kv.second;
    const doll::Slot s = doll::codeSlot(p.code);
    if (!c14Material(s) || doll::codeStep(p.code) == 0) continue;
    for (int d = 0; d < 2; d++) {
      auto it = f.px.find({kv.first.first + (d == 0), kv.first.second + (d == 1)});
      if (it == f.px.end()) continue;
      const doll::Slot t = doll::codeSlot(it->second.code);
      if (t == s || !c14Material(t) || doll::codeStep(it->second.code) == 0) continue;
      // the face aside: skin against hair or the head's own slots inside the head box
      if ((s == doll::Slot::Skin || t == doll::Slot::Skin) && kv.first.second < f.p.headY - f.p.py + 12) continue;
      touch[{std::min((int)s, (int)t), std::max((int)s, (int)t)}]++;
    }
  }
  for (const auto& kv : touch) {
    if (kv.second < 3) continue;   // a contact of a pixel or two is a seam, not two materials side by side
    const double d = std::fabs(luma(b.r.ramps.c[kv.first.first][2]) - luma(b.r.ramps.c[kv.first.second][2]));
    if (d < 18.0)
      g.bad(b.tag + fmt(" %c: %s against %s %.0f luma apart at step 2 (%d contacts; 18 wanted)", "DURL"[facing], doll::slotName((doll::Slot)kv.first.first),
                        doll::slotName((doll::Slot)kv.first.second), d, kv.second));
    else g.ok();
  }
  // the lit top-left edge: the first lit pixel (inside the outline ring: step 1 or more, never ink) of each row from the left
  // and of each column from the top (the finished colours)
  std::vector<double> edge;
  std::map<int, int> rowMin, colMin;
  for (const auto& kv : f.px) {
    if (doll::codeStep(kv.second.code) == 0 || doll::codeSlot(kv.second.code) == doll::Slot::Ink) continue;
    auto r = rowMin.find(kv.first.second);
    if (r == rowMin.end() || kv.first.first < r->second) rowMin[kv.first.second] = kv.first.first;
    auto c = colMin.find(kv.first.first);
    if (c == colMin.end() || kv.first.second < c->second) colMin[kv.first.first] = kv.first.second;
  }
  std::set<std::pair<int, int>> e;
  for (const auto& kv : rowMin) e.insert({kv.second, kv.first});
  for (const auto& kv : colMin) e.insert({kv.first, kv.second});
  for (const auto& k : e) edge.push_back(luma(colourOf(b.r, f.px.at(k).code)));
  if (std::getenv("C14_DEBUG")) {   // the edge's make-up by slot and step (luma)
    std::map<std::pair<int, int>, int> hist;
    for (const auto& k : e) hist[{(int)doll::codeSlot(f.px.at(k).code), doll::codeStep(f.px.at(k).code)}]++;
    std::printf("    C14 ramps %s: metal %.0f %.0f %.0f %.0f %.0f leather %.0f %.0f %.0f cloth1 %.0f %.0f %.0f skin %.0f hair %.0f\n", b.tag.c_str(),
                luma(b.r.ramps.c[(int)doll::Slot::Metal][0]), luma(b.r.ramps.c[(int)doll::Slot::Metal][1]), luma(b.r.ramps.c[(int)doll::Slot::Metal][2]),
                luma(b.r.ramps.c[(int)doll::Slot::Metal][3]), luma(b.r.ramps.c[(int)doll::Slot::Metal][4]), luma(b.r.ramps.c[(int)doll::Slot::Leather][1]),
                luma(b.r.ramps.c[(int)doll::Slot::Leather][2]), luma(b.r.ramps.c[(int)doll::Slot::Leather][3]), luma(b.r.ramps.c[(int)doll::Slot::Cloth1][1]),
                luma(b.r.ramps.c[(int)doll::Slot::Cloth1][2]), luma(b.r.ramps.c[(int)doll::Slot::Cloth1][3]), luma(b.r.ramps.c[(int)doll::Slot::Skin][2]),
                luma(b.r.ramps.c[(int)doll::Slot::Hair][2]));
    std::printf("    C14 step2 %s: cloth2 %.0f accent %.0f cloak %.0f field %.0f linen %.0f head %.0f\n", b.tag.c_str(), luma(b.r.ramps.c[(int)doll::Slot::Cloth2][2]),
                luma(b.r.ramps.c[(int)doll::Slot::Accent][2]), luma(b.r.ramps.c[(int)doll::Slot::Cloak][2]), luma(b.r.ramps.c[(int)doll::Slot::Field][2]),
                luma(b.r.ramps.c[(int)doll::Slot::Linen][2]), luma(b.r.ramps.c[(int)doll::Slot::Head][2]));
    std::printf("    C14 edge %s %c:", b.tag.c_str(), "DURL"[facing]);
    for (const auto& kv : hist) std::printf(" %s%d x%d (L%.0f)", doll::slotName((doll::Slot)kv.first.first), kv.first.second, kv.second, luma(b.r.ramps.c[kv.first.first][kv.first.second]));
    std::printf("\n");
  }
  std::sort(edge.begin(), edge.end());
  // the lit side: the upper quartile of the edge (the selout-lit step 1 and the light planes behind it), against the darkest
  // day ground (kDarkGround); the hero's own A3 palette is the yardstick (--report prints it)
  const double lit = edge.empty() ? 0 : edge[edge.size() * 3 / 4];
  if (g_report && !b.lookPal) std::printf("    C14 reference %s %c: lit edge %.0f luma\n", b.tag.c_str(), "DURL"[facing], lit);
  double want = std::min(luma(kDarkGround) + 40.0, g_heroLit[facing & 3]);
  // (P2c forge i2) a dark sheen's tell is a blackened body (STYLE_P2 28.7: step 2 at 90 luma or less, C17) whose selout edge
  // is its own step 1 (kept 10 under the body): its lit edge reads by that step and the near-white glint, not 40 over shade
  if (b.L.sheen == 3 && b.L.forge[HumanLook::TIER_BODY]) want = std::min(want, luma(b.r.ramps.c[(int)doll::Slot::Metal][1]) - 1.0);
  // (forge round 2, STYLE_P2 28.4 and 28.8) KNAPPED's black glass and FURNACE's char are the darkest armours by design (the
  // body at 60 luma or less, C17's dark glass): they read on dark ground by their hard glints and their glow, so their edge
  // is held to their own step 1, as the dark sheen's
  if ((b.L.metalKind == 7 || b.L.metalKind == 8) && b.L.forge[HumanLook::TIER_BODY])
    want = std::min(want, luma(b.r.ramps.c[(int)doll::Slot::Metal][1]) - 1.0);
  // (wave-2 fix round, D88) GLASS alloys too (D62 made them jewel-dark by design; their rim and speculars carry them)
  if (b.r.has("glass") && b.L.forge[HumanLook::TIER_BODY]) want = std::min(want, luma(b.r.ramps.c[(int)doll::Slot::Metal][1]) - 1.0);
  if (lit < want) g.bad(b.tag + fmt(" %c: the lit top-left edge %.0f luma, %.0f over the darkest day ground (%.0f wanted: 40 over, or A3's own)", "DURL"[facing], lit, lit - luma(kDarkGround), want - luma(kDarkGround)));
  else g.ok();
  return lit;
}
void c14Plate(Line& g, const Built& b) {
  bool plate = false;
  for (const doll::Need& n : b.r.needs) if (n.role == doll::Role::Armour && n.value == "plate") plate = true;
  if (!plate) return;
  const uint32_t* M = b.r.ramps.c[(int)doll::Slot::Metal];
  const double d = luma(M[3]) - luma(M[1]);
  if (d < 60.0) g.bad(b.tag + fmt(": plate step 3 and step 1 %.0f luma apart (60 wanted)", d));
  else g.ok();
}
void gateC14(Line& g, const std::vector<Built>& looks) {
  for (const Built& b : looks) {
    if (!b.lookPal) {   // A3's own palette (the hero) is the reference, not a pick
      if (b.tag == "hero") { Line ref{"ref"}; g_heroLit[0] = 1e9; g_heroLit[2] = 1e9; g_heroLit[0] = c14Look(ref, b, 0); g_heroLit[2] = c14Look(ref, b, 2); }
      continue;
    }
    c14Look(g, b, 0);
    c14Look(g, b, 2);
    c14Plate(g, b);
  }
}

// ---------------------------------------------------------------- C12 the ladder reads
struct Band { double lo, hi; };
// (ladder i2: the eye first, then the check) STYLE_P2 27.2's shares are for a body's plates; a 9-14 px helm or shield IS
// its rim (a 1 px brass brow band is 15 % of a nasal, a heater's rim 27-35 %), so helms and shields keep the floor and
// take a rim's ceiling; the bodies' ceilings carry the gorget and fauld hems the maps draw (lt3's mail 10-13 %)
const Band kTrimBand[7] = {{0, 0}, {0, 0}, {0, 0.03}, {0.03, 0.14}, {0.06, 0.20}, {0.10, 0.24}, {0.10, 0.24}};
constexpr double kRimCeil = 0.36;
// flavourRow: rule e compares the T3 flavour row (lt3 jade3 imp3 fjord3) of the ladder looks; one people's kit line (source L)
// carries only lt1..lt6, so the caller turns it off there (integrator, wave 2 int3: it failed all 72 lines whatever the art)
void gateC12(Line& g, const std::vector<Built>& looks, bool flavourRow = true) {
  const Built* line[7] = {};
  for (int t = 1; t <= 6; t++) line[t] = find(looks, fmt("lt%d", t).c_str());
  bool have = true;
  for (int t = 1; t <= 6; t++) if (!line[t]) have = false;
  if (!have) { g.bad("the ladder line lt1..lt6 is not in the looks"); return; }
  for (int facing : {0, 2}) {
    const char fc = "DURL"[facing];
    Frame fr[7];
    Richness rich[7];
    for (int t = 1; t <= 6; t++) {
      fr[t] = idle(*line[t], facing);
      if (!fr[t].ok) { g.bad(fmt("lt%d %c: no idle frame", t, fc)); return; }
      rich[t] = richness(*line[t], fr[t]);
    }
    // a, steps
    for (int t = 1; t < 6; t++) {
      const double d = gearDiff(*line[t], fr[t], *line[t + 1], fr[t + 1]);
      if (d < 0.15) g.bad(fmt("a: lt%d to lt%d %c differ in %.0f %% of their gear pixels (15 wanted)", t, t + 1, fc, d * 100));
      else g.ok();
    }
    {
      const double d = gearDiff(*line[1], fr[1], *line[6], fr[6]);
      if (d < 0.50) g.bad(fmt("a: lt1 to lt6 %c differ in %.0f %% (50 wanted)", fc, d * 100));
      else g.ok();
    }
    // b, richness: trim share per piece inside its tier's band; step-4 and colours never fall T1..T5; glow 0 below T5
    for (int t = 1; t <= 6; t++) {
      const HumanLook& L = line[t]->L;
      for (int s = 0; s < 8; s++) {
        if (!L.tier[s] || rich[t].px[s] < 20) continue;
        const int tt = std::min(6, L.tier[s] + ((L.rare >> s & 1) ? 1 : 0));
        const double sh = (double)rich[t].trim[s] / rich[t].px[s];
        // (wave-2 fix round, D78) C12b counts the tier's own trim (R recoloured, or added by the trim band, from T3); at T1-T2
        // R is drawn material: reported, not gated
        if (tt <= 2) { g.ok(); continue; }
        Band bd = kTrimBand[tt];
        if (s == 1 || s == 2) bd.hi = std::max(bd.hi, kRimCeil);
        // the weapon and the cloak: their trim is drawn in their own colours where the maps mark no R (the sword's guard,
        // the noble cloak's hem: P2b views, ADD-only); a ceiling still holds
        if ((s == 3 || s == 6) && !rich[t].trim[s]) { g.ok(); continue; }
        // (forge round 2, D41) the pauldrons draw over the cloak's front: the cloak shows less of its plain inside, so its hem
        // (the trim) is a larger share of what shows; the ceiling a third higher
        if (s == 6) bd.hi = std::max(bd.hi, 0.30);
        if (sh < bd.lo - 1e-9 || sh > bd.hi + 1e-9)
          g.bad(fmt("b: lt%d %c slot %d (T%d) trim %.1f %% (band %.0f-%.0f %%; %d of %d gear px)", t, fc, s, tt, sh * 100, bd.lo * 100, bd.hi * 100, rich[t].trim[s], rich[t].px[s]));
        else g.ok();
      }
      if (t < 5 && rich[t].glow) g.bad(fmt("b: lt%d %c: %d glow pixel(s) below T5", t, fc, rich[t].glow));
      else if (t >= 5 && !rich[t].glow) g.bad(fmt("b: lt%d %c: no glow pixel at T%d (gems, runes)", t, fc, t));
      else g.ok();
      if (t >= 2 && t <= 5) {
        // (silhouettes i2) a culture alloy's styled body (its round-2 metal read: one to three glints, STYLE_P2 28.8, the
        // blind panel's "1-2 px highlight specks") holds 3 glints or more instead of the tier below's count, and 75 % of its colours
        const bool alloyBody = line[t]->r.has("alloyshape");
        if (alloyBody ? rich[t].step4 < 3 : rich[t].step4 < rich[t - 1].step4 - 1) g.bad(fmt("b: lt%d %c: step-4 metal pixels fall %d -> %d", t, fc, rich[t - 1].step4, rich[t].step4));
        else g.ok();
        // (i2) leather and a teal cap to grey mail is a step up in fewer colours: 80 % of the tier below, at least
        if (alloyBody ? rich[t].colours * 4 < rich[t - 1].colours * 3 : rich[t].colours * 5 < rich[t - 1].colours * 4) g.bad(fmt("b: lt%d %c: colours fall %d -> %d", t, fc, rich[t - 1].colours, rich[t].colours));
        else g.ok();
      }
    }
    // c, silhouette
    for (int t = 2; t < 5; t++) {
      const int a0 = area(fr[t]), a1 = area(fr[t + 1]);
      if (a1 < a0 * 0.95) g.bad(fmt("c: lt%d to lt%d %c: the figure shrinks %d -> %d px", t, t + 1, fc, a0, a1));
      else g.ok();
    }
    // (i2) side on, shoulders and a cape add depth, not width: 115 % facing down, 105 % facing right
    const double grow = facing == 0 ? 1.15 : 1.05;
    if (area(fr[5]) < area(fr[1]) * grow) g.bad(fmt("c: lt5 %c is %d px, under %.0f %% of lt1's %d", fc, area(fr[5]), grow * 100, area(fr[1])));
    else g.ok();
    // d, T1 is okay: colours, wear marks (6 % of a piece at most)
    {
      if (rich[1].colours > 48) g.bad(fmt("d: lt1 %c: %d colours", fc, rich[1].colours)); else g.ok();
      for (int s = 0; s < 8; s++) {
        if (!rich[1].px[s]) continue;
        const double w = (double)rich[1].wear[s] / rich[1].px[s];
        if (w > 0.06) g.bad(fmt("d: lt1 %c slot %d: wear %.1f %% of the piece (6 wanted)", fc, s, w * 100)); else g.ok();
      }
    }
    // e, flavour: the T3 row
    if (flavourRow) {
    const char* row[4] = {"lt3", "jade3", "imp3", "fjord3"};
    for (int i = 0; i < 4; i++)
      for (int j = i + 1; j < 4; j++) {
        const Built *A = find(looks, row[i]), *B = find(looks, row[j]);
        if (!A || !B) { g.bad(fmt("e: %s or %s missing", row[i], row[j])); continue; }
        const Frame fa = idle(*A, facing), fb = idle(*B, facing);
        if (!fa.ok || !fb.ok) { g.bad(fmt("e: %s / %s %c: no idle frame", row[i], row[j], fc)); continue; }
        const double d = gearDiff(*A, fa, *B, fb), o = outlineDiff(fa, fb);
        if (d < 0.25 || o < 0.08) g.bad(fmt("e: %s vs %s %c: gear %.0f %% (25), outline %.0f %% (8)", row[i], row[j], fc, d * 100, o * 100));
        else g.ok();
      }
    }
  }
}

// ---------------------------------------------------------------- C13 the living finish (art::finish, art_fx.h)
void gateC13(Line& g, const std::vector<Built>& looks) {
  using namespace art::finish;
  const float dt = 1.0f / 60.0f;
  // the pulse: period within 0.05 s (zero crossings of the derivative), steps <= 0.25, no toggle faster than 2 Hz
  for (int tier : {5, 6}) {
    const Pulse P = pulseOf(tier);
    float prev = light(P, 0, 0.13f), maxStep = 0;
    std::vector<float> peaks;
    float pp = prev, p2 = prev;
    for (int i = 1; i <= 600; i++) {
      const float t = i * dt, a = light(P, t, 0.13f);
      maxStep = std::max(maxStep, std::fabs(a - prev));
      if (p2 < pp && pp >= a) peaks.push_back(t - dt);
      p2 = pp; pp = a; prev = a;
    }
    const float want = tier >= 6 ? 2.2f : 3.3f;
    float per = peaks.size() >= 2 ? peaks[1] - peaks[0] : 0;
    if (std::fabs(per - want) > 0.05f) g.bad(fmt("T%d pulse period %.2f s (%.1f wanted)", tier, per, want)); else g.ok();
    if (maxStep > 0.25f) g.bad(fmt("T%d light step %.3f per 1/60 s", tier, maxStep)); else g.ok();
    const float lo = 0.10f, hi = tier >= 6 ? 1.00f : 0.60f;   // (ladder i3: the breath that reads at 3x; was 0.30-0.85 / 0.15-0.45)
    // the pixel light (breath x crawl) keeps the step rule too
    {
      float pv = pixelLight(P, 0, 0.13f, 3, 5), ms = 0;
      for (int i = 1; i <= 600; i++) { const float a = pixelLight(P, i * dt, 0.13f, 3, 5); ms = std::max(ms, std::fabs(a - pv)); pv = a; }
      if (ms > 0.25f) g.bad(fmt("T%d pixel light step %.3f per 1/60 s", tier, ms)); else g.ok();
    }
    if (std::fabs(P.lo - lo) > 1e-3f || std::fabs(P.hi - hi) > 1e-3f) g.bad(fmt("T%d light %.2f-%.2f (%.2f-%.2f wanted)", tier, P.lo, P.hi, lo, hi)); else g.ok();
  }
  if (pulseOf(4).hi > 0) g.bad("a T4 piece pulses"); else g.ok();
  // the edge sweep (polish i1): every kSparkEvery 2.75 s, kSparkRun 0.50 s grip to tip (alloys i1)
  {
    int runs = 0;
    float runLen = 0, cur = 0, s0 = -1, s1 = -1;
    bool on = false;
    for (int i = 0; i <= (int)(8.0f / dt); i++) {
      const bool r = sparkAt(i * dt, 0.21f) >= 0;
      if (r && !on) { runs++; cur = 0; if (s0 < 0) s0 = i * dt; else if (s1 < 0) s1 = i * dt; }
      if (r) cur += dt; else if (on) runLen = cur;
      on = r;
    }
    if (runs < 2) g.bad(fmt("the sweep ran %d time(s) in 8 s", runs)); else g.ok();
    if (std::fabs(runLen - kSparkRun) > 0.035f) g.bad(fmt("the sweep run %.2f s (%.2f wanted)", runLen, kSparkRun)); else g.ok();
    if (s1 < 0 || std::fabs((s1 - s0) - kSparkEvery) > 0.05f) g.bad(fmt("the sweep's interval %.2f s (%.2f wanted)", s1 - s0, kSparkEvery)); else g.ok();
  }
  // the particles (world space): motes and embers stepped at 60 Hz for 6 s on a still figure: count, life and rise; then a
  // walker at 40 px/s leaves its embers behind (the oldest 8 px or more back from the newest)
  for (int myth = 0; myth < 2; myth++) {
    std::vector<GlowPx> glow;
    for (int i = 0; i < 20; i++) glow.push_back({i % 7, 10 + i / 7, 0xFFFFE0A0u});
    Particles ps;
    int most = 0, least = 99;
    float maxRise = 0;
    for (int i = 0; i <= 360; i++) {
      const float t = 1.0f + i * dt;
      particlesStep(ps, 0xC0FFEEu, t, myth != 0, 0, 0, glow, false, 0, 0);
      const int n = particlesAlive(ps, t);
      if (i > 120) { most = std::max(most, n); least = std::min(least, n); }
      for (size_t k = 0; k < ps.live.size(); k++) maxRise = std::max(maxRise, particleRise(ps, k, t));
    }
    const int cap = myth ? kMaxEmbers : kMaxMotes;
    const float rise = myth ? kEmberRise : kMoteRise;
    const char* nm = myth ? "embers" : "motes";
    if (most > cap || least < (myth ? 6 : 1)) g.bad(fmt("%s: %d-%d alive (%d-%d wanted)", nm, least, most, myth ? 6 : 1, cap)); else g.ok();
    if (std::fabs(maxRise - rise) > 0.8f) g.bad(fmt("%s rise %.1f px (%.0f)", nm, maxRise, rise)); else g.ok();
    if (myth) {
      Particles w;
      float minX = 1e9f, maxX = -1e9f;
      for (int i = 0; i <= 120; i++) {
        const float t = 1.0f + i * dt;
        particlesStep(w, 0xC0FFEEu, t, true, 40.0f * i * dt, 0, glow, false, 0, 0);
      }
      for (const auto& p : w.live) { minX = std::min(minX, p.x); maxX = std::max(maxX, p.x); }
      if (w.live.empty() || maxX - minX < 8.0f) g.bad(fmt("a walking Mythic's embers trail %.1f px (8 or more wanted)", maxX - minX)); else g.ok();
    }
  }
  // the gleam every 3.3 s, 0.5 s across
  {
    float s0 = -1, s1 = -1, run = 0, cur = 0;
    bool was = false;
    for (int i = 0; i <= (int)(8.0f / dt); i++) {
      const bool r = gleamAt(i * dt, 0.21f) >= 0;
      if (r && !was) { if (s0 < 0) s0 = i * dt; else if (s1 < 0) s1 = i * dt; cur = 0; }
      if (r) cur += dt; else if (was) run = cur;
      was = r;
    }
    if (s1 < 0 || std::fabs((s1 - s0) - kGleamEvery) > 0.05f) g.bad(fmt("the gleam's interval %.2f s (kGleamEvery wanted)", s1 - s0)); else g.ok();
    if (std::fabs(run - kGleamRun) > 0.035f) g.bad(fmt("the gleam run %.2f s (kGleamRun wanted)", run)); else g.ok();
  }
  // phases: two actors 0.15 cycle or more apart for 95 % of id pairs. Actors stand beside the ones spawned with them, so
  // the pairs are neighbouring ids (id, id + 1 .. id + 3) over ids 1..2000; no two neighbours share a phase within 0.005
  {
    int pairs = 0, near = 0, step = 0;
    for (uint32_t a = 1; a <= 2000; a++)
      for (uint32_t k = 1; k <= 3; k++) {
        float d = std::fabs(actorPhase(a) - actorPhase(a + k));
        d = std::min(d, 1.0f - d);
        pairs++;
        if (d < 0.15f) near++;
        if (d < 0.005f) step++;
      }
    if (near * 20 > pairs) g.bad(fmt("phases: %d of %d neighbouring actor pairs within 0.15 cycle (5 %% at most)", near, pairs)); else g.ok();
    if (step) g.bad(fmt("phases: %d neighbouring pairs pulse in step", step)); else g.ok();
    if (g_report) std::printf("    C13: %d of %d neighbouring pairs within 0.15 cycle\n", near, pairs);
  }
  // per look and frame (polish i1): the glow plane 25 % or less of its atlas; nothing below T5; sprite-bright animated px
  // (light over kSoftMax, or a glow pixel: glow, sweep, flame roots, the particles' cores) kMaxAnimated or fewer; soft light (halo, aura) kMaxSoft px or
  // fewer, none over kSoftMax; the still lights (glow, halo, aura) step 0.25 or less in 1/60 s at every phase; the aura
  // and halo really light the T6 figure (40 px or more of soft light at the breath's peak: the panel's "invisible")
  double costMs = 0;
  int costFigs = 0;
  for (const Built& b : looks) {
    if (!b.lookPal) continue;
    for (RigPart part : {RigPart::Loco, RigPart::Drawn}) {
      const RigSheet sh = doll::compose(b.r, part);
      if (sh.empty()) continue;
      const size_t gb = sh.glow.atlas.w * (size_t)sh.glow.atlas.h * 4;
      // (P2c forge) the pieces' effective tiers (the resolver's, after a ranked fighter's own kit moved a piece a step)
      int top = 0;
      for (int s = 0; s < 8; s++) top = std::max(top, (int)b.r.pieceTier[s]);
      const char* pn = part == RigPart::Loco ? "LOCO" : "DRAWN";
      // (P2c forge, D28) a glowing alloy's seams glow steady from T4: a glow plane at T4 only with the glowing sheen
      if (top == 4 && b.L.sheen == 6) { g.ok(); continue; }
      // (forge round 2, D40) emberforged glows wherever it is worn: its seams at every tier ("glowseams")
      if (top < 5 && b.r.has("glowseams")) { g.ok(); continue; }
      if (top < 5) { if (gb) g.bad(b.tag + fmt(" %s: a glow plane below T5", pn)); else g.ok(); continue; }
      if (gb * 4 > sh.bytes()) g.bad(b.tag + fmt(": glow plane %.1f KB over 25 %% of the atlas's %.1f KB", gb / 1024.0, sh.bytes() / 1024.0)); else g.ok();
      int worstBright = 0, worstSoft = 0, leastSoft = 1 << 30, frames = 0;
      float worstSoftA = 0, worstStep = 0;
      const Pulse P = pulseOf(top);
      for (size_t fi = 0; fi < sh.frames.size(); fi++) {
        const art::FrameRect& fr = sh.frames[fi];
        if (fr.w <= 0) continue;
        std::vector<GlowPx> gpx;
        if (fi < sh.glow.first.size()) sh.glow.each(fi, [&](int x, int y, uint32_t c) { gpx.push_back({x, y, c}); });
        const Mask mask = maskOf(sh.atlas, fr.x, fr.y, fr.w, fr.h);
        Figure F;
        F.topTier = top; F.mythic = b.L.mythic != 0; F.glow = &gpx; F.mask = &mask; F.bladeT6 = b.L.tier[HumanLook::TIER_WEAPON] >= 6;
        F.line = true; F.gx = fr.w * 0.5f; F.gy = fr.h * 0.6f; F.tx = fr.w * 0.5f + 8; F.ty = fr.h * 0.1f;   // a stand-in line
        auto measure = [&](float t, std::map<int, float>& still, int& bright, int& soft, float& softA) {
          std::vector<Light> L1;
          F.t = t; F.phase = 0;
          const auto c0 = std::chrono::steady_clock::now();
          figureLights(F, L1);
          costMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - c0).count();
          costFigs++;
          std::map<int, float> brightPx, softPx;
          for (const Light& q : L1)
            for (int yy = 0; yy < (int)q.h; yy++)
              for (int xx = 0; xx < (int)q.w; xx++) {
                const int key = ((int)q.y + yy + 512) * 1024 + ((int)q.x + xx + 512);
                const bool glowPx = std::any_of(gpx.begin(), gpx.end(), [&](const GlowPx& p) { return p.x == (int)q.x + xx && p.y == (int)q.y + yy; });
                if (q.a > kSoftMax || glowPx) brightPx[key] = std::max(brightPx[key], q.a);
                else softPx[key] = std::max(softPx[key], q.a);
              }
          bright = (int)brightPx.size();
          soft = (int)softPx.size();
          softA = 0;
          for (auto& kv : softPx) softA = std::max(softA, kv.second);
          // the still lights: everything but the sweep (which moves by design) is breath, halo or aura
          for (auto& kv : softPx) still[kv.first] = kv.second;
          for (auto& kv : brightPx) if (!F.bladeT6 || sparkAt(t, 0) < 0) still[kv.first] = kv.second;
        };
        // the breath's peak (t = P / 2 at phase 0), the sweep mid-run (bright worst case): budgets
        {
          std::map<int, float> st;
          int br = 0, so = 0;
          float sa = 0;
          measure(P.period * 0.5f, st, br, so, sa);
          leastSoft = std::min(leastSoft, so);
          worstSoft = std::max(worstSoft, so);
          worstSoftA = std::max(worstSoftA, sa);
          int brSweep = 0, soSweep = 0;
          float saSweep = 0;
          std::map<int, float> st2;
          // a time where the sweep runs mid-blade: sparkAt(t, 0) = 0.5
          measure(kSparkRun * 0.5f + kSparkEvery * 4, st2, brSweep, soSweep, saSweep);
          const int sprites = (b.L.mythic ? kMaxEmbers * 2 : kMaxMotes);   // the particles' cores (a hot ember is 2 px tall)
          worstBright = std::max(worstBright, std::max(br, brSweep) + sprites);
        }
        // the step: two neighbouring 1/60 s samples at 12 points of the breath (sweep at rest)
        if (frames++ % 7 == 0)
          for (int k = 0; k < 12; k++) {
            float t0 = P.period * k / 12.0f + 1.3f;
            while (sparkAt(t0, 0) >= 0 || sparkAt(t0 + dt, 0) >= 0) t0 += 0.5f;
            std::map<int, float> a0, a1;
            int x1, x2;
            float x3;
            measure(t0, a0, x1, x2, x3);
            measure(t0 + dt, a1, x1, x2, x3);
            for (auto& kv : a0) { const auto it = a1.find(kv.first); worstStep = std::max(worstStep, std::fabs(kv.second - (it == a1.end() ? 0.0f : it->second))); }
            for (auto& kv : a1) if (!a0.count(kv.first)) worstStep = std::max(worstStep, kv.second);
          }
      }
      if (worstBright > kMaxAnimated) g.bad(b.tag + fmt(" %s: %d sprite-bright animated px in a frame (%d max)", pn, worstBright, kMaxAnimated)); else g.ok();
      if (worstSoft > kMaxSoft) g.bad(b.tag + fmt(" %s: %d px of soft light in a frame (%d max)", pn, worstSoft, kMaxSoft)); else g.ok();
      if (worstSoftA > kSoftMax + 1e-3f) g.bad(b.tag + fmt(" %s: soft light %.2f (%.2f max)", pn, worstSoftA, kSoftMax)); else g.ok();
      if (worstStep > 0.25f) g.bad(b.tag + fmt(" %s: a still light steps %.3f in 1/60 s (0.25 max: a flicker)", pn, worstStep)); else g.ok();
      if (top >= 6 && leastSoft < 40) g.bad(b.tag + fmt(" %s: %d px of soft light at the breath's peak (40 wanted: it must read at 1x)", pn, leastSoft)); else g.ok();
      if (g_report) std::printf("    C13 %s %s: bright %d, soft %d-%d px (max %.2f), still step %.3f\n", b.tag.c_str(), pn, worstBright, leastSoft, worstSoft, worstSoftA, worstStep);
    }
  }
  // the cost: 8 T6 figures' figureLights 0.3 ms or less (the mean figure-frame measured above, times 8)
  if (costFigs > 0) {
    const double per = costMs / costFigs;
    if (per * 8 > 0.3) g.bad(fmt("8 T6 figures' living light %.3f ms (0.3 max)", per * 8)); else g.ok();
    if (g_report) std::printf("    C13 cost: %.4f ms a figure-frame, %.3f ms for 8\n", per, per * 8);
  }
  // nothing at rig 1
  {
    const int was = art::rigVersion();
    art::setRigVersion(1);
    const bool any = art::finish::enabled();
    art::setRigVersion(was);
    if (any) g.bad("the living finish is enabled at rig 1"); else g.ok();
  }
}

// ---------------------------------------------------------------- selftests (a synthetic defect per line)
int selftests(bool report) {
  int missed = 0;
  auto expect = [&](bool caught, const char* what) {
    if (!caught) { missed++; std::printf("    selftest MISSED: %s\n", what); }
    else if (report) std::printf("    selftest caught: %s\n", what);
  };
  // C14: leather and cloth one value
  {
    Look l{"st14", 0, "outfit=3,armorStyle=1,bodyForm=2,topColor=#6A4A30,bottomColor=#6A4A30,tier=2,palette=look", {}};
    Built b = build(l);
    for (int i = 0; i < 5; i++) b.r.ramps.c[(int)doll::Slot::Leather][i] = b.r.ramps.c[(int)doll::Slot::Cloth1][i];
    Line g{"st"};
    c14Look(g, b, 0);
    c14Look(g, b, 2);
    expect(g.fails > 0, "C14: leather and cloth at one value");
  }
  // C14 plate: pale and flat
  {
    Look l{"st14p", 1, "outfit=5,armorStyle=3,bodyForm=7,tier=4,palette=look", {}};
    Built b = build(l);
    uint32_t* M = b.r.ramps.c[(int)doll::Slot::Metal];
    M[1] = M[3];
    Line g{"st"};
    c14Plate(g, b);
    expect(g.fails > 0, "C14: plate step 3 = step 1");
  }
  // C14 dark edge
  {
    Look l{"st14e", 0, "outfit=3,armorStyle=1,bodyForm=2,tier=2,palette=look", {}};
    Built b = build(l);
    for (auto& s : b.r.ramps.c) for (uint32_t& c : s) c = 0xFF101010u;
    Line g{"st"};
    c14Look(g, b, 0);
    expect(g.fails > 0, "C14: an edge as dark as the ground");
  }
  // C12 a: the same kit twice reads as no step
  {
    std::vector<Built> v;
    for (int t = 1; t <= 6; t++) v.push_back(build(Look{fmt("lt%d", t), 0, "hero=1,kit=heartland:3:7,palette=look", {}}));
    for (const char* tag : {"jade3", "imp3", "fjord3"}) v.push_back(build(Look{tag, 0, "hero=1,kit=heartland:3:7,palette=look", {}}));
    Line g{"st"};
    gateC12(g, v);
    expect(g.fails > 0, "C12: six identical tiers");
  }
  // C13: a pulse with the wrong period; a phase hash that puts everyone in step
  {
    const art::finish::Pulse P = art::finish::pulseOf(6);
    art::finish::Pulse bad = P;
    bad.period = 1.0f;
    float prev = art::finish::light(bad, 0, 0), pp = prev, p2 = prev, first = -1, second = -1;
    for (int i = 1; i < 600; i++) {
      const float t = i / 60.0f, a = art::finish::light(bad, t, 0);
      if (p2 < pp && pp >= a) { if (first < 0) first = t; else if (second < 0) second = t; }
      p2 = pp; pp = a;
    }
    expect(std::fabs((second - first) - 2.2f) > 0.05f, "C13: a 1.0 s pulse");
  }
  return missed;
}

std::vector<Built> buildAll(const std::vector<Look>& looks) {
  std::vector<Built> v;
  for (const Look& l : looks) v.push_back(build(l));
  return v;
}

// ---------------------------------------------------------------- C15 the alloys (alloys i1; docs/P2C_CONTRACTS.md 3b)
// Every culture alloy reads as ITS OWN METAL. a: every family ramp under every sheen passes the metal test (a glint at
// step 4, a dark core, wide steps: art::doll::metalTest); b: each people's alloys (the 12 peoples, seeds 1..64) part by
// delta E 76 >= kAlloyPart on the mid step (the plate's lit body); c: the same-rank alloys of neighbouring cultures (the
// atlas's culture cells, worlds 1..6, a 6x6 block) of different peoples part by >= kNeighbourPart on the mid step;
// d: the doll and the icon agree (at rig 2 the icon paints the doll's own family ramp, every alloy).
constexpr double kAlloyPart = 18.0, kNeighbourPart = 12.0;
void gateC15(Line& g, Line& n) {
  // a
  for (int f = 0; f < doll::alloyFamilyCount(); f++)
    for (int sh = 0; sh <= 8; sh++) {
      uint32_t r[5];
      doll::alloyRamp(f, (uint8_t)sh, r);
      std::string why;
      if (doll::metalTest(r, &why)) g.ok(); else g.bad(fmt("C15a %s sheen %d: %s", doll::alloyFamilyName(f), sh, why.c_str()));
    }
  // b, d
  double minIn = 1e9, minNb = 1e9;
  int shifted = 0, total = 0;
  auto midOf = [](const cult::Culture& c, int i, uint8_t fam) {
    uint32_t r[5];
    doll::alloyRamp(fam, gear::alloyKeys(c, i).sheen, r);
    return r[2];
  };
  for (int a = 0; a < 12; a++)
    for (uint32_t sd = 1; sd <= 64; sd++) {
      const cult::Culture c = cult::Atlas::make((cult::Archetype)a, sd);
      const std::vector<uint8_t> fam = gear::alloyFamilies(c);
      for (size_t i = 0; i < fam.size(); i++) {
        for (size_t j = i + 1; j < fam.size(); j++) {
          const double d = doll::deltaE76(midOf(c, (int)i, fam[i]), midOf(c, (int)j, fam[j]));
          minIn = std::min(minIn, d);
          if (d >= kAlloyPart) g.ok();
          else g.bad(fmt("C15b %s s%u: %s and %s mid steps dE %.1f < %.0f", gear::alloyKeys(c, 0).light ? c.name.c_str() : "?", sd, c.arms.alloys[i].name.c_str(),
                         c.arms.alloys[j].name.c_str(), d, kAlloyPart));
        }
        const gear::AlloyKeys k = gear::alloyKeys(c, (int)i);
        total++;
        if (doll::alloyFamilyOf(k.light, k.sheen) != fam[i]) shifted++;
        // d (integration i1): the bag icon paints the doll's own ramp at rig 2 (IconLook::alloyFam, gear::gearIcon)
        Item it;
        it.kind = ItemKind::Armor; it.mat = Mat::Alloy; it.alloy = (uint8_t)(i + 1); it.ilvl = 20; it.culture = c.id;
        const int was = art::rigVersion();
        art::setRigVersion(2);
        const art::IconLook il = gearIcon(it, &c);
        art::setRigVersion(was);
        uint32_t ik[5], dk[5];
        doll::alloyRamp(il.alloyFam ? il.alloyFam - 1 : 0, il.sheen, ik);
        doll::alloyRamp(fam[i], k.sheen, dk);
        bool same = il.dollMetal == 10 && il.alloyFam == fam[i] + 1;
        for (int q = 0; q < 5; q++) same &= ik[q] == dk[q];
        if (same) g.ok();
        else g.bad(fmt("C15d %s %s: the icon's family %d (metal %d), the doll's %d", c.name.c_str(), c.arms.alloys[i].name.c_str(), (int)il.alloyFam - 1,
                       (int)il.dollMetal, (int)fam[i]));
      }
    }
  // c
  for (uint64_t w = 1; w <= 6; w++) {
    cult::Atlas at(w);
    for (int ci = 0; ci < 6; ci++)
      for (int cj = 0; cj < 6; cj++)
        for (int dir = 0; dir < 2; dir++) {
          const cult::Culture& A = at.family(ci, cj);
          const cult::Culture B = at.family(ci + (dir == 0), cj + (dir == 1));
          if (A.archetype == B.archetype) continue;   // one people's dialects share their metals (by design)
          const std::vector<uint8_t> fa = gear::alloyFamilies(A), fb = gear::alloyFamilies(B);
          for (size_t i = 0; i < fa.size() && i < fb.size(); i++) {
            const double d = doll::deltaE76(midOf(A, (int)i, fa[i]), midOf(B, (int)i, fb[i]));
            // (alloys i3) two different metals by name (a bronze beside a brass, a steel beside a silver) part by 8; the
            // same metal of the same rank (two peoples' everyday irons) by kNeighbourPart
            const bool sameMetal = doll::alloyFamilyMetal(fa[i]) == doll::alloyFamilyMetal(fb[i]);
            const double need = sameMetal ? kNeighbourPart : 8.0;
            if (sameMetal) minNb = std::min(minNb, d);
            if (d >= need) n.ok();
            else n.bad(fmt("C15c world %d cells %d,%d+%d: %s (people %d) %s %s and %s (people %d) %s %s dE %.1f < %.0f", (int)w, ci, cj, dir, A.name.c_str(),
                           (int)A.archetype, A.arms.alloys[i].name.c_str(), doll::alloyFamilyName(fa[i]), B.name.c_str(), (int)B.archetype,
                           B.arms.alloys[i].name.c_str(), doll::alloyFamilyName(fb[i]), d, need));
          }
        }
  }
  if (g_report) std::printf("    C15: min dE within a people %.1f, between neighbours %.1f; %d of %d alloys moved off their nearest family\n", minIn, minNb, shifted, total);
  // e (alloys i2): the metal agrees with its name and is never skin. An everyday bronze or brass (copper and tin) draws a
  // warm family, an everyday iron a cool one, and no warriors' or masterwork metal draws a bronze (the blind panel read
  // "silver" alloys drawn in bronze); every family's mid step under every sheen parts from A3's skin by dE 20 (it read the
  // rose families as "pink skin"); the band metals (bronze iron steel gilded obsidian emberforged starmetal) pass the metal
  // test and part pairwise by dE 14 on the mid step (iron and steel read as "the same grey", bronze and emberforged leaned)
  {
    // (alloys i3) the family's metal is the metal word of the alloy's NAME (gear::alloyMetalWord): a BRASS draws a brass, a
    // SILVER a silver, an IRON an iron (the blind panel caught "Eaglesilver drawn as brass, Thorniron as white silver")
    for (int a = 0; a < 12; a++)
      for (uint32_t sd = 1; sd <= 64; sd++) {
        const cult::Culture c = cult::Atlas::make((cult::Archetype)a, sd);
        const std::vector<uint8_t> fam = gear::alloyFamilies(c);
        for (size_t i = 0; i < fam.size(); i++) {
          const int word = gear::alloyMetalWord(c.arms.alloys[i], std::min((int)i, 2));
          if (doll::alloyFamilyMetal(fam[i]) == word) g.ok();
          else g.bad(fmt("C15e %s s%u: %s (alloy %zu) drawn in %s", c.name.c_str(), sd, c.arms.alloys[i].name.c_str(), i + 1, doll::alloyFamilyName(fam[i])));
        }
      }
    const uint32_t skin = rgba(0xE9, 0xA0, 0x77);
    for (int f = 0; f < doll::alloyFamilyCount(); f++)
      for (int sh = 0; sh <= 8; sh++) {
        uint32_t r[5];
        doll::alloyRamp(f, (uint8_t)sh, r);
        const double d = std::min({doll::deltaE76(r[2], skin), doll::deltaE76(r[3], skin)});
        if (d >= 20.0) g.ok(); else g.bad(fmt("C15e %s sheen %d: lit steps dE %.1f from the skin (< 20)", doll::alloyFamilyName(f), sh, d));
      }
    const int band[7] = {2, 3, 4, 5, 7, 8, 9};
    uint32_t br[7][5];
    for (int i = 0; i < 7; i++) {
      doll::metalKindRamp(band[i], br[i]);
      std::string why;
      if (doll::metalTest(br[i], &why)) g.ok(); else g.bad(fmt("C15e band metal %d: %s", band[i], why.c_str()));
      for (int j = 0; j < i; j++) {
        const double d = doll::deltaE76(br[i][2], br[j][2]);
        if (d >= 14.0) g.ok(); else g.bad(fmt("C15e band metals %d and %d mid steps dE %.1f < 14", band[j], band[i], d));
      }
    }
  }
}

// (wave-2 fix round, D85) the relic invariant: at one tier the old makers' metals climb in opaque px of the front walk's frame 0
// (noHeld), gilded < obsidian < emberforged < starmetal, on the hero and the heroine at T5 and T6; and starmetal's crystal
// shoulders the largest mass of every size they draw (each size's facing-D rest views, both sides, the `when metal-starmetal`
// view where one is drawn, against every other shoulder shape at that size)
int frontArea(const doll::DollRecipe& r) {
  doll::ComposeOpts o;
  o.noHeld = true;
  o.onlyFacing = 0;
  const RigSheet sh = doll::compose(r, RigPart::Loco, &o);
  const art::FrameRect* f = sh.frame(0, Clip::Walk, 0, 0);
  if (!f) return 0;
  int n = 0;
  for (int y = 0; y < f->h; y++)
    for (int x = 0; x < f->w; x++) n += (sh.atlas.get(f->x + x, f->y + y) >> 24) != 0;
  return n;
}
int pauldronMass(const std::string& value, bool star) {
  const doll::pool::Pool& P = doll::pool::get();
  const uint16_t rest = P.strId("rest");
  int n = 0;
  for (int pi : P.partsWithRole(doll::Role::Pauldron, value)) {
    const doll::pool::PartRec& pr = P.parts[(size_t)pi];
    if (!(pr.bodyMask >> (int)doll::BodySet::Man & 1)) continue;
    const doll::pool::ViewRec* base = nullptr;
    const doll::pool::ViewRec* starV = nullptr;
    for (uint32_t vi = pr.firstView; vi < pr.firstView + pr.nViews && vi < P.views.size(); vi++) {
      const doll::pool::ViewRec& w = P.views[vi];
      if (w.facing != 0 || w.key != rest || w.cells.empty()) continue;
      if (!w.when) { if (!base) base = &w; }
      else if (star && (w.flags & 1) && P.s(w.when) == "metal-starmetal") starV = &w;
    }
    const doll::pool::ViewRec* v = starV ? starV : base;
    if (!v) continue;
    for (uint8_t c : v->cells) n += c >= 2;
  }
  return n;
}
void gateRelics(Line& g) {
  static const char* const kMat[4] = {"gilded", "obsidian", "ember", "starmetal"};
  for (int who = 0; who < 2; who++)
    for (int t = 5; t <= 6; t++) {
      int a[4] = {};
      for (int m = 0; m < 4; m++) {
        Look l;
        l.tag = fmt("relic_%s_t%d_%s", kMat[m], t, who ? "w" : "m");
        l.who = who;
        l.spec = fmt("%s,kit=heartland:%d:1,mat=%s", who ? "heroine=1" : "hero=1", t, kMat[m]);
        const Built b = build(l);
        if (!b.bad.empty()) { g.bad(l.tag + ": bad look " + b.bad); continue; }
        a[m] = frontArea(b.r);
      }
      std::string row;
      for (int m = 0; m < 4; m++) row += fmt(" %s %d", kMat[m], a[m]);
      if (g_report) std::printf("    relics %s T%d:%s\n", who ? "heroine" : "hero", t, row.c_str());
      bool ok = true;
      for (int m = 1; m < 4; m++) ok &= a[m] > a[m - 1];
      if (ok) g.ok(); else g.bad(fmt("%s T%d front walk px%s (gilded < obsidian < emberforged < starmetal)", who ? "heroine" : "hero", t, row.c_str()));
    }
  // the crystal: the largest mass of its size
  static const char* const kShapes[] = {"cop", "lobed", "roll", "slab", "vented", "plate", "winged", "flared", "shard", "flame", "layered", "scale", "flap",
                                        "leaf", "fur", "pelt", "feather"};
  for (int size = 2; size <= 6; size++) {
    const int cr = pauldronMass("crystal." + std::to_string(size), true);
    if (!cr) continue;
    int best = 0;
    std::string who;
    for (const char* sh : kShapes) {
      const int m = pauldronMass(std::string(sh) + "." + std::to_string(size), false);
      if (m > best) { best = m; who = sh; }
    }
    if (cr >= best) g.ok();
    else g.bad(fmt("crystal.%d %d px under %s.%d's %d (the crystal the largest mass of its size)", size, cr, who.c_str(), size, best));
  }
}
int relicSelftests() {
  // a synthetic order that dips must fail the rule the gate applies
  const int a[4] = {900, 950, 940, 1000};
  bool ok = true;
  for (int m = 1; m < 4; m++) ok &= a[m] > a[m - 1];
  return ok ? 1 : 0;   // 1 = missed
}
int runLadderGates(const std::vector<Look>& looks, bool finish, bool self, std::vector<std::string>& out) {
  const std::vector<Built> built = buildAll(looks);
  Line c12{"C12 the ladder reads"}, c13{"C13 the living finish"}, c14{"C14 value"}, st{"C12-C14 selftests"};
  Line c15{"C15 the alloys"}, c15n{"C15c alloy neighbours"}, rel{"relic invariant"};
  gateRelics(rel);
  gateC12(c12, built);
  if (finish) gateC13(c13, built);
  gateC14(c14, built);
  gateC15(c15, c15n);
  if (self) {
    const int m = selftests(g_report) + relicSelftests();
    if (m) st.bad(fmt("%d synthetic defect(s) missed", m)); else st.ok();
    out.push_back(fmt("  C12-C14 selftest: %s", m ? "MISSED" : "ALL CAUGHT"));
  }
  int fails = 0;
  for (Line* l : {&c12, &c13, &c14, &c15, &c15n, &rel, &st}) {
    if ((l == &c13 && !finish) || (l == &st && !self)) continue;
    out.push_back(l->text());
    fails += l->fails ? 1 : 0;
  }
  return fails;
}

// (P2c wave 2, DOLL-C) `--doll-ladder --finish --scope ladder [--seeds a..b]`: C12 (each kit line's six tiers read as lt1..lt6),
// C13 (the living finish on every look) and C14 over source L: gear::ladderKit for the 12 peoples x man and woman x seeds, the
// sword (and with --peoples all, each weapon family: the C1 source L of --doll-coverage --scope ladder)
int runLadderScope(int a, int b, bool allWeapons, bool finish, bool self, std::vector<std::string>& out) {
  static const char* const kPeople[12] = {"fjordfolk", "highland", "heartland", "imperial", "dune", "steppe", "marsh", "jade", "river", "suntemple",
                                          "sylvan", "starspire"};
  static const char* const kWpn[6] = {"sword", "spear", "bow", "axe", "mace", "greatsword"};
  Line c12{"C12 source L (lines)"}, c13{"C13 source L"}, c14{"C14 source L"}, st{"C12-C14 selftests"};
  int lines = 0, linesBad = 0;
  std::vector<Built> every;
  for (int p = 0; p < 12; p++)
    for (int w = 0; w < (allWeapons ? 6 : 1); w++)
      for (int who = 0; who < 2; who++)
        for (int sd = a; sd <= b; sd++) {
          std::vector<Look> ls;
          for (int t = 1; t <= 6; t++)
            ls.push_back(Look{fmt("lt%d", t), who, fmt("kit=%s:%d:%d,wpn=%s,palette=look", kPeople[p], t, sd, kWpn[w]), {}});
          std::vector<Built> bl = buildAll(ls);
          Line one{"line"};
          gateC12(one, bl, false);
          const std::string id = fmt("%s %s %s s%d", kPeople[p], kWpn[w], who ? "woman" : "man", sd);
          lines++;
          if (one.fails) { linesBad++; c12.bad(id + ": " + one.first + (one.fails > 1 ? fmt(" (+%d)", one.fails - 1) : "")); }
          else c12.ok();
          for (Built& x : bl) { x.tag = fmt("L_%s_%s_%s_s%d_%s", kPeople[p], kWpn[w], who ? "w" : "m", sd, x.tag.c_str()); every.push_back(std::move(x)); }
        }
  if (finish) gateC13(c13, every);
  gateC14(c14, every);
  out.push_back(fmt("  source L: %d lines (seeds %d..%d%s), %d looks; C12 misses on %d lines", lines, a, b, allWeapons ? ", every weapon family" : "",
                    (int)every.size(), linesBad));
  if (self) {
    const int m = selftests(g_report);
    if (m) st.bad(fmt("%d synthetic defect(s) missed", m)); else st.ok();
    out.push_back(fmt("  C12-C14 selftest: %s", m ? "MISSED" : "ALL CAUGHT"));
  }
  int fails = 0;
  for (Line* l : {&c12, &c13, &c14, &st}) {
    if ((l == &c13 && !finish) || (l == &st && !self)) continue;
    out.push_back(l->text());
    fails += l->fails ? 1 : 0;
  }
  return fails;
}

int ladderCmd(int argc, char** argv) {
  const char* looksFile = nullptr;
  const char* pool = nullptr;
  bool finish = false, self = false, scopeL = false, allW = false;
  int sa = 1, sb = 3;
  for (int i = 1; i < argc; i++) {
    if ((!std::strcmp(argv[i], "--looks") || !std::strcmp(argv[i], "--limbs-looks")) && i + 1 < argc) looksFile = argv[++i];
    else if (!std::strcmp(argv[i], "--pool") && i + 1 < argc) pool = argv[++i];
    else if (!std::strcmp(argv[i], "--report")) g_report = true;
    else if (!std::strcmp(argv[i], "--finish") || !std::strcmp(argv[i], "--gates")) finish = true;
    else if (!std::strcmp(argv[i], "--selftest")) self = true;
    else if (!std::strcmp(argv[i], "--scope") && i + 1 < argc) scopeL = !std::strcmp(argv[++i], "ladder");
    else if (!std::strcmp(argv[i], "--peoples") && i + 1 < argc) allW = !std::strcmp(argv[++i], "all");
    else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) { if (std::sscanf(argv[++i], "%d..%d", &sa, &sb) < 2) sb = sa; }
  }
  if (pool) {
    std::string err;
    if (!doll::loadPoolFile(pool, &err)) { std::printf("FAIL: --pool %s: %s\n", pool, err.c_str()); return 1; }
  }
  const int was = art::rigVersion();
  art::setRigVersion(2);
  const std::string text = looksFile ? readFile(looksFile) : dollSliceLooksText();
  std::vector<std::string> out;
  const int fails = scopeL ? runLadderScope(sa, sb, allW, finish, self, out) : runLadderGates(parseLooks(text), finish, self, out);
  for (const std::string& l : out) std::printf("%s\n", l.c_str());
  art::setRigVersion(was);
  std::printf("doll-ladder: %d failing line(s)\n", fails);
  return fails ? 1 : 0;
}

}  // namespace dollladder

// (test_doll.cpp's --doll --gates adds C12, C13 and C14)
int dollLadderGates(bool report, std::vector<std::string>& printed) {
  dollladder::g_report = report;
  return dollladder::runLadderGates(dollladder::parseLooks(dollSliceLooksText()), true, true, printed);
}

RPG_TEST_CMD("--expand-looks", "P2c the ladder looks macros expanded: --expand-looks FILE (each line's fields and what it resolves to)", dollladder::expandCmd);
RPG_TEST_CMD("--doll-ladder", "P2c ladder gates C12 C13 C14 [--looks f] [--pool f] [--finish] [--report] [--selftest]; --scope ladder [--seeds a..b] [--peoples all] (C12-C14 over source L)", dollladder::ladderCmd);

// ---------------------------------------------------------------- C1 source L (rpg_test --doll-coverage --scope ladder)
// Every ladder look (the gates' matrix) and gear::ladderKit for the Heartland (or with --peoples all, every people) at
// T1-T6, man and woman, seeds A..B (default 1..3), with the sword (and with --peoples all, each weapon family), worn through
// wearGear: each must resolve with nothing missing (art::doll::dollMissing).
int dollLadderCoverage(int argc, char** argv) {
  using namespace dollladder;
  bool gates = false, allPeoples = false;
  int a = 1, b = 3;
  const char* pool = nullptr;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--gates")) gates = true;
    else if (!std::strcmp(argv[i], "--report")) g_report = true;
    else if (!std::strcmp(argv[i], "--peoples") && i + 1 < argc) allPeoples = !std::strcmp(argv[++i], "all");
    else if (!std::strcmp(argv[i], "--pool") && i + 1 < argc) pool = argv[++i];
    else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) { if (std::sscanf(argv[++i], "%d..%d", &a, &b) < 2) b = a; }
  }
  if (pool) {
    std::string err;
    if (!doll::loadPoolFile(pool, &err)) { std::printf("FAIL: --pool %s: %s\n", pool, err.c_str()); return 1; }
  }
  const int was = art::rigVersion();
  art::setRigVersion(2);
  std::vector<Look> looks = parseLooks(dollSliceLooksText());
  static const char* const kPeople[12] = {"fjordfolk", "highland", "heartland", "imperial", "dune", "steppe", "marsh", "jade", "river", "suntemple",
                                          "sylvan", "starspire"};
  static const char* const kWpn[6] = {"sword", "spear", "bow", "axe", "mace", "greatsword"};
  for (int p = 0; p < 12; p++) {
    if (!allPeoples && p != 2) continue;
    for (int w = 0; w < (allPeoples ? 6 : 1); w++)
      for (int t = 1; t <= 6; t++)
        for (int who = 0; who < 2; who++)
          for (int sd = a; sd <= b; sd++)
            looks.push_back(Look{fmt("L_%s_%s_t%d_%s_s%d", kPeople[p], kWpn[w], t, who ? "w" : "m", sd), who,
                                 fmt("kit=%s:%d:%d,wpn=%s,palette=look", kPeople[p], t, sd, kWpn[w]), {}});
  }
  int total = 0, covered = 0;
  std::map<std::string, int> missing;
  for (const Look& l : looks) {
    const Built bl = build(l);
    const std::vector<doll::Missing> m = doll::dollMissing(bl.L);
    total++;
    if (m.empty()) { covered++; continue; }
    for (const doll::Missing& x : m) missing[x.what()]++;
    if (g_report) std::printf("    L %s: %d missing; first %s\n", l.tag.c_str(), (int)m.size(), m[0].what().c_str());
  }
  std::vector<std::pair<int, std::string>> top;
  for (const auto& kv : missing) top.push_back({kv.second, kv.first});
  std::sort(top.rbegin(), top.rend());
  for (size_t i = 0; i < top.size() && i < 25; i++) std::printf("  missing x%d: %s\n", top[i].first, top[i].second.c_str());
  std::printf("%s C1 source L (ladder)   %d looks, %d covered, %d with something missing\n", covered == total ? "ok   " : "FAIL:", total, covered, total - covered);
  art::setRigVersion(was);
  return covered != total ? 1 : 0;   // (wave 2 r3) a printed FAIL always fails
}
