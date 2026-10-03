// rpg_test lane checks: factions, town defence, bounty clarity, the opening and the background traits. M0 town-defence lane.
// Called once per seed after runSeed; returns failures, reports each with out("FAIL: ...").
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "tools/tests/tests.h"

namespace {
// the nearest site of a type to the start village (exclude: skip this one)
int nearestOf(const Game& g, SiteType t, int exclude = -1) {
  const Site& h = g.world.sites[g.world.startSite];
  int best = -1;
  float bd = 1e30f;
  for (int i = 0; i < (int)g.world.sites.size(); i++) {
    const Site& s = g.world.sites[i];
    if (s.type != t || i == exclude) continue;
    float d = std::hypot((float)(s.ex - h.ex), (float)(s.ey - h.ey));
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}

// an open overworld tile near (tx, ty): its whole 3x3 block walkable
bool freeTile(const Game& g, int tx, int ty, int& ox, int& oy, int maxR = 14) {
  const Map& m = g.world.over;
  for (int r = 0; r <= maxR; r++)
    for (int dy = -r; dy <= r; dy++)
      for (int dx = -r; dx <= r; dx++) {
        if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
        int x = tx + dx, y = ty + dy;
        bool ok = true;
        for (int k = -1; k <= 1 && ok; k++) for (int j = -1; j <= 1 && ok; j++) if (m.blocked(x + j, y + k) || m.bldgAt.empty() || (m.in(x + j, y + k) && m.bldgAt[(size_t)(y + k) * m.w + x + j] >= 0)) ok = false;
        if (ok) { ox = x; oy = y; return true; }
      }
  return false;
}
Vec2 tileCentre(int x, int y) { return Vec2(x * TILE + 8.0f, y * TILE + 10.0f); }

void tick(Game& g, int frames, Input in = Input()) {
  for (int f = 0; f < frames; f++) {
    g.update(SIM_DT, in);
    if (g.mode == Mode::Dialogue || g.mode == Mode::Shop || g.mode == Mode::LevelUp) g.mode = Mode::Play;
    if (g.mode == Mode::Dead) g.respawn();
  }
}

// stand the player somewhere `dist` tiles from a settlement: close enough that its people are out, far enough that
// monsters inside pick on the townsfolk instead of the player
bool standNear(Game& g, int si, float dist) {
  const Site& s = g.world.sites[si];
  for (int k = 0; k < 16; k++) {
    float a = k * 0.3927f;
    int tx = s.r.cx() + (int)std::lround(std::cos(a) * dist), ty = s.r.cy() + (int)std::lround(std::sin(a) * dist);
    int x, y;
    if (!g.world.over.in(tx, ty) || !freeTile(g, tx, ty, x, y, 4)) continue;
    if (g.world.siteAt(x, y, 2) >= 0) continue;
    g.pl().p = tileCentre(x, y);
    tick(g, 3);
    return true;
  }
  return false;
}

bool enterBuilding(Game& g, int b) {
  const Bldg& B = g.world.over.bldgs[b];
  g.pl().p = Vec2(B.doorX() * 16 + 8.0f, B.doorY() * 16 + 10.0f);
  g.update(SIM_DT, Input());
  return g.inside && g.subBldg == b;
}
void leaveInside(Game& g) {
  for (int k = 0; k < 4 && g.inside; k++) {
    g.pl().p = Vec2(g.sub.exitX * 16 + 8.0f, (g.sub.exitY - 2) * 16 + 8.0f);
    g.update(SIM_DT, Input());
    g.pl().p = Vec2(g.sub.exitX * 16 + 8.0f, g.sub.exitY * 16 + 4.0f);
    Input down; down.move = Vec2(0, 1);
    for (int f = 0; f < 4 && g.inside; f++) g.update(SIM_DT, down);
  }
}
int findRole(const Game& g, Role r) {
  for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].npc && g.actors[k].role == r && g.actors[k].st != AState::Dead) return (int)k;
  return -1;
}
// open a dialogue with actors[k]; true when it is them talking
bool talk(Game& g, int k) {
  int id = g.actors[k].id;
  static const Vec2 offs[] = {{0, 12}, {0, 20}, {10, 0}, {-10, 0}, {0, -10}};
  for (Vec2 o : offs) {
    int kk = -1;
    for (size_t j = 1; j < g.actors.size(); j++) if (g.actors[j].id == id) kk = (int)j;
    if (kk < 0) return false;
    g.mode = Mode::Play;
    g.pl().p = g.actors[kk].p + o;
    Input in; in.interact = true;
    g.update(SIM_DT, in);
    if (g.mode == Mode::Dialogue && g.dlg.actor == id) return true;
  }
  return false;
}
int optIndex(const Game& g, const char* contains) {
  for (size_t o = 0; o < g.dlg.opts.size(); o++) if (g.dlg.opts[o].label.find(contains) != std::string::npos) return (int)o;
  return -1;
}
bool eventHas(const Game& g, const char* text) {
  for (const Event& e : g.events) if (e.s.find(text) != std::string::npos) return true;
  return false;
}
// a bandit camp's chief burns down (kill() runs the clear logic exactly as a player kill would)
bool killChief(Game& g, int camp) {
  const Site& s = g.world.sites[camp];
  int x, y;
  if (!freeTile(g, s.r.cx(), s.r.y + s.r.h + 2, x, y, 8)) return false;
  g.pl().p = tileCentre(x, y);
  tick(g, 3);
  for (int tries = 0; tries < 400; tries++) {
    int chief = -1;
    for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].boss && g.actors[k].site == camp && g.actors[k].st != AState::Dead) chief = (int)k;
    if (chief < 0) return g.world.sites[camp].cleared;
    g.actors[chief].hp = std::min(g.actors[chief].hp, 0.4f);
    g.actors[chief].burnT = 2.0f;
    g.actors[chief].reassembled = true;
    g.update(SIM_DT, Input());
    if (g.mode != Mode::Play) g.mode = Mode::Play;
  }
  return g.world.sites[camp].cleared;
}

