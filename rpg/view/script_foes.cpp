// M6 Steel FOES lane script commands: stage named uniques, world bosses, raids, boss phases and culture arms lineups
// for screenshots and checks (tools/scripts/m6_foes_*.txt).
//   gotonamed [n]            teleport 14 tiles south of the n-th nearest (0) named unique still alive (it comes out of
//                            its lair within half a second)
//   gotoboss                 teleport 16 tiles from where this kingdom cell's world boss roams right now (it comes)
//   beastraid                the nearest settlement in the window is raided by the world boss of its kingdom cell: the
//                            realm's event, the charred overlay, and the news told to the player at once
//   bossphase <pct>          the nearest boss (a dungeon boss or a world boss) drops to pct % health (its phases fire)
//   arms4 [archetypes...]    a lineup of four cultures' men (default fjordfolk dune jade steppe): per culture a soldier, a
//                            captain (the culture's alloy) and a bandit (the poor local kit), set out round the player
//   expect foes named|worldboss|elite|champion|phase [n]: at least n (1) such foes in play (phase: a boss in phase >= n)
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "rpg/culture/culture.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/game.h"
#include "rpg/story/story.h"
#include "rpg/view/script_api.h"
#include "rpg/world/source.h"

namespace census {
struct Rank { bool royal = false, capital = false; };
void dress(art::HumanLook& L, std::string& name, Role r, bool female, const cult::Culture& C, const cult::Culture& owner,
           uint64_t seed, const Rank& rank);
}  // namespace census

