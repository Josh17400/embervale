// rpg_test --content-p1b [--seeds A..B] [--gates] [--names] [--only <id prefix>] [--verbose]
// P1b "New Roads Content" (lead, phase A; VISION_PLAN 15.35): every new job kind, request and tale, force-started and
// played headlessly through EVERY branch, plus the mechanical text check with the longest names the world can inject.
// The lead's file (frozen for the lanes: report a harness problem to the integrator instead of editing it).
//
//   1. the library holds the P1b families (family job | request | tale | test) and they validate; --gates: 30 requests
//      (10 in each of req_village / req_town / req_city), 20 tales (tales2), 15 radiant job kinds (radiant.h)
//   2. TEXT (every P1b script, every composed variant of every job kind): each placeholder filled with the LONGEST name
//      of its kind (kMax below; a name in the seeds' worlds longer than the table fails the test, so the table stays the
//      real maximum): title <= 22, option <= 40, every say / hint / memory fits the phone panel (speech::PANEL_CHARS x
//      PANEL_LINES: 70 x 5), journal <= 150, fact <= 160, notice <= 60, item <= 32, gossip <= 38 (no placeholder), no word
//      longer than 38, no lower case, no "{" left after a real cast
//   3. PLAY (per seed): each script cast at a settlement its `settle` words allow (the start village, the nearest town,
//      city, capital or port; the player travels there and the giver is a person of the hook's trade when one is out),
//      then every path walked to an ending: every option (hidden ones too, both outcomes of a check), every goal done,
//      every lost escort / bout (else) and every clock run out (deadline). On every path: no stage soft-locks (a talk
//      stage shows at least one option in the state the path reached it in; a goal can be completed), every live stage
//      has journal text, every ending is reached by some path, a success ending pays (gold, XP or an item since the
//      start), an ending reached off a dialogue with a say queues its epilogue, a census effect moves the census
//   4. the offer gates (offerGates) on the lead's test script: family test is never offered; settle / once site /
//      cooldown behave
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
#include "rpg/sim/life.h"
#include "rpg/sim/talk.h"
#include "rpg/story/dsl.h"
#include "rpg/story/radiant.h"
#include "rpg/story/story.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

using namespace story;
bool g_verbose = false;

// ---------------------------------------------------------------- 2. the longest names
// Measured with --names over seeds 1..20 (region plans 3 rings round the start, every culture there with 3,000 names a
// sex, every kingdom, ruler and ruin record), rounded up. A longer name in a test seed fails the test.
struct MaxLen { const char* key; int len; };
const MaxLen kMax[] = {
    // measured 2026-10-17 (seeds 1..20): person 10, settlement 12, city 11, cave 15, ruin 18, camp 18, kingdom 12, ruler 29,
    // god 8, people 12, ruin.old 12, ruin.lord 31, ruin.builder 27, ruin.clue 93, ruin.cause 10; the table adds 1-2
    {"person", 11}, {"settlement", 14}, {"city", 13}, {"cave", 17}, {"ruin", 20}, {"camp", 20}, {"kingdom", 14}, {"ruler", 31},
    {"god", 10}, {"people", 14}, {"ruin.old", 14}, {"ruin.lord", 33}, {"ruin.builder", 29}, {"ruin.clue", 95}, {"ruin.cause", 12},
};
// fixed words (the engine's own): epithets, pronoun fields, directions, the player's name (creator kNameMax), counts
constexpr int EPITHET_MAX = 12;       // caster kEpithets "GALLOWS-BORN"
constexpr int PLAYER_MAX = 12;        // creator.cpp kNameMax
constexpr int VAR_MAX = 6;            // speech::countWord: "ELEVEN", "TWELVE" (digits after: 2-3)
constexpr int DIR_MAX = 10;           // "NORTH-EAST", "RIGHT HERE"
constexpr int DWELLING_MAX = 40;      // vignetteTitle: "<FOLK>'S PHYSIC GARDEN" with an 8-letter folk name (judge, jobs_b)
constexpr int WATCHTOWER_MAX = 30;
constexpr int BEAST_MAX = 30, KIND_MAX = 16, JOB_MAX = 12, KIN_MAX = 13, TITLE_MAX = 12, YEARS_MAX = 4, WAR_MAX = 30, EVENT_MAX = 30;

struct Names {
  std::map<std::string, int> m;
  void see(const std::string& k, const std::string& v) { int& x = m[k]; x = std::max(x, (int)v.size()); }
  int get(const std::string& k) const {
    int t = 0;
    for (const MaxLen& e : kMax) if (k == e.key) t = e.len;
    auto it = m.find(k);
    return std::max(t, it == m.end() ? 0 : it->second);
  }
};

