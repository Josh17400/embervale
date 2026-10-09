// rpg_test --foes [--seeds A..B] (or --foes A..B): M6 Steel FOES lane checks (VISION_PLAN 7.6, 15.19).
//   - elites: 6 % +-1 of 10,000 pack-member rolls at D >= 5 (and through the game's own spawns), none below D 5
//   - champion packs: about 1 % of packs at D >= 12; a champion pack is made whole (3 sharing their affixes)
//   - named uniques: 1-2 per region over 100 regions, deterministic (asked again, in another order, after the memo is
//     dropped: the same), distinct names, lairs on land and off settlements
//   - world bosses: one in every kingdom cell, a deterministic route about its lair, BeastRaid events in a 60-day run
//   - dungeon boss phases at 66 % and 33 %; every phase behaviour is reachable
//   - a headless fight per affix shows its effect; 200 splitting, volatile kills in one frame don't crash
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include "rpg/sim/foes.h"
#include "rpg/sim/game_internal.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

using FA = foes::Affix;
using foes::Rank;

void step(Game& g, int frames) {
  for (int i = 0; i < frames; i++) {
    g.update(SIM_DT, Input());
    if (g.mode != Mode::Play) g.mode = Mode::Play;
  }
}
Actor* byId(Game& g, int id) {
  for (Actor& a : g.actors) if (a.id == id) return &a;
  return nullptr;
}
bool textSeen(const Game& g, const char* t) {
  for (const Event& e : g.events) if (e.s.find(t) != std::string::npos) return true;
  return false;
}

// ---------------------------------------------------------------- rates
int rates(uint64_t seed) {
  int bad = 0;
  // the pure rolls
  int elites = 0, low = 0, n = 10000;
  for (int i = 0; i < n; i++) {
    const uint64_t h = ew::mix64(seed * 1000003ull + (uint64_t)i);
    if (foes::rollFoe(h, 5 + i % 30, art::Monster::Wolf, false).rank == Rank::Elite) elites++;
    if (foes::rollFoe(h, 1 + i % 4, art::Monster::Wolf, false).rank != Rank::Normal) low++;
  }
  if (elites < 500 || elites > 700) { out("FAIL: foes: %d elites in %d rolls at D >= 5 (want 6 %% +- 1)\n", elites, n); bad++; }
  if (low) { out("FAIL: foes: %d elites below D 5\n", low); bad++; }
  int packs = 0;
  const int np = 100000;
  for (int i = 0; i < np; i++) {
    uint16_t aff = 0;
    int pid = 0;
    if (foes::championPack(ew::mix64(seed ^ ((uint64_t)i << 20)), 12 + i % 20, art::Monster::Wolf, false, aff, pid)) {
      packs++;
      int bits = 0;
      for (int k = 0; k < (int)FA::COUNT; k++) bits += (aff >> k) & 1;
      if (bits != 3 || pid <= 0) { out("FAIL: foes: a champion pack with %d affixes, id %d\n", bits, pid); bad++; break; }
    }
  }
  if (packs < 850 || packs > 1150) { out("FAIL: foes: %d champion packs in %d (want about 1 %%)\n", packs, np); bad++; }
  // through the game's own spawns (foeSpawned): 3000 wolves at D 8 and 1000 at D 3
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  int ge = 0, gl = 0, names = 0;
  for (int i = 0; i < 4000; i++) {
    const bool hi = i < 3000;
    const int id = g.debugSpawnAt(art::Monster::Wolf, g.pl().p + Vec2(30.0f + (float)(i % 61) * 5.3f, -40.0f + (float)(i / 61) * 3.7f), hi ? 8 : 3);
    const Actor* a = byId(g, id);
    if (a && a->rank == (uint8_t)Rank::Elite) {
      (hi ? ge : gl)++;
      if (a->name.find("WOLF") == std::string::npos || a->name == "WOLF" || a->maxHp < 34.0f * 2.0f || !a->dropD) names++;
    }
    g.actors.resize(1);
  }
  if (ge < 120 || ge > 240) { out("FAIL: foes: %d elites of 3000 game spawns at D 8 (want about 180)\n", ge); bad++; }
  if (gl) { out("FAIL: foes: %d elites of 1000 game spawns at D 3\n", gl); bad++; }
  if (names) { out("FAIL: foes: %d elites without their name plate, HP x2.2 or loot danger\n", names); bad++; }
  out("foes rates: pure %.2f %% elites, %.2f %% champion packs; game %d / 3000 elites at D 8\n", 100.0 * elites / n, 100.0 * packs / np, ge);
  // a champion pack in play is made whole: three that share their affixes
  {
    Game c = g;
    c.godMode = true;
    int champ = -1;
    for (int i = 0; i < 36 * 36 && champ < 0; i++) {
      // one spawn per 6-tile cell of a 36 x 36 grid round the player (the pack key is the ground's cell and the day)
      const int id = c.debugSpawnAt(art::Monster::Wolf, c.pl().p + Vec2((float)(i % 36 - 18) * 96.0f + 8.0f, (float)(i / 36 - 18) * 96.0f + 8.0f), 15);
      const Actor* a = byId(c, id);
      if (a && a->rank == (uint8_t)Rank::Champion) champ = id;
      else c.actors.erase(std::remove_if(c.actors.begin() + 1, c.actors.end(), [&](const Actor& x) { return x.id == id; }), c.actors.end());
    }
    if (champ < 0) { out("FAIL: foes: no champion in 6000 spawns at D 15\n"); bad++; }
    else {
      const int pack = byId(c, champ)->pack;
      const uint16_t aff = byId(c, champ)->affixes;
      step(c, 2);
      int members = 0, same = 0;
      for (const Actor& a : c.actors) if (a.pack == pack && a.st != AState::Dead) { members++; same += a.affixes == aff && a.rank == (uint8_t)Rank::Champion; }
      if (members != 3 || same != 3) { out("FAIL: foes: a champion pack of %d (%d sharing its affixes), want 3\n", members, same); bad++; }
    }
  }
  return bad;
}

