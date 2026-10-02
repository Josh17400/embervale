// Simple autoplay bot used by the headless tool and the --bot app mode (tests, screenshots, balance).
#pragma once
#include "game/game.h"

struct Bot {
  Rng r{99};
  float phase = 0;
  Input act(Game& g, float dt) {
    Input in;
    phase += dt;
    Vec2 c; int n = 0;
    for (const Enemy& e : g.enemies)
      if (e.maxhp > 0 && len2(e.p - g.head) < 600.0f * 600.0f) { c += e.p; n++; }
    float cyc = std::fmod(phase, 3.0f);
    if (n > 0 && cyc > 2.4f) { c *= 1.0f / n; in.steer = norm(c - g.head); }   // dive toward the crowd
    else in.steer = fromAngle(g.heading + 0.9f);                                // carve a circle
    in.boost = (n > 25 && std::fmod(phase, 4.0f) < 0.05f);
    return in;
  }
};