// ---------------------------------------------------------------- town defence
// 3 wolves loose in a guarded town: the bell rings, the guards converge and kill them within 30 s with at most one
// villager down, townsfolk run home and come back out ~30 s after it is over, nobody gets stuck.
int townDefence(const Game& base, int si, bool guarded, const char* what) {
  int bad = 0;
  Game g = base;
  g.godMode = true; g.noWildSpawns = true; g.hour = 12;
  if (!standNear(g, si, 26)) { out("WARN: defence (%s): no spot near %s\n", what, g.world.sites[si].name.c_str()); return 0; }
  const Site& st = g.world.sites[si];
  int guards = 0, folk = 0, militia = 0;
  std::vector<int> spots;
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    if (!a.npc || a.site != si) continue;
    if (a.role == Role::Guard) guards++;
    else { folk++; if (a.militia) militia++; spots.push_back((int)k); }
  }
  if (folk == 0) { out("WARN: defence (%s): %s has no townsfolk out\n", what, st.name.c_str()); return 0; }
  if (guarded && guards == 0) { out("FAIL: defence: %s has no guards out\n", st.name.c_str()); return 1; }
  // three wolves in the street among the townsfolk, at whoever stands nearest the middle of town
  Vec2 mid((st.r.x + st.r.w * 0.5f) * TILE, (st.r.y + st.r.h * 0.5f) * TILE);
  std::sort(spots.begin(), spots.end(), [&](int x, int y) { return len2(g.actors[(size_t)x].p - mid) < len2(g.actors[(size_t)y].p - mid); });
  Vec2 at = g.actors[(size_t)spots[0]].p;
  int lvl = std::max(1, g.world.zoneLevel(st.ex, st.ey));
  std::vector<int> wolves;
  for (int k = 0; k < 3; k++) wolves.push_back(g.debugSpawnAt(art::Monster::Wolf, at + Vec2((k - 1) * 10.0f, -4.0f), lvl));
  std::set<int> downed;
  bool rang = false;
  int maxHidden = 0;
  float clearedAt = -1;
  float worstFlee = 0;
  std::map<int, float> guardNoProgress;   // guard id -> seconds chasing without closing in
  std::map<int, float> guardBest;
  for (int f = 0; f < 30 * 60; f++) {
    g.update(SIM_DT, Input());
    g.events.clear();
    if (g.mode != Mode::Play) g.mode = Mode::Play;
    if (g.alarmSite == si) rang = true;
    maxHidden = std::max(maxHidden, g.shelteredCount(si));
    int alive = 0;
    for (int id : wolves)
      for (const Actor& a : g.actors) if (a.id == id && a.st != AState::Dead) alive++;
    for (const Actor& a : g.actors) {
      if (!a.npc || a.site != si) continue;
      if (a.st == AState::Dead && a.role != Role::Guard) downed.insert(a.id);
      worstFlee = std::max(worstFlee, a.fleeT);
      if (a.role == Role::Guard && a.target >= 0 && a.st != AState::Dead) {
        float d = 1e9f;
        for (const Actor& t : g.actors) if (t.id == a.target && t.st != AState::Dead) d = len(t.p - a.p);
        if (d > 1e8f) continue;
        auto it = guardBest.find(a.id);
        if (it == guardBest.end() || d < it->second - 4 || d < 30) { guardBest[a.id] = d; guardNoProgress[a.id] = 0; }
        else guardNoProgress[a.id] += SIM_DT;
      }
    }
    if (!alive) { clearedAt = f / 60.0f; break; }
  }
  float stuckGuard = 0;
  for (auto& kv : guardNoProgress) stuckGuard = std::max(stuckGuard, kv.second);
  out("defence (%s) %s: %d guards, %d folk (%d militia): bell %s, wolves dead at %.1f s, %zu townsfolk down, %d hid indoors, longest flight %.1f s\n",
      what, st.name.c_str(), guards, folk, militia, rang ? "rang" : "silent", clearedAt, downed.size(), maxHidden, worstFlee);
  if (!rang) { out("FAIL: defence (%s): 3 wolves inside %s did not ring the bell\n", what, st.name.c_str()); bad++; }
  if (guarded && clearedAt < 0) { out("FAIL: defence: the guards of %s did not kill 3 wolves within 30 s\n", st.name.c_str()); bad++; }
  if (guarded && downed.size() > 1) { out("FAIL: defence: %zu villagers down in %s (max 1)\n", downed.size(), st.name.c_str()); bad++; }
  if (maxHidden == 0) { out("FAIL: defence (%s): nobody in %s ran home to hide\n", what, st.name.c_str()); bad++; }
  if (worstFlee > 20) { out("FAIL: defence (%s): a townsperson ran for %.0f s without getting home (stuck or oscillating)\n", what, worstFlee); bad++; }
  if (stuckGuard > 8) { out("FAIL: defence (%s): a guard chased for %.0f s without closing in (stuck)\n", what, stuckGuard); bad++; }
  if (clearedAt < 0) {   // unguarded: end it by hand, then they must still come back out
    for (int id : wolves)
      for (Actor& a : g.actors) if (a.id == id && a.st != AState::Dead) { a.hp = 0.3f; a.burnT = 2; }
    tick(g, 120);
  }
  // they come back out about 30 s after it is over
  int back = -1;
  for (int f = 0; f < 50 * 60; f++) {
    g.update(SIM_DT, Input());
    g.events.clear();
    if (g.mode != Mode::Play) g.mode = Mode::Play;
    if (g.shelteredCount(si) == 0) { back = f; break; }
  }
  if (back < 0) { out("FAIL: defence (%s): %d townsfolk still hiding 50 s after the threat ended\n", what, g.shelteredCount(si)); bad++; }
  else if (maxHidden > 0 && back < 20 * 60) { out("FAIL: defence (%s): townsfolk came out after only %.0f s (want ~30)\n", what, back / 60.0); bad++; }
  if (g.alarmSite == si) { out("FAIL: defence (%s): the bell still rings after the threat\n", what); bad++; }
  return bad;
}

