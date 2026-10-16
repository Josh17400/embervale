// EMBERVALE art, M10 "Beyond the Mist" (VISION_PLAN 2.12, 15.4, 15.31): the sea props (art::Prop::MooringPost ..
// Wreck) and the SHIPS. ART lane (its only editor after phase A; it may ADD here, never rename or change the meaning of
// what is listed). Included by rpg/art/art_props.cpp (sea props are drawn through propSprite like every other prop).
//
// Everything is painted in the game's high 3/4 view (a point at ground (x, y) and height z is drawn at (x, y - z)) with
// the light from the top-left (lit tops and west faces, mid-tone south faces, shaded east sides), a selective 1 px
// outline, cast shadows to the lower right, and the game's cohesive palette (art_internal.h ramps).
#pragma once
#include <cstdint>
#include "engine/pix.h"
#include "rpg/art/art_props.h"

namespace art {

int seaPropW(Prop p);
int seaPropH(Prop p);
int seaPropFrames(Prop p);
void paintSeaProp(Canvas& c, Prop p, int frame);

// ---- ships (the boats the player owns, ferries, pirates, moored ships in a harbour)
// hull: sea::Hull as int (0 rowboat, 1 sloop, 2 galleon). A sheet is SHIP_DIRS columns (headings: 0 east, then
// clockwise in 45 degree steps: 2 south, 4 west, 6 north) by SHIP_FRAMES rows (the swell's bob) of shipCellW x
// shipCellH cells. The hull's waterline centre sits at (shipAnchorX, shipAnchorY) in every cell: the view puts that on
// the boat's position (and the hero, aboard, stands on the deck at shipDeckY).
constexpr int SHIP_DIRS = 8, SHIP_FRAMES = 2;
int shipCellW(int hull);
int shipCellH(int hull);
int shipAnchorX(int hull);
int shipAnchorY(int hull);
int shipDeckY(int hull);        // the deck's height over the waterline (px): where a figure aboard stands
// a ship in a culture's style: the yard's colours and forms (the view fills it from the culture: cult::Culture's
// palette, archetype and arms; 0s: the plain heartland look)
struct ShipStyle {
  uint32_t hull = 0, trim = 0;    // planking and its trim / paint (0: natural timber)
  uint32_t sail = 0, sail2 = 0;   // sail cloth and its stripe / device colour (0: undyed canvas)
  uint8_t culture = 0;            // cult::Archetype + 1 (0 classic): picks the hull form, the sail cut and the figurehead
  uint8_t sailCut = 0;            // 0 square, 1 lateen, 2 battened junk, 3 crab-claw (the culture decides; 0 classic)
  uint8_t pennant = 0;            // 1: fly the owner's pennant (the view tints it with the arms)
  bool furled = false;            // sails furled (moored, at anchor)
  // ---- (M10 ART lane additions)
  int8_t brace = 0;               // the yards braced / the sheet eased: -1 to port, 0 square, 1 to starboard (the helm)
  uint8_t fill = 2;               // 0 slack (luffing, no way on), 1 drawing, 2 full (the wind astern)
  uint32_t pennantA = 0, pennantB = 0;   // the pennant's two colours (the arms' field and charge; 0: the sail colours)
  uint8_t culture2 = 0;           // a hidden people's second archetype + 1 (their crossed forms: its ornament and rig)
  uint8_t extras = 0;             // SX_* bits
};
enum : uint8_t {
  SX_LANTERN = 1,      // a lantern at the bow (sea::UP_LANTERN) as well as the stern's
  SX_FIGUREHEAD = 2,   // the culture's figurehead (sea::UP_FIGUREHEAD; galleons always carry one)
  SX_PIRATE = 4,       // a pirate: tarred hull, dark patched sails, the skull
  SX_PLATING = 8,      // iron strapping on the hull (sea::UP_PLATING)
  SX_NETS = 16,        // a fishing boat: nets and a basket aboard
  SX_WRECKED = 32      // a hulk: torn sails, broken spars (the view's sinking boat)
};
Canvas shipSheet(int hull, const ShipStyle& st);
uint64_t shipStyleKey(int hull, const ShipStyle& st);   // the view's cache key

// One ship cell at a fine heading (the view's own drawing: SHIP_FINE_DIRS headings so a turn rotates smoothly).
// fineDir 0 east, clockwise (8: south, 16: west, 24: north); frame 0..SHIP_FRAMES-1 (the bob: frame 1 sits a pixel
// deeper and heels a touch). layer: SHIP_LAYER_ALL the whole ship with its shadow on the water; for the boat the hero
// stands on, SHIP_LAYER_BACK (everything behind the hero's place, and the shadow) goes down before the hero and
// SHIP_LAYER_FRONT (the near gunwale, and the rig when it stands nearer the viewer than the hero) after him.
// Same cell size and anchor as shipSheet. shipSheet(h, s) cell d == shipCell(h, s, d * SHIP_FINE_DIRS / SHIP_DIRS, f, ALL).
constexpr int SHIP_FINE_DIRS = 32;
enum : int { SHIP_LAYER_ALL = 0, SHIP_LAYER_BACK = 1, SHIP_LAYER_FRONT = 2 };
Canvas shipCell(int hull, const ShipStyle& st, int fineDir, int frame, int layer);
// the sail cut a culture's yards carry (ShipStyle::culture -> sailCut): square, lateen, battened junk, crab-claw
uint8_t shipSailCutFor(uint8_t culture);
// the mast tops of a hull at a fine heading, in cell px (where the view hangs a lantern's light / a pennant's sound):
// returns the count (<= 3) written to xs / ys
int shipMastTops(int hull, const ShipStyle& st, int fineDir, int* xs, int* ys);
// the stern lantern's place in the cell at a fine heading (cell px; the view's light)
void shipLanternAt(int hull, const ShipStyle& st, int fineDir, int& x, int& y);

// ---- the catch card's fish (sea::Species::color, a seed for its shape, its rarity 0..3: a legendary one gleams)
Canvas fishSprite(uint32_t color, uint32_t seed, int rarity);
// ---- a kraken's tentacle rising from the swell (a Monster::Kraken swimmer of the SEA lane's SW_TENTACLE kind): one
// row of MONSTER_FRAMES cells like a monster sheet (0-3 sway, 4 wind-up rearing back, 5 the slam, 6 hurt, 7 limp);
// the base sits on the cell's bottom row (the waterline)
constexpr int SEA_TENTACLE_W = 32, SEA_TENTACLE_H = 44;
Canvas seaTentacleSheet();

}  // namespace art
