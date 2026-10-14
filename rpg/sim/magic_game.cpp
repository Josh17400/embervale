// M9 "Arcana & Mastery": magic's Game hooks (MagicOps, rpg/sim/magic_ops.h). MAGIC lane: the sim of every spell's life.
//
// The pipeline: a quick slot is tapped or held (input / begin) -> a wind-up while the cast time runs, upcast levels
// poured in while held (Ev::Spell WindUp, Charge) -> launch: the cost paid in the spell's resource, the Launch phase,
// then the FORM happens: a missile (Bolt / Lance / Orb / Mark, ProjKind::Spell, projHit / projEnd), an area at once
// (Nova / Cone), a lasting zone (Wall / Rune / Totem / Storm / Meteor: Live::zones), a self effect (Mend / Ward / Aura /
// Blink), a summon (an actor with Actor::magicTag MT_SUMMON), a beam held in concentration (Live::conc). Every impact
// sets the game feel (Game::hitStop, Ev::Shake by magnitude) and says Impact / Aftermath; surfaces combo with what the
// essence touches; statuses tick with real effects. The view (SPELLFX lane) draws and sounds all of it from Ev::Spell
// and the FxStyle: the sim plays no spell sound.
// The classic three (Firebolt, Mend, Frost Lance) keep their pre-M9 numbers exactly at a plain cast (classicCast; the
// --powers firebolt ratios); they now fly as ProjKind::Spell with the classic hit, burst and burn.
// Determinism: tradition data is generated in magic.cpp from the culture; in play, only combat rolls draw from rng_
// (a storm's strike, a blinded foe's miss), each draw on its own line.
#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>
#include <unordered_map>
#include "engine/audio.h"
#include "rpg/sim/game.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/magic_ops.h"
#include "rpg/sim/power_ops.h"
#include "rpg/world/biomes.h"

using art::Prop;
using magic::Essence;
using magic::Form;
using magic::Phase;
using magic::School;
using magic::SpellDef;
using magic::Status;
using magic::Surface;

namespace {
constexpr float PI_F = 3.14159265f;

bool perk(const Game& g, School s, int idx) { return g.skills.has(magic::schoolSkill(s), idx); }
// the schools' perk indices (magic_perks.cpp; saved bits)
enum : int { P_NOVICE = 0, P_DUAL = 1 };
enum : int { D_FLAMES = 2, D_FROST = 3, D_IMPACT = 4, D_SHOCK = 5, D_APPRENTICE = 6, D_ADEPT = 7, D_INTENSE = 8, D_DEEPFREEZE = 9, D_DISINTEGRATE = 10 };
enum : int { R_RESPITE = 2, R_RECOVERY = 3, R_WARDABSORB = 4, R_SUNWARDEN = 5, R_APPRENTICE = 6, R_WARMHANDS = 7, R_AVOIDDEATH = 8 };
enum : int { C_TWIN = 2, C_SUMMONER = 3, C_MYSTIC = 4, C_NECRO = 5, C_APPRENTICE = 6, C_ELEMENTAL = 7, C_HARVEST = 8 };
enum : int { I_QUIET = 2, I_ANIMAGE = 3, I_KINDRED = 4, I_HYPNOTIC = 5, I_APPRENTICE = 6, I_MASTERMIND = 7, I_TERROR = 8 };
enum : int { A_RESIST = 2, A_MAGEARMOUR = 3, A_STABILITY = 4, A_APPRENTICE = 5, A_FARBLINK = 6, A_SHAPER = 7, A_ATRONACH = 8 };

bool alive(const Actor& a) { return a.st != AState::Dead && !a.indoors; }
bool foe(const Actor& a) { return a.hostile && alive(a); }
uint16_t tags(const Actor& a) { return skills::tagsOf(a); }
int tileX(float x) { return (int)std::floor(x / TILE); }
int tileY(float y) { return (int)std::floor((y - 2) / TILE); }
Vec2 handsOf(const Actor& a) { return a.p + Vec2(0, -8); }

Essence essenceOfSurface(Surface s) {
  switch (s) {
    case Surface::Fire: return Essence::Fire;
    case Surface::Ice: return Essence::Frost;
    case Surface::Shock: return Essence::Storm;
    case Surface::Poison: return Essence::Venom;
    case Surface::Water: return Essence::Tide;
    case Surface::Steam: return Essence::Wind;
    case Surface::Blood: return Essence::Blood;
    default: return Essence::Arcane;
  }
}
float surfaceLife(Surface s) {
  switch (s) {
    case Surface::Fire: return 6;
    case Surface::Shock: return 4;
    case Surface::Steam: return 5;
    case Surface::Poison: return 8;
    case Surface::Ice: return 10;
    case Surface::Oil: return 20;
    case Surface::Water: return 15;
    default: return 10;
  }
}

// the feel of an impact: a few frames of hit-stop and a shake, light to heavy by its size and magnitude
void feel(Game& g, Vec2 p, int size, float mag, bool stop = true) {
  const float k = (float)std::max(1, size) * mag;
  if (stop) g.hitStop = std::max(g.hitStop, std::min(0.09f, 0.012f + 0.011f * k));
  MagicOps::ev(g, Ev::Shake, p, 0, std::min(7.0f, 0.8f + 0.9f * k));
}

// the school skill gain (only the hero's own casts) and a tradition's mastery when it strikes a foe
void gain(Game& g, const SpellDef& d, float base) { g.skillUse(magic::schoolSkill(d.school), base); }
void struckWith(Game& g, const SpellDef& d) {
  if (!d.tradition) return;
  MagicOps::gainMastery(g, d.tradition, 1);
  g.arcana.live.tradHitT[d.tradition] = g.time;
}

// the foe levels a spell of the mind reaches (Skyrim's rule: novice FEAR works on the low, master MAYHEM on most)
bool mindReaches(const Game& g, const SpellDef& d, float mag, const Actor& v) {
  if (v.boss || v.rank >= (uint8_t)foes::Rank::WorldBoss || v.mon == art::Monster::Dragon) return false;
  const uint16_t t = tags(v);
  if ((t & (skills::FT_UNDEAD | skills::FT_CONSTRUCT)) && !perk(g, School::Illusion, I_MASTERMIND)) return false;
  float cap = d.power * 0.5f * mag + 0.5f * (float)g.plLevel;
  if ((t & skills::FT_BEAST) && perk(g, School::Illusion, I_ANIMAGE)) cap += 10;
  if ((t & skills::FT_PEOPLE) && perk(g, School::Illusion, I_KINDRED)) cap += 10;
  return (float)v.level <= cap;
}

// the hero's statuses / a spell's statuses on a victim (longer when upcast)
void spellStatuses(Game& g, Actor& v, const SpellDef& d, int owner, float mag) {
  if (!d.statuses || d.statusSecs <= 0) return;
  for (int s = 0; s < magic::STATUSES; s++) {
    if (!(d.statuses & (1u << s))) continue;
    const Status st = (Status)s;
    if (st == Status::Charmed) continue;   // (charm is a turning: MagicOps' turn below)
    float secs = d.statusSecs * (0.75f + 0.25f * mag);
    if ((st == Status::Frightened || st == Status::Blinded) && owner == g.pl().id && perk(g, School::Illusion, I_HYPNOTIC)) secs *= 1.5f;
    if (st == Status::Frightened && d.school == School::Illusion && !mindReaches(g, d, mag, v)) continue;
    if (st == Status::Frightened && d.id == 19 && !(tags(v) & skills::FT_UNDEAD)) continue;   // TURN UNDEAD
    MagicOps::applyStatus(g, v, st, secs, owner);
  }
}

}  // namespace

// ================================================================== events, the book's mirror
void MagicOps::emit(Game& g, Phase ph, uint32_t spell, Vec2 p, Vec2 q, float f, uint32_t id, const std::string& s) {
  Event e;
  e.type = Ev::Spell;
  e.a = (int)ph;
  e.spell = spell;
  e.p = p; e.q = q; e.f = f; e.id = id; e.s = s;
  g.events.push_back(e);
}

void MagicOps::syncLegacy(Game& g) {
  int spell = (int)g.spell;
  magic::syncLegacy(g.arcana.book, g.spellsKnown, spell, g.arcana.live.legacyMirror);
  g.spell = (Spell)spell;
}

bool MagicOps::learn(Game& g, uint32_t id, bool announce) {
  const SpellDef* d = magic::spellById(id);
  if (!d || !g.arcana.book.learn(id)) return false;
  syncLegacy(g);
  if (announce) { MagicOps::tell(g, "LEARNED " + d->name); MagicOps::snd(g, (int)Sfx::LevelUp, g.pl().p, 1.3f); }
  return true;
}

void MagicOps::reset(Game& g) {
  g.arcana = magic::Arcana();
  g.spellsKnown = 0;
  g.spell = Spell::Flames;
}
void MagicOps::background(Game& g) { syncLegacy(g); }

// ================================================================== costs and power
float MagicOps::costOf(const Game& g, const SpellDef& d, int level, bool dual) {
  float c = d.cost;
  if (d.res == magic::Resource::Charge) return std::max(1.0f, c + (float)(level > 0 ? 1 : 0) + (dual ? 1.0f : 0.0f));
  // the schools' cost perks: NOVICE (tier 0 half), APPRENTICE (tier 1 half), Destruction's ADEPT (tier 2+: -25 %)
  const School sc = d.school;
  if (d.tier == 0 && perk(g, sc, P_NOVICE)) c *= 0.5f;
  const int appr = sc == School::Destruction ? D_APPRENTICE : sc == School::Restoration ? R_APPRENTICE : sc == School::Conjuration ? C_APPRENTICE
                   : sc == School::Illusion ? I_APPRENTICE : A_APPRENTICE;
  if (d.tier == 1 && perk(g, sc, appr)) c *= 0.5f;
  if (d.tier >= 2 && sc == School::Destruction && perk(g, sc, D_ADEPT)) c *= 0.75f;
  if (g.craft.live.stats.has(Unique::Arcanist) && d.res == magic::Resource::Magicka) c *= 0.8f;
  c *= 1.0f + 0.5f * (float)std::clamp(level, 0, 3);
  if (dual) c *= 2.0f;
  return c;
}
bool MagicOps::canPay(const Game& g, const SpellDef& d, int level, bool dual) {
  const float c = costOf(g, d, level, dual);
  if (d.res == magic::Resource::Magicka) {
    if (g.craft.live.stats.has(Unique::BloodMagic)) return g.pl().hp > c + 1;
    return g.mp >= c;
  }
  return g.arcana.book.res[(int)d.res] >= c;
}
namespace {
void pay(Game& g, const SpellDef& d, float c) {
  if (d.res == magic::Resource::Magicka) {
    if (g.craft.live.stats.has(Unique::BloodMagic)) { g.pl().hp -= c; g.events.push_back({Ev::Blood, g.pl().p + Vec2(0, -6), 0, 1, ""}); }
    else g.mp -= c;
    return;
  }
  float& r = g.arcana.book.res[(int)d.res];
  r = std::max(0.0f, r - c);
}
}  // namespace

