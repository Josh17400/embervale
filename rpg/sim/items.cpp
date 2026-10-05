#include "rpg/sim/items.h"
#include <algorithm>

using art::Icon;

const char* tierName(int t) {
  static const char* n[] = {"IRON", "STEEL", "GILDED", "JADE", "OBSIDIAN", "EMBERFORGED"};
  return n[std::clamp(t, 0, 5)];
}
uint32_t tierTint(int t) {
  static const uint32_t c[] = {rgba(150, 150, 158), rgba(200, 208, 220), rgba(222, 186, 92), rgba(120, 214, 140), rgba(92, 70, 120), rgba(200, 52, 44)};
  return c[std::clamp(t, 0, 5)];
}
uint32_t rarityColor(Rarity r) {
  static const uint32_t c[] = {rgba(230, 230, 230), rgba(110, 220, 110), rgba(90, 160, 255), rgba(200, 110, 255), rgba(255, 170, 40)};
  return c[(int)r];
}
const char* enchName(Ench e) {
  static const char* n[] = {"", "BURNING", "FROST", "DRAINING", "VITALITY", "THE MAGE", "ENDURANCE", "THE TITAN"};
  return n[(int)e];
}

namespace {
int tierFor(Rng& r, int level) {
  int t = level / 5 + (r.f() < 0.25f ? 1 : 0) - (r.f() < 0.25f ? 1 : 0);
  return std::clamp(t, 0, 5);
}
Rarity rollRarity(Rng& r, int level, bool boss) {
  float q = r.f() - (boss ? 0.35f : 0) - level * 0.004f;
  if (q < 0.03f) return Rarity::Legendary;
  if (q < 0.10f) return Rarity::Epic;
  if (q < 0.25f) return Rarity::Rare;
  if (q < 0.50f) return Rarity::Uncommon;
  return Rarity::Common;
}
}  // namespace

Item makeWeapon(Rng& r, int level, int forceType, bool allowEnch) {
  Item it;
  it.kind = ItemKind::Weapon;
  WeaponType wt = forceType >= 0 ? (WeaponType)forceType : (WeaponType)r.irange((int)WeaponType::COUNT);
  it.sub = (uint8_t)wt;
  it.tier = (uint8_t)tierFor(r, level);
  static const int base[] = {7, 8, 9, 5, 13};
  static const char* names[] = {"SWORD", "WAR AXE", "MACE", "DAGGER", "GREATSWORD"};
  static const Icon icons[] = {Icon::Sword, Icon::Axe, Icon::Mace, Icon::Dagger, Icon::Greatsword};
  it.power = (int16_t)(base[(int)wt] + it.tier * (wt == WeaponType::Greatsword ? 5 : 3) + level / 3);
  it.rarity = allowEnch ? rollRarity(r, level, false) : Rarity::Common;
  it.icon = icons[(int)wt];
  it.tint = tierTint(it.tier);
  it.name = std::string(tierName(it.tier)) + " " + names[(int)wt];
  if (it.rarity >= Rarity::Uncommon) {
    static const Ench we[] = {Ench::Fire, Ench::Frost, Ench::Drain};
    it.ench = we[r.irange(3)];
    it.enchPow = (int16_t)(3 + level / 2 + (int)it.rarity * 3);
    it.name += std::string(" OF ") + enchName(it.ench);
  }
  it.power = (int16_t)(it.power * (1.0f + (int)it.rarity * 0.08f));
  it.value = (it.power * 6 + it.enchPow * 10) * (1 + it.tier);
  return it;
}

Item makeBow(Rng& r, int level) {
  Item it;
  it.kind = ItemKind::Bow;
  it.tier = (uint8_t)tierFor(r, level);
  it.power = (int16_t)(8 + it.tier * 4 + level / 3);
  it.rarity = rollRarity(r, level, false);
  it.icon = Icon::Bow;
  it.tint = tierTint(it.tier);
  it.name = std::string(it.tier == 0 ? "HUNTING" : tierName(it.tier)) + " BOW";
  if (it.rarity >= Rarity::Rare) {
    it.ench = r.f() < 0.5f ? Ench::Fire : Ench::Frost;
    it.enchPow = (int16_t)(3 + level / 2 + (int)it.rarity * 2);
    it.name += std::string(" OF ") + enchName(it.ench);
  }
  it.value = (it.power * 7 + it.enchPow * 10) * (1 + it.tier);
  return it;
}

Item makeStaff(Rng& r, int level) {
  Item it;
  it.kind = ItemKind::Staff;
  it.tier = (uint8_t)tierFor(r, level);
  it.power = (int16_t)(12 + it.tier * 5 + level / 2);   // spell power
  it.rarity = (Rarity)std::max((int)Rarity::Uncommon, (int)rollRarity(r, level, false));
  it.icon = Icon::Staff;
  it.tint = r.f() < 0.5f ? rgba(255, 120, 40) : rgba(110, 200, 255);
  it.ench = it.tint == rgba(255, 120, 40) ? Ench::Fire : Ench::Frost;
  it.name = std::string("STAFF OF ") + (it.ench == Ench::Fire ? "FIREBOLTS" : "FROST LANCES");
  it.value = it.power * 25;
  return it;
}

