// M8b Treasure (VISION_PLAN 15.27, 15.28): what affixes and unique / Mythic powers DO in play. POWERS lane.
// This file: which affixes and powers work (only those roll: gear::affixImplemented / uniqueImplemented), and PowerOps
// (rpg/sim/power_ops.h), the worn powers' runtime: the companions' gear cache, the on-hit procs (Tempest's chain, a
// falling star), the kill procs (Soulfeast, Wolfking's call), the Phoenix's rebirth, Chronos' time slow, Shadowstep,
// the fire patches (Inferno, Fire Trail), the Bramble Crown's aura, Health Regen, and the spectral wolves' behaviour.
// The stat affixes and the M6 powers live where the numbers are (game.cpp damage / meleeHit / updatePlayer, player.cpp
// armorRating, party_game.cpp body). Every proc emits Ev::Power for the view (rpg/view/render_powers.cpp). A power's own
// damage runs with Game::powDepth_ > 0, so no proc ever fires from another proc's damage (no chain reactions).
#include "rpg/sim/powers.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>
#include "engine/audio.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/party.h"
#include "rpg/sim/power_ops.h"

namespace powers {

bool affixWorks(Affix a) {
  // (M8b phase B) every affix changes a number in play: the M6 eighteen, and VsBeasts / VsUndead / VsPeople, Burn
  // (game.cpp damage), AttackSpeed (updatePlayer, shootArrow, a companion's updateActor), ArmourPct (armorRating,
  // PartyOps::body), HealthRegen (PowerOps::step) and Stamina (updatePlayer). --powers measures each one.
  return a != Affix::None && (int)a < (int)Affix::COUNT;
}

bool uniqueWorks(Unique u) {
  // every power has its code now: the M6 ones, the five M6 TODOs (Fire Trail, Spectral Wolf, Split Arrows, Reflect
  // Block, Shadowstep) and the eight Mythic powers. --powers fails any that does nothing.
  return u != Unique::None && (int)u < (int)Unique::COUNT;
}

}  // namespace powers

// gear.h's gates are defined here, by the lane that makes the powers work
bool gear::uniqueImplemented(Unique u) { return powers::uniqueWorks(u); }
bool gear::affixImplemented(Affix a) { return a != Affix::None && (int)a < (int)Affix::COUNT && powers::affixWorks(a); }

// ===================================================================================================== PowerOps
using powers::Fx;

namespace {
bool foeAlive(const Actor& e) { return e.hostile && e.st != AState::Dead && !e.fly && !e.indoors && e.st != AState::Down; }
float groundD2(Vec2 a, Vec2 b) {   // distance on the ground in the 3/4 view (a little flatter north-south)
  const float dx = a.x - b.x, dy = (a.y - b.y) * 1.25f;
  return dx * dx + dy * dy;
}
}  // namespace

const gear::GearStats* PowerOps::statsOf(const Game& g, const Actor& a) {
  if (a.player) return &g.craft.live.stats;
  if (!a.comp) return nullptr;
  for (const auto& kv : g.compStats_) if (kv.first == a.id) return &kv.second;
  return nullptr;
}
const gear::GearStats* PowerOps::statsOfId(const Game& g, int id) {
  const int i = g.findActor(id);
  return i >= 0 ? statsOf(g, g.actors[(size_t)i]) : nullptr;
}

gear::GearStats PowerOps::memberStats(const Game& g, const party::Member& m) {
  std::vector<const Item*> w;
  for (int s = 0; s < party::SLOTS; s++) {
    const int e = m.eq[(size_t)s];
    if (e >= 0 && e < (int)m.inv.size()) w.push_back(&m.inv[(size_t)e]);
  }
  return gear::sumGear(w, party::levelFor(g.plLevel));
}

float PowerOps::baseDamage(const Game& g, const Actor& w) { return w.player ? g.weaponDamage() : std::max(1.0f, w.dmg); }

void PowerOps::emitPower(Game& g, Fx fx, Vec2 p, Vec2 q, float f, uint32_t id) {
  g.emit(Ev::Power, p, (int)fx, f);
  g.events.back().q = q;
  g.events.back().id = id;
  if ((int)fx < 16) g.craft.live.fx.procs[(int)fx]++;
}

