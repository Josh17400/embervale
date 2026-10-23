// EMBERVALE tests: B11 wear, the look matrix with part coverage, the freeze manifest, the M7 B10 watch and the --frames
// strips (docs/P2B_MOTION_ROLLOUT.md 5.3, 5.5, 5.6, 5.8; area GATES-WEAR). test_doll.cpp (GATES-LIMBS) calls the 5.8
// functions from `rpg_test --doll --gates` and `--doll --motion`; `rpg_test --doll-wear` runs this file alone (below).
//
// B11 wear, per side frame (walk, run, drawn walk, drawn run) of a look; every image is the doll's own compositor on a
// probe palette (each slot x step its own colour; the arm regions U F C X, the hand H and the leg regions thigh / knee,
// shin, boot / sole on their own slots), pivot-relative, u forward (R u = x, L u = -x - 1), y up negative:
//   W1  no silent vanishing: a part that draws in the look's side idle (either facing) has a view (an empty one declares
//       intent) for every key its channel takes in every side walk, run, drawn idle, drawn walk and drawn run frame of
//       the look ("-" excepted); parts that never draw in the idle (stride panels, the drawn sword) are exempt
//   W2  sleeves on their arms: every sleeve / drape part on an arm channel lies inside its own arm (the same side) dilated
//       by 3 px and covers at least one of the arm's U / F / C pixels
//   W3  hangs trail: run frames, no garment hang, cloak or flowing neckwear pixel more than 1 px forward of the torso
//       layer's front edge in the same row
//   W4  the torso leans: run frames, the torso layer's top 2 rows shift by the clip's lean and its bottom 2 rows by the
//       clip's hip offset (+/-1) against the same facing's idle
//   W5  attached: every 8-connected visible piece of a hang, cloak, neckwear or held item touches the body (legs, torso,
//       arms, sleeves, head; 8-neighbour)
//   W6  in hand: a held part not keyed by the hand's own channel (the stick on the legs, the run staff on the arms) lies
//       within 1 px of a hand (H) pixel
//   W7  the face clear: no visible held, belt / sheath or cloak-front pixel over a face pixel (the face: the eye, brow and
//       mouth marks and the skin forward of the eye from the eye row down to the chin)
//   W8  the box: visible cloak, neckwear and hair-hang pixels stay within the body's own box (legs, torso, arms, head,
//       garment): <= 1 column past it, 0 rows
//   W9  hair behind: run frames, no hair-hang pixel below the chin forward of the neck anchor + 2
//   W10 belt clear: sheath-channel items cover no shown boot pixel and shown shin pixels in at most 1 column
//   T1  tips shown (long-garment looks): every foot within 3 rows of the ground shows >= 2 rows of shoe; in the flight
//       (no boot pixel on the ground row) both feet show >= 1 row
//   T2  tips registered: at each frame change inside one stance the planted shoe's contact lands within 1 px (check.py's
//       REG on the shoe pixels as shown)
// The matrix (5.5) is built from the pool's role vocabulary; part coverage: every part with a side view for a key the
// side walk / run / drawn frames name must draw in at least one judged frame. The freeze manifest hashes every finished
// frame outside the side walk / run / drawn frames and the walking postures' side frames.
//
//   rpg_test --doll-wear [--pool FILE] [--matrix FILTER | --looks FILE] [--report] [--frames DIR] [--selftest]
//                        [--freeze-write FILE | --freeze-check FILE] [--b10] [--list]
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "rpg/art.h"
#include "rpg/art/doll/doll.h"
#include "rpg/art/doll/doll_internal.h"
#include "tools/tests/tests.h"
#if __has_include("tools/tests/doll_motion.h")
#include "tools/tests/doll_motion.h"
#define EMB_DOLL_MOTION 1
#endif

std::string dollSliceLooksText();   // (test_doll_arms.cpp: the slice looks file, embedded)

