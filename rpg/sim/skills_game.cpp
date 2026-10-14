// M9 "Arcana & Mastery": the skills' Game hooks (SkillOps, rpg/sim/skill_ops.h). SKILLS lane.
// Every skill rises only by its own act (SkillOps::use, the one doorway, at skills::skillRate's pace), each skill-up
// gives character XP (Game::gainXp: the M8b loot gates follow), the weapon families win and lose against foe tags, the
// armour classes trade speed, the roll, stamina, noise and damage reduction, and every perk of the trees changes play.
// Trainers (M5 census jobs), skill books (M8b loot sources, scholars' shelves) and companions (M8) teach; respec is
// costly and rare (a trainer's or a priest's dialogue).
// Determinism: nothing here draws from Game::rng_. The few chances (a persuasion, a book among the loot, a spare
// ingot) hash stable facts (the seed, the day, the kill count, places), so --metrics and the goldens stay exact.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/life.h"
#include "rpg/sim/party.h"
#include "rpg/sim/skill_ops.h"
#include "rpg/sim/skills.h"
#include "rpg/sim/talk.h"

using skills::Skill;
using skills::Weight;
using namespace skills::perk;

void Game::skillUse(skills::Skill s, float base) { SkillOps::use(*this, s, base); }

namespace {
// the SKILLS options (DLG_SKILLS .. DLG_SKILLS_END)
enum SkillDlg : int {
  K_TRAIN = DLG_SKILLS + 1,   // a trainer: the skills they teach (the list)
  K_RESPEC,                   // a trainer or a priest: perks to unlearn (the list)
  K_PRICES,                   // a merchant: the persuasion page
  K_PERSUADE,                 // ... talk them into a better price
  K_INTIMIDATE,               // ... threaten them into one
  K_COMPANION,                // a companion: what they can show you (arg: members index)
  K_BACK,                     // back to the trainer's list
  K_LEARN = DLG_SKILLS + 100,     // + skill: buy a level from the trainer
  K_UNLEARN = DLG_SKILLS + 200,   // + skill: respec its perks
  K_SHOWME = DLG_SKILLS + 300,    // + skill: a companion teaches a level (arg: members index)
};
constexpr uint8_t TAG_MARK = 1;     // Bow HUNTER'S MARK: struck first by the hero's arrow
constexpr uint8_t TAG_BROKEN = 2;   // Mace ARMOUR BREAKER: armour cracked while skillT lasts
constexpr uint8_t TAG_DOZE = SkillOps::TAG_DOZE;   // a staged sentry (scripts: skill foe ... unaware) stays put while unaware

void skillEv(Game& g, Skill s, int kind, uint32_t v, const std::string& line) {
  Event e;
  e.type = Ev::Skill;
  e.p = g.pl().p;
  e.a = (int)s;
  e.f = (float)kind;
  e.spell = v;
  e.s = line;
  g.events.push_back(e);
}
bool P(const Game& g, Skill s, int i) { return g.skills.has(s, i); }
const Item* at(const Game& g, int idx) { return idx >= 0 && idx < (int)g.inv.size() ? &g.inv[(size_t)idx] : nullptr; }
Skill famOf(const Game& g) { return skills::familyOf(at(g, g.eqWeapon)); }
uint32_t mixh(uint32_t a, uint32_t b) { return hash32(a * 0x9E3779B1u ^ hash32(b + 0x7F4A7C15u)); }
float lvl01(const Game& g, Skill s) { return std::clamp((float)(g.skills.level(s) - skills::START_LEVEL) / (float)(skills::MAX_LEVEL - skills::START_LEVEL), 0.0f, 1.0f); }

}  // namespace
void SkillOps::word(Game& g, Vec2 p, int color, float f, const std::string& s) {
  g.emit(Ev::Text, p, color, f, s);
  std::vector<std::string>& w = g.skills.live.words;
  w.push_back(s);
  if (w.size() > 24) w.erase(w.begin());
}
// a foe reels: its swing broken, a moment to strike (a boss reels less; the dragon not at all)
void SkillOps::stagger(Game& g, Actor& t, float secs, const char* what) {
  if (t.st == AState::Dead || t.st == AState::Down || t.mon == art::Monster::Dragon) return;
  if (t.boss || t.rank >= (uint8_t)foes::Rank::WorldBoss) secs *= 0.35f;
  t.st = AState::Hurt;
  t.stT = std::min(t.stT, 0.25f - secs);   // (updateAI holds Hurt until stT passes 0.25)
  t.heavy = t.lunge = false;
  t.atkCd = std::max(t.atkCd, secs * 0.8f);
  if (what) word(g, t.p + Vec2(0, -30), (int)rgba(255, 220, 140), 0, what);
}
namespace {
void bleed(Actor& t, float dps, float secs, int by) {
  if (!t.human) {
    using art::Monster;
    if (t.mon == Monster::Skeleton || t.mon == Monster::Wraith || t.mon == Monster::Wisp || t.mon == Monster::Golem || t.mon == Monster::Slime) return;
  }
  if (t.bleedT <= 0 || dps >= t.bleedDps) { t.bleedDps = dps; t.bleedBy = by; }
  t.bleedT = std::max(t.bleedT, secs);
}
}  // namespace

// ---------------------------------------------------------------- the gear the hero wears, read the skills' way
const Item* SkillOps::wielded(const Game& g) { return at(g, g.eqWeapon); }
Weight SkillOps::wornWeight(const Game& g) {
  if (const Item* b = at(g, g.eqArmor)) { const Weight w = skills::weightOf(*b); if (w != Weight::None) return w; }
  Weight w = Weight::None;   // no body piece: the heaviest of the rest (a helmet, gloves, boots)
  for (int idx : {g.eqHelmet, g.eqGloves, g.eqBoots})
    if (const Item* it = at(g, idx)) w = std::max(w, skills::weightOf(*it));
  return w;
}
bool SkillOps::fullSet(const Game& g, Weight w) {
  for (int idx : {g.eqArmor, g.eqHelmet, g.eqGloves, g.eqBoots}) {
    const Item* it = at(g, idx);
    if (!it || skills::weightOf(*it) != w) return false;
  }
  return w != Weight::None;
}
uint64_t SkillOps::setCulture(const Game& g) {
  uint64_t c = 0;
  for (int idx : {g.eqArmor, g.eqHelmet, g.eqGloves, g.eqBoots}) {
    const Item* it = at(g, idx);
    if (!it || !it->culture || (c && it->culture != c)) return 0;
    c = it->culture;
  }
  return c;
}
skills::ArmourTrade SkillOps::trade(const Game& g) {
  const Weight w = wornWeight(g);
  const Skill s = skills::armourSkill(w);
  return skills::armourTrade(w, s == Skill::COUNT ? skills::START_LEVEL : g.skills.level(s));
}

// ---------------------------------------------------------------- gains
// Smithing is one number with the forge's craft::Knowledge::skill (tenths, 0..1000; the recipes gate on it): the
// craft block holds it, the Smithing track mirrors it (its level and its share of the next level)
void SkillOps::smithFromCraft(Game& g) {
  skills::Track& t = g.skills[Skill::Smithing];
  const int lv = std::clamp((int)g.craft.skill / 10, 1, skills::MAX_LEVEL);
  const int tenth = lv >= skills::MAX_LEVEL ? 0 : (int)g.craft.skill % 10;
  const int now = t.level >= skills::MAX_LEVEL ? 0 : std::clamp((int)(t.xp / skills::xpToNext(t.level) * 10.0f), 0, 9);
  if (t.level != lv || now != tenth) { t.level = (uint8_t)lv; t.xp = lv >= skills::MAX_LEVEL ? 0.0f : (float)tenth * 0.1f * skills::xpToNext(lv); }
}
void SkillOps::craftFromSmith(Game& g) {
  const skills::Track& t = g.skills[Skill::Smithing];
  const int tenth = t.level >= skills::MAX_LEVEL ? 0 : std::clamp((int)(t.xp / skills::xpToNext(t.level) * 10.0f), 0, 9);
  g.craft.skill = (uint16_t)std::min(1000, (int)t.level * 10 + tenth);
}

void SkillOps::use(Game& g, Skill s, float base) {
  if ((int)s >= skills::SKILLS || !(base > 0)) return;
  if (s == Skill::Smithing) smithFromCraft(g);
  skills::Track& t = g.skills[s];
  g.skills.uses[(int)s]++;
  if (t.level >= skills::MAX_LEVEL) return;
  const float pts = base * skills::skillRate(s);
  g.skills.live.points[(int)s] += pts;
  t.xp += pts;
  int cx = 0;
  while (t.level < skills::MAX_LEVEL && t.xp >= skills::xpToNext(t.level)) {
    t.xp -= skills::xpToNext(t.level);
    t.level++;
    const bool mastered = t.level >= skills::MAX_LEVEL;
    skillEv(g, s, mastered ? 6 : 1, t.level,
            std::string(skills::skillInfo(s).name) + (mastered ? " MASTERED" : " RISES TO " + std::to_string((int)t.level)));
    cx += skills::charXpForSkillUp(t.level);
  }
  if (t.level >= skills::MAX_LEVEL) t.xp = 0;
  if (s == Skill::Smithing) craftFromSmith(g);
  if (cx > 0) g.gainXp(cx);   // (15.29.1) character level rises from the skills' gains, alongside the kills' XP
}

