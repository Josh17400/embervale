// P1b "New Roads Content": the radiant jobs' composer (rpg/story/radiant.h). JOBS lane.
//
// Each of the 15 job kinds is a DSL template (rpg/story/jobs/<kind>.cpp, the saga markup). One offer is composed from
// the template with the job's FLAGS (the giver's trade, the season, war, the harbour, the danger, the variant and twist
// the world chose) and its SEED (the [[...]] alternatives and the kind's picks): the text is expanded (saga::expand),
// the header lines prepended, then parsed, linked and validated like any library script. The composed script is named
// by its spec id "job1~<kind>~<flags hex>~<seed hex>" and regenerated from it whenever it is needed (never saved).
//
// The offer (offerFor) reads the giver's weekly pool job (rpg/sim/jobs_pool.cpp: QType::Story, need JN_RADIANT0 +
// kind), lets the kind choose its variant from the world, composes it and TRY-CASTS it with the very key Engine::start
// will use (offerCastKey), so what is pitched is what begins; a kind whose preferred places are not in this world
// (watchtowers, dwellings: ENGINE lane) falls back to its `fb` form, and an un-castable job is not offered at all.
#include "rpg/story/radiant.h"
#include <algorithm>
#include <cstdio>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/home.h"
#include "rpg/sim/jobs_ops.h"
#include "rpg/story/dsl.h"
#include "rpg/story/saga.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/source.h"