void PowerOps::companionBody(Game& g, int mi) {
  if (mi < 0 || mi >= (int)g.party.members.size()) return;
  const int ai = PartyOps::actorOf(g, mi);
  if (ai > 0) {
    // the cache first (body reads it through memberStats, the fight through statsOf)
    const gear::GearStats st = memberStats(g, g.party.members[(size_t)mi]);
    bool found = false;
    for (auto& kv : g.compStats_) if (kv.first == g.actors[(size_t)ai].id) { kv.second = st; found = true; }
    if (!found) g.compStats_.push_back({g.actors[(size_t)ai].id, st});
    PartyOps::body(g, g.party.members[(size_t)mi], g.actors[(size_t)ai]);
  }
}

void PowerOps::clearMap(Game& g) {
  powers::State& F = g.craft.live.fx;
  F.patches.clear();
  F.stars.clear();
  F.calls.clear();
  F.lastPatch = Vec2();
  F.summonId = 0;
  F.summonT = 0;
}

void PowerOps::shift(Game& g, Vec2 d) {
  powers::State& F = g.craft.live.fx;
  for (auto& p : F.patches) p.p += d;
  for (auto& s : F.stars) s.at += d;
  for (auto& c : F.calls) c.at += d;
  if (len2(F.lastPatch) > 0) F.lastPatch += d;
}

// ---------------------------------------------------------------- on hit: Tempest's chain, a falling star
// Tempest: the blow's lightning leaps from the struck foe to the next nearest, TEMPEST_ARCS times
void PowerOps::tempestChain(Game& g, int wid, Vec2 wp, int firstId, Vec2 from, float dmg) {
  std::vector<int> struck{firstId};
  int arcs = 0;
  for (int n = 0; n < powers::TEMPEST_ARCS; n++) {
    int best = -1;
    float bd = powers::TEMPEST_REACH * powers::TEMPEST_REACH;
    for (size_t i = 1; i < g.actors.size(); i++) {
      const Actor& e = g.actors[i];
      if (!foeAlive(e) || std::find(struck.begin(), struck.end(), e.id) != struck.end()) continue;
      const float d2 = len2(e.p - from);
      if (d2 < bd) { bd = d2; best = (int)i; }
    }
    if (best < 0) break;
    Actor& t = g.actors[(size_t)best];
    struck.push_back(t.id);
    PowerOps::emitPower(g, Fx::Lightning, from + Vec2(0, -8), t.p + Vec2(0, -8), 1.0f, 0);
    const Vec2 next = t.p;
    PowerOps::hurt(g, t, dmg, from, wid);
    g.craft.live.fx.chained++;
    arcs++;
    from = next;
  }
  if (arcs > 0) g.sfx((int)Sfx::Zap, wp, 1.0f + 0.06f * (float)arcs, 0.8f);
}

void PowerOps::afterHit(Game& g, int attacker, int victimId, Vec2 at, float dealt, bool crit) {
  (void)dealt;
  const int ai = g.findActor(attacker);
  if (ai < 0) return;
  Actor& w = g.actors[(size_t)ai];
  const gear::GearStats* S = statsOf(g, w);
  if (!S) return;
  powers::State& F = g.craft.live.fx;
  if (w.player) F.unseenT = 0;   // (Shadowstep: striking gives the hero away)
  const float base = baseDamage(g, w);
  g.powDepth_++;
  if (S->has(Unique::Tempest)) {
    float& cd = w.player ? F.tempestCd : w.powCd;
    if (cd <= 0) {   // (one chain a swing: the cone's other foes and an echo do not chain again)
      cd = 0.12f;
      tempestChain(g, w.id, w.p, victimId, at, base * powers::TEMPEST_FRAC);
    }
  }
  if (crit && S->has(Unique::Starfall) && F.starCd <= 0 && F.stars.size() < 4) {
    F.starCd = powers::STAR_GAP;
    powers::State::Star st;
    st.at = at; st.dmg = base * powers::STAR_FRAC; st.by = attacker;
    F.stars.push_back(st);
    emitPower(g, Fx::Star, at + Vec2(-26, -110), at, 1.0f, 0);
    g.sfx((int)Sfx::Zap, at, 0.55f, 0.5f);
  }
  g.powDepth_--;
}