namespace wear {
namespace {

using art::Clip;
using art::HumanLook;
using art::RigPart;
using art::RigSheet;
namespace doll = art::doll;
namespace P = art::doll::pool;

bool g_rep = false;
std::string g_ascii;   // --ascii TAG:GAIT:F:I (F R or L): prints that frame's images

std::string fmt(const char* f, ...) {
  char b[1024];
  va_list a;
  va_start(a, f);
  std::vsnprintf(b, sizeof b, f, a);
  va_end(a);
  return b;
}
uint32_t hexc(uint32_t rgb) { return 0xFF000000u | (rgb & 0xFF) << 16 | (rgb & 0xFF00) | (rgb >> 16 & 0xFF); }

// ---------------------------------------------------------------- looks ("tag who spec [gaits]", test_doll's format)
struct WLook { std::string tag; int who = 0; std::string spec; std::vector<std::string> gaits; std::string family; };
struct WFail { std::string check, where, why; };

HumanLook a3Hero() {
  HumanLook L;
  L.body = 0; L.hair = art::Hair::Short; L.hairVar = 2;
  L.neck = 1; L.weapon = 1; L.outfit = art::Outfit::Tunic;
  L.skin = hexc(0xE9A077); L.hairColor = hexc(0xAA4A2C); L.topColor = hexc(0x28728C); L.bottomColor = hexc(0x4F4B6B); L.tabardColor = hexc(0xC83E38);
  L.eyeColor = hexc(0x3A78B0);
  return L;
}
HumanLook a3Woman() {
  HumanLook L;
  L.body = 1; L.hair = art::Hair::Braids; L.hairVar = 0;
  L.outfit = art::Outfit::Dress;
  L.skin = hexc(0xEEAA86); L.hairColor = hexc(0xC79346); L.topColor = hexc(0x922F60); L.tabardColor = hexc(0x28728C);
  L.eyeColor = hexc(0x3F8A5A);
  return L;
}
struct Parsed { HumanLook L; bool lookPal = false, bare = false; std::string bad; };
Parsed parse(const WLook& w) {
  Parsed p;
  p.L = w.who ? a3Woman() : a3Hero();
  doll::applyLookSpec(p.L, w.spec, &p.lookPal, &p.bare, &p.bad);   // (P2c: the one list, doll.h)
  return p;
}
doll::DollRecipe recipeOf(const Parsed& p) {
  doll::DollRecipe r = doll::resolve(p.L);
  r.ramps = p.lookPal ? doll::rampsOf(p.L) : doll::a3Palette(p.L.body == 1 ? 1 : 0);
  if (p.bare) for (doll::Region g : {doll::Region::Thigh, doll::Region::Knee, doll::Region::Shin}) r.region[(int)g] = (uint8_t)doll::Slot::Skin;
  return r;
}
std::vector<std::string> splitc(const std::string& s, char c) {
  std::vector<std::string> v;
  size_t a = 0;
  while (a <= s.size()) {
    size_t b = s.find(c, a);
    if (b == std::string::npos) b = s.size();
    if (b > a) v.push_back(s.substr(a, b - a));
    a = b + 1;
  }
  return v;
}
std::vector<WLook> readLooksFile(const char* file) {
  std::vector<WLook> v;
  FILE* f = std::fopen(file, "rb");
  if (!f) { std::printf("FAIL: --looks %s: cannot read\n", file); return v; }
  char line[1024];
  while (std::fgets(line, sizeof line, f)) {
    char tag[128] = {}, who[32] = {}, spec[700] = {}, gaits[200] = {};
    if (line[0] == '#' || std::sscanf(line, "%127s %31s %699s %199s", tag, who, spec, gaits) < 3) continue;
    WLook l;
    l.tag = tag; l.who = !std::strcmp(who, "woman") ? 1 : 0; l.spec = !std::strcmp(spec, "-") ? "" : spec;
    l.gaits = gaits[0] ? splitc(gaits, ',') : std::vector<std::string>{"walk", "run"};
    v.push_back(l);
  }
  std::fclose(f);
  return v;
}

// ---------------------------------------------------------------- the gaits
struct GaitDef { const char* name; RigPart part; int variant; bool run; int posture; uint8_t pv; };
const GaitDef kGaits[] = {
    {"walk", RigPart::Loco, 0, false, -1, 0},  {"run", RigPart::Run, 0, true, -1, 0},
    {"dwalk", RigPart::Drawn, 1, false, -1, 0}, {"drun", RigPart::Drawn, 2, true, -1, 0},
    {"carry", RigPart::Loco, 0, false, (int)art::Posture::Carry, 0}, {"carry1", RigPart::Loco, 0, false, (int)art::Posture::Carry, 1},
    {"carry2", RigPart::Loco, 0, false, (int)art::Posture::Carry, 2}, {"play", RigPart::Loco, 0, true, (int)art::Posture::Play, 0},
    {"dance", RigPart::Loco, 0, false, (int)art::Posture::Dance, 0}};
constexpr int kNGaits = (int)(sizeof kGaits / sizeof kGaits[0]);
int gaitIx(const std::string& g) {
  for (int i = 0; i < kNGaits; i++) if (g == kGaits[i].name) return i;
  return -1;
}

// ---------------------------------------------------------------- the part categories
enum Kind : int {
  kFull, kNoHeld, kBody, kLegs, kArms, kArmsR, kArmsL, kSleeveR, kSleeveL, kTorso, kHead, kFace, kHang, kCloakB, kCloakF,
  kNeck, kNeckFlow, kHair, kFree, kArmsFree, kSheath, kKinds
};
const char* const kKindName[kKinds] = {"full", "noheld", "body", "legs", "arms", "armsR", "armsL", "sleeveR", "sleeveL", "torso",
                                       "head", "face", "hang", "cloakB", "cloakF", "neck", "neckflow", "hair", "free", "armsfree", "sheath"};
struct PartInfo {
  bool legs = false, arms = false, sleeve = false, held = false, free = false, sheath = false, cloakB = false, cloakF = false,
       neck = false, neckFlow = false, hair = false, hang = false, torso = false, head = false, face = false, fx = false;
  int side = 0;   // 1 R, 2 L
  std::string channel, name;
};
bool hasRole(const P::PartRec& p, doll::Role r) {
  for (const P::RoleRec& rr : p.roles) if (rr.cat == (uint8_t)r) return true;
  return false;
}
bool tagged(const P::Pool& Pl, const P::PartRec& p, const char* t) {
  for (uint16_t id : p.tags) if (Pl.s(id) == t) return true;
  return false;
}
PartInfo infoOf(int pi) {
  const P::Pool& Pl = P::get();
  PartInfo f;
  if (pi < 0 || pi >= (int)Pl.parts.size()) return f;
  const P::PartRec& p = Pl.parts[(size_t)pi];
  f.channel = Pl.s(p.channel); f.name = Pl.s(p.name); f.side = p.side;
  using R = doll::Role;
  using L = doll::Layer;
  const bool armCh = f.channel == "armR" || f.channel == "armL";
  if (hasRole(p, R::Legs)) f.legs = true;
  else if (hasRole(p, R::Arms)) f.arms = true;
  else if (f.channel == "sheath" || f.channel == "hilt" || hasRole(p, R::Sheath) || hasRole(p, R::Belt)) f.sheath = true;
  else if (hasRole(p, R::Held)) { f.held = true; f.free = f.channel != "held"; }
  else if (hasRole(p, R::Cloak)) { if (f.channel == "cloakfront") f.cloakF = true; else f.cloakB = true; }
  else if (hasRole(p, R::Neck)) { f.neck = true; f.neckFlow = f.channel == "neck" || f.channel == "hang"; }
  else if (hasRole(p, R::Hair) && (f.channel == "hair" || f.channel == "braid") && !tagged(Pl, p, "overlay")) f.hair = true;
  else if (hasRole(p, R::Fx)) f.fx = true;
  else if (armCh && (hasRole(p, R::Garment) || hasRole(p, R::Armour))) f.sleeve = true;
  else if ((p.layer == (uint8_t)L::HangBack || p.layer == (uint8_t)L::HangFront) && hasRole(p, R::Garment)) f.hang = true;
  else if (p.layer == (uint8_t)L::Torso && (hasRole(p, R::Garment) || hasRole(p, R::Armour) || hasRole(p, R::Overlay))) f.torso = true;
  if (hasRole(p, R::Head)) f.head = true;
  if (hasRole(p, R::Head) || hasRole(p, R::Nose) || hasRole(p, R::Brow) || hasRole(p, R::Expr) || hasRole(p, R::Marks) || hasRole(p, R::Paint)) f.face = true;
  return f;
}
bool inKind(const PartInfo& f, int k) {
  switch (k) {
    case kFull: return true;
    case kNoHeld: return !f.held;
    case kBody: return !(f.hang || f.cloakB || f.cloakF || f.neck || f.hair || f.held || f.sheath || f.fx);
    case kLegs: return f.legs;
    case kArms: return f.arms;
    case kArmsR: return f.arms && f.side == 1;
    case kArmsL: return f.arms && f.side == 2;
    case kSleeveR: return f.sleeve && f.side == 1;
    case kSleeveL: return f.sleeve && f.side == 2;
    case kTorso: return f.torso;
    case kHead: return f.head;
    case kFace: return f.face;
    case kHang: return f.hang;
    case kCloakB: return f.cloakB;
    case kCloakF: return f.cloakF;
    case kNeck: return f.neck;
    case kNeckFlow: return f.neckFlow;
    case kHair: return f.hair;
    case kFree: return f.free;
    case kArmsFree: return f.arms || f.free;
    case kSheath: return f.sheath;
  }
  return false;
}

// ---------------------------------------------------------------- the probe (every slot x step its own colour)
struct Probe { doll::DollRamps ramps; std::unordered_map<uint32_t, uint8_t> back; };
const Probe& probe() {
  static const Probe p = [] {
    Probe q;
    for (int s = 0; s < doll::SLOT_COUNT; s++)
      for (int k = 0; k < 5; k++) {
        const uint32_t c = 0xFF000000u | (uint32_t)(7 + s * 11) | (uint32_t)(23 + k * 41) << 8 | 0x5Au << 16;
        q.ramps.c[s][k] = c;
        q.back[c] = doll::code((doll::Slot)s, k);
      }
    return q;
  }();
  return p;
}
constexpr doll::Slot kSlotThigh = doll::Slot::Glow, kSlotShin = doll::Slot::Fx, kSlotBoot = doll::Slot::Device, kSlotArm = doll::Slot::Plume,
                     kSlotHand = doll::Slot::Field, kSlotSole = doll::Slot::Pattern;
doll::DollRecipe probed(doll::DollRecipe r) {
  r.ramps = probe().ramps;
  using G = doll::Region;
  r.region[(int)G::Thigh] = r.region[(int)G::Knee] = (uint8_t)kSlotThigh;
  r.region[(int)G::Shin] = (uint8_t)kSlotShin;
  r.region[(int)G::Boot] = (uint8_t)kSlotBoot;
  r.region[(int)G::Sole] = (uint8_t)kSlotSole;
  r.region[(int)G::Upper] = r.region[(int)G::Fore] = r.region[(int)G::Cuff] = r.region[(int)G::Flare] = (uint8_t)kSlotArm;
  r.region[(int)G::Hand] = (uint8_t)kSlotHand;
  return r;
}
inline bool isSlot(uint8_t c, doll::Slot s) { return c >= 2 && c != 255 && doll::codeSlot(c) == s; }

// a pivot-relative window of codes (0 clear)
struct Grid {
  static constexpr int W = 120, H = 100, CX = 60, CY = 84;
  std::vector<uint8_t> c;
  bool ok = false;
  Grid() : c((size_t)W * H, 0) {}
  uint8_t at(int X, int Y) const {
    const int x = X + CX, y = Y + CY;
    return x >= 0 && y >= 0 && x < W && y < H ? c[(size_t)y * W + x] : 0;
  }
  void set(int X, int Y, uint8_t v) {
    const int x = X + CX, y = Y + CY;
    if (x >= 0 && y >= 0 && x < W && y < H) { c[(size_t)y * W + x] = v; ok = true; }
  }
  bool any() const { for (uint8_t v : c) if (v) return true; return false; }
  template <class F> void each(F fn) const {
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) if (const uint8_t v = c[(size_t)y * W + x]) fn(x - CX, y - CY, v);
  }
};
Grid gridOf(const RigSheet& s, int fc, Clip clip, int variant, int i) {
  Grid g;
  const art::FrameRect* r = s.frame(fc, clip, variant, i);
  if (!r || r->w <= 0 || r->h <= 0) return g;
  g.ok = true;
  const auto& back = probe().back;
  for (int y = 0; y < r->h; y++)
    for (int x = 0; x < r->w; x++) {
      const uint32_t p = s.atlas.get(r->x + x, r->y + y);
      if (!(p >> 24)) continue;
      const auto it = back.find(p | 0xFF000000u);
      g.set(x - r->px, y - r->py, it == back.end() ? (uint8_t)255 : it->second);
    }
  return g;
}

// ---------------------------------------------------------------- one frame's images
struct FD {
  int facing = 2, gait = -1, i = 0;
  bool run = false;
  bool posture = false;   // a walking posture's side frame (B11 wear judges its load: W5 attached, W7 face clear)
  int lean = 0, hipX = 0, bob = 0, neckX = 0;
  int plant0 = INT32_MIN, plant1 = INT32_MIN;
  std::map<std::string, std::string> keys;
  Grid g[kKinds];
  bool has[kKinds] = {};
  int u(int X) const { return facing == 3 ? -X - 1 : X; }
};
// visible pixels of an alone image: the full figure shows its code there
inline bool vis(const FD& f, int k, int X, int Y) { const uint8_t c = f.g[k].at(X, Y); return c && f.g[kFull].at(X, Y) == c; }
inline bool heldVis(const FD& f, int X, int Y) { return f.has[kNoHeld] && f.g[kFull].at(X, Y) != f.g[kNoHeld].at(X, Y) && f.g[kFull].at(X, Y); }

const P::FrameRec* clipFrame(const doll::DollRecipe& r, RigPart part, int facing, Clip clip, int variant, int i) {
  for (const P::ClipRec* c : P::clipsOf(r.bodyKey, P::poolPart(part), r.family, -1, &r))
    if (c->clip == (uint8_t)clip && c->facing == facing && c->variant == variant && i < c->nFrames) return &P::get().frames[c->firstFrame + i];
  return nullptr;
}
int neckXOf(const doll::DollRecipe& r, int fc) {
  int x = 0;
  if (const P::BodyRec* b = P::bodyOf(r.bodyKey))
    for (const P::AnchorRec& a : b->anchors[fc & 3]) if (a.id == (uint8_t)doll::Anchor::Neck) x = a.x;
  return x;
}

struct Shot { doll::DollRecipe rec; std::vector<int> gaits; std::vector<FD> idle, frames; std::vector<PartInfo> info; bool tips = false; };

Shot shootLook(const doll::DollRecipe& full, const std::vector<int>& gaits) {
  Shot S;
  S.rec = full;
  S.gaits = gaits;
  std::vector<int> partsOf[kKinds];
  for (int p : full.parts) {
    PartInfo f = infoOf(p);
    if (doll::partHasRole(p, doll::Role::Legs, "tips")) S.tips = true;
    for (int k = 0; k < kKinds; k++) if (inKind(f, k)) partsOf[k].push_back(p);
    S.info.push_back(f);
  }
  bool need[kKinds];
  for (int k = 0; k < kKinds; k++) need[k] = !partsOf[k].empty();
  if (partsOf[kNoHeld].size() == full.parts.size()) need[kNoHeld] = false;   // nothing held
  if (partsOf[kFree].empty()) need[kArmsFree] = false;
  if (partsOf[kSleeveR].empty() && partsOf[kSleeveL].empty()) need[kArmsR] = need[kArmsL] = false;
  struct Need { RigPart part; std::vector<std::pair<int, int>> what; };   // (gait or -1 idle, variant)
  std::vector<RigPart> parts = {RigPart::Loco};
  for (int g : gaits) {
    if (kGaits[g].posture >= 0) continue;
    if (std::find(parts.begin(), parts.end(), kGaits[g].part) == parts.end()) parts.push_back(kGaits[g].part);
  }
  for (RigPart rp : parts)
    for (int fc : {2, 3}) {
      RigSheet sh[kKinds];
      for (int k = 0; k < kKinds; k++) {
        if (!need[k]) continue;
        doll::DollRecipe sub = full;
        sub.parts = partsOf[k];
        doll::ComposeOpts o;
        o.finish = false;
        o.onlyFacing = fc;
        sh[k] = doll::compose(probed(sub), rp, &o);
      }
      auto make = [&](int gait, Clip clip, int variant, int i) {
        FD f;
        f.facing = fc; f.gait = gait; f.i = i;
        f.run = gait >= 0 && kGaits[gait].run;
        for (int k = 0; k < kKinds; k++) if (need[k]) { f.g[k] = gridOf(sh[k], fc, clip, variant, i); f.has[k] = true; }
        f.neckX = neckXOf(full, fc);
        if (const P::FrameRec* fr = clipFrame(full, rp, fc, clip, variant, i)) {
          f.lean = fr->lean; f.hipX = fr->hipX; f.bob = fr->bob;
          f.neckX += fr->lean;
          if (fr->plant0 != P::kNoPlant) f.plant0 = (int)std::lround(fr->plant0 / 16.0);
          if (fr->plant1 != P::kNoPlant) f.plant1 = (int)std::lround(fr->plant1 / 16.0);
          const P::Pool& Pl = P::get();
          for (const P::KeyRec& kr : fr->keys) f.keys[Pl.s(kr.channel)] = Pl.s(kr.key);
        }
        return f;
      };
      if (rp == RigPart::Loco) S.idle.push_back(make(-1, Clip::Idle, 0, 0));
      for (int g : gaits) {
        const GaitDef& G = kGaits[g];
        if (G.posture >= 0 || G.part != rp) continue;
        const RigSheet& ref = sh[kFull];
        const int n = ref.count(fc, Clip::Walk, G.variant);
        for (int i = 0; i < n; i++) S.frames.push_back(make(g, Clip::Walk, G.variant, i));
      }
    }
  return S;
}

// ---------------------------------------------------------------- the walking postures (gates-wear fix round 1)
// 5.5 M6: Carry 0-2, Play, Dance (and the child's Play and Carry). B11 limbs judges their legs (test_doll_motion.cpp); B11
// wear judges what they carry: the load attached to the body (W5) and clear of the face (W7). doll::composePosture finishes
// its frames (the selout lights outline step 0 -> 1): neither check reads a lit slot's step. It also adds the posture's prop
// (withPosture) to whatever it composes: a kind's sheet hides the props it does not hold (their bodyMask cleared for the
// compose, then restored).
bool condOk(const doll::DollRecipe& r, const std::string& cond) {
  size_t a = 0;
  while (a <= cond.size()) {
    size_t b = cond.find(',', a);
    if (b == std::string::npos) b = cond.size();
    std::string t = cond.substr(a, b - a);
    const bool neg = !t.empty() && t[0] == '!';
    if (neg) t = t.substr(1);
    if (!t.empty() && r.has(t.c_str()) == neg) return false;
    a = b + 1;
  }
  return true;
}
// does part pi paint in this frame (the compositor's key and view choice; an empty cut is no frame in a trimmed sheet)
bool drawsIn(const doll::DollRecipe& r, int pi, int fc, const P::FrameRec& f) {
  const P::Pool& Pl = P::get();
  const P::PartRec& p = Pl.parts[(size_t)pi];
  uint16_t key = Pl.ids.rest;
  if (p.channel != Pl.ids.rest) {
    for (const P::KeyRec& k : f.keys) if (k.channel == p.channel) key = k.key;
    if (key == Pl.ids.dash) return false;
  }
  if (key == 0xFFFF) return false;
  const P::ViewRec* v = P::viewOf(p, fc, key, 0);
  for (uint32_t wi : p.whenViews) {
    const P::ViewRec& w = Pl.views[wi];
    if ((w.flags & 1) && w.facing == fc && w.key == key && condOk(r, Pl.s(w.when))) { v = &w; break; }
  }
  if (!v) return false;
  auto paints = [](const P::ViewRec& vv) { for (uint8_t c : vv.cells) if (c >= 2) return true; return false; };
  if (paints(*v)) return true;
  for (uint32_t wi : p.whenViews) {
    const P::ViewRec& w = Pl.views[wi];
    if (!(w.flags & 1) && w.facing == fc && w.key == key && condOk(r, Pl.s(w.when)) && paints(w)) return true;
  }
  return false;
}
struct HideProps {
  std::vector<std::pair<int, uint8_t>> saved;
  HideProps(const std::vector<int>& props, const std::vector<int>& keep) {
    P::Pool& Pl = const_cast<P::Pool&>(P::get());   // the pool's storage is a mutable static (doll_pool.cpp slot())
    for (int pi : props)
      if (std::find(keep.begin(), keep.end(), pi) == keep.end()) { saved.push_back({pi, Pl.parts[(size_t)pi].bodyMask}); Pl.parts[(size_t)pi].bodyMask = 0; }
  }
  ~HideProps() { P::Pool& Pl = const_cast<P::Pool&>(P::get()); for (auto& [pi, m] : saved) Pl.parts[(size_t)pi].bodyMask = m; }
};
void shootPostures(Shot& S, const doll::DollRecipe& full, const std::vector<int>& gaits) {
  const P::Pool& Pl = P::get();
  for (int g : gaits) {
    const GaitDef& G = kGaits[g];
    if (G.posture < 0) continue;
    const art::Posture pz = (art::Posture)G.posture;
    const doll::DollRecipe rp = doll::withPosture(full, pz, G.pv);
    std::vector<int> props;
    if (const char* prop = doll::postureProp(pz, G.pv)) props = Pl.partsWithRole(doll::Role::Held, prop);
    std::vector<int> partsOf[kKinds];
    for (int p : rp.parts) {
      const PartInfo f = infoOf(p);
      for (int k = 0; k < kKinds; k++) if (inKind(f, k)) partsOf[k].push_back(p);
    }
    bool need[kKinds];
    for (int k = 0; k < kKinds; k++) need[k] = !partsOf[k].empty();
    if (partsOf[kNoHeld].size() == rp.parts.size()) need[kNoHeld] = false;   // nothing held
    if (partsOf[kFree].empty()) need[kArmsFree] = false;
    if (partsOf[kSleeveR].empty() && partsOf[kSleeveL].empty()) need[kArmsR] = need[kArmsL] = false;
    RigSheet sh[kKinds];
    for (int k = 0; k < kKinds; k++) {
      if (!need[k]) continue;
      doll::DollRecipe sub = rp;
      sub.parts = partsOf[k];
      const HideProps hide(props, sub.parts);
      sh[k] = doll::composePosture(probed(sub), pz, G.pv);
    }
    const std::vector<const P::ClipRec*> clips = P::postureClips(rp.bodyKey, rp.family, G.posture, G.pv);
    for (int fc : {2, 3}) {
      int i = 0, idx[kKinds] = {};
      for (const P::ClipRec* c : clips) {
        if (c->facing != fc) continue;
        for (int n = 0; n < (int)c->nFrames; n++, i++) {
          const P::FrameRec& fr = Pl.frames[c->firstFrame + n];
          FD f;
          f.facing = fc; f.gait = g; f.i = i; f.run = G.run; f.posture = true;
          for (int k = 0; k < kKinds; k++) {
            if (!need[k]) continue;
            bool d = false;
            for (int p : partsOf[k]) if (drawsIn(rp, p, fc, fr)) { d = true; break; }
            f.has[k] = true;
            if (d) f.g[k] = gridOf(sh[k], fc, Clip::Idle, 0, idx[k]++);
          }
          f.neckX = neckXOf(full, fc) + fr.lean;
          f.lean = fr.lean; f.hipX = fr.hipX; f.bob = fr.bob;
          for (const P::KeyRec& kr : fr.keys) f.keys[Pl.s(kr.channel)] = Pl.s(kr.key);
          S.frames.push_back(std::move(f));
        }
      }
    }
  }
}

// ---------------------------------------------------------------- per-frame checks (W2-W10, T1)
using Report = std::function<void(const char* check, bool bad, const std::string& why)>;

struct Box { int x0 = 1 << 20, y0 = 1 << 20, x1 = -(1 << 20), y1 = -(1 << 20); bool ok() const { return x1 >= x0; }
             void add(int x, int y) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); } };

