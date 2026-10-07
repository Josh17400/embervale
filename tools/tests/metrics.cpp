// rpg_test: --metrics, game-feel and balance bots (PLAN.md targets table).
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include "rpg/sim/game_internal.h"
#include "tools/tests/tests.h"

bool openingStep(Game& g, std::vector<int>& path, int& clock, Input& in);   // seed_run.cpp: the opening bot

namespace {
// ---- game-feel metrics (PLAN.md "Game-feel and balance targets"): a bot that fights like an average player
// It walks in, swings when in reach, backs off when out of stamina, and rolls away from a telegraphed attack it
// notices (skill = chance of noticing each windup). Heavy attacks (bear/troll) are noticed more often (they read
// from further away) but the bot still misses some.
struct Bot {
  Rng r{1};
  float skill = 0.5f;
  std::map<long long, bool> seen;   // (enemy id, windup start) -> will roll for it
  int swingCd = 0, bowCd = 0;
  int rolls = 0;
  bool retreat = false;
  std::map<int, float> ignore;      // unreachable targets (stuck chasing them) -> until when
  int chase = -1; float chaseT = 0, chaseBest = 1e9f;
  Vec2 chaseP;
  Input think(Game& g, bool ranged, int* targetOut = nullptr) {
    Input in;
    const Actor& p = g.pl();
    int tgt = -1; float bd = 200 * 200;
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& e = g.actors[k];
      if (!e.hostile || e.st == AState::Dead || e.fly) continue;
      auto ig = ignore.find(e.id);
      if (ig != ignore.end() && ig->second > g.time) continue;
      float d2 = len2(e.p - p.p);
      if (d2 < bd) { bd = d2; tgt = (int)k; }
    }
    if (targetOut) *targetOut = tgt;
    if (swingCd > 0) swingCd--;
    if (bowCd > 0) bowCd--;
    // dodge: a telegraphed swing we noticed, about to land
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& e = g.actors[k];
      if (!e.hostile || e.st != AState::Windup) continue;
      float d = len(e.p - p.p);
      if (d > e.range + (e.lunge ? 48 : e.heavy ? 34 : 26)) continue;
      float wu = e.heavy ? heavyWindup(e.mon) : e.windup;   // (game_internal.h: the brutes, the sting, the spores)
      long long key = (long long)e.id * 100000 + (long long)((g.time - e.stT) * 20);
      auto it = seen.find(key);
      if (it == seen.end()) {
        bool heavy = e.heavy;
        it = seen.emplace(key, r.f() < (heavy ? std::min(0.9f, skill + 0.3f) : skill)).first;
      }
      if (it->second && e.stT >= wu - 0.14f && g.stamina >= 20) {
        Vec2 away = norm(p.p - e.p);
        Vec2 side(-away.y, away.x);
        if (r.f() < 0.5f) side = side * -1.0f;
        in.move = norm(away + side * 0.8f);
        in.roll = true;
        it->second = false;
        rolls++;
        return in;
      }
    }
    if (tgt < 0) return in;
    if (retreat && p.hp < p.maxHp * 0.25f) {   // out of potions and nearly dead: run (regen comes back once clear)
      bool pots = false;
      for (auto& it : g.inv) if (it.kind == ItemKind::Potion && it.sub == 0) pots = true;
      if (!pots) { in.move = norm(p.p - g.actors[tgt].p); if (g.stamina > 40 && r.f() < 0.02f) in.roll = true; return in; }
    }
    const Actor& e = g.actors[tgt];
    float d = std::sqrt(bd);
    // give up on a target we cannot get closer to for 4 s (across water, behind rock)
    // (progress is measured across targets: a stuck pack swaps "nearest" every few frames)
    if (chase < 0 || g.time - chaseT > 8.0f) { chase = e.id; chaseT = g.time; chaseBest = d; chaseP = p.p; }
    float reach0 = 17 + e.radius;
    if (d < chaseBest - 8 || d <= reach0) { chaseBest = d; chaseT = g.time; chaseP = p.p; }
    if (g.time - chaseT > 4.0f && d > reach0) {
      ignore[e.id] = g.time + 20.0f;
      for (const Actor& o : g.actors) if (o.hostile && (len2(o.p - p.p) < 90 * 90 || len2(o.p - e.p) < 60 * 60)) ignore[o.id] = g.time + 20.0f;
      chase = -1; if (targetOut) *targetOut = -1; return in;
    }
    if (g.time - chaseT > 1.5f && d > reach0) {   // blocked: walk around the obstacle
      Vec2 to0 = norm(e.p - p.p);
      in.move = Vec2(-to0.y, to0.x) * ((e.id & 1) ? 1.0f : -1.0f) + to0 * 0.3f;
      return in;
    }
    Vec2 to = norm(e.p - p.p);
    float reach = 17 + e.radius;
    if (g.stamina < 9 && d < 40) { in.move = to * -1.0f; return in; }   // winded: back off and recover
    if (ranged && d > 60 && bowCd == 0) { in.move = to * 0.2f; in.bow = true; bowCd = 30; return in; }
    if (d > reach) in.move = to;
    else if (swingCd == 0) { in.attack = true; swingCd = 9; in.move = to * 0.01f; }
    return in;
  }
};

