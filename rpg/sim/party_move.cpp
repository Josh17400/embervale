// M8 Fellowship: how the companions move (rpg/sim/party.h; owner: never stuck more than 30 tiles behind, pathing
// through cities, ramps and doors). FELLOWS lane.
//   - the formation: FOLLOW_TILES behind the player, members side by side; a run when they fall behind;
//   - the trail: the player's own footsteps on this map (Party::trail, a crumb a tile apart). A member whose way to the
//     formation is shut (a city wall between, a gate, a bridge, a terrain ramp, a corner the short path search cannot
//     see round) walks the player's footsteps instead, so wherever the player went they can follow;
//   - the catch-up: beyond CATCH_UP_TILES they are put back on the player's trail OUT OF SIGHT (past the screen's edge,
//     Game::sleepHalfW / sleepHalfH) and run in along it: they never pop into view;
//   - map changes (doors, stairs, caves, journeys, the carriage, a respawn, a load): the members with the player come
//     through with them and stand beside them (a downed member is carried: Member::downT x CARRY_FACTOR);
//   - waiting members stay where told (an overworld tile, or a building / cave map and floor) and are there when the
//     player comes back, after a save and a load too;
//   - living at the player's house: in the house when the player is there (a seat by day, a meal at mealtimes, a spare
//     bed at night), out in the yard by daylight when the player comes by;
//   - riding: a member with a horse (Member::horse, MF_MOUNTED) rides beside the mounted player at the horse's pace
//     (Actor::posture Ride) and gets down with them; one without runs and catches up.
#include <algorithm>
#include <cmath>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/home.h"
#include "rpg/sim/party.h"

using party::Member;
using party::State;

namespace {
constexpr float T = (float)TILE;
constexpr size_t TRAIL_MAX = 224;
int tileOf(float v) { return (int)std::floor(v / T); }
bool freeAt(const Game& g, Vec2 p, float r) { return HomeOps::bodyFree(g, p, r); }
// a body walks straight from a to b (the box clear along the way, as navStep's straight walk)
bool walkable(const Game& g, Vec2 a, Vec2 b, float r, bool (*free)(const Game&, Vec2, float)) {
  const Vec2 d = b - a;
  const float l = len(d);
  for (float t = 4.0f; t < l; t += 4.0f) if (!free(g, a + d * (t / l), r)) return false;
  return true;
}
}  // namespace

// ---------------------------------------------------------------- the formation
Vec2 PartyOps::slotBehind(const Game& g, int mi) {
  const Actor& p = g.pl();
  const Vec2 back = norm(Vec2(0, 0) - p.aim + Vec2(0.0001f, 0.0f));
  const Vec2 side(-back.y, back.x);
  // the members with the player take the slots in their order (waiting ones leave theirs free)
  int k = 0;
  for (int i = 0; i < mi && i < (int)g.party.members.size(); i++) if (g.party.members[(size_t)i].state == State::With) k++;
  const bool mounted = g.homeRiding() && mi < (int)g.party.members.size() && g.party.members[(size_t)mi].horse != 0 &&
                       (g.party.members[(size_t)mi].flags & party::MF_MOUNTED);
  // (fixer M8 r1, review: "following companions stack on top of each other and the player") a whole tile out to each
  // side at least: a 12 px figure never overlaps the player's or the next member's
  float s = (k % 2 == 0 ? 1.0f : -1.0f) * ((mounted ? 1.4f : 1.0f) + 0.9f * (float)(k / 2));
  float b = mounted ? 0.7f : party::FOLLOW_TILES;
  // (fixer M8 r1) Defend: close at the player's shoulder (the leash is 3 tiles even while the player runs on)
  if (!mounted && mi < (int)g.party.members.size() && g.party.members[(size_t)mi].stance == party::Cmd::Defend) {
    b = 0.9f;
    s = (k % 2 == 0 ? 1.0f : -1.0f) * 1.0f;
  }
  return p.p + back * (b * T) + side * (s * T);
}