// ---------------------------------------------------------------- combat
void SkillOps::preStrike(Game& g, Actor& v, Skill fam) {
  skills::Skills::Live& L = g.skills.live;
  L.preId = v.id;
  L.preUnaware = v.hostile && !v.aggro;
  L.preRush = v.lunge && (v.st == AState::Windup || v.st == AState::Strike);
  L.combatT = 0;
  // Mace SKULL CRUSHER: bones broken by a mace stay broken (kill() never lets them rise)
  if (fam == Skill::Mace && P(g, Skill::Mace, MA_SKULL) && !v.human && v.mon == art::Monster::Skeleton) v.reassembled = true;
}
bool SkillOps::forceCrit(Game& g, const Actor& v, Skill fam) {
  skills::Skills::Live& L = g.skills.live;
  if (fam == Skill::Sword && L.riposteT > 0) {   // Sword RIPOSTE: the parried foe is open
    L.riposteT = 0;
    word(g, v.p + Vec2(0, -36), (int)rgba(220, 230, 255), 0, "RIPOSTE");
    return true;
  }
  // daggers crit the unaware (15.29.1: "daggers backstab and crit the unaware")
  if (fam == Skill::Dagger && v.hostile && !v.aggro) {
    word(g, v.p + Vec2(0, -36), (int)rgba(180, 240, 210), 0, "BACKSTAB");
    return true;
  }
  return false;
}
float SkillOps::strikeMul(Game& g, const Actor& v, Skill fam, bool power) {
  (void)power;
  const skills::Skills::Live& L = g.skills.live;
  uint16_t tags = skills::tagsOf(v);
  if (L.preId == v.id && L.preUnaware) tags |= skills::FT_UNAWARE;
  if (fam == Skill::Bow && P(g, Skill::Bow, BO_RANGER)) tags &= (uint16_t)~skills::FT_SHIELDED;   // RANGER: shields no longer stop them
  float m = skills::levelDamageMul(g.skills.level(fam)) * (1.0f + 0.01f * (float)skills::familyVsPct(fam, tags));
  const bool unaware = (tags & skills::FT_UNAWARE) != 0;
  switch (fam) {
    case Skill::Sword: if (P(g, fam, SW_BLADESMAN)) m *= 1.15f; break;
    case Skill::Axe:
      if (P(g, fam, AX_AXEMAN)) m *= 1.15f;
      if (P(g, fam, AX_HEADSMAN) && v.hp < v.maxHp * 0.25f) m *= 1.4f;
      break;
    case Skill::Mace: {
      if (P(g, fam, MA_MACEMAN)) m *= 1.15f;
      // BONEBREAKER (a third of the armour ignored) and ARMOUR BREAKER (a fifth cracked off): the mitigation damage()
      // will take, undone by that share
      float keep = 1.0f;
      if (P(g, fam, MA_BONEBREAKER)) keep *= 2.0f / 3.0f;
      if ((v.skillTag & TAG_BROKEN) && v.skillT > 0) keep *= 0.8f;
      if (keep < 1.0f && v.armor > 0) {
        const int atk = std::max(1, g.plLevel);
        m *= (1.0f - gear::mitigation(v.armor * keep, atk)) / std::max(0.05f, 1.0f - gear::mitigation(v.armor, atk));
      }
      break;
    }
    case Skill::Spear:
      if (P(g, fam, SP_SPEARMAN)) m *= 1.15f;
      if (P(g, fam, SP_GIANT) && (tags & (skills::FT_HUGE | skills::FT_FLYING))) m *= 1.25f;
      break;
    case Skill::Dagger:
      if (P(g, fam, DA_HAND)) m *= 1.15f;
      if (unaware && P(g, fam, DA_ASSASSIN)) m *= 5.0f;
      else if (unaware && P(g, fam, DA_BACKSTAB)) m *= 3.0f;
      break;
    case Skill::Bow:
      if (P(g, fam, BO_OVERDRAW)) m *= 1.15f;
      if (P(g, fam, BO_MARK) && (tags & skills::FT_BEAST) && (v.skillTag & TAG_MARK)) m *= 1.3f;
      break;
    case Skill::Unarmed:
      if (P(g, fam, UN_FISTS)) m *= 1.3f;
      if (P(g, fam, UN_HAYMAKER) && (tags & skills::FT_PEOPLE)) m *= 1.25f;
      break;
    default: break;
  }
  // Stealth SNEAK ATTACK: sneaking, any blow to the unaware lands double
  if (unaware && L.sneaking && P(g, Skill::Stealth, ST_SNEAKATTACK)) m *= 2.0f;
  // Heritage: steel and alloys known well; a weapon of the same people as the armour
  if (const Item* w = fam == Skill::Bow ? at(g, g.eqBow) : at(g, g.eqWeapon)) {
    if (P(g, Skill::Heritage, HE_METALS) && (w->mat == Mat::Steel || w->mat == Mat::Alloy))
      m *= 1.0f + 0.08f * (float)g.skills.matFam[(int)w->mat] / 100.0f;
    if (P(g, Skill::Heritage, HE_BLADEMAIL) && w->culture) if (const Item* b = at(g, g.eqArmor)) if (b->culture == w->culture) m *= 1.1f;
  }
  return m;
}
void SkillOps::swingShape(Game& g, bool finisher, float& reach, float& cone) {
  const Skill fam = famOf(g);
  if (!finisher) return;
  if (fam == Skill::Sword && P(g, fam, SW_WHIRLWIND)) { cone = -1.01f; reach += 2.0f; }    // all round
  if (fam == Skill::Axe && P(g, fam, AX_CLEAVE)) { cone = std::min(cone, -0.45f); reach += 6.0f; }
  if (fam == Skill::Spear && P(g, fam, SP_IMPALE)) { cone = std::max(cone, 0.55f); reach += 16.0f; }   // a long thrust down a line
}

void SkillOps::struck(Game& g, Actor& v, Skill fam, float dealt, bool crit, bool power, bool killed) {
  if (!v.hostile || dealt <= 0) return;   // only real foes teach (no training on townsfolk)
  skills::Skills::Live& L = g.skills.live;
  L.combatT = 0;
  const bool unaware = L.preId == v.id && L.preUnaware;
  const bool rushing = L.preId == v.id && L.preRush;
  L.preId = -1;
  g.skillUse(fam, 1.0f + (killed ? 2.0f : 0.0f) + (crit ? 0.5f : 0.0f) + (power ? 0.5f : 0.0f));
  // a blow on the unaware while sneaking is Stealth's own act too (a sneak attack)
  if (unaware && L.sneaking) g.skillUse(Skill::Stealth, killed ? 3.0f : 1.5f);
  const uint16_t tags = skills::tagsOf(v);
  if (killed) {
    if (unaware) {
      // an unaware foe's death cry wakes its fellows ... unless the dagger's SILENT KILL made none
      if (fam == Skill::Dagger && P(g, fam, DA_SILENT)) {
        word(g, v.p + Vec2(0, -24), (int)rgba(170, 190, 230), 0, "SILENT");
      } else {
        for (Actor& o : g.actors)
          if (o.id != v.id && o.hostile && o.st != AState::Dead && !o.aggro && len2(o.p - v.p) < (7.0f * TILE) * (7.0f * TILE)) {
            o.aggro = true;
            o.target = g.pl().id;
          }
      }
    }
    if (fam == Skill::Axe && P(g, fam, AX_BLOODLUST) && v.bleedT > 0) g.stamina += 25.0f;
    if (fam == Skill::Dagger && P(g, fam, DA_SHADOW) && L.sneaking) { L.unseenT = std::max(L.unseenT, 2.0f); word(g, g.pl().p + Vec2(0, -30), (int)rgba(150, 150, 210), 0, "UNSEEN"); }
    return;
  }
  // ---- the family's after-effects on a living foe
  switch (fam) {
    case Skill::Sword:
      if (crit && P(g, fam, SW_MASTER)) stagger(g, v, 0.5f, "STAGGERED");
      break;
    case Skill::Axe: {
      if (power && P(g, fam, AX_HEW)) stagger(g, v, 0.6f, "STAGGERED");
      if (P(g, fam, AX_BLEED) && (tags & (skills::FT_BEAST | skills::FT_UNARMOURED))) {
        const bool deep = P(g, fam, AX_DEEP);
        bleed(v, dealt * (deep ? 0.3f : 0.15f), deep ? 8.0f : 4.0f, g.pl().id);
      }
      break;
    }
    case Skill::Mace:
      if (power && P(g, fam, MA_HAMMERFALL)) stagger(g, v, 1.4f, "FLATTENED");
      else if (power && P(g, fam, MA_CRUSH)) stagger(g, v, 0.7f, "STAGGERED");
      if (P(g, fam, MA_BREAKER)) { v.skillTag |= TAG_BROKEN; v.skillT = 6.0f; }
      if (power && P(g, fam, MA_SHOCKWAVE)) {
        g.emit(Ev::Shake, v.p, 0, 3.0f);
        g.emit(Ev::Dust, v.p);
        for (Actor& o : g.actors)
          if (o.id != v.id && o.hostile && o.st != AState::Dead && len2(o.p - v.p) < 34.0f * 34.0f) stagger(g, o, 0.6f, nullptr);
      }
      break;
    case Skill::Spear:
      if (rushing && (tags & skills::FT_CHARGER) && P(g, fam, SP_SETCHARGE)) { stagger(g, v, 1.0f, "CHARGE STOPPED"); v.knock = v.knock * 1.6f; }
      if (P(g, fam, SP_BAY)) v.knock = v.knock * 2.0f;
      break;
    case Skill::Dagger:
      if (crit && P(g, fam, DA_LACERATE)) bleed(v, std::max(1.0f, dealt * 0.2f), 5.0f, g.pl().id);
      break;
    case Skill::Bow:
      if (P(g, fam, BO_POWER)) stagger(g, v, 0.5f, nullptr);
      if (P(g, fam, BO_MARK) && (tags & skills::FT_BEAST) && !(v.skillTag & TAG_MARK)) {
        v.skillTag |= TAG_MARK;
        word(g, v.p + Vec2(0, -30), (int)rgba(200, 240, 140), 0, "MARKED");
      }
      break;
    case Skill::Unarmed:
      if (power && P(g, fam, UN_STUN)) stagger(g, v, 1.0f, "STUNNED");
      else if (power && P(g, fam, UN_BRAWLER)) stagger(g, v, 0.6f, "STAGGERED");
      break;
    default: break;
  }
}

