// Camera (camera.h): the zoom step. The projection, the HUD bands' insets and the follow stay in render.cpp.
#include <algorithm>
#include <cstdlib>
#include <vector>
#include "engine/audio.h"
#include "rpg/view/camera.h"
#include "rpg/view/screen.h"
#include "rpg/view/view.h"

void Camera::zoomStep(View& v, int dir) {
  const std::vector<int> lv = screen::zoomLevels();
  if (lv.empty() || dir == 0) return;
  const int cur = screen::worldScale();
  int i = 0;
  for (int k = 0; k < (int)lv.size(); k++) if (std::abs(lv[(size_t)k] - cur) < std::abs(lv[(size_t)i] - cur)) i = k;
  const int ni = std::clamp(i + (dir > 0 ? 1 : -1), 0, (int)lv.size() - 1);
  screen::Settings s = screen::get();
  const int z = std::clamp(screen::zoomLevelOf(lv[(size_t)ni]), screen::kZoomMin, screen::kZoomMax);
  if (ni == i && z == s.zoom) return;   // already at the end of the screen's levels
  s.zoom = z;
  screen::set(s);
  if (v.audio_) v.audio_->play(Sfx::MenuMove, dir > 0 ? 1.12f : 0.9f, 0.45f);
}
