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

std::vector<Vec2> trailOf(const Game& g);

// (P1b ENGINE lane) is role R of this instance walking with the player now (an escort goal on them, or `do follow`)?
bool following(const story::Instance& in, const story::dsl::Script& s, const story::dsl::RoleDef& R) {
  using namespace story;
  if (R.kind != dsl::RoleKind::Person) return false;   // (a census resident has a body of their own in the town)
  auto var = [&](const std::string& n) { auto it = in.vars.find(n); return it == in.vars.end() ? 0 : it->second; };
  if (var("_dead_" + R.name) || var("_hide_" + R.name)) return false;
  const dsl::StageDef& G = s.stages[(size_t)in.stage];
  if (G.kind == dsl::StageKind::Goal && G.goal.type == dsl::ObjType::Escort && G.goal.role == R.name) return true;
  return var("_follow_" + R.name) != 0;
}

// (P1b ENGINE lane) dress a spawned story person for their part: young (a child's body), the name, the sex's look
void dressPerson(Game& g, const story::Instance& in, Actor& a, const story::Binding& b, const story::dsl::RoleDef& R, bool npc) {
  // (P1b fixer) one look per person, whatever body the story sets down (the walker beside the player, the person at
  // the objective site, the one waiting inside a ruin): drawn from the binding and dressed in the culture of the
  // story's own town (its giver's site), not from the spawn's site and building
  a.name = b.name;
  if (npc) {
    const int site0 = a.site, bldg0 = a.bldg, slot0 = a.slot;
    a.site = in.cast.empty() ? -1 : g.world.siteHandle(in.cast[0].site);
    a.bldg = -1;
    a.slot = (int)(b.id & 0xFFFF);
    Rng rr(hash32((uint32_t)(b.id ^ (b.id >> 32)) * 2654435761u ^ 0x9E50u));
    a.look = art::HumanLook();   // (makeLook fills over the look it is given: start from a blank one every time)
    HomeOps::makeLook(g, a, Role::Villager, rr);
    a.site = site0; a.bldg = bldg0; a.slot = slot0;
  }
  a.name = b.name;
  a.fromMap = false;
  a.site = -1;   // (not a townsperson: never streamed away, offers no radiant work)
  if (b.female) { a.look.beard = false; if (a.look.hair == art::Hair::Short || a.look.hair == art::Hair::Bald || a.look.hair == art::Hair::Mohawk) a.look.hair = art::Hair::Braids; if (npc && !R.young) a.look.outfit = art::Outfit::Dress; }
  else { if (a.look.hair == art::Hair::Long || a.look.hair == art::Hair::Braids || a.look.hair == art::Hair::Ponytail) a.look.hair = art::Hair::Short; if (a.look.outfit == art::Outfit::Dress) a.look.outfit = art::Outfit::Tunic; }
  if (R.young && npc) {   // (P1b) a child or a youth: the census child's body (render.cpp childBody, keyed on Role::Child)
    a.role = Role::Child;
    a.radius = 3.5f;
    a.look.beard = false;
    a.look.helmet = false;
    a.look.weapon = 0;
  }
}

// a free tile a few tiles behind the player (the trail they walked, else behind their back): where a walker is set down
// after a door, a fast travel or a region shift (window tiles)
bool besidePlayer(const Game& g, float back, int& ox, int& oy, uint32_t salt) {
  const Actor& p = g.pl();
  const Map& m = g.map();
  // a tile the body can walk straight to the player from (no building, fence or bank between them: one set down on
  // the far side of a house would walk round it, away from the player)
  auto clearTo = [&](int tx, int ty) {
    if (!m.in(tx, ty) || m.blocked(tx, ty)) return false;
    if (!g.inside && m.bldgAt[(size_t)ty * m.w + tx] >= 0) return false;
    const Vec2 c(tx * TILE + 8.0f, ty * TILE + 10.0f), d = p.p - c;
    const float l = len(d);
    for (float t = 0.0f; t < l; t += 4.0f) if (!ClimbOps::bodyFree(g, c + d * (t / std::max(l, 0.01f)), 4.5f)) return false;
    return true;
  };
  const std::vector<Vec2> tr = trailOf(g);
  if (!tr.empty()) {   // the trail the player walked: back along it
    float acc = len(p.p - tr.back());
    int j = (int)tr.size() - 1;
    while (j > 0 && acc < back) { acc += len(tr[(size_t)j] - tr[(size_t)j - 1]); j--; }
    const int tx = (int)std::floor(tr[(size_t)j].x / TILE), ty = (int)std::floor(tr[(size_t)j].y / TILE);
    if (len2(tr[(size_t)j] - p.p) < (back + 2.0f * TILE) * (back + 2.0f * TILE) && (back > 6.0f * TILE || clearTo(tx, ty))) { ox = tx; oy = ty; return true; }
  }
  // else round the player, from behind their back first, then their sides, then ahead
  const float base = std::atan2(-p.aim.y, -p.aim.x);
  static const float kOff[8] = {0.0f, 0.6f, -0.6f, 1.2f, -1.2f, 1.9f, -1.9f, 3.14159f};
  const int px = (int)std::floor(p.p.x / TILE), py = (int)std::floor((p.p.y - 2) / TILE);
  for (int r = std::max(2, (int)(back / TILE)); r <= 5; r++)
    for (int k = 0; k < 8; k++) {
      const float an = base + kOff[(k + (int)(salt % 2u)) % 8];
      const int tx = px + (int)std::lround(std::cos(an) * r), ty = py + (int)std::lround(std::sin(an) * r);
      if (clearTo(tx, ty)) { ox = tx; oy = ty; return true; }
    }
  // (hemmed in: a doorway between walls) any open tile round them, then the nearest free spot at all
  for (int r = 1; r <= 4; r++)
    for (int k = 0; k < 8; k++) {
      const float an = base + kOff[k];
      const int tx = px + (int)std::lround(std::cos(an) * r), ty = py + (int)std::lround(std::sin(an) * r);
      if (m.in(tx, ty) && !m.blocked(tx, ty) && (g.inside || m.bldgAt[(size_t)ty * m.w + tx] < 0)) { ox = tx; oy = ty; return true; }
    }
  const int tx = (int)std::floor((p.p.x - p.aim.x * back) / TILE), ty = (int)std::floor((p.p.y - p.aim.y * back) / TILE);
  if (!g.inside && story::freeTileNear(g, tx, ty, ox, oy, salt)) return true;
  const Vec2 f = ClimbOps::freeSpot(g, tx, ty);
  ox = (int)std::floor(f.x / TILE); oy = (int)std::floor(f.y / TILE);
  return true;
}

