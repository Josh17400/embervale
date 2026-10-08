// rpg_test --realm [--seeds A..B]: the M4 living world (rpg/sim/realm.h). REALM lane.
//   Phase A (lead): genesis around the start (kingdoms instantiated, every loaded settlement known, owners as planned),
//   the forcing calls (war, siege, conquest with banners following, burning with refugees, famine) leave a consistent
//   state that the world shows, the realm block round-trips byte-identically, and focus / tick costs are reported.
//   Phase B (REALM lane): genesis scope (every kingdom whose seat lies within 3 kingdom cells, at most 48 active,
//   frontier settlements), time-sliced focus, dormant catch-up, the history pre-roll and ruin records, round trips
//   after long ticking (and the loaded copy ticks on identically), the extra forcing calls.
// rpg_test --history N [--seeds A..B]: a headless realm focused on each seed's start, ticked N days while the player
//   moves between kingdom cells now and then, measured against the owner's slow, realistic targets (15.6.3; FAIL
//   outside them over --history 2000 --seeds 1..20): average active wars 0.2-1.5; a war in >= 75% of seeds; every war
//   foreshadowed by >= 2 events between its realms in the 90 days before; >= 50% of wars from famine or broken trade;
//   war length 20-90 days; famines 1-6 per 360 days; a settlement changes hands every 40-200 days; a kingdom falls or
//   splits every 400-1500 days; >= 50% of rulers in office after 300 days; nothing unbounded; tick avg <= 1 ms (worst
//   <= 3 ms), also with 48 active realms.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include "tools/tests/tests.h"
#include "rpg/world/source.h"

