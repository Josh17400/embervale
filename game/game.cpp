#include "game/game.h"
#include <cstdio>

namespace {
constexpr float CELL = 128.0f;
constexpr float HEAD_R = 14.0f;

bool pointInPoly(const std::vector<Vec2>& poly, Vec2 p) {
  bool in = false;
  size_t n = poly.size();
  for (size_t i = 0, j = n - 1; i < n; j = i++) {
    const Vec2& a = poly[i];
    const Vec2& b = poly[j];
    if (((a.y > p.y) != (b.y > p.y)) && (p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x)) in = !in;
  }
  return in;
}

float polyArea(const std::vector<Vec2>& poly) {
  float a = 0;
  for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) a += cross(poly[j], poly[i]);
  return std::fabs(a) * 0.5f;
}

// Segment p->p2 vs q->q2. Returns true and the point if they cross.
bool segIntersect(Vec2 p, Vec2 p2, Vec2 q, Vec2 q2, Vec2& out) {
  Vec2 r = p2 - p, s = q2 - q;
  float d = cross(r, s);
  if (std::fabs(d) < 1e-9f) return false;
  Vec2 qp = q - p;
  float t = cross(qp, s) / d;
  float u = cross(qp, r) / d;
  if (t < 0 || t > 1 || u < 0 || u > 1) return false;
  out = p + r * t;
  return true;
}

float typeXp(EType t) {
  switch (t) {
    case EType::Drifter: return 1;
    case EType::Charger: return 3;
    case EType::Splitter: return 3;
    case EType::Shooter: return 4;
    case EType::Boss: return 8;
  }
  return 1;
}
}  // namespace

const char* Game::upName(Up u) {
  switch (u) {
    case Up::LongTail: return "LONGER TAIL";
    case Up::BigBlast: return "BIGGER BLAST";
    case Up::BurnTail: return "BURNING TAIL";
    case Up::Magnet: return "MAGNET";
    case Up::Swift: return "SWIFT";
    case Up::TightTurn: return "TIGHT TURNS";
    case Up::QuickBoost: return "QUICK BOOST";
    case Up::Vitality: return "VITALITY";
    case Up::Lightning: return "LIGHTNING TAIL";
    case Up::LoopHeal: return "LOOP LEECH";
    case Up::WideTail: return "WIDE TAIL";
    case Up::EchoLoop: return "ECHO LOOP";
    default: return "?";
  }
}
const char* Game::upDesc(Up u) {
  switch (u) {
    case Up::LongTail: return "TAIL STRETCHES FARTHER FOR BIGGER LOOPS";
    case Up::BigBlast: return "LOOPS AND CHAIN BLASTS HIT HARDER AND WIDER";
    case Up::BurnTail: return "TAIL SETS ENEMIES ON FIRE";
    case Up::Magnet: return "PULL XP GEMS FROM FAR AWAY";
    case Up::Swift: return "FLY FASTER";
    case Up::TightTurn: return "TURN SHARPER FOR TIGHTER LOOPS";
    case Up::QuickBoost: return "BOOST RECHARGES FASTER";
    case Up::Vitality: return "+25 MAX HP AND HEAL";
    case Up::Lightning: return "ARCS FROM YOUR TAIL ZAP NEARBY ENEMIES";
    case Up::LoopHeal: return "HEAL FOR EVERY LOOP KILL";
    case Up::WideTail: return "THICKER TAIL HITS MORE ENEMIES";
    case Up::EchoLoop: return "EVERY LOOP DETONATES AGAIN SHORTLY AFTER";
    default: return "";
  }
}

// ---------------------------------------------------------------- derived
int Game::tailMaxPts() const { return 56 + 2 * level + 18 * upLevel[(int)Up::LongTail]; }
float Game::tailWidth() const { return 14.0f + 7.0f * upLevel[(int)Up::WideTail]; }
float Game::turnRate() const { return 5.4f + 0.7f * upLevel[(int)Up::TightTurn]; }
float Game::cruiseSpeed() const { return 430.0f * (1.0f + 0.08f * upLevel[(int)Up::Swift]); }
float Game::boostCooldown() const { return 3.2f * (1.0f - 0.18f * upLevel[(int)Up::QuickBoost]); }
float Game::magnetRadius() const { return 170.0f * (1.0f + 0.5f * upLevel[(int)Up::Magnet]); }
float Game::blastMul() const { return 1.0f + 0.3f * upLevel[(int)Up::BigBlast]; }
float Game::loopDamage() const { return 70.0f * blastMul(); }
float Game::chainRadius() const { return 100.0f * blastMul(); }
bool Game::bossAlive() const { return boss() != nullptr; }
const Enemy* Game::boss() const {
  for (const Enemy& e : enemies) if (e.t == EType::Boss && e.maxhp > 0) return &e;
  return nullptr;
}