Item makeArmor(Rng& r, int level, ItemKind slot) {
  Item it;
  it.kind = slot;
  it.tier = (uint8_t)tierFor(r, level);
  it.rarity = rollRarity(r, level, false);
  it.tint = tierTint(it.tier);
  // tier 0 pieces are either leather or iron; the choice for the M0 slots (and the cap) comes from the level, not
  // the rng, so the random stream of the pre-M0 kinds is unchanged
  const bool leather0 = it.tier == 0 && (level & 1);
  static const char* bodyN[] = {"LEATHER ARMOR", "STEEL PLATE", "GILDED MAIL", "JADE SCALE", "OBSIDIAN PLATE", "EMBERFORGED PLATE"};
  if (slot == ItemKind::Armor) {
    it.power = (int16_t)(8 + it.tier * 7 + level / 2);
    it.icon = Icon::Armor;
    it.name = it.tier == 0 && r.f() < 0.5f ? "IRON ARMOR" : bodyN[it.tier];
  } else if (slot == ItemKind::Helmet) {
    it.power = (int16_t)(3 + it.tier * 3 + level / 4);
    it.icon = Icon::Helmet;
    it.name = leather0 ? "LEATHER CAP" : std::string(tierName(it.tier)) + " HELMET";
  } else if (slot == ItemKind::Gloves) {
    it.power = (int16_t)(2 + it.tier * 2 + level / 5);
    it.icon = Icon::Gloves;
    it.name = it.tier == 0 ? (leather0 ? "LEATHER GLOVES" : "IRON GAUNTLETS") : std::string(tierName(it.tier)) + " GAUNTLETS";
  } else if (slot == ItemKind::Boots) {
    it.power = (int16_t)(2 + it.tier * 2 + level / 5);
    it.icon = Icon::Boots;
    it.name = it.tier == 0 ? (leather0 ? "LEATHER BOOTS" : "IRON BOOTS") : std::string(tierName(it.tier)) + " BOOTS";
  } else if (slot == ItemKind::Cloak) {
    // cloth, not metal: the tint is the cloth colour (the paper doll and the hero wear it)
    static const char* cloakN[] = {"TRAVEL CLOAK", "WOOL CLOAK", "FUR MANTLE", "RANGER CLOAK", "SHADOW CLOAK", "EMBER MANTLE"};
    static const uint32_t cloakC[] = {rgba(112, 82, 58), rgba(60, 96, 62), rgba(52, 70, 120), rgba(40, 92, 66), rgba(46, 38, 58), rgba(150, 40, 36)};
    it.power = (int16_t)(1 + it.tier + level / 6);
    it.icon = Icon::Cloak;
    it.name = cloakN[it.tier];
    it.tint = cloakC[it.tier];
  } else {
    it.power = (int16_t)(4 + it.tier * 3 + level / 4);
    it.icon = Icon::Shield;
    it.name = std::string(tierName(it.tier)) + " SHIELD";
  }
  if (it.name.rfind("LEATHER", 0) == 0) it.tint = rgba(150, 100, 62);
  if (it.rarity >= Rarity::Uncommon) {
    static const Ench ae[] = {Ench::Health, Ench::Magicka, Ench::Stamina, Ench::Fortify};
    it.ench = ae[r.irange(4)];
    it.enchPow = (int16_t)(8 + level + (int)it.rarity * 6);
    it.name += std::string(" OF ") + enchName(it.ench);
  }
  it.value = (it.power * 8 + it.enchPow * 8) * (1 + it.tier);
  return it;
}

// which armour slot a random drop fills (one rng draw)
ItemKind randomArmorSlot(Rng& r) {
  float s = r.f();
  if (s < 0.30f) return ItemKind::Armor;
  if (s < 0.46f) return ItemKind::Helmet;
  if (s < 0.60f) return ItemKind::Shield;
  if (s < 0.73f) return ItemKind::Gloves;
  if (s < 0.86f) return ItemKind::Boots;
  return ItemKind::Cloak;
}

Item makeJewel(Rng& r, int level) {
  Item it;
  bool ring = r.f() < 0.5f;
  it.kind = ring ? ItemKind::Ring : ItemKind::Amulet;
  it.icon = ring ? Icon::Ring : Icon::Amulet;
  it.rarity = (Rarity)std::max((int)Rarity::Uncommon, (int)rollRarity(r, level, false));
  static const Ench je[] = {Ench::Health, Ench::Magicka, Ench::Stamina, Ench::Fortify};
  it.ench = je[r.irange(4)];
  it.enchPow = (int16_t)(10 + level * 2 + (int)it.rarity * 6);
  it.tint = ring ? rgba(230, 200, 90) : rgba(190, 200, 220);
  static const char* metal[] = {"COPPER", "SILVER", "GOLD", "GOLD", "MOONSTONE", "DRAGONBONE"};
  it.name = std::string(metal[std::min(5, level / 5)]) + (ring ? " RING OF " : " AMULET OF ") + enchName(it.ench);
  it.value = it.enchPow * 18;
  return it;
}

