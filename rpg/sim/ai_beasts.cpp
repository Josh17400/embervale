// EMBERVALE hostile behaviour sets (SOLID 3.5, split out of ai.cpp's updateAI): updateAI (ai.cpp) does the overrides,
// the target and the aggro check, then hands over here: beastDragon (its own dance) or beastStep, which plays the
// committed attack (the windup with each kind's blow, the strike, the recover), then closes in by kind (beastPursue) and
// picks the next blow by the same if-ladder as before (beastAttack: the pack's turn-taking, the brute's slam, the
// scorpion's sting, the lurker's rush, the blightspawn's burst, else a plain blow), or walks home (beastWander). The dice
// are drawn in the same order.
// Neighbours: ai.cpp (updateAI, folk, town defence, heavySlam), game_combat.cpp (meleeHit, damage).
#include <algorithm>
#include <cmath>
#include <vector>
#include "engine/sound_ids.h"
#include "rpg/sim/anim.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/ai_ops.h"
#include "rpg/sim/game.h"

using art::Monster;

// behaviour sets: wolves circle and lunge from the flank, goblins swarm and flee when hurt, bandit archers
// kite, bears and trolls mix in a slow heavy slam you roll through
// (M3c LIFE) the Wildlands wildlife: hyena packs and ember hounds circle and lunge like wolves (the hound's bite
// sets you alight), the yeti slams like a troll (and its slam frosts you), the scorpion stings hard every third
// blow, the lurker lies low and lunges from close by, the wisp kites and flickers and throws bolts of light, the
// blightspawn shambles in and bursts spores round itself now and then
// (M6 FOES) the harpy circles like a pack hunter and dives on you (a swoop you roll through), and shrieks; the golem
// is a brute whose every second blow is the heavy ground slam
struct Game::BeastKind {
  bool harpy, wolf, pack, goblin, golem, brute, archer, wisp, kite;
  float wu;   // the windup this blow takes
};

Game::BeastKind AiOps::beastKindOf(const Actor& a) {
  const bool harpy = a.mon == Monster::Harpy;
  const bool wolf = a.mon == Monster::Wolf || a.mon == Monster::IceWolf;
  const bool pack = wolf || a.mon == Monster::Hyena || a.mon == Monster::EmberHound || harpy;
  const bool goblin = a.mon == Monster::Goblin;
  const bool golem = a.mon == Monster::Golem;
  const bool brute = a.mon == Monster::Bear || a.mon == Monster::Troll || a.mon == Monster::Yeti || golem;
  const bool archer = a.ranged && a.human;
  const bool wisp = a.mon == Monster::Wisp;
  const bool kite = archer || wisp;
  // (the harpy rises before the dive; (P2a ANIM) a man or woman winds up by what they hold: animEnemyWindup)
  const float wu = a.heavy ? heavyWindup(a.mon) : (harpy && a.lunge ? 0.5f : a.human ? animEnemyWindup(a) : a.windup);
  return Game::BeastKind{harpy, wolf, pack, goblin, golem, brute, archer, wisp, kite, wu};
}

