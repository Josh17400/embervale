// M4 REALM lane script commands (rpg/view/script_api.h; `embervale --script-help` lists them). They put the realm in a
// state before the daily tick makes it by itself (VISION_PLAN M4 verification: "siege.txt, forced with --event siege"):
//   realm siege [n]      the n-th nearest settlement (default 0) is besieged by a neighbouring kingdom (war declared)
//   realm take [n]       ... taken by a neighbouring kingdom (Occupied; banners follow)
//   realm burn [n]       ... burned (refugees go to the nearest friendly settlement)
//   realm famine [n]     ... starving (food 0; its realm's granaries are empty: the road to war of 15.6.3 starts)
//   realm war            the kingdom whose land the player stands on and a neighbour go to war
//   realm days <N>       N days pass (the realm ticks them on the next step)
//   (REALM lane, phase B) for the kingdom whose land the player stands on:
//   realm succession     its ruler dies; the society's inheritance picks the heir (a new house on an elective win)
//   realm rebels         a civil war splits it: the far half becomes a rebel kingdom with new arms
//   realm peace          its wars end in treaties
//   realm harvest        its harvest fails (granaries drop; the food deals it sells break on the next tick)
//   realm tension <rung> it and its nearest neighbour stand on a rung of the road to war:
//                        strained | tradebroken | border | skirmishes (the tick climbs or cools from there)
//   realm stats          print the realm's counts and costs (focus and tick ms) to stdout
//   expect realm <state> [n]   the n-th nearest settlement is besieged | burned | occupied | famine | refugees | frontier |
//                        unrest | ruined | rebuilding | garrison | abandoned; or, for the player's land: war | peace |
//                        rebels (a rebel realm broke from it, or it is one) | tension (any neighbour off Calm) | hunger (its granary
//                        is empty)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"
#include "rpg/world/source.h"

namespace {

// loaded settlements, nearest to the player first
std::vector<int> nearestSettlements(Game& g) {
  std::vector<std::pair<float, int>> d;
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  for (size_t i = 0; i < g.world.sites.size(); i++) {
    const Site& s = g.world.sites[i];
    if (!s.settlement()) continue;
    const float dx = s.ex - px, dy = s.ey - py;
    d.push_back({dx * dx + dy * dy, (int)i});
  }
  std::sort(d.begin(), d.end());
  std::vector<int> out;
  for (auto& e : d) out.push_back(e.second);
  return out;
}
ew::Gid ownerOf(Game& g, const Site& s) { return s.kingdom >= 0 ? g.world.kingdoms[(size_t)s.kingdom].id : 0; }
ew::Gid homeOf(Game& g, const Site& s) { return s.homeKingdom >= 0 ? g.world.kingdoms[(size_t)s.homeKingdom].id : 0; }
// a kingdom of the realm other than `not` (the nearest by seat)
ew::Gid neighbour(Game& g, ew::Gid notK, int32_t gx, int32_t gy) {
  ew::Gid best = 0;
  double bd = 1e30;
  for (const realm::KingdomState& k : g.realm.kingdoms()) {
    if (k.id == notK || k.fallen) continue;
    const realm::SettlementState* cap = g.realm.settlement(k.capital);
    const ew::KingdomPlan* kp = g.world.src ? g.world.src->kingdom(k.id) : nullptr;
    const double sx = cap ? cap->gx : kp ? kp->gx : gx, sy = cap ? cap->gy : kp ? kp->gy : gy;
    const double dx = sx - gx, dy = sy - gy, d = dx * dx + dy * dy;
    if (d < bd) { bd = d; best = k.id; }
  }
  return best;
}
void playerTile(Game& g, int32_t& gx, int32_t& gy) {
  gx = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE);
  gy = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
}