// ---------------------------------------------------------------- lifecycle
void Game::reset(uint64_t seed) {
  rng = Rng(seed);
  stats = Stats{};
  head = prevHead = Vec2(ARENA_W * 0.5f, ARENA_H * 0.5f);
  heading = -PI * 0.5f;
  speed = cruiseSpeed();
  for (int& l : upLevel) l = 0;
  level = 1; xp = 0; xpNeed = 8;
  maxHp = 100; hp = 100; invuln = 0.5f;
  boostT = 0; boostCd = 0; boostHeld = false;
  tail.clear(); distAcc = 0;
  for (int i = 0; i < 24; i++) tail.push_back(head - fromAngle(heading) * (TAIL_SPACING * (i + 1)));
  enemies.clear(); gems.clear(); bullets.clear(); loopFx.clear(); zaps.clear(); events.clear();
  spawnAcc = 0; nextRing = 40.0f; bossesSpawned = 0; zapT = 0; echoT = 0; echoPoly.clear();
  pendingLevels = 0; numChoices = 0; chainShow = 0; chainCount = 0; chainQ.clear();
  gridW = (int)std::ceil(ARENA_W / CELL); gridH = (int)std::ceil(ARENA_H / CELL);
  grid.assign((size_t)gridW * gridH, {});
  tailGrid.assign((size_t)gridW * gridH, {});
  for (int i = 0; i < 10; i++) spawnEnemy(EType::Drifter, spawnPoint(rng.range(450, 750)));
  mode = Mode::Play;
}

void Game::setPaused(bool p) {
  if (p && mode == Mode::Play) mode = Mode::Paused;
  else if (!p && mode == Mode::Paused) mode = Mode::Play;
}

// ---------------------------------------------------------------- player
void Game::steer(float dt, const Input& in) {
  if (len(in.steer) > 0.12f) {
    float want = std::atan2(in.steer.y, in.steer.x);
    float d = wrapAngle(want - heading);
    float maxTurn = turnRate() * dt;
    heading = wrapAngle(heading + clampf(d, -maxTurn, maxTurn));
  }
  if (in.boost && !boostHeld && boostCd <= 0 && boostT <= 0) {
    boostT = 0.45f;
    boostCd = boostCooldown();
    emit(Ev::Boost, head);
  }
  boostHeld = in.boost;
  if (boostT > 0) boostT -= dt;
  if (boostCd > 0) boostCd -= dt;
  float target = cruiseSpeed() * (boostT > 0 ? 1.9f : 1.0f);
  speed += (target - speed) * std::min(1.0f, 9.0f * dt);

  prevHead = head;
  head += fromAngle(heading) * (speed * dt);
  const float M = 40;
  bool hit = false;
  if (head.x < M) { head.x = M; hit = true; }
  if (head.x > ARENA_W - M) { head.x = ARENA_W - M; hit = true; }
  if (head.y < M) { head.y = M; hit = true; }
  if (head.y > ARENA_H - M) { head.y = ARENA_H - M; hit = true; }
  if (hit) {
    // slide along the wall: turn toward the arena centre
    Vec2 c = Vec2(ARENA_W * 0.5f, ARENA_H * 0.5f) - head;
    float want = std::atan2(c.y, c.x);
    heading = wrapAngle(heading + clampf(wrapAngle(want - heading), -turnRate() * dt * 2, turnRate() * dt * 2));
  }
}

void Game::moveTail(float) {
  float L = len(head - prevHead);
  if (L < 1e-4f) return;
  distAcc += L;
  while (distAcc >= TAIL_SPACING) {
    float f = (L - (distAcc - TAIL_SPACING)) / L;
    tail.push_front(prevHead + (head - prevHead) * clampf(f, 0, 1));
    distAcc -= TAIL_SPACING;
  }
  while ((int)tail.size() > tailMaxPts()) tail.pop_back();
}

