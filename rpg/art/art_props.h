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
  // ---- M0b rooms and storeys (WORLDGEN_V7 genInterior; rpg/sim/rooms.h has the geometry contract). Phase A painted
  //      placeholders; the interior-art lane paints them for real. Stairs and doors are walk-through.
  StairsUp,       // a flight climbing north into the wall face above its tile: the steps fill the tile and rise over the
                  // face into a dark opening (16x40). Walking onto it takes you up a floor
  StairsDown,     // the head of a flight going down: a railed stairwell opening in the floor (16x24)
  DoorH,          // an interior door in an E-W partition, on its face-row doorway tile: frame, lintel over the cap row,
                  // the leaf standing open (16x36)
  DoorV,          // an interior door in an N-S partition, seen edge-on in its frame (16x32)
  Nightstand,     // a bedside table with a candle (12x16)
  Washstand,      // basin and jug on a stand (14x20)
  Oven,           // a domed bread oven / cook stove against a wall, fire glowing in its mouth (16x28, 4 frames)
  PrepTable,      // a kitchen work table: chopping board, knife, vegetables (16x18)
  BottleShelf,    // a bar's back shelf of bottles and mugs, against a wall (16x34)
  WeaponRack,     // spears, swords and a shield in a rack, against a wall (16x30)
  Lectern,        // a reading stand with an open book (12x20)
  Candelabra,     // a tall standing candle-holder (10x28, 4 frames)
  DisplayTable,   // a shop's table with wares laid out (16x18)
  QuenchTub,      // a smith's quenching tub with tongs (16x16)
  Grindstone,     // a treadle grindstone (16x20)
  Pillar,         // a stone column for halls, naves and keeps (16x48)
  BunkBed,        // a two-tier bunk, head to the wall (20x40)
  Dresser,        // a chest of drawers with a mirror, against a wall (16x30)
  COUNT
};
int propW(Prop p);
int propH(Prop p);
int propFrames(Prop p);
Canvas propSprite(Prop p);
// M1 kingdom identity (VISION_PLAN 15.8): a standing banner in a kingdom's colours (field / trim rgba, emblem 0..7), the
// same canvas size and anchor as propSprite(Prop::Banner). The view draws Prop::Banner tiles that belong to a kingdom's
// settlement with this. Phase A stub: the neutral banner; the TOWNS lane paints the colours and emblems.
Canvas kingdomBanner(uint32_t field, uint32_t trim, int emblem);

// ---------------------------------------------------------------- interior surfaces and floor clutter (M0)
// Interiors get painted walls (with thickness and a lit top), floors, rugs, contact shadows and small clutter on top
// of the plain terrain. rpg/sim/deco.h (decoLayers) decides which pieces a tile shows; rpg/view/render_deco.cpp draws
// them. A piece is identified by a 32-bit key (pieceKey) and painted by interiorPiece(key), deterministically.
enum class Piece : uint8_t {
  Floor,      // 16x16. style = FloorStyle | wall-shadow mask << 3 (1 N, 2 W, 4 E, 8 S); a = tx, b = ty
  BackWall,   // 16x32, drawn over the two top wall rows. style = RoomStyle | 16 left end | 32 right end; a = tx
  Cap,        // 16x16 wall top seen from above. style = RoomStyle | top-strip flags << 3 | partition E 64 / W 128;
              // a = room-neighbour mask (CapBits); b = (tx + ty) & 127 | partition N 128 (M0b: the shell joins a
              // partition's top without an edge)
  Shadow,     // a x b soft contact shadow (alpha), drawn under furniture
  Rug,        // 16x16. style = rug colour (0..3); a = same-rug neighbour mask (1 N, 2 E, 4 S, 8 W)
  Clutter,    // 16x16 floor clutter, anchored like a prop. style = clutter index (0 = Deco::Basket)
  Door,       // 16x16 threshold on the exit tile. style = RoomStyle
  // ---- M0b (rooms.h geometry contract). Partitions are cut-away walls one tile high: an E-W run shows its top on the
  //      cap row and a 16 px face on the face row; an N-S run shows its top, and a face where it ends at floor.
  PartFace,   // 16x16 partition face. style = RoomStyle | left end << 3 | right end << 5 (PartEnd); a = tx, b = ty
  PartCap,    // 16x16 partition top seen from above (fills the tile). style = RoomStyle | seed << 3;
              // a = neighbours that are open (floor or a visible face; CapBits), b = the open ones that are faces
  Sill,       // 16x16 threshold of an interior doorway (floor pass). style = RoomStyle; a = 0 E-W wall (DoorH), 1 N-S (DoorV)
  Styled,     // a prop painted in a room's material, same canvas and anchor as the prop. style = RoomStyle;
              // a = 0 StairsUp, 1 StairsDown, 2 DoorH, 3 DoorV; b = variant (StairsUp: 1 under a partition face;
              // stairs: 2 the left tile / 4 the right tile of a two-tile flight); a = 4: a two-tile bed (20x48,
              // anchored on the bed's tile; b = the quilt 0..3); a = 5: a shop counter segment (b = part 0..2 | goods << 2)
  LowDecor,   // 16x32 wall decor sized for a partition's short face (bottom = the face's foot). style = Prop - Tapestry
  COUNT
};
// how a partition face ends at its left / right side (Piece::PartFace)
enum PartEnd : uint8_t { PartEndRun = 0, PartEndWall = 1, PartEndFree = 2 };
// M0b appends Adobe (desert: lime-washed mud brick, niches) and Plaster (whitewashed walls over a wood wainscot: inns
// and town houses); Deco::WallAdobe / WallPlaster select them (rpg/sim/deco.h, same order).
enum class RoomStyle : uint8_t { Timber, Log, Stone, Hall, Soot, Arcane, Adobe, Plaster, COUNT };
// M0b appends Terracotta (fired clay tiles: Adobe rooms) and Rushes (boards strewn with rushes and straw: Log rooms).
enum class FloorStyle : uint8_t { Planks, OldPlanks, Flagstone, Slab, Terracotta, Rushes, COUNT };
enum CapBits : uint8_t { CapE = 1, CapW = 2, CapN = 4, CapNE = 8, CapNW = 16, CapS = 32, CapSE = 64, CapSW = 128 };
constexpr int kClutterKinds = 18;   // Deco::Basket .. Deco::Kindling (rpg/sim/deco.h), same order
inline uint32_t pieceKey(Piece k, int style, int a = 0, int b = 0) {
  return (uint32_t)k | ((uint32_t)(style & 255) << 8) | ((uint32_t)(a & 255) << 16) | ((uint32_t)(b & 255) << 24);
}
Canvas interiorPiece(uint32_t key);

}  // namespace art
