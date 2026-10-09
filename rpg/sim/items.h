// Items: procedural weapons/armour with tiers and enchantments, consumables, quest items.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/sim/common.h"

// Values are saved (writeItem): append only. Gloves, Boots and Cloak arrived with SAVE_VER 3 (M0).
// M6 Steel appends Material: a crafting material (ore, ingot, hide, leather...; sub = craft::Stuff, rpg/sim/craft.h).
enum class ItemKind : uint8_t { Weapon, Bow, Staff, Armor, Helmet, Shield, Ring, Amulet, Potion, Food, Arrows, Misc, Quest, Gloves, Boots, Cloak,
                                Material, COUNT };
inline bool itemEquippable(ItemKind k) {
  return k <= ItemKind::Amulet || k == ItemKind::Gloves || k == ItemKind::Boots || k == ItemKind::Cloak;
}
// M6 appends Spear: the polearm (its silhouette is the maker culture's cult::Polearm: spear, glaive or halberd)
enum class WeaponType : uint8_t { Sword, Axe, Mace, Dagger, Greatsword, Spear, COUNT };
enum class Ench : uint8_t { None, Fire, Frost, Drain, Health, Magicka, Stamina, Fortify, COUNT };
enum class PotionType : uint8_t { Health, Magicka, Stamina, COUNT };
enum class Rarity : uint8_t { Common, Uncommon, Rare, Epic, Legendary };

// ---------------------------------------------------------------- M6 Steel (VISION_PLAN 7.2, 7.3, 15.11)
// What a piece is made of. Leather .. Steel are the universal tiers every culture knows (cult::Tier + 1, in order);
// Alloy is the maker culture's own metal (Item::alloy names which one); Cloth (cloaks), Wood (bows, staves), Bone,
// Precious (jewellery: silver, gold, gems).
enum class Mat : uint8_t { None, Leather, Bronze, Iron, Steel, Alloy, Cloth, Wood, Bone, Precious, COUNT };
const char* matName(Mat m);   // "LEATHER", "BRONZE", "IRON", "STEEL", "ALLOY", "CLOTH", "WOOD", "BONE", "PRECIOUS"
// Affix pool (VISION_PLAN 7.2). Values are saved: append only. ItemAffix::value is in the unit affixInfo names
// (percent for the % affixes, points for Health / Magicka / SpellPower).
enum class Affix : uint8_t {
  None, FireDmg, FrostDmg, ShadowDmg, LifeDrain, CritChance, CritDmg, StaminaRegen, MoveSpeed, RollDist, Thorns,
  ResFire, ResFrost, ResShadow, SpellPower, Magicka, PotionEffect, GoldFind, Health, COUNT
};
struct AffixInfo { const char* name; const char* unit; bool weapon, armour, jewel; };   // name: "+8% FIRE DAMAGE" style label stem
const AffixInfo& affixInfo(Affix a);
struct ItemAffix { Affix kind = Affix::None; int16_t value = 0; };
// Legendary unique powers (VISION_PLAN 7.3; a hand-coded table of about 30). Values are saved: append only.
enum class Unique : uint8_t {
  None,
  ChainLightning,   // every third hit chains lightning to 2 foes
  FireTrail,        // rolling leaves a fire trail
  SpectralWolf,     // kills raise a spectral wolf for 10 s
  SplitArrows,      // arrows split in three beyond 6 tiles
  TimeSlow,         // below 30 % HP time slows for 2 s (60 s cooldown)
  ReflectBlock,     // blocking / rolling through reflects projectiles
  BloodMagic,       // spells cost HP at +40 % power
  StillCrit,        // standing still for 1 s makes the next hit crit
  Executioner,      // +50 % damage to foes below 25 % HP
  Bulwark,          // 30 % less damage while above 80 % HP
  Vampire,          // kills heal 8 % of max HP
  FrostNova,        // being hit freezes nearby foes (20 s cooldown)
  Thunderclap,      // the combo finisher stuns for 1 s
  GoldenTouch,      // +25 % gold from kills and chests
  Windwalker,       // +12 % move speed and a longer roll
  Berserker,        // +2 % damage per 10 % HP missing
  Ward,             // a shield of 15 % max HP that recharges out of combat
  Huntsman,         // +30 % damage to beasts
  Gravebane,        // +30 % damage to the undead
  Dragonbane,       // +40 % damage to dragons and world bosses
  Ember,            // hits burn for 3 s
  Rime,             // hits slow for 2 s
  Echo,             // 15 % chance a hit strikes twice
  SecondWind,       // below 25 % HP stamina refills at once (90 s cooldown)
  Stormcaller,      // crits call a lightning strike on the target
  Shadowstep,       // a roll makes you unseen for 1.5 s (foes lose track)
  Thornmail,        // melee attackers take 25 % of their blow back
  Quickdraw,        // bows draw 30 % faster
  Arcanist,         // +20 % spell power, -20 % magicka cost
  Lifebloom,        // potions also heal 10 % over 10 s
  Warcry,           // a kill gives +15 % damage for 6 s
  Ironhide,         // +25 % armour rating
  Wayfarer,         // +20 % stamina and stamina regeneration
  COUNT
};
struct UniqueInfo { const char* name; const char* line; };   // name: "CHAIN LIGHTNING"; line: the effect, one HUD line
const UniqueInfo& uniqueInfo(Unique u);
constexpr int kItemAffixes = 3;
// Item::flags
enum : uint8_t { IF_CRAFTED = 1, IF_STOLEN = 2, IF_QUESTREWARD = 4, IF_SHOP = 8 };

