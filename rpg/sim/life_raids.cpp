// M5 "Hearth and Hall": night raids by monster pressure (VISION_PLAN 10.4, M5 step). CITIZENS lane.
// Each loaded settlement's monster pressure = the uncleared dens and lairs within 60 tiles x (1 - its kingdom's
// military strength for its size) x the season: 2-8 % a night (life::Life::raidChancePct; none when nothing prowls
// near). The roll is deterministic per settlement and night (Life::raidRoll), decided once a night at the raid's hour
// (22:00-02:59, by the place). With the player within 120 tiles and outdoors, a raid party (the nearest den's pack
// plus a pack leader) comes in along a road: the bell rings (ai.cpp updateTownDefence: 3+ hostiles inside), guards and
// militia answer. The defence fails when no defender stands while raiders are in the streets, or 3+ raiders hold the
// streets for 30 s: the realm marks prosperity -10 and damage +5 (Realm::lifeRaid), the town is MF_RAIDED for three
// days, and maybe a villager is carried off (RF_AWAY; a rescue quest: life_quests.cpp offers a QType::Missing).
// Far from the player the outcome is decided in the abstract (pressure against the garrison and militia).
#include <algorithm>
#include <cmath>
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"

namespace {

uint32_t rh(uint64_t a, uint64_t b) { return (uint32_t)(ew::mix64(a * 0x9E3779B97F4A7C15ull ^ ew::mix64(b + 0x4A1Dull)) >> 16); }

// the raid's consequences for the settlement (both the live and the abstract outcome); returns the carried-off name
std::string raidOutcome(Game& g, int si, life::Census& c, bool failed, int night) {
  const Site& s = g.world.sites[(size_t)si];
  g.realm.lifeRaid(s.id, failed, g.day);
  if (!failed) return "";
  c.raidDay = g.day;
  if (c.takenIdx >= 0) return "";
  if (rh(c.site ^ 0x7A4Eull, (uint64_t)(uint32_t)night) % 100u >= 55) return "";
  const int n = (int)c.res.size();
  if (!n) return "";
  const int start = (int)(rh(c.site, (uint64_t)(uint32_t)night * 13u) % (uint32_t)n);
  for (int k = 0; k < n; k++) {
    life::Resident& r = c.res[(size_t)((start + k) % n)];
    if (r.flags & (life::RF_DEAD | life::RF_AWAY | life::RF_KEY)) continue;
    if (r.job == life::Job::Child || r.job == life::Job::Guard || r.job == life::Job::Noble || r.actor >= 0) continue;
    if (g.lifeGiverOpen(si, (int)r.idx)) continue;   // (M5) the giver of an open quest stays (its job can be handed in)
    r.flags |= life::RF_AWAY;
    c.takenIdx = (int16_t)r.idx;
    c.takenQuest = 0;
    return r.name;
  }
  return "";
}

// how far a tile lies outside a settlement's footprint (0 inside it), squared
int edgeDist2(const Site& s, int x, int y) {
  const int dx = x < s.r.x ? s.r.x - x : x >= s.r.x + s.r.w ? x - (s.r.x + s.r.w - 1) : 0;
  const int dy = y < s.r.y ? s.r.y - y : y >= s.r.y + s.r.h ? y - (s.r.y + s.r.h - 1) : 0;
  return dx * dx + dy * dy;
}

// uncleared dens and lairs (caves, a dragon's lair, bandit camps) within 60 tiles of the settlement (its footprint's
// edge: the generator keeps dens a walk away from the houses); the nearest den's beast (or a cave's) leads the raid
int pressureAt(Game& g, const Site& s, art::Monster& mon, int& pack, int& denIx, int (Game::*cleared)(int) const) {
  int p = 0;
  float bd = 1e30f;
  mon = art::Monster::Wolf;
  pack = 3;
  denIx = -1;
  for (int di : g.world.nearDens) {
    if (di < 0 || di >= (int)g.world.dens.size()) continue;
    const Den& d = g.world.dens[(size_t)di];
    const float dd = (float)edgeDist2(s, d.x, d.y);
    if (dd > 60.0f * 60.0f || (g.*cleared)(di) >= 0) continue;
    p++;
    if (dd < bd) { bd = dd; mon = d.mon; pack = d.pack; denIx = di; }
  }
  for (int oi : g.world.nearSites) {
    if (oi < 0 || oi >= (int)g.world.sites.size()) continue;
    const Site& o = g.world.sites[(size_t)oi];
    if ((o.type != SiteType::Cave && o.type != SiteType::DragonLair && o.type != SiteType::BanditCamp) || o.cleared) continue;
    const float dd = (float)edgeDist2(s, o.ex, o.ey);
    if (dd > 60.0f * 60.0f) continue;
    p++;
    if (denIx < 0 && o.type == SiteType::Cave && dd < bd) { bd = dd; mon = o.theme; }
  }
  return p;
}

float militaryOf(const Game& g, const Site& s) {
  if (s.kingdom < 0 || s.kingdom >= (int)g.world.kingdoms.size()) return 0.0f;   // the wildlands: militia only
  const realm::KingdomState* K = g.realm.kingdom(g.world.kingdoms[(size_t)s.kingdom].id);
  if (!K) return 0.4f;
  return std::clamp(K->military / std::max(0.1f, K->pop * 1.2f), 0.0f, 1.0f);
}

}  // namespace

