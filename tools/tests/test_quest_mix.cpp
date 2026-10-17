// rpg_test --quest-mix [--seeds A..B] [--weeks N] [--settle N] [--takes N] [--gates] [--report]
// P1 "Fresh Roads" (lead; VISION_PLAN 15.34): what quests a player is offered across weeks of play, measured with the
// game's own offer code (story::tellerOffer / placeOffer, Game::hasOffer / makeOffer: nothing here re-implements a
// picker). From the survey probe of 2026-10-17 (scratchpad qprobe.cpp), made a permanent test.
//
// Per seed: a fresh endless game; the N settlements nearest the start (default 6); W weeks (default 12). Each week the
// player visits every one of them: every person on the street and in every building is asked what they would offer
// (a story: the tale scan's and storyTalk's choice; work: "DO YOU NEED HELP?"), the notice board is read (its two
// posted jobs and its pinned story), the herald of the nearest capital is heard, the ruins near the settlement are read
// and the news is heard in a village (the places' story hooks). Up to T offers a visit (default 3) are taken and
// finished, as a player making progress would (a campaign offered is always taken when no other runs). Then a week
// passes (g.day += 7; the realm ticks those days on the next step).
//
// The measured numbers (and the 15.34 DONE gates, enforced with --gates):
//   finished tales re-offered    share of story offers that are a library tale already played          gate 0 %
//   top-4 tales                  share of story offers taken by the 4 most offered tales / archetypes  gate <= 25 %
//   campaigns started            distinct generated campaign plans started over all seeds (of 6;      gate >= 5
//                                the library's EMPTY THRONE is reported beside them)
//   top-3 job kinds              share of radiant jobs taken by the 3 most offered QTypes              gate <= 55 %
//   farmers' jobs                share of radiant jobs offered by farmers                              gate <= 30 %
//   hunts without a target       Hunt offers with neither a target site nor a position (no marker)     gate 0
// Survey baseline (20 seeds, 6 settlements, 12 weeks, 3 takes): re-offered 44 %, top-4 68 %, campaigns 3 (analytic),
// top-3 jobs 80 %, farmers 58 %, hunts without a target 100 %.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/sim/game_internal.h"
#include "rpg/story/dsl.h"
#include "rpg/story/saga.h"
#include "rpg/story/story.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

// Game's friend (game.h): the private offer and travel doors this measurement needs
struct QuestMixOps {
  static bool hasOffer(const Game& g, const Actor& a) { return g.hasOffer(a); }
  static Quest makeOffer(Game& g, const Actor& a) { return g.makeOffer(a); }
  static void accept(Game& g, const Quest& q) { g.acceptQuest(q); }
  static bool enter(Game& g, int b) { return g.debugEnterBuilding(b, 0); }
};

