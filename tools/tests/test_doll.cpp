// rpg_test --doll, --doll-coverage and --doll-variety (P2b New People, VISION_PLAN 15.36 phase A item 6: the lead's
// frozen harness; an area reports a harness problem instead of editing it). The doll (rpg/art/doll/doll.h) measured
// from its atlases, its pool and a census of looks.
//   rpg_test --doll [--gates] [--parity] [--report] [--pool FILE] [--dump DIR] [--frames DIR] [--limbs DIR [--limbs-looks FILE]]
//               [--limbs-selftest DIR] [--limbs-synth FILE DIR]
//   rpg_test --doll-coverage [--seeds A..B] [--scope civil|all] [--gates] [--pool FILE] [--report]
//   rpg_test --doll-variety [--seeds A..B] [--gates] [--report]
// --doll always (CI): the contract: the pool parses, the frozen tables (slots, regions, layers, anchors, roles, hair styles
//   24 from the 8 families, the names), the A3 palettes against docs/ref/style_a3/palettes.json's colours, rampA3 giving
//   A3's hand ramps back for their bases, the walk table's defaults (man 19, woman 20, run 36 px), the look fields (v1 and
//   rig 1 keys ignore them; rig 2 keys see them), the elder's height, an empty pool covering nothing.
// --parity: B1's pixels: the A3 hero and woman composed with A3's palettes against docs/ref/style_a3/frames: the 16 kept
//   frames (8 idles, 8 swing keys) at 0 px differing; the 48 walk frames reported for the review (redrawn 2026-10-19 for
//   the owner with the idle's leg mass: B11 gates them). --dump DIR writes each frame beside its reference (x4).
// B11 leg mass (with --gates; the owner's walk fix, 2026-10-19): every walk and run frame of the A3 hero and woman against
//   the idle of its facing, on the legs and the garments composed apart: the legs visible under the hem 90 % of the
//   idle's or more; the hip row (the first visible leg row) at most 1 px narrower than the idle's; no visible leg run
//   under 3 px (no sliver of a leg); the hip row within 1 px of the clip's contact frame (round 3: no squat); the walk's
//   down frames no wider than its contact; the woman's skirt 90 % of the idle's or more; also the woman in shirt and trousers
//   (the shared legs and her own overrides under her tunic) and her bare-leg base. --report prints every frame.
// B11 limbs and B12 boots (with --gates; docs/MOTION_P2.md 9, the owner's motion reference 2026-10-20): B11 above keeps
//   the front and back (D, U) walks; the SIDE (R, L) walk and run frames are measured as limbs on a probe twin (the
//   legs' regions T/K S B O composed in probe colours, the raw composite, so every pixel names its region) and the legs
//   composed with the garments over them (the visible leg). Coordinates are u: columns from the pivot, forward + (the
//   left view mirrored, u = -x - 1, so both facings share one table); rows above the sole row F. Per frame:
//   1 visible leg area >= 89.5 % of the same facing's idle; 2 hip row (the first visible row of 3 px or more) at most
//   1 px narrower than the idle's; 3 no visible leg run under 3 px that holds no boot or sole pixel; 4 the hip row
//   within 1 row of the contact frame's; 5 the walk's down frames no wider than its contact; 6 continuity: the legs one
//   4-connected piece, no empty row; 6b joints: every boot (B+O, 4-connected) touches a shin (S), every shin touches a
//   boot and a thigh (T/K), every thigh reaches the seat (the drawing's top row): a floating, glued or stub-hung boot
//   fails; 7 ground: a sole (O) pixel on row F in every walk frame and run contact/push, none in the run's flight;
//   8 registration: a sole run on row F whose heel end (least u) lies within 1 px of the clip's plant0 (L: plant0 - 1
//   in u); 9 envelope (adult walk u in [-7, 9] / [-8, 9] / [-10, 9] (fix r3: the pass's swing K3 behind the planted heel), run
//   [-9, 9] / [-7, 9] / [-9, 9]; youth [-7, 7]); 10 stance (u span of the legs)
//   within 1 of the table (adult walk 16 14 9, run 19 9 17; youth walk 14 12 7); 11 the default hero's trouser body
//   (Cloth2 step 1) luma >= 52 (once); 12 the feet on the table: each table foot (key, u span, lift) matched to a sole
//   by overlap; a whole sole must be that key (the run kick's K3 fallback allowed), its lift and its span +/-1 an end; a
//   covered (short) sole only overlaps (a hidden two-step piece must tilt the table's way: toe-up K1, heel-up K2-K4);
//   a foot with no sole fails; 13 the phase's shape (adults; the harness trace.py's measures): the legs traced from their
//   own soles, contact frames apart (background) from the table's row down where both legs have pixels, the walk pass's
//   and run push's swing knee ahead of the planted shin on its own pixels (each row's runs split where the near leg's
//   outline, a step-0 thigh or shin pixel between shaded ones, divides two legs; 3+ px of its own; 2 px pass, 3 push),
//   the push's swing shin folded back to the table's ankle, the flight's lead knee a row above the trailing knee, its
//   shin within 1 px of vertical, neither knee on its hip-ankle line. Long-garment looks (shoe tips) take 1, 9 and the
//   skirt (>= 89.5 % of the idle's).
//   B12 boots: every sole (O, 8-connected) one clean straight line: one pixel a column, every step 1 row the same way,
//   the runs one of the five keys (K0 flat one run; K1 toe-up runs of 2 (1:2: 3,2,2; the youth and child also 4 and 3
//   until R1-08); K2 heel-up two runs of 4 and 3 (equal +/-1); K3 runs
//   of 2, K4 runs of 1, both heel up; a shorter run only at an end), its length the key's (adult 7 7 7 6-7 5-6, youth
//   and child 6 6 6 5-6 4-5; longer = stretched; shorter only beside an occluding leg or a second sole: a short uneven
//   two-step piece is the visible part of a tilted sole behind the other leg, judged that way); a lone boot's
//   pixels within 2 of the look's flat boot (more = enlarged; fewer only where another leg covers it); 1 or 2 boots a
//   frame; and the stamped leg parts in the pool tagged `boots-stamped` with a view for every side walk/run key their
//   clips name. B11 selftest: synthetic frames (a clean leg, a floating boot, a boot glued to a thigh, a jagged, a
//   stretched, a squashed and a doubled sole, a lone bent-toe boot, a boot beside and a lifted boot over another, a 2 px
//   sliver, a hidden and an uncovered two-step piece) through the same measures, each failing exactly where it should,
//   both facings; and synth.py's frames (synthFrames: the plan's walk and run drawn from targets.json and the boot stamps,
//   and the broken variants) through checks 1-13: the plan's frames 0 faults (its pass, push and flight reported until
//   their re-plan lands), the straight flight, the column push and the hidden pass failing 13, the flat contact 12.
//   --limbs-selftest DIR writes the embedded synthetic looks; --limbs-synth FILE DIR measures synth.py's file into the
//   harness's looks (run_all.sh's J2 SELFTEST: trace.py, selftest.py).
//   --report prints every frame's numbers; --limbs DIR [--limbs-looks FILE] writes the harness's dumps (.tmp/judge/motion:
//   per look <tag>.json with every side idle, walk and run frame's region grid, visible mask, steps, plants, bob and
//   measures, and <tag>_<gait>_<F><i>.png, the figure at 1x in its palette, clear = #FF00FF).
// --gates: B1 parity; B2 heights (every body set, people, age and build in every facing: STYLE_P2 3); faces and hands
//   (A3's face rows and the 3 x 2 hands, from the marks); B8 craft on composed frames (no orphan, no #000000, step-0 luma
//   70 or less, 48 colours or fewer per figure); B9's structure (no civilian garment part shared by the man and the
//   woman); B10 atlases (LOCO 128, RUN 64, DEATH 52 (polish i1: the corpse's legs lie with it), POSTURE 64 KB per look) and speed (one facing's LOCO bake p95 at or
//   under the v1 sheet's), the pool (1.0 MB or less); the ramp generator within delta E 6 per step of A3's 14 ramps; B11.
// --doll-coverage: B6: the census's looks (A the world's humans after 60 frames and the start region's census residents;
//   B every culture of the seeds x every role x both sexes x child, youth, adult and elder x 8 seeds; D the creator's
//   hair, beard, build, people, skin and sex, with and without each homeland; E the party, story people and statues as the
//   world holds them), the share whose LOCO, DEATH (RUN when it may run, ACTION when armed) compose with nothing missing,
//   the missing values by count, and the dead parts. --scope civil (default) leaves out armour, helms, shields and the
//   culture weapon forms (P2c's). --gates: 100 % and 0 dead parts. (P2b fix r2) A census resident is dressed by the game's
//   resident path (Game::dressAsResident). The dead parts count every recipe the game draws on the doll at rig 2: the
//   coverage sources plus the roles (Game::makeLook), the towns, every archetype's musicians, the companions and the
//   creator in worn gear, each look in the postures the game gives it (Reach, posesOf below); --scope civil lists the
//   armour roles' parts and the parts only P2c-held looks want (armour, action families) as "P2c part"; --report names
//   each counted source and why a dead part is not drawn.
// --doll-variety: B7: per settlement of the seeds' start regions, the distinct head recipes (head type, nose, brows, hair
//   style and colour, beard, headwear), the largest group sharing one, the hair styles and beards seen, head types per
//   body, civilian garments per body. --gates: no recipe shared by more than 2 folk of a village or 3 % of a town or city;
//   18 or more of the 24 styles and all 9 beards; every head type on each body; every civilian garment on each body.
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/art/doll/doll_envelope.h"
#include "rpg/art/doll/doll.h"
#include "rpg/art/doll/doll_internal.h"   // (B6) withPosture, the pool's part roles
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/gear_look.h"
#include "rpg/sim/life.h"
#include "tools/tests/doll_motion.h"
#include "tools/tests/tests.h"

namespace dollmotion { void setMapsRoot(const std::string& dir); }   // test_doll_motion.cpp (--maps)

namespace census {
struct Rank { bool royal = false, capital = false; };
void dress(art::HumanLook& L, std::string& name, Role r, bool female, const cult::Culture& C, const cult::Culture& owner, uint64_t seed,
           const Rank& rank);
}  // namespace census

// (P2c SLICE-DOLL) the gear gates C3-C6 and the slice looks (test_doll_arms.cpp)
int dollArmsGates(bool report, std::vector<std::string>& printed);
std::string dollSliceLooksText();
int dollLadderGates(bool report, std::vector<std::string>& printed);   // test_doll_ladder.cpp
int dollLadderCoverage(int argc, char** argv);   // test_doll_ladder.cpp: --doll-coverage --scope ladder (C1 source L)
std::vector<std::pair<std::string, art::HumanLook>> dollSpecLooks(const std::string& text);   // test_doll_envelope.cpp (D79)
std::string dollSourceLText(int a, int b);
std::string dollSliceLooksText();   // test_doll_arms.cpp
std::string dollArmsLooksText();    // test_doll_arms.cpp
int dollForgeGates(bool report, std::vector<std::string>& printed);   // test_doll_forge.cpp: C16 a b c s, C17, C1 F, C12a
int dollForgeCoverage(int argc, char** argv);   // test_doll_forge.cpp: --doll-coverage --scope forge
int dollEnvelopeGates(bool report, std::vector<std::string>& printed);   // test_doll_envelope.cpp: C12f C3b C13 B10 by tier, C19
int dollEnvelopeCoverage(int argc, char** argv);   // test_doll_envelope.cpp: --doll-coverage --scope envelope
namespace {

using art::HumanLook;
using art::RigPart;
using art::RigSheet;
using art::Clip;
namespace doll = art::doll;

bool g_report = false;
struct Gate {
  const char* id;
  int checks = 0, fails = 0;
  std::string first;
  void ok() { checks++; }
  void bad(const std::string& why) {
    checks++;
    if (!fails) first = why;
    if (g_report || fails < 3) std::printf("  %s: %s\n", id, why.c_str());
    fails++;
  }
};
int g_contract = 0;
void cfail(const std::string& s) { std::printf("FAIL: doll contract: %s\n", s.c_str()); g_contract++; }
std::string fmt(const char* f, double a = 0, double b = 0, double c = 0, double d = 0) {
  char buf[320];
  std::snprintf(buf, sizeof buf, f, a, b, c, d);
  return buf;
}
std::string src(const char* rel) {
#ifdef EMB_SOURCE_DIR
  return std::string(EMB_SOURCE_DIR) + "/" + rel;
#else
  return rel;
#endif
}
float lumaOf(uint32_t c) { return (c & 255) * 0.299f + (c >> 8 & 255) * 0.587f + (c >> 16 & 255) * 0.114f; }
uint32_t hexc(uint32_t rgb) { return 0xFF000000u | (rgb & 0xFF) << 16 | (rgb & 0xFF00) | (rgb >> 16 & 0xFF); }

// ---------------------------------------------------------------- the A3 looks (the slice's hero and woman)
HumanLook a3Hero() {
  HumanLook L;
  L.body = 0; L.hair = art::Hair::Short; L.hairVar = 2;   // tousled
  L.neck = 1; L.weapon = 1; L.outfit = art::Outfit::Tunic;
  L.skin = hexc(0xE9A077); L.hairColor = hexc(0xAA4A2C); L.topColor = hexc(0x28728C); L.bottomColor = hexc(0x4F4B6B); L.tabardColor = hexc(0xC83E38);
  L.eyeColor = hexc(0x3A78B0);
  return L;
}
HumanLook a3Woman() {
  HumanLook L;
  L.body = 1; L.hair = art::Hair::Braids; L.hairVar = 0;   // braid
  L.outfit = art::Outfit::Dress;
  L.skin = hexc(0xEEAA86); L.hairColor = hexc(0xC79346); L.topColor = hexc(0x922F60); L.tabardColor = hexc(0x28728C);
  L.eyeColor = hexc(0x3F8A5A);
  return L;
}
doll::DollRecipe a3Recipe(int who) {
  doll::DollRecipe r = doll::resolve(who ? a3Woman() : a3Hero());
  r.ramps = doll::a3Palette(who);
  return r;
}

// ---------------------------------------------------------------- frames
struct Fr { Canvas c; int px = 0, py = 0; const art::RigAnchors* an = nullptr; bool ok = false; };
Fr cut(const RigSheet& s, int facing, Clip clip, int variant, int i) {
  Fr f;
  const art::FrameRect* r = s.frame(facing, clip, variant, i);
  if (!r || r->w <= 0 || r->h <= 0 || s.atlas.w <= 0) return f;
  f.c = Canvas(r->w, r->h);
  for (int y = 0; y < r->h; y++) for (int x = 0; x < r->w; x++) f.c.set(x, y, s.atlas.get(r->x + x, r->y + y));
  f.px = r->px; f.py = r->py;
  f.an = s.anchorsOf(facing, clip, variant, i);
  f.ok = true;
  return f;
}
bool box(const Canvas& c, int& x0, int& y0, int& x1, int& y1) {
  x0 = y0 = 1 << 20; x1 = y1 = -1;
  for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) if (c.get(x, y) >> 24) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); }
  return x1 >= 0;
}

// ---------------------------------------------------------------- parity (B1)
struct ParityFrame { int who; const char* name; RigPart part; int facing; Clip clip; int index; bool kept; };
std::vector<ParityFrame> parityFrames() {
  std::vector<ParityFrame> v;
  const char* F = "DURL";
  for (int who = 0; who < 2; who++)
    for (int f = 0; f < 4; f++) {
      v.push_back({who, nullptr, RigPart::Loco, f, Clip::Idle, 0, true});
      for (int i = 0; i < 6; i++) v.push_back({who, nullptr, RigPart::Loco, f, Clip::Walk, i, false});
    }
  for (int f : {2, 0}) {
    v.push_back({0, nullptr, RigPart::Action, f, Clip::Windup, 0, true});
    v.push_back({0, nullptr, RigPart::Action, f, Clip::Strike, 0, true});
    v.push_back({0, nullptr, RigPart::Action, f, Clip::Follow, 0, true});
    v.push_back({0, nullptr, RigPart::Action, f, Clip::Follow, 1, true});
  }
  (void)F;
  return v;
}
std::string refName(const ParityFrame& p) {
  const char F = "DURL"[p.facing];
  std::string n = p.who ? "woman_" : "hero_";
  if (p.part == RigPart::Action) {
    const int k = p.clip == Clip::Windup ? 0 : p.clip == Clip::Strike ? 1 : 2 + p.index;
    return n + "SWING_" + F + std::to_string(k);
  }
  if (p.clip == Clip::Idle) return n + "IDLE_" + F;
  return n + "WALK_" + F + std::to_string(p.index);
}
void writeSideBySide(const std::string& path, const Canvas& a, int apx, int apy, const std::vector<uint32_t>& ref, int rw, int rh, int rpx, int rpy) {
  const int k = 4, gap = 2;
  const int W = (std::max(apx, rpx) + std::max(a.w - apx, rw - rpx)), H = std::max(apy, rpy) + std::max(a.h - apy, rh - rpy);
  std::vector<uint32_t> img((size_t)(W * 3 + gap * 2) * k * H * k, 0xFF2A2026u);
  const int ox = std::max(apx, rpx), oy = std::max(apy, rpy);
  auto put = [&](int panel, int x, int y, uint32_t c) {
    for (int dy = 0; dy < k; dy++)
      for (int dx = 0; dx < k; dx++) img[(size_t)(y * k + dy) * (W * 3 + gap * 2) * k + (size_t)(panel * (W + gap) + x) * k + dx] = c;
  };
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const int ax = x - ox + apx, ay = y - oy + apy, rx = x - ox + rpx, ry = y - oy + rpy;
      const uint32_t ca = ax >= 0 && ay >= 0 && ax < a.w && ay < a.h ? a.get(ax, ay) : 0;
      const uint32_t cr = rx >= 0 && ry >= 0 && rx < rw && ry < rh ? ref[(size_t)ry * rw + rx] : 0;
      if (ca >> 24) put(0, x, y, ca);
      if (cr >> 24) put(1, x, y, cr);
      if ((ca >> 24 || cr >> 24) && ca != cr) put(2, x, y, 0xFF3C3CFFu);
      else if (ca >> 24) put(2, x, y, 0xFF505050u);
    }
  writePng(path.c_str(), (W * 3 + gap * 2) * k, H * k, img);
}
// returns the kept frames' differing pixel total (and prints); -1 if a frame is missing
// (2026-10-19, the owner's walk fix: "the walking looks bad ... his legs all of a sudden get super skinny". The walk and
// run were redrawn with the idle's leg mass, so the walk frames are no longer A3's: they are reported for the review and
// gated by B11 leg mass instead. The idles and the swing keys stay at 0 px; the swing's side keys stand on `stance`, A3's
// contact legs kept pixel for pixel.)
int parity(Gate& g, const char* dumpDir) {
  RigSheet sheets[2][4];
  for (int who = 0; who < 2; who++) {
    const doll::DollRecipe r = a3Recipe(who);
    for (RigPart p : {RigPart::Loco, RigPart::Action}) sheets[who][(int)p] = doll::compose(r, p);
  }
  int keptDiff = 0, review = 0, reviewDiff = 0;
  for (const ParityFrame& p : parityFrames()) {
    const std::string nm = refName(p);
    int rw = 0, rh = 0;
    std::vector<uint32_t> ref;
    if (!readPng(src(("docs/ref/style_a3/frames/" + nm + ".png").c_str()).c_str(), rw, rh, ref)) { g.bad(nm + ": cannot read the reference"); continue; }
    const int rpx = rw == 24 ? 12 : rw == 44 ? 22 : 24, rpy = rw == 24 ? 34 : 42;
    const Fr f = cut(sheets[p.who][(int)p.part], p.facing, p.clip, 0, p.index);
    if (!f.ok || sheets[p.who][(int)p.part].count(p.facing, p.clip) <= p.index) {
      if (p.kept) g.bad(nm + ": the doll has no such frame");
      continue;
    }
    // (P2c ladder i1, the owner 2026-10-23: "the sword swoosh is oversized and drops below the feet facing down": the
    // steel arc was re-cut smaller, so the strike keys compare the figure without its arc: the doll's arc pixels (the
    // probe's fx item) and A3's (its colours where the doll's figure is clear) are left out; every other pixel stays 0)
    const bool strike = p.clip == Clip::Strike;
    std::vector<uint8_t> fxMask;
    std::vector<uint32_t> arcCols;
    if (strike) {
      const doll::FrameProbe q = doll::probeFrame(a3Recipe(p.who), p.part, p.facing, p.clip, 0, p.index);
      fxMask.assign((size_t)f.c.w * f.c.h, 0);
      if (q.ok)
        for (int y = 0; y < q.h; y++)
          for (int x = 0; x < q.w; x++) {
            const int o = q.owner[(size_t)y * q.w + x];
            if (o < 0 || !q.items[(size_t)o].has(doll::Role::Fx)) continue;
            const int ax = f.px + (x - q.px), ay = f.py + (y - q.py);
            if (ax >= 0 && ay >= 0 && ax < f.c.w && ay < f.c.h) fxMask[(size_t)ay * f.c.w + ax] = 1;
          }
    }
    auto figAt = [&](int ax, int ay) {
      if (ax < 0 || ay < 0 || ax >= f.c.w || ay >= f.c.h) return false;
      return (f.c.get(ax, ay) >> 24) != 0 && !(strike && fxMask[(size_t)ay * f.c.w + ax]);
    };
    // the union of both boxes aligned at the pivots
    int diff = 0;
    const int x0 = -std::max(f.px, rpx), x1 = std::max(f.c.w - f.px, rw - rpx), y0 = -std::max(f.py, rpy), y1 = std::max(f.c.h - f.py, rh - rpy);
    if (strike)
      for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
          const int rx = rpx + x, ry = rpy + y;
          const uint32_t cr = rx >= 0 && ry >= 0 && rx < rw && ry < rh ? ref[(size_t)ry * rw + rx] : 0;
          if ((cr >> 24) && !figAt(f.px + x, f.py + y) && std::find(arcCols.begin(), arcCols.end(), cr) == arcCols.end()) arcCols.push_back(cr);
        }
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) {
        const int ax = f.px + x, ay = f.py + y, rx = rpx + x, ry = rpy + y;
        uint32_t ca = ax >= 0 && ay >= 0 && ax < f.c.w && ay < f.c.h ? f.c.get(ax, ay) : 0;
        uint32_t cr = rx >= 0 && ry >= 0 && rx < rw && ry < rh ? ref[(size_t)ry * rw + rx] : 0;
        if (!(ca >> 24)) ca = 0;
        if (!(cr >> 24)) cr = 0;
        if (strike) {
          if (!figAt(ax, ay)) continue;   // the doll's arc, or clear (A3's arc there)
          if (cr && std::find(arcCols.begin(), arcCols.end(), cr) != arcCols.end()) continue;   // A3's arc over the figure
          // the outline beside either arc (the finishing pass inks an edge by what lies next to it)
          bool besideArc = false;
          for (int dy = -1; dy <= 1 && !besideArc; dy++)
            for (int dx = -1; dx <= 1 && !besideArc; dx++) {
              const int bx = ax + dx, by = ay + dy, qx = rx + dx, qy = ry + dy;
              const bool docArc = bx >= 0 && by >= 0 && bx < f.c.w && by < f.c.h && fxMask[(size_t)by * f.c.w + bx] && (f.c.get(bx, by) >> 24);
              const uint32_t cq = qx >= 0 && qy >= 0 && qx < rw && qy < rh ? ref[(size_t)qy * rw + qx] : 0;
              const bool a3Arc = (cq >> 24) && !figAt(bx, by);
              besideArc = docArc || a3Arc;
            }
          if (besideArc) continue;
        }
        diff += ca != cr;
      }
    if (dumpDir) writeSideBySide(std::string(dumpDir) + "/" + nm + ".png", f.c, f.px, f.py, ref, rw, rh, rpx, rpy);
    if (p.kept) {
      keptDiff += diff;
      if (diff) g.bad(nm + fmt(": %.0f px differ from A3", diff)); else g.ok();
    } else {
      review++;
      reviewDiff += diff ? 1 : 0;
      if (g_report) std::printf("  B1 review %s: %d px differ from A3 (the redrawn walk: judged by B11 and beside A3's GIF)\n", nm.c_str(), diff);
    }
  }
  std::printf("  B1 parity: kept frames (idles, swing keys) %d px differing in all; %d walk frames for review (%d differ from A3)\n", keptDiff, review, reviewDiff);
  return keptDiff;
}

