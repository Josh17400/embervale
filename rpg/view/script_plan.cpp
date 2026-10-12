// M7c "Order of Streets" script commands (rpg/view/script_api.h, rpg/world/town_plan.h). SURVEY lane (phase B).
//   plan                         print the settlement the hero stands in (or the nearest within 40 tiles): its type,
//                                order, age, traits, the structure score's parts, its tiles per rank and per block role
//   expect structure <min> [max] its structure score is at least min (and at most max, when given): a review script's
//                                gate on the place it shoots
//   planmark                     remember the settlement here (its heart) for `mapcity last` after the hero has gone
//   planaway <dx> <dy>           teleport that far from the planmark settlement's heart (its plan then lies outside
//                                the Active Window: the map draws it from the plan raster)
//   planspot <spot> [in|out]     teleport to a street-level spot of the planmark settlement: plaza (its heart),
//                                street (a block street 18+ tiles out), junction (three or four streets meeting),
//                                gate in / gate out (either side of its first wall opening), faubourg (the approach
//                                road among the houses outside the walls)
//   mapcity [last]               centre the world map on the settlement here (or the one planmark remembered)
//   mapplan 0|1                  the map draws the settlements' plans even inside the Active Window at the street zoom
//   mapwaitplans [n|mark]        wait (script time stands still, up to 60 s) until n settlement plans are on the map
//                                (default 1), or the planmark place's
//   expect mapplans <n>          at least n settlement plans are on the map
//   mapstats [reset]             print the map's plan cache: plans held and made, the worst frame's plan work and tile
//                                painting (ms); reset zeroes the worst values (a pan's own numbers)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"
#include "rpg/world/biomes.h"
#include "rpg/world/settlement.h"
#include "rpg/world/source.h"
#include "rpg/world/town_plan.h"

// (rpg/view/worldmap.cpp)
void worldMapCentreTile(double gx, double gy);
void worldMapForcePlan(bool on);
int worldMapPlanCount();
bool worldMapHasPlan(uint64_t site);
int worldMapPlansMade();
double worldMapPlanWorstMs();
double worldMapWorstPaintMs();
void worldMapResetWorst();
double worldMapRegionWorstMs();
double worldMapFinishWorstMs();