// a quiet open patch of grassland far from every site, where a test fight can run undisturbed
bool arenaSpot(Game& g, int& ax, int& ay, int rx = 6, int ry = 4);
bool arenaSpot(Game& g, int& ax, int& ay, int rx, int ry) {
  const Map& m = g.world.over;
  const Site& home = g.world.sites[g.world.startSite];
  int best = -1; float bd = 1e30f;
  for (int y = 12; y < m.h - 12; y += 3)
    for (int x = 12; x < m.w - 12; x += 3) {
      Biome b = m.biomeAt(x, y);
      if (b != Biome::Plains && b != Biome::Forest && b != Biome::Autumn) continue;
      if (g.world.siteAt(x, y, 12) >= 0) continue;
      bool open = true;
      for (int oy = -ry; oy <= ry && open; oy++) for (int ox = -rx; ox <= rx && open; ox++) if (m.blocked(x + ox, y + oy)) open = false;
      if (!open) continue;
      float d = std::hypot((float)(x - home.r.cx()), (float)(y - home.r.cy()));
      if (d < bd) { bd = d; best = y * m.w + x; }
    }
  if (best < 0) return rx > 3 ? arenaSpot(g, ax, ay, rx - 1, ry - 1) : false;
  ax = best % m.w; ay = best / m.w;
  return true;
}

// a level-L character: health perks every level, a weapon and (from level 3) armour of their level
void makeHero(Game& g, int L) {
  if (g.eqWeapon < 0) g.debugKit();   // the real start is shirt only (M0): arena fights use the old kit
  g.plLevel = L;
  g.perkPts = L - 1;
  for (int k = 1; k < L; k++) g.chooseLevelUp(0);
  g.mode = Mode::Play;
  g.inv[g.eqWeapon].power = (int16_t)(8 + (L - 1) / 2);
  if (L >= 3) {
    Item a; a.kind = ItemKind::Armor; a.power = (int16_t)(8 + L / 2); a.name = "LEATHER ARMOR"; a.icon = art::Icon::Armor; a.value = 40;
    g.inv.push_back(a);
    g.useItem((int)g.inv.size() - 1);
  }
  g.pl().hp = g.pl().maxHp; g.stamina = g.maxSt;
}

struct FightResult { bool died = false, won = false; float secs = 0, hpLeft = 0; int hits = 0, rolls = 0, hitsTaken = 0; };