bool cmdRealm(ScriptCtx& c) {
  Game& g = c.game;
  if (!g.world.src) { c.fail("realm: no endless world"); return true; }
  const std::string what = c.arg(1);
  if (what == "days") { g.day += std::max(0, std::atoi(c.arg(2).c_str())); return true; }
  int32_t gx, gy;
  playerTile(g, gx, gy);
  if (what == "stats") {   // (before any focusNow: the costs are the game's own, time-sliced)
    const realm::Realm::Stats& s = g.realm.stats;
    int wars = 0;
    for (const realm::War& w : g.realm.wars()) if (!w.endDay) wars++;
    printf("realm: day %d, %zu kingdoms (%d active, %d dormant), %zu relations, %d wars, %zu sieges, %zu events, memory %zu B; "
           "focus last %.2f ms worst %.2f ms (%d quarter cells read; worst units: probe %.2f, read %.2f, plan %.2f, genesis %.2f, history %.2f ms); tick avg %.3f ms worst %.3f ms (%d ticks, %d weekly, %d catch-ups)\n",
           g.realm.day(), g.realm.kingdoms().size(), s.active, s.dormant, g.realm.relations().size(), wars, g.realm.sieges().size(),
           g.realm.events().size(), g.realm.memoryBytes(), s.lastFocusMs, s.worstFocusMs, s.scans, s.worstProbeMs, s.worstScanMs, s.worstPlanMs, s.worstInstMs, s.worstHistMs,
           s.ticks ? s.tickMsSum / s.ticks : 0.0, s.worstTickMs, s.ticks, s.weeklyTicks, s.catchUps);
    fflush(stdout);
    return true;
  }
  // the realm knows the kingdoms around the player (a script may force before the first step: no time-slicing here)
  g.realm.focusNow(*g.world.src, gx, gy, g.day);
  const ew::Gid land = g.realm.landOwner(g.world.src->kingdomAt(gx, gy), gx, gy);
  if (what == "war" || what == "succession" || what == "rebels" || what == "peace" || what == "harvest" || what == "tension") {
    if (!land || !g.realm.kingdom(land)) { c.fail("realm " + what + ": the player stands in the wildlands"); return true; }
    if (what == "war") {
      const ew::Gid other = neighbour(g, land, gx, gy);
      if (!other) { c.fail("realm war: no neighbouring kingdom"); return true; }
      g.realm.forceWar(other, land, g.day);
    } else if (what == "succession") {
      g.realm.forceRulerDeath(land, g.day);
    } else if (what == "rebels") {
      if (!g.realm.forceCivilWar(*g.world.src, land, g.day)) { c.fail("realm rebels: the realm is too small to split"); return true; }
    } else if (what == "peace") {
      for (const realm::War& w : std::vector<realm::War>(g.realm.wars()))
        if (!w.endDay && (w.attacker == land || w.defender == land)) g.realm.forcePeace(w.attacker, w.defender, g.day);
    } else if (what == "harvest") {
      g.realm.forceHarvestFail(land, g.day);
    } else {
      const std::string rung = c.arg(2);
      realm::Tension t = realm::Tension::Strained;
      if (rung == "tradebroken") t = realm::Tension::TradeBroken;
      else if (rung == "border") t = realm::Tension::BorderTension;
      else if (rung == "skirmishes") t = realm::Tension::Skirmishes;
      else if (!rung.empty() && rung != "strained") { c.fail("realm tension strained|tradebroken|border|skirmishes"); return true; }
      const ew::Gid other = neighbour(g, land, gx, gy);
      if (!other) { c.fail("realm tension: no neighbouring kingdom"); return true; }
      g.realm.forceTension(land, other, t, t == realm::Tension::TradeBroken ? realm::WarCause::BrokenTrade : realm::WarCause::Famine, g.day);
    }
    g.realmSync();
    return true;
  }
  const std::vector<int> near = nearestSettlements(g);
  const size_t n = (size_t)std::max(0, std::atoi(c.arg(2).c_str()));
  if (n >= near.size()) { c.fail("realm " + what + ": no such settlement loaded"); return true; }
  const Site& s = g.world.sites[(size_t)near[n]];
  g.realm.noteSite(s.id, homeOf(g, s), (uint8_t)s.type, g.world.ox + s.ex, g.world.oy + s.ey);
  const ew::Gid own = ownerOf(g, s);
  if (what == "siege" || what == "take") {
    const ew::Gid enemy = neighbour(g, own, g.world.ox + s.ex, g.world.oy + s.ey);
    if (!enemy) { c.fail("realm " + what + ": no neighbouring kingdom"); return true; }
    if (what == "siege") g.realm.forceSiege(s.id, enemy, g.day);
    else g.realm.forceOwner(s.id, enemy, g.day);
  } else if (what == "burn") g.realm.forceBurn(s.id, g.day);
  else if (what == "famine") g.realm.forceFamine(s.id, g.day);
  else { c.fail("realm siege|take|burn|famine [n] | war | days N | succession | rebels | peace | harvest | tension <rung> | stats"); return true; }
  g.realmSync();
  return true;
}
EMB_SCRIPT_CMD("realm", "realm siege|take|burn|famine [n] | war | days N | succession | rebels | peace | harvest | tension <rung> | stats (M4)", cmdRealm);

