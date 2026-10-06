// M2 VIEW lane: script commands for the world map, the geology view and the M2 art (screenshot scripts
// tools/scripts/m2_view_*.txt). Registered through rpg/view/script_api.h; `embervale --script-help` lists them.
//   mapgeo 0|1                  the MAP tab's geology debug view off / on (as the G key)
//   mapzoom N                   the map's zoom level 0 (street) .. 7 (continent)
//   mapcentre hero | X Y        centre the map on the hero or on a global tile
//   mapselect nearest|rumour|<type>   select (and centre on) the nearest discovered place of that type (city, town,
//                               village, cave, ruin, camp, shrine, lair, landmark, wonder), or the nearest rumour
//   maprumour [n]               test only: the n nearest undiscovered places (not settlements) become rumours
//   mapreveal [n]               test only: the n nearest undiscovered places are discovered
//   mapexplore R                test only: the fog lifts R tiles round the hero (the far zooms show more land)
//   placewild all|<prop> [dx dy]   test only: stamp M2 props (peak, stone, caravan, tower, shack, toll, herbs, grave,
//                               bedroll, elder, colossus, shard, bones; "range": a cluster of peaks) on the ground near
//                               the hero (dx, dy tiles from him), each on its frozen footprint
//   expect mapgeo 0|1           the geology view's state
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"
#include "rpg/world/poi.h"
#include "rpg/world/source.h"

// worldmap.cpp
void worldMapSetGeo(bool on);
bool worldMapGeo();
void worldMapSetZoomLevel(int zi);
void worldMapCentreTile(double gx, double gy);
void worldMapRequestSelect(int site);