bool Game::checkLoop() {
  int n = (int)tail.size();
  if (n < MIN_LOOP_SEGS + 2) return false;
  for (int k = MIN_LOOP_SEGS; k < n - 1; k++) {
    Vec2 I;
    if (!segIntersect(prevHead, head, tail[k], tail[k + 1], I)) continue;
    std::vector<Vec2> poly;
    poly.reserve(k + 3);
    poly.push_back(I);
    for (int i = k; i >= 0; i--) poly.push_back(tail[i]);
    poly.push_back(head);
    if (polyArea(poly) < MIN_LOOP_AREA) return false;
    Vec2 c;
    for (const Vec2& p : poly) c += p;
    c *= 1.0f / (float)poly.size();
    int kills = doLoop(poly, c);
    // spend the looped tail: the ribbon restarts at the crossing point
    tail.erase(tail.begin(), tail.begin() + (k + 1));
    tail.push_front(I);
    distAcc = 0;
    if (upLevel[(int)Up::EchoLoop] > 0) { echoT = 0.4f; echoPoly = poly; echoCenter = c; }
    (void)kills;
    return true;
  }
  return false;
}

// Applies a loop implosion to everything inside poly, then resolves chain blasts.
int Game::doLoop(const std::vector<Vec2>& poly, Vec2 center) {
  float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
  for (const Vec2& p : poly) {
    minx = std::min(minx, p.x); maxx = std::max(maxx, p.x);
    miny = std::min(miny, p.y); maxy = std::max(maxy, p.y);
  }
  loopKillCount = 0;
  chainQ.clear();
  float dmg = loopDamage();
  for (size_t i = 0; i < enemies.size(); i++) {
    Enemy& e = enemies[i];
    if (e.maxhp <= 0) continue;
    if (e.p.x < minx || e.p.x > maxx || e.p.y < miny || e.p.y > maxy) continue;
    if (!pointInPoly(poly, e.p)) continue;
    float d = e.t == EType::Boss ? dmg * 3.0f + e.maxhp * 0.03f : dmg;
    damageEnemy((int)i, d, true);
  }
  float cr = chainRadius();
  int guard = 0;
  while (!chainQ.empty() && guard++ < 400) {
    Vec2 c = chainQ.back();
    chainQ.pop_back();
    float r2 = cr * cr;
    for (size_t i = 0; i < enemies.size(); i++) {
      Enemy& e = enemies[i];
      if (e.maxhp <= 0) continue;
      float rr = cr + e.r;
      if (len2(e.p - c) > rr * rr) continue;
      (void)r2;
      damageEnemy((int)i, 28.0f * blastMul(), true);
    }
  }
  int kills = loopKillCount;
  float radius = 0;
  for (const Vec2& p : poly) radius = std::max(radius, len(p - center));
  LoopFx fx;
  fx.poly = poly; fx.kills = kills; fx.center = center; fx.radius = radius;
  loopFx.push_back(std::move(fx));
  stats.loops++;
  stats.bestLoop = std::max(stats.bestLoop, kills);
  if (kills > 0) { chainCount = kills; chainShow = 1.8f; }
  emit(Ev::Loop, center, radius, kills);
  return kills;
}

// ---------------------------------------------------------------- enemies
Vec2 Game::spawnPoint(float dist) {
  Vec2 best = head + fromAngle(rng.range(0, TAU)) * dist;
  for (int i = 0; i < 8; i++) {
    Vec2 p = head + fromAngle(rng.range(0, TAU)) * dist;
    if (p.x > 30 && p.x < ARENA_W - 30 && p.y > 30 && p.y < ARENA_H - 30) return p;
    best = p;
  }
  best.x = clampf(best.x, 30, ARENA_W - 30);
  best.y = clampf(best.y, 30, ARENA_H - 30);
  return best;
}

