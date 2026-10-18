// P1 "Fresh Roads" STORY lane checks (VISION_PLAN 15.34); (P1b TALES lane) --tale-reach.
//   rpg_test --story-p1 [--seeds A..B]                   the lane's own checks, one PASS/FAIL line each: the picker's
//                                                        policy on real worlds (no replays, one teller per tale per
//                                                        town, places falling through, news only when fresh), the
//                                                        ruin- and event-started stories, the campaigns' alternative
//                                                        heads, the tone fixes (the piper, lewis), slots that would cut
//                                                        into a conversation
//   rpg_test --story-events [--seeds A..B] [--weeks N]   which realm events reach the start's settlements in N weeks
//                                                        (the event-hooked campaigns' and stories' supply; a report)
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/sim/game_internal.h"
#include "rpg/sim/life.h"
#include "rpg/story/dsl.h"
#include "rpg/story/saga.h"
#include "rpg/story/story.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace story {
namespace saga {
std::vector<std::string> voiceLines(const std::string& key, int voice, char kind);   // (voice.cpp test hook)
}  // namespace saga
}  // namespace story

namespace {

using namespace story;
using story::saga::Motive;

int g_p1bad = 0;
void check(bool ok, const std::string& what) {
  printf("%s: story-p1 %s\n", ok ? "PASS" : "FAIL", what.c_str());
  if (!ok) g_p1bad++;
}

int hookOfBody(const char* body, int& ev) {
  std::vector<dsl::Script> v;
  std::vector<std::string> err;
  ev = -1;
  // (the hook line alone: the template's markup need not parse)
  std::string b = body ? body : "";
  size_t at = b.find("\nhook ");
  if (at == std::string::npos) return -1;
  const size_t e = b.find('\n', at + 1);
  const std::string line = b.substr(at + 1, e == std::string::npos ? std::string::npos : e - at - 1);
  dsl::parse(("script x\ntitle X\n" + line + "\n").c_str(), "p1", v, err);
  if (v.empty()) return -1;
  ev = v[0].hookEv;
  return (int)v[0].hook;
}

// the library checks (no world)
void libraryP1() {
  // ---- 5: ruin- and event-started single stories
  int ruin = 0, event = 0;
  for (const saga::Archetype& a : saga::archetypes()) {
    if (a.tier != 2) continue;
    int ev = -1;
    const int k = hookOfBody(a.body, ev);
    if (k == (int)dsl::HookKind::Ruin) ruin++;
    if (k == (int)dsl::HookKind::Event) event++;
  }
  check(ruin >= 6, "ruin-started single stories: " + std::to_string(ruin) + " (want >= 6)");
  check(event >= 6, "event-started single stories: " + std::to_string(event) + " (want >= 6)");
  // ---- 6: lewis, "NOBODY ELSE DARES NOT TO"
  {
    bool bad = false;
    for (int v = 0; v < 8; v++)
      for (uint32_t sd = 1; sd <= 6; sd++) {
        saga::Spec s;
        s.arch = "last_door"; s.voice = v; s.seed = sd * 7919u; s.motive = Motive::Devotion;
        saga::Composed c;
        if (saga::compose(s, c) && c.text.find("DARES NOT TO") != std::string::npos) bad = true;
      }
    check(!bad, "lewis last_door: no 'NOBODY ELSE DARES NOT TO' in 48 compositions");
  }
  // ---- 7: the piper: never a threat of blood after "I WON'T HURT THEM"; no road twist inside the piping contest
  std::set<std::string> harm;
  for (const std::string& k : saga::voiceKeys())
    for (int v = 0; v < 10; v++)
      for (char kind : {'F', 'B', 'M'}) {
        if (kind != 'M' && v >= 8) continue;
        for (const std::string& l : saga::voiceLines(k, v, kind))
          if (saga::phraseTone(k, l) & saga::TONE_HARM) harm.insert(l);
      }
  check(harm.size() >= 15 && harm.count("BLOOD WANTS BLOOD. YOURS WILL DO."), "tone tags: " + std::to_string(harm.size()) + " lines carry ~harm");
  {
    const saga::Archetype* piper = saga::archetype("piper");
    int bad = 0, n = 0;
    for (int v = 0; v < 8 && piper; v++)
      for (int m = 0; m < (int)Motive::COUNT; m++)
        for (uint32_t sd = 1; sd <= 4; sd++) {
          saga::Spec s;
          s.arch = "piper"; s.voice = v; s.motive = (Motive)m; s.seed = sd * 104729u + (uint32_t)v;
          saga::Composed c;
          if (!saga::compose(s, c)) continue;
          n++;
          const size_t at = c.text.find("I WON'T HURT THEM");
          const size_t eol = at == std::string::npos ? at : c.text.find('\n', at);
          const std::string tail = at == std::string::npos ? std::string() : c.text.substr(at, eol - at);
          for (const std::string& h : harm) if (tail.find(h) != std::string::npos) { bad++; break; }
        }
    check(piper && n > 0 && bad == 0, "piper: " + std::to_string(n) + " compositions, " + std::to_string(bad) + " with a threat of harm after 'I WON'T HURT THEM'");
    check(piper && saga::slotMidTalk(*piper, 1), "piper: slot t2 (inside the contest with the piper) takes no twist");
  }
  // ---- 7 (all archetypes): after words that promise no harm, never a ~harm phrase
  {
    int bad = 0, n = 0;
    std::string ex;
    for (const saga::Archetype& a : saga::archetypes()) {
      if (a.tier != 2) continue;
      for (int v = 0; v < 8; v++)
        for (int m = 0; m < (int)Motive::COUNT; m += 3) {
          if (a.motives && !(a.motives & saga::motiveBit((Motive)m))) continue;
          saga::Spec s;
          s.arch = a.id; s.voice = v; s.motive = (Motive)m; s.seed = 31u * (uint32_t)v + 7u * (uint32_t)m;
          saga::Composed c;
          if (!saga::compose(s, c)) continue;
          n++;
          size_t p = 0;
          while (p < c.text.size()) {
            size_t e = c.text.find('\n', p);
            if (e == std::string::npos) e = c.text.size();
            const std::string line = c.text.substr(p, e - p);
            p = e + 1;
            static const char* const kPeace[] = {"WON'T HURT", "WON'T HARM", "NO HARM", "MEAN YOU NO", "NEVER HURT"};
            for (const char* w : kPeace) {
              const size_t at = line.find(w);
              if (at == std::string::npos) continue;
              for (const std::string& h : harm)
                if (line.find(h, at) != std::string::npos) { bad++; if (ex.empty()) ex = std::string(a.id) + ": " + line.substr(0, 160); }
            }
          }
        }
    }
    check(bad == 0, "all stories: " + std::to_string(n) + " compositions, " + std::to_string(bad) + " threats of harm after a promise of none" + (ex.empty() ? "" : " (" + ex + ")"));
  }
  {
    int mid = 0, slots = 0;
    for (const saga::Archetype& a : saga::archetypes())
      for (int k = 0; k < 3 && a.tier == 2; k++)
        if (saga::slotsOf(a.body).count("t" + std::to_string(k + 1))) { slots++; if (saga::slotMidTalk(a, k)) mid++; }
    printf("  (twist slots: %d, of which %d cut into a conversation and stay empty)\n", slots, mid);
  }
  // ---- 4: the campaigns' alternative heads keep their head in the spec id
  {
    int heads = 0;
    bool ok = true;
    for (const saga::CampaignPlan& P : saga::campaignPlans())
      for (size_t h = 0; h < P.alts.size(); h++) {
        heads++;
        saga::Spec s;
        s.campaign = true; s.arch = P.id; s.head = (int)h + 1; s.seed = 9;
        for (const saga::ArcSlot& sl : P.arcs) s.arcs.push_back(sl.choices[0]);
        saga::Spec back;
        saga::Composed c;
        ok = ok && saga::parseSpecId(saga::specId(s), back) && back.head == s.head && saga::composeCampaign(s, c);
      }
    saga::Spec old;
    const bool oldOk = saga::parseSpecId("saga2~@burden~a.b.c.d~~~v1~m2~0000000a", old) && old.head == 0;
    check(ok && heads >= 2 && oldOk, "campaign alternative heads: " + std::to_string(heads) + " compose and round-trip; a head-less id reads as head 0");
  }
}

// the policy on a real world
void worldP1(uint64_t seed) {
  g_curSeed = seed;
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play; g.godMode = true; g.noWildSpawns = true;
  for (int f = 0; f < 30; f++) { g.update(SIM_DT, Input()); g.events.clear(); }
  const std::string sd = " (seed " + std::to_string(seed) + ")";
  // ---- 2: at most one teller per tale per town; asking again gives the same answer
  std::map<std::string, std::set<uint64_t>> tellers;
  int asked = 0, offers = 0;
  bool stable = true;
  for (int pass = 0; pass < 2; pass++)
    for (size_t k = 1; k < g.actors.size(); k++) {
      Actor& a = g.actors[k];
      if (!a.npc || !a.human || a.st == AState::Dead || isStoryPerson(a)) continue;
      const Offer o = tellerOffer(g, a);
      if (pass == 0) asked++;
      if (!o.any()) continue;
      if (pass == 0) offers++;
      std::string key = o.lib >= 0 ? "lib:" + o.id() : std::string();
      saga::Spec s;
      if (o.lib < 0 && saga::parseSpecId(o.saga, s)) key = "saga:" + s.arch;
      const int hs = homeSiteOf(g, a);
      const size_t before = tellers[key + "@" + std::to_string(hs)].size();
      tellers[key + "@" + std::to_string(hs)].insert(g.npcKey(a));
      if (pass == 1 && tellers[key + "@" + std::to_string(hs)].size() != before) stable = false;
    }
  int worst = 0;
  std::string worstKey;
  for (const auto& kv : tellers) if ((int)kv.second.size() > worst) { worst = (int)kv.second.size(); worstKey = kv.first; }
  check(worst <= 1 && stable, "one teller per tale per town: " + std::to_string(asked) + " asked, " + std::to_string(offers) + " offers, most tellers of one tale " +
                                  std::to_string(worst) + (worst > 1 ? " (" + worstKey + ")" : "") + sd);
  // ---- 1: a finished tale is never offered again (every library tale marked played: nobody offers one)
  for (const dsl::Script& sc : dsl::library().scripts) g.story.played_[sc.id]++;
  // ((P1b) a `once site` script, a town request, is played once per settlement, not once per world: marked played
  // here only in the world's count, it may still come up in a town that never had it, by design: not a replay)
  int replays = 0;
  std::string replayIds;
  for (size_t k = 1; k < g.actors.size(); k++) {
    Actor& a = g.actors[k];
    if (!a.npc || !a.human || a.st == AState::Dead || isStoryPerson(a)) continue;
    const Offer o = tellerOffer(g, a);
    if (o.lib < 0 || dsl::library().scripts[(size_t)o.lib].onceSite) continue;
    replays++;
    replayIds += " " + o.id();
  }
  // ---- 3: places fall through (a played tale is never a place's offer; the news only with fresh news)
  int placeReplays = 0;
  for (int si : g.world.nearSites) {
    if (si < 0 || si >= (int)g.world.sites.size()) continue;
    for (int kind : {(int)dsl::HookKind::Board, (int)dsl::HookKind::Ruin, (int)dsl::HookKind::Event, (int)dsl::HookKind::Herald})
      if (placeOffer(g, kind, si).lib >= 0) placeReplays++;
  }
  check(replays == 0 && placeReplays == 0, "finished tales never offered again: talk " + std::to_string(replays) + replayIds + ", places " + std::to_string(placeReplays) + sd);
  // a news tale with no fresh news: never offered (a fresh world: no famine, no war reached the start)
  for (const dsl::Script& sc : dsl::library().scripts) g.story.played_.erase(sc.id);
  int staleNews = 0;
  for (int si : g.world.nearSites) {
    if (si < 0 || si >= (int)g.world.sites.size()) continue;
    const Offer o = placeOffer(g, (int)dsl::HookKind::Event, si);
    if (o.lib >= 0 && !eventTaleFresh(g, dsl::library().scripts[(size_t)o.lib], si)) staleNews++;
  }
  check(staleNews == 0, "the news offers a tale only on fresh news: " + std::to_string(staleNews) + " stale" + sd);
}

int storyP1(int argc, char** argv) {
  uint64_t A = 1, B = 3;
  for (int i = 1; i < argc; i++)
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc && !parseSeedRange(argv[++i], A, B)) return 2;
  g_p1bad = 0;
  libraryP1();
  for (uint64_t s = A; s <= B; s++) worldP1(s);
  printf("story-p1: %d failures\n", g_p1bad);
  return g_p1bad ? 1 : 0;
}