// ---------------------------------------------------------------- named uniques
int named(uint64_t seed) {
  int bad = 0;
  Game g(seed);
  g.world.generateEndlessAt(seed, 0, 0);
  if (!g.world.src) return 0;
  ew::EndlessSource& src = *g.world.src;
  foes::clearMemo();
  std::map<std::pair<int, int>, std::vector<foes::NamedUnique>> first;
  std::map<std::string, int> names;
  const ew::GTile sp = src.start().spawn;
  const int32_t r0x = sp.x >= 0 ? sp.x / ew::REGION : -((-sp.x + ew::REGION - 1) / ew::REGION);
  const int32_t r0y = sp.y >= 0 ? sp.y / ew::REGION : -((-sp.y + ew::REGION - 1) / ew::REGION);
  int total = 0, sea = 0, offLand = 0, nearTown = 0;
  for (int j = -5; j < 5; j++)
    for (int i = -5; i < 5; i++) {
      const int rx = r0x + i, ry = r0y + j;
      const std::vector<foes::NamedUnique> v = foes::namedInRegion(src, rx, ry);
      first[{rx, ry}] = v;
      total += (int)v.size();
      if (v.size() > 2) { out("FAIL: foes: region %d,%d has %zu named uniques\n", rx, ry, v.size()); bad++; }
      if (v.empty() && !(i == 0 && j == 0)) {
        // allowed only where the region is (nearly) all sea or bare rock
        int land = 0;
        for (int y = 0; y < 4; y++)
          for (int x = 0; x < 4; x++) {
            const ew::MacroSample m = src.macro(rx * ew::REGION + 32 + x * 64, ry * ew::REGION + 32 + y * 64);
            if (!m.water && ecoFamily(m.eco) != Biome::Ocean && ecoFamily(m.eco) != Biome::Beach && ecoFamily(m.eco) != Biome::Mountain) land++;
          }
        if (land > 3) { out("FAIL: foes: region %d,%d (%d/16 land samples) has no named unique\n", rx, ry, land); bad++; }
        else sea++;
      }
      for (const foes::NamedUnique& u : v) {
        names[u.name]++;
        const ew::MacroSample m = src.macro(u.gx, u.gy);
        if (m.water) offLand++;
        for (const ew::SettlementNode& s : src.settlementsIn(u.gx - 60, u.gy - 60, u.gx + 61, u.gy + 61))
          if ((int64_t)(s.x - u.gx) * (s.x - u.gx) + (int64_t)(s.y - u.gy) * (s.y - u.gy) < 60ll * 60ll) { nearTown++; break; }
        if (u.name.empty() || u.rumour.find(u.name) == std::string::npos || u.D < 1) { out("FAIL: foes: named unique without name / rumour / danger\n"); bad++; }
        int na = 0;
        for (int k = 0; k < (int)FA::COUNT; k++) na += (u.affixes >> k) & 1;
        if (na < 2 || na > 3) { out("FAIL: foes: %s has %d affixes\n", u.name.c_str(), na); bad++; }
      }
    }
  int dups = 0;
  for (auto& kv : names) if (kv.second > 1) { dups++; if (dups <= 3) out("FAIL: foes: the name %s is used %d times\n", kv.first.c_str(), kv.second); }
  if (dups) bad++;
  if (offLand || nearTown) { out("FAIL: foes: %d lairs in the water, %d within 60 tiles of a settlement\n", offLand, nearTown); bad++; }
  // deterministic: dropped memo, asked again in the reverse order, from a fresh source
  foes::clearMemo();
  Game g2(seed);
  g2.world.generateEndlessAt(seed, 4096, -4096);
  int diff = 0;
  for (int j = 4; j >= -5; j--)
    for (int i = 4; i >= -5; i--) {
      const std::vector<foes::NamedUnique> v = foes::namedInRegion(*g2.world.src, r0x + i, r0y + j);
      const std::vector<foes::NamedUnique>& w = first[{r0x + i, r0y + j}];
      if (v.size() != w.size()) { diff++; continue; }
      for (size_t k = 0; k < v.size(); k++)
        if (v[k].id != w[k].id || v[k].name != w[k].name || v[k].gx != w[k].gx || v[k].gy != w[k].gy || v[k].mon != w[k].mon || v[k].affixes != w[k].affixes) diff++;
    }
  if (diff) { out("FAIL: foes: named uniques differ when asked again (%d regions)\n", diff); bad++; }
  out("foes named: %d in 100 regions (%d sea / rock regions without), %zu distinct names\n", total, sea, names.size());
  return bad;
}

