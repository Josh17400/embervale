// test_doll_motion.cpp (GATES-LIMBS, docs/P2B_MOTION_ROLLOUT.md 5.2 5.4 5.6 5.7): rig 2's side walk and run measured
// the way .tmp/judge/proto2/check.py (the oracle) measures them, in C++ on the game's own compositor:
//   shoot()          every side frame of a look composed three ways: each part of the recipe ALONE on its own layer (probe
//                    codes), the whole figure raw (probe codes) and finished (the look's ramps): proto2 dump.cpp's data
//   limbsChecks()    B11 limbs (check.py's FARWIDTH LEGLEN THICK HEAD BUNCH REG HIPLINE ANKLE FLIGHT PUSH FOLD POST BOB
//                    RHYTHM, plus the re-homed AREA HIPW SLIVER SQUAT JOINTS GROUND) and B12 boots (BOOT BOOTTILT
//                    BOOTSIZE); --report prints each look's block in check.py's own words, so the two diff cleanly
//                    (.tmp/judge/rollout/parity.py)
//   limbsSelftest()  check.py --synth ported (a synthetic good look passes; each synthetic failure fails exactly its own
//                    check) at adult, youth and child scale, both facings
// The adult constants are check.py's; the youth, the child and the long-garment tips read a per-body table (kBodies).
// Coordinates: u forward (+; R u = x, L u = -x - 1), y up negative, both from the pivot (the ground row is y 0).
#include "doll_motion.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <tuple>

#include "rpg/art/art_human.h"
#include "rpg/art/art_rig.h"
#include "rpg/art/doll/doll_internal.h"