// the face pixels: the eye / brow / mouth marks, and the skin forward of the eye's back from the eye row to the chin
void facePixels(const FD& f, std::set<std::pair<int, int>>& face) {
  if (!f.has[kFace]) return;
  const Grid& g = f.g[kFace];
  int eyeY = 1 << 20, eyeU = 1 << 20, chin = -(1 << 20), mouth = -(1 << 20);
  g.each([&](int X, int Y, uint8_t c) {
    if (isSlot(c, doll::Slot::Eyes) || (isSlot(c, doll::Slot::Face) && doll::codeStep(c) <= 1)) { eyeY = std::min(eyeY, Y); eyeU = std::min(eyeU, f.u(X)); }
    if (isSlot(c, doll::Slot::Face) && doll::codeStep(c) >= 2 && doll::codeStep(c) <= 3) mouth = std::max(mouth, Y);
  });
  if (f.has[kHead]) f.g[kHead].each([&](int, int Y, uint8_t) { chin = std::max(chin, Y); });
  // (calibrated on A3's approved drawn guard, whose blade crosses the hero's jaw beside the mouth facing left: the face's skin
  // runs from the eye row - 1 to the row above the mouth; the mouth's own pixels stay face)
  if (eyeY < (1 << 20)) chin = std::min(chin, mouth > eyeY ? mouth - 1 : eyeY + 2);
  // (calibrated: the profile's outermost pixel of a row is the face's edge, not the face: the drawn sword's approved guard
  // passes in front of the chin touching it)
  std::map<int, int> front;
  g.each([&](int X, int Y, uint8_t) { auto it = front.find(Y); if (it == front.end()) front[Y] = f.u(X); else it->second = std::max(it->second, f.u(X)); });
  g.each([&](int X, int Y, uint8_t c) {
    // (the mouth and lip marks are the face's lower edge in profile: A3's hero guard L crosses the mouth's corner pixel)
    if (isSlot(c, doll::Slot::Eyes) || (isSlot(c, doll::Slot::Face) && doll::codeStep(c) <= 1 && f.u(X) < front[Y])) { face.insert({X, Y}); return; }
    if (isSlot(c, doll::Slot::Face)) return;
    if (isSlot(c, doll::Slot::Skin) && eyeY < (1 << 20) && Y >= eyeY - 1 && Y <= chin && f.u(X) >= eyeU - 1 && f.u(X) < front[Y]) face.insert({X, Y});
  });
}

void frameChecks(const FD& f, const FD& idle, const Report& rep) {
  const bool run = f.run;
  // W2 sleeves on their arms
  for (int s = 0; s < 2; s++) {
    const int sk = s ? kSleeveL : kSleeveR, ak = s ? kArmsL : kArmsR;
    if (!f.has[sk] || !f.g[sk].any()) continue;
    const Grid &sl = f.g[sk], &ar = f.g[ak];
    int out = 0, on = 0, worst = 0, armU = 0, apart = 0;
    std::string where;
    ar.each([&](int, int, uint8_t a) { armU += isSlot(a, kSlotArm); });   // (an arm drawn as a hand only under a sleeve: no overlap asked)
    sl.each([&](int X, int Y, uint8_t) {
      const uint8_t a = ar.at(X, Y);
      if (a && isSlot(a, kSlotArm)) on++;
      int d = 99;
      for (int dy = -3; dy <= 3 && d > 0; dy++)
        for (int dx = -3; dx <= 3; dx++) if (ar.at(X + dx, Y + dy)) d = std::min(d, std::max(std::abs(dx), std::abs(dy)));
      if (d > 3) { out++; if (where.empty()) where = fmt("(%d,%d)", f.u(X), Y); }
      if (d > 1) apart++;
      worst = std::max(worst, d);
    });
    // (calibrated on MAN-WEAR's robe and gown: a sleeve view that is only its trim hanging off the arm, every pixel touching
    // the arm, passes without covering it)
    const bool bad = out > 0 || (armU > 0 && on == 0 && apart > 0);
    rep("W2", bad, bad ? fmt("%s sleeve: %d px off its arm (first at u,y %s), %d on its U/F/C%s", s ? "L" : "R", out, where.c_str(), on,
                             ar.any() ? "" : "; the arm draws nothing") : std::string());
  }
  // W3 hangs trail (run)
  if (run && f.has[kTorso]) {
    std::map<int, int> front;
    f.g[kTorso].each([&](int X, int Y, uint8_t) { auto it = front.find(Y); const int u = f.u(X); if (it == front.end()) front[Y] = u; else it->second = std::max(it->second, u); });
    int worst = -99, wy = 0;
    std::string what;
    const int top = front.empty() ? 0 : front.begin()->first;
    for (int k : {kHang, kCloakB, kNeckFlow}) {
      if (!f.has[k]) continue;
      f.g[k].each([&](int X, int Y, uint8_t) {
        auto it = front.find(Y);
        if (it == front.end() || (k == kCloakB && Y < top + 3)) return;
        const int d = f.u(X) - it->second;
        if (d > worst) { worst = d; wy = Y; what = kKindName[k]; }
      });
    }
    if (worst > -99) rep("W3", worst > 1, worst > 1 ? fmt("%s %d px forward of the torso's front at y %d", what.c_str(), worst, wy) : std::string());
  }
  // W4 the torso leans (run)
  if (run && f.has[kTorso] && idle.has[kTorso] && f.g[kTorso].any() && idle.g[kTorso].any()) {
    // each frame's own top 2 and bottom 2 torso rows (the hem as drawn; like rows were tried and read the woman's re-cut
    // runl hems, drawn to ride the hip at their own hem, as short)
    auto mids = [](const FD& d, double& top, double& hem) {
      std::map<int, std::pair<int, int>> rows;
      d.g[kTorso].each([&](int X, int Y, uint8_t) {
        const int u = d.u(X);
        auto it = rows.find(Y);
        if (it == rows.end()) rows[Y] = {u, u}; else { it->second.first = std::min(it->second.first, u); it->second.second = std::max(it->second.second, u); }
      });
      auto mid = [&](std::map<int, std::pair<int, int>>::const_iterator it) { return (it->second.first + it->second.second) / 2.0; };
      auto a = rows.cbegin();
      top = mid(a); if (rows.size() > 1) top = (top + mid(std::next(a))) / 2;
      auto z = std::prev(rows.cend());
      hem = mid(z); if (rows.size() > 1) hem = (hem + mid(std::prev(z))) / 2;
    };
    double t1, h1, t0, h0;
    mids(f, t1, h1);
    mids(idle, t0, h0);
    const int sgn = f.facing == 3 ? -1 : 1;
    const double wantTop = f.lean * sgn, wantHem = f.hipX * sgn, dt = t1 - t0, dh = h1 - h0;
    const bool bad = std::fabs(dt - wantTop) > 1.01 || std::fabs(dh - wantHem) > 1.01;
    rep("W4", bad, bad ? fmt("torso top shifts %+.1f (lean %+.0f), hem %+.1f (hip %+.0f)", dt, wantTop, dh, wantHem) : std::string());
  }
  // W5 attached
  {
    const Grid& body = f.g[kBody];
    auto touches = [&](int X, int Y) {
      for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if (body.at(X + dx, Y + dy)) return true;
      return false;
    };
    std::string why;
    int loose = 0;
    for (int k : {kHang, kCloakB, kCloakF, kNeck, kNoHeld}) {
      if (!f.has[k]) continue;
      std::set<std::pair<int, int>> px;
      if (k == kNoHeld) {
        f.g[kFull].each([&](int X, int Y, uint8_t) { if (heldVis(f, X, Y)) px.insert({X, Y}); });
        if (f.has[kArmsFree]) f.g[kArmsFree].each([&](int X, int Y, uint8_t c) { if (c != f.g[kArms].at(X, Y)) px.insert({X, Y}); });
      } else f.g[k].each([&](int X, int Y, uint8_t) { px.insert({X, Y}); });
      std::set<std::pair<int, int>> seen;
      for (const auto& s0 : px) {
        if (seen.count(s0)) continue;
        std::vector<std::pair<int, int>> st{s0}, comp;
        seen.insert(s0);
        while (!st.empty()) {
          auto q = st.back(); st.pop_back(); comp.push_back(q);
          for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
              std::pair<int, int> n{q.first + dx, q.second + dy};
              if (px.count(n) && !seen.count(n)) { seen.insert(n); st.push_back(n); }
            }
        }
        bool t = false;
        for (const auto& q : comp) if (touches(q.first, q.second)) { t = true; break; }
        if (!t) { loose++; if (why.empty()) why = fmt("%s piece of %d px at u,y (%d,%d) touches no body pixel", k == kNoHeld ? "held" : kKindName[k], (int)comp.size(), f.u(comp[0].first), comp[0].second); }
      }
    }
    if (f.has[kHang] || f.has[kCloakB] || f.has[kCloakF] || f.has[kNeck] || f.has[kNoHeld]) rep("W5", loose > 0, why);
  }
  // W6 in hand
  if (f.has[kArmsFree] && f.has[kArms]) {
    std::vector<std::pair<int, int>> hand, held;
    f.g[kArms].each([&](int X, int Y, uint8_t c) { if (isSlot(c, kSlotHand)) hand.push_back({X, Y}); });
    f.g[kArmsFree].each([&](int X, int Y, uint8_t c) { if (c != f.g[kArms].at(X, Y)) held.push_back({X, Y}); });
    if (!held.empty()) {
      int d = 99;
      for (const auto& h : held) for (const auto& a : hand) d = std::min(d, std::max(std::abs(h.first - a.first), std::abs(h.second - a.second)));
      rep("W6", d > 1, d > 1 ? (hand.empty() ? std::string("no hand pixel in the frame") : fmt("the held item is %d px from the nearest hand pixel", d)) : std::string());
    }
  }
  // W7 the face clear
  if (f.has[kFace] && (f.has[kNoHeld] || f.has[kArmsFree] || f.has[kSheath] || f.has[kCloakF])) {
    std::set<std::pair<int, int>> face;
    facePixels(f, face);
    int n = 0;
    std::string why;
    for (const auto& p : face) {
      const int X = p.first, Y = p.second;
      const bool through = f.has[kArmsFree] && f.g[kArmsFree].at(X, Y) && f.g[kArmsFree].at(X, Y) != f.g[kArms].at(X, Y);
      const char* what = heldVis(f, X, Y) ? "held" : through ? "held (through the head)" : (f.has[kSheath] && vis(f, kSheath, X, Y)) ? "sheath" : (f.has[kCloakF] && vis(f, kCloakF, X, Y)) ? "cloak front" : nullptr;
      if (what) { n++; if (why.empty()) why = fmt("%s over the face at u,y (%d,%d)", what, f.u(X), Y); }
    }
    rep("W7", n > 0, n ? why + fmt(" (%d px)", n) : std::string());
  }
  // W8 the box
  if (f.has[kCloakB] || f.has[kCloakF] || f.has[kNeck] || f.has[kHair]) {
    // the cloak: <= 1 column, 0 rows past the body's box (the bar). Neckwear and hair hangs (calibrated on the approved art:
    // the A3 scarf's flow trails 4 columns behind a bare head in the walk, a ponytail swings a column past its idle):
    // <= max(4, the same facing idle's reach + 1) columns, and no row past the idle's
    auto reach = [](const FD& d, bool cloak, int& col, int& row, std::string& what) {
      Box b;
      col = row = 0;
      d.g[kBody].each([&](int X, int Y, uint8_t) { b.add(X, Y); });
      if (d.has[kHang]) d.g[kHang].each([&](int X, int Y, uint8_t) { if (vis(d, kHang, X, Y)) b.add(X, Y); });
      for (int k : {kCloakB, kCloakF, kNeck, kHair}) {
        if (!d.has[k] || !b.ok() || cloak != (k == kCloakB || k == kCloakF)) continue;
        d.g[k].each([&](int X, int Y, uint8_t) {
          if (!vis(d, k, X, Y)) return;
          const int c = X < b.x0 ? b.x0 - X : X > b.x1 ? X - b.x1 : 0, r = Y < b.y0 ? b.y0 - Y : Y > b.y1 ? Y - b.y1 : 0;
          if (c > col || r > row) what = kKindName[k];
          col = std::max(col, c); row = std::max(row, r);
        });
      }
    };
    int cc, cr, nc, nr, ic, ir;
    std::string cw, nw, iw;
    reach(f, true, cc, cr, cw);
    reach(f, false, nc, nr, nw);
    reach(idle, false, ic, ir, iw);
    const int nm = std::max(4, ic + 1);
    const bool badC = cc > 1 || cr > 0, badN = nc > nm || nr > ir;
    std::string why;
    if (badC) why = fmt("%s %d column(s), %d row(s) past the body's box (1, 0)", cw.c_str(), cc, cr);
    if (badN) why += (why.empty() ? "" : "; ") + fmt("%s %d column(s), %d row(s) past the body's box (%d, %d)", nw.c_str(), nc, nr, nm, ir);
    rep("W8", badC || badN, why);
  }
  // W9 hair behind (run)
  if (run && f.has[kHair] && f.has[kHead]) {
    int chin = -(1 << 20);
    f.g[kHead].each([&](int, int Y, uint8_t) { chin = std::max(chin, Y); });
    const int nu = f.u(f.neckX);
    int worst = -99, wy = 0;
    f.g[kHair].each([&](int X, int Y, uint8_t) { if (Y > chin && f.u(X) - nu > worst) { worst = f.u(X) - nu; wy = Y; } });
    if (worst > -99) rep("W9", worst > 2, worst > 2 ? fmt("hair %d px forward of the neck anchor at y %d (below the chin, y %d)", worst, wy, chin) : std::string());
  }
  // W10 belt clear
  if (f.has[kSheath] && f.has[kLegs]) {
    int boot = 0;
    std::set<int> shinCols;
    f.g[kSheath].each([&](int X, int Y, uint8_t) {
      if (!vis(f, kSheath, X, Y)) return;
      const uint8_t l = f.g[kLegs].at(X, Y);
      if (!l) return;
      const bool shown = f.g[kBody].at(X, Y) == l && !(f.has[kHang] && f.g[kHang].at(X, Y));
      if (!shown) return;
      if (isSlot(l, kSlotBoot) || isSlot(l, kSlotSole)) boot++;
      else if (isSlot(l, kSlotShin)) shinCols.insert(X);
    });
    const bool bad = boot > 0 || shinCols.size() > 1;
    rep("W10", bad, bad ? fmt("the belt item covers %d boot px and shin in %d columns", boot, (int)shinCols.size()) : std::string());
  }
}

