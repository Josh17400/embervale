// EMBERVALE rpg_test: the P2c envelope (FORGE-DOLL, wave 1d; docs/PLAN.md "The envelope", docs/P2C_CONTRACTS.md 3d, 7).
//   rpg_test --envelope                       the tier envelope as data (art::doll::kEnvelope), for the judge to compare
//   rpg_test --doll-envelope [--looks F] [--report] [--selftest] [--pool P]
//       C12f the envelope on every look (shoulders, head rows and columns, the cloak, T1 inside the bare outline), C12f's ladder
//       lines (area ratios, each tier 0.05 over the one below, the Mythic at its T6, heads 5 % and 0.10 head OD apart), C3b by
//       the helm's tier, B10 by tier on the envelope looks, C13's drawn motion (a frame against its breeze view)
//   rpg_test --doll-adefects [--looks F] [--report] [--selftest] [--pool P]
//       C19: the review sheet's A1-A8 reads (PLAN "The envelope" A table) on tools/doll/envelope_looks.txt
//   rpg_test --doll-heads [--report] [--selftest]   C16k (also in --doll-forge): the owner's 37 heads apart, a head per alloy
//   rpg_test --doll-coverage --scope envelope       (test_doll.cpp dispatches) the envelope's fallbacks and undrawn variants
// Every number is read from rpg/art/doll/doll_envelope.h. Planned thresholds (set by eye in round 1) are marked "planned".
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/art/doll/doll.h"
#include "rpg/art/doll/doll_envelope.h"
#include "rpg/art/doll/doll_forge.h"
#include "rpg/art/doll/doll_internal.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/gear_forge.h"
#include "rpg/sim/gear_ladder.h"
#include "tools/tests/tests.h"

namespace dollenv {

namespace doll = art::doll;
namespace F = art::doll::forge;
using art::Clip;
using art::HumanLook;
using art::RigPart;
using art::RigSheet;
using doll::Role;

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
  std::vector<std::string> notes;
  void ok() { checks++; }
  void bad(const std::string& why) {
    checks++;
    fails++;
    if (first.empty()) first = why;
    if (g_report && (fails <= 40 || std::getenv("ENV_ALL"))) std::printf("    %s: %s\n", id.c_str(), why.c_str());
  }
  void check(bool good, const std::string& why) { if (good) ok(); else bad(why); }
  std::string text() const {
    return fmt("%s %-24s %4d checks, %4d failed%s%s", fails ? "FAIL:" : "ok   ", id.c_str(), checks, fails, fails ? "; first: " : "", first.c_str());
  }
};

const char* kEnvelopeLooks =   // (P2c envelope) tools/doll/envelope_looks.txt (tools/doll/embed_looks.py writes it)
    "e1       man    hero=1,kit=heartland:1:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "e2       man    hero=1,kit=heartland:2:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "e3       man    hero=1,kit=heartland:3:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "e4       man    hero=1,kit=heartland:4:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "e5       man    hero=1,kit=heartland:5:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "e6       man    hero=1,kit=heartland:6:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "e6m      man    hero=1,kit=heartland:6:7,rarity=mythic,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "e1w      woman  heroine=1,kit=heartland:1:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "e2w      woman  heroine=1,kit=heartland:2:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "e3w      woman  heroine=1,kit=heartland:3:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "e4w      woman  heroine=1,kit=heartland:4:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "e5w      woman  heroine=1,kit=heartland:5:7,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "e6w      woman  heroine=1,kit=heartland:6:7,wpn=spear,hairColor=#8C5A2A,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "eg0      man    npc=heartland:guard:0:7,palette=look  stay,walk,run,didle,hit1,fall,lying\n"
    "eg3      man    npc=heartland:guard:0:7,tier=3,palette=look  stay,walk,run,dwalk,drun,didle,hit1,hurt,fall,lying\n"
    "ecapt    man    npc=heartland:captain:1:7,palette=look  stay,walk,run\n"
    "eking    man    npc=heartland:king:0:7,palette=look  stay,walk,run\n"
    "a_uha    man    kit=jade:4:2,alloy=1,palette=look  stay,walk,run\n"
    "a_jaon   man    kit=jade:4:5,alloy=1,palette=look  stay,walk,run\n"
    "a_enkho  man    kit=steppe:4:1,alloy=1,palette=look  stay,walk,run\n"
    "a_papir  man    kit=marsh:4:2,alloy=1,palette=look  stay,walk,run\n"
    "a_crane  man    kit=jade:5:2,alloy=3,palette=look  stay,walk,run\n"
    "a_thaer  man    kit=sylvan:5:1,alloy=3,palette=look  stay,walk,run\n"
    "a_thorn  man    kit=highland:5:1,alloy=3,palette=look  stay,walk,run\n"
    "a_ilsil  man    kit=fjordfolk:5:1,alloy=3,palette=look  stay,walk,run\n"
    "fm_obsid man    hero=1,kit=heartland:4:7,pauldron=3,skirt=3,mat=obsidian,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "f5_obsid man    hero=1,kit=heartland:5:7,mat=obsidian,palette=look  stay,walk\n"
    "fm_myth  man    hero=1,kit=heartland:6:7,rarity=mythic,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "fm_star  man    hero=1,kit=heartland:4:7,pauldron=3,skirt=3,mat=starmetal,palette=look  stay,walk,run,didle,hit1,hurt,fall,lying\n"
    "f5_star  man    hero=1,kit=heartland:5:7,mat=starmetal,palette=look  stay,walk\n";

