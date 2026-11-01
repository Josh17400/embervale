// EMBERVALE combat (SOLID 3.5, moved whole from game.cpp): the melee blow, damage() and its modifier chain, the
// death (kill), experience and levels, the hero's arrow, the projectile step and the pickups. Neighbours: game.cpp
// (update, the player, movement, spawning), ai.cpp / ai_beasts.cpp (who swings when), magic_game.cpp (spells).
#include "rpg/sim/game.h"
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>
#include "engine/sound_ids.h"
#include "rpg/sim/anim.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/gear_look.h"
#include "rpg/sim/home.h"
#include "rpg/sim/power_ops.h"
#include "rpg/sim/magic_ops.h"
#include "rpg/sim/lord_ops.h"
#include "rpg/sim/sea_ops.h"
#include "rpg/sim/jobs_ops.h"
#include "rpg/sim/service_ops.h"
#include "rpg/sim/powers.h"
#include "rpg/sim/skill_ops.h"
#include "rpg/sim/stream.h"
#include "rpg/sim/war.h"
#include "rpg/story/story.h"
#include "rpg/culture/society.h"
#include "rpg/sim/life_hooks.h"
#include "rpg/sim/foe_hooks.h"
#include "rpg/sim/loot_ops.h"
#include "rpg/sim/war_ops.h"

using art::Monster;
using art::Prop;

void Game::meleeHit(Actor& a) {
  bool isPl = a.player;
  float reach = isPl ? (a.combo == 2 ? 24.0f : 20.0f) : a.range;
  float cone = isPl ? (a.combo == 2 ? -0.1f : 0.25f) : 0.2f;
  float dmg = isPl ? weaponDamage() * (a.combo == 2 ? 1.7f : 1.0f) : a.dmg;
  if (isPl && AnimOps::heroGreatsword(*this)) dmg *= kGreatswordPace;   // (P2a ANIM) its own slower pace: DPS unchanged
  if (isPl && eqWeapon < 0) {   // bare fists: short reach, quick jabs, a finisher that staggers
    reach -= 4;
    dmg *= a.combo == 2 ? 1.0f : 1.2f;
  }
  Ench ench = Ench::None; float ep = 0;
  if (isPl && eqWeapon >= 0) { ench = inv[eqWeapon].ench; ep = inv[eqWeapon].enchPow; }
  if (isPl && inv.size() && eqWeapon >= 0 && inv[eqWeapon].sub == (uint8_t)WeaponType::Dagger) reach -= 3;
  // (M9) the weapon family (SkillOps: its reach, its vs-tags, the perks; the skill's gain)
  const skills::Skill fam = skills::familyOf(isPl && eqWeapon >= 0 && eqWeapon < (int)inv.size() ? &inv[(size_t)eqWeapon] : nullptr);
  if (isPl) reach += SkillOps::reachAdd(*this);
  if (isPl) SkillOps::swingShape(*this, a.combo == 2, reach, cone);   // (M9) WHIRLWIND, CLEAVE, IMPALE
  Vec2 origin = a.p + Vec2(0, -4);
  bool any = false;
  for (size_t i = 0; i < actors.size(); i++) {
    Actor& v = actors[i];
    if (v.id == a.id || v.st == AState::Dead) continue;
    if (v.fly) continue;   // can't reach a dragon in the air
    // monsters hit townsfolk too (M0); (M4) soldiers and guards hit their kingdom's enemies, and whoever is hostile to
    // the player hits the player (warFoes)
    bool enemy = isPl ? v.hostile : warFoes(*this, a, v);
    if (!enemy) continue;
    Vec2 d = (v.p + Vec2(0, -4)) - origin;
    float l = len(d);
    if (l > reach + v.radius) continue;
    if (l > 4 && dot(d * (1.0f / l), a.aim) < cone) continue;
    // (M6) crits: 12 % for x1.8 plus the affixes; Stillness makes the hit after a second standing still a sure crit
    // (M8b, 15.27.5 "companions use the same rules") a companion's blows crit by the player's rule and their own gear
    const gear::GearStats* WS = isPl ? &craft.live.stats : a.comp ? PowerOps::statsOf(*this, a) : nullptr;
    const gear::GearStats& S = WS ? *WS : craft.live.stats;
    bool crit = WS && rng_.f() < gear::CRIT_CHANCE + 0.01f * (float)S.get(Affix::CritChance);
    if (isPl && S.has(Unique::StillCrit) && craft.live.stillT >= 1.0f) { crit = true; craft.live.stillT = 0; emit(Ev::Text, v.p + Vec2(0, -30), (int)rgba(200, 230, 255), 0, "STILLNESS"); }
    if (isPl) {   // (M9) the victim before the blow (unaware, mid-rush), a sure crit (a dagger on the unaware, RIPOSTE)
      SkillOps::preStrike(*this, v, fam);
      if (SkillOps::forceCrit(*this, v, fam)) crit = true;
    }
    const float cm = gear::CRIT_MULT + 0.01f * (float)S.get(Affix::CritDmg);
    const float hit = dmg * (0.9f + rng_.f() * 0.2f) * (crit ? cm : 1.0f) * (isPl ? SkillOps::strikeMul(*this, v, fam, a.combo == 2) : 1.0f);
    const int vid = v.id;
    const float hp0 = v.hp;
    damage(v, hit, a.p, a.id, ench, ep, crit);
    any = true;
    if (isPl) {   // (M9) the family's gain and its perks' after-effects (v may have moved: look it up again)
      const int k = findActor(vid);
      if (k >= 0) { Actor& t = actors[(size_t)k]; SkillOps::struck(*this, t, fam, hp0 - std::max(0.0f, t.hp), crit, a.combo == 2, t.st == AState::Dead); }
    }
    if (isPl) {
      // (M6 uniques) the blow's echoes: Stormcaller's lightning on a crit, Echo's second strike, Thunderclap's stun
      // ((M8b) a power's own damage: no procs from it)
      powDepth_++;
      auto alive = [&](int id) -> Actor* { const int k = findActor(id); return k >= 0 && actors[(size_t)k].st != AState::Dead ? &actors[(size_t)k] : nullptr; };
      if (crit && S.has(Unique::Stormcaller)) if (Actor* t = alive(vid)) {
        emit(Ev::Sparkle, t->p + Vec2(0, -14)); emit(Ev::Shake, t->p, 0, 2.0f);
        emit(Ev::Text, t->p + Vec2(0, -30), (int)rgba(170, 210, 255), 0, "STORM");
        damage(*t, hit * 0.5f, a.p, a.id, Ench::None, 0, false);
      }
      if (S.has(Unique::Echo) && rng_.f() < 0.15f) if (Actor* t = alive(vid)) {
        emit(Ev::Text, t->p + Vec2(0, -30), (int)rgba(220, 200, 255), 0, "ECHO");
        damage(*t, hit, a.p, a.id, ench, ep, false);
      }
      if (a.combo == 2 && S.has(Unique::Thunderclap)) if (Actor* t = alive(vid)) if (!t->boss) {
        t->st = AState::Hurt; t->stT = -0.85f; t->heavy = t->lunge = false; t->atkCd = std::max(t->atkCd, 1.0f);
        emit(Ev::Text, t->p + Vec2(0, -30), (int)rgba(255, 230, 120), 0, "STUNNED");
      }
      powDepth_--;
    }
  }
  if (isPl && any) {
    // (P2a ANIM delight, STYLE_P2 12) a finisher with a greatsword, a mace or a hammer lands with an impact flash on the
    // blow's point and a harder knock of the camera
    const art::Family wf = art::familyOf(a.look);
    const bool heavyFin = a.combo == 2 && (wf == art::Family::Greatsword || wf == art::Family::Mace);
    if (heavyFin) emit(Ev::Hit, a.p + a.aim * (reach * 0.7f) + Vec2(0, -6));
    emit(Ev::Shake, a.p, 0, heavyFin ? 3.6f : a.combo == 2 ? 2.5f : 1.2f);
    hitStop = a.combo == 2 ? 0.07f : 0.035f;
    // (M6) Chain Lightning: every third landed swing arcs to the two nearest other foes
    gear::Live& L6 = craft.live;
    if (L6.stats.has(Unique::ChainLightning) && ++L6.hitCount % 3 == 0) {
      std::vector<std::pair<float, int>> near;
      for (size_t i = 1; i < actors.size(); i++) {
        const Actor& e = actors[i];
        if (!e.hostile || e.st == AState::Dead || e.fly) continue;
        const float d2 = len2(e.p - a.p);
        if (d2 < 90.0f * 90.0f) near.push_back({d2, e.id});
      }
      std::sort(near.begin(), near.end());
      int n = 0;
      for (const auto& ne : near) {
        if (n >= 3) break;   // the struck foe (nearest) and two more
        const int k = findActor(ne.second);
        if (k < 0) continue;
        Actor& t = actors[(size_t)k];
        emit(Ev::Sparkle, t.p + Vec2(0, -10));
        if (n > 0) {
          emit(Ev::Text, t.p + Vec2(0, -30), (int)rgba(160, 200, 255), 0, "CHAIN");
          PowerOps::emitPower(*this, powers::Fx::Lightning, a.p + Vec2(0, -8), t.p + Vec2(0, -8), 0.7f, 0);
          powDepth_++;
          damage(t, weaponDamage() * 0.5f, a.p, a.id, Ench::None, 0, false);
          powDepth_--;
          craft.live.fx.chained++;
        }
        n++;
      }
      if (n > 1) sfx((int)Sfx::Frost, a.p, 1.6f, 0.7f);
    }
  }
  // smash pots/props? (not yet)
}

