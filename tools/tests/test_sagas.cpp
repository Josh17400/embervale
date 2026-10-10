// rpg_test --sagas [N] [--seeds A..B] [--verbose] [--sample K] [--no-play]: M6b generated stories on real worlds
// (rpg/story/saga.h). COMPOSER lane. Per seed, N stories (default 40; CI: 2000 x 5 seeds = 10,000) are composed from the
// archetype library (every archetype in turn, twists drawn per slot, every voice and allowed motive), cast at a hook
// place of the world that HAS what the archetype needs (wars, famines and news are forced at the hook like rpg_test
// --story does; a grieving household and a hungry neighbour are made in the start town), and EVERY path of each is
// walked headlessly to an ending. Checks (15.20 "Quality bar and tests"):
//   - every composition is a valid script (parser + M4 validator), composes the same twice, and casts on the world
//   - every bound entity exists and is reachable: places load, positions lie on land, census residents live, named
//     beasts and world bosses are the foes generator's own, givers are real people
//   - every path ends (the journal quest closes), every ending of the script is reached, no "{" "}" "[[" "<<" "%" "@"
//     and no lower case in any text the player reads
//   - rewards stay inside the M6 bands: no `do loot`, no `do gold` above 40; every stage pays at most its rewards'
//     gold and any gear is uncommon / rare at the place's own item level
//   - uniqueness metrics: distinct (archetype, twists) pairs, (archetype, twists, cast shape) triples and story texts per
//     1,000 stories; phrase-family reuse. GATED once the library is full (>= 60 story archetypes): >= 900 distinct texts
//     and >= 300 distinct pairs per 1,000
//   - (the first seed) a play walk: offers drawn from the people and boards of 30 settlements over two months, accepting
//     some; no two offers within GUARD_RADIUS share archetype + twists (the repetition guard), offers stay rare
// The walker visits each (stage, variables) state once (a script's graph is a DAG under forced conditions; a cycle is
// reported), snapshots the realm only for scripts that touch it, and composes each spec once (saga::script's cache).
//
// rpg_test --saga-read N [--seeds S]: N stories printed in full for the WRITING-QUALITY REVIEW LENS: the cast, then
// every stage of the script (what is said, the journal, the options, the memories and facts it leaves) filled in with the
// real names of this world.
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
#include "rpg/sim/gear.h"
#include "rpg/sim/life.h"
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
bool g_sverbose = false;
bool g_trace = false;

void steps(Game& g, int n) {
  for (int i = 0; i < n; i++) { g.update(SIM_DT, Input()); g.events.clear(); }
}

// ---------------------------------------------------------------- snapshots (the walker restores between branches)
struct Snap {
  story::Engine st;
  realm::Realm rm;
  bool hasRealm = false;
  int gold = 0, plXp = 0, plLevel = 1, nextQuestId = 1, trackedQuest = -1, day = 1;
  std::vector<Item> inv;
  std::map<uint64_t, int32_t> marks;
  std::vector<Quest> quests;
};
Snap snap(const Game& g, bool realmToo) {
  Snap s;
  s.st = g.story; s.gold = g.gold; s.plXp = g.plXp; s.plLevel = g.plLevel; s.nextQuestId = g.nextQuestId;
  s.trackedQuest = g.trackedQuest; s.inv = g.inv; s.marks = g.marks; s.quests = g.quests; s.day = g.day;
  if (realmToo) { s.rm = g.realm; s.hasRealm = true; }
  return s;
}
void restore(Game& g, const Snap& s) {
  g.story = s.st; g.gold = s.gold; g.plXp = s.plXp; g.plLevel = s.plLevel; g.nextQuestId = s.nextQuestId;
  g.trackedQuest = s.trackedQuest; g.inv = s.inv; g.marks = s.marks; g.quests = s.quests; g.day = s.day;
  if (s.hasRealm) g.realm = s.rm;
  g.events.clear();
  g.mode = Mode::Play;
}

// does the script change the realm (effects, or goals the walker completes by forcing the realm)?
bool touchesRealm(const dsl::Script& s) {
  for (const dsl::StageDef& G : s.stages) {
    if (G.kind == dsl::StageKind::Goal && (G.goal.type == dsl::ObjType::War || G.goal.type == dsl::ObjType::Famine || G.goal.type == dsl::ObjType::Rep))
      return true;
    for (const dsl::EffDef& e : G.effs)
      if (e.type == dsl::EffType::Rep || e.type == dsl::EffType::Fame || e.type == dsl::EffType::Rumour ||
          (e.type >= dsl::EffType::RealmWar && e.type <= dsl::EffType::RealmEvent))
        return true;
  }
  return false;
}

// text the player reads: upper case, every placeholder filled, no composer markup left behind
const char* unreadable(const std::string& t) {
  for (char c : t) if (c >= 'a' && c <= 'z') return "lower case";
  if (t.find('{') != std::string::npos || t.find('}') != std::string::npos) return "an unfilled placeholder";
  if (t.find("[[") != std::string::npos || t.find("]]") != std::string::npos) return "a [[...]] left";
  if (t.find("<<") != std::string::npos || t.find(">>") != std::string::npos) return "a <<voice>> slot left";
  if (t.find('%') != std::string::npos || t.find('@') != std::string::npos) return "a %local or @target left";
  return nullptr;
}

std::string varsKey(const std::map<std::string, int32_t>& v) {
  std::string k;
  for (const auto& kv : v) { k += kv.first; k += '='; k += std::to_string(kv.second); k += ';'; }
  return k;
}

struct Walk {
  int nodes = 0, ends = 0, bad = 0;
  std::set<std::string> endNames, memo;
  size_t maxSay = 0, over300 = 0;
  std::string maxSayText;
};

