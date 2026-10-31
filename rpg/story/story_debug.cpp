// SOLID 1.3 STORY: the test hooks of the story mechanics (syncNow, debugBout, debugEscort; story.h), used by rpg_test
// and the scripts. Moved from story_game.cpp; Engine::debugComplete / debugFail stay with the engine (story.cpp).
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include "engine/sound_ids.h"
#include "rpg/sim/game.h"
#include "rpg/sim/jobs_ops.h"
#include "rpg/sim/talk.h"
#include "rpg/story/dsl.h"
#include "rpg/story/story_internal.h"
#include "rpg/story/story_detail.h"
#include "rpg/world/coords.h"
#include "rpg/world/source.h"
#include "rpg/sim/stream.h"

namespace story {

void syncNow(Game& g) {
  if (!host().spawnHuman) return;   // (the host table is installed by the first story hook of the game)
  syncPersons(g);
  engineSync(g);
}

bool debugBout(Game& g, uint32_t inst, bool win) {
  engineSync(g);
  Engine& E = g.story;
  for (Engine::Bout& b : E.bouts_) {
    if (b.inst != inst) continue;
    int ai = b.actor >= 0 ? findA(g, b.actor) : -1;
    if (ai < 0) return false;
    Actor& a = g.actors[(size_t)ai];
    // face to face, a stride apart
    const Actor& p = g.pl();
    const Vec2 at = p.p + p.aim * 20.0f;
    if (ClimbOps::bodyFree(g, at, a.radius)) a.p = at;
    a.face = faceOf(p.p - a.p);
    a.aim = norm(p.p - a.p);
    if (b.phase == 0) { b.phase = 1; b.t = 0; armForBout(g, a); b.hp0 = a.maxHp; E.parts_[a.id] = Engine::P_BOUT; }
    if (b.phase != 1) return false;
    if (win) {
      a.hp = a.maxHp * 0.3f;
      float dmg = a.maxHp * 0.2f;
      boutBlow(g, a, dmg, g.pl().id);
      a.hp -= dmg;
    } else {
      Actor& pl = g.pl();
      pl.hp = std::min(pl.hp, 3.0f);
      float dmg = 10.0f;
      boutBlow(g, pl, dmg, a.id);
      pl.hp -= dmg;
    }
    return true;
  }
  return false;
}

bool debugEscort(Game& g, uint32_t inst) {
  engineSync(g);
  Engine& E = g.story;
  bool any = false;
  for (Engine::Walker& w : E.walkers_) {
    if (w.inst != inst) continue;
    int ai = w.actor >= 0 ? findA(g, w.actor) : -1;
    if (ai < 0) { syncPersons(g); engineSync(g); ai = w.actor >= 0 ? findA(g, w.actor) : -1; }
    if (ai < 0) continue;
    int tx = 0, ty = 0;
    if (!besidePlayer(g, 2.0f * TILE, tx, ty, (uint32_t)w.actor)) continue;
    Actor& a = g.actors[(size_t)ai];
    a.p = Vec2(tx * TILE + 8.0f, ty * TILE + 10.0f);
    a.navNext = -1;
    a.face = g.pl().face;
    w.cartSet = false; w.spd = 0; w.stuckT = 0;
    w.lastX = a.p.x; w.lastY = a.p.y;
    any = true;
    if (w.skin == dsl::ESCORT_DROVE)
      for (Actor& b : g.actors)
        if (isDroveBeast(b) && (uint32_t)((b.slot - DROVE_SLOT0) / DROVE_MAX) == inst % 1024u) {
          const int k = (b.slot - DROVE_SLOT0) % DROVE_MAX;
          b.p = ClimbOps::freeSpot(g, tx + (k % 3) - 1, ty + 1 + k / 3);
          b.navNext = -1;
        }
  }
  return any;
}

}  // namespace story