// ---------------------------------------------------------------- the contract (always)
void contract() {
  using doll::Slot;
  // (P2c ladder ADD: Slot::Blade after Fx; the finish regions R P G N W after Edge)
  if (doll::SLOT_COUNT != 22 || std::string(doll::slotName(Slot::Fx)) != "fx" || std::string(doll::slotName(Slot::Skin)) != "skin" ||
      std::string(doll::slotName(Slot::Blade)) != "blade") cfail("the slot table");
  if (doll::LAYER_COUNT != 24 || std::string(doll::layerName(doll::Layer::Fx)) != "fx" || std::string(doll::layerName(doll::Layer::FxBack)) != "fxBack") cfail("the layer table");
  if (doll::ANCHOR_COUNT != 9 || std::string(doll::anchorName(doll::Anchor::Crown)) != "crown" || doll::REGION_COUNT != 17 || doll::regionChar(doll::Region::Sole) != 'O' || doll::regionChar(doll::Region::Trim) != 'R' ||
      doll::regionChar(doll::Region::Wear) != 'W') cfail("the anchors and regions");
  if (doll::ROLE_COUNT != 26 || std::string(doll::roleName(doll::Role::Hair)) != "hair") cfail("the role table");
  if (doll::code(Slot::Metal, 4) != 2 + 11 * 5 + 4 || doll::codeSlot(doll::code(Slot::Cloth3, 3)) != Slot::Cloth3 || doll::codeStep(doll::code(Slot::Cloth3, 3)) != 3) cfail("pixel codes");
  // hair: 24 styles from the 8 families, every name distinct
  std::set<std::string> names;
  int n = 0;
  for (int f = 0; f < 8; f++) {
    n += doll::hairVariants((art::Hair)f);
    for (int v = 0; v < doll::hairVariants((art::Hair)f); v++) names.insert(doll::hairStyleName(doll::hairStyleOf((art::Hair)f, (uint8_t)v)));
  }
  if (n != 24 || names.size() != 24) cfail(fmt("hair: %.0f variants, %.0f distinct styles (24)", n, (double)names.size()));
  if (std::string(doll::hairStyleName(doll::hairStyleOf(art::Hair::Short, 2))) != "tousled" || doll::hairStyleOf(art::Hair::Bald, 9) != doll::hairStyleOf(art::Hair::Bald, 1))
    cfail("hairStyleOf: Short 2 is A3's tousled; a variant past the family clamps");
  if (std::string(doll::headTypeName(1, 1)) != "heart" || std::string(doll::beardName(6)) != "moustache" || std::string(doll::neckName(1)) != "scarf") cfail("the vocabulary");
  // the A3 palettes are A3's colours (docs/ref/style_a3/palettes.json via pal.py; spot checks over every slot)
  const doll::DollRamps h = doll::a3Palette(0), w = doll::a3Palette(1);
  struct C { const doll::DollRamps* r; Slot s; int step; uint32_t hex; };
  const C cs[] = {{&h, Slot::Ink, 0, 0x21152A}, {&h, Slot::Face, 0, 0x24142A}, {&h, Slot::Face, 1, 0xF6EEE3}, {&h, Slot::Eyes, 2, 0x3A78B0},
                  {&h, Slot::Eyes, 3, 0x8FC6E8}, {&h, Slot::Face, 2, 0x8E3A40}, {&h, Slot::Face, 4, 0xF0987E}, {&h, Slot::Hair, 0, 0x3A1622},
                  {&h, Slot::Skin, 4, 0xFFE6C2}, {&h, Slot::Cloth1, 3, 0x3E9FA7}, {&h, Slot::Accent, 3, 0xF47650}, {&h, Slot::Leather, 4, 0xE7B672},
                  {&h, Slot::Cloth2, 3, 0x777493}, {&h, Slot::Metal, 4, 0xFFFFFF}, {&h, Slot::Metal2, 1, 0x704020}, {&h, Slot::Fx, 2, 0xFFFDF4},
                  {&w, Slot::Face, 0, 0x2A1424}, {&w, Slot::Face, 1, 0xF7EFE4}, {&w, Slot::Face, 3, 0xD46470}, {&w, Slot::Eyes, 2, 0x3F8A5A},
                  {&w, Slot::Hair, 4, 0xFFEBA8}, {&w, Slot::Cloth1, 0, 0x2C1030}, {&w, Slot::Linen, 3, 0xFAF6EE}, {&w, Slot::Cloth3, 0, 0x4E3A30},
                  {&w, Slot::Accent, 1, 0x1F4A68}, {&w, Slot::Accent, 3, 0x3E9FA7}};
  for (const C& c : cs)
    if ((c.r->c[(int)c.s][c.step] | 0xFF000000u) != hexc(c.hex)) cfail(std::string("a3Palette: ") + doll::slotName(c.s) + fmt(" step %.0f", c.step));
  // rampA3 gives A3's hand ramps back for their bases (the curated tables' first entries)
  for (const doll::A3Ramp& r : doll::a3Ramps()) {
    if (r.steps != 5) continue;
    uint32_t o[5];
    doll::rampA3(r.c[2], r.material, o);
    for (int i = 0; i < 5; i++) if ((o[i] | 0xFF000000u) != (r.c[i] | 0xFF000000u)) { cfail(std::string("rampA3 does not give back A3's ") + r.name); break; }
  }
  if (doll::a3Ramps().size() != 14) cfail("A3's 14 hand ramps");
  // the walk tables' defaults (no clip yet: frames 0 and the strides of the plan)
  const doll::WalkTable wm = doll::walkTable(doll::BodySet::Man, doll::Gait::Walk), ww = doll::walkTable(doll::BodySet::Woman, doll::Gait::Walk),
                        wr = doll::walkTable(doll::BodySet::Man, doll::Gait::Run);
  if (!wm.frames && (wm.cycle != 19 || ww.cycle != 20 || wr.cycle != 36)) cfail("walkTable defaults: man 19, woman 20, run 36 px");
  if (doll::kGaitSwitch != 54.0f) cfail("the gait switch 54 px/s");
  // the look fields: v1 never sees them; rig 1's keys ignore them (and youth / elder key as the adult); rig 2's see them
  const int was = art::rigVersion();
  art::setRigVersion(1);
  HumanLook a, b;
  b.head = 3; b.nose = 2; b.brow = 1; b.marks = 5; b.hairVar = 1; b.beardStyle = 4; b.neck = 2; b.garb = 2;
  if (a.key() != b.key()) cfail("rig 1: the doll's look fields move key()");
  if (art::humanSheet(a).px != art::humanSheet(b).px) cfail("v1 paints the doll's look fields");
  HumanLook ye = a; ye.age = 3;
  HumanLook yy = a; yy.age = 2;
  if (ye.key() != a.key() || yy.key() != a.key()) cfail("rig 1: youth and elder key as the adult v1 paints");
  HumanLook ch = a; ch.age = 1;
  if (ch.key() == a.key()) cfail("rig 1: the child keeps its own key (P2a)");
  art::setRigVersion(2);
  if (a.key() == b.key() || ye.key() == a.key()) cfail("rig 2: the doll's look fields and the elder move key()");
  art::setRigVersion(was);
  if (art::rig(2).figH(0, 3, 0) != 31 || art::rig(1).figH(0, 3, 0) != 22 || art::rig(2).figH(1, 3, 2) != 31) cfail("figH: the elder a pixel under the adult at rig 2, v1's adult at rig 1");
  if (art::rig(2).eyeRow != 22 || art::rig(2).headTop != 31) cfail("RigMetrics r2: A3's rows (crown F-31, eyes F-22)");
  if ((int)art::RigPart::Run != 3 || (int)art::Posture::Row != (int)art::Posture::Kneel + 1) cfail("RigPart::Run and Posture::Row appended");
  // the pool parses; an empty pool covers nothing (rig 2 draws v1 whole)
  const doll::PoolStats ps = doll::poolStats();
  std::printf("  pool: %d parts, %d views, %d clips, %d frames, %d body sets; %zu bytes (%s%s)\n", ps.parts, ps.views, ps.clips, ps.frames, ps.bodySets, ps.bytes,
              ps.embedded ? "embedded, hash " : "loaded", ps.hash.c_str());
  if (!ps.bytes) cfail("the pool does not parse");
  if (!ps.parts && (doll::dollCovers(a3Hero(), RigPart::Loco) || !doll::compose(a3Hero(), RigPart::Loco).empty())) cfail("an empty pool covers a look");
}

