#include "game2/mg_game.h"
#include <cstdio>
#include <cstring>

namespace {
// ---- value noise (deterministic from seed)
float hash2(int x, int y, uint32_t seed) {
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + seed * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  h ^= h >> 16;
  return (h & 0xFFFFFF) / 16777216.0f;
}
float smooth(float t) { return t * t * (3 - 2 * t); }
float vnoise(float x, float y, uint32_t seed) {
  int xi = (int)std::floor(x), yi = (int)std::floor(y);
  float fx = smooth(x - xi), fy = smooth(y - yi);
  float a = hash2(xi, yi, seed), b = hash2(xi + 1, yi, seed), c = hash2(xi, yi + 1, seed), d = hash2(xi + 1, yi + 1, seed);
  return lerpf(lerpf(a, b, fx), lerpf(c, d, fx), fy);
}
float fbm(float x, float y, uint32_t seed) {
  return vnoise(x, y, seed) * 0.55f + vnoise(x * 2.1f, y * 2.1f, seed + 7) * 0.30f + vnoise(x * 4.3f, y * 4.3f, seed + 13) * 0.15f;
}

struct SpeciesStat { float hp, dmg, range, aggro, speed, cd, splash; };
const SpeciesStat kStat[3] = {
  /* Knight */ {70, 7, 13, 90, 62, 0.70f, 0},
  /* Archer */ {34, 6, 88, 125, 58, 0.90f, 0},
  /* Mage   */ {28, 9, 72, 112, 52, 1.50f, 26},
};
constexpr float LV_SCALE = 1.7f;
}  // namespace

const char* MGame::speciesName(Species s) {
  switch (s) { case Species::Knight: return "KNIGHT"; case Species::Archer: return "ARCHER"; case Species::Mage: return "MAGE"; default: return "?"; }
}
float MGame::unitMaxHp(Species s, int lv) { return kStat[(int)s].hp * std::pow(LV_SCALE, (float)(lv - 1)); }
float MGame::unitDmg(Species s, int lv) { return kStat[(int)s].dmg * std::pow(LV_SCALE, (float)(lv - 1)); }

// ====================================================================== world
void MGame::carveDisc(int cx, int cy, int r, Tile t) {
  for (int y = cy - r; y <= cy + r; y++)
    for (int x = cx - r; x <= cx + r; x++) {
      if (!inMap(x, y)) continue;
      if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > r * r) continue;
      tiles[y * MAP_W + x] = t;
      objs[y * MAP_W + x] = Obj::None;
    }
}

void MGame::carveRoad(Vec2 a, Vec2 b, int halfW) {
  float d = len(b - a);
  int steps = (int)(d / 6.0f) + 1;
  for (int i = 0; i <= steps; i++) {
    Vec2 p = a + (b - a) * ((float)i / steps);
    carveDisc((int)(p.x / TILE), (int)(p.y / TILE), halfW, Tile::Dirt);
  }
}

