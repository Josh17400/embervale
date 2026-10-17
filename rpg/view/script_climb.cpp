// M11 "High Ground" script commands of the CLIMB lane (moving over the relief: VISION_PLAN 11.2, 11.4, 13 M11, 15.32).
//
//   climbto ramp|stair|earth|cliff [n]   pick the n-th nearest way up of the loaded window (a ramp or stone stairs with
//                           walkable ground below and above; earth: a ramp without stairs; cliff: a stretch of face whose
//                           nearest way up is 16-24 tiles along it) and stand the hero on its high side, 3 tiles past
//                           the top (for cliff: 8 tiles beyond the face). The pick is remembered for the commands below.
//   climbguard [chase]      a town guard 3 tiles out on the picked way up's low side (cliff: 8 tiles out); chase: one
//                           hostile to the hero, who runs at them up the way (god mode keeps the hero standing)
//   climbparty [n]          n (default 2) companions recruited and left 6 tiles out on the low side: they follow the
//                           hero up by the party's own code
//   climbhero low|top|mid   the hero to the picked way's foot (5 tiles out), its top (3 past it) or onto it (the ramp's
//                           own tile)
//   climbseek fall|stack    the windows of the start and of the nearest ranges, peaks and hills are visited until one
//                           holds a waterfall (a stacked face); the hero stands 5 tiles south of the nearest one
//   climbnav [reset]        print the path searches' counters (Game::navStats: requests, short and long searches, nodes,
//                           the worst navStep ms); reset zeroes them
//   expect climbup guard|party [tiles]   every guard spawned here / every companion stands on the picked way's high side
//                           (its top level or higher) within `tiles` (default 4) of the hero
//   expect climbnav fails N | worst MS   at most N long searches failed / the worst navStep took at most MS ms
//   expect climbpace F      the hero's relief pace now (Actor::reliefK) is F (within 0.02): 0.8 a hillside or an earth
//                           ramp, 0.9 stairs, 1 flat
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/party.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"
#include "rpg/world/source.h"

