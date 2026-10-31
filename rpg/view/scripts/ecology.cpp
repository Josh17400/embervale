// M3c Wildlands script commands (rpg/view/script_api.h). WORLD lane owns this file.
//   gotoeco <biome> [n]   stand in the n-th nearest patch (default the nearest) of that biome (its key or name:
//                         savanna, birchwood, peatbog, "ash fields"... see rpg/world/biomes.h): a spiral over the far
//                         estimate (EndlessSource::ecoFar, 32-tile lattice, out to 9000 tiles), each sighting confirmed
//                         by the tile classifier (ecoAt) and moved toward the patch's middle (patches >= 300 tiles
//                         apart count as different ones)
//   eco                   print the biome here: name, family, the eco it blends toward and the weight, the profile
//   expect eco <biome>    the tile the player stands on is that biome
//   gotoecoborder <a> <b>   stand on the border between the nearest patch of a and the nearest patch of b beside it
//   gotospecialty <trade> [n]  stand in the n-th nearest village or town of that trade (fishing, mining, herding...)
//   ecotour               print every biome's nearest confirmed patch (global tile) without moving (slow; for writing
//                         scripts)
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"
#include "rpg/world/biomes.h"
#include "rpg/world/source.h"

namespace {

// the n-th nearest confirmed patch of `want` round (px, py); false if none within the search radius
bool findEco(ew::EndlessSource& S, Eco want, int32_t px, int32_t py, int nth, int32_t& ox, int32_t& oy) {
  constexpr int32_t STEP = 32, RMAX = 9000;
  std::vector<std::pair<int32_t, int32_t>> found;
  auto tryAt = [&](int32_t x, int32_t y) {
    if (S.ecoFar(x, y) != want) return;
    // confirm on the real tiles near the sighting, then walk toward the patch's middle (the most same-eco neighbours)
    int32_t bx = 0, by = 0;
    bool ok = false;
    for (int32_t dy = -24; dy <= 24 && !ok; dy += 6)
      for (int32_t dx = -24; dx <= 24 && !ok; dx += 6)
        if (S.ecoAt(x + dx, y + dy) == want) { bx = x + dx; by = y + dy; ok = true; }
    if (!ok) return;
    for (int it = 0; it < 6; it++) {
      int best = -1, bdx = 0, bdy = 0;
      for (int k = 0; k < 9; k++) {
        const int32_t cx = bx + (k % 3 - 1) * 6, cy = by + (k / 3 - 1) * 6;
        int n = 0;
        for (int j = 0; j < 8; j++) n += S.ecoAt(cx + (j % 3 - 1) * 5 + (j == 4 ? 5 : 0), cy + (j / 3 - 1) * 5) == want;
        if (n > best) { best = n; bdx = cx - bx; bdy = cy - by; }
      }
      if (!bdx && !bdy) break;
      bx += bdx; by += bdy;
    }
    if (S.ecoAt(bx, by) != want) return;
    for (auto& f : found) if (std::abs(f.first - bx) < 300 && std::abs(f.second - by) < 300) return;
    found.push_back({bx, by});
  };
  for (int32_t r = 0; r <= RMAX && (int)found.size() <= nth; r += STEP) {
    if (r == 0) { tryAt(px, py); continue; }
    for (int32_t k = -r; k < r && (int)found.size() <= nth; k += STEP) {
      tryAt(px + k, py - r); tryAt(px + r, py + k); tryAt(px - k, py + r); tryAt(px - r, py - k);
    }
  }
  if ((int)found.size() <= nth) return false;
  ox = found[(size_t)nth].first; oy = found[(size_t)nth].second;
  return true;
}

int32_t gxOf(Game& g) { return g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE); }
int32_t gyOf(Game& g) { return g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE); }

