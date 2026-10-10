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
  Spear,   // M6 Steel: the polearm (spear / glaive / halberd by IconLook::form)
  Ingot,   // M6 Steel: a bar of smelted metal (crafting; tint = the metal)
  // M7 Home (lead, phase A; ART lane paints them; phase A stand-ins): tint = the crop / product colour
  Seeds,   // a cloth seed pouch
  Sheaf,   // a sheaf of grain (wheat, barley, oats, rye, rice, maize; flax)
  Veg,     // a root or a head of greens (potato, turnip, cabbage, carrot, onion, beans)
  Fruit,   // a bunch of grapes / dates
  Egg,     // eggs in straw
  Milk,    // a milk jug
  Wool,    // a fleece / a skein of wool
  Hay,     // a bale of hay
  Flour,   // a sack of flour
  Honey,   // a honey pot
  Meal,    // a cooked meal in a bowl (tint = the dish)
  Hoe,     // farm tools: hoe, watering can, sickle, grooming brush
  WateringCan,
  Sickle,
  Brush,
  Deed,    // a sealed deed to a property
  // M7 fix round 1 (append only): the fish (raw: silver; tint = a cooked glaze) and the cook's dishes, so a meal's
  // picture matches it (the bowl Meal stays for soups, stews and broths)
  Fish, Pie, Roast, Cake,
  COUNT
};
// tint recolours the metal/main material (tiers: iron grey, steel, elven gold, glass green, ebony purple, daedric red).
Canvas itemIcon(Icon i, uint32_t tint = 0);

// M6 Steel (VISION_PLAN 7.2, 15.11): an icon in its maker culture's style and its metal's sheen, so loot from far lands
// LOOKS foreign in the pack. Built from an Item by gearIcon() (rpg/sim/gear_look.h); the art lane paints it. Every
// field 0 except icon / tint = the classic itemIcon(icon, tint), pixel for pixel.
struct IconLook {
  Icon icon = Icon::Sword;
  uint32_t tint = 0;        // the main material (as itemIcon)
  uint32_t tint2 = 0;       // an alloy's dark key (0: derived from tint)
  uint8_t form = 0;         // silhouette within the kind, by icon: helm cult::HelmForm + 1, body cult::BodyArm + 1,
                            // shield cult::ShieldForm + 1, sword / greatsword / dagger cult::Blade + 1, spear
                            // cult::Polearm + 1, bow cult::BowKind + 1 (0: the classic shape)
  uint8_t sheen = 0;        // cult::Sheen + 1 (matte, bright, dark, banded, iridescent, glowing, pale, burnished; 0 plain)
  uint16_t ornament = 0;    // cult::ARM_* bits (gilding, studs, fur trim, etching, filigree...)
  uint32_t accent = 0;      // the culture's cloth / leather / plume colour on grips, straps and crests (0: classic)
  uint8_t rarity = 0;       // Rarity (0 common .. 4 legendary): an epic or legendary piece may glint (the slot frame
                            // colour is the HUD's)
  uint8_t mat = 0;          // Mat (rpg/sim/items.h) as int: leather and cloth paint differently from metal
  uint64_t key() const {    // identity for the view's icon cache: equal keys paint identical pixels
    uint64_t k = 0x9AE16A3B2F90404Full;
    auto mx = [&](uint64_t v) { k ^= v; k *= 0x100000001B3ull; };
    mx((uint64_t)icon | (uint64_t)form << 8 | (uint64_t)sheen << 16 | (uint64_t)ornament << 24 | (uint64_t)rarity << 40 | (uint64_t)mat << 48);
    mx((uint64_t)tint << 32 | tint2);
    mx(accent);
    return k;
  }
};
Canvas itemIconLook(const IconLook& l);

}  // namespace art
