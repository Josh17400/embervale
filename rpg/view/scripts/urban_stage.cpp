// Test-only script commands for the city fabric, moved from rpg/view/terrain.cpp's test tail (SOLID 1.5): gotourb,
// expect:kerbs, urbstage (a synthetic M7c street-network stage, g_stage), tilescan and wallspot. The kerb measurement
// they call, View::urbKerbCheck, stays in terrain.cpp (it uses the painters' internals).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"
#include "rpg/world/urban.h"

using ew::Urb;

// ---------------------------------------------------------------- (M7b CITYART) test only: close shots of the fabric
//   gotourb <class|corner|kerb|grate|terrace> [n]   stand 2 tiles south of the n-th nearest (default 0) tile of a fabric
//                                                   class (avenue .. quay), an outer kerb corner (a walk with the
//                                                   carriageway south and east of it), a south kerb (a walk with the road
//                                                   south of it), a gutter grate (a gutter beside a square) or a row house;
//                                                   none there: the hero stays put (logged, not a failure)
namespace {
bool cmdGotoUrb(ScriptCtx& c) {
  Game& g = c.game;
  const Map& m = g.world.over;
  if (g.inside || m.urb.empty()) { c.fail("gotourb: no urban fabric here"); return true; }
  const std::string what = c.arg(1);
  const int want = c.arg(2).empty() ? 0 : std::max(0, std::atoi(c.arg(2).c_str()));
  auto cls = [&](int x, int y) { return ew::urbClass(m.urbAt(x, y)); };
  auto low = [&](int x, int y) { const ew::Urb u = cls(x, y); return ew::urbCarriage(u) || u == ew::Urb::Gutter; };
  int kind = -1;
  for (int k = 1; k < (int)ew::Urb::COUNT; k++) if (what == ew::urbName((ew::Urb)k)) kind = k;
  auto match = [&](int x, int y) {
    if (what == "terrace") return false;
    const ew::Urb u = cls(x, y);
    if (what == "corner") return u == ew::Urb::Sidewalk && low(x, y + 1) && (low(x + 1, y) || low(x - 1, y));
    if (what == "kerb") return u == ew::Urb::Sidewalk && low(x, y + 1) && cls(x - 1, y) == ew::Urb::Sidewalk && cls(x + 1, y) == ew::Urb::Sidewalk;
    if (what == "grate") {
      if (u != ew::Urb::Gutter) return false;
      for (int k = 0; k < 4; k++) { const ew::Urb q = cls(x + (k == 0) - (k == 1), y + (k == 2) - (k == 3)); if (q == ew::Urb::Square || q == ew::Urb::Court || q == ew::Urb::Close) return true; }
      return false;
    }
    return kind > 0 && (int)u == kind;
  };
  const int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
  std::vector<std::pair<int, std::pair<int, int>>> hits;
  if (what == "terrace") {
    for (const Bldg& b : m.bldgs) if (b.row) hits.push_back({(b.r.x - px) * (b.r.x - px) + (b.r.y - py) * (b.r.y - py), {b.r.x + b.r.w / 2, b.r.y + b.r.h}});
  } else {
    for (int y = std::max(0, py - 90); y < std::min(m.h, py + 90); y++)
      for (int x = std::max(0, px - 90); x < std::min(m.w, px + 90); x++)
        if (match(x, y)) hits.push_back({(x - px) * (x - px) + (y - py) * (y - py), {x, y}});
  }
  if ((int)hits.size() <= want) { std::printf("gotourb: no %s within 90 tiles (the hero stays)\n", what.c_str()); return true; }   // (not a failure: the plan may have none)
  std::nth_element(hits.begin(), hits.begin() + want, hits.end());
  std::sort(hits.begin(), hits.begin() + want + 1);
  const int x = hits[(size_t)want].second.first, y = hits[(size_t)want].second.second;
  g.pl().p = Vec2(x * (float)TILE + 8, (y + 2) * (float)TILE + 8);
  c.view.snap(g);
  std::printf("gotourb: %s %d at local %d,%d\n", what.c_str(), want, x, y);
  return true;
}
EMB_SCRIPT_CMD("gotourb", "gotourb <class|corner|kerb|grate|terrace> [n]: test only: stand by the n-th nearest piece of the city fabric (M7b CITYART)", cmdGotoUrb);
}  // namespace

