// M11 "High Ground" TERRAIN lane: test-only script commands for reviewing the baked look of the relief (VISION_PLAN 11.2 /
// 11.3, 15.32) before the WORLD lane's generation lands every case.
//
//   reliefstamp [up]        write a relief review garden round the player into the loaded window (g.world.over.height,
//                           ground and props) and re-bake the chunks: a plateau one level up (+up, default 1) with
//                           curved and diagonal runs, a stacked two-level face (a higher plateau whose south edge
//                           meets the first one's), a hillside (HEIGHT_SLOPE) along its north side that ends in cliffs,
//                           road stairs (HEIGHT_STAIR) up its south face, an earth ramp up its east side, a switchback up the stacked
//                           face, and a pond on top whose stream pours over the south face (HEIGHT_FALL). The player
//                           is left on the plateau, 4 tiles north of the south face (the view frames the garden). The land keeps its biome (the faces' material).
//   reliefstamp slope [L]   the same garden with every one-level step a hillside (the stacked face stays a cliff)
//   reliefstamp N [L]       the garden N levels up (1..3), on base level L (default: the player's)
//   reliefstamp mesa W H    a plain plateau W x H tiles, all cliffs (W, H >= 6)
//   gotochunkcliff [n]      stand 4 tiles south of the n-th nearest south face that a chunk edge (global x a multiple
//                           of 32) crosses, the face running on 2 tiles either side if one is loaded (a seam would break it)
//   gotorock [n]            stand by the n-th nearest rock mass (a Rock tile whose 5x5 tiles are all rock)
//   reliefground <ground>   lay grass|tundra|snow|sand on the dry land within 14 tiles (biome and eco stay) and re-bake:
//                           a look at a land's painter where the generator rarely puts that ground (the mountains' scree)
//   reliefrebake            drop every baked chunk (they are painted again from the map as it is)
// Test only: never used by the game; the window's next shift regenerates the land.
#include <algorithm>
#include <cctype>
#include <cmath>
#include <mutex>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"

struct TerrainScriptOps {
  static void rebake(View& v) {
    v.clearChunks();
    std::lock_guard<std::mutex> lk(v.mu_);
    v.done_.clear();
  }
};