float SkillOps::takenMul(Game& g, int attacker, float dmg) {
  if (dmg <= 0) return dmg;
  skills::Skills::Live& L = g.skills.live;
  Actor& p = g.pl();
  const int ai = attacker >= 0 ? g.findActor(attacker) : -1;
  Actor* atk = ai > 0 ? &g.actors[(size_t)ai] : nullptr;
  const bool melee = atk && len2(atk->p - p.p) < 44.0f * 44.0f && !atk->ranged;
  L.combatT = 0;
  // ---- the sword's parry (15.29.1 "swords are balanced, with parry"): a swing meeting a blow from the front turns part
  //      of it aside; PARRY makes the window the whole swing, turns it all and the foe reels; RIPOSTE opens it
  const Skill fam = famOf(g);
  if (fam == Skill::Sword && melee && dot(norm(atk->p - p.p), p.aim) > 0.2f) {
    const bool perk = P(g, Skill::Sword, SW_PARRY);
    if (p.st == AState::Strike || (perk && p.st == AState::Windup)) {
      word(g, p.p + Vec2(0, -26), (int)rgba(220, 230, 255), 0, "PARRY");
      g.sfx((int)Sfx::Block, p.p, 1.2f);
      g.emit(Ev::Sparkle, (p.p + atk->p) * 0.5f + Vec2(0, -8));
      if (!perk) dmg *= 0.6f;
      else {
        dmg = 0;
        stagger(g, *atk, 0.8f, nullptr);
        if (P(g, Skill::Sword, SW_RIPOSTE)) L.riposteT = 2.0f;
        return 0.0f;
      }
    }
  }
  // ---- the worn class: its own share of the blow stopped (the skill grows it), the root perks 10 % more
  const Weight w = wornWeight(g);
  float m = 1.0f - trade(g).dr;
  if (w == Weight::Light && P(g, Skill::LightArmour, LI_AGILE)) m *= 0.9f;
  if (w == Weight::Medium && P(g, Skill::MediumArmour, ME_BALANCED)) m *= 0.9f;
  if (w == Weight::Medium && P(g, Skill::MediumArmour, ME_VETERAN)) m *= 0.9f;
  if (w == Weight::Heavy && P(g, Skill::HeavyArmour, HV_JUGGERNAUT)) m *= 0.9f;
  // ---- the shield
  if (g.eqShield >= 0) {
    if (P(g, Skill::Block, BL_WALL)) m *= 0.9f;
    if (P(g, Skill::Block, BL_BULWARK)) m *= 0.8f;
    if (!melee && P(g, Skill::Block, BL_DEFLECT)) m *= 0.5f;
    if (fam == Skill::Spear && melee && P(g, Skill::Spear, SP_PHALANX) && (skills::tagsOf(*atk) & skills::FT_CHARGER)) stagger(g, *atk, 0.9f, "BRACED");
  }
  if (!at(g, g.eqWeapon) && P(g, Skill::Unarmed, UN_IRONSKIN)) m *= 0.9f;
  dmg *= m;
  // ---- the once-in-a-while saves
  if (g.eqShield >= 0 && P(g, Skill::Block, BL_REFLEX) && L.reflexCd <= 0 && dmg > p.maxHp * 0.2f) {
    dmg *= 0.5f;
    L.reflexCd = 10.0f;
    word(g, p.p + Vec2(0, -30), (int)rgba(200, 220, 255), 0, "QUICK REFLEXES");
  }
  if (w == Weight::Heavy && melee && P(g, Skill::HeavyArmour, HV_REFLECT) && ++L.reflectN % 10 == 0 && atk->st != AState::Dead) {
    word(g, atk->p + Vec2(0, -26), (int)rgba(255, 220, 160), 0, "REFLECTED");
    g.sfx((int)Sfx::Block, p.p, 0.8f);
    atk->hp -= dmg;
    atk->flash = 0.12f;
    if (atk->hp <= 0) g.kill(*atk, p.id);
    return 0.0f;
  }
  return dmg;
}
void SkillOps::taken(Game& g, int attacker, float dealt) {
  if (attacker < 0 || dealt <= 0) return;
  skills::Skills::Live& L = g.skills.live;
  L.combatT = 0;
  const Skill a = skills::armourSkill(wornWeight(g));
  if (a != Skill::COUNT) g.skillUse(a, 1.0f);
  if (g.eqShield >= 0) g.skillUse(Skill::Block, 1.0f);
  // Medium RALLY: badly hurt, the wind comes back at once
  Actor& p = g.pl();
  if (a == Skill::MediumArmour && P(g, a, ME_RALLY) && L.rallyCd <= 0 && p.hp > 0 && p.hp < p.maxHp / 3.0f) {
    L.rallyCd = 60.0f;
    g.stamina = std::max(g.stamina, g.maxSt);
    word(g, p.p + Vec2(0, -34), (int)rgba(160, 255, 170), 0, "RALLY");
  }
}
bool SkillOps::unstaggerable(const Game& g, float dmg) {
  const Weight w = wornWeight(g);
  if (w == Weight::Heavy && P(g, Skill::HeavyArmour, HV_IMMOVABLE)) return true;
  if (g.eqShield >= 0 && P(g, Skill::Block, BL_STEADFAST)) return true;
  if (w == Weight::Medium && P(g, Skill::MediumArmour, ME_STEADY) && dmg < g.pl().maxHp * 0.15f) return true;
  return false;
}
float SkillOps::knockMul(const Game& g) { return wornWeight(g) == Weight::Heavy && P(g, Skill::HeavyArmour, HV_IMMOVABLE) ? 0.0f : 1.0f; }
bool SkillOps::keepSaddle(const Game& g, float dmg) { return P(g, Skill::Riding, RI_SURESEAT) && dmg < g.pl().maxHp * 0.1f; }
void SkillOps::dodged(Game& g) {
  if (wornWeight(g) != Weight::Light) return;
  if (P(g, Skill::LightArmour, LI_EVASION)) g.stamina += 20.0f * rollCostMul(g);
  if (P(g, Skill::LightArmour, LI_MATCHLESS)) g.skills.live.unseenT = std::max(g.skills.live.unseenT, 1.2f);
}
bool SkillOps::hidden(const Game& g) { return g.skills.live.unseenT > 0; }
bool SkillOps::losesTrail(const Game& g, const Actor& foe, float dist) {
  (void)foe;
  return g.skills.live.sneaking && P(g, Skill::Stealth, ST_VANISH) && dist > 8.0f * TILE;
}