// ---------------------------------------------------------------- the gates (rig 2)
HumanLook bodyLook(int body, int build, int people, int age) {
  HumanLook L = body ? a3Woman() : a3Hero();
  L.weapon = 0;
  L.build = (uint8_t)build; L.people = (uint8_t)people; L.age = (uint8_t)age;
  return L;
}
void gateHeights(Gate& g) {   // B2 (STYLE_P2 3: man 32, woman 31, youth 26, child 21; elf +1; elder -1)
  for (int body = 0; body < 2; body++)
    for (int age : {0, 1, 2, 3})
      for (int people = 0; people < 3; people++)
        for (int build = 0; build < 3; build++) {
          const HumanLook L = bodyLook(body, build, people, age);
          const RigSheet s = doll::compose(L, RigPart::Loco);
          for (int facing = 0; facing < 4; facing++) {
            const Fr f = cut(s, facing, Clip::Idle, 0, 0);
            const std::string who = std::string(doll::bodySetName(doll::bodySetOf(L))) + fmt(" age %.0f people %.0f build %.0f facing %.0f", age, people, build, facing);
            if (!f.ok) { g.bad(who + ": no LOCO idle frame"); continue; }
            int x0, y0, x1, y1;
            box(f.c, x0, y0, x1, y1);
            const int want = art::rig(2).figH(L.body, L.age, L.people);
            // (the hair may rise 2 px over the crown: STYLE_P2 3; measured to the frame's top)
            const int h = y1 - y0 + 1;
            if (h < want || h > want + 2) g.bad(who + fmt(": %.0f px tall (%.0f, +2 for hair)", h, want)); else g.ok();
            if (y1 != f.py) g.bad(who + fmt(": the sole row %.0f is not the pivot row %.0f", y1, f.py));
          }
        }
}
void gateFaces(Gate& faces, Gate& hands) {   // A3's face (STYLE_P2 4) and hands (5), from the marks
  for (int who = 0; who < 2; who++) {
    const doll::DollRecipe r = a3Recipe(who);
    const RigSheet s = doll::compose(r, RigPart::Loco);
    const float skinL = lumaOf(r.ramps.c[(int)doll::Slot::Skin][2]);
    for (int i = 0; i < std::max(1, s.count(0, Clip::Walk)); i++) {
      const Fr f = i == 0 ? cut(s, 0, Clip::Idle, 0, 0) : cut(s, 0, Clip::Walk, 0, i);
      const std::string w = std::string(who ? "woman" : "hero") + (i ? fmt(" walk D%.0f", i) : std::string(" idle D"));
      if (!f.ok || !f.an || f.an->faceX == art::kNoAnchor) { faces.bad(w + ": no face mark"); continue; }
      auto at = [&](int x, int y) { const int X = f.px + f.an->faceX + x, Y = f.py + f.an->faceY + y; return X >= 0 && Y >= 0 && X < f.c.w && Y < f.c.h ? f.c.get(X, Y) : 0u; };
      int bad = 0;
      for (int col : {1, 2, 7, 8}) if (skinL - lumaOf(at(col, 1)) < 60) bad |= 1;                    // the lash row
      if (lumaOf(at(1, 2)) < skinL && lumaOf(at(8, 2)) < skinL) bad |= 2;                              // eye whites
      if (!(lumaOf(at(5, 3)) < skinL - 8 || lumaOf(at(4, 3)) < skinL - 8)) bad |= 4;                    // the nose pixel
      if (!(lumaOf(at(5, 4)) < skinL - 20 || lumaOf(at(4, 4)) < skinL - 20)) bad |= 8;                  // the mouth
      if (bad) faces.bad(w + ": face missing" + (bad & 1 ? " lashes" : "") + (bad & 2 ? " whites" : "") + (bad & 4 ? " nose" : "") + (bad & 8 ? " mouth" : ""));
      else faces.ok();
      if (who == 0 && (f.an->handRX == art::kNoAnchor || f.an->handLX == art::kNoAnchor)) { hands.bad(w + ": no hand marks"); continue; }
      if (who == 0) {
        int skinPx = 0;
        for (int hnd = 0; hnd < 2; hnd++)
          for (int dy = 0; dy < 2; dy++)
            for (int dx = 0; dx < 3; dx++) {
              const int X = f.px + (hnd ? f.an->handLX : f.an->handRX) + dx, Y = f.py + (hnd ? f.an->handLY : f.an->handRY) + dy;
              const uint32_t p = X >= 0 && Y >= 0 && X < f.c.w && Y < f.c.h ? f.c.get(X, Y) : 0;
              skinPx += (p >> 24) && std::fabs(lumaOf(p) - skinL) < 90;
            }
        if (skinPx < 8) hands.bad(w + fmt(": %.0f of 12 hand pixels in skin", skinPx)); else hands.ok();
      }
    }
  }
}
// (P2c ladder, B8 restated: D18) glow: the look's Glow-slot colours, up to 3 of which a T5-T6 figure may add over the 48
void craftFrame(Gate& g, const Canvas& c, const std::string& who, const std::set<uint32_t>* glow = nullptr) {
  int orphans = 0, black = 0;
  std::set<uint32_t> cols;
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      const uint32_t p = c.get(x, y);
      if (!(p >> 24)) continue;
      cols.insert(p);
      if ((p & 0xFFFFFFu) == 0) black++;
      bool any = false;
      for (int dy = -1; dy <= 1 && !any; dy++)
        for (int dx = -1; dx <= 1 && !any; dx++) {
          if (!dx && !dy) continue;
          const int X = x + dx, Y = y + dy;
          any = X >= 0 && Y >= 0 && X < c.w && Y < c.h && (c.get(X, Y) >> 24);
        }
      orphans += !any;
      if (!any && std::getenv("B8_DEBUG")) std::printf("    orphan %s at %d,%d (canvas %dx%d)\n", who.c_str(), x, y, c.w, c.h);
    }
  if (orphans) g.bad(who + fmt(": %.0f orphan pixel(s)", orphans));
  else if (black) g.bad(who + ": #000000");
  else {
    int glowCols = 0;
    if (glow) for (uint32_t p : cols) glowCols += glow->count(p) ? 1 : 0;
    const int n = (int)cols.size() - std::min(glowCols, 3);
    if (n > 48) g.bad(who + fmt(": %.0f colours (48", (double)n) + (glow ? " + 3 glow)" : ")"));
    else g.ok();
  }
}
void gateCraft(Gate& g) {   // B8 on composed frames: the A3 looks and a census sample
  std::vector<HumanLook> looks{a3Hero(), a3Woman()};
  for (int i = 0; i < 12; i++) looks.push_back(bodyLook(i & 1, i % 3, (i / 3) % 3, i % 4 == 3 ? 3 : 0));
  for (const HumanLook& L : looks) {
    const doll::DollRecipe r = doll::resolve(L);
    for (int s = 0; s < doll::SLOT_COUNT; s++) {
      if (!doll::slotLit((doll::Slot)s)) continue;
      const uint32_t o = r.ramps.c[s][0], b = r.ramps.c[s][2];
      if (lumaOf(o) > 70.5f || lumaOf(b) - lumaOf(o) < 39.5f) { g.bad(std::string(doll::slotName((doll::Slot)s)) + fmt(": step 0 luma %.0f, base %.0f (70 or less, 40 below)", lumaOf(o), lumaOf(b))); break; }
    }
    for (RigPart part : {RigPart::Loco, RigPart::Death, RigPart::Run}) {
      const RigSheet sh = doll::compose(r, part);
      for (int fc = 0; fc < 4; fc++)
        for (int c = 0; c < art::CLIP_COUNT; c++)
          for (int i = 0; i < sh.count(fc, (Clip)c); i++) {
            const Fr f = cut(sh, fc, (Clip)c, 0, i);
            if (f.ok) craftFrame(g, f.c, std::string(doll::bodySetName(r.body)) + " " + art::clipName((Clip)c) + fmt(" f%.0f #%.0f", fc, i));
          }
    }
  }
  // (P2c ladder) the gates' matrix (the ladder looks): 48 colours a figure plus up to 3 Glow steps on T5-T6 looks (D18)
  {
    const std::string t = dollSliceLooksText();
    size_t a = 0;
    while (a < t.size()) {
      size_t e = t.find('\n', a);
      if (e == std::string::npos) e = t.size();
      const std::string line = t.substr(a, e - a);
      a = e + 1;
      char tag[128] = {}, who[16] = {}, spec[1200] = {};
      if (line.empty() || line[0] == '#' || std::sscanf(line.c_str(), "%127s %15s %1199s", tag, who, spec) < 2) continue;
      HumanLook L = !std::strcmp(who, "woman") ? a3Woman() : a3Hero();
      bool lookPal = false;
      doll::applyLookSpec(L, std::strcmp(spec, "-") ? spec : "", &lookPal);
      doll::DollRecipe r = doll::resolve(L);
      if (!lookPal) r.ramps = doll::a3Palette(L.body == 1 ? 1 : 0);
      std::set<uint32_t> glow;
      if (L.topTier() >= 5) for (int k = 0; k < 5; k++) glow.insert(r.ramps.c[(int)doll::Slot::Glow][k] | 0xFF000000u);
      const bool fights = L.weapon >= 1 && L.weapon <= 8 && !L.civil;
      for (RigPart part : {RigPart::Loco, RigPart::Death, RigPart::Run, RigPart::Drawn}) {
        if (part == RigPart::Drawn && !fights) continue;
        const RigSheet sh = doll::compose(r, part);
        for (int fc = 0; fc < 4; fc++)
          for (int c = 0; c < art::CLIP_COUNT; c++)
            for (int v = 0; v < art::RIG_VARIANTS; v++)
              for (int i = 0; i < sh.count(fc, (Clip)c, v); i++) {
                if (v && sh.frame(fc, (Clip)c, v, i) == sh.frame(fc, (Clip)c, 0, i)) continue;
                const Fr f = cut(sh, fc, (Clip)c, v, i);
                if (f.ok) craftFrame(g, f.c, std::string(tag) + " " + art::clipName((Clip)c) + fmt(" v%.0f f%.0f #%.0f", v, fc, i), glow.empty() ? nullptr : &glow);
              }
      }
    }
  }
}
void gateFit(Gate& g) {   // B9's structure: every civilian garment a fitted pair (never one part for both bodies)
  for (int p = 0; p < doll::partCount(); p++) {
    bool garment = false;
    for (const char* gname : {"tunic", "culturetunic", "dress", "robe", "kaftan", "wrap", "coat", "kilt", "poncho", "gown", "rags", "apron", "smock", "jerkin"})
      garment = garment || doll::partHasRole(p, doll::Role::Garment, gname);
    if (!garment) continue;
    if (doll::partServes(p, doll::BodySet::Man) && doll::partServes(p, doll::BodySet::Woman)) g.bad(std::string(doll::partName(p)) + ": one garment part for the man and the woman");
    else g.ok();
  }
}
// (P2c SLICE-DOLL) the slice looks' atlases (B10): per look and part its KB and budget (-1 KB: no atlas)
struct SliceAtlas { std::string who, part; double kb = 0, budget = 0, base = 0; };   // (ladder) base: the part's base budget; budget: D19's for T5-T6
std::vector<SliceAtlas> sliceAtlases() {
  std::vector<SliceAtlas> v;
  const std::string t = dollSliceLooksText();
  size_t a = 0;
  while (a < t.size()) {
    size_t e = t.find('\n', a);
    if (e == std::string::npos) e = t.size();
    const std::string line = t.substr(a, e - a);
    a = e + 1;
    char tag[128] = {}, who[16] = {}, spec[1200] = {};
    if (line.empty() || line[0] == '#' || std::sscanf(line.c_str(), "%127s %15s %1199s", tag, who, spec) < 2) continue;
    HumanLook L = !std::strcmp(who, "woman") ? a3Woman() : a3Hero();
    bool lookPal = false;
    doll::applyLookSpec(L, std::strcmp(spec, "-") ? spec : "", &lookPal);
    doll::DollRecipe r = doll::resolve(L);
    r.ramps = lookPal ? doll::rampsOf(L) : doll::a3Palette(L.body == 1 ? 1 : 0);
    const bool fights = L.weapon >= 1 && L.weapon <= 8 && !L.civil;
    // (P2c ladder, D19) T5-T6 looks may use LOCO 160, RUN 80 and DRAWN 320 KB (reported beside the base budget)
    (void)0;   // (P2c envelope) B10 by tier below (kEnvelope)
    // (ladder i2, integration) two shape allowances, measured: a crest or plume stands 2-3 rows over the helm in every run
    // frame (24 frames: about 4 KB), so a crested look's RUN may use 68 KB; a polearm falls with its wearer, a 40-45 px
    // shaft across the fall and lying frames, so a polearm look's DEATH may use 56 KB. D19 (T5-T6) stays above both
    bool crest = false, polearm = false;
    {
      const auto& P = doll::pool::get();
      for (int pi : r.parts) {
        if (pi < 0 || pi >= (int)P.parts.size()) continue;
        const std::string& nm = P.s(P.parts[(size_t)pi].name);
        // (forge i2) a forge crest (headwear/helm/crest/<v>, which takes the helm's own crest or plume up to T4) stands as high
        if (nm.find("_crest") != std::string::npos || nm.find("_plume") != std::string::npos || nm.find("helm/crest/") != std::string::npos) crest = true;
        if (nm.find("held/weapons/spear") != std::string::npos || nm.find("held/weapons/glaive") != std::string::npos ||
            nm.find("held/weapons/halberd") != std::string::npos) polearm = true;
      }
    }
    // (P2c envelope, B10 by tier: D46, kEnvelope) the figure's highest piece picks the row: T1-T3 and untiered 128 / 64 / 256 /
    // 512 / 52, T4 160 / 80 / 320, T5-T6 200 / 100 / 400 with ACTION and DEATH measured on e6 x 1.1 (the cap until then); the
    // per-item allowances add on top as before (a crest or plume RUN + 4, a polearm LOCO + 8, RUN + 14, DEATH + 4)
    const int tt = L.topTier();
    struct B { RigPart p; int atlas; const char* n; };
    for (const B& b : {B{RigPart::Loco, 0, "LOCO"}, B{RigPart::Run, 1, "RUN"}, B{RigPart::Drawn, 2, "DRAWN"}, B{RigPart::Action, 3, "ACTION"},
                       B{RigPart::Death, 4, "DEATH"}}) {
      if ((b.p == RigPart::Drawn || b.p == RigPart::Action) && !fights) continue;
      const RigSheet sh = doll::compose(r, b.p);
      double base = doll::envelopeAtlasKB(0, b.atlas);
      const double tierKB = doll::envelopeAtlasKB(tt, b.atlas);
      if (b.p == RigPart::Run && crest) base += 4;
      if (b.p == RigPart::Death && polearm) base += 4;
      // (polish i1 integration, measured) the polearm's upright carry in front of the body (the Jade glaive fix, P5) makes
      // the R / L loco frames 2-4 rows taller: jade3 127.5 -> 130.3 KB, knight 121.5 -> 127.5 KB, so a polearm LOCO may use 136
      if (b.p == RigPart::Loco && polearm) base += 8;
      // (polish i2 integration, measured) the polearm's side run carries the haft 45 deg forward through the fist with the head
      // clear of the helm (the Jade glaive fix, P5): jade3 RUN 64 -> 76.5, knight 68 -> 80.9, lt6w 77.2 -> 89.2 KB, so a polearm
      // RUN may use its base (and D19) + 14 KB
      double d19 = tierKB + (base - doll::envelopeAtlasKB(0, b.atlas));   // the tier's row with the same allowances
      if (b.p == RigPart::Run && polearm) { base += 14; d19 += 14; }
      // (P2c forge, PLAN constants "forge: atlases") a styled body's shoulders, crest and edges add a column or row on some
      // frames: its LOCO and RUN may use 3 % more (measured i2: fm_star RUN 68.2 of 68 with its crest, jade3 LOCO 138.6 of 136)
      // (INTEGRATE silhouettes i1, measured) a round-2 silhouette (forgeShape: a skirt 6-11 rows below the belt, wide or heavy
      // sleeves, a mantle or half-cape) widens the side run frames by 2-4 columns: fm_star RUN 77.0, a_fj2 75.2, lt4 74.0 KB,
      // so its LOCO and RUN may use 14 % more (STARFALL's crystal helm over its long split skirt the most: fm_star 77.0 of 68 crested)
      if ((b.p == RigPart::Loco || b.p == RigPart::Run) && L.forgeShape) { base *= 1.14; d19 *= 1.14; }
      else if ((b.p == RigPart::Loco || b.p == RigPart::Run) && L.forge[HumanLook::TIER_BODY]) { base *= 1.03; d19 *= 1.03; }
      v.push_back(SliceAtlas{tag, b.n, sh.empty() ? -1.0 : sh.bytes() / 1024.0, tt >= 4 ? std::max(base, d19) : base, base});
    }
  }
  return v;
}
void gateAtlas(Gate& g) {   // B10
  std::vector<HumanLook> looks;
  for (int i = 0; i < 20; i++) {
    HumanLook L = bodyLook(i & 1, i % 3, (i / 3) % 3, 0);
    L.hair = (art::Hair)(i % 8); L.hairVar = (uint8_t)(i % 3); L.topColor = rgba(40 + i * 9, 90, 160 - i * 4);
    looks.push_back(L);
  }
  // (P2b fix r1, B10-ARMED-RUN-ATLAS) armed looks too: the A3 hero with his sword and an armed woman, every build; their
  // RUN holds the sheathed run only (the drawn run is ACTION's walk variant 2), and ACTION stays in its 512 KB
  const size_t nUnarmed = looks.size();
  for (int i = 0; i < 6; i++) {
    HumanLook L = bodyLook(i & 1, i % 3, (i / 2) % 3, 0);
    L.weapon = 1;
    looks.push_back(L);
  }
  looks.push_back(a3Hero());
  std::vector<double> v1ms, locoMs;
  for (size_t li = 0; li < looks.size(); li++) {
    const HumanLook& L = looks[li];
    const bool armed = li >= nUnarmed;
    auto t0 = std::chrono::steady_clock::now();
    const Canvas s1 = art::humanSheet(L);
    v1ms.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    const doll::DollRecipe r = doll::resolve(L);
    doll::ComposeOpts one;
    one.onlyFacing = 2;
    t0 = std::chrono::steady_clock::now();
    const RigSheet lo1 = doll::compose(r, RigPart::Loco, &one);
    locoMs.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    (void)s1; (void)lo1;
    struct B { RigPart p; size_t kb; const char* n; };
    for (const B& b : {B{RigPart::Loco, 128, "LOCO"}, B{RigPart::Run, 64, "RUN"}, B{RigPart::Death, 52, "DEATH"}, B{RigPart::Drawn, 256, "DRAWN"},
                       B{RigPart::Action, 512, "ACTION"}}) {
      if ((b.p == RigPart::Action || b.p == RigPart::Drawn) && !armed) continue;
      const RigSheet s = doll::compose(r, b.p);
      const std::string who = std::string(armed ? "armed " : "") + doll::bodySetName(r.body) + fmt(" look %.0f ", (double)li);
      if (s.empty()) { g.bad(who + b.n + ": no atlas"); continue; }
      if (s.bytes() > b.kb * 1024) g.bad(who + b.n + fmt(" %.1f KB (%.0f)", s.bytes() / 1024.0, (double)b.kb)); else g.ok();
      // the drawn run: in DRAWN (P2c; walk variant 2, six frames a facing), never in RUN
      if (armed && b.p == RigPart::Drawn)
        for (int fc = 0; fc < 4; fc++) {
          // (count and frame fall back to variant 0 for a missing variant: the drawn run is its own frames, not the walk's)
          const art::FrameRect* f2 = s.frame(fc, Clip::Walk, 2, 0);
          if (s.count(fc, Clip::Walk, 2) == 6 && f2 && f2 != s.frame(fc, Clip::Walk, 1, 0) && f2 != s.frame(fc, Clip::Walk, 0, 0)) g.ok();
          else g.bad(who + fmt("DRAWN facing %.0f: %.0f drawn-run frames (6 of its own)", fc, s.count(fc, Clip::Walk, 2)));
        }
      if (armed && b.p == RigPart::Run) {   // the sheathed run alone: six walk frames a facing, nothing else
        int run0 = 0;
        for (int fc = 0; fc < 4; fc++) run0 += s.count(fc, Clip::Walk, 0);
        if (run0 == 24 && s.frames.size() == 24) g.ok();
        else g.bad(who + fmt("RUN: %.0f frames, %.0f of them the sheathed run (24, 24)", (double)s.frames.size(), run0));
      }
    }
  }
  // (P2c SLICE-DOLL) the slice looks within LOCO 128, RUN 64, DRAWN 256, ACTION 512 and DEATH 52 KB (fighters: DRAWN and
  // ACTION), each part's atlas composed from the look's recipe
  for (const SliceAtlas& sa : sliceAtlases()) {
    if (sa.kb < 0) { g.bad(sa.who + " " + sa.part + ": no atlas"); continue; }
    if (g_report || (sa.budget > sa.base && sa.kb > sa.base))
      std::printf("  B10 ladder %-8s %-6s %6.1f KB (base %.0f%s)\n", sa.who.c_str(), sa.part.c_str(), sa.kb, sa.base,
                  sa.budget > sa.base ? fmt(", D19 %.0f", sa.budget).c_str() : "");
    if (sa.kb > sa.budget) g.bad(fmt("ladder ") + sa.who + " " + sa.part + fmt(" %.1f KB (%.0f)", sa.kb, sa.budget)); else g.ok();
  }
  if (g_report) {   // every armed body, build, people and age: the largest RUN and ACTION (the sample's worst case)
    double worstRun = 0, worstAct = 0, runContent = 0;
    std::string wr, wa;
    for (int body = 0; body < 2; body++)
      for (int age = 0; age < 4; age++)
        for (int people = 0; people < 3; people++)
          for (int build = 0; build < 3; build++) {
            HumanLook L = bodyLook(body, build, people, age);
            L.weapon = 1;
            const doll::DollRecipe r = doll::resolve(L);
            const RigSheet rs = doll::compose(r, RigPart::Run), as = doll::compose(r, RigPart::Action);
            const std::string nm = std::string(doll::bodySetName(r.body)) + fmt(" age %.0f people %.0f build %.0f", age, people, build);
            if (rs.bytes() / 1024.0 > worstRun) {
              worstRun = rs.bytes() / 1024.0; wr = nm;
              runContent = 0;
              std::vector<std::pair<int, int>> seen;   // (the atlas keeps one copy of a repeated frame)
              for (const art::FrameRect& f : rs.frames) {
                if (std::find(seen.begin(), seen.end(), std::make_pair((int)f.x, (int)f.y)) != seen.end()) continue;
                seen.emplace_back(f.x, f.y);
                runContent += f.w * f.h * 4 / 1024.0;
              }            }
            if (as.bytes() / 1024.0 > worstAct) { worstAct = as.bytes() / 1024.0; wa = nm; }
          }
    std::printf("  B10 armed sweep: worst RUN %.1f KB (%s; frames %.1f KB), worst ACTION %.1f KB (%s)\n", worstRun, wr.c_str(), runContent, worstAct, wa.c_str());
  }
  // (P2b fix B10-CIVIL-STAFF-RUN-ATLAS) the civil trades carry their tool on the doll's LOCO and RUN (HumanLook::civil: the
  // hunter's bow 3, the scholar's and lamplighter's staff 4, the smith's hammer 6, the elder's stick 9): every body, age,
  // people and build with each, plus the widest scholars the census dresses (the conical hat; the legacy hood over
  // pigtails and a robe), within LOCO 128, RUN 64, DEATH 52 and POSTURE 64 KB
  {
    std::vector<HumanLook> civ;
    for (int weapon : {3, 4, 6, 9})
      for (int body = 0; body < 2; body++)
        for (int age = 0; age < 4; age++)
          for (int people = 0; people < 3; people++)
            for (int build = 0; build < 3; build++) {
              HumanLook L = bodyLook(body, build, people, age);
              L.weapon = (uint8_t)weapon; L.civil = 1;
              civ.push_back(L);
            }
    for (int body = 0; body < 2; body++) {
      HumanLook L = bodyLook(body, 0, 1, 0);   // the conical hat (LAISHAN's scholars, seed 3)
      L.weapon = 4; L.civil = 1; L.outfit = art::Outfit::Robe; L.headwear = 7; L.neck = 3; L.cut = 2; L.pattern = 3;
      L.hair = art::Hair::Long; L.hairVar = 0;
      civ.push_back(L);
      L = bodyLook(body, 0, 0, 0);             // the legacy hood over pigtails (JEI #285, seed 16)
      L.weapon = 4; L.civil = 1; L.outfit = art::Outfit::Robe; L.hood = true; L.hair = art::Hair::Ponytail; L.hairVar = 2;
      L.cut = 8; L.head = 1; L.pattern = 3;
      civ.push_back(L);
    }
    double worst[4] = {};
    std::string worstWho[4];
    const char* names[4] = {"LOCO", "RUN", "DEATH", "POSTURE"};
    const double kbs[4] = {128, 64, 52, 64};   // (polish i1: DEATH 52, the corpse's legs lie with it)
    int over = 0;
    for (const HumanLook& L : civ) {
      const doll::DollRecipe r = doll::resolve(L);
      const std::string who = fmt("civil tool %.0f ", L.weapon) + doll::bodySetName(r.body) +
                              fmt(" age %.0f people %.0f build %.0f", L.age, L.people, L.build) + (L.headwear ? " hat" : L.hood ? " hood" : "");
      double kb[4] = {doll::compose(r, RigPart::Loco).bytes() / 1024.0, doll::compose(r, RigPart::Run).bytes() / 1024.0,
                      doll::compose(r, RigPart::Death).bytes() / 1024.0, 0};
      for (int p = 1; p < (int)art::Posture::COUNT; p++)
        if ((art::Posture)p != art::Posture::Sleep) kb[3] = std::max(kb[3], doll::composePosture(r, (art::Posture)p, 0).bytes() / 1024.0);
      for (int i = 0; i < 4; i++) {
        if (kb[i] > worst[i]) { worst[i] = kb[i]; worstWho[i] = who; }
        if (kb[i] > kbs[i]) { g.bad(who + " " + names[i] + fmt(" %.1f KB (%.0f)", kb[i], kbs[i])); over++; }
        else g.ok();
      }
    }
    if (g_report || over)
      std::printf("  B10 civil tool sweep (%zu looks): worst LOCO %.1f (%s), RUN %.1f (%s), DEATH %.1f, POSTURE %.1f KB\n", civ.size(),
                  worst[0], worstWho[0].c_str(), worst[1], worstWho[1].c_str(), worst[2], worst[3]);
  }
  for (int p = 1; p < (int)art::Posture::COUNT; p++) {
    if ((art::Posture)p == art::Posture::Sleep) continue;
    const RigSheet ps = doll::composePosture(looks[0], (art::Posture)p, 0);
    if (ps.empty()) { g.bad(fmt("posture %.0f: no atlas", p)); continue; }
    if (ps.bytes() > 64 * 1024) g.bad(fmt("posture %.0f atlas %.0f KB (64)", p, ps.bytes() / 1024.0)); else g.ok();
  }
  std::sort(v1ms.begin(), v1ms.end());
  std::sort(locoMs.begin(), locoMs.end());
  const double v1p95 = v1ms[v1ms.size() * 95 / 100], lp95 = locoMs[locoMs.size() * 95 / 100];
  std::printf("  B10 bake: v1 sheet p50 %.3f p95 %.3f ms; one facing's LOCO p50 %.3f p95 %.3f ms\n", v1ms[v1ms.size() / 2], v1p95, locoMs[locoMs.size() / 2], lp95);
  if (lp95 > std::max(v1p95, 0.385)) g.bad(fmt("one facing's LOCO bakes in %.3f ms at p95 (the v1 sheet's %.3f; the plan's 0.385)", lp95, v1p95)); else g.ok();
  {   // (P2B_MOTION_ROLLOUT 5.6) the M7 watch list: cloak x hood x staff / stick / sword (GATES-WEAR)
    std::vector<dollmotion::Fail> fs;
    dollmotion::b10Watch(fs, g_report);
    for (const dollmotion::Fail& f : fs) g.bad(f.check + " " + f.where + ": " + f.why);
    if (fs.empty()) g.ok();
  }
  const doll::PoolStats ps = doll::poolStats();
  // (P2c envelope, C8: D46, no compression D48) the pool 2.5 MiB (doll::kPoolBudget)
  if (ps.bytes > doll::kPoolBudget) g.bad(fmt("the pool %.0f B (%.0f: 2.5 MiB, D46)", (double)ps.bytes, (double)doll::kPoolBudget)); else g.ok();
}
void gateRamps(Gate& g) {   // the generator within delta E 6 per step of A3's 14 hand ramps (given their bases)
  float worst = 0;
  std::string worstName;
  for (const doll::A3Ramp& r : doll::a3Ramps()) {
    uint32_t o[5];
    doll::rampGenerate(r.c[2], r.material, o);
    for (int i = 0; i < 5; i++) {
      if (!r.c[i] || i == 2) continue;
      const float e = doll::deltaE76(o[i], r.c[i]);
      if (e > worst) { worst = e; worstName = std::string(r.name) + fmt(" step %.0f", i); }
      if (g_report) std::printf("  ramps %s step %d: dE %.1f\n", r.name, i, e);
      if (e > 6.0f) g.bad(std::string(r.name) + fmt(" step %.0f: delta E %.1f (6)", i, e)); else g.ok();
    }
  }
  std::printf("  ramps: the generator's worst step is %s at delta E %.1f (gate 6)\n", worstName.c_str(), worst);
}

// the walk fix's looks: 0 the A3 hero, 1 the A3 woman (her dress), 2 the woman in shirt and trousers (the shared legs and
// her own leg overrides), 3 her bare-leg base (the legs' thigh, knee and shin regions in skin: shorts, kilts, short hems)
doll::DollRecipe walkLook(int k) {
  if (k < 2) return a3Recipe(k);
  HumanLook L = a3Woman();
  L.outfit = art::Outfit::Tunic;
  L.bottomColor = hexc(0x4F4B6B);
  doll::DollRecipe r = doll::resolve(L);
  r.ramps = doll::a3Palette(1);
  if (k == 3) for (doll::Region g : {doll::Region::Thigh, doll::Region::Knee, doll::Region::Shin}) r.region[(int)g] = (uint8_t)doll::Slot::Skin;
  return r;
}
const char* walkLookName(int k) { static const char* const n[4] = {"hero", "woman", "womant", "womanb"}; return n[k & 3]; }

// B11 leg mass: the recipe split into its legs and its garments (composed apart, so a leg pixel is visible where no
// garment pixel lies over it)
doll::DollRecipe onlyParts(const doll::DollRecipe& full, const char* prefix) {
  doll::DollRecipe r = full;
  r.parts.clear();
  for (int p : full.parts)
    if (std::string(doll::partName(p)).rfind(prefix, 0) == 0) r.parts.push_back(p);
  return r;
}
struct LegMass { int area = 0, hip = 0, minRun = 99, skirt = 0, hipY = 0, stance = 0; };   // hipY: the hip row (rows above F); stance: the legs' span
// legsOver: the legs part draws over the garments (the woman's shoe tips over her hem), so every leg pixel shows
LegMass legMassOf(const Fr& l, const Fr& w, bool legsOver) {
  LegMass m;
  if (!l.ok) return m;
  auto worn = [&](int x, int y) {   // l's pixel (x, y) under a garment pixel of w (aligned at the pivots)
    if (!w.ok || legsOver) return false;
    const int X = x - l.px + w.px, Y = y - l.py + w.py;
    return X >= 0 && Y >= 0 && X < w.c.w && Y < w.c.h && (w.c.get(X, Y) >> 24);
  };
  if (w.ok) for (uint32_t p : w.c.px) m.skirt += (p >> 24) ? 1 : 0;
  int top = -1, x0 = 1 << 20, x1 = -1;
  for (int y = 0; y < l.c.h; y++) {
    int run = 0, rowPx = 0;
    for (int x = 0; x <= l.c.w; x++) {
      const bool vis = x < l.c.w && (l.c.get(x, y) >> 24) && !worn(x, y);
      if (vis) { run++; rowPx++; x0 = std::min(x0, x); x1 = std::max(x1, x); continue; }
      if (run) m.minRun = std::min(m.minRun, run);
      run = 0;
    }
    if (rowPx && top < 0) { top = y; m.hip = rowPx; }
    m.area += rowPx;
  }
  m.hipY = top >= 0 ? l.py - top : 0;
  m.stance = x1 >= x0 ? x1 - x0 + 1 : 0;
  return m;
}
void gateLegs(Gate& g) {
  static const char* const F = "DURL";
  for (int who = 0; who < 4; who++) {   // the A3 hero, the A3 woman (her dress, the shoe tips), the woman in trousers, bare-legged
    const doll::DollRecipe full = walkLook(who);
    const doll::DollRecipe legsR = onlyParts(full, "legs/"), wearR = onlyParts(full, "wear/");
    const RigSheet li = doll::compose(legsR, RigPart::Loco), wi = doll::compose(wearR, RigPart::Loco);
    const RigSheet lr = doll::compose(legsR, RigPart::Run), wr = doll::compose(wearR, RigPart::Run);
    for (int fc = 0; fc < 2; fc++) {   // (MOTION_P2 9) the front and back walks; the side views are B11 limbs' (gateLimbs)
      const LegMass idle = legMassOf(cut(li, fc, Clip::Idle, 0, 0), cut(wi, fc, Clip::Idle, 0, 0), who == 1);
      for (int run = 0; run < 2; run++) {
        const RigSheet& L = run ? lr : li;
        const RigSheet& W = run ? wr : wi;
        const LegMass contact = legMassOf(cut(L, fc, Clip::Walk, 0, 0), cut(W, fc, Clip::Walk, 0, 0), who == 1);
        for (int i = 0; i < L.count(fc, Clip::Walk); i++) {
          const LegMass m = legMassOf(cut(L, fc, Clip::Walk, 0, i), cut(W, fc, Clip::Walk, 0, i), who == 1);
          const std::string nm = std::string(who == 3 ? "woman/bare legs " : who == 2 ? "woman/trousers " : who ? "woman " : "man ") + (run ? "run " : "walk ") + F[fc] + std::to_string(i);
          const double ratio = idle.area ? (double)m.area / idle.area : 0, skirt = idle.skirt ? (double)m.skirt / idle.skirt : 1;
          if (g_report)
            std::printf("  B11 %-22s legs %3d / %3d (%3.0f %%)  hip %2d / %2d  narrowest run %d  hip row %+d  stance %2d (contact %2d)%s\n", nm.c_str(), m.area,
                        idle.area, 100 * ratio, m.hip, idle.hip, m.minRun == 99 ? 0 : m.minRun, contact.hipY - m.hipY, m.stance, contact.stance,
                        who == 1 ? fmt("  skirt %.0f %%", 100 * skirt).c_str() : "");
          if (ratio < 0.895) g.bad(nm + fmt(": legs %.0f %% of the idle's (90)", 100 * ratio));
          else if (who != 1 && m.hip < idle.hip - 1) g.bad(nm + fmt(": hip row %.0f px (the idle's %.0f, at most 1 narrower)", m.hip, idle.hip));
          else if (m.minRun < 3) g.bad(nm + fmt(": a leg run of %.0f px (3 or more)", m.minRun));
          else if (who != 1 && std::abs(contact.hipY - m.hipY) > 1) g.bad(nm + fmt(": the hips %.0f px from the contact frame's (1)", std::abs(contact.hipY - m.hipY)));
          else if (!run && (i == 1 || i == 4) && m.stance > contact.stance) g.bad(nm + fmt(": the down frame %.0f px wide (the contact %.0f)", m.stance, contact.stance));
          else if (who == 1 && skirt < 0.895) g.bad(nm + fmt(": skirt %.0f %% of the idle's (90)", 100 * skirt));
          else g.ok();
        }
      }
    }
  }
}