// ---------------------------------------------------------------- looks
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
struct Look { std::string tag; int who = 0; std::string spec; };
std::vector<Look> parseLooks(const std::string& text) {
  std::vector<Look> v;
  size_t a = 0;
  while (a < text.size()) {
    size_t b = text.find('\n', a);
    if (b == std::string::npos) b = text.size();
    const std::string line = text.substr(a, b - a);
    a = b + 1;
    if (line.empty() || line[0] == '#') continue;
    char tag[128] = {}, who[16] = {}, spec[1200] = {};
    if (std::sscanf(line.c_str(), "%127s %15s %1199s", tag, who, spec) < 2) continue;
    Look l;
    l.tag = tag; l.who = !std::strcmp(who, "woman") ? 1 : 0; l.spec = std::strcmp(spec, "-") ? spec : "";
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
struct Built { std::string tag; int who = 0; HumanLook L; doll::DollRecipe r; std::string bad; };
Built build(const Look& l) {
  gear::installLadderMacros();
  Built b;
  b.tag = l.tag;
  b.who = l.who;
  b.L = l.who ? a3Woman() : a3Hero();
  bool lookPal = false, bare = false;
  doll::applyLookSpec(b.L, l.spec, &lookPal, &bare, &b.bad);
  b.r = doll::resolve(b.L);
  if (!lookPal) b.r.ramps = doll::a3Palette(b.L.body == 1 ? 1 : 0);
  return b;
}
const Built* find(const std::vector<Built>& v, const std::string& tag) {
  for (const Built& b : v) if (b.tag == tag) return &b;
  return nullptr;
}

// ---------------------------------------------------------------- frames and their pixels
bool heldIt(const doll::FrameProbe::It& it) {
  using L = doll::Layer;
  return it.has(Role::Held) || it.has(Role::Shield) || it.has(Role::Sheath) || it.layer == L::Back || it.layer == L::HeldBack ||
         it.layer == L::FxBack || it.layer == L::Fx;
}
bool helmIt(const doll::FrameProbe::It& it) { return it.has(Role::Helm) || it.has(Role::Headwear); }
bool ornIt(const doll::FrameProbe::It& it) {   // a drawn crest or plume (the crest maps, a helm's _crest / _plume part)
  return it.part.find("helm/crest/") != std::string::npos || it.part.find("_crest") != std::string::npos || it.part.find("_plume") != std::string::npos;
}
struct Fig {
  bool ok = false;
  doll::FrameProbe p;
  const doll::DollRecipe* r = nullptr;
  const doll::FrameProbe::It* it(int k) const {
    const int o = p.owner[(size_t)k];
    return o >= 0 && (size_t)o < p.items.size() ? &p.items[(size_t)o] : nullptr;
  }
  bool opaque(int k) const { return p.code[(size_t)k] >= 2; }
  // a figure pixel without the held weapon, shield, sheath or back items (ComposeOpts::noHeld's figure); an outline pixel
  // the finish added (no owner) counts when it borders a counted pixel
  bool fig(int k) const {
    if (!opaque(k)) return false;
    if (const auto* i = it(k)) return !heldIt(*i);
    const int x = k % p.w, y = k / p.w;
    for (int d : {0, 1, 2, 3}) {
      const int k2 = p.at(x + (d == 0) - (d == 1), y + (d == 2) - (d == 3));
      if (k2 >= 0 && opaque(k2) && it(k2) && !heldIt(*it(k2))) return true;
    }
    return false;
  }
  doll::Slot slot(int k) const { return doll::codeSlot(p.code[(size_t)k]); }
  int step(int k) const { return doll::codeStep(p.code[(size_t)k]); }
  uint32_t colour(int k) const { return r->ramps.c[(int)slot(k)][step(k)]; }
};
Fig probe(const Built& b, RigPart part, int facing, Clip clip, int variant, int i) {
  Fig f;
  f.r = &b.r;
  f.p = doll::probeFrame(b.r, part, facing, clip, variant, i, true);
  f.ok = f.p.ok;
  return f;
}
Fig probeR(const doll::DollRecipe& r, RigPart part, int facing, Clip clip, int variant, int i) {
  Fig f;
  f.r = &r;
  f.p = doll::probeFrame(r, part, facing, clip, variant, i, true);
  f.ok = f.p.ok;
  return f;
}
double luma(uint32_t c) { return 0.299 * (c & 255) + 0.587 * (c >> 8 & 255) + 0.114 * (c >> 16 & 255); }
// HSV hue in degrees and chroma (0..255)
void hueChroma(uint32_t c, double& hue, double& chroma) {
  const double r = c & 255, g = c >> 8 & 255, b = c >> 16 & 255;
  const double mx = std::max({r, g, b}), mn = std::min({r, g, b});
  chroma = mx - mn;
  if (chroma < 1e-6) { hue = 0; return; }
  if (mx == r) hue = 60.0 * std::fmod((g - b) / chroma + 6.0, 6.0);
  else if (mx == g) hue = 60.0 * ((b - r) / chroma + 2.0);
  else hue = 60.0 * ((r - g) / chroma + 4.0);
}
double hueDist(double a, double b) { const double d = std::fabs(a - b); return std::min(d, 360.0 - d); }

// the crown row: the man's crown at F-31, the woman's at F-30 (art_rig.h), measured from the head anchor on the bare body's
// idle D so the bob never counts (PLAN "The envelope": measured from the head anchor)
int crownOffset(int who) {
  static int off[2] = {9999, 9999};
  if (off[who] == 9999) {
    Built b;
    b.L = who ? a3Woman() : a3Hero();
    b.L.weapon = 0;
    b.r = doll::resolve(b.L);
    const Fig f = probe(b, RigPart::Loco, 0, Clip::Idle, 0, 0);
    off[who] = f.ok ? (f.p.py - (who ? 30 : 31)) - f.p.headY : 0;
  }
  return off[who];
}

// ---------------------------------------------------------------- C12f / C3b measures
int tierOf(const HumanLook& L, int slot) { return slot >= 0 && slot < HumanLook::TIER_SLOTS ? L.tier[slot] : 0; }
// the families whose horns, wings, flares, brims and tails may reach the flare columns; those with a drawn brim
bool flarePart(const std::string& n) {
  for (const char* w : {"horn", "wing", "kabuto", "warhat", "morion", "sallet"}) if (n.find(w) != std::string::npos) return true;
  return false;
}
bool brimPart(const std::string& n) {
  for (const char* w : {"kettle", "warhat", "morion", "leathercap"}) if (n.find(w) != std::string::npos) return true;
  return false;
}
struct Head { int rows = -99, ornRows = -99, cols = 0, colsBrim = 0, colsFlare = 0, px = 0; std::string worst, worstCol; };
Head headOf(const Fig& f, int who) {
  Head h;
  if (!f.ok) return h;
  const int crown = f.p.headY + crownOffset(who);
  for (int y = 0; y < f.p.h; y++)
    for (int x = 0; x < f.p.w; x++) {
      const int k = y * f.p.w + x;
      if (!f.opaque(k)) continue;
      const auto* it = f.it(k);
      if (!it || !helmIt(*it)) continue;
      h.px++;
      const int up = crown - y;
      const int col = x < f.p.headX ? f.p.headX - x : x > f.p.headX + 15 ? x - (f.p.headX + 15) : 0;
      if (ornIt(*it) || f.slot(k) == doll::Slot::Plume) h.ornRows = std::max(h.ornRows, up);
      else {
        if (up > h.rows) { h.rows = up; h.worst = it->part; }
        if (flarePart(it->part)) h.colsFlare = std::max(h.colsFlare, col);
        else if (brimPart(it->part)) h.colsBrim = std::max(h.colsBrim, col);
        else if (col > h.cols) { h.cols = col; h.worstCol = it->part; }
      }
    }
  return h;
}
// shoulders across facing D: 16 + the pauldrons' reach past the head box's 16 columns (the shoulder line) each side
int shouldersOf(const Fig& f) {
  if (!f.ok) return 16;
  int mn = 1 << 20, mx = -(1 << 20);
  for (int y = 0; y < f.p.h; y++)
    for (int x = 0; x < f.p.w; x++) {
      const int k = y * f.p.w + x;
      const auto* it = f.opaque(k) ? f.it(k) : nullptr;
      if (it && it->has(Role::Pauldron)) { mn = std::min(mn, x); mx = std::max(mx, x); }
    }
  if (mn > mx) return 16;
  if (std::getenv("ENV_DEBUG")) {
    std::printf("      shoulders: headX %d pauldron x %d..%d pivot %d;", f.p.headX, mn, mx, f.p.px);
    for (const auto& it : f.p.items)
      if (it.has(Role::Pauldron) || it.anchor == doll::Anchor::ShoulderR || it.anchor == doll::Anchor::ShoulderL)
        std::printf(" [%s side %d anchor %d at %d,%d]", it.part.c_str(), it.side, (int)it.anchor, it.ax, it.ay);
    std::printf("\n");
  }
  return 16 + std::max(0, f.p.headX - mn) + std::max(0, mx - (f.p.headX + 15));
}
int areaOf(const Fig& f) {
  int n = 0;
  if (!f.ok) return 0;
  for (int k = 0; k < f.p.w * f.p.h; k++) if (f.fig(k)) n++;
  return n;
}
int helmPx(const Fig& f) {
  int n = 0;
  if (!f.ok) return 0;
  for (int k = 0; k < f.p.w * f.p.h; k++) if (f.opaque(k) && f.it(k) && helmIt(*f.it(k))) n++;
  return n;
}
// the cloak: facing D its pixels past each leg's outer column (rows the legs reach); facing R / L past the body's box behind
int cloakSideOf(const Fig& f) {
  if (!f.ok) return 0;
  int l0 = 1 << 20, l1 = -(1 << 20), top = 1 << 20;
  for (int k = 0; k < f.p.w * f.p.h; k++) {
    const auto* it = f.opaque(k) ? f.it(k) : nullptr;
    if (it && it->has(Role::Legs)) { l0 = std::min(l0, k % f.p.w); l1 = std::max(l1, k % f.p.w); top = std::min(top, k / f.p.w); }
  }
  if (l0 > l1) return 0;
  int reach = 0;
  for (int k = 0; k < f.p.w * f.p.h; k++) {
    const auto* it = f.opaque(k) ? f.it(k) : nullptr;
    if (!it || !it->has(Role::Cloak) || k / f.p.w < top) continue;
    const int x = k % f.p.w;
    reach = std::max(reach, x < l0 ? l0 - x : x > l1 ? x - l1 : 0);
  }
  return reach;
}
int cloakBackOf(const Fig& f, int facing) {
  if (!f.ok) return 0;
  int b0 = 1 << 20, b1 = -(1 << 20);
  for (int k = 0; k < f.p.w * f.p.h; k++) {
    const auto* it = f.opaque(k) ? f.it(k) : nullptr;
    if (it && (it->has(Role::Legs) || it->has(Role::Arms) || it->has(Role::Body) || it->has(Role::Head) || it->has(Role::Armour) ||
               it->has(Role::Garment))) { b0 = std::min(b0, k % f.p.w); b1 = std::max(b1, k % f.p.w); }
  }
  if (b0 > b1) return 0;
  int reach = 0;
  for (int k = 0; k < f.p.w * f.p.h; k++) {
    const auto* it = f.opaque(k) ? f.it(k) : nullptr;
    if (!it || !it->has(Role::Cloak)) continue;
    const int x = k % f.p.w;
    reach = std::max(reach, facing == 2 ? b0 - x : x - b1);   // facing R the back is to the left
  }
  return std::max(0, reach);
}
// T1 inside the bare body's outline + 1 px (the hood's rows inside the hair's 2-row rise exempt)
int t1Outside(const Fig& f, const Fig& bare, int who) {
  if (!f.ok || !bare.ok) return 0;
  std::set<std::pair<int, int>> dil;
  for (int k = 0; k < bare.p.w * bare.p.h; k++) {
    if (!bare.fig(k)) continue;
    const int x = k % bare.p.w - bare.p.px, y = k / bare.p.w - bare.p.py;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) dil.insert({x + dx, y + dy});
  }
  const int crown = f.p.headY + crownOffset(who);
  int out = 0;
  for (int k = 0; k < f.p.w * f.p.h; k++) {
    if (!f.fig(k)) continue;
    const int x = k % f.p.w - f.p.px, y = k / f.p.w - f.p.py;
    if (dil.count({x, y})) continue;
    const auto* it = f.it(k);
    if (it && helmIt(*it) && crown - k / f.p.w <= 2) continue;   // the hood inside the hair's rise
    if (std::getenv("ENV_DEBUG")) std::printf("      T1 outside: %d,%d %s (anchor %d,%d pivot %d,%d)\n", x, y, it ? it->part.c_str() : "?", it ? it->ax : 0,
                                               it ? it->ay : 0, f.p.px, f.p.py);
    out++;
  }
  return out;
}

// ---------------------------------------------------------------- the pure rules (the selftests drive them too)
std::string shoulderRule(int tierBody, int shoulders) {
  const doll::TierEnvelope& e = doll::envelopeOf(tierBody);
  if (tierBody >= 1 && e.shoulders && shoulders > e.shoulders) return fmt("shoulders %d px across (T%d: %d)", shoulders, tierBody, e.shoulders);
  return "";
}
std::string cloakRule(int tierCloak, int sideD, int back) {
  const doll::TierEnvelope& e = doll::envelopeOf(tierCloak);
  if (sideD > e.cloakSideD) return fmt("cloak %d px past a leg facing D (T%d: %d)", sideD, tierCloak, e.cloakSideD);
  if (back > e.cloakBack) return fmt("cloak %d columns behind facing R / L (T%d: %d)", back, tierCloak, e.cloakBack);
  return "";
}
std::string headRule(int tierHelm, const Head& h) {
  const doll::TierEnvelope& e = doll::envelopeOf(tierHelm);
  if (h.rows > e.rows) return fmt("helm %d rows over the crown (T%d: %d; %s)", h.rows, tierHelm, e.rows, h.worst.c_str());
  if (h.ornRows > e.ornRows) return fmt("crest or plume %d rows over the crown (T%d: %d)", h.ornRows, tierHelm, e.ornRows);
  if (h.cols > e.side) return fmt("helm shell %d columns past the head box (T%d: %d; %s)", h.cols, tierHelm, e.side, h.worstCol.c_str());
  if (h.colsBrim > std::max(e.side, e.brim)) return fmt("brim %d columns past the head box (T%d: %d)", h.colsBrim, tierHelm, std::max(e.side, e.brim));
  if (h.colsFlare > e.flare) return fmt("horns / wings / flare %d columns past the head box (%d)", h.colsFlare, e.flare);
  return "";
}
// a ladder line's areas (index 0 = T1 .. 5 = T6): each at kEnvelope's ratio of T1 or more and 0.05 over the tier below
std::vector<std::string> ladderRule(const std::vector<int>& area) {
  std::vector<std::string> out;
  if (area.empty() || area[0] <= 0) { out.push_back("no T1 area"); return out; }
  for (size_t t = 1; t < area.size(); t++) {
    const double ratio = (double)area[t] / area[0], need = doll::envelopeOf((int)t + 1).area / 100.0;
    const double prev = (double)area[t - 1] / area[0];
    if (ratio + 1e-9 < need) out.push_back(fmt("T%d area %.2f of T1 (%.2f)", (int)t + 1, ratio, need));
    if (ratio + 1e-9 < prev + doll::kAreaStep / 100.0) out.push_back(fmt("T%d area %.2f not 0.05 over T%d's %.2f", (int)t + 1, ratio, (int)t, prev));
  }
  return out;
}

// (envelope fix round, D57) the T3 guard's cape facing D: the rows above the legs' top where a cloak pixel lies 1 px or more
// outside every other pixel of that row (beside the arms and the coat), counted for each side
void capeSides(const Fig& f, int& left, int& right) {
  left = right = 0;
  if (!f.ok) return;
  int top = 1 << 20;
  for (int k = 0; k < f.p.w * f.p.h; k++) {
    const auto* it = f.opaque(k) ? f.it(k) : nullptr;
    if (it && it->has(Role::Legs)) top = std::min(top, k / f.p.w);
  }
  for (int y = 0; y < f.p.h && y < top; y++) {
    int c0 = 1 << 20, c1 = -1, o0 = 1 << 20, o1 = -1;
    for (int x = 0; x < f.p.w; x++) {
      const int k = y * f.p.w + x;
      if (!f.fig(k)) continue;
      const auto* it = f.it(k);
      if (it && it->has(Role::Cloak)) { c0 = std::min(c0, x); c1 = std::max(c1, x); }
      else { o0 = std::min(o0, x); o1 = std::max(o1, x); }
    }
    if (c1 < 0 || o1 < 0) continue;
    if (c0 < o0) left++;
    if (c1 > o1) right++;
  }
}
// D57's terms: eg3's area 1.20 of e1 and 1.10 of eg0 (C12f's measure), its shoulders 20, its cape beside each arm on 6 rows or
// more facing D and 1 column or more behind the body facing R
std::vector<std::string> guardRule(int a3, int a1, int a0, int shoulders, int capeL, int capeR, int back) {
  std::vector<std::string> out;
  if (a1 <= 0 || a0 <= 0) { out.push_back("no e1 or eg0 area"); return out; }
  if (a3 * 100 < a1 * doll::kGuardOverHeroT1) out.push_back(fmt("area %d %.2f of e1's %d (%.2f)", a3, (double)a3 / a1, a1, doll::kGuardOverHeroT1 / 100.0));
  if (a3 * 100 < a0 * doll::kGuardOverOwnTier) out.push_back(fmt("area %d %.2f of eg0's %d (%.2f)", a3, (double)a3 / a0, a0, doll::kGuardOverOwnTier / 100.0));
  if (shoulders < doll::kGuardShoulders) out.push_back(fmt("shoulders %d (%d)", shoulders, doll::kGuardShoulders));
  if (capeL < doll::kGuardCapeRowsD || capeR < doll::kGuardCapeRowsD)
    out.push_back(fmt("cape beside the arms facing D on %d (left) and %d (right) rows (%d each)", capeL, capeR, doll::kGuardCapeRowsD));
  if (back < doll::kGuardCapeBackR) out.push_back(fmt("cape %d columns behind facing R (%d)", back, doll::kGuardCapeBackR));
  return out;
}

// ---------------------------------------------------------------- C12f, C3b, C13, B10 by tier
struct LineSpec { const char* name; int who; const char* tags[6]; const char* mythic; };
const LineSpec kLines[] = {{"hero", 0, {"e1", "e2", "e3", "e4", "e5", "e6"}, "e6m"}, {"heroine", 1, {"e1w", "e2w", "e3w", "e4w", "e5w", "e6w"}, nullptr}};

struct Measured { int area = 0, areaD = 0, areaR = 0, shoulders = 16, cloakD = 0, cloakBack = 0, headArea = 0; Head hd, hr; };
// the figure's area as contracts 3d measures it: the opaque pixels of the composed frame with ComposeOpts::noHeld (the game's
// finish, no weapon, shield, sheath or back item); the same count doll_gallery's envelope boards print
int noHeldArea(const doll::DollRecipe& r, int facing, Clip clip, int i) {
  doll::ComposeOpts o;
  o.noHeld = true;
  o.onlyFacing = facing;
  const RigSheet sh = doll::compose(r, RigPart::Loco, &o);
  const art::FrameRect* f = sh.frame(facing, clip, 0, i);
  if (!f) return 0;
  int n = 0;
  for (int y = 0; y < f->h; y++)
    for (int x = 0; x < f->w; x++) n += (sh.atlas.get(f->x + x, f->y + y) >> 24) != 0;
  return n;
}
Measured measure(const Built& b) {
  Measured m;
  const Fig wd = probe(b, RigPart::Loco, 0, Clip::Walk, 0, 0), ir = probe(b, RigPart::Loco, 2, Clip::Idle, 0, 0),
            il = probe(b, RigPart::Loco, 3, Clip::Idle, 0, 0), id = probe(b, RigPart::Loco, 0, Clip::Idle, 0, 0);
  (void)wd;
  m.areaD = noHeldArea(b.r, 0, Clip::Walk, 0); m.areaR = noHeldArea(b.r, 2, Clip::Idle, 0); m.area = m.areaD + m.areaR;
  m.shoulders = shouldersOf(id);
  m.cloakD = cloakSideOf(id);
  m.cloakBack = std::max(cloakBackOf(ir, 2), cloakBackOf(il, 3));
  m.hd = headOf(id, b.who); m.hr = headOf(ir, b.who);
  m.headArea = helmPx(id) + helmPx(ir);
  return m;
}

int envelopeLooksGate(const std::vector<Built>& looks, Line& g, Line& c3b, Line& lad, std::map<std::string, Measured>* out) {
  for (const Built& b : looks) {
    if (!b.bad.empty()) { g.bad(b.tag + ": " + b.bad); continue; }
    const Measured m = measure(b);
    if (out) (*out)[b.tag] = m;
    const int tb = tierOf(b.L, HumanLook::TIER_BODY), th = tierOf(b.L, HumanLook::TIER_HELM), tc = tierOf(b.L, HumanLook::TIER_CLOAK);
    if (g_report)
      std::printf("  C12f %-9s T%d/%d/%d area %4d (D %3d R %3d) shoulders %2d head rows %3d orn %3d cols %d/%d/%d cloak D %d back %d headpx %d\n",
                  b.tag.c_str(), tb, th, tc, m.area, m.areaD, m.areaR, m.shoulders, m.hd.rows, m.hd.ornRows, m.hd.cols, m.hd.colsBrim,
                  m.hd.colsFlare, m.cloakD, m.cloakBack, m.headArea);
    const std::string s = shoulderRule(tb, m.shoulders);
    g.check(s.empty(), b.tag + ": " + s);
    const std::string c = cloakRule(tc, m.cloakD, m.cloakBack);
    g.check(c.empty(), b.tag + ": " + c);
    for (const Head* h : {&m.hd, &m.hr}) {
      const std::string hr = headRule(th, *h);
      c3b.check(hr.empty(), b.tag + ": " + hr);
    }
    if (tb == 1) {   // T1 inside the bare body's outline + 1 px facing D and R
      Built bare;
      bare.who = b.who;
      bare.L = b.who ? a3Woman() : a3Hero();
      bare.L.weapon = 0;
      bare.r = doll::resolve(bare.L);
      const int od = t1Outside(probe(b, RigPart::Loco, 0, Clip::Walk, 0, 0), probe(bare, RigPart::Loco, 0, Clip::Walk, 0, 0), b.who);
      const int orr = t1Outside(probe(b, RigPart::Loco, 2, Clip::Idle, 0, 0), probe(bare, RigPart::Loco, 2, Clip::Idle, 0, 0), b.who);
      g.check(od == 0 && orr == 0, b.tag + fmt(": T1 %d px (D) and %d px (R) outside the bare outline + 1", od, orr));
    }
  }
  // the ladder lines
  for (const LineSpec& ls : kLines) {
    std::vector<int> area;
    std::vector<const Built*> bs;
    for (const char* t : ls.tags) { const Built* b = find(looks, t); bs.push_back(b); area.push_back(b ? measure(*b).area : 0); }
    bool all = true;
    for (const Built* b : bs) all = all && b;
    if (!all) continue;
    std::string row;
    for (size_t i = 0; i < area.size(); i++) row += fmt(" T%d %d (%.2f)", (int)i + 1, area[i], area[0] ? (double)area[i] / area[0] : 0.0);
    lad.notes.push_back(fmt("  C12f ladder %-8s area:%s", ls.name, row.c_str()));
    for (const std::string& w : ladderRule(area)) lad.bad(std::string(ls.name) + ": " + w);
    lad.ok();
    if (ls.mythic) {
      if (const Built* mb = find(looks, ls.mythic)) {
        const Measured mm = measure(*mb), m6 = measure(*bs[5]);
        lad.check(mm.area >= m6.area, fmt("%s: the Mythic's area %d under its T6's %d", ls.name, mm.area, m6.area));
      }
    }
    std::string hrow;
    for (int t = 2; t < 6; t++) {   // T3..T6 heads: 5 % more area each, 0.10 head OD apart
      const Measured a = measure(*bs[(size_t)t - 1]), c = measure(*bs[(size_t)t]);
      const double od = doll::outlineDistance(doll::outlineOf(bs[(size_t)t - 1]->r), doll::outlineOf(bs[(size_t)t]->r)).head;
      hrow += fmt(" T%d-T%d %d->%d OD %.3f", t, t + 1, a.headArea, c.headArea, od);
      lad.check(c.headArea * 100 >= a.headArea * (100 + doll::kHeadAreaStepPct), fmt("%s: T%d head %d px not 5 %% over T%d's %d", ls.name, t + 1, c.headArea, t, a.headArea));
      lad.check(od >= doll::kHeadOdStep, fmt("%s: T%d-T%d head OD %.3f (%.2f, planned)", ls.name, t, t + 1, od, doll::kHeadOdStep));
    }
    lad.notes.push_back(fmt("  C12f ladder %-8s heads:%s", ls.name, hrow.c_str()));
    {   // (envelope fix round, D55) the T2 head budget: T2's helm pixels under T3's / 1.05 (the head step above, read the other way)
      const int h2 = measure(*bs[1]).headArea, h3 = measure(*bs[2]).headArea;
      lad.notes.push_back(fmt("  C12f ladder %-8s T2 head budget: %d px now, %d or less (T3's %d / 1.%02d)", ls.name, h2,
                              h3 * 100 / (100 + doll::kHeadAreaStepPct), h3, doll::kHeadAreaStepPct));
    }
  }
  // (envelope fix round, D57) the T3 guard: eg3 is the fair test's T3d in gear (C12f's measure: noHeld, walk D frame 0 + idle R)
  if (const Built* g3 = find(looks, "eg3")) {
    const Built* e1 = find(looks, "e1");
    const Built* g0 = find(looks, "eg0");
    if (e1 && g0 && g3->bad.empty()) {
      const Measured m3 = measure(*g3), m1 = measure(*e1), m0 = measure(*g0);
      int cl = 0, cr = 0;
      capeSides(probe(*g3, RigPart::Loco, 0, Clip::Idle, 0, 0), cl, cr);
      const int back = cloakBackOf(probe(*g3, RigPart::Loco, 2, Clip::Idle, 0, 0), 2);
      lad.notes.push_back(fmt("  C12f guard eg3: area %d (%.2f of e1 %d, %.2f of eg0 %d) shoulders %d cape D %d / %d rows, back R %d", m3.area,
                              (double)m3.area / std::max(1, m1.area), m1.area, (double)m3.area / std::max(1, m0.area), m0.area, m3.shoulders, cl, cr, back));
      const std::vector<std::string> w = guardRule(m3.area, m1.area, m0.area, m3.shoulders, cl, cr, back);
      for (const std::string& x : w) lad.bad("eg3 (D57): " + x);
      if (w.empty()) lad.ok();
    }
  }
  return g.fails + c3b.fails + lad.fails;
}

// C13's drawn motion: the idle's breathe frame (1) against the same frame without the breeze views, facing D and R
int drawnMotion(const Built& b, int facing) {
  // the compose draws the breeze views on the idle's breathe frame (frame 1) of a look flagged `breeze` (probeFrame does not)
  doll::DollRecipe still = b.r;
  still.flags.erase(std::remove(still.flags.begin(), still.flags.end(), std::string("breeze")), still.flags.end());
  doll::ComposeOpts o;
  o.onlyFacing = facing;
  const RigSheet a = doll::compose(b.r, RigPart::Loco, &o), c = doll::compose(still, RigPart::Loco, &o);
  const art::FrameRect* fa = a.frame(facing, Clip::Idle, 0, 1);
  const art::FrameRect* fc = c.frame(facing, Clip::Idle, 0, 1);
  if (!fa || !fc) return 0;
  std::map<std::pair<int, int>, uint32_t> pa, pc;
  for (int y = 0; y < fa->h; y++)
    for (int x = 0; x < fa->w; x++) { const uint32_t v = a.atlas.get(fa->x + x, fa->y + y); if (v >> 24) pa[{x - fa->px, y - fa->py}] = v; }
  for (int y = 0; y < fc->h; y++)
    for (int x = 0; x < fc->w; x++) { const uint32_t v = c.atlas.get(fc->x + x, fc->y + y); if (v >> 24) pc[{x - fc->px, y - fc->py}] = v; }
  int n = 0;
  for (const auto& kv : pa) { auto it = pc.find(kv.first); if (it == pc.end() || it->second != kv.second) n++; }
  for (const auto& kv : pc) if (!pa.count(kv.first)) n++;
  return n;
}
void motionGate(const std::vector<Built>& looks, Line& g) {
  for (const Built& b : looks) {
    if (!b.r.has("breeze")) continue;
    const int t = std::max(tierOf(b.L, HumanLook::TIER_CLOAK), tierOf(b.L, HumanLook::TIER_HELM));
    if (t < 5) continue;
    const doll::TierEnvelope& e = doll::envelopeOf(t);
    for (int fc : {0, 2}) {
      const int m = drawnMotion(b, fc);
      if (g_report) std::printf("  C13 %-9s T%d facing %d drawn motion %d px (%d-%d)\n", b.tag.c_str(), t, fc, m, e.movedMin, e.movedMax);
      g.check(m >= e.movedMin && m <= e.movedMax, b.tag + fmt(" facing %d: drawn motion %d px (T%d: %d-%d)", fc, m, t, e.movedMin, e.movedMax));
    }
  }
}
// B10 by tier on the envelope looks (the matrix's own looks are in --doll --gates' B10)
void atlasGate(const std::vector<Built>& looks, Line& g, std::map<std::string, std::vector<double>>* kb) {
  const auto& P = doll::pool::get();
  for (const Built& b : looks) {
    const HumanLook& L = b.L;
    const int tt = L.topTier();
    const bool fights = L.weapon >= 1 && L.weapon <= 8 && !L.civil;
    bool crest = false, polearm = false;
    for (int pi : b.r.parts) {
      if (pi < 0 || pi >= (int)P.parts.size()) continue;
      const std::string& nm = P.s(P.parts[(size_t)pi].name);
      if (nm.find("_crest") != std::string::npos || nm.find("_plume") != std::string::npos || nm.find("helm/crest/") != std::string::npos) crest = true;
      if (nm.find("held/weapons/spear") != std::string::npos || nm.find("held/weapons/glaive") != std::string::npos ||
          nm.find("held/weapons/halberd") != std::string::npos) polearm = true;
    }
    struct B { RigPart p; int atlas; const char* n; };
    for (const B& x : {B{RigPart::Loco, 0, "LOCO"}, B{RigPart::Run, 1, "RUN"}, B{RigPart::Drawn, 2, "DRAWN"}, B{RigPart::Action, 3, "ACTION"},
                       B{RigPart::Death, 4, "DEATH"}}) {
      if ((x.p == RigPart::Drawn || x.p == RigPart::Action) && !fights) continue;
      const RigSheet sh = doll::compose(b.r, x.p);
      double budget = doll::envelopeAtlasKB(tt, x.atlas);
      if (x.p == RigPart::Run && crest) budget += 4;
      if (x.p == RigPart::Loco && polearm) budget += 8;
      if (x.p == RigPart::Run && polearm) budget += 14;
      if (x.p == RigPart::Death && polearm) budget += 4;
      if ((x.p == RigPart::Loco || x.p == RigPart::Run) && L.forgeShape) budget *= 1.14;
      else if ((x.p == RigPart::Loco || x.p == RigPart::Run) && L.forge[HumanLook::TIER_BODY]) budget *= 1.03;
      const double k = sh.empty() ? -1 : sh.bytes() / 1024.0;
      if (kb) (*kb)[b.tag].push_back(k);
      if (g_report) std::printf("  B10 env %-9s T%d %-6s %6.1f KB (%.0f)\n", b.tag.c_str(), tt, x.n, k, budget);
      g.check(k >= 0 && k <= budget, b.tag + fmt(" %s %.1f KB (T%d: %.0f)", x.n, k, tt, budget));
    }
    // (integrator, wave 2 int3, D70) the POSTURE atlas measured for the report (the largest posture, Sleep aside), so kEnvelope's
    // T5-T6 POSTURE is written from e6 like ACTION and DEATH; report only, no check here
    if (g_report && tt >= 5) {
      double pk = 0;
      for (int p = 1; p < (int)art::Posture::COUNT; p++)
        if ((art::Posture)p != art::Posture::Sleep) pk = std::max(pk, doll::composePosture(b.r, (art::Posture)p, 0).bytes() / 1024.0);
      std::printf("  B10 env %-9s T%d %-6s %6.1f KB (%.0f)\n", b.tag.c_str(), tt, "POSTURE", pk, (double)doll::envelopeAtlasKB(tt, 5));
    }
  }
}

std::string envelopeLooksText() { return kEnvelopeLooks; }
std::vector<Built> buildLooks(const std::string& text) {
  std::vector<Built> v;
  for (const Look& l : parseLooks(text)) v.push_back(build(l));
  return v;
}

int envelopeSelftests() {
  int caught = 0, want = 0;
  auto expect = [&](bool failed, const char* what) {
    want++;
    if (failed) caught++;
    else std::printf("    C12f selftest NOT caught: %s\n", what);
  };
  // today's e6 smaller than e5 (the study: T5 674, T6 649 px)
  expect(!ladderRule({463, 510, 539, 600, 674, 649}).empty(), "a T6 smaller than its T5");
  // a T2 wearing large pauldrons (a T4 look measured under T2's caps)
  {
    gear::installLadderMacros();
    const Built b = build(Look{"st_t2", 0, "hero=1,kit=heartland:4:7,palette=look"});
    const int sh = shouldersOf(probe(b, RigPart::Loco, 0, Clip::Idle, 0, 0));
    expect(!shoulderRule(2, std::max(sh, 22)).empty(), "a T2 with large pauldrons");
  }
  // a T5 cloak 10 px behind
  expect(!cloakRule(5, 0, 10).empty(), "a T5 cloak 10 px behind");
  // a T1 hood 4 rows over the crown
  { Head h; h.rows = 4; expect(!headRule(1, h).empty(), "a T1 hood 4 rows over the crown"); }
  // (envelope fix round, D57) the T3 guard's terms, each tripped alone (round 3's eg3: 1.05 of eg0, shoulders 18, no cape),
  // and a good guard passes
  expect(!guardRule(950, 808, 800, 20, 8, 8, 2).empty(), "a T3 guard at 1.18 of e1 (1.20)");
  expect(!guardRule(1000, 808, 950, 20, 8, 8, 2).empty(), "a T3 guard at 1.05 of eg0 (round 3; 1.10)");
  expect(!guardRule(1100, 808, 859, 18, 8, 8, 2).empty(), "a T3 guard with shoulders 18 (round 3)");
  expect(!guardRule(1100, 808, 859, 20, 0, 0, 2).empty(), "a T3 guard whose cape hides facing D (round 3)");
  expect(!guardRule(1100, 808, 859, 20, 8, 8, 0).empty(), "a T3 guard whose cape hides facing R (round 3)");
  expect(guardRule(1100, 808, 859, 20, 8, 8, 2).empty(), "a good T3 guard FAILED its terms");
  std::printf("  C12f selftests: %d of %d caught\n", caught, want);
  return want - caught;
}

}  // namespace dollenv