// dragon: its own dance (the fire-raining flight, then the ground fight with its breath cone)
void AiOps::beastDragon(Game& g, Actor& a, Actor& p, Vec2 toP, float dist, float dt) {
  a.special -= dt;
  // (M6 fixer) a world-boss dragon keeps to its territory: it gives up once the hero is well clear of its home
  // (or once it has been drawn too far out), wings back to its roost, and only wakes again for a hero inside it.
  // Ashfang (the story dragon) keeps the old chase.
  if (a.rank == (uint8_t)foes::Rank::WorldBoss) {
    const float territory = 22.0f * TILE;
    const float pFromHome = len(p.p - a.home), meFromHome = len(a.p - a.home);
    if (a.aggro && (pFromHome > territory || meFromHome > territory * 1.4f || p.st == AState::Dead)) a.aggro = false;
    if (!a.aggro) {
      if (dist < 260 && pFromHome <= territory && p.st != AState::Dead) a.aggro = true;
      else {
        a.fly = meFromHome > 2.0f * TILE;   // home is a short flight away
        if (a.fly) {
          Vec2 d = a.home - a.p;
          a.p += norm(d) * std::min(len(d), 120.0f * dt);
          a.face = d.x >= 0 ? 2 : 3;
          a.st = AState::Walk;
        } else if (a.st == AState::Walk) { a.st = AState::Idle; a.stT = 0; }
        return;
      }
    }
  }
  if (!a.aggro) { if (dist < 260) a.aggro = true; else return; }
  a.aim = norm(toP);
  a.face = toP.x >= 0 ? 2 : 3;
  if (a.fly) {
    // circle the player and rain fire
    float ang = std::atan2(a.p.y - p.p.y, a.p.x - p.p.x) + dt * 0.9f;
    Vec2 want = p.p + Vec2(std::cos(ang), std::sin(ang)) * 90.0f;
    Vec2 d = want - a.p;
    a.p += d * std::min(1.0f, dt * 1.8f);
    a.st = AState::Walk;
    a.shootCd -= 0;
    if (a.shootCd <= 0) {
      a.shootCd = 1.6f;
      for (int k = -1; k <= 1; k++) {
        Projectile pr;
        pr.kind = ProjKind::DragonFire; pr.fromPlayer = false; pr.owner = a.id; pr.fac = a.faction;
        Vec2 aim = norm(p.p + Vec2(k * 14.0f, 0) - (a.p + Vec2(0, -28)));
        pr.p = a.p + Vec2(0, -28); pr.v = aim * 150; pr.dmg = a.dmg * 0.8f; pr.life = 0.7f + (g.rng_.f() * 0.3f); pr.radius = 6;
        // fire lands on the ground near the player: let it travel to the target point
        pr.life = std::min(1.4f, len(p.p - pr.p) / 150.0f);
        g.projs.push_back(pr);
      }
      g.sfx((int)Sfx::Fireball, a.p, 0.6f);
    }
    if (a.special <= 0) { a.fly = false; a.special = 7.0f; g.sfx((int)Sfx::Roar, a.p); g.emit(Ev::Shake, a.p, 0, 5); a.st = AState::Idle; a.stT = 0; }
  } else {
    if (a.special <= 0) { a.fly = true; a.special = 9.0f; g.sfx((int)Sfx::Roar, a.p, 1.1f); return; }
    if (a.st == AState::Windup) {
      if (a.stT > a.windup) { a.st = AState::Strike; a.stT = 0; g.meleeHit(a); g.sfx((int)Sfx::HitHeavy, a.p, 0.6f); g.emit(Ev::Shake, a.p, 0, 4); }
      return;
    }
    if (a.st == AState::Strike) { if (a.stT > 0.4f) { a.st = AState::Idle; a.stT = 0; a.atkCd = 1.2f; } return; }
    if (dist > a.range + 4) { ClimbOps::moveActor(g, a, a.aim * (a.speed * 0.6f * dt)); a.st = AState::Walk; }
    else if (a.atkCd <= 0) { a.st = AState::Windup; a.stT = 0; }
    // breath cone
    if (a.shootCd <= 0 && dist < 120) {
      a.shootCd = 2.6f;
      for (int k = 0; k < 5; k++) {
        Projectile pr; pr.kind = ProjKind::DragonFire; pr.owner = a.id; pr.radius = 5; pr.fac = a.faction;
        float ang = std::atan2(a.aim.y, a.aim.x) + (k - 2) * 0.18f;
        pr.p = a.p + Vec2(a.aim.x * 20, -14); pr.v = Vec2(std::cos(ang), std::sin(ang)) * 170; pr.dmg = a.dmg * 0.6f; pr.life = 0.6f;
        g.projs.push_back(pr);
      }
      g.sfx((int)Sfx::Fireball, a.p, 0.5f);
    }
  }
  return;
}