struct Item {
  ItemKind kind = ItemKind::Misc;
  uint8_t sub = 0;          // WeaponType / PotionType / misc id / craft::Stuff (Material)
  uint8_t tier = 0;         // pre-M6: 0 iron .. 5 emberforged. M6: the band - 1 (0..6; gear::bandOf(ilvl) - 1)
  Ench ench = Ench::None;
  Rarity rarity = Rarity::Common;
  int16_t power = 0;        // damage / armour / heal amount
  int16_t enchPow = 0;
  int count = 1;
  int value = 1;
  std::string name;
  art::Icon icon = art::Icon::Bone;
  uint32_t tint = 0;        // the main material's colour (an alloy's light key, a cloak's cloth)
  int questId = -1;
  // ---- M6 Steel (SAVE_VER 12). Zero everywhere = a pre-M6 / classic item (consumables, goods, quest items keep 0).
  uint8_t ilvl = 0;         // item level 1..60: the danger D of its source (VISION_PLAN 7.2); 0 = no level
  Mat mat = Mat::None;      // what it is made of
  uint8_t alloy = 0;        // Mat::Alloy (and alloy ingots): 1 + index into the maker culture's ArmsStyle::alloys
  uint64_t culture = 0;     // cult::CultureId of the culture that made it: its arms style (silhouettes, ornament,
                            // palette) on the paper doll and the icon, its words in the name. 0 = the heartland look
  uint8_t form = 0;         // the silhouette within the maker's style: cult::HelmForm / BodyArm / ShieldForm / Blade /
                            // Polearm / BowKind + 1 by kind (0 = the culture's first choice)
  uint32_t seed = 0;        // per-item variety (a legendary's name and lore, small look variations)
  ItemAffix affix[kItemAffixes];
  Unique unique = Unique::None;   // a legendary's power
  uint8_t flags = 0;        // IF_*
  bool stackable() const {
    return kind == ItemKind::Potion || kind == ItemKind::Food || kind == ItemKind::Arrows || kind == ItemKind::Misc || kind == ItemKind::Material;
  }
  bool same(const Item& o) const {
    return kind == o.kind && sub == o.sub && tier == o.tier && name == o.name && ilvl == o.ilvl && mat == o.mat && alloy == o.alloy &&
           culture == o.culture;
  }
  int affixCount() const { int n = 0; for (const ItemAffix& a : affix) n += a.kind != Affix::None; return n; }
  int affixValue(Affix k) const { int v = 0; for (const ItemAffix& a : affix) if (a.kind == k) v += a.value; return v; }
};

const char* tierName(int t);
uint32_t tierTint(int t);
uint32_t rarityColor(Rarity r);   // rgba
const char* enchName(Ench e);

Item makeWeapon(Rng& r, int level, int forceType = -1, bool allowEnch = true);
Item makeBow(Rng& r, int level);
Item makeStaff(Rng& r, int level);
Item makeArmor(Rng& r, int level, ItemKind slot);   // Armor, Helmet, Shield, Gloves, Boots, Cloak
Item makeJewel(Rng& r, int level);
Item makePotion(PotionType t, int size);   // size 0 minor, 1 normal, 2 plentiful
Item makeFood(int which);                  // 0-8 (4-8: M2 wayside fish, smoked meat, cakes)
Item makeArrows(int n);
Item makeMisc(int which);                  // pelts, bones, gems, ore... (8-12: M2 hides and herbs)
ItemKind randomArmorSlot(Rng& r);         // Armor, Helmet, Shield, Gloves, Boots or Cloak (one draw)
Item randomLoot(Rng& r, int level, bool boss);

void writeItem(BinW& w, const Item& it);
Item readItem(BinR& r);