// ---------------------------------------------------------------- B11 limbs, B12 boots (docs/MOTION_P2.md 9; the header says what)
namespace limbs {

enum : uint8_t { kNo = 0, kThigh = 1, kShin = 2, kBoot = 3, kSole = 4, kLeg = 5 };
const char kClsChar[6] = {'.', 'T', 'S', 'B', 'O', 'N'};

// the probe: every slot x step its own colour, the leg regions on their own slots (the raw composite reads back exactly)
struct Probe { doll::DollRamps ramps; std::map<uint32_t, uint8_t> back; };
const Probe& probe() {
  static const Probe p = [] {
    Probe q;
    for (int s = 0; s < doll::SLOT_COUNT; s++)
      for (int k = 0; k < 5; k++) {
        const uint32_t c = 0xFF000000u | (uint32_t)(10 + s * 11) | (uint32_t)(20 + k * 40) << 8 | 0x33u << 16;
        q.ramps.c[s][k] = c;
        q.back[c] = doll::code((doll::Slot)s, k);
      }
    return q;
  }();
  return p;
}
doll::DollRecipe probed(doll::DollRecipe r) {
  r.ramps = probe().ramps;
  r.region[(int)doll::Region::Thigh] = r.region[(int)doll::Region::Knee] = (uint8_t)doll::Slot::Glow;
  r.region[(int)doll::Region::Shin] = (uint8_t)doll::Slot::Fx;
  r.region[(int)doll::Region::Boot] = (uint8_t)doll::Slot::Plume;
  r.region[(int)doll::Region::Sole] = (uint8_t)doll::Slot::Device;
  return r;
}
uint8_t clsOf(uint32_t c, uint8_t* step) {
  const auto it = probe().back.find(c | 0xFF000000u);
  if (it == probe().back.end()) { *step = 0; return kLeg; }
  *step = (uint8_t)doll::codeStep(it->second);
  switch (doll::codeSlot(it->second)) {
    case doll::Slot::Glow: return kThigh;
    case doll::Slot::Fx: return kShin;
    case doll::Slot::Plume: return kBoot;
    case doll::Slot::Device: return kSole;
    default: return kLeg;
  }
}
bool partIs(int p, std::initializer_list<doll::Role> roles) {
  const auto& P = doll::pool::get();
  if (p < 0 || p >= (int)P.parts.size()) return false;
  for (const auto& rr : P.parts[p].roles)
    for (doll::Role r : roles) if (rr.cat == (uint8_t)r) return true;
  return false;
}
doll::DollRecipe withParts(const doll::DollRecipe& full, std::initializer_list<doll::Role> roles) {
  doll::DollRecipe r = full;
  r.parts.clear();
  for (int p : full.parts) if (partIs(p, roles)) r.parts.push_back(p);
  return r;
}
const doll::pool::FrameRec* clipFrame(const doll::DollRecipe& r, RigPart part, int facing, Clip clip, int variant, int i) {
  for (const doll::pool::ClipRec* c : doll::pool::clipsOf(r.bodyKey, doll::pool::poolPart(part), r.family, -1, &r))
    if (c->clip == (uint8_t)clip && c->facing == facing && c->variant == variant && i < c->nFrames) return &doll::pool::get().frames[c->firstFrame + i];
  return nullptr;
}

// one side frame: the legs' classes, ramp steps and visibility, pivot-relative
struct LimbFrame {
  bool ok = false, left = false;
  int w = 0, h = 0, px = 0, py = 0;
  std::vector<uint8_t> cls, step, vis;
  uint8_t c(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h ? cls[(size_t)y * w + x] : kNo; }
  int u(int x) const { return left ? px - x - 1 : x - px; }
  int up(int y) const { return py - y; }
};
LimbFrame limbFrame(const Fr& legs, const Fr& comb, bool left) {
  LimbFrame f;
  if (!legs.ok) return f;
  f.ok = true; f.left = left; f.w = legs.c.w; f.h = legs.c.h; f.px = legs.px; f.py = legs.py;
  f.cls.assign((size_t)f.w * f.h, kNo); f.step = f.cls; f.vis = f.cls;
  for (int y = 0; y < f.h; y++)
    for (int x = 0; x < f.w; x++) {
      const uint32_t p = legs.c.get(x, y);
      if (!(p >> 24)) continue;
      uint8_t st = 0;
      const size_t i = (size_t)y * f.w + x;
      f.cls[i] = clsOf(p, &st);
      f.step[i] = st;
      const int X = x - legs.px + comb.px, Y = y - legs.py + comb.py;
      f.vis[i] = (uint8_t)(!comb.ok || (X >= 0 && Y >= 0 && X < comb.c.w && Y < comb.c.h && (comb.c.get(X, Y) | 0xFF000000u) == (p | 0xFF000000u)));
    }
  return f;
}
// the body's sole lengths by key (K0 flat .. K4 hang): adult 7, youth and child 6 (STYLE_P2 24, MOTION_P2 4.2)
// k1Steep: K1 is the 1:2 toe-up (runs 3, 2, 2: the fix-r1 re-plan of MOTION_P2 4.2, R1-04) and a whole 1:4 two-step
// toe-up is no key; the youth's and child's Y1 / C1 follow in R1-08, until then either slope reads as their K1
struct SoleTab { int lo[5], hi[5]; bool k1Steep; };
SoleTab soleTab(doll::BodySet b) {
  const bool adult = b == doll::BodySet::Man || b == doll::BodySet::Woman;
  const int n = adult ? 7 : 6;
  return SoleTab{{n, n, n, n - 1, n - 2}, {n, n, n, n, n - 1}, adult};
}
struct Sole {
  int u0 = 0, u1 = 0, len = 0, rows = 0, low = 0, key = -1, comp = -1;   // low: its lowest row (rows above F)
  bool whole = false, split = false;   // split: one half of two soles that ran together
  bool partial = false;                // a short two-step piece of a tilted sole (the rest hidden): judged where it is covered
  std::string bad, runs;
};
// one sole's pixels (u, rows above F): the clean-line rules and its key
Sole analyseSole(std::vector<std::pair<int, int>> px, const SoleTab& T) {
  Sole s;
  std::sort(px.begin(), px.end());
  s.u0 = px.front().first; s.u1 = px.back().first; s.len = s.u1 - s.u0 + 1;
  std::vector<int> row;
  int lo = 1 << 20, hi = -(1 << 20);
  for (size_t i = 0; i < px.size(); i++) {
    if (i && px[i].first == px[i - 1].first) { s.bad = fmt("two sole pixels in column u %+.0f", px[i].first); return s; }
    if (i && px[i].first != px[i - 1].first + 1) { s.bad = fmt("a gap in the sole at u %+.0f", px[i].first); return s; }
    row.push_back(px[i].second);
    lo = std::min(lo, px[i].second); hi = std::max(hi, px[i].second);
  }
  s.low = lo; s.rows = hi - lo + 1;
  std::vector<int> runs;
  int dir = 0;
  for (size_t i = 0; i < row.size(); i++) {
    if (!i || row[i] != row[i - 1]) {
      if (i) {
        const int d = row[i] - row[i - 1];
        if (std::abs(d) != 1 || (dir && d != dir)) {
          std::string rs;
          for (int r : row) rs += std::to_string(r);
          s.bad = "a jagged sole (rows above F by column: " + rs + ")";
          return s;
        }
        dir = d;
      }
      runs.push_back(1);
    } else runs.back()++;
  }
  for (size_t i = 0; i < runs.size(); i++) s.runs += (i ? "," : "") + std::to_string(runs[i]);
  // dir > 0: the sole rises toward the toe (toe up); dir < 0: toward the heel (heel up, toe down)
  if (runs.size() == 1) s.key = 0;
  else if (runs.size() == 2) {
    if (std::abs(runs[0] - runs[1]) > 1 || std::min(runs[0], runs[1]) < 3) {
      // shorter than any two-step key: the visible piece of a tilted sole behind another leg (toe-up K1, else heel-up
      // K2-K4). It is no fault by itself: measure() fails it where nothing covers it (squashed), feetFaults where its tilt
      // is not the table's (the placements). A full-length uneven sole stays a fault.
      if (s.len < T.lo[1]) { s.key = dir > 0 ? 1 : 2; s.partial = true; s.whole = false; return s; }
      s.bad = "an uneven two-step sole (runs " + s.runs + "; K1/K2 are 4 and 3)";
      return s;
    }
    if (dir > 0 && T.k1Steep) { s.bad = "a 1:4 toe-up sole (runs " + s.runs + "): K1 is the 1:2 toe-up (runs 3,2,2)"; return s; }
    s.key = dir > 0 ? 1 : 2;
  } else {
    const int r = runs[1];
    for (size_t i = 1; i + 1 < runs.size(); i++)
      if (runs[i] != r) { s.bad = "uneven steps in the sole (runs " + s.runs + ")"; return s; }
    if (runs.front() > r + 1 || runs.back() > r + 1) { s.bad = "a long end run on a stepped sole (runs " + s.runs + ")"; return s; }
    if (r > 2) { s.bad = "a slope of 1:" + std::to_string(r) + " over " + std::to_string(runs.size()) + " steps (not a key)"; return s; }
    if (dir > 0 && r != 2) { s.bad = "a steep toe-up sole (runs " + s.runs + "): no key tilts the toe up that far"; return s; }
    s.key = dir > 0 ? 1 : r == 2 ? 3 : 4;   // toe-up runs of 2: K1 (1:2); heel-up runs of 2: K3, of 1: K4
  }
  if (s.len > T.hi[s.key]) { s.bad = fmt("stretched: %.0f px (K%.0f is %.0f)", s.len, s.key, T.hi[s.key]); return s; }
  s.whole = s.len >= T.lo[s.key];
  return s;
}

struct LimbM {
  int area = 0, hipW = 0, hipRow = -99, minRun = 99, u0 = 1 << 20, u1 = -(1 << 20), stance = 0, pieces = 0, emptyRows = 0, boots = 0, noRegion = 0;
  bool soleOnF = false;
  std::vector<int> heels;           // the heel end (least u) of each sole run on row F
  std::vector<Sole> soles;
  std::vector<std::string> joints;  // 6b faults
  std::vector<std::string> bootBad; // B12 faults (shape, length)
  std::vector<std::pair<int, int>> bootPx;   // (key, +100 when covered; pixels) of each lone boot with a whole sole
};
// components of the pixels a predicate keeps (4- or 8-connected): the component id per pixel (-1 none), the count
int components(const LimbFrame& f, const std::function<bool(uint8_t)>& keep, bool eight, std::vector<int>& id) {
  id.assign((size_t)f.w * f.h, -1);
  int n = 0;
  std::vector<int> st;
  for (int i = 0; i < f.w * f.h; i++) {
    if (id[i] >= 0 || !keep(f.cls[i])) continue;
    id[i] = n; st.push_back(i);
    while (!st.empty()) {
      const int j = st.back(); st.pop_back();
      const int x = j % f.w, y = j / f.w;
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
          if ((!dx && !dy) || (!eight && dx && dy)) continue;
          const int X = x + dx, Y = y + dy;
          if (X < 0 || Y < 0 || X >= f.w || Y >= f.h) continue;
          const int k = Y * f.w + X;
          if (id[k] < 0 && keep(f.cls[k])) { id[k] = n; st.push_back(k); }
        }
    }
    n++;
  }
  return n;
}
LimbM measure(const LimbFrame& f, const SoleTab& T) {
  LimbM m;
  if (!f.ok) return m;
  int top = -1, bottom = -1;
  for (int y = 0; y < f.h; y++) {
    int rowVis = 0, rowAll = 0;
    for (int x = 0; x < f.w; x++) {
      const uint8_t k = f.c(x, y);
      if (k) { rowAll++; m.u0 = std::min(m.u0, f.u(x)); m.u1 = std::max(m.u1, f.u(x)); if (k == kLeg) m.noRegion++; }
      if (k && f.vis[(size_t)y * f.w + x]) rowVis++;
    }
    // visible runs (a run holding a boot or sole pixel is the boot's: B12 checks it)
    int run = 0;
    bool boot = false;
    for (int x = 0; x <= f.w; x++) {
      const uint8_t k = f.c(x, y);
      if (k && f.vis[(size_t)y * f.w + x]) { run++; boot = boot || k == kBoot || k == kSole; continue; }
      if (run && !boot) m.minRun = std::min(m.minRun, run);
      run = 0; boot = false;
    }
    if (rowVis >= 3 && m.hipRow == -99) { m.hipRow = f.up(y); m.hipW = rowVis; }
    m.area += rowVis;
    if (rowAll) { if (top < 0) top = y; bottom = y; }
  }
  for (int y = top; y >= 0 && y <= bottom; y++) {
    bool any = false;
    for (int x = 0; x < f.w; x++) any = any || f.c(x, y);
    if (!any) m.emptyRows++;
  }
  m.stance = m.u1 >= m.u0 ? m.u1 - m.u0 + 1 : 0;
  std::vector<int> all, boots, shins, thighs, soles;
  m.pieces = components(f, [](uint8_t k) { return k != kNo; }, false, all);
  m.boots = components(f, [](uint8_t k) { return k == kBoot || k == kSole; }, false, boots);
  const int nShin = components(f, [](uint8_t k) { return k == kShin; }, false, shins);
  const int nThigh = components(f, [](uint8_t k) { return k == kThigh; }, false, thighs);
  const int nSole = components(f, [](uint8_t k) { return k == kSole; }, true, soles);
  // 6b joints: what each component touches (4-neighbours)
  std::vector<int> bootShin(m.boots, 0), shinBoot(nShin, 0), shinThigh(nShin, 0), thighSeat(nThigh, 0), bootOcc(m.boots, 0), bootSoles(m.boots, 0), bootN(m.boots, 0);
  std::vector<int> bootTop(m.boots, 1 << 20);
  static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  for (int y = 0; y < f.h; y++)
    for (int x = 0; x < f.w; x++) {
      const int i = y * f.w + x;
      if (boots[i] >= 0) { bootN[boots[i]]++; bootTop[boots[i]] = std::min(bootTop[boots[i]], y); }
      if (thighs[i] >= 0 && y == top) thighSeat[thighs[i]] = 1;
      for (const auto& d : D) {
        const int X = x + d[0], Y = y + d[1];
        if (X < 0 || Y < 0 || X >= f.w || Y >= f.h) continue;
        const int j = Y * f.w + X;
        if (boots[i] >= 0 && shins[j] >= 0) bootShin[boots[i]] = 1;
        if (shins[i] >= 0 && boots[j] >= 0) shinBoot[shins[i]] = 1;
        if (shins[i] >= 0 && thighs[j] >= 0) shinThigh[shins[i]] = 1;
        // a boot pixel beside (not under) another leg's pixel: something covers it
        if (boots[i] >= 0 && d[1] == 0 && (shins[j] >= 0 || thighs[j] >= 0)) bootOcc[boots[i]] = 1;
      }
    }
  for (int b = 0; b < m.boots; b++)
    if (!bootShin[b]) m.joints.push_back(fmt("a boot (top row F-%.0f) touches no shin: floating or glued", f.up(bootTop[b])));
  for (int s = 0; s < nShin; s++) {
    if (!shinBoot[s]) m.joints.push_back("a shin ends short of its boot");
    if (!shinThigh[s]) m.joints.push_back("a shin hangs from no thigh (no knee)");
  }
  for (int t = 0; t < nThigh; t++)
    if (!thighSeat[t]) m.joints.push_back("a thigh does not reach the seat row");
  // the soles
  std::vector<std::vector<std::pair<int, int>>> sp(nSole);
  std::vector<int> soleComp(nSole, -1);
  for (int y = 0; y < f.h; y++)
    for (int x = 0; x < f.w; x++) {
      const int s = soles[y * f.w + x];
      if (s < 0) continue;
      sp[s].push_back({f.u(x), f.up(y)});
      if (soleComp[s] < 0) soleComp[s] = boots[y * f.w + x];
    }
  // the shins each boot touches (two: two boots run together)
  std::vector<std::set<int>> bootShins(m.boots);
  for (int y = 0; y < f.h; y++)
    for (int x = 0; x < f.w; x++) {
      const int i = y * f.w + x;
      if (boots[i] < 0) continue;
      for (const auto& d : D) {
        const int X = x + d[0], Y = y + d[1];
        if (X >= 0 && Y >= 0 && X < f.w && Y < f.h && shins[Y * f.w + X] >= 0) bootShins[boots[i]].insert(shins[Y * f.w + X]);
      }
    }
  auto compAt = [&](int u, int up) {   // the boot component of a sole pixel given in (u, rows above F)
    const int x = f.left ? f.px - u - 1 : u + f.px, y = f.py - up;
    return x >= 0 && y >= 0 && x < f.w && y < f.h ? boots[y * f.w + x] : -1;
  };
  // two boots' evidence for a run of sole pixels: two boot components, a boot touching two shins, or two boots' pixels
  auto twoBoots = [&](int ca, int cb) {
    return ca != cb || (ca >= 0 && (bootShins[ca].size() >= 2 || bootN[ca] * 2 >= 3 * 3 * T.lo[0]));
  };
  auto keep = [&](Sole so) {
    if (so.comp >= 0) bootSoles[so.comp]++;
    m.soles.push_back(so);
  };
  for (int s = 0; s < nSole; s++) {
    // a sole component may hold two soles: a lifted boot over another (two pixels a column: chained by rows) or a boot
    // beside or behind another (8-connected end to end: split at a step); only on two boots' evidence, so a lone
    // distorted boot (a doubled or bent sole) never splits
    std::vector<std::pair<int, int>> pts = sp[s];
    std::sort(pts.begin(), pts.end());
    std::vector<std::vector<std::pair<int, int>>> chains;
    for (size_t i = 0; i < pts.size();) {
      size_t j = i;
      while (j < pts.size() && pts[j].first == pts[i].first) j++;
      std::vector<int> used, to(j - i, -1);
      const size_t n0 = chains.size();
      for (int pass = 0; pass < 2; pass++)   // same-row continuations first, then a 1-row step
        for (size_t k = i; k < j; k++) {
          if (to[k - i] >= 0) continue;
          for (int c = 0; c < (int)n0; c++) {
            const auto& last = chains[c].back();
            if (last.first != pts[k].first - 1 || std::find(used.begin(), used.end(), c) != used.end()) continue;
            const int d = pts[k].second - last.second;
            if (pass == 0 ? d == 0 : std::abs(d) == 1) { to[k - i] = c; used.push_back(c); break; }
          }
        }
      for (size_t k = i; k < j; k++) {
        if (to[k - i] < 0) { chains.push_back({}); to[k - i] = (int)chains.size() - 1; }
        chains[to[k - i]].push_back(pts[k]);
      }
      i = j;
    }
    if (chains.size() >= 2) {
      bool ok = true;
      for (size_t c = 1; c < chains.size(); c++)
        ok = ok && twoBoots(compAt(chains[0].front().first, chains[0].front().second), compAt(chains[c].front().first, chains[c].front().second));
      if (!ok) {   // one boot with a doubled sole
        Sole so = analyseSole(sp[s], T);
        so.comp = soleComp[s];
        keep(so);
        continue;
      }
    }
    for (const auto& ch : chains) {
      Sole so = analyseSole(ch, T);
      so.comp = compAt(ch.front().first, ch.front().second);
      so.split = chains.size() >= 2;
      if (!so.bad.empty()) {
        bool done = false;
        for (size_t k = 1; k < ch.size() && !done; k++) {
          if (ch[k].second == ch[k - 1].second) continue;
          const std::vector<std::pair<int, int>> a(ch.begin(), ch.begin() + k), b(ch.begin() + k, ch.end());
          Sole A = analyseSole(a, T), B = analyseSole(b, T);
          if (!A.bad.empty() || !B.bad.empty()) continue;
          A.comp = compAt(a.front().first, a.front().second);
          B.comp = compAt(b.front().first, b.front().second);
          if (!twoBoots(A.comp, B.comp)) continue;
          A.split = B.split = true;
          keep(A);
          keep(B);
          done = true;
        }
        if (done) continue;
      }
      keep(so);
    }
  }
  for (const Sole& so : m.soles) {
    if (!so.bad.empty()) { m.bootBad.push_back(fmt("sole u %+.0f..%+.0f: ", so.u0, so.u1) + so.bad); continue; }
    if (so.whole) {
      if (so.comp >= 0 && bootSoles[so.comp] == 1) m.bootPx.push_back({so.key + (bootOcc[so.comp] ? 100 : 0), bootN[so.comp]});
      continue;
    }
    // a short sole: only where something covers its boot (another leg beside it, a second sole in its boot, a split)
    if (!so.split && (so.comp < 0 || (!bootOcc[so.comp] && bootSoles[so.comp] < 2)))
      m.bootBad.push_back(fmt("sole u %+.0f..%+.0f: ", so.u0, so.u1) + fmt("squashed: %.0f px (K%.0f is %.0f) with nothing covering it", so.len, so.key, T.lo[so.key]));
  }
  if (m.boots < 1 || m.boots > 2) m.bootBad.push_back(fmt("%.0f boots in the frame (1 or 2)", m.boots));
  // the ground row
  for (int x = 0; x < f.w; x++) {
    if (f.c(x, f.py) != kSole) continue;
    m.soleOnF = true;
    if (x > 0 && f.c(x - 1, f.py) == kSole) continue;
    int e = x;
    while (f.c(e + 1, f.py) == kSole) e++;
    m.heels.push_back(std::min(f.u(x), f.u(e)));
  }
  return m;
}
// the looks: B11 limbs' list (MOTION_P2 9) and --limbs-looks files ("tag who spec [gaits]" a line; who hero|woman;
// spec k=v,... as doll_gallery's --look plus palette=look and bare=1, "-" for none; gaits walk,run,dwalk,drun)
struct LimbLook { std::string tag; int who = 0; std::string spec; bool walk = true, run = true, dwalk = false, drun = false, bare = false, lookPal = false;
                  std::vector<std::string> gaits; };   // (P2c) every gait the line names (the strips: all four facings)