// every hostile but the dragon: the committed attack first, then the chase or the walk home
// (The windup and the strike stay in this one function on purpose: as functions of their own, MSVC's /fp:fast compiled
// norm()'s x * (1 / l) as x / l in one place and not the other, and the sim digest changed: SOLID 3.5 step report.)
void AiOps::beastStep(Game& g, Actor& a, Actor& p, Vec2 toP, float dist, float dt, float slow) {
  const Game::BeastKind k = AiOps::beastKindOf(a);
  switch (a.st) {
    case AState::Windup: {
      const bool harpy = k.harpy;
      const bool pack = k.pack;
      const bool goblin = k.goblin;
      const bool wisp = k.wisp;
      const float wu = k.wu;
      // a blow at the current target only (the hound's fire bite, the scorpion's sting): in reach and in front
      auto hitTarget = [&](float mult, float extraReach, Ench e, float ep) -> bool {
        const Vec2 d = p.p - a.p;
        const float l = len(d);
        if (l > a.range + p.radius + extraReach || (l > 4 && dot(d * (1.0f / l), a.aim) < 0.1f)) return false;
        // (M3c fixer round 3, review: "a dodge roll does not stop the scorpion's sting venom") true only when the blow
        // LANDED: a roll's iframes, the grace after a hit or a block drop it inside damage(), and then no rider (venom, fire)
        const float hp0 = p.hp;
        g.damage(p, a.dmg * mult * (0.9f + g.rng_.f() * 0.2f), a.p, a.id, e, ep);
        return p.hp < hp0;
      };
      // the blightspawn's spore burst: a ring of short-lived spore clots flung out round it (they sting and slow)
      auto sporeBurst = [&]() {
        for (int k = 0; k < 10; k++) {
          Projectile pr;
          const float an = k * (TAU / 10) + g.rng_.f() * 0.3f;
          pr.kind = ProjKind::Spit; pr.fromPlayer = false; pr.owner = a.id; pr.fac = a.faction;
          pr.p = a.p + Vec2(0, -6); pr.v = Vec2(std::cos(an), std::sin(an) * 0.7f) * 105.0f;
          pr.dmg = a.dmg * 0.6f; pr.life = 0.42f; pr.radius = 4; pr.ench = Ench::Frost; pr.enchPow = 1;
          g.projs.push_back(pr);
        }
        for (int k = 0; k < 6; k++) g.emit(Ev::Dust, a.p + Vec2(std::cos(k * 1.047f) * 14, std::sin(k * 1.047f) * 9));
        g.sfx((int)Sfx::Splash, a.p, 0.7f, 0.9f);
      };
      a.face = faceOf(toP);
      if (!a.lunge) a.aim = norm(toP);
      if (a.heavy && a.stT < wu - 0.2f) a.aim = norm(toP);   // tracks you, then commits
      if (a.stT >= wu) {
        a.st = AState::Strike; a.stT = 0; a.atkN++; a.special = 0;
        if (a.ranged && dist > a.range + 6) {
          Projectile pr;
          pr.owner = a.id; pr.fromPlayer = false; pr.dmg = a.dmg; pr.fac = a.faction;
          Vec2 aim = norm(p.p + Vec2(0, -6) - (a.p + Vec2(0, -8)));
          pr.p = a.p + Vec2(0, -8) + aim * 5;
          if (a.human) { pr.kind = ProjKind::Arrow; pr.v = aim * 210; pr.life = 1.0f; g.sfx((int)Sfx::Arrow, a.p, 0.9f); }
          else if (a.mon == Monster::Wraith) { pr.kind = ProjKind::Magic; pr.v = aim * 130; pr.life = 1.6f; pr.ench = Ench::Frost; pr.enchPow = 4; g.sfx((int)Sfx::Frost, a.p, 0.8f); }
          else if (wisp) { pr.kind = ProjKind::Magic; pr.v = aim * 120; pr.life = 1.8f; pr.radius = 3; g.sfx((int)Sfx::Frost, a.p, 1.7f, 0.6f); }   // (a bolt of light: no frost)
          else { pr.kind = ProjKind::Spit; pr.v = aim * 150; pr.life = 1.0f; g.sfx((int)Sfx::Splash, a.p, 1.4f); }
          g.projs.push_back(pr);
          a.atkCd = 1.6f + g.rng_.f();
        } else if (a.heavy && a.mon == Monster::Scorpion) {
          // the sting: a long reach over the claws, hard, and its venom slows
          a.aim = norm(toP);
          if (hitTarget(1.8f, 9.0f, Ench::None, 0) && p.player && !g.godMode) {
            p.slowT = std::max(p.slowT, 2.5f);
            g.emit(Ev::Text, p.p + Vec2(0, -26), (int)rgba(196, 236, 96), 0, "VENOM");
          }
          g.sfx((int)Sfx::Hit, a.p, 0.6f);
          a.vel = a.aim * 50.0f;
          a.atkCd = 1.3f + g.rng_.f() * 0.5f;
        } else if (a.heavy && a.mon == Monster::Blightspawn) {
          sporeBurst();
          a.atkCd = 1.2f + g.rng_.f() * 0.5f;
        } else if (a.heavy) {
          AiOps::heavySlam(g, a);
          a.vel = a.aim * 30.0f;
          a.atkCd = 1.5f + g.rng_.f() * 0.6f;
        } else if (a.lunge) {
          a.aim = norm(toP);
          a.vel = a.aim * (a.mon == Monster::Lurker ? 260.0f : harpy ? 270.0f : 235.0f);
          a.hitDone = false;
          // (Witness playtests) a pack hunter falls back to the ring after its lunge and circles a while before the next
          // (1.5-2.3 s): the pack's pressure is a rhythm to read, not a bite a second (a level-1 hero who never rolled
          // lost to three same-level wolves in 8 s)
          a.atkCd = (pack && !harpy ? 1.5f : 0.8f) + g.rng_.f() * (pack && !harpy ? 0.8f : 0.6f);
          g.sfx((int)Sfx::Swing, a.p, 0.7f);
        } else if (a.mon == Monster::EmberHound) {
          hitTarget(1.0f, 2.0f, Ench::Fire, 4.0f);
          a.atkCd = 1.0f + g.rng_.f() * 0.6f;
          a.vel = a.aim * 120.0f;
          g.sfx((int)Sfx::Fireball, a.p, 1.6f, 0.5f);
        } else {
          g.meleeHit(a);
          a.atkCd = 1.0f + g.rng_.f() * 0.7f;
          if (pack || a.mon == Monster::Boar || goblin) a.vel = a.aim * 120.0f;
          else a.vel = a.aim * 40.0f;
          g.sfx((int)Sfx::Swing, a.p, 0.8f);
        }
      }
      return;
    }
    case AState::Strike:
      // a lunge bends a little toward a target that sidesteps; (Witness playtests) never after one mid-roll: the roll is
      // the answer to the telegraph, and a lunge that homed in on it caught a roll begun as the windup was seen
      if (a.lunge && !a.hitDone && len2(a.vel) > 1 && p.st != AState::Roll) {
        float sp = len(a.vel);
        a.vel = norm(norm(a.vel) + norm(toP) * std::min(1.0f, dt * 4.0f)) * sp;
      }
      ClimbOps::moveActor(g, a, a.vel * dt);
      a.vel *= std::pow(0.01f, dt);
      if (a.lunge && !a.hitDone && len(p.p - a.p) < a.range + p.radius + 2) {   // the bite lands on contact
        a.aim = norm(p.p - a.p);
        const bool ember = a.mon == Monster::EmberHound;
        g.damage(p, a.dmg * (a.mon == Monster::Lurker ? 1.3f : 1.0f) * (0.9f + g.rng_.f() * 0.2f), a.p, a.id, ember ? Ench::Fire : Ench::None, ember ? 4.0f : 0.0f);
        a.hitDone = true;
        a.vel *= 0.25f;
      }
      if (a.stT >= (a.heavy ? 0.4f : 0.28f)) { a.st = AState::Recover; a.stT = 0; }
      return;
    case AState::Recover:
      if (a.stT >= (a.heavy ? 0.75f : 0.35f)) { a.st = AState::Idle; a.stT = 0; a.heavy = false; a.lunge = false; }
      return;
    default: break;
  }
  if (a.aggro && p.st != AState::Dead) AiOps::beastPursue(g, a, p, toP, dist, dt, slow, k);
  else AiOps::beastWander(g, a, dt, slow);
}