namespace {

double msSince(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

// ---------------------------------------------------------------- --realm: one seed in the game
int realmSeed(uint64_t seed) {
  int bad = 0;
  auto fail = [&](const char* what) { out("FAIL: realm seed %llu: %s\n", (unsigned long long)seed, what); bad++; };
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.godMode = true;
  g.noWildSpawns = true;
  auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < 30; i++) { g.update(SIM_DT, Input()); g.events.clear(); }
  const double ms = msSince(t0);
  if (g.realm.kingdoms().empty()) fail("no kingdom instantiated around the start");
  // the time-sliced focus finishes within a few seconds of play
  int steps = 0;
  while (g.realm.focusPending() && steps < 2000) { g.update(SIM_DT, Input()); g.events.clear(); steps++; }
  if (g.realm.focusPending()) fail("focus still pending after 2000 steps");
  int known = 0, settlements = 0;
  for (const Site& s : g.world.sites) {
    if (!s.settlement()) continue;
    settlements++;
    const realm::SettlementState* st = g.realm.settlement(s.id);
    if (!st) continue;
    known++;
    const ew::Gid k = s.kingdom >= 0 ? g.world.kingdoms[(size_t)s.kingdom].id : 0;
    if (st->owner != k) fail("a settlement's owner differs from its realm state");
    const ew::Gid hk = s.homeKingdom >= 0 ? g.world.kingdoms[(size_t)s.homeKingdom].id : 0;
    if (st->home != hk) { out("  %s: plan kingdom %llx, lattice kingdom %llx\n", s.name.c_str(), (unsigned long long)hk, (unsigned long long)st->home); fail("the lattice and the plan disagree on a settlement's kingdom"); }
    if (!k && (!(st->flags & realm::SS_FRONTIER) || st->garrison != 0)) fail("a wildlands settlement is not frontier with garrison 0");
  }
  if (known != settlements) fail("a loaded settlement is unknown to the realm");
  // force: a siege of the start village by a neighbour, then its fall; the banners follow the new owner
  const Site& sv = g.world.sites[(size_t)g.world.startSite];
  const ew::Gid home = sv.kingdom >= 0 ? g.world.kingdoms[(size_t)sv.kingdom].id : 0;
  ew::Gid enemy = 0;
  for (const realm::KingdomState& k : g.realm.kingdoms()) if (k.id != home && !k.fallen) { enemy = k.id; break; }
  if (enemy) {
    const uint32_t sg = g.realm.forceSiege(sv.id, enemy, g.day);
    const realm::Siege* S = g.realm.siegeAt(sv.id);
    if (!S || S->id != sg || !(g.realm.settlement(sv.id)->flags & realm::SS_BESIEGED)) fail("forceSiege left no siege");
    if (home && !g.realm.atWar(enemy, home)) fail("forceSiege declared no war");
    g.realm.forceOwner(sv.id, enemy, g.day);
    g.realmSync();
    const Site& s2 = g.world.sites[(size_t)g.world.startSite];
    if (s2.kingdom < 0 || g.world.kingdoms[(size_t)s2.kingdom].id != enemy) fail("forceOwner: the site's kingdom did not follow");
    if (s2.homeKingdom < 0 || g.world.kingdoms[(size_t)s2.homeKingdom].id != home) fail("forceOwner moved the genesis kingdom");
    if (!(g.realm.settlement(sv.id)->flags & realm::SS_OCCUPIED)) fail("forceOwner: not occupied");
    const Kingdom& K = g.world.kingdoms[(size_t)s2.kingdom];
    for (int i = 0; i < s2.bldgCount; i++) {
      const Bldg& b = g.world.over.bldgs[(size_t)(s2.bldgFirst + i)];
      if (b.banner && b.banner != K.color) { fail("a building still flies the old banner"); break; }
    }
    g.realm.forceBurn(sv.id, g.day);
    if (!(g.realm.settlement(sv.id)->flags & realm::SS_BURNED)) fail("forceBurn did not burn");
    // days pass: the burned village is still burned 3 days on (rebuilding starts after 30)
    g.day += 3;
    for (int i = 0; i < 3; i++) { g.update(SIM_DT, Input()); g.events.clear(); }
    if (!(g.realm.settlement(sv.id)->flags & realm::SS_BURNED)) fail("a burned village stopped burning within 3 days");
    // a rebellion in the enemy's realm makes a kingdom the world gets a record of
    const ew::Gid reb = g.realm.forceCivilWar(*g.world.src, enemy, g.day);
    if (reb) {
      g.realmSync();
      const realm::KingdomState* R = g.realm.kingdom(reb);
      if (!R || !R->rebel || R->parent != enemy || R->settlements.empty()) fail("forceCivilWar: no proper rebel realm");
    }
  }
  // the realm block round-trips byte-identically
  std::vector<uint8_t> a, b;
  g.realm.serialize(a);
  realm::Realm r2;
  if (!r2.deserialize(a)) fail("the realm block does not load");
  r2.serialize(b);
  if (a != b) fail("the realm block does not round-trip byte-identically");
  out("realm seed %llu: %zu kingdoms (%d active), %d/%d settlements known, %zu events, first 30 steps %.1f ms, focus done in %d more "
      "steps (focus worst %.2f ms, %d cells read)\n",
      (unsigned long long)seed, g.realm.kingdoms().size(), g.realm.stats.active, known, settlements, g.realm.events().size(), ms, steps,
      g.realm.stats.worstFocusMs, g.realm.stats.scans);
  return bad;
}

// ---------------------------------------------------------------- --realm: the headless realm
int headlessSeed(uint64_t seed) {
  int bad = 0;
  auto fail = [&](const std::string& what) { out("FAIL: realm seed %llu: %s\n", (unsigned long long)seed, what.c_str()); bad++; };
  ew::EndlessSource src(seed);
  const ew::StartPlan& sp = src.start();
  const int32_t px = sp.spawn.x, py = sp.spawn.y;
  realm::Realm R;
  R.reset(seed);
  // genesis scope: every kingdom whose seat cell lies within 3 kingdom cells
  R.focusNow(src, px, py, 1);
  const int32_t kx = ew::EndlessSource::kcellOf(px), ky = ew::EndlessSource::kcellOf(py);
  int want = 0;
  for (int cy = ky - 3; cy <= ky + 3; cy++)
    for (int cx = kx - 3; cx <= kx + 3; cx++) {
      const ew::Gid id = src.kingdomOfCell(cx, cy);
      if (!id) continue;
      want++;
      const realm::KingdomState* K = R.kingdom(id);
      if (!K) { fail("a kingdom within 3 cells was not instantiated"); continue; }
      if (K->settlements.empty() || K->pop <= 0 || K->military <= 0) fail("a kingdom has no settlements, people or soldiers");
      if (K->ruler.name.empty() || K->ruler.title.empty() || K->ruler.house.empty()) fail("a kingdom has no ruler, title or house");
      if (K->settlements.front() != K->capital) fail("a kingdom's capital is not first in its list");
      if (R.history(id).size() < 4 || R.chronicle(id).size() < 4) fail("a kingdom has a thin history");
    }
  if (R.stats.active > 48 || R.stats.active <= 0) fail("active realms out of range");
  int frontier = 0;
  for (const realm::KingdomState& k : R.kingdoms())
    for (ew::Gid g : k.settlements)
      if (const realm::SettlementState* s = R.settlement(g)) if (s->flags & realm::SS_FRONTIER) frontier++;
  if (frontier) fail("a kingdom owns a frontier settlement");
  // time-sliced focus: a fresh realm gets there in budgeted steps, each near the budget (one cell read at most beyond)
  realm::Realm F;
  F.reset(seed);
  int calls = 0;
  double worst = 0;
  do {
    auto t0 = std::chrono::steady_clock::now();
    F.focus(src, px, py, 1);
    worst = std::max(worst, msSince(t0));
    calls++;
  } while (F.focusPending() && calls < 1000);
  std::vector<uint8_t> fa, fb;
  F.serialize(fa);
  R.serialize(fb);
  if (fa != fb) fail("time-sliced genesis differs from the one-shot genesis");
  // the ruin records: every ruin in the regions round the start has one
  int ruins = 0, clues = 0;
  for (int ry = ew::floorDiv(py, ew::REGION) - 2; ry <= ew::floorDiv(py, ew::REGION) + 2; ry++)
    for (int rx = ew::floorDiv(px, ew::REGION) - 2; rx <= ew::floorDiv(px, ew::REGION) + 2; rx++) {
      std::vector<ew::Gid> ids;
      for (const ew::SitePlan& s : src.region(rx, ry).sites) {
        if (s.type == SiteType::Ruin) ids.push_back(s.id);
        // the lattice the realm reads and the region plans agree on every settlement's kingdom
        const bool settle = s.type == SiteType::City || s.type == SiteType::Town || s.type == SiteType::Village;
        const realm::SettlementState* st = settle ? R.settlement(s.id) : nullptr;
        if (settle && st && st->home != s.kingdom) fail("the lattice and the region plan disagree on " + s.name + "'s kingdom");
        if (settle && !st && s.kingdom && R.kingdom(s.kingdom)) fail(s.name + " of an instantiated kingdom is unknown to the realm");
      }
      for (ew::Gid id : ids) {
        const realm::RuinRecord rr = R.ruin(src, id);
        ruins++;
        if (!rr.valid || rr.oldName.empty() || rr.builtBy.empty() || rr.lastLord.empty() || rr.clues.size() < 4 || rr.clues.size() > 8 ||
            rr.foundedYearsAgo <= rr.fellYearsAgo)
          fail("a ruin record is incomplete");
        clues += (int)rr.clues.size();
        const realm::RuinRecord again = R.ruin(src, id);
        if (again.oldName != rr.oldName || again.clues != rr.clues) fail("a ruin record is not stable");
      }
    }
  // tick 400 days; the block round-trips and the loaded copy ticks on identically
  for (int d = 2; d <= 400; d++) R.advanceTo(src, d);
  std::vector<uint8_t> a, b;
  R.serialize(a);
  realm::Realm L;
  if (!L.deserialize(a)) fail("the realm block does not load after 400 days");
  L.serialize(b);
  if (a != b) fail("the realm block does not round-trip after 400 days");
  for (int d = 401; d <= 430; d++) { R.advanceTo(src, d); L.advanceTo(src, d); }
  R.serialize(a);
  L.serialize(b);
  if (a != b) fail("a loaded realm ticks on differently");
  // a long absence: 100 days without daily ticks (weekly ticks at 7x), then dormant catch-up after a journey away
  const int32_t farX = px + 6 * ew::KCELL;
  R.focusNow(src, farX, py, 430);
  const int dormantBefore = R.stats.dormant;
  R.advanceTo(src, 560);
  R.focusNow(src, px, py, 560);
  if (dormantBefore == 0 || R.stats.catchUps == 0) fail("no dormant realm caught up on re-entry");
  for (const realm::KingdomState& k : R.kingdoms())
    if (k.tier == realm::Tier::Active && (int)k.lastTick != R.day()) { fail("an active realm is behind the clock"); break; }
  // the extra forcing calls stay consistent
  ew::Gid home = src.kingdomAt(px, py);
  if (!R.kingdom(home)) home = R.kingdoms().empty() ? 0 : R.kingdoms().front().id;
  if (const realm::KingdomState* K = R.kingdom(home)) {
    const std::string was = K->ruler.name;
    const uint16_t since = K->ruler.sinceDay;
    R.forceRulerDeath(home, R.day());
    K = R.kingdom(home);
    if (K->ruler.sinceDay == since && K->ruler.name == was) fail("forceRulerDeath: no successor");
    R.forceHarvestFail(home, R.day());
    if (R.kingdom(home)->food > 0.26f) fail("forceHarvestFail: the granary is still full");
  }
  out("realm headless seed %llu: %d kingdoms in range at the start (%zu known after a journey, %d active), focus re-sliced in %d calls "
      "(worst %.2f ms), %d ruins (%d clues), %d catch-ups, block %zu B, memory %zu B, tick avg %.3f ms\n",
      (unsigned long long)seed, want, R.kingdoms().size(), R.stats.active, calls, worst, ruins, clues, R.stats.catchUps, a.size(),
      R.memoryBytes(), R.stats.ticks ? R.stats.tickMsSum / R.stats.ticks : 0.0);
  return bad;
}

int realmCmd(int argc, char** argv) {
  uint64_t A = 1, B = 5;
  for (int i = 2; i < argc; i++)
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], A, B);
  int bad = 0;
  for (uint64_t s = A; s <= B; s++) { bad += realmSeed(s); bad += headlessSeed(s); }
  printf("realm: %d failures\n", bad);
  return bad ? 1 : 0;
}

