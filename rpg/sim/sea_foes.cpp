// M10 "Beyond the Mist": the sea's creatures and pirates (VISION_PLAN 2.12, 7.6; rpg/sim/sea_ops.h). SEA lane.
//
// Every blow is telegraphed (SeaEv::Telegraph: where it lands and when, and the attacker's Windup state for the view's
// danger ring), so a captain who watches the water can steer clear:
//   - the SEA SERPENT circles a boat half under the swell, then RISES at where the boat will be (a lunge from below) or
//     SWEEPS its tail round itself; a world boss of the open sea (foes::seaBossOf lairs) or a lone one of the deep;
//   - the KRAKEN lies in the storm sea; its TENTACLES rise round the boat one by one (a shadow first), slam the deck,
//     and sink again; a tentacle cut costs the kraken dearly; a world boss;
//   - MERROW come in bands of two to four out of reefs and wrecks: amphibious (Actor::swim 2), spears telegraphed;
//   - PIRATES (Role::Pirate) on their own black-sailed sloop (Sea::Ship) close in on the hero's boat, come alongside and
//     board; beaten, the sloop cuts loose and flees.
// The world bosses' phases at 66 % and 33 % are this file's (foeStep leaves swimmers alone): an enraged serpent, a
// kraken with more arms in the water.
#include "rpg/sim/sea_ops.h"

#include "engine/audio.h"

#include <algorithm>
#include <cmath>
#include "rpg/sim/foes.h"
#include "rpg/sim/game_internal.h"
#include "rpg/world/source.h"

using namespace sea;
using gsim::faceOf;

namespace {
constexpr float kPi = 3.14159265f;
enum : uint8_t { SW_SERPENT = 1, SW_KRAKEN, SW_TENTACLE, SW_MERROW, SW_PIRATE };
enum : uint8_t { MV_NONE, MV_RISE, MV_SWEEP, MV_SLAM, MV_THRUST, MV_SINK, MV_DECK, MV_JUMP, MV_GONE };
constexpr uint64_t kBossSlainTag = 65;   // foes::MK_FOES_BOSS_SLAIN

Sea::Swimmer* swimmerOf(Game& g, int id) {
  for (Sea::Swimmer& s : g.sea.live.swimmers) if (s.id == id) return &s;
  return nullptr;
}
Sea::Ship* shipOf(Game& g, uint32_t id) {
  for (Sea::Ship& s : g.sea.live.ships) if (s.id == id) return &s;
  return nullptr;
}
bool waterAt(const Game& g, Vec2 p) {
  const Map& m = g.world.over;
  const int tx = (int)std::floor(p.x / TILE), ty = (int)std::floor(p.y / TILE);
  return m.in(tx, ty) && groundWater(m.at(tx, ty));
}
// a free spot on the water at minR..maxR px from `near` for a body of radius r (false: none)
bool waterSpot(Game& g, Vec2 near, float minR, float maxR, float r, uint8_t swim, Vec2& out) {
  for (int k = 0; k < 24; k++) {
    const float a = SeaOps::rng(g).f() * 6.2831853f, d = minR + (maxR - minR) * SeaOps::rng(g).f();
    const Vec2 c = near + Vec2(std::cos(a) * d, std::sin(a) * d);
    if (!waterAt(g, c)) continue;
    if (!g.world.over.in((int)std::floor(c.x / TILE), (int)std::floor(c.y / TILE))) continue;
    bool free = true;
    for (int s = 0; s < 4 && free; s++) {
      const Vec2 o = c + Vec2(s & 1 ? r : -r, s & 2 ? r * 0.6f : -r * 0.6f);
      free = waterAt(g, o);
    }
    (void)swim;
    if (free) { out = c; return true; }
  }
  return false;
}
// a blow lands at `mark`: the hero (aboard, the hull takes most of it), companions in reach
void strikeAt(Game& g, Actor& a, Vec2 mark, float reach, float dmg) {
  Actor& p = g.pl();
  if (p.st != AState::Dead && len(p.p - mark) < reach + p.radius) {
    if (g.sea.atSea()) {
      SeaOps::hurtBoat(g, dmg * 0.7f, mark);
      SeaOps::hit(g, p, dmg * 0.35f, a.p, a.id);
    } else SeaOps::hit(g, p, dmg, a.p, a.id);
  }
  for (size_t i = 1; i < g.actors.size(); i++) {
    Actor& c = g.actors[i];
    if (!c.comp || c.st == AState::Dead || len(c.p - mark) > reach + c.radius) continue;
    SeaOps::hit(g, c, dmg * 0.8f, a.p, a.id);
  }
}
void telegraph(Game& g, Actor& a, Sea::Swimmer& sw, uint8_t move, Vec2 mark, float secs, bool heavy) {
  sw.move = move;
  sw.mark = mark;
  a.st = AState::Windup;
  a.stT = 0;
  a.windup = secs;
  a.heavy = heavy;
  a.aim = norm(mark - a.p);
  SeaOps::ev(g, SeaEv::Telegraph, mark, secs, (uint32_t)a.id, "", a.p);
}
// a boss of the sea passing 66 % / 33 %: roars, grows worse
bool phaseCheck(Game& g, Actor& a) {
  if (a.rank != (uint8_t)foes::Rank::WorldBoss) return false;
  const float frac = a.maxHp > 0 ? a.hp / a.maxHp : 1.0f;
  int idx = 0;
  if (a.bossPhase < 1 && frac <= 0.66f) idx = 1;
  else if (a.bossPhase < 2 && frac <= 0.33f) idx = 2;
  if (!idx) return false;
  a.bossPhase = (uint8_t)idx;
  a.speed *= 1.15f;
  a.overlays |= art::MO_EYES;
  const std::string what = a.mon == art::Monster::Kraken ? "THRASHES IN FURY!" : "IS ENRAGED!";
  SeaOps::emitEv(g, Ev::Text, a.p + Vec2(0, -(34.0f + a.radius * 1.5f)), (int)rgba(255, 120, 80), 0, what);
  SeaOps::emitEv(g, Ev::Shake, a.p, 0, 4.0f);
  SeaOps::emitEv(g, Ev::Notice, g.pl().p, (int)rgba(255, 120, 80), 0, a.name + " " + what);
  SeaOps::snd(g, (int)Sfx::BossRoar, a.p, 0.9f + 0.1f * idx);
  return true;
}
// move a swimmer: steer its velocity toward `want` (px/s) with some inertia (no snapping about)
void swimToward(Game& g, Actor& a, Vec2 want, float dt, float agility) {
  a.vel += (want - a.vel) * std::min(1.0f, dt * agility);
  if (len2(a.vel) > 0.25f) {
    SeaOps::move(g, a, a.vel * dt);
    a.face = faceOf(a.vel);
    if (a.st == AState::Idle) a.st = AState::Walk;
  } else if (a.st == AState::Walk) a.st = AState::Idle;
}
}  // namespace