// in the fight: close in by kind (flee, circle, swarm, kite, plain), flicker, sidestep when stuck, shriek; then the
// next blow
void AiOps::beastPursue(Game& g, Actor& a, Actor& p, Vec2 toP, float dist, float dt, float slow, const Game::BeastKind& k) {
  const bool harpy = k.harpy;
  const bool pack = k.pack;
  const bool goblin = k.goblin;
  const bool wisp = k.wisp;
  const bool kite = k.kite;
  a.aim = norm(toP);
  a.face = faceOf(toP);
  Vec2 side(-a.aim.y, a.aim.x);
  float want = a.ranged ? 70.0f : a.range * 0.8f;
  if (a.mon == Monster::Scorpion) want = a.range * 0.7f;
  float spd = 1.0f;
  Vec2 mv;
  if (a.fleeing) {
    // run, weaving a little; once well away, give up the fight
    mv = a.aim * -1.0f + side * (std::sin(a.animT * 3 + a.id) * 0.5f);
    spd = 1.1f;
    if (dist > 11 * TILE) { a.aggro = false; a.fleeing = false; a.goal = a.home; }
  } else if (pack) {
    // circle at a few strides, drifting in and out; the lunge comes from wherever the player isn't looking
    // (hyenas ring wider: the pack surrounds you before one darts in)
    float orbitR = (a.mon == Monster::Hyena ? 46.0f : harpy ? 54.0f : 38.0f) + (a.id % 3) * 5.0f;
    float radial = clampf((dist - orbitR) / 14.0f, -1.0f, 1.0f);
    mv = a.aim * radial + side * (0.95f * a.orbitDir);
    spd = 0.85f;
    if (g.rng_.f() < dt * 0.35f) a.orbitDir = (int8_t)-a.orbitDir;
    a.special += dt;   // time spent circling
  } else if (goblin) {
    // swarm: each goblin closes in from its own side so the pack surrounds you
    float spread = (float)((int)(a.id % 3) - 1) * 0.75f;
    if (dist > want) mv = a.aim + side * (dist > 28 ? spread : 0.0f);
    spd = 1.05f;
  } else if (kite) {
    // keep a bow-shot away: back off when crowded, close in when far, strafe in between
    if (dist < 62) mv = a.aim * -1.0f + side * (0.5f * a.orbitDir);
    else if (dist > 115) mv = a.aim;
    else mv = side * (0.6f * a.orbitDir);
    if (g.rng_.f() < dt * 0.4f) a.orbitDir = (int8_t)-a.orbitDir;
  } else if (dist > want) mv = a.aim;
  if (a.mon == Monster::Bat || a.mon == Monster::Wraith || wisp || harpy) {   // erratic flight
    float w = std::sin(a.animT * 5 + a.id) * 0.8f;
    mv = mv + side * w;
  }
  if (wisp && g.rng_.f() < dt * 0.3f) {   // the wisp flickers out and back a few strides aside
    const Vec2 to = a.p + side * (g.rng_.f() < 0.5f ? 22.0f : -22.0f);
    if (ClimbOps::bodyFree(g, to, a.radius, true)) { g.emit(Ev::Sparkle, a.p + Vec2(0, -8)); a.p = to; g.emit(Ev::Sparkle, a.p + Vec2(0, -8)); }
  }
  // simple obstacle avoidance: if stuck, sidestep
  Vec2 before = a.p;
  if (len2(mv) > 0.01f) {
    ClimbOps::moveActor(g, a, norm(mv) * (a.speed * spd * slow * dt));
    a.st = AState::Walk;
    if (len2(a.p - before) < 0.02f * a.speed * dt) {
      Vec2 sd = side * ((a.id & 1) ? 1.0f : -1.0f);
      ClimbOps::moveActor(g, a, sd * (a.speed * slow * dt));
      if (pack) a.orbitDir = (int8_t)-a.orbitDir;
      if (a.fleeing && dist < 34) a.fleeing = false;   // cornered: fight
    }
  } else a.st = AState::Idle;
  if (a.fleeing) return;
  if (harpy && a.shootCd <= 0 && dist < 84.0f && a.st != AState::Windup) {
    // the shriek: a scream that rattles whoever is close (a short slow); the harpy hangs in the air a moment
    a.shootCd = 7.0f + g.rng_.f() * 3.0f;
    g.emit(Ev::Text, a.p + Vec2(0, -24), (int)rgba(236, 200, 255), 0, "SHRIEK");
    g.emit(Ev::Shake, a.p, 0, 1.5f);
    g.sfx((int)Sfx::Roar, a.p, 2.3f, 0.7f);
    if (p.player && !g.godMode && p.iframes <= 0) p.slowT = std::max(p.slowT, 1.4f);
    a.atkCd = std::max(a.atkCd, 0.6f);
  }
  AiOps::beastAttack(g, a, p, dist, dt, k);
}