FightResult arenaFight(uint64_t seed, int trial, art::Monster mon, int n, int L, float timeout, bool fleeWins, bool fists = false) {
  FightResult fr;
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.noWildSpawns = true;
  g.hour = 12;
  int ax, ay;
  if (!arenaSpot(g, ax, ay)) { static bool warned = false; if (!warned) printf("  (seed %llu: no open arena spot)\n", (unsigned long long)seed); warned = true; return fr; }
  g.pl().p = Vec2(ax * TILE + 8.0f, ay * TILE + 8.0f);
  for (int f = 0; f < 4; f++) g.update(SIM_DT, Input());
  g.actors.resize(1);
  makeHero(g, L);
  if (fists) { g.eqWeapon = -1; g.eqBow = -1; }   // the shirt-only opening: bare knuckles
  Rng rr((uint32_t)(seed * 7919 + trial * 104729 + (int)mon * 31 + L));
  std::vector<int> ids;
  float a0 = rr.f() * TAU;
  for (int k = 0; k < n; k++) {
    float a = a0 + k * TAU / n + rr.range(-0.3f, 0.3f);
    ids.push_back(g.debugSpawnAt(mon, g.pl().p + Vec2(std::cos(a), std::sin(a) * 0.7f) * rr.range(70, 95), L));
  }
  Bot bot; bot.r = Rng((uint32_t)(seed * 31 + trial * 977 + L));
  std::map<int, float> lastHp;
  float plHp = g.pl().hp;
  int frames = (int)(timeout * 60);
  for (int f = 0; f < frames; f++) {
    Input in = bot.think(g, false);
    g.update(SIM_DT, in);
    g.events.clear();
    if (g.mode == Mode::LevelUp || g.mode == Mode::Dialogue) g.mode = Mode::Play;
    if (getenv("RPG_TRACE_ARENA") && trial == 0 && f % 6 == 0) {
      printf("  t%5.2f pl hp %3.0f st %d stam %2.0f |", f / 60.0f, g.pl().hp, (int)g.pl().st, g.stamina);
      for (size_t j = 1; j < g.actors.size(); j++) {
        const Actor& e = g.actors[j];
        printf(" [st%d d%3.0f cd%4.1f sp%3.1f %c%c hp%2.0f]", (int)e.st, len(e.p - g.pl().p), e.atkCd, e.special, e.lunge ? 'L' : '-', e.aggro ? 'A' : '-', e.hp);
      }
      printf("\n");
    }
    if (g.pl().hp < plHp - 0.5f) fr.hitsTaken++;
    plHp = g.pl().hp;
    int alive = 0;
    for (int id : ids) {
      int k = -1;
      for (size_t j = 1; j < g.actors.size(); j++) if (g.actors[j].id == id) k = (int)j;
      if (k < 0) continue;
      const Actor& e = g.actors[k];
      auto it = lastHp.find(id);
      if (it != lastHp.end() && e.hp < it->second - 0.01f && g.pl().st == AState::Strike) fr.hits++;
      lastHp[id] = e.hp;
      if (e.st != AState::Dead && !(fleeWins && e.fleeing)) alive++;   // a fleeing enemy has lost the fight
    }
    if (g.mode == Mode::Dead) { fr.died = true; fr.secs = f / 60.0f; return fr; }
    if (!alive) { fr.won = true; fr.secs = f / 60.0f; fr.hpLeft = g.pl().hp / g.pl().maxHp; fr.rolls = bot.rolls; return fr; }
  }
  fr.secs = timeout;
  fr.rolls = bot.rolls;
  return fr;
}

struct ProgResult { float weapon = -1, lvl2 = -1, lvl5 = -1; int deaths = 0, potions = 0, kills = 0, level = 1, gold = 0, dens = 0; std::string killers; };

