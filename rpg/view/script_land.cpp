// M3c Wildlands script commands (rpg/view/script_api.h). LAND lane owns this file.
//   floragrid [south]   test only: on the nearest open stretch of land round the hero (no building, wall, water, road or
//                       site), clear a stage and plant every Wildlands flora prop on it, as the land there grows it: the
//                       14 trees two tiles apart, the 8 rocks and shrubs two apart below them, the 15 ground covers in a
//                       row below those (art_flora.cpp's in-game gallery: the real ground, light, night and shadows).
//                       The hero stands just north of the rows (south N: how many tiles, default 1).
//   gotoecoedge <biome> <biome> [n]   stand on the n-th nearest edge where the first biome proper meets the second (an
//                       ecotone between any two ecos, two of one family too: savanna meadow, birchwood darkforest...)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"
#include "rpg/world/biomes.h"
#include "rpg/world/source.h"

namespace {

bool cmdFloraGrid(ScriptCtx& c) {
  Game& g = c.game;
  Map& m = g.world.over;
  int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
  const int south = c.arg(1) == "south" && !c.arg(2).empty() ? std::max(1, std::atoi(c.arg(2).c_str())) : 1;
  auto clearAt = [&](int x0, int y0) {
    for (int y = y0 - 3; y < y0 + south + 10; y++)
      for (int x = x0 - 17; x < x0 + 17; x++) {
        if (!m.in(x, y)) return false;
        const Ground gr = m.at(x, y);
        if (groundWater(gr) || gr == Ground::Rock || gr == Ground::Lava || gr == Ground::Road || gr == Ground::Plaza || m.bldgAt[(size_t)y * m.w + x] >= 0 ||
            (!m.wall.empty() && m.wall[(size_t)y * m.w + x]))
          return false;
      }
    return g.world.siteAt(x0, y0 + 6, 6) < 0;
  };
  bool found = false;
  for (int r = 0; r < 140 && !found; r += 3)
    for (int k = 0; k < 24 && !found; k++) {
      const int x = px + (int)std::lround(std::cos(k * 0.2618f) * r), y = py + (int)std::lround(std::sin(k * 0.2618f) * r);
      if (clearAt(x, y)) { px = x; py = y; found = true; }
    }
  if (!found) { c.fail("floragrid: no open stretch of land near the hero"); return true; }
  for (int y = py - 3; y < py + south + 10; y++)
    for (int x = px - 17; x < px + 17; x++) m.setP(x, y, 0);
  const int t0 = (int)art::Prop::AcaciaTree;
  for (int i = 0; i < 14; i++) m.setProp(px - 14 + i * 2, py + south, (art::Prop)(t0 + i));
  for (int i = 0; i < 8; i++) m.setProp(px - 8 + i * 2, py + south + 2, (art::Prop)(t0 + 14 + i));
  for (int i = 0; i < 15; i++) m.setProp(px - 7 + i, py + south + 4, (art::Prop)(t0 + 22 + i));
  m.rebuildSolid();
  g.pl().p = Vec2(px * (float)TILE + 8, py * (float)TILE + 8);
  c.view.snap(g);
  return true;
}
EMB_SCRIPT_CMD("floragrid", "floragrid [south N]: test only: plant every Wildlands flora prop on a cleared stage south of the hero (LAND lane)", cmdFloraGrid);

// gotoecoedge <biome> <biome> [n]: stand on the n-th nearest edge where the first biome proper meets the second
// (the ecotones between ANY two ecos: savanna meadow, birchwood darkforest, glacier tundra...). A spiral over the far
// estimate (ecoFar) finds the pair 16 tiles apart; the tile classifier (ecoAt) then walks between them to the edge.
bool cmdGotoEcoEdge(ScriptCtx& c) {
  Game& g = c.game;
  if (!g.world.src) { c.fail("gotoecoedge: no endless world"); return true; }
  const Eco a = ecoFromName(c.arg(1).c_str()), b = ecoFromName(c.arg(2).c_str());
  if (a == Eco::COUNT || b == Eco::COUNT) { c.fail("gotoecoedge: unknown biome '" + (a == Eco::COUNT ? c.arg(1) : c.arg(2)) + "' (rpg/world/biomes.h keys)"); return true; }
  ew::EndlessSource& S = *g.world.src;
  const int32_t px = (int32_t)std::floor(g.pl().p.x / TILE) + g.world.ox, py = (int32_t)std::floor(g.pl().p.y / TILE) + g.world.oy;
  const int skip = c.arg(3).empty() ? 0 : std::max(0, std::atoi(c.arg(3).c_str()));
  int found = 0;
  for (int32_t r = 0; r < 12000; r += 16)
    for (int k = 0, nk = std::max(1, std::min((int)(r / 4), 600)); k < nk; k++) {
      const float ang = k / (float)nk * 6.2831853f;
      const int32_t x = px + (int32_t)std::lround(std::cos(ang) * r), y = py + (int32_t)std::lround(std::sin(ang) * r);
      if (S.ecoFar(x, y) != a) continue;
      static const int dx[4] = {16, -16, 0, 0}, dy[4] = {0, 0, 16, -16};
      for (int d = 0; d < 4; d++) {
        if (S.ecoFar(x + dx[d], y + dy[d]) != b) continue;
        // walk the real tiles from the first toward the second: the last tile of a before the first of b
        int32_t ex = 0, ey = 0;
        bool edge = false;
        for (int t = -16; t <= 32 && !edge; t++) {
          const int32_t qx = x + dx[d] * t / 16, qy = y + dy[d] * t / 16, nx = x + dx[d] * (t + 1) / 16, ny = y + dy[d] * (t + 1) / 16;
          if (S.ecoAt(qx, qy) == a && S.ecoAt(nx, ny) == b) { ex = qx; ey = qy; edge = true; }
        }
        if (!edge) continue;
        if (found++ < skip) continue;
        if (g.inside) g.debugLeave();
        g.teleportGlobal(ex, ey);
        g.mode = Mode::Play;
        c.view.snap(g);
        std::printf("gotoecoedge: %s -> %s at %d,%d\n", ecoName(a), ecoName(b), ex, ey);
        return true;
      }
    }
  c.fail(std::string("gotoecoedge: no edge of ") + ecoName(a) + " on " + ecoName(b) + " within 12000 tiles");
  return true;
}
EMB_SCRIPT_CMD("gotoecoedge", "gotoecoedge <biome> <biome> [n]: stand on the n-th nearest edge where the first biome proper meets the second (LAND lane)", cmdGotoEcoEdge);

}  // namespace
