// M9 "Arcana & Mastery": magic's data. The names, the budget (section 6.3), the schools' own spells (novice ..
// master, every one on its budget), the combo table, the culture traditions (section 6: generated per culture,
// deterministic from the culture), tomes and the book block. MAGIC lane.
//
// What a spell's `power` means, by form (the budget checks it at item / player level 1; MagicOps scales it by level,
// the focus, perks, the signature and the cast's magnitude):
//   Bolt / Lance / Orb / Nova / Cone / Rune / Meteor / Mark: the damage of its blow (a Mark of the mind: its strength,
//   the foe levels it reaches); Beam / Wall: damage a second; Storm: each strike's damage (a Storm of the mind: its
//   strength); Mend: health healed; Totem: damage a second, or (a healing circle) a quarter of it as health a
//   second; Ward: points it
//   soaks; Aura: stone 1.5 % of every blow stopped per point, light / verdant / tide health a second x 0.5, arcane /
//   fire damage added to the hero's blows x 0.8, dream / wind 2.5 % miss chance per point, shadow: unseen; Summon: its
//   strength (health x3, its bite x0.28); Blink: none (its budget is 0).
#include "rpg/sim/magic.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include "rpg/culture/culture.h"

namespace magic {

namespace {
constexpr uint32_t rgb(int r, int g, int b) { return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | 0xFF000000u; }
FxStyle style(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, Particle p, Motion m, uint32_t glyph, uint8_t motif, uint8_t size = 1) {
  FxStyle s;
  s.ramp[0] = a; s.ramp[1] = b; s.ramp[2] = c; s.ramp[3] = d; s.ramp[4] = e;
  s.particle = p; s.motion = m; s.glyphSeed = glyph; s.motif = motif; s.size = size;
  return s;
}
// the schools' palettes (the SPELLFX lane may refine the ramps in its own baked sheets; these are the identities)
FxStyle fireFx() { return style(rgb(90, 14, 8), rgb(190, 40, 12), rgb(250, 120, 30), rgb(255, 210, 90), rgb(255, 250, 220), Particle::Ember, Motion::Boil, 0xF1EEu, 0); }
FxStyle frostFx() { return style(rgb(16, 30, 90), rgb(40, 90, 190), rgb(110, 190, 250), rgb(200, 240, 255), rgb(255, 255, 255), Particle::Shard, Motion::Shatter, 0xF057u, 1); }
FxStyle stormFx() { return style(rgb(40, 20, 90), rgb(90, 70, 220), rgb(150, 170, 255), rgb(220, 230, 255), rgb(255, 255, 255), Particle::Spark, Motion::Fork, 0x5707u, 2); }
FxStyle healFx() { return style(rgb(120, 70, 10), rgb(220, 150, 40), rgb(255, 210, 100), rgb(255, 240, 180), rgb(255, 255, 240), Particle::Mote, Motion::Rise, 0x4EA1u, 3); }
FxStyle conjFx() { return style(rgb(30, 10, 50), rgb(90, 30, 150), rgb(170, 90, 240), rgb(220, 170, 255), rgb(250, 240, 255), Particle::Glyph, Motion::Tear, 0xC0A1u, 4); }
FxStyle illuFx() { return style(rgb(60, 10, 60), rgb(160, 50, 160), rgb(240, 120, 220), rgb(255, 200, 245), rgb(255, 250, 255), Particle::Petal, Motion::Shimmer, 0x1115u, 5); }
FxStyle altFx() { return style(rgb(20, 50, 60), rgb(40, 120, 130), rgb(90, 200, 200), rgb(180, 240, 230), rgb(240, 255, 250), Particle::Shard, Motion::Pulse, 0xA17Eu, 6); }

// the look of an essence (the schools' own pick by school first; a tradition's by essence, tinted by its culture)
FxStyle essenceFx(Essence e) {
  switch (e) {
    case Essence::Fire: return fireFx();
    case Essence::Frost: return frostFx();
    case Essence::Storm: return stormFx();
    case Essence::Light: return healFx();
    case Essence::Shadow: return style(rgb(14, 6, 28), rgb(50, 20, 90), rgb(110, 60, 180), rgb(180, 140, 240), rgb(235, 220, 255), Particle::Smoke, Motion::Tear, 0x5AD0u, 4);
    case Essence::Bone: return style(rgb(50, 44, 36), rgb(120, 110, 92), rgb(196, 186, 160), rgb(232, 226, 204), rgb(255, 252, 240), Particle::Bone, Motion::Tear, 0xB0E5u, 4);
    case Essence::Arcane: return conjFx();
    case Essence::Dream: return illuFx();
    case Essence::Stone: return style(rgb(40, 32, 24), rgb(96, 80, 60), rgb(150, 132, 104), rgb(200, 186, 156), rgb(240, 232, 210), Particle::Shard, Motion::Pulse, 0x5703u, 6);
    case Essence::Tide: return style(rgb(8, 30, 70), rgb(20, 80, 150), rgb(60, 150, 210), rgb(150, 210, 240), rgb(230, 250, 255), Particle::Droplet, Motion::Spiral, 0x71DEu, 6);
    case Essence::Venom: return style(rgb(20, 40, 10), rgb(60, 110, 20), rgb(120, 190, 40), rgb(190, 240, 110), rgb(240, 255, 200), Particle::Smoke, Motion::Drift, 0x7E70u, 6);
    case Essence::Wind: return style(rgb(50, 70, 80), rgb(110, 140, 150), rgb(170, 200, 205), rgb(215, 235, 238), rgb(250, 255, 255), Particle::Feather, Motion::Spiral, 0x3170u, 6);
    case Essence::Verdant: return style(rgb(14, 40, 14), rgb(40, 100, 30), rgb(90, 170, 60), rgb(170, 225, 110), rgb(235, 255, 200), Particle::Leaf, Motion::Rise, 0x7EEDu, 3);
    case Essence::Star: return style(rgb(10, 10, 40), rgb(40, 40, 120), rgb(110, 120, 230), rgb(200, 210, 255), rgb(255, 255, 255), Particle::Star, Motion::Fall, 0x57A2u, 7);
    case Essence::Blood: return style(rgb(40, 0, 6), rgb(110, 10, 20), rgb(190, 30, 40), rgb(240, 110, 110), rgb(255, 220, 220), Particle::Droplet, Motion::Pulse, 0xB100u, 8);
    case Essence::Song: return style(rgb(70, 50, 10), rgb(160, 120, 30), rgb(230, 190, 70), rgb(250, 230, 150), rgb(255, 250, 230), Particle::Note, Motion::Pulse, 0x5096u, 9);
    case Essence::Rust: return style(rgb(50, 20, 8), rgb(120, 52, 20), rgb(184, 92, 40), rgb(226, 150, 90), rgb(250, 220, 180), Particle::Ember, Motion::Shatter, 0x2057u, 0);
    default: return conjFx();
  }
}
FxStyle schoolFx(School sc, Essence e) {
  switch (sc) {
    case School::Destruction: return essenceFx(e);
    case School::Restoration: return e == Essence::Light ? healFx() : essenceFx(e);
    case School::Conjuration: return e == Essence::Arcane ? conjFx() : essenceFx(e);
    case School::Illusion: return e == Essence::Dream ? illuFx() : essenceFx(e);
    case School::Alteration: return e == Essence::Stone ? altFx() : essenceFx(e);
    default: return essenceFx(e);
  }
}

// the timing of a form (the same table for the schools and the traditions, so equal forms at equal tiers trade equal
// damage a second: the tradition-vs-Common check)
struct Timing { float cd, castT, range, radius, dur, speed, costMul; };
Timing timingOf(Form f) {
  switch (f) {
    case Form::Bolt: return {0, 0, 220, 0, 3, 220, 1.0f};
    case Form::Lance: return {0.6f, 0, 260, 0, 3, 260, 1.3f};
    case Form::Nova: return {6, 0.3f, 0, 64, 6, 0, 1.8f};
    case Form::Cone: return {1.5f, 0, 72, 72, 4, 0, 1.2f};
    case Form::Wall: return {10, 0.4f, 64, 40, 6, 0, 2.0f};
    case Form::Rune: return {4, 0.3f, 48, 28, 30, 0, 1.5f};
    case Form::Aura: return {20, 0.3f, 0, 0, 60, 0, 1.8f};
    case Form::Summon: return {20, 0.6f, 48, 0, 60, 0, 2.5f};
    case Form::Ward: return {8, 0.2f, 0, 0, 12, 0, 1.8f};
    case Form::Blink: return {6, 0, 96, 0, 0, 0, 1.2f};
    case Form::Beam: return {6, 0.2f, 140, 0, 4, 0, 2.0f};
    case Form::Orb: return {2.5f, 0.35f, 220, 40, 6, 160, 1.8f};
    case Form::Mark: return {8, 0.25f, 200, 0, 20, 260, 1.6f};
    case Form::Mend: return {0, 0, 0, 0, 0, 0, 1.75f};
    case Form::Totem: return {20, 0.4f, 48, 56, 10, 0, 2.0f};
    case Form::Storm: return {75, 0.8f, 180, 90, 6, 0, 2.4f};
    case Form::Meteor: return {45, 1.0f, 200, 72, 10, 0, 2.4f};
    default: return {1, 0, 100, 0, 3, 0, 1.0f};
  }
}
constexpr float kTierCost[TIERS] = {16, 28, 45, 70, 100};

uint16_t defaultStatus(Essence e, float& secs) {
  switch (e) {
    case Essence::Fire: secs = 3; return statusBit(Status::Burning);
    case Essence::Frost: secs = 3; return statusBit(Status::Chilled);
    case Essence::Storm: secs = 2; return statusBit(Status::Shocked);
    case Essence::Tide: secs = 6; return statusBit(Status::Wet);
    case Essence::Venom: secs = 4; return statusBit(Status::Poisoned);
    default: secs = 0; return 0;
  }
}
Surface defaultLeaves(Essence e, Form f) {
  const bool area = f == Form::Orb || f == Form::Nova || f == Form::Wall || f == Form::Meteor;
  if (!area) return Surface::None;
  switch (e) {
    case Essence::Fire: return Surface::Fire;
    case Essence::Frost: return Surface::Ice;
    case Essence::Tide: return Surface::Water;
    case Essence::Venom: return Surface::Poison;
    case Essence::Blood: return Surface::Blood;
    default: return Surface::None;
  }
}

// a spell from the grammar: the form's timing, the tier's cost, the essence's status, power on its budget
SpellDef mk(uint32_t id, const char* name, School sc, Form f, Essence e, uint8_t tier, uint16_t mods = 0) {
  SpellDef s;
  s.id = id; s.name = name; s.school = sc; s.form = f; s.ess = e; s.tier = tier; s.mods = mods;
  const Timing t = timingOf(f);
  s.cooldown = t.cd; s.castTime = t.castT; s.range = t.range; s.radius = t.radius; s.duration = t.dur; s.speed = t.speed;
  s.cost = std::round(kTierCost[std::min<int>(tier, TIERS - 1)] * t.costMul);
  s.statuses = defaultStatus(e, s.statusSecs);
  s.leaves = defaultLeaves(e, f);
  s.fx = schoolFx(sc, e);
  s.fx.size = (uint8_t)(tier >= 4 ? 4 : tier >= 3 ? 3 : tier >= 2 ? 2 : 1);
  s.fx.glyphSeed ^= id * 0x9E3779B1u;
  s.power = budgetOf(s);
  if (f == Form::Beam || f == Form::Storm) s.upcast = true;
  return s;
}

std::vector<SpellDef> makeSchoolSpells() {
  std::vector<SpellDef> v;
  using S = School;
  using F = Form;
  using E = Essence;
  // ---- the classic three (pre-M9 numbers, untouched: Firebolt 18 damage, 16 magicka, 200 px/s; Mend heals 45 for 28;
  //      Frost Lance 24 damage for 24 at 260 px/s; the shared 0.35 s cooldown). MagicOps::classicCast casts them.
  {
    SpellDef s = mk(SPELL_FIREBOLT, "FIREBOLT", S::Destruction, F::Bolt, E::Fire, 0);
    s.power = 18; s.cost = 16; s.cooldown = 0; s.castTime = 0; s.range = 200; s.radius = 22; s.duration = 3; s.speed = 200;
    s.statuses = statusBit(Status::Burning); s.statusSecs = 3; s.leaves = Surface::Fire;
    v.push_back(s);
  }
  {
    SpellDef s = mk(SPELL_MEND, "MEND", S::Restoration, F::Mend, E::Light, 0);
    s.power = 45; s.cost = 28; s.cooldown = 0; s.castTime = 0; s.range = 0; s.radius = 0; s.duration = 0; s.speed = 0;
    v.push_back(s);
  }
  {
    SpellDef s = mk(SPELL_FROST_LANCE, "FROST LANCE", S::Destruction, F::Lance, E::Frost, 0);
    s.power = 24; s.cost = 24; s.cooldown = 0; s.castTime = 0; s.range = 260; s.radius = 0; s.duration = 3; s.speed = 260;
    s.statuses = statusBit(Status::Chilled); s.statusSecs = 3; s.leaves = Surface::Ice;
    v.push_back(s);
  }
  // ---- phase A's first spells (ids 4..8 are saved in tests/fixtures/save_v20.bin: kept)
  {
    SpellDef s = mk(4, "SPARK", S::Destruction, F::Bolt, E::Storm, 0, M_CHAIN);
    s.cost = 18; s.range = 220; s.duration = 2; s.speed = 320; s.statusSecs = 2;
    v.push_back(s);
  }
  {
    SpellDef s = mk(5, "STONE WARD", S::Restoration, F::Ward, E::Light, 0);
    s.cost = 30; s.cooldown = 8; s.castTime = 0.25f; s.duration = 12; s.reaction = true;
    v.push_back(s);
  }
  {
    SpellDef s = mk(6, "SPIRIT WOLF", S::Conjuration, F::Summon, E::Shadow, 0);
    s.cost = 40; s.cooldown = 20; s.castTime = 0.6f; s.range = 48; s.duration = 30; s.dual = false;
    v.push_back(s);
  }
  {
    SpellDef s = mk(7, "FEAR", S::Illusion, F::Nova, E::Dream, 0);
    s.cost = 30; s.cooldown = 12; s.castTime = 0.3f; s.radius = 64; s.duration = 6;
    s.statuses = statusBit(Status::Frightened); s.statusSecs = 6;
    v.push_back(s);
  }
  {
    SpellDef s = mk(8, "OAKFLESH", S::Alteration, F::Aura, E::Stone, 0);
    s.cost = 30; s.cooldown = 30; s.castTime = 0.3f; s.duration = 60;
    v.push_back(s);
  }
  // ---- DESTRUCTION: fire that boils, frost that shatters, lightning that forks and chains; a meteor and a storm cell
  {
    SpellDef s = mk(9, "FIREBALL", S::Destruction, F::Orb, E::Fire, 1);
    v.push_back(s);
  }
  { SpellDef s = mk(10, "CHAIN LIGHTNING", S::Destruction, F::Bolt, E::Storm, 1, M_CHAIN); s.speed = 340; s.statusSecs = 2; v.push_back(s); }
  { SpellDef s = mk(11, "ICE BURST", S::Destruction, F::Nova, E::Frost, 2); s.radius = 72; v.push_back(s); }
  { SpellDef s = mk(12, "FIRE WALL", S::Destruction, F::Wall, E::Fire, 2); v.push_back(s); }
  { SpellDef s = mk(13, "THUNDERBOLT", S::Destruction, F::Lance, E::Storm, 2); s.speed = 360; v.push_back(s); }
  { SpellDef s = mk(14, "INCINERATE", S::Destruction, F::Beam, E::Fire, 3); s.concentration = true; s.dual = false; v.push_back(s); }
  { SpellDef s = mk(15, "GLACIAL SPEAR", S::Destruction, F::Lance, E::Frost, 3, M_KNOCK); s.speed = 300; v.push_back(s); }
  { SpellDef s = mk(16, "METEOR", S::Destruction, F::Meteor, E::Fire, 4); s.fx.size = 4; v.push_back(s); }
  { SpellDef s = mk(17, "STORM CELL", S::Destruction, F::Storm, E::Storm, 4); s.fx.size = 4; s.leaves = Surface::None; v.push_back(s); }
  // ---- RESTORATION: warm rising heals, wards, turn undead
  { SpellDef s = mk(18, "CLOSE WOUNDS", S::Restoration, F::Mend, E::Light, 1); v.push_back(s); }
  {
    SpellDef s = mk(19, "TURN UNDEAD", S::Restoration, F::Nova, E::Light, 1);
    s.radius = 80; s.statuses = statusBit(Status::Frightened); s.statusSecs = 8;
    v.push_back(s);
  }
  { SpellDef s = mk(20, "STEADFAST WARD", S::Restoration, F::Ward, E::Light, 2); s.reaction = true; v.push_back(s); }
  { SpellDef s = mk(21, "REGENERATION", S::Restoration, F::Aura, E::Light, 2); s.duration = 10; s.cooldown = 30; v.push_back(s); }
  { SpellDef s = mk(22, "HEALING CIRCLE", S::Restoration, F::Totem, E::Light, 3); v.push_back(s); }
  { SpellDef s = mk(23, "SUNBURST", S::Restoration, F::Nova, E::Light, 3); s.radius = 88; s.statuses = statusBit(Status::Blinded); s.statusSecs = 3; v.push_back(s); }
  { SpellDef s = mk(24, "GRAND HEALING", S::Restoration, F::Mend, E::Light, 4); s.cooldown = 8; s.radius = 96; s.fx.size = 3; v.push_back(s); }
  { SpellDef s = mk(25, "HEAVEN'S PILLAR", S::Restoration, F::Meteor, E::Light, 4); s.fx.size = 4; s.leaves = Surface::None; v.push_back(s); }
  // ---- CONJURATION: rifts with summoned forms climbing out, bound weapons, a great summoned beast at master
  { SpellDef s = mk(26, "BOUND BLADE", S::Conjuration, F::Aura, E::Arcane, 0); s.duration = 60; v.push_back(s); }
  { SpellDef s = mk(27, "BONE THRALL", S::Conjuration, F::Summon, E::Bone, 1); v.push_back(s); }
  { SpellDef s = mk(28, "RAISE DEAD", S::Conjuration, F::Mark, E::Bone, 2); s.duration = 12; v.push_back(s); }
  { SpellDef s = mk(29, "FLAME ATRONACH", S::Conjuration, F::Summon, E::Fire, 2); s.statuses = 0; v.push_back(s); }
  { SpellDef s = mk(30, "STORM ATRONACH", S::Conjuration, F::Summon, E::Storm, 3); s.statuses = 0; v.push_back(s); }
  { SpellDef s = mk(31, "BANISHING RIFT", S::Conjuration, F::Nova, E::Arcane, 3); s.radius = 80; v.push_back(s); }
  { SpellDef s = mk(32, "RIFT BEHEMOTH", S::Conjuration, F::Summon, E::Shadow, 4); s.fx.size = 4; s.cooldown = 60; s.dual = false; v.push_back(s); }
  // ---- ILLUSION: fear, calm, charm (a charmed foe fights for you), blur and invisibility, blindness
  { SpellDef s = mk(33, "DAZZLE", S::Illusion, F::Cone, E::Dream, 0); s.statuses = statusBit(Status::Blinded); s.statusSecs = 4; v.push_back(s); }
  { SpellDef s = mk(34, "CALM", S::Illusion, F::Mark, E::Dream, 1); s.duration = 20; v.push_back(s); }
  { SpellDef s = mk(35, "BLUR", S::Illusion, F::Aura, E::Dream, 1); s.duration = 30; v.push_back(s); }
  {
    SpellDef s = mk(36, "CHARM", S::Illusion, F::Mark, E::Dream, 2);
    s.duration = 30; s.concentration = true; s.statuses = statusBit(Status::Charmed); s.statusSecs = 30;
    v.push_back(s);
  }
  {
    SpellDef s = mk(37, "HYSTERIA", S::Illusion, F::Nova, E::Dream, 3);
    s.radius = 110; s.statuses = statusBit(Status::Frightened); s.statusSecs = 10;
    v.push_back(s);
  }
  { SpellDef s = mk(38, "INVISIBILITY", S::Illusion, F::Aura, E::Shadow, 3); s.duration = 30; s.cooldown = 40; v.push_back(s); }
  {
    SpellDef s = mk(39, "MAYHEM", S::Illusion, F::Storm, E::Dream, 4);
    s.fx.size = 4; s.radius = 120; s.statuses = statusBit(Status::Charmed); s.statusSecs = 10;
    v.push_back(s);
  }
  // ---- ALTERATION: oakflesh / stoneskin, stone and water shaping, blink, paralysis
  { SpellDef s = mk(40, "STONE FIST", S::Alteration, F::Bolt, E::Stone, 0, M_KNOCK); s.speed = 200; v.push_back(s); }
  { SpellDef s = mk(41, "TIDECALL", S::Alteration, F::Nova, E::Tide, 0); s.radius = 56; v.push_back(s); }
  { SpellDef s = mk(42, "STONESKIN", S::Alteration, F::Aura, E::Stone, 1); v.push_back(s); }
  { SpellDef s = mk(43, "BLINK", S::Alteration, F::Blink, E::Wind, 1); s.cost = 30; v.push_back(s); }
  { SpellDef s = mk(44, "OIL FLASK", S::Alteration, F::Orb, E::Venom, 1); s.statuses = 0; s.leaves = Surface::Oil; s.radius = 44; v.push_back(s); }
  { SpellDef s = mk(45, "IRONFLESH", S::Alteration, F::Aura, E::Stone, 2); v.push_back(s); }
  { SpellDef s = mk(46, "MUDSLIDE", S::Alteration, F::Nova, E::Stone, 2, M_ROOT); s.leaves = Surface::Water; v.push_back(s); }
  { SpellDef s = mk(47, "SPELL BREAKER", S::Alteration, F::Ward, E::Arcane, 2); s.reaction = true; v.push_back(s); }
  { SpellDef s = mk(48, "PARALYZE", S::Alteration, F::Mark, E::Stone, 3); s.duration = 6; v.push_back(s); }
  { SpellDef s = mk(49, "DRAGONHIDE", S::Alteration, F::Aura, E::Stone, 4); s.duration = 40; s.cooldown = 60; s.fx.size = 3; v.push_back(s); }
  { SpellDef s = mk(50, "EARTHSHATTER", S::Alteration, F::Storm, E::Stone, 4); s.fx.size = 4; v.push_back(s); }
  return v;
}

// ---- the traditions' registry (traditionOf memoises; spellById finds their spells)
struct Registry {
  std::map<uint64_t, Tradition> byCulture;   // culture key -> tradition
  std::map<uint32_t, uint64_t> byId;         // tradition id -> culture key
  std::map<uint32_t, SpellDef> spells;       // tradition spell id -> its def
};
Registry& reg() {
  static Registry r;
  return r;
}
uint32_t g_songLand = 0;   // (M10) the nearest hidden land of the world being played (setSongLand)
bool isSongLand(const cult::Culture& c) { return c.hidden && c.hiddenLand && c.hiddenLand == g_songLand; }
// (M10) a hidden people's key carries its land (and whether it is the song land): a mainland culture's is unchanged
uint64_t cultureKey(const cult::Culture& c) {
  uint64_t k = c.id * 0x9E3779B97F4A7C15ull ^ ((uint64_t)c.seed << 1) ^ 0xC0FFEEull;
  if (c.hidden) k ^= ((uint64_t)hash32(c.hiddenLand ^ 0x41DD3Bu) << 17) ^ (isSongLand(c) ? 0x5096F14Eull << 3 : 0x1D3E7ull) ^ 0x7E1D0000000000ull;
  return k;
}

uint32_t blend(uint32_t a, uint32_t b, int t256) {
  auto ch = [&](int sh) {
    const int x = (int)((a >> sh) & 255u), y = (int)((b >> sh) & 255u);
    return (uint32_t)std::clamp(x + (y - x) * t256 / 256, 0, 255) << sh;
  };
  return ch(0) | ch(8) | ch(16) | 0xFF000000u;
}
const char* essenceWord(Essence e) {
  static const char* n[] = {"EMBER", "RIME", "THUNDER", "STONE", "TIDE", "VENOM", "SHADE", "DAWN", "BLOOD", "BONE", "GALE", "THORN", "STAR", "SONG",
                            "DREAM", "RUST", "RUNE"};
  return (int)e < (int)Essence::COUNT ? n[(int)e] : "";
}
const char* formNoun(Form f) {
  static const char* n[] = {"DART", "SPEAR", "BLOOM", "BREATH", "WALL", "SIGIL", "MANTLE", "CALLING", "AEGIS", "STEP", "RAY", "ORB", "BRAND", "BALM",
                            "TOTEM", "TEMPEST", "FALL"};
  return (int)f < (int)Form::COUNT ? n[(int)f] : "";
}
School schoolOfForm(Form f, Essence e) {
  switch (f) {
    case Form::Mend: case Form::Ward: return School::Restoration;
    case Form::Summon: return School::Conjuration;
    case Form::Blink: case Form::Aura: return e == Essence::Dream || e == Essence::Shadow ? School::Illusion : School::Alteration;
    case Form::Mark: return e == Essence::Dream ? School::Illusion : e == Essence::Bone || e == Essence::Verdant ? School::Conjuration : School::Alteration;
    case Form::Totem: return e == Essence::Light || e == Essence::Verdant || e == Essence::Tide ? School::Restoration : School::Destruction;
    default: return e == Essence::Dream ? School::Illusion : School::Destruction;
  }
}
struct Pick { uint8_t rank; Form form; Essence ess; bool keep = false; };   // keep: a rank-2 pick never dropped (a mechanic's own)
// the forms each signature favours (6.1: "the tradition's essences and signature constrain the forms it may use");
// rank 1 first, then rank 2, then rank 3 (the last is the signature spell). song: the nearest hidden land (its
// signature spell burns: the priests who sing fire into being)
std::vector<Pick> picksOf(Signature sig, uint32_t h = 0, bool song = false) {
  using F = Form;
  using E = Essence;
  switch (sig) {
    // ---- (M10) the hidden kingdoms' signatures
    case Signature::Blood:
      return {{1, F::Bolt, E::Blood}, {1, F::Lance, E::Blood}, {1, F::Mark, E::Blood},
              {2, F::Nova, E::Blood}, {2, F::Cone, E::Blood}, {2, F::Orb, E::Fire}, {2, F::Wall, E::Blood}, {2, F::Aura, E::Blood},
              {3, F::Beam, E::Blood}, {3, F::Storm, E::Blood}, {3, F::Nova, E::Blood}};
    case Signature::Rune:
      return {{1, F::Rune, E::Fire}, {1, F::Rune, E::Frost}, {1, F::Bolt, E::Stone},
              {2, F::Rune, E::Storm, true}, {2, F::Rune, E::Venom}, {2, F::Ward, E::Stone}, {2, F::Lance, E::Frost}, {2, F::Wall, E::Fire},
              {3, F::Rune, E::Stone}, {3, F::Meteor, E::Stone}, {3, F::Rune, E::Arcane}};
    case Signature::Song:
      return {{1, F::Bolt, E::Song}, {1, F::Nova, E::Song}, {1, F::Mend, E::Song},
              {2, F::Cone, E::Fire, true}, {2, F::Orb, E::Fire}, {2, F::Aura, E::Song}, {2, F::Lance, E::Song}, {2, F::Ward, E::Song},
              {3, F::Storm, E::Song}, {3, F::Beam, E::Fire}, {3, F::Nova, song || (h >> 21) % 3u != 0 ? E::Fire : E::Light}};
    case Signature::Mask:
      return {{1, F::Aura, E::Fire}, {1, F::Aura, E::Frost}, {1, F::Bolt, E::Shadow},
              {2, F::Aura, E::Storm, true}, {2, F::Cone, E::Wind}, {2, F::Lance, E::Bone}, {2, F::Nova, E::Shadow}, {2, F::Mark, E::Dream},
              {3, F::Storm, E::Shadow}, {3, F::Summon, E::Shadow}, {3, F::Nova, E::Arcane}};
    case Signature::Verdant:
      return {{1, F::Wall, E::Verdant}, {1, F::Nova, E::Verdant}, {1, F::Bolt, E::Verdant},
              {2, F::Blink, E::Verdant, true}, {2, F::Lance, E::Verdant}, {2, F::Totem, E::Verdant}, {2, F::Cone, E::Wind}, {2, F::Orb, E::Venom},
              {3, F::Storm, E::Verdant}, {3, F::Summon, E::Verdant}, {3, F::Wall, E::Verdant}};
    case Signature::Dream:
      return {{1, F::Summon, E::Dream}, {1, F::Bolt, E::Shadow}, {1, F::Cone, E::Dream},
              {2, F::Totem, E::Dream, true}, {2, F::Lance, E::Shadow}, {2, F::Mark, E::Dream}, {2, F::Aura, E::Dream}, {2, F::Nova, E::Shadow},
              {3, F::Storm, E::Dream}, {3, F::Summon, E::Dream}, {3, F::Totem, E::Dream}};
    case Signature::Star:
      return {{1, F::Bolt, E::Star}, {1, F::Nova, E::Frost}, {1, F::Ward, E::Light},
              {2, F::Lance, E::Star}, {2, F::Orb, E::Star}, {2, F::Cone, E::Frost}, {2, F::Beam, E::Light}, {2, F::Totem, E::Star},
              {3, F::Storm, E::Star}, {3, F::Nova, E::Star}, {3, F::Meteor, E::Star}};
    case Signature::Tide:
      return {{1, F::Bolt, E::Tide}, {1, F::Nova, E::Tide}, {1, F::Mend, E::Tide},
              {2, F::Lance, E::Frost}, {2, F::Wall, E::Tide}, {2, F::Cone, E::Storm}, {2, F::Orb, E::Tide}, {2, F::Ward, E::Tide},
              {3, F::Beam, E::Tide}, {3, F::Storm, E::Storm}, {3, F::Nova, E::Tide}};
    case Signature::Ancestor:
      return {{1, F::Summon, E::Bone}, {1, F::Bolt, E::Shadow}, {1, F::Ward, E::Bone},
              {2, F::Cone, E::Wind}, {2, F::Mark, E::Bone}, {2, F::Nova, E::Shadow}, {2, F::Lance, E::Bone}, {2, F::Totem, E::Bone},
              {3, F::Storm, E::Wind}, {3, F::Summon, E::Shadow}, {3, F::Summon, E::Bone}};
    case Signature::Ember:
      return {{1, F::Bolt, E::Fire}, {1, F::Cone, E::Fire}, {1, F::Ward, E::Stone},
              {2, F::Lance, E::Rust}, {2, F::Orb, E::Fire}, {2, F::Wall, E::Fire}, {2, F::Nova, E::Stone}, {2, F::Aura, E::Stone},
              {3, F::Beam, E::Fire}, {3, F::Meteor, E::Fire}, {3, F::Aura, E::Fire}};
    case Signature::Beast:
      return {{1, F::Mark, E::Verdant}, {1, F::Bolt, E::Verdant}, {1, F::Mend, E::Verdant},
              {2, F::Summon, E::Wind}, {2, F::Cone, E::Wind}, {2, F::Nova, E::Verdant}, {2, F::Lance, E::Blood}, {2, F::Aura, E::Verdant},
              {3, F::Storm, E::Wind}, {3, F::Summon, E::Verdant}, {3, F::Nova, E::Blood}};
    default: return {};
  }
}
const char* signatureSpellName(Signature s) {
  switch (s) {
    case Signature::Star: return "STARFALL";
    case Signature::Tide: return "TIDAL SURGE";
    case Signature::Ancestor: return "HOST OF ANCESTORS";
    case Signature::Ember: return "FORGEHEART";
    case Signature::Beast: return "CALL OF THE WILD";
    case Signature::Blood: return "CRIMSON COVENANT";
    case Signature::Rune: return "THE GREAT GLYPH";
    case Signature::Song: return "CHOIR OF EMBERS";
    case Signature::Mask: return "HUNDRED MASKS";
    case Signature::Verdant: return "THE WILDWOOD";
    case Signature::Dream: return "THE LONG MOMENT";
    default: return "";
  }
}
// (M10) a hidden tradition's focus, sold only in its own land (6.4)
const char* focusName(Signature s) {
  switch (s) {
    case Signature::Blood: return "VEIN CHALICE";
    case Signature::Rune: return "RUNE-KNIFE";
    case Signature::Song: return "SINGING BOWL";
    case Signature::Mask: return "SPIRIT MASK";
    case Signature::Verdant: return "THORNWOOD ROD";
    case Signature::Dream: return "DREAMCATCHER";
    default: return "";
  }
}
const char* traditionNoun(Signature s, uint32_t h) {
  static const char* star[] = {"STARCALLERS", "NIGHT CHOIR", "STAR CIRCLE"};
  static const char* tide[] = {"TIDEBINDERS", "WAY OF THE TIDE", "RAIN SPEAKERS"};
  static const char* anc[] = {"ASHWAKE RITES", "BARROW CALLERS", "ANCESTOR WAY"};
  static const char* emb[] = {"EMBERFORGE", "FORGE CANTORS", "ANVIL RITE"};
  static const char* bst[] = {"BEASTBINDERS", "WILD CIRCLE", "PACK SPEAKERS"};
  static const char* bld[] = {"BLOOD COVENANT", "CRIMSON RITES", "VEIN SINGERS"};
  static const char* run[] = {"RUNECARVERS", "GLYPHWRIGHTS", "STONE SCRIBES"};
  static const char* sng[] = {"NINEFOLD CHOIR", "EMBER CANTORS", "SONGWEAVERS"};
  static const char* msk[] = {"MASKWEARERS", "HUNDRED FACES", "SPIRIT DANCERS"};
  static const char* vrd[] = {"WILDSHAPERS", "THORN WARDENS", "GREEN WEAVE"};
  static const char* drm[] = {"DREAMWEAVERS", "STILL HOUR", "VEILED SLEEPERS"};
  const int k = (int)(h % 3u);
  switch (s) {
    case Signature::Blood: return bld[k];
    case Signature::Rune: return run[k];
    case Signature::Song: return sng[k];
    case Signature::Mask: return msk[k];
    case Signature::Verdant: return vrd[k];
    case Signature::Dream: return drm[k];
    case Signature::Star: return star[k];
    case Signature::Tide: return tide[k];
    case Signature::Ancestor: return anc[k];
    case Signature::Ember: return emb[k];
    case Signature::Beast: return bst[k];
    default: return "ARCANA";
  }
}
Signature signatureFor(const cult::Culture& c, uint32_t h) {
  using A = cult::Archetype;
  using G = Signature;
  // (M10) a people beyond the Mist: one of the six strange signatures no mainland culture has (6.2); the nearest
  // hidden land always sings (Songweaving)
  if (c.hidden) {
    if (isSongLand(c)) return G::Song;
    static const G six[6] = {G::Blood, G::Rune, G::Song, G::Mask, G::Verdant, G::Dream};
    return six[(h >> 5) % 6u];
  }
  const bool b = (h >> 7) & 1u;
  switch (c.archetype) {
    case A::Fjordfolk: return b ? G::Star : G::Ancestor;
    case A::Highland: return b ? G::Ancestor : G::Beast;
    case A::Imperial: return b ? G::Ember : G::Star;
    case A::Dune: return b ? G::Star : G::Ember;
    case A::Steppe: return b ? G::Beast : G::Ancestor;
    case A::Marsh: return b ? G::Tide : G::Beast;
    case A::Jade: return b ? G::Tide : G::Star;
    case A::River: return b ? G::Tide : G::Ember;
    case A::SunTemple: return b ? G::Ember : G::Star;
    case A::Sylvan: return b ? G::Beast : G::Tide;
    case A::Starspire: return G::Star;
    default: return G::Common;
  }
}
Resource resourceOf(Signature s) {
  switch (s) {
    case Signature::Star: return Resource::Starlight;
    case Signature::Ancestor: return Resource::Charge;
    case Signature::Ember: return Resource::Heat;
    case Signature::Blood: return Resource::Health;
    case Signature::Rune: case Signature::Mask: return Resource::Charge;
    case Signature::Song: return Resource::Notes;
    default: return Resource::Magicka;
  }
}
// a spell's own name for the hidden mechanics' spells (empty: the generated one)
std::string mechanicName(Signature sig, const Pick& p, const std::string& who) {
  if (sig == Signature::Mask && p.form == Form::Aura) {
    const int m = p.ess == Essence::Fire ? 0 : p.ess == Essence::Frost ? 1 : 2;
    return std::string("MASK OF THE ") + maskName(m);
  }
  const std::string pre = who.empty() ? std::string() : who + "'S ";
  if (sig == Signature::Verdant && p.form == Form::Blink) return pre + "VINE BRIDGE";
  if (sig == Signature::Verdant && p.form == Form::Nova) return pre + "ROOT SNARE";
  if (sig == Signature::Dream && p.form == Form::Summon) return pre + "DECOY";
  if (sig == Signature::Dream && p.form == Form::Totem) return pre + "STILL HOUR";
  return "";
}

Tradition makeTradition(const cult::Culture& c, uint64_t key, std::vector<SpellDef>& out) {
  Tradition t;
  t.culture = c.id;
  const uint32_t h = hash32((uint32_t)key ^ hash32((uint32_t)(key >> 32) ^ 0x7A11u));
  t.id = 1u + (h % 399999u);   // never 0 (Common Arcana)
  t.sig = signatureFor(c, h);
  t.res = resourceOf(t.sig);
  t.sigilSeed = c.faith.glyphSeed ^ h;
  t.hidden = c.hidden;
  t.minLevel = c.hidden ? (uint8_t)HIDDEN_LEVEL : 12;
  t.name = "THE " + c.adjective + " " + traditionNoun(t.sig, h >> 3);
  if (t.name.size() > 34) t.name = std::string("THE ") + traditionNoun(t.sig, h >> 3);
  t.focus = focusName(t.sig);
  const bool songLandHere = isSongLand(c);
  const std::vector<Pick> P = picksOf(t.sig, h, songLandHere);
  // Songweaving's notes: its three rank-1 spells sing three different degrees, the motif is them in a hashed order
  uint8_t deg[3];
  deg[0] = (uint8_t)(h % 7u);
  deg[1] = (uint8_t)((deg[0] + 2u + (h >> 3) % 3u) % 7u);
  deg[2] = (uint8_t)((deg[1] + 2u + (h >> 6) % 2u) % 7u);
  if (deg[2] == deg[0]) deg[2] = (uint8_t)((deg[2] + 1u) % 7u);
  if (deg[2] == deg[1]) deg[2] = (uint8_t)((deg[2] + 1u) % 7u);
  static const uint8_t perm[6][3] = {{0, 1, 2}, {1, 0, 2}, {2, 0, 1}, {0, 2, 1}, {1, 2, 0}, {2, 1, 0}};
  const uint8_t* pm = perm[(h >> 9) % 6u];
  for (int i = 0; i < 3; i++) t.motif[i] = deg[pm[i]];
  // 8..11 spells: 3 at rank 1, 3..5 at rank 2, 2..3 at rank 3 (the last always the signature)
  const int n = 8 + (int)((h >> 11) % 4u);
  const int r3 = n >= 10 ? 3 : 2;
  const int r2 = n - 3 - r3;
  std::vector<Pick> chosen;
  for (const Pick& p : P) if (p.rank == 1) chosen.push_back(p);
  {
    std::vector<Pick> two;
    for (const Pick& p : P) if (p.rank == 2) two.push_back(p);
    // drop rank-2 picks by the hash until r2 remain (order kept)
    uint32_t hh = hash32(h ^ 0x2222u);
    while ((int)two.size() > r2) {
      const size_t k = hh % (uint32_t)two.size();
      hh = hash32(hh);
      if (two[k].keep) continue;   // (a mechanic's own spell stays: the vine bridge, the third mask, the still hour ...)
      two.erase(two.begin() + (std::ptrdiff_t)k);
    }
    for (const Pick& p : two) chosen.push_back(p);
  }
  {
    std::vector<Pick> three;
    for (const Pick& p : P) if (p.rank == 3) three.push_back(p);
    if (r3 == 2) three.erase(three.begin() + (std::ptrdiff_t)((h >> 17) % 2u));   // never the signature (last)
    for (const Pick& p : three) chosen.push_back(p);
  }
  t.ess[0] = chosen.empty() ? Essence::Arcane : chosen[0].ess;
  t.ess[1] = chosen.size() > 3 ? chosen[3].ess : t.ess[0];
  t.ess[2] = chosen.size() > 1 ? chosen.back().ess : t.ess[0];
  const uint32_t tint = c.faith.colour ? c.faith.colour : c.heraldry.field;
  const uint32_t tint2 = c.heraldry.field ? c.heraldry.field : tint;
  int k1 = 0, k2 = 0;
  for (int i = 0; i < (int)chosen.size(); i++) {
    const Pick& p = chosen[(size_t)i];
    const bool sig = i == (int)chosen.size() - 1;
    // rank 1: novice (the first two) and apprentice; rank 2: apprentice and adept; rank 3: expert
    uint8_t tier = 0;
    if (p.rank == 1) tier = k1++ < 2 ? 0 : 1;
    else if (p.rank == 2) tier = k2++ < 2 ? 1 : 2;
    else tier = 3;
    const uint32_t sid = traditionSpellId(t.id, i);
    std::string nm;
    if (sig) nm = signatureSpellName(t.sig);
    else if (t.sig == Signature::Beast && p.form == Form::Mark) nm = "BIND BEAST";
    else {
      std::string who = cult::personName(c, h ^ (uint32_t)(i * 7919 + 13), (h >> (i & 15)) & 1u);
      if (const size_t sp = who.find(' '); sp != std::string::npos) who = who.substr(0, sp);
      nm = mechanicName(t.sig, p, who);
      if (nm.size() > 24) nm = mechanicName(t.sig, p, "");
      if (nm.empty()) {
        nm = who + "'S " + essenceWord(p.ess) + " " + formNoun(p.form);
        if (nm.size() > 24) nm = std::string(essenceWord(p.ess)) + " " + formNoun(p.form);
      }
    }
    SpellDef s = mk(sid, "", schoolOfForm(p.form, p.ess), p.form, p.ess, tier);
    s.name = nm;
    s.rank = p.rank;
    s.tradition = t.id;
    s.res = t.res;
    if (p.form == Form::Mark && p.ess == Essence::Verdant) { s.duration = 60; s.statuses = 0; }   // Beastbinding: 60 s
    if (p.form == Form::Mark && p.ess == Essence::Bone) s.duration = 12;
    if (p.ess == Essence::Wind && p.form == Form::Cone) s.mods = M_KNOCK;
    if (p.ess == Essence::Verdant && p.form == Form::Nova) s.mods = M_ROOT;
    if (p.ess == Essence::Blood && p.form == Form::Lance) s.mods = M_DRAIN;
    // (M10) the hidden mechanics' spells
    if (p.ess == Essence::Dream && p.form == Form::Cone) { s.statuses = statusBit(Status::Blinded); s.statusSecs = 4; }
    if (p.ess == Essence::Dream && p.form == Form::Mark) s.duration = 20;
    if (p.ess == Essence::Dream && p.form == Form::Storm) { s.statuses = statusBit(Status::Charmed); s.statusSecs = 8; }
    if (t.sig == Signature::Mask && p.form == Form::Aura) {
      s.mask = (int8_t)(p.ess == Essence::Fire ? 0 : p.ess == Essence::Frost ? 1 : 2);
      s.duration = MASK_SECS; s.cooldown = 4; s.castTime = 0.25f; s.dual = false; s.upcast = false;
    }
    if (t.sig == Signature::Verdant && p.form == Form::Blink) { s.range = 64; s.cooldown = 10; s.castTime = 0.35f; s.dual = false; s.upcast = false; }
    if (t.sig == Signature::Verdant && p.form == Form::Nova) s.statuses = 0;
    if (t.sig == Signature::Dream && p.form == Form::Totem) { s.duration = DREAM_SLOW_SECS; s.radius = sig ? 110.0f : 64.0f; }
    if (t.sig == Signature::Dream && p.form == Form::Summon) { s.duration = DECOY_SECS; s.dual = false; }
    if (t.sig == Signature::Song) s.note = (int8_t)(i < 3 ? deg[i] : (uint8_t)(hash32(h ^ (uint32_t)(i * 131 + 7)) % 7u));
    if (sig) {
      s.cooldown = std::max(60.0f, std::min(120.0f, s.cooldown * 1.5f));
      if (s.form == Form::Summon) s.duration = 40;
      if (s.form == Form::Nova) s.radius = 110;
      if (s.form == Form::Aura) s.duration = 30;
    }
    // what it costs in the tradition's resource: Starlight and Heat half the magicka (pools of 100), Charge 1-3
    if (t.res == Resource::Heat || t.res == Resource::Starlight) s.cost = std::round(s.cost * 0.5f);   // (pools of 100)
    else if (t.res == Resource::Health) s.cost = std::max(2.0f, std::round(s.cost * 0.4f));   // (M10 Blood rites: health)
    else if (t.res == Resource::Charge) s.cost = s.mask >= 0 ? 0.0f : (float)p.rank;          // (a mask is donned freely)
    if (t.sig == Signature::Song) s.res = Resource::Magicka;   // (Songweaving's Notes are the motif sung, not a price)
    // the look: the essence's ramp, its middle tinted by the culture's own colours; the particle from the signature
    s.fx = essenceFx(p.ess);
    s.fx.ramp[1] = blend(s.fx.ramp[1], tint2, 70);
    s.fx.ramp[2] = blend(s.fx.ramp[2], tint, 90);
    s.fx.ramp[3] = blend(s.fx.ramp[3], tint, 50);
    switch (t.sig) {
      case Signature::Star: s.fx.particle = Particle::Star; break;
      case Signature::Tide: s.fx.particle = Particle::Droplet; break;
      case Signature::Ancestor: s.fx.particle = p.ess == Essence::Bone ? Particle::Bone : Particle::Smoke; break;
      case Signature::Ember: s.fx.particle = Particle::Ember; break;
      case Signature::Beast: s.fx.particle = p.ess == Essence::Wind ? Particle::Feather : Particle::Leaf; break;
      case Signature::Blood: s.fx.particle = Particle::Droplet; break;
      case Signature::Rune: s.fx.particle = Particle::Glyph; break;
      case Signature::Song: s.fx.particle = Particle::Note; break;
      case Signature::Mask: s.fx.particle = p.ess == Essence::Wind ? Particle::Feather : Particle::Smoke; break;
      case Signature::Verdant: s.fx.particle = p.form == Form::Totem ? Particle::Petal : Particle::Leaf; break;
      case Signature::Dream: s.fx.particle = p.ess == Essence::Shadow ? Particle::Mote : Particle::Petal; break;
      default: break;
    }
    s.fx.glyphSeed = t.sigilSeed ^ (uint32_t)(i * 0x45D9F3Bu);
    s.fx.motif = (uint8_t)(16 + (int)t.sig * 8 + (int)(h % 8u));
    s.fx.size = (uint8_t)(sig ? 4 : p.rank >= 3 ? 3 : tier >= 2 ? 2 : 1);
    s.power = budgetOf(s);
    out.push_back(s);
    t.spells.push_back(sid);
  }
  return t;
}
}  // namespace

const char* schoolName(School s) {
  static const char* n[] = {"DESTRUCTION", "RESTORATION", "CONJURATION", "ILLUSION", "ALTERATION"};
  return (int)s < SCHOOLS ? n[(int)s] : "";
}
skills::Skill schoolSkill(School s) { return (skills::Skill)((int)skills::Skill::Destruction + std::min<int>((int)s, SCHOOLS - 1)); }
const char* formName(Form f) {
  static const char* n[] = {"BOLT", "LANCE", "NOVA", "CONE", "WALL", "RUNE", "AURA", "SUMMON", "WARD", "BLINK", "BEAM", "ORB", "MARK", "MEND", "TOTEM",
                            "STORM", "METEOR"};
  return (int)f < (int)Form::COUNT ? n[(int)f] : "";
}
const char* essenceName(Essence e) {
  static const char* n[] = {"FIRE", "FROST", "STORM", "STONE", "TIDE", "VENOM", "SHADOW", "LIGHT", "BLOOD", "BONE", "WIND", "VERDANT", "STAR", "SONG",
                            "DREAM", "RUST", "ARCANE"};
  return (int)e < (int)Essence::COUNT ? n[(int)e] : "";
}
const char* signatureName(Signature s) {
  static const char* n[] = {"COMMON ARCANA", "BLOOD RITES", "RUNECARVING", "SONGWEAVING", "STARCALLING", "TIDEBINDING", "ANCESTOR CALLS", "EMBERFORGE",
                            "BEASTBINDING", "MASKWORK", "VERDANT SHAPING", "DREAMWEFT"};
  return (int)s < (int)Signature::COUNT ? n[(int)s] : "";
}
const char* statusName(Status s) {
  static const char* n[] = {"BURNING", "CHILLED", "FROZEN", "WET", "SHOCKED", "FRIGHTENED", "CHARMED", "POISONED", "BLINDED"};
  return (int)s < STATUSES ? n[(int)s] : "";
}
uint32_t statusColor(Status s) {
  static const uint32_t c[] = {rgb(255, 120, 40), rgb(140, 200, 255), rgb(200, 240, 255), rgb(80, 140, 230), rgb(190, 190, 255),
                               rgb(200, 120, 220), rgb(255, 140, 200), rgb(130, 210, 70), rgb(230, 230, 200)};
  return (int)s < STATUSES ? c[(int)s] : 0;
}
const char* surfaceName(Surface s) {
  static const char* n[] = {"", "WATER", "ICE", "OIL", "FIRE", "POISON", "SHOCKED WATER", "STEAM", "BLOOD"};
  return (int)s < (int)Surface::COUNT ? n[(int)s] : "";
}
Surface comboOf(Surface g, Essence e) {
  switch (g) {
    case Surface::Water:
      if (e == Essence::Fire) return Surface::Steam;
      if (e == Essence::Frost) return Surface::Ice;
      if (e == Essence::Storm) return Surface::Shock;
      if (e == Essence::Venom) return Surface::Poison;
      break;
    case Surface::Ice: if (e == Essence::Fire) return Surface::Water; break;
    case Surface::Oil: if (e == Essence::Fire) return Surface::Fire; break;
    case Surface::Fire: if (e == Essence::Frost || e == Essence::Tide) return Surface::Steam; break;
    case Surface::Poison: if (e == Essence::Fire) return Surface::Fire; break;
    case Surface::Blood: if (e == Essence::Frost) return Surface::Ice; if (e == Essence::Storm) return Surface::Shock; break;
    case Surface::Shock: if (e == Essence::Frost) return Surface::Ice; break;
    case Surface::Steam: if (e == Essence::Frost) return Surface::Water; if (e == Essence::Wind) return Surface::None; break;
    default: break;
  }
  return Surface::None;
}
const char* tierName(int t) {
  static const char* n[] = {"NOVICE", "APPRENTICE", "ADEPT", "EXPERT", "MASTER"};
  return n[std::clamp(t, 0, TIERS - 1)];
}

// ---- the budget (section 6.3)
float basePower(Form f) {
  static const float b[] = {18, 24, 20, 16, 14, 26, 10, 30, 40, 0, 12, 30, 15, 45, 20, 40, 90};
  return (int)f < (int)Form::COUNT ? b[(int)f] : 0;
}
float essenceMult(Essence e) {
  static const float m[] = {1.0f, 1.0f, 1.05f, 0.95f, 0.95f, 0.9f, 1.0f, 1.0f, 1.1f, 1.0f, 0.9f, 0.9f, 1.05f, 0.95f, 0.9f, 0.95f, 1.0f};
  return (int)e < (int)Essence::COUNT ? m[(int)e] : 1.0f;
}
float budgetOf(const SpellDef& s) {
  static const float tierMult[TIERS] = {1.0f, 1.35f, 1.8f, 2.4f, 3.2f};
  static const float rankMult[4] = {1.0f, 1.0f, 1.15f, 1.30f};
  float b = basePower(s.form) * essenceMult(s.ess) * tierMult[std::clamp<int>(s.tier, 0, TIERS - 1)] * rankMult[std::clamp<int>(s.rank, 0, 3)];
  for (uint16_t m = s.mods; m; m &= (uint16_t)(m - 1)) b *= 0.85f;
  if (s.rank >= 3) b *= 1.1f;
  return b;
}
const std::vector<SpellDef>& schoolSpells() {
  static const std::vector<SpellDef> v = makeSchoolSpells();
  return v;
}
const SpellDef* spellById(uint32_t id) {
  if (id < 1000) {
    for (const SpellDef& s : schoolSpells()) if (s.id == id) return &s;
    return nullptr;
  }
  const Registry& R = reg();
  const auto it = R.spells.find(id);
  return it == R.spells.end() ? nullptr : &it->second;
}
uint32_t legacySpellId(int legacySpell) { return legacySpell >= 0 && legacySpell < 3 ? (uint32_t)legacySpell + 1 : 0; }
int legacySpellOf(uint32_t id) { return id >= 1 && id <= 3 ? (int)id - 1 : -1; }
float magnitudeOf(int upcastLevel, bool dual) { return (1.0f + 0.35f * (float)std::clamp(upcastLevel, 0, 3)) * (dual ? 1.6f : 1.0f); }

bool damaging(const SpellDef& s) {
  switch (s.form) {
    case Form::Bolt: case Form::Lance: case Form::Orb: case Form::Nova: case Form::Cone: case Form::Rune: case Form::Meteor: case Form::Beam:
    case Form::Wall:
      return s.ess != Essence::Dream && !(s.form == Form::Nova && s.ess == Essence::Tide && s.tradition == 0);
    case Form::Storm: return s.ess != Essence::Dream;
    case Form::Totem: return !(s.ess == Essence::Light || s.ess == Essence::Verdant || s.ess == Essence::Tide || s.ess == Essence::Dream);   // (M10: a dream's is a time bubble)
    case Form::Mark: return s.ess != Essence::Dream && s.ess != Essence::Verdant;
    default: return false;
  }
}
float sustainedDps(const SpellDef& s) {
  if (!damaging(s)) return 0;
  if (s.form == Form::Beam || s.form == Form::Wall || s.form == Form::Totem) return s.power;
  if (s.form == Form::Storm) return s.power / 0.7f;
  return s.power / std::max(0.35f, s.cooldown + s.castTime);
}
int teachPrice(const SpellDef& s) {
  if (s.id == SPELL_MEND) return 120;
  if (s.id == SPELL_FROST_LANCE) return 300;
  if (s.tradition) return s.rank >= 3 ? 4000 : s.rank == 2 ? 1500 : 300;
  return s.tier == 0 ? 150 : 300 + 600 * (int)s.tier * (int)s.tier;
}
std::vector<const SpellDef*> spellsOfSchool(School sc) {
  std::vector<const SpellDef*> v;
  for (const SpellDef& s : schoolSpells()) if (s.school == sc) v.push_back(&s);
  std::stable_sort(v.begin(), v.end(), [](const SpellDef* a, const SpellDef* b) { return a->tier < b->tier; });
  return v;
}
float resourceCap(Resource r) {
  switch (r) {
    case Resource::Heat: return HEAT_CAP;
    case Resource::Charge: return CHARGE_CAP;
    case Resource::Notes: return 3.0f;
    case Resource::Starlight: return STARLIGHT_CAP;
    default: return 0;
  }
}
int attuneSlots(int plLevel) { return 1 + (plLevel >= 15 ? 1 : 0) + (plLevel >= 35 ? 1 : 0); }
int masteryRank(uint16_t xp, bool trial) { return xp >= MASTERY_RANK3 && trial ? 3 : xp >= MASTERY_RANK2 ? 2 : 1; }

// ---- traditions
uint32_t traditionSpellId(uint32_t traditionId, int index) { return 1000u + (traditionId % 400000u) * 16u + (uint32_t)(index & 15); }
const Tradition& commonArcana() {
  static const Tradition t = [] {
    Tradition c;
    c.id = 0; c.name = "COMMON ARCANA"; c.sig = Signature::Common;
    for (const SpellDef& s : schoolSpells()) c.spells.push_back(s.id);
    return c;
  }();
  return t;
}
const Tradition& traditionOf(const cult::Culture& c) {
  // the heartland (and any culture before M3 gave it a family) teaches Common Arcana; every other mainland people its own
  if (!c.id || (!c.hidden && c.archetype == cult::Archetype::Heartland && c.archetype2 == cult::Archetype::Heartland)) return commonArcana();
  Registry& R = reg();
  const uint64_t key = cultureKey(c);
  auto it = R.byCulture.find(key);
  if (it != R.byCulture.end()) return it->second;
  std::vector<SpellDef> sp;
  Tradition t = makeTradition(c, key, sp);
  if (t.sig == Signature::Common) return commonArcana();
  for (const SpellDef& s : sp) R.spells[s.id] = s;
  R.byId[t.id] = key;
  return R.byCulture.emplace(key, std::move(t)).first->second;
}
Tradition generateTradition(const cult::Culture& c, std::vector<SpellDef>& spells) {
  spells.clear();
  if (!c.id || (!c.hidden && c.archetype == cult::Archetype::Heartland && c.archetype2 == cult::Archetype::Heartland)) return commonArcana();
  Tradition t = makeTradition(c, cultureKey(c), spells);
  if (t.sig == Signature::Common) { spells.clear(); return commonArcana(); }
  return t;
}
const Tradition* traditionById(uint32_t id) {
  if (!id) return &commonArcana();
  const Registry& R = reg();
  const auto k = R.byId.find(id);
  if (k == R.byId.end()) return nullptr;
  const auto t = R.byCulture.find(k->second);
  return t == R.byCulture.end() ? nullptr : &t->second;
}
const Tradition& traditionOfSpell(const SpellDef& s) {
  const Tradition* t = s.tradition ? traditionById(s.tradition) : nullptr;
  return t ? *t : commonArcana();
}

// ---- tomes
Item makeTome(uint32_t spellId) {
  Item it;
  it.kind = ItemKind::Book;
  it.sub = BOOK_TOME;
  it.seed = spellId;
  it.icon = art::Icon::Book;
  const SpellDef* s = spellById(spellId);
  it.name = std::string("TOME: ") + (s ? s->name : "UNKNOWN SPELL");
  it.tint = s ? s->fx.ramp[2] : rgb(200, 200, 255);
  it.value = s ? (int)(120 + 300 * s->tier * s->tier + 80 * s->rank) : 100;
  it.ilvl = s ? (uint8_t)(1 + 10 * s->tier) : 1;
  return it;
}
bool isTome(const Item& it) { return it.kind == ItemKind::Book && it.sub == BOOK_TOME && it.seed != 0; }

// ---- the book
bool Book::knows(uint32_t id) const { return std::find(known.begin(), known.end(), id) != known.end(); }
bool Book::learn(uint32_t id) {
  if (!id || knows(id)) return false;
  known.push_back(id);
  if (slotOf(id) < 0)
    for (uint32_t& s : slot) if (s == 0) { s = id; break; }
  return true;
}
int Book::slotOf(uint32_t id) const {
  for (int i = 0; i < QUICK_SLOTS; i++) if (id && slot[i] == id) return i;
  return -1;
}
// block version 2 (MAGIC lane): version 1's fields, then the lands (u32 n, u64 each) and the trial kills (u8 n, u8 each).
// Version 1 still reads (tests/fixtures/save_v20.bin)
// block version 3 (M10 ARCANA lane): version 2's fields, then the rites (u32 n, then u32 tradition + u8 bits each).
// Written only when a rite was passed, so a book without one keeps its version-2 bytes (save_v21.bin round-trips)
uint8_t Book::rite(uint32_t tid) const {
  for (const auto& r : rites) if (r.first == tid) return r.second;
  return 0;
}
void Book::passRite(uint32_t tid, int which) {
  if (!tid || which < 0 || which > 1) return;
  for (auto& r : rites) if (r.first == tid) { r.second |= (uint8_t)(1u << which); return; }
  rites.push_back({tid, (uint8_t)(1u << which)});
}
void Book::serialize(std::vector<uint8_t>& out) const {
  out.clear();
  BinW w(out);
  w.u8(rites.empty() ? 2 : 3);   // block version
  w.u32((uint32_t)known.size());
  for (uint32_t k : known) w.u32(k);
  w.u8((uint8_t)QUICK_SLOTS);
  for (uint32_t s : slot) w.u32(s);
  w.u8(sel);
  w.u32((uint32_t)mastery.size());
  for (const auto& kv : mastery) { w.u32(kv.first); w.u16(kv.second); }
  w.u32((uint32_t)attuned.size());
  for (uint32_t a : attuned) w.u32(a);
  w.u32(trials);
  w.u8((uint8_t)Resource::COUNT);
  for (float r : res) w.f32(r);
  w.u32((uint32_t)lands.size());
  for (uint64_t l : lands) { w.u32((uint32_t)l); w.u32((uint32_t)(l >> 32)); }
  w.u8((uint8_t)std::size(trialKills));
  for (uint8_t k : trialKills) w.u8(k);
  if (!rites.empty()) {
    w.u32((uint32_t)rites.size());
    for (const auto& r : rites) { w.u32(r.first); w.u8(r.second); }
  }
}
bool Book::deserialize(const std::vector<uint8_t>& in) {
  BinR r(in);
  const int ver = r.u8();
  if (ver < 1 || ver > 3) return false;
  *this = Book();
  const uint32_t nk = r.u32();
  if (nk > 4096) return false;
  for (uint32_t i = 0; i < nk && !r.bad; i++) { const uint32_t id = r.u32(); if (id && !knows(id)) known.push_back(id); }
  const int ns = r.u8();
  for (int i = 0; i < ns && !r.bad; i++) { const uint32_t s = r.u32(); if (i < QUICK_SLOTS) slot[i] = knows(s) ? s : 0; }
  sel = (uint8_t)(r.u8() % QUICK_SLOTS);
  const uint32_t nm = r.u32();
  if (nm > 4096) return false;
  for (uint32_t i = 0; i < nm && !r.bad; i++) { const uint32_t t = r.u32(); mastery[t] = r.u16(); }
  const uint32_t na = r.u32();
  if (na > 16) return false;
  for (uint32_t i = 0; i < na && !r.bad; i++) attuned.push_back(r.u32());
  trials = r.u32();
  const int nr = r.u8();
  for (int i = 0; i < nr && !r.bad; i++) { const float v = r.f32(); if (i < (int)Resource::COUNT) res[i] = std::isfinite(v) ? std::max(0.0f, v) : 0.0f; }
  if (ver >= 2) {
    const uint32_t nl = r.u32();
    if (nl > 256) return false;
    for (uint32_t i = 0; i < nl && !r.bad; i++) { const uint32_t lo = r.u32(); const uint32_t hi = r.u32(); lands.push_back((uint64_t)lo | ((uint64_t)hi << 32)); }
    const int nt = r.u8();
    for (int i = 0; i < nt && !r.bad; i++) { const uint8_t k = r.u8(); if (i < (int)std::size(trialKills)) trialKills[i] = k; }
  }
  if (ver >= 3) {
    const uint32_t nr2 = r.u32();
    if (nr2 > 256) return false;
    for (uint32_t i = 0; i < nr2 && !r.bad; i++) { const uint32_t t = r.u32(); const uint8_t b = r.u8(); if (t && !rite(t)) rites.push_back({t, b}); }
  }
  return !r.bad;
}

void syncLegacy(Book& B, uint8_t& bits, int& spell, int8_t& mirror) {
  for (int s = 0; s < 3; s++)
    if (bits & (1u << s)) B.learn(legacySpellId(s));
  uint8_t nb = 0;
  for (uint32_t id : B.known) if (const int l = legacySpellOf(id); l >= 0) nb |= (uint8_t)(1u << l);
  bits = nb;
  if (spell != mirror) {
    const int k = B.slotOf(legacySpellId(spell));
    if (k >= 0) B.sel = (uint8_t)k;
  }
  const int l = legacySpellOf(B.selected());
  if (l >= 0) spell = l;
  mirror = (int8_t)spell;
}

// ---- (M10) the hidden signatures' names, the song land
const char* maskName(int m) {
  static const char* n[MASKS] = {"EMBER FOX", "FROST OWL", "STORM STAG"};
  return m >= 0 && m < MASKS ? n[m] : "";
}
Essence maskEssence(int m) { return m == 1 ? Essence::Frost : m == 2 ? Essence::Storm : Essence::Fire; }
uint32_t songLand() { return g_songLand; }
void setSongLand(uint32_t landId) { g_songLand = landId; }

// ---- the field
const SurfaceTile* Field::tile(int tx, int ty) const {
  for (const SurfaceTile& t : tiles) if (t.tx == tx && t.ty == ty) return &t;
  return nullptr;
}
Surface Field::at(int tx, int ty) const {
  const SurfaceTile* t = tile(tx, ty);
  return t ? t->kind : Surface::None;
}

}  // namespace magic