float SkillOps::swingTime(const Game& g) {
  const Skill fam = famOf(g);
  float t = skills::familyTraits(fam).swingTime;
  if (fam == Skill::Dagger && P(g, fam, DA_QUICK)) t *= 0.85f;
  return t;
}
float SkillOps::reachAdd(const Game& g) {
  // (the dagger's -3 and the fists' -4 are meleeHit's own since M0; the spear's reach is the family's)
  const Skill fam = famOf(g);
  if (fam != Skill::Spear) return 0.0f;
  return skills::familyTraits(fam).reach + (P(g, fam, SP_REACH) ? 8.0f : 0.0f);
}
float SkillOps::swingCost(const Game& g) {
  const Skill fam = famOf(g);
  float c = skills::familyTraits(fam).stamina;
  if (fam == Skill::Sword && P(g, fam, SW_FLURRY)) c *= 0.67f;
  if (fam == Skill::Unarmed && P(g, fam, UN_FLURRY)) c = 0.0f;
  return c;
}
float SkillOps::speedMul(const Game& g) {
  const Weight w = wornWeight(g);
  float s = trade(g).speed;
  if (w == Weight::Heavy && P(g, Skill::HeavyArmour, HV_FITTED)) s = std::max(s, 1.0f);
  if (w == Weight::Light && P(g, Skill::LightArmour, LI_UNHINDERED)) s *= 1.05f;
  if (P(g, Skill::Athletics, AT_STRIDE)) s *= 1.05f;
  if (P(g, Skill::Athletics, AT_TRAVELLER) && g.skills.live.combatT > 6.0f) s *= 1.1f;
  if (g.skills.live.sneaking) s *= 0.6f;   // a careful step
  return s;
}
float SkillOps::rollMul(const Game& g) {
  float r = trade(g).roll;
  if (P(g, Skill::Athletics, AT_LIGHTFEET)) r *= 1.1f;
  return r;
}
float SkillOps::rollCostMul(const Game& g) {
  const Weight w = wornWeight(g);
  float c = trade(g).rollCost;
  if (w == Weight::Medium && P(g, Skill::MediumArmour, ME_WELLBAL)) c = std::min(c, 1.0f);
  if (w == Weight::Light && P(g, Skill::LightArmour, LI_DEFT)) c *= 0.5f;
  return c;
}
float SkillOps::staminaRegenMul(const Game& g) {
  const Weight w = wornWeight(g);
  float r = trade(g).stRegen;
  if (w == Weight::Heavy && P(g, Skill::HeavyArmour, HV_CONDITION)) r = std::max(r, 1.0f);
  if (w == Weight::Light && P(g, Skill::LightArmour, LI_WIND)) r *= 1.2f;
  if (P(g, Skill::Athletics, AT_SECONDWIND)) r *= 1.15f;
  return r;
}
bool SkillOps::rolled(Game& g) {
  g.skillUse(Skill::Athletics, 0.25f);
  skills::Skills::Live& L = g.skills.live;
  const bool charge = wornWeight(g) == Weight::Heavy && P(g, Skill::HeavyArmour, HV_CHARGE);
  const bool bash = g.eqShield >= 0 && P(g, Skill::Block, BL_BASH);
  if (!charge && !bash) return false;
  L.chargeT = 0.34f;
  L.charging = charge;
  for (uint32_t& id : L.chargeHit) id = 0;
  word(g, g.pl().p + Vec2(0, -26), (int)rgba(255, 210, 150), 0, charge ? "CHARGE" : "SHIELD BASH");
  return true;
}
float SkillOps::drawMul(const Game& g) { return P(g, Skill::Bow, BO_QUICK) ? 0.8f : 1.0f; }
void SkillOps::loosed(Game& g) {
  skills::Skills::Live& L = g.skills.live;
  if (g.projs.empty()) return;
  const size_t k = g.projs.size() - 1;
  if (g.projs[k].owner != g.pl().id || g.projs[k].kind != ProjKind::Arrow) return;
  L.combatT = std::min(L.combatT, 3.0f);
  if (P(g, Skill::Bow, BO_EAGLE)) g.projs[k].v = g.projs[k].v * 1.25f;
  if (P(g, Skill::Bow, BO_STEADY) && L.steadyCd <= 0) {   // a heartbeat of slow time as the string leaves the fingers
    g.slowMo = std::max(g.slowMo, 0.35f);
    L.steadyCd = 6.0f;
  }
  if (P(g, Skill::Bow, BO_TWIN) && ++L.arrows % 3 == 0) {
    Projectile c = g.projs[k];
    const float an = 0.12f, cs = std::cos(an), sn = std::sin(an);
    c.v = Vec2(c.v.x * cs - c.v.y * sn, c.v.x * sn + c.v.y * cs);
    g.projs.push_back(c);
  }
}
float SkillOps::detectMul(const Game& g, const Actor& foe) {
  const skills::Skills::Live& L = g.skills.live;
  if (L.unseenT > 0) return 0.0f;
  float m = 1.0f;
  if (L.sneaking) {
    // the worn class's noise counts when the hero tries to be quiet; the Stealth skill quiets the step
    const Weight w = wornWeight(g);
    m *= 0.55f * (0.6f + 0.8f * trade(g).noise) * (1.0f - 0.38f * lvl01(g, Skill::Stealth));
    if (P(g, Skill::Stealth, ST_SOFTSTEP)) m *= 0.8f;
    if (P(g, Skill::Stealth, ST_SHADOW) && (w == Weight::Light || w == Weight::None)) m *= 0.5f;
    if (w == Weight::Medium && P(g, Skill::MediumArmour, ME_VETERAN)) m *= 0.75f;
  }
  // Heritage ONE OF THEIR OWN: in a full set of their make, a people's soldiers take you for one of them at a distance
  if (foe.human && foe.culture && P(g, Skill::Heritage, HE_OWN) && setCulture(g) == foe.culture) m *= 0.5f;
  return m;
}

void SkillOps::step(Game& g, float dt) {
  skills::Skills::Live& L = g.skills.live;
  const Actor& p = g.pl();
  // ---- the clocks
  if (L.riposteT > 0) L.riposteT -= dt;
  if (L.unseenT > 0) L.unseenT -= dt;
  if (L.steadyCd > 0) L.steadyCd -= dt;
  if (L.reflexCd > 0) L.reflexCd -= dt;
  if (L.rallyCd > 0) L.rallyCd -= dt;
  const bool calm0 = L.combatT > 6.0f;
  L.combatT += dt;
  if (P(g, Skill::Athletics, AT_TRAVELLER) && calm0 != (L.combatT > 6.0f)) g.recalcPlayer();   // TRAVELLER's pace
  // ---- Smithing is the forge's number (a new hero's start level becomes the forge's on the first step)
  if (!L.smithInit) {
    L.smithInit = true;
    g.craft.skill = (uint16_t)std::max<int>(g.craft.skill, g.skills.level(Skill::Smithing) * 10);
  }
  smithFromCraft(g);
  // ---- the shoulder charge / shield bash under way: what the hero runs into reels
  if (L.chargeT > 0) {
    L.chargeT -= dt;
    for (Actor& t : g.actors) {
      if (!t.hostile || t.st == AState::Dead || t.fly || len2(t.p - p.p) > (p.radius + t.radius + 6.0f) * (p.radius + t.radius + 6.0f)) continue;
      bool hit = false;
      for (uint32_t id : L.chargeHit) hit |= id == (uint32_t)t.id;
      if (hit) continue;
      for (uint32_t& id : L.chargeHit) if (!id) { id = (uint32_t)t.id; break; }
      stagger(g, t, L.charging ? 0.9f : 0.6f, L.charging ? "BOWLED OVER" : "BASHED");
      t.knock = norm(t.p - p.p) * 170.0f;
      g.emit(Ev::Shake, t.p, 0, 2.0f);
      g.sfx((int)Sfx::HitHeavy, t.p, 0.8f);
      if (L.charging && P(g, Skill::HeavyArmour, HV_UNSTOPPABLE)) g.damage(t, g.weaponDamage() * 0.6f, p.p, p.id);
    }
  }
  // ---- marks and cracks fade; bleeds tick in updateActor
  // ---- distances: walking and running (Athletics), riding (Riding)
  if (!L.hasLast) { L.lastX = p.p.x; L.lastY = p.p.y; L.hasLast = true; return; }
  const float dx = p.p.x - L.lastX, dy = p.p.y - L.lastY;
  L.lastX = p.p.x; L.lastY = p.p.y;
  const float d = std::sqrt(dx * dx + dy * dy);
  if (d <= 48.0f) {   // (a door, a journey, the window moving: not walked)
    if (g.homeRiding()) {
      L.ridden += d;
      if (L.ridden >= 16.0f * TILE) { L.ridden -= 16.0f * TILE; g.skillUse(Skill::Riding, 1.0f); }
    } else {
      L.walked += d;
      if (L.walked >= 24.0f * TILE) { L.walked -= 24.0f * TILE; g.skillUse(Skill::Athletics, 1.0f); }
    }
  }
  // ---- sneaking near foes that have not noticed (Stealth)
  if (L.sneaking && p.st != AState::Dead) {
    bool near = false;
    for (const Actor& e : g.actors)
      if (e.hostile && e.st != AState::Dead && !e.aggro && len2(e.p - p.p) < (10.0f * TILE) * (10.0f * TILE)) { near = true; break; }
    if (near) {
      L.sneakT += dt;
      if (L.sneakT >= 4.0f) { L.sneakT -= 4.0f; g.skillUse(Skill::Stealth, 1.0f); }
    }
  }
  // ---- Heritage: time worn and wielded in a fight builds familiarity with each material and people's make
  if (L.combatT < 4.0f && p.st != AState::Dead) {
    L.heritT += dt;
    if (L.heritT >= 5.0f) {
      L.heritT -= 5.0f;
      bool any = false;
      L.heritTicks++;
      for (int idx : {g.eqArmor, g.eqHelmet, g.eqShield, g.eqGloves, g.eqBoots, g.eqWeapon, g.eqBow}) {
        const Item* it = at(g, idx);
        if (!it || it->mat == Mat::None) continue;
        any = true;
        if (L.heritTicks % 6 == 0) {   // a point of familiarity each half minute in a fight
          uint8_t& mf = g.skills.matFam[(int)it->mat];
          if (mf < 100) mf++;
          if (it->culture) { uint8_t& cf = g.skills.cultFam[it->culture]; if (cf < 100) cf++; }
        }
      }
      if (any) g.skillUse(Skill::Heritage, 1.0f);
    }
  }
  // ---- marks and cracks on foes; a staged sentry dozes where it stands until it notices the hero (scripts)
  for (Actor& a : g.actors) {
    if (a.skillT > 0) { a.skillT -= dt; if (a.skillT <= 0) a.skillTag &= (uint8_t)~TAG_BROKEN; }
    if ((a.skillTag & TAG_DOZE) && !a.aggro && a.st != AState::Dead) { a.goal = a.home; a.p = a.home; }
  }
}

