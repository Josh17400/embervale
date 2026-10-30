// P2c the gear ladder (rpg/sim/gear_ladder.h; docs/P2C_CONTRACTS.md 3b). LADDER-DOLL.
#include "rpg/sim/gear_ladder.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <vector>
#include "rpg/art/doll/doll.h"
#include "rpg/art/doll/doll_envelope.h"
#include "rpg/art/doll/doll_forge.h"
#include "rpg/sim/gear_forge.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/gear_look.h"
#include "rpg/sim/world.h"

namespace census {
struct Rank { bool royal = false, capital = false; };
void dress(art::HumanLook& L, std::string& name, Role r, bool female, const cult::Culture& C, const cult::Culture& owner, uint64_t seed,
           const Rank& rank);
}  // namespace census

namespace gear {

namespace {
bool wornKind(ItemKind k) {
  return k == ItemKind::Weapon || k == ItemKind::Bow || k == ItemKind::Staff || k == ItemKind::Armor || k == ItemKind::Helmet ||
         k == ItemKind::Shield || k == ItemKind::Gloves || k == ItemKind::Boots || k == ItemKind::Cloak;
}
const cult::Alloy* alloyOfItem(const Item& it, const cult::Culture* c) {
  if (!c || it.mat != Mat::Alloy || it.alloy == 0 || it.alloy > c->arms.alloys.size()) return nullptr;
  return &c->arms.alloys[(size_t)it.alloy - 1];
}
}  // namespace

LookTier lookTier(const Item& it, const cult::Culture* maker) {
  LookTier t;
  if (!wornKind(it.kind)) return t;
  const int band = std::clamp(bandOf(std::max(1, (int)it.ilvl)), 1, 7);
  static const uint8_t kBase[8] = {0, 1, 2, 3, 4, 4, 5, 5};
  switch (it.rarity) {
    case Rarity::Common: case Rarity::Uncommon: t.tier = kBase[band]; break;
    case Rarity::Rare:
      if (band == 1) t.tier = 2;   // a band-1 Rare escapes the scrap look outright
      else { t.tier = kBase[band]; t.rare = true; }
      break;
    case Rarity::Epic: t.tier = band == 1 ? 2 : (uint8_t)std::min(5, kBase[band] + 1); break;
    case Rarity::Legendary: t.tier = 6; break;
    case Rarity::Mythic: t.tier = 6; t.flame = true; break;
    default: t.tier = kBase[band]; break;
  }
  // (wave-2 fix round, D86) a culture alloy draws T4 or more (crafted or found: its people's silhouette shows from T4), and a
  // masterwork (tierStep 3) T5 only from band 4 (a low-band masterwork is T4; before, every masterwork was T5)
  if (const cult::Alloy* a = alloyOfItem(it, maker)) {
    if (a->tierStep >= 3 && band >= 4 && t.tier < 5) { t.tier = 5; t.rare = false; }
    else if (t.tier < 4 && art::doll::fixOn(art::doll::FIX_LINES)) { t.tier = 4; t.rare = false; }
  }
  return t;
}

uint8_t metalKindOf(const Item& it) {
  const bool metalPiece = it.mat == Mat::Bronze || it.mat == Mat::Iron || it.mat == Mat::Steel || it.mat == Mat::Alloy;
  // (wave-2 fix round, D84; D32 restated) MYTHIC is the style of Mythic bronze, iron, steel and the old makers' metals; a Mythic
  // culture-alloy piece keeps its alloy (its helm family, outline and colours) and wears the ember on top (HumanLook::mythic)
  if (it.rarity == Rarity::Mythic && metalPiece && (it.mat != Mat::Alloy || !art::doll::fixOn(art::doll::FIX_RELIC))) return 11;
  switch (it.mat) {
    case Mat::Leather: return 1;
    case Mat::Bronze: return 2;
    case Mat::Iron: return 3;
    case Mat::Steel: {
      const int band = bandOf(std::max(1, (int)it.ilvl));
      static const uint8_t kHigh[4] = {5, 7, 8, 9};   // gilded, obsidian, emberforged, starmetal (bands 4-7)
      return band <= 3 ? 4 : kHigh[std::min(band, 7) - 4];
    }
    case Mat::Alloy: return 10;
    default: break;
  }
  if (it.name.rfind("LEATHER", 0) == 0) return 1;
  return 0;
}

AlloyKeys alloyKeys(const cult::Culture& c, int alloyIndex) {
  AlloyKeys k;
  if (alloyIndex < 0 || alloyIndex >= (int)c.arms.alloys.size()) return k;
  Item it;
  it.kind = ItemKind::Armor;
  it.mat = Mat::Alloy;
  it.alloy = (uint8_t)(alloyIndex + 1);
  it.rarity = Rarity::Common;
  it.ilvl = 20;
  it.culture = c.id;
  const art::IconLook l = gearIconKeys(it, &c);
  k.light = l.tint; k.dark = l.tint2; k.sheen = l.sheen;
  return k;
}
int alloyMetalWord(const cult::Alloy& a, int rank) {
  static const char* const kWord[6] = {"BRONZE", "BRASS", "IRON", "STEEL", "SILVER", "GLASS"};
  const std::string& n = a.name;
  for (int w = 0; w < 6; w++) {
    const size_t k = std::strlen(kWord[w]);
    if (n.size() >= k && n.compare(n.size() - k, k, kWord[w]) == 0) return w;
  }
  // a name without a metal word (none today): the everyday metal by its recipe, the others a steel
  return rank == 0 ? (a.baseTier == cult::Tier::Bronze ? 0 : 2) : 3;
}
std::vector<uint8_t> alloyFamilies(const cult::Culture& c) {
  // (alloys i3, the blind panel: "Eaglesilver is drawn as brass, Thorniron as white silver") the metal follows the NAME: the
  // alloy's metal word and its rank pick a slot (the everyday bronze, brass or iron; the warriors' steel or silver; the
  // masterwork steel, silver or glass) and the people its own family in that slot (art::doll::kAlloyFam, solved so a
  // people's metals part by dE 22 and two peoples' same-slot metals by 13). A people whose metals would still sit within dE
  // 20 (a dialect's jittered sheen) borrows another people's family of the same metal word, so the name always holds.
  std::vector<uint8_t> out;
  std::vector<uint32_t> mids;
  const int a = std::clamp((int)c.archetype, 0, 11);
  for (int i = 0; i < (int)c.arms.alloys.size(); i++) {
    const AlloyKeys k = alloyKeys(c, i);
    const int rank = std::min(i, 2);
    const int w = alloyMetalWord(c.arms.alloys[(size_t)i], rank);
    int slot;
    if (rank == 0) slot = w == 1 ? 1 : (w == 0 ? 0 : 2);
    else if (rank == 1) slot = w == 4 ? 4 : 3;
    else slot = w == 4 ? 6 : (w == 5 ? 7 : 5);
    int pick = -1;
    for (int t = 0; t < 12 && pick < 0; t++) {
      const int f = art::doll::alloyFamilyAt(slot, (a + t) % 12);
      bool ok = true;
      for (uint8_t o : out) ok &= o != f;
      uint32_t r[5];
      art::doll::alloyRamp(f, k.sheen, r);
      for (uint32_t m : mids) ok &= art::doll::deltaE76(m, r[2]) >= 20.0f;
      if (ok) pick = f;
    }
    if (pick < 0) pick = art::doll::alloyFamilyAt(slot, a);
    uint32_t r[5];
    art::doll::alloyRamp(pick, k.sheen, r);
    mids.push_back(r[2]);
    out.push_back((uint8_t)pick);
  }
  return out;
}

void fillLadder(art::HumanLook& L, const WornGear& g, const CultureLookup& cultureOf) {
  using H = art::HumanLook;
  auto maker = [&](const Item* it) -> const cult::Culture* { return it && it->culture && cultureOf ? cultureOf(it->culture) : nullptr; };
  auto put = [&](int slot, const Item* it) {
    if (!it) return;
    const LookTier t = lookTier(*it, maker(it));
    if (!t.tier) return;
    L.tier[slot] = t.tier;
    if (t.rare) L.rare |= (uint8_t)(1u << slot);
    if (t.flame) L.mythic |= (uint8_t)(1u << slot);
  };
  put(H::TIER_BODY, g.armor);
  put(H::TIER_HELM, g.helmet);
  put(H::TIER_SHIELD, g.shield);
  put(H::TIER_GLOVES, g.gloves);
  put(H::TIER_BOOTS, g.boots);
  put(H::TIER_CLOAK, g.cloak);
  // in hand: the melee weapon, else the staff, else the bow (wearGear's rule); the bow or the staff left over rides the back
  const Item* hand = g.weapon ? g.weapon : (g.staff ? g.staff : g.bow);
  put(H::TIER_WEAPON, hand);
  const Item* back = g.bow && g.bow != hand ? g.bow : (g.staff && g.staff != hand ? g.staff : nullptr);
  put(H::TIER_BACK, back);
  // the armour's metal: the body's, else the helm's, the gloves', the boots' (gear_look's tint rule); the metal in hand
  for (const Item* src : {g.armor, g.helmet, g.gloves, g.boots})
    if (src) {
      const uint8_t m = metalKindOf(*src);
      if (m) { L.metalKind = m; break; }
    }
  if (hand) L.bladeKind = metalKindOf(*hand);
  // (alloys i1) the armour's alloy: its people's family for it (alloyFamilies), so the doll draws THAT metal
  if (L.metalKind == 10)
    for (const Item* src : {g.armor, g.helmet, g.gloves, g.boots})
      if (src && metalKindOf(*src) == 10) {
        if (const cult::Culture* c = maker(src)) {
          const std::vector<uint8_t> fam = alloyFamilies(*c);
          if (src->alloy >= 1 && src->alloy <= fam.size()) L.alloyFam = (uint8_t)(fam[src->alloy - 1] + 1);
          if (src->alloy >= 1) L.alloyRank = (uint8_t)std::min<int>(src->alloy, 3);   // (forge i2) the hidden 4th: a masterwork
        }
        break;
      }
  // (polish i1, the owner: "the player starts in T1 gear") nobody who dresses from an inventory goes bare at rig 2: an
  // empty body slot draws as T1's padded jerkin in the wearer's own shirt colour, empty feet as T1's leather boots (the
  // resolver's ladderLook makes them; the look's own fields, and so rig 1, are untouched). A worn piece keeps its tier.
  if (!g.armor && !L.tier[H::TIER_BODY]) { L.tier[H::TIER_BODY] = 1; L.floor |= (uint8_t)(1u << H::TIER_BODY); }
  if (!g.boots && !L.tier[H::TIER_BOOTS]) { L.tier[H::TIER_BOOTS] = 1; L.floor |= (uint8_t)(1u << H::TIER_BOOTS); }
  // (P2c forge, contracts 3c) each worn piece's forge style (forgeOf with its maker); the T1 floor's padded jerkin is
  // HOMESPUN, its leather boots TANNER'S
  auto forgeSlot = [&](int slot, const Item* it) {
    if (!it || !lookTier(*it, maker(it)).tier) return;
    L.forge[slot] = packForge(forgeOf(*it, maker(it)));
  };
  forgeSlot(H::TIER_BODY, g.armor);
  forgeSlot(H::TIER_HELM, g.helmet);
  forgeSlot(H::TIER_SHIELD, g.shield);
  forgeSlot(H::TIER_GLOVES, g.gloves);
  forgeSlot(H::TIER_BOOTS, g.boots);
  // (forge i2) the Heartland's leather, bronze, iron and steel wear their hero styles' own helm and shield shapes (another
  // people's keep its own forms): by the helm's maker, else the body's (no maker: the heartland look)
  {
    const Item* src = g.helmet ? g.helmet : g.armor;
    const cult::Culture* c = src ? maker(src) : nullptr;
    L.forgeKit = (uint8_t)((src && (!c || c->archetype == cult::Archetype::Heartland)) ? 1 : 0);
  }
  if (L.floor >> H::TIER_BODY & 1) L.forge[H::TIER_BODY] = packForge(baseStyle(0, cult::BodyArm::Padded));
  if (L.floor >> H::TIER_BOOTS & 1) L.forge[H::TIER_BOOTS] = packForge(baseStyle(1, cult::BodyArm::Leather));
  // (forge round 2, contracts 3c.1) the silhouette: the body piece's skirt, mass, layer, arm and motif, the helm piece's
  // family (D31: each piece its own style); the T1 floor's padded jerkin HOMESPUN's. forgeGuise is retired (left 0)
  {
    Shape s;
    if (L.floor >> H::TIER_BODY & 1) s = heroShape(art::doll::forge::B_HOMESPUN, nullptr);
    else if (g.armor && lookTier(*g.armor, maker(g.armor)).tier) {
      s = shapeOf(*g.armor, maker(g.armor));
      // (wave-2 fix round 2, D84 restated: "at its T6's area or more by construction") a Mythic relic body (gilded, obsidian,
      // emberforged, starmetal steel) wears MYTHIC's black, gold and fire and its tells over its own metal's outline (the
      // resolver reads relicBody: STARFALL's split skirt, FURNACE's heavy arms), as its helm keeps its family (D83)
      if (art::doll::fixOn(art::doll::FIX_RELIC) && g.armor->rarity == Rarity::Mythic && g.armor->mat == Mat::Steel) {
        const int band = bandOf(std::max(1, (int)g.armor->ilvl));
        static const uint8_t kRelicB[4] = {5, 7, 8, 9};   // metalKind gilded obsidian ember starmetal (bands 4-7)
        if (band >= 4) L.relicBody = (uint8_t)baseStyle(kRelicB[std::min(band, 7) - 4], cult::BodyArm::Plate).base;
      }
    }
    s.helm = 0;
    if (g.helmet && lookTier(*g.helmet, maker(g.helmet)).tier) {
      s.helm = shapeOf(*g.helmet, maker(g.helmet)).helm;
      // (wave-2 fix round, D83 / D84) a Mythic relic helm (gilded, obsidian, emberforged, starmetal steel) keeps its metal's helm
      // family in black, gold and fire: the onion, the mask, the furnace helm, the crystal (e6m the crystal's legend view)
      if (art::doll::fixOn(art::doll::FIX_RELIC) && g.helmet->rarity == Rarity::Mythic && g.helmet->mat == Mat::Steel) {
        const int band = bandOf(std::max(1, (int)g.helmet->ilvl));
        if (band >= 4) {
          static const uint8_t kRelic[4] = {5, 7, 8, 9};   // metalKind gilded obsidian ember starmetal (bands 4-7)
          const ForgeStyle rs = baseStyle(kRelic[std::min(band, 7) - 4], cult::BodyArm::Plate);
          const Shape rsh = heroShape(rs.base, nullptr);
          if (rsh.helm) s.helm = rsh.helm;
          L.relicHelm = (uint8_t)rs.base;
        }
      }
    }
    L.forgeShape = packShape(s);
    L.forgeGuise = 0;
  }
  // the filigree's motif: the body's maker people (else the helm's): Jade's cloud scrolls, the Imperial laurel, the northern
  // knotwork, the Heartland's oak leaves (the default, and every people without its own drawing yet)
  for (const Item* src : {g.armor, g.helmet})
    if (const cult::Culture* c = maker(src)) {
      using A = cult::Archetype;
      const A a = c->archetype;
      L.makerPeople = (uint8_t)((int)a + 1);   // (wave-2 fix round, D74) the people's T2 hems
      L.motif = a == A::Jade ? 1 : a == A::Imperial ? 2 : (a == A::Fjordfolk || a == A::Highland) ? 3 : 0;
      break;
    }
}

void rankTiers(art::HumanLook& L, int tier, bool person, const cult::Culture* realm) {
  using H = art::HumanLook;
  if (tier <= 0) return;
  const uint8_t t = (uint8_t)std::min(tier, 6);
  using O = art::Outfit;
  const bool armour = L.bodyForm || L.armorStyle || L.outfit == O::Leather || L.outfit == O::Chain || L.outfit == O::Plate ||
                      L.outfit == O::Guard || L.outfit == O::Elven || L.outfit == O::Ebony;
  if (armour) L.tier[H::TIER_BODY] = t;
  if (L.helmet || L.helmStyle || L.helmForm) L.tier[H::TIER_HELM] = t;
  if (L.shieldForm != 9 && (L.shield || L.shieldStyle || L.shieldForm)) L.tier[H::TIER_SHIELD] = t;
  if (L.weapon >= 1 && L.weapon <= 8) L.tier[H::TIER_WEAPON] = t;
  if (L.gloves) L.tier[H::TIER_GLOVES] = t;
  if (L.boots) L.tier[H::TIER_BOOTS] = t;
  if (L.cloak || L.cape) L.tier[H::TIER_CLOAK] = t;
  if (L.backItem) L.tier[H::TIER_BACK] = t;
  // (polish i1, the owner: "six identical guards in a row") each ranked fighter's own kit: a variant from the person's own
  // face (skin, hair, eyes, head: what census::dress already drew for them), so the same role, rank and culture still
  // differ man to man; the resolver reads it at rig 2 (ladderLook). No draw: the census's stream is untouched.
  if (armour && person) {
    uint32_t h = 0x9E3779B9u;
    for (uint32_t v : {L.skin, L.hairColor, L.eyeColor, (uint32_t)L.hair, (uint32_t)L.hairVar, (uint32_t)L.beard, (uint32_t)L.beardStyle,
                       (uint32_t)L.head, (uint32_t)L.nose, (uint32_t)L.brow, (uint32_t)L.body, (uint32_t)L.age})
      h = (h ^ v) * 0x01000193u, h ^= h >> 15;
    h *= 0x2C1B3C6Du; h ^= h >> 12;
    L.kitVar = (uint8_t)(1 + h % 255u);
  }
  // the metal: an alloy's tint, else the look's band (1 leather 2 iron 3 steel 4 gilded 5 jade 6 obsidian 7 ember)
  if (!L.metalKind) {
    static const uint8_t kBand[8] = {0, 1, 3, 4, 5, 6, 7, 8};
    const int band = L.armorStyle ? L.armorStyle : (L.helmStyle ? L.helmStyle : 0);
    if (L.armourTint) L.metalKind = 10;
    else if (band >= 1 && band <= 7) L.metalKind = kBand[band];
  }
  // (P2c forge, contracts 3c) the rank's metal as its hero style on every worn piece; (D29) the T4 officers (captains, the
  // royal guard, jarls) wear their realm's best alloy below the masterwork, kings (T5) the masterwork, so every kingdom's
  // officers look their own (the style and the alloy's metal family: ladder fields only, rig 1 untouched)
  {
    const cult::BodyArm form = L.bodyForm ? (cult::BodyArm)std::min<int>(L.bodyForm - 1, 7)
                                          : (L.outfit == O::Leather ? cult::BodyArm::Leather : L.outfit == O::Chain || L.outfit == O::Guard ? cult::BodyArm::Mail
                                                                                                                                         : cult::BodyArm::Plate);
    ForgeStyle st;
    if (realm && t >= 4 && !realm->arms.alloys.empty() && armour) {
      const int n = (int)realm->arms.alloys.size();
      const int idx = t >= 5 ? n - 1 : (n >= 3 ? n - 2 : n - 1 >= 1 ? 1 : 0);
      st = alloyStyle(*realm, idx);
      const std::vector<uint8_t> fam = alloyFamilies(*realm);
      L.metalKind = 10;
      if (idx < (int)fam.size()) L.alloyFam = (uint8_t)(fam[(size_t)idx] + 1);
      L.alloyRank = (uint8_t)std::min(idx + 1, 3);
      L.forgeGuise = 0;   // (forge round 2) retired: the silhouette below
      Shape sh = alloyShape(*realm, idx);
      if (!L.tier[H::TIER_HELM]) sh.helm = 0;
      if (!L.tier[H::TIER_BODY]) { const uint8_t h = sh.helm; sh = Shape{}; sh.helm = h; }
      L.forgeShape = packShape(sh);
    } else if (L.metalKind && L.metalKind != 10) {
      st = baseStyle(L.metalKind, L.metalKind == 1 && form != cult::BodyArm::Padded ? cult::BodyArm::Leather : form);
    } else if (armour && (form == cult::BodyArm::Padded || L.outfit == O::Tunic)) {
      st = baseStyle(0, cult::BodyArm::Padded);
    }
    const uint64_t pk = packForge(st);
    if (pk && (!realm || realm->archetype == cult::Archetype::Heartland)) L.forgeKit = 1;   // (forge i2) the Heartland's own kit
    if (pk && st.base != art::doll::forge::B_ALLOY) {   // (forge round 2) a material's silhouette (informational: heroes keep their drawings)
      Shape sh = heroShape(st.base, L.forgeKit ? nullptr : realm);
      if (!L.tier[H::TIER_HELM]) sh.helm = 0;
      if (!L.tier[H::TIER_BODY]) { const uint8_t h = sh.helm; sh = Shape{}; sh.helm = h; }
      L.forgeShape = packShape(sh);
    }
    if (pk) {
      if (L.tier[H::TIER_BODY]) L.forge[H::TIER_BODY] = pk;
      if (L.tier[H::TIER_HELM]) L.forge[H::TIER_HELM] = pk;
      if (L.tier[H::TIER_SHIELD]) L.forge[H::TIER_SHIELD] = pk;
      if (L.tier[H::TIER_GLOVES]) L.forge[H::TIER_GLOVES] = pk;
      if (L.tier[H::TIER_BOOTS]) L.forge[H::TIER_BOOTS] = pk;
    }
  }
}

// ---------------------------------------------------------------- the kit
WornGear LadderKit::worn() const {
  WornGear w;
  if (hasWeapon) w.weapon = &weapon;
  if (hasBow) w.bow = &bow;
  if (hasStaff) w.staff = &staff;
  if (hasArmor) w.armor = &armor;
  if (hasHelmet) w.helmet = &helmet;
  if (hasShield) w.shield = &shield;
  if (hasGloves) w.gloves = &gloves;
  if (hasBoots) w.boots = &boots;
  if (hasCloak) w.cloak = &cloak;
  return w;
}
CultureLookup LadderKit::lookup() const {
  const cult::Culture* c = &culture;
  return [c](uint64_t id) -> const cult::Culture* { return id == c->id ? c : nullptr; };
}

bool peopleOf(const std::string& n, cult::Archetype& out) {
  static const char* const k[12] = {"fjordfolk", "highland", "heartland", "imperial", "dune", "steppe", "marsh", "jade", "river", "suntemple",
                                    "sylvan", "starspire"};
  for (int i = 0; i < 12; i++)
    if (n == k[i]) { out = (cult::Archetype)i; return true; }
  return false;
}

const cult::Culture& kitCulture(cult::Archetype a, uint32_t seed) {
  static std::map<uint64_t, std::unique_ptr<cult::Culture>> memo;
  const uint64_t key = (uint64_t)a << 32 | seed;
  auto it = memo.find(key);
  if (it != memo.end()) return *it->second;
  auto c = std::make_unique<cult::Culture>(cult::Atlas::make(a, seed));
  // a test culture's id (Atlas::make bypasses the lattice): nonzero, so items carry it and the lookup resolves it
  c->id = 0x1ADD000000000000ull | key;
  return *(memo[key] = std::move(c));
}
namespace {
// (wave-2 fix round, D89) every family a kit tag names; -1 for one that is not a melee weapon type (bow, crossbow, staff,
// unarmed: ladderKit's own branches) and -2 for an unknown name (applyLadderSpec reports it: a tag renders what it says)
int weaponTypeOf(const std::string& w) {
  if (w.empty() || w == "sword") return (int)WeaponType::Sword;
  if (w == "axe") return (int)WeaponType::Axe;
  if (w == "mace") return (int)WeaponType::Mace;
  if (w == "dagger") return (int)WeaponType::Dagger;
  if (w == "greatsword") return (int)WeaponType::Greatsword;
  if (w == "spear") return (int)WeaponType::Spear;
  if (w == "bow" || w == "crossbow" || w == "staff" || w == "unarmed") return -1;
  return -2;
}
}  // namespace

// (P2c envelope r3) the kit's look dial that no item carries: the T3 cape (the cloak's tier 3, so the cape's T3 views draw);
// wearGear's own dials stand otherwise
static void applyKitDials(art::HumanLook& L, const LadderKit& K, int tier) {
  if (K.cape && !L.cloak) {
    L.cape = true;
    if (!L.tier[art::HumanLook::TIER_CLOAK]) L.tier[art::HumanLook::TIER_CLOAK] = (uint8_t)std::clamp(tier, 1, 6);
  }
  // (wave-2 fix round, D73) the kit's cloak is the tier's cloak: T3 the travel cloak at cloak.t3, T4 the officer's cloak at
  // cloak.t4 (the band-3 travel cloak item reads T3 by its own level; the kit row is T4's), T5-T6 the long cloak at its tier
  if (art::doll::fixOn(art::doll::FIX_CLOAK) && K.hasCloak && L.cloak && tier >= 3) L.tier[art::HumanLook::TIER_CLOAK] = (uint8_t)std::clamp(tier, 3, 6);
}

// (wave-2 fix round 4, D97) the kit's trim from its shield item, carried or not (wearGear's own rule for a carried shield:
// the item's tint, else its metal's light), so a kit tag draws one figure whatever it holds: Metal 2 (the crests, crowns,
// fittings and the trim below T5) never falls back to brass on the two-handed rows. wearGear itself is unchanged (rig 1)
static void kitTrim(art::HumanLook& L, const LadderKit& K) {
  if (!K.shieldMade || K.hasShield || !art::doll::fixOn(art::doll::FIX_KIT)) return;
  const CultureLookup look = K.lookup();
  const cult::Culture* mk = K.shield.culture ? look(K.shield.culture) : nullptr;
  L.trimColor = shieldTrimOf(K.shield, mk);
}

LadderKit ladderKit(cult::Archetype people, int tier, int body, const std::string& wpn, int rarity, uint32_t seed) {
  LadderKit k;
  k.culture = kitCulture(people, seed);
  const cult::Culture* C = &k.culture;
  const cult::ArmsStyle& A = C->arms;
  const bool heart = people == cult::Archetype::Heartland;
  tier = std::clamp(tier, 1, 6);
  Rng r(0x1ADDE7ull ^ (uint64_t)seed * 0x9E3779B97F4A7C15ull ^ (uint64_t)tier << 40 ^ (uint64_t)people << 48);
  // the kit table (contracts 3b): the piece's item level and rarity per tier
  static const int kIlvl[7] = {0, 3, 6, 20, 28, 47, 47};
  static const Rarity kRar[7] = {Rarity::Common, Rarity::Common, Rarity::Rare, Rarity::Common, Rarity::Common, Rarity::Rare, Rarity::Legendary};
  // (ladder i2) the Heartland's legend is STARMETAL (its band-7 steel: gear.cpp kHeartHigh): the panel read T6 in the
  // masterwork alloy as "T5 with a few sparkle pixels"; a legendary is earned from level 20 on, so a band-7 piece is a
  // fair pick for the kit, and its own metal (pale blue-white, a cold light) is the step up the eye wants
  const bool starT6 = heart && tier == 6;
  // (alloys i3) the Heartland's T5 is GILDED steel (its band-4 steel, an Epic piece: T5): i1-i2 drew its masterwork alloy
  // LIONSTEEL in brass to keep the owner's gold T5, and the panel caught the name ("a steel drawn as brass"); the gilding
  // keeps the gold and the name agrees, and LIONSTEEL draws as the steel it is (the noble cloak and the heater keep their
  // band-6 pieces: gear_look reads their shapes from the item's tier)
  const bool gildT5 = heart && tier == 5;
  const int il = starT6 ? 55 : gildT5 ? 30 : kIlvl[tier];
  const Rarity rar = rarity >= 0 ? (Rarity)std::clamp(rarity, 0, RARITIES - 1) : gildT5 ? Rarity::Epic : kRar[tier];
  auto ovr = [&](Rarity own) { return rarity >= 0 ? (Rarity)std::clamp(rarity, 0, RARITIES - 1) : own; };
  // the metal of a metal piece: steel at T3, the first alloy at T4, the masterwork (the best) alloy at T5-T6
  const uint8_t nAl = (uint8_t)A.alloys.size();
  auto metalAt = [&](uint8_t& alloy) -> Mat {
    alloy = 0;
    if (tier <= 2) return Mat::Iron;
    if (tier == 3 || !nAl || starT6 || gildT5) return Mat::Steel;
    alloy = tier == 4 ? 1 : nAl;
    return Mat::Alloy;
  };
  auto make = [&](ItemKind kind, int sub, int ilvl, Rarity rr, Mat m, uint8_t alloy, const cult::Culture* maker) {
    return makeGearC(r, kind, sub, ilvl, rr, maker, m, alloy);
  };
  auto form = [&](Item& it, int f) { if (heart && f >= 0) it.form = (uint8_t)(f + 1); };
  using HF = cult::HelmForm;
  using BA = cult::BodyArm;
  using SF = cult::ShieldForm;
  uint8_t al = 0;
  // body
  if (tier <= 2) {
    k.armor = make(ItemKind::Armor, 0, il, rar, Mat::Leather, 0, C);
    form(k.armor, tier == 1 ? (int)BA::Padded : (int)BA::Leather);
  } else {
    const Mat m = metalAt(al);
    k.armor = make(ItemKind::Armor, 0, il, rar, m, al, C);
    form(k.armor, tier == 3 ? (int)BA::Mail : (int)BA::Plate);
  }
  k.hasArmor = true;
  // helm (none at T1)
  if (tier == 2) {
    // the soft helm; the Heartland's leather cap is the band-1 heartland piece (no maker form: helm band1)
    k.helmet = make(ItemKind::Helmet, 0, il, rar, Mat::Leather, 0, heart ? nullptr : C);
    k.hasHelmet = true;
  } else if (tier >= 3) {
    const Mat m = metalAt(al);
    k.helmet = make(ItemKind::Helmet, 0, il, rar, m, al, C);
    // the Heartland's nasal at T3, then the open crested helm on both bodies (ladder i2: the panel, unanimous, read the
    // closed great helm on the hero as "a bucket", "a crate on legs", the face and the hero gone; the great helm stays
    // in the library for the census's knights and for a GREAT HELM item, and is one word here to bring back)
    (void)body;
    form(k.helmet, tier == 3 ? (int)HF::Nasal : (int)HF::Crested);
    k.hasHelmet = true;
  }
  // shield (a people with no shield bears none; two-handed weapons and bows carry none)
  const std::string w = wpn.empty() ? std::string("sword") : wpn;
  const bool twoHand = w == "greatsword" || w == "spear" || w == "bow" || w == "crossbow" || w == "staff" || w == "unarmed";   // (D89: fists bear no shield)
  // (wave-2 fix round 4, D97: one kit, one look) the shield item is made whatever the weapon, so every later draw of the kit's
  // Rng (the weapon, gloves, boots, cloak) is the same on every `wpn=` row; it is carried only with a one-handed weapon
  const bool makeShield = art::doll::fixOn(art::doll::FIX_KIT) ? (A.shield != SF::None || heart) : (!twoHand && (A.shield != SF::None || heart));
  if (makeShield) {
    if (tier <= 2) {
      k.shield = make(ItemKind::Shield, 0, il, rar, Mat::Leather, 0, C);
      if (heart || A.shield == SF::Round || A.shield == SF::Kite || A.shield == SF::Heater) k.shield.form = (uint8_t)((int)SF::Round + 1);
    } else {
      const Mat m = metalAt(al);
      // (alloys i3) the gilded T5's heater is the masterwork alloy's (a Rare band-6 piece, the noble heater's shape: a
      // band-4 heater is the smaller kite-cut one); the doll draws the shield in the harness's trim, so the steel reads
      if (gildT5 && nAl) k.shield = make(ItemKind::Shield, 0, kIlvl[5], kRar[5], Mat::Alloy, nAl, C);
      else k.shield = make(ItemKind::Shield, 0, il, rar, m, al, C);
      form(k.shield, tier == 3 ? (int)SF::Kite : (int)SF::Heater);
    }
    k.shieldMade = true;
    k.hasShield = !twoHand;
  }
  // the weapon in hand
  if (w == "bow" || w == "crossbow") {
    k.bow = make(ItemKind::Bow, 0, tier <= 2 ? (tier == 1 ? 3 : 10) : il, tier == 2 ? ovr(Rarity::Common) : rar, Mat::Wood, 0, C);
    if (w == "crossbow") k.bow.form = (uint8_t)((int)cult::BowKind::Crossbow + 1);   // (D89) the crossbow's own form, any people
    k.hasBow = true;
  } else if (w == "unarmed") {
    // (D89) fists: nothing in hand
  } else if (w == "staff") {
    k.staff = make(ItemKind::Staff, 0, tier <= 2 ? (tier == 1 ? 3 : 10) : il, tier == 2 ? ovr(Rarity::Common) : rar, Mat::Wood, 0, C);
    k.hasStaff = true;
  } else {
    const int sub = std::max(0, weaponTypeOf(w));
    if (tier == 1) {
      if (heart && sub == (int)WeaponType::Sword) {
        // the OLD BLADE (game_rpg.cpp giveFirstWeapon): an item-level-1 iron heirloom in the old heartland make
        k.weapon = make(ItemKind::Weapon, sub, 1, ovr(Rarity::Common), Mat::Iron, 0, nullptr);
        k.weapon.name = "OLD BLADE";
      } else {
        k.weapon = make(ItemKind::Weapon, sub, 3, ovr(Rarity::Common), Mat::Bronze, 0, C);
      }
    } else if (tier == 2) {
      k.weapon = make(ItemKind::Weapon, sub, 10, ovr(Rarity::Common), Mat::Iron, 0, C);
    } else {
      const Mat m = metalAt(al);
      k.weapon = make(ItemKind::Weapon, sub, il, rar, m, al, C);
    }
    if (heart && sub == (int)WeaponType::Sword && k.weapon.culture) k.weapon.form = (uint8_t)((int)cult::Blade::Straight + 1);
    if (heart && sub == (int)WeaponType::Spear) k.weapon.form = (uint8_t)((int)cult::Polearm::Spear + 1);   // the runed spear
    k.hasWeapon = true;
  }
  // gloves and boots: T1 boots only; T2 leather; T3 leather gloves (band-1 Rare), tall iron boots (band 2); T4+ metal
  if (tier == 2) {
    k.gloves = make(ItemKind::Gloves, 0, il, rar, Mat::Leather, 0, C); k.hasGloves = true;
  } else if (tier == 3) {
    k.gloves = make(ItemKind::Gloves, 0, 6, ovr(Rarity::Rare), Mat::Leather, 0, C); k.hasGloves = true;
  } else if (tier >= 4) {
    const Mat m = metalAt(al);
    k.gloves = make(ItemKind::Gloves, 0, il, rar, m, al, C); k.hasGloves = true;
  }
  if (tier <= 2) k.boots = make(ItemKind::Boots, 0, il, rar, Mat::Leather, 0, C);
  else if (tier == 3) k.boots = make(ItemKind::Boots, 0, 10, ovr(Rarity::Common), Mat::Iron, 0, C);
  else { const Mat m = metalAt(al); k.boots = make(ItemKind::Boots, 0, il, rar, m, al, C); }
  k.hasBoots = true;
  // the cloak: T3 a travel cloak where the people wear a long one (cape dial 2), T4 a travel cloak, T5-T6 the noble cloak
  if ((tier == 3 && A.cape == 2) || tier == 4) {
    k.cloak = make(ItemKind::Cloak, 0, 20, ovr(Rarity::Common), Mat::Cloth, 0, C);   // band 3: the travel cloak (cloakStyleOf)
    k.hasCloak = true;
  } else if (tier >= 5) {
    k.cloak = make(ItemKind::Cloak, 0, gildT5 ? kIlvl[5] : il, rar, Mat::Cloth, 0, C);   // band 6: the noble cloak
    k.hasCloak = true;
  }
  // the cloak's style is read from the item (gear_look.cpp cloakStyleOf: a MANTLE is the fur mantle): the kit wears the
  // travel cloak (T3-T4) and the noble cloak (T5-T6), so a culture's mantle name gives way to its cloak
  if (k.hasCloak) {
    const size_t m = k.cloak.name.find("MANTLE");
    if (m != std::string::npos) k.cloak.name.replace(m, 6, "CLOAK");
  }
  // (P2c envelope r3, C12f ladder lines) T3 wears the people's cape (PLAN's T3 row: "the people's cape, to the waist"; the
  // fair test's T3 guard): a people with a long cloak (cape dial 2) has its travel cloak above, one with none bears none
  // (wave-2 fix round, D73) every people's line wears its T3 cloak: the people's cape where its dial is 0 or 1 (a people with
  // no cape of its own wears the plain short cape in its colour), the travel cloak above where it is 2
  k.cape = tier == 3 && !k.hasCloak && (art::doll::fixOn(art::doll::FIX_CLOAK) || A.cape == 1 || heart);
  if (k.hasCloak && !k.cloak.tint) k.cloak.tint = (C->heraldry.field ? C->heraldry.field : (A.cloth ? A.cloth : 0)) | 0xFF000000u;
  return k;
}

// ---------------------------------------------------------------- the looks macros
namespace {
uint32_t hexc(uint32_t rgb) { return 0xFF000000u | (rgb & 0xFF) << 16 | (rgb & 0xFF00) | (rgb >> 16 & 0xFF); }
void a3HeroLook(art::HumanLook& L) {   // tools/tests' a3Hero (A3's face, hair and colours; the scarf)
  L.body = 0; L.hair = art::Hair::Short; L.hairVar = 2; L.neck = 1; L.outfit = art::Outfit::Tunic;
  L.skin = hexc(0xE9A077); L.hairColor = hexc(0xAA4A2C); L.topColor = hexc(0x28728C); L.bottomColor = hexc(0x4F4B6B); L.tabardColor = hexc(0xC83E38);
  L.eyeColor = hexc(0x3A78B0); L.head = 0; L.nose = 0; L.brow = 0; L.beard = false;
}
void a3HeroineLook(art::HumanLook& L) {
  L.body = 1; L.hair = art::Hair::Braids; L.hairVar = 0; L.neck = 1; L.outfit = art::Outfit::Tunic;
  L.skin = hexc(0xEEAA86); L.hairColor = hexc(0xC79346); L.topColor = hexc(0x922F60); L.bottomColor = hexc(0x4F4B6B); L.tabardColor = hexc(0x28728C);
  L.eyeColor = hexc(0x3F8A5A); L.head = 0; L.nose = 0; L.brow = 0; L.beard = false;
}
int rarityOf(const std::string& v) {
  static const char* const k[6] = {"common", "uncommon", "rare", "epic", "legendary", "mythic"};
  for (int i = 0; i < 6; i++) if (v == k[i]) return i;
  const int n = (int)std::strtol(v.c_str(), nullptr, 10);
  return n >= 0 && n < 6 && !v.empty() && v[0] >= '0' && v[0] <= '9' ? n : -1;
}
bool roleOf(const std::string& v, Role& out) {
  static const char* const k[(int)Role::COUNT] = {"villager", "guard", "merchant", "smith", "innkeeper", "priest", "jarl", "farmer", "child", "mage",
                                                  "bandit", "king", "hunter", "fisher", "herbalist", "traveller", "herald", "soldier", "captain",
                                                  "refugee", "harbourmaster", "shipwright", "sailor", "pirate", "steward", "warlord", "envoy"};
  for (int i = 0; i < (int)Role::COUNT; i++) if (v == k[i]) { out = (Role)i; return true; }
  return false;
}
std::vector<std::string> split(const std::string& s, char c) {
  std::vector<std::string> v;
  size_t a = 0;
  while (a <= s.size()) {
    size_t b = s.find(c, a);
    if (b == std::string::npos) b = s.size();
    v.push_back(s.substr(a, b - a));
    a = b + 1;
  }
  return v;
}
}  // namespace

void applyLadderSpec(art::HumanLook& L, const std::string& spec, bool* lookPal, bool* bare, std::string* bad) {
  if (spec.empty() || spec == "-") return;
  struct KV { std::string k, v; };
  std::vector<KV> plain, tiers, forges;   // (P2c forge) mat= forge= style= alloyStyle= forgeBody=..
  int hero = -1;   // 0 hero, 1 heroine
  std::string kit, npc, wpn;
  int rarity = -1, kitAlloy = 0;
  auto addBad = [&](const std::string& k) { if (bad) *bad += (bad->empty() ? "" : ",") + k; };
  for (const std::string& kv : split(spec, ',')) {
    if (kv.empty()) continue;
    const size_t eq = kv.find('=');
    if (eq == std::string::npos) { addBad(kv); continue; }
    const std::string k = kv.substr(0, eq), v = kv.substr(eq + 1);
    if (k == "hero") { if (v != "0") hero = 0; }
    else if (k == "heroine") { if (v != "0") hero = 1; }
    else if (k == "kit") kit = v;
    else if (k == "npc") npc = v;
    else if (k == "wpn") { wpn = v; if (weaponTypeOf(v) == -2) addBad("wpn=" + v + " (sword axe mace dagger greatsword spear bow crossbow staff unarmed)"); }
    else if (k == "alloy") kitAlloy = (int)std::strtol(v.c_str(), nullptr, 10);   // (alloys i1) the kit's metal pieces in alloy N
    else if (k == "rarity") { rarity = rarityOf(v); if (rarity < 0) addBad(k); }
    else if (k == "mat" || k == "forge" || k == "style" || k == "alloyStyle" || k == "shape" || k == "alloyShape" ||
             (k.rfind("forge", 0) == 0 && k.size() > 5 && k != "forgeKit" && k != "forgeGuise")) forges.push_back(KV{k, v});
    else if (k == "tier" || k.rfind("tier", 0) == 0 || k == "rare" || k == "metalKind" || k == "bladeKind" || k == "gemColor" || k == "mythic" || k == "motif" || k == "kitVar" || k == "floor" || k == "alloyFam" || k == "forgeKit" || k == "alloyRank" || k == "forgeGuise" || k == "king")
      tiers.push_back(KV{k, v});
    else plain.push_back(KV{k, v});
  }
  if (hero == 0) a3HeroLook(L);
  else if (hero == 1) a3HeroineLook(L);
  if (!kit.empty()) {
    const std::vector<std::string> f = split(kit, ':');
    cult::Archetype a = cult::Archetype::Heartland;
    // (envelope fix round, D64) kit=<people>:<tier>[m]:<seed>: `m` (tier 6 only) is the Mythic, the same look as
    // kit=<people>:6:<seed>,rarity=mythic; any other suffix or a malformed tier is an error naming the tag (the parser read
    // "6m" as 6 and silently dropped the m)
    int t = 0;
    bool tierOk = f.size() >= 2 && !f[1].empty();
    if (tierOk) {
      size_t i = 0;
      while (i < f[1].size() && f[1][i] >= '0' && f[1][i] <= '9') i++;
      const std::string suf = f[1].substr(i);
      t = i ? (int)std::strtol(f[1].substr(0, i).c_str(), nullptr, 10) : -1;
      if (!i || t < 1 || t > 6) tierOk = false;
      else if (suf == "m") { if (t != 6) tierOk = false; else if (rarity < 0) rarity = 5; else if (rarity != 5) tierOk = false; }
      else if (!suf.empty()) tierOk = false;
    }
    if (f.size() < 2 || !peopleOf(f[0], a)) addBad("kit");
    else if (!tierOk) addBad("kit=" + kit + " (tier 1-6, `m` only on 6)");
    else {
      const uint32_t seed = f.size() >= 3 ? (uint32_t)std::strtoul(f[2].c_str(), nullptr, 10) : 1u;
      LadderKit K = ladderKit(a, t, L.body, wpn, rarity, seed);
      // (alloys i1) alloy=N (1-based): every metal piece of the kit in the people's alloy N (the alloys board)
      if (kitAlloy >= 1 && kitAlloy <= (int)K.culture.arms.alloys.size())
        for (Item* it : {&K.weapon, &K.armor, &K.helmet, &K.shield, &K.gloves, &K.boots})
          if (it->mat == Mat::Alloy || it->mat == Mat::Steel || it->mat == Mat::Iron) { it->mat = Mat::Alloy; it->alloy = (uint8_t)kitAlloy; }
      L.outfit = art::Outfit::Tunic;   // appearanceLook's start: the shirt under the kit
      L.weapon = 0; L.shield = false; L.backItem = 0;
      wearGear(L, K.worn(), K.lookup());
      kitTrim(L, K);
      applyKitDials(L, K, t);
      // the scarf through T3 (contracts 3b): from T4 the gorget and the great helm cover it
      if (hero >= 0 && L.tier[art::HumanLook::TIER_BODY] >= 4) L.neck = 0;
      // (ladder i2) the hero's crest and plume in his own colour (the kit's maker device green read as "a sprig", on the
      // Mythic orange "a carrot"); (i3) the Mythic's crest a flame over the blackened steel
      if (hero >= 0 && L.tier[art::HumanLook::TIER_HELM] >= 3)
        L.plumeColor = L.mythic ? hexc(0xF06A1E) : (L.tabardColor ? L.tabardColor : 0xFF383EC8u);   // (i3) the Mythic's a flame
      // the hero's cloak in his own arms (the shield field's colour), not the maker's cloth: it parts from the alloy
      if (hero >= 0 && L.cloak) {   // a deep shade of it (55 % toward black), so it parts from the plume and the field
        const uint32_t c = L.tabardColor;
        L.cloakColor = 0xFF000000u | (uint32_t)((c & 255) * 0.55f) | (uint32_t)((c >> 8 & 255) * 0.55f) << 8 | (uint32_t)((c >> 16 & 255) * 0.55f) << 16;
        // (ladder i3: the blind panel saw T4, T5 and T6 from behind as "the same brown cloak three times") the dye climbs
        // with the cloak's tier, as dear dyes do: T4 the travel cloak's deep shade, T5 the arms' own colour near full
        // strength, T6 a royal blue (the STARMETAL's cold light), the Mythic a black crimson (darker than T5's) under the blackened steel
        const int tc = L.tier[art::HumanLook::TIER_CLOAK];
        if (tc == 5) L.cloakColor = 0xFF000000u | (uint32_t)((c & 255) * 0.74f) | (uint32_t)((c >> 8 & 255) * 0.5f) << 8 | (uint32_t)((c >> 16 & 255) * 0.62f) << 16;   // crimson
        if (tc >= 6) L.cloakColor = hexc(0x23387A);
        if (L.mythic) L.cloakColor = hexc(0x480A14);
      }
    }
  } else if (!npc.empty()) {
    const std::vector<std::string> f = split(npc, ':');
    cult::Archetype a = cult::Archetype::Heartland;
    Role role = Role::Villager;
    if (f.size() < 2 || !peopleOf(f[0], a) || !roleOf(f[1], role)) addBad("npc");
    else {
      const int rank = f.size() >= 3 ? (int)std::strtol(f[2].c_str(), nullptr, 10) : 0;
      const uint32_t seed = f.size() >= 4 ? (uint32_t)std::strtoul(f[3].c_str(), nullptr, 10) : 1u;
      const cult::Culture& C = kitCulture(a, seed);
      census::Rank rk;
      rk.royal = rank >= 2;
      rk.capital = rank >= 1;
      std::string name;
      const bool female = L.body == 1;
      art::HumanLook N;
      // the role's base as Game::makeLook sets it before the census dresses it (its fixed choices; no draw)
      using O = art::Outfit;
      N.body = female ? 1 : 0;
      switch (role) {
        case Role::Guard:
          N.outfit = O::Guard; N.helmet = true; N.shield = true; N.weapon = 1;
          if (rank >= 2) { N.outfit = O::Plate; N.armorStyle = 3; N.helmStyle = 3; N.shieldStyle = 2; N.cloak = 3; }
          else if (rank == 1) N.shieldStyle = 2;
          break;
        case Role::Bandit: N.outfit = O::Leather; N.hood = true; N.weapon = 1; break;
        case Role::King: N.outfit = O::Plate; N.armorStyle = 4; N.helmStyle = 4; N.helmet = true; N.cloak = 3; N.weapon = 1; N.amulet = true; N.ring = true; break;
        case Role::Soldier: N.outfit = O::Guard; N.helmet = true; N.shield = true; N.weapon = 8; break;
        case Role::Captain: N.outfit = O::Plate; N.helmet = true; N.shield = true; N.weapon = 1; N.shieldStyle = 2; N.cloak = 1; break;
        case Role::Jarl: N.outfit = O::Plate; N.cape = true; break;
        default: break;
      }
      census::dress(N, name, role, female, C, C, (uint64_t)seed * 0x9E3779B97F4A7C15ull ^ 0xC3A5ull, rk);
      L = N;
    }
  }
  for (const KV& kv : plain) {
    if (kv.k == "palette") { if (lookPal) *lookPal = kv.v == "look"; }
    else if (kv.k == "bare") { if (bare) *bare = kv.v == "1"; }
    else if (!art::doll::applyLookField(L, kv.k, kv.v)) addBad(kv.k);
  }
  for (const KV& kv : tiers) {
    const int n = (int)std::strtol(kv.v.c_str(), nullptr, 0);
    using H = art::HumanLook;
    static const char* const kSlot[8] = {"tierBody", "tierHelm", "tierShield", "tierWeapon", "tierGloves", "tierBoots", "tierCloak", "tierBack"};
    bool done = false;
    if (kv.k == "tier") {
      // every slot the look wears takes n (the census's rule: rankTiers), keeping the metal it has
      for (uint8_t& t : L.tier) t = 0;
      rankTiers(L, n, false);
      done = true;
    }
    for (int s = 0; s < 8 && !done; s++)
      if (kv.k == kSlot[s]) { L.tier[s] = (uint8_t)std::clamp(n, 0, 6); done = true; }
    if (!done) {
      if (kv.k == "rare") L.rare = (uint8_t)n;
      else if (kv.k == "mythic") L.mythic = (uint8_t)n;
      else if (kv.k == "metalKind") L.metalKind = (uint8_t)n;
      else if (kv.k == "bladeKind") L.bladeKind = (uint8_t)n;
      else if (kv.k == "motif") L.motif = (uint8_t)n;
      else if (kv.k == "kitVar") L.kitVar = (uint8_t)n;
      else if (kv.k == "floor") L.floor = (uint8_t)n;
      else if (kv.k == "alloyFam") L.alloyFam = (uint8_t)n;
      else if (kv.k == "forgeKit") L.forgeKit = (uint8_t)n;
      else if (kv.k == "alloyRank") L.alloyRank = (uint8_t)n;
      else if (kv.k == "forgeGuise") L.forgeGuise = (uint16_t)n;
      else if (kv.k == "king") L.king = (uint8_t)n;   // (D96)
      else if (kv.k == "gemColor") {
        const uint32_t rgb = (uint32_t)std::strtoul(kv.v.c_str() + (!kv.v.empty() && kv.v[0] == '#' ? 1 : 0), nullptr, 16);
        L.gemColor = kv.v == "0" ? 0u : hexc(rgb);
      } else addBad(kv.k);
    }
    (void)H::TIER_BODY;
  }
  // (P2c forge, contracts 3c) the forge macros, last: mat= (the kit's armour re-materialled: cloth padded, leather leather, the
  // rest in the kit's own form; the blade keeps its own metal), forge= (a hero style on every worn armour piece), style=
  // (values forced on the body), alloyStyle=<people>:<seed>:<n> (that culture's alloy n's style on the body), forgeBody=..
  // forgeBack= (a packed style, --expand-looks' output)
  namespace FG = art::doll::forge;
  using H = art::HumanLook;
  auto bodyArm = [&]() { return L.bodyForm ? (cult::BodyArm)std::min<int>(L.bodyForm - 1, 7) : cult::BodyArm::Plate; };
  // a hero style's silhouette (informational: the resolver keeps the hero's own drawings but for the round-2 fixes)
  auto heroSilhouette = [&](int base) {
    Shape sh = heroShape(base, nullptr);
    if (!(L.helmet || L.helmStyle || L.helmForm)) sh.helm = 0;
    L.forgeShape = packShape(sh);
  };
  auto onWorn = [&](uint64_t pk) {
    for (int sl : {(int)H::TIER_BODY, (int)H::TIER_HELM, (int)H::TIER_SHIELD, (int)H::TIER_GLOVES, (int)H::TIER_BOOTS}) {
      const bool worn = sl == H::TIER_BODY ? (L.bodyForm || L.armorStyle) : sl == H::TIER_HELM ? (L.helmet || L.helmStyle || L.helmForm)
                      : sl == H::TIER_SHIELD ? (L.shieldForm != 9 && (L.shield || L.shieldStyle || L.shieldForm)) : sl == H::TIER_GLOVES ? L.gloves != 0 : L.boots != 0;
      if (worn) L.forge[sl] = pk;
    }
  };
  for (const KV& kv : forges) {
    if (kv.k == "mat") {
      static const char* const kMat[10] = {"cloth", "leather", "bronze", "iron", "steel", "gilded", "obsidian", "ember", "starmetal", "mythic"};
      static const uint8_t kKind[10] = {1, 1, 2, 3, 4, 5, 7, 8, 9, 11};
      int m = -1;
      for (int i = 0; i < 10; i++) if (kv.v == kMat[i]) m = i;
      if (m < 0) { addBad(kv.k); continue; }
      if (!L.bladeKind) L.bladeKind = L.metalKind;   // the blade keeps its own metal (D30)
      if (L.bladeKind == 10) L.bladeKind = 4;          // (the kit's alloy blade drawn as the steel it is: the alloy's tint goes)
      L.metalKind = kKind[m];
      L.armourTint = 0; L.armourTint2 = 0; L.alloyFam = 0; L.sheen = 0; L.alloyRank = 0; L.forgeGuise = 0;
      if (m == 0) L.bodyForm = 1;
      else if (m == 1) L.bodyForm = 2;
      else if (L.bodyForm <= 2) L.bodyForm = 7;
      if (m == 9) L.mythic |= (uint8_t)(1u << H::TIER_BODY);
      L.forgeKit = 1;   // (forge i2) the materials board: each material's own helm and shield
      const ForgeStyle st = baseStyle(L.metalKind, m == 0 ? cult::BodyArm::Padded : bodyArm());
      onWorn(packForge(st));
      heroSilhouette(st.base);
    } else if (kv.k == "forge") {
      const int b = FG::valueOf(FG::AX_BASE, kv.v.c_str());
      if (b < 1) { addBad(kv.k); continue; }
      uint8_t ax[FG::AX_COUNT];
      FG::heroAxes(b, FG::familyOfBodyArm((int)bodyArm()), ax);
      onWorn(FG::pack(ax));
      L.forgeGuise = 0;   // (forge i3) a hero style is made in its own silhouette
      heroSilhouette(b);
    } else if (kv.k == "style") {
      uint8_t ax[FG::AX_COUNT];
      FG::unpack(L.forge[H::TIER_BODY], ax);
      bool ok = true;
      for (const std::string& av : split(kv.v, '+')) {
        const size_t c = av.find(':');
        const int a = c == std::string::npos ? -1 : FG::axisOfName(av.substr(0, c).c_str());
        const int v = a < 0 ? -1 : (av.substr(c + 1) == "none" ? 0 : FG::valueOf(a, av.substr(c + 1).c_str()));
        if (a < 0 || v < 0) { ok = false; continue; }
        ax[a] = (uint8_t)v;
      }
      if (!ok) addBad(kv.k);
      L.forge[H::TIER_BODY] = FG::pack(ax);
    } else if (kv.k == "alloyStyle") {
      const std::vector<std::string> f = split(kv.v, ':');
      cult::Archetype a = cult::Archetype::Heartland;
      if (f.size() < 3 || !peopleOf(f[0], a)) { addBad(kv.k); continue; }
      const cult::Culture& C = kitCulture(a, (uint32_t)std::strtoul(f[1].c_str(), nullptr, 10));
      const int n = (int)std::strtol(f[2].c_str(), nullptr, 10);
      if (n < 1 || n > (int)C.arms.alloys.size()) { addBad(kv.k); continue; }
      L.forge[H::TIER_BODY] = packForge(alloyStyle(C, n - 1));
      L.forgeGuise = 0;   // (forge round 2) retired; the silhouette
      L.forgeShape = packShape(alloyShape(C, n - 1));
      L.alloyRank = (uint8_t)std::min(n, 3);
    } else if (kv.k == "alloyShape") {
      // (forge round 2) alloyShape=<people>:<seed>:<n>: that culture's alloy n, its key and details on the body and the helm
      const std::vector<std::string> f = split(kv.v, ':');
      cult::Archetype a = cult::Archetype::Heartland;
      if (f.size() < 3 || !peopleOf(f[0], a)) { addBad(kv.k); continue; }
      const cult::Culture& C = kitCulture(a, (uint32_t)std::strtoul(f[1].c_str(), nullptr, 10));
      const int n = (int)std::strtol(f[2].c_str(), nullptr, 10);
      if (n < 1 || n > (int)C.arms.alloys.size()) { addBad(kv.k); continue; }
      const uint64_t pk = packForge(alloyStyle(C, n - 1));
      L.forge[H::TIER_BODY] = pk;
      if (L.helmet || L.helmStyle || L.helmForm) L.forge[H::TIER_HELM] = pk;
      L.forgeGuise = 0;
      L.forgeKit = 0;
      L.forgeShape = packShape(alloyShape(C, n - 1));
      L.alloyRank = (uint8_t)std::min(n, 3);
    } else if (kv.k == "shape") {
      // (forge round 2) shape=helm:<v>+skirt:<v>+mass:<v>+layer:<v>+arm:<v>+motif:<v>: forces values on the silhouette (`none`
      // is the value none where the axis has one, else clears; `unset` clears)
      uint8_t sv[FG::SX_COUNT];
      FG::unpackShapeAxes(L.forgeShape, sv);
      bool ok = true;
      for (const std::string& av : split(kv.v, '+')) {
        const size_t c = av.find(':');
        const int a = c == std::string::npos ? -1 : FG::shapeAxisOf(av.substr(0, c).c_str());
        if (a < 0) { ok = false; continue; }
        const std::string w = av.substr(c + 1);
        int v = w == "unset" ? 0 : FG::shapeValueOf(a, w.c_str());
        if (v < 0 && w == "none") v = 0;
        if (v < 0) { ok = false; continue; }
        sv[a] = (uint8_t)v;
      }
      if (!ok) addBad(kv.k);
      bool any = false;
      for (uint8_t x : sv) any |= x != 0;
      L.forgeShape = any ? FG::packShapeAxes(sv) : 0u;
    } else if (kv.k == "forgeShape") {
      L.forgeShape = (uint32_t)std::strtoul(kv.v.c_str(), nullptr, 0);
    } else {
      static const char* const kSlot[8] = {"forgeBody", "forgeHelm", "forgeShield", "forgeWeapon", "forgeGloves", "forgeBoots", "forgeCloak", "forgeBack"};
      bool done = false;
      for (int sl = 0; sl < 8; sl++)
        if (kv.k == kSlot[sl]) { L.forge[sl] = std::strtoull(kv.v.c_str(), nullptr, 0); done = true; }
      if (!done) addBad(kv.k);
    }
  }
}

// (forge round 2) a culture alloy on its people's kit: the boards (alloys1x, world:<w>, twins) and C18 compose every alloy so
art::HumanLook alloyKitLook(const art::HumanLook& base, const cult::Culture& c, int alloyIndex, int tier) {
  art::HumanLook L = base;
  if (alloyIndex < 0 || alloyIndex >= (int)c.arms.alloys.size()) return L;
  const int t = tier > 0 ? tier : (alloyIndex >= 2 ? 5 : 4);
  const bool kit = (c.id >> 48) == 0x1ADDull;
  LadderKit K = ladderKit(c.archetype, t, base.body, "", -1, kit ? c.seed : 1u);
  K.culture = c;
  for (Item* it : {&K.weapon, &K.bow, &K.staff, &K.armor, &K.helmet, &K.shield, &K.gloves, &K.boots, &K.cloak}) {
    it->culture = c.id;
    if (it->mat == Mat::Alloy || it->mat == Mat::Steel || it->mat == Mat::Iron || it->mat == Mat::Bronze) { it->mat = Mat::Alloy; it->alloy = (uint8_t)(alloyIndex + 1); }
  }
  L.outfit = art::Outfit::Tunic;
  L.weapon = 0; L.shield = false; L.backItem = 0;
  wearGear(L, K.worn(), K.lookup());
  kitTrim(L, K);
  applyKitDials(L, K, t);
  return L;
}

void installLadderMacros() { art::doll::setLookSpecHook(&applyLadderSpec); }
namespace {
struct AutoInstall { AutoInstall() { installLadderMacros(); } } g_autoInstall;
}

std::string describeLook(const art::HumanLook& L) {
  const art::HumanLook D;
  std::string out;
  char b[64];
  auto add = [&](const char* k, long v) { std::snprintf(b, sizeof b, "%s%s=%ld", out.empty() ? "" : ",", k, v); out += b; };
  auto addc = [&](const char* k, uint32_t c) {
    std::snprintf(b, sizeof b, "%s%s=#%02X%02X%02X", out.empty() ? "" : ",", k, c & 255u, (c >> 8) & 255u, (c >> 16) & 255u);
    out += b;
  };
#define N_(f) if (L.f != D.f) add(#f, (long)L.f);
#define C_(f) if (L.f != D.f) { if (L.f) addc(#f, L.f); else add(#f, 0); }
  N_(body) N_(age) N_(build) N_(people)
  if (L.outfit != D.outfit) add("outfit", (long)L.outfit);
  N_(garb) N_(cut) N_(weapon) N_(civil) N_(pattern) N_(cloak)
  if (L.cape != D.cape) add("cape", L.cape);
  if (L.hair != D.hair) add("hair", (long)L.hair);
  N_(hairVar)
  if (L.beard != D.beard) add("beard", L.beard);
  N_(beardStyle) N_(headwear)
  if (L.hood != D.hood) add("hood", L.hood);
  N_(neck) N_(jewellery)
  if (L.amulet != D.amulet) add("amulet", L.amulet);
  if (L.ring != D.ring) add("ring", L.ring);
  N_(head) N_(nose) N_(brow) N_(marks) N_(facePaint)
  if (L.helmet != D.helmet) add("helmet", L.helmet);
  N_(helmStyle) N_(helmForm) N_(armorStyle) N_(bodyForm)
  if (L.shield != D.shield) add("shield", L.shield);
  N_(shieldStyle) N_(shieldForm) N_(bladeForm) N_(polearmForm) N_(bowForm) N_(pauldron) N_(skirt) N_(forgeKit) N_(alloyRank) N_(forgeGuise) N_(crest) N_(armsOrnament) N_(gloves)
  N_(boots) N_(backItem) N_(sheen) N_(glow)
  C_(skin) C_(hairColor) C_(topColor) C_(bottomColor) C_(tabardColor) C_(cloakColor) C_(headColor) C_(patternColor) C_(eyeColor)
  C_(plumeColor) C_(armourTint) C_(armourTint2) C_(glowColor) C_(shieldField) C_(shieldDevice) C_(trimColor) C_(weaponColor)
  static const char* const kSlot[8] = {"tierBody", "tierHelm", "tierShield", "tierWeapon", "tierGloves", "tierBoots", "tierCloak", "tierBack"};
  for (int s = 0; s < 8; s++) if (L.tier[s]) add(kSlot[s], L.tier[s]);
  N_(rare) N_(mythic) N_(metalKind) N_(bladeKind) N_(motif) N_(kitVar) N_(floor) N_(alloyFam) C_(gemColor) N_(king)
  {   // (P2c forge) the packed styles (0x.., a byte per axis)
    static const char* const kFg[8] = {"forgeBody", "forgeHelm", "forgeShield", "forgeWeapon", "forgeGloves", "forgeBoots", "forgeCloak", "forgeBack"};
    for (int s = 0; s < 8; s++)
      if (L.forge[s]) { std::snprintf(b, sizeof b, "%s%s=0x%016llX", out.empty() ? "" : ",", kFg[s], (unsigned long long)L.forge[s]); out += b; }
    if (L.forgeShape) { std::snprintf(b, sizeof b, "%sforgeShape=0x%08X", out.empty() ? "" : ",", (unsigned)L.forgeShape); out += b; }
  }
#undef N_
#undef C_
  return out.empty() ? "-" : out;
}

}  // namespace gear
