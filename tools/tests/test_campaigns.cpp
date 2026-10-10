// rpg_test --campaigns [--seeds A..B] [--verbose] [--sample] [--print <campaign> [arc ...] [vN] [sN]]: M6b campaigns
// (rpg/story/chain.cpp, rpg/story/campaigns/*.cpp). CAMPAIGNS lane.
//   - the plans: 5 (6 with emptythrone), 4-8 arc slots of >= 2 choices, every choice a tier-3 arc of the library; every
//     plan composes VALID (parser + M4 validator, <= 16 roles) with EVERY combination of its arc choices, in every voice
//     family, for several motives and seeds, and composing is a pure function of the spec (twice the same text)
//   - rewards inside the M6 bands (no `do gold` over 40, no `do loot`; success endings pay through `reward`); every
//     plan has world-changing endings (realm effects) and 10-30 stages a player passes on a way through
//   - on real worlds (seeds A..B, default 1..5) each campaign is cast at its hook (a herald in a capital, a notice board,
//     a ruin, a fresh realm event the test forces: a famine, a beast raid) with a covering set of arc choices, and walked
//     headlessly: every reachable (stage, vars) state once (memoised: campaign paths explode), EVERY stage visited and
//     EVERY ending reached, no unfilled placeholder or lower case in any text the player reads, and every realm effect
//     changes the realm (succession: the story's ruler; war / peace: the realm's relation; famine: the settlement)
//   - pickCampaign offers a campaign at a matching hook (and never one that was already told in this world)
//   --sample prints one way through each campaign (the cast filled in) for the writing review; --print a composition.
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
#include "rpg/sim/foes.h"
#include "rpg/story/dsl.h"
#include "rpg/story/saga.h"
#include "rpg/story/story.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/ids.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

using namespace story::saga;
namespace dsl = story::dsl;

bool g_cverbose = false;

struct Snap {
  story::Engine st;
  realm::Realm rm;
  int gold = 0, plXp = 0, plLevel = 1, nextQuestId = 1, trackedQuest = -1, day = 1;
  std::vector<Item> inv;
  std::map<uint64_t, int32_t> marks;
  std::vector<Quest> quests;
};
Snap snap(const Game& g) {
  Snap s;
  s.st = g.story; s.rm = g.realm; s.gold = g.gold; s.plXp = g.plXp; s.plLevel = g.plLevel; s.nextQuestId = g.nextQuestId;
  s.trackedQuest = g.trackedQuest; s.inv = g.inv; s.marks = g.marks; s.quests = g.quests; s.day = g.day;
  return s;
}
void restore(Game& g, const Snap& s) {
  g.story = s.st; g.realm = s.rm; g.gold = s.gold; g.plXp = s.plXp; g.plLevel = s.plLevel; g.nextQuestId = s.nextQuestId;
  g.trackedQuest = s.trackedQuest; g.inv = s.inv; g.marks = s.marks; g.quests = s.quests; g.day = s.day;
  g.events.clear();
  g.mode = Mode::Play;
}

bool readable(const std::string& t) {
  for (char c : t) if (c >= 'a' && c <= 'z') return false;
  return t.find('{') == std::string::npos && t.find('}') == std::string::npos;
}

int hookKindOf(const CampaignPlan& P, int& ev) {
  std::vector<dsl::Script> v;
  std::vector<std::string> err;
  ev = -1;
  // (the head alone parses as a script fragment once a script line is put before it)
  dsl::parse((std::string("script x\n") + P.head).c_str(), "plan", v, err);
  if (v.empty()) return -1;
  ev = v[0].hookEv;
  return (int)v[0].hook;
}