namespace {
uint32_t th(int x, int y, uint32_t s) {
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + s * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}

bool cmdReliefStamp(ScriptCtx& c) {
  Game& g = c.game;
  if (g.inside) g.debugLeave();
  Map& m = g.world.over;
  if (m.height.empty()) { c.fail("reliefstamp: no relief on this map"); return true; }
  const std::string kind = c.arg(1);
  const bool slopeAll = kind == "slope";
  const bool mesa = kind == "mesa";
  const int up = (!kind.empty() && std::isdigit((unsigned char)kind[0])) ? std::clamp(std::atoi(kind.c_str()), 1, 3) : 1;
  const int MW = mesa ? std::max(6, std::atoi(c.arg(2).c_str())) : 26, MH = mesa ? std::max(6, std::atoi(c.arg(3).c_str())) : 10;
  const int hx = (int)std::floor(g.pl().p.x / 16.0f), hy = (int)std::floor(g.pl().p.y / 16.0f);
  // the south face's row y0 (the plateau above it), centred on x0
  const int x0 = hx, y0 = hy + 4;
  // (the region reaches past the phone's view, so its own border, where the stamped base level meets the land's, stays
  // off screen)
  const int R0x = x0 - MW / 2 - 10, R1x = x0 + MW / 2 + 10, R0y = y0 - MH - 8, R1y = y0 + 12;
  if (R0x < 2 || R0y < 2 || R1x >= m.w - 2 || R1y >= m.h - 2) { c.fail("reliefstamp: too near the window's edge"); return true; }
  int base = (!mesa && !c.arg(2).empty()) ? std::clamp(std::atoi(c.arg(2).c_str()), 0, 6) : m.heightAt(hx, hy);
  base = std::min(base, 7 - up - 1);
  Ground land = m.at(hx, hy);
  if (groundSolid(land) || land == Ground::Road || land == Ground::Plaza || land == Ground::Bridge || land == Ground::Dirt) land = Ground::Grass;
  const uint32_t sd = 0x51A7u;
  const int W = R1x - R0x + 1, H = R1y - R0y + 1;
  std::vector<int> lev((size_t)W * H, base);
  std::vector<uint8_t> mark((size_t)W * H, 0);   // 1 earth ramp, 2 stairs
  std::vector<uint8_t> wet((size_t)W * H, 0), road((size_t)W * H, 0);
  auto I = [&](int x, int y) { return (size_t)(y - R0y) * W + (x - R0x); };
  // plateau A: an organic blob over rows y0-MH .. y0-1, its south edge straight in the middle, a diagonal run to the
  // south-west and a rounded south-east corner
  for (int y = y0 - MH; y < y0; y++)
    for (int x = x0 - MW / 2; x <= x0 + MW / 2; x++) {
      const int dx = x - x0, dyTop = y - (y0 - MH);
      bool in = true;
      if (!mesa) {
        const int wob = (int)(th(x / 3, 0, sd) % 3);
        if (dyTop < wob) in = false;                                        // a ragged north edge
        const int west = -MW / 2 + (int)(th(0, y / 2, sd + 1) % 3);
        const int east = MW / 2 - (int)(th(1, y / 2, sd + 2) % 3);
        if (dx < west || dx > east) in = false;
        if (dx < -7 && (y0 - 1 - y) < (-7 - dx)) in = false;               // the diagonal run to the south-west
        const int ex = dx - (MW / 2 - 5), ey = y - (y0 - 5);                // the rounded south-east corner
        if (ex > 0 && ey > 0 && ex * ex + ey * ey > 20) in = false;
      }
      if (in) lev[I(x, y)] = base + up;
    }
  if (!mesa) {
    // plateau B: a level higher, its south edge on A's (a stacked two-level face from x0-3 to x0+8)
    for (int y = y0 - 7; y < y0; y++)
      for (int x = x0 - 3; x <= x0 + 8; x++) {
        const int dx = x - x0;
        if (y < y0 - 7 + (int)(th(x, 1, sd + 3) % 2)) continue;
        if (dx >= 6 && y < y0 - 7 + (dx - 5) * 2) continue;   // a slanted north-east edge
        lev[I(x, y)] = base + up + 1;
      }
    // the switchback: a notch in the stacked face, a ledge of A's level two tiles deep: a ramp up the lower face onto
    // the ledge at its east end, then a ramp west from the ledge onto B
    for (int y = y0 - 2; y < y0; y++)
      for (int x = x0 + 4; x <= x0 + 8; x++) lev[I(x, y)] = base + up;
    for (int x = x0 + 7; x <= x0 + 8; x++) mark[I(x, y0)] = 1;
    mark[I(x0 + 4, y0 - 1)] = 1;
    // the pond on A and its stream down over the south face into a pool below
    for (int y = y0 - 8; y <= y0 - 6; y++)
      for (int x = x0 - 7; x <= x0 - 5; x++) wet[I(x, y)] = 1;
    for (int y = y0 - 5; y <= y0 + 4; y++)
      for (int x = x0 - 6; x <= x0 - 5; x++) wet[I(x, y)] = 1;
    for (int y = y0 + 3; y <= y0 + 6; y++)
      for (int x = x0 - 8; x <= x0 - 3; x++)
        if (!((y == y0 + 6 || y == y0 + 3) && (x == x0 - 8 || x == x0 - 3))) wet[I(x, y)] = 1;
    // the road stairs up the south face (the road runs on over the plateau) and an earth ramp up its east side
    for (int x = x0 + 9; x <= x0 + 11; x++) mark[I(x, y0)] = 2;
    for (int y = y0 - 6; y <= y0 + 9; y++) road[I(x0 + 10, y)] = 1;
    for (int y = y0 - 9; y <= y0 - 7; y++)
      for (int x = x0 + MW / 2 - 3; x <= x0 + MW / 2 + 1; x++)
        if (lev[I(x, y)] == base && lev[I(x - 1, y)] == base + up) mark[I(x, y)] = 1;
  }
  // the step bits on the lower tile of every 4-neighbour step
  for (int y = R0y + 1; y < R1y; y++)
    for (int x = R0x + 1; x < R1x; x++) {
      const int l = lev[I(x, y)];
      const int ln = lev[I(x, y - 1)], ls = lev[I(x, y + 1)], lw = lev[I(x - 1, y)], le = lev[I(x + 1, y)];
      uint8_t b = (uint8_t)l;
      if (ln > l || ls > l || lw > l || le > l) {
        const int mk = mark[I(x, y)];
        const bool hill = !mesa && (slopeAll || (ls > l && std::abs(x - x0) <= MW / 2 - 4)) && std::max({ln, ls, lw, le}) == l + 1;
        if (wet[I(x, y)]) b |= Map::HEIGHT_CLIFF | Map::HEIGHT_FALL;
        else if (mk == 1) b |= Map::HEIGHT_RAMP;
        else if (mk == 2) b |= Map::HEIGHT_RAMP | Map::HEIGHT_STAIR;
        else if (hill) b |= Map::HEIGHT_SLOPE;
        else b |= Map::HEIGHT_CLIFF;
      }
      const int lx = x, ly = y;
      m.height[(size_t)ly * m.w + lx] = b;
      m.setG(lx, ly, wet[I(x, y)] ? Ground::Water : road[I(x, y)] ? Ground::Road : land);
      m.setP(lx, ly, 0);
      if (!m.deco.empty()) m.deco[(size_t)ly * m.w + lx] = 0;
      if (!m.blend.empty() && (m.blend[(size_t)ly * m.w + lx] >> 4) >= 0xD) m.blend[(size_t)ly * m.w + lx] = 0;
    }
  m.rebuildSolid();
  TerrainScriptOps::rebake(c.view);
  g.teleportGlobal(g.world.ox + x0, g.world.oy + y0 - 4);
  g.mode = Mode::Play;
  c.view.snap(g);
  std::printf("script: reliefstamp %s at %d,%d (base level %d, +%d; south face row %d)\n", kind.empty() ? "garden" : kind.c_str(),
              g.world.ox + x0, g.world.oy + y0, base, up, g.world.oy + y0);
  return true;
}

// gotochunkcliff [n]: the n-th nearest south face (CLIFF tiles running on 2 tiles either side, or fewer if none is loaded) that a chunk
// edge crosses (global x a multiple of 32), looked at from 4 tiles south: a seam would show as a break in the face
bool cmdGotoChunkCliff(ScriptCtx& c) {
  Game& g = c.game;
  if (g.inside) g.debugLeave();
  const Map& m = g.world.over;
  if (m.height.empty()) { c.fail("gotochunkcliff: no relief on this map"); return true; }
  const int want = c.arg(1).empty() ? 0 : std::max(0, std::atoi(c.arg(1).c_str()));
  const int hx = (int)std::floor(g.pl().p.x / 16.0f), hy = (int)std::floor(g.pl().p.y / 16.0f);
  struct T { int d, x, y; };
  std::vector<T> ts;
  auto face = [&](int x, int y) {
    const uint8_t b = m.heightBits(x, y);
    return (b & Map::HEIGHT_CLIFF) && !groundWater(m.at(x, y)) && m.heightAt(x, y - 1) > (b & Map::HEIGHT_LEVEL);
  };
  for (int reach = 2; reach >= 0 && (int)ts.size() <= want; reach--) {   // (a shorter run if no long one is loaded)
    ts.clear();
    for (int y = 8; y < m.h - 8; y++)
      for (int x = 8; x < m.w - 8; x++) {
        if (((g.world.ox + x) & 31) != 0) continue;
        bool run = true;
        for (int k = -reach; k <= reach && run; k++) run = face(x + k, y);
        if (run) ts.push_back({(x - hx) * (x - hx) + (y - hy) * (y - hy), x, y});
      }
  }
  std::sort(ts.begin(), ts.end(), [](const T& a, const T& b) { return a.d < b.d || (a.d == b.d && (a.y < b.y || (a.y == b.y && a.x < b.x))); });
  if ((int)ts.size() <= want) { c.fail("gotochunkcliff: " + std::to_string(ts.size()) + " faces on a chunk edge in the loaded window"); return true; }
  const T& t = ts[(size_t)want];
  g.teleportGlobal(g.world.ox + t.x, g.world.oy + t.y + 4);
  g.mode = Mode::Play;
  c.view.snap(g);
  std::printf("script: gotochunkcliff -> the face at %d,%d (chunk edge x = %d)\n", g.world.ox + t.x, g.world.oy + t.y, g.world.ox + t.x);
  return true;
}

// gotorock [n]: stand just south of the n-th nearest rock mass (a Rock tile whose 5x5 tiles are all rock: a massif's
// core or a crag), so the view shows its tiers, faces and scree
bool cmdGotoRock(ScriptCtx& c) {
  Game& g = c.game;
  if (g.inside) g.debugLeave();
  const Map& m = g.world.over;
  const int want = c.arg(1).empty() ? 0 : std::max(0, std::atoi(c.arg(1).c_str()));
  const int hx = (int)std::floor(g.pl().p.x / 16.0f), hy = (int)std::floor(g.pl().p.y / 16.0f);
  struct T { int d, x, y; };
  std::vector<T> ts;
  for (int y = 8; y < m.h - 8; y++)
    for (int x = 8; x < m.w - 8; x++) {
      bool all = true;
      for (int oy = -2; oy <= 2 && all; oy++)
        for (int ox = -2; ox <= 2 && all; ox++) all = m.at(x + ox, y + oy) == Ground::Rock;
      if (all) ts.push_back({(x - hx) * (x - hx) + (y - hy) * (y - hy), x, y});
    }
  std::sort(ts.begin(), ts.end(), [](const T& a, const T& b) { return a.d < b.d || (a.d == b.d && (a.y < b.y || (a.y == b.y && a.x < b.x))); });
  std::vector<T> picks;
  for (const T& t : ts) {
    bool same = false;
    for (const T& q : picks) if (std::abs(q.x - t.x) < 24 && std::abs(q.y - t.y) < 24) same = true;
    if (!same) picks.push_back(t);
    if ((int)picks.size() > want) break;
  }
  if ((int)picks.size() <= want) { c.fail("gotorock: " + std::to_string(picks.size()) + " rock masses in the loaded window"); return true; }
  const T& t = picks[(size_t)want];
  int sy = t.y;
  while (sy < t.y + 30 && sy < m.h - 2 && (m.at(t.x, sy) == Ground::Rock || m.blocked(t.x, sy))) sy++;
  g.teleportGlobal(g.world.ox + t.x, g.world.oy + std::min(sy, t.y + 4));
  g.mode = Mode::Play;
  c.view.snap(g);
  std::printf("script: gotorock -> the rock at %d,%d (%s)\n", g.world.ox + t.x, g.world.oy + t.y, m.biomeAt(t.x, t.y) == Biome::Mountain ? "a massif" : "a crag");
  return true;
}

// reliefground grass|tundra|snow|sand: lay that ground on the dry land within 14 tiles of the player (its biome and eco
// stay) and re-bake: a look at a land's painter where the generator rarely puts that ground (the mountains' scree)
bool cmdReliefGround(ScriptCtx& c) {
  Game& g = c.game;
  Map& m = g.world.over;
  const std::string w = c.arg(1);
  const Ground to = w == "tundra" ? Ground::Tundra : w == "snow" ? Ground::Snow : w == "sand" ? Ground::Sand : w == "grass" ? Ground::Grass : Ground::Void;
  if (to == Ground::Void) { c.fail("reliefground grass|tundra|snow|sand"); return true; }
  const int hx = (int)std::floor(g.pl().p.x / 16.0f), hy = (int)std::floor(g.pl().p.y / 16.0f);
  for (int y = hy - 14; y <= hy + 14; y++)
    for (int x = hx - 14; x <= hx + 14; x++) {
      const Ground q = m.at(x, y);
      if (!m.in(x, y) || groundWater(q) || q == Ground::Road || q == Ground::Plaza || q == Ground::Bridge || q == Ground::Rock) continue;
      m.setG(x, y, to);
    }
  TerrainScriptOps::rebake(c.view);
  return true;
}

bool cmdReliefRebake(ScriptCtx& c) {
  TerrainScriptOps::rebake(c.view);
  return true;
}
}  // namespace

EMB_SCRIPT_CMD("reliefstamp", "reliefstamp [1|2|3|slope [base]|mesa W H]: write a relief review garden round the player and re-bake (M11 TERRAIN, test only)", cmdReliefStamp);
EMB_SCRIPT_CMD("gotochunkcliff", "gotochunkcliff [n]: look at the n-th nearest south face a chunk edge crosses (M11 TERRAIN, test only)", cmdGotoChunkCliff);
EMB_SCRIPT_CMD("gotorock", "gotorock [n]: stand by the n-th nearest rock mass (M11 TERRAIN, test only)", cmdGotoRock);
EMB_SCRIPT_CMD("reliefground", "reliefground grass|tundra|snow|sand: lay that ground round the player and re-bake (M11 TERRAIN, test only)", cmdReliefGround);
EMB_SCRIPT_CMD("reliefrebake", "reliefrebake: drop every baked terrain chunk so it is painted again (M11 TERRAIN, test only)", cmdReliefRebake);