// ---------------------------------------------------------------- bounty clarity
int bountyChecks(const Game& base) {
  int bad = 0;
  Game g = base;
  g.godMode = true; g.noWildSpawns = true; g.hour = 12;
  const Site& home = g.world.sites[g.world.startSite];
  int inn = -1;
  for (int b = home.bldgFirst; b < home.bldgFirst + home.bldgCount; b++) if (g.world.over.bldgs[b].type == art::Building::Inn) inn = b;
  if (inn < 0 || !enterBuilding(g, inn)) { out("FAIL: bounty: cannot enter the start inn\n"); return 1; }
  int k = findRole(g, Role::Innkeeper);
  if (k < 0) { out("FAIL: bounty: no innkeeper\n"); return 1; }
  // the opening: the innkeeper has the "!" and hands over the old blade on the first talk
  if (!g.rewardWaiting(g.actors[k])) { out("FAIL: opening: no '!' over the start innkeeper\n"); bad++; }
  if (!talk(g, k)) { out("FAIL: opening: could not talk to the innkeeper\n"); return bad + 1; }
  if (g.eqWeapon < 0 || !g.hasFlag(SF_FIRST_WEAPON) || g.openingQuest() >= 0) { out("FAIL: opening: the innkeeper did not hand over a weapon\n"); bad++; }
  const Quest* mq = g.questById(g.trackedQuest);
  if (!mq || mq->type != QType::Main) { out("FAIL: opening: the main quest is not tracked after the old blade\n"); bad++; }
  g.mode = Mode::Play;
  k = findRole(g, Role::Innkeeper);
  if (g.rewardWaiting(g.actors[k])) { out("FAIL: opening: '!' still over the innkeeper after the blade\n"); bad++; }
  const Actor giver = g.actors[k];
  // a bounty on the nearest camp, from this innkeeper
  int camp = nearestOf(g, SiteType::BanditCamp), other = nearestOf(g, SiteType::BanditCamp, camp);
  if (camp < 0 || other < 0) { out("WARN: bounty: fewer than two bandit camps\n"); return bad; }
  Quest q;
  q.id = g.nextQuestId++;
  q.type = QType::Bounty; q.state = QState::Active;
  q.title = "BOUNTY: " + g.world.sites[camp].name;
  q.giverName = giver.name; q.giverSite = giver.site; q.giverBldg = giver.bldg; q.giverSlot = giver.slot;
  q.target = camp; q.gold = 150; q.xp = 60;
  g.quests.push_back(q);
  g.trackedQuest = q.id;
  std::string st0 = g.questStatus(g.quests.back());
  if (st0.rfind("CLEAR ", 0) != 0 || st0.find('(') == std::string::npos || st0.size() > 36) { out("FAIL: bounty: journal line '%s'\n", st0.c_str()); bad++; }
  leaveInside(g);
  if (g.inside) { out("FAIL: bounty: could not leave the inn\n"); return bad + 1; }
  // the wrong camp first: never silent
  g.events.clear();
  if (!killChief(g, other)) { out("WARN: bounty: could not clear the other camp %s\n", g.world.sites[other].name.c_str()); }
  else {
    bool wrong = eventHas(g, "NOT YOUR TARGET") && g.notice.find("NOT YOUR TARGET") != std::string::npos && g.notice.find(g.world.sites[camp].name) != std::string::npos;
    out("bounty: wrong camp %s -> \"%s\"\n", g.world.sites[other].name.c_str(), g.notice.c_str());
    if (!wrong) { out("FAIL: bounty: clearing the wrong camp gave no 'NOT YOUR TARGET' notice\n"); bad++; }
    if (g.questById(q.id)->state != QState::Active) { out("FAIL: bounty: the wrong camp completed the bounty\n"); bad++; }
  }
  // the right camp
  g.events.clear();
  if (!killChief(g, camp)) { out("FAIL: bounty: could not clear the target camp\n"); return bad + 1; }
  const Quest* qq = g.questById(q.id);
  std::string want = "BOUNTY READY: RETURN TO " + giver.name + " IN " + home.name;
  if (!qq || qq->state != QState::Complete) { out("FAIL: bounty: clearing the target did not complete it\n"); return bad + 1; }
  if (!eventHas(g, want.c_str()) || g.notice != want) { out("FAIL: bounty: no '%s' notice (got '%s')\n", want.c_str(), g.notice.c_str()); bad++; }
  std::string st1 = g.questStatus(*qq);
  if (st1.rfind("RETURN TO", 0) != 0) { out("FAIL: bounty: journal line after completion '%s'\n", st1.c_str()); bad++; }
  int tx = 0, ty = 0;
  if (!g.questTarget(q.id, tx, ty) || tx != g.world.over.bldgs[inn].doorX() || ty != g.world.over.bldgs[inn].doorY()) { out("FAIL: bounty: the marker does not point at the giver's door\n"); bad++; }
  out("bounty: done -> \"%s\", journal \"%s\" / \"%s\"\n", g.notice.c_str(), st0.c_str(), st1.c_str());
  // back to the giver: "!", the first option collects
  if (!enterBuilding(g, inn)) { out("FAIL: bounty: cannot re-enter the inn\n"); return bad + 1; }
  k = findRole(g, Role::Innkeeper);
  if (k < 0 || !g.rewardWaiting(g.actors[k])) { out("FAIL: bounty: no '!' over the giver with the reward waiting\n"); bad++; }
  if (k >= 0 && talk(g, k)) {
    std::string lab = g.dlg.opts.empty() ? "" : g.dlg.opts[0].label;
    if (lab != "COLLECT BOUNTY (+150 GOLD)") { out("FAIL: bounty: first option is '%s'\n", lab.c_str()); bad++; }
    int gold0 = g.gold;
    g.dialogueChoose(0);
    if (g.gold < gold0 + 150 || g.questById(q.id)->state != QState::Done) { out("FAIL: bounty: collecting paid %d\n", g.gold - gold0); bad++; }
    g.mode = Mode::Play;
    k = findRole(g, Role::Innkeeper);
    if (k >= 0 && g.rewardWaiting(g.actors[k])) { out("FAIL: bounty: '!' still shown after collecting\n"); bad++; }
  } else { out("FAIL: bounty: could not talk to the giver\n"); bad++; }
  // journal lines for the other quest types
  Quest h; h.type = QType::Hunt; h.mon = art::Monster::Wolf; h.need = 5; h.have = 3; h.giverName = giver.name; h.giverSite = giver.site;
  std::string sh = g.questStatus(h);
  if (sh.rfind("3/5 WOLVES", 0) != 0) { out("FAIL: hunt journal line '%s'\n", sh.c_str()); bad++; }
  Quest c; c.type = QType::Clear; c.target = nearestOf(g, SiteType::Cave); c.giverName = giver.name; c.giverSite = giver.site;
  std::string sc = g.questStatus(c);
  if (c.target >= 0 && sc.rfind("CLEAR ", 0) != 0) { out("FAIL: clear journal line '%s'\n", sc.c_str()); bad++; }
  for (const Quest& m : g.quests)
    if (m.type == QType::Main && g.questStatus(m).empty()) { out("FAIL: main quest has no journal line\n"); bad++; }
  return bad;
}

