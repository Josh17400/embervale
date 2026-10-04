// EMBERVALE art API: buildings, roofs, city walls, gate (painted in art_building.cpp). Part of rpg/art.h (include that).
//
// Projection: every building, wall and tower is painted as a real volume in the game's 3/4 (oblique) view: a point at
// ground position (x, y) and height z appears at screen (x, y - z). Light comes from the top-left (north-west, above):
// tops and north/west-facing planes are lit, south-facing walls are mid-tone, east-facing planes are in shade, and cast
// shadows fall to the lower right (the ground part of those shadows is baked into the terrain by the view).
#pragma once
#include <cstdint>
#include <utility>
#include <vector>
#include "engine/pix.h"
#include "rpg/culture/style.h"

namespace art {

// ---------------------------------------------------------------- buildings
// A building occupies a footprint of wTiles x hTiles (collision). Its sprite is wider than the footprint by
// BLDG_PAD_X on each side (eave overhangs, side wings, signs) and extends BLDG_PAD_B below the footprint's bottom
// edge (front steps, porch posts); everything above the footprint is roof and wall height. So the footprint's
// top-left tile corner sits at canvas (BLDG_PAD_X, h - BLDG_PAD_B - hTiles*16).
// The door is always in the bottom row at tile column doorCol = wTiles/2 (Bldg::doorX), painted on the front wall.
enum class Building : uint8_t {
  House,       // cottage in the local style
  StoneHouse,  // the same, built in stone
  Inn,         // two storeys, hanging sign with a mug, lanterns
  Smithy,      // open forge bay, anvil sign, big chimney
  Shop,        // general store, awning and sign
  Temple,      // stone, steeple/bell tower (dome in the desert), rose window
  Keep,        // castle keep for the city lord (jarl): crenellations, corner towers, banners
  Tower,       // mage tower, tall round, conical roof
  Farmhouse,   // wide barn
  Hut,         // small shack (village / bandits)
  COUNT
};
constexpr int BLDG_PAD_X = 8;
constexpr int BLDG_PAD_B = 4;

// Optional facts about a painted building (canvas coordinates), e.g. where chimney smoke rises.
struct BuildingInfo {
  int smokeN = 0;
  int smokeX[3] = {}, smokeY[3] = {};   // chimney mouths that smoke (empty unless the style or the type smokes)
  int height = 0;                       // pixels from the footprint's bottom edge to the highest roof pixel
  std::vector<uint8_t> glass;           // sprite-sized mask: 1 = a visible window pane (the view lights them at night)
  // M0b: what the painter actually drew, so a check (arch_gallery --check) can hold the exterior to the generator's
  // facts: storeys shown (rows of windows / floor beams) and chimney stacks on the roof
  int storeys = 0;
  int chimneys = 0;
};
// The night look of a building sprite: its window panes (BuildingInfo::glass) lit warm from inside, with a little
// variation per pane. Same size as the sprite.
Canvas buildingNight(const Canvas& sprite, const std::vector<uint8_t>& glass, uint32_t seed);

// M0b: what the world generator decided about a building that its exterior must show (VISION_PLAN 15.7: the exterior
// and the interior agree). Bldg carries these (world.h bldgFacts). The defaults reproduce the classic M0 look.
struct BuildingFacts {
  int storeys = 0;      // storeys the walls show (window rows, floor beams, wall height); 0 = defaultStoreys(type).
                        // From WORLDGEN_V7 the interior has exactly this many floors joined by stairs.
  bool hearth = true;   // false: nothing inside burns a fire, so the roof carries NO chimney. true: chimneys as the
                        // style and the type decide (a style without chimneys may still show none: chimney => hearth,
                        // not the reverse)
};
// The storeys each type showed before M0b (and still shows for worlds before WORLDGEN_V7): inns and keeps 2, mage
// towers 3, everything else 1. Header-only: the simulation (which does not link the art) needs it.
inline int defaultStoreys(Building b) {
  switch (b) {
    case Building::Inn: case Building::Keep: return 2;
    case Building::Tower: return 3;
    default: return 1;
  }
}

// The style decides roof shape and material, wall material and climate details; the type only adds its function
// (inn sign, forge, steeple, crenellations, awning). seed varies the massing (wings, porch, dormers, chimneys),
// windows and weathering, so neighbours in one style never look copy-pasted. facts: storeys and hearth (above).
Canvas buildingSprite(Building b, int wTiles, int hTiles, const ArchStyle& style, uint32_t seed, BuildingInfo* info,
                      const BuildingFacts& facts);
Canvas buildingSprite(Building b, int wTiles, int hTiles, const ArchStyle& style, uint32_t seed, BuildingInfo* info = nullptr);
// Older form kept for the tools: the plains style for this seed, roofColor (0 = material default) as the roof tint.
Canvas buildingSprite(Building b, int wTiles, int hTiles, uint32_t roofColor, uint32_t seed);
// Cheap estimate of BuildingInfo::height without painting (for baked ground shadows).
int buildingHeight(Building b, int wTiles, int hTiles, const ArchStyle& style, const BuildingFacts& facts);
int buildingHeight(Building b, int wTiles, int hTiles, const ArchStyle& style);

// ---------------------------------------------------------------- city wall
// The wall is drawn per wall tile, from the tile's 8-neighbourhood: straight runs, outer and inner corners (bevelled,
// so stair-stepped rings read as continuous 45-degree walls), diagonal links, T and X joins all join seamlessly.
// Each wall tile's sprite is WALL_CW x WALL_CH; the tile's top-left corner sits at canvas (WALL_OX, WALL_OY).
// Draw it sorted with the tile's row; a tile with a tower draws just after its row's plain wall tiles.
constexpr int WALL_H = 20;                     // height of the walkway above the ground, px
constexpr int WALL_OX = 8, WALL_OY = 40, WALL_CW = 32, WALL_CH = 60;
constexpr uint32_t WALL_BIT_N = 1u, WALL_BIT_NE = 2u, WALL_BIT_E = 4u, WALL_BIT_SE = 8u, WALL_BIT_S = 16u, WALL_BIT_SW = 32u,
                   WALL_BIT_W = 64u, WALL_BIT_NW = 128u;   // neighbours that are wall
constexpr uint32_t WALL_BIT_TOWER = 256u;      // a round tower stands on this tile (ends, strong corners, long runs)
constexpr uint32_t WALL_BIT_TOWER_N = 512u;    // the tile to the north has a tower (its base overlaps this tile)
constexpr int WALL_VAR_SHIFT = 12;             // 2 bits of per-tile variation (weathering) at bits 12..13
constexpr uint32_t WALL_BIT_CULVERT = 1u << 14; // a river runs under this tile: an arched water gate with an iron
                                               // grate in the south face (the view sets it on wall tiles over water)
Canvas wallTile(uint32_t key);

// The wall layout pass shared by the view, the gallery and the tests: for a wall grid (w x h, nonzero = wall) and the
// gatehouses (left tile of each 3-wide opening), the wallTile key of every tile, 0 = draw nothing (no wall, or a gate's
// flanking tile, which the gatehouse tower covers). Picks the towers: wall ends, strong corners and long runs, spaced
// out and kept clear of the gates. Deterministic, O(w*h).
void wallKeys(const uint8_t* wall, int w, int h, const std::pair<int, int>* gates, int nGates, std::vector<uint32_t>& keys);

// Ground shade cast by the wall layout at ground pixel (px, py): 0 none, 1 cast shadow (down-right of the wall),
// 2 contact shade at the foot of a wall face. The view bakes it into the terrain.
int wallShadeAt(const uint8_t* wall, int w, int h, int px, int py);

// Gatehouse over a 3-tile opening in a horizontal wall run (World::gates: (gx, gy) = left tile of the opening). Its
// flanking towers stand on the wall tiles gx-1 and gx+3 (which the view does not draw itself), and the arch spans the
// whole opening. Canvas GATE_CW x GATE_CH; tile (gx, gy)'s top-left corner sits at canvas (GATE_OX, GATE_OY).
constexpr int GATE_OX = 24, GATE_OY = 52, GATE_CW = 96, GATE_CH = 80;   // towers reach 6 px below the wall front
Canvas gateHouse(uint32_t seed);

// Older pieces kept for the tools: wallPiece(mask) is wallTile for a 4-bit N/E/S/W mask (diagonals filled where both
// sides are), gatePiece() is gateHouse(0).
Canvas wallPiece(int mask);
Canvas gatePiece();

}  // namespace art