namespace {
bool expKerbs(ScriptCtx& c) {
  int faces = 0;
  const int bad = c.view.urbKerbCheck(c.game, 40, faces);
  std::printf("expect kerbs: %d south kerbs within 40 tiles, %d without a face\n", faces, bad);
  if (bad || (!faces && c.arg(2) != "any")) c.fail("expect kerbs: " + std::to_string(bad) + " of " + std::to_string(faces) + " south kerbs show no face");
  return true;
}
EMB_SCRIPT_CMD("expect:kerbs", "expect kerbs [any]: every straight south kerb within 40 tiles shows its face (darker than the walk); any: none is no failure (M7b CITYART)", expKerbs);
}  // namespace

// ---------------------------------------------------------------- (M7c ART) test only: a street-network stage
//   urbstage build [far]      in a city: keep its look (the nearest row house's style, the paving), go to open land
//                             `far` (default 420) tiles south of here and lay a synthetic planned quarter round the
//                             hero: two crossing avenues with central drains, block streets with kerbed walks crossing
//                             them, a mews lane and a lane meeting the walks in T junctions, a 1:1 diagonal and a 1:2
//                             tilted street with their walks, a framed square, a river with quays and bridges, row
//                             houses fronting the streets with corner units (ROW_SIDE_W / _E) and backs on a street
//                             (ROW_BACK). Every junction kind of the M7c network on one stage, judged at street zoom.
//   urbstage go <spot>        stand at a spot of the stage: cross, street, tee, lane, corner, mews, diag, tilt,
//                             square, bridge, quay, back, east, yard
namespace {
struct UrbStage {
  bool have = false, built = false;
  Bldg tmpl;
  int pm = 0;
  Biome biome = Biome::Plains;
  int x0 = 0, y0 = 0;          // the stage's local origin in the window it was built in
  int32_t gox = 0, goy = 0;    // the window's global origin then
};
UrbStage g_stage;
constexpr int kStW = 60, kStH = 44;
enum StK : uint8_t { ST_OPEN, ST_WATER, ST_BRIDGE, ST_CARR, ST_WALK, ST_GUT, ST_SQUARE, ST_PARK, ST_GARDEN, ST_QUAY, ST_HOUSE };
struct StT { StK k = ST_OPEN; Urb u = Urb::None; int up = 3; };

bool cmdUrbStage(ScriptCtx& c) {
  Game& g = c.game;
  const std::string sub = c.arg(1);
  if (sub == "go") {
    if (!g_stage.built) { c.fail("urbstage go: no stage (urbstage build first)"); return true; }
    static const struct { const char* n; int x, y; } spots[] = {
        {"cross", 29, 24}, {"street", 12, 24}, {"tee", 38, 25}, {"lane", 38, 28}, {"corner", 14, 11}, {"mews", 20, 16},
        {"diag", 19, 28}, {"tilt", 53, 27}, {"square", 38, 15}, {"bridge", 4, 23}, {"quay", 6, 12}, {"back", 20, 37},
        {"east", 47, 21}, {"yard", 54, 15}};
    const std::string w = c.arg(2);
    for (const auto& s : spots)
      if (w == s.n) {
        const int lx = g_stage.x0 + s.x + (g_stage.gox - g.world.ox), ly = g_stage.y0 + s.y + (g_stage.goy - g.world.oy);
        g.pl().p = Vec2(lx * (float)TILE + 8, ly * (float)TILE + 8);
        c.view.snap(g);
        std::printf("urbstage go %s: local %d,%d\n", s.n, lx, ly);
        return true;
      }
    c.fail("urbstage go: unknown spot " + w);
    return true;
  }
  if (sub != "build") { c.fail("urbstage build [far] | urbstage go <spot>"); return true; }
  if (g.inside) { c.fail("urbstage: indoors"); return true; }
  {
    // the city's look: its nearest row house (else any city house) and the paving of its walks
    Map& m = g.world.over;
    const int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
    int best = -1, bd = 1 << 30;
    for (int i = 0; i < (int)m.bldgs.size(); i++) {
      const Bldg& b = m.bldgs[(size_t)i];
      if (b.urban < 2 || b.type != art::Building::House || b.home) continue;
      const int d = std::abs(b.r.x - px) + std::abs(b.r.y - py) + (b.row ? 0 : 400);
      if (d < bd) { bd = d; best = i; }
    }
    if (best < 0) { c.fail("urbstage build: no city house here"); return true; }
    g_stage.tmpl = m.bldgs[(size_t)best];
    int pm = -1;
    for (int r = 0; r < 40 && pm < 0; r++)
      for (int oy = -r; oy <= r && pm < 0; oy++)
        for (int ox = -r; ox <= r; ox++) {
          const int x = g_stage.tmpl.r.x + ox, y = g_stage.tmpl.r.y + oy;
          const uint8_t bl = m.blendAt(x, y);
          if ((bl >> 4) == (Map::PAVE_MARK >> 4) && ew::urbClass(m.urbAt(x, y)) != Urb::None) { pm = bl & 15; break; }
        }
    g_stage.pm = std::max(0, pm);
    g_stage.biome = m.biomeAt(g_stage.tmpl.r.x, g_stage.tmpl.r.y);
    g_stage.have = true;
  }
  const int far = c.arg(2).empty() ? 420 : std::max(200, std::atoi(c.arg(2).c_str()));
  g.teleportGlobal(g.world.ox + (int)std::floor(g.pl().p.x / TILE), g.world.oy + (int)std::floor(g.pl().p.y / TILE) + far);
  g.mode = Mode::Play;
  Map& m = g.world.over;
  const int hx = (int)std::floor(g.pl().p.x / TILE), hy = (int)std::floor(g.pl().p.y / TILE);
  const int X0 = hx - 30, Y0 = hy - 22;
  g_stage.x0 = X0; g_stage.y0 = Y0; g_stage.gox = g.world.ox; g_stage.goy = g.world.oy;
  // ---- the plan, in stage tiles
  using K = StK;
  using T = StT;
  std::vector<T> P((size_t)kStW * kStH);
  auto at = [&](int x, int y) -> T* { return (x >= 0 && y >= 0 && x < kStW && y < kStH) ? &P[(size_t)y * kStW + x] : nullptr; };
  auto put = [&](int x, int y, K k, Urb u, int up) { if (T* t = at(x, y)) { t->k = k; t->u = u; t->up = up; } };
  auto rect = [&](int ax, int ay, int bx, int by, K k, Urb u, int up) { for (int y = ay; y <= by; y++) for (int x = ax; x <= bx; x++) put(x, y, k, u, up); };
  auto carr = [&](int x, int y) { const T* t = at(x, y); return t && (t->k == ST_CARR || t->k == ST_GUT); };
  // walks first (the carriageways overwrite them where they cross), then the carriageways
  rect(0, 18, kStW - 1, 18, ST_WALK, Urb::Sidewalk, 3); rect(0, 22, kStW - 1, 22, ST_WALK, Urb::Sidewalk, 3);   // the E-W avenue's walks
  rect(27, 0, 27, kStH - 1, ST_WALK, Urb::Sidewalk, 3); rect(31, 0, 31, kStH - 1, ST_WALK, Urb::Sidewalk, 3);   // the N-S avenue's
  rect(0, 6, kStW - 1, 6, ST_WALK, Urb::Sidewalk, 2); rect(0, 9, kStW - 1, 9, ST_WALK, Urb::Sidewalk, 2);       // block streets
  rect(0, 32, kStW - 1, 32, ST_WALK, Urb::Sidewalk, 1); rect(0, 35, kStW - 1, 35, ST_WALK, Urb::Sidewalk, 1);
  rect(10, 0, 10, kStH - 1, ST_WALK, Urb::Sidewalk, 2); rect(13, 0, 13, kStH - 1, ST_WALK, Urb::Sidewalk, 2);
  rect(45, 0, 45, kStH - 1, ST_WALK, Urb::Sidewalk, 2); rect(48, 0, 48, kStH - 1, ST_WALK, Urb::Sidewalk, 2);
  rect(0, 19, kStW - 1, 21, ST_CARR, Urb::Avenue, 3);
  rect(28, 0, 30, kStH - 1, ST_CARR, Urb::Avenue, 3); rect(29, 0, 29, kStH - 1, ST_GUT, Urb::Gutter, 3);
  rect(0, 20, kStW - 1, 20, ST_GUT, Urb::Gutter, 3);
  rect(0, 7, kStW - 1, 8, ST_CARR, Urb::Street, 2); rect(0, 33, kStW - 1, 34, ST_CARR, Urb::Street, 1);
  rect(11, 0, 12, kStH - 1, ST_CARR, Urb::Street, 2); rect(46, 0, 47, kStH - 1, ST_CARR, Urb::Street, 2);
  rect(28, 19, 30, 21, ST_CARR, Urb::Avenue, 3); rect(29, 19, 29, 21, ST_GUT, Urb::Gutter, 3); rect(28, 20, 30, 20, ST_GUT, Urb::Gutter, 3);
  rect(14, 14, 26, 14, ST_CARR, Urb::Lane, 2);    // a mews lane between two rows of backs
  rect(38, 23, 38, 31, ST_CARR, Urb::Lane, 1);    // a lane off the avenue to the south street
  rect(32, 10, 44, 17, ST_SQUARE, Urb::Square, 3);
  // a 1:1 diagonal street (two tiles wide) across the block x 14..26, y 23..31, kerbed walks either side
  rect(14, 23, 26, 31, ST_PARK, Urb::Park, 3);
  for (int k = 0; k <= 8; k++) { put(14 + k, 31 - k, ST_CARR, Urb::Street, 2); put(15 + k, 31 - k, ST_CARR, Urb::Street, 2); }
  // a 1:2 tilted street across the block x 49..59, y 23..31
  rect(49, 23, kStW - 1, 31, ST_GARDEN, Urb::Garden, 2);
  for (int k = 0; k <= 10; k++) { const int y = 29 - k / 2; put(49 + k, y, ST_CARR, Urb::Street, 2); put(49 + k, y + 1, ST_CARR, Urb::Street, 2); }
  for (int y = 23; y <= 31; y++)
    for (int x = 14; x < kStW; x++) {
      if (x > 26 && x < 49) continue;
      T* t = at(x, y);
      if (!t || t->k == ST_CARR) continue;
      bool nbr = false;
      for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++) {
          if (!(ox || oy) || !carr(x + ox, y + oy)) continue;
          const T* q = at(x + ox, y + oy);
          if (q && q->u == Urb::Street) nbr = true;
        }
      if (nbr) { t->k = ST_WALK; t->u = Urb::Sidewalk; t->up = 2; }
    }
  // the river: water cols 3..5, quays on both banks, bridges where the streets cross
  for (int y = 0; y < kStH; y++) {
    const bool span = (y >= 6 && y <= 9) || (y >= 18 && y <= 22) || (y >= 32 && y <= 35);
    for (int x = 3; x <= 5; x++) put(x, y, span ? ST_BRIDGE : ST_WATER, Urb::None, 3);
    if (!span) { put(2, y, ST_QUAY, Urb::Quay, 2); put(6, y, ST_QUAY, Urb::Quay, 2); }
  }
  rect(7, 10, 9, 13, ST_GARDEN, Urb::Garden, 2);
  rect(7, 23, 9, 31, ST_GARDEN, Urb::Garden, 2);
  // ---- write the tiles
  const int lvl = m.heightAt(hx, hy);
  if (m.urb.empty()) m.urb.assign((size_t)m.w * m.h, 0);
  for (int y = -6; y < kStH + 6; y++)
    for (int x = -6; x < kStW + 6; x++) {
      const int mx = X0 + x, my = Y0 + y;
      if (!m.in(mx, my)) continue;
      const size_t i = (size_t)my * m.w + mx;
      const T* t = at(x, y);
      Ground gr = Ground::Grass;
      uint8_t ub = 0;
      if (t) {
        switch (t->k) {
          case ST_WATER: gr = Ground::Water; break;
          case ST_BRIDGE: gr = Ground::Bridge; break;
          case ST_CARR: case ST_GUT: gr = Ground::Road; break;
          case ST_WALK: case ST_SQUARE: case ST_QUAY: gr = Ground::Plaza; break;
          default: gr = Ground::Grass; break;
        }
        if (t->u != Urb::None) ub = ew::urbPack(t->u, t->up, ew::Quarter::Centre);
      }
      m.ground[i] = (uint8_t)gr;
      if (!m.prop.empty()) m.prop[i] = 0;
      if (!m.deco.empty()) m.deco[i] = 0;
      if (!m.wall.empty()) m.wall[i] = 0;
      if (!m.biome.empty()) m.biome[i] = (uint8_t)g_stage.biome;
      if (!m.eco.empty()) m.eco[i] = (uint8_t)ecoOfFamily(g_stage.biome);
      if (!m.ecoNb.empty()) m.ecoNb[i] = (uint8_t)ecoOfFamily(g_stage.biome);
      if (!m.height.empty()) m.height[i] = (uint8_t)lvl;
      if (!m.blend.empty()) m.blend[i] = t ? (uint8_t)(Map::PAVE_MARK | g_stage.pm) : 0;
      m.urb[i] = ub;
    }
  // buildings standing on the stage (a farm of the land) are moved off the map
  for (Bldg& b : m.bldgs)
    if (b.r.x + b.r.w > X0 - 6 && b.r.x < X0 + kStW + 6 && b.r.y + b.r.h > Y0 - 6 && b.r.y < Y0 + kStH + 6) b.r.x = -100000;
  // ---- the row houses: runs along a front, widths 2..3, the party walls between, the street faces at the ends / back
  auto streetish = [&](int x, int y) { const T* t = at(x, y); return t && (t->k == ST_CARR || t->k == ST_GUT || t->k == ST_WALK || t->k == ST_SQUARE || t->k == ST_QUAY); };
  int nb = 0;
  auto run = [&](int ax, int bx, int y0, int h, bool yard, uint32_t salt) {
    std::vector<int> ws;
    int left = bx - ax + 1;
    uint32_t hs = hash2((int)salt, 77, 9101);
    while (left > 0) {
      int w = 2 + (int)(hs & 1u);
      hs = hash2((int)hs, 0x51, 9103);
      if (left - w == 1) w = left >= 4 ? 2 : 3;
      if (w > left) w = left;
      ws.push_back(w);
      left -= w;
    }
    const uint8_t storeys = (uint8_t)(2 + hash2((int)salt, 3, 9105) % 2u);
    int x = ax;
    for (size_t k = 0; k < ws.size(); k++) {
      const int w = ws[k];
      Bldg b = g_stage.tmpl;
      b.id = 0; b.site = -1; b.home = 0; b.charred = 0;
      b.open = Bldg::Open();
      b.r = IRect{X0 + x, Y0 + y0, w, h};
      b.seed = hash2((int)salt * 131, (int)k * 977 + 5, 9107);
      b.storeys = storeys;
      b.type = (k == 1 && ws.size() >= 4) ? art::Building::Shop : art::Building::House;
      b.variant = (uint8_t)(1 + (k * 3 + salt) % 7);
      uint8_t row = 0;
      if (k > 0) row |= bld::ROW_W;
      if (k + 1 < ws.size()) row |= bld::ROW_E;
      if (k == 0 && streetish(x - 1, y0 + h - 1)) row |= bld::ROW_SIDE_W;
      if (k + 1 == ws.size() && streetish(x + w, y0 + h - 1)) row |= bld::ROW_SIDE_E;
      if (streetish(x + w / 2, y0 - 1)) row |= bld::ROW_BACK;
      if (yard && !(row & bld::ROW_BACK)) row |= bld::ROW_YARD;
      b.row = row;
      for (int yy = y0; yy < y0 + h; yy++) for (int xx = x; xx < x + w; xx++) put(xx, yy, ST_HOUSE, Urb::None, 0);
      m.bldgs.push_back(b);
      nb++;
      x += w;
    }
  };
  run(7, 9, 2, 4, false, 1); run(14, 26, 2, 4, false, 2); run(32, 44, 2, 4, false, 3); run(49, kStW - 1, 2, 4, false, 4);
  run(14, 26, 10, 4, false, 5); run(14, 26, 15, 3, false, 6);        // the mews: backs on the block street and the lane
  run(49, kStW - 1, 13, 5, true, 7);                                  // yards behind
  run(7, 9, 14, 4, false, 8);                                         // a corner house between the garden and a street
  run(32, 37, 23, 3, false, 9); run(39, 44, 23, 3, false, 10);        // backs on the avenue's walk, the lane beside
  run(32, 37, 27, 5, false, 11); run(39, 44, 27, 5, false, 12);
  run(14, 26, 36, 4, false, 13); run(32, 44, 36, 4, false, 14); run(49, kStW - 1, 36, 4, false, 15);   // backs on the south street
  m.rebuildSolid();
  g_stage.built = true;
  g.pl().p = Vec2((X0 + 29) * (float)TILE + 8, (Y0 + 24) * (float)TILE + 8);
  c.view.snap(g);
  std::printf("urbstage: built at local %d,%d (global %d,%d), paving %d, %d row houses\n", X0, Y0, X0 + g.world.ox, Y0 + g.world.oy, g_stage.pm, nb);
  return true;
}
EMB_SCRIPT_CMD("urbstage", "urbstage build [far] | go <spot>: test only: a synthetic M7c street network with row houses (M7c ART)", cmdUrbStage);

