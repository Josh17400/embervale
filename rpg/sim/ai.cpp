// EMBERVALE AI: per-species monster behaviour, NPC and guard behaviour, the heavy slam.
// (M0 town-defence lane: factions, monsters targeting villagers and guards, fleeing, militia, the bell.)
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/war.h"
#include "rpg/world/economy.h"

using art::Monster;
using art::Prop;

namespace {
bool isSettlement(SiteType t) { return t == SiteType::City || t == SiteType::Town || t == SiteType::Village; }
// the tile an actor stands on (its feet)
int tileX(Vec2 p) { return (int)std::floor(p.x / TILE); }
int tileY(Vec2 p) { return (int)std::floor((p.y - 2) / TILE); }
Vec2 tileCentre(int x, int y) { return Vec2(x * TILE + 8.0f, y * TILE + 10.0f); }
// what a militia member grabs when the bell rings (art::HumanLook::weapon: 2 axe, 6 hammer)
uint8_t militiaTool(Role r) { return r == Role::Farmer ? 2 : 6; }
uint8_t calmTool(Role r) { return r == Role::Smith ? 6 : 0; }
}  // namespace

int Game::settlementAt(Vec2 p) const {
  if (inside) return -1;
  int tx = tileX(p), ty = tileY(p);
  auto test = [&](int i) {
    const Site& s = world.sites[(size_t)i];
    return isSettlement(s.type) && tx >= s.r.x - 1 && ty >= s.r.y - 1 && tx < s.r.x + s.r.w + 1 && ty < s.r.y + s.r.h + 1;
  };
  // endless: the sites near the window (M1 spatial look-up; a point in the window can only lie in one of those)
  if (world.endless && world.nearWindow(tx, ty)) {
    for (int i : world.nearSites) if (test(i)) return i;
    return -1;
  }
  for (int i = 0; i < (int)world.sites.size(); i++) if (test(i)) return i;
  return -1;
}

int Game::shelteredCount(int site) const {
  int n = 0;
  for (const Actor& s : sheltered_) if (site < 0 || s.site == site) n++;
  return n;
}

// Hostiles fight the nearest enemy (factionsHostile), with the player counted 1.5x closer so they stay the main
// target; the current target counts a little closer still, so a monster doesn't flip between two victims.
int Game::pickTarget(const Actor& a) const {
  const Actor& p = actors[0];
  int best = 0;
  float bs = 1e30f;
  if (p.st != AState::Dead && factionsHostile(a.faction, p.faction)) bs = len(p.p - a.p) / 1.5f * (a.target == p.id ? 0.8f : 1.0f);
  if (inside || a.mon == Monster::Dragon) return 0;
  float reach = a.aggroR * (a.aggro ? 1.4f : 1.0f);
  for (size_t i = 1; i < actors.size(); i++) {
    const Actor& e = actors[i];
    if (e.id == a.id || !e.npc || e.st == AState::Dead || e.fly || e.indoors) continue;
    if (!factionsHostile(a.faction, e.faction)) continue;
    float d = len(e.p - a.p);
    if (d > reach) continue;
    float sc = d * (a.target == e.id ? 0.8f : 1.0f);
    if (sc < bs) { bs = sc; best = (int)i; }
  }
  return best;
}

// The building a townsperson runs to when trouble comes: the nearest door to where they live (houses first).
int Game::homeDoor(Actor& a) {
  if (a.homeBldg >= 0) return a.homeBldg;
  if (a.homeBldg == -2 || a.site < 0 || a.site >= (int)world.sites.size() || inside) return -1;
  const Site& s = world.sites[a.site];
  const Map& m = world.over;
  int best = -1;
  float bd = 1e30f;
  // (M5) a resident out on their day's business (life_game.cpp: Actor::home is their own door) makes for home when it
  // is near, else ducks into the nearest door from where they stand
  const Vec2 from = a.resident >= 0 && len2(a.home - a.p) > (10.0f * TILE) * (10.0f * TILE) ? a.p : a.home;
  for (int b = s.bldgFirst; b < s.bldgFirst + s.bldgCount && b < (int)m.bldgs.size(); b++) {
    const Bldg& B = m.bldgs[b];
    if (m.blocked(B.doorX(), B.doorY() + 1)) continue;   // a door you can't stand in front of
    if (B.charred == 2 || B.site != a.site) continue;      // (M4) a burned-out shell shelters nobody
    float d = len(tileCentre(B.doorX(), B.doorY() + 1) - from);
    bool home = B.type == art::Building::House || B.type == art::Building::StoneHouse || B.type == art::Building::Farmhouse || B.type == art::Building::Hut;
    if (!home) d *= 1.6f;
    if (d < bd) { bd = d; best = b; }
  }
  a.homeBldg = best >= 0 ? best : -2;
  return best;
}

// Walk toward `goal` (a world point): straight when the way is clear, otherwise along a 4-connected tile path
// found by a bounded breadth-first search (re-planned on every tile, so moving targets are fine).
bool Game::navStep(Actor& a, Vec2 goal, float speed, float dt) {
  const Map& m = map();
  Vec2 d = goal - a.p;
  float l = len(d);
  if (l < 0.5f) return true;
  // in plain sight: walk straight (sample the body box along the segment)
  bool clear = true;
  for (float t = 4.0f; t < l; t += 4.0f)
    if (!bodyFree(a.p + d * (t / l), a.radius, false)) { clear = false; break; }
  if (clear) {
    moveActor(a, d * (std::min(speed * dt, l) / l));
    a.face = faceOf(d);
    a.navNext = -1;
    return true;
  }
  int sx = tileX(a.p), sy = tileY(a.p), gx = tileX(goal), gy = tileY(goal);
  int goalIdx = gy * m.w + gx;
  a.navT -= dt;
  bool atNext = false;
  if (a.navNext >= 0) {
    Vec2 c = tileCentre(a.navNext % m.w, a.navNext / m.w);
    atNext = len2(c - a.p) < 2.5f * 2.5f;
  }
  if (a.navNext < 0 || atNext || a.navGoal != goalIdx || a.navT <= 0) {
    a.navGoal = goalIdx;
    a.navT = 0.5f;
    a.navNext = -1;
    // bounded BFS from the actor's tile to the goal tile
    // (the margin lets a path swing out through a city gate or around a long house row)
    int x0 = std::max(0, std::min(sx, gx) - 16), y0 = std::max(0, std::min(sy, gy) - 16);
    int x1 = std::min(m.w - 1, std::max(sx, gx) + 16), y1 = std::min(m.h - 1, std::max(sy, gy) + 16);
    int ww = x1 - x0 + 1, hh = y1 - y0 + 1;
    if (ww > 112 || hh > 112 || !m.in(sx, sy) || !m.in(gx, gy)) return false;
    navPrev_.assign((size_t)ww * hh, -1);
    auto loc = [&](int x, int y) { return (y - y0) * ww + (x - x0); };
    std::vector<int> q;
    q.reserve(256);
    q.push_back(loc(sx, sy));
    navPrev_[(size_t)loc(sx, sy)] = loc(sx, sy);
    bool found = false;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (size_t h = 0; h < q.size() && !found; h++) {
      int cx = q[h] % ww + x0, cy = q[h] / ww + y0;
      for (int k = 0; k < 4; k++) {
        int nx = cx + dx[k], ny = cy + dy[k];
        if (nx < x0 || ny < y0 || nx > x1 || ny > y1) continue;
        int li = loc(nx, ny);
        if (navPrev_[(size_t)li] >= 0) continue;
        if (m.blocked(nx, ny) && !(nx == gx && ny == gy)) continue;
        // (owner) a building's ways in (an open front's bays: only the pillars block there) are no street to walk along
        if (m.kind == MapKind::Overworld && m.bldgAt[(size_t)ny * m.w + nx] >= 0 && !(nx == gx && ny == gy)) continue;
        navPrev_[(size_t)li] = q[h];
        if (nx == gx && ny == gy) { found = true; break; }
        q.push_back(li);
      }
    }
    if (!found) return false;
    int cur = loc(gx, gy), start = loc(sx, sy);
    while (navPrev_[(size_t)cur] != start && navPrev_[(size_t)cur] != cur) cur = navPrev_[(size_t)cur];
    if (cur == start) return false;
    a.navNext = (cur / ww + y0) * m.w + (cur % ww + x0);
  }
  Vec2 c = tileCentre(a.navNext % m.w, a.navNext / m.w);
  Vec2 dc = c - a.p;
  float lc = len(dc);
  if (lc > 0.01f) {
    Vec2 before = a.p;
    moveActor(a, dc * (std::min(speed * dt, lc) / lc));
    a.face = faceOf(dc);
    if (len2(a.p - before) < 0.0001f) {
      a.navT = std::min(a.navT, 0.1f);   // wedged on a corner: re-plan soon
      // (M5) standing inside a solid tile (a prop the window streamed in under them): a step out to the nearest open one
      if (m.blocked(tileX(a.p), tileY(a.p))) { a.p = freeSpot(tileX(a.p), tileY(a.p)); a.navNext = -1; }
    }
  }
  return true;
}