namespace {

int nearestSite(Game& g, bool wantDiscovered, const std::string& type) {
  const Vec2 p = g.pl().p * (1.0f / TILE);
  int best = -1;
  float bd = 1e30f;
  for (int i = 0; i < (int)g.world.sites.size(); i++) {
    const Site& s = g.world.sites[(size_t)i];
    if (wantDiscovered != s.discovered) continue;
    if (!type.empty() && type != "nearest") {
      std::string t = siteTypeName(s.type);
      for (char& ch : t) ch = (char)std::tolower((unsigned char)ch);
      const bool match = t.find(type) != std::string::npos || (type == "camp" && s.type == SiteType::BanditCamp) ||
                         (type == "lair" && s.type == SiteType::DragonLair) || (type == "wonder" && s.type == SiteType::Wonder) ||
                         (type == "landmark" && s.type == SiteType::Vignette);
      if (!match) continue;
    }
    // the place the hero stands in comes first (M3 settlements are larger: a neighbour's gate can be nearer than
    // the home village's own)
    const float d = g.world.siteAt((int)p.x, (int)p.y) == i ? 0.0f : std::hypot(s.ex - p.x, s.ey - p.y);
    if (d < bd && (i != g.world.startSite || type == "village" || type == "nearest")) { bd = d; best = i; }
  }
  return best;
}

bool cmdMapGeo(ScriptCtx& c) {
  worldMapSetGeo(std::atoi(c.arg(1).c_str()) != 0);
  return true;
}
EMB_SCRIPT_CMD("mapgeo", "mapgeo 0|1: the MAP tab's geology debug view off / on (as the G key)", cmdMapGeo);

bool cmdMapZoom(ScriptCtx& c) {
  worldMapSetZoomLevel(std::atoi(c.arg(1).c_str()));
  return true;
}
EMB_SCRIPT_CMD("mapzoom", "mapzoom N: the map's zoom level 0 (street) .. 7 (continent)", cmdMapZoom);

bool cmdMapCentre(ScriptCtx& c) {
  Game& g = c.game;
  if (c.arg(1) == "hero" || c.arg(1).empty()) worldMapCentreTile(g.pl().p.x / TILE + g.world.ox, g.pl().p.y / TILE + g.world.oy);
  else worldMapCentreTile(std::atof(c.arg(1).c_str()), std::atof(c.arg(2).c_str()));
  return true;
}
EMB_SCRIPT_CMD("mapcentre", "mapcentre hero | X Y: centre the map on the hero or on a global tile", cmdMapCentre);

bool cmdMapSelect(ScriptCtx& c) {
  Game& g = c.game;
  const std::string t = c.arg(1);
  int s = -1;
  if (t == "rumour") {
    const Vec2 p = g.pl().p * (1.0f / TILE);
    float bd = 1e30f;
    for (int i = 0; i < (int)g.world.sites.size(); i++) {
      const Site& st = g.world.sites[(size_t)i];
      if (!st.rumoured || st.discovered) continue;
      const float d = std::hypot(st.ex - p.x, st.ey - p.y);
      if (d < bd) { bd = d; s = i; }
    }
  } else s = nearestSite(g, true, t);
  if (s < 0) { c.fail("mapselect: no " + t + " to select"); return true; }
  worldMapRequestSelect(s);
  std::printf("script: mapselect %s -> %s\n", t.c_str(), g.world.sites[(size_t)s].name.c_str());
  return true;
}
EMB_SCRIPT_CMD("mapselect", "mapselect nearest|rumour|<type>: select (and centre on) the nearest discovered place of that type, or the nearest rumour", cmdMapSelect);

bool cmdMapRumour(ScriptCtx& c) {
  Game& g = c.game;
  const int n = c.arg(1).empty() ? 2 : std::atoi(c.arg(1).c_str());
  const Vec2 p = g.pl().p * (1.0f / TILE);
  std::vector<std::pair<float, int>> l;
  for (int i = 0; i < (int)g.world.sites.size(); i++) {
    const Site& s = g.world.sites[(size_t)i];
    if (s.discovered || s.settlement() || s.rumoured) continue;
    l.push_back({std::hypot(s.ex - p.x, s.ey - p.y), i});
  }
  std::sort(l.begin(), l.end());
  for (int k = 0; k < n && k < (int)l.size(); k++) g.world.sites[(size_t)l[(size_t)k].second].rumoured = true;
  std::printf("script: maprumour %d of %zu\n", std::min(n, (int)l.size()), l.size());
  return true;
}
EMB_SCRIPT_CMD("maprumour", "maprumour [n]: test only: the n nearest undiscovered places (not settlements) become rumours", cmdMapRumour);

bool cmdMapReveal(ScriptCtx& c) {
  Game& g = c.game;
  const int n = c.arg(1).empty() ? 6 : std::atoi(c.arg(1).c_str());
  const Vec2 p = g.pl().p * (1.0f / TILE);
  std::vector<std::pair<float, int>> l;
  for (int i = 0; i < (int)g.world.sites.size(); i++) {
    const Site& s = g.world.sites[(size_t)i];
    if (s.discovered) continue;
    l.push_back({std::hypot(s.ex - p.x, s.ey - p.y), i});
  }
  std::sort(l.begin(), l.end());
  for (int k = 0; k < n && k < (int)l.size(); k++) {
    Site& s = g.world.sites[(size_t)l[(size_t)k].second];
    s.discovered = true;
    g.explored.markAround(s.ex + g.world.ox, s.ey + g.world.oy, 24);
  }
  return true;
}
EMB_SCRIPT_CMD("mapreveal", "mapreveal [n]: test only: the n nearest undiscovered places are discovered", cmdMapReveal);

bool cmdMapExplore(ScriptCtx& c) {
  Game& g = c.game;
  const int r = c.arg(1).empty() ? 400 : std::atoi(c.arg(1).c_str());
  const int32_t gx = (int32_t)std::floor(g.pl().p.x / TILE) + g.world.ox, gy = (int32_t)std::floor(g.pl().p.y / TILE) + g.world.oy;
  for (int32_t y = gy - r; y <= gy + r; y += ExploredMask::CELL)
    for (int32_t x = gx - r; x <= gx + r; x += ExploredMask::CELL)
      if ((int64_t)(x - gx) * (x - gx) + (int64_t)(y - gy) * (y - gy) <= (int64_t)r * r) g.explored.mark(x, y);
  return true;
}
EMB_SCRIPT_CMD("mapexplore", "mapexplore R: test only: the fog lifts R tiles round the hero", cmdMapExplore);

bool stampWild(Game& g, art::Prop p, int tx, int ty) {
  Map& m = g.world.over;
  int fw = 1, fh = 1;
  art::wildFootprint(p, fw, fh);
  for (int y = ty - fh + 1; y <= ty; y++)
    for (int x = tx - fw / 2; x <= tx + fw / 2; x++) {
      if (!m.in(x, y)) return false;
      const Ground gr = m.at(x, y);
      if (groundWater(gr) || gr == Ground::Rock) m.setG(x, y, Ground::Grass);
      m.setProp(x, y, x == tx && y == ty ? p : art::Prop::Filler);
    }
  return true;
}

bool cmdPlaceWild(ScriptCtx& c) {
  Game& g = c.game;
  struct N { const char* n; art::Prop p; };
  static const N names[] = {{"peak", art::Prop::Peak}, {"stone", art::Prop::StandingStone}, {"caravan", art::Prop::CaravanWreck},
                            {"tower", art::Prop::WatchtowerRuin}, {"shack", art::Prop::FishingShack}, {"toll", art::Prop::TollPost},
                            {"herbs", art::Prop::HerbBed}, {"grave", art::Prop::GraveCairn}, {"bedroll", art::Prop::Bedroll},
                            {"elder", art::Prop::ElderTree}, {"colossus", art::Prop::Colossus}, {"shard", art::Prop::StarShard},
                            {"bones", art::Prop::DragonBones}, {"greatpeak", art::Prop::GreatPeak}};
  int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
  const std::string what = c.arg(1);
  if (what == "all" || what == "range") {
    // open country first: the nearest stretch of land with no building, wall, water or site on it, then the hero on it
    const Map& m = g.world.over;
    auto clearAt = [&](int x0, int y0) {
      for (int y = y0 - 4; y < y0 + 20; y++)
        for (int x = x0 - 16; x < x0 + 16; x++) {
          if (!m.in(x, y)) return false;
          const Ground gr = m.at(x, y);
          if (groundWater(gr) || gr == Ground::Road || gr == Ground::Plaza || m.bldgAt[(size_t)y * m.w + x] >= 0 || (!m.wall.empty() && m.wall[(size_t)y * m.w + x])) return false;
        }
      return g.world.siteAt(x0, y0 + 8, 6) < 0;
    };
    bool found = false;
    for (int r = 0; r < 120 && !found; r += 4)
      for (int k = 0; k < 16 && !found; k++) {
        const int x = px + (int)std::lround(std::cos(k * 0.3927f) * r), y = py + (int)std::lround(std::sin(k * 0.3927f) * r);
        if (clearAt(x, y)) { px = x; py = y; found = true; }
      }
    if (found) {
      // a clean stage: the trees and bushes there are cleared
      for (int y = py - 8; y < py + 20; y++)
        for (int x = px - 16; x < px + 16; x++) g.world.over.setP(x, y, 0);
      g.pl().p = Vec2(px * (float)TILE + 8, py * (float)TILE + 8);
      c.view.snap(g);
    }
  }
  const int dx = c.arg(2).empty() ? 0 : std::atoi(c.arg(2).c_str()), dy = c.arg(3).empty() ? 3 : std::atoi(c.arg(3).c_str());
  if (what == "all") {
    // two rows south of the hero: the small wayside props, then the big ones
    const art::Prop row1[] = {art::Prop::StandingStone, art::Prop::StandingStone, art::Prop::GraveCairn, art::Prop::HerbBed, art::Prop::HerbBed,
                              art::Prop::TollPost, art::Prop::Bedroll, art::Prop::StarShard, art::Prop::CaravanWreck};
    const int off1[] = {-12, -10, -7, -5, -4, -2, 1, 4, 8};
    for (int k = 0; k < 9; k++) stampWild(g, row1[k], px + off1[k] + dx, py + dy);
    stampWild(g, art::Prop::WatchtowerRuin, px - 10 + dx, py + dy + 6);
    stampWild(g, art::Prop::FishingShack, px - 5 + dx, py + dy + 6);
    stampWild(g, art::Prop::Colossus, px + 1 + dx, py + dy + 7);
    stampWild(g, art::Prop::ElderTree, px + 8 + dx, py + dy + 8);
    stampWild(g, art::Prop::DragonBones, px - 6 + dx, py + dy + 11);
  } else if (what == "range") {
    // (M2 fixer round 2) a massif as the generator lays one: the great peak with spires on its flanks
    stampWild(g, art::Prop::GreatPeak, px + dx, py + dy + 2);
    const int pk[][2] = {{-7, 1}, {7, 0}, {-5, 4}, {5, 4}};
    for (auto& q : pk) stampWild(g, art::Prop::Peak, px + dx + q[0], py + dy + q[1]);
  } else {
    bool ok = false;
    for (const N& n : names)
      if (what == n.n) { ok = stampWild(g, n.p, px + dx, py + dy); break; }
    if (!ok) { c.fail("placewild: no room for or no prop called '" + what + "'"); return true; }
  }
  g.world.over.rebuildSolid();
  return true;
}
EMB_SCRIPT_CMD("placewild", "placewild all|range|<prop> [dx dy]: test only: stamp M2 props near the hero (peak stone caravan tower shack toll herbs grave bedroll elder colossus shard bones)", cmdPlaceWild);

// gotoecotone forest plains: stand where the first biome meets the second (the nearest such edge, searched outward on
// the far macro field), for screenshots of the dithered transitions
bool cmdGotoEcotone(ScriptCtx& c) {
  Game& g = c.game;
  if (!g.world.src) { c.fail("gotoecotone: not an endless world"); return true; }
  auto biomeOf = [](const std::string& s, Biome& b) {
    static const char* n[] = {"ocean", "beach", "plains", "forest", "autumn", "taiga", "snow", "swamp", "desert", "mountain"};
    for (int i = 0; i < (int)Biome::COUNT && i < 10; i++)
      if (s == n[i]) { b = (Biome)i; return true; }
    return false;
  };
  Biome a = Biome::Forest, b = Biome::Plains;
  if (!biomeOf(c.arg(1), a) || !biomeOf(c.arg(2), b)) { c.fail("gotoecotone: unknown biome names"); return true; }
  const int32_t px = (int32_t)std::floor(g.pl().p.x / TILE) + g.world.ox, py = (int32_t)std::floor(g.pl().p.y / TILE) + g.world.oy;
  const int skip = c.arg(3).empty() ? 0 : std::atoi(c.arg(3).c_str());   // the n-th nearest
  int found = 0;
  for (int r = 8; r < 8000; r += std::max(8, r / 48))
    for (int k = 0; k < std::min(r / 2, 400); k++) {
      const float ang = k / (float)std::min(r / 2, 400) * 6.2831853f;
      const int32_t x = px + (int32_t)(std::cos(ang) * r), y = py + (int32_t)(std::sin(ang) * r);
      const ew::MacroSample m0 = g.world.src->macro(x, y);
      if (m0.biome != a || m0.water) continue;
      for (int d = 0; d < 4; d++) {
        static const int dx[4] = {12, -12, 0, 0}, dy[4] = {0, 0, 12, -12};
        const ew::MacroSample m1 = g.world.src->macro(x + dx[d], y + dy[d]);
        if (m1.biome != b || m1.water) continue;
        if (found++ < skip) continue;
        g.teleportGlobal(x + dx[d] / 2, y + dy[d] / 2);
        g.mode = Mode::Play;
        c.view.snap(g);
        std::printf("script: gotoecotone %s -> %s at %d,%d\n", c.arg(1).c_str(), c.arg(2).c_str(), x + dx[d] / 2, y + dy[d] / 2);
        return true;
      }
    }
  c.fail("gotoecotone: no such edge within 8000 tiles");
  return true;
}
EMB_SCRIPT_CMD("gotoecotone", "gotoecotone <biome> <biome> [n]: stand on the n-th nearest edge where the first biome meets the second (forest plains, plains desert, taiga snow...)", cmdGotoEcotone);

bool expMapGeo(ScriptCtx& c) {
  const bool want = std::atoi(c.arg(2).c_str()) != 0;
  if (worldMapGeo() != want) c.fail(std::string("expect mapgeo: the geology view is ") + (worldMapGeo() ? "on" : "off"));
  return true;
}
EMB_SCRIPT_CMD("expect:mapgeo", "expect mapgeo 0|1: the map's geology debug view is off / on", expMapGeo);

}  // namespace