void Game::damage(Actor& v, float dmg, Vec2 from, int attacker, Ench ench, float ep, bool crit) {
  // ---- 1. void blows: the dead, the untouchable (a bout decided, a friendly story person)
  if (v.st == AState::Dead) return;
  if (story::untouchable(*this, v, attacker)) return;   // (P1b) a bout already decided, a friendly story person
  // ---- 2. the perfect roll; then the hero's grace (iframes, god mode) and the rider knocked off
  if (v.player && v.st == AState::Roll && v.iframes > 0 && attacker >= 0 && !perfectRoll_) {
    // rolled through a blow that would have landed: a slow-motion blip and a little stamina back
    perfectRoll_ = true;
    slowMo = 0.4f;
    stamina += 12.0f;   // (updatePlayer keeps it under the worn maximum: (M8b) the Stamina affix raises it)
    emit(Ev::Sparkle, v.p + Vec2(0, -8));
    emit(Ev::Text, v.p + Vec2(0, -22), (int)rgba(150, 220, 255), 0, "DODGE");
    sfx((int)Sfx::Roll, v.p, 1.6f);
    PowerOps::dodged(*this);   // (M8b) Chronos: a perfect dodge at the roll's start slows time
    SkillOps::dodged(*this);   // (M9) light armour's EVASION and MATCHLESS
  }
  if (v.player && (v.iframes > 0 || godMode)) return;
  if (v.player && homeRiding() && !SkillOps::keepSaddle(*this, dmg)) homeDismount(home::Dismount::Hurt);   // M7: a blow knocks the rider off ((M9) SURE SEAT)
  // ---- 3. bones on the ground are smashed outright
  if (v.st == AState::Down && !v.comp) {   // smashing the bones before they rise (M8: a downed companion is the lane's)
    v.reassembled = true;
    v.hp = 0;
    emit(Ev::Hit, v.p + Vec2(0, -4), 0, dmg);
    sfx((int)Sfx::HitHeavy, v.p, 1.3f);
    kill(v, attacker);
    return;
  }
  // ---- 4. townsfolk and guards are hurt only by their enemies
  if (v.npc && !v.hostile) {   // townsfolk and guards are only hurt by their enemies (never by the player's swings)
    int ai = attacker >= 0 ? findActor(attacker) : -1;
    if (ai < 0 || !warFoes(*this, actors[ai], v)) return;   // (M4: a kingdom's enemies too)
  }
  // ---- 5. who struck whom: the levels, the striker's and the victim's worn gear
  // (M6 NUMBERS, VISION_PLAN 7.2 / 7.4) who struck whom: the level gap both ways, mitigation by the attacker's level,
  // resistances, the gear's affixes and unique powers
  const int atkI = attacker >= 0 ? findActor(attacker) : -1;
  const int atkLevel = atkI == 0 || atkI < 0 ? std::max(1, plLevel) : actors[(size_t)atkI].level;   // (the hero's Actor::level is not kept)
  const gear::GearStats& S6 = craft.live.stats;
  const bool byPl = attacker == pl().id && !v.player;
  // (M8b, 15.27.5) the striker's worn gear: the hero's, or a companion's (the same rules); nullptr for anyone else
  const gear::GearStats* AS = byPl ? &S6 : (atkI > 0 && !v.player && !v.comp && actors[(size_t)atkI].comp) ? PowerOps::statsOf(*this, actors[(size_t)atkI]) : nullptr;
  // (M8b) a companion struck: their own resistances
  const gear::GearStats* VS = v.comp ? PowerOps::statsOf(*this, v) : nullptr;
  float elemental = 0;   // the gear's elemental share of this blow (fire / frost / shadow damage affixes)
  const bool undead = !v.human && (v.mon == Monster::Skeleton || v.mon == Monster::Draugr || v.mon == Monster::Wraith);
  const bool beast = !v.human && !undead && v.mon != Monster::Dragon && v.mon != Monster::Golem && v.mon != Monster::Wisp;
  // ---- 6. the striker's gear: the level gap, the uniques, the slayer affixes, the war cry, the elemental share
  if (AS) {
    const gear::GearStats& A = *AS;
    const Actor& wearer = byPl ? pl() : actors[(size_t)atkI];
    if (byPl) dmg *= gear::gapPlayerDamage(plLevel, v.level);
    if (A.has(Unique::Executioner) && v.hp < v.maxHp * 0.25f) dmg *= 1.5f;
    if (A.has(Unique::Berserker)) dmg *= 1.0f + 0.02f * std::floor(10.0f * (1.0f - wearer.hp / std::max(1.0f, wearer.maxHp)));
    if (A.has(Unique::Huntsman) && beast) dmg *= 1.3f;
    if (A.has(Unique::Gravebane) && undead) dmg *= 1.3f;
    if (A.has(Unique::Dragonbane) && (v.mon == Monster::Dragon || v.rank >= 5)) dmg *= 1.4f;
    // (M8b) the plain-word slayer affixes: "+12% DAMAGE VS BEASTS / UNDEAD / PEOPLE" (people: bandits, soldiers, raiders)
    int vs = 0;
    if (beast) vs += A.get(Affix::VsBeasts);
    if (undead) vs += A.get(Affix::VsUndead);
    if (v.human) vs += A.get(Affix::VsPeople);
    if (vs > 0) dmg *= 1.0f + 0.01f * (float)vs;
    if (byPl && craft.live.warcryT > 0) dmg *= 1.15f;
    elemental = 0.01f * (float)(A.get(Affix::FireDmg) + A.get(Affix::FrostDmg) + A.get(Affix::ShadowDmg));
  }
  // ---- 7. mitigation and resistances: the hero's armour, a foe's armour, a companion's resistances
  if (v.player) {
    // mitigation AR / (AR + 10 attacker level + 30), capped at 70 %; a foe far above the hero hits harder
    dmg *= (1.0f - gear::mitigation(armorRating(), atkLevel)) * gear::gapEnemyDamage(plLevel, atkLevel);
    const bool shadowFoe = atkI >= 0 && actors[(size_t)atkI].mon == Monster::Wraith && !actors[(size_t)atkI].human;
    const int res = ench == Ench::Fire ? S6.get(Affix::ResFire) : ench == Ench::Frost ? S6.get(Affix::ResFrost)
                    : (ench == Ench::Drain || shadowFoe) ? S6.get(Affix::ResShadow) : 0;
    if (res > 0) dmg *= 1.0f - 0.01f * (float)std::min(res, 75);
    if (S6.has(Unique::Bulwark) && v.hp > v.maxHp * 0.8f) dmg *= 0.7f;
  } else {
    dmg *= 1.0f - gear::mitigation(v.armor, atkLevel);   // (M6) a foe's armour (1.5 D) by the same rule
    if (VS) {   // (M8b) a companion's resistances and Bulwark, as the hero's
      const bool shadowFoe = atkI >= 0 && actors[(size_t)atkI].mon == Monster::Wraith && !actors[(size_t)atkI].human;
      const int res = ench == Ench::Fire ? VS->get(Affix::ResFire) : ench == Ench::Frost ? VS->get(Affix::ResFrost)
                      : (ench == Ench::Drain || shadowFoe) ? VS->get(Affix::ResShadow) : 0;
      if (res > 0) dmg *= 1.0f - 0.01f * (float)std::min(res, 75);
      if (VS->has(Unique::Bulwark) && v.hp > v.maxHp * 0.8f) dmg *= 0.7f;
    }
  }
  // ---- 8. the lanes' multipliers: foes (M6), the party (M8), the armour skills (M9), magic's statuses (M9)
  dmg = FoeHooks::hit(*this, v, dmg, attacker);   // M6: warded / armoured / vampiric / frost-bound (foes_game.cpp)
  dmg = PartyOps::hit(*this, v, dmg, attacker);  // M8: shield wall, defend, the damage-share statistics (party_game.cpp)
  if (v.player) dmg = SkillOps::takenMul(*this, attacker, dmg);   // M9: the worn armour class, perks (skills_game.cpp)
  dmg = MagicOps::hurtMul(*this, v, dmg, attacker, ench);         // M9: statuses and wards (magic_game.cpp)
  // ---- 9. the elemental affixes and riders: burn, frost, Ember, Rime, Burn DPS, the weapon's enchantment
  if (AS && elemental > 0) {
    dmg *= 1.0f + elemental;
    if (AS->get(Affix::FireDmg) > 0) { v.burnT = std::max(v.burnT, 2.0f); v.regen = 0; }
    if (AS->get(Affix::FrostDmg) > 0) v.slowT = std::max(v.slowT, 1.5f);
  }
  if (AS && AS->has(Unique::Ember)) { v.burnT = std::max(v.burnT, 3.0f); v.regen = 0; }
  if (AS && AS->has(Unique::Rime)) { if (v.slowT < 0.1f) emit(Ev::Frost, v.p); v.slowT = std::max(v.slowT, 2.0f); }
  // (M8b) Affix::Burn: "BURNS FOES FOR N DMG/S": N damage a second for 3 s (updateActor; the strongest burn holds)
  if (AS && AS->get(Affix::Burn) > 0) {
    const float n = (float)AS->get(Affix::Burn);
    if (v.burnDpsT <= 0 || n >= v.burnDps) { v.burnDps = n; v.burnBy = attacker; }
    v.burnDpsT = powers::BURN_SECS;
    v.regen = 0;
  }
  if (ench == Ench::Fire) { v.burnT = 3.0f; v.regen = 0; dmg += ep * 0.4f; }
  if (ench == Ench::Frost) { v.slowT = 3.0f; dmg += ep * 0.5f; emit(Ev::Frost, v.p); }
  if (ench == Ench::Drain && attacker == pl().id) { pl().hp = std::min(pl().maxHp, pl().hp + ep * 0.5f); dmg += ep * 0.3f; }
  // ---- 10. life drain and Soulfeast
  if (AS && (AS->get(Affix::LifeDrain) > 0 || AS->has(Unique::Soulfeast))) {
    // life drain (N % of the blow) and (M8b) Soulfeast's 6 %: the striker heals (a companion heals themselves)
    Actor& w = byPl ? pl() : actors[(size_t)atkI];
    const float pct = (float)AS->get(Affix::LifeDrain) + (AS->has(Unique::Soulfeast) ? powers::SOULFEAST_PCT : 0.0f);
    const float before = w.hp;
    w.hp = std::min(w.maxHp, w.hp + dmg * 0.01f * pct);
    if (AS->has(Unique::Soulfeast) && w.hp > before) {
      craft.live.fx.healed += w.hp - before;
      if (time - craft.live.fx.lastHealFx > 0.25f || craft.live.fx.lastHealFx > time) {   // (one mote stream a quarter second)
        craft.live.fx.lastHealFx = time;
        PowerOps::emitPower(*this, powers::Fx::Lifesteal, v.p + Vec2(0, -8), w.p + Vec2(0, -8), std::min(1.5f, (w.hp - before) / 6.0f), (uint32_t)w.id);
      }
    }
  }
  // ---- 11. the Ward soaks first
  if (v.player && craft.live.ward > 0) {   // Ward: the shield takes the blow first
    const float soak = std::min(craft.live.ward, dmg);
    craft.live.ward -= soak;
    dmg -= soak;
    emit(Ev::Sparkle, v.p + Vec2(0, -8));
  }
  // ---- 12. thorns and Thornmail
  if (v.player && atkI > 0 && dmg > 0) {
    // thorns and Thornmail: a melee attacker takes part of its blow back (never recursing into damage())
    Actor& at = actors[(size_t)atkI];
    const float share = 0.01f * (float)S6.get(Affix::Thorns) + (S6.has(Unique::Thornmail) ? 0.25f : 0.0f);
    if (share > 0 && at.st != AState::Dead && len2(at.p - v.p) < 40.0f * 40.0f) {
      const float back = dmg * share;
      at.hp -= back;
      at.flash = 0.12f;
      emit(Ev::Text, at.p + Vec2(0, -18), (int)rgba(200, 255, 160), back, std::to_string((int)std::ceil(back)));
      if (at.hp <= 0) kill(at, pl().id);
    }
  }
  // ---- 13. the blow lands (a bout's yield first); the skills' and the caster's hooks
  story::boutBlow(*this, v, dmg, attacker);   // (P1b) a bout: the yield at a quarter health, the player kept on 1 HP
  v.hp -= dmg;
  if (v.player) SkillOps::taken(*this, attacker, dmg);   // (M9) the armour skill's gain, Block's
  MagicOps::hurt(*this, v, dmg, attacker);              // (M9) a heavy hit breaks the caster's concentration
  // ---- 14. the hero's on-struck uniques: Frost Nova, Time Slow, Second Wind
  if (v.player && v.hp > 0) {
    // (M6 uniques) when the hero is struck: Frost Nova, Time Slow below 30 %, Second Wind below 25 %
    gear::Live& L6 = craft.live;
    if (S6.has(Unique::FrostNova) && L6.frostNovaCd <= 0) {
      L6.frostNovaCd = 20.0f;
      emit(Ev::Frost, v.p);
      emit(Ev::Text, v.p + Vec2(0, -30), (int)rgba(170, 220, 255), 0, "FROST NOVA");
      for (Actor& e : actors) if (e.hostile && e.st != AState::Dead && len2(e.p - v.p) < 56.0f * 56.0f) { e.slowT = std::max(e.slowT, 3.0f); emit(Ev::Frost, e.p); }
    }
    if (S6.has(Unique::TimeSlow) && L6.timeSlowCd <= 0 && v.hp < v.maxHp * 0.3f) {
      L6.timeSlowCd = 60.0f;
      slowMo = std::max(slowMo, 2.0f);
      emit(Ev::Text, v.p + Vec2(0, -30), (int)rgba(220, 200, 255), 0, "TIME SLOWS");
    }
    if (S6.has(Unique::SecondWind) && L6.secondWindCd <= 0 && v.hp < v.maxHp * 0.25f) {
      L6.secondWindCd = 90.0f;
      stamina = std::max(stamina, maxSt);
      emit(Ev::Text, v.p + Vec2(0, -38), (int)rgba(160, 255, 160), 0, "SECOND WIND");
    }
  }
  // ---- 15. the feedback: the flash, the knockback, the numbers, blood, the sounds
  v.flash = 0.12f;
  v.lastHitT = time;
  if (attacker == pl().id) v.plHitT = time;
  if (v.player) lastHurtT = time;
  Vec2 dir = norm(v.p - from);
  float kb = v.boss ? 30.0f : (v.player ? 110.0f : 150.0f);
  if (v.mon == Monster::Dragon) kb = 0;
  if (v.player) kb *= SkillOps::knockMul(*this);   // (M9) heavy armour's IMMOVABLE
  v.knock = dir * kb;
  emit(Ev::Text, v.p + Vec2(0, -18), v.player ? (int)rgba(255, 90, 80) : crit ? (int)rgba(255, 210, 60) : (int)rgba(255, 255, 255), dmg,
       std::to_string((int)std::ceil(dmg)) + (crit ? "!" : ""));
  if (crit) emit(Ev::Shake, v.p, 0, 3.0f);
  emit(Ev::Hit, v.p + Vec2(0, -6), 0, dmg);
  if (v.powTag != powers::PT_WOLF &&   // ((M8b) a spectral wolf does not bleed)
      (v.human || v.mon == Monster::Wolf || v.mon == Monster::Bear || v.mon == Monster::Boar || v.mon == Monster::Troll || v.mon == Monster::IceWolf || v.mon == Monster::Dragon ||
       v.mon == Monster::Hyena || v.mon == Monster::Lurker || v.mon == Monster::Yeti || v.mon == Monster::EmberHound))
    emit(Ev::Blood, v.p + Vec2(0, -6));
  if (v.player) { sfx((int)Sfx::PlayerHurt, v.p); emit(Ev::Shake, v.p, 0, 3); v.iframes = 0.35f; }
  else sfx((int)(dmg > 18 ? Sfx::HitHeavy : Sfx::Hit), v.p, 0.9f + rng_.f() * 0.2f);
  // ---- 16. the victim reacts: aggro, the stagger, nerve (flight); the hero staggers
  if (!v.player) {
    v.aggro = true;
    v.target = attacker;
    // a committed lunge or heavy slam can't be interrupted by a light hit: roll, don't trade
    bool committed = v.st == AState::Strike || (v.st == AState::Windup && (v.heavy || v.lunge) && dmg < v.maxHp * 0.3f);
    // the wolf-like pack hunters (wolves, ice wolves, M3c hyenas and ember hounds) share the wolves' nerve rules
    const bool wolfLike = !v.human && (v.mon == Monster::Wolf || v.mon == Monster::IceWolf || v.mon == Monster::Hyena || v.mon == Monster::EmberHound);
    if (wolfLike && dmg < v.maxHp * 0.4f) committed = true;   // wolves shrug off light cuts
    if (!v.boss && dmg > v.maxHp * 0.12f && !committed) { v.st = AState::Hurt; v.stT = 0; v.heavy = false; v.lunge = false; }
    // goblins lose their nerve when badly hurt; so does the last wolf of a pack
    bool lastWolf = false;
    // (inside a settlement nothing flees: whatever got in fights until the guards end it, as the leash rule says)
    const bool inTownV = !inside && settlementAt(v.p) >= 0;
    if (wolfLike && v.hp > 0 && v.hp < v.maxHp * 0.4f && !inTownV) {
      lastWolf = true;
      for (const Actor& o : actors) if (o.id != v.id && o.mon == v.mon && !o.human && o.hostile && o.st != AState::Dead && len2(o.p - v.p) < 160 * 160) lastWolf = false;
    }
    if (((v.mon == Monster::Goblin && v.hp < v.maxHp * 0.3f && !inTownV) || lastWolf) && !v.human && !v.fleeing && v.hp > 0) {
      v.fleeing = true;
      emit(Ev::Text, v.p + Vec2(0, -20), (int)rgba(255, 230, 120), 0, "!");
    }
  } else if (v.st != AState::Roll && dmg > 8 && !SkillOps::unstaggerable(*this, dmg)) { v.st = AState::Hurt; v.stT = 0; }   // ((M9) STEADFAST, IMMOVABLE, STEADY)
  // ---- 17. the death, then the striker's on-hit powers
  // (M8b) the striker's on-hit powers (Tempest, Starfall), never from a power's own damage; read before the kill (the
  // victim's place and id)
  const bool procs = AS && powDepth_ == 0 && v.hostile && dmg > 0;
  const int vid = v.id;
  const Vec2 vat = v.p;
  if (v.hp <= 0) kill(v, attacker);
  if (procs) PowerOps::afterHit(*this, attacker, vid, vat, dmg, crit);
}

