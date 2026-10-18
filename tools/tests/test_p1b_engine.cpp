// rpg_test --p1b-engine [--seeds A..B] [--verbose]
// P1b "New Roads Content", ENGINE lane (VISION_PLAN 15.35): the shared mechanics measured in real game steps, with the
// game's own update loop and the engine's test scripts (rpg/story/lib_p1b_test.cpp, family test):
//   walk     an escort walked 300+ tiles by a path bot: the person within 4 tiles in >= 95 % of the steps (after they
//            first join), never stuck > 3 s, survives a building (in and out) and a fast travel, and the goal completes
//            only when both have arrived (_escort_far)
//   wagon    the cart trails its driver (a tow of ~21 px, its heading easing: no per-step jump over 0.35 rad)
//   drove    a drove of N beasts walks 100+ tiles and arrives with N, gathered round the drover
//   bout     won: the opponent yields at <= 25 % health and never dies, the goal is won; lost: the player is left on 1
//            HP (no death screen, no gold lost) and the goal goes to its else
//   hostile  `do hostile`: the person fights to the death and `goal slay` completes on them
//   calm     `do calm`: 60 s inside the cave beside its creatures without a blow
//   inside   a person `in` a cave, a ruin and a camp: there, reachable from the way in, talkable (talksTo)
//   burden   `give ... heavy`: the walk at x0.7 (0.65..0.75 of the free pace), back to x1 once taken
//   props    `goal use beacon in <tower>`: the use button by the tower lights it (beaconLit) and completes the goal
//   casting  watchtower / dwelling / with road / with makes / with flag cast from the start village; 2 watchtowers within
//            250 tiles of each other (a beacon chain) reported
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/sim/game_internal.h"
#include "rpg/sim/life.h"
#include "rpg/story/dsl.h"
#include "rpg/story/story.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/poi.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

using namespace story;
constexpr float T = 16.0f;
bool g_verb = false;
int g_bad = 0;

void fail(const std::string& s) { out("FAIL: p1b-engine %s\n", s.c_str()); g_bad++; }
std::string sd(uint64_t seed) { return "seed " + std::to_string(seed) + ": "; }

void steps(Game& g, int n, Input in = Input()) {
  for (int i = 0; i < n; i++) { g.update(SIM_DT, in); g.events.clear(); if (g.mode == Mode::Dialogue) g.mode = Mode::Play; }
}

int hookActor(Game& g, int site) {
  for (const Actor& a : g.actors) {
    if (!a.npc || !a.human || isStoryPerson(a) || a.st == AState::Dead || a.critter) continue;
    if (homeSiteOf(g, a) == site) return a.id;
  }
  return -1;
}
uint32_t begin(Game& g, const std::string& id, std::string& why) {
  const int site = g.world.startSite;
  return g.story.start(g, id, hookActor(g, site), site, &why);
}
bool jump(Game& g, uint32_t inst, const std::string& stage) {
  Instance* in = g.story.find(inst);
  const dsl::Script* s = in ? scriptOf(*in) : nullptr;
  if (!s || s->stage(stage) < 0) return false;
  g.story.enterStage(g, *in, s->stage(stage));
  return true;
}
int actorOf(const Game& g, uint32_t inst, const std::string& role) {
  const Instance* in = g.story.find(inst);
  const dsl::Script* s = in ? scriptOf(*in) : nullptr;
  if (!s) return -1;
  const int ri = s->role(role);
  if (ri < 0) return -1;
  for (size_t k = 1; k < g.actors.size(); k++)
    if (isStoryPerson(g.actors[k]) && g.actors[k].slot == personSlot(inst, ri)) return (int)k;
  return -1;
}
std::string stageOf(const Game& g, uint32_t inst) {
  const Instance* in = g.story.find(inst);
  return in ? g.story.stageName(*in) : std::string("(ended)");
}
bool endedOk(const Game& g, uint32_t inst) {
  const Instance* in = g.story.find(inst);
  if (in) return in->done && !in->failed;
  for (const Quest& q : g.quests) (void)q;
  return true;   // (finished stories leave the running list)
}

// ---- the overworld path bot (4-connected over open ground, global tiles)
std::vector<std::pair<int32_t, int32_t>> pathTo(Game& g, int32_t gx, int32_t gy) {
  std::vector<std::pair<int32_t, int32_t>> outp;
  const Map& M = g.world.over;
  const int sx = (int)std::floor(g.pl().p.x / T), sy = (int)std::floor(g.pl().p.y / T);
  const int tx = gx - g.world.ox, ty = gy - g.world.oy;
  if (!M.in(sx, sy) || !M.in(tx, ty)) return outp;
  std::vector<int> prev((size_t)M.w * M.h, -1);
  std::deque<int> q;
  const int s = sy * M.w + sx, t = ty * M.w + tx;
  prev[(size_t)s] = s;
  q.push_back(s);
  static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
  while (!q.empty()) {
    const int c = q.front();
    q.pop_front();
    if (c == t) break;
    const int cx = c % M.w, cy = c / M.w;
    for (int k = 0; k < 4; k++) {
      const int nx = cx + dx[k], ny = cy + dy[k];
      if (!M.in(nx, ny) || nx < 2 || ny < 2 || nx >= M.w - 2 || ny >= M.h - 2) continue;
      const int ni = ny * M.w + nx;
      if (prev[(size_t)ni] >= 0 || M.blocked(nx, ny) || M.bldgAt[(size_t)ni] >= 0) continue;
      if (M.at(nx, ny) == Ground::Swamp || M.at(nx, ny) == Ground::Ice) continue;
      prev[(size_t)ni] = c;
      q.push_back(ni);
    }
  }
  if (prev[(size_t)t] < 0) return outp;
  for (int c = t; c != s; c = prev[(size_t)c]) outp.push_back({g.world.ox + c % M.w, g.world.oy + c / M.w});
  std::reverse(outp.begin(), outp.end());
  return outp;
}
// a reachable tile far by path within r tiles, preferring the way away from where the walk began (no back and forth)
bool farTile(Game& g, int r, int32_t& gx, int32_t& gy, int32_t awayX, int32_t awayY) {
  const Map& M = g.world.over;
  const int sx = (int)std::floor(g.pl().p.x / T), sy = (int)std::floor(g.pl().p.y / T);
  std::vector<int> dist((size_t)M.w * M.h, -1);
  std::deque<int> q;
  dist[(size_t)(sy * M.w + sx)] = 0;
  q.push_back(sy * M.w + sx);
  int best = -1;
  int64_t bs = -1;
  static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
  while (!q.empty()) {
    const int c = q.front();
    q.pop_front();
    const int cx = c % M.w, cy = c / M.w;
    const int64_t ax = g.world.ox + cx - awayX, ay = g.world.oy + cy - awayY;
    const int64_t sc = (int64_t)dist[(size_t)c] * 4 + (int64_t)std::sqrt((double)(ax * ax + ay * ay)) * 3;
    if (sc > bs && std::abs(cx - sx) <= r && std::abs(cy - sy) <= r) { bs = sc; best = c; }
    for (int k = 0; k < 4; k++) {
      const int nx = cx + dx[k], ny = cy + dy[k];
      if (!M.in(nx, ny) || nx < 2 || ny < 2 || nx >= M.w - 2 || ny >= M.h - 2 || std::abs(nx - sx) > r || std::abs(ny - sy) > r) continue;
      const int ni = ny * M.w + nx;
      if (dist[(size_t)ni] >= 0 || M.blocked(nx, ny) || M.bldgAt[(size_t)ni] >= 0) continue;
      if (M.at(nx, ny) == Ground::Swamp || M.at(nx, ny) == Ground::Ice) continue;
      dist[(size_t)ni] = dist[(size_t)c] + 1;
      q.push_back(ni);
    }
  }
  if (best < 0) return false;
  gx = g.world.ox + best % M.w; gy = g.world.oy + best / M.w;
  return true;
}

