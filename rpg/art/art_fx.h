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
// Late gear moves as light over the drawn frame, on the body's own clock: the glow plane (RigSheet::glow) breathing
// additively after the lighting with its halo, the T6 aura breathing round the silhouette (the Mythic's a flame), the edge
// sweep running grip to tip, the gleam, and world-space motes / embers that trail the walker. Pure timing (deterministic:
// Witness and rpg_test --doll-ladder --finish step it at 60 Hz); the view draws it at rig 2 only. (polish i1: the blind
// panel found the i3 finish "invisible at game scale": 1-2 px embers, a 3 px gem twinkle; every piece below reads at 1x.)
namespace finish {
struct Pulse { float period = 0, lo = 0, hi = 0; };   // period 0: no pulse
// T6 runes 2.2 s, 0.10-1.00; a T5 gem 3.3 s, 0.10-0.60; below: none
Pulse pulseOf(int topTier);
// a(t) = lo + (hi - lo)(0.5 - 0.5 cos 2pi(t / P + phase))
float light(const Pulse& p, float t, float phase);
// the per-actor phase (0..1) from its id: two wearers never pulse in step
float actorPhase(uint32_t id);
// the edge sweep (a T6 piece in hand): a white head with a glowing tail runs grip to tip in 0.4 s once every 2.75 s (offset
// by the phase) and ends in a star at the tip; -1 while it rests, else 0..1 along the line
constexpr float kSparkEvery = 2.75f, kSparkRun = 0.40f;
constexpr int kSweepTail = 6;                          // px of glowing tail behind the head
float sparkAt(float t, float phase);
int sparkFrame(float u);                               // the star's frame (0..2) along the run
// C13 (polish i1): sprite-bright animated pixels a figure-frame (light over kSoftMax, or a glow pixel: the glow plane, the
// sweep's head and tail, the Mythic's flame roots, the particles' cores) at most kMaxAnimated; soft light (the halo, the
// aura, the flame's tips) at most kMaxSoft px, each kSoftMax or less (was 32 animated px: the panel saw no animation)
constexpr int kMaxAnimated = 128, kMaxSoft = 320;   // (polish i3: 112 -> 128 for the Mythic's fire rim and lit crown)
constexpr float kSoftMax = 0.45f;
// the halo: the glow's light spills onto the non-glow pixels within 2 px (kHalo at 1 px, kHalo2 at 2 px): a band, not a dot
constexpr float kHalo = 0.42f, kHalo2 = 0.16f;
// the aura (T6): a 2 px band of warm white-gold light standing one clear pixel off the silhouette round the helm and
// shoulders, breathing with the runes (kAura x breath at 2 px, half at 3 px: polish i2, the dark outline keeps its edge);
// the Mythic: separate flame tongues up to 4.8 px off its crown (kFlame1 at the root; no ring down the sides since polish
// i2). kFlame2 (i1's fire ring) is kept for its readers and no longer drawn.
constexpr float kAura = 0.40f, kFlame1 = 0.58f, kFlame2 = 0.24f;
// (polish i3) the rim: since i3 the T6's light is drawn ON the figure, one pixel inside the dark outline (the panel saw the
// off-figure aura band as "a blurry selection halo"): the upper-left rim at kRim x breath (right side 60 %), the crown to
// 62 % of the figure, fading 60 % down it; the Mythic's fire rim up both flanks from 30 % to 86 % (below the face) on a
// flame noise (kRimM at its peak). At the peak the rim is sprite-bright (over kSoftMax: it counts in kMaxAnimated), fading
// to soft light down the body. kAura (the i2 band) is kept for its readers and no longer drawn.
constexpr float kRim = 0.75f, kRimM = 0.85f;
// the Mythic's drawn flame crown (its fire-coloured pixels, Mask value 2, in the upper 30 %) lit at kCrown x a flicker
constexpr float kCrown = 0.42f;
// the gleam: a slanted band of light kGleamW px wide sweeps the whole T6 figure in 0.5 s once every 3.3 s, half a cycle
// after the spark. The figure's own colours added back at kGleamA; the Mythic's in its fire.
constexpr float kGleamEvery = 3.3f, kGleamRun = 0.5f, kGleamA = 0.8f;
constexpr int kGleamW = 3;
float gleamAt(float t, float phase);                   // -1 while it rests, else 0..1 across the figure
// the band's left column at row y of a w x h frame (rows rise to the right: 1 px every 2 rows)
inline int gleamCol(float u, int w, int h, int y) { return (int)std::floor(-h * 0.5f - 2.0f + (w + h * 0.5f + 4.0f) * u) + (h - y) / 2; }
struct GlowPx { int x = 0, y = 0; uint32_t c = 0; };
struct Light { float x = 0, y = 0, w = 1, h = 1; uint32_t c = 0; float a = 0; };   // c: rgba (alpha ignored)
float pixelLight(const Pulse& p, float t, float phase, int x, int y);
// a frame's silhouette, one byte a pixel (frame coordinates: 0..w-1, 0..h-1, the frame rect's own origin)
struct Mask {
  int w = 0, h = 0;
  std::vector<uint8_t> on;   // 0 off, 1 on, 2 on and fire-coloured (polish i3: the Mythic's crown)
  bool at(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h && on[(size_t)y * w + x]; }
};
Mask maskOf(const Canvas& atlas, int fx, int fy, int fw, int fh);
// one figure-frame's living light, frame-relative (the view and doll_gallery both call it): the glow pixels' breath x the
// rune crawl, the 2 px halo, the T6 aura / the Mythic's flame ring (needs the mask), the edge sweep and its tip star (needs
// the grip and tip; `strike` flashes the star on an impact frame). The particles and the gleam are drawn apart.
struct Figure {
  int topTier = 0;
  bool mythic = false, bladeT6 = false, strike = false, line = false;
  float t = 0, phase = 0;
  float gx = 0, gy = 0, tx = 0, ty = 0;                // grip and tip (frame coordinates) when `line`
  const std::vector<GlowPx>* glow = nullptr;
  const Mask* mask = nullptr;
};
void figureLights(const Figure& f, std::vector<Light>& out);
// (i3's entry point, kept for its callers) the glow, halo and aura with no mask or line
void frameLights(int topTier, bool mythic, uint32_t seed, float t, float phase, const std::vector<GlowPx>& px, std::vector<Light>& out);
// ---- particles in WORLD space (the caller keeps one Particles per actor): born on the current frame's glow pixels (the
// Mythic's also at the blade's tip while it strikes: a trail of fire along the swing), they rise and sway where they were
// born, so a walker leaves them behind. T6 motes: every 0.45 s, 1.2 s, rising 10 px, 4 alive; the Mythic's embers: every
// 0.07 s, 0.8 s, licking 13 px up, 12 alive, white-yellow -> amber -> red with a soft glow round the hot ones.
constexpr int kMaxMotes = 4, kMaxEmbers = 12;
constexpr float kMoteEvery = 0.45f, kMoteLife = 1.2f, kMoteRise = 10.0f;
constexpr float kEmberEvery = 0.07f, kEmberLife = 0.8f, kEmberRise = 13.0f;
struct Particles {
  struct P { float x = 0, y = 0, born = 0; uint32_t h = 0; };
  std::vector<P> live;
  long long lastK = 0;
  float lastT = -1;
  bool mythic = false;
};
// advance to t: every birth tick since the last call (6 at most) takes a source at (ox, oy) + a glow pixel, upper ones
// first; with `tipOn` (a Mythic striking) every other birth takes the tip. A gap over 1 s starts afresh.
void particlesStep(Particles& ps, uint32_t seed, float t, bool mythic, float ox, float oy, const std::vector<GlowPx>& glow, bool tipOn,
                   float tipX, float tipY);
int particlesAlive(const Particles& ps, float t);
float particleRise(const Particles& ps, size_t i, float t);   // px risen by particle i at t (C13)
void particleLights(const Particles& ps, float t, uint32_t hot, std::vector<Light>& out);   // world coordinates
bool enabled();                                        // rig 2 only (nothing draws at rig 1)
}  // namespace finish

}  // namespace art