const char* evName(int t) {
  static const char* n[] = {"FirstContact", "TradeDeal", "TradeBroken", "HarvestFailed", "Famine", "BorderIncident", "Skirmish", "WarDeclared",
                            "SiegeBegun", "SiegeBroken", "TownTaken", "TownBurned", "Refugees", "Peace", "RulerDied", "Succession", "CivilWar",
                            "KingdomFell", "KingdomRose", "Resettled", "Ruined", "Festival", "TroopsMarching", "PricesRising", "BeastRaid",
                            "BeastSlain", "WarlordRose", "WarbandRaid", "SuccessorRose", "Reunified", "AllianceMade", "AllyJoined",
                            "Vassalised", "TributePaid", "TributeRefused", "Independence", "FiefGranted", "Enlisted", "Promoted",
                            "TermsCarried", "BossWoke", "Rebuilt"};
  return t >= 0 && t < (int)(sizeof(n) / sizeof(n[0])) ? n[t] : "?";
}

int storyEvents(int argc, char** argv) {
  uint64_t A = 1, B = 5;
  int weeks = 12;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) {
      if (!parseSeedRange(argv[++i], A, B)) return 2;
    } else if (!strcmp(argv[i], "--weeks") && i + 1 < argc) weeks = atoi(argv[++i]);
  }
  std::map<std::string, int> seen, seeds;
  for (uint64_t seed = A; seed <= B; seed++) {
    g_curSeed = seed;
    Game g(seed);
    g.newEndlessGame(seed);
    g.mode = Mode::Play; g.godMode = true; g.noWildSpawns = true;
    for (int f = 0; f < 10; f++) { g.update(SIM_DT, Input()); g.events.clear(); }
    const Site st = g.world.sites[(size_t)g.world.startSite];
    const int32_t sx = g.world.ox + st.ex, sy = g.world.oy + st.ey;
    std::vector<std::pair<int32_t, int32_t>> spots = {{sx, sy}};
    for (SiteType t : {SiteType::Town, SiteType::City}) {
      const int si = g.world.findSiteNear(sx, sy, t, 8);
      if (si >= 0) spots.push_back({g.world.ox + g.world.sites[(size_t)si].ex, g.world.oy + g.world.sites[(size_t)si].ey});
    }
    std::set<uint32_t> ids;
    std::set<std::string> here;
    for (int w = 0; w < weeks; w++) {
      for (const realm::WorldEvent& e : g.realm.events()) {
        if (ids.count(e.id) || g.day - (int)e.day > 12) continue;
        bool reach = false;
        for (const auto& p : spots) reach |= story::rumourReaches(e, g.day, p.first, p.second);
        if (!reach) continue;
        ids.insert(e.id);
        seen[evName((int)e.type)]++;
        here.insert(evName((int)e.type));
      }
      g.day += 7;
      for (int f = 0; f < 3; f++) { g.update(SIM_DT, Input()); g.events.clear(); }
    }
    for (const std::string& s : here) seeds[s]++;
  }
  printf("story-events: seeds %llu..%llu, %d weeks (events within rumour reach of the start, its town and city)\n", (unsigned long long)A,
         (unsigned long long)B, weeks);
  for (const auto& kv : seen) printf("  %-16s %4d events, in %d seeds\n", kv.first.c_str(), kv.second, seeds[kv.first]);
  return 0;
}