void Game::kill(Actor& a, int killer) {
  if (a.player && PowerOps::phoenix(*this)) return;   // (M8b) Phoenix: a fatal blow leaves the hero at 30 % instead
  if (a.powTag == powers::PT_WOLF) {   // (M8b) a spectral wolf fades: no body, no loot, no kill, no deeds, no quests
    a.hp = 0; a.st = AState::Dead; a.stT = 99.0f;
    PowerOps::emitPower(*this, powers::Fx::Wolf, a.p, a.p, 0.0f, (uint32_t)a.id);
    return;
  }
  // (M8b fixer r2) a spectral wolf's kill is its summoner's: XP, the kill count, den clears, hunts and saga Kill goals
  // credit the hero (or the companion) who wears the power. The powers themselves still see the wolf as the striker
  // (PowerOps::onKill below), so a wolf's kill never calls more wolves or feeds Soulfeast.
  const int striker = killer;
  if (killer >= 0) {
    const int ki = findActor(killer);
    if (ki >= 0 && actors[(size_t)ki].powTag == powers::PT_WOLF && actors[(size_t)ki].powBy) killer = (int)actors[(size_t)ki].powBy;
  }
  if (a.comp && PartyOps::fell(*this, a, killer)) return;   // M8: a companion is downed, not dead, until the lane says (party_game.cpp)
  if (a.mon == Monster::Skeleton && !a.human && !a.boss && !a.reassembled && a.st != AState::Down && rng_.f() < 0.4f) {
    // the bones collapse... and may stand back up (see updateActor). Another blow while it is down finishes it.
    a.reassembled = true;
    a.hp = 1;
    a.st = AState::Down; a.stT = 0;
    a.heavy = a.lunge = false;
    sfx((int)Sfx::EnemyDie, a.p, 1.4f);
    emit(Ev::Dust, a.p);
    return;
  }
  a.hp = 0;
  a.st = AState::Dead;
  a.stT = 0;
  a.fly = false;
  if (a.player) {
    mode = Mode::Dead;
    sfx((int)Sfx::Death, a.p);
    return;
  }
  sfx((int)Sfx::EnemyDie, a.p, a.boss ? 0.7f : 1.0f);
  questActorDown(a);   // M2: a farmer who asked for help, a missing person (quests.cpp)
  // only the player's own blows, arrows and fire earn XP, the kill count and hunt progress: a wolf the town guard
  // cuts down is not the player's kill
  const bool byPlayer = killer == pl().id;
  if (byPlayer) {
    // (M6, 7.4) XP decay: a foe far below the hero is worth little (Actor::xp already holds base x (1 + 0.12 (Le - 1)))
    if (a.xp > 0) gainXp(std::max(1, (int)std::lround((float)a.xp * gear::xpDecay(plLevel, a.level) * gear::earlyXpMul(plLevel))));
    kills++;
    const gear::GearStats& S6 = craft.live.stats;
    if (S6.has(Unique::Vampire)) { pl().hp = std::min(pl().maxHp, pl().hp + pl().maxHp * 0.08f); emit(Ev::Heal, pl().p); }
    if (S6.has(Unique::Warcry)) { if (craft.live.warcryT <= 0) emit(Ev::Text, pl().p + Vec2(0, -30), (int)rgba(255, 150, 90), 0, "WARCRY"); craft.live.warcryT = 6.0f; }
  }
  PowerOps::onKill(*this, a, striker);   // (M8b) Soulfeast's kill heal, Wolfking's call (the hero's and a companion's)
  // townsfolk felled by monsters are back on their feet when the town next loads (only enemies stay dead)
  if (a.fromMap && !a.npc) killedSlots[mapKey()].insert(!inside && a.site >= 0 ? owKillKey(a.site, a.slot) : a.slot);
  if (a.fromMap && a.npc && !inside && a.site >= 0 && a.slot >= 0) felled_[a.site].insert(a.slot);   // not back this visit
  if (a.npc && !inside) emit(Ev::Text, a.p + Vec2(0, -20), (int)rgba(255, 120, 100), 0, "DOWN");
  if (a.den >= 0 && !inside && a.den < (int)world.dens.size()) {
    bool left = false;
    for (const Actor& o : actors) if (o.id != a.id && o.den == a.den && o.st != AState::Dead) left = true;
    if (!left && denClearedDay(a.den) < 0) {
      // the den is cleared: a small reward, and it stays empty for a few days (killedSlots[-1] = den * 4096 + day)
      killedSlots[-1].insert(a.den * 4096 + std::min(day, 4095));
      int bonus = 12 + world.zoneLevel(world.dens[a.den].x, world.dens[a.den].y) * 4;
      if (byPlayer) {
        emit(Ev::Notice, a.p, (int)rgba(255, 210, 90), 0, "DEN CLEARED  +" + std::to_string(bonus) + " XP");
        sfx((int)Sfx::QuestDone, a.p, 1.15f);
        gainXp(bonus);
      }
    }
  }
  PartyOps::killed(*this, a, killer);   // M8: the deeds of a death, a companion's kill (party_game.cpp)
  FoeHooks::killed(*this, a, killer);   // M6: splitting, volatile, named beasts and world bosses slain (foes_game.cpp; queues new actors)
  LootOps::dropLoot(*this, a);
  // (M4 integration) a hunt beast the player was fighting still counts when a guard or militia lands the last blow
  // (WARDS guards now fight beasts in the streets); XP and the kill count stay the player's own
  // (fixer M8 r1) a companion's kill is the party's: it counts toward hunt quests and saga Kill goals like the player's
  const bool byParty = byPlayer || (killer >= 0 && party.indexOfActor(killer) >= 0);
  if (byParty || (a.plHitT > -99 && time - a.plHitT < 8.0f)) questKill(a);
  WarOps::kill(*this, a, killer);           // M4: siege contributions, reputation (war_game.cpp)
  ServiceOps::kill(*this, a, killer);   // M12: an enemy of the sworn kingdom slain: merit (service_game.cpp)
  storyKill(a, byParty);        // M4: story objectives (rpg/story/story_game.cpp)
  LifeHooks::kill(*this, a, killer);          // M5: a resident's death: grief, the census (life_game.cpp)
  if (!inside && a.site >= 0 && a.site < (int)world.sites.size() && world.sites[a.site].type == SiteType::BanditCamp) {
    // a camp is broken when its chief falls (what the bounty asks: "KILL THEIR CHIEF"), or when its last fighter
    // does (a camp whose chief is already gone). Either way the player hears about it at once: CLEARED and BOUNTY
    // READY, or NOT YOUR TARGET, and how many stragglers still fight on.
    int left = 0;
    for (const Actor& o : actors)
      if (o.id != a.id && o.site == a.site && o.hostile && o.st != AState::Dead && !o.npc) left++;
    if (a.boss || left == 0) {
      const bool was = world.sites[a.site].cleared;
      checkDungeonCleared(a.site);
      if (!was && a.boss && left > 0)
        emit(Ev::Text, a.p + Vec2(0, -34), (int)rgba(255, 200, 120), 0,
             "CHIEF SLAIN - " + std::to_string(left) + (left == 1 ? " BANDIT FIGHTS ON" : " BANDITS FIGHT ON"));
    }
  } else if (a.boss) {
    if (inside && subSite >= 0) checkDungeonCleared(subSite);
    else if (!inside && a.site >= 0) checkDungeonCleared(a.site);
  }
}