std::vector<LimbLook> gateLooks() {
  std::vector<LimbLook> v;
  auto add = [&](const char* tag, int who, const char* spec, bool run = true) { LimbLook l; l.tag = tag; l.who = who; l.spec = spec; l.run = run; v.push_back(l); };
  add("hero", 0, "");                                          // the A3 hero (A3 palette)
  add("woman", 1, "");                                         // the A3 woman: her dress, the shoe tips
  add("womant", 1, "outfit=0,bottomColor=#4F4B6B");           // the woman in shirt and trousers
  add("womanb", 1, "outfit=0,bottomColor=#4F4B6B,bare=1");    // bare-legged (the leg regions in skin)
  add("youthm", 0, "age=2,weapon=0");
  add("youthw", 1, "age=2,outfit=0,weapon=0");
  add("elderm", 0, "age=3,weapon=0", false);                  // the man elder (walk)
  add("elfm", 0, "people=2,weapon=0");
  add("defhero", 0, "weapon=0,topColor=#466E96,bottomColor=#4E3C2C,hairColor=#784624,palette=look");   // the default hero (last)
  return v;
}
HumanLook lookOf(LimbLook& l) {
  // (P2c ladder) the one looks parser (doll::applyLookSpec): the macros (hero= kit= npc= wpn= rarity= tier=) through the
  // simulation's hook, the plain fields through applyLookField
  HumanLook L = l.who ? a3Woman() : a3Hero();
  bool pal = l.lookPal, bare = l.bare;
  std::string bad;
  doll::applyLookSpec(L, l.spec, &pal, &bare, &bad);
  l.lookPal = pal; l.bare = bare;
  if (!bad.empty()) std::printf("  limbs: %s: unknown field(s) %s\n", l.tag.c_str(), bad.c_str());
  return L;
}
doll::DollRecipe recipeOf(LimbLook& l) {
  const HumanLook L = lookOf(l);
  doll::DollRecipe r = doll::resolve(L);
  r.ramps = l.lookPal ? doll::rampsOf(L) : doll::a3Palette(L.body == 1 ? 1 : 0);
  if (l.bare) for (doll::Region g : {doll::Region::Thigh, doll::Region::Knee, doll::Region::Shin}) r.region[(int)g] = (uint8_t)doll::Slot::Skin;
  return r;
}
bool tipsLook(const doll::DollRecipe& r) {
  for (int p : r.parts) if (doll::partHasRole(p, doll::Role::Legs, "tips")) return true;
  return false;
}

// one look's side frames, measured
struct Gait { const char* name; RigPart part; int variant; bool run; };
const Gait kGaits[4] = {{"walk", RigPart::Loco, 0, false}, {"run", RigPart::Run, 0, true}, {"dwalk", RigPart::Drawn, 1, false}, {"drun", RigPart::Drawn, 2, true}};
struct One { int gait = 0, facing = 2, i = 0; LimbFrame f; LimbM m; int skirt = 0; int plant0 = INT32_MIN, plant1 = INT32_MIN, bob = 0; Fr fig; };
struct LookFrames { std::vector<One> idle, frames; };   // idle: R then L
LookFrames shoot(LimbLook& l, bool figures) {
  LookFrames out;
  const doll::DollRecipe full = recipeOf(l);
  const SoleTab T = soleTab(full.body);
  const doll::DollRecipe legsP = probed(withParts(full, {doll::Role::Legs}));
  const doll::DollRecipe combP = probed(withParts(full, {doll::Role::Legs, doll::Role::Garment, doll::Role::Overlay, doll::Role::Armour}));
  const doll::DollRecipe wear = withParts(full, {doll::Role::Garment, doll::Role::Overlay, doll::Role::Armour});
  std::map<std::string, RigSheet> cache;
  auto sheet = [&](const doll::DollRecipe& r, bool fin, const char* k, RigPart part, int fc) -> const RigSheet& {
    const std::string key = std::string(k) + "|" + std::to_string((int)part) + "|" + std::to_string(fc);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    doll::ComposeOpts o;
    o.finish = fin;
    o.onlyFacing = fc;
    return cache[key] = doll::compose(r, part, &o);
  };
  auto one = [&](RigPart part, int fc, Clip clip, int variant, int i, int gait) {
    One o;
    o.gait = gait; o.facing = fc; o.i = i;
    const RigSheet& ls = sheet(legsP, false, "legs", part, fc);
    const RigSheet& cs = sheet(combP, false, "comb", part, fc);
    const RigSheet& ws = sheet(wear, true, "wear", part, fc);
    o.f = limbFrame(cut(ls, fc, clip, variant, i), cut(cs, fc, clip, variant, i), fc == 3);
    o.m = measure(o.f, T);
    const Fr w = cut(ws, fc, clip, variant, i);
    if (w.ok) for (uint32_t p : w.c.px) o.skirt += (p >> 24) ? 1 : 0;
    if (const doll::pool::FrameRec* fr = clipFrame(full, part, fc, clip, variant, i)) {
      if (fr->plant0 != doll::pool::kNoPlant) o.plant0 = (int)std::lround(fr->plant0 / 16.0);
      if (fr->plant1 != doll::pool::kNoPlant) o.plant1 = (int)std::lround(fr->plant1 / 16.0);
      o.bob = fr->bob;
    }
    if (figures) o.fig = cut(sheet(full, true, "full", part, fc), fc, clip, variant, i);
    return o;
  };
  for (int fc : {2, 3}) out.idle.push_back(one(RigPart::Loco, fc, Clip::Idle, 0, 0, -1));
  for (int g = 0; g < 4; g++) {
    if ((g == 0 && !l.walk) || (g == 1 && !l.run) || (g == 2 && !l.dwalk) || (g == 3 && !l.drun)) continue;
    for (int fc : {2, 3}) {
      const int n = sheet(legsP, false, "legs", kGaits[g].part, fc).count(fc, Clip::Walk, kGaits[g].variant);
      for (int i = 0; i < n; i++) out.frames.push_back(one(kGaits[g].part, fc, Clip::Walk, kGaits[g].variant, i, g));
    }
  }
  return out;
}
std::string frameName(const LimbLook& l, const One& o) {
  return l.tag + " " + (o.gait < 0 ? "idle" : kGaits[o.gait].name) + " " + "DURL"[o.facing] + (o.gait < 0 ? std::string() : std::to_string(o.i));
}

// B12's pool half: the stamped leg parts tagged, with a view for every side walk/run key their clips name
void gateStampedParts(Gate& g) {
  const auto& P = doll::pool::get();
  // (P2B_MOTION_ROLLOUT 5.4 LIBRARY) every side-loco legs part of every body: std, far and tips (the base six must exist;
  // every other legs part under legs/adult/loco/ or kin/*/legs/ is held to the same, so a new far part enters by itself)
  std::vector<std::string> names = {"legs/adult/loco/std", "legs/adult/loco/tips", "kin/youth/legs/std", "kin/youth/legs/tips", "kin/child/legs/std", "kin/child/legs/tips"};
  for (int q = 0; q < doll::partCount(); q++) {
    const std::string n = doll::partName(q);
    const bool loco = n.rfind("legs/adult/loco/", 0) == 0 || (n.rfind("kin/", 0) == 0 && n.find("/legs/") != std::string::npos);
    if (loco && std::find(names.begin(), names.end(), n) == names.end()) names.push_back(n);
  }
  for (const std::string& nameS : names) {
    const char* name = nameS.c_str();
    const int p = doll::findPart(name);
    if (p < 0) { g.bad(std::string(name) + ": not in the pool"); continue; }
    if (!doll::partTagged(p, "boots-stamped")) g.bad(std::string(name) + ": not tagged boots-stamped (MOTION_P2 8: every side walk/run boot a stamp)");
    else g.ok();
    const doll::pool::PartRec& pr = P.parts[p];
    std::set<std::string> missing;
    for (const doll::pool::ClipRec& c : P.clips) {
      if (c.clip != (uint8_t)Clip::Walk || (c.facing != 2 && c.facing != 3) || c.posture != doll::pool::kNone8) continue;
      const std::string& body = P.s(c.body);
      const std::string base = body.substr(0, body.find('.'));
      int bs = -1;
      for (int b = 0; b < (int)doll::BodySet::COUNT; b++) if (base == doll::bodySetName((doll::BodySet)b)) bs = b;
      if (bs < 0 || !doll::partServes(p, (doll::BodySet)bs)) continue;
      for (uint32_t i = 0; i < c.nFrames; i++)
        for (const doll::pool::KeyRec& k : P.frames[c.firstFrame + i].keys)
          if (k.channel == pr.channel && !doll::pool::viewOf(pr, c.facing, k.key)) missing.insert(std::string(1, "RL"[c.facing - 2]) + " " + P.s(k.key) + " (" + body + ")");
    }
    if (!missing.empty()) g.bad(std::string(name) + ": no view for " + *missing.begin() + (missing.size() > 1 ? fmt(" (+%.0f more)", (double)(missing.size() - 1)) : ""));
    else g.ok();
  }
}

// --limbs DIR: the harness's dumps (JSON per look, PNG figures)
void jsonGrid(FILE* o, const char* key, const LimbFrame& f, int what) {
  std::fprintf(o, "\"%s\":[", key);
  for (int y = 0; y < f.h; y++) {
    std::string s;
    for (int x = 0; x < f.w; x++) {
      const size_t i = (size_t)y * f.w + x;
      s += what == 0 ? kClsChar[f.cls[i]] : what == 1 ? (f.cls[i] ? (char)('0' + f.step[i]) : '.') : (f.cls[i] ? (f.vis[i] ? '1' : '0') : '.');
    }
    std::fprintf(o, "%s\"%s\"", y ? "," : "", s.c_str());
  }
  std::fprintf(o, "]");
}
std::string jsonEsc(const std::string& s) {
  std::string o;
  for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
  return o;
}
void writeLook(const char* dir, const std::string& tag, int who, const std::string& spec, const char* body, const std::string& bodyKey, bool tips, const LookFrames& lf) {
  FILE* o = std::fopen((std::string(dir) + "/" + tag + ".json").c_str(), "w");
  if (!o) { std::printf("FAIL: --limbs %s: cannot write\n", dir); return; }
  std::fprintf(o, "{\"tag\":\"%s\",\"who\":%d,\"spec\":\"%s\",\"body\":\"%s\",\"bodyKey\":\"%s\",\"tips\":%s,\"frames\":[\n", tag.c_str(), who, jsonEsc(spec).c_str(), body,
               bodyKey.c_str(), tips ? "true" : "false");
  bool first = true;
  for (int pass = 0; pass < 2; pass++)
    for (const One& f : pass ? lf.frames : lf.idle) {
      const std::string gait = f.gait < 0 ? "idle" : kGaits[f.gait].name;
      const std::string png = f.fig.ok ? tag + "_" + gait + "_" + "DURL"[f.facing] + std::to_string(f.i) + ".png" : std::string();
      if (f.fig.ok) {
        std::vector<uint32_t> px = f.fig.c.px;
        for (uint32_t& p : px) if (!(p >> 24)) p = 0xFFFF00FFu;
        writePng((std::string(dir) + "/" + png).c_str(), f.fig.c.w, f.fig.c.h, px);
      }
      const LimbM& m = f.m;
      std::fprintf(o, "%s{\"gait\":\"%s\",\"facing\":\"%c\",\"i\":%d,\"px\":%d,\"py\":%d,\"w\":%d,\"h\":%d,\"bob\":%d,\"plant0\":%s,\"plant1\":%s,\"png\":\"%s\",\"fig_px\":%d,\"fig_py\":%d,\"skirt\":%d,",
                   first ? "" : ",\n", gait.c_str(), "DURL"[f.facing], f.i, f.f.px, f.f.py, f.f.w, f.f.h, f.bob, f.plant0 == INT32_MIN ? "null" : std::to_string(f.plant0).c_str(),
                   f.plant1 == INT32_MIN ? "null" : std::to_string(f.plant1).c_str(), png.c_str(), f.fig.px, f.fig.py, f.skirt);
      first = false;
      jsonGrid(o, "cls", f.f, 0); std::fprintf(o, ","); jsonGrid(o, "step", f.f, 1); std::fprintf(o, ","); jsonGrid(o, "vis", f.f, 2);
      std::fprintf(o, ",\"m\":{\"area\":%d,\"hipW\":%d,\"hipRow\":%d,\"minRun\":%d,\"u0\":%d,\"u1\":%d,\"stance\":%d,\"pieces\":%d,\"emptyRows\":%d,\"boots\":%d,\"noRegion\":%d,\"soleOnF\":%s,\"heels\":[",
                   m.area, m.hipW, m.hipRow, m.minRun == 99 ? 0 : m.minRun, m.u0, m.u1, m.stance, m.pieces, m.emptyRows, m.boots, m.noRegion, m.soleOnF ? "true" : "false");
      for (size_t i = 0; i < m.heels.size(); i++) std::fprintf(o, "%s%d", i ? "," : "", m.heels[i]);
      std::fprintf(o, "],\"soles\":[");
      for (size_t i = 0; i < m.soles.size(); i++) {
        const Sole& s = m.soles[i];
        std::fprintf(o, "%s{\"u0\":%d,\"u1\":%d,\"len\":%d,\"rows\":%d,\"low\":%d,\"key\":%d,\"whole\":%s,\"partial\":%s,\"runs\":\"%s\",\"bad\":\"%s\"}", i ? "," : "", s.u0, s.u1, s.len, s.rows, s.low,
                     s.key, s.whole ? "true" : "false", s.partial ? "true" : "false", s.runs.c_str(), jsonEsc(s.bad).c_str());
      }
      std::fprintf(o, "],\"joints\":[");
      for (size_t i = 0; i < m.joints.size(); i++) std::fprintf(o, "%s\"%s\"", i ? "," : "", jsonEsc(m.joints[i]).c_str());
      std::fprintf(o, "],\"bootBad\":[");
      for (size_t i = 0; i < m.bootBad.size(); i++) std::fprintf(o, "%s\"%s\"", i ? "," : "", jsonEsc(m.bootBad[i]).c_str());
      std::fprintf(o, "]}}");
    }
  std::fprintf(o, "\n]}\n");
  std::fclose(o);
}
// (P2c SLICE-DOLL) every gait a looks line names, in all four facings: the finished frames as a 1x strip and a 4x strip
// per gait and facing (<tag>_<gait>_<F>.png, _x4.png: the frames side by side on their pivots), and <tag>_gaits.json with
// each frame's clip, size, pivot, ms and every part drawn with its layer and key. Gaits (docs/P2C_CONTRACTS.md 7): stay
// walk run dwalk drun didle hit1 hit2 hit3 block draw (draw + release) cast roll hurt fall lying (fall and lying: both
// sides)
struct GSeg { RigPart part; Clip clip; int variant; };
std::vector<GSeg> gaitSegs(const std::string& g) {
  if (g == "stay" || g == "idle") return {{RigPart::Loco, Clip::Idle, 0}};
  if (g == "walk") return {{RigPart::Loco, Clip::Walk, 0}};
  if (g == "run") return {{RigPart::Run, Clip::Walk, 0}};
  if (g == "dwalk") return {{RigPart::Drawn, Clip::Walk, 1}};
  if (g == "drun") return {{RigPart::Drawn, Clip::Walk, 2}};
  if (g == "didle") return {{RigPart::Drawn, Clip::Idle, 1}};
  if (g == "hit1" || g == "hit2" || g == "hit3") {
    const int v = g[3] - '1';
    return {{RigPart::Action, Clip::Windup, v}, {RigPart::Action, Clip::Strike, v}, {RigPart::Action, Clip::Follow, v}};
  }
  if (g == "block") return {{RigPart::Action, Clip::Block, 0}};
  if (g == "draw") return {{RigPart::Action, Clip::Draw, 0}, {RigPart::Action, Clip::Release, 0}};
  if (g == "cast") return {{RigPart::Action, Clip::Cast, 0}};
  if (g == "roll") return {{RigPart::Action, Clip::Roll, 0}};
  if (g == "hurt") return {{RigPart::Loco, Clip::Hurt, 0}};
  if (g == "fall") return {{RigPart::Death, Clip::Fall, 0}, {RigPart::Death, Clip::Fall, 1}};
  if (g == "lying") return {{RigPart::Death, Clip::Lying, 0}, {RigPart::Death, Clip::Lying, 1}};
  return {};
}
void writeStrip(const std::string& path, const std::vector<Fr>& fs, int k) {
  int L = 0, R = 0, U = 0, D = 0;
  for (const Fr& f : fs) { if (!f.ok) continue; L = std::max(L, f.px); R = std::max(R, f.c.w - f.px); U = std::max(U, f.py); D = std::max(D, f.c.h - f.py); }
  const int cw = L + R + 2, ch = U + D + 2, n = std::max<int>(1, (int)fs.size());
  const int W = (cw * n + 2) * k, H = (ch + 2) * k;
  std::vector<uint32_t> img((size_t)W * H, 0xFF2A3026u);
  for (size_t j = 0; j < fs.size(); j++) {
    const Fr& f = fs[j];
    // the pivot's column and the sole row marked faintly
    const int ox = 1 + (int)j * cw + 1 + L, oy = 1 + 1 + U;
    for (int y = 0; y < (oy + 1) * k && y < H; y++) for (int x = 0; x < k; x++) img[(size_t)y * W + (size_t)(ox * k + x)] = 0xFF343C30u;
    if (!f.ok) continue;
    for (int y = 0; y < f.c.h; y++)
      for (int x = 0; x < f.c.w; x++) {
        const uint32_t p = f.c.get(x, y);
        if (!(p >> 24)) continue;
        const int X = ox - f.px + x, Y = oy - f.py + y;
        for (int dy = 0; dy < k; dy++) for (int dx = 0; dx < k; dx++) img[(size_t)(Y * k + dy) * W + (size_t)(X * k + dx)] = p;
      }
  }
  writePng(path.c_str(), W, H, img);
}
void writeGaitStrips(const char* dir, LimbLook& l) {
  if (l.gaits.empty()) return;
  const doll::DollRecipe r = recipeOf(l);
  std::map<int, RigSheet> sheets;
  auto sheetOf = [&](RigPart p) -> const RigSheet& {
    auto it = sheets.find((int)p);
    if (it == sheets.end()) it = sheets.emplace((int)p, doll::compose(r, p)).first;
    return it->second;
  };
  FILE* o = std::fopen((std::string(dir) + "/" + l.tag + "_gaits.json").c_str(), "w");
  if (!o) { std::printf("FAIL: --limbs %s: cannot write\n", dir); return; }
  {
    const HumanLook LL = lookOf(l);
    const uint32_t* G = r.ramps.c[(int)doll::Slot::Glow];
    std::fprintf(o, "{\"tag\":\"%s\",\"spec\":\"%s\",\"topTier\":%d,\"weaponTier\":%d,\"glowRamp\":[%u,%u,%u,%u,%u],\"gaits\":[\n", l.tag.c_str(),
                 jsonEsc(l.spec).c_str(), (int)LL.topTier(), (int)LL.tier[art::HumanLook::TIER_WEAPON], G[0], G[1], G[2], G[3], G[4]);
  }
  bool firstG = true;
  int missing = 0;
  for (const std::string& g : l.gaits) {
    const std::vector<GSeg> segs = gaitSegs(g);
    if (segs.empty()) { std::printf("  limbs: %s: unknown gait %s\n", l.tag.c_str(), g.c_str()); continue; }
    for (int fc = 0; fc < 4; fc++) {
      std::vector<Fr> fs, gs;
      bool anyGlow = false;
      std::string frames;
      for (const GSeg& sg : segs) {
        const RigSheet& s = sheetOf(sg.part);
        const int n = s.count(fc, sg.clip, sg.variant);
        if (!n) { missing++; std::printf("  limbs: %s %s %c: no %s v%d frames\n", l.tag.c_str(), g.c_str(), "DURL"[fc], art::clipName(sg.clip), sg.variant); }
        for (int i = 0; i < n; i++) {
          fs.push_back(cut(s, fc, sg.clip, sg.variant, i));
          // (P2c ladder) the frame's glow plane on the same canvas (<base>_glow.png: the living finish's previews)
          {
            Fr gfr;
            const Fr& f0 = fs.back();
            if (f0.ok) {
              gfr.ok = true; gfr.px = f0.px; gfr.py = f0.py; gfr.c = Canvas(f0.c.w, f0.c.h);
              const art::FrameRect* fr = s.frame(fc, sg.clip, sg.variant, i);
              const size_t gi = fr ? (size_t)(fr - s.frames.data()) : (size_t)-1;
              if (!s.glow.empty() && gi < s.glow.rects.size() && s.glow.rects[gi].w > 0) {
                s.glow.each(gi, [&](int gx, int gy, uint32_t lc) { gfr.c.set(gx, gy, lc); });
                anyGlow = true;
              }
            }
            gs.push_back(gfr);
          }
          const doll::FrameInfo fi = doll::frameInfo(r, sg.part, fc, sg.clip, sg.variant, i);
          const Fr& f = fs.back();
          std::string parts;
          for (const doll::FrameInfo::P& p : fi.parts)
            parts += (parts.empty() ? "" : ",") + std::string("{\"part\":\"") + jsonEsc(p.part) + "\",\"layer\":\"" + p.layer + "\",\"key\":\"" + jsonEsc(p.key) + "\"}";
          char hd[400];
          const int NA = art::kNoAnchor;
          const int gX = f.an ? f.an->gripX : NA, gY = f.an ? f.an->gripY : NA, tX = f.an ? f.an->tipX : NA, tY = f.an ? f.an->tipY : NA;
          std::snprintf(hd, sizeof hd, "{\"clip\":\"%s\",\"variant\":%d,\"i\":%d,\"w\":%d,\"h\":%d,\"px\":%d,\"py\":%d,\"ms\":%d,\"turn\":%d,\"weapon\":%d,\"grip\":[%d,%d],\"tip\":[%d,%d],",
                        art::clipName(sg.clip), sg.variant, i, f.c.w, f.c.h, f.px, f.py, fi.ms, fi.turn, fi.weapon, gX, gY, tX, tY);
          frames += std::string(frames.empty() ? "" : ",\n    ") + hd + "\"parts\":[" + parts + "]}";
        }
      }
      const std::string base = std::string(dir) + "/" + l.tag + "_" + g + "_" + "DURL"[fc];
      if (!fs.empty()) { writeStrip(base + ".png", fs, 1); writeStrip(base + "_x4.png", fs, 4); }
      if (anyGlow) writeStrip(base + "_glow.png", gs, 1);
      std::fprintf(o, "%s  {\"gait\":\"%s\",\"facing\":\"%c\",\"png\":\"%s\",\"frames\":[\n    %s]}", firstG ? "" : ",\n", g.c_str(), "DURL"[fc],
                   fs.empty() ? "" : (l.tag + "_" + g + "_" + "DURL"[fc] + ".png").c_str(), frames.c_str());
      firstG = false;
    }
  }
  std::fprintf(o, "\n]}\n");
  std::fclose(o);
  if (missing) std::printf("  limbs: %s: %d gait segment(s) with no frames\n", l.tag.c_str(), missing);
}
void dumpLimbs(const char* dir, std::vector<LimbLook> looks) {
  for (LimbLook& l : looks) {
    const LookFrames lf = shoot(l, true);
    const doll::DollRecipe full = recipeOf(l);
    writeLook(dir, l.tag, l.who, l.spec, doll::bodySetName(full.body), full.bodyKey, tipsLook(full), lf);
    writeGaitStrips(dir, l);
  }
  std::printf("  limbs: %zu look(s) written to %s\n", looks.size(), dir);
}
std::vector<LimbLook> readLooks(const char* file) {
  std::vector<LimbLook> v;
  FILE* f = std::fopen(file, "r");
  if (!f) { std::printf("FAIL: --limbs-looks %s: cannot read\n", file); return v; }
  char line[1024];
  while (std::fgets(line, sizeof line, f)) {
    char tag[128] = {}, who[16] = {}, spec[700] = {}, gaits[128] = {};
    if (line[0] == '#' || std::sscanf(line, "%127s %15s %699s %127s", tag, who, spec, gaits) < 2) continue;
    LimbLook l;
    l.tag = tag; l.who = !std::strcmp(who, "woman") ? 1 : 0; l.spec = std::strcmp(spec, "-") && spec[0] ? spec : "";
    if (gaits[0]) {
      l.walk = l.run = l.dwalk = l.drun = false;
      std::string g = std::string(",") + gaits + ",";
      l.walk = g.find(",walk,") != std::string::npos;
      l.run = g.find(",run,") != std::string::npos;
      l.dwalk = g.find(",dwalk,") != std::string::npos;
      l.drun = g.find(",drun,") != std::string::npos;
      size_t a = 1;
      while (a < g.size()) { const size_t b = g.find(',', a); if (b > a) l.gaits.push_back(g.substr(a, b - a)); a = b + 1; }
    }
    v.push_back(l);
  }
  std::fclose(f);
  return v;
}

}  // namespace limbs

