// M4 "Banners": the story engine in the game (Game::story*; rpg/story/story.h). STORY lane.
// The hooks run the scripts (talk, choices, kills, entering places, using props, the per-step objectives), turn the
// realm's events into rumours and news (Ev::News; the speakers of VISION_PLAN 4.6), place notice boards and heralds,
// and the lore of ruins (15.3). storyEntered is also called while a save loads (deserialize re-enters the building or
// site it was made in).
//
// The engine (rpg/story/*.cpp) sees Game's public state; what it may do beyond that (give gold and XP, add items, play
// sounds, spawn people, complete quests) goes through story::host(), a table of captureless lambdas written HERE,
// inside Game's member functions, so they may call its private helpers.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include "engine/audio.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/story/dsl.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/coords.h"
#include "rpg/world/source.h"
#include "rpg/sim/stream.h"

namespace {
uint64_t strHash64(const std::string& s) {
  uint64_t h = 1469598103934665603ull;
  for (char c : s) { h ^= (uint8_t)c; h *= 1099511628211ull; }
  return h;
}
const char* const kIdle[] = {"NOT NOW. I NEED TO THINK.", "YOU KNOW WHAT HAS TO BE DONE. I'LL BE HERE.",
                             "I'LL WAIT. I'VE GOT GOOD AT WAITING.", "GO ON. THE LONGER WE TALK, THE LESS GETS DONE."};
}  // namespace

// the host table (see the header comment): filled on the first hook call of any Game
#define STORY_HOST_INSTALL()                                                                                                      \
  do {                                                                                                                             \
    story::Host& H = story::host();                                                                                               \
    if (!H.gold) {                                                                                                                 \
      H.gold = [](Game& g, int n) { g.giveGold(n); };                                                                             \
      H.xp = [](Game& g, int n) { g.gainXp(n); };                                                                                 \
      H.item = [](Game& g, const Item& it) { g.addItem(it); };                                                                    \
      H.sound = [](Game& g, int s, float pitch, float vol) { g.sfx(s, g.pl().p, pitch, vol); };                                  \
      H.say = [](Game& g, const std::string& s) { g.say(s); };                                                                    \
      H.spawnHuman = [](Game& g, const Spawn& sp, float x, float y) { return g.spawnHuman(sp, Vec2(x, y)); };                    \
      H.findActor = [](const Game& g, int id) { return g.findActor(id); };                                                        \
      H.accept = [](Game& g, const Quest& q) { g.acceptQuest(q); };                                                               \
      H.complete = [](Game& g, int qid) {                                                                                          \
        for (Quest& q : g.quests) if (q.id == qid && q.state == QState::Complete) { g.completeQuest(q); break; }                  \
      };                                                                                                                           \
      H.spawnMonster = [](Game& g, int m, float x, float y, int level, bool boss) {                                               \
        return g.spawnMonster((art::Monster)m, Vec2(x, y), level, boss);                                                          \
      };                                                                                                                           \
      H.loot = [](Game& g, int level) {                                                                                            \
        Rng r((uint32_t)(g.seed ^ (uint64_t)g.day * 2654435761ull ^ (uint64_t)g.nextQuestId * 40503ull));                        \
        g.addItem(randomLoot(r, level, true));                                                                                     \
      };                                                                                                                           \
      H.talkTo = [](Game& g, int id) { const int ai = g.findActor(id); if (ai >= 0) g.talkTo(g.actors[(size_t)ai]); };           \
    }                                                                                                                              \
  } while (0)

