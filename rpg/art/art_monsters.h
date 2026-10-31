// EMBERVALE art API: monsters (painted in art_monsters.cpp). Part of rpg/art.h (include that).
#pragma once
#include <cstdint>
#include "engine/canvas.h"
#include "rpg/core/art_ids.h"

namespace art {

// ---------------------------------------------------------------- monsters
// Sheet layout: one row, MONSTER_FRAMES cells of monsterCellW x monsterCellH, facing RIGHT (renderer flips for left).
//   columns: 0-3 move cycle, 4 attack wind-up, 5 attack strike, 6 hurt, 7 dead (lying on the ground)
constexpr int MONSTER_FRAMES = 8;
int monsterCellW(Monster m);
int monsterCellH(Monster m);
Canvas monsterSheet(Monster m);

// the species' sheet with the look's recolour and overlays (same layout and cell size as monsterSheet(base));
// classic() looks are monsterSheet(base) pixel for pixel
Canvas monsterSheetLook(const MonsterLook& look);
// the cell size of a look's sheet: the species' own, or the great form's (MO_GREAT) bigger cell
int lookCellW(const MonsterLook& look);
int lookCellH(const MonsterLook& look);

// (M6 BEASTS) the tint is a palette-ramp recolour: every hide pixel takes the tint ramp's hue at its own ramp step and
// luminance (eyes, teeth, claws and fire keep theirs). The overlays sit on per-frame anchors the painters report (the
// head, the eye, the torso), so horns stay on the head through the wind-up, the hurt frame and the body lying dead.
//
// Golem materials: a golem's tint picks its material, painted as such (not washed): 0 = stone (lichen, cracks, a cyan
// rune), and the tints below (or any tint golemMaterial maps to one; another tint recolours the stone).
constexpr uint32_t GOLEM_CRYSTAL = 0xFFE6B46Eu;   // rgba(110, 180, 230): faceted crystal, shards on the back, a magenta rune
constexpr uint32_t GOLEM_CLAY = 0xFF4668B0u;      // rgba(176, 104, 70): smoothed clay, incised glyphs, an amber rune
constexpr uint32_t GOLEM_IRON = 0xFF706460u;      // rgba(96, 100, 112): riveted iron plates, a furnace heart
int golemMaterial(uint32_t tint);                 // 0 stone, 1 crystal, 2 clay, 3 iron
// The emissive pixels of a look (glowing eyes, ember cracks, crystal glints, a golem's rune and eye slits) on their own
// sheet (same layout, transparent elsewhere): the view adds it after the night's darkness so they read in the dark.
bool monsterLookGlows(const MonsterLook& look);   // false: its glow sheet would be empty (skip it)
Canvas monsterGlowLook(const MonsterLook& look);

}  // namespace art
