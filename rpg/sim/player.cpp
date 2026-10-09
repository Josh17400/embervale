// EMBERVALE player: derived stats and the look that follows the chosen appearance and the equipped gear.
// (M0 hero lane: visible equipment for every slot; armour/weapon looks from the items.)
#include <algorithm>
#include <initializer_list>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/gear_look.h"
#include "rpg/world/source.h"

// (M6 phase A) the look's mapping (appearanceLook, the worn gear's silhouettes and metals) moved to rpg/sim/gear_look.cpp
// (ARMS lane); the numbers stay here (NUMBERS lane).

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
  // (M6 NUMBERS) the worn and wielded gear summed: affixes synced to the level, unique powers, legendaries attuned
  {
    std::vector<const Item*> w;
    for (int idx : {eqWeapon, eqBow, eqStaff, eqArmor, eqHelmet, eqShield, eqRing, eqAmulet, eqGloves, eqBoots, eqCloak})
      if (idx >= 0 && idx < (int)inv.size()) w.push_back(&inv[(size_t)idx]);
    craft.live.stats = gear::sumGear(w, plLevel);
  }
  const gear::GearStats& S = craft.live.stats;
  bonusHp += (float)S.get(Affix::Health);
  float oldMax = p.maxHp;
  // (M6, 7.4) +4 HP a level on top of the level-up choice
  p.maxHp = baseHp + gear::levelHp(plLevel) + bonusHp;
  if (oldMax > 0 && p.hp > p.maxHp) p.hp = p.maxHp;
  (void)bonusMp; (void)bonusSt;
  p.armor = armorRating();
  p.radius = 4.5f;
  // (M6) move speed: the affix (+5 % a piece) and Windwalker (+12 %)
  p.speed = 74.0f * (1.0f + 0.01f * (float)S.get(Affix::MoveSpeed) + (S.has(Unique::Windwalker) ? 0.12f : 0.0f));

  // ---- the look: the chosen appearance (Appearance defaults = the pre-M0 hero) dressed in the equipped gear.
  // Every field is set here, so the look never keeps a piece that was taken off. (M6: rpg/sim/gear_look.cpp)
  auto valid = [&](int idx) { return idx >= 0 && idx < (int)inv.size(); };
  auto at = [&](int idx) -> const Item* { return valid(idx) ? &inv[(size_t)idx] : nullptr; };
  const cult::Culture* home = app.homeland && world.src ? &world.src->culture(app.homeland) : nullptr;
  art::HumanLook L = appearanceLook(app, home);
  WornGear wg;
  wg.weapon = at(eqWeapon); wg.bow = at(eqBow); wg.staff = at(eqStaff); wg.armor = at(eqArmor); wg.helmet = at(eqHelmet);
  wg.shield = at(eqShield); wg.ring = at(eqRing); wg.amulet = at(eqAmulet); wg.gloves = at(eqGloves); wg.boots = at(eqBoots);
  wg.cloak = at(eqCloak);
  wearGear(L, wg, [this](uint64_t id) -> const cult::Culture* { return id && world.src ? &world.src->culture(id) : nullptr; });
  p.look = L;
}

// (M6, 7.2) the armour rating: every worn piece's power synced to the player's level (level sync), the classic
// Fortify enchantment, the blessing; Ironhide +25 %. gear::mitigation turns it into the share of a blow it stops.
float Game::armorRating() const {
  float a = 0;
  for (int idx : {eqArmor, eqHelmet, eqShield, eqGloves, eqBoots, eqCloak})
    if (idx >= 0 && idx < (int)inv.size()) a += gear::usePower(inv[(size_t)idx], plLevel);
  for (int idx : worn())
    if (idx >= 0 && idx < (int)inv.size() && inv[idx].ench == Ench::Fortify) a += inv[idx].enchPow * 0.5f;
  if (blessT > 0) a += 15;
  if (craft.live.stats.has(Unique::Ironhide)) a *= 1.25f;
  return a;
}
// (M6, 7.2 / 7.4) the weapon's power at eff (level sync) x +1.5 % a level; bare fists 4
float Game::weaponDamage() const {
  const float d = eqWeapon >= 0 && eqWeapon < (int)inv.size() ? gear::usePower(inv[(size_t)eqWeapon], plLevel) : 4.0f;
  return d * gear::playerDamageMul(plLevel);
}