// persons and foes of the running stories, spawned where they are when the player comes near (outdoors), put away when
// the player leaves or the story no longer needs them
// (P1b ENGINE lane) a person walking with the player is kept and set down beside them (any map); a person `in` a site
// waits inside it (placeInsiders), never outdoors
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
          // (P1b) a walker with the player is never put away (a far one is set down beside the player below)
          if (!hidden && following(in, *s, s->roles[ri])) keep = true;
        }
      }
      if (a.st == AState::Dead) keep = true;   // (the corpse clean-up takes it)
    }
    if (!keep) { g.actors.erase(g.actors.begin() + (std::ptrdiff_t)k); continue; }
    k++;
  }
  if (!host().spawnHuman) return;
  // (P1b ENGINE lane) walkers: inside a building they wait at its door (set down beside the player as they come out);
  // inside a cave or a ruin they come along (a child led out of the dark)
  const bool walkersHere = !(g.inside && g.subBldg >= 0);
  // (P1b ENGINE lane) walkers: set down beside the player when they have no body here (a door, a fast travel, the
  // dungeon's mouth) or theirs was left far behind (a region shift, a long fall behind): out of the view's middle, on the
  // trail the player walked
  for (const Instance& in : g.story.running()) {
    if (in.done) continue;
    const dsl::Script* s = scriptOf(in);
    if (!s) continue;
    for (size_t ri = 0; ri < s->roles.size(); ri++) {
      const dsl::RoleDef& R = s->roles[ri];
      if (!walkersHere || !following(in, *s, R)) continue;
      const Binding* b = bindingOf(in, R.name);
      if (!b) continue;
      const int slot = personSlot(in.id, (int)ri);
      int have = -1;
      for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].slot == slot && isStoryPerson(g.actors[k])) have = (int)k;
      if (have >= 0 && g.actors[(size_t)have].st == AState::Dead) continue;
      if (have >= 0 && len2(g.actors[(size_t)have].p - g.pl().p) < (40.0f * TILE) * (40.0f * TILE)) continue;
      int tx = 0, ty = 0;
      if (!besidePlayer(g, 2.5f * TILE, tx, ty, (uint32_t)(b->id ^ (b->id >> 32)) ^ (uint32_t)g.day)) continue;
      if (have >= 0) {   // far behind: walked up beside the player (out of sight: the view was elsewhere)
        Actor& a = g.actors[(size_t)have];
        a.p = Vec2(tx * TILE + 8.0f, ty * TILE + 10.0f);
        a.home = a.goal = a.p;
        a.navNext = -1;
        for (Engine::Walker& w : g.story.walkers_) if (w.actor == a.id) { w.cartSet = false; w.spd = 0; w.stuckT = 0; }
        continue;
      }
      Spawn sp;
      sp.npc = true;
      sp.role = Role::Villager;
      sp.site = -1;
      sp.slot = slot;
      sp.x = tx; sp.y = ty;
      const int id = host().spawnHuman(g, sp, tx * TILE + 8.0f, ty * TILE + 10.0f);
      const int ai = host().findActor(g, id);
      if (ai < 0) continue;
      Actor& a = g.actors[(size_t)ai];
      a.slot = slot;
      dressPerson(g, in, a, *b, R, true);
      a.face = g.pl().face;
    }
  }
  if (g.inside) return;
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
      if (following(in, *s, R)) continue;   // (P1b: set down beside the player above)
      // foes in caves and ruins wait inside (Engine::onEntered)
      const int sh = g.world.siteHandle(b->site);
      if (R.kind == dsl::RoleKind::Foe && sh >= 0 && (g.world.sites[(size_t)sh].type == SiteType::Cave || g.world.sites[(size_t)sh].type == SiteType::Ruin)) continue;
      // (P1b) a person `in` a cave or a ruin waits inside it (placeInsiders); `in` a camp: by one of its tents
      bool camp = false;
      if (R.inside && sh >= 0) {
        const SiteType st = g.world.sites[(size_t)sh].type;
        if (st == SiteType::Cave || st == SiteType::Ruin) continue;
        camp = st == SiteType::BanditCamp;
      }
      const int64_t dx = px - b->gx, dy = py - b->gy;
      if (dx * dx + dy * dy > 36 * 36) continue;
      const int slot = personSlot(in.id, (int)ri);
      bool there = false;
      for (const Actor& a : g.actors) if (a.slot == slot && isStoryPerson(a)) there = true;
      if (there) continue;
      int tx = 0, ty = 0;
      const uint32_t salt = (uint32_t)(b->id ^ (b->id >> 32));
      int cx = b->gx - g.world.ox, cy = b->gy - g.world.oy;
      if (camp && sh >= 0) {
        // by a tent of the camp (deterministic: the tent the person's key picks), on its free side
        const Site& S = g.world.sites[(size_t)sh];
        std::vector<std::pair<int, int>> tents;
        for (int y = S.r.y - 2; y < S.r.y + S.r.h + 2; y++)
          for (int x = S.r.x - 2; x < S.r.x + S.r.w + 2; x++)
            if (g.world.over.in(x, y) && g.world.over.propAt(x, y) == (int)art::Prop::Tent + 1) tents.push_back({x, y});
        if (!tents.empty()) { const auto& t = tents[hash32(salt) % tents.size()]; cx = t.first + 1; cy = t.second + 1; }
      }
      if (!freeTileNear(g, cx, cy, tx, ty, salt)) continue;
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
      a.slot = slot;
      dressPerson(g, in, a, *b, R, sp.npc);
      // (P1b) in a bandit camp: under the camp's colours (its men leave them be), not one of the town's
      if (camp) a.faction = Faction::Bandit;
    }
  }
}

// (P1b ENGINE lane) a quiet floor tile of a cave or ruin map (deterministic in `salt`): a nook (two walls) well away
// from the way in and from the boss's ground, off the spawns and props; false: none
bool quietTile(const Map& m, uint32_t salt, int& fx, int& fy) {
  int bx = -999, by = -999;
  for (const Spawn& sp : m.spawns) if (sp.boss) { bx = sp.x; by = sp.y; }
  const int ex = m.exitX, ey = m.exitY;
  int far = 1;
  for (int y = 1; y < m.h - 1; y++)
    for (int x = 1; x < m.w - 1; x++)
      if (!m.blocked(x, y)) far = std::max(far, std::abs(x - ex) + std::abs(y - ey));
  int best = -1;
  for (int y = 1; y < m.h - 1; y++)
    for (int x = 1; x < m.w - 1; x++) {
      if (m.blocked(x, y) || m.propAt(x, y)) continue;
      const int de = std::abs(x - ex) + std::abs(y - ey);
      if (de < 9 || de * 5 < far * 2) continue;
      if (bx > -999 && std::abs(x - bx) + std::abs(y - by) < 9) continue;
      bool spawnNear = false;
      for (const Spawn& sp : m.spawns) if (std::abs(sp.x - x) + std::abs(sp.y - y) < 3) spawnNear = true;
      if (spawnNear) continue;
      int walls = 0;
      walls += m.blocked(x - 1, y); walls += m.blocked(x + 1, y); walls += m.blocked(x, y - 1); walls += m.blocked(x, y + 1);
      if (walls >= 3) continue;
      const int sc = (walls == 2 ? 40 : walls == 1 ? 22 : 0) - std::abs(de - far * 3 / 5) + (int)(hash2(x, y, salt) % 9);
      if (sc > best) { best = sc; fx = x; fy = y; }
    }
  return best >= 0;
}

// (P1b ENGINE lane) the prop a running goal wants used (goal use crate / barrel / chest / well / brazier / sacks in
// <site>) set down in that site when its map has none of them: inside a cave or a ruin on a quiet tile; in a camp or
// a town on a free tile beside the place (the beacon's tower is always there: the vignette's own)
void placeWantedProps(Game& g) {
  using namespace story;
  for (const Instance& in : g.story.running()) {
    if (in.done) continue;
    const dsl::Script* s = scriptOf(in);
    if (!s || in.stage < 0 || in.stage >= (int)s->stages.size()) continue;
    const dsl::StageDef& G = s->stages[(size_t)in.stage];
    if (G.kind != dsl::StageKind::Goal || G.goal.type != dsl::ObjType::Use || G.goal.role.empty()) continue;
    const art::Prop p = (art::Prop)G.goal.prop;
    if (p != art::Prop::Crate && p != art::Prop::Barrel && p != art::Prop::Chest && p != art::Prop::Well && p != art::Prop::Brazier &&
        p != art::Prop::Sacks)
      continue;
    const Binding* b = bindingOf(in, G.goal.role);
    if (!b) continue;
    const uint32_t salt = (uint32_t)(b->id ^ (b->id >> 32)) ^ (uint32_t)in.id * 2654435761u;
    if (g.inside) {
      if (g.subSite < 0 || g.subSite >= (int)g.world.sites.size()) continue;
      const ew::Gid here = g.world.sites[(size_t)g.subSite].id;
      if (here != b->site && here != b->id) continue;
      Map& m = g.sub;
      bool have = false;
      for (int i = 0; i < m.w * m.h && !have; i++) have = m.prop[(size_t)i] == (int)p + 1;
      int fx = 0, fy = 0;
      if (have || !quietTile(m, salt, fx, fy)) continue;
      m.setProp(fx, fy, p);
      m.solid[(size_t)fy * m.w + fx] = 1;
      continue;
    }
    if (!b->hasPos) continue;
    const int cx = b->gx - g.world.ox, cy = b->gy - g.world.oy;
    Map& m = g.world.over;
    if (!m.in(cx, cy)) continue;
    int32_t px, py;
    playerGlobal(g, px, py);
    if ((int64_t)(px - b->gx) * (px - b->gx) + (int64_t)(py - b->gy) * (py - b->gy) > 40 * 40) continue;   // (set down as the player comes near)
    bool have = false;
    for (int y = cy - 12; y <= cy + 12 && !have; y++)
      for (int x = cx - 12; x <= cx + 12 && !have; x++) have = m.in(x, y) && m.propAt(x, y) == (int)p + 1;
    if (have) continue;
    int fx = 0, fy = 0;
    if (!freeTileNear(g, cx + (int)(hash32(salt) % 5u) - 2, cy + 2 + (int)(hash32(salt >> 3) % 3u), fx, fy, salt)) continue;
    if (m.bldgAt[(size_t)fy * m.w + fx] >= 0) continue;
    m.setProp(fx, fy, p);
    m.solid[(size_t)fy * m.w + fx] = 1;
  }
}

