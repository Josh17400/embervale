// M9 "Arcana & Mastery": the SPELLFX block's state inside the View (View::MagicFxState) and its friend Ops, shared by
// rpg/view/render_magic.cpp (the View's entry points), rpg/view/script_magicfx.cpp (the staging commands and
// `expect magicfx`) and rpg/view/render.cpp (a classic spell's sprite and explosion handed to the magic). SPELLFX lane.
#pragma once
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/view/magic_surface.h"
#include "rpg/view/view.h"

struct View::MagicFxState {
  mfx::World w;
  mfx::SurfLayer surf;
  MagicFxStats last, cur, peak;              // last frame's, this frame's (being counted), the most since the last reset
  double msWorst = 0, msSum = 0;             // the magic's draw time: the worst frame and the sum (since the last reset)
  int msFrames = 0;
  double updMs = 0, updWorst = 0;            // the magic's update (events, ageing, surface baking): last frame / worst
  // staged events and missiles (script_magicfx.cpp: `fx spell ...`), due at `at` seconds of the magic's own clock
  struct Staged { float at = 0; bool missile = false; Event e; Projectile pr; };
  std::vector<Staged> staged;
  float clock = 0;
  // synthetic missiles in flight (their spell is the view's own: the sim knows no def, so the view gives them their end)
  struct Flying { uint32_t spell = 0; Vec2 p, v; float mag = 1; bool seen = false; };
  std::vector<Flying> flying;
  // the classic spells' own sounds noted this frame (Ev::Sfx: Fireball, Frost, Heal, Explode), so the magic's voice
  // does not double them
  bool classicSfx[4] = {};
  float sfxT[(int)Sfx::COUNT] = {};          // the clock when each spell sound last played (a burst of events: one voice)
  float flashA = 0, liftA = 0;               // the frame's scene flash and light lift (World requests, consumed in draw)
  Color flashC, liftC;
  int lightsThisFrame = 0;
};

struct MagicFxOps {
  static View::MagicFxState& st(View& v);
  static Pix& pix(View& v);
  static Audio& audio(View& v);
  static void stage(View& v, float delay, const Event& e);              // a phase, delay seconds from now
  static void stageMissile(View& v, float delay, const Projectile& pr);  // a missile loosed then
  static void resetPeaks(View& v);
  static uint64_t musicStyle(View& v);
};

// (render.cpp) a classic spell's explosion: true when the magic's own impact already burst there this frame (the
// classic sprite is skipped); a classic spell sound noted (the magic's voice for it is dropped this frame)
bool mfxClaimExplode(View& v, Vec2 p);
void mfxNoteSfx(View& v, int sfx);
// (M9 fixer, render.cpp's prop pass) true when world px (x, y) lies inside a crater still fresh on the ground (a size-4
// impact: the grass, flowers and brush there burned away; trees and stones stand)
bool mfxScorched(View& v, float x, float y);
