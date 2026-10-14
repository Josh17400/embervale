// M8 Fellowship: how the companions fight (VISION_PLAN 9 AI; rpg/sim/party.h). FELLOWS lane.
//   - they engage what the player strikes or aims at, and whatever attacks the party (never townsfolk: their blows and
//     shots hit only what the player's would, Projectile::fromPlayer and meleeHit's warFoes);
//   - the Warrior closes in and holds the line (Taunt pulls the foes within 5 tiles, Shield Wall halves the blows on
//     the player and the party, Last Stand fights on at 1 HP: partyFell);
//   - the Archer keeps 3..6 tiles from its mark (the bandit-archer kiting; Pinning Shot slows, Volley rains on the
//     mark's pack, Eagle Eye: long, armour-piercing shots);
//   - the Mage casts a bolt of their people's tradition (fire, frost or plain force by cult::Culture::magicTradition;
//     Firebolt a heavy one, Ward a shield on the player, Storm a burst round them);
//   - the Healer mends whoever is hurt worst below half health (Mend: sooner and more; Purify puts out burns and lifts
//     slows; Second Wind raises a downed companion from afar);
//   - the Rogue circles to the mark's back (the wolf circling logic) and stabs there for more (Flank: a dash behind it,
//     Smoke: the foes lose the party, Assassinate: a finishing blow on the badly hurt);
//   - the Hound bites (Guard: stands over the downed, drawing the foes; Pin Down holds a foe; Fetch brings a pickup);
//   - all of them drink a potion they were given below 25 % health;
//   - stances: Follow (fight what the player fights), Attack (the player's mark first), Defend (never more than 3 tiles
//     from the player, intercepting what comes at them), Passive (keep out of it).
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/magic_ops.h"
#include "rpg/sim/party.h"
#include "rpg/world/source.h"

using party::Member;
using party::State;
using party::Vocation;

namespace {
constexpr float T = (float)TILE;
bool alive(const Actor& e) { return e.st != AState::Dead && !e.indoors && !e.fly; }
// a clear line for a shot (no wall, building or tree between)
bool lineClear(const Game& g, Vec2 a, Vec2 b) {
  const Map& m = g.map();
  const Vec2 d = b - a;
  const float l = len(d);
  for (float t = 8.0f; t < l - 6.0f; t += 8.0f) {
    const Vec2 q = a + d * (t / l);
    const int tx = (int)std::floor(q.x / T), ty = (int)std::floor((q.y + 2) / T);
    if (!m.in(tx, ty)) return false;
    const size_t i = (size_t)ty * m.w + tx;
    if (m.bldgAt[i] >= 0 || m.wall[i]) return false;
    const Ground gr = m.at(tx, ty);
    if (gr == Ground::Rock || gr == Ground::CaveWall || gr == Ground::InteriorWall || gr == Ground::Void) return false;
    const int pp = m.propAt(tx, ty);
    if (pp && art::isTreeProp((art::Prop)(pp - 1)) && (art::Prop)(pp - 1) != art::Prop::DeadTree) return false;
  }
  return true;
}
// a body walks straight from a to b (the box clear every 4 px along the way)
bool straightWay(const Game& g, Vec2 a, Vec2 b, float r) {
  const Vec2 d = b - a;
  const float l = len(d);
  for (float t = 4.0f; t < l; t += 4.0f) if (!HomeOps::bodyFree(g, a + d * (t / l), r)) return false;
  return HomeOps::bodyFree(g, b, r);
}
// Defend's goal: `want` when the way there is straight; else (a trunk or a boulder between: the path search would send
// them the long way round, out past the leash) the place at the player's side, a tile or so out, that they can walk
// straight to and that lies nearest `want`; else `want` round the obstacle
Vec2 defendGoal(const Game& g, const Actor& a, Vec2 want) {
  if (straightWay(g, a.p, want, a.radius)) return want;
  const Vec2 pp = g.pl().p;
  static const float dirs[8][2] = {{1, 0}, {0.7071f, 0.7071f}, {0, 1}, {-0.7071f, 0.7071f}, {-1, 0}, {-0.7071f, -0.7071f}, {0, -1}, {0.7071f, -0.7071f}};
  Vec2 best = want;
  float bd = 1e30f;
  for (int ring = 0; ring < 2; ring++)
    for (const auto& dv : dirs) {
      const Vec2 c = pp + Vec2(dv[0], dv[1]) * ((1.1f + 0.6f * (float)ring) * T);
      const float d2 = len2(c - want);
      if (d2 >= bd || !straightWay(g, a.p, c, a.radius) || !straightWay(g, c, pp, a.radius - 1.0f)) continue;
      bd = d2; best = c;
    }
  // (M7c merge, --companion seed 12: a birch's trunk square between them and the player, every spot at the player's
  // side behind it) a step round the trunk, a tile out from where they stand, the one that ends nearest `want`
  if (bd >= 1e30f) {
    float bs = len2(a.p - want);
    for (const auto& dv : dirs) {
      const Vec2 c = a.p + Vec2(dv[0], dv[1]) * T;
      const float d2 = len2(c - want);
      if (d2 >= bs || !straightWay(g, a.p, c, a.radius)) continue;
      bs = d2; best = c;
    }
  }
  return best;
}
// the bolt of the mage's people's tradition
ProjKind boltOf(const Game& g, const Member& m, Ench& e) {
  uint32_t trad = 0;
  if (m.culture && g.world.src) trad = g.world.src->culture(m.culture).magicTradition;
  switch (trad % 3u) {
    case 1: e = Ench::Frost; return ProjKind::IceSpike;
    case 2: e = Ench::None; return ProjKind::Magic;
    default: e = Ench::Fire; return ProjKind::Fireball;
  }
}
}  // namespace