void MGame::generate(uint64_t sd) {
  seed = sd;
  rng = Rng(sd);
  uint32_t s32 = (uint32_t)(sd * 2654435761u);
  tiles.assign((size_t)MAP_W * MAP_H, Tile::Grass);
  objs.assign((size_t)MAP_W * MAP_H, Obj::None);
  explored.assign((size_t)MAP_W * MAP_H, 0);
  exploredDirty = true;
  units.clear(); enemies.clear(); camps.clear(); villages.clear(); projs.clear(); pops.clear(); events.clear();
  solidRects.clear();
  gold = 30; hired = 0; raidNum = 0; raidTimer = 150.0f; raidActive = false; raidBanner = 0; time = 0; plundered_ = false; kills = 0; merges = 0;
  nextId = 1; banner.clear(); bannerT = 0;

  for (int y = 0; y < MAP_H; y++)
    for (int x = 0; x < MAP_W; x++) {
      float dx = (x - MAP_W * 0.5f) / (MAP_W * 0.5f), dy = (y - MAP_H * 0.5f) / (MAP_H * 0.5f);
      float d = std::max(std::fabs(dx), std::fabs(dy));
      float fall = clampf((d - 0.80f) / 0.2f, 0, 1);
      float e = fbm(x * 0.045f, y * 0.045f, s32) * 1.15f - 0.05f;
      e = e * (1.0f - 0.85f * fall * fall) - 0.25f * fall;
      Tile t = Tile::Grass;
      if (e < 0.30f) t = Tile::Water;
      else if (e < 0.345f) t = Tile::Sand;
      else if (e > 0.74f) t = Tile::Rock;
      tiles[y * MAP_W + x] = t;
      if (t == Tile::Grass) {
        float moist = fbm(x * 0.07f + 50, y * 0.07f + 50, s32 + 99);
        float h = hash2(x, y, s32 + 5);
        if (moist > 0.52f && h < 0.34f) objs[y * MAP_W + x] = Obj::Tree;
        else if (h < 0.035f) objs[y * MAP_W + x] = Obj::Tree;
        else if (h > 0.985f) objs[y * MAP_W + x] = Obj::Bush;
        else if (h > 0.972f) objs[y * MAP_W + x] = Obj::Boulder;
      }
    }

  // villages
  Village home;
  home.p = Vec2(MAP_W * 0.5f * TILE, MAP_H * 0.5f * TILE);
  home.home = true; home.discovered = true; home.name = "OAKHOLLOW";
  Village far;
  far.p = Vec2((MAP_W * 0.5f + 52) * TILE, (MAP_H * 0.5f - 34) * TILE);
  far.name = "RIVERMOOT";
  villages = {home, far};
  for (Village& v : villages) {
    v.door = v.p + Vec2(0, -6);
    carveDisc((int)(v.p.x / TILE), (int)(v.p.y / TILE), v.home ? 15 : 11, Tile::Grass);
  }
  carveRoad(villages[0].p, villages[1].p, 1);

  // camps (not too close to home or each other); each linked to home by a road so raiders can walk in
  int tries = 0;
  while ((int)camps.size() < 6 && tries++ < 2000) {
    int tx = 14 + rng.irange(MAP_W - 28), ty = 12 + rng.irange(MAP_H - 24);
    Vec2 p((tx + 0.5f) * TILE, (ty + 0.5f) * TILE);
    if (len(p - villages[0].p) < 40 * TILE) continue;
    if (len(p - villages[1].p) < 18 * TILE) continue;
    bool ok = true;
    for (const Camp& c : camps) if (len(c.p - p) < 30 * TILE) ok = false;
    if (!ok) continue;
    Camp c; c.p = p;
    camps.push_back(c);
  }
  for (size_t i = 0; i < camps.size(); i++) {
    carveDisc((int)(camps[i].p.x / TILE), (int)(camps[i].p.y / TILE), 6, Tile::Grass);
    carveRoad(camps[i].p, villages[0].p, 1);
  }

  placeBuildings();

  // hero + starting area
  hero = Hero{};
  hero.p = villages[0].door + Vec2(0, 26);
  revealAround(hero.p, 13);

  // two starter defenders already living at the home inn
  spawnUnit(Species::Knight, 1, villages[0].door + Vec2(-18, 22), UState::Lodged, 0);
  spawnUnit(Species::Archer, 1, villages[0].door + Vec2(20, 26), UState::Lodged, 0);

  // guaranteed early finds: two pairs near home so the first merges come quickly
  auto grassNear = [&](Vec2 c, float minR, float maxR) {
    for (int k = 0; k < 400; k++) {
      Vec2 p = c + fromAngle(rng.range(0, TAU)) * rng.range(minR, maxR);
      int tx = (int)(p.x / TILE), ty = (int)(p.y / TILE);
      if (!solidAt(p.x, p.y) && tileAt(tx, ty) != Tile::Water) return p;
    }
    return c;
  };
  Vec2 a = grassNear(villages[0].p, 150, 190);
  spawnUnit(Species::Knight, 1, a, UState::Wild);
  spawnUnit(Species::Knight, 1, grassNear(a, 20, 40), UState::Wild);
  Vec2 b = grassNear(villages[0].p, 170, 230);
  spawnUnit(Species::Archer, 1, b, UState::Wild);
  spawnUnit(Species::Archer, 1, grassNear(b, 20, 40), UState::Wild);

  // the wider world
  int made = 0;
  tries = 0;
  while (made < 26 && tries++ < 4000) {
    int tx = 6 + rng.irange(MAP_W - 12), ty = 6 + rng.irange(MAP_H - 12);
    Vec2 p((tx + 0.5f) * TILE, (ty + 0.5f) * TILE);
    if (solidAt(p.x, p.y) || tileAt(tx, ty) == Tile::Water) continue;
    if (len(p - villages[0].p) < 22 * TILE) continue;
    bool nearCamp = false;
    for (const Camp& c : camps) if (len(c.p - p) < 9 * TILE) nearCamp = true;
    if (nearCamp) continue;
    Species sp = (Species)rng.irange((int)Species::COUNT);
    int lv = rng.f() < 0.8f ? 1 : 2;
    spawnUnit(sp, lv, p, UState::Wild);
    made++;
    if (rng.f() < 0.45f) spawnUnit(sp, lv, grassNear(p, 16, 40), UState::Wild);
  }

  // drop wild units stranded on islets the player can't walk to
  {
    std::vector<uint8_t> seen((size_t)MAP_W * MAP_H, 0);
    std::vector<int> q;
    int sx = (int)(hero.p.x / TILE), sy = (int)(hero.p.y / TILE);
    q.push_back(sy * MAP_W + sx); seen[sy * MAP_W + sx] = 1;
    for (size_t h = 0; h < q.size(); h++) {
      int x = q[h] % MAP_W, y = q[h] / MAP_W;
      for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        int nx = x + dx, ny = y + dy;
        if (!inMap(nx, ny) || seen[ny * MAP_W + nx] || solidAt((nx + 0.5f) * TILE, (ny + 0.5f) * TILE)) continue;
        seen[ny * MAP_W + nx] = 1; q.push_back(ny * MAP_W + nx);
      }
    }
    size_t w = 0;
    for (size_t i = 0; i < units.size(); i++) {
      bool ok = units[i].st != UState::Wild || seen[(int)(units[i].p.y / TILE) * MAP_W + (int)(units[i].p.x / TILE)];
      if (ok) { if (w != i) units[w] = units[i]; w++; }
    }
    units.resize(w);
  }

  for (size_t i = 0; i < camps.size(); i++) spawnCampEnemies((int)i);
  mode = MMode::Title;
}