// ---------------------------------------------------------------- on a kill: Soulfeast, Vampire (companions), wolves
void PowerOps::onKill(Game& g, const Actor& victim, int killer) {
  if (victim.player || victim.comp || victim.powTag == powers::PT_WOLF || !victim.hostile) return;
  const int ai = killer >= 0 ? g.findActor(killer) : -1;
  if (ai < 0) return;
  Actor& w = g.actors[(size_t)ai];
  const gear::GearStats* S = statsOf(g, w);
  if (!S || w.st == AState::Dead) return;
  powers::State& F = g.craft.live.fx;
  if (S->has(Unique::Soulfeast)) {
    const float before = w.hp;
    w.hp = std::min(w.maxHp, w.hp + w.maxHp * 0.01f * powers::SOULFEAST_KILL_PCT);
    F.healed += w.hp - before;
    emitPower(g, Fx::Lifesteal, victim.p + Vec2(0, -8), w.p + Vec2(0, -8), 1.5f, (uint32_t)w.id);
  }
  if (!w.player && S->has(Unique::Vampire)) { w.hp = std::min(w.maxHp, w.hp + w.maxHp * 0.08f); g.emit(Ev::Heal, w.p); }
  if ((S->has(Unique::Wolfking) || S->has(Unique::SpectralWolf)) && F.calls.size() < 4) {
    powers::State::Call c;
    c.at = victim.p; c.by = w.id;
    c.life = S->has(Unique::Wolfking) ? powers::WOLF_SECS : powers::SPECTRAL_SECS;
    F.calls.push_back(c);
  }
}

// ---------------------------------------------------------------- the Phoenix
bool PowerOps::phoenix(Game& g) {
  powers::State& F = g.craft.live.fx;
  if (!g.craft.live.stats.has(Unique::Phoenix) || F.phoenixCd > 0) return false;
  Actor& p = g.pl();
  F.phoenixCd = powers::PHOENIX_CD;
  F.rebirths++;
  p.hp = std::max(1.0f, p.maxHp * powers::PHOENIX_HP);
  p.st = AState::Idle; p.stT = 0;
  p.iframes = 1.2f;
  p.burnT = 0; p.burnDpsT = 0;
  // the burst: the flames throw back and burn whatever stood over the hero
  const float dmg = baseDamage(g, p);
  const Vec2 pp = p.p;
  const int pid = p.id;
  g.powDepth_++;
  for (size_t i = 1; i < g.actors.size(); i++) {
    Actor& e = g.actors[i];
    if (!foeAlive(e) || groundD2(e.p, pp) > powers::PHOENIX_REACH * powers::PHOENIX_REACH) continue;
    const Vec2 dir = norm(e.p - pp + Vec2(0.01f, 0));
    e.burnT = std::max(e.burnT, 3.0f);
    e.regen = 0;
    g.damage(e, dmg, pp, pid, Ench::None, 0, false);
    if (e.st != AState::Dead && !e.boss) e.knock = dir * 240.0f;
  }
  g.powDepth_--;
  const Vec2 at = g.pl().p;
  emitPower(g, Fx::Phoenix, at, at, 1.0f, (uint32_t)g.pl().id);
  g.emit(Ev::Text, at + Vec2(0, -34), (int)rgba(255, 170, 60), 0, "REBORN");
  g.emit(Ev::Shake, at, 0, 4.0f);
  g.sfx((int)Sfx::Rebirth, at, 1.0f, 1.0f);
  g.slowMo = std::max(g.slowMo, 0.6f);
  if (g.mode == Mode::Dead) g.mode = Mode::Play;
  return true;
}

// ---------------------------------------------------------------- rolls: Chronos' window, Shadowstep
void PowerOps::rollStarted(Game& g) {
  powers::State& F = g.craft.live.fx;
  const gear::GearStats& S = g.craft.live.stats;
  F.dodgeT = powers::CHRONOS_WINDOW;
  if (S.has(Unique::Shadowstep) && F.shadowCd <= 0) {
    F.unseenT = powers::SHADOW_SECS;
    F.shadowCd = powers::SHADOW_CD;
    const int pid = g.pl().id;
    for (size_t i = 1; i < g.actors.size(); i++) {
      Actor& e = g.actors[i];
      if (e.hostile && e.target == pid && e.mon != art::Monster::Dragon) { e.aggro = false; e.target = -1; }
    }
    emitPower(g, Fx::Proc, g.pl().p, g.pl().p, 0.8f, (uint32_t)pid);
    g.emit(Ev::Text, g.pl().p + Vec2(0, -26), (int)rgba(170, 160, 220), 0, "UNSEEN");
  }
}

