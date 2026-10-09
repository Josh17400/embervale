// M6 Steel: the foes' Game hooks (game.h "M6 Steel hooks", rpg/sim/foes.h). FOES lane.
//   foeSpawned  a monster or bandit came into play: its rank (elite 6 % at D >= 5, champion packs 1 % at D >= 12),
//               affixes, variant look, name and stats. Never adds actors (a reference into Game::actors is live).
//   foeStep     once per step: the affix behaviours (regenerating, frenzied, summoner, blinking, burning trail, the
//               volatile's blast and the splitting's young after death), dungeon and world boss phases, champion packs
//               made whole, fire on the ground, named uniques at their lairs, world bosses on their routes, ruin wardens,
//               and the world bosses' raids on the realm (once a game day)
//   foeHit      a blow about to land: warded allies, a boss's shield, vampiric healing, frost-bound slows
//   foeKilled   a named unique or a world boss fell: marks (it stays dead), the realm's news, fame
//   foeTalk / foeChoose   innkeepers, guards, hunters and travellers warn of the named beasts near and the world boss
// Rules: foeSpawned and foeKilled never add to Game::actors; the step queues what it spawns and spawns it after its
// loop. Random choices come from hash streams of the world seed and stable ids / global tiles, never Game::rng_ (so the
// player's fights shift no other stream and rpg_test --metrics stays as in M5). Game::noWildSpawns (the arenas and the
// suites that need a quiet world) turns every roll and every named / world-boss spawn and raid off.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/game_internal.h"
#include "rpg/world/source.h"
#ifndef __EMSCRIPTEN__
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <set>
#include <system_error>
#include <thread>
#include <tuple>
#endif

using FAffix = foes::Affix;
using foes::Rank;
using foes::affixBit;

namespace {

uint64_t hmix(uint64_t a, uint64_t b) { return ew::mix64(a * 0x9E3779B97F4A7C15ull ^ ew::mix64(b + 0x7F4A7C15ull)); }
int floorDivI(int32_t a, int32_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
Mk mk(uint64_t tag) { return (Mk)tag; }
bool has(const Actor& a, FAffix f) { return (a.affixes & affixBit(f)) != 0; }
// a periodic event: the clock t crossed a multiple of `period` this step
bool crossed(float t, float dt, float period) { return dt > 0 && std::floor((t - dt) / period) != std::floor(t / period); }

// what spawns after the step's loop
enum class Req : uint8_t { Minion, Split, Mate };
struct SpawnReq {
  Req kind = Req::Minion;
  art::Monster mon = art::Monster::Wolf;
  Vec2 p, home;
  int level = 1;
  bool wild = false;
  int den = -1;
  int pack = 0;
  uint16_t affixes = 0;
  int eco = -1;
  bool aggro = true;
};

const uint32_t kBossCol = rgba(255, 120, 70);

#ifndef __EMSCRIPTEN__
// (M6 fixer round 2, review: "the foes step still freezes for 16-30 ms on a plain overland walk") a single cold
// region's named uniques or kingdom cell's world boss still cost 15-30 ms on desktop (far more on a phone): the cold
// ones are made OFF the main thread, like the chunk prefetch (rpg/sim/stream.cpp), by a worker with its own generator
// (one EndlessSource is not thread-safe; the results are a pure function of the seed and land in foes.cpp's memo, under
// its lock). foeStep asks, carries on, and uses a region or a cell once it is ready (namedReady / bossReady). Where no
// thread can be had (the web build) foeStep warms one cold slice per tick on the main thread as before.
struct FoeWarmer {
  using Key = std::tuple<uint64_t, bool, int32_t, int32_t>;
  std::mutex mu;
  std::condition_variable cv;
  std::thread th;
  bool stop = false, started = false, failed = false;
  std::deque<Key> q;       // guarded by mu
  std::set<Key> pending;   // guarded by mu: queued or being made
  std::unique_ptr<ew::EndlessSource> src;   // the worker's own
  void loop() {
    for (;;) {
      Key k;
      {
        std::unique_lock<std::mutex> lk(mu);
        cv.wait(lk, [&] { return stop || !q.empty(); });
        if (stop) return;
        k = q.front();
        q.pop_front();
      }
      const uint64_t seed = std::get<0>(k);
      if (!src || src->seed() != seed) src.reset(new ew::EndlessSource(seed));
      if (std::get<1>(k)) { bool ok = false; (void)foes::worldBossOf(*src, std::get<2>(k), std::get<3>(k), ok); }
      else (void)foes::namedInRegion(*src, std::get<2>(k), std::get<3>(k));
      std::lock_guard<std::mutex> lk(mu);
      pending.erase(k);
    }
  }
  // true: the worker has it in hand (ask again later); false: no worker here (warm it on this thread)
  bool request(uint64_t seed, bool boss, int32_t x, int32_t y) {
    std::lock_guard<std::mutex> lk(mu);
    if (failed) return false;
    if (!started) {
      try {
        th = std::thread([this] { loop(); });
        started = true;
      } catch (const std::system_error&) {
        failed = true;
        return false;
      }
    }
    const Key k{seed, boss, x, y};
    if (pending.count(k)) return true;
    if (q.size() >= 48) { pending.erase(q.front()); q.pop_front(); }   // the player moved on: the oldest wish goes
    pending.insert(k);
    q.push_back(k);
    cv.notify_one();
    return true;
  }
  ~FoeWarmer() {
    {
      std::lock_guard<std::mutex> lk(mu);
      stop = true;
      q.clear();
    }
    cv.notify_all();
    if (th.joinable()) th.join();
  }
};
FoeWarmer& warmer() {
  static FoeWarmer w;
  return w;
}
#endif
// ask the worker to make a cold region's named uniques (boss false) or a kingdom cell's world boss (boss true); false
// when there is no worker (the caller warms it itself)
bool warmOffThread(ew::EndlessSource& src, bool boss, int32_t x, int32_t y) {
#ifndef __EMSCRIPTEN__
  return warmer().request(src.seed(), boss, x, y);
#else
  (void)src; (void)boss; (void)x; (void)y;
  return false;
#endif
}

}  // namespace

// Game internals the foes need (game.h: friend struct FoeOps)
struct FoeOps {
  static bool suppress;   // a spawn the foes code makes itself: no rolls in foeSpawned

