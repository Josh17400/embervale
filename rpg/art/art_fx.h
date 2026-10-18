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
  // ---- P2a Clear Sight (VISION_PLAN 15.36 item 5, STYLE_P2 6 and 12): one swing effect per weapon family. Phase A
  //      stubs copy Slash (24x24, 4 frames); RIG-ARMS paints them at their own sizes and frame counts (fxW / fxH /
  //      fxFrames always tell the truth) and ANIM times them by the family's pace.
  SwingArc,      // sword and dagger: the Steel Arc crescent (32 x 32, 4 frames)
  HeavyArc,      // greatsword: the Heavy Arc with a trailing ghost blade (48 x 40, 5 frames)
  ChopArc,       // axe: a short heavy arc (28 x 28, 4 frames); Chips on a hit
  SmashRing,     // mace / hammer: the impact ring and dust (32 x 16, 5 frames); sparks on metal
  StabGlint,     // dagger stab: the Glint at the tip (12 x 6, 2 frames)
  ThrustStreak,  // spear: the Thrust Streak along the aim (24 x 5, 3 frames)
  BluntSwish,    // staff / walking stick: the Swish (28 x 28, 3 frames, no glint)
  PunchStar,     // fists: the Punch Star (9 x 9, 2 frames)
  Twang,         // bow release: the Twang at the bow (8 x 8, 2 frames)
  COUNT
};
int fxW(Fx f);
int fxH(Fx f);
int fxFrames(Fx f);
Canvas fxSprite(Fx f);
// (P2a style slice) the effect painted natively into fxFrames(f) cells of w x h, so a bigger copy (the finisher's 1.2x,
// STYLE_P2 6) keeps one pixel size instead of a sprite stretched at runtime. Only the Steel Arc (SwingArc) scales its
// art with the cell; any other effect returns an empty canvas (the renderer then stretches fxSprite as before).
Canvas fxSpriteSized(Fx f, int w, int h);

// ---------------------------------------------------------------- M6 Steel: elite auras (VISION_PLAN 7.6)
// An elite's, a champion's or a named unique's aura in its affix colour: AURA_FRAMES frames side by side, each w x h
// (h = w / 2 + 8): a flat ellipse of light on the ground round the feet (the bottom w / 2 rows, in the 3/4 view's
// foreshortening) plus motes rising above it. The renderer draws it additively, the ground part under the body.
constexpr int AURA_FRAMES = 6;
int auraH(int w);
Canvas auraSprite(uint32_t color, int w);

}  // namespace art