void measureNames(Game& g, Names& N) {
  if (!g.world.src) return;
  ew::EndlessSource& src = *g.world.src;
  const Site st = g.world.sites[(size_t)g.world.startSite];
  const int32_t sx = g.world.ox + st.ex, sy = g.world.oy + st.ey;
  std::set<uint64_t> cultures;
  int ruins = 0;
  for (int32_t ry = ew::regionOf(sy) - 3; ry <= ew::regionOf(sy) + 3; ry++)
    for (int32_t rx = ew::regionOf(sx) - 3; rx <= ew::regionOf(sx) + 3; rx++) {
      const ew::RegionPlan R = g.world.regionPlan(rx, ry);
      for (const ew::SitePlan& p : R.sites) {
        if (p.culture) cultures.insert(p.culture);
        switch (p.type) {
          case SiteType::Village: case SiteType::Town: N.see("settlement", p.name); break;
          case SiteType::City: N.see("settlement", p.name); N.see("city", p.name); break;
          case SiteType::Cave: N.see("cave", p.name); break;
          case SiteType::Ruin:
            N.see("ruin", p.name);
            if (ruins++ < 24) {
              const realm::RuinRecord rr = ruinRecordFor(g, p.id);
              N.see("ruin.old", rr.oldName); N.see("ruin.lord", rr.lastLord); N.see("ruin.builder", rr.builtBy);
              if (!rr.clues.empty()) N.see("ruin.clue", rr.clues[0]);
              N.see("ruin.cause", causeWords(rr.cause));
            }
            break;
          case SiteType::BanditCamp: N.see("camp", p.name); break;
          default: break;
        }
      }
    }
  for (uint64_t cu : cultures) {
    const cult::Culture& C = src.culture(cu);
    N.see("people", C.adjective.empty() ? C.name : C.adjective);
    if (!C.faith.names.empty()) N.see("god", C.faith.names[0]);
    for (uint32_t k = 0; k < 3000; k++) { N.see("person", cult::personName(C, k * 2654435761u + 17u, false)); N.see("person", cult::personName(C, k * 2246822519u + 5u, true)); }
  }
  for (const Kingdom& K : g.world.kingdoms) N.see("kingdom", K.name);
  for (const realm::KingdomState& K : g.realm.kingdoms()) {
    N.see("kingdom", K.name);
    std::string rn, rt;
    if (g.story.rulerOf(g, K.id, rn, rt)) N.see("ruler", rt + " " + rn);
  }
}

// the longest a placeholder can become in script `s`
int worstLen(const dsl::Script& s, const Names& N, const std::string& role, const std::string& field) {
  const int person = N.get("person");
  if (role == "player") return PLAYER_MAX;
  if (role == "var") return VAR_MAX;
  const int ri = s.role(role);
  if (ri < 0) return 12;
  const dsl::RoleDef& R = s.roles[(size_t)ri];
  const bool npcHook = s.hook == dsl::HookKind::Npc || s.hook == dsl::HookKind::Herald || s.hook == dsl::HookKind::Companion;
  if (!field.empty()) {
    static const std::map<std::string, int> fixed = {{"he", 3}, {"him", 3}, {"his", 3}, {"man", 5}, {"son", 8}, {"lad", 4}, {"brother", 7},
                                                     {"father", 6}, {"husband", 7}, {"sir", 4}, {"boy", 4}, {"dir", DIR_MAX}, {"job", JOB_MAX},
                                                     {"kin", KIN_MAX}, {"kind", KIND_MAX}, {"title", TITLE_MAX}, {"years", YEARS_MAX}};
    auto it = fixed.find(field);
    if (it != fixed.end()) return it->second;
    if (field == "realm") return std::max(N.get("kingdom"), 13);
    if (field == "god") return std::max(N.get("god"), 9);
    if (field == "people") return std::max(N.get("people"), 13);
    if (field == "lost" || field == "name") return person;
    if (field == "town") return N.get("settlement");
    if (field == "foe") return N.get("kingdom");
    if (field == "lord") return R.kind == dsl::RoleKind::Ruin ? N.get("ruin.lord") : TITLE_MAX + 1 + person;
    if (field == "old") return N.get("ruin.old");
    if (field == "builder") return N.get("ruin.builder");
    if (field == "clue") return N.get("ruin.clue");
    if (field == "cause") return N.get("ruin.cause");
    return 12;
  }
  switch (R.kind) {
    case dsl::RoleKind::Giver: return npcHook ? person : N.get("settlement");
    case dsl::RoleKind::Person: case dsl::RoleKind::Npc: case dsl::RoleKind::Resident: return person;
    case dsl::RoleKind::Foe: return person + 1 + EPITHET_MAX;
    case dsl::RoleKind::Site:
      if (R.a == "cave") return N.get("cave");
      if (R.a == "ruin") return N.get("ruin");
      if (R.a == "camp") return N.get("camp");
      if (R.a == "city") return N.get("city");
      if (R.a == "dwelling") return DWELLING_MAX;
      if (R.a == "watchtower") return WATCHTOWER_MAX;
      return N.get("settlement");
    case dsl::RoleKind::Capital: return N.get("city");
    case dsl::RoleKind::Kingdom: return N.get("kingdom");
    case dsl::RoleKind::Ruler: return N.get("ruler");
    case dsl::RoleKind::Ruin: return N.get("ruin");
    case dsl::RoleKind::War: return WAR_MAX;
    case dsl::RoleKind::Event: return EVENT_MAX;
    case dsl::RoleKind::Beast: case dsl::RoleKind::Boss: return BEAST_MAX;
    default: return 12;
  }
}