// Friendly NPCs: guards (and militia) fight what threatens the town, everyone else runs home and hides, and
// otherwise they stroll around where they live.
void Game::updateFolk(Actor& a, float dt) {
  Actor& p = pl();
  bool talking = mode == Mode::Dialogue && dlg.actor == a.id;
  // M5: a village animal (Actor::critter) lives by the TOWNSFOLK lane (life_game.cpp)
  if (!a.human && a.critter && lifeFolk(a, dt)) return;
  // M2: a beast that keeps a wayside place (the toll bridge's troll) holds its ground until it is crossed
  if (!a.human) {
    a.st = AState::Idle;
    if (len2(p.p - a.p) < (5.0f * TILE) * (5.0f * TILE)) a.face = faceOf(p.p - a.p);
    if (len2(a.home - a.p) > 4.0f) moveActor(a, norm(a.home - a.p) * std::min(a.speed * 0.4f * dt, len(a.home - a.p)));
    return;
  }
  // M2: a missing person (quests.cpp): cowers where they were found, then follows the player out, keeping out of fights;
  // once safe they walk off home (questTick puts them away out of sight)
  if (a.quest > 0) {
    const Quest* q = questById(a.quest);
    const bool following = q && q->type == QType::Missing && q->state == QState::Active && (q->flags & QF_FOUND);
    if (talking) { a.face = faceOf(p.p - a.p); a.st = AState::Idle; return; }
    if (q && q->type == QType::Missing && q->state == QState::Active && !following) {
      a.st = AState::Idle;
      if (len2(p.p - a.p) < (6.0f * TILE) * (6.0f * TILE)) a.face = faceOf(p.p - a.p);
      return;
    }
    if (!following) {   // safe: off home, away from the player
      const Vec2 away = norm(a.p - p.p + Vec2(0.01f, 0.0f));
      moveActor(a, away * (a.speed * 0.5f * dt));
      a.face = faceOf(away); a.st = AState::Walk;
      return;
    }
    // keep a stride behind the player; a beast close by: step away from it (toward the player's far side)
    const Actor* threat = nullptr;
    float td = 64.0f * 64.0f;
    for (int hi : hostiles_) {
      if (hi < 0 || hi >= (int)actors.size()) continue;
      const Actor& e = actors[(size_t)hi];
      if (e.player || e.st == AState::Dead || !e.hostile) continue;
      const float d2 = len2(e.p - a.p);
      if (d2 < td) { td = d2; threat = &e; }
    }
    if (len2(p.p - a.p) > (12.0f * TILE) * (12.0f * TILE)) {   // left behind (a door, a corner): catch up
      a.p = freeSpot((int)std::floor(p.p.x / TILE), (int)std::floor(p.p.y / TILE) + 1);
      a.navNext = -1;
    }
    Vec2 goal = p.p - p.aim * 18.0f;
    if (threat) goal = p.p + norm(p.p - threat->p) * 26.0f;
    const float run = std::max(a.speed * 1.5f, 80.0f);
    if (len2(goal - a.p) > 10.0f * 10.0f) {
      if (!navStep(a, goal, run, dt)) moveActor(a, norm(goal - a.p) * (run * dt));
      a.st = AState::Walk;
    } else { a.st = AState::Idle; a.face = faceOf(p.p - a.p); }
    return;
  }
  bool town = !inside && a.site >= 0 && a.fromMap && a.site < (int)world.sites.size() && isSettlement(world.sites[a.site].type);
  const SiteAlarm* al = nullptr;
  if (town) { auto it = alarms_.find(a.site); if (it != alarms_.end()) al = &it->second; }
  const bool ringing = al && al->ringing;
  const bool hurt = a.militia && a.hp < a.maxHp * 0.4f;   // militia run when badly hurt
  // (M4) a kingdom's soldiers and captains (road patrols, siege camps, checkpoints) fight like the watch
  const bool soldier = a.role == Role::Soldier || a.role == Role::Captain;
  const bool guard = a.role == Role::Guard || soldier;
  if (guard || (a.militia && !hurt)) {
    // (M4) a militiaman looks for the beast on his own clock (Actor::special, unused by townsfolk): thinkT is also the
    // stroll's timer and the flight's (fleeing pins it at 1 s), which kept militia from ever taking up arms once they
    // had started to run, or for up to six seconds after a stroll began
    float& thinkClock = (a.militia && !guard) ? a.special : a.thinkT;
    thinkClock -= dt;
    if (a.unreachT > 0) a.unreachT -= dt;
    if (thinkClock <= 0) {
      thinkClock = 0.4f;
      a.target = -1;
      float bd = 1e30f;
      const Site* st = town ? &world.sites[a.site] : nullptr;
      auto consider = [&](const Actor& e) {
        if (e.id == a.id || e.st == AState::Dead || e.fly || e.indoors || !warFoes(*this, a, e)) return;
        if (e.id == a.unreach && a.unreachT > 0) return;   // no way to it from here: leave it for someone else
        float d2 = len2(e.p - a.p);
        bool nearMe = d2 < (guard ? 110.0f * 110.0f : 90.0f * 90.0f);
        bool inTown = false;
        if (a.role == Role::Guard && st) {
          // the bell (or a beast on the hunt in town, M4: or anything hostile walking the streets) calls every guard:
          // converge on anything inside the walls
          int tx = tileX(e.p), ty = tileY(e.p);
          const bool inside4 = tx >= st->r.x - 4 && ty >= st->r.y - 4 && tx < st->r.x + st->r.w + 4 && ty < st->r.y + st->r.h + 4;
          const bool inside1 = tx >= st->r.x - 1 && ty >= st->r.y - 1 && tx < st->r.x + st->r.w + 1 && ty < st->r.y + st->r.h + 1;
          inTown = ((ringing || e.aggro) && inside4) || (inside1 && !e.npc && !e.player);
        } else if (a.militia && st && ringing) {
          // (M4) the bell calls the militia too: a beast in their own lanes is theirs to fight (a frontier village has
          // no one else)
          int tx = tileX(e.p), ty = tileY(e.p);
          inTown = !e.npc && !e.player && tx >= st->r.x - 1 && ty >= st->r.y - 1 && tx < st->r.x + st->r.w + 1 && ty < st->r.y + st->r.h + 1;
        }
        if (soldier) nearMe = d2 < 130.0f * 130.0f;   // soldiers in the field keep a wider eye
        if (!nearMe && !inTown) return;
        // militia defend their own street (M4: further from home while the bell rings; in a frontier village, where no
        // watch will come, the whole village is theirs to defend)
        const float reachM = ringing ? (warFrontier(*this, a.site) ? 40.0f : 16.0f) : 10.0f;
        if (a.militia && len2(e.p - a.home) > (reachM * TILE) * (reachM * TILE)) return;
        // a guard posted at a besieged gate, or a soldier of a camp, keeps to its post against the men across the field
        // unless they come close (the war is the realm's; the street fights are the player's to start)
        if (e.npc && e.faction == Faction::Army && !e.hostile && !a.hostile && d2 > 70.0f * 70.0f && !inTown) return;
        if (d2 < bd) { bd = d2; a.target = e.id; }
      };
      for (int hi : hostiles_)
        if (hi >= 0 && hi < (int)actors.size()) consider(actors[(size_t)hi]);
      // the watch fights soldiers of an enemy kingdom (in hostiles_) and, when hostile to the player, the player; a
      // soldier also fights the guards of the town it besieges (guards are no "hostiles" of the townsfolk)
      if (a.hostile && pl().st != AState::Dead) consider(pl());
      if (soldier || a.hostile)
        for (size_t k = 1; k < actors.size(); k++)
          if (actors[k].npc && actors[k].realm && !actors[k].hostile && actors[k].faction != Faction::Army) consider(actors[k]);
    }
    int ti = a.target >= 0 ? findActor(a.target) : -1;
    if (ti >= 0 && actors[ti].st != AState::Dead && !actors[ti].fly) {
      Actor& t = actors[ti];
      Vec2 d = t.p - a.p;
      float l = len(d);
      a.aim = norm(d);
      a.face = faceOf(d);
      if (a.militia) a.look.weapon = militiaTool(a.role);
      float reach = a.range + t.radius;
      if (a.st == AState::Windup) {
        if (l > reach - 3) moveActor(a, a.aim * (a.speed * 0.6f * dt));   // step into the blow
        if (a.stT > 0.3f) { a.st = AState::Strike; a.stT = 0; meleeHit(a); sfx((int)Sfx::Swing, a.p, guard ? 1.0f : 1.15f, 0.8f); }
        return;
      }
      if (a.st == AState::Strike) { if (a.stT > 0.25f) { a.st = AState::Idle; a.stT = 0; a.atkCd = guard ? 0.8f : 1.3f; } return; }
      if (l > reach) {
        // (M2) the watch runs when the bell rings: a big town's guards must reach the square before the beasts do harm
        const float chase = a.speed * (guard && ringing ? 1.6f : 1.2f);
        const Vec2 before = a.p;
        const bool stepped = navStep(a, t.p, chase, dt);
        if (stepped && len2(a.p - before) < 1e-4f && l < reach + 14.0f) {
          // (M6 fixer r2, rpg_test seed 36: two guards stood 21 px from their wolves for 30 s) wedged right beside the
          // target: a beast standing inside a solid tile (the window streamed a prop in under it) steps out to open
          // ground, and a target just past the reach of a guard who cannot close in is struck anyway (the step into the
          // blow and the swing's own reach carry it)
          const Map& mm = map();
          if (mm.blocked(tileX(t.p), tileY(t.p))) { t.p = freeSpot(tileX(t.p), tileY(t.p)); t.navNext = -1; }
          else if (a.atkCd <= 0) { a.st = AState::Windup; a.stT = 0; return; }
        }
        if (!stepped) {
          // no path (a wall, water or a house row in between): don't grind against it; give the target up for a
          // while unless it is right there
          if (l > 3.0f * TILE) { a.unreach = t.id; a.unreachT = 6.0f; a.target = -1; thinkClock = 0; a.st = AState::Idle; return; }
          moveActor(a, a.aim * (chase * dt));
        }
        a.st = AState::Walk;
      } else if (a.atkCd <= 0) { a.st = AState::Windup; a.stT = 0; }
      else a.st = AState::Idle;
      return;
    }
    if (a.militia) a.look.weapon = calmTool(a.role);
  }
  if (talking) { a.face = faceOf(p.p - a.p); a.st = AState::Idle; return; }
  // (M4) soldiers and refugees go where the war sends them (war_game.cpp sets a.goal each step; a.special the pace)
  if (soldier || a.role == Role::Refugee) {
    const Vec2 d = a.goal - a.p;
    const float l = len(d);
    const float pace = a.special > 0 ? a.special : 0.5f;
    if (l > 4.0f) {
      if (!navStep(a, a.goal, a.speed * pace, dt)) moveActor(a, d * (a.speed * pace * dt / l));
      a.st = AState::Walk;
    } else a.st = AState::Idle;
    return;
  }
  // townsfolk run home and hide from monsters on the loose (and from the bell); they come back out when it's over
  if (town && !guard) {
    const Actor* threat = nullptr;
    float td = 150.0f * 150.0f;   // (M2: from 100 px; folk start running sooner)
    // a threat is a monster on the hunt (aggro) or one right beside them; a pack dozing at its den by the
    // fields does not send the whole street indoors. (M4) A beast walking the streets themselves is a threat however
    // calm it looks (villagers no longer stroll past a wolf in the lane), and so are a kingdom's soldiers storming it
    const Site& home = world.sites[a.site];
    for (int hi : hostiles_) {
      if (hi < 0 || hi >= (int)actors.size()) continue;
      const Actor& e = actors[(size_t)hi];
      if (e.player || e.st == AState::Dead || e.fly || len2(e.p - a.p) >= td || !warFoes(*this, a, e)) continue;
      const int ex = tileX(e.p), ey = tileY(e.p);
      const bool inStreets = !e.npc && ex >= home.r.x && ey >= home.r.y && ex < home.r.x + home.r.w && ey < home.r.y + home.r.h;
      if (e.aggro || inStreets || len2(e.p - a.p) < 48.0f * 48.0f) { td = len2(e.p - a.p); threat = &e; }
    }
    if (threat || ringing) {
      if (threat) alarms_[a.site].lastThreatT = time;
      a.fleeT += dt;
      bool ran = false;
      if (a.fleeT > 8.0f && a.fleeT - dt <= 8.0f) {
        // home is cut off: make for the nearest door from here instead
        a.homeBldg = -1;
        Vec2 h = a.home;
        a.home = a.p;
        homeDoor(a);
        a.home = h;
      }
      // after 16 s with every door out of reach (cut off by the threat or the river) they give up running for a door:
      // they only ever go inside AT a door (vanishing in the open street read as a glitch), so from here they cower
      // where they stand and only scramble away when the threat closes in. The flight clock stops at 16 s.
      const bool cower = a.fleeT > 16.0f;
      if (cower) a.fleeT = 16.0f + 1e-3f;
      int b = homeDoor(a);
      if (b >= 0) {
        const Bldg& B = world.over.bldgs[b];
        Vec2 door = tileCentre(B.doorX(), B.doorY() + 1);
        if (len2(a.p - door) < 7.0f * 7.0f) { a.indoors = true; a.st = AState::Idle; return; }
        if (!cower) ran = navStep(a, door, a.speed * 1.4f, dt);
      }
      if (cower && !(threat && len2(threat->p - a.p) < 56.0f * 56.0f)) {
        if (threat) a.face = faceOf(threat->p - a.p);   // watching it, frozen to the spot
        a.st = AState::Idle;
        a.goal = a.p; a.thinkT = 1.0f;
        return;
      }
      if (!ran) {
        if (!threat) { a.st = AState::Idle; return; }
        Vec2 away = norm(a.p - threat->p);
        moveActor(a, away * (a.speed * 1.35f * dt));
        a.face = faceOf(away);
      }
      a.st = AState::Walk;
      a.goal = a.p; a.thinkT = 1.0f;   // when it's over, stay put a moment before wandering again
      return;
    }
    if (a.fleeT > 8.0f) a.homeBldg = -1;   // the nearest-door fallback was for that alarm only: home is home again
    a.fleeT = 0;
  }
  // M5: a resident with a plan lives it (work, meals, the tavern, sleep; life_game.cpp, TOWNSFOLK lane)
  if (!guard && lifeFolk(a, dt)) return;
  // M2 capital square life (owner note 6): the people whose place is out on a city's square linger near it by day and
  // go home at night (indoors until morning: updateTownDefence lets them out at dawn)
  const bool squareGoer = town && !guard && !a.stallKeeper && (a.role == Role::Villager || a.role == Role::Child) &&
                          world.sites[(size_t)a.site].type == SiteType::City &&
                          world.over.at((int)std::floor(a.home.x / TILE), (int)std::floor((a.home.y - 2) / TILE)) == Ground::Plaza;
  // (M5) a stall's keeper goes home for the night too (back at the counter in the morning: lifeSpawnAllowed)
  const bool keeperHome = town && a.stallKeeper && (hour >= 21.5f || hour < 6.0f);
  if ((squareGoer || keeperHome) && !talking && (hour >= 21.0f || hour < 6.0f)) {
    const int b = homeDoor(a);
    // (fixer M5 r3, review: "stall keepers still at closed stalls at 02:00") deep in the night they went home hours ago
    // (still out only after a jump of the clock): indoors at once; one with no door of their own leaves the street too
    if (b < 0 || hour >= 23.0f || hour < 5.0f) { a.indoors = true; a.nightHome = true; a.st = AState::Idle; return; }
    {
      const Bldg& B = world.over.bldgs[(size_t)b];
      const Vec2 door = tileCentre(B.doorX(), B.doorY() + 1);
      if (len2(a.p - door) < 7.0f * 7.0f) { a.indoors = true; a.nightHome = true; a.st = AState::Idle; return; }
      if (!navStep(a, door, a.speed * 0.6f, dt)) moveActor(a, norm(door - a.p) * (a.speed * 0.5f * dt));
      a.st = AState::Walk;
      return;
    }
  }
  a.thinkT -= dt;
  if (a.stallKeeper) {
    // (M1 economy) a stall keeper minds the counter while the stall is open: back behind it, facing the customers.
    // (M1 fixer) The way back (from shelter, from a door across the square) goes round the stalls by the path finder
    // to the keeper's tile behind the counter, then the last step in; after the stall's closing hour the keeper is off
    // duty and strolls the square until morning.
    // (stall facings) the keeper's post (the tile inside the stall) and the way they look follow the stall's facing.
    // (stalls fixer round 3) The stall from the keeper's spot (a side or back keeper stands on their post tile, behind
    // the counter's middle); a table's or cloth's seller stands on its tile (facing S)
    int stx = (int)std::floor(a.home.x / TILE), sty = (int)std::floor(a.home.y / TILE), sf = art::StallS;
    art::stallOfKeeper([&](int x, int y) { return world.over.propAt(x, y); }, a.home.x, a.home.y, world.ox, world.oy, stx, sty, sf);
    const art::StallKeeperSpot ks = art::stallKeeperSpot(sf);
    const int pox = stx + ks.postDx, poy = sty + ks.postDy;
    if (ew::stallOpen(stx + world.ox, sty + world.oy, hour)) {
      a.goal = a.home;
      if (len2(a.home - a.p) < 9.0f) { a.st = AState::Idle; a.face = ks.face; return; }
      if (tileX(a.p) == pox && tileY(a.p) == poy) { a.p = a.home; a.st = AState::Idle; a.face = ks.face; return; }
      const Vec2 post = tileCentre(pox, poy);
      if (!navStep(a, post, a.speed * 0.5f, dt)) moveActor(a, norm(post - a.p) * (a.speed * 0.45f * dt));
      a.face = faceOf(post - a.p);
      a.st = AState::Walk;
      return;
    }
    // off duty: out from behind the counter (the keeper stood a step into the stall's row), then a stroll round the
    // square behind the stall
    if (tileX(a.p) == stx && tileY(a.p) == sty) { a.p = tileCentre(pox, poy); a.thinkT = 0; }
    if (a.thinkT <= 0) {
      a.thinkT = 3.0f + rng_.f() * 5.0f;
      const float side = rng_.range(-4.0f, 4.0f), back = rng_.range(1.0f, 4.0f);
      a.goal = tileCentre(pox, poy) + Vec2(side * (float)std::abs(ks.postDy) + back * (float)ks.postDx, side * (float)std::abs(ks.postDx) + back * (float)ks.postDy) * TILE;
    }
  }
  if (a.thinkT <= 0 && !a.stallKeeper) {
    a.thinkT = 2.0f + rng_.f() * 4.0f;
    if (rng_.f() < 0.45f) a.goal = a.p;
    else {
      float range = inside ? 3.0f : (squareGoer ? 3.5f : 6.0f);
      a.goal = a.home + Vec2(rng_.range(-range, range) * TILE, rng_.range(-range * 0.4f, range * 0.4f) * TILE);
      // (M2 fixer round 3) indoors a stop keeps a few px clear of the walls and furniture: a person idling with their
      // body against a side wall was drawn half into it
      if (inside) {
        for (int k = 0; k < 4 && !bodyFree(a.goal, a.radius + 4.0f, false); k++)
          a.goal = a.home + Vec2(rng_.range(-range, range) * TILE, rng_.range(-range * 0.4f, range * 0.4f) * TILE);
        if (!bodyFree(a.goal, a.radius + 4.0f, false)) a.goal = a.p;
      }
    }
  }
  Vec2 d = a.goal - a.p;
  float l = len(d);
  if (l > 3) {
    Vec2 before = a.p;
    moveActor(a, d * (1.0f / l) * (a.speed * 0.45f * dt));
    a.face = faceOf(d);
    a.st = AState::Walk;
    if (len2(a.p - before) < 0.0004f) a.goal = a.p;
  } else a.st = AState::Idle;
}