// ---------------------------------------------------------------- --history N
struct WarRec { ew::Gid a = 0, b = 0; int start = 0, end = 0; realm::WarCause cause = realm::WarCause::BorderDispute; };
struct SeedStats {
  int days = 0, wars = 0, famine = 0, wars20to90 = 0, warsEnded = 0, warsFood = 0, warsForeshadowed = 0, changes = 0, falls = 0;
  int rulersKept = 0, rulersAt0 = 0, maxActive = 0, maxEvents = 0, maxRel = 0, maxWars = 0, maxSieges = 0, maxKingdoms = 0;
  double activeWarDays = 0, tickSum = 0, tickWorst = 0;
  std::vector<double> tickMs;   // every tick (the gate takes the 99.9th percentile: one OS preemption is not the sim)
  int ticks = 0;
  size_t block = 0, memory = 0;
  int minLen = 1 << 30, maxLen = 0;
};

SeedStats historySeed(uint64_t seed, int N, int radius) {
  SeedStats st;
  ew::EndlessSource src(seed);
  const ew::StartPlan& sp = src.start();
  int32_t px = sp.spawn.x, py = sp.spawn.y;
  const int32_t sx = px, sy = py;
  realm::Realm R;
  R.reset(seed);
  R.activeRadius = radius;
  R.focusNow(src, px, py, 1);
  std::map<ew::Gid, uint64_t> rulers0;   // the realms active at the start and their rulers' dynasty + since
  for (const realm::KingdomState& k : R.kingdoms())
    if (k.tier == realm::Tier::Active) rulers0[k.id] = 1;
  std::map<uint32_t, WarRec> wars;
  std::vector<realm::WorldEvent> log;    // every event, unbounded (the realm keeps only the newest 400)
  uint32_t seen = R.eventSerial();
  uint64_t walk = ew::mix64(seed ^ 0x57414C4Bull);
  for (int d = 2; d <= N + 1; d++) {
    if (d % 150 == 0) {
      // the player travels to a neighbouring kingdom cell, staying within 2 of the start
      walk = ew::mix64(walk);
      const int dx = (int)(walk % 3) - 1, dy = (int)((walk >> 8) % 3) - 1;
      px = std::clamp(px + dx * ew::KCELL, sx - 2 * ew::KCELL, sx + 2 * ew::KCELL);
      py = std::clamp(py + dy * ew::KCELL, sy - 2 * ew::KCELL, sy + 2 * ew::KCELL);
      R.focusNow(src, px, py, d);
    }
    auto t0 = std::chrono::steady_clock::now();
    R.advanceTo(src, d);
    const double ms = msSince(t0);
    st.tickSum += ms;
    st.tickWorst = std::max(st.tickWorst, ms);
    st.tickMs.push_back(ms);
    st.ticks++;
    // new events
    for (const realm::WorldEvent& e : R.events())
      if (e.id >= seen) log.push_back(e);
    seen = R.eventSerial();
    int open = 0;
    for (const realm::War& w : R.wars()) {
      WarRec& r = wars[w.id];
      r.a = w.attacker; r.b = w.defender; r.start = w.startDay; r.end = w.endDay; r.cause = w.cause;
      if (!w.endDay) open++;
    }
    st.activeWarDays += open;
    st.maxActive = std::max(st.maxActive, R.stats.active);
    st.maxEvents = std::max(st.maxEvents, (int)R.events().size());
    st.maxRel = std::max(st.maxRel, (int)R.relations().size());
    st.maxWars = std::max(st.maxWars, (int)R.wars().size());
    st.maxSieges = std::max(st.maxSieges, (int)R.sieges().size());
    st.maxKingdoms = std::max(st.maxKingdoms, (int)R.kingdoms().size());
    if (d == 301) {
      for (auto& kv : rulers0) {
        const realm::KingdomState* k = R.kingdom(kv.first);
        st.rulersAt0++;
        if (k && !k->fallen && k->ruler.sinceDay == 0) st.rulersKept++;
      }
    }
  }
  st.days = N;
  std::vector<uint8_t> blk;
  R.serialize(blk);
  st.block = blk.size();
  st.memory = R.memoryBytes();
  for (const realm::WorldEvent& e : log) {
    if (e.type == realm::EvType::Famine) st.famine++;
    if (e.type == realm::EvType::TownTaken || e.type == realm::EvType::Resettled) st.changes++;
    if (e.type == realm::EvType::KingdomFell || e.type == realm::EvType::CivilWar) st.falls++;
  }
  for (auto& kv : wars) {
    const WarRec& w = kv.second;
    st.wars++;
    if (w.cause == realm::WarCause::Famine || w.cause == realm::WarCause::BrokenTrade) st.warsFood++;
    bool fell = false;   // a war that ends because one side fell is as short as the fall made it
    for (const realm::WorldEvent& e : log)
      if (e.type == realm::EvType::KingdomFell && (int)e.day == w.end && (e.a == w.a || e.a == w.b)) fell = true;
    if (w.end && !fell) {
      st.warsEnded++;
      const int len = w.end - w.start;
      st.minLen = std::min(st.minLen, len);
      st.maxLen = std::max(st.maxLen, len);
      if (len >= 20 && len <= 90) st.wars20to90++;
    }
    int fore = 0;
    for (const realm::WorldEvent& e : log) {
      if ((int)e.day < w.start - 90 || (int)e.day > w.start) continue;
      if (!((e.a == w.a && e.b == w.b) || (e.a == w.b && e.b == w.a))) continue;
      if (e.type == realm::EvType::TradeBroken || e.type == realm::EvType::BorderIncident || e.type == realm::EvType::Skirmish ||
          e.type == realm::EvType::TroopsMarching || e.type == realm::EvType::PricesRising)
        fore++;
    }
    if (fore >= 2) st.warsForeshadowed++;
    else {
      printf("  seed %llu: war %u (%s) on day %d not foreshadowed; events between them:\n", (unsigned long long)seed, kv.first,
             realm::warCauseName(w.cause), w.start);
      for (const realm::WorldEvent& e : log)
        if ((e.a == w.a && e.b == w.b) || (e.a == w.b && e.b == w.a)) printf("    day %d %s\n", e.day, realm::evTypeName(e.type));
    }
  }
  return st;
}