bool expRealm(ScriptCtx& c) {
  Game& g = c.game;
  const std::string what = c.arg(2);
  if (what == "war" || what == "peace" || what == "rebels" || what == "tension" || what == "hunger") {
    if (!g.world.src) { c.fail("expect realm " + what + ": no world"); return true; }
    int32_t gx, gy;
    playerTile(g, gx, gy);
    const ew::Gid land = g.realm.landOwner(g.world.src->kingdomAt(gx, gy), gx, gy);
    bool war = false;
    for (const realm::War& w : g.realm.wars()) if (!w.endDay && (w.attacker == land || w.defender == land)) war = true;
    if (what == "war" && !war) c.fail("expect realm war: the player's land is at peace");
    if (what == "peace" && war) c.fail("expect realm peace: the player's land is at war");
    if (what == "rebels") {
      bool any = false;
      for (const realm::KingdomState& k : g.realm.kingdoms()) if (k.rebel && (k.parent == land || k.id == land)) any = true;
      if (!any) c.fail("expect realm rebels: no rebel realm broke from the player's land");
    }
    if (what == "tension") {
      bool any = false;
      for (const realm::Relation& r : g.realm.relations())
        if ((r.a == land || r.b == land) && r.tension != realm::Tension::Calm) any = true;
      if (!any) c.fail("expect realm tension: every neighbour of the player's land is calm");
    }
    if (what == "hunger") {
      const realm::KingdomState* k = g.realm.kingdom(land);
      if (!k || k->food > 0.3f) c.fail("expect realm hunger: the player's land has grain");
    }
    return true;
  }
  const std::vector<int> near = nearestSettlements(g);
  const size_t n = (size_t)std::max(0, std::atoi(c.arg(3).c_str()));
  if (n >= near.size()) { c.fail("expect realm: no such settlement"); return true; }
  const Site& s = g.world.sites[(size_t)near[n]];
  const realm::SettlementState* st = g.realm.settlement(s.id);
  const uint16_t f = st ? st->flags : 0;
  uint16_t want = 0;
  if (what == "besieged") want = realm::SS_BESIEGED;
  else if (what == "burned") want = realm::SS_BURNED;
  else if (what == "occupied") want = realm::SS_OCCUPIED;
  else if (what == "famine") want = realm::SS_FAMINE;
  else if (what == "refugees") want = realm::SS_REFUGEES;
  else if (what == "frontier") want = realm::SS_FRONTIER;
  else if (what == "unrest") want = realm::SS_UNREST;
  else if (what == "ruined") want = realm::SS_RUINED;
  else if (what == "rebuilding") want = realm::SS_REBUILDING;
  else if (what == "garrison") want = realm::SS_GARRISON;
  else if (what == "abandoned") want = realm::SS_ABANDONED;
  else {
    c.fail("expect realm besieged|burned|occupied|famine|refugees|frontier|unrest|ruined|rebuilding|garrison|abandoned [n] | war | peace | "
           "rebels | tension | hunger");
    return true;
  }
  if (!(f & want)) c.fail("expect realm " + what + ": " + s.name + " is not");
  return true;
}
EMB_SCRIPT_CMD("expect:realm", "expect realm <settlement state> [n] | war | peace | rebels | tension | hunger (M4)", expRealm);

}  // namespace