void SkillOps::applyPerks(Game& g, gear::GearStats& s) {
  auto add = [&](Affix a, int v) { s.aff[(int)a] += v; };
  const Skill fam = famOf(g);
  // the families' own crit (the dagger's); the sword's KEEN EDGE
  if (const int c = skills::familyTraits(fam).critPct) add(Affix::CritChance, c);
  if (fam == Skill::Sword && P(g, fam, SW_KEEN)) add(Affix::CritChance, 10);
  // stamina
  const Weight w = wornWeight(g);
  if (w == Weight::Medium && P(g, Skill::MediumArmour, ME_WIND)) add(Affix::Stamina, 20);
  if (P(g, Skill::Athletics, AT_ENDURANCE)) add(Affix::Stamina, 20);
  // a full set of one class, cut to fit
  if ((w == Weight::Light && P(g, Skill::LightArmour, LI_FIT)) || (w == Weight::Medium && P(g, Skill::MediumArmour, ME_FIT)) ||
      (w == Weight::Heavy && P(g, Skill::HeavyArmour, HV_FIT)))
    if (fullSet(g, w)) add(Affix::ArmourPct, 15);
  // Heritage: a full set of one people's make, as well as it is known; bronze, bone and wood; the homeland's make
  if (P(g, Skill::Heritage, HE_KINSHIP))
    if (const uint64_t c = setCulture(g)) {
      const auto it = g.skills.cultFam.find(c);
      add(Affix::ArmourPct, 5 + (it == g.skills.cultFam.end() ? 0 : (int)it->second / 10));
    }
  if (P(g, Skill::Heritage, HE_BRONZE))
    if (const Item* b = at(g, g.eqArmor)) if (b->mat == Mat::Bronze || b->mat == Mat::Bone || b->mat == Mat::Wood) add(Affix::ArmourPct, 10);
  if (P(g, Skill::Heritage, HE_HOMELAND) && g.app.homeland) {
    bool own = false;
    for (int idx : {g.eqArmor, g.eqHelmet, g.eqShield, g.eqGloves, g.eqBoots, g.eqCloak, g.eqWeapon})
      if (const Item* it = at(g, idx)) own |= it->culture == g.app.homeland;
    if (own) add(Affix::Health, 15);
  }
}
void SkillOps::leveledUp(Game& g) {
  g.skills.perkPts++;
  skillEv(g, Skill::COUNT, 2, g.skills.perkPts, "A PERK POINT TO SPEND");
}

// ---------------------------------------------------------------- the economy and talk
float SkillOps::buyMul(const Game& g, Role seller) {
  (void)seller;
  // Speech: a practised tongue pays up to 15 % less at mastery; HAGGLER 10 % more
  float m = 1.0f - 0.15f * lvl01(g, Skill::Speech);
  if (P(g, Skill::Speech, SC_HAGGLER)) m *= 0.9f;
  return m;
}
float SkillOps::sellMul(const Game& g) {
  float m = 1.0f + 0.15f * lvl01(g, Skill::Speech);
  if (P(g, Skill::Speech, SC_HAGGLER)) m *= 1.1f;
  return m;
}
// (M9 fixer, review: "buying and selling cheap items raises Speech in minutes") scaled by the price with no floor (a
// 4-gold plank is worth a sliver, a 60-gold deal about a point: the --skills rate), and at most 10 points (about 550
// gold of trade) per merchant per restock, so churning one shelf cannot farm the skill or the character levels
void SkillOps::traded(Game& g, int gold) {
  if (gold <= 0) return;
  constexpr float SPEECH_TRADE_CAP = 10.0f;
  float base = std::min(3.0f, 0.018f * (float)gold);
  auto& rec = g.skills.live.traded[g.shop.key];
  if (rec.first != g.day / 2) rec = {g.day / 2, 0.0f};
  base = std::min(base, SPEECH_TRADE_CAP - rec.second);
  if (base <= 0) return;
  rec.second += base;
  g.skillUse(Skill::Speech, base);
}

// who teaches what (15.29.4): the census job (or the role) -> up to three skills; the magic schools are the MAGIC lane's
int SkillOps::teaches(Game& g, const Actor& a, Skill out[3], int& cap) {
  cap = 0;
  if (!a.npc || !a.human || a.hostile || a.comp || a.critter) return 0;
  life::Census* c = nullptr;
  int idx = -1, site = -1;
  life::Job job = life::Job::None;
  if (HomeOps::residentOf(g, a, c, idx, site)) job = c->res[(size_t)idx].job;
  else site = HomeOps::siteOfActor(g, a);
  if (site < 0 || site >= (int)g.world.sites.size() || !g.world.sites[(size_t)site].settlement()) return 0;
  int n = 0;
  auto add = [&](Skill s) { if (n < 3) out[n++] = s; };
  using J = life::Job;
  switch (job) {
    case J::Smith: add(Skill::Smithing); add(Skill::Mace); add(Skill::HeavyArmour); break;
    case J::Guard: add(Skill::Sword); add(Skill::Block); add(Skill::HeavyArmour); break;
    case J::Hunter: add(Skill::Bow); add(Skill::Stealth); add(Skill::LightArmour); break;
    case J::Innkeeper: case J::Server: add(Skill::Cooking); add(Skill::Speech); break;
    case J::Baker: add(Skill::Cooking); break;
    case J::Farmer: add(Skill::Farming); break;
    case J::Herder: add(Skill::Farming); add(Skill::Riding); break;
    case J::Stablehand: add(Skill::Riding); break;
    case J::Merchant: case J::Bard: add(Skill::Speech); break;
    case J::Noble: add(Skill::Speech); add(Skill::Sword); add(Skill::Riding); break;
    case J::Woodcutter: add(Skill::Axe); add(Skill::Athletics); break;
    case J::Miner: add(Skill::Mace); add(Skill::Athletics); break;
    case J::Fisher: add(Skill::Spear); break;
    case J::Tailor: add(Skill::LightArmour); add(Skill::MediumArmour); break;
    case J::Labourer: add(Skill::Athletics); add(Skill::Unarmed); break;
    case J::Beggar: add(Skill::Stealth); add(Skill::Dagger); break;
    case J::None:   // not of the census (a keep's guard, a garrison's captain, a shopkeeper set by the map): by the role
      switch (a.role) {
        case Role::Smith: add(Skill::Smithing); add(Skill::Mace); add(Skill::HeavyArmour); break;
        case Role::Guard: case Role::Captain: add(Skill::Sword); add(Skill::Block); add(Skill::HeavyArmour); break;
        case Role::Hunter: add(Skill::Bow); add(Skill::Stealth); break;
        case Role::Innkeeper: add(Skill::Cooking); add(Skill::Speech); break;
        case Role::Farmer: add(Skill::Farming); break;
        case Role::Merchant: add(Skill::Speech); break;
        case Role::Fisher: add(Skill::Spear); break;
        default: break;
      }
      break;
    default: break;
  }
  const Site& S = g.world.sites[(size_t)site];
  cap = skills::trainerCap(S.type, S.capital);
  return n;
}
int SkillOps::trainCost(const Game& g, Skill s) {
  int p = skills::trainPrice(g.skills.level(s));
  if (P(g, Skill::Speech, SC_TRAINERPET)) p = p * 3 / 4;
  return std::max(1, p);
}
int SkillOps::trainLeft(const Game& g) {
  return g.skills.trainedLevel == (uint8_t)std::min(g.plLevel, 255) ? std::max(0, skills::TRAIN_PER_LEVEL - (int)g.skills.trainedCount) : skills::TRAIN_PER_LEVEL;
}
bool SkillOps::train(Game& g, Skill s, int cap, int gold, std::string* why) {
  auto no = [&](const std::string& w) { if (why) *why = w; return false; };
  if ((int)s >= skills::SKILLS) return no("NO SUCH SKILL");
  skills::Track& t = g.skills[s];
  if (t.level >= cap) return no("BEYOND WHAT I CAN TEACH");
  if (trainLeft(g) <= 0) return no("NO MORE TRAINING AT THIS LEVEL");
  if (g.gold < gold) return no("NEEDS " + std::to_string(gold) + " GOLD");
  g.gold -= gold;
  if (g.skills.trainedLevel != (uint8_t)std::min(g.plLevel, 255)) { g.skills.trainedLevel = (uint8_t)std::min(g.plLevel, 255); g.skills.trainedCount = 0; }
  g.skills.trainedCount++;
  t.level++;
  t.xp = 0;
  if (s == Skill::Smithing) craftFromSmith(g);
  skillEv(g, s, 5, t.level, std::string(skills::skillInfo(s).name) + " RISES TO " + std::to_string((int)t.level));
  g.recalcPlayer();
  return true;
}