void Game::raidStep(float dt) {
  life::Life::RaidLive& R = life.raid;
  // ---- a raid going on in front of the player
  if (R.site) {
    R.t += dt;
    const int si = R.si;
    life::Census* c = si >= 0 && si < (int)world.sites.size() && world.sites[(size_t)si].id == R.site ? life.findMut(R.site) : nullptr;
    if (!c) { R = life::Life::RaidLive(); return; }
    const Site& s = world.sites[(size_t)si];
    int alive = 0, in = 0;
    for (int id : R.ids) {
      const int k = findActor(id);
      if (k < 0) continue;
      const Actor& a = actors[(size_t)k];
      if (a.st == AState::Dead || !a.hostile) continue;
      alive++;
      const int tx = (int)std::floor(a.p.x / TILE), ty = (int)std::floor(a.p.y / TILE);
      if (tx >= s.r.x - 1 && ty >= s.r.y - 1 && tx < s.r.x + s.r.w + 1 && ty < s.r.y + s.r.h + 1) in++;
    }
    if (in >= 3) R.insideT += dt;
    int defenders = 0;
    for (size_t k = 0; k < actors.size(); k++) {
      const Actor& a = actors[k];
      if (a.st == AState::Dead) continue;
      const bool guard = a.npc && (a.role == Role::Guard || a.role == Role::Soldier || a.role == Role::Captain || a.militia);
      if (!guard && !(a.player && a.hp > 0)) continue;
      const int tx = (int)std::floor(a.p.x / TILE), ty = (int)std::floor(a.p.y / TILE);
      const int m = a.player ? 12 : 8;
      if (tx >= s.r.x - m && ty >= s.r.y - m && tx < s.r.x + s.r.w + m && ty < s.r.y + s.r.h + m) defenders++;
    }
    const bool night = hour >= 21.0f || hour < 5.0f;
    int outcome = 0;
    if (inside || travelling()) outcome = R.insideT >= 10.0f ? 2 : 1;   // out of sight: decided as it stood
    else if (alive == 0 && R.t > 1.0f) outcome = 1;
    else if (R.insideT >= 30.0f || (defenders == 0 && in > 0 && R.t > 8.0f)) outcome = 2;
    else if ((!night && R.t > 60.0f) || R.t > 140.0f) outcome = 1;   // dawn, or they never got in: back to the woods
    if (!outcome) return;
    R.outcome = outcome;
    life.lastRaidOutcome = outcome;
    const std::string taken = raidOutcome(*this, si, *c, outcome == 2, R.night);
    if (outcome == 2) life.stats.raidsFailed++;
    // whoever is left of the party leaves
    for (int id : R.ids) {
      const int k = findActor(id);
      if (k < 0) continue;
      Actor& a = actors[(size_t)k];
      if (a.st == AState::Dead) continue;
      const Vec2 away = a.p + (a.p - Vec2(s.ex * TILE + 8.0f, s.ey * TILE + 8.0f)) * 3.0f;
      a.aggro = false; a.wild = true; a.home = away; a.goal = away; a.target = -1;
    }
    if (!inside) {
      if (outcome == 1) say("THE RAIDERS ARE DRIVEN OFF. " + s.name + " HOLDS.");
      else {
        std::string m = s.name + " WAS OVERRUN IN THE NIGHT.";
        if (!taken.empty()) m += " " + taken + " WAS CARRIED OFF!";
        say(m);
        emit(Ev::Notice, pl().p, (int)rgba(255, 110, 80), 0, m);
      }
    }
    R = life::Life::RaidLive();
    return;
  }
  if (!world.endless || travelling()) return;
  const bool night = hour >= 22.0f || hour < 3.0f;
  if (!night && !life.forceRaidSite) return;
  const int nightIx = hour >= 12.0f ? day : day - 1;
  const float cur = hour >= 12.0f ? hour : hour + 24.0f;
  for (int si : world.nearSites) {
    if (si < 0 || si >= (int)world.sites.size()) continue;
    const Site& s = world.sites[(size_t)si];
    if (!s.settlement()) continue;
    life::Census* c = life.findMut(s.id);
    if (!c) continue;
    const bool forced = life.forceRaidSite == s.id;
    if (!forced) {
      if (c->raidNight == nightIx) continue;
      const float when = 22.0f + (float)(rh(s.id, (uint64_t)(uint32_t)nightIx) % 5u);   // 22:00 .. 02:59
      if (cur < when) continue;
    }
    c->raidNight = nightIx;
    art::Monster mon;
    int pack = 3, denIx = -1;
    const int pressure = pressureAt(*this, s, mon, pack, denIx, &Game::denClearedDay);
    const float mil = militaryOf(*this, s);
    const int pct = life::Life::raidChancePct(pressure, mil, ((day % realm::Realm::YEAR) + realm::Realm::YEAR) % realm::Realm::YEAR);
    if (pct > 0) life.stats.raidRolls++;
    if (forced) life.forceRaidSite = 0;
    if (!forced && !life::Life::raidRoll(s.id, nightIx, pct)) continue;
    // ---- a raid tonight
    const Vec2 heart(s.ex * TILE + 8.0f, s.ey * TILE + 8.0f);
    const float pd = len(pl().p - heart) / TILE;
    if (!inside && pd <= 120.0f && !R.site) {
      // live: along a road from the dens' side
      Vec2 dir(0, 1);
      if (denIx >= 0) {
        const Den& d = world.dens[(size_t)denIx];
        const Vec2 dd((float)(d.x - s.ex), (float)(d.y - s.ey));
        if (len2(dd) > 1.0f) dir = norm(dd);
      } else {
        const float ang = (float)(rh(s.id, (uint64_t)nightIx + 7) % 628u) / 100.0f;
        dir = Vec2(std::cos(ang), std::sin(ang));
      }
      // the footprint's edge on the dens' side (the ray from the heart leaves the settlement there)
      int rad = 4;
      while (rad < 200) {
        const int ex = s.ex + (int)std::lround(dir.x * (float)rad), ey = s.ey + (int)std::lround(dir.y * (float)rad);
        if (ex < s.r.x || ey < s.r.y || ex >= s.r.x + s.r.w || ey >= s.r.y + s.r.h) break;
        rad += 2;
      }
      int bx = s.ex + (int)std::lround(dir.x * (float)(rad + 8)), by = s.ey + (int)std::lround(dir.y * (float)(rad + 8));
      bool road = false;
      for (int dist = rad + 4; dist <= rad + 16 && !road; dist += 2) {
        const int cx = s.ex + (int)std::lround(dir.x * (float)dist), cy = s.ey + (int)std::lround(dir.y * (float)dist);
        for (int oy = -4; oy <= 4 && !road; oy++)
          for (int ox = -4; ox <= 4 && !road; ox++)
            if (world.over.in(cx + ox, cy + oy) && world.over.at(cx + ox, cy + oy) == Ground::Road) { bx = cx + ox; by = cy + oy; road = true; }
      }
      const int lvl = std::max(1, s.level);   // the place's own danger (the beasts that live round it)
      const int n = std::clamp((int)pack, 2, 4);
      R = life::Life::RaidLive();
      R.site = s.id; R.si = si; R.night = nightIx;
      for (int k = 0; k <= n; k++) {
        const bool elite = k == n;
        const Vec2 at = freeSpot(bx + (k % 3) - 1, by + (k / 3) - 1);
        const int id = spawnMonster(mon, at, lvl + (elite ? 3 : 0), false);
        const int ai = findActor(id);
        if (ai < 0) continue;
        Actor& a = actors[(size_t)ai];
        a.aggro = true; a.home = heart; a.goal = heart; a.fromMap = false; a.site = -1; a.den = -1; a.wild = false;
        if (elite) { a.maxHp *= 1.6f; a.hp = a.maxHp; a.dmg *= 1.25f; a.xp *= 2; a.name = std::string(monsterName(mon)) + " PACK LEADER"; }
        R.ids.push_back(id);
      }
      life.stats.raidsLive++;
      if (pd < 60.0f) emit(Ev::Notice, pl().p, (int)rgba(255, 150, 90), 0, "HOWLS ON THE ROAD TO " + s.name + "...");
    } else {
      // abstract: the beasts against the garrison and the militia
      life.stats.raidsAbstract++;
      const realm::SettlementState* st = realm.settlement(s.id);
      const int garrison = st ? (int)st->garrison : 1 + 3 * c->tier;
      int brave = 0;
      for (const life::Resident& r : c->res) if ((r.traits & life::TR_BRAVE) && !(r.flags & (life::RF_DEAD | life::RF_AWAY))) brave++;
      const int defence = garrison * 2 + brave / 3 + (int)std::lround(mil * 6.0f);
      const int attack = pressure * 3 + (int)(rh(s.id ^ 0xA77Aull, (uint64_t)nightIx) % 7u);
      const bool failed = attack > defence;
      if (failed) life.stats.raidsFailed++;
      raidOutcome(*this, si, *c, failed, nightIx);
    }
  }
}
