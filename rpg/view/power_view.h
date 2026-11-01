// PowerView (SOLID 4.2, View part: the M8b POWERS lane), a value member of View (View::powers_): the worn powers'
// effects in the world and their ageing. Bodies: rpg/view/render_powers.cpp.
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "engine/pix.h"
#include "rpg/art.h"
#include "rpg/sim/game.h"

class View;

struct PowerView {
  // M8b Treasure (POWERS lane; rpg/view/render_powers.cpp). The worn powers' effects in the world: a lightning arc,
  //      a fire patch, a burn, the thorns aura, the time-slow tint, a lifesteal mote, the phoenix's rebirth, a falling
  //      star, a spectral wolf's shimmer (Ev::Power: a = powers::Fx, p / q / f / id). The POWERS lane owns these members
  //      (it may add more state inside PowerFx); every other M8b view change belongs to the VIEW lane.
  struct PowerFx { int kind = 0; Vec2 p, q; float t = 0, life = 0.4f, mag = 1; uint32_t follow = 0; uint32_t seed = 0; };
  std::vector<PowerFx> powerFx_;
  void powerEvent(View& v, Game& g, const Event& e);    // Ev::Power (update's event pass)
  void powerUpdate(Game& g, float dt);         // age the effects (update, after partyUpdate)
};