std::string worstFill(const dsl::Script& s, const Names& N, const std::string& text) {
  std::string out;
  size_t i = 0;
  while (i < text.size()) {
    const size_t a = text.find('{', i);
    if (a == std::string::npos) { out += text.substr(i); break; }
    out += text.substr(i, a - i);
    const size_t e = text.find('}', a);
    if (e == std::string::npos) { out += text.substr(a); break; }
    std::string ph = text.substr(a + 1, e - a - 1);
    for (char& c : ph) c = (char)std::tolower((unsigned char)c);
    const size_t dot = ph.find('.');
    const int n = worstLen(s, N, dot == std::string::npos ? ph : ph.substr(0, dot), dot == std::string::npos ? std::string() : ph.substr(dot + 1));
    out += std::string((size_t)std::max(1, n), 'W');   // (one long word: the wrap's worst case)
    i = e + 1;
  }
  return out;
}

int textCheck(const dsl::Script& s, const Names& N) {
  int bad = 0;
  auto fail = [&](const std::string& what, const std::string& text, size_t len, size_t lim) {
    out("FAIL: content-p1b text %s: the %s is %zu > %zu with the longest names: %s\n", s.id.c_str(), what.c_str(), len, lim, text.c_str());
    bad++;
  };
  auto words = [&](const std::string& what, const std::string& filled, const std::string& raw) {
    size_t run = 0;
    for (char c : filled) {
      run = c == ' ' ? 0 : run + 1;
      if (run > 38) { out("FAIL: content-p1b text %s: a word longer than 38 in the %s: %s\n", s.id.c_str(), what.c_str(), raw.c_str()); bad++; break; }
    }
    for (char c : raw) if (c >= 'a' && c <= 'z') { out("FAIL: content-p1b text %s: lower case in the %s: %s\n", s.id.c_str(), what.c_str(), raw.c_str()); bad++; break; }
  };
  auto lim = [&](const std::string& what, const std::string& raw, size_t max) {
    if (raw.empty()) return;
    const std::string f = worstFill(s, N, raw);
    if (f.size() > max) fail(what, raw, f.size(), max);
    words(what, f, raw);
  };
  auto panel = [&](const std::string& what, const std::string& raw) {
    if (raw.empty()) return;
    const std::string f = worstFill(s, N, raw);
    const size_t rows = speech::wrapLines(f, speech::PANEL_CHARS).size();
    if (rows > (size_t)speech::PANEL_LINES) fail(what + " (panel rows)", raw, rows, (size_t)speech::PANEL_LINES);
    words(what, f, raw);
  };
  lim("title", s.title, 22);
  lim("pitch", s.pitch, 40);
  panel("hint", s.hint);
  for (const std::string& gl : s.gossip) lim("gossip", gl, 38);
  for (const dsl::StageDef& G : s.stages) {
    panel("say of " + G.name, G.say);
    lim("journal of " + G.name, G.journal, 150);
    for (const dsl::OptDef& o : G.opts) lim("option of " + G.name, o.label, 40);
    for (const dsl::EffDef& e : G.effs) {
      switch (e.type) {
        case dsl::EffType::Remember: case dsl::EffType::Shop: panel("memory in " + G.name, e.text); break;
        case dsl::EffType::Fact: lim("fact in " + G.name, e.text, 160); break;
        case dsl::EffType::Notice: lim("notice in " + G.name, e.text, 60); break;
        case dsl::EffType::Give: case dsl::EffType::Take: lim("item in " + G.name, e.text, 32); break;
        default: break;
      }
    }
    if (G.kind == dsl::StageKind::Goal && !G.goal.item.empty()) lim("item in " + G.name, G.goal.item, 32);
  }
  return bad;
}