  // the land's eco where an actor stands (a dungeon: its entrance's), -1 unknown
  static int ecoAt(const Game& g, Vec2 p) {
    if (!g.inside) {
      const int tx = (int)std::floor(p.x / TILE), ty = (int)std::floor(p.y / TILE);
      return g.world.over.in(tx, ty) ? (int)g.world.over.ecoAt(tx, ty) : -1;
    }
    if (g.subSite >= 0 && g.subSite < (int)g.world.sites.size()) {
      const Site& s = g.world.sites[(size_t)g.subSite];
      if (g.world.over.in(s.ex, s.ey)) return (int)g.world.over.ecoAt(s.ex, s.ey);
    }
    return -1;
  }
  // a spawn's own hash: the world seed, where (global tile or the dungeon map and its tile), the species, the day
  static uint64_t spawnHash(const Game& g, const Actor& a) {
    const int tx = (int)std::floor(a.p.x / TILE), ty = (int)std::floor(a.p.y / TILE);
    uint64_t where;
    if (!g.inside) where = ((uint64_t)(uint32_t)(g.world.ox + tx) << 32) | (uint32_t)(g.world.oy + ty);
    else where = ((uint64_t)(uint32_t)g.mapKey() << 40) ^ ((uint64_t)(uint32_t)tx << 20) ^ (uint64_t)(uint32_t)ty ^ 0xD0D0ull;
    const uint64_t sub = (uint64_t)((int)a.p.x & 15) << 4 | (uint64_t)((int)a.p.y & 15);
    return hmix(hmix(g.seed ^ 0xF0E5ull, where), ((uint64_t)a.mon << 40) ^ ((uint64_t)(uint32_t)g.day << 8) ^ sub ^ (a.human ? 0x8000000000ull : 0));
  }
  // the pack a spawn belongs to: its den (dens lie 30+ tiles apart, so the one within 3 tiles is its own), else the
  // ground it stands on (6-tile cells outdoors, 8-tile rooms in a dungeon), on this day
  static uint64_t packKey(const Game& g, const Actor& a) {
    const int tx = (int)std::floor(a.p.x / TILE), ty = (int)std::floor(a.p.y / TILE);
    uint64_t k;
    if (!g.inside) {
      k = 0;
      for (int di : g.world.nearDens) {
        if (di < 0 || di >= (int)g.world.dens.size()) continue;
        const Den& d = g.world.dens[(size_t)di];
        if (std::abs(d.x - tx) <= 3 && std::abs(d.y - ty) <= 3) { k = d.id ? d.id : (uint64_t)di + 1; break; }
      }
      if (!k) k = ((uint64_t)(uint32_t)floorDivI(g.world.ox + tx, 6) << 32) ^ (uint32_t)floorDivI(g.world.oy + ty, 6) ^ 0xC311ull;
    } else {
      k = ((uint64_t)(uint32_t)g.mapKey() << 32) ^ ((uint64_t)(uint32_t)(tx / 8) << 16) ^ (uint64_t)(uint32_t)(ty / 8) ^ 0x9A3Bull;
    }
    return hmix(g.seed ^ 0xA11ull, k ^ ((uint64_t)a.mon << 56) ^ ((uint64_t)(uint32_t)g.day << 20));
  }
  static std::string baseName(const Actor& a) {
    if (a.human) return a.name;
    return monsterName(a.mon);
  }
  // give an actor its rank: the variant look, the multipliers, the affixes' standing effects, the name plate
  static void applyRank(Actor& a, Rank r, uint16_t aff, int D, int eco, uint32_t seed) {
    const std::string base = baseName(a);
    a.rank = (uint8_t)r;
    a.affixes = aff;
    a.dropD = std::max(1, D);
    const foes::Variant v = foes::variantFor(a.mon, eco, aff, r, seed);
    if (!a.human) {
      a.overlays |= v.overlays;
      if (v.tint) a.bodyTint = v.tint;
    }
    if (v.scalePct) a.scalePct = v.scalePct;
    a.maxHp *= foes::rankHpMul(r);
    a.hp = a.maxHp;
    a.dmg *= foes::rankDmgMul(r);
    a.xp = (int)std::lround(a.xp * foes::rankXpMul(r));
    if (aff & affixBit(FAffix::Swift)) a.speed *= 1.3f;
    if (aff & affixBit(FAffix::Armoured)) { a.armor = std::max(a.armor * 1.6f, a.armor + 4.0f); a.speed *= 0.8f; }
    if (r == Rank::Named || r == Rank::WorldBoss) a.aggroR *= 1.25f;
    a.name = foes::foeName(base, aff, a.human ? std::string() : v.prefix);
  }
  // a spawn of the foes' own (minions, split-offs, pack mates, named uniques, world bosses): no rolls
  static int spawnPlain(Game& g, art::Monster m, Vec2 p, int level) {
    if (!g.bodyFree(p, gsim::mstat(m).radius, gsim::mstat(m).flying))
      p = g.freeSpot((int)std::floor(p.x / TILE), (int)std::floor(p.y / TILE));
    suppress = true;
    const int id = g.spawnMonster(m, p, level, false);
    suppress = false;
    return id;
  }
  // can fire lie on this spot (updateProjectiles' wall rule: the patch must not die against a wall at once)
  static bool fireFits(const Game& g, Vec2 p) {
    const Map& m = g.map();
    const int tx = (int)std::floor(p.x / TILE), ty = (int)std::floor((p.y + 8) / TILE);
    if (!m.in(tx, ty) || m.blocked(tx, ty)) return false;
    const Ground gr = m.at(tx, ty);
    if (gr == Ground::Rock || gr == Ground::CaveWall || gr == Ground::InteriorWall || gr == Ground::Void || gr == Ground::Water) return false;
    if (m.bldgAt[(size_t)ty * m.w + tx] >= 0 || m.wall[(size_t)ty * m.w + tx]) return false;
    return m.propAt(tx, ty) == 0;
  }
  // a patch of fire on the ground (a burning elite's trail, a boss's arena hazard): an inert fireball (Faction::Player,
  // never fromPlayer: updateProjectiles' contact test finds no enemy for it) that foeStep burns the player with and
  // takes away before it would burst
  static void dropFire(Game& g, Vec2 at, float dmg, int owner, float life) {
    if (!fireFits(g, at + Vec2(0, -6))) return;
    Projectile pr;
    pr.kind = ProjKind::Fireball; pr.fromPlayer = false; pr.fac = Faction::Player; pr.owner = owner;
    pr.p = at + Vec2(0, -6); pr.v = Vec2(); pr.dmg = dmg; pr.life = life; pr.radius = 5;
    g.projs.push_back(pr);
  }
  static bool isFire(const Projectile& pr) { return pr.kind == ProjKind::Fireball && !pr.fromPlayer && pr.fac == Faction::Player; }
  // a rune that becomes fire when it fades (the telegraph of a boss's hazard phase)
  static bool isRune(const Projectile& pr) { return pr.kind == ProjKind::Magic && !pr.fromPlayer && pr.fac == Faction::Player && pr.v.x == 0 && pr.v.y == 0; }
  static void dropRune(Game& g, Vec2 at, float dmg, int owner) {
    if (!fireFits(g, at + Vec2(0, -6))) return;
    Projectile pr;
    pr.kind = ProjKind::Magic; pr.fromPlayer = false; pr.fac = Faction::Player; pr.owner = owner;
    pr.p = at + Vec2(0, -6); pr.v = Vec2(); pr.dmg = dmg; pr.life = 0.9f; pr.radius = 4;
    g.projs.push_back(pr);
  }
  static void playerGlobal(const Game& g, int32_t& gx, int32_t& gy) {
    int x = 0, y = 0;
    gsim::overworldTile(g, x, y);
    gx = g.world.ox + x; gy = g.world.oy + y;
  }
  static bool present(const Game& g, ew::Gid uid) {
    for (const Actor& a : g.actors) if (a.unique == uid) return true;
    return false;
  }
  static bool slainNamed(const Game& g, ew::Gid id) { return g.marks.count(gsim::markKey(id, mk(foes::MK_FOES_NAMED_SLAIN))) != 0; }
  static bool slainBoss(const Game& g, ew::Gid id) { return g.marks.count(gsim::markKey(id, mk(foes::MK_FOES_BOSS_SLAIN))) != 0; }

