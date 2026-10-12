// rpg_test --companion, part two (FELLOWS lane; test_party.cpp calls partyWaysTests per seed): the companions' ways.
//   - each vocation in a fight: the warrior's taunt pulls a wolf off the player, the archer shoots from >= 3 tiles, the
//     healer heals, the rogue stabs from behind, the mage's bolts fly, the hound bites; skill counts at bond 80;
//   - stances: Passive takes ~nothing, Defend stays within 3 tiles of the player;
//   - never more than CATCH_UP_TILES behind on: the nearest city's gate and avenues, a terrain ramp, the inn's stairs up
//     and down, a cave in and out, fast travel, the carriage (the journey's days count together), 200 tiles on
//     horseback (a companion with a horse rides beside, Actor::posture Ride);
//   - waiting at the inn (and on the road), there again after leaving, and after a save and a load;
//   - living at a staged house (home::debugLot + an instant build): in the house with the player, a bed at night, the
//     yard by day, asked along again;
//   - determinism (two runs give the same party block); partyStep + partyActor < 0.3 ms per step with 3 members.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>
#include <utility>
#include <vector>
#include "rpg/sim/game_internal.h"
#include "rpg/sim/home.h"
#include "rpg/sim/life.h"
#include "rpg/sim/party.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

int g_fails = 0;
void fail(const std::string& m) { out("FAIL: companion: %s\n", m.c_str()); g_fails++; }
std::string sd(uint64_t s) { return "seed " + std::to_string(s) + ": "; }
constexpr float T = (float)TILE;

int compActor(const Game& g, int mi = 0) {
  for (size_t i = 1; i < g.actors.size(); i++) if (g.actors[i].comp == mi + 1 && g.actors[i].st != AState::Dead) return (int)i;
  return -1;
}
void steps(Game& g, int n, Input in = Input()) {
  for (int f = 0; f < n; f++) {
    g.update(SIM_DT, in);
    g.events.clear();
    if (g.mode != Mode::Play) g.mode = Mode::Play;
  }
}
float lag(const Game& g, int mi = 0) {
  const int ai = compActor(g, mi);
  return ai > 0 ? len(g.actors[(size_t)ai].p - g.pl().p) / T : 1e9f;
}
void newGame(Game& g, uint64_t seed) {
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.noWildSpawns = true;
  g.godMode = true;
  steps(g, 10);
}
// out in the open country, nobody about (wolves hunt townsfolk too, and the watch fights them): the fight checks
void toOpenCountry(Game& g) {
  const int32_t sx = g.world.ox + (int32_t)(g.pl().p.x / T), sy = g.world.oy + (int32_t)(g.pl().p.y / T);
  static const int off[][2] = {{90, 0}, {-90, 0}, {0, 90}, {0, -90}, {70, 70}, {-70, 70}, {70, -70}, {-70, -70}, {130, 0}, {-130, 0}};
  for (const auto& o : off) {
    g.teleportGlobal(sx + o[0], sy + o[1]);
    steps(g, 30);
    bool folk = g.settlementAt(g.pl().p) >= 0;
    for (const Actor& a : g.actors) if (a.npc && !a.comp && !a.hostile && len2(a.p - g.pl().p) < (25.0f * T) * (25.0f * T)) folk = true;
    if (!folk) break;
  }
  for (size_t i = 1; i < g.actors.size();) {
    const Actor& o = g.actors[i];
    if (o.npc && !o.comp && !o.hostile) g.actors.erase(g.actors.begin() + (std::ptrdiff_t)i); else i++;
  }
}
int recruitAt(Game& g, int si, int skip = 0) {
  const life::Census* c = g.life.census(g.world, si);
  if (!c) return -1;
  int n = 0;
  for (const life::Resident& r : c->res) {
    if ((r.flags & (life::RF_DEAD | life::RF_KEY | life::RF_AWAY)) || r.job == life::Job::Child || r.job == life::Job::Elder || r.age < 18 || r.age > 62) continue;
    if (n++ < skip) continue;
    Actor a;
    a.id = -99; a.npc = true; a.human = true; a.site = si; a.resident = r.idx; a.p = g.pl().p + Vec2(20, 0);
    return g.partyRecruit(a, party::Origin::Tavern);
  }
  return -1;
}
// a member of a given vocation (a saga's ally), bond as given
int withVoc(Game& g, party::Vocation v, int bond, uint32_t seed) {
  const int mi = g.partyRecruitStranger("TESTER", false, g.world.sites[(size_t)g.world.startSite].culture, seed, party::Origin::Saga, g.pl().p + Vec2(18, 6));
  if (mi < 0) return -1;
  party::Member& m = g.party.members[(size_t)mi];
  m.voc = v;
  m.bond = (uint8_t)bond;
  m.inv.clear();
  m.eq.fill(-1);
  PartyOps::kit(g, m, std::max(1, g.plLevel));
  const int ai = compActor(g, mi);
  if (ai > 0) {
    Actor& a = g.actors[(size_t)ai];
    a.human = v != party::Vocation::Hound;
    if (a.human) a.look = g.partyLook(m);
    PartyOps::body(g, m, a);
    a.hp = a.maxHp;
  }
  return mi;
}

