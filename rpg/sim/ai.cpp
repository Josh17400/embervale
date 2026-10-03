// EMBERVALE AI: per-species monster behaviour, NPC and guard behaviour, the heavy slam.
// (M0 town-defence lane: factions, monsters targeting villagers and guards, fleeing, militia, the bell.)
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/sim/game_internal.h"

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
  for (int i = 0; i < (int)world.sites.size(); i++) {
    const Site& s = world.sites[i];
    if (!isSettlement(s.type)) continue;
    if (tx >= s.r.x - 1 && ty >= s.r.y - 1 && tx < s.r.x + s.r.w + 1 && ty < s.r.y + s.r.h + 1) return i;
  }
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
  for (int b = s.bldgFirst; b < s.bldgFirst + s.bldgCount && b < (int)m.bldgs.size(); b++) {
    const Bldg& B = m.bldgs[b];
    if (m.blocked(B.doorX(), B.doorY() + 1)) continue;   // a door you can't stand in front of
    float d = len(tileCentre(B.doorX(), B.doorY() + 1) - a.home);
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
    int x0 = std::max(0, std::min(sx, gx) - 10), y0 = std::max(0, std::min(sy, gy) - 10);
    int x1 = std::min(m.w - 1, std::max(sx, gx) + 10), y1 = std::min(m.h - 1, std::max(sy, gy) + 10);
    int ww = x1 - x0 + 1, hh = y1 - y0 + 1;
    if (ww > 96 || hh > 96 || !m.in(sx, sy) || !m.in(gx, gy)) return false;
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
    if (len2(a.p - before) < 0.0001f) a.navT = std::min(a.navT, 0.1f);   // wedged on a corner: re-plan soon
  }
  return true;
}