float PartyOps::tauntT(const Member& m) { return m.tauntT; }

// ---------------------------------------------------------------- choosing the foe
int PartyOps::pickFoe(Game& g, const Actor& a, const Member& m) {
  if (m.stance == party::Cmd::Passive) return -1;
  const Actor& p = g.pl();
  const float reach = std::max(a.aggroR, 7.0f * T);
  int best = -1;
  float bs = 1e30f;
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& e = g.actors[i];
    if (!e.hostile || !alive(e) || e.comp) continue;
    const float dMe = len(e.p - a.p), dPl = len(e.p - p.p);
    if (dMe > reach && dPl > reach) continue;
    // engaged: it fights the player or the party, the player struck it lately, or it is at their throat
    const bool atUs = e.aggro && (e.target == p.id || g.party.indexOfActor(e.target) >= 0);
    const bool struck = e.plHitT > -99 && g.time - e.plHitT < 6.0f;
    const bool close = e.aggro && dMe < 2.5f * T;
    if (!atUs && !struck && !close) continue;
    if (m.stance == party::Cmd::Defend && dPl > 4.0f * T) continue;   // Defend: only what comes at the player
    float sc = std::min(dMe, dPl * 1.1f);
    if (e.target == p.id) sc *= 0.75f;   // what threatens the player first
    if (m.stance == party::Cmd::Attack && e.id == g.party.playerTarget && g.time - g.party.playerTargetT < 8.0f) sc *= 0.25f;
    if (e.id == m.target) sc *= 0.85f;   // (sticky: no flipping between two)
    if (m.voc == Vocation::Rogue && e.target != a.id) sc *= 0.8f;   // a back to stab: one busy with someone else
    if (sc < bs) { bs = sc; best = e.id; }
  }
  return best;
}

// ---------------------------------------------------------------- the blow / the shot
void PartyOps::strike(Game& g, Actor& a, Member& m) {
  const int ti = m.target >= 0 ? g.findActor(m.target) : -1;
  if (ranged(m) && m.closeIn) {   // the knife (the staff): a short, weaker blow
    const float d0 = a.dmg, r0 = a.range;
    a.dmg *= 0.6f; a.range = 15.0f;
    g.meleeHit(a);
    a.dmg = d0; a.range = r0;
    return;
  }
  if (ranged(m)) {
    if (ti <= 0) return;
    const Actor& t = g.actors[(size_t)ti];
    Projectile pr;
    const bool mage = m.voc == Vocation::Mage;
    pr.ench = Ench::None;
    pr.kind = mage ? boltOf(g, m, pr.ench) : ProjKind::Arrow;
    pr.enchPow = mage ? 2.0f : 0.0f;
    if (mage) {   // (M9) the rank-1 bolt of their land's tradition, a spell missile (Common Arcana: the classic FIREBOLT)
      pr.spell = MagicOps::companionSpell(g, m.culture);
      pr.kind = ProjKind::Spell;
      if (const magic::SpellDef* sd = magic::spellById(pr.spell))
        pr.ench = sd->ess == magic::Essence::Fire ? Ench::Fire : sd->ess == magic::Essence::Frost ? Ench::Frost : Ench::None;
    }
    pr.fromPlayer = true;   // (it hits what the player's shots hit: the hostile; never the player or the townsfolk)
    pr.owner = a.id; pr.fac = Faction::Player;
    pr.p = a.p + Vec2(0, -8);
    const Vec2 aim = norm(t.p + Vec2(0, -6) - pr.p);
    pr.v = aim * (mage ? 200.0f : 270.0f);
    pr.dmg = a.dmg * (m.eagleT > 0 ? 1.5f : 1.0f);
    pr.radius = mage ? 4.0f : 3.0f;
    pr.life = std::min(1.8f, (a.range * (m.eagleT > 0 ? 1.5f : 1.0f) + 40.0f) / len(pr.v));
    g.projs.push_back(pr);
    g.party.stats.arrows++;
    if (mage) g.party.stats.bolts++;
    g.sfx((int)(mage ? Sfx::Fireball : Sfx::Arrow), a.p, mage ? 1.2f : 1.05f, 0.7f);
    return;
  }
  // melee: the rogue's stab in the back and the finishing blow hit harder
  float mult = 1.0f;
  if (ti > 0) {
    const Actor& t = g.actors[(size_t)ti];
    const Vec2 fromT = norm(a.p - t.p + Vec2(0.0001f, 0));
    if (m.voc == Vocation::Rogue && dot(fromT, norm(t.aim + Vec2(0.0001f, 0))) < -0.2f) { mult = 1.6f; g.party.stats.backstabs++; }
  }
  if (m.voc == Vocation::Hound) g.party.stats.bites++;
  const float d0 = a.dmg;
  a.dmg *= mult;
  g.meleeHit(a);
  a.dmg = d0;
}