namespace {

// the settlement build of the site the hero is in (or near)
std::shared_ptr<const ew::SettlementOut> hereSettlement(const Game& g, std::string& name, uint64_t* id = nullptr) {
  if (!g.world.endless || !g.world.src || g.inside) return nullptr;
  const int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
  int si = g.world.siteAt(px, py, 0);
  if (si < 0) si = g.world.siteAt(px, py, 40);
  if (si < 0) return nullptr;
  const Site& s = g.world.sites[(size_t)si];
  if (s.type != SiteType::Village && s.type != SiteType::Town && s.type != SiteType::City) return nullptr;
  name = s.name;
  if (id) *id = s.id;
  return g.world.src->settlement(s.id);
}

bool cmdPlan(ScriptCtx& c) {
  std::string name;
  std::shared_ptr<const ew::SettlementOut> o = hereSettlement(c.game, name);
  if (!o) { std::printf("plan: no settlement here\n"); return true; }
  const ew::PlanTraits& T = o->traits;
  const ew::StructureScore S = ew::structureScore(*o);
  int ranks[(int)ew::Rank::COUNT] = {}, roles[(int)ew::BlockRole::COUNT] = {};
  for (uint8_t r : o->rank) if (r < (int)ew::Rank::COUNT) ranks[r]++;
  for (uint8_t r : o->role) if (r < (int)ew::BlockRole::COUNT) roles[r]++;
  std::printf("plan: %s: order %d age %d layout %d plan %d wall %d water %d gates %d avenues %d blocks %dx%d sections %d plazas %d faubourgs %d (%d bldgs)\n",
              name.c_str(), T.order, T.age, T.layout, T.planKind, (int)T.wall, (int)T.water, T.gates, T.avenues, T.blockW, T.blockH, T.gridSections,
              T.plazas, T.faubourgs, T.faubourgBldgs);
  std::printf("plan: structure %.1f (frontage %.2f %d/%d, straight %.2f, regularity %.2f, blocks %d, dead ends %d %.2f, connectivity %.2f)\n", S.total,
              S.frontage, S.fronting, S.bldgs, S.straightness, S.regularity, S.blocks, S.ends, S.deadEnds, S.connectivity);
  std::printf("plan: ranks");
  for (int k = 1; k < (int)ew::Rank::COUNT; k++) std::printf(" %s %d", ew::rankName((ew::Rank)k), ranks[k]);
  std::printf(" | roles");
  for (int k = 1; k < (int)ew::BlockRole::COUNT; k++) if (roles[k]) std::printf(" %s %d", ew::blockRoleName((ew::BlockRole)k), roles[k]);
  std::printf("\n");
  return true;
}

bool expStructure(ScriptCtx& c) {
  std::string name;
  std::shared_ptr<const ew::SettlementOut> o = hereSettlement(c.game, name);
  if (!o) { c.fail("expect structure: no settlement here"); return true; }
  const float v = ew::structureScore(*o).total;
  const float lo = (float)std::atof(c.arg(2).c_str());
  if (v < lo) c.fail("expect structure: " + name + " scores " + std::to_string(v) + ", wanted at least " + c.arg(2));
  if (!c.arg(3).empty() && v > (float)std::atof(c.arg(3).c_str()))
    c.fail("expect structure: " + name + " scores " + std::to_string(v) + ", wanted at most " + c.arg(3));
  return true;
}

double g_markX = 0, g_markY = 0;
bool g_mark = false;
uint64_t g_markId = 0;
std::string g_markName;

std::shared_ptr<const ew::SettlementOut> g_markSo;

bool cmdPlanMark(ScriptCtx& c) {
  std::string name;
  std::shared_ptr<const ew::SettlementOut> o = hereSettlement(c.game, name, &g_markId);
  if (!o) { c.fail("planmark: no settlement here"); return true; }
  g_markX = o->ex + 0.5; g_markY = o->ey + 0.5; g_mark = true; g_markName = name; g_markSo = o;
  std::printf("planmark: %s (heart %d,%d)\n", name.c_str(), (int)o->ex, (int)o->ey);
  return true;
}

bool cmdMapCity(ScriptCtx& c) {
  if (c.arg(1) == "last") {
    if (!g_mark) { c.fail("mapcity last: no planmark yet"); return true; }
    worldMapCentreTile(g_markX, g_markY);
    std::printf("mapcity: %s\n", g_markName.c_str());
    return true;
  }
  std::string name;
  std::shared_ptr<const ew::SettlementOut> o = hereSettlement(c.game, name);
  if (!o) { c.fail("mapcity: no settlement here"); return true; }
  worldMapCentreTile(o->ex + 0.5, o->ey + 0.5);
  std::printf("mapcity: %s\n", name.c_str());
  return true;
}

bool cmdPlanAway(ScriptCtx& c) {
  if (!g_mark) { c.fail("planaway: no planmark yet"); return true; }
  c.game.teleportGlobal((int32_t)std::floor(g_markX) + std::atoi(c.arg(1).c_str()), (int32_t)std::floor(g_markY) + std::atoi(c.arg(2).c_str()));
  c.game.mode = Mode::Play;
  c.view.snap(c.game);
  return true;
}

// a street-level spot of the planmark settlement (local tile, -1 none): plaza (the heart), street (a block street a
// way out from the heart), junction (a crossing of streets), gate in / gate out (a few tiles either side of the
// first wall opening), faubourg (the approach road outside the walls, among its houses)
bool planSpot(const ew::SettlementOut& o, const std::string& what, const std::string& side, int& sx, int& sy) {
  const Map& m = o.buf;
  const size_t n = (size_t)m.w * (size_t)m.h;
  const bool hasR = o.rank.size() == n, hasRole = o.role.size() == n;
  const int hx = o.ex - o.gx, hy = o.ey - o.gy;
  auto free = [&](int x, int y) {
    if (x < 1 || y < 1 || x >= m.w - 1 || y >= m.h - 1) return false;
    const size_t i = (size_t)y * m.w + x;
    if (m.bldgAt.size() == n && m.bldgAt[i] >= 0) return false;
    if (m.wall.size() == n && m.wall[i]) return false;
    if (m.solid.size() == n && m.solid[i]) return false;
    return m.ground.size() == n && !groundWater((Ground)m.ground[i]);
  };
  auto rk = [&](int x, int y) { return hasR && x >= 0 && y >= 0 && x < m.w && y < m.h ? (ew::Rank)o.rank[(size_t)y * m.w + x] : ew::Rank::None; };
  auto way = [&](int x, int y) { return ew::rankWay(rk(x, y)); };
  sx = sy = -1;
  if (what == "plaza") { sx = hx; sy = hy; return true; }
  if (what == "gate") {
    if (o.wallGaps.empty()) return false;
    const IRect& r = o.wallGaps[0];
    const int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    const float dx = (float)(hx - cx), dy = (float)(hy - cy), L = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
    const float k = side == "out" ? -8.0f : 6.0f;
    const int tx = cx + (int)std::lround(dx / L * k), ty = cy + (int)std::lround(dy / L * k);
    // the nearest open tile to that point, a way first (a door there walked the hero into a building)
    long bd = -1;
    for (int pass = 0; pass < 2 && bd < 0; pass++)
      for (int y = ty - 6; y <= ty + 6; y++)
        for (int x = tx - 6; x <= tx + 6; x++) {
          if (!free(x, y) || (pass == 0 && !way(x, y))) continue;
          const long d = (long)(x - tx) * (x - tx) + (long)(y - ty) * (y - ty);
          if (bd < 0 || d < bd) { bd = d; sx = x; sy = y; }
        }
    if (bd < 0) { sx = tx; sy = ty; }
    return true;
  }
  long best = -1;
  long long bestF = -1;
  for (int y = 1; y < m.h - 1; y++)
    for (int x = 1; x < m.w - 1; x++) {
      if (!free(x, y)) continue;
      const long d = (long)(x - hx) * (x - hx) + (long)(y - hy) * (y - hy);
      bool ok = false;
      if (what == "street") ok = rk(x, y) == ew::Rank::Street && d >= 18 * 18;
      else if (what == "junction") {
        int arms = 0;
        static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& dd : D) {
          bool all = true;
          for (int q = 1; q <= 4 && all; q++) all = way(x + dd[0] * q, y + dd[1] * q);
          arms += all;
        }
        ok = rk(x, y) != ew::Rank::Square && ew::rankStreet(rk(x, y)) && arms >= 3 && d >= 14 * 14;
      } else if (what == "faubourg" && hasRole && ew::rankStreet(rk(x, y))) {
        // (the road itself carries no role: a carriageway with the faubourg's lots beside it). (M7c fixer r3, review:
        // "planspot faubourg lands in the gate passage") among the faubourg itself, not under the gatehouse: no wall
        // within 6 tiles, and the spot with the most faubourg lots round it (the nearest of those)
        bool wallNear = false;
        for (int oy = -6; oy <= 6 && !wallNear; oy++)
          for (int ox = -6; ox <= 6; ox++) {
            const int qx = x + ox, qy = y + oy;
            if (qx >= 0 && qy >= 0 && qx < m.w && qy < m.h && m.wall.size() == n && m.wall[(size_t)qy * m.w + qx]) { wallNear = true; break; }
          }
        int fb = 0;
        for (int oy = -4; oy <= 4; oy++)
            for (int ox = -4; ox <= 4; ox++) {
              const int qx = x + ox, qy = y + oy;
              if (qx >= 0 && qy >= 0 && qx < m.w && qy < m.h && (ew::BlockRole)o.role[(size_t)qy * m.w + qx] == ew::BlockRole::Faubourg) fb++;
            }
        ok = fb > 0;
        // score: lots first, nearer second (packed into the distance key)
        if (ok) {
          const long long key = (wallNear ? 100000000000LL : 0LL) + (long long)(81 - fb) * 100000000LL + d;   // (a spot clear of the wall first)
          if (bestF < 0 || key < bestF) { bestF = key; sx = x; sy = y; best = 0; }
        }
        continue;
      }
      if (ok && (best < 0 || d < best)) { best = d; sx = x; sy = y; }
    }
  return best >= 0;
}

