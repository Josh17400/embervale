// SOLID 1.3 STORY: how story people move: the walker (followStep, trailGoal), a drove's beasts (droveStep), and the
// hooks folkStep / hostileStep that ai.cpp calls. Moved from story_game.cpp; engineSync (story_actors.cpp) sets
// their parts, engineTick (story_game.cpp) runs the bouts' moments.
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

// a walker's way: the player's own footsteps (gates, bridges, the street they took), `back` px of trail behind them;
// one fallen behind picks the trail up where it is nearest
Vec2 trailGoal(const Game& g, const Actor& a, float back, float lane) {
  const Actor& p = g.pl();
  const std::vector<Vec2> tr = trailOf(g);
  Vec2 goal = p.p - p.aim * back;
  if (tr.empty()) return goal;
  float acc = len(p.p - tr.back());
  int j = (int)tr.size() - 1;
  while (j > 0 && acc < back) { acc += len(tr[(size_t)j] - tr[(size_t)j - 1]); j--; }
  if (len2(tr[(size_t)j] - p.p) > (back + 3.0f * TILE) * (back + 3.0f * TILE)) return goal;   // (a stale trail)
  goal = tr[(size_t)j];
  const float d2 = len2(a.p - p.p);
  if (d2 > (5.0f * TILE) * (5.0f * TILE)) {
    int k0 = std::max(0, (int)tr.size() - 80), bk = -1;
    float bd = 1e30f;
    for (int k = k0; k < (int)tr.size(); k++) { const float e = len2(tr[(size_t)k] - a.p); if (e < bd) { bd = e; bk = k; } }
    if (bk >= 0 && bk < j && bd < (8.0f * TILE) * (8.0f * TILE)) goal = tr[(size_t)std::min(bk + 2, j)];
  } else if (lane != 0.0f) {
    // close by: a little to one side of the footsteps (people walk beside the way, not in each other's prints)
    const Vec2 dir = norm(p.p - goal + Vec2(0.001f, 0.0f));
    goal += Vec2(-dir.y, dir.x) * lane;
  }
  return goal;
}

bool onDoorstep(const Game& g, int tx, int ty) {
  if (g.inside) return false;
  for (const Bldg& B : g.world.over.bldgs)
    if (B.doorX() == tx && (B.doorY() == ty || B.doorY() + 1 == ty)) return true;
  return false;
}

