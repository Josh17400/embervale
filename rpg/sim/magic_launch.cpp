// M9 "Arcana & Mastery": the LAUNCH (SOLID 3.5, split out of magic_game.cpp). MagicOps::launch pays the price, works out
// the spell's numbers (its power, area and duration at this level and hand), says the Launch phase, and then the FORM
// happens: one MagicLaunch function per form group (a missile, an area at once, a lasting zone, a heal, a ward, an
// aura, a summon, a blink, a beam), chosen by the same switch on d.form as before. Neighbours: magic_game.cpp (the
// rest of the spell's life: wind-up, projectiles, zones, statuses, surfaces; the shared helpers in namespace
// magic_sim), rpg/sim/magic_ops.h (MagicOps), rpg/sim/magic.h (the spell data).
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "engine/sound_ids.h"
#include "rpg/sim/game.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/magic_ops.h"
#include "rpg/sim/power_ops.h"

using magic::Essence;
using magic::Form;
using magic::Phase;
using magic::School;
using magic::SpellDef;
using magic::Status;
using magic::Surface;

// ---- from magic_game.cpp (defined there, namespace magic_sim): the spell helpers the launch uses
namespace magic_sim {
bool perk(const Game& g, School s, int idx);
bool alive(const Actor& a);
bool foe(const Actor& a);
uint16_t tags(const Actor& a);
int tileX(float x);
int tileY(float y);
Vec2 handsOf(const Actor& a);
void feel(Game& g, Vec2 p, int size, float mag, bool stop = true);
void gain(Game& g, const SpellDef& d, float base);
void struckWith(Game& g, const SpellDef& d);
void spellStatuses(Game& g, Actor& v, const SpellDef& d, int owner, float mag);
void pay(Game& g, const SpellDef& d, float c);
float spellBlow(Game& g, Actor& v, const SpellDef& d, float dmg, Vec2 from, int owner, float mag);
void groundDisc(Game& g, Vec2 c, float r, const SpellDef& d, int owner, uint8_t power, bool leave);
art::Monster summonKind(const SpellDef& d);
int summon(Game& g, const SpellDef& d, Vec2 at, float mag, int level, art::Monster mon, float strengthMul);
int aimTarget(Game& g, const SpellDef& d, Vec2& aim, Vec2& point);
void heroBusy(Game& g, Vec2 aim);
void snare(Game& g, Actor& v, float secs);
void thornWall(Game& g, const SpellDef& d, const magic::Zone& z, Vec2 aim);
bool hiddenForm(Game& g, const SpellDef& d, int level, bool dual, float mag, float pw, float durK, float area, Vec2 aim, Vec2 point);
void singNote(Game& g, const SpellDef& d, int level, bool dual);
}  // namespace magic_sim
using namespace magic_sim;

namespace {
// the schools' perk indices the launch reads: a copy of magic_game.cpp's lines (magic_perks.cpp; saved bits, so they
// never change; wave 4 puts the one copy in a header)
enum : int { R_RESPITE = 2, R_RECOVERY = 3, R_WARDABSORB = 4, R_SUNWARDEN = 5, R_APPRENTICE = 6, R_WARMHANDS = 7, R_AVOIDDEATH = 8 };
enum : int { C_TWIN = 2, C_SUMMONER = 3, C_MYSTIC = 4, C_NECRO = 5, C_APPRENTICE = 6, C_ELEMENTAL = 7, C_HARVEST = 8 };
enum : int { I_QUIET = 2, I_ANIMAGE = 3, I_KINDRED = 4, I_HYPNOTIC = 5, I_APPRENTICE = 6, I_MASTERMIND = 7, I_TERROR = 8 };
enum : int { A_RESIST = 2, A_MAGEARMOUR = 3, A_STABILITY = 4, A_APPRENTICE = 5, A_FARBLINK = 6, A_SHAPER = 7, A_ATRONACH = 8 };
}  // namespace

