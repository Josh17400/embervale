// M4 WARDS lane script commands (rpg/view/script_api.h; `embervale --script-help` lists them). Test only: none touches a
// save. They put the player where the war's overlays and people are, so the review scripts can look at them:
//   war goto camp|refuge|tower|burned|gate [n]   stand by the n-th nearest besieged settlement's siege camp (8 tiles
//                               south of its fire), a refugee camp, a garrison tower, a burned-out building, the
//                               besieged town's gate (inside the walls)
//   war goto captain att|def    stand a step south of the besiegers' (att) or the defenders' (def) captain, facing him
//                               (then `key E` talks); waits up to 10 s for him to turn out
//   war goto road               stand on the nearest road tile of a kingdom's land, away from settlements
//   war goto checkpoint         stand by a checkpoint (barricades where a road crosses into a warring kingdom's land)
//   war goto border             go to where the player's land meets the land of a kingdom it is at war with
//   war goto patrol             a road patrol's leader within 9 tiles of the player (4 s for him to come by, then
//                               the player steps to the roadside ahead of him); fails at once in town, and within
//                               2 s when none is out
//   war goto soldiers [0 reset] the soldiers of a realm an earlier call has not shown (a culture not shown first): ones
//                               in play nearby, else out along its seat's road, where its patrol sets off to meet
//   war raid <monster> <n> [k]  n monsters loosed in the k-th nearest settlement's street, beside its people
//   war kill <n>                n of the besiegers (or, on JOIN THE ASSAULT, the defenders) fall to the player's hand
//   war mark                    remember the nearest siege's atk and def (for expect siege)
//   war at <x> <y>              (with a siege camp) stand at that offset in tiles from the camp's fire
//   war free [k] / war take [k] the k-th nearest settlement is let go by its kingdom (frontier) / taken by the nearest
//                               kingdom of another culture (a garrison tower goes up)
//   expect siege def>|atk>      the siege's def / atk is above the value `war mark` remembered
//   expect war guards <n> [k] | frontier [k] | militia <n> [k] | camp | refugees <n> | tower | charred <n> [k] | patrol
//                               the k-th nearest settlement keeps n guards on its streets / is frontier / has n militia
//                               out; a siege camp is laid out; n refugees are in play; a garrison tower stands; n of the
//                               k-th nearest settlement's buildings draw charred; a road patrol is out
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/art/art_props.h"
#include "rpg/sim/game.h"
#include "rpg/sim/war.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"
#include "rpg/world/source.h"

