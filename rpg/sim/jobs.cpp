// P1 "Fresh Roads" (VISION_PLAN 15.34): the settlements' weekly job pools, the journal's leads (rpg/sim/jobs.h) and
// their Game hooks (rpg/sim/jobs_ops.h). JOBS lane after phase A.
//
// Phase A: the records, the save block, Game::addLead (dedup, the newest LEAD_MAX kept) and the hooks: leads go stale
// after LEAD_DAYS, taking a job marks the leads that named its giver taken, pools older than last week are dropped.
// The JOBS lane fills the pools (rpg/sim/jobs_pool.cpp) and reads them in Game::hasOffer / makeOffer (quests.cpp).
#include <algorithm>
#include <string>
#include <vector>
#include "rpg/sim/game_internal.h"
#include "rpg/sim/jobs_ops.h"

namespace jobs {

const char* leadFromName(LeadFrom f) {
  static const char* n[] = {"RUMOUR", "OVERHEARD", "SEEN", "NOTICE", "TALE"};
  return (int)f < (int)LeadFrom::COUNT ? n[(int)f] : "LEAD";
}

const char* errandName(ErrandKind k) {
  static const char* n[] = {"THIEF", "CART", "BRAWL", "LOST", "HELP"};
  return (int)k < (int)ErrandKind::COUNT ? n[(int)k] : "ERRAND";
}

Pool* Book::pool(ew::Gid site, int32_t week) {
  for (Pool& p : pools) if (p.site == site && p.week == week) return &p;
  return nullptr;
}

Lead* Book::lead(uint32_t id) {
  for (Lead& l : leads) if (l.id == id) return &l;
  return nullptr;
}

void Book::serialize(std::vector<uint8_t>& out) const {
  out.clear();
  BinW w(out);
  w.u8(JOBS_BLOCK_VER);
  std::vector<const Pool*> ps;
  for (const Pool& p : pools) ps.push_back(&p);
  std::sort(ps.begin(), ps.end(), [](const Pool* a, const Pool* b) { return a->site != b->site ? a->site < b->site : a->week < b->week; });
  w.u32((uint32_t)ps.size());
  for (const Pool* p : ps) {
    w.u64(p->site); w.i32(p->week);
    w.u16((uint16_t)p->jobs.size());
    for (const PoolJob& j : p->jobs) { w.u8(j.qtype); w.u8(j.need); w.u8(j.state); w.u64(j.giver); }
  }
  w.u32(nextLead);
  w.u32((uint32_t)leads.size());
  for (const Lead& l : leads) {
    w.u32(l.id); w.u8((uint8_t)l.from); w.u8(l.qtype); w.u8(l.state); w.i32(l.day); w.u64(l.site); w.u64(l.who);
    w.u8(l.hasPos ? 1 : 0); w.i32(l.gx); w.i32(l.gy); w.str(l.text);
  }
}

bool Book::deserialize(const std::vector<uint8_t>& in) {
  BinR r(in);
  const uint8_t ver = r.u8();
  if (r.bad || ver < 1 || ver > JOBS_BLOCK_VER) return false;
  auto fits = [&](uint32_t n, size_t minBytes) { return !r.bad && (uint64_t)n * minBytes <= (uint64_t)(in.size() - std::min(in.size(), r.p)); };
  Book B;
  const uint32_t np = r.u32();
  if (!fits(np, 14)) return false;
  for (uint32_t i = 0; i < np && !r.bad; i++) {
    Pool p;
    p.site = r.u64(); p.week = r.i32();
    const uint32_t nj = r.u16();
    if (!fits(nj, 11)) return false;
    for (uint32_t k = 0; k < nj && !r.bad; k++) {
      PoolJob j;
      j.qtype = r.u8(); j.need = r.u8(); j.state = std::min<uint8_t>(r.u8(), PJ_DONE); j.giver = r.u64();
      p.jobs.push_back(j);
    }
    B.pools.push_back(std::move(p));
  }
  B.nextLead = r.u32();
  const uint32_t nl = r.u32();
  if (!fits(nl, 37)) return false;
  for (uint32_t i = 0; i < nl && !r.bad; i++) {
    Lead l;
    l.id = r.u32(); l.from = (LeadFrom)std::min<uint8_t>(r.u8(), (uint8_t)LeadFrom::COUNT - 1); l.qtype = r.u8();
    l.state = std::min<uint8_t>(r.u8(), LS_STALE); l.day = r.i32(); l.site = r.u64(); l.who = r.u64();
    l.hasPos = r.u8() != 0; l.gx = r.i32(); l.gy = r.i32(); l.text = r.str();
    B.leads.push_back(std::move(l));
  }
  if (r.bad || r.p != in.size()) return false;
  *this = std::move(B);
  return true;
}

}  // namespace jobs

