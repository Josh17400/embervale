// rpg_test --jobs [--seeds A..B] [--verbose]
// P1 "Fresh Roads" (JOBS lane; VISION_PLAN 15.34): the settlements' weekly job pools and the journal's leads.
//   pools      worked out once a week from the land's needs and frozen (the same seed gives the same pool; asking
//              again, or changing who has done what, changes nothing until the week turns); sizes by settlement
//              (village 3-5, town 6-10, city 10-16, fewer when fewer people can ask; towns and cities add the notice
//              board's two posted jobs); the givers rotate (last week's givers mostly wait); no lifetime cap blocks a
//              fresh week (a giver with 50 jobs done still asks); taking a job gives the pool's job out
//   hunts      every Hunt has a target area (a den or a hunting ground: hasPos), a marker (questTarget) and a journal
//              line ("THIN THE WOLVES BY ... 2/5")
//   board      the town's two posted jobs are its pool's board jobs; taken, the board posts nothing more that week
//   leads      dedup (same text, or same person and kind), the newest 24 kept, stale after 10 days, taken with the job
//              they name; a rumour names a real open job in another settlement
//   save       the jobs block round-trips byte-identical
//   cost       Game::hasOffer per call (us), every person of the start village asked many times
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/sim/game_internal.h"
#include "rpg/sim/jobs_ops.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

bool g_jverbose = false;

void tick(Game& g, int frames) {
  for (int f = 0; f < frames; f++) {
    g.update(SIM_DT, Input());
    g.events.clear();
    if (g.mode == Mode::Dialogue || g.mode == Mode::Shop || g.mode == Mode::LevelUp) g.mode = Mode::Play;
    if (g.mode == Mode::Dead) g.respawn();
  }
}

bool samePool(const jobs::Pool& a, const jobs::Pool& b) {
  if (a.site != b.site || a.week != b.week || a.jobs.size() != b.jobs.size()) return false;
  for (size_t i = 0; i < a.jobs.size(); i++)
    if (a.jobs[i].qtype != b.jobs[i].qtype || a.jobs[i].need != b.jobs[i].need || a.jobs[i].giver != b.jobs[i].giver || a.jobs[i].state != b.jobs[i].state) return false;
  return true;
}

// a stand-in body for a census resident (as life_game.cpp spawns them: site, bldg -1, slot RES_SLOT0 + idx)
Actor residentActor(const Game& g, int si, const life::Resident& r) {
  Actor a;
  a.npc = true; a.human = true; a.site = si; a.bldg = -1; a.slot = jobs::RES_SLOT0 + r.idx; a.resident = r.idx;
  a.role = life::jobRole(r.job); a.name = r.name;
  // home: the door step of its house (life_game.cpp spawnResident), else the settlement's heart
  const Site& S = g.world.sites[(size_t)si];
  int hx = S.ex, hy = S.ey;
  if (r.home >= 0 && S.bldgFirst + r.home < (int)g.world.over.bldgs.size()) { hx = g.world.over.bldgs[(size_t)(S.bldgFirst + r.home)].doorX(); hy = g.world.over.bldgs[(size_t)(S.bldgFirst + r.home)].doorY() + 1; }
  a.home = Vec2(hx * TILE + 8.0f, hy * TILE + 8.0f);
  a.p = a.home;
  return a;
}
// the resident whose key a pool job names (-1 none)
int residentOf(const Game& g, int si, uint64_t key) {
  const life::Census* c = g.life.find(g.world.sites[(size_t)si].id);
  if (!c) return -1;
  for (const life::Resident& r : c->res) if (g.npcKeyOf(si, -1, jobs::RES_SLOT0 + r.idx) == key) return r.idx;
  return -1;
}

const char* qtName(uint8_t t) {
  static const char* n[] = {"Main", "Clear", "Hunt", "Retrieve", "Bounty", "Deliver", "Heirloom", "Missing", "NamedBandit", "Protect", "Story", "War",
                            "Supply", "Sea", "Campaign", "Envoy", "Fief", "Errand"};
  return t < sizeof(n) / sizeof(n[0]) ? n[t] : "?";
}

