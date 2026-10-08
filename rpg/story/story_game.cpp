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
#include <string>
#include "engine/audio.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/story/dsl.h"
#include "rpg/story/story_internal.h"

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
          const bool hidden = (hid != in.vars.end() && hid->second) || (dead != in.vars.end() && dead->second);
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
      if (R.kind != dsl::RoleKind::Person && R.kind != dsl::RoleKind::Foe) continue;
      const Binding* b = bindingOf(in, R.name);
      if (!b || !b->hasPos) continue;
      auto hid = in.vars.find("_hide_" + R.name);
      auto dead = in.vars.find("_dead_" + R.name);
      if ((hid != in.vars.end() && hid->second) || (dead != in.vars.end() && dead->second)) continue;
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
      sp.npc = R.kind == dsl::RoleKind::Person;
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
  if (st < 0) return;
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
  if (a.role == Role::Herald && a.slot == HERALD_SLOT) { heraldTalk(*this, a); return; }
  for (Instance& in : story.running_)
    if (!in.done && story.talksTo(*this, in, a)) { story.showDialogue(*this, in); return; }
  if (isStoryPerson(a)) {
    dlg.text = kIdle[hash32((uint32_t)a.slot ^ (uint32_t)day) % 4];
    return;
  }
  const bool busy = blocksGossip(dlg);
  // a story to tell
  const int idx = busy ? -1 : story.offerFor(*this, a);
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
  if (k < SA_BOARD) {
    const int idx = k - SA_START;
    const dsl::Library& L = dsl::library();
    if (idx < 0 || idx >= (int)L.scripts.size()) return false;
    const dsl::Script& s = L.scripts[(size_t)idx];
    const int ai = findActor(dlg.actor);
    const bool npcHook = s.hook == dsl::HookKind::Npc || s.hook == dsl::HookKind::Herald;
    std::string why;
    const uint32_t id = story.start(*this, s.id, npcHook && ai >= 0 ? actors[(size_t)ai].id : -1, o.arg, &why);
    for (size_t i = 0; i < dlg.opts.size(); i++) if (dlg.opts[i].action == o.action) { dlg.opts.erase(dlg.opts.begin() + (std::ptrdiff_t)i); break; }
    if (!id) {
      if (ai >= 0) marks[storyMarkKey(npcKey(actors[(size_t)ai]), MK_STORY_GIVER)] = day;   // (no second try from them)
      dlg.text = npcHook ? "...NO. NO, FORGET I SAID ANYTHING. IT'S NOTHING." : "THE NOTICE IS TOO RAIN-SOAKED TO READ.";
      return true;
    }
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
      }
    }
    return true;
  }
  // shrines, altars: the story notes it, the blessing still happens
  story.onUseProp(*this, (int)p, tx, ty);
  return false;
}