bool MagicOps::fullMoon(const Game& g) {
  if (g.arcana.live.moonOverride >= 0) return g.arcana.live.moonOverride > 0;
  return magic::fullMoonDay(g.day);
}
bool MagicOps::raining(const Game& g) {
  const magic::Live& L = g.arcana.live;
  if (L.rainOverride >= 0) return L.rainOverride > 0;
  if (g.inside || g.map().kind != MapKind::Overworld) return false;
  // the same pick as the view's weather (render.cpp): the land's Sky at the hero's tile, this day's 3-hour slot
  const int slot = g.day * 8 + (int)(g.hour / 3);
  const uint32_t w = hash32((uint32_t)slot ^ (uint32_t)g.seed) % 100;
  const Eco e = g.world.over.ecoAt((int)std::floor(g.pl().p.x / 16), (int)std::floor(g.pl().p.y / 16));
  switch (ecoInfo(e).sky) {
    case Sky::Temperate: return w < 22;
    case Sky::Showery: return w < 45;
    case Sky::Misty: return w < 12;
    case Sky::Dry: return w < 5;
    case Sky::Monsoon: return w < 60;
    default: return false;
  }
}
bool MagicOps::nearWater(const Game& g, Vec2 p, int r) {
  const Map& m = g.map();
  const int cx = tileX(p.x), cy = tileY(p.y);
  for (int y = cy - r; y <= cy + r; y++)
    for (int x = cx - r; x <= cx + r; x++) {
      if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > r * r) continue;
      if (m.in(x, y) && groundWater(m.at(x, y))) return true;
      const Surface s = g.arcana.live.field.at(x, y);
      if (s == Surface::Water || s == Surface::Shock) return true;
    }
  return false;
}
bool MagicOps::nearGraves(const Game& g, Vec2 p) {
  // a dungeon of the dead (a ruin, a cave's barrow) or graves and cairns within 6 tiles
  if (g.inside && g.subSite >= 0 && g.subSite < (int)g.world.sites.size()) {
    const SiteType t = g.world.sites[(size_t)g.subSite].type;
    if (t == SiteType::Ruin || t == SiteType::Cave) return true;
  }
  const Map& m = g.map();
  const int cx = tileX(p.x), cy = tileY(p.y);
  for (int y = cy - 6; y <= cy + 6; y++)
    for (int x = cx - 6; x <= cx + 6; x++) {
      const int pp = m.propAt(x, y);
      if (!pp) continue;
      const Prop pr = (Prop)(pp - 1);
      if (pr == Prop::GraveCairn || pr == Prop::NamedGrave) return true;
    }
  return false;
}
float MagicOps::signatureMul(const Game& g, const SpellDef& d) {
  if (!d.tradition) return 1.0f;
  const magic::Tradition& t = magic::traditionOfSpell(d);
  switch (t.sig) {
    case magic::Signature::Star: return g.isNight() && fullMoon(g) ? magic::STAR_FULL_MOON : 1.0f;
    case magic::Signature::Tide: return nearWater(g, g.pl().p, 4) || raining(g) ? magic::TIDE_MULT : 1.0f;
    case magic::Signature::Ancestor: return nearGraves(g, g.pl().p) ? magic::ANCESTOR_MULT : 1.0f;
    default: return 1.0f;
  }
}
float MagicOps::powerOf(const Game& g, const SpellDef& d) {
  const gear::GearStats& S6 = g.craft.live.stats;
  const float sp = (g.eqStaff >= 0 && g.eqStaff < (int)g.inv.size() ? gear::usePower(g.inv[(size_t)g.eqStaff], g.plLevel) : 0.0f) + (float)S6.get(Affix::SpellPower);
  float k = gear::playerDamageMul(g.plLevel) + sp * 0.02f;
  if (S6.has(Unique::Arcanist)) k *= 1.2f;
  if (S6.has(Unique::BloodMagic) && d.res == magic::Resource::Magicka) k *= 1.4f;
  if (d.school == School::Destruction) {
    if (d.ess == Essence::Fire && perk(g, School::Destruction, D_FLAMES)) k *= 1.25f;
    if (d.ess == Essence::Frost && perk(g, School::Destruction, D_FROST)) k *= 1.25f;
    if (d.ess == Essence::Storm && perk(g, School::Destruction, D_SHOCK)) k *= 1.25f;
  }
  if (d.form == Form::Mend && perk(g, School::Restoration, R_WARMHANDS)) k *= 1.4f;
  return k * signatureMul(g, d);
}

// ================================================================== damage from a spell
namespace {
// a spell's blow on a victim: the essence against its tags and statuses (light burns the dead, storm leaps through
// water, arcane unmakes spirits), then Game::damage (armour, wards, the frozen shatter in hurtMul)
float spellBlow(Game& g, Actor& v, const SpellDef& d, float dmg, Vec2 from, int owner, float mag) {
  if (!alive(v) || dmg <= 0) return 0;
  const uint16_t t = tags(v);
  if (d.ess == Essence::Light && (t & skills::FT_UNDEAD)) dmg *= owner == g.pl().id && perk(g, School::Restoration, R_SUNWARDEN) ? 2.0f : 1.5f;
  if (d.ess == Essence::Arcane && (t & (skills::FT_SPIRIT | skills::FT_UNDEAD))) dmg *= 1.5f;
  if (d.ess == Essence::Storm && v.hasStatus(Status::Wet)) {   // water conducts: the shock runs through the soaked
    dmg *= 1.5f;
    v.status &= (uint16_t)~magic::statusBit(Status::Wet);
    v.statusT[(int)Status::Wet] = 0;
    MagicOps::emit(g, Phase::Reaction, d.id, v.p + Vec2(0, -6), v.p, mag, (uint32_t)v.id, "CONDUCTED");
  }
  if (v.hasStatus(Status::Frightened) && owner == g.pl().id && perk(g, School::Illusion, I_TERROR)) dmg *= 1.2f;
  const float hp0 = v.hp;
  const int vid = v.id;
  magic::Live& L = g.arcana.live;
  L.spellDepth++;
  MagicOps::hit(g, v, dmg, from, owner);
  L.spellDepth--;
  const int k = MagicOps::findActor(g, vid);
  if (k < 0) return hp0;
  Actor& w = g.actors[(size_t)k];
  const float dealt = hp0 - std::max(0.0f, w.hp);
  // DISINTEGRATE: storm finishes a foe left below 15 %
  if (alive(w) && d.ess == Essence::Storm && owner == g.pl().id && perk(g, School::Destruction, D_DISINTEGRATE) && w.hp < w.maxHp * 0.15f && !w.boss) {
    L.spellDepth++;
    MagicOps::hit(g, w, w.hp + 1, from, owner);
    L.spellDepth--;
  }
  return dealt;
}
}  // namespace

// ================================================================== surfaces
namespace {
// a tile's surface becomes another by a combo (Ev::Spell Combo, s = the new surface; electrified water runs through
// the whole puddle at once)
void combine(Game& g, magic::SurfaceTile& t, Surface c, uint8_t power, int owner) {
  magic::Live& L = g.arcana.live;
  magic::Field& F = L.field;
  const Surface was = t.kind;
  const Vec2 at((float)t.tx * TILE + 8, (float)t.ty * TILE + 10);
  L.stats.combos++;
  MagicOps::emit(g, Phase::Combo, 0, at, at, (float)std::max<int>(power, t.power), (uint32_t)std::max(0, owner), magic::surfaceName(c));
  t.kind = c; t.life = surfaceLife(c); t.age = 0; t.power = std::max(t.power, power); t.owner = owner;
  F.serial++;
  L.stats.surfacesLaid[(int)c]++;
  if (c != Surface::Shock || was == Surface::Shock) return;
  std::vector<std::pair<int, int>> open{{t.tx, t.ty}};
  int n = 0;
  while (!open.empty() && n < 48) {
    const std::pair<int, int> q = open.back();
    open.pop_back();
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++)
      for (magic::SurfaceTile& o : F.tiles) {
        if (o.tx != q.first + dx[k] || o.ty != q.second + dy[k]) continue;
        if (o.kind == Surface::Water || o.kind == Surface::Blood) {
          o.kind = Surface::Shock; o.life = surfaceLife(Surface::Shock); o.age = 0; o.owner = owner;
          open.push_back({o.tx, o.ty});
          n++;
        }
        break;
      }
  }
}
}  // namespace

bool MagicOps::touchSurface(Game& g, int tx, int ty, Essence e, int owner) {
  magic::Field& F = g.arcana.live.field;
  for (magic::SurfaceTile& t : F.tiles) {
    if (t.tx != tx || t.ty != ty) continue;
    if (t.kind == Surface::Steam && e == Essence::Wind) { t.life = 0; F.serial++; return true; }   // the wind blows the steam away
    const Surface c = magic::comboOf(t.kind, e);
    if (c == Surface::None) return false;
    combine(g, t, c, t.power, owner);
    return true;
  }
  return false;
}

void MagicOps::laySurface(Game& g, int tx, int ty, Surface s, float life, uint8_t power, int owner) {
  if (s == Surface::None || (int)s >= (int)Surface::COUNT) return;
  const Map& m = g.map();
  if (!m.in(tx, ty) || m.blocked(tx, ty)) return;
  magic::Live& L = g.arcana.live;
  magic::Field& F = L.field;
  if (owner >= 0 && owner == g.pl().id && perk(g, School::Alteration, A_SHAPER)) life *= 2.0f;
  for (magic::SurfaceTile& t : F.tiles) {
    if (t.tx != tx || t.ty != ty) continue;
    if (t.kind == s) { t.life = std::max(t.life, life); t.power = std::max(t.power, power); return; }
    // a different surface already lies here: the two combine when they can (water + fire = steam, oil + fire = fire...)
    const Surface c = magic::comboOf(t.kind, essenceOfSurface(s));
    if (c != Surface::None) { combine(g, t, c, power, owner); return; }
    t.kind = s; t.life = life; t.age = 0; t.power = std::max(t.power, power); t.owner = owner;   // (no combo: the new one covers the old)
    F.serial++;
    L.stats.surfacesLaid[(int)s]++;
    return;
  }
  if (F.tiles.size() >= 900) return;   // (a cap: the view's budget)
  magic::SurfaceTile t;
  t.tx = (int16_t)tx; t.ty = (int16_t)ty; t.kind = s; t.life = life; t.power = power; t.owner = owner;
  t.seed = hash2(tx, ty, (uint32_t)g.seed);
  F.tiles.push_back(t);
  F.serial++;
  L.stats.surfacesLaid[(int)s]++;
}

namespace {
// a spell's essence on the ground of a disc: combos with what lies there, then what it leaves on the rest
void groundDisc(Game& g, Vec2 c, float r, const SpellDef& d, int owner, uint8_t power, bool leave) {
  const int x0 = tileX(c.x - r), x1 = tileX(c.x + r), y0 = (int)std::floor((c.y - r) / TILE), y1 = (int)std::floor((c.y + r) / TILE);
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      const Vec2 tc((float)x * TILE + 8, (float)y * TILE + 8);
      if (len2(tc - c) > r * r) continue;
      if (MagicOps::touchSurface(g, x, y, d.ess, owner)) continue;
      if (leave && d.leaves != Surface::None) MagicOps::laySurface(g, x, y, d.leaves, std::max(2.0f, d.duration), power, owner);
    }
}
}  // namespace

// ================================================================== statuses
void MagicOps::applyStatus(Game& g, Actor& a, Status s, float secs, int by) {
  if ((int)s >= magic::STATUSES || secs <= 0 || a.st == AState::Dead) return;
  if (a.player && g.godMode) return;
  if (a.magicTag == magic::MT_SUMMON && by == a.magicBy) return;   // (the caster's own spells spare its summons)
  const bool fresh = !a.hasStatus(s);
  // the classic opposites: fire dries the wet and thaws the frozen; frost puts the burning out
  if (s == Status::Burning) {
    if (a.hasStatus(Status::Wet)) { a.status &= (uint16_t)~magic::statusBit(Status::Wet); a.statusT[(int)Status::Wet] = 0; return; }
    a.status &= (uint16_t)~(magic::statusBit(Status::Frozen) | magic::statusBit(Status::Chilled));
  }
  if (s == Status::Chilled || s == Status::Frozen) a.status &= (uint16_t)~magic::statusBit(Status::Burning);
  if (s == Status::Wet) a.status &= (uint16_t)~magic::statusBit(Status::Burning);
  if (s == Status::Chilled && a.hasStatus(Status::Wet)) { s = Status::Frozen; secs = std::max(secs, 2.5f); }   // wet and chilled: frozen
  if (s == Status::Chilled && a.hasStatus(Status::Chilled) && by == g.pl().id && perk(g, School::Destruction, D_DEEPFREEZE)) { s = Status::Frozen; secs = 2.0f; }
  if (s == Status::Frozen && (a.boss || a.mon == art::Monster::Dragon)) { s = Status::Chilled; }
  if (s == Status::Frightened && (a.boss || a.player)) return;
  const bool fresh2 = !a.hasStatus(s);
  a.status |= magic::statusBit(s);
  for (int k = 0; k < magic::STATUSES; k++) if (!(a.status & (1u << k))) a.statusT[k] = 0;
  a.statusT[(int)s] = std::max(a.statusT[(int)s], secs);
  a.statusBy = by;
  if (fresh || fresh2) {
    g.arcana.live.stats.statusApplied[(int)s]++;
    emit(g, Phase::StatusOn, 0, a.p, a.p, 1, (uint32_t)a.id, magic::statusName(s));
  }
}