// ---------------------------------------------------------------- 3. playing every branch
struct Snap {
  story::Engine st;
  realm::Realm rm;
  life::Life life;
  int gold = 0, plXp = 0, plLevel = 1, nextQuestId = 1, trackedQuest = -1, day = 1;
  std::vector<Item> inv;
  std::map<uint64_t, int32_t> marks;
  std::vector<Quest> quests;
};
Snap snap(const Game& g) {
  Snap s;
  s.st = g.story; s.rm = g.realm; s.life = g.life; s.gold = g.gold; s.plXp = g.plXp; s.plLevel = g.plLevel; s.nextQuestId = g.nextQuestId;
  s.trackedQuest = g.trackedQuest; s.inv = g.inv; s.marks = g.marks; s.quests = g.quests; s.day = g.day;
  return s;
}
void restore(Game& g, const Snap& s) {
  g.story = s.st; g.realm = s.rm; g.life = s.life; g.gold = s.gold; g.plXp = s.plXp; g.plLevel = s.plLevel; g.nextQuestId = s.nextQuestId;
  g.trackedQuest = s.trackedQuest; g.inv = s.inv; g.marks = s.marks; g.quests = s.quests; g.day = s.day;
  g.events.clear();
  g.mode = Mode::Play;
}
void steps(Game& g, int n) { for (int i = 0; i < n; i++) { g.update(SIM_DT, Input()); g.events.clear(); if (g.mode == Mode::Dialogue) g.mode = Mode::Play; } }

struct Walk {
  int paths = 0, choices = 0, bad = 0;
  std::set<std::string> ends;
  int startGold = 0, startXp = 0;
  size_t startInv = 0;
};

// the census numbers a script's census effects touch (site role -> mood + stock sum), for the outcome check
int censusSum(Game& g, const Instance& in, const dsl::StageDef& G) {
  int sum = 0;
  for (const dsl::EffDef& e : G.effs) {
    if (e.type != dsl::EffType::Census) continue;
    const Binding* b = bindingOf(in, e.args[0]);
    if (!b) continue;
    const int h = siteByIdLoad(g, b->site ? b->site : b->id);
    life::Census* c = h >= 0 && g.world.sites[(size_t)h].settlement() ? g.life.census(g.world, h) : nullptr;
    if (!c) continue;
    sum += c->mood * 7 + (int)c->famineUntil * 3 + c->festivalDay * 5 + (int)c->res.size() * 11;
    for (size_t k = 0; k < c->stock.size(); k++) sum += c->stock[k] * (int)(k + 2) + c->price[k] * (int)(k + 13);
  }
  return sum;
}