void PowerOps::dodged(Game& g) {
  powers::State& F = g.craft.live.fx;
  if (!g.craft.live.stats.has(Unique::Chronos) || F.chronosCd > 0 || F.dodgeT <= 0) return;
  F.slowT = powers::CHRONOS_SECS;
  F.chronosCd = powers::CHRONOS_CD;
  emitPower(g, Fx::TimeSlow, g.pl().p, g.pl().p, 1.0f, (uint32_t)g.pl().id);
  g.emit(Ev::Text, g.pl().p + Vec2(0, -30), (int)rgba(150, 210, 255), 0, "TIME SLOWS");
  g.sfx((int)Sfx::Frost, g.pl().p, 0.5f, 0.8f);
}

// ---------------------------------------------------------------- a spectral wolf
bool PowerOps::wolfStep(Game& g, Actor& a, float dt) {
  const int mi = g.findActor((int)a.powBy);
  if (mi < 0 || g.actors[(size_t)mi].st == AState::Dead) { a.powT = 0; return true; }
  const Actor& master = g.actors[(size_t)mi];
  if (a.st == AState::Hurt) { if (a.stT > 0.2f) { a.st = AState::Idle; a.stT = 0; } return true; }
  if (a.st == AState::Windup) {
    if (a.stT >= a.windup) { a.st = AState::Strike; a.stT = 0; g.meleeHit(a); g.sfx((int)Sfx::Hit, a.p, 1.3f, 0.6f); }
    return true;
  }
  if (a.st == AState::Strike) {
    if (a.stT > 0.16f) { a.st = AState::Idle; a.stT = 0; a.atkCd = 0.65f; }
    return true;
  }
  // the nearest foe near its master (it never runs off across the map)
  int best = -1;
  float bd = 1e30f;
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& e = g.actors[i];
    if (!foeAlive(e)) continue;
    const float dm = len2(e.p - master.p), d2 = len2(e.p - a.p);
    if (dm > 200.0f * 200.0f || d2 > 160.0f * 160.0f) continue;
    if (d2 < bd) { bd = d2; best = (int)i; }
  }
  const float slow = a.slowT > 0 ? 0.6f : 1.0f;
  if (best >= 0) {
    const Actor& t = g.actors[(size_t)best];
    const Vec2 d = t.p - a.p;
    const float l = len(d);
    a.aim = norm(d + Vec2(0.001f, 0));
    a.face = faceOf(a.aim);
    if (l > a.range + t.radius - 3.0f) {
      g.moveActor(a, a.aim * (a.speed * slow * dt));
      if (a.st != AState::Walk) { a.st = AState::Walk; a.stT = 0; }
    } else if (a.atkCd <= 0) {
      a.st = AState::Windup; a.stT = 0;
    } else if (a.st != AState::Idle) { a.st = AState::Idle; a.stT = 0; }
    return true;
  }
  // nothing to fight: at its master's heel
  const Vec2 heel = master.p + Vec2(master.face == 3 ? 14.0f : -14.0f, 6.0f);
  const Vec2 d = heel - a.p;
  const float l = len(d);
  if (l > 260.0f) { a.p = heel; return true; }   // (left behind: it runs out of the shadows at the master's side)
  if (l > 10.0f) {
    a.aim = norm(d);
    a.face = faceOf(a.aim);
    g.moveActor(a, a.aim * (a.speed * (l > 70.0f ? 1.4f : 1.0f) * slow * dt));
    if (a.st != AState::Walk) { a.st = AState::Walk; a.stT = 0; }
  } else if (a.st != AState::Idle) { a.st = AState::Idle; a.stT = 0; a.face = master.face; }
  return true;
}