int jobsSeed(uint64_t seed, double& usPerCall, int& hunts, int& huntsOk, std::map<std::string, int>& kinds, int& farmerJobs, int& allJobs) {
  int bad = 0;
  auto fail = [&](const std::string& s) { out("FAIL: jobs: %s\n", s.c_str()); bad++; };
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play; g.godMode = true; g.noWildSpawns = true; g.hour = 10.0f;
  tick(g, 20);
  const int start = g.world.startSite;
  if (!g.life.census(g.world, start)) { fail("no census for the start village"); return bad; }

  // ---------------------------------------------------------------- the pool: worked out once, deterministic, frozen
  jobs::Pool* p = JobOps::ensurePool(g, start);
  if (!p) { fail("no pool for the start village"); return bad; }
  const jobs::Pool first = *p;
  {
    Game h(seed);
    h.newEndlessGame(seed);
    h.mode = Mode::Play; h.godMode = true; h.noWildSpawns = true; h.hour = 10.0f;
    tick(h, 20);
    h.life.census(h.world, start);
    const jobs::Pool* q = JobOps::ensurePool(h, start);
    if (!q || !samePool(*q, first)) fail("the same seed worked out another pool (not deterministic)");
  }
  for (const jobs::PoolJob& j : first.jobs) g.npcQuestsDone[j.giver] += 3;   // (would change the rotation, were it re-worked)
  if (!samePool(*JobOps::ensurePool(g, start), first)) fail("the pool changed within its week (not frozen)");
  const int village = (int)first.jobs.size();
  if (village < 1 || village > 5) fail("start village pool of " + std::to_string(village) + " jobs (want 3-5)");
  else if (village < 3) out("WARN: jobs: start village pool of %d jobs (few people can ask)\n", village);
  if (g_jverbose)
    for (const jobs::PoolJob& j : first.jobs) out("  week %d: %s/%s giver %d\n", first.week, qtName(j.qtype), jobs::jobNeedName(j.need), residentOf(g, start, j.giver));

  // ---------------------------------------------------------------- every Hunt has a target, a marker and a line
  auto checkHunt = [&](Game& gg, const Quest& q, const std::string& who) {
    hunts++;
    if (!q.hasPos) { fail("a hunt from " + who + " without a target area"); return; }
    Game t = gg;
    t.debugAccept(q);
    const Quest& b = t.quests.back();
    int tx = 0, ty = 0;
    const bool mk = t.questTarget(b.id, tx, ty);
    const std::string line = t.questStatus(b);
    if (!mk) fail("a hunt from " + who + " without a marker");
    else if (line.find("THIN THE") == std::string::npos || line.size() > 36) fail("a hunt's journal line '" + line + "'");
    else huntsOk++;
    if (g_jverbose) out("  hunt: %s | %s | %s\n", b.title.c_str(), line.c_str(), b.desc.c_str());
  };

  // ---------------------------------------------------------------- offers: each pool job asks for its kind
  {
    const life::Census* c = g.life.find(g.world.sites[(size_t)start].id);
    int asked = 0, matched = 0;
    for (const jobs::PoolJob& j : first.jobs) {
      const int ri = residentOf(g, start, j.giver);
      if (ri < 0 || !c) continue;
      const Actor a = residentActor(g, start, c->res[(size_t)ri]);
      if (!JobOps::hasOffer(g, a)) {
        // (a night on the fields is asked for by day; an unmet need may stand first: lifeOffer)
        if (j.qtype != (uint8_t)QType::Protect) fail(std::string("a pool giver with no offer (") + qtName(j.qtype) + ")");
        continue;
      }
      Game t = g;
      const Quest q = JobOps::makeOffer(t, a);
      asked++;
      if ((uint8_t)q.type == j.qtype || q.type == QType::Supply || q.type == QType::Missing) matched++;
      else if (g_jverbose) out("  pool %s/%s came out as %s\n", qtName(j.qtype), jobs::jobNeedName(j.need), qtName((uint8_t)q.type));
      if (q.title.empty()) fail("a pool job without a title");
      if (q.type == QType::Hunt) checkHunt(t, q, a.name);
      // taking it gives the pool's job out: the giver has nothing more this week
      t.debugAccept(q);
      if (const jobs::Pool* tp = t.jobBook.pool(first.site, first.week)) {
        bool given = false;
        for (const jobs::PoolJob& x : tp->jobs) if (x.giver == j.giver && x.state == jobs::PJ_GIVEN) given = true;
        if (!given && q.giverSlot >= 0) fail("taking a pool job did not give it out");
      }
      t.quests.back().state = QState::Done;
      if (JobOps::openJob(t, a)) fail("a giver's pool job is still open after it was taken");
    }
    if (asked > 0 && matched < asked) out("WARN: jobs: %d of %d offers came out of another kind (a fallback hunt)\n", asked - matched, asked);
  }

  // ---------------------------------------------------------------- weeks: rotation, no lifetime cap
  std::map<uint64_t, int> givers;
  int total = 0;
  {
    Game w = g;
    for (int week = 0; week < 6; week++) {
      const jobs::Pool* wp = JobOps::ensurePool(w, start);
      if (!wp) { fail("no pool in week " + std::to_string(week)); break; }
      for (const jobs::PoolJob& j : wp->jobs) { givers[j.giver]++; total++; w.npcQuestsDone[j.giver] += 10; }   // (lifetime: far past any M2 cap)
      w.day += 7;
      tick(w, 2);
    }
    const jobs::Pool* last = w.jobBook.pool(w.world.sites[(size_t)start].id, w.day / 7);
    if (!last) last = JobOps::ensurePool(w, start);
    if (!last || last->jobs.empty()) fail("a fresh week has no jobs once every giver has done many (a lifetime cap)");
    int most = 0;
    for (const auto& kv : givers) most = std::max(most, kv.second);
    if (total > 0 && (int)givers.size() * 10 < total * 5) fail("the givers do not rotate: " + std::to_string(givers.size()) + " people for " + std::to_string(total) + " jobs");
    if (most > 4) fail("one person gave " + std::to_string(most) + " jobs in 6 weeks");
    if (g_jverbose) out("  6 weeks: %d jobs from %zu people (most %d)\n", total, givers.size(), most);
  }

  // ---------------------------------------------------------------- towns and cities: sizes, the board
  for (SiteType t : {SiteType::Town, SiteType::City}) {
    const Site st = g.world.sites[(size_t)start];
    const int si = g.world.findSiteNear(g.world.ox + st.ex, g.world.oy + st.ey, t, 8);
    if (si < 0) { out("WARN: jobs: no %s near the start\n", t == SiteType::Town ? "town" : "city"); continue; }
    Game h = g;
    const ew::Gid id = h.world.sites[(size_t)si].id;
    h.world.sites[(size_t)si].discovered = true;   // (heard of: the way there is known)
    if (!h.fastTravel(si)) { out("WARN: jobs: could not travel to the %s\n", t == SiteType::Town ? "town" : "city"); continue; }
    tick(h, 6);
    const int hs = h.world.siteHandle(id);
    if (hs < 0 || !h.life.census(h.world, hs)) { fail("no census in the town"); continue; }
    const jobs::Pool* tp = JobOps::ensurePool(h, hs);
    if (!tp) { fail("no pool in the town"); continue; }
    const int n = (int)tp->jobs.size() - 2;   // (the board's two)
    const int tier = h.life.find(id)->tier;
    const int lo = tier == 1 ? 6 : 10, hi = tier == 1 ? 10 : 16;
    if (n > hi || n < 1) fail(std::string(t == SiteType::Town ? "town" : "city") + " pool of " + std::to_string(n) + " jobs (want " + std::to_string(lo) + "-" + std::to_string(hi) + ")");
    else if (n < lo) out("WARN: jobs: %s pool of %d jobs (few people can ask)\n", t == SiteType::Town ? "town" : "city", n);
    for (const jobs::PoolJob& j : tp->jobs) { kinds[qtName(j.qtype)]++; allJobs++; }
    {
      const life::Census* c = h.life.find(id);
      for (const jobs::PoolJob& j : tp->jobs) {
        const int ri = residentOf(h, hs, j.giver);
        if (ri >= 0 && life::jobRole(c->res[(size_t)ri].job) == Role::Farmer) farmerJobs++;
      }
    }
    // the board: its posted jobs are the pool's two board jobs
    for (int b = 0; b < 2; b++) {
      Actor A;
      A.npc = true; A.human = true; A.site = hs; A.bldg = -1; A.slot = jobs::BOARD_SLOT0 + b; A.name = "THE NOTICE BOARD"; A.role = Role::Guard;
      const uint64_t key = h.npcKey(A);
      const jobs::PoolJob* bj = JobOps::openJobByKey(h, key);
      if (!bj) { fail("the board has no pool job"); continue; }
      const uint8_t want = bj->qtype;
      bool ok = false;
      Quest q = h.offerFor(A, b == 0 ? QType::Bounty : QType::Hunt, ok);
      if (!ok) { q = h.offerFor(A, QType::COUNT, ok); }
      if (!ok) { fail("the board's pool job made no offer"); continue; }
      if ((uint8_t)q.type != want) fail(std::string("the board posted a ") + qtName((uint8_t)q.type) + " for a pool " + qtName(want));
      if (q.type == QType::Hunt) checkHunt(h, q, "the board");
      h.debugAccept(q);
      h.quests.back().state = QState::Done;
      bool ok2 = false;
      (void)h.offerFor(A, QType::COUNT, ok2);
      if (ok2) fail("the board posts again after its job was taken");
    }
    // every Hunt in the town's pool has a target
    const life::Census* c = h.life.find(id);
    for (const jobs::PoolJob& j : tp->jobs) {
      if (j.qtype != (uint8_t)QType::Hunt) continue;
      const int ri = residentOf(h, hs, j.giver);
      if (ri < 0) continue;
      const Actor a = residentActor(h, hs, c->res[(size_t)ri]);
      Game t2 = h;
      if (!JobOps::hasOffer(t2, a)) continue;
      const Quest q = JobOps::makeOffer(t2, a);
      if (q.type == QType::Hunt) checkHunt(t2, q, a.name);
    }
    if (g_jverbose) out("  %s %s: %d jobs + board\n", t == SiteType::Town ? "town" : "city", h.world.sites[(size_t)hs].name.c_str(), n);
  }
  for (const jobs::PoolJob& j : first.jobs) {
    kinds[qtName(j.qtype)]++; allJobs++;
    const int ri = residentOf(g, start, j.giver);
    const life::Census* c = g.life.find(g.world.sites[(size_t)start].id);
    if (ri >= 0 && c && life::jobRole(c->res[(size_t)ri].job) == Role::Farmer) farmerJobs++;
  }

  // ---------------------------------------------------------------- leads
  {
    Game t = g;
    jobs::Lead l;
    l.from = jobs::LeadFrom::Gossip; l.qtype = (uint8_t)QType::Hunt; l.who = 0x1234; l.text = "THE MILLER WANTS THE WOLVES GONE";
    const uint32_t a = t.addLead(l);
    const uint32_t b = t.addLead(l);
    jobs::Lead l2 = l;
    l2.text = "SOMEONE ELSE SAYS THE MILLER WANTS THE WOLVES GONE";
    const uint32_t c2 = t.addLead(l2);
    if (!a || a != b || a != c2 || t.jobBook.leads.size() != 1) fail("lead dedup (same text, same person and kind)");
    for (int i = 0; i < 30; i++) {
      jobs::Lead x;
      x.from = jobs::LeadFrom::Rumour; x.qtype = (uint8_t)QType::COUNT; x.text = "A PLACE " + std::to_string(i);
      t.addLead(x);
    }
    if ((int)t.jobBook.leads.size() > jobs::LEAD_MAX) fail("more than LEAD_MAX leads kept");
    t.day += jobs::LEAD_DAYS + 1;
    tick(t, 2);
    int open = 0;
    for (const jobs::Lead& x : t.jobBook.leads) open += x.state == jobs::LS_OPEN;
    if (open) fail("leads did not go stale after LEAD_DAYS");
    // taken: a lead naming a pool giver is taken with their job
    Game u = g;
    if (!first.jobs.empty()) {
      const life::Census* cc = u.life.find(u.world.sites[(size_t)start].id);
      for (const jobs::PoolJob& j : first.jobs) {
        const int ri = residentOf(u, start, j.giver);
        if (ri < 0 || j.qtype == (uint8_t)QType::Protect) continue;
        const Actor ga = residentActor(u, start, cc->res[(size_t)ri]);
        if (!JobOps::hasOffer(u, ga)) continue;
        jobs::Lead ln;
        ln.from = jobs::LeadFrom::Rumour; ln.qtype = j.qtype; ln.who = j.giver; ln.site = u.world.sites[(size_t)start].id; ln.text = "A TEST LEAD";
        const uint32_t id = u.addLead(ln);
        if (!JobOps::leadNames(u, ga)) fail("leadNames does not see the lead's person");
        const Quest q = JobOps::makeOffer(u, ga);
        u.debugAccept(q);
        const jobs::Lead* got = u.jobBook.lead(id);
        if (!got || got->state != jobs::LS_TAKEN) fail("taking the job did not take the lead naming it");
        break;
      }
    }
    // a rumour names a real open job elsewhere (when another settlement's people are known)
    Game v = g;
    v.mode = Mode::Dialogue; v.dlg.actor = 1;
    for (int si : v.world.nearSites)
      if (si >= 0 && si != start && v.world.sites[(size_t)si].settlement()) v.life.census(v.world, si);
    const uint32_t rl = JobOps::rumourJob(v, start);
    if (rl) {
      const jobs::Lead* x = v.jobBook.lead(rl);
      if (!x || !x->who || x->text.empty() || !x->hasPos) fail("a rumour lead without a person, words or a place");
      else {
        const jobs::PoolJob* pj = JobOps::openJobByKey(v, x->who);
        if (!pj) fail("a rumour lead names no open job");
        if (g_jverbose) out("  rumour: %s\n", x->text.c_str());
      }
    } else out("WARN: jobs: no rumour lead (no other settlement known)\n");
  }

  // ---------------------------------------------------------------- save: the jobs block round-trips
  {
    Game t = g;
    jobs::Lead l;
    l.from = jobs::LeadFrom::Incident; l.qtype = (uint8_t)QType::COUNT; l.text = "A THIEF RAN TOWARD THE DOCKS"; l.hasPos = true; l.gx = 5; l.gy = -7;
    t.addLead(l);
    std::vector<uint8_t> a, b;
    t.serialize(a);
    Game h(1);
    if (!h.deserialize(a)) fail("the save with pools and leads did not load");
    else {
      h.serialize(b);
      if (a != b) fail("the save with pools and leads is not byte-identical");
      if (h.jobBook.pools.size() != t.jobBook.pools.size() || h.jobBook.leads.size() != t.jobBook.leads.size()) fail("pools or leads lost in the save");
    }
  }

  // ---------------------------------------------------------------- the cost of hasOffer (every frame, every person)
  {
    std::vector<const Actor*> folk;
    for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].npc && g.actors[k].human) folk.push_back(&g.actors[k]);
    if (!folk.empty()) {
      volatile int yes = 0;
      const int reps = std::max(1, 20000 / (int)folk.size());
      const auto t0 = std::chrono::steady_clock::now();
      for (int r = 0; r < reps; r++) for (const Actor* a : folk) yes += JobOps::hasOffer(g, *a) ? 1 : 0;
      const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
      usPerCall = us / (double)(reps * (int)folk.size());
      if (usPerCall > 25.0) fail("hasOffer costs " + std::to_string(usPerCall) + " us a call (want <= 25)");
    }
  }
  return bad;
}

