// EMBERVALE art: the doll's body set and walk tables (DOLL; P2b phase A, split out of doll_resolve.cpp). The simulation
// reads these (rpg/sim/anim.cpp: the gait, the walk clock), so they build with the pool (doll_pool.cpp, gen/doll_pool.cpp)
// into rpg_doll, the doll's data library below both rpg_sim and rpg_art: no painter, no SDL. Nothing here draws.
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/art/doll/doll_internal.h"

namespace art {
namespace doll {

BodySet bodySetOf(const HumanLook& L) {
  if (L.age == 1) return BodySet::Child;
  if (L.age == 2) return BodySet::Youth;
  return L.body == 1 ? BodySet::Woman : BodySet::Man;
}

// ---------------------------------------------------------------- the walk table
int WalkTable::frameAt(float px) const {
  if (frames <= 0 || cycle <= 0) return 0;
  float d = px - cycle * (float)(int)(px / cycle);
  if (d < 0) d += cycle;
  int f = 0;
  while (f + 1 < frames && d >= start[f + 1]) f++;
  return f;
}
// (P2b integration) the hair family table lives in the data library, so the simulation (gear_look.cpp: the created
// hero's style within the family) links without the painters (save_test)
namespace {
// family x variant -> style (art::Hair order: Bald, Short, Long, Ponytail, Mohawk, Braids, Bun, Curls)
const int kFamilyStyles[8][5] = {{0, 1, -1, -1, -1}, {3, 2, 4, 5, 7}, {12, 13, 11, 10, -1}, {14, 15, 22, -1, -1},
                                 {8, 9, -1, -1, -1}, {16, 17, 19, 20, -1}, {18, 21, -1, -1, -1}, {6, 23, -1, -1, -1}};
}  // namespace
int hairVariants(Hair f) {
  const int fi = std::clamp((int)f, 0, 7);
  int n = 0;
  while (n < 5 && kFamilyStyles[fi][n] >= 0) n++;
  return n;
}
int hairStyleOf(Hair f, uint8_t v) {
  const int fi = std::clamp((int)f, 0, 7);
  const int n = hairVariants(f);
  return kFamilyStyles[fi][std::min((int)v, n - 1)];
}
namespace {
WalkTable walkTableOf(BodySet set, const std::string& bodyKey, Gait gait, int facing) {
  using namespace pool;
  WalkTable t;
  static const float kStride[4] = {19.0f, 20.0f, 16.0f, 13.0f};
  t.cycle = gait == Gait::Run ? 36.0f : kStride[std::min((int)set, 3)];
  const Pool& P = get();
  const std::vector<const ClipRec*> cs = clipsOf(bodyKey, gait == Gait::Run ? (uint8_t)RigPart::Run : (uint8_t)RigPart::Loco, -1, -1);
  const ClipRec* c = nullptr;
  for (const ClipRec* x : cs) if (x->clip == (uint8_t)Clip::Walk && x->facing == facing) c = x;
  if (!c) return t;   // frames 0: no clip (the caller keeps its own table)
  t.frames = std::min<int>(c->nFrames, 16);
  float adv[16] = {};
  float total = 0;
  bool planted = true;
  for (int i = 0; i < t.frames; i++) {
    const FrameRec& f = P.frames[c->firstFrame + i];
    if (f.plant0 == kNoPlant || f.plant1 == kNoPlant) { planted = false; break; }
    adv[i] = std::max(0.0f, (f.plant0 - f.plant1) / 16.0f);   // the planted foot slides back as the body moves on
    total += adv[i];
  }
  if (!planted || total <= 0) {
    if (c->cycle16) t.cycle = c->cycle16 / 16.0f;
    for (int i = 0; i < t.frames; i++) t.start[i] = t.cycle * i / t.frames;
    return t;
  }
  t.cycle = total;
  float acc = 0;
  for (int i = 0; i < t.frames; i++) { t.start[i] = acc; acc += adv[i]; }
  return t;
}
}  // namespace
WalkTable walkTable(BodySet set, Gait gait, int facing) { return walkTableOf(set, bodySetName(set), gait, facing); }
WalkTable walkTable(const HumanLook& look, Gait gait, int facing) {
  const BodySet set = bodySetOf(look);
  std::string key = bodySetName(set);
  if (look.age == 3 && (set == BodySet::Man || set == BodySet::Woman)) key += ".elder";
  return walkTableOf(set, key, gait, facing);
}

}  // namespace doll
}  // namespace art

