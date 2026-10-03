// EMBERVALE art API: props (painted in art_props.cpp). Append new props just before COUNT: Map::prop stores Prop + 1. Part of rpg/art.h (include that).
#pragma once
#include <cstdint>
#include "engine/pix.h"

namespace art {

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
  // ---- M0 interiors (WORLDGEN_V3 genInterior). Wall decor hangs on the back wall: it is placed on the wall tile
  //      (row 1 of an interior) and its canvas bottom is the foot of the wall.
  Tapestry, WallShelf, HerbBundle, Antlers, Painting, Sconce, Window, WallShield, ToolRack, PanRack, Wreath, HolySymbol,
  // furniture. One-tile pieces unless noted; Hearth and Forge are 3 tiles wide (the two side tiles hold Filler).
  // Long tables and counters are built from L/M/R segments that join seamlessly.
  Cupboard, Wardrobe, Stool, Bench, Cradle, SpinningWheel, Loom, Desk, Workbench, Hearth, Forge,
  TableSmall, TableMeal, TableWork, TableL, TableM, TableR, CounterL, CounterM, CounterR,
  Filler,         // invisible, solid: the extra footprint tiles of a multi-tile piece
  COUNT
};
int propW(Prop p);
int propH(Prop p);
int propFrames(Prop p);
Canvas propSprite(Prop p);

// ---------------------------------------------------------------- interior surfaces and floor clutter (M0)
// Interiors get painted walls (with thickness and a lit top), floors, rugs, contact shadows and small clutter on top
// of the plain terrain. rpg/sim/deco.h (decoLayers) decides which pieces a tile shows; rpg/view/render_deco.cpp draws
// them. A piece is identified by a 32-bit key (pieceKey) and painted by interiorPiece(key), deterministically.
enum class Piece : uint8_t {
  Floor,      // 16x16. style = FloorStyle | wall-shadow mask << 3 (1 N, 2 W, 4 E, 8 S); a = tx, b = ty
  BackWall,   // 16x32, drawn over the two top wall rows. style = RoomStyle | 16 left end | 32 right end; a = tx
  Cap,        // 16x16 wall top seen from above. style = RoomStyle; a = room-neighbour mask (CapBits); b = tx + ty
  Shadow,     // a x b soft contact shadow (alpha), drawn under furniture
  Rug,        // 16x16. style = rug colour (0..3); a = same-rug neighbour mask (1 N, 2 E, 4 S, 8 W)
  Clutter,    // 16x16 floor clutter, anchored like a prop. style = clutter index (0 = Deco::Basket)
  Door,       // 16x16 threshold on the exit tile. style = RoomStyle
  COUNT
};
enum class RoomStyle : uint8_t { Timber, Log, Stone, Hall, Soot, Arcane, COUNT };
enum class FloorStyle : uint8_t { Planks, OldPlanks, Flagstone, Slab, COUNT };
enum CapBits : uint8_t { CapE = 1, CapW = 2, CapN = 4, CapNE = 8, CapNW = 16, CapS = 32, CapSE = 64, CapSW = 128 };
constexpr int kClutterKinds = 18;   // Deco::Basket .. Deco::Kindling (rpg/sim/deco.h), same order
inline uint32_t pieceKey(Piece k, int style, int a = 0, int b = 0) {
  return (uint32_t)k | ((uint32_t)(style & 255) << 8) | ((uint32_t)(a & 255) << 16) | ((uint32_t)(b & 255) << 24);
}
Canvas interiorPiece(uint32_t key);

}  // namespace art
