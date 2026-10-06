// EMBERVALE procedural sprite art. Everything is painted from code into Canvases (no art files).
// The renderer bakes these Canvases into textures once (or per distinct look) and blits them.
//
// Conventions
//   - 16px world tiles; 3/4 top-down view (the same perspective as classic SNES/GBA RPGs).
//   - Every sprite has a 1px dark outline (not pure black: a darkened version of the adjacent colour),
//     top-left light source, 3-4 tone shading, and a cohesive palette (see the palette block in rpg/art/art_internal.h).
//   - Transparent = 0 (alpha 0). Ground shadows are NOT part of sprites; the renderer draws them.
//   - Anchor: sprites are drawn with their bottom-centre at the entity's feet / the prop's base tile.
//   - Deterministic: the same inputs always produce identical pixels.
#pragma once
#include <cstdint>
#include "engine/pix.h"
#include "rpg/art/art_human.h"
#include "rpg/art/art_monsters.h"
#include "rpg/art/art_props.h"
#include "rpg/art/art_building.h"
#include "rpg/art/art_items.h"
#include "rpg/art/art_fx.h"
#include "rpg/art/art_culture.h"

namespace art {

// ---------------------------------------------------------------- palette helpers (shared with the renderer)
uint32_t shade(uint32_t c, float k);              // multiply rgb by k (k>1 brightens, clamped)
uint32_t mix(uint32_t a, uint32_t b, float t);    // lerp rgb, keeps a's alpha

}  // namespace art