// ---------------------------------------------------------------- background traits
Game withBackground(const Game& base, Background b) {
  Game g = base;
  g.background = b;
  g.finishCreator();
  g.godMode = true; g.noWildSpawns = true; g.hour = 12;
  return g;
}
// the shelf of the nearest seller with this role (building type), total list value without arrows; -1 none
int shelfValue(Game& g, art::Building type, Role role) {
  int best = -1;
  float bd = 1e30f;
  const Site& h = g.world.sites[g.world.startSite];
  for (size_t b = 0; b < g.world.over.bldgs.size(); b++) {
    const Bldg& B = g.world.over.bldgs[b];
    if (B.type != type) continue;
    float d = std::hypot((float)(B.doorX() - h.ex), (float)(B.doorY() - h.ey));
    if (d < bd) { bd = d; best = (int)b; }
  }
  if (best < 0 || !enterBuilding(g, best)) return -1;
  int k = findRole(g, role);
  if (k < 0 || !talk(g, k)) return -1;
  int o = optIndex(g, "WARES");
  if (o < 0) return -1;
  g.dialogueChoose(o);
  if (g.mode != Mode::Shop) return -1;
  int v = 0;
  for (const Item& it : g.shop.stock) if (it.kind != ItemKind::Arrows) v += it.value;
  g.mode = Mode::Play;
  leaveInside(g);
  return v;
}