namespace {
// a status's tick on any body (the hero too): burning and poison hurt, chill slows, frozen holds, shocked twitches
bool statusTick(Game& g, Actor& a, float dt) {
  bool held = false;
  for (int s = 0; s < magic::STATUSES; s++) {
    if (!(a.status & (1u << s))) continue;
    float& t = a.statusT[s];
    t -= dt;
    if (t <= 0) { t = 0; a.status &= (uint16_t)~(1u << s); continue; }
    switch ((Status)s) {
      case Status::Burning: a.hp -= dt * (2.0f + 0.5f * (float)a.level); a.regen = 0; break;
      case Status::Poisoned: a.hp -= dt * (1.5f + 0.4f * (float)a.level); break;
      case Status::Chilled: a.slowT = std::max(a.slowT, 0.1f); break;
      case Status::Frozen: held = true; a.slowT = std::max(a.slowT, 0.1f); break;
      case Status::Shocked: if (std::fmod(t, 0.5f) < 0.12f) held = true; break;   // the stun ticks
      default: break;
    }
  }
  return held;
}
}  // namespace

// ================================================================== summons, turned foes
namespace {
art::Monster summonKind(const SpellDef& d) {
  using M = art::Monster;
  switch (d.ess) {
    case Essence::Bone: return d.tradition ? M::Draugr : M::Skeleton;
    case Essence::Fire: return M::EmberHound;
    case Essence::Storm: return M::Golem;
    case Essence::Verdant: return M::Bear;
    case Essence::Wind: return M::Harpy;
    case Essence::Star: return M::Wisp;
    case Essence::Shadow: return d.tier >= 4 || d.rank >= 3 ? M::Troll : M::Wolf;
    default: return M::Wolf;
  }
}
int summonsOf(const Game& g, int caster) {
  int n = 0;
  for (const Actor& a : g.actors) n += a.magicTag == magic::MT_SUMMON && a.magicBy == caster && alive(a);
  return n;
}
void fade(Game& g, Actor& a) {
  MagicOps::emit(g, Phase::Release, 0, a.p, a.p, 1, (uint32_t)a.id, "FADES");
  a.hp = 0; a.st = AState::Dead; a.stT = 99.0f; a.magicTag = magic::MT_NONE; a.status = 0;
}
// a summoned form climbs out of its rift (returns its actor id). mon: the kind (a raised foe's own)
int summon(Game& g, const SpellDef& d, Vec2 at, float mag, int level, art::Monster mon, float strengthMul) {
  const Actor me = g.pl();
  const int cap = perk(g, School::Conjuration, C_TWIN) ? 2 : 1;
  // at the cap, the oldest summon fades for the new one (a host of ancestors calls three that share one cap)
  while (summonsOf(g, me.id) >= cap + (d.rank >= 3 && d.ess == Essence::Bone ? 2 : 0)) {
    int oldest = -1;
    for (size_t i = 1; i < g.actors.size(); i++) {
      const Actor& w = g.actors[i];
      if (w.magicTag != magic::MT_SUMMON || w.magicBy != me.id || !alive(w)) continue;
      if (oldest < 0 || w.magicT < g.actors[(size_t)oldest].magicT) oldest = (int)i;
    }
    if (oldest < 0) break;
    fade(g, g.actors[(size_t)oldest]);
  }
  const float pm = gear::playerDamageMul(g.plLevel);
  Actor w;
  w.id = MagicOps::newId(g);
  w.npc = false; w.hostile = false; w.human = false; w.faction = Faction::Player;
  w.mon = mon;
  w.name = d.name;
  w.magicTag = magic::MT_SUMMON;
  float life = d.duration * (1.0f + 0.25f * (float)level);
  if (perk(g, School::Conjuration, P_DUAL) && mag > 1.55f) life *= 2.0f;
  w.magicT = life;
  w.magicBy = me.id;
  w.level = std::max(1, g.plLevel);
  float hpK = 3.0f, dmK = 0.28f;
  if (perk(g, School::Conjuration, C_SUMMONER)) hpK *= 1.4f;
  if ((d.ess == Essence::Fire || d.ess == Essence::Storm) && perk(g, School::Conjuration, C_ELEMENTAL)) dmK *= 1.5f;
  if (d.ess == Essence::Bone && perk(g, School::Conjuration, C_NECRO)) dmK *= 1.5f;
  w.maxHp = w.hp = std::max(20.0f, d.power * hpK * pm * mag * strengthMul);
  w.dmg = std::max(2.0f, d.power * dmK * pm * mag * strengthMul);
  w.armor = 1.5f * (float)w.level;
  w.speed = mon == art::Monster::Golem || mon == art::Monster::Troll ? 62.0f : mon == art::Monster::Skeleton || mon == art::Monster::Draugr ? 70.0f : 92.0f;
  w.radius = mon == art::Monster::Troll || mon == art::Monster::Golem || mon == art::Monster::Bear ? 7.0f : 4.5f;
  w.range = w.radius + 11.0f; w.windup = 0.22f; w.aggroR = 160.0f;
  if (!MagicOps::fits(g, at, w.radius)) at = me.p + Vec2(me.face == 3 ? -14.0f : 14.0f, 4.0f);
  if (!MagicOps::fits(g, at, w.radius)) at = me.p;
  w.p = at; w.home = at; w.goal = at;
  w.face = me.face;
  w.bodyTint = d.fx.ramp[2];
  w.scalePct = (uint8_t)(d.fx.size >= 4 ? 150 : d.fx.size >= 3 ? 120 : 100);
  w.iframes = 0.4f;
  w.xp = 0;
  const int id = w.id;
  g.actors.push_back(w);
  MagicOps::emit(g, Phase::Summon, d.id, at, at + Vec2(0, -24), mag, (uint32_t)id, magic::formName(d.form));
  return id;
}
// a foe turned to the hero's side (charm, a bound beast, mayhem): it fights the hero's foes until it ends
void turn(Game& g, Actor& v, const SpellDef& d, float secs, bool conc, uint8_t tag) {
  magic::Live& L = g.arcana.live;
  for (magic::Turned& t : L.turned) if (t.id == v.id) { t.t = std::max(t.t, secs); return; }
  magic::Turned t;
  t.id = v.id; t.faction = (uint8_t)v.faction; t.hostile = v.hostile; t.t = secs; t.spell = d.id; t.conc = conc;
  L.turned.push_back(t);
  v.faction = Faction::Player; v.hostile = false; v.aggro = false; v.target = -1;
  v.magicTag = tag; v.magicT = secs; v.magicBy = g.pl().id;
  v.status |= magic::statusBit(Status::Charmed);
  v.statusT[(int)Status::Charmed] = secs;
  L.stats.statusApplied[(int)Status::Charmed]++;
  MagicOps::emit(g, Phase::StatusOn, d.id, v.p, v.p, 1, (uint32_t)v.id, magic::statusName(Status::Charmed));
}
void unturn(Game& g, magic::Turned& t) {
  const int k = MagicOps::findActor(g, t.id);
  if (k < 0) return;
  Actor& v = g.actors[(size_t)k];
  v.faction = (Faction)t.faction; v.hostile = t.hostile;
  v.magicTag = magic::MT_NONE; v.magicT = 0;
  v.status &= (uint16_t)~magic::statusBit(Status::Charmed);
  v.statusT[(int)Status::Charmed] = 0;
  if (v.hostile) { v.aggro = true; v.target = g.pl().id; }
  MagicOps::emit(g, Phase::Release, t.spell, v.p, v.p, 1, (uint32_t)v.id, "COMES TO");
}
// a body fighting for the hero (a summon, a charmed foe): the nearest foe near the hero, else at the hero's heel. Its
// blows are dealt by its own hand; what it kills is the hero's kill (XP, the count)
void allyStep(Game& g, Actor& a, float dt) {
  const Actor& master = g.pl();
  if (a.st == AState::Hurt) { if (a.stT > 0.2f) { a.st = AState::Idle; a.stT = 0; } return; }
  int best = -1;
  float bd = 1e30f;
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& e = g.actors[i];
    if (!foe(e) || e.id == a.id) continue;
    const float dm = len2(e.p - master.p), d2 = len2(e.p - a.p);
    if (dm > 220.0f * 220.0f || d2 > 180.0f * 180.0f) continue;
    if (d2 < bd) { bd = d2; best = (int)i; }
  }
  if (a.st == AState::Windup) {
    if (a.stT >= a.windup) {
      a.st = AState::Strike; a.stT = 0;
      if (best >= 0) {
        Actor& t = g.actors[(size_t)best];
        if (len(t.p - a.p) <= a.range + t.radius + 4.0f) {
          const bool was = alive(t);
          const int tid = t.id;
          const float tx = (float)t.xp;
          const int tl = t.level;
          MagicOps::hit(g, t, a.dmg, a.p, a.id);
          const int k = MagicOps::findActor(g, tid);
          if (was && (k < 0 || g.actors[(size_t)k].st == AState::Dead)) {
            if (tx > 0) MagicOps::giveXp(g, std::max(1, (int)std::lround(tx * gear::xpDecay(g.plLevel, tl) * gear::earlyXpMul(g.plLevel))));
            g.kills++;
            if (a.magicTag == magic::MT_SUMMON && perk(g, School::Conjuration, C_HARVEST)) g.mp = std::min(g.maxMp, g.mp + 10.0f);
          }
        }
      }
    }
    return;
  }
  if (a.st == AState::Strike) {
    if (a.stT > 0.18f) { a.st = AState::Idle; a.stT = 0; a.atkCd = 0.7f; }
    return;
  }
  const float slow = a.slowT > 0 ? 0.6f : 1.0f;
  if (best >= 0) {
    const Actor& t = g.actors[(size_t)best];
    const Vec2 d = t.p - a.p;
    const float l = len(d);
    a.aim = norm(d + Vec2(0.001f, 0));
    a.face = faceOf(a.aim);
    if (l > a.range + t.radius - 3.0f) {
      MagicOps::move(g, a, a.aim * (a.speed * slow * dt));
      if (a.st != AState::Walk) { a.st = AState::Walk; a.stT = 0; }
    } else if (a.atkCd <= 0) {
      a.st = AState::Windup; a.stT = 0;
    } else if (a.st != AState::Idle) { a.st = AState::Idle; a.stT = 0; }
    return;
  }
  const Vec2 heel = master.p + Vec2(master.face == 3 ? 16.0f : -16.0f, 6.0f);
  const Vec2 d = heel - a.p;
  const float l = len(d);
  if (l > 280.0f && a.magicTag == magic::MT_SUMMON) { a.p = heel; return; }
  if (l > 12.0f) {
    a.aim = norm(d);
    a.face = faceOf(a.aim);
    MagicOps::move(g, a, a.aim * (a.speed * (l > 70.0f ? 1.4f : 1.0f) * slow * dt));
    if (a.st != AState::Walk) { a.st = AState::Walk; a.stT = 0; }
  } else if (a.st != AState::Idle) { a.st = AState::Idle; a.stT = 0; a.face = master.face; }
}
}  // namespace

bool MagicOps::actorStep(Game& g, Actor& a, float dt) {
  if (a.magicTag == magic::MT_SUMMON) {
    a.burnT = 0; a.burnDpsT = 0; a.bleedT = 0;   // (a conjured body neither burns nor bleeds)
    a.magicT -= dt;
    if (a.magicT <= 0 || MagicOps::findActor(g, a.magicBy) < 0) { fade(g, a); return true; }
    if (a.status) statusTick(g, a, dt);
    allyStep(g, a, dt);
    return true;
  }
  if (!a.status) return false;
  const bool held = statusTick(g, a, dt);
  if (a.hp <= 0 && a.st != AState::Dead) {
    const int by = a.statusBy >= 0 && MagicOps::findActor(g, a.statusBy) >= 0 ? a.statusBy : g.pl().id;
    MagicOps::killRaw(g, a, by);
    return true;
  }
  return held;
}