// The form groups. A MagicOps underneath (so a name means here what it meant inside MagicOps::launch: emit, sigOf,
// attunedTo, applyStatus ... are MagicOps's), and Game's friend like MagicOps (game.h).
struct MagicLaunch : MagicOps {
  // one launch's numbers, as MagicOps::launch worked them out before the form happens
  struct Ctx {
    Game& g;
    const SpellDef& d;
    int level;
    bool dual;
    Actor& p;          // the caster (the hero)
    magic::Live& L;
    float mag, pw, area, durK;   // the magnitude, the power now, the area and duration factors
    Vec2 aim, point, hands;
    int tgt;           // the aimed-at actor index (-1 none)
    uint8_t spow;      // a surface's power
    int pid;           // the caster's id
  };
  static void formMissile(Ctx& lc);
  static void formBurst(Ctx& lc);
  static void formZone(Ctx& lc);
  static void formMend(Ctx& lc);
  static void formWard(Ctx& lc);
  static void formAura(Ctx& lc);
  static void formSummon(Ctx& lc);
  static void formBlink(Ctx& lc);
  static void formBeam(Ctx& lc);
};
bool MagicOps::launch(Game& g, const SpellDef& d, int slot, int level, bool dual) {
  Actor& p = g.pl();
  magic::Live& L = g.arcana.live;
  level = std::clamp(level, 0, 3);
  if (magic::legacySpellOf(d.id) >= 0) {
    const float mp0 = g.mp;
    classicCast(g, d, level, dual);
    if (g.mp < mp0 || (g.craft.live.stats.has(Unique::BloodMagic))) {
      if (slot >= 0) L.cd[slot] = d.cooldown;
      if (level) L.stats.upcasts++;
      if (dual) L.stats.duals++;
    }
    return true;
  }
  if (!L.refrain && !canPay(g, d, level, dual)) {
    emit(g, Phase::Fizzle, d.id, handsOf(p), p.p, 1, (uint32_t)p.id);
    L.stats.fizzles++;
    MagicOps::tell(g, d.res == magic::Resource::Health ? "TOO WEAK TO PAY IN BLOOD" : "NOT ENOUGH MAGICKA");
    return false;
  }
  const float mag = magic::magnitudeOf(level, dual);
  // (the power is the hero's as the cast begins: Blood rites reads the health before its price is paid)
  const float k = powerOf(g, d) * mag * (L.refrain ? magic::REFRAIN_MULT : 1.0f);   // (M10) a Refrain sings at x2
  if (!L.refrain) pay(g, d, costOf(g, d, level, dual));
  const float pw = d.power * k;   // the spell's power now (damage, a heal, a ward's points...)
  const float area = (1.0f + 0.15f * (float)level) * (dual ? 1.25f : 1.0f) * (dual && d.school == School::Illusion ? 1.2f : 1.0f);
  float durK = 1.0f + 0.25f * (float)level;
  if (d.school == School::Alteration && perk(g, School::Alteration, A_STABILITY)) durK *= 1.5f;
  if (dual && (d.school == School::Alteration || d.school == School::Illusion)) durK *= 1.3f;
  Vec2 aim, point;
  const int tgt = aimTarget(g, d, aim, point);
  const Vec2 hands = handsOf(p);
  if (slot >= 0) L.cd[slot] = d.cooldown;
  L.stats.casts++;
  if (level) L.stats.upcasts++;
  if (dual) L.stats.duals++;
  emit(g, Phase::Launch, d.id, hands, d.form == Form::Meteor || d.form == Form::Storm ? point : hands + aim * 24.0f, mag, (uint32_t)p.id);
  MagicOps::ev(g, Ev::Shake, p.p, 0, std::min(3.0f, 0.4f + 0.35f * (float)d.fx.size * mag));   // the launch's small kick
  // a spell is heard: foes near the caster turn on them (QUIET CASTING: they hear nothing)
  if (!perk(g, School::Illusion, I_QUIET))
    for (Actor& e : g.actors) if (foe(e) && !e.aggro && len2(e.p - p.p) < 110.0f * 110.0f) e.aggro = true;
  if (L.invisT > 0 && d.school != School::Illusion) { L.invisT = 0; MagicOps::tell(g, "YOU ARE SEEN"); }
  // Emberforge (6.2 #8): the tradition's spells imbue the weapon (the next 5 blows carry the essence)
  if (d.tradition && magic::traditionOfSpell(d).sig == magic::Signature::Ember) {
    L.imbueHits = 5; L.imbueEss = d.ess; L.imbuePow = d.power * 0.25f * k;
  }
  const uint8_t spow = (uint8_t)std::min(3, 1 + level + (dual ? 1 : 0));
  const int pid = p.id;
  // (M10) the hidden signatures' own forms: a spirit mask, a vine bridge, a decoy, a time bubble
  if (hiddenForm(g, d, level, dual, mag, pw, durK, area, aim, point)) {
    heroBusy(g, aim);
    singNote(g, d, level, dual);
    return true;
  }
  MagicLaunch::Ctx lc{g, d, level, dual, p, L, mag, pw, area, durK, aim, point, hands, tgt, spow, pid};
  switch (d.form) {
    case Form::Bolt: case Form::Lance: case Form::Orb: case Form::Mark: MagicLaunch::formMissile(lc); break;
    case Form::Nova: case Form::Cone: MagicLaunch::formBurst(lc); break;
    case Form::Wall: case Form::Rune: case Form::Totem: case Form::Storm: case Form::Meteor: MagicLaunch::formZone(lc); break;
    case Form::Mend: MagicLaunch::formMend(lc); break;
    case Form::Ward: MagicLaunch::formWard(lc); break;
    case Form::Aura: MagicLaunch::formAura(lc); break;
    case Form::Summon: MagicLaunch::formSummon(lc); break;
    case Form::Blink: MagicLaunch::formBlink(lc); break;
    case Form::Beam: MagicLaunch::formBeam(lc); break;
    default:
      emit(g, Phase::Impact, d.id, p.p, p.p, mag, (uint32_t)pid);
      gain(g, d, 1.0f);
      break;
  }
  heroBusy(g, aim);
  singNote(g, d, level, dual);   // (M10 Songweaving: its note, and the Refrain when the motif is sung)
  return true;
}

