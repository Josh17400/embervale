// M10 "Beyond the Mist": the sea's moods and the hull's fate (rpg/sim/sea_ops.h). SEA lane.
//   - the storm sea (ew::EndlessSource::seaAt Storm), cached per tile: a galleon's, or a sloop's with a storm keel;
//   - storms: weather fronts that come in from upwind over the sea, drift with the wind, push the boat, roughen the
//     water (small hulls take a beating), darken the sky (Live::stormDark) and throw lightning (SeaEv::Lightning);
//   - the hull's damage and sinking: SeaEv::Sink, the boat settles, the screen fades, the hero washes ashore on the
//     nearest coast (a journey behind the fade, never a snap) and the boat is marked sunk (a shipwright raises another);
//   - swimming (15.1): the shallows only, SWIM_SECS (the Sailor SAILOR_SWIM_SECS), stamina drains, then a tired
//     swimmer is pulled back to the last dry footing, eased strokes all the way.
#include "rpg/sim/sea_ops.h"

#include "engine/audio.h"

#include <algorithm>
#include <array>
#include <cmath>
#include "rpg/sim/game_internal.h"
#include "rpg/world/source.h"

using namespace sea;
using gsim::faceOf;

namespace {
// the storm sea per global tile (seaAt reads the macro field: dear enough to keep a small direct-mapped cache)
struct StormCache {
  std::array<uint64_t, 4096> key{};
  std::array<uint8_t, 4096> val{};
  uint64_t seed = 0;
};
StormCache& stormCache() { static StormCache c; return c; }
}  // namespace

bool SeaOps::stormSea(const Game& g, int tx, int ty) {
  if (!g.world.src || !g.world.endless) return false;
  const int32_t gx = g.world.ox + tx, gy = g.world.oy + ty;
  StormCache& c = stormCache();
  if (c.seed != g.seed + 1) { c.key.fill(0); c.seed = g.seed + 1; }
  const uint64_t k = ((uint64_t)(uint32_t)gx << 32 | (uint32_t)gy) ^ 0x9E3779B97F4A7C15ull;
  const size_t i = (size_t)(ew::mix64(k) & 4095u);
  if (c.key[i] != k) { c.key[i] = k; c.val[i] = g.world.src->seaAt(gx, gy) == ew::SeaKind::Storm ? 1 : 0; }
  return c.val[i] != 0;
}

void SeaOps::beginStorm(Game& g, Vec2 c, float power, float life) {
  Sea::Storm& S = g.sea.live.storm;
  S.on = true;
  S.c = c;
  S.radius = (34.0f + 26.0f * rng(g).f()) * TILE;
  S.t = 0;
  S.life = life;
  S.power = std::clamp(power, 0.2f, 1.0f);
  g.sea.live.lightningT = 6.0f + 4.0f * rng(g).f();
  ev(g, SeaEv::StormBegin, c, S.power, 0, "THE SKY DARKENS. A STORM IS RISING");
  if (g.sea.atSea()) tell(g, "THE SKY DARKENS. A STORM IS RISING");
}

void SeaOps::stormStep(Game& g, float dt) {
  Sea::Live& L = g.sea.live;
  Sea::Storm& S = L.storm;
  const bool atSea = g.sea.atSea() && L.sinkT <= 0;
  const Vec2 windV(std::cos(L.wind.dir), std::sin(L.wind.dir));
  const Vec2 hero = g.pl().p;
  float roughWant = 0, darkWant = 0, driftWant = 0;
  // a front now and then while at sea: more often over the storm sea and in the autumn and winter
  if (!S.on && atSea && (L.stormRollT -= dt) <= 0) {
    L.stormRollT = 25.0f + 30.0f * rng(g).f();
    const int season = ((g.day - 1) / 30) & 3;
    const int tx = (int)std::floor(hero.x / TILE), ty = (int)std::floor(hero.y / TILE);
    float p = 0.06f + (season >= 2 ? 0.04f : 0.0f);
    if (stormSea(g, tx, ty)) p += 0.22f;
    if (rng(g).f() < p) beginStorm(g, hero - windV * (40.0f * TILE), 0.5f + 0.5f * rng(g).f(), 100.0f + 110.0f * rng(g).f());
  }
  if (S.on) {
    S.t += dt;
    S.c += windV * (2.0f * TILE * dt);   // it rides the wind
    const float env = std::min(1.0f, S.t / 14.0f) * std::clamp((S.life - S.t) / 20.0f, 0.0f, 1.0f);
    const float dist = len(hero - S.c);
    const float prox = std::clamp(1.3f * (1.0f - dist / std::max(1.0f, S.radius)), 0.0f, 1.0f);
    const float k = env * S.power * smooth01(prox);
    if (S.t >= S.life || (!atSea && dist > S.radius * 1.6f)) {
      S.on = false;
      ev(g, SeaEv::StormEnd, S.c, 0);
    } else {
      darkWant = k * 0.85f;
      if (atSea) {
        roughWant = k;
        driftWant = k * 2.2f;
        const Boat& b = g.sea.boats[(size_t)g.sea.aboard];
        // rough water batters a small hull (a galleon rides it out; a keel steadies a sloop)
        if (!b.ferry && k > 0.15f) {
          const float rate = b.hull == Hull::Rowboat ? 2.4f : (b.hull == Hull::Sloop && !(b.upgrades & UP_KEEL)) ? 0.9f : 0.0f;
          if (rate > 0 && rng(g).f() < dt * 0.7f) hurtBoat(g, rate * k * 1.4f / 0.7f, hero + Vec2(rng(g).range(-8, 8), rng(g).range(-6, 6)));
        }
      }
      // lightning: under the clouds, at varied intervals, more the heavier it is
      if (k > 0.25f && (L.lightningT -= dt) <= 0) {
        const float a = rng(g).f() * 6.2831853f, r = (0.15f + 0.6f * rng(g).f()) * S.radius;
        const Vec2 at = hero + Vec2(std::cos(a) * std::min(r, 12.0f * TILE), std::sin(a) * std::min(r, 8.0f * TILE));
        ev(g, SeaEv::Lightning, at, k, 0, "", at + Vec2(rng(g).range(-30, 30), -220.0f));
        g.emit(Ev::Shake, at, 0, 0.35f + 0.4f * k);
        snd(g, (int)Sfx::Explode, at, 0.45f + 0.15f * rng(g).f(), 0.5f + 0.4f * k);
        L.lightningT = (2.5f + 5.5f * rng(g).f()) / std::max(0.4f, k);
      }
    }
  }
  if (atSea) {
    const int tx = (int)std::floor(hero.x / TILE), ty = (int)std::floor(hero.y / TILE);
    if (stormSea(g, tx, ty)) { roughWant = std::max(roughWant, 0.55f); darkWant = std::max(darkWant, 0.22f); }
  }
  const float e = std::min(1.0f, dt * 0.8f);
  L.rough += (roughWant - L.rough) * e;
  L.stormDark += (darkWant - L.stormDark) * e;
  L.drift += (driftWant - L.drift) * e;
}

