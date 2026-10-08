// rpg_test --folk [--seeds A..B] [--report] [--inns]: M5 "Hearth and Hall", the townsfolk as actors (TOWNSFOLK lane,
// rpg/sim/life_game.cpp). Per seed, headless, on the endless mainland around the start:
//   - a farming village at 12:00: at least 60 % of its farmers out in its fields (on or beside farmland), at 2:00 at most
//     10 % of the residents outdoors (the watch excepted);
//   - a town at 12:00: the smith at the anvil in the Hammer posture (the smithy's key person at their post); its
//     gathering place at 20:00: at least 4 seated patrons and the bard performing (Lute / Drum / Flute); at 2:00 at most
//     10 % of the residents outdoors;
//   - every key quest giver reachable in working hours (9:30, 12:00, 16:00): the innkeeper, the smith, the merchants,
//     the priest, the lord; the start village's innkeeper at 8:30 (the opening);
//   - nobody overlapping a solid tile or a piece of furniture (sleepers lie in their beds: the bed is theirs);
//   - nobody stuck: a walker whose plan moves them covers a tile in every 30 s (the morning's comings and goings);
//   - perf: 120 resident actors in play cost <= 1.5 ms per step on average (lifeStep + every resident's lifeFolk);
//   - inns rent their rooms upstairs only (owner rule): every inn of every culture has an upper floor and no guest room
//     on the ground floor (--inns runs only this).
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/interior_v4.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

using art::Building;
using art::Posture;
using art::Prop;

const cult::Culture& folkCulture(int a) {
  static std::vector<cult::Culture> cs;
  if (cs.empty())
    for (int k = 0; k < (int)cult::Archetype::COUNT; k++) cs.push_back(cult::Atlas::make((cult::Archetype)k, 1000u + (uint32_t)k * 7919u, 2));
  return cs[(size_t)a];
}

// an inn-like building of culture a, as the town generator asks for it (storeys from its blueprint)
Bldg innOf(Building t, int a, uint32_t seed, int urban, int w, int h) {
  Bldg b;
  b.type = t;
  b.r = IRect{20, 20, w, h};
  b.owner = Role::Innkeeper;
  b.seed = hash32(seed ^ ((uint32_t)t * 2654435761u) ^ ((uint32_t)a * 40503u));
  b.genVer = WORLDGEN_LATEST;
  b.biome = Biome::Plains;
  b.urban = (uint8_t)urban;
  b.wealth = (uint8_t)(seed % 4);
  b.hearth = true;
  b.site = 0;
  b.styled = true;
  b.arch = cult::buildingArch(folkCulture(a), 2, urban, b.wealth, b.seed);
  b.storeys = (uint8_t)bldgStoreysV7(t, b.r.w, b.r.h, b.seed);
  const bld::Blueprint bp = bldgBlueprint(b);
  b.storeys = (uint8_t)std::clamp((int)bp.interior.floors, 1, 3);
  return b;
}

int innScan(bool report) {
  int bad = 0, n = 0, one = 0, ground = 0, few = 0;
  std::map<std::string, int> why;
  for (int a = 0; a < (int)cult::Archetype::COUNT; a++)
    for (uint32_t s = 0; s < 24; s++)
      for (int urban = 0; urban < 4; urban++) {
        const int w = 6 + (int)(s % 3), h = 3 + (int)((s / 3) % 2);
        const Bldg b = innOf(Building::Inn, a, s * 977u + (uint32_t)urban, urban, w, h);
        n++;
        int g0 = 0, up = 0;
        for (int f = 0; f < b.floors(); f++) {
          Map m;
          genInterior(m, b, b.seed, f);
          for (const RoomInfo& R : m.rooms)
            if (R.kind == RoomKind::GuestRoom) (f == 0 ? g0 : up)++;
        }
        std::string w0;
        if (b.floors() < 2) { one++; w0 = "one floor"; }
        else if (g0) { ground++; w0 = "guest room on the ground floor"; }
        else if (up < 2) { few++; w0 = "fewer than 2 rooms upstairs"; }
        if (!w0.empty()) {
          why[std::string(cult::archetypeName((cult::Archetype)a)) + ": " + w0]++;
          bad++;
        }
      }
  if (report || bad) {
    out("  folk inns: %d inns, %d one-floor, %d with ground-floor guest rooms, %d with < 2 rooms upstairs\n", n, one, ground, few);
    for (auto& kv : why) out("    %s x%d\n", kv.first.c_str(), kv.second);
  }
  if (bad) out("FAIL: folk: %d of %d inns break the rooms-upstairs rule\n", bad, n);
  return bad;
}

