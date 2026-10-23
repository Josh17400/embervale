// EMBERVALE art: the doll's resolver and coverage (DOLL; P2b phase A first version by the lead). A look becomes a
// recipe: its body set, every field value it asks for (needs, in the role vocabulary of doll.h), the pool's parts that
// serve them (every part with that role, for that body set), its ramps, the slot of each region and its flags. Coverage
// walks the recipe through the body set's clips and names every value no authored part covers (VISION_PLAN 15.36
// "Coverage"). Nothing here draws.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <unordered_map>
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
std::string pauldronOf(const HumanLook& L) {
  if (L.pauldron < 2) return "";   // 0 the band's own (none drawn yet), 1 none
  const char* a = armourOf(L);
  if (!a) return "";
  const std::string m = a;
  const char* fam = (m == "plate" || m == "brigandine" || m == "leaf") ? "plate" : (m == "padded" || m == "leather") ? "fur" : "scale";
  return std::string(fam) + "." + std::to_string(std::min<int>(L.pauldron, 4));
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

void needsOf(const HumanLook& L, DollRecipe& r) {
  auto need = [&](Role role, const std::string& v) { if (!v.empty()) r.needs.push_back(Need{role, v}); };
  const bool kid = r.body == BodySet::Child || r.body == BodySet::Youth;
  const std::string g = garmentOf(L);
  need(Role::Legs, longGarment(g) ? "tips" : "std");
  need(Role::Arms, "std");
  need(Role::Garment, g);
  if (const char* a = armourOf(L)) need(Role::Armour, a);
  // (P2c) the helm: form1..form10 (HelmForm + 1), else the band (band1..band7; a legacy helmet's band by its outfit)
  if (const int hb = helmBandOf(L)) need(Role::Helm, L.helmForm ? drawnForm(Role::Helm, L.helmForm) : "band" + std::to_string(hb));
  else if (L.helmForm) need(Role::Helm, drawnForm(Role::Helm, L.helmForm));
  // (P2c) the shield: the shield arm for every bearer, plus its form (form1..8) or shape (shape0 legacy round, 1..4);
  // form9 is ShieldForm::None: no shield at all (v1's hasShield rule)
  if (shieldOf(L)) {
    need(Role::Shield, "arm");
    need(Role::Shield, L.shieldForm ? drawnForm(Role::Shield, L.shieldForm) : "shape" + std::to_string(L.shieldStyle));
  }
  // (P2c restated) pauldrons: <family>.<size> (plate: plate brigandine leaf; fur: padded leather; scale: mail scale
  // lamellar), size 2 small, 3 medium, 4 large from the dial; dial 1 is none
  {
    const std::string pd = pauldronOf(L);
    if (!pd.empty()) need(Role::Pauldron, pd);
  }
  // (P2c) the tabard over the armour of every Outfit::Guard look (watch, soldiers, heralds): its colour is the accent
  if (L.outfit == Outfit::Guard) need(Role::Overlay, "tabard");
  // (P2c) the armour's material tile over its sleeves (D3), where the pool draws one (a decoration, never a hole)
  if (const char* t = sleeveTileOf(L))
    if (!pool::get().partsWithRole(Role::Overlay, t).empty()) need(Role::Overlay, t);
  if (L.neck) need(Role::Neck, neckName(L.neck));
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
  if (L.gloves) {
    set(Region::Hand, L.gloves >= 2 ? Slot::Metal : Slot::Leather);
    if (L.gloves >= 3) set(Region::Flare, Slot::Metal);
  }
  if (L.boots) {
    set(Region::Boot, L.boots >= 3 ? Slot::Metal : Slot::Leather);
    if (L.boots == 2) set(Region::Shin, Slot::Leather);
    if (L.boots >= 3) { set(Region::Shin, Slot::Metal); set(Region::Knee, Slot::Metal); }
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
  static const char* const kGlint[5] = {"glint-weapon", "glint-body", "glint-helm", "glint-shield", "glint-cloak"};
  for (int b = 0; b < 5; b++) if (L.glow >> b & 1) flag(kGlint[b]);
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
  const int tb = L.tier[HumanLook::TIER_BODY], th = L.tier[HumanLook::TIER_HELM];
  if (tb && !L.pauldron && !L.skirt && !L.crest) {
    static const uint8_t kPd[7] = {0, 1, 2, 3, 3, 4, 4}, kSk[7] = {0, 2, 2, 3, 3, 4, 4};
    E.pauldron = kPd[tb];
    E.skirt = kSk[tb];
    if (!L.bodyForm && L.metalKind && (L.armorStyle || L.outfit == Outfit::Leather || L.outfit == Outfit::Chain || L.outfit == Outfit::Plate)) {
      const int m = L.metalKind;
      E.bodyForm = (uint8_t)(m == 1 ? 2 : (m == 2 || m == 3) ? 3 : 7);   // BodyArm + 1: leather 2, mail 3, plate 7
    }
  }
  if (tb == 1) { E.pauldron = std::min<uint8_t>(E.pauldron, 1); if (E.skirt) E.skirt = std::min<uint8_t>(E.skirt, 2); }
  else if (tb == 2) { E.pauldron = std::min<uint8_t>(E.pauldron, 2); if (E.skirt) E.skirt = std::min<uint8_t>(E.skirt, 3); }
  if (th == 1) { if (E.crest) E.crest = 1; E.armsOrnament &= (uint16_t)~1u; }   // no crest, no plume (ARM_PLUMES)
  else if (th == 2) { if (E.crest) E.crest = std::min<uint8_t>(E.crest, 2); E.armsOrnament &= (uint16_t)~1u; }
  // (ladder i3) the top never steps down: a T6 piece wears T5's dials at least (the heartland's band-7 STARMETAL is steel,
  // so gear_look gave it the people's plain dials, one under the masterwork alloy's: the blind panel saw T6 lose the T5
  // plume and the large pauldrons). A lift only where the piece is worn; rig 2 only (the resolver).
  if (tb >= 6 && E.pauldron && E.pauldron < 4) E.pauldron = 4;
  if (th >= 6 && E.crest && E.crest < 2) E.crest = 2;
  if (th >= 5 && E.crest >= 2) E.armsOrnament |= 1u;   // a plume on a helm made for one
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
  r.body = bodySetOf(L);
  r.bodyKey = bodySetName(r.body);
  if (L.age == 3 && (r.body == BodySet::Man || r.body == BodySet::Woman)) r.bodyKey += ".elder";
  r.elf = L.people == 2;
  int build = std::clamp((int)L.build, 0, 2);
  if (L.people == 2) build = build == 2 ? 0 : 1;           // elves one build slimmer
  else if (L.people == 1 && build == 0) build = 1;          // half-breeds slimmer at average
  r.build = build;
  r.family = (uint8_t)familyOf(L);
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
  const uint64_t k = (look.key() * 0x9E3779B97F4A7C15ull ^ f * 0xBF58476D1CE4E5B9ull ^ (uint64_t)look.age << 3 ^ (uint64_t)look.civil << 11 ^
                      t8 * 0x94D049BB133111EBull ^ lad * 0x2545F4914F6CDD1Dull) + ((uint64_t)part + 1) * 0xC2B2AE3D27D4EB4Full;
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
          case Role::Overlay: if (v == "tabard") s = HumanLook::TIER_BODY; break;
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

