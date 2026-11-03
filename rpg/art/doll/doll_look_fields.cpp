// EMBERVALE art: the looks-file fields (P2c SLICE-DOLL's one list; SOLID 5.4 moved it here out of doll_walk.cpp), part of
// rpg_doll, the doll's data library below both rpg_sim and rpg_art: no painter, no SDL. applyLookField sets the plain
// HumanLook fields, applyLadderLookField the ladder fields (tiers, marks, alloys, packed forge styles) that the gear ladder
// (rpg/sim/gear_ladder.cpp) and art_hash (tools/art_hash.cpp) both read: one setter, so the two can never drift apart.
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/art/doll/doll_internal.h"
#include "rpg/art/doll/doll_look_fields.h"
#include "rpg/core/fingerprint.h"

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
  else if (k == "makers") return applyLadderLookField(L, k, v);   // (P5) a looks line's makers= wherever its parser files it
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

// ---------------------------------------------------------------- (SOLID 5.4) the ladder fields, one list
namespace art {
namespace doll {
namespace {
// (P5, FIX_BATCH_P5 4.3) one maker people: a name (fp::kRows: "fjordfolk" ...), a number (0..12) or "-" (none); -1 unknown
int makerOf(const std::string& t) {
  if (t.empty() || t == "-") return 0;
  if (t[0] >= '0' && t[0] <= '9') { const int n = (int)std::strtol(t.c_str(), nullptr, 10); return n >= 0 && n < fp::kPeoples ? n : -1; }
  for (int i = 0; i < fp::kPeoples; i++) if (t == fp::kRows[i].name) return i;
  return -1;
}
// makers=<body>:<helm>:<shield>:<weapon>:<gloves>:<boots>:<cloak>:<back> (colons: the spec splits on commas); one people alone
// sets every worn armour slot (body helm shield gloves boots cloak)
bool applyMakers(HumanLook& L, const std::string& v) {
  std::vector<int> ps;
  size_t a = 0;
  while (a <= v.size()) {
    size_t b = v.find(':', a);
    if (b == std::string::npos) b = v.size();
    const int m = makerOf(v.substr(a, b - a));
    if (m < 0) return false;
    ps.push_back(m);
    a = b + 1;
  }
  if (ps.size() == 1) {
    for (int s : {(int)HumanLook::TIER_BODY, (int)HumanLook::TIER_HELM, (int)HumanLook::TIER_SHIELD, (int)HumanLook::TIER_GLOVES,
                  (int)HumanLook::TIER_BOOTS, (int)HumanLook::TIER_CLOAK})
      L.setMaker(s, ps[0]);
    return true;
  }
  if (ps.size() > (size_t)HumanLook::TIER_SLOTS) return false;
  for (size_t s = 0; s < ps.size(); s++) L.setMaker((int)s, ps[s]);
  return true;
}
}  // namespace
bool applyLadderLookField(HumanLook& L, const std::string& k, const std::string& v) {
  const int n = (int)std::strtol(v.c_str(), nullptr, 0);
  static const char* const kTier[8] = {"tierBody", "tierHelm", "tierShield", "tierWeapon", "tierGloves", "tierBoots", "tierCloak", "tierBack"};
  static const char* const kForge[8] = {"forgeBody", "forgeHelm", "forgeShield", "forgeWeapon", "forgeGloves", "forgeBoots", "forgeCloak", "forgeBack"};
  for (int s = 0; s < 8; s++) {
    if (k == kTier[s]) { L.tier[s] = (uint8_t)std::clamp(n, 0, 6); return true; }
    if (k == kForge[s]) { L.forge[s] = std::strtoull(v.c_str(), nullptr, 0); return true; }
  }
  if (k == "makers") return applyMakers(L, v);   // (P5)
  if (k == "rare") L.rare = (uint8_t)n;
  else if (k == "mythic") L.mythic = (uint8_t)n;
  else if (k == "metalKind") L.metalKind = (uint8_t)n;
  else if (k == "bladeKind") L.bladeKind = (uint8_t)n;
  else if (k == "motif") L.motif = (uint8_t)n;
  else if (k == "kitVar") L.kitVar = (uint8_t)n;
  else if (k == "floor") L.floor = (uint8_t)n;
  else if (k == "alloyFam") L.alloyFam = (uint8_t)n;
  else if (k == "forgeKit") L.forgeKit = (uint8_t)n;
  else if (k == "alloyRank") L.alloyRank = (uint8_t)n;
  else if (k == "forgeGuise") L.forgeGuise = (uint16_t)n;
  else if (k == "king") L.king = (uint8_t)n;   // (D96)
  else if (k == "forgeShape") L.forgeShape = (uint32_t)std::strtoul(v.c_str(), nullptr, 0);
  else if (k == "gemColor") {
    const uint32_t rgb = (uint32_t)std::strtoul(v.c_str() + (!v.empty() && v[0] == '#' ? 1 : 0), nullptr, 16);
    L.gemColor = v == "0" ? 0u : 0xFF000000u | (rgb & 0xFF) << 16 | (rgb & 0xFF00) | (rgb >> 16 & 0xFF);
  } else return false;
  return true;
}
}  // namespace doll
}  // namespace art
