// rpg_test --climb [--seeds A..B] [--crossings N]: M11 "High Ground", moving over the relief (VISION_PLAN 11.2, 11.4,
// 13 M11, 15.29, 15.31, 15.32). CLIMB lane owns this file.
//
// The protocol (15.32), every check 100 %:
//   - the contract: Map::reliefSpeed (hillside 0.8, earth ramp 0.8, stone stairs 0.9, flat 1), the step mask;
//   - speed: what the player (Input through Game::update), an NPC, a companion and a mount actually move at on a
//     hillside, an earth ramp and stone stairs, against flat ground, measured over a timed walk along a run of those
//     tiles once the pace has eased in (within 3 %); the pace eases (no single step changes it by more than 0.2 of a
//     level's worth); a sure-footed horse is exempt;
//   - pathing: per seed, a new endless game; N (12) crossings in the loaded windows (a ramp or stairs whose low side
//     and high side are walkable, picked beside a cliff of the same face so the straight line is blocked): a guard
//     sent from 8 tiles out on the low side to 8 tiles beyond on the high side arrives within 1 tile in 25 s of sim
//     (Game::navStep, the guard's chase pace); the same for a goal across a cliff whose nearest way up is 16-24 tiles
//     along the face (no "no path" give-up); a companion left on the low side reaches the player standing on the
//     plateau (the party's own follow code through Game::update), with no off-screen catch-up;
//   - waterfalls are walls: a swimmer, a rowboat (the hero aboard: Actor::swim 1) and the wading hero pushed at a fall
//     stop short of it (a real fall of the windows, else a river tile made one for the check);
//   - cost: the path searches' requests, long searches, nodes and worst ms are printed (the budget: no search over
//     NAV_BUDGET nodes, the worst navStep under 4 ms).
// The windows: the start's, then the named ranges and peaks nearest the start (as rpg_test --relief), until the
// crossings are found.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <utility>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/home.h"
#include "rpg/sim/party.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {
constexpr float T = (float)TILE;
int c_bad = 0;
#define CLIMB_FAIL(...) do { out("FAIL: climb: " __VA_ARGS__); c_bad++; } while (0)

Vec2 centreOf(int x, int y) { return Vec2(x * T + 8.0f, y * T + 10.0f); }
int tileXOf(Vec2 p) { return (int)std::floor(p.x / T); }
int tileYOf(Vec2 p) { return (int)std::floor((p.y - 2) / T); }

Game makeGame(uint64_t seed) {
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.noWildSpawns = true;
  g.godMode = true;
  g.hour = 12;
  return g;
}
void run(Game& g, float secs) {
  const int n = (int)std::lround(secs / SIM_DT);
  for (int f = 0; f < n; f++) {
    g.update(SIM_DT, Input());
    g.events.clear();
    if (g.mode != Mode::Play) g.mode = Mode::Play;
  }
}

// a tile a body may stand on outdoors (not blocked, no building over it)
bool walkOpen(const Map& m, int x, int y) { return m.in(x, y) && !m.blocked(x, y) && m.bldgAt[(size_t)y * m.w + x] < 0; }

// BFS distance (tiles, 4-connected over open ground) from s to t inside the window; -1 none
int walkDist(const Map& m, int sx, int sy, int tx, int ty, int cap) {
  if (!walkOpen(m, sx, sy) || !walkOpen(m, tx, ty)) return -1;
  std::vector<int> dist((size_t)m.w * m.h, -1);
  std::deque<int> q;
  dist[(size_t)sy * m.w + sx] = 0;
  q.push_back(sy * m.w + sx);
  static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
  while (!q.empty()) {
    const int c = q.front();
    q.pop_front();
    const int cx = c % m.w, cy = c / m.w, cd = dist[(size_t)c];
    if (cx == tx && cy == ty) return cd;
    if (cd >= cap) continue;
    for (int k = 0; k < 4; k++) {
      const int nx = cx + dx[k], ny = cy + dy[k];
      if (!walkOpen(m, nx, ny) || dist[(size_t)ny * m.w + nx] >= 0) continue;
      dist[(size_t)ny * m.w + nx] = cd + 1;
      q.push_back(ny * m.w + nx);
    }
  }
  return -1;
}

struct Crossing {
  int fx, fy;     // the cliff tile the straight line meets (the lower tile of the face)
  int ux, uy;     // the way up (toward the higher level)
  int sx, sy, gx, gy;   // 8 tiles out on the low side, 8 beyond on the high side
  int along;      // tiles along the face from the cliff tile to its nearest way up (ramp, stairs, hillside)
  int walk;       // BFS tiles from start to goal
};

// how far along the face (both ways along the perpendicular) the nearest walkable step of the same face lies
int wayUpAlong(const Map& m, int fx, int fy, int ux, int uy, int maxD) {
  const int px = uy, py = ux;   // the face's direction
  const int lvl = m.heightAt(fx, fy);
  for (int d = 1; d <= maxD; d++)
    for (int s = -1; s <= 1; s += 2) {
      const int x = fx + px * d * s, y = fy + py * d * s;
      const uint8_t b = m.heightBits(x, y);
      if ((b & (Map::HEIGHT_RAMP | Map::HEIGHT_SLOPE)) && m.heightAt(x + ux, y + uy) > (b & Map::HEIGHT_LEVEL) && !m.blocked(x, y)) return d;
      (void)lvl;
    }
  return 999;
}