struct WalkStats {
  int frames = 0, counted = 0, near = 0, tiles = 0, missing = 0, far4to6 = 0, far6up = 0, farIdle = 0;
  float worstStuck = 0, stuck = 0, maxCartJump = 0, maxD = 0;
  bool joined = false;
};

// walk the bot `wantTiles` tiles in legs; per step: where the follower actor (inst/role) is
void walkWith(Game& g, uint32_t inst, const std::string& role, int wantTiles, WalkStats& W, bool wagon) {
  const int32_t ax = g.world.ox + (int32_t)std::floor(g.pl().p.x / T), ay = g.world.oy + (int32_t)std::floor(g.pl().p.y / T);
  float lastAng = 1e9f;
  Vec2 lastF, lastCart;
  bool haveLast = false;
  for (int leg = 0; leg < 14 && W.tiles < wantTiles; leg++) {
    int32_t tx = 0, ty = 0;
    if (!farTile(g, 60, tx, ty, ax, ay)) break;
    const auto path = pathTo(g, tx, ty);
    if (path.size() < 8) break;
    size_t k = 0;
    int px = (int)std::floor(g.pl().p.x / T) + g.world.ox, py = (int)std::floor(g.pl().p.y / T) + g.world.oy;
    for (int f = 0; f < 60 * 90 && k < path.size(); f++) {
      const Vec2 c((path[k].first - g.world.ox) * T + 8.0f, (path[k].second - g.world.oy) * T + 10.0f);
      const Vec2 d = c - g.pl().p;
      if (len(d) < 5.0f) { k++; continue; }
      Input in;
      in.move = norm(d);
      g.update(SIM_DT, in);
      g.events.clear();
      if (g.mode != Mode::Play) g.mode = Mode::Play;
      if (g.inside) { g.debugLeave(); haveLast = false; break; }   // (the bot stepped into a doorway or a cave mouth: out, next leg)
      W.frames++;
      const int nx = (int)std::floor(g.pl().p.x / T) + g.world.ox, ny = (int)std::floor(g.pl().p.y / T) + g.world.oy;
      if (nx != px || ny != py) { W.tiles += std::abs(nx - px) + std::abs(ny - py); px = nx; py = ny; }
      const int ai = actorOf(g, inst, role);
      if (ai < 0) { if (W.joined) { W.counted++; W.missing++; } haveLast = false; continue; }
      const Actor& a = g.actors[(size_t)ai];
      const float dd = len(a.p - g.pl().p);
      if (!W.joined && dd <= 4.0f * T) W.joined = true;
      if (!W.joined) continue;
      W.counted++;
      if (dd <= 4.0f * T) W.near++;
      else if (dd <= 6.0f * T) W.far4to6++;
      else {
        W.far6up++;
        if (g_verb && W.far6up % 40 == 1) {
          const Engine::Walker* w = g.story.walkerOf(a.id);
          out("p1b-engine:   far %.1f tiles: player (%d,%d) st %d, follower (%d,%d) st %d spd %.0f stuck %.1f, trail %zu, inside %d\n", dd / T,
              (int)(g.pl().p.x / T), (int)(g.pl().p.y / T), (int)g.pl().st, (int)(a.p.x / T), (int)(a.p.y / T), (int)a.st, w ? w->spd : -1.0f,
              w ? w->stuckT : -1.0f, g.party.trail.size(), (int)g.inside);
        }
      }
      if (dd > 4.0f * T && a.st != AState::Walk) W.farIdle++;
      W.maxD = std::max(W.maxD, dd / T);
      // stuck: left behind and not moving
      const Vec2 gv = haveLast ? a.p - lastF : Vec2(1, 0);
      if (dd > 3.0f * T && len(gv) < 0.05f) W.stuck += SIM_DT; else W.stuck = 0;
      W.worstStuck = std::max(W.worstStuck, W.stuck);
      lastF = a.p;
      haveLast = true;
      if (wagon) {
        const Engine::Walker* w = g.story.walkerOf(a.id);
        if (w && w->cartSet) {
          const Vec2 cp(w->cartX, w->cartY);
          if (len(cp - lastCart) > 24.0f) lastAng = 1e9f;   // (the cart set afresh behind its driver: not a swing)
          lastCart = cp;
          if (lastAng < 1e8f) {
            float da = std::fabs(w->cartAng - lastAng);
            if (da > 3.14159f) da = 6.28318f - da;
            W.maxCartJump = std::max(W.maxCartJump, da);
          }
          lastAng = w->cartAng;
        }
      }
    }
  }
}

