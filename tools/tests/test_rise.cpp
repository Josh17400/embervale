// rpg_test --rise [--seeds A..B]: the M12 "Rise and Fall" contracts (VISION_PLAN 15.33). Phase A (lead), per seed in
// a real endless game:
//   realm   a bandit camp noted and grown into a warlord realm that holds a village (the banners follow), an alliance,
//           a vassal with tribute, a fallen realm's successor with its claim, a ruin resettled, a world boss's raid, the
//           diplomacy rolls; the player's realm (a KO_PLAYER realm) is never split, never loses its ruler, never
//           declares a war over 120 ticked days; if its enemies take its last holding it falls with news and its wars
//           end (no zombie realm), otherwise it still holds land.
//   service enlisting with the start kingdom (the renown mirror), merit to Sergeant, an order's Campaign quest carried
//           out, a mission rolled at its court, the weekly wage.
//   lord    the start village granted, taxes into the coffers over 31 days, tribute paid, independence (the village's
//           banner is the player's own realm).
//   save    the game round-trips byte-identically with all of it, and the loaded copy holds the same facts.
// The lanes grow this file's checks next to the code they add (REALM: the tick making these by itself; SERVICE: orders
// from sieges, the squad, missions; LORD: the steward, works, conquest, the crown).
// (REALM lane, phase B) every deep-sim event has a news line of its own; the player's realm after 120 days is reported
// (its settlements, the wars on it); and a LONG RUN per seed (5000 days of a headless realm round the start, the player
// walking a kingdom cell now and then and noting camps and lairs as the game does, NO forcing call) must, over the seeds,
// raise warbands, warlords, successors, resettlements and rebuilt ruins, boss raids and wakings, alliances, allies called
// to war, vassals, tributes and risen realms that fell, all by the tick itself, and never act for the player.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "rpg/sim/foes.h"
#include "rpg/sim/lord_ops.h"
#include "rpg/sim/service_ops.h"
#include "rpg/story/story.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

void stepDays(Game& g, int days) {
  for (int d = 0; d < days; d++) {
    g.day++;
    for (int i = 0; i < 3; i++) { g.update(SIM_DT, Input()); g.events.clear(); }
  }
}