// ---------------------------------------------------------------- C19: the A defects (PLAN "The envelope" A table)
namespace dollenv {

bool gearPx(const Fig& f, int k) {
  if (!f.fig(k)) return false;
  const doll::Slot s = f.slot(k);
  if (s == doll::Slot::Skin || s == doll::Slot::Hair || s == doll::Slot::Eyes || s == doll::Slot::Face || s == doll::Slot::Ink) return false;
  return f.step(k) != 0;
}
bool metalPx(const Fig& f, int k) { return gearPx(f, k) && (f.slot(k) == doll::Slot::Metal || f.slot(k) == doll::Slot::Metal2); }
// the connected groups (4-neighbour) of pixels a predicate keeps; each group's size
std::vector<std::vector<int>> groups(const Fig& f, const std::function<bool(int)>& keep, bool eight = false) {
  std::vector<std::vector<int>> out;
  std::vector<char> seen((size_t)f.p.w * f.p.h, 0);
  for (int k = 0; k < f.p.w * f.p.h; k++) {
    if (seen[(size_t)k] || !keep(k)) continue;
    std::vector<int> g, st{k};
    seen[(size_t)k] = 1;
    while (!st.empty()) {
      const int q = st.back();
      st.pop_back();
      g.push_back(q);
      const int x = q % f.p.w, y = q / f.p.w;
      for (int d = 0; d < (eight ? 8 : 4); d++) {
        static const int kDx[8] = {1, -1, 0, 0, 1, 1, -1, -1}, kDy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
        const int k2 = f.p.at(x + kDx[d], y + kDy[d]);
        if (k2 >= 0 && !seen[(size_t)k2] && keep(k2)) { seen[(size_t)k2] = 1; st.push_back(k2); }
      }
    }
    out.push_back(g);
  }
  return out;
}
// C16n's 1 px features (a gear pixel above or below all of its 3-4 same-slot neighbours; glints counted when asked)
int onePx(const Fig& f, const std::function<bool(int)>& in, bool glints, int* total = nullptr) {
  int feat = 0, n = 0;
  for (int y = 1; y + 1 < f.p.h; y++)
    for (int x = 1; x + 1 < f.p.w; x++) {
      const int k = y * f.p.w + x;
      if (!gearPx(f, k) || !in(k)) continue;
      n++;
      const doll::Slot s = f.slot(k);
      const int st = f.step(k);
      if (st == 4 && !glints) continue;
      int above = 0, below = 0, m = 0;
      for (int k2 : {k - 1, k + 1, k - f.p.w, k + f.p.w}) {
        if (!f.opaque(k2) || f.slot(k2) != s || f.step(k2) == 0) continue;
        m++;
        if (st > f.step(k2)) above++;
        if (st < f.step(k2)) below++;
      }
      if (m >= 3 && (above == m || below == m)) feat++;
    }
  if (total) *total = n;
  return feat;
}
bool itemHas(const Fig& f, int k, Role r) { const auto* it = f.it(k); return it && it->has(r); }
bool partHas(const Fig& f, int k, std::initializer_list<const char*> ws) {
  const auto* it = f.it(k);
  if (!it) return false;
  for (const char* w : ws) if (it->part.find(w) != std::string::npos) return true;
  return false;
}
// a lit plane of 4 px or more (metal steps 3-4, connected) on the pixels `in` keeps
int litPlane(const Fig& f, const std::function<bool(int)>& in) {
  int best = 0;
  for (const auto& g : groups(f, [&](int k) { return metalPx(f, k) && f.step(k) >= 3 && in(k); })) best = std::max(best, (int)g.size());
  return best;
}
// the glass tell: metal step-3 pixels on the 1-2 px inside the step-0 outline at the lower right of the pieces `in` keeps
int glassRim(const Fig& f, const std::function<bool(int)>& in) {
  int n = 0;
  for (int k = 0; k < f.p.w * f.p.h; k++) {
    if (!metalPx(f, k) || f.step(k) != 3 || !in(k)) continue;
    const int x = k % f.p.w, y = k / f.p.w;
    bool edge = false;
    for (int d = 1; d <= 2 && !edge; d++)
      for (int k2 : {f.p.at(x + d, y), f.p.at(x, y + d), f.p.at(x + d, y + d)})
        if (k2 < 0 || !f.opaque(k2) || f.step(k2) == 0) edge = true;
    // ... with a darker core (steps 1-2) up-left of it
    const int kc = f.p.at(x - 1, y - 1), kl = f.p.at(x - 1, y);
    const bool core = (kc >= 0 && metalPx(f, kc) && f.step(kc) <= 2) || (kl >= 0 && metalPx(f, kl) && f.step(kl) <= 2);
    if (edge && core) n++;
  }
  return n;
}
struct Share { int gear = 0, metal = 0, lit = 0, layer = 0; };
Share shareOf(const Fig& f) {
  Share s;
  for (int k = 0; k < f.p.w * f.p.h; k++) {
    if (!gearPx(f, k)) continue;
    s.gear++;
    if (metalPx(f, k)) { s.metal++; if (f.step(k) >= 3) s.lit++; }
    if (partHas(f, k, {"tabard", "sash", "halfcape", "scarf", "mantle", "pelt", "fur", "feather"})) s.layer++;
  }
  return s;
}
std::string d52(const doll::DollRecipe& r) {   // the antique rule on the Metal ramp
  const uint32_t* m = r.ramps.c[(int)doll::Slot::Metal];
  const double l1 = luma(m[1]), l2 = luma(m[2]), l3 = luma(m[3]);
  if (l2 < 95 || l2 > 125 || l1 > 45 || l3 < 175) return fmt("D52 antique: step 1 %.0f (45 or less), step 2 %.0f (95-125), step 3 %.0f (175 or more)", l1, l2, l3);
  return "";
}
std::string warmTerm(const doll::DollRecipe& r) {   // C17's warm term: the lit plane 15 deg toward gold, steps 2-3 out of the skin box
  const uint32_t* m = r.ramps.c[(int)doll::Slot::Metal];
  double h2, c2, h3, c3;
  hueChroma(m[2], h2, c2); hueChroma(m[3], h3, c3);
  for (int s : {2, 3}) {
    double h, c;
    hueChroma(m[s], h, c);
    const double l = luma(m[s]);
    if (h >= 5 && h <= 45 && c >= 25 && c <= 60 && l >= 110 && l <= 210) return fmt("metal step %d in the skin box (hue %.0f chroma %.0f luma %.0f)", s, h, c, l);
  }
  if (hueDist(h3, 48) > hueDist(h2, 48) - 15 && hueDist(h3, 48) > 10) return fmt("the lit plane hue %.0f not 15 deg toward gold from %.0f", h3, h2);
  return "";
}
// (envelope r3; D56, the round-3 scope the judge accepted, recorded in PLAN D56 and contracts 3d; thresholds unchanged)
// C19's torso terms read the torso as drawn: the armour and its chest motif (armour/motif/: the keel, the bib,
// the plates laid on the torso), on the frame without the held weapon, shield, sheath or Fx (ComposeOpts::noHeld's
// recipe, C12f's figure): facing D a shield covers the chest, and the term asks after the torso's own glass, not the shield's
bool torsoPx(const Fig& f, int k) {
  const auto* it = f.it(k);
  return it && (it->has(Role::Armour) || it->part.find("armour/motif/") != std::string::npos);
}
doll::DollRecipe noHeldRecipe(const doll::DollRecipe& r) {
  doll::DollRecipe bare = r;
  const doll::pool::Pool& P = doll::pool::get();
  bare.parts.erase(std::remove_if(bare.parts.begin(), bare.parts.end(), [&](int pi) {
                     for (const auto& rr : P.parts[(size_t)pi].roles)
                       if (rr.cat == (uint8_t)Role::Held || rr.cat == (uint8_t)Role::Shield || rr.cat == (uint8_t)Role::Sheath || rr.cat == (uint8_t)Role::Fx)
                         return true;
                     return false;
                   }),
                   bare.parts.end());
  return bare;
}
// (envelope fix round, D58) the torso box's slots (armour + motif, noHeld): its Metal / Metal2 pixels and its cloth slots
// (Cloth1-3, Linen, Accent), step 0 (the outline) excluded; ENV_DEBUG prints each slot's count and the parts painting it
struct TorsoMix { int n = 0, metal = 0, cloth = 0; };
TorsoMix torsoMix(const Fig& f, const char* id = nullptr) {
  TorsoMix t;
  std::map<std::string, int> by;
  for (int k = 0; k < f.p.w * f.p.h; k++) {
    if (!f.fig(k) || !torsoPx(f, k) || f.step(k) == 0) continue;
    const doll::Slot s = f.slot(k);
    if (s == doll::Slot::Skin || s == doll::Slot::Hair || s == doll::Slot::Eyes || s == doll::Slot::Face || s == doll::Slot::Ink) continue;
    t.n++;
    if (s == doll::Slot::Metal || s == doll::Slot::Metal2) t.metal++;
    if (s == doll::Slot::Cloth1 || s == doll::Slot::Cloth2 || s == doll::Slot::Cloth3 || s == doll::Slot::Linen || s == doll::Slot::Accent) t.cloth++;
    if (id && std::getenv("ENV_DEBUG")) by[fmt("slot %2d %s", (int)s, f.it(k) ? f.it(k)->part.c_str() : "-")]++;
  }
  if (id && std::getenv("ENV_DEBUG"))
    for (const auto& kv : by) std::printf("      torso %s %s: %d\n", id, kv.first.c_str(), kv.second);
  return t;
}
struct AFrames { Fig d, r, w1; };
AFrames aframes(const Built& b) {
  return AFrames{probe(b, RigPart::Loco, 0, Clip::Idle, 0, 0), probe(b, RigPart::Loco, 2, Clip::Idle, 0, 0), probe(b, RigPart::Loco, 2, Clip::Walk, 0, 1)};
}
// A1-A3: brass that reads as metal
void rowBrass(Line& g, const std::vector<Built>& looks, const char* tag, int item) {
  const Built* b = find(looks, tag);
  if (!b) { g.bad(fmt("A%d %s: no look", item, tag)); return; }
  const AFrames F = aframes(*b);
  const std::string id = fmt("A%d %s", item, tag);
  for (const Fig* f : {&F.d, &F.r, &F.w1}) {
    const Share s = shareOf(*f);
    g.check(s.gear && s.metal * 100 >= s.gear * 35, id + fmt(": metal %d of %d gear px (35 %%, planned)", s.metal, s.gear));
    g.check(s.metal && s.lit * 100 >= s.metal * 22, id + fmt(": lit steps 3-4 %d of %d metal px (22 %%, planned)", s.lit, s.metal));
    g.check(s.gear && s.layer * 100 <= s.gear * 25, id + fmt(": over-layer, mantle and pelt %d of %d gear px (25 %%)", s.layer, s.gear));
  }
  g.check(litPlane(F.d, [&](int k) { return itemHas(F.d, k, Role::Helm) || itemHas(F.d, k, Role::Headwear); }) >= 4, id + ": no 4 px lit plane on the helm");
  g.check(litPlane(F.d, [&](int k) { return itemHas(F.d, k, Role::Armour) || itemHas(F.d, k, Role::Overlay); }) >= 4, id + ": no 4 px lit plane on the torso or skirt");
  for (int side : {1, 2}) {
    bool any = false;
    for (const auto& it : F.d.p.items) any |= it.has(Role::Pauldron) && it.side == side;
    if (!any) continue;
    g.check(litPlane(F.d, [&](int k) { const auto* it = F.d.it(k); return it && it->has(Role::Pauldron) && it->side == side; }) >= 4,
            id + fmt(": no 4 px lit plane on the %s shoulder", side == 1 ? "right" : "left"));
  }
  const std::string w = warmTerm(b->r), a = d52(b->r);
  g.check(w.empty(), id + ": " + w);
  g.check(a.empty(), id + ": " + a);
  // facing R nothing but a masterwork's cape more than 3 px behind the torso's back line
  {
    // the torso's back line: its armour's back edge, and never in front of the bare body's own back (a shoulder piece that
    // hides the whole torso facing R, JAONBRASS's scale.4, leaves only the chest's front edge visible: that is not the back)
    int back = 1 << 20;
    for (int k = 0; k < F.r.p.w * F.r.p.h; k++) if (F.r.opaque(k) && itemHas(F.r, k, Role::Armour)) back = std::min(back, k % F.r.p.w);
    {
      Built bare;
      bare.who = b->who;
      bare.L = b->who ? a3Woman() : a3Hero();
      bare.L.weapon = 0;
      bare.r = doll::resolve(bare.L);
      const Fig fb = probe(bare, RigPart::Loco, 2, Clip::Idle, 0, 0);
      int bb = 1 << 20;
      for (int k = 0; fb.ok && k < fb.p.w * fb.p.h; k++) {
        const auto* it = fb.fig(k) ? fb.it(k) : nullptr;
        if (it && it->has(Role::Garment)) bb = std::min(bb, k % fb.p.w - fb.p.px);
      }
      if (bb != (1 << 20) && back != (1 << 20)) back = std::min(back, bb + F.r.p.px);
    }
    if (std::getenv("ENV_DEBUG")) std::printf("      backline %s x %d\n", id.c_str(), back - F.r.p.px);
    int worst = 0;
    for (int k = 0; k < F.r.p.w * F.r.p.h; k++) {
      if (!F.r.fig(k) || back == (1 << 20)) continue;
      const auto* it = F.r.it(k);
      if (it && it->has(Role::Cloak) && b->L.alloyRank == 3) continue;
      // (the body's back: the head, its hat's brim and the hair are the head's own outline, C3b's)
      if (it && (it->has(Role::Helm) || it->has(Role::Headwear) || it->has(Role::Head) || it->has(Role::Hair))) continue;
      worst = std::max(worst, back - k % F.r.p.w);
      if (back - k % F.r.p.w > 3 && std::getenv("ENV_DEBUG"))
        std::printf("      behind %s (%d,%d) %d px part %s\n", id.c_str(), k % F.r.p.w - F.r.p.px, k / F.r.p.w - F.r.p.py, back - k % F.r.p.w, it ? it->part.c_str() : "-");
    }
    g.check(worst <= 3, id + fmt(": %d px behind the torso's back line facing R (3)", worst));
  }
  if (item == 3) {   // (envelope fix round, D58) A3 ENKHOBRASS wears brass: its torso box Metal 50 % or more, cloth 30 % or less
    const doll::DollRecipe bare = noHeldRecipe(b->r);
    for (int fc : {0, 2}) {
      const Fig nf = probeR(bare, RigPart::Loco, fc, Clip::Idle, 0, 0);
      const TorsoMix t = torsoMix(nf, id.c_str());
      if (g_report) std::printf("  C19 %s torso facing %d: metal %d cloth %d of %d\n", id.c_str(), fc, t.metal, t.cloth, t.n);
      g.check(t.n && t.metal * 100 >= t.n * 50, id + fmt(": the torso's brass %d of %d px facing %s (50 %%; armour and motif, no held items)", t.metal, t.n, fc ? "R" : "D"));
      g.check(t.n && t.cloth * 100 <= t.n * 30, id + fmt(": the torso's cloth %d of %d px facing %s (30 %% or less)", t.cloth, t.n, fc ? "R" : "D"));
    }
  }
  if (item == 2 || item == 3) {   // the helm's top-left: 6 px or more of steps 3-4
    int x0 = 1 << 20, y0 = 1 << 20, x1 = -1, y1 = -1;
    for (int k = 0; k < F.d.p.w * F.d.p.h; k++)
      if (F.d.opaque(k) && (itemHas(F.d, k, Role::Helm) || itemHas(F.d, k, Role::Headwear))) {
        x0 = std::min(x0, k % F.d.p.w); x1 = std::max(x1, k % F.d.p.w); y0 = std::min(y0, k / F.d.p.w); y1 = std::max(y1, k / F.d.p.w);
      }
    int lit = 0;
    for (int k = 0; k < F.d.p.w * F.d.p.h; k++) {
      const int x = k % F.d.p.w, y = k / F.d.p.w;
      if (x > (x0 + x1) / 2 || y > (y0 + y1) / 2) continue;
      if (metalPx(F.d, k) && F.d.step(k) >= 3 && (itemHas(F.d, k, Role::Helm) || itemHas(F.d, k, Role::Headwear))) lit++;
    }
    g.check(lit >= 6, id + fmt(": the helm's top-left %d px of steps 3-4 (6)", lit));
  }
}
// (debug) ENV_HEADDUMP=<tag>:<facing>: the look's idle frame round the head as slot.step per pixel (H = a helm pixel)
void headDump(const Built& b) {
  const char* t = std::getenv("ENV_HEADDUMP");
  if (!t) return;
  const std::string ts = t;
  const size_t c = ts.find(':');
  if (ts.substr(0, c) != b.tag) return;
  const int fc = c == std::string::npos ? 0 : std::atoi(ts.c_str() + c + 1);
  const Fig d = probe(b, RigPart::Loco, fc, Clip::Idle, 0, 0);
  for (int y = d.p.headY - 4; y < d.p.headY + 22; y++) {
    std::printf("      %3d ", y - d.p.headY);
    for (int x = d.p.headX - 6; x < d.p.headX + 22; x++) {
      const int k = d.p.at(x, y);
      if (k < 0 || !d.opaque(k)) { std::printf("  . "); continue; }
      std::printf("%c%02d%d", itemHas(d, k, Role::Helm) ? 'H' : ' ', (int)d.slot(k), d.step(k));
    }
    std::printf("\n");
  }
}
// A4: obsidian, black glass
void rowObsidian(Line& g, const Built& b, int item) {
  headDump(b);
  const AFrames F = aframes(b);
  const std::string id = fmt("A%d %s", item, b.tag.c_str());
  const doll::DollRecipe bare = noHeldRecipe(b.r);
  const AFrames N{probeR(bare, RigPart::Loco, 0, Clip::Idle, 0, 0), probeR(bare, RigPart::Loco, 2, Clip::Idle, 0, 0), probeR(bare, RigPart::Loco, 2, Clip::Walk, 0, 1)};
  for (const Fig* f : {&F.d, &F.r, &F.w1}) {
    const Fig& nf = f == &F.d ? N.d : f == &F.r ? N.r : N.w1;
    const int hx = f->p.headX, hy = f->p.headY;
    auto inHead = [&](int k) { const int x = k % f->p.w, y = k / f->p.w; return x >= hx && x < hx + 16 && y >= hy && y < hy + 14; };
    g.check(onePx(*f, inHead, true) <= 2, id + fmt(": %d 1 px features in the head box (2)", onePx(*f, inHead, true)));
    int tot = 0;
    const int feat = onePx(*f, [&](int k) { return !inHead(k); }, false, &tot);
    g.check(tot == 0 || feat * 100 <= tot * 3, id + fmt(": C16n %d of %d body px (3 %%)", feat, tot));
    int g4 = 0, off = 0, bad = 0;
    for (const auto& gr : groups(*f, [&](int k) { return metalPx(*f, k) && f->step(k) == 4; }, true)) {
      g4++;
      if ((int)gr.size() < 2 || (int)gr.size() > 6) bad++;
      if (!itemHas(*f, gr[0], Role::Helm)) off++;
    }
    g.check(g4 >= 2 && g4 <= 4 && bad == 0 && off >= 1, id + fmt(": %d speculars (2-4 of 2-6 px; %d off size; %d off the helm, 1 or more)", g4, bad, off));
    const int rim = glassRim(nf, [&](int k) { return torsoPx(nf, k); });
    g.check(rim >= 3, id + fmt(": the glass rim %d px on the torso's lower-right inside edge (3; armour and motif, no held items)", rim));
  }
  // C17 dark and glass
  const uint32_t* m = b.r.ramps.c[(int)doll::Slot::Metal];
  double h3, c3;
  hueChroma(m[3], h3, c3);
  g.check(luma(m[2]) <= 50, id + fmt(": the body luma %.0f (50 or less)", luma(m[2])));
  g.check(h3 >= 200 && h3 <= 290, id + fmt(": the lit hue %.0f (200-290)", h3));
}
// A5: the Mythic over its T6
void rowMythic(Line& g, const Built& b, const Built* t6, int item) {
  const std::string id = fmt("A%d %s", item, b.tag.c_str());
  for (int fc : {0, 2, 3}) {
    const Fig f = probe(b, RigPart::Loco, fc, Clip::Idle, 0, 1);
    if (!f.ok) continue;
    int bad = 0, lone = 0;
    for (int k = 0; k < f.p.w * f.p.h; k++) {
      if (!f.opaque(k)) continue;
      const int x = k % f.p.w, y = k / f.p.w;
      const bool face = x >= f.p.headX + 2 && x <= f.p.headX + 13 && y >= f.p.headY + 4 && y <= f.p.headY + 13;   // W7's face + 1
      if (f.slot(k) == doll::Slot::Glow) {
        if (face) bad++;
        int n = 0;
        for (int k2 : {k - 1, k + 1, k - f.p.w, k + f.p.w}) n += k2 >= 0 && k2 < f.p.w * f.p.h && f.opaque(k2) && f.slot(k2) == doll::Slot::Glow;
        if (!n) lone++;
        if (!n && std::getenv("ENV_DEBUG")) {
          const auto* it = f.it(k);
          std::printf("      glow %s f%d (%d,%d) step %d part %s layer %d\n", id.c_str(), fc, x - f.p.px, y - f.p.py, f.step(k), it ? it->part.c_str() : "-", it ? (int)it->layer : -1);
        }
      }
    }
    g.check(bad == 0, id + fmt(" facing %d: %d glow px in the face box", fc, bad));
    g.check(lone == 0, id + fmt(" facing %d: %d lone glow px", fc, lone));
    int tot = 0;
    const int feat = onePx(f, [&](int k) { return itemHas(f, k, Role::Helm) || itemHas(f, k, Role::Armour); }, false, &tot);
    g.check(tot == 0 || feat * 100 <= tot * 3, id + fmt(" facing %d: C16n %d of %d (3 %%)", fc, feat, tot));
  }
  if (t6) {
    const Measured a = measure(b), c = measure(*t6);
    g.check(a.area >= c.area, id + fmt(": area %d under its T6's %d", a.area, c.area));
    g.check(a.headArea >= c.headArea, id + fmt(": head %d px under its T6's %d", a.headArea, c.headArea));
  }
  const Fig d = probe(b, RigPart::Loco, 0, Clip::Idle, 0, 0);
  std::vector<double> ls;
  for (int k = 0; k < d.p.w * d.p.h; k++) if (d.opaque(k) && itemHas(d, k, Role::Helm) && d.step(k) > 0) ls.push_back(luma(d.colour(k)));
  std::sort(ls.begin(), ls.end());
  const double range = ls.size() > 4 ? ls[ls.size() * 9 / 10] - ls[ls.size() / 10] : 0;
  g.check(range >= 70, id + fmt(": the helm's luma p90 - p10 %.0f (70)", range));
}
// A6: starmetal, iridescent crystal
void rowStar(Line& g, const Built& b, int item) {
  const std::string id = fmt("A%d %s", item, b.tag.c_str());
  const Fig d = probe(b, RigPart::Loco, 0, Clip::Idle, 0, 0);
  int m1 = 0, m2 = 0, metal = 0;
  std::vector<double> ls;
  for (int k = 0; k < d.p.w * d.p.h; k++) {
    if (!metalPx(d, k)) continue;
    metal++;
    (d.slot(k) == doll::Slot::Metal ? m1 : m2)++;
    ls.push_back(luma(d.colour(k)));
  }
  double h1, c1, h2, c2;
  hueChroma(b.r.ramps.c[(int)doll::Slot::Metal][3], h1, c1);
  hueChroma(b.r.ramps.c[(int)doll::Slot::Metal2][3], h2, c2);
  g.check(hueDist(h1, h2) >= 40, id + fmt(": the two lit hues %.0f and %.0f (40 deg apart)", h1, h2));
  g.check(metal && m1 * 100 >= metal * 20 && m2 * 100 >= metal * 20, id + fmt(": Metal %d / Metal2 %d of %d (20 %% each)", m1, m2, metal));
  std::sort(ls.begin(), ls.end());
  const double range = ls.size() > 4 ? ls[ls.size() * 9 / 10] - ls[ls.size() / 10] : 0;
  g.check(range >= 60, id + fmt(": the metal's luma p90 - p10 %.0f (60)", range));
  const uint32_t* m = b.r.ramps.c[(int)doll::Slot::Metal];
  g.check(luma(m[1]) <= 0.45 * luma(m[3]), id + fmt(": step 1 luma %.0f over 45 %% of step 3's %.0f", luma(m[1]), luma(m[3])));
  int planes = 0;
  for (const auto& gr : groups(d, [&](int k) { return metalPx(d, k) && d.step(k) >= 3 && itemHas(d, k, Role::Helm); })) planes += gr.size() >= 4;
  g.check(planes >= 2, id + fmt(": %d lit planes of 4 px on the helm (2)", planes));
}
// A7: glass grown round silver
void rowGlass(Line& g, const Built& b, int item) {
  const std::string id = fmt("A%d %s", item, b.tag.c_str());
  const Fig d = probe(b, RigPart::Loco, 0, Clip::Idle, 0, 0);
  if (std::getenv("ENV_DEBUG")) std::printf("      %s glass flag %d metalKind %d\n", id.c_str(), (int)b.r.has("glass"), (int)b.L.metalKind);
  {
    const int rim = glassRim(d, [&](int k) { return itemHas(d, k, Role::Helm); });
    g.check(rim >= 3, id + fmt(": the glass tell %d px on the helm (3)", rim));
    const doll::DollRecipe bare = noHeldRecipe(b.r);
    const Fig nd = probeR(bare, RigPart::Loco, 0, Clip::Idle, 0, 0);
    const int rt = glassRim(nd, [&](int k) { return torsoPx(nd, k); });
    g.check(rt >= 3, id + fmt(": the glass tell %d px on the torso (3; armour and motif, no held items)", rt));
  }
  for (int side : {1, 2}) {
    bool any = false;
    for (const auto& it : d.p.items) any |= it.has(Role::Pauldron) && it.side == side;
    if (!any) continue;
    const int rim = glassRim(d, [&](int k) { const auto* it = d.it(k); return it && it->has(Role::Pauldron) && it->side == side; });
    g.check(rim >= 3, id + fmt(": the glass tell %d px on the %s shoulder (3)", rim, side == 1 ? "right" : "left"));
  }
  int sp = 0, bad = 0;
  for (const auto& gr : groups(d, [&](int k) { return metalPx(d, k) && d.step(k) == 4; }, true)) { sp++; bad += gr.size() < 2 || gr.size() > 6; }
  g.check(sp >= 2 && sp <= 4 && bad == 0, id + fmt(": %d speculars (2-4 of 2-6 px; %d off size)", sp, bad));
  int glow = 0;
  for (int k = 0; k < d.p.w * d.p.h; k++) glow += d.opaque(k) && d.slot(k) == doll::Slot::Glow;
  g.check(glow >= 8, id + fmt(": %d glow px on the seams (8)", glow));
}
// A8: no glint noise on heavy plate, every LOCO and RUN frame
void rowPlate(Line& g, const Built& b, int item) {
  const std::string id = fmt("A%d %s", item, b.tag.c_str());
  struct C { RigPart p; Clip c; int n; };
  for (const C& c : {C{RigPart::Loco, Clip::Idle, 2}, C{RigPart::Loco, Clip::Walk, 6}, C{RigPart::Run, Clip::Walk, 6}})
    for (int fc = 0; fc < 4; fc++)
      for (int i = 0; i < c.n; i++) {
        const Fig f = probe(b, c.p, fc, c.c, 0, i);
        if (!f.ok) continue;
        const std::string at = fmt(" %s %s f%d facing %d", c.p == RigPart::Run ? "run" : "loco", c.c == Clip::Idle ? "idle" : "walk", i, fc);
        int tot = 0;
        const int feat = onePx(f, [](int) { return true; }, false, &tot);
        g.check(tot == 0 || feat * 100 <= tot * 3, id + at + fmt(": C16n %d of %d (3 %%)", feat, tot));
        int g4 = 0;
        for (const auto& gr : groups(f, [&](int k) { return metalPx(f, k) && f.step(k) == 4; }, true)) {
          g4++;
          if (std::getenv("ENV_DEBUG_G")) {
            const auto* it = f.it(gr[0]);
            std::printf("      glint %s%s (%d,%d) %zu px slot %d part %s\n", b.tag.c_str(), at.c_str(), gr[0] % f.p.w - f.p.px, gr[0] / f.p.w - f.p.py, gr.size(), (int)f.slot(gr[0]), it ? it->part.c_str() : "-");
          }
        }
        g.check(g4 <= 3, id + at + fmt(": %d step-4 groups (3)", g4));
        // a lone 1 px run of steps 3-4 on a plate piece (a broken dash): a lit pixel with no lit 4-neighbour
        int lone = 0;
        for (int k = 0; k < f.p.w * f.p.h; k++) {
          if (!metalPx(f, k) || f.step(k) < 3 || f.step(k) == 4) continue;
          int n = 0;
          const int x = k % f.p.w, y = k / f.p.w;
          for (int k2 : {f.p.at(x - 1, y), f.p.at(x + 1, y), f.p.at(x, y - 1), f.p.at(x, y + 1)}) n += k2 >= 0 && metalPx(f, k2) && f.step(k2) >= 3;
          lone += n == 0;
          if (n == 0 && std::getenv("ENV_DEBUG")) {
            const auto* it = f.it(k);
            std::printf("      lone %s%s (%d,%d) slot %d step %d part %s\n", b.tag.c_str(), at.c_str(), x - f.p.px, y - f.p.py, (int)f.slot(k), f.step(k), it ? it->part.c_str() : "-");
            for (int k2 : {f.p.at(x - 1, y), f.p.at(x + 1, y), f.p.at(x, y - 1), f.p.at(x, y + 1)})
              if (k2 >= 0 && f.opaque(k2)) {
                const auto* i2 = f.it(k2);
                std::printf("        nb slot %d step %d fig %d layer %d held %d part %s\n", (int)f.slot(k2), f.step(k2), (int)f.fig(k2), i2 ? (int)i2->layer : -1, i2 ? (int)i2->has(Role::Held) : -1, i2 ? i2->part.c_str() : "-");
              }
          }
        }
        g.check(lone == 0, id + at + fmt(": %d lone 1 px lit pixels", lone));
      }
}
// ---- (envelope fix round, FORGE-MATERIAL draft for FORGE-DOLL; P2C_CONTRACTS 3d "Round 4 adds", PLAN D59-D62) the round-4 C19
// terms as MATERIAL measures them on its preview pool. DOLL owns the gate; these are the reference implementation.
void rchk(Line& g, bool ok, const std::string& t) { if (g_report) std::printf("    %s %s\n", ok ? "pass" : "FAIL", t.c_str()); g.check(ok, t); }
// the largest 4-connected run of the pixels `keep` holds
int bestRun(const Fig& f, const std::function<bool(int)>& keep) {
  int best = 0;
  for (const auto& g : groups(f, keep)) best = std::max(best, (int)g.size());
  return best;
}
// glassRim's pixel test (step 3 on the 1-2 px inside the outline at the lower right, a darker core up-left of it)
bool rimPx(const Fig& f, int k) {
  if (!metalPx(f, k) || f.step(k) != 3) return false;
  const int x = k % f.p.w, y = k / f.p.w;
  bool edge = false;
  for (int d = 1; d <= 2 && !edge; d++)
    for (int k2 : {f.p.at(x + d, y), f.p.at(x, y + d), f.p.at(x + d, y + d)})
      if (k2 < 0 || !f.opaque(k2) || f.step(k2) == 0) edge = true;
  const int kc = f.p.at(x - 1, y - 1), kl = f.p.at(x - 1, y);
  const bool core = (kc >= 0 && metalPx(f, kc) && f.step(kc) <= 2) || (kl >= 0 && metalPx(f, kl) && f.step(kl) <= 2);
  return edge && core;
}
int rimRun(const Fig& f, const std::function<bool(int)>& in) { return bestRun(f, [&](int k) { return in(k) && rimPx(f, k); }); }
bool faceBox(const Fig& f, int k) {   // W7's face box + 1 px
  const int x = k % f.p.w, y = k / f.p.w;
  return x >= f.p.headX + 2 && x <= f.p.headX + 13 && y >= f.p.headY + 4 && y <= f.p.headY + 13;
}
void r4Obsidian(Line& g, const Built& b, int item) {
  const std::string id = fmt("A%d %s r4", item, b.tag.c_str());
  const doll::DollRecipe bare = noHeldRecipe(b.r);
  for (int fc : {0, 2}) {
    const Fig n = probeR(bare, RigPart::Loco, fc, Clip::Idle, 0, 0);
    const int run = rimRun(n, [&](int k) { return torsoPx(n, k); });
    rchk(g, run >= 3, id + fmt(" facing %d: the torso rim one run %d px (3)", fc, run));
  }
  for (int fc : {2, 3}) {
    const Fig f = probe(b, RigPart::Loco, fc, Clip::Idle, 0, 0);
    const int run = bestRun(f, [&](int k) { return gearPx(f, k) && f.step(k) >= 3 && faceBox(f, k) && (metalPx(f, k) || f.slot(k) == doll::Slot::Glow); });
    rchk(g, run < 2, id + fmt(" facing %d: a step 3-4 run %d px in the face box + 1 (under 2)", fc, run));
  }
}
struct LitStat { int gear = 0, lit = 0, warm = 0; double sum = 0, litSum = 0; };
LitStat litStat(const Fig& f) {
  LitStat s;
  for (int k = 0; k < f.p.w * f.p.h; k++) {
    if (!gearPx(f, k)) continue;
    const uint32_t c = f.colour(k);
    const double l = luma(c);
    double h, ch;
    hueChroma(c, h, ch);
    const bool glow = f.slot(k) == doll::Slot::Glow;
    s.gear++; s.sum += l;
    if (l >= 170 || glow) { s.lit++; s.litSum += l; }
    if (glow || (h >= 20 && h <= 60 && ch >= 30 && l >= 170)) s.warm++;
  }
  return s;
}
void r4Mythic(Line& g, const Built& b, const Built* t6, int item) {
  const std::string id = fmt("A%d %s r4", item, b.tag.c_str());
  if (!t6) return;
  for (int fc : {0, 2}) {
    const LitStat a = litStat(probe(b, RigPart::Loco, fc, Clip::Idle, 0, 0)), c = litStat(probe(*t6, RigPart::Loco, fc, Clip::Idle, 0, 0));
    rchk(g, a.lit >= 1.15 * c.lit, id + fmt(" facing %d: lit + glow %d px, %.2f of its T6's %d (1.15)", fc, a.lit, c.lit ? (double)a.lit / c.lit : 0.0, c.lit));
    const double ma = a.gear ? a.sum / a.gear : 0, mc = c.gear ? c.sum / c.gear : 1;
    const double la = a.lit ? a.litSum / a.lit : 0, lc = c.lit ? c.litSum / c.lit : 1;
    rchk(g, la >= 0.90 * lc, id + fmt(" facing %d: the lit + glow px mean luma %.0f, %.2f of its T6's %.0f (0.90)", fc, la, la / lc, lc));
    if (g_report) std::printf("    %s facing %d: gear mean luma %.0f vs T6 %.0f (%.2f; info)\n", id.c_str(), fc, ma, mc, ma / mc);
    if (g_report) for (const Built* q : {&b, t6}) {
      const Fig f = probe(*q, RigPart::Loco, fc, Clip::Idle, 0, 0);
      int hist[doll::SLOT_COUNT][5] = {};
      for (int k = 0; k < f.p.w * f.p.h; k++) if (gearPx(f, k)) hist[(int)f.slot(k)][f.step(k)]++;
      std::printf("    %s %s f%d hist:", id.c_str(), q->tag.c_str(), fc);
      for (int sl = 0; sl < doll::SLOT_COUNT; sl++) {
        int t = 0;
        for (int st = 0; st < 5; st++) t += hist[sl][st];
        if (t) std::printf(" s%d[%d %d %d %d] L%.0f/%.0f/%.0f/%.0f", sl, hist[sl][1], hist[sl][2], hist[sl][3], hist[sl][4], luma(q->r.ramps.c[sl][1]), luma(q->r.ramps.c[sl][2]), luma(q->r.ramps.c[sl][3]), luma(q->r.ramps.c[sl][4]));
      }
      std::printf("\n");
    }
    rchk(g, a.warm * 100 >= a.gear * 15, id + fmt(" facing %d: warm light %d of %d gear px (%.0f %%; 15)", fc, a.warm, a.gear, a.gear ? 100.0 * a.warm / a.gear : 0.0));
  }
  const Fig d = probe(b, RigPart::Loco, 0, Clip::Idle, 0, 0);
  std::vector<double> ls;
  for (int k = 0; k < d.p.w * d.p.h; k++) if (d.opaque(k) && itemHas(d, k, Role::Helm) && d.step(k) > 0) ls.push_back(luma(d.colour(k)));
  std::sort(ls.begin(), ls.end());
  const double range = ls.size() > 4 ? ls[ls.size() * 9 / 10] - ls[ls.size() / 10] : 0;
  rchk(g, range >= 100, id + fmt(": the helm's luma p90 - p10 %.0f (100)", range));
}
// the alternating Metal / Metal 2 row bands on the torso (facing D, noHeld): rows whose torso metal is 4 px or more and
// mostly one slot, runs of 2 rows or more, the count of alternations
int starBands(const Fig& n) {
  std::vector<int> lab, wid;
  for (int y = 0; y < n.p.h; y++) {
    int a = 0, c = 0;
    for (int x = 0; x < n.p.w; x++) {
      const int k = y * n.p.w + x;
      if (!torsoPx(n, k) || !metalPx(n, k)) continue;
      (n.slot(k) == doll::Slot::Metal ? a : c)++;
    }
    if (!a && !c) continue;   // a row with no torso metal (a collar, a belt) has no say: the runs count the rows the metal shows on
    lab.push_back(a > c ? 1 : c > a ? 2 : 0);
    wid.push_back(std::max(a, c));
  }
  std::vector<int> runs;   // the labels of runs of 2 rows or more and 4 px wide or more, in order (adjacent equal runs merge)
  for (size_t i = 0; i < lab.size();) {
    size_t j = i;
    int w = 0;
    while (j < lab.size() && lab[j] == lab[i]) w = std::max(w, wid[j++]);
    if (lab[i] && j - i >= 2 && w >= 4 && (runs.empty() || runs.back() != lab[i])) runs.push_back(lab[i]);
    i = j;
  }
  return (int)runs.size();
}
void r4Star(Line& g, const Built& b, int item) {
  const std::string id = fmt("A%d %s r4", item, b.tag.c_str());
  const doll::DollRecipe bare = noHeldRecipe(b.r);
  const Fig sb = probeR(bare, RigPart::Loco, 0, Clip::Idle, 0, 0);
  const int bands = starBands(sb);
  if (std::getenv("ENV_DUMP"))
    for (int y = 0; y < sb.p.h; y++) {
      std::string s;
      for (int x = 0; x < sb.p.w; x++) {
        const int k = y * sb.p.w + x;
        s += !sb.opaque(k) ? ' ' : !torsoPx(sb, k) ? '.' : sb.slot(k) == doll::Slot::Metal ? (char)('0' + sb.step(k)) : sb.slot(k) == doll::Slot::Metal2 ? (char)('a' + sb.step(k)) : 'o';
      }
      const int kc = y * sb.p.w + sb.p.headX + 8;
      const auto* ic = sb.opaque(kc) ? sb.it(kc) : nullptr;
      if (s.find_first_not_of(' ') != std::string::npos) std::printf("      %s |%s| %s slot %d\n", b.tag.c_str(), s.c_str(), ic ? ic->part.c_str() : "-", sb.opaque(kc) ? (int)sb.slot(kc) : -1);
    }
  rchk(g, bands >= 3, id + fmt(": %d alternating Metal / Metal 2 bands on the torso (3)", bands));
  for (int fc : {0, 2}) {
    const Fig f = probe(b, RigPart::Loco, fc, Clip::Idle, 0, 0);
    int m = 0, s1 = 0;
    for (int k = 0; k < f.p.w * f.p.h; k++) if (metalPx(f, k)) { m++; s1 += f.step(k) == 1; }
    rchk(g, m && s1 * 100 >= m * 20, id + fmt(" facing %d: step 1 %d of %d metal px (%.0f %%; 20)", fc, s1, m, m ? 100.0 * s1 / m : 0.0));
  }
  for (doll::Slot sl : {doll::Slot::Metal, doll::Slot::Metal2})
    for (int st : {2, 3}) {
      double h, c;
      hueChroma(b.r.ramps.c[(int)sl][st], h, c);
      rchk(g, c >= 28, id + fmt(": %s step %d chroma %.0f (28)", sl == doll::Slot::Metal ? "Metal" : "Metal 2", st, c));
    }
}
void r4Glass(Line& g, const Built& b, int item) {
  const std::string id = fmt("A%d %s r4", item, b.tag.c_str());
  for (int fc : {0, 2}) {
    const Fig f = probe(b, RigPart::Loco, fc, Clip::Idle, 0, 0);
    int n = 0, dark = 0, s3 = 0;
    for (int k = 0; k < f.p.w * f.p.h; k++) if (metalPx(f, k)) { n++; dark += f.step(k) <= 2; s3 += f.step(k) == 3; }
    rchk(g, n && dark * 100 >= n * 55, id + fmt(" facing %d: steps 1-2 %d of %d glass px (%.0f %%; 55)", fc, dark, n, n ? 100.0 * dark / n : 0.0));
    rchk(g, n && s3 * 100 <= n * 25, id + fmt(" facing %d: step 3 %d of %d glass px (%.0f %%; 25 or less)", fc, s3, n, n ? 100.0 * s3 / n : 0.0));
    const int pale = bestRun(f, [&](int k) { return metalPx(f, k) && f.step(k) >= 3; });
    rchk(g, pale < 12, id + fmt(" facing %d: a pale step 3-4 patch of %d px (under 12)", fc, pale));
  }
  const Fig d = probe(b, RigPart::Loco, 0, Clip::Idle, 0, 0);
  const int rh = rimRun(d, [&](int k) { return itemHas(d, k, Role::Helm); });
  rchk(g, rh >= 3, id + fmt(": the helm rim one run %d px (3)", rh));
  const doll::DollRecipe bare = noHeldRecipe(b.r);
  const Fig nd = probeR(bare, RigPart::Loco, 0, Clip::Idle, 0, 0);
  const int rt = rimRun(nd, [&](int k) { return torsoPx(nd, k); });
  rchk(g, rt >= 3, id + fmt(": the torso rim one run %d px (3)", rt));
  for (int side : {1, 2}) {
    bool any = false;
    for (const auto& it : d.p.items) any |= it.has(Role::Pauldron) && it.side == side;
    if (!any) continue;
    const int rs = rimRun(d, [&](int k) { const auto* it = d.it(k); return it && it->has(Role::Pauldron) && it->side == side; });
    rchk(g, rs >= 3, id + fmt(": the %s shoulder rim one run %d px (3)", side == 1 ? "right" : "left", rs));
  }
  const uint32_t* m = b.r.ramps.c[(int)doll::Slot::Metal];
  rchk(g, luma(m[3]) - luma(m[2]) >= 80, id + fmt(": Metal step 3 luma %.0f over step 2's %.0f by %.0f (80)", luma(m[3]), luma(m[2]), luma(m[3]) - luma(m[2])));
  if (g_report)
    for (doll::Slot sl : {doll::Slot::Metal, doll::Slot::Metal2}) {
      std::printf("    %s %s ramp:", id.c_str(), sl == doll::Slot::Metal ? "Metal" : "Metal2");
      for (int s = 0; s < 5; s++) std::printf(" #%06X(%.0f)", b.r.ramps.c[(int)sl][s] & 0xFFFFFFu, luma(b.r.ramps.c[(int)sl][s]));
      std::printf("\n");
    }
}
// ENV_DUMPTAG=<tag>: the figure facing D and R as slot letters (M Metal, G Metal2, W Glow, others '.'), steps as digits
void dumpTorso(const Built& b, int fc);
void dumpFig(const Built& b) {
  const char* t = std::getenv("ENV_DUMPTAG");
  if (!t || b.tag != t) return;
  std::printf("flags mythic %d metal-mythic %d metalKind %d\n", (int)b.r.has("mythic"), (int)b.r.has("metal-mythic"), (int)b.L.metalKind);
  dumpTorso(b, 2);
  for (int fc : {0, 2}) {
    const Fig f = probe(b, RigPart::Loco, fc, Clip::Idle, 0, 0);
    for (int y = 0; y < f.p.h; y++) {
      std::string s;
      for (int x = 0; x < f.p.w; x++) {
        const int k = y * f.p.w + x;
        if (!f.opaque(k)) { s += "  "; continue; }
        const doll::Slot sl = f.slot(k);
        const char c = sl == doll::Slot::Metal ? 'M' : sl == doll::Slot::Metal2 ? 'G' : sl == doll::Slot::Glow ? 'W' : sl == doll::Slot::Skin ? 's' : sl == doll::Slot::Cloak ? 'c' : '.';
        s += c; s += (char)('0' + f.step(k));
      }
      if (s.find_first_not_of(' ') != std::string::npos) std::printf("D%d %s\n", fc, s.c_str());
    }
  }
}
void dumpTorso(const Built& b, int fc) {
  const doll::DollRecipe bare = noHeldRecipe(b.r);
  const Fig n = probeR(bare, RigPart::Loco, fc, Clip::Idle, 0, 0);
  for (int y = 0; y < n.p.h; y++) {
    std::string s;
    for (int x = 0; x < n.p.w; x++) {
      const int k = y * n.p.w + x;
      s += !n.opaque(k) ? ' ' : rimPx(n, k) && torsoPx(n, k) ? '*' : torsoPx(n, k) && metalPx(n, k) ? (char)('0' + n.step(k)) : torsoPx(n, k) ? 'o' : n.step(k) == 0 ? '#' : '.';
    }
    if (s.find_first_not_of(' ') != std::string::npos) std::printf("T%d %s\n", fc, s.c_str());
  }
}

int adefectRows(const std::vector<Built>& looks, std::vector<Line>& lines) {
  lines.clear();
  for (int i = 1; i <= 8; i++) lines.push_back(Line{fmt("C19 A%d", i)});
  rowBrass(lines[0], looks, "a_uha", 1);
  rowBrass(lines[1], looks, "a_jaon", 2);
  rowBrass(lines[2], looks, "a_enkho", 3);
  // A1: Uhabrass's body parts from Jaonbrass's; A2: the head keys and distances
  if (const Built* u = find(looks, "a_uha"))
    if (const Built* j = find(looks, "a_jaon")) {
      const double od = doll::outlineDistance(doll::outlineOf(u->r), doll::outlineOf(j->r)).body;
      lines[0].check(od >= 0.08, fmt("A1 a_uha: body OD to JAONBRASS %.3f (0.08)", od));
    }
  if (const Built* j = find(looks, "a_jaon"))
    for (const char* t : {"a_papir", "a_crane"})
      if (const Built* o = find(looks, t)) {
        const double od = doll::outlineDistance(doll::outlineOf(j->r), doll::outlineOf(o->r)).head;
        lines[1].check(od >= 0.15, fmt("A2 a_jaon: head OD to %s %.3f (0.15)", t, od));
      }
  for (const char* t : {"fm_obsid", "f5_obsid"}) if (const Built* b = find(looks, t)) rowObsidian(lines[3], *b, 4);
  if (const Built* b = find(looks, "fm_myth")) rowMythic(lines[4], *b, find(looks, "e6") ? find(looks, "e6") : find(looks, "lt6"), 5);
  for (const char* t : {"fm_star", "f5_star"}) if (const Built* b = find(looks, t)) rowStar(lines[5], *b, 6);
  if (const Built* b = find(looks, "a_thaer")) rowGlass(lines[6], *b, 7);
  for (const char* t : {"a_thorn", "a_ilsil", "e6", "e6w"}) if (const Built* b = find(looks, t)) rowPlate(lines[7], *b, 8);
  for (const char* t : {"fm_obsid", "f5_obsid"}) if (const Built* b = find(looks, t)) r4Obsidian(lines[3], *b, 4);
  if (const Built* b = find(looks, "fm_myth")) r4Mythic(lines[4], *b, find(looks, "e6") ? find(looks, "e6") : find(looks, "lt6"), 5);
  for (const char* t : {"fm_star", "f5_star"}) if (const Built* b = find(looks, t)) r4Star(lines[5], *b, 6);
  if (const Built* b = find(looks, "a_thaer")) r4Glass(lines[6], *b, 7);
  for (const Built& b : looks) dumpFig(b);
  int f = 0;
  for (const Line& l : lines) f += l.fails ? 1 : 0;
  return f;
}
int adefectSelftests(const std::vector<Built>& looks) {
  int caught = 0, want = 0;
  auto expect = [&](bool failed, const char* what) {
    want++;
    if (failed) caught++;
    else std::printf("    C19 selftest NOT caught: %s\n", what);
  };
  // a chocolate brass ramp: D52 and the warm term catch it
  if (const Built* u = find(looks, "a_uha")) {
    Built c = *u;
    const uint32_t choc[5] = {hexc(0x1A0F08), hexc(0x3A2414), hexc(0x5A3A22), hexc(0x7A5434), hexc(0xA88050)};
    for (int s = 0; s < 5; s++) c.r.ramps.c[(int)doll::Slot::Metal][s] = choc[s];
    expect(!d52(c.r).empty(), "a chocolate brass ramp (A1-A3)");
  }
  // a mask with 1 px streaks: alternate steps across the head box's metal
  if (const Built* o = find(looks, "fm_obsid")) {
    Fig f = probe(*o, RigPart::Loco, 0, Clip::Idle, 0, 0);
    for (int k = 0; k < f.p.w * f.p.h; k++) {
      const int x = k % f.p.w, y = k / f.p.w;
      if (metalPx(f, k) && x >= f.p.headX && x < f.p.headX + 16 && y >= f.p.headY && y < f.p.headY + 14 && (x + y) % 2 == 0)
        f.p.code[(size_t)k] = (uint8_t)(2 + (int)f.slot(k) * 5 + (f.step(k) >= 3 ? 1 : 3));
    }
    const int hx = f.p.headX, hy = f.p.headY;
    expect(onePx(f, [&](int k) { const int x = k % f.p.w, y = k / f.p.w; return x >= hx && x < hx + 16 && y >= hy && y < hy + 14; }, true) > 2,
           "a mask face plate with 1 px streaks (A4)");
  }
  // a one-hue starmetal
  if (const Built* s = find(looks, "fm_star")) {
    Built c = *s;
    for (int k = 0; k < 5; k++) c.r.ramps.c[(int)doll::Slot::Metal2][k] = c.r.ramps.c[(int)doll::Slot::Metal][k];
    Line l{"st"};
    rowStar(l, c, 6);
    expect(l.fails > 0, "a one-hue starmetal (A6)");
  }
  // a speck in the Mythic's face box: a glow pixel at the face
  {
    Fig f;
    f.p.w = f.p.h = 20; f.p.code.assign(400, 0); f.p.owner.assign(400, -1); f.p.headX = 2; f.p.headY = 0;
    f.ok = true;
    f.p.code[(size_t)(8 * 20 + 8)] = (uint8_t)(2 + (int)doll::Slot::Glow * 5 + 3);
    int bad = 0;
    for (int k = 0; k < 400; k++) {
      const int x = k % 20, y = k / 20;
      if (f.p.code[(size_t)k] >= 2 && f.slot(k) == doll::Slot::Glow && x >= f.p.headX + 2 && x <= f.p.headX + 13 && y >= f.p.headY + 4 && y <= f.p.headY + 13) bad++;
    }
    expect(bad > 0, "a speck in the Mythic's face box (A5)");
  }
  // a one-sided glass (no rim): a glass body with its step 3 removed
  if (const Built* t = find(looks, "a_thaer")) {
    Fig f = probe(*t, RigPart::Loco, 0, Clip::Idle, 0, 0);
    for (int k = 0; k < f.p.w * f.p.h; k++) if (metalPx(f, k) && f.step(k) == 3) f.p.code[(size_t)k] = (uint8_t)(f.p.code[(size_t)k] - 1);
    expect(glassRim(f, [&](int k) { return itemHas(f, k, Role::Armour); }) < 3, "a one-sided glass (A7)");
  }
  // a 1 px broken dash on a plate's walk frame
  {
    Fig f;
    f.p.w = f.p.h = 9; f.p.code.assign(81, (uint8_t)(2 + (int)doll::Slot::Metal * 5 + 2)); f.p.owner.assign(81, -1); f.ok = true;
    f.p.code[40] = (uint8_t)(2 + (int)doll::Slot::Metal * 5 + 3);
    int lone = 0;
    for (int k = 0; k < 81; k++) {
      if (f.p.code[(size_t)k] < 2 || f.step(k) != 3) continue;
      int n = 0;
      const int x = k % 9, y = k / 9;
      for (int k2 : {f.p.at(x - 1, y), f.p.at(x + 1, y), f.p.at(x, y - 1), f.p.at(x, y + 1)}) n += k2 >= 0 && f.step(k2) >= 3;
      lone += n == 0;
    }
    expect(lone > 0, "a 1 px broken dash on a plate (A8)");
  }
  // ---- (envelope fix round, DOLL) the round-4 terms (contracts 3d "Round 4 adds", D58-D62), each tripped by a bad frame
  auto recode = [](Fig& f, const std::function<bool(int)>& in, const std::function<uint8_t(int)>& to) {
    for (int k = 0; k < f.p.w * f.p.h; k++) if (in(k)) f.p.code[(size_t)k] = to(k);
  };
  auto cd = [](doll::Slot sl, int st) { return (uint8_t)(2 + (int)sl * 5 + st); };
  // A3: a torso of blue cloth (the packager's Enkhobrass)
  if (const Built* e = find(looks, "a_enkho")) {
    const doll::DollRecipe bare = noHeldRecipe(e->r);
    Fig f = probeR(bare, RigPart::Loco, 0, Clip::Idle, 0, 0);
    recode(f, [&](int k) { return f.fig(k) && torsoPx(f, k) && f.step(k) >= 1; }, [&](int k) { return cd(doll::Slot::Cloth1, f.step(k)); });
    const TorsoMix t = torsoMix(f);
    expect(t.n && (t.metal * 100 < t.n * 50 || t.cloth * 100 > t.n * 30), "A3: a torso of cloth, not brass (D58)");
  }
  // A4: the torso rim broken into 1-2 px bits; a lit bar across the mask's lower face facing R
  if (const Built* o = find(looks, "f5_obsid")) {
    const doll::DollRecipe bare = noHeldRecipe(o->r);
    Fig f = probeR(bare, RigPart::Loco, 0, Clip::Idle, 0, 0);
    int i = 0;
    recode(f, [&](int k) { return torsoPx(f, k) && rimPx(f, k) && (i++ % 3) != 0; }, [&](int k) { return cd(f.slot(k), 2); });
    expect(rimRun(f, [&](int k) { return torsoPx(f, k); }) < 3, "A4: a torso rim of 1 px dots (D59)");
  }
  if (const Built* o = find(looks, "fm_obsid")) {
    Fig f = probe(*o, RigPart::Loco, 2, Clip::Idle, 0, 0);
    const int y = f.p.headY + 10;
    recode(f, [&](int k) { return k / f.p.w == y && k % f.p.w >= f.p.headX + 6 && k % f.p.w <= f.p.headX + 9 && f.opaque(k); }, [&](int) { return cd(doll::Slot::Metal, 3); });
    expect(bestRun(f, [&](int k) { return gearPx(f, k) && f.step(k) >= 3 && faceBox(f, k) && (metalPx(f, k) || f.slot(k) == doll::Slot::Glow); }) >= 2,
           "A4: a pale bar across the mask's lower face facing R (D59)");
  }
  // A5: a Mythic darker than its T6 (every lit step pressed to the body), flat helm, no warm light
  if (const Built* m = find(looks, "fm_myth")) {
    if (const Built* t6 = find(looks, "e6")) {
      Fig f = probe(*m, RigPart::Loco, 0, Clip::Idle, 0, 0);
      // every lit step pressed to the body, and the gold, the fire and the flame crown painted over in the black steel
      recode(f, [&](int k) { return gearPx(f, k) && (f.step(k) >= 3 || f.slot(k) == doll::Slot::Glow || f.slot(k) == doll::Slot::Metal2 || f.slot(k) == doll::Slot::Plume); },
             [&](int k) { return cd(doll::Slot::Metal, std::min(2, f.step(k))); });
      const LitStat a = litStat(f), c = litStat(probe(*t6, RigPart::Loco, 0, Clip::Idle, 0, 0));
      expect(a.lit < 1.15 * c.lit, "A5: a Mythic with no lit planes under its T6's (D60)");
      expect(a.warm * 100 < a.gear * 15, "A5: a Mythic with no gold or fire light (D60)");
      recode(f, [&](int k) { return f.opaque(k) && itemHas(f, k, Role::Helm) && f.step(k) > 0; }, [&](int) { return cd(doll::Slot::Metal, 2); });
      std::vector<double> ls;
      for (int k = 0; k < f.p.w * f.p.h; k++) if (f.opaque(k) && itemHas(f, k, Role::Helm) && f.step(k) > 0) ls.push_back(luma(f.colour(k)));
      std::sort(ls.begin(), ls.end());
      expect(ls.size() <= 4 || ls[ls.size() * 9 / 10] - ls[ls.size() / 10] < 100, "A5: a flat dark Mythic helm (D60)");
    }
  }
  // A6: a one-hue torso (Metal 2 painted over in Metal), no dark core, a grey ramp
  if (const Built* st = find(looks, "f5_star")) {
    const doll::DollRecipe bare = noHeldRecipe(st->r);
    Fig f = probeR(bare, RigPart::Loco, 0, Clip::Idle, 0, 0);
    recode(f, [&](int k) { return metalPx(f, k) && f.slot(k) == doll::Slot::Metal2; }, [&](int k) { return cd(doll::Slot::Metal, f.step(k)); });
    expect(starBands(f) < 3, "A6: a one-hue starmetal torso (D61)");
    recode(f, [&](int k) { return metalPx(f, k) && f.step(k) == 1; }, [&](int) { return cd(doll::Slot::Metal, 2); });
    int mm = 0, s1 = 0;
    for (int k = 0; k < f.p.w * f.p.h; k++) if (metalPx(f, k)) { mm++; s1 += f.step(k) == 1; }
    expect(!mm || s1 * 100 < mm * 20, "A6: a starmetal with no dark core (D61)");
    double h, c;
    hueChroma(hexc(0x9A9AA0), h, c);
    expect(c < 28, "A6: a grey (unsaturated) starmetal step (D61)");
  }
  // A7: a pale enamel plane (the glass lit all over), a rim the body's own value, the rim broken into dots
  if (const Built* t = find(looks, "a_thaer")) {
    Fig f = probe(*t, RigPart::Loco, 0, Clip::Idle, 0, 0);
    recode(f, [&](int k) { return metalPx(f, k) && f.step(k) >= 1 && f.step(k) <= 2; }, [&](int k) { return cd(f.slot(k), 3); });
    int n = 0, dark = 0, s3 = 0;
    for (int k = 0; k < f.p.w * f.p.h; k++) if (metalPx(f, k)) { n++; dark += f.step(k) <= 2; s3 += f.step(k) == 3; }
    expect(n && (dark * 100 < n * 55 || s3 * 100 > n * 25), "A7: a glass lit all over: enamel (D62)");
    expect(bestRun(f, [&](int k) { return metalPx(f, k) && f.step(k) >= 3; }) >= 12, "A7: a pale patch of 12 px or more: jade (D62)");
    Built c = *t;
    c.r.ramps.c[(int)doll::Slot::Metal][3] = c.r.ramps.c[(int)doll::Slot::Metal][2];
    const uint32_t* m = c.r.ramps.c[(int)doll::Slot::Metal];
    expect(luma(m[3]) - luma(m[2]) < 80, "A7: a rim no brighter than the glass body (D62)");
    Fig g = probe(*t, RigPart::Loco, 0, Clip::Idle, 0, 0);
    int i = 0;
    recode(g, [&](int k) { return itemHas(g, k, Role::Helm) && rimPx(g, k) && (i++ % 3) != 0; }, [&](int k) { return cd(g.slot(k), 2); });
    expect(rimRun(g, [&](int k) { return itemHas(g, k, Role::Helm); }) < 3, "A7: a helm rim of 1 px dots (D62)");
  }
  std::printf("  C19 selftests: %d of %d caught\n", caught, want);
  return want - caught;
}

}  // namespace dollenv

