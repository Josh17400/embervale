// M12 SERVICE lane script commands (rpg/view/script_api.h). Phase A (the lead):
//   enlist                     swear to the army of the kingdom whose land the player stands on (fails: why)
//   promote [n]                rise n ranks now (default 1)
//   merit <n>                  n merit (rises when it reaches the rank's need)
//   order <kind> [n]           an order about the n-th nearest settlement: breaksiege | assault | supply | scout |
//                              assassinate | escort | hold (its Campaign quest is added)
//   order done|fail            the newest open order carried out / failed
//   mission <kind>             a mission from the player's land's realm to its nearest neighbour: terms | alliance |
//                              tribute | envoy (its Envoy quest is added)
//   mission arrive             the newest open mission reaches its court (the realm rolls)
//   discharge [desert]         leave the army (honourably when allowed, else a deserter)
//   expect service rank <name|n>   recruit | sergeant | captain | marshal | none (or 0..4)
//   expect service sworn <0|1>     enlisted or not
//   expect service orders <n>      at least n orders given
//   expect service missions <n>    at least n missions given
// SERVICE lane (phase B):
//   service order              the army's generator now (from the sworn kingdom's sieges and wars; fails: nothing to do)
//   service goto order [d]     teleport d tiles (default 8) south of the open order's place (its camp, town, spot)
//   service goto court         teleport to the open mission's receiving capital
//   service court              the kingdom of the land the player stands on goes to war with its nearest neighbour (a
//                              court with terms to send; scripts of the missions)
//   service home               the squad and the order's people sent home
//   expect service squad <n>       at least n squad soldiers in play and standing
//   expect service order open|done|failed|<kind>   the newest order's state or kind (breaksiege, assault, ...)
//   expect service captain <0|1>   a town's captain of the watch in play
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/service_ops.h"
#include "rpg/view/script_api.h"
#include "rpg/world/source.h"

namespace {

ew::Gid landHere(Game& g, int32_t& gx, int32_t& gy) {
  gx = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE);
  gy = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
  if (!g.world.src) return 0;
  g.realm.focusNow(*g.world.src, gx, gy, g.day);
  g.realmNoteSites();
  return g.realm.landOwner(g.world.src->kingdomAt(gx, gy), gx, gy);
}
int nthSettlement(Game& g, int n) {
  std::vector<std::pair<float, int>> d;
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  for (size_t i = 0; i < g.world.sites.size(); i++) {
    const Site& s = g.world.sites[i];
    if (!s.settlement()) continue;
    d.push_back({(s.ex - px) * (s.ex - px) + (s.ey - py) * (s.ey - py), (int)i});
  }
  std::sort(d.begin(), d.end());
  return n >= 0 && n < (int)d.size() ? d[(size_t)n].second : -1;
}