// ---------------------------------------------------------------- world bosses
int worldBosses(uint64_t seed) {
  int bad = 0;
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  if (!g.world.src) return 0;
  ew::EndlessSource& src = *g.world.src;
  const ew::GTile sp = src.start().spawn;
  const int32_t kx0 = ew::EndlessSource::kcellOf(sp.x), ky0 = ew::EndlessSource::kcellOf(sp.y);
  int cells = 0, bosses = 0;
  std::set<int> kinds;
  for (int dy = -2; dy <= 2; dy++)
    for (int dx = -2; dx <= 2; dx++) {
      const bool kingdom = src.kingdomOfCell(kx0 + dx, ky0 + dy) != 0;
      bool ok = false;
      const foes::WorldBoss b = foes::worldBossOf(src, kx0 + dx, ky0 + dy, ok);
      if (kingdom) cells++;
      if (!ok) { if (kingdom) { out("FAIL: foes: kingdom cell %d,%d has no world boss\n", kx0 + dx, ky0 + dy); bad++; } continue; }
      bosses++;
      kinds.insert((int)b.mon);
      if (b.name.empty() || b.D < 6 || b.range < 100 || b.routeN < 1) { out("FAIL: foes: world boss %s incomplete (D %d range %d route %d)\n", b.name.c_str(), b.D, b.range, b.routeN); bad++; }
      if (b.mon == art::Monster::Dragon && dx == 0 && dy == 0) { out("FAIL: foes: a dragon world boss in Ashfang's own cell\n"); bad++; }
      for (int day = 0; day < 12; day++)
        for (float h = 0; h < 24; h += 3) {
          int32_t x1, y1, x2, y2;
          foes::worldBossAt(b, day, h, x1, y1);
          foes::worldBossAt(b, day, h, x2, y2);
          const int64_t d = (int64_t)(x1 - b.lairX) * (x1 - b.lairX) + (int64_t)(y1 - b.lairY) * (y1 - b.lairY);
          if (x1 != x2 || y1 != y2 || d > (int64_t)b.range * b.range) { out("FAIL: foes: %s's route strays (day %d %.0fh)\n", b.name.c_str(), day, h); bad++; day = 99; break; }
        }
      // (M6 fixer r3, review: "BeastSlain news can name a world boss when a named unique of its species fell") the news
      // of a named unique's death never names the world boss (and a world boss's names it)
      if (dx == 0 && dy == 0) {
        if (foes::beastNameNear(src, (int)b.mon, b.lairX, b.lairY, (int)foes::Rank::Named) == b.name) {
          out("FAIL: foes: a named unique's death news names the world boss %s\n", b.name.c_str()); bad++;
        }
        if (foes::beastNameNear(src, (int)b.mon, b.lairX, b.lairY, (int)foes::Rank::WorldBoss) != b.name) {
          out("FAIL: foes: the world boss's death news does not name %s\n", b.name.c_str()); bad++;
        }
      }
      if (src.settlementsIn(b.lairX - 50, b.lairY - 50, b.lairX + 51, b.lairY + 51).size()) {
        for (const ew::SettlementNode& s : src.settlementsIn(b.lairX - 50, b.lairY - 50, b.lairX + 51, b.lairY + 51))
          if ((int64_t)(s.x - b.lairX) * (s.x - b.lairX) + (int64_t)(s.y - b.lairY) * (s.y - b.lairY) < 50ll * 50ll) { out("FAIL: foes: %s's lair is in %s's fields\n", b.name.c_str(), s.name.c_str()); bad++; break; }
      }
    }
  // a 60-day run: the raids reach the realm's news
  g.godMode = true;
  g.noWildSpawns = false;
  int raids0 = 0;
  for (const realm::WorldEvent& e : g.realm.events()) raids0 += e.type == realm::EvType::BeastRaid;
  for (int d = 0; d < 60; d++) {
    g.day++;
    g.hour = 12.0f;
    step(g, 40);
    g.actors.erase(std::remove_if(g.actors.begin() + 1, g.actors.end(), [](const Actor& a) { return a.hostile && !a.npc; }), g.actors.end());
  }
  int raids = 0;
  std::string line;
  for (const realm::WorldEvent& e : g.realm.events())
    if (e.type == realm::EvType::BeastRaid) { raids++; if (line.empty()) line = story::newsLine(g, e, 0); }
  raids -= raids0;
  if (raids < 1) { out("FAIL: foes: no BeastRaid in a 60-day run (%d world bosses near)\n", bosses); bad++; }
  out("foes world bosses: %d in %d cells (%d kingdom cells), %zu kinds; %d raids in 60 days: %s\n", bosses, 25, cells, kinds.size(), raids, line.c_str());
  return bad;
}

