// M7b Cities script commands (rpg/view/script_api.h). CITY lane owns this file (phase A: the lead's first cut).
//   urb [r]                     print the urban fabric (Map::urb, rpg/world/urban.h) within r tiles of the hero (default
//                               60): tiles per street class, per quarter and per upkeep, and the row houses near by
//   expect urb <class> <min>    at least min tiles of that class (avenue, street, lane, alley, sidewalk, gutter, square,
//                               close, park, parkpath, garden, court, quay) within 60 tiles; "expect urb rows <min>":
//                               buildings with party walls (Bldg::row) in the window
//   cityspot <place> [n]        put the hero at a place of the city he stands in: heart, gate (the n-th nearest, just
//                               inside), avenue (halfway from the heart to that gate), park, rich, poor, craft (a street
//                               of that quarter), seat (before the seat of power), close, well (the n-th well-square),
//                               terrace (before the n-th terrace's west end)
//   cityprops [south N]         test only: on the nearest open stretch round the hero, clear a stage and set out every
//                               M7b city prop (Bollard .. Canopy) in a row, a run of garden wall and of hedge under it
//                               (the CITYART lane's in-game gallery: the real ground, light, night and shadows)
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
#include <SDL3/SDL.h>
#include "rpg/art/art_props.h"
#include "rpg/build/blueprint.h"
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"
#include "rpg/world/urban.h"