// the crossings of the loaded window: by a ramp / stairs, and (hard) a cliff whose way up is 16-24 tiles along
void findCrossings(const Map& m, int ptx, int pty, std::vector<Crossing>& easy, std::vector<Crossing>& hard) {
  static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
  const int M = 34;
  std::vector<std::pair<int, Crossing>> e, h;
  for (int y = M; y < m.h - M; y++)
    for (int x = M; x < m.w - M; x++) {
      const uint8_t b = m.heightBits(x, y);
      if (!(b & Map::HEIGHT_CLIFF) || (b & Map::HEIGHT_FALL) || groundWater(m.at(x, y))) continue;
      const int lvl = b & Map::HEIGHT_LEVEL;
      int ux = 0, uy = 0, ups = 0;
      for (int k = 0; k < 4; k++)
        if (m.heightAt(x + dx[k], y + dy[k]) > lvl) { ux = dx[k]; uy = dy[k]; ups++; }
      if (ups != 1) continue;   // (a plain run of face, not a corner)
      const int sx = x - ux * 8, sy = y - uy * 8, gx = x + ux * 8, gy = y + uy * 8;
      if (!walkOpen(m, sx, sy) || !walkOpen(m, gx, gy)) continue;
      if (m.heightAt(sx, sy) > lvl || m.heightAt(gx, gy) <= lvl) continue;
      // the start and goal away from other steps (a body stands on plain ground)
      if ((m.heightBits(sx, sy) & Map::HEIGHT_STEP) || (m.heightBits(gx, gy) & Map::HEIGHT_STEP)) continue;
      const int along = wayUpAlong(m, x, y, ux, uy, 26);
      Crossing c{x, y, ux, uy, sx, sy, gx, gy, along, 0};
      const int d2 = (x - ptx) * (x - ptx) + (y - pty) * (y - pty);
      if (along >= 2 && along <= 6) e.push_back({d2, c});
      else if (along >= 16 && along <= 24) h.push_back({d2, c});
    }
  auto pick = [&](std::vector<std::pair<int, Crossing>>& v, std::vector<Crossing>& outV, int want) {
    std::sort(v.begin(), v.end(), [](const std::pair<int, Crossing>& p, const std::pair<int, Crossing>& q) {
      return p.first != q.first ? p.first < q.first : (p.second.fy != q.second.fy ? p.second.fy < q.second.fy : p.second.fx < q.second.fx);
    });
    for (auto& pc : v) {
      if ((int)outV.size() >= want) break;
      Crossing c = pc.second;
      bool near = false;
      for (const Crossing& o : outV) near |= std::abs(o.fx - c.fx) + std::abs(o.fy - c.fy) < 14;
      if (near) continue;
      // a way exists inside the window and within the lattice's reach (16 + 2 x 24 tiles and the slack of a detour)
      c.walk = walkDist(m, c.sx, c.sy, c.gx, c.gy, 110);
      if (c.walk < 0 || c.walk > 96) continue;
      outV.push_back(c);
    }
  };
  pick(e, easy, 40);
  pick(h, hard, 6);
}

// a guard sent across: navStep at the guard's chase pace (ai.cpp: speed 46 x 1.2) until it stands within a tile of
// the goal or the time is up. Returns the seconds taken (-1: did not arrive) and the closest it came (tiles).
float sendGuard(Game& g, const Crossing& c, float limit, int id, float& closest) {
  Actor a;
  a.id = id; a.npc = true; a.human = true; a.role = Role::Guard; a.faction = Faction::Town;
  a.speed = 46; a.radius = 4.5f;
  a.p = centreOf(c.sx, c.sy);
  const Vec2 goal = centreOf(c.gx, c.gy);
  closest = 1e9f;
  for (float t = 0; t < limit; t += SIM_DT) {
    ClimbOps::navStep(g, a, goal, a.speed * 1.2f, SIM_DT);
    const float d = len(goal - a.p) / T;
    closest = std::min(closest, d);
    if (d <= 1.0f) return t;
  }
  return -1.0f;
}