void MGame::placeBuildings() {
  // footprints in world px; the renderer uses the same layout constants (see mg_render.cpp)
  for (const Village& v : villages) {
    Vec2 p = v.p;
    auto add = [&](float x0, float y0, float x1, float y1) {
      solidRects.push_back(Vec2(x0, y0));
      solidRects.push_back(Vec2(x1, y1));
    };
    add(p.x - 24, p.y - 44, p.x + 24, p.y - 12);            // inn
    add(p.x - 88, p.y - 40, p.x - 56, p.y - 16);            // house L
    add(p.x + 56, p.y - 44, p.x + 88, p.y - 20);            // house R
    add(p.x - 16, p.y + 30, p.x + 16, p.y + 54);            // house S
  }
}

bool MGame::solidAt(float x, float y) const {
  int tx = (int)std::floor(x / TILE), ty = (int)std::floor(y / TILE);
  if (!inMap(tx, ty)) return true;
  Tile t = tiles[ty * MAP_W + tx];
  if (t == Tile::Water || t == Tile::Rock) return true;
  Obj o = objs[ty * MAP_W + tx];
  if (o == Obj::Tree || o == Obj::Boulder) return true;
  for (size_t i = 0; i + 1 < solidRects.size(); i += 2)
    if (x >= solidRects[i].x && x <= solidRects[i + 1].x && y >= solidRects[i].y && y <= solidRects[i + 1].y) return true;
  return false;
}

bool MGame::moveBody(Vec2& p, Vec2 d, float r) {
  auto blocked = [&](Vec2 q) {
    return solidAt(q.x - r, q.y - r * 0.4f) || solidAt(q.x + r, q.y - r * 0.4f) || solidAt(q.x - r, q.y + r) || solidAt(q.x + r, q.y + r);
  };
  bool moved = false;
  Vec2 q = p; q.x += d.x;
  if (!blocked(q)) { p.x = q.x; moved |= std::fabs(d.x) > 1e-5f; }
  q = p; q.y += d.y;
  if (!blocked(q)) { p.y = q.y; moved |= std::fabs(d.y) > 1e-5f; }
  return moved;
}

void MGame::revealAround(Vec2 p, int r) {
  int cx = (int)(p.x / TILE), cy = (int)(p.y / TILE);
  for (int y = cy - r; y <= cy + r; y++)
    for (int x = cx - r; x <= cx + r; x++) {
      if (!inMap(x, y)) continue;
      if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > r * r) continue;
      if (!explored[y * MAP_W + x]) { explored[y * MAP_W + x] = 1; exploredDirty = true; }
    }
}

// ====================================================================== units
void MGame::recomputeStats(Unit& u, bool heal) {
  float old = u.maxhp;
  u.maxhp = unitMaxHp(u.sp, u.lv);
  if (heal) u.hp = u.maxhp; else u.hp = std::min(u.maxhp, u.hp * (old > 0 ? u.maxhp / old : 1));
}

void MGame::spawnUnit(Species s, int lv, Vec2 p, UState st, int village) {
  Unit u;
  u.id = nextId++;
  u.sp = s; u.lv = lv; u.st = st; u.village = village; u.p = p; u.dest = p;
  u.maxhp = 0;
  recomputeStats(u, true);
  u.wander = st == UState::Wild ? 0.0f : rng.range(0, 3);   // wild: wander doubles as a 'party full' message cooldown
  units.push_back(u);
}

Unit* MGame::unitById(int id) { for (Unit& u : units) if (u.id == id) return &u; return nullptr; }
const Unit* MGame::unitById(int id) const { for (const Unit& u : units) if (u.id == id) return &u; return nullptr; }

int MGame::partyCount() const { int n = 0; for (const Unit& u : units) if (u.st == UState::Party) n++; return n; }

int MGame::unitAt(Vec2 p, float r) const {
  int best = -1; float bd = r * r;
  for (size_t i = 0; i < units.size(); i++) {
    if (units[i].st == UState::Wild) continue;
    float d = len2(units[i].p + Vec2(0, -5) - p);
    if (d < bd) { bd = d; best = units[i].id; }
  }
  return best;
}

int MGame::nearVillage(Vec2 p, float r) const {
  for (size_t i = 0; i < villages.size(); i++)
    if (villages[i].discovered && len(villages[i].p - p) < r) return (int)i;
  return -1;
}