ProgResult progression(uint64_t seed, float secs) {
  ProgResult pr;
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  Bot bot; bot.r = Rng((uint32_t)seed * 2654435761u); bot.retreat = true;
  Rng r(seed);
  Input wander;
  Vec2 lastPos;
  int insideT = 0;
  int frames = (int)(secs * 60);
  std::vector<int> openPath;
  int openClock = 0;
  bool opening = true;   // the real start (M0): shirt only, so first fetch the old blade from the start inn
  for (int f = 0; f < frames; f++) {
    int tgt = -1;
    Input in = bot.think(g, g.eqBow >= 0, &tgt);
    bool walkingOpening = false;
    if (opening) {
      Input oin;
      if (openingStep(g, openPath, openClock, oin)) opening = false;
      else if (tgt < 0 || len2(g.actors[(size_t)tgt].p - g.pl().p) > 40 * 40 || g.inside) { in = oin; walkingOpening = true; }   // fists only for what is in our face
    }
    if (pr.weapon < 0 && g.eqWeapon >= 0) pr.weapon = f / 60.0f;
    if (tgt < 0 && !walkingOpening) {
      // wander; turn when blocked; walk back out of buildings and dungeons after a while
      if (f % 240 == 0 || (f % 30 == 0 && len2(g.pl().p - lastPos) < 4 * 4)) { float a = r.f() * TAU; wander.move = Vec2(std::cos(a), std::sin(a)); }
      if (f % 30 == 0) lastPos = g.pl().p;
      in.move = wander.move;
      if (g.inside) {
        insideT++;
        if (insideT > (g.subBldg >= 0 ? 300 : 3600)) {
          Vec2 ex(g.sub.exitX * TILE + 8.0f, g.sub.exitY * TILE + 8.0f);
          Vec2 d = ex - g.pl().p;
          in.move = len2(d) > 30 * 30 || f % 120 < 90 ? norm(d) : wander.move;
          if (len2(d) < 24 * 24) in.move = std::fabs(d.x) > 2 ? Vec2(d.x > 0 ? 1.0f : -1.0f, 0) : Vec2(0, 1);
        }
      } else insideT = 0;
      if (!opening) in.interact = (f % 60) == 0;
    }
    if (g.pl().hp < g.pl().maxHp * 0.3f && f % 30 == 0) {
      int before = 0; for (auto& it : g.inv) if (it.kind == ItemKind::Potion) before += it.count;
      in.potion = true;
      g.update(SIM_DT, in);
      int after = 0; for (auto& it : g.inv) if (it.kind == ItemKind::Potion) after += it.count;
      if (after < before) pr.potions++;
    } else g.update(SIM_DT, in);
    g.events.clear();
    if (g.mode == Mode::Dialogue) { if (!opening) g.dialogueChoose(0); g.mode = Mode::Play; }
    if (g.mode == Mode::Shop || g.mode == Mode::LevelUp) g.mode = Mode::Play;
    while (g.perkPts > 0) { g.chooseLevelUp(0); g.mode = Mode::Play; }
    if (g.mode == Mode::Dead) {
      pr.deaths++;
      const Actor* k = nullptr; float kd = 1e9f;
      for (size_t j = 1; j < g.actors.size(); j++) { const Actor& e = g.actors[j]; if (e.hostile && e.st != AState::Dead && len2(e.p - g.pl().p) < kd) { kd = len2(e.p - g.pl().p); k = &e; } }
      if (k) { if (!pr.killers.empty()) pr.killers += ","; pr.killers += k->name + "@" + std::to_string(k->level); }
      g.respawn();
    }
    if (g.inside && f % 600 == 0 && r.f() < 0.5f) { /* wander out of buildings eventually */ }
    float t = f / 60.0f;

    if (getenv("RPG_TRACE") && f > 25200 && f < 25500 && f % 15 == 0) {
      printf("      st %d move %.2f,%.2f tgt %d stam %.0f hp %.0f p %.1f,%.1f time %.2f chaseT %.2f best %.1f", (int)g.pl().st, in.move.x, in.move.y, tgt, g.stamina, g.pl().hp, g.pl().p.x, g.pl().p.y,
             g.time, bot.chaseT, bot.chaseBest);
      if (tgt >= 0 && tgt < (int)g.actors.size()) printf(" -> %s at %.0f", g.actors[tgt].name.c_str(), len(g.actors[tgt].p - g.pl().p));
      printf("\n");
    }
    if (getenv("RPG_TRACE") && f % 3600 == 0)
      printf("    t %4.0f pos %d,%d inside %d bldg %d site %d mode %d lvl %d kills %d loc %s\n", t, (int)(g.pl().p.x / TILE), (int)(g.pl().p.y / TILE), g.inside, g.subBldg, g.subSite,
             (int)g.mode, g.plLevel, g.kills, g.locName.c_str());
    if (getenv("RPG_TRACE") && f % 3600 == 0) {   // what surrounds the player (1 = blocked), to spot a bot wedged in a wall
      int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
      printf("      around %.1f,%.1f:", g.pl().p.x, g.pl().p.y);
      for (int oy = -1; oy <= 1; oy++) { printf(" "); for (int ox = -1; ox <= 1; ox++) printf("%d", g.map().blocked(px + ox, py + oy) ? 1 : 0); }
      printf("  ground %d prop %d\n", (int)g.map().at(px, py), g.map().propAt(px, py));
    }
    if (pr.lvl2 < 0 && g.plLevel >= 2) pr.lvl2 = t;
    if (pr.lvl5 < 0 && g.plLevel >= 5) pr.lvl5 = t;
  }
  pr.kills = g.kills; pr.level = g.plLevel; pr.gold = g.gold;
  for (auto& kv : g.killedSlots) if (kv.first == -1) pr.dens = (int)kv.second.size();
  return pr;
}

}  // namespace