// ---------------------------------------------------------------- the checks
void walkCheck(Game& g, uint64_t seed) {
  std::string why;
  const uint32_t inst = begin(g, "p1b_engine_walk", why);
  if (!inst) { fail(sd(seed) + "walk: cannot begin: " + why); return; }
  jump(g, inst, "road");
  steps(g, 30);
  WalkStats W;
  walkWith(g, inst, "walker", 320, W, false);
  const double pct = W.counted ? 100.0 * W.near / W.counted : 0.0;
  out("p1b-engine: %swalk %d tiles, %d steps counted, follower within 4 tiles %.1f %%, worst stuck %.2f s\n", sd(seed).c_str(), W.tiles, W.counted, pct, W.worstStuck);
  if (g_verb) out("p1b-engine: %s  off 4-6 tiles %d steps, 6+ %d, missing %d, standing while off %d, farthest %.1f tiles\n", sd(seed).c_str(), W.far4to6, W.far6up, W.missing, W.farIdle, W.maxD);
  if (W.tiles < 300) fail(sd(seed) + "walk: the bot walked only " + std::to_string(W.tiles) + " tiles");
  if (!W.joined) fail(sd(seed) + "walk: the follower never joined the player");
  else if (pct < 95.0) fail(sd(seed) + "walk: the follower was within 4 tiles in only " + std::to_string(pct) + " % of the steps");
  if (W.worstStuck > 3.0f) fail(sd(seed) + "walk: the follower stood stuck for " + std::to_string(W.worstStuck) + " s");
  // a building: in and out, the follower beside the player again
  int bi = -1;
  float bd = 1e30f;
  for (size_t k = 0; k < g.world.over.bldgs.size(); k++) {
    const Bldg& B = g.world.over.bldgs[k];
    const float d = len2(Vec2(B.doorX() * T, B.doorY() * T) - g.pl().p);
    if (d < bd) { bd = d; bi = (int)k; }
  }
  if (bi >= 0 && g.debugEnterBuilding(bi, 0)) {
    steps(g, 20);
    g.debugLeave();
    for (int f = 0; f < 60; f++) {
      steps(g, 1);
      if (g_verb) {
        int n = 0;
        for (const Actor& o : g.actors) n += isStoryPerson(o) ? 1 : 0;
        out("p1b-engine: %s  frame %d after leaving: %d story persons, %zu actors, inside %d, mode %d\n", sd(seed).c_str(), f, n, g.actors.size(), (int)g.inside, (int)g.mode);
      }
    }
    const int ai = actorOf(g, inst, "walker");
    const float d = ai >= 0 ? len(g.actors[(size_t)ai].p - g.pl().p) / T : 999.0f;
    if (ai < 0) {
      const Instance* i3 = g.story.find(inst);
      out("p1b-engine: %s  after the building: inside %d, stage %s, walkers %zu, actors %zu\n", sd(seed).c_str(), (int)g.inside,
          stageOf(g, inst).c_str(), g.story.walkers_.size(), g.actors.size());
      for (const Actor& o : g.actors)
        if (isStoryPerson(o)) out("p1b-engine: %s    story person %s slot %d (want %d) st %d at %.1f tiles\n", sd(seed).c_str(), o.name.c_str(), o.slot,
                                  i3 ? personSlot(inst, scriptOf(*i3)->role("walker")) : -1, (int)o.st, len(o.p - g.pl().p) / T);
      if (i3) for (const auto& kv : i3->vars) out("p1b-engine: %s    var %s = %d\n", sd(seed).c_str(), kv.first.c_str(), kv.second);
    }
    if (d > 6.0f) fail(sd(seed) + "walk: after a building the follower is " + std::to_string(d) + " tiles off");
    else if (g_verb) out("p1b-engine: %safter a building: %.1f tiles\n", sd(seed).c_str(), d);
  }
  // the destination by fast travel: the follower set down beside the player, and the goal completes (both arrived)
  Instance* in = g.story.find(inst);
  const Binding* to = in ? bindingOf(*in, "far") : nullptr;
  const int h = to ? siteByIdLoad(g, to->site) : -1;
  if (h < 0) { fail(sd(seed) + "walk: no destination site"); return; }
  g.world.sites[(size_t)h].discovered = true;
  if (!g.fastTravel(h)) { fail(sd(seed) + "walk: no fast travel to the destination"); return; }
  bool done = false;
  float firstNear = -1;
  for (int f = 0; f < 60 * 8 && !done; f++) {
    steps(g, 1);
    const int ai = actorOf(g, inst, "walker");
    if (firstNear < 0 && ai >= 0 && len(g.actors[(size_t)ai].p - g.pl().p) < 6.0f * T) firstNear = f * SIM_DT;
    const Instance* i2 = g.story.find(inst);
    done = !i2 || i2->done;
  }
  out("p1b-engine: %sfast travel: follower beside the player after %.2f s, goal %s\n", sd(seed).c_str(), firstNear, done ? "done" : "NOT done");
  if (firstNear < 0) fail(sd(seed) + "walk: after a fast travel the follower never came back beside the player");
  if (!done) fail(sd(seed) + "walk: at the destination with the follower the escort did not complete (stage " + stageOf(g, inst) + ")");
  // the epilogue: shown as a closing scene once the moment has passed
  bool shown = false;
  for (int f = 0; f < 60 * 4 && !shown; f++) { g.update(SIM_DT, Input()); g.events.clear(); shown = g.mode == Mode::Dialogue; }
  if (!shown) fail(sd(seed) + "walk: the arrival's closing scene was not shown");
  g.mode = Mode::Play;
}

void wagonCheck(Game& g, uint64_t seed) {
  std::string why;
  const uint32_t inst = begin(g, "p1b_mechanics", why);
  if (!inst) { fail(sd(seed) + "wagon: cannot begin: " + why); return; }
  jump(g, inst, "road");
  steps(g, 30);
  WalkStats W;
  walkWith(g, inst, "carter", 90, W, true);
  steps(g, 10);
  const int ai = actorOf(g, inst, "carter");
  const Engine::Walker* w = ai >= 0 ? g.story.walkerOf(g.actors[(size_t)ai].id) : nullptr;
  float tow = -1;
  if (w && w->cartSet) tow = len(Vec2(w->cartX, w->cartY) - g.actors[(size_t)ai].p);
  out("p1b-engine: %swagon %d tiles, within 4 tiles %.1f %%, tow %.1f px, worst heading step %.3f rad\n", sd(seed).c_str(), W.tiles,
      W.counted ? 100.0 * W.near / W.counted : 0.0, tow, W.maxCartJump);
  if (!w || !w->cartSet) fail(sd(seed) + "wagon: no cart trails the driver");
  else if (tow < 16.0f || tow > 25.0f) fail(sd(seed) + "wagon: the cart is " + std::to_string(tow) + " px from its driver (want ~21)");
  if (W.maxCartJump > 0.35f) fail(sd(seed) + "wagon: the cart's heading jumped " + std::to_string(W.maxCartJump) + " rad in one step");
  if (Instance* in = g.story.find(inst)) g.story.drop(in->id);
  steps(g, 10);
}

void droveCheck(Game& g, uint64_t seed) {
  std::string why;
  const uint32_t inst = begin(g, "p1b_engine_drove", why);
  if (!inst) { fail(sd(seed) + "drove: cannot begin: " + why); return; }
  jump(g, inst, "road");
  steps(g, 40);
  auto beasts = [&](float& spread) {
    int n = 0;
    spread = 0;
    const int di = actorOf(g, inst, "drover");
    for (const Actor& a : g.actors)
      if (isDroveBeast(a) && a.st != AState::Dead) {
        n++;
        if (di >= 0) spread = std::max(spread, len(a.p - g.actors[(size_t)di].p) / T);
      }
    return n;
  };
  float sp0 = 0;
  const int n0 = beasts(sp0);
  int herd = 0;
  for (const Engine::Walker& w : g.story.walkers_) if (w.inst == inst) herd = w.herd;
  WalkStats W;
  walkWith(g, inst, "drover", 110, W, false);
  steps(g, 150);   // (the drove arrives: they gather round their drover as it stops)
  float sp1 = 0;
  const int n1 = beasts(sp1);
  out("p1b-engine: %sdrove of %d (set out %d): after %d tiles %d beasts, farthest %.1f tiles from the drover, drover within 4 tiles %.1f %%\n",
      sd(seed).c_str(), herd, n0, W.tiles, n1, sp1, W.counted ? 100.0 * W.near / W.counted : 0.0);
  if (herd < 3 || n0 != herd) fail(sd(seed) + "drove: " + std::to_string(n0) + " beasts set out for a drove of " + std::to_string(herd));
  if (n1 != herd) fail(sd(seed) + "drove: " + std::to_string(n1) + " beasts arrived of " + std::to_string(herd));
  if (sp1 > 10.0f) fail(sd(seed) + "drove: a beast straggles " + std::to_string(sp1) + " tiles behind");
  if (Instance* in = g.story.find(inst)) g.story.drop(in->id);
  steps(g, 10);
}