int SeaOps::foesNear(const Game& g, float px) {
  int n = 0;
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& a = g.actors[i];
    if (a.hostile && a.st != AState::Dead && len2(a.p - g.pl().p) < px * px) n++;
  }
  return n;
}

void SeaOps::dropSwimmers(Game& g) {
  g.sea.live.swimmers.clear();
  g.sea.live.ships.clear();
}

int SeaOps::spawnSwimmer(Game& g, art::Monster m, Vec2 at, int level, uint8_t kind) {
  const int id = g.spawnMonster(m, at, std::max(1, level), false);
  const int i = g.findActor(id);
  if (i < 0) return -1;
  Actor& a = g.actors[(size_t)i];
  a.swim = m == art::Monster::Merrow ? 2 : 1;
  a.wild = true;
  a.home = at; a.goal = at;
  a.aggro = kind != SW_MERROW;
  Sea::Swimmer s;
  s.id = id;
  s.kind = kind;
  s.cd = 1.5f + 1.5f * rng(g).f();
  s.dive = kind == SW_TENTACLE ? 1.0f : 0.6f;
  g.sea.live.swimmers.push_back(s);
  if (kind != SW_TENTACLE) ev(g, SeaEv::Splash, at, a.radius / 6.0f, (uint32_t)id);
  return id;
}

void SeaOps::spawnPirates(Game& g, Vec2 near, int level) {
  Sea::Live& L = g.sea.live;
  Vec2 at;
  if (!waterSpot(g, near, 0, 6.0f * TILE, 14.0f, 1, at)) return;
  Sea::Ship s;
  s.id = L.nextShip++;
  s.kind = 1;
  s.hull = Hull::Sloop;
  s.p = at;
  s.heading = std::atan2(g.pl().p.y - at.y, g.pl().p.x - at.x);
  s.speed = 3.0f;
  s.hp = hullInfo(Hull::Sloop).hull;
  L.ships.push_back(s);
  const int crew = 3 + rng(g).irange(2);
  for (int k = 0; k < crew; k++) {
    Actor a;
    a.id = g.nextId_++;
    a.human = true; a.hostile = true; a.role = Role::Pirate; a.faction = Faction::Bandit;
    a.p = at; a.home = at; a.goal = at;
    Rng rr(hash32((uint32_t)a.id * 2654435761u) ^ (uint32_t)g.seed ^ 0x50495241u);
    g.makeLook(a, Role::Bandit, rr);
    a.look.cape = false;
    a.look.tabardColor = rgba(30, 30, 34);
    a.maxHp = 46; a.dmg = 9; a.speed = 50; a.range = 15; a.aggroR = 160; a.windup = 0.4f; a.xp = 20; a.radius = 4.5f;
    a.name = k == 0 ? "PIRATE CAPTAIN" : "PIRATE";
    g.applyLevel(a, std::max(1, level));
    if (k == 0) { a.maxHp *= 2.2f; a.hp = a.maxHp; a.dmg *= 1.3f; a.xp *= 3; a.look.outfit = art::Outfit::Plate; a.look.helmet = false; }
    g.foeSpawned(a, std::max(1, level));
    a.swim = 2;
    a.wild = true;
    a.aggro = true;
    g.actors.push_back(a);
    Sea::Swimmer sw;
    sw.id = a.id;
    sw.kind = SW_PIRATE;
    sw.ship = (int)s.id;
    sw.move = MV_DECK;
    sw.parent = k;   // (its place on deck)
    sw.dive = 0;
    L.swimmers.push_back(sw);
  }
  ev(g, SeaEv::Splash, at, 1.0f);
  tell(g, "SAIL HO! A BLACK SAIL, AND SHE'S CLOSING FAST");
}