Vec2 PartyOps::spotBehind(Game& g, int mi) {
  const Vec2 v = slotBehind(g, mi);
  const Vec2 s = g.freeSpot(tileOf(v.x), tileOf(v.y - 2));
  // (a wall, a stairwell or a doorway behind the player: beside the player instead, where the way to them is open)
  if (len2(s - g.pl().p) < (3.0f * T) * (3.0f * T) && walkable(g, s, g.pl().p, 4.5f, freeAt)) return s;
  const Vec2 near = g.freeSpot(tileOf(g.pl().p.x), tileOf(g.pl().p.y - 2) + 1);
  return near;
}

// where a member coming through with the player stands (a door, the stairs, a journey's end): at the player's side
// (never in the doorway behind them, under the roof), on open ground with a clear step to the player
static Vec2 arriveSpot(Game& g, int mi) {
  const Actor& p = g.pl();
  const Vec2 f = faceVec(p.face), side(-f.y, f.x);
  int k = 0;
  for (int i = 0; i < mi && i < (int)g.party.members.size(); i++) k += g.party.members[(size_t)i].state == State::With;
  const float sgn = k % 2 == 0 ? 1.0f : -1.0f, off = 1.0f + 0.7f * (float)(k / 2);
  const Vec2 cands[] = {p.p + side * (sgn * off * T), p.p - side * (sgn * off * T), p.p + side * (sgn * off * T) + f * (0.8f * T),
                        p.p - side * (sgn * off * T) + f * (0.8f * T), p.p + f * (1.2f * T), p.p - f * (1.5f * T)};
  const Map& M = g.map();
  for (const Vec2& c : cands) {
    const int tx = tileOf(c.x), ty = tileOf(c.y - 2);
    if (!M.in(tx, ty) || M.blocked(tx, ty)) continue;
    if (M.kind == MapKind::Overworld && M.bldgAt[(size_t)ty * M.w + tx] >= 0) continue;   // (a doorway: under the roof)
    if (!freeAt(g, c, 4.5f) || !walkable(g, c, p.p, 4.5f, freeAt)) continue;
    return c;
  }
  return HomeOps::freeSpot(g, tileOf(p.p.x), tileOf(p.p.y - 2) + 1);
}

float PartyOps::riderSpeed(const Game& g) { return g.pl().speed * g.homeSpeedMul(); }

// ---------------------------------------------------------------- the player's footsteps
void PartyOps::trailStep(Game& g) {
  party::Party& P = g.party;
  const int mk = g.mapKey();
  if (P.trailMap != mk) {
    P.trail.clear();
    P.trailMap = mk;
    for (Member& m : P.members) m.trailAt = -1;
  }
  const Vec2 at = g.pl().p;
  if (P.trail.empty() || len2(at - P.trail.back()) >= T * T) P.trail.push_back(at);
  if (P.trail.size() > TRAIL_MAX) {
    const int cut = 32;
    P.trail.erase(P.trail.begin(), P.trail.begin() + cut);
    for (Member& m : P.members) if (m.trailAt >= 0) m.trailAt = std::max(0, m.trailAt - cut);
  }
}

// the newest of the player's footsteps in plain reach (the box clear), else the nearest one at all: where a member
// who lost the way picks up the trail
static void pickCrumb(Game& g, const Actor& a, Member& m) {
  const std::vector<Vec2>& tr = g.party.trail;
  int pick = -1, nearest = -1, tried = 0;
  float nd = 1e30f;
  for (int k = (int)tr.size() - 1; k >= 0; k--) {
    const float d2 = len2(tr[(size_t)k] - a.p);
    if (d2 < nd) { nd = d2; nearest = k; }
    if (pick < 0 && d2 < (9.0f * T) * (9.0f * T) && tried++ < 24 && walkable(g, a.p, tr[(size_t)k], a.radius, freeAt)) { pick = k; break; }
  }
  m.trailAt = pick >= 0 ? pick : nearest;
  m.stuckT = 0; m.lastPos = a.p;
}