// ---------------------------------------------------------------- the running game
void tick(Game& g, float secs) {
  const int frames = (int)(secs * 60);
  for (int f = 0; f < frames; f++) {
    g.update(SIM_DT, Input());
    g.events.clear();
    if (g.mode != Mode::Play) g.mode = Mode::Play;
  }
}
int tileXof(Vec2 p) { return (int)std::floor(p.x / TILE); }
int tileYof(Vec2 p) { return (int)std::floor((p.y - 2) / TILE); }
bool seated(Posture p) { return p == Posture::Sit || p == Posture::SitEat || p == Posture::SitDrink || p == Posture::SitFloor || p == Posture::SitFloorEat || p == Posture::SitFloorDrink; }
bool playing(Posture p) { return p == Posture::Lute || p == Posture::Drum || p == Posture::Flute; }

// stand at a settlement's heart (the window comes along), at an hour
void goTo(Game& g, int si, float hour) {
  if (g.inside) g.debugLeave();
  const ew::Gid id = g.world.sites[(size_t)si].id;
  const Site s = g.world.sites[(size_t)si];
  g.teleportGlobal(g.world.ox + s.ex, g.world.oy + s.ey + 1);
  g.hour = hour;
  tick(g, 2.5f);
  (void)id;
}

// nobody stands in a solid tile (walls, furniture): sleepers lie on their own reserved bed
int overlaps(Game& g, const char* where, bool report) {
  int bad = 0;
  const Map& m = g.map();
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    if (!a.npc || a.st == AState::Dead || a.fly) continue;
    if (a.posture == Posture::Sleep && a.useX >= 0) continue;
    const int x = tileXof(a.p), y = tileYof(a.p);
    if (!m.blocked(x, y)) continue;
    // (the generator's people who stand behind a counter on a walk-through prop are not ours; a stall keeper stands at
    //  its counter by design)
    if (a.stallKeeper || (a.resident < 0 && !a.critter)) continue;
    // (M5 fixer r2) a bather stands in a bath's pool by design (life_game.cpp bathe: one to a tile, apart)
    if (g.inside && a.resident >= 0 && a.posture == Posture::None && (m.at(x, y) == Ground::Water || m.at(x, y) == Ground::DeepWater) && !m.propAt(x, y))
      continue;
    if (bad < 3 || report) out("FAIL: folk: %s: %s (%s) inside a solid tile %d,%d (prop %d, posture %d)\n", where, a.name.c_str(),
                               a.critter ? "animal" : "resident", x, y, m.propAt(x, y) - 1, (int)a.posture);
    bad++;
  }
  return bad;
}

// residents outdoors now (the watch excepted) / residents alive
void outdoorShare(Game& g, int si, int& out_, int& alive) {
  out_ = alive = 0;
  life::Census* c = g.life.census(g.world, si);
  if (!c) return;
  for (const life::Resident& r : c->res) if (!(r.flags & (life::RF_DEAD | life::RF_AWAY)) && r.job != life::Job::Guard) alive++;
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    if (a.site == si && a.resident >= 0 && a.npc && a.role != Role::Guard && a.st != AState::Dead && !a.critter) out_++;
  }
}

