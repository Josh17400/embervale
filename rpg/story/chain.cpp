// M6b "Sagas": the campaign chainer (rpg/story/saga.h composeCampaign, campaignPlans, pickCampaign). CAMPAIGNS lane.
//
// A campaign is ONE tier-3 script: its plan's head (title, hook, the recurring roles and vars, the opening stages),
// then one arc per ArcSlot (a tier-3 archetype from rpg/story/campaigns/*.cpp, expanded with prefix "a<n>_" (n from 1)
// and @next leading to the next chosen arc's first stage), then the finale (the world-changing endings). Recurring
// characters are the head's roles; arcs name them freely and add their own %roles. The engine runs it like
// `emptythrone`. Flags for ?flag / !flag lines: m_<motive>, v<voice>, and arc_<id> for every chosen arc (so the finale
// can remember what happened on the way).
// personSlot() keys spawned people on (role index & 15): composeCampaign refuses a composition with more than 16 roles
// (the plans keep head + arcs inside that; the COMPOSER lane may widen the slot scheme later).
//
// Placeholders that name an arc's own roles ({%scout}) expand to the prefixed lower-case name ({a2_scout}); the chainer
// upper-cases everything inside braces (placeholders are case-blind; the validator wants no lower case in a text).
//
// pickCampaign: campaigns are stumbled into (boards, heralds, ruins, fresh realm events) where the plan's needs hold,
// at most once per world, rarely, and deterministically in (world seed, hook site, day bucket).
//
// Determinism: everything is a pure function of the spec (composing) or of (world seed, site, day bucket, the engine's
// record of campaigns told) (picking). Every random draw is its own statement.
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/game.h"
#include "rpg/story/dsl.h"
#include "rpg/story/saga.h"
#include "rpg/story/story.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/ids.h"
#include "rpg/world/source.h"