// toward `goal` round buildings (navStep); when that way is shut or they make no headway, along the player's footsteps
bool PartyOps::moveTo(Game& g, Actor& a, Member& m, Vec2 goal, float speed, float dt) {
  const std::vector<Vec2>& tr = g.party.trail;
  if (m.trailAt >= (int)tr.size()) m.trailAt = -1;
  if (m.trailAt >= 0) {
    // back on the direct way as soon as it is open
    if (len2(goal - a.p) < (8.0f * T) * (8.0f * T) && walkable(g, a.p, goal, a.radius, freeAt)) m.trailAt = -1;
    else {
      Vec2 c = tr[(size_t)m.trailAt];
      while (len2(c - a.p) < 7.0f * 7.0f && m.trailAt + 1 < (int)tr.size()) c = tr[(size_t)++m.trailAt];
      if (len2(c - a.p) < 7.0f * 7.0f) { m.trailAt = -1; return true; }   // (the newest crumb reached)
      if (!g.navStep(a, c, speed, dt)) g.moveActor(a, norm(c - a.p) * std::min(speed * dt, len(c - a.p)));
      // no headway on this footstep (a body that cannot fit where the player passed): on to the next ones
      if (len2(a.p - m.lastPos) > (0.5f * T) * (0.5f * T)) { m.lastPos = a.p; m.stuckT = 0; }
      else if ((m.stuckT += dt) > 0.6f) {
        m.stuckT = 0;
        m.trailAt = std::min(m.trailAt + 2, (int)tr.size() - 1);
        if (len2(tr[(size_t)m.trailAt] - a.p) > (2.5f * T) * (2.5f * T) && freeAt(g, tr[(size_t)m.trailAt], a.radius) &&
            (std::fabs(a.p.x - g.pl().p.x) > g.sleepHalfW * T || std::fabs(a.p.y - g.pl().p.y) > g.sleepHalfH * T)) {
          a.p = tr[(size_t)m.trailAt];   // (out of sight, wedged: a step along the player's own way)
          a.navNext = -1;
        }
        m.lastPos = a.p;
      }
      return true;
    }
  }
  const bool ok = g.navStep(a, goal, speed, dt);
  if (len2(a.p - m.lastPos) > (0.5f * T) * (0.5f * T)) { m.lastPos = a.p; m.stuckT = 0; }
  else m.stuckT += dt;
  if ((!ok || m.stuckT > 0.8f) && !tr.empty()) pickCrumb(g, a, m);
  return ok;
}