// farmers of the settlement on or beside its farmland now / farmers in all
void farmersInField(Game& g, int si, int& inField, int& farmers) {
  inField = farmers = 0;
  life::Census* c = g.life.census(g.world, si);
  if (!c) return;
  const Map& m = g.world.over;
  for (const life::Resident& r : c->res) {
    if (r.job != life::Job::Farmer || (r.flags & (life::RF_DEAD | life::RF_AWAY))) continue;
    farmers++;
    bool f = false;
    int ax = -1, ay = -1;
    for (size_t k = 1; k < g.actors.size() && r.actor >= 0; k++) {
      const Actor& a = g.actors[k];
      if (a.id != r.actor) continue;
      const int x = tileXof(a.p), y = tileYof(a.p);
      ax = x; ay = y;
      for (int dy = -1; dy <= 1 && !f; dy++)
        for (int dx = -1; dx <= 1 && !f; dx++) f = m.at(x + dx, y + dy) == Ground::Farmland;
    }
    inField += f;
    if (std::getenv("EMB_FOLK_TRACE")) {
      const life::Plan p = g.life.plan(g.world, *c, r, g.day, g.hour);
      out("    farmer %s: %s at %s, actor %d at %d,%d, in a field %d\n", r.name.c_str(), life::actName(p.act), life::placeName(p.place), r.actor, ax, ay, f);
    }
  }
}

int farmlandNear(const Game& g, int si) {
  const Site& s = g.world.sites[(size_t)si];
  const Map& m = g.world.over;
  int n = 0;
  for (int y = s.r.y - 14; y < s.r.y + s.r.h + 14; y++)
    for (int x = s.r.x - 14; x < s.r.x + s.r.w + 14; x++) n += m.in(x, y) && m.at(x, y) == Ground::Farmland;
  return n;
}

// the settlements near the start (records the session holds), by kind
std::vector<int> settlementsNear(Game& g, SiteType t) {
  std::vector<std::pair<float, int>> v;
  const Site& h = g.world.sites[(size_t)g.world.startSite];
  for (int i = 0; i < (int)g.world.sites.size(); i++) {
    const Site& s = g.world.sites[(size_t)i];
    if (s.type != t) continue;
    v.push_back({std::hypot((float)(s.ex - h.ex), (float)(s.ey - h.ey)), i});
  }
  std::sort(v.begin(), v.end());
  std::vector<int> o;
  for (auto& p : v) o.push_back(p.second);
  return o;
}

int keyHere(Game& g, Role role) {
  for (size_t k = 1; k < g.actors.size(); k++)
    if (g.actors[k].npc && g.actors[k].role == role && g.actors[k].st != AState::Dead) return (int)k;
  return -1;
}

struct Totals {
  int seeds = 0;
  double perfMs = 0, perfWorst = 0;
  int perfResidents = 0;
};