bool cmdPlanSpot(ScriptCtx& c) {
  if (!g_mark || !g_markSo) { c.fail("planspot: no planmark yet"); return true; }
  int sx, sy;
  if (!planSpot(*g_markSo, c.arg(1), c.arg(2), sx, sy)) { c.fail("planspot: " + g_markName + " has no " + c.arg(1)); return true; }
  c.game.teleportGlobal(g_markSo->gx + sx, g_markSo->gy + sy);
  c.game.mode = Mode::Play;
  c.view.snap(c.game);
  std::printf("planspot: %s %s%s%s at %d,%d\n", g_markName.c_str(), c.arg(1).c_str(), c.arg(2).empty() ? "" : " ", c.arg(2).c_str(),
              (int)(g_markSo->gx + sx), (int)(g_markSo->gy + sy));
  return true;
}

bool cmdMapPlan(ScriptCtx& c) { worldMapForcePlan(std::atoi(c.arg(1).c_str()) != 0); return true; }

bool cmdMapWaitPlans(ScriptCtx& c) {
  if (c.arg(1) == "mark") return !g_mark || worldMapHasPlan(g_markId);   // (the planmark place's plan)
  const int n = c.arg(1).empty() ? 1 : std::atoi(c.arg(1).c_str());
  return worldMapPlanCount() >= n;
}