void Game::spawnEnemy(EType t, Vec2 at, bool ring) {
  if (enemies.size() >= 700) return;
  float hpMul = 1.0f + stats.time / 200.0f;
  Enemy e;
  e.p = at; e.t = t; e.spawnT = ring ? 0.0f : 0.0f;
  switch (t) {
    case EType::Drifter: e.maxhp = e.hp = 12 * hpMul; e.r = 18; break;
    case EType::Charger: e.maxhp = e.hp = 34 * hpMul; e.r = 22; e.timer = rng.range(0.5f, 2.0f); break;
    case EType::Splitter: e.maxhp = e.hp = 46 * hpMul; e.r = 30; break;
    case EType::Shooter: e.maxhp = e.hp = 28 * hpMul; e.r = 20; e.timer = rng.range(0.6f, 2.0f); break;
    case EType::Boss: e.maxhp = e.hp = 2600.0f * (1.0f + 0.8f * bossesSpawned); e.r = 90; e.timer = 3.0f; break;
  }
  enemies.push_back(e);
}

void Game::spawnDirector(float dt) {
  float t = stats.time;
  float rate = 1.8f + 0.032f * t;
  spawnAcc += rate * dt;
  while (spawnAcc >= 1.0f) {
    spawnAcc -= 1.0f;
    float wC = t > 30 ? std::min(0.5f, 0.2f + t / 800.0f) : 0;
    float wS = t > 70 ? 0.2f : 0;
    float wH = t > 100 ? 0.18f : 0;
    float tot = 1.0f + wC + wS + wH;
    float r = rng.f() * tot;
    EType ty = EType::Drifter;
    if (r > 1.0f) {
      r -= 1.0f;
      if (r < wC) ty = EType::Charger;
      else if (r < wC + wS) ty = EType::Splitter;
      else ty = EType::Shooter;
    }
    spawnEnemy(ty, spawnPoint(rng.range(1150, 1350)));
  }
  if (t >= nextRing) {
    nextRing += 45.0f;
    int n = 26 + (int)(t / 60.0f) * 4;
    for (int i = 0; i < n; i++) {
      float a = TAU * i / n;
      Vec2 p = head + fromAngle(a) * 700.0f;
      p.x = clampf(p.x, 30, ARENA_W - 30);
      p.y = clampf(p.y, 30, ARENA_H - 30);
      spawnEnemy(EType::Drifter, p, true);
    }
  }
  if (bossesSpawned < 3 && t >= 180.0f * (bossesSpawned + 1)) {
    spawnEnemy(EType::Boss, spawnPoint(1000));
    bossesSpawned++;
    emit(Ev::BossSpawn, head);
  }
}

void Game::buildGrids() {
  for (auto& c : grid) c.clear();
  for (auto& c : tailGrid) c.clear();
  auto cellOf = [&](Vec2 p) {
    int cx = std::min(gridW - 1, std::max(0, (int)(p.x / CELL)));
    int cy = std::min(gridH - 1, std::max(0, (int)(p.y / CELL)));
    return cy * gridW + cx;
  };
  for (size_t i = 0; i < enemies.size(); i++) grid[cellOf(enemies[i].p)].push_back((int)i);
  for (size_t i = 0; i < tail.size(); i++) tailGrid[cellOf(tail[i])].push_back((int)i);
}

void Game::hurtPlayer(float dmg) {
  if (invuln > 0 || mode != Mode::Play) return;
  hp -= dmg;
  invuln = 0.7f;
  emit(Ev::Hurt, head, dmg);
  if (hp <= 0) {
    hp = 0;
    mode = Mode::Dead;
    emit(Ev::Dead, head);
  }
}

void Game::damageEnemy(int i, float dmg, bool viaLoop) {
  Enemy& e = enemies[i];
  if (e.maxhp <= 0) return;
  e.hp -= dmg;
  e.flash = 0.12f;
  if (e.hp <= 0) killEnemy(i, viaLoop);
}