namespace dollmotion {
namespace doll = art::doll;
namespace P = art::doll::pool;
using art::Clip;
using art::RigPart;
using art::RigSheet;

namespace {
std::string fmt(const char* f, ...) {
  char b[2048];
  va_list a;
  va_start(a, f);
  std::vsnprintf(b, sizeof b, f, a);
  va_end(a);
  return b;
}
uint32_t hexc(uint32_t rgb) { return 0xFF000000u | (rgb & 0xFF) << 16 | (rgb & 0xFF00) | (rgb >> 16 & 0xFF); }
// the A3 looks (the bases of every looks-file line; the same as test_doll.cpp's and dump.cpp's)
art::HumanLook a3Hero() {
  art::HumanLook L;
  L.body = 0; L.hair = art::Hair::Short; L.hairVar = 2; L.neck = 1; L.weapon = 1; L.outfit = art::Outfit::Tunic;
  L.skin = hexc(0xE9A077); L.hairColor = hexc(0xAA4A2C); L.topColor = hexc(0x28728C); L.bottomColor = hexc(0x4F4B6B); L.tabardColor = hexc(0xC83E38);
  L.eyeColor = hexc(0x3A78B0);
  return L;
}
art::HumanLook a3Woman() {
  art::HumanLook L;
  L.body = 1; L.hair = art::Hair::Braids; L.hairVar = 0; L.outfit = art::Outfit::Dress;
  L.skin = hexc(0xEEAA86); L.hairColor = hexc(0xC79346); L.topColor = hexc(0x922F60); L.tabardColor = hexc(0x28728C);
  L.eyeColor = hexc(0x3F8A5A);
  return L;
}
bool applyField(art::HumanLook& L, const std::string& k, const std::string& v) { return art::doll::applyLookField(L, k, v); }   // (P2c: the one list, doll.h)
struct LookOpts { art::HumanLook L; bool lookPal = false, bare = false; };
LookOpts lookOf(const Look& l) {
  LookOpts o;
  o.L = l.who ? a3Woman() : a3Hero();
  size_t a = 0;
  while (a < l.spec.size()) {
    size_t b = l.spec.find(',', a);
    if (b == std::string::npos) b = l.spec.size();
    const std::string kv = l.spec.substr(a, b - a);
    const size_t eq = kv.find('=');
    if (eq != std::string::npos) {
      const std::string k = kv.substr(0, eq), val = kv.substr(eq + 1);
      if (k == "palette") o.lookPal = val == "look";
      else if (k == "bare") o.bare = val == "1";
      else if (!applyField(o.L, k, val)) std::printf("  motion: %s: unknown field %s\n", l.tag.c_str(), kv.c_str());
    }
    a = b + 1;
  }
  return o;
}
// the probe palette: every (slot, step) its own colour, the code in the low byte; the leg regions recoded so T K S B O
// survive (Glow Field Fx Plume Device), as dump.cpp's probed()
doll::DollRecipe probed(doll::DollRecipe r) {
  for (int s = 0; s < doll::SLOT_COUNT; s++)
    for (int k = 0; k < 5; k++) r.ramps.c[s][k] = 0xFF000000u | 0x40u << 16 | 0x80u << 8 | (uint32_t)(2 + s * 5 + k);
  r.region[(int)doll::Region::Thigh] = (uint8_t)doll::Slot::Glow;
  r.region[(int)doll::Region::Knee] = (uint8_t)doll::Slot::Field;
  r.region[(int)doll::Region::Shin] = (uint8_t)doll::Slot::Fx;
  r.region[(int)doll::Region::Boot] = (uint8_t)doll::Slot::Plume;
  r.region[(int)doll::Region::Sole] = (uint8_t)doll::Slot::Device;
  return r;
}
const P::ClipRec* clipOf(const doll::DollRecipe& r, RigPart part, int fc, Clip clip, int variant) {
  for (const P::ClipRec* c : P::clipsOf(r.bodyKey, P::poolPart(part), r.family, -1, &r))
    if (c->clip == (uint8_t)clip && c->facing == fc && c->variant == variant) return c;
  return nullptr;
}
bool condHolds(const doll::DollRecipe& r, const std::string& cond) {
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
// does part pi draw anything in this frame (the compositor's own key and view choice)? A part alone composes no frame
// where it does not, and the atlas skips empty frames: the index of frame i in the part's own sheet counts these (dump.cpp)
bool partDraws(const doll::DollRecipe& r, int pi, int fc, const P::FrameRec& f) {
  const P::Pool& PP = P::get();
  const P::PartRec& p = PP.parts[(size_t)pi];
  uint16_t key = PP.ids.rest;
  if (p.channel != PP.ids.rest) {
    for (const P::KeyRec& k : f.keys) if (k.channel == p.channel) key = k.key;
    if (key == PP.ids.dash) return false;
  }
  if (key == 0xFFFF) return false;
  const P::ViewRec* v = P::viewOf(p, fc, key, 0);
  for (uint32_t wi : p.whenViews) {
    const P::ViewRec& w = PP.views[wi];
    if ((w.flags & 1) && w.facing == fc && w.key == key && condHolds(r, PP.s(w.when))) { v = &w; break; }
  }
  if (!v) return false;
  auto paints = [](const P::ViewRec& vv) { for (uint8_t c : vv.cells) if (c >= 2) return true; return false; };
  if (paints(*v)) return true;
  for (uint32_t wi : p.whenViews) {
    const P::ViewRec& w = PP.views[wi];
    if (!(w.flags & 1) && w.facing == fc && w.key == key && condHolds(r, PP.s(w.when)) && paints(w)) return true;
  }
  return false;
}
Img cutImg(const RigSheet& s, const art::FrameRect* fr, bool rgba) {
  Img m;
  if (!fr) return m;
  m.w = fr->w; m.h = fr->h; m.px = fr->px; m.py = fr->py;
  m.c.assign((size_t)m.w * m.h, 0);
  for (int y = 0; y < m.h; y++)
    for (int x = 0; x < m.w; x++) {
      const uint32_t p = s.atlas.get(fr->x + x, fr->y + y);
      m.c[(size_t)y * m.w + x] = rgba ? ((p >> 24) ? p : 0u) : ((p >> 24) ? (p & 0xFF) : 0u);
    }
  return m;
}
struct GaitDef { const char* name; RigPart part; Clip clip; int variant; };
const GaitDef kGaitDefs[5] = {{"idle", RigPart::Loco, Clip::Idle, 0}, {"walk", RigPart::Loco, Clip::Walk, 0}, {"run", RigPart::Run, Clip::Walk, 0},
                              {"dwalk", RigPart::Drawn, Clip::Walk, 1}, {"drun", RigPart::Drawn, Clip::Walk, 2}};
std::string g_poolLoaded;

// ---- the walking postures (gates-wear fix round 1)
// doll::composePosture is the game's own posture compositor, but it takes no ComposeOpts: its frames are finished (A3's
// selout lights the top-left outline step 0 -> 1 on lit slots) and trimmed (the pivot kept). Trimming is harmless (Img keeps
// the pivot). The finish is undone on the legs exactly: a second compose maps the five leg regions to Fx, the one leg probe
// slot the finish leaves alone, and its steps replace the first compose's on every leg pixel (the regions come from the
// first). composePosture also adds the posture's prop (withPosture); a part composed ALONE hides the other props for the
// compose (their bodyMask cleared, then restored), so each layer is that part only, whole even where the load covers it.
struct PostureDef { const char* name; art::Posture p; uint8_t v; };
const PostureDef kPostureDefs[] = {{"carry", art::Posture::Carry, 0}, {"carry1", art::Posture::Carry, 1}, {"carry2", art::Posture::Carry, 2},
                                   {"play", art::Posture::Play, 0}, {"dance", art::Posture::Dance, 0}};
doll::DollRecipe probedFx(doll::DollRecipe r) {   // a probed recipe with the leg regions on Fx (unlit: the finish leaves their steps)
  for (doll::Region g : {doll::Region::Thigh, doll::Region::Knee, doll::Region::Shin, doll::Region::Boot, doll::Region::Sole})
    r.region[(int)g] = (uint8_t)doll::Slot::Fx;
  return r;
}
void unfinishLegs(Img& a, const Img& fx) {
  if (a.w != fx.w || a.h != fx.h || a.px != fx.px || a.py != fx.py) return;
  const int fxSlot = (int)doll::Slot::Fx;
  for (size_t k = 0; k < a.c.size(); k++) {
    const int c = (int)a.c[k], b = (int)fx.c[k];
    if (c < 2 || b < 2) continue;
    const int s = (c - 2) / 5;
    const bool legSlot = s == (int)doll::Slot::Glow || s == (int)doll::Slot::Field || s == (int)doll::Slot::Plume || s == (int)doll::Slot::Device;
    if (legSlot && (b - 2) / 5 == fxSlot) a.c[k] = (uint32_t)(2 + s * 5 + (b - 2) % 5);
  }
}
struct PropHide {   // clears the bodyMask of the posture's prop parts but `keep` for one compose (restored on scope exit)
  std::vector<std::pair<int, uint8_t>> saved;
  PropHide(const std::vector<int>& props, int keep) {
    P::Pool& PP = const_cast<P::Pool&>(P::get());   // the pool's storage is a mutable static (doll_pool.cpp slot())
    for (int pi : props) if (pi != keep) { saved.push_back({pi, PP.parts[(size_t)pi].bodyMask}); PP.parts[(size_t)pi].bodyMask = 0; }
  }
  ~PropHide() { P::Pool& PP = const_cast<P::Pool&>(P::get()); for (auto& [pi, m] : saved) PP.parts[(size_t)pi].bodyMask = m; }
};
void shootPosture(LookFrames& out, const doll::DollRecipe& full, const doll::DollRecipe& prb, const PostureDef& pd) {
  const P::Pool& PP = P::get();
  const doll::DollRecipe rp = doll::withPosture(full, pd.p, pd.v);
  std::vector<int> props;
  if (const char* prop = doll::postureProp(pd.p, pd.v)) props = PP.partsWithRole(doll::Role::Held, prop);
  const doll::DollRecipe pP = doll::withPosture(prb, pd.p, pd.v), pFx = probedFx(pP);
  const RigSheet sFig = doll::composePosture(rp, pd.p, pd.v);
  const RigSheet sRaw = doll::composePosture(pP, pd.p, pd.v), sRawFx = doll::composePosture(pFx, pd.p, pd.v);
  std::vector<RigSheet> sPart(rp.parts.size()), sPartFx(rp.parts.size());
  for (size_t k = 0; k < rp.parts.size(); k++) {
    const PropHide hide(props, rp.parts[k]);
    doll::DollRecipe one = pP, oneFx = pFx;
    one.parts = oneFx.parts = {rp.parts[k]};
    sPart[k] = doll::composePosture(one, pd.p, pd.v);
    sPartFx[k] = doll::composePosture(oneFx, pd.p, pd.v);
  }
  const std::vector<const P::ClipRec*> clips = P::postureClips(rp.bodyKey, rp.family, (int)pd.p, pd.v);
  for (int fc : {2, 3}) {
    int i = 0;
    std::vector<int> idx(rp.parts.size(), 0);
    for (const P::ClipRec* cr : clips) {
      if (cr->facing != fc) continue;
      for (int n = 0; n < (int)cr->nFrames; n++, i++) {
        const P::FrameRec& f = PP.frames[cr->firstFrame + n];
        Frame F;
        F.gait = pd.name; F.facing = fc; F.i = i;
        F.plant0 = f.plant0; F.plant1 = f.plant1;
        F.bob = f.bob; F.lean = f.lean; F.hipX = f.hipX; F.hipY = f.hipY; F.headX = f.headX; F.headY = f.headY;
        for (const P::KeyRec& kk : f.keys) F.keys[PP.s(kk.channel)] = PP.s(kk.key);
        F.fin = cutImg(sFig, sFig.frame(fc, Clip::Idle, 0, i), true);
        F.raw = cutImg(sRaw, sRaw.frame(fc, Clip::Idle, 0, i), false);
        unfinishLegs(F.raw, cutImg(sRawFx, sRawFx.frame(fc, Clip::Idle, 0, i), false));
        for (size_t k = 0; k < rp.parts.size(); k++) {
          if (!partDraws(rp, rp.parts[k], fc, f)) continue;
          Img m = cutImg(sPart[k], sPart[k].frame(fc, Clip::Idle, 0, idx[k]), false);
          unfinishLegs(m, cutImg(sPartFx[k], sPartFx[k].frame(fc, Clip::Idle, 0, idx[k]), false));
          F.parts.emplace_back(doll::partName(rp.parts[k]), std::move(m));
          idx[k]++;
        }
        out.frames.push_back(std::move(F));
      }
    }
  }
}
}  // namespace

// ---------------------------------------------------------------- looks and shooting
std::vector<Look> readLooks(const char* file) {
  std::vector<Look> v;
  FILE* f = std::fopen(file, "r");
  if (!f) { std::printf("FAIL: looks file %s: cannot read\n", file); return v; }
  char line[1024];
  while (std::fgets(line, sizeof line, f)) {
    char tag[128] = {}, who[16] = {}, spec[700] = {}, gaits[128] = {};
    if (line[0] == '#' || std::sscanf(line, "%127s %15s %699s %127s", tag, who, spec, gaits) < 2) continue;
    Look l;
    l.tag = tag; l.who = !std::strcmp(who, "woman") ? 1 : 0; l.spec = std::strcmp(spec, "-") && spec[0] ? spec : "";
    std::string g = gaits[0] ? gaits : "walk,run";
    size_t a = 0;
    while (a < g.size()) {
      size_t b = g.find(',', a);
      if (b == std::string::npos) b = g.size();
      if (b > a) l.gaits.push_back(g.substr(a, b - a));
      a = b + 1;
    }
    v.push_back(l);
  }
  std::fclose(f);
  return v;
}

LookFrames shoot(const Look& l, const char* poolFile) {
  if (poolFile && g_poolLoaded != poolFile) {
    std::string err;
    if (!doll::loadPoolFile(poolFile, &err)) std::printf("FAIL: pool %s: %s\n", poolFile, err.c_str());
    else g_poolLoaded = poolFile;
  }
  LookFrames out;
  out.look = l;
  const LookOpts o = lookOf(l);
  doll::DollRecipe full = doll::resolve(o.L);
  full.ramps = o.lookPal ? doll::rampsOf(o.L) : doll::a3Palette(o.L.body == 1 ? 1 : 0);
  if (o.bare) for (doll::Region g : {doll::Region::Thigh, doll::Region::Knee, doll::Region::Shin}) full.region[(int)g] = (uint8_t)doll::Slot::Skin;
  out.recipe = full;
  const doll::DollRecipe prb = probed(full);
  const P::Pool& PP = P::get();
  for (const GaitDef& g : kGaitDefs) {
    const bool idle = g.clip == Clip::Idle;
    if (!idle && std::find(l.gaits.begin(), l.gaits.end(), g.name) == l.gaits.end()) continue;
    for (int fc : {2, 3}) {
      const P::ClipRec* cr = clipOf(full, g.part, fc, g.clip, g.variant);
      if (!cr) continue;
      doll::DollRecipe flagged = full;
      if (g.part == RigPart::Run) flagged.flags.push_back("run");
      doll::ComposeOpts raw; raw.finish = false; raw.trim = false; raw.onlyFacing = fc;
      doll::ComposeOpts fin; fin.finish = true; fin.trim = false; fin.onlyFacing = fc;
      const RigSheet sFig = doll::compose(full, g.part, &fin);
      const RigSheet sRaw = doll::compose(prb, g.part, &raw);
      std::vector<RigSheet> sPart(full.parts.size());
      for (size_t k = 0; k < full.parts.size(); k++) {
        doll::DollRecipe one = prb;
        one.parts = {full.parts[k]};
        sPart[k] = doll::compose(one, g.part, &raw);
      }
      const int n = idle ? 1 : (int)cr->nFrames;
      std::vector<int> idx(full.parts.size(), 0);
      for (int i = 0; i < (int)cr->nFrames; i++) {
        const P::FrameRec& f = PP.frames[cr->firstFrame + i];
        if (i < n) {
          Frame F;
          F.gait = g.name; F.facing = fc; F.i = i;
          F.plant0 = f.plant0; F.plant1 = f.plant1;
          F.bob = f.bob; F.lean = f.lean; F.hipX = f.hipX; F.hipY = f.hipY; F.headX = f.headX; F.headY = f.headY;
          for (const P::KeyRec& kk : f.keys) F.keys[PP.s(kk.channel)] = PP.s(kk.key);
          F.fin = cutImg(sFig, sFig.frame(fc, g.clip, g.variant, i), true);
          F.raw = cutImg(sRaw, sRaw.frame(fc, g.clip, g.variant, i), false);
          for (size_t k = 0; k < full.parts.size(); k++) {
            if (!partDraws(flagged, full.parts[k], fc, f)) continue;
            F.parts.emplace_back(doll::partName(full.parts[k]), cutImg(sPart[k], sPart[k].frame(fc, g.clip, g.variant, idx[k]), false));
          }
          (idle ? out.idle : out.frames).push_back(std::move(F));
        }
        for (size_t k = 0; k < full.parts.size(); k++) if (partDraws(flagged, full.parts[k], fc, f)) idx[k]++;
      }
    }
  }
  // (gates-wear fix round 1) the walking postures (5.5 M6: Carry 0-2, Play, Dance; the child's Play and Carry): their side
  // frames, shot like the walk's so B11 limbs and B12 judge their legs (leg checks only: judgeLook drops REG BOB RHYTHM POST)
  for (const PostureDef& pd : kPostureDefs) {
    if (std::find(l.gaits.begin(), l.gaits.end(), pd.name) == l.gaits.end()) continue;
    shootPosture(out, full, prb, pd);
  }
  return out;
}

// ---------------------------------------------------------------- the check.py port
namespace cp {
using K = std::pair<int, int>;   // (u, y)
using PX = std::map<K, int>;     // (u, y) -> probe code
using PS = std::set<K>;
struct Pt { double u = 0, y = 0; };
double dist(Pt a, Pt b) { return std::hypot(a.u - b.u, a.y - b.y); }
double dist(K a, Pt b) { return std::hypot(a.first - b.u, a.second - b.y); }
int pyround(double v) {   // Python's round(): half to even
  const double f = std::floor(v), d = v - f;
  if (d > 0.5) return (int)f + 1;
  if (d < 0.5) return (int)f;
  return ((long long)f % 2 == 0) ? (int)f : (int)f + 1;
}
int floordiv(int a, int b) { int q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) q--; return q; }
constexpr double kPi = 3.14159265358979323846;
double deg(double r) { return r * 180.0 / kPi; }
double rad(double d) { return d * kPi / 180.0; }

int stepOf(int c) { return ((c - 2) % 5 + 5) % 5; }
int slotOf(int c) { return floordiv(c - 2, 5); }
char reg(int c) {
  switch (slotOf(c)) {
    case 19: return 'T';
    case 17: return 'K';
    case 20: return 'S';
    case 16: return 'B';
    case 18: return 'O';
    default: return '?';
  }
}
bool shaft(int c) { const char r = reg(c); return r == 'T' || r == 'K' || r == 'S'; }
bool bootc(int c) { const char r = reg(c); return r == 'B' || r == 'O'; }
bool legc(int c) { return shaft(c) || bootc(c); }
int codeOf(char region, int st) {
  const int s = region == 'T' ? 19 : region == 'K' ? 17 : region == 'S' ? 20 : region == 'B' ? 16 : 18;
  return 2 + s * 5 + st;
}

// the per-body table (5.2): adult = check.py's constants; the kin start there ("set by the slice"), the child's THICK 3.3
struct BodyT {
  const char* name;
  double thick;          // THICK_BAR
  int ankleMin;          // ANKLE_MIN
  double ankleAcross;    // ANKLE_ACROSS
  int flightLift;        // FLIGHT_LIFT
  double flightBend, pushLean, foldBend, postVert, postStraight, postLead;
  int foldRows, hipRows;
  const char* stamps;    // the boot stamp library
  bool run;              // the body runs (the child: walk only)
  double area;           // AREA: visible leg mass / the idle's (adult 89.5%; the child's idle feet are one 9 px double boot
                         // over 5-row legs, so the walk's overlapping single boots read 85% with nothing lost, and under the
                         // poncho the idle's fringe shows the thighs through its strands where the walk hem closes them (its pass 78%): 75%)
};
const BodyT kBodies[3] = {
    {"adult", 3.8, 4, 3.5, 2, 40.0, 10.0, 25.0, 4.0, 15.0, 25.0, 2, 3, "boots_adult", true, 0.895},
    {"youth", 3.8, 4, 3.5, 2, 40.0, 10.0, 25.0, 4.0, 15.0, 25.0, 2, 3, "boots_youth", true, 0.895},
    {"child", 3.3, 4, 3.5, 2, 40.0, 10.0, 25.0, 4.0, 15.0, 25.0, 2, 3, "boots_child", false, 0.75},
};
constexpr double LEN_TOL = 1.0, LEN_SHORT = 1.5, BOOT_SHOW = 0.60, BOOT_TOL = 0.10, BOOT_T_TOL = 0.5;
constexpr int SLIVER = 3, RHYTHM_MAX = 2;
constexpr double TILT_MAX = 60.0, TILT_T_TOL = 0.30, BOOTSIZE_TOL = 0.10;
const int CROP[4] = {-26, -46, 26, 4};
const std::set<std::string> HEADL = {"head", "face", "hairFront", "ears", "headwear", "beard"};

// ---- the look as check.py reads its dump
struct JPart { std::string id, name, layer; int z = 0; std::vector<std::string> roles; };
struct Res { bool ok = true; std::string t; };
struct Results {
  std::vector<std::pair<std::string, Res>> v;   // insertion order (check.py's dict)
  void set(const std::string& c, bool ok, const std::string& t) {
    for (auto& e : v) if (e.first == c) { e.second = {ok, t}; return; }
    v.push_back({c, {ok, t}});
  }
};
struct Post { bool isp = false; bool hasLb = false; double lb = 0; std::string t; };
struct JFrame {
  std::string gait;
  char fac = 'R';
  int i = 0, bob = 0, lean = 0, hipX = 0, hipY = 0, headX = 0, headY = 0, hbx = 0, hby = -10;
  bool hp0 = false, hp1 = false;
  double plant0 = 0, plant1 = 0;
  std::map<std::string, std::string> keys;
  PX raw;
  std::vector<std::pair<std::string, PX>> layers;   // part id -> pixels, the recipe's order
  // judge's results
  PX lnear, lfar; std::set<int> lseat; std::string lmode;
  bool hasPost = false; Post post;
  Results res;
  const PX* layer(const std::string& id) const { for (auto& e : layers) if (e.first == id) return &e.second; return nullptr; }
  PX* layer(const std::string& id) { for (auto& e : layers) if (e.first == id) return &e.second; return nullptr; }
  void setLayer(const std::string& id, PX px) { if (PX* p = layer(id)) *p = std::move(px); else layers.push_back({id, std::move(px)}); }
  void eraseLayer(const std::string& id) { layers.erase(std::remove_if(layers.begin(), layers.end(), [&](auto& e) { return e.first == id; }), layers.end()); }
};
struct JLook {
  std::string tag, bodyKey;
  std::vector<std::string> flags;
  std::vector<JPart> parts;
  std::vector<JFrame> frames;
  const BodyT* T = &kBodies[0];
  const JPart* part(const std::string& id) const { for (auto& p : parts) if (p.id == id) return &p; return nullptr; }
  int order(const std::string& id) const { for (size_t k = 0; k < parts.size(); k++) if (parts[k].id == id) return (int)k; return 1 << 20; }
};

PX toPX(const Img& m, bool left) {
  PX out;
  for (int y = 0; y < m.h; y++)
    for (int x = 0; x < m.w; x++) {
      const uint32_t c = m.c[(size_t)y * m.w + x];
      if (!c) continue;
      const int X = x - m.px, Y = y - m.py;
      if (X < CROP[0] || X >= CROP[2] || Y < CROP[1] || Y >= CROP[3]) continue;
      out[{left ? -X - 1 : X, Y}] = (int)(c & 0xFF);
    }
  return out;
}
PX cropped(const PX& px) {   // enc()/grid()'s round trip: only the crop survives
  PX out;
  for (auto& [k, c] : px) {
    if (!c) continue;
    if (k.second < CROP[1] || k.second >= CROP[3]) continue;
    // u -> x: R x = u, L x = -u - 1; the crop's x range is symmetric about -0.5 so both facings keep u in [-26, 26)
    if (k.first < CROP[0] || k.first >= CROP[2]) continue;
    out[k] = c;
  }
  return out;
}

const BodyT* bodyTable(const doll::DollRecipe& r) {
  if (r.body == doll::BodySet::Youth) return &kBodies[1];
  if (r.body == doll::BodySet::Child) return &kBodies[2];
  return &kBodies[0];
}

JLook toJLook(const LookFrames& f) {
  JLook L;
  const P::Pool& PP = P::get();
  L.tag = f.look.tag; L.bodyKey = f.recipe.bodyKey; L.flags = f.recipe.flags; L.T = bodyTable(f.recipe);
  for (int pi : f.recipe.parts) {
    const P::PartRec& p = PP.parts[(size_t)pi];
    JPart jp;
    jp.id = std::to_string(pi); jp.name = PP.s(p.name); jp.layer = doll::layerName((doll::Layer)p.layer); jp.z = (int)p.z;
    for (const auto& r : p.roles) jp.roles.push_back(std::string(doll::roleName((doll::Role)r.cat)) + " " + PP.s(r.value));
    L.parts.push_back(jp);
  }
  int hb[4][2] = {{0, -10}, {0, -10}, {0, -10}, {0, -10}};
  if (const P::BodyRec* b = P::bodyOf(f.recipe.bodyKey))
    for (int fc = 0; fc < 4; fc++)
      for (const P::AnchorRec& a : b->anchors[fc]) if (a.id == (uint8_t)doll::Anchor::Hip) { hb[fc][0] = a.x; hb[fc][1] = a.y; }
  auto add = [&](const Frame& F) {
    JFrame j;
    j.gait = F.gait; j.fac = F.facing == 3 ? 'L' : 'R'; j.i = F.i;
    j.bob = F.bob; j.lean = F.lean; j.hipX = F.hipX; j.hipY = F.hipY; j.headX = F.headX; j.headY = F.headY;
    j.hbx = hb[F.facing & 3][0]; j.hby = hb[F.facing & 3][1];
    j.hp0 = F.plant0 != P::kNoPlant; j.hp1 = F.plant1 != P::kNoPlant;
    j.plant0 = F.plant0 / 16.0; j.plant1 = F.plant1 / 16.0;
    j.keys = F.keys;
    const bool left = F.facing == 3;
    j.raw = toPX(F.raw, left);
    for (const auto& [name, img] : F.parts) {
      const int pi = doll::findPart(name);
      // (a walking posture's prop, added by withPosture, is a part of that frame's recipe only)
      if (pi >= 0 && !L.part(std::to_string(pi))) {
        const P::PartRec& p = PP.parts[(size_t)pi];
        JPart jp;
        jp.id = std::to_string(pi); jp.name = PP.s(p.name); jp.layer = doll::layerName((doll::Layer)p.layer); jp.z = (int)p.z;
        for (const auto& r : p.roles) jp.roles.push_back(std::string(doll::roleName((doll::Role)r.cat)) + " " + PP.s(r.value));
        L.parts.push_back(jp);
      }
      j.layers.push_back({std::to_string(pi), toPX(img, left)});
    }
    L.frames.push_back(std::move(j));
  };
  for (const Frame& F : f.idle) add(F);
  for (const Frame& F : f.frames) add(F);
  return L;
}

const JFrame* frameOf(const JLook& L, const std::string& g, char fac, int i) {
  for (const JFrame& f : L.frames) if (f.gait == g && f.fac == fac && f.i == i) return &f;
  return nullptr;
}
JFrame* frameOf(JLook& L, const std::string& g, char fac, int i) {
  for (JFrame& f : L.frames) if (f.gait == g && f.fac == fac && f.i == i) return &f;
  return nullptr;
}

// ---- the legs: near and far
bool hasRoleSuffix(const JPart& p, const char* suf) {
  const size_t n = std::strlen(suf);
  for (const std::string& r : p.roles) if (r.size() >= n && r.compare(r.size() - n, n, suf) == 0) return true;
  return false;
}
bool hasRolePrefix(const JPart& p, const char* pre) {
  for (const std::string& r : p.roles) if (r.rfind(pre, 0) == 0) return true;
  return false;
}
std::vector<std::string> legParts(const JLook& L, const JFrame& f) {
  std::vector<std::string> ids;
  for (auto& [id, px] : f.layers) {
    const JPart* p = L.part(id);
    if (p && p->layer == "legs" && !hasRoleSuffix(*p, " tips")) ids.push_back(id);
  }
  std::stable_sort(ids.begin(), ids.end(), [&](const std::string& a, const std::string& b) {
    const JPart *pa = L.part(a), *pb = L.part(b);
    return std::make_pair(pa->z, L.order(a)) < std::make_pair(pb->z, L.order(b));
  });
  return ids;
}

using Row = std::vector<std::pair<int, int>>;   // sorted (u, code)
std::vector<Row> segments(const Row& row, bool tonesplit = false) {
  std::vector<Row> runs;
  Row cur;
  for (auto& e : row) {
    if (!cur.empty() && e.first != cur.back().first + 1) { runs.push_back(cur); cur.clear(); }
    cur.push_back(e);
  }
  if (!cur.empty()) runs.push_back(cur);
  std::vector<Row> segs;
  for (const Row& run : runs) {
    std::vector<int> inter;
    for (int n = 0; n < (int)run.size(); n++) if (stepOf(run[n].second) != 0 && reg(run[n].second) != 'O') inter.push_back(n);
    if (inter.empty()) { segs.push_back(run); continue; }
    std::vector<std::pair<int, int>> stretches;
    int a = inter[0];
    for (size_t k = 0; k + 1 < inter.size(); k++)
      if (inter[k + 1] != inter[k] + 1) { stretches.push_back({a, inter[k]}); a = inter[k + 1]; }
    stretches.push_back({a, inter.back()});
    if (tonesplit) {
      std::vector<std::pair<int, int>> st2;
      for (auto [a0, a1] : stretches) {
        std::vector<bool> dk;
        for (int n = a0; n <= a1; n++) dk.push_back(stepOf(run[n].second) == 1);
        int cut = 0;
        for (int m = 2; m < (int)dk.size() - 1; m++) {
          bool ok = true;
          for (int q = 0; q < m && ok; q++) ok = dk[q] == dk[0];
          for (int q = m; q < (int)dk.size() && ok; q++) ok = dk[q] != dk[0];
          if (ok) cut = m;
        }
        bool allShaft = true;
        for (int n = a0; n <= a1; n++) allShaft = allShaft && shaft(run[n].second);
        if (cut && allShaft) { st2.push_back({a0, a0 + cut - 1}); st2.push_back({a0 + cut, a1}); }
        else st2.push_back({a0, a1});
      }
      stretches = st2;
    }
    if (stretches.size() == 1) { segs.push_back(run); continue; }
    std::vector<std::pair<int, int>> cuts;
    for (size_t k = 0; k + 1 < stretches.size(); k++) {
      const int a1 = stretches[k].second, b0 = stretches[k + 1].first;
      const int gap = b0 - a1 - 1;
      if (gap <= 0) cuts.push_back({a1, b0});
      else if (gap == 1) cuts.push_back({a1 + 1, a1 + 1});
      else { const int h = a1 + gap / 2; cuts.push_back({h, h + 1}); }
    }
    int lo = 0;
    for (auto [e, st] : cuts) { segs.push_back(Row(run.begin() + lo, run.begin() + e + 1)); lo = st; }
    segs.push_back(Row(run.begin() + lo, run.end()));
  }
  return segs;
}
struct Tone { int lit = 0, dark = 0; bool has = false; double mean = 0; };
Tone tone(const Row& seg) {
  Tone t;
  double s = 0; int n = 0;
  for (auto& [u, c] : seg) {
    const int st = stepOf(c);
    if (st == 0 || !shaft(c)) continue;
    s += st; n++;
    if (st == 1) t.dark++; else t.lit++;
  }
  if (n) { t.has = true; t.mean = s / n; }
  return t;
}
std::map<int, Row> rowsOfPX(const PX& px) {
  std::map<int, Row> rows;
  for (auto& [k, c] : px) rows[k.second].push_back({k.first, c});
  for (auto& [y, r] : rows) std::sort(r.begin(), r.end());
  return rows;
}
std::set<int> seatRows(const PX& px, std::map<int, Row>& rows) {
  rows = rowsOfPX(px);
  std::set<int> seat;
  if (rows.empty()) return seat;
  std::vector<int> ys;
  for (auto& [y, r] : rows) ys.push_back(y);
  seat.insert(ys[0]);
  for (size_t k = 1; k < ys.size() && k < 3; k++) {
    const int y = ys[k];
    const std::vector<Row> ss = segments(rows[y], true);
    if (ss.size() == 1 && ss[0].size() >= 7 && seat.count(y - 1)) seat.insert(y);
    else break;
  }
  return seat;
}

// one drawing of both legs -> (near, far, seat rows): check.py split_palette (the kin's one-part legs)
void splitPalette(const PX& px, PX& near, PX& far, std::set<int>& seat) {
  std::map<int, Row> rows;
  seat = seatRows(px, rows);
  near.clear(); far.clear();
  if (rows.empty()) return;
  std::vector<int> ys;
  for (auto& [y, r] : rows) ys.push_back(y);
  std::map<int, std::vector<Row>> segs;
  std::map<std::pair<int, int>, char> cls;   // 0: None
  int underSeat = 1 << 20;
  bool hasUnder = false;
  for (int y : ys) if (!seat.count(y)) { underSeat = y; hasUnder = true; break; }
  for (int y : ys) {
    if (seat.count(y)) continue;
    Row sh;
    for (auto& e : rows[y]) if (shaft(e.second)) sh.push_back(e);
    segs[y] = segments(sh, true);
    if (hasUnder && y == underSeat) { for (size_t n = 0; n < segs[y].size(); n++) cls[{y, (int)n}] = 0; continue; }
    std::vector<Tone> tones;
    for (const Row& sg : segs[y]) tones.push_back(tone(sg));
    for (size_t n = 0; n < segs[y].size(); n++) {
      const Tone& t = tones[n];
      char k = 0;
      std::vector<double> others;
      for (size_t j = 0; j < tones.size(); j++) if (j != n && tones[j].has) others.push_back(tones[j].mean);
      if (t.has && !others.empty()) {
        const double mo = *std::max_element(others.begin(), others.end());
        const double best = std::max(t.mean, mo);
        if (t.mean == best && t.mean > mo + 0.25) k = 'N';
        else if (t.mean < best - 0.25) k = 'F';
      }
      if (!k) { if (t.lit > t.dark) k = 'N'; else if (t.dark > t.lit) k = 'F'; }
      cls[{y, (int)n}] = k;
    }
  }
  for (int it = 0; it < 6; it++)
    for (auto yit = segs.rbegin(); yit != segs.rend(); ++yit) {
      const int y = yit->first;
      for (size_t n = 0; n < yit->second.size(); n++) {
        if (cls[{y, (int)n}]) continue;
        std::set<int> us;
        for (auto& e : yit->second[n]) us.insert(e.first);
        for (int yy : {y + 1, y - 1}) {
          int bestN = 0, bestM = -1;
          auto sit = segs.find(yy);
          if (sit != segs.end())
            for (size_t m = 0; m < sit->second.size(); m++) {
              auto ci = cls.find({yy, (int)m});
              if (ci == cls.end() || !ci->second) continue;
              int ov = 0;
              for (auto& e : sit->second[m]) ov += us.count(e.first) ? 1 : 0;
              if (ov > 0 && ov >= bestN) { bestN = ov; bestM = (int)m; }   // (max of (overlap, m): ties to the later segment)
            }
          if (bestN) { cls[{y, (int)n}] = cls[{yy, bestM}]; break; }
        }
      }
    }
  for (int y : ys) {
    if (seat.count(y)) { for (auto& e : rows[y]) { near[{e.first, y}] = e.second; far[{e.first, y}] = e.second; } continue; }
    for (size_t n = 0; n < segs[y].size(); n++) {
      const char k = cls[{y, (int)n}] ? cls[{y, (int)n}] : 'N';
      for (auto& e : segs[y][n]) (k == 'N' ? near : far)[{e.first, y}] = e.second;
    }
  }
  // the hip fill (the pelvis' dark rounding, no far leg)
  PS fsh;
  for (auto& [k, c] : far) if (shaft(c) && !seat.count(k.second)) fsh.insert(k);
  std::vector<PS> comps;
  PS seen;
  for (const K& k : fsh) {
    if (seen.count(k)) continue;
    PS comp; std::vector<K> st{k}; seen.insert(k);
    while (!st.empty()) {
      K q = st.back(); st.pop_back(); comp.insert(q);
      for (K n : {K{q.first + 1, q.second}, K{q.first - 1, q.second}, K{q.first, q.second + 1}, K{q.first, q.second - 1}})
        if (fsh.count(n) && !seen.count(n)) { seen.insert(n); st.push_back(n); }
    }
    comps.push_back(comp);
  }
  if (comps.size() >= 2 && hasUnder) {
    size_t mi = 0;
    auto keyOf = [](const PS& c) { int my = -(1 << 20); for (auto& k : c) my = std::max(my, k.second); return std::make_pair(my, (int)c.size()); };
    for (size_t k = 1; k < comps.size(); k++) if (keyOf(comps[k]) > keyOf(comps[mi])) mi = k;
    double mu = 0;
    for (auto& k : comps[mi]) mu += k.first;
    mu /= comps[mi].size();
    for (size_t ci = 0; ci < comps.size(); ci++) {
      if (ci == mi || keyOf(comps[ci]).first > underSeat + 2) continue;
      std::set<int> rowsc;
      for (auto& k : comps[ci]) rowsc.insert(k.second);
      std::vector<int> nu;
      for (auto& [k, c] : near) if (rowsc.count(k.second) && shaft(c) && !far.count(k)) nu.push_back(k.first);
      if (nu.empty()) continue;
      double nc = 0, pu = 0;
      for (int u : nu) nc += u;
      nc /= nu.size();
      for (auto& k : comps[ci]) pu += k.first;
      pu /= comps[ci].size();
      if ((pu - nc) * (mu - nc) < 0) for (auto& k : comps[ci]) far.erase(k);
    }
  }
  // the boots: a flood from each leg's shin through the boot pixels
  PX boot;
  for (auto& [k, c] : px) if (!shaft(c) && !seat.count(k.second)) boot[k] = c;
  std::map<K, char> owner;
  std::set<K> fN, fF;
  for (auto& [k, c] : near) if (shaft(c) && !seat.count(k.second)) fN.insert(k);
  for (auto& [k, c] : far) if (shaft(c) && !seat.count(k.second)) fF.insert(k);
  while (!fN.empty() || !fF.empty()) {
    std::map<K, std::pair<int, int>> claim;   // N count, F count
    for (int tg = 0; tg < 2; tg++)
      for (const K& q0 : tg ? fF : fN)
        for (int du = -1; du <= 1; du++)
          for (int dy = -1; dy <= 1; dy++) {
            if (!du && !dy) continue;
            const K q{q0.first + du, q0.second + dy};
            if (boot.count(q) && !owner.count(q)) (tg ? claim[q].second : claim[q].first)++;
          }
    std::set<K> nN, nF;
    for (auto& [q, d] : claim) {
      const char t = d.first >= d.second ? 'N' : 'F';
      owner[q] = t;
      (t == 'N' ? nN : nF).insert(q);
    }
    fN = nN; fF = nF;
  }
  std::vector<K> rest;
  for (auto& [k, c] : boot) if (!owner.count(k)) rest.push_back(k);
  if (!rest.empty()) {
    bool lit = false;
    for (const K& k : rest) lit = lit || stepOf(boot[k]) >= 3;
    for (const K& k : rest) owner[k] = lit ? 'N' : 'F';
  }
  for (auto& [k, t] : owner) (t == 'N' ? near : far)[k] = boot[k];
}

// ---- the maps: boot stamps and the leg views' puts (check.py parse_stamps / parse_pxm)
std::string g_maps = "rpg/art/doll/maps";       // check.py's MAPROOT (CHECK_MAPS / --maps)
const std::string kRootMaps = "rpg/art/doll/maps";   // check.py's ROOT maps (the stamp library near_boot and --synth read)
struct Stamp { int ox = 0, oy = 0; std::vector<K> cellsV; std::set<K> cells; std::map<K, char> ch, rg; };
using StampLib = std::map<std::pair<std::string, std::string>, Stamp>;   // (name, facing)
std::map<std::string, StampLib> g_stamps;
bool readLines(const std::string& fn, std::vector<std::string>& out) {
  std::ifstream in(fn);
  if (!in) return false;
  std::string s;
  while (std::getline(in, s)) { if (!s.empty() && s.back() == '\r') s.pop_back(); out.push_back(s); }
  return true;
}
std::vector<std::string> splitWs(const std::string& s) {
  std::istringstream is(s);
  std::vector<std::string> t;
  std::string w;
  while (is >> w) t.push_back(w);
  return t;
}
std::string gridPart(const std::string& line) {   // line[1:].split('|')[0]
  const std::string r = line.substr(1);
  const size_t b = r.find('|');
  return b == std::string::npos ? r : r.substr(0, b);
}
const StampLib& parseStamps(const std::string& root, const std::string& lib) {
  const std::string key = root + "|" + lib;
  auto it = g_stamps.find(key);
  if (it != g_stamps.end()) return it->second;
  StampLib out;
  std::vector<std::string> lines;
  readLines(root + "/stamps/" + lib + ".stamps", lines);
  Stamp* cur = nullptr;
  bool gridOn = false;
  int cy = 0, ry = 0;
  for (const std::string& line : lines) {
    if (line.rfind("stamp ", 0) == 0) {
      const auto t = splitWs(line);
      if (t.size() < 6) continue;
      Stamp& s = out[{t[1], t[2]}];
      s = Stamp();
      s.ox = std::atoi(t[4].c_str()); s.oy = std::atoi(t[5].c_str());
      cur = &s; gridOn = true; cy = 0;
    } else if (line.rfind("regions", 0) == 0) { gridOn = false; ry = 0; }
    else if (!line.empty() && line[0] == '|' && cur && gridOn) {
      const std::string g = gridPart(line);
      for (int cx = 0; cx < (int)g.size(); cx++)
        if (g[cx] != '.' && g[cx] != ' ') { if (!cur->cells.count({cx, cy})) cur->cellsV.push_back({cx, cy}); cur->cells.insert({cx, cy}); cur->ch[{cx, cy}] = g[cx]; }
      cy++;
    } else if (!line.empty() && line[0] == '|' && cur) {
      const std::string g = gridPart(line);
      for (int cx = 0; cx < (int)g.size(); cx++) if (g[cx] != '.' && g[cx] != ' ') cur->rg[{cx, ry}] = g[cx];
      ry++;
    }
  }
  return g_stamps[key] = out;
}
struct Put { std::string name; int dx = 0, dy = 0; bool under = false; bool hasCols = false; int c0 = 0, c1 = 0; };
struct PView { std::string facing, key, when; bool replace = false, hasO = false; int ox = 0, oy = 0; std::vector<Put> puts; std::set<K> cells; int gy = 0; bool grid = true; };
struct Pxm { std::vector<PView> views; std::vector<std::string> libs; };
std::map<std::string, Pxm> g_pxm;
const Pxm& parsePxm(const std::string& name) {
  const std::string key = g_maps + "|" + name;
  auto it = g_pxm.find(key);
  if (it != g_pxm.end()) return it->second;
  Pxm out;
  std::vector<std::string> lines;
  if (readLines(g_maps + "/" + name + ".pxm", lines)) {
    PView* cur = nullptr;
    for (const std::string& line : lines) {
      if (!line.empty() && line[0] == '|' && cur && cur->grid) {
        const std::string g = gridPart(line);
        for (int cx = 0; cx < (int)g.size(); cx++) if (g[cx] != '.' && g[cx] != ' ' && g[cx] != '-') cur->cells.insert({cx, cur->gy});
        cur->gy++;
        continue;
      }
      const auto t = splitWs(line);
      if (t.empty() || t[0][0] == '#' || t[0][0] == '|') continue;
      if (t[0] == "stamps") out.libs.insert(out.libs.end(), t.begin() + 1, t.end());
      else if (t[0] == "view") {
        PView v;
        v.facing = t.size() > 1 ? t[1] : "";
        v.key = "rest";
        // (Python: the dict comprehension keeps the last pair of each name)
        for (size_t k = 2; k + 1 < t.size(); k++) { if (t[k] == "key") v.key = t[k + 1]; if (t[k] == "when") v.when = t[k + 1]; }
        v.replace = std::find(t.begin(), t.end(), "replace") != t.end();
        const size_t org = (size_t)(std::find(t.begin(), t.end(), "origin") - t.begin());
        if (org + 2 < t.size()) { v.hasO = true; v.ox = std::atoi(t[org + 1].c_str()); v.oy = std::atoi(t[org + 2].c_str()); }
        out.views.push_back(v);
        cur = &out.views.back();
      } else if (t[0] == "regions" || t[0] == "build") {
        if (cur) cur->grid = false;
      } else if (t[0] == "put" && cur && t.size() >= 4) {
        Put p;
        p.name = t[1]; p.dx = std::atoi(t[2].c_str()); p.dy = std::atoi(t[3].c_str());
        p.under = std::find(t.begin(), t.end(), "under") != t.end();
        auto c = std::find(t.begin(), t.end(), "cols");
        if (c != t.end() && c + 1 != t.end()) {
          const size_t d = (c + 1)->find('-');
          if (d != std::string::npos) { p.hasCols = true; p.c0 = std::atoi((c + 1)->substr(0, d).c_str()); p.c1 = std::atoi((c + 1)->substr(d + 1).c_str()); }
        }
        cur->puts.push_back(p);
      }
    }
  }
  return g_pxm[key] = out;
}
bool condHoldsFlags(const std::string& cond, const std::vector<std::string>& flags) {
  size_t a = 0;
  while (a <= cond.size()) {
    size_t b = cond.find(',', a);
    if (b == std::string::npos) b = cond.size();
    std::string c = cond.substr(a, b - a);
    const bool neg = !c.empty() && c[0] == '!';
    while (!c.empty() && c[0] == '!') c = c.substr(1);
    if (!c.empty() && ((std::find(flags.begin(), flags.end(), c) != flags.end()) == neg)) return false;
    a = b + 1;
  }
  return true;
}
int stepOfCh(char ch) {   // person.legend: leather f-j 0-4, ink K 0; -1 none
  switch (ch) { case 'f': return 0; case 'g': return 1; case 'h': return 2; case 'i': return 3; case 'j': return 4; case 'K': return 0; default: return -1; }
}
// {(u, y): 'N'|'F'} for the cells the view's boot puts cover, plus the view's whole shape; false when none
bool bootOwner(const JLook& L, const JFrame& f, const std::string& pid, std::map<K, char>& own, PS& shape) {
  own.clear(); shape.clear();
  const JPart* jp = L.part(pid);
  if (!jp) return false;
  const Pxm& pm = parsePxm(jp->name);
  auto ki = f.keys.find("legs");
  if (ki == f.keys.end()) return false;
  const std::string key = ki->second, fac(1, f.fac);
  const PView* base = nullptr;
  for (const PView& v : pm.views) if (v.facing == fac && v.key == key && v.when.empty()) { base = &v; break; }
  PView merged;
  for (const PView& v : pm.views)
    if (v.facing == fac && v.key == key && !v.when.empty() && v.replace && condHoldsFlags(v.when, L.flags)) {
      merged = v;
      if (!merged.hasO && base && base->hasO) { merged.hasO = true; merged.ox = base->ox; merged.oy = base->oy; }
      base = &merged;
      break;
    }
  if (!base || base->puts.empty() || !base->hasO) return false;
  StampLib st;
  for (const std::string& lib : pm.libs) for (auto& [k, v] : parseStamps(g_maps, lib)) st[k] = v;
  const bool left = f.fac == 'L';
  for (const K& c : base->cells) shape.insert({left ? -(c.first - base->ox) - 1 : c.first - base->ox, c.second - base->oy});
  // (integration 2026-10-23, as check.py) an additive `when` overlay that holds (the elf's seat row) is the drawing's too
  for (const PView& v : pm.views)
    if (v.facing == fac && v.key == key && !v.when.empty() && !v.replace && condHoldsFlags(v.when, L.flags)) {
      const int ox = v.hasO ? v.ox : base->ox, oy = v.hasO ? v.oy : base->oy;
      for (const K& c : v.cells) shape.insert({left ? -(c.first - ox) - 1 : c.first - ox, c.second - oy});
    }
  for (int under = 0; under < 2; under++)
    for (const Put& p : base->puts) {
      if ((int)p.under != under) continue;
      auto si = st.find({p.name, fac});
      if (si == st.end()) continue;
      const char tag = !p.name.empty() && p.name.back() == 'n' ? 'N' : !p.name.empty() && p.name.back() == 'f' ? 'F' : 0;
      if (!tag) continue;
      const Stamp& sp = si->second;
      for (const K& c : sp.cells) {
        if (p.hasCols && !(p.c0 <= c.first && c.first <= p.c1)) continue;
        const int x = p.dx + c.first - sp.ox, y = p.dy + c.second - sp.oy;
        const K q{left ? -x - 1 : x, y};
        if (!own.count(q)) own[q] = tag;
        shape.insert(q);
      }
    }
  return true;
}
PS keysOf(const PX& px) { PS s; for (auto& [k, c] : px) s.insert(k); return s; }
// (proto6 i1) each leg's OWN boot stamp cells its own shaft covers (layers mode only)
std::pair<PS, PS> bootHiddenCells(const JLook& L, const JFrame& f, const PX& near, const PX& far, const std::string& mode) {
  std::pair<PS, PS> out;
  if (mode != "layers") return out;
  const std::vector<std::string> ids = legParts(L, f);
  for (int w = 0; w < 2; w++) {
    const PX& leg = w ? far : near;
    const char tag = w ? 'F' : 'N';
    std::vector<std::string> pids;
    if (w) pids.assign(ids.begin(), ids.end() - (ids.empty() ? 0 : 1));
    else if (!ids.empty()) pids.push_back(ids.back());
    for (const std::string& pid : pids) {
      std::map<K, char> own;
      PS shape;
      if (!bootOwner(L, f, pid, own, shape)) continue;
      const PX* lp = f.layer(pid);
      if (!lp || shape != keysOf(*lp)) {
        if (lp && std::getenv("BOOT_SHAPE_DEBUG")) {
          std::string a, b;
          for (const K& k : shape) if (!lp->count(k)) a += fmt(" %d,%d", k.first, k.second);
          for (auto& [k, c] : *lp) if (!shape.count(k)) b += fmt(" %d,%d", k.first, k.second);
          std::printf("  shape %s %c%d %s: maps-only%s | drawn-only%s\n", L.tag.c_str(), f.fac, f.i, pid.c_str(), a.c_str(), b.c_str());
        }
        continue;
      }
      for (auto& [k, t] : own) {
        if (t != tag) continue;
        auto li = leg.find(k);
        if (li != leg.end() && shaft(li->second)) (w ? out.second : out.first).insert(k);
      }
    }
  }
  return out;
}

// the near boot found whole in one drawing (check.py near_boot), else the stamp match (stamp_match)
StampLib rootLib() {
  StampLib lib;
  for (const char* n : {"boots_adult", "boots_child", "boots_youth"}) for (auto& [k, v] : parseStamps(kRootMaps, n)) lib[k] = v;
  return lib;
}
bool nearBoot(const PX& px, char facc, std::map<K, char>& out) {
  const StampLib lib = rootLib();
  const std::string fac(1, facc);
  std::map<K, std::pair<int, char>> boot;
  for (auto& [k, c] : px) if (bootc(c)) boot[k] = {stepOf(c), reg(c)};
  std::vector<K> lit;
  for (auto& [k, v] : boot) if (v.first == 3) lit.push_back(k);
  std::map<K, int> best;
  bool found = false;
  for (auto& [nk, sp] : lib) {
    if (nk.second != fac || nk.first.empty() || nk.first.back() != 'n' || sp.rg.empty()) continue;
    std::map<K, std::pair<int, char>> cells;
    for (auto& [c, ch] : sp.ch) { auto r = sp.rg.find(c); cells[c] = {stepOfCh(ch), r == sp.rg.end() ? 0 : r->second}; }
    std::vector<K> icells;
    for (auto& [c, v] : cells) if (v.first == 3) icells.push_back(c);
    std::vector<int> xs;
    for (auto& [c, v] : cells) xs.push_back(c.first);
    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
    for (const K& l : lit) {
      const int x = facc == 'L' ? -l.first - 1 : l.first, y = l.second;
      for (const K& ic : icells) {
        const int ox = x - ic.first, oy = y - ic.second;
        for (size_t kk = 0; kk <= xs.size(); kk++) {
          const bool none = kk == xs.size();
          const int ksplit = none ? 0 : xs[kk];
          for (int d : none ? std::vector<int>{0} : std::vector<int>{-2, -1, 1, 2}) {
            std::map<K, int> cov;
            bool ok = true;
            for (auto& [c, cc] : cells) {
              const bool other = !none && ((c.first >= ksplit) != (ic.first >= ksplit));
              const int X = ox + c.first, Y = oy + c.second + (other ? d : 0);
              const int U = facc == 'L' ? -X - 1 : X;
              auto bi = boot.find({U, Y});
              if (bi == boot.end() || bi->second != cc) { ok = false; break; }
              cov[{U, Y}] = 1;
            }
            if (ok && (!found || cov.size() > best.size())) { best = cov; found = true; }
          }
        }
      }
    }
  }
  if (!found || best.empty()) return false;
  out.clear();
  for (auto& [k, v] : boot) out[k] = best.count(k) ? 'N' : 'F';
  return true;
}
bool stampMatch(const PX& px, char facc, std::map<K, char>& own) {
  const StampLib lib = rootLib();
  const std::string fac(1, facc);
  std::map<K, std::pair<int, char>> boot;
  for (auto& [k, c] : px) if (bootc(c)) boot[k] = {stepOf(c), reg(c)};
  if (boot.empty()) return false;
  own.clear();
  for (char want : {'n', 'f'}) {
    std::map<K, std::pair<int, char>> freeB;
    for (auto& [k, v] : boot) if (!own.count(k)) freeB[k] = v;
    std::vector<std::pair<int, std::vector<K>>> found;
    for (auto& [nk, sp] : lib) {
      if (nk.second != fac || nk.first.empty() || nk.first.back() != want || sp.rg.empty()) continue;
      std::vector<std::pair<K, std::pair<int, char>>> cells;
      for (auto& [c, ch] : sp.ch) { auto r = sp.rg.find(c); cells.push_back({c, {stepOfCh(ch), r == sp.rg.end() ? 0 : r->second}}); }
      auto ti = lib.find({nk.first.substr(0, nk.first.size() - 1) + (want == 'n' ? "f" : "n"), nk.second});
      std::set<K> tell;
      for (auto& [c, ch] : sp.ch) {
        if (ti == lib.end()) { tell.insert(c); continue; }
        auto tc = ti->second.ch.find(c);
        if (tc == ti->second.ch.end() || tc->second != ch) tell.insert(c);
      }
      std::map<K, int> offs;
      for (auto& [k, code] : freeB) {
        const int x = facc == 'L' ? -k.first - 1 : k.first;
        for (auto& [c, cc] : cells) if (cc == code) offs[{x - c.first, k.second - c.second}]++;
      }
      for (auto& [o, n] : offs) {
        if (n < 3) continue;
        std::map<int, std::tuple<int, int, int>> col;
        for (auto& [c, cc] : cells) {
          const int x = o.first + c.first, y = o.second + c.second;
          auto fi = freeB.find({facc == 'L' ? -x - 1 : x, y});
          auto& e = col[c.first];
          if (fi != freeB.end()) {
            const bool hit = fi->second == cc;
            e = {std::get<0>(e) + hit, std::get<1>(e) + !hit, std::get<2>(e) + (hit && tell.count(c))};
          }
        }
        std::vector<int> run;
        std::vector<int> sx;
        for (auto& [c, e] : col) sx.push_back(c);
        for (size_t q = 0; q <= sx.size(); q++) {
          if (q < sx.size() && std::get<1>(col[sx[q]]) == 0) { run.push_back(sx[q]); continue; }
          int hits = 0, tl = 0;
          for (int c : run) { hits += std::get<0>(col[c]); tl += std::get<2>(col[c]); }
          if (hits >= 4 && tl >= 1) {
            std::vector<K> cov;
            for (auto& [c, cc] : cells)
              if (std::find(run.begin(), run.end(), c.first) != run.end()) cov.push_back({facc == 'L' ? -(o.first + c.first) - 1 : o.first + c.first, o.second + c.second});
            found.push_back({hits, cov});
          }
          run.clear();
        }
      }
    }
    std::stable_sort(found.begin(), found.end(), [](auto& a, auto& b) { return a.first > b.first; });
    for (auto& [h, cov] : found)
      for (const K& k : cov) if (freeB.count(k) && !own.count(k)) own[k] = want == 'n' ? 'N' : 'F';
  }
  if (own.empty()) return false;
  for (int it = 0; it < 3; it++)
    for (auto& [k, v] : boot) {
      if (own.count(k)) continue;
      std::map<char, int> nb;
      for (K q : {K{k.first + 1, k.second}, K{k.first - 1, k.second}, K{k.first, k.second + 1}, K{k.first, k.second - 1}}) {
        auto oi = own.find(q);
        if (oi != own.end()) nb[oi->second]++;
      }
      if (!nb.empty()) own[k] = (nb['N'] >= nb['F']) ? 'N' : 'F';
    }
  return true;
}

std::set<std::string> g_stale;
void legsOf(const JLook& L, const JFrame& f, PX& near, PX& far, std::set<int>& seat, std::string& mode) {
  near.clear(); far.clear(); seat.clear();
  const std::vector<std::string> ids = legParts(L, f);
  if (ids.size() >= 2) {
    for (size_t k = 0; k + 1 < ids.size(); k++) for (auto& [p, c] : *f.layer(ids[k])) far[p] = c;
    near = *f.layer(ids.back());
    mode = "layers";
    // (integration 2026-10-23, as check.py) an elf stands a row taller over the same legs, so its hem shows the legs' top
    // row, which far's `when elf` overlay fills across both thighs to the idle's hips: that row is the seat (hip, not thigh)
    if (std::find(L.flags.begin(), L.flags.end(), "elf") != L.flags.end()) {
      int top = 1 << 20;
      for (auto& [k, c] : near) top = std::min(top, k.second);
      for (auto& [k, c] : far) top = std::min(top, k.second);
      if (top != (1 << 20)) seat.insert(top);
    }
    return;
  }
  if (ids.empty()) { mode = "none"; return; }
  const PX& px = *f.layer(ids[0]);
  splitPalette(px, near, far, seat);
  std::map<K, char> own;
  PS shape;
  bool have = bootOwner(L, f, ids[0], own, shape);
  if (have && shape != keysOf(px)) { have = false; g_stale.insert(L.tag); }
  if (!have) have = nearBoot(px, f.fac, own) || stampMatch(px, f.fac, own);
  if (have)
    for (auto& [k, c] : px) {
      if (shaft(c) || seat.count(k.second) || !own.count(k)) continue;
      near.erase(k); far.erase(k);
      (own[k] == 'N' ? near : far)[k] = c;
    }
  mode = "palette";
}

// ---- leg geometry
std::map<int, std::vector<int>> rowsOf(const PX& px, const char* keep = nullptr) {
  std::map<int, std::vector<int>> R;
  for (auto& [k, c] : px) {
    if (keep && !std::strchr(keep, reg(c))) continue;
    R[k.second].push_back(k.first);
  }
  return R;
}
double mean(const std::vector<int>& v) { double s = 0; for (int x : v) s += x; return s / v.size(); }
struct Sk { bool ok = false; Pt hip, knee, ank; };
Sk skeleton(const PX& leg, int hipY, const std::set<int>& seat) {
  Sk sk;
  PX own;
  for (auto& [k, c] : leg) if (!seat.count(k.second)) own[k] = c;
  const auto sh = rowsOf(own, "TKS");
  if (sh.empty()) return sk;
  const int ytop = sh.begin()->first;
  double hx = mean(sh.begin()->second);
  if (!seat.empty()) {
    std::vector<int> su;
    for (auto& [k, c] : leg) if (seat.count(k.second)) su.push_back(k.first);
    if (!su.empty()) hx = std::min(std::max(hx, (double)*std::min_element(su.begin(), su.end()) + 2), (double)*std::max_element(su.begin(), su.end()) - 2);
  }
  sk.hip = {hx, (double)hipY};
  std::vector<K> Kp, bootp;
  PS Sset;
  for (auto& [k, c] : own) {
    if (reg(c) == 'K') Kp.push_back(k);
    if (bootc(c)) bootp.push_back(k);
    if (reg(c) == 'S') Sset.insert(k);
  }
  const auto S = rowsOf(own, "S");
  bool hasKn0 = !Kp.empty();
  Pt kn0;
  if (hasKn0) { for (auto& k : Kp) { kn0.u += k.first; kn0.y += k.second; } kn0.u /= Kp.size(); kn0.y /= Kp.size(); }
  std::vector<K> adj;
  for (const K& k : bootp) {
    bool a = false;
    for (int du = -1; du <= 1 && !a; du++)
      for (int dy = -1; dy <= 1 && !a; dy++) a = Sset.count({k.first + du, k.second + dy}) > 0;
    if (a) adj.push_back(k);
  }
  if (!adj.empty()) bootp = adj;
  if (!adj.empty() && !Sset.empty()) {
    int ys = -(1 << 20);
    for (auto& k : Sset) ys = std::max(ys, k.second);
    std::vector<K> adj2;
    for (auto& k : adj) if (k.second >= ys) adj2.push_back(k);
    if (!adj2.empty()) bootp = adj2;
  }
  if (!bootp.empty() && hasKn0) {
    double d0 = 1e18;
    for (auto& k : bootp) d0 = std::min(d0, dist(k, kn0));
    double su = 0, sy = 0; int n = 0;
    for (auto& k : bootp) if (dist(k, kn0) <= d0 + 0.6) { su += k.first; sy += k.second; n++; }
    sk.ank = {su / n, sy / n};
  } else if (!S.empty()) {
    const auto& last = *S.rbegin();
    sk.ank = {mean(last.second), (double)last.first};
  } else {
    const auto B = rowsOf(own, "B");
    if (B.empty()) return sk;
    sk.ank = {mean(B.begin()->second), (double)B.begin()->first};
  }
  if (!Kp.empty()) sk.knee = kn0;
  else {
    const int ym = pyround((ytop + sk.ank.y) / 2);
    auto it = sh.find(ym);
    if (it == sh.end()) {
      int bestY = sh.begin()->first; double bd = 1e18;
      for (auto& [y, v] : sh) if (std::abs(y - ym) < bd) { bd = std::abs(y - ym); bestY = y; }
      it = sh.find(bestY);
    }
    sk.knee = {mean(it->second), (double)ym};
  }
  sk.ok = true;
  return sk;
}
double fitSlope(const std::vector<std::pair<double, double>>& pts) {
  const size_t n = pts.size();
  if (n < 2) return 0.0;
  double mw = 0, mv = 0;
  for (auto& p : pts) { mw += p.first; mv += p.second; }
  mw /= n; mv /= n;
  double sww = 0, swv = 0;
  for (auto& p : pts) { sww += (p.first - mw) * (p.first - mw); swv += (p.first - mw) * (p.second - mv); }
  return sww ? swv / sww : 0.0;
}
bool segThick(const PX& leg, Pt a, Pt b, const char* keep, const std::set<int>& seat, double& out) {
  const double lo = std::min(a.y, b.y), hi = std::max(a.y, b.y);
  PX own;
  for (auto& [k, c] : leg)
    if (!seat.count(k.second) && std::strchr(keep, reg(c)) && lo - 0.5 <= k.second && k.second <= hi + 0.5) own[k] = c;
  if (std::hypot(b.u - a.u, b.y - a.y) < 2 || own.empty()) return false;
  const double dx = b.u - a.u, dy = b.y - a.y;
  const auto R = rowsOf(own);
  const double joint = std::atan2(std::abs(dx), std::abs(dy));
  std::vector<std::pair<double, double>> cen;
  for (auto& [y, v] : R) cen.push_back({(double)y, mean(v)});
  double sl = R.size() >= 2 ? fitSlope(cen) : 0.0;
  double meanW = 0;
  for (auto& [y, v] : R) meanW += v.size();
  meanW /= R.size();
  if (std::abs(dy) >= 1 && (R.size() >= 3 || std::abs(dx) <= std::abs(dy)) && std::abs(std::atan(std::abs(sl)) - joint) <= rad(25) && joint <= rad(50)) {
    out = meanW / std::sqrt(1 + sl * sl);
    return true;
  }
  if (std::abs(dx) <= std::abs(dy)) { out = meanW / std::sqrt(1 + (dx / dy) * (dx / dy)); return true; }
  std::map<int, std::vector<int>> C;
  for (auto& [k, c] : own) C[k.first].push_back(k.second);
  std::vector<std::pair<double, double>> cc;
  for (auto& [u, v] : C) cc.push_back({(double)u, mean(v)});
  sl = C.size() >= 3 ? fitSlope(cc) : dy / dx;
  double mc = 0;
  for (auto& [u, v] : C) mc += v.size();
  mc /= C.size();
  out = mc / std::sqrt(1 + sl * sl);
  return true;
}
bool upright(const Sk& sk, int y) {
  const Pt a = y <= sk.knee.y ? sk.hip : sk.knee, b = y <= sk.knee.y ? sk.knee : sk.ank;
  return std::abs(b.u - a.u) <= std::abs(b.y - a.y);
}

// ---- the idle reference
struct Ref {
  int W = 5; bool hasL = false; double L = 0; int boot = 0; PS head, torso, torsoTop; int iboot = 0; double ibootT = 3.0, iaxis = 0.0; int touch = 0; std::set<int> neck; std::string mode;
};
PS layerPx(const JLook& L, const JFrame& f, const std::set<std::string>& layers) {
  PS out;
  for (auto& [id, px] : f.layers) {
    const JPart* p = L.part(id);
    if (p && layers.count(p->layer)) for (auto& [k, c] : px) out.insert(k);
  }
  return out;
}
PS topRows(const PS& px, int n) {
  PS out;
  if (px.empty()) return out;
  int y0 = 1 << 20;
  for (auto& k : px) y0 = std::min(y0, k.second);
  for (auto& k : px) if (k.second < y0 + n) out.insert(k);
  return out;
}
PS neckRef(const JLook& L, const JFrame& f) {
  PS out;
  for (auto& [id, px] : f.layers) {
    const JPart* p = L.part(id);
    if (p && hasRolePrefix(*p, "neck ")) for (auto& [k, c] : px) out.insert(k);
  }
  return !out.empty() ? out : topRows(layerPx(L, f, {"torso"}), 2);
}
PS bodyPx(const JLook& L, const JFrame& f) {
  PS out;
  for (auto& [id, px] : f.layers) {
    const JPart* p = L.part(id);
    if (p && (p->layer == "torso" || hasRolePrefix(*p, "neck "))) for (auto& [k, c] : px) out.insert(k);
  }
  return out;
}
std::map<int, int> colMaxY(const PS& s) { std::map<int, int> m; for (auto& k : s) { auto it = m.find(k.first); if (it == m.end() || k.second > it->second) m[k.first] = k.second; } return m; }
std::map<int, int> colMinY(const PS& s) { std::map<int, int> m; for (auto& k : s) { auto it = m.find(k.first); if (it == m.end() || k.second < it->second) m[k.first] = k.second; } return m; }
std::set<int> neckCols(const PS& head, const PS& body) {
  std::set<int> out;
  const auto hb = colMaxY(head), ts = colMinY(body);
  for (auto& [u, y] : hb) { auto t = ts.find(u); if (t != ts.end() && t->second <= y + 1) out.insert(u); }
  return out;
}
int touchCols(const PS& head, const PS& torso) { return (int)neckCols(head, torso).size(); }
int gapCols(const JLook& L, const JFrame& f, const std::set<int>& cols) {
  // (integration 2026-10-23, as check.py) loose is the SKULL off its neck: the head, face and ears layers; a column the skull
  // does not reach (only hanging hair, a hood's fall, a veil) is no neck column of this frame (the hair swings and streams
  // in the walk and run: its tip lifting off the shoulder is the swing, not a loose head)
  const PS head = layerPx(L, f, {"head", "face", "ears"}), body = bodyPx(L, f);
  const PS raw = keysOf(f.raw);
  int n = 0;
  for (int u : cols) {
    int hb = -(1 << 20);
    bool hs = false;
    for (auto& k : head) if (k.first == u) { hs = true; hb = std::max(hb, k.second); }
    if (!hs) continue;
    int mb = 1 << 20;
    bool below = false, behind = false;
    for (auto& k : body) if (k.first == u) { if (k.second > hb) { below = true; mb = std::min(mb, k.second); } else behind = true; }
    if (!below) { if (!behind) n++; continue; }
    for (int y = hb + 1; y < mb; y++) if (!raw.count({u, y})) { n++; break; }
  }
  return n;
}
std::pair<int, int> bestShift(const PS& a, const PS& b, int r) {
  bool have = false;
  std::pair<int, int> key{0, 0}, best{0, 0};
  for (int dy = -r; dy <= r; dy++)
    for (int du = -r; du <= r; du++) {
      int n = 0;
      for (auto& k : a) n += b.count({k.first + du, k.second + dy}) ? 1 : 0;
      const std::pair<int, int> kk{n, -(std::abs(du) + std::abs(dy))};
      if (!have || kk > key) { key = kk; best = {du, dy}; have = true; }
    }
  return best;
}
double bootThick(const PX& px) {
  std::vector<std::pair<double, double>> O;
  std::set<int> ou;
  for (auto& [k, c] : px) if (reg(c) == 'O') { O.push_back({(double)k.first, (double)k.second}); ou.insert(k.first); }
  if (ou.size() < 2) {
    O.clear(); ou.clear();
    std::map<int, int> mx;
    for (auto& [k, c] : px) { auto it = mx.find(k.first); if (it == mx.end() || k.second > it->second) mx[k.first] = k.second; }
    for (auto& [u, y] : mx) { O.push_back({(double)u, (double)y}); ou.insert(u); }
  }
  const double sl = ou.size() >= 2 ? fitSlope(O) : 0.0;
  double cs;
  std::map<int, int> cnt;
  if (std::abs(sl) <= 1) { cs = 1 / std::sqrt(1 + sl * sl); for (auto& [k, c] : px) cnt[k.first]++; }
  else { cs = std::abs(sl) / std::sqrt(1 + sl * sl); for (auto& [k, c] : px) cnt[k.second]++; }
  std::vector<int> v;
  for (auto& [k, n] : cnt) v.push_back(n);
  std::sort(v.begin(), v.end());
  const double m = v.size() % 2 ? v[v.size() / 2] : (v[v.size() / 2 - 1] + v[v.size() / 2]) / 2.0;
  return m * cs;
}
double bootAxis(const PS& px) {
  const double n = (double)px.size();
  double mu = 0, my = 0;
  for (auto& k : px) { mu += k.first; my += k.second; }
  mu /= n; my /= n;
  double sxx = 0, syy = 0, sxy = 0;
  for (auto& k : px) { sxx += (k.first - mu) * (k.first - mu); syy += (k.second - my) * (k.second - my); sxy += (k.first - mu) * (k.second - my); }
  return deg(0.5 * std::atan2(2 * sxy, sxx - syy));
}
double axisDiff(double a, double b) { const double d = std::fmod(std::abs(a - b), 180.0); return std::min(d, 180 - d); }

bool idleRef(const JLook& L, char fac, Ref& ref) {
  const JFrame* f = nullptr;
  for (const JFrame& x : L.frames) if (x.gait == "idle" && x.fac == fac) { f = &x; break; }
  if (!f) return false;
  const std::vector<std::string> ids = legParts(L, *f);
  PX px;
  for (auto& id : ids) for (auto& [k, c] : *f->layer(id)) px[k] = c;
  // idle_legs: per shaft row the back and the front segment
  std::map<int, Row> rows;
  const std::set<int> seat = seatRows(px, rows);
  PS A, B;
  for (auto& [y, row] : rows) {
    if (seat.count(y)) continue;
    Row sh;
    for (auto& e : row) if (shaft(e.second)) sh.push_back(e);
    const std::vector<Row> ss = segments(sh);
    if (ss.empty()) continue;
    for (auto& e : ss.front()) A.insert({e.first, y});
    for (auto& e : ss.back()) B.insert({e.first, y});
  }
  const int hipY = f->hby + f->bob + f->hipY;
  std::set<int> ys;
  for (auto& k : A) ys.insert(k.second);
  for (auto& k : B) ys.insert(k.second);
  std::vector<int> ws;
  for (int y : ys) {
    int a = 0, b = 0;
    for (auto& k : A) a += k.second == y;
    for (auto& k : B) b += k.second == y;
    ws.push_back(std::max(a, b));
  }
  if (!ws.empty()) {
    std::set<int> uw(ws.begin(), ws.end());
    int bw = 0; std::pair<long, int> bk{-1, 0};
    for (int w : uw) { const std::pair<long, int> k{(long)std::count(ws.begin(), ws.end(), w), w}; if (k > bk) { bk = k; bw = w; } }
    ref.W = bw;
  } else ref.W = 5;
  PX n0, f0; std::set<int> s0; std::string m0;
  legsOf(L, *f, n0, f0, s0, m0);
  const Sk sk = skeleton(n0, hipY, s0);
  if (sk.ok) { ref.hasL = true; ref.L = dist(sk.hip, sk.knee) + dist(sk.knee, sk.ank); }
  ref.boot = 0;
  for (const JFrame& g : L.frames)
    if (g.gait == "walk" && g.fac == fac) {
      PX n, fa; std::set<int> se; std::string mo;
      legsOf(L, g, n, fa, se, mo);
      int c = 0;
      for (auto& [k, cc] : n) c += bootc(cc) && !se.count(k.second);
      ref.boot = std::max(ref.boot, c);
    }
  ref.head = layerPx(L, *f, HEADL);
  ref.torso = neckRef(L, *f);
  ref.torsoTop = topRows(layerPx(L, *f, {"torso"}), 2);
  PX ib;
  for (auto& [k, c] : n0) if (bootc(c) && !s0.count(k.second)) ib[k] = c;
  ref.iboot = (int)ib.size();
  ref.ibootT = ib.empty() ? 3.0 : bootThick(ib);
  ref.iaxis = ib.size() >= 3 ? bootAxis(keysOf(ib)) : 0.0;
  const PS hd = layerPx(L, *f, HEADL), bd = bodyPx(L, *f);
  ref.touch = touchCols(hd, bd);
  ref.neck = neckCols(layerPx(L, *f, {"head", "face", "ears"}), bd);   // (integration 2026-10-23) the skull's neck columns (gapCols)
  ref.mode = ids.size() >= 2 ? "layers" : "palette";
  return true;
}

// ---- the 2026-10-21 b checks
struct Sil { bool ok = false; int hem = 0; std::map<int, std::pair<int, int>> span; std::pair<int, int> hemSpan; };
Sil silhouette(const JLook& L, const JFrame& f) {
  Sil s;
  const PS T = layerPx(L, f, {"torso"});
  if (T.empty()) return s;
  for (auto& k : T) {
    auto it = s.span.find(k.second);
    if (it == s.span.end()) s.span[k.second] = {k.first, k.first};
    else it->second = {std::min(it->second.first, k.first), std::max(it->second.second, k.first)};
  }
  s.hem = s.span.rbegin()->first;
  int lo = 1 << 20, hi = -(1 << 20);
  for (int y : {s.hem - 1, s.hem}) { auto it = s.span.find(y); if (it != s.span.end()) { lo = std::min(lo, it->second.first); hi = std::max(hi, it->second.second); } }
  s.hemSpan = {lo, hi};
  s.ok = true;
  return s;
}
std::pair<int, int> spanAt(const Sil& s, int y) {
  auto it = s.span.find(y);
  return (y < s.hem - 1 && it != s.span.end()) ? it->second : s.hemSpan;
}
bool hipline(const JLook& L, const JFrame& f, const PX& near, const PX& far, bool along, int& worst, std::string& txt) {
  const Sil s = silhouette(L, f);
  if (!s.ok) { txt = "no torso"; return false; }
  worst = 0;
  std::vector<std::string> rep;
  for (int w = 0; w < 2; w++) {
    const PX& leg = w ? far : near;
    int back = 0, front = 0;
    for (auto& [k, c] : leg) {
      if (k.second > s.hem + L.T->hipRows) continue;
      const auto sp = spanAt(s, k.second);
      const int al = along ? std::max(0, k.second - s.hem) : 0;
      back = std::max(back, sp.first - k.first - al);
      front = std::max(front, k.first - sp.second - al);
    }
    if (back || front) rep.push_back(fmt("%s back %d front %d", w ? "f" : "n", back, front));
    worst = std::max({worst, back, front});
  }
  std::string r;
  for (auto& x : rep) r += (r.empty() ? "" : ", ") + x;
  txt = fmt("hem y%d span %d..%d", s.hem, s.hemSpan.first, s.hemSpan.second) + (rep.empty() ? " inside" : " overhang " + r);
  return true;
}
struct AnkRow { std::string where; int n; double a; };
std::vector<AnkRow> ankle(const PX& leg, const Sk& sk, const std::set<int>& seat, int ankleMinUnused = 0) {
  (void)ankleMinUnused;
  PX own;
  for (auto& [k, c] : leg) if (!seat.count(k.second)) own[k] = c;
  const double dx = sk.ank.u - sk.knee.u, dy = sk.ank.y - sk.knee.y;
  std::vector<AnkRow> out;
  if (std::hypot(dx, dy) < 1.5) return out;
  if (std::abs(dy) >= std::abs(dx)) {
    double cs = std::abs(dy) / std::hypot(dx, dy);
    const int sgn = dy > 0 ? 1 : -1;
    const int y0 = pyround(sk.knee.y) + sgn, y1 = pyround(sk.ank.y);
    std::vector<std::pair<double, double>> cen;
    for (int y = y0; sgn > 0 ? y <= y1 : y >= y1; y += sgn) {
      std::vector<int> us;
      for (auto& [k, c] : own) if (k.second == y && shaft(c)) us.push_back(k.first);
      if (!us.empty()) cen.push_back({(double)y, mean(us)});
    }
    if (cen.size() >= 3) {
      const double sl = fitSlope(cen);
      if (std::abs(std::atan(std::abs(sl)) - std::atan2(std::abs(dx), std::abs(dy))) <= rad(25)) cs = 1 / std::sqrt(1 + sl * sl);
    }
    for (int y = y0; sgn > 0 ? y <= y1 : y >= y1; y += sgn) {
      std::vector<int> us;
      for (auto& [k, c] : own) if (k.second == y && shaft(c)) us.push_back(k.first);
      if (us.empty()) continue;
      std::sort(us.begin(), us.end());
      const double cx = sk.knee.u + (y - sk.knee.y) * dx / dy;
      std::vector<std::vector<int>> runs;
      std::vector<int> cur{us[0]};
      for (size_t q = 1; q < us.size(); q++) { if (us[q] == cur.back() + 1) cur.push_back(us[q]); else { runs.push_back(cur); cur = {us[q]}; } }
      runs.push_back(cur);
      size_t bi = 0; double bd = 1e18;
      for (size_t q = 0; q < runs.size(); q++) {
        double m = 1e18;
        for (int u : runs[q]) m = std::min(m, std::abs(u - cx));
        if (m < bd) { bd = m; bi = q; }
      }
      out.push_back({fmt("y%d", y), (int)runs[bi].size(), runs[bi].size() * cs});
    }
  } else {
    const double cs = std::abs(dx) / std::hypot(dx, dy);
    const int sgn = dx > 0 ? 1 : -1;
    const int u0 = pyround(sk.knee.u) + 2 * sgn, u1 = pyround(sk.ank.u);
    for (int u = u0; sgn > 0 ? u <= u1 : u >= u1; u += sgn) {
      int n = 0;
      for (auto& [k, c] : own) if (k.first == u && (reg(c) == 'S' || reg(c) == 'K')) n++;
      if (n) out.push_back({fmt("u%d", u), n, n * cs});
    }
  }
  return out;
}
struct BootRes { char nm; int n; double t; bool piece; int pn, pw, ph; int vis; };
std::vector<BootRes> bootCheck(const PX& near, const PX& far, const std::set<int>& seat, const PX* rawc) {
  std::vector<BootRes> res;
  for (int w = 0; w < 2; w++) {
    const PX& leg = w ? far : near;
    PX b;
    for (auto& [k, c] : leg) if (bootc(c) && !seat.count(k.second)) b[k] = c;
    if (b.empty()) { res.push_back({w ? 'f' : 'n', 0, 0.0, false, 0, 0, 0, 0}); continue; }
    BootRes r{w ? 'f' : 'n', (int)b.size(), bootThick(b), false, 0, 0, 0, (int)b.size()};
    if (w) {
      PS vis;
      for (auto& [k, c] : b) {
        if (near.count(k)) continue;
        if (rawc) {
          auto ri = rawc->find(k);
          const int rc = ri == rawc->end() || !ri->second ? 2 : ri->second;
          if (!bootc(rc)) continue;
        }
        vis.insert(k);
      }
      PS seen;
      for (const K& k : vis) {
        if (seen.count(k)) continue;
        PS comp; std::vector<K> st{k}; seen.insert(k);
        while (!st.empty()) {
          K q = st.back(); st.pop_back(); comp.insert(q);
          for (K n : {K{q.first + 1, q.second}, K{q.first - 1, q.second}, K{q.first, q.second + 1}, K{q.first, q.second - 1}})
            if (vis.count(n) && !seen.count(n)) { seen.insert(n); st.push_back(n); }
        }
        int u0 = 1 << 20, u1 = -(1 << 20), y0 = 1 << 20, y1 = -(1 << 20);
        for (auto& q : comp) { u0 = std::min(u0, q.first); u1 = std::max(u1, q.first); y0 = std::min(y0, q.second); y1 = std::max(y1, q.second); }
        const int ww = u1 - u0 + 1, hh = y1 - y0 + 1;
        if (std::min(ww, hh) < 2 || std::max(ww, hh) < 3)
          if (!r.piece || (int)comp.size() < r.pn) { r.piece = true; r.pn = (int)comp.size(); r.pw = ww; r.ph = hh; }
      }
      r.vis = (int)vis.size();
    }
    res.push_back(r);
  }
  return res;
}
bool boneDir(const PS& px, Pt toward, Pt frm, Pt& v) {
  if (px.size() < 3) return false;
  std::map<int, std::vector<int>> R, C2;
  for (auto& k : px) { R[k.second].push_back(k.first); C2[k.first].push_back(k.second); }
  bool have = false;
  if (R.size() >= 2) {
    std::vector<std::pair<double, double>> p;
    for (auto& [y, us] : R) p.push_back({(double)y, mean(us)});
    const double sl = fitSlope(p);
    if (std::abs(sl) <= 1) { v = {sl, 1.0}; have = true; }
  }
  if (!have && C2.size() >= 2) {
    std::vector<std::pair<double, double>> p;
    for (auto& [u, ys] : C2) p.push_back({(double)u, mean(ys)});
    const double sl = fitSlope(p);
    v = {1.0, sl}; have = true;
  }
  if (!have) return false;
  const double du = toward.u - frm.u, dy = toward.y - frm.y;
  if (v.u * du + v.y * dy < 0) v = {-v.u, -v.y};
  return true;
}
double angleBetween(Pt a, Pt b) {
  const double na = std::hypot(a.u, a.y), nb = std::hypot(b.u, b.y);
  if (na < 1e-6 || nb < 1e-6) return 0.0;
  return deg(std::acos(std::max(-1.0, std::min(1.0, (a.u * b.u + a.y * b.y) / (na * nb)))));
}
double bend(const Sk& sk, const PX* leg = nullptr, const std::set<int>* seat = nullptr) {
  Pt a{sk.knee.u - sk.hip.u, sk.knee.y - sk.hip.y}, b{sk.ank.u - sk.knee.u, sk.ank.y - sk.knee.y};
  if (leg) {
    PS T, S;
    for (auto& [k, c] : *leg) {
      if (seat && seat->count(k.second)) continue;
      if (reg(c) == 'T') T.insert(k);
      if (reg(c) == 'S') S.insert(k);
    }
    if (!S.empty()) {
      Pt sc;
      for (auto& k : S) { sc.u += k.first; sc.y += k.second; }
      sc.u /= S.size(); sc.y /= S.size();
      Pt a2, b2;
      if (boneDir(T, sk.knee, sk.hip, a2)) a = a2;
      if (boneDir(S, sc, sk.knee, b2)) b = b2;
    }
  }
  return angleBetween(a, b);
}
Pt thighVec(const Sk& sk, const PX& leg, const std::set<int>& seat) {
  PS T;
  for (auto& [k, c] : leg) if (reg(c) == 'T' && !seat.count(k.second)) T.insert(k);
  Pt v;
  if (boneDir(T, sk.knee, sk.hip, v)) return v;
  return {sk.knee.u - sk.hip.u, sk.knee.y - sk.hip.y};
}
double jbend(const Sk& sk) { return angleBetween({sk.knee.u - sk.hip.u, sk.knee.y - sk.hip.y}, {sk.ank.u - sk.knee.u, sk.ank.y - sk.knee.y}); }
double sbend(const Sk& sk, const PX& leg, const std::set<int>& seat) {
  std::vector<K> S;
  for (auto& [k, c] : leg) if (reg(c) == 'S' && !seat.count(k.second)) S.push_back(k);
  if (S.size() < 3) return jbend(sk);
  Pt sc;
  for (auto& k : S) { sc.u += k.first; sc.y += k.second; }
  sc.u /= S.size(); sc.y /= S.size();
  Sk s2 = sk; s2.ank = sc;
  return jbend(s2);
}
double ebend(const Sk& sk, const PX& leg, const std::set<int>& seat) {
  std::vector<K> S;
  for (auto& [k, c] : leg) if (reg(c) == 'S' && !seat.count(k.second)) S.push_back(k);
  if (S.size() < 3) return jbend(sk);
  double dm = 0;
  for (auto& k : S) dm = std::max(dm, dist(k, sk.knee));
  Pt e; int n = 0;
  for (auto& k : S) if (dist(k, sk.knee) >= dm - 0.5) { e.u += k.first; e.y += k.second; n++; }
  e.u /= n; e.y /= n;
  Sk s2 = sk; s2.ank = e;
  return jbend(s2);
}
double fromVertical(Pt v) { return deg(std::atan2(std::abs(v.u), v.y)); }
bool foldCheck(const JLook& L, const JFrame& f, const PX& leg, const Sk& sk, const std::set<int>& seat, std::string& out) {
  const BodyT& T = *L.T;
  std::vector<std::string> bad;
  const double ta = fromVertical(thighVec(sk, leg, seat));
  const double ja = fromVertical({sk.knee.u - sk.hip.u, sk.knee.y - sk.hip.y});
  const double jb = jbend(sk);
  double bd = sbend(sk, leg, seat);
  const double eb = ebend(sk, leg, seat);
  const double rb = bend(sk, &leg, &seat);
  if (bd < T.foldBend && eb >= T.foldBend && rb >= T.flightBend) bd = eb;
  if (bd < T.foldBend) bad.push_back(fmt("knee bend %.0f < %.0f (no knee)", bd, T.foldBend));
  const Sil s = silhouette(L, f);
  std::string txt = fmt("thigh %.0f deg from vertical (rows %.0f) bend %.0f (shin mass %.0f, far end %.0f; joints %.0f, rows %.0f)", ja, ta, bd, sbend(sk, leg, seat), eb, jb, rb);
  if (!s.ok) bad.push_back("no torso");
  else {
    const int lo = s.hemSpan.first, hi = s.hemSpan.second;
    const int y1 = s.hem + T.foldRows;
    std::vector<K> outp;
    for (auto& [k, c] : leg) {
      if (k.second > y1) continue;
      const auto sp = spanAt(s, k.second);
      if (k.first < sp.first || k.first > sp.second) outp.push_back(k);
    }
    txt += fmt("; rows to y%d (the hem y%d + %d) span u%d..u%d", y1, s.hem, T.foldRows, lo, hi);
    if (!outp.empty()) {
      int ov = -(1 << 20), ya = 1 << 20, yb = -(1 << 20);
      for (auto& k : outp) {
        const auto sp = spanAt(s, k.second);
        ov = std::max(ov, std::max(sp.first - k.first, k.first - sp.second));
        ya = std::min(ya, k.second); yb = std::max(yb, k.second);
      }
      bad.push_back(fmt("thigh outside the hips by %d px (%d px in rows y%d..y%d)", ov, (int)outp.size(), ya, yb));
    }
    int mb = 1 << 20;
    bool below = false;
    for (auto& [k, c] : leg) if (k.second > y1 && k.first < lo) { below = true; mb = std::min(mb, k.first); }
    if (below) txt += fmt("; below the hip rows %d px behind (allowed)", lo - mb);
  }
  std::string b;
  for (auto& x : bad) b += (b.empty() ? "" : ", ") + x;
  out = txt + (bad.empty() ? "" : " " + b);
  return bad.empty();
}
struct PostOf { double la, bd; bool isp; };
PostOf postOf(const BodyT& T, const PX& leg, const Sk& sk, const std::set<int>& seat) {
  const double la = std::abs(deg(std::atan2(sk.ank.u - sk.hip.u, sk.ank.y - sk.hip.y)));
  const double bd = std::min(jbend(sk), bend(sk, &leg, &seat));
  return {la, bd, la < T.postVert && bd < T.postStraight};
}
bool soleY(const PX& leg, const std::set<int>& seat, int& y) {
  bool any = false;
  for (auto& [k, c] : leg) if (bootc(c) && !seat.count(k.second)) { y = any ? std::max(y, k.second) : k.second; any = true; }
  return any;
}
bool levelWidth(const PX& leg, int y, const Sk& sk, const std::set<int>& seat, int& out) {
  std::vector<std::pair<int, int>> own;
  for (auto& [k, c] : leg) if (k.second == y && !seat.count(k.second) && shaft(c)) own.push_back({k.first, c});
  if (own.empty()) return false;
  int nt = 0;
  for (auto& e : own) nt += reg(e.second) == 'T';
  const Pt a = nt * 2 > (int)own.size() ? sk.hip : sk.knee, b = nt * 2 > (int)own.size() ? sk.knee : sk.ank;
  if (std::abs(b.u - a.u) <= std::abs(b.y - a.y)) return false;
  std::vector<int> us;
  for (auto& e : own) us.push_back(e.first);
  std::sort(us.begin(), us.end());
  const int mid = us[us.size() / 2];
  out = 0;
  for (auto& [k, c] : leg) if (k.first == mid && !seat.count(k.second) && shaft(c)) out++;
  return true;
}
std::string join(const std::vector<std::string>& v, const char* sep, size_t maxN = 1u << 30) {
  std::string s;
  for (size_t k = 0; k < v.size() && k < maxN; k++) s += (k ? sep : "") + v[k];
  return s;
}
bool isRunGait(const std::string& g) { return g == "run" || g == "drun"; }
// (gates-wear fix round 1) the walking postures (Carry 0-2, Play, Dance) borrow the walk's and the run's leg keys: a frame
// on a run key (Play: run1 run3 run4 run6) is measured as the run's legs are (HIPW's 2 px), and a frame on a run flight key
// (run3, run6) or the child's hop is in the air (GROUND: no sole on the ground row, as the run's flight)
bool isPostureGait(const std::string& g) { return g != "walk" && g != "run" && g != "dwalk" && g != "drun" && g != "idle"; }
std::string legsKeyOf(const std::map<std::string, std::string>& keys) { auto it = keys.find("legs"); return it == keys.end() ? std::string() : it->second; }

// ---- the checks of one frame (check.py check_frame)
void checkFrame(const JLook& L, JFrame& f, const Ref& ref) {
  const BodyT& T = *L.T;
  Results& out = f.res;
  out = Results();
  PX near, far; std::set<int> seat; std::string mode;
  legsOf(L, f, near, far, seat, mode);
  const int hipY = f.hby + f.bob + f.hipY;
  const Sk skn = skeleton(near, hipY, seat), skf = skeleton(far, hipY, seat);
  const PX& rawc0 = f.raw;
  // FARWIDTH
  {
    std::vector<std::string> bad;
    int minw = 99;
    if (far.empty() || !skf.ok) bad.push_back("no far leg");
    else {
      PX fo, no;
      for (auto& [k, c] : far) if (!seat.count(k.second)) fo[k] = c;
      for (auto& [k, c] : near) if (!seat.count(k.second)) no[k] = c;
      const auto fr = rowsOf(fo, "TKS"), nr = rowsOf(no, "TKS");
      const int ftop = fr.begin()->first;
      const bool hasN = !nr.empty();
      const int ntop = hasN ? nr.begin()->first : 0;
      const PS nhid = bootHiddenCells(L, f, near, far, mode).first;
      for (auto& [y, us] : fr) {
        int w = (int)us.size(), lw = 0;
        if (levelWidth(far, y, skf, seat, lw)) w = lw;
        minw = std::min(minw, w);
        if (w < ref.W) bad.push_back(fmt("y%d %d<%didle", y, w, ref.W));
        if (hasN && skn.ok) {
          const int yn = ntop + (y - ftop);
          auto ni = nr.find(yn);
          if (ni != nr.end() && upright(skf, y) && upright(skn, yn) && w < (int)ni->second.size()) {
            bool hid = false;
            for (int u : ni->second) hid = hid || nhid.count({u, yn});
            if (!hid) bad.push_back(fmt("y%d %d<%dnear", y, w, (int)ni->second.size()));
          }
        }
      }
      for (auto& [y, us0] : fr) {
        int lw = 0;
        if (levelWidth(far, y, skf, seat, lw)) continue;
        std::vector<int> vis;
        for (auto& [k, c] : far) if (k.second == y && shaft(c) && !near.count(k)) vis.push_back(k.first);
        std::sort(vis.begin(), vis.end());
        std::vector<std::vector<int>> runs;
        std::vector<int> cur;
        for (int u : vis) { if (!cur.empty() && u != cur.back() + 1) { runs.push_back(cur); cur.clear(); } cur.push_back(u); }
        if (!cur.empty()) runs.push_back(cur);
        for (auto& r : runs) {
          bool seen = false;
          for (int u : r) { auto ri = rawc0.find({u, y}); const int rc = ri == rawc0.end() || !ri->second ? 2 : ri->second; seen = seen || shaft(rc); }
          if (!seen) continue;
          if ((int)r.size() < SLIVER) bad.push_back(fmt("y%d sliver %d", y, (int)r.size()));
        }
      }
    }
    out.set("FARWIDTH", bad.empty(), (minw < 99 ? fmt("far min %d (idle %d)", minw, ref.W) : fmt("far min - (idle %d)", ref.W)) +
                                         (bad.empty() ? "" : " " + join(bad, ", ", 4) + (bad.size() > 4 ? "..." : "")));
  }
  // LEGLEN, THICK
  {
    std::vector<std::string> lens, ths, lbad, tbad;
    for (int w = 0; w < 2; w++) {
      const PX& leg = w ? far : near;
      const Sk& sk = w ? skf : skn;
      const std::string nm = w ? "f" : "n";
      if (!sk.ok) { lbad.push_back(nm + " none"); tbad.push_back(nm + " none"); continue; }
      const double Lg = dist(sk.hip, sk.knee) + dist(sk.knee, sk.ank);
      lens.push_back(nm + fmt("%.1f", Lg));
      if (ref.hasL && Lg > ref.L + LEN_TOL) lbad.push_back(fmt("%s %.1f > idle %.1f + %g (stretched)", nm.c_str(), Lg, ref.L, LEN_TOL));
      std::vector<K> S;
      for (auto& [k, c] : leg) if (reg(c) == 'S' && !seat.count(k.second)) S.push_back(k);
      double Ls = Lg;
      if (!S.empty()) {
        double dm = 0;
        for (auto& k : S) dm = std::max(dm, dist(k, sk.knee));
        Pt e; int n = 0;
        for (auto& k : S) if (dist(k, sk.knee) >= dm - 1) { e.u += k.first; e.y += k.second; n++; }
        e.u /= n; e.y /= n;
        Ls = std::max(Lg, dist(sk.hip, sk.knee) + dist(sk.knee, e));
      }
      if (Ls > Lg + 0.05) lens.back() += fmt("/%.1fend", Ls);
      if (ref.hasL && Ls < ref.L - LEN_SHORT) lbad.push_back(fmt("%s %.1f < idle %.1f - %g (shortened)", nm.c_str(), Ls, ref.L, LEN_SHORT));
      double t1 = 0, t2 = 0;
      const bool h1 = segThick(leg, sk.hip, sk.knee, "TK", seat, t1), h2 = segThick(leg, sk.knee, sk.ank, "KS", seat, t2);
      for (int q = 0; q < 2; q++) {
        if (!(q ? h2 : h1)) continue;
        const double t = q ? t2 : t1;
        ths.push_back(nm + (q ? "s" : "t") + fmt("%.1f", t));
        if (t < T.thick) tbad.push_back(fmt("%s %s %.2f", nm.c_str(), q ? "shin" : "thigh", t));
      }
    }
    out.set("LEGLEN", lbad.empty(), join(lens, " ") + fmt(" (idle %.1f)", ref.L) + (lbad.empty() ? "" : " " + join(lbad, ", ")));
    out.set("THICK", tbad.empty(), join(ths, " ") + (tbad.empty() ? "" : " " + join(tbad, ", ")));
  }
  // HEAD
  {
    const PS head = layerPx(L, f, HEADL), tor = bodyPx(L, f);
    const auto [hdu, hdy] = bestShift(ref.head, head, 6);
    const auto [tdu, tdy] = bestShift(ref.torso, neckRef(L, f), 6);
    int rdx = hdu - tdu, rdy = hdy - tdy;
    // (integration 2026-10-23, as check.py) the neckwear is not rigid (the woman's scarf and kerchief on her hang keys, the
    // man's kerchief flowing): where the head is off the neckwear's shape, it is still attached when it sits on the torso's
    // top 2 rows as in the idle
    if (rdx != 0) {
      const auto [t2u, t2y] = bestShift(ref.torsoTop, topRows(layerPx(L, f, {"torso"}), 2), 6);
      if (hdu - t2u == 0) { rdx = 0; rdy = hdy - t2y; }
    }
    const int tc = touchCols(head, tor);
    std::set<int> cols;
    for (int u : ref.neck) cols.insert(u + hdu);
    const int gc = gapCols(L, f, cols);
    const int need = std::min(3, ref.touch);
    std::vector<std::string> hb;
    if (f.headX != 0) hb.push_back(fmt("clip headX %d", f.headX));
    if (rdx != 0) hb.push_back(fmt("head dx %+d vs neck", rdx));
    if (tc < need) hb.push_back(fmt("%d touching cols (< %d)", tc, need));
    if (gc) hb.push_back(fmt("head loose (transparent under it) in %d of its %d neck cols", gc, (int)ref.neck.size()));
    out.set("HEAD", hb.empty(), fmt("dx %+d dy %+d (clip headX %d headY %d) touch %d/%d loose %d/%d", rdx, rdy, f.headX, f.headY, tc, ref.touch, gc, (int)ref.neck.size()) +
                                    (hb.empty() ? "" : " " + join(hb, ", ")));
  }
  const bool run = isRunGait(f.gait);
  // BUNCH (run contact and push)
  if (run && (f.i == 0 || f.i == 1 || f.i == 3 || f.i == 4)) {
    PS nb, fb;
    for (auto& [k, c] : near) if (bootc(c)) nb.insert(k);
    for (auto& [k, c] : far) if (bootc(c) && !near.count(k)) fb.insert(k);
    bool touch = false;
    for (auto& k : nb)
      for (int du = -1; du <= 1 && !touch; du++)
        for (int dy = -1; dy <= 1 && !touch; dy++) touch = fb.count({k.first + du, k.second + dy}) > 0;
    int merge = 0;
    for (auto& k : nb) {
      if (stepOf(near.at(k)) == 0) continue;
      for (K d : {K{1, 0}, K{-1, 0}, K{0, 1}, K{0, -1}}) {
        const K q{k.first + d.first, k.second + d.second};
        if (fb.count(q) && stepOf(far.at(q)) != 0) merge++;
      }
    }
    const double show = ref.boot ? (double)fb.size() / ref.boot : 0;
    const bool ok = !touch || (merge == 0 && show >= BOOT_SHOW);
    out.set("BUNCH", ok, fmt("%s, %d interior contacts, far boot shows %d/%d = %.0f%%", touch ? "touch" : "apart", merge, (int)fb.size(), ref.boot, 100 * show) + (ok ? "" : " LUMP"));
  }
  // HIPLINE
  {
    int ov = 0;
    std::string txt;
    const bool have = hipline(L, f, near, far, run && (f.i == 1 || f.i == 4), ov, txt);
    out.set("HIPLINE", have && ov == 0, (have ? fmt("overhang %d px; ", ov) : std::string()) + txt);
  }
  // ANKLE
  {
    std::vector<std::string> ab, at;
    for (int w = 0; w < 2; w++) {
      const PX& leg = w ? far : near;
      const Sk& sk = w ? skf : skn;
      const std::string nm = w ? "f" : "n";
      if (!sk.ok) { ab.push_back(nm + " none"); continue; }
      const std::vector<AnkRow> rows = ankle(leg, sk, seat);
      if (rows.empty()) { at.push_back(nm + " -"); continue; }
      size_t mi = 0;
      auto key = [&](const AnkRow& r) { return std::make_pair((r.n >= T.ankleMin && r.a >= T.ankleAcross) ? 1 : 0, r.a); };
      for (size_t q = 1; q < rows.size(); q++) if (key(rows[q]) < key(rows[mi])) mi = q;
      at.push_back(fmt("%s min %dpx/%.1f (%d rows)", nm.c_str(), rows[mi].n, rows[mi].a, (int)rows.size()));
      for (auto& r : rows) if (r.n < T.ankleMin || r.a < T.ankleAcross) ab.push_back(fmt("%s %s %dpx across %.1f", nm.c_str(), r.where.c_str(), r.n, r.a));
    }
    out.set("ANKLE", ab.empty(), join(at, " ") + (ab.empty() ? "" : " " + join(ab, ", ", 5) + (ab.size() > 5 ? "..." : "")));
  }
  // BOOT
  const auto hidC = bootHiddenCells(L, f, near, far, mode);
  const int hidN = (int)hidC.first.size(), hidF = (int)hidC.second.size();
  {
    std::vector<std::string> bb, bt;
    const int ib = ref.iboot;
    const double it = ref.ibootT;
    for (const BootRes& r : bootCheck(near, far, seat, &f.raw)) {
      const int hid = r.nm == 'n' ? hidN : hidF;
      int n = r.n;
      if (n && hid) n += hid;
      bt.push_back(fmt("%c %dpx T%.2f", r.nm, n, r.t) + (r.nm == 'f' ? fmt(" shows %d", r.vis) : std::string()) + (hid ? fmt(" (%d under its shaft)", hid) : std::string()));
      if (!n) { bb.push_back(fmt("%c no boot", r.nm)); continue; }
      if (std::abs(n - ib) > BOOT_TOL * ib) bb.push_back(fmt("%c %dpx vs idle %d", r.nm, n, ib));
      if (r.t < it - BOOT_T_TOL) bb.push_back(fmt("%c thickness %.2f < idle %.2f", r.nm, r.t, it));
      if (r.piece) bb.push_back(fmt("%c visible sliver %dx%d (%d px)", r.nm, r.pw, r.ph, r.pn));
    }
    out.set("BOOT", bb.empty(), join(bt, " ") + fmt(" (idle %dpx T%.2f)", ib, it) + (bb.empty() ? "" : " " + join(bb, ", ")));
  }
  // FLIGHT
  if (run && (f.i == 2 || f.i == 5)) {
    std::vector<std::string> fb, ft;
    for (int w = 0; w < 2; w++) {
      const PX& leg = w ? far : near;
      int sy = 0;
      const bool has = soleY(leg, seat, sy);
      ft.push_back(std::string(w ? "f" : "n") + (has ? fmt(" sole y%d", sy) : std::string(" sole yNone")));
      if (!has || -sy < T.flightLift) fb.push_back(fmt("%s sole %d px up < %d", w ? "f" : "n", has ? -sy : 0, T.flightLift));
    }
    if (skn.ok && skf.ok) {
      const bool nTr = skn.ank.u < skf.ank.u;
      const double bd = bend(nTr ? skn : skf, nTr ? &near : &far, &seat);
      ft.push_back(fmt("trailing %s knee bend %.0f deg", nTr ? "n" : "f", bd));
      if (bd < T.flightBend) fb.push_back(fmt("trailing %s bend %.0f < %.0f", nTr ? "n" : "f", bd, T.flightBend));
    } else fb.push_back("a leg missing");
    out.set("FLIGHT", fb.empty(), join(ft, " ") + (fb.empty() ? "" : " " + join(fb, ", ")));
  }
  auto ground = [&](const PX& leg) { int g = 0; for (auto& [k, c] : leg) g += k.second == 0 && bootc(c); return g; };
  // PUSH
  if (run && (f.i == 1 || f.i == 4)) {
    int sg = 0; const Sk* ss = nullptr; const char* sn = "";
    for (int w = 0; w < 2; w++) {
      const Sk& sk = w ? skf : skn;
      const int g = ground(w ? far : near);
      if (sk.ok && g && (!ss || g > sg)) { sg = g; ss = &sk; sn = w ? "f" : "n"; }
    }
    if (!ss) out.set("PUSH", false, "no support leg on the ground");
    else {
      const double lean = deg(std::atan2(ss->hip.u - ss->ank.u, ss->ank.y - ss->hip.y));
      out.set("PUSH", lean >= T.pushLean, fmt("support %s hip-ankle leans back %.0f deg (>= %.0f)", sn, lean, T.pushLean));
    }
  }
  // BOOTTILT, BOOTSIZE
  {
    std::vector<std::string> tb, tt;
    std::vector<int> sz;
    for (int w = 0; w < 2; w++) {
      const PX& leg = w ? far : near;
      const char* nm = w ? "f" : "n";
      PX b;
      for (auto& [k, c] : leg) if (bootc(c) && !seat.count(k.second)) b[k] = c;
      if (b.size() < 3) { tb.push_back(fmt("%s no boot", nm)); continue; }
      const double tilt = axisDiff(bootAxis(keysOf(b)), ref.iaxis);
      const double th = bootThick(b);
      tt.push_back(fmt("%s tilt %.0f T%.2f", nm, tilt, th));
      if (tilt > TILT_MAX) tb.push_back(fmt("%s tilt %.0f > %.0f", nm, tilt, TILT_MAX));
      if (th < ref.ibootT - TILT_T_TOL) tb.push_back(fmt("%s thick %.2f < idle %.2f - %g (slab)", nm, th, ref.ibootT, TILT_T_TOL));
      sz.push_back((int)b.size() + (w ? hidF : hidN));
    }
    out.set("BOOTTILT", tb.empty(), join(tt, " ") + fmt(" (idle axis %.0f T%.2f)", ref.iaxis, ref.ibootT) + (tb.empty() ? "" : " " + join(tb, ", ")));
    if (sz.size() == 2 && std::max(sz[0], sz[1]) > 0) {
      const double d = std::abs(sz[0] - sz[1]) / (double)std::max(sz[0], sz[1]);
      out.set("BOOTSIZE", d <= BOOTSIZE_TOL, fmt("n %dpx f %dpx differ %.0f%% (<= %.0f%%)", sz[0], sz[1], 100 * d, 100 * BOOTSIZE_TOL));
    } else out.set("BOOTSIZE", false, "a boot missing");
  }
  // FOLD, POST's reading
  if (run && (f.i == 2 || f.i == 5)) {
    if (skn.ok && skf.ok) {
      const bool nTr = skn.ank.u < skf.ank.u;
      std::string t;
      const bool ok = foldCheck(L, f, nTr ? near : far, nTr ? skn : skf, seat, t);
      out.set("FOLD", ok, (nTr ? "trailing n " : "trailing f ") + t);
    } else out.set("FOLD", false, "a leg missing");
  }
  f.hasPost = false;
  if (run) {
    int sg = 0; int sw = -1;
    for (int w = 0; w < 2; w++) {
      const Sk& sk = w ? skf : skn;
      const int g = ground(w ? far : near);
      if (sk.ok && g && (sw < 0 || g > sg)) { sg = g; sw = w; }
    }
    int lw = -1;
    if (skn.ok && skf.ok) lw = skn.ank.u >= skf.ank.u ? 0 : 1;
    const int who = sw >= 0 ? sw : lw;
    f.hasPost = true;
    if (who >= 0) {
      const PostOf po = postOf(T, who ? far : near, who ? skf : skn, seat);
      std::string txt = fmt("%s %s hip-ankle %.0f deg from vertical, knee bend %.0f", sw >= 0 ? "support" : "lead", who ? "f" : "n", po.la, po.bd) + (po.isp ? " (a post)" : "");
      f.post = Post{po.isp, false, 0, ""};
      if ((f.i == 2 || f.i == 5) && lw >= 0) {
        const Sk& ls = lw ? skf : skn;
        const double lb = std::min(jbend(ls), bend(ls, lw ? &far : &near, &seat));
        f.post.hasLb = true; f.post.lb = lb;
        txt += fmt("; flight lead %s knee bend %.0f (>= %.0f)", lw ? "f" : "n", lb, T.postLead);
      }
      f.post.t = txt;
    } else f.post = Post{false, false, 0, "no lead/support leg"};
  }
  f.lnear = near; f.lfar = far; f.lseat = seat; f.lmode = mode;
}

// the planted boot (check.py contact)
struct Contact { bool ok = false; int heel = 0, toe = 0; bool heelDown = false, toeDown = false; };
Contact contact(const PX& near, const PX& far, bool hasPlant, double plant, int sole = 7) {
  Contact r;
  std::set<int> cs;
  for (auto& [k, c] : near) if (k.second == 0 && bootc(c)) cs.insert(k.first);
  for (auto& [k, c] : far) if (k.second == 0 && bootc(c)) cs.insert(k.first);
  if (cs.empty() || !hasPlant) return r;
  std::vector<std::vector<int>> runs;
  std::vector<int> cur;
  for (int u : cs) { if (!cur.empty() && u != cur.back() + 1) { runs.push_back(cur); cur.clear(); } cur.push_back(u); }
  runs.push_back(cur);
  size_t bi = 0; double bd = 1e18;
  for (size_t q = 0; q < runs.size(); q++) {
    double m = 1e18;
    for (int u : runs[q]) m = std::min(m, std::abs(u - plant));
    if (m < bd) { bd = m; bi = q; }
  }
  const std::vector<int>& rr = runs[bi];
  r.ok = true; r.heel = rr.front(); r.toe = rr.back();
  if ((int)rr.size() >= sole - 1) { r.heelDown = r.toeDown = true; return r; }
  int inN = 0, inF = 0;
  for (int u : rr) { inN += near.count({u, 0}) ? 1 : 0; inF += far.count({u, 0}) ? 1 : 0; }
  const PX& leg = inN >= inF ? near : far;
  PS bp;
  for (auto& [k, c] : leg) if (bootc(c)) bp.insert(k);
  PS comp;
  std::vector<K> st;
  for (int u : rr) st.push_back({u, 0});
  while (!st.empty()) {
    K q = st.back(); st.pop_back();
    if (comp.count(q)) continue;
    comp.insert(q);
    for (K n : {K{q.first + 1, q.second}, K{q.first - 1, q.second}, K{q.first, q.second - 1}, K{q.first, q.second + 1}})
      if (bp.count(n) && !comp.count(n)) st.push_back(n);
  }
  int ahead = 0, behind = 0;
  for (auto& k : comp) { ahead += k.first > rr.back(); behind += k.first < rr.front(); }
  r.heelDown = ahead >= behind; r.toeDown = behind >= ahead;
  return r;
}
struct RegR { int i; bool ok; std::string t; };
std::vector<RegR> regCheck(std::vector<JFrame*> fr, int sole) {
  std::vector<RegR> res;
  std::sort(fr.begin(), fr.end(), [](JFrame* a, JFrame* b) { return a->i < b->i; });
  const size_t n = fr.size();
  for (size_t k = 0; k < n; k++) {
    JFrame *a = fr[k], *b = fr[(k + 1) % n];
    if (!a->hp0 || !a->hp1 || !b->hp0) continue;
    if (std::abs(a->plant1 - b->plant0) > 0.01) continue;
    const double adv = a->plant0 - a->plant1;
    const Contact ca = contact(a->lnear, a->lfar, true, a->plant0, sole), cb = contact(b->lnear, b->lfar, true, b->plant0, sole);
    if (!ca.ok || !cb.ok) continue;
    int pa, pb;
    const char* what;
    if (ca.heelDown && cb.heelDown) { pa = ca.heel; pb = cb.heel; what = "heel"; }
    else if (ca.toeDown && cb.toeDown) { pa = ca.toe; pb = cb.toe; what = "toe"; }
    else { res.push_back({a->i, false, fmt("%d>%d the planted foot changes ends (heel %d..toe %d > %d..%d)", a->i, b->i, ca.heel, ca.toe, cb.heel, cb.toe)}); continue; }
    const double slide = adv + pb - pa;
    res.push_back({a->i, std::abs(slide) <= 1, fmt("%d>%d %s %+.0f (advance %.0f, %d > %d)", a->i, b->i, what, slide, adv, pa, pb)});
  }
  return res;
}

// ---- the re-homed B11 checks (AREA HIPW SLIVER SQUAT JOINTS GROUND; 5.1, 5.2's last rows)
bool coverRole(const JPart& p) { return hasRolePrefix(p, "garment ") || hasRolePrefix(p, "overlay ") || hasRolePrefix(p, "armour "); }
static const char* const kLayerOrder[] = {"FxBack", "HeldBack", "Back", "CloakBack", "HairBack", "ArmFar", "Legs", "Shoes", "HangBack", "Torso", "HangFront", "Belt", "Neck",
                                          "ArmNear", "Head", "Face", "Beard", "HairFront", "Ears", "Headwear", "Held", "Shield", "CloakFront", "Fx"};
int layerIndex(const std::string& l) {
  for (int k = 0; k < (int)(sizeof kLayerOrder / sizeof *kLayerOrder); k++) {
    const char* n = kLayerOrder[k];
    if (l.size() == std::strlen(n) && std::equal(l.begin(), l.end(), n, [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); })) return k;
  }
  return -1;
}
struct Mass { int area = 0, hipW = 0, hipRow = -99, minRun = 99, skirt = 0, runU = 0, runY = 0; };
Mass massOf(const JLook& L, const JFrame& f) {
  Mass m;
  PX legs;
  PS cover;
  const int legsL = layerIndex("legs");
  for (auto& [id, px] : f.layers) {
    const JPart* p = L.part(id);
    if (!p) continue;
    if (p->layer == "legs") for (auto& [k, c] : px) legs[k] = c;
    else if (coverRole(*p) && layerIndex(p->layer) > legsL) for (auto& [k, c] : px) { cover.insert(k); m.skirt++; }
  }
  m.skirt = (int)cover.size();
  std::map<int, std::vector<std::pair<int, int>>> rows;   // y -> (u, code) visible
  for (auto& [k, c] : legs) if (!cover.count(k)) rows[k.second].push_back({k.first, c});
  auto fringe = [&](int y) {   // the cover's row y in 2 or more pieces
    std::vector<int> us;
    for (auto& k : cover) if (k.second == y) us.push_back(k.first);
    std::sort(us.begin(), us.end());
    int pieces = 0;
    for (size_t q = 0; q < us.size(); q++) if (q == 0 || us[q] != us[q - 1] + 1) pieces++;
    return pieces >= 2;
  };
  for (auto& [y, r] : rows) {
    std::sort(r.begin(), r.end());
    m.area += (int)r.size();
    if ((int)r.size() >= 3 && m.hipRow == -99) { m.hipRow = -y; m.hipW = (int)r.size(); }
    int run = 0, last = 1 << 30;
    bool boot = false;
    for (size_t q = 0; q <= r.size(); q++) {
      if (q < r.size() && (run == 0 || r[q].first == last + 1)) { run++; boot = boot || bootc(r[q].second); last = r[q].first; continue; }
      // (integration 2026-10-23) a run the garment's own cover bounds on both sides is a gap in the hem, not a leg sliver;
      // on a fringe row (the cover in 2+ pieces: the rags' tatters) a run beside a strand is the fringe's, too
      const bool lc = cover.count({last - run, y}) > 0, rc = cover.count({last + 1, y}) > 0;
      const bool notch = run && ((lc && rc) || ((lc || rc) && fringe(y)));
      if (run && !boot && !notch && run < m.minRun) { m.minRun = run; m.runU = last - run + 1; m.runY = y; }
      run = 0; boot = false;
      if (q < r.size()) { run = 1; boot = bootc(r[q].second); last = r[q].first; }
    }
  }
  return m;
}
std::vector<std::string> jointFaults(const PX& leg) {
  std::vector<std::string> out;
  if (leg.empty()) return {"no leg"};
  PS all = keysOf(leg);
  // one 4-connected piece
  PS seen; int pieces = 0;
  for (const K& k : all) {
    if (seen.count(k)) continue;
    pieces++;
    std::vector<K> st{k}; seen.insert(k);
    while (!st.empty()) {
      K q = st.back(); st.pop_back();
      for (K n : {K{q.first + 1, q.second}, K{q.first - 1, q.second}, K{q.first, q.second + 1}, K{q.first, q.second - 1}})
        if (all.count(n) && !seen.count(n)) { seen.insert(n); st.push_back(n); }
    }
  }
  if (pieces != 1) out.push_back(fmt("%d pieces", pieces));
  std::set<int> ys;
  for (auto& k : all) ys.insert(k.second);
  int empty = 0;
  for (int y = *ys.begin(); y <= *ys.rbegin(); y++) empty += ys.count(y) ? 0 : 1;
  if (empty) out.push_back(fmt("%d empty rows", empty));
  auto touches = [&](const char* a, const char* b) {
    for (auto& [k, c] : leg) {
      if (!std::strchr(a, reg(c))) continue;
      for (K n : {K{k.first + 1, k.second}, K{k.first - 1, k.second}, K{k.first, k.second + 1}, K{k.first, k.second - 1}}) {
        auto it = leg.find(n);
        if (it != leg.end() && std::strchr(b, reg(it->second))) return true;
      }
    }
    return false;
  };
  bool hasB = false, hasS = false, hasT = false;
  for (auto& [k, c] : leg) { hasB = hasB || bootc(c); hasS = hasS || reg(c) == 'S'; hasT = hasT || reg(c) == 'T'; }
  if (hasB && !touches("BO", "SK")) out.push_back("the boot touches no shin");
  if (hasS && !touches("S", "KT")) out.push_back("the shin hangs from no thigh");
  if (hasT) {
    bool top = false;
    for (auto& [k, c] : leg) top = top || (k.second == *ys.begin() && reg(c) == 'T');
    if (!top) out.push_back("the thigh does not reach the leg's top row");
  }
  return out;
}

void kept(const JLook& L, JFrame& f, const JFrame& idle, const Mass& im, bool tips) {
  Results& out = f.res;
  const Mass m = massOf(L, f);
  if (tips) {
    const double r = im.skirt ? (double)m.skirt / im.skirt : 1;
    out.set("AREA", r >= 0.895, fmt("skirt %d / idle %d = %.0f%% (>= 89.5%%)", m.skirt, im.skirt, 100 * r));
  } else {
    const double r = im.area ? (double)m.area / im.area : 0;
    out.set("AREA", r >= L.T->area, fmt("legs %d / idle %d = %.0f%% (>= %.1f%%)", m.area, im.area, 100 * r, 100 * L.T->area));
    // (the run's lean turns the hips: the profile under the hem may read 2 px narrower than the idle's; the walk 1)
    const std::string lk = legsKeyOf(f.keys);
    const int tol = isRunGait(f.gait) || (isPostureGait(f.gait) && (lk.rfind("run", 0) == 0 || lk == "hop")) ? 2 : 1;
    out.set("HIPW", m.hipW >= im.hipW - tol, fmt("hip row %d px at F-%d (idle %d, at most %d narrower)", m.hipW, m.hipRow, im.hipW, tol));
    out.set("SLIVER", m.minRun >= SLIVER, m.minRun == 99 ? std::string("no short run") : fmt("shortest visible run %d px at u%d y%d (>= %d)", m.minRun, m.runU, m.runY, SLIVER));
  }
  out.set("SQUAT", f.bob <= 1, fmt("bob %+d (<= +1)", f.bob));
  {
    std::vector<std::string> jf;
    if (f.lmode == "layers") {
      for (auto& x : jointFaults(f.lnear)) jf.push_back("n " + x);
      for (auto& x : jointFaults(f.lfar)) jf.push_back("f " + x);
    } else if (f.lmode == "palette") {
      PX u = f.lnear;
      for (auto& [k, c] : f.lfar) u[k] = c;
      for (auto& x : jointFaults(u)) if (x.find("pieces") == std::string::npos) jf.push_back(x);
    } else if (!tips) jf.push_back("no legs");
    if (!tips) out.set("JOINTS", jf.empty(), jf.empty() ? std::string("each leg one piece, boot-shin-thigh joined") : join(jf, ", "));
  }
  {
    int g = 0;
    if (tips) {   // the shoe tips under the hem
      for (auto& [id, px] : f.layers) {
        const JPart* p = L.part(id);
        // (integration 2026-10-23) the tips part counts on whatever layer its view draws (ADULT-RIG's side walk / run tips
        // draw on layer belt, over the long skirts)
        const bool tipsPart = p && std::find(p->roles.begin(), p->roles.end(), std::string("legs tips")) != p->roles.end();
        if (p && (p->layer == "legs" || tipsPart)) for (auto& [k, c] : px) g += k.second == 0 && bootc(c);
      }
    } else {
      for (auto& [k, c] : f.lnear) g += k.second == 0 && bootc(c);
      for (auto& [k, c] : f.lfar) g += k.second == 0 && bootc(c);
    }
    const std::string lk = legsKeyOf(f.keys);
    const bool flight = isPostureGait(f.gait) ? (lk == "run3" || lk == "run6" || lk == "hop") : isRunGait(f.gait) && (f.i % 3 == 2);
    out.set("GROUND", flight ? g == 0 : g > 0, flight ? fmt("%d sole px on the ground row in the flight (0)", g) : fmt("%d sole px on the ground row (> 0)", g));
  }
  (void)idle;
}

const char* const kChecks[] = {"FARWIDTH", "LEGLEN", "THICK", "HEAD", "BUNCH", "REG", "HIPLINE", "ANKLE", "BOOT", "FLIGHT", "PUSH", "FOLD", "BOOTTILT",
                               "POST", "BOOTSIZE", "BOB", "RHYTHM", "AREA", "HIPW", "SLIVER", "SQUAT", "JOINTS", "GROUND"};
bool isB12(const std::string& c) { return c == "BOOT" || c == "BOOTTILT" || c == "BOOTSIZE"; }
bool isOracle(const std::string& c) {
  for (int k = 0; k < 17; k++) if (c == kChecks[k]) return true;
  return false;
}
bool tipsLook(const JLook& L) {
  for (auto& p : L.parts) if (p.layer == "legs" && hasRoleSuffix(p, " tips")) return true;
  for (auto& p : L.parts) if (p.name.find("/tips") != std::string::npos) return true;
  return false;
}

// judge one look: every side frame's results (check.py judge(), plus the re-homed checks); lines: check.py's report
struct Judged { std::map<std::string, int> fails, total; std::vector<std::string> lines; std::vector<Fail> failList; };
const char* const kGaitOrder[] = {"walk", "run", "dwalk", "drun", "carry", "carry1", "carry2", "play", "dance"};
void judgeLook(JLook& L, Judged& J, bool extra, bool quiet) {
  const BodyT& T = *L.T;
  const bool tips = tipsLook(L);
  for (char fac : {'R', 'L'}) {
    Ref ref;
    if (!idleRef(L, fac, ref)) { J.lines.push_back(L.tag + " " + fac + ": no idle"); continue; }
    if (!quiet) J.lines.push_back(fmt("== %s %c (idle leg width %d, leg length %.1f, boot %d px, head-torso touch %d, legs by %s)", L.tag.c_str(), fac, ref.W, ref.L, ref.boot, ref.touch, ref.mode.c_str()));
    JFrame* idle = nullptr;
    for (JFrame& x : L.frames) if (x.gait == "idle" && x.fac == fac) idle = &x;
    Mass im;
    if (extra && idle) {
      PX n, fa; std::set<int> s; std::string m;
      legsOf(L, *idle, n, fa, s, m);
      im = massOf(L, *idle);
    }
    for (const char* g : kGaitOrder) {
      std::vector<JFrame*> fs;
      for (JFrame& x : L.frames) if (x.gait == g && x.fac == fac) fs.push_back(&x);
      if (fs.empty()) continue;
      std::sort(fs.begin(), fs.end(), [](JFrame* a, JFrame* b) { return a->i < b->i; });
      const bool posture = std::strcmp(g, "walk") && std::strcmp(g, "run") && std::strcmp(g, "dwalk") && std::strcmp(g, "drun");
      for (JFrame* f : fs) checkFrame(L, *f, ref);
      std::map<int, RegR> rg;
      if (!posture) for (const RegR& r : regCheck(fs, T.name == std::string("adult") ? 7 : 6)) rg[r.i] = r;
      bool allPost = true;
      for (JFrame* f : fs) allPost = allPost && f->hasPost;
      if (allPost && !posture) {
        const size_t n = fs.size();
        for (size_t k = 0; k < n; k++) {
          const Post& p = fs[k]->post;
          std::vector<std::string> pb;
          if (p.isp && (fs[(k + n - 1) % n]->post.isp || fs[(k + 1) % n]->post.isp)) pb.push_back("a post two frames in a row");
          if (p.hasLb && p.lb < T.postLead) pb.push_back(fmt("flight lead bend %.0f < %.0f", p.lb, T.postLead));
          fs[k]->res.set("POST", pb.empty(), p.t + (pb.empty() ? "" : " " + join(pb, ", ")));
        }
      }
      if (!posture) {
        int bmin = 1 << 20, bmax = -(1 << 20);
        std::string bobs;
        for (JFrame* f : fs) { bmin = std::min(bmin, f->bob); bmax = std::max(bmax, f->bob); bobs += (bobs.empty() ? "" : "/") + fmt("%+d", f->bob); }
        const int brange = bmax - bmin, bmaxAllowed = isRunGait(g) ? 2 : 1;
        bool allPl = true;
        for (JFrame* f : fs) allPl = allPl && f->hp0 && f->hp1;
        if (isRunGait(g) && allPl) {
          double hmin = 1e18, hmax = -1e18;
          std::string hs;
          for (JFrame* f : fs) { const double h = f->plant0 - f->plant1; hmin = std::min(hmin, h); hmax = std::max(hmax, h); hs += (hs.empty() ? "" : "/") + fmt("%.0f", h); }
          const double hr = hmax - hmin;
          for (JFrame* f : fs) f->res.set("RHYTHM", hr <= RHYTHM_MAX, fmt("hold %.0f px, cycle %s spread %.0f (<= %d)", f->plant0 - f->plant1, hs.c_str(), hr, RHYTHM_MAX));
        }
        for (JFrame* f : fs) f->res.set("BOB", brange <= bmaxAllowed, fmt("bob %+d, cycle %s range %d (<= %d)", f->bob, bobs.c_str(), brange, bmaxAllowed));
      }
      for (JFrame* f : fs) {
        auto ri = rg.find(f->i);
        if (ri != rg.end()) f->res.set("REG", ri->second.ok, ri->second.t);
        if (extra && idle) kept(L, *f, *idle, im, tips);
        if (posture) {   // the walking postures: leg checks only (5.5 M6: no REG, BOB, RHYTHM; time-keyed)
          Results r;
          // (gates-wear fix round 1) a posture frame on the standing legs (Dance's turn: legs=idle) shows the idle's own
          // drawing (B1 parity: the far leg stands behind the near one): the far leg and boot measures are the approved
          // stance's, not a step's; LEGLEN, HIPLINE, HEAD, SQUAT, GROUND and the mass checks still judge the body on them
          const bool standing = legsKeyOf(f->keys) == "idle";
          for (auto& e : f->res.v) {
            if (e.first == "REG" || e.first == "BOB" || e.first == "RHYTHM" || e.first == "POST") continue;
            if (standing && (e.first == "FARWIDTH" || e.first == "THICK" || e.first == "ANKLE" || e.first == "BUNCH" || isB12(e.first))) continue;
            r.v.push_back(e);
          }
          f->res = r;
        }
        if (tips) {      // a long garment: only shoe tips show under it (B11 wear T1 T2 judge them): no leg to measure
          Results r;
          for (auto& e : f->res.v)
            if (e.first == "HEAD" || e.first == "BOB" || e.first == "RHYTHM" || e.first == "AREA" || e.first == "SQUAT" || e.first == "GROUND") r.v.push_back(e);
          f->res = r;
        }
        std::vector<std::string> bad;
        for (auto& [c, r] : f->res.v) {
          if (!extra && !isOracle(c)) continue;
          J.total[c]++;
          if (!r.ok) {
            J.fails[c]++;
            bad.push_back(c);
            J.failList.push_back({c, fmt("%s %s %c%d", L.tag.c_str(), g, fac, f->i), r.t});
          }
        }
        if (!quiet) {
          auto ki = f->keys.find("legs");
          J.lines.push_back(fmt("%s %s %c%d [%s] ", L.tag.c_str(), g, fac, f->i, ki == f->keys.end() ? "?" : ki->second.c_str()) + (bad.empty() ? "ok" : "FAIL " + join(bad, ",")));
          for (auto& [c, r] : f->res.v) {
            if (!extra && !isOracle(c)) continue;
            J.lines.push_back(fmt("    %-8s %s %s", c.c_str(), r.ok ? "ok  " : "FAIL", r.t.c_str()));
          }
        }
      }
    }
  }
}
std::vector<std::string> asciiLegs(const JFrame& f) {
  std::vector<std::string> out;
  PS pts = keysOf(f.lnear);
  for (auto& [k, c] : f.lfar) pts.insert(k);
  if (pts.empty()) return {"    (no legs)"};
  int u0 = 1 << 20, u1 = -(1 << 20), y0 = 1 << 20, y1 = -(1 << 20);
  for (auto& k : pts) { u0 = std::min(u0, k.first); u1 = std::max(u1, k.first); y0 = std::min(y0, k.second); y1 = std::max(y1, k.second); }
  out.push_back(fmt("    legs by %s; u %d..%d (forward ->); N near, F far, X both; region + ramp step", f.lmode.c_str(), u0, u1));
  for (int y = y0; y <= y1; y++) {
    std::string s = fmt("    %4d ", y);
    for (int u = u0; u <= u1; u++) {
      auto ni = f.lnear.find({u, y}), fi = f.lfar.find({u, y});
      const bool n = ni != f.lnear.end(), fa = fi != f.lfar.end();
      const int c = n ? ni->second : fa ? fi->second : 0;
      s += (n && fa) ? 'X' : n ? 'N' : fa ? 'F' : '.';
      s += c ? std::string(1, reg(c)) + std::to_string(stepOf(c)) : std::string("  ");
    }
    out.push_back(s);
  }
  return out;
}
// ---- the self-test (check.py --synth, ported; adult, youth and child scale, both facings)
// A synthetic GOOD look on a real look's dump: the idle's head, torso and arms, a synthetic coat on the torso layer, and
// every walk and run frame's legs redrawn as two whole legs on two layers (draw_leg: hips under the coat, the far leg drawn
// first, a value darker, a pixel wider), every leg part of the look removed first; the kin also get synthetic idle legs
// (their own idle is the old one-part drawing). Each failure is that look with one fault cut in: it must fail exactly its
// own check(s), in both facings, and its failure text must name the fault.
struct SLeg {
  bool fold = false;
  Pt H, K, A;
  bool hasK = false, flat = false, bar = false;   // flat: rows not widened for the bone's lean; bar: drawn row by row
  int heel = 0, sole = 0;
  std::string stamp;   // "" the scale's flat key; "K3@back": the other facing's entry (pointing back)
  std::vector<std::tuple<int, int, int, char>> rows;   // fold: (y, u0, u1, region) from the coat's middle
  int fu = 0, fy = 0;                                    // fold: the boot's origin (u from the coat's middle, y)
};
SLeg leg(Pt H, Pt A, int heel, int sole) { SLeg l; l.H = H; l.A = A; l.heel = heel; l.sole = sole; return l; }
SLeg legK(Pt H, Pt K, Pt A, int heel, int sole) { SLeg l = leg(H, A, heel, sole); l.K = K; l.hasK = true; return l; }
struct Scale {
  const char* name;
  const BodyT* T;
  Look base;
  bool synthIdle;
  int band, coatHalf;
  char key;   // the stamp library's key letter (K Y C)
  int hip;    // the hip row (the body's hip anchor, bob 0)
};
const Stamp* stampOf(const Scale& S, const std::string& name, bool near, char fac) {
  const std::string base = name.substr(0, name.find('@'));
  const char ef = name.find('@') == std::string::npos ? fac : (fac == 'R' ? 'L' : 'R');
  const StampLib& lib = parseStamps(kRootMaps, S.T->stamps);
  auto it = lib.find({base + (near ? "n" : "f"), std::string(1, ef)});
  return it == lib.end() ? nullptr : &it->second;
}
void layStamp(PX& px, const Stamp* sp, int u0, int y0, char fac) {
  if (!sp) return;
  for (auto& [c, ch] : sp->ch) {
    const int du = c.first - sp->ox;
    auto r = sp->rg.find(c);
    const int st = stepOfCh(ch);
    if (r == sp->rg.end() || st < 0) continue;
    px[{u0 + (fac == 'R' ? du : -du), y0 + c.second - sp->oy}] = codeOf(r->second, st);
  }
}
PX drawLeg(const Scale& S, Pt H, Pt K, Pt A, int heel, int sole, bool near, char fac, const std::string& stamp, int top, int minw, bool flat = false) {
  PX px;
  auto inner = [&](int k) { return near ? (k == 1 ? 3 : 2) : 1; };
  auto bone = [&](Pt a, Pt b, char rg, bool hasFirst, int first) {
    const double dx = b.u - a.u, dy = b.y - a.y;
    if (std::abs(dy) >= std::abs(dx)) {
      const double wr = S.band / std::cos(std::atan2(std::abs(dx), std::abs(dy)));
      const int w = (flat && rg == 'T') ? minw : std::max(minw, S.key == 'K' ? pyround(wr) : (int)std::ceil(wr - 0.05));   // (a 4 px band keeps 3.8 across)
      const int sg = dy > 0 ? 1 : -1;
      const int y0 = hasFirst ? first : pyround(a.y), y1 = pyround(b.y);
      for (int y = y0; sg > 0 ? y <= y1 : y >= y1; y += sg) {
        const double c = a.u + (y - a.y) * dx / dy;
        const int x0 = pyround(c - (w - 1) / 2.0);
        for (int k = 0; k < w; k++) px[{x0 + k, y}] = codeOf(rg, (k == 0 || k == w - 1) ? 0 : inner(k));
      }
    } else {
      const int h = std::max(minw, pyround(S.band / std::cos(std::atan2(std::abs(dy), std::abs(dx)))));
      const int sg = dx > 0 ? 1 : -1;
      for (int u = pyround(a.u); sg > 0 ? u <= pyround(b.u) : u >= pyround(b.u); u += sg) {
        const double c = a.y + (u - a.u) * dy / dx;
        const int y0 = pyround(c - (h - 1) / 2.0);
        for (int k = 0; k < h; k++) px[{u, y0 + k}] = codeOf(rg, (k == 0 || k == h - 1) ? 0 : inner(k));
      }
    }
  };
  bone(H, K, 'T', std::abs(K.y - H.y) >= std::abs(K.u - H.u), top + 1);
  bone(K, A, 'S', false, 0);
  // the knee: the pixels within 1.6 of K (check.py's draw_leg); a 4 px band's knee as its whole row (a partial knee row
  // reads a 4 px thigh thinner than it is)
  for (auto& [k, c] : px) if (S.key == 'K' ? dist(k, K) <= 1.6 : k.second == pyround(K.y)) c = codeOf('K', stepOf(c));
  layStamp(px, stampOf(S, stamp.empty() ? std::string(1, S.key) + "0" : stamp, near, fac), heel, sole, fac);
  return px;
}
PX drawFold(const Scale& S, int c, bool near, char fac, const SLeg& l) {
  std::map<K, char> rg;
  for (auto& [y, u0, u1, r] : l.rows) for (int u = c + u0; u <= c + u1; u++) rg[{u, y}] = r;
  PX out;
  for (auto& [k, r] : rg) {
    const bool edge = !rg.count({k.first - 1, k.second}) || !rg.count({k.first + 1, k.second});
    out[k] = codeOf(r, edge ? 0 : (near ? 2 : 1));
  }
  layStamp(out, stampOf(S, l.stamp, near, fac), c + l.fu, l.fy, fac);
  return out;
}
// a short leg (the child's: thigh row, knee row, shin rows) drawn row by row from the hip to the ankle, each row centred on
// the hip-ankle line and widened for its lean (a bone too short for draw_leg's two bones)
PX drawRows(const Scale& S, Pt H, Pt A, int heel, int sole, bool near, char fac, const std::string& stamp, int minw) {
  PX px;
  const int y0 = pyround(H.y), y1 = pyround(A.y);
  const double slope = y1 != y0 ? (A.u - H.u) / (A.y - H.y) : 0.0;
  const int w = std::max(minw, (int)std::ceil(S.band * std::sqrt(1 + slope * slope) - 0.05));
  for (int y = y0; y <= y1; y++) {
    const double c = H.u + (y - H.y) * slope;
    const int x0 = pyround(c - (w - 1) / 2.0);
    const char rg = y1 == y0 ? 'K' : y == y0 ? 'T' : (y == y0 + 1 && y1 - y0 >= 2) ? 'K' : 'S';
    for (int k = 0; k < w; k++) px[{x0 + k, y}] = codeOf(rg, (k == 0 || k == w - 1) ? 0 : (near ? (k == 1 ? 3 : 2) : 1));
  }
  layStamp(px, stampOf(S, stamp.empty() ? std::string(1, S.key) + "0" : stamp, near, fac), heel, sole, fac);
  return px;
}
PX synthDraw(const Scale& S, const SLeg& l, bool near, char fac, int c) {
  if (l.fold) return drawFold(S, c, near, fac, l);
  if (S.key == 'C' || l.bar) return drawRows(S, l.H, l.A, l.heel, l.sole, near, fac, l.stamp, near ? S.band : S.band + 2);
  const Pt K = l.hasK ? l.K : Pt{(l.H.u + l.A.u) / 2, (l.H.y + l.A.y) / 2 + 0.5};
  return drawLeg(S, l.H, K, l.A, l.heel, l.sole, near, fac, l.stamp, pyround(l.H.y) - 1, near ? S.band : S.band + (S.key == 'K' ? 1 : 2), l.flat);
}
// the folded trailing flight leg (hand-placed: thigh back, the shin a rising bar behind it, a toe-down-back boot under it)
SLeg synthFold(const Scale& S, const char* boot) {
  SLeg l;
  l.fold = true;
  l.stamp = std::string(1, S.key) + boot;
  const int h = S.hip - 2;   // the flight's hip row (bob -2)
  if (S.key == 'K') {        // check.py's: rows -12..-7 at the man's hip -10
    for (int y = h + 1; y <= h + 5; y++) l.rows.push_back({y, -9, -5, 'S'});
    l.rows.push_back({h, -5, 0, 'T'}); l.rows.push_back({h + 1, -5, 0, 'T'}); l.rows.push_back({h + 2, -6, -1, 'T'});
    l.rows.push_back({h + 3, -6, -1, 'K'}); l.rows.push_back({h + 4, -6, -2, 'K'});
    l.fu = -6; l.fy = -3;
  } else {                   // the youth (4 px band, an 8-row leg): the shin a rising bar behind the knee
    for (int y = h; y <= h + 3; y++) l.rows.push_back({y, -8, -5, 'S'});
    l.rows.push_back({h, -4, 0, 'T'}); l.rows.push_back({h + 1, -5, -1, 'T'});
    l.rows.push_back({h + 2, -5, -1, 'K'}); l.rows.push_back({h + 3, -5, -2, 'K'});
    l.fu = -7; l.fy = -2;
  }
  return l;
}
struct SPose { SLeg n, f; double p0, p1; int bob; };
SPose synthPose(const Scale& S, const std::string& g, int i, int c) {
  const int H = S.hip;
  const bool kin = S.key != 'K';
  if (g == "walk" && S.key == 'C') {
    const int p0 = 1 - i % 3;
    return {leg({(double)c + 1, (double)H}, {(double)p0 + c + 1, -3}, p0 + c - 1, 0), leg({(double)c - 2, (double)H}, {(double)p0 + c - 2, -3}, p0 + c - 4, 0), (double)p0, (double)p0 - 1, 0};
  }
  if (g == "walk") {
    const int p0 = 1 - i % 3;
    return {leg({(double)c + 1, (double)H}, {(double)p0 + 2, -3}, p0, 0), leg({(double)c - 2, (double)H}, {(double)p0 - 1, -3}, p0 - 3, 0), (double)p0, (double)p0 - 1, 0};
  }
  const int r = i % 3, kd = kin ? 1 : 0;   // the youth's knees a row lower (a 3-row thigh)
  if (r == 0)
    return {legK({(double)c + 1, (double)H}, {(double)c + 2, (double)H + 4 - kd + kd * 0}, {4.0 - kd, -3}, 2 - kd, 0),
            legK({(double)c - 2, (double)H}, {(double)c - 3, (double)H + 4 - kd}, {(double)c - 5 + kd, -4}, c - 7 + kd, -1), (double)(2 - kd), (double)(-3 - kd), 0};
  if (r == 1)
    return {leg({(double)c + 2 + kd, (double)H}, {-1, -3}, -3, 0), legK({(double)c + 4 + kd, (double)H}, {(double)c + 6, (double)H + 3}, {(double)c + 7 - kd, -4}, c + 5 - kd, -1),
            (double)(-3 - kd), (double)(-8 - kd), 0};
  if (!kin) return {legK({(double)c + 1, (double)H - 2}, {(double)c + 3, H + 1.5}, {(double)c + 4, (double)H + 4}, c + 2, -3), synthFold(S, "3@back"), -8, -13, -2};
  return {legK({(double)c + 1, (double)H - 2}, {c + 3.5, H + 0.5}, {(double)c + 2, (double)H + 2}, c, -3), synthFold(S, "2@back"), -9, -14, -2};
}
bool isLegPart(const JLook& L, const std::string& id) { const JPart* p = L.part(id); return p && p->layer == "legs"; }
PX withoutLegs(const PX& raw) { PX o; for (auto& [k, c] : raw) if (!legc(c)) o[k] = c; return o; }
// the frame's whole figure (raw codes) painted again from its layers: by layer, then z, then the recipe's order
void recompose(const JLook& L, JFrame& f) {
  std::vector<std::tuple<int, int, int, const PX*>> order;
  for (auto& [id, px] : f.layers) {
    const JPart* p = L.part(id);
    order.push_back({p ? layerIndex(p->layer) : -1, p ? p->z : 0, L.order(id), &px});
  }
  std::stable_sort(order.begin(), order.end(), [](auto& a, auto& b) { return std::make_tuple(std::get<0>(a), std::get<1>(a), std::get<2>(a)) < std::make_tuple(std::get<0>(b), std::get<1>(b), std::get<2>(b)); });
  PX raw;
  for (auto& e : order) for (auto& [k, c] : *std::get<3>(e)) raw[k] = c;
  f.raw = cropped(raw);
}

struct Synth { JLook look; std::set<std::string> fail; std::map<std::string, std::string> why; };
struct Built { JLook good; std::map<char, int> cm; };
Built buildGood(const Scale& S, const JLook& base) {
  Built B;
  JLook& good = B.good;
  good = base;
  good.frames.clear();
  for (const JFrame& f : base.frames) if (f.gait == "idle" || f.gait == "walk" || f.gait == "run") good.frames.push_back(f);
  for (JFrame& f : good.frames) {
    std::vector<std::pair<std::string, PX>> keep;
    for (auto& e : f.layers) {
      const JPart* p = base.part(e.first);
      const std::string ly = p ? p->layer : "";
      if (S.synthIdle && S.key != 'K' && (ly == "hangBack" || ly == "hangFront" || ly == "shoes" || ly == "belt")) continue;
      keep.push_back(e);
    }
    f.layers = keep;
  }
  good.parts.push_back({"9999", "synthetic/far_leg", "legs", -1, {"legs std"}});
  good.parts.push_back({"9998", "synthetic/coat", "torso", 5, {"garment coat"}});
  for (char fac : {'R', 'L'}) {
    JFrame* fi = frameOf(good, "idle", fac, 0);
    if (!fi) continue;
    const Sil s = silhouette(good, *fi);
    const int c = floordiv(s.hemSpan.first + s.hemSpan.second, 2);
    B.cm[fac] = c;
    if (std::getenv("SYNTH_DUMP")) std::printf("  synth %s %c: coat middle u%d, hem y%d span %d..%d, hip y%d\n", S.name, fac, c, s.hem, s.hemSpan.first, s.hemSpan.second, S.hip);
    PX coat;
    for (int u = c - S.coatHalf; u <= c + S.coatHalf; u++)
      for (int y = s.hem - 4; y <= s.hem; y++) coat[{u, y}] = codeOf('T', 2);
    fi->setLayer("9998", cropped(coat));
    if (S.synthIdle) {   // the kin: two whole idle legs on two layers
      std::vector<std::string> legIds;
      for (auto& [id, px] : fi->layers) if (isLegPart(base, id)) legIds.push_back(id);
      const std::string lid = legParts(base, *fi).empty() ? std::string("9997") : legParts(base, *fi).back();
      for (auto& id : legIds) fi->eraseLayer(id);
      const PX nl = synthDraw(S, leg({(double)c + 1, (double)S.hip}, {(double)c + 1, -3}, c - 1, 0), true, fac, c);
      const PX fl = synthDraw(S, leg({(double)c - 2, (double)S.hip}, {(double)c - 2, -3}, c - 4, 0), false, fac, c);
      fi->setLayer(lid, cropped(nl));
      fi->setLayer("9999", cropped(fl));
    }
  }
  for (JFrame& f : good.frames) {
    if (f.gait == "idle") continue;
    const char fac = f.fac;
    const JFrame* fi = frameOf(good, "idle", fac, 0);
    const std::vector<std::string> lp = legParts(base, f);
    const std::string lid = lp.empty() ? std::string("9997") : lp.back();
    // the body above the legs as the idle's, moved by the frame's bob (the run's flight lifts it 2 px, as the game does)
    const SPose p = synthPose(S, f.gait, f.i, B.cm[fac]);
    auto lift = [&](const PX& px) { PX o; for (auto& [k, c] : px) o[{k.first, k.second + p.bob}] = c; return cropped(o); };
    std::vector<std::pair<std::string, PX>> keep;
    for (auto& [id, px] : f.layers) {
      if (isLegPart(base, id)) continue;
      const PX* ip = fi->layer(id);
      keep.push_back({id, lift(ip ? *ip : px)});
    }
    f.layers = keep;
    f.setLayer("9998", lift(*fi->layer("9998")));
    f.headX = f.headY = 0; f.bob = p.bob; f.hipY = 0; f.lean = 0;
    f.hp0 = f.hp1 = true; f.plant0 = p.p0; f.plant1 = p.p1;
    const PX nl = cropped(synthDraw(S, p.n, true, fac, B.cm[fac])), fl = cropped(synthDraw(S, p.f, false, fac, B.cm[fac]));
    f.setLayer(lid, nl);
    f.setLayer("9999", fl);
  }
  for (JFrame& f : good.frames) recompose(good, f);
  return B;
}
std::string nearIdOf(const JLook& L, const JFrame& f) { const auto ids = legParts(L, f); return ids.empty() ? std::string() : ids.back(); }
void editLayer(JLook& L, const std::string& g, char fac, int i, const std::string& lid, const std::function<PX(const PX&)>& move) {
  JFrame* f = frameOf(L, g, fac, i);
  if (!f || !f->layer(lid)) return;
  f->setLayer(lid, cropped(move(*f->layer(lid))));
  recompose(L, *f);
}
void redraw(const Scale& S, JLook& L, const std::string& g, char fac, int i, char which, const SLeg& spec, int c) {
  JFrame* f = frameOf(L, g, fac, i);
  if (!f) return;
  const std::string lid = which == 'n' ? nearIdOf(L, *f) : std::string("9999");
  f->setLayer(lid, cropped(synthDraw(S, spec, which == 'n', fac, c)));
  recompose(L, *f);
}
int minShaftU(const PX& px) { int m = 1 << 20; for (auto& [k, c] : px) if (shaft(c)) m = std::min(m, k.first); return m; }

struct Cut { std::string name; std::set<std::string> fail; std::map<std::string, std::string> why; bool run; std::function<void(JLook&, char, int)> fn; std::set<std::string> allow; };
std::vector<Cut> cutsFor(const Scale& S) {
  std::vector<Cut> v;
  const int H = S.hip;
  const bool kin = S.key != 'K';
  const double kd = kin ? 1 : 0;
  auto add = [&](const char* name, std::set<std::string> fail, bool run, std::function<void(JLook&, char, int)> fn, std::map<std::string, std::string> why = {}) {
    v.push_back({name, fail, why, run, fn});
  };
  const Scale* SP = &S;
  auto torsoTrim = [](JLook& lk, JFrame* f, bool back, int deep) {   // the torso layers cut back to the legs' own edge (the hips' line moved)
    const Sil s = silhouette(lk, *f);
    int edge = back ? (1 << 20) : -(1 << 20);
    for (auto& id : legParts(lk, *f))
      for (auto& [k, c] : *f->layer(id))
        if (k.second <= s.hem + 3) edge = back ? std::min(edge, k.first) : std::max(edge, k.first);
    for (auto& [id, px] : f->layers) {
      const JPart* p = lk.part(id);
      if (!p || p->layer != "torso") continue;
      PX o;
      for (auto& [k, c] : px) if (k.second < s.hem - 1 || (back ? k.first > edge + deep : k.first < edge - deep)) o[k] = c;
      px = o;
    }
    recompose(lk, *f);
  };
  // FARWIDTH: the far leg drawn a pixel narrower (its back column gone)
  add("far leg narrower (its back columns gone)", {"FARWIDTH"}, false, [kin](JLook& lk, char fac, int) {
    const int cols = kin ? 2 : 1;   // (the kin's far leg is drawn 2 px wider than the near: 1 px less still passes)
    editLayer(lk, "walk", fac, 2, "9999", [cols](const PX& px) { const int m = minShaftU(px); PX o; for (auto& [k, c] : px) if (!(shaft(c) && k.first < m + cols)) o[k] = c; return o; });
  });
  // FARWIDTH: the far leg 1 px nearer the near leg: only a 2 px sliver shows past it
  if (S.key != 'C') add("far leg showing a 2 px sliver", {"FARWIDTH"}, false, [](JLook& lk, char fac, int) {
    editLayer(lk, "walk", fac, 0, "9999", [](const PX& px) { PX o; for (auto& [k, c] : px) o[{k.first + 1, k.second}] = c; return o; });
  });
  // LEGLEN: a walk frame's legs hung 2 px below its hip (the clip's hip 2 px up, the legs drawn as before: both read 2 px
  // longer along their bones)
  add("walk legs hung 2 px below the hip", {"LEGLEN"}, false, [](JLook& lk, char fac, int) { frameOf(lk, "walk", fac, 1)->hipY -= 2; }, {{"LEGLEN", "stretched"}});
  // LEGLEN: a straight near leg drawn short (its boot lifted under a straight leg: shortened, not a bend)
  {
    const int sh = 3;
    add("near leg drawn short (straight)", {"LEGLEN"}, false, [SP, H, sh](JLook& lk, char fac, int c) {
      if (SP->key == 'C') redraw(*SP, lk, "walk", fac, 1, 'n', leg({(double)c + 1, (double)H}, {(double)c + 1, (double)H}, c - 1, -2), c);
      else if (SP->key != 'K') { SLeg l = leg({(double)c + 1, (double)H}, {(double)c + 1, (double)H + 2}, c + 1, H + 5); l.bar = true; redraw(*SP, lk, "walk", fac, 1, 'n', l, c); }
      else redraw(*SP, lk, "walk", fac, 1, 'n', leg({(double)c + 1, (double)H}, {2, -3.0 - sh}, 0, -sh), c);
    });
    if (S.key != 'K') v.back().allow = {"AREA", "THICK"};   // (a short leg is less leg, its short shin read by its boot: the kin's small legs may show it in AREA and THICK too)
  }
  // THICK: the near thigh leaning 45 degrees with its rows not widened (5 px rows read 3.5 across)
  if (S.key != 'C')
    add("near thigh leaning 45 deg, rows not widened", {"THICK"}, false, [SP, H, kin](JLook& lk, char fac, int c) {
      SLeg l = kin ? legK({(double)c + 1, (double)H}, {(double)c + 3, (double)H + 2}, {3, -4}, 1, -1) : legK({(double)c + 1, (double)H}, {(double)c + 3, (double)H + 2}, {3, -3}, 1, 0);
      l.flat = true;
      redraw(*SP, lk, "walk", fac, 0, 'n', l, c);
    });
  // HEAD: a pixel forward (headX 1 and the head's layers moved); lifted 2 (a gap)
  auto head = [](JLook& lk, char fac, const std::string& g, int du, int dy, int hx) {
    JFrame* f = frameOf(lk, g, fac, 0);
    if (!f) return;
    for (auto& [id, px] : f->layers) {
      const JPart* p = lk.part(id);
      if (!p || !HEADL.count(p->layer)) continue;
      PX o;
      for (auto& [k, c] : px) o[{k.first + du, k.second + dy}] = c;
      px = cropped(o);
    }
    f->headX = hx;
    recompose(lk, *f);
  };
  add("head a pixel forward", {"HEAD"}, false, [head](JLook& lk, char fac, int) { head(lk, fac, frameOf(lk, "run", fac, 0) ? "run" : "walk", 1, 0, 1); });
  add("head lifted 2 (a gap)", {"HEAD"}, false, [head](JLook& lk, char fac, int) { head(lk, fac, frameOf(lk, "run", fac, 0) ? "run" : "walk", 0, -2, 0); });
  // BUNCH: run push with the swinging far leg's boot set down under the support boot's toe (a lump of boots)
  add("run push boots lumped", {"BUNCH"}, true, [SP, H, kd](JLook& lk, char fac, int c) {
    if (kd == 0) redraw(*SP, lk, "run", fac, 1, 'f', legK({(double)c + 4, (double)H}, {(double)c + 6, (double)H + 3}, {2, -3}, 0, 0), c);
    else redraw(*SP, lk, "run", fac, 1, 'f', legK({(double)c + 6, (double)H}, {c + 4.5, H + 2.5}, {(double)c + 3, -3}, c + 1, 0), c);
  });
  // FARWIDTH, one drawing: the same two legs flattened into ONE part (the near leg painted over the far one): the far
  // leg's covered pixels are not drawn, so the far leg reads thin and its boot partial (THICK, BOOT, BOOTSIZE agree)
  add("the good legs flattened into one drawing", {"FARWIDTH"}, false, [](JLook& lk, char fac, int) {
    for (JFrame& f : lk.frames) {
      if (f.gait != "walk" || f.fac != fac || !f.layer("9999")) continue;
      std::string lid;
      for (auto& id : legParts(lk, f)) if (id != "9999") lid = id;
      PX px = *f.layer("9999");
      f.eraseLayer("9999");
      for (auto& [k, c] : *f.layer(lid)) px[k] = c;
      f.setLayer(lid, px);
      recompose(lk, f);
    }
  });
  v.back().allow = {"THICK", "BOOT", "BOOTSIZE"};   // (one drawing: the far leg's covered pixels are not drawn at all)
  // REG: both legs of a stance frame moved 2 px forward (the planted foot skates)
  add("planted foot skates 2 px", {"REG"}, false, [](JLook& lk, char fac, int) {
    JFrame* f = frameOf(lk, "walk", fac, 1);
    for (auto& id : legParts(lk, *f)) editLayer(lk, "walk", fac, 1, id, [](const PX& px) { PX o; for (auto& [k, c] : px) o[{k.first + 2, k.second}] = c; return o; });
  });
  // HIPLINE: the hips' back line (every torso layer) drawn 1 px inside the far thigh's back edge
  add("coat back edge inside the far thigh", {"HIPLINE"}, false, [torsoTrim](JLook& lk, char fac, int) { torsoTrim(lk, frameOf(lk, "walk", fac, 0), true, 6); });
  // HIPLINE: the hips' front line drawn 1 px inside the near thigh's front edge
  add("coat front edge inside the near thigh", {"HIPLINE"}, false, [torsoTrim, SP](JLook& lk, char fac, int) { torsoTrim(lk, frameOf(lk, "walk", fac, 1), false, SP->key == 'C' ? 8 : 0); });
  // ANKLE: the near shin's row over the boot cut to 3 px (walk)
  add("near ankle cut to 3 px", {"ANKLE"}, false, [](JLook& lk, char fac, int) {
    JFrame* f = frameOf(lk, "walk", fac, 1);
    editLayer(lk, "walk", fac, 1, nearIdOf(lk, *f), [](const PX& px) {
      int ys = -(1 << 20);
      for (auto& [k, c] : px) if (reg(c) == 'S') ys = std::max(ys, k.second);
      std::vector<int> us;
      for (auto& [k, c] : px) if (k.second == ys && shaft(c)) us.push_back(k.first);
      std::sort(us.begin(), us.end());
      const int cutN = (int)us.size() - 3;
      PX o;
      for (auto& [k, c] : px) {
        bool drop = false;
        for (int q = 0; q < cutN && q < (int)us.size(); q++) drop = drop || (k.second == ys && k.first == us[q]);
        if (!drop) o[k] = c;
      }
      return o;
    });
  });
  // ANKLE: the far shin drawn 3 px across on its own layer (a far shaft row under the idle's width: FARWIDTH agrees)
  add("far shin 3 px across", {"ANKLE", "FARWIDTH"}, false, [](JLook& lk, char fac, int) {   // (a 3 px shin is thin across too: THICK may agree)
    editLayer(lk, "walk", fac, 2, "9999", [](const PX& px) {
      PX o;
      for (auto& [k, c] : px) {
        if (reg(c) == 'S') {
          std::vector<int> us;
          for (auto& [k2, c2] : px) if (k2.second == k.second && reg(c2) == 'S') us.push_back(k2.first);
          std::sort(us.begin(), us.end());
          const int cutN = (int)us.size() - 3;
          bool drop = false;
          for (int q = 0; q < cutN; q++) drop = drop || k.first == us[q];
          if (drop) continue;
        }
        o[k] = c;
      }
      return o;
    });
  });
  v.back().allow = {"THICK"};
  // BOOT: both boots cut short at the toe (each over 10 % under the idle's boot; the two still alike)
  add("both boots cut short at the toe", {"BOOT"}, false, [kin](JLook& lk, char fac, int) {
    JFrame* f = frameOf(lk, "walk", fac, 1);
    const int cols = kin ? 1 : 2;   // (20 px: 2 columns are 25 %; the kin's 17: 1 column is 12 %)
    for (auto& id : legParts(lk, *f))
      editLayer(lk, "walk", fac, 1, id, [cols](const PX& px) {
        int mu = -(1 << 20);
        for (auto& [k, c] : px) if (bootc(c)) mu = std::max(mu, k.first);
        PX o;
        for (auto& [k, c] : px) if (!(bootc(c) && k.first > mu - cols)) o[k] = c;
        return o;
      });
  }, {{"BOOT", "vs idle"}});
  // BOOT: the far boot covered (a held thing over it in the figure) but a 1 px sliver
  add("far boot covered but a 1 px sliver", {"BOOT"}, false, [](JLook& lk, char fac, int) {
    JFrame* f = frameOf(lk, "walk", fac, 0);
    const PX& nl = *f->layer(nearIdOf(lk, *f));
    PS vis;
    for (auto& [k, c] : *f->layer("9999")) if (bootc(c) && !nl.count(k)) vis.insert(k);
    int u0 = 1 << 20;
    for (auto& k : vis) u0 = std::min(u0, k.first);
    for (auto& k : vis) if (!(k.first == u0 && k.second == 0)) f->raw[k] = 2 + (int)doll::Slot::Cloth1 * 5 + 2;
  }, {{"BOOT", "visible sliver"}});
  // FLIGHT: the flight frame's legs and hip 3 px lower (the soles on the ground: a lunge, not airborne); GROUND agrees
  add("flight soles on the ground", {"FLIGHT", "GROUND"}, true, [](JLook& lk, char fac, int) {
    JFrame* f = frameOf(lk, "run", fac, 2);
    for (auto& id : legParts(lk, *f)) editLayer(lk, "run", fac, 2, id, [](const PX& px) { PX o; for (auto& [k, c] : px) o[{k.first, k.second + 3}] = c; return o; });
    f->hipY += 3;
    f->plant0 += 0.5; f->plant1 += 0.5;   // (no stance runs into a flight frame: REG keeps reading the planted feet only)
  });
  // FLIGHT: the trailing leg a straight hip-to-boot diagonal (no knee: FOLD's knee agrees)
  add("flight trailing leg straight", {"FLIGHT", "FOLD"}, true, [SP, H, kin](JLook& lk, char fac, int c) {
    const Pt h{(double)c - 3, (double)H - 2}, a{(double)c - (kin ? 4 : 5), kin ? -5.0 : -6.0};
    if (kin) redraw(*SP, lk, "run", fac, 5, 'f', legK(h, {(h.u + a.u) / 2, (h.y + a.y) / 2}, a, c - 6, -2), c);   // (the knee on the line)
    else redraw(*SP, lk, "run", fac, 5, 'f', leg(h, a, c - 7, -3), c);
  }, {{"FOLD", "knee bend"}});
  // PUSH: the support leg a vertical post under the hip (one frame: POST allows a single post frame)
  add("push support leg vertical", {"PUSH"}, true, [SP, H](JLook& lk, char fac, int c) {
    redraw(*SP, lk, "run", fac, 4, 'n', leg({-1, (double)H}, {-1, -3}, -3, 0), c);
  });
  // FOLD: the hips' back line drawn inside the folded trailing leg in the flight (HIPLINE agrees: a leg behind the coat)
  add("flight coat back edge inside the trailing leg", {"FOLD", "HIPLINE"}, true, [torsoTrim](JLook& lk, char fac, int) { torsoTrim(lk, frameOf(lk, "run", fac, 2), true, 9); },
      {{"FOLD", "outside the hips"}});
  // BOOTTILT: the push's swinging far boot turned on its end (90 deg: whole and as thick, hanging straight down from its
  // shin, toe to the ground and below)
  add("push far boot turned on end", {"BOOTTILT"}, true, [](JLook& lk, char fac, int) {
    editLayer(lk, "run", fac, 1, "9999", [](const PX& px) {
      PX b, o;
      for (auto& [k, c] : px) (bootc(c) ? b : o)[k] = c;
      int u0 = 1 << 20, y1 = -(1 << 20), ys = -(1 << 20);
      for (auto& [k, c] : b) { u0 = std::min(u0, k.first); y1 = std::max(y1, k.second); }
      for (auto& [k, c] : o) if (reg(c) == 'S') ys = std::max(ys, k.second);
      std::vector<int> us;
      for (auto& [k, c] : o) if (reg(c) == 'S' && k.second == ys) us.push_back(k.first);
      double cu = 0;
      for (int u : us) cu += u;
      const int cx = pyround(cu / std::max<size_t>(1, us.size()));
      for (auto& [k, c] : b) o[{cx - 1 + (y1 - k.second), ys + 1 + (k.first - u0)}] = c;
      return o;
    });
  }, {{"BOOTTILT", "tilt"}});
  // BOOTTILT: the near boot rolled into a slab (its columns stepped down 1 px every 2 toward the toe: as many pixels, its
  // sole sloped 1:2, so 3 rows read 2.7 across)
  add("near boot rolled into a slab", {"BOOTTILT"}, false, [](JLook& lk, char fac, int) {
    const bool run = frameOf(lk, "run", fac, 0) != nullptr;   // (the run's contact: the far boot clear behind)
    const char* g = run ? "run" : "walk";
    const int i = run ? 0 : 2;
    JFrame* f = frameOf(lk, g, fac, i);
    editLayer(lk, g, fac, i, nearIdOf(lk, *f), [run](const PX& px) {
      int u0 = 1 << 20, ys = -(1 << 20), us = -(1 << 20);
      for (auto& [k, c] : px) if (bootc(c)) u0 = std::min(u0, k.first);
      for (auto& [k, c] : px) if (reg(c) == 'S') ys = std::max(ys, k.second);
      for (auto& [k, c] : px) if (reg(c) == 'S' && k.second == ys) us = std::max(us, k.first);
      PX best;
      // (the walk: the columns the shin stands in stay, as many of them as the boot's length allows)
      for (int from : run ? std::vector<int>{u0} : std::vector<int>{us, us - 1, us - 2, u0})
      for (int t = 2; t <= 40 && best.empty(); t++) {
        PX b;
        for (auto& [k, c] : px) if (bootc(c)) b[{k.first, k.second + (int)std::ceil(std::max(0, k.first - from) * t / 10.0 - 1e-9)}] = c;
        const double th = bootThick(b);
        PX whole;
        for (auto& [k, c] : px) if (!bootc(c)) whole[k] = c;
        for (auto& [k, c] : b) whole[k] = c;
        if (th >= 2.52 && th <= 2.68 && jointFaults(whole).empty()) best = b;   // (still one leg, its boot on its shin)
      }
      PX o;
      for (auto& [k, c] : px) if (!bootc(c)) o[k] = c;
      for (auto& [k, c] : best) o[k] = c;
      if (best.empty())
        for (const auto& e : px)
          if (bootc(e.second)) o[e.first] = e.second;
      return o;
    });
  }, {{"BOOTTILT", "slab"}});
  // POST: the contact's support leg a straight plumb post, and the flight before it lands on a straight lead leg (posts
  // in two cyclically adjacent frames)
  add("run legs posts two frames running", {"POST"}, true, [SP, H, kin](JLook& lk, char fac, int c) {
    redraw(*SP, lk, "run", fac, 0, 'n', leg({4.0 - kin, (double)H}, {4.0 - kin, -3}, 2 - kin, 0), c);
    redraw(*SP, lk, "run", fac, 5, 'n', leg({(double)c + 2, (double)H - 2}, {(double)c + 2, -6.0}, c, -3), c);
  }, {{"POST", "two frames in a row"}});
  // POST: the flight lead leg straight (no knee)
  add("flight lead leg straight", {"POST"}, true, [SP, H, kin](JLook& lk, char fac, int c) {
    redraw(*SP, lk, "run", fac, 2, 'n', leg({(double)c + 1, (double)H - 2}, {(double)c + 3, kin ? -5.0 : -6.0}, c + 2, kin ? -2 : -3), c);
  }, {{"POST", "flight lead bend"}});
  // BOOTSIZE: the near boot 2 px longer at its toe, the far 2 px shorter (each within BOOT's 10 % of the idle)
  add("push boots longer and shorter", {"BOOTSIZE"}, true, [kin](JLook& lk, char fac, int) {
    JFrame* f = frameOf(lk, "run", fac, 1);
    const size_t n = kin ? 1 : 2;   // (each within BOOT's 10 % of the idle's boot, the two more than 10 % apart)
    editLayer(lk, "run", fac, 1, nearIdOf(lk, *f), [n](const PX& px) {
      int mu = -(1 << 20);
      for (auto& [k, c] : px) if (bootc(c) && k.second == 0) mu = std::max(mu, k.first);
      PX o = px;
      o[{mu + 1, 0}] = codeOf('O', 0);
      if (n > 1) o[{mu + 1, -1}] = codeOf('B', 0);
      return o;
    });
    editLayer(lk, "run", fac, 1, "9999", [n](const PX& px) {
      std::vector<K> b;
      for (auto& [k, c] : px) if (bootc(c)) b.push_back(k);
      std::sort(b.begin(), b.end(), [](const K& a, const K& c) { return std::make_pair(-a.first, a.second) < std::make_pair(-c.first, c.second); });
      const size_t m = std::min(n, b.size());
      PX o;
      for (auto& [k, c] : px) if (std::find(b.begin(), b.begin() + m, k) == b.begin() + m) o[k] = c;
      return o;
    });
  }, {{"BOOTSIZE", "differ"}});
  // BOB: the run's flight 3 px up (range 3 > 2); the walk's frame 2 px low (range 2 > 1; SQUAT agrees: +2 squats). The
  // hip anchor follows the body (hipY), so only the bob moves.
  add("run bob 3 px", {"BOB"}, true, [](JLook& lk, char fac, int) { for (int i : {2, 5}) { JFrame* f = frameOf(lk, "run", fac, i); f->bob = -3; f->hipY += 1; } }, {{"BOB", "range 3"}});
  add("walk bob 2 px", {"BOB", "SQUAT"}, false, [](JLook& lk, char fac, int) { JFrame* f = frameOf(lk, "walk", fac, 1); f->bob = 2; f->hipY -= 2; }, {{"BOB", "range 2 (<= 1)"}});
  // RHYTHM: round 6's holds (contact 8, push 4, flight 6); the plants a quarter px apart so no stance links (REG unread)
  add("run holds 8/4/6", {"RHYTHM"}, true, [](JLook& lk, char fac, int) {
    std::vector<JFrame*> fs;
    for (JFrame& f : lk.frames) if (f.gait == "run" && f.fac == fac) fs.push_back(&f);
    std::sort(fs.begin(), fs.end(), [](JFrame* a, JFrame* b) { return a->i < b->i; });
    const double p0 = fs[0]->plant0;
    for (size_t k = 0; k < fs.size(); k++) {
      static const int A0[3] = {0, 8, 12}, B0[3] = {8, 12, 18};
      const int a = A0[k % 3], b = B0[k % 3];
      fs[k]->plant0 = p0 - a + 0.25 * (double)(k + 1); fs[k]->plant1 = p0 - b + 0.25 * (double)(k + 1);
    }
  }, {{"RHYTHM", "spread 4"}});
  // ---- the re-homed checks
  // JOINTS: a row cut out of the far shin (the leg in two pieces)
  add("far shin cut in two", {"JOINTS"}, false, [](JLook& lk, char fac, int) {
    editLayer(lk, "walk", fac, 1, "9999", [](const PX& px) {
      int ys = -(1 << 20), yt = 1 << 20;
      for (auto& [k, c] : px) if (reg(c) == 'S') { ys = std::max(ys, k.second); yt = std::min(yt, k.second); }
      const int cutY = (ys + yt) / 2;
      PX o;
      for (auto& [k, c] : px) if (k.second != cutY) o[k] = c;
      return o;
    });
  }, {{"JOINTS", "pieces"}});
  // GROUND: a walk frame's legs lifted a pixel (no sole on the ground row)
  add("walk frame drawn 1 px up (floating)", {"GROUND"}, false, [](JLook& lk, char fac, int) {
    JFrame* f = frameOf(lk, "walk", fac, 2);
    for (auto& [id, px] : f->layers) { PX o; for (auto& [k, c] : px) o[{k.first, k.second - 1}] = c; px = cropped(o); }
    f->hipY -= 1;   // (the clip's hip drawn up with it: the legs keep their length)
    recompose(lk, *f);
  });
  return v;
}

std::vector<Scale> scales() {
  return {
      {"adult", &kBodies[0], {"defhero", 0, "weapon=0,topColor=#466E96,bottomColor=#4E3C2C,hairColor=#784624,palette=look", {"walk", "run"}}, true, 5, 9, 'K', -10},
      {"youth", &kBodies[1], {"synth_youth", 0, "age=2,weapon=0", {"walk", "run"}}, true, 4, 8, 'Y', -8},
      {"child", &kBodies[2], {"synth_child", 0, "age=1,weapon=0", {"walk"}}, true, 4, 6, 'C', -5},
  };
}
std::set<std::string> failingSet(JLook L, std::vector<Fail>* fl = nullptr) {
  Judged J;
  judgeLook(L, J, true, true);
  std::set<std::string> s;
  for (auto& [c, n] : J.fails) if (n) s.insert(c);
  if (fl) *fl = J.failList;
  return s;
}
std::string setStr(const std::set<std::string>& s) { std::string o; for (auto& x : s) o += (o.empty() ? "" : ",") + x; return o.empty() ? "nothing" : o; }
int selftestAll(bool report) {
  int miss = 0;
  for (const Scale& S : scales()) {
    const LookFrames lf = shoot(S.base, nullptr);
    JLook base = toJLook(lf);
    base.T = S.T;
    Scale s2 = S;
    if (!base.frames.empty()) s2.hip = base.frames[0].hby;   // the body's own hip anchor
    const Built B = buildGood(s2, base);
    if (const char* dump = std::getenv("SYNTH_DUMP"))   // (debugging the synthetics) SYNTH_DUMP=youth: the good look's report
      if (std::string(dump) == S.name) {
        JLook g = B.good;
        Judged J;
        judgeLook(g, J, true, false);
        for (auto& l : J.lines) std::printf("%s\n", l.c_str());
        for (auto& f : J.failList) {
          char gg[32] = {}, fc = 'R', tg[64] = {}; int i = 0;
          if (std::sscanf(f.where.c_str(), "%63s %31s %c%d", tg, gg, &fc, &i) == 4)
            if (const JFrame* fr = frameOf(g, gg, fc, i)) { std::printf("FAIL %s %s\n", f.check.c_str(), f.where.c_str()); for (auto& s : asciiLegs(*fr)) std::printf("%s\n", s.c_str()); }
        }
      }
    std::vector<Fail> gf;
    const std::set<std::string> g = failingSet(B.good, &gf);
    const bool gok = g.empty();
    miss += gok ? 0 : 1;
    if (report || !gok) std::printf("  SYNTH %s %s good two-layer legs: fails %s (expected nothing)\n", gok ? "ok  " : "MISS", S.name, setStr(g).c_str());
    if (!gok && report) for (auto& f : gf) std::printf("      %s %s: %s\n", f.check.c_str(), f.where.c_str(), f.why.c_str());
    for (const Cut& c : cutsFor(s2)) {
      if (c.run && !S.T->run) continue;
      for (char fac : {'R', 'L'}) {
        JLook lk = B.good;
        auto ci = B.cm.find(fac);
        c.fn(lk, fac, ci == B.cm.end() ? 0 : ci->second);
        std::vector<Fail> fl;
        const std::set<std::string> got = failingSet(lk, &fl);
        bool ok = std::includes(got.begin(), got.end(), c.fail.begin(), c.fail.end());
        for (auto& x : got) ok = ok && (c.fail.count(x) || c.allow.count(x));
        std::string whyTxt;
        for (auto& [chk, sub] : c.why) {
          bool hit = false;
          for (auto& f : fl) hit = hit || (f.check == chk && f.why.find(sub) != std::string::npos);
          ok = ok && hit;
          whyTxt += fmt("; %s names \"%s\": %s", chk.c_str(), sub.c_str(), hit ? "yes" : "NO");
        }
        // every failure in the cut's own facing (a cut in R never fails an L frame)
        for (auto& f : fl) ok = ok && f.where.find(std::string(" ") + fac) != std::string::npos;
        miss += ok ? 0 : 1;
        if (report || !ok) std::printf("  SYNTH %s %s %s %c: fails %s (expected %s)%s\n", ok ? "ok  " : "MISS", S.name, c.name.c_str(), fac, setStr(got).c_str(), setStr(c.fail).c_str(), whyTxt.c_str());
        if (!ok && report) for (auto& f : fl) std::printf("      %s %s: %s\n", f.check.c_str(), f.where.c_str(), f.why.c_str());
      }
    }
  }
  std::printf("SYNTH %s\n", miss ? "A CHECK MISSED ITS SYNTHETIC" : "ALL CAUGHT");
  return miss;
}

}  // namespace cp