// the walker: behind the player at a walking distance, easing in and out of their pace, halting when they halt,
// catching up when left behind (a jog), the trail through gates and round corners; a wagon's cart trails them
void followStep(Game& g, Actor& a, story::Engine::Walker& w, float dt) {
  using namespace story;
  Actor& p = g.pl();
  if (g.mode == Mode::Dialogue && g.dlg.actor == a.id) { a.st = AState::Idle; a.face = faceOf(p.p - a.p); w.spd = 0; return; }
  const float T = (float)TILE;
  const float back = w.skin == dsl::ESCORT_WAGON ? 3.0f * T : w.skin == dsl::ESCORT_DROVE ? 2.5f * T : 2.0f * T;
  // a jump of the body (set down beside the player, a window shift's fix-up): the cart starts afresh behind them
  if (len2(a.p - Vec2(w.lastX, w.lastY)) > 40.0f * 40.0f) w.cartSet = false;
  const float d = len(p.p - a.p);
  const Vec2 goal = trailGoal(g, a, back, w.laneOff);
  const float gd = len(goal - a.p);
  const bool plMoving = p.st == AState::Walk || p.st == AState::Roll;
  const float ps = std::max(40.0f, p.speed * (g.homeRiding() ? g.homeSpeedMul() : 1.0f));
  float want = 0;
  // walking with the player: a touch under their pace at the place behind them, a touch over it when it falls back (so
  // the gap settles about a third of a tile behind the place: 2-3 tiles in all)
  if (plMoving) want = ps * std::clamp(0.85f + 0.45f * (gd / T), 0.85f, 1.5f);
  if (plMoving && d > back + 1.3f * T) want = std::max(want, ps * 1.3f);   // (a bend or a climb cost them ground)
  else if (d > back + 0.9f * T && gd > 0.8f * T) want = ps * std::clamp(0.45f + 0.15f * (gd / T), 0.45f, 1.1f);
  if (d > 6.0f * T) want = ps * (w.skin == dsl::ESCORT_WAGON ? 1.5f : 1.65f);   // left behind: a jog
  if (!plMoving && d < back * 0.9f) want = 0;
  // eased: no snapping into the player's pace or out of it (quicker to stop than to start, as people are)
  const float k = want > w.spd ? 2.6f : 5.0f;
  w.spd += (want - w.spd) * (1.0f - std::exp(-dt * k));
  if (w.spd < 2.5f && want <= 0) w.spd = 0;
  const Vec2 before = a.p;
  if (w.spd > 0.5f) {
    if (!ClimbOps::navStep(g, a, goal, w.spd, dt)) ClimbOps::moveActor(g, a, norm(goal - a.p) * (w.spd * dt));
    a.st = AState::Walk;
    // the stride follows the pace (a jog's quicker steps, a slow walk's slower ones: feet that do not slide)
    a.animT += dt * (std::clamp(w.spd / 52.0f, 0.55f, 1.7f) - 1.0f);
  } else {
    a.st = AState::Idle;
    if (d < 7.0f * T) a.face = faceOf(p.p - a.p);
    // never left standing in a doorway
    const int tx = (int)std::floor(a.p.x / T), ty = (int)std::floor((a.p.y - 2) / T);
    if (onDoorstep(g, tx, ty)) {
      const int side = (w.laneOff >= 0 ? 1 : -1);
      ClimbOps::moveActor(g, a, Vec2((float)side * 30.0f * dt, 0));
      a.st = AState::Walk;
    }
  }
  // stuck (a fence corner the way finder keeps failing at): the clock runs while it should move and does not
  const float moved = len(a.p - before);
  if (want > 8.0f && moved < want * dt * 0.15f) w.stuckT += dt;
  else w.stuckT = std::max(0.0f, w.stuckT - dt * 2.0f);
  if (w.stuckT > 1.2f) a.navNext = -1;
  const bool farOff = d > 22.0f * T;
  if (w.stuckT > 2.4f || farOff) {
    int tx = 0, ty = 0;
    if (besidePlayer(g, (farOff ? 9.0f : 3.0f) * T, tx, ty, (uint32_t)a.id * 7919u)) {
      a.p = Vec2(tx * T + 8.0f, ty * T + 10.0f);
      a.navNext = -1;
      w.cartSet = false;
      w.spd = ps * 0.6f;
    }
    w.stuckT = 0;
  }
  // the wagon: a hand cart on the driver's shafts, dragged along behind them (its heading is the tow's)
  if (w.skin == dsl::ESCORT_WAGON) {
    const float L = 21.0f;   // (the driver stands between the shafts, the cross bar at their hands)
    if (!w.cartSet) {
      const Vec2 bk = a.face == 0 ? Vec2(0, -1) : a.face == 1 ? Vec2(0, 1) : a.face == 2 ? Vec2(-1, 0) : Vec2(1, 0);
      w.cartX = a.p.x + bk.x * L; w.cartY = a.p.y + bk.y * L;
      w.cartAng = std::atan2(-bk.y, -bk.x);
      w.cartSet = true;
    }
    Vec2 c(w.cartX, w.cartY);
    const Vec2 dv = a.p - c;
    const float l = len(dv);
    if (l > 0.5f && std::fabs(l - L) > 0.05f) {   // the shafts are rigid: dragged on, or pushed back when the driver backs up
      const Vec2 nc = c + dv * ((l - L) / l);
      w.wheel += (l > L ? 1.0f : -1.0f) * len(nc - c) / 5.0f;
      c = nc;
    }
    if (l > 0.5f) {
      // the heading eases toward the tow (a cart swings round, it does not snap)
      const float want = std::atan2(dv.y, dv.x);
      float da = want - w.cartAng;
      while (da > 3.14159265f) da -= 6.2831853f;
      while (da < -3.14159265f) da += 6.2831853f;
      w.cartAng += da * (1.0f - std::exp(-dt * 7.0f));
    }
    w.cartX = c.x; w.cartY = c.y;
  }
  w.lastX = a.p.x; w.lastY = a.p.y;
}