void SeaOps::voyageEvent(Game& g, int kind) {
  Actor& p = g.pl();
  const int32_t gx = heroGx(g), gy = heroGy(g);
  const int D = std::max(g.plLevel, g.world.src ? g.world.src->danger(gx, gy) : 1);
  const Vec2 windV(std::cos(g.sea.live.wind.dir), std::sin(g.sea.live.wind.dir));
  if (kind == 1) {
    const float a = g.sea.live.heading + (rng(g).f() < 0.5f ? 1.2f : -1.2f);
    spawnPirates(g, p.p + Vec2(std::cos(a), std::sin(a)) * (20.0f * TILE), D);
  } else if (kind == 2) {
    beginStorm(g, p.p - windV * (22.0f * TILE), 0.85f, 75.0f);
  } else if (kind == 3) {
    Vec2 at;
    if (waterSpot(g, p.p, 8.0f * TILE, 12.0f * TILE, 12.0f, 1, at)) {
      const int id = spawnSwimmer(g, art::Monster::SeaSerpent, at, D, SW_SERPENT);
      if (const int i = g.findActor(id); i >= 0) g.actors[(size_t)i].name = "SEA SERPENT";
      tell(g, "SOMETHING BIG RISES OUT OF THE SWELL!");
      snd(g, (int)Sfx::Roar, at, 0.6f, 1.0f);
    }
  }
}