// ---------------------------------------------------------------- potions they were given
void PartyOps::potion(Game& g, Actor& a, Member& m) {
  if (m.potionCd > 0 || a.hp >= a.maxHp * 0.25f || a.hp <= 0) return;
  for (size_t i = 0; i < m.inv.size(); i++) {
    Item& it = m.inv[i];
    if (it.kind != ItemKind::Potion || it.sub != (uint8_t)PotionType::Health || it.count <= 0) continue;
    // (no slot points at a potion: potions are never worn)
    a.hp = std::min(a.maxHp, a.hp + std::max((float)it.power, a.maxHp * 0.35f));
    m.hp = a.hp;
    m.potionCd = 8.0f;
    g.emit(Ev::Heal, a.p);
    g.sfx((int)Sfx::Heal, a.p, 1.25f, 0.6f);
    g.party.stats.potions++;
    if (--it.count <= 0) {
      m.inv.erase(m.inv.begin() + (std::ptrdiff_t)i);
      for (int16_t& x : m.eq) if (x > (int16_t)i) x--;
    }
    return;
  }
}

// ---------------------------------------------------------------- the healer's mend
void PartyOps::heal(Game& g, Actor& a, Member& m, int mi) {
  (void)mi;
  if (m.voc != Vocation::Healer) return;
  const bool mend = (party::unlockedSkills(m) & 1) != 0;   // Mend: sooner and more
  if (m.skillCd[0] > 0) return;
  // whoever is hurt worst below half health (the player or one of the party), within 8 tiles
  Actor* best = nullptr;
  float bf = 0.5f;
  Actor& p = g.pl();
  if (p.st != AState::Dead && p.hp < p.maxHp * bf && len2(p.p - a.p) < (8.0f * T) * (8.0f * T)) { bf = p.hp / p.maxHp; best = &p; }
  for (size_t k = 0; k < g.party.members.size(); k++) {
    if (g.party.members[k].state != State::With) continue;
    const int ai = actorOf(g, (int)k);
    if (ai <= 0) continue;
    Actor& o = g.actors[(size_t)ai];
    if (o.maxHp > 0 && o.hp < o.maxHp * bf && len2(o.p - a.p) < (8.0f * T) * (8.0f * T)) { bf = o.hp / o.maxHp; best = &o; }
  }
  if (!best) return;
  m.skillCd[0] = mend ? party::skillInfo(party::Skill::Mend).cooldown * 0.5f : 9.0f;
  best->hp = std::min(best->maxHp, best->hp + best->maxHp * (mend ? 0.3f : 0.2f));
  g.emit(Ev::Heal, best->p);
  g.emit(Ev::Text, best->p + Vec2(0, -26), (int)rgba(150, 255, 160), 0, "MENDED");
  g.sfx((int)Sfx::Heal, a.p, 1.1f, 0.6f);
  a.face = faceOf(best->p - a.p);
  g.party.stats.heals++;
}