// walk up to an actor and swing at it (or just stand); returns frames run
int fight(Game& g, uint32_t inst, const std::string& role, bool hit, int maxFrames, const std::function<bool()>& until) {
  int f = 0;
  for (; f < maxFrames && !until(); f++) {
    const int ai = actorOf(g, inst, role);
    Input in;
    if (ai >= 0) {
      const Vec2 d = g.actors[(size_t)ai].p - g.pl().p;
      const float l = len(d);
      if (hit) {
        in.move = l > 16.0f ? norm(d) : Vec2();
        if (l < 24.0f) { g.pl().aim = norm(d); in.attack = f % 16 == 0; }
      } else if (l > 3.0f * T) in.move = norm(d);
    }
    g.update(SIM_DT, in);
    g.events.clear();
    if (g.mode == Mode::Dialogue) g.mode = Mode::Play;
    if (g.mode == Mode::Dead) break;
    if (g.inside) g.debugLeave();   // (the bot stepped into a doorway: back out to the fight)
  }
  return f;
}

void boutCheck(Game& g, uint64_t seed) {
  // won
  {
    std::string why;
    const uint32_t inst = begin(g, "p1b_engine_bout", why);
    if (!inst) { fail(sd(seed) + "bout: cannot begin: " + why); return; }
    jump(g, inst, "bout");
    g.godMode = false;
    g.pl().hp = g.pl().maxHp;
    steps(g, 20);
    float minPct = 2.0f, yieldPct = -1;
    bool died = false;
    int tick = 0, champId = -1;
    const int fr = fight(g, inst, "champ", true, 60 * 120, [&]() {
      const int ai = actorOf(g, inst, "champ");
      if (ai >= 0) champId = g.actors[(size_t)ai].id;
      else if (g_verb && champId >= 0) {
        for (const Actor& o : g.actors) if (o.id == champId) out("p1b-engine: %s  champ id %d now slot %d st %d hostile %d npc %d\n", sd(seed).c_str(), champId, o.slot, (int)o.st, (int)o.hostile, (int)o.npc);
        out("p1b-engine: %s  champ id %d gone; %zu actors\n", sd(seed).c_str(), champId, g.actors.size());
        champId = -1;
      }
      if (g_verb && (tick++ % 120) == 0) {
        int ph = -1;
        for (const Engine::Bout& b : g.story.bouts_) if (b.inst == inst) ph = b.phase;
        const Instance* i3 = g.story.find(inst);
        out("p1b-engine: %s  bout t=%.0f phase %d champ %s hp %.0f/%.0f hostile %d dist %.1f stage %s won %d\n", sd(seed).c_str(), tick / 60.0, ph,
            ai >= 0 ? "here" : "MISSING", ai >= 0 ? g.actors[(size_t)ai].hp : 0.0f, ai >= 0 ? g.actors[(size_t)ai].maxHp : 0.0f,
            ai >= 0 ? (int)g.actors[(size_t)ai].hostile : -1, ai >= 0 ? len(g.actors[(size_t)ai].p - g.pl().p) / T : -1.0f, stageOf(g, inst).c_str(),
            i3 && i3->vars.count("_won_bout") ? i3->vars.at("_won_bout") : -1);
      }
      if (ai >= 0) {
        const Actor& a = g.actors[(size_t)ai];
        if (a.st == AState::Dead) died = true;
        if (a.posture == art::Posture::Kneel && yieldPct < 0) yieldPct = a.hp / std::max(1.0f, a.maxHp);
        if (a.hostile || a.posture == art::Posture::Kneel) minPct = std::min(minPct, a.hp / std::max(1.0f, a.maxHp));
      }
      g.pl().hp = std::max(g.pl().hp, g.pl().maxHp * 0.5f);   // (the won bout: the player is not the one tested here)
      const Instance* in = g.story.find(inst);
      return !in || in->done;
    });
    out("p1b-engine: %sbout won in %.1f s: yielded at %.0f %% health, lowest %.0f %%, %s\n", sd(seed).c_str(), fr * SIM_DT, yieldPct * 100, minPct * 100,
        died ? "DIED" : "alive");
    if (died) fail(sd(seed) + "bout: the opponent died");
    if (yieldPct < 0) fail(sd(seed) + "bout: the opponent never knelt to yield");
    else if (yieldPct > 0.26f) fail(sd(seed) + "bout: the opponent yielded above a quarter health");
    if (!endedOk(g, inst) || g.story.find(inst)) fail(sd(seed) + "bout: the won bout did not complete (stage " + stageOf(g, inst) + ")");
    steps(g, 30);
  }
  // lost
  {
    std::string why;
    const uint32_t inst = begin(g, "p1b_engine_bout", why);
    if (!inst) { fail(sd(seed) + "bout (lost): cannot begin: " + why); return; }
    jump(g, inst, "bout");
    g.pl().hp = g.pl().maxHp;
    const int gold0 = g.gold;
    const size_t inv0 = g.inv.size();
    steps(g, 20);
    float minHp = 1e9f;
    bool beatenSeen = false;
    int tick = 0;
    const int fr = fight(g, inst, "champ", false, 60 * 150, [&]() {
      minHp = std::min(minHp, g.pl().hp);
      if (g_verb && (tick++ % 300) == 0) {
        const int ai = actorOf(g, inst, "champ");
        int ph = -1;
        for (const Engine::Bout& b : g.story.bouts_) if (b.inst == inst) ph = b.phase;
        out("p1b-engine: %s  lost t=%.0f phase %d champ %s hostile %d st %d dist %.1f player hp %.0f god %d iframes %.1f\n", sd(seed).c_str(), tick / 60.0, ph,
            ai >= 0 ? "here" : "MISSING", ai >= 0 ? (int)g.actors[(size_t)ai].hostile : -1, ai >= 0 ? (int)g.actors[(size_t)ai].st : -1,
            ai >= 0 ? len(g.actors[(size_t)ai].p - g.pl().p) / T : -1.0f, g.pl().hp, (int)g.godMode, g.pl().iframes);
        if (ai >= 0) { const Actor& c = g.actors[(size_t)ai]; out("p1b-engine:     champ dmg %.1f range %.0f atkCd %.2f level %d mon %d ranged %d; player st %d armor %.1f\n", c.dmg, c.range, c.atkCd, c.level, (int)c.mon, (int)c.ranged, (int)g.pl().st, g.pl().armor);
          for (const Actor& o : g.actors) if (!o.player && len2(o.p - g.pl().p) < (3.0f * T) * (3.0f * T)) out("p1b-engine:       near: %s npc %d critter %d hostile %d slot %d\n", o.name.c_str(), (int)o.npc, (int)o.critter, (int)o.hostile, o.slot); }
      }
      const Instance* in = g.story.find(inst);
      if (in && in->vars.count("_lost_bout")) beatenSeen = true;
      return !in || in->done || g.mode == Mode::Dead;
    });
    const Instance* in = g.story.find(inst);
    const bool toElse = !in || (in->done && in->failed);
    out("p1b-engine: %sbout lost in %.1f s: lowest HP %.1f, mode %s, gold %d -> %d, %s\n", sd(seed).c_str(), fr * SIM_DT, minHp,
        g.mode == Mode::Dead ? "DEAD" : "alive", gold0, g.gold, toElse ? "to its else" : "NOT ended");
    if (g.mode == Mode::Dead) { fail(sd(seed) + "bout: the player died in a bout"); g.mode = Mode::Play; g.pl().st = AState::Idle; g.pl().hp = g.pl().maxHp; }
    if (minHp < 0.99f) fail(sd(seed) + "bout: the player's HP fell to " + std::to_string(minHp));
    if (g.gold != gold0 || g.inv.size() != inv0) fail(sd(seed) + "bout: something was lost in the lost bout");
    if (!toElse) fail(sd(seed) + "bout: the lost bout did not go to its else (stage " + stageOf(g, inst) + ")");
    (void)beatenSeen;
    g.godMode = true;
    g.pl().hp = g.pl().maxHp;
    steps(g, 30);
  }
}