// ---- the overworld path search: global tiles from the player's tile to (gx, gy), 4-connected over open ground
std::vector<std::pair<int32_t, int32_t>> pathTo(Game& g, int32_t gx, int32_t gy, bool noSwamp = false) {
  std::vector<std::pair<int32_t, int32_t>> out;
  const Map& M = g.world.over;
  const int sx = (int)std::floor(g.pl().p.x / T), sy = (int)std::floor(g.pl().p.y / T);
  const int tx = gx - g.world.ox, ty = gy - g.world.oy;
  if (!M.in(sx, sy) || !M.in(tx, ty)) return out;
  std::vector<int> prev((size_t)M.w * M.h, -1);
  std::deque<int> q;
  const int s = sy * M.w + sx, t = ty * M.w + tx;
  prev[(size_t)s] = s;
  q.push_back(s);
  static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
  while (!q.empty()) {
    const int c = q.front();
    q.pop_front();
    if (c == t) break;
    const int cx = c % M.w, cy = c / M.w;
    for (int k = 0; k < 4; k++) {
      const int nx = cx + dx[k], ny = cy + dy[k];
      if (!M.in(nx, ny) || nx < 2 || ny < 2 || nx >= M.w - 2 || ny >= M.h - 2) continue;
      const int ni = ny * M.w + nx;
      if (prev[(size_t)ni] >= 0 || M.blocked(nx, ny) || M.bldgAt[(size_t)ni] >= 0) continue;
      // (the body is wide: keep a tile off walls on both sides where the way allows)
      if (noSwamp && (M.at(nx, ny) == Ground::Swamp || M.at(nx, ny) == Ground::Ice)) continue;
      prev[(size_t)ni] = c;
      q.push_back(ni);
    }
  }
  if (prev[(size_t)t] < 0) return out;
  for (int c = t; c != s; c = prev[(size_t)c]) out.push_back({g.world.ox + c % M.w, g.world.oy + c / M.w});
  std::reverse(out.begin(), out.end());
  return out;
}
// the reachable tile farthest by path from the player within r tiles (global), false none
bool farthest(Game& g, int r, int32_t& gx, int32_t& gy, int minD) {
  const Map& M = g.world.over;
  const int sx = (int)std::floor(g.pl().p.x / T), sy = (int)std::floor(g.pl().p.y / T);
  std::vector<int> dist((size_t)M.w * M.h, -1);
  std::deque<int> q;
  dist[(size_t)(sy * M.w + sx)] = 0;
  q.push_back(sy * M.w + sx);
  int best = -1, bd = -1;
  static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
  while (!q.empty()) {
    const int c = q.front();
    q.pop_front();
    const int cx = c % M.w, cy = c / M.w;
    if (dist[(size_t)c] > bd && std::abs(cx - sx) <= r && std::abs(cy - sy) <= r) { bd = dist[(size_t)c]; best = c; }
    for (int k = 0; k < 4; k++) {
      const int nx = cx + dx[k], ny = cy + dy[k];
      if (!M.in(nx, ny) || nx < 2 || ny < 2 || nx >= M.w - 2 || ny >= M.h - 2 || std::abs(nx - sx) > r || std::abs(ny - sy) > r) continue;
      const int ni = ny * M.w + nx;
      if (dist[(size_t)ni] >= 0 || M.blocked(nx, ny) || M.bldgAt[(size_t)ni] >= 0) continue;
      if (M.at(nx, ny) == Ground::Swamp) continue;
      dist[(size_t)ni] = dist[(size_t)c] + 1;
      q.push_back(ni);
    }
  }
  if (best < 0 || bd < minD) return false;
  gx = g.world.ox + best % M.w; gy = g.world.oy + best / M.w;
  return true;
}
// walk (or ride) the player along a path of global tiles; returns the tiles walked
int walkPath(Game& g, const std::vector<std::pair<int32_t, int32_t>>& path, int maxFrames) {
  size_t k = 0;
  int frames = 0;
  while (k < path.size() && frames < maxFrames) {
    const Vec2 c((path[k].first - g.world.ox) * T + 8.0f, (path[k].second - g.world.oy) * T + 10.0f);
    const Vec2 d = c - g.pl().p;
    if (len(d) < 5.0f) { k++; continue; }
    Input in;
    in.move = norm(d);
    g.update(SIM_DT, in);
    g.events.clear();
    if (g.mode != Mode::Play) g.mode = Mode::Play;
    frames++;
  }
  return (int)k;
}

