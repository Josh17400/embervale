// M9 "Arcana & Mastery" (VISION_PLAN 15.29, 15.30): the learn-by-use skill system for everything, its perk trees, foe
// tags and the weapon families' and armour classes' trade-offs. Contract written by the M9 lead (phase A, 2026-10-12).
// The SKILLS lane is its only editor (it may ADD declarations and tune numbers; it never renames, removes or changes the
// meaning of what is listed here) and implements it in rpg/sim/skills.cpp (data, the curve, tags, tables, the block),
// rpg/sim/skills_perks.cpp (the non-magic perk trees) and rpg/sim/skills_game.cpp (SkillOps: the Game hooks).
// The magic schools' perk trees are the MAGIC lane's (rpg/sim/magic_perks.cpp, magic::schoolPerks): perkTable routes
// to them, so the UI draws every tree the same way.
//
// The owner's rules this file carries:
//   - each skill rises 1..100 by DOING the thing (and from no other action), at a pace that slows as it climbs: about
//     10-20 h of play to master one (rpg_test --skills measures it);
//   - character level rises from the total of skill gains, alongside the existing XP (SkillOps grants it through
//     Game::gainXp, so the M8b loot gates that read the player level follow);
//   - every weapon family wins and loses against the right foe tags, and the UI says so in plain words;
//   - light / medium / heavy armour trade stamina, noise, the roll, speed and damage reduction;
//   - a perk point per character level; each perk needs a skill level; perks change how the game plays.
// Determinism: nothing here draws from Game::rng_ (gains, perks and tags are pure functions of the state).
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "rpg/art.h"
#include "rpg/sim/backgrounds.h"
#include "rpg/sim/items.h"

struct Actor;
enum class SiteType : uint8_t;   // rpg/sim/world.h
enum class Role : uint8_t;   // rpg/sim/world.h