// ---------------------------------------------------------------- the library checks
int libraryChecks(int& compositions) {
  int bad = 0;
  auto fail = [&](const std::string& m) { printf("FAIL: campaigns: %s\n", m.c_str()); bad++; };
  const std::vector<CampaignPlan>& plans = campaignPlans();
  if (plans.size() < 5) fail("fewer than 5 campaign plans (" + std::to_string(plans.size()) + ")");
  std::set<std::string> ids;
  for (const CampaignPlan& P : plans) {
    const std::string id = P.id;
    if (!ids.insert(id).second) fail("two plans are called " + id);
    if (dsl::library().find(id) >= 0) fail("plan " + id + " shares its id with a library script");
    if (P.arcs.size() < 4 || P.arcs.size() > 8) fail(id + ": " + std::to_string(P.arcs.size()) + " arc slots (4..8)");
    for (size_t i = 0; i < P.arcs.size(); i++) {
      int real = 0;
      for (const std::string& c : P.arcs[i].choices) {
        if (c.empty()) continue;
        real++;
        const Archetype* a = archetype(c);
        if (!a) fail(id + ": slot " + std::to_string(i + 1) + " names unknown arc '" + c + "'");
        else if (a->tier != 3) fail(id + ": arc '" + c + "' is not tier 3");
      }
      if (P.arcs[i].choices.size() < 2 || real < 1) fail(id + ": slot " + std::to_string(i + 1) + " offers fewer than 2 choices");
    }
    int ev = -1;
    const int hk = hookKindOf(P, ev);
    if (hk != (int)dsl::HookKind::Board && hk != (int)dsl::HookKind::Herald && hk != (int)dsl::HookKind::Ruin && hk != (int)dsl::HookKind::Event)
      fail(id + ": a campaign is stumbled into at a board, a herald, a ruin or an event (not an npc)");
    // every combination of arc choices, every voice, a few motives and seeds
    size_t combos = 1;
    for (const ArcSlot& sl : P.arcs) combos *= std::max<size_t>(1, sl.choices.size());
    int planFails = 0, minStages = 1 << 30, maxStages = 0, words = 0;
    bool realmEnd = false, successReward = true;
    for (size_t c = 0; c < combos; c++) {
      Spec s;
      s.campaign = true;
      s.arch = P.id;
      size_t k = c;
      for (const ArcSlot& sl : P.arcs) { s.arcs.push_back(sl.choices[k % sl.choices.size()]); k /= sl.choices.size(); }
      for (int v = 0; v < 8; v++)
        for (uint32_t sd = 1; sd <= 2; sd++) {
          s.voice = v;
          s.motive = (Motive)((c * 3 + (size_t)v + sd) % (size_t)Motive::COUNT);
          s.seed = sd * 2654435761u + (uint32_t)c * 97u + (uint32_t)v;
          Composed cm, cm2;
          compositions++;
          const bool ok = composeCampaign(s, cm);
          if (!ok) {
            if (++planFails <= 3) {
              printf("FAIL: campaigns: %s does not compose:\n", specId(s).c_str());
              for (size_t e = 0; e < cm.errors.size() && e < 8; e++) printf("    %s\n", cm.errors[e].c_str());
            }
            bad++;
            continue;
          }
          composeCampaign(s, cm2);
          if (cm.text != cm2.text) { fail(specId(s) + " composes differently twice"); }
          if (v != 0 || sd != 1) continue;
          // the static checks, once per combination
          std::vector<dsl::Script> sv;
          std::vector<std::string> err;
          dsl::parse(cm.text.c_str(), "chk", sv, err);
          if (sv.size() != 1) continue;
          const dsl::Script& sc = sv[0];
          minStages = std::min(minStages, (int)sc.stages.size());
          maxStages = std::max(maxStages, (int)sc.stages.size());
          words = std::max(words, cm.words);
          for (const dsl::StageDef& G : sc.stages) {
            bool reward = false;
            for (const dsl::EffDef& e : G.effs) {
              if (e.type == dsl::EffType::Loot) fail(specId(s) + ": `do loot` in a campaign (use reward)");
              if (e.type == dsl::EffType::Gold && e.n > 40) fail(specId(s) + ": `do gold " + std::to_string(e.n) + "` (over 40: use reward)");
              if (e.type == dsl::EffType::Reward) reward = true;
              if (G.kind == dsl::StageKind::End && e.type >= dsl::EffType::RealmWar && e.type <= dsl::EffType::RealmEvent) realmEnd = true;
            }
            if (G.kind == dsl::StageKind::End && G.success && !reward) { successReward = false; if (g_cverbose) printf("  (no reward at %s)\n", G.name.c_str()); }
          }
        }
    }
    if (!realmEnd) fail(id + ": no ending changes the realm (succession, war, peace, famine or a realm event)");
    if (!successReward) fail(id + ": a success ending pays nothing (end with `do reward`)");
    printf("  campaign %-10s %-30s %zu arc slots, %3zu combinations x 16 composed, %d-%d stages, %d words\n", P.id, P.name, P.arcs.size(),
           combos, minStages == (1 << 30) ? 0 : minStages, maxStages, words);
  }
  return bad;
}