// ---------------------------------------------------------------- the bond skills
bool PartyOps::useSkill(Game& g, Actor& a, Member& m, int k, bool ordered) {
  if (k < 0 || k > 2 || !(party::unlockedSkills(m) & (1u << k)) || m.skillCd[k] > 0) return false;
  const party::Skill sk = party::skillsOf(m.voc)[(size_t)k];
  const party::SkillInfo& si = party::skillInfo(sk);
  Actor& p = g.pl();
  const int ti = m.target >= 0 ? g.findActor(m.target) : -1;
  Actor* t = ti > 0 && g.actors[(size_t)ti].hostile && alive(g.actors[(size_t)ti]) ? &g.actors[(size_t)ti] : nullptr;
  auto foesNear = [&](Vec2 c, float r) {
    int n = 0;
    for (size_t i = 1; i < g.actors.size(); i++) {
      const Actor& e = g.actors[i];
      if (e.hostile && alive(e) && !e.comp && len2(e.p - c) < r * r) n++;
    }
    return n;
  };
  auto text = [&](const char* s, uint32_t col) { g.emit(Ev::Text, a.p + Vec2(0, -30), (int)col, 0, s); };
  bool done = false;
  switch (sk) {
    case party::Skill::Taunt: {
      int pulled = 0;
      for (size_t i = 1; i < g.actors.size(); i++) {
        Actor& e = g.actors[i];
        if (!e.hostile || !alive(e) || e.boss || len2(e.p - a.p) > (5.0f * T) * (5.0f * T)) continue;
        e.target = a.id; e.aggro = true; e.thinkT = std::max(e.thinkT, 2.5f);
        pulled++;
      }
      if (!pulled && !ordered) return false;
      m.tauntT = 4.0f;
      g.party.stats.taunts++;
      text("TAUNT", rgba(255, 190, 110));
      g.sfx((int)Sfx::Roar, a.p, 1.5f, 0.5f);
      done = true;
      break;
    }
    case party::Skill::ShieldWall:
      g.party.shieldWallT = 6.0f;
      g.party.stats.shieldWalls++;
      text("SHIELD WALL", rgba(200, 220, 255));
      g.sfx((int)Sfx::Block, a.p, 0.8f);
      done = true;
      break;
    case party::Skill::LastStand: return false;   // (it fires by itself when they fall: partyFell)
    case party::Skill::PinningShot: case party::Skill::Volley: {
      if (!t) return false;
      if (sk == party::Skill::PinningShot) {
        t->slowT = std::max(t->slowT, 3.0f);
        g.damage(*t, a.dmg * 0.8f, a.p, a.id, Ench::Frost, 1.0f);
        g.party.stats.pins++;
        g.party.stats.arrows++;   // (a Pinning Shot is an arrow loosed too)
        text("PINNED", rgba(200, 230, 255));
      } else {
        if (foesNear(t->p, 2.5f * T) < 2 && !ordered) return false;
        const Vec2 c = t->p;
        std::vector<int> ids;
        for (size_t i = 1; i < g.actors.size(); i++) {
          const Actor& e = g.actors[i];
          if (e.hostile && alive(e) && !e.comp && len2(e.p - c) < (2.5f * T) * (2.5f * T)) ids.push_back(e.id);
        }
        for (int id : ids) {
          const int ei = g.findActor(id);
          if (ei > 0 && g.actors[(size_t)ei].st != AState::Dead) { g.emit(Ev::Sparkle, g.actors[(size_t)ei].p + Vec2(0, -10)); g.damage(g.actors[(size_t)ei], a.dmg * 0.8f, a.p, a.id); }
        }
        g.party.stats.volleys++;
        text("VOLLEY", rgba(255, 230, 160));
      }
      g.sfx((int)Sfx::Arrow, a.p, 0.9f, 0.8f);
      done = true;
      break;
    }
    case party::Skill::EagleEye:
      if (!t && !ordered) return false;
      m.eagleT = 8.0f;
      g.party.stats.eagleEyes++;
      text("EAGLE EYE", rgba(255, 240, 180));
      done = true;
      break;
    case party::Skill::Firebolt: {
      if (!t) return false;
      Projectile pr;
      pr.kind = boltOf(g, m, pr.ench);
      pr.enchPow = 4.0f;
      pr.fromPlayer = true; pr.owner = a.id; pr.fac = Faction::Player;
      pr.p = a.p + Vec2(0, -8);
      pr.v = norm(t->p + Vec2(0, -6) - pr.p) * 210.0f;
      pr.dmg = a.dmg * 1.8f; pr.radius = 5.0f; pr.life = 1.6f;
      g.projs.push_back(pr);
      g.party.stats.bolts++;
      g.sfx((int)Sfx::Fireball, a.p, 0.9f, 0.9f);
      done = true;
      break;
    }
    case party::Skill::Ward:
      if (!ordered && (g.party.wardHp > 0 || p.hp > p.maxHp * 0.6f)) return false;
      g.party.wardHp = p.maxHp * 0.25f;
      g.emit(Ev::Sparkle, p.p + Vec2(0, -10));
      g.party.stats.wards++;
      text("WARD", rgba(170, 210, 255));
      g.sfx((int)Sfx::Heal, p.p, 1.4f, 0.6f);
      done = true;
      break;
    case party::Skill::Storm: {
      if (foesNear(a.p, 3.5f * T) < 2 && !ordered) return false;
      std::vector<int> ids;
      for (size_t i = 1; i < g.actors.size(); i++) {
        const Actor& e = g.actors[i];
        if (e.hostile && alive(e) && !e.comp && len2(e.p - a.p) < (3.5f * T) * (3.5f * T)) ids.push_back(e.id);
      }
      for (int id : ids) {
        const int ei = g.findActor(id);
        if (ei > 0 && g.actors[(size_t)ei].st != AState::Dead) g.damage(g.actors[(size_t)ei], a.dmg * 1.5f, a.p, a.id, Ench::None, 0);
      }
      g.emit(Ev::Explode, a.p, 0, 1.4f);
      g.emit(Ev::Shake, a.p, 0, 3.0f);
      g.party.stats.storms++;
      text("STORM", rgba(190, 200, 255));
      g.sfx((int)Sfx::Explode, a.p, 1.2f, 0.9f);
      done = true;
      break;
    }
    case party::Skill::Mend: {
      const float cd = m.skillCd[0];
      m.skillCd[0] = 0;
      const int heals = g.party.stats.heals;
      heal(g, a, m, indexOf(g, m));
      if (g.party.stats.heals == heals) { m.skillCd[0] = cd; return false; }
      return true;   // (heal set its own cool-down)
    }
    case party::Skill::Purify: {
      bool any = p.burnT > 0 || p.slowT > 0;
      for (size_t k2 = 0; k2 < g.party.members.size() && !any; k2++) {
        const int ai = actorOf(g, (int)k2);
        if (ai > 0 && (g.actors[(size_t)ai].burnT > 0 || g.actors[(size_t)ai].slowT > 0)) any = true;
      }
      if (!any && !ordered) return false;
      p.burnT = 0; p.slowT = 0;
      for (size_t k2 = 0; k2 < g.party.members.size(); k2++) {
        const int ai = actorOf(g, (int)k2);
        if (ai > 0) { g.actors[(size_t)ai].burnT = 0; g.actors[(size_t)ai].slowT = 0; }
      }
      g.emit(Ev::Sparkle, p.p + Vec2(0, -10));
      g.party.stats.purifies++;
      text("PURIFY", rgba(220, 255, 220));
      done = true;
      break;
    }
    case party::Skill::SecondWind: {
      int who = -1;
      for (size_t k2 = 0; k2 < g.party.members.size() && who < 0; k2++)
        if (&g.party.members[k2] != &m && g.party.members[k2].state == State::Downed && actorOf(g, (int)k2) > 0) who = (int)k2;
      if (who < 0) return false;
      const int ai = actorOf(g, who);
      g.emit(Ev::Sparkle, g.actors[(size_t)ai].p + Vec2(0, -10));
      revive(g, who, g.actors[(size_t)ai], true);
      g.party.stats.secondWinds++;
      text("SECOND WIND", rgba(170, 255, 190));
      done = true;
      break;
    }
    case party::Skill::Flank: {
      if (!t || len(t->p - a.p) > 5.0f * T) return false;
      const Vec2 back = t->p - norm(t->aim + Vec2(0.0001f, 0)) * (t->radius + a.radius + 4.0f);
      if (g.bodyFree(back, a.radius, false)) { a.p = back; a.navNext = -1; g.emit(Ev::Dust, a.p); }
      a.aim = norm(t->p - a.p + Vec2(0.0001f, 0)); a.face = faceOf(a.aim);
      g.damage(*t, a.dmg * 2.0f, a.p, a.id);
      g.party.stats.flanks++;
      text("FLANK", rgba(255, 210, 150));
      done = true;
      break;
    }
    case party::Skill::Smoke: {
      if (!ordered && foesNear(a.p, 6.0f * T) < 2) return false;
      for (size_t i = 1; i < g.actors.size(); i++) {
        Actor& e = g.actors[i];
        if (!e.hostile || !alive(e) || e.boss || len2(e.p - a.p) > (6.0f * T) * (6.0f * T)) continue;
        e.aggro = false; e.thinkT = 3.0f; e.goal = e.p + norm(e.p - p.p + Vec2(0.0001f, 0)) * (3.0f * T);
        if (e.st == AState::Windup) { e.st = AState::Idle; e.stT = 0; }
      }
      g.party.smokeT = 3.5f;
      for (int k2 = 0; k2 < 8; k2++) g.emit(Ev::Dust, a.p + Vec2(std::cos(k2 * 0.785f) * 18.0f, std::sin(k2 * 0.785f) * 12.0f));
      g.party.stats.smokes++;
      text("SMOKE", rgba(200, 200, 200));
      done = true;
      break;
    }
    case party::Skill::Assassinate:
      if (!t || t->hp > t->maxHp * 0.3f || len(t->p - a.p) > a.range + t->radius + 6.0f) return false;
      g.damage(*t, a.dmg * 3.0f, a.p, a.id, Ench::None, 0, true);
      g.party.stats.assassinations++;
      text("ASSASSINATE", rgba(255, 120, 110));
      done = true;
      break;
    case party::Skill::Guard: {
      bool someone = false;
      for (const Member& o : g.party.members) if (o.state == State::Downed) someone = true;
      if (!someone && p.hp > p.maxHp * 0.3f && !ordered) return false;
      m.guardT = 8.0f;
      g.party.stats.guards++;
      text("GUARD", rgba(255, 220, 160));
      g.sfx((int)Sfx::Howl, a.p, 1.0f, 0.7f);
      done = true;
      break;
    }
    case party::Skill::PinDown:
      if (!t || len(t->p - a.p) > a.range + t->radius + 6.0f) return false;
      t->slowT = std::max(t->slowT, 3.0f);
      if (!t->boss) { t->st = AState::Hurt; t->stT = -0.9f; t->heavy = t->lunge = false; t->atkCd = std::max(t->atkCd, 1.0f); }
      g.damage(*t, a.dmg, a.p, a.id);
      g.party.stats.pinDowns++;
      text("PINNED DOWN", rgba(255, 220, 160));
      done = true;
      break;
    case party::Skill::Fetch: {
      int best = -1;
      float bd = (8.0f * T) * (8.0f * T);
      for (size_t i = 0; i < g.pickups.size(); i++) {
        const Pickup& k2 = g.pickups[i];
        if (k2.magnet || k2.full) continue;
        const float d2 = len2(k2.p - p.p);
        if (d2 > (2.5f * T) * (2.5f * T) && d2 < bd) { bd = d2; best = (int)i; }
      }
      if (best < 0) return false;
      g.pickups[(size_t)best].p = p.p + Vec2(10, 6);
      a.p = g.freeSpot((int)std::floor((p.p.x + 12) / T), (int)std::floor((p.p.y + 4) / T));
      g.party.stats.fetches++;
      done = true;
      break;
    }
    default: return false;
  }
  if (done) m.skillCd[k] = si.cooldown;
  return done;
}