bool cmdEnlist(ScriptCtx& c) {
  int32_t gx, gy;
  const ew::Gid k = landHere(c.game, gx, gy);
  std::string why;
  if (!ServiceOps::enlist(c.game, k, why)) c.fail("enlist: " + why);
  return true;
}
bool cmdPromote(ScriptCtx& c) {
  const int n = std::max(1, std::atoi(c.arg(1).c_str()));
  for (int i = 0; i < n; i++) if (!ServiceOps::promote(c.game)) { c.fail("promote: not enlisted or already marshal"); break; }
  return true;
}
bool cmdMerit(ScriptCtx& c) {
  if (!c.game.service.enlisted()) { c.fail("merit: not enlisted"); return true; }
  ServiceOps::addMerit(c.game, std::max(0, std::atoi(c.arg(1).c_str())));
  return true;
}
bool cmdOrder(ScriptCtx& c) {
  Game& g = c.game;
  const std::string w = c.arg(1);
  if (w == "done" || w == "fail") {
    for (auto it = g.service.orders.rbegin(); it != g.service.orders.rend(); ++it)
      if (it->state == service::OS_OPEN) { ServiceOps::finishOrder(g, it->id, w == "done"); return true; }
    c.fail("order " + w + ": no open order");
    return true;
  }
  static const char* words[] = {"", "breaksiege", "assault", "supply", "scout", "assassinate", "escort", "hold"};
  int k = 0;
  for (int i = 1; i < (int)service::OrderKind::COUNT; i++) if (w == words[i]) k = i;
  if (!k) { c.fail("order breaksiege|assault|supply|scout|assassinate|escort|hold [n] | done | fail"); return true; }
  const int si = nthSettlement(g, std::max(0, std::atoi(c.arg(2).c_str())));
  if (si < 0) { c.fail("order: no settlement loaded"); return true; }
  const Site& s = g.world.sites[(size_t)si];
  if (!ServiceOps::giveOrder(g, (service::OrderKind)k, s.id, g.world.ox + s.ex, g.world.oy + s.ey, 0, 0)) c.fail("order: not enlisted");
  return true;
}
bool cmdMission(ScriptCtx& c) {
  Game& g = c.game;
  const std::string w = c.arg(1);
  if (w == "arrive") {
    for (auto it = g.service.missions.rbegin(); it != g.service.missions.rend(); ++it)
      if (it->state == service::OS_OPEN) { ServiceOps::arrive(g, it->id); return true; }
    c.fail("mission arrive: no open mission");
    return true;
  }
  service::MissionKind k = service::MissionKind::None;
  if (w == "terms") k = service::MissionKind::CarryTerms;
  else if (w == "alliance") k = service::MissionKind::SeekAlliance;
  else if (w == "tribute") k = service::MissionKind::DemandTribute;
  else if (w == "envoy") k = service::MissionKind::FirstEnvoy;
  if (k == service::MissionKind::None) { c.fail("mission terms|alliance|tribute|envoy|arrive"); return true; }
  int32_t gx, gy;
  const ew::Gid from = landHere(g, gx, gy);
  ew::Gid to = 0;
  double bd = 1e30;
  for (const realm::KingdomState& K : g.realm.kingdoms()) {
    if (K.id == from || K.fallen || K.player) continue;
    const realm::SettlementState* cap = g.realm.settlement(K.capital);
    if (!cap) continue;
    const double d = (double)(cap->gx - gx) * (cap->gx - gx) + (double)(cap->gy - gy) * (cap->gy - gy);
    if (d < bd) { bd = d; to = K.id; }
  }
  if (!from || !to || !ServiceOps::giveMission(g, k, from, to)) c.fail("mission: no court to send to");
  return true;
}
bool cmdLeave(ScriptCtx& c) {
  if (!c.game.service.enlisted()) { c.fail("discharge: not enlisted"); return true; }
  ServiceOps::leave(c.game, c.arg(1) == "desert");
  return true;
}
const char* kOrderWords[] = {"", "breaksiege", "assault", "supply", "scout", "assassinate", "escort", "hold"};
bool cmdServiceX(ScriptCtx& c) {
  Game& g = c.game;
  const std::string w = c.arg(1), x = c.arg(2);
  if (w == "order") {
    if (!g.service.enlisted()) { c.fail("service order: not enlisted"); return true; }
    if (!ServiceOps::openOrder(g) && !ServiceOps::generateOrder(g)) c.fail("service order: the sworn kingdom has no siege or war to send you to");
  } else if (w == "goto" && x == "order") {
    const service::Order* o = ServiceOps::openOrder(g);
    if (!o) { c.fail("service goto order: no open order"); return true; }
    const int d = c.arg(3).empty() ? 8 : std::atoi(c.arg(3).c_str());
    g.teleportGlobal(o->gx, o->gy + d);
  } else if (w == "goto" && x == "court") {
    const service::Mission* m = ServiceOps::openMission(g);
    if (!m) { c.fail("service goto court: no open mission"); return true; }
    g.teleportGlobal(m->gx, m->gy + 3);
  } else if (w == "court") {
    int32_t gx, gy;
    const ew::Gid k = landHere(g, gx, gy);
    ew::Gid best = 0;
    double bd = 1e30;
    const realm::KingdomState* K = g.realm.kingdom(k);
    for (const realm::KingdomState& o : g.realm.kingdoms()) {
      if (!K || o.id == k || o.fallen || o.player || realm::Realm::apart(*K, o)) continue;
      const realm::SettlementState* cap = g.realm.settlement(o.capital);
      if (!cap) continue;
      const double d = (double)(cap->gx - gx) * (cap->gx - gx) + (double)(cap->gy - gy) * (cap->gy - gy);
      if (d < bd) { bd = d; best = o.id; }
    }
    if (!k || !best) { c.fail("service court: no kingdom here, or no neighbour"); return true; }
    if (!g.realm.atWar(k, best)) g.realm.forceWar(k, best, g.day);
    g.realmSync();
  } else if (w == "home") {
    ServiceOps::sendSquadHome(g);
  } else {
    c.fail("service order | goto order [d] | goto court | court | home");
  }
  return true;
}