// ---------------------------------------------------------------- boss phases
int phases(uint64_t seed) {
  int bad = 0;
  std::set<int> seen;
  for (uint32_t s = 0; s < 200; s++) for (int i = 1; i <= 2; i++) seen.insert((int)foes::bossPhaseFor(art::Monster::Troll, i, s * 2654435761u));
  if (seen.size() != 4) { out("FAIL: foes: only %zu of the 4 phase behaviours ever chosen\n", seen.size()); bad++; }
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.noWildSpawns = true;
  g.godMode = true;
  const int id = g.debugSpawnAt(art::Monster::Troll, g.pl().p + Vec2(70, 0), 6);
  Actor* b = byId(g, id);
  b->boss = true;
  step(g, 2);
  b = byId(g, id);
  if (!b || b->bossPhase != 0) { out("FAIL: foes: a fresh boss already in a phase\n"); return bad + 1; }
  b->hp = b->maxHp * 0.6f;
  g.events.clear();
  step(g, 1);
  b = byId(g, id);
  const bool tele1 = textSeen(g, b ? b->name.c_str() : "?");
  if (!b || b->bossPhase != 1 || !tele1) { out("FAIL: foes: boss phase at 66 %%: phase %d, telegraph %d\n", b ? b->bossPhase : -1, tele1); bad++; }
  if (b) b->hp = b->maxHp * 0.3f;
  g.events.clear();
  step(g, 1);
  b = byId(g, id);
  if (!b || b->bossPhase != 2 || !textSeen(g, b->name.c_str())) { out("FAIL: foes: boss phase at 33 %%: phase %d\n", b ? b->bossPhase : -1); bad++; }
  return bad;
}