void PartyOps::follow(Game& g, Actor& a, Member& m, int mi, float dt) {
  const Actor& p = g.pl();
  const bool riding = g.homeRiding();
  const bool mounted = riding && m.horse != 0 && (m.flags & party::MF_MOUNTED) && m.voc != party::Vocation::Hound;
  a.posture = mounted ? art::Posture::Ride : art::Posture::None;
  Vec2 goal = slotBehind(g, mi);
  // (fixer M8 r1) the slot shut (a wall behind the player in a doorway, a room's corner) or on another body: the
  // nearest open place round the player at least a tile from them and from the others, so nobody stands on the hero
  {
    auto clearOf = [&](Vec2 c) {
      if (len2(c - p.p) < (0.95f * T) * (0.95f * T) || !freeAt(g, c, a.radius)) return false;
      for (size_t i = 1; i < g.actors.size(); i++) {
        const Actor& o = g.actors[i];
        if (o.id == a.id || !o.comp || o.st == AState::Dead) continue;
        if (len2(o.p - c) < (0.8f * T) * (0.8f * T) && (int)o.comp - 1 < mi) return false;   // (the earlier member keeps their place)
      }
      return true;
    };
    if (!clearOf(goal)) {
      const Vec2 back = norm(Vec2(0, 0) - p.aim + Vec2(0.0001f, 0.0f));
      float best = 1e30f;
      Vec2 pick = goal;
      for (int r = 0; r < 2; r++)
        for (int k = 0; k < 8; k++) {
          const float an = (float)k * 0.785398f;
          const Vec2 dir(back.x * std::cos(an) - back.y * std::sin(an), back.x * std::sin(an) + back.y * std::cos(an));
          const Vec2 c = p.p + dir * ((1.2f + 0.6f * (float)r) * T);
          if (!clearOf(c)) continue;
          const float d2 = len2(c - goal) + (k == 4 ? 4.0f * T * T : 0.0f);   // (in front of the player: the last resort)
          if (d2 < best) { best = d2; pick = c; }
        }
      goal = pick;
    }
  }
  // Passive (hold back): with a fight at the player's side they keep well back, away from it
  if (m.stance == party::Cmd::Passive) {
    const Actor* near = nullptr;
    float nd = (6.0f * T) * (6.0f * T);
    for (size_t i = 1; i < g.actors.size(); i++) {
      const Actor& e = g.actors[i];
      if (!e.hostile || e.st == AState::Dead || !e.aggro) continue;
      const float d2 = len2(e.p - p.p);
      if (d2 < nd) { nd = d2; near = &e; }
    }
    if (near) goal = p.p + norm(p.p - near->p + Vec2(0.0001f, 0)) * (4.0f * T);
  }
  const float dist = len(goal - a.p);
  const float pSpeed = riding ? riderSpeed(g) : p.speed;
  const bool pMoving = p.st == AState::Walk;
  if (dist > 0.6f * T) {
    float sp = a.speed;
    if (dist > 6.0f * T) sp = std::max(a.speed * 1.6f, pSpeed * 1.35f);        // well behind: a sprint (a gallop) to close up
    else if (dist > 3.0f * T) sp = std::max(a.speed * 1.5f, pSpeed * 1.15f);   // a run
    else if (pMoving) sp = std::max(a.speed, pSpeed * (dist > 1.5f * T ? 1.08f : 1.0f));
    if (riding && !mounted) sp = std::min(sp, std::max(a.speed * 1.7f, 96.0f));   // afoot they cannot keep a horse's pace
    if (mounted) sp = std::max(sp, pSpeed * (dist > 6.0f * T ? 1.35f : dist > 2.0f * T ? 1.12f : 1.0f));
    // Defend: at the shoulder, and quick to get back there (the 3-tile leash holds from the first blow of a fight, not
    // only once they have a foe of their own)
    if (m.stance == party::Cmd::Defend && !riding && len2(a.p - p.p) > (1.8f * T) * (1.8f * T)) sp = std::max(sp, std::max(a.speed * 1.3f, pSpeed * 1.4f));
    // the way to the formation shut (a wall, a corner): the player's own footsteps, not a path search (cheap)
    if (m.trailAt < 0 && !g.party.trail.empty() && dist > 2.0f * T && !walkable(g, a.p, goal, a.radius, freeAt)) pickCrumb(g, a, m);
    moveTo(g, a, m, goal, sp, dt);
    a.st = AState::Walk;
  } else {
    a.st = AState::Idle;
    a.face = p.face;
    m.stuckT = 0;
    m.trailAt = -1;
  }
}

// where a catch-up lands: the newest footstep of the player's that lies past the screen's edge and within reach
Vec2 PartyOps::outOfSight(Game& g, int mi, int& crumb) {
  const std::vector<Vec2>& tr = g.party.trail;
  const Actor& p = g.pl();
  const float hw = (g.sleepHalfW + 0.5f) * T, hh = (g.sleepHalfH + 0.5f) * T;
  const float maxR = (float)(party::CATCH_UP_TILES - 3) * T;
  const float r = 4.5f;
  for (int k = (int)tr.size() - 1; k >= 0; k--) {
    const Vec2 d = tr[(size_t)k] - p.p;
    if (len2(d) > maxR * maxR) break;
    if ((std::fabs(d.x) > hw || std::fabs(d.y) > hh) && freeAt(g, tr[(size_t)k], r)) { crumb = k; return tr[(size_t)k]; }
  }
  // (a short trail, a small map) the farthest footstep still within reach
  for (int k = 0; k < (int)tr.size(); k++) {
    if (len2(tr[(size_t)k] - p.p) < maxR * maxR && freeAt(g, tr[(size_t)k], r)) { crumb = k; return tr[(size_t)k]; }
  }
  crumb = -1;
  return spotBehind(g, mi);
}

