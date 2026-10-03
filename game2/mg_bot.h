// Autoplay bot for HOLDLINE: used by the headless tool and the app's --bot mode (tests, screenshots).
#pragma once
#include "game2/mg_game.h"

struct MBot {
  float mergeT = 0, stuckT = 0, wiggleT = 0;
  Vec2 lastPos, wiggleDir;
  // Returns the hero move vector and performs inn / merge actions as needed.
  Vec2 act(MGame& g, float dt) {
    mergeT += dt;
    Vec2 target = g.villages[0].p;
    float bd = 1e9f; int wid = -1;
    for (const Unit& u : g.units) if (u.st == UState::Wild) { float d = len2(u.p - g.hero.p); if (d < bd) { bd = d; wid = u.id; } }
    bool partyFull = g.partyCount() >= MAX_PARTY;
    bool home = g.raidActive || g.raidTimer < 25.0f;   // be back to defend
    if (!home && !partyFull && wid >= 0) target = g.unitById(wid)->p;
    else {
      if (g.nearVillage(g.hero.p) >= 0) g.lodgeParty();
      target = g.villages[0].p + Vec2(0, 20);
    }
    int owned = 0;
    for (const Unit& u : g.units) if (u.st != UState::Wild) owned++;
    if (!home && owned >= 6 && !partyFull) {
      float cd = 1e9f;
      for (const Camp& c : g.camps) if (!c.cleared) { float d = len(c.p - g.hero.p); if (d < cd) { cd = d; target = c.p; } }
    }
    if (mergeT > 4.0f) {
      mergeT = 0;
      g.autoMerge();
      if (g.gold >= g.hireCost() && len(g.hero.p - g.villages[0].p) < VILLAGE_R) g.hire();
    }
    if (g.hero.dead) g.hero.respawn = 0.0f;
    // no pathfinding: if we stop making progress, wander off in a random direction for a moment
    stuckT += dt;
    if (stuckT > 1.5f) {
      stuckT = 0;
      if (len(g.hero.p - lastPos) < 8.0f && wiggleT <= 0) { wiggleT = 1.2f; wiggleDir = fromAngle(Rng((uint64_t)(g.time * 977)).range(0, TAU)); }
      lastPos = g.hero.p;
    }
    if (wiggleT > 0) { wiggleT -= dt; return wiggleDir; }
    Vec2 d = target - g.hero.p;
    return len(d) < 4 ? Vec2() : norm(d);
  }
};