// Town defence, once per frame after the actors moved: people at their door go inside, settlements count the
// hostiles within their footprint and ring the bell at 3 or more, and the hidden come back out ~30 s after the
// last threat. Nothing here is saved: after a load everyone is simply back on the street.
void Game::updateTownDefence(float dt) {
  if (inside) { alarmSite = -1; return; }
  for (size_t i = 1; i < actors.size();) {
    if (!actors[i].indoors) { i++; continue; }
    Actor s = actors[i];
    s.indoors = false; s.st = AState::Idle; s.stT = 0; s.navNext = -1; s.knock = Vec2(); s.target = -1;
    sfx((int)Sfx::Door, s.p, 1.15f, 0.35f);
    sheltered_.push_back(s);
    actors.erase(actors.begin() + i);
  }
  collectHostiles();   // (indices moved: people went indoors)
  const Actor& p = pl();
  for (int si : activeSites_) {
    const Site& st = world.sites[si];
    if (!isSettlement(st.type)) continue;
    SiteAlarm& al = alarms_[si];
    int n = 0, near = 0;   // inside the footprint (the bell) / prowling at its edge (keeps folk indoors)
    const ew::Gid ownerK = st.kingdom >= 0 ? world.kingdoms[(size_t)st.kingdom].id : 0;
    for (int hi : hostiles_) {
      const Actor& e = actors[(size_t)hi];
      if (e.player || e.st == AState::Dead) continue;
      // (M4) a kingdom's soldiers at war with the owner, storming its streets, ring the bell too
      const bool enemyArmy = e.faction == Faction::Army && e.realm && ownerK && e.realm != ownerK && realm.atWar(e.realm, ownerK);
      if (!factionsHostile(Faction::Town, e.faction) && !enemyArmy) continue;
      int tx = tileX(e.p), ty = tileY(e.p);
      bool in = tx >= st.r.x - 1 && ty >= st.r.y - 1 && tx < st.r.x + st.r.w + 1 && ty < st.r.y + st.r.h + 1;
      if (in) n++;
      // the threat lasts while anything hostile is inside, or something hunts around the edge of town (a wolf
      // chasing a villager out past the houses); a pack idling at a nearby den or camp is not a threat
      // a stray (no den or camp to go home to: what came into town) is a threat while it hunts anyone nearby or
      // prowls just outside the houses; a den or camp pack only while it hunts townsfolk (a pack defending its den
      // against the player out in the fields is the player's fight, not the town's)
      const bool stray = e.den < 0 && e.site < 0;
      const bool hunting = e.aggro && (stray || e.target != p.id);
      const bool edge6 = tx >= st.r.x - 6 && ty >= st.r.y - 6 && tx < st.r.x + st.r.w + 6 && ty < st.r.y + st.r.h + 6;
      if (in || (stray && edge6) || (hunting && tx >= st.r.x - 14 && ty >= st.r.y - 14 && tx < st.r.x + st.r.w + 14 && ty < st.r.y + st.r.h + 14)) near++;
    }
    al.hostiles = n;
    if (near > 0) al.lastThreatT = time;
    Vec2 c = tileCentre(st.r.cx(), st.r.cy());
    float pd = len(c - p.p) / TILE;
    if (n >= 3 && !al.ringing) {
      al.ringing = true; al.bellT = 0; al.quietT = 0;
      if (pd < 50) {
        say("THE TOWN IS UNDER ATTACK!");
        emit(Ev::Notice, p.p, (int)rgba(255, 96, 70), 0, "THE BELL OF " + st.name + " IS RINGING");
      }
    }
    if (al.ringing) {
      al.bellT -= dt;
      if (al.bellT <= 0) {
        al.bellT = 4.5f;
        if (pd < 50) sfx((int)Sfx::Bell, c, 1.0f, clampf(1.25f - pd / 40.0f, 0.3f, 1.0f));
      }
      if (n == 0) {
        al.quietT += dt;
        if (al.quietT > 6.0f) {
          al.ringing = false;
          if (pd < 50) say("THE BELL FALLS SILENT. " + st.name + " IS SAFE.");
        }
      } else al.quietT = 0;
    }
  }
  // the hidden come back out once it has been quiet for a while (healed: they patched themselves up indoors)
  for (size_t i = 0; i < sheltered_.size();) {
    Actor& s = sheltered_[i];
    auto it = alarms_.find(s.site);
    bool safe = it == alarms_.end() || (!it->second.ringing && time - it->second.lastThreatT > 30.0f);
    if (s.nightHome && (hour >= 21.0f || hour < 6.0f)) safe = false;   // (M2) home for the night: out again at dawn
    int b = s.homeBldg;
    if (safe && b >= 0 && b < (int)world.over.bldgs.size()) {
      const Bldg& B = world.over.bldgs[b];
      Vec2 door = tileCentre(B.doorX(), B.doorY() + 1);
      const IRect sr = s.site >= 0 && s.site < (int)world.sites.size() ? world.sites[s.site].r : IRect{};
      for (int hi : hostiles_) {
        const Actor& e = actors[(size_t)hi];
        if (e.player || e.st == AState::Dead || !factionsHostile(s.faction, e.faction) || len2(e.p - door) >= 140.0f * 140.0f) continue;
        int tx = tileX(e.p), ty = tileY(e.p);
        bool inTown = tx >= sr.x - 1 && ty >= sr.y - 1 && tx < sr.x + sr.w + 1 && ty < sr.y + sr.h + 1;
        const bool stray = e.den < 0 && e.site < 0;
        if ((e.aggro && e.target != pl().id) || inTown || stray) safe = false;   // (not a pack dozing at its den or fighting the player)
      }
      if (safe) {
        s.p = door; s.vel = Vec2(); s.fleeT = 0; s.fleeing = false; s.hp = s.maxHp; s.face = 0; s.nightHome = false;
        s.homeBldg = -1;   // out of whatever door sheltered them; the next alarm sends them home again (homeDoor)
        s.goal = s.home; s.thinkT = 0.5f + rng_.f() * 2.0f; s.st = AState::Walk; s.stT = 0;
        if (s.militia) s.look.weapon = calmTool(s.role);
        sfx((int)Sfx::Door, door, 1.1f, 0.3f);
        actors.push_back(s);
        sheltered_.erase(sheltered_.begin() + (long)i);
        continue;
      }
    } else if (safe) {   // no door to come out of: drop them back where they live
      s.p = freeSpot(tileX(s.home), tileY(s.home));
      actors.push_back(s);
      sheltered_.erase(sheltered_.begin() + (long)i);
      continue;
    }
    i++;
  }
  alarmSite = -1;
  for (auto& kv : alarms_)
    if (kv.second.ringing && activeSites_.count(kv.first) && (alarmSite < 0 || kv.first == curSite)) alarmSite = kv.first;
}