bool MGame::tryMerge(int aId, int bId) {
  if (aId == bId) return false;
  Unit* a = unitById(aId);
  Unit* b = unitById(bId);
  if (!a || !b || a->st == UState::Wild || b->st == UState::Wild) return false;
  if (a->sp != b->sp || a->lv != b->lv || a->lv >= MAX_LV) { emit(Ev::Denied, b->p); return false; }
  bool lucky = rng.f() < 0.08f;
  int nl = std::min(MAX_LV, b->lv + 1 + (lucky ? 1 : 0));
  b->lv = nl;
  recomputeStats(*b, true);
  b->flash = 0.4f;
  Vec2 where = b->p;
  int n = nl;
  // remove a (pointer into the vector: erase by id)
  for (size_t i = 0; i < units.size(); i++) if (units[i].id == aId) { units.erase(units.begin() + i); break; }
  merges++;
  emit(Ev::Merge, where, 0, n);
  if (lucky) emit(Ev::Lucky, where, 0, n);
  pop(where + Vec2(0, -14), lucky ? "LUCKY! LV " + std::to_string(n) : "LV " + std::to_string(n), lucky ? 0xFF40E0FF : 0xFF60E8FF);
  return true;
}

void MGame::clickUnit(int id) {
  Unit* u = unitById(id);
  if (!u) return;
  if (u->st == UState::Party) {
    int v = nearVillage(u->p);
    if (v < 0) { pop(u->p + Vec2(0, -14), "FIND AN INN", 0xFF8080FF); emit(Ev::Denied, u->p); return; }
    u->st = UState::Lodged; u->village = v;
    emit(Ev::Lodge, u->p);
    pop(u->p + Vec2(0, -14), "LODGED", 0xFFB0FFB0);
  } else if (u->st == UState::Lodged) {
    if (partyCount() >= MAX_PARTY) { pop(u->p + Vec2(0, -14), "PARTY FULL", 0xFF8080FF); emit(Ev::Denied, u->p); return; }
    u->st = UState::Party;
    emit(Ev::Join, u->p);
    pop(u->p + Vec2(0, -14), "JOINED", 0xFFB0FFB0);
  }
}

int MGame::lodgeParty() {
  int v = nearVillage(hero.p);
  if (v < 0) { say("GET CLOSER TO AN INN"); emit(Ev::Denied, hero.p); return 0; }
  int n = 0;
  for (Unit& u : units) if (u.st == UState::Party) { u.st = UState::Lodged; u.village = v; n++; }
  if (n) { emit(Ev::Lodge, hero.p); say(std::to_string(n) + " ALLIES SETTLE IN AT " + villages[v].name); }
  else say("NO PARTY TO LODGE");
  return n;
}

bool MGame::hire() {
  if (len(hero.p - villages[0].p) > VILLAGE_R) { say("HIRE AT THE HOME INN"); emit(Ev::Denied, hero.p); return false; }
  if (gold < hireCost()) { say("NOT ENOUGH GOLD - NEED " + std::to_string(hireCost())); emit(Ev::Denied, hero.p); return false; }
  gold -= hireCost();
  hired++;
  Species sp = (Species)rng.irange((int)Species::COUNT);
  spawnUnit(sp, 1, villages[0].door + Vec2(rng.range(-14, 14), 12), UState::Lodged, 0);
  emit(Ev::Hire, villages[0].door);
  say(std::string("HIRED A ") + speciesName(sp));
  return true;
}

int MGame::autoMerge() {
  int count = 0;
  for (int guard = 0; guard < 60; guard++) {
    int ia = -1, ib = -1;
    for (size_t i = 0; i < units.size() && ia < 0; i++) {
      if (units[i].st == UState::Wild || units[i].lv >= MAX_LV) continue;
      for (size_t j = i + 1; j < units.size(); j++)
        if (units[j].st != UState::Wild && units[j].sp == units[i].sp && units[j].lv == units[i].lv) { ia = units[i].id; ib = units[j].id; break; }
    }
    if (ia < 0) break;
    if (!tryMerge(ia, ib)) break;
    count++;
  }
  return count;
}

void MGame::spawnCampEnemies(int ci) {
  Camp& c = camps[ci];
  float dist = len(c.p - villages[0].p) / TILE;
  int n = 3 + ci / 2 + (dist > 60 ? 1 : 0);
  c.total = n;
  for (int i = 0; i < n; i++) {
    Enemy e;
    e.id = nextId++;
    e.kind = (i == 0 && dist > 55) ? 1 : 0;
    e.p = c.p + fromAngle(rng.range(0, TAU)) * rng.range(14, 50);
    e.home = c.p;
    e.camp = ci;
    e.maxhp = e.hp = e.kind ? 90.0f : 18.0f;
    enemies.push_back(e);
  }
}

// ====================================================================== update
void MGame::hurtHero(float dmg) {
  if (godMode || hero.dead) return;
  hero.hp -= dmg;
  hero.hurt = 0.25f;
  emit(Ev::HeroHurt, hero.p, dmg);
  if (hero.hp <= 0) {
    hero.hp = 0; hero.dead = true; hero.respawn = 4.0f;
    emit(Ev::HeroDown, hero.p);
    say("YOU WERE KNOCKED OUT - RETURNING TO THE INN");
  }
}

