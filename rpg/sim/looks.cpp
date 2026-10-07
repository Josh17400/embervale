// Townsfolk, guards, bandits and wayside people: how they look and what they are called (moved out of game.cpp in M3
// phase A so the PEOPLE lane owns it). makeLook's random draws (rr) are a contract: quests.cpp reproduces the first
// one (personFemale) to know a quest person's sex before the actor exists, so keep the draw order of the shared part;
// culture work (M3) draws from its own hash streams, never from rr (it only reads rr's state on entry, which every
// caller already makes a pure function of the person: site, building and slot, or a quest's person seed).
#include <algorithm>
#include <cstdint>
#include <string>
#include "rpg/sim/game.h"
#include "rpg/culture/culture.h"
#include "rpg/culture/society.h"
#include "rpg/sim/game_internal.h"

// ------------------------------------------------------------------ the census look (M3, VISION_PLAN 5.6 "NPCs")
// Dresses a person in a culture: people from its mix, skin and hair from its ranges, the dress cut per sex, headwear,
// the cloth palette and pattern, jewellery and face paint, a name in its phonology. Guards wear their OWNER kingdom's
// arms (so occupation shows) in the kingdom's colours; bandits a poor local version; priests the faith's colours; the
// king and the jarls the culture's regalia. Pure function of its arguments (the galleries and tests call it too).
namespace census {

struct Rank { bool royal = false, capital = false; };

namespace {
struct Stream {   // a private hash stream (never the caller's Rng)
  uint64_t s;
  explicit Stream(uint64_t seed) : s(seed ^ 0xC3A5C85C97CB3127ull) {}
  uint32_t next() {
    s += 0x9E3779B97F4A7C15ull;
    uint64_t z = s;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return (uint32_t)((z ^ (z >> 31)) >> 32);
  }
  int pick(int n) { return n <= 1 ? 0 : (int)(next() % (uint32_t)n); }
  bool chance(int p256) { return (int)(next() & 255u) < p256; }
};
uint32_t darker(uint32_t c, float k) {
  return rgba((int)((c & 255) * k), (int)(((c >> 8) & 255) * k), (int)(((c >> 16) & 255) * k));
}
// the material band (art::HumanLook: 2 iron, 3 steel, 4 gilded/bronze, 5 jade, 6 obsidian) a culture's metal reads as
uint8_t bandOf(uint32_t metal, int rank) {
  const int r = metal & 255, g = (metal >> 8) & 255, b = (metal >> 16) & 255;
  const bool warm = metal && r > b + 46 && g > b + 14;          // bronze, brass, gold
  const bool green = metal && g > r + 18 && g > b + 6;          // jade-lacquered
  const bool dark = metal && r + g + b < 200;                   // blackened
  if (rank >= 2) return warm ? 4 : (green ? 5 : (dark ? 6 : 3));
  if (rank == 1) return warm ? 4 : (green ? 5 : 3);
  return warm && r > 170 ? 4 : 2;
}
art::Hair hairFor(Stream& h, uint16_t bits, bool female, art::Hair fallback) {
  static const art::Hair fem[] = {art::Hair::Long, art::Hair::Braids, art::Hair::Ponytail, art::Hair::Bun, art::Hair::Curls};
  static const art::Hair male[] = {art::Hair::Short, art::Hair::Bald, art::Hair::Long, art::Hair::Mohawk, art::Hair::Curls,
                                   art::Hair::Ponytail, art::Hair::Braids};
  art::Hair opts[8];
  int n = 0;
  if (female) { for (art::Hair x : fem) if (bits & (1u << (int)x)) opts[n++] = x; }
  else for (art::Hair x : male) if (bits & (1u << (int)x)) opts[n++] = x;
  if (!n) return fallback;
  // the first options are the more common
  const int i = std::min(h.pick(n), h.pick(n));
  return opts[i];
}
}  // namespace

void dress(art::HumanLook& L, std::string& name, Role r, bool female, const cult::Culture& C, const cult::Culture& owner,
           uint64_t seed, const Rank& rank) {
  Stream h(seed);
  const cult::DressStyle& D = C.dress;
  // ---- the people (weights from the culture's mix)
  {
    const int w0 = C.peopleMix[0], w1 = C.peopleMix[1], w2 = C.peopleMix[2], tot = w0 + w1 + w2;
    const int x = tot > 0 ? (int)(h.next() % (uint32_t)tot) : 0;
    L.people = (uint8_t)(x < w0 ? 0 : (x < w0 + w1 ? 1 : 2));
  }
  // ---- skin and hair from the culture's ranges (elves in a human land keep to the paler half)
  {
    int lo = std::clamp((int)D.skinLo, 0, cult::kSkinSteps - 1), hi = std::clamp((int)D.skinHi, lo, cult::kSkinSteps - 1);
    if (L.people == 2 && C.archetype != cult::Archetype::Sylvan && C.archetype != cult::Archetype::Starspire) hi = lo + (hi - lo) / 2;
    L.skin = cult::skinRamp(lo + h.pick(hi - lo + 1)) | 0xFF000000u;
    int nh = 0;
    for (uint32_t hc : D.hairCols) nh += hc != 0;
    if (nh) {
      const int i = std::min(h.pick(nh), h.pick(4));   // the first colours are seen most
      L.hairColor = D.hairCols[std::min(i, nh - 1)] | 0xFF000000u;
    }
    L.hair = hairFor(h, D.hairStyles, female, L.hair);
    L.beard = !female && L.people != 2 && h.chance(D.beardP);
  }
  // ---- cloth: the culture's palette and cut (role colours of the M0/M2 trades stay, so trades stay readable)
  int nc = 0;
  for (uint32_t cc : D.cloth) nc += cc != 0;
  const auto cloth = [&](int i) { return nc ? (D.cloth[(size_t)(i % nc)] | 0xFF000000u) : L.topColor; };
  const bool clothRole = r == Role::Villager || r == Role::Merchant || r == Role::Innkeeper || r == Role::Farmer || r == Role::Child ||
                         r == Role::Fisher || r == Role::Traveller;
  if (clothRole) {
    if (r == Role::Villager || r == Role::Child) {
      const int a = h.pick(std::max(1, nc));
      L.topColor = cloth(a);
      L.bottomColor = darker(cloth(a + 1 + h.pick(std::max(1, nc - 1))), 0.72f);
    } else {
      L.bottomColor = darker(cloth(h.pick(std::max(1, nc))), 0.72f);
    }
    L.outfit = art::Outfit::Tunic;
    L.cut = (uint8_t)((int)(female ? D.cutF : D.cutM) + 1);
    if (D.pattern != cult::Pattern::Plain && h.chance(190)) {
      L.pattern = (uint8_t)D.pattern;
      L.patternColor = D.trim ? (D.trim | 0xFF000000u) : 0;
    }
  }
  // headwear (helmets and the legacy hood win in the painter)
  if (!L.hood && h.chance(D.headP)) {
    const int roll = h.pick(10);
    const cult::Headwear w = D.head[roll < 5 ? 0 : (roll < 8 ? 1 : 2)];
    if (w != cult::Headwear::None) {
      L.headwear = (uint8_t)w;
      static const uint32_t furs[4] = {rgba(118, 88, 64), rgba(150, 140, 128), rgba(84, 64, 54), rgba(200, 190, 172)};
      L.headColor = w == cult::Headwear::FurHat ? furs[h.pick(4)] : (w == cult::Headwear::Conical ? 0 : cloth(h.pick(std::max(1, nc)) + 2));
    }
  }
  if (h.chance(D.jewellery)) L.jewellery = (uint8_t)(1 + h.pick(3));
  if (D.facePaint && h.chance(female ? 70 : 130)) L.facePaint = D.facePaint;

  // ---- roles
  const cult::ArmsStyle& A = owner.arms;   // guards serve the owner
  const cult::ArmsStyle& Al = C.arms;      // bandits are local
  auto applyArms = [&](const cult::ArmsStyle& S, int band, int bodyI, int helmI) {
    L.armorStyle = (uint8_t)band;
    L.helmStyle = (uint8_t)band;
    L.helmet = true;
    L.bodyForm = (uint8_t)((int)S.body[bodyI] + 1);
    L.helmForm = (uint8_t)((int)S.helm[helmI] + 1);
    L.armsOrnament = S.ornament;
    L.pauldron = (uint8_t)(S.pauldron + 1);
    L.skirt = (uint8_t)(S.skirt + 1);
    L.crest = (uint8_t)(S.crest + 1);
    L.headwear = 0; L.cut = 0; L.pattern = 0; L.patternColor = 0;
  };
  switch (r) {
    case Role::Guard: {
      const int rk = rank.royal ? 2 : (rank.capital ? 1 : 0);
      applyArms(A, bandOf(A.metal, rk), 0, 0);
      L.shieldForm = (uint8_t)((int)A.shield + 1);
      L.bladeForm = (uint8_t)((int)A.blade + 1);
      L.weapon = 1;
      L.plumeColor = L.trimColor ? L.trimColor : (A.plume ? (A.plume | 0xFF000000u) : 0);
      L.gloves = L.boots = (uint8_t)std::min<int>(L.armorStyle, 3);
      if (rk >= 1) { L.pauldron = (uint8_t)std::min(4, L.pauldron + 1); L.crest = (uint8_t)std::min(4, L.crest + 1); }
      if (rk == 2) L.armsOrnament |= cult::ARM_GILDING;
      if (!L.cloak) {   // the culture's cape in the kingdom's colours
        if (A.cape == 1) L.cape = true;
        else if (A.cape == 2) { L.cloak = 1; L.cloakColor = L.tabardColor; }
      }
      L.beard = false;
      break;
    }
    case Role::Bandit: {
      // a poor local version: leather or padding, a leather helm of the local form (or the local hood), local blades
      L.armorStyle = (uint8_t)(h.chance(150) ? 1 : 0);
      if (L.outfit == art::Outfit::Leather || L.armorStyle) L.bodyForm = (uint8_t)((int)Al.body[1] + 1);
      if (L.bodyForm > (int)cult::BodyArm::Leather + 1) L.bodyForm = (uint8_t)((int)cult::BodyArm::Leather + 1);
      if (!L.hood && h.chance(110)) { L.helmStyle = 1; L.helmForm = (uint8_t)((int)Al.helm[1] + 1); L.headwear = 0; }
      if (L.weapon == 1) L.bladeForm = (uint8_t)((int)Al.blade + 1);
      if (h.chance(70) && Al.shield != cult::ShieldForm::None) { L.shieldForm = (uint8_t)((int)cult::ShieldForm::Buckler + 1); }
      L.armsOrnament = (uint16_t)(Al.ornament & (cult::ARM_FUR_TRIM | cult::ARM_STUDS | cult::ARM_TASSELS));
      L.cut = 0; L.pattern = 0;
      break;
    }
    case Role::Priest: {
      if (C.faith.colour) L.topColor = C.faith.colour | 0xFF000000u;
      if (C.faith.colour2) L.tabardColor = C.faith.colour2 | 0xFF000000u;
      const cult::Cut c0 = female ? D.cutF : D.cutM;
      L.cut = (uint8_t)((c0 == cult::Cut::Gown || c0 == cult::Cut::Kaftan ? (int)c0 : (int)cult::Cut::Robe) + 1);
      L.pattern = (uint8_t)cult::Pattern::BorderTrim;
      L.patternColor = L.tabardColor;
      if (L.headwear == (uint8_t)cult::Headwear::FurHat || L.headwear == (uint8_t)cult::Headwear::Cap) L.headwear = (uint8_t)cult::Headwear::Hood;
      break;
    }
    case Role::Mage: case Role::Herbalist: {
      const cult::Cut c0 = female ? D.cutF : D.cutM;
      L.cut = (uint8_t)((c0 == cult::Cut::Gown || c0 == cult::Cut::Kaftan ? (int)c0 : (int)cult::Cut::Robe) + 1);
      L.pattern = D.pattern != cult::Pattern::Plain ? (uint8_t)cult::Pattern::BorderTrim : 0;
      L.patternColor = L.tabardColor;
      break;
    }
    case Role::Jarl: {
      // the culture's own steel, bareheaded, with its shoulders, skirt and ornament
      L.armorStyle = 3;
      L.bodyForm = (uint8_t)((int)C.arms.body[0] + 1);
      L.armsOrnament = (uint16_t)(C.arms.ornament | cult::ARM_GILDING);
      L.pauldron = (uint8_t)std::min(4, C.arms.pauldron + 2);
      L.skirt = (uint8_t)(C.arms.skirt + 1);
      L.headwear = 0; L.cut = 0; L.pattern = 0;
      if (h.chance(140)) L.jewellery |= 2;
      break;
    }
    case Role::King: {
      // regalia: the culture's finest helm and body form, gilded, in the noble cloak (set by makeLook)
      L.armorStyle = bandOf(C.arms.metal, 2);   // the culture's finest metal; the crown-helm is always gilded
      L.helmStyle = L.armorStyle == 5 ? 5 : 4;
      L.bodyForm = (uint8_t)((int)C.arms.body[0] + 1);
      L.helmForm = (uint8_t)((int)C.arms.helm[0] + 1);
      L.armsOrnament = (uint16_t)(C.arms.ornament | cult::ARM_GILDING | cult::ARM_FILIGREE);
      L.pauldron = (uint8_t)std::min(4, C.arms.pauldron + 2);
      L.skirt = (uint8_t)(C.arms.skirt + 1);
      L.crest = (uint8_t)std::min(4, C.arms.crest + 2);
      L.plumeColor = L.tabardColor;
      L.bladeForm = (uint8_t)((int)C.arms.blade + 1);
      L.headwear = 0; L.cut = 0; L.pattern = 0;
      break;
    }
    case Role::Smith: case Role::Hunter: L.cut = 0; L.pattern = 0; break;
    default: break;
  }
  // ---- the name, in the culture's phonology (titles kept; guards keep theirs: the spawner names them)
  if (r != Role::Guard) {
    Stream hn(seed ^ 0x6E616D65ull);   // its own stream: every bit of the person seed reaches the name
    std::string nm = cult::personName(C, hn.next(), female);
    if (!nm.empty()) {
      if (r == Role::Jarl) nm = std::string(cult::societyOf(owner).lordTitle) + " " + nm;   // (M3b fixer) the society's lord: NOYAN, EMIR, PROVOST...
      else if (r == Role::Priest) nm = "PRIEST " + nm;
      else if (r == Role::King) nm = std::string(cult::societyOf(owner).rulerTitle) + " " + nm;   // (fix) the society's own title: KHAN, HIGH JARL, DOGE...
      name = nm;
    }
  }
}

}  // namespace census