// ---------------------------------------------------------------- vocations and stances
struct Fight { double taken = 0, party = 0; int wolvesOnMember = 0; };
Fight fightWolves(Game& g, int n, int D, float secs, int mi, bool playerHits = true) {
  Fight f;
  g.party.stats = party::Stats();
  std::vector<int> ids;
  // (on clear ground: a wolf put down inside a boulder or a trunk stands there for good, out of every reach, and the
  // checks below would be measuring the rock. The world's own spawns pick open ground.)
  auto clearAt = [&](Vec2 at) {
    for (int ring = 0; ring <= 16; ring++) {
      const float d = 2.0f * (float)ring;
      const Vec2 offs[8] = {Vec2(0, -d), Vec2(d, 0), Vec2(0, d), Vec2(-d, 0), Vec2(d, -d), Vec2(d, d), Vec2(-d, d), Vec2(-d, -d)};
      for (const Vec2& o : offs) if (HomeOps::bodyFree(g, at + o, 6.0f)) return at + o;
    }
    return at;
  };
  for (int k = 0; k < n; k++) ids.push_back(g.debugSpawnAt(art::Monster::Wolf, clearAt(g.pl().p + Vec2(70.0f + 12.0f * k, -18.0f + 22.0f * k)), D));
  for (int fr = 0; fr < (int)(secs * 60); fr++) {
    Input in;
    int tgt = -1;
    float bd = 1e9f;
    for (size_t i = 1; i < g.actors.size(); i++) {
      const Actor& e = g.actors[i];
      if (!e.hostile || e.st == AState::Dead) continue;
      const float d2 = len2(e.p - g.pl().p);
      if (d2 < bd) { bd = d2; tgt = (int)i; }
    }
    if (tgt < 0) break;
    const Vec2 d = g.actors[(size_t)tgt].p - g.pl().p;
    if (playerHits) { in.move = bd > 18.0f * 18.0f ? norm(d) : Vec2(); in.attack = bd < 26.0f * 26.0f && fr % 14 == 0; }
    g.pl().aim = norm(d);
    if (!g.godMode) g.pl().hp = std::max(g.pl().hp, g.pl().maxHp * 0.5f);
    g.update(SIM_DT, in);
    g.events.clear();
    if (g.mode != Mode::Play) g.mode = Mode::Play;
    if (g.pl().st == AState::Dead) { g.pl().st = AState::Idle; g.pl().hp = g.pl().maxHp * 0.5f; }
    const int ai = compActor(g, mi);
    if (ai > 0) for (int id : ids) { const int wi = HomeOps::findActor(g, id); if (wi > 0 && g.actors[(size_t)wi].target == g.actors[(size_t)ai].id) f.wolvesOnMember++; }
    if (mi < (int)g.party.members.size() && g.party.members[(size_t)mi].state == party::State::Downed) {
      g.party.members[(size_t)mi].state = party::State::With;
      if (ai > 0) { g.actors[(size_t)ai].hp = g.actors[(size_t)ai].maxHp; g.actors[(size_t)ai].posture = art::Posture::None; }
    }
  }
  f.taken = g.party.stats.dmgPlayer + g.party.stats.dmgParty;
  f.party = g.party.stats.dmgParty;
  return f;
}

void vocations(uint64_t seed) {
  using party::Vocation;
  for (int v = 0; v < (int)Vocation::COUNT; v++) {
    Game g(seed);
    newGame(g, seed);
    toOpenCountry(g);
    g.plLevel = 10;
    g.debugKit();
    const int mi = withVoc(g, (Vocation)v, 80, 1000u + (uint32_t)v);
    if (mi < 0) { fail(sd(seed) + "no member of vocation " + party::vocationName((Vocation)v)); continue; }
    steps(g, 30);
    if ((Vocation)v == Vocation::Healer) g.pl().hp = g.pl().maxHp * 0.4f;
    const float hp0 = g.pl().hp;
    const Fight f = fightWolves(g, 3, 10, 20.0f, mi, false);
    const party::Stats& s = g.party.stats;
    out("companion: %s%s: arrows %d bolts %d heals %d backstabs %d bites %d | taunts %d walls %d stands %d pins %d volleys %d eagle %d wards %d storms %d "
        "purify %d flanks %d smokes %d assassinations %d guards %d pindowns %d; archer nearest %.1f tiles\n",
        sd(seed).c_str(), party::vocationName((Vocation)v), s.arrows, s.bolts, s.heals, s.backstabs, s.bites, s.taunts, s.shieldWalls, s.lastStands, s.pins,
        s.volleys, s.eagleEyes, s.wards, s.storms, s.purifies, s.flanks, s.smokes, s.assassinations, s.guards, s.pinDowns, s.archerMinTiles == 1e9f ? 0.0f : s.archerMinTiles);
    switch ((Vocation)v) {
      case Vocation::Warrior:
        if (s.taunts < 1) fail(sd(seed) + "the warrior never taunted");
        if (f.wolvesOnMember < 30) fail(sd(seed) + "a taunt did not pull a wolf off the player");
        break;
      case Vocation::Archer:
        if (s.arrows < 3) fail(sd(seed) + "the archer did not shoot");
        if (s.archerMinTiles < 3.0f) fail(sd(seed) + "the archer let a foe within " + std::to_string(s.archerMinTiles) + " tiles while shooting");
        if (s.pins + s.volleys + s.eagleEyes < 1) fail(sd(seed) + "the archer's bond skills did not fire");
        break;
      case Vocation::Mage:
        if (s.bolts < 2) fail(sd(seed) + "the mage cast no bolts");
        break;
      case Vocation::Healer:
        if (s.heals < 1 || g.pl().hp <= hp0) fail(sd(seed) + "the healer did not heal the hurt player");
        break;
      case Vocation::Rogue:
        if (s.backstabs + s.flanks < 1) fail(sd(seed) + "the rogue never struck from behind");
        break;
      case Vocation::Hound:
        if (s.bites < 2) fail(sd(seed) + "the hound did not bite");
        break;
      default: break;
    }
  }
  // the hound's Guard stands over a downed companion; the healer's Second Wind raises them from afar
  for (int k = 0; k < 2; k++) {
    Game g(seed);
    newGame(g, seed);
    toOpenCountry(g);
    g.plLevel = party::LEADERSHIP_LEVEL;
    const int w = withVoc(g, Vocation::Warrior, 30, 77);
    std::string why;
    const int helper = k == 0 ? g.partyAdoptAnimal(0, 79, g.pl().p + Vec2(-16, 0), why) : withVoc(g, Vocation::Healer, 80, 78);
    if (w < 0 || helper < 0) { fail(sd(seed) + "the guard party could not be formed"); return; }
    g.party.members[(size_t)helper].flags = (uint16_t)(g.party.members[(size_t)helper].flags & ~party::MF_PUP);
    if (k == 0) { g.party.members[(size_t)helper].bond = 30; const int ha = compActor(g, helper); if (ha > 0) PartyOps::body(g, g.party.members[(size_t)helper], g.actors[(size_t)ha]); }
    steps(g, 30);
    const int wa = compActor(g, w);
    g.debugFell(g.actors[(size_t)wa].id);
    steps(g, 60 * 5);
    if (k == 0 && g.party.stats.guards < 1) fail(sd(seed) + "the hound did not guard the downed warrior");
    if (k == 1 && (g.party.stats.secondWinds < 1 || g.party.members[(size_t)w].state != party::State::With)) fail(sd(seed) + "the healer's Second Wind did not raise the downed warrior");
  }
}