std::vector<dollmotion::Look> motionRefLooks(bool kin);
// (P2c SLICE-DOLL) the gear gates C3-C6 (test_doll_arms.cpp) and the slice looks they read
std::vector<dollmotion::Look> sliceLooks() {
  std::vector<dollmotion::Look> v;
  const std::string t = dollSliceLooksText();
  size_t a = 0;
  while (a < t.size()) {
    size_t b = t.find('\n', a);
    if (b == std::string::npos) b = t.size();
    const std::string line = t.substr(a, b - a);
    a = b + 1;
    char tag[128] = {}, who[16] = {}, spec[800] = {}, gaits[200] = {};
    if (line.empty() || line[0] == '#' || std::sscanf(line.c_str(), "%127s %15s %799s %199s", tag, who, spec, gaits) < 2) continue;
    dollmotion::Look l;
    l.tag = std::string("slice_") + tag; l.who = !std::strcmp(who, "woman") ? 1 : 0; l.spec = std::strcmp(spec, "-") ? spec : "";
    std::string g = gaits;
    size_t c = 0;
    while (c < g.size()) { size_t d = g.find(',', c); if (d == std::string::npos) d = g.size(); if (d > c) l.gaits.push_back(g.substr(c, d - c)); c = d + 1; }
    v.push_back(l);
  }
  return v;
}
// B11 limbs (R L) and B12 boots: check.py's checks in C++ on the reference looks and the kin (test_doll_motion.cpp); B11
// wear on GATES-WEAR's matrix; the selftests (P2B_MOTION_ROLLOUT 5.2-5.6)
void gateLimbs(Gate& g, Gate& b12, Gate& wear, Gate& self) {
  limbs::gateStampedParts(b12);   // B12 LIBRARY
  auto take = [](Gate& gt, const std::vector<dollmotion::Fail>& fs, int frames) {
    gt.checks += frames;   // (one per side frame judged; each failure counts again)
    for (const dollmotion::Fail& f : fs) gt.bad(f.check + " " + f.where + ": " + f.why);
  };
  for (const dollmotion::Look& l : motionRefLooks(true)) {
    const dollmotion::LookFrames lf = dollmotion::shoot(l, nullptr);
    std::vector<dollmotion::Fail> fs, lim, boots;
    dollmotion::limbsChecks(lf, fs, false);
    for (const dollmotion::Fail& f : fs) (f.check.rfind("B12", 0) == 0 ? boots : lim).push_back(f);
    take(g, lim, (int)lf.frames.size());
    take(b12, boots, (int)lf.frames.size());
  }
  // (P2c) the slice looks: B11 limbs and B12 boots on their side frames (armoured and armed, MOTION_P2 13)
  for (const dollmotion::Look& l : sliceLooks()) {
    const dollmotion::LookFrames lf = dollmotion::shoot(l, nullptr);
    std::vector<dollmotion::Fail> fs, lim, boots;
    dollmotion::limbsChecks(lf, fs, false);
    for (const dollmotion::Fail& f : fs) (f.check.rfind("B12", 0) == 0 ? boots : lim).push_back(f);
    take(g, lim, (int)lf.frames.size());
    take(b12, boots, (int)lf.frames.size());
  }
  std::vector<dollmotion::Look> matrix = dollmotion::matrixLooks("");
  for (const dollmotion::Look& l : sliceLooks()) matrix.push_back(l);   // (P2c) and B11 wear on them
  for (const dollmotion::Look& l : matrix) {
    const dollmotion::LookFrames lf = dollmotion::shoot(l, nullptr);
    std::vector<dollmotion::Fail> fs;
    dollmotion::wearChecks(lf, fs, false);
    take(wear, fs, (int)lf.frames.size());
  }
  if (matrix.empty()) wear.ok();
  const int miss = dollmotion::limbsSelftest(g_report) + dollmotion::wearSelftest(g_report);
  if (miss) self.bad(fmt("%.0f synthetic failure(s) missed (rpg_test --doll --motion --selftest --report)", miss)); else self.ok();
  // the default hero's trouser body (MOTION_P2 14: the dark trousers read as legs on grass)
  dollmotion::Look dh{"defhero", 0, "weapon=0,topColor=#466E96,bottomColor=#4E3C2C,hairColor=#784624,palette=look", {}};
  limbs::LimbLook ll; ll.tag = dh.tag; ll.who = 0; ll.spec = dh.spec;
  const HumanLook L = limbs::lookOf(ll);
  const float luma = lumaOf(doll::rampsOf(L).c[(int)doll::Slot::Cloth2][1]);
  if (g_report) std::printf("  B11L LUMA the default hero's trouser body (Cloth2 step 1) luma %.1f (52 or more)\n", luma);
  if (luma < 52.0f) g.bad(fmt("LUMA the default hero's trouser body (Cloth2 step 1) luma %.1f (52 or more)", luma)); else g.ok();
}

// --frames DIR (the walk fix's look-and-measure loop): every LOCO idle and walk frame and every RUN frame of the A3 hero
// and woman (A3's palettes) at 1x as <who>_<loco|run>_<F><i>.png (idle = loco frame "I"), its twin with the legs and
// garments only (_lg.png: the leg mass under the hem, nothing in front of it), and frames.tsv (name px py w h); clear
// pixels are written as the key #FF00FF
void dumpFrames(const char* dir) {
  FILE* tsv = std::fopen((std::string(dir) + "/frames.tsv").c_str(), "w");
  if (!tsv) { std::printf("FAIL: --frames %s: cannot write\n", dir); return; }
  for (int who = 0; who < 4; who++) {
    const doll::DollRecipe full = walkLook(who);
    doll::DollRecipe lg = full;
    lg.parts.clear();
    for (int p : full.parts) {
      const std::string n = doll::partName(p);
      if (n.rfind("legs/", 0) == 0 || n.rfind("wear/", 0) == 0) lg.parts.push_back(p);
    }
    for (int twin = 0; twin < 2; twin++)
      for (RigPart part : {RigPart::Loco, RigPart::Run}) {
        const RigSheet s = doll::compose(twin ? lg : full, part);
        for (int fc = 0; fc < 4; fc++)
          for (Clip c : {Clip::Idle, Clip::Walk}) {
            if (part == RigPart::Run && c == Clip::Idle) continue;
            const int n = c == Clip::Idle ? 1 : s.count(fc, c);
            for (int i = 0; i < n; i++) {
              const Fr f = cut(s, fc, c, 0, i);
              if (!f.ok) continue;
              const std::string nm = std::string(walkLookName(who)) + "_" + (part == RigPart::Run ? "run_" : "loco_") + "DURL"[fc] +
                                     (c == Clip::Idle ? std::string("I") : std::to_string(i)) + (twin ? "_lg" : "");
              std::vector<uint32_t> px = f.c.px;   // (writePng is opaque: clear pixels as the key #FF00FF)
              for (uint32_t& p : px) if (!(p >> 24)) p = 0xFFFF00FFu;
              writePng((std::string(dir) + "/" + nm + ".png").c_str(), f.c.w, f.c.h, px);
              std::fprintf(tsv, "%s\t%d\t%d\t%d\t%d\n", nm.c_str(), f.px, f.py, f.c.w, f.c.h);
            }
          }
      }
  }
  std::fclose(tsv);
  std::printf("  frames written to %s\n", dir);
}

// ---------------------------------------------------------------- B11 limbs, B11 wear, B12 boots (P2B_MOTION_ROLLOUT 5)
// the reference looks (5.5 M1) and the kin: B11 limbs and B12 judge these in --gates and by default in --motion
std::vector<dollmotion::Look> motionRefLooks(bool kin) {
  std::vector<dollmotion::Look> v;
  auto add = [&](const char* tag, int who, const char* spec, std::vector<std::string> gaits) { v.push_back({tag, who, spec, gaits}); };
  const std::vector<std::string> wr = {"walk", "run"}, w = {"walk"}, all = {"walk", "run", "dwalk", "drun"}, dr = {"dwalk", "drun"};
  add("hero", 0, "", all);                                                                                  // the A3 hero, sword
  add("defhero", 0, "weapon=0,topColor=#466E96,bottomColor=#4E3C2C,hairColor=#784624,palette=look", wr);   // the default hero
  add("womant", 1, "outfit=0,bottomColor=#4F4B6B,weapon=1", all);                                          // shirt and trousers, sword
  add("womanb", 1, "outfit=0,bottomColor=#4F4B6B,bare=1", wr);                                             // bare legs
  add("elfm", 0, "people=2,weapon=0", wr);
  add("elfw", 1, "people=2,outfit=0,bottomColor=#4F4B6B", wr);
  add("halfm", 0, "people=1,weapon=0", wr);
  add("broadm", 0, "build=2,weapon=0", wr);
  add("slimw", 1, "build=1,outfit=0,bottomColor=#4F4B6B", wr);
  add("elderm", 0, "age=3,weapon=0", w);
  add("elderw", 1, "age=3,outfit=0,bottomColor=#4F4B6B", w);
  add("hdrawn", 0, "weapon=1", dr);
  add("wdrawn", 1, "outfit=0,bottomColor=#4F4B6B,weapon=1", dr);
  if (kin) {
    add("youthm", 0, "age=2,weapon=0", wr);
    add("youthw", 1, "age=2,outfit=0,weapon=0", wr);
    add("childm", 0, "age=1,weapon=0", w);
    add("childw", 1, "age=1,outfit=0,weapon=0", w);
  }
  return v;
}
// rpg_test --doll --motion [--pool FILE] [--maps DIR] [--limbs-looks FILE | --matrix FILTER] [--report] [--frames DIR] [--selftest]
int motionCmd(const char* looksFile, const char* matrix, bool selftest, const char* framesDir) {
  std::vector<dollmotion::Look> looks;
  if (looksFile) looks = dollmotion::readLooks(looksFile);
  else if (matrix) looks = dollmotion::matrixLooks(matrix);
  else {
    looks = dollmotion::matrixLooks("");
    if (looks.empty()) looks = motionRefLooks(true);   // (until GATES-WEAR's matrix lands)
  }
  std::vector<dollmotion::Fail> fails;
  std::map<std::string, int> per;
  int frames = 0;
  for (const dollmotion::Look& l : looks) {
    const dollmotion::LookFrames lf = dollmotion::shoot(l, nullptr);
    frames += (int)lf.frames.size();
    const size_t was = fails.size();
    dollmotion::limbsChecks(lf, fails, g_report);
    dollmotion::wearChecks(lf, fails, g_report);
    if (framesDir) dollmotion::writeStrips(lf, framesDir);
    std::map<std::string, int> mine;
    for (size_t k = was; k < fails.size(); k++) { mine[fails[k].check]++; per[fails[k].check]++; }
    std::string s;
    for (auto& [c, n] : mine) s += " " + c + " " + std::to_string(n);
    std::printf("  motion %-10s %3d side frames: %s%s\n", l.tag.c_str(), (int)lf.frames.size(), mine.empty() ? "ok" : "FAIL", s.c_str());
  }
  int self = 0;
  if (selftest) {
    self += dollmotion::limbsSelftest(g_report);
    self += dollmotion::wearSelftest(g_report);
  }
  std::string s;
  for (auto& [c, n] : per) s += " " + c + " " + std::to_string(n) + ";";
  std::printf("motion: %d looks, %d side frames, %d failure(s)%s%s\n", (int)looks.size(), frames, (int)fails.size(), s.empty() ? "" : ":", s.c_str());
  if (selftest) std::printf("motion selftest: %d miss(es)\n", self);
  return fails.empty() && !self ? 0 : 1;
}
void mapsArg(int argc, char** argv) {
  const char* env = std::getenv("CHECK_MAPS");
  std::string maps = env && *env ? env : "";
  for (int i = 1; i + 1 < argc; i++) {
    if (!std::strcmp(argv[i], "--maps")) maps = argv[i + 1];
    if (!std::strcmp(argv[i], "--pool") && maps.empty()) {   // an isolated try: <root>/x.pool beside <root>/doll/maps
      std::string d = argv[i + 1];
      const size_t sl = d.find_last_of("/\\");
      d = sl == std::string::npos ? std::string(".") : d.substr(0, sl);
      if (FILE* t = std::fopen((d + "/doll/maps/stamps/boots_adult.stamps").c_str(), "r")) { std::fclose(t); maps = d + "/doll/maps"; }
    }
  }
  if (!maps.empty()) { dollmotion::setMapsRoot(maps); std::printf("  maps read from %s\n", maps.c_str()); }
}

bool loadPoolArg(int argc, char** argv) {
  for (int i = 1; i + 1 < argc; i++)
    if (!std::strcmp(argv[i], "--pool")) {
      std::string err;
      if (!doll::loadPoolFile(argv[i + 1], &err)) { std::printf("FAIL: --pool %s: %s\n", argv[i + 1], err.c_str()); return false; }
      std::printf("  pool loaded from %s\n", argv[i + 1]);
    }
  return true;
}

int dollCmd(int argc, char** argv) {
  bool gates = false, par = false;
  const char* dump = nullptr;
  const char* frames = nullptr;
  const char* limbsDir = nullptr;
  const char* limbsLooks = nullptr;
  bool motion = false, mself = false;
  const char* matrix = nullptr;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--gates")) gates = true;
    else if (!std::strcmp(argv[i], "--parity")) par = true;
    else if (!std::strcmp(argv[i], "--report")) g_report = true;
    else if (!std::strcmp(argv[i], "--dump") && i + 1 < argc) dump = argv[++i];
    else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) frames = argv[++i];
    else if (!std::strcmp(argv[i], "--limbs") && i + 1 < argc) limbsDir = argv[++i];
    else if (!std::strcmp(argv[i], "--limbs-looks") && i + 1 < argc) limbsLooks = argv[++i];
    else if (!std::strcmp(argv[i], "--motion")) motion = true;
    else if (!std::strcmp(argv[i], "--selftest")) mself = true;
    else if (!std::strcmp(argv[i], "--matrix") && i + 1 < argc) matrix = argv[++i];
  }
  if (!loadPoolArg(argc, argv)) return 1;
  mapsArg(argc, argv);
  if (motion) {   // only B11 limbs, B11 wear and B12 (5.7): exit 0 iff 0 failures
    const int was = art::rigVersion();
    art::setRigVersion(2);
    const int r = motionCmd(limbsLooks, matrix, mself, frames);
    art::setRigVersion(was);
    return r;
  }
  g_contract = 0;
  contract();
  std::printf("doll contract: %d failure(s)\n", g_contract);
  int gateFails = 0;
  const int was = art::rigVersion();
  art::setRigVersion(2);
  if (frames) dumpFrames(frames);
  if (limbsDir) limbs::dumpLimbs(limbsDir, limbsLooks ? limbs::readLooks(limbsLooks) : limbs::gateLooks());
  if (par || gates) {
    Gate b1{"B1 parity"};
    parity(b1, dump);
    std::printf("%s %-24s %4d checks, %4d failed%s%s\n", b1.fails ? "FAIL:" : "ok   ", b1.id, b1.checks, b1.fails, b1.fails ? "; first: " : "", b1.first.c_str());
    gateFails += b1.fails ? 1 : 0;
  }
  if (gates) {
    Gate b2{"B2 heights"}, fc{"faces"}, hd{"hands"}, b8{"B8 craft"}, b9{"B9 fitted pairs"}, b10{"B10 atlas and speed"}, rp{"ramps dE 6"}, b11{"B11 leg mass (D U)"}, b11l{"B11 limbs (R L)"},
        b12{"B12 boots"}, b11w{"B11 wear (R L)"}, b11s{"B11 selftest"};
    gateHeights(b2);
    gateFaces(fc, hd);
    gateCraft(b8);
    gateFit(b9);
    gateAtlas(b10);
    gateRamps(rp);
    gateLegs(b11);
    gateLimbs(b11l, b12, b11w, b11s);
    std::vector<std::string> armsLines;
    const int armsFails = dollArmsGates(g_report, armsLines);   // (P2c) C3 gear, C4 armour change, C5 families, C6 weapons
    std::vector<std::string> ladderLines;
    const int ladderFails = dollLadderGates(g_report, ladderLines);   // (P2c ladder) C12 C13 C14 and their selftests
    for (Gate* g : {&b2, &fc, &hd, &b8, &b9, &b10, &rp, &b11, &b11l, &b11w, &b12, &b11s}) {
      std::printf("%s %-24s %4d checks, %4d failed%s%s\n", g->fails ? "FAIL:" : "ok   ", g->id, g->checks, g->fails, g->fails ? "; first: " : "", g->first.c_str());
      gateFails += g->fails ? 1 : 0;
    }
    for (const std::string& l : armsLines) std::printf("%s\n", l.c_str());
    gateFails += armsFails;
    for (const std::string& l : ladderLines) std::printf("%s\n", l.c_str());
    gateFails += ladderFails;
    std::vector<std::string> forgeLines;
    gateFails += dollForgeGates(g_report, forgeLines);   // (P2c forge) C16 a b c s, C17, C1 source F, C12a restated
    for (const std::string& l : forgeLines) std::printf("%s\n", l.c_str());
    std::vector<std::string> envLines;
    gateFails += dollEnvelopeGates(g_report, envLines);   // (P2c envelope) C12f, C3b and B10 by tier, C13 drawn motion, C19
    for (const std::string& l : envLines) std::printf("%s\n", l.c_str());
  }
  art::setRigVersion(was);
  const int bad = g_contract + gateFails;
  std::printf(bad ? "doll: %d failure(s)\n" : "doll: 0 failures\n", bad);
  return bad ? 1 : 0;
}
RPG_TEST_CMD("--doll", "P2b doll contract [--parity: B1] [--gates: B1 B2 faces B8 B9 B10 ramps B11 B11-limbs B12] [--report] [--pool f] [--dump dir] [--frames dir] [--limbs dir [--limbs-looks f]] [--limbs-synth f dir]", dollCmd);