// a beast of a drove: trailing its drover in a loose bunch, each its own pace and a little wander, gathering again when
// strung out; grazing when the drove stands
void droveStep(Game& g, Actor& a, float dt) {
  using namespace story;
  Engine& E = g.story;
  const uint32_t im = (uint32_t)((a.slot - DROVE_SLOT0) / DROVE_MAX);
  const int k = (a.slot - DROVE_SLOT0) % DROVE_MAX;
  const Engine::Walker* w = nullptr;
  for (const Engine::Walker& o : E.walkers_) if (o.skin == dsl::ESCORT_DROVE && o.inst % 1024u == im && o.actor >= 0) w = &o;
  const int di = w ? findA(g, w->actor) : -1;
  const uint32_t h = hash32((uint32_t)a.id * 0x9E3779B1u);
  const float own = (float)(h % 1000u) * 0.001f;
  if (di < 0) {   // no drove now: it grazes where it stopped
    a.st = AState::Idle;
    return;
  }
  const Actor& dr = g.actors[(size_t)di];
  const float T = (float)TILE;
  // the drove's heading: from the herd toward the drover (the way they are all going)
  const Vec2 toD = dr.p - a.p;
  const float dd = len(toD);
  const Vec2 head = dd > 0.5f ? toD * (1.0f / dd) : Vec2(0, 1);
  // its own place in the bunch: rows behind the drover, a side offset, a slow wander (each beast on its own clock)
  const float row = 1.6f + (float)(k / 2) * 1.1f + own * 0.6f;
  const float side = ((k & 1) ? 1.0f : -1.0f) * (0.6f + 0.5f * own) + std::sin(g.time * (0.35f + own * 0.4f) + own * 11.0f) * 0.45f;
  Vec2 goal = dr.p - head * (row * T) + Vec2(-head.y, head.x) * (side * T);
  // elbow room: a step away from a neighbour pressed against it
  for (const Actor& o : g.actors) {
    if (o.id == a.id || !isDroveBeast(o) || o.st == AState::Dead) continue;
    const Vec2 dv = a.p - o.p;
    const float l2 = len2(dv);
    if (l2 < 11.0f * 11.0f && l2 > 0.01f) goal += dv * (6.0f / std::sqrt(l2));
  }
  const float gd = len(goal - a.p);
  const bool drMoving = dr.st == AState::Walk;
  const float base = std::max(40.0f, w->spd * 1.05f);   // (the drover's own pace, a touch over it to keep up)
  float want = 0;
  if (gd > 0.7f * T || (drMoving && gd > 0.3f * T)) want = base * std::clamp(0.55f + 0.35f * (gd / T), 0.5f, 1.9f) * (0.9f + 0.25f * own);
  if (dd > 9.0f * T) want = base * 2.0f;   // strung out: a trot to gather
  float& spd = a.special;   // (the beast's eased pace: Actor::special is unused by a critter's own AI here)
  const float kk = want > spd ? 2.0f + own : 4.5f;
  spd += (want - spd) * (1.0f - std::exp(-dt * kk));
  if (spd < 2.0f && want <= 0) spd = 0;
  const Vec2 before = a.p;
  if (spd > 0.5f) {
    if (!ClimbOps::navStep(g, a, goal, spd, dt)) ClimbOps::moveActor(g, a, norm(goal - a.p) * (spd * dt));
    a.st = AState::Walk;
    a.animT += dt * (std::clamp(spd / 40.0f, 0.5f, 1.8f) - 1.0f);
    // stuck behind a fence or a bank (Actor::lifeT: the beast's stuck clock; the life lane never runs a drove's beast),
    // or strung out far behind: back into the bunch behind the drover (out of the view's middle)
    if (len(a.p - before) < spd * dt * 0.15f) a.lifeT += dt; else a.lifeT = std::max(0.0f, a.lifeT - dt * 2.0f);
    if (a.lifeT > 2.0f || dd > 13.0f * T || (dd > 9.0f * T && a.lifeT > 0.8f)) {
      const Vec2 at = dr.p - head * ((row + 0.5f) * T);
      a.p = ClimbOps::freeSpot(g, (int)std::floor(at.x / T), (int)std::floor(at.y / T));
      a.navNext = -1;
      a.lifeT = 0;
      spd = base;
    }
  } else {
    a.st = AState::Idle;   // (grazing the way it was going: the view plays its cropping frames now and then)
  }
}
}  // namespace