void stances(uint64_t seed) {
  {
    Game g(seed);
    newGame(g, seed);
    toOpenCountry(g);
    g.plLevel = 8;
    g.debugKit();
    const int mi = withVoc(g, party::Vocation::Warrior, 0, 501);
    g.partyCommand(mi, party::Cmd::Passive);
    g.godMode = false;   // (the player takes their blows: the share is of both)
    steps(g, 30);
    const Fight f = fightWolves(g, 3, 7, 20.0f, mi);
    const int pct = f.taken > 0 ? (int)std::lround(100.0 * f.party / f.taken) : 0;
    out("companion: %sPassive: took %d %% of %.0f damage\n", sd(seed).c_str(), pct, f.taken);
    if (pct > 8) fail(sd(seed) + "a Passive companion took " + std::to_string(pct) + " % of the damage");
  }
  for (int v : {(int)party::Vocation::Warrior, (int)party::Vocation::Archer}) {
    Game g(seed);
    newGame(g, seed);
    toOpenCountry(g);
    g.plLevel = 8;
    g.debugKit();
    const int mi = withVoc(g, (party::Vocation)v, 0, 502u + (uint32_t)v);
    g.partyCommand(mi, party::Cmd::Defend);
    steps(g, 30);
    (void)fightWolves(g, 3, 7, 20.0f, mi);
    out("companion: %sDefend (%s): farthest %.1f tiles from the player, %d intercepts\n", sd(seed).c_str(), party::vocationName((party::Vocation)v),
        g.party.stats.defendMaxTiles, g.party.stats.intercepts);
    if (g.party.stats.defendMaxTiles > 3.3f) fail(sd(seed) + "a Defend companion strayed " + std::to_string(g.party.stats.defendMaxTiles) + " tiles from the player");
  }
}