void PartyOps::keepUp(Game& g, float dt) {
  (void)dt;
  const Actor& p = g.pl();
  for (int mi = 0; mi < (int)g.party.members.size(); mi++) {
    Member& m = g.party.members[(size_t)mi];
    if (m.state != State::With && m.state != State::Downed) continue;
    const int ai = actorOf(g, mi);
    if (ai < 0 && m.state == State::Downed && (m.flags & (party::MF_DOWN_WAIT | party::MF_DOWN_HOME))) {
      // (fixer M8 r2) one downed where they waited or lived, left behind out of play: the foes went with the player,
      // and they come round by themselves (wounded) to the life they had; never carried along into the party
      m.state = (m.flags & party::MF_DOWN_WAIT) ? State::Waiting : State::Home;
      m.flags = (uint16_t)((m.flags & ~(party::MF_DOWN_WAIT | party::MF_DOWN_HOME | party::MF_CARRIED)) | party::MF_WOUNDED);
      m.woundedUntil = g.day + 1;
      m.downT = 0; m.reviveT = 0; m.hp = -1;
      continue;
    }
    if (ai < 0) {
      // not in play: a map change (they come through with the player), a load, a journey's end
      if (m.state == State::Downed && m.inPlay) {   // carried out bleeding: less time left
        m.downT *= party::CARRY_FACTOR;
        m.flags |= party::MF_CARRIED;
        m.reviveT = 0;
        g.party.stats.carried++;
      }
      spawn(g, mi, arriveSpot(g, mi));
      continue;
    }
    if (m.state != State::With) continue;
    Actor& a = g.actors[(size_t)ai];
    const float lag = len(a.p - p.p) / T;
    g.party.stats.worstLagTiles = std::max(g.party.stats.worstLagTiles, lag);
    if (lag > (float)party::CATCH_UP_TILES) {
      int crumb = -1;
      a.p = outOfSight(g, mi, crumb);
      a.home = a.p; a.goal = a.p; a.navNext = -1; a.navGoal = -1; a.knock = Vec2();
      m.trailAt = crumb >= 0 ? std::min(crumb + 1, (int)g.party.trail.size() - 1) : -1;
      m.stuckT = 0; m.lastPos = a.p;
      if (a.st == AState::Windup || a.st == AState::Strike) a.st = AState::Idle;
      m.target = -1;
      g.party.stats.catchUps++;
    }
  }
}

// ---------------------------------------------------------------- waiting, at home
int PartyOps::homePlotOf(const Game& g, const Member& m) {
  if (!m.homePlot) return -1;
  for (size_t i = 0; i < g.home.plots.size(); i++) if ((uint32_t)g.home.plots[i].id == m.homePlot) return (int)i;
  return -1;
}

void PartyOps::placesStep(Game& g) {
  const Actor& p = g.pl();
  const int housePlot = g.inside ? home::plotOfInterior(g) : -1;
  for (int mi = 0; mi < (int)g.party.members.size(); mi++) {
    Member& m = g.party.members[(size_t)mi];
    const int ai = actorOf(g, mi);
    if (m.state == State::Waiting) {
      bool here = false;
      Vec2 at;
      if (m.mapKind == 0) {
        if (!g.inside) {
          const int lx = m.gx - g.world.ox, ly = m.gy - g.world.oy;
          at = Vec2(lx * T + 8.0f, ly * T + 10.0f);
          here = g.world.over.in(lx, ly) && len2(at - p.p) < (40.0f * T) * (40.0f * T);
        }
      } else if (g.inside) {
        const bool bldg = m.mapKind == 2 && g.subBldg >= 0 && g.world.over.bldgs[(size_t)g.subBldg].id == m.mapId && g.subFloor == m.floor;
        const bool site = m.mapKind == 1 && g.subSite >= 0 && g.world.sites[(size_t)g.subSite].id == m.mapId;
        if (bldg || site) { here = g.sub.in(m.gx, m.gy); at = Vec2(m.gx * T + 8.0f, m.gy * T + 10.0f); }
      }
      if (here && ai < 0) spawn(g, mi, g.freeSpot(tileOf(at.x), tileOf(at.y - 2)));
      else if (!here && ai > 0) dropActor(g, mi);
      continue;
    }
    if (m.state == State::Home && m.homePlot) {
      const int pi = homePlotOf(g, m);
      bool here = false;
      Vec2 at;
      if (pi >= 0 && pi == housePlot) {
        here = true;
        at = p.p + Vec2(0, -2.0f * T);
      } else if (pi >= 0 && !g.inside && g.hour >= 7.0f && g.hour < 20.0f) {   // out in the yard by daylight
        const home::Plot& P = g.home.plots[(size_t)pi];
        const int lx = P.gx - g.world.ox, ly = P.gy - g.world.oy;
        int gx = 0, gy = 0;
        home::gateOf(P, gx, gy);
        const Vec2 c((lx + P.w / 2) * T + 8.0f, (ly + P.h / 2) * T + 8.0f);
        here = g.world.over.in(lx, ly) && len2(c - p.p) < (36.0f * T) * (36.0f * T);
        at = Vec2((lx + std::clamp(gx, 1, (int)P.w - 2)) * T + 8.0f, (ly + std::clamp(gy - 1, 1, (int)P.h - 2)) * T + 10.0f);
      }
      if (here && ai < 0) {
        const int id = spawn(g, mi, g.freeSpot(tileOf(at.x), tileOf(at.y - 2)));
        const int k = g.findActor(id);
        if (k > 0) g.actors[(size_t)k].lifeA = -1;   // (homeLife places them at once)
      } else if (!here && ai > 0) dropActor(g, mi);
      continue;
    }
    if ((m.state == State::Home || m.state == State::Left) && ai > 0) dropActor(g, mi);
  }
}