namespace story {

bool folkStep(Game& g, Actor& a, float dt) {
  Engine& E = g.story;
  const uint8_t part = E.actorPart(a.id);
  if (part == Engine::P_NONE) return false;
  switch (part) {
    case Engine::P_FOLLOW:
      for (Engine::Walker& w : E.walkers_)
        if (w.actor == a.id) { followStep(g, a, w, dt); return true; }
      return false;
    case Engine::P_BEAST: droveStep(g, a, dt); return true;
    case Engine::P_YIELD: case Engine::P_BEATEN:
      // the bout's moment (engineTick keeps the pose); they do not wander off in the middle of it
      a.st = AState::Idle;
      if (len2(g.pl().p - a.p) < (8.0f * TILE) * (8.0f * TILE)) a.face = faceOf(g.pl().p - a.p);
      return true;
    case Engine::P_INSIDE:
      // waiting where the story put them (a nook of the cave): they turn to the player as the player comes near
      a.st = AState::Idle;
      if (len2(g.pl().p - a.p) < (6.0f * TILE) * (6.0f * TILE)) a.face = faceOf(g.pl().p - a.p);
      return true;
    default: return false;   // (waiting for a bout: their usual round)
  }
}

bool hostileStep(Game& g, Actor& a, float dt) {
  Engine& E = g.story;
  if (isStoryPerson(a)) {
    const uint8_t part = E.actorPart(a.id);
    if (part == Engine::P_BOUT) {   // the bout's opponent fights the player only, and never gives up the fight
      a.target = g.pl().id; a.aggro = true; a.fleeing = false;
      a.thinkT = std::max(a.thinkT, 0.3f);
      return false;
    }
  }
  if (a.npc || a.player) return false;
  ew::Gid here = 0;
  if (g.inside) { if (g.subSite >= 0 && g.subSite < (int)g.world.sites.size()) here = g.world.sites[(size_t)g.subSite].id; }
  else if (a.site >= 0 && a.site < (int)g.world.sites.size()) here = g.world.sites[(size_t)a.site].id;
  if (!here) return false;
  // a running story's calm, or a finished story's lasting one (`do calm` in its end stage: MK_P1B_CALM)
  if (!E.calm_.count(here) && !g.marks.count(storyMarkKey(here, MK_P1B_CALM))) return false;
  // stood down: no attack, a slow wander about its ground (each on its own clock), watching the player pass
  a.aggro = false; a.target = -1; a.fleeing = false;
  if (a.st == AState::Windup || a.st == AState::Strike || a.st == AState::Recover) { a.st = AState::Idle; a.stT = 0; a.heavy = a.lunge = false; }
  a.thinkT -= dt;
  if (a.thinkT <= 0) {
    const uint32_t h = hash32((uint32_t)a.id * 2654435761u ^ (uint32_t)(g.time * 0.5f));
    a.thinkT = 2.5f + (float)(h % 400u) * 0.01f;
    if (h & 1u) a.goal = a.home + Vec2((float)((int)((h >> 4) % 7u) - 3) * TILE * 0.8f, (float)((int)((h >> 8) % 5u) - 2) * TILE * 0.8f);
    else a.goal = a.p;
  }
  const Vec2 d = a.goal - a.p;
  if (len2(d) > 9.0f) { ClimbOps::moveActor(g, a, norm(d) * std::min(a.speed * 0.35f * dt, len(d))); a.face = faceOf(d); a.aim = norm(d); a.st = AState::Walk; }
  else {
    a.st = AState::Idle;
    if (len2(g.pl().p - a.p) < (5.0f * TILE) * (5.0f * TILE)) { a.face = faceOf(g.pl().p - a.p); a.aim = norm(g.pl().p - a.p); }
  }
  return true;
}
}  // namespace story