namespace {

std::string low(std::string s) {
  for (char& ch : s) if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
  return s;
}
int floorDivI(int32_t a, int32_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
// where the player is on the overworld (inside: the door or entrance they went in by)
void playerGlobal(const Game& g, int32_t& gx, int32_t& gy) {
  if (g.inside && g.subBldg >= 0 && g.subBldg < (int)g.world.over.bldgs.size()) {
    const Bldg& b = g.world.over.bldgs[(size_t)g.subBldg];
    gx = g.world.ox + b.doorX(); gy = g.world.oy + b.doorY() + 1;
    return;
  }
  if (g.inside && g.subSite >= 0 && g.subSite < (int)g.world.sites.size()) {
    gx = g.world.ox + g.world.sites[(size_t)g.subSite].ex; gy = g.world.oy + g.world.sites[(size_t)g.subSite].ey;
    return;
  }
  gx = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE);
  gy = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
}
bool slain(const Game& g, ew::Gid id, uint64_t tag) {
  // gsim::markKey (rpg/sim/game_internal.h), restated: the view does not include the sim's internals
  const uint64_t k = ew::mix64(id ^ (tag * 0xD1B54A32D192ED03ull));
  return g.marks.count(k) != 0;
}

bool cmdGotoNamed(ScriptCtx& c) {
  Game& g = c.game;
  if (!g.world.src) { c.fail("gotonamed: no endless world"); return true; }
  const int nth = c.arg(1).empty() ? 0 : std::max(0, std::atoi(c.arg(1).c_str()));
  int32_t px, py;
  playerGlobal(g, px, py);
  std::vector<foes::NamedUnique> all;
  const int32_t rx = floorDivI(px, ew::REGION), ry = floorDivI(py, ew::REGION);
  for (int dy = -2; dy <= 2; dy++)
    for (int dx = -2; dx <= 2; dx++)
      for (const foes::NamedUnique& u : foes::namedInRegion(*g.world.src, rx + dx, ry + dy))
        if (!slain(g, u.id, foes::MK_FOES_NAMED_SLAIN)) all.push_back(u);
  if ((int)all.size() <= nth) { c.fail("gotonamed: no named unique alive nearby"); return true; }
  std::sort(all.begin(), all.end(), [&](const foes::NamedUnique& a, const foes::NamedUnique& b) {
    return (int64_t)(a.gx - px) * (a.gx - px) + (int64_t)(a.gy - py) * (a.gy - py) < (int64_t)(b.gx - px) * (b.gx - px) + (int64_t)(b.gy - py) * (b.gy - py);
  });
  const foes::NamedUnique& u = all[(size_t)nth];
  g.teleportGlobal(u.gx, u.gy + 14);
  std::printf("gotonamed: %s (%s, D %d) at %d,%d\n", u.name.c_str(), std::to_string((int)u.mon).c_str(), u.D, (int)u.gx, (int)u.gy);
  return true;
}
EMB_SCRIPT_CMD("gotonamed", "gotonamed [n]: teleport 14 tiles south of the n-th nearest named unique's lair (M6 FOES)", cmdGotoNamed);

bool cmdGotoBoss(ScriptCtx& c) {
  Game& g = c.game;
  if (!g.world.src) { c.fail("gotoboss: no endless world"); return true; }
  int32_t px, py;
  playerGlobal(g, px, py);
  bool ok = false;
  const foes::WorldBoss b = foes::worldBossOf(*g.world.src, ew::EndlessSource::kcellOf(px), ew::EndlessSource::kcellOf(py), ok);
  if (!ok) { c.fail("gotoboss: no world boss in this kingdom cell"); return true; }
  int32_t bx, by;
  foes::worldBossAt(b, g.day, g.hour, bx, by);
  g.teleportGlobal(bx, by + 16);
  std::printf("gotoboss: %s, the %s (D %d) at %d,%d\n", b.name.c_str(), b.kind.c_str(), b.D, (int)bx, (int)by);
  return true;
}
EMB_SCRIPT_CMD("gotoboss", "gotoboss: teleport 16 tiles from this kingdom cell's world boss (M6 FOES)", cmdGotoBoss);

bool cmdBeastRaid(ScriptCtx& c) {
  Game& g = c.game;
  if (!g.world.src) { c.fail("beastraid: no endless world"); return true; }
  int best = -1;
  float bd = 1e30f;
  for (int si = 0; si < (int)g.world.sites.size(); si++) {
    if (!g.world.sites[(size_t)si].settlement() || !g.world.nearWindow(g.world.sites[(size_t)si].ex, g.world.sites[(size_t)si].ey)) continue;
    const Site& s = g.world.sites[(size_t)si];
    const float d = std::hypot(s.ex * TILE - g.pl().p.x, s.ey * TILE - g.pl().p.y);
    if (d < bd) { bd = d; best = si; }
  }
  if (best < 0) { c.fail("beastraid: no settlement near"); return true; }
  const Site& s = g.world.sites[(size_t)best];
  const int32_t gx = g.world.ox + s.ex, gy = g.world.oy + s.ey;
  bool ok = false;
  const foes::WorldBoss b = foes::worldBossOf(*g.world.src, ew::EndlessSource::kcellOf(gx), ew::EndlessSource::kcellOf(gy), ok);
  if (!g.realm.settlement(s.id)) g.realm.noteSite(s.id, s.kingdom >= 0 ? g.world.kingdoms[(size_t)s.kingdom].id : 0, (uint8_t)s.type, gx, gy);
  g.realm.beastRaid(s.id, (uint8_t)(ok ? b.mon : art::Monster::Troll), 40, g.day);
  g.realmSync();
  const realm::WorldEvent& e = g.realm.events().back();
  Event ev;
  ev.type = Ev::News; ev.p = g.pl().p; ev.a = (int)e.id; ev.s = story::newsLine(g, e, 0);
  g.events.push_back(ev);
  g.realm.markHeard(e.id);
  std::printf("beastraid: %s\n", ev.s.c_str());
  return true;
}
EMB_SCRIPT_CMD("beastraid", "beastraid: the nearest settlement is raided by its kingdom cell's world boss; the news is told (M6 FOES)", cmdBeastRaid);

bool cmdBossPhase(ScriptCtx& c) {
  Game& g = c.game;
  const float pct = c.arg(1).empty() ? 60.0f : (float)std::atof(c.arg(1).c_str());
  Actor* best = nullptr;
  float bd = 1e30f;
  for (Actor& a : g.actors) {
    if (a.player || a.st == AState::Dead || !(a.boss || a.rank == (uint8_t)foes::Rank::WorldBoss)) continue;
    const float d = len2(a.p - g.pl().p);
    if (d < bd) { bd = d; best = &a; }
  }
  if (!best) { c.fail("bosshp: no boss in play"); return true; }
  best->hp = std::max(1.0f, best->maxHp * pct / 100.0f);
  return true;
}
EMB_SCRIPT_CMD("bosshp", "bosshp <pct>: the nearest boss drops to pct % health, so the sim passes its phase (M6 FOES)", cmdBossPhase);

bool cmdNearBoss(ScriptCtx& c) {
  Game& g = c.game;
  Actor* best = nullptr;
  float bd = 1e30f;
  for (Actor& a : g.actors) {
    if (a.player || a.st == AState::Dead || !(a.boss || a.rank == (uint8_t)foes::Rank::WorldBoss || a.rank == (uint8_t)foes::Rank::Named)) continue;
    const float d = len2(a.p - g.pl().p);
    if (d < bd) { bd = d; best = &a; }
  }
  if (!best) { c.fail("nearboss: no boss in play"); return true; }
  const Vec2 offs[] = {{0, 44}, {0, -44}, {44, 0}, {-44, 0}, {0, 28}, {28, 0}, {-28, 0}, {0, -28}, {32, 32}, {-32, 32}};
  Vec2 at = best->p + offs[0];
  for (const Vec2& o : offs) {
    const Vec2 q = best->p + o;
    if (!g.map().blocked((int)std::floor(q.x / TILE), (int)std::floor((q.y - 2) / TILE))) { at = q; break; }
  }
  g.pl().p = at;
  best->aggro = true;
  return true;
}
EMB_SCRIPT_CMD("nearboss", "nearboss: step up to the nearest boss, world boss or named unique (M6 FOES)", cmdNearBoss);

int archOf(const std::string& w) {
  auto key = [](std::string s) {   // lower case, no spaces, dashes or underscores ("sun temple" == "suntemple")
    std::string k;
    for (char ch : low(s)) if (ch != ' ' && ch != '-' && ch != '_') k += ch;
    return k;
  };
  for (int a = 0; a < (int)cult::Archetype::COUNT; a++)
    if (key(cult::archetypeName((cult::Archetype)a)) == key(w)) return a;
  return -1;
}

bool cmdArms4(ScriptCtx& c) {
  Game& g = c.game;
  std::vector<int> arch;
  for (size_t i = 1; i < c.a.size(); i++) { const int a = archOf(c.a[i]); if (a >= 0) arch.push_back(a); }
  if (arch.empty()) arch = {(int)cult::Archetype::Fjordfolk, (int)cult::Archetype::Dune, (int)cult::Archetype::Jade, (int)cult::Archetype::Steppe};
  // a fresh lineup replaces the last one (its men carry ids from 900000: the game's own never get there)
  g.actors.erase(std::remove_if(g.actors.begin() + 1, g.actors.end(), [](const Actor& a) { return a.id >= 900000; }), g.actors.end());
  int id = 900000;
  const int cols = (int)arch.size();
  // a clear patch of ground for the line (no trees or rocks in front of anyone): the nearest within 40 tiles
  Vec2 centre = g.pl().p;
  if (!g.inside) {
    const Map& m = g.map();
    const int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
    bool found = false;
    for (int r = 0; r <= 40 && !found; r++)
      for (int dy = -r; dy <= r && !found; dy++)
        for (int dx = -r; dx <= r && !found; dx++) {
          if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
          const int cx = px + dx, cy = py + dy;
          bool clear = true;
          // (trees two rows below the line still reach up into it with their crowns)
          for (int y = cy - 4; y <= cy + 5 && clear; y++)
            for (int x = cx - 6; x <= cx + 6 && clear; x++) {
              if (!m.in(x, y) || m.blocked(x, y)) { clear = false; break; }
              const int pr = m.propAt(x, y);
              if (pr && art::isTreeProp((art::Prop)(pr - 1))) clear = false;
            }
          if (clear) { found = true; centre = Vec2(cx * TILE + 8.0f, cy * TILE + 10.0f); }
        }
    g.pl().p = centre + Vec2(0, 44.0f);   // the hero stands before the line, the camera on both (the line clear of the place banner)
  }
  static const uint32_t rags[] = {rgba(96, 82, 64), rgba(84, 92, 70), rgba(110, 80, 66), rgba(72, 70, 78)};
  for (int k = 0; k < cols; k++) {
    const cult::Culture C = cult::Atlas::make((cult::Archetype)arch[(size_t)k], 4242u + (uint32_t)arch[(size_t)k] * 97u);
    for (int row = 0; row < 3; row++) {
      const Role r = row == 0 ? Role::Soldier : row == 1 ? Role::Captain : Role::Bandit;
      Actor a;
      a.id = id++;
      a.human = true; a.npc = true; a.role = r; a.faction = Faction::Army; a.hostile = false;
      a.p = centre + Vec2((float)(k - (cols - 1) * 0.5f) * 34.0f, -30.0f + (float)row * 30.0f);
      a.home = a.p; a.goal = a.p; a.face = 0; a.speed = 0; a.special = 0.01f;
      art::HumanLook& L = a.look;
      const bool female = false;
      if (r == Role::Soldier) { L.outfit = art::Outfit::Guard; L.helmet = true; L.shield = true; L.weapon = 8; }
      else if (r == Role::Captain) { L.outfit = art::Outfit::Plate; L.helmet = true; L.shield = true; L.weapon = 1; L.shieldStyle = 2; L.cloak = 1; }
      else {
        L.outfit = art::Outfit::Leather; L.weapon = (k % 2) ? 3 : 1; L.tabardColor = rgba(80, 60, 50);
        L.topColor = rags[k % 4]; L.bottomColor = rags[(k + 2) % 4];
      }
      if (r != Role::Bandit) {
        L.tabardColor = C.heraldry.field ? (C.heraldry.field | 0xFF000000u) : rgba(150, 40, 40);
        L.trimColor = C.heraldry.charge ? (C.heraldry.charge | 0xFF000000u) : 0;
        L.cloakColor = L.tabardColor;
      }
      census::dress(L, a.name, r, female, C, C, 7000u + (uint64_t)k * 31u + (uint64_t)row, census::Rank{});
      a.name = std::string(cult::archetypeName(C.archetype)) + (r == Role::Soldier ? " SOLDIER" : r == Role::Captain ? " CAPTAIN" : " BANDIT");
      a.role = Role::Soldier;   // (they stand in line: the soldiers' AI holds them at their post)
      g.actors.push_back(a);
    }
  }
  return true;
}
EMB_SCRIPT_CMD("arms4", "arms4 [archetypes...]: four cultures' soldier, captain and bandit lined up round the player (M6 FOES)", cmdArms4);

bool expFoes(ScriptCtx& c) {
  const Game& g = c.game;
  const std::string what = low(c.arg(2));
  const int want = c.arg(3).empty() ? 1 : std::atoi(c.arg(3).c_str());
  int n = 0;
  for (const Actor& a : g.actors) {
    if (a.player || a.st == AState::Dead) continue;
    if (what == "named") n += a.rank == (uint8_t)foes::Rank::Named;
    else if (what == "worldboss") n += a.rank == (uint8_t)foes::Rank::WorldBoss;
    else if (what == "elite") n += a.rank == (uint8_t)foes::Rank::Elite;
    else if (what == "champion") n += a.rank == (uint8_t)foes::Rank::Champion;
    else if (what == "phase") n += (a.boss || a.rank == (uint8_t)foes::Rank::WorldBoss) && a.bossPhase >= want;
  }
  if (what == "phase" ? n < 1 : n < want) c.fail("expect foes " + what + ": " + std::to_string(n) + " in play");
  return true;
}
EMB_SCRIPT_CMD("expect:foes", "expect foes named|worldboss|elite|champion|phase [n]: ranked foes in play (M6 FOES)", expFoes);

}  // namespace