// ---------------------------------------------------------------- C16k: a head each (the owner's 37; every culture)
namespace dollenv {

struct OwnerAlloy { cult::Culture c; int idx; std::string name; };
std::vector<OwnerAlloy> owner37() {
  std::vector<OwnerAlloy> v;
  for (const gear::BoardCulture& bc : gear::forgeBoardSet()) {
    if (!bc.owner) continue;
    // (the ladder kit's culture: its id lets alloyKitLook's items find their maker, as C16o's boardAlloys)
    const cult::Culture& c = gear::kitCulture((cult::Archetype)bc.people, bc.seed);
    for (int i = 0; i < (int)c.arms.alloys.size() && i < 3; i++) v.push_back(OwnerAlloy{c, i, c.arms.alloys[(size_t)i].name});
  }
  return v;
}
std::vector<std::string> keyRepeats(const std::vector<std::pair<std::string, uint32_t>>& keys) {
  std::vector<std::string> out;
  for (size_t i = 0; i < keys.size(); i++)
    for (size_t j = i + 1; j < keys.size(); j++)
      if (keys[i].second == keys[j].second) out.push_back(keys[i].first + " and " + keys[j].first + fmt(" share head key 0x%X", keys[i].second));
  return out;
}
// the stability test: each owner alloy's mass moved to another value keeps every owner's head key
std::vector<std::string> stability(const std::vector<OwnerAlloy>& os) {
  std::vector<std::string> out;
  static uint32_t t[12][36][3];
  const uint32_t (*base)[36][3] = gear::shapeTable();
  for (const OwnerAlloy& o : os) {
    const uint32_t k0 = gear::alloyHeadKey(o.c, o.idx);
    std::memcpy(t, base, sizeof t);
    const int p = std::clamp((int)o.c.archetype, 0, 11), s = gear::forgeSchool6(o.c), r = gear::shapeRank(o.idx);
    gear::Shape x = gear::shapeOfKey(t[p][s][r]);
    x.mass = (uint8_t)(x.mass == F::MS_BROAD ? F::MS_NONE : F::MS_BROAD);
    t[p][s][r] = gear::shapeKey(x);
    gear::setShapeTableOverride(t);
    const uint32_t k1 = gear::alloyHeadKey(o.c, o.idx);
    gear::setShapeTableOverride(nullptr);
    if (k0 != k1) out.push_back(o.name + fmt(": head key 0x%X -> 0x%X when its mass moves", k0, k1));
  }
  return out;
}
int headGates(bool selftest, std::vector<std::string>& printed) {
  struct Rig { int was; Rig() : was(art::rigVersion()) { art::setRigVersion(2); } ~Rig() { art::setRigVersion(was); } } rig;
  Line data{"C16k heads (data)"}, cult{"C16k a head each"}, meas{"C16k heads apart (1x)"};
  const std::vector<OwnerAlloy> os = owner37();
  std::vector<std::pair<std::string, uint32_t>> keys;
  for (const OwnerAlloy& o : os) {
    const uint32_t k = gear::alloyHeadKey(o.c, o.idx);
    keys.push_back({o.name, k});
    const gear::BoardHead* bh = gear::boardHead(o.name);
    data.check(bh != nullptr, o.name + ": not in kBoardHeads");
    if (bh) {
      const gear::Shape s = gear::alloyShape(o.c, o.idx);
      data.check(s.helm == bh->helm && s.variant == bh->variant, o.name + fmt(": helm %s v%d (the board: %s v%d)", F::shapeName(F::SX_HELM, s.helm), s.variant,
                                                                              F::shapeName(F::SX_HELM, bh->helm), bh->variant));
    }
  }
  for (const std::string& w : keyRepeats(keys)) data.bad(w);
  data.ok();
  // every culture's alloys apart (seeds 1..64, every people)
  for (int p = 0; p < 12; p++)
    for (uint32_t sd = 1; sd <= 64; sd++) {
      const cult::Culture c = cult::Atlas::make((cult::Archetype)p, sd);
      std::vector<std::pair<std::string, uint32_t>> ck;
      for (int i = 0; i < (int)c.arms.alloys.size(); i++) ck.push_back({c.arms.alloys[(size_t)i].name, gear::alloyHeadKey(c, i)});
      const auto rep = keyRepeats(ck);
      for (const std::string& w : rep) cult.bad(fmt("%s s%u: ", cult::archetypeName((cult::Archetype)p), sd) + w);
      if (rep.empty()) cult.ok();
    }
  // measured: the 37 heads 0.10 head OD apart at 1x (outline masks: greyscale-blind)
  {
    std::vector<doll::Outline> outs;
    HumanLook base = a3Hero();
    for (const OwnerAlloy& o : os) outs.push_back(doll::outlineOf(doll::resolve(gear::alloyKitLook(base, o.c, o.idx))));
    int near = 0;
    for (size_t i = 0; i < os.size(); i++)
      for (size_t j = i + 1; j < os.size(); j++) {
        const double od = doll::outlineDistance(outs[i], outs[j]).head;
        if (od < doll::kHeadOdStep) near++;
        meas.check(od >= doll::kHeadOdStep, os[i].name + " / " + os[j].name + fmt(": head OD %.3f (%.2f, planned)", od, doll::kHeadOdStep));
      }
    (void)near;
  }
  int fails = (data.fails ? 1 : 0) + (cult.fails ? 1 : 0) + (meas.fails ? 1 : 0);
  printed.push_back(data.text());
  printed.push_back(cult.text());
  printed.push_back(meas.text());
  // the stability test holds on the shipped rule
  Line stab{"C16k stability"};
  for (const std::string& w : stability(os)) stab.bad(w);
  stab.ok();
  printed.push_back(stab.text());
  fails += stab.fails ? 1 : 0;
  if (selftest) {
    int want = 0, caught = 0;
    auto expect = [&](bool failed, const char* what) { want++; if (failed) caught++; else printed.push_back(std::string("    C16k selftest NOT caught: ") + what); };
    auto forced = keys;
    if (forced.size() >= 2) forced[1].second = forced[0].second;
    expect(!keyRepeats(forced).empty(), "two of the 37 forced to one key");
    gear::setLegacyVariantRule(true);
    expect(!stability(os).empty(), "a variant keyed on the mass (the stability test)");
    gear::setLegacyVariantRule(false);
    printed.push_back(fmt("  C16k selftests: %d of %d caught", caught, want));
    fails += want - caught;
  }
  return fails;
}

}  // namespace dollenv

