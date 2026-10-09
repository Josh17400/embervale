// M6 Steel: the item and progression numbers (rpg/sim/gear.h). NUMBERS lane. Phase A (lead): the closed-form rules
// of VISION_PLAN 7.2 / 7.4 / 7.5 as written. Phase B (numbers): the generators for real (item level, the universal
// tiers then the maker culture's alloys, forms from its arms grammar, names in its words, rarity budget, eff-scaled
// affixes, legendaries with their unique powers, names and lore), the use-time numbers (level sync of power and
// affixes), the summed worn gear and the tuned XP curve.
#include "rpg/sim/gear.h"
#include <algorithm>
#include <cmath>
#include "rpg/culture/culture.h"
#include "rpg/sim/craft.h"

// (M6 fixer round 2, review: "gear rolls may differ by one at rounding edges on clang/arm64, where FMA contraction is on
// by default") item values and affix ranges must agree with the Windows build: no fused multiply-adds in this file
// (a source pragma; the build's flags are untouched). MSVC does not contract without /fp:contract.
#if defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

namespace gear {

int bandOf(int ilvl) {
  static const int hi[BANDS] = {8, 16, 24, 32, 42, 52, 60};
  for (int b = 0; b < BANDS; b++) if (ilvl <= hi[b]) return b + 1;
  return BANDS;
}
int bandLo(int band) {
  static const int lo[BANDS] = {1, 9, 17, 25, 33, 43, 53};
  return lo[std::clamp(band, 1, BANDS) - 1];
}
int bandHi(int band) {
  static const int hi[BANDS] = {8, 16, 24, 32, 42, 52, 60};
  return hi[std::clamp(band, 1, BANDS) - 1];
}
int effLevel(int ilvl, int playerLevel) { return ilvl <= 0 ? playerLevel : std::min(ilvl, playerLevel + 4); }
float statScale(int eff) { return 1.0f + 0.09f * (float)(std::max(1, eff) - 1); }

float rarityMult(Rarity r) {
  static const float m[] = {1.00f, 1.05f, 1.10f, 1.15f, 1.20f};
  return m[std::clamp((int)r, 0, 4)];
}
int affixSlots(Rarity r) {
  static const int n[] = {0, 1, 2, 3, 2};
  return n[std::clamp((int)r, 0, 4)];
}
Rarity rollRarity(Rng& r, const DropSource& s) {
  // 55 / 28 / 12 / 4 / 1 %; the legendary slot falls back to epic unless the source may give one (rank >= champion:
  // foes::Rank Champion 2, Named 3, Boss 4, WorldBoss 5) and D >= 10
  const float q = r.f();
  if (q < 0.01f) return (s.rank >= 2 && s.D >= 10) ? Rarity::Legendary : Rarity::Epic;
  if (q < 0.05f) return Rarity::Epic;
  if (q < 0.17f) return Rarity::Rare;
  if (q < 0.45f) return Rarity::Uncommon;
  return Rarity::Common;
}
int dropIlvl(const DropSource& s) {
  int l = s.D;
  if (s.rank == 3) l += 3;                    // a named unique
  else if (s.rank >= 4) l += 2;               // a boss, a world boss
  return std::clamp(l, 1, MAX_D);
}

float weaponBase(WeaponType t) {
  static const float b[] = {8, 9, 9, 6, 14, 10};
  static_assert(sizeof(b) / sizeof(b[0]) == (size_t)WeaponType::COUNT, "a base for every weapon type");
  return b[std::clamp((int)t, 0, (int)WeaponType::COUNT - 1)];
}
float armourBase(ItemKind slot) {
  switch (slot) {
    case ItemKind::Armor: return 10;
    case ItemKind::Helmet: return 4;
    case ItemKind::Shield: return 5;
    case ItemKind::Gloves: return 2;
    case ItemKind::Boots: return 2;
    case ItemKind::Cloak: return 1;
    default: return 0;
  }
}
float mitigation(float ar, int attackerLevel) {
  if (ar <= 0) return 0;
  const float m = ar / (ar + 10.0f * (float)std::max(1, attackerLevel) + 30.0f);
  return std::min(m, 0.70f);
}
float playerDamageMul(int L) { return 1.0f + 0.015f * (float)(std::max(1, L) - 1); }
float gapPlayerDamage(int Lp, int Le) {
  const int g = Le - Lp - 4;
  return g > 0 ? std::max(0.4f, 1.0f - 0.04f * (float)g) : 1.0f;
}
float gapEnemyDamage(int Lp, int Le) {
  const int g = Le - Lp - 4;
  return g > 0 ? 1.0f + 0.03f * (float)g : 1.0f;
}
float enemyHpMul(int D) { return 1.0f + 0.10f * (float)(std::max(1, D) - 1); }
// (M6 finish fixer) plus the early bite: +0.09 a level for the first four levels above 1 (then held), so a level-3 to
// level-5 pack of wolves stays the M5 threat (7.x alone let the levelled, armoured hero shrug it off: deaths 17 -> 5 %)
float enemyDamageMul(int D) {
  const int d = std::max(1, D) - 1;
  return 1.0f + 0.09f * (float)d + 0.09f * (float)std::min(d, 4);
}
float enemyArmour(int D) { return 1.5f * (float)std::max(1, D); }

float xpDecay(int Lp, int Le) { return std::clamp(1.0f - 0.12f * (float)std::max(0, Lp - Le - 2), 0.05f, 1.0f); }
float earlyXpMul(int Lp) { return Lp <= 1 ? 1.35f : (Lp == 2 ? 1.15f : 1.0f); }
int killXp(int base, int Le, int Lp) {
  const float up = 1.0f + 0.12f * (float)(std::max(1, Le) - 1);
  return std::max(base > 0 ? 1 : 0, (int)std::lround((float)base * up * xpDecay(Lp, Le) * earlyXpMul(Lp)));
}
// a 120, b 100, c 24: rpg_test --power-curve fits c to the 1.4 hours table (levels 5 / 10 / 20 / 30 / 40 / 50 at
// 0.5 / 2.5 / 12.3 / 30.4 / 56.9 / 92.1 h under its pacing model); levels 1 and 2 keep the M5 first-hour pace
int xpForNext(int L) { L = std::max(1, L); return 120 + (L - 1) * 100 + (L - 1) * (L - 1) * 24; }

int attunementSlots(int L) { return L >= 40 ? 3 : (L >= 25 ? 2 : (L >= 10 ? 1 : 0)); }
int merchantGold(int urban) {
  static const int g[] = {300, 800, 1600, 3000};
  return g[std::clamp(urban, 0, 3)];
}
int sellPrice(const Item& it) {
  if (it.kind == ItemKind::Arrows) return std::max(1, it.count / 3);
  return std::max(1, it.value * 2 / 5);
}

// ===================================================================================================== affixes
namespace {
struct ARange { int lo, hi; };
// the 7.2 pool's bounds (eff 1 .. 60 interpolates inside them)
const ARange& aRange(Affix a) {
  static const ARange t[] = {
      {0, 0},    {3, 12},  {3, 12},  {3, 12},  // none, fire, frost, shadow damage %
      {2, 4},                                  // life drain %
      {2, 8},    {10, 40},                     // crit chance %, crit damage %
      {5, 25},   {2, 5},   {5, 20},  {5, 25},  // stamina regen %, move speed %, roll distance %, thorns %
      {10, 30},  {10, 30}, {10, 30},           // fire, frost, shadow resistance %
      {3, 30},   {8, 60},                      // spell power, magicka (points)
      {5, 25},   {3, 15},  {8, 80},            // potion effect %, gold find % (at most 15), health (points)
  };
  static_assert(sizeof(t) / sizeof(t[0]) == (size_t)Affix::COUNT, "a range for every affix");
  return t[std::clamp((int)a, 0, (int)Affix::COUNT - 1)];
}
float rangeMid(Affix a, int eff) {
  int lo, hi;
  affixRange(a, eff, lo, hi);
  return 0.5f * (float)(lo + hi);
}
bool weaponish(ItemKind k) { return k == ItemKind::Weapon || k == ItemKind::Bow || k == ItemKind::Staff; }
bool jewel(ItemKind k) { return k == ItemKind::Ring || k == ItemKind::Amulet; }
// a suffix for the best affix of a rare-or-better piece ("SCALE OF EMBERS")
const char* affixSuffix(Affix a) {
  static const char* s[] = {"",        "EMBERS",     "RIME",       "SHADOWS",  "THE LEECH", "PRECISION", "RUIN",
                            "THE WIND", "HASTE",      "THE HARE",   "THORNS",   "THE SALAMANDER", "THE BEAR", "DAWN",
                            "SORCERY",  "THE MAGE",   "THE ALCHEMIST", "FORTUNE", "VITALITY"};
  static_assert(sizeof(s) / sizeof(s[0]) == (size_t)Affix::COUNT, "a suffix for every affix");
  return s[std::clamp((int)a, 0, (int)Affix::COUNT - 1)];
}
}  // namespace

void affixRange(Affix a, int eff, int& lo, int& hi) {
  const ARange& R = aRange(a);
  const float t = (float)(std::clamp(eff, 1, MAX_D) - 1) / (float)(MAX_D - 1);
  const float span = (float)(R.hi - R.lo);
  lo = R.lo + (int)std::floor(span * t * 0.7f);
  hi = R.lo + (int)std::ceil(span * (0.3f + 0.7f * t));
  hi = std::clamp(hi, lo, R.hi);
}
bool affixAllowed(Affix a, ItemKind k) {
  if (a == Affix::None || (int)a >= (int)Affix::COUNT) return false;
  const AffixInfo& I = affixInfo(a);
  if (weaponish(k)) return I.weapon;
  if (jewel(k)) return I.jewel;
  if (itemEquippable(k)) return I.armour;
  return false;
}
int affixUse(const Item& it, Affix a, int L) {
  const int v = it.affixValue(a);
  if (!v || it.ilvl == 0) return v;
  const int eff = effLevel(it.ilvl, L);
  if (eff >= it.ilvl) return v;
  return std::max(1, (int)std::lround((float)v * rangeMid(a, eff) / std::max(0.5f, rangeMid(a, it.ilvl))));
}
namespace { float powerBase(const Item& it); }
float usePower(const Item& it, int L) {
  if (it.ilvl == 0) return (float)it.power;
  const int eff = effLevel(it.ilvl, L);
  // the exact 7.2 number when the stored power is the formula's own (rounded into Item::power), so a whole point is
  // not lost to rounding; a power set by hand (an heirloom, a quest's piece) scales from what it holds
  float p = (float)it.power;
  if (itemEquippable(it.kind) && it.kind != ItemKind::Ring && it.kind != ItemKind::Amulet) {
    const float exact = powerBase(it) * statScale(it.ilvl) * rarityMult(it.rarity);
    if (std::lround(exact) == (long)it.power) p = exact;
  }
  return p * statScale(eff) / statScale(it.ilvl);
}

// ===================================================================================================== uniques
bool uniqueImplemented(Unique u) {
  switch (u) {
    case Unique::None: case Unique::FireTrail: case Unique::SpectralWolf: case Unique::SplitArrows: case Unique::ReflectBlock:
    case Unique::Shadowstep: case Unique::COUNT:
      return false;
    default: return true;
  }
}
std::vector<Unique> uniquesFor(ItemKind k) {
  using U = Unique;
  std::vector<U> v;
  switch (k) {
    case ItemKind::Weapon:
      v = {U::ChainLightning, U::StillCrit, U::Executioner, U::Thunderclap, U::Berserker, U::Huntsman, U::Gravebane, U::Dragonbane,
           U::Ember, U::Rime, U::Echo, U::Stormcaller, U::Warcry, U::Vampire};
      break;
    case ItemKind::Bow: v = {U::Quickdraw, U::Huntsman, U::Ember, U::Rime, U::Echo, U::Stormcaller, U::StillCrit, U::Executioner}; break;
    case ItemKind::Staff: v = {U::Arcanist, U::BloodMagic, U::Ember, U::Rime}; break;
    case ItemKind::Ring: case ItemKind::Amulet:
      v = {U::GoldenTouch, U::Vampire, U::Lifebloom, U::Arcanist, U::Wayfarer, U::TimeSlow, U::SecondWind, U::Ward, U::Warcry};
      break;
    case ItemKind::Boots: case ItemKind::Cloak:
      v = {U::Windwalker, U::Wayfarer, U::SecondWind, U::TimeSlow, U::Ward, U::Lifebloom};
      break;
    case ItemKind::Armor: case ItemKind::Helmet: case ItemKind::Shield: case ItemKind::Gloves:
      v = {U::Bulwark, U::FrostNova, U::Ward, U::SecondWind, U::Thornmail, U::Ironhide, U::TimeSlow, U::Lifebloom};
      break;
    default: break;
  }
  v.erase(std::remove_if(v.begin(), v.end(), [](U u) { return !uniqueImplemented(u); }), v.end());
  return v;
}

GearStats sumGear(const std::vector<const Item*>& worn, int L) {
  GearStats s;
  for (const Item* it : worn) {
    if (!it) continue;
    for (const ItemAffix& a : it->affix)
      if (a.kind != Affix::None && (int)a.kind < (int)Affix::COUNT) s.aff[(int)a.kind] += affixUse(*it, a.kind, L);
    if (it->unique != Unique::None && (int)it->unique < (int)Unique::COUNT) s.uniques |= 1ull << (int)it->unique;
    if (attunes(*it)) s.legendaries++;
  }
  s.aff[(int)Affix::GoldFind] = std::min(s.aff[(int)Affix::GoldFind], 15 * 3);   // 15 % a piece (7.2), three jewels at most
  return s;
}

// ===================================================================================================== materials
namespace {
// heartland band words above the universal tiers (bands 4 .. 7)
const char* kHeartHigh[] = {"GILDED", "OBSIDIAN", "EMBERFORGED", "STARMETAL"};
uint32_t matTint(Mat m) {
  switch (m) {
    case Mat::Leather: return rgba(150, 100, 62);
    case Mat::Bronze: return rgba(196, 146, 70);
    case Mat::Iron: return rgba(150, 152, 160);
    case Mat::Steel: return rgba(200, 208, 222);
    case Mat::Bone: return rgba(226, 216, 190);
    case Mat::Wood: return rgba(150, 110, 70);
    case Mat::Precious: return rgba(230, 200, 90);
    default: return rgba(170, 170, 176);
  }
}
const cult::Alloy* alloyOf(const cult::Culture* c, uint8_t alloy) {
  if (!c || alloy < 1 || alloy > c->arms.alloys.size()) return nullptr;
  return &c->arms.alloys[(size_t)alloy - 1];
}
bool armourish(ItemKind k) { return k == ItemKind::Armor || k == ItemKind::Helmet || k == ItemKind::Gloves || k == ItemKind::Boots || k == ItemKind::Shield; }
}  // namespace

std::string bandWord(int band, const cult::Culture* c) {
  band = std::clamp(band, 1, BANDS);
  static const char* low[] = {"BRONZE", "IRON", "STEEL"};
  if (band <= 3) return low[band - 1];
  // the culture's alloys by the band they reach (3 + tierStep), best that fits
  if (c && !c->arms.alloys.empty()) {
    const cult::Alloy* best = &c->arms.alloys[0];
    for (const cult::Alloy& A : c->arms.alloys) if (3 + (int)A.tierStep <= band) best = &A;
    return best->name;
  }
  return kHeartHigh[band - 4];
}

Mat matFor(ItemKind kind, int ilvl, uint32_t seed, const cult::Culture* c, uint8_t& alloy) {
  alloy = 0;
  if (kind == ItemKind::Cloak) return Mat::Cloth;
  if (kind == ItemKind::Bow || kind == ItemKind::Staff) return Mat::Wood;
  if (jewel(kind)) return Mat::Precious;
  const int band = bandOf(ilvl);
  if (band == 1) {
    if (armourish(kind)) {
      const uint32_t h = hash32(seed ^ 0x1EA7u) % 100u;
      return h < (ilvl <= 4 ? 60u : 35u) ? Mat::Leather : Mat::Bronze;
    }
    return Mat::Bronze;
  }
  if (band == 2) return Mat::Iron;
  if (band == 3 || !c || c->arms.alloys.empty()) return Mat::Steel;
  // the exotic tier: the best of the maker's alloys this band reaches (best last; the first when none fits)
  alloy = 1;
  for (size_t i = 0; i < c->arms.alloys.size(); i++)
    if (3 + (int)c->arms.alloys[i].tierStep <= band) alloy = (uint8_t)(i + 1);
  return Mat::Alloy;
}

int matBandCap(Mat m, const cult::Culture* c, uint8_t alloy) {
  switch (m) {
    case Mat::Leather: case Mat::Bronze: return 1;
    case Mat::Iron: return 2;
    case Mat::Steel: case Mat::Cloth: case Mat::Wood: case Mat::Bone: return 3;
    case Mat::Alloy: { const cult::Alloy* A = alloyOf(c, alloy); return std::min(BANDS, 3 + (A ? std::max(1, (int)A->tierStep) : 1)); }
    default: return BANDS;
  }
}

// ===================================================================================================== names
std::string matWord(const Item& it, const cult::Culture* c) {
  switch (it.mat) {
    case Mat::Leather: return "LEATHER";
    case Mat::Bronze: return "BRONZE";
    case Mat::Iron: return "IRON";
    case Mat::Steel: return bandOf(it.ilvl) >= 4 ? bandWord(bandOf(it.ilvl), nullptr) : std::string("STEEL");
    case Mat::Alloy: { const cult::Alloy* A = alloyOf(c, it.alloy); return A ? A->name : bandWord(bandOf(it.ilvl), nullptr); }
    case Mat::Bone: return "BONE";
    default: return "";
  }
}

std::string pieceNoun(const Item& it) {
  const int f = (int)it.form - 1;   // the arms enum (-1: the classic look)
  switch (it.kind) {
    case ItemKind::Weapon: {
      const WeaponType wt = (WeaponType)std::min<int>(it.sub, (int)WeaponType::COUNT - 1);
      if (wt == WeaponType::Spear) {
        static const char* p[] = {"SPEAR", "GLAIVE", "HALBERD", "SPEAR"};
        return f >= 0 && f < 4 ? p[f] : "SPEAR";
      }
      if (wt == WeaponType::Sword) {
        static const char* b[] = {"SWORD", "LEAFBLADE", "FALCHION", "SCIMITAR", "KHOPESH", "KRIS", "BROADSWORD", "SABRE", "WARBLADE"};
        return f >= 0 && f < 9 ? b[f] : "SWORD";
      }
      if (wt == WeaponType::Greatsword) {
        static const char* b[] = {"GREATSWORD", "GREATBLADE", "GREAT FALCHION", "GREAT SCIMITAR", "GREAT KHOPESH", "FLAMBERGE", "CLAYMORE", "NODACHI", "ZWEIHANDER"};
        return f >= 0 && f < 9 ? b[f] : "GREATSWORD";
      }
      static const char* n[] = {"SWORD", "WAR AXE", "MACE", "DAGGER", "GREATSWORD", "SPEAR"};
      return n[(int)wt];
    }
    case ItemKind::Bow: {
      static const char* b[] = {"BOW", "RECURVE", "COMPOSITE BOW", "CROSSBOW", "LONGBOW"};
      return f >= 0 && f < 5 ? b[f] : "BOW";
    }
    case ItemKind::Staff: return "STAFF";
    case ItemKind::Helmet: {
      static const char* h[] = {"HELM", "KETTLE HAT", "GREAT HELM", "SPANGENHELM", "HORNED HELM", "PLUMED HELM", "AVENTAIL", "MASKED HELM", "CRESTED HELM", "WINGED HELM"};
      if (it.mat == Mat::Leather) return "CAP";
      return f >= 0 && f < 10 ? h[f] : "HELMET";
    }
    case ItemKind::Armor: {
      static const char* b[] = {"GAMBESON", "JERKIN", "MAIL", "SCALE", "LAMELLAR", "BRIGANDINE", "PLATE", "LEAF MAIL"};
      if (f >= 0 && f < 8) return b[f];
      return it.mat == Mat::Leather ? "ARMOR" : "PLATE";
    }
    case ItemKind::Shield: {
      static const char* s[] = {"ROUND SHIELD", "KITE SHIELD", "HEATER", "TOWER SHIELD", "CRESCENT SHIELD", "OVAL SHIELD", "BUCKLER", "LEAF SHIELD", "SHIELD"};
      return f >= 0 && f < 9 ? s[f] : "SHIELD";
    }
    case ItemKind::Gloves: return it.mat == Mat::Leather ? "GLOVES" : "GAUNTLETS";
    case ItemKind::Boots: return "BOOTS";
    case ItemKind::Cloak: return "CLOAK";
    case ItemKind::Ring: return "RING";
    case ItemKind::Amulet: return "AMULET";
    default: return "";
  }
}

namespace {
const cult::Culture& heartland() {
  static const cult::Culture H = cult::Atlas::make(cult::Archetype::Heartland, 0x4EA27u);
  return H;
}
}  // namespace

std::string legendaryName(const Item& it, const cult::Culture* c) {
  const cult::Culture& C = c ? *c : heartland();
  const uint32_t s = it.seed ? it.seed : 1u;
  // (M6 fixer) a well-mixed draw per part (hash32 of seeds a bit apart drew the same words: two legendaries of one kit
  // came out "THE STARLIT WIDOW"), and nouns that fit the piece (no "SALT CROWN" shield)
  auto pick = [&](uint32_t k, uint32_t n) {
    uint64_t x = (uint64_t)s * 0x9E3779B97F4A7C15ull ^ ((uint64_t)k + (uint64_t)it.kind * 0x1F3u) * 0xC2B2AE3D27D4EB4Full;
    x ^= x >> 31; x *= 0xBF58476D1CE4E5B9ull; x ^= x >> 29; x *= 0x94D049BB133111EBull; x ^= x >> 32;
    return (uint32_t)(x % n);
  };
  static const char* own[] = {"OATH", "WRATH", "VOW", "BANE", "PROMISE", "LAMENT", "FURY", "GRACE", "VIGIL", "REVENGE", "MERCY", "HUNGER"};
  static const char* adj[] = {"SALT", "ASHEN", "WINTER", "HOLLOW", "IRON", "PALE", "LAST", "RED", "SILENT", "SUNKEN", "BROKEN", "STARLIT"};
  static const char* nWeapon[] = {"WIDOWMAKER", "FANG", "THORN", "TALON", "REAPER", "HOUND", "WOLF", "EDGE", "STING", "HERALD", "TOOTH", "MARTYR"};
  static const char* nBow[] = {"STING", "SONG", "WHISPER", "HAWK", "RAIN", "WIND", "STAR", "THORN", "REACH", "HERALD", "WIDOW", "LARK"};
  static const char* nShield[] = {"WALL", "WARD", "BULWARK", "GATE", "WARDEN", "OATHKEEPER", "TIDE", "TOWER", "DOOR", "SHELTER", "BASTION", "VIGIL"};
  static const char* nHelm[] = {"CROWN", "HELM", "BROW", "KING", "WARDEN", "MASK", "VISAGE", "CIRCLET", "HERALD", "STAR", "WATCHER", "MARTYR"};
  static const char* nBody[] = {"MANTLE", "HAUBERK", "SKIN", "WARDEN", "MARTYR", "OATHKEEPER", "SHELL", "HIDE", "KING", "TIDE", "WALL", "HEART"};
  static const char* nHands[] = {"GRASP", "HANDS", "GRIP", "FIST", "TOUCH", "CLAWS", "HOLD", "REACH", "KNUCKLES", "PALMS", "TALONS", "GIFT"};
  static const char* nFeet[] = {"STRIDE", "STEPS", "ROAD", "MARCH", "TREAD", "PATH", "WANDERER", "HOUND", "WOLF", "TIDE", "PILGRIM", "WAY"};
  static const char* nCloak[] = {"SHROUD", "MANTLE", "VEIL", "WINGS", "SHADOW", "TIDE", "NIGHT", "WIDOW", "HERALD", "PALL", "STORM", "MIST"};
  static const char* nTrinket[] = {"STAR", "EYE", "HEART", "TEAR", "SEAL", "OATH", "WIDOW", "KING", "MOON", "EMBER", "HERALD", "SECRET"};
  const char* const* noun = nTrinket;
  switch (it.kind) {
    case ItemKind::Weapon: case ItemKind::Staff: noun = nWeapon; break;
    case ItemKind::Bow: noun = nBow; break;
    case ItemKind::Shield: noun = nShield; break;
    case ItemKind::Helmet: noun = nHelm; break;
    case ItemKind::Armor: noun = nBody; break;
    case ItemKind::Gloves: noun = nHands; break;
    case ItemKind::Boots: noun = nFeet; break;
    case ItemKind::Cloak: noun = nCloak; break;
    default: break;
  }
  std::string nm;
  if (pick(0x7E9Du, 3u) != 0) {
    std::string who = cult::personName(C, s, (s >> 7) & 1u);
    const size_t sp = who.find(' ');
    if (sp != std::string::npos) who = who.substr(0, sp);   // the given name only
    if (who.size() > 10) who = who.substr(0, 10);
    nm = who + "'S " + own[pick(0x51u, 12u)];
  } else {
    nm = std::string("THE ") + adj[pick(0x52u, 12u)] + " " + noun[pick(0x53u, 12u)];
  }
  return nm;
}
std::string legendaryLore(const Item& it, const cult::Culture* c) {
  const cult::Culture& C = c ? *c : heartland();
  const uint32_t s = it.seed ? it.seed : 1u;
  std::string place = cult::placeName(C, hash32(s ^ 0xA11u));
  std::string who = cult::personName(C, hash32(s ^ 0xB22u), (s >> 9) & 1u);
  const size_t sp = who.find(' ');
  if (sp != std::string::npos) who = who.substr(0, sp);
  switch (hash32(s ^ 0xC33u) % 5u) {
    case 0: return "FORGED IN " + place + " FOR " + who + ", WHO NEVER CAME HOME.";
    case 1: return "IT WAS LOST AT " + place + " IN THE WINTER WAR.";
    case 2: return who + " CARRIED IT OUT OF THE FIRE AT " + place + ".";
    case 3: return "THE SMITHS OF " + place + " STILL SING OF IT.";
    default: return "BURIED WITH " + who + " UNDER THE STONES OF " + place + ".";
  }
}

// ===================================================================================================== making
namespace {
uint8_t pickForm(ItemKind kind, int sub, Mat mat, uint32_t seed, const cult::Culture* c) {
  if (!c || !c->id) return 0;
  const cult::ArmsStyle& A = c->arms;
  switch (kind) {
    case ItemKind::Weapon:
      if (sub == (int)WeaponType::Spear) return (uint8_t)((A.polearm == cult::Polearm::None ? 0 : (int)A.polearm) + 1);
      if (sub == (int)WeaponType::Sword || sub == (int)WeaponType::Greatsword || sub == (int)WeaponType::Dagger) return (uint8_t)((int)A.blade + 1);
      return 0;
    case ItemKind::Bow: return (uint8_t)((int)A.bow + 1);
    // (M6 fixer r4, review: "the steel kit looks the same across several cultures") a metal helm is the culture's
    // signature form (helm[0]); its everyday second form (often a plain kettle or nasal cap) is the leather one's
    case ItemKind::Helmet: return (uint8_t)((int)A.helm[mat == Mat::Leather ? 1 : 0] + 1);
    case ItemKind::Shield: return (uint8_t)((A.shield == cult::ShieldForm::None ? (int)cult::ShieldForm::Round : (int)A.shield) + 1);
    case ItemKind::Armor: {
      auto soft = [](cult::BodyArm b) { return b == cult::BodyArm::Leather || b == cult::BodyArm::Padded; };
      cult::BodyArm b = A.body[seed & 1u];
      if (mat == Mat::Leather) {
        if (!soft(b)) b = soft(A.body[(seed & 1u) ^ 1u]) ? A.body[(seed & 1u) ^ 1u] : cult::BodyArm::Leather;
      } else if (soft(b)) {
        b = !soft(A.body[(seed & 1u) ^ 1u]) ? A.body[(seed & 1u) ^ 1u] : cult::BodyArm::Mail;
      }
      return (uint8_t)((int)b + 1);
    }
    default: return 0;
  }
}
float powerBase(const Item& it) {
  switch (it.kind) {
    case ItemKind::Weapon: return weaponBase((WeaponType)std::min<int>(it.sub, (int)WeaponType::COUNT - 1));
    case ItemKind::Bow: return BOW_BASE;
    case ItemKind::Staff: return FOCUS_BASE;
    default: return armourBase(it.kind) * (it.mat == Mat::Leather ? 0.92f : 1.0f);
  }
}
int valueBase(ItemKind k) {
  switch (k) {
    case ItemKind::Weapon: return 48;
    case ItemKind::Bow: return 56;
    case ItemKind::Staff: return 240;
    case ItemKind::Armor: return 80;
    case ItemKind::Helmet: return 32;
    case ItemKind::Shield: return 40;
    case ItemKind::Gloves: case ItemKind::Boots: return 18;
    case ItemKind::Cloak: return 14;
    case ItemKind::Ring: case ItemKind::Amulet: return 110;
    default: return 10;
  }
}
art::Icon iconOf(ItemKind k, int sub) {
  using art::Icon;
  switch (k) {
    case ItemKind::Weapon: {
      static const Icon w[] = {Icon::Sword, Icon::Axe, Icon::Mace, Icon::Dagger, Icon::Greatsword, Icon::Spear};
      return w[std::clamp(sub, 0, (int)WeaponType::COUNT - 1)];
    }
    case ItemKind::Bow: return Icon::Bow;
    case ItemKind::Staff: return Icon::Staff;
    case ItemKind::Armor: return Icon::Armor;
    case ItemKind::Helmet: return Icon::Helmet;
    case ItemKind::Shield: return Icon::Shield;
    case ItemKind::Gloves: return Icon::Gloves;
    case ItemKind::Boots: return Icon::Boots;
    case ItemKind::Cloak: return Icon::Cloak;
    case ItemKind::Ring: return Icon::Ring;
    case ItemKind::Amulet: return Icon::Amulet;
    default: return Icon::Bone;
  }
}
void rollAffixes(Rng& r, Item& it, int n) {
  for (ItemAffix& a : it.affix) a = ItemAffix();
  std::vector<Affix> pool;
  for (int a = 1; a < (int)Affix::COUNT; a++) if (affixAllowed((Affix)a, it.kind)) pool.push_back((Affix)a);
  n = std::min(n, std::min((int)pool.size(), kItemAffixes));
  for (int k = 0; k < n; k++) {
    const int pick = r.irange((int)pool.size());
    const Affix a = pool[(size_t)pick];
    pool.erase(pool.begin() + pick);
    int lo, hi;
    affixRange(a, std::max<int>(1, it.ilvl), lo, hi);
    it.affix[k].kind = a;
    it.affix[k].value = (int16_t)(lo + r.irange(hi - lo + 1));
  }
}
}  // namespace

void finishGear(Item& it, const cult::Culture* c) {
  const int ilvl = std::clamp<int>(it.ilvl, 1, MAX_D);
  it.tier = (uint8_t)(bandOf(ilvl) - 1);
  it.icon = iconOf(it.kind, it.sub);
  // power at the item's own level (usePower syncs it at use)
  const float base = powerBase(it);
  it.power = (int16_t)std::lround(base * statScale(ilvl) * rarityMult(it.rarity));
  if (jewel(it.kind)) it.power = 0;
  // colour: the alloy's key, a universal metal's, a cloak's cloth in the maker's colours
  if (it.mat == Mat::Alloy) {
    const cult::Alloy* A = alloyOf(c, it.alloy);
    it.tint = A ? (A->color | 0xFF000000u) : tierTint(it.tier);
  } else if (it.mat == Mat::Cloth) {
    static const uint32_t cloakC[] = {rgba(112, 82, 58), rgba(60, 96, 62), rgba(52, 70, 120), rgba(40, 92, 66), rgba(46, 38, 58), rgba(150, 40, 36), rgba(70, 60, 110)};
    it.tint = c && c->arms.cloth ? (c->arms.cloth | 0xFF000000u) : cloakC[it.tier];
  } else if (it.mat == Mat::Precious) {
    it.tint = it.kind == ItemKind::Ring ? rgba(230, 200, 90) : rgba(190, 200, 220);
  } else it.tint = matTint(it.mat);
  // the name in the maker's words
  if (it.rarity == Rarity::Legendary) it.name = legendaryName(it, c);
  else {
    std::string w = matWord(it, c);
    if (it.kind == ItemKind::Cloak) {
      static const char* cn[] = {"TRAVEL CLOAK", "WOOL CLOAK", "FUR MANTLE", "RANGER CLOAK", "SHADOW CLOAK", "EMBER MANTLE", "STAR MANTLE"};
      it.name = cn[it.tier];
    } else if (it.kind == ItemKind::Bow) {
      static const char* bw[] = {"HUNTING", "YEW", "ASH", "HORN", "BLACKWOOD", "EMBERWOOD", "STARWOOD"};
      it.name = std::string(bw[it.tier]) + " " + pieceNoun(it);
    } else if (it.kind == ItemKind::Staff) {
      static const char* sw[] = {"OAK", "YEW", "ASH", "RUNED", "BLACKWOOD", "EMBERWOOD", "STARWOOD"};
      it.name = std::string(sw[it.tier]) + " STAFF";
    } else if (jewel(it.kind)) {
      static const char* jw[] = {"COPPER", "SILVER", "GOLD", "MOONSTONE", "SUNSTONE", "DRAGONBONE", "STARGLASS"};
      it.name = std::string(jw[it.tier]) + " " + pieceNoun(it);
    } else it.name = (w.empty() ? std::string() : w + " ") + pieceNoun(it);
    // a rare-or-better piece is named for its strongest affix
    if (it.rarity >= Rarity::Rare && it.affix[0].kind != Affix::None) {
      const ItemAffix* best = &it.affix[0];
      for (const ItemAffix& a : it.affix)
        if (a.kind != Affix::None) {
          const ARange& R = aRange(a.kind), &B = aRange(best->kind);
          if ((float)(a.value - R.lo) / std::max(1, R.hi - R.lo) > (float)(best->value - B.lo) / std::max(1, B.hi - B.lo)) best = &a;
        }
      it.name += std::string(" OF ") + affixSuffix(best->kind);
    }
  }
  // value: the slot's worth, the level, the rarity, the affixes
  const float lv = std::pow(statScale(ilvl), 1.5f);
  float v = (float)valueBase(it.kind) * lv * (1.0f + 0.5f * (float)it.rarity) + 12.0f * (float)it.affixCount() * statScale(ilvl);
  if (it.rarity == Rarity::Legendary) v *= 2.0f;
  if (it.mat == Mat::Alloy) v *= 1.25f;
  it.value = std::max(1, (int)std::lround(v));
}

Item makeGearC(Rng& r, ItemKind kind, int sub, int ilvl, Rarity rarity, const cult::Culture* c, Mat mat, uint8_t alloy) {
  Item it;
  it.kind = kind;
  it.sub = kind == ItemKind::Weapon ? (uint8_t)std::clamp(sub, 0, (int)WeaponType::COUNT - 1) : 0;
  it.ilvl = (uint8_t)std::clamp(ilvl, 1, MAX_D);
  it.seed = r.next();
  it.rarity = rarity;
  if (jewel(kind) && it.rarity < Rarity::Uncommon) it.rarity = Rarity::Uncommon;   // a ring is worth its stone
  it.culture = c ? c->id : 0;
  uint8_t al = alloy;
  if (mat == Mat::None) it.mat = matFor(kind, it.ilvl, it.seed, c, al);
  else it.mat = mat;
  if (it.mat == Mat::Alloy) {
    if (!al) al = 1;
    if (c && al > c->arms.alloys.size()) al = (uint8_t)std::max<size_t>(1, c->arms.alloys.size());
    it.alloy = al;
  }
  it.form = pickForm(kind, it.sub, it.mat, it.seed, c);
  rollAffixes(r, it, affixSlots(it.rarity));
  if (it.rarity == Rarity::Legendary) {
    const std::vector<Unique> u = uniquesFor(kind);
    if (!u.empty()) it.unique = u[(size_t)r.irange((int)u.size())];
  }
  finishGear(it, c);
  return it;
}

Item makeGear(Rng& r, ItemKind kind, int sub, int ilvl, Rarity rarity, uint64_t culture, Mat mat, uint8_t alloy) {
  Item it = makeGearC(r, kind, sub, ilvl, rarity, nullptr, mat, alloy);
  if (culture) it.culture = culture;   // the id alone (no culture data in hand: heartland words and forms)
  return it;
}

Item rollDropC(Rng& r, const DropSource& s, const cult::Culture* c) {
  const bool big = s.rank >= 4;   // bosses and world bosses always drop gear
  const float q = big ? 0.99f : r.f();
  // what kind of thing: the classic loot table's shares (randomLoot), gear made with the 7.2 rules
  if (q < 0.30f) { const int D = std::max(1, s.D); return makePotion((PotionType)r.irange(3), D > 12 ? 1 + (r.f() < 0.3f) : (D > 5 ? (r.f() < 0.5f) : 0)); }
  if (q < 0.42f) return makeArrows(5 + r.irange(10));
  if (q < 0.56f) {
    // (M6 fixer) ore is crafting stuff now: the smelter's IRON ORE (craft::Stuff::IronOre), never the old misc good of
    // the same name that no recipe takes (same draw, so the random stream is unchanged)
    const int w = r.irange(8);
    return w == 3 ? craft::makeStuff(craft::Stuff::IronOre, 1) : makeMisc(w);
  }
  if (q < 0.64f) return makeFood(r.irange(4));
  ItemKind kind;
  int sub = 0;
  const float g = big ? r.f() : (q - 0.64f) / 0.36f;
  if (g < 0.39f) {
    kind = ItemKind::Weapon;
    const bool spear = c && c->arms.polearm != cult::Polearm::None;
    sub = r.irange(spear ? 6 : 5);
  } else if (g < 0.72f) kind = randomArmorSlot(r);
  else if (g < 0.86f) kind = ItemKind::Bow;
  else if (g < 0.95f) kind = r.f() < 0.5f ? ItemKind::Ring : ItemKind::Amulet;
  else kind = ItemKind::Staff;
  Rarity rar = rollRarity(r, s);
  // +1 rarity roll: elites (foes::Rank::Elite) and bosses; named uniques Rare+ and 15 % legendary; a world boss a legendary
  if (s.rank == 1 || s.rank == 4) { const Rarity b = rollRarity(r, s); if ((int)b > (int)rar) rar = b; }
  if (s.rank == 3) {
    if ((int)rar < (int)Rarity::Rare) rar = Rarity::Rare;
    if (s.D >= 10 && r.f() < 0.15f) rar = Rarity::Legendary;
  }
  if (s.rank >= 5) rar = s.D >= 10 ? Rarity::Legendary : Rarity::Epic;
  if (rar == Rarity::Legendary && (s.rank < 2 || s.D < 10)) rar = Rarity::Epic;   // 7.2: never below D 10, never a plain foe
  return makeGearC(r, kind, sub, dropIlvl(s), rar, c);
}

Item rollDrop(Rng& r, const DropSource& s) { return rollDropC(r, s, nullptr); }

std::string gearName(const Item& it, const cult::Culture* maker) {
  if (!itemEquippable(it.kind) || it.ilvl == 0) return it.name;
  Item t = it;
  finishGear(t, maker);
  return t.name;
}

}  // namespace gear