void Game::makeLook(Actor& a, Role r, Rng& rr) {
  const uint64_t entry = rr.s;   // the person's seed (read, never drawn): the culture stream starts from it
  art::HumanLook& L = a.look;
  static const uint32_t skins[] = {rgba(244, 204, 168), rgba(232, 180, 140), rgba(200, 146, 104), rgba(150, 100, 70), rgba(110, 72, 50)};
  static const uint32_t hairs[] = {rgba(40, 30, 24), rgba(90, 56, 30), rgba(150, 96, 50), rgba(210, 170, 90), rgba(180, 70, 40), rgba(200, 200, 200), rgba(240, 220, 150)};
  static const uint32_t cloth[] = {rgba(70, 110, 150), rgba(150, 60, 50), rgba(80, 120, 70), rgba(140, 110, 60), rgba(110, 80, 130), rgba(160, 140, 110), rgba(60, 70, 90), rgba(170, 120, 60)};
  bool female = rr.f() < 0.45f;
  L.skin = skins[rr.irange(5)];
  L.hairColor = hairs[rr.irange(7)];
  L.topColor = cloth[rr.irange(8)];
  L.bottomColor = cloth[rr.irange(8)];
  L.bottomColor = rgba((int)((L.bottomColor & 255) * 0.7f), (int)(((L.bottomColor >> 8) & 255) * 0.7f), (int)(((L.bottomColor >> 16) & 255) * 0.7f));
  L.hair = female ? (rr.f() < 0.5f ? art::Hair::Long : (rr.f() < 0.5f ? art::Hair::Braids : art::Hair::Ponytail))
                  : (rr.f() < 0.15f ? art::Hair::Bald : (rr.f() < 0.15f ? art::Hair::Mohawk : art::Hair::Short));
  L.beard = !female && rr.f() < 0.45f;
  L.outfit = female && rr.f() < 0.6f ? art::Outfit::Dress : art::Outfit::Tunic;
  L.weapon = 0;
  a.name = makePersonName(rr, female);
  switch (r) {
    case Role::Guard: L.outfit = art::Outfit::Guard; L.helmet = true; L.shield = true; L.weapon = 1; L.tabardColor = rgba(150, 40, 40); L.beard = false; break;
    case Role::Jarl: L.outfit = art::Outfit::Plate; L.cape = true; L.tabardColor = rgba(130, 30, 40); a.name = "JARL " + a.name; break;
    case Role::Priest: L.outfit = art::Outfit::Robe; L.topColor = rgba(232, 234, 244); L.tabardColor = rgba(150, 34, 48); a.name = "PRIEST " + a.name; break;   // white + crimson: never skin-toned
    case Role::Mage: L.outfit = art::Outfit::Robe; L.topColor = rgba(70, 60, 140); L.tabardColor = rgba(200, 180, 80); L.hood = rr.f() < 0.5f; L.weapon = 4; break;
    case Role::Smith: L.outfit = art::Outfit::Leather; L.weapon = 6; L.beard = !female; break;
    case Role::Innkeeper: L.outfit = female ? art::Outfit::Dress : art::Outfit::Tunic; L.topColor = rgba(170, 140, 100); break;
    case Role::Merchant: L.topColor = rgba(150, 70, 110); break;
    case Role::Farmer: L.topColor = rgba(140, 120, 70); L.weapon = 0; break;
    case Role::Bandit:
      L.outfit = rr.f() < 0.6f ? art::Outfit::Leather : art::Outfit::Rags; L.hood = rr.f() < 0.5f; L.tabardColor = rgba(80, 60, 50);
      L.weapon = rr.f() < 0.5f ? 1 : 2;
      break;
    case Role::King:
      // gilded plate under a long noble cloak in the kingdom's colours (below), a winged gilded helm, a sword
      L.outfit = art::Outfit::Plate; L.armorStyle = 4; L.helmStyle = 4; L.helmet = true; L.cloak = 3; L.beard = !female;
      L.weapon = 1; L.weaponColor = rgba(236, 206, 120); L.trimColor = rgba(236, 196, 92); L.amulet = true; L.ring = true;
      L.tabardColor = rgba(130, 30, 40); L.cloakColor = rgba(130, 30, 40);
      a.name = "KING " + a.name;
      break;
    // M2 wayside people (rpg/sim/wayside.cpp): colours only, after the shared draws, so every other look is unchanged
    case Role::Hunter:
      L.outfit = art::Outfit::Leather; L.topColor = rgba(96, 112, 64); L.bottomColor = rgba(84, 64, 44); L.weapon = 3;
      L.hood = (hash32((uint32_t)a.slot * 977u + 13u) & 1) != 0; L.tabardColor = rgba(120, 90, 54);
      break;
    case Role::Fisher: L.outfit = art::Outfit::Tunic; L.topColor = rgba(74, 104, 132); L.bottomColor = rgba(70, 64, 54); break;
    case Role::Herbalist: L.outfit = art::Outfit::Robe; L.topColor = rgba(92, 128, 76); L.tabardColor = rgba(200, 180, 110); L.hood = true; break;
    case Role::Traveller: L.cape = true; L.tabardColor = rgba(110, 84, 60); L.topColor = rgba(140, 120, 90); L.hood = true; break;
    default: break;
  }
  // M1 kingdom identity (VISION_PLAN 15.8): guards wear their kingdom's colours; the king's cloak and the royal guard
  // (the palace and its barracks) carry the royal banner. (Colours only: the look's random draws above are unchanged.)
  const int home = a.site >= 0 ? a.site : (a.bldg >= 0 && a.bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)a.bldg].site : -1);
  census::Rank rank;
  if (const Kingdom* K = world.kingdomOf(home)) {
    const art::Building in = a.bldg >= 0 && a.bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)a.bldg].type : art::Building::House;
    const bool royal = in == art::Building::Palace || in == art::Building::Barracks;
    if (r == Role::Guard) {
      L.tabardColor = K->color;
      if (royal) {   // the royal guard: steel plate and great-helm, a heater shield with the royal field and trim
        L.outfit = art::Outfit::Plate; L.armorStyle = 3; L.helmStyle = 3; L.shieldStyle = 2; L.trimColor = K->color2;
        L.cloak = 3; L.cloakColor = K->color;
        a.name = "ROYAL GUARD";
        rank.royal = true;
      } else if (world.sites[(size_t)home].capital) {
        L.shieldStyle = 2; L.trimColor = K->color2;   // a capital's watch carries the royal shield
        rank.capital = true;
      }
    }
    if (r == Role::King) { L.cloakColor = K->color; L.tabardColor = K->color; }
    if (r == Role::Jarl) L.tabardColor = K->color;
  }
  // M3: the culture they live in (their settlement's), and the culture of whoever rules it (guards wear the owner's arms)
  const cult::Culture* C = world.cultureOf(home);
  const int kh = home >= 0 && home < (int)world.sites.size() ? world.sites[(size_t)home].kingdom : -1;
  const cult::Culture* owner = world.cultureOfKingdom(kh);
  if (C) {
    const uint64_t s = entry ^ ((uint64_t)(uint32_t)(a.site + 1) << 40) ^ ((uint64_t)(uint32_t)(a.bldg + 1) << 20) ^ (uint64_t)(uint32_t)a.slot ^ C->id;
    census::dress(L, a.name, r, female, *C, owner ? *owner : *C, s, rank);
  }
}