void walk(Game& g, uint32_t id, const dsl::Script& s, Walk& W, std::map<std::string, int>& visits, int depth, std::string& trail, bool viaChoice) {
  Instance* in = g.story.find(id);
  if (!in) { out("FAIL: content-p1b %s: the instance vanished (%s)\n", s.id.c_str(), trail.c_str()); W.bad++; return; }
  for (const std::string& t : g.story.stageTexts(g, *in))
    if (t.find('{') != std::string::npos || t.find('}') != std::string::npos) {
      out("FAIL: content-p1b %s stage %s: an unresolved placeholder: %s\n", s.id.c_str(), g.story.stageName(*in).c_str(), t.c_str());
      W.bad++;
    }
  const std::string stName = g.story.stageName(*in);
  const int stIx = s.stage(stName);
  if (in->done || g.story.isEnd(*in)) {
    W.paths++;
    W.ends.insert(stName);
    const dsl::StageDef& E = s.stages[(size_t)std::max(0, stIx)];
    const bool declined = !in->questId && in->failed;
    if (E.success && s.family != "test" && !declined) {
      const bool paid = g.gold > W.startGold || g.plXp > W.startXp || g.inv.size() > W.startInv;
      if (!paid) { out("FAIL: content-p1b %s: the success ending %s pays nothing (%s)\n", s.id.c_str(), stName.c_str(), trail.c_str()); W.bad++; }
    }
    if (!viaChoice && !E.say.empty() && g.story.epilogues_.empty()) {
      out("FAIL: content-p1b %s: the ending %s was reached off a dialogue but its say was not queued as an epilogue\n", s.id.c_str(), stName.c_str());
      W.bad++;
    }
    if (g_verbose) out("  path %s -> %s\n", trail.c_str(), stName.c_str());
    return;
  }
  if (++visits[stName] > 2 || depth > 90) {
    out("FAIL: content-p1b %s: a loop through stage %s (%s)\n", s.id.c_str(), stName.c_str(), trail.c_str());
    visits[stName]--;
    W.bad++;
    return;
  }
  // a live stage says what to do in the journal
  for (const Quest& q : g.quests)
    if (q.id == in->questId && in->questId && q.desc.empty()) { out("FAIL: content-p1b %s stage %s: no journal text\n", s.id.c_str(), stName.c_str()); W.bad++; }
  const Snap s0 = snap(g);
  auto branch = [&](const char* tag, bool choice, auto&& act) {
    restore(g, s0);
    g.story.epilogues_.clear();
    const dsl::StageDef* landing = nullptr;
    int before = 0;
    if (!act()) return false;
    Instance* after = g.story.find(id);
    if (after) {
      const int ai = s.stage(g.story.stageName(*after));
      if (ai >= 0) landing = &s.stages[(size_t)ai];
    }
    (void)before; (void)landing;
    const std::string t0 = trail;
    trail += std::string(">") + tag + ":" + (after ? g.story.stageName(*after) : std::string("?"));
    walk(g, id, s, W, visits, depth + 1, trail, choice);
    trail = t0;
    return true;
  };
  // (everything about this stage is read before any branch runs: a restore moves the engine, and `in` with it)
  const bool canFail = g.story.goalCanFail(*in), clock = g.story.stageDeadline(*in) > 0, goal = g.story.isGoal(*in);
  const int n = goal ? 0 : g.story.optionCount(*in);
  int shown = 0;
  std::vector<uint8_t> isCheck((size_t)std::max(0, n), 0);
  for (int i = 0; i < n; i++) { shown += g.story.optionShown(g, *in, i) ? 1 : 0; isCheck[(size_t)i] = g.story.optionIsCheck(*in, i) ? 1 : 0; }
  in = nullptr;
  if (canFail && !branch("lost", false, [&] { return g.story.debugFail(g, id); })) {
    out("FAIL: content-p1b %s stage %s: the else branch cannot be taken\n", s.id.c_str(), stName.c_str()); W.bad++;
  }
  if (clock && !branch("late", false, [&] { return g.story.debugDeadline(g, id); })) {
    out("FAIL: content-p1b %s stage %s: the deadline branch cannot be taken\n", s.id.c_str(), stName.c_str()); W.bad++;
  }
  if (goal) {
    if (!branch("done", false, [&] { return g.story.debugComplete(g, id); })) {
      out("FAIL: content-p1b %s stage %s: the objective cannot be completed (a soft lock)\n", s.id.c_str(), stName.c_str());
      W.bad++;
    }
    restore(g, s0);
    visits[stName]--;
    return;
  }
  if (n <= 0) { out("FAIL: content-p1b %s stage %s: a dialogue with no options\n", s.id.c_str(), stName.c_str()); W.bad++; restore(g, s0); visits[stName]--; return; }
  // no soft lock: in the state this path reached the dialogue in, the player sees at least one option
  if (!shown) { out("FAIL: content-p1b %s stage %s: every option is hidden on this path (a soft lock: %s)\n", s.id.c_str(), stName.c_str(), trail.c_str()); W.bad++; }
  for (int i = 0; i < n; i++) {
    const bool check = isCheck[(size_t)i] != 0;
    for (int c = check ? 1 : 0; c <= (check ? 2 : 0); c++) {
      W.choices++;
      const std::string tag = std::to_string(i) + (check ? (c == 1 ? "+" : "-") : "");
      // a census effect on the stage the option lands on moves the census
      if (!branch(tag.c_str(), true, [&] {
            const dsl::StageDef& G0 = s.stages[(size_t)stIx];
            const int to = i < (int)G0.opts.size() ? (G0.opts[(size_t)i].check && c == 2 ? G0.opts[(size_t)i].elseIx : G0.opts[(size_t)i].toIx) : -1;
            Instance* cur = g.story.find(id);
            const int pre = to >= 0 && cur ? censusSum(g, *cur, s.stages[(size_t)to]) : 0;
            if (!g.story.choose(g, id, i, true, c)) return false;
            Instance* aft = g.story.find(id);
            if (to >= 0 && aft) {
              bool hasCensus = false;
              for (const dsl::EffDef& e : s.stages[(size_t)to].effs) hasCensus |= e.type == dsl::EffType::Census;
              if (hasCensus && censusSum(g, *aft, s.stages[(size_t)to]) == pre) {
                out("FAIL: content-p1b %s: the census effects of stage %s changed nothing\n", s.id.c_str(), s.stages[(size_t)to].name.c_str());
                W.bad++;
              }
            }
            return true;
          })) {
        out("FAIL: content-p1b %s stage %s: option %d cannot be chosen\n", s.id.c_str(), stName.c_str(), i);
        W.bad++;
      }
    }
  }
  restore(g, s0);
  visits[stName]--;
}