// ---- speed over a timed walk
struct Run { int x, y, dx, dy, n; };
bool findRun(const Map& m, uint8_t want, int ptx, int pty, Run& r) {
  // want: HEIGHT_SLOPE, HEIGHT_RAMP (earth), HEIGHT_RAMP|HEIGHT_STAIR (stairs), 0 flat. A straight run of 4+ such
  // tiles (3+ for a ramp), every one walkable with open tiles to its sides along the walk
  auto is = [&](int x, int y) {
    if (!walkOpen(m, x, y)) return false;
    const uint8_t b = m.heightBits(x, y);
    const uint8_t k = b & (Map::HEIGHT_SLOPE | Map::HEIGHT_RAMP | Map::HEIGHT_STAIR | Map::HEIGHT_CLIFF | Map::HEIGHT_FALL);
    if (want == 0) return k == 0 && !groundWater(m.at(x, y));
    return k == want;
  };
  const int need = want == 0 ? 6 : (want & Map::HEIGHT_RAMP) ? 3 : 4;
  long best = -1;
  for (int y = 8; y < m.h - 8; y++)
    for (int x = 8; x < m.w - 8; x++) {
      if (!is(x, y)) continue;
      for (int dir = 0; dir < 2; dir++) {
        const int dx = dir == 0 ? 1 : 0, dy = dir == 0 ? 0 : 1;
        if (is(x - dx, y - dy)) continue;   // the run's first tile
        int n = 0;
        while (n < 12 && is(x + dx * n, y + dy * n)) n++;
        if (n < need) continue;
        const long d2 = (long)(x - ptx) * (x - ptx) + (long)(y - pty) * (y - pty);
        if (best < 0 || d2 < best) { best = d2; r = Run{x, y, dx, dy, n}; }
      }
    }
  return best >= 0;
}

// the pace a mover keeps along a run once eased in (px/s): moveActor at `speed` from the first tile's centre to the
// last's, measured once the body has gone 10 px onto the run (the ease takes at most 8 px)
float paceOf(Game& g, Actor a, const Run& r, float speed) {
  const Vec2 dir((float)r.dx, (float)r.dy);
  const Vec2 startP = centreOf(r.x, r.y);
  a.p = startP;
  a.reliefK = 1.0f;
  const Vec2 endP = centreOf(r.x + r.dx * (r.n - 1), r.y + r.dy * (r.n - 1));
  float t = 0, measured = 0, dist = 0;
  while (t < 10.0f) {
    const Vec2 before = a.p;
    ClimbOps::moveActor(g, a, dir * (speed * SIM_DT));
    t += SIM_DT;
    if (dot(endP - a.p, dir) <= 0) break;
    if (dot(before - startP, dir) >= 10.0f) { dist += len(a.p - before); measured += SIM_DT; }
  }
  return measured > 0.05f ? dist / measured : -1.0f;
}

// the player's own pace along the run through Game::update and the stick
float playerPace(Game& g, Run r) {
  // (the window recentres on the player: stand there first, then read the run's tiles in the moved window)
  const int32_t rgx = g.world.ox + r.x, rgy = g.world.oy + r.y;
  g.teleportGlobal(rgx, rgy);
  r.x = rgx - g.world.ox; r.y = rgy - g.world.oy;
  const Vec2 dir((float)r.dx, (float)r.dy);
  const Vec2 startP = centreOf(r.x, r.y);
  g.actors[0].p = startP;
  g.actors[0].reliefK = 1.0f;
  g.actors[0].st = AState::Idle;
  const Vec2 endP = centreOf(r.x + r.dx * (r.n - 1), r.y + r.dy * (r.n - 1));
  Input in;
  in.move = dir;
  float t = 0, measured = 0, dist = 0;
  const int key0 = g.mapKey();
  const int32_t ox = g.world.ox, oy = g.world.oy;
  while (t < 10.0f) {
    const Vec2 before = g.actors[0].p;
    g.update(SIM_DT, in);
    g.events.clear();
    if (g.mode != Mode::Play) g.mode = Mode::Play;
    if (g.mapKey() != key0 || g.world.ox != ox || g.world.oy != oy) return -1.0f;   // (the window moved: no measure)
    const Vec2 now = g.actors[0].p;
    t += SIM_DT;
    if (dot(endP - now, dir) <= 0) break;
    if (dot(before - startP, dir) >= 10.0f) { dist += len(now - before); measured += SIM_DT; }
  }
  return measured > 0.05f ? dist / measured : -1.0f;
}