// a missile: Bolt, Lance, Orb, Mark (ProjKind::Spell; projHit / projEnd)
void MagicLaunch::formMissile(Ctx& lc) {
  Game& g = lc.g;
  const SpellDef& d = lc.d;
  const int level = lc.level;
  const bool dual = lc.dual;
  const float mag = lc.mag;
  const float pw = lc.pw;
  const Vec2 aim = lc.aim;
  const Vec2 hands = lc.hands;
  const int pid = lc.pid;
  Projectile pr;
  pr.kind = ProjKind::Spell; pr.spell = d.id; pr.mag = mag; pr.upcast = (uint8_t)level; pr.dual = dual;
  pr.p = hands + aim * 6; pr.v = aim * std::max(120.0f, d.speed);
  pr.dmg = d.form == Form::Mark && !magic::damaging(d) ? 0.0f : pw;
  pr.radius = (d.form == Form::Orb ? 6.0f : 4.0f) * (dual ? 1.3f : 1.0f);
  pr.life = std::max(0.3f, (d.range > 0 ? d.range : 200.0f) / std::max(120.0f, d.speed));
  pr.fromPlayer = true; pr.owner = pid; pr.from = pr.p; pr.fac = Faction::Player;
  g.projs.push_back(pr);
}

// an area at once: Nova round the caster, Cone from the hands
void MagicLaunch::formBurst(Ctx& lc) {
  Game& g = lc.g;
  const SpellDef& d = lc.d;
  const int level = lc.level;
  Actor& p = lc.p;
  magic::Live& L = lc.L;
  const float mag = lc.mag;
  const float pw = lc.pw;
  const float area = lc.area;
  const float durK = lc.durK;
  const Vec2 aim = lc.aim;
  const Vec2 hands = lc.hands;
  const uint8_t spow = lc.spow;
  const int pid = lc.pid;
  const bool cone = d.form == Form::Cone;
  const float R = (d.radius > 0 ? d.radius : 64.0f) * area;
  const Vec2 c = cone ? hands : p.p;
  emit(g, Phase::Impact, d.id, cone ? hands + aim * (R * 0.6f) : p.p, cone ? hands + aim * R : p.p + Vec2(R, 0), mag, (uint32_t)pid, cone ? "CONE" : "NOVA");
  feel(g, c, d.fx.size, mag);
  std::vector<int> ids;
  for (const Actor& e : g.actors) {
    if (!foe(e) || len2(e.p - c) > R * R) continue;
    if (cone && dot(norm(e.p - c + Vec2(0.001f, 0)), aim) < 0.64f) continue;   // (a 50-degree half-angle)
    ids.push_back(e.id);
  }
  int hit = 0;
  for (int id : ids) {
    const int kk = MagicOps::findActor(g, id);
    if (kk < 0) continue;
    Actor& e = g.actors[(size_t)kk];
    if (d.id == 19 && !(tags(e) & skills::FT_UNDEAD)) continue;   // TURN UNDEAD: only the dead
    if (magic::damaging(d)) spellBlow(g, e, d, pw, c, pid, mag);
    const int k2 = MagicOps::findActor(g, id);
    if (k2 < 0) continue;
    Actor& e2 = g.actors[(size_t)k2];
    spellStatuses(g, e2, d, pid, mag);
    if ((d.mods & magic::M_ROOT) && alive(e2)) e2.slowT = std::max(e2.slowT, 3.0f * durK);
    if ((d.mods & magic::M_ROOT) && alive(e2) && sigOf(d) == magic::Signature::Verdant) snare(g, e2, magic::SNARE_SECS * durK);
    if ((d.mods & magic::M_KNOCK) && alive(e2) && !e2.boss) e2.knock = norm(e2.p - c + Vec2(0.001f, 0)) * 240.0f;
    if (d.ess == Essence::Tide) applyStatus(g, e2, Status::Wet, 6.0f * durK, pid);
    hit++;
  }
  if (hit) { gain(g, d, (1.0f + 0.5f * (float)level) * std::min(3.0f, 1.0f + 0.5f * (float)(hit - 1))); struckWith(g, d); L.stats.hits += hit; }
  else if (!magic::damaging(d)) gain(g, d, 0.5f);
  if (cone) {
    for (int s = 1; s <= 4; s++) groundDisc(g, hands + aim * (R * (float)s / 4.0f) + Vec2(0, 8), R * 0.18f * (float)s, d, pid, spow, d.leaves != Surface::None);
  } else groundDisc(g, p.p, R, d, pid, spow, true);
  emit(g, Phase::Aftermath, d.id, c, c, mag * (float)d.fx.size, 0);
}

