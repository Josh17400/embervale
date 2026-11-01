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
#include "engine/sound_ids.h"
#include "rpg/sim/game.h"
#include "rpg/sim/jobs_ops.h"
#include "rpg/sim/talk.h"
#include "rpg/story/dsl.h"
#include "rpg/story/story_internal.h"
#include "rpg/story/story_detail.h"
#include "rpg/world/coords.h"
#include "rpg/world/source.h"
#include "rpg/sim/stream.h"
#include "rpg/sim/quest_ops.h"
#include "rpg/sim/story_ops.h"

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
        for (Quest& q : g.quests) if (q.id == qid && q.state == QState::Complete) { QuestOps::completeQuest(g, q); break; }       \
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

// ---------------------------------------------------------------- (P1b ENGINE lane) the shared mechanics in play
// Every quarter second engineSync reads the running instances (their goals and vars) into Engine::walkers_ / bouts_ /
// parts_ / calm_, sets the drove's beasts out beside their drover, turns `do hostile` people on the player and keeps
// _escort_far; engineTick (every frame) runs the bouts' moments (squaring up, the yield, the stand-down); the actors
// themselves move in folkStep / hostileStep (called from ai.cpp).
namespace {
const char* const kSquare[] = {"HAVE AT YOU!", "FISTS UP, THEN.", "TO THE FIRST FALL!", "COME ON, THEN!"};
}  // namespace

namespace storyg {

// every frame: the bouts' moments
void engineTick(Game& g, float dt) {
  using namespace story;
  Engine& E = g.story;
  // the footsteps (every map: a cave's walker follows too); a new map starts a new trail
  if (E.trailMap_ != g.mapKey()) { E.trail_.clear(); E.trailMap_ = g.mapKey(); }
  {
    const Vec2 at = g.pl().p;
    if (E.trail_.empty() || len2(at - Vec2(E.trail_.back().x, E.trail_.back().y)) >= (float)TILE * (float)TILE) E.trail_.push_back({at.x, at.y});
    if (E.trail_.size() > 160) E.trail_.erase(E.trail_.begin(), E.trail_.begin() + 32);
  }
  for (Engine::Bout& b : E.bouts_) {
    if (b.actor < 0) continue;
    const int ai = findA(g, b.actor);
    if (ai < 0) continue;
    Actor& a = g.actors[(size_t)ai];
    Instance* in = E.find(b.inst);
    if (!in || a.st == AState::Dead) continue;
    const float d2 = len2(a.p - g.pl().p);
    const uint32_t h = hash32((uint32_t)a.id * 0x27D4EB2Du ^ (uint32_t)b.inst);
    switch (b.phase) {
      case 0:   // waiting: the bout begins when the player steps up (or at once, beside them; a foe is already on guard)
        if (g.mode == Mode::Play && (d2 < (5.0f * TILE) * (5.0f * TILE) || (a.hostile && !a.npc))) {
          b.phase = 1; b.t = 0;
          armForBout(g, a);
          b.hp0 = a.maxHp;
          spoken(g, a, kSquare[h % 4u], rgba(255, 220, 140));
          E.parts_[a.id] = Engine::P_BOUT;
        }
        break;
      case 1:
        b.t += dt;
        a.target = g.pl().id; a.aggro = true;
        if (d2 > (24.0f * TILE) * (24.0f * TILE)) {   // the player walked away from it: it waits for them again
          standDown(a); a.hp = a.maxHp; b.phase = 0; b.t = 0;
          E.parts_[a.id] = Engine::P_BOUT_WAIT;
        }
        break;
      case 2:   // kneeling: the yield, then up again (the goal is won as they rise)
        b.t += dt;
        a.posture = art::Posture::Kneel; a.st = AState::Idle; a.face = faceOf(g.pl().p - a.p);
        if (b.t > 2.6f) {
          a.posture = art::Posture::None;
          a.hp = std::max(a.hp, a.maxHp * 0.4f);
          in->vars["_won_bout"] = 1;
          b.phase = 4;
          E.parts_[a.id] = Engine::P_YIELD;
        }
        break;
      case 3:   // the player is down on one knee; the victor stands back, then the bout is lost
        b.t += dt;
        a.st = AState::Idle; a.face = faceOf(g.pl().p - a.p);
        // the player reels from the last blow a moment (the hurt pose), then the bout is lost
        if (b.t < 0.9f && g.pl().st != AState::Dead && g.pl().st != AState::Roll) { g.pl().st = AState::Hurt; g.pl().stT = 0; }
        if (b.t > 1.8f) { in->vars["_lost_bout"] = 1; b.phase = 4; }
        break;
      default: break;
    }
  }
}
}  // namespace storyg