void speedChecks(Game& g, bool& done) {
  const Map& m = g.world.over;
  const int ptx = tileXOf(g.pl().p), pty = tileYOf(g.pl().p);
  struct K { const char* name; uint8_t bits; float want; };
  const K kinds[3] = {{"hillside", Map::HEIGHT_SLOPE, 0.8f}, {"earth ramp", Map::HEIGHT_RAMP, 0.8f}, {"stone stairs", (uint8_t)(Map::HEIGHT_RAMP | Map::HEIGHT_STAIR), 0.9f}};
  Run flat;
  if (!findRun(m, 0, ptx, pty, flat)) return;
  Actor npc;
  npc.id = 900001; npc.npc = true; npc.human = true; npc.speed = 46; npc.radius = 4.5f;
  Actor comp = npc;
  comp.id = 900002; comp.speed = 44; comp.comp = 1;
  Actor horse = npc;
  horse.id = 900003; horse.human = false; horse.critter = 1; horse.speed = 128; horse.radius = 5.0f;
  Actor hound = npc;
  hound.id = 900004; hound.human = false; hound.speed = 60; hound.radius = 4.0f;
  struct Mover { const char* name; Actor* a; float speed; };
  Mover movers[4] = {{"an NPC", &npc, 46}, {"a companion", &comp, 66}, {"a mount", &horse, 128}, {"the hound", &hound, 72}};
  for (const K& k : kinds) {
    Run r;
    if (!findRun(m, k.bits, ptx, pty, r)) continue;
    done = true;
    for (const Mover& mv : movers) {
      const float base = paceOf(g, *mv.a, flat, mv.speed), on = paceOf(g, *mv.a, r, mv.speed);
      if (base <= 0 || on <= 0) { CLIMB_FAIL("%s's timed walk on %s could not be measured\n", mv.name, k.name); continue; }
      const float ratio = on / base;
      out("climb: speed %-11s %-12s %.1f / %.1f px/s = %.3f (want %.2f)\n", mv.name, k.name, on, base, ratio, k.want);
      if (std::fabs(ratio / k.want - 1.0f) > 0.03f) CLIMB_FAIL("%s walks %s at %.3f of flat (Map::reliefSpeed %.2f, 3 %%)\n", mv.name, k.name, ratio, k.want);
    }
    // the player through the stick (Game::update); the window comes back to where it was after
    const int32_t kx = g.world.ox + tileXOf(g.pl().p), ky = g.world.oy + tileYOf(g.pl().p);
    const int32_t fgx = g.world.ox + flat.x, fgy = g.world.oy + flat.y, rgx = g.world.ox + r.x, rgy = g.world.oy + r.y;
    const float base = playerPace(g, flat);
    Run rr = r;
    rr.x = rgx - g.world.ox; rr.y = rgy - g.world.oy;
    const float on = playerPace(g, rr);
    g.teleportGlobal(kx, ky);
    flat.x = fgx - g.world.ox; flat.y = fgy - g.world.oy;
    (void)rgy;
    if (base > 0 && on > 0) {
      const float ratio = on / base;
      out("climb: speed %-11s %-12s %.1f / %.1f px/s = %.3f (want %.2f)\n", "the player", k.name, on, base, ratio, k.want);
      if (std::fabs(ratio / k.want - 1.0f) > 0.03f) CLIMB_FAIL("the player walks %s at %.3f of flat (Map::reliefSpeed %.2f, 3 %%)\n", k.name, ratio, k.want);
    } else
      CLIMB_FAIL("the player's timed walk on %s could not be measured\n", k.name);
  }
  // easing: stepping from flat onto a hillside or ramp never changes the pace by more than a fifth of the gap at once
  {
    Run r;
    if (findRun(m, Map::HEIGHT_SLOPE, ptx, pty, r) || findRun(m, Map::HEIGHT_RAMP, ptx, pty, r)) {
      Actor a = npc;
      const Vec2 dir((float)r.dx, (float)r.dy);
      a.p = centreOf(r.x, r.y) - dir * 16.0f;   // a tile before the run
      a.reliefK = 1.0f;
      float worst = 0, prevK = 1.0f, t = 0;
      int frames = 0;
      bool reached = false;
      while (t < 2.0f) {
        ClimbOps::moveActor(g, a, dir * (46.0f * SIM_DT));
        t += SIM_DT;
        worst = std::max(worst, std::fabs(a.reliefK - prevK));
        if (a.reliefK < 0.99f && !reached) { reached = true; frames = 0; }
        if (reached) frames++;
        prevK = a.reliefK;
        if (a.reliefK <= 0.801f && reached) break;
      }
      const float easeS = frames * SIM_DT;
      out("climb: ease onto the relief over %.2f s (largest step %.3f)\n", easeS, worst);
      if (worst > 0.05f) CLIMB_FAIL("the relief pace snaps (a step of %.3f in one frame)\n", worst);
      if (reached && (easeS < 0.08f || easeS > 0.3f)) CLIMB_FAIL("the relief pace eases over %.2f s (about 0.15 s)\n", easeS);
    }
  }
  // the sure-footed breed is exempt (home.h BreedInfo::sure); any other horse takes the factor
  {
    Run r;
    if (findRun(m, Map::HEIGHT_RAMP, ptx, pty, r)) {
      const Vec2 keep = g.pl().p;
      const int riding = g.home.riding;
      const uint8_t breed = g.home.ridingBreed;
      g.actors[0].p = centreOf(r.x, r.y);
      for (int b = 0; b < (int)home::Breed::COUNT; b++) {
        g.home.riding = 0;
        g.home.ridingBreed = (uint8_t)b;
        const float k = ClimbOps::reliefTarget(g, g.pl());
        const bool sure = home::breedInfo((home::Breed)b).sure;
        if (sure ? k != 1.0f : std::fabs(k - 0.8f) > 0.001f) CLIMB_FAIL("the %s on an earth ramp asks %.2f (sure-footed: 1, else 0.8)\n", home::breedInfo((home::Breed)b).name, k);
      }
      g.home.riding = riding;
      g.home.ridingBreed = breed;
      g.actors[0].p = keep;
    }
  }
}