void Game::killEnemy(int i, bool viaLoop) {
  enemies[i].maxhp = 0;
  const Enemy e = enemies[i];   // copy: push_back below may reallocate
  stats.kills++;
  emit(Ev::Pop, e.p, e.r, viaLoop ? 1 : 0);
  if (viaLoop) {
    loopKillCount++;
    chainQ.push_back(e.p);
    float heal = 0.5f * upLevel[(int)Up::LoopHeal];
    if (heal > 0) hp = std::min(maxHp, hp + heal);
  }
  float val = typeXp(e.t) * (viaLoop ? 1.5f : 1.0f);
  int pieces = e.t == EType::Boss ? 24 : 1;
  for (int k = 0; k < pieces; k++) {
    Gem g;
    g.p = e.p + (pieces > 1 ? Vec2(rng.range(-80, 80), rng.range(-80, 80)) : Vec2());
    g.v = fromAngle(rng.range(0, TAU)) * rng.range(30, 120);
    g.value = e.t == EType::Boss ? 8.0f : val;
    if (gems.size() > 800) gems[rng.irange((int)gems.size())].value += g.value;
    else gems.push_back(g);
  }
  if (e.t == EType::Splitter) {
    for (int k = 0; k < 2; k++) {
      Vec2 at = e.p + fromAngle(rng.range(0, TAU)) * 24.0f;
      // push directly: enemies vector may reallocate, so don't touch `e` after this
      Enemy d;
      d.p = at; d.t = EType::Drifter; d.r = 18;
      d.maxhp = d.hp = 12 * (1.0f + stats.time / 200.0f) * 0.6f;
      if (enemies.size() < 700) enemies.push_back(d);
    }
  }
  if (e.t == EType::Boss) { stats.bosses++; emit(Ev::BossDie, e.p); }
}

void Game::reapDead() {
  size_t w = 0;
  for (size_t i = 0; i < enemies.size(); i++) {
    if (enemies[i].maxhp > 0) {
      if (w != i) enemies[w] = enemies[i];
      w++;
    }
  }
  enemies.resize(w);
}