// ---------------------------------------------------------------- the world walk
int32_t pgx(const Game& g) { return g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE); }
int32_t pgy(const Game& g) { return g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE); }

// the hook place of a campaign on this world, with the world facts it needs forced there
int hookFor(Game& g, const dsl::Script& s) {
  const int32_t gx = pgx(g), gy = pgy(g);
  int site = -1;
  switch (s.hook) {
    case dsl::HookKind::Herald: {
      const ew::Gid land = story::landAt(g, gx, gy);
      if (const realm::KingdomState* K = g.realm.kingdom(land)) site = story::siteByIdLoad(g, K->capital);
      if (site < 0) site = g.world.findSiteNear(gx, gy, SiteType::City, 4, true);
      break;
    }
    case dsl::HookKind::Ruin: {
      // the nearest ruin that lies in a kingdom
      for (int r = 2; r <= 6 && site < 0; r++) {
        const int h = g.world.findSiteNear(gx, gy, SiteType::Ruin, r);
        if (h >= 0) {
          const Site& S = g.world.sites[(size_t)h];
          if (story::landAt(g, g.world.ox + S.ex, g.world.oy + S.ey)) site = h;
        }
      }
      if (site < 0) site = g.world.findSiteNear(gx, gy, SiteType::Ruin, 3);
      break;
    }
    case dsl::HookKind::Board: {
      site = g.world.findSiteNear(gx, gy, SiteType::Town, 3);
      if (site < 0) site = g.world.findSiteNear(gx, gy, SiteType::City, 4);
      // (fixer M6b r2) a board campaign needs a kingdom (plans declare N_KINGDOM): the nearest known market town or city
      // that lies in one, not a wildlands town (seed 27)
      if (site >= 0) {
        const Site& S0 = g.world.sites[(size_t)site];
        if (!story::landAt(g, g.world.ox + S0.ex, g.world.oy + S0.ey)) {
          int64_t best = -1;
          for (size_t i = 0; i < g.world.sites.size(); i++) {
            const Site& S = g.world.sites[i];
            if (S.type != SiteType::Town && S.type != SiteType::City) continue;
            const int32_t sx = g.world.ox + S.ex, sy = g.world.oy + S.ey;
            if (!story::landAt(g, sx, sy)) continue;
            const int64_t dx = sx - gx, dy = sy - gy, d2 = dx * dx + dy * dy;
            if (best < 0 || d2 < best) { best = d2; site = (int)i; }
          }
        }
      }
      break;
    }
    default: site = g.world.startSite; break;
  }
  if (site < 0 || s.hook != dsl::HookKind::Event) return site;
  const Site S = g.world.sites[(size_t)site];
  const int32_t hx = g.world.ox + S.ex, hy = g.world.oy + S.ey;
  const ew::Gid land = story::landAt(g, hx, hy);
  const ew::Gid home = S.homeKingdom >= 0 ? g.world.kingdoms[(size_t)S.homeKingdom].id : 0;
  g.realm.noteSite(S.id, home, (uint8_t)S.type, hx, hy);
  if (s.hookEv == (int)realm::EvType::Famine) {
    g.realm.forceFamine(S.id, g.day);
    g.realm.forceEvent(realm::EvType::Famine, land, 0, S.id, hx, hy, g.day);
  } else {
    g.realm.forceEvent((realm::EvType)s.hookEv, land, 0, S.id, hx, hy, g.day);
  }
  return site;
}

int hookActorOf(Game& g, const dsl::Script& s, int site) {
  if (s.hook != dsl::HookKind::Npc && s.hook != dsl::HookKind::Herald) return -1;
  for (const Actor& a : g.actors) {
    if (!a.npc || !a.human || story::isStoryPerson(a) || a.role == Role::Child) continue;
    if (story::homeSiteOf(g, a) != site) continue;
    if (s.hook == dsl::HookKind::Herald ? a.role == Role::Herald : (s.hookTrade < 0 || (int)a.role == s.hookTrade)) return a.id;
  }
  return -1;
}