// ---- falls are walls
bool fallChecks(Game& g, uint64_t seed, bool last) {
  Map& m = g.world.over;
  static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
  int fx = -1, fy = -1, wx = -1, wy = -1;
  bool made = false;
  const int ptx = tileXOf(g.pl().p), pty = tileYOf(g.pl().p);
  long best = -1;
  for (int y = 4; y < m.h - 4; y++)
    for (int x = 4; x < m.w - 4; x++) {
      if (!m.fallAt(x, y)) continue;
      for (int k = 0; k < 4; k++) {
        const int ax = x + dx[k], ay = y + dy[k];
        if (!groundWater(m.at(ax, ay)) || m.fallAt(ax, ay) || m.solid[(size_t)ay * m.w + ax]) continue;
        const long d2 = (long)(x - ptx) * (x - ptx) + (long)(y - pty) * (y - pty);
        if (best < 0 || d2 < best) { best = d2; fx = x; fy = y; wx = ax; wy = ay; }
      }
    }
  uint8_t keepBits = 0;
  if (fx < 0) {   // no real fall here: a river tile made one for the check (restored after)
    for (int y = 4; y < m.h - 4 && fx < 0; y++)
      for (int x = 4; x < m.w - 4 && fx < 0; x++) {
        if (!groundWater(m.at(x, y)) || m.solid[(size_t)y * m.w + x]) continue;
        if (!groundWater(m.at(x, y + 1)) || m.solid[(size_t)(y + 1) * m.w + x]) continue;
        fx = x; fy = y; wx = x; wy = y + 1;
      }
    if (fx < 0) {
      if (last) CLIMB_FAIL("seed %llu: no water in the windows for the falls check\n", (unsigned long long)seed);
      return false;
    }
    keepBits = m.height[(size_t)fy * m.w + fx];
    m.height[(size_t)fy * m.w + fx] = (uint8_t)(keepBits | Map::HEIGHT_FALL | Map::HEIGHT_CLIFF);
    made = true;
  }
  const Vec2 fallC = centreOf(fx, fy), from = centreOf(wx, wy);
  const Vec2 push = norm(fallC - from);
  auto pushed = [&](Actor& a, const char* who) {
    a.p = from;
    for (float t = 0; t < 2.5f; t += SIM_DT) ClimbOps::moveActor(g, a, push * (60.0f * SIM_DT));
    const float rx = a.radius, ry = a.radius * 0.6f;
    bool in = false;
    for (int c = 0; c < 4; c++) {
      const float px = a.p.x + (c & 1 ? rx : -rx), py = a.p.y + (c & 2 ? ry : -ry);
      in |= (int)std::floor(px / T) == fx && (int)std::floor(py / T) == fy;
    }
    if (in) CLIMB_FAIL("seed %llu: %s pushed at a waterfall went into it (%.1f, %.1f)\n", (unsigned long long)seed, who, a.p.x / T, a.p.y / T);
  };
  Actor sw;
  sw.id = 900010; sw.npc = true; sw.swim = 2; sw.radius = 4.0f; sw.speed = 40;
  if (ClimbOps::bodyFree(g, from, sw.radius, false, 2)) pushed(sw, "a swimmer");
  // the hero aboard a rowboat (SeaOps sets swim 1), and the hero wading (SeaOps::passable)
  Actor& p = g.actors[0];
  const Vec2 keepP = p.p;
  const uint8_t keepSwim = p.swim;
  p.swim = 1;
  if (ClimbOps::bodyFree(g, from, p.radius, false, 1)) pushed(p, "a rowboat");
  p.swim = 0;
  pushed(p, "the wading hero");
  p.swim = keepSwim;
  p.p = keepP;
  out("climb: seed %llu falls: %s fall at global (%d,%d), pushed from (%d,%d)\n", (unsigned long long)seed, made ? "a made" : "a real", fx + g.world.ox,
      fy + g.world.oy, wx + g.world.ox, wy + g.world.oy);
  if (made) m.height[(size_t)fy * m.w + fx] = keepBits;
  return true;
}

// a companion left on the low side reaches the player standing on the high side (the party's follow code)
bool companionCheck(Game& g, const Crossing& c, uint64_t seed, int k) {
  if (g.party.members.empty()) {
    const int mi = g.partyRecruitStranger("CLIMBER", false, g.world.sites[(size_t)g.world.startSite].culture, 4242, party::Origin::Saga, g.pl().p + Vec2(16, 0));
    if (mi < 0) { CLIMB_FAIL("seed %llu: no companion could be recruited\n", (unsigned long long)seed); return false; }
    run(g, 0.2f);
  }
  int ai = -1;
  for (size_t i = 1; i < g.actors.size(); i++) if (g.actors[i].comp) { ai = (int)i; break; }
  if (ai < 0) { CLIMB_FAIL("seed %llu: the companion has no body\n", (unsigned long long)seed); return false; }
  g.actors[0].p = centreOf(c.gx, c.gy);
  g.actors[0].st = AState::Idle;
  g.actors[(size_t)ai].p = centreOf(c.sx, c.sy);
  g.actors[(size_t)ai].navNext = -1;
  g.party.trail.clear();
  for (party::Member& m : g.party.members) m.trailAt = -1;
  const int catch0 = g.party.stats.catchUps;
  const int lvl = g.world.over.heightAt(c.gx, c.gy);
  float took = -1;
  for (float t = 0; t < 30.0f; t += SIM_DT) {
    g.update(SIM_DT, Input());
    g.events.clear();
    if (g.mode != Mode::Play) g.mode = Mode::Play;
    ai = -1;
    for (size_t i = 1; i < g.actors.size(); i++) if (g.actors[i].comp) { ai = (int)i; break; }
    if (ai < 0) break;
    const Actor& a = g.actors[(size_t)ai];
    if (len(a.p - g.pl().p) < 3.0f * T && g.world.over.heightAt(tileXOf(a.p), tileYOf(a.p)) >= lvl) { took = t; break; }
  }
  const bool ok = took >= 0 && g.party.stats.catchUps == catch0;
  if (!ok)
    CLIMB_FAIL("seed %llu: crossing %d (%d,%d): the companion did not reach the player on the high side in 30 s (catch-ups %d)\n",
               (unsigned long long)seed, k, c.fx, c.fy, g.party.stats.catchUps - catch0);
  else
    out("climb: seed %llu companion crossing %d reached the player in %.1f s (walk %d tiles)\n", (unsigned long long)seed, k, took, c.walk);
  return ok;
}
}  // namespace