void MGame::hurtUnit(Unit& u, float dmg) {
  if (u.fallen > 0) return;
  u.hp -= dmg;
  u.flash = 0.15f;
  if (u.hp <= 0) { u.hp = 0; u.fallen = 18.0f; pop(u.p + Vec2(0, -12), "KO", 0xFF8080FF); }
}

void MGame::damageEnemy(int idx, float dmg, bool) {
  if (idx < 0 || idx >= (int)enemies.size()) return;
  Enemy& e = enemies[idx];
  if (e.hp <= 0) return;
  e.hp -= dmg;
  e.flash = 0.1f;
  emit(Ev::Hit, e.p, dmg);
  if (e.hp <= 0) {
    int g = e.kind ? 15 : 3;
    if (e.camp < 0) g += 1 + raidNum / 3;
    gold += g;
    kills++;
    emit(Ev::Kill, e.p, 0, g);
    pop(e.p + Vec2(0, -10), "+" + std::to_string(g), 0xFF40D8FF);
  }
}

void MGame::updateHero(float dt, Vec2 move) {
  Hero& h = hero;
  if (h.hurt > 0) h.hurt -= dt;
  if (h.dead) {
    h.respawn -= dt;
    if (h.respawn <= 0) { h.dead = false; h.hp = h.maxhp; h.p = villages[0].door + Vec2(0, 26); }
    return;
  }
  float speed = 82.0f;
  float ml = len(move);
  h.moving = false;
  if (ml > 0.05f) {
    Vec2 dir = move * (1.0f / std::max(1.0f, ml));
    Vec2 d = dir * (speed * dt);
    h.moving = moveBody(h.p, d, 4.0f);
    if (std::fabs(dir.x) > 0.1f) h.face = dir.x > 0 ? 1.0f : -1.0f;
  }
  if (h.moving) h.anim += dt * 9.0f;
  // auto attack
  if (h.cd > 0) h.cd -= dt;
  int best = -1; float bd = 24.0f * 24.0f, near = 110.0f * 110.0f; bool danger = false;
  for (size_t i = 0; i < enemies.size(); i++) {
    float d = len2(enemies[i].p - h.p);
    if (d < bd) { bd = d; best = (int)i; }
    if (d < near) danger = true;
  }
  if (best >= 0 && h.cd <= 0) {
    h.cd = 0.42f;
    damageEnemy(best, 11.0f, true);
    emit(Ev::Shot, h.p, 0, 0);
    h.face = enemies[best].p.x >= h.p.x ? 1.0f : -1.0f;
  }
  if (!danger) h.hp = std::min(h.maxhp, h.hp + 4.0f * dt);

  // fog + discovery
  revealAround(h.p, 8);
  for (Village& v : villages)
    if (!v.discovered && len(v.p - h.p) < 120.0f) {
      v.discovered = true;
      emit(Ev::Discover, v.p);
      say("DISCOVERED " + v.name + "! LODGE YOUR ALLIES AT ITS INN");
    }
  // wild units join the party
  for (Unit& u : units) {
    if (u.st != UState::Wild) continue;
    if (u.wander > 0) { u.wander -= dt; continue; }
    if (len(u.p - h.p) < 18.0f) {
      if (partyCount() >= MAX_PARTY) { pop(u.p + Vec2(0, -12), "PARTY FULL", 0xFF8080FF); u.wander = 3.0f; continue; }
      u.st = UState::Party;
      emit(Ev::Recruit, u.p);
      pop(u.p + Vec2(0, -14), std::string("FOUND ") + speciesName(u.sp) + "!", 0xFF70FFD0);
    }
  }
  // camp chests
  for (Camp& c : camps) {
    if (!c.cleared || c.chestTaken) continue;
    if (len(c.p + Vec2(0, 18) - h.p) < 16.0f) {
      c.chestTaken = true;
      gold += 40;
      Species sp = (Species)rng.irange((int)Species::COUNT);
      spawnUnit(sp, rng.f() < 0.3f ? 2 : 1, h.p + Vec2(10, 6), partyCount() < MAX_PARTY ? UState::Party : UState::Lodged, 0);
      emit(Ev::Chest, c.p);
      say(std::string("CHEST! +40 GOLD AND A ") + speciesName(sp));
    }
  }
}