void PowerOps::callWolf(Game& g, const powers::State::Call& c) {
  powers::State& F = g.craft.live.fx;
  const int mi = g.findActor(c.by);
  if (mi < 0) return;
  // at the cap, the oldest wolf is renewed instead
  int n = 0, oldest = -1;
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& w = g.actors[i];
    if (w.powTag != powers::PT_WOLF || (int)w.powBy != c.by || w.st == AState::Dead) continue;
    n++;
    if (oldest < 0 || w.powT < g.actors[(size_t)oldest].powT) oldest = (int)i;
  }
  if (n >= powers::WOLF_CAP && oldest >= 0) {
    g.actors[(size_t)oldest].powT = c.life;
    PowerOps::emitPower(g, Fx::Wolf, g.actors[(size_t)oldest].p, g.actors[(size_t)oldest].p, 0.6f, (uint32_t)g.actors[(size_t)oldest].id);
    return;
  }
  const Actor master = g.actors[(size_t)mi];   // (a copy: the push below may move the actors)
  Actor w;
  w.id = g.nextId_++;
  w.npc = false; w.hostile = false; w.human = false; w.faction = Faction::Player;
  w.mon = art::Monster::Wolf;
  w.name = "SPECTRAL WOLF";
  w.powTag = powers::PT_WOLF; w.powT = c.life; w.powBy = (uint32_t)c.by;
  w.level = master.player ? std::max(1, g.plLevel) : std::max(1, master.level);
  w.maxHp = w.hp = std::max(20.0f, master.maxHp * 0.35f);
  w.dmg = PowerOps::baseDamage(g, master) * 0.45f;
  w.armor = master.player ? g.armorRating() : master.armor;
  w.speed = 96.0f; w.radius = 4.0f; w.range = 15.0f; w.windup = 0.16f; w.aggroR = 150.0f;
  Vec2 at = c.at;
  if (!g.bodyFree(at, w.radius, false)) at = master.p;
  w.p = at; w.home = at; w.goal = at;
  w.face = master.face;
  w.bodyTint = rgba(118, 196, 255);   // the spectral blue (the view adds the glow and the translucent shimmer)
  w.iframes = 0.3f;
  g.actors.push_back(w);
  F.wolvesCalled++;
  F.summonId = (uint32_t)w.id;
  F.summonT = c.life;
  PowerOps::emitPower(g, Fx::Wolf, at, at, 1.0f, (uint32_t)w.id);
  g.sfx((int)Sfx::Howl, at, 1.35f, 0.55f);
}

void PowerOps::aura(Game& g, const Actor& wr, float dmg) {
  powers::State& F = g.craft.live.fx;
  const int wid = wr.id;
  const Vec2 wp = wr.p;
  int hit = 0;
  g.powDepth_++;
  for (size_t i = 1; i < g.actors.size(); i++) {
    Actor& e = g.actors[i];
    if (!foeAlive(e) || groundD2(e.p, wp) > (powers::BRAMBLE_REACH + e.radius) * (powers::BRAMBLE_REACH + e.radius)) continue;
    const float before = e.hp;
    PowerOps::hurt(g, e, dmg, wp, wid);
    F.auraDealt += std::max(0.0f, before - std::max(0.0f, e.hp));
    hit++;
  }
  g.powDepth_--;
  PowerOps::emitPower(g, Fx::Thorns, wp, wp, hit > 0 ? 1.0f : 0.4f, (uint32_t)wid);
}

