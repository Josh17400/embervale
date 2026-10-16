// M9 "Arcana & Mastery" (VISION_PLAN 6, 13 M9, 15.5, 15.29, 15.30): magic. The Skyrim schools levelled by use (they are
// skills: skills::Skill::Destruction ..), the culture traditions on top (section 6: a culture's way of doing magic, a
// hand-coded signature mechanic, everything else generated), and the BG3 layer: elemental surfaces and combos,
// concentration, upcasting, dual-casting, a prepared loadout of 3-4 quick-cast slots, reaction casts and statuses.
// Contract written by the M9 lead (phase A, 2026-10-12). The MAGIC lane is its only editor (it may ADD declarations and
// fields; it never renames, removes or changes the meaning of what is listed here) and implements it in
// rpg/sim/magic.cpp (the schools' spells, traditions, the budget, the book block), rpg/sim/magic_perks.cpp (the schools'
// perk trees) and rpg/sim/magic_game.cpp (MagicOps: casting, surfaces, statuses, teachers, tomes; rpg/sim/magic_ops.h).
//
// The sim says WHAT happens (Ev::Spell events in phases, ProjKind::Spell projectiles, surfaces, statuses); the view
// (rpg/view/render_magic.cpp, the SPELLFX lane) decides how it LOOKS and SOUNDS from the spell's FxStyle and school.
// Spell sounds are the view's: the sim plays none for a spell (it emits Ev::Spell phases instead).
// Determinism: generation (traditions, their spell lists, names) is a pure function of the culture id; nothing here
// draws from Game::rng_ except combat rolls MagicOps makes in play.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "rpg/sim/common.h"
#include "rpg/sim/items.h"
#include "rpg/sim/skills.h"

namespace cult { struct Culture; }