namespace {
std::string lower(const char* s) {   // "HEAVY ARMOUR" in a sentence stays upper case (the game's voice): a plain copy
  return std::string(s);
}
// the companion's teachings by vocation (the magic schools are the MAGIC lane's; the hound teaches nothing)
int companionSkills(const party::Member& m, Skill out[3]) {
  int n = 0;
  auto add = [&](Skill s) { if (n < 3) out[n++] = s; };
  switch (m.voc) {
    case party::Vocation::Warrior: add(Skill::Sword); add(Skill::Block); add(Skill::HeavyArmour); break;
    case party::Vocation::Archer: add(Skill::Bow); add(Skill::Stealth); add(Skill::LightArmour); break;
    case party::Vocation::Rogue: add(Skill::Dagger); add(Skill::Stealth); add(Skill::Speech); break;
    case party::Vocation::Healer: add(Skill::Cooking); break;
    default: break;
  }
  return n;
}
int companionCap(const party::Member& m) { return std::min(90, 30 + (int)m.bond * 6 / 10); }
}  // namespace

int SkillOps::companionTeaches(const Game& g, int mi, Skill out[3], int& cap) {
  cap = 0;
  if (mi < 0 || mi >= (int)g.party.members.size()) return 0;
  const party::Member& m = g.party.members[(size_t)mi];
  cap = companionCap(m);
  return companionSkills(m, out);
}

void SkillOps::talk(Game& g, Actor& a) {
  if (a.comp || !a.npc || !a.human || a.hostile) return;
  Skill sk[3];
  int cap = 0;
  // at most two SKILLS options on the first page: a trainer's lessons, a merchant's prices (with the speech perks), or a
  // priest's unlearning
  const bool trainer = teaches(g, a, sk, cap) > 0;
  if (trainer) g.dlg.opts.push_back({"COULD YOU TRAIN ME?", K_TRAIN, 0});
  const bool trades = a.role == Role::Merchant || a.role == Role::Smith || a.role == Role::Hunter || a.role == Role::Herbalist || a.role == Role::Fisher;
  bool anyPerk = false;   // (M9 fixer) the perks held now, not perksTaken (a respec gives them back)
  for (int i = 0; i < skills::SKILLS && !anyPerk; i++) anyPerk = g.skills[(Skill)i].perks != 0;
  if (trades && (P(g, Skill::Speech, SC_PERSUADE) || P(g, Skill::Speech, SC_INTIMIDATE))) g.dlg.opts.push_back({"LET'S TALK ABOUT YOUR PRICES.", K_PRICES, 0});
  else if (!trainer && a.role == Role::Priest && anyPerk) g.dlg.opts.push_back({"HELP ME LET GO OF OLD HABITS.", K_RESPEC, 0});
}