// a lasting zone: Wall, Rune, Totem, Storm, Meteor (Live::zones)
void MagicLaunch::formZone(Ctx& lc) {
  Game& g = lc.g;
  const SpellDef& d = lc.d;
  const int level = lc.level;
  Actor& p = lc.p;
  magic::Live& L = lc.L;
  const float mag = lc.mag;
  const float pw = lc.pw;
  const float area = lc.area;
  const float durK = lc.durK;
  const Vec2 aim = lc.aim;
  const Vec2 point = lc.point;
  const uint8_t spow = lc.spow;
  const int pid = lc.pid;
  magic::Zone z;
  z.spell = d.id; z.owner = pid; z.mag = mag; z.seed = hash2((int)point.x, (int)point.y, (uint32_t)L.stats.casts);
  z.r = (d.radius > 0 ? d.radius : 48.0f) * area;
  z.life = d.duration * durK;
  z.power = pw;
  if (d.form == Form::Wall) {
    z.kind = magic::ZoneKind::Burn;
    const Vec2 side(-aim.y, aim.x);
    const Vec2 c = p.p + aim * std::min(d.range, std::max(32.0f, len(point - p.p)));
    z.p = c - side * z.r; z.q = c + side * z.r;
    for (int s = 0; s <= 8; s++) {
      const Vec2 w = z.p + (z.q - z.p) * ((float)s / 8.0f);
      if (!touchSurface(g, tileX(w.x), tileY(w.y), d.ess, pid) && d.leaves != Surface::None) laySurface(g, tileX(w.x), tileY(w.y), d.leaves, z.life, spow, pid);
    }
    if (sigOf(d) == magic::Signature::Verdant) thornWall(g, d, z, aim);   // (M10) the thorns stand, solid, for 30 s
    emit(g, Phase::Impact, d.id, z.p, z.q, mag, (uint32_t)pid, "WALL");
  } else if (d.form == Form::Rune) {
    z.kind = magic::ZoneKind::Rune; z.p = z.q = point;
    emit(g, Phase::Aftermath, d.id, point, point, mag, (uint32_t)pid, "RUNE");
  } else if (d.form == Form::Totem) {
    z.kind = magic::damaging(d) ? magic::ZoneKind::Totem : magic::ZoneKind::Heal;
    z.p = z.q = z.kind == magic::ZoneKind::Heal ? p.p : point;
    emit(g, Phase::Summon, d.id, z.p, z.p + Vec2(0, -20), mag, (uint32_t)pid, "TOTEM");
  } else if (d.form == Form::Storm) {
    z.kind = magic::ZoneKind::Storm; z.p = z.q = point; z.tick = 0.4f;
    emit(g, Phase::Channel, d.id, point + Vec2(0, -160), point, mag, (uint32_t)pid, "STORM");
  } else {
    z.kind = magic::ZoneKind::Meteor; z.p = point; z.q = point + Vec2(-70, -240); z.delay = 0.9f; z.life = 1.0f;
  }
  L.zones.push_back(z);
  if (d.form != Form::Meteor && d.form != Form::Storm) gain(g, d, 0.5f + 0.5f * (float)level);
}