namespace {

// persons and foes of the running stories, spawned where they are when the player comes near (outdoors), put away when
// the player leaves or the story no longer needs them
void syncPersons(Game& g) {
  using namespace story;
  // (M6b) a resident role of a running story needs a stand-in body only while the story waits to talk to them and their
  // own body is not in play (asleep in the aggregate, across town, a census not on the street); an invented resident (no
  // census where they live) is met like a person of the story
  auto residentWanted = [&](const Instance& in, const dsl::Script& s, const dsl::RoleDef& R, const Binding& b) -> bool {
    if (b.trade != RESIDENT_CENSUS) return true;
    const dsl::StageDef& G = s.stages[(size_t)in.stage];
    if (G.kind != dsl::StageKind::Talk || G.talk != R.name) return false;
    const life::Census* c = g.life.find(b.site);
    const int ix = residentIndexOf(g, b);
    if (!c || ix < 0) return false;
    const life::Resident& r = c->res[(size_t)ix];
    if (r.flags & (life::RF_DEAD | life::RF_AWAY)) return false;
    if (r.actor >= 0 && host().findActor && host().findActor(g, r.actor) >= 0) return false;   // their own body is here
    return true;
  };
  // put away the ones no longer wanted
  for (size_t k = 1; k < g.actors.size();) {
    const Actor& a = g.actors[k];
    bool keep = true;
    if (isStoryPerson(a)) {
      keep = false;
      for (const Instance& in : g.story.running()) {
        if (in.done) continue;
        const dsl::Script* s = scriptOf(in);
        if (!s) continue;
        for (size_t ri = 0; ri < s->roles.size(); ri++) {
          if (a.slot != personSlot(in.id, (int)ri)) continue;
          auto hid = in.vars.find("_hide_" + s->roles[ri].name);
          auto dead = in.vars.find("_dead_" + s->roles[ri].name);
          bool hidden = (hid != in.vars.end() && hid->second) || (dead != in.vars.end() && dead->second);
          if (s->roles[ri].kind == dsl::RoleKind::Resident) {
            const Binding* b = bindingOf(in, s->roles[ri].name);
            if (!b || !residentWanted(in, *s, s->roles[ri], *b)) hidden = true;
          }
          const bool talking = g.mode == Mode::Dialogue && g.dlg.actor == a.id;
          const bool near = len2(a.p - g.pl().p) < (64.0f * TILE) * (64.0f * TILE);
          keep = talking || (!hidden && near) || a.aggro;
        }
      }
      if (a.st == AState::Dead) keep = true;   // (the corpse clean-up takes it)
    }
    if (!keep) { g.actors.erase(g.actors.begin() + (std::ptrdiff_t)k); continue; }
    k++;
  }
  if (g.inside || !host().spawnHuman) return;
  int32_t px, py;
  playerGlobal(g, px, py);
  for (const Instance& in : g.story.running()) {
    if (in.done) continue;
    const dsl::Script* s = scriptOf(in);
    if (!s) continue;
    for (size_t ri = 0; ri < s->roles.size(); ri++) {
      const dsl::RoleDef& R = s->roles[ri];
      if (R.kind != dsl::RoleKind::Person && R.kind != dsl::RoleKind::Foe && R.kind != dsl::RoleKind::Resident) continue;
      const Binding* b = bindingOf(in, R.name);
      if (!b || !b->hasPos) continue;
      auto hid = in.vars.find("_hide_" + R.name);
      auto dead = in.vars.find("_dead_" + R.name);
      if ((hid != in.vars.end() && hid->second) || (dead != in.vars.end() && dead->second)) continue;
      if (R.kind == dsl::RoleKind::Resident && !residentWanted(in, *s, R, *b)) continue;
      // foes in caves and ruins wait inside (Engine::onEntered)
      const int sh = g.world.siteHandle(b->site);
      if (R.kind == dsl::RoleKind::Foe && sh >= 0 && (g.world.sites[(size_t)sh].type == SiteType::Cave || g.world.sites[(size_t)sh].type == SiteType::Ruin)) continue;
      const int64_t dx = px - b->gx, dy = py - b->gy;
      if (dx * dx + dy * dy > 36 * 36) continue;
      const int slot = personSlot(in.id, (int)ri);
      bool there = false;
      for (const Actor& a : g.actors) if (a.slot == slot && isStoryPerson(a)) there = true;
      if (there) continue;
      int tx = 0, ty = 0;
      if (!freeTileNear(g, b->gx - g.world.ox, b->gy - g.world.oy, tx, ty, (uint32_t)(b->id ^ (b->id >> 32)))) continue;
      Spawn sp;
      sp.npc = R.kind != dsl::RoleKind::Foe;
      sp.bandit = R.kind == dsl::RoleKind::Foe;
      sp.boss = sp.bandit;
      sp.role = Role::Villager;
      sp.site = sh >= 0 ? sh : -1;
      sp.slot = slot;
      sp.x = tx; sp.y = ty;
      if (sp.site < 0 && sp.bandit) continue;   // (a foe needs its site's level)
      const int id = host().spawnHuman(g, sp, tx * TILE + 8.0f, ty * TILE + 10.0f);
      const int ai = host().findActor(g, id);
      if (ai < 0) continue;
      Actor& a = g.actors[(size_t)ai];
      a.name = b->name;
      a.fromMap = false;
      a.site = -1;   // (not a townsperson: never streamed away, offers no radiant work)
      a.slot = slot;
      if (b->female) { a.look.beard = false; if (a.look.hair == art::Hair::Short || a.look.hair == art::Hair::Bald || a.look.hair == art::Hair::Mohawk) a.look.hair = art::Hair::Braids; if (sp.npc) a.look.outfit = art::Outfit::Dress; }
      else { if (a.look.hair == art::Hair::Long || a.look.hair == art::Hair::Braids || a.look.hair == art::Hair::Ponytail) a.look.hair = art::Hair::Short; if (a.look.outfit == art::Outfit::Dress) a.look.outfit = art::Outfit::Tunic; }
    }
  }
}

// a story that begins with a realm event: the player arrives in a settlement the news has reached
void eventStories(Game& g) {
  using namespace story;
  if (g.inside || g.mode != Mode::Play || g.curSite < 0 || g.curSite >= (int)g.world.sites.size()) return;
  // (a village: the old captain who is all the garrison there is, the elder of a hungry hamlet; towns and cities have
  //  their watch and their granaries)
  if (g.world.sites[(size_t)g.curSite].type != SiteType::Village) return;
  const int st = g.story.offerForPlace(g, (int)dsl::HookKind::Event, g.curSite);
  if (st < 0) {
    // (M6b) a generated story the news brings (saga.h offerForPlace: one a fortnight per village, fresh news only)
    const std::string sid = saga::offerForPlace(g, (int)dsl::HookKind::Event, g.curSite);
    if (sid.empty()) return;
    if (const uint32_t id = g.story.start(g, sid, -1, g.curSite)) {
      saga::Hook hk;
      hk.kind = (int)dsl::HookKind::Event;
      hk.site = g.curSite;
      saga::noteTold(g, sid, id, hk);
    }
    return;
  }
  const dsl::Script& s = dsl::library().scripts[(size_t)st];
  const Site& home = g.world.sites[(size_t)g.curSite];
  const int32_t hx = g.world.ox + home.ex, hy = g.world.oy + home.ey;
  uint32_t fresh = 0;
  for (const realm::WorldEvent& e : g.realm.events()) if ((int)e.type == s.hookEv && rumourReaches(e, g.day, hx, hy)) fresh = std::max(fresh, e.id);
  const uint64_t key = storyMarkKey(strHash64(s.id), MK_STORY_MARK);
  auto it = g.marks.find(key);
  if (!fresh || (it != g.marks.end() && (uint32_t)it->second >= fresh)) return;
  g.marks[key] = (int32_t)fresh;
  g.story.start(g, s.id, -1, g.curSite);
}

// the palace's ruler is the realm's (or the story's, after a succession the player made): name and title follow it
void crownTheKing(Game& g) {
  for (size_t k = 1; k < g.actors.size(); k++) {
    Actor& a = g.actors[k];
    if (a.role != Role::King) continue;
    const int hs = story::homeSiteOf(g, a);
    if (hs < 0 || g.world.sites[(size_t)hs].kingdom < 0) continue;
    std::string rn, rt;
    if (!g.story.rulerOf(g, g.world.kingdoms[(size_t)g.world.sites[(size_t)hs].kingdom].id, rn, rt)) continue;
    const std::string nm = rt + " " + rn;
    if (a.name != nm) a.name = nm;
  }
}

bool blocksGossip(const Dialogue& d) {
  for (const DlgOpt& o : d.opts)
    if (o.action == A_TURNIN || o.action == A_DELIVER || o.action == A_ESCORT || o.action == A_MAIN || (o.action >= DLG_STORY && o.action < DLG_END))
      return true;
  return false;
}

}  // namespace