namespace {
constexpr float T = (float)TILE;
struct Pick {
  bool ok = false;
  int32_t rx = 0, ry = 0;   // the way up's tile (global; for cliff the face tile)
  int ux = 0, uy = -1;      // toward the higher level
  int top = 0;              // the level above
  bool cliff = false;
};
Pick g_pick;
std::vector<int> g_guards;

Vec2 centreAt(const Game& g, int32_t gx, int32_t gy) { return Vec2((gx - g.world.ox) * T + 8.0f, (gy - g.world.oy) * T + 10.0f); }
bool openAt(const Map& m, int x, int y) { return m.in(x, y) && !m.blocked(x, y) && m.bldgAt[(size_t)y * m.w + x] < 0; }

// the tile `k` steps from the pick along the way up (negative: down the low side), global
void along(int k, int32_t& x, int32_t& y) { x = g_pick.rx + g_pick.ux * k; y = g_pick.ry + g_pick.uy * k; }

bool cmdClimbTo(ScriptCtx& c) {
  Game& g = c.game;
  if (g.inside) g.debugLeave();
  const std::string kind = c.arg(1).empty() ? std::string("ramp") : c.arg(1);
  const int want = c.arg(2).empty() ? 0 : std::max(0, std::atoi(c.arg(2).c_str()));
  const Map& m = g.world.over;
  const int ptx = (int)std::floor(g.pl().p.x / T), pty = (int)std::floor((g.pl().p.y - 2) / T);
  static const int dx[4] = {0, 1, -1, 0}, dy[4] = {-1, 0, 0, 1};
  struct Cand { long d2; int x, y, ux, uy, top; };
  std::vector<Cand> cs;
  for (int y = 12; y < m.h - 12; y++)
    for (int x = 12; x < m.w - 12; x++) {
      const uint8_t b = m.heightBits(x, y);
      const int l = b & Map::HEIGHT_LEVEL;
      const bool ramp = (b & Map::HEIGHT_RAMP) && !m.blocked(x, y);
      const bool face = (b & Map::HEIGHT_CLIFF) && !(b & Map::HEIGHT_FALL) && !groundWater(m.at(x, y));
      if (kind == "cliff" ? !face : !ramp) continue;
      if (kind == "stair" && !(b & Map::HEIGHT_STAIR)) continue;
      if (kind == "earth" && (b & Map::HEIGHT_STAIR)) continue;
      int ups = 0, ux = 0, uy = 0;
      for (int k = 0; k < 4; k++)
        if (m.heightAt(x + dx[k], y + dy[k]) > l) { ups++; ux = dx[k]; uy = dy[k]; }
      if (ups != 1) continue;
      const int reach = kind == "cliff" ? 8 : 5;
      if (!openAt(m, x + ux * reach, y + uy * reach) || !openAt(m, x - ux * reach, y - uy * reach)) continue;
      if (kind != "cliff" && (!openAt(m, x + ux, y + uy) || !openAt(m, x - ux, y - uy))) continue;
      if (kind == "cliff") {   // its nearest way up 16-24 tiles along the face
        int best = 999;
        for (int d = 1; d <= 26 && best == 999; d++)
          for (int s = -1; s <= 1; s += 2) {
            const int qx = x + uy * d * s, qy = y + ux * d * s;
            const uint8_t q = m.heightBits(qx, qy);
            if ((q & (Map::HEIGHT_RAMP | Map::HEIGHT_SLOPE)) && !m.blocked(qx, qy)) { best = d; break; }
          }
        if (best < 16 || best > 24) continue;
      }
      cs.push_back({(long)(x - ptx) * (x - ptx) + (long)(y - pty) * (y - pty), x, y, ux, uy, m.heightAt(x + ux, y + uy)});
    }
  std::sort(cs.begin(), cs.end(), [](const Cand& a, const Cand& b) { return a.d2 != b.d2 ? a.d2 < b.d2 : (a.y != b.y ? a.y < b.y : a.x < b.x); });
  // one per feature: picks closer than 10 tiles to an earlier one are the same way up
  std::vector<Cand> uniq;
  for (const Cand& q : cs) {
    bool near = false;
    for (const Cand& u : uniq) near |= std::abs(u.x - q.x) + std::abs(u.y - q.y) < 10;
    if (!near) uniq.push_back(q);
    if ((int)uniq.size() > want) break;
  }
  if ((int)uniq.size() <= want) { c.fail("climbto " + kind + ": only " + std::to_string(uniq.size()) + " in the window"); return true; }
  const Cand& q = uniq[(size_t)want];
  g_pick.ok = true;
  g_pick.rx = g.world.ox + q.x; g_pick.ry = g.world.oy + q.y;
  g_pick.ux = q.ux; g_pick.uy = q.uy; g_pick.top = q.top; g_pick.cliff = kind == "cliff";
  g_guards.clear();
  int32_t hx, hy;
  along(g_pick.cliff ? 8 : 3, hx, hy);
  g.teleportGlobal(hx, hy);
  g.mode = Mode::Play;
  // (the window may have moved: the pick is global, the hero faces down the way)
  g.actors[0].face = q.uy < 0 ? 0 : q.uy > 0 ? 1 : q.ux > 0 ? 3 : 2;
  c.view.snap(g);
  std::printf("script: climbto %s %d -> way up at %d,%d (up %+d,%+d, top level %d)\n", kind.c_str(), want, g_pick.rx, g_pick.ry, q.ux, q.uy, q.top);
  return true;
}

bool cmdClimbGuard(ScriptCtx& c) {
  Game& g = c.game;
  if (!g_pick.ok) { c.fail("climbguard: no climbto pick"); return true; }
  int32_t x, y;
  along(g_pick.cliff ? -8 : -3, x, y);
  const int id = ClimbOps::spawnGuard(g, centreAt(g, x, y), c.arg(1) == "chase");
  g_guards.push_back(id);
  std::printf("script: climbguard %s at %d,%d (actor %d)\n", c.arg(1) == "chase" ? "chasing the hero" : "posted", x, y, id);
  return true;
}

// one sim step with no input (bodies of new companions come into play in the party's keep-up)
void run0(Game& g) { g.update(SIM_DT, Input()); g.events.clear(); }

bool cmdClimbParty(ScriptCtx& c) {
  Game& g = c.game;
  if (!g_pick.ok) { c.fail("climbparty: no climbto pick"); return true; }
  const int n = c.arg(1).empty() ? 2 : std::clamp(std::atoi(c.arg(1).c_str()), 1, 4);
  static const char* names[4] = {"ASKA", "BRAND", "CERI", "DUNN"};
  int32_t x, y;
  along(g_pick.cliff ? -8 : -6, x, y);
  const uint64_t culture = g.world.startSite >= 0 && g.world.startSite < (int)g.world.sites.size() ? g.world.sites[(size_t)g.world.startSite].culture : 0;
  // (test only: a level that leads n companions; the ones already with the hero are reused)
  g.plLevel = std::max(g.plLevel, 24);
  int have = 0;
  for (const party::Member& m : g.party.members) have += m.state == party::State::With;
  for (int k = have; k < n; k++) {
    const Vec2 at = centreAt(g, x + g_pick.uy * (k - n / 2), y + g_pick.ux * (k - n / 2));
    const int mi = g.partyRecruitStranger(names[k], (k & 1) != 0, culture, 9100u + (uint32_t)k * 77u, party::Origin::Saga, at);
    if (mi < 0) { c.fail("climbparty: a companion could not be recruited"); return true; }
  }
  // (their bodies come into play at the hero's side: put them back down the way, where they were left)
  if (have || n) run0(g);
  int k = 0;
  for (size_t i = 1; i < g.actors.size(); i++) {
    Actor& a = g.actors[i];
    if (!a.comp || k >= n) continue;
    a.p = ClimbOps::freeSpot(g, x - g.world.ox + g_pick.uy * (k - n / 2), y - g.world.oy + g_pick.ux * (k - n / 2));
    a.navNext = -1;
    k++;
  }
  g.party.trail.clear();
  for (party::Member& m : g.party.members) m.trailAt = -1;
  std::printf("script: climbparty %d companions left at %d,%d\n", k, x, y);
  return true;
}

bool cmdClimbHero(ScriptCtx& c) {
  Game& g = c.game;
  if (!g_pick.ok) { c.fail("climbhero: no climbto pick"); return true; }
  const std::string w = c.arg(1);
  int32_t x, y;
  along(w == "low" ? -5 : w == "mid" ? 0 : 3, x, y);
  g.actors[0].p = centreAt(g, x, y);
  g.actors[0].navNext = -1;
  c.view.snap(g);
  return true;
}

// climbseek fall|stack: the windows of the start and of the high landmarks nearest it (ranges, peaks, and the windows round each; up to 64) are visited
// until one holds a waterfall (a face of 2+ levels); the hero stands 5 tiles south of the nearest one
bool cmdClimbSeek(ScriptCtx& c) {
  Game& g = c.game;
  if (g.inside) g.debugLeave();
  if (!g.world.src) { c.fail("climbseek: no endless source"); return true; }
  const std::string kind = c.arg(1).empty() ? std::string("fall") : c.arg(1);
  auto is = [&](const Map& m, int x, int y) {
    const uint8_t b = m.heightBits(x, y);
    if (kind == "stack") return (b & Map::HEIGHT_CLIFF) && !(b & Map::HEIGHT_FALL) && m.heightAt(x, y - 1) >= (b & Map::HEIGHT_LEVEL) + 2;
    return (b & Map::HEIGHT_FALL) != 0;
  };
  const int32_t px = g.world.ox + (int32_t)std::floor(g.pl().p.x / T), py = g.world.oy + (int32_t)std::floor((g.pl().p.y - 2) / T);
  std::vector<std::pair<double, std::pair<int32_t, int32_t>>> cands;
  cands.push_back({0.0, {px, py}});
  for (int32_t ry = ew::regionOf(py) - 6; ry <= ew::regionOf(py) + 6; ry++)
    for (int32_t rx = ew::regionOf(px) - 6; rx <= ew::regionOf(px) + 6; rx++)
      for (const ew::LandmarkPlan& L : g.world.src->region(rx, ry).landmarks)
        if (L.kind == ew::LandmarkKind::Range || L.kind == ew::LandmarkKind::Peak)
          for (int k = 0; k < 9; k++) {   // (its window and the eight round it: a range is wider than a window)
            const int32_t cx = L.x + (k % 3 - 1) * 200, cy = L.y + (k / 3 - 1) * 200;
            cands.push_back({std::hypot((double)(L.x - px), (double)(L.y - py)) + (k == 4 ? 0.0 : 1.0 + k), {cx, cy}});
          }
  std::sort(cands.begin(), cands.end());
  for (size_t k = 0; k < cands.size() && k < 64; k++) {
    if (k > 0) g.teleportGlobal(cands[k].second.first, cands[k].second.second);
    const Map& m = g.world.over;
    const int hx = (int)std::floor(g.pl().p.x / T), hy = (int)std::floor((g.pl().p.y - 2) / T);
    long bd = -1;
    int fx = 0, fy = 0;
    for (int y = 8; y < m.h - 8; y++)
      for (int x = 8; x < m.w - 8; x++)
        if (is(m, x, y)) {
          const long d2 = (long)(x - hx) * (x - hx) + (long)(y - hy) * (y - hy);
          if (bd < 0 || d2 < bd) { bd = d2; fx = x; fy = y; }
        }
    if (bd < 0) continue;
    const int32_t gx = g.world.ox + fx, gy = g.world.oy + fy;
    g.teleportGlobal(gx, gy + 5);
    g.mode = Mode::Play;
    c.view.snap(g);
    std::printf("script: climbseek %s -> %d,%d (window %d of %d)\n", kind.c_str(), gx, gy, (int)k, (int)cands.size());
    return true;
  }
  c.fail("climbseek " + kind + ": none in the " + std::to_string(std::min<size_t>(cands.size(), 64)) + " windows tried");
  return true;
}

bool cmdClimbNav(ScriptCtx& c) {
  Game& g = c.game;
  const Game::NavStats& s = g.navStats;
  std::printf("script: climbnav requests %d short %d/%d long %d (found %d failed %d held %d) follows %d nodes %d (worst %d) ms %.2f (worst %.3f)\n", s.requests,
              s.shortFound, s.shortFound + s.shortFails, s.longSearches, s.longFound, s.longFails, s.failHeld, s.cacheFollows, s.nodes, s.worstNodes, s.ms, s.worstMs);
  if (c.arg(1) == "reset") g.navStats = Game::NavStats();
  return true;
}

bool expClimbUp(ScriptCtx& c) {
  Game& g = c.game;
  if (!g_pick.ok) { c.fail("expect climbup: no climbto pick"); return true; }
  const float tiles = c.arg(3).empty() ? 4.0f : (float)std::atof(c.arg(3).c_str());
  const Map& m = g.world.over;
  // (the hero's own level, when the free tile they were put on lies below the pick's top: a switchback's turn)
  const int need = std::min(g_pick.top, m.heightAt((int)std::floor(g.pl().p.x / T), (int)std::floor((g.pl().p.y - 2) / T)));
  int n = 0;
  for (const Actor& a : g.actors) {
    const bool mine = c.arg(2) == "party" ? a.comp != 0 : std::find(g_guards.begin(), g_guards.end(), a.id) != g_guards.end();
    if (!mine || a.st == AState::Dead) continue;
    n++;
    const int lv = m.heightAt((int)std::floor(a.p.x / T), (int)std::floor((a.p.y - 2) / T));
    const float d = len(a.p - g.pl().p) / T;
    if (lv < need || d > tiles) {
      char b[160];
      std::snprintf(b, sizeof b, "expect climbup %s: %s on level %d (the hero's %d), %.1f tiles from the hero", c.arg(2).c_str(), a.name.c_str(), lv, need, d);
      c.fail(b);
    }
  }
  if (!n) c.fail("expect climbup " + c.arg(2) + ": none in play");
  return true;
}

bool expClimbNav(ScriptCtx& c) {
  const Game::NavStats& s = c.game.navStats;
  const double v = std::atof(c.arg(3).c_str());
  if (c.arg(2) == "fails") { if (s.longFails > (int)v) c.fail("expect climbnav fails: " + std::to_string(s.longFails) + " long searches failed"); }
  else if (c.arg(2) == "worst") { if (s.worstMs > v) c.fail("expect climbnav worst: " + std::to_string(s.worstMs) + " ms"); }
  else c.fail("expect climbnav fails N | worst MS");
  return true;
}

bool expClimbPace(ScriptCtx& c) {
  const float want = (float)std::atof(c.arg(2).c_str()), k = c.game.pl().reliefK;
  if (std::fabs(k - want) > 0.02f) c.fail("expect climbpace " + c.arg(2) + ": the hero's relief pace is " + std::to_string(k));
  return true;
}
}  // namespace