bool MagicOps::aiOverride(Game& g, Actor& a, float dt) {
  magic::Live& L = g.arcana.live;
  if (a.magicTag == magic::MT_CHARM || a.magicTag == magic::MT_BOUND) { allyStep(g, a, dt); return true; }
  if (a.magicTag == magic::MT_PARALYZE) {
    if ((a.magicT -= dt) > 0) { a.vel = Vec2(); if (a.st != AState::Hurt) { a.st = AState::Idle; a.stT = 0; } return true; }
    a.magicTag = magic::MT_NONE;
  }
  if (a.magicTag == magic::MT_CALM) {
    if ((a.magicT -= dt) > 0) {
      a.aggro = false; a.target = -1;
      if (a.st == AState::Windup || a.st == AState::Walk) { a.st = AState::Idle; a.stT = 0; }
      return true;
    }
    a.magicTag = magic::MT_NONE;
  }
  if (a.status) {
    if (a.hasStatus(Status::Frozen)) return true;
    if (a.hasStatus(Status::Shocked) && std::fmod(a.statusT[(int)Status::Shocked], 0.5f) < 0.12f) return true;
    const bool panic = a.hasStatus(Status::Frightened) ||
                       (a.hasStatus(Status::Burning) && a.hp < a.maxHp * 0.3f && a.statusBy == g.pl().id && perk(g, School::Destruction, D_INTENSE));
    if (panic && !a.boss) {
      const int k = a.statusBy >= 0 ? MagicOps::findActor(g, a.statusBy) : 0;
      const Actor& from = g.actors[(size_t)(k >= 0 ? k : 0)];
      const Vec2 away = norm(a.p - from.p + Vec2(0.001f, 0));
      MagicOps::move(g, a, away * (a.speed * 0.9f * dt));
      a.face = faceOf(away);
      if (a.st != AState::Walk) { a.st = AState::Walk; a.stT = 0; }
      return true;
    }
    // blinded: it loses the hero unless the hero is at arm's length, and gropes about
    if (a.hasStatus(Status::Blinded) && a.hostile && !a.boss && len2(g.pl().p - a.p) > 30.0f * 30.0f) {
      a.aggro = false; a.target = -1;
      const float ang = a.statusT[(int)Status::Blinded] * 2.3f + (float)(a.id % 7);
      MagicOps::move(g, a, Vec2(std::cos(ang), std::sin(ang)) * (a.speed * 0.25f * dt));
      if (a.st == AState::Windup) { a.st = AState::Idle; a.stT = 0; }
      return true;
    }
  }
  // the hero is invisible: what hunts them has lost them (they stand and search)
  if (L.invisT > 0 && a.hostile && !a.boss && a.mon != art::Monster::Dragon) {
    a.aggro = false; a.target = -1;
    if (a.st == AState::Windup || a.st == AState::Walk) { a.st = AState::Idle; a.stT = 0; }
    return true;
  }
  return false;
}

// ================================================================== missiles
namespace {
// the chain's leap: up to n more foes within 70 px, each blow at x0.7 of the last (Ev::Spell Impact p = the foe, q =
// the link it leapt from)
void chainFrom(Game& g, const SpellDef& d, Vec2 from, int firstId, float dmg, int owner, float mag, int n) {
  std::vector<int> struck{firstId};
  Vec2 at = from;
  for (int k = 0; k < n; k++) {
    dmg *= 0.7f;
    int best = -1;
    float bd = 70.0f * 70.0f;
    for (size_t i = 1; i < g.actors.size(); i++) {
      const Actor& e = g.actors[i];
      if (!foe(e) || std::find(struck.begin(), struck.end(), e.id) != struck.end()) continue;
      const float d2 = len2(e.p - at);
      if (d2 < bd) { bd = d2; best = (int)i; }
    }
    if (best < 0) break;
    Actor& e = g.actors[(size_t)best];
    struck.push_back(e.id);
    const Vec2 to = e.p + Vec2(0, -7);
    MagicOps::emit(g, Phase::Impact, d.id, to, at, mag * 0.7f, (uint32_t)std::max(0, owner));
    const int eid = e.id;
    spellBlow(g, e, d, dmg, at, owner, mag);
    const int ek = MagicOps::findActor(g, eid);
    if (ek >= 0) spellStatuses(g, g.actors[(size_t)ek], d, owner, mag);
    MagicOps::touchSurface(g, tileX(to.x), tileY(to.y + 7), d.ess, owner);
    at = to;
  }
}
// a mark's work on what it struck: the mind (calm, charm), stone (paralysis), bone (raised when it falls), verdant
// (a beast bound)
void markHit(Game& g, Actor& v, const SpellDef& d, float mag, int owner) {
  const float secs = d.duration * (0.75f + 0.25f * mag) * (perk(g, School::Illusion, I_HYPNOTIC) && d.school == School::Illusion ? 1.5f : 1.0f);
  switch (d.ess) {
    case Essence::Dream:
      if (!mindReaches(g, d, mag, v)) { MagicOps::emit(g, Phase::Fizzle, d.id, v.p + Vec2(0, -8), v.p, mag, (uint32_t)v.id, "RESISTS"); return; }
      if (d.concentration || (d.statuses & magic::statusBit(Status::Charmed))) {
        turn(g, v, d, secs, d.concentration, magic::MT_CHARM);
        if (d.concentration) {
          magic::Conc& C = g.arcana.live.conc;
          if (C.spell && C.spell != d.id) MagicOps::releaseConc(g, false);
          C = magic::Conc();
          C.spell = d.id; C.left = secs; C.target = v.id; C.mag = mag; C.caster = owner;
        }
      } else {
        v.magicTag = magic::MT_CALM; v.magicT = secs; v.magicBy = owner;
        v.aggro = false; v.target = -1;
        MagicOps::emit(g, Phase::StatusOn, d.id, v.p, v.p, mag, (uint32_t)v.id, "CALMED");
      }
      break;
    case Essence::Stone:
      if (v.boss) return;
      v.magicTag = magic::MT_PARALYZE; v.magicT = std::min(secs, 8.0f); v.magicBy = owner;
      MagicOps::emit(g, Phase::StatusOn, d.id, v.p, v.p, mag, (uint32_t)v.id, "PARALYZED");
      break;
    case Essence::Bone:
      if (v.boss || v.human) return;
      v.magicTag = magic::MT_RAISE; v.magicT = secs; v.magicBy = owner;
      MagicOps::emit(g, Phase::StatusOn, d.id, v.p, v.p, mag, (uint32_t)v.id, "MARKED");
      break;
    case Essence::Verdant:   // Beastbinding (6.2 #9): a non-boss beast no higher than the hero's level, 60 s an ally
      if (!(tags(v) & skills::FT_BEAST) || v.boss || v.level > g.plLevel) {
        MagicOps::emit(g, Phase::Fizzle, d.id, v.p + Vec2(0, -8), v.p, mag, (uint32_t)v.id, "RESISTS");
        return;
      }
      turn(g, v, d, std::max(60.0f, secs), false, magic::MT_BOUND);
      break;
    default: break;
  }
}
}  // namespace

bool MagicOps::projHit(Game& g, Projectile& pr, Actor& v) {
  const SpellDef* d = magic::spellById(pr.spell);
  if (!d) return false;
  const bool mine = pr.owner == g.pl().id;
  if (mine) { gain(g, *d, 1.0f + 0.5f * (float)pr.upcast); struckWith(g, *d); }
  g.arcana.live.stats.hits++;
  const Vec2 from = pr.p - norm(pr.v) * 4.0f;
  const int vid = v.id;
  if (magic::legacySpellOf(d->id) >= 0 || pr.kind != ProjKind::Spell) {
    // the classic hit (Firebolt / Frost Lance, a companion's bolt): the old blow with its enchantment
    if (pr.kind != ProjKind::Spell) return false;
    MagicOps::hit(g, v, pr.dmg, from, pr.owner, pr.ench, pr.enchPow);
  } else if (d->form == Form::Mark) {
    if (pr.dmg > 0) spellBlow(g, v, *d, pr.dmg, from, pr.owner, pr.mag);
    const int k = MagicOps::findActor(g, vid);
    if (k >= 0 && alive(g.actors[(size_t)k])) markHit(g, g.actors[(size_t)k], *d, pr.mag, pr.owner);
  } else {
    spellBlow(g, v, *d, pr.dmg, from, pr.owner, pr.mag);
    const int k = MagicOps::findActor(g, vid);
    if (k >= 0) {
      Actor& w = g.actors[(size_t)k];
      spellStatuses(g, w, *d, pr.owner, pr.mag);
      if ((d->mods & magic::M_KNOCK) && alive(w) && !w.boss) w.knock = norm(pr.v) * 260.0f * std::min(1.6f, pr.mag);
      if ((d->mods & magic::M_DRAIN) && mine) g.pl().hp = std::min(g.pl().maxHp, g.pl().hp + pr.dmg * 0.3f);
      if (pr.dual && mine && perk(g, d->school, d->school == School::Destruction ? D_IMPACT : P_DUAL) && alive(w) && !w.boss) { w.st = AState::Hurt; w.stT = 0; }
    }
    if (d->mods & magic::M_CHAIN) chainFrom(g, *d, v.p + Vec2(0, -7), vid, pr.dmg, pr.owner, pr.mag, 2 + (int)pr.upcast);
  }
  return true;
}

void MagicOps::projEnd(Game& g, const Projectile& pr) {
  const SpellDef* d = magic::spellById(pr.spell);
  if (!d) return;
  const float mag = pr.mag;
  const int owner = pr.owner;
  const Vec2 at = pr.p;
  emit(g, Phase::Impact, pr.spell, at, at - norm(pr.v) * 8.0f, mag, (uint32_t)std::max(0, owner));
  feel(g, at, d->fx.size, mag, owner == g.pl().id);
  const int tx = tileX(at.x), ty = (int)std::floor((at.y + 8) / TILE);
  const bool classic = magic::legacySpellOf(d->id) >= 0;
  if (pr.kind == ProjKind::Spell && d->id == magic::SPELL_FIREBOLT) {
    // the classic Firebolt's burst (the old Fireball's: 40 % of the blow to the others within 22 px, a fire enchantment)
    const float R = 22.0f * (1.0f + 0.15f * (float)pr.upcast) * (pr.dual ? 1.25f : 1.0f);
    for (Actor& a : g.actors)
      if (a.hostile && a.st != AState::Dead && len2(a.p - at) < R * R && len2(a.p - at) > 4) MagicOps::hit(g, a, pr.dmg * 0.4f, at, owner, Ench::Fire, pr.enchPow * 0.5f);
  }
  if (pr.kind == ProjKind::Spell && (d->form == Form::Orb) && d->radius > 0) {
    // an orb bursts where it lands: its full blow on every foe in the radius but the one it struck (already hit)
    const float R = d->radius * (1.0f + 0.15f * (float)pr.upcast) * (pr.dual ? 1.25f : 1.0f);
    std::vector<int> ids;
    for (const Actor& a : g.actors) if (foe(a) && len2(a.p - at) < R * R && len2(a.p - at) > 36.0f) ids.push_back(a.id);
    for (int id : ids) {
      const int k = MagicOps::findActor(g, id);
      if (k < 0) continue;
      spellBlow(g, g.actors[(size_t)k], *d, pr.dmg * 0.6f, at, owner, mag);
      const int k2 = MagicOps::findActor(g, id);
      if (k2 >= 0) spellStatuses(g, g.actors[(size_t)k2], *d, owner, mag);
    }
    groundDisc(g, at + Vec2(0, 8), R, *d, owner, (uint8_t)std::min(3, 1 + (int)pr.upcast), true);
  } else {
    // the essence touches the ground it lands on (combos); a classic spell leaves its surface only when upcast or
    // dual-cast (a plain cast keeps the pre-M9 numbers)
    const bool combo = touchSurface(g, tx, ty, d->ess, owner);
    if (!combo && d->leaves != Surface::None && pr.kind == ProjKind::Spell && (!classic || pr.upcast > 0 || pr.dual))
      laySurface(g, tx, ty, d->leaves, std::max(2.0f, d->duration * 2.0f), (uint8_t)std::min(3, 1 + (int)pr.upcast), owner);
  }
  emit(g, Phase::Aftermath, pr.spell, at + Vec2(0, 8), at, mag * (float)d->fx.size, 0);
}

