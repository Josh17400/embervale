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
#include "rpg/sim/game_internal.h"
#include "tools/tests/tests.h"

namespace {
// the nearest site of a type to the start village (exclude: skip this one)
int nearestOf(const Game& g, SiteType t, int exclude = -1) {
  const Site& h = g.world.sites[g.world.startSite];
  int best = -1;   // (among the records the session holds: the window and the regions around it)
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
  if (!g.world.over.in(g.world.over.bldgs[(size_t)b].doorX(), g.world.over.bldgs[(size_t)b].doorY() + 1)) {   // (M2) far away: go there first
    const Bldg& F = g.world.over.bldgs[(size_t)b];
    g.teleportGlobal(g.world.ox + F.doorX(), g.world.oy + F.doorY() + 1);
  }
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
// a bandit camp's chief burns down (kill() runs the clear logic exactly as a player kill would). The bounty asks for
// the chief: the camp must count as cleared the moment the chief falls (CLEARED and BOUNTY READY / NOT YOUR TARGET
// at once, plus a "CHIEF SLAIN - N BANDITS FIGHT ON" line when stragglers remain), not only after the last bandit.
bool killChief(Game& g, int camp) {
  if (!g.world.over.in(g.world.sites[(size_t)camp].ex, g.world.sites[(size_t)camp].ey)) {   // (M2) bring a far camp into the window
    const Site& f = g.world.sites[(size_t)camp];
    g.teleportGlobal(g.world.ox + f.ex, g.world.oy + f.r.y + f.r.h + 3);
    tick(g, 3);
  }
  const Site s = g.world.sites[camp];   // (a copy: streaming appends sites)
  int x, y;
  if (!freeTile(g, s.r.cx(), s.r.y + s.r.h + 2, x, y, 8)) return false;
  g.pl().p = tileCentre(x, y);
  tick(g, 3);
  for (int tries = 0; tries < 800; tries++) {
    int chief = -1, others = 0;
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& a = g.actors[k];
      if (a.site != camp || !a.hostile || a.st == AState::Dead) continue;
      if (a.boss) chief = (int)k; else others++;
    }
    if (chief < 0) {
      if (!g.world.sites[camp].cleared) { out("FAIL: bounty: the chief of %s is dead but the camp is not cleared\n", s.name.c_str()); return false; }
      if (others > 0 && !eventHas(g, "CHIEF SLAIN")) { out("FAIL: bounty: chief of %s slain with %d bandits left, no 'CHIEF SLAIN' line\n", s.name.c_str(), others); return false; }
      return true;
    }
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
  {   // (M2: the endless world) bring the place into the window first: settlements lie far apart
    const Site& s0 = g.world.sites[(size_t)si];
    if (!g.world.over.in(s0.r.x, s0.r.y) || !g.world.over.in(s0.r.x + s0.r.w - 1, s0.r.y + s0.r.h - 1)) {
      g.teleportGlobal(g.world.ox + s0.ex, g.world.oy + s0.ey + 2);
      tick(g, 3);
    }
  }
  if (!standNear(g, si, std::max(26.0f, std::max(g.world.sites[(size_t)si].r.w, g.world.sites[(size_t)si].r.h) * 0.5f + 6.0f))) { out("WARN: defence (%s): no spot near %s\n", what, g.world.sites[si].name.c_str()); return 0; }
  const Site st = g.world.sites[si];   // (a copy: streaming appends sites)
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
  if (getenv("EMB_DEF_TRACE")) {   // where the fight starts and where the watch stands
    printf("  trace %s: wolves (level %d) at %.0f,%.0f, heart %d,%d, middle %.0f,%.0f\n", st.name.c_str(), lvl, at.x / TILE, at.y / TILE, st.ex, st.ey, mid.x / TILE, mid.y / TILE);
    for (const Actor& a : g.actors)
      if (a.npc && a.site == si && a.role == Role::Guard) printf("  trace   guard at %.0f,%.0f (home %.0f,%.0f)\n", a.p.x / TILE, a.p.y / TILE, a.home.x / TILE, a.home.y / TILE);
  }
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
      if (a.st == AState::Dead && a.role != Role::Guard && !downed.count(a.id) && getenv("EMB_DEF_TRACE"))
        printf("  trace %.1f s: %s down (militia %d, flee %.1f s) at %.0f,%.0f\n", f / 60.0f, a.name.c_str(), a.militia, a.fleeT, a.p.x / TILE, a.p.y / TILE);
      if (a.st == AState::Dead && a.role != Role::Guard) downed.insert(a.id);
      worstFlee = std::max(worstFlee, a.fleeT);
      if (a.role == Role::Guard && a.target >= 0 && a.st != AState::Dead) {
        float d = 1e9f;
        for (const Actor& t : g.actors) if (t.id == a.target && t.st != AState::Dead) d = len(t.p - a.p);
        if (d > 1e8f) continue;
        auto it = guardBest.find(a.id);
        if (it == guardBest.end() || d < it->second - 4 || d < 30) { guardBest[a.id] = d; guardNoProgress[a.id] = 0; }
        else guardNoProgress[a.id] += SIM_DT;
        if (getenv("EMB_DEF_TRACE") && guardNoProgress[a.id] > 3.0f && guardNoProgress[a.id] < 3.0f + SIM_DT * 1.5f) {
          const int gx0 = (int)(a.p.x / TILE), gy0 = (int)((a.p.y - 2) / TILE);
          for (int yy = gy0 - 3; yy <= gy0 + 3; yy++) {
            std::string row;
            for (int xx = gx0 - 4; xx <= gx0 + 4; xx++) {
              const Map& mm = g.world.over;
              char ch = mm.blocked(xx, yy) ? '#' : (mm.in(xx, yy) && mm.bldgAt[(size_t)yy * mm.w + xx] >= 0 ? 'b' : '.');
              if (xx == gx0 && yy == gy0) { ch = ch == '#' ? 'X' : 'G'; printf("  trace tile prop %d ground %d solid %d\n", mm.propAt(xx, yy) - 1, (int)mm.at(xx, yy), (int)mm.solid[(size_t)yy * mm.w + xx]); }
              row += ch;
            }
            printf("  trace map %s\n", row.c_str());
          }
          for (const Actor& o : g.actors)
            if (o.id != a.id && len(o.p - a.p) < 24.0f)
              printf("  trace near: %s id %d at %.1f,%.1f critter %d resident %d posture %d st %d npc %d hostile %d\n", o.name.c_str(), o.id, o.p.x / TILE, o.p.y / TILE,
                     (int)o.critter, o.resident, (int)o.posture, (int)o.st, (int)o.npc, (int)o.hostile);
        }
        if (getenv("EMB_DEF_TRACE") && f % 30 == 0)
          printf("  trace %.1f s: guard %d at %.1f,%.1f st %d target %d d %.0f noprog %.1f nav %d/%d asleep %d knock %.1f\n", f / 60.0f, a.id, a.p.x / TILE, a.p.y / TILE, (int)a.st, a.target, d,
                 guardNoProgress[a.id], a.navNext, a.navGoal, (int)a.asleep, len(a.knock));
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
  // (M2: on the endless towns of 40-60 homes the watch keeps posts on the square and runs when the bell rings)
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
  const Site home = g.world.sites[g.world.startSite];   // a copy: the endless world appends sites as it streams
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
  if ((st0.rfind("KILL THE CHIEF", 0) != 0 && st0.rfind("CHIEF AT ", 0) != 0) || st0.find('(') == std::string::npos || st0.size() > 36) { out("FAIL: bounty: journal line '%s'\n", st0.c_str()); bad++; }
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


// (M3c LIFE) the Wildlands wildlife in the open (M6: and the harpy and the golem): each of the seven, set on a sturdy idle player at level 1, must close
// in and land its blows and its own move within 20 s (the scorpion's venom sting, the yeti's frost slam, the hound's
// fire bite, the wisp's bolts, the blightspawn's spores; hyenas and the lurker their bites); nothing goes NaN
int wildlifeChecks(const Game& base) {
  int bad = 0;
  const Site& h = base.world.sites[base.world.startSite];
  int sx = -1, sy = -1;
  for (int r = 18; r < 60 && sx < 0; r += 2)
    for (int k = 0; k < 24 && sx < 0; k++) {
      const int tx = h.r.cx() + (int)std::lround(std::cos(k * 0.2618f) * r), ty = h.r.cy() + (int)std::lround(std::sin(k * 0.2618f) * r);
      if (!base.world.over.in(tx, ty) || base.world.siteAt(tx, ty, 12) >= 0) continue;
      // a clear strip: the player at its west end, the creature at its east end, nothing solid in between (wild
      // creatures close in straight; a tree between them is a fight of its own)
      bool clear = true;
      for (int y = ty - 2; y <= ty + 2 && clear; y++)
        for (int x = tx - 2; x <= tx + 6 && clear; x++)
          if (base.world.over.blocked(x, y) || base.world.over.propAt(x, y) != 0) clear = false;
      if (clear) { sx = tx; sy = ty; }
    }
  if (sx < 0) { out("WARN: wildlife: no open ground for the creature checks\n"); return 0; }
  using art::Monster;
  // (M6 FOES) the harpy (its shriek slows) and the golem (its wide ground slam shakes the earth) too
  const Monster ms[] = {Monster::Scorpion, Monster::Hyena, Monster::Lurker, Monster::Yeti, Monster::Wisp, Monster::EmberHound, Monster::Blightspawn,
                        Monster::Harpy, Monster::Golem};
  std::string line = "wildlife:";
  for (Monster m : ms) {
    Game g = base;
    g.noWildSpawns = true;
    g.pl().p = tileCentre(sx, sy);
    g.pl().maxHp = g.pl().hp = 5000;
    tick(g, 2);
    const int id = g.debugSpawnAt(m, g.pl().p + Vec2(64, 0), 1);   // (after the ticks: the window may have shifted)
    bool special = false, nan = false;
    float firstHit = -1, lost = 0;
    for (int f = 0; f < (int)(20.0f / SIM_DT); f++) {
      const float before = g.pl().hp;
      g.update(SIM_DT, Input());
      if (g.mode != Mode::Play) g.mode = Mode::Play;
      if (g.pl().hp < before) { lost += before - g.pl().hp; if (firstHit < 0) firstHit = f * SIM_DT; }
      switch (m) {
        case Monster::Scorpion: if (eventHas(g, "VENOM")) special = true; break;
        case Monster::Yeti: case Monster::Blightspawn: if (g.pl().slowT > 0) special = true; break;
        case Monster::EmberHound: if (g.pl().burnT > 0) special = true; break;
        case Monster::Harpy: if (eventHas(g, "SHRIEK") && g.pl().slowT > 0) special = true; break;
        case Monster::Golem: for (const Event& ev : g.events) if (ev.type == Ev::Shake && ev.f >= 6.0f) special = true; break;
        case Monster::Wisp: for (const Projectile& pr : g.projs) if (pr.owner == id && pr.kind == ProjKind::Magic) special = true; break;
        default: special = firstHit >= 0; break;
      }
      if (m == Monster::Blightspawn) for (const Projectile& pr : g.projs) if (pr.owner == id && pr.kind == ProjKind::Spit) special = true;
      for (const Actor& a : g.actors) if (a.id == id && (!std::isfinite(a.p.x) || !std::isfinite(a.p.y))) nan = true;
      g.events.clear();
      if (special && firstHit >= 0 && f * SIM_DT > 8.0f) break;
    }
    char b[96];
    std::snprintf(b, sizeof b, " %s first hit %.1fs lost %.0f%s;", monsterName(m), firstHit, lost, special ? "" : " NO MOVE");
    line += b;
    if (firstHit < 0 || !special || nan) {
      float d = -1;
      int st = -1, ag = -1;
      for (const Actor& a : g.actors) if (a.id == id) { d = len(a.p - g.pl().p); st = (int)a.st; ag = a.aggro ? 1 : 0; }
      out("FAIL: wildlife: %s: first hit %.1f s, its move %s, nan %d (at the end %.0f px off, state %d, aggro %d)\n", monsterName(m), firstHit,
          special ? "seen" : "never seen", nan, d, st, ag);
      bad++;
    }
  }
  out("%s\n", line.c_str());
  return bad;
}

int defenceChecks(uint64_t seed) {
  int bad = 0;
  Game base(seed);
  base.newEndlessGame(seed);
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
  // (M2: the endless world) the nearest town in the region plans around the start (loading it into the records)
  // (M4) a kingdom's town keeps a watch; an independent town (the wildlands) has militia only, so the guarded check takes
  // the nearest town a kingdom holds, and an independent one met first is checked unguarded (the bell, the hiding)
  const Site home = base.world.sites[(size_t)base.world.startSite];
  int town = base.world.findSiteNear(base.world.ox + home.ex, base.world.oy + home.ey, SiteType::Town, 8);
  if (town < 0) town = base.world.findSiteNear(base.world.ox + home.ex, base.world.oy + home.ey, SiteType::City, 8);
  int member = -1;
  {
    float bd = 1e30f;
    for (int i = 0; i < (int)base.world.sites.size(); i++) {
      const Site& s = base.world.sites[(size_t)i];
      if ((s.type != SiteType::Town && s.type != SiteType::City) || s.kingdom < 0) continue;
      const float d = std::hypot((float)(s.ex - home.ex), (float)(s.ey - home.ey));
      if (d < bd) { bd = d; member = i; }
    }
  }
  if (member >= 0) bad += townDefence(base, member, true, "town");
  if (town >= 0 && town != member && base.world.sites[(size_t)town].kingdom < 0) bad += townDefence(base, town, false, "free town");
  // (M4) the start village: a kingdom's village keeps a small watch (2-3), so it is held to the guarded standard too
  bad += townDefence(base, base.world.startSite, base.world.sites[base.world.startSite].type != SiteType::Village || base.world.sites[base.world.startSite].kingdom >= 0, "start");
  // (M4) the kingdom's watch wears its owner's colours and serves it; an independent place keeps none
  {
    Game g = base;
    g.noWildSpawns = true;
    tick(g, 30);
    const Site& s = g.world.sites[(size_t)g.world.startSite];
    int guards = 0, wrong = 0;
    for (const Actor& a : g.actors) {
      if (!a.npc || a.site != g.world.startSite || a.role != Role::Guard) continue;
      guards++;
      if (s.kingdom < 0 || a.realm != g.world.kingdoms[(size_t)s.kingdom].id || (a.look.tabardColor | 0xFF000000u) != (g.world.kingdoms[(size_t)s.kingdom].tabard() | 0xFF000000u)) wrong++;
    }
    if (s.kingdom >= 0 && guards == 0) { out("FAIL: defence: the member village %s keeps no watch\n", s.name.c_str()); bad++; }
    if (s.kingdom < 0 && guards > 0) { out("FAIL: defence: the independent village %s has %d guards\n", s.name.c_str(), guards); bad++; }
    if (wrong) { out("FAIL: defence: %d guards of %s do not wear or serve its owner\n", wrong, s.name.c_str()); bad++; }
  }
  bad += bountyChecks(base);
  bad += backgroundChecks(base);
  bad += wildlifeChecks(base);
  // (M1) a townsperson felled by a monster stays down while their town is active: the site streamer must not bring the
  // same person back at their spawn tile once the body is cleared (they return when the town next loads)
  {
    Game g = base;
    g.noWildSpawns = true;
    tick(g, 30);
    int victim = -1;
    float bd = 1e30f;
    for (const Actor& a : g.actors)
      if (a.npc && a.fromMap && a.site >= 0 && a.slot >= 0 && a.st != AState::Dead && a.role != Role::Guard) {
        const float d = len2(a.p - g.pl().p);
        if (d < bd) { bd = d; victim = a.id; }
      }
    if (victim >= 0) {
      int site = -1, slot = -1;
      for (const Actor& a : g.actors) if (a.id == victim) { site = a.site; slot = a.slot; }
      g.debugFell(victim);
      int back = 0;
      for (int f = 0; f < (int)(12.0f / SIM_DT); f++) {
        tick(g, 1);
        for (const Actor& a : g.actors)
          if (a.site == site && a.slot == slot && a.fromMap && a.st != AState::Dead) { back++; break; }
        if (back) break;
      }
      if (back) { out("FAIL: a felled townsperson (site %d slot %d) was back on their feet within 12 s\n", site, slot); bad++; }
      // ...nor after a quick visit to the nearest building (entering and leaving clears the actors and re-streams the
      // town: the felled stay down until the town is put away)
      int bi = -1;
      float bb = 1e30f;
      for (size_t i = 0; i < g.world.over.bldgs.size(); i++) {
        const Bldg& B = g.world.over.bldgs[i];
        const float d = len2(Vec2(B.doorX() * TILE + 8.0f, B.doorY() * TILE + 8.0f) - g.pl().p);
        if (d < bb) { bb = d; bi = (int)i; }
      }
      if (!back && bi >= 0 && g.debugEnterBuilding(bi, 0)) {
        tick(g, 20);
        g.debugLeave();
        for (int f = 0; f < (int)(6.0f / SIM_DT) && !back; f++) {
          tick(g, 1);
          for (const Actor& a : g.actors)
            if (a.site == site && a.slot == slot && a.fromMap && a.st != AState::Dead) { back++; break; }
        }
        if (back) { out("FAIL: a felled townsperson (site %d slot %d) was back after a visit to a building\n", site, slot); bad++; }
      }
    }
  }
  return bad;
}