int backgroundChecks(const Game& base) {
  int bad = 0;
  Game none = withBackground(base, Background::None);
  // blacksmith's child: smiths charge a fifth less; urchin: merchants a little less
  {
    Game a = none, b = withBackground(base, Background::Blacksmith), c = withBackground(base, Background::Urchin);
    int va = shelfValue(a, art::Building::Smithy, Role::Smith), vb = shelfValue(b, art::Building::Smithy, Role::Smith);
    if (va > 0 && vb > 0) { if (vb > va * 0.85f) { out("FAIL: blacksmith's child: smith shelf %d vs %d\n", vb, va); bad++; } }
    else out("WARN: background: no smith reachable to price\n");
    Game a2 = none;
    int ma = shelfValue(a2, art::Building::Shop, Role::Merchant), mc = shelfValue(c, art::Building::Shop, Role::Merchant);
    if (ma > 0 && mc > 0) { if (mc > ma * 0.9f) { out("FAIL: urchin: merchant shelf %d vs %d\n", mc, ma); bad++; } }
    else out("WARN: background: no merchant reachable to price\n");
    out("backgrounds: smith shelf %d -> %d (blacksmith's child), merchant shelf %d -> %d (urchin)\n", va, vb, ma, mc);
  }
  // hunter: a wolf 95 px away at noon notices an ordinary traveller (120 px) but not a hunter (84 px)
  {
    const Site& h = base.world.sites[base.world.startSite];
    int sx = -1, sy = -1;
    for (int r = 16; r < 60 && sx < 0; r += 2)
      for (int k = 0; k < 24 && sx < 0; k++) {
        int tx = h.r.cx() + (int)std::lround(std::cos(k * 0.2618f) * r), ty = h.r.cy() + (int)std::lround(std::sin(k * 0.2618f) * r);
        int x, y;
        if (!base.world.over.in(tx, ty) || base.world.siteAt(tx, ty, 12) >= 0 || !freeTile(base, tx, ty, x, y, 1)) continue;
        sx = x; sy = y;
      }
    if (sx < 0) out("WARN: hunter: no open ground for the aggro test\n");
    else {
      bool noticed[2] = {false, false};
      for (int i = 0; i < 2; i++) {
        Game g = i == 0 ? none : withBackground(base, Background::Hunter);
        g.pl().p = tileCentre(sx, sy);
        tick(g, 2);
        int id = g.debugSpawnAt(art::Monster::Wolf, g.pl().p + Vec2(95, 0), 1);
        for (Actor& a : g.actors) if (a.id == id) { a.aggro = false; a.home = a.p; }
        for (int f = 0; f < 40; f++) {
          g.update(SIM_DT, Input());
          for (const Actor& a : g.actors) if (a.id == id && a.aggro) noticed[i] = true;
          for (Actor& a : g.actors) if (a.id == id) a.p = g.pl().p + Vec2(95, 0);   // hold it at range
        }
      }
      if (!noticed[0] || noticed[1]) { out("FAIL: hunter: wolf at 95 px noticed traveller %d, hunter %d\n", noticed[0], noticed[1]); bad++; }
    }
  }
  // farmhand: stamina comes back faster, food heals more
  {
    Game a = none, b = withBackground(base, Background::Farmhand);
    float got[2], healed[2];
    Game* gs[2] = {&a, &b};
    for (int i = 0; i < 2; i++) {
      Game& g = *gs[i];
      g.stamina = 0;
      tick(g, 60);
      got[i] = g.stamina;
      g.pl().hp = 10;
      int bread = -1;
      for (int j = 0; j < (int)g.inv.size(); j++) if (g.inv[j].kind == ItemKind::Food) bread = j;
      if (bread >= 0) g.useItem(bread);
      healed[i] = g.pl().hp - 10;
    }
    if (got[1] < got[0] * 1.2f || healed[1] < healed[0] * 1.4f) { out("FAIL: farmhand: stamina %.1f vs %.1f, food %.1f vs %.1f\n", got[1], got[0], healed[1], healed[0]); bad++; }
  }
  // temple novice: knows mend, blessings last longer
  {
    Game g = withBackground(base, Background::Novice);
    if (!(g.spellsKnown & (1 << (int)Spell::Heal)) || g.blessingSecs() <= none.blessingSecs()) { out("FAIL: temple novice: mend %d, blessing %.0f s\n", (g.spellsKnown >> (int)Spell::Heal) & 1, g.blessingSecs()); bad++; }
    if (none.spellsKnown & (1 << (int)Spell::Heal)) { out("FAIL: a wanderer knows mend from the start\n"); bad++; }
  }
  // exiled noble: town guards offer work
  {
    int town = nearestOf(base, SiteType::Town);
    bool offers[2] = {false, false}, talked[2] = {false, false};
    for (int i = 0; i < 2 && town >= 0; i++) {
      Game g = i == 0 ? none : withBackground(base, Background::Noble);
      if (!standNear(g, town, 26)) break;   // the town's people come out; talk() walks up to the guard
      for (size_t k = 1; k < g.actors.size() && !talked[i]; k++) {
        if (g.actors[k].role != Role::Guard || g.actors[k].site != town) continue;
        if (talk(g, (int)k)) { talked[i] = true; offers[i] = optIndex(g, "HELP") >= 0; }
      }
    }
    if (talked[0] && talked[1]) { if (offers[0] || !offers[1]) { out("FAIL: noble: guard offers work to traveller %d, noble %d\n", offers[0], offers[1]); bad++; } }
    else out("WARN: noble: could not talk to a guard\n");
  }
  // sailor and marked one: small hooks, visible in the stats
  {
    Game s = withBackground(base, Background::Sailor), m = withBackground(base, Background::Marked);
    if (s.maxSt <= none.maxSt || m.maxMp <= none.maxMp) { out("FAIL: sailor stamina %.0f / marked magicka %.0f\n", s.maxSt, m.maxMp); bad++; }
  }
  return bad;
}
}  // namespace