// ---------------------------------------------------------------- every step
void PowerOps::step(Game& g, float dt) {
  powers::State& F = g.craft.live.fx;
  const gear::GearStats& S = g.craft.live.stats;
  Actor& p = g.pl();
  // the companions' worn gear (their fights read it through statsOf)
  g.compStats_.clear();
  for (size_t i = 1; i < g.actors.size(); i++)
    if (g.actors[i].comp) if (const party::Member* m = PartyOps::of(g, g.actors[i])) g.compStats_.push_back({g.actors[i].id, memberStats(g, *m)});
  // clocks
  auto down = [dt](float& t) { if (t > 0) t = std::max(0.0f, t - dt); };
  down(F.starCd); down(F.tempestCd); down(F.chronosCd); down(F.phoenixCd); down(F.shadowCd); down(F.unseenT); down(F.dodgeT);
  if (F.slowT > 0) { F.slowedSecs += std::min(dt, F.slowT); down(F.slowT); }
  const bool alive = p.st != AState::Dead && g.mode != Mode::Dead;
  // ---- Health Regen: N health every 5 s (the hero)
  if (const int n = S.get(Affix::HealthRegen); n > 0 && alive) {
    F.regenT += dt;
    while (F.regenT >= powers::REGEN_EVERY) {
      F.regenT -= powers::REGEN_EVERY;
      const float before = p.hp;
      p.hp = std::min(p.maxHp, p.hp + (float)n);
      if (p.hp > before) {
        F.healed += p.hp - before;
        g.emit(Ev::Text, p.p + Vec2(0, -24), (int)rgba(140, 240, 140), p.hp - before, "+" + std::to_string((int)std::lround(p.hp - before)));
      }
    }
  } else F.regenT = 0;
  // ---- Inferno (walking) and Fire Trail (rolling): fire patches on the ground; foes standing in them burn
  const bool inferno = S.has(Unique::Inferno), trail = S.has(Unique::FireTrail) && p.st == AState::Roll;
  if (alive && (inferno || trail) && !p.fly) {
    if (len2(F.lastPatch) <= 0) F.lastPatch = p.p;
    else if (len2(p.p - F.lastPatch) > powers::PATCH_STEP * powers::PATCH_STEP) {
      powers::State::Patch pa;
      pa.p = p.p + Vec2(0, 1);
      pa.life = powers::PATCH_LIFE;
      pa.seed = hash32((uint32_t)g.time * 977u ^ (uint32_t)F.patches.size() * 2654435761u ^ (uint32_t)(p.p.x * 13.0f));
      if ((int)F.patches.size() >= powers::PATCH_CAP) F.patches.erase(F.patches.begin());
      F.patches.push_back(pa);
      F.lastPatch = p.p;
      F.procs[(int)Fx::FirePatch]++;
    }
  } else F.lastPatch = Vec2();
  for (auto& pa : F.patches) pa.t += dt;
  F.patches.erase(std::remove_if(F.patches.begin(), F.patches.end(), [](const powers::State::Patch& q) { return q.t >= q.life; }), F.patches.end());
  if (!F.patches.empty())
    for (size_t i = 1; i < g.actors.size(); i++) {
      Actor& e = g.actors[i];
      if (!foeAlive(e)) continue;
      for (const auto& pa : F.patches)
        if (groundD2(e.p, pa.p) < (powers::PATCH_R + e.radius) * (powers::PATCH_R + e.radius) && pa.t < pa.life - 0.25f) {
          e.burnT = std::max(e.burnT, 0.6f);
          e.regen = 0;
          break;
        }
    }
  // ---- falling stars land
  for (size_t k = 0; k < F.stars.size();) {
    F.stars[k].t += dt;
    if (F.stars[k].t < powers::STAR_DELAY) { k++; continue; }
    const powers::State::Star st = F.stars[k];
    F.stars.erase(F.stars.begin() + (std::ptrdiff_t)k);
    g.powDepth_++;
    for (size_t i = 1; i < g.actors.size(); i++) {
      Actor& e = g.actors[i];
      if (!foeAlive(e) || groundD2(e.p, st.at) > (powers::STAR_REACH + e.radius) * (powers::STAR_REACH + e.radius)) continue;
      hurt(g, e, st.dmg, st.at + Vec2(0, -6), st.by);
      F.starHits++;
    }
    g.powDepth_--;
    g.emit(Ev::Shake, st.at, 0, 3.0f);
    g.sfx((int)Sfx::Explode, st.at, 1.35f, 0.6f);
  }
  // ---- the Bramble Crown: a thorn aura, a sting every second (the hero and companions who wear it)
  if (S.has(Unique::BrambleCrown) && alive) {
    F.auraT += dt;
    if (F.auraT >= 1.0f) { F.auraT -= 1.0f; aura(g, p, baseDamage(g, p) * powers::BRAMBLE_FRAC); }
  } else F.auraT = 0;
  for (const auto& kv : std::vector<std::pair<int, gear::GearStats>>(g.compStats_)) {
    const int ai = g.findActor(kv.first);
    if (ai <= 0) continue;
    Actor& c = g.actors[(size_t)ai];
    if (c.st == AState::Dead || c.posture == art::Posture::Downed) continue;
    if (kv.second.has(Unique::BrambleCrown)) {
      c.powAuraT += dt;
      if (c.powAuraT >= 1.0f) { c.powAuraT -= 1.0f; aura(g, g.actors[(size_t)ai], baseDamage(g, c) * powers::BRAMBLE_FRAC); }
    }
    if (const int n = kv.second.get(Affix::HealthRegen); n > 0) {
      Actor& c2 = g.actors[(size_t)ai];
      c2.powRegenT += dt;
      while (c2.powRegenT >= powers::REGEN_EVERY) {
        c2.powRegenT -= powers::REGEN_EVERY;
        const float before = c2.hp;
        c2.hp = std::min(c2.maxHp, c2.hp + (float)n);
        F.healed += c2.hp - before;
      }
    }
  }
  // ---- wolves: the called ones come, the spent ones fade
  if (!F.calls.empty()) {
    const std::vector<powers::State::Call> calls = F.calls;
    F.calls.clear();
    for (const auto& c : calls) callWolf(g, c);
  }
  for (size_t i = g.actors.size(); i-- > 1;) {
    Actor& w = g.actors[i];
    if (w.powTag != powers::PT_WOLF) continue;
    if (w.st != AState::Dead) w.powT -= dt;
    if (w.powT <= 0 || w.st == AState::Dead || !alive) {
      if (w.st != AState::Dead) emitPower(g, Fx::Wolf, w.p, w.p, 0.0f, (uint32_t)w.id);
      if ((uint32_t)w.id == F.summonId) { F.summonId = 0; F.summonT = 0; }
      g.actors.erase(g.actors.begin() + (std::ptrdiff_t)i);
    }
  }
  if (F.summonId) F.summonT = std::max(0.0f, F.summonT - dt);
}

