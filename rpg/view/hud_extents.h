// HudExtents (SOLID 4.2, View part), a value member of View (View::hudx_): where the HUD's pieces stood last frame
// (the location column, a boss bar, the quick bar, the name tag, the rise herald's strip), so spoken lines, banners and
// the quest compass keep clear of them. Written by hud.cpp's drawHud pieces, read by the plates and banners.
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "engine/pix.h"
#include "rpg/art.h"
#include "rpg/sim/game.h"

class View;

struct HudExtents {
  // (M5 fixer) the location / quest column's box as the HUD last drew it (screen): spoken lines keep out of it
  float hudColLeft_ = 1e9f, hudColBottom_ = 0;
  float colFoeFade_ = 0, colFoeT_ = -1;   // (C10 fix) the column's fade while the hero's foe stands under it, eased
  float bossBarX0_ = 0, bossBarX1_ = 0;
  float bossBarBottom_ = 0;   // (M6 fixer) the bottom of a world boss's top bar this frame (0: none): banners keep below it
  float quickBarR_ = 0;       // (M9 fixer) the desktop quick-cast bar's right end this frame (0: none drawn)
  // (M5) last frame's NPC name tag / interact prompt (drawHud): a spoken line's plate stacks above it, never over it
  bool tagOn_ = false;
  float tagX0_ = 0, tagY0_ = 0, tagX1_ = 0, tagY1_ = 0;
  // (M12 integrator) the herald's banner's strip last drawn (riseBanBottom_ 0: none): the quest compass keeps out of it
  float riseBanX0_ = 0, riseBanX1_ = 0, riseBanBottom_ = 0;
};