// EMB_SCRIPT_INFO=1 rpg_test <seed>: what tools/scripts/bounty.txt needs to know about a seed (camps, chiefs, which
// villager near the start has work and what it is)
static void scriptInfo(const Game& base) {
  Game g = base;
  g.godMode = true; g.noWildSpawns = true;
  tick(g, 3);
  int camp = nearestOf(g, SiteType::BanditCamp), other = nearestOf(g, SiteType::BanditCamp, camp);
  for (int c : {camp, other}) {
    if (c < 0) continue;
    const Site& s = g.world.sites[c];
    for (const Spawn& sp : g.world.over.spawns)
      if (sp.site == c && sp.boss) printf("info: camp %s (site %d) entrance %d,%d chief at %d,%d\n", s.name.c_str(), c, s.ex, s.ey, sp.x, sp.y);
  }
  const Site& h = g.world.sites[g.world.startSite];
  printf("info: start %s centre %d,%d player %.0f,%.0f\n", h.name.c_str(), h.r.cx(), h.r.cy(), g.pl().p.x / TILE, g.pl().p.y / TILE);
  std::vector<std::pair<float, int>> folk;
  for (size_t k = 1; k < g.actors.size(); k++)
    if (g.actors[k].npc && g.actors[k].role == Role::Villager) folk.push_back({len(g.actors[k].p - g.pl().p), g.actors[k].id});
  std::sort(folk.begin(), folk.end());
  for (auto& f : folk) {
    Game t = g;
    int k = -1;
    for (size_t j = 1; j < t.actors.size(); j++) if (t.actors[j].id == f.second) k = (int)j;
    if (k < 0) continue;
    std::string nm = t.actors[k].name;
    Vec2 at = t.actors[k].p;
    if (!talk(t, k)) continue;
    int o = optIndex(t, "HELP");
    std::string offer = "-";
    if (o >= 0) { t.dialogueChoose(o); offer = t.dlg.text.substr(0, 90); }
    printf("info: villager %s %.0f px from the player at %.0f,%.0f: %s\n", nm.c_str(), f.first, at.x / TILE, at.y / TILE, offer.c_str());
  }
}