// ---------------------------------------------------------------- (P2c wave 2, DOLL-C) C12f over source L and the caps on every look
// `--doll-envelope --scope ladder [--seeds a..b]`: gear::ladderKit for the 12 peoples x man and woman x seeds (kit=<people>:<t>:<s>
// on A3's hero / heroine, as the planner measured on 2026-10-28), each line against its own T1 with C12f's measure (noHeld, the
// walk's frame 0 facing D + the idle facing R): the area ladder, the Mythic (kit=<people>:6m:<s>) at its T6 or more, the head 5 %
// over the tier below from T2-T3 to T5-T6 (T2-T3 is the T2 budget: T2's head <= T3's / 1.05) and consecutive heads 0.10 head OD
// apart; a failing term under a ruling of doll::kEnvelopeRulings is printed as ruled. `--scope all`: the caps (shoulders, head
// rows and columns, the cloak's sides and back, T1 inside the bare outline, the skirt's flare) on every look of C1's sources: the
// census (A B D E G K T: test_doll.cpp dollCensusLooks), F (the gates' matrix with the forge looks) and L (with the envelope looks).
namespace dollenv {

const char* const kLPeople[12] = {"fjordfolk", "highland", "heartland", "imperial", "dune", "steppe", "marsh", "jade", "river", "suntemple",
                                  "sylvan", "starspire"};
const char* const kLPeopleName[12] = {"Fjordfolk", "Highland", "Heartland", "Imperial", "Dune", "Steppe", "Marsh", "Jade", "River",
                                      "Sun Temple", "Sylvan", "Starspire"};

std::string lTag(int p, int who, int seed, int t, bool mythic) {
  return fmt("L_%s_%s_s%d_t%d%s", kLPeople[p], who ? "w" : "m", seed, t, mythic ? "m" : "");
}
std::string sourceLText(int a, int b, int onlyPeople = -1) {
  std::string s;
  for (int p = 0; p < 12; p++) {
    if (onlyPeople >= 0 && p != onlyPeople) continue;
    for (int who = 0; who < 2; who++)
      for (int sd = a; sd <= b; sd++) {
        for (int t = 1; t <= 6; t++)
          s += fmt("%s %s kit=%s:%d:%d,palette=look\n", lTag(p, who, sd, t, false).c_str(), who ? "woman" : "man", kLPeople[p], t, sd);
        s += fmt("%s %s kit=%s:6m:%d,palette=look\n", lTag(p, who, sd, 6, true).c_str(), who ? "woman" : "man", kLPeople[p], sd);
      }
  }
  return s;
}

// one line's numbers (index 0 = T1 .. 5 = T6; od[i] = the head OD between T(i+2) and T(i+3): T2-T3 .. T5-T6)
struct LineNums { int area[6] = {}, head[6] = {}; double od[4] = {}; int mythic = -1; };
// every failing term of a line; each starts with the term's name as kEnvelopeRulings matches it
std::vector<std::string> climbRule(const LineNums& n) {
  std::vector<std::string> out = ladderRule(std::vector<int>(n.area, n.area + 6));
  if (n.mythic >= 0 && n.mythic < n.area[5]) out.push_back(fmt("Mythic area %d under its T6's %d", n.mythic, n.area[5]));
  if (n.head[1] * (100 + doll::kHeadAreaStepPct) > n.head[2] * 100)
    out.push_back(fmt("T2 head %d px over T3's %d / 1.%02d", n.head[1], n.head[2], doll::kHeadAreaStepPct));
  for (int t = 3; t < 6; t++)   // T3-T4 .. T5-T6
    if (n.head[t] * 100 < n.head[t - 1] * (100 + doll::kHeadAreaStepPct))
      out.push_back(fmt("T%d-T%d head %d -> %d px (5 %%)", t, t + 1, n.head[t - 1], n.head[t]));
  for (int i = 0; i < 4; i++)
    if (n.od[i] + 1e-9 < doll::kHeadOdStep) out.push_back(fmt("T%d-T%d head OD %.3f (%.2f)", i + 2, i + 3, n.od[i], doll::kHeadOdStep));
  // (wave 2 r3) what-if t2cap (the owner's switch, D69: the area ladder from T3, T2 only capped): never set outside --what-if
  if (doll::envelopeWhatIf() & 8)
    out.erase(std::remove_if(out.begin(), out.end(), [](const std::string& t) { return t.rfind("T2 area", 0) == 0 || (t.rfind("T3 area", 0) == 0 && t.find("over T2's") != std::string::npos); }), out.end());
  return out;
}
// the recorded ruling a failing term stands under (nullptr: none)
const doll::EnvelopeRuling* rulingFor(const char* people, int who, const std::string& term) {
  for (const doll::EnvelopeRuling& r : doll::kEnvelopeRulings) {
    if (!r.people || !r.term) continue;
    if (std::strcmp(r.people, people) || (r.body >= 0 && r.body != who)) continue;
    if (term.rfind(r.term, 0) == 0) return &r;
  }
  return nullptr;
}

struct LResult { int p, who, seed; LineNums n; std::vector<std::string> fails, ruled; };

int ladderSelftests() {
  int caught = 0, want = 0;
  auto expect = [&](bool failed, const char* what) {
    want++;
    if (failed) caught++;
    else std::printf("    C12f source L selftest NOT caught: %s\n", what);
  };
  LineNums good;
  const int ga[6] = {800, 890, 970, 1090, 1250, 1370}, gh[6] = {40, 60, 66, 72, 80, 88};
  for (int i = 0; i < 6; i++) { good.area[i] = ga[i]; good.head[i] = gh[i]; }
  for (double& d : good.od) d = 0.2;
  good.mythic = 1400;
  expect(climbRule(good).empty(), "a good line FAILED");
  { LineNums n = good; n.area[4] = 1200; expect(!climbRule(n).empty(), "a T5 at 1.50 of T1 (1.55)"); }
  { LineNums n = good; n.area[5] = n.area[4] + 20; expect(!climbRule(n).empty(), "a T6 0.03 over its T5 (0.05)"); }
  { LineNums n = good; n.head[1] = 64; expect(!climbRule(n).empty(), "a T2 head over T3's / 1.05 (the leather masked helm)"); }
  { LineNums n = good; n.head[4] = 74; expect(!climbRule(n).empty(), "a T5 head 3 % over its T4"); }
  { LineNums n = good; n.od[3] = 0.05; expect(!climbRule(n).empty(), "a T6 helm the shape of its T5 (OD 0.05)"); }
  { LineNums n = good; n.mythic = 1300; expect(!climbRule(n).empty(), "a Mythic smaller than its T6"); }
  expect(rulingFor("heartland", 0, "T5 area 1.50 of T1 (1.55)") == nullptr, "an unruled term reads as ruled");
  std::printf("  C12f source L selftests: %d of %d caught\n", caught, want);
  return want - caught;
}

std::vector<LResult> measureLines(int a, int b, int onlyPeople) {
  const std::vector<Built> looks = buildLooks(sourceLText(a, b, onlyPeople));
  std::map<std::string, const Built*> byTag;
  for (const Built& x : looks) byTag[x.tag] = &x;
  std::vector<LResult> res;
  for (int p = 0; p < 12; p++) {
    if (onlyPeople >= 0 && p != onlyPeople) continue;
    for (int who = 0; who < 2; who++)
      for (int sd = a; sd <= b; sd++) {
        LResult r{p, who, sd, {}, {}, {}};
        const Built* bs[6] = {};
        std::string err;
        for (int t = 1; t <= 6; t++) {
          bs[t - 1] = byTag[lTag(p, who, sd, t, false)];
          if (!bs[t - 1]->bad.empty()) err = bs[t - 1]->tag + ": " + bs[t - 1]->bad;
        }
        const Built* mb = byTag[lTag(p, who, sd, 6, true)];
        if (!err.empty()) { r.fails.push_back("bad look " + err); res.push_back(r); continue; }
        doll::Outline ol[6];
        for (int t = 0; t < 6; t++) {
          const Measured m = measure(*bs[t]);
          r.n.area[t] = m.area;
          r.n.head[t] = m.headArea;
          if (t >= 1) ol[t] = doll::outlineOf(bs[t]->r);
        }
        for (int i = 0; i < 4; i++) r.n.od[i] = doll::outlineDistance(ol[i + 1], ol[i + 2]).head;
        if (mb && mb->bad.empty()) r.n.mythic = measure(*mb).area;
        for (const std::string& f : climbRule(r.n)) {
          if (const doll::EnvelopeRuling* ru = rulingFor(kLPeople[p], who, f)) r.ruled.push_back(f + " [ruled: " + ru->plan + "]");
          else r.fails.push_back(f);
        }
        res.push_back(r);
      }
  }
  return res;
}

int runLadderScope(int a, int b, bool self, std::vector<std::string>& printed) {
  const int was = art::rigVersion();
  art::setRigVersion(2);
  const std::vector<LResult> res = measureLines(a, b, -1);
  int failLines = 0, ruledLines = 0;
  for (const LResult& r : res) {
    if (!r.fails.empty()) failLines++;
    else if (!r.ruled.empty()) ruledLines++;
    if (!g_report && r.fails.empty() && r.ruled.empty()) continue;
    std::string row, hrow;
    for (int t = 0; t < 6; t++) row += fmt(" %.2f", r.n.area[0] ? (double)r.n.area[t] / r.n.area[0] : 0.0);
    for (int t = 1; t < 6; t++) hrow += fmt(" %d", r.n.head[t]);
    const std::string why = r.fails.empty() ? "" : " -- " + r.fails[0] + (r.fails.size() > 1 ? fmt(" (+%d more)", (int)r.fails.size() - 1) : "");
    printed.push_back(fmt("  %s %-9s %s s%d T1 %3d px, ratios%s Mythic %.2f | heads T2-T6%s | OD %.2f %.2f %.2f %.2f%s", r.fails.empty() ? "ok  " : "MISS",
                          kLPeople[r.p], r.who ? "woman" : "man  ", r.seed, r.n.area[0], row.c_str(),
                          r.n.area[0] && r.n.mythic >= 0 ? (double)r.n.mythic / r.n.area[0] : 0.0, hrow.c_str(), r.n.od[0], r.n.od[1], r.n.od[2],
                          r.n.od[3], why.c_str()));
    if (g_report)
      for (size_t i = 1; i < r.fails.size(); i++) printed.push_back("        also: " + r.fails[i]);
    for (const std::string& x : r.ruled) printed.push_back("        ruled: " + x);
  }
  // the per-people table (PLAN "The envelope in production"): the median of the people's lines as a multiple of T1, in brackets
  // how many reach the tier's ratio; the Mythic at its T6; the lines whose head steps hold (T2-T3 / T3-T4 / T4-T5 / T5-T6)
  printed.push_back("  | people | T2 | T3 | T4 | T5 | T6 | Mythic >= T6 | head steps T2-3/3-4/4-5/5-6 | lines ok |");
  printed.push_back("  |---|---|---|---|---|---|---|---|---|");
  int atRatio[6] = {}, stepUnder[5] = {}, headUnder[4] = {}, odUnder[4] = {}, mythUnder = 0;
  for (int p = 0; p < 12; p++) {
    std::string row = fmt("  | %s |", kLPeopleName[p]);
    std::vector<const LResult*> ls;
    for (const LResult& r : res) if (r.p == p && r.n.area[0] > 0) ls.push_back(&r);
    for (int t = 1; t < 6; t++) {
      std::vector<double> v;
      int at = 0;
      for (const LResult* r : ls) {
        const double q = (double)r->n.area[t] / r->n.area[0];
        v.push_back(q);
        if (q + 1e-9 >= doll::envelopeOf(t + 1).area / 100.0) at++;
      }
      std::sort(v.begin(), v.end());
      const double med = v.empty() ? 0 : v.size() % 2 ? v[v.size() / 2] : (v[v.size() / 2 - 1] + v[v.size() / 2]) / 2;
      row += fmt(" %.2f [%d] |", med, at);
      atRatio[t] += at;
    }
    int my = 0, hs[4] = {}, ok = 0;
    for (const LResult* r : ls) {
      my += r->n.mythic >= r->n.area[5];
      hs[0] += r->n.head[1] * (100 + doll::kHeadAreaStepPct) <= r->n.head[2] * 100;
      for (int t = 3; t < 6; t++) hs[t - 2] += r->n.head[t] * 100 >= r->n.head[t - 1] * (100 + doll::kHeadAreaStepPct);
      for (int i = 0; i < 4; i++) odUnder[i] += r->n.od[i] + 1e-9 < doll::kHeadOdStep;
      for (int t = 1; t < 6; t++) stepUnder[t - 1] += (double)(r->n.area[t] - r->n.area[t - 1]) / r->n.area[0] + 1e-9 < doll::kAreaStep / 100.0;
    }
    for (const LResult& r : res) if (r.p == p) ok += r.fails.empty();
    mythUnder += (int)ls.size() - my;
    for (int i = 0; i < 4; i++) headUnder[i] += (int)ls.size() - hs[i];
    row += fmt(" %d/%zu | %d/%d/%d/%d | %d/%zu |", my, ls.size(), hs[0], hs[1], hs[2], hs[3], ok, ls.size());
    printed.push_back(row);
  }
  const int nl = (int)res.size();
  printed.push_back(fmt("  lines at the ratio (of %d): T2 %d, T3 %d, T4 %d, T5 %d, T6 %d", nl, atRatio[1], atRatio[2], atRatio[3], atRatio[4], atRatio[5]));
  printed.push_back(fmt("  steps under 0.05 of T1: T1-T2 %d, T2-T3 %d, T3-T4 %d, T4-T5 %d, T5-T6 %d", stepUnder[0], stepUnder[1], stepUnder[2],
                        stepUnder[3], stepUnder[4]));
  printed.push_back(fmt("  heads: T2 over T3's / 1.05 on %d; under the 5 %% step T3-T4 %d, T4-T5 %d, T5-T6 %d; head OD under %.2f: T2-T3 %d, T3-T4 %d, "
                        "T4-T5 %d, T5-T6 %d; the Mythic under its T6 on %d",
                        headUnder[0], headUnder[1], headUnder[2], headUnder[3], doll::kHeadOdStep, odUnder[0], odUnder[1], odUnder[2], odUnder[3], mythUnder));
  Line g{"C12f source L lines"};
  for (const LResult& r : res) {
    for (const std::string& f : r.fails) g.bad(fmt("%s %s s%d: ", kLPeople[r.p], r.who ? "woman" : "man", r.seed) + f);
    if (r.fails.empty()) g.ok();
  }
  printed.push_back(fmt("  C12f source L: %d lines (seeds %d..%d), %d pass, %d ruled, %d miss", nl, a, b, nl - failLines - ruledLines, ruledLines, failLines));
  printed.push_back(g.text());
  int fails = g.fails ? 1 : 0;
  if (self) {
    const int miss = ladderSelftests();
    printed.push_back(fmt("%s %-24s %4d checks, %4d failed", miss ? "FAIL:" : "ok   ", "C12f source L selftests", 8, miss));
    fails += miss ? 1 : 0;
  }
  art::setRigVersion(was);
  return fails;
}

// the skirt's flare facing D: the pixels of a hang part (skirt, tassets, faulds) past the bare body's row at the same height,
// the widest reach on either side
bool hangIt(const doll::FrameProbe::It& it) {
  for (const char* w : {"skirt", "tasset", "fauld", "hang"})
    if (it.part.find(w) != std::string::npos) return it.has(Role::Armour) || it.has(Role::Overlay) || it.has(Role::Garment);
  return false;
}
int skirtFlareOf(const Fig& f, const Fig& bare) {
  if (!f.ok || !bare.ok) return 0;
  std::map<int, std::pair<int, int>> rows;   // y (pivot-relative) -> the bare figure's x range
  for (int k = 0; k < bare.p.w * bare.p.h; k++) {
    if (!bare.fig(k)) continue;
    const int x = k % bare.p.w - bare.p.px, y = k / bare.p.w - bare.p.py;
    auto it = rows.find(y);
    if (it == rows.end()) rows[y] = {x, x};
    else { it->second.first = std::min(it->second.first, x); it->second.second = std::max(it->second.second, x); }
  }
  int reach = 0;
  for (int k = 0; k < f.p.w * f.p.h; k++) {
    if (!f.opaque(k)) continue;
    const auto* it = f.it(k);
    if (!it || !hangIt(*it)) continue;
    const int x = k % f.p.w - f.p.px, y = k / f.p.w - f.p.py;
    auto r = rows.find(y);
    if (r == rows.end()) continue;
    reach = std::max(reach, std::max(r->second.first - x, x - r->second.second));
  }
  return reach;
}

struct CapStat { int looks = 0, fails = 0; std::map<std::string, int> terms; std::vector<std::string> first; std::map<int, std::vector<int>> flare; };
void capsOn(const Built& b, char src, CapStat& cs, std::map<int, Built>& bare) {
  cs.looks++;
  if (!b.bad.empty()) {
    cs.fails++;
    cs.terms["bad look"]++;
    if (cs.first.size() < 12) cs.first.push_back(b.tag + ": " + b.bad);
    return;
  }
  const int tb = tierOf(b.L, HumanLook::TIER_BODY), th = tierOf(b.L, HumanLook::TIER_HELM), tc = tierOf(b.L, HumanLook::TIER_CLOAK);
  const Fig id = probe(b, RigPart::Loco, 0, Clip::Idle, 0, 0), ir = probe(b, RigPart::Loco, 2, Clip::Idle, 0, 0),
            il = probe(b, RigPart::Loco, 3, Clip::Idle, 0, 0);
  std::vector<std::string> w;
  const std::string s = shoulderRule(tb, shouldersOf(id));
  if (!s.empty()) w.push_back(s);
  const std::string c = cloakRule(tc, cloakSideOf(id), std::max(cloakBackOf(ir, 2), cloakBackOf(il, 3)));
  if (!c.empty()) w.push_back(c);
  for (const Fig* f : {&id, &ir}) {
    const std::string h = headRule(th, headOf(*f, b.who));
    if (!h.empty()) { w.push_back(h); break; }
  }
  const int bk = b.who * 100 + b.L.build;
  if (!bare.count(bk)) {
    Built x;
    x.who = b.who;
    x.L = b.who ? a3Woman() : a3Hero();
    x.L.build = b.L.build;
    x.L.weapon = 0;
    x.r = doll::resolve(x.L);
    bare[bk] = x;
  }
  const Built& bb = bare[bk];
  // the skirt's flare: measured on every tiered look and reported per tier (kEnvelope's skirtFlare is "extra px past the hips"
  // over the base skirt, whose width the contracts do not fix yet: reported, not gated, until the planner names the base)
  if (tb >= 1) cs.flare[tb].push_back(skirtFlareOf(id, probe(bb, RigPart::Loco, 0, Clip::Idle, 0, 0)));
  if (tb == 1) {
    const int od = t1Outside(probe(b, RigPart::Loco, 0, Clip::Walk, 0, 0), probe(bb, RigPart::Loco, 0, Clip::Walk, 0, 0), b.who);
    const int orr = t1Outside(ir, probe(bb, RigPart::Loco, 2, Clip::Idle, 0, 0), b.who);
    if (od || orr) w.push_back(fmt("T1 %d px (D) and %d px (R) outside the bare outline + 1", od, orr));
  }
  if (w.empty()) return;
  cs.fails++;
  for (const std::string& x : w) cs.terms[x.substr(0, x.find_first_of("0123456789"))]++;
  if (cs.first.size() < 12 || g_report) cs.first.push_back(fmt("%c %s: ", src, b.tag.c_str()) + w[0] + (w.size() > 1 ? fmt(" (+%d)", (int)w.size() - 1) : ""));
}

}  // namespace dollenv
void dollCensusLooks(uint64_t a, uint64_t b, std::vector<std::pair<char, art::HumanLook>>& out);   // test_doll.cpp
std::string dollSliceLooksText();   // test_doll_arms.cpp (the gates' matrix with the forge looks)
namespace dollenv {

int runAllScope(int a, int b, std::vector<std::string>& printed) {
  const int was = art::rigVersion();
  art::setRigVersion(2);
  gear::installLadderMacros();
  std::map<char, CapStat> st;
  std::map<int, Built> bare;
  {   // the census: A B D E G K T
    std::vector<std::pair<char, HumanLook>> cl;
    dollCensusLooks((uint64_t)a, (uint64_t)b, cl);
    int i = 0;
    for (const auto& w : cl) {
      if (!art::rig2Covers(w.second)) continue;   // drawn on v1 (C1 counts it)
      Built x;
      x.tag = fmt("%c%d", w.first, i++);
      x.who = w.second.body == 1 ? 1 : 0;
      x.L = w.second;
      x.r = doll::resolve(x.L);
      capsOn(x, w.first, st[w.first], bare);
    }
  }
  for (const Built& x : buildLooks(dollSliceLooksText())) capsOn(x, 'F', st['F'], bare);
  for (const Built& x : buildLooks(sourceLText(a, b) + kEnvelopeLooks)) capsOn(x, 'L', st['L'], bare);
  int fails = 0, looks = 0;
  std::map<int, std::vector<int>> flare;
  for (const auto& kv : st) {
    looks += kv.second.looks;
    fails += kv.second.fails;
    std::string terms;
    for (const auto& t : kv.second.terms) terms += fmt("; %sx%d", t.first.c_str(), t.second);
    printed.push_back(fmt("  caps source %c: %5d looks, %4d outside a cap%s", kv.first, kv.second.looks, kv.second.fails, terms.c_str()));
    for (const std::string& f : kv.second.first) printed.push_back("      " + f);
    for (const auto& fl : kv.second.flare) flare[fl.first].insert(flare[fl.first].end(), fl.second.begin(), fl.second.end());
  }
  std::string fr;
  for (auto& kv : flare) {
    std::sort(kv.second.begin(), kv.second.end());
    fr += fmt(" T%d max %d (median %d, %zu looks);", kv.first, kv.second.back(), kv.second[kv.second.size() / 2], kv.second.size());
  }
  printed.push_back("  skirt flare past the bare hips facing D (reported):" + fr);
  printed.push_back(fmt("%s %-24s %4d checks, %4d failed", fails ? "FAIL:" : "ok   ", "C12f caps (every look)", looks, fails));
  art::setRigVersion(was);
  return fails ? 1 : 0;
}

}  // namespace dollenv