bool expService(ScriptCtx& c) {
  const service::Service& S = c.game.service;
  const std::string what = c.arg(2), v = c.arg(3);
  if (what == "rank") {
    int want = std::atoi(v.c_str());
    for (int i = 0; i < (int)service::Rank::COUNT; i++) {
      std::string n = service::rankName((service::Rank)i);
      for (char& ch : n) ch = (char)(ch >= 'A' && ch <= 'Z' ? ch + 32 : ch);
      if (v == n || (i == 0 && v == "none")) want = i;
    }
    if ((int)S.rank != want) c.fail("expect service rank " + v + ": got " + service::rankName(S.rank));
  } else if (what == "sworn") {
    if ((int)S.enlisted() != std::atoi(v.c_str())) c.fail("expect service sworn " + v);
  } else if (what == "orders") {
    if ((int)S.orders.size() < std::atoi(v.c_str())) c.fail("expect service orders " + v + ": got " + std::to_string(S.orders.size()));
  } else if (what == "missions") {
    if ((int)S.missions.size() < std::atoi(v.c_str())) c.fail("expect service missions " + v + ": got " + std::to_string(S.missions.size()));
  } else if (what == "squad") {
    if (ServiceOps::squadAlive(c.game) < std::atoi(v.c_str())) c.fail("expect service squad " + v + ": got " + std::to_string(ServiceOps::squadAlive(c.game)));
  } else if (what == "order") {
    const service::Order* o = S.orders.empty() ? nullptr : &S.orders.back();
    if (!o) { c.fail("expect service order " + v + ": no order"); return true; }
    bool ok = false;
    if (v == "open") ok = o->state == service::OS_OPEN;
    else if (v == "done") ok = o->state == service::OS_DONE;
    else if (v == "failed") ok = o->state == service::OS_FAILED;
    else ok = (int)o->kind < (int)service::OrderKind::COUNT && v == kOrderWords[(int)o->kind];
    if (!ok) c.fail("expect service order " + v + ": got " + kOrderWords[std::clamp((int)o->kind, 0, 7)] + " state " + std::to_string(o->state));
  } else if (what == "captain") {
    bool any = false;
    for (size_t i = 1; i < c.game.actors.size(); i++) any |= ServiceOps::isServiceCaptain(c.game.actors[i]) && c.game.actors[i].st != AState::Dead;
    if ((int)any != std::atoi(v.c_str())) c.fail("expect service captain " + v);
  } else {
    c.fail("expect service rank|sworn|orders|missions|squad|order|captain <v>");
  }
  return true;
}

}  // namespace

EMB_SCRIPT_CMD("enlist", "enlist: swear to the army of the kingdom whose land the player stands on (M12)", cmdEnlist);
EMB_SCRIPT_CMD("promote", "promote [n]: rise n ranks now (M12)", cmdPromote);
EMB_SCRIPT_CMD("merit", "merit <n>: merit toward the next rank (M12)", cmdMerit);
EMB_SCRIPT_CMD("order", "order breaksiege|assault|supply|scout|assassinate|escort|hold [n] | done | fail (M12)", cmdOrder);
EMB_SCRIPT_CMD("mission", "mission terms|alliance|tribute|envoy | arrive (M12)", cmdMission);
EMB_SCRIPT_CMD("discharge", "discharge [desert]: leave the army (M12)", cmdLeave);
EMB_SCRIPT_CMD("service", "service order | goto order [d] | goto court | court | home (M12 SERVICE)", cmdServiceX);
EMB_SCRIPT_CMD("expect:service", "expect service rank <name|n> | sworn <0|1> | orders <n> | missions <n> | squad <n> | order open|done|failed|<kind> | captain <0|1>", expService);
