// SOLID 1.3 STORY: the people and props of the running stories in the world: syncPersons (spawn / put away),
// dressPerson, placeInsiders, placeWantedProps and the wanted-prop words, engineSync (walkers, bouts, droves) and
// Engine::personTitle. Moved from story_game.cpp (the hooks); the walkers' steps are story_ai.cpp.
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
#include "rpg/story/story_detail.h"
#include "rpg/world/coords.h"
#include "rpg/world/source.h"
#include "rpg/sim/stream.h"

namespace {
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
}  // namespace

namespace storyg {

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
}  // namespace storyg

namespace {

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
}  // namespace

namespace storyg {

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
}  // namespace storyg

namespace {
int actorBySlot(const Game& g, int slot, bool beast = false) {
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    if (a.slot == slot && (beast ? story::isDroveBeast(a) : story::isStoryPerson(a))) return (int)k;
  }
  return -1;
}
}  // namespace

namespace storyg {
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
}  // namespace storyg

namespace story {

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

bool beaconLit(const Game& g, int tx, int ty) {
  const uint64_t k = storyMarkKey(((uint64_t)(uint32_t)(g.world.ox + tx) << 32) | (uint32_t)(g.world.oy + ty), MK_P1B_BEACON);
  auto it = g.marks.find(k);
  return it != g.marks.end() && g.day - it->second <= 2;
}

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
