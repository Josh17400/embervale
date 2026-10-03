// EMBERVALE procedural sprite art. Everything is painted from code into Canvases (no art files).
// The renderer bakes these Canvases into textures once (or per distinct look) and blits them.
//
// Conventions
//   - 16px world tiles; 3/4 top-down view (the same perspective as classic SNES/GBA RPGs).
//   - Every sprite has a 1px dark outline (not pure black: a darkened version of the adjacent colour),
//     top-left light source, 3-4 tone shading, and a cohesive palette (see art.cpp palette block).
//   - Transparent = 0 (alpha 0). Ground shadows are NOT part of sprites; the renderer draws them.
//   - Anchor: sprites are drawn with their bottom-centre at the entity's feet / the prop's base tile.
//   - Deterministic: the same inputs always produce identical pixels.
#pragma once
#include <cstdint>
#include "engine/pix.h"

namespace art {

// ---------------------------------------------------------------- humans (player, NPCs, bandits)
// Sheet layout: cells of HUMAN_W x HUMAN_H, HUMAN_FRAMES columns x 3 rows.
//   rows:    0 = facing down (toward camera), 1 = facing up (away), 2 = facing right (flip for left)
//   columns: 0 idle, 1-4 walk cycle, 5 attack wind-up (arm raised back), 6 attack strike (arm forward), 7 hurt
// The character is ~11-13 px wide and ~20-22 px tall, standing on the bottom row of the cell.
constexpr int HUMAN_W = 16, HUMAN_H = 24, HUMAN_FRAMES = 8;

enum class Hair : uint8_t { Bald, Short, Long, Ponytail, Mohawk, Braids, COUNT };
enum class Outfit : uint8_t {
  Tunic,      // shirt + trousers (villager / player default)
  Dress,      // long skirt
  Robe,       // mage / priest, hooded optional
  Leather,    // leather armour (brown, studs)
  Chain,      // iron chainmail (grey)
  Plate,      // steel plate (bright steel, pauldrons)
  Elven,      // gold-green ornate armour
  Ebony,      // black/purple dark armour
  Guard,      // city guard: chain + tabard in tabardColor
  Rags,       // beggar / prisoner
  COUNT
};

struct HumanLook {
  uint32_t skin = rgba(232, 184, 140);
  uint32_t hairColor = rgba(90, 56, 30);
  uint32_t topColor = rgba(60, 100, 160);     // tunic/dress/robe/tabard main colour
  uint32_t bottomColor = rgba(80, 64, 50);    // trousers
  uint32_t tabardColor = rgba(150, 40, 40);   // Guard outfit trim, robe trim
  Hair hair = Hair::Short;
  Outfit outfit = Outfit::Tunic;
  bool beard = false;
  bool helmet = false;   // drawn over hair, style follows outfit (leather cap, iron helm, steel great-helm, elven, ebony horns)
  bool hood = false;     // cloth hood (robes, bandits)
  bool cape = false;     // cape behind (visible esp. from behind), colour = tabardColor
  bool shield = false;   // round shield on off-hand
  uint8_t weapon = 0;    // 0 none, 1 sword, 2 axe, 3 bow, 4 staff, 5 dagger, 6 hammer/pick — drawn in hand, swings in frames 5/6
  uint32_t weaponColor = rgba(200, 205, 215);   // blade/metal colour (tier tint)
};
Canvas humanSheet(const HumanLook& look);

// ---------------------------------------------------------------- monsters
// Sheet layout: one row, MONSTER_FRAMES cells of monsterCellW x monsterCellH, facing RIGHT (renderer flips for left).
//   columns: 0-3 move cycle, 4 attack wind-up, 5 attack strike, 6 hurt, 7 dead (lying on the ground)
constexpr int MONSTER_FRAMES = 8;
enum class Monster : uint8_t {
  Wolf, Boar, Bear, Slime, Spider, Bat, Skeleton, Draugr, Goblin, Troll, Wraith, Mudcrab,
  IceWolf, FrostSpider, Sandworm,   // biome variants
  Dragon,                           // boss: big (~64x48), wings
  COUNT
};
int monsterCellW(Monster m);
int monsterCellH(Monster m);
Canvas monsterSheet(Monster m);

// ---------------------------------------------------------------- props (world objects)
// Each prop is its own canvas with its natural size (propW x propH). Anchor = bottom-centre of the canvas
// sits on the bottom-centre of the prop's tile. "frames" > 1 means the canvas holds that many frames side by side
// (each propW wide) for animation (torches, campfire, fountain).
enum class Prop : uint8_t {
  // nature
  OakTree, OakTree2, PineTree, PineTree2, SnowPine, BirchTree, DeadTree, WillowTree, PalmTree, AutumnTree,
  Bush, BerryBush, SnowBush, Boulder, Rock, MossRock, SnowRock, Stump, Log,
  Flowers1, Flowers2, Flowers3, TallGrass, Reeds, Cactus, Mushrooms, LilyPad, Fern,
  // camp / civilisation
  Chest, ChestOpen, Barrel, Crate, Torch, Campfire, Signpost, Well, FenceH, FenceV, Lamppost, Tent,
  MarketStall, Haystack, Anvil, Cart, Fountain, Statue, Banner, Woodpile, Gravestone, Shrine,
  // dungeon
  CaveEntrance,   // rock arch with dark opening, ~48x32, sits on a mountain/cliff edge
  Ladder,         // ladder up out of a cave (exit), 16x24
  Stalagmite, Crystal, Bones, SkullPile, Cobweb, Brazier, Coffin, Urn, IronDoor, Altar,
  // interior furniture
  Bed, Table, Chair, Shelf, Fireplace, Rug, Counter, Barrel2, PlantPot, Cauldron, Bookshelf, Throne,
  COUNT
};
int propW(Prop p);
int propH(Prop p);
int propFrames(Prop p);
Canvas propSprite(Prop p);

// ---------------------------------------------------------------- buildings
// A building occupies a footprint of wTiles x hTiles (collision). Its sprite is wTiles*16 wide and
// (hTiles+2)*16 tall: the roof rises 2 tiles above the footprint. Bottom of the canvas = bottom of the footprint.
// The door is always in the bottom row at tile column doorCol = wTiles/2 (a 16x~20px door painted on the front wall).
// Front wall is visible as the lower ~1.5 tiles; the roof covers the rest. Chimney/sign/windows per style.
enum class Building : uint8_t {
  House,       // timber-frame cottage, thatch or shingle roof
  StoneHouse,  // stone walls, slate roof
  Inn,         // bigger, hanging sign with mug
  Smithy,      // open forge side, anvil sign, chimney smoke stack
  Shop,        // general store, awning
  Temple,      // stone, steeple/bell, stained window
  Keep,        // castle keep for city lord (jarl): crenellations, banners
  Tower,       // mage tower, tall narrow
  Farmhouse,   // wide barn, red
  Hut,         // small wooden shack (village / bandits)
  COUNT
};
// roofColor tints the roof (pass 0 for the style default). seed varies windows/details.
Canvas buildingSprite(Building b, int wTiles, int hTiles, uint32_t roofColor, uint32_t seed);

// ---------------------------------------------------------------- city wall
// Autotiled wall piece, 16 x 32 (bottom 16 = footprint tile, top 16 = wall height/crenellation).
// mask bits: 1 = wall to north, 2 = east, 4 = south, 8 = west.
Canvas wallPiece(int mask);
Canvas gatePiece();   // 48 x 40 gatehouse arch spanning 3 tiles (passable middle), placed in a wall line

// ---------------------------------------------------------------- items
constexpr int ICON = 16;
enum class Icon : uint8_t {
  Sword, Greatsword, Axe, Dagger, Mace, Bow, Staff, Arrows,
  Shield, Helmet, Armor, Boots, Gloves, Ring, Amulet,
  PotionRed, PotionBlue, PotionGreen, Bread, Meat, Apple, Cheese,
  Gold, Gem, Key, Scroll, Book, Map, Pelt, Bone, Ore, Herb, Sigil, Letter, Crown,
  COUNT
};
// tint recolours the metal/main material (tiers: iron grey, steel, elven gold, glass green, ebony purple, daedric red).
Canvas itemIcon(Icon i, uint32_t tint = 0);

// ---------------------------------------------------------------- effects (small, animated, frames side by side)
enum class Fx : uint8_t {
  Slash,       // 4 frames 24x24, white-silver arc sweeping clockwise, drawn facing right (renderer flips/rotates by choosing variant)
  SlashDown,   // 4 frames 24x24, arc for facing down
  SlashUp,     // 4 frames 24x24, arc for facing up
  Arrow,       // 1 frame 12x3 pointing right
  ArrowDown,   // 1 frame 3x12 pointing down
  Fireball,    // 4 frames 12x12
  Explosion,   // 6 frames 32x32
  Sparkle,     // 4 frames 8x8 (pickups, level up)
  Blood,       // 4 frames 8x8 red splash
  Dust,        // 4 frames 8x8 footstep/roll dust
  Frost,       // 4 frames 12x12 ice bolt
  Heal,        // 4 frames 16x16 green rising crosses
  COUNT
};
int fxW(Fx f);
int fxH(Fx f);
int fxFrames(Fx f);
Canvas fxSprite(Fx f);

// ---------------------------------------------------------------- palette helpers (shared with the renderer)
uint32_t shade(uint32_t c, float k);              // multiply rgb by k (k>1 brightens, clamped)
uint32_t mix(uint32_t a, uint32_t b, float t);    // lerp rgb, keeps a's alpha

}  // namespace art