namespace {

using namespace story;
constexpr int BOARD_SLOT0 = 960;   // places.cpp: the notice board's two posted jobs (slots 960, 961)

const char* qtName(QType t) {
  static const char* n[] = {"Main", "Clear", "Hunt", "Retrieve", "Bounty", "Deliver", "Heirloom", "Missing", "NamedBandit", "Protect", "Story", "War",
                            "Supply", "Sea", "Campaign", "Envoy", "Fief", "Errand"};
  return (int)t < (int)(sizeof(n) / sizeof(n[0])) ? n[(int)t] : "?";
}

const char* roleTalkName(Role r) {
  static const char* n[] = {"villager", "guard", "merchant", "smith", "innkeeper", "priest", "jarl", "farmer", "child", "mage", "bandit", "king",
                            "hunter", "fisher", "herbalist", "traveller", "herald", "soldier", "captain", "refugee", "harbourmaster",
                            "shipwright", "sailor", "pirate", "steward", "warlord", "envoy"};
  return (int)r < (int)(sizeof(n) / sizeof(n[0])) ? n[(int)r] : "other";
}

struct Mix {
  std::map<std::string, int> story;      // tale key ("lib:<id>" / "saga:<archetype>") -> offers
  std::map<std::string, int> storyFrom;  // where offered (talk / board / herald / ruin / event)
  int storyOffers = 0, replays = 0;
  std::map<std::string, int> jobs;       // QType name -> offers
  std::map<std::string, int> jobRole;    // giver role -> offers
  std::map<std::string, int> titles;     // job title -> offers
  int jobOffers = 0, farmerJobs = 0, huntOffers = 0, huntNoTarget = 0;
  std::map<std::string, int> board;      // the boards' posted jobs by QType
  std::map<std::string, int> campaignsOffered, campaignsStarted;   // plan id -> seeds
  int people = 0, withStory = 0, withWork = 0;
  std::map<std::string, int> visits;     // settlement visits by type
};

std::string taleKey(const Offer& o) {
  if (o.lib >= 0) return "lib:" + o.id();
  saga::Spec s;
  if (!saga::parseSpecId(o.saga, s)) return "saga:?";
  return std::string(s.campaign ? "saga:@" : "saga:") + s.arch;
}
std::string campaignOf(const Offer& o) {
  if (o.lib >= 0) return o.campaign() ? "lib:" + o.id() : std::string();
  saga::Spec s;
  return saga::parseSpecId(o.saga, s) && s.campaign ? s.arch : std::string();
}

// take (start and finish at once) a story offered at hook `kind`; true when it started
bool takeStory(Game& g, const Offer& o, int kind, int actorId, int site) {
  const std::string id = o.id();
  if (id.empty()) return false;
  if (o.lib >= 0) {
    if (kind == (int)dsl::HookKind::Npc) {
      // (as the probe: the library tale counts as played; its teller is marked as having told it)
      g.story.played_[id]++;
      for (const Actor& a : g.actors)
        if (a.id == actorId) g.marks[storyMarkKey(g.npcKey(a), MK_STORY_GIVER)] = g.day;
      return true;
    }
    const uint32_t inst = g.story.start(g, id, -1, site);
    if (!inst) return false;
    for (Instance& in : g.story.running_) if (in.id == inst) in.done = true;
    g.story.played_[id]++;
    return true;
  }
  const uint32_t inst = g.story.start(g, id, kind == (int)dsl::HookKind::Npc ? actorId : -1, site);
  if (!inst) return false;
  saga::Hook hk;
  hk.kind = kind; hk.actor = kind == (int)dsl::HookKind::Npc ? actorId : -1; hk.site = site;
  saga::noteTold(g, id, inst, hk);
  for (Instance& in : g.story.running_) if (in.id == inst) in.done = true;
  g.story.played_[id]++;
  return true;
}

void noteStory(Mix& M, const Offer& o, const char* from) {
  M.storyOffers++;
  M.story[taleKey(o)]++;
  M.storyFrom[from]++;
  if (o.lib >= 0 && o.replay) M.replays++;
}

struct Visit {
  std::set<uint64_t> seen;
  int takes = 0;
};

void considerActor(Game& g, Actor& a, Visit& v, Mix& M, std::set<std::string>& seedCamps, std::set<std::string>& seedStarted) {
  if (!a.npc || !a.human || a.st == AState::Dead || isStoryPerson(a)) return;
  const uint64_t key = g.npcKey(a);
  if (!v.seen.insert(key).second) return;
  M.people++;
  if (a.quest > 0) return;
  // a story (storyTalk's own choice)
  const Offer o = tellerOffer(g, a);
  if (o.any()) {
    M.withStory++;
    noteStory(M, o, "talk");
    const std::string camp = campaignOf(o);
    if (!camp.empty()) seedCamps.insert(camp);
    if (v.takes > 0 && takeStory(g, o, (int)dsl::HookKind::Npc, a.id, homeSiteOf(g, a))) {
      v.takes--;
      if (!camp.empty()) seedStarted.insert(camp);
    }
  }
  // work ("DO YOU NEED HELP?": hasOffer, then makeOffer)
  if (QuestMixOps::hasOffer(g, a)) {
    M.withWork++;
    const Quest q = QuestMixOps::makeOffer(g, a);
    if (q.title.empty()) return;
    M.jobOffers++;
    M.jobs[qtName(q.type)]++;
    M.jobRole[roleTalkName(a.role)]++;
    M.titles[q.title]++;
    if (a.role == Role::Farmer) M.farmerJobs++;
    if (q.type == QType::Hunt) {
      M.huntOffers++;
      if (q.target < 0 && !q.hasPos) M.huntNoTarget++;
    }
    if (v.takes > 0) {
      QuestMixOps::accept(g, q);
      Quest& b = g.quests.back();
      b.state = QState::Done;
      g.npcQuestsDone[key]++;
      v.takes--;
    }
  }
}

// the places' story hooks at settlement `si` this week: the board (towns and cities), the nearest capital's herald,
// the ruins within 160 tiles (their words read), the news in a village
void considerPlaces(Game& g, int si, Visit& v, Mix& M, std::set<std::string>& seedCamps, std::set<std::string>& seedStarted) {
  const Site S = g.world.sites[(size_t)si];
  auto place = [&](int kind, int site, const char* from) {
    const Offer o = placeOffer(g, kind, site);
    if (!o.any()) return;
    noteStory(M, o, from);
    const std::string camp = campaignOf(o);
    if (!camp.empty()) seedCamps.insert(camp);
    // a campaign is always taken (one runs at a time: the next can then come up); other stories while takes last
    if ((!camp.empty() || v.takes > 0) && takeStory(g, o, kind, -1, site)) {
      if (camp.empty()) v.takes--;
      else seedStarted.insert(camp);
    }
  };
  if (S.type == SiteType::Town || S.type == SiteType::City) {
    for (int kk = 0; kk < 2; kk++) {
      Actor A;
      A.npc = true; A.human = true; A.site = si; A.bldg = -1; A.slot = BOARD_SLOT0 + kk; A.name = "THE NOTICE BOARD"; A.role = Role::Guard;
      bool ok = false;
      Quest q = g.offerFor(A, kk == 0 ? QType::Bounty : QType::Hunt, ok);
      if (!ok) q = g.offerFor(A, QType::COUNT, ok);
      if (ok) M.board[qtName(q.type)]++;
    }
    place((int)dsl::HookKind::Board, si, "board");
  }
  if (S.type == SiteType::Village) place((int)dsl::HookKind::Event, si, "event");
  int cap = -1, ruins = 0;
  float bd = 1e30f;
  std::vector<std::pair<float, int>> near;
  for (int k : g.world.nearSites) {
    if (k < 0 || k >= (int)g.world.sites.size()) continue;
    const Site& T = g.world.sites[(size_t)k];
    const float d = std::hypot((float)(T.ex - S.ex), (float)(T.ey - S.ey));
    if (T.capital && d < bd) { bd = d; cap = k; }
    if (T.type == SiteType::Ruin && d < 160.0f) near.push_back({d, k});
  }
  if (cap >= 0 && bd < 400.0f) place((int)dsl::HookKind::Herald, cap, "herald");
  std::sort(near.begin(), near.end());
  for (const auto& n : near) {
    if (ruins++ >= 2) break;
    place((int)dsl::HookKind::Ruin, n.second, "ruin");
  }
}

int questMix(int argc, char** argv) {
  uint64_t A = 1, B = 20;
  int weeks = 12, settle = 6, takesPerVisit = 3;
  bool gates = false, report = false, towns = true;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) {
      if (!parseSeedRange(argv[++i], A, B)) { printf("bad --seeds range '%s'\n", argv[i]); return 2; }
    } else if (!strcmp(argv[i], "--weeks") && i + 1 < argc) weeks = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--settle") && i + 1 < argc) settle = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--takes") && i + 1 < argc) takesPerVisit = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--gates")) gates = true;
    else if (!strcmp(argv[i], "--no-towns")) towns = false;
    else if (!strcmp(argv[i], "--report")) report = true;
  }
  Mix M;
  const auto t0 = std::chrono::steady_clock::now();
  int seeds = 0;
  for (uint64_t seed = A; seed <= B; seed++) {
    g_curSeed = seed;
    seeds++;
    Game g(seed);
    g.newEndlessGame(seed);
    g.mode = Mode::Play; g.godMode = true; g.noWildSpawns = true;
    for (int f = 0; f < 10; f++) { g.update(SIM_DT, Input()); g.events.clear(); }
    // the N settlements nearest the start, from the region plans (two rings round the start's region: the survey's
    // probe read World::nearSites, which at the start holds only the start village, so it measured that one alone)
    std::vector<std::pair<float, ew::Gid>> near;
    const Site st = g.world.sites[(size_t)g.world.startSite];
    const int32_t sx = g.world.ox + st.ex, sy = g.world.oy + st.ey;
    for (int32_t ry = ew::regionOf(sy) - 2; ry <= ew::regionOf(sy) + 2; ry++)
      for (int32_t rx = ew::regionOf(sx) - 2; rx <= ew::regionOf(sx) + 2; rx++) {
        const ew::RegionPlan R = g.world.regionPlan(rx, ry);
        for (const ew::SitePlan& p : R.sites)
          if (p.type == SiteType::Village || p.type == SiteType::Town || p.type == SiteType::City)
            near.push_back({std::hypot((float)(p.ex - sx), (float)(p.ey - sy)), p.id});
      }
    std::sort(near.begin(), near.end());
    if ((int)near.size() > settle) near.resize((size_t)settle);
    // (lead) the nearest town and city too, when the nearest settlements hold none: the opening sends the player to
    // the capital, and boards (a campaign's hook) stand only in towns and cities. --no-towns: villages as they come.
    if (towns)
      for (SiteType t : {SiteType::Town, SiteType::City}) {
        bool have = false;
        for (const auto& nd : near) {
          const int h = g.world.ensureSite(nd.second);
          have |= h >= 0 && g.world.sites[(size_t)h].type == t;
        }
        if (have) continue;
        const int si = g.world.findSiteNear(sx, sy, t, 8);
        if (si >= 0) near.push_back({0.0f, g.world.sites[(size_t)si].id});
      }
    std::set<std::string> seedCamps, seedStarted;
    auto tick = [&](int frames) {
      for (int f = 0; f < frames; f++) {
        g.update(SIM_DT, Input());
        g.events.clear();
        if (g.mode == Mode::Dialogue || g.mode == Mode::Shop || g.mode == Mode::LevelUp) g.mode = Mode::Play;
        if (g.mode == Mode::Dead) g.respawn();
      }
    };
    for (int w = 0; w < weeks; w++) {
      for (const auto& nd : near) {
        const int si = g.world.ensureSite(nd.second);
        if (si < 0) continue;
        g.world.sites[(size_t)si].discovered = true;   // (heard of: the way there is known)
        if (!g.fastTravel(si)) continue;
        tick(6);
        const int here = g.world.siteHandle(nd.second);
        if (here < 0) continue;
        Visit v;
        v.takes = takesPerVisit;
        {
          const SiteType t = g.world.sites[(size_t)here].type;
          M.visits[t == SiteType::Village ? "village" : t == SiteType::Town ? "town" : t == SiteType::City ? "city" : "other"]++;
        }
        for (size_t k = 1; k < g.actors.size(); k++) considerActor(g, g.actors[k], v, M, seedCamps, seedStarted);
        const Site S = g.world.sites[(size_t)here];
        for (int b = S.bldgFirst; b < S.bldgFirst + S.bldgCount; b++) {
          if (!QuestMixOps::enter(g, b)) continue;
          tick(2);
          for (size_t k = 1; k < g.actors.size(); k++) considerActor(g, g.actors[k], v, M, seedCamps, seedStarted);
          g.debugLeave();
          tick(1);
        }
        const int here2 = g.world.siteHandle(nd.second);
        if (here2 >= 0) considerPlaces(g, here2, v, M, seedCamps, seedStarted);
      }
      g.day += 7;
      tick(3);
    }
    for (const std::string& c : seedCamps) M.campaignsOffered[c]++;
    for (const std::string& c : seedStarted) M.campaignsStarted[c]++;
    if (report) {
      const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      printf("  quest-mix seed %llu: campaigns started %zu (%.0f s)\n", (unsigned long long)seed, seedStarted.size(), secs);
    }
  }
  auto top = [](const std::map<std::string, int>& m, int n, std::string* names) {
    std::vector<std::pair<int, std::string>> v;
    for (const auto& kv : m) v.push_back({kv.second, kv.first});
    std::sort(v.rbegin(), v.rend());
    int s = 0;
    for (int i = 0; i < n && i < (int)v.size(); i++) {
      s += v[(size_t)i].first;
      if (names) *names += (i ? ", " : "") + v[(size_t)i].second + " " + std::to_string(v[(size_t)i].first);
    }
    return s;
  };
  auto pct = [](int a, int b) { return b > 0 ? 100.0 * a / b : 0.0; };
  std::string top4n, top3n, campN;
  const int top4 = top(M.story, 4, &top4n), top3 = top(M.jobs, 3, &top3n);
  int started = 0;
  for (const saga::CampaignPlan& P : saga::campaignPlans()) {
    auto it = M.campaignsStarted.find(P.id);
    const int n = it == M.campaignsStarted.end() ? 0 : it->second;
    started += n > 0;
    auto io = M.campaignsOffered.find(P.id);
    campN += std::string(campN.empty() ? "" : ", ") + P.id + " " + std::to_string(n) + "/" + std::to_string(io == M.campaignsOffered.end() ? 0 : io->second);
  }
  const int nPlans = (int)saga::campaignPlans().size();
  {
    // the library's campaigns (tier 3: EMPTY THRONE) are reported beside the generated plans (the gate counts the plans)
    const dsl::Library& L = dsl::library();
    for (const dsl::Script& sc : L.scripts) {
      if (sc.tier != 3) continue;
      const std::string id = "lib:" + sc.id;
      auto it = M.campaignsStarted.find(id);
      const int n = it == M.campaignsStarted.end() ? 0 : it->second;
      auto io = M.campaignsOffered.find(id);
      campN += "; " + id + " " + std::to_string(n) + "/" + std::to_string(io == M.campaignsOffered.end() ? 0 : io->second);
    }
  }
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  printf("quest-mix: seeds %llu..%llu, %d settlements x %d weeks, %d takes a visit (%.0f s)\n", (unsigned long long)A, (unsigned long long)B, settle, weeks,
         takesPerVisit, secs);
  {
    std::string v;
    for (const auto& kv : M.visits) v += (v.empty() ? "" : ", ") + kv.first + " " + std::to_string(kv.second);
    printf("  visits: %s; people asked %d: with a story %d, with work %d\n", v.c_str(), M.people, M.withStory, M.withWork);
  }
  printf("  story offers %d: finished tales re-offered %.1f %% (%d), top-4 tales %.1f %% (%s)\n", M.storyOffers, pct(M.replays, M.storyOffers), M.replays,
         pct(top4, M.storyOffers), top4n.c_str());
  {
    std::string f;
    for (const auto& kv : M.storyFrom) f += (f.empty() ? "" : ", ") + kv.first + " " + std::to_string(kv.second);
    printf("  story offers by hook: %s; distinct tales %zu\n", f.c_str(), M.story.size());
  }
  printf("  campaigns started %d of %d over %d seeds (started/offered seeds: %s)\n", started, nPlans, seeds, campN.c_str());
  printf("  radiant jobs %d: top-3 kinds %.1f %% (%s), farmers %.1f %% (%d), hunts %d without a target %d\n", M.jobOffers, pct(top3, M.jobOffers), top3n.c_str(),
         pct(M.farmerJobs, M.jobOffers), M.farmerJobs, M.huntOffers, M.huntNoTarget);
  if (report) {
    std::string s;
    top(M.jobs, 20, &s);
    printf("  job kinds: %s\n", s.c_str());
    s.clear(); top(M.jobRole, 20, &s);
    printf("  job givers: %s\n", s.c_str());
    s.clear(); top(M.titles, 12, &s);
    printf("  top job titles: %s\n", s.c_str());
    s.clear(); top(M.board, 10, &s);
    printf("  board jobs: %s\n", s.c_str());
    s.clear(); top(M.story, 16, &s);
    printf("  top tales: %s\n", s.c_str());
  }
  int bad = 0;
  if (gates) {
    auto gate = [&](bool ok, const char* what, double have, const char* want) {
      if (!ok) { printf("FAIL: quest-mix %s: %.1f (want %s)\n", what, have, want); bad++; }
    };
    gate(M.replays == 0, "finished tales re-offered %", pct(M.replays, M.storyOffers), "0");
    gate(pct(top4, M.storyOffers) <= 25.0, "top-4 tales %", pct(top4, M.storyOffers), "<= 25");
    gate(started >= 5 && nPlans >= 6, "campaigns started (generated plans)", started, ">= 5 of the 6 plans");
    gate(pct(top3, M.jobOffers) <= 55.0, "top-3 job kinds %", pct(top3, M.jobOffers), "<= 55");
    gate(pct(M.farmerJobs, M.jobOffers) <= 30.0, "farmers' jobs %", pct(M.farmerJobs, M.jobOffers), "<= 30");
    gate(M.huntNoTarget == 0, "hunts without a target", M.huntNoTarget, "0");
    gate(M.storyOffers > 0 && M.jobOffers > 0, "offers seen", M.storyOffers + M.jobOffers, "> 0");
    printf("quest-mix gates: %s\n", bad ? "FAIL" : "ok");
  }
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--quest-mix", "P1: the quests offered over weeks of play [--seeds A..B --weeks 12 --settle 6 --takes 3 --gates --report]", questMix);
