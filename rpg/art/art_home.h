// EMBERVALE art API, M7 "Home" (VISION_PLAN 8, 15.22): crops in four growth stages, the farm's yard objects
// (fences, gates, coops, pens, stables, beehives, scarecrows...), tilled farmland and garden paths that join their
// neighbours, the house under construction (scaffolding), the riding horse and its rider's seat, wall trophies,
// paintings of places the player has seen, the FOR SALE sign. Part of rpg/art.h (include that). Painted in
// rpg/art/art_home.cpp (ART lane; phase A stand-ins). The ART lane may ADD to this header; never rename, remove or
// change the meaning of what phase A put here. rpg/sim/home.h uses these enums (art::Crop, art::FarmObj): the sim
// links the art headers, never the reverse.
//
// OWNER QUALITY BAR: commercial 16-bit quality in the game's high 3/4 top-down view, top-left light, a 1 px dark
// outline (a darkened neighbour colour, never pure black), 3-4 tone shading from the cohesive palette (art_internal.h),
// depth and dimension (a coop has a lit roof plane and a shaded side wall, a fence its post tops and a shadow side),
// no ground shadows in the sprites (the renderer draws them) and no boxy repetition (variants by seed). Runs of
// fence, path and farmland JOIN seamlessly: no gaps, orphan posts or broken corners, including diagonal steps and
// where a fence meets its gate.
#pragma once
#include <cstdint>
#include "engine/pix.h"
#include "rpg/art/art_building.h"
#include "rpg/art/art_monsters.h"
#include "rpg/art/art_props.h"