// a member at the player's house: a seat by day, a meal at mealtimes, a spare bed at night; the yard by daylight
void PartyOps::homeLife(Game& g, Actor& a, Member& m, float dt) {
  a.st = AState::Idle;
  m.homeT += dt;
  if (!g.inside) {   // in the yard: an idle stroll round the plot
    a.posture = art::Posture::None;
    a.useX = a.useY = -1;
    const int pi = homePlotOf(g, m);
    if (pi < 0) return;
    const home::Plot& P = g.home.plots[(size_t)pi];
    const int lx = P.gx - g.world.ox, ly = P.gy - g.world.oy;
    if (m.homeT > 6.0f || a.lifeA < 0) {
      m.homeT = 0;
      a.lifeA = 0;
      const uint32_t h = hash32((uint32_t)m.id * 2654435761u ^ (uint32_t)(g.time * 4.0f));
      const int x = lx + 1 + (int)(h % (uint32_t)std::max(1, (int)P.w - 2)), y = ly + 1 + (int)((h >> 8) % (uint32_t)std::max(1, (int)P.h - 2));
      if (!g.world.over.blocked(x, y)) a.goal = Vec2(x * T + 8.0f, y * T + 10.0f);
    }
    if (len2(a.goal - a.p) > 6.0f * 6.0f) {
      if (!g.navStep(a, a.goal, a.speed * 0.5f, dt)) a.goal = a.p;
      a.st = AState::Walk;
    } else if (len2(g.pl().p - a.p) < (5.0f * T) * (5.0f * T)) a.face = faceOf(g.pl().p - a.p);
    return;
  }
  // inside: what the hour says
  const float h = g.hour;
  const int want = (h >= 21.5f || h < 6.5f) ? 1 : ((h >= 6.5f && h < 8.0f) || (h >= 12.0f && h < 13.0f) || (h >= 18.5f && h < 20.0f)) ? 2 : 3;
  if (a.lifeA == want) return;
  a.lifeA = want;
  a.posture = art::Posture::None;
  a.useX = a.useY = -1;
  const Map& M = g.sub;
  // the house's beds and seats, in tile order (the first bed is the player's: a companion takes a spare one)
  std::vector<int> beds, seats;
  for (int y = 0; y < M.h; y++)
    for (int x = 0; x < M.w; x++) {
      const int pr = M.propAt(x, y);
      if (!pr) continue;
      const art::Prop q = (art::Prop)(pr - 1);
      if (q == art::Prop::Bed || q == art::Prop::BunkBed) beds.push_back(y * M.w + x);
      if (q == art::Prop::Chair || q == art::Prop::Stool || q == art::Prop::Bench) seats.push_back(y * M.w + x);
    }
  // (fixer M8 r2) the mats and bedrolls after the beds, whatever their tiles: the first bed stays the player's (each
  // member living here has their own mat by the fire, home_interior.cpp)
  for (int y = 0; y < M.h; y++)
    for (int x = 0; x < M.w; x++) {
      const int pr = M.propAt(x, y);
      if (pr == (int)art::Prop::SleepingMat + 1 || pr == (int)art::Prop::Bedroll + 1) beds.push_back(y * M.w + x);
    }
  if (!beds.empty() && M.propAt(beds[0] % M.w, beds[0] / M.w) != (int)art::Prop::Bed + 1 &&
      M.propAt(beds[0] % M.w, beds[0] / M.w) != (int)art::Prop::BunkBed + 1)
    beds.insert(beds.begin(), -1);   // (no bed of the player's at all: none is taken from them; -1 is never a tile)
  // which of the members at this house they are (the k-th takes the k-th spare bed and seat)
  int k = 0;
  for (const Member& o : g.party.members) { if (&o == &m) break; if (o.state == State::Home && o.homePlot == m.homePlot) k++; }
  auto taken = [&](int tile) {
    for (size_t i = 1; i < g.actors.size(); i++) if (g.actors[i].id != a.id && g.actors[i].useX >= 0 && g.actors[i].useY * M.w + g.actors[i].useX == tile) return true;
    return false;
  };
  if (want == 1) {
    for (size_t b = 1, n = 0; b < beds.size(); b++) {   // (a spare bed: the first is the player's)
      if (taken(beds[b])) continue;
      if ((int)n++ < k) continue;
      if (lifeLayInBed(g, a, beds[b] % M.w, beds[b] / M.w)) return;
    }
  }
  for (size_t s = 0, n = 0; s < seats.size(); s++) {
    if (taken(seats[s])) continue;
    if ((int)n++ < k) continue;
    if (lifeSitOn(g, a, seats[s] % M.w, seats[s] / M.w, want == 2 ? art::Posture::SitEat : art::Posture::Sit)) return;
  }
  // no spare bed or seat: on the floor by the fire (a hearth, a stove), else in the middle of the room, out of the way
  int fx = M.w / 2, fy = M.h / 2;
  for (int y = 0; y < M.h; y++)
    for (int x = 0; x < M.w; x++) {
      const int pr = M.propAt(x, y);
      if (pr == (int)art::Prop::Hearth + 1 || pr == (int)art::Prop::Stove + 1 || pr == (int)art::Prop::Fireplace + 1) { fx = x; fy = y; }
    }
  int best = -1, bd = 1 << 30;
  for (int y = 1; y < M.h - 1; y++)
    for (int x = 1; x < M.w - 1; x++) {
      if (M.blocked(x, y) || y >= M.exitY - 1 || !freeAt(g, Vec2(x * T + 8.0f, y * T + 10.0f), 6.0f)) continue;
      const int d = std::abs(x - fx) * 2 + std::abs(y - fy) * 2 + (int)(hash32((uint32_t)(m.id * 131u + (uint32_t)(y * M.w + x))) % 3u);
      const Vec2 c(x * T + 8.0f, y * T + 10.0f);
      if (len2(c - g.pl().p) < (1.5f * T) * (1.5f * T) || std::abs(x - fx) + std::abs(y - fy) < 2) continue;   // (not on the player, not in the fire)
      if (std::abs(x - fx) <= 1 && y < fy) continue;   // (never under a stove's flue or behind the hearth: beside it or before it)
      if (d < bd) { bd = d; best = y * M.w + x; }
    }
  if (best >= 0) { a.p = Vec2((best % M.w) * T + 8.0f, (best / M.w) * T + 10.0f); a.home = a.p; a.goal = a.p; a.face = faceOf(Vec2((float)(fx - best % M.w), (float)(fy - best / M.w))); }
  a.posture = want == 1 ? art::Posture::SitFloor : want == 2 ? art::Posture::SitFloorEat : art::Posture::None;
}