// the stage just entered pays no more than its rewards and gifts, and its gear is in the place's band
void checkPay(Game& g, const story::Instance& in, const dsl::Script& sc, int gold0, size_t inv0, int D, const std::string& name, Walk& w) {
  if (in.stage < 0 || in.stage >= (int)sc.stages.size()) return;
  const dsl::StageDef& G = sc.stages[(size_t)in.stage];
  int allowGold = 0, items = 0;
  for (const dsl::EffDef& e : G.effs) {
    if (e.type == dsl::EffType::Reward) {
      const story::RewardRoll R = story::rewardRoll(D, e.n, 0, nullptr, 1);
      allowGold += R.gold;
      if (e.n >= 2) items++;
    }
    if (e.type == dsl::EffType::Gold && e.n > 0) allowGold += e.n;
    if (e.type == dsl::EffType::Give) items++;
  }
  const int got = g.gold - gold0;
  if (got > allowGold) { out("FAIL: saga %s stage %s pays %d gold (its rewards allow %d)\n", name.c_str(), G.name.c_str(), got, allowGold); w.bad++; }
  int newItems = 0;
  for (size_t i = inv0; i < g.inv.size(); i++) {
    const Item& it = g.inv[i];
    if (it.kind == ItemKind::Quest) continue;
    newItems++;
    if ((int)it.rarity > (int)Rarity::Rare) { out("FAIL: saga %s stage %s gives a %d-rarity piece (rare at most)\n", name.c_str(), G.name.c_str(), (int)it.rarity); w.bad++; }
    if (it.ilvl && gear::bandOf(it.ilvl) != gear::bandOf(D)) { out("FAIL: saga %s stage %s gives item level %d at danger %d (out of band)\n", name.c_str(), G.name.c_str(), it.ilvl, D); w.bad++; }
  }
  if (newItems > items) { out("FAIL: saga %s stage %s gives %d pieces (its rewards allow %d)\n", name.c_str(), G.name.c_str(), newItems, items); w.bad++; }
}

void walk(Game& g, uint32_t id, const dsl::Script& sc, const std::string& name, Walk& w, std::map<std::string, int>& onPath, int depth,
          bool realmToo, int D) {
  story::Instance* in = g.story.find(id);
  if (!in) { out("FAIL: saga %s: the instance vanished\n", name.c_str()); w.bad++; return; }
  const std::string st = g.story.stageName(*in);
  const std::string key = st + "|" + varsKey(in->vars);
  if (w.memo.count(key)) return;
  w.nodes++;
  for (const std::string& t : g.story.stageTexts(g, *in))
    if (const char* why = unreadable(t)) { out("FAIL: saga %s stage %s: %s in: %s\n", name.c_str(), st.c_str(), why, t.c_str()); w.bad++; }
  const std::string say = g.story.sayText(g, *in);
  if (say.size() > w.maxSay) { w.maxSay = say.size(); w.maxSayText = say; }
  if (say.size() > 300) w.over300++;
  if (in->done || g.story.isEnd(*in)) {
    w.memo.insert(key);
    w.ends++;
    w.endNames.insert(st);
    bool closed = false;
    for (const Quest& q : g.quests) if (q.id == in->questId && q.type == QType::Story && q.state == QState::Done) closed = true;
    // (fixer M6b r2) turned down at the opening: declined, the journal keeps no entry
    if (!in->questId && in->failed) closed = true;
    if (!closed) { out("FAIL: saga %s ended at %s but its journal quest is not done\n", name.c_str(), st.c_str()); w.bad++; }
    return;
  }
  if (onPath[st] > 0 || depth > 80) { out("FAIL: saga %s: a loop through stage %s\n", name.c_str(), st.c_str()); w.bad++; return; }
  w.memo.insert(key);
  onPath[st]++;
  const Snap s0 = snap(g, realmToo);
  if (g.story.isGoal(*in)) {
    const int gold0 = g.gold;
    const size_t inv0 = g.inv.size();
    if (!g.story.debugComplete(g, id)) { out("FAIL: saga %s stage %s: the objective cannot be completed\n", name.c_str(), st.c_str()); w.bad++; }
    else {
      if (const story::Instance* in2 = g.story.find(id)) checkPay(g, *in2, sc, gold0, inv0, D, name, w);
      walk(g, id, sc, name, w, onPath, depth + 1, realmToo, D);
    }
    restore(g, s0);
    onPath[st]--;
    return;
  }
  const int n = g.story.optionCount(*in);
  if (n <= 0) { out("FAIL: saga %s stage %s: a dialogue with no options\n", name.c_str(), st.c_str()); w.bad++; onPath[st]--; return; }
  // (which options are checks: read now, before any branch moves the instance on or a restore moves it in memory)
  std::vector<uint8_t> isCheck((size_t)n, 0);
  for (int i = 0; i < n; i++) isCheck[(size_t)i] = g.story.optionIsCheck(*in, i) ? 1 : 0;
  bool first = true;
  for (int i = 0; i < n; i++) {
    const bool check = isCheck[(size_t)i] != 0;
    for (int c = check ? 1 : 0; c <= (check ? 2 : 0); c++) {
      if (!first) restore(g, s0);
      first = false;
      const int gold0 = g.gold;
      const size_t inv0 = g.inv.size();
      if (!g.story.choose(g, id, i, true, c)) { out("FAIL: saga %s stage %s: option %d cannot be chosen\n", name.c_str(), st.c_str(), i); w.bad++; continue; }
      if (g_trace) { const story::Instance* t2 = g.story.find(id); printf("  trace %s -[%d/%d]-> %s\n", st.c_str(), i, c, t2 ? g.story.stageName(*t2).c_str() : "?"); }
      if (const story::Instance* in2 = g.story.find(id)) checkPay(g, *in2, sc, gold0, inv0, D, name, w);
      walk(g, id, sc, name, w, onPath, depth + 1, realmToo, D);
    }
  }
  restore(g, s0);
  onPath[st]--;
}

