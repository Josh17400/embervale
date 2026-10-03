// EMBERVALE art API: item icons (painted in art_items.cpp). Part of rpg/art.h (include that).
#pragma once
#include <cstdint>
#include "engine/pix.h"

namespace art {

// ---------------------------------------------------------------- items
constexpr int ICON = 16;
enum class Icon : uint8_t {
  Sword, Greatsword, Axe, Dagger, Mace, Bow, Staff, Arrows,
  Shield, Helmet, Armor, Boots, Gloves, Ring, Amulet,
  PotionRed, PotionBlue, PotionGreen, Bread, Meat, Apple, Cheese,
  Gold, Gem, Key, Scroll, Book, Map, Pelt, Bone, Ore, Herb, Sigil, Letter, Crown,
  Cloak,   // M0 (append only: Item::icon is saved)
  COUNT
};
// tint recolours the metal/main material (tiers: iron grey, steel, elven gold, glass green, ebony purple, daedric red).
Canvas itemIcon(Icon i, uint32_t tint = 0);

}  // namespace art