void PartyOps::autoSkills(Game& g, Actor& a, Member& m, int mi) {
  (void)mi;
  const uint8_t un = party::unlockedSkills(m);
  if (!un || m.stance == party::Cmd::Passive) return;
  const Actor& p = g.pl();
  const int ti = m.target >= 0 ? g.findActor(m.target) : -1;
  const bool fighting = ti > 0;
  for (int k = 0; k < 3; k++) {
    if (!(un & (1u << k)) || m.skillCd[k] > 0) continue;
    const party::Skill sk = party::skillsOf(m.voc)[(size_t)k];
    bool want = false;
    switch (sk) {
      case party::Skill::Taunt: {
        // a foe within 5 tiles of them is at the player
        for (size_t i = 1; i < g.actors.size() && !want; i++) {
          const Actor& e = g.actors[i];
          if (e.hostile && alive(e) && e.aggro && e.target == p.id && len2(e.p - a.p) < (5.0f * T) * (5.0f * T)) want = true;
        }
        break;
      }
      case party::Skill::ShieldWall: want = fighting && (p.hp < p.maxHp * 0.6f || a.hp < a.maxHp * 0.5f); break;
      case party::Skill::PinningShot: want = fighting && len(g.actors[(size_t)ti].p - p.p) < 4.0f * T; break;
      case party::Skill::Volley: case party::Skill::Storm: case party::Skill::Smoke: want = fighting; break;
      case party::Skill::EagleEye: want = fighting && g.actors[(size_t)ti].hp > g.actors[(size_t)ti].maxHp * 0.5f; break;
      case party::Skill::Firebolt: case party::Skill::Flank: case party::Skill::Assassinate: case party::Skill::PinDown: want = fighting; break;
      case party::Skill::Ward: want = fighting; break;
      case party::Skill::Purify: case party::Skill::SecondWind: case party::Skill::Guard: want = true; break;
      case party::Skill::Fetch: want = !fighting; break;
      default: break;
    }
    if (want && useSkill(g, a, m, k, false)) return;   // one a step
  }
}