// ---------------------------------------------------------------- the cast is real
int checkCast(Game& g, const story::Instance& in, const dsl::Script& sc, const std::string& name) {
  int bad = 0;
  auto fail = [&](const std::string& m) { out("FAIL: saga %s: %s\n", name.c_str(), m.c_str()); bad++; };
  for (size_t ri = 0; ri < sc.roles.size() && ri < in.cast.size(); ri++) {
    const dsl::RoleDef& R = sc.roles[ri];
    const story::Binding& b = in.cast[ri];
    if (b.name.empty()) fail("role '" + R.name + "' has no name");
    if (b.hasPos && g.world.src) {
      if (std::abs(b.gx) > (1 << 28) || std::abs(b.gy) > (1 << 28)) fail("role '" + R.name + "' lies off the world");
      else if (R.kind == dsl::RoleKind::Site || R.kind == dsl::RoleKind::Ruin || R.kind == dsl::RoleKind::Beast || R.kind == dsl::RoleKind::Boss) {
        const ew::MacroSample m = g.world.src->macroFar(b.gx, b.gy);
        if (m.elev < ew::ELEV_SEA && !m.water) fail("role '" + R.name + "' (" + b.name + ") lies in the sea");
      }
    }
    switch (R.kind) {
      case dsl::RoleKind::Site: case dsl::RoleKind::Ruin: case dsl::RoleKind::Capital: {
        const ew::Gid sid = b.site ? b.site : b.id;
        if (story::siteByIdLoad(g, sid) < 0) fail("role '" + R.name + "' (" + b.name + ") is not a place that loads");
        break;
      }
      case dsl::RoleKind::Resident:
        if (b.trade == story::RESIDENT_CENSUS) {
          const int ix = story::residentIndexOf(g, b);
          const life::Census* c = g.life.find(b.site);
          if (ix < 0 || !c) fail("resident '" + R.name + "' (" + b.name + ") is not in the census");
          else if (c->res[(size_t)ix].flags & life::RF_DEAD) fail("resident '" + R.name + "' (" + b.name + ") is dead");
          else if (c->res[(size_t)ix].name != b.name) fail("resident '" + R.name + "': the census calls them " + c->res[(size_t)ix].name);
        } else if (R.a != "any") fail("resident '" + R.name + "' (" + R.a + ") was invented");
        break;
      case dsl::RoleKind::Beast: {
        bool found = false;
        for (const foes::NamedUnique& u : foes::namedInRegion(*g.world.src, ew::regionOf(b.gx), ew::regionOf(b.gy))) if (u.id == b.id) found = true;
        if (!found) fail("beast '" + R.name + "' (" + b.name + ") is not a named unique of its region");
        break;
      }
      case dsl::RoleKind::Boss: {
        bool ok = false;
        const foes::WorldBoss wb = foes::worldBossOf(*g.world.src, ew::EndlessSource::kcellOf(b.gx), ew::EndlessSource::kcellOf(b.gy), ok);
        if (!ok || wb.id != b.id) fail("boss '" + R.name + "' (" + b.name + ") is not its kingdom cell's world boss");
        break;
      }
      case dsl::RoleKind::Kingdom:
        if (!g.realm.kingdom(b.id) && !(g.world.src && g.world.src->kingdom(b.id))) fail("kingdom '" + R.name + "' is unknown");
        break;
      default: break;
    }
  }
  return bad;
}

// ---------------------------------------------------------------- hooks that have what the archetype needs
// the world events an event story needs, forced at its hook place (as rpg_test --story does)
void forceHookEvent(Game& g, const dsl::Script& s, int site) {
  if (s.hook != dsl::HookKind::Event || site < 0) return;
  const Site S = g.world.sites[(size_t)site];
  const int32_t hx = g.world.ox + S.ex, hy = g.world.oy + S.ey;
  const ew::Gid land = story::landAt(g, hx, hy);
  const ew::Gid home = S.homeKingdom >= 0 ? g.world.kingdoms[(size_t)S.homeKingdom].id : 0;
  g.realm.noteSite(S.id, home, (uint8_t)S.type, hx, hy);
  if (s.hookEv == (int)realm::EvType::Famine) g.realm.forceFamine(S.id, g.day);
  else if (s.hookEv == (int)realm::EvType::WarDeclared) {
    ew::Gid other = 0;
    for (const realm::KingdomState& K : g.realm.kingdoms()) if (K.id != land && !K.fallen) { other = K.id; break; }
    if (other && land) {
      if (!g.realm.atWar(other, land)) g.realm.forceWar(other, land, g.day);
      g.realm.forceEvent(realm::EvType::WarDeclared, other, land, S.id, hx, hy, g.day);
    }
  } else g.realm.forceEvent((realm::EvType)s.hookEv, land, 0, S.id, hx, hy, g.day);
}

