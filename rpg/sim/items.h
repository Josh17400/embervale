// Items: procedural weapons/armour with tiers and enchantments, consumables, quest items.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/sim/common.h"

enum class ItemKind : uint8_t { Weapon, Bow, Staff, Armor, Helmet, Shield, Ring, Amulet, Potion, Food, Arrows, Misc, Quest, COUNT };
enum class WeaponType : uint8_t { Sword, Axe, Mace, Dagger, Greatsword, COUNT };
enum class Ench : uint8_t { None, Fire, Frost, Drain, Health, Magicka, Stamina, Fortify, COUNT };
enum class PotionType : uint8_t { Health, Magicka, Stamina, COUNT };
enum class Rarity : uint8_t { Common, Uncommon, Rare, Epic, Legendary };

struct Item {
  ItemKind kind = ItemKind::Misc;
  uint8_t sub = 0;          // WeaponType / PotionType / misc id
  uint8_t tier = 0;         // 0 iron .. 5 daedric
  Ench ench = Ench::None;
  Rarity rarity = Rarity::Common;
  int16_t power = 0;        // damage / armour / heal amount
  int16_t enchPow = 0;
  int count = 1;
  int value = 1;
  std::string name;
  art::Icon icon = art::Icon::Bone;
  uint32_t tint = 0;
  int questId = -1;
  bool stackable() const { return kind == ItemKind::Potion || kind == ItemKind::Food || kind == ItemKind::Arrows || kind == ItemKind::Misc; }
  bool same(const Item& o) const { return kind == o.kind && sub == o.sub && tier == o.tier && name == o.name; }
};

const char* tierName(int t);
uint32_t tierTint(int t);
uint32_t rarityColor(Rarity r);   // rgba
const char* enchName(Ench e);

Item makeWeapon(Rng& r, int level, int forceType = -1, bool allowEnch = true);
Item makeBow(Rng& r, int level);
Item makeStaff(Rng& r, int level);
Item makeArmor(Rng& r, int level, ItemKind slot);
Item makeJewel(Rng& r, int level);
Item makePotion(PotionType t, int size);   // size 0 minor, 1 normal, 2 plentiful
Item makeFood(int which);
Item makeArrows(int n);
Item makeMisc(int which);                  // pelts, bones, gems, ore...
Item randomLoot(Rng& r, int level, bool boss);

void writeItem(BinW& w, const Item& it);
Item readItem(BinR& r);