std::string dollArmsLooksText();   // test_doll_arms.cpp: the matrices M8-M12 (tools/doll/arms_looks.txt, embedded)
// ---------------------------------------------------------------- the commands
namespace dollenv {

bool loadPool(int argc, char** argv) {
  for (int i = 1; i + 1 < argc; i++)
    if (!std::strcmp(argv[i], "--pool")) {
      std::string err;
      if (!doll::loadPoolFile(argv[i + 1], &err)) { std::printf("--pool %s: %s\n", argv[i + 1], err.c_str()); return false; }
      std::printf("  pool loaded from %s\n", argv[i + 1]);
    }
  return true;
}
std::string looksArg(int argc, char** argv) {
  for (int i = 1; i + 1 < argc; i++) if (!std::strcmp(argv[i], "--looks")) return readFile(argv[i + 1]);
  return kEnvelopeLooks;
}

int envelopePrint(int, char**) {
  std::printf("%s", doll::describeEnvelope().c_str());
  return 0;
}

// (envelope fix round, D64; the packager: `kit=heartland:6m` rendered the Starmetal T6) a kit tag renders what it says:
// kit=<people>:6m:<seed> gives the same fields as kit=<people>:6:<seed>,rarity=mythic, and any other suffix or a malformed
// tier is an error naming the tag
void kitTagGate(Line& g) {
  gear::installLadderMacros();
  auto fields = [](const std::string& spec, std::string* bad) {
    HumanLook L = a3Hero();
    doll::applyLookSpec(L, spec, nullptr, nullptr, bad);
    return gear::describeLook(L);
  };
  std::string b1, b2;
  const std::string m = fields("hero=1,kit=heartland:6m:7", &b1), r = fields("hero=1,kit=heartland:6:7,rarity=mythic", &b2);
  g.check(b1.empty() && b2.empty() && m == r, "kit=heartland:6m:7 differs from kit=heartland:6:7,rarity=mythic" + (b1.empty() ? std::string() : " (bad " + b1 + ")"));
  g.check(m.find("mythic=") != std::string::npos, "kit=heartland:6m:7 is not the Mythic");
  for (const char* t : {"hero=1,kit=heartland:6x:7", "hero=1,kit=heartland:5m:7", "hero=1,kit=heartland:m:7", "hero=1,kit=heartland:9:7",
                        "hero=1,kit=heartland:6m:7,rarity=rare"}) {
    std::string bad;
    fields(t, &bad);
    g.check(!bad.empty() && bad.find("kit") != std::string::npos, std::string(t) + ": no error naming the tag");
  }
}

int runEnvelope(const std::string& text, bool self, std::vector<std::string>& printed) {
  const int was = art::rigVersion();
  art::setRigVersion(2);
  const std::vector<Built> looks = buildLooks(text);
  Line g{"C12f envelope"}, c3b{"C3b helm by tier"}, lad{"C12f ladder lines"}, mot{"C13 drawn motion"}, b10{"B10 by tier (env)"}, kt{"D64 kit tags"};
  envelopeLooksGate(looks, g, c3b, lad, nullptr);
  kitTagGate(kt);
  motionGate(looks, mot);
  atlasGate(looks, b10, nullptr);
  int fails = 0;
  for (const std::string& n : lad.notes) printed.push_back(n);
  for (Line* l : {&g, &c3b, &lad, &mot, &b10, &kt}) { printed.push_back(l->text()); fails += l->fails ? 1 : 0; }
  if (self) {
    const int miss = envelopeSelftests();
    printed.push_back(fmt("%s %-24s %4d checks, %4d failed", miss ? "FAIL:" : "ok   ", "C12f selftests", 10, miss));
    fails += miss ? 1 : 0;
  }
  art::setRigVersion(was);
  return fails;
}
int envelopeCmd(int argc, char** argv) {
  bool self = false;
  const char* scope = nullptr;
  int a = 1, b = 3;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--report")) g_report = true;
    if (!std::strcmp(argv[i], "--selftest")) self = true;
    if (!std::strcmp(argv[i], "--scope") && i + 1 < argc) scope = argv[++i];
    else if (!std::strcmp(argv[i], "--what-if") && i + 1 < argc) {   // (wave 2 r3) the planner's numbers: see envelopeWhatIf
      const std::string w = std::string(",") + argv[++i] + ",";
      int v = 0;
      const char* const names[4] = {"t4large", "t3nocape", "t4cloak", "t2cap"};
      for (int k = 0; k < 4; k++) if (w.find(std::string(",") + names[k] + ",") != std::string::npos) v |= 1 << k;
      doll::envelopeWhatIf() = v;
      std::printf("WHAT-IF %s (bits %d): numbers for the planner's ruling, never a gate result\n", argv[i], v);
    } else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) { if (std::sscanf(argv[++i], "%d..%d", &a, &b) < 2) b = a; }
  }
  if (scope && std::strcmp(scope, "ladder") && std::strcmp(scope, "all")) { std::printf("--doll-envelope --scope ladder|all\n"); return 1; }
  if (!loadPool(argc, argv)) return 1;
  std::vector<std::string> out;
  // (P2c wave 2, DOLL-C) --scope ladder: C12f over source L; --scope all: the caps on every look of C1's sources
  const int f = scope && !std::strcmp(scope, "ladder") ? runLadderScope(a, b, self, out)
              : scope                                  ? runAllScope(a, b, out)
                                                       : runEnvelope(looksArg(argc, argv), self, out);
  for (const std::string& s : out) std::printf("%s\n", s.c_str());
  std::printf("doll-envelope: %d failure(s)\n", f);
  return f ? 1 : 0;
}
int runAdefects(const std::string& text, bool self, std::vector<std::string>& printed) {
  const int was = art::rigVersion();
  art::setRigVersion(2);
  std::string t = text;
  if (t.find("\ne6 ") == std::string::npos && t.rfind("e6 ", 0) != 0) t += std::string("\n") + kEnvelopeLooks;   // the T6 the Mythic stands beside
  const std::vector<Built> looks = buildLooks(t);
  std::vector<Line> lines;
  int fails = adefectRows(looks, lines);
  for (const Line& l : lines) printed.push_back(l.text());
  if (self) {
    const int miss = adefectSelftests(looks);
    printed.push_back(fmt("%s %-24s %4d checks, %4d failed", miss ? "FAIL:" : "ok   ", "C19 selftests", 19, miss));
    fails += miss ? 1 : 0;
  }
  art::setRigVersion(was);
  return fails;
}
int adefectsCmd(int argc, char** argv) {
  bool self = false;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--report")) g_report = true;
    if (!std::strcmp(argv[i], "--selftest")) self = true;
  }
  if (!loadPool(argc, argv)) return 1;
  std::vector<std::string> out;
  const int f = runAdefects(looksArg(argc, argv), self, out);
  for (const std::string& s : out) std::printf("%s\n", s.c_str());
  std::printf("doll-adefects: %d failure(s)\n", f);
  return f ? 1 : 0;
}
int headsCmd(int argc, char** argv) {
  bool self = false;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--report")) g_report = true;
    if (!std::strcmp(argv[i], "--selftest")) self = true;
  }
  if (!loadPool(argc, argv)) return 1;
  const int was = art::rigVersion();
  art::setRigVersion(2);
  std::vector<std::string> out;
  const int f = headGates(self, out);
  art::setRigVersion(was);
  for (const std::string& s : out) std::printf("%s\n", s.c_str());
  if (g_report)
    for (const OwnerAlloy& o : owner37()) {
      const gear::Shape s = gear::alloyShape(o.c, o.idx);
      const gear::AlloyDetails d = gear::alloyDetails(o.c, o.idx, s);
      std::printf("  head %-13s %-8s v%d crest %-7s key 0x%04X\n", o.name.c_str(), F::shapeName(F::SX_HELM, s.helm), s.variant,
                  d.crest ? F::valueName(F::AX_CREST, d.crest) : "-", gear::alloyHeadKey(o.c, o.idx));
    }
  std::printf("doll-heads: %d failure(s)\n", f);
  return f ? 1 : 0;
}
// --doll-coverage --scope envelope: the envelope's fallbacks (pauldron sizes) and the variants the pool does not draw yet
int coverage(int argc, char** argv) {
  for (int i = 1; i < argc; i++) if (!std::strcmp(argv[i], "--report")) g_report = true;
  if (!loadPool(argc, argv)) return 1;
  const int was = art::rigVersion();
  art::setRigVersion(2);
  std::map<std::string, std::vector<std::string>> miss, vocab;
  // (P2c wave 2, DOLL-C) the envelope looks, source L (seeds 1..3) and the matrices M8-M12: the envelope's fallbacks, and the
  // production vocabulary's (contracts 3: sheaths, back items, library weapons, effects; DollRecipe::vocabMissing)
  for (const Built& b : buildLooks(std::string(kEnvelopeLooks) + sourceLText(1, 3) + dollArmsLooksText())) {
    for (const std::string& m : b.r.envelopeMissing) miss[m].push_back(b.tag);
    for (const std::string& m : b.r.vocabMissing) vocab[m].push_back(b.tag);
  }
  // a variant is drawn when the flag changes the head: the figure with and without `variant-<n>`
  for (const OwnerAlloy& o : owner37()) {
    const doll::DollRecipe r = doll::resolve(gear::alloyKitLook(a3Hero(), o.c, o.idx));
    if (!r.helmVariant) continue;
    doll::DollRecipe r0 = r;
    const std::string fl = "variant-" + std::to_string(r.helmVariant);
    r0.flags.erase(std::remove(r0.flags.begin(), r0.flags.end(), fl), r0.flags.end());
    const doll::FrameProbe a = doll::probeFrame(r, RigPart::Loco, 0, Clip::Idle, 0, 0, true), c = doll::probeFrame(r0, RigPart::Loco, 0, Clip::Idle, 0, 0, true);
    if (a.code == c.code && a.w == c.w) miss[fmt("helm fam.%s %s", F::shapeName(F::SX_HELM, r.shape[F::SX_HELM]), fl.c_str())].push_back(o.name);
  }
  art::setRigVersion(was);
  for (const auto& kv : miss) {
    std::string who;
    for (size_t i = 0; i < kv.second.size() && i < 6; i++) who += (i ? " " : "") + kv.second[i];
    std::printf("  envelope not drawn: %-34s (%zu: %s%s)\n", kv.first.c_str(), kv.second.size(), who.c_str(), kv.second.size() > 6 ? " ..." : "");
  }
  for (const auto& kv : vocab) {
    std::string who;
    for (size_t i = 0; i < kv.second.size() && i < 4; i++) who += (i ? " " : "") + kv.second[i];
    std::printf("  vocabulary fallback: %-34s (%zu: %s%s)\n", kv.first.c_str(), kv.second.size(), who.c_str(), kv.second.size() > 4 ? " ..." : "");
  }
  std::printf("doll-coverage (envelope): %zu value(s) not drawn yet; %zu vocabulary value(s) on their fallback\n", miss.size(), vocab.size());
  return miss.empty() && vocab.empty() ? 0 : 1;
}

