// P1 "Fresh Roads" JOBS lane script commands (rpg/view/script_api.h; `embervale --script-help` lists them). Test only:
// none of them touches a save.
//   jobs                  print this week's job pool of the settlement here (or the nearest): each job's kind, its
//                         need and the person who will ask (worked out now if nobody has asked yet)
//   jobsleads [n]         test only: fill the journal's LEADS as play would: n rumours (default 3) naming real open jobs
//                         in the settlements round about (their people are counted first), a place rumour, something
//                         overheard in the street, a street incident seen, a notice, and one old lead gone stale
//   expect jobs pool <n>  this week's pool here holds at least n jobs
//   expect jobs leads <n> the journal holds at least n leads that are not taken
//   expect jobs hunt      the tracked quest is a Hunt with a target area and a marker
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/jobs_ops.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"

namespace {

const char* qtWord(uint8_t t) {
  static const char* n[] = {"MAIN", "CLEAR", "HUNT", "RETRIEVE", "BOUNTY", "DELIVER", "HEIRLOOM", "MISSING", "NAMEDBANDIT", "PROTECT", "STORY", "WAR",
                            "SUPPLY", "SEA", "CAMPAIGN", "ENVOY", "FIEF", "ERRAND"};
  return t < sizeof(n) / sizeof(n[0]) ? n[t] : "?";
}

// the settlement the hero is in, else the nearest one in play
int siteHere(Game& g) {
  if (g.curSite >= 0 && g.curSite < (int)g.world.sites.size() && g.world.sites[(size_t)g.curSite].settlement()) return g.curSite;
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  int best = -1;
  float bd = 1e30f;
  for (size_t i = 0; i < g.world.sites.size(); i++) {
    const Site& s = g.world.sites[i];
    if (!s.settlement()) continue;
    const float d = (s.ex - px) * (s.ex - px) + (s.ey - py) * (s.ey - py);
    if (d < bd) { bd = d; best = (int)i; }
  }
  return best;
}

bool cmdJobs(ScriptCtx& c) {
  Game& g = c.game;
  const int si = siteHere(g);
  if (si < 0) { c.fail("jobs: no settlement in play"); return true; }
  g.life.census(g.world, si);
  const jobs::Pool* p = JobOps::ensurePool(g, si);
  if (!p) { c.fail("jobs: no pool (no census)"); return true; }
  const life::Census* cs = g.life.find(g.world.sites[(size_t)si].id);
  std::printf("script: jobs of %s, week %d: %d\n", g.world.sites[(size_t)si].name.c_str(), p->week, (int)p->jobs.size());
  for (const jobs::PoolJob& j : p->jobs) {
    std::string who = "THE NOTICE BOARD";
    if (cs)
      for (const life::Resident& r : cs->res)
        if (g.npcKeyOf(si, -1, jobs::RES_SLOT0 + r.idx) == j.giver) { who = r.name + " (" + life::jobName(r.job) + ")"; break; }
    std::printf("  %-11s %-10s %-6s %s\n", qtWord(j.qtype), jobs::jobNeedName(j.need), j.state == jobs::PJ_OPEN ? "open" : j.state == jobs::PJ_GIVEN ? "given" : "done",
                who.c_str());
  }
  return true;
}
EMB_SCRIPT_CMD("jobs", "jobs: print this week's job pool of the settlement here or nearest (P1 JOBS)", cmdJobs);

bool cmdJobsLeads(ScriptCtx& c) {
  Game& g = c.game;
  const int n = c.arg(1).empty() ? 3 : std::max(0, std::atoi(c.arg(1).c_str()));
  const int si = siteHere(g);
  for (int k : g.world.nearSites)
    if (k >= 0 && k < (int)g.world.sites.size() && g.world.sites[(size_t)k].settlement()) g.life.census(g.world, k);
  // the rumours as an innkeeper tells them (a person talking)
  const Mode m0 = g.mode;
  const int a0 = g.dlg.actor;
  g.mode = Mode::Dialogue;
  g.dlg.actor = 1;
  int made = 0;
  for (int i = 0; i < n; i++) made += JobOps::rumourJob(g, si) ? 1 : 0;
  g.mode = m0;
  g.dlg.actor = a0;
  // what the other lanes add in play (the LIFE lane's gossip and incidents, the board), with real places
  const int32_t px = g.world.ox + (int)std::floor(g.pl().p.x / TILE), py = g.world.oy + (int)std::floor(g.pl().p.y / TILE);
  auto add = [&](jobs::LeadFrom f, int dx, int dy, const char* text, int daysAgo) {
    jobs::Lead l;
    l.from = f; l.qtype = (uint8_t)QType::COUNT; l.hasPos = true; l.gx = px + dx; l.gy = py + dy; l.text = text;
    if (si >= 0) l.site = g.world.sites[(size_t)si].id;
    const uint32_t id = g.addLead(l);
    if (jobs::Lead* x = g.jobBook.lead(id)) { x->day = g.day - daysAgo; if (daysAgo > jobs::LEAD_DAYS) x->state = jobs::LS_STALE; }
  };
  add(jobs::LeadFrom::Gossip, 30, -12, "TWO CARTERS SAY WOLVES TOOK A FOAL BY THE MILL", 0);
  add(jobs::LeadFrom::Incident, -8, 4, "A CUTPURSE RAN OFF TOWARD THE DOCKS", 1);
  add(jobs::LeadFrom::Board, 2, -3, "A NOTICE ASKS FOR HANDS AT THE QUARRY", 2);
  add(jobs::LeadFrom::Teller, 120, 90, "AN OLD WOMAN SPOKE OF A BELL UNDER THE LAKE", 3);
  add(jobs::LeadFrom::Rumour, -260, 140, "A BANDIT CAMP IN THE HILLS, THEY SAY", 13);
  std::printf("script: jobsleads: %d rumours of jobs, %d leads in the journal\n", made, (int)g.jobBook.leads.size());
  return true;
}
EMB_SCRIPT_CMD("jobsleads", "jobsleads [n]: test only: n rumour leads naming real jobs round about, and one lead of each other source (P1 JOBS)",
               cmdJobsLeads);

bool expJobs(ScriptCtx& c) {
  Game& g = c.game;
  const std::string w = c.arg(2);
  const int n = std::atoi(c.arg(3).c_str());
  if (w == "pool") {
    const int si = siteHere(g);
    const jobs::Pool* p = si >= 0 ? JobOps::ensurePool(g, si) : nullptr;
    if (!p || (int)p->jobs.size() < n) c.fail("expect jobs pool: " + std::to_string(p ? (int)p->jobs.size() : -1) + " jobs");
  } else if (w == "leads") {
    int k = 0;
    for (const jobs::Lead& l : g.jobBook.leads) k += l.state != jobs::LS_TAKEN;
    if (k < n) c.fail("expect jobs leads: " + std::to_string(k));
  } else if (w == "hunt") {
    const Quest* q = nullptr;
    for (const Quest& x : g.quests) if (x.id == g.trackedQuest) q = &x;
    int tx = 0, ty = 0;
    if (!q || q->type != QType::Hunt || !q->hasPos || !g.questTarget(q->id, tx, ty)) c.fail("expect jobs hunt: the tracked quest is not a hunt with a target");
    else std::printf("script: hunt '%s': %s\n", q->title.c_str(), g.questStatus(*q).c_str());
  } else c.fail("expect jobs pool|leads|hunt");
  return true;
}
EMB_SCRIPT_CMD("expect:jobs", "expect jobs pool <n> | leads <n> | hunt: this week's pool here, the journal's leads, the tracked hunt (P1 JOBS)", expJobs);

}  // namespace