// ---------------------------------------------------------------- the creatures' moves
bool SeaOps::aiOverride(Game& g, Actor& a, float dt) {
  if (!a.swim || a.player) return false;
  Sea::Swimmer* swp = swimmerOf(g, a.id);
  if (!swp) {   // (a swimmer without its record: a creature of the sea spawned elsewhere)
    Sea::Swimmer s;
    s.id = a.id;
    s.kind = a.human ? SW_PIRATE : a.mon == art::Monster::Kraken ? SW_KRAKEN : a.mon == art::Monster::Merrow ? SW_MERROW : SW_SERPENT;
    s.cd = 2.0f;
    g.sea.live.swimmers.push_back(s);
    swp = &g.sea.live.swimmers.back();
  }
  Sea::Swimmer& sw = *swp;
  sw.t += dt;
  if (sw.cd > 0) sw.cd -= dt;
  if (a.st == AState::Dead || sw.move == MV_GONE) return true;
  if (a.st == AState::Hurt) { if (a.stT > 0.25f) { a.st = AState::Idle; a.stT = 0; } return true; }
  Actor& p = g.pl();
  const Vec2 to = p.p - a.p;
  const float dist = len(to);
  const bool alive = p.st != AState::Dead;
  const float boatR = g.sea.atSea() ? hullInfo(g.sea.boats[(size_t)g.sea.aboard].hull).radius : p.radius;
  const Vec2 lead = g.sea.atSea() ? Vec2(std::cos(g.sea.live.heading), std::sin(g.sea.live.heading)) * (g.sea.live.speed * TILE * 0.8f) : Vec2();
  if (!a.aggro && alive && dist < a.aggroR * (sw.kind == SW_MERROW ? 1.0f : 1.6f)) a.aggro = true;
  if (!alive) a.aggro = false;
  const float far = sw.kind == SW_TENTACLE ? 1e9f : (a.rank == (uint8_t)foes::Rank::WorldBoss ? 70.0f : 45.0f) * TILE;
  if (dist > far) { sw.move = MV_GONE; return true; }   // (the sea takes it back: encounterStep removes it)
  switch (sw.kind) {
    case SW_SERPENT: {
      phaseCheck(g, a);
      if (a.st == AState::Windup) {
        sw.dive = std::max(0.0f, sw.dive - dt * 3.0f);
        if (sw.move == MV_RISE) swimToward(g, a, norm(sw.mark - a.p) * (a.speed * 0.4f), dt, 3.0f);
        if (a.stT >= a.windup) { a.st = AState::Strike; a.stT = 0; a.hitDone = false; }
        return true;
      }
      if (a.st == AState::Strike) {
        if (sw.move == MV_RISE) {
          const Vec2 d = sw.mark - a.p;
          if (len(d) > 2.0f) move(g, a, norm(d) * std::min(len(d), 260.0f * dt));
          if (!a.hitDone && a.stT >= 0.16f) {
            a.hitDone = true;
            strikeAt(g, a, sw.mark, 18.0f + boatR * 0.5f, a.dmg * 1.2f);
            ev(g, SeaEv::Splash, sw.mark, 1.6f, (uint32_t)a.id);
            snd(g, (int)Sfx::Splash, sw.mark, 0.55f, 1.0f);
            SeaOps::emitEv(g, Ev::Shake, sw.mark, 0, 2.5f);
          }
        } else if (sw.move == MV_SWEEP && !a.hitDone) {
          a.hitDone = true;
          strikeAt(g, a, sw.mark, 3.2f * TILE, a.dmg * 0.9f);
          ev(g, SeaEv::Splash, a.p, 2.2f, (uint32_t)a.id);
          snd(g, (int)Sfx::HitHeavy, a.p, 0.6f, 0.8f);
        }
        if (a.stT >= 0.3f) { a.st = AState::Recover; a.stT = 0; }
        return true;
      }
      if (a.st == AState::Recover) {
        if (a.stT >= 0.7f) {
          a.st = AState::Idle; a.stT = 0; sw.move = MV_NONE;
          sw.cd = (2.4f + 1.6f * rng(g).f()) * (a.bossPhase >= 2 ? 0.6f : a.bossPhase == 1 ? 0.8f : 1.0f);
        }
        return true;
      }
      if (!a.aggro) {   // idle: slow circles round home, low in the water
        sw.dive = std::min(1.0f, sw.dive + dt * 0.5f);
        const Vec2 h = a.home - a.p;
        swimToward(g, a, Vec2(-h.y, h.x) * (0.4f / std::max(1.0f, len(h))) * a.speed + h * 0.3f, dt, 1.0f);
        return true;
      }
      // circling the boat at five or six tiles, half under the swell; then a move
      const float ring = 5.5f * TILE + boatR;
      const Vec2 n = dist > 1 ? to * (1.0f / dist) : Vec2(1, 0);
      const Vec2 tan(-n.y * a.orbitDir, n.x * a.orbitDir);
      swimToward(g, a, tan * (a.speed * 1.1f) + n * ((dist - ring) * 1.6f), dt, 1.6f);
      sw.dive += ((dist > ring * 0.8f ? 0.75f : 0.25f) - sw.dive) * std::min(1.0f, dt * 1.5f);
      if (sw.cd <= 0 && dist < 10.0f * TILE) {
        if (dist < 4.0f * TILE && rng(g).f() < 0.45f) telegraph(g, a, sw, MV_SWEEP, a.p, 1.0f, true);
        else telegraph(g, a, sw, MV_RISE, p.p + lead, a.bossPhase >= 2 ? 0.75f : 0.9f, true);
        snd(g, (int)Sfx::Splash, a.p, 0.7f, 0.5f);
      }
      return true;
    }
    case SW_KRAKEN: {
      phaseCheck(g, a);
      sw.dive += (0.7f - sw.dive) * std::min(1.0f, dt);
      // the body keeps off at six tiles, slowly
      const float ring = 6.0f * TILE + boatR;
      if (a.aggro) swimToward(g, a, (dist > 1 ? to * (1.0f / dist) : Vec2()) * ((dist - ring) * 0.8f), dt, 0.8f);
      else swimToward(g, a, Vec2(), dt, 1.0f);
      if (!a.aggro || sw.cd > 0 || dist > 14.0f * TILE || sw.move == MV_RISE) return true;
      int arms = 0;
      for (const Sea::Swimmer& o : g.sea.live.swimmers) if (o.kind == SW_TENTACLE && o.parent == a.id && o.move != MV_GONE) arms++;
      if (arms >= 2 + a.bossPhase) return true;
      // a tentacle rises somewhere round the boat: its shadow first (the telegraph), then the arm (encounterStep makes
      // it: no actor is added while the actors are being stepped)
      for (int k = 0; k < 10; k++) {
        const float an = rng(g).f() * 6.2831853f, r = boatR + (1.4f + 1.4f * rng(g).f()) * TILE;
        const Vec2 at = p.p + Vec2(std::cos(an) * r, std::sin(an) * r);
        if (!waterAt(g, at)) continue;
        sw.mark = at;
        sw.move = MV_RISE;
        ev(g, SeaEv::Telegraph, at, 1.1f, (uint32_t)a.id, "", a.p);
        break;
      }
      sw.cd = (2.2f + 1.4f * rng(g).f()) * (a.bossPhase >= 2 ? 0.65f : a.bossPhase == 1 ? 0.8f : 1.0f);
      return true;
    }
    case SW_TENTACLE: {
      // rising (its shadow, then the arm), slams telegraphed each time, then sinks after a while
      if (sw.move == MV_SINK) {
        sw.dive = std::min(1.0f, sw.dive + dt * 2.0f);
        if (sw.dive >= 1.0f) sw.move = MV_GONE;
        return true;
      }
      if (sw.t < 1.1f) { sw.dive = 1.0f - sw.t / 1.1f; return true; }
      if (sw.dive > 0) {
        sw.dive = 0;
        ev(g, SeaEv::Tentacle, a.p, 1, (uint32_t)a.id);
        ev(g, SeaEv::Splash, a.p, 1.2f, (uint32_t)a.id);
        snd(g, (int)Sfx::Splash, a.p, 0.7f, 0.9f);
        sw.cd = 0.25f;
      }
      if (sw.t > 7.0f + (float)(a.id % 3) && a.st != AState::Windup && a.st != AState::Strike) {
        sw.move = MV_SINK;
        ev(g, SeaEv::Tentacle, a.p, 0, (uint32_t)a.id);
        return true;
      }
      if (a.st == AState::Windup) {
        if (a.stT >= a.windup) { a.st = AState::Strike; a.stT = 0; a.hitDone = false; }
        return true;
      }
      if (a.st == AState::Strike) {
        if (!a.hitDone) {
          a.hitDone = true;
          strikeAt(g, a, sw.mark, 1.0f * TILE + boatR * 0.6f, a.dmg);
          SeaOps::emitEv(g, Ev::Shake, sw.mark, 0, 2.0f);
          snd(g, (int)Sfx::HitHeavy, sw.mark, 0.5f, 0.9f);
          ev(g, SeaEv::Splash, sw.mark, 1.0f, (uint32_t)a.id);
        }
        if (a.stT > 0.35f) { a.st = AState::Recover; a.stT = 0; }
        return true;
      }
      if (a.st == AState::Recover) { if (a.stT > 0.9f) { a.st = AState::Idle; a.stT = 0; sw.cd = 1.4f + 1.2f * rng(g).f(); } return true; }
      if (sw.cd <= 0 && alive && dist < 4.0f * TILE + boatR) telegraph(g, a, sw, MV_SLAM, p.p + lead * 0.5f, 0.6f, true);
      a.face = faceOf(to);
      return true;
    }
    case SW_MERROW:
    case SW_PIRATE: {
      if (sw.kind == SW_PIRATE && sw.move == MV_DECK) {
        // riding its sloop: a place on deck, until the sloop comes alongside and they board
        const Sea::Ship* s = shipOf(g, (uint32_t)sw.ship);
        if (!s) { sw.move = MV_NONE; return true; }
        const float an = s->heading + kPi * 0.5f * (float)((sw.parent % 2) ? 1 : -1);
        const Vec2 slot = s->p + Vec2(std::cos(s->heading), std::sin(s->heading)) * (4.0f - 5.0f * (float)(sw.parent / 2)) + Vec2(std::cos(an), std::sin(an)) * 3.0f;
        a.p = slot;
        a.vel = Vec2();
        a.face = faceOf(to);
        a.st = AState::Idle;
        if (s->boarded) {
          sw.move = MV_JUMP;
          sw.off = norm(a.p - p.p) * (boatR * 0.7f);   // (where on the hero's rail it lands, from the hero)
          sw.t = 0;
          sw.cd = 0.3f + 0.35f * (float)sw.parent;      // (one after another, never all at once)
        }
        return true;
      }
      if (sw.kind == SW_PIRATE && sw.move == MV_JUMP) {
        // over the rail: a quick eased leap onto the hero's deck; until its turn it keeps its place on its own deck
        if (sw.cd > 0) {
          if (const Sea::Ship* s = shipOf(g, (uint32_t)sw.ship)) {
            const float an = s->heading + kPi * 0.5f * (float)((sw.parent % 2) ? 1 : -1);
            a.p = s->p + Vec2(std::cos(s->heading), std::sin(s->heading)) * (4.0f - 5.0f * (float)(sw.parent / 2)) + Vec2(std::cos(an), std::sin(an)) * 3.0f;
          }
          return true;
        }
        const Vec2 d = p.p + sw.off - a.p;
        const float l = len(d);
        const float k = std::min(1.0f, sw.t / 0.45f);
        if (l > 1.5f) a.p += norm(d) * std::min(l, (90.0f + 260.0f * std::sin(k * kPi * 0.5f)) * dt);
        a.face = faceOf(d);
        if (l <= 2.5f || sw.t > 1.6f) {
          sw.move = MV_NONE;
          sw.aboard = true;
          sw.off = a.p - p.p;
          ev(g, SeaEv::Boarded, a.p, 1, (uint32_t)a.id);
          a.atkCd = 0.5f + 0.4f * rng(g).f();
        }
        return true;
      }
      // a boarder rides the hero's deck: its place on it eases in to fighting reach (never left in the wake)
      if (sw.aboard) {
        if (!g.sea.atSea()) sw.aboard = false;
        else {
          const float want = boatR * 0.55f;
          const float l = len(sw.off);
          if (l > 0.1f) sw.off = sw.off * ((l + (want - l) * std::min(1.0f, dt * 2.0f)) / l);
          a.p = p.p + sw.off;
          a.vel = Vec2();
        }
      }
      // a spear (or a cutlass) telegraphed: the wind-up marks where it lands
      if (a.st == AState::Windup) {
        if (a.stT >= a.windup) { a.st = AState::Strike; a.stT = 0; a.hitDone = false; }
        return true;
      }
      if (a.st == AState::Strike) {
        if (!a.hitDone && a.stT >= 0.05f) {
          a.hitDone = true;
          strikeAt(g, a, sw.mark, 12.0f, a.dmg);
          snd(g, (int)Sfx::Swing, a.p, 1.1f, 0.6f);
        }
        if (a.stT > 0.15f) { a.st = AState::Recover; a.stT = 0; }
        return true;
      }
      if (a.st == AState::Recover) { if (a.stT > 0.35f) { a.st = AState::Idle; a.stT = 0; a.atkCd = 1.0f + 0.6f * rng(g).f(); } return true; }
      // a hurt merrow slips away under the water
      if (sw.kind == SW_MERROW && a.hp < a.maxHp * 0.25f) {
        sw.dive = std::min(1.0f, sw.dive + dt);
        swimToward(g, a, (dist > 1 ? to * (-1.0f / dist) : Vec2(1, 0)) * a.speed * 1.2f, dt, 3.0f);
        if (sw.dive >= 1.0f && dist > 10.0f * TILE) sw.move = MV_GONE;
        return true;
      }
      sw.dive = std::max(0.0f, sw.dive - dt);
      if (!a.aggro) {
        const Vec2 h = a.home - a.p;
        swimToward(g, a, len(h) > 2.0f * TILE ? norm(h) * a.speed * 0.5f : Vec2(), dt, 2.0f);
        return true;
      }
      const float reach = a.range + a.radius + boatR;
      if (dist > reach - 2.0f && !sw.aboard) {
        // close in; keep a little apart from the rest of the band
        Vec2 want = (dist > 1 ? to * (1.0f / dist) : Vec2()) * a.speed;
        for (const Sea::Swimmer& o : g.sea.live.swimmers) {
          if (o.id == a.id || (o.kind != SW_MERROW && o.kind != SW_PIRATE)) continue;
          const int oi = g.findActor(o.id);
          if (oi < 0) continue;
          const Vec2 dd = a.p - g.actors[(size_t)oi].p;
          const float dl = len(dd);
          if (dl > 0.5f && dl < 14.0f) want += dd * (30.0f / dl);
        }
        swimToward(g, a, want, dt, 4.0f);
      } else {
        a.vel = Vec2();
        a.face = faceOf(to);
        if (a.atkCd <= 0 && alive) telegraph(g, a, sw, MV_THRUST, p.p, sw.kind == SW_MERROW ? 0.45f : 0.4f, false);
        else if (a.st == AState::Walk) a.st = AState::Idle;
      }
      // a merrow never strays far from the water
      if (sw.kind == SW_MERROW && !waterAt(g, a.p) && len(a.p - a.home) > 8.0f * TILE) a.aggro = false;
      return true;
    }
    default: return false;
  }
}