// T1 tips shown (one frame of a long-garment look)
void tipsShown(const FD& f, const Report& rep) {
  if (!f.has[kLegs]) return;
  std::set<std::pair<int, int>> boot;
  f.g[kLegs].each([&](int X, int Y, uint8_t c) { if (isSlot(c, kSlotBoot) || isSlot(c, kSlotSole)) boot.insert({X, Y}); });
  bool flight = f.run;
  for (const auto& p : boot) if (p.second == 0) flight = false;
  std::set<std::pair<int, int>> seen;
  std::string why;
  int feet = 0;
  for (const auto& s0 : boot) {
    if (seen.count(s0)) continue;
    std::vector<std::pair<int, int>> st{s0}, comp;
    seen.insert(s0);
    while (!st.empty()) {
      auto q = st.back(); st.pop_back(); comp.push_back(q);
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
          std::pair<int, int> n{q.first + dx, q.second + dy};
          if (boot.count(n) && !seen.count(n)) { seen.insert(n); st.push_back(n); }
        }
    }
    int low = -(1 << 20);
    std::set<int> rows, bootRows;
    for (const auto& q : comp) {
      low = std::max(low, q.second);
      if (!vis(f, kLegs, q.first, q.second)) continue;
      rows.insert(q.second);
      if (isSlot(f.g[kLegs].at(q.first, q.second), kSlotBoot)) bootRows.insert(q.second);
    }
    feet++;
    const int up = -low, want = flight ? 1 : (up <= 3 ? 2 : 0);
    // (the flight: a row of the boot itself, not the sole strip alone, must show under the hem)
    const int shown = flight ? (int)bootRows.size() : (int)rows.size();
    if (shown < want && why.empty())
      why = fmt("a foot %d row(s) up shows %d row(s) of %s (%d)%s", up, shown, flight ? "boot" : "shoe", want, flight ? " in the flight" : "");
  }
  rep("T1", !why.empty(), why);
}

// T2 tips registered: check.py's contact() and reg_check() on the shoe pixels
struct Contact { int heel, toe; bool heelDown, toeDown; };
bool contactOf(const FD& f, int plant, int sole, Contact& out) {
  std::set<std::pair<int, int>> bp;   // (u, y) boot pixels shown
  f.g[kLegs].each([&](int X, int Y, uint8_t c) { if ((isSlot(c, kSlotBoot) || isSlot(c, kSlotSole)) && vis(f, kLegs, X, Y)) bp.insert({f.u(X), Y}); });
  std::vector<int> cells;
  for (const auto& p : bp) if (p.second == 0) cells.push_back(p.first);
  if (cells.empty() || plant == INT32_MIN) return false;
  std::sort(cells.begin(), cells.end());
  std::vector<std::vector<int>> runs{{cells[0]}};
  for (size_t k = 1; k < cells.size(); k++) { if (cells[k] != runs.back().back() + 1) runs.push_back({}); runs.back().push_back(cells[k]); }
  const std::vector<int>* best = nullptr;
  int bd = 1 << 20;
  for (const auto& r : runs) { int d = 1 << 20; for (int u : r) d = std::min(d, std::abs(u - plant)); if (d < bd) { bd = d; best = &r; } }
  const std::vector<int>& r = *best;
  if ((int)r.size() >= sole - 1) { out = {r.front(), r.back(), true, true}; return true; }
  std::set<std::pair<int, int>> comp;
  std::vector<std::pair<int, int>> st;
  for (int u : r) st.push_back({u, 0});
  while (!st.empty()) {
    auto q = st.back(); st.pop_back();
    if (comp.count(q)) continue;
    comp.insert(q);
    for (auto n : {std::make_pair(q.first + 1, q.second), std::make_pair(q.first - 1, q.second), std::make_pair(q.first, q.second - 1), std::make_pair(q.first, q.second + 1)})
      if (bp.count(n) && !comp.count(n)) st.push_back(n);
  }
  // (integration 2026-10-23) only what this boot can reach: a shoe is `sole` long, so nothing further than sole - n columns
  // past either end of its n-pixel contact is its own (the child's lifted trailing shoe touching the planted heel's upper
  // under a long skirt read as this shoe's back half: T2 called a heel strike toe-down)
  const int reach = sole - (int)r.size();
  int ahead = 0, behind = 0;
  for (const auto& q : comp) {
    if (q.first > r.back() && q.first <= r.back() + reach) ahead++;
    if (q.first < r.front() && q.first >= r.front() - reach) behind++;
  }
  out = {r.front(), r.back(), ahead >= behind, behind >= ahead};
  return true;
}
void tipsRegistered(const std::vector<const FD*>& fr, int sole, const std::function<void(int i, bool bad, const std::string&)>& rep) {
  const size_t n = fr.size();
  for (size_t k = 0; k < n; k++) {
    const FD &a = *fr[k], &b = *fr[(k + 1) % n];
    if (a.plant0 == INT32_MIN || a.plant1 == INT32_MIN || b.plant0 == INT32_MIN || a.plant1 != b.plant0) continue;
    Contact ca, cb;
    if (!contactOf(a, a.plant0, sole, ca) || !contactOf(b, b.plant0, sole, cb)) continue;
    const int adv = a.plant0 - a.plant1;
    int pa, pb;
    const char* what;
    if (ca.heelDown && cb.heelDown) { pa = ca.heel; pb = cb.heel; what = "heel"; }
    else if (ca.toeDown && cb.toeDown) { pa = ca.toe; pb = cb.toe; what = "toe"; }
    else { rep(a.i, true, fmt("%d>%d the planted shoe changes ends", a.i, b.i)); continue; }
    const int slide = adv + pb - pa;
    rep(a.i, std::abs(slide) > 1, fmt("%d>%d %s %+d (advance %d, %d > %d)", a.i, b.i, what, slide, adv, pa, pb));
  }
}

// ---------------------------------------------------------------- W1 no silent vanishing (the pool, no images)
// the side frames a look's gaits name (sheathed walk and run; drawn idle, walk and run), facings R and L
struct KeyFrame { int facing; std::string gait; int i; const P::FrameRec* fr; };
std::vector<KeyFrame> sideFrames(const doll::DollRecipe& r, const std::vector<int>& gaits, bool idle) {
  std::vector<KeyFrame> v;
  bool drawn = false;
  for (int g : gaits) drawn = drawn || kGaits[g].part == RigPart::Drawn;
  for (int fc : {2, 3}) {
    auto add = [&](RigPart part, Clip clip, int variant, const char* name) {
      for (const P::ClipRec* c : P::clipsOf(r.bodyKey, P::poolPart(part), r.family, -1, &r))
        if (c->clip == (uint8_t)clip && c->facing == fc && c->variant == variant) {
          for (int i = 0; i < c->nFrames; i++) v.push_back({fc, name, i, &P::get().frames[c->firstFrame + i]});
          break;
        }
    };
    if (idle) { add(RigPart::Loco, Clip::Idle, 0, "idle"); continue; }
    for (int g : gaits) if (kGaits[g].posture < 0) add(kGaits[g].part, Clip::Walk, kGaits[g].variant, kGaits[g].name);
    if (drawn) add(RigPart::Drawn, Clip::Idle, 1, "didle");
  }
  return v;
}
uint16_t keyFor(const P::PartRec& p, const P::FrameRec& f) {
  const P::Ids& I = P::get().ids;
  if (p.channel == I.rest) return I.rest;
  uint16_t key = I.rest;
  for (const P::KeyRec& k : f.keys) if (k.channel == p.channel) key = k.key;
  return key;
}
bool drawsView(const P::ViewRec* v) {
  if (!v) return false;
  for (uint8_t c : v->cells) if (c >= 2) return true;
  return false;
}
using Hide = std::function<bool(int part, int facing, uint16_t key)>;   // the selftest's deleted views
void w1Look(const doll::DollRecipe& r, const std::vector<int>& gaits, const Report& rep, const Hide* hide = nullptr) {
  const P::Pool& Pl = P::get();
  const P::Ids& I = Pl.ids;
  const std::vector<KeyFrame> idle = sideFrames(r, gaits, true), moving = sideFrames(r, gaits, false);
  for (int pi : r.parts) {
    const P::PartRec& p = Pl.parts[(size_t)pi];
    if (p.channel == I.rest) continue;
    bool inIdle = false;
    for (const KeyFrame& k : idle) {
      if (k.i != 0) continue;   // the standing frame (the blink and breath frames are overlays)
      const uint16_t key = keyFor(p, *k.fr);
      if (key != I.dash && drawsView(P::viewOf(p, k.facing, key))) inIdle = true;
    }
    if (!inIdle) continue;
    std::set<std::string> told;
    for (const KeyFrame& k : moving) {
      const uint16_t key = keyFor(p, *k.fr);
      if (key == I.dash || (P::viewOf(p, k.facing, key) && !(hide && (*hide)(pi, k.facing, key)))) continue;
      // (P2c, INTEGRATE i1) an action arm (arms/adult/action: the spear's two fists, the bow arms) serves the key in the
      // loco arm's place on the same channel: the channel is drawn, so the loco arm's silence is not a hole
      // (a garment riding the arm's channel, a sleeve, is never excused: only an arm by an action arm)
      bool sibling = false;
      if (Pl.s(p.name).rfind("arms/", 0) == 0)
        for (int qi : r.parts)
          if (qi != pi && Pl.parts[(size_t)qi].channel == p.channel && Pl.s(Pl.parts[(size_t)qi].name).rfind("arms/adult/action/", 0) == 0 &&
              drawsView(P::viewOf(Pl.parts[(size_t)qi], k.facing, key)) && !(hide && (*hide)(qi, k.facing, key))) sibling = true;
      if (sibling) continue;
      // (P2c, INTEGRATE i1) `tags anyfacing` (the weapon library): D's view serves every facing, as the compositor draws it
      if (I.anyfacing != 0xFFFF && std::find(p.tags.begin(), p.tags.end(), I.anyfacing) != p.tags.end() && P::viewOf(p, 0, key) &&
          !(hide && (*hide)(pi, 0, key))) continue;
      const std::string w = fmt("%s %c key %s", Pl.s(p.name).c_str(), "DURL"[k.facing], Pl.s(key).c_str());
      if (told.insert(w).second) rep("W1", true, w + fmt(": no view (%s %d; the part draws nothing)", k.gait.c_str(), k.i));
    }
    if (told.empty()) rep("W1", false, std::string());
  }
}

// --ascii: each image of a frame as letters (slot initial; '.' clear), pivot column marked
void dumpAscii(const FD& f) {
  const char* sl = "shefiCcalnLmMdkpPFDgx";
  for (int k = 0; k < kKinds; k++) {
    if (!f.has[k] || !f.g[k].any()) continue;
    Box b;
    f.g[k].each([&](int X, int Y, uint8_t) { b.add(X, Y); });
    std::printf("  [%s] x %d..%d y %d..%d\n", kKindName[k], b.x0, b.x1, b.y0, b.y1);
    for (int Y = b.y0; Y <= b.y1; Y++) {
      std::string row;
      for (int X = b.x0; X <= b.x1; X++) { const uint8_t c = f.g[k].at(X, Y); row += !c ? (X == 0 ? '|' : '.') : c == 255 ? '?' : sl[std::min<int>((int)doll::codeSlot(c), 20)]; }
      std::printf("  %4d %s\n", Y, row.c_str());
    }
  }
}

