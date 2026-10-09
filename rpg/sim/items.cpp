#include "rpg/sim/items.h"
#include <algorithm>

using art::Icon;

// (M6: a seventh band, STARMETAL; the NUMBERS lane replaces the heartland words with each culture's, VISION_PLAN 7.2)
const char* tierName(int t) {
  static const char* n[] = {"IRON", "STEEL", "GILDED", "JADE", "OBSIDIAN", "EMBERFORGED", "STARMETAL"};
  return n[std::clamp(t, 0, 6)];
}
uint32_t tierTint(int t) {
  static const uint32_t c[] = {rgba(150, 150, 158), rgba(200, 208, 220), rgba(222, 186, 92), rgba(120, 214, 140), rgba(92, 70, 120), rgba(200, 52, 44),
                               rgba(176, 200, 255)};
  return c[std::clamp(t, 0, 6)];
}
const char* matName(Mat m) {
  static const char* n[] = {"", "LEATHER", "BRONZE", "IRON", "STEEL", "ALLOY", "CLOTH", "WOOD", "BONE", "PRECIOUS"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Mat::COUNT, "a name for every material");
  return (int)m < (int)Mat::COUNT ? n[(int)m] : "";
}
const AffixInfo& affixInfo(Affix a) {
  //                               name                   unit  weapon armour jewel
  static const AffixInfo t[] = {{"",                      "",   false, false, false},
                                {"FIRE DAMAGE",           "%",  true,  false, true},
                                {"FROST DAMAGE",          "%",  true,  false, true},
                                {"SHADOW DAMAGE",         "%",  true,  false, true},
                                {"LIFE DRAIN",            "%",  true,  false, false},
                                {"CRITICAL CHANCE",       "%",  true,  false, true},
                                {"CRITICAL DAMAGE",       "%",  true,  false, true},
                                {"STAMINA REGENERATION",  "%",  false, true,  true},
                                {"MOVE SPEED",            "%",  false, true,  false},
                                {"ROLL DISTANCE",         "%",  false, true,  false},
                                {"THORNS",                "%",  false, true,  false},
                                {"FIRE RESISTANCE",       "%",  false, true,  true},
                                {"FROST RESISTANCE",      "%",  false, true,  true},
                                {"SHADOW RESISTANCE",     "%",  false, true,  true},
                                {"SPELL POWER",           "",   true,  false, true},
                                {"MAGICKA",               "",   false, true,  true},
                                {"POTION EFFECT",         "%",  false, true,  true},
                                {"GOLD FIND",             "%",  false, false, true},
                                {"HEALTH",                "",   false, true,  true}};
  static_assert(sizeof(t) / sizeof(t[0]) == (size_t)Affix::COUNT, "an AffixInfo for every affix");
  return t[(int)a < (int)Affix::COUNT ? (int)a : 0];
}
const UniqueInfo& uniqueInfo(Unique u) {
  static const UniqueInfo t[] = {
      {"", ""},
      {"CHAIN LIGHTNING", "EVERY THIRD HIT CHAINS LIGHTNING TO 2 FOES"},
      {"FIRE TRAIL", "ROLLING LEAVES A TRAIL OF FIRE"},
      {"SPECTRAL WOLF", "KILLS RAISE A SPECTRAL WOLF FOR 10 S"},
      {"SPLIT ARROWS", "ARROWS SPLIT IN THREE BEYOND 6 TILES"},
      {"TIME SLOW", "BELOW 30% HEALTH TIME SLOWS FOR 2 S"},
      {"REFLECTION", "ROLLING THROUGH A MISSILE SENDS IT BACK"},
      {"BLOOD MAGIC", "SPELLS COST HEALTH AT +40% POWER"},
      {"STILLNESS", "STAND STILL 1 S: THE NEXT HIT IS CRITICAL"},
      {"EXECUTIONER", "+50% DAMAGE TO FOES BELOW 25% HEALTH"},
      {"BULWARK", "30% LESS DAMAGE ABOVE 80% HEALTH"},
      {"VAMPIRE", "KILLS HEAL 8% OF YOUR HEALTH"},
      {"FROST NOVA", "BEING HIT FREEZES FOES NEARBY"},
      {"THUNDERCLAP", "THE COMBO FINISHER STUNS FOR 1 S"},
      {"GOLDEN TOUCH", "+25% GOLD FROM KILLS AND CHESTS"},
      {"WINDWALKER", "+12% MOVE SPEED AND A LONGER ROLL"},
      {"BERSERKER", "+2% DAMAGE PER 10% HEALTH MISSING"},
      {"WARD", "A SHIELD OF 15% HEALTH OUT OF COMBAT"},
      {"HUNTSMAN", "+30% DAMAGE TO BEASTS"},
      {"GRAVEBANE", "+30% DAMAGE TO THE UNDEAD"},
      {"DRAGONBANE", "+40% DAMAGE TO DRAGONS AND WORLD BOSSES"},
      {"EMBER", "HITS BURN FOR 3 S"},
      {"RIME", "HITS SLOW FOR 2 S"},
      {"ECHO", "15% OF HITS STRIKE TWICE"},
      {"SECOND WIND", "BELOW 25% HEALTH STAMINA REFILLS"},
      {"STORMCALLER", "CRITICAL HITS CALL DOWN LIGHTNING"},
      {"SHADOWSTEP", "A ROLL HIDES YOU FOR 1.5 S"},
      {"THORNMAIL", "MELEE ATTACKERS TAKE 25% BACK"},
      {"QUICKDRAW", "BOWS DRAW 30% FASTER"},
      {"ARCANIST", "+20% SPELL POWER, -20% MAGICKA COST"},
      {"LIFEBLOOM", "POTIONS ALSO HEAL 10% OVER 10 S"},
      {"WARCRY", "A KILL GIVES +15% DAMAGE FOR 6 S"},
      {"IRONHIDE", "+25% ARMOUR RATING"},
      {"WAYFARER", "+20% STAMINA AND ITS REGENERATION"},
  };
  static_assert(sizeof(t) / sizeof(t[0]) == (size_t)Unique::COUNT, "a UniqueInfo for every unique power");
  return t[(int)u < (int)Unique::COUNT ? (int)u : 0];
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
  const int up = r.f() < 0.25f ? 1 : 0;   // (draws sequenced left to right, as MSVC evaluates them)
  const int down = r.f() < 0.25f ? 1 : 0;
  int t = level / 5 + up - down;
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
  // (M6 phase A: a random draw keeps to the classic five, so the random stream is unchanged; a Spear only when forced)
  WeaponType wt = forceType >= 0 && forceType < (int)WeaponType::COUNT ? (WeaponType)forceType : (WeaponType)r.irange(5);
  it.sub = (uint8_t)wt;
  it.tier = (uint8_t)tierFor(r, level);
  static const int base[] = {7, 8, 9, 5, 13, 9};
  static const char* names[] = {"SWORD", "WAR AXE", "MACE", "DAGGER", "GREATSWORD", "SPEAR"};
  static const Icon icons[] = {Icon::Sword, Icon::Axe, Icon::Mace, Icon::Dagger, Icon::Greatsword, Icon::Spear};
  static_assert(sizeof(names) / sizeof(names[0]) == (size_t)WeaponType::COUNT, "a name for every weapon type");
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
  // 0-3 the M0 foods; 4-8 (M2) the wayside's: a fisher's catch, a hunter's smoked meat, an herbalist's cake
  static const char* nm[] = {"BREAD", "VENISON", "APPLE", "CHEESE WHEEL", "RIVER TROUT", "SMOKED SALMON", "EEL PIE", "SMOKED VENISON", "HONEY CAKE"};
  static const Icon ic[] = {Icon::Bread, Icon::Meat, Icon::Apple, Icon::Cheese, Icon::Meat, Icon::Meat, Icon::Bread, Icon::Meat, Icon::Bread};
  static const int heal[] = {15, 30, 10, 20, 18, 28, 24, 34, 16};
  static const uint32_t tint[] = {0, 0, 0, 0, rgba(150, 170, 190), rgba(230, 130, 100), rgba(170, 130, 80), rgba(140, 70, 50), rgba(230, 190, 90)};
  which = std::clamp(which, 0, 8);
  it.tint = tint[which];
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
  // 0-7 the M0 goods; 8-12 (M2) the wayside's: a hunter's hides, an herbalist's herbs
  static const char* nm[] = {"WOLF PELT", "BONE MEAL", "FLAWLESS GEM", "IRON ORE", "TROLL FAT", "SPIDER SILK", "SPIRIT STONE", "MOUNTAIN HERB",
                             "DEER HIDE", "BEAR PELT", "NIGHTSHADE", "BLUE MOUNTAIN FLOWER", "LAVENDER"};
  static const Icon ic[] = {Icon::Pelt, Icon::Bone, Icon::Gem, Icon::Ore, Icon::Meat, Icon::Herb, Icon::Gem, Icon::Herb,
                            Icon::Pelt, Icon::Pelt, Icon::Herb, Icon::Herb, Icon::Herb};
  static const int val[] = {12, 6, 120, 8, 25, 14, 80, 10, 18, 40, 16, 12, 8};
  which = std::clamp(which, 0, 12);
  it.name = nm[which]; it.icon = ic[which]; it.value = val[which];
  static const uint32_t tint[] = {0, 0, 0, 0, 0, 0, rgba(170, 120, 255), 0, rgba(176, 130, 86), rgba(110, 76, 52), rgba(110, 60, 140), rgba(90, 130, 230), rgba(170, 140, 220)};
  it.tint = tint[which];
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
  if (q < 0.30f) {   // (draws sequenced: the right argument's first, as MSVC/GCC evaluate them)
    const int size = level > 12 ? 1 + (r.f() < 0.3f) : (level > 5 ? (r.f() < 0.5f) : 0);
    return makePotion((PotionType)r.irange(3), size);
  }
  if (q < 0.42f) return makeArrows(5 + r.irange(10));
  if (q < 0.56f) { const int w = r.irange(8); return makeMisc(w == 3 ? 7 : w); }   // (M6) ore is crafting stuff (craft.h)
  if (q < 0.64f) return makeFood(r.irange(4));
  if (q < 0.78f) return makeWeapon(r, level);
  if (q < 0.90f) return makeArmor(r, level, randomArmorSlot(r));
  if (q < 0.95f) return makeBow(r, level);
  if (q < 0.98f) return makeJewel(r, level);
  return makeStaff(r, level);
}

