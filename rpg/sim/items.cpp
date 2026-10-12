#include "rpg/sim/items.h"
#include <algorithm>
#include <cmath>
#include "rpg/sim/gear.h"

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
                                {"HEALTH",                "",   false, true,  true},
                                // M8b Treasure (15.28)
                                {"DAMAGE VS BEASTS",      "%",  true,  false, true},
                                {"DAMAGE VS UNDEAD",      "%",  true,  false, true},
                                {"DAMAGE VS PEOPLE",      "%",  true,  false, true},
                                {"BURNING",               "",   true,  false, false},
                                {"ATTACK SPEED",          "%",  true,  false, true},
                                {"ARMOUR",                "%",  false, true,  false},
                                {"HEALTH REGENERATION",   "",   false, true,  true},
                                {"STAMINA",               "",   false, true,  true}};
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
      // M8b Treasure (15.28): the Mythic powers
      {"TEMPEST", "EVERY HIT CHAINS LIGHTNING TO 3 FOES"},
      {"INFERNO", "YOU LEAVE A BURNING TRAIL THAT SETS FOES ALIGHT"},
      {"SOULFEAST", "6% OF YOUR DAMAGE HEALS YOU; KILLS HEAL 5%"},
      {"BRAMBLE CROWN", "THORNS AURA: FOES BESIDE YOU BLEED EVERY SECOND"},
      {"CHRONOS", "A PERFECT DODGE SLOWS TIME FOR 3 S"},
      {"PHOENIX", "A FATAL BLOW LEAVES YOU AT 30% (ONCE IN 3 MIN)"},
      {"STARFALL", "CRITICAL HITS CALL DOWN A FALLING STAR"},
      {"WOLFKING", "KILLS CALL A SPECTRAL WOLF TO FIGHT FOR 12 S"},
  };
  static_assert(sizeof(t) / sizeof(t[0]) == (size_t)Unique::COUNT, "a UniqueInfo for every unique power");
  return t[(int)u < (int)Unique::COUNT ? (int)u : 0];
}
bool uniqueIsMythic(Unique u) { return (int)u >= (int)Unique::Tempest && (int)u < (int)Unique::COUNT; }
const char* rarityName(Rarity r) {
  static const char* n[] = {"COMMON", "UNCOMMON", "RARE", "EPIC", "LEGENDARY", "MYTHIC"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Rarity::COUNT, "a name for every rarity");
  return n[std::clamp((int)r, 0, RARITIES - 1)];
}
// (M8b, 15.27) white, green, blue, purple, gold, and Mythic's flaming red-orange
uint32_t rarityColor(Rarity r) {
  static const uint32_t c[] = {rgba(230, 230, 230), rgba(110, 220, 110), rgba(90, 160, 255), rgba(200, 110, 255), rgba(255, 196, 56),
                               rgba(255, 92, 36)};
  static_assert(sizeof(c) / sizeof(c[0]) == (size_t)Rarity::COUNT, "a colour for every rarity");
  return c[std::clamp((int)r, 0, RARITIES - 1)];
}
uint32_t rarityColorAt(Rarity r, float t, float phase) {
  if (r != Rarity::Mythic) return rarityColor(r);
  // a flame: ember red -> orange -> hot yellow and back, two waves beating so it never looks like a plain blink
  const float w = 0.5f + 0.5f * std::sin(t * 5.3f + phase * 0.9f) * 0.7f + 0.15f * std::sin(t * 11.7f + phase * 2.3f);
  const float k = std::clamp(w, 0.0f, 1.0f);
  auto mix = [](int a, int b, float f) { return (uint8_t)std::clamp((int)std::lround((float)a + (float)(b - a) * f), 0, 255); };
  // 0 ember (220, 40, 24), 0.5 orange (255, 120, 30), 1 hot yellow (255, 222, 110)
  if (k < 0.5f) { const float f = k * 2; return rgba(mix(220, 255, f), mix(40, 120, f), mix(24, 30, f)); }
  const float f = (k - 0.5f) * 2;
  return rgba(255, mix(120, 222, f), mix(30, 110, f));
}
std::string affixText(Affix a, int v) {
  const std::string n = std::to_string(v);
  switch (a) {
    case Affix::None: case Affix::COUNT: return "";
    case Affix::FireDmg: return "+" + n + "% FIRE DAMAGE";
    case Affix::FrostDmg: return "+" + n + "% FROST DAMAGE";
    case Affix::ShadowDmg: return "+" + n + "% SHADOW DAMAGE";
    case Affix::LifeDrain: return n + "% OF DAMAGE HEALS YOU";
    case Affix::CritChance: return "+" + n + "% CRITICAL CHANCE";
    case Affix::CritDmg: return "+" + n + "% CRITICAL DAMAGE";
    case Affix::StaminaRegen: return "+" + n + "% STAMINA REGENERATION";
    case Affix::MoveSpeed: return "+" + n + "% MOVE SPEED";
    case Affix::RollDist: return "+" + n + "% ROLL DISTANCE";
    case Affix::Thorns: return "ATTACKERS TAKE " + n + "% OF THEIR BLOW";
    case Affix::ResFire: return "+" + n + "% FIRE RESISTANCE";
    case Affix::ResFrost: return "+" + n + "% FROST RESISTANCE";
    case Affix::ResShadow: return "+" + n + "% SHADOW RESISTANCE";
    case Affix::SpellPower: return "+" + n + " SPELL POWER";
    case Affix::Magicka: return "+" + n + " MAGICKA";
    case Affix::PotionEffect: return "+" + n + "% POTION EFFECT";
    case Affix::GoldFind: return "+" + n + "% GOLD FOUND";
    case Affix::Health: return "+" + n + " HEALTH";
    case Affix::VsBeasts: return "+" + n + "% DAMAGE VS BEASTS";
    case Affix::VsUndead: return "+" + n + "% DAMAGE VS UNDEAD";
    case Affix::VsPeople: return "+" + n + "% DAMAGE VS PEOPLE";
    case Affix::Burn: return "BURNS FOES FOR " + n + " DMG/S";
    case Affix::AttackSpeed: return "+" + n + "% ATTACK SPEED";
    case Affix::ArmourPct: return "+" + n + "% ARMOUR";
    case Affix::HealthRegen: return "HEALS " + n + " HEALTH EVERY 5 S";
    case Affix::Stamina: return "+" + n + " STAMINA";
  }
  return "";
}
const char* enchName(Ench e) {
  static const char* n[] = {"", "BURNING", "FROST", "DRAINING", "VITALITY", "THE MAGE", "ENDURANCE", "THE TITAN"};
  return n[(int)e];
}