void MGame::updateUnits(float dt) {
  int partyIdx = 0;
  for (Unit& u : units) {
    if (u.st == UState::Wild) continue;
    const SpeciesStat& S = kStat[(int)u.sp];
    if (u.flash > 0) u.flash -= dt;
    if (u.fallen > 0) {
      u.fallen -= dt;
      if (u.fallen <= 0) u.hp = u.maxhp * 0.5f;
      if (u.st == UState::Party) partyIdx++;
      continue;
    }
    if (u.cd > 0) u.cd -= dt;
    Vec2 anchor = u.st == UState::Party ? hero.p : villages[std::max(0, u.village)].p;
    float leash = u.st == UState::Party ? 170.0f : 150.0f;

    int target = -1; float bd = S.aggro * S.aggro;
    for (size_t i = 0; i < enemies.size(); i++) {
      if (enemies[i].hp <= 0) continue;
      if (len2(enemies[i].p - anchor) > leash * leash) continue;
      float d = len2(enemies[i].p - u.p);
      if (d < bd) { bd = d; target = (int)i; }
    }
    float speed = S.speed;
    Vec2 dir;
    u.moving = false;
    if (target >= 0) {
      Enemy& e = enemies[target];
      Vec2 to = e.p - u.p;
      float d = len(to);
      u.face = to.x >= 0 ? 1.0f : -1.0f;
      if (d > S.range * 0.9f) dir = norm(to);
      else if (u.sp != Species::Knight && d < S.range * 0.45f) dir = norm(to) * -0.8f;
      if (d <= S.range && u.cd <= 0) {
        float dmg = unitDmg(u.sp, u.lv);
        u.cd = S.cd / (1.0f + 0.04f * (u.lv - 1));
        if (u.sp == Species::Knight) { damageEnemy(target, dmg, false); emit(Ev::Shot, u.p, 1, 0); }
        else { projs.push_back({u.p + Vec2(0, -6), e.id, dmg, S.splash * (1.0f + 0.1f * (u.lv - 1)), u.sp == Species::Archer}); emit(Ev::Shot, u.p, 2, 0); }
      }
    } else if (u.st == UState::Party) {
      float ang = partyIdx * 2.1f + 0.6f;
      Vec2 want = hero.p + fromAngle(ang) * (16.0f + 5.0f * (partyIdx / 3)) + Vec2(0, 4);
      Vec2 to = want - u.p;
      float d = len(to);
      if (d > 220.0f) { u.p = hero.p + Vec2(rng.range(-12, 12), rng.range(8, 16)); }
      else if (d > 5.0f) { dir = norm(to); if (d > 50.0f) speed *= 1.9f; }
    } else {   // lodged: idle around the inn yard, heal
      u.hp = std::min(u.maxhp, u.hp + 3.0f * dt);
      u.wander -= dt;
      Vec2 door = villages[std::max(0, u.village)].door;
      if (u.wander <= 0) {
        u.wander = rng.range(1.5f, 4.5f);
        u.dest = door + Vec2(rng.range(-46, 46), rng.range(10, 46));
      }
      Vec2 to = u.dest - u.p;
      if (len(to) > 3.0f) { dir = norm(to); speed *= 0.5f; }
    }
    if (u.st == UState::Party && target < 0) u.hp = std::min(u.maxhp, u.hp + 1.5f * dt);
    if (len2(dir) > 1e-4f) {
      u.moving = moveBody(u.p, dir * (speed * dt), 3.0f);
      if (std::fabs(dir.x) > 0.2f) u.face = dir.x > 0 ? 1.0f : -1.0f;
    }
    if (u.moving) u.anim += dt * 8.0f;
    if (u.st == UState::Party) partyIdx++;
  }
}

void MGame::updateEnemies(float dt) {
  Village& home = villages[0];
  for (Enemy& e : enemies) {
    if (e.hp <= 0) continue;
    if (e.flash > 0) e.flash -= dt;
    if (e.cd > 0) e.cd -= dt;
    float speed = e.kind ? 28.0f : 36.0f, range = e.kind ? 13.0f : 10.0f, aggro = e.kind ? 90.0f : 80.0f;
    float dmg = (e.kind ? 11.0f : 4.0f) * (e.camp < 0 ? 1.0f + 0.08f * raidNum : 1.0f);
    // nearest friendly
    int tType = 0; int tIdx = -1; float bd = aggro * aggro; Vec2 tp;
    if (!hero.dead) { float d = len2(hero.p - e.p); if (d < bd) { bd = d; tType = 1; tp = hero.p; } }
    for (size_t i = 0; i < units.size(); i++) {
      const Unit& u = units[i];
      if (u.st == UState::Wild || u.fallen > 0) continue;
      float d = len2(u.p - e.p);
      if (d < bd) { bd = d; tType = 2; tIdx = (int)i; tp = u.p; }
    }
    if (e.camp >= 0 && tType != 0 && len(tp - e.home) > 170.0f) tType = 0;   // camps don't chase far
    Vec2 dir;
    e.moving = false;
    if (tType != 0) {
      Vec2 to = tp - e.p;
      float d = len(to);
      e.face = to.x >= 0 ? 1.0f : -1.0f;
      if (d > range * 0.85f) dir = norm(to);
      else if (e.cd <= 0) {
        e.cd = e.kind ? 1.3f : 1.0f;
        if (tType == 1) hurtHero(dmg); else hurtUnit(units[tIdx], dmg);
      }
    } else if (e.camp < 0) {   // raider marches on the home village
      Vec2 to = home.p - e.p;
      float d = len(to);
      if (d > 30.0f) { dir = norm(to); e.face = to.x >= 0 ? 1.0f : -1.0f; }
      else if (e.cd <= 0) {
        e.cd = 1.2f;
        home.hp -= e.kind ? 9.0f : 3.0f;
        emit(Ev::VillageHit, home.p);
        if (home.hp <= 0) {
          home.hp = home.maxhp;
          int lost = gold / 2;
          gold -= lost;
          emit(Ev::Plundered, home.p, 0, lost);
          say("OAKHOLLOW WAS PLUNDERED! LOST " + std::to_string(lost) + " GOLD");
          for (Enemy& r : enemies) if (r.camp < 0) r.hp = 0;
          plundered_ = true;
          return;
        }
      }
    } else {   // camp idle: loiter near the fire
      if (len(e.p - e.home) > 46.0f) dir = norm(e.home - e.p) * 0.6f;
    }
    if (len2(dir) > 1e-4f) {
      e.moving = moveBody(e.p, dir * (speed * dt), 3.0f);
      if (!e.moving && e.camp < 0) e.p += Vec2(dir.y, -dir.x) * (speed * dt * 0.7f);   // slide around obstacles
    }
    if (e.moving) e.anim += dt * 8.0f;
  }
}