// SAVE_VER 12 (M6): the classic fields, then ilvl u8, mat u8, alloy u8, culture u64, form u8, seed u32,
// 3 x (affix u8, value i16), unique u8, flags u8
void writeItem(BinW& w, const Item& it) {
  w.u8((uint8_t)it.kind); w.u8(it.sub); w.u8(it.tier); w.u8((uint8_t)it.ench); w.u8((uint8_t)it.rarity);
  w.u16((uint16_t)it.power); w.u16((uint16_t)it.enchPow); w.i32(it.count); w.i32(it.value);
  w.str(it.name); w.u8((uint8_t)it.icon); w.u32(it.tint); w.i32(it.questId);
  w.u8(it.ilvl); w.u8((uint8_t)it.mat); w.u8(it.alloy); w.u64(it.culture); w.u8(it.form); w.u32(it.seed);
  for (const ItemAffix& a : it.affix) { w.u8((uint8_t)a.kind); w.u16((uint16_t)a.value); }
  w.u8((uint8_t)it.unique); w.u8(it.flags);
}
Item readItem(BinR& r) {
  Item it;
  it.kind = (ItemKind)r.u8(); it.sub = r.u8(); it.tier = r.u8(); it.ench = (Ench)r.u8(); it.rarity = (Rarity)r.u8();
  it.power = (int16_t)r.u16(); it.enchPow = (int16_t)r.u16(); it.count = r.i32(); it.value = r.i32();
  it.name = r.str(); it.icon = (art::Icon)r.u8(); it.tint = r.u32(); it.questId = r.i32();
  it.ilvl = r.u8(); it.mat = (Mat)r.u8(); it.alloy = r.u8(); it.culture = r.u64(); it.form = r.u8(); it.seed = r.u32();
  for (ItemAffix& a : it.affix) {
    a.kind = (Affix)r.u8(); a.value = (int16_t)r.u16();
    if ((int)a.kind >= (int)Affix::COUNT) a = ItemAffix();
  }
  it.unique = (Unique)r.u8(); it.flags = r.u8();
  if ((int)it.mat >= (int)Mat::COUNT) it.mat = Mat::None;
  if ((int)it.unique >= (int)Unique::COUNT) it.unique = Unique::None;
  if (it.ilvl > 60) it.ilvl = 60;
  if (it.kind == ItemKind::Weapon && it.sub >= (uint8_t)WeaponType::COUNT) it.sub = 0;
  // a damaged save must not index past the tables (names, colours, icons): out-of-range values load as defaults
  if ((int)it.kind >= (int)ItemKind::COUNT) it.kind = ItemKind::Misc;
  if ((int)it.ench >= (int)Ench::COUNT) it.ench = Ench::None;
  if ((int)it.rarity > (int)Rarity::Legendary) it.rarity = Rarity::Common;
  if ((int)it.icon >= (int)art::Icon::COUNT) it.icon = art::Icon::Scroll;
  if (it.tier > 6) it.tier = 6;
  return it;
}
