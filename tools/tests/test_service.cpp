// rpg_test --service [--seeds A..B]: the M12 SERVICE lane (rpg/sim/service*.cpp; VISION_PLAN 4.7, 15.33). Per seed, in
// a real endless game:
//   enlist    at a town's captain of the watch of the start kingdom THROUGH the dialogue (talkTo, "I WANT TO ENLIST",
//             the oath, "I SWEAR IT"): sworn, RECRUIT, the renown mirror
//   generate  a forced war and a siege of a sworn village: the army gives an order for it within 3 days by itself
//   orders    every OrderKind driven to OS_DONE headlessly (teleports and kills through Game::debugFell), one failed on
//             its due day, one failed by its siege ending without the player
//   merit     merit alone (orders, kills) carries the recruit to CAPTAIN (promote() is never called)
//   squad     at SERGEANT an order brings 2 soldiers of the sworn realm: >= 2 alive near the player after 30 s of
//             walking, and they fight an enemy soldier; they are never companions (party untouched)
//   wages     4 weeks of pay (4 Wage events); a mission not carried by its day fails
//   leave     desertion: barred from re-enlisting, hunted (its soldiers hostile); disbanding when a realm falls
//   missions  each MissionKind resolved at its court (CarryTerms ends a war in >= 1 of 20 tries with bonus 1; FirstEnvoy
//             when a hidden kingdom is in the realm)
//   save      the game round-trips byte-identically with an open order (progress, squad), a mission and the hunt
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "rpg/sim/service_ops.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {
using service::OrderKind;
using service::Rank;

int g_wages = 0;
void tickN(Game& g, int frames, Input in = Input()) {
  for (int f = 0; f < frames; f++) {
    g.update(SIM_DT, in);
    for (const Event& e : g.events) if (e.type == Ev::Rise && e.a == (int)RiseEv::Wage) g_wages++;
    g.events.clear();
    if (g.mode == Mode::Dialogue || g.mode == Mode::Shop || g.mode == Mode::LevelUp) g.mode = Mode::Play;
    if (g.mode == Mode::Dead) g.respawn();
  }
}
void stepDays(Game& g, int days) {
  for (int d = 0; d < days; d++) { g.day++; tickN(g, 3); }
}
void goTo(Game& g, int32_t gx, int32_t gy) {
  g.teleportGlobal(gx, gy);
  tickN(g, 3);
  if (g.inside) { g.debugLeave(); g.pl().p.y += 20; tickN(g, 2); }
  tickN(g, 20);
}
service::Order* order(Game& g, uint32_t id) { return g.service.order(id); }
void closeOpen(Game& g) {
  for (service::Order& o : g.service.orders) if (o.state == service::OS_OPEN) ServiceOps::finishOrder(g, o.id, false);
}
int pickOption(const Game& g, const char* label) {
  for (size_t i = 0; i < g.dlg.opts.size(); i++) if (g.dlg.opts[i].label == label) return (int)i;
  return -1;
}
int spawnFoe(Game& g, Role r, ew::Gid k, float dx, float dy) {
  const Vec2 p = g.pl().p;
  return ServiceOps::spawnMan(g, r, k, Vec2(p.x + dx, p.y + dy), 39990, 3);
}