// ---------------------------------------------------------------- the paths
void paths(uint64_t seed) {
  Game g(seed);
  newGame(g, seed);
  g.gold = 50000;
  if (recruitAt(g, g.world.startSite) != 0) { fail(sd(seed) + "paths: no recruit"); return; }
  steps(g, 20);
  float worst = 0;
  int catchUps = 0;
  auto note = [&](const char* what) {
    worst = std::max(worst, g.party.stats.worstLagTiles);
    catchUps += g.party.stats.catchUps;
    const float l = lag(g);
    if (l > (float)party::CATCH_UP_TILES || g.party.stats.worstLagTiles > (float)party::CATCH_UP_TILES + 0.6f)
      fail(sd(seed) + what + ": the companion was " + std::to_string(std::max(l, g.party.stats.worstLagTiles)) + " tiles behind");
    out("companion: %s%s: worst lag %.1f tiles, %d catch-ups, now %.1f tiles\n", sd(seed).c_str(), what, g.party.stats.worstLagTiles, g.party.stats.catchUps, l);
    g.party.stats.worstLagTiles = 0; g.party.stats.catchUps = 0;
  };
  // ---- the nearest city: from far outside its walls to its heart (through a gate, along the avenues)
  {
    const ew::GTile st = g.world.src->start().spawn;
    std::vector<ew::SettlementNode> nodes = g.world.src->settlementsIn(st.x - 1200, st.y - 1200, st.x + 1200, st.y + 1200);
    const ew::SettlementNode* city = nullptr;
    int64_t bd = -1;
    for (const ew::SettlementNode& n : nodes) {
      if (n.type != SiteType::City) continue;
      const int64_t d = (int64_t)(n.x - st.x) * (n.x - st.x) + (int64_t)(n.y - st.y) * (n.y - st.y);
      if (bd < 0 || d < bd) { bd = d; city = &n; }
    }
    if (!city) out("companion: %sno city within 1200 tiles\n", sd(seed).c_str());
    else {
      g.teleportGlobal(city->x, city->y + 2);
      steps(g, 30);
      int32_t ox = 0, oy = 0;
      if (farthest(g, 60, ox, oy, 70)) {
        g.teleportGlobal(ox, oy);
        steps(g, 40);
        g.party.stats.worstLagTiles = 0; g.party.stats.catchUps = 0;
        const auto path = pathTo(g, city->x, city->y + 2);
        const int walked = walkPath(g, path, 60 * 120);
        steps(g, 90);
        out("companion: %sthe city %s: %d of %d tiles walked to its heart\n", sd(seed).c_str(), city->name.c_str(), walked, (int)path.size());
        note("the city's gate and avenues");
      } else out("companion: %sno open ground far outside the city\n", sd(seed).c_str());
    }
  }
  // ---- a terrain ramp: from the low ground up onto the high
  {
    g.teleportGlobal(g.world.src->start().spawn.x, g.world.src->start().spawn.y);
    steps(g, 30);
    const Map& M = g.world.over;
    int rx = -1, ry = -1;
    const int px = (int)(g.pl().p.x / T), py = (int)(g.pl().p.y / T);
    int bestD = 1 << 30;
    for (int y = 20; y < M.h - 20; y++)
      for (int x = 20; x < M.w - 20; x++)
        if ((M.heightBits(x, y) & Map::HEIGHT_RAMP) && !M.blocked(x, y)) {
          const int d = std::abs(x - px) + std::abs(y - py);
          if (d < bestD) { bestD = d; rx = x; ry = y; }
        }
    if (rx < 0) out("companion: %sno terrain ramp in the window\n", sd(seed).c_str());
    else {
      // a low tile and a high tile either side of it
      int lo = -1, hi = -1, loH = 99, hiH = -1;
      for (int d = 6; d <= 10; d++)
        for (int k = 0; k < 4; k++) {
          static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
          const int x = rx + dx[k] * d, y = ry + dy[k] * d;
          if (!M.in(x, y) || M.blocked(x, y)) continue;
          const int h = M.heightAt(x, y);
          if (h < loH) { loH = h; lo = y * M.w + x; }
          if (h > hiH) { hiH = h; hi = y * M.w + x; }
        }
      if (lo < 0 || hi < 0 || loH == hiH) out("companion: %sthe ramp at %d,%d has no level change round it\n", sd(seed).c_str(), rx, ry);
      else {
        const int32_t hgx = g.world.ox + hi % M.w, hgy = g.world.oy + hi / M.w;
        g.teleportGlobal(g.world.ox + lo % M.w, g.world.oy + lo / M.w);
        steps(g, 40);
        g.party.stats.worstLagTiles = 0; g.party.stats.catchUps = 0;
        const auto path = pathTo(g, hgx, hgy);
        const int walked = walkPath(g, path, 60 * 40);
        steps(g, 90);
        out("companion: %sthe ramp: %d of %d tiles, level %d to %d\n", sd(seed).c_str(), walked, (int)path.size(), loH, hiH);
        note("a terrain ramp");
      }
    }
  }
  // ---- the inn's stairs: up and down again
  {
    const Site& home = g.world.sites[(size_t)g.world.startSite];
    int inn = -1;
    for (int b = home.bldgFirst; b < home.bldgFirst + home.bldgCount && inn < 0; b++)
      if (g.world.over.bldgs[(size_t)b].type == art::Building::Inn && g.world.over.bldgs[(size_t)b].floors() > 1) inn = b;
    if (inn >= 0 && g.debugEnterBuilding(inn, 0)) {
      steps(g, 20);
      g.changeFloor(1);
      steps(g, 60);
      if (lag(g) > 5.0f) fail(sd(seed) + "the companion did not come up the inn's stairs (" + std::to_string(lag(g)) + " tiles)");
      g.changeFloor(0);
      steps(g, 60);
      if (lag(g) > 5.0f) fail(sd(seed) + "the companion did not come down the inn's stairs (" + std::to_string(lag(g)) + " tiles)");
      g.debugLeave();
      steps(g, 20);
      note("the inn's stairs");
    }
  }
  // ---- a cave in and out
  {
    int cave = -1;
    for (size_t i = 0; i < g.world.sites.size() && cave < 0; i++) if (g.world.sites[i].type == SiteType::Cave) cave = (int)i;
    if (cave >= 0 && g.debugEnterSite(cave)) {
      steps(g, 30);
      if (lag(g) > 4.0f) fail(sd(seed) + "the companion did not come into the cave");
      g.debugLeave();
      steps(g, 30);
      if (lag(g) > 4.0f) fail(sd(seed) + "the companion did not come out of the cave");
      note("a cave in and out");
    }
  }
  // ---- fast travel and the carriage (the journey's days count as days together)
  {
    int dest = -1;
    for (size_t i = 0; i < g.world.sites.size(); i++)
      if (g.world.sites[i].settlement() && (int)i != g.world.startSite) { dest = (int)i; break; }
    if (dest >= 0) {
      g.world.sites[(size_t)dest].discovered = true;
      if (g.fastTravel(dest)) {
        steps(g, 40);
        if (lag(g) > 4.0f) fail(sd(seed) + "the companion did not come along on a fast travel");
        note("fast travel");
      }
      g.world.sites[(size_t)g.world.startSite].discovered = true;
      const uint32_t days0 = g.party.members[0].days;
      const int day0 = g.day;
      g.hour = 20.0f;
      if (g.beginTravel(g.world.startSite, true)) {
        // (the gather waits for the streamer and its cap is real time: a frame count alone gave up before the
        // destination was gathered on a fast machine, leaving the player on the road for the ride below)
        const auto t0 = std::chrono::steady_clock::now();
        while (g.travelling() && std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() < 20.0) {
          const auto s0 = std::chrono::steady_clock::now();
          g.update(SIM_DT, Input()); g.events.clear(); if (g.mode != Mode::Play) g.mode = Mode::Play;
          while (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - s0).count() < 1.0) {}
        }
        if (g.travelling()) fail(sd(seed) + "the carriage never arrived");
        steps(g, 40);
        if (lag(g) > 4.0f) fail(sd(seed) + "the companion did not come along in the carriage");
        if (g.day > day0 && g.party.members[0].days <= days0) fail(sd(seed) + "the carriage's days on the road did not count as days together");
        note("the carriage");
      }
    }
  }
  // ---- 200 tiles on horseback: the companion with their own horse rides beside
  {
    const int32_t px = g.world.ox + (int32_t)(g.pl().p.x / T), py = g.world.oy + (int32_t)(g.pl().p.y / T);
    const int pi = home::debugLot(g, px + 3, py - 5, home::LotSize::Medium);
    std::string why;
    bool ok = pi >= 0;
    if (ok) {
      const home::Plot& P = g.home.plots[(size_t)pi];
      bool st = false;
      for (int y = 1; y + 3 < P.h && !st; y++)
        for (int x = 1; x + 3 < P.w && !st; x++)
          if (home::canPlaceOutside(P, home::Obj::Stable, x, y, false, why)) st = home::placeObj(g, pi, home::Obj::Stable, x, y, false, false, why);
      ok = st && home::buyAnimal(g, pi, home::Animal::Horse, why) && home::mount(g, pi, (int)g.home.plots[(size_t)pi].animals.size() - 1, why);
    }
    if (!ok) fail(sd(seed) + "no horse to ride: " + why);
    else {
      if (!g.partyBuyHorse(0, g.home.ridingBreed, why)) fail(sd(seed) + "no horse for the companion: " + why);
      steps(g, 20);
      int32_t tx = 0, ty = 0;
      g.party.stats.worstLagTiles = 0; g.party.stats.catchUps = 0;
      int rode = 0, ride0x = g.world.ox + (int)(g.pl().p.x / T), ride0y = g.world.oy + (int)(g.pl().p.y / T);
      bool mountedSeen = false;
      for (int leg = 0; leg < 4 && rode < 200; leg++) {
        if (!g.homeRiding()) break;
        if (!farthest(g, 70, tx, ty, 40)) break;
        const auto path = pathTo(g, tx, ty, true);
        size_t k = 0;
        for (int f = 0; f < 60 * 60 && k < path.size() && g.homeRiding(); f++) {
          const Vec2 c((path[k].first - g.world.ox) * T + 8.0f, (path[k].second - g.world.oy) * T + 10.0f);
          const Vec2 d = c - g.pl().p;
          if (len(d) < 6.0f) { k++; continue; }
          Input in;
          in.move = norm(d);
          g.update(SIM_DT, in);
          g.events.clear();
          if (g.mode != Mode::Play) g.mode = Mode::Play;
          const int ai = compActor(g);
          if (ai > 0 && g.actors[(size_t)ai].posture == art::Posture::Ride) mountedSeen = true;
        }
        const int cx = g.world.ox + (int)(g.pl().p.x / T), cy = g.world.oy + (int)(g.pl().p.y / T);
        rode = std::abs(cx - ride0x) + std::abs(cy - ride0y) > rode ? std::abs(cx - ride0x) + std::abs(cy - ride0y) : rode;
      }
      steps(g, 60);
      out("companion: %shorseback: %d tiles from the start, companion mounted %s\n", sd(seed).c_str(), rode, mountedSeen ? "yes" : "no");
      if (rode < 150) out("companion: %s(the ride ended early: %s)\n", sd(seed).c_str(), g.homeRiding() ? "no more open ground" : "dismounted");
      if (!mountedSeen) fail(sd(seed) + "the companion with a horse did not ride beside the mounted player");
      note("on horseback");
      g.homeDismount(home::Dismount::Player);
      steps(g, 5);
      const int ai = compActor(g);
      if (ai > 0 && g.actors[(size_t)ai].posture == art::Posture::Ride) fail(sd(seed) + "the companion did not get down with the player");
    }
  }
  out("companion: %spaths: worst lag %.1f tiles, %d catch-ups in all\n", sd(seed).c_str(), worst, catchUps);
}