void Game::gainXp(int xp) {
  if (xp > 0) xp = std::max(1, (int)std::lround(xp * LifeHooks::xpMul(*this)));   // M5 (15.2): Rested
  plXp += xp;
  while (plXp >= gear::xpForNext(plLevel)) {   // (M6: the tuned curve, rpg/sim/gear.cpp)
    plXp -= gear::xpForNext(plLevel);
    plLevel++;
    perkPts++;
    SkillOps::leveledUp(*this);   // (M9) a perk point to spend in the trees
    emit(Ev::LevelUp, pl().p, plLevel);
    sfx((int)Sfx::LevelUp, pl().p);
    emit(Ev::Shake, pl().p, 0, 3.0f);
    slowMo = std::max(slowMo, 0.5f);
    say("LEVEL UP! YOU ARE NOW LEVEL " + std::to_string(plLevel));
    recalcPlayer();
  }
}

void Game::chooseLevelUp(int stat) {
  if (perkPts <= 0) return;
  perkPts--;
  if (stat == 0) baseHp += 12;
  else if (stat == 1) maxMp += 12;
  else maxSt += 12;
  recalcPlayer();
  pl().hp = pl().maxHp; mp = maxMp; stamina = maxSt;
  sfx((int)Sfx::MenuSelect, pl().p);
  if (perkPts <= 0 && mode == Mode::LevelUp) mode = Mode::Play;
}