// ================================================================== a blow on anyone
float MagicOps::hurtMul(Game& g, Actor& v, float dmg, int attacker, Ench ench) {
  magic::Live& L = g.arcana.live;
  const int atk = attacker >= 0 ? MagicOps::findActor(g, attacker) : -1;
  const Actor* A = atk >= 0 ? &g.actors[(size_t)atk] : nullptr;
  // a blinded attacker swings at shadows: half its blows miss
  if (A && A->hasStatus(Status::Blinded) && !A->player && dmg > 0) {
    const float roll = MagicOps::rng(g).f();
    if (roll < 0.5f) { MagicOps::ev(g, Ev::Text, v.p + Vec2(0, -20), (int)rgba(230, 230, 200), 0, "MISS"); return 0; }
  }
  if (v.player && dmg > 0) {
    // blur: a share of the blows find nothing
    if (L.blurT > 0 && A && !A->player) {
      const float roll = MagicOps::rng(g).f();
      if (roll < L.blur) { MagicOps::ev(g, Ev::Text, v.p + Vec2(0, -20), (int)rgba(240, 160, 230), 0, "MISS"); return 0; }
    }
    // MAGIC RESISTANCE (Alteration): foes' spells do 15 % less; ATRONACH: a share of them comes back as magicka
    const bool foeSpell = A && !A->player && (ench != Ench::None || (A->ranged && !A->human));
    if (foeSpell && perk(g, School::Alteration, A_RESIST)) dmg *= 0.85f;
    if (foeSpell && perk(g, School::Alteration, A_ATRONACH)) g.mp = std::min(g.maxMp, g.mp + dmg * 0.15f);
    // oakflesh / stoneskin / ironflesh / dragonhide
    if (L.auraT > 0 && L.auraDR > 0) dmg *= 1.0f - L.auraDR;
    // a ward soaks first
    if (L.wardT > 0 && L.ward > 0) {
      const float soak = std::min(L.ward, dmg);
      L.ward -= soak;
      dmg -= soak;
      if (perk(g, School::Restoration, R_WARDABSORB)) g.mp = std::min(g.maxMp, g.mp + soak * 0.5f);
      emit(g, Phase::Ward, L.wardSpell, v.p + Vec2(0, -8), A ? A->p : v.p, std::min(2.0f, soak / 10.0f + 0.5f), (uint32_t)v.id, L.ward <= 0 ? "BROKEN" : "STRUCK");
      if (L.ward <= 0) { L.wardT = 0; L.ward = 0; }
    }
    // AVOID DEATH (Restoration): once a day a killing blow leaves the hero standing, healed
    if (dmg >= v.hp && perk(g, School::Restoration, R_AVOIDDEATH) && L.avoidDeathDay != (float)g.day) {
      L.avoidDeathDay = (float)g.day;
      dmg = std::max(0.0f, v.hp - 1.0f);
      v.hp = std::min(v.maxHp, v.hp + 120.0f);
      emit(g, Phase::Impact, magic::SPELL_MEND, v.p, v.p, 1.5f, (uint32_t)v.id, "AVOID DEATH");
    }
  }
  // the hero's own blows (not a spell's): Emberforge's heat, a bound blade, an imbued weapon
  if (A && A->player && L.spellDepth == 0 && dmg > 0 && v.hostile && len2(v.p - A->p) < 48.0f * 48.0f) {
    if (L.boundT > 0) dmg += L.boundDmg;
    if (L.imbueHits > 0) {
      L.imbueHits--;
      dmg += L.imbuePow;
      const Status st = L.imbueEss == Essence::Frost ? Status::Chilled : L.imbueEss == Essence::Storm ? Status::Shocked : L.imbueEss == Essence::Venom ? Status::Poisoned : Status::Burning;
      applyStatus(g, v, st, 3.0f, A->id);
      emit(g, Phase::Reaction, 0, v.p + Vec2(0, -8), A->p, 0.8f, (uint32_t)A->id, "IMBUED");
    }
    for (uint32_t tid : g.arcana.book.attuned) {
      const magic::Tradition* t = magic::traditionById(tid);
      if (t && t->sig == magic::Signature::Ember) {
        float& heat = g.arcana.book.res[(int)magic::Resource::Heat];
        heat = std::min(magic::HEAT_CAP, heat + 7.0f);
        L.heatIdle = 0;
      }
    }
  }
  // a summon is not killed: it fades (no body, no loot); hurt() below lets it go
  if (v.magicTag == magic::MT_SUMMON && dmg >= v.hp) dmg = std::max(0.0f, v.hp - 0.01f);
  if (v.hasStatus(Status::Frozen) && dmg > 0) {   // a frozen body shatters out of the ice: the blow lands harder
    v.status &= (uint16_t)~magic::statusBit(Status::Frozen);
    v.statusT[(int)Status::Frozen] = 0;
    emit(g, Phase::Combo, 0, v.p + Vec2(0, -8), v.p, 1.2f, (uint32_t)v.id, "SHATTER");
    return dmg * 1.5f;
  }
  return dmg;
}
void MagicOps::hurt(Game& g, Actor& v, float dealt, int attacker) {
  magic::Live& L = g.arcana.live;
  magic::Conc& C = L.conc;
  if (v.player && C.spell && dealt > v.maxHp * 0.15f) releaseConc(g, true);
  if (v.magicTag == magic::MT_SUMMON && v.hp <= 0.02f) { v.hp = 0.01f; v.magicT = 0; }   // (it fades next step: actorStep)
  if (v.magicTag == magic::MT_CALM && attacker == g.pl().id) { v.magicTag = magic::MT_NONE; v.aggro = true; v.target = attacker; }
}

void MagicOps::releaseConc(Game& g, bool broken) {
  magic::Live& L = g.arcana.live;
  magic::Conc& C = L.conc;
  if (!C.spell) return;
  const Actor& p = g.pl();
  emit(g, Phase::Release, C.spell, handsOf(p), p.p, C.mag, (uint32_t)p.id, broken ? "BROKEN" : "");
  for (magic::Turned& t : L.turned) if (t.conc && t.spell == C.spell) t.t = 0;
  for (magic::Zone& z : L.zones) if (z.conc && z.spell == C.spell) z.life = 0;
  if (broken) { L.stats.concBroken++; MagicOps::tell(g, "CONCENTRATION BROKEN"); }
  C = magic::Conc();
}

// ================================================================== casting
void MagicOps::cast(Game& g) { begin(g, g.arcana.book.sel, false, false); }

bool MagicOps::begin(Game& g, int slot, bool held, bool dual) {
  syncLegacy(g);
  Actor& p = g.pl();
  magic::Book& B = g.arcana.book;
  magic::Live& L = g.arcana.live;
  if (slot < 0 || slot >= magic::QUICK_SLOTS) slot = B.sel;
  const uint32_t id = B.slot[(size_t)slot];
  const SpellDef* d = id ? magic::spellById(id) : nullptr;
  if (!d || !B.knows(id)) {
    MagicOps::tell(g, B.known.empty() ? "YOU KNOW NO SPELLS YET" : "SWAP TO A SPELL YOU KNOW");
    MagicOps::snd(g, (int)Sfx::MenuBack, p.p);
    return false;
  }
  B.sel = (uint8_t)slot;
  if (d->tradition && !attuned(g, d->tradition)) {
    MagicOps::tell(g, "ATTUNE TO " + magic::traditionOfSpell(*d).name + " TO CAST IT");
    MagicOps::snd(g, (int)Sfx::MenuBack, p.p);
    return false;
  }
  // a concentration spell cast again lets it go
  if (d->concentration && L.conc.spell == id) { releaseConc(g, false); return true; }
  if (dual && !d->dual) dual = false;
  if (dual && !perk(g, d->school, P_DUAL)) { MagicOps::tell(g, std::string("DUAL-CASTING ") + magic::schoolName(d->school) + " NEEDS ITS DUAL CASTING PERK"); dual = false; }
  if (p.shootCd > 0) return false;
  if (L.cd[slot] > 0) {
    MagicOps::tell(g, d->name + " READY IN " + std::to_string((int)std::ceil(L.cd[slot])) + " S");
    return false;
  }
  if (!canPay(g, *d, 0, dual)) {
    MagicOps::tell(g, d->res == magic::Resource::Magicka ? "NOT ENOUGH MAGICKA" : std::string("NOT ENOUGH ") + (d->res == magic::Resource::Heat ? "HEAT" : d->res == magic::Resource::Charge ? "CHARGE" : "STARLIGHT"));
    MagicOps::snd(g, (int)Sfx::MenuBack, p.p);
    emit(g, Phase::Fizzle, id, handsOf(p), p.p, 1, (uint32_t)p.id);
    L.stats.fizzles++;
    return false;
  }
  if (held || d->castTime > 0) {
    magic::Cast& C = L.cast;
    C = magic::Cast();
    C.spell = id; C.slot = (int8_t)slot; C.t = 0; C.level = 0; C.dual = dual; C.held = held && d->upcast; C.aim = p.aim;
    p.st = AState::Cast; p.stT = 0;
    emit(g, Phase::WindUp, id, handsOf(p), handsOf(p) + p.aim * 16.0f, magic::magnitudeOf(0, dual), (uint32_t)p.id);
    return true;
  }
  return launch(g, *d, slot, 0, dual);
}

namespace {
// where a spell goes: the nearest foe in front within its reach (the aim assist of the classic cast), else ahead
int aimTarget(Game& g, const SpellDef& d, Vec2& aim, Vec2& point) {
  Actor& p = g.pl();
  aim = p.aim;
  const float reach = std::max(170.0f, d.range);
  int best = -1;
  float bd = 1e9f;
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& e = g.actors[i];
    if (!foe(e)) continue;
    const Vec2 dd = e.p - p.p;
    const float l = len(dd);
    if (l > reach || l < 1) continue;
    if (dot(dd * (1.0f / l), p.aim) > 0.1f && l < bd) { bd = l; best = (int)i; aim = norm(e.p + Vec2(0, -6) - handsOf(p)); }
  }
  point = best >= 0 ? g.actors[(size_t)best].p : p.p + aim * std::max(48.0f, (d.range > 0 ? d.range : 96.0f) * 0.6f);
  return best;
}
void heroBusy(Game& g, Vec2 aim) {
  Actor& p = g.pl();
  p.aim = aim; p.face = faceOf(aim);
  p.st = AState::Cast; p.stT = 0; p.shootCd = 0.35f;
}
}  // namespace

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
  if (!canPay(g, d, level, dual)) {
    emit(g, Phase::Fizzle, d.id, handsOf(p), p.p, 1, (uint32_t)p.id);
    L.stats.fizzles++;
    MagicOps::tell(g, "NOT ENOUGH MAGICKA");
    return false;
  }
  pay(g, d, costOf(g, d, level, dual));
  const float mag = magic::magnitudeOf(level, dual);
  const float k = powerOf(g, d) * mag;
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
  switch (d.form) {
    case Form::Bolt: case Form::Lance: case Form::Orb: case Form::Mark: {
      Projectile pr;
      pr.kind = ProjKind::Spell; pr.spell = d.id; pr.mag = mag; pr.upcast = (uint8_t)level; pr.dual = dual;
      pr.p = hands + aim * 6; pr.v = aim * std::max(120.0f, d.speed);
      pr.dmg = d.form == Form::Mark && !magic::damaging(d) ? 0.0f : pw;
      pr.radius = (d.form == Form::Orb ? 6.0f : 4.0f) * (dual ? 1.3f : 1.0f);
      pr.life = std::max(0.3f, (d.range > 0 ? d.range : 200.0f) / std::max(120.0f, d.speed));
      pr.fromPlayer = true; pr.owner = pid; pr.from = pr.p; pr.fac = Faction::Player;
      g.projs.push_back(pr);
      break;
    }
    case Form::Nova: case Form::Cone: {
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
      break;
    }
    case Form::Wall: case Form::Rune: case Form::Totem: case Form::Storm: case Form::Meteor: {
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
      break;
    }
    case Form::Mend: {
      const float before = p.hp;
      p.hp = std::min(p.maxHp, p.hp + pw);
      if (perk(g, School::Restoration, R_RESPITE)) g.stamina = std::min(g.maxSt, g.stamina + pw * 0.5f);
      emit(g, Phase::Impact, d.id, p.p, p.p + Vec2(0, -24), mag, (uint32_t)pid, "HEAL");
      if (d.radius > 0)   // GRAND HEALING: the companions within its reach too
        for (Actor& a : g.actors) if (a.comp && alive(a) && len2(a.p - p.p) < d.radius * d.radius * area * area) {
          a.hp = std::min(a.maxHp, a.hp + pw);
          emit(g, Phase::Impact, d.id, a.p, a.p + Vec2(0, -24), mag * 0.7f, (uint32_t)a.id, "HEAL");
        }
      if (d.ess != Essence::Light) for (Actor& a : g.actors) if (a.player || a.comp) a.status &= (uint16_t)~(magic::statusBit(Status::Burning) | magic::statusBit(Status::Poisoned));
      gain(g, d, (p.hp > before ? 1.0f : 0.3f) * (1.0f + 0.5f * (float)level));
      break;
    }
    case Form::Ward: {
      L.ward = std::max(L.ward, pw); L.wardT = d.duration * durK; L.wardSpell = d.id;
      emit(g, Phase::Ward, d.id, p.p + Vec2(0, -8), p.p, mag, (uint32_t)pid, "RAISED");
      gain(g, d, 1.0f);
      break;
    }
    case Form::Aura: {
      const float secs = d.duration * durK * (perk(g, School::Alteration, A_MAGEARMOUR) && d.school == School::Alteration ? 2.0f : 1.0f);
      switch (d.ess) {
        case Essence::Stone: case Essence::Bone: case Essence::Rust:
          L.auraDR = std::min(0.6f, d.power * 1.5f * 0.01f * mag); L.auraT = secs; L.auraSpell = d.id; break;
        case Essence::Light: case Essence::Verdant: case Essence::Tide:
          L.regen = pw * 0.5f; L.regenT = secs; break;
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
      break;
    }
    case Form::Summon: {
      const Vec2 at = p.p + aim * std::max(20.0f, d.range * 0.6f);
      emit(g, Phase::Impact, d.id, at, at + Vec2(0, -30), mag, (uint32_t)pid, "RIFT");
      feel(g, at, d.fx.size, mag);
      const int n = d.rank >= 3 && d.ess == Essence::Bone ? 3 : 1;   // a host of ancestors: three echoes
      for (int i = 0; i < n; i++) {
        const Vec2 off = n > 1 ? Vec2(std::cos((float)i * 2.1f) * 16.0f, std::sin((float)i * 2.1f) * 10.0f) : Vec2();
        summon(g, d, at + off, mag, level, summonKind(d), n > 1 ? 0.6f : 1.0f);
      }
      gain(g, d, 1.0f + 0.5f * (float)level);
      break;
    }
    case Form::Blink: {
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
      break;
    }
    case Form::Beam: {
      if (L.conc.spell) releaseConc(g, false);
      magic::Conc& C = L.conc;
      C = magic::Conc();
      C.spell = d.id; C.left = d.duration * durK; C.mag = mag; C.power = pw; C.caster = pid; C.tick = 0;
      emit(g, Phase::Channel, d.id, hands, hands + aim * d.range, mag, (uint32_t)pid, "BEGIN");
      break;
    }
    default:
      emit(g, Phase::Impact, d.id, p.p, p.p, mag, (uint32_t)pid);
      gain(g, d, 1.0f);
      break;
  }
  heroBusy(g, aim);
  return true;
}