namespace {

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

void StoryOps::step(Game& g, float dt) {
  STORY_HOST_INSTALL();
  if (g.mode == Mode::Title || g.mode == Mode::Creator) return;
  g.story.stepT_ += dt;
  if (g.story.stepT_ >= 0.25f) {
    g.story.stepT_ = 0;
    engineSync(g);     // (P1b: _escort_far as things stand now, before the goals are judged)
    g.story.step(g);
    syncPersons(g);
    engineSync(g);     // (P1b ENGINE lane: walkers, bouts, hostile people, calm sites, the drove, _escort_far)
    standInsLive(g);   // (P1: a round by day, a doorstep by night)
    if (g.inside) crownTheKing(g);
    story::heraldsProclaim(g);
  }
  engineTick(g, dt);   // (P1b ENGINE lane: the bouts' moments)
  g.story.clock_ += dt;
  // (P1b, lead) an ending reached off a dialogue (a goal done, a clock run out, an escort lost) is told now as a closing
  // scene: its say under the giver's name (or the tale's title when it is told as a story), one FAREWELL
  // (P1b ENGINE lane) it waits out a fight (no roused foe near the player), a journey and a fade, and a breath after the
  // moment that ended it (the last blow, the arrival) so that moment is seen first
  bool epiCalm = g.mode == Mode::Play && !g.travelling() && g.sleepFade <= 0.0f;
  if (epiCalm && !g.story.epilogues_.empty())
    for (size_t i = 1; i < g.actors.size() && epiCalm; i++) {
      const Actor& e = g.actors[i];
      if (e.hostile && e.aggro && e.st != AState::Dead && len2(e.p - g.pl().p) < (14.0f * TILE) * (14.0f * TILE)) epiCalm = false;
    }
  if (g.story.epilogues_.empty() || !epiCalm) g.story.engineT_ = 0;
  else g.story.engineT_ += dt;
  if (epiCalm && g.story.engineT_ > 1.1f && !g.story.epilogues_.empty()) {
    g.story.engineT_ = 0;
    const story::Engine::Epilogue ep = g.story.epilogues_.front();
    g.story.epilogues_.erase(g.story.epilogues_.begin());
    g.dlg = Dialogue();
    g.dlg.actor = -1;
    g.dlg.speaker = ep.speaker;
    g.dlg.role = Role::Villager;
    g.dlg.text = ep.text;
    g.dlg.opts.push_back({"FAREWELL.", 0, 0});
    g.mode = Mode::Dialogue;
  }
  if (g.mode == Mode::Play && !g.travelling()) {
    using namespace story;
    // (fixer M6b r1) the composer's needs (caves, camps, ruins and fallen lords up to ~640 tiles out) and the caster read
    // the region plans 3 regions round a hook; building the ones not yet planned in the talk frame cost 60-80 ms on a
    // desktop. The prefetcher builds them (World::storyRing: off the main thread natively, in the pump's budget on the
    // web); the tale scan below waits until they are ready. Without a streamer (tests) one is planned every 0.08 s here.
    g.story.warmT_ += dt;
    bool warmDone = false;
    if (g.story.warmT_ >= 0.08f) {
      g.story.warmT_ = 0;
      warmDone = true;
      if (g.story.warmSeed_ != g.seed || g.story.warmed_.size() > 4096) { g.story.warmed_.clear(); g.story.warmSeed_ = g.seed; }
      int32_t px, py;
      story::playerGlobal(g, px, py);
      const int32_t rx0 = ew::regionOf(px), ry0 = ew::regionOf(py);
      const int32_t bx0 = ew::regionOf(g.world.ox - ew::REGION), by0 = ew::regionOf(g.world.oy - ew::REGION);
      const int32_t bx1 = ew::regionOf(g.world.ox + World::WIN + ew::REGION - 1), by1 = ew::regionOf(g.world.oy + World::WIN + ew::REGION - 1);
      const int R = std::max(1, std::min(3, g.world.storyRing));
      for (int32_t ry = ry0 - R; ry <= ry0 + R && warmDone; ry++)
        for (int32_t rx = rx0 - R; rx <= rx0 + R && warmDone; rx++) {
          if (rx >= bx0 && rx <= bx1 && ry >= by0 && ry <= by1) continue;   // (loaded with the window)
          const uint64_t k = ((uint64_t)(uint32_t)rx << 32) | (uint32_t)ry;
          if (g.story.warmed_.count(k)) continue;
          if (g.world.streamer && g.world.storyRing > 0) {
            if (g.world.streamer->hasRegion(rx, ry)) g.story.warmed_.insert(k);
            else warmDone = false;
          } else {
            g.story.warmed_.insert(k);
            (void)g.world.regionPlan(rx, ry);
            warmDone = false;
          }
        }
    }
    // (fixer M6b r1) who near the player has a tale to tell: one person per 0.3 s (once the regions are warm), so the
    // talk frame finds the offer memoised, and the people with a tale carry a small marker (render_markers.cpp)
    g.story.scanT_ += dt;
    // (fixer M6b r2) the composer's world facts at the settlement the player is in (caves, camps, ruins, the coast, the
    // census) one per warm tick once the regions are warm, so the first offer composed there costs ~3-7 ms (desktop),
    // not 15-27 ms in the arrival frame. The scan waits until they are known. (The composer's tables: Engine::reset.)
    saga::warmComposer();
    bool hookWarm = true;
    if (warmDone && g.curSite >= 0 && g.curSite < (int)g.world.sites.size() && g.world.sites[(size_t)g.curSite].settlement())
      hookWarm = saga::warmHook(g, g.curSite);
    if (warmDone && hookWarm && g.story.scanT_ >= 0.3f) {
      g.story.scanT_ = 0;
      int best = -1;
      float bestAt = 1e30f;
      for (size_t i = 1; i < g.actors.size(); i++) {
        const Actor& a = g.actors[i];
        if (!a.npc || !a.human || a.st == AState::Dead || story::isStoryPerson(a)) continue;
        if (len2(a.p - g.pl().p) > (18.0f * TILE) * (18.0f * TILE)) continue;
        auto it = g.story.tales_.find(a.id);
        const float at = it == g.story.tales_.end() || it->second.key != g.npcKey(a) ? -1.0f : it->second.at;
        if (at >= 0 && g.story.clock_ - at < 12.0f) continue;
        if (at < bestAt) { bestAt = at; best = (int)i; }
      }
      if (best >= 0) {
        Actor& a = g.actors[(size_t)best];
        story::Engine::TaleScan ts;
        ts.at = g.story.clock_;
        ts.key = g.npcKey(a);
        bool waiting = false;   // (someone a running story waits on has the quest's own marker)
        for (const story::Instance& in : g.story.running_) if (!in.done && g.story.talksTo(g, in, a)) waiting = true;
        story::Offer of;
        if (!waiting) of = story::tellerOffer(g, a);   // (P1: the one picker, offers.cpp)
        ts.tale = of.any();
        if (g.story.tales_.size() > 256) g.story.tales_.clear();
        g.story.tales_[a.id] = ts;
        // (P1 Fresh Roads) stumbled on: passing close by a teller, the player overhears what troubles them, and the journal
        // keeps it as a lead (a TALE) to the person; heard once in a while per teller, never from across the square
        if (of.any() && !of.campaign() && len2(a.p - g.pl().p) < (6.0f * TILE) * (6.0f * TILE)) {
          const uint64_t hk = story::storyMarkKey(ts.key, story::MK_TALE_HEARD);
          auto hm = g.marks.find(hk);
          const story::dsl::Script* sc = story::scriptById(of.id());
          if (sc && !sc->hint.empty() && (hm == g.marks.end() || g.day - hm->second >= 5)) {
            g.marks[hk] = g.day;
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
            const int hs = story::homeSiteOf(g, a);
            const std::string town = hs >= 0 && hs < (int)g.world.sites.size() ? g.world.sites[(size_t)hs].name : std::string();
            jobs::Lead L;
            L.from = jobs::LeadFrom::Teller;
            L.qtype = (uint8_t)QType::COUNT;
            L.site = hs >= 0 && hs < (int)g.world.sites.size() ? g.world.sites[(size_t)hs].id : 0;
            L.who = ts.key;
            int32_t gx = g.world.ox + (int32_t)std::floor(a.p.x / TILE), gy = g.world.oy + (int32_t)std::floor(a.p.y / TILE);
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
            g.addLead(L, false);
          }
        }
      }
    }
  }
  g.story.placeT_ += dt;
  if (g.story.placeT_ >= 0.7f) {
    g.story.placeT_ = 0;
    if (!g.inside && !g.travelling()) {
      story::placeBoards(g);
      story::spawnHeralds(g);
      story::placeRuinStatues(g);
      eventStories(g);
      placeWantedProps(g);   // (P1b ENGINE lane: a prop a goal wants, set down as the player comes near its place)
    }
  }
}

void StoryOps::talk(Game& g, Actor& a) {
  STORY_HOST_INSTALL();
  using namespace story;
  g.story.sagaOffers_.clear();   // (M6b: the offers of a dialogue live as long as it)
  g.story.said_.clear();         // (M7b: and what was said in it)
  g.story.tales_.erase(a.id);    // (fixer M6b r1: looked at again by the next scan)
  // (fixer M6b r3) a running story bound to this actor speaks first, heralds included: a herald-hooked saga's
  // `talk giver` stages are only reachable here, so checking the herald first left those stories stuck for good
  for (Instance& in : g.story.running_)
    if (!in.done && g.story.talksTo(g, in, a)) { g.story.showDialogue(g, in); return; }
  if (a.role == Role::Herald && a.slot == HERALD_SLOT) { heraldTalk(g, a); return; }
  if (isStoryPerson(a)) {
    // (P1b ENGINE lane) a walker on the road with the player has their own words
    static const char* const kWalk[] = {"LEAD ON. I'M RIGHT BEHIND YOU.", "KEEP GOING. I CAN KEEP UP.", "NOT FAR NOW, I HOPE.",
                                        "I'LL STAY CLOSE. LEAD THE WAY."};
    if (g.story.actorPart(a.id) == story::Engine::P_FOLLOW) {
      speech::propose(g.dlg, speech::P_STORY, speech::Topic::Story, kWalk[hash32((uint32_t)a.slot ^ (uint32_t)g.day * 31u) % 4]);
      return;
    }
    speech::propose(g.dlg, speech::P_STORY, speech::Topic::Story, kIdle[hash32((uint32_t)a.slot ^ (uint32_t)g.day) % 4]);
    return;
  }
  const bool busy = blocksGossip(g.dlg);
  // a story to tell
  // (P1) the one picker (offers.cpp tellerOffer): a library tale or a generated story (saga.h offerFor)
  const Offer of = busy ? Offer() : tellerOffer(g, a);
  const int idx = of.lib;
  if (!of.saga.empty()) {
    const std::string sid = of.saga;
    const dsl::Script* sc = scriptById(sid);
    if (sc) {
      g.story.sagaOffers_.push_back(sid);
      tellerLine(g, *sc, a);
      g.dlg.opts.insert(g.dlg.opts.begin(), {sc->pitch, DLG_STORY + SA_SAGA + (int)g.story.sagaOffers_.size() - 1, homeSiteOf(g, a)});
      saga::Hook hk;
      hk.kind = (int)dsl::HookKind::Npc;
      hk.actor = a.id;
      hk.site = homeSiteOf(g, a);
      saga::noteOffered(g, hk, sid);
      return;
    }
  }
  if (idx >= 0) {
    const dsl::Script& s = dsl::library().scripts[(size_t)idx];
    tellerLine(g, s, a);
    g.dlg.opts.insert(g.dlg.opts.begin(), {s.pitch, DLG_STORY + SA_START + idx, homeSiteOf(g, a)});
    return;
  }
  if (busy) return;
  // someone who remembers what the player did
  const int hs = homeSiteOf(g, a);
  const uint64_t key = g.npcKey(a);
  const std::string* mem = g.story.memoryOf(key);
  if (!mem && hs >= 0) mem = g.story.memoryOf(ew::mix64(g.world.sites[(size_t)hs].id ^ (0x7EADull + (uint64_t)a.role)));
  if (mem && ((hash32((uint32_t)key ^ (uint32_t)g.day) & 1) == 0 || g.story.memories().back().line == *mem)) {
    speech::propose(g.dlg, speech::P_BARK, speech::Topic::Story, *mem);
    return;
  }
  // the realm's news (VISION_PLAN 4.6): innkeepers 70 %, travellers 50 %, guards 60 % (war news only), the rest 25 %
  int32_t gx, gy;
  playerGlobal(g, gx, gy);
  const uint64_t voice = hs >= 0 ? g.world.sites[(size_t)hs].culture : 0;
  const uint32_t roll = hash32((uint32_t)key ^ (uint32_t)(key >> 29) ^ (uint32_t)g.day * 7919u) % 100;
  if ((int)roll < speakerChance((int)a.role) && a.role != Role::Child) {
    if (const realm::WorldEvent* e = pickRumour(g, gx, gy, a.role == Role::Guard, key)) {
      // (M7b) only told when it is what this speaker leads with (hearing it marks it heard)
      if ((!e->heard || roll % 3 == 0) && g.dlg.pri <= speech::P_NEWS) { speech::propose(g.dlg, speech::P_NEWS, speech::Topic::Rumour, hearEvent(g, *e, voice)); return; }
    }
  }
  // the road to war, foreshadowed (15.6.3): prices, troops, refugees
  if (roll % 4 == 1 && hs >= 0) {
    const ew::Gid k = g.world.sites[(size_t)hs].kingdom >= 0 ? g.world.kingdoms[(size_t)g.world.sites[(size_t)hs].kingdom].id : 0;
    const std::string f = foreshadowLine(g, k, voice, key ^ (uint64_t)g.day);
    if (!f.empty() && speech::propose(g.dlg, speech::P_NEWS, speech::Topic::News, f)) return;
  }
  // the player's deeds, as the town tells them
  if (roll % 5 == 2 && hs >= 0)
    for (auto it = g.story.facts().rbegin(); it != g.story.facts().rend(); ++it)
      if (it->site == g.world.sites[(size_t)hs].id && g.day - it->day < 40) {
        speech::propose(g.dlg, speech::P_NEWS, speech::Topic::News, "HAVE YOU HEARD? " + it->text);
        return;
      }
}

bool StoryOps::choose(Game& g, const DlgOpt& o) {
  STORY_HOST_INSTALL();
  using namespace story;
  const int k = o.action - DLG_STORY;
  if (k < SA_START) return g.story.choose(g, (uint32_t)o.arg, k - SA_OPT);
  // a story was taken up: the teller says its first lines, or points the way to whoever must say them
  auto begun = [&](const dsl::Script& s, uint32_t id, int ai) {
    if (ai >= 0) JobOps::taleStarted(g, g.npcKey(g.actors[(size_t)ai]));   // (P1 fixer) their TALE lead: taken now
    Instance* in = g.story.find(id);
    if (in && ai >= 0 && g.story.talksTo(g, *in, g.actors[(size_t)ai])) {
      g.dlg.opts.clear();
      g.story.showDialogue(g, *in);
      g.dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
    } else if (in && in->stage >= 0 && in->stage < (int)s.stages.size() && s.stages[(size_t)in->stage].kind == dsl::StageKind::Talk) {
      // (fixer M4 r1) the story's first words belong to someone who is not here (the mother the notice names): the
      // notice (or the teller) only points the way; the speaker says their lines when the player finds them
      // (M7b) said in the teller's words: who to see and where (the journal keeps the errand)
      const std::string hand = g.story.handOffLine(g, *in, s.stages[(size_t)in->stage].talk, g.dlg.text, ai >= 0);
      g.dlg.text = hand.empty() ? std::string("YOUR JOURNAL HOLDS THE REST.") : hand;
      g.dlg.opts = {{"FAREWELL.", A_BYE, 0}};
    } else if (in) {
      // (M7b) the goal's own line, or the way to it in the teller's words (a notice: the journal keeps the errand)
      std::string say = g.story.sayText(g, *in);
      const std::string step = g.story.nextStepLine(g, *in, g.dlg.text + " " + say, say.empty());
      if (say.empty()) say = step;
      else if (!step.empty() && speech::fitsPanel(say + " " + step)) say += " " + step;
      g.dlg.text = say.empty() ? std::string("YOU COMMIT THE NOTICE TO MEMORY. YOUR JOURNAL HOLDS THE REST.") : say;
      g.dlg.opts = {{"FAREWELL.", A_BYE, 0}};
    }
  };
  auto dropOpt = [&]() {
    for (size_t i = 0; i < g.dlg.opts.size(); i++) if (g.dlg.opts[i].action == o.action) { g.dlg.opts.erase(g.dlg.opts.begin() + (std::ptrdiff_t)i); break; }
  };
  if (k < SA_BOARD) {
    const int idx = k - SA_START;
    const dsl::Library& L = dsl::library();
    if (idx < 0 || idx >= (int)L.scripts.size()) return false;
    const dsl::Script& s = L.scripts[(size_t)idx];
    const int ai = g.findActor(g.dlg.actor);
    const bool npcHook = s.hook == dsl::HookKind::Npc || s.hook == dsl::HookKind::Herald;
    std::string why;
    const uint32_t id = g.story.start(g, s.id, npcHook && ai >= 0 ? g.actors[(size_t)ai].id : -1, o.arg, &why);
    dropOpt();
    if (!id) {
      if (ai >= 0) g.marks[storyMarkKey(g.npcKey(g.actors[(size_t)ai]), MK_STORY_GIVER)] = g.day;   // (no second try from them)
      g.dlg.text = npcHook ? "...NO. NO, FORGET I SAID ANYTHING. IT'S NOTHING." : "THE NOTICE IS TOO RAIN-SOAKED TO READ.";
      return true;
    }
    begun(s, id, ai);
    return true;
  }
  if (k >= SA_SAGA && k < SA_SAGA + 100) {
    // (M6b) a generated story offered in this dialogue (Engine::sagaOffers_; arg: the hook place)
    const int i = k - SA_SAGA;
    if (i < 0 || i >= (int)g.story.sagaOffers_.size()) return false;
    const std::string sid = g.story.sagaOffers_[(size_t)i];
    const dsl::Script* sp = scriptById(sid);
    dropOpt();
    if (!sp) { g.dlg.text = "...NO. IT'S GONE FROM ME. FORGIVE ME."; return true; }
    const dsl::Script& s = *sp;
    const int ai = g.findActor(g.dlg.actor);
    const bool npcHook = s.hook == dsl::HookKind::Npc || s.hook == dsl::HookKind::Herald;
    const int hookActor = npcHook && ai >= 0 ? g.actors[(size_t)ai].id : -1;
    std::string why;
    const uint32_t id = g.story.start(g, sid, hookActor, o.arg, &why);
    if (!id) {
      if (ai >= 0) g.marks[storyMarkKey(g.npcKey(g.actors[(size_t)ai]), MK_STORY_GIVER)] = g.day;
      g.dlg.text = npcHook ? "...NO. NO, FORGET I SAID ANYTHING. IT'S NOTHING." : "THE NOTICE IS TOO RAIN-SOAKED TO READ.";
      return true;
    }
    saga::Hook hk;
    hk.kind = (int)s.hook;
    hk.actor = hookActor;
    hk.site = o.arg;
    saga::noteTold(g, sid, id, hk);
    // (the script pointer may not outlive the cache: look it up again)
    if (const dsl::Script* s2 = scriptById(sid)) begun(*s2, id, ai);
    return true;
  }
  if (k == SA_READON) {
    // (P1) the rest of a ruin's words: the first lines of the story they began
    const Instance* in = g.story.find((uint32_t)o.arg);
    const std::string say = in ? g.story.sayText(g, *in) : std::string();
    if (!say.empty()) g.dlg.text = say;
    dropOpt();
    return true;
  }
  return boardChoose(g, k, o.arg);
}

// (P1b, lead) a radiant job begun from "ANY WORK GOING?": its composed script starts on the giver, whose opening talk (the
// pitch with ACCEPT / DECLINE) takes over the open dialogue; a decline closes it quietly (the "_quiet" opening)
bool StoryOps::beginJob(Game& g, Actor& a, const std::string& specId) {
  STORY_HOST_INSTALL();
  using namespace story;
  const uint32_t id = g.story.start(g, specId, a.id, homeSiteOf(g, a));
  if (!id) return false;
  Instance* in = g.story.find(id);
  if (!in || !g.story.talksTo(g, *in, a)) {   // (a template whose opening is not the giver's talk: refused, quietly)
    if (in) { in->vars["_quiet"] = 1; g.story.finish(g, *in, false); }
    return false;
  }
  g.dlg.opts.clear();
  g.story.showDialogue(g, *in);
  return true;
}

void StoryOps::kill(Game& g, const Actor& victim, bool byPlayer) {
  STORY_HOST_INSTALL();
  g.story.onKill(g, victim, byPlayer);
}

void StoryOps::entered(Game& g, int site, int bldg) {
  STORY_HOST_INSTALL();
  if (site >= 0 && site < (int)g.world.sites.size() && g.world.sites[(size_t)site].type == SiteType::Ruin) {
    g.story.ruinProps_ = story::placeRuinLore(g, site);
    g.story.ruinPropsSite_ = g.world.sites[(size_t)site].id;
  } else {
    g.story.ruinProps_.clear();
    g.story.ruinPropsSite_ = 0;
  }
  if (bldg >= 0) crownTheKing(g);
  g.story.onEntered(g, site, bldg);
  if (site >= 0) placeInsiders(g, site);   // (P1b ENGINE lane: the people waiting inside)
  placeWantedProps(g);                     // (P1b: a crate to search where the goal sends the player)
  syncPersons(g);                          // (P1b: the walkers come in with the player at once)
  engineSync(g);
}

bool StoryOps::useProp(Game& g, art::Prop p, int tx, int ty) {
  STORY_HOST_INSTALL();
  using namespace story;
  // (P1b ENGINE lane) a prop a running story asked for (goal use beacon / crate / barrel / chest / well / brazier / sacks)
  if (useWantedProp(g, (int)p, tx, ty)) return true;
  if (p == art::Prop::NoticeBoard) return useBoard(g, tx, ty);
  if (p == art::Prop::ToppledStatue && !g.inside) { const bool r = readStatue(g, tx, ty); g.story.onUseProp(g, (int)p, tx, ty); return r; }
  if (art::isLoreProp(p)) {
    if (!readLore(g, tx, ty)) return false;
    g.story.onUseProp(g, (int)p, tx, ty);
    // a ruin's words may begin a story (a dead adventurer's journal, an oath carved for the living)
    if (g.inside && g.subSite >= 0) {
      // (P1) the story's first lines after the lore's when both fit the panel, else behind a "READ ON." option (the
      // ruin's own story is the point of reading: it is never lost for want of room)
      auto share = [&](uint32_t id) {
        const Instance* in = g.story.find(id);
        if (!in) return;
        const std::string say = g.story.sayText(g, *in);
        if (say.empty()) return;
        if (g.dlg.text.size() + say.size() < 290) { g.dlg.text += " " + say; return; }
        g.dlg.opts.insert(g.dlg.opts.begin(), {"READ ON.", DLG_STORY + SA_READON, (int)id});
      };
      const story::Offer of = story::placeOffer(g, (int)dsl::HookKind::Ruin, g.subSite);   // (P1: offers.cpp)
      const int st = of.lib;
      if (st >= 0) {
        const dsl::Script& s = dsl::library().scripts[(size_t)st];
        const uint64_t key = storyMarkKey(g.world.sites[(size_t)g.subSite].id ^ strHash64(s.id), MK_STORY_MARK);
        if (!g.marks.count(key)) {
          g.marks[key] = g.day;
          if (const uint32_t id = g.story.start(g, s.id, -1, g.subSite)) share(id);
        }
      } else {
        // (M6b) a generated story (or campaign) the ruin's words begin (offers.cpp placeOffer; a fortnight between)
        const std::string sid = of.saga;
        if (!sid.empty()) {
          if (const uint32_t id = g.story.start(g, sid, -1, g.subSite)) {
            saga::Hook hk;
            hk.kind = (int)dsl::HookKind::Ruin;
            hk.site = g.subSite;
            saga::noteTold(g, sid, id, hk);
            share(id);
          }
        }
      }
    }
    return true;
  }
  // shrines, altars: the story notes it, the blessing still happens
  g.story.onUseProp(g, (int)p, tx, ty);
  return false;
}

// ---------------------------------------------------------------- (P1b ENGINE lane) the hooks (story.h)
namespace story {

float burdenMul(const Game& g) { return g.story.burdened() ? 0.7f : 1.0f; }

}  // namespace story
