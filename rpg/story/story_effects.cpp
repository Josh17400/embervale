// SOLID 1.3 STORY: the story engine's conditions and effects (evalCond, runEffect / runEffect1, the ambush and the
// road test, the offer cast key). Moved from story.cpp (the engine); story_text.cpp holds the dialogue, story_save.cpp
// the persistence. Shared helpers: story_detail.h.
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
#include "rpg/story/dsl.h"
#include "rpg/story/radiant.h"
#include "rpg/story/story_internal.h"
#include "rpg/story/story_detail.h"
#include "rpg/sim/home.h"
#include "rpg/world/economy.h"
#include "rpg/world/source.h"

namespace story {
namespace {
void notice(Game& g, const std::string& text, uint32_t col = rgba(255, 220, 140)) {
  Event e;
  e.type = Ev::Notice; e.p = g.pl().p; e.a = (int)col; e.f = 0; e.s = text;
  g.events.push_back(e);
}
}  // namespace

// ---------------------------------------------------------------- conditions and effects
namespace {

uint64_t markId(const Instance& in, const std::string& name) {
  return ew::mix64(strHash(in.script) ^ ew::mix64(strHash(name)) ^ (in.cast.empty() ? 0 : in.cast[0].id));
}

// (P1b) the day a story began (its `_start` var; an instance saved before P1b: the day its stage began)
int storyStartDay(const Instance& in) {
  auto it = in.vars.find("_start");
  return it == in.vars.end() ? in.stageDay : it->second;
}
}  // namespace

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
    // (P1b)
    case CondType::Is: return var(c.a) == c.n;
    case CondType::Days: return g.day - storyStartDay(in) >= c.n;
    case CondType::NoDays: return g.day - storyStartDay(in) < c.n;
    case CondType::Season: return (int)home::seasonOf(g.day) == c.n;
    case CondType::Hour: {
      const int from = c.n, to = atoi(c.a.c_str());
      const float h = g.hour;
      return from <= to ? (h >= (float)from && h < (float)to) : (h >= (float)from || h < (float)to);
    }
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

namespace {
int hookLevel(const Game& g, const Instance& in) {
  if (!in.cast.empty()) { const int h = g.world.siteHandle(in.cast[0].site); if (h >= 0) return std::max(1, g.world.sites[(size_t)h].level); }
  return std::max(1, g.plLevel);
}

}  // namespace

// (P1b, lead) the cast key a generated story (a saga or a radiant job) is begun with: an offer try-casts with the same
// key, so what was pitched is what begins (the M6b fixer r2/r3 rule, shared)
uint64_t offerCastKey(const Game& g, const dsl::Script& s, const Actor* A, int hookSite) {
  const bool siteKeyed = !A || s.hook == dsl::HookKind::Herald;
  const ew::Gid sid = hookSite >= 0 && hookSite < (int)g.world.sites.size() ? g.world.sites[(size_t)hookSite].id : 0;
  const uint64_t hookKey = !siteKeyed ? g.npcKey(*A) : ew::mix64(sid ^ (0xB0A2Dull * (uint64_t)((int)s.hook + 1)));
  return ew::mix64(g.seed ^ strHash(s.id) ^ hookKey);
}

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
// (P1b) the ambush itself: n foes of monster word m from 8-12 tiles round the player
void storyAmbush(Game& g, Instance& in, int mword, int count) {
  if (g.inside || !host().spawnMonster) return;
  const int lvl = std::max(1, storyDanger(g, in));
  const Vec2 pp = g.pl().p;
  for (int k = 0; k < count; k++) {
    const float ang = (float)k * 6.2831853f / (float)count + (float)(hash32((uint32_t)(in.id * 977u + (uint32_t)g.day * 31u + (uint32_t)k)) % 628u) / 100.0f;
    const float dist = (float)(8 + (int)(hash32((uint32_t)k * 2654435761u ^ in.id) % 5u)) * TILE;
    int tx = (int)std::floor((pp.x + std::cos(ang) * dist) / TILE), ty = (int)std::floor((pp.y + std::sin(ang) * dist) / TILE), ox = tx, oy = ty;
    if (!freeTileNear(g, tx, ty, ox, oy, (uint32_t)(in.id * 131u + (uint32_t)k))) continue;
    int m = mword;
    if (m == dsl::MON_BEAST) m = (int)art::Monster::Wolf;
    else if (m == dsl::MON_UNDEAD) m = (int)art::Monster::Skeleton;
    if (m == dsl::MON_BANDIT) {
      if (!host().spawnHuman) continue;
      Spawn sp;
      sp.bandit = true;
      host().spawnHuman(g, sp, ox * TILE + 8.0f, oy * TILE + 10.0f);
    } else if (m >= 0) host().spawnMonster(g, m, ox * TILE + 8.0f, oy * TILE + 10.0f, lvl, false);
  }
}

// (P1b) someone walks with the player in this story right now (an escort goal stage, or a `do follow` still on)
bool storyOnRoad(const Instance& in) {
  if (const dsl::Script* s = scriptOf(in))
    if (in.stage >= 0 && in.stage < (int)s->stages.size()) {
      const dsl::StageDef& G = s->stages[(size_t)in.stage];
      if (G.kind == dsl::StageKind::Goal && G.goal.type == dsl::ObjType::Escort) return true;
    }
  for (const auto& kv : in.vars)
    if (kv.second == 1 && kv.first.compare(0, 8, "_follow_") == 0) return true;
  return false;
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
      // (P1b) a burden (a body carried home, a cask): the player walks slower while any quest item of a story with
      // `_heavy` is in the pack (ENGINE lane: the walking speed reads Engine::burdened)
      if (e.ev == 1) in.vars["_heavy:" + it.name] = 1;   // (until it is taken back)
      break;
    }
    case EffType::Take: {
      const std::string n = fillText(g, in, e.text);
      in.vars.erase("_heavy:" + n);   // (P1b) a burden handed over
      for (int i = (int)g.inv.size() - 1; i >= 0; i--) if (g.inv[(size_t)i].kind == ItemKind::Quest && g.inv[(size_t)i].name == n) { g.dropItem(i); break; }
      break;
    }
    case EffType::Loot: {
      // (M8b) by the gear rules (storyLoot), never the classic maker: a finale's when the stage is a success ending
      const dsl::Script* sc = scriptOf(in);
      const bool fin = sc && in.stage >= 0 && in.stage < (int)sc->stages.size() && rewardSrcOf(*sc, sc->stages[(size_t)in.stage]) != gear::LootSrc::Saga;
      const uint64_t key = ew::mix64(strHash(in.script) ^ ((uint64_t)in.stage << 40) ^ 0x100Full ^ (in.cast.empty() ? 0 : in.cast[0].id));
      const Item it = storyLoot(hookLevel(g, in) + 1, g.plLevel, key, fin);
      if (host().item) host().item(g, it); else g.inv.push_back(it);
      break;
    }
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
      // (M8b, 15.27.4) a success ending is the story's finale (a campaign's: the main way to an early Legendary)
      const dsl::Script* sc = scriptOf(in);
      const gear::LootSrc src = sc && in.stage >= 0 && in.stage < (int)sc->stages.size() ? rewardSrcOf(*sc, sc->stages[(size_t)in.stage]) : gear::LootSrc::Saga;
      RewardRoll R = rewardRoll(D, e.n, cu, maker, key, g.plLevel, src);
      if (e.ev > 0) { R.gold = std::max(1, R.gold * e.ev / 100); R.xp = std::max(1, R.xp * e.ev / 100); }   // (P1b) reward <band> <pct>
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
    case EffType::Deed: {
      // (M8) the player's deed on the companions' deeds bus, about the story's hook place
      const Binding* hb = bindingOf(in, "home");
      const ew::Gid about = hb ? (hb->site ? hb->site : hb->id) : (in.cast.empty() ? 0 : in.cast[0].site);
      g.deed((party::Deed)e.n, about, std::max(1, e.ev));
      break;
    }
    case EffType::Recruit: {
      // (M8) a rescued or sworn person of the story joins the party: a census resident with their census identity, anyone
      // else as a stranger of the hook's culture standing where the story's person stood
      const Binding* b = bindingOf(in, e.args[0]);
      const dsl::Script* sc = scriptOf(in);
      if (!b || !sc) break;
      const int ri = sc->role(e.args[0]);
      Vec2 at = g.pl().p + Vec2(14.0f, 0.0f);
      for (const Actor& a : g.actors) if (ri >= 0 && isStoryPerson(a) && a.slot == personSlot(in.id, ri)) at = a.p;
      const party::Origin origin = e.n == 1 ? party::Origin::Saga : party::Origin::Rescued;
      int mi = -1;
      const int ix = residentIndexOf(g, *b);
      if (ix >= 0) {
        const int sh = siteByIdLoad(g, b->site);
        life::Census* c = sh >= 0 ? g.life.census(g.world, sh) : nullptr;
        int ai = -1;
        if (c && ix < (int)c->res.size() && c->res[(size_t)ix].actor >= 0 && host().findActor) ai = host().findActor(g, c->res[(size_t)ix].actor);
        if (ai > 0) mi = g.partyRecruit(g.actors[(size_t)ai], origin);
        else if (sh >= 0) {
          Actor tmp;
          tmp.id = -98; tmp.npc = true; tmp.human = true; tmp.site = sh; tmp.resident = ix; tmp.p = at; tmp.name = b->name;
          mi = g.partyRecruit(tmp, origin);
        }
      } else {
        uint64_t cu = 0;
        const int hh = in.cast.empty() ? -1 : g.world.siteHandle(in.cast[0].site);
        if (hh >= 0) cu = g.world.sites[(size_t)hh].culture;
        mi = g.partyRecruitStranger(b->name, b->female, cu, (uint32_t)(b->id ^ (b->id >> 32)), origin, at);
      }
      if (mi >= 0) in.vars["_hide_" + e.args[0]] = 1;
      else notice(g, b->name + " WOULD HAVE COME WITH YOU, BUT YOUR COMPANY IS FULL.");
      break;
    }
    case EffType::Approve: case EffType::Bond: {
      const int mi = companionOf(g, in);
      if (mi < 0) break;
      party::Member& m = g.party.members[(size_t)mi];
      if (e.type == EffType::Approve) m.approval = (int8_t)std::clamp((int)m.approval + e.n, -100, 100);
      else m.bond = (uint8_t)std::clamp((int)m.bond + e.n, 0, 100);
      break;
    }
    case EffType::Fate: {
      const int mi = companionOf(g, in);
      if (mi < 0) break;
      party::Member& m = g.party.members[(size_t)mi];
      m.flags |= party::MF_QUEST_DONE;
      if (e.args[0] == "home") g.partyCommand(mi, party::Cmd::Home);
      else g.partyDismiss(mi);
      break;
    }
    // ---- (P1b) the shared mechanics
    case EffType::Days: in.vars[e.args[0]] = g.day - storyStartDay(in); break;
    case EffType::Follow: in.vars["_follow_" + e.args[0]] = 1; break;    // (ENGINE lane: the person walks behind the player)
    case EffType::Stay: in.vars["_follow_" + e.args[0]] = 0; break;
    case EffType::Hostile: in.vars["_hostile_" + e.args[0]] = 1; break;  // (ENGINE lane: their actor turns on the player)
    case EffType::Calm: {   // (ENGINE lane: the site's creatures stand down)
      in.vars["_calm_" + e.args[0]] = 1;
      // (P1b fixer) said in an end stage, the calm outlives the story (engineSync rebuilds calm_ from running stories
      // only): a persistent mark the stand-down check reads
      const dsl::Script* cs = scriptOf(in);
      if (cs && in.stage >= 0 && in.stage < (int)cs->stages.size() && cs->stages[(size_t)in.stage].kind == dsl::StageKind::End)
        if (const Binding* b = bindingOf(in, e.args[0])) {
          const uint64_t site = b->site ? b->site : b->id;
          if (site) g.marks[storyMarkKey(site, MK_P1B_CALM)] = g.day;
        }
      break;
    }
    case EffType::Ambush: {
      // n hostile foes come at the player from 8-12 tiles off, outdoors only (the hook's danger: their level). A road
      // ambush (someone walks with the player: an escort goal or `do follow`) set off inside a settlement waits until
      // the player is out on the road, so a wagon leaving the square does not ring the town's raid bell (engineSync)
      if (g.inside || !host().spawnMonster) break;
      if (storyOnRoad(in) && g.settlementAt(g.pl().p) >= 0) {
        in.vars["_amb_m"] = e.n; in.vars["_amb_n"] = e.ev; in.vars["_amb_stage"] = in.stage;
        break;
      }
      storyAmbush(g, in, e.n, e.ev);
      break;
    }
    case EffType::Census: {
      const ew::Gid site = bsite(e.args[0]);
      const int h = site ? siteByIdLoad(g, site) : -1;
      life::Census* c = h >= 0 && g.world.sites[(size_t)h].settlement() ? g.life.census(g.world, h) : nullptr;
      if (!c) break;
      switch (e.n) {
        case dsl::CEN_MOOD: c->mood = (uint8_t)std::clamp((int)c->mood + e.ev, 0, 100); break;
        case dsl::CEN_STOCK: {
          const int gd = dsl::goodWord(e.args.size() > 1 ? e.args[1] : std::string());
          if (gd >= 0) c->stock[(size_t)gd] = (uint16_t)std::clamp((int)c->stock[(size_t)gd] + e.ev, 0, 60000);
          break;
        }
        case dsl::CEN_PRICE: {
          const int gd = dsl::goodWord(e.args.size() > 1 ? e.args[1] : std::string());
          if (gd >= 0) c->price[(size_t)gd] = (uint8_t)std::clamp(e.ev, 25, 250);
          break;
        }
        case dsl::CEN_FAMINE: c->famineUntil = std::max(c->famineUntil, (int32_t)(g.day + e.ev)); break;
        case dsl::CEN_FESTIVAL: c->festivalDay = g.day; break;
        case dsl::CEN_REFUGEES: g.life.storyRefugees(g.world, *c, e.ev); break;
        default: break;
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

}  // namespace story