namespace magic {

// ---- the schools (15.5): each is a skill (schoolSkill) with its perk tree (schoolPerks)
enum class School : uint8_t { Destruction, Restoration, Conjuration, Illusion, Alteration, COUNT };
constexpr int SCHOOLS = (int)School::COUNT;
const char* schoolName(School s);                 // "DESTRUCTION" ...
skills::Skill schoolSkill(School s);
// the schools' perk trees (skills::perkTable routes the magic skills here); never null
const skills::PerkDef* schoolPerks(skills::Skill s, int& n);

// ---- the grammar (section 6.1). Saved inside spell ids only through the tables: append only.
enum class Form : uint8_t { Bolt, Lance, Nova, Cone, Wall, Rune, Aura, Summon, Ward, Blink, Beam, Orb, Mark, Mend, Totem,
                            Storm, Meteor, COUNT };
enum class Essence : uint8_t { Fire, Frost, Storm, Stone, Tide, Venom, Shadow, Light, Blood, Bone, Wind, Verdant, Star, Song,
                               Dream, Rust, Arcane, COUNT };
const char* formName(Form f);
const char* essenceName(Essence e);               // "FIRE", "FROST", "STORM" ... (spell names use them)
enum Mod : uint16_t { M_CHAIN = 1, M_SPLIT = 2, M_DELAY = 4, M_ECHO = 8, M_HOMING = 16, M_LINGER = 32, M_DRAIN = 64,
                      M_KNOCK = 128, M_ROOT = 256, M_PULL = 512 };
enum class Resource : uint8_t { Magicka, Health, Heat, Charge, Notes, Starlight, COUNT };
// the twelve signature mechanics (section 6.2); mainland cultures get 1 and 5..9, hidden kingdoms the strange ones
enum class Signature : uint8_t { Common, Blood, Rune, Song, Star, Tide, Ancestor, Ember, Beast, Mask, Verdant, Dream, COUNT };
const char* signatureName(Signature s);           // "COMMON ARCANA", "BLOOD RITES", "STARCALLING" ...

// ---- statuses (15.5: burning, chilled, frozen, wet, shocked, frightened, charmed, poisoned, blinded). On Actor::status
//      (bits) and Actor::statusT (seconds left each); never saved.
enum class Status : uint8_t { Burning, Chilled, Frozen, Wet, Shocked, Frightened, Charmed, Poisoned, Blinded, COUNT };
constexpr int STATUSES = (int)Status::COUNT;
constexpr uint16_t statusBit(Status s) { return (uint16_t)(1u << (unsigned)s); }
const char* statusName(Status s);                 // "BURNING" ... (the plate over a foe, the HUD)
uint32_t statusColor(Status s);                   // rgba: its tint (the plate, the view's overlay)

// ---- surfaces (15.5: ice, water, oil, fire, poison clouds, electrified water, rain dousing fire) on the ground of the
//      current map, tile by tile (Field::tiles). The MAGIC lane runs them and their combos; the view draws them.
enum class Surface : uint8_t { None, Water, Ice, Oil, Fire, Poison, Shock, Steam, Blood, COUNT };
const char* surfaceName(Surface s);               // "WATER", "ICE", "OIL", "FIRE", "POISON", "SHOCKED WATER", "STEAM", "BLOOD"
// what an essence does to a surface it lands on (the combo table: fire on oil -> a fire, fire on water -> steam, frost on
// water -> ice, storm on water -> shock, fire on ice -> water, ...). None: no change
Surface comboOf(Surface ground, Essence e);

// ---- what the view needs to make every spell look like itself (15.29.7: "tell a spell apart by its silhouette alone")
enum class Particle : uint8_t { Ember, Shard, Petal, Glyph, Note, Star, Droplet, Smoke, Spark, Mote, Bone, Leaf, Feather, COUNT };
enum class Motion : uint8_t { Boil, Shatter, Fork, Rise, Tear, Shimmer, Drift, Spiral, Pulse, Fall, COUNT };
struct FxStyle {
  uint32_t ramp[5] = {};         // rgba, dark to hot (the school's / the culture tradition's palette)
  Particle particle = Particle::Ember;
  Motion motion = Motion::Boil;
  uint32_t glyphSeed = 0;        // the runes that gather in the wind-up, the sigil on the ground
  uint8_t motif = 0;             // the sound motif (the view plays it in the culture's scale: SPELLFX lane)
  uint8_t size = 1;              // 1 a bolt .. 4 a spectacle (meteor, storm cell, great summon): the base scale
};

// ---- a spell (section 6.1 + 15.5)
constexpr int TIERS = 5;                          // novice, apprentice, adept, expert, master (school skill 0/25/50/75/100)
const char* tierName(int tier);                   // "NOVICE" ...
struct SpellDef {
  uint32_t id = 0;               // stable: the schools' own < 1000 (SPELL_*); a tradition's from traditionSpellId
  std::string name;              // "FIREBOLT", "VEL'KA'S EMBER CHOIR"
  School school = School::Destruction;
  Form form = Form::Bolt;
  Essence ess = Essence::Fire;
  uint16_t mods = 0;             // Mod bits
  uint8_t tier = 0;              // 0..4: the school skill it needs is 25 * tier (a perk may lower it)
  uint8_t rank = 1;              // a tradition's rank 1..3 (3: its signature spell); 1 for the schools' own
  float power = 0;               // damage (a heal: health; a ward: points absorbed; a summon: its strength) at item / player
                                 // level 1, before upcast / dual-cast: what budgetOf checks (--spells, +-5 %)
  float cost = 0;                // in its resource (magicka for the schools)
  float cooldown = 0;            // s
  float castTime = 0;            // s of wind-up before the launch (0: instant)
  float range = 0, radius = 0;   // px
  float duration = 0;            // s (a concentration spell's most; a status, a summon, a ward, a surface's life)
  float speed = 0;               // px/s of its missile (0: no missile)
  bool concentration = false;    // held: breaks on a heavy hit (MagicOps::hurt), one at a time (BG3)
  bool upcast = true;            // hold to pour in more (Cast::level up to 3)
  bool dual = true;              // dual-cast (both hands: overcharge)
  bool reaction = false;         // may be cast as a reaction to a telegraphed enemy cast (counterspell / shield)
  Surface leaves = Surface::None;   // what it leaves on the ground where it lands
  uint16_t statuses = 0;         // statusBit()s it applies
  float statusSecs = 0;
  Resource res = Resource::Magicka;
  uint32_t tradition = 0;        // 0: the schools' own (Common Arcana); else the Tradition::id it belongs to
  FxStyle fx;
  // (M10 ARCANA lane) the hidden signatures' marks: note, the scale degree (0..6) a Songweaving spell sings (-1 none);
  // mask, which of Maskwork's three spirit masks an Aura of that tradition dons (0 ember fox, 1 frost owl, 2 storm
  // stag; -1 none)
  int8_t note = -1;
  int8_t mask = -1;
};
// the budget of a spell (section 6.3): basePower(form) x essenceMult x rankMult x tierMult x 0.85 per mod; the
// signature (rank 3) x1.1; hidden traditions no more than +10 %. --spells: every spell's power within +-5 % of it
float budgetOf(const SpellDef& s);
float basePower(Form f);
float essenceMult(Essence e);
// the schools' own spells (Common Arcana and the five schools' novice..master lists)
const std::vector<SpellDef>& schoolSpells();
// a spell by id: the schools' own, or a tradition's that traditionOf has made this session; nullptr: unknown
const SpellDef* spellById(uint32_t id);
// the classic three (pre-M9 Spell enum Flames, Heal, IceSpike = ids 1, 2, 3: legacySpell maps them)
constexpr uint32_t SPELL_FIREBOLT = 1, SPELL_MEND = 2, SPELL_FROST_LANCE = 3;
uint32_t legacySpellId(int legacySpell);          // Spell::Flames -> SPELL_FIREBOLT ...; -1 / unknown -> 0
int legacySpellOf(uint32_t id);                   // the inverse (-1: not one of the classic three)
struct Book;
// the pre-M9 fields (Game::spellsKnown bits, Game::spell) folded into the book and mirrored back (pure: MagicOps::
// syncLegacy runs it on the game; Game::serialize on copies, so a save and its load agree byte for byte). mirror: the
// spell value last mirrored (an outside change of `spell` selects the slot holding it)
void syncLegacy(Book& b, uint8_t& bits, int& spell, int8_t& mirror);

// ---- traditions (section 6)
struct Tradition {
  uint32_t id = 0;               // stable: a hash of the culture id (0: Common Arcana)
  uint64_t culture = 0;          // cult::CultureId
  std::string name;              // "THE NINEFOLD CHOIR", "ASHWAKE RITES"
  Signature sig = Signature::Common;
  Resource res = Resource::Magicka;
  Essence ess[3] = {Essence::Fire, Essence::Frost, Essence::Storm};
  std::vector<uint32_t> spells;  // 8..12 ids (3 rank 1, 3-5 rank 2, 2-3 rank 3 incl. one signature)
  uint32_t sigilSeed = 0;
  bool hidden = false;
  uint8_t minLevel = 1;          // 1 common, 12 foreign mainland, 15 hidden valley, 25 beyond the Mist
  // (M10 ARCANA lane) Songweaving's 3-note motif (scale degrees 0..6, the notes of three of its spells: casting them in
  // this order sings the Refrain); the tradition's focus, sold only in its own land ("SINGING BOWL", "RUNE-KNIFE" ...)
  uint8_t motif[3] = {0, 0, 0};
  std::string focus;
};
uint32_t traditionSpellId(uint32_t traditionId, int index);   // >= 1000
// the tradition of a culture (generated once per culture and memoised; its spells become known to spellById). Phase A:
// every culture teaches Common Arcana (sig Common, the schools' own spells); the MAGIC lane generates the real ones
const Tradition& traditionOf(const cult::Culture& c);
const Tradition& commonArcana();
// (MAGIC lane) the pure generator behind traditionOf: the tradition and its spells, made afresh (nothing registered;
// --spells checks two generations agree)
Tradition generateTradition(const cult::Culture& c, std::vector<SpellDef>& spells);
// (MAGIC lane) a tradition traditionOf has made this session (0: Common Arcana); nullptr: not made yet
const Tradition* traditionById(uint32_t id);
// the tradition a spell belongs to (Common Arcana for the schools' own)
const Tradition& traditionOfSpell(const SpellDef& s);
// 6.3 attunement: 1 tradition slot at start, a 2nd at level 15, a 3rd at level 35
int attuneSlots(int plLevel);
// 6.4 mastery: rank 1 from the start, rank 2 at 300 XP, rank 3 at 1200 XP and the trial passed
constexpr uint16_t MASTERY_RANK2 = 300, MASTERY_RANK3 = 1200;
int masteryRank(uint16_t xp, bool trialPassed);
constexpr int TRIAL_KILLS = 10;   // the Trial of the Threshold: kills under the tradition's affinity once at 1200 XP
// the signatures' numbers (6.2)
constexpr float STAR_FULL_MOON = 1.5f, TIDE_MULT = 1.6f, ANCESTOR_MULT = 1.4f;
constexpr float HEAT_CAP = 100.0f, CHARGE_CAP = 5.0f, STARLIGHT_CAP = 100.0f;
float resourceCap(Resource r);
// the full moon comes every 8 days (6.2 Starcalling)
inline bool fullMoonDay(int day) { return ((day % 8) + 8) % 8 == 4; }
// ---- (M10 ARCANA lane) the hidden kingdoms' signatures (6.2 #2, 3, 4, 10, 11, 12), their numbers and the five steps
//      of learning one (6.4)
//   Blood rites    spells cost health; power x(1 + BLOOD_POWER (1 - hp / maxHp)); the hero's spell healing is halved
//   Runecarving    runes laid on the ground, triggered by a foe's step; two of the hero's runes within RUNE_LINK px go
//                  off together as a combo (fire + frost: a steam burst ...); charge 5 refills one every RUNE_CHARGE_SECS
//   Songweaving    every cast sings its note (SpellDef::note); three casts in the tradition's motif order sing a free
//                  Refrain of the third at x REFRAIN_MULT (Book::res[Notes] holds the motif's progress 0..3)
//   Maskwork       three spirit masks (the tradition's Auras: SpellDef::mask) worn MASK_SECS; while one is worn every
//                  melee blow carries its essence and builds a charge (5), and the swing's finisher is a spell-strike
//   Verdant        thorn walls (solid, VERDANT_SECS), root snares (held), a vine bridge over up to VINE_SPAN water tiles
//                  (Ground::Bridge): the only magic that edits the map, and every tile is restored exactly
//   Dreamweft      decoys of the hero that pull foes' attacks (DECOY_SECS), a bubble where foes move at DREAM_SLOW
constexpr float BLOOD_POWER = 0.8f, BLOOD_HEAL = 0.5f;
constexpr float RUNE_LINK = 48.0f, RUNE_CHARGE_SECS = 3.5f, RUNE_COMBO_MULT = 1.5f;
constexpr float NOTES_CAP = 3.0f, REFRAIN_MULT = 2.0f;
constexpr int MASKS = 3;
constexpr float MASK_SECS = 60.0f;
constexpr float VERDANT_SECS = 30.0f, SNARE_SECS = 2.5f;
constexpr int VINE_SPAN = 3;
constexpr float DREAM_SLOW = 0.4f, DREAM_SLOW_SECS = 3.0f, DECOY_SECS = 8.0f, DECOY_PULL = 150.0f;
constexpr int HIDDEN_LEVEL = 25, REP_FRIENDLY = 25, REP_HONOURED = 60;
inline bool hiddenSignature(Signature s) {
  return s == Signature::Blood || s == Signature::Rune || s == Signature::Song || s == Signature::Mask || s == Signature::Verdant ||
         s == Signature::Dream;
}
const char* maskName(int mask);                   // "EMBER FOX", "FROST OWL", "STORM STAG"
Essence maskEssence(int mask);
// the NEAREST hidden land of the world being played (WORLD lane's ew::LandPlan id; 0 unknown): its people's tradition
// is Songweaving with Fire among its essences, its priests sing fire into being at dusk. MagicOps sets it once per
// world (before any of its traditions is made); generation reads it (a pure function of the culture and this id)
uint32_t songLand();
void setSongLand(uint32_t landId);
// does the spell deal damage (its power is damage: a bolt, a lance, a nova, a cone, a beam, a wall, a storm, a meteor...)
bool damaging(const SpellDef& s);
// its sustained damage a second at item / player level 1 (power over its cycle: cast time + cooldown, the shared 0.35 s
// at least; a beam / wall / storm: its power is already a second's): what the tradition-vs-Common check compares
float sustainedDps(const SpellDef& s);
// a teacher's price for the spell (gold) and a tome's (Item::value)
int teachPrice(const SpellDef& s);
// the school skill a spell needs (25 per tier)
inline int skillNeed(const SpellDef& s) { return 25 * (int)s.tier; }
// every school spell of a school, novice first
std::vector<const SpellDef*> spellsOfSchool(School sc);

// ---- learning (15.29.4, 6.4): tomes are ItemKind::Book with Item::sub == BOOK_TOME and Item::seed = the spell id
constexpr uint8_t BOOK_TOME = 1;
Item makeTome(uint32_t spellId);
bool isTome(const Item& it);

// ---- the player's spellbook (saved: the magic block, SAVE_VER 20; its own version byte first)
constexpr int QUICK_SLOTS = 4;   // 15.5: "a limited prepared-spell loadout ... the phone UI to 3-4 quick-cast buttons"
struct Book {
  std::vector<uint32_t> known;                // spell ids learned
  uint32_t slot[QUICK_SLOTS] = {};            // the prepared loadout (0: empty); swapped at rest or camp
  uint8_t sel = 0;                            // the selected slot (the single SPELL button / key casts it)
  std::map<uint32_t, uint16_t> mastery;       // tradition id -> mastery XP (6.4: rank 2 at 300, rank 3 at 1200 + trial)
  std::vector<uint32_t> attuned;              // traditions attuned (6.3: 1 slot, a 2nd at level 15, a 3rd at 35)
  uint32_t trials = 0;                        // bit per attuned tradition's trial passed (by attuned index)
  float res[(int)Resource::COUNT] = {};       // the non-magicka pools (Heat, Charge, Notes, Starlight)
  // (MAGIC lane, block version 2) the cultures whose traditions this book has touched (attuned or learned from): a load
  // regenerates their traditions so spellById knows their spells again; and the trial's progress per attuned index
  // (the Trial of the Threshold: kills under the tradition's affinity once mastery reaches rank 3's 1200 XP)
  std::vector<uint64_t> lands;
  uint8_t trialKills[4] = {};
  // (ARCANA lane, block version 3, written only when not empty: a book without it keeps writing version 2) the hidden
  // traditions' Trials of the Threshold passed (6.4 step 4 and the rank-3 trial): tradition id -> bit 0 the first
  // trial, bit 1 the second
  std::vector<std::pair<uint32_t, uint8_t>> rites;
  uint8_t rite(uint32_t traditionId) const;
  void passRite(uint32_t traditionId, int which);
  bool knows(uint32_t id) const;
  bool learn(uint32_t id);                    // false: known already. A new spell fills the first empty slot
  int slotOf(uint32_t id) const;              // -1 not prepared
  uint32_t selected() const { return sel < QUICK_SLOTS ? slot[sel] : 0; }
  void serialize(std::vector<uint8_t>& out) const;
  bool deserialize(const std::vector<uint8_t>& in);
};

// ---- runtime (never saved; Arcana::live). The MAGIC lane adds fields as it needs them.
struct Cast {                    // the cast in progress (15.29.7: wind-up -> launch)
  uint32_t spell = 0;            // 0: none
  int8_t slot = -1;
  float t = 0;                   // s since the wind-up began
  uint8_t level = 0;             // upcast level 0..3 (hold to pour in more: Ev::Spell Phase::Charge at each step)
  bool dual = false;             // dual-cast (both hands)
  bool held = false;             // the button is still held
  Vec2 aim;
};
struct Conc {                    // the concentration spell held (one at a time)
  uint32_t spell = 0;
  float t = 0, left = 0;
  int target = -1;               // actor id (a charmed foe, a summon...) or -1
  float mag = 1;
  float tick = 0;                // (MAGIC lane) s to the next channel tick
  float power = 0;               // the per-second power a beam deals (already scaled)
  int caster = -1;               // whose concentration (the hero's)
};
// (MAGIC lane) a spell's lasting place in the world: a wall of fire, a storm cell, a healing circle, a rune waiting to
// be stepped on, a meteor falling, a totem
enum class ZoneKind : uint8_t { Burn, Heal, Storm, Rune, Meteor, Totem };
struct Zone {
  uint32_t spell = 0;
  ZoneKind kind = ZoneKind::Burn;
  Vec2 p, q;                     // its centre (a wall: from p to q)
  float r = 0, life = 0, tick = 0, power = 0, mag = 1, delay = 0;
  int owner = -1;
  uint32_t seed = 0;
  bool conc = false;             // ends with the concentration that holds it
};
// (MAGIC lane) a foe made to fight for the hero (charm, a bound beast, mayhem) and what it was before
struct Turned {
  int id = -1;
  uint8_t faction = 0;           // its Faction before
  bool hostile = true;
  float t = 0;                   // s left
  uint32_t spell = 0;
  bool conc = false;
};
// Actor::magicTag values (the MAGIC lane's per-actor scratch; magicT its time left, magicBy the caster)
enum : uint8_t { MT_NONE = 0, MT_SUMMON = 1, MT_CHARM = 2, MT_CALM = 3, MT_PARALYZE = 4, MT_RAISE = 5, MT_BOUND = 6,
                 MT_DECOY = 7 };   // (M10) a Dreamweft decoy of the hero: foes near it attack it instead
// (M10 ARCANA lane) a tile Verdant shaping changed for a while: a thorn wall's (solid), a vine bridge's (Ground::Bridge
// over water), restored exactly when it ends (never under a body: it waits, then the body is set on firm ground)
enum : uint8_t { GROW_THORN = 0, GROW_VINE = 1 };
struct Growth {
  uint8_t kind = GROW_THORN;
  int16_t tx = 0, ty = 0;        // tile of the map it grew on
  uint8_t ground = 0;            // the Ground byte it had
  uint8_t solid = 0;             // the solid byte it had
  bool over = true;              // on the overworld (World::over), else on the sub-map it was laid in
  uint8_t order = 0;             // a bridge's tile from the near bank (0) out (the view grows them one after another)
  float life = 0, age = 0, grace = 0;   // s left / since grown / waited for a body to step off
  uint32_t seed = 0;
  uint32_t spell = 0;
};
// a Dreamweft bubble: foes inside move, swing and burn at DREAM_SLOW
struct Bubble { Vec2 p; float r = 0, life = 0, age = 0; uint32_t spell = 0; };
// the Trial of the Threshold the hero stands in (rpg/sim/rooms.h TrialLayout; MagicOps runs its puzzle)
struct TrialRun {
  uint64_t site = 0;             // the trial's site id (0: not in a trial)
  uint32_t gatesOpen = 0;        // bit per TrialLayout gate
  uint64_t platesDown = 0;       // bit per plate the hero (or a decoy) stands on now
  uint64_t paid = 0;             // blood altars paid (bit per plate)
  uint8_t prog = 0;              // Song: notes of the motif matched so far
  uint8_t room = 0;              // Song: the hall the last note was sung in
  uint8_t seen = 0;              // bit per hall whose lesson was told
  float t = 0, msgT = 0;
  int decoy = -1;                // Dream: the decoy the mirror made (actor id)
  bool passed = false;
};
struct SurfaceTile {
  int16_t tx = 0, ty = 0;        // tile of the current map
  Surface kind = Surface::None;
  uint8_t power = 1;             // 1..3: how strong (an upcast fire burns harder)
  float life = 0, age = 0;       // s left / s since laid (the view fades edges in and out by them)
  uint32_t seed = 0;
  int owner = -1;                // actor id whose spell laid it (kills credited to them)
};
struct Field {
  std::vector<SurfaceTile> tiles;
  uint32_t serial = 0;           // bumped whenever a tile is added, removed or changes kind (the view's cache key)
  Surface at(int tx, int ty) const;
  const SurfaceTile* tile(int tx, int ty) const;
  void clear() { tiles.clear(); serial++; }
};
struct Stats {                   // counters (tests, scripts, the view's perf report)
  int casts = 0, hits = 0, kills = 0, upcasts = 0, duals = 0, combos = 0, reactions = 0, fizzles = 0, concBroken = 0;
  // (M10) the hidden signatures
  int notes = 0, refrains = 0, runeTriggers = 0, runeCombos = 0, maskStrikes = 0, maskHits = 0, growths = 0, reverts = 0,
      snares = 0, decoys = 0, decoyHits = 0, slowedSteps = 0, bubbles = 0, songFires = 0, gates = 0, bloodPaid = 0;
  int statusApplied[STATUSES] = {};
  int surfacesLaid[(int)Surface::COUNT] = {};
};
struct Live {
  Cast cast;
  Conc conc;
  Field field;
  Stats stats;
  float cd[QUICK_SLOTS] = {};    // each slot's cooldown left
  float gcd = 0;                 // the shared cooldown after any cast
  int8_t legacyMirror = -1;      // the Game::spell value last mirrored from the book (an outside change selects its slot)
  // ---- (MAGIC lane) what the spells leave running
  std::vector<Zone> zones;
  std::vector<Turned> turned;
  float ward = 0, wardT = 0;     // a ward's points left and its time (it soaks blows first)
  uint32_t wardSpell = 0;
  float auraDR = 0, auraT = 0;   // oakflesh / stoneskin: the share of every blow stopped
  uint32_t auraSpell = 0;
  float boundDmg = 0, boundT = 0;   // a bound blade: damage added to every blow
  float regen = 0, regenT = 0;   // a regeneration aura: health a second
  float blur = 0, blurT = 0;     // a blur: the chance a foe's blow misses
  float invisT = 0;              // invisible (ends on the hero's own attack)
  int imbueHits = 0;             // Emberforge: the next blows carry the essence of the spell last cast
  Essence imbueEss = Essence::Fire;
  float imbuePow = 0;
  float reactCd = 0;             // a reaction cast's own cooldown
  float prepT = 0;               // s the loadout may still be changed after a rest (15.5: at rest or camp)
  int lastKills = -1;            // Game::kills last seen (Ancestor calls: charge from kills)
  int spellDepth = 0;            // > 0 while a spell's own damage is being dealt (no Heat, no bound blade, no imbue)
  int8_t rainOverride = -1;      // tests / scripts: 1 force rain, 0 force dry (-1 the land's own weather)
  int8_t moonOverride = -1;      // tests / scripts: 1 full moon, 0 not
  float avoidDeathDay = -1;      // Restoration's AVOID DEATH: the day it last saved the hero
  std::vector<std::pair<int, float>> chained;   // actor ids already struck by the chain in flight (id, s left)
  std::map<uint32_t, float> tradHitT;  // tradition id -> Game::time its spell last struck a foe (mastery: kills under it)
  float heatIdle = 0;            // Emberforge: s since the last blow (Heat cools after 4 s)
  float shockT = 0;              // the electrified-water tick
  float rainT = 0;               // the rain's dousing tick
  // a quick-cast press that came while the hero was still busy from the last cast (shootCd): kept for a moment and
  // cast as soon as the hero can (tap-then-hold chaining on the phone); bufHeld: the press was a hold
  int8_t bufSlot = -1;
  bool bufHeld = false, bufDual = false;
  float bufUntil = 0;            // Game::time the buffered press expires
  // ---- (M10 ARCANA lane) the hidden signatures' runtime
  int8_t lastNote = -1;          // Songweaving: the note last sung (the view's glyph) and when
  float noteT = 0;
  bool refrain = false;          // the cast being made is a Refrain (free, x REFRAIN_MULT)
  int8_t mask = -1;              // Maskwork: the spirit mask worn (-1 none), its time left, the spell that donned it
  float maskT = 0;
  uint32_t maskSpell = 0;
  bool maskFired = false;        // the finisher's spell-strike of this swing went off
  float runeT = 0;               // Runecarving: the charge's clock
  std::vector<Growth> growths;   // Verdant shaping
  std::vector<std::pair<int, float>> snared;   // root-snared actor ids, s left (the view's roots)
  std::vector<Bubble> bubbles;   // Dreamweft
  TrialRun trial;
  std::map<int, float> songT;    // dusk song-fire: priest actor id -> s to its next verse
  float songScanT = 0;
  int trialKingdoms = -1;        // World::kingdoms.size() the trial scan last saw
  uint64_t songWorld = 0;        // the world (seed + 1) the song land was found for
  int16_t repOverride = -1000;   // tests / scripts: the hero's standing with the land (-1000: the realm's own)
};
struct Arcana {
  Book book;                     // saved
  Live live;                     // never saved
};

// ---- Ev::Spell (game.h): the phases of a spell's life (15.29.7), for the view. Event fields: a = (int)Phase,
//      spell = the spell id, p = where (the caster's hands for WindUp / Charge / Launch; the point of impact), q = the
//      aim / the far point (a beam's end, a chain's next link), f = magnitude (1 a plain cast; upcast and dual-cast
//      raise it: magnitudeOf), id = the caster's actor id (the view follows its hands), s = "" (or a status / surface
//      name for StatusOn / Combo)
enum class Phase : uint8_t {
  WindUp,       // the hands or staff glow, runes gather, a hum (castTime > 0, or a hold begun)
  Charge,       // an upcast step reached while held (f = the new magnitude)
  Launch,       // the release: a flash, a recoil, a small screen kick (the missile, if any, is born now)
  Impact,       // it lands: a burst, sparks or debris, a light flash on nearby tiles, a hit-stop, a light shake
  Aftermath,    // what it leaves: scorch marks, frost decals, puddles, lingering embers or smoke (p = where, f = size)
  Fizzle,       // not enough magicka / interrupted: a puff
  Channel,      // a concentration / beam spell's tick while held (p -> q)
  Release,      // a concentration spell ends (broken or let go)
  Combo,        // a surface combo went off (s = the new surface's name: "STEAM", "SHOCKED WATER", an oil fire's burst)
  StatusOn,     // a status took hold on an actor (id = that actor, s = statusName)
  Summon,       // a summoned form climbs out of its rift (p = where, id = the summon's actor id)
  Ward,         // a ward raised / struck (p = the wearer)
  Blink,        // a blink: p from, q to
  Reaction,     // a reaction cast (counterspell / shield) answered a telegraphed enemy cast
  COUNT
};
// the magnitude of a cast: 1 plain, + 0.35 a level of upcast, x1.6 dual-cast (15.29.7: "visibly bigger")
float magnitudeOf(int upcastLevel, bool dual);

}  // namespace magic
