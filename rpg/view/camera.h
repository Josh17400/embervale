// Camera (SOLID 4.2, View part 3): where the view stands, a value member of View (View::camera_). The camera's top-left
// in world px (cam_), the projection between the world layer and the UI layer (toUi, toWorld, uiPerWorld, worldCam;
// View forwards the frozen P2a names), the HUD's bands seen from the world (uiInsetsToWorld), the zoom (zoomStep and
// the zoom's hand-over, lensViewW_ / lensViewH_ / lensD_), the pinch and the wheel, the follow (updateCamera) and the
// heavy finisher's nudge. Included by rpg/view/view.h. The float projection and the follow stay in render.cpp beside
// the camera's snap state (strict identity: their code is compiled where it always was); camera.cpp holds the zoom step.
#pragma once
#include <cstdint>
#include "engine/mathx.h"

class View;
class Game;

struct Camera {
  // (item 3) the projection between the world layer and the UI layer (engine/pix.h layers): a world point in map px (as
  // Actor::p) to UI logical px (what Pix::text and taps use) and back; how many UI px one world px spans. Every text drawn
  // over the world goes through toUi (STYLE_P2 18: text never lives in the world layer), every tap on the world through
  // toWorld. World-anchored UI call sites LENS moves onto them: hud.cpp View::draw (speech plates, combat numbers),
  // drawHud (the name tag and E: TALK, door and quest labels); party_hud.cpp drawBanter; the elite name plates' text
  // (render.cpp, the beasts::plates pass after the lighting); buildmode.cpp taps; script_test.cpp label taps.
  Vec2 toUi(const View& v, Vec2 world) const;
  Vec2 toWorld(const View& v, Vec2 ui) const;
  float uiPerWorld(const View& v) const;
  Vec2 worldCam(const View& v) const;   // the world layer's camera this frame: floor(camera + shake), what every world draw subtracts
  void zoomStep(View& v, int dir);
  Vec2 cam_;                 // top-left of the view in world pixels
  bool updateCamera(View& v, Game& g, float dt, float& VW, float& VH);   // true: the camera is held (p2b.camHold)
  float nudgeT_ = -1, nudgeLen_ = 0.18f;   // (P2c SWITCH) the camera nudge on a heavy finisher's impact: its clock (-1 off)
  Vec2 nudgeDir_;
  bool nudgeWas_ = false;
  int lensViewW_ = 0, lensViewH_ = 0, lensD_ = 0;
  struct Pinch { bool on = false; uint64_t a = 0, b = 0; Vec2 pa, pb; float d0 = 0; int done = 0; } pinch_;
  float wheelAcc_ = 0, wheelT_ = -1;
  // UI-layer insets (left, top, right, bottom in UI px from the UI canvas's edges: the HUD's bands) as world-layer insets
  // (world px from the world view's edges)
  void uiInsetsToWorld(const View& v, float l, float t, float r, float b, float out[4]) const;
};