void SeaOps::hurtBoat(Game& g, float dmg, Vec2 at) {
  if (!g.sea.atSea() || dmg <= 0) return;
  Sea::Live& L = g.sea.live;
  Boat& b = g.sea.boats[(size_t)g.sea.aboard];
  if (b.ferry || L.sinkT > 0) return;   // (a ferry's crew bails and patches: the passenger never goes down with her)
  b.hp -= dmg;
  ev(g, SeaEv::HullHit, at, dmg, b.id);
  g.pl().flash = 0.12f;
  if (b.hp > 0) {
    if (b.hp < maxHp(b) * 0.25f && b.hp + dmg >= maxHp(b) * 0.25f) tell(g, "THE HULL IS TAKING WATER! MAKE FOR THE SHORE");
    return;
  }
  b.hp = 0;
  L.sinkT = 0.0001f;
  L.sinkBoat = g.sea.aboard;
  L.mooring = false;
  L.line = Sea::Line();
  ev(g, SeaEv::Sink, g.pl().p, 1, b.id, b.name + " IS GOING DOWN");
  snd(g, (int)Sfx::Splash, g.pl().p, 0.6f, 1.0f);
  tell(g, "THE HULL IS BREACHED! " + b.name + " IS GOING DOWN");
}

bool SeaOps::coastNear(const Game& g, Vec2 from, int radius, Vec2& out) {
  const Map& m = g.world.over;
  const int cx = (int)std::floor(from.x / TILE), cy = (int)std::floor(from.y / TILE);
  for (int r = 1; r <= radius; r++) {
    float best = 1e18f;
    bool found = false;
    for (int dy = -r; dy <= r; dy++)
      for (int dx = -r; dx <= r; dx++) {
        if (std::max(std::abs(dx), std::abs(dy)) != r) continue;   // (the ring only)
        const int tx = cx + dx, ty = cy + dy;
        if (!m.in(tx, ty)) continue;
        const Ground gr = m.at(tx, ty);
        if (groundWater(gr) || gr == Ground::Void || m.blocked(tx, ty)) continue;
        const Vec2 c((float)tx * TILE + 8.0f, (float)ty * TILE + 10.0f);
        if (!g.bodyFree(c, 4.5f, false)) continue;
        const float d = len2(c - from);
        if (d < best) { best = d; out = c; found = true; }
      }
    if (found) return true;
  }
  return false;
}