// the next blow, by the same if-ladder: pack, brute, scorpion, lurker, blightspawn, then any other
void AiOps::beastAttack(Game& g, Actor& a, Actor& p, float dist, float dt, const Game::BeastKind& k) {
  const bool pack = k.pack;
  const bool golem = k.golem;
  const bool brute = k.brute;
  bool inMelee = dist < a.range + p.radius + 2;
  bool inShot = a.ranged && dist < 150 && dist > 30;
  if (a.atkCd > 0) return;
  if (pack) { AiOps::beastPackAttack(g, a, p, dist, inMelee, k); return; }
  if (brute) {
    bool heavyNow = golem ? (a.atkN % 2 == 1) : ((a.atkN % 3 == 2) || g.rng_.f() < 0.15f);
    if (inMelee || (heavyNow && dist < a.range + p.radius + 12)) { a.st = AState::Windup; a.stT = 0; a.heavy = heavyNow; }
    return;
  }
  if (a.mon == Monster::Scorpion) {   // claws twice, then the sting (telegraphed: roll away or through it)
    const bool sting = a.atkN % 3 == 2;
    if (inMelee || (sting && dist < a.range + p.radius + 8)) { a.st = AState::Windup; a.stT = 0; a.heavy = sting; }
    return;
  }
  if (a.mon == Monster::Lurker) {   // a short rush from a few strides, or a snap up close
    if (dist > 18 && dist < 58) { a.st = AState::Windup; a.stT = 0; a.lunge = true; }
    else if (inMelee) { a.st = AState::Windup; a.stT = 0; a.lunge = false; }
    return;
  }
  if (a.mon == Monster::Blightspawn && a.shootCd <= 0 && dist < 44) {   // the spore burst (its ring shows the cloud)
    a.st = AState::Windup; a.stT = 0; a.heavy = true;
    a.shootCd = 6.0f + g.rng_.f() * 2.0f;
    return;
  }
  if (inMelee || (inShot && g.rng_.f() < dt * 2.5f)) { a.st = AState::Windup; a.stT = 0; }
}