// the living world the library asks for, made at the start town once per seed: a war of its kingdom, a famine, a
// household in mourning, a hungry neighbour (every census of the loaded window is built)
void makeWorldFacts(Game& g) {
  const int ss = g.world.startSite;
  if (ss < 0) return;
  const Site S = g.world.sites[(size_t)ss];
  const int32_t hx = g.world.ox + S.ex, hy = g.world.oy + S.ey;
  const ew::Gid land = story::landAt(g, hx, hy);
  ew::Gid other = 0;
  double bd = 1e30;
  for (const realm::KingdomState& K : g.realm.kingdoms()) {
    if (K.id == land || K.fallen) continue;
    const ew::KingdomPlan* kp = g.world.src->kingdom(K.id);
    const double d = kp ? std::hypot((double)(kp->gx - hx), (double)(kp->gy - hy)) : 1e20;
    if (d < bd) { bd = d; other = K.id; }
  }
  if (other && land && !g.realm.atWar(other, land)) g.realm.forceWar(other, land, g.day);
  // every settlement of the loaded window: a famine, a death in a household of three or more (its kin and friends
  // grieve), and a hungry, penniless neighbour
  std::vector<int> towns = {ss};
  for (int si : g.world.nearSites)
    if (si >= 0 && si < (int)g.world.sites.size() && si != ss && g.world.sites[(size_t)si].settlement()) towns.push_back(si);
  for (int si : towns) {
    const Site T = g.world.sites[(size_t)si];
    const ew::Gid home = T.homeKingdom >= 0 ? g.world.kingdoms[(size_t)T.homeKingdom].id : 0;
    g.realm.noteSite(T.id, home, (uint8_t)T.type, g.world.ox + T.ex, g.world.oy + T.ey);
    g.realm.forceFamine(T.id, g.day);
    life::Census* C = g.life.census(g.world, si);
    if (!C) continue;
    std::map<int, int> hh;
    for (const life::Resident& r : C->res) if (!(r.flags & life::RF_DEAD)) hh[r.household]++;
    int died = -1;
    for (const life::Resident& r : C->res)
      if (!(r.flags & (life::RF_DEAD | life::RF_AWAY)) && r.age >= 30 && hh[r.household] >= 3 && r.actor < 0) { died = r.idx; break; }
    if (died >= 0) g.life.residentDied(T.id, died, g.day);
    if (life::Census* M = g.life.findMut(T.id))
      for (life::Resident& r : M->res)
        if (!(r.flags & (life::RF_DEAD | life::RF_AWAY | life::RF_GRIEVING)) && r.age >= 20) { r.need[(size_t)life::Need::Hunger] = 20; r.need[(size_t)life::Need::Money] = 15; break; }
    if (g_sverbose) {
      int grieving = 0, ties = 0, actorsHere = 0, residents = 0;
      std::map<int, int> roles;
      for (const life::Resident& r : C->res) if (r.flags & life::RF_GRIEVING) grieving++;
      for (const life::Tie& t : C->ties) if (t.b != life::PLAYER_TIE) ties++;
      for (const Actor& a : g.actors) if (a.npc && a.human && story::homeSiteOf(g, a) == si) { actorsHere++; roles[(int)a.role]++; if (a.resident >= 0) residents++; }
      std::string rl;
      for (const auto& kv : roles) rl += std::string(" ") + dsl::tradeName(kv.first) + "x" + std::to_string(kv.second);
      printf("world facts at %s: %zu residents, %d grieving, %d ties; %d people out (%d residents):%s\n", T.name.c_str(), C->res.size(), grieving, ties,
             actorsHere, residents, rl.c_str());
    }
  }
}

bool tradeFits(int trade, Role r) {
  if (r == Role::Child || r == Role::King || r == Role::Herald) return false;
  if (trade == -2 || trade < 0) return r != Role::Guard && r != Role::Jarl;
  return trade == (int)r;
}

// cast `id` at a hook place of this world that has what it needs; the instance id (0: none fits, why says so)
uint32_t startAtFittingHook(Game& g, const std::string& id, const dsl::Script& sc, uint32_t needs, std::string& why) {
  const int32_t gx = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), gy = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
  std::vector<int> sites;
  auto add = [&](int h) { if (h >= 0 && std::find(sites.begin(), sites.end(), h) == sites.end()) sites.push_back(h); };
  switch (sc.hook) {
    case dsl::HookKind::Herald: {
      const ew::Gid land = story::landAt(g, gx, gy);
      if (const realm::KingdomState* K = g.realm.kingdom(land)) add(story::siteByIdLoad(g, K->capital));
      add(g.world.findSiteNear(gx, gy, SiteType::City, 4, true));
      break;
    }
    case dsl::HookKind::Ruin:
      add(g.world.findSiteNear(gx, gy, SiteType::Ruin, 3));
      for (int si : g.world.nearSites) if (si >= 0 && g.world.sites[(size_t)si].type == SiteType::Ruin) add(si);
      break;
    case dsl::HookKind::Board:
      add(g.world.findSiteNear(gx, gy, SiteType::Town, 3));
      add(g.world.findSiteNear(gx, gy, SiteType::City, 4));
      for (int si : g.world.nearSites) if (si >= 0 && (g.world.sites[(size_t)si].type == SiteType::Town || g.world.sites[(size_t)si].type == SiteType::City)) add(si);
      break;
    default:
      add(g.world.startSite);
      for (int si : g.world.nearSites) if (si >= 0 && g.world.sites[(size_t)si].settlement()) add(si);
      break;
  }
  why = "no hook place";
  std::vector<std::string> reasons;
  auto keep = [&]() { if (why != "no hook place" && reasons.size() < 3 && (reasons.empty() || reasons.back() != why)) reasons.push_back(why); };
  for (int site : sites) {
    keep();
    Hook hk;
    hk.kind = (int)sc.hook;
    hk.site = site;
    hk.ev = sc.hook == dsl::HookKind::Event ? sc.hookEv : -1;
    forceHookEvent(g, sc, site);
    const uint32_t miss = needsMissing(g, hk, needs);
    if (miss) {
      why = std::string(g.world.sites[(size_t)site].name) + " lacks";
      for (uint32_t b = 1; b && b <= N_EVENT; b <<= 1) if (miss & b) why += std::string(" ") + needName(b);
      continue;
    }
    if (sc.hook == dsl::HookKind::Npc || sc.hook == dsl::HookKind::Herald) {
      int tried = 0;
      // (census residents first: kin and friends of the giver need a household)
      for (size_t k2 = 1; k2 < 2 * g.actors.size() && tried < 16; k2++) {
        const size_t k = k2 % g.actors.size();
        const bool residentPass = k2 < g.actors.size();
        if (k == 0) continue;
        const Actor& a = g.actors[k];
        if ((a.resident >= 0) != residentPass) continue;
        if (!a.npc || !a.human || story::isStoryPerson(a) || story::homeSiteOf(g, a) != site || a.st == AState::Dead) continue;
        if (sc.hook == dsl::HookKind::Herald ? a.role != Role::Herald : !tradeFits(sc.hookTrade, a.role)) continue;
        tried++;
        std::string w;
        if (const uint32_t inst = g.story.start(g, id, a.id, site, &w)) return inst;
        why = w;
      }
      // the census's people of that trade who are not out in the street now (the innkeeper behind the bar, the priest
      // at prayer, the farmer in the far field): their own body is brought into play beside the player, as the life
      // simulation does when the player comes near them (a resident's street identity: slot LIFE_SLOT0 + index)
      if (sc.hook == dsl::HookKind::Npc && story::host().spawnHuman && g.world.sites[(size_t)site].settlement()) {
        life::Census* C = g.life.census(g.world, site);
        for (size_t ri = 0; C && ri < C->res.size() && tried < 32; ri++) {
          const life::Resident r = C->res[ri];
          if ((r.flags & (life::RF_DEAD | life::RF_AWAY)) || r.age < 16 || !tradeFits(sc.hookTrade, life::jobRole(r.job))) continue;
          if (r.actor >= 0 && story::host().findActor(g, r.actor) >= 0) continue;   // (out already: tried above)
          tried++;
          Spawn sp;
          sp.npc = true;
          sp.role = life::jobRole(r.job);
          sp.site = site;
          sp.slot = 3000 + (int)ri;
          const int aid = story::host().spawnHuman(g, sp, g.pl().p.x + 24.0f, g.pl().p.y);
          const int ai = story::host().findActor(g, aid);
          if (ai < 0) continue;
          Actor& a = g.actors[(size_t)ai];
          a.site = site; a.bldg = -1; a.slot = 3000 + (int)ri; a.resident = (int)ri; a.name = r.name; a.fromMap = false;
          C->res[ri].actor = aid;
          std::string w;
          const uint32_t inst = g.story.start(g, id, aid, site, &w);
          // (the body goes again: the walk needs only its cast)
          C = g.life.census(g.world, site);
          if (C && ri < C->res.size()) C->res[ri].actor = -1;
          const int ai2 = story::host().findActor(g, aid);
          if (ai2 >= 0) g.actors.erase(g.actors.begin() + ai2);
          if (inst) return inst;
          why = w;
        }
      }
      if (sc.hook == dsl::HookKind::Npc) { if (!tried) why = std::string("no ") + (sc.hookTrade >= 0 ? dsl::tradeName(sc.hookTrade) : "TALKER") + " to tell it in " + g.world.sites[(size_t)site].name; continue; }
    }
    std::string w;
    if (const uint32_t inst = g.story.start(g, id, -1, site, &w)) return inst;
    why = w;
  }
  keep();
  if (!reasons.empty()) { why.clear(); for (const std::string& r : reasons) why += (why.empty() ? "" : "; ") + r; }
  return 0;
}