int jobsCmd(int argc, char** argv) {
  uint64_t A = 1, B = 5;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) {
      if (!parseSeedRange(argv[++i], A, B)) { printf("bad --seeds range '%s'\n", argv[i]); return 2; }
    } else if (!strcmp(argv[i], "--verbose")) g_jverbose = true;
  }
  int bad = 0, hunts = 0, huntsOk = 0, farmer = 0, all = 0;
  double worstUs = 0, sumUs = 0;
  std::map<std::string, int> kinds;
  for (uint64_t s = A; s <= B; s++) {
    g_curSeed = s;
    double us = 0;
    bad += jobsSeed(s, us, hunts, huntsOk, kinds, farmer, all);
    worstUs = std::max(worstUs, us);
    sumUs += us;
  }
  std::string k;
  {
    std::vector<std::pair<int, std::string>> v;
    for (const auto& kv : kinds) v.push_back({kv.second, kv.first});
    std::sort(v.rbegin(), v.rend());
    for (const auto& x : v) k += (k.empty() ? "" : ", ") + x.second + " " + std::to_string(x.first);
  }
  printf("jobs: seeds %llu..%llu: %d failures; hunts %d with a target, marker and line %d; pool kinds: %s; farmers %d of %d; hasOffer %.2f us a call (worst seed %.2f)\n",
         (unsigned long long)A, (unsigned long long)B, bad, hunts, huntsOk, k.c_str(), farmer, all, sumUs / (double)(B - A + 1), worstUs);
  printf("%s: --jobs\n", bad ? "FAIL" : "PASS");
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--jobs", "P1: the weekly job pools (determinism, freezing, sizes, rotation, no lifetime cap, the board), hunts' targets, leads, the save, hasOffer's cost [--seeds A..B --verbose]",
             jobsCmd);