// ---------------------------------------------------------------- one look, every wear check
struct LookResult { int frames = 0, checks = 0, fails = 0; };
LookResult wearLook(const std::string& tag, const doll::DollRecipe& r, const std::vector<int>& gaitsIn, std::vector<WFail>& out) {
  LookResult res;
  std::vector<int> gaits, postures;
  // the walking postures: their legs are B11 limbs' (GATES-LIMBS); B11 wear judges their load (W5, W7)
  for (int g : gaitsIn) (kGaits[g].posture < 0 ? gaits : postures).push_back(g);
  auto add = [&](const char* check, const std::string& where, bool bad, const std::string& why) {
    res.checks++;
    if (bad) { res.fails++; out.push_back({check, where, why}); }
  };
  w1Look(r, gaits, [&](const char* c, bool bad, const std::string& why) { add(c, tag, bad, why); });
  if (gaits.empty() && postures.empty()) return res;
  Shot S = shootLook(r, gaits);
  shootPostures(S, r, postures);
  for (const FD& f : S.frames) {
    res.frames++;
    if (!g_ascii.empty() && g_ascii == fmt("%s:%s:%c:%d", tag.c_str(), kGaits[f.gait].name, "DURL"[f.facing], f.i)) dumpAscii(f);
    const FD& idle = S.idle[f.facing == 3 && S.idle.size() > 1 ? 1 : 0];
    const std::string where = fmt("%s %s %c%d", tag.c_str(), kGaits[f.gait].name, "DURL"[f.facing], f.i);
    if (f.posture) {
      frameChecks(f, idle, [&](const char* c, bool bad, const std::string& why) { if (!std::strcmp(c, "W5") || !std::strcmp(c, "W7")) add(c, where, bad, why); });
      continue;
    }
    frameChecks(f, idle, [&](const char* c, bool bad, const std::string& why) { add(c, where, bad, why); });
    if (S.tips) tipsShown(f, [&](const char* c, bool bad, const std::string& why) { add(c, where, bad, why); });
  }
  if (S.tips) {
    const int sole = r.body == doll::BodySet::Youth || r.body == doll::BodySet::Child ? 6 : 7;
    for (int g : gaits)
      for (int fc : {2, 3}) {
        std::vector<const FD*> fr;
        for (const FD& f : S.frames) if (f.gait == g && f.facing == fc) fr.push_back(&f);
        tipsRegistered(fr, sole, [&](int i, bool bad, const std::string& why) {
          add("T2", fmt("%s %s %c%d", tag.c_str(), kGaits[g].name, "DURL"[fc], i), bad, why);
        });
      }
  }
  return res;
}

// ---------------------------------------------------------------- the matrix (5.5)
const char* const kGarbOf[] = {"", "apron", "smock", "jerkin", "dress", "tunic", "coat", "culturetunic", "kilt", "gown", "rags", "kaftan", "wrap", "poncho"};
std::string garmentSpec(const std::string& g) {
  if (g == "robe") return "cut=2";
  for (int i = 1; i < 14; i++) if (g == kGarbOf[i]) return fmt("garb=%d", i);
  return "";
}
// the pool's values for a role on a body set (sorted, unique)
std::vector<std::string> valuesOf(doll::Role role, doll::BodySet b) {
  const P::Pool& Pl = P::get();
  std::set<std::string> v;
  for (size_t pi = 0; pi < Pl.parts.size(); pi++) {
    if (!doll::partServes((int)pi, b)) continue;
    for (const P::RoleRec& rr : Pl.parts[pi].roles) if (rr.cat == (uint8_t)role) v.insert(Pl.s(rr.value));
  }
  return {v.begin(), v.end()};
}
bool hasHang(const std::string& style, doll::BodySet b) {
  const P::Pool& Pl = P::get();
  for (int pi : Pl.partsWithRole(doll::Role::Hair, style))
    if (doll::partServes(pi, b) && infoOf(pi).hair) return true;
  return false;
}
bool hairSpec(const std::string& style, bool child, std::string& spec) {
  for (int f = 0; f < (int)art::Hair::COUNT; f++)
    for (int v = 0; v < 5; v++) {
      if (!child && (v >= doll::hairVariants((art::Hair)f) || style != doll::hairStyleName(doll::hairStyleOf((art::Hair)f, (uint8_t)v)))) continue;
      if (child) {
        HumanLook L; L.age = 1; L.hair = (art::Hair)f; L.hairVar = (uint8_t)v;
        const doll::DollRecipe r = doll::resolve(L);
        bool ok = false;
        for (const doll::Need& n : r.needs) if (n.role == doll::Role::Hair && n.value == style) ok = true;
        if (!ok) continue;
      }
      spec = fmt("hair=%d,hairVar=%d", f, v);
      return true;
    }
  return false;
}

std::vector<WLook> buildMatrix() {
  std::vector<WLook> m;
  auto add = [&](const char* fam, const std::string& tag, int who, const std::string& spec, const char* gaits) {
    WLook l; l.family = fam; l.tag = tag; l.who = who; l.spec = spec; l.gaits = splitc(gaits, ','); m.push_back(l);
  };
  const std::string tw = "outfit=0,bottomColor=#4F4B6B";   // the woman in shirt and trousers
  // M1 reference
  add("M1", "hero", 0, "", "walk,run");
  add("M1", "defhero", 0, "weapon=0,topColor=#466E96,bottomColor=#4E3C2C,hairColor=#784624,palette=look", "walk,run");
  add("M1", "woman", 1, "", "walk,run");
  add("M1", "womant", 1, tw, "walk,run");
  add("M1", "womanb", 1, tw + ",bare=1", "walk,run");
  add("M1", "elfm", 0, "people=2,weapon=0", "walk,run");
  add("M1", "elfw", 1, tw + ",people=2", "walk,run");
  add("M1", "halfm", 0, "people=1,weapon=0", "walk,run");
  add("M1", "broadm", 0, "build=2,weapon=0", "walk,run");
  add("M1", "slimw", 1, tw + ",build=1", "walk,run");
  add("M1", "elderm", 0, "age=3,weapon=0", "walk");
  add("M1", "elderw", 1, tw + ",age=3", "walk");
  add("M1", "hdrawn", 0, "", "dwalk,drun");
  add("M1", "wdrawn", 1, tw + ",weapon=1", "dwalk,drun");
  // M2 every garment on every body that wears it (adults also slim and broad); the mail with the sword
  const char* bodyName[4] = {"man", "woman", "youth", "child"};
  for (int b = 0; b < 4; b++) {
    for (const std::string& g : valuesOf(doll::Role::Garment, (doll::BodySet)b)) {
      const std::string gs = garmentSpec(g);
      if (gs.empty()) continue;
      const int who = b == 1 || (b >= 2 && (g == "dress" || g == "gown")) ? 1 : 0;
      const std::string age = b == 2 ? ",age=2" : b == 3 ? ",age=1" : "";
      const std::string base = gs + ",weapon=0" + age;
      const char* gaits = b == 3 ? "walk" : "walk,run";
      add("M2", fmt("g_%s_%s", bodyName[b], g.c_str()), who, base, gaits);
      if (b < 2) {
        add("M2", fmt("g_%s_%s_slim", bodyName[b], g.c_str()), who, base + ",build=1", gaits);
        add("M2", fmt("g_%s_%s_broad", bodyName[b], g.c_str()), who, base + ",build=2", gaits);
      }
    }
  }
  for (int b = 0; b < 2; b++)
    if (!valuesOf(doll::Role::Armour, (doll::BodySet)b).empty()) add("M2", fmt("mail_%s", bodyName[b]), b, "outfit=4,weapon=1", "walk,run,dwalk,drun");
  // M3 cloaks x man / woman; headwear x man (+ the woman's veil and headscarf, the youth's hood and cap); neckwear x every body
  const char* cloaks[5] = {"", "travel", "fur", "noble", "cape"};
  for (int b = 0; b < 2; b++) {
    const std::vector<std::string> cv = valuesOf(doll::Role::Cloak, (doll::BodySet)b);
    for (int c = 1; c <= 4; c++) {
      if (std::find(cv.begin(), cv.end(), cloaks[c]) == cv.end()) continue;
      const std::string s = (c == 4 ? std::string("cape=1") : fmt("cloak=%d", c)) + ",weapon=1" + (b ? "," + tw : std::string());
      add("M3", fmt("cloak_%s_%s", bodyName[b], cloaks[c]), b, s, "walk,run,drun");
    }
  }
  const char* hw[9] = {"", "hood", "cap", "turban", "furhat", "veil", "circlet", "conical", "headscarf"};
  for (int h = 1; h <= 8; h++) add("M3", fmt("hat_man_%s", hw[h]), 0, fmt("headwear=%d,weapon=0", h), "walk,run");
  add("M3", "hat_man_legacyhood", 0, "hood=1,weapon=0", "walk,run");
  add("M3", "hat_woman_veil", 1, "headwear=5," + tw, "walk,run");
  add("M3", "hat_woman_headscarf", 1, "headwear=8," + tw, "walk,run");
  add("M3", "hat_youth_hood", 0, "headwear=1,age=2,weapon=0", "walk,run");
  add("M3", "hat_youth_cap", 0, "headwear=2,age=2,weapon=0", "walk,run");
  struct NeckDef { const char* name; const char* spec; };
  const NeckDef necks[5] = {{"scarf", "neck=1"}, {"kerchief", "neck=2"}, {"collar", "neck=3"}, {"torc", "neck=0,jewellery=2"}, {"amulet", "neck=0,amulet=1"}};
  for (int b = 0; b < 4; b++) {
    const std::vector<std::string> nv = valuesOf(doll::Role::Neck, (doll::BodySet)b);
    for (const NeckDef& n : necks) {
      if (std::find(nv.begin(), nv.end(), n.name) == nv.end()) continue;
      const std::string age = b == 2 ? ",age=2" : b == 3 ? ",age=1" : "";
      add("M3", fmt("neck_%s_%s", bodyName[b], n.name), b == 1 ? 1 : 0, std::string(n.spec) + ",weapon=0" + age + (b == 1 ? "," + tw : std::string()), b == 3 ? "walk" : "walk,run");
    }
  }
  // M4 every hanging hair style on the woman, the man, the youth girl (+ the child's hangs)
  for (const std::string& st : valuesOf(doll::Role::Hair, doll::BodySet::Woman)) {
    std::string hs;
    if (!hairSpec(st, false, hs)) continue;
    if (!hasHang(st, doll::BodySet::Woman)) {   // (part coverage) a style with only a moving sheen or tuft: once, on the man
      bool moving = false;
      for (int pi : P::get().partsWithRole(doll::Role::Hair, st)) moving = moving || P::get().parts[(size_t)pi].channel != P::get().ids.rest;
      if (moving) add("M4", "hair_man_" + st, 0, hs + ",weapon=0", "walk,run");
      continue;
    }
    add("M4", "hair_woman_" + st, 1, hs + "," + tw, "walk,run");
    add("M4", "hair_man_" + st, 0, hs + ",weapon=0", "walk,run");
    add("M4", "hair_youth_" + st, 1, hs + ",age=2,outfit=0,weapon=0", "walk,run");
  }
  for (const std::string& st : valuesOf(doll::Role::Hair, doll::BodySet::Child)) {
    if (!hasHang(st, doll::BodySet::Child)) continue;
    std::string hs;
    if (hairSpec(st, true, hs)) add("M4", "hair_child_" + st, 1, hs + ",age=1,outfit=0,weapon=0", "walk");
  }
  // M5 what is carried
  add("M5", "sword_man", 0, "weapon=1", "walk,run,dwalk,drun");
  add("M5", "sword_woman", 1, tw + ",weapon=1", "walk,run,dwalk,drun");
  add("M5", "sword_elf", 0, "people=2,weapon=1", "walk,run,dwalk,drun");
  add("M5", "axe_man", 0, "weapon=2,civil=1", "walk,run");
  add("M5", "bow_man", 0, "weapon=3,civil=1", "walk,run");
  add("M5", "hammer_man", 0, "weapon=6,civil=1", "walk,run");
  add("M5", "horn_man", 0, "weapon=12,civil=1", "walk,run");
  add("M5", "staff_civil_man", 0, "weapon=4,civil=1", "walk,run");
  add("M5", "staff_civil_woman", 1, tw + ",weapon=4,civil=1", "walk,run");
  add("M5", "staff_player", 0, "weapon=4", "walk,run");
  add("M5", "stick_man", 0, "weapon=9,civil=1", "walk,run");
  add("M5", "stick_woman", 1, tw + ",weapon=9,civil=1", "walk,run");
  add("M5", "stick_elder", 0, "age=3,weapon=9,civil=1", "walk");
  add("M5", "ledger_man", 0, "weapon=10,civil=1", "walk,run");
  add("M5", "scroll_man", 0, "weapon=11,civil=1", "walk,run");
  // M6 the walking postures (leg checks; the freeze keeps their front and back frames)
  for (int b = 0; b < 2; b++) add("M6", fmt("pose_%s", bodyName[b]), b, b ? tw + ",weapon=0" : "weapon=0", "carry,carry1,carry2,play,dance");
  add("M6", "pose_child", 0, "age=1,weapon=0", "carry,play");
  // (P2c, INTEGRATE i1) the slice looks (tools/doll/slice_looks.txt, embedded in test_doll_arms.cpp): the armour, helms,
  // shield and the spear / bow carriers judged on their moving side frames, so their parts are covered (COVER)
  {
    std::string t = dollSliceLooksText();
    size_t a = 0;
    while (a < t.size()) {
      size_t e = t.find('\n', a);
      if (e == std::string::npos) e = t.size();
      const std::string line = t.substr(a, e - a);
      a = e + 1;
      if (line.empty() || line[0] == '#') continue;
      std::vector<std::string> w;
      { size_t i = 0; while (i < line.size()) { while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) i++; size_t j = i;
          while (j < line.size() && line[j] != ' ' && line[j] != '\t' && line[j] != '\r') j++; if (j > i) w.push_back(line.substr(i, j - i)); i = j; } }
      if (w.size() < 3) continue;
      std::string gaits;
      for (const std::string& g : splitc(w.size() > 3 ? w[3] : std::string("walk,run"), ','))
        if (g == "walk" || g == "run" || g == "dwalk" || g == "drun") gaits += (gaits.empty() ? "" : ",") + g;
      if (gaits.empty()) gaits = "walk,run";
      add("P2c", "slice_" + w[0], w[1] == "woman" ? 1 : 0, w[2] == "-" ? std::string() : w[2], gaits.c_str());
    }
  }
  return m;
}

