// M11 "High Ground", FALLS lane: test commands for the live relief (render_relief.cpp). Scripts only; never saves.
//
//   gotofall <kind> [n] [far] stand where the n-th nearest waterfall of that kind in the loaded window shows well (4
//                            tiles south of it, on dry walkable ground): north (down a face toward the viewer) | side
//                            (a cascade from the west or east) | rim (over the rim, away from the viewer) | frozen |
//                            wide (3+ tiles across) | tall (2+ levels) | lake (spilling from a plateau lake) | any.
//                            Falls nearer than 10 tiles to an earlier pick are the same fall. "far": when the loaded
//                            window has none, up to 24 more windows are probed in a spiral 200 tiles apart.
//   fallfx                   print the relief view's last frame (falls, mist puffs, sprays, the roar, ms)
//   expect fallfx roar X     the roar handed to the audio last frame is at least X (expect fallfx quiet: under 0.02;
//                            expect fallfx puffs N: at least N mist puffs drawn; expect fallfx ms X: the relief draw
//                            took at most X ms)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"

// render_relief.cpp
int reliefFallDir(const Map& m, int tx, int ty);
bool reliefFallFrozen(const Map& m, int tx, int ty, int dir);

namespace {
int fallWidth(const Map& m, int x, int y, int dir) {
  int w = 1;
  const int sx = (dir == 0 || dir == 3) ? 1 : 0, sy = 1 - sx;
  for (int s = -1; s <= 1; s += 2)
    for (int k = 1; k <= 8; k++) {
      bool hit = false;
      for (int o = -1; o <= 1 && !hit; o++) hit = reliefFallDir(m, x + s * k * sx + o * sy, y + s * k * sy + o * sx) == dir;
      if (!hit) break;
      w++;
    }
  return w;
}
bool lakeAbove(const Map& m, int x, int y, int dir) {
  const int ux = x + (dir == 1 ? -1 : dir == 2 ? 1 : 0), uy = y + (dir == 0 ? -1 : dir == 3 ? 1 : 0);
  const int l = m.heightAt(ux, uy);
  int wet = 0;
  for (int oy = -3; oy <= 3; oy++)
    for (int ox = -3; ox <= 3; ox++) {
      const int qx = ux + ox + (dir == 1 ? -3 : dir == 2 ? 3 : 0), qy = uy + oy + (dir == 0 ? -3 : dir == 3 ? 3 : 0);
      if (m.heightAt(qx, qy) == l && groundWater(m.at(qx, qy))) wet++;
    }
  return wet >= 30;
}
bool kindOk(const Map& m, int x, int y, const std::string& k) {
  const int d = reliefFallDir(m, x, y);
  if (d < 0) return false;
  if (k == "any") return true;
  if (k == "north") return d == 0 && !reliefFallFrozen(m, x, y, d);
  if (k == "side") return (d == 1 || d == 2) && !reliefFallFrozen(m, x, y, d);
  if (k == "rim") return d == 3 && !reliefFallFrozen(m, x, y, d);
  if (k == "frozen") return reliefFallFrozen(m, x, y, d);
  if (k == "wide") return d == 0 && fallWidth(m, x, y, d) >= 3;
  if (k == "tall") {
    const int ux = x + (d == 1 ? -1 : d == 2 ? 1 : 0), uy = y + (d == 0 ? -1 : d == 3 ? 1 : 0);
    return m.heightAt(ux, uy) - m.heightAt(x, y) >= 2;
  }
  if (k == "lake") return lakeAbove(m, x, y, d);
  return false;
}

bool cmdGotoFall(ScriptCtx& c) {
  Game& g = c.game;
  if (g.inside) g.debugLeave();
  const std::string k = c.arg(1);
  const int want = c.arg(2).empty() ? 0 : std::max(0, std::atoi(c.arg(2).c_str()));
  if (g.world.over.height.empty()) { c.fail("gotofall: no relief on this map"); return true; }
  struct T { int d, x, y; };
  std::vector<T> picks;
  // the loaded window first; with "far", up to 24 windows more in a spiral round the start (each teleport loads one)
  const bool far = c.arg(3) == "far";
  const int32_t gx0 = g.world.ox + (int32_t)std::floor(g.pl().p.x / 16.0f), gy0 = g.world.oy + (int32_t)std::floor(g.pl().p.y / 16.0f);
  for (int probe = 0; probe <= (far ? 24 : 0); probe++) {
    if (probe > 0) {
      const int ring = (probe + 7) / 8, slot = (probe - 1) % 8;
      static const int DX[8] = {1, 1, 0, -1, -1, -1, 0, 1}, DY[8] = {0, 1, 1, 1, 0, -1, -1, -1};
      g.teleportGlobal(gx0 + DX[slot] * ring * 200, gy0 + DY[slot] * ring * 200);
    }
    const Map& mm = g.world.over;
    const int px = (int)std::floor(g.pl().p.x / 16.0f), py = (int)std::floor(g.pl().p.y / 16.0f);
    std::vector<T> ts;
    for (int y = 6; y < mm.h - 6; y++)
      for (int x = 6; x < mm.w - 6; x++)
        if (mm.fallAt(x, y) && kindOk(mm, x, y, k)) ts.push_back({(x - px) * (x - px) + (y - py) * (y - py), x, y});
    std::sort(ts.begin(), ts.end(), [](const T& a, const T& b) { return a.d < b.d || (a.d == b.d && (a.y < b.y || (a.y == b.y && a.x < b.x))); });
    picks.clear();
    for (const T& t : ts) {
      bool same = false;
      for (const T& p : picks) if (std::abs(p.x - t.x) < 10 && std::abs(p.y - t.y) < 10) same = true;
      if (same) continue;
      picks.push_back(t);
      if ((int)picks.size() > want) break;
    }
    if ((int)picks.size() > want) break;
  }
  const Map& m = g.world.over;
  if ((int)picks.size() <= want) { c.fail("gotofall " + k + ": " + std::to_string(picks.size()) + (far ? " in 25 windows" : " in the loaded window")); return true; }
  const T& t = picks[(size_t)want];
  int sx = t.x, sy = t.y + 4;
  bool ok = false;
  for (int dy = 4; dy <= 7 && !ok; dy++)
    for (int dx = 0; dx <= 4 && !ok; dx++)
      for (int s = -1; s <= 1 && !ok; s += 2)
        if (!m.blocked(t.x + dx * s, t.y + dy) && !groundWater(m.at(t.x + dx * s, t.y + dy))) { sx = t.x + dx * s; sy = t.y + dy; ok = true; }
  for (int r = 2; r <= 7 && !ok; r++)
    for (int dy = -r; dy <= r && !ok; dy++)
      for (int dx = -r; dx <= r && !ok; dx++)
        if (!m.blocked(t.x + dx, t.y + dy) && !groundWater(m.at(t.x + dx, t.y + dy))) { sx = t.x + dx; sy = t.y + dy; ok = true; }
  const int d = reliefFallDir(m, t.x, t.y);
  const int ux = t.x + (d == 1 ? -1 : d == 2 ? 1 : 0), uy = t.y + (d == 0 ? -1 : d == 3 ? 1 : 0);
  const int drop = m.heightAt(ux, uy) - m.heightAt(t.x, t.y), wid = fallWidth(m, t.x, t.y, d);
  const bool fz = reliefFallFrozen(m, t.x, t.y, d);
  const int32_t fgx = g.world.ox + t.x, fgy = g.world.oy + t.y, sgx = g.world.ox + sx, sgy = g.world.oy + sy;
  g.teleportGlobal(sgx, sgy);
  g.mode = Mode::Play;
  c.view.snap(g);
  std::printf("script: gotofall %s -> the fall at %d,%d (dir %d, drop %d, width %d%s), standing at %d,%d\n", k.c_str(), fgx, fgy, d, drop, wid,
              fz ? ", frozen" : "", sgx, sgy);
  return true;
}

bool cmdFallFx(ScriptCtx& c) {
  const View::ReliefViewStats s = c.view.reliefViewStats();
  std::printf("script: fallfx falls %d puffs %d sprays %d roar %.3f ms %.3f\n", s.falls, s.mistPuffs, s.sprays, s.roar, s.ms);
  return true;
}

bool expFallFx(ScriptCtx& c) {
  const View::ReliefViewStats s = c.view.reliefViewStats();
  const std::string w = c.arg(2);
  const float x = (float)std::atof(c.arg(3).c_str());
  char buf[160];
  if (w == "roar") {
    std::snprintf(buf, sizeof buf, "expect fallfx roar %s: %.3f", c.arg(3).c_str(), s.roar);
    if (s.roar < x) c.fail(buf);
  } else if (w == "quiet") {
    std::snprintf(buf, sizeof buf, "expect fallfx quiet: the roar is %.3f", s.roar);
    if (s.roar >= 0.02f) c.fail(buf);
  } else if (w == "puffs") {
    std::snprintf(buf, sizeof buf, "expect fallfx puffs %s: %d drawn", c.arg(3).c_str(), s.mistPuffs);
    if (s.mistPuffs < (int)x) c.fail(buf);
  } else if (w == "ms") {
    std::snprintf(buf, sizeof buf, "expect fallfx ms %s: %.3f ms", c.arg(3).c_str(), s.ms);
    if (s.ms > x) c.fail(buf);
  } else
    c.fail("expect fallfx roar X | quiet | puffs N | ms X");
  return true;
}
}  // namespace

EMB_SCRIPT_CMD("gotofall", "gotofall north|side|rim|frozen|wide|tall|lake|any [n]: stand south of the n-th nearest waterfall of a kind (M11 FALLS)", cmdGotoFall);
EMB_SCRIPT_CMD("fallfx", "fallfx: print the relief view's last frame: falls, mist puffs, sprays, the roar, ms (M11 FALLS)", cmdFallFx);
EMB_SCRIPT_CMD("expect:fallfx", "expect fallfx roar X | quiet | puffs N | ms X: the relief view's last frame (M11 FALLS)", expFallFx);