// ... and when nothing in the loaded window fits (a teller with no friends, no priest in this hamlet), the next towns
// along: the player travels there (the window follows) and the world's facts are made there too
uint32_t startAnywhere(Game& g, const std::string& id, uint32_t needs, std::string& why) {
  const dsl::Script* sc = story::scriptById(id);
  if (!sc) { why = "no script"; return 0; }
  if (const uint32_t inst = startAtFittingHook(g, id, *sc, needs, why)) return inst;
  const int32_t px = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), py = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
  std::vector<ew::SettlementNode> towns = g.world.src->settlementsIn(px - 1400, py - 1400, px + 1400, py + 1400, true);
  std::sort(towns.begin(), towns.end(), [&](const ew::SettlementNode& a, const ew::SettlementNode& b) {
    const int64_t da = (int64_t)(a.x - px) * (a.x - px) + (int64_t)(a.y - py) * (a.y - py), db = (int64_t)(b.x - px) * (b.x - px) + (int64_t)(b.y - py) * (b.y - py);
    return da != db ? da < db : a.id < b.id;
  });
  const std::string first = why;
  int moves = 0;
  for (const ew::SettlementNode& T : towns) {
    if (moves >= 5) break;
    const int64_t d2 = (int64_t)(T.x - px) * (T.x - px) + (int64_t)(T.y - py) * (T.y - py);
    if (d2 < 200 * 200) continue;   // (in the window already)
    moves++;
    g.teleportGlobal(T.x, T.y + 2);
    steps(g, 30);
    makeWorldFacts(g);
    sc = story::scriptById(id);
    if (!sc) break;
    if (const uint32_t inst = startAtFittingHook(g, id, *sc, needs, why)) return inst;
  }
  why = first + " (and " + std::to_string(moves) + " towns further: " + why + ")";
  return 0;
}

// the i-th spec of a seed's run: every tier-2 archetype in turn, a twist drawn for each slot (or none), every voice
Spec specFor(uint64_t seed, int i, const std::vector<const Archetype*>& arch) {
  Spec s;
  const Archetype& a = *arch[(size_t)i % arch.size()];
  uint64_t r = ew::mix64(seed * 0x9E3779B97F4A7C15ull + (uint64_t)i);
  s.arch = a.id;
  s.voice = (int)(r % 8);
  r = ew::mix64(r);
  std::vector<Motive> ms;
  for (int m = 0; m < (int)Motive::COUNT; m++) if (!a.motives || (a.motives & motiveBit((Motive)m))) ms.push_back((Motive)m);
  s.motive = ms[(size_t)(r % ms.size())];
  r = ew::mix64(r);
  s.seed = (uint32_t)r;
  const std::map<std::string, std::string> slots = slotsOf(a.body);
  std::vector<std::string> chosen;
  for (int k = 0; k < 3; k++) {
    if (!slots.count("t" + std::to_string(k + 1))) continue;
    r = ew::mix64(r);
    if (r % 4 == 0) continue;   // a quarter of the slots stay empty
    std::vector<const Twist*> fit;
    for (const Twist& t : twists()) if (twistFitsSlot(a, t, k, chosen)) fit.push_back(&t);
    if (fit.empty()) continue;
    r = ew::mix64(r);
    const Twist* t = fit[(size_t)(r % fit.size())];
    s.twist[k] = t->id;
    chosen.push_back(t->id);
  }
  return s;
}

uint32_t needsOf(const Spec& s) {
  uint32_t n = 0;
  if (const Archetype* a = archetype(s.arch)) n |= a->needs;
  for (int k = 0; k < 3; k++) if (const Twist* t = twist(s.twist[k])) n |= t->needs;
  return n;
}

Game* freshWorld(uint64_t seed) {
  Game* g = new Game(seed);
  g->newEndlessGame(seed);
  g->mode = Mode::Play;
  g->godMode = true;
  g->noWildSpawns = true;
  steps(*g, 90);
  makeWorldFacts(*g);
  return g;
}