namespace {

struct UrbCount {
  int cls[16] = {}, quarter[4] = {}, upkeep[4] = {}, rows = 0, tiles = 0;
};
UrbCount countUrb(const Game& g, int r) {
  UrbCount u;
  const Map& m = g.world.over;
  if (g.inside) return u;
  const int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
  for (int y = py - r; y <= py + r; y++)
    for (int x = px - r; x <= px + r; x++) {
      const uint8_t b = m.urbAt(x, y);
      if (!b) continue;
      u.tiles++;
      u.cls[(int)ew::urbClass(b) & 15]++;
      u.quarter[(int)ew::urbQuarter(b) & 3]++;
      u.upkeep[ew::urbUpkeep(b) & 3]++;
    }
  for (const Bldg& b : m.bldgs) if (b.row) u.rows++;
  return u;
}

bool cmdUrb(ScriptCtx& c) {
  const int r = c.arg(1).empty() ? 60 : std::max(1, std::atoi(c.arg(1).c_str()));
  const UrbCount u = countUrb(c.game, r);
  std::printf("urb: %d tiles within %d:", u.tiles, r);
  for (int k = 1; k < (int)ew::Urb::COUNT; k++) if (u.cls[k]) std::printf(" %s %d", ew::urbName((ew::Urb)k), u.cls[k]);
  std::printf(" | quarters centre %d rich %d craft %d poor %d | upkeep %d %d %d %d | row houses in the window %d\n", u.quarter[0], u.quarter[1],
              u.quarter[2], u.quarter[3], u.upkeep[0], u.upkeep[1], u.upkeep[2], u.upkeep[3], u.rows);
  if (c.arg(2) == "map" && !c.game.inside) {   // (fixer M7b r2, debug) the fabric as letters (B a building, P a prop on open ground)
    static const char L[16] = {'.', 'A', 'S', 'l', 'a', 'w', 'g', 'Q', 'C', 'K', 'k', 'G', 'O', 'U', '?', '?'};
    const Map& m = c.game.world.over;
    const int px = (int)std::floor(c.game.pl().p.x / TILE), py = (int)std::floor(c.game.pl().p.y / TILE);
    for (int y = py - r; y <= py + r; y++) {
      std::string line;
      for (int x = px - r; x <= px + r; x++) {
        const uint8_t b = m.urbAt(x, y);
        char k = L[(int)ew::urbClass(b) & 15];
        if (!b && m.in(x, y) && m.bldgAt[(size_t)y * m.w + x] >= 0) k = 'B';
        else if (!b && m.propAt(x, y)) k = 'p';
        else if (b && m.propAt(x, y)) k = (char)(k == '.' ? 'p' : (k >= 'a' ? k - 32 : k + 32));
        line += (x == px && y == py) ? '@' : k;
      }
      std::printf("urbmap %4d: %s\n", y, line.c_str());
    }
  }
  return true;
}
EMB_SCRIPT_CMD("urb", "urb [r] [map]: print the urban fabric (street classes, quarters, upkeep) within r tiles and the row houses (M7b CITY)", cmdUrb);

bool expUrb(ScriptCtx& c) {
  const std::string what = c.arg(2);
  const int want = std::atoi(c.arg(3).c_str());
  if (what == "none") {   // (M7b CITY) a village keeps no urban fabric: none within 30 tiles of the hero
    const UrbCount v = countUrb(c.game, 30);
    if (v.tiles) c.fail("expect urb none: " + std::to_string(v.tiles) + " fabric tiles within 30");
    return true;
  }
  const UrbCount u = countUrb(c.game, 60);
  if (what == "rows") {
    if (u.rows < want) c.fail("expect urb rows: " + std::to_string(u.rows) + " row houses, wanted " + std::to_string(want));
    return true;
  }
  for (int k = 1; k < (int)ew::Urb::COUNT; k++)
    if (what == ew::urbName((ew::Urb)k)) {
      if (u.cls[k] < want) c.fail("expect urb " + what + ": " + std::to_string(u.cls[k]) + " tiles, wanted " + std::to_string(want));
      return true;
    }
  c.fail("expect urb: unknown class " + what);
  return true;
}
EMB_SCRIPT_CMD("expect:urb", "expect urb <class|rows> <min>: urban fabric tiles of a class within 60 tiles, or row houses in the window (M7b CITY)", expUrb);

// cityspot <heart|gate|avenue|park|rich|poor|craft|seat|close|well|terrace> [n]: put the hero at that place of the
// settlement he stands in (from its urban fabric, buildings and gates within 120 tiles): the square at the heart, just
// inside the n-th nearest gate, on an avenue halfway from the heart to it, on a path of the park, a street of the rich,
// the poor or the craft quarter, before the seat of power, in the temple close, at a well-square, before a terrace
bool cmdCitySpot(ScriptCtx& c) {
  Game& g = c.game;
  const Map& m = g.world.over;
  if (g.inside) { c.fail("cityspot: indoors"); return true; }
  const std::string what = c.arg(1);
  const int nth = c.arg(2).empty() ? 0 : std::max(0, std::atoi(c.arg(2).c_str()));
  const int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
  const int R = 120;
  // the settlement: the nearest one's heart
  int hx = px, hy = py, bestD = 1 << 30;
  for (const Site& s : g.world.sites) {
    if (!s.settlement()) continue;
    const int d = (s.ex - px) * (s.ex - px) + (s.ey - py) * (s.ey - py);
    if (d < bestD) { bestD = d; hx = s.ex; hy = s.ey; }
  }
  auto walk = [&](int x, int y) { return m.in(x, y) && !m.blocked(x, y); };
  // the walkable tile nearest (x, y) that satisfies pred (a spiral out to r)
  auto nearest = [&](int x, int y, int r, auto pred, int& ox, int& oy) {
    for (int k = 0; k <= r; k++)
      for (int oy2 = -k; oy2 <= k; oy2++)
        for (int ox2 = -k; ox2 <= k; ox2++) {
          if (std::max(std::abs(ox2), std::abs(oy2)) != k) continue;
          const int qx = x + ox2, qy = y + oy2;
          if (walk(qx, qy) && pred(qx, qy)) { ox = qx; oy = qy; return true; }
        }
    return false;
  };
  auto centroid = [&](auto pred, int& ox, int& oy) {
    long sx = 0, sy = 0, n = 0;
    for (int y = hy - R; y <= hy + R; y++)
      for (int x = hx - R; x <= hx + R; x++) if (m.in(x, y) && pred(x, y)) { sx += x; sy += y; n++; }
    if (!n) return false;
    ox = (int)(sx / n); oy = (int)(sy / n);
    return true;
  };
  auto cls = [&](int x, int y) { return ew::urbClass(m.urbAt(x, y)); };
  auto carriage = [&](int x, int y) { return ew::urbCarriage(cls(x, y)) || cls(x, y) == ew::Urb::Gutter; };
  int tx = -1, ty = -1;
  bool ok = false;
  if (what == "heart") ok = nearest(hx, hy, 12, [&](int, int) { return true; }, tx, ty);
  else if (what == "gate" || what == "avenue") {
    std::vector<std::pair<int, int>> gs;
    for (auto& gt : g.world.gates) if (std::abs(gt.first - hx) < R && std::abs(gt.second - hy) < R) gs.push_back(gt);
    std::stable_sort(gs.begin(), gs.end(), [&](const std::pair<int, int>& a, const std::pair<int, int>& b) {
      return (a.first - hx) * (a.first - hx) + (a.second - hy) * (a.second - hy) < (b.first - hx) * (b.first - hx) + (b.second - hy) * (b.second - hy);
    });
    if (gs.empty()) { c.fail("cityspot: no gate"); return true; }
    const auto gt = gs[(size_t)nth % gs.size()];
    const int gx = gt.first + 1, gy = gt.second, sy = gy > hy ? -1 : 1;
    if (what == "gate") ok = nearest(gx, gy + sy * 4, 6, [&](int, int) { return true; }, tx, ty);
    else {
      // along the line from the gate to the heart, the first avenue tile past a third of the way (that gate's, then
      // the others'); else any avenue tile 20 to 45 tiles out from the heart
      auto av = [&](int qx, int qy) { return cls(qx, qy) == ew::Urb::Avenue || cls(qx, qy) == ew::Urb::Gutter; };
      for (size_t gi = 0; gi < gs.size() && !ok; gi++) {
        const auto gq = gs[((size_t)nth + gi) % gs.size()];
        const int qx0 = gq.first + 1, qy0 = gq.second;
        for (int k = 33; k <= 80 && !ok; k++) {
          const int x = qx0 + (hx - qx0) * k / 100, y = qy0 + (hy - qy0) * k / 100;
          ok = nearest(x, y, 10, av, tx, ty);
        }
      }
      for (int r = 20; r <= 45 && !ok; r++)
        for (int oy = -r; oy <= r && !ok; oy++)
          for (int ox = -r; ox <= r && !ok; ox++)
            if (std::max(std::abs(ox), std::abs(oy)) == r && walk(hx + ox, hy + oy) && av(hx + ox, hy + oy)) { tx = hx + ox; ty = hy + oy; ok = true; }
    }
  } else if (what == "park") {
    int ox, oy;
    if (centroid([&](int x, int y) { return cls(x, y) == ew::Urb::Park || cls(x, y) == ew::Urb::ParkPath; }, ox, oy))
      ok = nearest(ox, oy + 2, 14, [&](int x, int y) { return cls(x, y) == ew::Urb::ParkPath; }, tx, ty);
  } else if (what == "rich" || what == "poor" || what == "craft") {
    const ew::Quarter q = what == "rich" ? ew::Quarter::Rich : (what == "poor" ? ew::Quarter::Poor : ew::Quarter::Craft);
    int ox, oy;
    // (the poor quarter: its alleys first; the others: their streets and walks off the avenues)
    auto inQ = [&](int x, int y) {
      const uint8_t u = m.urbAt(x, y);
      if (!u || ew::urbQuarter(u) != q) return false;
      const ew::Urb k = ew::urbClass(u);
      return q == ew::Quarter::Poor ? k == ew::Urb::Alley : (k == ew::Urb::Street || k == ew::Urb::Lane || k == ew::Urb::Sidewalk);
    };
    auto inQ2 = [&](int x, int y) { const uint8_t u = m.urbAt(x, y); return u && ew::urbQuarter(u) == q && carriage(x, y); };
    if (centroid(inQ, ox, oy)) ok = nearest(ox, oy, 30, inQ, tx, ty);
    if (!ok && centroid(inQ2, ox, oy)) ok = nearest(ox, oy, 30, inQ2, tx, ty);
  } else if (what == "seat") {
    for (const Bldg& b : m.bldgs)
      if (bldgIsSeat(b) && std::abs(b.doorX() - hx) < R && std::abs(b.doorY() - hy) < R) { ok = nearest(b.doorX(), b.doorY() + 3, 8, [&](int, int) { return true; }, tx, ty); break; }
  } else if (what == "close") {
    int ox, oy;
    if (centroid([&](int x, int y) { return cls(x, y) == ew::Urb::Close; }, ox, oy)) ok = nearest(ox, oy + 1, 10, [&](int, int) { return true; }, tx, ty);
  } else if (what == "well") {
    int k = 0;
    for (int y = hy - R; y <= hy + R && !ok; y++)
      for (int x = hx - R; x <= hx + R && !ok; x++)
        if (m.in(x, y) && m.propAt(x, y) == (int)art::Prop::Well + 1 && cls(x, y) == ew::Urb::Square && std::abs(x - hx) + std::abs(y - hy) > 14 && k++ == nth)
          ok = nearest(x, y + 2, 4, [&](int, int) { return true; }, tx, ty);
  } else if (what == "terrace") {
    int k = 0;
    for (const Bldg& b : m.bldgs)
      if ((b.row & bld::ROW_E) && !(b.row & bld::ROW_W) && std::abs(b.doorX() - hx) < R && std::abs(b.doorY() - hy) < R && k++ == nth) {
        ok = nearest(b.r.x + 4, b.r.y + b.r.h + 1, 6, [&](int, int) { return true; }, tx, ty);
        break;
      }
  } else {
    c.fail("cityspot: unknown place " + what);
    return true;
  }
  if (!ok) { c.fail("cityspot: no " + what + " here"); return true; }
  g.pl().p = Vec2(tx * (float)TILE + 8, ty * (float)TILE + 8);
  c.view.snap(g);
  std::printf("cityspot %s: %d,%d\n", what.c_str(), tx, ty);
  return true;
}
EMB_SCRIPT_CMD("cityspot", "cityspot <heart|gate|avenue|park|rich|poor|craft|seat|close|well|terrace> [n]: put the hero at that place of the city (M7b CITY)", cmdCitySpot);

bool cmdCityProps(ScriptCtx& c) {
  Game& g = c.game;
  Map& m = g.world.over;
  int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
  const int south = c.arg(1) == "south" && !c.arg(2).empty() ? std::max(1, std::atoi(c.arg(2).c_str())) : 1;
  auto clearAt = [&](int x0, int y0) {
    for (int y = y0 - 3; y < y0 + south + 8; y++)
      for (int x = x0 - 12; x < x0 + 12; x++) {
        if (!m.in(x, y)) return false;
        const Ground gr = m.at(x, y);
        if (groundWater(gr) || gr == Ground::Rock || gr == Ground::Lava || m.bldgAt[(size_t)y * m.w + x] >= 0 || (!m.wall.empty() && m.wall[(size_t)y * m.w + x]))
          return false;
      }
    return true;
  };
  bool found = false;
  for (int r = 0; r < 140 && !found; r += 3)
    for (int k = 0; k < 24 && !found; k++) {
      const int x = px + (int)std::lround(std::cos(k * 0.2618f) * r), y = py + (int)std::lround(std::sin(k * 0.2618f) * r);
      if (clearAt(x, y)) { px = x; py = y; found = true; }
    }
  if (!found) { c.fail("cityprops: no open stretch near the hero"); return true; }
  for (int y = py - 3; y < py + south + 8; y++)
    for (int x = px - 12; x < px + 12; x++) m.setP(x, y, 0);
  int x = px - 10;
  for (int p = (int)art::Prop::Bollard; p <= (int)art::Prop::Canopy; p++) {
    int fw = 1, fh = 1;
    art::cityFootprint((art::Prop)p, fw, fh);
    const int cx = x + fw / 2, by = py + south + fh;
    for (int yy = by - fh + 1; yy <= by; yy++)
      for (int xx = x; xx < x + fw; xx++) m.setProp(xx, yy, art::Prop::Filler);
    m.setProp(cx, by, (art::Prop)p);
    x += fw + 1;
  }
  for (int k = 0; k < 8; k++) { m.setProp(px - 10 + k, py + south + 5, art::Prop::GardenWall); m.setProp(px + 1 + k, py + south + 5, art::Prop::Hedge); }
  m.setProp(px - 3, py + south + 6, art::Prop::GardenWall);   // a corner: the run turns south
  m.rebuildSolid();
  g.pl().p = Vec2(px * (float)TILE + 8, py * (float)TILE + 8);
  c.view.snap(g);
  return true;
}
EMB_SCRIPT_CMD("cityprops", "cityprops [south N]: test only: set out every M7b city prop on a cleared stage south of the hero (M7b CITYART)", cmdCityProps);

// (M7c CITY) cityplan <file.png> [scale] [r]: a schematic of the settlement the hero stands in, one cell per tile (scale
// px, default 4) within r tiles of its heart (default 96): the street classes of the urban fabric (avenues, their drains
// and walks, block streets, lanes, alleys, squares, closes, the park and its paths, gardens, courts, quays), the
// buildings (row houses darker; a side or back face onto a street marked on that edge), the walls, the water, trees
// and the open ground. The plan reviews read it next to the street-level shots.
bool cmdCityPlan(ScriptCtx& c) {
  Game& g = c.game;
  const Map& m = g.world.over;
  if (g.inside) { c.fail("cityplan: indoors"); return true; }
  const std::string file = c.arg(1).empty() ? "cityplan.png" : c.arg(1);
  const int S = c.arg(2).empty() ? 4 : std::clamp(std::atoi(c.arg(2).c_str()), 1, 8);
  const int R = c.arg(3).empty() ? 96 : std::clamp(std::atoi(c.arg(3).c_str()), 16, 200);
  const int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
  int hx = px, hy = py, bestD = 1 << 30;
  for (const Site& s : g.world.sites) {
    if (!s.settlement()) continue;
    const int d = (s.ex - px) * (s.ex - px) + (s.ey - py) * (s.ey - py);
    if (d < bestD) { bestD = d; hx = s.ex; hy = s.ey; }
  }
  const int N = 2 * R + 1, IW = N * S;
  std::vector<uint32_t> img((size_t)IW * IW, 0xFF000000u);
  auto col = [](int r, int gg, int b) { return 0xFF000000u | ((uint32_t)b << 16) | ((uint32_t)gg << 8) | (uint32_t)r; };   // RGBA32 little end
  auto put = [&](int cx0, int cy0, int x0, int y0, int w, int h, uint32_t v) {
    for (int y = y0; y < y0 + h; y++)
      for (int x = x0; x < x0 + w; x++) {
        const int X = cx0 * S + x, Y = cy0 * S + y;
        if (X >= 0 && Y >= 0 && X < IW && Y < IW) img[(size_t)Y * IW + X] = v;
      }
  };
  for (int j = 0; j < N; j++)
    for (int i = 0; i < N; i++) {
      const int x = hx - R + i, y = hy - R + j;
      uint32_t v = col(20, 20, 24);
      if (m.in(x, y)) {
        const Ground gr = m.at(x, y);
        switch (gr) {
          case Ground::Water: case Ground::DeepWater: v = col(52, 92, 168); break;
          case Ground::Ice: v = col(170, 200, 230); break;
          case Ground::Bridge: v = col(150, 110, 70); break;
          case Ground::Road: v = col(150, 146, 136); break;
          case Ground::Plaza: v = col(196, 188, 168); break;
          case Ground::Dirt: v = col(132, 104, 70); break;
          case Ground::Farmland: v = col(120, 92, 50); break;
          case Ground::Sand: v = col(206, 190, 130); break;
          case Ground::Snow: v = col(228, 232, 238); break;
          case Ground::Swamp: v = col(70, 96, 70); break;
          case Ground::Meadow: v = col(96, 150, 70); break;
          default: v = col(78, 126, 60); break;
        }
        const uint8_t u = m.urbAt(x, y);
        if (u) switch (ew::urbClass(u)) {
            case ew::Urb::Avenue: v = col(176, 172, 164); break;
            case ew::Urb::Gutter: v = col(110, 108, 104); break;
            case ew::Urb::Sidewalk: v = col(214, 204, 178); break;
            case ew::Urb::Street: v = col(160, 156, 150); break;
            case ew::Urb::Lane: v = col(170, 150, 120); break;
            case ew::Urb::Alley: v = col(140, 112, 80); break;
            case ew::Urb::Square: v = col(226, 214, 186); break;
            case ew::Urb::Close: v = col(200, 220, 180); break;
            case ew::Urb::Park: v = col(70, 160, 70); break;
            case ew::Urb::ParkPath: v = col(190, 170, 120); break;
            case ew::Urb::Garden: v = col(110, 168, 80); break;
            case ew::Urb::Court: v = col(180, 160, 140); break;
            case ew::Urb::Quay: v = col(150, 150, 170); break;
            default: break;
          }
        const int p = m.propAt(x, y);
        if (p && art::isTreeProp((art::Prop)(p - 1))) v = col(30, 80, 30);
        if (!m.wall.empty() && m.wall[(size_t)y * m.w + x]) v = col(60, 56, 60);
      }
      put(i, j, 0, 0, S, S, v);
    }
  for (const Bldg& b : m.bldgs) {
    if (std::abs(b.r.cx() - hx) > R + 8 || std::abs(b.r.cy() - hy) > R + 8) continue;
    const uint32_t body = b.row ? col(150, 56, 40) : ((b.type == art::Building::House || b.type == art::Building::StoneHouse || b.type == art::Building::Hut) ? col(196, 96, 60) : col(120, 70, 150));
    for (int yy = b.r.y; yy < b.r.y + b.r.h; yy++)
      for (int xx = b.r.x; xx < b.r.x + b.r.w; xx++) put(xx - hx + R, yy - hy + R, 0, 0, S, S, body);
    // the front edge (south) lighter; a side or back face onto a street marked on that edge
    for (int xx = b.r.x; xx < b.r.x + b.r.w; xx++) put(xx - hx + R, b.r.y + b.r.h - 1 - hy + R, 0, S - 1, S, 1, col(240, 210, 150));
    if (b.row & bld::ROW_SIDE_W) for (int yy = b.r.y; yy < b.r.y + b.r.h; yy++) put(b.r.x - hx + R, yy - hy + R, 0, 0, 1, S, col(255, 255, 0));
    if (b.row & bld::ROW_SIDE_E) for (int yy = b.r.y; yy < b.r.y + b.r.h; yy++) put(b.r.x + b.r.w - 1 - hx + R, yy - hy + R, S - 1, 0, 1, S, col(255, 255, 0));
    if (b.row & bld::ROW_BACK) for (int xx = b.r.x; xx < b.r.x + b.r.w; xx++) put(xx - hx + R, b.r.y - hy + R, 0, 0, S, 1, col(0, 255, 255));
    const int dx = b.doorX();
    put(dx - hx + R, b.r.y + b.r.h - 1 - hy + R, S / 4, S / 2, std::max(1, S / 2), std::max(1, S / 2), col(40, 20, 10));
  }
  SDL_Surface* s = SDL_CreateSurfaceFrom(IW, IW, SDL_PIXELFORMAT_RGBA32, img.data(), IW * 4);
  const bool ok = s && SDL_SavePNG(s, file.c_str());
  if (s) SDL_DestroySurface(s);
  std::printf("cityplan %s: %s (%dx%d, heart %d,%d)\n", file.c_str(), ok ? "ok" : "FAILED", IW, IW, hx, hy);
  if (!ok) c.fail("cityplan: could not write " + file);
  return true;
}
EMB_SCRIPT_CMD("cityplan", "cityplan <file.png> [scale] [r]: a schematic of the settlement here (streets, blocks, buildings, row faces) (M7c CITY)", cmdCityPlan);

}  // namespace