int folkSeed(uint64_t seed, bool report, Totals& T) {
  int bad = 0;
  auto fail = [&](const std::string& s) { out("FAIL: folk seed %llu: %s\n", (unsigned long long)seed, s.c_str()); bad++; };
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.godMode = true;
  g.noWildSpawns = true;
  tick(g, 0.5f);
  // ---- the opening: the start village's innkeeper at 8:30
  {
    const Site& sv = g.world.sites[(size_t)g.world.startSite];
    int inn = -1;
    for (int b = sv.bldgFirst; b < sv.bldgFirst + sv.bldgCount; b++)
      if (g.world.over.bldgs[(size_t)b].type == Building::Inn) { inn = b; break; }
    if (inn >= 0) {
      g.hour = 8.5f;
      g.debugEnterBuilding(inn, 0);
      tick(g, 0.3f);
      if (keyHere(g, Role::Innkeeper) < 0) fail("the start village's innkeeper is not at the inn at 8:30");
      bad += overlaps(g, "start inn 8:30", report);
      g.debugLeave();
    }
  }
  // ---- a farming village: farmers in the fields at noon, the street empty at 2:00
  {
    const Site& h0 = g.world.sites[(size_t)g.world.startSite];
    const int32_t sgx = g.world.ox + h0.ex, sgy = g.world.oy + h0.ey;
    (void)g.world.findSiteNear(sgx, sgy, SiteType::Village, 8, false, (int)ew::Archetype::Farming);   // (loads the region plans around)
    std::vector<std::pair<float, ew::Gid>> cands;
    for (const Site& s : g.world.sites)
      if (s.type == SiteType::Village && s.archetype == (uint8_t)ew::Archetype::Farming)
        cands.push_back({std::hypot((float)(g.world.ox + s.ex - sgx), (float)(g.world.oy + s.ey - sgy)), s.id});
    std::sort(cands.begin(), cands.end());
    int vil = -1;
    for (size_t k = 0; k < cands.size() && k < 5 && vil < 0; k++) {
      int si = g.world.siteHandle(cands[k].second);
      g.world.ensureSiteRecords(si);
      life::Census* c = g.life.census(g.world, si);
      int farmers = 0;
      if (c) for (const life::Resident& r : c->res) farmers += r.job == life::Job::Farmer;
      if (farmers < 3) continue;
      goTo(g, si, 12.0f);
      si = g.world.siteHandle(cands[k].second);
      if (farmlandNear(g, si) >= 20) vil = si;
    }
    if (vil < 0) out("WARN: folk seed %llu: no farming village near the start\n", (unsigned long long)seed);
    else {
      const ew::Gid vid = g.world.sites[(size_t)vil].id;
      vil = g.world.siteHandle(vid);
      tick(g, 20.0f);   // (the noon block: those whose lunch began walk out to their field)
      int inField = 0, farmers = 0;
      farmersInField(g, vil, inField, farmers);
      bad += overlaps(g, "village noon", report);
      int o2 = 0, alive = 0;
      g.hour = 2.0f;
      tick(g, 3.0f);
      vil = g.world.siteHandle(vid);
      outdoorShare(g, vil, o2, alive);
      bad += overlaps(g, "village 2:00", report);
      const std::string vname = g.world.sites[(size_t)vil].name;
      if (report) out("  folk seed %llu: village %s: farmers in the fields at 12:00 %d/%d, outdoors at 2:00 %d/%d\n", (unsigned long long)seed, vname.c_str(),
                      inField, farmers, o2, alive);
      if (inField * 100 < farmers * 60) fail("village " + vname + ": only " + std::to_string(inField) + " of " + std::to_string(farmers) + " farmers in the fields at 12:00");
      if (o2 * 100 > alive * 10) fail("village " + vname + ": " + std::to_string(o2) + " of " + std::to_string(alive) + " residents outdoors at 2:00");
    }
  }
  // ---- a town: the smith, the tavern, the night, the key givers, the morning's walkers, the perf budget
  if (g.inside) g.debugLeave();
  const Site& h1 = g.world.sites[(size_t)g.world.startSite];
  int town = g.world.findSiteNear(g.world.ox + h1.ex, g.world.oy + h1.ey, SiteType::Town, 8);
  if (town < 0) { out("WARN: folk seed %llu: no town near the start\n", (unsigned long long)seed); return bad; }
  const ew::Gid tid = g.world.sites[(size_t)town].id;
  goTo(g, town, 7.0f);
  town = g.world.siteHandle(tid);
  {
    // the morning's comings and goings: nobody stuck
    int stuck = 0, walkers = 0;
    std::map<int, std::pair<Vec2, float>> track;   // id -> (position 30 s ago, seconds walking in the window)
    for (int s = 0; s < 75 * 4; s++) {
      tick(g, 0.25f);
      if (g.inside) break;
      for (size_t k = 1; k < g.actors.size(); k++) {
        const Actor& a = g.actors[k];
        if (a.resident < 0 || a.critter || a.site != town) continue;
        auto it = track.find(a.id);
        if (it == track.end()) { track[a.id] = {a.p, 0.0f}; continue; }
        if (a.st == AState::Walk && a.posture != Posture::Play) it->second.second += 0.25f;
        else { it->second = {a.p, 0.0f}; continue; }
        if (it->second.second >= 30.0f) {
          walkers++;
          if (len(a.p - it->second.first) < TILE) {
            if (stuck < 3) out("  folk seed %llu: stuck walker %s at %d,%d\n", (unsigned long long)seed, a.name.c_str(), tileXof(a.p), tileYof(a.p));
            stuck++;
          }
          it->second = {a.p, 0.0f};
        }
      }
    }
    town = g.world.siteHandle(tid);
    bad += overlaps(g, "town morning", report);
    const Game::LifeFolkStats& st = g.lifeFolkStats();
    if (report) out("  folk seed %llu: town %s morning: %d residents out, %d walking, paths %d (cache hits %d, failed %d), stuck %d (watchdog %d)\n",
                    (unsigned long long)seed, g.world.sites[(size_t)town].name.c_str(), st.residents, st.walking, st.pathRequests, st.pathCacheHits,
                    st.pathFails, stuck, st.stuck);
    if (stuck) fail(std::to_string(stuck) + " walkers in " + g.world.sites[(size_t)town].name + " moved less than a tile in 30 s");
  }
  {
    // the smith at the anvil at noon (and every key giver in working hours)
    const Site st = g.world.sites[(size_t)town];
    struct Key { Building t; Role r; };
    const Key keys[] = {{Building::Inn, Role::Innkeeper}, {Building::Smithy, Role::Smith}, {Building::Shop, Role::Merchant}, {Building::Temple, Role::Priest}};
    for (float h : {9.5f, 12.0f, 16.0f})
      for (const Key& k : keys) {
        int b = -1;
        for (int bi = st.bldgFirst; bi < st.bldgFirst + st.bldgCount; bi++)
          if (g.world.over.bldgs[(size_t)bi].type == k.t && g.world.over.bldgs[(size_t)bi].charred != 2) { b = bi; break; }
        if (b < 0) continue;
        g.hour = h;
        g.debugEnterBuilding(b, 0);
        tick(g, 0.4f);
        const int ki = keyHere(g, k.r);
        if (ki < 0) fail(std::string("no ") + bldgTypeName(k.t) + " keeper in " + st.name + " at " + std::to_string((int)h) + ":" + (h - (int)h > 0 ? "30" : "00"));
        else if (k.t == Building::Smithy && h == 12.0f) {
          const Actor& a = g.actors[(size_t)ki];
          bool anvil = false;
          const int x = tileXof(a.p), y = tileYof(a.p);
          for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) anvil |= g.sub.propAt(x + dx, y + dy) == (int)Prop::Anvil + 1;
          bool hasAnvil = false;
          for (uint8_t p : g.sub.prop) hasAnvil |= p == (int)Prop::Anvil + 1;
          if (report) out("  folk seed %llu: smith %s at 12:00: posture %d, at the anvil %d\n", (unsigned long long)seed, a.name.c_str(), (int)a.posture, anvil);
          if (a.posture != Posture::Hammer) fail("the smith of " + st.name + " is not hammering at 12:00");
          if (hasAnvil && !anvil) fail("the smith of " + st.name + " is not at the anvil at 12:00");
        }
        bad += overlaps(g, (std::string(bldgTypeName(k.t)) + " daytime").c_str(), report);
        g.debugLeave();
      }
  }
  {
    // the gathering place at 20:00: seated patrons and the bard
    life::Census* c = g.life.census(g.world, town);
    const Site st = g.world.sites[(size_t)town];
    if (c && c->gathering >= 0 && c->gathering < st.bldgCount) {
      g.hour = 20.0f;
      g.debugEnterBuilding(st.bldgFirst + c->gathering, 0);
      tick(g, 2.0f);
      int sit = 0, bard = 0, people = 0;
      for (size_t k = 1; k < g.actors.size(); k++) {
        const Actor& a = g.actors[k];
        if (!a.npc || a.st == AState::Dead) continue;
        people++;
        if (a.resident >= 0 && seated(a.posture)) sit++;
        if (playing(a.posture)) bard++;
      }
      // seats are never shared
      std::set<std::pair<int, int>> seats;
      for (size_t k = 1; k < g.actors.size(); k++) {
        const Actor& a = g.actors[k];
        if (!seated(a.posture)) continue;
        if (!seats.insert({tileXof(a.p), tileYof(a.p)}).second) { fail("two patrons on one seat in " + st.name); if (report) out("  at %d,%d posture %d %s p %.1f,%.1f prop %d\n", tileXof(a.p), tileYof(a.p), (int)a.posture, a.name.c_str(), a.p.x, a.p.y, g.sub.propAt(tileXof(a.p), tileYof(a.p))); }
      }
      if (report) out("  folk seed %llu: %s's %s at 20:00: %d people, %d seated, bard %d\n", (unsigned long long)seed, st.name.c_str(),
                      bldgTypeName(g.world.over.bldgs[(size_t)(st.bldgFirst + c->gathering)].type), people, sit, bard);
      if (sit < 4) fail(std::string("only ") + std::to_string(sit) + " seated patrons in " + st.name + "'s gathering place at 20:00");
      if (!bard && c->tier >= 1) fail("no bard performing in " + st.name + "'s gathering place at 20:00");
      bad += overlaps(g, "tavern 20:00", report);
      g.debugLeave();
    } else out("WARN: folk seed %llu: %s gathers outdoors (no gathering building)\n", (unsigned long long)seed, st.name.c_str());
  }
  {
    // the town at 2:00
    goTo(g, town, 2.0f);
    town = g.world.siteHandle(tid);
    int o2 = 0, alive = 0;
    outdoorShare(g, town, o2, alive);
    bad += overlaps(g, "town 2:00", report);
    if (report) out("  folk seed %llu: town %s outdoors at 2:00 %d/%d\n", (unsigned long long)seed, g.world.sites[(size_t)town].name.c_str(), o2, alive);
    if (o2 * 100 > alive * 10) fail(g.world.sites[(size_t)town].name + ": " + std::to_string(o2) + " of " + std::to_string(alive) + " residents outdoors at 2:00");
  }
  {
    // perf: 120 resident actors (the biggest place near the start, the cap raised for the test)
    int big = g.world.findSiteNear(g.world.ox + g.world.sites[(size_t)town].ex, g.world.oy + g.world.sites[(size_t)town].ey, SiteType::City, 8);
    if (big < 0) big = town;
    const ew::Gid bid = g.world.sites[(size_t)big].id;
    g.lifeFolkCap = 160;
    goTo(g, big, 12.0f);
    big = g.world.siteHandle(bid);
    tick(g, 4.0f);
    const Game::LifeFolkStats s0 = g.lifeFolkStats();
    int minRes = 1 << 30;
    for (int f = 0; f < 6 * 60; f++) {
      tick(g, 1.0f / 60.0f);
      minRes = std::min(minRes, g.lifeFolkStats().residents);
    }
    const Game::LifeFolkStats& s1 = g.lifeFolkStats();
    const double avg = (s1.sumMs - s0.sumMs) / std::max(1, s1.steps - s0.steps);
    T.perfMs = std::max(T.perfMs, avg);
    T.perfWorst = std::max(T.perfWorst, s1.worstMs);
    T.perfResidents = std::max(T.perfResidents, minRes);
    if (report) out("  folk seed %llu: perf in %s: %d resident actors (at least), %.3f ms per step on average (worst %.2f), %d animals\n",
                    (unsigned long long)seed, g.world.sites[(size_t)big].name.c_str(), minRes, avg, s1.worstMs, s1.critters);
    if (minRes >= 120 && avg > 1.5) fail("120 resident actors cost " + std::to_string(avg) + " ms per step (budget 1.5)");
    bad += overlaps(g, "city noon", report);
    g.lifeFolkCap = 0;
  }
  T.seeds++;
  return bad;
}

int folkCmd(int argc, char** argv) {
  uint64_t A = 1, B = 5;
  bool report = false, innsOnly = false;
  for (int i = 2; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], A, B);
    else if (!strcmp(argv[i], "--report")) report = true;
    else if (!strcmp(argv[i], "--inns")) innsOnly = true;
  }
  int bad = innScan(report);
  Totals T;
  if (!innsOnly)
    for (uint64_t s = A; s <= B; s++) { g_curSeed = s; bad += folkSeed(s, report, T); }
  if (!innsOnly) printf("folk: %d seeds, perf worst average %.3f ms per step with %d resident actors (worst step %.2f ms)\n", T.seeds, T.perfMs,
                        T.perfResidents, T.perfWorst);
  printf("folk: %d failures\n", bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--folk", "M5 townsfolk as actors: schedules, furniture, taverns, key givers, the night, the perf budget [--seeds A..B] [--report] [--inns]",
             folkCmd);
