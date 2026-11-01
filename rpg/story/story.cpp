// M4 "Banners": the quest and campaign engine (rpg/story/story.h). STORY lane.
//
// The engine runs the scripts of the library (rpg/story/dsl.h) as instances: a cast (caster.cpp), the current stage, a
// few variables. Entering a stage runs its effects; a dialogue stage waits for the player to talk to its role and
// choose; a goal stage waits for the world (a place reached, a site entered, kills, a foe slain, an item found, a prop
// used, days passed, a realm condition); an end stage closes the story. Each instance mirrors itself into
// Game::quests (QType::Story, Quest::stage = the instance id), so the journal, the markers and the map need nothing new.
#include "rpg/story/story.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "engine/sound_ids.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/craft.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/life.h"
#include "rpg/sim/party.h"
#include "rpg/sim/party_voice.h"
#include "rpg/sim/talk.h"
#include "rpg/story/saga.h"
#include "rpg/sim/common.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/story/dsl.h"
#include "rpg/story/radiant.h"
#include "rpg/story/story_internal.h"
#include "rpg/story/story_detail.h"
#include "rpg/sim/home.h"
#include "rpg/world/economy.h"
#include "rpg/world/source.h"

namespace story {
namespace {
void questUpdate(Game& g, int qid, float f, const std::string& text) {
  Event e;
  e.type = Ev::QuestUpdate; e.p = g.pl().p; e.a = qid; e.f = f; e.s = text;
  g.events.push_back(e);
}

}  // namespace

// ---------------------------------------------------------------- shared helpers (story_internal.h)
Host& host() { static Host h; return h; }
uint64_t storyMarkKey(uint64_t id, uint64_t tag) { return markKey(id, (Mk)tag); }

int scriptIndex(const std::string& id) { return dsl::library().find(id); }
const dsl::Script* scriptById(const std::string& id) {
  const int i = scriptIndex(id);
  if (i >= 0) return &dsl::library().scripts[(size_t)i];
  if (saga::isSagaId(id)) return saga::script(id);
  return radiant::isJobId(id) ? radiant::script(id) : nullptr;   // (P1b) a radiant job's composed script
}
const dsl::Script* scriptOf(const Instance& in) { return scriptById(in.script); }
uint64_t strHash64s(const std::string& s) { return strHash(s); }

bool residentOfActor(const Game& g, const Actor& a, ew::Gid& site, int& idx) {
  if (a.resident < 0) return false;
  const int h = homeSiteOf(g, a);
  if (h < 0 || h >= (int)g.world.sites.size()) return false;
  const life::Census* c = g.life.find(g.world.sites[(size_t)h].id);
  if (!c || a.resident >= (int)c->res.size()) return false;
  site = c->site;
  idx = a.resident;
  return true;
}

int residentIndexOf(const Game& g, const Binding& b) {
  if (b.trade != RESIDENT_CENSUS || b.kind != (uint8_t)dsl::RoleKind::Resident || !b.site) return -1;
  const life::Census* c = g.life.find(b.site);
  if (!c) return -1;
  for (const life::Resident& r : c->res) if (life::npcId(b.site, r.idx) == b.id) return r.idx;
  return -1;
}

gear::DropSource rewardSource(int D, int tier, uint64_t culture, int playerLevel, gear::LootSrc src) {
  gear::DropSource ds;
  ds.D = std::clamp(D, 1, gear::MAX_D);
  ds.culture = culture;
  ds.playerLevel = std::max(1, playerLevel);
  ds.src = src;
  ds.atLeast = Rarity::Uncommon;
  const bool great = tier >= 3;
  int boost = 0;
  switch (src) {
    case gear::LootSrc::SagaFinale: boost = great ? 2 : 1; break;
    case gear::LootSrc::CampaignFinale:
      // the main way to an early Legendary or Mythic (15.27.4): Legendary guaranteed, ungated (gear.cpp gives Mythic
      // its chance from level 20)
      ds.atLeast = Rarity::Legendary;
      boost = great ? 1 : 0;
      break;
    default:   // a stage on the way: the M6b rule (rare at most)
      ds.src = gear::LootSrc::Saga;
      ds.atMost = Rarity::Rare;
      boost = great ? 1 : 0;
      break;
  }
  if (ds.D >= ds.playerLevel + 3) boost++;   // a hard story
  ds.boost = (int8_t)boost;
  return ds;
}

gear::LootSrc rewardSrcOf(const dsl::Script& s, const dsl::StageDef& G) {
  if (G.kind != StageKind::End || !G.success) return gear::LootSrc::Saga;
  return s.tier >= 3 ? gear::LootSrc::CampaignFinale : gear::LootSrc::SagaFinale;
}

namespace {
// a piece of story gear: the kind (one draw), its weapon type, then the rarity and the piece (draws sequenced)
Item storyGear(Rng& r, const gear::DropSource& ds, const cult::Culture* maker) {
  const int what = r.irange(100);
  ItemKind kind = ItemKind::Weapon;
  int sub = 0;
  if (what < 34) { kind = ItemKind::Weapon; sub = r.irange(5); }
  else if (what < 52) kind = ItemKind::Armor;
  else if (what < 62) kind = ItemKind::Helmet;
  else if (what < 70) kind = ItemKind::Shield;
  else if (what < 78) kind = ItemKind::Bow;
  else if (what < 86) kind = ItemKind::Boots;
  else if (what < 93) kind = ItemKind::Ring;
  else kind = ItemKind::Amulet;
  const Rarity rar = gear::rollRarity(r, ds);
  Item it = gear::makeGearC(r, kind, sub, gear::itemLevelFor(ds), rar, maker);
  if (ds.culture && !it.culture) it.culture = ds.culture;
  return it;
}
}  // namespace

Item storyLoot(int D, int playerLevel, uint64_t seed, bool finale, const cult::Culture* maker) {
  Rng r(seed ? seed : 1);
  // (P3 LOOT) made by the hook's people (their forms, their alloys in order), never maker-less (a relic look)
  const gear::DropSource ds = rewardSource(D, 2, maker ? maker->id : 0, playerLevel, finale ? gear::LootSrc::SagaFinale : gear::LootSrc::Saga);
  return storyGear(r, ds, maker);
}

RewardRoll rewardRoll(int D, int tier, uint64_t culture, const cult::Culture* maker, uint64_t seed, int playerLevel, gear::LootSrc src) {
  // (M6b, COMPOSER lane) story rewards by the hook's danger, never the player's level (15.9 "slow rags to riches"): a
  // fair ending pays about what one of the M4 tales does early on (40-60 gold), rising gently with D; rich and great add
  // one piece of gear AT the place's band (item level D): uncommon for rich, rare for great (never epic or legendary:
  // those come from champions, uniques, bosses and the forge)
  RewardRoll R;
  D = std::clamp(D, 1, gear::MAX_D);
  tier = std::clamp(tier, 0, 3);
  static const int goldBase[4] = {20, 40, 60, 90}, goldPer[4] = {5, 10, 15, 22};
  static const int xpBase[4] = {40, 70, 100, 150}, xpPer[4] = {8, 12, 16, 22};
  R.gold = goldBase[tier] + goldPer[tier] * (D - 1);
  R.xp = xpBase[tier] + xpPer[tier] * (D - 1);
  if (tier < 2) return R;
  Rng r(seed ? seed : 1);
  R.item = storyGear(r, rewardSource(D, tier, culture, playerLevel, src), maker);
  R.hasItem = true;
  return R;
}

int storyDanger(Game& g, const Instance& in) {
  // the hook settlement: the giver's site (npc hooks) or the hook place itself
  for (const Binding& b : in.cast) {
    const ew::Gid id = b.site ? b.site : (b.kind == (uint8_t)dsl::RoleKind::Giver ? b.id : 0);
    if (!id) continue;
    const int h = siteByIdLoad(g, id);
    if (h >= 0) return std::max(1, g.world.sites[(size_t)h].level);
    break;
  }
  return 1;
}
const Binding* bindingOf(const Instance& in, const std::string& role) {
  for (const Binding& b : in.cast) if (b.role == role) return &b;
  return nullptr;
}
Binding* bindingOf(Instance& in, const std::string& role) {
  for (Binding& b : in.cast) if (b.role == role) return &b;
  return nullptr;
}
void playerGlobal(const Game& g, int32_t& gx, int32_t& gy) {
  int x = 0, y = 0;
  overworldTile(g, x, y);
  gx = g.world.ox + x; gy = g.world.oy + y;
}
int homeSiteOf(const Game& g, const Actor& a) {
  if (a.site >= 0 && a.site < (int)g.world.sites.size()) return a.site;
  if (a.bldg >= 0 && a.bldg < (int)g.world.over.bldgs.size()) return g.world.over.bldgs[(size_t)a.bldg].site;
  return -1;
}
int siteByIdLoad(Game& g, ew::Gid id) {
  if (!id) return -1;
  const int h = g.world.siteHandle(id);
  return h >= 0 ? h : g.world.ensureSite(id);
}
ew::Gid landAt(const Game& g, int32_t gx, int32_t gy) {
  if (!g.world.src) return 0;
  return g.realm.landOwner(g.world.src->kingdomAt(gx, gy), gx, gy);
}
std::string kingdomName(const Game& g, ew::Gid k) {
  if (const realm::KingdomState* K = g.realm.kingdom(k)) return K->name;
  for (const Kingdom& K : g.world.kingdoms) if (K.id == k) return K.name;
  if (g.world.src) if (const ew::KingdomPlan* kp = g.world.src->kingdom(k)) return kp->name;
  return "A FAR REALM";
}
std::string personNameIn(const Game& g, uint64_t culture, uint64_t key, bool female) {
  if (g.world.src && culture) return cult::personName(g.world.src->culture(culture), (uint32_t)(key >> 7), female);
  Rng r((uint32_t)key);
  return makePersonName(r, female);
}
bool freeTileNear(const Game& g, int tx, int ty, int& ox, int& oy, uint32_t salt) {
  const Map& m = g.world.over;
  const int sx = tx + (int)(hash32(salt) % 5) - 2, sy = ty + (int)(hash32(salt * 7 + 1) % 3) - 1;
  for (int r = 0; r <= 8; r++)
    for (int dy = -r; dy <= r; dy++)
      for (int dx = -r; dx <= r; dx++) {
        if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
        const int x = sx + dx, y = sy + dy;
        if (!m.in(x, y) || m.blocked(x, y) || m.propAt(x, y) || groundSolid(m.at(x, y))) continue;
        if (m.at(x, y) == Ground::Water || m.at(x, y) == Ground::DeepWater) continue;
        ox = x; oy = y;
        return true;
      }
  return false;
}

// ---------------------------------------------------------------- text
std::string fillText(const Game& g, const Instance& in, const std::string& text) {
  if (text.find('{') == std::string::npos) return text;
  std::string out;
  size_t i = 0;
  while (i < text.size()) {
    const size_t a = text.find('{', i);
    if (a == std::string::npos) { out += text.substr(i); break; }
    out += text.substr(i, a - i);
    const size_t e = text.find('}', a);
    if (e == std::string::npos) { out += text.substr(a); break; }
    std::string ph = text.substr(a + 1, e - a - 1);
    for (char& c : ph) if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
    i = e + 1;
    const size_t dot = ph.find('.');
    const std::string role = dot == std::string::npos ? ph : ph.substr(0, dot), field = dot == std::string::npos ? "" : ph.substr(dot + 1);
    if (role == "player") { out += g.app.name.empty() ? std::string("TRAVELLER") : upper(g.app.name); continue; }
    if (role == "var") {   // (P1b) {var.<name>}: the variable spelled ("THREE"; digits past twelve)
      auto it = in.vars.find(field);
      out += speech::countWord(it == in.vars.end() ? 0 : (int)it->second);
      continue;
    }
    const Binding* b = bindingOf(in, role);
    if (!b) { out += "{" + ph + "}"; continue; }   // (the validator forbids it; tests look for a leftover brace)
    if (field.empty()) { out += b->name; continue; }
    const RoleKind k = (RoleKind)b->kind;
    const bool f = b->female;
    if (field == "he") { out += f ? "SHE" : "HE"; continue; }
    if (field == "him") { out += f ? "HER" : "HIM"; continue; }
    if (field == "his") { out += f ? "HER" : "HIS"; continue; }
    if (field == "man") { out += f ? "WOMAN" : "MAN"; continue; }
    if (field == "son") { out += f ? "DAUGHTER" : "SON"; continue; }
    if (field == "lad") { out += f ? "LASS" : "LAD"; continue; }
    if (field == "brother") { out += f ? "SISTER" : "BROTHER"; continue; }
    if (field == "father") { out += f ? "MOTHER" : "FATHER"; continue; }      // (M6b)
    if (field == "husband") { out += f ? "WIFE" : "HUSBAND"; continue; }
    if (field == "sir") { out += f ? "LADY" : "SIR"; continue; }
    if (field == "boy") { out += f ? "GIRL" : "BOY"; continue; }
    if (field == "dir") {
      int32_t px, py;
      playerGlobal(g, px, py);
      out += dirWord(b->gx - px, b->gy - py, false);
      continue;
    }
    if (field == "realm") { const ew::Gid k2 = landAt(g, b->gx, b->gy); out += k2 ? kingdomName(g, k2) : std::string("THE WILDLANDS"); continue; }
    if (field == "god" || field == "people") {
      // (M6b) the culture of the binding's place (its settlement's, else the land's)
      uint64_t cu = 0;
      const int h = b->site ? g.world.siteHandle(b->site) : (b->kind == (uint8_t)RoleKind::Site ? g.world.siteHandle(b->id) : -1);
      if (h >= 0) cu = g.world.sites[(size_t)h].culture;
      if (!cu && g.world.src && b->hasPos) cu = g.world.src->cultureAt(b->gx, b->gy);
      if (!cu && g.world.src) { int32_t px, py; playerGlobal(g, px, py); cu = g.world.src->cultureAt(px, py); }
      if (cu && g.world.src) {
        const cult::Culture& C = g.world.src->culture(cu);
        if (field == "god") out += C.faith.names.empty() ? std::string("THE GODS") : C.faith.names[0];
        else out += C.adjective.empty() ? C.name : C.adjective;
      } else out += field == "god" ? "THE GODS" : "THESE LANDS'";
      continue;
    }
    if (field == "job" || field == "kin" || field == "kind" || field == "lost") {
      // (M6b) the caster's words: a resident's extra is "JOB|KIN|LOST" (LOST: a mourner's dead), a beast's or boss's
      // its kind
      std::string part[3];
      size_t p0 = 0;
      for (int k2 = 0; k2 < 3; k2++) {
        const size_t bar = b->extra.find('|', p0);
        part[k2] = b->extra.substr(p0, bar == std::string::npos ? std::string::npos : bar - p0);
        if (bar == std::string::npos) break;
        p0 = bar + 1;
      }
      if (field == "kin") out += part[1].empty() ? std::string("NEIGHBOUR") : part[1];
      else if (field == "lost") out += part[2].empty() ? std::string(b->female ? "HER DEAD" : "HIS DEAD") : part[2];
      else out += part[0];
      continue;
    }
    if (field == "town" || field == "foe") { out += b->extra; continue; }
    if (field == "title") { out += b->extra; continue; }
    if (field == "name") {
      const std::string& n = b->name;
      out += b->extra.size() + 1 < n.size() && n.compare(0, b->extra.size(), b->extra) == 0 ? n.substr(b->extra.size() + 1) : n;
      continue;
    }
    if (k == RoleKind::Kingdom && field == "lord") {
      std::string rn, rt;
      out += g.story.rulerOf(g, b->id, rn, rt) ? rt + " " + rn : std::string("THE CROWN");
      continue;
    }
    if (k == RoleKind::Ruin) {
      realm::RuinRecord& R = ruinCached(const_cast<Game&>(g), b->site);
      if (field == "old") out += R.oldName;
      else if (field == "lord") out += R.lastLord;
      else if (field == "builder") out += R.builtBy;
      else if (field == "years") out += std::to_string(R.fellYearsAgo);
      else if (field == "cause") out += causeWords(R.cause);
      else if (field == "clue") out += R.clues.empty() ? std::string() : R.clues[0];
      continue;
    }
    out += b->name;
  }
  return upper(out);
}

// ---------------------------------------------------------------- the engine
void Engine::reset(uint64_t worldSeed) {
  *this = Engine();
  seed_ = worldSeed;
  saga::warmComposer();   // (fixer M6b r2: the composer's one-time tables in the loading frame, not in play)
}

int Engine::scriptCount() const { return (int)dsl::library().scripts.size(); }
std::vector<std::string> Engine::validate() const { return dsl::library().errors; }

LibraryStats Engine::libraryStats() {
  LibraryStats st;
  for (const dsl::Script& s : dsl::library().scripts) {
    st.scripts++;
    if (s.tier == 3) st.campaigns++;
    st.words += dsl::wordCount(s.hint) + dsl::wordCount(s.pitch);
    for (const dsl::StageDef& G : s.stages) {
      st.stages++;
      st.words += dsl::wordCount(G.say) + dsl::wordCount(G.journal);
      for (const dsl::OptDef& o : G.opts) { st.options++; st.words += dsl::wordCount(o.label); }
      for (const dsl::EffDef& e : G.effs) st.words += dsl::wordCount(e.text);
    }
  }
  return st;
}
std::vector<std::string> Engine::scriptNames() {
  std::vector<std::string> v;
  for (const dsl::Script& s : dsl::library().scripts) v.push_back(s.id);
  return v;
}
std::string Engine::scriptTitle(const std::string& id) {
  const dsl::Script* s = scriptById(id);
  return s ? s->title : std::string();
}

Instance* Engine::find(uint32_t id) { for (Instance& in : running_) if (in.id == id) return &in; return nullptr; }
const Instance* Engine::find(uint32_t id) const { for (const Instance& in : running_) if (in.id == id) return &in; return nullptr; }
Instance* Engine::byScript(const std::string& script) {
  for (Instance& in : running_) if (in.script == script && !in.done) return &in;
  return nullptr;
}
std::string Engine::stageName(const Instance& in) const {
  const dsl::Script* s = scriptOf(in);
  return s && in.stage >= 0 && in.stage < (int)s->stages.size() ? s->stages[(size_t)in.stage].name : std::string();
}
bool Engine::isEnd(const Instance& in) const {
  const dsl::Script* s = scriptOf(in);
  return !s || in.done || (in.stage >= 0 && in.stage < (int)s->stages.size() && s->stages[(size_t)in.stage].kind == StageKind::End);
}
bool Engine::isGoal(const Instance& in) const {
  const dsl::Script* s = scriptOf(in);
  return s && !in.done && in.stage >= 0 && in.stage < (int)s->stages.size() && s->stages[(size_t)in.stage].kind == StageKind::Goal;
}
int Engine::optionCount(const Instance& in) const {
  const dsl::Script* s = scriptOf(in);
  if (!s || in.done || in.stage < 0 || in.stage >= (int)s->stages.size()) return 0;
  return (int)s->stages[(size_t)in.stage].opts.size();
}
bool Engine::optionIsCheck(const Instance& in, int opt) const {
  const dsl::Script* s = scriptOf(in);
  if (!s || in.stage < 0 || in.stage >= (int)s->stages.size()) return false;
  const auto& O = s->stages[(size_t)in.stage].opts;
  return opt >= 0 && opt < (int)O.size() && O[(size_t)opt].check;
}
std::string Engine::sayText(const Game& g, const Instance& in) const {
  const dsl::Script* s = scriptOf(in);
  if (!s || in.stage < 0 || in.stage >= (int)s->stages.size()) return "";
  return fillText(g, in, s->stages[(size_t)in.stage].say);
}
std::string Engine::journalText(const Game& g, const Instance& in) const {
  const dsl::Script* s = scriptOf(in);
  if (!s || in.stage < 0 || in.stage >= (int)s->stages.size()) return "";
  return fillText(g, in, s->stages[(size_t)in.stage].journal);
}
std::vector<std::string> Engine::stageTexts(const Game& g, const Instance& in) const {
  std::vector<std::string> v;
  const dsl::Script* s = scriptOf(in);
  if (!s || in.stage < 0 || in.stage >= (int)s->stages.size()) return v;
  const dsl::StageDef& G = s->stages[(size_t)in.stage];
  if (!G.say.empty()) v.push_back(fillText(g, in, G.say));
  if (!G.journal.empty()) v.push_back(fillText(g, in, G.journal));
  for (const dsl::OptDef& o : G.opts) v.push_back(fillText(g, in, o.label));
  for (const dsl::EffDef& e : G.effs) if (!e.text.empty()) v.push_back(fillText(g, in, e.text));
  return v;
}

const std::string* Engine::memoryOf(uint64_t who) const {
  for (auto it = memories_.rbegin(); it != memories_.rend(); ++it) if (it->who == who) return &it->line;
  return nullptr;
}
void Engine::addMemory(uint64_t who, const std::string& line, int day) {
  for (Memory& m : memories_) if (m.who == who) { m.line = line; m.day = day; return; }
  memories_.push_back({who, line, day});
  if (memories_.size() > 400) memories_.erase(memories_.begin());
}
void Engine::addFact(uint64_t site, const std::string& text, int day) {
  facts_.push_back({site, day, text});
  if (facts_.size() > 400) facts_.erase(facts_.begin());
}
LoreEntry* Engine::loreFor(uint64_t key) {
  for (LoreEntry& e : lore_) if (e.key == key) return &e;
  return nullptr;
}
bool Engine::rulerOf(const Game& g, ew::Gid kingdom, std::string& name, std::string& title) const {
  for (auto it = rulers_.rbegin(); it != rulers_.rend(); ++it)
    if (it->kingdom == kingdom) { name = it->name; title = it->title; return true; }
  if (const realm::KingdomState* K = g.realm.kingdom(kingdom)) {
    name = K->ruler.name;
    title = K->ruler.title.empty() ? std::string("KING") : K->ruler.title;
    return !name.empty();
  }
  return false;
}
int Engine::timesPlayed(const std::string& script) const {
  auto it = played_.find(script);
  return it == played_.end() ? 0 : it->second;
}
void Engine::drop(uint32_t inst) {
  for (size_t i = 0; i < running_.size(); i++) if (running_[i].id == inst) { running_.erase(running_.begin() + (std::ptrdiff_t)i); return; }
}

void Engine::enterStage(Game& g, Instance& in, int st) {
  const dsl::Script* s = scriptOf(in);
  if (!s || st < 0 || st >= (int)s->stages.size()) { in.done = true; in.failed = true; return; }
  in.stage = st;
  in.stageDay = g.day;
  in.count = 0;
  const dsl::StageDef& G = s->stages[(size_t)st];
  for (const dsl::EffDef& e : G.effs) runEffect(g, *this, in, e);
  if (G.kind == StageKind::End) {
    // (P1b) an ending reached off a dialogue (a goal done, a clock run out, an escort lost) has no panel to show its say
    // in: it is queued as an epilogue (ENGINE lane: story_game.cpp shows it as a closing scene)
    if (!choosing_ && !G.say.empty() && !s->family.empty()) {   // (P1b items; the older library never needed it)
      Epilogue ep;
      ep.inst = in.id;
      ep.text = fillText(g, in, G.say);
      ep.speaker = in.cast.empty() ? s->title : in.cast[0].name;
      if (narration(ep.text, ep.speaker)) ep.speaker = s->title;
      epilogues_.push_back(ep);
      if (epilogues_.size() > 8) epilogues_.erase(epilogues_.begin());
    }
    finish(g, in, G.success);
    return;
  }
  mirror(g, in);
  // a fetch inside the site the player stands in: the item is laid out now
  if (G.kind == StageKind::Goal && (G.goal.type == ObjType::Fetch || G.goal.type == ObjType::Slay) && g.inside && g.subSite >= 0)
    onEntered(g, g.subSite, -1);
}

void Engine::finish(Game& g, Instance& in, bool success) {
  in.done = true;
  in.failed = !success;
  const dsl::Script* s = scriptOf(in);
  // (P1b) a radiant job's composed script is one of thousands: the pool remembers the job, not the play count
  if (!radiant::isJobId(in.script)) played_[in.script]++;
  // (P1b) `once site` and `cooldown`: where it was played (not a decline at the opening)
  if (s && !s->family.empty() && !in.vars.count("_quiet") && !in.cast.empty()) {
    const ew::Gid site = in.cast[0].site ? in.cast[0].site : in.cast[0].id;
    if (s->onceSite) g.marks[storyMarkKey(ew::mix64(strHash(in.script) ^ site), MK_P1B_PLAYED)] = g.day;
    if (s->cooldown > 0) g.marks[storyMarkKey(site, MK_P1B_COOLDOWN)] = g.day;
  }
  // (M8) a tale seen through or let drop is a deed the companions weigh (not one turned down at the start)
  if (in.vars.find("_quiet") == in.vars.end() && !in.cast.empty()) {
    if (const int mi = companionOf(g, in); mi >= 0) g.party.members[(size_t)mi].flags |= party::MF_QUEST_DONE;
    g.deed(success ? party::Deed::QuestDone : party::Deed::QuestFailed, in.cast[0].site);
  }
  // (fixer M6b r2) turned down at the opening (still quiet: never committed): declined, not a quest the journal keeps
  auto qv = in.vars.find("_quiet");
  if (qv != in.vars.end()) {
    in.vars.erase(qv);
    if (!success && in.questId) {
      g.quests.erase(std::remove_if(g.quests.begin(), g.quests.end(), [&](const Quest& q) { return q.id == in.questId; }), g.quests.end());
      if (g.trackedQuest == in.questId) g.trackedQuest = -1;
      in.questId = 0;
      sound(g, Sfx::MenuBack);
      return;
    }
  }
  for (Quest& q : g.quests)
    if (q.id == in.questId) {
      q.state = QState::Done;
      if (!success) q.flags |= QF_FAILED;
      const std::string ep = s ? fillText(g, in, s->stages[(size_t)in.stage].journal) : std::string();
      if (!ep.empty()) q.desc = ep;
      // a quest's own items go when it ends
      for (int i = (int)g.inv.size() - 1; i >= 0; i--) if (g.inv[(size_t)i].kind == ItemKind::Quest && g.inv[(size_t)i].questId == q.id) g.dropItem(i);
      questUpdate(g, q.id, 1, std::string(success ? "QUEST COMPLETE: " : "QUEST ENDED: ") + q.title);
      if (g.trackedQuest == q.id) {
        g.trackedQuest = -1;
        for (const Quest& o : g.quests) if (o.state != QState::Done && o.type != QType::Main) { g.trackedQuest = o.id; break; }
      }
    }
  sound(g, success ? Sfx::QuestDone : Sfx::MenuBack);
}

void Engine::mirror(Game& g, Instance& in) {
  const dsl::Script* s = scriptOf(in);
  if (!s) return;
  Quest* Q = nullptr;
  for (Quest& q : g.quests) if (q.id == in.questId && in.questId) Q = &q;
  const bool fresh = !Q;
  if (!Q) {
    Quest q;
    q.id = g.nextQuestId++;
    q.type = QType::Story;
    q.state = QState::Active;
    q.giverSite = -1; q.giverBldg = -2; q.giverSlot = -3;   // (never an NPC's: isGiver and the turn-in rules skip it)
    q.target = -1;
    g.quests.push_back(q);
    Q = &g.quests.back();
    in.questId = Q->id;
  }
  const dsl::StageDef& G = s->stages[(size_t)in.stage];
  // (fixer M6b r2) a tale that opens with the giver's own talk (the pitch was accepted: the opening is on screen) is not
  // a quest yet: no banner and no tracking until the player commits (any later stage). Turned down at the opening, it
  // leaves the journal without a trace (finish)
  if (fresh) {
    const Binding* ob = G.kind == StageKind::Talk ? bindingOf(in, G.talk) : nullptr;
    if (ob && ob->kind == (uint8_t)RoleKind::Giver) in.vars["_quiet"] = 1;
    else g.trackedQuest = Q->id;
  }
  bool commit = false;
  if (!fresh) {
    auto qv = in.vars.find("_quiet");
    if (qv != in.vars.end()) { in.vars.erase(qv); commit = true; g.trackedQuest = Q->id; }
  }
  Q->title = s->title;
  Q->stage = (int)in.id;
  Q->giverName = in.cast.empty() ? std::string() : in.cast[0].name;
  Q->giverId = in.cast.empty() ? 0 : in.cast[0].site;
  // the marker: the dialogue's person or the goal's place
  const Binding* tb = nullptr;
  std::string what;
  if (G.kind == StageKind::Talk) {
    tb = bindingOf(in, G.talk);
    if (tb) what = "SPEAK WITH " + tb->name;
  } else if (G.kind == StageKind::Goal) {
    const dsl::ObjDef& o = G.goal;
    if (!o.role.empty()) tb = bindingOf(in, o.role);
    switch (o.type) {
      case ObjType::Goto: what = tb ? "GO TO " + tb->name : "TRAVEL"; break;
      case ObjType::Enter: what = tb ? "ENTER " + tb->name : "ENTER"; break;
      case ObjType::Kill: {
        std::string mw = o.monster >= 0 ? monsterPlural((art::Monster)o.monster) : o.monster == dsl::MON_BANDIT ? "BANDITS"
                         : o.monster == dsl::MON_UNDEAD ? "RESTLESS DEAD" : o.monster == dsl::MON_BEAST ? "BEASTS" : "FOES";
        what = std::to_string(in.count) + "/" + std::to_string(o.n) + " " + mw;
        break;
      }
      case ObjType::Slay: what = tb ? "SLAY " + tb->name : "SLAY"; break;
      case ObjType::Fetch: what = "FIND " + fillText(g, in, o.item); break;
      case ObjType::Use: what = o.prop == (int)art::Prop::NoticeBoard ? "READ A NOTICE BOARD" : "LOOK FOR IT"; break;
      case ObjType::Wait: { const int left = std::max(0, in.stageDay + o.n - g.day); what = left > 0 ? "WAIT " + std::to_string(left) + (left == 1 ? " DAY" : " DAYS") : "ANY MOMENT NOW"; break; }
      case ObjType::War: what = "WAIT FOR WORD OF WAR"; break;
      case ObjType::Famine: what = "WATCH THE HARVEST"; break;
      case ObjType::Rep: what = "EARN THE TRUST OF " + (tb ? tb->name : std::string("THE REALM")); break;
      case ObjType::Have: what = "FIND " + fillText(g, in, o.item); break;
      case ObjType::Escort: {   // (P1b)
        const Binding* pb = bindingOf(in, o.role);
        const Binding* to = bindingOf(in, o.role2);
        what = "TAKE " + (pb ? pb->name : std::string("THEM")) + " TO " + (to ? to->name : std::string("SAFETY"));
        if (to) tb = to;
        break;
      }
      case ObjType::Bout: what = tb ? "BEAT " + tb->name + " (NOT TO THE DEATH)" : "WIN THE BOUT"; break;
      case ObjType::Time: what = o.n >= 17 || o.n < 4 ? "WAIT FOR NIGHTFALL" : o.n < 10 ? "WAIT FOR MORNING" : "WAIT FOR THE HOUR"; break;
      default: break;
    }
  }
  // (P1b) a clock: the days left
  if (G.deadline > 0 && (G.kind == StageKind::Talk || G.kind == StageKind::Goal)) {
    const int left = std::max(0, in.stageDay + G.deadline - g.day);
    what += left > 1 ? " (" + std::to_string(left) + " DAYS LEFT)" : left == 1 ? " (ONE DAY LEFT)" : " (LAST DAY)";
  }
  Q->subject = what;
  if (!G.journal.empty()) Q->desc = fillText(g, in, G.journal);
  else if (G.kind == StageKind::Talk && tb) {
    // (fixer M6b r1) a hand-off stage without a journal line keeps what the quest is about: the last journal line (or
    // what the last speaker asked), then who to see next and where
    std::string base = fresh ? std::string() : Q->desc;
    const size_t cut = base.find(" NEXT: ");
    if (cut != std::string::npos) base = base.substr(0, cut);
    if (base.rfind("SPEAK WITH ", 0) == 0) base.clear();
    if (base.empty() && !handCtx_.empty()) base = handCtx_;
    std::string where;
    if (tb->kind != (uint8_t)RoleKind::Site && tb->kind != (uint8_t)RoleKind::Capital && tb->site) {
      const int h = g.world.siteHandle(tb->site);
      if (h >= 0 && h < (int)g.world.sites.size() && g.world.sites[(size_t)h].name != tb->name) where = " IN " + g.world.sites[(size_t)h].name;
    }
    Q->desc = base.empty() ? what + where + "." : base + " NEXT: " + what + where + ".";
  } else Q->desc = what + ".";
  handCtx_.clear();
  Q->hasPos = false;
  if (tb && tb->hasPos) { Q->hasPos = true; Q->tgx = tb->gx; Q->tgy = tb->gy; Q->targetId = tb->site; }
  if (fresh || commit) {
    if (!in.vars.count("_quiet")) {
      questUpdate(g, Q->id, 0, "QUEST STARTED: " + Q->title);
      sound(g, Sfx::QuestStart);
    }
  } else if (!what.empty()) {
    // (fixer M4 r1) a "GO TO <place>" step is not announced while the player already stands there (it completes at once)
    bool there = false;
    if (G.kind == StageKind::Goal && G.goal.type == ObjType::Goto && tb && tb->site && !g.inside) {
      const int h = g.settlementAt(g.pl().p);
      there = h >= 0 && h < (int)g.world.sites.size() && g.world.sites[(size_t)h].id == tb->site;
    }
    if (!there) questUpdate(g, Q->id, 2, Q->title + ": " + what);
  }
}

uint32_t Engine::start(Game& g, const std::string& script, int hookActor, int hookSite, std::string* why) {
  const dsl::Script* sp = scriptById(script);
  if (!sp) { if (why) *why = "no script '" + script + "'"; return 0; }
  const dsl::Script& s = *sp;
  if (byScript(script)) { if (why) *why = "already running"; return 0; }
  Instance in;
  in.id = nextId_++;
  in.script = s.id;
  for (const auto& kv : s.vars) in.vars[kv.first] = kv.second;
  in.vars["_start"] = g.day;   // (P1b) days / nodays / do days
  std::string w;
  const uint64_t key = ew::mix64(seed_ ^ strHash(s.id) ^ ((uint64_t)in.id << 32) ^ (uint64_t)(uint32_t)hookSite * 0x9E3779B97F4A7C15ull);
  // (fixer M6b r1) the composer's try-cast used its own key: a role picked from a few candidates (the giver's kin, a
  // friend of the first one picked) may have chosen someone with no friend of their own, so the cast is tried with a
  // few more keys (deterministic) before the story is refused
  bool cast = false;
  // (fixer M6b r2) a saga is cast first with the very key the composer's try-cast validated the offer with (compose.cpp
  // hookCtx/pickFrom: the hook's npcKey, or its site id mixed with the hook kind), so an offer that was pitched can be
  // begun with the same people the pitch was checked against
  if (saga::isSagaId(s.id) || radiant::isJobId(s.id)) {   // (P1b: a radiant job too: radiant::offerFor try-casts with offerCastKey)
    const Actor* A = nullptr;
    if (hookActor >= 0 && host().findActor) {
      const int ai = host().findActor(g, hookActor);
      if (ai >= 0 && ai < (int)g.actors.size()) A = &g.actors[(size_t)ai];
    }
    int hs = hookSite;
    if (hs < 0 && A) hs = homeSiteOf(g, *A);
    if (hs >= 0 && hs < (int)g.world.sites.size()) {
      // (fixer M6b r3) a herald's offer is worked out for its capital (saga::offerForPlace, no actor), so its key is
      // the site's even when the herald who proclaimed it becomes the giver
      const uint64_t ck = offerCastKey(g, s, A, hs);
      in.cast.clear();
      cast = castScript(g, s, hookActor, hookSite, ck, in.cast, w);
    }
  }
  for (int t = 0; t < 4 && !cast; t++) {
    in.cast.clear();
    cast = castScript(g, s, hookActor, hookSite, t == 0 ? key : ew::mix64(key + 0x9E3779B97F4A7C15ull * (uint64_t)t), in.cast, w);
  }
  if (!cast) {
    if (why) *why = w;
    nextId_--;
    return 0;
  }
  // (fixer M6b r2) a giver who is a census resident: remember their index (1-based, a saved var) so the step can tell
  // when they die and end the tale instead of waiting forever on a `talk giver`
  if (hookActor >= 0 && host().findActor) {
    const int ai = host().findActor(g, hookActor);
    ew::Gid rs = 0;
    int rix = -1;
    if (ai >= 0 && ai < (int)g.actors.size() && residentOfActor(g, g.actors[(size_t)ai], rs, rix))
      for (const Binding& b : in.cast)
        if (b.kind == (uint8_t)RoleKind::Giver && b.site == rs) { in.vars["_giver_res"] = rix + 1; break; }
  }
  // a giver who told a story tells no other
  if (!in.cast.empty() && (s.hook == dsl::HookKind::Npc || s.hook == dsl::HookKind::Herald) && !radiant::isJobId(s.id))   // (P1b: a job is work)
    g.marks[storyMarkKey(in.cast[0].id, MK_STORY_GIVER)] = g.day;
  running_.push_back(in);
  Instance& I = running_.back();
  const uint32_t id = I.id;
  enterStage(g, I, 0);
  return id;
}

// ---------------------------------------------------------------- objectives
namespace {

bool atPlace(const Game& g, const Binding& b, int radius) {
  if (g.inside && g.subSite >= 0 && g.subSite < (int)g.world.sites.size()) {
    const ew::Gid id = g.world.sites[(size_t)g.subSite].id;
    if (id == b.site || id == b.id) return true;
  }
  if (g.inside && g.subBldg >= 0 && g.subBldg < (int)g.world.over.bldgs.size()) {
    const int sh = g.world.over.bldgs[(size_t)g.subBldg].site;
    if (sh >= 0 && sh < (int)g.world.sites.size() && (g.world.sites[(size_t)sh].id == b.site || g.world.sites[(size_t)sh].id == b.id) &&
        (b.kind == (uint8_t)RoleKind::Site || b.kind == (uint8_t)RoleKind::Capital || b.kind == (uint8_t)RoleKind::Giver))
      return true;
  }
  if (!b.hasPos) return false;
  int32_t px, py;
  playerGlobal(g, px, py);
  const int64_t dx = px - b.gx, dy = py - b.gy;
  return dx * dx + dy * dy <= (int64_t)radius * radius;
}

// how near counts as "there": a city's sprawl is wide, a camp or a cave's mouth is a spot
int placeRadius(const Game& g, const Binding& b) {
  if (b.kind == (uint8_t)RoleKind::Capital) return 40;
  const int h = g.world.siteHandle(b.site ? b.site : b.id);
  if (h < 0) return 14;
  switch (g.world.sites[(size_t)h].type) {
    case SiteType::City: return 40;
    case SiteType::Town: return 26;
    case SiteType::Village: return 18;
    default: return 12;
  }
}

bool isDungeonSite(const Game& g, ew::Gid id) {
  const int h = g.world.siteHandle(id);
  return h >= 0 && (g.world.sites[(size_t)h].type == SiteType::Cave || g.world.sites[(size_t)h].type == SiteType::Ruin);
}

bool killMatches(const Actor& v, int m) {
  if (v.npc) return false;
  switch (m) {
    case dsl::MON_BANDIT: return v.human && v.role == Role::Bandit;
    case dsl::MON_UNDEAD: return !v.human && (v.mon == art::Monster::Skeleton || v.mon == art::Monster::Draugr || v.mon == art::Monster::Wraith);
    case dsl::MON_BEAST: return !v.human && monsterFaction(v.mon) == Faction::Wild;
    case dsl::MON_ANY: return v.hostile;
    default: return !v.human && (int)v.mon == m;
  }
}

}  // namespace

void Engine::step(Game& g) {
  // (M6b) generated scripts nobody runs any more leave the cache (no Script pointer is held across steps)
  if (saga::cacheSize() > 48) saga::trimCache(*this);
  // (M8) a companion's own story ends when the companion is gone from the party (dead, or parted for good)
  for (size_t i = 0; i < running_.size(); i++) {
    Instance& in = running_[i];
    if (in.done) continue;
    const dsl::Script* s = scriptOf(in);
    if (!s || s->hook != dsl::HookKind::Companion || companionOf(g, in) >= 0) continue;
    in.vars.erase("_quiet");
    finish(g, in, false);
    for (Quest& q : g.quests)
      if (q.id == in.questId && in.questId) q.desc = "THE ROAD PARTED YOU FROM " + (in.cast.empty() ? std::string("YOUR COMPANION") : in.cast[0].name) + ", AND THEIR STORY ENDS UNTOLD.";
  }
  for (size_t i = 0; i < running_.size(); i++) {
    Instance& in = running_[i];
    if (in.done) continue;
    const dsl::Script* s = scriptOf(in);
    if (!s) { in.done = true; in.failed = true; continue; }
    const dsl::StageDef& G = s->stages[(size_t)in.stage];
    // (P1b) the clock ran out: on to the deadline's stage (a talk waiting, a goal not done)
    if (G.deadline > 0 && G.deadlineIx >= 0 && (G.kind == StageKind::Talk || G.kind == StageKind::Goal) && g.day >= in.stageDay + G.deadline &&
        !in.vars.count("_quiet")) {
      enterStage(g, in, G.deadlineIx);
      continue;
    }
    if (G.kind != StageKind::Goal) continue;
    const dsl::ObjDef& o = G.goal;
    const Binding* b = o.role.empty() ? nullptr : bindingOf(in, o.role);
    bool done = false;
    // (P1b) an escort is lost when the person dies; a bout when the player went down (ENGINE lane sets _lost_bout)
    auto vget = [&](const std::string& n) { auto it = in.vars.find(n); return it == in.vars.end() ? 0 : it->second; };
    if ((o.type == ObjType::Escort && vget("_dead_" + o.role)) || (o.type == ObjType::Bout && vget("_lost_bout"))) {
      in.vars.erase("_lost_bout");
      enterStage(g, in, G.elseIx);
      continue;
    }
    switch (o.type) {
      case ObjType::Goto: done = b && atPlace(g, *b, placeRadius(g, *b)); break;
      case ObjType::Enter: done = b && !isDungeonSite(g, b->site) && atPlace(g, *b, placeRadius(g, *b)); break;
      case ObjType::Fetch: case ObjType::Have: done = hasItem(g, fillText(g, in, o.item)); break;
      case ObjType::Wait: done = g.day >= in.stageDay + o.n; break;
      case ObjType::War: { const Binding* b2 = bindingOf(in, o.role2); done = b && b2 && g.realm.atWar(b->id, b2->id); break; }
      case ObjType::Famine: { const realm::SettlementState* st = b ? g.realm.settlement(b->site ? b->site : b->id) : nullptr; done = st && (st->flags & realm::SS_FAMINE); break; }
      case ObjType::Rep: done = b && g.realm.rep(b->id) >= o.n; break;
      case ObjType::Slay:
        // (M6b) a named unique or world boss is dead whoever killed it, whenever: the foes' marks remember it
        if (b && b->kind == (uint8_t)RoleKind::Beast) done = g.marks.count(storyMarkKey(b->id, foes::MK_FOES_NAMED_SLAIN)) > 0;
        else if (b && b->kind == (uint8_t)RoleKind::Boss) done = g.marks.count(storyMarkKey(b->id, foes::MK_FOES_BOSS_SLAIN)) > 0;
        break;
      case ObjType::Escort: {
        // (P1b, phase A) done when the player reaches the place with the person alive; the ENGINE lane makes them walk
        // there with the player (and checks they arrived too: _escort_near)
        const Binding* to = bindingOf(in, o.role2);
        auto nv = in.vars.find("_escort_far");
        done = to && atPlace(g, *to, placeRadius(g, *to)) && (nv == in.vars.end() || !nv->second);
        break;
      }
      case ObjType::Bout: done = vget("_won_bout") != 0; if (done) in.vars.erase("_won_bout"); break;   // (ENGINE lane sets it)
      case ObjType::Time: {   // (P1b) the time of day
        const float h = g.hour;
        done = o.n <= o.prop ? (h >= (float)o.n && h < (float)o.prop) : (h >= (float)o.n || h < (float)o.prop);
        break;
      }
      default: break;
    }
    if (done) {
      const uint32_t id = in.id;
      enterStage(g, in, G.thenIx);
      (void)id;
    }
  }
  // (fixer M6b r1) a census resident of the cast who died or left town for good: the role is marked dead, and a story
  // that waits to talk to them ends (failed) with a journal line that says why, instead of waiting forever
  for (size_t i = 0; i < running_.size(); i++) {
    Instance& in = running_[i];
    if (in.done) continue;
    const dsl::Script* s = scriptOf(in);
    if (!s) continue;
    for (size_t ri = 0; ri < s->roles.size(); ri++) {
      const dsl::RoleDef& R = s->roles[ri];
      if (R.kind != RoleKind::Resident && R.kind != RoleKind::Giver) continue;
      const Binding* b = bindingOf(in, R.name);
      if (!b) continue;
      bool dead = false, gone = false;
      // (fixer M6b r2) the giver too: a census resident who died (raid, old age, war, the player), or a street NPC the
      // kill hook saw fall
      if (R.kind == RoleKind::Giver) {
        auto dv = in.vars.find("_dead_" + R.name);
        dead = dv != in.vars.end() && dv->second;
        auto gv = in.vars.find("_giver_res");
        if (!dead && gv != in.vars.end() && gv->second > 0 && b->site) {
          const life::Census* c = g.life.find(b->site);
          const int ix = gv->second - 1;
          if (c && ix < (int)c->res.size()) {
            const life::Resident& r = c->res[(size_t)ix];
            dead = (r.flags & life::RF_DEAD) != 0;
            gone = dead || ((r.flags & life::RF_AWAY) && (int)r.idx != (int)c->takenIdx);
          }
        }
        gone = gone || dead;
      } else {
        if (b->trade != RESIDENT_CENSUS) continue;
        const life::Census* c = g.life.find(b->site);
        const int ix = residentIndexOf(g, *b);
        if (!c || ix < 0 || ix >= (int)c->res.size()) continue;
        const life::Resident& r = c->res[(size_t)ix];
        dead = (r.flags & life::RF_DEAD) != 0;
        // (a villager carried off by raiders may yet be rescued: the story waits for them)
        gone = dead || ((r.flags & life::RF_AWAY) && (int)r.idx != (int)c->takenIdx);
      }
      if (!gone) continue;
      if (dead) in.vars["_dead_" + R.name] = 1;
      const dsl::StageDef& G = s->stages[(size_t)in.stage];
      const bool needed = (G.kind == StageKind::Talk && G.talk == R.name) || (G.kind == StageKind::Goal && G.goal.role == R.name);
      if (!needed) continue;
      std::string town;
      const int h = g.world.siteHandle(b->site);
      if (h >= 0 && h < (int)g.world.sites.size()) town = g.world.sites[(size_t)h].name;
      in.vars.erase("_quiet");   // (fixer M6b r2: not a decline: the journal says why the tale ended)
      finish(g, in, false);
      for (Quest& q : g.quests)
        if (q.id == in.questId && in.questId)
          q.desc = dead ? b->name + " IS DEAD, AND WHAT THEY KNEW WENT INTO THE GROUND WITH THEM. THE TALE ENDS UNFINISHED."
                        : b->name + " HAS LEFT " + (town.empty() ? std::string("TOWN") : town) + " FOR GOOD. THE TALE ENDS UNFINISHED.";
      break;
    }
  }
  // (M6b) a story whose script is gone (a saga of another composer version, a tale the library lost) was loaded as
  // ended: its journal entry closes quietly
  for (const Instance& in : running_) {
    auto fg = in.vars.find("_forgotten");
    if (fg == in.vars.end() || !fg->second) continue;
    for (Quest& q : g.quests)
      if (q.id == in.questId && in.questId && q.state != QState::Done) {
        q.state = QState::Done;
        q.flags |= QF_FAILED;
        q.desc = "THE TALE FADED FROM MEMORY BEFORE IT WAS DONE.";
        if (g.trackedQuest == q.id) g.trackedQuest = -1;
      }
  }
  // finished stories leave the running list (their quests stay in the journal as done)
  running_.erase(std::remove_if(running_.begin(), running_.end(), [](const Instance& in) { return in.done; }), running_.end());
}

void Engine::onKill(Game& g, const Actor& v, bool byPlayer) {
  for (size_t i = 0; i < running_.size(); i++) {
    Instance& in = running_[i];
    if (in.done) continue;
    const dsl::Script* s = scriptOf(in);
    if (!s) continue;
    // a story's foe stays dead
    for (size_t ri = 0; ri < s->roles.size(); ri++)
      if ((s->roles[ri].kind == RoleKind::Foe || s->roles[ri].kind == RoleKind::Person) && isStoryPerson(v) && v.slot == personSlot(in.id, (int)ri))
        in.vars["_dead_" + s->roles[ri].name] = 1;   // (P1b: a person too: an escort lost, a hostile one slain)
      else if (s->roles[ri].kind == RoleKind::Giver && v.npc && !isStoryPerson(v)) {
        // (fixer M6b r2) the street NPC who told the tale fell: the step ends it when it next needs them
        const Binding* gb = bindingOf(in, s->roles[ri].name);
        if (gb && gb->id && g.npcKey(v) == gb->id) in.vars["_dead_" + s->roles[ri].name] = 1;
      }
    const dsl::StageDef& G = s->stages[(size_t)in.stage];
    if (G.kind != StageKind::Goal) continue;
    const dsl::ObjDef& o = G.goal;
    if (o.type == ObjType::Slay || o.type == ObjType::Bout) {
      // (P1b) a bout's opponent killed after all (the ENGINE lane's yield failed to stop it): the bout is won
      const int ri = s->role(o.role);
      if (ri >= 0 && isStoryPerson(v) && v.slot == personSlot(in.id, ri)) { enterStage(g, in, G.thenIx); continue; }
      if (o.type == ObjType::Bout) continue;
      // (M6b) a named unique or the world boss the story cast: its own body, by its stable id
      const Binding* b = bindingOf(in, o.role);
      if (b && v.unique && v.unique == b->id && (b->kind == (uint8_t)RoleKind::Beast || b->kind == (uint8_t)RoleKind::Boss)) {
        in.vars["_dead_" + o.role] = 1;
        enterStage(g, in, G.thenIx);
      }
      continue;
    }
    if (o.type != ObjType::Kill || !byPlayer || !killMatches(v, o.monster)) continue;
    if (!o.role.empty()) {
      const Binding* b = bindingOf(in, o.role);
      if (!b || !atPlace(g, *b, 40)) continue;
    }
    in.count++;
    if (in.count >= o.n) enterStage(g, in, G.thenIx);
    else {
      mirror(g, in);
      Event e;
      e.type = Ev::Text; e.p = v.p + Vec2(0, -26); e.a = (int)rgba(255, 220, 120); e.f = 0;
      e.s = std::to_string(in.count) + "/" + std::to_string(o.n);
      g.events.push_back(e);
    }
  }
}

bool Engine::onUseProp(Game& g, int prop, int tx, int ty) {
  (void)tx; (void)ty;
  bool any = false;
  for (size_t i = 0; i < running_.size(); i++) {
    Instance& in = running_[i];
    if (in.done) continue;
    const dsl::Script* s = scriptOf(in);
    if (!s) continue;
    const dsl::StageDef& G = s->stages[(size_t)in.stage];
    if (G.kind != StageKind::Goal || G.goal.type != ObjType::Use) continue;
    const dsl::ObjDef& o = G.goal;
    const bool sameProp = o.prop == prop || (o.prop == (int)art::Prop::Inscription && art::isLoreProp((art::Prop)prop) && prop != (int)art::Prop::NoticeBoard);
    if (!sameProp) continue;
    if (!o.role.empty()) { const Binding* b = bindingOf(in, o.role); if (!b || !atPlace(g, *b, 30)) continue; }
    enterStage(g, in, G.thenIx);
    any = true;
  }
  return any;
}

void Engine::onEntered(Game& g, int site, int bldg) {
  (void)bldg;
  if (site < 0 || site >= (int)g.world.sites.size() || !g.inside) return;
  const ew::Gid sid = g.world.sites[(size_t)site].id;
  for (size_t i = 0; i < running_.size(); i++) {
    Instance& in = running_[i];
    if (in.done) continue;
    const dsl::Script* s = scriptOf(in);
    if (!s) continue;
    const dsl::StageDef& G = s->stages[(size_t)in.stage];
    // foes waiting in this dungeon (once a slay stage is reached or passed: they are there for the whole story)
    for (size_t ri = 0; ri < s->roles.size(); ri++) {
      const dsl::RoleDef& R = s->roles[ri];
      if (R.kind != RoleKind::Foe) continue;
      const Binding* b = bindingOf(in, R.name);
      if (!b || b->site != sid || in.vars["_dead_" + R.name] || in.vars["_hide_" + R.name]) continue;
      bool there = false;
      for (const Actor& a : g.actors) if (isStoryPerson(a) && a.slot == personSlot(in.id, (int)ri)) there = true;
      if (there || !host().spawnHuman) continue;
      // the far end of the map: the boss room (the spawn the generator made for its boss, else the farthest floor)
      int fx = g.sub.exitX, fy = g.sub.exitY - 2;
      for (const Spawn& sp : g.sub.spawns) if (sp.boss) { fx = sp.x; fy = sp.y + 1; }
      if (g.sub.blocked(fx, fy)) for (int r = 1; r < 6 && g.sub.blocked(fx, fy); r++) { if (!g.sub.blocked(fx + r, fy)) fx += r; else if (!g.sub.blocked(fx - r, fy)) fx -= r; }
      Spawn sp;
      sp.bandit = true; sp.boss = true; sp.site = site; sp.slot = personSlot(in.id, (int)ri);
      const int id = host().spawnHuman(g, sp, fx * TILE + 8.0f, fy * TILE + 10.0f);
      const int ai = host().findActor(g, id);
      if (ai >= 0) {
        Actor& a = g.actors[(size_t)ai];
        a.name = b->name; a.fromMap = false; a.slot = personSlot(in.id, (int)ri); a.site = -1;
        if (b->female) { a.look.beard = false; a.look.hair = art::Hair::Braids; }
      }
    }
    // an item to fetch from here
    if (G.kind == StageKind::Goal && G.goal.type == ObjType::Fetch) {
      const Binding* b = bindingOf(in, G.goal.role);
      const std::string item = fillText(g, in, G.goal.item);
      if (b && (b->site == sid || b->id == sid) && !hasItem(g, item)) {
        bool lying = false;
        for (const Pickup& p : g.pickups) if (p.item.kind == ItemKind::Quest && p.item.name == item) lying = true;
        if (!lying) {
          // a far floor tile (deterministic per site): the generator's boss room, else the far chest
          int fx = g.sub.exitX, fy = g.sub.exitY - 3;
          int best = -1;
          for (int y = 1; y < g.sub.h - 1; y++)
            for (int x = 1; x < g.sub.w - 1; x++) {
              if (g.sub.blocked(x, y) || g.sub.propAt(x, y)) continue;
              const int d = std::abs(x - g.sub.exitX) + std::abs(y - g.sub.exitY) * 2 + (int)(hash2(x, y, (uint32_t)sid) % 7);
              if (d > best) { best = d; fx = x; fy = y; }
            }
          Pickup p;
          p.p = Vec2(fx * TILE + 8.0f, fy * TILE + 12.0f);
          p.item.kind = ItemKind::Quest; p.item.name = item; p.item.icon = art::Icon::Scroll; p.item.value = 0;
          p.item.tint = rgba(214, 176, 96); p.item.questId = in.questId;
          g.pickups.push_back(p);
        }
      }
    }
    if (G.kind == StageKind::Goal && G.goal.type == ObjType::Enter) {
      const Binding* b = bindingOf(in, G.goal.role);
      if (b && (b->site == sid || b->id == sid)) enterStage(g, in, G.thenIx);
    }
  }
}

bool Engine::debugComplete(Game& g, uint32_t instId) {
  Instance* in = find(instId);
  if (!in || in->done) return false;
  const dsl::Script* s = scriptOf(*in);
  if (!s) return false;
  const dsl::StageDef& G = s->stages[(size_t)in->stage];
  if (G.kind != StageKind::Goal) return false;
  const dsl::ObjDef& o = G.goal;
  auto kid = [&](const std::string& r) -> ew::Gid { const Binding* b = bindingOf(*in, r); return b ? b->id : 0; };
  switch (o.type) {
    case ObjType::Fetch: case ObjType::Have: {
      const std::string item = fillText(g, *in, o.item);
      if (!hasItem(g, item)) {
        Item it;
        it.kind = ItemKind::Quest; it.name = item; it.icon = art::Icon::Scroll; it.value = 0; it.questId = in->questId;
        g.inv.push_back(it);
      }
      break;
    }
    case ObjType::War:
      if (kid(o.role) && kid(o.role2) && kid(o.role) != kid(o.role2)) g.realm.forceWar(kid(o.role), kid(o.role2), g.day);
      break;
    case ObjType::Famine: {
      const Binding* b = bindingOf(*in, o.role);
      if (b && noteRealmSite(g, b->site ? b->site : b->id)) g.realm.forceFamine(b->site ? b->site : b->id, g.day);
      break;
    }
    case ObjType::Rep: g.realm.addRep(kid(o.role), std::max(0, o.n - g.realm.rep(kid(o.role)))); break;
    case ObjType::Slay: in->vars["_dead_" + o.role] = 1; break;
    case ObjType::Time: g.hour = (float)o.n + 0.5f; break;   // (P1b)
    default: break;   // (P1b: escort, bout: arrived together / they yielded)
  }
  enterStage(g, *in, G.thenIx);
  return true;
}

// (P1b) the walker's other branches: a lost escort or bout, a clock run out
bool Engine::goalCanFail(const Instance& in) const {
  const dsl::Script* s = scriptOf(in);
  if (!s || in.done || in.stage < 0 || in.stage >= (int)s->stages.size()) return false;
  const dsl::StageDef& G = s->stages[(size_t)in.stage];
  return G.kind == StageKind::Goal && G.elseIx >= 0;
}
bool Engine::debugFail(Game& g, uint32_t instId) {
  Instance* in = find(instId);
  if (!in || !goalCanFail(*in)) return false;
  const dsl::Script* s = scriptOf(*in);
  const dsl::StageDef& G = s->stages[(size_t)in->stage];
  if (G.goal.type == ObjType::Escort) in->vars["_dead_" + G.goal.role] = 1;
  enterStage(g, *in, G.elseIx);
  return true;
}
int Engine::stageDeadline(const Instance& in) const {
  const dsl::Script* s = scriptOf(in);
  if (!s || in.done || in.stage < 0 || in.stage >= (int)s->stages.size()) return 0;
  const dsl::StageDef& G = s->stages[(size_t)in.stage];
  return G.deadlineIx >= 0 ? G.deadline : 0;
}
bool Engine::debugDeadline(Game& g, uint32_t instId) {
  Instance* in = find(instId);
  if (!in || stageDeadline(*in) <= 0) return false;
  const dsl::Script* s = scriptOf(*in);
  const dsl::StageDef& G = s->stages[(size_t)in->stage];
  g.day = std::max(g.day, in->stageDay + G.deadline);
  in->vars.erase("_quiet");
  enterStage(g, *in, G.deadlineIx);
  return true;
}
bool Engine::burdened() const {
  for (const Instance& in : running_) {
    if (in.done) continue;
    for (auto it = in.vars.lower_bound("_heavy:"); it != in.vars.end() && it->first.compare(0, 7, "_heavy:") == 0; ++it)
      if (it->second) return true;
  }
  return false;
}

// ---------------------------------------------------------------- offers
// (P1b, lead) Engine::offerFor / offerForPlace, their precheck and the P1b offer gates moved to rpg/story/offers.cpp: the
// whole offer policy has one owner (the TALES lane in P1b), the engine (this file) another (the ENGINE lane).

}  // namespace story
