// rpg_test --doll, --doll-coverage and --doll-variety (P2b New People, VISION_PLAN 15.36 phase A item 6: the lead's
// frozen harness; an area reports a harness problem instead of editing it). The doll (rpg/art/doll/doll.h) measured
// from its atlases, its pool and a census of looks.
//   rpg_test --doll [--gates] [--parity] [--report] [--pool FILE] [--dump DIR]
//   rpg_test --doll-coverage [--seeds A..B] [--scope civil|all] [--gates] [--pool FILE] [--report]
//   rpg_test --doll-variety [--seeds A..B] [--gates] [--report]
// --doll always (CI): the contract: the pool parses, the frozen tables (slots, regions, layers, anchors, roles, hair styles
//   24 from the 8 families, the names), the A3 palettes against docs/ref/style_a3/palettes.json's colours, rampA3 giving
//   A3's hand ramps back for their bases, the walk table's defaults (man 19, woman 20, run 36 px), the look fields (v1 and
//   rig 1 keys ignore them; rig 2 keys see them), the elder's height, an empty pool covering nothing.
// --parity: B1's pixels: the A3 hero and woman composed with A3's palettes against docs/ref/style_a3/frames: the 48 kept
//   frames (8 idles, 24 front and back walks, 8 side contact frames, 8 swing keys) at 0 px differing; the 16 re-registered
//   side frames reported for the review beside A3's GIF. --dump DIR writes each frame beside its reference (x4).
// --gates: B1 parity; B2 heights (every body set, people, age and build in every facing: STYLE_P2 3); faces and hands
//   (A3's face rows and the 3 x 2 hands, from the marks); B8 craft on composed frames (no orphan, no #000000, step-0 luma
//   70 or less, 48 colours or fewer per figure); B9's structure (no civilian garment part shared by the man and the
//   woman); B10 atlases (LOCO 128, RUN 64, DEATH 48, POSTURE 64 KB per look) and speed (one facing's LOCO bake p95 at or
//   under the v1 sheet's), the pool (1.0 MB or less); the ramp generator within delta E 6 per step of A3's 14 ramps.
// --doll-coverage: B6: the census's looks (A the world's humans after 60 frames and the start region's census residents;
//   B every culture of the seeds x every role x both sexes x child, youth, adult and elder x 8 seeds; D the creator's
//   hair, beard, build, people, skin and sex, with and without each homeland; E the party, story people and statues as the
//   world holds them), the share whose LOCO, DEATH (RUN when it may run, ACTION when armed) compose with nothing missing,
//   the missing values by count, and the dead parts. --scope civil (default) leaves out armour, helms, shields and the
//   culture weapon forms (P2c's). --gates: 100 % and 0 dead parts.
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
      for (int i = 0; i < 6; i++) v.push_back({who, nullptr, RigPart::Loco, f, Clip::Walk, i, f < 2 || i == 0 || i == 3});
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
      if (g_report) std::printf("  B1 review %s: %d px differ from A3 (re-registered: judged beside A3's GIF)\n", nm.c_str(), diff);
    }
  }
  std::printf("  B1 parity: kept frames %d px differing in all; %d re-registered side frames for review (%d differ from A3)\n", keptDiff, review, reviewDiff);
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
  std::vector<double> v1ms, locoMs;
  for (const HumanLook& L : looks) {
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
    for (const B& b : {B{RigPart::Loco, 128, "LOCO"}, B{RigPart::Run, 64, "RUN"}, B{RigPart::Death, 48, "DEATH"}}) {
      const RigSheet s = doll::compose(r, b.p);
      if (s.empty()) { g.bad(std::string(b.n) + ": no atlas"); continue; }
      if (s.bytes() > b.kb * 1024) g.bad(std::string(b.n) + fmt(" %.0f KB (%.0f)", s.bytes() / 1024.0, (double)b.kb)); else g.ok();
    }
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
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--gates")) gates = true;
    else if (!std::strcmp(argv[i], "--parity")) par = true;
    else if (!std::strcmp(argv[i], "--report")) g_report = true;
    else if (!std::strcmp(argv[i], "--dump") && i + 1 < argc) dump = argv[++i];
  }
  if (!loadPoolArg(argc, argv)) return 1;
  g_contract = 0;
  contract();
  std::printf("doll contract: %d failure(s)\n", g_contract);
  int gateFails = 0;
  const int was = art::rigVersion();
  art::setRigVersion(2);
  if (par || gates) {
    Gate b1{"B1 parity"};
    parity(b1, dump);
    std::printf("%s %-24s %4d checks, %4d failed%s%s\n", b1.fails ? "FAIL:" : "ok   ", b1.id, b1.checks, b1.fails, b1.fails ? "; first: " : "", b1.first.c_str());
    gateFails += b1.fails ? 1 : 0;
  }
  if (gates) {
    Gate b2{"B2 heights"}, fc{"faces"}, hd{"hands"}, b8{"B8 craft"}, b9{"B9 fitted pairs"}, b10{"B10 atlas and speed"}, rp{"ramps dE 6"};
    gateHeights(b2);
    gateFaces(fc, hd);
    gateCraft(b8);
    gateFit(b9);
    gateAtlas(b10);
    gateRamps(rp);
    for (Gate* g : {&b2, &fc, &hd, &b8, &b9, &b10, &rp}) {
      std::printf("%s %-24s %4d checks, %4d failed%s%s\n", g->fails ? "FAIL:" : "ok   ", g->id, g->checks, g->fails, g->fails ? "; first: " : "", g->first.c_str());
      gateFails += g->fails ? 1 : 0;
    }
  }
  art::setRigVersion(was);
  const int bad = g_contract + gateFails;
  std::printf(bad ? "doll: %d failure(s)\n" : "doll: 0 failures\n", bad);
  return bad ? 1 : 0;
}
RPG_TEST_CMD("--doll", "P2b doll contract [--parity: B1] [--gates: B1 B2 faces B8 B9 B10 ramps] [--report] [--pool f] [--dump dir]", dollCmd);