// ---------------------------------------------------------------- Game: leads
uint32_t Game::addLead(const jobs::Lead& l0, bool announce) {
  if (l0.text.empty()) return 0;
  for (const jobs::Lead& l : jobBook.leads) {
    if (l.state != jobs::LS_OPEN) continue;
    if (l.text == l0.text || (l0.who && l.who == l0.who && l.qtype == l0.qtype)) return l.id;
  }
  jobs::Lead l = l0;
  l.id = jobBook.nextLead++;
  l.day = day;
  l.state = jobs::LS_OPEN;
  jobBook.leads.push_back(l);
  // the newest LEAD_MAX kept: the oldest taken / stale ones go first, then the oldest open ones
  while ((int)jobBook.leads.size() > jobs::LEAD_MAX) {
    auto it = std::find_if(jobBook.leads.begin(), jobBook.leads.end(), [](const jobs::Lead& x) { return x.state != jobs::LS_OPEN; });
    jobBook.leads.erase(it != jobBook.leads.end() ? it : jobBook.leads.begin());
  }
  // (P1 integrator) the notice strip holds two short lines on a phone: a long lead (a teller's overheard hint runs to
  // 120 characters) is cut at a word with "..."; the journal keeps the whole text
  if (!announce) return l.id;
  std::string shown = l.text;
  if (shown.size() > 50) {
    const size_t sp = shown.rfind(' ', 47);
    shown = shown.substr(0, sp == std::string::npos || sp < 20 ? 47 : sp);
    while (!shown.empty() && (shown.back() == ',' || shown.back() == ':' || shown.back() == ' ')) shown.pop_back();
    shown += "...";
  }
  say("NEW LEAD: " + shown);
  return l.id;
}

// ---------------------------------------------------------------- the hooks
namespace {
constexpr Mk MK_JOB_WEEK = (Mk)128;   // a giver outside the settlements (npcKey): the week they last gave a job
}

void JobOps::step(Game& g, float dt) {
  (void)dt;
  for (jobs::Lead& l : g.jobBook.leads)
    if (l.state == jobs::LS_OPEN && g.day - l.day > jobs::LEAD_DAYS) l.state = jobs::LS_STALE;
  const int32_t week = g.day / 7;
  auto& ps = g.jobBook.pools;
  const size_t n0 = ps.size();
  ps.erase(std::remove_if(ps.begin(), ps.end(), [&](const jobs::Pool& p) { return p.week < week - 1; }), ps.end());
  if (ps.size() != n0) g.jobBook.liveDirty = true;
}

void JobOps::accepted(Game& g, const Quest& q) {
  // (P1 fixer) a street errand (helping in an incident) is not the person's job: it neither takes their week's pool
  // job nor closes the leads that name them
  if (q.type == QType::Errand) return;
  if (q.giverSlot < 0 && q.giverBldg < 0) return;
  const uint64_t raw = g.npcKeyOf(q.giverSite, q.giverBldg, q.giverSlot);
  const uint64_t key = questGiverKey(g, q);
  for (jobs::Lead& l : g.jobBook.leads)
    // (a teller's TALE lead closes when its story starts, never on a plain job: Game::storyTalk / JobOps::taleStarted)
    if (l.state == jobs::LS_OPEN && l.from != jobs::LeadFrom::Teller && (l.who == key || l.who == raw) && (l.qtype == (uint8_t)QType::COUNT || l.qtype == (uint8_t)q.type)) l.state = jobs::LS_TAKEN;
  // outside the settlements (a wayside hunter, a hermit): one job a week
  if (q.giverSite >= 0 && q.giverSite < (int)g.world.sites.size() && !g.world.sites[(size_t)q.giverSite].settlement())
    g.marks[markKey(raw, MK_JOB_WEEK)] = g.day / 7;
  // the giver's job this week (its own kind first; a job that came out as another kind - a fallback hunt - still counts)
  jobs::PoolJob* any = nullptr;
  for (jobs::Pool& p : g.jobBook.pools)
    for (jobs::PoolJob& j : p.jobs) {
      if (j.state != jobs::PJ_OPEN || (j.giver != key && j.giver != raw)) continue;
      if (j.qtype == (uint8_t)q.type) { j.state = jobs::PJ_GIVEN; g.jobBook.liveDirty = true; return; }
      if (!any) any = &j;
    }
  if (any) { any->state = jobs::PJ_GIVEN; g.jobBook.liveDirty = true; }
}

void JobOps::questDone(Game& g, const Quest& q) {
  if (q.type == QType::Errand) return;   // (P1 fixer: an errand never held a pool job)
  if (q.giverSlot < 0 && q.giverBldg < 0) return;
  const uint64_t raw = g.npcKeyOf(q.giverSite, q.giverBldg, q.giverSlot);
  const uint64_t key = questGiverKey(g, q);
  for (jobs::Pool& p : g.jobBook.pools)
    for (jobs::PoolJob& j : p.jobs)
      if (j.state == jobs::PJ_GIVEN && (j.giver == key || j.giver == raw)) { j.state = jobs::PJ_DONE; g.jobBook.liveDirty = true; return; }
}

void JobOps::taleStarted(Game& g, uint64_t who) {
  if (!who) return;
  for (jobs::Lead& l : g.jobBook.leads)
    if (l.state == jobs::LS_OPEN && l.from == jobs::LeadFrom::Teller && l.who == who) l.state = jobs::LS_TAKEN;
}

void JobOps::reset(Game& g) { g.jobBook.clear(); }

void JobOps::loaded(Game& g) { g.jobBook.liveDirty = true; g.jobBook.canon.clear(); }

bool JobOps::waysideFree(const Game& g, uint64_t raw) {
  auto it = g.marks.find(markKey(raw, MK_JOB_WEEK));
  return it == g.marks.end() || it->second != g.day / 7;
}