// ---------------------------------------------------------------- waiting, the house
void waitingAndHouse(uint64_t seed) {
  Game g(seed);
  newGame(g, seed);
  if (recruitAt(g, g.world.startSite) != 0) { fail(sd(seed) + "waiting: no recruit"); return; }
  steps(g, 20);
  const Site home = g.world.sites[(size_t)g.world.startSite];
  int inn = -1;
  for (int b = home.bldgFirst; b < home.bldgFirst + home.bldgCount && inn < 0; b++)
    if (g.world.over.bldgs[(size_t)b].type == art::Building::Inn) inn = b;
  if (inn >= 0 && g.debugEnterBuilding(inn, 0)) {
    steps(g, 30);
    const ew::Gid innId = g.world.over.bldgs[(size_t)inn].id;
    if (!g.partyCommand(0, party::Cmd::Wait)) fail(sd(seed) + "the companion would not wait at the inn");
    const int wx = g.party.members[0].gx, wy = g.party.members[0].gy;
    g.debugLeave();
    steps(g, 20);
    if (compActor(g) > 0) fail(sd(seed) + "a companion waiting at the inn came out with the player");
    {
      Input in; in.move = Vec2(1, 0);
      steps(g, 60 * 4, in);
    }
    g.debugEnterBuilding(inn, 0);
    steps(g, 20);
    int ai = compActor(g);
    if (ai < 0 || std::abs((int)(g.actors[(size_t)ai].p.x / T) - wx) > 2 || std::abs((int)(g.actors[(size_t)ai].p.y / T) - wy) > 2)
      fail(sd(seed) + "the companion was not waiting where they were told at the inn");
    // after a save and a load
    g.debugLeave();
    steps(g, 10);
    std::vector<uint8_t> a;
    g.serialize(a);
    Game h(1);
    if (!h.deserialize(a)) fail(sd(seed) + "a save with a companion waiting does not load");
    else {
      h.mode = Mode::Play;
      steps(h, 10);
      const int bi = h.world.bldgHandle(innId);
      if (bi < 0 || !h.debugEnterBuilding(bi, 0)) fail(sd(seed) + "the inn is not in the loaded world");
      else {
        steps(h, 20);
        if (compActor(h) < 0 || h.party.members[0].state != party::State::Waiting) fail(sd(seed) + "after a load the companion is not waiting at the inn");
        if (!h.partyCommand(0, party::Cmd::Follow)) fail(sd(seed) + "the waiting companion would not come along again");
        h.debugLeave();
        steps(h, 20);
        if (compActor(h) < 0 || lag(h) > 4.0f) fail(sd(seed) + "the companion asked along again did not follow out of the inn");
      }
    }
    g.debugEnterBuilding(inn, 0);
    steps(g, 10);
    g.partyCommand(0, party::Cmd::Follow);
    g.debugLeave();
    steps(g, 20);
  }
  // ---- waiting on the road
  {
    g.partyCommand(0, party::Cmd::Wait);
    const int wx = g.party.members[0].gx, wy = g.party.members[0].gy;
    const int32_t bx = g.world.ox + (int)(g.pl().p.x / T), by = g.world.oy + (int)(g.pl().p.y / T);
    int32_t fx = 0, fy = 0;
    if (!farthest(g, 60, fx, fy, 50)) out("companion: %sno open road to walk away along\n", sd(seed).c_str());
    else {
      walkPath(g, pathTo(g, fx, fy), 60 * 60);
      steps(g, 30);
      if (compActor(g) > 0 && lag(g) < 10.0f) fail(sd(seed) + "a companion told to wait followed anyway");
      walkPath(g, pathTo(g, bx, by), 60 * 60);
      steps(g, 30);
      const int ai = compActor(g);
      if (ai < 0 || std::abs(g.world.ox + (int)(g.actors[(size_t)ai].p.x / T) - wx) > 2 || std::abs(g.world.oy + (int)(g.actors[(size_t)ai].p.y / T) - wy) > 2)
        fail(sd(seed) + "the companion was not where they were told to wait on the road");
    }
    g.partyCommand(0, party::Cmd::Follow);
    steps(g, 30);
  }
  // ---- living at the player's house (a staged lot, an instant build)
  {
    const int32_t px = g.world.ox + (int32_t)(g.pl().p.x / T), py = g.world.oy + (int32_t)(g.pl().p.y / T);
    const int pi = home::debugLot(g, px + 3, py - 6, home::LotSize::Medium);
    if (pi < 0) { fail(sd(seed) + "debugLot refused"); return; }
    const uint64_t cul = g.world.src->cultureAt(px, py);
    if (cul && !g.home.knowsStyle(cul)) g.home.styles.push_back(cul);
    g.gold = 100000;
    std::string why;
    if (!home::startBuild(g, pi, home::Shell::Cottage, cul, 0, 0, why)) { fail(sd(seed) + "startBuild: " + why); return; }
    g.update(SIM_DT, Input());
    g.day += home::shellInfo(home::Shell::Cottage).days;
    steps(g, 3);
    if (g.home.plots[(size_t)pi].state != home::PlotState::Built) { fail(sd(seed) + "the staged house was not built"); return; }
    if (!g.partyCommand(0, party::Cmd::Home)) fail(sd(seed) + "the companion would not go to live at the player's house");
    const party::Member& m = g.party.members[0];
    if (m.state != party::State::Home || !m.homePlot || PartyOps::homePlotOf(g, m) != pi) fail(sd(seed) + "the companion is not living at the player's house");
    steps(g, 60);
    const Bldg hb = home::houseBldg(g.home.plots[(size_t)pi], g);
    const int bi = g.world.bldgHandle(hb.id);
    if (bi < 0 || !g.debugEnterBuilding(bi, 0)) { fail(sd(seed) + "the staged house cannot be entered"); return; }
    g.hour = 23.0f;
    steps(g, 30);
    int ai = compActor(g);
    if (ai < 0) fail(sd(seed) + "the companion is not in the house when the player is");
    else {
      const art::Posture ps = g.actors[(size_t)ai].posture;
      out("companion: %sat the house by night: posture %d at %d,%d\n", sd(seed).c_str(), (int)ps, g.actors[(size_t)ai].useX, g.actors[(size_t)ai].useY);
      if (ps != art::Posture::Sleep && ps != art::Posture::SitFloor) fail(sd(seed) + "at night the companion at the house does not lie down");
    }
    g.hour = 10.0f;
    g.debugLeave();
    steps(g, 30);
    ai = compActor(g);
    if (ai < 0) fail(sd(seed) + "by day the companion is not out in the yard");
    else {
      const home::Plot& P = g.home.plots[(size_t)pi];
      const int tx = g.world.ox + (int)(g.actors[(size_t)ai].p.x / T), ty = g.world.oy + (int)(g.actors[(size_t)ai].p.y / T);
      if (tx < P.gx - 1 || ty < P.gy - 1 || tx > P.gx + P.w || ty > P.gy + P.h) fail(sd(seed) + "the companion at home by day is not in the yard");
    }
    if (!g.partyCommand(0, party::Cmd::Follow)) fail(sd(seed) + "the companion at the house would not come along again");
    {
      Input in; in.move = Vec2(0, 1);
      steps(g, 60 * 3, in);
    }
    if (g.party.members[0].state != party::State::With || lag(g) > 6.0f) fail(sd(seed) + "the companion did not leave the house with the player");
  }
}

