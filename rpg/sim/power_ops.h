// M8b Treasure: the worn powers' internals (Game's combat is private; PowerOps is its friend, game.h). POWERS lane.
// Defined in rpg/sim/powers.cpp. The combat code (game.cpp damage / meleeHit / kill / updatePlayer / update,
// ai.cpp, the companions' party_*.cpp) calls the hooks; the tests (tools/tests/test_powers.cpp) and the scripts
// (rpg/view/script_powers.cpp) use the rest.
#pragma once
#include "rpg/sim/game.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/powers.h"

struct PowerOps {
  // ---- whose gear: the player's (Live::stats) or a companion in play's (compStats_). nullptr: none (a foe, a wolf)
  static const gear::GearStats* statsOf(const Game& g, const Actor& a);
  static const gear::GearStats* statsOfId(const Game& g, int actorId);
  // a companion's worn gear summed at their level (party::levelFor), for body() and the cache
  static gear::GearStats memberStats(const Game& g, const party::Member& m);
  // the base a power's damage scales from: the player's weapon damage, a companion's blow
  static float baseDamage(const Game& g, const Actor& wearer);

  // ---- hooks (game.cpp / ai.cpp)
  static void step(Game& g, float dt);                // after updatePlayer: trails, auras, regen, stars, wolves, the slow
  static void afterHit(Game& g, int attacker, int victimId, Vec2 at, float dealt, bool crit);   // damage(), depth 0
  static void onKill(Game& g, const Actor& victim, int killer);   // kill(): a foe died
  static bool phoenix(Game& g);                       // kill(player): true = reborn instead
  static void rollStarted(Game& g);                   // updatePlayer: a roll began
  static void dodged(Game& g);                        // damage(): a blow passed through a roll's iframes (perfect roll)
  static bool wolfStep(Game& g, Actor& a, float dt);  // updateActor: a spectral wolf's behaviour (true: handled)
  static bool timeSlowed(const Game& g) { return g.craft.live.fx.slowT > 0; }
  static float foeScale(const Game& g) { return timeSlowed(g) ? powers::CHRONOS_SCALE : 1.0f; }
  static bool unseen(const Game& g) { return g.craft.live.fx.unseenT > 0; }
  static void clearMap(Game& g);                      // clearNonPlayer: the map's patches, stars and calls go
  static void shift(Game& g, Vec2 d);                 // windowMoved: they move with the window
  static void emitPower(Game& g, powers::Fx fx, Vec2 p, Vec2 q, float f, uint32_t id);

  // ---- tests and scripts
  static void recalc(Game& g) { g.recalcPlayer(); }
  static void swing(Game& g) { g.meleeHit(g.pl()); }
  static void hurt(Game& g, Actor& v, float dmg, Vec2 from, int attacker, bool crit = false) { g.damage(v, dmg, from, attacker, Ench::None, 0, crit); }
  static void hurtE(Game& g, Actor& v, float dmg, Vec2 from, int attacker, Ench e, float ep) { g.damage(v, dmg, from, attacker, e, ep, false); }
  static void kill(Game& g, Actor& a, int killer) { g.kill(a, killer); }
  static void cast(Game& g) { g.castSpell(); }
  static void shoot(Game& g) { g.shootArrow(); }
  static void companionSwing(Game& g, Actor& a) { g.meleeHit(a); }
  // a hostile human (a bandit) at `at` (map pixels) of that level: its actor id
  static int spawnBandit(Game& g, Vec2 at, int level) {
    Spawn sp;
    sp.bandit = true; sp.npc = false; sp.role = Role::Bandit; sp.site = -1; sp.slot = 0;
    sp.x = (int)(at.x / TILE); sp.y = (int)(at.y / TILE);
    const int id = g.spawnHuman(sp, at);
    const int k = g.findActor(id);
    if (k < 0) return -1;
    g.applyLevel(g.actors[(size_t)k], level);
    return id;
  }
  static Rng& rng(Game& g) { return g.rng_; }
  static void companionBody(Game& g, int member);     // PartyOps::body again (after a test changes their gear)
  static int findActor(const Game& g, int id) { return g.findActor(id); }
  // a power fired at once where the player stands (scripts: `power proc <name>`; the nearest foe is the target)
  static bool proc(Game& g, Unique u);

  // ---- internals (powers.cpp)
  static void tempestChain(Game& g, int wid, Vec2 wp, int firstId, Vec2 from, float dmg);
  static void callWolf(Game& g, const powers::State::Call& c);
  static void aura(Game& g, const Actor& wearer, float dmg);
};