EMB_SCRIPT_CMD("climbto", "climbto ramp|stair|earth|cliff [n]: stand above the n-th nearest way up (cliff: a face whose way up is 16-24 tiles along) (M11)", cmdClimbTo);
EMB_SCRIPT_CMD("climbguard", "climbguard [chase]: a guard on the picked way up's low side (chase: hostile to the hero) (M11)", cmdClimbGuard);
EMB_SCRIPT_CMD("climbparty", "climbparty [n]: n companions left on the picked way up's low side (M11)", cmdClimbParty);
EMB_SCRIPT_CMD("climbhero", "climbhero low|mid|top: the hero to the picked way up's foot, onto it, or past its top (M11)", cmdClimbHero);
EMB_SCRIPT_CMD("climbseek", "climbseek fall|stack: visit the start's and the nearest high landmarks' windows until one holds that feature; stand 5 tiles south of it (M11)", cmdClimbSeek);
EMB_SCRIPT_CMD("climbnav", "climbnav [reset]: print the path searches' counters (M11)", cmdClimbNav);
EMB_SCRIPT_CMD("expect:climbup", "expect climbup guard|party [tiles]: they stand on the picked way's high side near the hero (M11)", expClimbUp);
EMB_SCRIPT_CMD("expect:climbnav", "expect climbnav fails N | worst MS: the long path searches (M11)", expClimbNav);
EMB_SCRIPT_CMD("expect:climbpace", "expect climbpace F: the hero's relief pace (Actor::reliefK) (M11)", expClimbPace);