void placesChecks(Game& g, uint64_t seed) {
  std::string why;
  const uint32_t inst = begin(g, "p1b_engine_places", why);
  if (!inst) { fail(sd(seed) + "places: cannot cast: " + why); return; }
  Instance* in = g.story.find(inst);
  auto bind = [&](const std::string& r) { Instance* i2 = g.story.find(inst); return i2 ? bindingOf(*i2, r) : nullptr; };
  if (g_verb)
    for (const char* r : {"tower", "hut", "den", "crypt", "camp", "mill"})
      if (const Binding* b = bind(r)) out("p1b-engine: %s%s = %s\n", sd(seed).c_str(), r, b->name.c_str());
  (void)in;
  // ---- the beacon: by the tower, the use button lights it and the goal moves on
  {
    jump(g, inst, "beacon");
    const Binding* tb = bind("tower");
    bool ok = false;
    if (tb) {
      g.teleportGlobal(tb->gx, tb->gy + 4);
      steps(g, 20);
      int tx = -1, ty = -1;
      const Map& M = g.world.over;
      const int cx = tb->gx - g.world.ox, cy = tb->gy - g.world.oy;
      for (int y = cy - 14; y <= cy + 14 && tx < 0; y++)
        for (int x = cx - 14; x <= cx + 14 && tx < 0; x++)
          if (M.in(x, y) && M.propAt(x, y) == (int)art::Prop::WatchtowerRuin + 1) { tx = x; ty = y; }
      if (tx < 0) fail(sd(seed) + "beacon: no watchtower prop near " + tb->name);
      else {
        // stand at the tower's foot, facing it
        Vec2 at = ClimbOps::freeSpot(g, tx, ty + 1);
        g.pl().p = at;
        g.pl().aim = norm(Vec2(tx * T + 8.0f, ty * T + 8.0f) - at);
        g.pl().face = faceOf(g.pl().aim);
        int wx = 0, wy = 0;
        const int wp = wantedProp(g, wx, wy);
        Input use;
        use.interact = true;
        g.update(SIM_DT, use);
        g.events.clear();
        steps(g, 5);
        ok = stageOf(g, inst) == "hermit";
        out("p1b-engine: %sbeacon at %s: wanted %s, lit %s, goal %s\n", sd(seed).c_str(), tb->name.c_str(), wp ? "yes" : "no",
            beaconLit(g, tx, ty) ? "yes" : "no", ok ? "done" : "NOT done");
        if (!wp) fail(sd(seed) + "beacon: the use button does not offer the tower's beacon");
        if (!beaconLit(g, tx, ty)) fail(sd(seed) + "beacon: the beacon is not lit");
        if (!ok) fail(sd(seed) + "beacon: lighting it did not complete the goal (stage " + stageOf(g, inst) + ")");
      }
    }
  }
  // ---- the people inside: a cave, a ruin, a camp
  auto reachable = [&](const Map& m, int fx, int fy) {
    std::vector<uint8_t> seen((size_t)m.w * m.h, 0);
    std::deque<int> q;
    const int s = (m.exitY - 1) * m.w + m.exitX;
    if (s < 0 || s >= m.w * m.h) return false;
    q.push_back(s);
    seen[(size_t)s] = 1;
    while (!q.empty()) {
      const int c = q.front();
      q.pop_front();
      const int x = c % m.w, y = c / m.w;
      if (std::abs(x - fx) <= 1 && std::abs(y - fy) <= 1) return true;
      static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
      for (int k = 0; k < 4; k++) {
        const int nx = x + dx[k], ny = y + dy[k];
        if (!m.in(nx, ny) || m.blocked(nx, ny)) continue;
        const int ni = ny * m.w + nx;
        if (seen[(size_t)ni]) continue;
        seen[(size_t)ni] = 1;
        q.push_back(ni);
      }
    }
    return false;
  };
  for (const char* pair : {"hermit:den", "scholar:crypt"}) {
    const std::string pr = pair, role = pr.substr(0, pr.find(':')), site = pr.substr(pr.find(':') + 1);
    jump(g, inst, role);
    const Binding* sb = bind(site);
    const int h = sb ? siteByIdLoad(g, sb->site) : -1;
    if (h < 0 || !g.debugEnterSite(h)) { fail(sd(seed) + "inside: cannot enter " + site); continue; }
    steps(g, 20);
    const int ai = actorOf(g, inst, role);
    if (ai < 0) { fail(sd(seed) + "inside: nobody waits inside " + g.world.sites[(size_t)h].name); g.debugLeave(); steps(g, 5); continue; }
    const Actor& a = g.actors[(size_t)ai];
    const int fx = (int)std::floor(a.p.x / T), fy = (int)std::floor((a.p.y - 2) / T);
    const int de = std::abs(fx - g.sub.exitX) + std::abs(fy - g.sub.exitY);
    const bool reach = reachable(g.sub, fx, fy);
    const Instance* i2 = g.story.find(inst);
    const bool talk = i2 && g.story.talksTo(g, *i2, a);
    out("p1b-engine: %sinside %s: %s %d tiles from the way in, reachable %s, talkable %s\n", sd(seed).c_str(), g.world.sites[(size_t)h].name.c_str(),
        a.name.c_str(), de, reach ? "yes" : "no", talk ? "yes" : "no");
    if (!reach) fail(sd(seed) + "inside: " + role + " cannot be reached from the way in");
    if (!talk) fail(sd(seed) + "inside: " + role + " cannot be talked to");
    if (de < 6) fail(sd(seed) + "inside: " + role + " stands in the way in");
    g.debugLeave();
    steps(g, 10);
  }
  {
    jump(g, inst, "captive");
    const Binding* cb = bind("camp");
    if (!cb) fail(sd(seed) + "inside: no camp");
    else {
      g.teleportGlobal(cb->gx, cb->gy + 3);
      steps(g, 30);
      const int ai = actorOf(g, inst, "captive");
      if (ai < 0) fail(sd(seed) + "inside: nobody waits in the camp " + cb->name);
      else {
        const Actor& a = g.actors[(size_t)ai];
        const int fx = (int)std::floor(a.p.x / T), fy = (int)std::floor(a.p.y / T);
        int tent = 99;
        for (int y = fy - 4; y <= fy + 4; y++)
          for (int x = fx - 4; x <= fx + 4; x++)
            if (g.world.over.in(x, y) && g.world.over.propAt(x, y) == (int)art::Prop::Tent + 1) tent = std::min(tent, std::max(std::abs(x - fx), std::abs(y - fy)));
        const Instance* i2 = g.story.find(inst);
        const bool talk = i2 && g.story.talksTo(g, *i2, a);
        out("p1b-engine: %scamp %s: %s (child %s), nearest tent %d tiles, talkable %s\n", sd(seed).c_str(), cb->name.c_str(), a.name.c_str(),
            a.role == Role::Child ? "yes" : "no", tent, talk ? "yes" : "no");
        if (!talk) fail(sd(seed) + "inside: the camp's captive cannot be talked to");
        if (a.role != Role::Child) fail(sd(seed) + "young: the captive is not drawn as a child");
        if (tent > 4) out("p1b-engine: %s(the camp %s has no tent near the captive: %d)\n", sd(seed).c_str(), cb->name.c_str(), tent);
      }
    }
  }
  // ---- calm: 60 s inside the cave beside its creatures, no blow lands
  {
    jump(g, inst, "calm");
    const Binding* db = bind("den");
    const int h = db ? siteByIdLoad(g, db->site) : -1;
    if (h >= 0 && g.debugEnterSite(h)) {
      steps(g, 30);
      int best = -1;
      float bd = 1e30f;
      for (size_t k = 1; k < g.actors.size(); k++)
        if (g.actors[k].hostile && g.actors[k].st != AState::Dead && len2(g.actors[k].p - g.pl().p) < bd) { bd = len2(g.actors[k].p - g.pl().p); best = (int)k; }
      if (best < 0) out("p1b-engine: %scalm: the cave has no creatures to test\n", sd(seed).c_str());
      else {
        const Vec2 at = g.actors[(size_t)best].p;
        g.pl().p = ClimbOps::freeSpot(g, (int)std::floor(at.x / T), (int)std::floor(at.y / T) + 1);
        g.godMode = false;
        g.pl().hp = g.pl().maxHp;
        const float hp0 = g.pl().hp;
        int swings = 0;
        for (int f = 0; f < 60 * 60; f++) {
          g.update(SIM_DT, Input());
          g.events.clear();
          if (g.mode == Mode::Dialogue) g.mode = Mode::Play;
          for (const Actor& a : g.actors) if (a.hostile && (a.st == AState::Windup || a.st == AState::Strike) && len2(a.p - g.pl().p) < (4.0f * T) * (4.0f * T)) swings++;
        }
        out("p1b-engine: %scalm: 60 s beside %s's creatures: HP %.0f -> %.0f, %d swing steps\n", sd(seed).c_str(), g.world.sites[(size_t)h].name.c_str(), hp0,
            g.pl().hp, swings);
        if (g.pl().hp < hp0 - 0.01f || swings > 0) fail(sd(seed) + "calm: the calmed cave's creatures attacked");
        g.godMode = true;
        g.pl().hp = g.pl().maxHp;
      }
      g.debugLeave();
      steps(g, 10);
    } else fail(sd(seed) + "calm: cannot enter the cave");
  }
  // ---- burden: the pace with the sack and without
  {
    g.teleportGlobal(g.world.ox + g.world.sites[(size_t)g.world.startSite].ex, g.world.oy + g.world.sites[(size_t)g.world.startSite].ey + 2);
    steps(g, 20);
    const int32_t ax = g.world.ox + (int32_t)std::floor(g.pl().p.x / T), ay = g.world.oy + (int32_t)std::floor(g.pl().p.y / T);
    int32_t tx = 0, ty = 0;
    auto pace = [&]() {
      const Vec2 start(((float)(ax - g.world.ox)) * T + 8.0f, ((float)(ay - g.world.oy)) * T + 10.0f);
      g.pl().p = start;
      const auto path = pathTo(g, tx, ty);
      float dist = 0;
      int frames = 0;
      size_t k = 0;
      for (int f = 0; f < 120 && k < path.size(); f++) {
        const Vec2 c((path[k].first - g.world.ox) * T + 8.0f, (path[k].second - g.world.oy) * T + 10.0f);
        const Vec2 d = c - g.pl().p;
        if (len(d) < 5.0f) { k++; continue; }
        Input in;
        in.move = norm(d);
        const Vec2 b = g.pl().p;
        g.update(SIM_DT, in);
        g.events.clear();
        if (g.mode == Mode::Dialogue) g.mode = Mode::Play;
        if (f > 10) { dist += len(g.pl().p - b); frames++; }
      }
      return frames ? dist / (frames * SIM_DT) : 0.0f;
    };
    if (!farTile(g, 30, tx, ty, ax, ay)) fail(sd(seed) + "burden: no open ground");
    else {
      const bool heavy = g.story.burdened();
      const float withSack = pace();
      jump(g, inst, "turn");   // (takes the sack; turns the rogue on the player)
      const float without = pace();
      const float ratio = without > 0 ? withSack / without : 0;
      out("p1b-engine: %sburden: %s, pace %.1f px/s with the sack, %.1f without (x%.2f), burdened after %s\n", sd(seed).c_str(), heavy ? "heavy" : "NOT heavy",
          withSack, without, ratio, g.story.burdened() ? "STILL" : "no");
      if (!heavy) fail(sd(seed) + "burden: the heavy sack did not burden the player");
      if (ratio < 0.65f || ratio > 0.75f) fail(sd(seed) + "burden: the pace with the sack is x" + std::to_string(ratio) + " (want 0.7)");
      if (g.story.burdened()) fail(sd(seed) + "burden: still burdened after the sack was taken");
    }
  }
  // ---- hostile: the rogue fights to the death, slay completes on him
  {
    if (stageOf(g, inst) != "turn") jump(g, inst, "turn");
    const Binding* rb = bind("rogue");
    if (rb) g.teleportGlobal(rb->gx, rb->gy + 3);
    steps(g, 30);
    bool hostileSeen = false, guardOn = false;
    const int fr = fight(g, inst, "rogue", true, 60 * 120, [&]() {
      const int ai = actorOf(g, inst, "rogue");
      if (ai >= 0) {
        const Actor& a = g.actors[(size_t)ai];
        if (a.hostile && !a.npc) hostileSeen = true;
        for (const Actor& o : g.actors) if (o.role == Role::Guard && o.target == a.id) guardOn = true;
      }
      const Instance* i2 = g.story.find(inst);
      return !i2 || i2->done;
    });
    const Instance* i2 = g.story.find(inst);
    const bool done = !i2 || (i2->done && !i2->failed);
    out("p1b-engine: %shostile: turned %s, slain in %.1f s, goal %s, a guard took him on: %s\n", sd(seed).c_str(), hostileSeen ? "yes" : "NO", fr * SIM_DT,
        done ? "done" : "NOT done", guardOn ? "yes" : "no");
    if (!hostileSeen) fail(sd(seed) + "hostile: the person never turned on the player");
    if (!done) fail(sd(seed) + "hostile: slaying him did not complete the goal (stage " + stageOf(g, inst) + ")");
  }
}