// ------------------------------------------------------------------ ranged
void Game::shootArrow() {
  Actor& p = pl();
  if (eqBow < 0) { say("NO BOW EQUIPPED"); return; }
  int ai = -1;
  for (int i = 0; i < (int)inv.size(); i++) if (inv[i].kind == ItemKind::Arrows && inv[i].count > 0) { ai = i; break; }
  if (ai < 0) { say("OUT OF ARROWS"); return; }
  if (p.shootCd > 0) return;
  // aim at the nearest hostile roughly in front
  Vec2 aim = p.aim;
  float bd = 1e9f;
  for (size_t i = 1; i < actors.size(); i++) {
    const Actor& e = actors[i];
    if (!e.hostile || e.st == AState::Dead) continue;
    Vec2 d = e.p - p.p;
    float l = len(d);
    if (l > 190 || l < 1) continue;
    float score = l * (1.6f - dot(d * (1.0f / l), p.aim));
    if (dot(d * (1.0f / l), p.aim) > 0.2f && score < bd) { bd = score; aim = norm(e.p + Vec2(0, -6) - (p.p + Vec2(0, -8))); }
  }
  inv[ai].count--;
  if (inv[ai].count <= 0) {
    inv.erase(inv.begin() + ai);
    auto fix = [&](int& e) { if (e == ai) e = -1; else if (e > ai) e--; };
    fix(eqWeapon); fix(eqBow); fix(eqStaff); fix(eqArmor); fix(eqHelmet); fix(eqShield); fix(eqRing); fix(eqAmulet);
    fix(eqGloves); fix(eqBoots); fix(eqCloak);
  }
  Projectile pr;
  pr.p = p.p + Vec2(0, -8) + aim * 6;
  pr.v = aim * 290;
  pr.dmg = gear::usePower(inv[eqBow], plLevel) * gear::playerDamageMul(plLevel);   // (M6) level sync, +1.5 % a level
  pr.ench = inv[eqBow].ench; pr.enchPow = inv[eqBow].enchPow;
  pr.fromPlayer = true; pr.owner = p.id; pr.kind = ProjKind::Arrow; pr.life = 0.9f;
  projs.push_back(pr);
  p.aim = aim; p.face = faceOf(aim);
  projs.back().from = pr.p;   // (M8b SplitArrows: where it was loosed)
  // (M8b) Affix::AttackSpeed draws the bow that much faster too
  p.st = AState::Cast; p.stT = 0;
  p.shootCd = (craft.live.stats.has(Unique::Quickdraw) ? 0.42f * 0.7f : 0.42f) / (1.0f + 0.01f * (float)std::max(0, craft.live.stats.get(Affix::AttackSpeed)));
  p.shootCd *= SkillOps::drawMul(*this);   // (M9) Archery's draw
  sfx((int)Sfx::Arrow, p.p);
  SkillOps::loosed(*this);                 // (M9) the steady-draw perk
}