namespace story {
namespace radiant {

// ---------------------------------------------------------------- the kinds
const std::vector<const KindDef*>& kinds() {
  static const std::vector<const KindDef*> K = {
      &kindRoadWarden(), &kindFence(), &kindRunDown(), &kindSurvey(), &kindHomeward(),
      &kindHaunt(), &kindRemedy(), &kindSmuggle(), &kindRansom(), &kindRounds(),
      &kindChampion(), &kindBounds(), &kindMend(), &kindFeast(), &kindBeacon()};
  return K;
}
int kindCount() { return (int)kinds().size(); }
const char* kindId(int kind) { return kind >= 0 && kind < kindCount() ? kinds()[(size_t)kind]->id : ""; }
const char* kindTitle(int kind) { return kind >= 0 && kind < kindCount() ? kinds()[(size_t)kind]->title : ""; }

namespace {

const char* tradeWordOf(int role) {
  switch ((Role)role) {
    case Role::Merchant: return "merchant";
    case Role::Innkeeper: return "innkeeper";
    case Role::Jarl: return "jarl";
    case Role::Guard: return "guard";
    case Role::Priest: return "priest";
    case Role::Smith: return "smith";
    case Role::Farmer: return "farmer";
    case Role::Mage: return "mage";
    case Role::Hunter: return "hunter";
    case Role::Fisher: return "fisher";
    case Role::Herbalist: return "herbalist";
    default: return "villager";
  }
}
const char* seasonWordOf(int s) {
  static const char* n[] = {"spring", "summer", "autumn", "winter"};
  return n[(unsigned)s & 3u];
}


struct Parsed { int kind = -1; uint32_t flags = 0, seed = 0; };
bool parseId(const std::string& id, Parsed& p) {
  if (!isJobId(id)) return false;
  // job<ver>~<kind>~<flags>~<seed>
  const size_t a = id.find('~');
  if (a == std::string::npos) return false;
  if (std::atoi(id.substr(3, a - 3).c_str()) != RADIANT_GEN_VER) return false;
  const size_t b = id.find('~', a + 1);
  if (b == std::string::npos) return false;
  const size_t c = id.find('~', b + 1);
  if (c == std::string::npos) return false;
  const std::string kid = id.substr(a + 1, b - a - 1);
  p.kind = -1;
  for (int k = 0; k < kindCount(); k++) if (kid == kinds()[(size_t)k]->id) p.kind = k;
  if (p.kind < 0) return false;
  const std::string fs = id.substr(b + 1, c - b - 1), ss = id.substr(c + 1);
  if (fs.empty() || ss.empty() || fs.size() > 8 || ss.size() > 8) return false;
  for (char ch : fs + ss) if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) return false;
  p.flags = (uint32_t)std::strtoul(fs.c_str(), nullptr, 16);
  p.seed = (uint32_t)std::strtoul(ss.c_str(), nullptr, 16);
  return true;
}

// the composer's block conditions (before the saga markup): a line "{?flag" (or "{!flag", "{?a|b" any of them) opens a
// block kept only when the condition holds; a line "}" closes it. Blocks nest. Everything else passes through.
std::string blocks(const std::string& body, const std::set<std::string>& flags, std::vector<std::string>& errors) {
  std::string out;
  std::vector<bool> keep;   // the open blocks' verdicts
  size_t i = 0;
  while (i <= body.size()) {
    size_t e = body.find('\n', i);
    if (e == std::string::npos) e = body.size();
    const std::string line = body.substr(i, e - i);
    size_t a = line.find_first_not_of(" \t\r");
    const std::string t = a == std::string::npos ? std::string() : line.substr(a);
    const bool on = std::find(keep.begin(), keep.end(), false) == keep.end();
    if (t.size() >= 3 && t[0] == '{' && (t[1] == '?' || t[1] == '!')) {
      std::string cond = t.substr(2);
      while (!cond.empty() && (cond.back() == ' ' || cond.back() == '\r' || cond.back() == '\t')) cond.pop_back();
      bool any = false;
      size_t p = 0;
      while (p <= cond.size()) {
        size_t q = cond.find('|', p);
        if (q == std::string::npos) q = cond.size();
        if (flags.count(cond.substr(p, q - p))) any = true;
        p = q + 1;
      }
      keep.push_back(t[1] == '?' ? any : !any);
    } else if (t == "}") {
      if (keep.empty()) errors.push_back("a block closed that was never opened");
      else keep.pop_back();
    } else if (on) {
      out += line;
      out += '\n';
    }
    if (e >= body.size()) break;
    i = e + 1;
  }
  if (!keep.empty()) errors.push_back("a block was never closed");
  return out;
}

// the whole DSL text of a spec id ("" with errors when it cannot be composed)
std::string composeText(const std::string& id, std::vector<std::string>& errors) {
  Parsed p;
  if (!parseId(id, p)) { errors.push_back("not a job spec id: " + id); return std::string(); }
  const KindDef& K = *kinds()[(size_t)p.kind];
  const JobCtx c = unpackCtx(p.flags);
  saga::Expand x;
  x.rng = ew::mix64(0x6A0B1ull ^ ((uint64_t)p.seed << 20) ^ (uint64_t)p.flags * 0x9E3779B97F4A7C15ull);
  x.flags.insert(std::string("t_") + tradeWordOf(c.trade));
  x.flags.insert(std::string("s_") + seasonWordOf(c.season));
  x.flags.insert(c.war ? "war" : "peace");
  if (c.port) x.flags.insert("port");
  if (c.coast) x.flags.insert("coast");
  if (c.danger) x.flags.insert("danger");
  if (c.fallback) x.flags.insert("fb");
  x.flags.insert(c.tier == 0 ? "village" : c.tier == 1 ? "town" : "city");
  if (c.variant >= 0 && c.variant < (int)K.variants.size()) x.flags.insert(std::string("v_") + K.variants[(size_t)c.variant]);
  else if (!K.variants.empty()) { errors.push_back("variant out of range"); return std::string(); }
  if (!K.twists.empty()) {
    if (c.twist < 0 || c.twist >= (int)K.twists.size()) { errors.push_back("twist out of range"); return std::string(); }
    x.flags.insert(std::string("tw_") + K.twists[(size_t)c.twist]);
  }
  if (!K.extras.empty()) {
    if (c.extra < 0 || c.extra >= (int)K.extras.size()) { errors.push_back("extra out of range"); return std::string(); }
    x.flags.insert(std::string("x_") + K.extras[(size_t)c.extra]);
  }
  if (K.prep) {
    std::map<std::string, std::string> picks;
    K.prep(c, ew::mix64((uint64_t)p.seed * 0x2545F4914F6CDD1Dull ^ p.flags), picks);
    for (const auto& kv : picks) {
      x.picks[kv.first] = kv.second;
      if (!kv.second.empty() && kv.second[0] == '#') x.errors.push_back("a pick may not start with #");
    }
  }
  std::string text = "script " + id + "\n";
  text += std::string("archetype ") + K.title + "\n";
  text += "tier 2\nfamily job\n";
  text += std::string("hook npc ") + tradeWordOf(c.trade) + "\n";
  text += "pitch \"ANY WORK GOING?\"\n";
  text += saga::expand(blocks(K.body, x.flags, errors), x);
  for (const std::string& e : x.errors) errors.push_back(e);
  return text;
}

// the parsed, linked script and its problems
std::unique_ptr<dsl::Script> build(const std::string& id, std::vector<std::string>& errors) {
  const std::string text = composeText(id, errors);
  if (text.empty() || !errors.empty()) return nullptr;
  std::vector<dsl::Script> out;
  dsl::parse(text.c_str(), "radiant", out, errors);
  if (!errors.empty() || out.size() != 1 || out[0].id != id) {
    if (out.size() != 1) errors.push_back("composed " + std::to_string(out.size()) + " scripts");
    return nullptr;
  }
  std::unique_ptr<dsl::Script> s(new dsl::Script(std::move(out[0])));
  dsl::link(*s);
  for (const std::string& v : dsl::validate(*s)) errors.push_back(v);
  if (!errors.empty()) return nullptr;
  return s;
}

// ---------------------------------------------------------------- the cache (least recently used first out)
struct Cache {
  std::map<std::string, std::unique_ptr<dsl::Script>> m;
  std::list<std::string> order;   // most recent at the back
};
Cache& cache() { static Cache c; return c; }
constexpr size_t CACHE_MAX = 384;

}  // namespace

uint32_t packCtx(const JobCtx& c) {
  return (uint32_t)(c.trade & 31) | ((uint32_t)(c.season & 3) << 5) | ((uint32_t)c.war << 7) | ((uint32_t)c.port << 8) |
         ((uint32_t)c.coast << 9) | ((uint32_t)c.danger << 10) | ((uint32_t)c.fallback << 11) | ((uint32_t)(c.variant & 15) << 12) |
         ((uint32_t)(c.twist & 15) << 16) | ((uint32_t)(c.extra & 15) << 20) | ((uint32_t)(c.tier & 3) << 24);
}
JobCtx unpackCtx(uint32_t f) {
  JobCtx c;
  c.trade = (int)(f & 31); c.season = (int)((f >> 5) & 3); c.war = ((f >> 7) & 1) != 0; c.port = ((f >> 8) & 1) != 0;
  c.coast = ((f >> 9) & 1) != 0; c.danger = ((f >> 10) & 1) != 0; c.fallback = ((f >> 11) & 1) != 0;
  c.variant = (int)((f >> 12) & 15); c.twist = (int)((f >> 16) & 15); c.extra = (int)((f >> 20) & 15); c.tier = (int)((f >> 24) & 3);
  return c;
}

std::string specIdOf(int kind, const JobCtx& c, uint32_t seed) {
  if (kind < 0 || kind >= kindCount()) return std::string();
  char buf[64];
  std::snprintf(buf, sizeof buf, "%08x~%08x", (unsigned)packCtx(c), (unsigned)seed);
  return "job" + std::to_string(RADIANT_GEN_VER) + "~" + kinds()[(size_t)kind]->id + "~" + buf;
}

int kindOf(const std::string& id) {
  Parsed p;
  return parseId(id, p) ? p.kind : -1;
}

const dsl::Script* script(const std::string& id) {
  if (!isJobId(id)) return nullptr;
  Cache& C = cache();
  auto it = C.m.find(id);
  if (it != C.m.end()) {
    // (touched: the most recent; a pointer handed out a moment ago is never the one evicted)
    for (auto o = C.order.begin(); o != C.order.end(); ++o) if (*o == id) { C.order.erase(o); break; }
    C.order.push_back(id);
    return it->second.get();
  }
  std::vector<std::string> errs;
  std::unique_ptr<dsl::Script> s = build(id, errs);
  const dsl::Script* p = s.get();
  C.m[id] = std::move(s);   // (a failure is cached as nullptr too)
  C.order.push_back(id);
  while (C.order.size() > CACHE_MAX) {
    C.m.erase(C.order.front());
    C.order.pop_front();
  }
  return p;
}

std::vector<std::string> composeErrors(const std::string& id) {
  std::vector<std::string> errs;
  build(id, errs);
  return errs;
}
std::string composedText(const std::string& id) {
  std::vector<std::string> errs;
  return composeText(id, errs);
}

// ---------------------------------------------------------------- the world at a hook
namespace {

JobCtx worldAt(Game& g, int site, int trade) {
  JobCtx w;
  w.trade = trade;
  w.season = (int)home::seasonOf(g.day);
  if (site < 0 || site >= (int)g.world.sites.size()) return w;
  const Site S = g.world.sites[(size_t)site];
  w.tier = S.type == SiteType::City ? 2 : S.type == SiteType::Town ? 1 : 0;
  w.port = S.archetype == (uint8_t)ew::Archetype::Port;
  const int32_t hx = g.world.ox + S.ex, hy = g.world.oy + S.ey;
  w.coast = w.port || nearCoast(g, hx, hy);
  const ew::Gid land = landAt(g, hx, hy);
  if (land)
    for (const realm::War& wr : g.realm.wars())
      if (!wr.endDay && (wr.attacker == land || wr.defender == land)) w.war = true;
  w.danger = S.level >= g.plLevel + 3;
  return w;
}

// compose and try-cast (the giver `A` or none, at `site`); the fallback when the kind has one. "" none
std::string composeCast(Game& g, int kind, JobCtx c, uint32_t seed, const Actor* A, int site) {
  const KindDef& K = *kinds()[(size_t)kind];
  for (int pass = 0; pass < 2; pass++) {
    if (pass == 1) { if (!K.fallback || c.fallback) break; c.fallback = true; }
    const std::string id = specIdOf(kind, c, seed);
    const dsl::Script* s = script(id);
    if (!s) continue;
    std::vector<Binding> cast;
    std::string why;
    if (castScript(g, *s, A ? A->id : -1, site, offerCastKey(g, *s, A, site), cast, why)) return id;
  }
  return std::string();
}

}  // namespace

std::string offerFor(Game& g, const Actor& a) {
  if (!a.npc || !a.human || a.site < 0 || a.site >= (int)g.world.sites.size()) return std::string();
  const jobs::PoolJob* j = JobOps::openJob(g, a);
  if (!j || j->qtype != (uint8_t)QType::Story || j->need < jobs::JN_RADIANT0 || j->need >= jobs::JN_RADIANT0 + kindCount()) return std::string();
  if (JobOps::lifeOffer(g, a)) return std::string();   // (an unmet need asks first, as in makeOffer)
  const int kind = j->need - jobs::JN_RADIANT0;
  const int site = a.site;
  const KindDef& K = *kinds()[(size_t)kind];
  const JobCtx w = worldAt(g, site, (int)a.role);
  const uint64_t giver = JobOps::giverKey(g, a);
  const uint64_t dice = ew::mix64(giver ^ (uint64_t)(uint32_t)(g.day / 7) * 0x9E3779B97F4A7C15ull ^ ((uint64_t)kind << 40) ^ g.world.sites[(size_t)site].id);
  JobCtx c;
  if (!K.choose || !K.choose(w, dice, c)) return std::string();
  c.trade = w.trade; c.season = w.season; c.war = w.war; c.port = w.port; c.coast = w.coast; c.danger = c.danger || w.danger; c.tier = w.tier;
  return composeCast(g, kind, c, (uint32_t)(ew::mix64(dice ^ 0x5EEDull) & 0xFFFFFFFFu), &a, site);
}

std::string composeFor(Game& g, int kind, const Actor* giver, int hookSite, uint64_t seed) {
  if (kind < 0 || kind >= kindCount()) return std::string();
  const KindDef& K = *kinds()[(size_t)kind];
  if (hookSite < 0 && giver) hookSite = homeSiteOf(g, *giver);
  if (hookSite < 0 || hookSite >= (int)g.world.sites.size()) return std::string();
  // the giver's trade, or the first of the kind's trades that fits this world
  std::vector<int> trades;
  if (giver) trades.push_back((int)giver->role);
  else trades = K.trades;
  for (int t : trades) {
    const JobCtx w = worldAt(g, hookSite, t);
    JobCtx c;
    if (!K.choose || !K.choose(w, seed, c)) continue;
    c.trade = w.trade; c.season = w.season; c.war = w.war; c.port = w.port; c.coast = w.coast; c.danger = c.danger || w.danger; c.tier = w.tier;
    const std::string id = composeCast(g, kind, c, (uint32_t)(ew::mix64(seed ^ 0x5EEDull) & 0xFFFFFFFFu), giver, hookSite);
    if (!id.empty()) return id;
  }
  return std::string();
}

int variantCount(int kind) { return kind >= 0 && kind < kindCount() ? (int)kinds()[(size_t)kind]->walks.size() : 0; }

std::string composeVariant(Game& g, int kind, int variant, int hookSite, uint64_t seed) {
  if (kind < 0 || kind >= kindCount()) return std::string();
  const KindDef& K = *kinds()[(size_t)kind];
  if (variant < 0 || variant >= (int)K.walks.size()) return std::string();
  const uint32_t s = (uint32_t)(ew::mix64(seed * 0x9E3779B97F4A7C15ull + (uint64_t)kind) & 0xFFFFFFFFu);
  return composeCast(g, kind, K.walks[(size_t)variant], s, nullptr, hookSite);
}

}  // namespace radiant
}  // namespace story