// ---------------------------------------------------------------- scripts: a power fired on demand
bool PowerOps::proc(Game& g, Unique u) {
  powers::State& F = g.craft.live.fx;
  Actor& p = g.pl();
  int near = -1;
  float bd = 1e30f;
  for (size_t i = 1; i < g.actors.size(); i++)
    if (foeAlive(g.actors[i]) && len2(g.actors[i].p - p.p) < bd) { bd = len2(g.actors[i].p - p.p); near = (int)i; }
  const Vec2 at = near > 0 ? g.actors[(size_t)near].p : p.p + Vec2(30, 0);
  switch (u) {
    case Unique::Tempest: case Unique::ChainLightning: case Unique::Stormcaller:
      if (near < 0) return false;
      g.powDepth_++;
      tempestChain(g, p.id, p.p, -1, p.p, baseDamage(g, p) * powers::TEMPEST_FRAC);
      g.powDepth_--;
      return true;
    case Unique::Inferno: case Unique::FireTrail:
      for (int k = 0; k < 7; k++) {
        powers::State::Patch pa;
        pa.p = p.p + Vec2(-10.0f * (float)k, 2.0f * std::sin((float)k));
        pa.t = 0.25f * (float)k; pa.life = powers::PATCH_LIFE; pa.seed = hash32(0xF1u + (uint32_t)k * 7919u);
        F.patches.push_back(pa);
      }
      return true;
    case Unique::Soulfeast: case Unique::Vampire:
      for (int k = 0; k < 3; k++) emitPower(g, Fx::Lifesteal, at + Vec2(0, -8), p.p + Vec2(0, -8), 1.5f, (uint32_t)p.id);
      return true;
    case Unique::BrambleCrown: case Unique::Thornmail:
      aura(g, p, baseDamage(g, p) * powers::BRAMBLE_FRAC);
      return true;
    case Unique::Chronos: case Unique::TimeSlow:
      F.slowT = powers::CHRONOS_SECS;
      emitPower(g, Fx::TimeSlow, p.p, p.p, 1.0f, (uint32_t)p.id);
      return true;
    case Unique::Phoenix:
      F.phoenixCd = 0;
      if (!g.craft.live.stats.has(Unique::Phoenix)) {
        emitPower(g, Fx::Phoenix, p.p, p.p, 1.0f, (uint32_t)p.id);
        g.sfx((int)Sfx::Rebirth, p.p);
        return true;
      }
      return phoenix(g);
    case Unique::Starfall: {
      powers::State::Star st;
      st.at = at; st.dmg = baseDamage(g, p) * powers::STAR_FRAC; st.by = p.id;
      F.stars.push_back(st);
      emitPower(g, Fx::Star, at + Vec2(-26, -110), at, 1.0f, 0);
      return true;
    }
    case Unique::Wolfking: case Unique::SpectralWolf: {
      powers::State::Call c;
      c.at = at + Vec2(-12, 4); c.by = p.id; c.life = powers::WOLF_SECS;
      F.calls.push_back(c);
      return true;
    }
    default: return false;
  }
}