// Friendly NPCs: guards (and militia) fight what threatens the town, everyone else runs home and hides, and
// otherwise they stroll around where they live.
void Game::updateFolk(Actor& a, float dt) {
  Actor& p = pl();
  bool talking = mode == Mode::Dialogue && dlg.actor == a.id;
  bool town = !inside && a.site >= 0 && a.fromMap && a.site < (int)world.sites.size() && isSettlement(world.sites[a.site].type);
  const SiteAlarm* al = nullptr;
  if (town) { auto it = alarms_.find(a.site); if (it != alarms_.end()) al = &it->second; }
  const bool ringing = al && al->ringing;
  const bool hurt = a.militia && a.hp < a.maxHp * 0.4f;   // militia run when badly hurt
  const bool guard = a.role == Role::Guard;
  if (guard || (a.militia && !hurt)) {
    a.thinkT -= dt;
    if (a.thinkT <= 0) {
      a.thinkT = 0.4f;
      a.target = -1;
      float bd = 1e30f;
      const Site* st = town ? &world.sites[a.site] : nullptr;
      for (const Actor& e : actors) {
        if (e.player || e.st == AState::Dead || e.fly || !factionsHostile(a.faction, e.faction)) continue;
        float d2 = len2(e.p - a.p);
        bool nearMe = d2 < (guard ? 110.0f * 110.0f : 90.0f * 90.0f);
        bool inTown = false;
        if (guard && ringing && st) {   // the bell calls every guard: converge on anything inside the walls
          int tx = tileX(e.p), ty = tileY(e.p);
          inTown = tx >= st->r.x - 4 && ty >= st->r.y - 4 && tx < st->r.x + st->r.w + 4 && ty < st->r.y + st->r.h + 4;
        }
        if (!nearMe && !inTown) continue;
        if (a.militia && len2(e.p - a.home) > (10.0f * TILE) * (10.0f * TILE)) continue;   // militia defend their own street
        if (d2 < bd) { bd = d2; a.target = e.id; }
      }
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
        if (!navStep(a, t.p, a.speed * 1.2f, dt)) moveActor(a, a.aim * (a.speed * 1.2f * dt));
        a.st = AState::Walk;
      } else if (a.atkCd <= 0) { a.st = AState::Windup; a.stT = 0; }
      else a.st = AState::Idle;
      return;
    }
    if (a.militia) a.look.weapon = calmTool(a.role);
  }
  if (talking) { a.face = faceOf(p.p - a.p); a.st = AState::Idle; return; }
  // townsfolk run home and hide from monsters on the loose (and from the bell); they come back out when it's over
  if (town && !guard) {
    const Actor* threat = nullptr;
    float td = 100.0f * 100.0f;
    for (const Actor& e : actors)
      if (!e.player && e.st != AState::Dead && !e.fly && factionsHostile(a.faction, e.faction) && len2(e.p - a.p) < td) { td = len2(e.p - a.p); threat = &e; }
    if (threat || ringing) {
      if (threat) alarms_[a.site].lastThreatT = time;
      a.fleeT += dt;
      bool ran = false;
      int b = homeDoor(a);
      if (b >= 0) {
        const Bldg& B = world.over.bldgs[b];
        Vec2 door = tileCentre(B.doorX(), B.doorY() + 1);
        if (len2(a.p - door) < 7.0f * 7.0f) { a.indoors = true; a.st = AState::Idle; return; }
        ran = navStep(a, door, a.speed * 1.25f, dt);
      }
      if (!ran) {
        if (!threat) { a.st = AState::Idle; return; }
        Vec2 away = norm(a.p - threat->p);
        moveActor(a, away * (a.speed * 1.15f * dt));
        a.face = faceOf(away);
      }
      a.st = AState::Walk;
      a.goal = a.p; a.thinkT = 1.0f;   // when it's over, stay put a moment before wandering again
      return;
    }
    a.fleeT = 0;
  }
  a.thinkT -= dt;
  if (a.thinkT <= 0) {
    a.thinkT = 2.0f + rng_.f() * 4.0f;
    if (rng_.f() < 0.45f) a.goal = a.p;
    else {
      float range = inside ? 3.0f : 6.0f;
      a.goal = a.home + Vec2(rng_.range(-range, range) * TILE, rng_.range(-range * 0.4f, range * 0.4f) * TILE);
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
  const Actor& p = pl();
  for (int si : activeSites_) {
    const Site& st = world.sites[si];
    if (!isSettlement(st.type)) continue;
    SiteAlarm& al = alarms_[si];
    int n = 0, near = 0;   // inside the footprint (the bell) / prowling at its edge (keeps folk indoors)
    for (const Actor& e : actors) {
      if (e.player || e.st == AState::Dead || !factionsHostile(Faction::Town, e.faction)) continue;
      int tx = tileX(e.p), ty = tileY(e.p);
      if (tx >= st.r.x - 1 && ty >= st.r.y - 1 && tx < st.r.x + st.r.w + 1 && ty < st.r.y + st.r.h + 1) n++;
      if (tx >= st.r.x - 6 && ty >= st.r.y - 6 && tx < st.r.x + st.r.w + 6 && ty < st.r.y + st.r.h + 6) near++;
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
    int b = s.homeBldg;
    if (safe && b >= 0 && b < (int)world.over.bldgs.size()) {
      const Bldg& B = world.over.bldgs[b];
      Vec2 door = tileCentre(B.doorX(), B.doorY() + 1);
      for (const Actor& e : actors)
        if (!e.player && e.st != AState::Dead && factionsHostile(s.faction, e.faction) && len2(e.p - door) < 140.0f * 140.0f) safe = false;
      if (safe) {
        s.p = door; s.vel = Vec2(); s.fleeT = 0; s.fleeing = false; s.hp = s.maxHp; s.face = 0;
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

  // ---- friendly NPCs
  if (!a.hostile) { updateFolk(a, dt); return; }

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
    if (!a.boss && !inside && dist > std::max(a.aggroR * 2.4f, 15.0f * TILE)) a.aggro = false;
    if (p.st == AState::Dead) a.aggro = false;
    if (!a.aggro) a.fleeing = false;
    if (!a.aggro && a.st != AState::Windup) {
      if (rng_.f() < 0.25f) {
        float r = a.wild ? 5.0f : 3.0f;
        a.goal = a.home + Vec2(rng_.range(-r, r) * TILE, rng_.range(-r, r) * TILE);
      }
    }
  }

  // dragon: its own dance
  if (a.mon == Monster::Dragon) {
    a.special -= dt;
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
  const bool wolf = a.mon == Monster::Wolf || a.mon == Monster::IceWolf;
  const bool goblin = a.mon == Monster::Goblin;
  const bool brute = a.mon == Monster::Bear || a.mon == Monster::Troll;
  const bool archer = a.ranged && a.human;
  const float wu = a.heavy ? (a.mon == Monster::Troll ? 0.9f : 0.8f) : a.windup;
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
          else { pr.kind = ProjKind::Spit; pr.v = aim * 150; pr.life = 1.0f; sfx((int)Sfx::Splash, a.p, 1.4f); }
          projs.push_back(pr);
          a.atkCd = 1.6f + rng_.f();
        } else if (a.heavy) {
          heavySlam(a);
          a.vel = a.aim * 30.0f;
          a.atkCd = 1.5f + rng_.f() * 0.6f;
        } else if (a.lunge) {
          a.aim = norm(toP);
          a.vel = a.aim * 235.0f;
          a.hitDone = false;
          a.atkCd = 0.8f + rng_.f() * 0.6f;
          sfx((int)Sfx::Swing, a.p, 0.7f);
        } else {
          meleeHit(a);
          a.atkCd = 1.0f + rng_.f() * 0.7f;
          if (wolf || a.mon == Monster::Boar || goblin) a.vel = a.aim * 120.0f;
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
        damage(p, a.dmg * (0.9f + rng_.f() * 0.2f), a.p, a.id);
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
    float spd = 1.0f;
    Vec2 mv;
    if (a.fleeing) {
      // run, weaving a little; once well away, give up the fight
      mv = a.aim * -1.0f + side * (std::sin(a.animT * 3 + a.id) * 0.5f);
      spd = 1.1f;
      if (dist > 11 * TILE) { a.aggro = false; a.fleeing = false; a.goal = a.home; }
    } else if (wolf) {
      // circle at a few strides, drifting in and out; the lunge comes from wherever the player isn't looking
      float orbitR = 38.0f + (a.id % 3) * 5.0f;
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
    } else if (archer) {
      // keep a bow-shot away: back off when crowded, close in when far, strafe in between
      if (dist < 62) mv = a.aim * -1.0f + side * (0.5f * a.orbitDir);
      else if (dist > 115) mv = a.aim;
      else mv = side * (0.6f * a.orbitDir);
      if (rng_.f() < dt * 0.4f) a.orbitDir = (int8_t)-a.orbitDir;
    } else if (dist > want) mv = a.aim;
    if (a.mon == Monster::Bat || a.mon == Monster::Wraith) {   // erratic flight
      float w = std::sin(a.animT * 5 + a.id) * 0.8f;
      mv = mv + side * w;
    }
    // simple obstacle avoidance: if stuck, sidestep
    Vec2 before = a.p;
    if (len2(mv) > 0.01f) {
      moveActor(a, norm(mv) * (a.speed * spd * slow * dt));
      a.st = AState::Walk;
      if (len2(a.p - before) < 0.02f * a.speed * dt) {
        Vec2 sd = side * ((a.id & 1) ? 1.0f : -1.0f);
        moveActor(a, sd * (a.speed * slow * dt));
        if (wolf) a.orbitDir = (int8_t)-a.orbitDir;
        if (a.fleeing && dist < 34) a.fleeing = false;   // cornered: fight
      }
    } else a.st = AState::Idle;
    if (a.fleeing) return;
    bool inMelee = dist < a.range + p.radius + 2;
    bool inShot = a.ranged && dist < 150 && dist > 30;
    if (a.atkCd > 0) return;
    if (wolf) {
      // the pack takes turns (two at once in a big pack); prefer the flank, but don't circle forever
      int busy = 0, mates = 0;
      for (const Actor& o : actors) {
        if (o.id == a.id || o.st == AState::Dead || o.mon != a.mon || len2(o.p - p.p) > 140 * 140) continue;
        mates++;
        if (o.lunge && o.st == AState::Windup) busy++;
      }
      int slots = mates >= 2 ? 2 : 1;
      bool flank = dot(norm(a.p - p.p), p.aim) < 0.3f;
      if (busy < slots && dist > 16 && dist < 48 && (flank || a.special > 0.7f)) {
        a.st = AState::Windup; a.stT = 0; a.lunge = true;
      } else if (inMelee && dist <= 16 && busy < slots) {
        a.st = AState::Windup; a.stT = 0; a.lunge = false;
      }
      return;
    }
    if (brute) {
      bool heavyNow = (a.atkN % 3 == 2) || rng_.f() < 0.15f;
      if (inMelee || (heavyNow && dist < a.range + p.radius + 12)) { a.st = AState::Windup; a.stT = 0; a.heavy = heavyNow; }
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
  Vec2 c = a.p + a.aim * 10.0f;
  float r = 26.0f + a.radius;
  for (Actor& v : actors) {
    if (v.id == a.id || v.st == AState::Dead || v.fly) continue;
    if (!(v.player || v.npc) || !factionsHostile(a.faction, v.faction)) continue;
    if (len2(v.p - c) > (r + v.radius) * (r + v.radius)) continue;
    damage(v, a.dmg * 2.1f * (0.9f + rng_.f() * 0.2f), a.p, a.id);
    if (v.player && v.st != AState::Roll && v.iframes <= 0.36f) v.knock = norm(v.p - a.p) * 190.0f;
  }
  emit(Ev::Shake, c, 0, 4.5f);
  for (int k = 0; k < 6; k++) emit(Ev::Dust, c + Vec2(std::cos(k * 1.047f) * r * 0.6f, std::sin(k * 1.047f) * r * 0.4f));
  sfx((int)Sfx::HitHeavy, a.p, 0.55f);
}