void Game::updateEnemies(float dt) {
  buildGrids();
  float t = stats.time;
  float spdMul = std::min(1.8f, 1.0f + t / 600.0f);
  float tw = tailWidth();
  int burnL = upLevel[(int)Up::BurnTail];
  bool boosting = boostT > 0;

  // tail contact
  for (size_t i = 0; i < enemies.size(); i++) {
    Enemy& e = enemies[i];
    if (e.maxhp <= 0) continue;
    if (e.tailCd > 0) e.tailCd -= dt;
    if (e.flash > 0) e.flash -= dt;
    if (e.spawnT < 1) e.spawnT += dt * 3;
    if (e.tailCd <= 0) {
      float hitR = e.r + tw * 0.5f + 6.0f;
      int cx = (int)(e.p.x / CELL), cy = (int)(e.p.y / CELL);
      bool touched = false;
      for (int gy = std::max(0, cy - 1); gy <= std::min(gridH - 1, cy + 1) && !touched; gy++)
        for (int gx = std::max(0, cx - 1); gx <= std::min(gridW - 1, cx + 1) && !touched; gx++)
          for (int idx : tailGrid[gy * gridW + gx]) {
            if (len2(tail[idx] - e.p) < hitR * hitR) { touched = true; break; }
          }
      if (touched) {
        e.tailCd = 0.3f;
        if (burnL > 0) e.burn = 2.0f;
        float d = e.t == EType::Boss ? 6.0f : 14.0f;
        damageEnemy((int)i, d, false);
        Enemy& e2 = enemies[i];   // damageEnemy may have grown the vector
        if (e2.maxhp > 0 && e2.t != EType::Boss) e2.p += norm(e2.p - head) * 8.0f;
      }
    }
  }
  reapDead();

  // movement / AI
  size_t count = enemies.size();
  for (size_t i = 0; i < count; i++) {
    Enemy& e = enemies[i];
    Vec2 to = head - e.p;
    float d = len(to);
    Vec2 dir = d > 1e-3f ? to * (1.0f / d) : Vec2(1, 0);
    if (e.burn > 0) {
      e.burn -= dt;
      float bd = 6.0f * burnL * dt;
      e.hp -= bd;
      if (e.hp <= 0) { killEnemy((int)i, false); continue; }
    }
    switch (e.t) {
      case EType::Drifter: e.v = dir * (95.0f * spdMul); break;
      case EType::Splitter: e.v = dir * (70.0f * spdMul); break;
      case EType::Boss: e.v = dir * 55.0f; break;
      case EType::Charger:
        e.timer -= dt;
        if (e.state == 0) {
          e.v = dir * (110.0f * spdMul);
          if (d < 430 && e.timer <= 0) { e.state = 1; e.timer = 0.6f; e.dir = dir; }
        } else if (e.state == 1) {
          e.v = Vec2(); e.dir = dir;
          if (e.timer <= 0) { e.state = 2; e.timer = 0.7f; }
        } else if (e.state == 2) {
          e.v = e.dir * 720.0f;
          if (e.timer <= 0) { e.state = 3; e.timer = 0.6f; }
        } else {
          e.v = Vec2();
          if (e.timer <= 0) { e.state = 0; e.timer = rng.range(0.8f, 2.0f); }
        }
        break;
      case EType::Shooter: {
        e.timer -= dt;
        Vec2 perp(-dir.y, dir.x);
        if (d > 540) e.v = dir * 120.0f;
        else if (d < 380) e.v = dir * -120.0f;
        else e.v = perp * 70.0f;
        if (e.timer <= 0) {
          e.timer = 2.2f;
          Bullet b; b.p = e.p; b.v = dir * 330.0f; b.life = 4.0f;
          bullets.push_back(b);
        }
        break;
      }
    }
    if (e.t == EType::Boss) {
      e.timer -= dt;
      if (e.timer <= 0) {
        e.timer = 5.0f;
        Vec2 bp = e.p;
        for (int k = 0; k < 6; k++) spawnEnemy(EType::Drifter, bp + fromAngle(TAU * k / 6.0f) * 120.0f);
      }
    }
  }
  // spawnEnemy/killEnemy may have reallocated `enemies`; indices stay valid, references do not.

  // separation (cheap crowd feel) + integrate
  for (size_t i = 0; i < enemies.size(); i++) {
    Enemy& e = enemies[i];
    if (e.maxhp <= 0) continue;
    if (e.t != EType::Boss && e.state != 2) {
      int cx = (int)(e.p.x / CELL), cy = (int)(e.p.y / CELL);
      for (int gy = std::max(0, cy - 1); gy <= std::min(gridH - 1, cy + 1); gy++)
        for (int gx = std::max(0, cx - 1); gx <= std::min(gridW - 1, cx + 1); gx++)
          for (int j : grid[gy * gridW + gx]) {
            if (j <= (int)i || j >= (int)enemies.size()) continue;
            Enemy& o = enemies[j];
            if (o.maxhp <= 0 || o.t == EType::Boss) continue;
            Vec2 dv = e.p - o.p;
            float rr = e.r + o.r;
            float dd = len2(dv);
            if (dd < rr * rr && dd > 1e-4f) {
              float dl = std::sqrt(dd);
              Vec2 push = dv * ((rr - dl) * 0.5f / dl);
              e.p += push * 0.5f;
              o.p -= push * 0.5f;
            }
          }
    }
    e.p += e.v * dt;
    e.p.x = clampf(e.p.x, 10, ARENA_W - 10);
    e.p.y = clampf(e.p.y, 10, ARENA_H - 10);
  }

  // head contact
  for (size_t i = 0; i < enemies.size(); i++) {
    Enemy& e = enemies[i];
    if (e.maxhp <= 0) continue;
    float rr = e.r + HEAD_R;
    if (len2(e.p - head) > rr * rr) continue;
    if (boosting) {
      damageEnemy((int)i, 45.0f, false);
      if (enemies[i].maxhp > 0 && enemies[i].t != EType::Boss) enemies[i].p += norm(enemies[i].p - head) * 30.0f;
    } else if (invuln <= 0) {
      float dmg = e.t == EType::Boss ? 20.0f : (e.t == EType::Charger ? 16.0f : (e.t == EType::Splitter ? 10.0f : 8.0f));
      hurtPlayer(dmg);
      if (e.t != EType::Boss) e.p += norm(e.p - head) * 40.0f;
    }
  }
  reapDead();
}

void Game::updateBullets(float dt) {
  size_t w = 0;
  for (size_t i = 0; i < bullets.size(); i++) {
    Bullet b = bullets[i];
    b.p += b.v * dt;
    b.life -= dt;
    bool keep = b.life > 0 && b.p.x > 0 && b.p.x < ARENA_W && b.p.y > 0 && b.p.y < ARENA_H;
    if (keep && len2(b.p - head) < (HEAD_R + 6) * (HEAD_R + 6)) {
      if (boostT <= 0) hurtPlayer(8.0f);
      keep = false;
    }
    if (keep) bullets[w++] = b;
  }
  bullets.resize(w);
}