// the M7 B10 watch list: cloak x hood x staff / stick / sword x human / elf x man / woman (the game's default look)
struct M7 { std::string tag, spec; HumanLook L; };
std::vector<M7> m7Looks() {
  std::vector<M7> v;
  const char* cl[4] = {"", "travel", "fur", "noble"};
  struct It { const char* n; int weapon, civil; };
  const It items[3] = {{"staff", 4, 1}, {"stick", 9, 1}, {"sword", 1, 0}};
  for (int body = 0; body < 2; body++)
    for (int people : {0, 2})
      for (int c = 1; c <= 3; c++)
        for (int hood = 0; hood < 2; hood++)
          for (const It& it : items) {
            M7 m;
            m.L.body = (uint8_t)body; m.L.people = (uint8_t)people; m.L.cloak = (uint8_t)c; m.L.hood = hood != 0;
            m.L.weapon = (uint8_t)it.weapon; m.L.civil = (uint8_t)it.civil;
            m.tag = fmt("m7_%s_%s_%s%s_%s", body ? "woman" : "man", people ? "elf" : "human", cl[c], hood ? "_hood" : "", it.n);
            m.spec = fmt("body=%d,people=%d,cloak=%d,hood=%d,weapon=%d,civil=%d", body, people, c, hood, it.weapon, it.civil);
            v.push_back(m);
          }
  return v;
}

// part coverage: every part with a side view for a key the side moving frames name draws in at least one judged frame
std::vector<std::string> g_coverMiss;
int g_coverParts = 0;
void partCoverage(const std::vector<WLook>& looks) {
  const P::Pool& Pl = P::get();
  const P::Ids& I = Pl.ids;
  g_coverMiss.clear();
  // the keys each channel takes in any body's side walk / run / drawn frames
  std::map<uint16_t, std::set<uint16_t>> chanKeys;
  for (const P::ClipRec& c : Pl.clips) {
    if (c.facing != 2 && c.facing != 3) continue;
    if (c.when) continue;   // (INTEGRATE i1) a conditioned clip's keys (a shield bearer's, a spear or bow fighter's) bind only its own looks
    const bool moving = c.clip == (uint8_t)Clip::Walk && c.posture == P::kNone8 && (c.part == (uint8_t)RigPart::Loco || c.part == (uint8_t)RigPart::Run || (c.part == P::kPartDrawn && c.variant >= 1));
    if (!moving) continue;
    for (int i = 0; i < c.nFrames; i++)
      for (const P::KeyRec& k : Pl.frames[c.firstFrame + i].keys) chanKeys[k.channel].insert(k.key);
  }
  std::vector<char> drawn(Pl.parts.size(), 0);
  for (const WLook& w : looks) {
    std::vector<int> gaits;
    for (const std::string& g : w.gaits) { const int gi = gaitIx(g); if (gi >= 0 && kGaits[gi].posture < 0) gaits.push_back(gi); }
    if (gaits.empty()) continue;
    const doll::DollRecipe r = recipeOf(parse(w));
    for (const KeyFrame& k : sideFrames(r, gaits, false))
      for (int pi : r.parts) {
        const P::PartRec& p = Pl.parts[(size_t)pi];
        const uint16_t key = keyFor(p, *k.fr);
        if (key != I.dash && drawsView(P::viewOf(p, k.facing, key))) drawn[(size_t)pi] = 1;
      }
  }
  g_coverParts = 0;
  for (size_t pi = 0; pi < Pl.parts.size(); pi++) {
    const P::PartRec& p = Pl.parts[pi];
    if (p.channel == I.rest) continue;
    auto ck = chanKeys.find(p.channel);
    if (ck == chanKeys.end()) continue;
    bool side = false;
    for (uint32_t v = p.firstView; v < p.firstView + p.nViews; v++) {
      const P::ViewRec& vr = Pl.views[v];
      if ((vr.facing == 2 || vr.facing == 3) && vr.when == 0 && ck->second.count(vr.key) && drawsView(&vr)) side = true;
    }
    if (!side) continue;
    g_coverParts++;
    if (!drawn[pi]) g_coverMiss.push_back(Pl.s(p.name));
  }
}

std::vector<WLook> matrix(const std::string& filter) {
  const std::vector<WLook> all = buildMatrix();
  if (filter.empty() || filter == "all") { partCoverage(all); return all; }
  std::vector<WLook> v;
  if (filter.rfind("part:", 0) == 0) {
    const std::string pn = filter.substr(5);
    const int pi = doll::findPart(pn);
    for (const WLook& w : all) {
      const doll::DollRecipe r = recipeOf(parse(w));
      if (std::find(r.parts.begin(), r.parts.end(), pi) != r.parts.end()) v.push_back(w);
    }
    return v;
  }
  for (const std::string& f : splitc(filter, ','))
    for (const WLook& w : all)
      if (w.family == f || w.tag == f || (f.size() > 1 && f.back() == '*' && w.tag.rfind(f.substr(0, f.size() - 1), 0) == 0)) v.push_back(w);
  return v;
}

// ---------------------------------------------------------------- the freeze manifest (5.6)
uint64_t fnv(uint64_t h, const void* p, size_t n) {
  const uint8_t* b = (const uint8_t*)p;
  for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; }
  return h;
}
uint64_t frameHash(const RigSheet& s, const art::FrameRect& r) {
  uint64_t h = 1469598103934665603ull;
  const int16_t hd[4] = {r.w, r.h, r.px, r.py};
  h = fnv(h, hd, sizeof hd);
  for (int y = 0; y < r.h; y++)
    for (int x = 0; x < r.w; x++) { const uint32_t p = s.atlas.get(r.x + x, r.y + y); h = fnv(h, &p, 4); }
  return h;
}
// every frozen frame of a look: "<part> <facing> <clip> <variant> <i>" -> hash
void frozenFrames(const WLook& w, std::map<std::string, uint64_t>& out) {
  const Parsed p = parse(w);
  const doll::DollRecipe r = recipeOf(p);
  auto sheet = [&](const RigSheet& s, const std::string& name, bool posture) {
    for (int fc = 0; fc < 4; fc++)
      for (int c = 0; c < (int)art::CLIP_COUNT; c++)
        for (int v = 0; v < art::RIG_VARIANTS; v++) {
          const RigSheet::Span& sp = s.spans[RigSheet::slot(fc, (Clip)c, v)];
          if (sp.first < 0) continue;
          const bool side = fc == 2 || fc == 3;
          if (side && posture) continue;                                        // the walking postures' side frames
          if (side && c == (int)Clip::Walk && (name == "loco" || name == "run" || (name == "action" && v >= 1))) continue;   // walk, run, drawn walk / run
          for (int i = 0; i < sp.count; i++)
            out[fmt("%s %d %d %d %d", name.c_str(), fc, c, v, i)] = frameHash(s, s.frames[(size_t)sp.first + i]);
        }
  };
  bool posture = false;
  for (const std::string& g : w.gaits) { const int gi = gaitIx(g); if (gi >= 0 && kGaits[gi].posture >= 0) posture = true; }
  if (!posture) {
    sheet(doll::compose(r, RigPart::Loco), "loco", false);
    sheet(doll::compose(r, RigPart::Run), "run", false);
    sheet(doll::compose(r, RigPart::Death), "death", false);
    if (p.L.weapon >= 1 && p.L.weapon <= 8 && !p.L.civil) {
      sheet(doll::compose(r, RigPart::Action), "action", false);
      // (P2c) the drawn idle, walk and run moved from ACTION to DRAWN: their frames keep the manifest's "action" lines
      // (clips Idle / Walk variants 1-2, which ACTION no longer holds), so the freeze reads them there
      sheet(doll::compose(r, RigPart::Drawn), "action", false);
    }
  } else {
    for (const std::string& g : w.gaits) {
      const int gi = gaitIx(g);
      if (gi < 0 || kGaits[gi].posture < 0) continue;
      sheet(doll::composePosture(r, (art::Posture)kGaits[gi].posture, kGaits[gi].pv), g, true);
    }
  }
}
int freeze(const char* file, bool write) {
  const std::vector<WLook> looks = matrix("");
  if (write) {
    FILE* f = std::fopen(file, "wb");
    if (!f) { std::printf("FAIL: freeze: cannot write %s\n", file); return 1; }
    const doll::PoolStats st = doll::poolStats();
    std::fprintf(f, "# freeze manifest (P2B_MOTION_ROLLOUT 5.6): pool %s, %d parts; per matrix look every frame outside the side walk / run / drawn frames\n",
                 st.hash.empty() ? "(file)" : st.hash.c_str(), st.parts);
    size_t n = 0;
    for (const WLook& w : looks) {
      std::map<std::string, uint64_t> fr;
      frozenFrames(w, fr);
      for (const auto& kv : fr) std::fprintf(f, "%s %s %016llx\n", w.tag.c_str(), kv.first.c_str(), (unsigned long long)kv.second);
      n += fr.size();
    }
    std::fclose(f);
    std::printf("  freeze: wrote %zu frames of %zu looks to %s\n", n, looks.size(), file);
    return 0;
  }
  FILE* f = std::fopen(file, "rb");
  if (!f) { std::printf("FAIL: freeze: cannot read %s\n", file); return 1; }
  std::map<std::string, std::map<std::string, uint64_t>> man;
  char line[512];
  while (std::fgets(line, sizeof line, f)) {
    if (line[0] == '#') continue;
    char tag[128], part[32];
    int fc, c, v, i;
    unsigned long long h;
    if (std::sscanf(line, "%127s %31s %d %d %d %d %llx", tag, part, &fc, &c, &v, &i, &h) != 7) continue;
    man[tag][fmt("%s %d %d %d %d", part, fc, c, v, i)] = h;
  }
  std::fclose(f);
  // the integrator's ruled exceptions (freeze_accepted.txt beside the manifest): "<tag> <part> <f> <c> <v> <i>  # why" a line,
  // or "<tag> * # why" for every frame of a look; an accepted frame that changed is counted apart, never as changed
  std::set<std::string> accepted;
  {
    std::string af = file;
    const size_t sl = af.find_last_of("/\\");
    af = (sl == std::string::npos ? std::string() : af.substr(0, sl + 1)) + "freeze_accepted.txt";
    if (FILE* a = std::fopen(af.c_str(), "rb")) {
      char ln[512];
      while (std::fgets(ln, sizeof ln, a)) {
        if (ln[0] == '#') continue;
        char tag[128], part[32];
        int fc, c, v, i;
        if (std::sscanf(ln, "%127s %31s %d %d %d %d", tag, part, &fc, &c, &v, &i) == 6) accepted.insert(fmt("%s %s %d %d %d %d", tag, part, fc, c, v, i));
        else if (std::sscanf(ln, "%127s %31s", tag, part) == 2 && !std::strcmp(part, "*")) accepted.insert(std::string(tag) + " *");
      }
      std::fclose(a);
    }
  }
  int changed = 0, missing = 0, extra = 0, newLooks = 0, okChanged = 0;
  size_t checked = 0;
  std::map<std::string, int> byLook;
  for (const WLook& w : looks) {
    auto it = man.find(w.tag);
    if (it == man.end()) { newLooks++; continue; }
    std::map<std::string, uint64_t> fr;
    frozenFrames(w, fr);
    for (const auto& kv : it->second) {
      auto j = fr.find(kv.first);
      if (j == fr.end()) { missing++; if (g_rep) std::printf("  freeze %s %s: gone\n", w.tag.c_str(), kv.first.c_str()); continue; }
      checked++;
      if (j->second == kv.second) continue;
      if (accepted.count(w.tag + " " + kv.first) || accepted.count(w.tag + " *")) { okChanged++; if (g_rep) std::printf("  freeze accepted %s %s\n", w.tag.c_str(), kv.first.c_str()); continue; }
      changed++; byLook[w.tag]++;
      if (g_rep || changed <= 20) std::printf("  freeze CHANGED %s %s\n", w.tag.c_str(), kv.first.c_str());
    }
    for (const auto& kv : fr) if (!it->second.count(kv.first)) extra++;
  }
  std::printf("  freeze: %zu frames checked, %d changed, %d gone, %d new frames, %d looks not in the manifest; %d changed as ruled (freeze_accepted.txt)\n", checked, changed, missing, extra, newLooks, okChanged);
  for (const auto& kv : byLook) std::printf("  freeze changed %-28s %d frames\n", kv.first.c_str(), kv.second);
  return changed + missing;
}

// ---------------------------------------------------------------- the M7 B10 watch
int b10(std::vector<WFail>& out) {
  int bad = 0;
  double wl = 0, wr = 0;
  std::string wln, wrn;
  for (const M7& m : m7Looks()) {
    const doll::DollRecipe r = doll::resolve(m.L);
    const double lo = doll::compose(r, RigPart::Loco).bytes() / 1024.0, ru = doll::compose(r, RigPart::Run).bytes() / 1024.0;
    if (std::getenv("B10_AREA")) {   // (debugging) the RUN atlas's cut area against its size, and the largest cuts
      const auto sh = doll::compose(r, RigPart::Run);
      std::set<std::pair<int, int>> seen;
      long area = 0;
      std::string big;
      for (const auto& fr : sh.frames) if (seen.insert({fr.x, fr.y}).second) { area += (long)fr.w * fr.h; big += fmt(" %dx%d", fr.w, fr.h); }
      std::printf("  B10 area %s: atlas %dx%d = %d px, cuts %ld px (%zu):%s\n", m.tag.c_str(), sh.atlas.w, sh.atlas.h, sh.atlas.w * sh.atlas.h, area, seen.size(), big.c_str());
    }
    if (lo > wl) { wl = lo; wln = m.tag; }
    if (ru > wr) { wr = ru; wrn = m.tag; }
    const bool over = lo > 128.0 || ru > 64.0;
    if (over) { bad++; out.push_back({"B10", m.tag, fmt("LOCO %.1f KB (128), RUN %.1f KB (64): %s", lo, ru, m.spec.c_str())}); }
    if (g_rep || over) std::printf("  B10 M7 %-34s LOCO %6.1f  RUN %5.1f%s\n", m.tag.c_str(), lo, ru, over ? "  OVER" : "");
  }
  std::printf("  B10 M7 watch (%zu looks): worst LOCO %.1f KB (%s), RUN %.1f KB (%s); %d over budget\n", m7Looks().size(), wl, wln.c_str(), wr, wrn.c_str(), bad);
  return bad;
}