// (envelope fix round) --doll-recipe <tag>[,<tag>...] [--looks f] [--pool f]: a look's resolved parts, flags and fallbacks
int recipeCmd(int argc, char** argv) {
  const char* tags = nullptr;
  for (int i = 1; i + 1 < argc; i++) if (!std::strcmp(argv[i], "--doll-recipe")) tags = argv[i + 1];
  if (!tags) { std::printf("--doll-recipe <tag>[,<tag>...] [--looks f] [--pool f]\n"); return 1; }
  if (!loadPool(argc, argv)) return 1;
  const int was = art::rigVersion();
  art::setRigVersion(2);
  const std::string want = std::string(",") + tags + ",";
  const doll::pool::Pool& P = doll::pool::get();
  for (const Built& b : buildLooks(looksArg(argc, argv))) {
    if (want.find("," + b.tag + ",") == std::string::npos) continue;
    std::printf("%s%s\n  needs:", b.tag.c_str(), b.bad.empty() ? "" : (" BAD " + b.bad).c_str());
    for (const auto& n : b.r.needs) std::printf(" %s:%s", doll::roleName(n.role), n.value.c_str());
    std::printf("\n  parts:");
    for (int pi : b.r.parts) std::printf(" %s", P.s(P.parts[(size_t)pi].name).c_str());
    std::printf("\n  flags:");
    for (const std::string& f : b.r.flags) std::printf(" %s", f.c_str());
    std::printf("\n  envelopeMissing:");
    for (const std::string& f : b.r.envelopeMissing) std::printf(" [%s]", f.c_str());
    std::printf("\n  forgeMissing:");
    for (const std::string& f : b.r.forgeMissing) std::printf(" [%s]", f.c_str());
    const Measured m = measure(b);
    std::printf("\n  area %d (D %d R %d) shoulders %d head px %d rows %d cloak D %d back %d\n", m.area, m.areaD, m.areaR, m.shoulders, m.headArea,
                m.hd.rows, m.cloakD, m.cloakBack);
    // --map: the idle D and R frames as slot letters (a Skin b Hair c Eyes d Face e Ink f Cloth1 g Cloth2 h Accent i Linen j Cloth3
    // k Leather l Metal m Metal2 n Head o Cloak p Pattern q Plume r Field s Device t Glow u Fx v Blade) with the step, and the
    // part under each pixel by index
    bool map = false;
    for (int i = 1; i < argc; i++) map |= !std::strcmp(argv[i], "--map");
    if (map)
      for (int fc : {0, 2}) {
        const Fig f = probe(b, RigPart::Loco, fc, Clip::Idle, 0, 0);
        std::map<std::string, int> ix;
        std::vector<std::string> names;
        std::printf("  facing %d (slot letter + step | part index)\n", fc);
        for (int y = 0; y < f.p.h; y++) {
          std::string a, c;
          for (int x = 0; x < f.p.w; x++) {
            const int k = y * f.p.w + x;
            if (!f.opaque(k)) { a += " ."; c += " ."; continue; }
            a += (char)('a' + (int)f.slot(k));
            a += (char)('0' + f.step(k));
            const std::string pn = f.it(k) ? f.it(k)->part : "-";
            if (!ix.count(pn)) { ix[pn] = (int)names.size(); names.push_back(pn); }
            const int n = ix[pn];
            c += n < 10 ? " " + std::to_string(n) : std::string(1, (char)('A' + n - 10)) + " ";
          }
          std::printf("  %s   %s\n", a.c_str(), c.c_str());
        }
        for (size_t i = 0; i < names.size(); i++) std::printf("    %zu %s\n", i, names[i].c_str());
      }
  }
  art::setRigVersion(was);
  return 0;
}

}  // namespace dollenv