// ---------------------------------------------------------------- the fight
bool PartyOps::fight(Game& g, Actor& a, Member& m, int mi, float dt) {
  (void)mi;
  const Actor& p = g.pl();
  if (g.homeRiding()) { m.target = -1; return false; }   // on the road on horseback: they ride on
  a.thinkT -= dt;
  if (a.thinkT <= 0) { a.thinkT = 0.3f; m.target = pickFoe(g, a, m); }
  // the hound's Guard: stands over the downed (or a hurt player), drawing what comes near
  if (m.voc == Vocation::Hound && m.guardT > 0) {
    int over = -1;
    for (size_t k = 0; k < g.party.members.size() && over < 0; k++)
      if (g.party.members[k].state == State::Downed) over = actorOf(g, (int)k);
    const Vec2 spot = over > 0 ? g.actors[(size_t)over].p + Vec2(10, 2) : p.p + Vec2(10, 2);
    for (size_t i = 1; i < g.actors.size(); i++) {
      Actor& e = g.actors[i];
      if (e.hostile && alive(e) && !e.boss && len2(e.p - spot) < (3.0f * T) * (3.0f * T)) { e.target = a.id; e.thinkT = std::max(e.thinkT, 1.0f); }
    }
    if (len(spot - a.p) > 0.8f * T) { moveTo(g, a, m, spot, a.speed * 1.3f, dt); a.st = AState::Walk; return true; }
  }
  const int ti = m.target >= 0 ? g.findActor(m.target) : -1;
  if (ti <= 0) { m.target = -1; return false; }
  Actor& t = g.actors[(size_t)ti];
  if (!alive(t) || !t.hostile) { m.target = -1; return false; }
  const Vec2 d = t.p - a.p;
  const float l = len(d);
  const bool defend = m.stance == party::Cmd::Defend;
  // badly hurt (and no potion to drink): they fall back behind the player to catch their breath (danger feedback; the
  // foes go for whoever stands in front: partyAggro), striking only what corners them
  if (a.hp < a.maxHp * 0.35f && m.lastStandT <= 0 && m.stance != party::Cmd::Attack && l > a.range + t.radius) {
    const Vec2 behind = p.p + norm(p.p - t.p + Vec2(0.0001f, 0)) * (1.6f * T);
    if (len(behind - a.p) > 0.6f * T) { moveTo(g, a, m, behind, std::max(a.speed * 1.2f, 64.0f), dt); a.st = AState::Walk; }
    else { a.st = AState::Idle; a.face = faceOf(d); }
    return true;
  }
  // Defend: never more than 3 tiles from the player (intercept what comes, then back to their side)
  if (defend) {
    const float fromP = len(a.p - p.p);
    g.party.stats.defendMaxTiles = std::max(g.party.stats.defendMaxTiles, fromP / T);
    const Vec2 guard = p.p + norm(t.p - p.p + Vec2(0.0001f, 0)) * std::min(1.6f * T, len(t.p - p.p) * 0.5f);
    const float reachD = a.range + t.radius + (ranged(m) ? 4.0f * T : 2.0f);
    // (the turn back starts well inside the leash: round trunks and boulders the way back is longer than the line, and
    // a player running on gains on them while they go round)
    if (fromP > 2.2f * T || (l > reachD && len(guard - a.p) > 0.5f * T)) {
      // (fixer M8 r1) faster than the player runs, so a player chasing a foe never outruns the leash
      // (M7c merge, --companion seeds 12 / 17) near the leash's end with a trunk between (the straight way back shut,
      // defendGoal's detour round it) they sprint: a detour at 1.4x the player's pace gained too little on a player
      // running on, and the member strayed past 3 tiles in a birch wood
      const float urge = fromP > 2.6f * T && !straightWay(g, a.p, guard, a.radius) ? 1.8f : fromP > 2.2f * T ? 1.4f : 1.12f;
      const float sp = std::max(std::max(a.speed * 1.3f, 70.0f), p.speed * urge);
      moveTo(g, a, m, defendGoal(g, a, guard), sp, dt);
      a.st = AState::Walk;
      return true;
    }
  }
  // an archer or a mage with a foe at their throat draws the knife (or swings the staff): no shot at that range
  // (a foe faster than they are cannot be outrun: they stand and fight it from three tiles in)
  m.closeIn = ranged(m) && t.target == a.id && (l < 2.0f * T || (t.speed > a.speed && l < 3.0f * T));
  if (ranged(m) && !m.closeIn) {
    const float lo = 3.0f * T, hi = (m.voc == Vocation::Mage ? 5.0f : 6.0f) * T;
    const bool clear = lineClear(g, a.p + Vec2(0, -6), t.p + Vec2(0, -6));
    const float fromP = len(a.p - p.p);
    if (fromP > 8.0f * T && !defend) {   // never wandering off from the player while they shoot
      moveTo(g, a, m, p.p, std::max(a.speed * 1.2f, 64.0f), dt);
      a.st = AState::Walk;
      return true;
    }
    if (l < lo && !defend) {   // too close: step back (the bandit-archer kiting), sideways when the way back is shut
      Vec2 away = norm(a.p - t.p + Vec2(0.0001f, 0));
      m.kiteT += dt;
      const Vec2 before = a.p;
      g.moveActor(a, away * (a.speed * 0.8f * dt));
      if (len2(a.p - before) < 0.01f) {
        const Vec2 side(-away.y * (float)m.orbit, away.x * (float)m.orbit);
        g.moveActor(a, side * (a.speed * 0.8f * dt));
        if (len2(a.p - before) < 0.01f) m.orbit = -m.orbit;
      }
      a.face = faceOf(d);
      a.st = AState::Walk;
      return true;
    }
    if ((l > hi || !clear) && !defend) {
      moveTo(g, a, m, t.p, a.speed, dt);
      a.st = AState::Walk;
      return true;
    }
    if (a.atkCd <= 0 && clear && l < hi + 2.0f * T) {
      a.aim = norm(d); a.face = faceOf(d);
      a.st = AState::Windup; a.stT = 0;
      a.atkCd = m.voc == Vocation::Mage ? 1.5f : 1.25f;
      if (m.voc == Vocation::Archer && l / T < g.party.stats.archerMinTiles) g.party.stats.archerMinTiles = l / T;   // (where they chose to loose)
    } else { a.st = AState::Idle; a.face = faceOf(d); }
    return true;
  }
  // melee: the rogue circles to the back first
  const float want = (m.closeIn ? 15.0f : a.range) + t.radius - 2.0f;
  if (m.voc == Vocation::Rogue && l < 3.0f * T && t.target != a.id) {
    const Vec2 tf = norm(t.aim + Vec2(0.0001f, 0));
    const Vec2 back = t.p - tf * (t.radius + a.radius + 3.0f);
    const bool behind = dot(norm(a.p - t.p + Vec2(0.0001f, 0)), tf) < -0.2f;
    m.flankT += dt;
    if (!behind && m.flankT < 1.6f) {
      // around the foe, not through it: a step along the circle toward the back
      const Vec2 rel = a.p - t.p;
      const float ang = std::atan2(rel.y, rel.x) + (float)m.orbit * 2.4f * dt;
      const Vec2 on = t.p + Vec2(std::cos(ang), std::sin(ang)) * std::max(want + 4.0f, 18.0f);
      const Vec2 goal = len(back - a.p) < 10.0f ? back : on;
      const Vec2 before = a.p;
      g.moveActor(a, norm(goal - a.p + Vec2(0.0001f, 0)) * std::min(a.speed * 1.2f * dt, len(goal - a.p)));
      if (len2(a.p - before) < 0.01f) m.orbit = -m.orbit;
      a.face = faceOf(d);
      a.st = AState::Walk;
      return true;
    }
  } else m.flankT = 0;
  if (l > want) {
    Vec2 goal = t.p;
    if (defend) {   // on the leash: never more than 2.1 tiles out from the player (inside the turn back above)
      const Vec2 off = t.p - p.p;
      const float ol = len(off);
      if (ol > 2.1f * T) goal = p.p + off * (2.1f * T / ol);
      goal = defendGoal(g, a, goal);
    }
    moveTo(g, a, m, goal, a.speed * 1.15f, dt);
    a.st = AState::Walk;
  } else if (a.atkCd <= 0) {
    a.aim = norm(d); a.face = faceOf(d);
    a.st = AState::Windup; a.stT = 0;
    a.atkCd = m.voc == Vocation::Hound ? 0.8f : (m.voc == Vocation::Rogue ? 0.85f : 1.0f);
    m.flankT = 0;
    if (defend && len(t.p - p.p) < 3.0f * T) g.party.stats.intercepts++;
  } else {
    a.st = AState::Idle; a.face = faceOf(d);
  }
  return true;
}
