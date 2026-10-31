// EMBERVALE art API: humans (painted in art_human.cpp). Part of rpg/art.h (include that).
#pragma once
#include <cstdint>
#include "engine/canvas.h"
#include "rpg/core/art_ids.h"

namespace art {

// ---------------------------------------------------------------- humans (player, NPCs, bandits)
// Sheet layout: cells of HUMAN_W x HUMAN_H, HUMAN_FRAMES columns x 3 rows.
//   rows:    0 = facing down (toward camera), 1 = facing up (away), 2 = facing right (flip for left)
//   columns: 0 idle, 1-4 walk cycle, 5 attack wind-up (arm raised back), 6 attack strike (arm forward), 7 hurt
// The character is ~11-13 px wide and ~20-22 px tall, standing on the bottom row of the cell.
constexpr int HUMAN_W = 16, HUMAN_H = 24, HUMAN_FRAMES = 8;

Canvas humanSheet(const HumanLook& look);
// M6 Steel: the 5-step metal ramp (0 deepest shadow .. 4 highlight) of an alloy's light / dark keys and its sheen
// (HumanLook::sheen: cult::Sheen + 1; 0 plain, e.g. bronze). The armour painters and the item icons share it so a
// piece looks the same metal on the doll and in the pack.
void metalRampKeys(uint32_t light, uint32_t dark, uint8_t sheen, uint32_t out[5]);
// one cell of the sheet (row = facing, frame = column) before the 1px outline: tests check what is painted where
Canvas humanCellRaw(const HumanLook& look, int row, int frame);

}  // namespace art
