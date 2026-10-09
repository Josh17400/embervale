// EMBERVALE art API: effects (painted in art_fx.cpp). Part of rpg/art.h (include that).
#pragma once
#include <cstdint>
#include "engine/pix.h"

namespace art {

// ---------------------------------------------------------------- effects (small, animated, frames side by side)
enum class Fx : uint8_t {
  Slash,       // 4 frames 24x24, white-silver arc sweeping clockwise, drawn facing right (renderer flips/rotates by choosing variant)
  SlashDown,   // 4 frames 24x24, arc for facing down
  SlashUp,     // 4 frames 24x24, arc for facing up
  Arrow,       // 1 frame 12x3 pointing right
  ArrowDown,   // 1 frame 3x12 pointing down
  Fireball,    // 4 frames 12x12
  Explosion,   // 6 frames 32x32
  Sparkle,     // 4 frames 8x8 (pickups, level up)
  Blood,       // 4 frames 8x8 red splash
  Dust,        // 4 frames 8x8 footstep/roll dust
  Frost,       // 4 frames 12x12 ice bolt
  Heal,        // 4 frames 16x16 green rising crosses
  COUNT
};
int fxW(Fx f);
int fxH(Fx f);
int fxFrames(Fx f);
Canvas fxSprite(Fx f);

// ---------------------------------------------------------------- M6 Steel: elite auras (VISION_PLAN 7.6)
// An elite's, a champion's or a named unique's aura in its affix colour: AURA_FRAMES frames side by side, each w x h
// (h = w / 2 + 8): a flat ellipse of light on the ground round the feet (the bottom w / 2 rows, in the 3/4 view's
// foreshortening) plus motes rising above it. The renderer draws it additively, the ground part under the body.
constexpr int AURA_FRAMES = 6;
int auraH(int w);
Canvas auraSprite(uint32_t color, int w);

}  // namespace art