// ---------------------------------------------------------------- determinism and cost
std::vector<uint8_t> scripted(uint64_t seed) {
  Game g(seed);
  newGame(g, seed);
  g.godMode = false;
  g.debugKit();
  g.plLevel = 6;
  recruitAt(g, g.world.startSite);
  Input in; in.move = Vec2(1, 0);
  steps(g, 300, in);
  (void)fightWolves(g, 2, 5, 10.0f, 0);
  g.deed(party::Deed::ClearedBandits, 5);
  g.deed(party::Deed::Prayed, 7);
  steps(g, 30);
  std::vector<uint8_t> b;
  g.party.serialize(b);
  return b;
}

void costAndDeterminism(uint64_t seed) {
  if (scripted(seed) != scripted(seed)) fail(sd(seed) + "two runs of the same play give different party blocks");
  Game g(seed);
  newGame(g, seed);
  g.plLevel = party::LEADERSHIP_LEVEL;
  g.debugKit();
  recruitAt(g, g.world.startSite);
  recruitAt(g, g.world.startSite, 1);
  std::string why;
  g.partyAdoptAnimal(0, 4242, g.pl().p + Vec2(-14, 0), why);
  if (g.party.with() != 3) { fail(sd(seed) + "the cost run has " + std::to_string(g.party.with()) + " members, not 3"); return; }
  g.party.stats = party::Stats();
  Input in; in.move = Vec2(1, 0.3f);
  steps(g, 60 * 20, in);
  (void)fightWolves(g, 3, 18, 20.0f, 0);
  const party::Stats& s = g.party.stats;
  const double avg = s.stepN ? s.stepMsSum / s.stepN : 0;
  out("companion: %scost with 3 members: %.4f ms per step on average (worst %.3f ms; banter worst %.3f ms) over %d steps\n", sd(seed).c_str(), avg, s.stepMsWorst, s.banterMsWorst, s.stepN);
  if (avg > 0.3) fail(sd(seed) + "partyStep + partyActor cost " + std::to_string(avg) + " ms per step with 3 members");
}

