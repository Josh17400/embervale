// EMBERVALE art API: effects (painted in art_fx.cpp). Part of rpg/art.h (include that).
#pragma once
#include <cstdint>
#include <utility>
#include <vector>
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

// ---------------------------------------------------------------- P2c the living finish (docs/P2C_CONTRACTS.md 6b; STYLE_P2 27.7)
// Late gear moves as light over the drawn frame, on the body's own clock: the glow plane (RigSheet::glow) pulsed
// additively after the lighting, the edge spark on the grip-to-tip line, and motes. Pure timing (deterministic: Witness
// and rpg_test --doll-ladder --finish step it at 60 Hz); the view draws it at rig 2 only.
namespace finish {
struct Pulse { float period = 0, lo = 0, hi = 0; };   // period 0: no pulse
// (ladder i3: the blind panel, five of five, saw no animation at 3x: "about 11 sprite px change", "constant colour") the
// breath now spans most of the light: T6 runes 2.2 s, 0.10-1.00; a T5 gem 3.3 s, 0.10-0.60; below: none
Pulse pulseOf(int topTier);
// a(t) = lo + (hi - lo)(0.5 - 0.5 cos 2pi(t / P + phase))
float light(const Pulse& p, float t, float phase);
// the per-actor phase (0..1) from its id: two wearers never pulse in step
float actorPhase(uint32_t id);
// the edge spark: grip to tip in 0.33 s once every 3.3 s (offset by the phase); -1 while it rests, else 0..1 along the line
constexpr float kSparkEvery = 3.3f, kSparkRun = 0.33f;
float sparkAt(float t, float phase);
int sparkFrame(float u);                               // the 3-frame star (0..2) along the run
// motes: born every 0.45 s on a glow pixel, rising 8 px over 1.0 s in glow steps 3 then 2, fading; 3 alive at most
constexpr int kMaxMotes = 3, kMaxAnimated = 32;        // C13: 32 animated pixels a figure-frame at most (ladder i2: was 24; the owner asked for late gear that visibly lives)
constexpr float kMoteEvery = 0.45f, kMoteLife = 1.0f, kMoteRise = 8.0f;
struct Mote { float x = 0, y = 0, age = 0, rise = 0, alpha = 0; int step = 3; };
// glowPx: the current frame's glow pixels (frame coordinates); returns the motes alive at t (seeded per actor)
int motes(uint32_t seed, float t, const std::vector<std::pair<int, int>>& glowPx, Mote out[kMaxMotes]);
// (ladder i3) the Mythic's flame: embers born every 0.12 s on a glow pixel, licking up 10 px over 0.7 s with a sway, white-
// yellow, then amber, then red as they cool (the Mythic wears embers instead of motes); 6 alive at most
constexpr int kMaxEmbers = 6;
constexpr float kEmberEvery = 0.12f, kEmberLife = 0.7f, kEmberRise = 10.0f;
struct Ember { float x = 0, y = 0, age = 0, alpha = 0; int heat = 0; bool tall = false; };   // heat 0 white-yellow 1 amber 2 red
int embers(uint32_t seed, float t, const std::vector<std::pair<int, int>>& glowPx, Ember out[kMaxEmbers]);
// One figure-frame's living light (the view and doll_gallery share it, so the boards show what the game draws): additive
// light in frame px. Each glow pixel's light is its colour half-way to white at the figure's breath, rippled a third by the
// rune crawl ((x + y) / 2 mod 3 a third of a cycle apart: the light runs along the seams while the whole piece visibly
// brightens and dims); the halo (kHalo of that light, the glow's own colour) spills onto the 4 neighbours that are not glow
// (light, not pixels: the plane's pixel budget is unchanged); T6 motes, or the Mythic's embers. The spark is the caller's
// (it needs the frame's grip and tip).
constexpr float kHalo = 0.32f;
// (ladder i3) the gleam: a slanted band of light kGleamW px wide sweeps the whole T6 figure in 0.5 s once every 3.3 s, half a
// cycle after the spark (the polished-metal glint of a legend: the one big light the eye catches at 3x). The figure's own
// colours added back at kGleamA (white metal flashes, dark cloth barely moves); the Mythic's in its fire.
constexpr float kGleamEvery = 3.3f, kGleamRun = 0.5f, kGleamA = 0.8f;
constexpr int kGleamW = 3;
float gleamAt(float t, float phase);                   // -1 while it rests, else 0..1 across the figure
// the band's left column at row y of a w x h frame (rows rise to the right: 1 px every 2 rows)
inline int gleamCol(float u, int w, int h, int y) { return (int)std::floor(-h * 0.5f - 2.0f + (w + h * 0.5f + 4.0f) * u) + (h - y) / 2; }
struct GlowPx { int x = 0, y = 0; uint32_t c = 0; };
struct Light { float x = 0, y = 0, w = 1, h = 1; uint32_t c = 0; float a = 0; };   // c: rgba (alpha ignored)
float pixelLight(const Pulse& p, float t, float phase, int x, int y);
void frameLights(int topTier, bool mythic, uint32_t seed, float t, float phase, const std::vector<GlowPx>& px, std::vector<Light>& out);
bool enabled();                                        // rig 2 only (nothing draws at rig 1)
}  // namespace finish

}  // namespace art