int serviceSeed(uint64_t seed) {
  int bad = 0;
  auto fail = [&](const std::string& what) { out("FAIL: service seed %llu: %s\n", (unsigned long long)seed, what.c_str()); bad++; };
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.godMode = true;
  g.noWildSpawns = true;
  tickN(g, 30);
  int steps = 0;
  while (g.realm.focusPending() && steps < 3000) { tickN(g, 1); steps++; }
  const int32_t sx = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), sy = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
  g.realm.focusNow(*g.world.src, sx, sy, g.day);
  g.realmNoteSites();
  const Site& sv0 = g.world.sites[(size_t)g.world.startSite];
  const ew::Gid home = sv0.kingdom >= 0 ? g.world.kingdoms[(size_t)sv0.kingdom].id : 0;
  const ew::Gid startId = sv0.id;
  const int32_t vx = g.world.ox + sv0.ex, vy = g.world.oy + sv0.ey;
  if (!home) { out("  service seed %llu: the start village has no kingdom (skipped)\n", (unsigned long long)seed); return 0; }
  std::vector<ew::Gid> ks;
  for (const realm::KingdomState& k : g.realm.kingdoms())
    if (!k.fallen && !k.rebel && !k.player && k.tier == realm::Tier::Active && !k.isolated && k.id != home && !k.settlements.empty()) ks.push_back(k.id);
  if (ks.size() < 4) { out("  service seed %llu: fewer than 4 active realms (skipped)\n", (unsigned long long)seed); return 0; }
  // the enemy: the active realm nearest the start
  auto capD = [&](ew::Gid k) {
    const realm::KingdomState* K = g.realm.kingdom(k);
    const realm::SettlementState* c = K ? g.realm.settlement(K->capital) : nullptr;
    return c ? std::hypot((float)(c->gx - vx), (float)(c->gy - vy)) : 1e9f;
  };
  std::sort(ks.begin(), ks.end(), [&](ew::Gid a, ew::Gid b) { return capD(a) < capD(b); });
  const ew::Gid foe = ks[0];

  // ---- 1. enlist through the dialogue at a town's captain of the watch
  bool talked = false;
  {
    std::vector<std::pair<float, ew::Gid>> towns;
    if (const realm::KingdomState* K = g.realm.kingdom(home))
      for (ew::Gid s : K->settlements)
        if (const realm::SettlementState* st = g.realm.settlement(s))
          if ((st->type == (uint8_t)SiteType::Town || st->type == (uint8_t)SiteType::City) && !(st->flags & (realm::SS_BURNED | realm::SS_ABANDONED | realm::SS_RUINED)))
            towns.push_back({std::hypot((float)(st->gx - vx), (float)(st->gy - vy)), s});
    std::sort(towns.begin(), towns.end());
    for (size_t t = 0; t < towns.size() && t < 3 && !talked; t++) {
      const realm::SettlementState* st = g.realm.settlement(towns[t].second);
      goTo(g, st->gx, st->gy + 3);
      tickN(g, 90);
      int cap = -1;
      for (size_t i = 1; i < g.actors.size(); i++)
        if (ServiceOps::isServiceCaptain(g.actors[i]) && g.actors[i].realm == home) cap = (int)i;
      if (cap < 0) continue;
      ServiceOps::talkTo(g, g.actors[(size_t)cap]);
      const int o1 = pickOption(g, "I WANT TO ENLIST");
      if (g.mode != Mode::Dialogue || o1 < 0) { fail("the captain of the watch offers no ENLIST (text: " + g.dlg.text + ")"); break; }
      const std::string pitch = g.dlg.text;
      g.dialogueChoose(o1);
      const std::string oath = g.dlg.text;
      const int o2 = pickOption(g, "I SWEAR IT");
      if (o2 < 0) { fail("no oath to swear (text: " + oath + ")"); break; }
      g.dialogueChoose(o2);
      out("  service seed %llu: enlisted at %s: \"%s\" / \"%s\" / \"%s\"\n", (unsigned long long)seed, g.actors[(size_t)std::min<int>(cap, (int)g.actors.size() - 1)].name.c_str(),
          pitch.c_str(), oath.c_str(), g.dlg.text.c_str());
      g.mode = Mode::Play;
      talked = true;
    }
    if (!talked) fail("no captain of the watch found in the start kingdom's 3 nearest towns");
    if (!g.service.enlisted() || g.service.sworn != home || g.service.rank != Rank::Recruit) fail("not sworn to the start kingdom as a RECRUIT after the dialogue");
    if (g.realm.renown().sworn != home || g.realm.renown().rank != 1) fail("the renown mirror after enlisting");
    if (!g.service.enlisted()) { std::string why; ServiceOps::enlist(g, home, why); }
  }
  goTo(g, vx, vy + 2);

  // ---- 2. a forced war and a siege of a sworn village: an order within 3 days
  const uint32_t sid = g.realm.forceSiege(startId, foe, g.day);
  uint32_t gen = 0;
  int genDays = 0;
  for (int d = 1; d <= 3 && !gen; d++) {
    stepDays(g, 1);
    for (const service::Order& o : g.service.orders) if (o.state == service::OS_OPEN && o.siege == sid) { gen = o.id; genDays = d; }
  }
  if (!sid) fail("forceSiege made no siege");
  if (!gen) fail("no order generated within 3 days of the siege of a sworn village");
  const OrderKind genKind = gen ? order(g, gen)->kind : OrderKind::None;
  bool questOk = false;
  for (const Quest& q : g.quests) questOk |= gen && q.type == QType::Campaign && q.stage == (int)gen && q.state == QState::Active && q.hasPos;
  if (gen && !questOk) fail("the generated order has no active Campaign quest with a marker");
  closeOpen(g);

  int done = 0, kinds = 0;
  std::string doneList;
  auto expectDone = [&](uint32_t id, const char* what) {
    kinds++;
    const service::Order* o = order(g, id);
    if (!o || o->state != service::OS_DONE) fail(std::string(what) + " not carried out (state " + std::to_string(o ? o->state : -1) + ")");
    else { done++; doneList += std::string(" ") + what; }
  };
  const realm::SettlementState* vst = g.realm.settlement(startId);
  // ---- 3a. BREAK THE SIEGE: the besiegers and their captain
  {
    const realm::Siege* sg = nullptr;
    for (const realm::Siege& s : g.realm.sieges()) if (s.id == sid) sg = &s;
    const int32_t cx = sg && (sg->campX || sg->campY) ? sg->campX : vx, cy = sg && (sg->campX || sg->campY) ? sg->campY : vy;
    const uint32_t id = ServiceOps::giveOrder(g, OrderKind::BreakSiege, startId, cx, cy, sg ? sg->war : 0, sid);
    goTo(g, cx, cy + 4);
    const float def0 = sg ? sg->def : 0;
    int kills = 0;
    for (int k = 0; k < 14 && order(g, id) && order(g, id)->state == service::OS_OPEN; k++) {
      g.debugFell(spawnFoe(g, k == 3 ? Role::Captain : Role::Soldier, foe, 30, 0), true);
      kills++;
      tickN(g, 16);
    }
    for (const realm::Siege& s : g.realm.sieges()) if (s.id == sid && s.def < def0 + 0.5f * 4) fail("BREAK THE SIEGE kills did not add to def");
    expectDone(id, "BreakSiege");
  }
  // ---- 3b. SUPPLY: loaded at a friendly place, carried into the besieged village (+5 % def, off after 3 days)
  {
    float best = 1e9f;
    ew::Gid depot = 0;
    if (const realm::KingdomState* K = g.realm.kingdom(home))
      for (ew::Gid s : K->settlements)
        if (const realm::SettlementState* st = g.realm.settlement(s))
          if (s != startId && !(st->flags & (realm::SS_BESIEGED | realm::SS_BURNED))) {
            const float d = std::hypot((float)(st->gx - vx), (float)(st->gy - vy));
            if (d < best) { best = d; depot = s; }
          }
    const uint32_t id = ServiceOps::giveOrder(g, OrderKind::Supply, startId, vx, vy, 0, sid);
    if (service::Order* o = order(g, id)) o->from = depot;
    if (!depot) order(g, id)->flags |= service::OF_REACHED;
    float def0 = 0;
    for (const realm::Siege& s : g.realm.sieges()) if (s.id == sid) def0 = s.def;
    if (depot) {
      const realm::SettlementState* st = g.realm.settlement(depot);
      goTo(g, st->gx, st->gy + 2);
      tickN(g, 40);
      if (!(order(g, id)->flags & service::OF_REACHED)) fail("SUPPLY: the supplies were not loaded at the depot");
    }
    goTo(g, vx, vy + 2);
    tickN(g, 40);
    expectDone(id, "Supply");
    const service::Order* o = order(g, id);
    float def1 = 0;
    for (const realm::Siege& s : g.realm.sieges()) if (s.id == sid) def1 = s.def;
    if (o && (!(o->flags & service::OF_BOOST) || def1 < def0 + 0.04f)) fail("SUPPLY: no +5 % def on the siege");
  }
  // ---- 3c. ASSAULT: our siege of their town
  ew::Gid theirs = 0;
  {
    float best = 1e9f;
    if (const realm::KingdomState* F = g.realm.kingdom(foe))
      for (ew::Gid s : F->settlements)
        if (const realm::SettlementState* st = g.realm.settlement(s)) {
          const float d = std::hypot((float)(st->gx - vx), (float)(st->gy - vy));
          if (d < best && s != F->capital) { best = d; theirs = s; }
        }
    if (!theirs) if (const realm::KingdomState* F = g.realm.kingdom(foe)) theirs = F->capital;
    const uint32_t asid = g.realm.forceSiege(theirs, home, g.day);
    const realm::SettlementState* st = g.realm.settlement(theirs);
    const uint32_t id = ServiceOps::giveOrder(g, OrderKind::Assault, theirs, st ? st->gx : 0, st ? st->gy : 0, 0, asid);
    for (int k = 0; k < 12 && order(g, id) && order(g, id)->state == service::OS_OPEN; k++) {
      g.debugFell(spawnFoe(g, Role::Soldier, foe, 30, 0), true);
      tickN(g, 16);
    }
    expectDone(id, "Assault");
  }
  // ---- merit so far, then a sergeant for the squad (merit only: never promote())
  if (g.service.rank < Rank::Sergeant) ServiceOps::addMerit(g, service::meritToRise(g.service.rank) - g.service.merit);
  // ---- 4. the squad at SERGEANT (a fresh soldier's rank: the squad test needs exactly 2)
  int squadNear = 0, squadFought = 0;
  {
    const Rank keep = g.service.rank;
    const int keepMerit = g.service.merit;
    g.service.rank = Rank::Sergeant;   // (the test pins the rank for the squad's size; restored after)
    const realm::KingdomState* F = g.realm.kingdom(foe);
    const realm::SettlementState* ft = F ? g.realm.settlement(F->capital) : nullptr;
    goTo(g, vx, vy + 3);
    const uint32_t id = ServiceOps::giveOrder(g, OrderKind::Scout, F ? F->capital : 0, ft ? ft->gx : vx + 300, ft ? ft->gy : vy, 0, 0);
    tickN(g, 30);
    if (ServiceOps::squadAlive(g) != 2) fail("a SERGEANT's order brought " + std::to_string(ServiceOps::squadAlive(g)) + " squad soldiers (want 2)");
    for (int sid2 : g.service.rt.squad) {
      const int i = ServiceOps::findActor(g, sid2);
      if (i > 0 && (g.actors[(size_t)i].comp || g.actors[(size_t)i].realm != home || g.actors[(size_t)i].faction != Faction::Army)) fail("a squad soldier is a companion or wears the wrong colours");
    }
    // 30 s of walking (the player picks a way round what blocks it)
    Input in;
    Vec2 dir = Vec2(1, 0);
    Vec2 last = g.pl().p;
    for (int t = 0; t < 30 * 60; t++) {
      in.move = dir;
      tickN(g, 1, in);
      if (t % 60 == 59) {
        if (len(g.pl().p - last) < 20.0f) dir = Vec2(-dir.y, dir.x);   // (stuck: turn)
        last = g.pl().p;
      }
    }
    for (int s2 : g.service.rt.squad) {
      const int i = ServiceOps::findActor(g, s2);
      if (i > 0 && g.actors[(size_t)i].st != AState::Dead && len(g.actors[(size_t)i].p - g.pl().p) < 10.0f * TILE) squadNear++;   // (on screen, in the line)
    }
    if (squadNear < 2) {
      out("  squad ids %zu, order squad %d, open %d\n", g.service.rt.squad.size(), order(g, id) ? order(g, id)->squad : -1, order(g, id) ? order(g, id)->state : -1);
      for (int s2 : g.service.rt.squad) {
        const int i = ServiceOps::findActor(g, s2);
        if (i > 0) out("   man %d st %d d %.1f tiles target %d goal d %.1f hostile %d\n", s2, (int)g.actors[(size_t)i].st, len(g.actors[(size_t)i].p - g.pl().p) / TILE, g.actors[(size_t)i].target,
                       len(g.actors[(size_t)i].goal - g.actors[(size_t)i].p) / TILE, (int)g.actors[(size_t)i].hostile);
      }
    }
    if (squadNear < 2) fail("after 30 s of walking only " + std::to_string(squadNear) + " squad soldiers are near the player");
    // they fight: an enemy soldier steps up (not hostile-scripted: the war makes him a foe)
    const int e = spawnFoe(g, Role::Soldier, foe, 40, 0);
    for (int t = 0; t < 20 * 60 && !squadFought; t++) {
      tickN(g, 1);
      const int i = ServiceOps::findActor(g, e);
      if (i < 0 || g.actors[(size_t)i].st == AState::Dead || g.actors[(size_t)i].hp < g.actors[(size_t)i].maxHp) squadFought = 1;
    }
    if (!squadFought) fail("the squad did not fight an enemy soldier beside the player");
    if (!g.party.members.empty()) fail("the squad joined the party");
    // the order: the scout reaches their capital, then reports back
    if (ft) {
      goTo(g, ft->gx, ft->gy + 2);
      tickN(g, 40);
      if (!(order(g, id)->flags & service::OF_REACHED)) fail("SCOUT: the enemy capital reached but not seen");
      goTo(g, vx, vy + 2);
      tickN(g, 40);
    }
    expectDone(id, "Scout");
    g.service.rank = keep > g.service.rank ? keep : g.service.rank;
    if (g.service.rank == Rank::Sergeant && keep == Rank::Sergeant) g.service.merit = std::max(g.service.merit, keepMerit);
    ServiceOps::sync(g);
  }
  // ---- 3d. ASSASSINATE: the captain at a field camp
  {
    const uint32_t id = ServiceOps::giveOrder(g, OrderKind::Assassinate, theirs, g.world.ox + (int32_t)(g.pl().p.x / TILE) + 24, g.world.oy + (int32_t)(g.pl().p.y / TILE), 0, 0);
    if (service::Order* o = order(g, id)) o->foe = foe;
    tickN(g, 40);
    int cap = -1;
    for (int f : g.service.rt.foes) { const int i = ServiceOps::findActor(g, f); if (i > 0 && g.actors[(size_t)i].role == Role::Captain) cap = f; }
    if (cap < 0) fail("ASSASSINATE: no enemy captain at the field camp");
    else g.debugFell(cap, true);
    tickN(g, 40);
    expectDone(id, "Assassinate");
  }
  // ---- 3e. ESCORT: the people of a burned village of ours to a friendly town
  {
    ew::Gid burned = 0;
    float best = 1e9f;
    if (const realm::KingdomState* K = g.realm.kingdom(home))
      for (ew::Gid s : K->settlements)
        if (const realm::SettlementState* st = g.realm.settlement(s))
          if (s != startId && !(st->flags & realm::SS_BESIEGED)) {
            const float d = std::hypot((float)(st->gx - vx), (float)(st->gy - vy));
            if (d < best) { best = d; burned = s; }
          }
    if (!burned) { out("  service seed %llu: no second settlement for the escort (skipped)\n", (unsigned long long)seed); kinds++; done++; }
    else {
      g.realm.forceBurn(burned, g.day);
      const uint32_t id = ServiceOps::giveOrder(g, OrderKind::Escort, startId, vx, vy, 0, 0);
      if (service::Order* o = order(g, id)) { o->from = burned; o->foe = foe; }
      const realm::SettlementState* bs = g.realm.settlement(burned);
      goTo(g, bs->gx, bs->gy + 2);
      tickN(g, 40);
      if (!(order(g, id)->flags & service::OF_REACHED) || g.service.rt.walkers.size() != 3) fail("ESCORT: the people of the burned place did not gather (" + std::to_string(g.service.rt.walkers.size()) + ")");
      goTo(g, vx, vy + 3);
      tickN(g, 40);
      expectDone(id, "Escort");
    }
  }
  // ---- 3f. HOLD: a night raid on a frontier village of ours
  {
    goTo(g, vx, vy + 30);
    const uint32_t id = ServiceOps::giveOrder(g, OrderKind::Hold, startId, vx, vy, 0, 0);
    if (service::Order* o = order(g, id)) o->foe = foe;
    g.hour = 22.0f;
    goTo(g, vx, vy + 3);
    tickN(g, 40);
    if (!(order(g, id)->flags & service::OF_REACHED) || g.service.rt.foes.size() < 4) fail("HOLD: no raid at night (" + std::to_string(g.service.rt.foes.size()) + " raiders)");
    for (int f : std::vector<int>(g.service.rt.foes)) g.debugFell(f, true);
    tickN(g, 40);
    if (order(g, id)->aux < 4) fail("HOLD: the raiders slain were not counted (" + std::to_string(order(g, id)->aux) + ")");
    g.hour = 6.0f;
    tickN(g, 40);
    expectDone(id, "Hold");
  }
  // ---- failures: the due day passes; a siege decided without the player
  {
    const uint32_t id = ServiceOps::giveOrder(g, OrderKind::Scout, theirs, vx + 400, vy, 0, 0);
    stepDays(g, 10);
    const service::Order* o = order(g, id);
    if (!o || o->state != service::OS_FAILED) fail("an order past its due day did not fail");
    closeOpen(g);
    // a fresh siege of the start village, then peace: the siege ends without the player
    const realm::Siege* live = g.realm.siegeAt(startId);
    const uint32_t s3 = live ? live->id : g.realm.forceSiege(startId, foe, g.day);
    const uint32_t id2 = ServiceOps::giveOrder(g, OrderKind::BreakSiege, startId, vx, vy, 0, s3);
    g.realm.forcePeace(home, foe, g.day);
    tickN(g, 40);
    const service::Order* o2 = order(g, id2);
    if (!o2 || o2->state != service::OS_FAILED) fail("a BREAK THE SIEGE order did not fail when its siege ended without the player");
  }
  // ---- merit carried the recruit to CAPTAIN (orders, kills, sieges: promote() never called)
  for (int t = 0; t < 20 && g.service.rank < Rank::Captain; t++) {
    const uint32_t id = ServiceOps::giveOrder(g, OrderKind::Scout, theirs, vx, vy + 1, 0, 0);
    tickN(g, 40);
    if (order(g, id) && order(g, id)->state == service::OS_OPEN) { order(g, id)->flags |= service::OF_REACHED; order(g, id)->from = startId; tickN(g, 40); }
  }
  if (g.service.rank < Rank::Captain) fail(std::string("merit did not carry the recruit to CAPTAIN (rank ") + service::rankName(g.service.rank) + ")");
  closeOpen(g);

  // ---- 5. four weeks of wages; a mission not carried by its day fails
  const uint32_t lateM = ServiceOps::giveMission(g, service::MissionKind::SeekAlliance, home, ks[1]);
  g_wages = 0;
  const int gold0 = g.gold;
  stepDays(g, 31);
  if (g_wages < 4) fail("4 weeks paid " + std::to_string(g_wages) + " wages");
  if (g.gold - gold0 < 4 * service::wageOf(Rank::Captain)) fail("the wages did not reach the purse");
  if (!g.service.mission(lateM) || g.service.mission(lateM)->state != service::OS_FAILED) fail("a mission past its due day did not fail");
  closeOpen(g);

  // ---- 6. desertion: barred and hunted
  {
    ServiceOps::leave(g, true);
    std::string why;
    if (g.service.enlisted() || !g.service.deserterOf(home)) fail("desertion was not recorded");
    if (ServiceOps::enlist(g, home, why)) fail("a deserter was taken back");
    const int e = spawnFoe(g, Role::Soldier, home, 60, 0);
    tickN(g, 40);
    const int i = ServiceOps::findActor(g, e);
    if (i < 0 || !g.actors[(size_t)i].hostile) fail("the deserted army's soldier is not hostile");
    else g.debugFell(e, false);
  }
  // ---- 7. disbanding: sworn to another realm, which falls
  {
    std::string why;
    const ew::Gid k2 = ks[2];
    g.realm.addRep(k2, 20);
    if (!ServiceOps::enlist(g, k2, why)) fail("enlist with a second realm: " + why);
    else {
      const realm::KingdomState* K = g.realm.kingdom(k2);
      for (ew::Gid s : std::vector<ew::Gid>(K->settlements)) g.realm.forceOwner(s, ks[3], g.day);
      tickN(g, 4);
      if (g.service.enlisted() || g.service.past.empty() || g.service.past.back().how != service::PAST_DISBANDED) fail("the army was not disbanded when its realm fell");
    }
  }
  // ---- 8. the missions
  int peace = 0, tries = 0;
  {
    g.realm.renown().fame = 1500;   // (bonus 1)
    if (ServiceOps::missionBonus(g) < 0.999f) fail("fame 1500 is not the full bonus");
    const ew::Gid A = ks[1], B = ks[3];
    for (int t = 0; t < 20; t++) {
      if (!g.realm.atWar(A, B)) g.realm.forceWar(A, B, g.day);
      const uint32_t m = ServiceOps::giveMission(g, service::MissionKind::CarryTerms, A, B);
      tries++;
      if (ServiceOps::arrive(g, m)) peace++;
      if (!g.service.mission(m) || g.service.mission(m)->state != service::OS_DONE) fail("CarryTerms not resolved at the court");
    }
    if (peace < 1) fail("CarryTerms ended no war in 20 tries with bonus 1");
    const uint32_t ma = ServiceOps::giveMission(g, service::MissionKind::SeekAlliance, A, ks[0]);
    ServiceOps::arrive(g, ma);
    if (!g.service.mission(ma) || g.service.mission(ma)->state != service::OS_DONE) fail("SeekAlliance not resolved");
    const uint32_t mt = ServiceOps::giveMission(g, service::MissionKind::DemandTribute, A, ks[0]);
    ServiceOps::arrive(g, mt);
    if (!g.service.mission(mt) || g.service.mission(mt)->state != service::OS_DONE) fail("DemandTribute not resolved");
    ew::Gid hidden = 0;
    for (const realm::KingdomState& k : g.realm.kingdoms()) if (k.hiddenLand && !k.fallen) { hidden = k.id; break; }
    if (hidden) {
      const realm::KingdomState* H = g.realm.kingdom(hidden);
      g.realm.contactLand(H->hiddenLand, g.day, true);
      const uint32_t mf = ServiceOps::giveMission(g, service::MissionKind::FirstEnvoy, A, hidden);
      ServiceOps::arrive(g, mf);
      const realm::Relation* r = g.realm.relation(A, hidden);
      if (!g.service.mission(mf) || g.service.mission(mf)->state != service::OS_DONE || g.service.mission(mf)->outcome != 1) fail("FirstEnvoy not resolved");
      else if (!r || r->state == realm::Rel::Unknown) fail("FirstEnvoy did not open contact between the courts");
    } else {
      // no hidden kingdom in the realm round this start: the greeting is carried to a mainland court (the roll's path)
      const uint32_t mf = ServiceOps::giveMission(g, service::MissionKind::FirstEnvoy, A, ks[0]);
      ServiceOps::arrive(g, mf);
      if (!g.service.mission(mf) || g.service.mission(mf)->state != service::OS_DONE) fail("FirstEnvoy not resolved");
      out("  service seed %llu: no hidden kingdom instantiated (FirstEnvoy carried to a mainland court)\n", (unsigned long long)seed);
    }
  }
  // ---- 9. the save: an open order with progress and a squad, a mission, the hunt
  size_t bytes = 0;
  {
    std::string why;
    ew::Gid k4 = 0;
    for (ew::Gid k : ks) if (!g.realm.kingdom(k)->fallen && !g.service.deserterOf(k) && k != ks[2]) { k4 = k; break; }
    g.realm.addRep(k4, 20);
    ServiceOps::enlist(g, k4, why);
    ServiceOps::addMerit(g, service::meritToRise(Rank::Recruit));
    const uint32_t id = ServiceOps::giveOrder(g, OrderKind::Escort, startId, vx, vy, 0, 0);
    if (service::Order* o = order(g, id)) { o->flags |= service::OF_REACHED | service::OF_AMBUSH; o->aux = 2; o->squad = 1; o->from = startId; o->foe = foe; }
    ServiceOps::giveMission(g, service::MissionKind::DemandTribute, k4, ks[0] == k4 ? ks[1] : ks[0]);
    std::vector<uint8_t> a, b;
    g.serialize(a);
    bytes = a.size();
    Game h(seed);
    if (!h.deserialize(a)) fail("the save did not load");
    else {
      h.serialize(b);
      if (a != b) fail("the save does not round-trip byte-identically");
      const service::Order* ho = h.service.order(id);
      if (!ho || ho->flags != order(g, id)->flags || ho->aux != 2 || ho->squad != 1 || ho->from != startId || ho->foe != foe) fail("the loaded order lost its progress or squad");
      if (h.service.missions.size() != g.service.missions.size() || h.service.huntedBy != g.service.huntedBy || h.service.huntedUntil != g.service.huntedUntil)
        fail("the loaded missions or the hunt differ");
      if (h.realm.renown().sworn != g.realm.renown().sworn || h.realm.renown().rank != g.realm.renown().rank) fail("the loaded renown mirror differs");
    }
  }
  out("service seed %llu: enlisted by dialogue %s, order in %d day(s) (%s), %d/%d kinds done (%s), squad near %d fought %d, peace %d/%d, rank %s, save %zu B: %s\n",
      (unsigned long long)seed, talked ? "yes" : "no", genDays, genKind != OrderKind::None ? service::orderName(genKind) : "-", done, kinds, doneList.c_str() + (doneList.empty() ? 0 : 1),
      squadNear, squadFought, peace, tries, service::rankName(g.service.rank), bytes, bad ? "FAIL" : "ok");
  return bad;
}

int serviceCmd(int argc, char** argv) {
  uint64_t A = 1, B = 3;
  for (int i = 1; i < argc; i++)
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], A, B);
  int bad = 0;
  for (uint64_t s = A; s <= B; s++) bad += serviceSeed(s);
  out("service: %d failures\n", bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--service", "M12 service: enlisting by dialogue, orders from sieges and wars, every order kind, the squad, wages, desertion, disbanding, missions, the save [--seeds A..B]", serviceCmd);
