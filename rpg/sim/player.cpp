// EMBERVALE player: derived stats and the look that follows the chosen appearance and the equipped gear.
// (M0 hero lane: visible equipment for every slot; armour/weapon looks from the items.)
#include <algorithm>
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/world/source.h"

namespace {
// an armour piece's material band (art::HumanLook): 1 leather, 2 iron, 3 steel, 4 gilded, 5 jade, 6 obsidian, 7 ember
uint8_t bandOf(const Item& it) {
  if (it.name.rfind("LEATHER", 0) == 0) return 1;
  return (uint8_t)std::clamp(it.tier + 2, 2, 7);
}
// shield shape by tier: iron round, steel heater, gilded kite, jade heater, obsidian tower, ember kite
uint8_t shieldShapeOf(const Item& it) {
  static const uint8_t s[6] = {1, 2, 3, 2, 4, 3};
  return s[std::clamp((int)it.tier, 0, 5)];
}
// cloak style: mantles are fur-collared and short; the finer cloaks (tier 3+) get gilt trim
uint8_t cloakStyleOf(const Item& it) {
  if (it.name.find("MANTLE") != std::string::npos) return 2;
  return it.tier >= 3 ? 3 : 1;
}
}  // namespace

// The chosen appearance in the shirt and trousers it starts with (the creator's preview uses it too). M3: the people
// (ears, proportions), the personal arms' field on every shield, and the homeland's dress cut and pattern on the shirt.
art::HumanLook appearanceLook(const Appearance& app, const cult::Culture* home) {
  art::HumanLook L;
  L.skin = app.skin;
  L.hairColor = app.hairColor;
  L.hair = (art::Hair)(app.hair < (uint8_t)art::Hair::COUNT ? app.hair : 1);
  L.beard = app.beard;
  L.eyeColor = app.eyeColor;
  L.build = app.build;
  L.topColor = app.topColor;
  L.bottomColor = app.bottomColor;
  L.tabardColor = rgba(170, 50, 40);   // the hero's heraldry: the painted field of every shield
  if (!app.heraldry.empty()) L.tabardColor = app.heraldry.field | 0xFF000000u;   // M3: personal arms chosen in the creator
  L.people = (uint8_t)std::min<int>(app.people, 2);   // M3: human, half-breed or elf (ears, proportions)
  L.outfit = art::Outfit::Tunic;
  if (home) {
    const cult::Cut c = app.female ? home->dress.cutF : home->dress.cutM;
    // a poncho or a robe still reads as "the shirt on their back"; every cut is cloth (armour hides it)
    L.cut = (uint8_t)((int)c + 1);
    if (home->dress.pattern != cult::Pattern::Plain) {
      L.pattern = (uint8_t)home->dress.pattern;
      L.patternColor = home->dress.trim ? (home->dress.trim | 0xFF000000u) : 0;
    }
  }
  return L;
}

void Game::recalcPlayer() {
  Actor& p = pl();
  float bonusHp = 0, bonusMp = 0, bonusSt = 0;
  for (int idx : worn()) {
    if (idx < 0 || idx >= (int)inv.size()) continue;
    const Item& it = inv[idx];
    if (it.ench == Ench::Health) bonusHp += it.enchPow;
    if (it.ench == Ench::Magicka) bonusMp += it.enchPow;
    if (it.ench == Ench::Stamina) bonusSt += it.enchPow;
  }
  float oldMax = p.maxHp;
  p.maxHp = baseHp + bonusHp;
  if (oldMax > 0 && p.hp > p.maxHp) p.hp = p.maxHp;
  (void)bonusMp; (void)bonusSt;
  p.armor = armorRating();
  p.radius = 4.5f;
  p.speed = 74;

  // ---- the look: the chosen appearance (Appearance defaults = the pre-M0 hero) dressed in the equipped gear.
  // Every field is set here, so the look never keeps a piece that was taken off.
  auto valid = [&](int idx) { return idx >= 0 && idx < (int)inv.size(); };
  const cult::Culture* home = app.homeland && world.src ? &world.src->culture(app.homeland) : nullptr;
  art::HumanLook L = appearanceLook(app, home);
  if (valid(eqArmor)) L.armorStyle = bandOf(inv[eqArmor]);
  if (valid(eqHelmet)) L.helmStyle = bandOf(inv[eqHelmet]);
  if (valid(eqGloves)) L.gloves = bandOf(inv[eqGloves]);
  if (valid(eqBoots)) L.boots = bandOf(inv[eqBoots]);
  if (valid(eqCloak)) {
    L.cloak = cloakStyleOf(inv[eqCloak]);
    L.cloakColor = inv[eqCloak].tint ? inv[eqCloak].tint : rgba(112, 82, 58);
  }
  if (valid(eqShield)) {
    L.shieldStyle = shieldShapeOf(inv[eqShield]);
    L.trimColor = inv[eqShield].tint;
  }
  L.amulet = valid(eqAmulet);
  L.ring = valid(eqRing);
  // in hand: the melee weapon, else the staff, else the bow; whatever else is equipped rides on the back
  auto tintOf = [&](int idx) { return inv[idx].tint ? inv[idx].tint : rgba(200, 205, 215); };
  if (valid(eqWeapon)) {
    static const uint8_t wmap[] = {1, 2, 6, 5, 1};   // sword, axe, mace (hammer head), dagger, greatsword
    L.weapon = wmap[inv[eqWeapon].sub % 5];
    L.weaponColor = tintOf(eqWeapon);
  } else if (valid(eqStaff)) {
    L.weapon = 4;
    L.weaponColor = tintOf(eqStaff);
  } else if (valid(eqBow)) {
    L.weapon = 3;
    L.weaponColor = tintOf(eqBow);
  }
  if (valid(eqBow) && L.weapon != 3) L.backItem |= 1;
  if (valid(eqStaff) && L.weapon != 4) L.backItem |= 2;
  p.look = L;
}

float Game::armorRating() const {
  float a = 0;
  for (int idx : {eqArmor, eqHelmet, eqShield, eqGloves, eqBoots, eqCloak}) if (idx >= 0 && idx < (int)inv.size()) a += inv[idx].power;
  for (int idx : worn())
    if (idx >= 0 && idx < (int)inv.size() && inv[idx].ench == Ench::Fortify) a += inv[idx].enchPow * 0.5f;
  if (blessT > 0) a += 15;
  return a;
}
float Game::weaponDamage() const {
  float d = eqWeapon >= 0 ? inv[eqWeapon].power : 4;
  return d * (1.0f + (plLevel - 1) * 0.04f);
}