void Game::storyStep(float dt) {
  STORY_HOST_INSTALL();
  if (mode == Mode::Title || mode == Mode::Creator) return;
  story.stepT_ += dt;
  if (story.stepT_ >= 0.25f) {
    story.stepT_ = 0;
    story.step(*this);
    syncPersons(*this);
    if (inside) crownTheKing(*this);
    story::heraldsProclaim(*this);
  }
  story.clock_ += dt;
  if (mode == Mode::Play && !travelling()) {
    using namespace story;
    // (fixer M6b r1) the composer's needs (caves, camps, ruins and fallen lords up to ~640 tiles out) and the caster read
    // the region plans 3 regions round a hook; building the ones not yet planned in the talk frame cost 60-80 ms on a
    // desktop. The prefetcher builds them (World::storyRing: off the main thread natively, in the pump's budget on the
    // web); the tale scan below waits until they are ready. Without a streamer (tests) one is planned every 0.08 s here.
    story.warmT_ += dt;
    bool warmDone = false;
    if (story.warmT_ >= 0.08f) {
      story.warmT_ = 0;
      warmDone = true;
      if (story.warmSeed_ != seed || story.warmed_.size() > 4096) { story.warmed_.clear(); story.warmSeed_ = seed; }
      int32_t px, py;
      story::playerGlobal(*this, px, py);
      const int32_t rx0 = ew::regionOf(px), ry0 = ew::regionOf(py);
      const int32_t bx0 = ew::regionOf(world.ox - ew::REGION), by0 = ew::regionOf(world.oy - ew::REGION);
      const int32_t bx1 = ew::regionOf(world.ox + World::WIN + ew::REGION - 1), by1 = ew::regionOf(world.oy + World::WIN + ew::REGION - 1);
      const int R = std::max(1, std::min(3, world.storyRing));
      for (int32_t ry = ry0 - R; ry <= ry0 + R && warmDone; ry++)
        for (int32_t rx = rx0 - R; rx <= rx0 + R && warmDone; rx++) {
          if (rx >= bx0 && rx <= bx1 && ry >= by0 && ry <= by1) continue;   // (loaded with the window)
          const uint64_t k = ((uint64_t)(uint32_t)rx << 32) | (uint32_t)ry;
          if (story.warmed_.count(k)) continue;
          if (world.streamer && world.storyRing > 0) {
            if (world.streamer->hasRegion(rx, ry)) story.warmed_.insert(k);
            else warmDone = false;
          } else {
            story.warmed_.insert(k);
            (void)world.regionPlan(rx, ry);
            warmDone = false;
          }
        }
    }
    // (fixer M6b r1) who near the player has a tale to tell: one person per 0.3 s (once the regions are warm), so the
    // talk frame finds the offer memoised, and the people with a tale carry a small marker (render_markers.cpp)
    story.scanT_ += dt;
    // (fixer M6b r2) the composer's world facts at the settlement the player is in (caves, camps, ruins, the coast, the
    // census) one per warm tick once the regions are warm, so the first offer composed there costs ~3-7 ms (desktop),
    // not 15-27 ms in the arrival frame. The scan waits until they are known. (The composer's tables: Engine::reset.)
    saga::warmComposer();
    bool hookWarm = true;
    if (warmDone && curSite >= 0 && curSite < (int)world.sites.size() && world.sites[(size_t)curSite].settlement())
      hookWarm = saga::warmHook(*this, curSite);
    if (warmDone && hookWarm && story.scanT_ >= 0.3f) {
      story.scanT_ = 0;
      int best = -1;
      float bestAt = 1e30f;
      for (size_t i = 1; i < actors.size(); i++) {
        const Actor& a = actors[i];
        if (!a.npc || !a.human || a.st == AState::Dead || story::isStoryPerson(a)) continue;
        if (len2(a.p - pl().p) > (18.0f * TILE) * (18.0f * TILE)) continue;
        auto it = story.tales_.find(a.id);
        const float at = it == story.tales_.end() || it->second.key != npcKey(a) ? -1.0f : it->second.at;
        if (at >= 0 && story.clock_ - at < 12.0f) continue;
        if (at < bestAt) { bestAt = at; best = (int)i; }
      }
      if (best >= 0) {
        Actor& a = actors[(size_t)best];
        story::Engine::TaleScan ts;
        ts.at = story.clock_;
        ts.key = npcKey(a);
        bool waiting = false;   // (someone a running story waits on has the quest's own marker)
        for (const story::Instance& in : story.running_) if (!in.done && story.talksTo(*this, in, a)) waiting = true;
        if (!waiting) {
          const int idx = story.offerFor(*this, a);
          if (idx >= 0 && story.timesPlayed(dsl::library().scripts[(size_t)idx].id) == 0) ts.tale = true;
          else {
            const std::string sid = saga::offerFor(*this, a);
            ts.tale = (!sid.empty() && story::scriptById(sid)) || idx >= 0;
          }
        }
        if (story.tales_.size() > 256) story.tales_.clear();
        story.tales_[a.id] = ts;
      }
    }
  }
  story.placeT_ += dt;
  if (story.placeT_ >= 0.7f) {
    story.placeT_ = 0;
    if (!inside && !travelling()) {
      story::placeBoards(*this);
      story::spawnHeralds(*this);
      story::placeRuinStatues(*this);
      eventStories(*this);
    }
  }
}