// ---------------------------------------------------------------- --frames: a 4x strip per look (idle, walk 0-5, run 0-5,
// then every walking posture's side frames: Carry 0-2, Play, Dance; R over L; on grass)
void strips(const std::string& tag, const doll::DollRecipe& r, const std::vector<int>& gaits, const char* dir) {
  const int CW = 40, CH = 48, S = 4, PX = 20, PY = 42;
  struct Col { int sheet; Clip clip; int variant; };   // the walk clip of a part, or a posture's frames (Idle 0 of its sheet)
  std::vector<RigSheet> sheets;
  std::map<int, int> byPart;
  auto partSheet = [&](RigPart p) { auto it = byPart.find((int)p); if (it != byPart.end()) return it->second; sheets.push_back(doll::compose(r, p)); return byPart[(int)p] = (int)sheets.size() - 1; };
  const int loco = partSheet(RigPart::Loco);
  std::vector<Col> cols;
  for (int g : gaits) {
    if (kGaits[g].posture < 0) { cols.push_back({partSheet(kGaits[g].part), Clip::Walk, kGaits[g].variant}); continue; }
    sheets.push_back(doll::composePosture(r, (art::Posture)kGaits[g].posture, kGaits[g].pv));
    cols.push_back({(int)sheets.size() - 1, Clip::Idle, 0});
  }
  auto width = [&](const Col& c) { return std::max({1, sheets[(size_t)c.sheet].count(2, c.clip, c.variant), sheets[(size_t)c.sheet].count(3, c.clip, c.variant)}); };
  int ncol = 1;
  for (auto& c : cols) ncol += width(c);
  const int W = ncol * CW * S, H = 2 * CH * S;
  std::vector<uint32_t> img((size_t)W * H);
  const uint32_t grass = 0xFF3C7A4Au, grass2 = 0xFF346C41u, dirt = 0xFF2A5A38u;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const int cy = (y / S) % CH;
      img[(size_t)y * W + x] = cy > PY ? dirt : ((x / S + y / S) % 7 == 0 ? grass2 : grass);
    }
  auto blit = [&](const RigSheet& s, int fc, Clip clip, int variant, int i, int col, int row) {
    const art::FrameRect* fr = s.frame(fc, clip, variant, i);
    if (!fr) return;
    for (int y = 0; y < fr->h; y++)
      for (int x = 0; x < fr->w; x++) {
        const uint32_t p = s.atlas.get(fr->x + x, fr->y + y);
        if (!(p >> 24)) continue;
        const int X = col * CW + PX + x - fr->px, Y = row * CH + PY + y - fr->py;
        if (X < col * CW || X >= (col + 1) * CW || Y < row * CH || Y >= (row + 1) * CH) continue;
        for (int a = 0; a < S; a++) for (int b = 0; b < S; b++) img[(size_t)(Y * S + a) * W + X * S + b] = p;
      }
  };
  for (int row = 0; row < 2; row++) {
    const int fc = row ? 3 : 2;
    blit(sheets[(size_t)loco], fc, Clip::Idle, 0, 0, 0, row);
    int col = 1;
    for (auto& c : cols) {
      const RigSheet& s = sheets[(size_t)c.sheet];
      const int n = s.count(fc, c.clip, c.variant);
      for (int i = 0; i < n; i++) blit(s, fc, c.clip, c.variant, i, col + i, row);
      col += width(c);
    }
  }
  // a column divider between the gaits (one dark pixel line at 1x), so the strip reads gait by gait
  {
    int col = 1;
    for (auto& c : cols) {
      for (int y = 0; y < H; y++) for (int b = 0; b < S / 2; b++) img[(size_t)y * W + col * CW * S + b] = 0xFF1E3A26u;
      col += width(c);
    }
  }
  const std::string path = std::string(dir) + "/" + tag + ".png";
  if (!writePng(path.c_str(), W, H, img)) std::printf("FAIL: --frames: cannot write %s\n", path.c_str());
}

// ---------------------------------------------------------------- the selftest: a synthetic figure, each wear failure cut into it
// The figure (facing R; L is its mirror): legs with boots, a torso, a head with its face, two arms with hands, a sleeve,
// a hang, a cloak, a flowing scarf, a hair hang behind, a stick in the hand, a sheath at the hip.
struct Synth { FD f, idle; };
void put(FD& f, int k, int X, int Y, uint8_t c) { f.g[k].set(f.facing == 3 ? -X - 1 : X, Y, c); f.has[k] = true; }
void rect(FD& f, int k, int u0, int y0, int u1, int y1, uint8_t c) { for (int y = y0; y <= y1; y++) for (int u = u0; u <= u1; u++) put(f, k, u, y, c); }
uint8_t sc(doll::Slot s, int st = 2) { return doll::code(s, st); }
void finishSynth(FD& f) {
  // the composites: body (legs, torso, arms, sleeves, head), full (body, hang, cloak, neck, hair, held, sheath), noheld
  const int order[] = {kCloakB, kHair, kArms, kLegs, kHang, kTorso, kSleeveR, kNeck, kHead, kFace, kSheath, kCloakF, kFree};
  for (int k : {kFull, kNoHeld, kBody, kArmsFree}) { f.g[k] = Grid(); f.has[k] = true; }
  for (int k : order) {
    if (!f.has[k]) continue;
    f.g[k].each([&](int X, int Y, uint8_t c) {
      f.g[kFull].set(X, Y, c);
      if (k != kFree) f.g[kNoHeld].set(X, Y, c);
      if (k == kArms || k == kLegs || k == kTorso || k == kSleeveR || k == kHead || k == kFace) f.g[kBody].set(X, Y, c);
      if (k == kArms || k == kFree) f.g[kArmsFree].set(X, Y, c);
    });
  }
  f.g[kArmsR] = f.g[kArms]; f.has[kArmsR] = true;
}
FD synthFrame(int facing, bool run, int mutate) {
  FD f;
  f.facing = facing; f.run = run; f.gait = run ? 1 : 0;
  const int lean = run ? 4 : 0, hip = run ? 2 : 0;
  f.lean = facing == 3 ? -lean : lean; f.hipX = facing == 3 ? -hip : hip;
  f.neckX = facing == 3 ? -lean : lean;
  // legs: two shafts, boots on the ground row
  rect(f, kLegs, -3, -9, -1, -3, sc(kSlotThigh)); rect(f, kLegs, -3, -2, -1, -1, sc(kSlotShin)); rect(f, kLegs, -3, -1, 2, 0, sc(kSlotBoot));
  rect(f, kLegs, 1, -9, 3, -3, sc(kSlotThigh)); rect(f, kLegs, 1, -2, 3, -1, sc(kSlotShin)); rect(f, kLegs, 1, -1, 6, 0, sc(kSlotBoot));
  // torso: shoulders moved by the lean, hem by the hip
  for (int y = -19; y <= -10; y++) { const int sh = hip + (lean - hip) * (-10 - y) / 9; rect(f, kTorso, -4 + sh, y, 3 + sh, y, sc(doll::Slot::Cloth1)); }
  // the head with its face (eye at u 4, y -27; the chin y -20)
  rect(f, kHead, -4 + lean, -32, 4 + lean, -20, sc(doll::Slot::Skin)); f.g[kFace] = f.g[kHead]; f.has[kFace] = true;
  put(f, kFace, 3 + lean, -27, sc(doll::Slot::Eyes)); put(f, kFace, 3 + lean, -28, sc(doll::Slot::Face, 0));
  // the arm and its hand (the hand at u 4..5, y -10..-9), the sleeve on its upper arm
  rect(f, kArms, 0 + lean, -18, 1 + lean, -13, sc(kSlotArm)); rect(f, kArms, 1 + lean, -12, 2 + lean, -11, sc(kSlotArm)); rect(f, kArms, 2 + lean, -10, 3 + lean, -9, sc(kSlotHand));
  rect(f, kSleeveR, 0 + lean, -18, 1 + lean, -15, sc(doll::Slot::Cloth1, 3));
  // the hang below the hem, trailing; the cloak behind the back; the scarf at the neck; the hair hang behind the neck
  rect(f, kHang, -4 + hip, -9, 2 + hip, -7, sc(doll::Slot::Cloth1, 1));
  rect(f, kCloakB, -6 + lean, -19, -4 + hip, -9, sc(doll::Slot::Cloak));
  rect(f, kNeck, -2 + lean, -20, 1 + lean, -19, sc(doll::Slot::Accent)); f.g[kNeckFlow] = f.g[kNeck]; f.has[kNeckFlow] = true;
  rect(f, kHair, -5 + lean, -24, -5 + lean, -16, sc(doll::Slot::Hair));
  // the stick in the hand (the hand's column + 1, from the hand down to the ground); the sheath behind the hip
  rect(f, kFree, 4 + lean, -10, 4 + lean, -1, sc(doll::Slot::Leather));
  rect(f, kSheath, -6, -12, -5, -6, sc(doll::Slot::Leather, 1));
  switch (mutate) {
    case 3: f.g[kHang] = Grid(); rect(f, kHang, -4 + hip + 3, -9, 2 + hip + 3, -7, sc(doll::Slot::Cloth1, 1)); rect(f, kHang, 6 + hip, -11, 6 + hip, -11, sc(doll::Slot::Cloth1, 1)); break;   // a hang 3 px forward
    case 7: put(f, kFree, 3 + lean, -27, sc(doll::Slot::Leather)); rect(f, kFree, 3 + lean, -26, 3 + lean, -11, sc(doll::Slot::Leather)); break;   // a staff over the eye
    case 6: f.g[kFree] = Grid(); for (int y = -10; y <= 0; y++) put(f, kFree, 3 + lean + 3 - ((y + 10) * (lean - 1)) / 10, y, sc(doll::Slot::Leather)); f.has[kFree] = true; break;   // a stick 3 px off the hand
    case 8: rect(f, kCloakB, -9 + lean, -19, -7 + lean, -12, sc(doll::Slot::Cloak)); break;   // a cloak 2 columns past the box
    case 9: rect(f, kHair, -4 + lean, -19, 4 + lean, -19, sc(doll::Slot::Hair)); rect(f, kHair, 4 + lean, -18, 4 + lean, -15, sc(doll::Slot::Hair)); break;   // a braid on the chest
    case 2: f.g[kSleeveR] = Grid(); rect(f, kSleeveR, -10 + lean, -18, -9 + lean, -15, sc(doll::Slot::Cloth1, 3)); break;   // a sleeve off its arm
    case 4: { Grid t = f.g[kTorso]; f.g[kTorso] = Grid(); t.each([&](int X, int Y, uint8_t c) { f.g[kTorso].set(X + (facing == 3 ? 3 : -3), Y, c); }); } break;   // an upright torso
    case 5: f.g[kNeck] = Grid(); rect(f, kNeck, -2 + lean, -20, 1 + lean, -19, sc(doll::Slot::Accent)); rect(f, kNeck, 5, -4, 6, -4, sc(doll::Slot::Accent)); f.g[kNeckFlow] = Grid(); f.g[kNeckFlow].set(0, 0, 0); f.has[kNeckFlow] = false; break;   // a loose scrap
    case 10: f.g[kSheath] = Grid(); rect(f, kSheath, -3, -2, -2, 0, sc(doll::Slot::Leather, 1)); rect(f, kSheath, -5, -12, -4, -3, sc(doll::Slot::Leather, 1)); break;   // a scabbard over the boot
    case 11: f.g[kLegs] = Grid(); rect(f, kLegs, -3, -9, -1, -3, sc(kSlotThigh)); rect(f, kLegs, 1, -9, 3, -3, sc(kSlotThigh)); rect(f, kLegs, -3, -2, 3, -1, sc(kSlotShin)); rect(f, kLegs, -3, 0, 6, 0, sc(kSlotBoot));
             rect(f, kHang, -4, -6, 6, -1, sc(doll::Slot::Cloth1, 1)); rect(f, kHang, -4, 0, 6, 0, sc(doll::Slot::Cloth1, 1)); break;   // the hem hides the shoes
  }
  finishSynth(f);
  return f;
}
int selftest(bool report) {
  struct Case { const char* name; int mutate; const char* expect; };
  const Case cases[] = {{"good", 0, ""}, {"sleeve off its arm", 2, "W2"}, {"hang 3 px forward", 3, "W3"}, {"upright torso", 4, "W4"},
                        {"loose scrap", 5, "W5"}, {"stick 3 px off the hand", 6, "W6"}, {"staff over the eye", 7, "W7"},
                        {"cloak 2 columns past the box", 8, "W8"}, {"braid on the chest", 9, "W9"}, {"scabbard over the boot", 10, "W10"},
                        {"hem hides the shoes", 11, "T1"}};
  int missed = 0;
  for (const Case& c : cases)
    for (int fc : {2, 3}) {
      const FD f = synthFrame(fc, true, c.mutate), idle = synthFrame(fc, false, 0);
      FD id = idle;
      id.lean = 0; id.hipX = 0;
      std::set<std::string> got;
      std::string why;
      frameChecks(f, id, [&](const char* k, bool bad, const std::string& w) { if (bad) { got.insert(k); why += std::string(" ") + k + ": " + w + ";"; } });
      if (c.mutate == 11 || c.mutate == 0) tipsShown(f, [&](const char* k, bool bad, const std::string& w) { if (bad) { got.insert(k); why += std::string(" ") + k + ": " + w + ";"; } });
      const bool ok = c.expect[0] ? (got.size() == 1 && got.count(c.expect)) : got.empty();
      if (!ok) missed++;
      if (report || !ok) std::printf("  wear selftest %-30s %c expect {%s} got {%s}%s%s\n", c.name, "DURL"[fc], c.expect, [&] { std::string s; for (auto& g : got) s += (s.empty() ? "" : ",") + g; return s; }().c_str(),
                                     ok ? "" : "  MISSED:", ok ? "" : why.c_str());
    }
  // W1: a deleted sleeve view (the woman in her gown: her right sleeve's pB views taken away)
  {
    const P::Pool& Pl = P::get();
    HumanLook L = a3Woman();
    L.garb = 9;
    const doll::DollRecipe r = doll::resolve(L);
    int sleeve = -1;
    for (int pi : r.parts) {
      const PartInfo f = infoOf(pi);
      if ((f.sleeve || f.arms) && f.side == 1 && P::viewOf(Pl.parts[(size_t)pi], 2, Pl.strId("pB"))) { sleeve = pi; if (f.sleeve) break; }
    }
    int base = 0, cut = 0;
    std::string why;
    w1Look(r, {0, 1}, [&](const char*, bool bad, const std::string&) { base += bad; });
    const uint16_t pB = Pl.strId("pB");
    const Hide hide = [&](int part, int, uint16_t key) { return part == sleeve && key == pB; };
    w1Look(r, {0, 1}, [&](const char*, bool bad, const std::string& w) { cut += bad; if (bad && w.find(Pl.s(Pl.parts[(size_t)std::max(sleeve, 0)].name)) != std::string::npos) why = w; }, &hide);
    const bool ok = sleeve >= 0 && cut > base && !why.empty();
    if (!ok) missed++;
    if (report || !ok) std::printf("  wear selftest %-30s * expect {W1} %s (%d fails as drawn, %d with the view gone)%s\n", "deleted sleeve view",
                                   sleeve >= 0 ? Pl.s(Pl.parts[(size_t)sleeve].name).c_str() : "(no sleeve)", base, cut, ok ? "" : "  MISSED");
  }
  // T2: a planted shoe that slides 2 px between two frames of one stance
  {
    int bad[2] = {0, 0};
    for (int slide = 0; slide < 2; slide++) {
      FD a, b;
      for (FD* d : {&a, &b}) { d->facing = 2; d->has[kLegs] = d->has[kFull] = true; }
      rect(a, kLegs, 0, 0, 6, 0, sc(kSlotBoot)); rect(b, kLegs, -3 + slide * 2, 0, 3 + slide * 2, 0, sc(kSlotBoot));
      a.g[kFull] = a.g[kLegs]; b.g[kFull] = b.g[kLegs];
      a.plant0 = 3; a.plant1 = 0; b.plant0 = 0; b.plant1 = -3; a.i = 0; b.i = 1;
      tipsRegistered({&a, &b}, 7, [&](int, bool x, const std::string&) { bad[slide] += x; });
    }
    const bool ok = bad[0] == 0 && bad[1] > 0;
    if (!ok) missed++;
    if (report || !ok) std::printf("  wear selftest %-30s R registered %d fails (0), slid 2 px %d fails (>0)%s\n", "shoe slides 2 px", bad[0], bad[1], ok ? "" : "  MISSED");
  }
  std::printf("  wear selftest: %s\n", missed ? fmt("%d MISSED", missed).c_str() : "ALL CAUGHT");
  return missed;
}