void MGame::updateProjs(float dt) {
  for (size_t i = 0; i < projs.size();) {
    Proj& p = projs[i];
    int ti = -1;
    for (size_t k = 0; k < enemies.size(); k++) if (enemies[k].id == p.targetId && enemies[k].hp > 0) { ti = (int)k; break; }
    if (ti < 0) { projs.erase(projs.begin() + i); continue; }
    Vec2 to = enemies[ti].p + Vec2(0, -4) - p.p;
    float d = len(to);
    float step = 230.0f * dt;
    if (d <= step + 3.0f) {
      Vec2 at = enemies[ti].p;
      float dmg = p.dmg, sp = p.splash;
      damageEnemy(ti, dmg, false);
      if (sp > 0)
        for (size_t k = 0; k < enemies.size(); k++)
          if ((int)k != ti && enemies[k].hp > 0 && len(enemies[k].p - at) < sp) damageEnemy((int)k, dmg * 0.7f, false);
      projs.erase(projs.begin() + i);
      continue;
    }
    p.p += to * (step / d);
    i++;
  }
}

void MGame::updateRaids(float dt) {
  raidTimer -= dt;
  if (raidBanner > 0) raidBanner -= dt;
  // raid over?
  if (raidActive && plundered_) { raidActive = false; plundered_ = false; }
  if (raidActive) {
    bool any = false;
    for (const Enemy& e : enemies) if (e.camp < 0 && e.hp > 0) { any = true; break; }
    if (!any) {
      raidActive = false;
      int reward = 12 + raidNum * 4;
      gold += reward;
      emit(Ev::RaidEnd, villages[0].p, 0, reward);
      say("RAID REPELLED! +" + std::to_string(reward) + " GOLD");
    }
  }
  if (raidTimer <= 0 && !raidActive) {
    std::vector<int> live;
    for (size_t i = 0; i < camps.size(); i++) if (!camps[i].cleared) live.push_back((int)i);
    if (live.empty()) { raidTimer = 1e9f; say("EVERY CAMP IS CLEARED - THE MEADOW IS SAFE!"); return; }
    int ci = live[rng.irange((int)live.size())];
    raidNum++;
    raidTimer = 90.0f;
    raidActive = true;
    raidBanner = 5.0f;
    raidFrom = camps[ci].p;
    int n = 3 + raidNum;
    for (int i = 0; i < n; i++) {
      Enemy e;
      e.id = nextId++;
      e.kind = (raidNum % 3 == 0 && i == 0) ? 1 : 0;
      e.p = camps[ci].p + fromAngle(rng.range(0, TAU)) * rng.range(10, 36);
      e.home = e.p; e.camp = -1;
      e.maxhp = e.hp = (e.kind ? 90.0f : 18.0f) * (1.0f + 0.1f * raidNum);
      enemies.push_back(e);
    }
    Vec2 dd = raidFrom - villages[0].p;
    const char* dir = std::fabs(dd.x) > std::fabs(dd.y) ? (dd.x > 0 ? "EAST" : "WEST") : (dd.y > 0 ? "SOUTH" : "NORTH");
    emit(Ev::RaidWarn, raidFrom);
    say(std::string("RAID INCOMING FROM THE ") + dir + "!");
  }
}

void MGame::update(float dt, Vec2 move) {
  if (mode != MMode::Play) return;
  time += dt;
  if (bannerT > 0) bannerT -= dt;
  for (size_t i = 0; i < pops.size();) { pops[i].t += dt; if (pops[i].t > 1.2f) pops.erase(pops.begin() + i); else i++; }

  updateHero(dt, move);
  updateUnits(dt);
  updateEnemies(dt);
  updateProjs(dt);
  updateRaids(dt);

  // camp clear detection
  for (size_t ci = 0; ci < camps.size(); ci++) {
    Camp& c = camps[ci];
    if (c.cleared) continue;
    bool any = false;
    for (const Enemy& e : enemies) if (e.camp == (int)ci && e.hp > 0) { any = true; break; }
    if (!any) {
      c.cleared = true;
      gold += 25;
      emit(Ev::CampCleared, c.p);
      say("CAMP CLEARED! +25 GOLD - A CHEST WAS LEFT BEHIND");
    }
  }
  // reap the dead
  size_t w = 0;
  for (size_t i = 0; i < enemies.size(); i++) if (enemies[i].hp > 0) { if (w != i) enemies[w] = enemies[i]; w++; }
  enemies.resize(w);
}

