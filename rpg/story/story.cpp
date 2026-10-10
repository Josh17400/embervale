// M4 "Banners": the quest and campaign engine (rpg/story/story.h). STORY lane.
//
// The engine runs the scripts of the library (rpg/story/dsl.h) as instances: a cast (caster.cpp), the current stage, a
// few variables. Entering a stage runs its effects; a dialogue stage waits for the player to talk to its role and
// choose; a goal stage waits for the world (a place reached, a site entered, kills, a foe slain, an item found, a prop
// used, days passed, a realm condition); an end stage closes the story. Each instance mirrors itself into
// Game::quests (QType::Story, Quest::stage = the instance id), so the journal, the markers and the map need nothing new.
#include "rpg/story/story.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include "engine/audio.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/craft.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/life.h"
#include "rpg/story/saga.h"
#include "rpg/sim/common.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/story/dsl.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/source.h"

namespace story {
namespace {
constexpr uint8_t STORY_BLOCK_V1 = 1;
constexpr uint8_t STORY_BLOCK_V2 = 2;    // (STORY lane) v2: bindings' places, stage day / counter, lore masks, memories,
                                          // facts, successions, plays, the heralds' proclaimed serial. v1 still loads.
constexpr uint8_t STORY_BLOCK_V3 = 3;    // (M6b, SAVE_VER 13) v3: v2 + the sagas' repetition guard
constexpr uint8_t STORY_BLOCK_VER = 4;   // (M6b COMPOSER) v4: the guard's records carry their hook and the offered flag
using dsl::CondType;
using dsl::EffType;
using dsl::ObjType;
using dsl::RoleKind;
using dsl::StageKind;

uint64_t strHash(const std::string& s) {
  uint64_t h = 1469598103934665603ull;
  for (char c : s) { h ^= (uint8_t)c; h *= 1099511628211ull; }
  return h;
}

bool hasItem(const Game& g, const std::string& name) {
  for (const Item& it : g.inv) if (it.kind == ItemKind::Quest && it.name == name) return true;
  return false;
}

void notice(Game& g, const std::string& text, uint32_t col = rgba(255, 220, 140)) {
  Event e;
  e.type = Ev::Notice; e.p = g.pl().p; e.a = (int)col; e.f = 0; e.s = text;
  g.events.push_back(e);
}
void questUpdate(Game& g, int qid, float f, const std::string& text) {
  Event e;
  e.type = Ev::QuestUpdate; e.p = g.pl().p; e.a = qid; e.f = f; e.s = text;
  g.events.push_back(e);
}
void sound(Game& g, Sfx s, float pitch = 1.0f, float vol = 1.0f) {
  if (host().sound) host().sound(g, (int)s, pitch, vol);
}

}  // namespace

// ---------------------------------------------------------------- shared helpers (story_internal.h)
Host& host() { static Host h; return h; }
uint64_t storyMarkKey(uint64_t id, uint64_t tag) { return markKey(id, (Mk)tag); }

int scriptIndex(const std::string& id) { return dsl::library().find(id); }
const dsl::Script* scriptById(const std::string& id) {
  const int i = scriptIndex(id);
  if (i >= 0) return &dsl::library().scripts[(size_t)i];
  return saga::isSagaId(id) ? saga::script(id) : nullptr;
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

RewardRoll rewardRoll(int D, int tier, uint64_t culture, const cult::Culture* maker, uint64_t seed) {
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
  gear::DropSource ds;
  ds.D = D;
  ds.culture = culture;
  const Rarity rar = tier == 3 ? Rarity::Rare : Rarity::Uncommon;
  R.item = gear::makeGearC(r, kind, sub, gear::dropIlvl(ds), rar, maker);
  if (culture && !R.item.culture) R.item.culture = culture;
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

// ---------------------------------------------------------------- conditions and effects
namespace {

uint64_t markId(const Instance& in, const std::string& name) {
  return ew::mix64(strHash(in.script) ^ ew::mix64(strHash(name)) ^ (in.cast.empty() ? 0 : in.cast[0].id));
}

bool evalCond(const Game& g, const Instance& in, const dsl::CondDef& c) {
  auto var = [&](const std::string& n) { auto it = in.vars.find(n); return it == in.vars.end() ? 0 : it->second; };
  auto kid = [&](const std::string& r) -> ew::Gid { const Binding* b = bindingOf(in, r); return b ? b->id : 0; };
  switch (c.type) {
    case CondType::Gold: return g.gold >= c.n;
    case CondType::Level: return g.plLevel >= c.n;
    case CondType::Bg: return (int)g.background == c.n;
    case CondType::Rep: return g.realm.rep(kid(c.a)) >= c.n;
    case CondType::Fame: return g.realm.renown().fame >= c.n;
    case CondType::Var: return var(c.a) >= c.n;
    case CondType::NoVar: return var(c.a) < c.n;
    case CondType::Have: return hasItem(g, fillText(g, in, c.a));
    case CondType::Mark: return g.marks.count(storyMarkKey(markId(in, c.a), MK_STORY_MARK)) > 0;
    case CondType::NoMark: return g.marks.count(storyMarkKey(markId(in, c.a), MK_STORY_MARK)) == 0;
    case CondType::Female: { const Binding* b = bindingOf(in, c.a); return b && b->female; }
    case CondType::War: return g.realm.atWar(kid(c.a), kid(c.b));
    default: return false;
  }
}

// the realm's record of a site exists (forcing needs its genesis owner, type and heart)
bool noteRealmSite(Game& g, ew::Gid site) {
  const int h = siteByIdLoad(g, site);
  if (h < 0) return false;
  const Site& s = g.world.sites[(size_t)h];
  if (!s.settlement()) return false;
  const ew::Gid home = s.homeKingdom >= 0 ? g.world.kingdoms[(size_t)s.homeKingdom].id : 0;
  g.realm.noteSite(s.id, home, (uint8_t)s.type, g.world.ox + s.ex, g.world.oy + s.ey);
  return true;
}

int hookLevel(const Game& g, const Instance& in) {
  if (!in.cast.empty()) { const int h = g.world.siteHandle(in.cast[0].site); if (h >= 0) return std::max(1, g.world.sites[(size_t)h].level); }
  return std::max(1, g.plLevel);
}

}  // namespace

void runEffect1(Game& g, Engine& E, Instance& in, const dsl::EffDef& e);
// a realm effect the player's story caused is news the player hears at once (the News journal, the toast)
void runEffect(Game& g, Engine& E, Instance& in, const dsl::EffDef& e) {
  const uint32_t before = g.realm.eventSerial();
  runEffect1(g, E, in, e);
  if (e.type < EffType::RealmWar || g.realm.eventSerial() == before) return;
  std::vector<uint32_t> fresh;
  for (const realm::WorldEvent& ev : g.realm.events()) if (ev.id >= before && !ev.heard) fresh.push_back(ev.id);
  for (uint32_t id : fresh)
    for (const realm::WorldEvent& ev : g.realm.events())
      if (ev.id == id) { hearEvent(g, ev, 0); break; }
}
void runEffect1(Game& g, Engine& E, Instance& in, const dsl::EffDef& e) {
  auto kid = [&](const std::string& r) -> ew::Gid { const Binding* b = bindingOf(in, r); return b ? b->id : 0; };
  auto bsite = [&](const std::string& r) -> ew::Gid { const Binding* b = bindingOf(in, r); return b ? (b->site ? b->site : b->id) : 0; };
  int32_t px, py;
  playerGlobal(g, px, py);
  switch (e.type) {
    case EffType::Gold:
      if (e.n > 0) { if (host().gold) host().gold(g, e.n); else g.gold += e.n; }
      else if (e.n < 0) { g.gold = std::max(0, g.gold + e.n); sound(g, Sfx::Coin, 0.8f); }
      break;
    case EffType::Xp: if (host().xp) host().xp(g, e.n); else g.plXp += e.n; break;
    case EffType::Give: {
      Item it;
      it.kind = ItemKind::Quest; it.name = fillText(g, in, e.text); it.icon = (art::Icon)e.icon; it.value = 0;
      it.tint = rgba(214, 176, 96); it.questId = in.questId;
      if (host().item) host().item(g, it); else g.inv.push_back(it);
      break;
    }
    case EffType::Take: {
      const std::string n = fillText(g, in, e.text);
      for (int i = (int)g.inv.size() - 1; i >= 0; i--) if (g.inv[(size_t)i].kind == ItemKind::Quest && g.inv[(size_t)i].name == n) { g.dropItem(i); break; }
      break;
    }
    case EffType::Loot: if (host().loot) host().loot(g, hookLevel(g, in) + 1); break;
    case EffType::Rep: {
      const ew::Gid k = kid(e.args[0]);
      g.realm.addRep(k, e.n);
      notice(g, (e.n >= 0 ? "+" : "") + std::to_string(e.n) + " STANDING WITH " + kingdomName(g, k), e.n >= 0 ? rgba(150, 230, 150) : rgba(240, 140, 120));
      break;
    }
    case EffType::Fame: g.realm.renown().fame += e.n; break;
    case EffType::Set: in.vars[e.args[0]] = e.n; break;
    case EffType::Add: in.vars[e.args[0]] += e.n; break;
    case EffType::Mark: g.marks[storyMarkKey(markId(in, e.args[0]), MK_STORY_MARK)] = g.day; break;
    case EffType::Remember: {
      const Binding* b = bindingOf(in, e.args[0]);
      if (b) E.addMemory(b->id, fillText(g, in, e.text), g.day);
      break;
    }
    case EffType::Fact: E.addFact(in.cast.empty() ? 0 : in.cast[0].site, fillText(g, in, e.text), g.day); break;
    case EffType::Moves: {
      Binding* p = bindingOf(in, e.args[0]);
      const Binding* to = bindingOf(in, e.args[1]);
      if (p && to) { p->site = to->site ? to->site : to->id; p->gx = to->gx; p->gy = to->gy; p->hasPos = to->hasPos; }
      break;
    }
    case EffType::Shop: {
      const Binding* b = bindingOf(in, e.args[0]);
      if (b) { E.addMemory(b->id, fillText(g, in, e.text), g.day); E.addFact(b->site, fillText(g, in, e.text), g.day); }
      break;
    }
    case EffType::Hide: in.vars["_hide_" + e.args[0]] = 1; break;
    case EffType::Show: in.vars["_hide_" + e.args[0]] = 0; break;
    case EffType::Notice: notice(g, fillText(g, in, e.text)); break;
    case EffType::Sound: {
      const std::string& s = e.args[0];
      if (s == "fanfare") { sound(g, Sfx::LevelUp, 0.74f); sound(g, Sfx::QuestStart, 0.9f, 0.7f); }
      else if (s == "bell") sound(g, Sfx::Bell, 0.9f);
      else if (s == "roar") sound(g, Sfx::Roar, 1.0f, 0.7f);
      else sound(g, Sfx::QuestStart);
      break;
    }
    case EffType::Rumour: { const std::string l = g.hearRumour(); if (!l.empty()) notice(g, "ON YOUR MAP: " + l.substr(0, 40)); break; }
    case EffType::RealmWar:
      if (kid(e.args[0]) && kid(e.args[1]) && kid(e.args[0]) != kid(e.args[1])) { g.realm.forceWar(kid(e.args[0]), kid(e.args[1]), g.day); g.realmSync(); }
      break;
    case EffType::RealmFamine:
      if (noteRealmSite(g, bsite(e.args[0]))) { g.realm.forceFamine(bsite(e.args[0]), g.day); g.realmSync(); }
      break;
    case EffType::RealmPeace:
      if (kid(e.args[0]) && kid(e.args[1]) && g.realm.atWar(kid(e.args[0]), kid(e.args[1]))) { g.realm.forcePeace(kid(e.args[0]), kid(e.args[1]), g.day); g.realmSync(); }
      else g.realm.forceEvent(realm::EvType::Peace, kid(e.args[0]), kid(e.args[1]), 0, px, py, g.day);
      break;
    case EffType::RealmSuccession: {
      // (no Realm call changes a ruler yet: the story records the new ruler (Engine::rulerOf wins over the realm's) and
      // the realm tells of it; REALM lane request: forceSuccession(kingdom, name, female, day))
      const ew::Gid k = kid(e.args[0]);
      const Binding* heir = bindingOf(in, e.args[1]);
      if (!k || !heir) break;
      std::string rn, rt;
      E.rulerOf(g, k, rn, rt);
      RulerOverride o;
      o.kingdom = k; o.name = heir->name; o.title = rt.empty() ? std::string("KING") : rt; o.day = g.day;
      E.rulers_.push_back(o);
      ew::Gid cap = 0;
      if (const realm::KingdomState* K = g.realm.kingdom(k)) cap = K->capital;
      g.realm.forceEvent(realm::EvType::Succession, k, 0, cap, px, py, g.day);
      break;
    }
    case EffType::Reward: {
      // (M6b, COMPOSER lane) inside the M6 band of the hook's danger (rewardRoll): gold and XP, and for rich / great one
      // piece of gear at the place's item level (uncommon / rare at most). Keyed on the story, so a reload pays the same.
      const int D = storyDanger(g, in);
      uint64_t cu = 0;
      if (!in.cast.empty() && in.cast[0].site) { const int h = siteByIdLoad(g, in.cast[0].site); if (h >= 0) cu = g.world.sites[(size_t)h].culture; }
      const cult::Culture* maker = cu && g.world.src ? &g.world.src->culture(cu) : nullptr;
      const uint64_t key = ew::mix64(strHash(in.script) ^ ((uint64_t)in.stage << 40) ^ (in.cast.empty() ? 0 : in.cast[0].id));
      const RewardRoll R = rewardRoll(D, e.n, cu, maker, key);
      if (host().gold) host().gold(g, R.gold); else g.gold += R.gold;
      if (host().xp) host().xp(g, R.xp); else g.plXp += R.xp;
      if (R.hasItem) {
        if (host().item) host().item(g, R.item); else g.inv.push_back(R.item);
      }
      break;
    }
    case EffType::Secret: {
      // (M6b, COMPOSER lane) progress toward one smithing secret of the place's culture (craft HOW_QUEST): the first the
      // player does not know yet, in an order fixed by the place and the story (its alloys' recipes, its weapon and
      // armour patterns), 40 points of 100
      const ew::Gid site = bsite(e.args[0]);
      const int h = site ? siteByIdLoad(g, site) : -1;
      if (h >= 0 && g.world.src && g.world.sites[(size_t)h].culture) {
        const cult::Culture& C = g.world.src->culture(g.world.sites[(size_t)h].culture);
        struct Pick { uint8_t alloy; craft::SecretKind kind; };
        std::vector<Pick> picks;
        for (size_t a = 0; a < C.arms.alloys.size() && a < 8; a++) picks.push_back({(uint8_t)(a + 1), craft::SecretKind::AlloyRecipe});
        picks.push_back({0, craft::SecretKind::WeaponPattern});
        picks.push_back({0, craft::SecretKind::ArmourPattern});
        const size_t n = picks.size();
        const size_t first = (size_t)(ew::mix64(site ^ strHash(in.script)) % n);
        const uint64_t ck = craft::secretKey(C.id);
        for (size_t k = 0; k < n; k++) {
          const Pick& p = picks[(first + k) % n];
          if (g.craft.knows(ck, p.alloy, p.kind)) continue;
          const bool known = craft::learnSecret(g.craft, C, p.alloy, p.kind, 40, craft::HOW_QUEST);
          const int pct = g.craft.progress(ck, p.alloy, p.kind);
          const std::string nm = craft::secretName(C, p.alloy, p.kind);
          notice(g, known ? "YOU HAVE LEARNT " + nm : "A SECRET OF THE FORGE: " + nm + " (" + std::to_string(pct) + "%)", rgba(150, 230, 150));
          break;
        }
      }
      break;
    }
    case EffType::Befriend: {
      // (M6b, COMPOSER lane) a census resident (trade RESIDENT_CENSUS, id life::npcId(site, idx)) becomes the player's
      // friend (a life tie); one the caster invented remembers the player kindly instead
      const Binding* b = bindingOf(in, e.args[0]);
      if (!b) break;
      const int ix = residentIndexOf(g, *b);
      if (ix >= 0) {
        if (g.life.befriend(b->site, ix)) notice(g, b->name + " COUNTS YOU A FRIEND", rgba(150, 230, 150));
      } else if (!E.memoryOf(b->id)) {
        E.addMemory(b->id, "YOU AGAIN. GOOD. SIT DOWN, FRIEND.", g.day);
      }
      break;
    }
    case EffType::RealmEvent: {
      ew::Gid a = 0, b = 0, site = 0;
      int32_t ex = px, ey = py;
      for (const std::string& r : e.args) {
        const Binding* bb = bindingOf(in, r);
        if (!bb) continue;
        if (bb->kind == (uint8_t)RoleKind::Kingdom) { if (!a) a = bb->id; else b = bb->id; }
        else if (bb->site) { site = bb->site; if (bb->hasPos) { ex = bb->gx; ey = bb->gy; } }
      }
      g.realm.forceEvent((realm::EvType)e.ev, a, b, site, ex, ey, g.day);
      break;
    }
    default: break;
  }
}

void Engine::enterStage(Game& g, Instance& in, int st) {
  const dsl::Script* s = scriptOf(in);
  if (!s || st < 0 || st >= (int)s->stages.size()) { in.done = true; in.failed = true; return; }
  in.stage = st;
  in.stageDay = g.day;
  in.count = 0;
  const dsl::StageDef& G = s->stages[(size_t)st];
  for (const dsl::EffDef& e : G.effs) runEffect(g, *this, in, e);
  if (G.kind == StageKind::End) { finish(g, in, G.success); return; }
  mirror(g, in);
  // a fetch inside the site the player stands in: the item is laid out now
  if (G.kind == StageKind::Goal && (G.goal.type == ObjType::Fetch || G.goal.type == ObjType::Slay) && g.inside && g.subSite >= 0)
    onEntered(g, g.subSite, -1);
}

void Engine::finish(Game& g, Instance& in, bool success) {
  in.done = true;
  in.failed = !success;
  played_[in.script]++;
  const dsl::Script* s = scriptOf(in);
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
      default: break;
    }
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
  std::string w;
  const uint64_t key = ew::mix64(seed_ ^ strHash(s.id) ^ ((uint64_t)in.id << 32) ^ (uint64_t)(uint32_t)hookSite * 0x9E3779B97F4A7C15ull);
  // (fixer M6b r1) the composer's try-cast used its own key: a role picked from a few candidates (the giver's kin, a
  // friend of the first one picked) may have chosen someone with no friend of their own, so the cast is tried with a
  // few more keys (deterministic) before the story is refused
  bool cast = false;
  // (fixer M6b r2) a saga is cast first with the very key the composer's try-cast validated the offer with (compose.cpp
  // hookCtx/pickFrom: the hook's npcKey, or its site id mixed with the hook kind), so an offer that was pitched can be
  // begun with the same people the pitch was checked against
  if (saga::isSagaId(s.id)) {
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
      const bool siteKeyed = !A || s.hook == dsl::HookKind::Herald;
      const uint64_t hookKey = !siteKeyed ? g.npcKey(*A) : ew::mix64(g.world.sites[(size_t)hs].id ^ (0xB0A2Dull * (uint64_t)((int)s.hook + 1)));
      const uint64_t ck = ew::mix64(g.seed ^ strHash(s.id) ^ hookKey);
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
  if (!in.cast.empty() && (s.hook == dsl::HookKind::Npc || s.hook == dsl::HookKind::Herald))
    g.marks[storyMarkKey(in.cast[0].id, MK_STORY_GIVER)] = g.day;
  running_.push_back(in);
  Instance& I = running_.back();
  const uint32_t id = I.id;
  enterStage(g, I, 0);
  return id;
}

// ---------------------------------------------------------------- dialogue
bool Engine::talksTo(const Game& g, const Instance& in, const Actor& a) const {
  const dsl::Script* s = scriptOf(in);
  if (!s || in.done || !a.npc) return false;
  const dsl::StageDef& G = s->stages[(size_t)in.stage];
  if (G.kind != StageKind::Talk) return false;
  const int ri = s->role(G.talk);
  const Binding* b = bindingOf(in, G.talk);
  if (!b || ri < 0) return false;
  switch ((RoleKind)b->kind) {
    case RoleKind::Giver: return !isStoryPerson(a) && g.npcKey(a) == b->id;
    case RoleKind::Npc: {
      if ((int)a.role != (int)b->trade || isStoryPerson(a)) return false;
      const int h = homeSiteOf(g, a);
      return h >= 0 && g.world.sites[(size_t)h].id == b->site;
    }
    case RoleKind::Person: return isStoryPerson(a) && a.slot == personSlot(in.id, ri) && a.name == b->name;
    case RoleKind::Resident: {
      // (M6b) the census resident's own body in the town, or the stand-in syncPersons sent while they are out of play
      if (isStoryPerson(a)) return a.slot == personSlot(in.id, ri) && a.name == b->name;
      if (b->trade != RESIDENT_CENSUS) return false;
      ew::Gid rs = 0;
      int ix = -1;
      return residentOfActor(g, a, rs, ix) && rs == b->site && life::npcId(rs, ix) == b->id;
    }
    case RoleKind::Ruler: {
      if (a.role != Role::King) return false;
      const int h = homeSiteOf(g, a);
      if (h < 0) return false;
      const Site& S = g.world.sites[(size_t)h];
      return S.kingdom >= 0 && g.world.kingdoms[(size_t)S.kingdom].id == b->id;
    }
    default: return false;
  }
}

bool Engine::showDialogue(Game& g, Instance& in) {
  const dsl::Script* s = scriptOf(in);
  if (!s || in.done) return false;
  const dsl::StageDef& G = s->stages[(size_t)in.stage];
  if (G.kind != StageKind::Talk) return false;
  g.dlg.text = fillText(g, in, G.say);
  std::vector<DlgOpt> mine;
  for (size_t i = 0; i < G.opts.size(); i++) {
    const dsl::OptDef& o = G.opts[i];
    bool ok = true;
    for (const dsl::CondDef& c : o.ifs) if (!evalCond(g, in, c)) ok = false;
    if (!ok) continue;
    mine.push_back({fillText(g, in, o.label), DLG_STORY + SA_OPT + (int)i, (int)in.id});
  }
  // the story's choices come first; the person's usual business stays below them
  std::vector<DlgOpt> rest;
  for (const DlgOpt& o : g.dlg.opts) if (!(o.action >= DLG_STORY && o.action < DLG_WAR)) rest.push_back(o);
  g.dlg.opts = mine;
  for (const DlgOpt& o : rest) g.dlg.opts.push_back(o);
  return true;
}

bool Engine::choose(Game& g, uint32_t instId, int opt, bool forceIf, int forceCheck) {
  Instance* in = find(instId);
  if (!in || in->done) return false;
  const dsl::Script* s = scriptOf(*in);
  if (!s) return false;
  const dsl::StageDef& G = s->stages[(size_t)in->stage];
  if (G.kind != StageKind::Talk || opt < 0 || opt >= (int)G.opts.size()) return false;
  const dsl::OptDef& o = G.opts[(size_t)opt];
  if (!forceIf) for (const dsl::CondDef& c : o.ifs) if (!evalCond(g, *in, c)) return false;
  int to = o.toIx;
  if (o.check) {
    const bool pass = forceCheck == 1 || (forceCheck == 0 && evalCond(g, *in, o.chk));
    to = pass ? o.toIx : o.elseIx;
  }
  const std::string who = G.talk;
  {
    // (fixer M6b r1) the journal context of a hand-off: what this speaker told the player (their first sentence or
    // two) and what the player answered
    handCtx_.clear();
    const Binding* sb = bindingOf(*in, who);
    std::string said = fillText(g, *in, G.say);
    size_t end = std::string::npos;
    for (size_t k = 0; k < said.size(); k++)
      if ((said[k] == '.' || said[k] == '?' || said[k] == '!') && k >= 40 && (k + 1 >= said.size() || said[k + 1] == ' ')) { end = k + 1; break; }
    if (end != std::string::npos) said = said.substr(0, end);
    if (said.size() > 170) {
      size_t sp = said.rfind(' ', 166);
      said = said.substr(0, sp == std::string::npos ? 166 : sp) + "...";
    }
    if (sb && !said.empty()) handCtx_ = sb->name + " SAID: \"" + said + "\" YOU ANSWERED: \"" + fillText(g, *in, o.label) + "\"";
  }
  enterStage(g, *in, to);
  handCtx_.clear();
  in = find(instId);
  if (!in) return true;
  if (g.mode != Mode::Dialogue) return true;
  const dsl::StageDef& T = s->stages[(size_t)in->stage];
  const DlgOpt bye{"FAREWELL.", 0, 0};   // (A_BYE == 0: game_internal.h DlgAct)
  // (fixer M6b r1) the next talk is still the one in front of the player (the same role, or another role the same
  // person plays): it goes on here
  bool samePerson = T.kind == StageKind::Talk && T.talk == who;
  if (T.kind == StageKind::Talk && !samePerson && !in->done) {
    const int ai = host().findActor ? host().findActor(g, g.dlg.actor) : -1;
    samePerson = ai >= 0 && ai < (int)g.actors.size() && talksTo(g, *in, g.actors[(size_t)ai]);
  }
  if (samePerson && !in->done) {
    g.dlg.opts.clear();
    showDialogue(g, *in);
    g.dlg.opts.push_back(bye);
    return true;
  }
  // (fixer M6b r1) a talk stage that belongs to someone else is never read out here: its words are theirs (the
  // player hears them from that person); only a goal's or an ending's lines close this conversation
  if (!T.say.empty() && T.kind != StageKind::Talk) {
    g.dlg.text = fillText(g, *in, T.say);
    g.dlg.opts = {bye};
    return true;
  }
  if (T.kind == StageKind::Talk) {
    // the story moves on to someone else: the journal says who
    const Binding* b = bindingOf(*in, T.talk);
    std::string where;
    if (b && b->site && b->kind != (uint8_t)RoleKind::Site && b->kind != (uint8_t)RoleKind::Capital) {
      const int h = g.world.siteHandle(b->site);
      if (h >= 0 && h < (int)g.world.sites.size() && g.world.sites[(size_t)h].name != b->name) where = ", IN " + g.world.sites[(size_t)h].name + ",";
    }
    g.dlg.text = b ? "(" + b->name + where + " IS WHO YOU MUST SEE NEXT.)" : g.dlg.text;
    g.dlg.opts = {bye};
    return true;
  }
  g.mode = Mode::Play;
  return true;
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
  for (size_t i = 0; i < running_.size(); i++) {
    Instance& in = running_[i];
    if (in.done) continue;
    const dsl::Script* s = scriptOf(in);
    if (!s) { in.done = true; in.failed = true; continue; }
    const dsl::StageDef& G = s->stages[(size_t)in.stage];
    if (G.kind != StageKind::Goal) continue;
    const dsl::ObjDef& o = G.goal;
    const Binding* b = o.role.empty() ? nullptr : bindingOf(in, o.role);
    bool done = false;
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
      if (s->roles[ri].kind == RoleKind::Foe && isStoryPerson(v) && v.slot == personSlot(in.id, (int)ri)) in.vars["_dead_" + s->roles[ri].name] = 1;
      else if (s->roles[ri].kind == RoleKind::Giver && v.npc && !isStoryPerson(v)) {
        // (fixer M6b r2) the street NPC who told the tale fell: the step ends it when it next needs them
        const Binding* gb = bindingOf(in, s->roles[ri].name);
        if (gb && gb->id && g.npcKey(v) == gb->id) in.vars["_dead_" + s->roles[ri].name] = 1;
      }
    const dsl::StageDef& G = s->stages[(size_t)in.stage];
    if (G.kind != StageKind::Goal) continue;
    const dsl::ObjDef& o = G.goal;
    if (o.type == ObjType::Slay) {
      const int ri = s->role(o.role);
      if (ri >= 0 && isStoryPerson(v) && v.slot == personSlot(in.id, ri)) { enterStage(g, in, G.thenIx); continue; }
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
    default: break;
  }
  enterStage(g, *in, G.thenIx);
  return true;
}

// ---------------------------------------------------------------- offers
namespace {
// a cheap look before casting: does the world have what the script's roles need (a war, an event)?
bool precheck(const Game& g, const dsl::Script& s, int hookSite) {
  if (hookSite < 0 || hookSite >= (int)g.world.sites.size()) return false;
  const Site& home = g.world.sites[(size_t)hookSite];
  const int32_t hx = g.world.ox + home.ex, hy = g.world.oy + home.ey;
  const ew::Gid land = landAt(g, hx, hy);
  for (const dsl::RoleDef& R : s.roles) {
    if ((R.kind == RoleKind::Kingdom || R.kind == RoleKind::Ruler || R.kind == RoleKind::Capital) && !land) return false;
    if (R.kind == RoleKind::Kingdom && !g.realm.kingdom(land)) return false;
    if (R.kind == RoleKind::War) {
      bool any = false;
      for (const realm::War& w : g.realm.wars()) if (!w.endDay && (w.attacker == land || w.defender == land)) any = true;
      if (!any) return false;
    }
    if (R.kind == RoleKind::Event) {
      bool any = false;
      for (const realm::WorldEvent& e : g.realm.events()) if ((int)e.type == R.ev && rumourReaches(e, g.day, hx, hy)) any = true;
      if (!any) return false;
    }
    if (R.kind == RoleKind::Site && R.a == "home" && !home.settlement()) return false;
  }
  return true;
}
}  // namespace

int Engine::offerFor(Game& g, const Actor& a) {
  if (!a.npc || !a.human || isStoryPerson(a) || a.role == Role::King || a.role == Role::Child || a.role == Role::Herald) return -1;
  const int hs = homeSiteOf(g, a);
  if (hs < 0 || !g.world.sites[(size_t)hs].settlement()) return -1;
  const uint64_t key = g.npcKey(a);
  if (g.marks.count(storyMarkKey(key, MK_STORY_GIVER))) return -1;
  for (const Instance& in : running_) if (!in.cast.empty() && in.cast[0].id == key) return -1;
  // not everyone has a tale: innkeepers and priests often, everyone else now and then
  const uint32_t h = hash32((uint32_t)key ^ (uint32_t)(key >> 32) ^ 0x57A1u);
  const int pct = a.role == Role::Innkeeper ? 60 : a.role == Role::Priest ? 50 : a.role == Role::Guard ? 25 : 30;
  if ((int)(h % 100) >= pct) return -1;
  const dsl::Library& L = dsl::library();
  int best = -1, bestPlays = 1 << 30;
  uint32_t bestRank = 0;
  for (size_t i = 0; i < L.scripts.size(); i++) {
    const dsl::Script& s = L.scripts[i];
    if (s.hook != dsl::HookKind::Npc) continue;
    if (s.hookTrade == -2 ? (a.role == Role::Guard || a.role == Role::Jarl) : s.hookTrade != (int)a.role) continue;
    bool running = false;
    for (const Instance& in : running_) if (in.script == s.id) running = true;
    if (running) continue;
    const int plays = timesPlayed(s.id);
    const uint32_t rank = hash32(h ^ (uint32_t)strHash(s.id));
    if (plays < bestPlays || (plays == bestPlays && rank > bestRank)) {
      if (!precheck(g, s, hs)) continue;
      best = (int)i; bestPlays = plays; bestRank = rank;
    }
  }
  return best;
}

int Engine::offerForPlace(Game& g, int hookKind, int site) {
  const dsl::Library& L = dsl::library();
  int best = -1, bestPlays = 1 << 30;
  for (size_t i = 0; i < L.scripts.size(); i++) {
    const dsl::Script& s = L.scripts[i];
    if ((int)s.hook != hookKind) continue;
    bool running = false;
    for (const Instance& in : running_) if (in.script == s.id) running = true;
    if (running) continue;
    if (s.tier == 3 && timesPlayed(s.id) > 0) continue;   // a campaign is told once per world
    if (!precheck(g, s, site)) continue;
    const int plays = timesPlayed(s.id);
    if (plays < bestPlays) { best = (int)i; bestPlays = plays; }
  }
  return best;
}

// ---------------------------------------------------------------- persistence
// Layout v2: ver u8, nextId u32, running (u32 n; each: id u32, script str, stage i32, questId i32, done u8, failed u8,
// stageDay i32, count i32, cast (u32 n: role str, id u64, name str, kind u8, site u64, gx i32, gy i32, hasPos u8,
// female u8, trade u8, extra str), vars (u32 n: name str, value i32)), lore (u32 n; each: key u64, title str, found i32,
// total i32, foundMask u32, complete u8, lines (u32 n: str)), memories (u32 n: who u64, line str, day i32), facts
// (u32 n: site u64, day i32, text str), rulers (u32 n: kingdom u64, name str, title str, day i32), played (u32 n:
// script str, count i32), proclaimed u32.
// v3 (M6b) appends the repetition guard: seen (u32 n: arch u32, twists u32, shape u32, day i32, gx i32, gy i32 [v4: hook
// u32, offered u8]), phrases (u32 n: family u32, count u16). v1, v2 and v3 blocks still load.
// Layout v1 (phase A; the save fixture's empty block): ver u8, nextId u32, running (as v2 without stageDay, count and
// the bindings' places), lore (as v2 without foundMask and complete). It loads as the same state with the new fields
// at their defaults.
void Engine::serialize(std::vector<uint8_t>& out) const {
  out.clear();
  BinW w(out);
  w.u8(STORY_BLOCK_VER);
  w.u32(nextId_);
  w.u32((uint32_t)running_.size());
  for (const Instance& in : running_) {
    w.u32(in.id); w.str(in.script); w.i32(in.stage); w.i32(in.questId); w.u8(in.done ? 1 : 0); w.u8(in.failed ? 1 : 0);
    w.i32(in.stageDay); w.i32(in.count);
    w.u32((uint32_t)in.cast.size());
    for (const Binding& b : in.cast) {
      w.str(b.role); w.u64(b.id); w.str(b.name);
      w.u8(b.kind); w.u64(b.site); w.i32(b.gx); w.i32(b.gy); w.u8(b.hasPos ? 1 : 0); w.u8(b.female ? 1 : 0); w.u8(b.trade); w.str(b.extra);
    }
    w.u32((uint32_t)in.vars.size());
    for (const auto& kv : in.vars) { w.str(kv.first); w.i32(kv.second); }
  }
  w.u32((uint32_t)lore_.size());
  for (const LoreEntry& e : lore_) {
    w.u64(e.key); w.str(e.title); w.i32(e.found); w.i32(e.total); w.u32(e.foundMask); w.u8(e.complete ? 1 : 0);
    w.u32((uint32_t)e.lines.size());
    for (const std::string& l : e.lines) w.str(l);
  }
  w.u32((uint32_t)memories_.size());
  for (const Memory& m : memories_) { w.u64(m.who); w.str(m.line); w.i32(m.day); }
  w.u32((uint32_t)facts_.size());
  for (const Fact& f : facts_) { w.u64(f.site); w.i32(f.day); w.str(f.text); }
  w.u32((uint32_t)rulers_.size());
  for (const RulerOverride& r : rulers_) { w.u64(r.kingdom); w.str(r.name); w.str(r.title); w.i32(r.day); }
  w.u32((uint32_t)played_.size());
  for (const auto& kv : played_) { w.str(kv.first); w.i32(kv.second); }
  w.u32(proclaimed_);
  // v3: the repetition guard (v4: + hook, offered)
  w.u32((uint32_t)guard_.seen.size());
  for (const saga::Seen& x : guard_.seen) { w.u32(x.arch); w.u32(x.twists); w.u32(x.shape); w.i32(x.day); w.i32(x.gx); w.i32(x.gy); w.u32(x.hook); w.u8(x.offered); }
  w.u32((uint32_t)guard_.phrases.size());
  for (const auto& kv : guard_.phrases) { w.u32(kv.first); w.u16(kv.second); }
}

bool Engine::deserialize(const std::vector<uint8_t>& data) {
  BinR r(data);
  const uint8_t ver = r.u8();
  if (r.bad || (ver != STORY_BLOCK_V1 && ver != STORY_BLOCK_V2 && ver != STORY_BLOCK_V3 && ver != STORY_BLOCK_VER)) return false;
  const bool v2 = ver >= STORY_BLOCK_V2, v3 = ver >= STORY_BLOCK_V3, v4 = ver >= STORY_BLOCK_VER;
  Engine E;
  E.seed_ = seed_;
  E.nextId_ = r.u32();
  const uint32_t n = r.u32();
  if (n > 10000 || n > data.size()) return false;
  for (uint32_t i = 0; i < n && !r.bad; i++) {
    Instance in;
    in.id = r.u32(); in.script = r.str(); in.stage = r.i32(); in.questId = r.i32(); in.done = r.u8() != 0; in.failed = r.u8() != 0;
    if (v2) { in.stageDay = r.i32(); in.count = r.i32(); }
    const uint32_t nc = r.u32();
    if (nc > 1000) return false;
    for (uint32_t k = 0; k < nc && !r.bad; k++) {
      Binding b;
      b.role = r.str(); b.id = r.u64(); b.name = r.str();
      if (v2) {
        b.kind = r.u8(); b.site = r.u64(); b.gx = r.i32(); b.gy = r.i32(); b.hasPos = r.u8() != 0; b.female = r.u8() != 0;
        b.trade = r.u8(); b.extra = r.str();
      }
      in.cast.push_back(b);
    }
    const uint32_t nv = r.u32();
    if (nv > 10000) return false;
    for (uint32_t k = 0; k < nv && !r.bad; k++) { std::string key = r.str(); in.vars[key] = r.i32(); }
    // (a damaged save: a stage past the script's loads as its end, never as an index past the table)
    if (const dsl::Script* s = scriptOf(in)) { if (in.stage < 0 || in.stage >= (int)s->stages.size()) { in.stage = 0; in.done = true; in.failed = true; } }
    else { in.done = true; in.failed = true; in.vars["_forgotten"] = 1; }   // (M6b: closed by the next step)
    E.running_.push_back(std::move(in));
  }
  const uint32_t nl = r.u32();
  if (nl > 100000 || nl > data.size()) return false;
  for (uint32_t i = 0; i < nl && !r.bad; i++) {
    LoreEntry e;
    e.key = r.u64(); e.title = r.str(); e.found = r.i32(); e.total = r.i32();
    if (v2) { e.foundMask = r.u32(); e.complete = r.u8() != 0; }
    const uint32_t k = r.u32();
    if (k > 1000) return false;
    for (uint32_t j = 0; j < k && !r.bad; j++) e.lines.push_back(r.str());
    E.lore_.push_back(std::move(e));
  }
  if (v2) {
    uint32_t c = r.u32();
    if (c > 100000 || c > data.size()) return false;
    for (uint32_t i = 0; i < c && !r.bad; i++) { Memory m; m.who = r.u64(); m.line = r.str(); m.day = r.i32(); E.memories_.push_back(m); }
    c = r.u32();
    if (c > 100000 || c > data.size()) return false;
    for (uint32_t i = 0; i < c && !r.bad; i++) { Fact f; f.site = r.u64(); f.day = r.i32(); f.text = r.str(); E.facts_.push_back(f); }
    c = r.u32();
    if (c > 100000 || c > data.size()) return false;
    for (uint32_t i = 0; i < c && !r.bad; i++) { RulerOverride o; o.kingdom = r.u64(); o.name = r.str(); o.title = r.str(); o.day = r.i32(); E.rulers_.push_back(o); }
    c = r.u32();
    if (c > 100000 || c > data.size()) return false;
    for (uint32_t i = 0; i < c && !r.bad; i++) { std::string s = r.str(); E.played_[s] = r.i32(); }
    E.proclaimed_ = r.u32();
  }
  if (v3) {
    uint32_t c = r.u32();
    if (c > saga::Guard::GUARD_SEEN || c > data.size()) return false;
    for (uint32_t i = 0; i < c && !r.bad; i++) {
      saga::Seen x;
      x.arch = r.u32(); x.twists = r.u32(); x.shape = r.u32(); x.day = r.i32(); x.gx = r.i32(); x.gy = r.i32();
      if (v4) { x.hook = r.u32(); x.offered = r.u8(); }
      E.guard_.seen.push_back(x);
    }
    c = r.u32();
    if (c > saga::Guard::GUARD_PHRASES || c > data.size()) return false;
    for (uint32_t i = 0; i < c && !r.bad; i++) { const uint32_t f = r.u32(); E.guard_.phrases[f] = r.u16(); }
  }
  if (r.bad || r.p != data.size()) return false;
  *this = std::move(E);
  return true;
}

}  // namespace story