// ---------------------------------------------------------------- affixes in a fight
int affixFight(uint64_t seed) {
  int bad = 0;
  Game base(seed);
  base.newEndlessGame(seed);
  base.mode = Mode::Play;
  base.noWildSpawns = true;
  base.pl().maxHp = base.pl().hp = 50000;
  // open ground away from the start village (its watch would end the fight): 30+ tiles off, a clear 10 x 7 patch
  {
    const Site& h = base.world.sites[(size_t)base.world.startSite];
    int sx = -1, sy = -1;
    for (int r = 30; r < 120 && sx < 0; r += 3)
      for (int k = 0; k < 48 && sx < 0; k++) {
        const int tx = h.r.cx() + (int)std::lround(std::cos(k * 0.1309f) * r), ty = h.r.cy() + (int)std::lround(std::sin(k * 0.1309f) * r);
        if (!base.world.over.in(tx - 7, ty - 5) || !base.world.over.in(tx + 7, ty + 5) || base.world.siteAt(tx, ty, 5) >= 0) continue;
        bool clear = true;
        for (int y = ty - 3; y <= ty + 3 && clear; y++)
          for (int x = tx - 4; x <= tx + 5 && clear; x++)
            if (base.world.over.blocked(x, y)) clear = false;
        if (clear) { sx = tx; sy = ty; }
      }
    if (sx < 0) { out("WARN: foes: no open ground for the affix fight\n"); return 0; }
    base.pl().p = Vec2(sx * TILE + 8.0f, sy * TILE + 10.0f);
  }
  step(base, 2);
  std::string line = "foes affixes:";
  for (int f = 0; f < (int)FA::COUNT; f++) {
    const FA af = (FA)f;
    Game g = base;
    const int lvl = 6;
    const int nid = g.debugSpawnAt(art::Monster::Wolf, g.pl().p + Vec2(0, 300), lvl);   // a plain one to compare with
    const int id = g.debugSpawnAt(art::Monster::Wolf, g.pl().p + Vec2(40, 0), lvl);
    byId(g, id)->rank = (uint8_t)Rank::Elite;
    byId(g, id)->affixes = foes::affixBit(af);
    int ally = -1;
    if (af == FA::Warded) ally = g.debugSpawnAt(art::Monster::Wolf, g.pl().p + Vec2(40, 22), lvl);
    step(g, 1);   // the staged rank is applied
    Actor* e = byId(g, id);
    const Actor* n = byId(g, nid);
    if (!e || !n || e->rank != (uint8_t)Rank::Elite || e->maxHp < n->maxHp * 2.1f) { out("FAIL: foes: %s elite not applied\n", foes::affixInfo(af).name); bad++; continue; }
    bool ok = false;
    switch (af) {
      case FA::Swift: ok = e->speed > n->speed * 1.25f; break;
      case FA::Armoured: ok = e->armor > n->armor * 1.5f && e->speed < n->speed; break;
      case FA::Vampiric: {
        e->hp = e->maxHp * 0.5f;
        for (int k = 0; k < 900 && !ok; k++) { step(g, 1); for (const Event& ev : g.events) if (ev.type == Ev::Heal) ok = true; g.events.clear(); }
        break;
      }
      case FA::Frenzied: {
        e->hp = e->maxHp * 0.3f;
        e->atkCd = 5.0f;
        step(g, 30);
        e = byId(g, id);
        ok = e && e->atkCd < 5.0f - 30 * SIM_DT * 1.3f;
        break;
      }
      case FA::Splitting: {
        g.debugFell(id, true);
        step(g, 2);
        int lesser = 0;
        for (const Actor& a : g.actors) lesser += a.name.find("LESSER") != std::string::npos && a.st != AState::Dead;
        ok = lesser == 2;
        break;
      }
      case FA::Burning: {
        g.pl().p += Vec2(-90, 0);   // it must walk to the player
        for (int k = 0; k < 600 && !ok; k++) {
          step(g, 1);
          for (const Projectile& pr : g.projs) if (pr.kind == ProjKind::Fireball && !pr.fromPlayer && pr.fac == Faction::Player) ok = true;
        }
        break;
      }
      case FA::Frostbound: for (int k = 0; k < 900 && !ok; k++) { step(g, 1); ok = g.pl().slowT > 0; } break;
      case FA::Warded: {
        // the same arrow at the ally with and without the ward-bearer near
        auto shoot = [&](Game& gg, int target) {
          Actor* t = byId(gg, target);
          Projectile pr;
          pr.p = t->p + Vec2(-20, -7); pr.v = Vec2(300, 0); pr.dmg = 30; pr.fromPlayer = true; pr.owner = gg.pl().id; pr.life = 0.5f;
          const float hp0 = t->hp;
          gg.projs.push_back(pr);
          step(gg, 10);
          t = byId(gg, target);
          return t ? hp0 - t->hp : 0.0f;
        };
        Game h = g;
        const float with = shoot(g, ally);
        for (Actor& a : h.actors) if (a.id == id) a.p += Vec2(0, 400);
        const float without = shoot(h, ally);
        ok = with > 0 && without > 0 && with < without * 0.7f;
        break;
      }
      case FA::Summoner: {
        for (int k = 0; k < 60 * 14 && !ok; k++) {
          step(g, 1);
          for (const Actor& a : g.actors) if (a.scalePct == 85 && a.st != AState::Dead) ok = true;
        }
        break;
      }
      case FA::Blinking: {
        Vec2 last = e->p;
        for (int k = 0; k < 60 * 8 && !ok; k++) {
          step(g, 1);
          const Actor* x = byId(g, id);
          if (!x) break;
          int sparks = 0;
          for (const Event& ev : g.events) sparks += ev.type == Ev::Sparkle;
          if (len(x->p - last) > 15.0f || sparks >= 2) ok = true;   // (a jump, or the blink's out-and-in flash)
          g.events.clear();
          last = x->p;
        }
        break;
      }
      case FA::Regenerating: {
        g.pl().p += Vec2(0, -2000);   // far: nothing hits it, nothing it hits
        e = byId(g, id);
        e->hp = e->maxHp * 0.5f;
        e->aggro = false;
        const float h0 = e->hp;
        step(g, 120);
        e = byId(g, id);
        ok = e && e->hp > h0 + e->maxHp * 0.03f;
        break;
      }
      case FA::Volatile: {
        const float hp0 = g.pl().hp;
        e->p = g.pl().p + Vec2(12, 0);
        g.debugFell(id, true);
        bool boom = false;
        for (int k = 0; k < 90; k++) { step(g, 1); for (const Event& ev : g.events) boom = boom || ev.type == Ev::Explode; g.events.clear(); }
        ok = boom && g.pl().hp < hp0;
        break;
      }
      default: break;
    }
    if (!ok) { const Actor* x = byId(g, id); out("DEBUG %s: alive %d aggro %d affixT %.2f affixes %d hp %.0f/%.0f st %d dist %.0f plhp %.0f\n", foes::affixInfo(af).name, x ? 1 : 0, x ? x->aggro : -1, x ? x->affixT : -1.f, x ? x->affixes : -1, x ? x->hp : 0.f, x ? x->maxHp : 0.f, x ? (int)x->st : -1, x ? len(x->p - g.pl().p) : -1.f, g.pl().hp); }
    line += std::string(" ") + foes::affixInfo(af).name + (ok ? "" : " NO EFFECT") + ";";
    if (!ok) { out("FAIL: foes: the %s affix showed no effect in a fight\n", foes::affixInfo(af).name); bad++; }
  }
  out("%s\n", line.c_str());
  // 200 splitting, volatile kills in one frame
  {
    Game g = base;
    std::vector<int> ids;
    for (int i = 0; i < 200; i++) {
      const int id = g.debugSpawnAt(i % 2 ? art::Monster::Wolf : art::Monster::Skeleton, g.pl().p + Vec2(30.0f + (float)(i % 20) * 6.0f, -30.0f + (float)(i / 20) * 6.0f), 7);
      Actor* a = byId(g, id);
      a->rank = (uint8_t)Rank::Elite;
      a->affixes = foes::affixBit(FA::Splitting) | foes::affixBit(FA::Volatile);
      ids.push_back(id);
    }
    step(g, 1);
    for (int id : ids) {
      Actor* a = byId(g, id);
      if (a && a->st != AState::Dead) { a->reassembled = true; g.debugFell(id, true); }
    }
    step(g, 90);
    int alive = 0, nan = 0;
    for (const Actor& a : g.actors) { alive += a.st != AState::Dead; nan += !std::isfinite(a.p.x) || !std::isfinite(a.hp); }
    if (nan || !std::isfinite(g.pl().hp) || alive < 300) { out("FAIL: foes: 200 kills in a frame: %d alive after, %d NaN\n", alive, nan); bad++; }
  }
  return bad;
}

int foesCmd(int argc, char** argv) {
  uint64_t a = 1, b = 2;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) { if (!parseSeedRange(argv[++i], a, b)) { printf("bad --seeds\n"); return 2; } }
    else if (argv[i][0] != '-' && !parseSeedRange(argv[i], a, b)) { printf("bad seed range %s\n", argv[i]); return 2; }
  }
  int bad = 0;
  for (uint64_t s = a; s <= b; s++) {
    g_curSeed = s;
    const int sb = rates(s) + named(s) + worldBosses(s) + phases(s) + affixFight(s);
    printf("foes seed %llu: %d failure(s)\n", (unsigned long long)s, sb);
    bad += sb;
  }
  printf("foes: %d failure(s)\n", bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--foes", "M6 Steel foes: elite / champion rates, named uniques, world bosses and raids, boss phases, every affix in a fight, mass kills [--seeds A..B]", foesCmd);
