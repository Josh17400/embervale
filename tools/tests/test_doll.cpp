// rpg_test --doll, --doll-coverage and --doll-variety (P2b New People, VISION_PLAN 15.36 phase A item 6: the lead's
// frozen harness; an area reports a harness problem instead of editing it). The doll (rpg/art/doll/doll.h) measured
// from its atlases, its pool and a census of looks.
//   rpg_test --doll [--gates] [--parity] [--report] [--pool FILE] [--dump DIR] [--frames DIR]
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
// --gates: B1 parity; B2 heights (every body set, people, age and build in every facing: STYLE_P2 3); faces and hands
//   (A3's face rows and the 3 x 2 hands, from the marks); B8 craft on composed frames (no orphan, no #000000, step-0 luma
//   70 or less, 48 colours or fewer per figure); B9's structure (no civilian garment part shared by the man and the
//   woman); B10 atlases (LOCO 128, RUN 64, DEATH 48, POSTURE 64 KB per look) and speed (one facing's LOCO bake p95 at or
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
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/art/doll/doll.h"
#include "rpg/art/doll/doll_internal.h"   // (B6) withPosture, the pool's part roles
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/gear_look.h"
#include "rpg/sim/life.h"
#include "tools/tests/tests.h"

namespace census {
struct Rank { bool royal = false, capital = false; };
void dress(art::HumanLook& L, std::string& name, Role r, bool female, const cult::Culture& C, const cult::Culture& owner, uint64_t seed,
           const Rank& rank);
}  // namespace census

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
    // the union of both boxes aligned at the pivots
    int diff = 0;
    const int x0 = -std::max(f.px, rpx), x1 = std::max(f.c.w - f.px, rw - rpx), y0 = -std::max(f.py, rpy), y1 = std::max(f.c.h - f.py, rh - rpy);
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) {
        const int ax = f.px + x, ay = f.py + y, rx = rpx + x, ry = rpy + y;
        uint32_t ca = ax >= 0 && ay >= 0 && ax < f.c.w && ay < f.c.h ? f.c.get(ax, ay) : 0;
        uint32_t cr = rx >= 0 && ry >= 0 && rx < rw && ry < rh ? ref[(size_t)ry * rw + rx] : 0;
        if (!(ca >> 24)) ca = 0;
        if (!(cr >> 24)) cr = 0;
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
  if (doll::SLOT_COUNT != 21 || std::string(doll::slotName(Slot::Fx)) != "fx" || std::string(doll::slotName(Slot::Skin)) != "skin") cfail("the slot table");
  if (doll::LAYER_COUNT != 24 || std::string(doll::layerName(doll::Layer::Fx)) != "fx" || std::string(doll::layerName(doll::Layer::FxBack)) != "fxBack") cfail("the layer table");
  if (doll::ANCHOR_COUNT != 8 || doll::REGION_COUNT != 12 || doll::regionChar(doll::Region::Sole) != 'O') cfail("the anchors and regions");
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
void craftFrame(Gate& g, const Canvas& c, const std::string& who) {
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
    }
  if (orphans) g.bad(who + fmt(": %.0f orphan pixel(s)", orphans));
  else if (black) g.bad(who + ": #000000");
  else if (cols.size() > 48) g.bad(who + fmt(": %.0f colours (48)", (double)cols.size()));
  else g.ok();
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
    for (const B& b : {B{RigPart::Loco, 128, "LOCO"}, B{RigPart::Run, 64, "RUN"}, B{RigPart::Death, 48, "DEATH"}, B{RigPart::Action, 512, "ACTION"}}) {
      if (b.p == RigPart::Action && !armed) continue;
      const RigSheet s = doll::compose(r, b.p);
      const std::string who = std::string(armed ? "armed " : "") + doll::bodySetName(r.body) + fmt(" look %.0f ", (double)li);
      if (s.empty()) { g.bad(who + b.n + ": no atlas"); continue; }
      if (s.bytes() > b.kb * 1024) g.bad(who + b.n + fmt(" %.1f KB (%.0f)", s.bytes() / 1024.0, (double)b.kb)); else g.ok();
      // the drawn run: in ACTION (walk variant 2, six frames a facing), never in RUN
      if (armed && b.p == RigPart::Action)
        for (int fc = 0; fc < 4; fc++) {
          // (count and frame fall back to variant 0 for a missing variant: the drawn run is its own frames, not the walk's)
          const art::FrameRect* f2 = s.frame(fc, Clip::Walk, 2, 0);
          if (s.count(fc, Clip::Walk, 2) == 6 && f2 && f2 != s.frame(fc, Clip::Walk, 1, 0) && f2 != s.frame(fc, Clip::Walk, 0, 0)) g.ok();
          else g.bad(who + fmt("ACTION facing %.0f: %.0f drawn-run frames (6 of its own)", fc, s.count(fc, Clip::Walk, 2)));
        }
      if (armed && b.p == RigPart::Run) {   // the sheathed run alone: six walk frames a facing, nothing else
        int run0 = 0;
        for (int fc = 0; fc < 4; fc++) run0 += s.count(fc, Clip::Walk, 0);
        if (run0 == 24 && s.frames.size() == 24) g.ok();
        else g.bad(who + fmt("RUN: %.0f frames, %.0f of them the sheathed run (24, 24)", (double)s.frames.size(), run0));
      }
    }
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
  // pigtails and a robe), within LOCO 128, RUN 64, DEATH 48 and POSTURE 64 KB
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
    const double kbs[4] = {128, 64, 48, 64};
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
  const doll::PoolStats ps = doll::poolStats();
  if (ps.bytes > 1024 * 1024) g.bad(fmt("the pool %.2f MB (1.0)", ps.bytes / 1048576.0)); else g.ok();
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
    for (int fc = 0; fc < 4; fc++) {
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
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--gates")) gates = true;
    else if (!std::strcmp(argv[i], "--parity")) par = true;
    else if (!std::strcmp(argv[i], "--report")) g_report = true;
    else if (!std::strcmp(argv[i], "--dump") && i + 1 < argc) dump = argv[++i];
    else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) frames = argv[++i];
  }
  if (!loadPoolArg(argc, argv)) return 1;
  g_contract = 0;
  contract();
  std::printf("doll contract: %d failure(s)\n", g_contract);
  int gateFails = 0;
  const int was = art::rigVersion();
  art::setRigVersion(2);
  if (frames) dumpFrames(frames);
  if (par || gates) {
    Gate b1{"B1 parity"};
    parity(b1, dump);
    std::printf("%s %-24s %4d checks, %4d failed%s%s\n", b1.fails ? "FAIL:" : "ok   ", b1.id, b1.checks, b1.fails, b1.fails ? "; first: " : "", b1.first.c_str());
    gateFails += b1.fails ? 1 : 0;
  }
  if (gates) {
    Gate b2{"B2 heights"}, fc{"faces"}, hd{"hands"}, b8{"B8 craft"}, b9{"B9 fitted pairs"}, b10{"B10 atlas and speed"}, rp{"ramps dE 6"}, b11{"B11 leg mass"};
    gateHeights(b2);
    gateFaces(fc, hd);
    gateCraft(b8);
    gateFit(b9);
    gateAtlas(b10);
    gateRamps(rp);
    gateLegs(b11);
    for (Gate* g : {&b2, &fc, &hd, &b8, &b9, &b10, &rp, &b11}) {
      std::printf("%s %-24s %4d checks, %4d failed%s%s\n", g->fails ? "FAIL:" : "ok   ", g->id, g->checks, g->fails, g->fails ? "; first: " : "", g->first.c_str());
      gateFails += g->fails ? 1 : 0;
    }
  }
  art::setRigVersion(was);
  const int bad = g_contract + gateFails;
  std::printf(bad ? "doll: %d failure(s)\n" : "doll: 0 failures\n", bad);
  return bad ? 1 : 0;
}
RPG_TEST_CMD("--doll", "P2b doll contract [--parity: B1] [--gates: B1 B2 faces B8 B9 B10 ramps B11] [--report] [--pool f] [--dump dir] [--frames dir]", dollCmd);

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
      std::set<std::string> once;
      for (const doll::Missing& x : m) {
        const std::string id = x.part.empty() ? std::string(doll::roleName(x.role)) + "=" + x.value + " (" + doll::bodySetName(x.body) + ")" : x.what();
        if (once.insert(id).second) missing[id]++;
      }
    }
  }
  std::vector<std::string> dead, p2c;
  {
    const std::vector<std::string> d = doll::deadParts();
    const std::set<std::string> ds(d.begin(), d.end());
    for (int p = 0; p < doll::partCount(); p++) {
      if (!ds.count(doll::partName(p))) {   // alive: the sources that reach it (a part one source alone reaches)
        const uint32_t m = (size_t)p < reach.partMask.size() ? reach.partMask[(size_t)p] : 0;
        for (size_t s = 0; s < reach.names.size(); s++) if (m == (1u << s)) reach.src[s].only++;
        continue;
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
  if (gates && (share < 100.0 || !dead.empty())) { std::printf("FAIL: B6 %.1f %% covered (100), %zu dead parts (0)\n", share, dead.size()); return 1; }
  return 0;
}
RPG_TEST_CMD("--doll-coverage", "P2b B6 doll coverage of a census [--seeds A..B] [--scope civil|all] [--gates] [--pool f] [--report]", coverageCmd);

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
  for (uint64_t s = a; s <= b; s++) {
    std::vector<Who> looks;
    censusA(s, looks, true);
    std::map<std::string, std::vector<const Who*>> bySite;
    for (const Who& w : looks) if (!w.site.empty()) bySite[w.site].push_back(&w);
    for (const auto& kv : bySite) {
      settlements++;
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
      if (g_report) std::printf("  %s (%s): %d folk, %zu head recipes, largest group %d (limit %d)\n", kv.first.c_str(), tier == 0 ? "village" : tier == 1 ? "town" : "city", adults,
                                recipes.size(), largest, limit);
      if (largest > limit) g7.bad(kv.first + fmt(": %.0f folk share one head recipe (%.0f)", largest, limit)); else g7.ok();
    }
  }
  std::printf("B7 variety (seeds %llu..%llu): %d settlements; %zu of 24 hair styles; %zu of 9 beards; head types man %zu woman %zu of 9; garments man %zu (13) woman %zu (14)\n",
              (unsigned long long)a, (unsigned long long)b, settlements, styles.size(), beards.size(), heads[0].size(), heads[1].size(), garments[0].size(), garments[1].size());
  if (styles.size() < 18) g7.bad(fmt("%.0f hair styles seen (18 of 24)", (double)styles.size())); else g7.ok();
  if (beards.size() < 9) g7.bad(fmt("%.0f beards seen (9)", (double)beards.size())); else g7.ok();
  if (heads[0].size() < 9 || heads[1].size() < 9) g7.bad(fmt("head types seen: man %.0f, woman %.0f (9 each)", (double)heads[0].size(), (double)heads[1].size())); else g7.ok();
  if (garments[0].size() < 13 || garments[1].size() < 14) g7.bad(fmt("civilian garments seen: man %.0f (13), woman %.0f (14)", (double)garments[0].size(), (double)garments[1].size())); else g7.ok();
  std::printf("%s %-24s %4d checks, %4d failed%s%s\n", g7.fails ? "FAIL:" : "ok   ", g7.id, g7.checks, g7.fails, g7.fails ? "; first: " : "", g7.first.c_str());
  return gates && g7.fails ? 1 : 0;
}
RPG_TEST_CMD("--doll-variety", "P2b B7 variety per settlement [--seeds A..B] [--gates] [--report]", varietyCmd);

}  // namespace