// (P1b ENGINE lane) the people of the running stories who wait INSIDE this cave or ruin (`person ... in <site>`): each
// on a quiet floor tile of its own (deterministic per site and person: a side passage or a room's edge, well away from
// the way in and from the boss room), talkable; and the walkers come in with the player
void placeInsiders(Game& g, int site) {
  using namespace story;
  if (!g.inside || site < 0 || site >= (int)g.world.sites.size() || !host().spawnHuman) return;
  const ew::Gid sid = g.world.sites[(size_t)site].id;
  const Map& m = g.sub;
  for (const Instance& in : g.story.running()) {
    if (in.done) continue;
    const dsl::Script* s = scriptOf(in);
    if (!s) continue;
    for (size_t ri = 0; ri < s->roles.size(); ri++) {
      const dsl::RoleDef& R = s->roles[ri];
      if (R.kind != dsl::RoleKind::Person || !R.inside || following(in, *s, R)) continue;
      const Binding* b = bindingOf(in, R.name);
      if (!b || b->site != sid) continue;
      auto var = [&](const std::string& n) { auto it = in.vars.find(n); return it == in.vars.end() ? 0 : it->second; };
      if (var("_dead_" + R.name) || var("_hide_" + R.name)) continue;
      const int slot = personSlot(in.id, (int)ri);
      bool there = false;
      for (const Actor& a : g.actors) if (a.slot == slot && isStoryPerson(a)) there = true;
      if (there) continue;
      // the boss's ground (its spawn) and the way in are kept clear; a floor tile with a wall on two sides scores best
      // (a nook), then the middle of the map's depth, with a little per-person variety
      int bx = -999, by = -999;
      for (const Spawn& sp : m.spawns) if (sp.boss) { bx = sp.x; by = sp.y; }
      const int ex = m.exitX, ey = m.exitY;
      int far = 1;
      for (int y = 1; y < m.h - 1; y++)
        for (int x = 1; x < m.w - 1; x++)
          if (!m.blocked(x, y)) far = std::max(far, std::abs(x - ex) + std::abs(y - ey));
      int best = -1, fx = ex, fy = ey - 2;
      const uint32_t salt = (uint32_t)(b->id ^ (b->id >> 32)) ^ (uint32_t)ri * 7919u;
      for (int y = 1; y < m.h - 1; y++)
        for (int x = 1; x < m.w - 1; x++) {
          if (m.blocked(x, y) || m.propAt(x, y)) continue;
          const int de = std::abs(x - ex) + std::abs(y - ey);
          if (de < 9 || de * 5 < far * 2) continue;                                   // well away from the way in
          if (bx > -999 && std::abs(x - bx) + std::abs(y - by) < 9) continue;          // not the boss room
          bool spawnNear = false;
          for (const Spawn& sp : m.spawns) if (std::abs(sp.x - x) + std::abs(sp.y - y) < 3) spawnNear = true;
          if (spawnNear) continue;
          int walls = 0;
          walls += m.blocked(x - 1, y); walls += m.blocked(x + 1, y); walls += m.blocked(x, y - 1); walls += m.blocked(x, y + 1);
          if (walls >= 3) continue;   // (a dead end one tile wide: no room to stand beside them)
          const int mid = far * 3 / 5;
          const int sc = (walls == 2 ? 40 : walls == 1 ? 22 : 0) - std::abs(de - mid) + (int)(hash2(x, y, salt) % 9);
          if (sc > best) { best = sc; fx = x; fy = y; }
        }
      if (best < 0) {   // (a small map: the farthest free tile from the door that is not the boss's)
        int bd = -1;
        for (int y = 1; y < m.h - 1; y++)
          for (int x = 1; x < m.w - 1; x++) {
            if (m.blocked(x, y) || m.propAt(x, y)) continue;
            const int d = std::abs(x - ex) + std::abs(y - ey) - (bx > -999 && std::abs(x - bx) + std::abs(y - by) < 5 ? 100 : 0);
            if (d > bd) { bd = d; fx = x; fy = y; }
          }
      }
      Spawn sp;
      sp.npc = true; sp.role = Role::Villager; sp.site = -1; sp.slot = slot; sp.x = fx; sp.y = fy;
      const int id = host().spawnHuman(g, sp, fx * TILE + 8.0f, fy * TILE + 10.0f);
      const int ai = host().findActor(g, id);
      if (ai < 0) continue;
      Actor& a = g.actors[(size_t)ai];
      a.slot = slot;
      dressPerson(g, in, a, *b, R, true);
      a.face = 0;
    }
  }
}

// ---------------------------------------------------------------- (P1b ENGINE lane) the shared mechanics in play
// Every quarter second engineSync reads the running instances (their goals and vars) into Engine::walkers_ / bouts_ /
// parts_ / calm_, sets the drove's beasts out beside their drover, turns `do hostile` people on the player and keeps
// _escort_far; engineTick (every frame) runs the bouts' moments (squaring up, the yield, the stand-down); the actors
// themselves move in folkStep / hostileStep (called from ai.cpp).

// the engine's own record of the player's footsteps (Engine::trail_; the companions' trail is kept only with a party)
std::vector<Vec2> trailOf(const Game& g) {
  std::vector<Vec2> v;
  v.reserve(g.story.trail_.size());
  for (const story::Engine::Crumb& c : g.story.trail_) v.push_back(Vec2(c.x, c.y));
  return v;
}

int findA(const Game& g, int id) {
  if (story::host().findActor) return story::host().findActor(g, id);
  for (size_t k = 0; k < g.actors.size(); k++) if (g.actors[k].id == id) return (int)k;
  return -1;
}

int actorBySlot(const Game& g, int slot, bool beast = false) {
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    if (a.slot == slot && (beast ? story::isDroveBeast(a) : story::isStoryPerson(a))) return (int)k;
  }
  return -1;
}

const char* const kSquare[] = {"HAVE AT YOU!", "FISTS UP, THEN.", "TO THE FIRST FALL!", "COME ON, THEN!"};
const char* const kYield[] = {"ENOUGH! I YIELD!", "HOLD! YOU HAVE ME.", "I YIELD. WELL FOUGHT.", "ENOUGH... ENOUGH. YOURS."};
const char* const kStood[] = {"STAY DOWN. IT'S DONE.", "THAT'S THE FIGHT. UP YOU GET.", "YIELD! ...GOOD. NO HARM DONE."};

void spoken(Game& g, const Actor& a, const std::string& line, uint32_t col) {
  Event e;
  e.type = Ev::Text; e.p = a.p + Vec2(0, -30); e.a = (int)col; e.f = 0; e.s = line;
  g.events.push_back(e);
  if (story::host().say) story::host().say(g, a.name.substr(0, a.name.find(' ')) + ": " + line);
}

// a bout opponent squares up: stats matched to the player (a fair fight, not a slaughter either way), hostile to the
// player only (game.cpp collectHostiles leaves the town out of it), not to be talked to while it lasts
void armForBout(Game& g, Actor& a) {
  const Actor& p = g.pl();
  if (!a.hostile || a.npc) {   // (a foe of the story is already armed: its own stats and colours stand)
    a.level = std::max(1, g.plLevel);
    a.maxHp = std::max(40.0f, p.maxHp * 0.9f);
    a.hp = a.maxHp;
    a.dmg = std::max(3.0f, p.maxHp * 0.085f);
    a.windup = 0.42f; a.range = 15; a.speed = 52; a.aggroR = 220; a.armor = 0;
    // the town's own colours: hostile to the player alone (warFoes: whoever is hostile fights the player), so neither
    // the bystanders nor the watch are any part of it, and its blows land on nobody else
    a.faction = Faction::Town;
  }
  a.hostile = true; a.npc = false; a.aggro = true; a.target = p.id;
  a.posture = art::Posture::None; a.fleeing = false;
}
// they stand down: friendly again (the yield, the player beaten, the player gone)
void standDown(Actor& a) {
  a.hostile = false; a.npc = true; a.faction = Faction::Town; a.aggro = false; a.target = -1;
  a.st = AState::Idle; a.stT = 0; a.heavy = a.lunge = false; a.knock = Vec2();
}