// the classic cast (the pre-M9 Game::castSpell body, numbers unchanged at a plain cast) for the classic three
void MagicOps::classicCast(Game& g, const SpellDef& d, int level, bool dual) {
  Actor& p = g.pl();
  const Spell spell = (Spell)magic::legacySpellOf(d.id);
  const float mag = magic::magnitudeOf(level, dual);
  const float costK = (1.0f + 0.5f * (float)level) * (dual ? 2.0f : 1.0f);
  // (M6) Arcanist: -20 % cost, +20 % power; Blood Magic: health pays, +40 % power; spell power: staff and affixes
  const gear::GearStats& S6 = g.craft.live.stats;
  int cost = spellCost(spell);
  if (S6.has(Unique::Arcanist)) cost = cost * 4 / 5;
  if (d.tier == 0 && perk(g, d.school, P_NOVICE)) cost = cost / 2;
  if (costK != 1.0f) cost = (int)std::lround((float)cost * costK);
  const bool blood = S6.has(Unique::BloodMagic);
  if (!blood && g.mp < cost) { MagicOps::tell(g, "NOT ENOUGH MAGICKA"); MagicOps::snd(g, (int)Sfx::MenuBack, p.p); emit(g, Phase::Fizzle, d.id, handsOf(p), p.p, 1, (uint32_t)p.id); g.arcana.live.stats.fizzles++; return; }
  if (blood && p.hp <= cost + 1) { MagicOps::tell(g, "TOO WEAK TO PAY IN BLOOD"); MagicOps::snd(g, (int)Sfx::MenuBack, p.p); return; }
  if (p.shootCd > 0) return;
  if (blood) { p.hp -= (float)cost; g.events.push_back({Ev::Blood, p.p + Vec2(0, -6), 0, 1, ""}); }
  else g.mp -= cost;
  const float sp = (g.eqStaff >= 0 ? gear::usePower(g.inv[(size_t)g.eqStaff], g.plLevel) : 0.0f) + (float)S6.get(Affix::SpellPower);
  float power = gear::playerDamageMul(g.plLevel) + sp * 0.02f;
  if (S6.has(Unique::Arcanist)) power *= 1.2f;
  if (blood) power *= 1.4f;
  if (spell == Spell::Flames && perk(g, School::Destruction, D_FLAMES)) power *= 1.25f;
  if (spell == Spell::IceSpike && perk(g, School::Destruction, D_FROST)) power *= 1.25f;
  if (spell == Spell::Heal && perk(g, School::Restoration, R_WARMHANDS)) power *= 1.4f;
  power *= mag;
  g.arcana.live.stats.casts++;
  const Vec2 hands = handsOf(p);
  if (spell == Spell::Heal) {
    p.hp = std::min(p.maxHp, p.hp + 45 * power);
    if (perk(g, School::Restoration, R_RESPITE)) g.stamina = std::min(g.maxSt, g.stamina + 22 * power);
    g.events.push_back({Ev::Heal, p.p, 0, 1, ""});
    MagicOps::emit(g, Phase::Launch, d.id, hands, hands, mag, (uint32_t)p.id);
    MagicOps::emit(g, Phase::Impact, d.id, p.p, p.p + Vec2(0, -24), mag, (uint32_t)p.id, "HEAL");
    g.skillUse(skills::Skill::Restoration, 1.0f + 0.5f * (float)level);
  } else {
    Vec2 aim = p.aim; float bd = 1e9f;
    for (size_t i = 1; i < g.actors.size(); i++) {
      const Actor& e = g.actors[i];
      if (!e.hostile || e.st == AState::Dead) continue;
      Vec2 dd = e.p - p.p; float l = len(dd);
      if (l > 170 || l < 1) continue;
      if (dot(dd * (1.0f / l), p.aim) > 0.1f && l < bd) { bd = l; aim = norm(e.p + Vec2(0, -6) - (p.p + Vec2(0, -8))); }
    }
    Projectile pr;
    pr.p = p.p + Vec2(0, -8) + aim * 6;
    pr.fromPlayer = true; pr.owner = p.id;
    pr.kind = ProjKind::Spell;   // (M9: the classic missiles fly as spells; their numbers are the old Fireball's / IceSpike's)
    if (spell == Spell::Flames) { pr.v = aim * 200; pr.dmg = 18 * power; pr.ench = Ench::Fire; pr.enchPow = 8 * power; pr.radius = 4; }
    else { pr.v = aim * 260; pr.dmg = 24 * power; pr.ench = Ench::Frost; pr.enchPow = 6 * power; }
    if (dual) pr.radius *= 1.3f;
    pr.life = 1.0f;
    pr.spell = d.id; pr.mag = mag; pr.upcast = (uint8_t)level; pr.dual = dual;
    pr.from = pr.p;
    g.projs.push_back(pr);
    p.aim = aim; p.face = faceOf(aim);
    MagicOps::emit(g, Phase::Launch, d.id, hands, hands + aim * 24.0f, mag, (uint32_t)p.id);
    MagicOps::ev(g, Ev::Shake, p.p, 0, std::min(3.0f, 0.4f + 0.35f * mag));
  }
  if (g.arcana.live.invisT > 0) { g.arcana.live.invisT = 0; MagicOps::tell(g, "YOU ARE SEEN"); }
  p.st = AState::Cast; p.stT = 0; p.shootCd = 0.35f;
}

bool MagicOps::input(Game& g, const Input& in, bool canAct) {
  magic::Book& B = g.arcana.book;
  magic::Live& L = g.arcana.live;
  if (L.cast.spell) {   // a wind-up or a hold in progress: let go launches it (step); nothing else meanwhile
    if (L.cast.held && !in.castHold) L.cast.held = false;
    return true;
  }
  auto pick = [&](int s) {
    B.sel = (uint8_t)s;
    const int l = magic::legacySpellOf(B.selected());
    if (l >= 0) g.spell = (Spell)l;
    L.legacyMirror = (int8_t)g.spell;
  };
  // a press buffered while the last cast's recovery ran: cast it the moment the hero is free (a hold still held
  // stays a hold; one let go meanwhile becomes a tap); a newer press replaces it, and it lapses after a moment
  if (L.bufSlot >= 0) {
    if (g.time > L.bufUntil || in.castSlot >= 0 || in.spell) L.bufSlot = -1;
    else if (canAct && g.pl().shootCd <= 0) {
      const int s = L.bufSlot;
      const bool h = L.bufHeld && in.castHold, du = L.bufDual;
      L.bufSlot = -1;
      if (B.slot[(size_t)s]) { pick(s); begin(g, s, h, du); return true; }
    }
  }
  const bool busy = g.pl().shootCd > 0 || (!canAct && g.pl().st == AState::Cast);
  if (busy && !L.conc.spell && in.castSlot >= 0 && in.castSlot < magic::QUICK_SLOTS && B.slot[(size_t)in.castSlot]) {
    // still recovering from the last cast: hold the press (not dropped silently)
    L.bufSlot = in.castSlot; L.bufHeld = in.castHold; L.bufDual = in.castDual; L.bufUntil = g.time + 0.6f;
    return true;
  }
  if (canAct && in.castSlot >= 0 && in.castSlot < magic::QUICK_SLOTS) {
    if (!B.slot[(size_t)in.castSlot]) { MagicOps::tell(g, "NO SPELL PREPARED IN THAT SLOT"); return true; }
    pick(in.castSlot);
    begin(g, in.castSlot, in.castHold, in.castDual);
    return true;
  }
  if (canAct && in.spell) { begin(g, B.sel, in.castHold, in.castDual); return true; }
  if (in.swapSpell) {
    syncLegacy(g);
    for (int k = 1; k <= magic::QUICK_SLOTS; k++) {
      const int s = (B.sel + k) % magic::QUICK_SLOTS;
      if (B.slot[s]) { B.sel = (uint8_t)s; break; }
    }
    const int l = magic::legacySpellOf(B.selected());
    if (l >= 0) g.spell = (Spell)l;
    L.legacyMirror = (int8_t)g.spell;
    const SpellDef* d = magic::spellById(B.selected());
    MagicOps::tell(g, std::string("SPELL: ") + (d ? d->name : spellName(g.spell)));
    MagicOps::snd(g, (int)Sfx::MenuMove, g.pl().p);
  }
  return false;
}

