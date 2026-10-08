// rpg_test --wards [--seeds A..B]: M4 "Banners", the war made visible and the kingdoms' guards (rpg/sim/war.h). WARDS lane.
// Per seed, in the endless world round the start:
//   guards    a member village keeps a watch of 2-3 in its owner's tabard colour, serving it (Actor::realm); guard counts
//             grow city > town > village; a frontier village (no kingdom) has no guards and at least 2 militia
//   conquest  after forceOwner the old garrison stands down and the next guards wear the new owner's colours
//   raids     a wolf pack loosed in a member village is killed by its watch within 30 s with at most 1 villager down; in a
//             frontier village the bell rings, the militia take up arms and the rest hide
//   burned    forceBurn: >= 50 % of the buildings draw charred, the burned-out ones refuse entry and spawn nobody, a
//             refugee camp (lean-tos, refugees) stands outside the friendly town
//   siege     forceSiege: the camp's war props lie 12-36 tiles outside the walls (never on roads, water or buildings, with
//             a way out), attacker soldiers and their captain turn out, warHostile is true across the war and false for
//             allies, the player's kills on a BREAK THE SIEGE quest raise the siege's def by 0.5 each
//   patrols   a road patrol of the land's kingdom appears near a player standing on its road within 3 minutes
//   costs     warStep's average and worst cost, an overlay apply per settlement (targets 0.3 ms and 0.2 ms: reported)
#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>
#include "rpg/sim/game_internal.h"
#include "rpg/sim/war.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