bool expMapPlans(ScriptCtx& c) {
  const int n = std::atoi(c.arg(2).c_str());
  if (worldMapPlanCount() < n) c.fail("expect mapplans: " + std::to_string(worldMapPlanCount()) + " plans on the map, wanted " + c.arg(2));
  return true;
}

bool cmdMapStats(ScriptCtx& c) {
  std::printf("mapstats: plans %d (made %d), worst frame: plans %.2f ms, tile painting %.2f ms (a region plan %.2f, a tile's last pass %.2f)\n",
              worldMapPlanCount(), worldMapPlansMade(), worldMapPlanWorstMs(), worldMapWorstPaintMs(), worldMapRegionWorstMs(), worldMapFinishWorstMs());
  if (c.arg(1) == "reset") worldMapResetWorst();
  return true;
}

}  // namespace

EMB_SCRIPT_CMD("planmark", "planmark: remember the settlement here for mapcity last (M7c SURVEY)", cmdPlanMark);
EMB_SCRIPT_CMD("planaway", "planaway <dx> <dy>: teleport that far from the planmark settlement's heart (M7c SURVEY)", cmdPlanAway);
EMB_SCRIPT_CMD("planspot", "planspot plaza|street|junction|gate in|gate out|faubourg: teleport to that spot of the planmark settlement (M7c SURVEY)", cmdPlanSpot);
EMB_SCRIPT_CMD("mapcity", "mapcity [last]: centre the world map on the settlement here (or the planmark one) (M7c SURVEY)", cmdMapCity);
EMB_SCRIPT_CMD("mapplan", "mapplan 0|1: the map's settlement plans inside the Active Window at the street zoom too (M7c SURVEY)", cmdMapPlan);
EMB_SCRIPT_CMD("mapwaitplans", "mapwaitplans [n]: wait until n settlement plans are on the world map (M7c SURVEY)", cmdMapWaitPlans);
EMB_SCRIPT_CMD("expect:mapplans", "expect mapplans <n>: at least n settlement plans on the world map (M7c SURVEY)", expMapPlans);
EMB_SCRIPT_CMD("mapstats", "mapstats [reset]: the map's plan cache and its worst frame's work (M7c SURVEY)", cmdMapStats);
EMB_SCRIPT_CMD("plan", "plan: the settlement here: order, age, traits, structure score, ranks and block roles (M7c SURVEY)", cmdPlan);
EMB_SCRIPT_CMD("expect:structure", "expect structure <min> [max]: the structure score of the settlement here (M7c SURVEY)", expStructure);