void Game::addXp(float v) {
  xp += v;
  while (xp >= xpNeed) {
    xp -= xpNeed;
    level++;
    xpNeed = 6.0f + 4.0f * std::pow((float)level, 1.3f);
    pendingLevels++;
    hp = std::min(maxHp, hp + maxHp * 0.1f);
  }
  stats.level = level;
}

void Game::updateGems(float dt) {
  float mr = magnetRadius();
  size_t w = 0;
  for (size_t i = 0; i < gems.size(); i++) {
    Gem g = gems[i];
    Vec2 to = head - g.p;
    float d = len(to);
    if (d < 30) { addXp(g.value); emit(Ev::Gem, g.p, g.value); continue; }
    if (d < mr) g.magnet = true;
    if (g.magnet) {
      g.v = g.v * 0.9f + norm(to) * (900.0f * dt * 8);
      float sp = len(g.v);
      float maxsp = 900.0f + speed;
      if (sp > maxsp) g.v *= maxsp / sp;
    } else {
      g.v *= std::max(0.0f, 1.0f - 3.0f * dt);
    }
    g.p += g.v * dt;
    gems[w++] = g;
  }
  gems.resize(w);
}

void Game::rollChoices() {
  Up pool[(int)Up::COUNT];
  int n = 0;
  for (int i = 0; i < (int)Up::COUNT; i++)
    if (upLevel[i] < MAX_UPGRADE_LEVEL) pool[n++] = (Up)i;
  numChoices = std::min(3, n);
  for (int i = 0; i < numChoices; i++) {
    int j = i + rng.irange(n - i);
    std::swap(pool[i], pool[j]);
    choices[i] = pool[i];
  }
}

void Game::pickUpgrade(int idx) {
  if (mode != Mode::LevelUp || idx < 0 || idx >= numChoices) return;
  Up u = choices[idx];
  upLevel[(int)u]++;
  if (u == Up::Vitality) { maxHp += 25; hp = std::min(maxHp, hp + 25); }
  emit(Ev::Select, head);
  pendingLevels--;
  if (pendingLevels > 0) { rollChoices(); emit(Ev::Level, head); }
  else mode = Mode::Play;
}

// ---------------------------------------------------------------- step
void Game::update(float dt, const Input& in) {
  if (mode != Mode::Play) return;
  stats.time += dt;
  if (invuln > 0) invuln -= dt;
  if (chainShow > 0) chainShow -= dt;

  steer(dt, in);
  moveTail(dt);
  if (checkLoop()) {}
  if (echoT > 0) {
    echoT -= dt;
    if (echoT <= 0 && !echoPoly.empty()) {
      int saved = upLevel[(int)Up::BigBlast];
      doLoop(echoPoly, echoCenter);
      (void)saved;
      echoPoly.clear();
    }
  }
  updateEnemies(dt);
  updateBullets(dt);
  updateGems(dt);
  spawnDirector(dt);

  // lightning tail
  int lt = upLevel[(int)Up::Lightning];
  if (lt > 0 && !tail.empty()) {
    zapT -= dt;
    if (zapT <= 0) {
      zapT = std::max(0.25f, 0.75f - 0.09f * lt);
      for (int a = 0; a < 1 + lt; a++) {
        Vec2 src = tail[rng.irange((int)tail.size())];
        int best = -1; float bd = 280.0f * 280.0f;
        for (size_t i = 0; i < enemies.size(); i++) {
          if (enemies[i].maxhp <= 0) continue;
          float d = len2(enemies[i].p - src);
          if (d < bd) { bd = d; best = (int)i; }
        }
        if (best >= 0) {
          zaps.push_back({src, enemies[best].p, 0.18f});
          emit(Ev::Zap, enemies[best].p);
          damageEnemy(best, 18.0f + 6.0f * lt, false);
        }
      }
      reapDead();
    }
  }
  for (size_t i = 0; i < zaps.size();) {
    zaps[i].t -= dt;
    if (zaps[i].t <= 0) zaps.erase(zaps.begin() + i); else i++;
  }
  for (size_t i = 0; i < loopFx.size();) {
    loopFx[i].t += dt;
    if (loopFx[i].t > 1.0f) loopFx.erase(loopFx.begin() + i); else i++;
  }

  if (pendingLevels > 0 && mode == Mode::Play) {
    rollChoices();
    if (numChoices == 0) { pendingLevels = 0; }
    else { mode = Mode::LevelUp; emit(Ev::Level, head); }
  }
}