// ================================================================== the world step
namespace {
void stepCast(Game& g, float dt) {
  magic::Live& L = g.arcana.live;
  magic::Cast& C = L.cast;
  if (!C.spell) return;
  Actor& p = g.pl();
  const SpellDef* d = magic::spellById(C.spell);
  if (!d || p.st == AState::Roll || p.st == AState::Hurt || p.st == AState::Dead || g.mode != Mode::Play) {
    MagicOps::emit(g, Phase::Fizzle, C.spell, handsOf(p), p.p, 1, (uint32_t)p.id, "INTERRUPTED");
    L.stats.fizzles++;
    C = magic::Cast();
    return;
  }
  C.t += dt;
  p.st = AState::Cast; p.stT = 0;
  if (C.held) {
    // hold to pour in more: a level each 0.45 s past the cast time, while the hero can pay for it
    const int want = std::min(3, (int)std::floor(std::max(0.0f, C.t - d->castTime) / 0.45f));
    if (want > C.level && MagicOps::canPay(g, *d, C.level + 1, C.dual)) {
      C.level++;
      MagicOps::emit(g, Phase::Charge, C.spell, handsOf(p), handsOf(p) + p.aim * 16.0f, magic::magnitudeOf(C.level, C.dual), (uint32_t)p.id);
    }
    if (C.t > d->castTime + 4.0f) C.held = false;   // (held too long: it goes)
    return;
  }
  if (C.t >= d->castTime) {
    const int slot = C.slot, level = C.level;
    const bool dual = C.dual;
    C = magic::Cast();
    p.st = AState::Idle; p.shootCd = 0;
    MagicOps::launch(g, *d, slot, level, dual);
  }
}

void stepConc(Game& g, float dt) {
  magic::Live& L = g.arcana.live;
  magic::Conc& C = L.conc;
  if (!C.spell) return;
  const SpellDef* d = magic::spellById(C.spell);
  Actor& p = g.pl();
  C.t += dt;
  C.left -= dt;
  if (!d || C.left <= 0 || p.st == AState::Dead || p.st == AState::Roll) { MagicOps::releaseConc(g, false); return; }
  if (d->form == Form::Mark) {   // a held charm: it lasts while its target does
    bool on = false;
    for (const magic::Turned& t : L.turned) on |= t.spell == C.spell && t.t > 0;
    if (!on) MagicOps::releaseConc(g, false);
    return;
  }
  if (d->form != Form::Beam) return;
  if ((C.tick -= dt) > 0) return;
  C.tick += 0.1f;
  // the beam: from the hands along the aim (to the nearest foe in front), burning the first body on the line
  Vec2 aim, point;
  aimTarget(g, *d, aim, point);
  p.aim = aim; p.face = faceOf(aim);
  const Vec2 a = handsOf(p);
  Vec2 end = a + aim * d->range;
  int hitK = -1;
  float best = d->range;
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& e = g.actors[i];
    if (!foe(e)) continue;
    const Vec2 c = e.p + Vec2(0, -7);
    const float along = dot(c - a, aim);
    if (along < 0 || along > best) continue;
    const Vec2 perp = c - (a + aim * along);
    if (len2(perp) > (e.radius + 6) * (e.radius + 6)) continue;
    best = along; hitK = (int)i;
  }
  if (hitK >= 0) end = a + aim * best;
  MagicOps::emit(g, Phase::Channel, C.spell, a, end, C.mag, (uint32_t)p.id);
  if (hitK >= 0) {
    Actor& e = g.actors[(size_t)hitK];
    const int eid = e.id;
    spellBlow(g, e, *d, C.power * 0.1f, a, p.id, C.mag);
    const int k2 = MagicOps::findActor(g, eid);
    if (k2 >= 0 && std::fmod(C.t, 0.5f) < 0.1f) spellStatuses(g, g.actors[(size_t)k2], *d, p.id, C.mag);
    if (std::fmod(C.t, 0.5f) < 0.1f) { gain(g, *d, 0.5f); struckWith(g, *d); L.stats.hits++; }
  }
  if (std::fmod(C.t, 0.3f) < 0.1f) MagicOps::touchSurface(g, tileX(end.x), tileY(end.y + 7), d->ess, p.id);
}

void stepZones(Game& g, float dt) {
  magic::Live& L = g.arcana.live;
  for (size_t zi = 0; zi < L.zones.size(); zi++) {
    magic::Zone z = L.zones[zi];   // (a copy: the blows below may grow the zone list)
    const SpellDef* d = magic::spellById(z.spell);
    if (!d) { L.zones[zi].life = 0; continue; }
    z.life -= dt;
    switch (z.kind) {
      case magic::ZoneKind::Meteor: {
        z.delay -= dt;
        if (z.delay <= 0 && z.life > -1) {
          // it lands: the whole radius takes the blow, the ground burns, the world shakes
          MagicOps::emit(g, Phase::Impact, z.spell, z.p, z.q, z.mag, (uint32_t)std::max(0, z.owner), "METEOR");
          g.hitStop = std::max(g.hitStop, 0.1f);
          MagicOps::ev(g, Ev::Shake, z.p, 0, std::min(9.0f, 5.0f + 1.5f * z.mag));
          std::vector<int> ids;
          for (const Actor& e : g.actors) if (foe(e) && len2(e.p - z.p) < z.r * z.r) ids.push_back(e.id);
          for (int id : ids) {
            const int k = MagicOps::findActor(g, id);
            if (k < 0) continue;
            Actor& e = g.actors[(size_t)k];
            const float fall = 1.0f - 0.4f * std::min(1.0f, len(e.p - z.p) / std::max(1.0f, z.r));
            spellBlow(g, e, *d, z.power * fall, z.p, z.owner, z.mag);
            const int k2 = MagicOps::findActor(g, id);
            if (k2 >= 0) {
              Actor& e2 = g.actors[(size_t)k2];
              spellStatuses(g, e2, *d, z.owner, z.mag);
              if (alive(e2) && !e2.boss) e2.knock = norm(e2.p - z.p + Vec2(0.001f, 0)) * 220.0f;
            }
          }
          if (!ids.empty() && z.owner == g.pl().id) { gain(g, *d, std::min(4.0f, 1.0f + 0.5f * (float)ids.size())); struckWith(g, *d); L.stats.hits += (int)ids.size(); }
          groundDisc(g, z.p + Vec2(0, 6), z.r * 0.8f, *d, z.owner, 3, true);
          MagicOps::emit(g, Phase::Aftermath, z.spell, z.p, z.p, z.mag * 4.0f, 0, "CRATER");
          z.life = 0;
        }
        break;
      }
      case magic::ZoneKind::Storm: {
        if ((z.tick -= dt) <= 0) {
          z.tick += 0.7f;
          std::vector<int> ids;
          for (const Actor& e : g.actors) if (foe(e) && len2(e.p - z.p) < z.r * z.r) ids.push_back(e.id);
          if (!ids.empty()) {
            const uint32_t pick = (uint32_t)MagicOps::rng(g).next();
            const int id = ids[(size_t)(pick % (uint32_t)ids.size())];
            const int k = MagicOps::findActor(g, id);
            if (k >= 0) {
              Actor& e = g.actors[(size_t)k];
              const Vec2 at = e.p + Vec2(0, -7);
              MagicOps::emit(g, Phase::Impact, z.spell, at, at + Vec2(10, -150), z.mag, (uint32_t)std::max(0, z.owner), "STRIKE");
              feel(g, at, 2, z.mag, false);
              if (d->ess == Essence::Dream) {
                if (mindReaches(g, *d, z.mag, e)) turn(g, e, *d, d->statusSecs, false, magic::MT_CHARM);
              } else {
                spellBlow(g, e, *d, z.power, z.p, z.owner, z.mag);
                const int k2 = MagicOps::findActor(g, id);
                if (k2 >= 0) spellStatuses(g, g.actors[(size_t)k2], *d, z.owner, z.mag);
                MagicOps::touchSurface(g, tileX(at.x), tileY(at.y + 7), d->ess, z.owner);
              }
              if (z.owner == g.pl().id) { gain(g, *d, 1.0f); struckWith(g, *d); L.stats.hits++; }
            }
          } else MagicOps::emit(g, Phase::Channel, z.spell, z.p + Vec2(0, -160), z.p, z.mag, (uint32_t)std::max(0, z.owner));
        }
        break;
      }
      case magic::ZoneKind::Burn: case magic::ZoneKind::Totem: {
        if ((z.tick -= dt) <= 0) {
          z.tick += 0.5f;
          std::vector<int> ids;
          for (const Actor& e : g.actors) {
            if (!foe(e)) continue;
            bool in = false;
            if (z.kind == magic::ZoneKind::Burn) {   // a wall: within 12 px of its line
              const Vec2 ab = z.q - z.p;
              const float t = std::clamp(dot(e.p - z.p, ab) / std::max(1.0f, len2(ab)), 0.0f, 1.0f);
              in = len2(e.p - (z.p + ab * t)) < 14.0f * 14.0f;
            } else in = len2(e.p - z.p) < z.r * z.r;
            if (in) ids.push_back(e.id);
          }
          for (int id : ids) {
            const int k = MagicOps::findActor(g, id);
            if (k < 0) continue;
            spellBlow(g, g.actors[(size_t)k], *d, z.power * 0.5f, z.p, z.owner, z.mag);
            const int k2 = MagicOps::findActor(g, id);
            if (k2 >= 0) spellStatuses(g, g.actors[(size_t)k2], *d, z.owner, z.mag);
          }
          if (!ids.empty() && z.owner == g.pl().id) { gain(g, *d, 0.5f); struckWith(g, *d); L.stats.hits += (int)ids.size(); }
          if (z.kind == magic::ZoneKind::Totem) MagicOps::emit(g, Phase::Channel, z.spell, z.p + Vec2(0, -20), z.p, z.mag, 0, "PULSE");
        }
        break;
      }
      case magic::ZoneKind::Heal: {
        if ((z.tick -= dt) <= 0) {
          z.tick += 1.0f;
          for (Actor& a : g.actors)
            if ((a.player || a.comp) && alive(a) && len2(a.p - z.p) < z.r * z.r) a.hp = std::min(a.maxHp, a.hp + z.power * 0.25f);
          MagicOps::emit(g, Phase::Channel, z.spell, z.p, z.p + Vec2(0, -20), z.mag, 0, "PULSE");
          if (z.owner == g.pl().id) gain(g, *d, 0.5f);
        }
        break;
      }
      case magic::ZoneKind::Rune: {
        int who = -1;
        for (const Actor& e : g.actors) if (foe(e) && len2(e.p - z.p) < 14.0f * 14.0f) { who = e.id; break; }
        if (who >= 0) {
          MagicOps::emit(g, Phase::Impact, z.spell, z.p, z.p, z.mag, (uint32_t)std::max(0, z.owner), "RUNE");
          feel(g, z.p, d->fx.size, z.mag);
          std::vector<int> ids;
          for (const Actor& e : g.actors) if (foe(e) && len2(e.p - z.p) < z.r * z.r) ids.push_back(e.id);
          for (int id : ids) {
            const int k = MagicOps::findActor(g, id);
            if (k < 0) continue;
            spellBlow(g, g.actors[(size_t)k], *d, z.power, z.p, z.owner, z.mag);
            const int k2 = MagicOps::findActor(g, id);
            if (k2 >= 0) spellStatuses(g, g.actors[(size_t)k2], *d, z.owner, z.mag);
          }
          groundDisc(g, z.p, z.r, *d, z.owner, 2, true);
          if (z.owner == g.pl().id) { gain(g, *d, 1.0f); struckWith(g, *d); }
          z.life = 0;
        }
        break;
      }
    }
    L.zones[zi] = z;
  }
  L.zones.erase(std::remove_if(L.zones.begin(), L.zones.end(), [](const magic::Zone& z) { return z.life <= 0 && z.delay <= 0; }), L.zones.end());
}