void tick(Game& g, int frames) {
  for (int f = 0; f < frames; f++) {
    g.update(SIM_DT, Input());
    g.events.clear();
    if (g.mode == Mode::Dialogue || g.mode == Mode::Shop || g.mode == Mode::LevelUp) g.mode = Mode::Play;
    if (g.mode == Mode::Dead) g.respawn();
  }
}
Vec2 tileC(int x, int y) { return Vec2(x * TILE + 8.0f, y * TILE + 10.0f); }
ew::Gid kidOf(const Game& g, int kh) { return kh >= 0 ? g.world.kingdoms[(size_t)kh].id : 0; }
int rectDist(const IRect& r, int x, int y) {
  const int dx = std::max({r.x - x, x - (r.x + r.w - 1), 0}), dy = std::max({r.y - y, y - (r.y + r.h - 1), 0});
  return std::max(dx, dy);
}
// the player on a free tile near (tx, ty) (window tiles), the window following
void standAt(Game& g, int tx, int ty) {
  g.teleportGlobal(g.world.ox + tx, g.world.oy + ty);
  tick(g, 3);
  if (g.inside) { g.debugLeave(); g.pl().p.y += 20; tick(g, 2); }   // (a teleport onto a doorstep walks in)
}
// bring a settlement into the window's middle and stand the player at its heart
int visit(Game& g, ew::Gid siteId) {
  int si = g.world.siteHandle(siteId);
  if (si < 0) return -1;
  standAt(g, g.world.sites[(size_t)si].ex, g.world.sites[(size_t)si].ey + 1);
  si = g.world.siteHandle(siteId);
  tick(g, 40);   // the site activates, its people stream in
  return si;
}
struct Count { int guards = 0, guardsOk = 0, folk = 0, militia = 0; };
Count countPeople(const Game& g, int si) {
  Count c;
  const Site& s = g.world.sites[(size_t)si];
  const ew::Gid owner = kidOf(g, s.kingdom);
  const uint32_t col = s.kingdom >= 0 ? g.world.kingdoms[(size_t)s.kingdom].tabard() : 0;
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    if (!a.npc || a.site != si || a.st == AState::Dead) continue;
    if (a.role == Role::Guard) {
      c.guards++;
      if (a.realm == owner && (a.look.tabardColor | 0xFF000000u) == (col | 0xFF000000u)) c.guardsOk++;
    } else if (a.human) {
      c.folk++;
      if (a.militia) c.militia++;
    }
  }
  return c;
}
// a neighbouring kingdom of the realm (not k)
ew::Gid otherKingdom(const Game& g, ew::Gid k) {
  for (const realm::KingdomState& K : g.realm.kingdoms())
    if (K.id != k && !K.fallen && g.world.kingdomById.count(K.id)) return K.id;
  for (const Kingdom& K : g.world.kingdoms) if (K.id != k) return K.id;
  return 0;
}
// the nearest loaded settlement of a type that a kingdom holds (exclude: skip it)
int nearestMember(const Game& g, SiteType t, bool member, int exclude = -1) {
  const Site& h = g.world.sites[(size_t)g.world.startSite];
  int best = -1;
  float bd = 1e30f;
  for (int i = 0; i < (int)g.world.sites.size(); i++) {
    const Site& s = g.world.sites[(size_t)i];
    if (s.type != t || i == exclude || (s.kingdom >= 0) != member) continue;
    const float d = std::hypot((float)(s.ex - h.ex), (float)(s.ey - h.ey));
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}

// a wolf pack loosed among the townsfolk; returns failures
int raid(const Game& base, ew::Gid siteId, bool guarded, const char* what, uint64_t seed) {
  int bad = 0;
  Game g = base;
  g.godMode = true; g.noWildSpawns = true; g.hour = 12;
  int si = visit(g, siteId);
  if (si < 0) return 0;
  // stand off at the edge, so the beasts pick on the townsfolk
  const Site s0 = g.world.sites[(size_t)si];
  standAt(g, s0.r.x + s0.r.w + 24, s0.ey);
  si = g.world.siteHandle(siteId);
  tick(g, 60);
  const Site s = g.world.sites[(size_t)si];
  Count c = countPeople(g, si);
  if (c.folk == 0) { out("WARN: wards seed %llu raid (%s): nobody out in %s\n", (unsigned long long)seed, what, s.name.c_str()); return 0; }
  Vec2 mid((s.r.x + s.r.w * 0.5f) * TILE, (s.r.y + s.r.h * 0.5f) * TILE);
  Vec2 at = mid;
  float bd = 1e30f;
  for (const Actor& a : g.actors)
    if (a.npc && a.site == si && a.role != Role::Guard && !a.stallKeeper && len2(a.p - mid) < bd) { bd = len2(a.p - mid); at = a.p; }
  {   // in the open street beside them (never inside a stall or a fence)
    const int tx = (int)std::floor(at.x / TILE), ty = (int)std::floor((at.y - 2) / TILE);
    bool found = false;
    for (int r = 0; r < 6 && !found; r++)
      for (int dy = -r; dy <= r && !found; dy++)
        for (int dx = -r; dx <= r && !found; dx++) {
          const int x = tx + dx, y = ty + dy;
          bool ok = true;
          for (int k = -1; k <= 1 && ok; k++) for (int j = -1; j <= 1 && ok; j++) if (g.world.over.blocked(x + j, y + k)) ok = false;
          if (ok) { at = tileC(x, y); found = true; }
        }
  }
  const int lvl = std::max(1, g.world.zoneLevel(s.ex, s.ey));
  std::vector<int> wolves;
  for (int k = 0; k < 3; k++) wolves.push_back(g.debugSpawnAt(art::Monster::Wolf, at + Vec2((k - 1) * 10.0f, -4.0f), lvl));
  std::set<int> downed;
  bool rang = false, armed = false;
  int maxHidden = 0;
  float cleared = -1;
  for (int f = 0; f < 30 * 60; f++) {
    g.update(SIM_DT, Input());
    g.events.clear();
    if (g.mode != Mode::Play) g.mode = Mode::Play;
    if (g.alarmSite == si) rang = true;
    maxHidden = std::max(maxHidden, g.shelteredCount(si));
    int alive = 0;
    for (int id : wolves) for (const Actor& a : g.actors) if (a.id == id && a.st != AState::Dead) alive++;
    for (const Actor& a : g.actors) {
      if (!a.npc || a.site != si) continue;
      if (a.st == AState::Dead && a.role != Role::Guard) downed.insert(a.id);
      if (a.militia && a.target >= 0) armed = true;
    }
    if (!alive) { cleared = f / 60.0f; break; }
    if (getenv("EMB_WARDS_TRACE") && f % 300 == 0) {
      std::string line = "  t=" + std::to_string(f / 60) + ":";
      for (const Actor& a : g.actors) {
        if (a.st == AState::Dead) continue;
        const bool wolf = std::find(wolves.begin(), wolves.end(), a.id) != wolves.end();
        if (!wolf && !(a.npc && a.site == si && (a.role == Role::Guard || a.militia))) continue;
        char b[96];
        const int qx = (int)std::floor(a.p.x / TILE), qy = (int)std::floor((a.p.y - 2) / TILE);
        std::snprintf(b, sizeof b, " %s(%.1f,%.1f t%d hp%.0f s%d b%d k%d)", wolf ? "W" : (a.role == Role::Guard ? "G" : "M"), a.p.x / TILE, a.p.y / TILE, a.target, a.hp, (int)a.st, g.world.over.bldgAt[(size_t)qy * g.world.over.w + qx], (int)g.world.over.blocked(qx, qy));
        line += b;
      }
      fprintf(stderr, "%s\n", line.c_str());
    }
  }
  out("wards seed %llu raid (%s) %s: %d guards, %d folk (%d militia): bell %s, wolves dead at %.1f s, %zu down, %d hid, militia %s\n",
      (unsigned long long)seed, what, s.name.c_str(), c.guards, c.folk, c.militia, rang ? "rang" : "silent", cleared, downed.size(), maxHidden,
      armed ? "fought" : "idle");
  if (!rang) { out("FAIL: wards seed %llu raid (%s): 3 wolves in %s rang no bell\n", (unsigned long long)seed, what, s.name.c_str()); bad++; }
  if (guarded && cleared < 0) { out("FAIL: wards seed %llu raid (%s): the watch of %s did not kill 3 wolves in 30 s\n", (unsigned long long)seed, what, s.name.c_str()); bad++; }
  if (guarded && downed.size() > 1) { out("FAIL: wards seed %llu raid (%s): %zu villagers down in %s\n", (unsigned long long)seed, what, downed.size(), s.name.c_str()); bad++; }
  if (!guarded && !armed && c.militia > 0) { out("FAIL: wards seed %llu raid (%s): no militia fought in %s\n", (unsigned long long)seed, what, s.name.c_str()); bad++; }
  if (maxHidden == 0 && c.folk - c.militia > 0) { out("FAIL: wards seed %llu raid (%s): nobody in %s hid\n", (unsigned long long)seed, what, s.name.c_str()); bad++; }
  return bad;
}

int wardsSeed(uint64_t seed) {
  int bad = 0;
  auto fail = [&](const std::string& what) { out("FAIL: wards seed %llu: %s\n", (unsigned long long)seed, what.c_str()); bad++; };
  Game base(seed);
  base.newEndlessGame(seed);
  base.mode = Mode::Play;
  base.godMode = true;
  base.noWildSpawns = true;
  tick(base, 30);
  for (int k = 0; k < 3000 && base.realm.focusPending(); k++) tick(base, 1);
  // the places: member villages, a town, a city round the start (loading their regions)
  const Site home = base.world.sites[(size_t)base.world.startSite];
  base.world.findSiteNear(base.world.ox + home.ex, base.world.oy + home.ey, SiteType::Town, 6);
  base.world.findSiteNear(base.world.ox + home.ex, base.world.oy + home.ey, SiteType::City, 8);
  const int vi = nearestMember(base, SiteType::Village, true);
  const int ti = nearestMember(base, SiteType::Town, true);
  const int ci = nearestMember(base, SiteType::City, true);
  if (getenv("EMB_WARDS_TRACE")) { fprintf(stderr, "wards trace line %d\n", __LINE__); fflush(stderr); }
  // ---- guard counts: city > town > village (VISION_PLAN 10.4), frontier 0
  if (vi >= 0 && ti >= 0 && ci >= 0) {
    const int nv = warGuardsWanted(base, vi), nt = warGuardsWanted(base, ti), nc = warGuardsWanted(base, ci);
    if (!(nc > nt && nt > nv)) fail("guard counts not city > town > village (" + std::to_string(nc) + ", " + std::to_string(nt) + ", " + std::to_string(nv) + ")");
    if (nv < 2 || nv > 5) fail("a member village's watch is not 2-3 (+2 at a capital): " + std::to_string(nv));
  }
  if (getenv("EMB_WARDS_TRACE")) { fprintf(stderr, "wards trace line %d\n", __LINE__); fflush(stderr); }
  // ---- a member village's watch in its owner's colours
  if (vi >= 0) {
    Game g = base;
    const ew::Gid vid = g.world.sites[(size_t)vi].id;
    const int si = visit(g, vid);
    const Count c = countPeople(g, si);
    const int want = warGuardsWanted(g, si);
    if (c.guards != want) fail("member village " + g.world.sites[(size_t)si].name + ": " + std::to_string(c.guards) + " guards out, want " + std::to_string(want));
    if (c.guardsOk != c.guards) fail("a member village guard does not wear or serve its owner");
    if (getenv("EMB_WARDS_TRACE")) { fprintf(stderr, "wards trace line %d\n", __LINE__); fflush(stderr); }
  // ---- conquest: the next guards wear the new owner's colours, the old garrison is gone
    const ew::Gid owner = kidOf(g, g.world.sites[(size_t)si].kingdom), enemy = otherKingdom(g, owner);
    if (enemy) {
      g.realm.forceOwner(vid, enemy, g.day);
      g.realmSync();
      tick(g, 60);
      const int s2 = g.world.siteHandle(vid);
      const Count c2 = countPeople(g, s2);
      int old = 0;
      for (const Actor& a : g.actors) if (a.npc && a.site == s2 && a.role == Role::Guard && a.realm == owner && a.st != AState::Dead) old++;
      if (old) fail("conquest: " + std::to_string(old) + " guards of the old owner still on the streets");
      if (c2.guards == 0) fail("conquest: no guards after the owner changed");
      if (c2.guardsOk != c2.guards) fail("conquest: a guard does not wear the new owner's colours");
      out("wards seed %llu: %s watch %d (want %d) in its colours; taken: %d guards in the new colours\n", (unsigned long long)seed,
          g.world.sites[(size_t)s2].name.c_str(), c.guards, want, c2.guards);
    }
    bad += raid(base, vid, true, "member village", seed);
  }
  if (getenv("EMB_WARDS_TRACE")) { fprintf(stderr, "wards trace line %d\n", __LINE__); fflush(stderr); }
  // ---- a frontier village: militia only. The nearest wildlands village, else a member village the realm lets go.
  {
    Game g = base;
    int fi = nearestMember(g, SiteType::Village, false);
    ew::Gid fid = fi >= 0 ? g.world.sites[(size_t)fi].id : 0;
    if (!fid && vi >= 0) {
      fid = g.world.sites[(size_t)vi].id;
      g.realm.forceOwner(fid, 0, g.day);
      g.realmSync();
    }
    if (fid) {
      const int si = visit(g, fid);
      if (si >= 0) {
        if (!warFrontier(g, si)) fail("a village no kingdom holds is not frontier");
        const Count c = countPeople(g, si);
        if (c.guards) fail("frontier village " + g.world.sites[(size_t)si].name + " has " + std::to_string(c.guards) + " guards");
        if (c.militia < 2 && c.folk >= 4) fail("frontier village " + g.world.sites[(size_t)si].name + " has " + std::to_string(c.militia) + " militia (want >= 2)");
        out("wards seed %llu: frontier %s: %d guards, %d militia of %d folk\n", (unsigned long long)seed, g.world.sites[(size_t)si].name.c_str(), c.guards,
            c.militia, c.folk);
        Game r = g;
        bad += raid(r, fid, false, "frontier village", seed);
      }
    }
  }
  if (getenv("EMB_WARDS_TRACE")) { fprintf(stderr, "wards trace line %d\n", __LINE__); fflush(stderr); }
  // ---- burned: charred buildings, barred doors, nobody home in them, the refugee camp at the friendly town
  if (vi >= 0) {
    Game g = base;
    const ew::Gid vid = g.world.sites[(size_t)vi].id;
    int si = visit(g, vid);
    g.realm.forceBurn(vid, g.day);
    g.realmSync();
    tick(g, 60);
    si = g.world.siteHandle(vid);
    const Site s = g.world.sites[(size_t)si];   // (a copy: visiting the refuge streams more sites in)
    int n = 0, ch = 0, out2 = 0, barredBi = -1;
    for (int i = 0; i < s.bldgCount; i++) {
      const Bldg& b = g.world.over.bldgs[(size_t)(s.bldgFirst + i)];
      if (b.site != si) continue;
      n++;
      if (b.charred) ch++;
      if (b.charred == 2) { out2++; if (barredBi < 0) barredBi = s.bldgFirst + i; }
    }
    if (n && ch * 2 < n) fail("burned " + s.name + ": " + std::to_string(ch) + "/" + std::to_string(n) + " buildings charred (want >= 50%)");
    if (!out2) fail("burned " + s.name + ": no building burned out");
    if (barredBi >= 0) {
      const Bldg b = g.world.over.bldgs[(size_t)barredBi];
      g.pl().p = Vec2(b.doorX() * 16 + 8.0f, (b.doorY() + 1) * 16 + 6.0f);
      Input up; up.move = Vec2(0, -1);
      for (int f = 0; f < 30 && !g.inside; f++) { g.update(SIM_DT, up); g.events.clear(); }
      if (g.inside) { fail("a burned-out building let the player in"); g.debugLeave(); }
      // nobody lives in it now
      for (const Actor& a : g.actors)
        if (a.npc && a.site == si && a.fromMap && a.role != Role::Guard) {
          const int hx = (int)std::floor(a.home.x / TILE), hy = (int)std::floor(a.home.y / TILE);
          if (hx >= b.r.x && hx < b.r.x + b.r.w && hy >= b.r.y - 1 && hy <= b.r.y + b.r.h) { fail("someone came out of a burned-out home"); break; }
        }
    }
    // the refugees' camp outside the nearest friendly settlement
    ew::Gid refuge = 0;
    for (const Site& o : g.world.sites)
      if (o.settlement()) if (const realm::SettlementState* st = g.realm.settlement(o.id)) if ((st->flags & realm::SS_REFUGEES) && st->refugeesFrom == vid) refuge = o.id;
    if (!refuge) fail("burned " + s.name + ": no settlement took in its refugees");
    else {
      const int ri = visit(g, refuge);
      tick(g, 30);
      const WarCamp* camp = warRefugeCampOf(g, refuge);
      if (!camp && ri >= 0) {
        // a big city: its camp may lie outside the window seen from its heart; go to the spot the overlay keeps for it
        const Site R = g.world.sites[(size_t)ri];
        int32_t bx = 0, by = 0;
        int64_t bd = INT64_MAX;
        for (const auto& kv : g.war.spots) {
          const int64_t dx = kv.second.first - (g.world.ox + R.ex), dy = kv.second.second - (g.world.oy + R.ey), d = dx * dx + dy * dy;
          if (d < bd) { bd = d; bx = kv.second.first; by = kv.second.second; }
        }
        if (bd < INT64_MAX) { standAt(g, bx - g.world.ox + 3, by - g.world.oy + 4); tick(g, 30); camp = warRefugeCampOf(g, refuge); }
      }
      if (!camp) {
        fail("no refugee camp outside " + (ri >= 0 ? g.world.sites[(size_t)ri].name : std::string("the refuge")));
        if (getenv("EMB_WARDS_TRACE") && ri >= 0) {
          const Site& R = g.world.sites[(size_t)ri];
          fprintf(stderr, "  refuge %s rect %d,%d %dx%d, camps %zu, overlays %zu, spots %zu, applied sites %d\n", R.name.c_str(), R.r.x, R.r.y, R.r.w, R.r.h,
                  g.war.camps.size(), g.war.ov.size(), g.war.spots.size(), g.war.appliedSites);
        }
      }
      else {
        standAt(g, camp->gx - g.world.ox + 3, camp->gy - g.world.oy + 4);
        tick(g, 90);
        camp = warRefugeCampOf(g, refuge);
        int tents = 0, refugees = 0;
        if (camp)
          for (const auto& t : camp->tents) if (g.world.over.propAt(t.first - g.world.ox, t.second - g.world.oy) == (int)art::Prop::RefugeeTent + 1) tents++;
        for (const Actor& a : g.actors) if (a.npc && a.role == Role::Refugee && a.st != AState::Dead) refugees++;
        if (getenv("EMB_WARDS_TRACE"))
          fprintf(stderr, "  refuge camp at %d,%d (local %d,%d), player %.0f,%.0f inside %d, camp %s, posts %zu, camps %zu\n", camp ? camp->gx : 0,
                  camp ? camp->gy : 0, camp ? camp->gx - g.world.ox : 0, camp ? camp->gy - g.world.oy : 0, g.pl().p.x / TILE, g.pl().p.y / TILE,
                  (int)g.inside, camp ? "yes" : "no", camp ? camp->posts.size() : (size_t)0, g.war.camps.size());
        if (tents < 2) fail("the refugee camp has " + std::to_string(tents) + " lean-tos");
        if (refugees < 3) fail("the refugee camp has " + std::to_string(refugees) + " refugees");
        out("wards seed %llu: burned %s: %d/%d charred (%d burned out); refugee camp: %d lean-tos, %d refugees\n", (unsigned long long)seed,
            s.name.c_str(), ch, n, out2, tents, refugees);
      }
    }
  }
  if (getenv("EMB_WARDS_TRACE")) { fprintf(stderr, "wards trace line %d\n", __LINE__); fflush(stderr); }
  // ---- siege: the camp, its men, hostility across the war, the player's kills feeding def
  {
    Game g = base;
    const int target = ti >= 0 ? ti : vi;
    if (target >= 0) {
      const ew::Gid sid = g.world.sites[(size_t)target].id;
      int si = visit(g, sid);
      const ew::Gid def = kidOf(g, g.world.sites[(size_t)si].kingdom), att = otherKingdom(g, def);
      if (att) {
        const uint32_t sgId = g.realm.forceSiege(sid, att, g.day);
        g.realmSync();
        tick(g, 40);
        si = g.world.siteHandle(sid);
        const WarCamp* camp = warCampOf(g, sgId);
        if (!camp) fail("no siege camp laid out for " + g.world.sites[(size_t)si].name);
        else {
          const Site S = g.world.sites[(size_t)si];
          const Map& M = g.world.over;
          int props = 0, nearest = 1 << 30, farthest = 0;
          for (const auto& kv : g.war.propKingdom) {
            const int x = (int32_t)(kv.first >> 32) - g.world.ox, y = (int32_t)(uint32_t)kv.first - g.world.oy;
            if (kv.second != att || !M.in(x, y)) continue;
            const int p = M.propAt(x, y) - 1;
            if (p < 0 || !art::isWarProp((art::Prop)p) || p == (int)art::Prop::Barricade) continue;
            props++;
            const int d = rectDist(S.r, x, y);
            nearest = std::min(nearest, d); farthest = std::max(farthest, d);
            const Ground gr = M.at(x, y);
            if (gr == Ground::Road || gr == Ground::Bridge || groundWater(gr) || M.bldgAt[(size_t)y * M.w + x] >= 0) { fail("a camp prop stands on a road, water or a building"); break; }
          }
          if (props < 6) fail("the siege camp has only " + std::to_string(props) + " war props");
          if (props && (nearest < 12 || farthest > 36)) fail("siege camp props " + std::to_string(nearest) + "-" + std::to_string(farthest) + " tiles out (want 12-36)");
          // a way out of the camp: from the fire to 14 tiles off, over walkable tiles
          {
            const int cx = camp->gx - g.world.ox, cy = camp->gy - g.world.oy;
            std::vector<int> seen((size_t)M.w * M.h, 0), q;
            int sx = cx, sy = cy + 1;
            if (M.blocked(sx, sy)) sy = cy - 1;
            bool outOk = false;
            if (M.in(sx, sy) && !M.blocked(sx, sy)) {
              q.push_back(sy * M.w + sx); seen[(size_t)(sy * M.w + sx)] = 1;
              for (size_t h = 0; h < q.size() && !outOk && q.size() < 20000; h++) {
                const int x = q[h] % M.w, y = q[h] / M.w;
                if (std::abs(x - cx) + std::abs(y - cy) > 16) { outOk = true; break; }
                static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
                for (int k = 0; k < 4; k++) {
                  const int nx = x + dx[k], ny = y + dy[k];
                  if (!M.in(nx, ny) || M.blocked(nx, ny) || seen[(size_t)(ny * M.w + nx)]) continue;
                  seen[(size_t)(ny * M.w + nx)] = 1;
                  q.push_back(ny * M.w + nx);
                }
              }
            }
            if (!outOk) fail("the siege camp is sealed (no walkable way out from its fire)");
          }
          // the attackers turn out when the player comes near the camp
          standAt(g, camp->gx - g.world.ox + 6, camp->gy - g.world.oy + 9);
          tick(g, 120);
          int soldiers = 0, captains = 0, guardId = -1, soldierId = -1;
          for (const Actor& a : g.actors) {
            if (!a.npc || a.st == AState::Dead) continue;
            if (a.realm == att && a.role == Role::Soldier) { soldiers++; soldierId = a.id; }
            if (a.realm == att && a.role == Role::Captain) captains++;
          }
          if (soldiers < 4) fail("the siege camp has " + std::to_string(soldiers) + " soldiers (want >= 4)");
          if (!captains) fail("the siege camp has no captain");
          // hostility across the war, none between allies
          Actor A, D, D2;
          A.realm = att; A.npc = true; A.faction = Faction::Army;
          D.realm = def; D.npc = true; D.faction = Faction::Town; D.role = Role::Guard;
          D2.realm = att; D2.npc = true; D2.faction = Faction::Town; D2.role = Role::Guard;
          if (!g.warHostile(A, D)) fail("warHostile: an attacker soldier and a defender guard are not enemies");
          if (g.warHostile(A, D2)) fail("warHostile: a soldier and a guard of the same kingdom are enemies");
          if (warFoes(g, A, D2)) fail("warFoes: allies fight");
          (void)guardId;
          // the player's kills on BREAK THE SIEGE raise def (0.5 each)
          const realm::Siege* sg = nullptr;
          for (const realm::Siege& x : g.realm.sieges()) if (x.id == sgId) sg = &x;
          if (sg && soldierId >= 0) {
            const float d0 = sg->def;
            Quest q;
            q.type = QType::War; q.title = "BREAK THE SIEGE"; q.stage = (int)sgId; q.need = 12; q.target = si; q.targetId = sid;
            q.giverSite = si; q.giverSlot = 99999;
            g.debugAccept(q);
            tick(g, 2);
            int killed = 0, capKilled = 0;
            // (fixer M4 r3) the captain falls FIRST: later besieger kills must still count toward 12
            for (const Actor& a : g.actors)
              if (a.npc && a.realm == att && a.role == Role::Captain && a.st != AState::Dead) { g.debugFell(a.id, true); capKilled = 1; break; }
            for (int k = 0; k < 3; k++) {
              int id = -1;
              for (const Actor& a : g.actors) if (a.npc && a.realm == att && a.role == Role::Soldier && a.st != AState::Dead) { id = a.id; break; }
              if (id < 0) break;
              g.debugFell(id, true);
              killed++;
            }
            const realm::Siege* sg2 = nullptr;
            for (const realm::Siege& x : g.realm.sieges()) if (x.id == sgId) sg2 = &x;
            const float want = d0 + 0.5f * (killed + capKilled);
            if (!sg2 || std::fabs(sg2->def - want) > 0.01f) fail("siege def did not rise 0.5 a kill (" + std::to_string(d0) + " -> " + std::to_string(sg2 ? sg2->def : -1) + ")");
            const Quest* qq = nullptr;
            for (const Quest& x : g.quests) if (x.type == QType::War) qq = &x;
            if (!qq || qq->have != killed) fail("the siege quest did not count the kills (have " + std::to_string(qq ? qq->have : -1) + ", want " + std::to_string(killed) + ")");
            if (capKilled && (!qq || !(qq->flags & QF_WAR_CAPDEAD))) fail("the besiegers' captain fell but the quest did not mark it");
            // the besiegers are hostile to the player now
            int hostile = 0;
            for (const Actor& a : g.actors) if (a.npc && a.realm == att && a.hostile && a.st != AState::Dead) hostile++;
            tick(g, 40);
            for (const Actor& a : g.actors) if (a.npc && a.realm == att && a.hostile && a.st != AState::Dead) hostile++;
            if (!hostile) fail("the besiegers are not hostile to a player who sides with the town");
            out("wards seed %llu: siege of %s: %d camp props %d-%d tiles out, %d soldiers + %d captain; %d kills: def %.1f -> %.1f\n",
                (unsigned long long)seed, S.name.c_str(), props, nearest, farthest, soldiers, captains, killed, d0, sg2 ? sg2->def : -1.0f);
          }
        }
      }
    }
  }
  if (getenv("EMB_WARDS_TRACE")) { fprintf(stderr, "wards trace line %d\n", __LINE__); fflush(stderr); }
  // ---- road patrols near a player standing on a kingdom's road
  {
    Game g = base;
    g.godMode = true;
    // a road tile in kingdom land, out of town, near the start
    const Map& M = g.world.over;
    int rx = -1, ry = -1;
    const int px = (int)(g.pl().p.x / TILE), py = (int)(g.pl().p.y / TILE);
    for (int r = 12; r < 100 && rx < 0; r += 2)
      for (int k = 0; k < 64 && rx < 0; k++) {
        const float a = k * 0.0982f;
        const int x = px + (int)std::lround(std::cos(a) * r), y = py + (int)std::lround(std::sin(a) * r);
        if (!M.in(x, y) || M.at(x, y) != Ground::Road || M.blocked(x, y) || g.world.siteAt(x, y, 6) >= 0) continue;
        const int32_t gx = g.world.ox + x, gy = g.world.oy + y;
        if (!g.realm.landOwner(g.world.src->kingdomAt(gx, gy), gx, gy)) continue;
        rx = x; ry = y;
      }
    if (rx < 0) out("WARN: wards seed %llu: no kingdom road near the start\n", (unsigned long long)seed);
    else {
      standAt(g, rx, ry);
      float at = -1;
      for (int f = 0; f < 180 * 60 && at < 0; f++) {
        tick(g, 1);
        int men = 0;
        for (const WarPatrol& p : g.war.patrols) men += (int)p.ids.size();
        if (men >= 2) at = f / 60.0f;
      }
      if (at < 0) fail("no road patrol within 3 minutes on a kingdom road");
      else {
        // they walk: the leader moves along the road over the next seconds
        const WarPatrol& p0 = g.war.patrols.front();
        Vec2 a0;
        for (const Actor& a : g.actors) if (a.id == p0.ids.front()) a0 = a.p;
        tick(g, 300);
        float moved = 0;
        if (!g.war.patrols.empty())
          for (const Actor& a : g.actors) if (a.id == g.war.patrols.front().ids.front()) moved = len(a.p - a0) / TILE;
        if (moved < 3) fail("the road patrol does not walk its road (" + std::to_string(moved) + " tiles in 5 s)");
        out("wards seed %llu: a patrol of %zu after %.1f s, the leader walked %.1f tiles in 5 s\n", (unsigned long long)seed,
            g.war.patrols.empty() ? 0 : g.war.patrols.front().ids.size(), at, moved);
      }
    }
  }
  if (getenv("EMB_WARDS_TRACE")) { fprintf(stderr, "wards trace line %d\n", __LINE__); fflush(stderr); }
  // ---- costs (the base game's steps: overlays with the realm's states, camps, patrols)
  {
    const double step = base.war.steps ? base.war.stepMs / base.war.steps : 0;
    out("wards seed %llu: warStep avg %.3f ms (worst %.2f) over %d steps; overlay applies %d (avg %.3f ms)\n", (unsigned long long)seed, step,
        base.war.worstStepMs, base.war.steps, base.war.applies, base.war.applies ? base.war.applyMs / base.war.applies : 0.0);
  }
  return bad;
}

// the cost of the war's step and overlays with a siege, a burning and a refugee camp in the window (targets: step 0.3 ms,
// an overlay apply 0.2 ms a settlement)
int costSeed(uint64_t seed, double& stepAvg, double& applyPer) {
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.godMode = true;
  g.noWildSpawns = true;
  tick(g, 30);
  for (int k = 0; k < 3000 && g.realm.focusPending(); k++) tick(g, 1);
  const Site& s = g.world.sites[(size_t)g.world.startSite];
  const ew::Gid own = kidOf(g, s.kingdom), enemy = otherKingdom(g, own);
  if (enemy) {
    g.realm.forceSiege(s.id, enemy, g.day);
    g.realmSync();
  }
  g.war = WarState();
  g.war.dirty = true;
  tick(g, 600);
  stepAvg = g.war.steps ? g.war.stepMs / g.war.steps : 0;
  applyPer = g.war.applies && g.war.appliedSites ? g.war.applyMs / g.war.applies / std::max(1, g.war.appliedSites) : 0;
  out("wards seed %llu cost: warStep avg %.3f ms, worst %.2f ms (%d steps); overlay %.3f ms a settlement (%d applies, %d sites)\n",
      (unsigned long long)seed, stepAvg, g.war.worstStepMs, g.war.steps, applyPer, g.war.applies, g.war.appliedSites);
  return 0;
}

int wardsCmd(int argc, char** argv) {
  uint64_t A = 1, B = 5;
  for (int i = 2; i < argc; i++)
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], A, B);
  int bad = 0;
  double stepSum = 0, applySum = 0;
  int n = 0;
  for (uint64_t s = A; s <= B; s++) {
    bad += wardsSeed(s);
    double st = 0, ap = 0;
    bad += costSeed(s, st, ap);
    stepSum += st; applySum += ap; n++;
  }
  const double stepAvg = n ? stepSum / n : 0, applyAvg = n ? applySum / n : 0;
  printf("wards: warStep avg %.3f ms (target <= 0.3), overlay apply %.3f ms a settlement (target <= 0.2)\n", stepAvg, applyAvg);
  if (stepAvg > 0.3) { printf("FAIL: wards: warStep averages %.3f ms (target 0.3)\n", stepAvg); bad++; }
  if (applyAvg > 0.2) { printf("FAIL: wards: an overlay apply costs %.3f ms a settlement (target 0.2)\n", applyAvg); bad++; }
  printf("wards: %d failures\n", bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--wards", "M4 war made visible: kingdom guards, frontier militia, raids, burned towns, refugees, sieges, patrols, costs [--seeds A..B]", wardsCmd);
