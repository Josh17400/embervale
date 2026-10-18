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
//   jobs radiant <kind> [walk]   (P1b) test only: begin a radiant job of that kind here: its giver is the nearest person
//                         whose trade the kind takes (the walk's own trade with a walk number: rpg/story/jobs/<kind>.cpp
//                         KindDef::walks), composed from the world (or that walk's variant and twist), and its opening
//                         (the pitch, ACCEPT / DECLINE) opens as "ANY WORK GOING?" shows it. The `story` commands drive it
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/jobs_ops.h"
#include "rpg/story/dsl.h"
#include "rpg/story/radiant.h"
#include "rpg/story/story.h"
#include "rpg/story/story_internal.h"
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

// (P1b) jobs radiant <kind> [walk]: begin a radiant job here, as "ANY WORK GOING?" would
bool cmdJobsRadiant(ScriptCtx& c) {
  using namespace story;
  Game& g = c.game;
  const std::string kid = c.arg(2);
  int kind = -1;
  for (int k = 0; k < radiant::kindCount(); k++) if (kid == radiant::kindId(k)) kind = k;
  if (kind < 0) { c.fail("jobs radiant: no kind '" + kid + "'"); return true; }
  const radiant::KindDef& K = *radiant::kinds()[(size_t)kind];
  int si = siteHere(g);
  if (si < 0) { c.fail("jobs radiant: no settlement in play"); return true; }
  g.life.census(g.world, si);
  const int walk = c.arg(3).empty() ? -1 : std::atoi(c.arg(3).c_str());
  if (walk >= (int)K.walks.size()) { c.fail("jobs radiant: kind " + kid + " has " + std::to_string(K.walks.size()) + " walks"); return true; }
  // the giver: the nearest person of a trade the kind takes (the walk's own trade), in this settlement
  const Vec2 pp = g.pl().p;
  const Actor* giver = nullptr;
  float bd = 1e30f;
  for (const Actor& a : g.actors) {
    if (!a.npc || !a.human || isStoryPerson(a) || a.st == AState::Dead) continue;
    bool fits = false;
    if (walk >= 0) fits = (int)a.role == K.walks[(size_t)walk].trade;
    else for (int t : K.trades) fits |= (int)a.role == t;
    if (!fits) continue;
    const float d = (a.p.x - pp.x) * (a.p.x - pp.x) + (a.p.y - pp.y) * (a.p.y - pp.y);
    if (d < bd && d < (90.0f * TILE) * (90.0f * TILE)) { bd = d; giver = &a; }
  }
  if (!giver && c.waited < 4.0f) return false;   // (the street fills a moment after the hero arrives)
  if (giver) {
    // step up to the giver, as the talk would (the pitch is theirs)
    const int hs = homeSiteOf(g, *giver);
    if (hs >= 0 && g.world.sites[(size_t)hs].settlement()) si = hs;
    g.pl().p = giver->p + Vec2(0, 16);
    g.pl().face = 1; g.pl().aim = Vec2(0, -1);
  }
  const uint64_t seed = ew::mix64((uint64_t)g.day * 0x9E37ull ^ (uint64_t)kind ^ g.seed);
  const std::string id = walk >= 0 ? radiant::composeVariant(g, kind, walk, si, seed) : radiant::composeFor(g, kind, giver, si, seed);
  if (id.empty()) { c.fail("jobs radiant: kind " + kid + " cannot be composed and cast here"); return true; }
  std::string why;
  const uint32_t inst = g.story.start(g, id, giver ? giver->id : -1, si, &why);
  if (!inst) { c.fail("jobs radiant: " + id + ": " + why); return true; }
  Instance* in = g.story.find(inst);
  std::printf("script: jobs radiant %s: %s (instance %u, stage %s, giver %s)\n", kid.c_str(), id.c_str(), inst, in ? g.story.stageName(*in).c_str() : "?",
              giver ? giver->name.c_str() : "the place");
  if (in && !g.story.isGoal(*in) && !g.story.isEnd(*in)) {
    // the opening is the giver's pitch with its own ACCEPT / DECLINE, as Game::storyBeginJob shows it
    const dsl::Script* s = scriptById(id);
    g.dlg = Dialogue();
    const Binding* b = s ? bindingOf(*in, s->stages[(size_t)in->stage].talk) : nullptr;
    g.dlg.speaker = b ? b->name : std::string();
    g.story.showDialogue(g, *in);
    g.mode = Mode::Dialogue;
  }
  return true;
}

bool cmdJobs(ScriptCtx& c) {
  Game& g = c.game;
  if (c.arg(1) == "radiant") return cmdJobsRadiant(c);
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
EMB_SCRIPT_CMD("jobs", "jobs | jobs radiant <kind> [walk]: print this week's job pool here or nearest (P1 JOBS); begin a radiant job of a kind here, its pitch open (P1b)", cmdJobs);

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