bool cmdGotoEco(ScriptCtx& c) {
  Game& g = c.game;
  if (!g.world.src) { c.fail("gotoeco: no endless world"); return true; }
  const std::string name = c.arg(2).empty() || std::isdigit((unsigned char)c.arg(2)[0]) ? c.arg(1) : c.arg(1) + " " + c.arg(2);
  const Eco want = ecoFromName(name.c_str());
  if (want == Eco::COUNT) { c.fail("gotoeco: unknown biome '" + name + "' (rpg/world/biomes.h keys: savanna, birchwood, peatbog...)"); return true; }
  const std::string last = c.a.back();
  const int nth = (c.a.size() >= 3 && std::isdigit((unsigned char)last[0])) ? std::max(0, std::atoi(last.c_str())) : 0;
  int32_t x = 0, y = 0;
  if (!findEco(*g.world.src, want, gxOf(g), gyOf(g), nth, x, y)) { c.fail(std::string("gotoeco: no ") + ecoName(want) + " within 9000 tiles"); return true; }
  if (g.inside) g.debugLeave();
  g.teleportGlobal(x, y);
  // (the teleport steps off a tree, rock or water onto the nearest free tile, which may be another biome at a patch's
  // ragged edge: then the nearest free tile of the wanted biome in the window, rings outward)
  {
    const Map& m = g.world.over;
    const int tx = (int)std::floor(g.pl().p.x / TILE), ty = (int)std::floor(g.pl().p.y / TILE);
    if (m.ecoAt(tx, ty) != want) {
      bool moved = false;
      for (int r = 1; r <= 24 && !moved; r++)
        for (int oy = -r; oy <= r && !moved; oy++)
          for (int ox = -r; ox <= r && !moved; ox++) {
            if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
            const int qx = tx + ox, qy = ty + oy;
            if (!m.in(qx, qy) || m.ecoAt(qx, qy) != want || m.blocked(qx, qy)) continue;
            g.teleportGlobal(g.world.ox + qx, g.world.oy + qy);
            moved = true;
          }
    }
  }
  g.mode = Mode::Play;
  c.view.snap(g);
  printf("gotoeco: %s at %d,%d\n", ecoName(want), x, y);
  return true;
}
EMB_SCRIPT_CMD("gotoeco", "gotoeco <biome> [n]: stand in the n-th nearest patch of that biome (rpg/world/biomes.h keys)", cmdGotoEco);

// gotoecoborder <a> <b>: stand on the border between the nearest patch of biome a and the patch of biome b nearest to
// it (the first change from a along the line between their middles; keys without spaces)
bool cmdGotoEcotone(ScriptCtx& c) {
  Game& g = c.game;
  if (!g.world.src) { c.fail("gotoecoborder: no endless world"); return true; }
  const Eco a = ecoFromName(c.arg(1).c_str()), b = ecoFromName(c.arg(2).c_str());
  if (a == Eco::COUNT || b == Eco::COUNT) { c.fail("gotoecoborder: unknown biome '" + c.arg(1) + "' or '" + c.arg(2) + "'"); return true; }
  ew::EndlessSource& S = *g.world.src;
  int32_t ax = 0, ay = 0, bx = 0, by = 0;
  if (!findEco(S, a, gxOf(g), gyOf(g), 0, ax, ay) || !findEco(S, b, ax, ay, 0, bx, by)) {
    c.fail(std::string("gotoecoborder: no ") + ecoName(a) + " / " + ecoName(b) + " within 9000 tiles");
    return true;
  }
  // walk from a's middle toward b's: the last tile of a before the line leaves it for good
  const int32_t n = std::max(std::abs(bx - ax), std::abs(by - ay));
  int32_t tx = ax, ty = ay;
  for (int32_t k = 0; k <= n; k++) {
    const int32_t x = ax + (int32_t)((int64_t)(bx - ax) * k / std::max(1, n)), y = ay + (int32_t)((int64_t)(by - ay) * k / std::max(1, n));
    if (S.ecoAt(x, y) == a) { tx = x; ty = y; }
    else if (S.ecoAt(x, y) == b) break;
  }
  if (g.inside) g.debugLeave();
  g.teleportGlobal(tx, ty);
  g.mode = Mode::Play;
  c.view.snap(g);
  printf("gotoecoborder: %s / %s at %d,%d (%s at %d,%d, %s at %d,%d)\n", ecoName(a), ecoName(b), tx, ty, ecoName(a), ax, ay, ecoName(b), bx, by);
  return true;
}
EMB_SCRIPT_CMD("gotoecoborder", "gotoecoborder <a> <b>: stand on the border between a patch of biome a and the nearest patch of b", cmdGotoEcotone);