// ====================================================================== save / load
namespace {
struct W {
  std::vector<uint8_t>& o;
  template <class T> void put(const T& v) { const uint8_t* p = (const uint8_t*)&v; o.insert(o.end(), p, p + sizeof(T)); }
};
struct R {
  const std::vector<uint8_t>& i; size_t pos = 0; bool ok = true;
  template <class T> T get() { T v{}; if (pos + sizeof(T) > i.size()) { ok = false; return v; } std::memcpy(&v, &i[pos], sizeof(T)); pos += sizeof(T); return v; }
};
}  // namespace

void MGame::serialize(std::vector<uint8_t>& out) const {
  W w{out};
  w.put<uint32_t>(0x31444C48u);   // "HLD1"
  w.put<uint64_t>(seed);
  w.put(hero.p.x); w.put(hero.p.y); w.put(hero.hp);
  w.put<int32_t>(gold); w.put<int32_t>(hired); w.put<int32_t>(raidNum); w.put(raidTimer);
  w.put(time); w.put<int32_t>(kills); w.put<int32_t>(merges); w.put<int32_t>(nextId);
  out.insert(out.end(), explored.begin(), explored.end());
  w.put<uint32_t>((uint32_t)camps.size());
  for (const Camp& c : camps) { w.put<uint8_t>(c.cleared); w.put<uint8_t>(c.chestTaken); }
  w.put<uint32_t>((uint32_t)villages.size());
  for (const Village& v : villages) { w.put<uint8_t>(v.discovered); w.put(v.hp); }
  w.put<uint32_t>((uint32_t)units.size());
  for (const Unit& u : units) {
    w.put<int32_t>(u.id); w.put<uint8_t>((uint8_t)u.sp); w.put<uint8_t>((uint8_t)u.lv); w.put<uint8_t>((uint8_t)u.st);
    w.put<int8_t>((int8_t)u.village); w.put(u.p.x); w.put(u.p.y); w.put(u.hp);
  }
}

bool MGame::deserialize(const std::vector<uint8_t>& in) {
  R r{in};
  if (r.get<uint32_t>() != 0x31444C48u) return false;
  uint64_t sd = r.get<uint64_t>();
  if (!r.ok) return false;
  generate(sd);
  hero.p.x = r.get<float>(); hero.p.y = r.get<float>(); hero.hp = r.get<float>();
  gold = r.get<int32_t>(); hired = r.get<int32_t>(); raidNum = r.get<int32_t>(); raidTimer = r.get<float>();
  time = r.get<float>(); kills = r.get<int32_t>(); merges = r.get<int32_t>(); nextId = r.get<int32_t>();
  if (!r.ok || r.pos + explored.size() > in.size()) { generate(sd); return false; }
  std::memcpy(explored.data(), &in[r.pos], explored.size());
  r.pos += explored.size();
  exploredDirty = true;
  uint32_t nc = r.get<uint32_t>();
  for (uint32_t i = 0; i < nc; i++) { uint8_t a = r.get<uint8_t>(), b = r.get<uint8_t>(); if (i < camps.size()) { camps[i].cleared = a; camps[i].chestTaken = b; } }
  uint32_t nv = r.get<uint32_t>();
  for (uint32_t i = 0; i < nv; i++) { uint8_t d = r.get<uint8_t>(); float hp = r.get<float>(); if (i < villages.size()) { villages[i].discovered = d || villages[i].home; villages[i].hp = hp; } }
  uint32_t nu = r.get<uint32_t>();
  units.clear();
  for (uint32_t i = 0; i < nu && r.ok; i++) {
    Unit u;
    u.id = r.get<int32_t>(); u.sp = (Species)r.get<uint8_t>(); u.lv = r.get<uint8_t>(); u.st = (UState)r.get<uint8_t>();
    u.village = r.get<int8_t>(); u.p.x = r.get<float>(); u.p.y = r.get<float>(); float hp = r.get<float>();
    u.dest = u.p; u.maxhp = 0; recomputeStats(u, true); u.hp = std::min(u.maxhp, hp);
    u.wander = rng.range(0, 3);
    if (r.ok) units.push_back(u);
  }
  if (!r.ok) { generate(sd); return false; }
  // camps that were cleared lose their goblins
  for (size_t i = 0; i < camps.size(); i++)
    if (camps[i].cleared)
      for (Enemy& e : enemies) if (e.camp == (int)i) e.hp = 0;
  size_t w = 0;
  for (size_t i = 0; i < enemies.size(); i++) if (enemies[i].hp > 0) { if (w != i) enemies[w] = enemies[i]; w++; }
  enemies.resize(w);
  raidActive = false;
  return true;
}