struct Walk {
  int states = 0, bad = 0, minDepth = 1 << 30, maxDepth = 0;
  std::set<std::string> stages, ends, seen;
  std::map<std::string, int> effectsChecked;
};

std::string stateKey(const story::Instance& in, const std::string& st) {
  std::string k = st;
  for (const auto& kv : in.vars) k += "|" + kv.first + "=" + std::to_string(kv.second);
  return k;
}

// the realm effects of the stage just entered took hold
void checkEffects(Game& g, const story::Instance& in, const dsl::Script& sc, const std::string& name, Walk& w) {
  const int si = sc.stage(g.story.stageName(in));
  if (si < 0) return;
  auto kid = [&](const std::string& r) -> ew::Gid { const story::Binding* b = story::bindingOf(in, r); return b ? b->id : 0; };
  for (const dsl::EffDef& e : sc.stages[(size_t)si].effs) {
    switch (e.type) {
      case dsl::EffType::RealmSuccession: {
        std::string rn, rt;
        const story::Binding* heir = story::bindingOf(in, e.args[1]);
        if (!heir || !g.story.rulerOf(g, kid(e.args[0]), rn, rt) || rn != heir->name) {
          out("FAIL: campaign %s stage %s: realm succession did not crown %s (ruler: %s)\n", name.c_str(), sc.stages[(size_t)si].name.c_str(),
              heir ? heir->name.c_str() : "?", rn.c_str());
          w.bad++;
        }
        w.effectsChecked["succession"]++;
        break;
      }
      case dsl::EffType::RealmWar:
        if (!g.realm.atWar(kid(e.args[0]), kid(e.args[1]))) { out("FAIL: campaign %s: realm war did not begin a war\n", name.c_str()); w.bad++; }
        w.effectsChecked["war"]++;
        break;
      case dsl::EffType::RealmPeace:
        if (g.realm.atWar(kid(e.args[0]), kid(e.args[1]))) { out("FAIL: campaign %s: realm peace left the kingdoms at war\n", name.c_str()); w.bad++; }
        w.effectsChecked["peace"]++;
        break;
      case dsl::EffType::RealmFamine: {
        const story::Binding* b = story::bindingOf(in, e.args[0]);
        const realm::SettlementState* st = b ? g.realm.settlement(b->site ? b->site : b->id) : nullptr;
        if (!st || !(st->flags & realm::SS_FAMINE)) { out("FAIL: campaign %s: realm famine did not starve %s\n", name.c_str(), b ? b->name.c_str() : "?"); w.bad++; }
        w.effectsChecked["famine"]++;
        break;
      }
      case dsl::EffType::RealmEvent: {
        bool found = false;
        for (const realm::WorldEvent& ev : g.realm.events()) if ((int)ev.type == e.ev && (int)ev.day == g.day) found = true;
        if (!found) { out("FAIL: campaign %s: realm event %d was not recorded\n", name.c_str(), e.ev); w.bad++; }
        w.effectsChecked[std::string("event ") + realm::evTypeName((realm::EvType)e.ev)]++;
        break;
      }
      default: break;
    }
  }
}