// ---------------------------------------------------------------- the pools' bookkeeping (JobOps, rpg/sim/jobs_ops.h)
// A radiant job runs as a story Instance, not a Quest the pool hooks see: a job past its opening (committed, not a
// declined pitch) gives the giver's pool job out; when its instance has ended (or is gone) the pool job is done.
void JobOps::radiantStep(Game& g) {
  using namespace story;
  bool any = false;
  for (const jobs::Pool& p : g.jobBook.pools)
    for (const jobs::PoolJob& j : p.jobs) if (j.qtype == (uint8_t)QType::Story && j.state != jobs::PJ_DONE) any = true;
  if (!any) return;
  const int32_t week = g.day / 7;
  for (jobs::Pool& p : g.jobBook.pools)
    for (jobs::PoolJob& j : p.jobs) {
      if (j.qtype != (uint8_t)QType::Story || j.need < jobs::JN_RADIANT0 || j.state == jobs::PJ_DONE) continue;
      const int kind = j.need - jobs::JN_RADIANT0;
      // the running job of this kind whose giver is this pool giver (their body's key, or its census identity)
      const Instance* live = nullptr;
      for (const Instance& in : g.story.running()) {
        if (in.cast.empty() || radiant::kindOf(in.script) != kind) continue;
        const uint64_t raw = in.cast[0].id;
        auto it = g.jobBook.canon.find(raw);
        const uint64_t key = it != g.jobBook.canon.end() ? it->second : raw;
        if (key == j.giver || raw == j.giver) { live = &in; break; }
      }
      if (j.state == jobs::PJ_OPEN) {
        if (live && !live->vars.count("_quiet") && (!live->done || !live->failed || live->questId)) {
          j.state = live->done ? jobs::PJ_DONE : jobs::PJ_GIVEN;
          g.jobBook.liveDirty = true;
          // the leads that named this giver's work are taken with it (as JobOps::accepted does for a quest)
          for (jobs::Lead& l : g.jobBook.leads)
            if (l.state == jobs::LS_OPEN && l.from != jobs::LeadFrom::Teller && l.who == j.giver &&
                (l.qtype == (uint8_t)QType::Story || l.qtype == (uint8_t)QType::COUNT))
              l.state = jobs::LS_TAKEN;
        }
      } else if (j.state == jobs::PJ_GIVEN) {
        if (!live || live->done) { j.state = jobs::PJ_DONE; g.jobBook.liveDirty = true; }
      }
      (void)week;
    }
}