// ---------------------------------------------------------------- the census (B6, B7)
bool civil(const HumanLook& L) {
  const bool armour = L.armorStyle || L.bodyForm || L.outfit == art::Outfit::Leather || L.outfit == art::Outfit::Chain || L.outfit == art::Outfit::Plate ||
                      L.outfit == art::Outfit::Elven || L.outfit == art::Outfit::Ebony || L.outfit == art::Outfit::Guard;
  if (armour || L.helmet || L.helmStyle || L.helmForm || L.shield || L.shieldStyle || L.shieldForm) return false;
  if (L.bladeForm || L.polearmForm > 1 || L.bowForm || L.pauldron || L.skirt || L.crest || L.gloves || L.boots) return false;
  return true;
}
struct Who { HumanLook look; char src; std::string site; int tier = 0; };   // tier: 0 village, 1 town, 2 city

void censusA(uint64_t seed, std::vector<Who>& out, bool withResidents) {
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  for (int f = 0; f < 60; f++) { g.update(SIM_DT, Input()); g.events.clear(); g.mode = Mode::Play; }
  for (const Actor& x : g.actors) if (x.human) out.push_back({x.look, x.comp ? 'E' : 'A', "", 0});
  if (!withResidents) return;
  const Site& home = g.world.sites[(size_t)g.world.startSite];
  for (size_t si = 0; si < g.world.sites.size(); si++) {
    const Site& s = g.world.sites[si];
    if (s.type != SiteType::Village && s.type != SiteType::Town && s.type != SiteType::City) continue;
    if (std::abs(s.r.cx() - home.r.cx()) > 160 || std::abs(s.r.cy() - home.r.cy()) > 160) continue;
    life::Census c;
    life::buildCensus(g.world, (int)si, c);
    const int tier = s.type == SiteType::Village ? 0 : s.type == SiteType::Town ? 1 : 2;
    for (const life::Resident& r : c.res) {
      Actor a;
      a.human = true; a.site = (int)si; a.role = life::jobRole(r.job);
      for (uint32_t k = 0; k < 12; k++) {   // (life_game.cpp dressResident: the look from the resident's seed, its census sex)
        Rng rr(r.lookSeed + k * 7919u);
        Rng probe = rr;
        const bool fem = probe.f() < 0.45f;
        if (fem != r.female && k + 1 < 12) continue;
        HomeOps::makeLook(g, a, a.role, rr);   // (Game::makeLook through its friend)
        break;
      }
      if (r.job == life::Job::Beggar) a.look.outfit = art::Outfit::Rags;
      // the census ages (VISION_PLAN 15.36: youth for children aged 10-14, elder from 60 or Job::Elder)
      if (r.job == life::Job::Child) a.look.age = (r.age >= 10 && r.age <= 14) ? 2 : 1;
      else if (r.job == life::Job::Elder || r.age >= 60) a.look.age = 3;
      out.push_back({a.look, 'A', s.name, tier});
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
  std::map<std::string, int> missing;
  std::map<char, std::pair<int, int>> bySrc;   // covered / total
  int total = 0, covered = 0;
  std::set<uint64_t> done;
  for (uint64_t s = a; s <= b; s++) {
    std::vector<Who> looks;
    censusA(s, looks, true);
    censusB(s, looks);
    if (s == a) censusD(s, looks);
    for (const Who& w : looks) {
      if (!all && !civil(w.look)) continue;
      // (each distinct look once: the census repeats many)
      const uint64_t k = w.look.key() ^ (uint64_t)w.look.head << 7 ^ (uint64_t)w.look.hairVar << 13 ^ (uint64_t)w.look.age << 19 ^ (uint64_t)w.look.garb << 23;
      if (!done.insert(k).second) continue;
      total++;
      doll::markReached(doll::resolve(w.look));
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
  const std::vector<std::string> dead = doll::deadParts();
  art::setRigVersion(was);
  const double share = total ? 100.0 * covered / total : 0.0;
  std::printf("B6 doll coverage (%s, seeds %llu..%llu): %d of %d distinct looks covered (%.1f %%)", all ? "all" : "civil", (unsigned long long)a,
              (unsigned long long)b, covered, total, share);
  for (const auto& kv : bySrc) std::printf("; %c %d/%d", kv.first, kv.second.first, kv.second.second);
  std::printf("; %zu dead part(s)\n", dead.size());
  std::vector<std::pair<int, std::string>> top;
  for (const auto& kv : missing) top.push_back({kv.second, kv.first});
  std::sort(top.begin(), top.end(), [](const auto& x, const auto& y) { return x.first != y.first ? x.first > y.first : x.second < y.second; });
  for (size_t i = 0; i < top.size() && (g_report || i < 25); i++) std::printf("  missing in %5d looks: %s\n", top[i].first, top[i].second.c_str());
  if (top.size() > 25 && !g_report) std::printf("  ... %zu more (--report)\n", top.size() - 25);
  for (size_t i = 0; i < dead.size() && (g_report || i < 10); i++) std::printf("  dead part: %s\n", dead[i].c_str());
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