// (P1b TALES lane) rpg_test --tale-reach [--seeds A..B] [--rings N]: where each P1b tale (tales2) could be offered, a
// report: over the settlements of N region rings round the start, at how many its offer gates hold (offerPrecheck:
// settle / needs / the world's wars and events) and how many census residents of its hook trade live there (its
// possible tellers). A tale the quest-mix visit set never offers is explained here (the coast, the trade, a war).
int taleReach(int argc, char** argv) {
  uint64_t A = 1, B = 5;
  int rings = 2;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) {
      if (!parseSeedRange(argv[++i], A, B)) return 2;
    } else if (!strcmp(argv[i], "--rings") && i + 1 < argc) rings = std::max(0, atoi(argv[++i]));
  }
  const dsl::Library& L = dsl::library();
  struct Reach { int gates = 0, tellers = 0, both = 0; std::set<uint64_t> seeds; };
  std::map<std::string, Reach> R;
  int settlements = 0, coast = 0;
  for (uint64_t seed = A; seed <= B; seed++) {
    g_curSeed = seed;
    Game g(seed);
    g.newEndlessGame(seed);
    g.mode = Mode::Play; g.godMode = true; g.noWildSpawns = true;
    for (int f = 0; f < 10; f++) { g.update(SIM_DT, Input()); g.events.clear(); }
    const Site st = g.world.sites[(size_t)g.world.startSite];
    const int32_t sx = g.world.ox + st.ex, sy = g.world.oy + st.ey;
    for (int32_t ry = ew::regionOf(sy) - rings; ry <= ew::regionOf(sy) + rings; ry++)
      for (int32_t rx = ew::regionOf(sx) - rings; rx <= ew::regionOf(sx) + rings; rx++) {
        const ew::RegionPlan RP = g.world.regionPlan(rx, ry);
        for (const ew::SitePlan& p : RP.sites) {
          if ((p.type != SiteType::Village && p.type != SiteType::Town && p.type != SiteType::City) || (p.flags & ew::SPF_HIDDEN)) continue;
          const int h = g.world.ensureSite(p.id);
          if (h < 0) continue;
          settlements++;
          const Site S = g.world.sites[(size_t)h];
          if (nearCoast(g, g.world.ox + S.ex, g.world.oy + S.ey)) coast++;
          const life::Census* c = g.life.census(g.world, h);
          for (const dsl::Script& s : L.scripts) {
            if (s.family != "tale") continue;
            Reach& r = R[s.id];
            const bool gates = offerPrecheck(g, s, h);
            int tellers = 0;
            if (s.hook != dsl::HookKind::Npc) tellers = (s.hook == dsl::HookKind::Board ? S.type != SiteType::Village : true) ? 1 : 0;
            else if (c)
              for (const life::Resident& res : c->res)
                if (s.hookTrade == -2 ? (life::jobRole(res.job) == Role::Guard || life::jobRole(res.job) == Role::Jarl) : (int)life::jobRole(res.job) == s.hookTrade) tellers++;
            if (gates) r.gates++;
            if (tellers) r.tellers++;
            if (gates && tellers) { r.both++; r.seeds.insert(seed); }
          }
        }
      }
  }
  printf("tale-reach: seeds %llu..%llu, %d region rings: %d settlements (%d near the sea)\n", (unsigned long long)A, (unsigned long long)B, rings,
         settlements, coast);
  for (const dsl::Script& s : L.scripts) {
    if (s.family != "tale") continue;
    const Reach& r = R[s.id];
    printf("  %-13s %-23s gates hold at %4d, a teller of its trade at %4d, both at %4d settlements, in %zu seeds\n", s.id.c_str(), s.title.c_str(),
           r.gates, r.tellers, r.both, r.seeds.size());
  }
  return 0;
}

}  // namespace

RPG_TEST_CMD("--tale-reach", "P1b tales: where each new tale's offer gates hold and its hook trade lives (a report) [--seeds A..B --rings 2]", taleReach);
RPG_TEST_CMD("--story-p1", "P1 story lane: the picker's policy, ruin and event stories, alternative heads, tone fixes [--seeds A..B]", storyP1);
RPG_TEST_CMD("--story-events", "P1 story: realm events reaching the start's settlements [--seeds A..B --weeks 12]", storyEvents);