void engineSync(Game& g) {
  using namespace story;
  Engine& E = g.story;
  std::vector<Engine::Walker> keepW;
  std::vector<Engine::Bout> keepB;
  E.calm_.clear();
  E.parts_.clear();
  for (Instance& in : E.running_) {
    if (in.done) continue;
    const dsl::Script* s = scriptOf(in);
    if (!s) continue;
    const dsl::StageDef& G = s->stages[(size_t)in.stage];
    for (size_t ri = 0; ri < s->roles.size(); ri++) {
      const dsl::RoleDef& R = s->roles[ri];
      if (!following(in, *s, R)) continue;
      Engine::Walker w;
      for (const Engine::Walker& o : E.walkers_) if (o.inst == in.id && o.role == (int)ri) w = o;
      const bool esc = G.kind == dsl::StageKind::Goal && G.goal.type == dsl::ObjType::Escort && G.goal.role == R.name;
      const int skin = esc ? G.goal.n : dsl::ESCORT_WALK;
      if (w.inst != in.id || w.skin != skin) {
        w = Engine::Walker();
        w.inst = in.id; w.role = (int)ri; w.skin = skin;
        const uint32_t h = hash32((uint32_t)in.id * 2654435761u ^ (uint32_t)ri * 40503u);
        w.herd = skin == dsl::ESCORT_DROVE ? 3 + (int)(h % 3u) : 0;
        w.laneOff = ((int)((h >> 8) % 5u) - 2) * 2.0f;
      }
      keepW.push_back(w);
    }
    if (G.kind == dsl::StageKind::Goal && G.goal.type == dsl::ObjType::Bout) {
      const int ri = s->role(G.goal.role);
      if (ri >= 0) {
        Engine::Bout b;
        for (const Engine::Bout& o : E.bouts_) if (o.inst == in.id && o.role == ri) b = o;
        if (b.inst != in.id) { b = Engine::Bout(); b.inst = in.id; b.role = ri; }
        keepB.push_back(b);
      }
    }
    for (auto it = in.vars.lower_bound("_calm_"); it != in.vars.end() && it->first.compare(0, 6, "_calm_") == 0; ++it) {
      if (!it->second) continue;
      const Binding* b = bindingOf(in, it->first.substr(6));
      if (b) E.calm_.insert(b->site ? b->site : b->id);
    }
  }
  E.walkers_.swap(keepW);
  E.bouts_.swap(keepB);
  // the bodies
  for (Engine::Walker& w : E.walkers_) {
    const int ai = actorBySlot(g, personSlot(w.inst, w.role));
    w.actor = ai >= 0 && g.actors[(size_t)ai].st != AState::Dead ? g.actors[(size_t)ai].id : -1;
    if (w.actor >= 0) E.parts_[w.actor] = Engine::P_FOLLOW;
  }
  for (Engine::Bout& b : E.bouts_) {
    const int ai = actorBySlot(g, personSlot(b.inst, b.role));
    const int was = b.actor;
    b.actor = ai >= 0 && g.actors[(size_t)ai].st != AState::Dead ? g.actors[(size_t)ai].id : -1;
    if (b.actor != was && b.phase != 4) { b.phase = 0; b.t = 0; }   // (a new body: the bout begins again)
    if (b.actor >= 0) {
      static const uint8_t kp[] = {Engine::P_BOUT_WAIT, Engine::P_BOUT, Engine::P_YIELD, Engine::P_BEATEN, Engine::P_YIELD};
      E.parts_[b.actor] = kp[std::min<int>(b.phase, 4)];
      // (a bout won or lost: the opponent is friendly whatever their part says next)
    }
  }
  // `do hostile`: the person turns on the player (to the death; the watch treats them as the attacker they are)
  for (Instance& in : E.running_) {
    if (in.done) continue;
    const dsl::Script* s = scriptOf(in);
    if (!s) continue;
    for (auto it = in.vars.lower_bound("_hostile_"); it != in.vars.end() && it->first.compare(0, 9, "_hostile_") == 0; ++it) {
      if (!it->second) continue;
      const int ri = s->role(it->first.substr(9));
      if (ri < 0) continue;
      const int ai = actorBySlot(g, personSlot(in.id, ri));
      if (ai < 0) continue;
      Actor& a = g.actors[(size_t)ai];
      if (a.st == AState::Dead) continue;
      if (E.actorPart(a.id) == Engine::P_BOUT || E.actorPart(a.id) == Engine::P_YIELD || E.actorPart(a.id) == Engine::P_BEATEN) continue;
      E.parts_[a.id] = Engine::P_HOSTILE;
      if (!a.hostile || a.npc) {
        const int L = std::max(std::max(1, g.plLevel), storyDanger(g, in));
        a.level = L;
        a.maxHp = std::max(60.0f, g.pl().maxHp * 1.15f);
        a.hp = a.maxHp;
        a.dmg = std::max(5.0f, g.pl().maxHp * 0.11f);
        a.windup = 0.38f; a.range = 15; a.speed = 54; a.aggroR = 200;
        a.hostile = true; a.npc = false; a.faction = Faction::Bandit; a.aggro = true; a.target = g.pl().id;
        a.posture = art::Posture::None;
        a.xp = std::max(a.xp, 20 + 6 * L);
        a.look.weapon = a.look.weapon ? a.look.weapon : 1;
      }
    }
  }
  // the people waiting inside a site, and the drove's beasts
  for (Instance& in : E.running_) {
    if (in.done || !g.inside) continue;
    const dsl::Script* s = scriptOf(in);
    if (!s) continue;
    for (size_t ri = 0; ri < s->roles.size(); ri++) {
      if (!s->roles[ri].inside) continue;
      const int ai = actorBySlot(g, personSlot(in.id, (int)ri));
      if (ai >= 0 && !E.parts_.count(g.actors[(size_t)ai].id)) E.parts_[g.actors[(size_t)ai].id] = Engine::P_INSIDE;
    }
  }
  // drove: the beasts kept beside their drover (set out when missing: a drove that lost one to wolves stays smaller);
  // beasts of no drove (the escort ended, the story moved on) graze where they stopped until the player is well away
  std::set<int> wanted;
  for (Engine::Walker& w : E.walkers_) {
    if (w.skin != dsl::ESCORT_DROVE || w.actor < 0) continue;
    const int di = findA(g, w.actor);
    if (di < 0) continue;
    const Vec2 dp = g.actors[(size_t)di].p;
    Instance* in = E.find(w.inst);
    if (!in) continue;
    const uint32_t h0 = hash32((uint32_t)w.inst * 0x85EBCA6Bu ^ 0xD20Eu);
    const bool sheep = (h0 % 5u) < 3u;   // a flock (3 in 5) or cattle
    for (int k = 0; k < w.herd; k++) {
      const int slot = DROVE_SLOT0 + (int)(w.inst % 1024u) * DROVE_MAX + k;
      if (in->vars.count("_drove_dead" + std::to_string(slot))) continue;   // (lost on the road: never set out again)
      wanted.insert(slot);
      const int bi = actorBySlot(g, slot, true);
      if (bi >= 0) {
        Actor& b = g.actors[(size_t)bi];
        if (b.st == AState::Dead) continue;
        E.parts_[b.id] = Engine::P_BEAST;
        if (len2(b.p - dp) > (30.0f * TILE) * (30.0f * TILE)) {   // far behind (a door, a fast travel): back by the drover
          b.p = ClimbOps::freeSpot(g, (int)std::floor(dp.x / TILE) + (k % 3) - 1, (int)std::floor(dp.y / TILE) + 1 + k / 3);
          b.navNext = -1;
        }
        continue;
      }
      const Vec2 at = ClimbOps::freeSpot(g, (int)std::floor(dp.x / TILE) + (k % 3) - 1, (int)std::floor(dp.y / TILE) + 1 + k / 3);
      Actor a;
      a.id = HomeOps::nextId(g);
      a.npc = true; a.human = false; a.hostile = false; a.faction = Faction::Town;
      const art::Critter kind = sheep ? art::Critter::Sheep : art::Critter::Cow;
      a.critter = (uint8_t)((int)kind + 1);
      a.critterVar = hash32(h0 + (uint32_t)k * 2246822519u) & (sheep ? 0x7Fu : 0xFFu);
      a.p = at; a.home = at; a.goal = at;
      a.site = -1; a.slot = slot; a.fromMap = false;
      a.maxHp = a.hp = sheep ? 26.0f : 44.0f;
      a.radius = 4.0f;
      a.speed = sheep ? 34.0f : 30.0f;
      a.name = sheep ? "SHEEP" : "COW";
      a.level = 1;
      a.face = g.actors[(size_t)di].face;
      g.actors.push_back(a);
      E.parts_[a.id] = Engine::P_BEAST;
    }
  }
  for (size_t k = 1; k < g.actors.size();) {
    const Actor& a = g.actors[k];
    if (isDroveBeast(a) && !wanted.count(a.slot)) {
      if (a.st != AState::Dead && len2(a.p - g.pl().p) > (34.0f * TILE) * (34.0f * TILE)) { g.actors.erase(g.actors.begin() + (std::ptrdiff_t)k); continue; }
      if (a.st != AState::Dead) E.parts_[a.id] = Engine::P_BEAST;
    }
    k++;
  }
  // a beast of a running drove that died: remembered as lost (the drove arrives smaller; never set out again)
  for (const Actor& a : g.actors) {
    if (!isDroveBeast(a) || a.st != AState::Dead) continue;
    const uint32_t im = (uint32_t)((a.slot - DROVE_SLOT0) / DROVE_MAX);
    for (Instance& in : E.running_)
      if (!in.done && in.id % 1024u == im) {
        std::string key = "_drove_dead" + std::to_string(a.slot);
        if (!in.vars.count(key)) { in.vars[key] = 1; in.vars["_drove_lost"]++; }
      }
  }
  // a road ambush held back in a settlement (story.cpp, EffType::Ambush) falls on the player once out on the road;
  // dropped if the story moved on to another stage meanwhile
  for (Instance& in : E.running_) {
    auto it = in.vars.find("_amb_n");
    if (it == in.vars.end()) continue;
    if (in.done || in.vars["_amb_stage"] != in.stage) { in.vars.erase("_amb_n"); in.vars.erase("_amb_m"); in.vars.erase("_amb_stage"); continue; }
    if (g.inside || g.settlementAt(g.pl().p) >= 0) continue;
    const int m = in.vars["_amb_m"], n = it->second;
    in.vars.erase("_amb_n"); in.vars.erase("_amb_m"); in.vars.erase("_amb_stage");
    storyAmbush(g, in, m, n);
  }
  // the escort is with the player? (within 12 tiles, and a drove gathered round its drover)
  for (Instance& in : E.running_) {
    if (in.done) continue;
    const dsl::Script* s = scriptOf(in);
    if (!s) continue;
    const dsl::StageDef& G = s->stages[(size_t)in.stage];
    if (G.kind != dsl::StageKind::Goal || G.goal.type != dsl::ObjType::Escort) { in.vars.erase("_escort_far"); continue; }
    const int ri = s->role(G.goal.role);
    const Engine::Walker* w = nullptr;
    for (const Engine::Walker& o : E.walkers_) if (o.inst == in.id && o.role == ri) w = &o;
    bool far = true;
    if (w && w->actor >= 0) {
      const int ai = findA(g, w->actor);
      if (ai >= 0) {
        const Actor& a = g.actors[(size_t)ai];
        far = len2(a.p - g.pl().p) > (12.0f * TILE) * (12.0f * TILE);
        if (!far && w->skin == dsl::ESCORT_DROVE)
          for (const Actor& b : g.actors)
            if (isDroveBeast(b) && b.st != AState::Dead && (uint32_t)((b.slot - DROVE_SLOT0) / DROVE_MAX) == in.id % 1024u &&
                len2(b.p - a.p) > (10.0f * TILE) * (10.0f * TILE)) far = true;
      }
    }
    in.vars["_escort_far"] = far ? 1 : 0;
    // the walker's place is where they are (a save, a door, the end of the road: they are found there)
    if (w && w->actor >= 0 && !g.inside) {
      const int ai = findA(g, w->actor);
      if (ai >= 0)
        if (Binding* b = bindingOf(in, G.goal.role)) {
          b->gx = g.world.ox + (int32_t)std::floor(g.actors[(size_t)ai].p.x / TILE);
          b->gy = g.world.oy + (int32_t)std::floor(g.actors[(size_t)ai].p.y / TILE);
          b->hasPos = true;
        }
    }
  }
  // any other walker (`do follow`) is found where they walked too
  for (const Engine::Walker& w : E.walkers_) {
    if (w.actor < 0 || g.inside) continue;
    Instance* in = E.find(w.inst);
    const dsl::Script* s = in ? scriptOf(*in) : nullptr;
    const int ai = findA(g, w.actor);
    if (!s || ai < 0 || w.role >= (int)s->roles.size()) continue;
    if (Binding* b = bindingOf(*in, s->roles[(size_t)w.role].name)) {
      b->gx = g.world.ox + (int32_t)std::floor(g.actors[(size_t)ai].p.x / TILE);
      b->gy = g.world.oy + (int32_t)std::floor(g.actors[(size_t)ai].p.y / TILE);
      b->hasPos = true;
    }
  }
}

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

