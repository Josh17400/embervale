// M11 "High Ground" script commands (VISION_PLAN 11, 15.32). WORLD lane owns this file (the lead wrote phase A); the
// other lanes add their own script_*.cpp.
//
//   gotorelief <kind> [n]   stand where the n-th nearest (default 0) relief feature of the loaded window shows well:
//                           cliff | stack (a face of 2+ levels) | ramp (earth) | stair (stone, HEIGHT_STAIR) | slope |
//                           fall (a waterfall) | high (the highest level loaded) | plateau (a flat level-4+ field) |
//                           switchback (a way up a stacked face in two flights with a landing) | roadsw (one of a
//                           road's or track's: stone stairs) |
//                           summit (the nearest summit shrine, SPF_SUMMIT, searched in the region plans). Faces, ramps,
//                           stairs and falls are looked at from 3 tiles south of them (the lower side), the rest from
//                           where they are. Features nearer than 10 tiles to an earlier pick are the same feature.
//   expect relief walk      the player's tile is walkable (expect relief level N: the player stands on level N or
//                           higher; expect relief near <kind>: a feature of that kind within 12 tiles)
//   expect reliefview falls N   at least N fall tiles were drawn last frame (View::reliefViewStats)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"
#include "rpg/world/source.h"

namespace {
bool kindAt(const Map& m, int x, int y, const std::string& k, int top) {
  const uint8_t b = m.heightBits(x, y);
  const int l = b & Map::HEIGHT_LEVEL;
  if (k == "cliff") return (b & Map::HEIGHT_CLIFF) && !(b & Map::HEIGHT_FALL) && !groundWater(m.at(x, y));
  if (k == "stack") return (b & Map::HEIGHT_CLIFF) && m.heightAt(x, y - 1) >= l + 2;
  if (k == "ramp") return (b & Map::HEIGHT_RAMP) && !(b & Map::HEIGHT_STAIR);
  if (k == "stair") return (b & Map::HEIGHT_RAMP) && (b & Map::HEIGHT_STAIR);
  if (k == "slope") return (b & Map::HEIGHT_SLOPE) != 0;
  // (WORLD lane) a stairway in two flights up a stacked face: a way up whose upper side is a level above it and whose
  // lower side is another way up a level below (the landing); "roadsw" only where it is built stone stairs (a road's)
  if (k == "switchback" || k == "roadsw") {
    if (!(b & Map::HEIGHT_RAMP) || (k == "roadsw" && !(b & Map::HEIGHT_STAIR))) return false;
    static const int d[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
    for (auto& v : d) {
      const uint8_t lo = m.heightBits(x + v[0], y + v[1]);
      if ((lo & Map::HEIGHT_RAMP) && (lo & Map::HEIGHT_LEVEL) == l - 1 && m.heightAt(x - v[0], y - v[1]) == l + 1) return true;
    }
    return false;
  }
  if (k == "fall") return (b & Map::HEIGHT_FALL) != 0;
  if (k == "high") return l == top && !(b & Map::HEIGHT_STEP) && !m.blocked(x, y);
  if (k == "plateau") {
    if (l < 4 || m.blocked(x, y)) return false;
    for (int oy = -3; oy <= 3; oy++)
      for (int ox = -3; ox <= 3; ox++) {
        const uint8_t q = m.heightBits(x + ox, y + oy);
        if ((q & Map::HEIGHT_LEVEL) != l || (q & Map::HEIGHT_STEP)) return false;
      }
    return true;
  }
  return false;
}
bool lookFromSouth(const std::string& k) {
  return k == "cliff" || k == "stack" || k == "ramp" || k == "stair" || k == "fall" || k == "switchback" || k == "roadsw";
}

bool cmdGotoRelief(ScriptCtx& c) {
  Game& g = c.game;
  if (g.inside) g.debugLeave();
  const std::string k = c.arg(1);
  const int want = c.arg(2).empty() ? 0 : std::max(0, std::atoi(c.arg(2).c_str()));
  if (k == "summit") {
    if (!g.world.src) { c.fail("gotorelief summit: no endless source"); return true; }
    ew::EndlessSource& A = *g.world.src;
    const int32_t px = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), py = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
    struct F { double d; ew::SitePlan s; };
    std::vector<F> fs;
    for (int32_t ry = ew::regionOf(py) - 6; ry <= ew::regionOf(py) + 6; ry++)
      for (int32_t rx = ew::regionOf(px) - 6; rx <= ew::regionOf(px) + 6; rx++)
        for (const ew::SitePlan& s : A.region(rx, ry).sites)
          if (s.flags & ew::SPF_SUMMIT) fs.push_back({std::hypot((double)(s.ex - px), (double)(s.ey - py)), s});
    std::sort(fs.begin(), fs.end(), [](const F& a, const F& b) { return a.d < b.d || (a.d == b.d && a.s.id < b.s.id); });
    if ((int)fs.size() <= want) { c.fail("gotorelief summit: " + std::to_string(fs.size()) + " summit shrines within 6 regions"); return true; }
    const ew::SitePlan& s = fs[(size_t)want].s;
    g.teleportGlobal(s.ex, s.ey + 3);
    g.mode = Mode::Play;
    c.view.snap(g);
    std::printf("script: gotorelief summit -> %s at %d,%d\n", s.name.c_str(), s.ex, s.ey);
    return true;
  }
  const Map& m = g.world.over;
  if (m.height.empty()) { c.fail("gotorelief: no relief on this map"); return true; }
  int top = 0;
  for (size_t i = 0; i < m.height.size(); i++) top = std::max(top, (int)(m.height[i] & Map::HEIGHT_LEVEL));
  const int px = (int)std::floor(g.pl().p.x / 16.0f), py = (int)std::floor(g.pl().p.y / 16.0f);
  struct T { int d, x, y; };
  std::vector<T> ts;
  for (int y = 4; y < m.h - 4; y++)
    for (int x = 4; x < m.w - 4; x++)
      if (kindAt(m, x, y, k, top)) ts.push_back({(x - px) * (x - px) + (y - py) * (y - py), x, y});
  if (ts.empty() && k != "cliff" && k != "stack" && k != "ramp" && k != "stair" && k != "slope" && k != "fall" && k != "high" && k != "plateau" &&
      k != "switchback" && k != "roadsw") {
    c.fail("gotorelief: cliff|stack|ramp|stair|slope|fall|high|plateau|switchback|roadsw|summit");
    return true;
  }
  std::sort(ts.begin(), ts.end(), [](const T& a, const T& b) { return a.d < b.d || (a.d == b.d && (a.y < b.y || (a.y == b.y && a.x < b.x))); });
  std::vector<T> picks;
  for (const T& t : ts) {
    bool same = false;
    for (const T& p : picks) if (std::abs(p.x - t.x) < 10 && std::abs(p.y - t.y) < 10) same = true;
    if (same) continue;
    picks.push_back(t);
    if ((int)picks.size() > want) break;
  }
  if ((int)picks.size() <= want) { c.fail("gotorelief " + k + ": " + std::to_string(picks.size()) + " in the loaded window"); return true; }
  const T& t = picks[(size_t)want];
  int sx = t.x, sy = t.y;
  if (lookFromSouth(k)) {
    // the nearest walkable tile 3..6 tiles south (the lower side), else anywhere within 5
    bool ok = false;
    for (int dy = 3; dy <= 6 && !ok; dy++)
      for (int dx = 0; dx <= 3 && !ok; dx++)
        for (int s = -1; s <= 1 && !ok; s += 2)
          if (!m.blocked(t.x + dx * s, t.y + dy) && !groundWater(m.at(t.x + dx * s, t.y + dy))) { sx = t.x + dx * s; sy = t.y + dy; ok = true; }
    for (int r = 1; r <= 5 && !ok; r++)
      for (int dy = -r; dy <= r && !ok; dy++)
        for (int dx = -r; dx <= r && !ok; dx++)
          if (!m.blocked(t.x + dx, t.y + dy) && !groundWater(m.at(t.x + dx, t.y + dy))) { sx = t.x + dx; sy = t.y + dy; ok = true; }
  }
  const int32_t gx = g.world.ox + sx, gy = g.world.oy + sy, fx = g.world.ox + t.x, fy = g.world.oy + t.y;
  const int flv = m.heightAt(t.x, t.y);
  g.teleportGlobal(gx, gy);
  g.mode = Mode::Play;
  c.view.snap(g);
  std::printf("script: gotorelief %s -> the feature at %d,%d (level %d), standing at %d,%d\n", k.c_str(), fx, fy, flv, gx, gy);
  return true;
}

bool expRelief(ScriptCtx& c) {
  Game& g = c.game;
  const Map& m = g.world.over;
  const int px = (int)std::floor(g.pl().p.x / 16.0f), py = (int)std::floor(g.pl().p.y / 16.0f);
  const std::string w = c.arg(2);
  if (w == "walk") {
    if (m.blocked(px, py)) c.fail("expect relief walk: the player's tile is blocked");
  } else if (w == "level") {
    const int n = std::atoi(c.arg(3).c_str());
    if (m.heightAt(px, py) < n) c.fail("expect relief level " + c.arg(3) + ": the player stands on level " + std::to_string(m.heightAt(px, py)));
  } else if (w == "near") {
    int top = 0;
    for (size_t i = 0; i < m.height.size(); i++) top = std::max(top, (int)(m.height[i] & Map::HEIGHT_LEVEL));
    bool any = false;
    for (int y = py - 12; y <= py + 12 && !any; y++)
      for (int x = px - 12; x <= px + 12 && !any; x++) any = kindAt(m, x, y, c.arg(3), top);
    if (!any) c.fail("expect relief near " + c.arg(3) + ": none within 12 tiles");
  } else
    c.fail("expect relief walk | level N | near <kind>");
  return true;
}

bool expReliefView(ScriptCtx& c) {
  const View::ReliefViewStats s = c.view.reliefViewStats();
  if (c.arg(2) == "falls") {
    const int n = std::atoi(c.arg(3).c_str());
    if (s.falls < n) c.fail("expect reliefview falls " + c.arg(3) + ": " + std::to_string(s.falls) + " drawn");
  } else
    c.fail("expect reliefview falls N");
  return true;
}
}  // namespace

EMB_SCRIPT_CMD("gotorelief", "gotorelief cliff|stack|ramp|stair|slope|fall|high|plateau|switchback|roadsw|summit [n]: stand by the n-th nearest relief feature (M11)", cmdGotoRelief);
EMB_SCRIPT_CMD("expect:relief", "expect relief walk | level N | near <kind>: the relief round the player (M11)", expRelief);
EMB_SCRIPT_CMD("expect:reliefview", "expect reliefview falls N: fall tiles drawn last frame (M11)", expReliefView);
