// M6 Steel (VISION_PLAN 7.1 - 7.5, 15.11): item level, bands, level sync, rarity, affixes, legendaries and attunement,
// mitigation, the level-gap rules, XP, potions and merchant gold: the numbers every M6 lane reads. Shared contract,
// frozen after M6 phase A (lead, 2026-10-08): the NUMBERS lane is its only editor (it may ADD declarations; it never
// renames, removes or changes the meaning of what is listed here) and implements it in rpg/sim/gear.cpp.
//
// Phase A implements the closed-form rules exactly as VISION_PLAN 7.2 / 7.4 / 7.5 write them (bandOf, effLevel,
// rarityMult, mitigation, the gap multipliers, killXp, attunement) and stubs the generators (makeGear, rollDrop) on the
// classic item makers, so the game plays as in M5 until the NUMBERS lane wires and tunes them.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "rpg/sim/common.h"
#include "rpg/sim/items.h"

namespace cult { struct Culture; }

namespace gear {

// ---- danger and item level (7.1, 7.2)
constexpr int MAX_D = 60, LEVEL_CAP = 50, BANDS = 7;
int bandOf(int ilvl);                  // 1..7: 1-8, 9-16, 17-24, 25-32, 33-42, 43-52, 53-60 (ilvl <= 0: 1)
int bandLo(int band);                  // the band's first item level
int bandHi(int band);                  // its last
int effLevel(int ilvl, int playerLevel);   // level sync: min(ilvl, playerLevel + 4) (ilvl 0: playerLevel)
float statScale(int eff);              // 1 + 0.09 (eff - 1)

// ---- rarity (7.2)
float rarityMult(Rarity r);            // 1.00, 1.05, 1.10, 1.15, 1.20
int affixSlots(Rarity r);              // 0, 1, 2, 3, 2 (+ the unique power)
// the odds of the table (55 / 28 / 12 / 4 / 1 %); a legendary only from champions, named uniques, bosses and world
// bosses and never below D 10
struct DropSource {
  int D = 1;                           // the danger of the place (VISION 7.1; World::zoneLevel)
  uint8_t rank = 0;                    // foes::Rank of the source (0 an ordinary foe, a chest)
  bool chest = false;                  // a chest rather than a body
  uint64_t culture = 0;                // the culture whose arms the loot is made in (cult::CultureId; 0: the land's own)
  int playerLevel = 1;                 // for nothing but the XP-free "level sync" preview: drops NEVER scale with it
};
Rarity rollRarity(Rng& r, const DropSource& s);
int dropIlvl(const DropSource& s);     // D, bosses +2, named uniques +3 (clamped 1..60)

// ---- damage and armour (7.2, 7.4)
float weaponBase(WeaponType t);        // sword 8, war axe 9, mace 9, dagger 6, greatsword 14, spear 10
constexpr float BOW_BASE = 9, FOCUS_BASE = 12;
float armourBase(ItemKind slot);       // body 10, helmet 4, shield 5, gloves 2, boots 2, cloak 1
// AR / (AR + 10 attackerLevel + 30), capped at 70 %: the share of a blow the armour takes
float mitigation(float armourRating, int attackerLevel);
float playerDamageMul(int playerLevel);          // +1.5 % per level
constexpr float HP_PER_LEVEL = 4;
// against an enemy more than 4 levels up: the player's damage x max(0.4, 1 - 0.04 (Le - Lp - 4)), the enemy's
// x (1 + 0.03 (Le - Lp - 4)); 1 otherwise
float gapPlayerDamage(int playerLevel, int enemyLevel);
float gapEnemyDamage(int playerLevel, int enemyLevel);
// enemies at danger D (7.4): HP x (1 + 0.10 (D - 1)), damage x (1 + 0.09 (D - 1) + 0.09 min(D - 1, 4)) (the early bite, M6 finish), armour 1.5 D, XP x (1 + 0.12 (D - 1))
float enemyHpMul(int D);
float enemyDamageMul(int D);
float enemyArmour(int D);

// ---- experience (7.4)
// base x (1 + 0.12 (Le - 1)) x clamp(1 - 0.12 max(0, Lp - Le - 2), 0.05, 1)
int killXp(int base, int enemyLevel, int playerLevel);
int xpForNext(int level);              // a + b (L - 1) + c (L - 1)^2, tuned by --power-curve (phase B: a 120, b 100,
                                       // c 24: the 1.4 hours table; levels 1-2 keep the M5 first-hour pace)

// ---- anti-creep rules (7.3, 7.5)
int attunementSlots(int playerLevel);
// (NUMBERS) a piece that takes an attunement slot: an M6 legendary (item level set). The classic makers' rarity-4 rolls
// (test kits, pre-M6 code paths) do not
inline bool attunes(const Item& it) { return it.rarity == Rarity::Legendary && it.ilvl > 0; }  // legendaries worn at once: 0 below 10, 1 at 10, 2 at 25, 3 at 40
constexpr float POTION_COOLDOWN = 8.0f;   // seconds, shared by every potion
constexpr int POTION_CARRY = 10;          // potions carried at most (all kinds together)
int merchantGold(int urban);           // gold a merchant holds per restock: 0 village 300 .. 3 capital 3000
constexpr int SHOP_BAND_SPREAD = 2;    // shops stock their settlement's band +- 2 (by D, never by the player's level)
constexpr int DEATH_GOLD_PCT = 10;     // the death penalty: 10 % of carried gold

// ---- making items (7.2, 7.3, 15.11). The NUMBERS lane implements them for real; phase A stubs them on the classic
//      makers (makeWeapon, makeArmor...) and fills ilvl / mat / culture so every new field is exercised.
// A piece of gear: kind (Weapon, Bow, Staff, Armor, Helmet, Shield, Gloves, Boots, Cloak, Ring, Amulet), sub (the
// WeaponType for weapons), its item level, rarity, maker culture (0: heartland) and material (None: the band's usual
// metal; Alloy: `alloy` names the culture's alloy, 1-based).
Item makeGear(Rng& r, ItemKind kind, int sub, int ilvl, Rarity rarity, uint64_t culture, Mat mat = Mat::None, uint8_t alloy = 0);
// What a body or a chest drops: gear, potions, arrows, goods (the 7.2 rarity table, the 7.5 rules)
Item rollDrop(Rng& r, const DropSource& s);
// the name of a piece in its maker's words: "VETHMARKI STEEL SPEAR OF EMBERS" (c may be null: the heartland words)
std::string gearName(const Item& it, const cult::Culture* maker);
// the band's material word in a culture's language (7.2: "IRON, STEEL, GILDED..." in the heartland, "COPPER,
// BRONZE, SUNSTEEL..." in a Qasri land); c may be null
std::string bandWord(int band, const cult::Culture* c);

// =====================================================================================================================
// NUMBERS lane additions (M6 phase B). Everything below is added; nothing above changed meaning.
// =====================================================================================================================

// ---- making items with the maker culture in hand (the id-only makers above call these with c = nullptr). c: the maker
//      culture (its alloys, forms and words); nullptr = the heartland. The universal tiers come first (band 1 leather or
//      bronze, band 2 iron, band 3 steel); from band 4 up a culture's piece is in its own alloy (Mat::Alloy + alloy
//      index, the exotic tier on top: owner 15.11) and a heartland piece stays steel under the band's heartland word.
Item makeGearC(Rng& r, ItemKind kind, int sub, int ilvl, Rarity rarity, const cult::Culture* c, Mat mat = Mat::None, uint8_t alloy = 0);
Item rollDropC(Rng& r, const DropSource& s, const cult::Culture* c);
// the material a piece of this kind gets at this item level (when the caller does not force one); alloy: 1-based
Mat matFor(ItemKind kind, int ilvl, uint32_t seed, const cult::Culture* c, uint8_t& alloy);
// the band a material belongs to (crafted gear's item level is capped by it): leather / bronze 1, iron 2, steel 3,
// alloy 4 + its tierStep - 1 (max 7), cloth / wood 3, precious 7
int matBandCap(Mat m, const cult::Culture* c, uint8_t alloy);
// the piece's noun in its maker's arms grammar ("SPANGENHELM", "LAMELLAR", "KHOPESH", "GLAIVE", "RECURVE")
std::string pieceNoun(const Item& it);
// the material word an item shows ("BRONZE", "IRON", "VETHSTEEL", "GILDED"...)
std::string matWord(const Item& it, const cult::Culture* c);
// a legendary's name and lore line in its maker's phonology ("HROTHGAR'S OATH" / "FORGED FOR ... AT ...")
std::string legendaryName(const Item& it, const cult::Culture* c);
std::string legendaryLore(const Item& it, const cult::Culture* c);
// re-derive name, power and value of a gear item from its fields (ilvl, rarity, mat, culture, affixes, unique)
void finishGear(Item& it, const cult::Culture* c);

// ---- use-time numbers (level sync, 7.2: damage and armour read eff, so a too-good item works at playerLevel + 4)
// Item::power holds the number at the item's own level; usePower scales it to eff (ilvl 0: a classic item, as is)
float usePower(const Item& it, int playerLevel);
// an affix's value now (synced like power): value x range-mid(eff) / range-mid(ilvl)
int affixUse(const Item& it, Affix a, int playerLevel);
// the eff-scaled range of an affix (7.2): lo..hi inclusive (percent or points as affixInfo's unit)
void affixRange(Affix a, int eff, int& lo, int& hi);
// may this kind carry that affix (affixInfo's weapon / armour / jewel flags; bows and staves count as weapons)
bool affixAllowed(Affix a, ItemKind kind);
// the XP decay factor alone (killXp = base x (1 + 0.12 (Le - 1)) x xpDecay): Actor::xp already holds the first part
float xpDecay(int playerLevel, int enemyLevel);
// (M6 finish fixer, owner: "a slow rags-to-riches economy, but the first hour should feel like M5") the first two levels
// earn kill XP a little faster (x1.35 at level 1, x1.15 at level 2, x1 after): M6's ranked foes, enemy armour and the
// shirt-only start slowed the first level-up 35 % past M5 (the --metrics gate); the curve itself (xpForNext) is unchanged
float earlyXpMul(int playerLevel);
// the classic shop price of selling (2/5 of value, arrows by the third)
int sellPrice(const Item& it);
// the base HP of a level (HP_PER_LEVEL from level 2 on)
inline float levelHp(int playerLevel) { return HP_PER_LEVEL * (float)((playerLevel > 1 ? playerLevel : 1) - 1); }
// reputation at which a capital's merchants open their Epic shelf (7.5: "capitals offer Epic at reputation Honoured")
constexpr int REP_HONOURED = 40;
// player crits (base): 12 % for x1.8 (M5), affixes add to both
constexpr float CRIT_CHANCE = 0.12f, CRIT_MULT = 1.8f;
// unique powers that do something in play (the rest are listed in UNIQUE_TODO below and never roll)
bool uniqueImplemented(Unique u);
// the uniques a kind of item may roll (implemented ones only)
std::vector<Unique> uniquesFor(ItemKind kind);
// TODO (later milestones), listed so legendaries never roll them: FireTrail (a roll leaves fire: needs ground fire
// hazards), SpectralWolf (an allied summon: needs companion AI, M8), SplitArrows (projectile splitting by range),
// ReflectBlock (missile reflection on a roll), Shadowstep (foes losing track: needs an AI "unseen" state, FOES lane).

// ---- the worn gear summed (recalcPlayer fills Live::stats; affix values synced to the player's level)
struct GearStats {
  int aff[(int)Affix::COUNT] = {};
  uint64_t uniques = 0;          // bit per Unique worn
  int legendaries = 0;           // legendaries worn (attunement)
  bool has(Unique u) const { return (uniques >> (int)u) & 1ull; }
  int get(Affix a) const { return aff[(int)a]; }
};
GearStats sumGear(const std::vector<const Item*>& worn, int playerLevel);

// ---- the player's M6 runtime state. game.h is frozen for M6, so it lives in craft::Knowledge::live (rpg/sim/craft.h).
//      Timers restart on a load (cooldowns ready); (M6 fixer r4) the merchant purses and the per-restock trust caps
//      (purse, trustBuy, commissionAt) ARE saved in the craft block (v2), so a save and load cannot refill or reset them.
struct Live {
  GearStats stats;
  float potionCd = 0;            // POTION_COOLDOWN counting down
  float potionCdMax = POTION_COOLDOWN;
  int hitCount = 0;              // ChainLightning: every third hit
  float stillT = 0;              // StillCrit: seconds standing still
  float timeSlowCd = 0, frostNovaCd = 0, secondWindCd = 0;
  float warcryT = 0;             // Warcry: the buff left
  float ward = 0;                // Ward: the shield's hit points
  float bloomT = 0, bloomRate = 0;   // Lifebloom: heal over time
  // merchant purses (7.5 rule 8): npcKey -> (restock period day / 2, gold left)
  std::map<uint64_t, std::pair<int, int>> purse;
  // (M6 fixer r3) apprenticeship trust earned by buying a smith's own gear: npcKey -> (restock period, trust gained);
  // capped per restock (craft::TRUST_BUY_CAP) so buying and selling back cannot farm it
  std::map<uint64_t, std::pair<int, int>> trustBuy;
  // (M6 fixer r4) the restock period (day / 2 + 1) in which a smith's commission last earned trust: npcKey -> period.
  // Only the first commission of a restock builds trust, so gold alone cannot buy the apprenticeship in one visit.
  std::map<uint64_t, int> commissionAt;
};

}  // namespace gear