// ---------------------------------------------------------------- encounters at sea
void SeaOps::encounterStep(Game& g, float dt) {
  Sea::Live& L = g.sea.live;
  // ---- a kraken's arm rising where its shadow showed (asked for while the actors were stepped)
  struct Arm { int parent; Vec2 at; };
  std::vector<Arm> arms;
  for (Sea::Swimmer& s : L.swimmers)
    if (s.kind == SW_KRAKEN && s.move == MV_RISE) { arms.push_back({s.id, s.mark}); s.move = MV_NONE; }
  for (const Arm& A : arms) {
    const int ki = g.findActor(A.parent);
    if (ki < 0) continue;
    const int level = g.actors[(size_t)ki].level;
    const int id = spawnSwimmer(g, art::Monster::Kraken, A.at, level, SW_TENTACLE);
    const int ti = g.findActor(id);
    const int k2 = g.findActor(A.parent);
    if (ti < 0 || k2 < 0) continue;
    const Actor& k = g.actors[(size_t)k2];
    Actor& t = g.actors[(size_t)ti];
    t.maxHp = t.hp = std::max(20.0f, k.maxHp * 0.06f);
    t.dmg = k.dmg * 0.8f;
    t.scalePct = 55;
    t.radius = 6.0f;
    t.name = "TENTACLE";
    t.xp = 4;
    t.rank = 0; t.affixes = 0; t.overlays = 0; t.bodyTint = k.bodyTint;
    t.aggro = true;
    if (Sea::Swimmer* ts = swimmerOf(g, id)) { ts->parent = A.parent; ts->cd = 1.1f; ts->mark = A.at; }
  }
  // ---- the records: the departed, the sunk, the cut tentacles (a cut arm costs the kraken dearly)
  for (int i = (int)L.swimmers.size() - 1; i >= 0; i--) {
    Sea::Swimmer& s = L.swimmers[(size_t)i];
    const int ai = g.findActor(s.id);
    const bool gone = ai < 0 || g.actors[(size_t)ai].st == AState::Dead;
    if (s.kind == SW_TENTACLE && gone && s.move != MV_GONE && s.move != MV_SINK) {
      const int ki = g.findActor(s.parent);
      if (ki >= 0 && g.actors[(size_t)ki].st != AState::Dead) {
        Actor& k = g.actors[(size_t)ki];
        k.hp -= k.maxHp * 0.08f;
        k.flash = 0.2f;
        SeaOps::emitEv(g, Ev::Text, k.p + Vec2(0, -30), (int)rgba(255, 200, 120), 0, "THE KRAKEN SHUDDERS");
        if (k.hp <= 0) { k.hp = 0.5f; hit(g, k, 10.0f, k.p, g.pl().id); }
      }
    }
    if (s.move == MV_GONE && ai >= 0 && g.actors[(size_t)ai].st != AState::Dead) {
      g.actors.erase(g.actors.begin() + ai);   // (sunk out of sight: no corpse, no loot)
      L.swimmers.erase(L.swimmers.begin() + i);
      continue;
    }
    if (gone) L.swimmers.erase(L.swimmers.begin() + i);
  }
  // ---- pirate sloops: close in, come alongside and board; beaten, they cut loose and flee
  for (int i = (int)L.ships.size() - 1; i >= 0; i--) {
    Sea::Ship& s = L.ships[(size_t)i];
    s.t += dt;
    int crew = 0;
    for (const Sea::Swimmer& w : L.swimmers) if (w.kind == SW_PIRATE && w.ship == (int)s.id) crew++;
    const Vec2 hero = g.pl().p;
    const Vec2 to = hero - s.p;
    const float dist = len(to);
    if (!s.fleeing && (crew == 0 || !g.sea.atSea() || s.t > 150.0f)) {
      s.fleeing = true;
      s.t = 0;
      if (crew == 0) {
        tell(g, "THE PIRATES CUT LOOSE AND FLEE. THEIR PLUNDER IS YOURS");
        g.giveGold(40 + 12 * g.plLevel + rng(g).irange(40));
      }
    }
    float want = std::atan2(to.y, to.x);
    float speed = 0;
    if (s.fleeing) {
      want += kPi;
      speed = 7.0f;
      if (s.t > 25.0f || dist > 60.0f * TILE) { L.ships.erase(L.ships.begin() + i); continue; }
    } else if (s.boarded) {
      // lashed alongside: she keeps station by the hero's beam
      const Vec2 side(-std::sin(L.heading), std::cos(L.heading));
      const float sgn = dot(side, s.p - hero) >= 0 ? 1.0f : -1.0f;
      const Vec2 station = hero + side * (sgn * (hullInfo(s.hull).radius + 8.0f + (g.sea.atSea() ? hullInfo(g.sea.boats[(size_t)g.sea.aboard].hull).radius : 6.0f)));
      s.p += (station - s.p) * std::min(1.0f, dt * 1.5f);
      s.heading += std::clamp((float)std::remainder(L.heading - s.heading, 2 * kPi), -dt, dt);
      continue;
    } else {
      const float lead = std::min(2.0f, dist / (8.0f * TILE));
      const Vec2 aim = hero + Vec2(std::cos(L.heading), std::sin(L.heading)) * (L.speed * TILE * lead) - s.p;
      want = std::atan2(aim.y, aim.x);
      speed = std::max(L.speed + 1.5f, 6.5f * windFactor(Hull::Sloop, s.heading, L.wind));
      const float reach = hullInfo(s.hull).radius + (g.sea.atSea() ? hullInfo(g.sea.boats[(size_t)g.sea.aboard].hull).radius : 6.0f) + 10.0f;
      if (dist < reach) {
        s.boarded = true;
        ev(g, SeaEv::Boarded, s.p, 1, s.id);
        tell(g, "PIRATES COME OVER THE RAIL!");
        snd(g, (int)Sfx::Cheer, s.p, 0.7f, 0.8f);
        continue;
      }
    }
    // the helm: an eased turn toward the wanted course, away from land ahead
    const Vec2 ahead = s.p + Vec2(std::cos(s.heading), std::sin(s.heading)) * (hullInfo(s.hull).radius + 2.0f * TILE);
    float turn = (float)std::remainder(want - s.heading, 2 * kPi);
    if (!waterAt(g, ahead)) turn = 1.5f;
    s.heading += std::clamp(turn, -1.1f * dt, 1.1f * dt);
    s.speed += std::clamp(speed - s.speed, -1.5f * dt, 1.2f * dt);
    const Vec2 step = Vec2(std::cos(s.heading), std::sin(s.heading)) * (s.speed * TILE * dt);
    if (waterAt(g, s.p + step + Vec2(std::cos(s.heading), std::sin(s.heading)) * hullInfo(s.hull).radius)) s.p += step;
    else s.speed *= 0.5f;
    if ((s.t - dt) * 4.0f < std::floor(s.t * 4.0f) && s.speed > 1.0f) ev(g, SeaEv::Wake, s.p - Vec2(std::cos(s.heading), std::sin(s.heading)) * 12.0f, s.speed, s.id);
  }
  // ---- what lives out here: rolls only at sea, under way in our own boat (a ferry's trouble is its voyage's event)
  if (!g.sea.atSea() || g.travelling() || !g.world.src || g.noWildSpawns) return;
  const Boat& b = g.sea.boats[(size_t)g.sea.aboard];
  if (b.ferry || L.sinkT > 0 || L.mooring) return;
  const Vec2 hero = g.pl().p;
  const int32_t gx = heroGx(g), gy = heroGy(g);
  // the world bosses of the open sea: one rises when a boat comes within 40 tiles of its lair
  if ((L.encounterT -= dt) <= 0 || L.encounterT > 60.0f) {
    L.encounterT = 1.5f;
    float& rollT = L.rollT;
    rollT += 1.5f;
    const int32_t sx = foes::seaCellOf(gx), sy = foes::seaCellOf(gy);
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++) {
        bool ok = false;
        const foes::SeaBoss B = foes::seaBossOf(*g.world.src, sx + dx, sy + dy, ok);
        if (!ok || std::hypot((double)B.lairX - gx, (double)B.lairY - gy) > 40.0) continue;
        if (g.marks.count(gsim::markKey(B.id, (Mk)kBossSlainTag))) continue;
        bool present = false;
        for (const Actor& a : g.actors) if (a.unique == B.id && a.st != AState::Dead) present = true;
        if (present) continue;
        Vec2 at;
        if (!waterSpot(g, hero, 9.0f * TILE, 13.0f * TILE, 16.0f, 1, at)) continue;
        const int id = spawnSwimmer(g, B.mon, at, B.D, B.mon == art::Monster::Kraken ? SW_KRAKEN : SW_SERPENT);
        const int i = g.findActor(id);
        if (i < 0) continue;
        Actor& a = g.actors[(size_t)i];
        a.rank = (uint8_t)foes::Rank::WorldBoss;
        a.affixes = 0;
        a.maxHp *= foes::rankHpMul(foes::Rank::WorldBoss) * foes::SEA_BOSS_HP_SHARE;
        a.hp = a.maxHp;
        a.dmg *= foes::rankDmgMul(foes::Rank::WorldBoss);
        a.xp = (int)std::lround(a.xp * foes::rankXpMul(foes::Rank::WorldBoss));
        a.overlays = B.look.overlays; a.bodyTint = B.look.tint; a.scalePct = 150;
        a.radius += 2.0f;
        a.name = B.name;
        a.unique = B.id;
        a.aggroR *= 1.4f;
        SeaOps::emitEv(g, Ev::Notice, hero, (int)rgba(255, 190, 60), 0, B.name + ", THE " + B.kind + ", RISES FROM THE DEEP!");
        snd(g, (int)Sfx::BossRoar, at, 0.8f, 1.0f);
        SeaOps::emitEv(g, Ev::Shake, at, 0, 5.0f);
      }
    // the sea's commoner perils, every half minute or so
    if (rollT < 24.0f + 18.0f * rng(g).f()) return;
    rollT = 0;
    if (foesNear(g, 30.0f * TILE) > 0 || !L.ships.empty()) return;
    const int tx = (int)std::floor(hero.x / TILE), ty = (int)std::floor(hero.y / TILE);
    const Ground gr = g.world.over.at(tx, ty);
    const int D = std::max(1, g.world.src->danger(gx, gy));
    const float r = rng(g).f();
    if (gr == Ground::Water) {
      // merrow bands out of reefs and wrecks: the shallows away from any town (more near a wreck)
      bool wreck = false;
      for (int dy = -12; dy <= 12 && !wreck; dy += 2)
        for (int dx = -12; dx <= 12 && !wreck; dx += 2) {
          const int pr = g.world.over.in(tx + dx, ty + dy) ? g.world.over.propAt(tx + dx, ty + dy) : 0;
          if (pr == (int)art::Prop::Wreck + 1) wreck = true;
        }
      if (g.settlementAt(hero) < 0 && r < (wreck ? 0.6f : 0.12f)) {
        const int n = 2 + rng(g).irange(3);
        Vec2 at;
        if (waterSpot(g, hero, 6.0f * TILE, 10.0f * TILE, 6.0f, 2, at))
          for (int k = 0; k < n; k++) {
            Vec2 m;
            if (!waterSpot(g, at, 0, 2.0f * TILE, 6.0f, 2, m)) m = at;
            const int id = spawnSwimmer(g, art::Monster::Merrow, m, D, SW_MERROW);
            if (const int i = g.findActor(id); i >= 0) g.actors[(size_t)i].aggro = true;
          }
      }
      return;
    }
    // the deep: a serpent (rarely), pirates (now and then, once the hero is worth robbing)
    bool open = true;
    for (int k = 0; k < 8 && open; k++) {
      const int qx = tx + (int)std::lround(std::cos(k * 0.785398) * 16), qy = ty + (int)std::lround(std::sin(k * 0.785398) * 16);
      open = g.world.over.in(qx, qy) && groundWater(g.world.over.at(qx, qy));
    }
    if (open && r < 0.10f) {
      Vec2 at;
      if (waterSpot(g, hero, 9.0f * TILE, 12.0f * TILE, 12.0f, 1, at)) spawnSwimmer(g, art::Monster::SeaSerpent, at, D + 1, SW_SERPENT);
    } else if (r < 0.18f && g.plLevel >= 3) {
      const float an = L.heading + (rng(g).f() < 0.5f ? 1.4f : -1.4f);
      spawnPirates(g, hero + Vec2(std::cos(an), std::sin(an)) * (22.0f * TILE), D);
    }
  }
}