int defenceChecks(uint64_t seed) {
  int bad = 0;
  Game base(seed);
  base.newGame(seed);
  base.mode = Mode::Play;
  if (getenv("EMB_SCRIPT_INFO")) scriptInfo(base);
  // factions: the old `hostile` flag still means "hostile to the player"
  {
    Game g = base;
    g.noWildSpawns = true;
    for (int m = 0; m < (int)art::Monster::COUNT; m++) g.debugSpawnAt((art::Monster)m, g.pl().p + Vec2(200, 0), 1);
    for (const Actor& a : g.actors)
      if (a.hostile != factionsHostile(a.faction, Faction::Player)) { out("FAIL: %s: hostile %d but faction %s\n", a.name.c_str(), a.hostile, factionName(a.faction)); bad++; }
  }
  for (const Actor& a : base.actors)
    if (a.hostile != factionsHostile(a.faction, Faction::Player)) { out("FAIL: actor %s: hostile %d but faction %s\n", a.name.c_str(), a.hostile, factionName(a.faction)); bad++; }
  // town defence: the nearest guarded town (villages have no guards), then the start village (militia only)
  int town = nearestOf(base, SiteType::Town);
  if (town < 0) town = nearestOf(base, SiteType::City);
  if (town >= 0) bad += townDefence(base, town, true, "town");
  bad += townDefence(base, base.world.startSite, base.world.sites[base.world.startSite].type != SiteType::Village, "start");
  bad += bountyChecks(base);
  bad += backgroundChecks(base);
  return bad;
}