namespace story {

// (lore.cpp) does this ruin hold the record of a made thing its makers could not unmake (the burden's origin)?
bool burdenRuin(Game& g, ew::Gid site);

namespace saga {
namespace camp {
// (rpg/story/campaigns/*.cpp) each campaign file's plan
CampaignPlan burdenPlan();
CampaignPlan plaguePlan();
CampaignPlan faePlan();
CampaignPlan rebellionPlan();
CampaignPlan dragonPlan();
}  // namespace camp

namespace {

uint32_t fnv32(const std::string& s) {
  uint32_t h = 2166136261u;
  for (char c : s) { h ^= (uint8_t)c; h *= 16777619u; }
  return h;
}

uint64_t draw(uint64_t& rng) {
  rng = ew::mix64(rng + 0x9E3779B97F4A7C15ull);
  return rng;
}

std::string trimL(const std::string& s) {
  size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
  return s.substr(i);
}

// the first `stage <name>` of a template, %names made <prefix>name
std::string entryStage(const std::string& body, const std::string& prefix) {
  size_t p = 0;
  while (p <= body.size()) {
    size_t e = body.find('\n', p);
    if (e == std::string::npos) e = body.size();
    const std::string l = trimL(body.substr(p, e - p));
    p = e + 1;
    if (l.compare(0, 6, "stage ") != 0) continue;
    std::string n = trimL(l.substr(6));
    size_t k = 0;
    while (k < n.size() && n[k] != ' ' && n[k] != '\t' && n[k] != '\r' && n[k] != '#') k++;
    n = n.substr(0, k);
    if (!n.empty() && n[0] == '%') n = prefix + n.substr(1);
    return n;
  }
  return std::string();
}

// everything inside {...} upper case (local role names in placeholders)
void upperPlaceholders(std::string& t) {
  bool in = false;
  for (char& c : t) {
    if (c == '{') in = true;
    else if (c == '}') in = false;
    else if (in && c >= 'a' && c <= 'z') c = (char)(c - 32);
  }
}

int readable(const dsl::Script& s) {
  int n = dsl::wordCount(s.pitch) + dsl::wordCount(s.hint);
  for (const dsl::StageDef& G : s.stages) {
    n += dsl::wordCount(G.say) + dsl::wordCount(G.journal);
    for (const dsl::OptDef& o : G.opts) n += dsl::wordCount(o.label);
  }
  return n;
}

// the hook a plan's head declares (`hook herald`, `hook event famine`...): kind, and the event type for event hooks
void headHook(const char* head, int& kind, int& ev) {
  kind = (int)dsl::HookKind::Npc;
  ev = -1;
  const std::string h = head ? head : "";
  size_t p = 0;
  while (p <= h.size()) {
    size_t e = h.find('\n', p);
    if (e == std::string::npos) e = h.size();
    const std::string l = trimL(h.substr(p, e - p));
    p = e + 1;
    if (l.compare(0, 5, "hook ") != 0) continue;
    std::string w = trimL(l.substr(5));
    std::string a = w.substr(0, w.find(' '));
    if (a == "board") kind = (int)dsl::HookKind::Board;
    else if (a == "herald") kind = (int)dsl::HookKind::Herald;
    else if (a == "ruin") kind = (int)dsl::HookKind::Ruin;
    else if (a == "event") {
      kind = (int)dsl::HookKind::Event;
      const size_t sp = w.find(' ');
      if (sp != std::string::npos) ev = dsl::evWord(trimL(w.substr(sp + 1)).substr(0, trimL(w.substr(sp + 1)).find(' ')));
    }
    return;
  }
}

}  // namespace

const std::vector<CampaignPlan>& campaignPlans() {
  static const std::vector<CampaignPlan> v = [] {
    std::vector<CampaignPlan> p;
    p.push_back(camp::burdenPlan());
    p.push_back(camp::plaguePlan());
    p.push_back(camp::faePlan());
    p.push_back(camp::rebellionPlan());
    p.push_back(camp::dragonPlan());
    return p;
  }();
  return v;
}

bool composeCampaign(const Spec& s, Composed& out) {
  out = Composed();
  if (s.ver != SAGA_GEN_VER) { out.errors.push_back("another composer version"); return false; }
  const CampaignPlan* P = campaignPlan(s.arch);
  if (!P) { out.errors.push_back("no campaign '" + s.arch + "'"); return false; }
  if (s.arcs.size() != P->arcs.size()) {
    out.errors.push_back("campaign '" + s.arch + "' has " + std::to_string(P->arcs.size()) + " arc slots, the spec names " +
                         std::to_string(s.arcs.size()));
    return false;
  }
  // the chosen arcs: each one of its slot's choices, a tier-3 archetype
  std::vector<const Archetype*> arcs(s.arcs.size(), nullptr);
  for (size_t i = 0; i < s.arcs.size(); i++) {
    bool listed = false;
    for (const std::string& c : P->arcs[i].choices) if (c == s.arcs[i]) listed = true;
    if (!listed) { out.errors.push_back("arc '" + s.arcs[i] + "' is not a choice of slot " + std::to_string(i + 1) + " of '" + s.arch + "'"); continue; }
    if (s.arcs[i].empty()) continue;
    const Archetype* a = archetype(s.arcs[i]);
    if (!a) { out.errors.push_back("no arc archetype '" + s.arcs[i] + "'"); continue; }
    if (a->tier != 3) { out.errors.push_back("archetype '" + s.arcs[i] + "' is not a campaign arc (tier 3)"); continue; }
    arcs[i] = a;
  }
  if (!out.errors.empty()) return false;

  Expand x;
  x.rng = ew::mix64(((uint64_t)s.seed << 32) ^ fnv32("@" + s.arch) ^ ((uint64_t)s.voice << 8) ^ (uint64_t)s.motive);
  x.voice = s.voice;
  x.motive = s.motive;
  x.flags.insert(std::string("m_") + motiveName(s.motive));
  x.flags.insert("v" + std::to_string(s.voice));
  for (const Archetype* a : arcs) if (a) x.flags.insert(std::string("arc_") + a->id);
  // where each part begins, and where @next leads from each (the next chosen arc, or the finale)
  const std::string finaleEntry = entryStage(P->finale, std::string());
  if (finaleEntry.empty()) { out.errors.push_back("campaign '" + s.arch + "': the finale has no stage"); return false; }
  std::vector<std::string> entry(arcs.size());
  for (size_t i = 0; i < arcs.size(); i++)
    if (arcs[i]) {
      entry[i] = entryStage(arcs[i]->body, "a" + std::to_string(i + 1) + "_");
      if (entry[i].empty()) out.errors.push_back(std::string("arc '") + arcs[i]->id + "' has no stage");
    }
  auto nextAfter = [&](size_t from) {   // the first chosen arc at index >= from, else the finale
    for (size_t j = from; j < arcs.size(); j++) if (arcs[j]) return entry[j];
    return finaleEntry;
  };

  out.text = "script " + specId(s) + "\narchetype " + std::string(P->name) + "\ntier 3\n";
  x.targets["@next"] = nextAfter(0);
  out.text += expand(P->head, x);
  for (size_t i = 0; i < arcs.size(); i++) {
    if (!arcs[i]) continue;
    Expand y;
    y.rng = x.rng;
    y.voice = x.voice;
    y.motive = x.motive;
    y.flags = x.flags;
    y.picks = x.picks;
    y.prefix = "a" + std::to_string(i + 1) + "_";
    y.targets["@next"] = nextAfter(i + 1);
    out.text += "# arc " + std::to_string(i + 1) + ": " + arcs[i]->id + "\n" + expand(arcs[i]->body, y);
    x.rng = y.rng;
    x.picks = y.picks;   // (a later part may repeat what an arc chose: [[=name]])
    for (uint32_t f : y.families) x.families.push_back(f);
    for (const std::string& e : y.errors) x.errors.push_back("arc " + std::string(arcs[i]->id) + ": " + e);
  }
  x.targets.clear();   // (the finale goes nowhere further: @next there is an error)
  out.text += "# finale\n" + expand(P->finale, x);
  upperPlaceholders(out.text);
  for (const std::string& e : x.errors) out.errors.push_back(e);
  out.families = x.families;

  std::vector<dsl::Script> v;
  dsl::parse(out.text.c_str(), "saga", v, out.errors);
  if (v.size() != 1) { out.errors.push_back("composed text holds " + std::to_string(v.size()) + " scripts"); return false; }
  dsl::Script& sc = v[0];
  dsl::link(sc);
  for (const std::string& p : dsl::validate(sc)) out.errors.push_back(p);
  if (sc.roles.size() > 16) out.errors.push_back("campaign '" + s.arch + "' casts " + std::to_string(sc.roles.size()) + " roles (16 at most: personSlot)");
  if (sc.tier != 3) out.errors.push_back("a campaign must be tier 3");
  out.words = readable(sc);
  return out.errors.empty();
}

// ---------------------------------------------------------------- choosing a campaign for a hook
namespace {

// has this campaign been begun in this world (running or ended)?
bool campaignTold(const Engine& E, const std::string& id) {
  const std::string pre = "saga" + std::to_string(SAGA_GEN_VER) + "~@" + id + "~";
  for (const Instance& in : E.running()) if (in.script.compare(0, pre.size(), pre) == 0) return true;
  for (const auto& kv : E.played_) if (kv.first.compare(0, pre.size(), pre) == 0) return true;
  return false;
}
bool anyCampaignRunning(const Engine& E) {
  for (const Instance& in : E.running())
    if (!in.done && in.script.compare(0, 4, "saga") == 0 && in.script.find("~@") != std::string::npos) return true;
  return false;
}

// do the plan's world needs hold at this hook place?
bool needsHold(Game& g, const CampaignPlan& P, const Hook& h, int hookEv, const Site& S) {
  const int32_t hx = g.world.ox + S.ex, hy = g.world.oy + S.ey;
  const ew::Gid land = landAt(g, hx, hy);
  const uint32_t n = P.needs;
  if (n & (N_KINGDOM | N_RIVAL | N_CAPITAL | N_WAR)) {
    const realm::KingdomState* K = land ? g.realm.kingdom(land) : nullptr;
    if (!K || K->fallen) return false;
    if (n & N_CAPITAL) {
      std::string rn, rt;
      if (!K->capital || !g.story.rulerOf(g, land, rn, rt)) return false;
    }
    if (n & N_RIVAL) {
      bool other = false;
      for (const realm::KingdomState& o : g.realm.kingdoms()) if (o.id != land && !o.fallen) other = true;
      if (!other) return false;
    }
    if (n & N_WAR) {
      bool war = false;
      for (const realm::War& w : g.realm.wars()) if (!w.endDay && (w.attacker == land || w.defender == land)) war = true;
      if (!war) return false;
    }
  }
  if ((n & (N_VILLAGE | N_TOWN | N_CITY)) && h.kind != (int)dsl::HookKind::Ruin && !S.settlement()) return false;
  if (n & N_BOSS) {
    if (!g.world.src) return false;
    bool ok = false;
    const foes::WorldBoss wb = foes::worldBossOf(*g.world.src, ew::EndlessSource::kcellOf(hx), ew::EndlessSource::kcellOf(hy), ok);
    if (!ok || g.marks.count(storyMarkKey(wb.id, foes::MK_FOES_BOSS_SLAIN))) return false;   // (the dead stay dead)
  }
  if (h.kind == (int)dsl::HookKind::Event || (n & N_EVENT)) {
    // a fresh realm event of the hook's type has reached this place
    bool fresh = false;
    for (const realm::WorldEvent& e : g.realm.events())
      if ((hookEv < 0 || (int)e.type == hookEv) && g.day - (int)e.day <= 12 && rumourReaches(e, g.day, hx, hy)) fresh = true;
    if (!fresh) return false;
  }
  if (h.kind == (int)dsl::HookKind::Ruin && P.id == std::string("burden") && !burdenRuin(g, S.id)) return false;
  return true;
}

}  // namespace

bool pickCampaign(Game& g, const Hook& h, Spec& out) {
  if (!g.world.endless || !g.world.src) return false;
  if (h.site < 0 || h.site >= (int)g.world.sites.size()) return false;
  if (anyCampaignRunning(g.story)) return false;   // one campaign at a time: they are long
  const Site S = g.world.sites[(size_t)h.site];
  const int32_t hx = g.world.ox + S.ex, hy = g.world.oy + S.ey;
  const int bucket = g.day / 8;
  const CampaignPlan* best = nullptr;
  uint64_t bestRank = 0;
  for (const CampaignPlan& P : campaignPlans()) {
    int kind = 0, ev = -1;
    headHook(P.head, kind, ev);
    if (kind != h.kind) continue;
    if (kind == (int)dsl::HookKind::Event && h.ev >= 0 && ev >= 0 && h.ev != ev) continue;
    if (campaignTold(g.story, P.id)) continue;
    // rare: a hook place offers a given campaign in about one week-long bucket in N (heralds and events more often:
    // they are themselves rare; boards and ruins are everywhere)
    uint64_t r = ew::mix64(g.seed ^ ((uint64_t)S.id * 0x9E3779B97F4A7C15ull));
    r = ew::mix64(r ^ ((uint64_t)(uint32_t)bucket << 20) ^ fnv32(P.id));
    const int chance = kind == (int)dsl::HookKind::Herald ? 30 : kind == (int)dsl::HookKind::Event ? 45 : kind == (int)dsl::HookKind::Ruin ? 60 : 9;
    if ((int)(r % 100) >= chance) continue;
    if (!needsHold(g, P, h, ev, S)) continue;
    if (!best || r > bestRank) { best = &P; bestRank = r; }
  }
  if (!best) return false;
  Spec s;
  s.campaign = true;
  s.arch = best->id;
  uint64_t rng = ew::mix64(bestRank ^ 0xCA3Bull);
  for (const ArcSlot& sl : best->arcs) {
    const uint64_t r = draw(rng);
    s.arcs.push_back(sl.choices.empty() ? std::string() : sl.choices[(size_t)(r % sl.choices.size())]);
  }
  s.voice = voiceOf(g, S.culture ? S.culture : g.world.src->cultureAt(hx, hy));
  const uint64_t rm = draw(rng);
  static const Motive kMotives[4] = {Motive::Duty, Motive::Fear, Motive::Hope, Motive::Grief};
  s.motive = kMotives[rm % 4];
  const uint64_t rs = draw(rng);
  s.seed = (uint32_t)rs;
  if (g.story.guard_.penalty(s, g.day, hx, hy) >= 700) return false;
  if (!script(specId(s))) return false;   // (it composes: the library lint keeps this true)
  out = s;
  return true;
}

}  // namespace saga
}  // namespace story