int runMetrics(uint64_t A, uint64_t B, float progSecs) {
  printf("game-feel metrics, seeds %llu..%llu (bot skill 0.5: notices half the normal windups)\n", (unsigned long long)A, (unsigned long long)B);
  // 1. progression
  if (progSecs > 0) {
    double s2 = 0, s5 = 0, sw = 0; float maxW = 0; int nw = 0, n2 = 0, n5 = 0, deaths = 0, pots = 0, n = 0, kills = 0, dens = 0;
    for (uint64_t s = A; s <= B; s++) {
      ProgResult p = progression(s, progSecs);
      printf("  prog seed %-4llu weapon %4.0fs  lvl2 %5.0fs  lvl5 %5.0fs  final lvl %d  kills %3d  deaths %d  potions %d  gold %d  dens cleared %d  killed by: %s\n", (unsigned long long)s,
             p.weapon, p.lvl2, p.lvl5, p.level, p.kills, p.deaths, p.potions, p.gold, p.dens, p.killers.c_str());
      fflush(stdout);
      if (p.weapon >= 0) { sw += p.weapon; nw++; maxW = std::max(maxW, p.weapon); }
      if (p.lvl2 >= 0) { s2 += p.lvl2; n2++; }
      if (p.lvl5 >= 0) { s5 += p.lvl5; n5++; }
      deaths += p.deaths; pots += p.potions; kills += p.kills; dens += p.dens; n++;
    }
    printf("METRIC first weapon: avg %.0f s, worst %.0f s (%d/%d armed)   [target <= 300 s]\n", nw ? sw / nw : -1.0, (double)maxW, nw, n);
    printf("METRIC first level-up: avg %.1f min (%d/%d reached)   [target 3-5 min]\n", n2 ? s2 / n2 / 60 : -1.0, n2, n);
    printf("METRIC level 5: avg %.1f min (%d/%d reached in %.0f min)   [target 25-35 min]\n", n5 ? s5 / n5 / 60 : -1.0, n5, n, progSecs / 60);
    printf("METRIC bot run: deaths %.2f, potions %.2f, kills %.0f, dens cleared %.1f per run\n", (double)deaths / n, (double)pots / n, (double)kills / n, (double)dens / n);
  }
  // 2. three same-level wolves, no potions
  for (int L : {1, 3, 5}) {
    int died = 0, trials = 0; double secs = 0, hp = 0; int won = 0, taken = 0, rolls = 0;
    for (uint64_t s = A; s <= B; s++)
      for (int t = 0; t < 8; t++) {
        FightResult fr = arenaFight(s, t, art::Monster::Wolf, 3, L, 90, true);
        trials++; if (fr.died) died++; taken += fr.hitsTaken; rolls += fr.rolls;
        if (fr.won) { won++; secs += fr.secs; hp += fr.hpLeft; }
      }
    printf("METRIC 3 wolves lvl %d, no potions: died %d%% (%d/%d), won fights %.1fs avg, %.0f%% hp left, %.1f hits taken, %.1f rolls   [target ~25%%]\n", L, 100 * died / std::max(1, trials), died, trials,
           won ? secs / won : 0.0, won ? 100 * hp / won : 0.0, (double)taken / trials, (double)rolls / trials);
    fflush(stdout);
  }
  // 2b. the shirt-only opening: bare fists against one level-1 beast
  {
    printf("METRIC fists at lvl 1:");
    for (art::Monster m : {art::Monster::Wolf, art::Monster::Boar, art::Monster::Goblin}) {
      double hits = 0, secs = 0; int n = 0, died = 0, trials = 0;
      for (uint64_t s = A; s <= B; s++)
        for (int t = 0; t < 3; t++) {
          FightResult fr = arenaFight(s, 200 + t, m, 1, 1, 90, false, true);
          trials++;
          if (fr.died) died++;
          if (fr.won) { hits += fr.hits; secs += fr.secs; n++; }
        }
      printf("  %s %.1f hits (%.0fs, won %d/%d, died %d)", m == art::Monster::Wolf ? "wolf" : m == art::Monster::Boar ? "boar" : "goblin", n ? hits / n : -1.0, n ? secs / n : 0.0, n, trials, died);
    }
    printf("   [bare knuckles: winnable, slower than a blade]\n");
    fflush(stdout);
  }
  // 3. hits to kill one same-level enemy (melee only), fight length, rolls the bot made
  {
    struct M { art::Monster m; const char* n; };
    const M ms[] = {{art::Monster::Wolf, "wolf"}, {art::Monster::Boar, "boar"}, {art::Monster::Goblin, "goblin"}, {art::Monster::Skeleton, "skeleton"},
                    {art::Monster::Bear, "bear"}, {art::Monster::Troll, "troll"},
                    // (M3c) the Wildlands wildlife
                    {art::Monster::Scorpion, "scorpion"}, {art::Monster::Hyena, "hyena"}, {art::Monster::Lurker, "lurker"},
                    {art::Monster::Yeti, "yeti"}, {art::Monster::Wisp, "wisp"}, {art::Monster::EmberHound, "emberhound"},
                    {art::Monster::Blightspawn, "blightspawn"}};
    for (int L : {1, 3, 5}) {
      printf("METRIC hits to kill at lvl %d:", L);
      for (const M& m : ms) {
        double hits = 0, secs = 0, rolls = 0; int n = 0, died = 0;
        for (uint64_t s = A; s <= B; s++)
          for (int t = 0; t < 3; t++) {
            FightResult fr = arenaFight(s, 100 + t, m.m, 1, L, 120, false);
            if (fr.died) died++;
            if (!fr.won) continue;
            hits += fr.hits; secs += fr.secs; rolls += fr.rolls; n++;
          }
        printf("  %s %.1f (%.0fs, %.1f rolls%s)", m.n, n ? hits / n : -1.0, n ? secs / n : 0.0, n ? rolls / n : 0.0, died ? (std::string(", ") + std::to_string(died) + " died").c_str() : "");
      }
      printf("   [target 3-4 small, 8-12 bear/troll/yeti/lurker, 4-6 the wild hunters]\n");
      fflush(stdout);
    }
  }
  return 0;
}
