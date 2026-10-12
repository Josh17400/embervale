// M8 Fellowship, VOICE lane: a companion's personal quest chosen and begun through the M6b saga composer
// (rpg/sim/party_voice.h questFor / questBegin / questStatus; the archetypes: rpg/story/arch/companion.cpp).
//
// The choice is deterministic in (world seed, the member's id and stake seed): the archetypes whose `hook companion
// <stake> [<fact>]` matches the member's Stake::hook and history are tried in a hashed order, each with a voice from
// their culture, a motive their stake gives, twists drawn per slot, and every random draw its own statement (clang and
// MSVC agree); the first that composes and CASTS from the companion's real history is the one. Nothing is offered
// that cannot be told.
#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/party.h"
#include "rpg/sim/party_voice.h"
#include "rpg/story/dsl.h"
#include "rpg/story/saga.h"
#include "rpg/story/story.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/ids.h"

namespace party {
namespace voice {

namespace {

using story::saga::Archetype;
using story::saga::Motive;
using story::saga::Spec;
using story::saga::Twist;

uint64_t stepRng(uint64_t& s) {
  s += 0x9E3779B97F4A7C15ull;
  return ew::mix64(s);
}

// the `hook companion <stake> [<fact>]` of an archetype's template: stake 1..10 (0: not a companion story), fact
struct CompHook { int stake = 0; int fact = 0; };
CompHook compHookOf(const Archetype& a) {
  static std::map<std::string, CompHook> memo;
  auto it = memo.find(a.id);
  if (it != memo.end()) return it->second;
  CompHook h;
  const std::string body = a.body ? a.body : "";
  size_t p = 0;
  while (p < body.size()) {
    size_t e = body.find('\n', p);
    if (e == std::string::npos) e = body.size();
    std::string l = body.substr(p, e - p);
    p = e + 1;
    const size_t s0 = l.find_first_not_of(" \t");
    if (s0 == std::string::npos) continue;
    l = l.substr(s0);
    if (l.compare(0, 15, "hook companion ") != 0) continue;
    std::vector<std::string> w;
    std::string cur;
    for (char c : l.substr(15)) {
      if (c == ' ' || c == '\t' || c == '\r') { if (!cur.empty()) w.push_back(cur); cur.clear(); }
      else cur += c;
    }
    if (!cur.empty()) w.push_back(cur);
    if (!w.empty()) h.stake = std::max(0, story::dsl::companionHookWord(w[0]));
    if (w.size() > 1) h.fact = std::max(0, story::dsl::companionFactWord(w[1]));
    break;
  }
  memo[a.id] = h;
  return h;
}

bool factHolds(const Member& m, const Bio& b, int fact) {
  switch (fact) {
    case story::dsl::COMPANION_KIN_ALIVE: return b.stakeKin.name.empty() || !b.stakeKin.dead;
    case story::dsl::COMPANION_KIN_DEAD: return !b.stakeKin.name.empty() && b.stakeKin.dead;
    case story::dsl::COMPANION_VETERAN: return m.p.brave >= 0;     // (party_recruit.cpp stakeFor, banter.cpp: the same rule)
    case story::dsl::COMPANION_DESERTER: return m.p.brave < 0;
    default: return true;
  }
}

// the motives their stake speaks with, first the strongest
std::vector<Motive> motivesFor(Hook h) {
  switch (h) {
    case Hook::BurnedHome: return {Motive::Vengeance, Motive::Grief, Motive::Hope, Motive::Duty};
    case Hook::LostKin: return {Motive::Love, Motive::Grief, Motive::Hope, Motive::Fear};
    case Hook::War: return {Motive::Shame, Motive::Grief, Motive::Duty, Motive::Vengeance, Motive::Fear};
    case Hook::Revenge: return {Motive::Vengeance, Motive::Grief, Motive::Fear};
    case Hook::Heirloom: return {Motive::Pride, Motive::Duty, Motive::Grief};
    case Hook::Debt: return {Motive::Fear, Motive::Shame, Motive::Duty};
    case Hook::Exile: return {Motive::Hope, Motive::Shame, Motive::Pride};
    case Hook::Faith: return {Motive::Devotion, Motive::Duty, Motive::Hope};
    case Hook::Ambition: return {Motive::Pride, Motive::Hope};
    case Hook::Love: return {Motive::Love, Motive::Hope, Motive::Fear};
    default: return {Motive::Duty};
  }
}

int homeHandle(Game& g, const Member& m) {
  if (!m.site) return -1;
  const int h = story::siteByIdLoad(g, m.site);
  return h >= 0 && h < (int)g.world.sites.size() && g.world.sites[(size_t)h].settlement() ? h : -1;
}

int actorIndexOf(const Game& g, const Member& m) {
  if (m.actor < 0) return -1;
  for (size_t i = 1; i < g.actors.size(); i++) if (g.actors[i].id == m.actor) return (int)i;
  return -1;
}

}  // namespace

std::string questFor(Game& g, const Member& m) {
  if (m.voc == Vocation::Hound || m.origin == Origin::Mercenary || m.stake.hook == Hook::None) return std::string();
  const int hs = homeHandle(g, m);
  const int ai = actorIndexOf(g, m);
  if (hs < 0 || ai < 0) return std::string();
  const Bio b = bio(g, m);
  std::vector<const Archetype*> cands;
  for (const Archetype& a : story::saga::archetypes()) {
    if (a.tier != 2) continue;
    const CompHook ch = compHookOf(a);
    if (ch.stake != (int)m.stake.hook || !factHolds(m, b, ch.fact)) continue;
    cands.push_back(&a);
  }
  if (cands.empty()) return std::string();
  uint64_t rng = ew::mix64(g.seed * 0x9E3779B97F4A7C15ull ^ ((uint64_t)m.id << 32) ^ (uint64_t)m.stake.seed ^ 0xC0A1D9E57ull);
  const std::vector<Motive> ranked = motivesFor(m.stake.hook);
  const int voice = story::voiceOf(g, m.culture);
  const Actor& A = g.actors[(size_t)ai];
  for (int attempt = 0; attempt < 8 && !cands.empty(); attempt++) {
    const uint64_t r0 = stepRng(rng);
    const size_t ci = (size_t)(r0 % cands.size());
    const Archetype& a = *cands[ci];
    Spec s;
    s.arch = a.id;
    s.voice = voice;
    s.motive = ranked.front();
    for (Motive mo : ranked) if (!a.motives || (a.motives & story::saga::motiveBit(mo))) { s.motive = mo; break; }
    const std::map<std::string, std::string> slots = story::saga::slotsOf(a.body);
    std::vector<std::string> chosen;
    for (int k = 0; k < 3; k++) {
      s.twist[k].clear();
      if (!slots.count("t" + std::to_string(k + 1))) continue;
      const uint64_t rf = stepRng(rng);
      if (rf % 3 == 0) continue;   // (a third of the slots stay plain: the companion's own story first)
      std::vector<const Twist*> fit;
      for (const Twist& t : story::saga::twists()) if (story::saga::twistFitsSlot(a, t, k, chosen)) fit.push_back(&t);
      if (fit.empty()) continue;
      const uint64_t rt = stepRng(rng);
      const Twist* t = fit[(size_t)(rt % fit.size())];
      s.twist[k] = t->id;
      chosen.push_back(t->id);
    }
    const uint64_t rs = stepRng(rng);
    s.seed = (uint32_t)rs;
    const std::string id = story::saga::specId(s);
    const story::dsl::Script* sc = story::saga::script(id);
    if (!sc) continue;
    std::vector<story::Binding> cast;
    std::string why;
    const uint64_t key = ew::mix64(g.seed ^ story::strHash64s(id) ^ g.npcKey(A));
    if (story::castScript(g, *sc, A.id, hs, key, cast, why)) return id;
    // (twists may need what this place lacks: the plain telling of the same archetype next)
    if (!s.twist[0].empty() || !s.twist[1].empty() || !s.twist[2].empty()) {
      Spec plain = s;
      plain.twist[0].clear(); plain.twist[1].clear(); plain.twist[2].clear();
      const std::string pid = story::saga::specId(plain);
      const story::dsl::Script* ps = story::saga::script(pid);
      cast.clear();
      if (ps && story::castScript(g, *ps, A.id, hs, ew::mix64(g.seed ^ story::strHash64s(pid) ^ g.npcKey(A)), cast, why)) return pid;
    }
    cands.erase(cands.begin() + (std::ptrdiff_t)ci);
  }
  return std::string();
}

uint32_t questBegin(Game& g, int mi) {
  if (mi < 0 || mi >= (int)g.party.members.size()) return 0;
  Member& m = g.party.members[(size_t)mi];
  const int ai = actorIndexOf(g, m);
  const int hs = homeHandle(g, m);
  if (ai < 0 || hs < 0) return 0;
  std::string id = m.quest;
  if (id.empty() || !story::scriptById(id)) id = questFor(g, m);
  if (id.empty()) return 0;
  std::string why;
  const uint32_t inst = g.story.start(g, id, g.actors[(size_t)ai].id, hs, &why);
  Member& m2 = g.party.members[(size_t)mi];   // (the start may have moved actors, never the party)
  if (!inst) return 0;
  m2.quest = id;
  m2.questInst = inst;
  m2.questState = 2;
  m2.flags |= MF_QUEST_OFFERED;
  return inst;
}

int questStatus(Game& g, Member& m) {
  if (!m.questInst) {
    if (m.questState >= 2 && m.questState <= 4 && !(m.flags & MF_QUEST_DONE)) m.questState = 0;   // (a quest dropped on load)
    return m.questState;
  }
  const story::Instance* in = g.story.find(m.questInst);
  if (!in) {   // gone (another composer version on load, or long finished): what the flags remember
    m.questInst = 0;
    m.questState = (m.flags & MF_QUEST_DONE) ? 3 : 0;
    return m.questState;
  }
  if (!in->done) { m.questState = 2; return 2; }
  m.flags |= MF_QUEST_DONE;
  m.questState = in->failed ? 4 : 3;
  return m.questState;
}

}  // namespace voice
}  // namespace party