// (M9) the book's selected quick slot (rpg/sim/magic_game.cpp, MAGIC lane: the classic three keep their numbers)
void Game::castSpell() { MagicOps::cast(*this); }

void Game::updateProjectiles(float dt0) {
  // (M8b) SplitArrows' new arrows join after the pass (never pushed while `pr` is held)
  std::vector<Projectile> born;
  const bool split = craft.live.stats.has(Unique::SplitArrows);
  const bool reflect = craft.live.stats.has(Unique::ReflectBlock);
  for (size_t i = 0; i < projs.size();) {
    Projectile& pr = projs[i];
    const float dt = pr.fromPlayer ? dt0 : dt0 * PowerOps::foeScale(*this);   // (M8b) Chronos slows the foes' missiles
    pr.life -= dt;
    Vec2 prev = pr.p;
    pr.p += pr.v * dt;
    // (M8b) SplitArrows: past 6 tiles the hero's arrow splits in three (a 12-degree fan; the children never split again)
    if (split && pr.kind == ProjKind::Arrow && pr.owner == pl().id && !(pr.powFlags & PJ_SPLIT) && len2(pr.from) > 0 &&
        len2(pr.p - pr.from) > powers::SPLIT_RANGE * powers::SPLIT_RANGE) {
      pr.powFlags |= PJ_SPLIT;
      for (int k = -1; k <= 1; k += 2) {
        Projectile c = pr;
        const float an = 0.21f * (float)k, cs = std::cos(an), sn = std::sin(an);
        c.v = Vec2(pr.v.x * cs - pr.v.y * sn, pr.v.x * sn + pr.v.y * cs);
        c.dmg = pr.dmg * 0.6f;
        born.push_back(c);
      }
      PowerOps::emitPower(*this, powers::Fx::Proc, pr.p, pr.p + pr.v * 0.05f, 0.5f, 0);
      craft.live.fx.procs[(int)powers::Fx::Proc]++;
    }
    bool dead = pr.life <= 0;
    // (M6 fixer) a foe's fire on the ground (an inert Fireball: still, from no player, of the Player faction; foes_game
    // dropFire) only fades: it never bursts, even when a step grows past foeStep's quiet-removal margin (slow-mo ends)
    if (dead && pr.kind == ProjKind::Fireball && !pr.fromPlayer && pr.fac == Faction::Player && pr.v.x == 0 && pr.v.y == 0) {
      projs.erase(projs.begin() + (std::ptrdiff_t)i);
      continue;
    }
    // walls stop projectiles (water and low props don't)
    const Map& m = map();
    int tx = (int)std::floor(pr.p.x / TILE), ty = (int)std::floor((pr.p.y + 8) / TILE);
    if (!dead && pr.kind != ProjKind::DragonFire) {
      Ground g = m.at(tx, ty);
      bool wall = g == Ground::Rock || g == Ground::CaveWall || g == Ground::InteriorWall || g == Ground::Void;
      if (m.in(tx, ty) && (m.bldgAt[(size_t)ty * m.w + tx] >= 0 || m.wall[(size_t)ty * m.w + tx])) wall = true;
      int pp = m.propAt(tx, ty);
      if (pp) {
        Prop p = (Prop)(pp - 1);
        // (M3c) every tree, and the tall Wildlands rocks, stop a shot (art_props.h isTreeProp)
        if ((art::isTreeProp(p) && p != Prop::DeadTree && p != Prop::PalmTree) || p == Prop::Boulder || p == Prop::Stalagmite ||
            p == Prop::Hoodoo || p == Prop::CrystalSpire || p == Prop::BasaltColumns || p == Prop::IceSerac || p == Prop::TermiteMound) wall = true;
      }
      if (wall) dead = true;
    }
    if (!dead) {
      for (size_t k = 0; k < actors.size(); k++) {
        Actor& a = actors[k];
        if (a.st == AState::Dead || a.id == pr.owner) continue;
        bool enemy = pr.fromPlayer ? a.hostile : (a.player || a.npc) && factionsHostile(pr.fac, a.faction);
        if (!enemy) continue;
        Vec2 c = a.p + Vec2(0, a.fly ? -28.0f : -7.0f);
        float r = a.radius + pr.radius + (a.mon == Monster::Dragon ? 10 : 2);
        if (len2(c - pr.p) < r * r) {
          // (M8b) ReflectBlock: a missile met mid-roll turns back on whoever loosed it, the hero's now
          if (a.player && reflect && a.st == AState::Roll && pr.kind != ProjKind::DragonFire && !(pr.powFlags & PJ_REFLECTED)) {
            pr.v = pr.v * -1.2f;
            pr.fromPlayer = true; pr.owner = a.id; pr.fac = Faction::Player;
            pr.powFlags |= PJ_REFLECTED;
            pr.life = std::max(pr.life, 0.8f);
            pr.p = c + norm(pr.v) * (r + 1.0f);
            PowerOps::emitPower(*this, powers::Fx::Proc, c, c + norm(pr.v) * 10.0f, 1.0f, 0);
            craft.live.fx.procs[(int)powers::Fx::Proc]++;
            emit(Ev::Text, a.p + Vec2(0, -26), (int)rgba(200, 230, 255), 0, "REFLECT");
            sfx((int)Sfx::Block, a.p, 1.3f);
            break;
          }
          // (M9) a spell's missile is magic's (true: handled); a hero's arrow is Archery's (its vs-tags, its gain)
          if (pr.spell && MagicOps::projHit(*this, pr, a)) { dead = true; break; }
          const bool plArrow = pr.kind == ProjKind::Arrow && pr.owner == pl().id;
          const int vid = a.id;
          const float hp0 = a.hp;
          if (plArrow) SkillOps::preStrike(*this, a, skills::Skill::Bow);
          damage(a, pr.dmg * (plArrow ? SkillOps::strikeMul(*this, a, skills::Skill::Bow, false) : 1.0f), prev, pr.owner, pr.ench, pr.enchPow);
          if (plArrow) {
            const int k = findActor(vid);
            if (k >= 0) { Actor& t = actors[(size_t)k]; SkillOps::struck(*this, t, skills::Skill::Bow, hp0 - std::max(0.0f, t.hp), false, false, t.st == AState::Dead); }
          }
          dead = true;
          if (pr.kind == ProjKind::Arrow) sfx((int)Sfx::ArrowHit, pr.p);
          break;
        }
      }
    }
    if (dead) {
      if (pr.spell) MagicOps::projEnd(*this, pr);   // (M9) its impact, aftermath and the surface it leaves
      if (pr.kind == ProjKind::Fireball || pr.kind == ProjKind::DragonFire) {
        emit(Ev::Explode, pr.p, 0, pr.kind == ProjKind::DragonFire ? 1.5f : 1.0f);
        if (pr.kind == ProjKind::Fireball) {
          sfx((int)Sfx::Explode, pr.p, 1.0f, 0.7f);
          for (auto& a : actors)
            if (a.hostile && a.st != AState::Dead && len2(a.p - pr.p) < 22 * 22 && len2(a.p - pr.p) > 4) damage(a, pr.dmg * 0.4f, pr.p, pr.owner, Ench::Fire, pr.enchPow * 0.5f);
        } else {
          Actor& p = pl();
          if (len2(p.p - pr.p) < 20 * 20) damage(p, pr.dmg, pr.p, pr.owner, Ench::Fire, 4);
        }
      }
      if (pr.kind == ProjKind::IceSpike) emit(Ev::Frost, pr.p);
      projs.erase(projs.begin() + i);
    } else i++;
  }
  for (Projectile& b : born) projs.push_back(b);
}

