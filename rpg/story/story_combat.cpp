// SOLID 1.3 STORY: the bouts and the story people in a fight: untouchable, boutBlow (the yield, the player stood
// down) and threatensTown. Moved from story_game.cpp; the bouts are set up in engineSync (story_actors.cpp).
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include "engine/audio.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/jobs_ops.h"
#include "rpg/sim/talk.h"
#include "rpg/story/dsl.h"
#include "rpg/story/story_internal.h"
#include "rpg/story/story_detail.h"
#include "rpg/world/coords.h"
#include "rpg/world/source.h"
#include "rpg/sim/stream.h"

namespace {
const char* const kYield[] = {"ENOUGH! I YIELD!", "HOLD! YOU HAVE ME.", "I YIELD. WELL FOUGHT.", "ENOUGH... ENOUGH. YOURS."};
const char* const kStood[] = {"STAY DOWN. IT'S DONE.", "THAT'S THE FIGHT. UP YOU GET.", "YIELD! ...GOOD. NO HARM DONE."};
}  // namespace

namespace story {

bool untouchable(const Game& g, const Actor& v, int attacker) {
  if (v.player) return false;
  if (!isStoryPerson(v)) return false;
  const uint8_t part = g.story.actorPart(v.id);
  if (part == Engine::P_YIELD || part == Engine::P_BEATEN) return true;   // (a bout is over: no blow lands)
  // a friendly story person under a camp's colours, or waiting inside a site: not the player's to strike
  if (!v.hostile && attacker == g.pl().id) return true;
  return false;
}

void boutBlow(Game& g, Actor& v, float& dmg, int attacker) {
  Engine& E = g.story;
  if (E.bouts_.empty() || dmg <= 0) return;
  if (v.player) {
    // the player at nothing in a bout: left on one knee with 1 HP (no death, nothing lost), the opponent stands down
    if (v.hp - dmg >= 1.0f) return;
    for (Engine::Bout& b : E.bouts_) {
      if (b.phase != 1 || b.actor < 0 || b.actor != attacker) continue;
      dmg = std::max(0.0f, v.hp - 1.0f);
      b.phase = 3; b.t = 0;
      v.iframes = std::max(v.iframes, 2.2f);
      const int ai = findA(g, b.actor);
      if (ai >= 0) {
        Actor& a = g.actors[(size_t)ai];
        standDown(a);
        a.knock = norm(a.p - v.p) * 40.0f;
        spoken(g, a, kStood[hash32((uint32_t)a.id) % 3u], rgba(255, 200, 150));
        E.parts_[a.id] = Engine::P_BEATEN;
      }
      return;
    }
    return;
  }
  if (!isStoryPerson(v)) return;
  for (Engine::Bout& b : E.bouts_) {
    if (b.actor != v.id || b.phase != 1) continue;
    const float floor = v.maxHp * 0.25f;
    if (v.hp - dmg > floor) return;
    // a quarter health: they yield (held there), drop to one knee, say so; no more blows land
    dmg = std::max(0.0f, v.hp - floor);
    b.phase = 2; b.t = 0;
    standDown(v);
    v.posture = art::Posture::Kneel;
    spoken(g, v, kYield[hash32((uint32_t)v.id * 31u) % 4u], rgba(255, 230, 160));
    E.parts_[v.id] = Engine::P_YIELD;
    return;
  }
}

bool threatensTown(const Game& g, const Actor& a) {
  if (!isStoryPerson(a)) return true;
  const uint8_t part = g.story.actorPart(a.id);
  return part != Engine::P_BOUT && part != Engine::P_YIELD && part != Engine::P_BEATEN;
}
}  // namespace story
