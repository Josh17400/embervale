// Floor decorations (Map::deco): one byte per tile, drawn under the y-sorted scene by rpg/view/render_deco.cpp and
// never blocking movement. Interiors fill them (genInterior, WORLDGEN_V3+); the overworld leaves them 0.
// Owned by the props/interiors lane: append new ids before COUNT (values are regenerated, never saved).
#pragma once
#include <cstdint>
#include <vector>

#include "rpg/sim/world.h"

enum class Deco : uint8_t {
  None = 0,
  // small floor clutter (art::Piece::Clutter, index = id - Basket)
  Basket, Sacks, PotsPans, Laundry, Tools, Books, Bottles, Candles, Scrolls, Jugs, Plates, Bread, Cheese, FruitBowl,
  Bucket, Straw, Broom, Kindling,
  // rugs (art::Piece::Rug, colour = id - RugRed): neighbouring tiles with the same id form one rug with a border
  RugRed, RugBlue, RugGreen, RugGold,
  // the room's wall style, set on every back-wall tile (row 1) of a v3 interior (art::RoomStyle = id - WallTimber)
  WallTimber, WallLog, WallStone, WallHall, WallSoot, WallArcane,
  WallAdobe, WallPlaster,   // M0b (art::RoomStyle::Adobe / Plaster)
  COUNT
};

// One piece to draw for a tile: an art::interiorPiece key, its top-left offset from the tile's top-left, and the
// pass it belongs to (all pass-0 pieces are drawn before any pass-1 piece, and so on).
struct DecoLayer {
  uint32_t key;
  int16_t dx, dy;
  uint8_t pass;   // 0 floors and walls, 1 rugs, 2 contact shadows, 3 clutter
};
constexpr int kDecoPasses = 4;

// The pieces tile (tx, ty) of map m shows (appended to out). Interiors get walls, floors, rugs, shadows and clutter
// (old interiors too: their look improves, their layout is untouched); other maps only their clutter. rpg/sim/prop_rules.cpp
void decoLayers(const Map& m, int tx, int ty, std::vector<DecoLayer>& out);

// M0b: the room style of an interior (the Deco::Wall* id on its back wall; older interiors derive it from the floor)
art::RoomStyle interiorStyle(const Map& m);
// M0b: some props are drawn in the room's material or fitted to a partition's short face (stairs and doors in stone
// or wood, wall decor on a partition). Returns the art::interiorPiece key to draw in place of the prop's own sprite
// (same canvas size and anchor: bottom-centre on the tile's bottom-centre), or 0 to draw the prop sprite.
uint32_t interiorPropKey(const Map& m, int tx, int ty, art::Prop p);