void Game::storyTalk(Actor& a) {
  STORY_HOST_INSTALL();
  using namespace story;
  story.sagaOffers_.clear();   // (M6b: the offers of a dialogue live as long as it)
  story.tales_.erase(a.id);    // (fixer M6b r1: looked at again by the next scan)
  // (fixer M6b r3) a running story bound to this actor speaks first, heralds included: a herald-hooked saga's
  // `talk giver` stages are only reachable here, so checking the herald first left those stories stuck for good
  for (Instance& in : story.running_)
    if (!in.done && story.talksTo(*this, in, a)) { story.showDialogue(*this, in); return; }
  if (a.role == Role::Herald && a.slot == HERALD_SLOT) { heraldTalk(*this, a); return; }
  if (isStoryPerson(a)) {
    dlg.text = kIdle[hash32((uint32_t)a.slot ^ (uint32_t)day) % 4];
    return;
  }
  const bool busy = blocksGossip(dlg);
  // a story to tell
  const int idx = busy ? -1 : story.offerFor(*this, a);
  // (M6b) the library's unplayed tales first, then a generated story (saga.h offerFor); a tale already told comes back
  // only when no generated story fits this teller
  if (!busy && (idx < 0 || story.timesPlayed(dsl::library().scripts[(size_t)idx].id) > 0)) {
    const std::string sid = saga::offerFor(*this, a);
    const dsl::Script* sc = sid.empty() ? nullptr : scriptById(sid);
    if (sc) {
      story.sagaOffers_.push_back(sid);
      if (!sc->hint.empty()) dlg.text = sc->hint;
      dlg.opts.insert(dlg.opts.begin(), {sc->pitch, DLG_STORY + SA_SAGA + (int)story.sagaOffers_.size() - 1, homeSiteOf(*this, a)});
      saga::Hook hk;
      hk.kind = (int)dsl::HookKind::Npc;
      hk.actor = a.id;
      hk.site = homeSiteOf(*this, a);
      saga::noteOffered(*this, hk, sid);
      return;
    }
  }
  if (idx >= 0) {
    const dsl::Script& s = dsl::library().scripts[(size_t)idx];
    if (!s.hint.empty()) dlg.text = s.hint;
    dlg.opts.insert(dlg.opts.begin(), {s.pitch, DLG_STORY + SA_START + idx, homeSiteOf(*this, a)});
    return;
  }
  if (busy) return;
  // someone who remembers what the player did
  const int hs = homeSiteOf(*this, a);
  const uint64_t key = npcKey(a);
  const std::string* mem = story.memoryOf(key);
  if (!mem && hs >= 0) mem = story.memoryOf(ew::mix64(world.sites[(size_t)hs].id ^ (0x7EADull + (uint64_t)a.role)));
  if (mem && ((hash32((uint32_t)key ^ (uint32_t)day) & 1) == 0 || story.memories().back().line == *mem)) { dlg.text = *mem; return; }
  // the realm's news (VISION_PLAN 4.6): innkeepers 70 %, travellers 50 %, guards 60 % (war news only), the rest 25 %
  int32_t gx, gy;
  playerGlobal(*this, gx, gy);
  const uint64_t voice = hs >= 0 ? world.sites[(size_t)hs].culture : 0;
  const uint32_t roll = hash32((uint32_t)key ^ (uint32_t)(key >> 29) ^ (uint32_t)day * 7919u) % 100;
  if ((int)roll < speakerChance((int)a.role) && a.role != Role::Child) {
    if (const realm::WorldEvent* e = pickRumour(*this, gx, gy, a.role == Role::Guard, key)) {
      if (!e->heard || roll % 3 == 0) { dlg.text = hearEvent(*this, *e, voice); return; }
    }
  }
  // the road to war, foreshadowed (15.6.3): prices, troops, refugees
  if (roll % 4 == 1 && hs >= 0) {
    const ew::Gid k = world.sites[(size_t)hs].kingdom >= 0 ? world.kingdoms[(size_t)world.sites[(size_t)hs].kingdom].id : 0;
    const std::string f = foreshadowLine(*this, k, voice, key ^ (uint64_t)day);
    if (!f.empty()) { dlg.text = f; return; }
  }
  // the player's deeds, as the town tells them
  if (roll % 5 == 2 && hs >= 0)
    for (auto it = story.facts().rbegin(); it != story.facts().rend(); ++it)
      if (it->site == world.sites[(size_t)hs].id && day - it->day < 40) { dlg.text = "HAVE YOU HEARD? " + it->text; return; }
}