// a walker's way: the player's own footsteps (gates, bridges, the street they took), `back` px of trail behind them;
// one fallen behind picks the trail up where it is nearest
Vec2 trailGoal(const Game& g, const Actor& a, float back, float lane) {
  const Actor& p = g.pl();
  const std::vector<Vec2> tr = trailOf(g);
  Vec2 goal = p.p - p.aim * back;
  if (tr.empty()) return goal;
  float acc = len(p.p - tr.back());
  int j = (int)tr.size() - 1;
  while (j > 0 && acc < back) { acc += len(tr[(size_t)j] - tr[(size_t)j - 1]); j--; }
  if (len2(tr[(size_t)j] - p.p) > (back + 3.0f * TILE) * (back + 3.0f * TILE)) return goal;   // (a stale trail)
  goal = tr[(size_t)j];
  const float d2 = len2(a.p - p.p);
  if (d2 > (5.0f * TILE) * (5.0f * TILE)) {
    int k0 = std::max(0, (int)tr.size() - 80), bk = -1;
    float bd = 1e30f;
    for (int k = k0; k < (int)tr.size(); k++) { const float e = len2(tr[(size_t)k] - a.p); if (e < bd) { bd = e; bk = k; } }
    if (bk >= 0 && bk < j && bd < (8.0f * TILE) * (8.0f * TILE)) goal = tr[(size_t)std::min(bk + 2, j)];
  } else if (lane != 0.0f) {
    // close by: a little to one side of the footsteps (people walk beside the way, not in each other's prints)
    const Vec2 dir = norm(p.p - goal + Vec2(0.001f, 0.0f));
    goal += Vec2(-dir.y, dir.x) * lane;
  }
  return goal;
}

bool onDoorstep(const Game& g, int tx, int ty) {
  if (g.inside) return false;
  for (const Bldg& B : g.world.over.bldgs)
    if (B.doorX() == tx && (B.doorY() == ty || B.doorY() + 1 == ty)) return true;
  return false;
}

// the walker: behind the player at a walking distance, easing in and out of their pace, halting when they halt,
// catching up when left behind (a jog), the trail through gates and round corners; a wagon's cart trails them
void followStep(Game& g, Actor& a, story::Engine::Walker& w, float dt) {
  using namespace story;
  Actor& p = g.pl();
  if (g.mode == Mode::Dialogue && g.dlg.actor == a.id) { a.st = AState::Idle; a.face = faceOf(p.p - a.p); w.spd = 0; return; }
  const float T = (float)TILE;
  const float back = w.skin == dsl::ESCORT_WAGON ? 3.0f * T : w.skin == dsl::ESCORT_DROVE ? 2.5f * T : 2.0f * T;
  // a jump of the body (set down beside the player, a window shift's fix-up): the cart starts afresh behind them
  if (len2(a.p - Vec2(w.lastX, w.lastY)) > 40.0f * 40.0f) w.cartSet = false;
  const float d = len(p.p - a.p);
  const Vec2 goal = trailGoal(g, a, back, w.laneOff);
  const float gd = len(goal - a.p);
  const bool plMoving = p.st == AState::Walk || p.st == AState::Roll;
  const float ps = std::max(40.0f, p.speed * (g.homeRiding() ? g.homeSpeedMul() : 1.0f));
  float want = 0;
  // walking with the player: a touch under their pace at the place behind them, a touch over it when it falls back (so
  // the gap settles about a third of a tile behind the place: 2-3 tiles in all)
  if (plMoving) want = ps * std::clamp(0.85f + 0.45f * (gd / T), 0.85f, 1.5f);
  if (plMoving && d > back + 1.3f * T) want = std::max(want, ps * 1.3f);   // (a bend or a climb cost them ground)
  else if (d > back + 0.9f * T && gd > 0.8f * T) want = ps * std::clamp(0.45f + 0.15f * (gd / T), 0.45f, 1.1f);
  if (d > 6.0f * T) want = ps * (w.skin == dsl::ESCORT_WAGON ? 1.5f : 1.65f);   // left behind: a jog
  if (!plMoving && d < back * 0.9f) want = 0;
  // eased: no snapping into the player's pace or out of it (quicker to stop than to start, as people are)
  const float k = want > w.spd ? 2.6f : 5.0f;
  w.spd += (want - w.spd) * (1.0f - std::exp(-dt * k));
  if (w.spd < 2.5f && want <= 0) w.spd = 0;
  const Vec2 before = a.p;
  if (w.spd > 0.5f) {
    if (!ClimbOps::navStep(g, a, goal, w.spd, dt)) ClimbOps::moveActor(g, a, norm(goal - a.p) * (w.spd * dt));
    a.st = AState::Walk;
    // the stride follows the pace (a jog's quicker steps, a slow walk's slower ones: feet that do not slide)
    a.animT += dt * (std::clamp(w.spd / 52.0f, 0.55f, 1.7f) - 1.0f);
  } else {
    a.st = AState::Idle;
    if (d < 7.0f * T) a.face = faceOf(p.p - a.p);
    // never left standing in a doorway
    const int tx = (int)std::floor(a.p.x / T), ty = (int)std::floor((a.p.y - 2) / T);
    if (onDoorstep(g, tx, ty)) {
      const int side = (w.laneOff >= 0 ? 1 : -1);
      ClimbOps::moveActor(g, a, Vec2((float)side * 30.0f * dt, 0));
      a.st = AState::Walk;
    }
  }
  // stuck (a fence corner the way finder keeps failing at): the clock runs while it should move and does not
  const float moved = len(a.p - before);
  if (want > 8.0f && moved < want * dt * 0.15f) w.stuckT += dt;
  else w.stuckT = std::max(0.0f, w.stuckT - dt * 2.0f);
  if (w.stuckT > 1.2f) a.navNext = -1;
  const bool farOff = d > 22.0f * T;
  if (w.stuckT > 2.4f || farOff) {
    int tx = 0, ty = 0;
    if (besidePlayer(g, (farOff ? 9.0f : 3.0f) * T, tx, ty, (uint32_t)a.id * 7919u)) {
      a.p = Vec2(tx * T + 8.0f, ty * T + 10.0f);
      a.navNext = -1;
      w.cartSet = false;
      w.spd = ps * 0.6f;
    }
    w.stuckT = 0;
  }
  // the wagon: a hand cart on the driver's shafts, dragged along behind them (its heading is the tow's)
  if (w.skin == dsl::ESCORT_WAGON) {
    const float L = 21.0f;   // (the driver stands between the shafts, the cross bar at their hands)
    if (!w.cartSet) {
      const Vec2 bk = a.face == 0 ? Vec2(0, -1) : a.face == 1 ? Vec2(0, 1) : a.face == 2 ? Vec2(-1, 0) : Vec2(1, 0);
      w.cartX = a.p.x + bk.x * L; w.cartY = a.p.y + bk.y * L;
      w.cartAng = std::atan2(-bk.y, -bk.x);
      w.cartSet = true;
    }
    Vec2 c(w.cartX, w.cartY);
    const Vec2 dv = a.p - c;
    const float l = len(dv);
    if (l > 0.5f && std::fabs(l - L) > 0.05f) {   // the shafts are rigid: dragged on, or pushed back when the driver backs up
      const Vec2 nc = c + dv * ((l - L) / l);
      w.wheel += (l > L ? 1.0f : -1.0f) * len(nc - c) / 5.0f;
      c = nc;
    }
    if (l > 0.5f) {
      // the heading eases toward the tow (a cart swings round, it does not snap)
      const float want = std::atan2(dv.y, dv.x);
      float da = want - w.cartAng;
      while (da > 3.14159265f) da -= 6.2831853f;
      while (da < -3.14159265f) da += 6.2831853f;
      w.cartAng += da * (1.0f - std::exp(-dt * 7.0f));
    }
    w.cartX = c.x; w.cartY = c.y;
  }
  w.lastX = a.p.x; w.lastY = a.p.y;
}