void walk(Game& g, uint32_t id, const dsl::Script& sc, const std::string& name, Walk& w, int depth) {
  story::Instance* in = g.story.find(id);
  if (!in) { out("FAIL: campaign %s: the instance vanished\n", name.c_str()); w.bad++; return; }
  const std::string st = g.story.stageName(*in);
  const std::string key = stateKey(*in, st);
  if (!w.seen.insert(key).second) return;   // this (stage, vars) state was walked already
  w.states++;
  w.stages.insert(st);
  for (const std::string& t : g.story.stageTexts(g, *in))
    if (!readable(t)) { out("FAIL: campaign %s stage %s: unreadable text: %s\n", name.c_str(), st.c_str(), t.c_str()); w.bad++; }
  checkEffects(g, *in, sc, name, w);
  if (in->done || g.story.isEnd(*in)) {
    w.ends.insert(st);
    w.minDepth = std::min(w.minDepth, depth);
    w.maxDepth = std::max(w.maxDepth, depth);
    bool closed = false;
    for (const Quest& q : g.quests) if (q.id == in->questId && q.type == QType::Story && q.state == QState::Done) closed = true;
    // (fixer M6b r2) turned down at the opening: declined, the journal keeps no entry
    if (!in->questId && in->failed) closed = true;
    if (!closed) { out("FAIL: campaign %s ended at %s but its journal quest is not done\n", name.c_str(), st.c_str()); w.bad++; }
    return;
  }
  if (depth > 120) { out("FAIL: campaign %s: a path deeper than 120 stages at %s\n", name.c_str(), st.c_str()); w.bad++; return; }
  const Snap s0 = snap(g);
  if (g.story.isGoal(*in)) {
    if (!g.story.debugComplete(g, id)) { out("FAIL: campaign %s stage %s: the objective cannot be completed\n", name.c_str(), st.c_str()); w.bad++; }
    else walk(g, id, sc, name, w, depth + 1);
    restore(g, s0);
    return;
  }
  const int n = g.story.optionCount(*in);
  if (n <= 0) { out("FAIL: campaign %s stage %s: a dialogue with no options\n", name.c_str(), st.c_str()); w.bad++; return; }
  std::vector<bool> checks((size_t)n);   // (read now: restoring the engine below frees `in`)
  for (int i = 0; i < n; i++) checks[(size_t)i] = g.story.optionIsCheck(*in, i);
  for (int i = 0; i < n; i++) {
    const bool check = checks[(size_t)i];
    for (int c = check ? 1 : 0; c <= (check ? 2 : 0); c++) {
      restore(g, s0);
      if (!g.story.choose(g, id, i, true, c)) { out("FAIL: campaign %s stage %s: option %d cannot be chosen\n", name.c_str(), st.c_str(), i); w.bad++; continue; }
      walk(g, id, sc, name, w, depth + 1);
    }
  }
  restore(g, s0);
}

// one way through (option `pick` rotating), as the player reads it
void transcript(Game& g, uint32_t id, uint32_t pick) {
  for (int guard = 0; guard < 90; guard++) {
    story::Instance* in = g.story.find(id);
    if (!in) return;
    const std::string st = g.story.stageName(*in);
    const std::string say = g.story.sayText(g, *in), jr = g.story.journalText(g, *in);
    printf("   [%s]%s%s\n", st.c_str(), say.empty() ? "" : " \"", say.empty() ? "" : (say + "\"").c_str());
    if (!jr.empty()) printf("      journal: %s\n", jr.c_str());
    if (in->done || g.story.isEnd(*in)) return;
    if (g.story.isGoal(*in)) { g.story.debugComplete(g, id); continue; }
    const dsl::Script* s = story::scriptOf(*in);
    if (!s) return;
    const auto& opts = s->stages[(size_t)in->stage].opts;
    for (const dsl::OptDef& o : opts) printf("      > %s\n", story::fillText(g, *in, o.label).c_str());
    pick = pick * 1103515245u + 12345u;
    const int k = opts.empty() ? 0 : (int)((pick >> 16) % opts.size());
    g.story.choose(g, id, k, true, 1);
  }
}