bool Game::storyChoose(const DlgOpt& o) {
  STORY_HOST_INSTALL();
  using namespace story;
  const int k = o.action - DLG_STORY;
  if (k < SA_START) return story.choose(*this, (uint32_t)o.arg, k - SA_OPT);
  // a story was taken up: the teller says its first lines, or points the way to whoever must say them
  auto begun = [&](const dsl::Script& s, uint32_t id, int ai) {
    Instance* in = story.find(id);
    if (in && ai >= 0 && story.talksTo(*this, *in, actors[(size_t)ai])) {
      dlg.opts.clear();
      story.showDialogue(*this, *in);
      dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
    } else if (in && in->stage >= 0 && in->stage < (int)s.stages.size() && s.stages[(size_t)in->stage].kind == dsl::StageKind::Talk) {
      // (fixer M4 r1) the story's first words belong to someone who is not here (the mother the notice names): the
      // notice (or the teller) only points the way; the speaker says their lines when the player finds them
      const Binding* b = nullptr;
      for (const Binding& c : in->cast) if (c.role == s.stages[(size_t)in->stage].talk) b = &c;
      const std::string who = b ? b->name : std::string();
      dlg.text = (s.hint.empty() ? std::string() : s.hint + " ") + (who.empty() ? "(" + story.journalText(*this, *in) + ")" : "(SEEK OUT " + who + ".)");
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
    } else if (in) {
      const std::string say = story.sayText(*this, *in);
      dlg.text = say.empty() ? "YOU COMMIT IT TO MEMORY. (" + story.journalText(*this, *in) + ")" : say;
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
    }
  };
  auto dropOpt = [&]() {
    for (size_t i = 0; i < dlg.opts.size(); i++) if (dlg.opts[i].action == o.action) { dlg.opts.erase(dlg.opts.begin() + (std::ptrdiff_t)i); break; }
  };
  if (k < SA_BOARD) {
    const int idx = k - SA_START;
    const dsl::Library& L = dsl::library();
    if (idx < 0 || idx >= (int)L.scripts.size()) return false;
    const dsl::Script& s = L.scripts[(size_t)idx];
    const int ai = findActor(dlg.actor);
    const bool npcHook = s.hook == dsl::HookKind::Npc || s.hook == dsl::HookKind::Herald;
    std::string why;
    const uint32_t id = story.start(*this, s.id, npcHook && ai >= 0 ? actors[(size_t)ai].id : -1, o.arg, &why);
    dropOpt();
    if (!id) {
      if (ai >= 0) marks[storyMarkKey(npcKey(actors[(size_t)ai]), MK_STORY_GIVER)] = day;   // (no second try from them)
      dlg.text = npcHook ? "...NO. NO, FORGET I SAID ANYTHING. IT'S NOTHING." : "THE NOTICE IS TOO RAIN-SOAKED TO READ.";
      return true;
    }
    begun(s, id, ai);
    return true;
  }
  if (k >= SA_SAGA && k < SA_SAGA + 100) {
    // (M6b) a generated story offered in this dialogue (Engine::sagaOffers_; arg: the hook place)
    const int i = k - SA_SAGA;
    if (i < 0 || i >= (int)story.sagaOffers_.size()) return false;
    const std::string sid = story.sagaOffers_[(size_t)i];
    const dsl::Script* sp = scriptById(sid);
    dropOpt();
    if (!sp) { dlg.text = "...NO. IT'S GONE FROM ME. FORGIVE ME."; return true; }
    const dsl::Script& s = *sp;
    const int ai = findActor(dlg.actor);
    const bool npcHook = s.hook == dsl::HookKind::Npc || s.hook == dsl::HookKind::Herald;
    const int hookActor = npcHook && ai >= 0 ? actors[(size_t)ai].id : -1;
    std::string why;
    const uint32_t id = story.start(*this, sid, hookActor, o.arg, &why);
    if (!id) {
      if (ai >= 0) marks[storyMarkKey(npcKey(actors[(size_t)ai]), MK_STORY_GIVER)] = day;
      dlg.text = npcHook ? "...NO. NO, FORGET I SAID ANYTHING. IT'S NOTHING." : "THE NOTICE IS TOO RAIN-SOAKED TO READ.";
      return true;
    }
    saga::Hook hk;
    hk.kind = (int)s.hook;
    hk.actor = hookActor;
    hk.site = o.arg;
    saga::noteTold(*this, sid, id, hk);
    // (the script pointer may not outlive the cache: look it up again)
    if (const dsl::Script* s2 = scriptById(sid)) begun(*s2, id, ai);
    return true;
  }
  return boardChoose(*this, k, o.arg);
}