void Game::updateAI(Actor& a, float dt) {
  float slow = a.slowT > 0 ? 0.55f : 1.0f;
  if (a.st == AState::Hurt) { if (a.stT > 0.25f) { a.st = AState::Idle; a.stT = 0; } return; }

  // ---- friendly NPCs (M4: and the kingdoms' men-at-arms even when they are the player's enemies: a guard of a town the
  //      player assaults, an enemy patrol, a besieger's soldier fight as soldiers, not as beasts)
  if (!a.hostile || (a.npc && a.human && (a.role == Role::Guard || a.role == Role::Soldier || a.role == Role::Captain))) { updateFolk(a, dt); return; }

  // ---- hostiles: fight the nearest enemy (the player counted 1.5x closer); the leash only holds outside towns
  a.thinkT -= dt;
  const bool think = a.thinkT <= 0;
  if (think) a.target = actors[(size_t)pickTarget(a)].id;
  int ti = a.mon == Monster::Dragon || a.target < 0 ? 0 : findActor(a.target);
  if (ti < 0 || actors[(size_t)ti].st == AState::Dead || actors[(size_t)ti].indoors) ti = 0;
  Actor& p = actors[(size_t)ti];   // the target (usually the player)
  Vec2 toP = p.p - a.p;
  float dist = len(toP);
  if (think) {
    a.thinkT = 0.25f;
    float sense = a.aggroR * (isNight() && !inside ? 0.8f : 1.0f);
    if (p.player && background == Background::Hunter && a.faction == Faction::Wild) sense *= 0.7f;   // the hunter's quiet step
    if (p.st != AState::Dead && !a.fleeing && dist < sense) a.aggro = true;
    // packs are tied to their den: past the leash (or once the prey is long gone) they give up and go home.
    // Inside a settlement the leash doesn't hold: whatever got in fights until the guards end it.
    float leash = a.den >= 0 ? 18.0f * TILE : 28.0f * TILE;
    bool inTown = !inside && settlementAt(a.p) >= 0;
    if (!inTown && len(a.p - a.home) > leash && dist > 6 * TILE && !a.boss) a.aggro = false;
    // (M3c) a lurker never strays far from its water: it slides back once the prey is out of the shallows
    if (a.mon == Monster::Lurker && !inTown && len(a.p - a.home) > 9.0f * TILE && dist > 3 * TILE) a.aggro = false;
    if (!a.boss && !inside && dist > std::max(a.aggroR * 2.4f, 15.0f * TILE)) a.aggro = false;
    if (p.st == AState::Dead) a.aggro = false;
    if (!a.aggro) a.fleeing = false;
    if (!a.aggro && a.st != AState::Windup) {
      if (rng_.f() < 0.25f && a.mon != Monster::Lurker) {   // (a lurker lies still in the shallows)
        float r = a.wild ? 5.0f : 3.0f;
        a.goal = a.home + Vec2(rng_.range(-r, r) * TILE, rng_.range(-r, r) * TILE);
      }
    }
  }

  // dragon: its own dance
  if (a.mon == Monster::Dragon) {
    a.special -= dt;
    // (M6 fixer) a world-boss dragon keeps to its territory: it gives up once the hero is well clear of its home
    // (or once it has been drawn too far out), wings back to its roost, and only wakes again for a hero inside it.
    // Ashfang (the story dragon) keeps the old chase.
    if (a.rank == (uint8_t)foes::Rank::WorldBoss) {
      const float territory = 22.0f * TILE;
      const float pFromHome = len(p.p - a.home), meFromHome = len(a.p - a.home);
      if (a.aggro && (pFromHome > territory || meFromHome > territory * 1.4f || p.st == AState::Dead)) a.aggro = false;
      if (!a.aggro) {
        if (dist < 260 && pFromHome <= territory && p.st != AState::Dead) a.aggro = true;
        else {
          a.fly = meFromHome > 2.0f * TILE;   // home is a short flight away
          if (a.fly) {
            Vec2 d = a.home - a.p;
            a.p += norm(d) * std::min(len(d), 120.0f * dt);
            a.face = d.x >= 0 ? 2 : 3;
            a.st = AState::Walk;
          } else if (a.st == AState::Walk) { a.st = AState::Idle; a.stT = 0; }
          return;
        }
      }
    }
    if (!a.aggro) { if (dist < 260) a.aggro = true; else return; }
    a.aim = norm(toP);
    a.face = toP.x >= 0 ? 2 : 3;
    if (a.fly) {
      // circle the player and rain fire
      float ang = std::atan2(a.p.y - p.p.y, a.p.x - p.p.x) + dt * 0.9f;
      Vec2 want = p.p + Vec2(std::cos(ang), std::sin(ang)) * 90.0f;
      Vec2 d = want - a.p;
      a.p += d * std::min(1.0f, dt * 1.8f);
      a.st = AState::Walk;
      a.shootCd -= 0;
      if (a.shootCd <= 0) {
        a.shootCd = 1.6f;
        for (int k = -1; k <= 1; k++) {
          Projectile pr;
          pr.kind = ProjKind::DragonFire; pr.fromPlayer = false; pr.owner = a.id; pr.fac = a.faction;
          Vec2 aim = norm(p.p + Vec2(k * 14.0f, 0) - (a.p + Vec2(0, -28)));
          pr.p = a.p + Vec2(0, -28); pr.v = aim * 150; pr.dmg = a.dmg * 0.8f; pr.life = 0.7f + (rng_.f() * 0.3f); pr.radius = 6;
          // fire lands on the ground near the player: let it travel to the target point
          pr.life = std::min(1.4f, len(p.p - pr.p) / 150.0f);
          projs.push_back(pr);
        }
        sfx((int)Sfx::Fireball, a.p, 0.6f);
      }
      if (a.special <= 0) { a.fly = false; a.special = 7.0f; sfx((int)Sfx::Roar, a.p); emit(Ev::Shake, a.p, 0, 5); a.st = AState::Idle; a.stT = 0; }
    } else {
      if (a.special <= 0) { a.fly = true; a.special = 9.0f; sfx((int)Sfx::Roar, a.p, 1.1f); return; }
      if (a.st == AState::Windup) {
        if (a.stT > a.windup) { a.st = AState::Strike; a.stT = 0; meleeHit(a); sfx((int)Sfx::HitHeavy, a.p, 0.6f); emit(Ev::Shake, a.p, 0, 4); }
        return;
      }
      if (a.st == AState::Strike) { if (a.stT > 0.4f) { a.st = AState::Idle; a.stT = 0; a.atkCd = 1.2f; } return; }
      if (dist > a.range + 4) { moveActor(a, a.aim * (a.speed * 0.6f * dt)); a.st = AState::Walk; }
      else if (a.atkCd <= 0) { a.st = AState::Windup; a.stT = 0; }
      // breath cone
      if (a.shootCd <= 0 && dist < 120) {
        a.shootCd = 2.6f;
        for (int k = 0; k < 5; k++) {
          Projectile pr; pr.kind = ProjKind::DragonFire; pr.owner = a.id; pr.radius = 5; pr.fac = a.faction;
          float ang = std::atan2(a.aim.y, a.aim.x) + (k - 2) * 0.18f;
          pr.p = a.p + Vec2(a.aim.x * 20, -14); pr.v = Vec2(std::cos(ang), std::sin(ang)) * 170; pr.dmg = a.dmg * 0.6f; pr.life = 0.6f;
          projs.push_back(pr);
        }
        sfx((int)Sfx::Fireball, a.p, 0.5f);
      }
    }
    return;
  }

  // behaviour sets: wolves circle and lunge from the flank, goblins swarm and flee when hurt, bandit archers
  // kite, bears and trolls mix in a slow heavy slam you roll through
  // (M3c LIFE) the Wildlands wildlife: hyena packs and ember hounds circle and lunge like wolves (the hound's bite
  // sets you alight), the yeti slams like a troll (and its slam frosts you), the scorpion stings hard every third
  // blow, the lurker lies low and lunges from close by, the wisp kites and flickers and throws bolts of light, the
  // blightspawn shambles in and bursts spores round itself now and then
  // (M6 FOES) the harpy circles like a pack hunter and dives on you (a swoop you roll through), and shrieks; the golem
  // is a brute whose every second blow is the heavy ground slam
  const bool harpy = a.mon == Monster::Harpy;
  const bool wolf = a.mon == Monster::Wolf || a.mon == Monster::IceWolf;
  const bool pack = wolf || a.mon == Monster::Hyena || a.mon == Monster::EmberHound || harpy;
  const bool goblin = a.mon == Monster::Goblin;
  const bool golem = a.mon == Monster::Golem;
  const bool brute = a.mon == Monster::Bear || a.mon == Monster::Troll || a.mon == Monster::Yeti || golem;
  const bool archer = a.ranged && a.human;
  const bool wisp = a.mon == Monster::Wisp;
  const bool kite = archer || wisp;
  const float wu = a.heavy ? heavyWindup(a.mon) : (harpy && a.lunge ? 0.5f : a.windup);   // (the harpy rises before the dive)
  // a blow at the current target only (the hound's fire bite, the scorpion's sting): in reach and in front
  auto hitTarget = [&](float mult, float extraReach, Ench e, float ep) -> bool {
    const Vec2 d = p.p - a.p;
    const float l = len(d);
    if (l > a.range + p.radius + extraReach || (l > 4 && dot(d * (1.0f / l), a.aim) < 0.1f)) return false;
    // (M3c fixer round 3, review: "a dodge roll does not stop the scorpion's sting venom") true only when the blow
    // LANDED: a roll's iframes, the grace after a hit or a block drop it inside damage(), and then no rider (venom, fire)
    const float hp0 = p.hp;
    damage(p, a.dmg * mult * (0.9f + rng_.f() * 0.2f), a.p, a.id, e, ep);
    return p.hp < hp0;
  };
  // the blightspawn's spore burst: a ring of short-lived spore clots flung out round it (they sting and slow)
  auto sporeBurst = [&]() {
    for (int k = 0; k < 10; k++) {
      Projectile pr;
      const float an = k * (TAU / 10) + rng_.f() * 0.3f;
      pr.kind = ProjKind::Spit; pr.fromPlayer = false; pr.owner = a.id; pr.fac = a.faction;
      pr.p = a.p + Vec2(0, -6); pr.v = Vec2(std::cos(an), std::sin(an) * 0.7f) * 105.0f;
      pr.dmg = a.dmg * 0.6f; pr.life = 0.42f; pr.radius = 4; pr.ench = Ench::Frost; pr.enchPow = 1;
      projs.push_back(pr);
    }
    for (int k = 0; k < 6; k++) emit(Ev::Dust, a.p + Vec2(std::cos(k * 1.047f) * 14, std::sin(k * 1.047f) * 9));
    sfx((int)Sfx::Splash, a.p, 0.7f, 0.9f);
  };
  switch (a.st) {
    case AState::Windup:
      a.face = faceOf(toP);
      if (!a.lunge) a.aim = norm(toP);
      if (a.heavy && a.stT < wu - 0.2f) a.aim = norm(toP);   // tracks you, then commits
      if (a.stT >= wu) {
        a.st = AState::Strike; a.stT = 0; a.atkN++; a.special = 0;
        if (a.ranged && dist > a.range + 6) {
          Projectile pr;
          pr.owner = a.id; pr.fromPlayer = false; pr.dmg = a.dmg; pr.fac = a.faction;
          Vec2 aim = norm(p.p + Vec2(0, -6) - (a.p + Vec2(0, -8)));
          pr.p = a.p + Vec2(0, -8) + aim * 5;
          if (a.human) { pr.kind = ProjKind::Arrow; pr.v = aim * 210; pr.life = 1.0f; sfx((int)Sfx::Arrow, a.p, 0.9f); }
          else if (a.mon == Monster::Wraith) { pr.kind = ProjKind::Magic; pr.v = aim * 130; pr.life = 1.6f; pr.ench = Ench::Frost; pr.enchPow = 4; sfx((int)Sfx::Frost, a.p, 0.8f); }
          else if (wisp) { pr.kind = ProjKind::Magic; pr.v = aim * 120; pr.life = 1.8f; pr.radius = 3; sfx((int)Sfx::Frost, a.p, 1.7f, 0.6f); }   // (a bolt of light: no frost)
          else { pr.kind = ProjKind::Spit; pr.v = aim * 150; pr.life = 1.0f; sfx((int)Sfx::Splash, a.p, 1.4f); }
          projs.push_back(pr);
          a.atkCd = 1.6f + rng_.f();
        } else if (a.heavy && a.mon == Monster::Scorpion) {
          // the sting: a long reach over the claws, hard, and its venom slows
          a.aim = norm(toP);
          if (hitTarget(1.8f, 9.0f, Ench::None, 0) && p.player && !godMode) {
            p.slowT = std::max(p.slowT, 2.5f);
            emit(Ev::Text, p.p + Vec2(0, -26), (int)rgba(196, 236, 96), 0, "VENOM");
          }
          sfx((int)Sfx::Hit, a.p, 0.6f);
          a.vel = a.aim * 50.0f;
          a.atkCd = 1.3f + rng_.f() * 0.5f;
        } else if (a.heavy && a.mon == Monster::Blightspawn) {
          sporeBurst();
          a.atkCd = 1.2f + rng_.f() * 0.5f;
        } else if (a.heavy) {
          heavySlam(a);
          a.vel = a.aim * 30.0f;
          a.atkCd = 1.5f + rng_.f() * 0.6f;
        } else if (a.lunge) {
          a.aim = norm(toP);
          a.vel = a.aim * (a.mon == Monster::Lurker ? 260.0f : harpy ? 270.0f : 235.0f);
          a.hitDone = false;
          a.atkCd = 0.8f + rng_.f() * 0.6f;
          sfx((int)Sfx::Swing, a.p, 0.7f);
        } else if (a.mon == Monster::EmberHound) {
          hitTarget(1.0f, 2.0f, Ench::Fire, 4.0f);
          a.atkCd = 1.0f + rng_.f() * 0.6f;
          a.vel = a.aim * 120.0f;
          sfx((int)Sfx::Fireball, a.p, 1.6f, 0.5f);
        } else {
          meleeHit(a);
          a.atkCd = 1.0f + rng_.f() * 0.7f;
          if (pack || a.mon == Monster::Boar || goblin) a.vel = a.aim * 120.0f;
          else a.vel = a.aim * 40.0f;
          sfx((int)Sfx::Swing, a.p, 0.8f);
        }
      }
      return;
    case AState::Strike:
      if (a.lunge && !a.hitDone && len2(a.vel) > 1) {   // a lunge bends a little toward a dodging target
        float sp = len(a.vel);
        a.vel = norm(norm(a.vel) + norm(toP) * std::min(1.0f, dt * 4.0f)) * sp;
      }
      moveActor(a, a.vel * dt);
      a.vel *= std::pow(0.01f, dt);
      if (a.lunge && !a.hitDone && len(p.p - a.p) < a.range + p.radius + 2) {   // the bite lands on contact
        a.aim = norm(p.p - a.p);
        const bool ember = a.mon == Monster::EmberHound;
        damage(p, a.dmg * (a.mon == Monster::Lurker ? 1.3f : 1.0f) * (0.9f + rng_.f() * 0.2f), a.p, a.id, ember ? Ench::Fire : Ench::None, ember ? 4.0f : 0.0f);
        a.hitDone = true;
        a.vel *= 0.25f;
      }
      if (a.stT >= (a.heavy ? 0.4f : 0.28f)) { a.st = AState::Recover; a.stT = 0; }
      return;
    case AState::Recover:
      if (a.stT >= (a.heavy ? 0.75f : 0.35f)) { a.st = AState::Idle; a.stT = 0; a.heavy = false; a.lunge = false; }
      return;
    default: break;
  }

  if (a.aggro && p.st != AState::Dead) {
    a.aim = norm(toP);
    a.face = faceOf(toP);
    Vec2 side(-a.aim.y, a.aim.x);
    float want = a.ranged ? 70.0f : a.range * 0.8f;
    if (a.mon == Monster::Scorpion) want = a.range * 0.7f;
    float spd = 1.0f;
    Vec2 mv;
    if (a.fleeing) {
      // run, weaving a little; once well away, give up the fight
      mv = a.aim * -1.0f + side * (std::sin(a.animT * 3 + a.id) * 0.5f);
      spd = 1.1f;
      if (dist > 11 * TILE) { a.aggro = false; a.fleeing = false; a.goal = a.home; }
    } else if (pack) {
      // circle at a few strides, drifting in and out; the lunge comes from wherever the player isn't looking
      // (hyenas ring wider: the pack surrounds you before one darts in)
      float orbitR = (a.mon == Monster::Hyena ? 46.0f : harpy ? 54.0f : 38.0f) + (a.id % 3) * 5.0f;
      float radial = clampf((dist - orbitR) / 14.0f, -1.0f, 1.0f);
      mv = a.aim * radial + side * (0.95f * a.orbitDir);
      spd = 0.85f;
      if (rng_.f() < dt * 0.35f) a.orbitDir = (int8_t)-a.orbitDir;
      a.special += dt;   // time spent circling
    } else if (goblin) {
      // swarm: each goblin closes in from its own side so the pack surrounds you
      float spread = (float)((int)(a.id % 3) - 1) * 0.75f;
      if (dist > want) mv = a.aim + side * (dist > 28 ? spread : 0.0f);
      spd = 1.05f;
    } else if (kite) {
      // keep a bow-shot away: back off when crowded, close in when far, strafe in between
      if (dist < 62) mv = a.aim * -1.0f + side * (0.5f * a.orbitDir);
      else if (dist > 115) mv = a.aim;
      else mv = side * (0.6f * a.orbitDir);
      if (rng_.f() < dt * 0.4f) a.orbitDir = (int8_t)-a.orbitDir;
    } else if (dist > want) mv = a.aim;
    if (a.mon == Monster::Bat || a.mon == Monster::Wraith || wisp || harpy) {   // erratic flight
      float w = std::sin(a.animT * 5 + a.id) * 0.8f;
      mv = mv + side * w;
    }
    if (wisp && rng_.f() < dt * 0.3f) {   // the wisp flickers out and back a few strides aside
      const Vec2 to = a.p + side * (rng_.f() < 0.5f ? 22.0f : -22.0f);
      if (bodyFree(to, a.radius, true)) { emit(Ev::Sparkle, a.p + Vec2(0, -8)); a.p = to; emit(Ev::Sparkle, a.p + Vec2(0, -8)); }
    }
    // simple obstacle avoidance: if stuck, sidestep
    Vec2 before = a.p;
    if (len2(mv) > 0.01f) {
      moveActor(a, norm(mv) * (a.speed * spd * slow * dt));
      a.st = AState::Walk;
      if (len2(a.p - before) < 0.02f * a.speed * dt) {
        Vec2 sd = side * ((a.id & 1) ? 1.0f : -1.0f);
        moveActor(a, sd * (a.speed * slow * dt));
        if (pack) a.orbitDir = (int8_t)-a.orbitDir;
        if (a.fleeing && dist < 34) a.fleeing = false;   // cornered: fight
      }
    } else a.st = AState::Idle;
    if (a.fleeing) return;
    if (harpy && a.shootCd <= 0 && dist < 84.0f && a.st != AState::Windup) {
      // the shriek: a scream that rattles whoever is close (a short slow); the harpy hangs in the air a moment
      a.shootCd = 7.0f + rng_.f() * 3.0f;
      emit(Ev::Text, a.p + Vec2(0, -24), (int)rgba(236, 200, 255), 0, "SHRIEK");
      emit(Ev::Shake, a.p, 0, 1.5f);
      sfx((int)Sfx::Roar, a.p, 2.3f, 0.7f);
      if (p.player && !godMode && p.iframes <= 0) p.slowT = std::max(p.slowT, 1.4f);
      a.atkCd = std::max(a.atkCd, 0.6f);
    }
    bool inMelee = dist < a.range + p.radius + 2;
    bool inShot = a.ranged && dist < 150 && dist > 30;
    if (a.atkCd > 0) return;
    if (pack) {
      // the pack takes turns (two at once in a big pack); prefer the flank, but don't circle forever
      int busy = 0, mates = 0;
      for (const Actor& o : actors) {
        if (o.id == a.id || o.st == AState::Dead || o.mon != a.mon || len2(o.p - p.p) > 140 * 140) continue;
        mates++;
        if (o.lunge && o.st == AState::Windup) busy++;
      }
      int slots = mates >= 2 ? 2 : 1;
      bool flank = dot(norm(a.p - p.p), p.aim) < 0.3f;
      const float lungeMax = a.mon == Monster::Hyena ? 62.0f : a.mon == Monster::EmberHound ? 54.0f : harpy ? 76.0f : 48.0f;   // (beyond the pack's ring)
      if (busy < slots && dist > 16 && dist < lungeMax && (flank || a.special > 0.7f)) {
        a.st = AState::Windup; a.stT = 0; a.lunge = true;
      } else if (inMelee && dist <= 16 && busy < slots) {
        a.st = AState::Windup; a.stT = 0; a.lunge = false;
      }
      return;
    }
    if (brute) {
      bool heavyNow = golem ? (a.atkN % 2 == 1) : ((a.atkN % 3 == 2) || rng_.f() < 0.15f);
      if (inMelee || (heavyNow && dist < a.range + p.radius + 12)) { a.st = AState::Windup; a.stT = 0; a.heavy = heavyNow; }
      return;
    }
    if (a.mon == Monster::Scorpion) {   // claws twice, then the sting (telegraphed: roll away or through it)
      const bool sting = a.atkN % 3 == 2;
      if (inMelee || (sting && dist < a.range + p.radius + 8)) { a.st = AState::Windup; a.stT = 0; a.heavy = sting; }
      return;
    }
    if (a.mon == Monster::Lurker) {   // a short rush from a few strides, or a snap up close
      if (dist > 18 && dist < 58) { a.st = AState::Windup; a.stT = 0; a.lunge = true; }
      else if (inMelee) { a.st = AState::Windup; a.stT = 0; a.lunge = false; }
      return;
    }
    if (a.mon == Monster::Blightspawn && a.shootCd <= 0 && dist < 44) {   // the spore burst (its ring shows the cloud)
      a.st = AState::Windup; a.stT = 0; a.heavy = true;
      a.shootCd = 6.0f + rng_.f() * 2.0f;
      return;
    }
    if (inMelee || (inShot && rng_.f() < dt * 2.5f)) { a.st = AState::Windup; a.stT = 0; }
  } else {
    Vec2 d = a.goal - a.p;
    float l = len(d);
    // a pack that lost its prey trots back to the den
    float back = (a.den >= 0 && len2(a.p - a.home) > 5.0f * TILE * 5.0f * TILE) ? 0.7f : 0.35f;
    if (back > 0.5f) { d = a.home - a.p; l = len(d); }
    if (l > 4) { moveActor(a, d * (1.0f / l) * (a.speed * back * slow * dt)); a.face = faceOf(d); a.st = AState::Walk; }
    else a.st = AState::Idle;
  }
}