// ---- casting: the new place kinds and filters from the start village
struct CastRates { int tower = 0, dwelling = 0, road = 0, makes = 0, flag = 0, chain = 0, seeds = 0, dwell3 = 0; std::map<std::string, int> roadAny; };
void castingCheck(Game& g, uint64_t seed, CastRates& R) {
  R.seeds++;
  ew::Gid lastSite = 0;
  auto tryCast = [&](const char* role, bool& ok, std::string& name) {
    std::string src = std::string("script p1b_cast_probe\ntitle PROBE\narchetype probe\ntier 2\nfamily test\nhook npc any\npitch \"PROBE?\"\n"
                                  "role giver giver\nrole home site home\n") + role +
                      "\nstage start\n  talk giver\n  say \"{PLACE}.\"\n  opt \"YES.\" -> done\nstage done\n  end success\n  journal \"DONE.\"\n  do reward small\n";
    std::vector<dsl::Script> v;
    std::vector<std::string> errs;
    dsl::parse(src.c_str(), "probe", v, errs);
    if (v.empty() || !errs.empty()) { fail(sd(seed) + "casting: the probe does not parse: " + (errs.empty() ? std::string() : errs[0])); ok = false; return; }
    dsl::link(v[0]);
    std::vector<Binding> cast;
    std::string why;
    ok = castScript(g, v[0], hookActor(g, g.world.startSite), g.world.startSite, seed * 31u + 7u, cast, why);
    name = ok && cast.size() > 2 ? cast[2].name : why;
    lastSite = ok && cast.size() > 2 ? cast[2].site : 0;
  };
  bool ok = false;
  std::string nm;
  tryCast("role place site watchtower near", ok, nm); if (ok) R.tower++; if (g_verb || !ok) out("p1b-engine: %scast watchtower: %s\n", sd(seed).c_str(), nm.c_str());
  tryCast("role place site dwelling near", ok, nm); if (ok) R.dwelling++; if (g_verb || !ok) out("p1b-engine: %scast dwelling: %s\n", sd(seed).c_str(), nm.c_str());
  // with road: a camp, a ruin or a cave near a road (the gate is the camp's: a road ambush; the others reported)
  for (const char* t : {"camp", "ruin", "cave"}) {
    tryCast((std::string("role place site ") + t + " near with road").c_str(), ok, nm);
    if (ok && std::string(t) == "camp") R.road++;
    if (ok) R.roadAny[t]++;
    if (g_verb) out("p1b-engine: %scast %s with road: %s\n", sd(seed).c_str(), t, nm.c_str());
  }
  tryCast("role place site town near with makes bread", ok, nm); if (ok) R.makes++; if (g_verb || !ok) out("p1b-engine: %scast town makes bread: %s\n", sd(seed).c_str(), nm.c_str());
  // with flag: a town near the start whose census is raided now (forced on the nearest town's census: the mechanism)
  {
    // (a census exists only for a settlement built in the window: the nearest such town in the near band)
    const Site st0 = g.world.sites[(size_t)g.world.startSite];
    const int32_t sx0 = g.world.ox + st0.ex, sy0 = g.world.oy + st0.ey;
    int h = -1;
    int64_t hd = INT64_MAX;
    for (int k = 0; k < (int)g.world.sites.size(); k++) {
      const Site& S = g.world.sites[(size_t)k];
      if (S.type != SiteType::Town || S.bldgCount <= 0) continue;
      const int64_t d = (int64_t)(g.world.ox + S.ex - sx0) * (g.world.ox + S.ex - sx0) + (int64_t)(g.world.oy + S.ey - sy0) * (g.world.oy + S.ey - sy0);
      if (d >= 40LL * 40 && d <= 560LL * 560 && d < hd) { hd = d; h = k; }
    }
    if (h < 0) {   // (none built yet: the nearest town is visited, so its streets and census exist)
      tryCast("role place site town near", ok, nm);
      const int th = ok && lastSite ? siteByIdLoad(g, lastSite) : -1;
      if (th >= 0) { g.world.sites[(size_t)th].discovered = true; g.fastTravel(th); steps(g, 30); h = th; }
    }
    life::Census* c = h >= 0 ? g.life.census(g.world, h) : nullptr;
    if (!c) out("p1b-engine: %scast with flag: no census at the nearest town (%s)\n", sd(seed).c_str(), nm.c_str());
    if (c) {
      const int32_t keep = c->raidDay;
      c->raidDay = g.day;
      c->moodFlags |= life::MF_RAIDED;
      tryCast("role place site town near with flag raided", ok, nm);
      c->moodFlags &= (uint16_t)~life::MF_RAIDED;
      c->raidDay = keep;
      if (ok) R.flag++;
      if (g_verb || !ok) out("p1b-engine: %scast town with flag raided: %s\n", sd(seed).c_str(), nm.c_str());
    }
  }
  if (g_verb) {
    const Site st = g.world.sites[(size_t)g.world.startSite];
    const int32_t sx = g.world.ox + st.ex, sy = g.world.oy + st.ey;
    for (SiteType ty : {SiteType::BanditCamp, SiteType::Ruin, SiteType::Cave}) {
      std::string line;
      int n = 0;
      for (int32_t ry = ew::regionOf(sy) - 2; ry <= ew::regionOf(sy) + 2; ry++)
        for (int32_t rx = ew::regionOf(sx) - 2; rx <= ew::regionOf(sx) + 2; rx++)
          for (const ew::SitePlan& p : g.world.regionPlan(rx, ry).sites) {
            if (p.type != ty || n >= 8) continue;
            int64_t best = INT64_MAX, best2 = INT64_MAX;
            for (int32_t qy = ew::regionOf(p.ey) - 1; qy <= ew::regionOf(p.ey) + 1; qy++)
              for (int32_t qx = ew::regionOf(p.ex) - 1; qx <= ew::regionOf(p.ex) + 1; qx++)
                for (const ew::RoadPlan& rd : g.world.regionPlan(qx, qy).roads)
                  for (const ew::GTile& t : rd.pts) {
                    const int64_t d = (int64_t)(t.x - p.ex) * (t.x - p.ex) + (int64_t)(t.y - p.ey) * (t.y - p.ey);
                    if (rd.cls <= 1) best = std::min(best, d); else best2 = std::min(best2, d);
                  }
            line += " " + std::to_string((int)std::sqrt((double)best)) + "/" + std::to_string((int)std::sqrt((double)best2));
            n++;
          }
      out("p1b-engine: %sroad distances (road/spur) for type %d:%s\n", sd(seed).c_str(), (int)ty, line.c_str());
    }
  }
  // a beacon chain: two watchtowers within 250 tiles of each other, both within 560 of the start
  {
    const Site st = g.world.sites[(size_t)g.world.startSite];
    const int32_t sx = g.world.ox + st.ex, sy = g.world.oy + st.ey;
    std::vector<std::pair<int32_t, int32_t>> towers;
    int dwellings = 0;
    for (int32_t ry = ew::regionOf(sy) - 3; ry <= ew::regionOf(sy) + 3; ry++)
      for (int32_t rx = ew::regionOf(sx) - 3; rx <= ew::regionOf(sx) + 3; rx++)
        for (const ew::SitePlan& p : g.world.regionPlan(rx, ry).sites)
          if (p.type == SiteType::Vignette && (p.kind == (uint8_t)ew::VignetteKind::HunterCamp || p.kind == (uint8_t)ew::VignetteKind::FishingHut ||
                                                p.kind == (uint8_t)ew::VignetteKind::HerbGarden) &&
              (int64_t)(p.ex - sx) * (p.ex - sx) + (int64_t)(p.ey - sy) * (p.ey - sy) <= 560LL * 560) dwellings++;
          else if (p.type == SiteType::Vignette && p.kind == (uint8_t)ew::VignetteKind::Watchtower &&
              (int64_t)(p.ex - sx) * (p.ex - sx) + (int64_t)(p.ey - sy) * (p.ey - sy) <= 560LL * 560)
            towers.push_back({p.ex, p.ey});
    bool chain = false;
    for (size_t i = 0; i < towers.size() && !chain; i++)
      for (size_t j = i + 1; j < towers.size() && !chain; j++)
        chain = (int64_t)(towers[i].first - towers[j].first) * (towers[i].first - towers[j].first) +
                    (int64_t)(towers[i].second - towers[j].second) * (towers[i].second - towers[j].second) <= 250LL * 250;
    if (chain) R.chain++;
    if (dwellings >= 3) R.dwell3++;
    if (g_verb) out("p1b-engine: %s%zu watchtowers within 560 tiles, a chain pair: %s\n", sd(seed).c_str(), towers.size(), chain ? "yes" : "no");
  }
}