// ---------------------------------------------------------------- the census (B6, B7)
bool civil(const HumanLook& L) {
  // (P2b fix TRADES-DRAWN-ON-V1) a civil trade's leathers are its work clothes (HumanLook::civil): it is counted
  const bool armour = L.armorStyle || L.bodyForm || (L.outfit == art::Outfit::Leather && !L.civil) || L.outfit == art::Outfit::Chain || L.outfit == art::Outfit::Plate ||
                      L.outfit == art::Outfit::Elven || L.outfit == art::Outfit::Ebony || L.outfit == art::Outfit::Guard;
  if (armour || L.helmet || L.helmStyle || L.helmForm || L.shield || L.shieldStyle || L.shieldForm) return false;
  if (L.bladeForm || L.polearmForm > 1 || L.bowForm || L.pauldron || L.skirt || L.crest || L.gloves || L.boots) return false;
  return true;
}
// tier: 0 village, 1 town, 2 city; job: the census resident's life::Job (-1: none); role: the Role it was dressed as
// (-1: none); music: its culture's (the bard's instrument); who: 'H' the hero, 'E' a companion, else 0
struct Who {
  HumanLook look; char src; std::string site; int tier = 0;
  int job = -1, role = -1; char who = 0;
  bool hasMusic = false; decltype(cult::Culture::music) music{};   // its culture's music (by value: the census Game is gone)
  void culture(const cult::Culture* c) { hasMusic = c != nullptr; if (c) music = c->music; }
  uint8_t traits = 0;   // the resident's life::TR_* (a sociable soul plays the tavern's tune when the bard is not free)
};

// (P2b fix r2, B6) the start region's census residents dressed by the GAME's resident path (Game::dressAsResident: the
// TOWNSFOLK rules of dressResident: the lamplighter's pole, the smith's hammer, the census ages, the trades' garments,
// the beggar's rags and marks, the small child's dots), never a mirror of it
void censusA(uint64_t seed, std::vector<Who>& out, bool withResidents) {
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  for (int f = 0; f < 60; f++) { g.update(SIM_DT, Input()); g.events.clear(); g.mode = Mode::Play; }
  for (const Actor& x : g.actors)
    if (x.human) {
      Who w{x.look, x.comp ? 'E' : 'A', "", 0};
      w.role = (int)x.role;
      w.culture(x.site >= 0 ? g.world.cultureOf(x.site) : nullptr);
      w.who = x.player ? 'H' : x.comp ? 'E' : 0;
      out.push_back(w);
    }
  if (!withResidents) return;
  const Site& home = g.world.sites[(size_t)g.world.startSite];
  for (size_t si = 0; si < g.world.sites.size(); si++) {
    const Site& s = g.world.sites[si];
    if (s.type != SiteType::Village && s.type != SiteType::Town && s.type != SiteType::City) continue;
    if (std::abs(s.r.cx() - home.r.cx()) > 160 || std::abs(s.r.cy() - home.r.cy()) > 160) continue;
    g.world.ensureSiteRecords((int)si);   // (a site outside the window: its buildings, as the census needs them)
    const life::Census* c = g.life.census(g.world, (int)si);
    if (!c) continue;
    const int tier = s.type == SiteType::Village ? 0 : s.type == SiteType::Town ? 1 : 2;
    const cult::Culture* C = g.world.cultureOf((int)si);
    for (size_t idx = 0; idx < c->res.size(); idx++) {
      Actor a;
      a.human = true;
      if (!g.dressAsResident(a, s.id, (int)idx)) continue;
      Who w{a.look, 'A', s.name, tier};
      w.job = (int)c->res[idx].job;
      w.role = (int)life::jobRole(c->res[idx].job);
      w.traits = c->res[idx].traits;
      w.culture(C);
      out.push_back(w);
    }
  }
}
void censusB(uint64_t seed, std::vector<Who>& out) {
  Game g(seed);
  g.newEndlessGame(seed);
  std::set<uint64_t> seen;
  for (size_t si = 0; si < g.world.sites.size(); si++) {
    const cult::Culture* C = g.world.cultureOf((int)si);
    if (!C || !seen.insert(C->id).second) continue;
    for (int role = 0; role < (int)Role::COUNT; role++)
      for (int female = 0; female < 2; female++)
        for (int age : {1, 2, 0, 3})
          for (uint64_t ps = 0; ps < 8; ps++) {
            // children (1) and youths (2) are the Child role's; adults (0) and elders (3) every other role's
            if (((Role)role == Role::Child) != (age == 1 || age == 2)) continue;
            HumanLook L;
            std::string name;
            census::dress(L, name, (Role)role, female != 0, *C, *C, 1000003u * (ps + 1) ^ C->id ^ (uint64_t)role << 40, census::Rank{});
            L.age = (uint8_t)age;
            out.push_back({L, 'B', "", 0});
          }
  }
}
void censusD(uint64_t seed, std::vector<Who>& out) {
  static const uint32_t skins[8] = {rgba(250, 220, 190), rgba(242, 204, 166), rgba(236, 188, 146), rgba(214, 160, 116),
                                    rgba(190, 134, 94),  rgba(160, 104, 70),  rgba(124, 80, 56),   rgba(92, 60, 46)};
  Game g(seed);
  g.newEndlessGame(seed);
  std::vector<const cult::Culture*> homes{nullptr};
  std::set<uint64_t> seen;
  for (size_t si = 0; si < g.world.sites.size() && homes.size() < 6; si++) {
    const cult::Culture* C = g.world.cultureOf((int)si);
    if (C && seen.insert(C->id).second) homes.push_back(C);
  }
  for (const cult::Culture* home : homes)
    for (int hair = 0; hair < (int)art::Hair::COUNT; hair++)
      for (int beard = 0; beard < 2; beard++)
        for (int build = 0; build < 3; build++)
          for (int people = 0; people < 3; people++)
            for (int skin = 0; skin < 8; skin += home ? 7 : 1)
              for (int fem = 0; fem < 2; fem++) {
                if (fem && beard) continue;
                Appearance a;
                a.female = fem != 0; a.build = (uint8_t)build; a.skinTone = (uint8_t)skin; a.skin = skins[skin]; a.hair = (uint8_t)hair;
                a.beard = beard != 0; a.people = (uint8_t)people; a.homeland = home ? home->id : 0;
                out.push_back({appearanceLook(a, home), 'D', "", 0});
              }
}

// ---------------------------------------------------------------- (P2b fix r2) B6's dead parts: every source drawn
// A part is alive when a recipe the GAME draws on the doll at rig 2 uses it: a look the game dresses (each source below)
// that art::rig2Covers (else the view draws it on v1, and none of its parts is drawn), and each posture the game puts that
// look in that the doll covers (render.cpp rigPoseTex: dollCoversPosture, else v1) with the posture's prop in hand
// (doll_internal.h withPosture). The sources:
//   residents  the start region's census through the game's resident path (Game::dressAsResident), each in its job's
//              postures; the world's humans after 60 frames (the hero, the key persons, the companions)
//   cultures   every culture of the seed x every role x both sexes x the ages (census::dress, as Game::makeLook calls it)
//   roles      Game::makeLook itself for every role at one settlement of each culture and each kingdom's capital (the
//              key persons, the wayside, the harbour, the court: the tool or weapon each role carries, the hoods and
//              capes), in the role's trade postures
//   creator    the character creator's looks (hair, beard, build, people, skin, sex, homeland)
//   gear       the creator's looks in worn gear (wearGear: each weapon type, the staff, the bow, each cloak, the
//              amulet and the ring), as the hero and a companion are drawn, in the hero's postures (kneel, ride, row)
//   companions census residents recruited (party_recruit.cpp: the vocation's weapon), in the hero's postures
//   towns      every culture's first village, town and city through the resident path (the trades a start region
//              lacks), in their jobs' postures
//   peoples    every archetype's people (cult::Atlas::make): its musicians in their people's instruments
// --scope civil lists the parts only P2c's armour roles serve (armour, helm, shield, pauldron) apart: P2c's census
// reaches them (--scope all counts them dead).
using art::Posture;
struct PoseUse { Posture p; uint8_t v; };
uint64_t lookKey(const HumanLook& L) {
  return L.key() ^ (uint64_t)L.head << 7 ^ (uint64_t)L.hairVar << 13 ^ (uint64_t)L.age << 19 ^ (uint64_t)L.garb << 23 ^ (uint64_t)L.civil << 31;
}
// the postures the game gives a person (life_game.cpp): the everyday seats (chair and floor), meals, prayers and
// greetings; the grown-ups' tavern (a drink, a toast, a dance); the children's games; the trade's work postures by job
// (beginAct's field hoe, rod, axe and pick, the labourer's load, the sweepers; workAt's anvil, oven, lectern, altar and
// workbench; the lamplighter's round, the beggar's bowl); the bard's instrument by his people's music (render_anim.cpp's
// variant: the drum by MusicStyle::perc, else bardVariant); the hero's and a companion's kneel (binding wounds), ride,
// row and downed (party_game.cpp, home). At rig 2 the view gives each carrier his own load (sack, basket, crate) and
// the musician his people's drum one tune in three (render_anim.cpp).
void posesOf(const Who& w, std::vector<PoseUse>& out) {
  out.clear();
  auto add = [&](Posture p, uint8_t v = 0) { out.push_back({p, v}); };
  for (Posture p : {Posture::Sit, Posture::SitEat, Posture::SitFloor, Posture::SitFloorEat, Posture::Eat, Posture::Wave, Posture::Pray}) add(p);
  // (P2c wave 2 round 2, DOLL-C) a youth drinks (kin_youth's drink clips) and so toasts on a festival night (life_game: Cheer
  // from SitDrink)
  if (w.look.age == 2) for (Posture p : {Posture::SitDrink, Posture::SitFloorDrink, Posture::Drink, Posture::Cheer}) add(p);
  // (wave-2 fix round, D79) a child leaves with its emigrating family carrying its own sack (life.cpp: "their children with
  // them"; life_game T_AWAY: Carry): held/props/sack_child
  if (w.look.age == 1) add(Posture::Carry);
  if (w.look.age == 1 || w.look.age == 2) { add(Posture::Play); add(Posture::Dance); return; }
  for (Posture p : {Posture::SitDrink, Posture::SitFloorDrink, Posture::Drink, Posture::Cheer, Posture::Dance}) add(p);
  if (w.who == 'H' || w.who == 'E') for (Posture p : {Posture::Kneel, Posture::Ride, Posture::Row, Posture::Downed}) add(p);
  using J = life::Job;
  J job = w.job >= 0 ? (J)w.job : J::COUNT;
  if (job == J::COUNT && w.role >= 0) switch ((Role)w.role) {   // a role's trade (life::jobRole inverted)
      case Role::Smith: case Role::Shipwright: job = J::Smith; break;
      case Role::Farmer: job = J::Farmer; break;
      case Role::Fisher: job = J::Fisher; break;
      case Role::Innkeeper: job = J::Innkeeper; break;
      case Role::Merchant: job = J::Merchant; break;
      case Role::Priest: job = J::Priest; break;
      case Role::Mage: job = J::Scholar; break;
      case Role::Jarl: job = J::Noble; break;
      default: break;
    }
  switch (job) {
    case J::Farmer: add(Posture::Hoe); break;
    case J::Fisher: add(Posture::Fish); break;
    case J::Woodcutter: add(Posture::Chop); break;
    case J::Miner: add(Posture::Hammer); add(Posture::Chop); break;
    case J::Smith: add(Posture::Hammer); add(Posture::Sweep); break;
    case J::Baker: add(Posture::Stir); add(Posture::Sweep); break;
    case J::Scholar: add(Posture::Read); break;
    case J::Lamplighter: add(Posture::Lamp); break;
    case J::Beggar: add(Posture::Beg); break;
    case J::Labourer: for (uint8_t v = 0; v < 3; v++) add(Posture::Carry, v); break;   // (rig 2: each carrier's own load)
    case J::Servant: case J::Server: case J::Stablehand: case J::Innkeeper: case J::Merchant: add(Posture::Sweep); break;
    default: break;
  }
  // the evening's musician (life_game.cpp tavern night: the town's bard if free, else a sociable soul) plays his people's
  // lead (Act::Perform: bardPosture), the view's variant by its music (render_anim.cpp)
  if (job == J::Bard || (w.job >= 0 && (w.traits & life::TR_SOCIABLE))) {
    if (w.hasMusic) {
      const Posture bp = art::bardPosture(w.music);
      add(bp, bp == Posture::Drum ? (uint8_t)w.music.perc : art::bardVariant(w.music));
      // (rig 2) one tune in three on his people's drum (render_anim.cpp)
      if (bp != Posture::Drum && w.music.perc != PercKind::None) add(Posture::Drum, (uint8_t)w.music.perc);
    } else add(Posture::Lute, 0);
  }
}
struct Reach {
  struct Src { int looks = 0, doll = 0, poses = 0, only = 0; };
  std::vector<std::string> names;
  std::vector<Src> src;
  std::vector<uint32_t> partMask;   // which sources reached each part
  std::set<std::pair<uint64_t, int>> seenLook;
  std::set<uint64_t> seenPose;
  std::vector<PoseUse> poses;
  int id(const char* name) {
    for (size_t i = 0; i < names.size(); i++) if (names[i] == name) return (int)i;
    names.push_back(name); src.push_back({});
    return (int)names.size() - 1;
  }
  void mark(const doll::DollRecipe& r, int s) {
    doll::markReached(r);
    if (partMask.size() < (size_t)doll::partCount()) partMask.resize((size_t)doll::partCount(), 0);
    for (int p : r.parts) if (p >= 0 && (size_t)p < partMask.size()) partMask[(size_t)p] |= 1u << s;
  }
  // why a part a source wants is not drawn (the first look or posture that wanted it, and what it lacks); wait: the
  // parts wanted only by looks that P2c's scope holds back (armour, helms, shields, the action families: VISION_PLAN
  // 15.36 "Armour, helms, shields and the action families are P2c's"), with the first such look's P2c need
  std::map<int, std::string> why, wait;
  // (P2c wave 2 round 2, DOLL-C) a posture the sim gives a body that the doll does not cover is drawn from a v1 sheet at rig 2
  // (the judge: a youth dancing at play, v1Draws 1): counted by body and posture, a C1 failure (never only a dead-part reason)
  std::map<std::string, int> poseV1;
  static bool p2cNeed(const doll::Missing& m) {
    if (m.role == doll::Role::Armour || m.role == doll::Role::Helm || m.role == doll::Role::Shield || m.role == doll::Role::Pauldron) return true;
    return m.role == doll::Role::Body && m.value.find("action") != std::string::npos;
  }
  static bool p2cHeld(const std::vector<doll::Missing>& m) {
    if (m.empty()) return false;
    for (const doll::Missing& x : m) if (!p2cNeed(x)) return false;
    return true;
  }
  void wanted(const doll::DollRecipe& r, const std::vector<doll::Missing>& m, const char* source, const char* what) {
    std::map<int, std::string>& to = p2cHeld(m) ? wait : why;
    for (int p : r.parts) {
      if (to.count(p)) continue;
      to[p] = std::string(source) + " " + what + (m.empty() ? std::string(": not drawn") : ": " + m[0].what());
    }
  }
  void add(const Who& w, const char* source) {
    const int s = id(source);
    const uint64_t k = lookKey(w.look);
    if (!seenLook.insert({k, s}).second) return;
    src[(size_t)s].looks++;
    const doll::DollRecipe r = doll::resolve(w.look);
    if (!art::rig2Covers(w.look)) {   // drawn on v1 (render.cpp rigTex): none of its doll parts is drawn
      const std::vector<doll::Missing> m = doll::dollMissing(w.look);
      wanted(r, m, source, "look drawn on v1");
      if (p2cHeld(m)) {   // its postures' props wait on P2c with it
        posesOf(w, poses);
        for (const PoseUse& u : poses) wanted(doll::withPosture(r, u.p, u.v), m, source, "look drawn on v1");
      }
      return;
    }
    src[(size_t)s].doll++;
    mark(r, s);
    posesOf(w, poses);
    for (const PoseUse& u : poses) {
      const uint64_t pk = (k * 0x9E3779B97F4A7C15ull) ^ ((uint64_t)u.p + 1) << 40 ^ (uint64_t)u.v << 52 ^ (uint64_t)s << 58;
      if (!seenPose.insert(pk).second) continue;
      if (!doll::dollCoversPosture(w.look, u.p, u.v)) {   // drawn on v1 (render.cpp rigPoseTex)
        wanted(doll::withPosture(r, u.p, u.v), doll::dollMissingPosture(w.look, u.p, u.v), source, "posture drawn on v1");
        {
          const std::vector<doll::Missing> pm = doll::dollMissingPosture(w.look, u.p, u.v);
          poseV1[std::string(doll::bodySetName(r.body)) + " posture " + std::to_string((int)u.p) + (u.v ? "." + std::to_string((int)u.v) : std::string()) +
                 (pm.empty() ? std::string() : " (" + pm[0].what() + ")")]++;
        }
        continue;
      }
      src[(size_t)s].poses++;
      mark(doll::withPosture(r, u.p, u.v), s);
    }
  }
};
// a part only P2c's armour serves (every role it takes is armour, helm, shield or pauldron)
bool p2cPart(int part) {
  const auto& P = doll::pool::get();
  if (part < 0 || (size_t)part >= P.parts.size() || P.parts[(size_t)part].roles.empty()) return false;
  for (const auto& rr : P.parts[(size_t)part].roles) {
    const doll::Role ro = (doll::Role)rr.cat;
    if (ro != doll::Role::Armour && ro != doll::Role::Helm && ro != doll::Role::Shield && ro != doll::Role::Pauldron) return false;
  }
  return true;
}
// the creator's look in worn heartland gear (wearGear), by its index: each weapon type, the staff, the bow; each cloak
// (a cloak, a fur mantle, a fine cloak of tier 3); the amulet and the ring on every other look
HumanLook gearLook(const HumanLook& base, size_t i) {
  HumanLook L = base;
  Item wpn, staff, bow, cloak, ring, amulet;
  WornGear wg;
  const int w = (int)(i % 9u), c = (int)(i / 9u % 4u);
  if (w >= 1 && w <= (int)WeaponType::COUNT) { wpn.kind = ItemKind::Weapon; wpn.sub = (uint8_t)(w - 1); wpn.name = "IRON"; wg.weapon = &wpn; }
  if (w == 7) { staff.kind = ItemKind::Staff; staff.name = "STAFF"; wg.staff = &staff; }
  if (w == 8) { bow.kind = ItemKind::Bow; bow.name = "BOW"; wg.bow = &bow; }
  if (c) {
    cloak.kind = ItemKind::Cloak;
    cloak.name = c == 2 ? "FUR MANTLE" : "CLOAK";
    cloak.tier = (uint8_t)(c == 3 ? 3 : 0);
    wg.cloak = &cloak;
  }
  if (i & 1) { ring.kind = ItemKind::Ring; amulet.kind = ItemKind::Amulet; wg.ring = &ring; wg.amulet = &amulet; }
  wearGear(L, wg, CultureLookup());
  return L;
}
// every people the world generator makes (cult::Atlas::make for each archetype): its musicians (a bard and a sociable
// soul of each sex, census::dress) in their people's instruments, the instruments a seed range may never roll
void censusM(uint64_t seed, std::vector<Who>& out) {
  for (int ar = 0; ar < (int)cult::Archetype::COUNT; ar++) {
    const cult::Culture C = cult::Atlas::make((cult::Archetype)ar, (uint32_t)(seed * 2654435761u + (uint64_t)ar));
    for (int female = 0; female < 2; female++)
      for (uint64_t ps = 0; ps < 2; ps++) {
        HumanLook L;
        std::string name;
        census::dress(L, name, Role::Villager, female != 0, C, C, 7919u * (ps + 1) ^ C.id ^ (uint64_t)ar << 40, census::Rank{});
        Who w{L, 'M', "", 0};
        w.job = (int)(ps ? life::Job::Bard : life::Job::Farmer);
        w.traits = ps ? 0 : life::TR_SOCIABLE;
        w.culture(&C);
        out.push_back(w);
      }
  }
}
// Game::makeLook for every role at one settlement of each culture and at each kingdom's capital
void censusK(uint64_t seed, std::vector<Who>& out) {
  Game g(seed);
  g.newEndlessGame(seed);
  std::set<uint64_t> seen;
  for (size_t si = 0; si < g.world.sites.size(); si++) {
    const Site& s = g.world.sites[si];
    if (!s.settlement()) continue;
    const cult::Culture* C = g.world.cultureOf((int)si);
    if (!s.capital && (!C || !seen.insert(C->id).second)) continue;
    for (int role = 0; role < (int)Role::COUNT; role++)
      for (int slot = 0; slot < 6; slot++) {
        Actor a;
        a.human = true; a.site = (int)si; a.slot = slot; a.bldg = -1;
        Rng rr(seed * 1000003u ^ (uint64_t)si * 7919u ^ (uint64_t)role * 131u ^ (uint64_t)slot * 104729u);
        HomeOps::makeLook(g, a, (Role)role, rr);
        Who w{a.look, 'K', "", 0};
        w.role = role; w.culture(C);
        out.push_back(w);
      }
  }
  // the towns: every culture's first village, town and city, its census dressed by the game's resident path (the trades
  // a start region lacks: the baker, the bard and his people's instrument, the scholar, the lamplighter, the beggar)
  std::set<std::pair<uint64_t, int>> kinds;
  for (size_t si = 0; si < g.world.sites.size(); si++) {
    const Site& s = g.world.sites[si];
    const cult::Culture* C = g.world.cultureOf((int)si);
    if (!s.settlement() || !C || !kinds.insert({C->id, (int)s.type}).second) continue;
    g.world.ensureSiteRecords((int)si);
    const life::Census* c = g.life.census(g.world, (int)si);
    if (!c) continue;
    for (size_t idx = 0; idx < c->res.size(); idx++) {
      Actor a;
      a.human = true;
      if (!g.dressAsResident(a, s.id, (int)idx)) continue;
      Who w{a.look, 'T', s.name, 0};
      w.job = (int)c->res[idx].job;
      w.role = (int)life::jobRole(c->res[idx].job);
      w.traits = c->res[idx].traits;
      w.culture(C);
      out.push_back(w);
    }
  }
}

