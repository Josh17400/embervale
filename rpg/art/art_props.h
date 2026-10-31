// EMBERVALE art API: props (painted in art_props.cpp). Append new props just before COUNT: Map::prop stores Prop + 1. Part of rpg/art.h (include that).
#pragma once
#include <cstdint>
#include "engine/pix.h"
#include "rpg/core/art_ids.h"
#include "rpg/culture/style.h"

namespace art {

// M4 (rpg/art/art_war.cpp, WARDS lane): a war prop in a kingdom's colours (tent cloth, pennants, the command tent's
// banner): field and trim rgba (0: the neutral stand-in look). Same canvas size and anchor as propSprite(p) (frame 0 of
// its sheet); the view caches it by (p, field, trim) and draws it for tiles whose camp has an owner
// (Game::warPropKingdom).
Canvas warPropSprite(Prop p, uint32_t field, uint32_t trim);
// (M12 fixer) a Palisade tile joined into its run (mask: neighbours N 1 E 2 S 4 W 8; gates: sides facing a gateway get a
// gatepost); 16 x 34, bottom on the tile's bottom edge (art_war.cpp)
Canvas palisadePiece(int mask, int gates, uint32_t seed);
// (M12 fixer r2) a lord's stone ring (GardenWall) where a road cuts it: a squat crenellated gate tower over each post and
// a crenellated way between them, in the wall's own masonry (culture: PropStyle::culture, stone: PropStyle::stone).
//   stoneGateTower: 16 x 44, bottom on the post tile's bottom edge (over the post's wall piece)
//   stoneGateArch:  spanPx x 44, an east-west arch, bottom on the posts' tile row's bottom edge, its x 0 at the west
//                   tower's east outline + 1 (the view: post x * 16 + 13; spanPx = 16 * tiles between posts - 11)
//   stoneGateWay:   16 x (28 + 16 * tiles + 16), a north-south way over the road between a north and a south tower, its
//                   top at the north post tile's top - 28 (canvas row = ground row from that tile's top + 28 - height)
Canvas stoneGateTower(int culture, uint32_t stone, bool snow, uint32_t seed);
Canvas stoneGateArch(int spanPx, int culture, uint32_t stone, bool snow, uint32_t seed);
Canvas stoneGateWay(int tiles, int culture, uint32_t stone, bool snow, uint32_t seed);
// (M12 fixer r2) a burned house being cleared at a resettled place's heart: 64 x 64 over a 4 x 3 tile footprint, canvas
// row 0 one tile above its back row; part 0 its scorched floor (flat), 1 its back (the sooty wall, a fallen beam; sorted
// at the back row), 2 its front (charred posts, fallen stone; sorted at the front row); variant 0..3 (art_war.cpp)
Canvas burntShell(int part, int variant);
// M3c (rpg/art/art_flora.cpp): a Wildlands flora prop in variant v (v % floraVariants(p): size, lean, crown shape,
// colour drift), as the land of eco (rpg/world/biomes.h Eco value) grows it (snow on it in the cold, a drier or lusher
// tint by the biome); the same canvas size and anchor as propSprite(p). The view caches them by (p, v, eco).
Canvas floraVariant(Prop p, int v, int eco);
int floraVariants(Prop p);   // 1..16
// a Peak in variant v (0..7: height, lean, ridge pattern) for its land (0 grey rock, 1 snow-capped, 2 sandstone);
// the same canvas size and anchor as propSprite(Prop::Peak). v 8..15: a GreatPeak (propSprite(Prop::GreatPeak)'s size)
Canvas peakVariant(int v, int land);
// (M2 fixer round 2) TallGrass / Boulder in variant v % 8 (the propSprite's canvas size and anchor); the view picks one
// per global tile and flips some, so the wild's commonest props never tile
Canvas tallGrassVariant(int v);
Canvas boulderVariant(int v);
int propW(Prop p);
int propH(Prop p);
int propFrames(Prop p);
Canvas propSprite(Prop p);
// (M3c fixer round 2) a leafy tree as it stands in the snow: snow lying on the top of every clump of its crown (its
// lit upper edges), a cool cast to the leaves; the same canvas and anchor as propSprite(p)
Canvas winterTree(Prop p);
Canvas marketStallVariant(int v);   // M1: a market stall with awning v % 6 and goods (v / 6) % 6 (36 looks)
// M1 economy: a trade's market stall (trade 0..6 in StallProduce order) under awning cloth `awning` (0..7). The same
// canvas and anchor as propSprite(stallProp(trade)) (which is awning 0).
Canvas marketStall(int trade, int awning);
// (fixer M5 r3) snow on the market's 3/4 models (stalls, tables, cloths, carts): while set, every up-facing surface at
// or above model height z is painted under snow; 1e9 (the default) for none. Per thread: set, paint, reset.
void setMarketSnow(float z, uint32_t seed = 0);   // seed: this stall's own drifts (neighbours differ)
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
// a stall in a facing: same trade / awning / form / closed as marketStallForm (which is facing S). The canvas is the
// whole model (sizes differ per facing); stallOrigin gives its top-left from the prop tile's top-left (px). The view
// y-sorts a stall by stallSortY (against its keeper: before them facing N, after them otherwise).
Canvas marketStallFacing(int trade, int awning, int form, bool closed, int facing);
// M3: the same stall with the culture's awnings (PropStyle::awning: 0 striped canvas, 1 plain dyed cloth, 2 reed mat,
// 3 a tiled lean-to (the timber booth's roof), 4 silk with tassels, 5 hide; awningA / awningB colours, else the cloth).
// The layout, goods, keeper spots and canvas are the classic stall's; a classic style gives exactly marketStallFacing.
Canvas marketStallStyled(int trade, int awning, int form, bool closed, int facing, const PropStyle& st);
void stallOrigin(int facing, int& dx, int& dy);
// the ground shadow a stall casts (soft, to the lower right: the light is top-left), drawn under everything standing;
// its top-left from the prop tile's top-left is (kVendorShadowX, kVendorShadowY)
Canvas marketStallShadow(int form, int facing);
constexpr int kVendorShadowX = -28, kVendorShadowY = -40;
// where an open stall hangs its lantern at dusk: the lantern glass's centre, px from the prop tile's top-left (screen)
void stallLantern(int facing, int& lx, int& ly);
// (the open tables, ground cloths and carts in the same projection) their canvases' top-left from the prop tile, and
// their shadows (same origin as marketStallShadow)
void marketTableOrigin(int& dx, int& dy);
void groundClothOrigin(int& dx, int& dy);
Canvas marketTableShadow(int shade);
Canvas groundClothShadow();
// a cart (Prop::Cart) in 3/4: a two-wheeled market cart, its load (v & 3: sacks, crates, barrels, hay) and which way
// its shafts point (v & 4: west); stands on its tile, the shafts reaching over the next
constexpr int kCartLooks = 8;
Canvas marketCart(int v);
Canvas marketCartBroken(int v);   // (P1 fixer r2) the same cart with its near wheel off, the bed tilted onto that side
void marketCartOrigin(int& dx, int& dy);
Canvas marketCartShadow(int v);
// a tile of mine track joined toward its neighbours (bit 1 north, 2 east, 4 south, 8 west); 0 or a lone bit: straight
Canvas mineRail(int joins);
// the mine hill in variant v (0..3) for its land (0 green, 1 snow, 2 dry grass); the canvas of propSprite(MineHill)
Canvas mineHill(int variant, int land);
Canvas ruinVariant(Prop p, int v);   // M1: RuinWall / RuinColumn variant v % 8 (height, broken top, moss); a RuinWall
                                     // with v & 8 joins a wall tile north of it (its top runs on unbroken)
// (M8 ART) the ruin wall tile knowing its run: joins = the RuinWall neighbours (bits 1 N, 2 E, 4 S, 8 W). A tile with a
// wall north or south of it is part of a north-south run: one height for the run and a continuous wall top seen from
// above (blocks laid along it, the west rim lit, the north end broken); any other tile is ruinVariant(p, v | N-join).
// Same canvas (16x36) and anchor as propSprite(RuinWall).
Canvas ruinVariantRun(Prop p, int v, int joins);
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
  Culture,    // (M3 fixer) a people's own version of a piece of furniture (cultureInteriorPiece): style = cult::Archetype,
              // a = the Prop, b = variant (Bed: 1 the two-tile bed; Hearth: the animation frame 0..3)
  // ---- M3b (shaped floors and the new rooms). These keys carry 4-bit room / floor styles: build them with pieceKeyX
  //      (the key's top three bits of the kind byte hold extra flags).
  Pool,       // 16x16 a tile of a bath pool: water, its coping and the far inner side seen below the coping. style =
              // FloorStyle of the floor round it | RoomStyle << 4; a = open-water neighbour mask (CapBits); b = tx + ty
  ShellFace,  // 16x64 the face of a shaped floor's outer wall seen from the room (a round room's back arc, an L's inner
              // wall), anchored on the wall tile (dy = -48): style = RoomStyle | flags << 4 (1 left end free, 2 right end
              // free); a = tx; b = the top's rise toward the left / right neighbour in px (low / high nibble, 0..15)
  ShellCap,   // 16x16 the top of a shaped floor's outer wall where it bends: the strip follows a curve. style = RoomStyle;
              // a = room mask (CapBits), b = face mask (CapBits: the open neighbours that are faces); x = 1 rounded
  RoundEdge,  // (M3b round 3) 16x16 a round room's wall top where the outline is a smooth curve (the front arc and the
              // sides): the strip 6 px wide along the true ellipse, transparent on the room's side, black beyond.
              // style = RoomStyle (low 3 bits; the fourth in x); a = the outward normal's angle in 64ths of a turn;
              // b = the signed distance (px) from the tile's centre to the outline + 64
  RoundBack,  // (M3b fixer) 16x80 a round room's back arc on the TRUE ellipse: the wall's face (and its top band) over one
              // tile column, its foot following the curve. Anchored on the tile holding the column's lowest foot (dy =
              // -64): style = RoomStyle (low 3 bits; the fourth in x bit 0) | 8 left end | 16 right end; x bits 1-2 =
              // tx & 3 (the texture's run); a / b = the foot's row at the column's left / right edge, rows from the
              // anchor tile's top + 64
  COUNT
};
// how a partition face ends at its left / right side (Piece::PartFace)
enum PartEnd : uint8_t { PartEndRun = 0, PartEndWall = 1, PartEndFree = 2 };
// M0b appends Adobe (desert: lime-washed mud brick, niches) and Plaster (whitewashed walls over a wood wainscot: inns
// and town houses); Deco::WallAdobe / WallPlaster select them (rpg/sim/deco.h, same order).
enum class RoomStyle : uint8_t { Timber, Log, Stone, Hall, Soot, Arcane, Adobe, Plaster, Felt, Paper, Living, Marble, Tile, COUNT };
// M0b appends Terracotta (fired clay tiles: Adobe rooms) and Rushes (boards strewn with rushes and straw: Log rooms).
enum class FloorStyle : uint8_t { Planks, OldPlanks, Flagstone, Slab, Terracotta, Rushes, Court, Tatami, Marble, Mosaic, Felt, Roots, Water, COUNT };
enum CapBits : uint8_t { CapE = 1, CapW = 2, CapN = 4, CapNE = 8, CapNW = 16, CapS = 32, CapSE = 64, CapSW = 128 };
constexpr int kClutterKinds = 18;   // Deco::Basket .. Deco::Kindling (rpg/sim/deco.h), same order
inline uint32_t pieceKey(Piece k, int style, int a = 0, int b = 0) {
  return (uint32_t)k | ((uint32_t)(style & 255) << 8) | ((uint32_t)(a & 255) << 16) | ((uint32_t)(b & 255) << 24);
}
Canvas interiorPiece(uint32_t key);
// (M7 fix) FloorStyle::Felt lays fulled felt sheets of FELT_SW x FELT_SH px in staggered rows, now and then a dyed
// shyrdak rug: the sheet holding the floor pixel (gx, gy) (its top-left in floor px) and whether it is a rug. A Floor
// key built with pieceKeyX's flag 4 paints no rug sheets (a rug the curve of a round room's wall would cut).
constexpr int FELT_SW = 56, FELT_SH = 40;
inline uint32_t feltHash(int x, int y, uint32_t s) {   // == art_internal.h hash3 (the painter's)
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + s * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
inline int feltFloorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
inline bool feltRugSheet(int gx, int gy, int& x0, int& y0) {
  const int row = feltFloorDiv(gy, FELT_SH);
  const int shift = (int)(feltHash(row, 0, 1797) % (uint32_t)FELT_SW);
  const int ia = feltFloorDiv(gx + shift, FELT_SW);
  x0 = ia * FELT_SW - shift; y0 = row * FELT_SH;
  return feltHash(ia, row, 1798) % 16u >= 13u;
}
// (M3 fixer) the peoples' furniture indoors (rpg/art/art_culture_props.cpp): whether the culture (cult::Archetype) has its
// own hearth, shelves, bed or cabinet for prop p, and that piece (same canvas and anchor as the classic prop; a Bed is
// 20x32, or 20x48 with variant 1 like Piece::Styled's two-tile bed; a Hearth takes the frame 0..3 as its variant)
// (inline: the sim's interiorPropKey asks it, and the headless tests do not link the painters)
inline bool cultureInteriorHas(int arch, Prop p) {
  // the peoples with their own kit: Dune 4, Steppe 5, Marsh 6, Jade 7, SunTemple 9, Sylvan 10, Starspire 11 (cult::Archetype)
  if (!(arch == 4 || arch == 5 || arch == 6 || arch == 7 || arch == 9 || arch == 10 || arch == 11)) return false;
  switch (p) {
    case Prop::Hearth: case Prop::Bookshelf: case Prop::Shelf: case Prop::Bed: case Prop::Dresser: case Prop::Wardrobe: case Prop::Cupboard:
    case Prop::Painting: case Prop::Wreath:
      return true;
    default: return false;
  }
}
Canvas cultureInteriorPiece(int arch, Prop p, int variant);

// ---------------------------------------------------------------- M3b interiors (INTERIORS lane, appended)
// RoomStyle appends Felt (a yurt's felt over its red lattice wall), Paper (bamboo frames and paper screens: jade),
// Living (grown living wood: sylvan), Marble (white marble: starspire, palaces), Tile (glazed tile dados: baths, dune
// and imperial halls). FloorStyle appends Court (an open court's paving with grass in the joints), Tatami (rush mats),
// Marble, Mosaic (glazed tile mosaic), Felt (felt carpets over the ground: yurts), Roots (living wood boards and moss),
// Water (a bath pool). Both now need four bits: the M3b pieces and the decoders take the fourth from the key's extra
// bits (pieceKeyX: three flag bits in the kind byte; pieceKey's layout otherwise, so every older key is unchanged).
inline uint32_t pieceKeyX(Piece k, int x, int style, int a = 0, int b = 0) {
  return ((uint32_t)k & 31u) | ((uint32_t)(x & 7) << 5) | ((uint32_t)(style & 255) << 8) | ((uint32_t)(a & 255) << 16) | ((uint32_t)(b & 255) << 24);
}
// a seat of power's high seat in the society's idiom (Piece::Styled 8: b = cult::Seat; 48x40 like the Throne, anchored
// bottom-centre on the throne's tile): the khan's dais of cushions, the jarl's carved high seat, the high priest's
// stone seat, the doge's chair, the elven council's white or living-wood throne
constexpr int kStyledThrone = 8;

}  // namespace art