void Game::storyKill(const Actor& victim, bool byPlayer) {
  STORY_HOST_INSTALL();
  story.onKill(*this, victim, byPlayer);
}

void Game::storyEntered(int site, int bldg) {
  STORY_HOST_INSTALL();
  if (site >= 0 && site < (int)world.sites.size() && world.sites[(size_t)site].type == SiteType::Ruin) {
    story.ruinProps_ = story::placeRuinLore(*this, site);
    story.ruinPropsSite_ = world.sites[(size_t)site].id;
  } else {
    story.ruinProps_.clear();
    story.ruinPropsSite_ = 0;
  }
  if (bldg >= 0) crownTheKing(*this);
  story.onEntered(*this, site, bldg);
}

bool Game::storyUseProp(art::Prop p, int tx, int ty) {
  STORY_HOST_INSTALL();
  using namespace story;
  if (p == art::Prop::NoticeBoard) return useBoard(*this, tx, ty);
  if (p == art::Prop::ToppledStatue && !inside) { const bool r = readStatue(*this, tx, ty); story.onUseProp(*this, (int)p, tx, ty); return r; }
  if (art::isLoreProp(p)) {
    if (!readLore(*this, tx, ty)) return false;
    story.onUseProp(*this, (int)p, tx, ty);
    // a ruin's words may begin a story (a dead adventurer's journal, an oath carved for the living)
    if (inside && subSite >= 0) {
      const int st = story.offerForPlace(*this, (int)dsl::HookKind::Ruin, subSite);
      if (st >= 0) {
        const dsl::Script& s = dsl::library().scripts[(size_t)st];
        const uint64_t key = storyMarkKey(world.sites[(size_t)subSite].id ^ strHash64(s.id), MK_STORY_MARK);
        if (!marks.count(key)) {
          marks[key] = day;
          if (const uint32_t id = story.start(*this, s.id, -1, subSite)) {
            if (const Instance* in = story.find(id)) {
              const std::string say = story.sayText(*this, *in);
              if (!say.empty() && dlg.text.size() + say.size() < 290) dlg.text += " " + say;
            }
          }
        }
      } else {
        // (M6b) a generated story the ruin's words begin (saga.h offerForPlace: rare, one a fortnight here)
        const std::string sid = saga::offerForPlace(*this, (int)dsl::HookKind::Ruin, subSite);
        if (!sid.empty()) {
          if (const uint32_t id = story.start(*this, sid, -1, subSite)) {
            saga::Hook hk;
            hk.kind = (int)dsl::HookKind::Ruin;
            hk.site = subSite;
            saga::noteTold(*this, sid, id, hk);
            if (const Instance* in = story.find(id)) {
              const std::string say = story.sayText(*this, *in);
              if (!say.empty() && dlg.text.size() + say.size() < 290) dlg.text += " " + say;
            }
          }
        }
      }
    }
    return true;
  }
  // shrines, altars: the story notes it, the blessing still happens
  story.onUseProp(*this, (int)p, tx, ty);
  return false;
}