void Game::updatePickups(float dt) {
  Actor& p = pl();
  for (size_t i = 0; i < pickups.size();) {
    Pickup& k = pickups[i];
    k.t += dt;
    Vec2 d = p.p - k.p;
    float l = len(d);
    auto beltFull = [&] {
      int held = 0;
      for (const Item& it : inv) if (it.kind == ItemKind::Potion) held += it.count;
      return held + k.item.count > gear::POTION_CARRY;
    };
    if (k.full) {   // (M6 fixer r2) it lies still (no chime, no pull) until the belt has room again
      if (l > 24) k.told = false;
      if (!beltFull()) { k.full = false; k.told = false; }
    }
    if (k.t > 0.5f && l < 40 && !k.full) {
      if (!k.magnet) { k.magnet = true; sfx((int)(k.gold > 0 ? Sfx::Coin : Sfx::Pickup), k.p, 1.5f + rng_.f() * 0.2f, 0.5f); }
      k.p += d * (std::min(1.0f, dt * 7.0f));
    }
    if (k.t > 0.5f && l < 9) {
      // (M6) a full potion belt leaves the bottle lying (7.5: at most 10 carried)
      if (k.gold <= 0 && k.item.kind == ItemKind::Potion && (k.full || beltFull())) {
        k.full = true;
        k.magnet = false;
        if (!k.told) { k.told = true; say("YOU CAN CARRY NO MORE THAN 10 POTIONS"); }
        if (k.t > 240) { pickups.erase(pickups.begin() + i); continue; }
        i++;
        continue;
      }
      // (M6) gold find (affixes, Golden Touch) on every coin picked up from kills and chests
      if (k.gold > 0) {
        const gear::GearStats& S6 = craft.live.stats;
        const float gf = 0.01f * (float)S6.get(Affix::GoldFind) + (S6.has(Unique::GoldenTouch) ? 0.25f : 0.0f);
        giveGold(gf > 0 ? (int)std::lround((float)k.gold * (1.0f + gf)) : k.gold);
      }
      else addItem(k.item);
      pickups.erase(pickups.begin() + i);
      continue;
    }
    if (k.t > 240) { pickups.erase(pickups.begin() + i); continue; }
    i++;
  }
}
