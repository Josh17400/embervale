// rpg_test --life [--seeds A..B] [--report]: M5 "Hearth and Hall" needs-driven citizens (rpg/sim/life.h). CITIZENS lane.
// Per seed (FAIL lines fail the run; --report prints the numbers and fails nothing):
//   - templates: every job and shift covers all 24 hours;
//   - the census: deterministic (built twice, identical people, homes, spawn map), homes hold 1-6, everyone housed but
//     beggars, the bard, the lamplighter and refugees, the inn's keeper is its interior slot 0; the spawn map is one to
//     one (no two generator spawns, overworld or interior, are the same resident) and covers the overworld folk;
//   - plans by the hour: most asleep at 2:00, at work or play at 10:00;
//   - 15.12 thresholds over 5 simulated days (headless, the clock run an hour per step): residents eat 2+ times on 90 %
//     of their days and sleep 6+ hours on 85 % of them; each need's average stays within 30-85;
//   - towns: 25 % or more of the adults at the gathering place at 20:00;
//   - a forced famine: MF_HUNGRY within a day, food prices >= 150 %, mood down 15 or more, the realm's food lower;
//   - a content place holds its festival within 20 days; a death leaves kin and friends grieving;
//   - raids: every settlement with monster pressure has a 2-8 % chance a night; the rolls over 60 nights agree with it;
//   - the life block: byte-identical round trips (also after the censuses are rebuilt), damaged blocks refused or
//     loaded without a crash, within 64 KB;
//   - performance (120 residents <= 1.5 ms on iPhone web): Life::tick's average <= 0.1 ms and worst step <= 1.5 ms with
//     a capital loaded; a census slice <= 1.5 ms; 120 residents' plans per hour well under budget.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include "tools/tests/tests.h"