// gotospecialty <farming|fishing|mining|lumber|herding> [n]: stand at the heart of the n-th nearest village or town
// that lives by that trade (the region plans round the player, rings of regions out to 24 regions), and print its biome
bool cmdGotoSpecialty(ScriptCtx& c) {
  Game& g = c.game;
  if (!g.world.src) { c.fail("gotospecialty: no endless world"); return true; }
  std::string want = c.arg(1);
  for (char& ch : want) ch = (char)std::toupper((unsigned char)ch);
  const int nth = c.arg(2).empty() ? 0 : std::max(0, std::atoi(c.arg(2).c_str()));
  ew::EndlessSource& S = *g.world.src;
  const int32_t px = gxOf(g), py = gyOf(g), rx0 = ew::regionOf(px), ry0 = ew::regionOf(py);
  std::vector<std::pair<int64_t, std::pair<int32_t, int32_t>>> found;
  std::vector<std::string> names;
  for (int r = 0; r <= 24 && (int)found.size() <= nth + 2; r++)
    for (int32_t ry = ry0 - r; ry <= ry0 + r; ry++)
      for (int32_t rx = rx0 - r; rx <= rx0 + r; rx++) {
        if (std::max(std::abs(rx - rx0), std::abs(ry - ry0)) != r) continue;
        for (const ew::SitePlan& p : S.region(rx, ry).sites) {
          if (p.type != SiteType::Village && p.type != SiteType::Town) continue;
          if (want != ew::specialtyName(p.special)) continue;
          const int64_t dx = p.ex - px, dy = p.ey - py;
          found.push_back({dx * dx + dy * dy, {p.ex, p.ey}});
        }
      }
  if ((int)found.size() <= nth) { c.fail("gotospecialty: no " + want + " settlement within 24 regions"); return true; }
  std::sort(found.begin(), found.end());
  const int32_t x = found[(size_t)nth].second.first, y = found[(size_t)nth].second.second;
  if (g.inside) g.debugLeave();
  g.teleportGlobal(x, y + 3);
  g.mode = Mode::Play;
  c.view.snap(g);
  printf("gotospecialty: %s settlement at %d,%d in %s\n", want.c_str(), x, y, ecoName(S.ecoAt(x, y)));
  return true;
}
EMB_SCRIPT_CMD("gotospecialty", "gotospecialty <farming|fishing|mining|lumber|herding> [n]: stand in the n-th nearest settlement of that trade",
               cmdGotoSpecialty);

bool cmdEco(ScriptCtx& c) {
  Game& g = c.game;
  const int tx = (int)std::floor(g.pl().p.x / TILE), ty = (int)std::floor(g.pl().p.y / TILE);
  const Map& m = g.world.over;
  const Eco e = m.ecoAt(tx, ty), nb = m.ecoNbAt(tx, ty);
  const EcoInfo& I = ecoInfo(e);
  const int w = m.blendAt(tx, ty) >> 4;
  printf("eco at %d,%d: %s (%s, family %s) blends toward %s (weight %d/16); sky %d ambience %d mood %d habit %d trades %d\n", gxOf(g), gyOf(g), I.name,
         I.key, biomeName(I.family), ecoName(nb), w <= 8 ? w : 0, (int)I.sky, (int)I.amb, (int)I.mood, (int)I.habit, (int)I.trades);
  return true;
}
EMB_SCRIPT_CMD("eco", "eco: print the biome where the player stands", cmdEco);

bool expEco(ScriptCtx& c) {
  Game& g = c.game;
  const std::string name = c.rest(2);
  const Eco want = ecoFromName(name.c_str());
  if (want == Eco::COUNT) { c.fail("expect eco: unknown biome '" + name + "'"); return true; }
  const Eco e = g.world.over.ecoAt((int)std::floor(g.pl().p.x / TILE), (int)std::floor(g.pl().p.y / TILE));
  if (e != want) c.fail(std::string("expect eco ") + ecoName(want) + ": standing in " + ecoName(e));
  return true;
}
EMB_SCRIPT_CMD("expect:eco", "expect eco <biome>: the player stands in that biome", expEco);

bool cmdEcoTour(ScriptCtx& c) {
  Game& g = c.game;
  if (!g.world.src) { c.fail("ecotour: no endless world"); return true; }
  for (int i = 0; i < (int)Eco::COUNT; i++) {
    int32_t x = 0, y = 0;
    if (findEco(*g.world.src, (Eco)i, gxOf(g), gyOf(g), 0, x, y)) printf("ecotour: %-16s %d,%d\n", ecoInfo((Eco)i).key, x, y);
    else printf("ecotour: %-16s NONE within 9000 tiles\n", ecoInfo((Eco)i).key);
  }
  return true;
}
EMB_SCRIPT_CMD("ecotour", "ecotour: print every biome's nearest patch (global tiles)", cmdEcoTour);

}  // namespace
