// Fog of war for the world map (VISION_PLAN 2.11): 1 bit per 8 x 8-tile cell, 128 bytes per region (256 x 256 tiles),
// keyed by global region. Saved (SAVE_VER 5). The SIM lane owns it (Game marks what the player has seen); the VIEW lane
// reads it to draw the world map's discovered areas.
#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "rpg/world/coords.h"

struct ExploredMask {
  static constexpr int CELL = 8;                          // tiles per cell side
  static constexpr int CELLS = ew::REGION / CELL;         // 32 cells per region side
  std::unordered_map<uint64_t, std::vector<uint8_t>> regions;   // key(rx, ry) -> CELLS * CELLS bits

  static uint64_t key(int32_t rx, int32_t ry) { return ((uint64_t)(uint32_t)rx << 32) | (uint32_t)ry; }
  static int32_t keyRx(uint64_t k) { return (int32_t)(uint32_t)(k >> 32); }
  static int32_t keyRy(uint64_t k) { return (int32_t)(uint32_t)k; }

  // the cell holding global tile (gx, gy)
  bool seen(int32_t gx, int32_t gy) const {
    auto it = regions.find(key(ew::regionOf(gx), ew::regionOf(gy)));
    if (it == regions.end()) return false;
    int cx = (gx & (ew::REGION - 1)) / CELL, cy = (gy & (ew::REGION - 1)) / CELL, b = cy * CELLS + cx;
    return (it->second[(size_t)(b >> 3)] >> (b & 7)) & 1;
  }
  void mark(int32_t gx, int32_t gy) {
    std::vector<uint8_t>& m = regions[key(ew::regionOf(gx), ew::regionOf(gy))];
    if (m.empty()) m.assign(CELLS * CELLS / 8, 0);
    int cx = (gx & (ew::REGION - 1)) / CELL, cy = (gy & (ew::REGION - 1)) / CELL, b = cy * CELLS + cx;
    m[(size_t)(b >> 3)] |= (uint8_t)(1u << (b & 7));
  }
  // every cell within r tiles (a box) of a global tile
  void markAround(int32_t gx, int32_t gy, int r) {
    for (int32_t cy = ew::floorDiv(gy - r, CELL); cy <= ew::floorDiv(gy + r, CELL); cy++)
      for (int32_t cx = ew::floorDiv(gx - r, CELL); cx <= ew::floorDiv(gx + r, CELL); cx++) mark(cx * CELL, cy * CELL);
  }
  void clear() { regions.clear(); }
};