bool SkillOps::choose(Game& g, const DlgOpt& o) {
  const int ai = g.findActor(g.dlg.actor);
  Actor* a = ai >= 0 ? &g.actors[(size_t)ai] : nullptr;
  Dialogue& d = g.dlg;
  auto bye = [&](const char* w) { d.opts.push_back({w, A_BYE, 0}); };
  // ---- a trainer's list
  auto trainerPage = [&](const std::string& line) {
    Skill sk[3];
    int cap = 0;
    const int n = a ? teaches(g, *a, sk, cap) : 0;
    d.opts.clear();
    d.group = 0;
    d.text = line;
    for (int i = 0; i < n; i++) {
      const Skill s = sk[i];
      const int lv = g.skills.level(s);
      if (lv < cap) d.opts.push_back({"TEACH ME " + std::string(skills::skillInfo(s).name) + " (" + std::to_string(trainCost(g, s)) + " GOLD)", K_LEARN + (int)s, 0});
    }
    bool perks = false;
    for (int i = 0; i < n; i++) if (g.skills[sk[i]].perks) perks = true;
    if (perks) d.opts.push_back({"HELP ME UNLEARN A HABIT OR TWO.", K_RESPEC, 1});
    bye("THAT'S ALL FOR NOW.");
  };
  if (o.action == K_TRAIN || o.action == K_BACK) {
    if (!a) { g.mode = Mode::Play; return true; }
    const int left = trainLeft(g);
    trainerPage(left <= 0 ? "YOU'VE TRAINED ENOUGH FOR NOW. GO AND USE WHAT YOU'VE LEARNED, AND WE'LL TALK AGAIN WHEN YOU'VE GROWN."
                          : "I CAN TRAIN YOU, FOR A FAIR PRICE. " + std::string(left == 1 ? "ONE MORE LESSON" : speech::countWord(left) + " LESSONS") + ", THEN GO AND PRACTISE.");
    return true;
  }
  if (o.action >= K_LEARN && o.action < K_LEARN + skills::SKILLS) {
    const Skill s = (Skill)(o.action - K_LEARN);
    Skill sk[3];
    int cap = 0;
    if (!a || !teaches(g, *a, sk, cap)) { g.mode = Mode::Play; return true; }
    std::string why;
    if (train(g, s, cap, trainCost(g, s), &why)) {
      g.sfx((int)Sfx::LevelUp, g.pl().p, 1.2f, 0.6f);
      trainerPage("GOOD. YOU'RE QUICKER THAN YOU LOOK. YOUR " + lower(skills::skillInfo(s).name) + " IS " + std::to_string(g.skills.level(s)) + " NOW.");
    } else {
      trainerPage(why == "BEYOND WHAT I CAN TEACH" ? "THERE'S NOTHING MORE I CAN TEACH YOU THERE. FIND A BETTER MASTER IN A BIGGER TOWN."
                  : why == "NO MORE TRAINING AT THIS LEVEL" ? "ENOUGH TEACHING FOR TODAY. PRACTISE WHAT YOU HAVE FIRST."
                  : "COME BACK WHEN YOU HAVE THE COIN. I DON'T TEACH FOR FREE.");
    }
    return true;
  }
  // ---- respec: a trainer's own skills, or any skill at a priest's
  if (o.action == K_RESPEC) {
    d.opts.clear();
    d.group = 0;
    Skill sk[3];
    int cap = 0;
    const int n = a && a->role != Role::Priest ? teaches(g, *a, sk, cap) : 0;
    for (int i = 0; i < skills::SKILLS && (int)d.opts.size() < speech::FIRST_PAGE - 1; i++) {
      const Skill s = (Skill)i;
      if (!g.skills[s].perks) continue;   // (M9 fixer) the magic schools' perks too (a priest unlearns any tree)
      if (n > 0 && std::find(sk, sk + n, s) == sk + n) continue;
      d.opts.push_back({"FORGET MY " + std::string(skills::skillInfo(s).name) + " PERKS (" + std::to_string(respecCost(g, s)) + " GOLD)", K_UNLEARN + i, 0});
    }
    d.text = d.opts.empty() ? "THERE'S NOTHING I CAN HELP YOU LET GO OF." :
             a && a->role == Role::Priest ? "OLD HABITS CAN BE LAID DOWN, BUT NOT LIGHTLY. IT TAKES A NIGHT OF PRAYER AND A GIFT TO THE TEMPLE."
                                          : "UNLEARNING IS HARDER THAN LEARNING. IT WILL COST YOU.";
    bye("NEVER MIND.");
    return true;
  }
  if (o.action >= K_UNLEARN && o.action < K_UNLEARN + skills::SKILLS) {
    const Skill s = (Skill)(o.action - K_UNLEARN);
    std::string why;
    d.opts.clear();
    d.group = 0;
    if (respec(g, s, &why)) d.text = "IT'S DONE. YOU'LL FIND YOUR " + lower(skills::skillInfo(s).name) + " READY FOR A NEW PATH.";
    else d.text = why.rfind("NEEDS", 0) == 0 ? "THAT TAKES MORE GOLD THAN YOU CARRY." : "YOU HAVE NO PERKS THERE TO FORGET.";
    bye("THANK YOU.");
    return true;
  }
  // ---- persuasion and threats at a merchant's counter
  if (o.action == K_PRICES) {
    d.opts.clear();
    d.group = 0;
    d.text = "MY PRICES ARE FAIR. WHAT ABOUT THEM?";
    if (P(g, Skill::Speech, SC_PERSUADE)) d.opts.push_back({"SURELY A REGULAR LIKE ME GETS A BETTER PRICE?", K_PERSUADE, 0});
    if (P(g, Skill::Speech, SC_INTIMIDATE)) d.opts.push_back({"LOWER THEM, OR YOU'LL REGRET IT.", K_INTIMIDATE, 0});
    bye("NOTHING. THEY'RE FAIR.");
    return true;
  }
  if (o.action == K_PERSUADE || o.action == K_INTIMIDATE) {
    d.opts.clear();
    d.group = 0;
    if (!a) { g.mode = Mode::Play; return true; }
    const uint64_t key = g.npcKey(*a);
    const bool threat = o.action == K_INTIMIDATE;
    skills::Skills::Live& L = g.skills.live;
    bool settled = false;
    for (size_t i = 0; i < L.haggled.size();) {
      if (L.haggled[i].second != g.day) { L.haggled.erase(L.haggled.begin() + (std::ptrdiff_t)i); continue; }
      if (L.haggled[i].first == key) settled = true;
      i++;
    }
    if (settled) {
      d.text = "YES, AND WE'VE ALREADY SETTLED THE PRICE TODAY.";
      d.opts.push_back({"LET ME SEE YOUR WARES.", A_TRADE, 0});
      bye("FAIR ENOUGH.");
      return true;
    }
    // the chance: the Speech level, the target's nerve (a smith or a hunter does not scare easily); SILVER TONGUE never fails
    const int sp = g.skills.level(Skill::Speech);
    int chance = threat ? 25 + sp / 2 - (a->role == Role::Smith || a->role == Role::Hunter ? 20 : 0) : 35 + sp / 2;
    if (P(g, Skill::Speech, SC_SILVER)) chance = 100;
    const bool ok = (int)(mixh((uint32_t)key ^ (uint32_t)(key >> 32), (uint32_t)g.day * 31u + (threat ? 7u : 3u)) % 100u) < chance;
    L.haggled.push_back({key, g.day});
    g.skillUse(Skill::Speech, ok ? 4.0f : 1.0f);
    if (!ok) {
      d.text = threat ? "REGRET? YOU'RE THE ONE WHO'LL REGRET THIS. THE GUARDS WILL HEAR OF IT." : "NICE TRY. THE PRICE IS THE PRICE.";
      if (threat) g.talkMem.flag(key, speech::TM_SLIGHTED, g.day);
      d.opts.push_back({"LET ME SEE YOUR WARES.", A_TRADE, 0});
      bye("FORGET IT.");
      return true;
    }
    // the shelf of this restock, lowered (the shop opens on the cached shelf)
    auto it = g.shopCache_.find(key);
    if (it == g.shopCache_.end() || it->second.first != g.day / 2) { g.shopCache_[key] = {g.day / 2, g.shopStock(*a)}; it = g.shopCache_.find(key); }
    const float cut = threat ? 0.8f : 0.85f;
    // (M9 fixer) a shelf is cut once per restock: a second haggle on the next day of the same restock never stacks
    int& low = L.lowered[key];
    if (low != g.day / 2 + 1) {
      low = g.day / 2 + 1;
      for (Item& s : it->second.second) if (s.kind != ItemKind::Arrows) s.value = std::max(1, (int)std::lround(s.value * cut));
    }
    if (threat) g.talkMem.flag(key, speech::TM_SLIGHTED, g.day);
    d.text = threat ? "ALL RIGHT! I'LL LOWER THEM. A FIFTH OFF, JUST LEAVE ME BE." : "WELL, SINCE IT'S YOU. I'LL KNOCK A LITTLE OFF TODAY.";
    word(g, g.pl().p + Vec2(0, -30), (int)rgba(250, 220, 170), 0, threat ? "INTIMIDATED" : "PERSUADED");
    d.opts.push_back({"LET ME SEE YOUR WARES.", A_TRADE, 0});
    bye("THANK YOU.");
    return true;
  }
  // ---- a companion shows you their vocation (party_talk.cpp offers it in their own conversation)
  if (o.action == K_COMPANION || (o.action >= K_SHOWME && o.action < K_SHOWME + skills::SKILLS)) {
    const int mi = o.arg;
    Skill sk[3];
    int cap = 0;
    const int n = companionTeaches(g, mi, sk, cap);
    if (n <= 0) { g.mode = Mode::Play; return true; }
    const party::Member& m = g.party.members[(size_t)mi];
    d.opts.clear();
    d.group = 0;
    if (o.action >= K_SHOWME) {
      const Skill s = (Skill)(o.action - K_SHOWME);
      std::string why;
      if (train(g, s, cap, 0, &why)) {
        g.sfx((int)Sfx::LevelUp, g.pl().p, 1.25f, 0.5f);
        d.text = m.bond >= 60 ? "GOOD. YOU'VE GOT IT NOW. I'D TRUST YOU AT MY BACK." : "WELL, LIKE THAT. NOT BAD. KEEP PRACTISING.";
      } else d.text = why == "BEYOND WHAT I CAN TEACH" ? "YOU'VE CAUGHT UP WITH ME THERE. I'VE NOTHING LEFT TO SHOW YOU."
                                                       : "WELL, LET'S LEAVE IT THERE FOR NOW. USE WHAT I SHOWED YOU FIRST.";
    } else d.text = m.bond >= 40 ? "OF COURSE. I'LL SHOW YOU WHAT I KNOW, AS WE GO." : "I SUPPOSE I COULD TEACH YOU A THING OR TWO.";
    for (int i = 0; i < n; i++) {
      const Skill s = sk[i];
      if (g.skills.level(s) < cap && trainLeft(g) > 0) d.opts.push_back({"SHOW ME YOUR " + std::string(skills::skillInfo(s).name), K_SHOWME + (int)s, mi});
    }
    bye("THANKS. LET'S GO ON.");
    return true;
  }
  return false;
}