// a settlement the script may be offered in (by its settle words), loaded and travelled to; -1 none near
int hookSiteFor(Game& g, const dsl::Script& s) {
  const Site st = g.world.sites[(size_t)g.world.startSite];
  const int32_t sx = g.world.ox + st.ex, sy = g.world.oy + st.ey;
  bool village = s.settle.empty(), town = false, city = false, capital = false, port = false;
  for (const std::string& w : s.settle) {
    village |= w == "village"; town |= w == "town"; city |= w == "city"; capital |= w == "capital"; port |= w == "port";
  }
  if (!village && !town && !city && !capital) village = town = city = true;
  // the nearest fitting settlement from the region plans (a port: archetype Port)
  ew::Gid best = 0;
  int64_t bd = INT64_MAX;
  for (int32_t ry = ew::regionOf(sy) - 4; ry <= ew::regionOf(sy) + 4; ry++)
    for (int32_t rx = ew::regionOf(sx) - 4; rx <= ew::regionOf(sx) + 4; rx++) {
      const ew::RegionPlan R = g.world.regionPlan(rx, ry);
      for (const ew::SitePlan& p : R.sites) {
        const bool cap = (p.flags & ew::SPF_CAPITAL) != 0;
        const bool tierOk = (village && p.type == SiteType::Village) || (town && p.type == SiteType::Town) || (city && p.type == SiteType::City) ||
                            (capital && p.type == SiteType::City && cap);
        if (!tierOk || (p.flags & ew::SPF_HIDDEN)) continue;
        if (port && p.archetype != ew::Archetype::Port) continue;
        const int64_t d = (int64_t)(p.ex - sx) * (p.ex - sx) + (int64_t)(p.ey - sy) * (p.ey - sy);
        if (d < bd) { bd = d; best = p.id; }
      }
    }
  if (!best) return -1;
  const int h = g.world.ensureSite(best);
  if (h < 0) return -1;
  if (g.world.sites[(size_t)h].id != g.world.sites[(size_t)g.world.startSite].id) {
    g.world.sites[(size_t)h].discovered = true;
    if (!g.fastTravel(h)) return -1;
    steps(g, 8);
  }
  return g.world.siteHandle(best);
}

int hookActorFor(Game& g, const dsl::Script& s, int site) {
  if (s.hook != dsl::HookKind::Npc && s.hook != dsl::HookKind::Herald) return -1;
  for (const Actor& a : g.actors) {
    if (!a.npc || !a.human || isStoryPerson(a) || a.st == AState::Dead) continue;
    if (homeSiteOf(g, a) != site) continue;
    if (s.hook == dsl::HookKind::Herald ? a.role == Role::Herald : (s.hookTrade < 0 || (int)a.role == s.hookTrade)) return a.id;
  }
  return -1;
}

// the world events a script's event roles and hook need, forced at its hook place
void forceEvents(Game& g, const dsl::Script& s, int site) {
  const Site S = g.world.sites[(size_t)site];
  const int32_t hx = g.world.ox + S.ex, hy = g.world.oy + S.ey;
  const ew::Gid land = landAt(g, hx, hy);
  const ew::Gid home = S.homeKingdom >= 0 ? g.world.kingdoms[(size_t)S.homeKingdom].id : 0;
  g.realm.noteSite(S.id, home, (uint8_t)S.type, hx, hy);
  std::vector<int> evs;
  if (s.hook == dsl::HookKind::Event) evs.push_back(s.hookEv);
  for (const dsl::RoleDef& R : s.roles) {
    if (R.kind == dsl::RoleKind::Event) evs.push_back(R.ev);
    if (R.kind == dsl::RoleKind::War) evs.push_back((int)realm::EvType::WarDeclared);
  }
  for (int ev : evs) {
    if (ev == (int)realm::EvType::Famine) g.realm.forceFamine(S.id, g.day);
    else if (ev == (int)realm::EvType::WarDeclared) {
      ew::Gid other = 0;
      for (const realm::KingdomState& K : g.realm.kingdoms()) if (K.id != land && !K.fallen) { other = K.id; break; }
      if (other && land && !g.realm.atWar(other, land)) { g.realm.forceWar(other, land, g.day); g.realm.forceEvent(realm::EvType::WarDeclared, other, land, S.id, hx, hy, g.day); }
    } else g.realm.forceEvent((realm::EvType)ev, land, 0, S.id, hx, hy, g.day);
  }
}

