// LifeOps (life_ops.h, TOWNSFOLK lane): village animals.
// A handful of dogs, cats, hens, goats, pigs and ducks near the player (critters) and how each moves (critterFolk).
// Moved verbatim from life_game.cpp (SOLID 1.2); the Game::life* hooks that call these stay in life_game.cpp.
#include "rpg/sim/life_ops.h"
#include "rpg/sim/game.h"

// ---------------------------------------------------------------- village animals
void LifeOps::critters(Game& g, int si) {
  RT::SiteRt& S = siteRt(g, si);
  if (S.crittersOut) return;
  life::Census* c = censusOfSite(g, si);
  if (!c) return;
  S.crittersOut = true;
  const Site& st = g.world.sites[(size_t)si];
  const uint32_t h0 = mixu(c->seed, 0xA417A15ull);
  const bool farm = (ew::Specialty)st.special == ew::Specialty::Farming || st.type == SiteType::Village;
  const bool herd = (ew::Specialty)st.special == ew::Specialty::Herding;
  struct Want { art::Critter k; const std::vector<RT::Spot>* v; };
  std::vector<Want> w;
  const int dogs = st.type == SiteType::Village ? 2 : 3, cats = 2;
  for (int k = 0; k < dogs; k++) w.push_back({art::Critter::Dog, &S.yard});
  for (int k = 0; k < cats; k++) w.push_back({art::Critter::Cat, &S.street});
  if (farm) { w.push_back({art::Critter::Rooster, &S.yard}); for (int k = 0; k < 3; k++) w.push_back({art::Critter::Chicken, &S.yard}); }
  if (herd || farm) { w.push_back({herd ? art::Critter::Goat : art::Critter::Pig, !S.pasture.empty() ? &S.pasture : &S.yard}); }
  if (!S.water.empty()) for (int k = 0; k < 2; k++) w.push_back({art::Critter::Duck, &S.water});
  int n = 0;
  for (size_t i = 0; i < w.size() && n < CRITTER_MAX; i++) {
    const std::vector<RT::Spot>& v = w[i].v->empty() ? S.street : *w[i].v;
    const RT::Spot* s = pick(g, v, h0 + (uint32_t)i * 2654435761u);
    if (!s) continue;
    const int lx = s->x - g.world.ox, ly = s->y - g.world.oy;
    if (len2(tileCentre(lx, ly) - g.pl().p) > (float)(Game::FOLK_OUT * TILE) * (float)(Game::FOLK_OUT * TILE)) continue;
    // hens of one yard stay together: near the rooster
    Actor a;
    a.id = g.nextId_++;
    a.npc = true; a.human = false; a.hostile = false; a.faction = Faction::Town;
    a.critter = (uint8_t)((int)w[i].k + 1);
    a.critterVar = mixu(c->seed, i * 977u + 13u);
    a.p = tileCentre(lx, ly); a.home = a.p; a.goal = a.p;
    a.site = si; a.slot = CRITTER_SLOT0 + (int)i; a.fromMap = true;
    a.maxHp = a.hp = 12; a.radius = w[i].k == art::Critter::Dog || w[i].k == art::Critter::Goat || w[i].k == art::Critter::Pig ? 4.0f : 2.5f;
    a.speed = w[i].k == art::Critter::Dog ? 60.0f : w[i].k == art::Critter::Cat ? 50.0f : 28.0f;
    a.lifeBits = LB_CRITTER;
    static const char* names[] = {"DOG", "CAT", "HEN", "ROOSTER", "GOAT", "PIG", "DUCK"};
    a.name = names[(int)w[i].k];
    a.level = 1;
    if (g.felled_.count(si) && g.felled_[si].count(a.slot)) continue;
    g.actors.push_back(a);
    n++;
  }
}