int riseSeed(uint64_t seed) {
  int bad = 0;
  auto fail = [&](const char* what) { out("FAIL: rise seed %llu: %s\n", (unsigned long long)seed, what); bad++; };
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.godMode = true;
  g.noWildSpawns = true;
  for (int i = 0; i < 30; i++) { g.update(SIM_DT, Input()); g.events.clear(); }
  int steps = 0;
  while (g.realm.focusPending() && steps < 3000) { g.update(SIM_DT, Input()); g.events.clear(); steps++; }
  const int32_t gx = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), gy = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
  g.realm.focusNow(*g.world.src, gx, gy, g.day);
  g.realmNoteSites();
  const Site& sv = g.world.sites[(size_t)g.world.startSite];
  const ew::Gid home = sv.kingdom >= 0 ? g.world.kingdoms[(size_t)sv.kingdom].id : 0;
  int warlords = 0, successors = 0;

  // ---- realm: a warlord from a camp
  for (const Site& s : g.world.sites)
    if (s.type == SiteType::BanditCamp) g.realm.noteCamp(s.id, g.world.ox + s.ex, g.world.oy + s.ey, 0, g.day);
  if (!g.realm.camps().empty()) {
    const ew::Gid camp = g.realm.camps()[0].site;
    const ew::Gid wl = g.realm.forceWarlord(camp, g.day);
    if (!wl) fail("forceWarlord made no warlord realm (no village within 1200 tiles?)");
    else {
      const realm::KingdomState* W = g.realm.kingdom(wl);
      if (!W || W->origin != realm::KO_WARLORD || W->settlements.empty() || W->camp != camp) fail("the warlord realm's record");
      const realm::Camp* c = g.realm.camp(camp);
      if (!c || !(c->flags & realm::CF_WARLORD) || c->realm != wl) fail("the camp does not name its warlord realm");
      g.realmSync();
      if (W && !W->settlements.empty()) {
        const int si = g.world.siteHandle(W->settlements[0]);
        if (si >= 0 && (g.world.sites[(size_t)si].kingdom < 0 || g.world.kingdoms[(size_t)g.world.sites[(size_t)si].kingdom].id != wl))
          fail("the warlord's village does not fly its banner");
      }
      warlords++;
    }
  } else {
    out("  rise seed %llu: no bandit camp loaded round the start (warlord check skipped)\n", (unsigned long long)seed);
  }
  // ---- alliances, vassals, diplomacy rolls
  std::vector<ew::Gid> ks;
  for (const realm::KingdomState& k : g.realm.kingdoms()) if (!k.fallen && !k.rebel && k.tier == realm::Tier::Active && !k.isolated && k.id != home) ks.push_back(k.id);   // (the start kingdom stays whole for the service and lordship checks)
  if (ks.size() >= 4) {
    g.realm.forceAlliance(ks[0], ks[1], g.day);
    const realm::Relation* r = g.realm.relation(ks[0], ks[1]);
    if (!r || r->state != realm::Rel::Alliance) fail("forceAlliance");
    g.realm.forceVassal(ks[2], ks[0], g.day);
    const realm::KingdomState* V = g.realm.kingdom(ks[2]);
    if (!V || V->overlord != ks[0] || V->tribute <= 0) fail("forceVassal");
    g.realm.forceWar(ks[3], ks[1], g.day);
    int peace = 0;
    for (int t = 0; t < 40 && g.realm.atWar(ks[3], ks[1]); t++) peace += g.realm.peaceRoll(ks[3], ks[1], 1.0f, g.day);
    if (g.realm.atWar(ks[3], ks[1])) fail("40 peace rolls with the full bonus never ended the war");
    g.realm.allianceRoll(ks[1], ks[3], 0.5f, g.day);
    g.realm.tributeRoll(ks[3], ks[0], 0.5f, g.day);
    // a successor: ks[1]'s towns all go to ks[0], it falls, its remnant crowns a successor
    const realm::KingdomState* F = g.realm.kingdom(ks[1]);
    if (F) {
      g.realm.forcePeace(ks[0], ks[1], g.day);
      for (ew::Gid s : std::vector<ew::Gid>(F->settlements)) g.realm.forceOwner(s, ks[0], g.day);
      F = g.realm.kingdom(ks[1]);
      if (!F || !F->fallen) fail("a realm with no settlement left did not fall");
      const ew::Gid su = g.realm.forceSuccessor(ks[1], g.day);
      const realm::KingdomState* S = g.realm.kingdom(su);
      if (!su || !S || S->origin != realm::KO_SUCCESSOR || S->claim != ks[1] || S->settlements.empty()) fail("forceSuccessor");
      else successors++;
    }
  } else {
    out("  rise seed %llu: fewer than 4 active mainland realms (diplomacy checks skipped)\n", (unsigned long long)seed);
  }
  // ---- a ruin resettled, a world boss's raid
  {
    const ew::Gid s = sv.id;
    g.realm.forceBurn(s, g.day);
    if (home) g.realm.forceResettle(s, home, g.day);
    const realm::SettlementState* st = g.realm.settlement(s);
    if (home && (!st || !(st->flags & realm::SS_REBUILDING) || st->owner != home)) fail("forceResettle");
    g.realm.noteLair(0xB055ull, gx + 40, gy + 40, (uint8_t)art::Monster::Wolf, 30, g.day);
    const int dmg = st ? st->damage : 0;
    g.realm.forceBossRaid(0xB055ull, s, g.day);
    st = g.realm.settlement(s);
    if (!st || st->damage <= dmg || g.realm.lairs().empty() || g.realm.lairs()[0].raids != 1) fail("forceBossRaid");
  }

  // ---- (REALM lane) every deep-sim event has its own news line (never the generic "WARLORD ROSE NEAR ...")
  {
    const ew::Gid other = ks.empty() ? home : ks[0];
    for (int t = (int)realm::EvType::WarlordRose; t < (int)realm::EvType::COUNT; t++) {
      const realm::EvType et = (realm::EvType)t;
      g.realm.forceEvent(et, home ? home : other, other, sv.id, gx, gy, g.day);
      const realm::WorldEvent& e = g.realm.events().back();
      const std::string line = story::newsLine(g, e, 0), voiced = story::newsLine(g, e, g.world.sites[(size_t)g.world.startSite].culture);
      const std::string generic = std::string(realm::evTypeName(et)) + " NEAR";
      if (line.empty() || line.find(generic) != std::string::npos || voiced.find(generic) != std::string::npos || line.find('{') != std::string::npos) {
        out("  %s: %s\n", realm::evTypeName(et), line.c_str());
        fail("a deep-sim event has no news line of its own");
      }
    }
  }

  // ---- service
  std::string why;
  if (!home) out("  rise seed %llu: the start village has no kingdom (service checks skipped)\n", (unsigned long long)seed);
  else {
    if (!ServiceOps::enlist(g, home, why)) { out("  %s\n", why.c_str()); fail("enlist with the start kingdom"); }
    if (g.realm.renown().sworn != home || g.realm.renown().rank != 1) fail("the renown mirror after enlisting");
    ServiceOps::addMerit(g, service::meritToRise(service::Rank::Recruit));
    if (g.service.rank != service::Rank::Sergeant) fail("merit did not make a sergeant");
    const uint32_t o = ServiceOps::giveOrder(g, service::OrderKind::Scout, sv.id, gx + 10, gy, 0, 0);
    bool quest = false;
    for (const Quest& q : g.quests) quest |= q.type == QType::Campaign && q.stage == (int)o && q.state == QState::Active;
    if (!o || !quest) fail("giveOrder made no Campaign quest");
    const int merit0 = g.service.merit;
    ServiceOps::finishOrder(g, o, true);
    if (g.service.merit <= merit0 || g.service.ordersDone != 1) fail("finishOrder gave no merit");
    if (ks.size() >= 4) {
      const uint32_t m = ServiceOps::giveMission(g, service::MissionKind::SeekAlliance, home, ks[2] != home ? ks[2] : ks[3]);
      if (!m) fail("giveMission");
      else { ServiceOps::arrive(g, m); if (!g.service.mission(m) || g.service.mission(m)->state != service::OS_DONE) fail("arrive"); }
    }
    const int gold0 = g.gold;
    stepDays(g, 8);
    if (g.gold <= gold0) fail("no wage after 8 days");
  }

  // ---- lordship
  if (home) {
    if (!LordOps::grant(g, g.world.startSite, lord::How::Granted, 0, why)) { out("  %s\n", why.c_str()); fail("grant the start village"); }
    if (g.realm.renown().lordOf != sv.id || g.lordship.liege != home) fail("the lordship's mirror");
    stepDays(g, 31);
    if (g.lordship.treasury <= 0 && g.lordship.tributeDue <= 0) fail("no taxes in 31 days");
    bool paid = false;
    for (const realm::WorldEvent& e : g.realm.events()) paid |= e.type == realm::EvType::TributePaid || e.type == realm::EvType::TributeRefused;
    if (!paid) { out("  day %d treasury %d due %d tributeDay %d liege %llx\n", g.day, g.lordship.treasury, g.lordship.tributeDue, g.lordship.tributeDay, (unsigned long long)g.lordship.liege); fail("no tribute paid or refused after 31 days"); }
    if (!LordOps::independence(g, "THE FIXTURE MARCH", why)) { out("  %s\n", why.c_str()); fail("independence"); }
    const ew::Gid mine = g.realm.playerRealm();
    const realm::KingdomState* P = g.realm.kingdom(mine);
    if (!P || !P->player || P->origin != realm::KO_PLAYER || P->name != "THE FIXTURE MARCH") fail("the player's realm record");
    const Site& s2 = g.world.sites[(size_t)g.world.startSite];
    if (s2.kingdom < 0 || g.world.kingdoms[(size_t)s2.kingdom].id != mine) fail("the village does not fly the player's banner");
    // the tick leaves the player's realm to the player: it never changes its ruler, never declares a war for it, never
    // splits it. Its enemies may take its land, and (review round 1, VISION_PLAN 15.33) losing the LAST holding ends the
    // realm with news (KingdomFell) and ends its wars: never a "zombie" realm with no land, still at war.
    const std::string ruler = P ? P->ruler.name : "";
    const size_t ev0 = g.realm.events().size();
    stepDays(g, 120);
    P = g.realm.kingdom(mine);
    if (!P || P->ruler.name != ruler) fail("the tick changed the player's ruler");
    for (const realm::War& w : g.realm.wars()) if (w.attacker == mine) fail("the tick declared a war for the player's realm");
    for (const realm::KingdomState& k : g.realm.kingdoms()) if (k.parent == mine && k.origin == realm::KO_REBEL) fail("the player's realm split");
    int against = 0, open = 0;
    for (const realm::War& w : g.realm.wars()) {
      if (w.defender != mine) continue;
      against++;
      open += w.endDay == 0;
      out("  rise seed %llu: %s on the player's realm (by %s), days %d..%d\n", (unsigned long long)seed, w.name.c_str(),
          w.attacker == home ? "the old liege" : "another realm", (int)w.startDay, (int)w.endDay);
    }
    const realm::SettlementState* vst = g.realm.settlement(sv.id);
    const bool fell = P && P->fallen;
    if (fell) {
      bool news = false;
      for (size_t i = ev0; i < g.realm.events().size(); i++) {
        const realm::WorldEvent& e = g.realm.events()[i];
        if (e.type == realm::EvType::KingdomFell && e.a == mine) news = !story::newsLine(g, e, 0).empty();
      }
      if (!news) fail("the player's realm fell with no news");
      if (!vst || vst->owner == mine || !P->settlements.empty()) fail("the player's realm fell while it still held land");
      if (open) fail("the player's fallen realm is still at war");
      if (!g.lordship.holdings.empty() || g.lordship.realm || g.realm.renown().realmOf || g.realm.playerRealm())
        fail("the lordship outlived the realm's last holding");
    } else if (!P || P->settlements.empty() || g.lordship.holdings.empty() || !g.lordship.realm) {
      fail("a zombie realm: the player's realm stands with no land");
    }
    out("  rise seed %llu: the player's realm after 120 days: %s, %zu settlements, %d wars against it (%d open), liege tension %s\n",
        (unsigned long long)seed, fell ? "FELL with its last holding" : "stands", P ? P->settlements.size() : 0, against, open,
        [&] { const realm::Relation* r = g.realm.relation(mine, home); return r ? realm::tensionName(r->tension) : "-"; }());
  }

  // ---- save
  std::vector<uint8_t> a, b;
  g.serialize(a);
  Game h(seed);
  if (!h.deserialize(a)) fail("the save did not load");
  else {
    h.serialize(b);
    if (a != b) fail("the save does not round-trip byte-identically");
    if (h.service.rank != g.service.rank || h.lordship.realm != g.lordship.realm || h.lordship.treasury != g.lordship.treasury ||
        h.realm.renown().realmOf != g.realm.renown().realmOf || h.realm.camps().size() != g.realm.camps().size())
      fail("the loaded copy differs");
  }
  out("rise seed %llu: %d warlords, %d successors, rank %s, holdings %zu, realm %s, save %zu B: %s\n", (unsigned long long)seed, warlords,
      successors, service::rankName(g.service.rank), g.lordship.holdings.size(), g.lordship.realm ? "yes" : "no", a.size(), bad ? "FAIL" : "ok");
  return bad;
}