int playScript(Game& g, const dsl::Script& s, const std::string& id, int site, int& paths) {
  int bad = 0;
  const Snap s0 = snap(g);
  forceEvents(g, s, site);
  const int actor = hookActorFor(g, s, site);
  std::string why;
  const uint32_t inst = g.story.start(g, id, actor, site, &why);
  if (!inst) { out("FAIL: content-p1b %s: cannot cast it at %s: %s\n", id.c_str(), g.world.sites[(size_t)site].name.c_str(), why.c_str()); restore(g, s0); return 1; }
  Walk W;
  W.startGold = s0.gold; W.startXp = s0.plXp; W.startInv = s0.inv.size();
  std::map<std::string, int> visits;
  std::string trail = g.story.stageName(*g.story.find(inst));
  walk(g, inst, s, W, visits, 0, trail, true);
  bad += W.bad;
  paths += W.paths;
  for (const dsl::StageDef& G : s.stages)
    if (G.kind == dsl::StageKind::End && !W.ends.count(G.name)) { out("FAIL: content-p1b %s: no path reaches the ending '%s'\n", id.c_str(), G.name.c_str()); bad++; }
  if (g_verbose) out("content-p1b: %-24s at %-14s giver %s: %d paths, %d choices, %zu endings\n", id.c_str(), g.world.sites[(size_t)site].name.c_str(),
                     actor >= 0 ? "a person" : "the place", W.paths, W.choices, W.ends.size());
  restore(g, s0);
  return bad;
}

// ---------------------------------------------------------------- 4. the offer gates
int gateChecks(Game& g) {
  int bad = 0;
  const dsl::Library& L = dsl::library();
  const int ti = L.find("p1b_mechanics");
  if (ti < 0) { out("FAIL: content-p1b: the lead's test script p1b_mechanics is missing\n"); return 1; }
  const dsl::Script& T = L.scripts[(size_t)ti];
  std::string why;
  if (offerGates(g, T, g.world.startSite, &why)) { out("FAIL: content-p1b: a family test script passed the offer gates\n"); bad++; }
  // the same gates with the family lifted: settle village town city fits the start village; once site; cooldown 3
  dsl::Script U = T;
  U.family = "tale";
  if (!offerGates(g, U, g.world.startSite, &why)) { out("FAIL: content-p1b: settle village refused the start village (%s)\n", why.c_str()); bad++; }
  U.settle = {"capital"};
  if (offerGates(g, U, g.world.startSite, &why)) { out("FAIL: content-p1b: settle capital passed at a village\n"); bad++; }
  U.settle = {"village"};
  const ew::Gid home = g.world.sites[(size_t)g.world.startSite].id;
  g.marks[storyMarkKey(ew::mix64(strHash64s(U.id) ^ home), MK_P1B_PLAYED)] = g.day;
  if (offerGates(g, U, g.world.startSite, &why)) { out("FAIL: content-p1b: once site offered again where it was played\n"); bad++; }
  g.marks.erase(storyMarkKey(ew::mix64(strHash64s(U.id) ^ home), MK_P1B_PLAYED));
  g.marks[storyMarkKey(home, MK_P1B_COOLDOWN)] = g.day;
  if (offerGates(g, U, g.world.startSite, &why)) { out("FAIL: content-p1b: a cooldown script offered the day another ended\n"); bad++; }
  g.day += 3;
  if (!offerGates(g, U, g.world.startSite, &why)) { out("FAIL: content-p1b: the cooldown never ends (%s)\n", why.c_str()); bad++; }
  g.day -= 3;
  g.marks.erase(storyMarkKey(home, MK_P1B_COOLDOWN));
  return bad;
}