// the pack's turn: a lunge from the flank or a snap up close
void AiOps::beastPackAttack(Game& g, Actor& a, Actor& p, float dist, bool inMelee, const Game::BeastKind& k) {
  const bool harpy = k.harpy;
  // the pack takes turns (two at once in a big pack); prefer the flank, but don't circle forever
  int busy = 0, mates = 0;
  for (const Actor& o : g.actors) {
    if (o.id == a.id || o.st == AState::Dead || o.mon != a.mon || len2(o.p - p.p) > 140 * 140) continue;
    mates++;
    if (o.lunge && o.st == AState::Windup) busy++;
  }
  // (Witness playtests) "big" is four or more: a pack of three lunges one wolf at a time, so a newcomer reads one
  // telegraph at a time (two at once from three left a slow reaction no safe way to roll)
  int slots = mates >= 3 ? 2 : 1;
  bool flank = dot(norm(a.p - p.p), p.aim) < 0.3f;
  const float lungeMax = a.mon == Monster::Hyena ? 62.0f : a.mon == Monster::EmberHound ? 54.0f : harpy ? 76.0f : 48.0f;   // (beyond the pack's ring)
  if (busy < slots && dist > 16 && dist < lungeMax && (flank || a.special > 0.7f)) {
    a.st = AState::Windup; a.stT = 0; a.lunge = true;
  } else if (inMelee && dist <= 16 && busy < slots) {
    a.st = AState::Windup; a.stT = 0; a.lunge = false;
  }
}

// out of the fight: wander near home; a pack that lost its prey trots back to the den
void AiOps::beastWander(Game& g, Actor& a, float dt, float slow) {
  Vec2 d = a.goal - a.p;
  float l = len(d);
  // a pack that lost its prey trots back to the den
  float back = (a.den >= 0 && len2(a.p - a.home) > 5.0f * TILE * 5.0f * TILE) ? 0.7f : 0.35f;
  if (back > 0.5f) { d = a.home - a.p; l = len(d); }
  if (l > 4) { ClimbOps::moveActor(g, a, d * (1.0f / l) * (a.speed * back * slow * dt)); a.face = faceOf(d); a.st = AState::Walk; }
  else a.st = AState::Idle;
}
