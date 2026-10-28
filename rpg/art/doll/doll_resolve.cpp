// EMBERVALE art: the doll's resolver and coverage (DOLL; P2b phase A first version by the lead). A look becomes a
// recipe: its body set, every field value it asks for (needs, in the role vocabulary of doll.h), the pool's parts that
// serve them (every part with that role, for that body set), its ramps, the slot of each region and its flags. Coverage
// walks the recipe through the body set's clips and names every value no authored part covers (VISION_PLAN 15.36
// "Coverage"). Nothing here draws.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <unordered_map>
#include "rpg/art/doll/doll_envelope.h"
#include "rpg/art/doll/doll_forge.h"
#include "rpg/art/doll/doll_internal.h"

namespace art {
namespace doll {

// ---------------------------------------------------------------- the vocabulary
namespace {
const char* const kHairStyle[HAIR_STYLES] = {"bald", "shaved", "cropped", "short", "tousled", "spiky", "curly", "receding", "mohawk", "topknot", "bob",
                                             "shag", "longloose", "longwavy", "ponytail", "highponytail", "braid", "twinbraids", "bun", "locs",
                                             "crownbraid", "topbun", "pigtails", "bigcurls"};
// (family x variant -> style, hairVariants and hairStyleOf: doll_walk.cpp, the rpg_doll data library)
const char* const kChildHair[CHILD_HAIR_STYLES] = {"short", "tousled", "cropped", "curly", "bob", "long", "pigtails", "braid", "bun", "ponytail"};
const char* const kHeadMan[9] = {"oval", "square", "round", "long", "craggy", "heavy", "fine", "sharp", "longelf"};
const char* const kHeadWoman[9] = {"oval", "heart", "round", "long", "square", "full", "fine", "sharp", "longelf"};
const char* const kHeadChild[4] = {"round", "oval", "pointed", "elfin"};
const char* const kNoseMan[5] = {"button", "straight", "long", "hooked", "broad"};
const char* const kNoseWoman[4] = {"button", "straight", "upturned", "aquiline"};
const char* const kBrowMan[4] = {"straight", "heavy", "arched", "thin"};
const char* const kBrowWoman[4] = {"arched", "straight", "thin", "full"};
const char* const kBeard[10] = {"full", "stubble", "boxed", "long", "braided", "goatee", "moustache", "vandyke", "chops", "handlebar"};
const char* const kNeck[4] = {"", "scarf", "kerchief", "collar"};
const char* const kMarks[6] = {"freckles", "scar", "beauty", "weathered", "lined", "gaunt"};
const char* const kHeadwear[9] = {"", "hood", "cap", "turban", "furhat", "veil", "circlet", "conical", "headscarf"};
const char* const kCut[9] = {"", "culturetunic", "robe", "kaftan", "wrap", "coat", "kilt", "poncho", "gown"};
const char* const kPaint[5] = {"", "stripes", "dots", "mask", "sun"};
const char* const kPattern[7] = {"", "stripes", "checks", "border", "dots", "embroidery", "vines"};
const char* const kCloak[4] = {"", "travel", "fur", "noble"};
const char* const kHeld[13] = {"", "sword", "axe", "bow", "staff", "dagger", "mace", "greatsword", "spear", "stick", "ledger", "scroll", "horn"};
int childHairOf(Hair f, uint8_t v) {
  switch (f) {
    case Hair::Short: return v == 1 ? 2 : v == 2 ? 1 : 0;   // short, cropped, tousled
    case Hair::Long: return v == 3 ? 4 : 5;                 // bob, long
    case Hair::Ponytail: return v == 2 ? 6 : 9;             // pigtails, ponytail
    case Hair::Braids: return 7;
    case Hair::Bun: return 8;
    case Hair::Curls: return 3;
    case Hair::Mohawk: return 1;
    default: return 2;                                      // bald and shaved children: cropped
  }
}
}  // namespace

const char* hairStyleName(int s) { return s >= 0 && s < HAIR_STYLES ? kHairStyle[s] : "?"; }
const char* childHairName(int s) { return s >= 0 && s < CHILD_HAIR_STYLES ? kChildHair[s] : "?"; }
const char* headTypeName(int body, uint8_t h) { return body == 1 ? kHeadWoman[std::min<int>(h, 8)] : kHeadMan[std::min<int>(h, 8)]; }
const char* noseName(int body, uint8_t n) { return body == 1 ? kNoseWoman[std::min<int>(n, 3)] : kNoseMan[std::min<int>(n, 4)]; }
const char* browName(int body, uint8_t b) { return body == 1 ? kBrowWoman[std::min<int>(b, 3)] : kBrowMan[std::min<int>(b, 3)]; }
const char* beardName(uint8_t s) { return kBeard[std::min<int>(s, 9)]; }
const char* neckName(uint8_t n) { return kNeck[std::min<int>(n, 3)]; }

// (bodySetOf, the walk tables: doll_walk.cpp, the rpg_doll library the simulation links)

bool DollRecipe::has(const char* flag) const {
  for (const std::string& f : flags) if (f == flag) return true;
  return false;
}

std::string Missing::what() const {
  char b[256];
  if (part.empty()) std::snprintf(b, sizeof b, "%s=%s (%s): no part%s%s", roleName(role), value.c_str(), bodySetName(body), key.empty() ? "" : " for key ", key.c_str());
  else std::snprintf(b, sizeof b, "%s=%s (%s) %s %c key %s: no view", roleName(role), value.c_str(), bodySetName(body), part.c_str(),
                     facing >= 0 && facing < 4 ? "DURL"[facing] : '*', key.c_str());
  return b;
}

// ---------------------------------------------------------------- the resolver
namespace {

std::string garmentOf(const HumanLook& L) {
  // (FOLK-SIM) rags over any cut (a beggar's clothes are worn through), then the doll's garment (HumanLook::garb)
  if (L.outfit == Outfit::Rags) return "rags";
  // (FOLK-SIM, P2b production) 10 rags (the village's poorest), 11-13 a woman in her people's men's kaftan, wrap or poncho
  static const char* const kGarb[14] = {"", "apron", "smock", "jerkin", "dress", "tunic", "coat", "culturetunic", "kilt", "gown",
                                        "rags", "kaftan", "wrap", "poncho"};
  if (L.garb >= 1 && L.garb <= 13) return kGarb[L.garb];
  if (L.cut >= 1 && L.cut <= 8) return kCut[L.cut];
  switch (L.outfit) {
    case Outfit::Dress: return "dress";
    case Outfit::Robe: return "robe";
    case Outfit::Rags: return "rags";
    default: return "tunic";   // (the armour outfits wear their armour over the tunic: P2c's role armour)
  }
}
bool longGarment(const std::string& g) { return g == "dress" || g == "robe" || g == "gown" || g == "kaftan" || g == "wrap"; }
const char* armourOf(const HumanLook& L) {
  // (P2b fix TRADES-DRAWN-ON-V1) a civil trade's v1 leathers are its work clothes: the doll draws its garb (the smith's
  // apron, the hunter's jerkin), never P2c's leather armour; a culture's arms on it (armorStyle, bodyForm) stay armour
  if (L.civil && L.outfit == Outfit::Leather && !L.bodyForm && !L.armorStyle) return nullptr;
  if (L.bodyForm) {
    static const char* f[] = {"padded", "leather", "mail", "scale", "lamellar", "brigandine", "plate", "leaf"};
    return f[std::min<int>(L.bodyForm - 1, 7)];
  }
  // (P2c fix, contracts 3) the herald: Outfit::Guard with no helm and no body form wears padded cloth under the tabard
  if (L.outfit == Outfit::Guard && !L.armorStyle && !(L.helmet || L.helmStyle || L.helmForm)) return "padded";
  switch (L.outfit) {
    case Outfit::Leather: return "leather";
    case Outfit::Chain: case Outfit::Guard: return "mail";
    case Outfit::Plate: case Outfit::Ebony: return "plate";
    case Outfit::Elven: return "leaf";
    default: break;
  }
  if (L.armorStyle) return L.armorStyle == 1 ? "leather" : L.armorStyle >= 3 ? "plate" : "mail";
  return nullptr;
}

// the culture headwear drawn (0: none): a helmet or the legacy hood takes its place (v1's rule)
int headwearOf(const HumanLook& L) {
  if (L.helmet || L.helmStyle || L.helmForm || L.hood) return 0;
  return L.headwear >= 1 && L.headwear <= 8 ? L.headwear : 0;
}

// (P2c) the helm's band (1..7; 0 no helm): helmStyle, else a legacy helmet's band by its outfit (v1: the leather cap,
// the iron nasal, the steel great helm, the elven winged helm, ebony's horns)
int helmBandOf(const HumanLook& L) {
  if (L.helmStyle) return std::min<int>(L.helmStyle, 7);
  if (!L.helmet) return 0;
  switch (L.outfit) {
    case Outfit::Leather: return 1;
    case Outfit::Plate: return 3;
    case Outfit::Elven: return 4;
    case Outfit::Ebony: return 6;
    default: return 2;
  }
}
// (P2c) a shield is borne (shieldForm 9 = ShieldForm::None + 1: none)
bool shieldOf(const HumanLook& L) {
  if (L.shieldForm == 9) return false;
  return L.shield || L.shieldStyle || L.shieldForm;
}
// (P2c forge, contracts 3c) shoulder: the body style's shape (doll_forge.h's shoulder value, 0 the family's own): that shape
// at the dial's size, else its nearest drawn size, then the form family's shape (today's rule), then none
std::vector<std::string>* g_envMissing = nullptr;   // (P2c envelope) the recipe being resolved: its envelopeMissing
// (P2c envelope) a pauldron value's width across facing D: 16 + the reach of its parts' facing-D rest views past the 16 px
// shoulder line (the right part's shoulder anchor sits 1 px inside the line's left end, the left part's 1 px inside its right
// end; measured on the pool, cached per pool)
int pauldronAcross(const std::string& v) {
  const pool::Pool& P = pool::get();
  static const void* poolAt = nullptr;
  static size_t poolParts = 0, poolViews = 0;
  static std::unordered_map<std::string, int> cache;
  if (poolAt != (const void*)&P || poolParts != P.parts.size() || poolViews != P.views.size()) {
    cache.clear(); poolAt = &P; poolParts = P.parts.size(); poolViews = P.views.size();
  }
  auto it = cache.find(v);
  if (it != cache.end()) return it->second;
  const uint16_t rest = P.strId("rest");
  int reachR = 0, reachL = 0;
  for (int pi : P.partsWithRole(Role::Pauldron, v)) {
    const pool::PartRec& pr = P.parts[(size_t)pi];
    for (uint32_t vi = pr.firstView; vi < pr.firstView + pr.nViews && vi < P.views.size(); vi++) {
      const pool::ViewRec& w = P.views[vi];
      if (w.facing != 0 || w.when != 0 || w.key != rest || w.cells.empty()) continue;
      int mn = 1 << 20, mx = -1;
      for (int y = 0; y < w.h; y++)
        for (int x = 0; x < w.w; x++) if (w.cells[(size_t)y * w.w + x] >= 2) { mn = std::min(mn, x); mx = std::max(mx, x); }
      if (mx < 0) continue;
      if (pr.side == 1) reachR = std::max(reachR, w.ox - mn - 1);
      else if (pr.side == 2) reachL = std::max(reachL, mx - w.ox - 1);
    }
  }
  const int across = 16 + std::max(0, reachR) + std::max(0, reachL);
  cache[v] = across;
  return across;
}
// (envelope fix round, D55; the judge's round 3: the hero's T2 measured 1.05 of T1 and read as T1's silhouette) the hero lines'
// T2 kit: TANNER'S at T2 on the Heartland's kit wears the soft cap (the leather cap's T2 drawing), the short hooded mantle (the
// pauldron shape `capelet` at size 2) and its hood thrown back (the neck piece `cowl`); each falls back to today's leather cap,
// flap2 and no neck piece until drawn, listed in envelopeMissing (rpg_test --doll-coverage --scope envelope)
bool t2Kit(const HumanLook& L) {
  return L.forgeKit && L.tier[HumanLook::TIER_BODY] == 2 && L.forge[HumanLook::TIER_BODY] && (L.forge[HumanLook::TIER_BODY] & 0xFF) == forge::B_TANNER;
}
// (envelope fix round, D57; the judge's round 3: eg3 measured shoulders 18, no cape, 1.05 of eg0) the village watch at T3 wears
// the fair test's T3d kit (docs/ref/envelope f32_guard_t3d): a guard's coat (Outfit::Guard) at body tier 3 outside the forge kits
bool watchT3(const HumanLook& L) {
  return L.outfit == Outfit::Guard && L.tier[HumanLook::TIER_BODY] == 3 && !L.forge[HumanLook::TIER_BODY] && !L.civil;
}
// a `when` condition word some view of the pool keys on (a T2 view drawn into an existing part)
bool poolKeysOn(const char* word) {
  const pool::Pool& P = pool::get();
  static const void* at = nullptr;
  static size_t nv = 0;
  static std::unordered_map<std::string, bool> cache;
  if (at != (const void*)&P || nv != P.views.size()) { cache.clear(); at = &P; nv = P.views.size(); }
  const auto it = cache.find(word);
  if (it != cache.end()) return it->second;
  bool any = false;
  for (const pool::ViewRec& v : P.views) {
    if (!v.when) continue;
    const std::string& c = P.s(v.when);
    for (size_t a = 0; a <= c.size() && !any;) {
      size_t b = c.find(',', a);
      if (b == std::string::npos) b = c.size();
      if (c.compare(a, b - a, word) == 0 && b - a == std::strlen(word)) any = true;
      a = b + 1;
    }
    if (any) break;
  }
  cache[word] = any;
  return any;
}
std::string pauldronOf(const HumanLook& L, int shoulder = 0) {
  if (L.pauldron < 2) return "";   // 0 the band's own (none drawn yet), 1 none
  const char* a = armourOf(L);
  if (!a) return "";
  const std::string m = a;
  const char* fam = (m == "plate" || m == "brigandine" || m == "leaf") ? "plate" : (m == "padded" || m == "leather") ? "fur" : "scale";
  // (polish i1) the nearest size the library has drawn for the family (the T2 cap made "plate.2", never drawn: the imperial
  // watchwoman fell back to v1 in a row of rig-2 guards); none drawn at all: no pauldron rather than a hole
  const pool::Pool& P = pool::get();
  // (forge i2) a culture's masterwork alloy is worked elaborate (STYLE_P2 28.5's rank): its shoulders a size larger
  const bool master = L.alloyRank == 3 && (L.forge[HumanLook::TIER_BODY] & 0xFF) == 0 && L.forge[HumanLook::TIER_BODY] && !L.forgeGuise;
  int want = std::min<int>(L.pauldron + (master ? 1 : 0), 4);
  // (P2c envelope, contracts 3d, D49) the body's tier caps the shoulders: at T1-T4 the dial's size at most the tier's (T2
  // small, T3 medium, T4 large); at T5-T6 a body with shoulder plates asks for the great (5) and the legend (6) size. Each
  // drawing's MEASURED width (its facing-D rest views: 16 + the reach past the shoulder line each side) is checked against
  // kEnvelope's shoulders. A fallback (6 -> 5 -> 4) is listed in the
  // recipe's envelopeMissing; never a hole. Untiered looks keep the dial (row 0). The size is nominal: a drawing wider than
  // its size's reach is listed (and C12f fails the look) until it is redrawn, never swapped for a smaller one here.
  const int tb = L.tier[HumanLook::TIER_BODY];
  const bool tiered = tb >= 1 && tb <= 6;
  const int capSize = tiered ? envelopeOf(tb).pauldron : 4;
  const int capAcross = tiered ? envelopeOf(tb).shoulders : 99;
  if (tiered && tb >= 5) want = capSize;
  else if (tiered) want = std::min(want, capSize);
  auto nearest = [&](const char* shape) -> std::string {
    // the size asked, then smaller, then larger up to the cap
    static const int kOrder[] = {0, -1, 1, -2, 2, -3, -4};
    for (int d : kOrder) {
      const int sz = want + d;
      if (sz < 2 || sz > 6 || sz > std::max(capSize, 2)) continue;
      const std::string v = std::string(shape) + "." + std::to_string(sz);
      if (P.partsWithRole(Role::Pauldron, v).empty()) continue;
      if (g_envMissing) {
        if (d) g_envMissing->push_back("pauldron " + std::string(shape) + "." + std::to_string(want) + " -> ." + std::to_string(sz));
        // a drawing wider than its tier's shoulders (C12f fails it): FORGE-MANTLE redraws it to its size's reach (contracts
        // 3d: sizes 2-6 reach 1 / 2 / 3 / 5 / 6 px past the line); listed, the figure keeps it meanwhile
        if (pauldronAcross(v) > capAcross) g_envMissing->push_back("pauldron " + v + " drawn " + std::to_string(pauldronAcross(v)) + " px across (T" + std::to_string(tb) + ": " + std::to_string(capAcross) + ")");
      }
      return v;
    }
    return "";
  };
  if (watchT3(L)) {   // (D57) round steel pauldrons, the cop, at medium (20 px across)
    const std::string v = nearest("cop");
    if (!v.empty()) return v;
  }
  if (t2Kit(L)) {   // (D55) the short hooded mantle: 18 px across, its own size 2 drawing; today's flap2 until drawn
    if (!P.partsWithRole(Role::Pauldron, "capelet.2").empty()) return "capelet.2";
    if (g_envMissing) g_envMissing->push_back("pauldron capelet.2 (T2 kit, D55) -> today's shoulders");
  }
  if (shoulder > 0) {
    const std::string v = nearest(forge::valueName(forge::AX_SHOULDER, shoulder));
    if (!v.empty()) return v;
  }
  return nearest(fam);
}
// (P2c, D3) the material tile laid over an armour's sleeves
const char* sleeveTileOf(const HumanLook& L) {
  const char* a = armourOf(L);
  if (!a) return nullptr;
  const std::string m = a;
  if (m == "mail") return "mail";
  if (m == "scale") return "scale";
  if (m == "lamellar") return "lamellar";
  if (m == "plate") return "lames";
  if (m == "leather") return "stitch";
  if (m == "padded") return "quilt";
  if (m == "brigandine") return "rivets";
  if (m == "leaf") return "leaf";
  return nullptr;
}
// (P2c) the form name of a fighter's weapon (contracts 3)
std::string weaponFormOf(const HumanLook& L) {
  static const char* const kBlade[9] = {"straight", "leaf", "falchion", "scimitar", "khopesh", "wavy", "broad", "curved", "glaive"};
  static const char* const kPole[3] = {"spear", "glaive", "halberd"};
  static const char* const kBow[6] = {"classic", "self", "recurve", "composite", "crossbow", "longbow"};
  switch (L.weapon) {
    case 1: case 7: return kBlade[L.bladeForm >= 1 && L.bladeForm <= 9 ? L.bladeForm - 1 : 0];
    case 2: return "bearded";
    case 6: return "flanged";
    case 5: return "straight";
    case 8: return kPole[L.polearmForm >= 1 && L.polearmForm <= 3 ? L.polearmForm - 1 : 0];
    case 4: return "plain";
    case 3: return kBow[L.bowForm <= 5 ? L.bowForm : 0];
    case 9: return "crook";
    default: return "";
  }
}
std::string heldOf(const HumanLook& L) {
  if (L.weapon < 1 || L.weapon > 12) return "";
  const std::string bare = kHeld[L.weapon];
  if (L.civil || L.weapon >= 10) return bare;
  const std::string form = weaponFormOf(L);
  if (form.empty()) return bare;
  const std::string lib = (L.weapon == 9 ? std::string("stick") : bare) + "." + form;
  return pool::get().partsWithRole(Role::Held, lib).empty() ? bare : lib;
}

// (ladder i3) a culture's helm or shield form the library has not drawn yet wears the nearest drawn form (the blind panel
// saw the Dune, Marsh and Highland T3 kits bare-headed: kettle, conical-aventail and the like have no part yet; a people
// never loses its helm or shield to a missing drawing). The form named stays the look's; only the part it draws changes.
// Chains (HelmForm + 1): kettle 2 -> nasal 1; great helm 3 -> crested 9; horned 5, conical 7, winged 10, masked 8 ->
// spangen 4 -> nasal 1; plumed 6 -> crested 9. Shields (ShieldForm + 1): crescent 5 -> round 1; oval 6, leaf 8 -> kite 2.
std::string drawnForm(Role role, int form) {
  const pool::Pool& P = pool::get();
  auto has = [&](int f) { return !P.partsWithRole(role, "form" + std::to_string(f)).empty(); };
  int f = form;
  for (int guard = 0; guard < 4 && f > 0 && !has(f); guard++) {
    if (role == Role::Helm) {
      static const int kNext[11] = {0, 0, 1, 9, 1, 4, 9, 4, 4, 1, 4};
      f = f <= 10 ? kNext[f] : 0;
    } else {
      static const int kNext[9] = {0, 0, 0, 0, 0, 1, 2, 0, 2};
      f = f <= 8 ? kNext[f] : 0;
    }
  }
  return "form" + std::to_string(f > 0 && has(f) ? f : form);
}

// (forge round 2, contracts 3c.1) where the silhouette applies: a culture alloy's body (its skirt, mass, layer, arm and
// motif) and helm (its family), from HumanLook::forgeShape; a hero style keeps its own drawings but for the round-2 fixes
// (FURNACE's furnace helm and heavy arms, STARFALL's crystal helm and split skirt). The round-1 guise (forgeGuise, the
// selftests' old bodies) turns the silhouette off
struct ShapeUse { bool body = false, helm = false; int bodyBase = -1, helmBase = -1; uint8_t sx[forge::SX_COUNT] = {}; };
ShapeUse shapeUseOf(const HumanLook& L) {
  ShapeUse u;
  forge::unpackShapeAxes(L.forgeShape, u.sx);
  if (L.forge[HumanLook::TIER_BODY]) u.bodyBase = (int)(L.forge[HumanLook::TIER_BODY] & 0xFF);
  if (L.forge[HumanLook::TIER_HELM]) u.helmBase = (int)(L.forge[HumanLook::TIER_HELM] & 0xFF);
  if (!L.forgeShape || L.forgeGuise) return u;
  u.body = u.bodyBase == forge::B_ALLOY && L.tier[HumanLook::TIER_BODY] && armourOf(L) != nullptr;
  u.helm = u.helmBase == forge::B_ALLOY && (helmBandOf(L) || L.helmForm) && u.sx[forge::SX_HELM] >= 1 && u.sx[forge::SX_HELM] <= forge::H_ALLOY_LAST;
  return u;
}
// a value some part draws for the body set
bool drawnFor(Role role, const std::string& v, BodySet body) {
  const pool::Pool& P = pool::get();
  for (int i : P.partsWithRole(role, v)) if (P.parts[(size_t)i].bodyMask >> (int)body & 1) return true;
  return false;
}

// (P2c forge i2; the blind panel: "nearly every material uses the same rounded cap helmet ... the same kite or round
// shield"; "Cloth wears the same spiked metal helmet as plate") a hero style carries its own helm and shield shape, drawn
// from the library: HOMESPUN a cloth hood and the oval, TANNER'S the leather cap and the buckler, FOUNDRY the crested
// helm (the Greek comb's helm) and the round shield, WROUGHT the kettle hat and the round shield, ARMOURER'S the nasal and the tower
// shield (the knight's pavise), PARADE the crested helm and the oval cartouche, KNAPPED the masked helm (a black glass visor) and the round
// shield, FURNACE the great helm and the tower, STARFALL the spangenhelm under its spire and the oval (C16a measured each
// pair: a shared helm parts by its shield, crest and cut, 0.31 or more of the step edges). Only over a
// people's generic forms (nasal, kettle, great helm, spangen, plumed, crested; round, kite, heater, tower, oval, buckler), and
// the universal materials' (TANNER'S, FOUNDRY, WROUGHT, ARMOURER'S) only on the Heartland's kit (HumanLook::forgeKit: another
// people's leather, bronze, iron and steel keep its own helm and shield, C12e); a people's signature shape (horned, conical, masked, winged; crescent, leaf, none) stays its own. At T5-T6 PARADE,
// STARFALL and the MYTHIC keep the ladder's signature helms (the crested T5 crest, the T6 winged crown, the flame crown).
// Rig 2 only (forge[] is set at rig 2 only). helm: HelmForm + 1; -1 the band-1 leather cap; -2 the hood
struct StyleKit { int helm = 0, shield = 0, hw = 0; };   // hw: (forge i3) a guise's metal head drawn as headwear (cult::Headwear)
StyleKit styleKitOf(const HumanLook& L, const DollRecipe& r) {
  StyleKit k;
  // (forge i3; the panel: "Leather vs Gilded: both a small cap with a band"; C16a bronze vs gilt 0.29) PARADE the gilt turban
  // helm (12: the parade helm of the east), a metal head cut from the headwear library (guiseHeadwear). (ARMOURER'S in the
  // aventail was tried: the T3 hero grew past T4, C12c, and the lying views have no aventail, C3e)
  static const int8_t kHelm[forge::B_COUNT] = {0, -2, -1, 9, 2, 1, 12, 8, 3, 4, 0};
  static const int8_t kShield[forge::B_COUNT] = {0, 6, 7, 1, 1, 4, 6, 1, 1, 6, 0};   // (round 2) FURNACE the round shield
  auto signature = [&](int base, int tier) {
    if (base >= forge::B_TANNER && base <= forge::B_ARMOURER && !L.forgeKit) return true;   // another people's own forms
    return base == forge::B_MYTHIC || (tier >= 5 && (base == forge::B_PARADE || base == forge::B_STARFALL));
  };
  const int hb = r.forgeAx[HumanLook::TIER_HELM][forge::AX_BASE];
  const bool worn = helmBandOf(L) || L.helmForm;
  const int hf = L.helmForm;
  const bool genericHelm = hf == 0 || hf == 1 || hf == 2 || hf == 3 || hf == 4 || hf == 6 || hf == 9;
  if (worn && genericHelm && L.forge[HumanLook::TIER_HELM] && hb > 0 && hb < forge::B_COUNT && !signature(hb, L.tier[HumanLook::TIER_HELM]))
    k.helm = kHelm[hb];
  if (k.helm > 10) { k.hw = forge::guiseHeadwear(k.helm); k.helm = 0; }
  // a culture alloy's helm by its crest (forge i2; the panel: "about 20 of the 37 alloys share one conical spike helm"):
  // the comb, crown and fin on the crested helm, the knife comb and ridge on the nasal, the brow band and finial on the
  // kettle hat, the spike, spire and shards on the spangenhelm, the chimney on the great helm; horns, wings and the flame
  // keep the people's own. A culture's alloys never share a crest (gear_forge's pick rule), so their heads part too
  if (worn && genericHelm && L.forge[HumanLook::TIER_HELM] && hb == forge::B_ALLOY) {
    static const int8_t kByCrest[15] = {0, 1, 9, 1, 2, 9, 4, 3, 4, 9, 2, 4, 0, 0, 0};
    const int cr = r.forgeAx[HumanLook::TIER_HELM][forge::AX_CREST];
    if (cr > 0 && cr < 15) k.helm = kByCrest[cr];
  }
  // (forge i3; the panel: "the Fjord, Dune, Jade alloys all wear their people's one helm, told apart by colour") a culture
  // alloy's guise names its own drawn helm (gear::alloyGuise: a culture's alloys never share one), over the people's
  // signature forms too (their horned, conical and winged helms are not drawn: they all fell back to the spangenhelm)
  if (worn && L.forgeGuise && L.forge[HumanLook::TIER_HELM] && hb == forge::B_ALLOY) {
    const forge::Guise g = forge::unpackGuise(L.forgeGuise);
    // (a mail hood, the coif or the aventail, hides the T5-T6 body armour it drapes: C4's king, his armour swap 23 % of him; there
    // the crested helm, the T5 parade helm, in its place)
    const bool hood = g.helm == 11 || g.helm == 14;
    if (hood && L.tier[HumanLook::TIER_BODY] >= 5) k.helm = 9;
    else if (g.helm && forge::guiseHeadwear(g.helm)) { k.hw = forge::guiseHeadwear(g.helm); k.helm = 0; }
    else if (g.helm) k.helm = g.helm;
  }
  const int sb = r.forgeAx[HumanLook::TIER_SHIELD][forge::AX_BASE];
  const int sf = L.shieldForm;
  const bool genericShield = sf == 0 || (sf >= 1 && sf <= 4) || sf == 6 || sf == 7;
  if (shieldOf(L) && genericShield && L.forge[HumanLook::TIER_SHIELD] && sb > 0 && sb < forge::B_COUNT &&
      !signature(sb, L.tier[HumanLook::TIER_SHIELD]))
    k.shield = kShield[sb];
  // a culture alloy's shield: the people's own shape or one of its two neighbours (round: oval, buckler; kite: heater, oval;
  // heater: kite, round; tower: oval, kite; oval: round, tower; buckler: round, oval) by its rank (HumanLook::alloyRank: the
  // everyday the people's own, the warriors' and the masterwork the neighbours; no rank: by its shoulder and crest picks), so
  // a culture's alloys part on the shield's outline as well as the head's
  if (shieldOf(L) && genericShield && sf >= 1 && L.forge[HumanLook::TIER_SHIELD] && sb == forge::B_ALLOY) {
    static const int8_t kAlt[8][3] = {{0, 0, 0}, {1, 6, 7}, {2, 3, 6}, {3, 2, 1}, {4, 6, 2}, {0, 0, 0}, {6, 1, 4}, {7, 1, 6}};
    const uint8_t* ax = r.forgeAx[HumanLook::TIER_SHIELD];
    const int i = L.alloyRank ? (L.alloyRank - 1) % 3 : (ax[forge::AX_SHOULDER] * 5 + ax[forge::AX_CREST] * 7) % 3;
    if (sf < 8 && kAlt[sf][i]) k.shield = kAlt[sf][i];
  }
  return k;
}

void needsOf(const HumanLook& L, DollRecipe& r) {
  auto need = [&](Role role, const std::string& v) { if (!v.empty()) r.needs.push_back(Need{role, v}); };
  const bool kid = r.body == BodySet::Child || r.body == BodySet::Youth;
  const std::string g = garmentOf(L);
  need(Role::Legs, longGarment(g) ? "tips" : "std");
  need(Role::Arms, "std");
  need(Role::Garment, g);
  if (const char* a = armourOf(L)) need(Role::Armour, a);
  // (P2c) the helm: form1..form10 (HelmForm + 1), else the band (band1..band7; a legacy helmet's band by its outfit)
  // (alloys i1, A6: a heartland band-4/6/7 helm from real loot drew nothing: "helm=band4: no part") a band the library has
  // not drawn wears the nearest drawn one: 4, 6, 7 -> 5 (the crested helm) -> 3 (the great helm) -> 2 (the nasal)
  auto drawnBand = [&](int hb) {
    const pool::Pool& P = pool::get();
    static const int kNext[8] = {0, 0, 0, 2, 5, 3, 5, 5};
    for (int guard = 0; guard < 5 && hb > 0 && P.partsWithRole(Role::Helm, "band" + std::to_string(hb)).empty(); guard++) hb = kNext[std::min(hb, 7)];
    return "band" + std::to_string(hb > 0 ? hb : 2);
  };
  const StyleKit sk = styleKitOf(L, r);
  // (forge round 2) the helm family: a culture alloy's helm piece `helm fam.<v>`; FURNACE's furnace helm and STARFALL's
  // crystal helm where styleKitOf gives the hero its helm (below T5 for STARFALL). A family the pool does not draw yet keeps
  // today's helm and is listed (forgeMissing)
  const ShapeUse su = shapeUseOf(L);
  std::string famHelm;
  if (su.helm) famHelm = std::string("fam.") + forge::shapeName(forge::SX_HELM, su.sx[forge::SX_HELM]);
  else if (sk.helm > 0 && (su.helmBase == forge::B_FURNACE || su.helmBase == forge::B_STARFALL))
    famHelm = su.helmBase == forge::B_FURNACE ? "fam.furnace" : "fam.crystal";
  if (!famHelm.empty() && !drawnFor(Role::Helm, famHelm, r.body)) {
    r.forgeMissing.push_back("helm " + famHelm + " (" + bodySetName(r.body) + ")");
    famHelm.clear();
  }
  if (!famHelm.empty()) { need(Role::Helm, famHelm); r.shape[forge::SX_HELM] = (uint8_t)forge::shapeValueOf(forge::SX_HELM, famHelm.c_str() + 4); }
  else if (sk.hw) { need(Role::Headwear, kHeadwear[sk.hw]); r.guiseHead = true; }
  else if (sk.helm == -2) need(Role::Headwear, "hood");
  else if (sk.helm == -1 && t2Kit(L) && drawnFor(Role::Helm, "softcap", r.body)) need(Role::Helm, "softcap");   // (D55) its own part
  else if (sk.helm == -1) {
    need(Role::Helm, drawnBand(1));
    // (D55) the soft cap drawn as the leather cap's views `when softcap` (flagsOf); neither drawn: listed
    if (t2Kit(L) && g_envMissing && !poolKeysOn("softcap") && !poolKeysOn("forge-tanner")) g_envMissing->push_back("helm softcap (T2 kit, D55) -> the leather cap");
  }
  else if (sk.helm > 0) need(Role::Helm, drawnForm(Role::Helm, sk.helm));
  else if (const int hb = helmBandOf(L)) need(Role::Helm, L.helmForm ? drawnForm(Role::Helm, L.helmForm) : drawnBand(hb));
  else if (L.helmForm) need(Role::Helm, drawnForm(Role::Helm, L.helmForm));
  // (P2c) the shield: the shield arm for every bearer, plus its form (form1..8) or shape (shape0 legacy round, 1..4);
  // form9 is ShieldForm::None: no shield at all (v1's hasShield rule)
  if (shieldOf(L)) {
    need(Role::Shield, "arm");
    need(Role::Shield, sk.shield ? drawnForm(Role::Shield, sk.shield)
                                 : L.shieldForm ? drawnForm(Role::Shield, L.shieldForm) : "shape" + std::to_string(L.shieldStyle));
  }
  // (P2c restated) pauldrons: <family>.<size> (plate: plate brigandine leaf; fur: padded leather; scale: mail scale
  // lamellar), size 2 small, 3 medium, 4 large from the dial; dial 1 is none
  {
    const std::string pd = pauldronOf(L, r.forgeAx[HumanLook::TIER_BODY][forge::AX_SHOULDER]);
    // (forge round 2) a culture alloy's mass NONE wears no shoulders (ladderLook's dial 1 already; the guard here)
    if (!pd.empty() && !(su.body && su.sx[forge::SX_MASS] == forge::MS_NONE)) need(Role::Pauldron, pd);
    if (su.body && su.sx[forge::SX_MASS]) r.shape[forge::SX_MASS] = su.sx[forge::SX_MASS];
  }
  // (forge round 2, contracts 3c.1) the body's silhouette: the skirt replaces the armour's hang (`none` drops it), the
  // over-layer, the sleeves and the chest motif; STARFALL's split skirt and FURNACE's heavy arms. A value the pool does not
  // draw yet keeps today's parts and is listed
  {
    const char* bs = bodySetName(r.body);
    auto want = [&](Role role, const std::string& v, int axis, int val) {
      if (drawnFor(role, v, r.body)) { need(role, v); r.shape[axis] = (uint8_t)val; return true; }
      r.forgeMissing.push_back(std::string(roleName(role)) + " " + v + " (" + bs + ")");
      return false;
    };
    int skirt = su.body ? su.sx[forge::SX_SKIRT] : 0, arm = su.body ? su.sx[forge::SX_ARM] : 0;
    if (!su.body && su.bodyBase == forge::B_STARFALL && armourOf(L)) skirt = forge::SK_SPLIT;
    if (!su.body && su.bodyBase == forge::B_FURNACE && armourOf(L)) arm = forge::AR_HEAVY;
    if (skirt == forge::SK_NONE) { r.dropHang = true; r.shape[forge::SX_SKIRT] = (uint8_t)skirt; }
    else if (skirt > 0 && want(Role::Overlay, std::string("skirt.") + forge::shapeName(forge::SX_SKIRT, skirt), forge::SX_SKIRT, skirt)) r.dropHang = true;
    if (arm == forge::AR_WIDE || arm == forge::AR_HEAVY) want(Role::Overlay, std::string("sleeve.") + forge::shapeName(forge::SX_ARM, arm), forge::SX_ARM, arm);
    else if (arm) r.shape[forge::SX_ARM] = (uint8_t)arm;   // close: the arm's own line; bare: regionsOf
    if (su.body) {
      const int ly = su.sx[forge::SX_LAYER], mo = su.sx[forge::SX_MOTIF];
      if (ly == forge::LY_TABARD) want(Role::Overlay, "tabard", forge::SX_LAYER, ly);
      else if (ly == forge::LY_SASH) want(Role::Overlay, "sash", forge::SX_LAYER, ly);
      else if (ly == forge::LY_HALFCAPE) want(Role::Cloak, "halfcape", forge::SX_LAYER, ly);
      else if (ly) r.shape[forge::SX_LAYER] = (uint8_t)ly;   // none; cape (ladderLook's noble cloak); scarf (ladderLook's neck scarf)
      if (mo == forge::MO_PLAIN) r.shape[forge::SX_MOTIF] = (uint8_t)mo;   // PLAIN: the armour's own planes, no part
      else if (mo) want(Role::Overlay, std::string("motif.") + forge::shapeName(forge::SX_MOTIF, mo), forge::SX_MOTIF, mo);
    }
  }
  // (P2c forge, contracts 3c) the style's parts, where the library draws them (a decoration, never a hole: the forge
  // coverage, rpg_test --doll-coverage --scope forge, names what is not drawn yet): the body's collar, cut tile, edge stamp,
  // fittings and limb tile; the helm's crest (from T3: T1 bare, T2's small cap plain, STYLE_P2 27.4; forge i2, C12a); the gloves' and boots' limb tiles
  bool forgeLimbBody = false;
  {
    const pool::Pool& P = pool::get();
    auto drawn = [&](const std::string& v) {
      for (int i : P.partsWithRole(Role::Overlay, v)) if (P.parts[(size_t)i].bodyMask >> (int)r.body & 1) return true;
      return false;
    };
    auto want = [&](int slot, int axis, const char* prefix) {
      const int v = r.forgeAx[slot][axis];
      if (!v) return false;
      const std::string n = std::string(prefix) + forge::valueName(axis, v);
      if (!drawn(n)) return false;
      need(Role::Overlay, n);
      return true;
    };
    const int B = HumanLook::TIER_BODY;
    if (armourOf(L)) {
      want(B, forge::AX_COLLAR, "collar.");
      want(B, forge::AX_CUT, "cut.");
      want(B, forge::AX_EDGE, "edge.");
      want(B, forge::AX_FIT, "fit.");
      forgeLimbBody = want(B, forge::AX_LIMB, "limb.");
    }
    if ((helmBandOf(L) || L.helmForm) && (L.tier[HumanLook::TIER_HELM] == 0 || L.tier[HumanLook::TIER_HELM] >= 3) && sk.helm != -2) want(HumanLook::TIER_HELM, forge::AX_CREST, "crest.");
    if (L.gloves) want(HumanLook::TIER_GLOVES, forge::AX_LIMB, "limb.");
    if (L.boots) want(HumanLook::TIER_BOOTS, forge::AX_LIMB, "limb.");
  }
  // (P2c) the tabard over the armour of every Outfit::Guard look (watch, soldiers, heralds): its colour is the accent
  // (polish i1) only where the library draws a tabard for the body: the watchwoman has none yet, and a missing tabard sent
  // her whole figure back to v1 in a row of rig-2 guards; she wears her armour bare until LADDER-ARMOUR draws hers
  if (L.outfit == Outfit::Guard && !(su.body && su.sx[forge::SX_LAYER])) {   // (forge round 2: a styled alloy's layer replaces it)
    const pool::Pool& P = pool::get();
    bool drawn = false;
    for (int i : P.partsWithRole(Role::Overlay, "tabard")) if (P.parts[(size_t)i].bodyMask >> (int)r.body & 1) drawn = true;
    if (drawn) need(Role::Overlay, "tabard");
  }
  // (P2c) the armour's material tile over its sleeves (D3), where the pool draws one (a decoration, never a hole)
  // (P2c forge) the body style's limb tile, where drawn, takes the material's place on the sleeves
  if (const char* t = sleeveTileOf(L))
    if (!forgeLimbBody && !pool::get().partsWithRole(Role::Overlay, t).empty()) need(Role::Overlay, t);
  // (envelope fix round, D55) the T2 kit's hood thrown back: the cowl in the scarf's place, once drawn for the body
  if (t2Kit(L) && drawnFor(Role::Neck, "cowl", r.body)) need(Role::Neck, "cowl");
  else {
    if (t2Kit(L) && g_envMissing) g_envMissing->push_back(std::string("neck cowl (T2 kit, D55; ") + bodySetName(r.body) + ")");
    if (L.neck) need(Role::Neck, neckName(L.neck));
  }
  if (L.jewellery & 2) need(Role::Neck, "torc");
  if (L.amulet) need(Role::Neck, "amulet");
  // the head
  if (r.body == BodySet::Child) need(Role::Head, kHeadChild[std::min<int>(L.head, 3)]);
  else need(Role::Head, headTypeName(L.body, L.head));
  need(Role::Nose, r.body == BodySet::Child ? (L.nose & 1 ? "straight" : "button") : noseName(L.body, L.nose));
  need(Role::Brow, r.body == BodySet::Child ? (L.brow & 1 ? "thin" : "soft") : browName(L.body, L.brow));
  need(Role::Ears, L.people == 2 ? "elf" : L.people == 1 ? "half" : "human");
  if (!kid)
    for (int b = 0; b < 6; b++) if (L.marks >> b & 1) need(Role::Marks, kMarks[b]);
  for (const char* e : {"blink", "hurt", "closed", "talk", "fierce"}) need(Role::Expr, e);
  if (L.facePaint >= 1 && L.facePaint <= 4) need(Role::Paint, kPaint[L.facePaint]);
  // the patterns laid as tiles over the garment's cloth (stripes, checks, dots, vines: `role overlay <name>`, `tags tile`);
  // the border trim and the embroidered hem are the garment edge's colour (regionsOf), embroidery's motifs `when` views
  if (L.pattern == 1 || L.pattern == 2 || L.pattern == 4 || L.pattern == 6) need(Role::Overlay, kPattern[L.pattern]);
  if (L.jewellery & 1) need(Role::Earring, "earrings");
  if (L.beard && L.body == 0 && !kid) need(Role::Beard, beardName(L.beardStyle));
  if (r.body == BodySet::Child) need(Role::Hair, childHairName(childHairOf(L.hair, L.hairVar)));
  else need(Role::Hair, hairStyleName(hairStyleOf(L.hair, L.hairVar)));
  // what sits on the head, as v1 paints it (art_human.cpp: helmets and the legacy hood win over the culture's headwear)
  if (const int hw = headwearOf(L)) need(Role::Headwear, kHeadwear[hw]);
  else if (L.hood && !(L.helmet || L.helmStyle || L.helmForm)) need(Role::Headwear, "legacyhood");
  if (L.cloak >= 1 && L.cloak <= 3) need(Role::Cloak, kCloak[L.cloak]);
  else if (L.cape) need(Role::Cloak, "cape");
  // what is carried: a sword rides sheathed at the hip (LOCO) and is drawn in hand to fight (ACTION, with its arc).
  // (P2c) a fighter's weapon is the library's `<weapon>.<form>` (heldOf: P2b's bare name while the pool serves no library
  // map for it, so a look covered today stays covered); civil tools keep their bare names
  const std::string held = heldOf(L);
  if (L.weapon == 1) { need(Role::Sheath, "sword"); need(Role::Held, held); need(Role::Fx, "arc"); }
  else if (L.weapon == 2 || L.weapon == 5 || L.weapon == 6) { need(Role::Sheath, kHeld[L.weapon]); need(Role::Held, held); }
  else if (L.weapon >= 3 && L.weapon <= 12) need(Role::Held, held);
  if (!L.civil) {
    if (L.weapon == 7) need(Role::Sheath, "greatsword");   // across the back
    if (L.weapon == 3) { need(Role::Sheath, "quiver"); need(Role::Sheath, "bowback"); need(Role::Fx, "twang"); }   // (P2c fix) the quiver; the bow on the back at rest
    if (L.weapon == 8) need(Role::Fx, "thrust");
  }
}

// the sleeve and the legwear of each civilian garment: which slot the arm and leg libraries' regions take (STYLE_P2 24;
// unmapped regions draw as drawn: the arm drawn in a long cloth1 sleeve, the leg in cloth2 trousers and leather boots).
// GARB-M / GARB-W own the garments' looks: a change of sleeve is a line here (ask DOLL).
void regionsOf(const HumanLook& L, const std::string& g, DollRecipe& r) {
  auto set = [&](Region rg, Slot s) { r.region[(int)rg] = (uint8_t)s; };
  if (g == "rags") {   // bare arms, bare shins and feet
    set(Region::Upper, Slot::Skin); set(Region::Fore, Slot::Skin); set(Region::Cuff, Slot::Skin);
    set(Region::Shin, Slot::Skin); set(Region::Boot, Slot::Skin);
    if (r.body == BodySet::Woman) set(Region::Knee, Slot::Skin);   // (GARB-W) her torn shift ends at the knee: bare knees under it
  }
  if (g == "dress") { set(Region::Upper, Slot::Linen); set(Region::Fore, Slot::Skin); set(Region::Cuff, Slot::Skin); }   // the chemise's puff sleeve
  if (g == "apron" || g == "smock") { set(Region::Upper, Slot::Linen); set(Region::Fore, Slot::Skin); set(Region::Cuff, Slot::Skin); }   // rolled sleeves
  if (g == "jerkin") { set(Region::Upper, Slot::Linen); set(Region::Fore, Slot::Linen); set(Region::Cuff, Slot::Linen); }   // the shirt under it
  if (g == "kilt") set(Region::Knee, Slot::Skin);
  if (g == "coat" || g == "robe" || g == "kaftan") set(Region::Cuff, Slot::Accent);   // a trimmed cuff
  // (GARB-M) the man's sleeves: the jerkin over his own shirt (the arm as drawn: the shirt's colour, the archer's
  // bracers), the culture tunic's trimmed cuff, the linen shirt under a kilt or a poncho, the gown's gilt cuff, the
  // wrap's bare arms
  if (r.body == BodySet::Man) {
    if (g == "jerkin") r.region[(int)Region::Upper] = r.region[(int)Region::Fore] = r.region[(int)Region::Cuff] = 0xFF;
    if (g == "culturetunic") set(Region::Cuff, Slot::Accent);
    if (g == "kilt" || g == "poncho") { set(Region::Upper, Slot::Linen); set(Region::Fore, Slot::Linen); set(Region::Cuff, Slot::Linen); }
    if (g == "gown") set(Region::Cuff, Slot::Metal2);
    if (g == "wrap") { set(Region::Upper, Slot::Skin); set(Region::Fore, Slot::Skin); set(Region::Cuff, Slot::Skin); }
  }
  // the garment's edge: a border trim or an embroidered hem takes the pattern's colour
  if (L.pattern == 3 || L.pattern == 5) set(Region::Edge, Slot::Pattern);
  // (P2c, contracts 3) the sleeves by armour form (mail's only where its tile is drawn: D3 keeps P2b's drawn mail sleeves
  // otherwise); gloves recolour the hand and the flare, boots the boot, tall boots and greaves the shin, a metal cop the knee
  if (const char* a = armourOf(L)) {
    const std::string m = a;
    // (INTEGRATE i1) mail's U / F are metal whether or not the mail tile exists: P2b's drawn mail sleeves cover the loco
    // arms, and an arm no sleeve map draws (the shield arm, A3's re-cut sword arm) shows steel, not the tunic's cloth1
    if (m == "mail") { set(Region::Upper, Slot::Metal); set(Region::Fore, Slot::Metal); }
    else if (m == "scale" || m == "lamellar" || m == "leaf") set(Region::Upper, Slot::Metal);
    else if (m == "plate") { set(Region::Upper, Slot::Metal); set(Region::Fore, Slot::Metal); }
    else if (m == "leather") set(Region::Fore, Slot::Leather);
    else if (m == "padded") { const Slot q = L.body == 1 ? Slot::Linen : Slot::Cloth3; set(Region::Upper, q); set(Region::Fore, q); }   // (ladder i2) the quilt: buff, hers linen
    else if (m == "brigandine") { set(Region::Upper, Slot::Cloth1); set(Region::Fore, Slot::Cloth1); }
  }
  // (forge round 2, contracts 3c.1) a culture alloy's BARE arm: the forearm and the cuff in skin (the cuff linen on a cloth
  // body); its boots' cops, greaves and toes in the alloy from T4 (D39)
  {
    const ShapeUse su = shapeUseOf(L);
    if (su.body && su.sx[forge::SX_ARM] == forge::AR_BARE) {
      const char* a = armourOf(L);
      set(Region::Fore, Slot::Skin);
      set(Region::Cuff, a && std::string(a) == "padded" ? Slot::Linen : Slot::Skin);
    }
  }
  if (L.gloves) {
    set(Region::Hand, L.gloves >= 2 ? Slot::Metal : Slot::Leather);
    if (L.gloves >= 3) set(Region::Flare, Slot::Metal);
  }
  if (L.boots) {
    set(Region::Boot, L.boots >= 3 ? Slot::Metal : Slot::Leather);
    if (L.boots == 2) set(Region::Shin, Slot::Leather);
    if (L.boots >= 3) { set(Region::Shin, Slot::Metal); set(Region::Knee, Slot::Metal); }
    if (L.tier[HumanLook::TIER_BOOTS] >= 4 && L.forge[HumanLook::TIER_BOOTS] && (L.forge[HumanLook::TIER_BOOTS] & 0xFF) == forge::B_ALLOY && L.forgeShape) {
      set(Region::Boot, Slot::Metal); set(Region::Shin, Slot::Metal); set(Region::Knee, Slot::Metal);
    }
  }
  // (forge fix i1, the owner: "a single-hue alloy tint wipes out the mix of materials: keep leather straps, cloth and skin in
  // their own colours") a culture alloy's figure keeps a leather foot under its alloy greaves and cops, and a leather glove
  // under a gauntlet's alloy cuff (the flare) below T5: the alloy reads as plate on a man, not a painted doll
  {
    const ShapeUse su = shapeUseOf(L);
    if (su.body) {
      if (L.boots >= 3 || L.tier[HumanLook::TIER_BOOTS] >= 4) set(Region::Boot, Slot::Leather);
      if (L.gloves >= 2 && L.tier[HumanLook::TIER_BODY] <= 4) set(Region::Hand, Slot::Leather);
    }
  }
}

void flagsOf(const HumanLook& L, DollRecipe& r) {
  auto flag = [&](const char* f) { r.flags.push_back(f); };
  flag(bodySetName(r.body));
  if (L.age == 3) flag("elder");
  if (r.build == 1) flag("slim");
  if (r.build == 2) flag("broad");
  if (L.people == 1) flag("half");
  if (L.people == 2) flag("elf");
  if (L.amulet) flag("amulet");
  if (L.ring) flag("ring");
  // (PROPS) armed = a weapon sheathed at the hip (needsOf's sheath: sword, axe, dagger, mace), where the off hand rests on
  // the hilt; a stick, staff, bow, spear, ledger, scroll or horn is carried, and both arms keep their swing
  if (L.weapon == 1 || L.weapon == 2 || L.weapon == 5 || L.weapon == 6) flag("armed");
  if (L.pattern >= 1 && L.pattern <= 6) { flag("pattern"); flag(kPattern[L.pattern]); }
  // hair under a hat or a hood: hair parts draw their `capped` views (STYLE_P2 21: capped variants for volume)
  // (OUTER) the circlet (headwear 6) is a thin band over the hair: it hides nothing, so the hair keeps its full style
  if ((L.headwear >= 1 && L.headwear <= 8 && L.headwear != 6) || L.hood || L.helmet || L.helmStyle || L.helmForm) flag("capped");
  if (const int hw = headwearOf(L)) flag(kHeadwear[hw]);
  if (L.hood) flag("hood");
  if (styleKitOf(L, r).helm == -2 && !L.hood) flag("hood");   // (forge i2) HOMESPUN's hood in the helm's place
  if (t2Kit(L)) {   // (envelope fix round, D55) the T2 kit's pieces as flags: the leather cap's `when softcap` views, and any view of
    // another part that makes room for the mantle or the cowl
    if (styleKitOf(L, r).helm == -1) flag("softcap");
    flag("t2kit");
  }
  if (const int ghw = styleKitOf(L, r).hw) if (!headwearOf(L)) flag(kHeadwear[ghw]);   // (forge i3) the guise's metal head
  if (L.beard && L.body == 0) flag("beard");
  if (L.age == 1 || L.age == 2) flag("kid");
  if (r.body == BodySet::Youth) flag(L.body == 1 ? "girl" : "boy");   // (KIN-YOUTH) the youth's garments fitted per sex
  if (L.glow) flag("glint");
  if (L.jewellery & 1) flag("earrings");
  for (int b = 0; b < 6; b++) if (L.marks >> b & 1) flag(kMarks[b]);
  // (P2c, contracts 3) the arms flags `when` tests
  auto flagS = [&](const std::string& f) { r.flags.push_back(f); };
  if (const int hb = helmBandOf(L)) flagS("band" + std::to_string(hb));
  if (L.armourTint) flag(L.sheen ? "alloy" : "bronze");
  if (L.sheen >= 1 && L.sheen <= 8) {
    static const char* const kSheen[8] = {"matte", "bright", "dark", "banded", "iridescent", "glowing", "pale", "burnished"};
    flagS(std::string("sheen-") + kSheen[L.sheen - 1]);
  }
  static const char* const kOrn[10] = {"plumes", "furtrim", "gilding", "horsetail", "studs", "etching", "tassels", "scallops", "rivets", "filigree"};
  for (int b = 0; b < 10; b++) if (L.armsOrnament >> b & 1) flag(kOrn[b]);
  if (L.skirt >= 1 && L.skirt <= 4) flagS("skirt" + std::to_string(L.skirt));
  if (L.crest >= 1 && L.crest <= 4) flagS("crest" + std::to_string(L.crest));
  if (L.pauldron >= 2 && L.pauldron <= 4) flagS("pauldron" + std::to_string(L.pauldron));
  if (shieldOf(L)) flag("shield");
  const Family fam = familyOf(L);
  if (!L.civil && L.weapon >= 1 && L.weapon <= 9) {
    if (fam == Family::Greatsword || fam == Family::Spear || fam == Family::Staff) flag("twohand");
    else if (fam != Family::Bow) flag("onehand");
  }
  flag(familyName(L.civil ? Family::Unarmed : fam));
  if (!L.civil) {
    const std::string form = weaponFormOf(L);
    if (!form.empty()) {
      if (L.weapon == 1 || L.weapon == 7) flagS("blade-" + form);
      else if (L.weapon == 8) flagS("pole-" + form);
      else if (L.weapon == 3) flagS("bow-" + form);
    }
  }
  if (L.glow && L.glowColor == 0xFF2868FFu) flag("mythic");   // (P2c ladder) gear_look.h kMythicGlow
  // (P2c forge, contracts 3c) the forge flags: forge-<base> and the body's picks, the helm's crest, the shield's boss and rim,
  // the gloves' and boots' limbs; the armour's form; the metal in hand
  {
    const uint8_t* b = r.forgeAx[HumanLook::TIER_BODY];
    if (L.forge[HumanLook::TIER_BODY]) {
      flagS(std::string("forge-") + forge::valueName(forge::AX_BASE, b[0]));
      static const char* const kPre[8] = {"", "shoulder-", "", "cut-", "edge-", "collar-", "limb-", "fit-"};
      for (int a : {(int)forge::AX_SHOULDER, (int)forge::AX_CUT, (int)forge::AX_EDGE, (int)forge::AX_COLLAR, (int)forge::AX_LIMB, (int)forge::AX_FIT})
        if (b[a]) flagS(std::string(kPre[a]) + forge::valueName(a, b[a]));
    }
    const uint8_t* h = r.forgeAx[HumanLook::TIER_HELM];
    if (h[forge::AX_CREST]) flagS(std::string("crest-") + forge::valueName(forge::AX_CREST, h[forge::AX_CREST]));
    const uint8_t* s = r.forgeAx[HumanLook::TIER_SHIELD];
    if (s[forge::AX_FIT]) flagS(std::string("boss-") + forge::valueName(forge::AX_FIT, s[forge::AX_FIT]));
    if (s[forge::AX_EDGE]) flagS(std::string("rim-") + forge::valueName(forge::AX_EDGE, s[forge::AX_EDGE]));
    const uint8_t* g = r.forgeAx[HumanLook::TIER_GLOVES];
    if (g[forge::AX_LIMB]) flagS(std::string("glimb-") + forge::valueName(forge::AX_LIMB, g[forge::AX_LIMB]));
    const uint8_t* t = r.forgeAx[HumanLook::TIER_BOOTS];
    if (t[forge::AX_LIMB]) flagS(std::string("blimb-") + forge::valueName(forge::AX_LIMB, t[forge::AX_LIMB]));
    if (L.forge[HumanLook::TIER_BODY])
      if (const char* a = armourOf(L)) flagS(std::string("form-") + a);
    static const char* const kBlade[12] = {"", "leather", "bronze", "iron", "steel", "gilded", "steel", "obsidian", "ember", "starmetal", "alloy", "mythic"};
    const int bk = L.bladeKind ? L.bladeKind : L.metalKind;
    if (bk >= 1 && bk <= 11 && !L.civil && L.weapon >= 1 && L.weapon <= 9) flagS(std::string("blademetal-") + kBlade[bk]);
  }
  static const char* const kGlint[5] = {"glint-weapon", "glint-body", "glint-helm", "glint-shield", "glint-cloak"};
  for (int b = 0; b < 5; b++) if (L.glow >> b & 1) flag(kGlint[b]);
  // (forge round 2, contracts 3c.1) the silhouette's flags, as applied: helmfam- skirt- mass- layer- arm- motif-; alloyshape (a
  // styled culture alloy's body: the T5 surcoat views step aside, D38); alloyface (its shield tinted, D39); glowseams
  // (FURNACE at any tier, a glowing alloy from T4: D28, D40)
  {
    static const char* const kPre[forge::SX_COUNT] = {"helmfam-", "skirt-", "mass-", "layer-", "arm-", "motif-"};
    for (int a = 0; a < forge::SX_COUNT; a++)
      if (r.shape[a]) flagS(std::string(kPre[a]) + forge::shapeName(a, r.shape[a]));
    const ShapeUse su = shapeUseOf(L);
    if (su.body) flag("alloyshape");
    // (P2c envelope, contracts 3d, D51) the helm's variant: a culture alloy's from its forgeShape (the board's 37: kBoardHeads),
    // every other look the variant its mass drew in the fix round (no approved pixel moves); views key `helmfam-<v>,variant-<n>`
    if (r.shape[forge::SX_HELM]) {
      const int v = su.helm ? (int)forge::shapeVariant(L.forgeShape) : forge::legacyHelmVariant(r.shape[forge::SX_HELM], r.shape[forge::SX_MASS], su.body);
      r.helmVariant = (uint8_t)v;
      if (v > 0) flagS("variant-" + std::to_string(v));
    }
    // (forge fix i1) KNAPPED's obsidian: glassy specular streaks across its facets (the compositor's glass pass)
    if ((L.metalKind == 7 || (L.metalKind == 10 && (L.alloyFam || L.armourTint) && alloyFamilyMetal(L.alloyFam ? L.alloyFam - 1 : alloyFamilyOf(L.armourTint, L.sheen)) == 5)) && armourOf(L) && L.tier[HumanLook::TIER_BODY]) flag("glass");
    if (su.body && shieldOf(L)) flag("alloyface");
    // (envelope fix round, D58; the packager: A3 ENKHOBRASS's torso read blue cloth, only the helm brass) a brass or bronze
    // everyday alloy's torso armour and chest motif paint in Metal: the compositor moves their cloth, trim and leather cells to
    // the alloy's brass (the people's blue stays on cloth: the collar, the cuffs, the sash, the scarf)
    if (L.metalKind == 10 && L.alloyRank <= 1 && armourOf(L) && L.tier[HumanLook::TIER_BODY] &&
        (L.alloyFam || L.armourTint) && alloyFamilyMetal(L.alloyFam ? L.alloyFam - 1 : alloyFamilyOf(L.armourTint, L.sheen)) <= 1)
      flag("brassbody");
    if ((su.bodyBase == forge::B_FURNACE && armourOf(L)) || (L.sheen == 6 && L.tier[HumanLook::TIER_BODY] >= 4 && su.bodyBase == forge::B_ALLOY))
      flag("glowseams");
    // (silhouettes i2, the blind panel: "Emberforged: hot orange only on the visor slits and the boss, 3 % of its pixels; a clay
    // stove") the furnace's cracks glow on every piece (the compositor's furnace pass)
    if (su.bodyBase == forge::B_FURNACE && armourOf(L) && L.tier[HumanLook::TIER_BODY] <= 5) flag("furnace");
  }
}

// ---------------------------------------------------------------- (P2c ladder, contracts 3b) the tiers
// The resolver's effective dials by tier (the look's fields stay as they are): T1 no pauldron, skirt short, no crest or
// plume; T2 pauldron small, skirt mid, crest small; T3-T4 the look's own; T5-T6 the look's plus `plumes` on a helm with
// a crest. A look with a tier and no culture dials takes the heartland line (pauldron none small medium medium large
// large; skirt short short mid mid knee knee) and, with no body form, its armour form from its metal (leather leather,
// bronze and iron mail, steel and up plate). Tiers only cap what the sim gave, never lift it.
HumanLook ladderLook(const HumanLook& L) {
  if (!L.topTier()) return L;
  HumanLook E = L;
  const int tb = L.tier[HumanLook::TIER_BODY];
  // (polish i1) the T1 floor (gear::fillLadder): a T1 body with no armour is T1's padded jerkin in the wearer's shirt colour,
  // T1 feet with no boots are leather boots (the player starts in T1 gear: the rags of the rags-to-riches ladder)
  using O = Outfit;
  const bool cloth = L.outfit == O::Tunic || L.outfit == O::Rags;
  if ((L.floor >> HumanLook::TIER_BODY & 1) && tb == 1 && !L.armorStyle && !L.bodyForm && cloth && !L.civil) { E.armorStyle = 1; E.bodyForm = 1; }
  if ((L.floor >> HumanLook::TIER_BOOTS & 1) && L.tier[HumanLook::TIER_BOOTS] == 1 && !L.boots) E.boots = 1;
  if (tb && !L.pauldron && !L.skirt && !L.crest) {
    static const uint8_t kPd[7] = {0, 1, 2, 3, 3, 4, 4}, kSk[7] = {0, 2, 2, 3, 3, 4, 4};
    E.pauldron = kPd[tb];
    E.skirt = kSk[tb];
    if (!L.bodyForm && L.metalKind && (L.armorStyle || L.outfit == Outfit::Leather || L.outfit == Outfit::Chain || L.outfit == Outfit::Plate)) {
      const int m = L.metalKind;
      E.bodyForm = (uint8_t)(m == 1 ? 2 : (m == 2 || m == 3) ? 3 : 7);   // BodyArm + 1: leather 2, mail 3, plate 7
    }
  }
  // (alloys i3) the one lift: a T5-T6 body with shoulder plates wears the large ones (the heartland line's T5-T6 size), so the
  // top tiers never shrink at the shoulder (a gilded band-4 plate's sim dial is a size under an alloy's; the panel read the
  // shoulders as the step up, and the medium plate left the heater's grip bare facing U)
  if (tb >= 5 && E.pauldron >= 2 && E.pauldron < 4) E.pauldron = 4;
  // (polish i1, the owner: "six identical guards in a row") a ranked fighter's own kit (HumanLook::kitVar, from census:
  // gear::rankTiers): his helm, shield, gloves and boots a step worse or better than his rank's (a dented old helm, a
  // brass-trimmed shield), the shoulder and skirt dials a step either way, a third bare-handed, a sergeant in six with a
  // travel cloak in the livery, and in the village watch one in eight bare-headed. The body keeps the rank.
  if (L.kitVar) {
    uint32_t h = L.kitVar * 0x9E3779B1u;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    auto jit = [&](int slot, int bit) {
      int t = E.tier[slot];
      if (!t) return;
      const int j = (int)(h >> bit & 7u);
      if (j <= 1 && t > 1) t--;
      else if (j == 2 && t < 6 && t < tb + 1) t++;
      E.tier[slot] = (uint8_t)t;
    };
    jit(HumanLook::TIER_HELM, 0);
    jit(HumanLook::TIER_SHIELD, 3);
    jit(HumanLook::TIER_GLOVES, 6);
    jit(HumanLook::TIER_BOOTS, 9);
    if (E.pauldron) { const int j = (int)(h >> 20 & 3u); if (j == 0 && E.pauldron > 1) E.pauldron--; else if (j == 1 && E.pauldron < 4) E.pauldron++; }
    if (E.skirt) { const int j = (int)(h >> 22 & 3u); if (j == 0 && E.skirt > 1) E.skirt--; else if (j == 1 && E.skirt < 4) E.skirt++; }
    if (tb <= 3 && E.gloves && (h >> 12 & 3u) == 0) { E.gloves = 0; E.tier[HumanLook::TIER_GLOVES] = 0; }
    if (tb >= 2 && tb <= 4 && (h >> 24) % 6u == 0 && !E.cloak && !E.cape) {
      E.cloak = 1;
      const uint32_t c = L.tabardColor ? L.tabardColor : rgba(110, 40, 40);
      auto dk = [&](int sh) { return (uint32_t)((c >> sh & 255u) * 62u / 100u); };
      E.cloakColor = 0xFF000000u | dk(16) << 16 | dk(8) << 8 | dk(0);
      E.tier[HumanLook::TIER_CLOAK] = (uint8_t)tb;   // (no crest: a crest dial closes the helm, and the face must read)
    } else if (tb <= 2 && (h >> 28) % 8u == 3 && (E.helmet || E.helmStyle || E.helmForm)) {
      E.helmet = false; E.helmStyle = 0; E.helmForm = 0; E.tier[HumanLook::TIER_HELM] = 0;
    }
    // (polish i2, the panel: "Heartland and Highland lineups are near clones: same kit, varying only in skin and beard";
    // "Highland soldiers wear red caps with no kettle hat") the man's own kit reaches the shapes that read at 1x, all
    // within his people's outputs: a second hash (the first's bits are spent above)
    uint32_t h2 = h * 0x85EBCA6Bu ^ 0x5F3759DFu;
    h2 ^= h2 >> 13; h2 *= 0xC2B2AE35u; h2 ^= h2 >> 16;
    const bool helmed = E.helmet || E.helmStyle || E.helmForm;
    // the bonnet peoples (census: the Highland soldier's cap): one in three has bought the people's kettle hat (iron)
    if (!helmed && !E.hood && E.headwear == 2 && tb >= 2 && h2 % 3u == 0) {
      E.helmet = true; E.helmStyle = 2; E.helmForm = 2; E.headwear = 0;
      E.tier[HumanLook::TIER_HELM] = (uint8_t)tb;
    } else if (!helmed && E.headwear == 2 && E.headColor) {
      // the rest dye their caps unevenly: the livery's shade, a darker dyeing, or the hose's cloth
      const uint32_t c = E.headColor;
      auto sh = [&](uint32_t col, uint32_t pct) {
        return 0xFF000000u | ((col >> 16 & 255u) * pct / 100u) << 16 | ((col >> 8 & 255u) * pct / 100u) << 8 | (col & 255u) * pct / 100u;
      };
      const uint32_t j = h2 >> 4 & 3u;
      if (j == 1) E.headColor = sh(c, 72);
      else if (j == 2 && E.bottomColor) E.headColor = E.bottomColor | 0xFF000000u;
    }
    // the coat: a padded man of rank 2 or more may wear a leather jerkin (one in three); a leather man a quilted coat (one
    // in four); a mailed man of rank 3 or less a leather jerkin over his shirt (one in four). The body's TIER stays the rank's
    if (tb >= 2 && E.bodyForm) {
      const uint32_t j = h2 >> 8 & 15u;
      if (E.bodyForm == 1 && j % 3u == 0) E.bodyForm = 2;
      else if (E.bodyForm == 2 && j % 4u == 1) E.bodyForm = 1;
      else if (E.bodyForm == 3 && tb <= 3 && j % 4u == 2) E.bodyForm = 2;
    }
    // the shield's face: one in four painted dark in the livery, one in four a pale field (the device as drawn)
    if (E.tier[HumanLook::TIER_SHIELD] && !E.shieldField) {
      const uint32_t j = h2 >> 14 & 3u;
      const uint32_t c = E.tabardColor ? E.tabardColor : rgba(110, 40, 40);
      if (j == 1) E.shieldField = 0xFF000000u | ((c >> 16 & 255u) * 58u / 100u) << 16 | ((c >> 8 & 255u) * 58u / 100u) << 8 | (c & 255u) * 58u / 100u;
      else if (j == 2) E.shieldField = rgba(214, 200, 162);
    }
    // (polish i3, the panel: "within a culture the six seeds still share one palette: all red tabards, the same hose") the
    // livery's dye lots: the coat and tabard in the people's colours, but one in four dyed darker, one in eight sun-faded
    // toward undyed wool, one in eight a deeper, richer lot, one in eight pale and washed out; the hose of whatever cloth the man owns (his own, a dark
    // brown, a grey, or the livery's dark). The hue stays the people's: a row reads as one company of different men.
    // Rank and file only (body tier 4 or less): an officer's, a lord's or a king's cloth is the dear dye, as drawn.
    if (tb <= 4) {
      auto mixC = [](uint32_t a, uint32_t b, uint32_t pct) {   // a toward b by pct %
        auto ch = [&](int sh) { const int x = (int)(a >> sh & 255u), y = (int)(b >> sh & 255u); return (uint32_t)(x + (y - x) * (int)pct / 100); };
        return 0xFF000000u | ch(16) << 16 | ch(8) << 8 | ch(0);
      };
      const uint32_t dye = h2 >> 18 & 7u;
      auto lot = [&](uint32_t c) -> uint32_t {
        if (!c) return c;
        // a darker dyeing, only of a cloth with light to lose: a dark coat dyed darker drowns the lit edge in the grass (C14:
        // the heartland bandit's brown read 33 over the darkest ground)
        const int lu = (int)(0.299f * (c >> 16 & 255u) + 0.587f * (c >> 8 & 255u) + 0.114f * (c & 255u));
        if (dye == 1 || dye == 2) return lu >= 110 ? mixC(c, 0xFF000000u, 34) : c;
        if (dye == 3) return mixC(c, rgba(158, 148, 124), 48);                  // sun-faded toward undyed wool
        if (dye == 5) return mixC(c, rgba(236, 224, 196), 26);                  // a pale, washed-out lot
        return c;
      };
      E.topColor = lot(E.topColor);
      E.tabardColor = lot(E.tabardColor);
      if (dye == 4) {   // a richer lot: the strongest channel lifted, the others pressed (more saturated, same hue)
        auto rich = [](uint32_t c) {
          int ch[3] = {(int)(c & 255u), (int)(c >> 8 & 255u), (int)(c >> 16 & 255u)};
          const int mx = std::max(ch[0], std::max(ch[1], ch[2]));
          for (int& v : ch) v = v == mx ? std::min(255, v + 24) : v * 66 / 100;
          return 0xFF000000u | (uint32_t)ch[2] << 16 | (uint32_t)ch[1] << 8 | (uint32_t)ch[0];
        };
        E.topColor = rich(E.topColor);
        E.tabardColor = rich(E.tabardColor);
      }
      const uint32_t hose = h2 >> 21 & 3u;
      if (hose == 1) E.bottomColor = rgba(74, 58, 44);
      else if (hose == 2) E.bottomColor = rgba(98, 98, 104);
      else if (hose == 3 && L.tabardColor) E.bottomColor = mixC(L.tabardColor, 0xFF000000u, 45);
    }
  }
  // (forge i3) a culture alloy's guise (gear::alloyGuise): its skirt length, its shoulders' size (a people that wears none,
  // the Dune, takes them: the style's shoulder must show) and the masterwork's cloak in the people's colour, darkened. The
  // tier caps below still hold
  if (L.forgeGuise && tb && L.forge[HumanLook::TIER_BODY] && (L.forge[HumanLook::TIER_BODY] & 0xFF) == forge::B_ALLOY) {
    const forge::Guise g = forge::unpackGuise(L.forgeGuise);
    // (lifts only: the ladder's figure never shrinks a step up, C12c; a T4 hero in his people's everyday alloy kept T3's size)
    if (g.skirt) E.skirt = std::max(E.skirt, g.skirt);
    if (g.pauldron) E.pauldron = std::max<uint8_t>(E.pauldron, std::max<uint8_t>(g.pauldron, tb >= 5 || (L.forgeKit && tb >= 4) ? 4 : 0));
    if (g.cloak && tb >= 4 && !E.cloak && !E.cape) {
      E.cloak = g.cloak;
      const uint32_t c = L.tabardColor ? L.tabardColor : (L.topColor ? L.topColor : rgba(110, 40, 40));
      auto dk = [&](int sh) { return (uint32_t)((c >> sh & 255u) * 70u / 100u); };
      E.cloakColor = 0xFF000000u | dk(16) << 16 | dk(8) << 8 | dk(0);
      if (!E.tier[HumanLook::TIER_CLOAK]) E.tier[HumanLook::TIER_CLOAK] = (uint8_t)tb;
    }
  }
  // (forge round 2, contracts 3c.1) a culture alloy's silhouette sets the dials it owns: no shoulders for mass NONE, else the
  // tier's size lifted (T4 medium, T5 up large: culture alloys appear from T4, their whole silhouette shows); its over-layer
  // replaces the ladder's cape dial (D38): a CAPE is the noble cloak in the people's colour darkened, a SCARF the neck
  // scarf; the tier caps below still hold
  {
    const ShapeUse su = shapeUseOf(L);
    if (su.body && tb) {
      const int mass = su.sx[forge::SX_MASS], ly = su.sx[forge::SX_LAYER];
      if (mass == forge::MS_NONE) E.pauldron = 1;
      else if (mass) E.pauldron = std::max<uint8_t>(E.pauldron, (uint8_t)(tb >= 5 ? 4 : tb >= 4 ? 3 : 2));
      if (ly) {
        E.cloak = 0; E.cape = false;
        const uint32_t c = L.tabardColor ? L.tabardColor : (L.topColor ? L.topColor : rgba(110, 40, 40));
        auto dk = [&](int sh) { return (uint32_t)((c >> sh & 255u) * 70u / 100u); };
        if (ly == forge::LY_CAPE || ly == forge::LY_HALFCAPE) {
          if (ly == forge::LY_CAPE) E.cloak = 3;
          if (!L.cloakColor || L.cloak == 0) E.cloakColor = 0xFF000000u | dk(16) << 16 | dk(8) << 8 | dk(0);
          if (!E.tier[HumanLook::TIER_CLOAK]) E.tier[HumanLook::TIER_CLOAK] = (uint8_t)tb;
        }
        if (ly == forge::LY_SCARF) E.neck = 1;
      }
    }
  }
  // (forge i3, C12a: lt2 to lt3 outline +4.4 %, 6 wanted, once the integrated helm maps landed) ARMOURER'S breastplate over
  // mail on the Heartland kit wears the hauberk to the knee under it: the mail skirt shows below the plackart (a lift only)
  // and ARMOURER'S flared guards a size up (the step's shoulders, C12a)
  if (L.forgeKit && tb >= 3 && (L.forge[HumanLook::TIER_BODY] & 0xFF) == forge::B_ARMOURER && E.bodyForm == 3) {
    if (E.skirt && E.skirt < 4) E.skirt = 4;
    if (E.pauldron >= 2 && E.pauldron < 4) E.pauldron = 4;
  }
  // (envelope fix round, D57) the village watch at T3: the T3d's kit is the watch's issue at that rank, so his own kit's helm,
  // shield and shoulder steps (kitVar above) give way to it: the kettle hat at T3 (HEAD's T3 kettle views: the lit dome, the
  // brim 1 px past, the finial crest 3 rows over the crown), the cop at medium (pauldronOf), the people's cape worn (its T3
  // views show beside the arms, BODY's `when cloak.t3`), the round shield where he bears one
  if (watchT3(L)) {
    E.helmet = true;
    E.helmForm = 2;   // cult::HelmForm::Kettle + 1
    if (!E.helmStyle) E.helmStyle = 2;
    E.tier[HumanLook::TIER_HELM] = 3;
    E.pauldron = 3;
    if (shieldOf(E)) { E.shieldForm = 1;   /* cult::ShieldForm::Round + 1 */ if (E.tier[HumanLook::TIER_SHIELD] < 3) E.tier[HumanLook::TIER_SHIELD] = 3; }
    if (!E.cloak && !E.cape) E.cape = true;
    if (E.cape && !E.cloak) E.tier[HumanLook::TIER_CLOAK] = 3;
  }
  const int thE = E.tier[HumanLook::TIER_HELM];   // the helm's tier after the fighter's own kit
  if (tb == 1) { E.pauldron = std::min<uint8_t>(E.pauldron, 1); if (E.skirt) E.skirt = std::min<uint8_t>(E.skirt, 2); }
  else if (tb == 2) { E.pauldron = std::min<uint8_t>(E.pauldron, 2); if (E.skirt) E.skirt = std::min<uint8_t>(E.skirt, 3); }
  if (thE == 1) { if (E.crest) E.crest = 1; E.armsOrnament &= (uint16_t)~1u; }   // no crest, no plume (ARM_PLUMES)
  else if (thE == 2) { if (E.crest) E.crest = std::min<uint8_t>(E.crest, 2); E.armsOrnament &= (uint16_t)~1u; }
  // (ladder i3) the top never steps down: a T6 piece wears T5's dials at least (the heartland's band-7 STARMETAL is steel,
  // so gear_look gave it the people's plain dials, one under the masterwork alloy's: the blind panel saw T6 lose the T5
  // plume and the large pauldrons). A lift only where the piece is worn; rig 2 only (the resolver).
  if (tb >= 6 && E.pauldron && E.pauldron < 4) E.pauldron = 4;
  if (thE >= 6 && E.crest && E.crest < 2) E.crest = 2;
  if (thE >= 5 && E.crest >= 2) E.armsOrnament |= 1u;   // a plume on a helm made for one
  return E;
}

// the finish by each piece's own tier (contracts 3b's table); the Rare mark gives R the next tier's column
void ladderOf(const HumanLook& L, DollRecipe& r) {
  if (!L.topTier()) return;
  const pool::Pool& P = pool::get();
  uint8_t used = 0;
  for (int s = 0; s < 8; s++) {
    const int t = L.tier[s];
    r.pieceTier[s] = (uint8_t)t;
    if (!t) continue;
    const int tr = std::min(6, t + ((L.rare >> s & 1) ? 1 : 0));
    uint8_t* f = r.fin[s];
    if (tr >= 3) f[0] = (uint8_t)Slot::Metal2;                              // R: brass from T3, gold from T5 (the Metal2 ramp)
    if (t >= 3 && t <= 4) f[2] = (uint8_t)Slot::Metal2;                     // G: a metal-2 boss
    if (t >= 5) f[2] = (uint8_t)Slot::Glow;                                 //    a gem
    if (t >= 6) f[3] = (uint8_t)Slot::Glow;                                 // N: runes
    uint8_t tiles = 0;
    if (t == 1) tiles |= FIN_TILE_WEAR;
    if (t == 4) tiles |= FIN_TILE_ETCH;
    if (t >= 5) tiles |= FIN_TILE_FILIGREE | FIN_TILE_GEM;
    if (t >= 6) tiles |= FIN_TILE_RUNE | FIN_TILE_GLYPH;
    r.finTiles[s] = tiles;
    used |= tiles;
  }
  // the finish tiles the pool draws (a decoration, never a hole: a missing tile is no need)
  static const char* const kTile[6] = {"finish.wear", "finish.etch", "finish.filigree", "finish.rune", "finish.gem", "finish.glyph"};
  bool runeTile = false;
  for (int b = 0; b < 6; b++)
    if (used >> b & 1)
      if (!P.partsWithRole(Role::Overlay, kTile[b]).empty()) {
        r.needs.push_back(Need{Role::Overlay, kTile[b]});
        if (b == 3) runeTile = true;
      }
  // (LADDER-FINISH's ruling) the rune tile writes the Glow on its dashes and the gaps keep the seam as drawn: N is recoloured
  // to Glow wholesale only while no rune tile is drawn
  if (runeTile) for (auto& f : r.fin) f[3] = 0xFF;
  // (P2c forge, D28; STYLE_P2 28.7) a glowing alloy's tell: its seams (N) glow from T4, steady (the breath from T5; at T6 the
  // runes take them). Its metal pieces only (body helm shield gloves boots)
  if (L.sheen == 6)
    for (int s : {(int)HumanLook::TIER_BODY, (int)HumanLook::TIER_HELM, (int)HumanLook::TIER_SHIELD, (int)HumanLook::TIER_GLOVES, (int)HumanLook::TIER_BOOTS})
      if (L.tier[s] >= 4 && L.tier[s] <= 5) r.fin[s][3] = (uint8_t)Slot::Glow;
  // (forge round 2, D40) emberforged glows wherever it is worn: FURNACE's seams (the vents, the skirt's lame joins, the
  // grille) in Glow at every tier below T6 (T6's runes take them), on every piece made in it
  for (int s : {(int)HumanLook::TIER_BODY, (int)HumanLook::TIER_HELM, (int)HumanLook::TIER_SHIELD, (int)HumanLook::TIER_GLOVES, (int)HumanLook::TIER_BOOTS})
    if (L.forge[s] && (L.forge[s] & 0xFF) == forge::B_FURNACE && L.tier[s] >= 1 && L.tier[s] <= 5 && r.fin[s][3] == 0xFF) r.fin[s][3] = (uint8_t)Slot::Glow;
  static const char* const kMotif[4] = {"oak", "clouds", "laurel", "knot"};
  r.motif = kMotif[L.motif < 4 ? L.motif : 0];
  auto flagS = [&](const std::string& f) { r.flags.push_back(f); };
  static const char* const kSlotFlag[8] = {"armour", "helm", "shield", "held", "gloves", "boots", "cloak", "back"};
  for (int s = 0; s < 8; s++) {
    for (int t = 1; t <= L.tier[s] && t <= 6; t++) flagS(std::string(kSlotFlag[s]) + ".t" + std::to_string(t));
    if (L.tier[s] && (L.rare >> s & 1)) flagS(std::string(kSlotFlag[s]) + ".r");
  }
  for (int t = 1; t <= L.topTier() && t <= 6; t++) flagS("tier" + std::to_string(t));
  if (L.tier[HumanLook::TIER_CLOAK] >= 5 || (L.tier[HumanLook::TIER_HELM] >= 5 && (L.crest >= 2 || (L.armsOrnament & 1u)))) flagS("breeze");
  static const char* const kMetal[12] = {"", "leather", "bronze", "iron", "steel", "gilded", "jade", "obsidian", "ember", "starmetal", "alloy", "mythic"};
  if (L.metalKind >= 1 && L.metalKind <= 11) flagS(std::string("metal-") + kMetal[L.metalKind]);
  if (L.mythic && !r.has("mythic")) flagS("mythic");
}

}  // namespace

DollRecipe resolve(const HumanLook& L0) {
  // (P2c ladder) the effective dials by tier (the look as given at no tier)
  const HumanLook L = ladderLook(L0);
  DollRecipe r;
  struct EnvGuard { explicit EnvGuard(std::vector<std::string>* v) { g_envMissing = v; } ~EnvGuard() { g_envMissing = nullptr; } } envGuard(&r.envelopeMissing);
  r.body = bodySetOf(L);
  r.bodyKey = bodySetName(r.body);
  if (L.age == 3 && (r.body == BodySet::Man || r.body == BodySet::Woman)) r.bodyKey += ".elder";
  r.elf = L.people == 2;
  int build = std::clamp((int)L.build, 0, 2);
  if (L.people == 2) build = build == 2 ? 0 : 1;           // elves one build slimmer
  else if (L.people == 1 && build == 0) build = 1;          // half-breeds slimmer at average
  r.build = build;
  r.family = (uint8_t)familyOf(L);
  // (P2c forge) the styles unpacked; the body's cut on the family its armour draws (a hero style's own column, a generated
  // alloy's cut by its character: doll_forge.h cutFor)
  {
    const char* a = armourOf(L);
    r.armourFam = a ? forge::familyOfArmour(a) : -1;
    for (int s = 0; s < 8 && s < HumanLook::TIER_SLOTS; s++) forge::unpack(L.forge[s], r.forgeAx[s]);
    uint8_t* b = r.forgeAx[HumanLook::TIER_BODY];
    if (L.forge[HumanLook::TIER_BODY] && r.armourFam >= 0) b[forge::AX_CUT] = (uint8_t)forge::cutFor(b, r.armourFam);
  }
  r.sheen = L.sheen;
  needsOf(L, r);
  regionsOf(L, garmentOf(L), r);
  flagsOf(L, r);
  ladderOf(L, r);
  r.ramps = rampsOf(L);
  // (P2c ladder) the held part's metal draws in Slot::Blade where its ramp differs from the armour's
  for (int i = 0; i < 5; i++) if (r.ramps.c[(int)Slot::Blade][i] != r.ramps.c[(int)Slot::Metal][i]) r.blade = true;
  const pool::Pool& P = pool::get();
  // (KIN-YOUTH) the youth wears the adult head box (STYLE_P2 14): heads, faces, ears, hair and headwear drawn for the
  // adult of its own sex (a part for both adults serves both); every other part serves it by `body youth`
  auto headBox = [](Role ro) {
    return ro == Role::Head || ro == Role::Nose || ro == Role::Brow || ro == Role::Ears || ro == Role::Expr || ro == Role::Paint ||
           ro == Role::Earring || ro == Role::Hair || ro == Role::Headwear;
  };
  // (polish i1 integration, P7) an armour material the body has no drawing for wears the nearest drawn one, as drawnForm
  // does for helms and shields: the census's women of the scale and lamellar peoples (Imperial, Steppe, Jade) fell back
  // whole to the v1 sprite at rig 2, half a head short among the dolls. Chains: scale, lamellar -> mail; brigandine -> plate;
  // leaf -> leather; mail -> leather -> padded. The material named stays the look's (its regions and colours); only the
  // part it draws changes.
  for (Need& n : r.needs) {
    if (n.role != Role::Armour || (r.body != BodySet::Man && r.body != BodySet::Woman)) continue;
    auto served = [&](const std::string& v) {
      for (int i : P.partsWithRole(Role::Armour, v)) if (P.parts[(size_t)i].bodyMask >> (int)r.body & 1) return true;
      return false;
    };
    std::string v = n.value;
    for (int guard = 0; guard < 4 && !served(v); guard++) {
      const char* next = v == "scale" || v == "lamellar" ? "mail" : v == "brigandine" ? "plate" : v == "leaf" ? "leather" :
                         v == "mail" || v == "plate" ? "leather" : v == "leather" ? "padded" : nullptr;
      if (!next) break;
      v = next;
    }
    if (served(v)) n.value = v;   // (a chain that finds nothing drawn leaves the need as it was: still reported missing)
  }
  for (const Need& n : r.needs)
    for (int i : P.partsWithRole(n.role, n.value)) {
      const uint8_t m = P.parts[(size_t)i].bodyMask;
      bool ok = (m >> (int)r.body & 1) != 0;
      if (r.body == BodySet::Youth && headBox(n.role)) {
        const bool man = m >> (int)BodySet::Man & 1, woman = m >> (int)BodySet::Woman & 1;
        if (man || woman) ok = L.body == 1 ? woman : man;
      }
      if (ok) r.parts.push_back(i);
    }
  std::sort(r.parts.begin(), r.parts.end());
  r.parts.erase(std::unique(r.parts.begin(), r.parts.end()), r.parts.end());
  // (forge round 2, contracts 3c.1) a styled skirt replaces the body armour's own hang (its hangFront and hangBack parts: the
  // hang, the mail stride); the T5 surcoat views of the hang go with it (D38)
  if (r.dropHang)
    r.parts.erase(std::remove_if(r.parts.begin(), r.parts.end(), [&](int pi) {
                    const pool::PartRec& p = P.parts[(size_t)pi];
                    const bool arm = std::any_of(p.roles.begin(), p.roles.end(), [](const pool::RoleRec& rr) { return rr.cat == (uint8_t)Role::Armour; });
                    // (a hang that is itself the styled skirt, `role overlay skirt.tassets` on plate_hang, stays)
                    const bool skirt = std::any_of(p.roles.begin(), p.roles.end(), [&](const pool::RoleRec& rr) {
                      return rr.cat == (uint8_t)Role::Overlay && P.s(rr.value).rfind("skirt.", 0) == 0 &&
                             std::any_of(r.needs.begin(), r.needs.end(), [&](const Need& n) { return n.role == Role::Overlay && n.value == P.s(rr.value); });
                    });
                    return arm && !skirt && (p.layer == (uint8_t)Layer::HangFront || p.layer == (uint8_t)Layer::HangBack);
                  }),
                  r.parts.end());
  {
    // D41: shoulders stay on top of a cloak's front and the T5 surcoat
    bool cloak = false;
    for (const Need& n : r.needs) if (n.role == Role::Cloak) cloak = true;
    r.pauldronTop = cloak || L.tier[HumanLook::TIER_BODY] >= 5;
    const ShapeUse su = shapeUseOf(L);
    r.irisBands = su.body && L.sheen == 5;
    // (P2c envelope r2, FORGE-MATERIAL prototype; PLAN A6) STARFALL's starmetal is iridescent crystal: its armour, shoulders and
    // helm alternate the lavender star-white (Metal) and a turned sea-glass cyan (Metal 2) in 2-row bands, the lit planes
    // 70+ degrees apart, so the crystal shifts colour course by course at 1x (C17's iridescent term)
    if (L.metalKind == 9 && armourOf(L) && L.tier[HumanLook::TIER_BODY] >= 4) {   // (envelope r3, A6: T5-T6 starmetal too)

      r.irisBands = true;
    }
    // D39: the shield's field mixed 30 % toward the body metal's step 2 (its rim and boss already in the metal)
    if (r.has("alloyface"))
      for (int i = 1; i < 5; i++) {
        const uint32_t a = r.ramps.c[(int)Slot::Field][i], m = r.ramps.c[(int)Slot::Metal][2];
        auto ch = [&](int sh) { const int x = (int)(a >> sh & 255u), y = (int)(m >> sh & 255u); return (uint32_t)(x + (y - x) * 30 / 100); };
        r.ramps.c[(int)Slot::Field][i] = 0xFF000000u | ch(16) << 16 | ch(8) << 8 | ch(0);
      }
  }
  // (P2c forge i2; the blind panel: "the old red plume on the Heartland T4 helm covers every new crest") a style's crest takes
  // the helm's own crest or plume part (crested_crest, greathelm_crest, greathelm_plume) up to T4; T5-T6 keep the
  // ladder's crest (the T5 crest, the T6 winged crown, the flame crown are drawn there)
  {
    bool forgeCrest = false;
    for (const Need& n : r.needs) if (n.role == Role::Overlay && n.value.rfind("crest.", 0) == 0) forgeCrest = true;
    // (another people's leather, bronze, iron and steel keep their own helm, its plume with it: C12e, the peoples apart)
    const bool kitHelm = L.forgeKit || (L.forge[HumanLook::TIER_HELM] && (L.forge[HumanLook::TIER_HELM] & 0xFF) == 0) ||
                         ((L.forge[HumanLook::TIER_HELM] & 0xFF) > forge::B_ARMOURER);
    auto ownCrest = [&](int pi) {
      const pool::PartRec& p = P.parts[(size_t)pi];
      const std::string& nm = P.s(p.name);
      const bool helm = std::any_of(p.roles.begin(), p.roles.end(), [](const pool::RoleRec& rr) { return rr.cat == (uint8_t)Role::Helm; });
      auto ends = [&](const char* suf) { const size_t n = std::strlen(suf); return nm.size() >= n && nm.compare(nm.size() - n, n, suf) == 0; };
      return helm && (ends("_crest") || ends("_plume"));
    };
    if (forgeCrest && kitHelm && L.tier[HumanLook::TIER_HELM] <= 4) {
      r.parts.erase(std::remove_if(r.parts.begin(), r.parts.end(), ownCrest), r.parts.end());
    } else if (forgeCrest && !kitHelm && std::any_of(r.parts.begin(), r.parts.end(), ownCrest)) {
      // (the plume stays and the style's crest goes: one crest on a helm, the people's)
      auto styleCrest = [&](int pi) {
        for (const pool::RoleRec& rr : P.parts[(size_t)pi].roles)
          if (rr.cat == (uint8_t)Role::Overlay && P.s(rr.value).rfind("crest.", 0) == 0) return true;
        return false;
      };
      r.parts.erase(std::remove_if(r.parts.begin(), r.parts.end(), styleCrest), r.parts.end());
      r.needs.erase(std::remove_if(r.needs.begin(), r.needs.end(), [](const Need& n) { return n.role == Role::Overlay && n.value.rfind("crest.", 0) == 0; }),
                    r.needs.end());
      // (forge i3 integration) and its crest-<v> flag: the own crest's and plume's maps yield to a forge crest by that
      // flag (FORGE-HELM's plume_yield views), so with the style's crest gone the flag would blank the people's plume too
      // (imp3: the Imperial's horsehair crest vanished, no crest at all; C12e)
      r.flags.erase(std::remove_if(r.flags.begin(), r.flags.end(), [](const std::string& f) { return f.rfind("crest-", 0) == 0; }), r.flags.end());
    }
  }
  return r;
}

// ---------------------------------------------------------------- (POSES-B, P2b) postures: the prop in hand, the clips
const char* postureProp(Posture p, uint8_t v) {
  switch (p) {
    case Posture::SitEat: case Posture::SitFloorEat: return "spoon";
    case Posture::Eat: return "bread";
    case Posture::SitDrink: case Posture::SitFloorDrink: case Posture::Drink: case Posture::Cheer: return "mug";
    case Posture::Hammer: return "hammer";
    case Posture::Hoe: return "hoe";
    case Posture::Sweep: return "broom";
    case Posture::Chop: return "woodaxe";
    case Posture::Stir: return "ladle";
    case Posture::Carry: return v == 1 ? "basket" : v == 2 ? "crate" : "sack";
    case Posture::Fish: return "rod";
    case Posture::Beg: return "bowl";
    case Posture::Lamp: return "lamppole";
    case Posture::Read: return "book";
    case Posture::Kneel: return v == 1 ? nullptr : "linen";
    case Posture::Lute:   // (uint8_t)LeadInst
      return v == (uint8_t)LeadInst::Oud ? "oud" : v == (uint8_t)LeadInst::Fiddle ? "fiddle" : v == (uint8_t)LeadInst::Harp ? "harp" : "lute";
    case Posture::Flute:
      switch ((LeadInst)v) {
        case LeadInst::Pipes: return "pipes";
        case LeadInst::Reed: return "reed";
        case LeadInst::Horn: case LeadInst::Brass: return "horn";
        case LeadInst::Bells: return "handbells";
        case LeadInst::Voice: return nullptr;
        default: return "flute";
      }
    case Posture::Drum:   // (uint8_t)PercKind
      switch ((PercKind)v) {
        case PercKind::Bodhran: return "bodhran";
        case PercKind::Taiko: case PercKind::Kettle: return "taiko";
        case PercKind::Hand: case PercKind::Tabla: return "handdrums";
        case PercKind::Gong: return "gong";
        case PercKind::Wood: return "clappers";
        case PercKind::Bells: return "jingle";
        default: return "framedrum";
      }
    default: return nullptr;
  }
}

DollRecipe withPosture(const DollRecipe& r, Posture p, uint8_t variant) {
  DollRecipe o = r;
  // (PROPS) the flag `posture`: a carried stick, staff, ledger or scroll draws its set-down view (`when posture replace`:
  // leaning on the body, or none) while the hands are busy, never floating at a pose's fist; and `prop-<value>` for the
  // prop in hand (the smith's belt hammer and the herald's slung horn leave the hip while the same thing is in hand)
  o.flags.push_back("posture");
  const char* prop = postureProp(p, variant);
  if (!prop) return o;
  o.flags.push_back(std::string("prop-") + prop);
  o.needs.push_back(Need{Role::Held, prop});
  for (int i : pool::get().partsWithRole(Role::Held, prop)) {
    const uint8_t m = pool::get().parts[(size_t)i].bodyMask;
    // (KIN-YOUTH) a prop is an object, not a fitted piece: the youth holds the adult's at its own grip mark
    const bool youthHolds = o.body == BodySet::Youth && (m & ((1u << (int)BodySet::Man) | (1u << (int)BodySet::Woman)));
    if ((m >> (int)o.body & 1) || youthHolds) o.parts.push_back(i);
  }
  std::sort(o.parts.begin(), o.parts.end());
  o.parts.erase(std::unique(o.parts.begin(), o.parts.end()), o.parts.end());
  return o;
}

namespace pool {
std::vector<const ClipRec*> postureClips(const std::string& bodyKey, int family, int posture, uint8_t variant) {
  const std::vector<const ClipRec*> all = clipsOf(bodyKey, kPartPosture, family, posture);
  bool own[4] = {};
  for (const ClipRec* c : all) if (c->variant == variant) own[c->facing & 3] = true;
  std::vector<const ClipRec*> out;
  for (const ClipRec* c : all)
    if (c->variant == variant || (c->variant == 0 && !own[c->facing & 3])) out.push_back(c);
  return out;
}
}  // namespace pool

// ---------------------------------------------------------------- coverage
namespace {

const Need* needOfPart(const DollRecipe& r, int part) {
  for (const Need& n : r.needs) if (partHasRole(part, n.role, n.value)) return &n;
  return nullptr;
}

void missingIn(const DollRecipe& r, uint8_t poolPart, int posture, std::vector<Missing>& out, int variant = -1) {
  using namespace pool;
  const Pool& P = get();
  // values no part serves at all
  for (const Need& n : r.needs) {
    bool any = false;
    for (int pi : r.parts) if (partHasRole(pi, n.role, n.value)) { any = true; break; }
    if (!any) {
      Missing m; m.role = n.role; m.value = n.value; m.body = r.body;
      bool dup = false;
      for (const Missing& o : out) if (o.part.empty() && o.role == m.role && o.value == m.value) dup = true;
      if (!dup) out.push_back(m);
    }
  }
  const std::vector<const ClipRec*> cs = poolPart == kPartPosture && variant >= 0 ? postureClips(r.bodyKey, r.family, posture, (uint8_t)variant)
                                                                                 : clipsOf(r.bodyKey, poolPart, r.family, posture, &r);
  if (cs.empty()) {
    Missing m; m.role = Role::Body; m.value = poolPart == kPartPosture ? "posture clips" : poolPart == 0 ? "loco clips" : poolPart == 1 ? "action clips" :
                                                                                          poolPart == 2 ? "death clips" : poolPart == kPartDrawn ? "drawn clips" : "run clips";
    m.body = r.body;
    out.push_back(m);
    return;
  }
  const uint16_t restId = P.ids.rest, dashId = P.ids.dash;
  std::set<std::string> seen;
  auto report = [&](int pi, int facing, uint16_t key) {
    const Need* n = needOfPart(r, pi);
    Missing m;
    if (n) { m.role = n->role; m.value = n->value; }
    m.part = P.s(P.parts[(size_t)pi].name); m.body = r.body; m.facing = facing; m.key = P.s(key);
    const std::string id = m.what();
    if (seen.insert(id).second) out.push_back(m);
  };
  const uint16_t tileId = P.ids.tile;
  for (const ClipRec* c : cs) {
    for (uint16_t fi = 0; fi < c->nFrames; fi++) {
      const FrameRec& f = P.frames[c->firstFrame + fi];
      // a turned frame (lying, a topple) is composed in the left view
      const int facing = (f.turn || c->clip == (uint8_t)Clip::Lying) ? 3 : c->facing;
      // rigid parts: a view in this facing; channel parts: the key the frame names, by at least one part on the channel
      std::vector<uint16_t> chans;
      for (int pi : r.parts) {
        const PartRec& p = P.parts[(size_t)pi];
        if (tileId != 0xFFFF && std::find(p.tags.begin(), p.tags.end(), tileId) != p.tags.end()) {   // a tile: any view (D serves all)
          // (P2c ladder) a fit or stamp tile (LADDER-FINISH) is placed by its keyed views: any view serves, `rest` is optional
          if (partTagged(pi, "fit") || partTagged(pi, "stamp")) { if (!p.nViews) report(pi, facing, restId); continue; }
          if (!viewOf(p, facing, restId) && !viewOf(p, 0, restId)) report(pi, facing, restId);
          continue;
        }
        if (p.channel == restId) { if (!viewOf(p, facing, restId)) report(pi, facing, restId); continue; }
        if (std::find(chans.begin(), chans.end(), p.channel) == chans.end()) chans.push_back(p.channel);
      }
      for (uint16_t ch : chans) {
        uint16_t key = 0xFFFF;
        for (const KeyRec& k : f.keys) if (k.channel == ch) key = k.key;
        if (key == dashId) continue;
        bool named = key != 0xFFFF;
        if (!named) key = restId;
        // (P2c, INTEGRATE i1) the held channel's angle view (`weapon=<deg>`) is a named key too, as the compositor picks it
        if (!named && ch == P.ids.held && P.ids.held != 0xFFFF && f.weapon != -32768) { key = P.ids.angle[angleIndex(f.weapon)]; named = key != 0xFFFF; }
        if (key == 0xFFFF) continue;
        bool any = false;
        int first = -1;
        for (int pi : r.parts) {
          const PartRec& p = P.parts[(size_t)pi];
          if (p.channel != ch) continue;
          if (first < 0) first = pi;
          if (viewOf(p, facing, key)) { any = true; break; }
          // (P2c, INTEGRATE i1) `tags anyfacing`: D's view serves every facing, as in the compositor
          if (P.ids.anyfacing != 0xFFFF && std::find(p.tags.begin(), p.tags.end(), P.ids.anyfacing) != p.tags.end() && viewOf(p, 0, key)) { any = true; break; }
        }
        if (!any && named && first >= 0) report(first, facing, key);
      }
    }
  }
}

struct CoverCache {
  uint32_t gen = 0;
  std::unordered_map<uint64_t, bool> map;
};
CoverCache& cache() { static CoverCache c; return c; }

std::set<int>& reached() { static std::set<int> s; return s; }

}  // namespace

std::vector<Missing> dollMissing(const HumanLook& look, RigPart part) {
  const DollRecipe r = resolve(look);
  std::vector<Missing> out;
  missingIn(r, pool::poolPart(part), -1, out);
  return out;
}
std::vector<Missing> dollMissing(const HumanLook& look) {
  const DollRecipe r = resolve(look);
  std::vector<Missing> out;
  missingIn(r, (uint8_t)RigPart::Loco, -1, out);
  missingIn(r, (uint8_t)RigPart::Death, -1, out);
  const bool runs = r.body != BodySet::Child && look.age != 3;   // elders never run; kids run with the Play posture
  if (runs) missingIn(r, (uint8_t)RigPart::Run, -1, out);
  if (look.weapon >= 1 && look.weapon <= 8 && !look.civil) {   // (a civil tool rests)
    missingIn(r, (uint8_t)RigPart::Action, -1, out);
    missingIn(r, pool::kPartDrawn, -1, out);   // (P2c) the drawn idle, walk and run
  }
  return out;
}
bool dollCovers(const HumanLook& look, RigPart part) {
  CoverCache& c = cache();
  if (c.gen != pool::generation()) { c.map.clear(); c.gen = pool::generation(); }
  if (pool::get().parts.empty()) return false;   // (no maps yet: nothing is covered; the census's fast path)
  // (the doll's own fields mixed in whatever the rig: key() holds them only at rig 2)
  const uint64_t f = (uint64_t)look.head | (uint64_t)look.nose << 8 | (uint64_t)look.brow << 16 | (uint64_t)look.marks << 24 |
                     (uint64_t)look.hairVar << 32 | (uint64_t)look.beardStyle << 40 | (uint64_t)look.neck << 48 | (uint64_t)look.garb << 56;
  uint64_t t8 = 0;   // (P2c ladder) the tiers and metals (key() holds them only at rig 2)
  for (int i = 0; i < HumanLook::TIER_SLOTS; i++) t8 |= (uint64_t)look.tier[i] << (8 * i);
  const uint64_t lad = (uint64_t)look.rare | (uint64_t)look.metalKind << 8 | (uint64_t)look.bladeKind << 16 | (uint64_t)look.mythic << 24 |
                       (uint64_t)look.motif << 32;
  uint64_t fg = 0;   // (P2c forge) the styles
  for (uint64_t x : look.forge) fg = (fg ^ x) * 0x100000001B3ull;
  const uint64_t k = (look.key() * 0x9E3779B97F4A7C15ull ^ f * 0xBF58476D1CE4E5B9ull ^ (uint64_t)look.age << 3 ^ (uint64_t)look.civil << 11 ^
                      t8 * 0x94D049BB133111EBull ^ lad * 0x2545F4914F6CDD1Dull ^ fg * 0xD6E8FEB86659FD93ull) + ((uint64_t)part + 1) * 0xC2B2AE3D27D4EB4Full;
  auto it = c.map.find(k);
  if (it != c.map.end()) return it->second;
  if (c.map.size() > 50000) c.map.clear();
  std::vector<Missing> m;
  missingIn(resolve(look), pool::poolPart(part), -1, m);
  return c.map[k] = m.empty();
}
bool dollCoversPosture(const HumanLook& look, Posture p) { return dollCoversPosture(look, p, 0); }
// (POSES-B) a posture's variant: its own clips (else variant 0's per facing) with its prop in hand
bool dollCoversPosture(const HumanLook& look, Posture p, uint8_t variant) {
  if (pool::get().parts.empty()) return false;
  std::vector<Missing> m;
  missingIn(withPosture(resolve(look), p, variant), pool::kPartPosture, (int)p, m, variant);
  return m.empty();
}
std::vector<Missing> dollMissingPosture(const HumanLook& look, Posture p, uint8_t variant) {
  std::vector<Missing> m;
  missingIn(withPosture(resolve(look), p, variant), pool::kPartPosture, (int)p, m, variant);
  return m;
}

int partTierSlot(int part) {
  static uint32_t gen = 0xFFFFFFFFu;
  static std::vector<int8_t> memo;
  const pool::Pool& P = pool::get();
  if (gen != pool::generation() || memo.size() != P.parts.size()) {
    gen = pool::generation();
    memo.assign(P.parts.size(), -1);
    for (size_t i = 0; i < P.parts.size(); i++) {
      int8_t s = -1;
      for (const pool::RoleRec& rr : P.parts[i].roles) {
        const std::string& v = P.s(rr.value);
        switch ((Role)rr.cat) {
          case Role::Armour: case Role::Pauldron: s = HumanLook::TIER_BODY; break;
          // (INTEGRATE silhouettes i1) the body piece's round-2 parts (motif, skirt, sleeve, sash) take its tier: their R P G
          // N regions finish like the armour's (a glowing alloy's motif and skirt seams light: contracts 3c.1 glow seams)
          case Role::Overlay:
            if (v == "tabard" || v == "sash" || v.rfind("motif.", 0) == 0 || v.rfind("skirt.", 0) == 0 || v.rfind("sleeve.", 0) == 0)
              s = HumanLook::TIER_BODY;
            break;
          case Role::Helm: s = HumanLook::TIER_HELM; break;
          case Role::Shield: s = HumanLook::TIER_SHIELD; break;
          case Role::Held: s = HumanLook::TIER_WEAPON; break;
          case Role::Sheath: s = (v == "bowback" || v == "staffback" || v == "quiver") ? HumanLook::TIER_BACK : HumanLook::TIER_WEAPON; break;
          case Role::Cloak: s = HumanLook::TIER_CLOAK; break;
          case Role::Arms: s = HumanLook::TIER_GLOVES; break;
          case Role::Legs: s = HumanLook::TIER_BOOTS; break;
          default: break;
        }
        if (s >= 0) break;
      }
      memo[i] = s;
    }
  }
  return part >= 0 && (size_t)part < memo.size() ? memo[(size_t)part] : -1;
}

void markReached(const DollRecipe& r) { for (int p : r.parts) reached().insert(p); }
void clearReached() { reached().clear(); }
std::vector<std::string> deadParts() {
  std::vector<std::string> out;
  // (P2c ladder) the living finish's sprites (`fx glint`, `fx mote`) are drawn by the view, never by a recipe
  for (int i = 0; i < partCount(); i++)
    if (!reached().count(i) && !partHasRole(i, Role::Fx, "glint") && !partHasRole(i, Role::Fx, "mote")) out.push_back(partName(i));
  return out;
}

}  // namespace doll
}  // namespace art