int contentCmd(int argc, char** argv) {
  uint64_t A = 1, B = 3;
  bool gates = false, names = false;
  std::string only;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) {
      if (!parseSeedRange(argv[++i], A, B)) { printf("bad --seeds range '%s'\n", argv[i]); return 2; }
    } else if (!strcmp(argv[i], "--gates")) gates = true;
    else if (!strcmp(argv[i], "--names")) names = true;
    else if (!strcmp(argv[i], "--only") && i + 1 < argc) only = argv[++i];
    else if (!strcmp(argv[i], "--verbose")) g_verbose = true;
  }
  const auto t0 = std::chrono::steady_clock::now();
  int bad = 0;
  const dsl::Library& L = dsl::library();
  for (const std::string& e : L.errors) { printf("FAIL: content-p1b library: %s\n", e.c_str()); bad++; }
  // ---- 1. the families
  std::map<std::string, int> bySource, byFamily;
  for (const dsl::Script& s : L.scripts) {
    if (s.family.empty()) continue;
    byFamily[s.family]++;
    bySource[s.source]++;
    if ((s.source == "req_village" || s.source == "req_town" || s.source == "req_city") && s.family != "request") { printf("FAIL: content-p1b: %s in %s is not family request\n", s.id.c_str(), s.source.c_str()); bad++; }
    if (s.source == "tales2" && s.family != "tale") { printf("FAIL: content-p1b: %s in tales2 is not family tale\n", s.id.c_str()); bad++; }
  }
  const int kinds = radiant::kindCount();
  printf("content-p1b: requests %d (village %d, town %d, city %d), tales %d, radiant job kinds %d, test scripts %d\n", byFamily["request"],
         bySource["req_village"], bySource["req_town"], bySource["req_city"], byFamily["tale"], kinds, byFamily["test"]);
  if (gates) {
    auto want = [&](int have, int n, const char* what) { if (have != n) { printf("FAIL: content-p1b: %d %s (want %d)\n", have, what, n); bad++; } };
    want(bySource["req_village"], 10, "village requests"); want(bySource["req_town"], 10, "town requests"); want(bySource["req_city"], 10, "city requests");
    want(byFamily["tale"], 20, "tales"); want(kinds, 15, "radiant job kinds");
  }
  // ---- per seed: names, text, play
  Names N;
  int scriptsPlayed = 0, jobsPlayed = 0, paths = 0;
  for (uint64_t seed = A; seed <= B; seed++) {
    g_curSeed = seed;
    Game g(seed);
    g.newEndlessGame(seed);
    g.mode = Mode::Play; g.godMode = true; g.noWildSpawns = true;
    steps(g, 60);
    Names Ns;
    measureNames(g, Ns);
    for (const auto& kv : Ns.m) {
      N.see(kv.first, std::string((size_t)kv.second, 'X'));
      int table = 0;
      for (const MaxLen& e : kMax) if (kv.first == e.key) table = e.len;
      if (!names && kv.second > table) { out("FAIL: content-p1b names: a %s of %d letters is longer than the table's %d (re-measure with --names)\n", kv.first.c_str(), kv.second, table); bad++; }
    }
    if (names) continue;
    if (seed == A) bad += gateChecks(g);
    // the library's P1b scripts
    for (const dsl::Script& s : L.scripts) {
      if (s.family.empty() || (!only.empty() && s.id.compare(0, only.size(), only) != 0)) continue;
      if (seed == A) bad += textCheck(s, N);
      const int site = hookSiteFor(g, s);
      if (site < 0) { out("FAIL: content-p1b %s: no settlement near the start fits `settle`\n", s.id.c_str()); bad++; continue; }
      bad += playScript(g, s, s.id, site, paths);
      scriptsPlayed++;
    }
    // every radiant job kind, every variant worth walking
    for (int k = 0; k < kinds; k++) {
      if (!only.empty() && std::string(radiant::kindId(k)).compare(0, only.size(), only) != 0) continue;
      const int nv = std::max(1, radiant::variantCount(k));
      int composed = 0;
      for (int v = 0; v < nv; v++) {
        const std::string id = radiant::composeVariant(g, k, v, g.world.startSite, seed * 7919u + (uint64_t)v);
        if (id.empty()) continue;
        const dsl::Script* s = scriptById(id);
        if (!s) { out("FAIL: content-p1b job %s: %s does not compose\n", radiant::kindId(k), id.c_str()); bad++; continue; }
        for (const std::string& p : dsl::validate(*s)) { out("FAIL: content-p1b job %s: %s\n", id.c_str(), p.c_str()); bad++; }
        if (s->family != "job") { out("FAIL: content-p1b job %s: not family job\n", id.c_str()); bad++; }
        bad += textCheck(*s, N);
        int site = g.world.startSite;
        if (!s->settle.empty()) site = hookSiteFor(g, *s);
        if (site < 0) continue;
        bad += playScript(g, *s, id, site, paths);
        composed++;
        jobsPlayed++;
      }
      if (!composed) { out("FAIL: content-p1b job %s: no variant composes on this world\n", radiant::kindId(k)); bad++; }
    }
  }
  if (names) {
    printf("content-p1b names (seeds %llu..%llu):", (unsigned long long)A, (unsigned long long)B);
    for (const auto& kv : N.m) printf(" %s %d,", kv.first.c_str(), kv.second);
    printf("\n");
    return 0;
  }
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  printf("content-p1b: seeds %llu..%llu: %d scripts and %d composed jobs played, %d paths walked (%.0f s); %d failures\n", (unsigned long long)A,
         (unsigned long long)B, scriptsPlayed, jobsPlayed, paths, secs, bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--content-p1b", "P1b: every new job, request and tale played through every branch; text with the longest names [--seeds A..B --gates --names --only id]", contentCmd);