Item makePotion(PotionType t, int size) {
  Item it;
  it.kind = ItemKind::Potion;
  it.sub = (uint8_t)t;
  it.tier = (uint8_t)size;
  static const int amt[] = {35, 70, 140};
  static const char* sz[] = {"MINOR ", "", "PLENTIFUL "};
  static const char* nm[] = {"HEALTH POTION", "MAGICKA POTION", "STAMINA POTION"};
  static const Icon ic[] = {Icon::PotionRed, Icon::PotionBlue, Icon::PotionGreen};
  it.power = (int16_t)amt[size];
  it.name = std::string(sz[size]) + nm[(int)t];
  it.icon = ic[(int)t];
  it.value = 15 + size * 25;
  return it;
}

Item makeFood(int which) {
  Item it;
  it.kind = ItemKind::Food;
  it.sub = (uint8_t)which;
  static const char* nm[] = {"BREAD", "VENISON", "APPLE", "CHEESE WHEEL"};
  static const Icon ic[] = {Icon::Bread, Icon::Meat, Icon::Apple, Icon::Cheese};
  static const int heal[] = {15, 30, 10, 20};
  which = std::clamp(which, 0, 3);
  it.name = nm[which]; it.icon = ic[which]; it.power = (int16_t)heal[which]; it.value = 3 + heal[which] / 3;
  return it;
}

Item makeArrows(int n) {
  Item it;
  it.kind = ItemKind::Arrows;
  it.name = "ARROWS";
  it.icon = Icon::Arrows;
  it.count = n;
  it.value = 1;
  return it;
}

Item makeMisc(int which) {
  Item it;
  it.kind = ItemKind::Misc;
  it.sub = (uint8_t)which;
  static const char* nm[] = {"WOLF PELT", "BONE MEAL", "FLAWLESS GEM", "IRON ORE", "TROLL FAT", "SPIDER SILK", "SPIRIT STONE", "MOUNTAIN HERB"};
  static const Icon ic[] = {Icon::Pelt, Icon::Bone, Icon::Gem, Icon::Ore, Icon::Meat, Icon::Herb, Icon::Gem, Icon::Herb};
  static const int val[] = {12, 6, 120, 8, 25, 14, 80, 10};
  which = std::clamp(which, 0, 7);
  it.name = nm[which]; it.icon = ic[which]; it.value = val[which];
  it.tint = which == 6 ? rgba(170, 120, 255) : 0;
  return it;
}

Item randomLoot(Rng& r, int level, bool boss) {
  float q = r.f();
  if (boss) {
    if (q < 0.35f) { Item w = makeWeapon(r, level + 3); return w; }
    if (q < 0.65f) return makeArmor(r, level + 3, randomArmorSlot(r));
    if (q < 0.8f) return makeJewel(r, level + 2);
    if (q < 0.9f) return makeBow(r, level + 3);
    return makeStaff(r, level + 2);
  }
  if (q < 0.30f) return makePotion((PotionType)r.irange(3), level > 12 ? 1 + (r.f() < 0.3f) : (level > 5 ? (r.f() < 0.5f) : 0));
  if (q < 0.42f) return makeArrows(5 + r.irange(10));
  if (q < 0.56f) return makeMisc(r.irange(8));
  if (q < 0.64f) return makeFood(r.irange(4));
  if (q < 0.78f) return makeWeapon(r, level);
  if (q < 0.90f) return makeArmor(r, level, randomArmorSlot(r));
  if (q < 0.95f) return makeBow(r, level);
  if (q < 0.98f) return makeJewel(r, level);
  return makeStaff(r, level);
}

void writeItem(BinW& w, const Item& it) {
  w.u8((uint8_t)it.kind); w.u8(it.sub); w.u8(it.tier); w.u8((uint8_t)it.ench); w.u8((uint8_t)it.rarity);
  w.u16((uint16_t)it.power); w.u16((uint16_t)it.enchPow); w.i32(it.count); w.i32(it.value);
  w.str(it.name); w.u8((uint8_t)it.icon); w.u32(it.tint); w.i32(it.questId);
}
Item readItem(BinR& r) {
  Item it;
  it.kind = (ItemKind)r.u8(); it.sub = r.u8(); it.tier = r.u8(); it.ench = (Ench)r.u8(); it.rarity = (Rarity)r.u8();
  it.power = (int16_t)r.u16(); it.enchPow = (int16_t)r.u16(); it.count = r.i32(); it.value = r.i32();
  it.name = r.str(); it.icon = (art::Icon)r.u8(); it.tint = r.u32(); it.questId = r.i32();
  // a damaged save must not index past the tables (names, colours, icons): out-of-range values load as defaults
  if ((int)it.kind >= (int)ItemKind::COUNT) it.kind = ItemKind::Misc;
  if ((int)it.ench >= (int)Ench::COUNT) it.ench = Ench::None;
  if ((int)it.rarity > (int)Rarity::Legendary) it.rarity = Rarity::Common;
  if ((int)it.icon >= (int)art::Icon::COUNT) it.icon = art::Icon::Scroll;
  if (it.tier > 5) it.tier = 5;
  return it;
}