// ---------------------------------------------------------------- reporting
struct Summary { int looks = 0, frames = 0, checks = 0; std::map<std::string, int> byCheck; };
void printFails(const std::vector<WFail>& f, size_t from, bool all) {
  std::map<std::string, int> per;
  for (size_t i = from; i < f.size(); i++) {
    const std::string k = f[i].check + " " + f[i].where.substr(0, f[i].where.find(' '));
    if (all || ++per[k] <= 2) std::printf("FAIL %s %s: %s\n", f[i].check.c_str(), f[i].where.c_str(), f[i].why.c_str());
  }
}

int wearCmd(int argc, char** argv) {
  const char *pool = nullptr, *looksFile = nullptr, *framesDir = nullptr, *frzW = nullptr, *frzC = nullptr;
  std::string filter;
  bool self = false, b10only = false, list = false, any = false, kb = false;
  const char *fpTag = nullptr, *fpPart = nullptr, *fpOut = nullptr;   // --frame-png TAG PART F CLIP VARIANT I OUT.png (a freeze line's frame, 8x)
  int fpF = 0, fpC = 0, fpV = 0, fpI = 0;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--pool") && i + 1 < argc) pool = argv[++i];
    else if (!std::strcmp(argv[i], "--looks") && i + 1 < argc) looksFile = argv[++i];
    else if (!std::strcmp(argv[i], "--matrix") && i + 1 < argc) filter = argv[++i];
    else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) framesDir = argv[++i];
    else if (!std::strcmp(argv[i], "--freeze-write") && i + 1 < argc) frzW = argv[++i];
    else if (!std::strcmp(argv[i], "--freeze-check") && i + 1 < argc) frzC = argv[++i];
    else if (!std::strcmp(argv[i], "--report")) g_rep = true;
    else if (!std::strcmp(argv[i], "--selftest")) self = true;
    else if (!std::strcmp(argv[i], "--b10")) b10only = true;
    else if (!std::strcmp(argv[i], "--list")) list = true;
    else if (!std::strcmp(argv[i], "--kb")) kb = true;
    else if (!std::strcmp(argv[i], "--frame-png") && i + 7 < argc) { fpTag = argv[i + 1]; fpPart = argv[i + 2]; fpF = std::atoi(argv[i + 3]); fpC = std::atoi(argv[i + 4]);
                                                                    fpV = std::atoi(argv[i + 5]); fpI = std::atoi(argv[i + 6]); fpOut = argv[i + 7]; i += 7; }
    else if (!std::strcmp(argv[i], "--ascii") && i + 1 < argc) g_ascii = argv[++i];
  }
  if (pool) {
    std::string err;
    if (!doll::loadPoolFile(pool, &err)) { std::printf("FAIL: --pool %s: %s\n", pool, err.c_str()); return 1; }
  }
  const auto t0 = std::chrono::steady_clock::now();
  int fails = 0;
  if (self) { fails += selftest(g_rep); any = true; }
  if (frzW || frzC) { fails += freeze(frzW ? frzW : frzC, frzW != nullptr); any = true; }
  std::vector<WFail> out;
  if (b10only) { fails += b10(out); any = true; }
  if (any && filter.empty() && !looksFile && !list) {
    std::printf("  doll-wear: %.1f s\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
    return fails ? 1 : 0;
  }
  const std::vector<WLook> looks = looksFile ? readLooksFile(looksFile) : matrix(filter);
  if (fpTag) {
    for (const WLook& w : matrix("")) {
      if (w.tag != fpTag) continue;
      const doll::DollRecipe r = recipeOf(parse(w));
      const std::string pn = fpPart;
      RigSheet sh;
      if (pn == "loco") sh = doll::compose(r, RigPart::Loco);
      else if (pn == "run") sh = doll::compose(r, RigPart::Run);
      else if (pn == "death") sh = doll::compose(r, RigPart::Death);
      else if (pn == "action") sh = doll::compose(r, RigPart::Action);
      else if (pn == "drawn") sh = doll::compose(r, RigPart::Drawn);
      else { const int gi = gaitIx(pn); if (gi >= 0 && kGaits[gi].posture >= 0) sh = doll::composePosture(r, (art::Posture)kGaits[gi].posture, kGaits[gi].pv); }
      const art::FrameRect* fr = sh.frame(fpF, (Clip)fpC, fpV, fpI);
      if (!fr) { std::printf("FAIL: --frame-png: no such frame\n"); return 1; }
      const int S = 8, W = 48 * S, H = 48 * S;
      std::vector<uint32_t> img((size_t)W * H, 0xFF3C7A4Au);
      for (int y = 0; y < fr->h; y++)
        for (int x = 0; x < fr->w; x++) {
          const uint32_t p = sh.atlas.get(fr->x + x, fr->y + y);
          if (!(p >> 24)) continue;
          const int X = 24 + x - fr->px, Y = 40 + y - fr->py;
          if (X < 0 || Y < 0 || X >= 48 || Y >= 48) continue;
          for (int a = 0; a < S; a++) for (int b = 0; b < S; b++) img[(size_t)(Y * S + a) * W + X * S + b] = p;
        }
      writePng(fpOut, W, H, img);
      return 0;
    }
    std::printf("FAIL: --frame-png: no matrix look %s\n", fpTag);
    return 1;
  }
  if (kb) {   // the atlases of every look (the game's own ramps; sizes do not depend on colour): "KB tag LOCO RUN"
    for (const WLook& w : looks) {
      const doll::DollRecipe r = doll::resolve(parse(w).L);
      std::printf("KB %-30s %6.1f %5.1f\n", w.tag.c_str(), doll::compose(r, RigPart::Loco).bytes() / 1024.0, doll::compose(r, RigPart::Run).bytes() / 1024.0);
    }
    for (const M7& m : m7Looks()) {
      const doll::DollRecipe r = doll::resolve(m.L);
      std::printf("KB %-30s %6.1f %5.1f\n", m.tag.c_str(), doll::compose(r, RigPart::Loco).bytes() / 1024.0, doll::compose(r, RigPart::Run).bytes() / 1024.0);
    }
    return 0;
  }
  if (list) {
    for (const WLook& w : looks) {
      std::string g; for (auto& s : w.gaits) g += (g.empty() ? "" : ",") + s;
      std::printf("%-30s %-6s %s %s   # %s\n", w.tag.c_str(), w.who ? "woman" : "hero", w.spec.empty() ? "-" : w.spec.c_str(), g.c_str(), w.family.c_str());
    }
    std::printf("  matrix: %zu looks\n", looks.size());
    return 0;
  }
  Summary sm;
  for (const WLook& w : looks) {
    const Parsed p = parse(w);
    if (!p.bad.empty()) std::printf("  wear: %s: unknown field(s) %s\n", w.tag.c_str(), p.bad.c_str());
    const doll::DollRecipe r = recipeOf(p);
    std::vector<int> gaits;
    for (const std::string& g : w.gaits) { const int gi = gaitIx(g); if (gi >= 0) gaits.push_back(gi); }
    const size_t from = out.size();
    const LookResult lr = wearLook(w.tag, r, gaits, out);
    sm.looks++; sm.frames += lr.frames; sm.checks += lr.checks;
    for (size_t i = from; i < out.size(); i++) sm.byCheck[out[i].check]++;
    if (g_rep) std::printf("  wear %-30s %3d frames %4d checks %3d fails\n", w.tag.c_str(), lr.frames, lr.checks, lr.fails);
    printFails(out, from, g_rep);
    if (framesDir) strips(w.tag, r, gaits, framesDir);
  }
  if (!looksFile && (filter.empty() || filter == "all")) {
    for (const std::string& m : g_coverMiss) { out.push_back({"COVER", m, "no judged frame draws it"}); sm.byCheck["COVER"]++; std::printf("FAIL COVER %s: no judged frame draws it\n", m.c_str()); }
    std::printf("  part coverage: %d parts with side moving views, %zu unjudged\n", g_coverParts, g_coverMiss.size());
  }
  std::string per;
  for (const auto& kv : sm.byCheck) per += fmt(" %s %d", kv.first.c_str(), kv.second);
  std::printf("  B11 wear: %d looks, %d frames, %d checks, %zu failures%s; %.1f s\n", sm.looks, sm.frames, sm.checks, out.size(), per.c_str(),
              std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
  return (fails || !out.empty()) ? 1 : 0;
}

}  // namespace
}  // namespace wear

RPG_TEST_CMD("--doll-wear", "P2b B11 wear, the look matrix, freeze and the M7 B10 watch [--pool f] [--matrix FILTER | --looks f] [--report] [--frames dir] [--selftest] [--freeze-write f | --freeze-check f] [--b10] [--list] [--kb]", wear::wearCmd);

// ---------------------------------------------------------------- the 5.8 contract (test_doll.cpp calls these)
#ifdef EMB_DOLL_MOTION
namespace dollmotion {
namespace {
std::vector<int> gaitsOf(const std::vector<std::string>& g) {
  std::vector<int> v;
  for (const std::string& s : g) { const int gi = wear::gaitIx(s); if (gi >= 0) v.push_back(gi); }
  return v;
}
bool g_coverPending = false;
}  // namespace
std::vector<Look> matrixLooks(const std::string& filter) {
  std::vector<Look> v;
  for (const wear::WLook& w : wear::matrix(filter)) {
    Look l;
    l.tag = w.tag; l.who = w.who; l.spec = w.spec; l.gaits = w.gaits;
    v.push_back(l);
  }
  g_coverPending = filter.empty() || filter == "all";
  return v;
}
void wearChecks(const LookFrames& f, std::vector<Fail>& out, bool report) {
  wear::g_rep = report;
  std::vector<wear::WFail> w;
  wear::wearLook(f.look.tag, f.recipe, gaitsOf(f.look.gaits), w);
  if (g_coverPending) {
    g_coverPending = false;
    for (const std::string& m : wear::g_coverMiss) w.push_back({"COVER", m, "no judged frame draws it (part coverage)"});
  }
  for (const wear::WFail& x : w) {
    out.push_back(Fail{x.check, x.where, x.why});
    if (report) std::printf("FAIL %s %s: %s\n", x.check.c_str(), x.where.c_str(), x.why.c_str());
  }
}
int wearSelftest(bool report) { return wear::selftest(report); }
int freezeCmd(const char* file, bool write) { return wear::freeze(file, write); }
void b10Watch(std::vector<Fail>& out, bool report) {
  wear::g_rep = report;
  std::vector<wear::WFail> w;
  wear::b10(w);
  for (const wear::WFail& x : w) out.push_back(Fail{x.check, x.where, x.why});
}
void writeStrips(const LookFrames& f, const char* dir) { wear::strips(f.look.tag, f.recipe, gaitsOf(f.look.gaits), dir); }
}  // namespace dollmotion
#endif
