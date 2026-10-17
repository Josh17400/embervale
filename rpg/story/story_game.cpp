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
#include "rpg/sim/jobs_ops.h"
#include "rpg/sim/talk.h"
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
        g.addItem(story::storyLoot(level, g.plLevel, r.next(), false));   /* (M8b) the gear rules, never randomLoot */          \
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

// (P1 Fresh Roads) the stand-ins of a story's people are not statues: by day each walks a small round of its own
// (three stops near where the story put it, a stop every ~45 game minutes, each person's round and timing their own;
// the townsfolk AI strolls about each stop), and at night it waits on the doorstep of the nearest house, where the
// player can still find it (questTarget's story branch follows the live actor). Foes keep their ground.
void standInsLive(Game& g) {
  using namespace story;
  if (g.inside) return;
  for (Actor& a : g.actors) {
    if (!isStoryPerson(a) || !a.npc || a.hostile || a.st == AState::Dead || a.aggro) continue;
    const uint32_t inst = (uint32_t)((a.slot - PERSON_SLOT0) / PERSON_ROLES);
    const int ri = (a.slot - PERSON_SLOT0) % PERSON_ROLES;
    const Binding* b = nullptr;
    for (const Instance& in : g.story.running()) {
      if (in.done || in.id % 1024u != inst) continue;
      const dsl::Script* s = scriptOf(in);
      if (s && ri < (int)s->roles.size()) b = bindingOf(in, s->roles[(size_t)ri].name);
      break;
    }
    if (!b || !b->hasPos) continue;
    const int ax = b->gx - g.world.ox, ay = b->gy - g.world.oy;
    const uint32_t salt = (uint32_t)(b->id ^ (b->id >> 32)) ^ (uint32_t)a.slot * 0x9E37u;
    int tx = ax, ty = ay;
    if (g.isNight()) {
      // the doorstep of the nearest house within 16 tiles (else the stop by day it is nearest to)
      int best = -1;
      int64_t bd = 16 * 16;
      for (size_t k = 0; k < g.world.over.bldgs.size(); k++) {
        const Bldg& B = g.world.over.bldgs[k];
        const int64_t dx = B.doorX() - ax, dy = B.doorY() + 1 - ay, d2 = dx * dx + dy * dy;
        if (d2 < bd) { bd = d2; best = (int)k; }
      }
      if (best >= 0) { tx = g.world.over.bldgs[(size_t)best].doorX(); ty = g.world.over.bldgs[(size_t)best].doorY() + 1; }
    } else {
      // the round: stop 0 the story's own place, stops 1 and 2 a few tiles off it; each person's clock is offset
      const double gameMin = ((double)g.day * 24.0 + (double)g.hour) * 60.0;
      const int period = 35 + (int)(hash32(salt) % 25);   // 35..59 game minutes a stop
      const int stop = (int)(((int64_t)(gameMin / period) + (int64_t)(hash32(salt ^ 0x51u) % 3)) % 3);
      if (stop > 0) {
        const uint32_t h = hash32(salt + (uint32_t)stop * 7919u);
        const int r = 4 + (int)(h % 6);
        const int dir = (int)((h >> 8) % 8);
        static const int kDx[8] = {1, 1, 0, -1, -1, -1, 0, 1}, kDy[8] = {0, 1, 1, 1, 0, -1, -1, -1};
        tx = ax + kDx[dir] * r;
        ty = ay + kDy[dir] * r;
      }
    }
    int fx = tx, fy = ty;
    if (!freeTileNear(g, tx, ty, fx, fy, salt)) continue;
    const Vec2 home((float)fx * TILE + 8.0f, (float)fy * TILE + 10.0f);
    if (len2(home - a.home) > 4.0f) { a.home = home; a.goal = home; a.thinkT = 0.5f + (float)(hash32(salt ^ (uint32_t)g.day) % 100) * 0.02f; }
  }
}