void SkillOps::shopStock(Game& g, const Actor& a, std::vector<Item>& stock) {
  // a scholar's shelf (a Mage-role keeper in a settlement) holds one skill book a restock, priced as the rare thing it is
  if (a.role != Role::Mage) return;
  const uint32_t h = mixh((uint32_t)g.npcKey(a) ^ 0xB00Cu, (uint32_t)(g.day / 2));
  static const Skill pool[] = {Skill::Sword, Skill::Axe, Skill::Mace, Skill::Spear, Skill::Dagger, Skill::Bow, Skill::Block, Skill::LightArmour,
                               Skill::MediumArmour, Skill::HeavyArmour, Skill::Heritage, Skill::Athletics, Skill::Smithing, Skill::Cooking,
                               Skill::Farming, Skill::Riding, Skill::Speech, Skill::Stealth};
  const Skill s = pool[h % (uint32_t)std::size(pool)];
  Item b = skills::makeSkillBook(s, h | 1u, std::max(1, g.plLevel));
  b.value = 150 + 10 * b.ilvl;
  stock.push_back(b);
}
bool SkillOps::readBook(Game& g, int i) {
  if (i < 0 || i >= (int)g.inv.size() || !skills::isSkillBook(g.inv[(size_t)i])) return false;
  const Item& it = g.inv[(size_t)i];
  const Skill s = (Skill)it.form;
  std::vector<uint32_t>& read = g.skills.booksRead;
  if (std::find(read.begin(), read.end(), it.seed) != read.end()) { g.say("YOU HAVE READ THIS ONE"); return false; }
  read.push_back(it.seed);
  if (s == Skill::Smithing) smithFromCraft(g);
  skills::Track& t = g.skills[s];
  if (t.level < skills::MAX_LEVEL) { t.level++; t.xp = 0; }
  if (s == Skill::Smithing) craftFromSmith(g);
  skillEv(g, s, 4, t.level, std::string(skills::skillInfo(s).name) + " RISES TO " + std::to_string((int)t.level));
  g.recalcPlayer();
  return true;
}
void SkillOps::loot(Game& g, Vec2 where, int D, gear::LootSrc src) {
  // (15.29.4) a skill book now and then among the loot: rare on a body, likelier in a chest, likeliest in a hoard or an
  // explorer's find. One hash of stable facts decides (no rng_ draw: the loot rolls keep their sequence)
  using S = gear::LootSrc;
  int permille = 0;
  switch (src) {
    case S::Body: permille = 6; break;
    case S::Chest: permille = 30; break;
    case S::Hoard: permille = 150; break;
    case S::Explore: permille = 200; break;
    case S::Boss: case S::WorldBoss: permille = 250; break;
    default: return;
  }
  const uint32_t h = mixh((uint32_t)g.seed ^ (uint32_t)(g.seed >> 32) ^ ((uint32_t)g.kills * 2654435761u),
                          (uint32_t)(int)std::floor(where.x) * 73856093u ^ (uint32_t)(int)std::floor(where.y) * 19349663u ^ (uint32_t)g.day * 83492791u ^ (uint32_t)src);
  if ((int)(h % 1000u) >= permille) return;
  static const Skill pool[] = {Skill::Sword, Skill::Axe, Skill::Mace, Skill::Spear, Skill::Dagger, Skill::Bow, Skill::Unarmed, Skill::Block,
                               Skill::LightArmour, Skill::MediumArmour, Skill::HeavyArmour, Skill::Heritage, Skill::Athletics, Skill::Smithing,
                               Skill::Cooking, Skill::Farming, Skill::Riding, Skill::Speech, Skill::Stealth};
  const uint32_t h2 = hash32(h ^ 0x51ABu);
  Pickup o;
  o.p = where + Vec2((float)((int)(h2 % 9u) - 4), (float)((int)((h2 >> 8) % 7u) - 3));
  o.item = skills::makeSkillBook(pool[(h2 >> 16) % (uint32_t)std::size(pool)], h2 | 1u, D);
  g.pickups.push_back(o);
}

// ---------------------------------------------------------------- the crafts
void SkillOps::forged(Game& g, Item& made) {
  const bool gear = itemEquippable(made.kind);
  if (gear) {
    // Smithing's improvement perks: the piece comes off the anvil stronger
    float mul = 1.0f;
    if (P(g, Skill::Smithing, SM_STEEL) && (made.mat == Mat::Iron || made.mat == Mat::Steel)) mul *= 1.1f;
    if (P(g, Skill::Smithing, SM_TEMPER)) mul *= 1.1f;
    if (P(g, Skill::Smithing, SM_MASTERWORK) && (made.mat == Mat::Alloy || made.culture)) mul *= 1.15f;
    if (mul > 1.0f && made.power > 0) {
      made.power = (int16_t)std::lround((float)made.power * mul);
      made.value = std::max(1, (int)std::lround((float)made.value * mul));
    }
  } else if (made.kind == ItemKind::Material && P(g, Skill::Smithing, SM_THRIFTY)) {
    const uint32_t h = mixh((uint32_t)g.craft.skill ^ (uint32_t)g.seed, (uint32_t)(g.time * 60.0f) ^ (uint32_t)g.inv.size() * 977u);
    if (h % 3u == 0) { made.count += 1; g.say("A SPARE INGOT FROM THE MELT"); }
  }
  g.skillUse(Skill::Smithing, gear ? 2.0f : 1.0f);
}
void SkillOps::studied(Game& g) { g.skillUse(Skill::Smithing, 1.0f); }
int SkillOps::studyMul(const Game& g) { return P(g, Skill::Smithing, SM_STUDY) ? 2 : 1; }
void SkillOps::cooked(Game& g, const Item& meal) { (void)meal; g.skillUse(Skill::Cooking, 1.0f); }
bool SkillOps::secondHelping(const Game& g) {
  if (!P(g, Skill::Cooking, CO_SECOND)) return false;
  return mixh((uint32_t)g.seed ^ (uint32_t)g.day, (uint32_t)(g.time * 60.0f) ^ g.skills.uses[(int)Skill::Cooking]) % 3u == 0;
}
float SkillOps::fedMul(const Game& g, const Item& food) { return (food.flags & IF_CRAFTED) && P(g, Skill::Cooking, CO_HEARTY) ? 2.0f : 1.0f; }
float SkillOps::mealHeal(const Game& g, const Item& food) { return (food.flags & IF_CRAFTED) && P(g, Skill::Cooking, CO_FIELD) ? 25.0f : 0.0f; }
void SkillOps::farmed(Game& g, int act) { g.skillUse(Skill::Farming, act >= 3 ? 1.0f : 0.5f); }
int SkillOps::harvestBonus(const Game& g) { return P(g, Skill::Farming, FA_GREEN) ? 1 : 0; }
bool SkillOps::rainMaker(const Game& g) { return P(g, Skill::Farming, FA_RAIN); }
int SkillOps::produceMul(const Game& g) { return P(g, Skill::Farming, FA_HUSBANDRY) ? 2 : 1; }
float SkillOps::horseMul(const Game& g) { return P(g, Skill::Riding, RI_HORSEMAN) ? 1.1f : 1.0f; }
int SkillOps::stableFee(const Game& g, int fee) { return P(g, Skill::Riding, RI_STABLE) ? fee / 2 : fee; }

// ---------------------------------------------------------------- tests and scripts
int SkillOps::spawnPerson(Game& g, Role role, Vec2 where, int level) {
  Spawn sp;
  sp.bandit = true;
  sp.site = -1;
  sp.slot = 7000 + g.nextId_ % 1000;
  const int id = g.spawnHuman(sp, where);
  const int k = g.findActor(id);
  if (k < 0) return -1;
  Actor& a = g.actors[(size_t)k];
  // the bandit's body at this level, nothing elite about it (a clean dummy for the time-to-kill table)
  a.rank = 0; a.affixes = 0; a.boss = false; a.ranged = false;
  a.fromMap = false; a.site = -1;   // (a staged body: no map slot keeps or culls it)
  a.maxHp = 46.0f; a.dmg = 9.0f; a.xp = 18;
  g.applyLevel(a, level);
  a.look.weapon = 1;
  if (role != Role::Bandit) {   // a man-at-arms: mail and a shield (what tagsOf reads from the look)
    a.role = role;
    a.look.shield = true; a.look.shieldStyle = 2; a.look.armorStyle = 2; a.look.helmet = true;
    a.name = role == Role::Guard ? "GUARD" : "SOLDIER";
  }
  a.aggro = true;
  return id;
}
void SkillOps::setLevel(Game& g, Skill s, int level) {
  if ((int)s >= skills::SKILLS) return;
  g.skills[s].level = (uint8_t)std::clamp(level, 1, skills::MAX_LEVEL);
  g.skills[s].xp = 0;
  if (s == Skill::Smithing) craftFromSmith(g);
  g.recalcPlayer();
}
bool SkillOps::takePerk(Game& g, Skill s, int idx, std::string* why) {
  const std::string b = skills::perkBlocked(g.skills, s, idx);
  if (!b.empty()) { if (why) *why = b; return false; }
  int n = 0;
  const skills::PerkDef* P_ = skills::perkTable(s, n);
  g.skills[s].perks |= 1u << idx;
  g.skills.perkPts = (uint16_t)(g.skills.perkPts - P_[idx].cost);
  g.skills.perksTaken++;
  skillEv(g, s, 3, (uint32_t)idx, std::string(P_[idx].name));
  g.recalcPlayer();
  return true;
}
int SkillOps::respecCost(const Game& g, Skill s) {
  int n = 0, taken = 0;
  skills::perkTable(s, n);
  for (int i = 0; i < n; i++) taken += g.skills[s].has(i);
  return taken == 0 ? 0 : 250 * taken * (1 + g.skills.respecs);
}
bool SkillOps::respec(Game& g, Skill s, std::string* why) {
  const int cost = respecCost(g, s);
  if (cost <= 0) { if (why) *why = "NO PERKS TO RETURN"; return false; }
  if (g.gold < cost) { if (why) *why = "NEEDS " + std::to_string(cost) + " GOLD"; return false; }
  int n = 0;
  const skills::PerkDef* P_ = skills::perkTable(s, n);
  int back = 0, count = 0;
  for (int i = 0; i < n; i++) if (g.skills[s].has(i)) { back += P_[i].cost; count++; }
  g.gold -= cost;
  g.skills[s].perks = 0;
  g.skills.perkPts = (uint16_t)(g.skills.perkPts + back);
  g.skills.perksTaken = (uint16_t)std::max(0, (int)g.skills.perksTaken - count);
  g.skills.respecs++;
  skillEv(g, s, 7, (uint32_t)back, std::string(skills::skillInfo(s).name) + ": PERKS RETURNED");
  g.recalcPlayer();
  return true;
}
