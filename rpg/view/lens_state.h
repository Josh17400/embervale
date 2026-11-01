// LensState (SOLID 4.2, View part: P2a LENS), a value member of View (View::lens_): the hero's ring and its flare, the
// night lift (the people drawn this frame, lifted again after the light map), the pips, the living finish's queue and
// motes, the creator preview's swing clock, and lensUpdate (the zoom hand-over, the ring's flare, the v2 bake queue).
// Bodies: rpg/view/render.cpp, render_anim.cpp, hud.cpp.
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "engine/pix.h"
#include "rpg/art.h"
#include "rpg/sim/game.h"
#include "rpg/view/sprite_cache.h"   // SpriteCache::RigTex
class View;

struct LensState {
  using RigTex = SpriteCache::RigTex;
  // (P2c ladder, contracts 6b) the living finish of a rig-2 frame just drawn (T5-T6): its glow plane pulsed with the
  // actor's phase, the edge spark, the motes; queued and drawn additively after the lighting (render.cpp)
  void queueLivingFinish(View& v, const Actor& a, const art::HumanLook& L, const RigTex& rt, const art::FrameRect& f, const art::RigAnchors* an,
                         float x2, float y2, float pivX, float pivY, float alpha, bool strike = false);
  void queueLivingFinishId(View& v, int id, const art::HumanLook& L, const RigTex& rt, const art::FrameRect& f, const art::RigAnchors* an,
                           float x2, float y2, float pivX, float pivY, float alpha, bool strike, bool menu);
  size_t finishAuraEnd_ = 0;   // (P2c SWITCH) where the last queued figure's aura lights end in the finish queue
  float menuSwingT0_ = -1;    // the creator preview's swing: when the current blow began
  // (polish i1) the living finish's world-space motes / embers, one set per actor id (dropped 2 s after its last draw)
  std::unordered_map<int, art::finish::Particles> finishParts_;
  // (P2a LENS) the ring's flare (s left) and what triggers it; the people drawn in the sorted pass this frame (the night
  // lift re-adds them after the light map)
  float ringFlare_ = 0, ringHpWas_ = -1;
  bool ringCrowdWas_ = false;
  bool ringOn_ = false;   // drawn this frame at (ringX_, ringY_) (world-view px) with ringA_: the night pass lifts it again
  float ringX_ = 0, ringY_ = 0, ringA_ = 0;
  const Tex* ringTex_ = nullptr;
  std::vector<Pix::BlitRec> liftRecs_;
  void drawRing(View& v, const Game& g, const Actor& a, Vec2 cam);
  void drawNightLift(View& v, const Game& g, float dark);
  void drawPips(View& v, const Game& g);
  void lensUpdate(View& v, Game& g, float dt);
};