namespace {

float g_markAtk = -1, g_markDef = -1;

std::vector<int> nearestSettlements(Game& g) {
  std::vector<std::pair<float, int>> d;
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  for (size_t i = 0; i < g.world.sites.size(); i++) {
    const Site& s = g.world.sites[i];
    if (!s.settlement()) continue;
    const float dx = s.ex - px, dy = s.ey - py;
    d.push_back({dx * dx + dy * dy, (int)i});
  }
  std::sort(d.begin(), d.end());
  std::vector<int> out;
  for (auto& e : d) out.push_back(e.second);
  return out;
}
// teleport onto the free tile nearest (tx, ty) (window tiles); never onto a doorstep
void standAt(Game& g, int tx, int ty) {
  g.teleportGlobal(g.world.ox + tx, g.world.oy + ty);
}
// dry open ground to stand on beside a road (not the road, a bridge, water, a swamp, rock or ice)
bool roadside(const Map& m, int x, int y) {
  if (!m.in(x, y) || m.blocked(x, y)) return false;
  switch (m.at(x, y)) {
    case Ground::Sand: case Ground::Grass: case Ground::Meadow: case Ground::ForestFloor: case Ground::Autumn:
    case Ground::Tundra: case Ground::Snow: case Ground::Dirt: case Ground::Farmland: return true;
    default: return false;
  }
}
// the player steps to the roadside a few tiles ahead of a patrol's leader (in the window: no teleport, the patrol
// stays), facing him; false when its leader is gone or no open tile lies beside the road there
bool meetPatrol(Game& g, const WarPatrol& p) {
  if (p.ids.empty() || p.path.empty()) return false;
  for (const Actor& a : g.actors)
    if (a.id == p.ids.front() && a.st != AState::Dead) {
      const auto& to = p.path[(size_t)std::min<int>((int)p.path.size() - 1, p.at + 6)];
      const int rx = to.first - g.world.ox, ry = to.second - g.world.oy;
      for (int r = 2; r <= 8; r++)   // the first dry open tile beside the road, two tiles out or more
        for (int s = -1; s <= 1; s += 2)
          for (const bool xs : {true, false}) {
            const int tx = xs ? rx + s * r : rx, ty = xs ? ry : ry + s * r;
            if (!roadside(g.map(), tx, ty)) continue;
            g.pl().p = Vec2(tx * TILE + 8.0f, ty * TILE + 10.0f);
            return true;
          }
      return false;
    }
  return false;
}
const realm::Siege* nearestSiege(Game& g) {
  const realm::Siege* best = nullptr;
  float bd = 1e30f;
  for (const realm::Siege& s : g.realm.sieges()) {
    if (s.over) continue;
    const int si = g.world.siteHandle(s.site);
    if (si < 0) continue;
    const Site& S = g.world.sites[(size_t)si];
    const float d = std::hypot(S.ex - g.pl().p.x / TILE, S.ey - g.pl().p.y / TILE);
    if (d < bd) { bd = d; best = &s; }
  }
  return best;
}
art::Monster monsterNamed(const std::string& w) {
  std::string u = w;
  for (char& ch : u) ch = (char)std::toupper((unsigned char)ch);
  for (int m = 0; m < (int)art::Monster::COUNT; m++) {
    static const char* n[] = {"WOLF", "BOAR", "BEAR", "SLIME", "SPIDER", "BAT", "SKELETON", "DRAUGR", "GOBLIN", "TROLL", "WRAITH", "MUDCRAB",
                              "ICEWOLF", "FROSTSPIDER", "SANDWORM", "DRAGON", "SCORPION", "HYENA", "LURKER", "YETI", "WISP", "EMBERHOUND", "BLIGHTSPAWN",
                              "HARPY", "GOLEM", "SEASERPENT", "KRAKEN", "MERROW"};
    static_assert(sizeof(n) / sizeof(n[0]) == (size_t)art::Monster::COUNT, "a name for every monster");
    if (u == n[m]) return (art::Monster)m;
  }
  return art::Monster::Wolf;
}

bool cmdWar(ScriptCtx& c) {
  Game& g = c.game;
  const std::string what = c.arg(1);
  if (what == "mark") {
    const realm::Siege* s = nearestSiege(g);
    if (!s) { c.fail("war mark: no siege"); return true; }
    g_markAtk = s->atk; g_markDef = s->def;
    std::printf("war mark: siege %u atk %.1f def %.1f\n", s->id, s->atk, s->def);
    return true;
  }
  if (what == "kill") {
    const int n = std::max(1, std::atoi(c.arg(2).c_str()));
    const realm::Siege* s = nearestSiege(g);
    if (!s) { c.fail("war kill: no siege"); return true; }
    bool assault = false;
    for (const Quest& q : g.quests) if (q.type == QType::War && q.state == QState::Active && q.stage == (int)s->id) assault = (q.flags & QF_WAR_ATTACK) != 0;
    const ew::Gid foe = assault ? s->defender : s->attacker;
    int done = 0;
    for (int k = 0; k < n; k++) {
      int id = -1;
      float bd = 1e30f;
      for (const Actor& a : g.actors)
        if (a.npc && a.realm == foe && a.st != AState::Dead && (a.role == Role::Soldier || (assault && a.role == Role::Guard))) {
          const float d = len2(a.p - g.pl().p);
          if (d < bd) { bd = d; id = a.id; }
        }
      if (id < 0) break;
      g.debugFell(id, true);
      done++;
    }
    if (!done) c.fail("war kill: nobody of the foe in play");
    return true;
  }
  if (what == "raid") {
    const art::Monster m = monsterNamed(c.arg(2));
    const int n = std::max(1, std::atoi(c.arg(3).c_str()));
    const size_t k = (size_t)std::max(0, std::atoi(c.arg(4).c_str()));
    const std::vector<int> near = nearestSettlements(g);
    if (k >= near.size()) { c.fail("war raid: no such settlement"); return true; }
    const Site& s = g.world.sites[(size_t)near[k]];
    // beside the townsperson nearest the middle, in the open street
    Vec2 mid((s.r.x + s.r.w * 0.5f) * TILE, (s.r.y + s.r.h * 0.5f) * TILE), at = mid;
    float bd = 1e30f;
    for (const Actor& a : g.actors)
      if (a.npc && a.site == near[k] && a.role != Role::Guard && !a.stallKeeper && len2(a.p - mid) < bd) { bd = len2(a.p - mid); at = a.p; }
    const int tx = (int)std::floor(at.x / TILE), ty = (int)std::floor(at.y / TILE);
    for (int r = 0; r < 8; r++) {
      bool found = false;
      for (int dy = -r; dy <= r && !found; dy++)
        for (int dx = -r; dx <= r && !found; dx++) {
          bool ok = true;
          for (int yy = -1; yy <= 1 && ok; yy++) for (int xx = -1; xx <= 1 && ok; xx++) if (g.world.over.blocked(tx + dx + xx, ty + dy + yy)) ok = false;
          if (ok) { at = Vec2((tx + dx) * TILE + 8.0f, (ty + dy) * TILE + 10.0f); found = true; }
        }
      if (found) break;
    }
    const int lvl = std::max(1, g.world.zoneLevel(s.ex, s.ey));
    for (int i = 0; i < n; i++) g.debugSpawnAt(m, at + Vec2((float)((i % 3) - 1) * 10.0f, (float)(i / 3) * 8.0f - 4.0f), lvl);
    return true;
  }
  if (what == "free" || what == "take") {
    // free: no kingdom holds it any more (a frontier place); take: a kingdom of ANOTHER culture conquers it (so its
    // garrison tower goes up in the conqueror's architecture)
    const size_t k = (size_t)std::max(0, std::atoi(c.arg(2).c_str()));
    const std::vector<int> near = nearestSettlements(g);
    if (k >= near.size()) { c.fail("war " + what + ": no such settlement"); return true; }
    const Site& s = g.world.sites[(size_t)near[k]];
    const ew::Gid home = s.homeKingdom >= 0 ? g.world.kingdoms[(size_t)s.homeKingdom].id : 0;
    g.realm.noteSite(s.id, home, (uint8_t)s.type, g.world.ox + s.ex, g.world.oy + s.ey);
    const ew::Gid own = s.kingdom >= 0 ? g.world.kingdoms[(size_t)s.kingdom].id : 0;
    ew::Gid to = 0;
    if (what == "take") {
      const realm::KingdomState* O = g.realm.kingdom(own);
      double bd = 1e30;
      for (const realm::KingdomState& K : g.realm.kingdoms()) {
        if (K.id == own || K.fallen || (O && K.culture == O->culture) || !g.world.kingdomById.count(K.id)) continue;
        const Kingdom& R = g.world.kingdoms[(size_t)g.world.kingdomById[K.id]];
        const double dx = R.gx - (g.world.ox + s.ex), dy = R.gy - (g.world.oy + s.ey), d = dx * dx + dy * dy;
        if (d < bd) { bd = d; to = K.id; }
      }
      if (!to) { c.fail("war take: no kingdom of another culture"); return true; }
    }
    g.realm.forceOwner(s.id, to, g.day);
    g.realmSync();
    return true;
  }
  if (what == "at") {
    const realm::Siege* s = nearestSiege(g);
    const WarCamp* camp = s ? warCampOf(g, s->id) : nullptr;
    if (!camp) { c.fail("war at: no siege camp"); return true; }
    standAt(g, camp->gx - g.world.ox + std::atoi(c.arg(2).c_str()), camp->gy - g.world.oy + std::atoi(c.arg(3).c_str()));
    return true;
  }
  if (what != "goto") { c.fail("war goto|raid|kill|mark|at ..."); return true; }
  const std::string where = c.arg(2);
  const size_t n = (size_t)std::max(0, std::atoi(c.arg(3).c_str()));
  if (where == "camp" || where == "gate") {
    const realm::Siege* s = nearestSiege(g);
    if (!s) { c.fail("war goto " + where + ": no siege"); return true; }
    const WarCamp* camp = warCampOf(g, s->id);
    if (where == "camp") {
      if (!camp) { if (c.waited < 5) return false; c.fail("war goto camp: no camp laid out"); return true; }
      standAt(g, camp->gx - g.world.ox, camp->gy - g.world.oy + 5);
      return true;
    }
    const int si = g.world.siteHandle(s->site);
    if (si < 0) { c.fail("war goto gate: the town is not loaded"); return true; }
    const Site& S = g.world.sites[(size_t)si];
    int gx = S.ex, gy = S.ey;
    int bd = 1 << 30;
    for (const IRect& r : g.world.wallGaps) {
      if (r.x + r.w < S.r.x - 2 || r.y + r.h < S.r.y - 2 || r.x > S.r.x + S.r.w + 2 || r.y > S.r.y + S.r.h + 2) continue;
      const int x = r.x + r.w / 2, y = r.y + r.h / 2;
      const int d = camp ? std::abs(x - (camp->gx - g.world.ox)) + std::abs(y - (camp->gy - g.world.oy)) : 0;
      if (d < bd) { bd = d; gx = x; gy = y + (y > S.ey ? -3 : 3); }
    }
    standAt(g, gx, gy);
    return true;
  }
  if (where == "captain") {
    const realm::Siege* s = nearestSiege(g);
    if (!s) { c.fail("war goto captain: no siege"); return true; }
    const bool att = c.arg(3) != "def";
    const ew::Gid k = att ? s->attacker : s->defender;
    // bring the place near first (the captains turn out when the player comes close)
    if (c.waited == 0) {
      const WarCamp* camp = warCampOf(g, s->id);
      const int si = g.world.siteHandle(s->site);
      if (att && camp) standAt(g, camp->gx - g.world.ox, camp->gy - g.world.oy + 6);
      else if (!att && si >= 0) standAt(g, g.world.sites[(size_t)si].ex, g.world.sites[(size_t)si].ey + 2);
      return false;
    }
    for (const Actor& a : g.actors)
      if (a.npc && a.role == Role::Captain && a.realm == k && a.st != AState::Dead) {
        const int tx = (int)std::floor(a.p.x / TILE), ty = (int)std::floor(a.p.y / TILE);
        g.pl().p = Vec2(tx * TILE + 8.0f, (ty + 1) * TILE + 9.0f);
        g.pl().face = 1;
        g.pl().aim = Vec2(0, -1);
        return true;
      }
    if (c.waited < 10) return false;
    c.fail("war goto captain: no captain in play");
    return true;
  }
  if (where == "border") {
    // the border between the player's land and a kingdom at war with it: along the line between their seats, where the
    // land changes hands (sampled every 16 tiles), the window brought there
    int32_t px = g.world.ox + (int)std::floor(g.pl().p.x / TILE), py = g.world.oy + (int)std::floor(g.pl().p.y / TILE);
    const ew::Gid me = g.realm.landOwner(g.world.src->kingdomAt(px, py), px, py);
    ew::Gid foe = 0;
    for (const realm::War& w : g.realm.wars())
      if (!w.endDay && (w.attacker == me || w.defender == me)) foe = w.attacker == me ? w.defender : w.attacker;
    auto it = g.world.kingdomById.find(foe);
    if (!foe || it == g.world.kingdomById.end()) { c.fail("war goto border: the player's land is at peace"); return true; }
    const Kingdom& F = g.world.kingdoms[(size_t)it->second];
    const float dx = (float)(F.gx - px), dy = (float)(F.gy - py), L = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
    for (float t = 0; t < L; t += 16) {
      const int32_t x = px + (int32_t)std::lround(dx / L * t), y = py + (int32_t)std::lround(dy / L * t);
      if (g.realm.landOwner(g.world.src->kingdomAt(x, y), x, y) != me) {
        g.teleportGlobal(x, y);
        return true;
      }
    }
    c.fail("war goto border: no border found toward the enemy");
    return true;
  }
  if (where == "checkpoint") {
    for (const WarCamp& camp : g.war.camps)
      if (camp.kind == CampKind::Checkpoint) { standAt(g, camp.gx - g.world.ox + 1, camp.gy - g.world.oy + 3); return true; }
    if (c.waited < 3) return false;
    c.fail("war goto checkpoint: no checkpoint in the window (no warring border road near)");
    return true;
  }
  if (where == "refuge") {
    for (const WarCamp& camp : g.war.camps)
      if (camp.kind == CampKind::Refugee) { standAt(g, camp.gx - g.world.ox + 2, camp.gy - g.world.oy + 6); return true; }
    // the settlement sheltering refugees may lie beyond the window: go there first, then find its camp
    if (c.waited == 0)
      for (const Site& s : g.world.sites)
        if (s.settlement())
          if (const realm::SettlementState* st = g.realm.settlement(s.id))
            if (st->flags & realm::SS_REFUGEES) { standAt(g, s.ex, s.ey + 2); return false; }
    if (c.waited < 5) return false;
    c.fail("war goto refuge: no refugee camp");
    return true;
  }
  if (where == "tower") {
    for (int bi : g.war.barred) {
      if (bi < 0 || bi >= (int)g.world.over.bldgs.size() || g.world.over.bldgs[(size_t)bi].charred == 2) continue;
      const Bldg& b = g.world.over.bldgs[(size_t)bi];
      standAt(g, b.doorX() + 2, b.doorY() + 5);
      return true;
    }
    if (c.waited < 5) return false;
    c.fail("war goto tower: no garrison tower");
    return true;
  }
  if (where == "burned") {
    std::vector<int> found;
    for (int bi : g.war.barred)
      if (bi >= 0 && bi < (int)g.world.over.bldgs.size() && g.world.over.bldgs[(size_t)bi].charred == 2) found.push_back(bi);
    if (found.empty()) { if (c.waited < 5) return false; c.fail("war goto burned: nothing burned out"); return true; }
    const Bldg& b = g.world.over.bldgs[(size_t)found[std::min(n, found.size() - 1)]];
    standAt(g, b.doorX(), b.doorY() + 4);
    return true;
  }
  if (where == "road") {
    const Map& M = g.world.over;
    const int px = (int)(g.pl().p.x / TILE), py = (int)(g.pl().p.y / TILE);
    // (fix, C10 T4 playtest: "the bandit 3 tiles east stands under a tree's crown") the road is where the fighter
    // goal's duels are staged: a foe spawned up to 4 tiles east of the stand tile must be in full view. A tree's crown
    // and a building's roof rise above the tile they stand on, so none may stand in the strip east of the hero nor in
    // the rows just below it (an ElderTree's spread and the souk's cloth canopies count too); the duel tiles stay open
    // duelCover: how much stands over the duel (0: none). A crown or a roof hiding the hero's or the foe's body (the
    // tiles 0 and 3 east: a crown spreads a tile each side of its trunk and rises about three tiles over it) weighs 10,
    // anything else in the strip 1. -1: the duel tiles east are not open ground
    auto duelCover = [&](int x, int y) {
      for (int dx = 1; dx <= 4; dx++)
        if (!M.in(x + dx, y) || M.blocked(x + dx, y)) return -1;
      int n = 0;
      // (a tree or a roof in the rows above stands behind the duel and is drawn first: only the stand row and the rows
      // below count)
      for (int yy = y; yy <= y + 7; yy++)
        for (int xx = x - 2; xx <= x + 6; xx++) {
          if (!M.in(xx, yy)) continue;
          const bool body = yy <= y + 3 && (std::abs(xx - x) <= 1 || std::abs(xx - (x + 3)) <= 1);
          if (M.bldgAt[(size_t)yy * M.w + xx] >= 0) { n += body || yy <= y + 5 ? 10 : 1; continue; }
          if (yy > y + 4 || xx > x + 5) continue;
          const int q = M.propAt(xx, yy);
          if (!q) continue;
          const art::Prop pr = (art::Prop)(q - 1);
          if (art::isTreeProp(pr) || pr == art::Prop::ElderTree || pr == art::Prop::Canopy) n += body ? 10 : 1;
        }
      return n;
    };
    // the nearest kingdom road tile with nothing over the duel; else, within the search, the one least covered (the
    // hero's and the foe's bodies in view before the strip's edges), nearer first among equals
    int bestX = -1, bestY = -1, bestN = 1 << 30, bestR = 0;
    for (int r = 8; r < 110 && bestN > 0; r += 2) {
      const int steps = std::max(72, (int)(r * 6.3f));
      for (int k = 0; k < steps && bestN > 0; k++) {
        const float a = k * 6.2832f / (float)steps;
        const int x = px + (int)std::lround(std::cos(a) * r), y = py + (int)std::lround(std::sin(a) * r);
        if (!M.in(x, y) || M.at(x, y) != Ground::Road || M.blocked(x, y) || g.world.siteAt(x, y, 8) >= 0) continue;
        const int32_t gx = g.world.ox + x, gy = g.world.oy + y;
        if (!g.realm.landOwner(g.world.src->kingdomAt(gx, gy), gx, gy)) continue;
        const int n = duelCover(x, y);
        if (n < 0 || n >= bestN) continue;
        bestN = n; bestX = x; bestY = y; bestR = r;
      }
    }
    if (bestX >= 0) {
      standAt(g, bestX, bestY);
      // (C10 T4 playtest: "the first pose after goto faced north until d was tapped again") the hero faces the duel's
      // open tiles east, where the staged foe stands
      g.pl().face = 2;
      g.pl().aim = Vec2(1, 0);
      if (bestN == 0) std::printf("script: war goto road: an open stretch (no tree or roof by the duel tiles east) %d tiles away\n", bestR);
      else std::printf("script: war goto road: no open stretch near; the least covered kingdom road %d tiles away (cover %d)\n", bestR, bestN);
      return true;
    }
    c.fail("war goto road: no kingdom road near");
    return true;
  }
  if (where == "patrol") {
    // (no teleport: that would put the patrol away) a patrol's leader within 9 tiles of the player: done
    for (const WarPatrol& p : g.war.patrols)
      for (const Actor& a : g.actors)
        if (!p.ids.empty() && a.id == p.ids.front() && a.st != AState::Dead && len2(a.p - g.pl().p) < (9.0f * TILE) * (9.0f * TILE)) return true;
    if (g.war.patrols.empty()) {
      // (fix, T6 playtest: "war goto patrol returned busy and then every later call stayed busy") none out: the call
      // no longer waits for one that may never come. Patrols set off only while the player is out of town, from a
      // kingdom's road 24-36 tiles away (Game::warStep), so in town it fails at once; out of town the next spawn
      // check is brought forward (test only) and the call fails within 2 s if none sets off
      if (g.settlementAt(g.pl().p) >= 0) {
        c.fail("war goto patrol: no road patrol is out, and none sets off while you are in a town (try 'war goto soldiers', "
               "which travels to a realm's road and meets its patrol, or 'war goto road' then 'war goto patrol')");
        return true;
      }
      g.war.patrolT = 0;
      if (c.waited < 2) return false;
      c.fail("war goto patrol: no road patrol is out near here (patrols set off from a kingdom's road 24-36 tiles away; "
             "try 'war goto soldiers', which travels to a realm's road and meets its patrol)");
      return true;
    }
    // one out but walking elsewhere: a few seconds for its leader to come by, then the player steps to the roadside a
    // few tiles ahead of him (in the window: no teleport, the patrol stays). (M7c merge) a patrol walks to the far end
    // of a road picked at random round the ring, so waiting alone failed by the toss
    if (c.waited < 4) return false;
    for (const WarPatrol& p : g.war.patrols)
      if (meetPatrol(g, p)) { c.view.snap(g); return true; }
    c.fail("war goto patrol: no patrol leader in reach (no open roadside ahead of the patrol near here)");
    return true;
  }
  if (where == "soldiers") {
    // (T6 setup) the soldiers of a realm whose soldiers an earlier `war goto soldiers` has not shown yet (so calling it
    // twice shows two realms), at peace or at war: the nearest such soldiers already in play (a siege camp, a
    // checkpoint, a patrol), else a trip to that realm's seat and out along its road, where its road patrol sets off
    // (the spawn check is brought forward: test only) and the player stands at the roadside ahead of its leader
    static std::vector<ew::Gid> shown;
    static std::vector<uint64_t> shownCult;
    static ew::Gid target = 0;
    static uint64_t lastSeed = 0;
    static float lastTime = 0;
    if (g.seed != lastSeed || g.time + 1e-3f < lastTime) { shown.clear(); shownCult.clear(); target = 0; }   // a new game or a reset
    lastSeed = g.seed;
    lastTime = g.time;
    auto realmName = [&](ew::Gid K) {
      if (const realm::KingdomState* S = g.realm.kingdom(K)) return S->name;
      const ew::KingdomPlan* P = g.world.src ? g.world.src->kingdom(K) : nullptr;
      return P ? P->name : std::string("a realm");
    };
    auto isShown = [&](ew::Gid K) { return std::find(shown.begin(), shown.end(), K) != shown.end(); };
    auto done = [&](ew::Gid K, const char* how) {
      target = 0;
      shown.push_back(K);
      if (const realm::KingdomState* S = g.realm.kingdom(K)) shownCult.push_back(S->culture);
      g.mode = Mode::Play;
      c.view.snap(g);
      std::printf("script: war goto soldiers -> %s (%s); realms shown so far %zu\n", realmName(K).c_str(), how, shown.size());
      return true;
    };
    // (a command's first two frames both see waited 0: the trip is under way while target is set)
    if (c.waited == 0 && !target) {
      if (c.arg(3) == "reset" || c.arg(4) == "reset") { shown.clear(); shownCult.clear(); }
      // 1. soldiers in play within 40 tiles
      const Actor* best = nullptr;
      float bd = (40.0f * TILE) * (40.0f * TILE);
      for (const Actor& a : g.actors)
        if (a.npc && a.role == Role::Soldier && a.realm && a.st != AState::Dead && !isShown(a.realm) && len2(a.p - g.pl().p) < bd) {
          bd = len2(a.p - g.pl().p);
          best = &a;
        }
      if (best) {
        const ew::Gid K = best->realm;
        bool placed = false;
        for (const WarPatrol& p : g.war.patrols)
          if (std::find(p.ids.begin(), p.ids.end(), best->id) != p.ids.end()) placed = meetPatrol(g, p);
        if (!placed) {   // a man at his post: two tiles south of him, facing him
          const int tx = (int)std::floor(best->p.x / TILE), ty = (int)std::floor(best->p.y / TILE);
          for (int dy = 2; dy <= 4 && !placed; dy++)
            if (roadside(g.map(), tx, ty + dy) || (g.map().in(tx, ty + dy) && !g.map().blocked(tx, ty + dy) && (g.map().at(tx, ty + dy) == Ground::Road || g.map().at(tx, ty + dy) == Ground::Plaza))) { g.pl().p = Vec2(tx * TILE + 8.0f, (ty + dy) * TILE + 10.0f); placed = true; }
          g.pl().face = 1;
          g.pl().aim = Vec2(0, -1);
        }
        if (placed) return done(K, "soldiers in play nearby");
      }
      // 2. the nearest realm not shown yet (one of a culture not shown yet first), a mainland realm with a seat
      if (!g.world.src) { c.fail("war goto soldiers: no endless world"); return true; }
      const int32_t px = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), py = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
      const ew::KingdomPlan* pick = nullptr;
      double pickD = 1e300;
      bool pickNew = false;
      for (const realm::KingdomState& K : g.realm.kingdoms()) {
        if (K.fallen || isShown(K.id)) continue;
        const ew::KingdomPlan* P = g.world.src->kingdom(K.id);
        if (!P || !P->capital || P->hiddenLand) continue;
        const bool newCult = std::find(shownCult.begin(), shownCult.end(), K.culture) == shownCult.end();
        const double d = std::hypot((double)(P->gx - px), (double)(P->gy - py));
        if ((newCult && !pickNew) || (newCult == pickNew && d < pickD)) { pick = P; pickD = d; pickNew = newCult; }
      }
      if (!pick) { c.fail("war goto soldiers: no realm left whose soldiers have not been shown ('war goto soldiers 0 reset' starts over)"); return true; }
      const ew::Gid K = pick->id;
      const int32_t seatX = pick->gx, seatY = pick->gy;
      if (g.inside) g.debugLeave();
      // where its road patrols set off: a road tile of its land out of town with room for the patrols' ring round it
      // (Game::warStep picks a road 24-36 tiles from the player). A capital fills most of the window, so its villages
      // and towns are tried after the seat, nearest the seat first
      std::vector<std::pair<int32_t, int32_t>> spots{{seatX, seatY}};
      {
        std::vector<std::pair<double, std::pair<int32_t, int32_t>>> near;
        const int32_t rx0 = ew::regionOf(seatX), ry0 = ew::regionOf(seatY);
        for (int32_t ry = ry0 - 3; ry <= ry0 + 3; ry++)
          for (int32_t rx = rx0 - 3; rx <= rx0 + 3; rx++)
            for (const ew::SitePlan& s : g.world.src->region(rx, ry).sites)
              if (s.kingdom == K && (s.type == SiteType::Village || s.type == SiteType::Town))
                near.push_back({std::hypot((double)(s.ex - seatX), (double)(s.ey - seatY)), {s.ex, s.ey}});
        std::sort(near.begin(), near.end());
        for (size_t i = 0; i < near.size() && i < 5; i++) spots.push_back(near[i].second);
      }
      const Map& M = g.world.over;
      int rx = -1, ry = -1;
      for (const auto& sp : spots) {
        g.teleportGlobal(sp.first, sp.second + 3);
        const int sx = sp.first - g.world.ox, sy = sp.second - g.world.oy;
        for (int r = 12; r <= 90 && rx < 0; r += 2)
          for (int k = 0; k < 96 && rx < 0; k++) {
            const float an = 6.2831853f * k / 96;
            const int x = sx + (int)std::lround(std::cos(an) * r), y = sy + (int)std::lround(std::sin(an) * r);
            if (x < 44 || y < 44 || x >= World::WIN - 44 || y >= World::WIN - 44) continue;
            if (M.at(x, y) != Ground::Road || M.blocked(x, y) || g.world.siteAt(x, y, 6) >= 0) continue;
            const int32_t gx = g.world.ox + x, gy = g.world.oy + y;
            if (g.realm.landOwner(g.world.src->kingdomAt(gx, gy), gx, gy) != K) continue;
            rx = x; ry = y;
          }
        if (rx >= 0) break;
      }
      if (rx < 0) { c.fail("war goto soldiers: " + realmName(K) + " has no road out of town near its seat or its villages"); return true; }
      // beside the road (the patrol walks it past the player)
      bool placed = false;
      for (int r = 2; r <= 6 && !placed; r++)
        for (const int s : {1, -1})
          for (const bool xs : {false, true}) {
            if (placed) break;
            const int tx = xs ? rx + s * r : rx, ty = xs ? ry : ry + s * r;
            if (!roadside(M, tx, ty)) continue;
            g.pl().p = Vec2(tx * TILE + 8.0f, ty * TILE + 10.0f);
            placed = true;
          }
      if (!placed) g.pl().p = Vec2(rx * TILE + 8.0f, ry * TILE + 10.0f);
      target = K;
      g.war.patrolT = 0;
      g.mode = Mode::Play;
      c.view.snap(g);
      return false;
    }
    // 3. its patrol (or one of any realm not shown yet) sets off: meet it
    for (const WarPatrol& p : g.war.patrols)
      if (!p.ids.empty() && (p.kingdom == target || !isShown(p.kingdom)) && meetPatrol(g, p)) return done(p.kingdom, "its road patrol");
    if (g.war.patrols.size() < 2) g.war.patrolT = 0;
    if (c.waited < 6) return false;
    c.fail("war goto soldiers: no patrol of " + realmName(target) + " set off from its road");
    target = 0;
    return true;
  }
  c.fail("war goto camp|gate|captain att|def|refuge|tower|burned|road|patrol|soldiers|checkpoint|border");
  return true;
}
EMB_SCRIPT_CMD("war", "war goto camp|gate|captain att|def|refuge|tower|burned|road|patrol|soldiers|checkpoint|border | raid <monster> <n> [k] | kill <n> | mark | at <x> <y> | free [k] | take [k] (M4 WARDS)", cmdWar);