// ---------------------------------------------------------------- (M7c ART) test only: the city wall's places
//   wallspot <vertex|diag|gate|water|any> [n]   stand the hero by the nth (nearest first) place of that kind on the
//                                               city wall within 90 tiles: a vertex (the run turns by 30 degrees or
//                                               more: a polygon's corner, a straight run onto a diagonal), a diagonal
//                                               run, a gate whose curtains leave it on the slant, the wall meeting water
// The run's direction at a wall tile is read from the wall tiles on the square ring 4 tiles out (two clusters: the run
// arriving and leaving).
// tilescan <dx0> <dy0> <dx1> <dy1>: debug: print what lies on the tiles of that box round the hero (ground, prop, deco,
// wall, building) (M7c fixer r3)
bool cmdTileScan(ScriptCtx& c) {
  Game& g = c.game;
  const Map& m = g.world.over;
  const int hx = (int)(g.pl().p.x / TILE), hy = (int)(g.pl().p.y / TILE);
  const int x0 = hx + std::atoi(c.arg(1).c_str()), y0 = hy + std::atoi(c.arg(2).c_str()), x1 = hx + std::atoi(c.arg(3).c_str()), y1 = hy + std::atoi(c.arg(4).c_str());
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      if (!m.in(x, y)) continue;
      const size_t i = (size_t)y * m.w + x;
      const int pr = m.prop.empty() ? 0 : m.prop[i], dc = m.deco.empty() ? 0 : m.deco[i], wl = m.wall.empty() ? 0 : m.wall[i];
      const int bi = m.bldgAt.empty() ? -1 : m.bldgAt[i];
      if (!pr && !dc && !wl && bi < 0) continue;
      std::printf("tilescan %d,%d: ground %d prop %d deco %d wall %d bldg %d", x, y, (int)m.ground[i], pr - 1, dc, wl, bi);
      if (bi >= 0 && bi < (int)m.bldgs.size()) { const Bldg& b = m.bldgs[(size_t)bi]; std::printf(" (type %d rect %d,%d %dx%d home %d)", (int)b.type, b.r.x, b.r.y, b.r.w, b.r.h, (int)b.home); }
      std::printf("\n");
    }
  if (c.arg(5) == "h")
    for (int y = y0; y <= y1; y++) {
      std::printf("tilescan h %d:", y);
      for (int x = x0; x <= x1; x++) std::printf(" %2d", (int)m.heightBits(x, y));
      std::printf("\n");
    }
  for (size_t k = 0; k < m.bldgs.size(); k++) {
    const Bldg& b = m.bldgs[k];
    if (b.r.x > x1 || b.r.y > y1 || b.r.x + b.r.w < x0 || b.r.y + b.r.h < y0) continue;
    std::printf("tilescan bldg %zu: type %d rect %d,%d %dx%d home %d site %d\n", k, (int)b.type, b.r.x, b.r.y, b.r.w, b.r.h, (int)b.home, b.site);
  }
  return true;
}
EMB_SCRIPT_CMD("tilescan", "tilescan <dx0> <dy0> <dx1> <dy1>: debug: what lies on the tiles of that box round the hero (M7c fixer r3)", cmdTileScan);
bool cmdWallSpot(ScriptCtx& c) {
  Game& g = c.game;
  const Map& m = g.world.over;
  const std::string what = c.arg(1);
  const int nth = c.arg(2).empty() ? 0 : std::atoi(c.arg(2).c_str());
  const int hx = (int)(g.pl().p.x / TILE), hy = (int)(g.pl().p.y / TILE);
  auto wallAt = [&](int x, int y) { return m.in(x, y) && !m.wall.empty() && m.wall[(size_t)y * m.w + x] != 0; };
  // the two directions of the run at (x, y): false where the ring does not show exactly two runs
  auto dirs = [&](int x, int y, float& ax, float& ay, float& bx, float& by) {
    constexpr int R = 4;
    std::vector<std::pair<int, int>> ring;
    for (int i = -R; i < R; i++) ring.push_back({i, -R});
    for (int j = -R; j < R; j++) ring.push_back({R, j});
    for (int i = R; i > -R; i--) ring.push_back({i, R});
    for (int j = R; j > -R; j--) ring.push_back({-R, j});
    const int n = (int)ring.size();
    int start = -1;
    for (int k = 0; k < n; k++) if (!wallAt(x + ring[(size_t)k].first, y + ring[(size_t)k].second)) { start = k; break; }
    if (start < 0) return false;
    float sx[3] = {0, 0, 0}, sy[3] = {0, 0, 0};
    int cnt[3] = {0, 0, 0}, nc = 0;
    bool in = false;
    for (int k = 1; k <= n; k++) {
      const auto& q = ring[(size_t)((start + k) % n)];
      const bool w = wallAt(x + q.first, y + q.second);
      if (w && !in) { if (nc == 3) return false; nc++; }
      in = w;
      if (w) { sx[nc - 1] += (float)q.first; sy[nc - 1] += (float)q.second; cnt[nc - 1]++; }
    }
    if (nc != 2) return false;
    ax = sx[0] / cnt[0]; ay = sy[0] / cnt[0]; bx = sx[1] / cnt[1]; by = sy[1] / cnt[1];
    return true;
  };
  auto turnDeg = [](float ax, float ay, float bx, float by) {
    const float d = (ax * bx + ay * by) / std::max(1e-3f, std::sqrt((ax * ax + ay * ay) * (bx * bx + by * by)));
    return 180.0f - std::acos(std::clamp(d, -1.0f, 1.0f)) * 57.2958f;   // 0: straight on
  };
  auto slanted = [](float ax, float ay) { const float r = std::fabs(ax) / std::max(1e-3f, std::fabs(ax) + std::fabs(ay)); return r > 0.3f && r < 0.7f; };
  struct Cand { int d, x, y; };
  std::vector<Cand> cs;
  constexpr int RR = 90;
  if (what == "gate") {
    for (const auto& gt : g.world.gates) {
      const int gx = gt.first, gy = gt.second;
      if (std::abs(gx - hx) > RR || std::abs(gy - hy) > RR) continue;
      bool sl = false;
      for (int side = 0; side < 2 && !sl; side++) {
        const int px = side ? gx + 6 : gx - 4;
        float ax, ay, bx, by;
        for (int oy = -2; oy <= 2 && !sl; oy++)
          for (int ox = -1; ox <= 1 && !sl; ox++)
            if (wallAt(px + ox, gy + oy) && dirs(px + ox, gy + oy, ax, ay, bx, by) && (slanted(ax, ay) || slanted(bx, by))) sl = true;
      }
      if (sl) cs.push_back({std::abs(gx - hx) + std::abs(gy - hy), gx + 1, gy});
    }
  } else {
    for (int y = std::max(0, hy - RR); y < std::min(m.h, hy + RR); y++)
      for (int x = std::max(0, hx - RR); x < std::min(m.w, hx + RR); x++) {
        if (!wallAt(x, y)) continue;
        bool ok = what == "any";
        if (what == "water") {
          for (int oy = -1; oy <= 1 && !ok; oy++)
            for (int ox = -1; ox <= 1 && !ok; ox++) ok = m.in(x + ox, y + oy) && groundWater(m.at(x + ox, y + oy));
        } else if (what == "vertex" || what == "diag") {
          float ax, ay, bx, by;
          if (!dirs(x, y, ax, ay, bx, by)) continue;
          const float t = turnDeg(ax, ay, bx, by);
          ok = what == "vertex" ? t >= 30.0f : (t < 12.0f && slanted(ax, ay) && slanted(bx, by));
          for (const auto& gt : g.world.gates)   // (a gate's opening reads as a turn: never a vertex)
            if (ok && std::abs(gt.first + 1 - x) <= 7 && std::abs(gt.second - y) <= 5) ok = false;
        } else if (what != "any") {
          c.fail("wallspot: unknown place " + what + " (vertex|diag|gate|water|any)");
          return true;
        }
        if (ok) cs.push_back({std::abs(x - hx) + std::abs(y - hy), x, y});
      }
  }
  // one place per 12-tile cell (a vertex is a cluster of tiles), nearest first
  std::stable_sort(cs.begin(), cs.end(), [](const Cand& a, const Cand& b) { return a.d < b.d; });
  std::vector<Cand> one;
  for (const Cand& q : cs) {
    bool near = false;
    for (const Cand& o : one) if (std::abs(o.x - q.x) < 12 && std::abs(o.y - q.y) < 12) { near = true; break; }
    if (!near) one.push_back(q);
  }
  if (one.empty()) { c.fail("wallspot: no " + what + " on a wall here"); return true; }
  const Cand p = one[(size_t)nth % one.size()];
  // stand on open ground a few tiles south of it (the wall's face is its south side)
  int tx = p.x, ty = p.y + 3;
  bool found = false;
  for (int r = 0; r <= 8 && !found; r++)
    for (int oy = -r; oy <= r && !found; oy++)
      for (int ox = -r; ox <= r && !found; ox++) {
        const int x = p.x + ox, y = p.y + 3 + oy;
        if (std::max(std::abs(ox), std::abs(oy)) == r && m.in(x, y) && !m.blocked(x, y) && !groundWater(m.at(x, y))) { tx = x; ty = y; found = true; }
      }
  g.pl().p = Vec2(tx * (float)TILE + 8, ty * (float)TILE + 8);
  c.view.snap(g);
  std::printf("wallspot %s %d of %zu: wall %d,%d, hero %d,%d\n", what.c_str(), nth, one.size(), p.x, p.y, tx, ty);
  return true;
}
EMB_SCRIPT_CMD("wallspot", "wallspot <vertex|diag|gate|water|any> [n]: test only: stand by that place of the city wall (M7c ART)", cmdWallSpot);
}  // namespace