void Game::heavySlam(Actor& a) {
  // a ground slam in front of the brute: big damage and knockback, no cone check. Rolling through it is the answer.
  // (M6) the golem's slam is wider: the ground cracks round its fists
  const bool golem = a.mon == Monster::Golem;
  Vec2 c = a.p + a.aim * (golem ? 12.0f : 10.0f);
  float r = (golem ? 32.0f : 26.0f) + a.radius;
  for (Actor& v : actors) {
    if (v.id == a.id || v.st == AState::Dead || v.fly) continue;
    if (!(v.player || v.npc) || !factionsHostile(a.faction, v.faction)) continue;
    if (len2(v.p - c) > (r + v.radius) * (r + v.radius)) continue;
    // (M3c) the yeti's slam is frost-bitten: it slows whoever it catches
    const bool yeti = a.mon == Monster::Yeti;
    damage(v, a.dmg * 2.1f * (0.9f + rng_.f() * 0.2f), a.p, a.id, yeti ? Ench::Frost : Ench::None, yeti ? 2.0f : 0.0f);
    if (v.player && v.st != AState::Roll && v.iframes <= 0.36f) v.knock = norm(v.p - a.p) * (golem ? 230.0f : 190.0f);
  }
  emit(Ev::Shake, c, 0, golem ? 6.0f : 4.5f);
  if (golem) for (int k = 0; k < 10; k++) emit(Ev::Dust, c + Vec2(std::cos(k * 0.628f) * r * 0.9f, std::sin(k * 0.628f) * r * 0.6f));
  for (int k = 0; k < 6; k++) emit(Ev::Dust, c + Vec2(std::cos(k * 1.047f) * r * 0.6f, std::sin(k * 1.047f) * r * 0.4f));
  sfx((int)Sfx::HitHeavy, a.p, 0.55f);
}