  static void spawnNamed(Game& g, const foes::NamedUnique& u) {
    const int lx = u.gx - g.world.ox, ly = u.gy - g.world.oy;
    if (!g.world.over.in(lx, ly)) return;
    const Vec2 at = g.freeSpot(lx, ly);
    const int id = spawnPlain(g, u.mon, at, u.D);
    const int i = g.findActor(id);
    if (i < 0) return;
    Actor& a = g.actors[(size_t)i];
    const std::string keepName = u.name;
    applyRank(a, Rank::Named, u.affixes, u.D, -1, (uint32_t)u.id);
    a.overlays = u.look.overlays;
    a.bodyTint = u.look.tint;
    a.scalePct = u.look.scalePct ? u.look.scalePct : 125;
    a.name = keepName;
    a.unique = u.id;
    a.wild = true;
    a.home = at; a.goal = at;
    // the lair's look: bones and a skull pile on the open ground round it (an overlay on the window's map: the next
    // visit lays them again)
    Map& m = g.world.over;
    for (int k = 0; k < 6; k++) {
      const uint64_t h = hmix(u.id, (uint64_t)k + 11);
      const int bx = lx + (int)(h % 7) - 3, by = ly + (int)((h >> 8) % 5) - 2;
      if (!m.in(bx, by) || (bx == lx && by == ly) || m.blocked(bx, by) || m.propAt(bx, by) || m.bldgAt[(size_t)by * m.w + bx] >= 0) continue;
      const Ground gr = m.at(bx, by);
      if (gr == Ground::Road || gr == Ground::Plaza || gr == Ground::Bridge || gr == Ground::Water || gr == Ground::DeepWater) continue;
      m.setProp(bx, by, k == 0 ? art::Prop::SkullPile : art::Prop::Bones);
    }
  }
  static void spawnWorldBoss(Game& g, const foes::WorldBoss& b, Vec2 at) {
    const int id = spawnPlain(g, b.mon, at, b.D);
    const int i = g.findActor(id);
    if (i < 0) return;
    Actor& a = g.actors[(size_t)i];
    if (b.mon == art::Monster::Dragon) {
      // the dragon's body is already a boss's (spawnMonster): a longer fight than Ashfang's, not a slog
      a.maxHp *= 2.5f; a.hp = a.maxHp; a.dmg *= 1.2f;
      a.rank = (uint8_t)Rank::WorldBoss; a.dropD = b.D; a.xp = (int)(a.xp * 1.5f);
    } else {
      applyRank(a, Rank::WorldBoss, 0, b.D, -1, (uint32_t)b.id);
      a.boss = true;
      a.radius += 2.0f;
    }
    a.overlays = b.look.overlays;
    a.bodyTint = b.look.tint;
    a.scalePct = b.look.scalePct;
    a.name = b.name;
    a.unique = b.id;
    a.wild = true;
    a.home = at; a.goal = at;
  }
};
bool FoeOps::suppress = false;

void Game::foeSpawned(Actor& a, int D) {
  if (!a.human && a.mon == art::Monster::Golem) a.armor = a.armor * 2.0f + 4.0f;   // stone: blades skid off it
  if (FoeOps::suppress || a.npc || a.player) return;
  if (a.boss) {
    if (!a.human && a.mon != art::Monster::Dragon) { a.rank = (uint8_t)Rank::Boss; a.dropD = std::max(1, D); }
    return;
  }
  if (noWildSpawns) return;   // the arenas and the quiet suites: every foe plain, as in M5
  const int eco = FoeOps::ecoAt(*this, a.p);
  const uint64_t h = FoeOps::spawnHash(*this, a);
  if (!a.human && D >= foes::CHAMPION_MIN_D) {
    uint16_t aff = 0;
    int pid = 0;
    const uint64_t pk = FoeOps::packKey(*this, a);
    if (foes::championPack(pk, D, a.mon, false, aff, pid)) {
      FoeOps::applyRank(a, Rank::Champion, aff, D, eco, (uint32_t)pk);
      a.pack = pid;
      a.affixT2 = -1.0f;   // the pack is made whole (3) by the next foeStep
      return;
    }
  }
  const foes::FoeRoll r = foes::rollFoe(h, D, a.mon, a.human);
  if (r.rank == Rank::Elite) FoeOps::applyRank(a, Rank::Elite, r.affixes, D, eco, (uint32_t)h);
}

float Game::foeHit(Actor& victim, float dmg, int attacker) {
  if (dmg <= 0) return dmg;
  // a boss behind its shield (the shield phase)
  if (!victim.player && victim.bossPhase > 0 && victim.affixT2 > 0 && (victim.boss || victim.rank == (uint8_t)Rank::WorldBoss)) {
    dmg *= 0.15f;
    emit(Ev::Sparkle, victim.p + Vec2(0, -10));
  }
  // warded: a ward-bearer within 5 tiles halves the blow on its allies (not on itself)
  if (!victim.player && victim.hostile) {
    for (const Actor& w : actors) {
      if (w.id == victim.id || w.st == AState::Dead || !(w.affixes & affixBit(FAffix::Warded)) || w.faction != victim.faction) continue;
      if (len2(w.p - victim.p) > (5.0f * TILE) * (5.0f * TILE)) continue;
      dmg *= 0.5f;
      emit(Ev::Sparkle, victim.p + Vec2(0, -8));
      break;
    }
  }
  if (attacker >= 0 && attacker != victim.id) {
    const int ai = findActor(attacker);
    if (ai > 0) {
      Actor& at = actors[(size_t)ai];
      if (at.st != AState::Dead && has(at, FAffix::Vampiric)) {
        const float heal = dmg * 0.3f;
        if (at.hp < at.maxHp) { at.hp = std::min(at.maxHp, at.hp + heal); emit(Ev::Heal, at.p); }
      }
      if (has(at, FAffix::Frostbound) && !(victim.player && godMode)) {
        victim.slowT = std::max(victim.slowT, 1.6f);
        emit(Ev::Frost, victim.p);
      }
    }
  }
  return dmg;
}

void Game::foeKilled(const Actor& a, int killer) {
  if (!a.unique || a.player) return;
  const bool byPlayer = killer == pl().id;
  int32_t gx = 0, gy = 0;
  if (!inside) { gx = world.ox + (int)std::floor(a.p.x / TILE); gy = world.oy + (int)std::floor(a.p.y / TILE); }
  else FoeOps::playerGlobal(*this, gx, gy);
  if (a.rank == (uint8_t)Rank::Named) {
    marks[markKey(a.unique, mk(foes::MK_FOES_NAMED_SLAIN))] = std::max(1, day);
    realm.beastSlain((uint8_t)a.mon, gx, gy, day, (uint8_t)Rank::Named, byPlayer);
    if (byPlayer) {
      realm.renown().fame += 10;
      emit(Ev::Notice, a.p, (int)rgba(255, 210, 90), 0, a.name + " IS SLAIN  +10 FAME");
      sfx((int)Sfx::QuestDone, a.p, 0.9f);
    }
  } else if (a.rank == (uint8_t)Rank::WorldBoss) {
    marks[markKey(a.unique, mk(foes::MK_FOES_BOSS_SLAIN))] = std::max(1, day);
    realm.beastSlain((uint8_t)a.mon, gx, gy, day, (uint8_t)Rank::WorldBoss, byPlayer);
    if (byPlayer) {
      realm.renown().fame += 40;
      emit(Ev::Notice, a.p, (int)rgba(255, 190, 60), 0, a.name + " HAS FALLEN!  +40 FAME");
      emit(Ev::Shake, a.p, 0, 6.0f);
      sfx((int)Sfx::Roar, a.p, 0.6f);
    }
  }
}

void Game::foeStep(float dt) {
  if (dt <= 0) return;
  std::vector<SpawnReq> reqs;
  Actor& P0 = pl();
  const Vec2 pp = P0.p;
  const bool plAlive = P0.st != AState::Dead;
  // ---- the actors
  const size_t n0 = actors.size();
  for (size_t i = 1; i < n0; i++) {
    Actor& a = actors[i];
    if (a.npc) continue;
    // ranks staged by a script (`elite`): the multipliers and the look the spawn would have given
    if (a.rank && !a.dropD && a.st != AState::Dead && a.rank != (uint8_t)Rank::Boss) {
      const uint16_t aff = a.affixes;
      const uint16_t ov = a.overlays;
      const uint32_t tint = a.bodyTint;
      // (M6 fixer) a name the script gave a named unique or a world boss stands (it is what the plate is checked for)
      const std::string given = a.name != monsterName(a.mon) && (a.rank == (uint8_t)Rank::Named || a.rank == (uint8_t)Rank::WorldBoss) ? a.name : std::string();
      a.affixes = 0;
      FoeOps::applyRank(a, (Rank)a.rank, aff, std::max(1, a.level), FoeOps::ecoAt(*this, a.p), (uint32_t)a.id * 2654435761u);
      a.overlays |= ov;
      if (tint) a.bodyTint = tint;
      if (!given.empty()) a.name = given;
    }
    if (a.st == AState::Dead) {
      // ---- after death: the splitting one's young, the volatile one's blast (1 s, telegraphed)
      if (has(a, FAffix::Splitting)) {
        a.affixes &= (uint16_t)~affixBit(FAffix::Splitting);
        for (int k = 0; k < 2; k++) {
          SpawnReq r;
          r.kind = Req::Split; r.mon = a.mon; r.p = a.p + Vec2(k ? 9.0f : -9.0f, (float)(k * 2 - 1) * 3.0f);
          r.home = a.home; r.level = std::max(1, a.level - 1); r.wild = a.wild; r.den = a.den;
          reqs.push_back(r);
        }
        emit(Ev::Dust, a.p);
        emit(Ev::Text, a.p + Vec2(0, -20), (int)foes::affixInfo(FAffix::Splitting).aura, 0, "SPLITS!");
      }
      if (has(a, FAffix::Volatile)) {
        if (a.stT < 1.0f) {
          if (a.stT <= dt + 1e-4f) emit(Ev::Text, a.p + Vec2(0, -22), (int)rgba(255, 200, 60), 0, "!!");
          if (crossed(a.stT, dt, 0.2f)) { emit(Ev::Sparkle, a.p + Vec2(0, -6)); sfx((int)Sfx::Fireball, a.p, 2.0f, 0.35f); }
        } else {
          a.affixes &= (uint16_t)~affixBit(FAffix::Volatile);
          emit(Ev::Explode, a.p, 0, 1.3f);
          emit(Ev::Shake, a.p, 0, 3.5f);
          sfx((int)Sfx::Explode, a.p, 0.9f);
          const float r = 36.0f;
          for (size_t k = 0; k < actors.size(); k++) {
            Actor& v = actors[k];
            if (v.st == AState::Dead || v.id == a.id || !(v.player || v.npc) || !factionsHostile(a.faction, v.faction)) continue;
            if (len2(v.p - a.p) > r * r) continue;
            damage(v, a.dmg * 1.6f, a.p, -1);
          }
        }
      }
      continue;
    }
    if (!a.hostile) continue;
    // golems shrug off fire and frost sooner (stone does not burn; ice cracks off it)
    if (a.mon == art::Monster::Golem && !a.human) {
      if (a.burnT > 0) a.burnT -= dt * 0.6f;
      if (a.slowT > 0) a.slowT -= dt * 0.6f;
    }
    // ---- boss phases (7.6): at 66 % and 33 % health a behaviour from {summon, enrage, hazard, shield}
    const bool bossy = !a.human && ((a.boss && a.mon != art::Monster::Dragon) || a.rank == (uint8_t)Rank::WorldBoss);
    if (bossy) {
      if (a.affixT2 > 0) a.affixT2 = std::max(0.0f, a.affixT2 - dt);
      const float frac = a.maxHp > 0 ? a.hp / a.maxHp : 1.0f;
      int phaseIdx = 0;
      if (a.bossPhase < 1 && frac <= 0.66f) phaseIdx = 1;
      else if (a.bossPhase < 2 && frac <= 0.33f) phaseIdx = 2;
      if (phaseIdx) {
        a.bossPhase = (uint8_t)phaseIdx;
        const uint32_t seed = (uint32_t)(a.unique ? a.unique : (uint64_t)(a.site + 1) * 7919u + (uint64_t)a.mon);
        const foes::Phase ph = foes::bossPhaseFor(a.mon, phaseIdx, seed);
        std::string what;
        switch (ph) {
          case foes::Phase::SummonAdds: {
            what = "CALLS FOR AID!";
            const int n = phaseIdx == 1 ? 2 : 3;
            for (int k = 0; k < n; k++) {
              SpawnReq r;
              r.kind = Req::Minion; r.mon = foes::minionOf(a.mon);
              const float an = (float)k * (TAU / (float)n) + 0.6f;
              r.p = a.p + Vec2(std::cos(an) * 26.0f, std::sin(an) * 18.0f);
              r.home = a.home; r.level = std::max(1, a.level - 1); r.wild = a.wild;
              reqs.push_back(r);
            }
            break;
          }
          case foes::Phase::Enrage:
            what = "IS ENRAGED!";
            a.speed *= 1.2f;
            a.atkCd = 0;
            a.overlays |= art::MO_EYES;
            break;
          case foes::Phase::Hazard: {
            what = "SETS THE GROUND ALIGHT!";
            for (int k = 0; k < 7; k++) {
              const float an = (float)k * (TAU / 7.0f) + (float)phaseIdx;
              const float rr = k == 0 ? 0.0f : 30.0f + (float)(k % 3) * 12.0f;
              FoeOps::dropRune(*this, pp + Vec2(std::cos(an) * rr, std::sin(an) * rr * 0.7f), a.dmg * 0.35f, a.id);
            }
            break;
          }
          case foes::Phase::Shield:
            what = "RAISES A WARD!";
            a.affixT2 = 5.0f;
            sfx((int)Sfx::Block, a.p, 0.6f);
            break;
          default: break;
        }
        emit(Ev::Text, a.p + Vec2(0, -(34.0f + a.radius * 1.5f)), (int)kBossCol, 0, what);   // (fixer r4) over its head, not its body
        emit(Ev::Shake, a.p, 0, 4.0f);
        sfx((int)Sfx::Roar, a.p, 0.8f + 0.1f * (float)phaseIdx);
        if (len2(a.p - pp) < (22.0f * TILE) * (22.0f * TILE)) emit(Ev::Notice, pp, (int)kBossCol, 0, a.name + " " + what);
      }
    }
    // ---- a champion pack made whole: three that share their affixes (the rest of a den's pack stays with them)
    if (a.pack > 0 && a.affixT2 < 0) {
      int members = 0;
      for (size_t k = 1; k < n0; k++) {
        Actor& o = actors[k];
        if (o.pack != a.pack || o.st == AState::Dead) continue;
        members++;
        o.affixT2 = 0;
      }
      for (int k = members; k < 3; k++) {
        SpawnReq r;
        r.kind = Req::Mate; r.mon = a.mon; r.p = a.p + Vec2((float)(k - 1) * 14.0f, (float)(k % 2) * 10.0f - 5.0f);
        r.home = a.home; r.level = a.level; r.wild = a.wild; r.den = a.den; r.pack = a.pack; r.affixes = a.affixes;
        r.eco = FoeOps::ecoAt(*this, a.p); r.aggro = a.aggro;
        reqs.push_back(r);
      }
    }
    if (!a.affixes) continue;
    // ---- the affixes (7.6 table)
    if (a.aggro) a.affixT += dt;
    if (has(a, FAffix::Regenerating) && a.burnT <= 0 && a.hp < a.maxHp) a.hp = std::min(a.maxHp, a.hp + a.maxHp * 0.02f * dt);
    if (has(a, FAffix::Frenzied) && a.hp < a.maxHp * 0.5f && a.atkCd > 0) a.atkCd = std::max(0.0f, a.atkCd - dt * 0.4f);
    if (!a.aggro || !plAlive) continue;
    const float t = a.affixT;
    const float pd2 = len2(pp - a.p);
    if (has(a, FAffix::Summoner)) {
      if (std::fmod(t, 12.0f) > 11.2f && crossed(t, dt, 0.2f)) emit(Ev::Sparkle, a.p + Vec2(0, -12));
      if (crossed(t, dt, 12.0f)) {
        int near = 0;
        for (size_t k = 1; k < n0; k++) {
          const Actor& o = actors[k];
          if (o.st != AState::Dead && o.mon == foes::minionOf(a.mon) && o.id != a.id && !o.rank && len2(o.p - a.p) < (8.0f * TILE) * (8.0f * TILE)) near++;
        }
        if (near < 4) {
          for (int k = 0; k < 2; k++) {
            SpawnReq r;
            r.kind = Req::Minion; r.mon = foes::minionOf(a.mon);
            r.p = a.p + Vec2(k ? 18.0f : -18.0f, 6.0f);
            r.home = a.home; r.level = std::max(1, a.level - 2); r.wild = a.wild; r.den = -1;
            reqs.push_back(r);
          }
          emit(Ev::Text, a.p + Vec2(0, -24), (int)foes::affixInfo(FAffix::Summoner).aura, 0, "SUMMONS!");
          sfx((int)Sfx::Frost, a.p, 0.6f);
        }
      }
    }
    if (has(a, FAffix::Blinking) && pd2 < 170.0f * 170.0f) {
      // behind the player (where they are not looking); a wall there: a quarter or a half turn round them
      const Vec2 back = norm(P0.aim.x == 0 && P0.aim.y == 0 ? Vec2(0, 1) : P0.aim) * -1.0f;
      Vec2 behind = pp + back * 22.0f;
      const Vec2 alts[] = {back, norm(back + Vec2(-back.y, back.x)), norm(back - Vec2(-back.y, back.x)), Vec2(-back.y, back.x), Vec2(back.y, -back.x)};
      for (const Vec2& d : alts)
        if (bodyFree(pp + d * 22.0f, a.radius, a.flying)) { behind = pp + d * 22.0f; break; }
      const float ph = std::fmod(t, 6.0f);
      if (ph > 5.3f && crossed(t, dt, 0.15f)) emit(Ev::Sparkle, behind + Vec2(0, -6));
      if (crossed(t, dt, 6.0f) && bodyFree(behind, a.radius, a.flying)) {
        emit(Ev::Sparkle, a.p + Vec2(0, -8));
        a.p = behind;
        a.knock = Vec2();
        a.aim = norm(pp - a.p);
        a.face = faceOf(a.aim);
        a.atkCd = std::min(a.atkCd, 0.25f);
        emit(Ev::Sparkle, a.p + Vec2(0, -8));
        sfx((int)Sfx::Frost, a.p, 1.6f, 0.6f);
      }
    }
    if (has(a, FAffix::Burning) && a.st == AState::Walk && crossed(t, dt, 0.35f))
      FoeOps::dropFire(*this, a.p, std::max(2.0f, a.dmg * 0.25f), a.id, 2.6f);
  }
  // ---- fire on the ground: it burns the player who stands in it; it fades out quietly (never bursts)
  for (size_t k = 0; k < projs.size();) {
    Projectile& pr = projs[k];
    if (FoeOps::isRune(pr)) {
      if (pr.life <= dt * 1.5f) {   // the rune flares into fire
        pr.kind = ProjKind::Fireball; pr.life = 3.2f; pr.radius = 6;
        emit(Ev::Explode, pr.p, 0, 0.5f);
      }
      k++;
      continue;
    }
    if (!FoeOps::isFire(pr)) { k++; continue; }
    if (pr.life <= dt * 1.5f) { projs.erase(projs.begin() + (std::ptrdiff_t)k); continue; }
    if (plAlive && len2(pl().p + Vec2(0, -6) - pr.p) < (pr.radius + 5.0f) * (pr.radius + 5.0f) && pl().iframes <= 0)
      damage(pl(), pr.dmg, pr.p, -1, Ench::Fire, 1.0f);
    k++;
  }
  // ---- what the step queued
  for (const SpawnReq& r : reqs) {
    const int id = FoeOps::spawnPlain(*this, r.mon, r.p, r.level);
    const int i = findActor(id);
    if (i < 0) continue;
    Actor& m = actors[(size_t)i];
    m.home = r.home; m.goal = m.p; m.wild = r.wild; m.den = r.den; m.aggro = r.aggro;
    // (M6 fixer) a split-off never joins its den (the den may just have been called cleared by the parent's death,
    // and its young would re-award it), and a champion's mate joins only a den that still has someone alive in it
    if (r.kind == Req::Split) m.den = -1;
    else if (m.den >= 0) {
      bool alive = false;
      for (const Actor& o : actors) if (o.id != m.id && o.den == m.den && o.st != AState::Dead && !o.npc && !o.player) { alive = true; break; }
      if (!alive) m.den = -1;
    }
    switch (r.kind) {
      case Req::Minion:
        m.maxHp *= 0.6f; m.hp = m.maxHp; m.xp = std::max(1, m.xp / 3); m.scalePct = 85; m.den = -1;
        emit(Ev::Sparkle, m.p + Vec2(0, -6));
        break;
      case Req::Split:
        m.maxHp *= 0.4f; m.hp = m.maxHp; m.dmg *= 0.6f; m.xp = std::max(1, m.xp / 3); m.scalePct = 70; m.radius *= 0.8f;
        m.name = "LESSER " + m.name;
        break;
      case Req::Mate:
        FoeOps::applyRank(m, Rank::Champion, r.affixes, r.level, r.eco, (uint32_t)r.pack);
        m.pack = r.pack; m.affixT2 = 0;
        break;
    }
  }
  // ---- the land's great beasts (twice a second): named uniques at their lairs, world bosses on their routes, ruin
  //      wardens, and the world bosses' raids (once a game day)
  if (!crossed(time, dt, 0.5f) || noWildSpawns || !world.endless || !world.src || travelling()) return;
  ew::EndlessSource& src = *world.src;
  foeWarmedTick_ = false;
  int32_t pgx = 0, pgy = 0;
  FoeOps::playerGlobal(*this, pgx, pgy);
  if (!inside && plAlive) {
    const int32_t rx = floorDivI(pgx, ew::REGION), ry = floorDivI(pgy, ew::REGION);
    // (M6 integration) a cold region or kingdom cell costs a few ms to generate: warm one of each per tick, the
    // player's own first, and leave the cold ones for the next ticks (nine at once hitched the frame by 40-80 ms)
    // (M6 fixer r3, review: "foes step still costs up to 18 ms on a plain overland walk") ONE cold region or kingdom
    // cell per tick in all (named uniques, world bosses and the raids' warm-up share the budget), never one of each
    bool& coldNamed = foeWarmedTick_;
    bool& coldBoss = foeWarmedTick_;
    auto namedWarm = [&](int32_t x, int32_t y) {
      if (foes::namedReady(src, x, y)) return true;
      if (warmOffThread(src, false, x, y)) return false;
      if (coldNamed) return false;
      coldNamed = true;
      (void)foes::namedInRegion(src, x, y);
      return true;
    };
    auto bossWarm = [&](int32_t x, int32_t y) {
      if (foes::bossReady(src, x, y)) return true;
      if (warmOffThread(src, true, x, y)) return false;
      if (coldBoss) return false;
      coldBoss = true;
      return foes::bossWarmStep(src, x, y);   // (fixer r4) the lair one tick, the rest the next
    };
    (void)namedWarm(rx, ry);
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++) {
        if (!namedWarm(rx + dx, ry + dy)) continue;
        for (const foes::NamedUnique& u : foes::namedInRegion(src, rx + dx, ry + dy)) {
          const int64_t ddx = u.gx - pgx, ddy = u.gy - pgy;
          if (ddx * ddx + ddy * ddy > 26ll * 26ll) continue;
          if (FoeOps::slainNamed(*this, u.id) || FoeOps::present(*this, u.id)) continue;
          FoeOps::spawnNamed(*this, u);
        }
      }
    // (M6 fixer r2) the outer ring the innkeepers' rumours read ("ANY BEASTS ABOUT?", foeChoose), one cold one per tick
    // when the 3x3 is warm, so the talk never generates regions synchronously
    for (int dy = -2; dy <= 2; dy++)
      for (int dx = -2; dx <= 2; dx++)
        if (std::abs(dx) == 2 || std::abs(dy) == 2) (void)namedWarm(rx + dx, ry + dy);
    const int32_t kx = ew::EndlessSource::kcellOf(pgx), ky = ew::EndlessSource::kcellOf(pgy);
    (void)bossWarm(kx, ky);
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++) {
        if (!bossWarm(kx + dx, ky + dy)) continue;
        bool ok = false;
        const foes::WorldBoss b = foes::worldBossOf(src, kx + dx, ky + dy, ok);
        if (!ok || FoeOps::slainBoss(*this, b.id) || FoeOps::present(*this, b.id)) continue;
        int32_t bx = 0, by = 0;
        foes::worldBossAt(b, day, hour, bx, by);
        const int64_t ddx = bx - pgx, ddy = by - pgy;
        if (ddx * ddx + ddy * ddy > 30ll * 30ll) continue;
        // never in the start's own country (a passing route may cut a corner of it: it keeps to the far side then)
        const ew::GTile sp = src.start().spawn;
        if ((int64_t)(bx - sp.x) * (bx - sp.x) + (int64_t)(by - sp.y) * (by - sp.y) < 220ll * 220ll) continue;
        const int lx = bx - world.ox, ly = by - world.oy;
        if (!world.over.in(lx, ly)) continue;
        // (M6 fixer r2) outside a raid a roaming world boss keeps clear of the settlements (its route between two haunts
        // may pass by a town, and a neighbouring cell's town was never checked): it comes into play only in the open
        // country, 40 tiles or more from any settlement, and on dry walkable ground with room for its body
        {
          const std::vector<ew::SettlementNode> near = src.settlementsIn(bx - 48, by - 48, bx + 48, by + 48);
          bool townNear = false;
          for (const ew::SettlementNode& nd : near)
            if ((int64_t)(nd.x - bx) * (nd.x - bx) + (int64_t)(nd.y - by) * (nd.y - by) < 40ll * 40ll) { townNear = true; break; }
          if (townNear || world.over.blocked(lx, ly)) continue;
          const Ground gr = world.over.at(lx, ly);
          if (gr == Ground::Road || gr == Ground::Plaza || gr == Ground::Bridge) continue;
        }
        FoeOps::spawnWorldBoss(*this, b, freeSpotClear(lx, ly, b.mon == art::Monster::Dragon ? 2 : 1));
        emit(Ev::Notice, pp, (int)kBossCol, 0, "THE GROUND SHAKES. " + b.name + " IS NEAR");
        emit(Ev::Shake, pp, 0, 3.0f);
      }
  }
  // a ruin's stone warden (one ruin in three keeps one; it stays dead: killedSlots under its own slot)
  if (inside && subSite >= 0 && subSite < (int)world.sites.size() && subBldg < 0) {
    const Site& s = world.sites[(size_t)subSite];
    constexpr int WARDEN_SLOT = 9001;
    if (s.type == SiteType::Ruin && hmix(s.id ^ seed, 0x3A2Dull) % 3 == 0) {
      bool here = false;
      for (const Actor& a : actors) if (a.slot == WARDEN_SLOT && a.site == subSite) { here = true; break; }
      auto it = killedSlots.find(mapKey());
      const bool dead = it != killedSlots.end() && it->second.count(WARDEN_SLOT);
      if (!here && !dead) {
        // a free floor tile 9-16 tiles from the player (the deep hall), the first in a fixed scan order
        const Map& m = map();
        const int ptx = (int)std::floor(pp.x / TILE), pty = (int)std::floor(pp.y / TILE);
        int bx = -1, by = -1, bestD = 1 << 30;
        for (int y = 1; y < m.h - 1; y++)
          for (int x = 1; x < m.w - 1; x++) {
            if (m.blocked(x, y)) continue;
            const int d = (x - ptx) * (x - ptx) + (y - pty) * (y - pty);
            if (d < 9 * 9 || d > 16 * 16) continue;
            const int score = std::abs(d - 13 * 13);
            if (score < bestD) { bestD = score; bx = x; by = y; }
          }
        if (bx >= 0) {
          const int id = FoeOps::spawnPlain(*this, art::Monster::Golem, Vec2(bx * TILE + 8.0f, by * TILE + 10.0f), std::max(1, s.level + 1));
          const int i = findActor(id);
          if (i >= 0) {
            Actor& g = actors[(size_t)i];
            g.fromMap = true; g.slot = WARDEN_SLOT; g.site = subSite;
            g.name = "RUIN WARDEN";
            g.maxHp *= 1.3f; g.hp = g.maxHp; g.xp = (int)(g.xp * 1.3f);
            g.overlays |= art::MO_MOSS | art::MO_EYES;
            g.aggroR = 70;
          }
        }
      }
    }
  }
  // ---- the world bosses' raids (VISION_PLAN 7.6 "Ashfang burned Kettlebrook"): once a game day, each boss of the
  //      kingdom cells round the player may fall on a settlement in its territory (1 day in 15). The realm hears of it
  //      (BeastRaid: damage, news naming the beast); a settlement the player is near sees it happen (the M5 raid:
  //      the bell, the guards, the outcome)
  const uint64_t dayKey = markKey(0, mk(foes::MK_FOES_RAIDDAY));
  auto dit = marks.find(dayKey);
  const int last = dit == marks.end() ? 0 : dit->second;
  if (last == day) return;
  {
    // (M6 integration) the twenty-five kingdom cells round the player warmed one per tick before the day is counted
    const int32_t wkx = ew::EndlessSource::kcellOf(pgx), wky = ew::EndlessSource::kcellOf(pgy);
    for (int dy = -2; dy <= 2; dy++)
      for (int dx = -2; dx <= 2; dx++)
        if (!foes::bossReady(src, wkx + dx, wky + dy)) {
          if (warmOffThread(src, true, wkx + dx, wky + dy)) return;   // (fixer round 2) the worker makes it
          if (foeWarmedTick_) return;   // (M6 fixer r3) this tick already generated one: the next tick warms this
          (void)foes::bossWarmStep(src, wkx + dx, wky + dy);   // (fixer r4) one slice a tick
          return;
        }
  }
  marks[dayKey] = day;
  if (last == 0 || day <= last) return;   // the first day seen only starts the count
  const int32_t kx = ew::EndlessSource::kcellOf(pgx), ky = ew::EndlessSource::kcellOf(pgy);
  for (int dy = -2; dy <= 2; dy++)
    for (int dx = -2; dx <= 2; dx++) {
      bool ok = false;
      const foes::WorldBoss b = foes::worldBossOf(src, kx + dx, ky + dy, ok);
      if (!ok || FoeOps::slainBoss(*this, b.id)) continue;
      const uint64_t h = hmix(b.id ^ seed, (uint64_t)(uint32_t)day);
      if (h % 15u != 0) continue;
      int32_t bx = 0, by = 0;
      foes::worldBossAt(b, day, 12.0f, bx, by);
      const int32_t R = b.range * 3 / 4;
      const ew::SettlementNode* tgt = nullptr;
      int64_t bd = -1;
      const std::vector<ew::SettlementNode> nodes = src.settlementsIn(bx - R, by - R, bx + R, by + R);
      for (const ew::SettlementNode& nd : nodes) {
        if (nd.flags & ew::SPF_START) continue;
        const int64_t d = (int64_t)(nd.x - bx) * (nd.x - bx) + (int64_t)(nd.y - by) * (nd.y - by);
        if (d > (int64_t)R * R || (bd >= 0 && d >= bd)) continue;
        bd = d; tgt = &nd;
      }
      if (!tgt) continue;
      if (!realm.settlement(tgt->id)) realm.noteSite(tgt->id, tgt->kingdom, (uint8_t)tgt->type, tgt->x, tgt->y);
      const int dmg = 10 + (int)((h >> 8) % 16u);
      const int si = world.siteHandle(tgt->id);
      const int64_t pdx = tgt->x - pgx, pdy = tgt->y - pgy;
      const bool live = si >= 0 && !inside && plAlive && !life.raid.site && pdx * pdx + pdy * pdy <= 110ll * 110ll &&
                        !FoeOps::present(*this, b.id);
      if (!live) { realm.beastRaid(tgt->id, (uint8_t)b.mon, dmg, day); continue; }
      // the raid comes in front of the player: the beast and a few of its kind at the edge of town, on its side
      const Site& s = world.sites[(size_t)si];
      Vec2 dir((float)(bx - tgt->x), (float)(by - tgt->y));
      dir = len2(dir) > 1.0f ? norm(dir) : Vec2(0, 1);
      const int rad = std::max(s.r.w, s.r.h) / 2 + 10;
      const int ex = s.ex + (int)std::lround(dir.x * (float)rad), ey = s.ey + (int)std::lround(dir.y * (float)rad);
      if (!world.over.in(ex, ey)) { realm.beastRaid(tgt->id, (uint8_t)b.mon, dmg, day); continue; }
      FoeOps::spawnWorldBoss(*this, b, freeSpotClear(ex, ey, 1));
      const Vec2 heart(s.ex * TILE + 8.0f, s.ey * TILE + 8.0f);
      life::Life::RaidLive R0;
      R0.site = s.id; R0.si = si; R0.night = day;
      for (size_t k = 1; k < actors.size(); k++)
        if (actors[k].unique == b.id) {
          Actor& a = actors[k];
          a.aggro = true; a.home = heart; a.goal = heart; a.wild = false;
          R0.ids.push_back(a.id);
        }
      const art::Monster mm = foes::minionOf(b.mon);
      for (int k = 0; k < 3; k++) {
        const int id = FoeOps::spawnPlain(*this, mm, Vec2((ex + k - 1) * TILE + 8.0f, (ey + 1) * TILE + 10.0f), std::max(1, b.D - 3));
        const int i = findActor(id);
        if (i < 0) continue;
        Actor& a = actors[(size_t)i];
        a.aggro = true; a.home = heart; a.goal = heart;
        R0.ids.push_back(id);
      }
      life.raid = R0;
      realm.beastRaid(tgt->id, (uint8_t)b.mon, 0, day);   // the news; the damage is the defence's to decide
      emit(Ev::Notice, pp, (int)kBossCol, 0, b.name + " FALLS ON " + s.name + "!");
      emit(Ev::Shake, pp, 0, 4.0f);
      sfx((int)Sfx::Roar, pp, 0.7f);
    }
}