// ---- (REALM lane) the TICK grows the deep sim by itself: a headless realm round the start, the player walking a kingdom
//      cell now and then and noting what the game's window and the FOES lane note (the camps of the 3 x 3 regions, the
//      lairs of the 5 x 5 kingdom cells), ticked LONG days with no forcing call at all. Counts what rose by itself.
struct LongRun { int warlords = 0, warbands = 0, successors = 0, resettled = 0, rebuilt = 0, raids = 0, woke = 0, alliances = 0, joined = 0,
                 vassals = 0, tribute = 0, reunified = 0, risenFell = 0, playerActs = 0; };
LongRun longRun(uint64_t seed, int days) {
  LongRun L;
  ew::EndlessSource src(seed);
  const ew::StartPlan& sp = src.start();
  int32_t px = sp.spawn.x, py = sp.spawn.y;
  const int32_t sx = px, sy = py;
  realm::Realm R;
  R.reset(seed);
  auto note = [&](int day) {
    const int32_t rx0 = ew::floorDiv(px, ew::REGION), ry0 = ew::floorDiv(py, ew::REGION);
    for (int32_t ry = ry0 - 1; ry <= ry0 + 1; ry++)
      for (int32_t rx = rx0 - 1; rx <= rx0 + 1; rx++) {
        std::vector<std::pair<ew::Gid, std::pair<int32_t, int32_t>>> camps;
        for (const ew::SitePlan& s : src.region(rx, ry).sites) if (s.type == SiteType::BanditCamp) camps.push_back({s.id, {s.ex, s.ey}});
        for (auto& c : camps) R.noteCamp(c.first, c.second.first, c.second.second, src.kingdomAt(c.second.first, c.second.second), day);
      }
    const int32_t kx = ew::EndlessSource::kcellOf(px), ky = ew::EndlessSource::kcellOf(py);
    for (int dy = -2; dy <= 2; dy++)
      for (int dx = -2; dx <= 2; dx++) {
        bool ok = false;
        const foes::WorldBoss b = foes::worldBossOf(src, kx + dx, ky + dy, ok);
        if (ok) R.noteLair(b.id, b.lairX, b.lairY, (uint8_t)b.mon, (uint8_t)std::clamp(b.D, 1, 255), day);
      }
  };
  R.focusNow(src, px, py, 1);
  note(1);
  uint64_t walk = ew::mix64(seed ^ 0x52495345ull);
  uint32_t seen = R.eventSerial();
  std::vector<ew::Gid> risen;
  for (int d = 2; d <= days; d++) {
    if (d % 150 == 0) {
      walk = ew::mix64(walk);
      px = std::clamp(px + ((int)(walk % 3) - 1) * ew::KCELL, sx - 2 * ew::KCELL, sx + 2 * ew::KCELL);
      py = std::clamp(py + ((int)((walk >> 8) % 3) - 1) * ew::KCELL, sy - 2 * ew::KCELL, sy + 2 * ew::KCELL);
      R.focusNow(src, px, py, d);
      note(d);
    }
    R.advanceTo(src, d);
    for (const realm::WorldEvent& e : R.events()) {
      if (e.id < seen) continue;
      switch (e.type) {
        case realm::EvType::WarlordRose: L.warlords++; risen.push_back(e.a); break;
        case realm::EvType::WarbandRaid: L.warbands++; break;
        case realm::EvType::SuccessorRose: L.successors++; risen.push_back(e.a); break;
        case realm::EvType::KingdomRose: risen.push_back(e.a); break;
        case realm::EvType::KingdomFell: if (std::find(risen.begin(), risen.end(), e.a) != risen.end()) L.risenFell++; break;
        case realm::EvType::Resettled: L.resettled++; break;
        case realm::EvType::Rebuilt: L.rebuilt++; break;
        case realm::EvType::BeastRaid: L.raids++; break;
        case realm::EvType::BossWoke: L.woke++; break;
        case realm::EvType::AllianceMade: L.alliances++; break;
        case realm::EvType::AllyJoined: L.joined++; break;
        case realm::EvType::Vassalised: L.vassals++; break;
        case realm::EvType::TributePaid: L.tribute++; break;
        case realm::EvType::Reunified: L.reunified++; break;
        case realm::EvType::FiefGranted: case realm::EvType::Enlisted: case realm::EvType::Promoted: case realm::EvType::Independence:
        case realm::EvType::TermsCarried: L.playerActs++; break;
        default: break;
      }
    }
    seen = R.eventSerial();
  }
  return L;
}