void stepSurfaces(Game& g, float dt) {
  magic::Live& L = g.arcana.live;
  magic::Field& F = L.field;
  if (F.tiles.empty()) return;
  const bool rain = MagicOps::raining(g);
  // rain douses fire: burning ground goes out four times as fast, and a burning body in the open is put out
  if (rain && (L.rainT -= dt) <= 0) {
    L.rainT = 1.0f;
    for (Actor& a : g.actors) if (a.hasStatus(Status::Burning)) { a.status &= (uint16_t)~magic::statusBit(Status::Burning); a.statusT[(int)Status::Burning] = 0; }
  }
  for (size_t i = 0; i < F.tiles.size();) {
    magic::SurfaceTile& t = F.tiles[i];
    t.life -= dt * (rain && t.kind == Surface::Fire ? 4.0f : 1.0f);
    t.age += dt;
    if (rain && t.kind == Surface::Fire && t.age > 1.0f) {   // doused: the burning ground hisses out in steam
      const Vec2 at((float)t.tx * TILE + 8, (float)t.ty * TILE + 10);
      MagicOps::emit(g, Phase::Combo, 0, at, at, 0.6f, 0, "DOUSED");
      t.kind = Surface::Steam; t.life = 2.5f; t.age = 0; F.serial++;
      L.stats.combos++;
      i++;
      continue;
    }
    if (t.life <= 0) {
      if (t.kind == Surface::Shock || t.kind == Surface::Ice) {   // the charge runs out / the ice melts: water again
        t.kind = Surface::Water; t.life = 6.0f; t.age = 0; F.serial++;
        i++;
        continue;
      }
      F.tiles.erase(F.tiles.begin() + (std::ptrdiff_t)i); F.serial++;
      continue;
    }
    i++;
  }
  // a tile index for this step (the field holds up to 900 tiles; every lookup below is one hash probe)
  std::unordered_map<uint32_t, uint32_t> idx;
  idx.reserve(F.tiles.size() * 2);
  auto key = [](int x, int y) { return ((uint32_t)(uint16_t)(int16_t)x) | ((uint32_t)(uint16_t)(int16_t)y << 16); };
  for (size_t i = 0; i < F.tiles.size(); i++) idx[key(F.tiles[i].tx, F.tiles[i].ty)] = (uint32_t)i;
  auto at = [&](int x, int y) -> const magic::SurfaceTile* {
    const auto it = idx.find(key(x, y));
    return it == idx.end() ? nullptr : &F.tiles[it->second];
  };
  // fire runs along oil: a burning tile lights the oil beside it
  std::vector<std::pair<int, int>> light;
  for (const magic::SurfaceTile& t : F.tiles) {
    if (t.kind != Surface::Fire || t.age < 0.15f) continue;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) if (const magic::SurfaceTile* o = at(t.tx + dx[k], t.ty + dy[k])) if (o->kind == Surface::Oil) light.push_back({o->tx, o->ty});
  }
  for (const auto& q : light) {
    const magic::SurfaceTile* o = at(q.first, q.second);
    if (o && o->kind == Surface::Oil) MagicOps::laySurface(g, q.first, q.second, Surface::Fire, surfaceLife(Surface::Fire), 2, o->owner);
  }
  if (!light.empty()) {   // (laySurface may have grown the list: index it again)
    idx.clear();
    for (size_t i = 0; i < F.tiles.size(); i++) idx[key(F.tiles[i].tx, F.tiles[i].ty)] = (uint32_t)i;
  }
  // what stands in them takes their status (electrified water also bites)
  const bool shockTick = (L.shockT -= dt) <= 0;
  if (shockTick) L.shockT = 0.5f;
  std::vector<std::pair<int, int>> fell;   // (actor id, killer): killed after the pass
  for (size_t ai = 0; ai < g.actors.size(); ai++) {
    Actor& a = g.actors[ai];
    if (a.st == AState::Dead || a.fly) continue;
    const magic::SurfaceTile* t = at(tileX(a.p.x), tileY(a.p.y));
    if (!t) continue;
    if (a.player && g.godMode) continue;
    const int owner = t->owner;
    switch (t->kind) {
      case Surface::Fire: MagicOps::applyStatus(g, a, Status::Burning, 1.5f, owner); break;
      case Surface::Water: MagicOps::applyStatus(g, a, Status::Wet, 4.0f, owner); break;
      case Surface::Ice: MagicOps::applyStatus(g, a, Status::Chilled, 1.0f, owner); break;
      case Surface::Shock:
        MagicOps::applyStatus(g, a, Status::Shocked, 1.0f, owner);
        if (shockTick && (a.hostile || a.player)) {
          a.hp -= 3.0f + 0.6f * (float)a.level * (float)t->power;
          a.flash = 0.1f;
          if (a.hp <= 0) fell.push_back({a.id, owner >= 0 && owner != a.id ? owner : (a.player ? -1 : g.pl().id)});
        }
        break;
      case Surface::Poison: MagicOps::applyStatus(g, a, Status::Poisoned, 2.0f, owner); break;
      case Surface::Steam: MagicOps::applyStatus(g, a, Status::Blinded, 1.0f, owner); break;
      case Surface::Oil: a.slowT = std::max(a.slowT, 0.3f); break;
      default: break;
    }
  }
  for (const auto& f : fell) {
    const int k = MagicOps::findActor(g, f.first);
    if (k >= 0 && g.actors[(size_t)k].st != AState::Dead) MagicOps::killRaw(g, g.actors[(size_t)k], f.second);
  }
}

// a reaction cast (15.5): a prepared ward or spell breaker answers a telegraphed foe cast or heavy wind-up at the hero
void stepReaction(Game& g, float dt) {
  magic::Live& L = g.arcana.live;
  if (L.reactCd > 0) { L.reactCd -= dt; return; }
  if (L.cast.spell) return;
  Actor& p = g.pl();
  if (p.st == AState::Dead) return;
  int threat = -1;
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& e = g.actors[i];
    if (!foe(e) || e.st != AState::Windup || e.stT > 0.12f || e.target != p.id) continue;
    const bool caster = e.ranged && !e.human;
    const float d2 = len2(e.p - p.p);
    if ((caster && d2 < 170.0f * 170.0f) || ((e.heavy || e.boss) && d2 < 64.0f * 64.0f)) { threat = (int)i; break; }
  }
  if (threat < 0) return;
  magic::Book& B = g.arcana.book;
  for (int k = 0; k < magic::QUICK_SLOTS; k++) {
    const SpellDef* d = B.slot[k] ? magic::spellById(B.slot[k]) : nullptr;
    if (!d || !d->reaction || L.cd[k] > 0 || !MagicOps::canPay(g, *d, 0, false)) continue;
    Actor& e = g.actors[(size_t)threat];
    pay(g, *d, MagicOps::costOf(g, *d, 0, false));
    L.cd[k] = d->cooldown;
    L.reactCd = 6.0f;
    L.stats.reactions++;
    const float pw = d->power * MagicOps::powerOf(g, *d);
    L.ward = std::max(L.ward, pw); L.wardT = std::max(L.wardT, 3.0f); L.wardSpell = d->id;
    // a spell breaker (arcane) also breaks a caster's spell: its wind-up is lost
    std::string what = "WARD";
    if (d->ess == Essence::Arcane && e.ranged && !e.human) { e.st = AState::Hurt; e.stT = 0; e.atkCd = std::max(e.atkCd, 1.2f); what = "COUNTERSPELL"; }
    MagicOps::emit(g, Phase::Reaction, d->id, p.p + Vec2(0, -8), e.p + Vec2(0, -8), 1, (uint32_t)p.id, what);
    MagicOps::emit(g, Phase::Ward, d->id, p.p + Vec2(0, -8), e.p, 1, (uint32_t)p.id, "RAISED");
    MagicOps::ev(g, Ev::Text, p.p + Vec2(0, -30), (int)rgba(200, 230, 255), 0, what);
    gain(g, *d, 1.5f);
    return;
  }
}

// the pools of the signatures (6.2): starlight at night, heat from blows (cooling, overheating), charge from kills
void stepPools(Game& g, float dt) {
  magic::Book& B = g.arcana.book;
  magic::Live& L = g.arcana.live;
  const int kills = g.kills;
  const int newKills = L.lastKills < 0 ? 0 : std::max(0, kills - L.lastKills);
  L.lastKills = kills;
  if (B.attuned.empty()) return;
  for (size_t ai = 0; ai < B.attuned.size(); ai++) {
    const magic::Tradition* t = magic::traditionById(B.attuned[ai]);
    if (!t) continue;
    switch (t->sig) {
      case magic::Signature::Star: {
        float& s = B.res[(int)magic::Resource::Starlight];
        if (g.isNight() && !g.inside && !MagicOps::raining(g)) s = std::min(magic::STARLIGHT_CAP, s + dt * 1.6f * (1.0f - g.daylight()));
        break;
      }
      case magic::Signature::Ember: {
        float& h = B.res[(int)magic::Resource::Heat];
        L.heatIdle += dt;
        if (L.heatIdle > 4.0f) h = std::max(0.0f, h - dt * 3.0f);
        if (h >= magic::HEAT_CAP) {   // overheating burns the hero
          h = 60.0f;
          MagicOps::applyStatus(g, g.pl(), Status::Burning, 3.0f, g.pl().id);
          MagicOps::tell(g, "OVERHEATED!");
        }
        break;
      }
      case magic::Signature::Ancestor: {
        float& c = B.res[(int)magic::Resource::Charge];
        c = std::min(magic::CHARGE_CAP, c + (float)newKills);
        break;
      }
      default: break;
    }
    // mastery (6.4): a kill soon after the tradition's spell struck, under its affinity, is worth 5; at 1200 XP such
    // kills are the Trial of the Threshold
    if (newKills > 0) {
      const auto it = L.tradHitT.find(t->id);
      if (it != L.tradHitT.end() && g.time - it->second < 5.0f && MagicOps::affinity(g, *t)) {
        MagicOps::gainMastery(g, t->id, 5 * newKills);
        if (B.mastery[t->id] >= magic::MASTERY_RANK3 && !(B.trials & (1u << ai)) && ai < std::size(B.trialKills)) {
          B.trialKills[ai] = (uint8_t)std::min(255, (int)B.trialKills[ai] + newKills);
          if (B.trialKills[ai] >= magic::TRIAL_KILLS) {
            B.trials |= 1u << ai;
            MagicOps::ev(g, Ev::Notice, g.pl().p, (int)rgba(255, 220, 140), 0, "TRIAL OF THE THRESHOLD PASSED: " + t->name);
          }
        }
      }
    }
  }
}
}  // namespace

void MagicOps::step(Game& g, float dt) {
  magic::Live& L = g.arcana.live;
  for (float& c : L.cd) if (c > 0) c = std::max(0.0f, c - dt);
  if (L.gcd > 0) L.gcd = std::max(0.0f, L.gcd - dt);
  if (L.prepT > 0) L.prepT = std::max(0.0f, L.prepT - dt);
  // the lasting self effects
  Actor& p = g.pl();
  if (L.wardT > 0 && (L.wardT -= dt) <= 0) { L.ward = 0; emit(g, Phase::Release, L.wardSpell, p.p, p.p, 1, (uint32_t)p.id, "WARD"); }
  if (L.auraT > 0 && (L.auraT -= dt) <= 0) { L.auraDR = 0; emit(g, Phase::Release, L.auraSpell, p.p, p.p, 1, (uint32_t)p.id, "AURA"); }
  if (L.boundT > 0 && (L.boundT -= dt) <= 0) L.boundDmg = 0;
  if (L.blurT > 0 && (L.blurT -= dt) <= 0) L.blur = 0;
  if (L.regenT > 0) { L.regenT -= dt; p.hp = std::min(p.maxHp, p.hp + L.regen * dt); }
  if (L.invisT > 0) { L.invisT -= dt; if (p.st == AState::Windup || p.st == AState::Strike) { L.invisT = 0; MagicOps::tell(g, "YOU ARE SEEN"); } }
  if (perk(g, School::Restoration, R_RECOVERY)) g.mp = std::min(g.maxMp, g.mp + dt * 0.8f);   // RECOVERY: magicka returns faster
  // the hero's own statuses (burning ground, a poison cloud)
  if (p.status && p.st != AState::Dead) {
    statusTick(g, p, dt);
    if (p.hp <= 0) MagicOps::killRaw(g, p, p.statusBy >= 0 && MagicOps::findActor(g, p.statusBy) >= 0 ? p.statusBy : -1);
  }
  stepCast(g, dt);
  stepConc(g, dt);
  stepReaction(g, dt);
  stepPools(g, dt);
  // turned foes come to their senses; a marked foe that fell rises as the hero's thrall
  for (magic::Turned& t : L.turned) {
    t.t -= dt;
    if (t.t <= 0) unturn(g, t);
  }
  L.turned.erase(std::remove_if(L.turned.begin(), L.turned.end(), [&](const magic::Turned& t) { return t.t <= 0 || MagicOps::findActor(g, t.id) < 0; }), L.turned.end());
  std::vector<std::pair<Vec2, art::Monster>> rise;
  float risePower = 0;
  for (Actor& a : g.actors) {
    if (a.magicTag != magic::MT_RAISE) continue;
    if (a.st == AState::Dead) {
      rise.push_back({a.p, a.mon});
      a.magicTag = magic::MT_NONE;
    } else if ((a.magicT -= dt) <= 0) a.magicTag = magic::MT_NONE;
  }
  if (!rise.empty()) {
    const SpellDef* d = magic::spellById(28);   // RAISE DEAD's thrall
    if (d) {
      risePower = 1.0f;
      for (const auto& r : rise) summon(g, *d, r.first, 1.0f, 0, r.second, risePower);
    }
  }
  stepZones(g, dt);
  stepSurfaces(g, dt);
}

void MagicOps::clearMap(Game& g) {
  magic::Live& L = g.arcana.live;
  for (magic::Turned& t : L.turned) unturn(g, t);
  L.turned.clear();
  L.field.clear();
  L.zones.clear();
  L.cast = magic::Cast();
  L.conc = magic::Conc();
}
void MagicOps::shift(Game& g, Vec2 d) {
  magic::Live& L = g.arcana.live;
  for (magic::Zone& z : L.zones) { z.p += d; z.q += d; }
  magic::Field& F = L.field;
  if (F.tiles.empty()) return;
  const int dx = (int)std::lround(d.x / TILE), dy = (int)std::lround(d.y / TILE);
  for (magic::SurfaceTile& t : F.tiles) { t.tx = (int16_t)(t.tx + dx); t.ty = (int16_t)(t.ty + dy); }
  F.serial++;
}