// ---------------------------------------------------------------- the play walk: offers across settlements
int playWalk(uint64_t seed, int settlementsN) {
  int bad = 0;
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.godMode = true;
  g.noWildSpawns = true;
  steps(g, 60);
  struct Rec { Spec s; int32_t gx, gy; uint32_t hook; int day; std::string id, where; };
  std::vector<Rec> recs;
  const int32_t sx = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), sy = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
  std::vector<ew::SettlementNode> towns = g.world.src->settlementsIn(sx - 2600, sy - 2600, sx + 2600, sy + 2600, true);
  std::sort(towns.begin(), towns.end(), [&](const ew::SettlementNode& a, const ew::SettlementNode& b) {
    const int64_t da = (int64_t)(a.x - sx) * (a.x - sx) + (int64_t)(a.y - sy) * (a.y - sy), db = (int64_t)(b.x - sx) * (b.x - sx) + (int64_t)(b.y - sy) * (b.y - sy);
    return da != db ? da < db : a.id < b.id;
  });
  if ((int)towns.size() > settlementsN) towns.resize((size_t)settlementsN);
  int asked = 0, offered = 0, accepted = 0, boards = 0, boardOffers = 0, visited = 0;
  for (size_t ti = 0; ti < towns.size(); ti++) {
    const ew::SettlementNode& T = towns[ti];
    g.teleportGlobal(T.x, T.y + 2);
    steps(g, 30);
    g.day += 2;   // (two days on the road between towns)
    const int si = story::siteByIdLoad(g, T.id);
    if (si < 0) continue;
    visited++;
    auto record = [&](const std::string& id, const Hook& hk, const std::string& where) {
      Spec s;
      if (!parseSpecId(id, s)) { out("FAIL: play walk: offer %s is not a spec id\n", id.c_str()); bad++; return; }
      int32_t hx = 0, hy = 0;
      const uint32_t hh = hookHashOf(g, hk, hx, hy);
      recs.push_back({s, hx, hy, hh, g.day, id, where});
      noteOffered(g, hk, id);
      // some offers are taken (and told at once: the walker does not play them out)
      if ((ew::mix64(hh ^ (uint64_t)g.day) % 3) == 0) {
        const dsl::Script* sc = story::scriptById(id);
        const int actor = hk.actor;
        std::string why;
        const uint32_t inst = sc ? g.story.start(g, id, actor, hk.site, &why) : 0;
        if (!inst) { out("FAIL: play walk: the offer %s at %s cannot be begun: %s\n", id.c_str(), where.c_str(), why.c_str()); bad++; return; }
        noteTold(g, id, inst, hk);
        g.story.drop(inst);
        accepted++;
      }
    };
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& a = g.actors[k];
      if (!a.npc || !a.human || story::isStoryPerson(a) || story::homeSiteOf(g, a) != si) continue;
      asked++;
      const std::string id = offerFor(g, a);
      if (id.empty()) continue;
      offered++;
      Hook hk;
      hk.kind = (int)dsl::HookKind::Npc;
      hk.actor = a.id;
      hk.site = si;
      // (asking twice gives the same pitch)
      if (offerFor(g, a) != id) { out("FAIL: play walk: %s offers a different story when asked again\n", a.name.c_str()); bad++; }
      record(id, hk, a.name + " of " + T.name);
    }
    if (story::isBoardSite(g, si)) {
      boards++;
      const std::string id = offerForPlace(g, (int)dsl::HookKind::Board, si);
      if (!id.empty()) {
        boardOffers++;
        Hook hk;
        hk.kind = (int)dsl::HookKind::Board;
        hk.site = si;
        record(id, hk, "the board of " + T.name);
      }
    }
  }
  // the guard: no two offers within GUARD_RADIUS share archetype + twists (unless one hook repeated its own pitch)
  int pairsNear = 0;
  for (size_t i = 0; i < recs.size(); i++)
    for (size_t j = i + 1; j < recs.size(); j++) {
      const int64_t dx = recs[i].gx - recs[j].gx, dy = recs[i].gy - recs[j].gy;
      if (dx * dx + dy * dy > (int64_t)Guard::GUARD_RADIUS * Guard::GUARD_RADIUS) continue;
      pairsNear++;
      const Spec& a = recs[i].s;
      const Spec& b = recs[j].s;
      if (a.arch == b.arch && a.twist[0] == b.twist[0] && a.twist[1] == b.twist[1] && a.twist[2] == b.twist[2] && recs[i].hook != recs[j].hook) {
        out("FAIL: play walk: %s (%s, day %d) and %s (%s, day %d) share archetype and twists within %d tiles\n", recs[i].id.c_str(),
            recs[i].where.c_str(), recs[i].day, recs[j].id.c_str(), recs[j].where.c_str(), recs[j].day, (int)Guard::GUARD_RADIUS);
        bad++;
      }
    }
  std::set<std::string> archs;
  for (const Rec& r : recs) archs.insert(r.s.arch);
  printf("sagas play walk seed %llu: %d settlements over %d days, %d people asked, %d offered a story (%.1f %%), %d boards (%d offers), "
         "%d taken; %zu archetypes; %d offer pairs within %d tiles checked\n",
         (unsigned long long)seed, visited, visited * 2, asked, offered, asked ? 100.0 * offered / asked : 0.0, boards, boardOffers, accepted,
         archs.size(), pairsNear, (int)Guard::GUARD_RADIUS);
  return bad;
}