// a heal (GRAND HEALING: the companions within its reach too)
void MagicLaunch::formMend(Ctx& lc) {
  Game& g = lc.g;
  const SpellDef& d = lc.d;
  const int level = lc.level;
  Actor& p = lc.p;
  const float mag = lc.mag;
  const float pw = lc.pw;
  const float area = lc.area;
  const int pid = lc.pid;
  const float before = p.hp;
  const float heal = pw * (attunedTo(g, magic::Signature::Blood) ? magic::BLOOD_HEAL : 1.0f);   // (M10 Blood rites: healing halved)
  p.hp = std::min(p.maxHp, p.hp + heal);
  if (perk(g, School::Restoration, R_RESPITE)) g.stamina = std::min(g.maxSt, g.stamina + pw * 0.5f);
  emit(g, Phase::Impact, d.id, p.p, p.p + Vec2(0, -24), mag, (uint32_t)pid, "HEAL");
  if (d.radius > 0)   // GRAND HEALING: the companions within its reach too
    for (Actor& a : g.actors) if (a.comp && alive(a) && len2(a.p - p.p) < d.radius * d.radius * area * area) {
      a.hp = std::min(a.maxHp, a.hp + pw);
      emit(g, Phase::Impact, d.id, a.p, a.p + Vec2(0, -24), mag * 0.7f, (uint32_t)a.id, "HEAL");
    }
  if (d.ess != Essence::Light) for (Actor& a : g.actors) if (a.player || a.comp) a.status &= (uint16_t)~(magic::statusBit(Status::Burning) | magic::statusBit(Status::Poisoned));
  gain(g, d, (p.hp > before ? 1.0f : 0.3f) * (1.0f + 0.5f * (float)level));
}

// a ward that takes the blows first
void MagicLaunch::formWard(Ctx& lc) {
  Game& g = lc.g;
  const SpellDef& d = lc.d;
  Actor& p = lc.p;
  magic::Live& L = lc.L;
  const float mag = lc.mag;
  const float pw = lc.pw;
  const float durK = lc.durK;
  const int pid = lc.pid;
  L.ward = std::max(L.ward, pw); L.wardT = d.duration * durK; L.wardSpell = d.id;
  emit(g, Phase::Ward, d.id, p.p + Vec2(0, -8), p.p, mag, (uint32_t)pid, "RAISED");
  gain(g, d, 1.0f);
}