void setMapsRoot(const std::string& dir);   // (test_doll.cpp's --maps)
void setMapsRoot(const std::string& dir) { cp::g_maps = dir; cp::g_pxm.clear(); }

void limbsChecks(const LookFrames& f, std::vector<Fail>& out, bool report) {
  cp::JLook L = cp::toJLook(f);
  cp::Judged J;
  cp::judgeLook(L, J, true, !report);
  for (Fail& x : J.failList) {
    x.check = (cp::isB12(x.check) ? "B12 " : "B11 ") + x.check;
    out.push_back(x);
  }
  if (report) {
    for (const std::string& s : J.lines) std::printf("%s\n", s.c_str());
    for (const Fail& x : J.failList) {
      std::printf("FAIL %s %s: %s\n", x.check.c_str(), x.where.c_str(), x.why.c_str());
      // the failing frame's legs as the tool reads them
      char g[32] = {}, fc = 'R'; int i = 0; char tag[128] = {};
      if (std::sscanf(x.where.c_str(), "%127s %31s %c%d", tag, g, &fc, &i) == 4)
        if (const cp::JFrame* fr = cp::frameOf(L, g, fc, i)) for (const std::string& s : cp::asciiLegs(*fr)) std::printf("%s\n", s.c_str());
    }
    if (const char* want = std::getenv("LIMBS_ASCII"))   // (debugging) LIMBS_ASCII=run: every frame of that gait, passing or not
      for (char fc : {'R', 'L'})
        for (int i = 0; i < 8; i++)
          if (const cp::JFrame* fr = cp::frameOf(L, want, fc, i)) {
            std::printf("MAP %s %s %c%d\n", f.look.tag.c_str(), want, fc, i);
            for (const std::string& s : cp::asciiLegs(*fr)) std::printf("%s\n", s.c_str());
          }
  }
}

int limbsSelftest(bool report) { return cp::selftestAll(report); }

}  // namespace dollmotion