int campaignsCmd(int argc, char** argv) {
  uint64_t A = 1, B = 5;
  bool sample = false;
  std::vector<std::string> printSpec;
  for (int i = 2; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], A, B);
    else if (!strcmp(argv[i], "--verbose")) g_cverbose = true;
    else if (!strcmp(argv[i], "--sample")) sample = true;
    else if (!strcmp(argv[i], "--print")) { while (i + 1 < argc && argv[i + 1][0] != '-') printSpec.push_back(argv[++i]); }
  }
  if (!printSpec.empty()) {
    Spec s;
    s.campaign = true;
    s.arch = printSpec[0];
    s.seed = 1;
    for (size_t k = 1; k < printSpec.size(); k++) {
      const std::string& w = printSpec[k];
      if (w.size() >= 2 && w[0] == 'v' && w[1] >= '0' && w[1] <= '9') s.voice = atoi(w.c_str() + 1);
      else if (w.size() >= 2 && w[0] == 's' && w[1] >= '0' && w[1] <= '9') s.seed = (uint32_t)strtoul(w.c_str() + 1, nullptr, 10);
      else s.arcs.push_back(w);
    }
    if (s.arcs.empty())
      if (const CampaignPlan* P = campaignPlan(s.arch)) for (const ArcSlot& sl : P->arcs) s.arcs.push_back(sl.choices[0]);
    Composed c;
    composeCampaign(s, c);
    printf("%s\n", c.text.c_str());
    for (const std::string& e : c.errors) printf("ERROR: %s\n", e.c_str());
    return c.errors.empty() ? 0 : 1;
  }
  auto t0 = std::chrono::steady_clock::now();
  int compositions = 0;
  printf("campaigns: %zu plans (+ emptythrone in the library)\n", campaignPlans().size());
  int bad = libraryChecks(compositions);
  const double libMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  printf("campaigns: %d compositions checked in %.0f ms\n", compositions, libMs);

  // ---- every plan on real worlds: a covering set of arc choices (each choice of each slot at least once per seed)
  std::map<std::string, std::set<std::string>> stagesAll, endsAll, stagesWant, endsWant;
  std::map<std::string, std::map<std::string, int>> effectsAll;
  std::map<std::string, int> casts;
  int walks = 0, states = 0, picks = 0;
  for (uint64_t seed = A; seed <= B; seed++) {
    g_curSeed = seed;
    Game g(seed);
    g.newEndlessGame(seed);
    g.mode = Mode::Play;
    g.godMode = true;
    g.noWildSpawns = true;
    for (int i = 0; i < 90; i++) { g.update(SIM_DT, Input()); g.events.clear(); }
    int seedBad = 0;
    for (const CampaignPlan& P : campaignPlans()) {
      size_t maxChoices = 0;
      for (const ArcSlot& sl : P.arcs) maxChoices = std::max(maxChoices, sl.choices.size());
      for (size_t k = 0; k < maxChoices; k++) {
        Spec s;
        s.campaign = true;
        s.arch = P.id;
        for (size_t j = 0; j < P.arcs.size(); j++) {
          const size_t n = P.arcs[j].choices.size();
          s.arcs.push_back(P.arcs[j].choices[(k + (size_t)seed * (j + 1)) % n]);
        }
        s.voice = (int)((seed + k) % 8);
        s.motive = (Motive)((seed * 3 + k) % (uint64_t)Motive::COUNT);
        s.seed = (uint32_t)(seed * 7919u + k * 104729u);
        const std::string id = specId(s);
        const dsl::Script* sc = script(id);
        if (!sc) { out("FAIL: campaign %s does not compose\n", id.c_str()); seedBad++; continue; }
        for (const dsl::StageDef& G : sc->stages) {
          // (the composition's own stage names: arcs are prefixed by slot, so a name stands for (slot, arc, stage))
          stagesWant[P.id].insert(G.name);
          if (G.kind == dsl::StageKind::End) endsWant[P.id].insert(G.name);
        }
        const Snap s0 = snap(g);
        const int site = hookFor(g, *sc);
        const int actor = site >= 0 ? hookActorOf(g, *sc, site) : -1;
        std::string why;
        const uint32_t inst = site >= 0 ? g.story.start(g, id, actor, site, &why) : 0;
        if (!inst) { out("FAIL: campaign %s cannot be cast on seed %llu: %s\n", id.c_str(), (unsigned long long)seed, why.c_str()); seedBad++; restore(g, s0); continue; }
        casts[P.id]++;
        if (sample && seed == A) {
          const story::Instance* in = g.story.find(inst);
          printf("--- %s  \"%s\"  hook %s\n", id.c_str(), sc->title.c_str(), in && !in->cast.empty() ? in->cast[0].name.c_str() : "?");
          if (in) for (const story::Binding& b : in->cast) printf("      cast %-12s %s\n", b.role.c_str(), b.name.c_str());
          const Snap s1 = snap(g);
          transcript(g, inst, (uint32_t)(seed * 31 + k));
          restore(g, s1);
        }
        Walk w;
        walk(g, inst, *sc, id, w, 0);
        walks++;
        states += w.states;
        seedBad += w.bad;
        for (const std::string& x : w.stages) stagesAll[P.id].insert(x);
        for (const std::string& x : w.ends) endsAll[P.id].insert(x);
        for (const auto& kv : w.effectsChecked) effectsAll[P.id][kv.first] += kv.second;
        // every stage and every ending of THIS composition was reached
        for (const dsl::StageDef& G : sc->stages)
          if (!w.stages.count(G.name)) { out("FAIL: campaign %s: stage %s was never reached\n", id.c_str(), G.name.c_str()); seedBad++; }
        if (g_cverbose || seed == A)
          printf("  seed %llu %-58s %4d states, %zu endings, %d-%d stages deep\n", (unsigned long long)seed, id.c_str(), w.states, w.ends.size(),
                 w.minDepth == (1 << 30) ? 0 : w.minDepth, w.maxDepth);
        restore(g, s0);
        g.story.drop(inst);
        trimCache(g.story);
      }
      // ---- pickCampaign at the plan's hook: offered when the world fits (over a few day buckets); never once told
      {
        const Snap s0 = snap(g);
        Spec probe;
        probe.campaign = true;
        probe.arch = P.id;
        for (const ArcSlot& sl : P.arcs) probe.arcs.push_back(sl.choices[0]);
        const dsl::Script* sc = script(specId(probe));
        const int site = sc ? hookFor(g, *sc) : -1;
        Hook h;
        h.kind = sc ? (int)sc->hook : 0;
        h.site = site;
        h.ev = sc ? sc->hookEv : -1;
        bool offered = false;
        Spec got;
        for (int d = 0; d < 400 && !offered && site >= 0; d += 8) {
          g.day = s0.day + d;
          if (sc && sc->hook == dsl::HookKind::Event) hookFor(g, *sc);   // (a fresh event each bucket)
          if (pickCampaign(g, h, got) && got.arch == P.id) offered = true;
        }
        if (offered) {
          picks++;
          if (!script(specId(got))) { out("FAIL: pickCampaign offered %s, which does not compose\n", specId(got).c_str()); seedBad++; }
          // told once: an ended run of it in this world stops every later offer
          g.story.played_[specId(got)]++;
          bool again = false;
          for (int d = 0; d < 400 && !again; d += 8) {
            g.day = s0.day + d;
            Spec x;
            if (pickCampaign(g, h, x) && x.arch == P.id) again = true;
          }
          if (again) { out("FAIL: pickCampaign offered %s again after it was told\n", P.id); seedBad++; }
        } else if (g_cverbose) {
          printf("  (seed %llu: %s was not offered at its hook in 50 day buckets)\n", (unsigned long long)seed, P.id);
        }
        restore(g, s0);
      }
    }
    printf("campaigns seed %llu: %d failures\n", (unsigned long long)seed, seedBad);
    bad += seedBad;
  }
  // ---- over all seeds: every stage of every arc and every ending of every plan was reached somewhere
  for (const CampaignPlan& P : campaignPlans()) {
    const std::string id = P.id;
    for (const std::string& e : endsWant[id]) if (!endsAll[id].count(e)) { printf("FAIL: campaigns: %s: no walk reached the ending %s\n", id.c_str(), e.c_str()); bad++; }
    std::string eff;
    for (const auto& kv : effectsAll[id]) eff += " " + kv.first + " x" + std::to_string(kv.second);
    printf("  %-10s cast %2d times, %3zu/%3zu stages and %zu/%zu endings reached; realm effects checked:%s\n", P.id, casts[id], stagesAll[id].size(),
           stagesWant[id].size(), endsAll[id].size(), endsWant[id].size(), eff.c_str());
  }
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  printf("campaigns: %d walks, %d (stage, vars) states, %d hooks offered a campaign, %.0f ms; %d failures\n", walks, states, picks, ms, bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--campaigns", "M6b campaigns: plans compose with every arc choice, cast on real worlds, every stage and ending walked, realm effects [--seeds A..B] [--sample] [--print <id> [arcs]] [--verbose]", campaignsCmd);