namespace skills {

// ---- the skills. Saved by index (the skills block): append only.
enum class Skill : uint8_t {
  // weapon families (15.29.1): one per family; WeaponType::Greatsword trains Sword
  Sword, Axe, Mace, Spear, Dagger, Bow, Unarmed, Block,
  // armour (15.29.1)
  LightArmour, MediumArmour, HeavyArmour,
  // material and culture mastery (15.29.1): wearing a culture's make / a material builds familiarity; its tree holds
  // the full-set and material perks (familiarity per material and per culture: Skills::matFam / cultFam)
  Heritage,
  // movement and athletics
  Athletics,
  // broad skills
  Smithing, Cooking, Farming, Riding, Speech, Stealth, Lockpicking,
  // the magic schools (15.5, 15.29.3): their perk trees are the MAGIC lane's (magic::schoolPerks)
  Destruction, Restoration, Conjuration, Illusion, Alteration,
  COUNT
};
constexpr int SKILLS = (int)Skill::COUNT;
constexpr int MAX_LEVEL = 100;
constexpr int START_LEVEL = 5;          // a new hero's level in every skill (a background adds to its own: startLevels)
enum class Group : uint8_t { Combat, Armour, Body, Craft, Social, Magic, COUNT };
struct SkillInfo {
  const char* name;      // "SWORDS", "HEAVY ARMOUR", "DESTRUCTION"
  const char* line;      // what raises it, one HUD line: "HIT FOES WITH A SWORD"
  Group group;
  uint32_t color;        // rgba: the constellation's star colour (the UI's tree, the skill-up toast)
  uint32_t glyphSeed;    // the culture-glyph / constellation shape seed (the UI draws the tree's figure from it)
};
const SkillInfo& skillInfo(Skill s);
const char* groupName(Group g);         // "COMBAT", "ARMOUR", "BODY", "CRAFT", "SOCIAL", "MAGIC"
inline bool isMagic(Skill s) { return s >= Skill::Destruction && s < Skill::COUNT; }

// ---- the gain curve (15.29.1: slowing gains; ~10-20 h to master). One "use" of base 1 is one ordinary act (a blow
//      landed, an arrow that hits, a spell that lands, a blow taken in armour, a piece forged, a meal cooked...).
//      SkillOps::use scales the act's base by what it was (a kill, a crit, a hard foe: more).
float xpToNext(int level);              // use points from level to level + 1 (level 1..99)
float xpToReach(int from, int to);      // the sum (tests: the time to master)
// the character XP a skill rising to `newLevel` gives (Skyrim's rule, scaled by the SKILLS lane to keep --metrics):
// SkillOps::use passes it to Game::gainXp
int charXpForSkillUp(int newLevel);

// ---- perks (15.29.2). A perk is (skill, index < 32): saved as a bit per index in the skill's track.
struct PerkDef {
  const char* name;      // "CRUSHING BLOWS"
  const char* line;      // what it does, in plain words: "POWER ATTACKS STAGGER ARMOURED FOES"
  uint8_t need;          // the skill level it needs
  int8_t parent;         // the perk it hangs from (-1: a root); both parents must be taken when parent2 >= 0
  int8_t parent2;
  int8_t x, y;           // the star's place in the constellation (a -8..8 grid; the UI scales it to the screen)
  uint8_t cost = 1;      // perk points it takes
};
constexpr int MAX_PERKS = 32;
// the tree of a skill (n perks, n <= MAX_PERKS); never null (an empty tree: n == 0). Magic schools: magic::schoolPerks
const PerkDef* perkTable(Skill s, int& n);
// why a perk cannot be taken now ("" = it can): the level, the parents, the points
std::string perkBlocked(const struct Skills& k, Skill s, int idx);

// ---- foe tags (15.29.1: "these must work through foe tags (beast, undead, armoured, construct, people, flying...) with
//      the M6 foe roster and the M8b 'vs' affixes"). beast / undead / people agree with the M8b VsBeasts / VsUndead /
//      VsPeople classification (game.cpp damage()).
enum FoeTag : uint16_t {
  FT_BEAST = 1 << 0,       // wolves, bears, spiders, the wildlands' wildlife (M8b "beast")
  FT_UNDEAD = 1 << 1,      // skeletons, draugr, wraiths
  FT_SKELETAL = 1 << 2,    // bare bones (maces shatter them; arrows pass through)
  FT_ARMOURED = 1 << 3,    // plate, shells, scales, stone (soldiers, guards, draugr, mudcrabs, golems, the Armoured affix)
  FT_PEOPLE = 1 << 4,      // humans: bandits, soldiers, raiders (M8b "people")
  FT_CONSTRUCT = 1 << 5,   // golems: stone that walks
  FT_FLYING = 1 << 6,      // bats, harpies, wisps, the dragon
  FT_CHARGER = 1 << 7,     // what rushes you: wolves, boars, hounds, hyenas, riders, the Swift affix (spears stop them)
  FT_SHIELDED = 1 << 8,    // carries a shield or a ward (bows are weak against them)
  FT_UNARMOURED = 1 << 9,  // soft hide or cloth (axes bleed them)
  FT_SPIRIT = 1 << 10,     // wraiths, wisps: barely there
  FT_HUGE = 1 << 11,       // bears, trolls, yetis, golems, world bosses, the dragon
  FT_DRAGON = 1 << 12,
  FT_UNAWARE = 1 << 13,    // runtime only: has not noticed the player (daggers backstab)
};
constexpr int FOE_TAGS = 14;
const char* foeTagName(FoeTag t);       // "BEASTS", "UNDEAD", "SKELETAL", "ARMOURED", "PEOPLE", ... (the UI's words)
// the tags of a kind (pure: the roster's nature) and of an actor in play (its kind, role, rank, foes:: affixes, shield,
// whether it is aware of the player)
uint16_t tagsOf(art::Monster mon, bool human, Role role, uint8_t rank, uint16_t foeAffixes);
uint16_t tagsOf(const Actor& a);

// ---- weapon families (15.29.1)
Skill familyOf(const Item* weapon);     // Sword / Greatsword -> Sword, Axe, Mace, Spear, Dagger; a Bow -> Bow; none -> Unarmed
Skill familyOfType(WeaponType t);
struct FamilyTraits {
  float swingTime = 1;   // x the swing's windup and recovery (maces 1.25: slow; daggers 0.8: quick)
  float reach = 0;       // px added to the reach (spears +6, daggers -3 as today)
  float stamina = 1;     // x the swing's stamina cost
  int critPct = 0;       // added crit chance (daggers)
  const char* line;      // the family in a line: "BALANCED. PARRIES A BLOW WHEN TIMED"
};
const FamilyTraits& familyTraits(Skill fam);
// the damage percent this family gains (> 0) or loses (< 0) against a foe with these tags (summed over the matching
// rows, clamped to -50..+80). SkillOps applies it once the SKILLS lane switches it on (phase A: shown, not applied)
int familyVsPct(Skill fam, uint16_t tags);
struct VsRow { uint16_t tag; int8_t pct; const char* why; };   // one row: FT_ARMOURED, +35, "CRUSHES PLATE"
const std::vector<VsRow>& familyVs(Skill fam);
// the rows in plain words, for the UI: "+35% VS ARMOURED: CRUSHES PLATE", "-15% VS BEASTS"
std::vector<std::string> familyVsLines(Skill fam);

// ---- armour classes (15.29.1)
enum class Weight : uint8_t { None, Light, Medium, Heavy };
const char* weightName(Weight w);       // "", "LIGHT", "MEDIUM", "HEAVY"
Weight weightOf(const Item& it);        // a worn armour piece's class (by material and form; jewellery None)
Skill armourSkill(Weight w);            // Light -> LightArmour ... (None -> COUNT)
struct ArmourTrade {
  float speed = 1;        // x move speed
  float roll = 1;         // x roll distance
  float rollCost = 1;     // x the roll's stamina
  float stRegen = 1;      // x stamina regeneration
  float noise = 0;        // 0 silent .. 1 loud: how far foes hear the wearer (SkillOps::detectMul)
  float dr = 0;           // extra share of a blow stopped (0..0.3) on top of the armour rating
};
// the trade-off of wearing this class (the worn body armour's class sets it; skill level and perks soften the costs)
ArmourTrade armourTrade(Weight w, int skillLevel);

// ---- the state (saved: the skills block, SAVE_VER 20; its own version byte first)
struct Track {
  uint8_t level = START_LEVEL;
  float xp = 0;           // use points toward the next level
  uint32_t perks = 0;     // bit per PerkDef index taken
  bool has(int idx) const { return idx >= 0 && idx < MAX_PERKS && ((perks >> idx) & 1u); }
};
struct Skills {
  Track t[SKILLS];
  uint8_t matFam[(int)Mat::COUNT] = {};       // familiarity 0..100 per material worn / wielded (Heritage)
  std::map<uint64_t, uint8_t> cultFam;         // familiarity 0..100 per culture (cult::CultureId) whose make is worn
  uint16_t perkPts = 0;                        // unspent perk points (one per character level: SkillOps::leveledUp)
  uint16_t perksTaken = 0;
  uint8_t trainedLevel = 0;                    // the character level the trainer count below belongs to
  uint8_t trainedCount = 0;                    // levels bought from trainers at that character level (capped per level)
  std::vector<uint32_t> booksRead;             // skill books read (their seed: each one teaches once)
  uint16_t respecs = 0;                        // respecs done (each costs more)
  uint32_t uses[SKILLS] = {};                  // acts counted per skill (tests, the stats page)
  // runtime (never saved; the SKILLS lane adds what it needs): sneaking, distances gathered toward the next use
  struct Live {
    bool sneaking = false;
    float walked = 0, ridden = 0, sneakT = 0, wornT = 0;
    float lastX = 0, lastY = 0;
    bool hasLast = false;
    // (SKILLS lane) the perks' clocks and counters
    float riposteT = 0;        // Sword RIPOSTE: the next blow crits while it lasts
    float unseenT = 0;         // Dagger SHADOW STRIKE / Light MATCHLESS: foes lose the hero while it lasts
    float steadyCd = 0;        // Bow STEADY DRAW's cooldown
    float reflexCd = 0;        // Block QUICK REFLEXES' cooldown
    float rallyCd = 0;         // Medium RALLY's cooldown
    float chargeT = 0;         // Heavy CHARGE: the shoulder charge under way (the roll's time left)
    float combatT = 99;        // seconds since the hero last struck or was struck (Heritage, Athletics TRAVELLER)
    float heritT = 0;          // seconds worn in combat toward the next Heritage use / familiarity point
    int arrows = 0;            // arrows loosed (Bow TWIN SHOT: every third)
    int reflectN = 0;          // melee blows taken in heavy armour (Heavy REFLECT BLOWS: every tenth)
    uint32_t chargeHit[4] = {};   // actor ids the current charge or bash already struck
    bool charging = false;     // the charge is a heavy-armour shoulder charge (else a shield bash)
    bool smithInit = false;    // the forge's number took the Smithing start level (the session's first step)
    int heritTicks = 0;        // Heritage's 5 s ticks (a familiarity point every sixth)
    // the blow about to land (SkillOps::preStrike): its victim, unaware of the hero, mid-rush (a charger's lunge)
    int preId = -1;
    bool preUnaware = false, preRush = false;
    // persuasion: every merchant (npcKey) haggled with and the day: each merchant's shelf is lowered at most once a day
    std::vector<std::pair<uint64_t, int>> haggled;
    std::map<uint64_t, int> lowered;   // merchant -> the restock (day / 2) whose cached shelf a haggle already cut
    // Speech from trade: each merchant's (npcKey) restock (day / 2) and the use points already earned there; a
    // restock pays out at most SPEECH_TRADE_CAP, so buying and selling the same cheap goods cannot farm the skill
    std::map<uint64_t, std::pair<int, float>> traded;
    std::vector<std::string> words;   // the last words the perks said over the action (scripts: expect skillword)
    // use points earned per skill since the last reset (rpg_test --skills: the play-rate bot's measure)
    float points[(int)Skill::COUNT] = {};
  } live;
  int level(Skill s) const { return t[(int)s].level; }
  bool has(Skill s, int perkIdx) const { return t[(int)s].has(perkIdx); }
  Track& operator[](Skill s) { return t[(int)s]; }
  const Track& operator[](Skill s) const { return t[(int)s]; }
  int total() const { int n = 0; for (const Track& k : t) n += k.level; return n; }
  void serialize(std::vector<uint8_t>& out) const;
  bool deserialize(const std::vector<uint8_t>& in);   // false: a block this build cannot read (the save is refused)
};
// a new hero's levels: START_LEVEL everywhere, the background's own skills higher (a hunter's bow, a novice's
// restoration, a smith's child's smithing...)
void startLevels(Skills& k, Background bg);
// (15.29.5) a save made before M9 (SAVE_VER 19) loads with skills fitting its character level: what a hero of that level
// would plausibly have (the equipped weapon's family, the worn armour's class, the background's skills higher), the
// perk points of its levels unspent
void defaultsForLevel(Skills& k, int plLevel, Background bg, const Item* weapon, const Item* bodyArmour);

// ---- (SKILLS lane) the pace of each skill: x on every act's use points (rpg_test --skills' play-rate bot calibrates
//      it so a skill used at a normal play rate masters in about 10-20 h of play)
float skillRate(Skill s);
// the damage a weapon family's level adds: +0.3 % a level above START_LEVEL (+28.5 % at 100; modest: the perks matter)
float levelDamageMul(int level);
// the levels a trainer of this skill can teach up to (village 40, town 60, city 80, a capital 90)
int trainerCap(SiteType where, bool capital);
// gold for one trained level, from the skill's level now (Skyrim's rule, scaled to this economy)
int trainPrice(int level);
constexpr int TRAIN_PER_LEVEL = 5;   // levels a hero may buy from trainers (and companions) per character level

// ---- perk indices, named (the order of the trees in skills_perks.cpp; saved as bits: append only)
namespace perk {
enum Sword : int { SW_BLADESMAN, SW_PARRY, SW_KEEN, SW_RIPOSTE, SW_WHIRLWIND, SW_FLURRY, SW_MASTER };
enum Axe : int { AX_AXEMAN, AX_BLEED, AX_CLEAVE, AX_HEADSMAN, AX_DEEP, AX_HEW, AX_BLOODLUST };
enum Mace : int { MA_MACEMAN, MA_CRUSH, MA_BONEBREAKER, MA_SKULL, MA_HAMMERFALL, MA_BREAKER, MA_SHOCKWAVE };
enum Spear : int { SP_SPEARMAN, SP_SETCHARGE, SP_REACH, SP_IMPALE, SP_BAY, SP_GIANT, SP_PHALANX };
enum Dagger : int { DA_HAND, DA_BACKSTAB, DA_QUICK, DA_SILENT, DA_ASSASSIN, DA_LACERATE, DA_SHADOW };
enum Bow : int { BO_OVERDRAW, BO_EAGLE, BO_STEADY, BO_MARK, BO_QUICK, BO_POWER, BO_RANGER, BO_TWIN };
enum Unarmed : int { UN_FISTS, UN_BRAWLER, UN_HAYMAKER, UN_IRONSKIN, UN_FLURRY, UN_STUN };
enum Block : int { BL_WALL, BL_BASH, BL_DEFLECT, BL_STEADFAST, BL_REFLEX, BL_BULWARK };
enum Light : int { LI_AGILE, LI_WIND, LI_DEFT, LI_UNHINDERED, LI_FIT, LI_EVASION, LI_MATCHLESS };
enum Medium : int { ME_BALANCED, ME_STEADY, ME_WELLBAL, ME_WIND, ME_FIT, ME_RALLY, ME_VETERAN };
enum Heavy : int { HV_JUGGERNAUT, HV_FITTED, HV_CHARGE, HV_CONDITION, HV_FIT, HV_IMMOVABLE, HV_UNSTOPPABLE, HV_REFLECT };
enum Heritage : int { HE_KINSHIP, HE_METALS, HE_BLADEMAIL, HE_BRONZE, HE_OWN, HE_HOMELAND };
enum Athletics : int { AT_STRIDE, AT_SECONDWIND, AT_LIGHTFEET, AT_ENDURANCE, AT_TRAVELLER };
enum Smithing : int { SM_STEEL, SM_TEMPER, SM_THRIFTY, SM_STUDY, SM_MASTERWORK };
enum Cooking : int { CO_HEARTY, CO_SECOND, CO_FIELD };
enum Farming : int { FA_GREEN, FA_RAIN, FA_HUSBANDRY };
enum Riding : int { RI_HORSEMAN, RI_SURESEAT, RI_STABLE };
enum Speech : int { SC_HAGGLER, SC_INTIMIDATE, SC_PERSUADE, SC_TRAINERPET, SC_SILVER };
enum Stealth : int { ST_SOFTSTEP, ST_SHADOW, ST_SNEAKATTACK, ST_VANISH };
}  // namespace perk

// ---- skill books (15.29.4): ItemKind::Book, Item::sub == BOOK_SKILL, Item::form = the Skill, Item::seed = which book
//      (each teaches once: Skills::booksRead). Found as loot (M8b sources: SkillOps::loot), sold by scholars.
constexpr uint8_t BOOK_SKILL = 0;
Item makeSkillBook(Skill s, uint32_t seed, int ilvl);
bool isSkillBook(const Item& it);

}  // namespace skills