// a beast of a drove: trailing its drover in a loose bunch, each its own pace and a little wander, gathering again when
// strung out; grazing when the drove stands
void droveStep(Game& g, Actor& a, float dt) {
  using namespace story;
  Engine& E = g.story;
  const uint32_t im = (uint32_t)((a.slot - DROVE_SLOT0) / DROVE_MAX);
  const int k = (a.slot - DROVE_SLOT0) % DROVE_MAX;
  const Engine::Walker* w = nullptr;
  for (const Engine::Walker& o : E.walkers_) if (o.skin == dsl::ESCORT_DROVE && o.inst % 1024u == im && o.actor >= 0) w = &o;
  const int di = w ? findA(g, w->actor) : -1;
  const uint32_t h = hash32((uint32_t)a.id * 0x9E3779B1u);
  const float own = (float)(h % 1000u) * 0.001f;
  if (di < 0) {   // no drove now: it grazes where it stopped
    a.st = AState::Idle;
    return;
  }
  const Actor& dr = g.actors[(size_t)di];
  const float T = (float)TILE;
  // the drove's heading: from the herd toward the drover (the way they are all going)
  const Vec2 toD = dr.p - a.p;
  const float dd = len(toD);
  const Vec2 head = dd > 0.5f ? toD * (1.0f / dd) : Vec2(0, 1);
  // its own place in the bunch: rows behind the drover, a side offset, a slow wander (each beast on its own clock)
  const float row = 1.6f + (float)(k / 2) * 1.1f + own * 0.6f;
  const float side = ((k & 1) ? 1.0f : -1.0f) * (0.6f + 0.5f * own) + std::sin(g.time * (0.35f + own * 0.4f) + own * 11.0f) * 0.45f;
  Vec2 goal = dr.p - head * (row * T) + Vec2(-head.y, head.x) * (side * T);
  // elbow room: a step away from a neighbour pressed against it
  for (const Actor& o : g.actors) {
    if (o.id == a.id || !isDroveBeast(o) || o.st == AState::Dead) continue;
    const Vec2 dv = a.p - o.p;
    const float l2 = len2(dv);
    if (l2 < 11.0f * 11.0f && l2 > 0.01f) goal += dv * (6.0f / std::sqrt(l2));
  }
  const float gd = len(goal - a.p);
  const bool drMoving = dr.st == AState::Walk;
  const float base = std::max(40.0f, w->spd * 1.05f);   // (the drover's own pace, a touch over it to keep up)
  float want = 0;
  if (gd > 0.7f * T || (drMoving && gd > 0.3f * T)) want = base * std::clamp(0.55f + 0.35f * (gd / T), 0.5f, 1.9f) * (0.9f + 0.25f * own);
  if (dd > 9.0f * T) want = base * 2.0f;   // strung out: a trot to gather
  float& spd = a.special;   // (the beast's eased pace: Actor::special is unused by a critter's own AI here)
  const float kk = want > spd ? 2.0f + own : 4.5f;
  spd += (want - spd) * (1.0f - std::exp(-dt * kk));
  if (spd < 2.0f && want <= 0) spd = 0;
  const Vec2 before = a.p;
  if (spd > 0.5f) {
    if (!ClimbOps::navStep(g, a, goal, spd, dt)) ClimbOps::moveActor(g, a, norm(goal - a.p) * (spd * dt));
    a.st = AState::Walk;
    a.animT += dt * (std::clamp(spd / 40.0f, 0.5f, 1.8f) - 1.0f);
    // stuck behind a fence or a bank (Actor::lifeT: the beast's stuck clock; the life lane never runs a drove's beast),
    // or strung out far behind: back into the bunch behind the drover (out of the view's middle)
    if (len(a.p - before) < spd * dt * 0.15f) a.lifeT += dt; else a.lifeT = std::max(0.0f, a.lifeT - dt * 2.0f);
    if (a.lifeT > 2.0f || dd > 13.0f * T || (dd > 9.0f * T && a.lifeT > 0.8f)) {
      const Vec2 at = dr.p - head * ((row + 0.5f) * T);
      a.p = ClimbOps::freeSpot(g, (int)std::floor(at.x / T), (int)std::floor(at.y / T));
      a.navNext = -1;
      a.lifeT = 0;
      spd = base;
    }
  } else {
    a.st = AState::Idle;   // (grazing the way it was going: the view plays its cropping frames now and then)
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
    engineSync(*this);     // (P1b: _escort_far as things stand now, before the goals are judged)
    story.step(*this);
    syncPersons(*this);
    engineSync(*this);     // (P1b ENGINE lane: walkers, bouts, hostile people, calm sites, the drove, _escort_far)
    standInsLive(*this);   // (P1: a round by day, a doorstep by night)
    if (inside) crownTheKing(*this);
    story::heraldsProclaim(*this);
  }
  engineTick(*this, dt);   // (P1b ENGINE lane: the bouts' moments)
  story.clock_ += dt;
  // (P1b, lead) an ending reached off a dialogue (a goal done, a clock run out, an escort lost) is told now as a closing
  // scene: its say under the giver's name (or the tale's title when it is told as a story), one FAREWELL
  // (P1b ENGINE lane) it waits out a fight (no roused foe near the player), a journey and a fade, and a breath after the
  // moment that ended it (the last blow, the arrival) so that moment is seen first
  bool epiCalm = mode == Mode::Play && !travelling() && sleepFade <= 0.0f;
  if (epiCalm && !story.epilogues_.empty())
    for (size_t i = 1; i < actors.size() && epiCalm; i++) {
      const Actor& e = actors[i];
      if (e.hostile && e.aggro && e.st != AState::Dead && len2(e.p - pl().p) < (14.0f * TILE) * (14.0f * TILE)) epiCalm = false;
    }
  if (story.epilogues_.empty() || !epiCalm) story.engineT_ = 0;
  else story.engineT_ += dt;
  if (epiCalm && story.engineT_ > 1.1f && !story.epilogues_.empty()) {
    story.engineT_ = 0;
    const story::Engine::Epilogue ep = story.epilogues_.front();
    story.epilogues_.erase(story.epilogues_.begin());
    dlg = Dialogue();
    dlg.actor = -1;
    dlg.speaker = ep.speaker;
    dlg.role = Role::Villager;
    dlg.text = ep.text;
    dlg.opts.push_back({"FAREWELL.", 0, 0});
    mode = Mode::Dialogue;
  }
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
      placeWantedProps(*this);   // (P1b ENGINE lane: a prop a goal wants, set down as the player comes near its place)
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
    // (P1b ENGINE lane) a walker on the road with the player has their own words
    static const char* const kWalk[] = {"LEAD ON. I'M RIGHT BEHIND YOU.", "KEEP GOING. I CAN KEEP UP.", "NOT FAR NOW, I HOPE.",
                                        "I'LL STAY CLOSE. LEAD THE WAY."};
    if (story.actorPart(a.id) == story::Engine::P_FOLLOW) {
      speech::propose(dlg, speech::P_STORY, speech::Topic::Story, kWalk[hash32((uint32_t)a.slot ^ (uint32_t)day * 31u) % 4]);
      return;
    }
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

// (P1b, lead) a radiant job begun from "ANY WORK GOING?": its composed script starts on the giver, whose opening talk (the
// pitch with ACCEPT / DECLINE) takes over the open dialogue; a decline closes it quietly (the "_quiet" opening)
bool Game::storyBeginJob(Actor& a, const std::string& specId) {
  STORY_HOST_INSTALL();
  using namespace story;
  const uint32_t id = story.start(*this, specId, a.id, homeSiteOf(*this, a));
  if (!id) return false;
  Instance* in = story.find(id);
  if (!in || !story.talksTo(*this, *in, a)) {   // (a template whose opening is not the giver's talk: refused, quietly)
    if (in) { in->vars["_quiet"] = 1; story.finish(*this, *in, false); }
    return false;
  }
  dlg.opts.clear();
  story.showDialogue(*this, *in);
  return true;
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
  if (site >= 0) placeInsiders(*this, site);   // (P1b ENGINE lane: the people waiting inside)
  placeWantedProps(*this);                     // (P1b: a crate to search where the goal sends the player)
  syncPersons(*this);                          // (P1b: the walkers come in with the player at once)
  engineSync(*this);
}

bool Game::storyUseProp(art::Prop p, int tx, int ty) {
  STORY_HOST_INSTALL();
  using namespace story;
  // (P1b ENGINE lane) a prop a running story asked for (goal use beacon / crate / barrel / chest / well / brazier / sacks)
  if (useWantedProp(*this, (int)p, tx, ty)) return true;
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

// ---------------------------------------------------------------- (P1b ENGINE lane) the hooks (story.h)
namespace story {

bool folkStep(Game& g, Actor& a, float dt) {
  Engine& E = g.story;
  const uint8_t part = E.actorPart(a.id);
  if (part == Engine::P_NONE) return false;
  switch (part) {
    case Engine::P_FOLLOW:
      for (Engine::Walker& w : E.walkers_)
        if (w.actor == a.id) { followStep(g, a, w, dt); return true; }
      return false;
    case Engine::P_BEAST: droveStep(g, a, dt); return true;
    case Engine::P_YIELD: case Engine::P_BEATEN:
      // the bout's moment (engineTick keeps the pose); they do not wander off in the middle of it
      a.st = AState::Idle;
      if (len2(g.pl().p - a.p) < (8.0f * TILE) * (8.0f * TILE)) a.face = faceOf(g.pl().p - a.p);
      return true;
    case Engine::P_INSIDE:
      // waiting where the story put them (a nook of the cave): they turn to the player as the player comes near
      a.st = AState::Idle;
      if (len2(g.pl().p - a.p) < (6.0f * TILE) * (6.0f * TILE)) a.face = faceOf(g.pl().p - a.p);
      return true;
    default: return false;   // (waiting for a bout: their usual round)
  }
}

bool hostileStep(Game& g, Actor& a, float dt) {
  Engine& E = g.story;
  if (isStoryPerson(a)) {
    const uint8_t part = E.actorPart(a.id);
    if (part == Engine::P_BOUT) {   // the bout's opponent fights the player only, and never gives up the fight
      a.target = g.pl().id; a.aggro = true; a.fleeing = false;
      a.thinkT = std::max(a.thinkT, 0.3f);
      return false;
    }
  }
  if (a.npc || a.player) return false;
  ew::Gid here = 0;
  if (g.inside) { if (g.subSite >= 0 && g.subSite < (int)g.world.sites.size()) here = g.world.sites[(size_t)g.subSite].id; }
  else if (a.site >= 0 && a.site < (int)g.world.sites.size()) here = g.world.sites[(size_t)a.site].id;
  if (!here) return false;
  // a running story's calm, or a finished story's lasting one (`do calm` in its end stage: MK_P1B_CALM)
  if (!E.calm_.count(here) && !g.marks.count(storyMarkKey(here, MK_P1B_CALM))) return false;
  // stood down: no attack, a slow wander about its ground (each on its own clock), watching the player pass
  a.aggro = false; a.target = -1; a.fleeing = false;
  if (a.st == AState::Windup || a.st == AState::Strike || a.st == AState::Recover) { a.st = AState::Idle; a.stT = 0; a.heavy = a.lunge = false; }
  a.thinkT -= dt;
  if (a.thinkT <= 0) {
    const uint32_t h = hash32((uint32_t)a.id * 2654435761u ^ (uint32_t)(g.time * 0.5f));
    a.thinkT = 2.5f + (float)(h % 400u) * 0.01f;
    if (h & 1u) a.goal = a.home + Vec2((float)((int)((h >> 4) % 7u) - 3) * TILE * 0.8f, (float)((int)((h >> 8) % 5u) - 2) * TILE * 0.8f);
    else a.goal = a.p;
  }
  const Vec2 d = a.goal - a.p;
  if (len2(d) > 9.0f) { ClimbOps::moveActor(g, a, norm(d) * std::min(a.speed * 0.35f * dt, len(d))); a.face = faceOf(d); a.aim = norm(d); a.st = AState::Walk; }
  else {
    a.st = AState::Idle;
    if (len2(g.pl().p - a.p) < (5.0f * TILE) * (5.0f * TILE)) { a.face = faceOf(g.pl().p - a.p); a.aim = norm(g.pl().p - a.p); }
  }
  return true;
}

bool untouchable(const Game& g, const Actor& v, int attacker) {
  if (v.player) return false;
  if (!isStoryPerson(v)) return false;
  const uint8_t part = g.story.actorPart(v.id);
  if (part == Engine::P_YIELD || part == Engine::P_BEATEN) return true;   // (a bout is over: no blow lands)
  // a friendly story person under a camp's colours, or waiting inside a site: not the player's to strike
  if (!v.hostile && attacker == g.pl().id) return true;
  return false;
}

void boutBlow(Game& g, Actor& v, float& dmg, int attacker) {
  Engine& E = g.story;
  if (E.bouts_.empty() || dmg <= 0) return;
  if (v.player) {
    // the player at nothing in a bout: left on one knee with 1 HP (no death, nothing lost), the opponent stands down
    if (v.hp - dmg >= 1.0f) return;
    for (Engine::Bout& b : E.bouts_) {
      if (b.phase != 1 || b.actor < 0 || b.actor != attacker) continue;
      dmg = std::max(0.0f, v.hp - 1.0f);
      b.phase = 3; b.t = 0;
      v.iframes = std::max(v.iframes, 2.2f);
      const int ai = findA(g, b.actor);
      if (ai >= 0) {
        Actor& a = g.actors[(size_t)ai];
        standDown(a);
        a.knock = norm(a.p - v.p) * 40.0f;
        spoken(g, a, kStood[hash32((uint32_t)a.id) % 3u], rgba(255, 200, 150));
        E.parts_[a.id] = Engine::P_BEATEN;
      }
      return;
    }
    return;
  }
  if (!isStoryPerson(v)) return;
  for (Engine::Bout& b : E.bouts_) {
    if (b.actor != v.id || b.phase != 1) continue;
    const float floor = v.maxHp * 0.25f;
    if (v.hp - dmg > floor) return;
    // a quarter health: they yield (held there), drop to one knee, say so; no more blows land
    dmg = std::max(0.0f, v.hp - floor);
    b.phase = 2; b.t = 0;
    standDown(v);
    v.posture = art::Posture::Kneel;
    spoken(g, v, kYield[hash32((uint32_t)v.id * 31u) % 4u], rgba(255, 230, 160));
    E.parts_[v.id] = Engine::P_YIELD;
    return;
  }
}

bool threatensTown(const Game& g, const Actor& a) {
  if (!isStoryPerson(a)) return true;
  const uint8_t part = g.story.actorPart(a.id);
  return part != Engine::P_BOUT && part != Engine::P_YIELD && part != Engine::P_BEATEN;
}

namespace {
bool p1bProp(int prop) {
  const art::Prop p = (art::Prop)prop;
  return p == art::Prop::WatchtowerRuin || p == art::Prop::Crate || p == art::Prop::Barrel || p == art::Prop::Chest || p == art::Prop::Well ||
         p == art::Prop::Brazier || p == art::Prop::Sacks;
}
}  // namespace

int wantedProp(const Game& g, int& otx, int& oty) {
  const Engine& E = g.story;
  // the props the running stories wait on now (each with its place: within 30 tiles of it, or inside it)
  int want[8];
  int n = 0;
  for (const Instance& in : E.running_) {
    if (in.done || n >= 8) continue;
    const dsl::Script* s = scriptOf(in);
    if (!s || in.stage < 0 || in.stage >= (int)s->stages.size()) continue;
    const dsl::StageDef& G = s->stages[(size_t)in.stage];
    if (G.kind != dsl::StageKind::Goal || G.goal.type != dsl::ObjType::Use || !p1bProp(G.goal.prop)) continue;
    if (!G.goal.role.empty()) {
      const Binding* b = bindingOf(in, G.goal.role);
      if (!b) continue;
      int32_t px, py;
      playerGlobal(g, px, py);
      bool there = false;
      if (g.inside && g.subSite >= 0 && g.subSite < (int)g.world.sites.size()) there = g.world.sites[(size_t)g.subSite].id == b->site || g.world.sites[(size_t)g.subSite].id == b->id;
      if (!there && b->hasPos) there = (int64_t)(px - b->gx) * (px - b->gx) + (int64_t)(py - b->gy) * (py - b->gy) <= 30 * 30;
      if (!there) continue;
    }
    want[n++] = G.goal.prop;
  }
  if (!n) return 0;
  const Actor& p = g.pl();
  const Map& m = g.map();
  const Vec2 probe = p.p + p.aim * 12.0f + Vec2(0, -4);
  // (a tower's fire basket is reached from its foot: a wider look for it)
  for (int r = 0; r <= 2; r++)
    for (int oy = -r; oy <= r; oy++)
      for (int ox = -r; ox <= r; ox++) {
        if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
        const int tx = (int)std::floor(probe.x / TILE) + ox, ty = (int)std::floor(probe.y / TILE) + oy;
        if (!m.in(tx, ty)) continue;
        const int pr = m.propAt(tx, ty);
        if (!pr) continue;
        for (int k = 0; k < n; k++) {
          if (want[k] != pr - 1) continue;
          if (r == 2 && (art::Prop)(pr - 1) != art::Prop::WatchtowerRuin) continue;
          otx = tx; oty = ty;
          return pr;
        }
      }
  return 0;
}

const char* wantedVerb(int prop) {
  switch ((art::Prop)prop) {
    case art::Prop::WatchtowerRuin: case art::Prop::Brazier: return "LIGHT";
    case art::Prop::Chest: return "OPEN";
    case art::Prop::Well: return "DRAW";
    case art::Prop::Crate: case art::Prop::Barrel: case art::Prop::Sacks: return "SEARCH";
    default: return "USE";
  }
}

float burdenMul(const Game& g) { return g.story.burdened() ? 0.7f : 1.0f; }

bool beaconLit(const Game& g, int tx, int ty) {
  const uint64_t k = storyMarkKey(((uint64_t)(uint32_t)(g.world.ox + tx) << 32) | (uint32_t)(g.world.oy + ty), MK_P1B_BEACON);
  auto it = g.marks.find(k);
  return it != g.marks.end() && g.day - it->second <= 2;
}

}  // namespace story

namespace story {

void syncNow(Game& g) {
  if (!host().spawnHuman) return;   // (the host table is installed by the first story hook of the game)
  syncPersons(g);
  engineSync(g);
}

bool debugBout(Game& g, uint32_t inst, bool win) {
  engineSync(g);
  Engine& E = g.story;
  for (Engine::Bout& b : E.bouts_) {
    if (b.inst != inst) continue;
    int ai = b.actor >= 0 ? findA(g, b.actor) : -1;
    if (ai < 0) return false;
    Actor& a = g.actors[(size_t)ai];
    // face to face, a stride apart
    const Actor& p = g.pl();
    const Vec2 at = p.p + p.aim * 20.0f;
    if (ClimbOps::bodyFree(g, at, a.radius)) a.p = at;
    a.face = faceOf(p.p - a.p);
    a.aim = norm(p.p - a.p);
    if (b.phase == 0) { b.phase = 1; b.t = 0; armForBout(g, a); b.hp0 = a.maxHp; E.parts_[a.id] = Engine::P_BOUT; }
    if (b.phase != 1) return false;
    if (win) {
      a.hp = a.maxHp * 0.3f;
      float dmg = a.maxHp * 0.2f;
      boutBlow(g, a, dmg, g.pl().id);
      a.hp -= dmg;
    } else {
      Actor& pl = g.pl();
      pl.hp = std::min(pl.hp, 3.0f);
      float dmg = 10.0f;
      boutBlow(g, pl, dmg, a.id);
      pl.hp -= dmg;
    }
    return true;
  }
  return false;
}

bool debugEscort(Game& g, uint32_t inst) {
  engineSync(g);
  Engine& E = g.story;
  bool any = false;
  for (Engine::Walker& w : E.walkers_) {
    if (w.inst != inst) continue;
    int ai = w.actor >= 0 ? findA(g, w.actor) : -1;
    if (ai < 0) { syncPersons(g); engineSync(g); ai = w.actor >= 0 ? findA(g, w.actor) : -1; }
    if (ai < 0) continue;
    int tx = 0, ty = 0;
    if (!besidePlayer(g, 2.0f * TILE, tx, ty, (uint32_t)w.actor)) continue;
    Actor& a = g.actors[(size_t)ai];
    a.p = Vec2(tx * TILE + 8.0f, ty * TILE + 10.0f);
    a.navNext = -1;
    a.face = g.pl().face;
    w.cartSet = false; w.spd = 0; w.stuckT = 0;
    w.lastX = a.p.x; w.lastY = a.p.y;
    any = true;
    if (w.skin == dsl::ESCORT_DROVE)
      for (Actor& b : g.actors)
        if (isDroveBeast(b) && (uint32_t)((b.slot - DROVE_SLOT0) / DROVE_MAX) == inst % 1024u) {
          const int k = (b.slot - DROVE_SLOT0) % DROVE_MAX;
          b.p = ClimbOps::freeSpot(g, tx + (k % 3) - 1, ty + 1 + k / 3);
          b.navNext = -1;
        }
  }
  return any;
}

}  // namespace story

namespace story {

bool useWantedProp(Game& g, int prop, int tx, int ty) {
  const art::Prop p = (art::Prop)prop;
  if (p != art::Prop::WatchtowerRuin && p != art::Prop::Crate && p != art::Prop::Barrel && p != art::Prop::Chest && p != art::Prop::Well &&
      p != art::Prop::Brazier && p != art::Prop::Sacks)
    return false;
  int wx = 0, wy = 0;
  if (wantedProp(g, wx, wy) != prop + 1 || wx != tx || wy != ty) return false;
  const Vec2 at(tx * TILE + 8.0f, ty * TILE + 8.0f);
  const char* what = p == art::Prop::WatchtowerRuin ? "THE BEACON IS LIT." : p == art::Prop::Well ? "YOU DRAW UP THE BUCKET." :
                     p == art::Prop::Brazier ? "THE BRAZIER CATCHES." : p == art::Prop::Chest ? "YOU OPEN THE CHEST." :
                     p == art::Prop::Sacks ? "YOU SEARCH THE SACKS." : p == art::Prop::Barrel ? "YOU SEARCH THE BARREL." : "YOU PRY OPEN THE CRATE.";
  auto ev = [&](Ev t, Vec2 q, int a, const std::string& s) { Event e; e.type = t; e.p = q; e.a = a; e.f = 0; e.s = s; g.events.push_back(e); };
  if (p == art::Prop::WatchtowerRuin && !g.inside) {
    g.marks[storyMarkKey(((uint64_t)(uint32_t)(g.world.ox + tx) << 32) | (uint32_t)(g.world.oy + ty), MK_P1B_BEACON)] = g.day;
    ev(Ev::Sparkle, at + Vec2(0, -40), 0, std::string());
    if (host().sound) host().sound(g, (int)Sfx::Fireball, 0.7f, 0.8f);
  } else if (host().sound) host().sound(g, (int)Sfx::Door, 1.25f, 0.7f);
  ev(Ev::Text, at + Vec2(0, -18), (int)rgba(255, 220, 140), wantedVerb(prop) == std::string("LIGHT") ? "LIT" : "DONE");
  if (host().say) host().say(g, what);
  g.story.onUseProp(g, prop, tx, ty);
  return true;
}

}  // namespace story

namespace story {

// (P1b fixer) the panel's title for a story's person: the script's `as <title>`, else a trade word their part names
std::string Engine::personTitle(const Actor& a) const {
  if (!story::isStoryPerson(a)) return std::string();
  for (const story::Instance& in : running_) {
    const story::dsl::Script* s = story::scriptOf(in);
    if (!s) continue;
    for (size_t ri = 0; ri < s->roles.size(); ri++) {
      const story::dsl::RoleDef& R = s->roles[ri];
      if (R.kind != story::dsl::RoleKind::Person || story::personSlot(in.id, (int)ri) != a.slot) continue;
      if (!R.title.empty()) return R.title;
      std::string w = R.name;
      if (w.size() > 4 && w.compare(0, 2, "tw") == 0) w = w.substr(2);   // a twist's people: "twreeve"
      // the parts whose name is a calling (never a relation, a plot word or a nickname: "rival", "ghost", "fox")
      static const std::map<std::string, const char*> kCalling = {
          {"carter", "CARTER"}, {"driver", "CARTER"}, {"courier", "COURIER"}, {"msgr", "MESSENGER"}, {"dyer", "DYER"},
          {"ropemaker", "ROPEMAKER"}, {"skipper", "SKIPPER"}, {"mate", "SHIP'S MATE"}, {"captain", "CAPTAIN"},
          {"capt", "CAPTAIN"}, {"sailor", "SAILOR"}, {"baker", "BAKER"}, {"miller", "MILLER"}, {"clerk", "CLERK"},
          {"reeve", "REEVE"}, {"trader", "TRADER"}, {"weigher", "WEIGHER"}, {"eggwife", "EGG-WIFE"}, {"maid", "MAID"},
          {"warden", "WARDEN"}, {"salter", "SALTER"}, {"spinner", "SPINNER"}, {"alewife", "ALEWIFE"}, {"cook", "COOK"},
          {"scribe", "SCRIBE"}, {"steward", "STEWARD"}, {"sergeant", "SERGEANT"}, {"sarge", "SERGEANT"},
          {"marshal", "MARSHAL"}, {"knight", "KNIGHT"}, {"healer", "HEALER"}, {"nurse", "NURSE"}, {"midwife", "MIDWIFE"},
          {"pedlar", "PEDLAR"}, {"peddler", "PEDLAR"}, {"tinker", "TINKER"}, {"drover", "DROVER"},
          {"goatherd", "GOATHERD"}, {"miner", "MINER"}, {"hunter", "HUNTER"}, {"huntress", "HUNTRESS"},
          {"huntsman", "HUNTSMAN"}, {"trapper", "TRAPPER"}, {"piper", "PIPER"}, {"jester", "JESTER"},
          {"jailer", "JAILER"}, {"lookout", "LOOKOUT"}, {"foreman", "FOREMAN"}, {"priest", "PRIEST"},
          {"abbess", "ABBESS"}, {"hermit", "HERMIT"}, {"scholar", "SCHOLAR"}, {"chancellor", "CHANCELLOR"},
          {"envoy", "ENVOY"}, {"squire", "SQUIRE"}, {"servant", "SERVANT"}, {"elder", "ELDER"}, {"seer", "SEER"},
          {"pilgrim", "PILGRIM"}, {"recruiter", "RECRUITER"}, {"vsmith", "SMITH"}, {"smith", "SMITH"},
          {"gleaner", "GLEANER"}, {"prophet", "PROPHET"}, {"vet", "VETERAN"}, {"wayfarer", "TRAVELLER"},
          {"scout", "SCOUT"}, {"guide", "GUIDE"}};
      auto it = kCalling.find(w);
      return it != kCalling.end() ? std::string(it->second) : std::string();
    }
  }
  return std::string();
}

}  // namespace story