int engineCmd(int argc, char** argv) {
  uint64_t A = 1, B = 5;
  bool castOnly = false;
  std::string only;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) {
      if (!parseSeedRange(argv[++i], A, B)) { printf("bad --seeds range '%s'\n", argv[i]); return 2; }
    } else if (!strcmp(argv[i], "--verbose")) g_verb = true;
    else if (!strcmp(argv[i], "--casting")) castOnly = true;
    else if (!strcmp(argv[i], "--only") && i + 1 < argc) only = argv[++i];
  }
  const auto t0 = std::chrono::steady_clock::now();
  g_bad = 0;
  CastRates R;
  auto want = [&](const char* k) { return only.empty() || only == k; };
  for (uint64_t seed = A; seed <= B; seed++) {
    g_curSeed = seed;
    Game g(seed);
    g.newEndlessGame(seed);
    g.mode = Mode::Play; g.godMode = true; g.noWildSpawns = true;
    steps(g, 60);
    if (want("casting")) castingCheck(g, seed, R);
    if (castOnly) continue;
    const int startSite = g.world.startSite;
    auto home = [&]() {
      const Site& st = g.world.sites[(size_t)startSite];
      g.teleportGlobal(g.world.ox + st.ex, g.world.oy + st.ey + 2);
      g.godMode = true; g.pl().hp = g.pl().maxHp;
      steps(g, 30);
    };
    if (want("wagon")) { home(); wagonCheck(g, seed); }
    if (want("drove")) { home(); droveCheck(g, seed); }
    if (want("bout")) { home(); boutCheck(g, seed); }
    if (want("places")) { home(); placesChecks(g, seed); }
    if (want("walk")) { home(); walkCheck(g, seed); }
  }
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  if (R.seeds)
    printf("p1b-engine casting over %d seeds: watchtower %d, dwelling %d, camp with road %d, town makes bread %d, town with flag (forced) %d; "
           "a beacon chain pair (2 towers <= 250 tiles apart) on %d\n", R.seeds, R.tower, R.dwelling, R.road, R.makes, R.flag, R.chain);
  if (R.seeds >= 10) {
    const int need = (R.seeds * 9 + 9) / 10;
    auto gate = [&](int n, const char* what) { if (n < need) { printf("FAIL: p1b-engine casting: %s cast on %d of %d seeds (want %d)\n", what, n, R.seeds, need); g_bad++; } };
    gate(R.tower, "watchtower"); gate(R.dwelling, "dwelling"); gate(R.road, "with road"); gate(R.makes, "with makes"); gate(R.flag, "with flag");
  }
  if (R.seeds) printf("p1b-engine casting: with road by type: camp %d, ruin %d, cave %d; 3+ dwellings within 560 tiles on %d\n", R.roadAny["camp"], R.roadAny["ruin"], R.roadAny["cave"], R.dwell3);
  printf("p1b-engine: seeds %llu..%llu (%.0f s): %d failures\n", (unsigned long long)A, (unsigned long long)B, secs, g_bad);
  return g_bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--p1b-engine", "P1b ENGINE: escort / wagon / drove walked by a bot, bout won and lost, hostile, calm, people inside, burden, beacon, casting rates [--seeds A..B --only walk|wagon|drove|bout|places|casting --casting --verbose]", engineCmd);