RPG_TEST_CMD("--doll-recipe", "P2c a look's resolved parts, flags and fallbacks: --doll-recipe <tag>[,<tag>...] [--looks f] [--pool f]", dollenv::recipeCmd);

// the lines in --doll --gates (test_doll.cpp) and --doll-forge (test_doll_forge.cpp)
int dollEnvelopeGates(bool report, std::vector<std::string>& printed) {
  dollenv::g_report = report;
  int f = dollenv::runEnvelope(dollenv::kEnvelopeLooks, true, printed);
  f += dollenv::runAdefects(dollenv::kEnvelopeLooks, true, printed);
  return f;
}
int dollHeadGates(bool report, bool selftest, std::vector<std::string>& printed) {
  dollenv::g_report = report;
  return dollenv::headGates(selftest, printed);
}
int dollEnvelopeCoverage(int argc, char** argv) { return dollenv::coverage(argc, argv); }
std::string dollEnvelopeLooksText() { return dollenv::envelopeLooksText(); }

RPG_TEST_CMD("--envelope", "P2c envelope: print the tier envelope (art::doll::kEnvelope)", dollenv::envelopePrint);
RPG_TEST_CMD("--doll-envelope", "P2c envelope gates C12f C3b-by-tier C13-motion B10-by-tier [--looks f] [--pool f] [--report] [--selftest]; --scope ladder [--seeds a..b] (C12f over source L, the per-people table) / --scope all [--seeds a..b] (the caps on every look of C1's sources)", dollenv::envelopeCmd);
RPG_TEST_CMD("--doll-adefects", "P2c C19 the A-defect reads [--looks f] [--pool f] [--report] [--selftest]", dollenv::adefectsCmd);
RPG_TEST_CMD("--doll-heads", "P2c C16k a head each: the owner's 37 [--report] [--selftest] [--pool f]", dollenv::headsCmd);