// an aura by essence: armour, regeneration, a bound blade, blur, invisibility
void MagicLaunch::formAura(Ctx& lc) {
  Game& g = lc.g;
  const SpellDef& d = lc.d;
  Actor& p = lc.p;
  magic::Live& L = lc.L;
  const float mag = lc.mag;
  const float pw = lc.pw;
  const float durK = lc.durK;
  const int pid = lc.pid;
  const float secs = d.duration * durK * (perk(g, School::Alteration, A_MAGEARMOUR) && d.school == School::Alteration ? 2.0f : 1.0f);
  switch (d.ess) {
    case Essence::Stone: case Essence::Bone: case Essence::Rust:
      L.auraDR = std::min(0.6f, d.power * 1.5f * 0.01f * mag); L.auraT = secs; L.auraSpell = d.id; break;
    case Essence::Light: case Essence::Verdant: case Essence::Tide:
      L.regen = pw * 0.5f * (attunedTo(g, magic::Signature::Blood) ? magic::BLOOD_HEAL : 1.0f); L.regenT = secs; break;
    case Essence::Arcane: case Essence::Fire:
      L.boundDmg = pw * 0.8f * (perk(g, School::Conjuration, C_MYSTIC) ? 1.5f : 1.0f); L.boundT = secs;
      if (d.ess == Essence::Fire) { L.imbueHits = std::max(L.imbueHits, 8); L.imbueEss = Essence::Fire; L.imbuePow = std::max(L.imbuePow, pw * 0.3f); }
      break;
    case Essence::Dream: case Essence::Wind:
      L.blur = std::min(0.6f, d.power * 2.5f * 0.01f * mag); L.blurT = secs; break;
    case Essence::Shadow:
      L.invisT = secs;
      for (Actor& e : g.actors) if (foe(e) && !e.boss) { e.aggro = false; e.target = -1; }
      break;
    default: L.auraDR = std::min(0.6f, d.power * 0.01f * mag); L.auraT = secs; L.auraSpell = d.id; break;
  }
  emit(g, Phase::Ward, d.id, p.p + Vec2(0, -8), p.p, mag, (uint32_t)pid, "AURA");
  gain(g, d, 1.0f);
}

// a summon through a rift (a host of ancestors: three echoes)
void MagicLaunch::formSummon(Ctx& lc) {
  Game& g = lc.g;
  const SpellDef& d = lc.d;
  const int level = lc.level;
  Actor& p = lc.p;
  const float mag = lc.mag;
  const Vec2 aim = lc.aim;
  const int pid = lc.pid;
  const Vec2 at = p.p + aim * std::max(20.0f, d.range * 0.6f);
  emit(g, Phase::Impact, d.id, at, at + Vec2(0, -30), mag, (uint32_t)pid, "RIFT");
  feel(g, at, d.fx.size, mag);
  const int n = d.rank >= 3 && d.ess == Essence::Bone ? 3 : 1;   // a host of ancestors: three echoes
  for (int i = 0; i < n; i++) {
    const Vec2 off = n > 1 ? Vec2(std::cos((float)i * 2.1f) * 16.0f, std::sin((float)i * 2.1f) * 10.0f) : Vec2();
    summon(g, d, at + off, mag, level, summonKind(d), n > 1 ? 0.6f : 1.0f);
  }
  gain(g, d, 1.0f + 0.5f * (float)level);
}

// a blink to the farthest free spot along the way
void MagicLaunch::formBlink(Ctx& lc) {
  Game& g = lc.g;
  const SpellDef& d = lc.d;
  Actor& p = lc.p;
  const float mag = lc.mag;
  const float area = lc.area;
  const int tgt = lc.tgt;
  const int pid = lc.pid;
  const float reach = d.range * (perk(g, School::Alteration, A_FARBLINK) ? 1.5f : 1.0f) * area;
  Vec2 dir = p.aim;
  if (tgt >= 0) dir = norm(g.actors[(size_t)tgt].p - p.p + Vec2(0.001f, 0)) * -1.0f;   // (away from the foe in front)
  Vec2 to = p.p;
  for (int s = 16; s >= 1; s--) {
    const Vec2 c = p.p + dir * (reach * (float)s / 16.0f);
    if (MagicOps::fits(g, c, p.radius)) { to = c; break; }
  }
  emit(g, Phase::Blink, d.id, p.p, to, mag, (uint32_t)pid);
  p.p = to;
  p.iframes = std::max(p.iframes, 0.25f);
  gain(g, d, 1.0f);
}

// a beam held in concentration (Live::conc)
void MagicLaunch::formBeam(Ctx& lc) {
  Game& g = lc.g;
  const SpellDef& d = lc.d;
  magic::Live& L = lc.L;
  const float mag = lc.mag;
  const float pw = lc.pw;
  const float durK = lc.durK;
  const Vec2 aim = lc.aim;
  const Vec2 hands = lc.hands;
  const int pid = lc.pid;
  if (L.conc.spell) releaseConc(g, false);
  magic::Conc& C = L.conc;
  C = magic::Conc();
  C.spell = d.id; C.left = d.duration * durK; C.mag = mag; C.power = pw; C.caster = pid; C.tick = 0;
  emit(g, Phase::Channel, d.id, hands, hands + aim * d.range, mag, (uint32_t)pid, "BEGIN");
}