// ---------------------------------------------------------------- (P2c SLICE-DOLL) the looks-file fields, one list (moved here from doll_resolve.cpp by LADDER-DOLL: rpg_doll)
namespace art {
namespace doll {
bool applyLookField(HumanLook& L, const std::string& k, const std::string& v) {
  const int n = (int)std::strtol(v.c_str(), nullptr, 0);
  auto col = [&]() {
    const uint32_t rgb = (uint32_t)std::strtoul(v.c_str() + (!v.empty() && v[0] == '#' ? 1 : 0), nullptr, 16);
    return 0xFF000000u | (rgb & 0xFF) << 16 | (rgb & 0xFF00) | (rgb >> 16 & 0xFF);
  };
  auto col0 = [&]() { return v == "0" ? 0u : col(); };   // fields where 0 means "derived"
  auto u8 = [&]() { return (uint8_t)std::clamp(n, 0, 255); };
  if (k == "body") L.body = u8();
  else if (k == "build") L.build = u8();
  else if (k == "people") L.people = u8();
  else if (k == "age") L.age = u8();
  else if (k == "garb") L.garb = u8();
  else if (k == "cut") L.cut = u8();
  else if (k == "outfit") L.outfit = (Outfit)std::clamp(n, 0, (int)Outfit::COUNT - 1);
  else if (k == "weapon") L.weapon = u8();
  else if (k == "civil") L.civil = u8();
  else if (k == "pattern") L.pattern = u8();
  else if (k == "cloak") L.cloak = u8();
  else if (k == "cape") L.cape = n != 0;
  else if (k == "hair") L.hair = (Hair)std::clamp(n, 0, (int)Hair::COUNT - 1);
  else if (k == "hairVar") L.hairVar = u8();
  else if (k == "beard") L.beard = n != 0;
  else if (k == "beardStyle") L.beardStyle = u8();
  else if (k == "headwear") L.headwear = u8();
  else if (k == "hood") L.hood = n != 0;
  else if (k == "neck") L.neck = u8();
  else if (k == "jewellery") L.jewellery = u8();
  else if (k == "amulet") L.amulet = n != 0;
  else if (k == "ring") L.ring = n != 0;
  else if (k == "head") L.head = u8();
  else if (k == "nose") L.nose = u8();
  else if (k == "brow") L.brow = u8();
  else if (k == "marks") L.marks = u8();
  else if (k == "facePaint") L.facePaint = u8();
  else if (k == "helmet") L.helmet = n != 0;
  else if (k == "helmStyle") L.helmStyle = u8();
  else if (k == "helmForm") L.helmForm = u8();
  else if (k == "armorStyle" || k == "armourStyle") L.armorStyle = u8();
  else if (k == "bodyForm") L.bodyForm = u8();
  else if (k == "shield") L.shield = n != 0;
  else if (k == "shieldStyle") L.shieldStyle = u8();
  else if (k == "shieldForm") L.shieldForm = u8();
  else if (k == "bladeForm") L.bladeForm = u8();
  else if (k == "polearmForm") L.polearmForm = u8();
  else if (k == "bowForm") L.bowForm = u8();
  else if (k == "pauldron") L.pauldron = u8();
  else if (k == "skirt") L.skirt = u8();
  else if (k == "crest") L.crest = u8();
  else if (k == "armsOrnament") L.armsOrnament = (uint16_t)std::clamp(n, 0, 65535);
  else if (k == "gloves") L.gloves = u8();
  else if (k == "boots") L.boots = u8();
  else if (k == "backItem") L.backItem = u8();
  else if (k == "sheen") L.sheen = u8();
  else if (k == "glow") L.glow = u8();
  else if (k == "skin") L.skin = col();
  else if (k == "hairColor") L.hairColor = col();
  else if (k == "topColor") L.topColor = col();
  else if (k == "bottomColor") L.bottomColor = col();
  else if (k == "tabardColor") L.tabardColor = col();
  else if (k == "cloakColor") L.cloakColor = col();
  else if (k == "headColor") L.headColor = col0();
  else if (k == "patternColor") L.patternColor = col0();
  else if (k == "eyeColor") L.eyeColor = col0();
  else if (k == "plumeColor") L.plumeColor = col0();
  else if (k == "armourTint" || k == "armorTint") L.armourTint = col0();
  else if (k == "armourTint2" || k == "armorTint2") L.armourTint2 = col0();
  else if (k == "glowColor") L.glowColor = col0();
  else if (k == "shieldField") L.shieldField = col0();
  else if (k == "shieldDevice") L.shieldDevice = col0();
  else if (k == "trimColor") L.trimColor = col0();
  else if (k == "weaponColor") L.weaponColor = col();
  else return false;
  return true;
}
// (P2c ladder) the looks macros' hook (rpg/sim/gear_ladder.cpp installs it); this file is rpg_doll's, below rpg_sim, so
// the simulation's half links without the painters (save_test)
namespace { LookSpecHook g_lookHook = nullptr; }
void setLookSpecHook(LookSpecHook hook) { g_lookHook = hook; }
void applyLookSpec(HumanLook& L, const std::string& spec, bool* lookPal, bool* bare, std::string* bad) {
  if (spec.empty() || spec == "-") return;
  if (g_lookHook) { g_lookHook(L, spec, lookPal, bare, bad); return; }
  applyLookSpecPlain(L, spec, lookPal, bare, bad);
}
void applyLookSpecPlain(HumanLook& L, const std::string& spec, bool* lookPal, bool* bare, std::string* bad) {
  if (spec.empty() || spec == "-") return;
  size_t a = 0;
  while (a < spec.size()) {
    size_t b = spec.find(',', a);
    if (b == std::string::npos) b = spec.size();
    const std::string kv = spec.substr(a, b - a);
    a = b + 1;
    const size_t eq = kv.find('=');
    if (eq == std::string::npos) { if (!kv.empty() && bad) *bad += (bad->empty() ? "" : ",") + kv; continue; }
    const std::string k = kv.substr(0, eq), v = kv.substr(eq + 1);
    if (k == "palette") { if (lookPal) *lookPal = v == "look"; }
    else if (k == "bare") { if (bare) *bare = v == "1"; }
    else if (!applyLookField(L, k, v) && bad) *bad += (bad->empty() ? "" : ",") + k;
  }
}
}  // namespace doll
}  // namespace art