namespace {

double msSince(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

bool atGathering(const life::Census& c, const life::Resident& r) {
  if (r.flags & (life::RF_DEAD | life::RF_AWAY)) return false;
  using A = life::Act;
  const bool social = r.act == A::Tavern || r.act == A::Perform || r.act == A::Eat || r.act == A::Socialise || r.act == A::Bathe ||
                      r.act == A::Brawl || r.act == A::Work;
  if (!social) return false;
  if (r.place == life::Place::Gathering || r.place == life::Place::Gathering2) return true;
  if (c.gathering < 0 && r.place == life::Place::Plaza && r.act != life::Act::Work) return true;   // a grove or the square itself
  return r.at >= 0 && (r.at == c.gathering || r.at == c.gathering2);
}

int lifeSeed(uint64_t seed, bool report) {
  int bad = 0;
  auto fail = [&](const char* fmt, auto... args) {
    char buf[512];
    snprintf(buf, sizeof buf, fmt, args...);
    if (report) out("  (report) life seed %llu: %s\n", (unsigned long long)seed, buf);
    else { out("FAIL: life seed %llu: %s\n", (unsigned long long)seed, buf); bad++; }
  };
  auto info = [&](const char* fmt, auto... args) {
    char buf[512];
    snprintf(buf, sizeof buf, fmt, args...);
    if (report) out("  life seed %llu: %s\n", (unsigned long long)seed, buf);
  };
  // ---- templates
  for (int j = 0; j < (int)life::Job::COUNT; j++)
    for (int sh = 0; sh < 2; sh++) {
      int n = 0;
      const life::Block* t = life::jobTemplate((life::Job)j, sh, n);
      int covered = 0;
      for (int h = 0; h < 24; h++)
        for (int k = 0; k < n; k++) if (h >= t[k].from && h < t[k].to) { covered++; break; }
      if (covered != 24) fail("job %s shift %d: the template covers %d of 24 hours", life::jobName((life::Job)j), sh, covered);
    }

  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.godMode = true;
  g.noWildSpawns = true;
  auto step = [&](int n) { for (int i = 0; i < n; i++) { g.update(SIM_DT, Input()); g.events.clear(); g.mode = Mode::Play; } };
  step(10);
  const int sv = g.world.startSite;
  life::Census* c = g.life.census(g.world, sv);
  if (!c) { fail("no census for the start village"); return bad; }
  const ew::Gid svId = g.world.sites[(size_t)sv].id;

  // ---- the census: determinism, bounds, keys
  {
    life::Census twice;
    life::buildCensus(g.world, sv, twice);
    bool same = twice.res.size() == c->res.size() - c->refugees && twice.spawnKeys == c->spawnKeys && twice.spawnRes == c->spawnRes &&
                twice.inner == c->inner && twice.baseTies == c->baseTies;
    for (size_t i = 0; same && i < twice.res.size(); i++)
      same = twice.res[i].name == c->res[i].name && twice.res[i].job == c->res[i].job && twice.res[i].home == c->res[i].home &&
             twice.res[i].age == c->res[i].age && twice.res[i].traits == c->res[i].traits;
    if (!same) fail("the census is not deterministic");
  }
  if (c->res.size() < 8) fail("the start village has only %zu residents", c->res.size());
  {
    const Site& s = g.world.sites[(size_t)sv];
    std::vector<int> perHome((size_t)std::max(1, s.bldgCount), 0);
    int homeless = 0, keyInn = 0, kids = 0, elders = 0, couples = 0;
    for (const life::Resident& r : c->res) {
      const bool roams = r.job == life::Job::Beggar || r.job == life::Job::Bard || r.job == life::Job::Lamplighter || (r.flags & life::RF_REFUGEE);
      if (r.home < 0 && !roams) homeless++;
      if (r.home >= 0 && r.home < (int)perHome.size() && r.home != r.work && r.job != life::Job::Bard) perHome[(size_t)r.home]++;
      if (r.job == life::Job::Innkeeper && r.keySlot == 0 && r.keyBldg == c->inn) keyInn++;
      kids += r.job == life::Job::Child;
      elders += r.job == life::Job::Elder;
      couples += r.spouse >= 0;
    }
    if (homeless) fail("%d residents without a home", homeless);
    for (size_t off = 0; off < perHome.size(); off++)
      if (perHome[off] > 6) fail("building %zu holds %d people (a home holds 1-6)", off, perHome[off]);
    if (c->inn >= 0 && keyInn != 1) fail("the inn's keeper is not its interior slot 0 (%d)", keyInn);
    info("%s: %zu residents (%d children, %d elders, %d married), %zu ties, gathering %d", s.name.c_str(), c->res.size(), kids, elders,
         couples / 2, c->ties.size(), (int)c->gathering);
  }

  // ---- the spawn map: one to one over the overworld spawns and every interior's slots
  {
    const Site& s = g.world.sites[(size_t)sv];
    std::set<int> seen;
    int dup = 0, mapped = 0, folk = 0;
    auto it = g.world.siteSpawns.find(sv);
    if (it != g.world.siteSpawns.end())
      for (int idx : it->second) {
        const Spawn& sp = g.world.over.spawns[(size_t)idx];
        if (!sp.npc || sp.bandit || sp.role == Role::Guard) continue;
        folk++;
        const int r = life::spawnResident(*c, sp, -1);
        if (r < 0) continue;
        mapped++;
        if (!seen.insert(r).second) dup++;
      }
    for (int off = 0; off < s.bldgCount; off++)
      for (int slot : {0, 1, 2, 3, 4, 17, 18, 19, 20, 33, 34, 35}) {
        Spawn sp;
        sp.npc = true; sp.slot = slot; sp.role = slot == 0 ? Role::Innkeeper : Role::Villager;
        const int r = life::spawnResident(*c, sp, off);
        if (r < 0) continue;
        if (!seen.insert(r).second) dup++;
      }
    if (dup) fail("the spawn map gives %d spawns a resident another spawn already is", dup);
    const int adults = (int)std::count_if(c->res.begin(), c->res.end(), [](const life::Resident& r) { return r.job != life::Job::Child; });
    if (folk > 0 && mapped * 10 < std::min(folk, adults / 2) * 8) fail("only %d of %d overworld folk spawns are residents", mapped, folk);
    info("spawn map: %d of %d overworld folk spawns are residents; %zu residents spoken for", mapped, folk, seen.size());
  }

  // ---- a day, run fast: the plans at 2:00, 10:00
  auto at = [&](float hour) { g.hour = hour; step(3); };
  auto count = [&](const life::Census& cs, life::Act a) { int n = 0; for (const life::Resident& r : cs.res) n += r.act == a && !(r.flags & (life::RF_DEAD | life::RF_AWAY)); return n; };
  at(2.0f);
  c = g.life.findMut(svId);
  const int n0 = (int)c->res.size();
  const int asleep = count(*c, life::Act::Sleep);
  at(10.0f);
  c = g.life.findMut(svId);
  const int working = count(*c, life::Act::Work) + count(*c, life::Act::Patrol) + count(*c, life::Act::Play) + count(*c, life::Act::Eat);
  if (asleep * 10 < n0 * 7) fail("at 2:00 only %d of %d asleep", asleep, n0);
  if (working * 2 < n0) fail("at 10:00 only %d of %d at work or play", working, n0);
  if (c->occHour < 0) fail("the hourly aggregate never ran for the start village");

  // ---- five days, an hour a step: meals, sleep, needs (15.12)
  const life::Life::Stats s0 = g.life.stats;
  g.hour = 0.5f;
  g.day++;
  step(2);
  for (int h = 0; h < 5 * 24; h++) { g.life.advanceHours(g, 1); }
  {
    const life::Life::Stats& s1 = g.life.stats;
    const int days = s1.dayResidents - s0.dayResidents;
    const int mealsOk = s1.dayMealsOk - s0.dayMealsOk, sleepOk = s1.daySleepOk - s0.daySleepOk;
    const int64_t samples = s1.needSamples - s0.needSamples;
    if (days < 20) fail("only %d resident-days were lived in five days", days);
    else {
      const int mp = mealsOk * 100 / days, sp = sleepOk * 100 / days;
      if (mp < 90) fail("residents ate twice or more on %d%% of their days (want 90%%)", mp);
      if (sp < 85) fail("residents slept 6 h or more on %d%% of their nights (want 85%%)", sp);
      info("5 days: %d resident-days, ate 2+ on %d%%, slept 6+ h on %d%%", days, mp, sp);
    }
    if (samples > 0) {
      std::string line;
      for (int k = 0; k < life::NEEDS; k++) {
        const int avg = (int)((s1.needSum[k] - s0.needSum[k]) / samples);
        if (avg < 30 || avg > 85) fail("the average %s need over five days is %d (want 30-85)", life::needName((life::Need)k), avg);
        line += std::string(" ") + life::needName((life::Need)k) + " " + std::to_string(avg);
      }
      info("need averages:%s", line.c_str());
    }
    c = g.life.findMut(svId);
    if (c) {
      int coin = 0;
      for (const life::Resident& r : c->res) coin += r.coin;
      std::string st;
      for (ew::Good gd : {ew::Good::Bread, ew::Good::Produce, ew::Good::Fish, ew::Good::Meat, ew::Good::Grain, ew::Good::Flour})
        st += std::string(" ") + ew::goodName(gd) + " " + std::to_string(c->stock[(size_t)gd]) + "@" + std::to_string(c->price[(size_t)gd]) + "%";
      info("start village after 5 days: mood %d flags 0x%x, coin %d, ties %zu, stock%s", c->mood, c->moodFlags, coin, c->ties.size(), st.c_str());
    }
  }

  // ---- a festival within 20 days of a content place
  {
    c = g.life.findMut(svId);
    const int startDay = g.day;
    int heldOn = -1;
    for (int d = 0; d < 20 * 24 && heldOn < 0; d++) {
      g.life.advanceHours(g, 1);
      c = g.life.findMut(svId);
      if (c && (c->moodFlags & life::MF_FESTIVAL)) heldOn = g.day;
    }
    c = g.life.findMut(svId);
    if (heldOn < 0) fail("no festival in 20 days (mood %d, flags 0x%x, next %d)", c ? c->mood : -1, c ? c->moodFlags : 0, c ? c->festivalDay : -2);
    else info("festival (%s) on day %d, %d days in", c ? g.life.festivalTitle(g.world, *c).c_str() : "", heldOn, heldOn - startDay);
  }

  // ---- a death: kin and friends grieve
  {
    c = g.life.findMut(svId);
    int victim = -1;
    for (const life::Resident& r : c->res)
      if (!(r.flags & (life::RF_DEAD | life::RF_AWAY)) && r.spouse >= 0 && !(c->res[(size_t)r.spouse].flags & (life::RF_DEAD | life::RF_AWAY))) { victim = r.idx; break; }
    if (victim < 0) fail("nobody in the start village has a living spouse");
    else {
      const std::vector<int> fr = g.life.friendsOf(*c, victim);
      const int spouse = c->res[(size_t)victim].spouse;
      g.life.residentDied(svId, victim, g.day);
      int grieving = 0;
      for (const life::Resident& r : c->res) grieving += (r.flags & life::RF_GRIEVING) != 0;
      if (!(c->res[(size_t)spouse].flags & life::RF_GRIEVING)) fail("the widow(er) of %s does not grieve", c->res[(size_t)victim].name.c_str());
      for (int f : fr)
        if (!(c->res[(size_t)f].flags & (life::RF_GRIEVING | life::RF_DEAD | life::RF_AWAY))) { fail("a friend of %s does not grieve", c->res[(size_t)victim].name.c_str()); break; }
      info("%s died: %d grieve (%zu friends)", c->res[(size_t)victim].name.c_str(), grieving, fr.size());
    }
  }

  // ---- raids: the chance a night by pressure, and the rolls over 60 nights
  {
    double expect = 0;
    int got = 0, places = 0;
    for (int si : g.world.nearSites) {
      if (si < 0 || si >= (int)g.world.sites.size() || !g.world.sites[(size_t)si].settlement()) continue;
      const Site& s = g.world.sites[(size_t)si];
      auto edge2 = [&](int x, int y) {
        const int dx = x < s.r.x ? s.r.x - x : x >= s.r.x + s.r.w ? x - (s.r.x + s.r.w - 1) : 0;
        const int dy = y < s.r.y ? s.r.y - y : y >= s.r.y + s.r.h ? y - (s.r.y + s.r.h - 1) : 0;
        return dx * dx + dy * dy;
      };
      int pressure = 0;
      for (int di : g.world.nearDens) {
        const Den& d = g.world.dens[(size_t)di];
        if (edge2(d.x, d.y) <= 3600) pressure++;
      }
      for (int oi : g.world.nearSites) {
        const Site& o = g.world.sites[(size_t)oi];
        if ((o.type == SiteType::Cave || o.type == SiteType::DragonLair || o.type == SiteType::BanditCamp) && !o.cleared && edge2(o.ex, o.ey) <= 3600)
          pressure++;
      }
      for (float mil : {0.0f, 0.5f, 1.0f})
        for (int doy : {0, 100, 200, 300}) {
          const int pct = life::Life::raidChancePct(pressure, mil, doy);
          if (pressure > 0 && (pct < 2 || pct > 8)) fail("%s: pressure %d gives a %d%% raid chance (want 2-8)", s.name.c_str(), pressure, pct);
          if (pressure == 0 && pct != 0) fail("%s: no pressure yet a %d%% raid chance", s.name.c_str(), pct);
        }
      if (pressure <= 0) continue;
      places++;
      const int pct = life::Life::raidChancePct(pressure, 0.4f, 300);
      for (int night = 0; night < 60; night++) got += life::Life::raidRoll(s.id, night, pct);
      expect += pct * 60 / 100.0;
    }
    const double sd = std::sqrt(std::max(1.0, expect));
    if (places && (got < expect - 3 * sd - 1 || got > expect + 3 * sd + 1)) fail("raids over 60 nights: %d against %.1f expected", got, expect);
    info("raids: %d places with monster pressure, %d raids in 60 nights (%.1f expected)", places, got, expect);
  }

  // ---- the gathering place at 20:00 in a town
  const int32_t homeGx = g.world.ox + g.world.sites[(size_t)sv].ex, homeGy = g.world.oy + g.world.sites[(size_t)sv].ey;
  const int town = g.world.findSiteNear(homeGx, homeGy, SiteType::Town, 8);
  if (town >= 0) g.world.sites[(size_t)town].discovered = true;
  if (town >= 0 && g.fastTravel(town)) {
    step(30);
    g.life.advanceHours(g, 24 - (int)g.hour + 19);   // the next evening, 19:xx
    g.hour = 20.0f;
    step(3);
    const ew::Gid tid = g.world.sites[(size_t)town].id;
    life::Census* tc = g.life.census(g.world, town);
    if (!tc) fail("no census for the town %s", g.world.sites[(size_t)town].name.c_str());
    else {
      int adults = 0, there = 0;
      for (const life::Resident& r : tc->res) {
        if (r.job == life::Job::Child || (r.flags & (life::RF_DEAD | life::RF_AWAY))) continue;
        adults++;
        there += atGathering(*tc, r);
      }
      if (adults && there * 4 < adults) fail("%s at 20:00: %d of %d adults at the gathering place (want 25%%)", g.world.sites[(size_t)town].name.c_str(), there, adults);
      info("%s (town, %zu residents) at 20:00: %d of %d adults at the gathering place (kind %d, building %d); mood %d", g.world.sites[(size_t)town].name.c_str(), tc->res.size(), there,
           adults, (int)tc->gatherKind, (int)tc->gathering, tc->mood);
      // ---- a forced famine: hungry within a day, dear food, mood down, the realm's food lower
      const realm::SettlementState* rs = g.realm.settlement(tid);
      const int food0 = rs ? rs->food : -1;
      const int mood0 = tc->mood;
      g.life.forceFamine(tid, g.day, 3);
      tc = g.life.findMut(tid);
      int dear = 0;
      for (ew::Good gd : {ew::Good::Bread, ew::Good::Produce, ew::Good::Meat}) dear += tc->price[(size_t)gd] >= 150;
      if (dear < 3) fail("a famine in %s: food prices %d%% / %d%% / %d%% (want 150%%+)", g.world.sites[(size_t)town].name.c_str(), tc->price[(size_t)ew::Good::Bread],
                         tc->price[(size_t)ew::Good::Produce], tc->price[(size_t)ew::Good::Meat]);
      int hungryAt = -1;
      for (int h = 0; h < 30; h++) {
        g.life.advanceHours(g, 1);
        tc = g.life.findMut(tid);
        if (tc && (tc->moodFlags & life::MF_HUNGRY) && hungryAt < 0) hungryAt = h + 1;
      }
      const realm::SettlementState* rs1 = g.realm.settlement(tid);
      if (hungryAt < 0 || hungryAt > 24) fail("a famine in %s: MF_HUNGRY %s", g.world.sites[(size_t)town].name.c_str(), hungryAt < 0 ? "never came" : "came after a day");
      if (tc && mood0 - tc->mood < 15) fail("a famine in %s: mood %d -> %d (want 15 down)", g.world.sites[(size_t)town].name.c_str(), mood0, tc->mood);
      if (rs1 && food0 >= 0 && rs1->food >= food0) fail("a famine in %s: the realm's food %d -> %d (want lower)", g.world.sites[(size_t)town].name.c_str(), food0, rs1->food);
      info("famine in %s: hungry after %d h, mood %d -> %d, realm food %d -> %d, flags 0x%x", g.world.sites[(size_t)town].name.c_str(), hungryAt, mood0,
           tc ? tc->mood : -1, food0, rs1 ? rs1->food : -1, tc ? tc->moodFlags : 0);
    }
  } else info("no town within reach");

  // ---- a capital: the step budget with the biggest census loaded
  {
    const int cap = g.world.findSiteNear(homeGx, homeGy, SiteType::City, 8, true);
    if (cap >= 0) g.world.sites[(size_t)cap].discovered = true;
    if (cap >= 0 && g.fastTravel(cap)) {
      g.life.stats.worstTickMs = 0;
      g.life.stats.tickMsSum = 0;
      g.life.stats.ticks = 0;
      step(120);
      for (int h = 0; h < 24; h++) { g.hour = (float)h + 0.2f; step(4); }
      const life::Census* cc = g.life.find(g.world.sites[(size_t)cap].id);
      const double avg = g.life.stats.ticks ? g.life.stats.tickMsSum / g.life.stats.ticks : 0;
      info("capital %s: %zu residents; Life::tick average %.4f ms, worst %.3f ms; census slice worst %.3f ms, whole census worst %.2f ms",
           g.world.sites[(size_t)cap].name.c_str(), cc ? cc->res.size() : (size_t)0, avg, g.life.stats.worstTickMs, g.life.stats.worstSliceMs,
           g.life.stats.worstCensusMs);
      if (!cc) fail("no census for the capital %s", g.world.sites[(size_t)cap].name.c_str());
      if (g.life.stats.worstTickMs > 1.5) fail("Life::tick's worst step with a capital loaded: %.3f ms (budget 1.5)", g.life.stats.worstTickMs);
      if (avg > 0.1) fail("Life::tick's average step: %.4f ms (budget 0.1)", avg);
      if (g.life.stats.worstSliceMs > 1.5) fail("a census slice took %.3f ms (budget 1.5)", g.life.stats.worstSliceMs);
    } else info("no capital within reach");
  }

  // ---- the life block
  {
    std::vector<uint8_t> a, b;
    g.life.serialize(a);
    if (a.size() > 64 * 1024) fail("the life block is %zu bytes (budget 64 KB)", a.size());
    life::Life L2;
    if (!L2.deserialize(a)) fail("the life block did not load");
    L2.serialize(b);
    if (a != b) fail("the life block does not round-trip byte-identically");
    // the censuses the window holds, rebuilt from their records
    for (int si : g.world.nearSites)
      if (si >= 0 && si < (int)g.world.sites.size() && g.world.sites[(size_t)si].settlement()) L2.census(g.world, si);
    std::vector<uint8_t> d;
    L2.serialize(d);
    if (d != a) {
      fail("the life block changes once the loaded censuses are rebuilt (%zu -> %zu bytes)", a.size(), d.size());
      for (int si : g.world.nearSites) {
        if (si < 0 || si >= (int)g.world.sites.size() || !g.world.sites[(size_t)si].settlement()) continue;
        const life::Census* x = g.life.find(g.world.sites[(size_t)si].id);
        const life::Census* y = L2.find(g.world.sites[(size_t)si].id);
        if (!x || !y) { info("DBG %s: %d %d", g.world.sites[(size_t)si].name.c_str(), x != nullptr, y != nullptr); continue; }
        int fd = 0;
        for (size_t k = 0; k < x->res.size() && k < y->res.size(); k++) fd += (x->res[k].flags & life::RF_SAVED) != (y->res[k].flags & life::RF_SAVED);
        info("DBG %s: res %zu/%zu ties %zu/%zu base %d/%d flagsdiff %d", g.world.sites[(size_t)si].name.c_str(), x->res.size(), y->res.size(), x->ties.size(),
             y->ties.size(), x->baseTies, y->baseTies, fd);
      }
    }
    // damaged blocks: refused, or loaded without a crash
    uint64_t h = 0x11FEull ^ seed;
    int loaded = 0;
    for (int i = 0; i < 200 && !a.empty(); i++) {
      std::vector<uint8_t> x = a;
      h = ew::mix64(h);
      if (i % 3 == 0) x.resize((size_t)(h % x.size()));
      else for (int k = 0; k < 1 + (int)(h % 5); k++) x[(size_t)(ew::mix64(h + k) % x.size())] ^= (uint8_t)(1 + (h >> (8 + k)) % 255);
      life::Life L3;
      if (!L3.deserialize(x)) continue;
      loaded++;
      for (int si : g.world.nearSites)
        if (si >= 0 && si < (int)g.world.sites.size() && g.world.sites[(size_t)si].settlement()) L3.census(g.world, si);
      std::vector<uint8_t> y;
      L3.serialize(y);
    }
    info("life block %zu bytes; %d of 200 damaged copies loaded (none crashed)", a.size(), loaded);
  }

  // ---- 120 residents' plans for an hour
  {
    life::Census big;
    life::buildCensus(g.world, sv, big);
    const size_t base = std::max<size_t>(1, big.res.size());
    while (big.res.size() < 120) { life::Resident r = big.res[big.res.size() % base]; r.idx = (uint16_t)big.res.size(); big.res.push_back(r); }
    big.res.resize(120);
    auto t0 = std::chrono::steady_clock::now();
    int sum = 0;
    for (int rep = 0; rep < 50; rep++)
      for (const life::Resident& r : big.res) sum += (int)g.life.plan(g.world, big, r, g.day, (float)(rep % 24)).act;
    const double perHour = msSince(t0) / 50.0;
    info("120 residents' plans %.4f ms per hour (sum %d)", perHour, sum);
    if (perHour > 0.5) fail("120 residents' plans take %.3f ms (budget 0.5 of the 1.5)", perHour);
  }
  return bad;
}

int lifeCmd(int argc, char** argv) {
  uint64_t A = 1, B = 5;
  bool report = false;
  for (int i = 2; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], A, B);
    else if (!strcmp(argv[i], "--report")) report = true;
  }
  int bad = 0;
  for (uint64_t s = A; s <= B; s++) bad += lifeSeed(s, report);
  printf("life: %d failures\n", bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--life", "M5 needs-driven citizens: census, spawn map, plans, 15.12 thresholds, famine, festival, grief, raids, the life block, the perf budget [--seeds A..B] [--report]", lifeCmd);