int sagasCmd(int argc, char** argv) {
  uint64_t A = 1, B = 3;
  int N = 40, sample = 0;
  bool play = true;
  std::string only;
  for (int i = 2; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], A, B);
    else if (!strcmp(argv[i], "--verbose")) g_sverbose = true;
    else if (!strcmp(argv[i], "--no-play")) play = false;
    else if (!strcmp(argv[i], "--trace")) g_trace = true;
    else if (!strcmp(argv[i], "--only") && i + 1 < argc) only = argv[++i];
    else if (!strcmp(argv[i], "--sample") && i + 1 < argc) sample = atoi(argv[++i]);
    else if (argv[i][0] >= '0' && argv[i][0] <= '9') N = atoi(argv[i]);
  }
  std::vector<const Archetype*> arch;
  for (const Archetype& a : archetypes()) if (a.tier == 2 && (only.empty() || only == a.id)) arch.push_back(&a);
  if (arch.empty()) { printf("FAIL: sagas: no story archetypes\n"); return 1; }
  int bad = 0, stories = 0, cast = 0, nodes = 0, ends = 0;
  size_t maxSay = 0, over300 = 0;
  std::string maxSayText;
  std::set<std::string> pairs, triples, texts;
  long phraseUses = 0, phraseRepeats = 0;
  double castMs = 0, walkMs = 0, composeMs = 0;
  auto t0 = std::chrono::steady_clock::now();
  for (uint64_t seed = A; seed <= B; seed++) {
    g_curSeed = seed;
    Game* gp = freshWorld(seed);
    Game& g = *gp;
    std::set<uint32_t> famSeen;
    int seedBad = 0;
    for (int i = 0; i < N; i++) {
      const Spec s = specFor(seed, i, arch);
      const std::string id = specId(s);
      stories++;
      auto c0 = std::chrono::steady_clock::now();
      Composed c, c2;
      const bool ok = compose(s, c);
      compose(s, c2);
      if (c.text != c2.text) { out("FAIL: saga %s composes differently twice\n", id.c_str()); seedBad++; }
      if (!ok) {
        out("FAIL: saga %s does not compose:\n", id.c_str());
        for (size_t k = 0; k < c.errors.size() && k < 5; k++) out("    %s\n", c.errors[k].c_str());
        seedBad++;
        continue;
      }
      texts.insert(c.text.substr(c.text.find('\n')));
      for (uint32_t f : c.families) { phraseUses++; if (!famSeen.insert(f).second) phraseRepeats++; }
      const dsl::Script* sc = script(id);
      composeMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - c0).count();
      if (!sc) { out("FAIL: saga %s: script() refused what compose() accepted\n", id.c_str()); seedBad++; continue; }
      // rewards inside the bands
      for (const dsl::StageDef& G : sc->stages)
        for (const dsl::EffDef& e : G.effs) {
          if (e.type == dsl::EffType::Loot) { out("FAIL: saga %s: `do loot` in a composed story (use reward)\n", id.c_str()); seedBad++; }
          if (e.type == dsl::EffType::Gold && e.n > 40) { out("FAIL: saga %s: `do gold %d` (over 40: use reward)\n", id.c_str(), e.n); seedBad++; }
        }
      const bool realmToo = touchesRealm(*sc);
      const Snap s0 = snap(g, true);
      auto c1 = std::chrono::steady_clock::now();
      std::string why;
      const uint32_t inst = startAnywhere(g, id, needsOf(s), why);
      castMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - c1).count();
      if (!inst) { out("FAIL: saga %s cannot be cast: %s\n", id.c_str(), why.c_str()); seedBad++; restore(g, s0); continue; }
      cast++;
      sc = script(id);   // (the game ran while the hook was sought: the cache may have moved)
      if (!sc) { out("FAIL: saga %s: lost from the cache while it runs\n", id.c_str()); seedBad++; restore(g, s0); continue; }
      pairs.insert(s.arch + "|" + s.twist[0] + "|" + s.twist[1] + "|" + s.twist[2]);
      {
        std::string shape;
        for (const story::Binding& b : g.story.find(inst)->cast) { shape += std::to_string(b.kind) + "." + std::to_string(b.trade) + ","; }
        triples.insert(s.arch + "|" + s.twist[0] + "|" + s.twist[1] + "|" + s.twist[2] + "|" + shape);
      }
      seedBad += checkCast(g, *g.story.find(inst), *sc, id);
      const int D = story::storyDanger(g, *g.story.find(inst));
      if (sample > 0 && (int)(stories - 1) % std::max(1, N * (int)(B - A + 1) / sample) == 0) {
        const story::Instance* in = g.story.find(inst);
        printf("--- %s  \"%s\" (giver %s)\n", id.c_str(), sc->title.c_str(), in && !in->cast.empty() ? in->cast[0].name.c_str() : "?");
      }
      auto c2t = std::chrono::steady_clock::now();
      Walk w;
      std::map<std::string, int> onPath;
      walk(g, inst, *sc, id, w, onPath, 0, realmToo, D);
      walkMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - c2t).count();
      nodes += w.nodes;
      ends += w.ends;
      seedBad += w.bad;
      if (w.maxSay > maxSay) { maxSay = w.maxSay; maxSayText = w.maxSayText; }
      over300 += w.over300;
      for (const dsl::StageDef& G : sc->stages)
        if (G.kind == dsl::StageKind::End && !w.endNames.count(G.name)) { out("FAIL: saga %s: no path reaches the ending '%s'\n", id.c_str(), G.name.c_str()); seedBad++; }
      if (g_sverbose) out("saga seed %llu: %s cast, %d states, %d endings reached\n", (unsigned long long)seed, id.c_str(), w.nodes, (int)w.endNames.size());
      restore(g, s0);
      g.story.drop(inst);
      trimCache(g.story);
    }
    printf("sagas seed %llu: %d stories, %d failures\n", (unsigned long long)seed, N, seedBad);
    bad += seedBad;
    delete gp;
  }
  if (play) bad += playWalk(A, 30);
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  const double per1000 = stories ? 1000.0 / stories : 0;
  printf("sagas: %d composed, %d cast, %d story states walked, %d endings reached, %.0f ms (%.2f ms a story: compose %.2f, cast %.2f, walk %.2f)\n",
         stories, cast, nodes, ends, ms, stories ? ms / stories : 0.0, stories ? composeMs / stories : 0.0, stories ? castMs / stories : 0.0,
         stories ? walkMs / stories : 0.0);
  const double pairsK = pairs.size() * per1000, textsK = texts.size() * per1000;
  printf("  uniqueness: %.0f distinct (archetype, twists) pairs, %.0f (archetype, twists, cast shape) triples and %.0f distinct texts per "
         "1,000 stories; phrase-family reuse %.1f %%\n",
         pairsK, triples.size() * per1000, textsK, phraseUses ? 100.0 * phraseRepeats / phraseUses : 0.0);
  printf("  the longest line said: %zu characters (%zu lines over 300)%s%s\n", maxSay, over300, g_sverbose ? ": " : "", g_sverbose ? maxSayText.c_str() : "");
  const LibraryReport lr = report();
  if (lr.tier2 >= 60 && stories >= 1000) {
    if (textsK < 900) { printf("FAIL: sagas: %.0f distinct texts per 1,000 stories (the bar: 900)\n", textsK); bad++; }
    if (pairsK < 300) { printf("FAIL: sagas: %.0f distinct (archetype, twists) pairs per 1,000 stories (the bar: 300)\n", pairsK); bad++; }
  } else {
    printf("  (uniqueness not gated yet: %d story archetypes of the 60 that make the library full, %d stories)\n", lr.tier2, stories);
  }
  printf("sagas: %d failures\n", bad);
  return bad ? 1 : 0;
}