int riseCmd(int argc, char** argv) {
  uint64_t A = 1, B = 3;
  for (int i = 1; i < argc; i++)
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], A, B);
  int bad = 0;
  for (uint64_t s = A; s <= B; s++) bad += riseSeed(s);
  // (REALM lane) the tick by itself over a long run: over the seeds, every kind of rise and fall must happen at least once
  LongRun T;
  const int days = 5000;
  for (uint64_t s = A; s <= B; s++) {
    const LongRun L = longRun(s, days);
    out("rise long run seed %llu (%d days, no forcing): %d warband raids, %d warlords, %d successors, %d resettled (%d rebuilt), %d boss raids "
        "(%d wakings), %d alliances, %d allies called, %d vassals, %d tributes, %d claims fulfilled, %d risen realms fell\n",
        (unsigned long long)s, days, L.warbands, L.warlords, L.successors, L.resettled, L.rebuilt, L.raids, L.woke, L.alliances, L.joined,
        L.vassals, L.tribute, L.reunified, L.risenFell);
    if (L.playerActs) { out("FAIL: rise long run seed %llu: the tick made a player's act (fief, enlisting, independence, terms)\n", (unsigned long long)s); bad++; }
    T.warlords += L.warlords; T.warbands += L.warbands; T.successors += L.successors; T.resettled += L.resettled; T.rebuilt += L.rebuilt;
    T.raids += L.raids; T.woke += L.woke; T.alliances += L.alliances; T.joined += L.joined; T.vassals += L.vassals; T.tribute += L.tribute;
    T.risenFell += L.risenFell;
  }
  const struct { const char* what; int n; } need[] = {
      {"warband raids", T.warbands}, {"warlords", T.warlords}, {"successors", T.successors}, {"resettlements", T.resettled},
      {"ruins rebuilt", T.rebuilt}, {"boss raids", T.raids}, {"lairs waking", T.woke}, {"alliances", T.alliances},
      {"allies called to war", T.joined}, {"vassals", T.vassals}, {"tributes", T.tribute}, {"risen realms that fell", T.risenFell}};
  for (const auto& n : need)
    if (n.n < 1) { out("FAIL: rise long run: the tick made no %s by itself over seeds %llu..%llu\n", n.what, (unsigned long long)A, (unsigned long long)B); bad++; }
  out("rise: %d failures\n", bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--rise", "M12 Rise and Fall: warlords, successors, alliances, vassals, service, lordship, the save [--seeds A..B]", riseCmd);