int coverageCmd(int argc, char** argv) {
  for (int i = 1; i + 1 < argc; i++) {
    if (!std::strcmp(argv[i], "--scope") && !std::strcmp(argv[i + 1], "ladder")) return dollLadderCoverage(argc, argv);
    if (!std::strcmp(argv[i], "--scope") && !std::strcmp(argv[i + 1], "forge")) return dollForgeCoverage(argc, argv);
    if (!std::strcmp(argv[i], "--scope") && !std::strcmp(argv[i + 1], "envelope")) return dollEnvelopeCoverage(argc, argv);
  }
  bool gates = false, all = false;
  uint64_t a = 1, b = 3;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--gates")) gates = true;
    else if (!std::strcmp(argv[i], "--report")) g_report = true;
    else if (!std::strcmp(argv[i], "--scope") && i + 1 < argc) all = !std::strcmp(argv[++i], "all");
    else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
  }
  if (!loadPoolArg(argc, argv)) return 1;
  const int was = art::rigVersion();
  art::setRigVersion(2);
  doll::clearReached();
  Reach reach;
  std::map<std::string, int> missing;
  std::map<char, std::pair<int, int>> bySrc;   // covered / total
  int total = 0, covered = 0;
  std::set<uint64_t> done;
  const auto t0 = std::chrono::steady_clock::now();
  for (uint64_t s = a; s <= b; s++) {
    std::vector<Who> looks;
    censusA(s, looks, true);
    censusB(s, looks);
    if (s == a) censusD(s, looks);
    std::vector<Who> extra;   // (the dead parts' sources beyond the coverage census)
    censusK(s, extra);
    censusM(s, extra);
    size_t ci = 0;
    for (const Who& w : looks) {
      if (w.src == 'A' && !w.site.empty() && w.look.age != 1 && w.look.age != 2) {   // a resident recruited
        static const uint8_t voc[4] = {1, 2, 3, 4};   // sword (fighter), axe (rogue), bow (archer), staff (mage)
        Who e = w;
        e.look.weapon = voc[ci++ % 4u];
        e.src = 'E'; e.who = 'E';
        extra.push_back(e);
      }
      if (w.src == 'D') {
        Who h = w;
        for (size_t gi = 0; gi < 2; gi++) {   // each creator look in two gear sets of the 36
          h.look = gearLook(w.look, ci * 2 + gi);
          h.src = 'G'; h.who = (gi & 1) ? 'E' : 'H';
          extra.push_back(h);
        }
        ci++;
      }
    }
    for (const Who& w : looks)
      reach.add(w, w.src == 'B' ? "cultures" : w.src == 'D' ? "creator" : w.site.empty() ? "world" : "residents");
    for (const Who& w : extra)
      reach.add(w, w.src == 'K' ? "roles" : w.src == 'T' ? "towns" : w.src == 'M' ? "peoples" : w.src == 'G' ? "gear" : "companions");
    for (const Who& w : looks) {
      if (!all && !civil(w.look)) continue;
      // (each distinct look once: the census repeats many)
      const uint64_t k = lookKey(w.look);
      if (!done.insert(k).second) continue;
      total++;
      const std::vector<doll::Missing> m = doll::dollMissing(w.look);
      bySrc[w.src].second++;
      if (m.empty()) { covered++; bySrc[w.src].first++; continue; }
      if (g_report && w.src == 'A')   // (P2c i2) the world's own folk drawn on v1: what each lacks
        std::printf("  world look on v1: outfit %d armorStyle %d helmet %d shield %d weapon %d: %s%s\n", (int)w.look.outfit, (int)w.look.armorStyle,
                    (int)w.look.helmet, (int)w.look.shield, (int)w.look.weapon, m[0].part.empty() ? (std::string(doll::roleName(m[0].role)) + "=" + m[0].value).c_str() : m[0].what().c_str(),
                    m.size() > 1 ? " ..." : "");
      std::set<std::string> once;
      for (const doll::Missing& x : m) {
        const std::string id = x.part.empty() ? std::string(doll::roleName(x.role)) + "=" + x.value + " (" + doll::bodySetName(x.body) + ")" : x.what();
        if (once.insert(id).second) missing[id]++;
      }
    }
  }
  // (wave-2 fix round, D79) C1's dead parts count over sources A-E, F (the gates' matrix with the forge looks), L (the kit lines,
  // seeds 1..3, with the envelope looks) and M8-M12 (the arms matrices) too
  if (all) {
    Who w;
    for (const auto& tl : dollSpecLooks(dollSliceLooksText())) { w.look = tl.second; w.src = 'F'; reach.add(w, "F gates"); }
    for (const auto& tl : dollSpecLooks(dollSourceLText(1, 3))) { w.look = tl.second; w.src = 'L'; reach.add(w, "L kit lines"); }
    for (const auto& tl : dollSpecLooks(dollArmsLooksText())) { w.look = tl.second; w.src = 'M'; reach.add(w, "M matrices"); }
  }
  std::vector<std::string> dead, p2c, reserve;
  {
    const std::vector<std::string> d = doll::deadParts();
    const std::set<std::string> ds(d.begin(), d.end());
    for (int p = 0; p < doll::partCount(); p++) {
      if (!ds.count(doll::partName(p))) {   // alive: the sources that reach it (a part one source alone reaches)
        const uint32_t m = (size_t)p < reach.partMask.size() ? reach.partMask[(size_t)p] : 0;
        for (size_t s = 0; s < reach.names.size(); s++) if (m == (1u << s)) reach.src[s].only++;
        continue;
      }
      // (D79) a drawn part no source reaches by design: tagged `reserve` in its map, or on the planner's list (motif plain and
      // straps; a pauldron size no source asks): kept, listed, never dead
      {
        const std::string nm = doll::partName(p);
        const char* why = doll::partTagged(p, "reserve") ? "tags reserve"
                          : (nm.rfind("armour/motif/", 0) == 0 && (nm.size() > 6 && (nm.compare(nm.size() - 6, 6, "/plain") == 0 || (nm.size() > 7 && nm.compare(nm.size() - 7, 7, "/straps") == 0))))
                              ? "the planner's list (D79: motif plain and straps)"
                          : nm.rfind("armour/shared/pauldron_", 0) == 0 ? "the planner's list (D79: a pauldron size no source asks)"
                          // (wave-2 fix round 3, D79 "by design") forge round 2 retired the cut, edge, limb and fittings of every
                          // culture alloy (contracts 3c.1; gear_forge.cpp alloyStyle) and no hero style's column names these
                          // values (doll_forge.h hero()), so no look of any world asks them; kept drawn (D49)
                          : (nm == "forge/cut/splinted" || nm == "forge/edge/fringed" || nm == "forge/edge/spiked" || nm == "forge/fit/knot")
                              ? "D79 by design: a culture alloy's retired forge value (contracts 3c.1), no hero style names it"
                              : nullptr;
        if (why && all) { reserve.push_back(nm + " (" + why + ")"); continue; }
      }
      const bool waits = reach.wait.count(p) && !reach.why.count(p);   // wanted only by looks P2c holds back
      if (!all && (p2cPart(p) || waits)) {
        p2c.push_back(std::string(doll::partName(p)) + (p2cPart(p) ? " (armour)" : " (waits on P2c: " + reach.wait[p] + ")"));
        continue;
      }
      dead.push_back(doll::partName(p));
    }
  }
  art::setRigVersion(was);
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  const double share = total ? 100.0 * covered / total : 0.0;
  std::printf("B6 doll coverage (%s, seeds %llu..%llu): %d of %d distinct looks covered (%.1f %%)", all ? "all" : "civil", (unsigned long long)a,
              (unsigned long long)b, covered, total, share);
  for (const auto& kv : bySrc) std::printf("; %c %d/%d", kv.first, kv.second.first, kv.second.second);
  std::printf("; %zu dead part(s)", dead.size());
  if (all) std::printf("; %zu reserve (D79)", reserve.size());
  if (!all) std::printf("; %zu P2c part(s) apart", p2c.size());
  std::printf(" (%.0f s)\n", secs);
  for (size_t s = 0; s < reach.names.size(); s++)
    if (g_report)
      std::printf("  counted source: %-10s %6d looks, %6d drawn on the doll, %6d posture recipes; %3d parts it alone reaches\n", reach.names[s].c_str(),
                  reach.src[s].looks, reach.src[s].doll, reach.src[s].poses, reach.src[s].only);
  std::vector<std::pair<int, std::string>> top;
  for (const auto& kv : missing) top.push_back({kv.second, kv.first});
  std::sort(top.begin(), top.end(), [](const auto& x, const auto& y) { return x.first != y.first ? x.first > y.first : x.second < y.second; });
  for (size_t i = 0; i < top.size() && (g_report || i < 25); i++) std::printf("  missing in %5d looks: %s\n", top[i].first, top[i].second.c_str());
  if (top.size() > 25 && !g_report) std::printf("  ... %zu more (--report)\n", top.size() - 25);
  for (size_t i = 0; i < dead.size() && (g_report || i < 10); i++) {
    int p = -1;
    for (int q = 0; q < doll::partCount() && p < 0; q++) if (dead[i] == doll::partName(q)) p = q;
    const auto it = reach.why.find(p);
    const std::string why = it != reach.why.end() ? it->second : "no source asks for it";
    std::printf("  dead part: %s%s%s%s\n", dead[i].c_str(), g_report ? " (" : "", g_report ? why.c_str() : "", g_report ? ")" : "");
  }
  for (size_t i = 0; i < p2c.size() && g_report; i++) std::printf("  P2c part: %s\n", p2c[i].c_str());
  for (const std::string& x : reserve) std::printf("  reserve: %s\n", x.c_str());
  for (const std::string& x : doll::fxViewParts()) std::printf("  drawn by art_fx (contracts 4; no recipe asks it): %s\n", x.c_str());
  int poseMiss = 0;
  for (const auto& kv : reach.poseV1) poseMiss += kv.second;
  std::printf("  postures drawn on v1: %d (%zu body x posture)\n", poseMiss, reach.poseV1.size());
  for (const auto& kv : reach.poseV1) std::printf("  posture on v1 in %5d looks: %s\n", kv.second, kv.first.c_str());
  if (gates && (share < 100.0 || !dead.empty() || poseMiss)) {
    std::printf("FAIL: B6 %.1f %% covered (100), %zu dead parts (0), %d postures on v1 (0)\n", share, dead.size(), poseMiss);
    return 1;
  }
  return 0;
}
RPG_TEST_CMD("--doll-coverage", "P2b B6 doll coverage of a census [--seeds A..B] [--scope civil|all|ladder] [--gates] [--pool f] [--report]", coverageCmd);

int varietyCmd(int argc, char** argv) {
  bool gates = false;
  uint64_t a = 1, b = 3;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--gates")) gates = true;
    else if (!std::strcmp(argv[i], "--report")) g_report = true;
    else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
  }
  Gate g7{"B7 variety"};
  std::set<int> styles;
  std::set<int> beards;
  std::set<int> heads[2];
  std::set<std::string> garments[2];
  int settlements = 0;
  // (P2c wave 2 round 2, DOLL-C) B7's "seen" counts (18 of 24 hair styles, all 9 beards, every head type and garment) are
  // defined over the census of seeds 1..20 (VISION_PLAN 15.36 B7: `--doll-variety --seeds 1..20`): three villages hold 5
  // of the 9 beards whatever the art does (the judge's w2 r1 item: "5 beards seen (9)" at 1..3, the same at w0). The
  // per-settlement checks run on the seeds asked; the seen counts always run on 1..20 plus the seeds asked, never fewer
  const uint64_t seenLo = std::min<uint64_t>(a, 1), seenHi = std::max<uint64_t>(b, 20);
  for (uint64_t s = seenLo; s <= seenHi; s++) {
    const bool own = s >= a && s <= b;   // the per-settlement checks: the seeds asked
    std::vector<Who> looks;
    censusA(s, looks, true);
    std::map<std::string, std::vector<const Who*>> bySite;
    for (const Who& w : looks) if (!w.site.empty()) bySite[w.site].push_back(&w);
    for (const auto& kv : bySite) {
      if (own) settlements++;
      std::map<std::string, int> recipes;
      int adults = 0;
      for (const Who* w : kv.second) {
        const HumanLook& L = w->look;
        if (!civil(L)) continue;
        const int style = doll::hairStyleOf(L.hair, L.hairVar);
        char rk[160];
        std::snprintf(rk, sizeof rk, "%d.%d.%d.%d.%d.%08x.%d.%d.%d.%d", L.body, L.age, L.head, L.nose, L.brow, (unsigned)L.hairColor, style, L.beard ? L.beardStyle + 1 : 0,
                      L.headwear, L.hood ? 1 : 0);
        recipes[rk]++;
        adults++;
        if (L.age != 1) styles.insert(style);
        if (L.beard && L.body == 0 && L.age != 1 && L.age != 2) beards.insert(L.beardStyle);
        if (L.age != 1) heads[L.body ? 1 : 0].insert(L.head);
        const doll::DollRecipe r = doll::resolve(L);
        for (const doll::Need& n : r.needs) if (n.role == doll::Role::Garment) garments[L.body ? 1 : 0].insert(n.value);
      }
      int largest = 0;
      for (const auto& rc : recipes) largest = std::max(largest, rc.second);
      const int tier = kv.second.empty() ? 0 : kv.second[0]->tier;
      const int limit = tier == 0 ? 2 : std::max(2, (int)std::floor(adults * 0.03));
      if (g_report && own) std::printf("  %s (%s): %d folk, %zu head recipes, largest group %d (limit %d)\n", kv.first.c_str(), tier == 0 ? "village" : tier == 1 ? "town" : "city", adults,
                                recipes.size(), largest, limit);
      if (!own) continue;
      if (largest > limit) g7.bad(kv.first + fmt(": %.0f folk share one head recipe (%.0f)", largest, limit)); else g7.ok();
    }
  }
  std::printf("B7 variety (seeds %llu..%llu): %d settlements; seen over seeds %llu..%llu: %zu of 24 hair styles; %zu of 9 beards; head types man %zu woman %zu of 9; garments man %zu (13) woman %zu (14)\n",
              (unsigned long long)a, (unsigned long long)b, settlements, (unsigned long long)seenLo, (unsigned long long)seenHi, styles.size(), beards.size(), heads[0].size(), heads[1].size(), garments[0].size(), garments[1].size());
  if (styles.size() < 18) g7.bad(fmt("%.0f hair styles seen (18 of 24)", (double)styles.size())); else g7.ok();
  if (beards.size() < 9) g7.bad(fmt("%.0f beards seen (9)", (double)beards.size())); else g7.ok();
  if (heads[0].size() < 9 || heads[1].size() < 9) g7.bad(fmt("head types seen: man %.0f, woman %.0f (9 each)", (double)heads[0].size(), (double)heads[1].size())); else g7.ok();
  if (garments[0].size() < 13 || garments[1].size() < 14) g7.bad(fmt("civilian garments seen: man %.0f (13), woman %.0f (14)", (double)garments[0].size(), (double)garments[1].size())); else g7.ok();
  std::printf("%s %-24s %4d checks, %4d failed%s%s\n", g7.fails ? "FAIL:" : "ok   ", g7.id, g7.checks, g7.fails, g7.fails ? "; first: " : "", g7.first.c_str());
  // (P2c wave 2 round 2, DOLL-C) C11 fighters' faces (PLAN "Gates" C11; contracts: `--doll-variety` adds C11): no head recipe
  // (type, nose, brows, beard, hair colour) on more than 2 of a settlement's watch (12 men as spawnHuman makes them, every
  // settlement within 160 tiles of the start) or of a kingdom's squad of 8 soldiers (the folk sim's F17, on the seeds asked)
  Gate g11{"C11 fighters' faces"};
  int c11Men = 0, c11Worst = 0;
  for (uint64_t seed = a; seed <= b; seed++) {
    Game g(seed);
    g.newEndlessGame(seed);
    World& w = g.world;
    auto recipe = [](const HumanLook& L) { return fmt("%.0f.%.0f.%.0f.%.0f", (double)L.head, (double)L.nose, (double)L.brow, (double)(L.beard ? L.beardStyle : 99)) + fmt(".%.0f", (double)L.hairColor); };
    auto group = [&](const std::vector<HumanLook>& men, const std::string& where) {
      std::map<std::string, int> n;
      for (const HumanLook& L : men) n[recipe(L)]++;
      int worst = 0;
      std::string worstKey;
      for (const auto& kv : n) if (kv.second > worst) { worst = kv.second; worstKey = kv.first; }
      c11Men += (int)men.size();
      c11Worst = std::max(c11Worst, worst);
      if (worst > 2) g11.bad(fmt("seed %.0f ", (double)seed) + where + fmt(": %.0f men share ", (double)worst) + worstKey); else g11.ok();
    };
    const Site& home = w.sites[(size_t)w.startSite];
    for (size_t si = 0; si < w.sites.size(); si++) {
      const Site& s = w.sites[si];
      if (s.type != SiteType::Village && s.type != SiteType::Town && s.type != SiteType::City) continue;
      if (std::abs(s.r.cx() - home.r.cx()) > 160 || std::abs(s.r.cy() - home.r.cy()) > 160) continue;
      std::vector<HumanLook> guards;
      for (int k = 0; k < 12; k++) {
        Actor x;
        x.id = 96000 + k; x.human = true; x.npc = true; x.role = Role::Guard; x.site = (int)si; x.slot = 40 + k; x.bldg = -1;
        if (s.kingdom >= 0) x.realm = w.kingdoms[(size_t)s.kingdom].id;
        const uint64_t key = ((uint64_t)(x.site + 1) << 24) ^ ((uint64_t)(x.bldg + 1) << 12) ^ (uint64_t)x.slot;
        Rng rr(hash32((uint32_t)key) ^ (uint32_t)seed);
        HomeOps::makeLook(g, x, Role::Guard, rr);
        guards.push_back(x.look);
      }
      group(guards, "the watch of " + s.name);
    }
    for (size_t ki = 0; ki < w.kingdoms.size() && ki < 8; ki++) {
      std::vector<HumanLook> squad;
      for (int k = 0; k < 8; k++) {
        Actor x;
        x.id = 95000 + k; x.human = true; x.npc = true; x.role = Role::Soldier; x.site = -1; x.bldg = -1; x.slot = 7000 + (int)ki * 16 + k;
        x.realm = w.kingdoms[ki].id;
        Rng rr(hash32((uint32_t)x.slot * 2654435761u ^ (uint32_t)x.realm ^ (uint32_t)(x.realm >> 32)) ^ (uint32_t)seed);
        HomeOps::makeLook(g, x, Role::Soldier, rr);
        squad.push_back(x.look);
      }
      group(squad, fmt("a squad of kingdom %.0f", (double)ki));
    }
  }
  std::printf("C11 fighters' faces (seeds %llu..%llu): %d men in %d groups, largest shared head recipe %d (2)\n", (unsigned long long)a,
              (unsigned long long)b, c11Men, g11.checks, c11Worst);
  if (!g11.checks) g11.bad("no watch or squad measured");
  std::printf("%s %-24s %4d checks, %4d failed%s%s\n", g11.fails ? "FAIL:" : "ok   ", g11.id, g11.checks, g11.fails, g11.fails ? "; first: " : "", g11.first.c_str());
  return (g7.fails || g11.fails) ? 1 : 0;   // (wave 2 r3) a printed FAIL always fails
}
RPG_TEST_CMD("--doll-variety", "P2b B7 variety per settlement [--seeds A..B] [--gates] [--report]", varietyCmd);

}  // namespace

// (P2c wave 2, DOLL-C) C1's census sources as distinct looks, for the envelope's caps on every look (`rpg_test --doll-envelope
// --scope all`, test_doll_envelope.cpp): A the world and the start region's residents, B every culture x role x sex x age,
// D the creator (first seed), E residents recruited with each vocation's weapon, G the creator in worn gear, K the roles
// (Game::makeLook) and the towns; each distinct look once (lookKey)
void dollCensusLooks(uint64_t a, uint64_t b, std::vector<std::pair<char, art::HumanLook>>& out) {
  std::set<uint64_t> done;
  auto add = [&](char src, const HumanLook& L) { if (done.insert(lookKey(L)).second) out.push_back({src, L}); };
  for (uint64_t s = a; s <= b; s++) {
    std::vector<Who> looks, extra;
    censusA(s, looks, true);
    censusB(s, looks);
    if (s == a) censusD(s, looks);
    censusK(s, extra);
    size_t ci = 0;
    for (const Who& w : looks) {
      add(w.src, w.look);
      if (w.src == 'A' && !w.site.empty() && w.look.age != 1 && w.look.age != 2) {
        static const uint8_t voc[4] = {1, 2, 3, 4};
        HumanLook e = w.look;
        e.weapon = voc[ci++ % 4u];
        add('E', e);
      }
      if (w.src == 'D') {
        for (size_t gi = 0; gi < 2; gi++) add('G', gearLook(w.look, ci * 2 + gi));
        ci++;
      }
    }
    for (const Who& w : extra) add(w.src, w.look);
  }
}