void Game::foeWarm() {
  if (!world.endless || !world.src) return;
  ew::EndlessSource& src = *world.src;
  int32_t pgx = 0, pgy = 0;
  FoeOps::playerGlobal(*this, pgx, pgy);
  const int32_t rx = floorDivI(pgx, ew::REGION), ry = floorDivI(pgy, ew::REGION);
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) (void)foes::namedInRegion(src, rx + dx, ry + dy);
  const int32_t kx = ew::EndlessSource::kcellOf(pgx), ky = ew::EndlessSource::kcellOf(pgy);
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) { bool ok = false; (void)foes::worldBossOf(src, kx + dx, ky + dy, ok); }
}

void Game::foeTalk(Actor& a) {
  if (!a.npc || !a.human || !world.endless || !world.src) return;
  if (a.role != Role::Innkeeper && a.role != Role::Guard && a.role != Role::Hunter && a.role != Role::Traveller) return;
  dlg.opts.push_back({"ANY BEASTS ABOUT?", DLG_FOES + 1, 0});
}

bool Game::foeChoose(const DlgOpt& o) {
  if (o.action != DLG_FOES + 1 || !world.src) return false;
  ew::EndlessSource& src = *world.src;
  int32_t pgx = 0, pgy = 0;
  FoeOps::playerGlobal(*this, pgx, pgy);
  // the nearest named beast still alive within two regions, and the world boss of this kingdom cell
  const foes::NamedUnique* best = nullptr;
  std::vector<foes::NamedUnique> found;
  const int32_t rx = floorDivI(pgx, ew::REGION), ry = floorDivI(pgy, ew::REGION);
  // (M6 fixer r2) only the regions already generated (foeStep warms the 5x5 one per tick): asking never stalls a frame
  for (int dy = -2; dy <= 2; dy++)
    for (int dx = -2; dx <= 2; dx++) {
      if (!foes::namedReady(src, rx + dx, ry + dy)) continue;
      for (const foes::NamedUnique& u : foes::namedInRegion(src, rx + dx, ry + dy))
        if (!FoeOps::slainNamed(*this, u.id)) found.push_back(u);
    }
  int64_t bd = -1;
  for (const foes::NamedUnique& u : found) {
    const int64_t d = (int64_t)(u.gx - pgx) * (u.gx - pgx) + (int64_t)(u.gy - pgy) * (u.gy - pgy);
    if (bd < 0 || d < bd) { bd = d; best = &u; }
  }
  std::string t;
  if (best) {
    const int dist = (int)std::sqrt((double)bd);
    const char* how = dist < 60 ? "CLOSE BY, NOT AN HOUR'S WALK" : dist < 160 ? "A FEW HOURS' WALK" : dist < 320 ? "A DAY'S WALK OR SO" : "FAR OFF";
    t = best->rumour + " ITS LAIR LIES " + how + " TO THE " + dirWord(best->gx - pgx, best->gy - pgy, false) + ".";
    marks[markKey(best->id, mk(foes::MK_FOES_RUMOUR))] = 1;
  }
  bool ok = false;
  const foes::WorldBoss b = foes::worldBossOf(src, ew::EndlessSource::kcellOf(pgx), ew::EndlessSource::kcellOf(pgy), ok);
  if (ok && !FoeOps::slainBoss(*this, b.id)) {
    int32_t bx = 0, by = 0;
    foes::worldBossAt(b, day, hour, bx, by);
    const std::string boss = " AND PRAY " + b.name + " KEEPS TO THE " + dirWord(bx - pgx, by - pgy, false) + ". IT IS A " + b.kind + ", BIGGER THAN A BARN.";
    t += t.empty() ? boss.substr(5) : boss;
  }
  if (t.empty()) t = "NOTHING WORSE THAN WOLVES ROUND HERE, THE GODS BE THANKED.";
  dlg.text = t;
  for (size_t k = 0; k < dlg.opts.size(); k++)
    if (dlg.opts[k].action == o.action) { dlg.opts.erase(dlg.opts.begin() + (std::ptrdiff_t)k); break; }
  return true;
}