// ---------------------------------------------------------------- the writing-quality review lens
void printStory(Game& g, uint32_t inst, const dsl::Script& sc, const std::string& id) {
  const story::Instance* in = g.story.find(inst);
  if (!in) return;
  printf("=====================================================================================================\n");
  printf("%s\n\"%s\"  (archetype: %s)\n", id.c_str(), sc.title.c_str(), sc.archetype.c_str());
  printf("PITCH: %s\n", sc.pitch.c_str());
  if (!sc.hint.empty()) printf("HINT: %s\n", sc.hint.c_str());
  printf("CAST:");
  for (size_t ri = 0; ri < in->cast.size() && ri < sc.roles.size(); ri++) {
    const story::Binding& b = in->cast[ri];
    std::string extra;
    if (sc.roles[ri].kind == dsl::RoleKind::Resident) extra = " [" + story::fillText(g, *in, "{" + b.role + ".job}") + ", " + story::fillText(g, *in, "{" + b.role + ".kin}") + "]";
    printf(" %s=%s%s;", b.role.c_str(), b.name.c_str(), extra.c_str());
  }
  printf("\n");
  for (const dsl::StageDef& G : sc.stages) {
    std::string kind;
    if (G.kind == dsl::StageKind::Talk) kind = "talk " + G.talk;
    else if (G.kind == dsl::StageKind::Goal) kind = "goal -> " + G.then;
    else kind = G.success ? "END (success)" : "END (failure)";
    printf("  [%s] %s\n", G.name.c_str(), kind.c_str());
    if (!G.say.empty()) printf("      SAY: %s\n", story::fillText(g, *in, G.say).c_str());
    if (!G.journal.empty()) printf("      JOURNAL: %s\n", story::fillText(g, *in, G.journal).c_str());
    for (const dsl::OptDef& o : G.opts)
      printf("      > %s  -> %s%s\n", story::fillText(g, *in, o.label).c_str(), o.to.c_str(), o.check ? (" / else " + o.orElse).c_str() : "");
    for (const dsl::EffDef& e : G.effs) {
      if (e.type == dsl::EffType::Remember) printf("      (%s remembers: %s)\n", e.args[0].c_str(), story::fillText(g, *in, e.text).c_str());
      else if (e.type == dsl::EffType::Fact) printf("      (the town says: %s)\n", story::fillText(g, *in, e.text).c_str());
      else if (e.type == dsl::EffType::Reward) printf("      (reward %s)\n", e.n == 0 ? "small" : e.n == 1 ? "fair" : e.n == 2 ? "rich" : "great");
      else if (e.type == dsl::EffType::Give) printf("      (gives: %s)\n", story::fillText(g, *in, e.text).c_str());
      else if (e.type == dsl::EffType::Secret) printf("      (a smithing secret of %s)\n", e.args[0].c_str());
    }
  }
}

int sagaReadCmd(int argc, char** argv) {
  uint64_t A = 1, B = 1;
  int N = 10;
  for (int i = 2; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], A, B);
    else if (argv[i][0] >= '0' && argv[i][0] <= '9') N = atoi(argv[i]);
  }
  std::vector<const Archetype*> arch;
  for (const Archetype& a : archetypes()) if (a.tier == 2) arch.push_back(&a);
  if (arch.empty()) { printf("no story archetypes\n"); return 1; }
  int printed = 0, failed = 0;
  for (uint64_t seed = A; seed <= B && printed < N; seed++) {
    Game* gp = freshWorld(seed);
    Game& g = *gp;
    const int per = (int)((N - printed + (int)(B - seed)) / (int)(B - seed + 1));
    for (int i = 0; i < per && printed < N; i++) {
      // a random story of the library (not the archetypes in order: the review samples the whole library)
      const uint64_t r = ew::mix64(seed * 7919u + (uint64_t)i * 104729u);
      const Spec s = specFor(seed ^ 0xBEEF, (int)(r % 1000003u), arch);
      const std::string id = specId(s);
      const dsl::Script* sc = script(id);
      if (!sc) { printf("(%s does not compose)\n", id.c_str()); failed++; continue; }
      const Snap s0 = snap(g, true);
      std::string why;
      const uint32_t inst = startAnywhere(g, id, needsOf(s), why);
      if (!inst) { printf("(%s cannot be cast: %s)\n", id.c_str(), why.c_str()); failed++; restore(g, s0); continue; }
      sc = script(id);
      if (sc) printStory(g, inst, *sc, id);
      printed++;
      restore(g, s0);
      g.story.drop(inst);
    }
    delete gp;
  }
  printf("saga-read: %d stories printed, %d could not be told\n", printed, failed);
  return failed ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--sagas", "M6b generated stories: compose N per seed on real worlds, cast them, walk every path; rewards, the guard, uniqueness [N] [--seeds A..B] [--sample K] [--no-play] [--verbose]", sagasCmd);
RPG_TEST_CMD("--saga-read", "M6b writing review lens: print N composed stories in full, cast on real worlds [N] [--seeds A..B]", sagaReadCmd);