namespace art {

// ---------------------------------------------------------------- crops (VISION_PLAN 8.4)
// Values are saved (home::CropRec::kind): append only.
enum class Crop : uint8_t {
  Wheat, Barley, Oats, Rye,          // grain: green blades, then heads, then gold ears bowing
  Potato, Turnip, Cabbage, Carrot, Onion,   // roots and greens: leafy rosettes, the crop showing at the soil when ripe
  Flax,                              // blue-flowered stalks
  Beans,                             // climbing beans on a stake
  Grapes,                            // a vine on a trellis post (perennial)
  Dates,                             // a young date palm (perennial)
  Maize,                             // tall stalks with cobs and tassels (up to 32 px tall)
  Tea,                               // a clipped tea bush (perennial)
  Rice,                              // a paddy clump standing in water
  Herbs,                             // a herb clump (sage, thyme, lavender by variant)
  COUNT
};
constexpr int CROP_W = 16, CROP_H = 32;   // every crop canvas; it stands on the canvas's bottom row = the tile's bottom
// stage 0 seedling, 1 growing, 2 ripening, 3 ripe. wilted: drooping, paler (dry for 2 days; recovers when watered).
// variant: per-tile variety (lean, leaf count, a missing stalk) so a field is never one sprite cloned.
Canvas cropSprite(Crop c, int stage, uint32_t variant, bool wilted);

// ---------------------------------------------------------------- ground edits (16 x 16, drawn flat over the ground)
// joins: bit 0 N, 1 E, 2 S, 3 W neighbours of the same kind (the run continues there; an edge is drawn where it does
// not). Farmland: tilled furrows in rich soil, darker when watered; a path: the culture's path stones or packed earth
// (style: the settlement's paving; 0 the plain look).
Canvas farmlandTile(uint8_t joins, bool watered, uint32_t variant);
Canvas pathTile(uint8_t joins, uint8_t style, uint32_t variant);
// (M7) a kept yard's trodden grass over the woods' floor (tone 0 green, 1 autumn olive) at world tile (tileX, tileY)
// (its patches continue across tiles); fades toward the sides not joined
Canvas yardGroundTile(uint8_t joins, int tone, int32_t tileX, int32_t tileY);

// ---------------------------------------------------------------- yard objects (VISION_PLAN 8.2)
enum class FarmObj : uint8_t {
  Fence, Gate, Well, Woodpile, Beehive, Scarecrow, Coop, Pen, Stable, Trough, Workbench, Forge, FlowerBed, Sapling,
  Bench, Lantern, Statue, Banner, Campfire, Doghouse, HayRack, ShippingCrate,
  COUNT
};
struct FarmObjLook {
  FarmObj kind = FarmObj::Fence;
  uint8_t joins = 0;         // fences, gates, pens: bit 0 N, 1 E, 2 S, 3 W (the run continues there)
  bool turned = false;       // two-way objects (benches, gates in a N-S fence)
  uint8_t stage = 0;         // a sapling's growth 0..3; a coop / pen / stable: animals inside (0 empty .. 3 full)
  uint32_t variant = 0;
  // the culture's building style (timber, stone, roof colours, ornament) so a yard matches its house; nullptr: the
  // heartland look
  const ArchStyle* style = nullptr;
  uint32_t banner = 0, banner2 = 0;   // Banner: the player's arms (field, charge rgba)
  uint8_t emblem = 0;
};
// The canvas size of an object (by kind and turned): it stands with its canvas bottom on the footprint's bottom edge,
// centred on the footprint (home::objInfo w x h tiles); anything taller rises above (roofs, the well's winch).
int farmObjW(const FarmObjLook& l);
int farmObjH(const FarmObjLook& l);
Canvas farmObjSprite(const FarmObjLook& l);

// ---------------------------------------------------------------- the house under construction
// The building site of a house of wTiles x hTiles (and its storeys) at progress 0 (marked out, a timber pile) ..
// 3 (the frame up, scaffolding round it, half the roof on). Same canvas size and anchor as the finished building's
// sprite (art::buildingSprite of that footprint): bottom-left of the canvas at the footprint's bottom-left.
Canvas scaffoldSprite(int wTiles, int hTiles, int storeys, int progress, uint32_t seed);

// ---------------------------------------------------------------- the riding horse (VISION_PLAN 8.5)
// Sheet: MOUNT_FRAMES columns x 4 rows (0 facing down, 1 up, 2 right; flip for left; (M7 fixer r2) 3: facing down,
// the neck and head alone, drawn over the rider) of HORSE_W x HORSE_H cells:
// columns 0 idle, 1-4 walk, 5-7 gallop. The hooves stand on the cell's row HORSE_H - 2, anchored at the cell's
// bottom-centre like a human. saddled: tack and a saddle cloth (a ridden or stabled horse); coat rgba from the breed,
// varied by `variant` (blaze, socks, dapples).
constexpr int MOUNT_FRAMES = 8, HORSE_W = 36, HORSE_H = 36;   // (M7 fix r3: 36 tall, the ears of the up-facing walk had no room for their outline)
Canvas horseSheet(uint32_t coat, uint32_t variant, bool saddled);
// where the rider's hips sit on the horse in a frame (px from the cell's bottom-centre, y up negative): the view
// draws the rider's Posture::Ride cell so its seat lands there (the horse's head drawn over the rider facing down)
void riderSeat(int row, int frame, int& dx, int& dy);
// (ART lane) one HORSE_W x HORSE_H horse cell, outlined: row as horseSheet; pose 0 idle, 1-4 walk, 5-7 gallop (the
// sheet's columns), 8-9 grazing (head down), 10 lying down. The loose horse critter (Critter::Horse) is painted from it.
Canvas horseCell(uint32_t coat, uint32_t variant, bool saddled, int row, int pose);

// ---------------------------------------------------------------- trophies and paintings (VISION_PLAN 8.3)
constexpr int TROPHY_W = 20, TROPHY_H = 24;   // wall decor: the canvas bottom is the foot of the wall (like Tapestry)
Canvas trophySprite(Monster m, uint32_t variant);   // a mounted head / horns on a shield-shaped board
struct PaintingSpec {
  uint8_t kind = 0;          // 0 a city, 1 a wonder, 2 a peak, 3 the sea, 4 a forest, 5 a ruin
  uint32_t sky = 0, land = 0, accent = 0;   // from the world map's colours at the place (rgba)
  uint32_t seed = 0;         // the place's id: the composition
};
constexpr int PAINTING_W = 32, PAINTING_H = 24;   // wall decor, framed
Canvas paintingSprite(const PaintingSpec& s);

// ---------------------------------------------------------------- the FOR SALE sign (art::Prop::ForSaleSign)
// A post with a hanging board, "FOR SALE" in the culture's letters (the board's cloth or paint by style); painted by
// paintHomeProp (art_props.cpp dispatches isHomeProp props here).
int homePropW(Prop p);
int homePropH(Prop p);
int homePropFrames(Prop p);
void paintHomeProp(Canvas& c, Prop p, int frame);

}  // namespace art