// (fixer M8 r2) a raid's captive led home and recruited at the end of the Missing quest joins as their census self (their
// home, kin and history; one person, not a stranger in the party and the same name walking at home)
void rescuedCaptive(uint64_t seed) {
  Game g;
  newGame(g, seed);
  const int si = g.world.startSite;
  life::Census* c = si >= 0 ? g.life.census(g.world, si) : nullptr;
  if (!c) { fail(sd(seed) + "captive: no census at the start village"); return; }
  int ix = -1;
  for (const life::Resident& r : c->res)
    if (!(r.flags & (life::RF_DEAD | life::RF_KEY | life::RF_AWAY)) && r.job != life::Job::Child && r.age >= 18 && r.age <= 62) { ix = (int)r.idx; break; }
  if (ix < 0) { fail(sd(seed) + "captive: nobody to take"); return; }
  c->takenIdx = (int16_t)ix;
  c->res[(size_t)ix].flags |= life::RF_AWAY;
  Quest q;
  q.id = 987001; q.type = QType::Missing; q.state = QState::Complete; q.flags |= QF_ESCORTED;
  q.giverSite = si; q.giverId = g.world.sites[(size_t)si].id; q.subject = c->res[(size_t)ix].name; q.giverName = "THE ELDER";
  g.quests.push_back(q);
  c->takenQuest = q.id;
  Actor a;
  a.id = -97; a.npc = true; a.human = true; a.resident = -1; a.quest = q.id; a.name = q.subject; a.p = g.pl().p + Vec2(18, 0);
  const int mi = g.partyRecruit(a, party::Origin::Rescued);
  if (mi < 0) { fail(sd(seed) + "captive: the rescued one would not join"); return; }
  const party::Member& m = g.party.members[(size_t)mi];
  const life::Resident& r = c->res[(size_t)ix];
  if (m.res != ix || m.npc != life::npcId(c->site, r.idx)) fail(sd(seed) + "captive: joined as a stranger, not as " + r.name + " of the census");
  steps(g, 60);
  if (!(r.flags & life::RF_AWAY) || !(r.flags & life::RF_COMPANION)) fail(sd(seed) + "captive: " + r.name + " is back at home while travelling with the player");
  if (c->takenIdx >= 0) fail(sd(seed) + "captive: the raid's thread is still open");
  out("companion: %scaptive %s joined as the census person (res %d, origin %s)\n", sd(seed).c_str(), r.name.c_str(), (int)m.res, party::originName(m.origin));
}

}  // namespace

int partyWaysTests(uint64_t seed) {
  g_fails = 0;
  vocations(seed);
  stances(seed);
  paths(seed);
  waitingAndHouse(seed);
  costAndDeterminism(seed);
  rescuedCaptive(seed);
  return g_fails;
}