// a story that begins with a realm event: the player arrives in a settlement the news has reached
void eventStories(Game& g) {
  using namespace story;
  if (g.inside || g.mode != Mode::Play || g.curSite < 0 || g.curSite >= (int)g.world.sites.size()) return;
  // (a village: the old captain who is all the garrison there is, the elder of a hungry hamlet; towns and cities have
  //  their watch and their granaries)
  if (g.world.sites[(size_t)g.curSite].type != SiteType::Village) return;
  const Offer of = placeOffer(g, (int)dsl::HookKind::Event, g.curSite);   // (P1: the one picker, offers.cpp)
  const int st = of.lib;
  if (st < 0) {
    // (M6b) a generated story the news brings (saga.h offerForPlace: one a fortnight per village, fresh news only)
    const std::string sid = of.saga;
    if (sid.empty()) return;
    if (const uint32_t id = g.story.start(g, sid, -1, g.curSite)) {
      saga::Hook hk;
      hk.kind = (int)dsl::HookKind::Event;
      hk.site = g.curSite;
      saga::noteTold(g, sid, id, hk);
    }
    return;
  }
  // (P1) the picker offers a news tale only on news it has not answered yet (offers.cpp eventTaleFresh), so a stale tale
  // no longer stops the village's generated news stories and campaigns (PLAGUE) here
  const dsl::Script& s = dsl::library().scripts[(size_t)st];
  const uint32_t fresh = eventTaleFresh(g, s, g.curSite);
  if (!fresh) return;
  g.marks[eventTaleMarkKey(s)] = (int32_t)fresh;
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

// (M7b) the teller's line while they have a story to tell: who they are first when the player has not met them
// (speech::Memory), proposed as the story's hook (P_STORY: a lower hook's line never replaces it)
void tellerLine(Game& g, const story::dsl::Script& s, const Actor& a) {
  if (s.hint.empty()) return;
  const int hs = story::homeSiteOf(g, a);
  const std::string home = hs >= 0 && hs < (int)g.world.sites.size() ? g.world.sites[(size_t)hs].name : std::string();
  const uint64_t key = g.npcKey(a);
  const bool introduce = key && !g.talkMem.introduced(key);
  const std::string line = g.story.hookLineFor(g, s, a, home, introduce);
  g.story.said_ += line + " ";
  if (speech::propose(g.dlg, speech::P_STORY, speech::Topic::QuestOffer, line) && introduce && line != s.hint) g.talkMem.flag(key, speech::TM_INTRODUCED, g.day);
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
    standInsLive(*this);   // (P1: a round by day, a doorstep by night)
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
        story::Offer of;
        if (!waiting) of = story::tellerOffer(*this, a);   // (P1: the one picker, offers.cpp)
        ts.tale = of.any();
        if (story.tales_.size() > 256) story.tales_.clear();
        story.tales_[a.id] = ts;
        // (P1 Fresh Roads) stumbled on: passing close by a teller, the player overhears what troubles them, and the journal
        // keeps it as a lead (a TALE) to the person; heard once in a while per teller, never from across the square
        if (of.any() && !of.campaign() && len2(a.p - pl().p) < (6.0f * TILE) * (6.0f * TILE)) {
          const uint64_t hk = story::storyMarkKey(ts.key, story::MK_TALE_HEARD);
          auto hm = marks.find(hk);
          const story::dsl::Script* sc = story::scriptById(of.id());
          if (sc && !sc->hint.empty() && (hm == marks.end() || day - hm->second >= 5)) {
            marks[hk] = day;
            std::string said = sc->hint;
            size_t stop = said.find_first_of(".!?");
            // (P1 fixer) a first sentence too short to say anything ("AH, A TRAVELLER...", "IT DOESN'T EAT THEM.") runs on
            // into the next, so the lead carries what troubles them
            while (stop != std::string::npos && stop < 34) {
              size_t e = stop;
              while (e + 1 < said.size() && (said[e + 1] == '.' || said[e + 1] == '!' || said[e + 1] == '?')) e++;
              const size_t nx = said.find_first_of(".!?", e + 1);
              if (nx == std::string::npos) { stop = said.size() - 1; break; }
              stop = nx;
            }
            if (stop != std::string::npos) said = said.substr(0, stop + 1);
            if (said.size() > 90) {
              const size_t sp = said.rfind(' ', 87);
              said = said.substr(0, sp == std::string::npos ? 87 : sp) + "...";
            }
            const int hs = story::homeSiteOf(*this, a);
            const std::string town = hs >= 0 && hs < (int)world.sites.size() ? world.sites[(size_t)hs].name : std::string();
            jobs::Lead L;
            L.from = jobs::LeadFrom::Teller;
            L.qtype = (uint8_t)QType::COUNT;
            L.site = hs >= 0 && hs < (int)world.sites.size() ? world.sites[(size_t)hs].id : 0;
            L.who = ts.key;
            int32_t gx = world.ox + (int32_t)std::floor(a.p.x / TILE), gy = world.oy + (int32_t)std::floor(a.p.y / TILE);
            L.hasPos = true; L.gx = gx; L.gy = gy;
            // (P1 fixer) the lead says what it is: someone troubled, and the words that gave it away
            // (P1 fixer r2) who and what first: the tale's own title ("EJE OF DERGZALT: THE SMITH'S LOST BOOK"), the words
            // overheard after it for the lead's page; a row cut to the journal's column keeps the name and the title
            // (three rows of 'SEEMS TROUBLED: "IT...' could not be told apart). Quietly: the "?" over the teller is the
            // street's cue, and a NEW LEAD toast for every teller passed buried the moment's own notices
            const std::string who = a.name.substr(0, a.name.find(' ')) + (town.empty() ? std::string() : " OF " + town);
            std::string title = sc->title;
            if (title.find('[') != std::string::npos || title.find('{') != std::string::npos) title.clear();   // (an unresolved form)
            L.text = title.empty() ? who + " SEEMS TROUBLED: \"" + said + "\"" : who + ": " + title + " - \"" + said + "\"";
            addLead(L, false);
          }
        }
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
  story.said_.clear();         // (M7b: and what was said in it)
  story.tales_.erase(a.id);    // (fixer M6b r1: looked at again by the next scan)
  // (fixer M6b r3) a running story bound to this actor speaks first, heralds included: a herald-hooked saga's
  // `talk giver` stages are only reachable here, so checking the herald first left those stories stuck for good
  for (Instance& in : story.running_)
    if (!in.done && story.talksTo(*this, in, a)) { story.showDialogue(*this, in); return; }
  if (a.role == Role::Herald && a.slot == HERALD_SLOT) { heraldTalk(*this, a); return; }
  if (isStoryPerson(a)) {
    speech::propose(dlg, speech::P_STORY, speech::Topic::Story, kIdle[hash32((uint32_t)a.slot ^ (uint32_t)day) % 4]);
    return;
  }
  const bool busy = blocksGossip(dlg);
  // a story to tell
  // (P1) the one picker (offers.cpp tellerOffer): a library tale or a generated story (saga.h offerFor)
  const Offer of = busy ? Offer() : tellerOffer(*this, a);
  const int idx = of.lib;
  if (!of.saga.empty()) {
    const std::string sid = of.saga;
    const dsl::Script* sc = scriptById(sid);
    if (sc) {
      story.sagaOffers_.push_back(sid);
      tellerLine(*this, *sc, a);
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
    tellerLine(*this, s, a);
    dlg.opts.insert(dlg.opts.begin(), {s.pitch, DLG_STORY + SA_START + idx, homeSiteOf(*this, a)});
    return;
  }
  if (busy) return;
  // someone who remembers what the player did
  const int hs = homeSiteOf(*this, a);
  const uint64_t key = npcKey(a);
  const std::string* mem = story.memoryOf(key);
  if (!mem && hs >= 0) mem = story.memoryOf(ew::mix64(world.sites[(size_t)hs].id ^ (0x7EADull + (uint64_t)a.role)));
  if (mem && ((hash32((uint32_t)key ^ (uint32_t)day) & 1) == 0 || story.memories().back().line == *mem)) {
    speech::propose(dlg, speech::P_BARK, speech::Topic::Story, *mem);
    return;
  }
  // the realm's news (VISION_PLAN 4.6): innkeepers 70 %, travellers 50 %, guards 60 % (war news only), the rest 25 %
  int32_t gx, gy;
  playerGlobal(*this, gx, gy);
  const uint64_t voice = hs >= 0 ? world.sites[(size_t)hs].culture : 0;
  const uint32_t roll = hash32((uint32_t)key ^ (uint32_t)(key >> 29) ^ (uint32_t)day * 7919u) % 100;
  if ((int)roll < speakerChance((int)a.role) && a.role != Role::Child) {
    if (const realm::WorldEvent* e = pickRumour(*this, gx, gy, a.role == Role::Guard, key)) {
      // (M7b) only told when it is what this speaker leads with (hearing it marks it heard)
      if ((!e->heard || roll % 3 == 0) && dlg.pri <= speech::P_NEWS) { speech::propose(dlg, speech::P_NEWS, speech::Topic::Rumour, hearEvent(*this, *e, voice)); return; }
    }
  }
  // the road to war, foreshadowed (15.6.3): prices, troops, refugees
  if (roll % 4 == 1 && hs >= 0) {
    const ew::Gid k = world.sites[(size_t)hs].kingdom >= 0 ? world.kingdoms[(size_t)world.sites[(size_t)hs].kingdom].id : 0;
    const std::string f = foreshadowLine(*this, k, voice, key ^ (uint64_t)day);
    if (!f.empty() && speech::propose(dlg, speech::P_NEWS, speech::Topic::News, f)) return;
  }
  // the player's deeds, as the town tells them
  if (roll % 5 == 2 && hs >= 0)
    for (auto it = story.facts().rbegin(); it != story.facts().rend(); ++it)
      if (it->site == world.sites[(size_t)hs].id && day - it->day < 40) {
        speech::propose(dlg, speech::P_NEWS, speech::Topic::News, "HAVE YOU HEARD? " + it->text);
        return;
      }
}

bool Game::storyChoose(const DlgOpt& o) {
  STORY_HOST_INSTALL();
  using namespace story;
  const int k = o.action - DLG_STORY;
  if (k < SA_START) return story.choose(*this, (uint32_t)o.arg, k - SA_OPT);
  // a story was taken up: the teller says its first lines, or points the way to whoever must say them
  auto begun = [&](const dsl::Script& s, uint32_t id, int ai) {
    if (ai >= 0) JobOps::taleStarted(*this, npcKey(actors[(size_t)ai]));   // (P1 fixer) their TALE lead: taken now
    Instance* in = story.find(id);
    if (in && ai >= 0 && story.talksTo(*this, *in, actors[(size_t)ai])) {
      dlg.opts.clear();
      story.showDialogue(*this, *in);
      dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
    } else if (in && in->stage >= 0 && in->stage < (int)s.stages.size() && s.stages[(size_t)in->stage].kind == dsl::StageKind::Talk) {
      // (fixer M4 r1) the story's first words belong to someone who is not here (the mother the notice names): the
      // notice (or the teller) only points the way; the speaker says their lines when the player finds them
      // (M7b) said in the teller's words: who to see and where (the journal keeps the errand)
      const std::string hand = story.handOffLine(*this, *in, s.stages[(size_t)in->stage].talk, dlg.text, ai >= 0);
      dlg.text = hand.empty() ? std::string("YOUR JOURNAL HOLDS THE REST.") : hand;
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
    } else if (in) {
      // (M7b) the goal's own line, or the way to it in the teller's words (a notice: the journal keeps the errand)
      std::string say = story.sayText(*this, *in);
      const std::string step = story.nextStepLine(*this, *in, dlg.text + " " + say, say.empty());
      if (say.empty()) say = step;
      else if (!step.empty() && speech::fitsPanel(say + " " + step)) say += " " + step;
      dlg.text = say.empty() ? std::string("YOU COMMIT THE NOTICE TO MEMORY. YOUR JOURNAL HOLDS THE REST.") : say;
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
  if (k == SA_READON) {
    // (P1) the rest of a ruin's words: the first lines of the story they began
    const Instance* in = story.find((uint32_t)o.arg);
    const std::string say = in ? story.sayText(*this, *in) : std::string();
    if (!say.empty()) dlg.text = say;
    dropOpt();
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
      // (P1) the story's first lines after the lore's when both fit the panel, else behind a "READ ON." option (the
      // ruin's own story is the point of reading: it is never lost for want of room)
      auto share = [&](uint32_t id) {
        const Instance* in = story.find(id);
        if (!in) return;
        const std::string say = story.sayText(*this, *in);
        if (say.empty()) return;
        if (dlg.text.size() + say.size() < 290) { dlg.text += " " + say; return; }
        dlg.opts.insert(dlg.opts.begin(), {"READ ON.", DLG_STORY + SA_READON, (int)id});
      };
      const story::Offer of = story::placeOffer(*this, (int)dsl::HookKind::Ruin, subSite);   // (P1: offers.cpp)
      const int st = of.lib;
      if (st >= 0) {
        const dsl::Script& s = dsl::library().scripts[(size_t)st];
        const uint64_t key = storyMarkKey(world.sites[(size_t)subSite].id ^ strHash64(s.id), MK_STORY_MARK);
        if (!marks.count(key)) {
          marks[key] = day;
          if (const uint32_t id = story.start(*this, s.id, -1, subSite)) share(id);
        }
      } else {
        // (M6b) a generated story (or campaign) the ruin's words begin (offers.cpp placeOffer; a fortnight between)
        const std::string sid = of.saga;
        if (!sid.empty()) {
          if (const uint32_t id = story.start(*this, sid, -1, subSite)) {
            saga::Hook hk;
            hk.kind = (int)dsl::HookKind::Ruin;
            hk.site = subSite;
            saga::noteTold(*this, sid, id, hk);
            share(id);
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