static int climbCmd(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  int want = 12;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!std::strcmp(argv[i], "--crossings") && i + 1 < argc) want = std::max(1, std::atoi(argv[++i]));
  }
  c_bad = 0;
  // ---- the contract
  auto chk = [&](bool ok, const char* what) { if (!ok) CLIMB_FAIL("%s\n", what); };
  chk(Map::reliefSpeed(0) == 1.0f, "flat ground walks at 1");
  chk(Map::reliefSpeed(Map::HEIGHT_SLOPE | 2) == 0.8f, "a hillside walks at 0.8");
  chk(Map::reliefSpeed(Map::HEIGHT_RAMP | 3) == 0.8f, "an earth ramp walks at 0.8");
  chk(Map::reliefSpeed(Map::HEIGHT_RAMP | Map::HEIGHT_STAIR | 3) == 0.9f, "stone stairs walk at 0.9");
  chk((Map::HEIGHT_STEP & Map::HEIGHT_FALL) == 0 && (Map::HEIGHT_STEP & Map::HEIGHT_STAIR) == 0, "the step mask is cliff | ramp | slope");
  int crossTotal = 0, crossOk = 0, hardTotal = 0, hardOk = 0, compTotal = 0, compOk = 0;
  bool speedDone = false;
  double worstMs = 0;
  int worstNodes = 0;
  for (uint64_t s = a; s <= b; s++) {
    g_curSeed = s;
    Game g = makeGame(s);
    // the windows: the start's, then the ranges and peaks nearest it
    std::vector<std::pair<int32_t, int32_t>> centres;
    centres.push_back({g.world.ox + tileXOf(g.pl().p), g.world.oy + tileYOf(g.pl().p)});
    {
      ew::EndlessSource& S = *g.world.src;
      std::vector<std::pair<int64_t, std::pair<int32_t, int32_t>>> highs;
      const int32_t rx0 = ew::regionOf(centres[0].first), ry0 = ew::regionOf(centres[0].second);
      for (int r = 0; r <= 6 && highs.size() < 12; r++)
        for (int32_t ry = ry0 - r; ry <= ry0 + r; ry++)
          for (int32_t rx = rx0 - r; rx <= rx0 + r; rx++) {
            if (std::max(std::abs(rx - rx0), std::abs(ry - ry0)) != r) continue;
            for (const ew::LandmarkPlan& L : S.region(rx, ry).landmarks)
              if (L.kind == ew::LandmarkKind::Range || L.kind == ew::LandmarkKind::Peak || L.kind == ew::LandmarkKind::Hills) {
                const int64_t dx = L.x - centres[0].first, dy = L.y - centres[0].second;
                highs.push_back({dx * dx + dy * dy, {L.x, L.y}});
              }
          }
      std::sort(highs.begin(), highs.end());
      for (const auto& h : highs) {
        bool near = false;
        for (const auto& c : centres) near |= std::abs(c.first - h.second.first) < 160 && std::abs(c.second - h.second.second) < 160;
        if (!near && centres.size() < 6) centres.push_back(h.second);
      }
    }
    int sCross = 0, sCrossOk = 0, sHard = 0, sHardOk = 0, sComp = 0, sCompOk = 0, sTrek = 0, sTrekOk = 0, sStack = 0, sStackOk = 0;
    bool fallsDone = false;
    for (size_t w = 0; w < centres.size() && (sCross < want || sHard < 1 || sComp < 2 || sTrek < 1 || !fallsDone); w++) {
      if (w > 0) { g.teleportGlobal(centres[w].first, centres[w].second); run(g, 0.3f); }
      if (!speedDone) speedChecks(g, speedDone);   // (first: its player walks move the window)
      const Map& m = g.world.over;
      std::vector<Crossing> easy, hard;
      findCrossings(m, tileXOf(g.pl().p), tileYOf(g.pl().p), easy, hard);
      if (!fallsDone) fallsDone = fallChecks(g, s, w + 1 == centres.size());
      g.navStats = Game::NavStats();
      for (const Crossing& c : easy) {
        if (sCross >= want) break;
        float closest = 0;
        const float took = sendGuard(g, c, 25.0f, 800000 + sCross, closest);
        sCross++;
        if (m.heightAt(c.fx + c.ux, c.fy + c.uy) >= m.heightAt(c.fx, c.fy) + 2) { sStack++; if (took >= 0) sStackOk++; }
        if (took >= 0) sCrossOk++;
        else
          CLIMB_FAIL("seed %llu: guard crossing %d (%d,%d) up %+d,%+d (way up %d along, walk %d tiles) did not arrive in 25 s (closest %.1f tiles)\n",
                     (unsigned long long)s, sCross, c.fx, c.fy, c.ux, c.uy, c.along, c.walk, closest);
      }
      for (const Crossing& c : hard) {
        if (sHard >= 2) break;
        float closest = 0;
        const float took = sendGuard(g, c, 25.0f, 810000 + sHard, closest);
        sHard++;
        if (took >= 0) { sHardOk++; out("climb: seed %llu far way up (%d along, walk %d tiles) reached in %.1f s\n", (unsigned long long)s, c.along, c.walk, took); }
        else
          CLIMB_FAIL("seed %llu: goal across a cliff at (%d,%d), way up %d tiles along (walk %d), not reached in 25 s (closest %.1f tiles)\n",
                     (unsigned long long)s, c.fx, c.fy, c.along, c.walk, closest);
      }
      // a long trek over the relief (a goal 82+ tiles off, past the short search's box): the long search's own path,
      // walked from its cache, and the A* alone agrees there is a way
      for (size_t i = 0; i < easy.size() && sTrek < 1; i++)
        for (size_t j = 0; j < easy.size() && sTrek < 1; j++) {
          const Crossing &c = easy[i], &e = easy[j];
          if (std::max(std::abs(c.sx - e.gx), std::abs(c.sy - e.gy)) < 82) continue;   // (past the short search's 112-tile box)
          const int walk = walkDist(m, c.sx, c.sy, e.gx, e.gy, 180);
          if (walk < 0 || walk > 170) continue;
          Crossing t = c;
          t.gx = e.gx; t.gy = e.gy; t.walk = walk;
          const float limit = walk * T / (46.0f * 1.2f * 0.85f) + 8.0f;
          float closest = 0;
          const int long0 = g.navStats.longFound;
          const float took = sendGuard(g, t, limit, 820000 + (int)i, closest);
          sTrek++;
          if (took >= 0 && g.navStats.longFound > long0) {
            sTrekOk++;
            out("climb: seed %llu trek of %d tiles walked in %.1f s (limit %.1f) by the long search\n", (unsigned long long)s, walk, took, limit);
          } else
            CLIMB_FAIL("seed %llu: trek (%d,%d) -> (%d,%d), %d tiles: %s (closest %.1f tiles)\n", (unsigned long long)s, t.sx, t.sy, t.gx, t.gy, walk,
                       took < 0 ? "not reached in time" : "the long search never ran", closest);
        }
      for (const Crossing& c : hard) {   // the A* alone over each far way up
        std::vector<int32_t> way;
        int nodes = 0;
        Actor probe;
        probe.id = 830000; probe.npc = true; probe.human = true;
        if (!ClimbOps::longSearch(g, probe, c.sx, c.sy, c.gx, c.gy, way, nodes) || way.size() < 2 || way[way.size() - 2] != c.gx || way.back() != c.gy)
          CLIMB_FAIL("seed %llu: the long search finds no way across the cliff at (%d,%d) (way up %d along)\n", (unsigned long long)s, c.fx, c.fy, c.along);
        else if ((int)way.size() / 2 > c.walk * 13 / 10 + 2)
          CLIMB_FAIL("seed %llu: the long search's way across (%d,%d) is %d tiles (the shortest %d)\n", (unsigned long long)s, c.fx, c.fy, (int)way.size() / 2, c.walk);
        worstNodes = std::max(worstNodes, nodes);
      }
      worstMs = std::max(worstMs, g.navStats.worstMs);
      worstNodes = std::max(worstNodes, g.navStats.worstNodes);
      out("climb: seed %llu window %d: %d easy %d far crossings found; paths %d requests, short %d/%d, long %d (found %d, failed %d), nodes worst %d, worst %.2f ms\n",
          (unsigned long long)s, (int)w, (int)easy.size(), (int)hard.size(), g.navStats.requests, g.navStats.shortFound, g.navStats.shortFound + g.navStats.shortFails,
          g.navStats.longSearches, g.navStats.longFound, g.navStats.longFails, g.navStats.worstNodes, g.navStats.worstMs);
      for (size_t k = 0; k < easy.size() && sComp < 2; k++) {
        if (easy[k].walk > 40) continue;
        sComp++;
        if (companionCheck(g, easy[k], s, (int)k)) sCompOk++;
      }
    }
    // ---- the climb to a summit shrine (15.32 SPF_SUMMIT: a trail of stairs and switchbacks up stacked levels): a guard
    // sent from the lowest open ground within 40 tiles of the shrine that a walk reaches, to the shrine's door
    {
      ew::EndlessSource& S = *g.world.src;
      const int32_t px = g.world.ox + tileXOf(g.pl().p), py = g.world.oy + tileYOf(g.pl().p);
      bool have = false;
      ew::SitePlan best;
      double bd = 1e18;
      for (int32_t ry = ew::regionOf(py) - 6; ry <= ew::regionOf(py) + 6; ry++)
        for (int32_t rx = ew::regionOf(px) - 6; rx <= ew::regionOf(px) + 6; rx++)
          for (const ew::SitePlan& sp : S.region(rx, ry).sites)
            if (sp.flags & ew::SPF_SUMMIT) {
              const double d = std::hypot((double)(sp.ex - px), (double)(sp.ey - py));
              if (d < bd || (d == bd && sp.id < best.id)) { bd = d; best = sp; have = true; }
            }
      if (!have) out("climb: seed %llu: no summit shrine within 6 regions (WORLD lane: SPF_SUMMIT)\n", (unsigned long long)s);
      else {
        g.teleportGlobal(best.ex, best.ey + 2);
        run(g, 0.2f);
        const Map& m = g.world.over;
        const int gx = best.ex - g.world.ox, gy = best.ey + 1 - g.world.oy;
        int sx = -1, sy = -1, walk = -1, low = 99;
        for (int r = 12; r <= 40; r += 4)   // the lowest reachable ground on rings out from the shrine
          for (int a = 0; a < 32; a++) {
            const int x = gx + (int)std::lround(std::cos(a * 0.19635) * r), y = gy + (int)std::lround(std::sin(a * 0.19635) * r);
            if (!walkOpen(m, x, y) || (m.heightBits(x, y) & Map::HEIGHT_STEP) || m.heightAt(x, y) >= low) continue;
            const int w = walkDist(m, x, y, gx, gy, 220);
            if (w < 0) continue;
            low = m.heightAt(x, y); sx = x; sy = y; walk = w;
          }
        if (sx < 0 || !walkOpen(m, gx, gy)) CLIMB_FAIL("seed %llu: no walkable way to the summit shrine %s at (%d,%d)\n", (unsigned long long)s, best.name.c_str(), best.ex, best.ey);
        else {
          Crossing t{gx, gy, 0, -1, sx, sy, gx, gy, 0, walk};
          const float limit = walk * T / (46.0f * 1.2f * 0.8f) + 10.0f;
          float closest = 0;
          const float took = sendGuard(g, t, limit, 840000, closest);
          if (took < 0)
            CLIMB_FAIL("seed %llu: the climb to the summit shrine %s (level %d to %d, %d tiles) not made in %.0f s (closest %.1f tiles)\n", (unsigned long long)s,
                       best.name.c_str(), low, m.heightAt(gx, gy), walk, limit, closest);
          else
            out("climb: seed %llu summit shrine %s: climbed level %d to %d, %d tiles, in %.1f s\n", (unsigned long long)s, best.name.c_str(), low, m.heightAt(gx, gy), walk, took);
        }
      }
    }
    if (sCross < want) CLIMB_FAIL("seed %llu: only %d crossings found in %d windows (want %d)\n", (unsigned long long)s, sCross, (int)centres.size(), want);
    if (sHard < 1) out("climb: seed %llu: no cliff with its way up 16-24 tiles along in the windows (the lattice keeps them rare)\n", (unsigned long long)s);
    if (sTrek < 1) CLIMB_FAIL("seed %llu: no long trek found in the windows\n", (unsigned long long)s);
    if (sComp < 1) CLIMB_FAIL("seed %llu: no crossing for the companion check\n", (unsigned long long)s);
    out("climb: seed %llu: guards %d/%d crossings (%d/%d by stacked faces), far ways up %d/%d, treks %d/%d, companions %d/%d\n", (unsigned long long)s, sCrossOk,
        sCross, sStackOk, sStack, sHardOk, sHard, sTrekOk, sTrek, sCompOk, sComp);
    crossTotal += sCross; crossOk += sCrossOk; hardTotal += sHard; hardOk += sHardOk; compTotal += sComp; compOk += sCompOk;
  }
  if (!speedDone) CLIMB_FAIL("no hillside, ramp or stairs run found for the speed check\n");
  if (worstMs > 4.0) CLIMB_FAIL("the worst navStep took %.2f ms (budget 4)\n", worstMs);
  out("climb: guards %d/%d, far ways up %d/%d, companions %d/%d; worst search %d nodes, %.2f ms; %d failures\n", crossOk, crossTotal, hardOk, hardTotal,
      compOk, compTotal, worstNodes, worstMs, c_bad);
  return c_bad ? 1 : 0;
}
RPG_TEST_CMD("--climb", "M11: moving over the relief: the speed of hillsides, ramps and stairs, guards and companions pathing across ramps, falls as walls [--seeds A..B] [--crossings N]", climbCmd);