bool LifeOps::critterFolk(Game& g, Actor& a, float dt) {
  RT& R = rt(g);
  RT::ARt& A = R.rt(a.id);
  const art::Critter k = (art::Critter)(a.critter - 1);
  if (a.bubbleT > 0 && (a.bubbleT -= dt) <= 0) a.bubble = Bubble::None;
  // (M7) a yard spot that lies under a building or a solid prop now (a lot built on, a house's footprint): off it
  if (!g.inside && g.world.over.blocked(tileX(a.p), tileY(a.p))) { a.p = snapWalk(g, a.p); a.home = a.goal = a.p; }
  a.postureT += dt;
  A.t -= dt;
  // danger: a beast near (dogs bark at it, everyone else scatters)
  const Actor* threat = nullptr;
  float td = (6.0f * TILE) * (6.0f * TILE);
  for (int hi : g.hostiles_) {
    if (hi < 0 || hi >= (int)g.actors.size()) continue;
    const Actor& e = g.actors[(size_t)hi];
    if (e.st == AState::Dead || !e.hostile || e.player) continue;
    const float d = len2(e.p - a.p);
    if (d < td) { td = d; threat = &e; }
  }
  if (threat) {
    a.posture = Posture::None;
    if (k == art::Critter::Dog) {
      a.face = faceOf(threat->p - a.p);
      a.st = AState::Idle;
      if (A.t2 <= 0) { A.t2 = 1.3f; g.sfx((int)Sfx::Bark, a.p, 1.0f, 0.8f); bubble(a, Bubble::Exclaim, 0.9f); }
      A.t2 -= dt;
      return true;
    }
    const Vec2 away = norm(a.p - threat->p);
    ClimbOps::moveActor(g, a, away * (a.speed * 1.2f * dt));
    a.face = faceOf(away); a.st = AState::Walk;
    return true;
  }
  const bool night = g.hour >= 21.0f || g.hour < 5.5f;
  // dogs keep to their owner (the nearest resident in the street) by day, lie by a door at night
  if (k == art::Critter::Dog && !night) {
    int best = -1;
    float bd = (7.0f * TILE) * (7.0f * TILE);
    for (size_t j = 1; j < g.actors.size(); j++) {
      const Actor& o = g.actors[j];
      if (!(o.lifeBits & LB_LIFE) || o.site != a.site || o.st != AState::Walk) continue;
      const float d = len2(o.p - a.p);
      if (d < bd) { bd = d; best = (int)j; }
    }
    if (best > 0 && (mixu(a.id, (uint64_t)(g.time / 20.0f)) % 3u) != 0) {
      const Actor& o = g.actors[(size_t)best];
      const Vec2 to = o.p - o.aim * 12.0f + Vec2(8.0f, 2.0f);
      if (len2(to - a.p) > 10.0f * 10.0f) {
        a.posture = Posture::None;
        stepToward(g, a, to, a.speed * 0.8f, dt);
        return true;
      }
    }
  }
  if (night && (k == art::Critter::Dog || k == art::Critter::Cat || k == art::Critter::Chicken || k == art::Critter::Rooster)) {
    a.posture = Posture::Sleep; a.st = AState::Idle;
    return true;
  }
  // idle and potter about the home spot: peck, graze, groom
  if (A.t <= 0) {
    A.t = 2.0f + (float)(mixu(a.id, (uint64_t)(g.time * 2)) % 500u) * 0.01f;
    const uint32_t hh = mixu(a.id, (uint64_t)(g.time * 7));
    if (hh % 3u == 0) {
      a.goal = a.home + Vec2((float)((int)(hh >> 8) % 41 - 20), (float)((int)(hh >> 16) % 25 - 12));
      if (!ClimbOps::bodyFree(g, a.goal, a.radius, false)) a.goal = a.p;
      a.posture = Posture::None;
    } else {
      a.goal = a.p;
      a.posture = (hh % 3u == 1) ? Posture::Eat : Posture::Sit;   // the action frames (peck, graze, groom / sit and wag)
      a.postureT = 0;
      // (the idle voices are the view's: render.cpp picks one animal near the player every few seconds; the sim only
      // sounds event barks, e.g. a dog at wolves, so nothing is heard twice)
    }
  }
  const Vec2 d = a.goal - a.p;
  if (len2(d) > 4.0f) {
    const Vec2 before = a.p;
    stepToward(g, a, a.goal, a.speed * 0.35f, dt);
    if (len2(a.p - before) < 0.0004f) a.goal = a.p;
  } else a.st = AState::Idle;
  return true;
}