bool expSiege(ScriptCtx& c) {
  Game& g = c.game;
  const realm::Siege* s = nearestSiege(g);
  if (!s) {
    // the siege may have ended already: look through every siege for the marked values
    c.fail("expect siege: no siege");
    return true;
  }
  const std::string what = c.arg(2);
  if (what == "def>" && !(s->def > g_markDef + 1e-3f)) c.fail("expect siege def>: " + std::to_string(s->def) + " not above " + std::to_string(g_markDef));
  else if (what == "atk>" && !(s->atk > g_markAtk + 1e-3f)) c.fail("expect siege atk>: " + std::to_string(s->atk) + " not above " + std::to_string(g_markAtk));
  else if (what != "def>" && what != "atk>") c.fail("expect siege def>|atk>");
  else std::printf("expect siege: atk %.1f def %.1f (marked %.1f / %.1f)\n", s->atk, s->def, g_markAtk, g_markDef);
  return true;
}
EMB_SCRIPT_CMD("expect:siege", "expect siege def>|atk>: the nearest siege's def / atk rose since `war mark` (M4 WARDS)", expSiege);

bool expWar(ScriptCtx& c) {
  Game& g = c.game;
  const std::string what = c.arg(2);
  const std::vector<int> near = nearestSettlements(g);
  auto site = [&](size_t argIx) -> int {
    const size_t k = (size_t)std::max(0, std::atoi(c.arg(argIx).c_str()));
    return k < near.size() ? near[k] : -1;
  };
  if (what == "guards" || what == "militia") {
    const int want = std::atoi(c.arg(3).c_str());
    const int si = site(4);
    if (si < 0) { c.fail("expect war " + what + ": no settlement"); return true; }
    int n = 0;
    for (const Actor& a : g.actors)
      if (a.npc && a.site == si && a.st != AState::Dead && (what == "guards" ? a.role == Role::Guard : a.militia)) n++;
    if (what == "guards" ? n != want : n < want) {
      if (c.waited < 3) return false;
      c.fail("expect war " + what + ": " + std::to_string(n) + " in " + g.world.sites[(size_t)si].name + ", want " + (what == "guards" ? "" : ">= ") + std::to_string(want));
    } else std::printf("expect war %s: %d in %s\n", what.c_str(), n, g.world.sites[(size_t)si].name.c_str());
    return true;
  }
  if (what == "frontier") {
    const int si = site(3);
    if (si < 0 || !warFrontier(g, si)) c.fail("expect war frontier: not frontier");
    return true;
  }
  if (what == "camp") {
    bool any = false;
    for (const WarCamp& cp : g.war.camps) if (cp.kind == CampKind::Siege) any = true;
    if (!any) { if (c.waited < 3) return false; c.fail("expect war camp: no siege camp laid out"); }
    return true;
  }
  if (what == "refugees") {
    const int want = std::atoi(c.arg(3).c_str());
    int n = 0;
    for (const Actor& a : g.actors) if (a.npc && a.role == Role::Refugee && a.st != AState::Dead) n++;
    if (n < want) { if (c.waited < 5) return false; c.fail("expect war refugees: " + std::to_string(n)); }
    return true;
  }
  if (what == "tower") {
    bool any = false;
    for (int bi : g.war.barred) if (bi >= 0 && bi < (int)g.world.over.bldgs.size() && g.world.over.bldgs[(size_t)bi].charred != 2) any = true;
    if (!any) { if (c.waited < 3) return false; c.fail("expect war tower: no garrison tower"); }
    return true;
  }
  if (what == "charred") {
    const int want = std::atoi(c.arg(3).c_str());
    const int si = site(4);
    if (si < 0) { c.fail("expect war charred: no settlement"); return true; }
    const Site& s = g.world.sites[(size_t)si];
    int n = 0;
    for (int i = 0; i < s.bldgCount; i++) {
      const int bi = s.bldgFirst + i;
      if (bi >= 0 && bi < (int)g.world.over.bldgs.size() && g.world.over.bldgs[(size_t)bi].site == si && g.world.over.bldgs[(size_t)bi].charred) n++;
    }
    if (n < want) { if (c.waited < 3) return false; c.fail("expect war charred: " + std::to_string(n) + " in " + s.name); }
    return true;
  }
  if (what == "patrol") {
    if (g.war.patrols.empty()) { if (c.waited < 3) return false; c.fail("expect war patrol: none out"); }
    return true;
  }
  c.fail("expect war guards|militia <n> [k] | frontier [k] | camp | refugees <n> | tower | charred <n> [k] | patrol");
  return true;
}
EMB_SCRIPT_CMD("expect:war", "expect war guards|militia <n> [k] | frontier [k] | camp | refugees <n> | tower | charred <n> [k] | patrol (M4 WARDS)", expWar);

}  // namespace