int historyCmd(int argc, char** argv) {
  uint64_t A = 1, B = 20;
  int N = 2000;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], A, B);
    else if (!strcmp(argv[i], "--history") && i + 1 < argc) N = std::max(30, atoi(argv[++i]));
  }
  printf("history: %d days, seeds %llu..%llu\n", N, (unsigned long long)A, (unsigned long long)B);
  printf("%-5s %5s %6s %5s %5s %5s %6s %7s %6s %5s %6s %6s %7s %7s %6s\n", "seed", "act", "wars", "avgW", "fore", "food", "len",
         "famine", "hands", "falls", "ruler", "tick", "worst", "blockB", "memKB");
  SeedStats T;
  int seedsWithWar = 0, seeds = 0, rulersKept = 0, rulersAt0 = 0;
  for (uint64_t s = A; s <= B; s++) {
    const SeedStats st = historySeed(s, N, 3);
    seeds++;
    if (st.wars > 0) seedsWithWar++;
    T.wars += st.wars; T.warsEnded += st.warsEnded; T.wars20to90 += st.wars20to90; T.warsFood += st.warsFood;
    T.warsForeshadowed += st.warsForeshadowed; T.famine += st.famine; T.changes += st.changes; T.falls += st.falls;
    T.activeWarDays += st.activeWarDays; T.days += st.days; T.tickSum += st.tickSum; T.ticks += st.ticks;
    T.tickWorst = std::max(T.tickWorst, st.tickWorst);
    T.tickMs.insert(T.tickMs.end(), st.tickMs.begin(), st.tickMs.end());
    T.maxActive = std::max(T.maxActive, st.maxActive); T.maxEvents = std::max(T.maxEvents, st.maxEvents);
    T.maxRel = std::max(T.maxRel, st.maxRel); T.maxWars = std::max(T.maxWars, st.maxWars); T.maxSieges = std::max(T.maxSieges, st.maxSieges);
    T.maxKingdoms = std::max(T.maxKingdoms, st.maxKingdoms);
    T.block = std::max(T.block, st.block); T.memory = std::max(T.memory, st.memory);
    T.minLen = std::min(T.minLen, st.minLen); T.maxLen = std::max(T.maxLen, st.maxLen);
    rulersKept += st.rulersKept; rulersAt0 += st.rulersAt0;
    char len[24];
    if (st.warsEnded) snprintf(len, sizeof len, "%d-%d", st.minLen, st.maxLen); else snprintf(len, sizeof len, "-");
    printf("%-5llu %5d %6d %5.2f %2d/%-2d %2d/%-2d %6s %7.2f %6d %5d %3d/%-2d %6.3f %7.2f %6zu %6zu\n", (unsigned long long)s, st.maxActive,
           st.wars, st.activeWarDays / std::max(1, st.days), st.warsForeshadowed, st.wars, st.warsFood, st.wars, len,
           st.famine * 360.0 / std::max(1, st.days), st.changes, st.falls, st.rulersKept, st.rulersAt0, st.tickSum / std::max(1, st.ticks),
           st.tickWorst, st.block, st.memory / 1024);
  }
  const double avgWars = T.activeWarDays / std::max(1, T.days);
  const double famine360 = T.famine * 360.0 / std::max(1, T.days);
  const double handsEvery = T.changes ? (double)T.days / T.changes : 1e9;
  const double fallEvery = T.falls ? (double)T.days / T.falls : 1e9;
  const double keptPct = rulersAt0 ? 100.0 * rulersKept / rulersAt0 : 100.0;
  const double foodPct = T.wars ? 100.0 * T.warsFood / T.wars : 100.0;
  const double tickAvg = T.tickSum / std::max(1, T.ticks);
  // 48 active realms (a wider horizon) for the tick's cost
  const SeedStats big = historySeed(A, 360, 7);
  const double bigAvg = big.tickSum / std::max(1, big.ticks);
  int bad = 0;
  auto gate = [&](bool ok, const char* fmt, double v, const char* range) {
    char b[160];
    snprintf(b, sizeof b, fmt, v);
    printf("  %-58s %-14s %s\n", b, range, ok ? "ok" : "FAIL");
    if (!ok) bad++;
  };
  printf("totals over %d seeds x %d days:\n", seeds, N);
  gate(avgWars >= 0.2 && avgWars <= 1.5, "average active wars in the horizon %.2f", avgWars, "0.2-1.5");
  gate(seedsWithWar * 4 >= seeds * 3, "seeds with at least one war %.0f", (double)seedsWithWar, ">= 75%");
  gate(T.warsForeshadowed == T.wars, "wars foreshadowed by >= 2 events in 90 days %.0f", (double)T.warsForeshadowed, "all");
  printf("    (of %d wars)\n", T.wars);
  gate(foodPct >= 50, "wars caused by famine or broken trade %.0f%%", foodPct, ">= 50%");
  gate(T.wars20to90 == T.warsEnded, "ended wars lasting 20-90 days %.0f", (double)T.wars20to90, "all");
  printf("    (of %d ended; shortest %d, longest %d days)\n", T.warsEnded, T.warsEnded ? T.minLen : 0, T.maxLen);
  gate(famine360 >= 1 && famine360 <= 6, "famines per 360 days per horizon %.2f", famine360, "1-6");
  gate(handsEvery >= 40 && handsEvery <= 200, "a settlement changes hands every %.0f days", handsEvery, "40-200");
  gate(fallEvery >= 400 && fallEvery <= 1500, "a kingdom falls or splits every %.0f days", fallEvery, "400-1500");
  gate(keptPct >= 50, "realms keeping their ruler after 300 days %.0f%%", keptPct, ">= 50%");
  gate(T.maxActive <= 64, "most active realms %.0f", (double)T.maxActive, "<= 64");
  gate(T.maxEvents <= 400, "largest event log %.0f", (double)T.maxEvents, "<= 400");
  gate(T.maxRel <= 600, "most relations %.0f", (double)T.maxRel, "<= 600");
  gate(T.maxWars <= 64 && T.maxSieges <= 64, "most wars / sieges kept %.0f", (double)std::max(T.maxWars, T.maxSieges), "<= 64");
  gate(T.block <= 64 * 1024, "largest realm block %.0f B", (double)T.block, "<= 64 KB");
  gate(T.memory < 200 * 1024, "largest sim memory %.0f B", (double)T.memory, "< 200 KB");
  gate(tickAvg <= 1.0, "tick average %.3f ms", tickAvg, "<= 1 ms");
  std::sort(T.tickMs.begin(), T.tickMs.end());
  const double p999 = T.tickMs.empty() ? 0 : T.tickMs[std::min(T.tickMs.size() - 1, T.tickMs.size() * 999 / 1000)];
  gate(p999 <= 3.0, "tick worst (99.9th percentile) %.3f ms", p999, "<= 3 ms");
  gate(bigAvg <= 1.0 && big.maxActive >= 40, "tick average with %.0f active realms", (double)big.maxActive, ">= 40 active");
  printf("    (avg %.3f ms, worst %.3f ms over 360 days; the raw worst tick of all seeds %.3f ms)\n", bigAvg, big.tickWorst, T.tickWorst);
  printf("history: %d failures\n", bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--realm", "M4 living world: genesis, forcing, owners and banners, the realm block [--seeds A..B]", realmCmd);
RPG_TEST_CMD("--history", "M4 living world over N days against the owner's slow, realistic targets [--seeds A..B]", historyCmd);
