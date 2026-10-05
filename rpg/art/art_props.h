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
  // ---- M1 ruins: the standing remains of old halls (overworld ruin sites). The view draws each tile with its own
  //      variant (ruinVariant), so a run of wall never repeats one broken top.
  RuinWall,       // a tile of broken masonry wall in 3/4 view: a 16 px deep top over a south face (16x36). Runs join
                  // seamlessly side by side, and north-south runs show one continuous wall top
  RuinColumn,     // a snapped-off column on its plinth (16x40)
  // ---- M1 economy (owner 2026-10-05; rpg/world/economy.h). Market stalls by trade: a counter three tiles wide under a
  //      sloping awning, seen from the customers' side (south), the trade's goods on the counter and hanging under the
  //      awning. The prop stands on the middle tile; the generator puts Filler on the tiles either side (the counter is
  //      solid) and the stall keeper on the tile behind. The view varies the awning cloth per stall (marketStall).
  StallProduce, StallFish, StallCloth, StallPottery, StallMeat, StallBread, StallTools,
  StallTimber,    // (M1 fixer) the lumber trade's stall: sawn planks, split firewood and a log or two, an axe on the rail
  //      market and trade clutter, and the specialisations' yard machinery
  Sacks,          // a heap of grain / flour sacks (20x16)
  Baskets,        // two wicker baskets of produce (20x14)
  DryingRack,     // a fish-drying rack: a pole frame hung with split fish (30x30)
  HideRack,       // a tanner's stretching frame with a hide laced in it (24x28)
  OreCart,        // a mine cart heaped with ore on a stub of rail (26x22)
  OrePile,        // a heap of broken ore (22x14)
  MineEntrance,   // a timbered adit into the hillside (44x36); stands on its middle tile, Filler either side
  LogPile,        // trunks stacked for the sawmill (34x20)
  Trough,         // a wooden water trough for the beasts (26x14)
  WaterWheel,     // a watermill's undershot wheel turning in the river beside the mill (24x34, 4 frames); stands on the
                  // water tile against the mill's side wall
  // ---- M1 fixer round 2: a grand market has more than one kind of stall, and the trades' work yards
  MarketTable,    // an open trestle table of goods two tiles wide, sometimes under a square sunshade (48x44). Stands on
                  // its WEST tile (the canvas's left third is empty), Filler on the east tile; the view picks its goods
                  // and shade by tile (marketTable)
  GroundCloth,    // a cloth spread on the paving with goods laid out on it, two tiles wide (48x20); like MarketTable
  MarketCross,    // a market cross: a stepped stone base, a shaft and a carved head (32x60), a market's centrepiece
  Sheep,          // a grazing sheep (20x18, 8 frames: head down cropping, then up)
  Cow,            // a grazing cow (30x24, 8 frames)
  MineRail,       // a tile of mine-cart track (16x16, flat, walk-over); the view joins it to its neighbours (mineRail)
  PenShelter,     // an open-fronted lean-to shed for the flock, hay in its rack (48x44); middle tile, Filler either side
  MineHill,       // the mine: a grassy knoll, its south face a cliff of layered rock with the timbered adit cut into its
                  // foot (80x76); stands on the adit's tile, the knoll's five-by-four footprint all Filler (mineHill)
  COUNT
};
// M1 economy: the market stall trades in prop order (ew::StallTrade); stallProp(t) and back
constexpr int kStallTrades = 8;
inline Prop stallProp(int trade) { return (Prop)((int)Prop::StallProduce + (trade % kStallTrades)); }
inline bool isStall(Prop p) { return (int)p >= (int)Prop::StallProduce && (int)p <= (int)Prop::StallTimber; }
inline int stallTrade(Prop p) { return (int)p - (int)Prop::StallProduce; }
int propW(Prop p);
int propH(Prop p);
int propFrames(Prop p);
Canvas propSprite(Prop p);
Canvas marketStallVariant(int v);   // M1: a market stall with awning v % 6 and goods (v / 6) % 6 (36 looks)
// M1 economy: a trade's market stall (trade 0..6 in StallProduce order) under awning cloth `awning` (0..7). The same
// canvas and anchor as propSprite(stallProp(trade)) (which is awning 0).
Canvas marketStall(int trade, int awning);
constexpr int kStallAwnings = 8;
// (M1 fixer round 2) one stall in three looks: 0 the cloth-awning booth, 1 a peaked canvas tent over a clothed trestle,
// 2 a timber booth under a shingle roof with a painted board. closed: packed up for the night (the goods under a
// cover, the hanging goods taken in, a booth's shutter down). Same canvas and anchor as marketStall.
constexpr int kStallForms = 3;
Canvas marketStallForm(int trade, int awning, int form, bool closed);
// the open table's goods (cheese, eggs, spices, flowers, honey and candles, baskets, wool, apples and cider) and its
// sunshade (0 none, else a cloth colour); the ground cloth's goods (pots, rugs, gourds and roots, furs). Canvases as
// propSprite(MarketTable / GroundCloth).
constexpr int kTableGoods = 8, kTableShades = 4, kClothGoods = 4;
Canvas marketTable(int goods, int shade, bool closed);
Canvas groundCloth(int goods, int cloth, bool closed);
// a tile of mine track joined toward its neighbours (bit 1 north, 2 east, 4 south, 8 west); 0 or a lone bit: straight
inline bool isVendorProp(Prop p) { return isStall(p) || p == Prop::MarketTable || p == Prop::GroundCloth; }
Canvas mineRail(int joins);
// the mine hill in variant v (0..3) for its land (0 green, 1 snow, 2 dry grass); the canvas of propSprite(MineHill)
Canvas mineHill(int variant, int land);
Canvas ruinVariant(Prop p, int v);   // M1: RuinWall / RuinColumn variant v % 8 (height, broken top, moss); a RuinWall
                                     // with v & 8 joins a wall tile north of it (its top runs on unbroken)
// M1 kingdom identity (VISION_PLAN 15.8): a standing banner in a kingdom's colours (field / trim rgba, emblem 0..7), the
// same canvas size and anchor as propSprite(Prop::Banner). The view draws Prop::Banner tiles that belong to a kingdom's
// settlement with this (field == 0 gives the plain banner).
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