void SeaOps::sinkStep(Game& g, float dt) {
  Sea::Live& L = g.sea.live;
  if (L.sinkT <= 0) return;
  L.sinkT += dt;
  L.speed *= std::pow(0.15f, dt);
  if (L.sinkT < 1.6f || g.travelling()) return;   // (she settles; the view sinks the hull and fades)
  // ashore: the nearest coast in the window, else the hero's last town
  Actor& p = g.pl();
  Vec2 to;
  int32_t gx = 0, gy = 0;
  if (coastNear(g, p.p, 110, to)) { gx = g.world.ox + (int32_t)std::floor(to.x / TILE); gy = g.world.oy + (int32_t)std::floor(to.y / TILE); }
  else if (g.lastTown >= 0 && g.lastTown < (int)g.world.sites.size()) {
    const Site& s = g.world.sites[(size_t)g.lastTown];
    gx = g.world.ox + s.ex; gy = g.world.oy + s.ey;
  } else { const ew::StartPlan& sp = g.world.src->start(); gx = sp.spawn.x; gy = sp.spawn.y; }
  std::string name;
  if (L.sinkBoat >= 0 && L.sinkBoat < (int)g.sea.boats.size()) {
    Boat& b = g.sea.boats[(size_t)L.sinkBoat];
    b.sunk = true;
    b.dock = 0;
    b.gx = heroGx(g); b.gy = heroGy(g);
    name = b.name;
  }
  g.sea.aboard = -1;
  p.swim = 0;
  p.radius = 4.5f;
  L.speed = 0;
  L.sinkT = 0;
  L.sinkBoat = -1;
  L.serial++;
  g.stamina = 0;
  p.hp = std::max(1.0f, std::min(p.hp, p.maxHp * 0.5f));
  g.beginTravelTo(gx, gy);
  ev(g, SeaEv::WashAshore, Vec2((float)(gx - g.world.ox) * TILE + 8.0f, (float)(gy - g.world.oy) * TILE + 8.0f), 1, 0,
     "YOU WASH ASHORE, HALF DROWNED. " + name + " IS LOST");
  {   // (fixer M10 r1) one line that fits the 480-px canvas (the old 75+ characters lost both ends)
    std::string s = "YOU WASH ASHORE. " + (name.empty() ? std::string("THE BOAT") : name) + " IS LOST";
    if (s.size() + 18 <= 60) s += ". SEE A SHIPWRIGHT";
    tell(g, s);
  }
  g.recalcPlayer();
}

bool SeaOps::swimInput(Game& g, const Input& in, float dt) {
  Sea::Live& L = g.sea.live;
  Actor& p = g.pl();
  const Map& m = g.world.over;
  const int tx = (int)std::floor(p.p.x / TILE), ty = (int)std::floor(p.p.y / TILE);
  const bool wet = m.in(tx, ty) && m.at(tx, ty) == Ground::Water && !g.homeRiding() && p.st != AState::Dead;
  if (!wet) {
    if (L.swimming) { L.swimming = false; L.pulled = false; ev(g, SeaEv::Splash, p.p, 0.3f); }
    if (m.in(tx, ty) && !groundWater(m.at(tx, ty)) && m.at(tx, ty) != Ground::Void) L.lastDry = p.p;
    L.swimT = std::max(0.0f, L.swimT - dt * 2.0f);
    return false;
  }
  if (!L.swimming) {
    L.swimming = true;
    L.swimMax = g.background == Background::Sailor ? SAILOR_SWIM_SECS : SWIM_SECS;
    L.swimVel = Vec2();
    L.pulled = false;
    L.stuckT = 0;
    ev(g, SeaEv::Splash, p.p, 0.6f);
    snd(g, (int)Sfx::Splash, p.p, 1.1f, 0.6f);
    if (len2(L.lastDry) < 1.0f) { Vec2 c; if (coastNear(g, p.p, 12, c)) L.lastDry = c; }
  }
  L.swimT += dt;
  g.stamina = std::max(0.0f, g.stamina - dt * (L.pulled ? 1.5f : 4.5f));
  if (!L.pulled && (L.swimT >= L.swimMax || g.stamina <= 0.5f)) {
    L.pulled = true;
    L.stuckT = 0;
    ev(g, SeaEv::SwimTired, p.p, 1);
    tell(g, "TOO TIRED TO SWIM ON. BACK TO THE SHORE");
  }
  Vec2 want;
  if (L.pulled) {
    const Vec2 d = L.lastDry - p.p;
    want = len(d) > 1.0f ? norm(d) * 44.0f : Vec2();
  } else {
    Vec2 mv = in.move;
    const float ml = len(mv);
    if (ml > 1) mv = mv * (1.0f / ml);
    want = ml > 0.15f ? mv * (p.speed * 0.45f) : Vec2();
  }
  L.swimVel += (want - L.swimVel) * std::min(1.0f, dt * 4.0f);   // eased strokes: no snap in or out of the way
  const Vec2 before = p.p;
  g.moveActor(p, L.swimVel * dt);
  if (L.pulled) {
    if (len2(p.p - before) < 0.01f) L.stuckT += dt; else L.stuckT = 0;
    if (L.stuckT > 2.5f) {   // (the way back is fouled: the nearest beach instead)
      Vec2 c;
      if (coastNear(g, p.p, 20, c)) L.lastDry = c;
      L.stuckT = 0;
    }
  }
  if (len(L.swimVel) > 4.0f) { p.face = faceOf(L.swimVel); p.aim = norm(L.swimVel); p.st = AState::Walk; }
  else p.st = AState::Idle;
  if ((L.wakeT -= dt) <= 0 && len(L.swimVel) > 8.0f) { ev(g, SeaEv::Splash, p.p, 0.35f); L.wakeT = 0.42f + 0.3f * rng(g).f(); }
  return true;
}