// ---- (M8b NUMBERS, 15.27.2 "no item above Common may roll without at least one working, visible affix") the classic
//      makers are M6 gear now: an item level (= level), a material, the rarity rolled on the level-gated ladder
//      (gear::rollRarity: an ordinary find of a level-`level` hero at D `level`) and the affixes / power that rarity owes,
//      made by gear::makeGearC (heartland words). No legacy enchantment. Signatures kept (tests, debug kits, old paths).
namespace {
gear::DropSource classicSource(int level) {
  gear::DropSource s;
  s.D = std::clamp(level, 1, gear::MAX_D);
  s.playerLevel = s.D;
  s.src = gear::LootSrc::Body;
  return s;
}
}  // namespace

Item makeWeapon(Rng& r, int level, int forceType, bool allowEnch) {
  // (a random draw keeps to the classic five; a Spear only when forced)
  const WeaponType wt = forceType >= 0 && forceType < (int)WeaponType::COUNT ? (WeaponType)forceType : (WeaponType)r.irange(5);
  const Rarity rar = allowEnch ? gear::rollRarity(r, classicSource(level)) : Rarity::Common;   // the start sword: Common
  return gear::makeGearC(r, ItemKind::Weapon, (int)wt, std::clamp(level, 1, gear::MAX_D), rar, nullptr);
}

Item makeBow(Rng& r, int level) {
  const Rarity rar = gear::rollRarity(r, classicSource(level));
  return gear::makeGearC(r, ItemKind::Bow, 0, std::clamp(level, 1, gear::MAX_D), rar, nullptr);
}

Item makeStaff(Rng& r, int level) {
  // a staff is a spell focus: never Common (as the classic maker)
  Rarity rar = gear::rollRarity(r, classicSource(level));
  if (rar < Rarity::Uncommon) rar = Rarity::Uncommon;
  return gear::makeGearC(r, ItemKind::Staff, 0, std::clamp(level, 1, gear::MAX_D), rar, nullptr);
}

Item makeArmor(Rng& r, int level, ItemKind slot) {
  const Rarity rar = gear::rollRarity(r, classicSource(level));
  return gear::makeGearC(r, slot, 0, std::clamp(level, 1, gear::MAX_D), rar, nullptr);
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
  const bool ring = r.f() < 0.5f;
  const Rarity rar = gear::rollRarity(r, classicSource(level));   // (makeGearC lifts a Common jewel to Uncommon)
  return gear::makeGearC(r, ring ? ItemKind::Ring : ItemKind::Amulet, 0, std::clamp(level, 1, gear::MAX_D), rar, nullptr);
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
  // (M7 fix) the fish are fish (Icon::Fish: raw trout silver, the salmon's smoked glaze), the pie a pie, the cake a cake
  static const Icon ic[] = {Icon::Bread, Icon::Meat, Icon::Apple, Icon::Cheese, Icon::Fish, Icon::Fish, Icon::Pie, Icon::Meat, Icon::Cake};
  static const int heal[] = {15, 30, 10, 20, 18, 28, 24, 34, 16};
  static const uint32_t tint[] = {0, 0, 0, 0, 0, rgba(210, 120, 80), rgba(150, 110, 70), rgba(140, 70, 50), rgba(230, 190, 90)};
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

// (M8b NUMBERS) the classic loot table is gear::rollDropC's: an ordinary body of a level-`level` hero at D `level`, or a
// boss's (rank 4, LootSrc::Boss: always gear, a boss's luck and floor, item level + 2)
Item randomLoot(Rng& r, int level, bool boss) {
  gear::DropSource s = classicSource(level);
  if (boss) { s.rank = 4; s.src = gear::LootSrc::Boss; }
  return gear::rollDropC(r, s, nullptr);
}

// SAVE_VER 12 (M6): the classic fields, then ilvl u8, mat u8, alloy u8, culture u64, form u8, seed u32,
// kItemAffixes x (affix u8, value i16), unique u8, flags u8. SAVE_VER 19 (M8b): kItemAffixes is 4 (it was 3)
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
  if ((int)it.rarity >= (int)Rarity::COUNT) it.rarity = Rarity::Common;
  if ((int)it.icon >= (int)art::Icon::COUNT) it.icon = art::Icon::Scroll;
  if (it.tier > 6) it.tier = 6;
  return it;
}
