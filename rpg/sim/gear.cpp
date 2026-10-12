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
// (M8b NUMBERS) the item level scale by knots (linear between them). M6 had 1 + 0.09 (eff - 1) all the way: with the
// foes' HP at +10 % a level, same-band trash fell from 4.4 hits at level 1 to 3.0 at level 50, which left no room for
// rarity (a Mythic would kill trash in under 2 hits). The curve keeps the first ten levels (the M5 / metrics feel: a
// level-10 sword is still ~1.7x a level-1 one) and then flattens, so a hero in common gear of his band takes 4.4 - 4.9
// hits on same-band trash at every level and the six-step ladder and its affixes carry the late game (--power-curve,
// whose model reads these real items). The knots fold in the band materials (bronze, iron, steel, the band words above
// it: matMult) and the +1.5 % a level of playerDamageMul.
float statScale(int eff) {
  static const int kL[] = {1, 5, 10, 20, 30, 40, 50, 60};
  static const float kS[] = {1.000f, 1.358f, 1.704f, 2.040f, 2.334f, 2.558f, 2.700f, 2.818f};
  const int e = std::clamp(eff, 1, MAX_D);
  for (int i = 1; i < 8; i++)
    if (e <= kL[i]) return kS[i - 1] + (kS[i] - kS[i - 1]) * (float)(e - kL[i - 1]) / (float)(kL[i] - kL[i - 1]);
  return kS[7];
}

// (M8b NUMBERS, 15.27.3) the rarity's power. Tuned with powerScore so the owner's feel holds at player level 10: a level-2
// Rare bronze sword ~ a level-10 Common iron one (+-15 %), an Uncommon 0-25 % below it, an Epic above it; each step at
// least +10 % over the one below at the same item level (power and affixes); and the best rarity the gate allows still
// takes >= 2.5 hits on same-band trash (--power-curve). The big step is Common -> Uncommon -> Rare; from Epic on the
// affixes, the unique and the Mythic power carry the climb.
float rarityMult(Rarity r) {
  static const float m[] = {1.00f, 1.27f, 1.50f, 1.60f, 1.64f, 1.68f};
  static_assert(sizeof(m) / sizeof(m[0]) == (size_t)Rarity::COUNT, "a multiplier for every rarity");
  return m[std::clamp((int)r, 0, RARITIES - 1)];
}
// (M8b NUMBERS, 15.27.2 over 7.2) Uncommon one minor affix, Rare two, Epic three, Legendary three and its unique, Mythic
// four strong ones and its Mythic power
int affixSlots(Rarity r) {
  static const int n[] = {0, 1, 2, 3, 3, 4};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Rarity::COUNT, "a budget for every rarity");
  return n[std::clamp((int)r, 0, RARITIES - 1)];
}
void affixQuality(Rarity r, float& lo, float& hi) {
  static const float q[][2] = {{0.00f, 0.50f}, {0.00f, 0.50f}, {0.15f, 0.75f}, {0.30f, 0.90f}, {0.45f, 1.00f}, {0.75f, 1.00f}};
  static_assert(sizeof(q) / sizeof(q[0]) == (size_t)Rarity::COUNT, "a quality band for every rarity");
  const int k = std::clamp((int)r, 0, RARITIES - 1);
  lo = q[k][0];
  hi = q[k][1];
}

// ---- M8b Treasure (15.27 / 15.28). Phase A (lead) wrote the contract; the NUMBERS lane the level-gated ladder below.
const char* lootSrcName(LootSrc s) {
  static const char* n[] = {"BODY", "CHEST", "HOARD", "EXPLORE", "BOSS", "WORLD BOSS", "SHOP", "MERCHANT", "QUEST", "SAGA",
                            "SAGA FINALE", "CAMPAIGN FINALE", "FORGE", "COMPANION", "DEBUG"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)LootSrc::COUNT, "a name for every loot source");
  return n[std::clamp((int)s, 0, (int)LootSrc::COUNT - 1)];
}
int rarityFloorLevel(Rarity r) {
  static const int f[] = {1, 1, 5, 12, 20, 30};
  static_assert(sizeof(f) / sizeof(f[0]) == (size_t)Rarity::COUNT, "a floor for every rarity");
  return f[std::clamp((int)r, 0, RARITIES - 1)];
}
// =====================================================================================================================
// (M8b NUMBERS, owner 15.27.4) THE LADDER. "We dont over saturate the player with a bunch of rare equipment right off
// the bat": rarity is gated by the finder's level and by how hard the find was.
//
// 1. The gate level G = the finder's level (a shop / merchant: its settlement's D, never the player: 7.5 rule 4)
//      + half the gap to a harder place (at most +3; an easier place costs up to 6 levels: farming the start zone pays
//        poorly) + the source's LUCK (lootLuck, in levels):
//        rank:   elite 3, champion 5, named unique 8, boss 8, world boss 12
//        source: chest 1, quest / saga 3, explore 5, hoard 6, boss / saga finale 8, world boss / campaign finale 12
//                (the larger of the rank's and the source's), plus 6 a DropSource::boost step (-3 .. +3)
// 2. Each rarity above Common has a ramp from its floor level F (rarityFloorLevel 1 / 5 / 12 / 20 / 30): at G >= F it is
//    start + per x (G - F) percent, up to a cap; below its floor it never rolls.
//        Uncommon   F 1    15 %  +1.4 a level   cap 25 %
//        Rare       F 5     1 %  +0.9           cap 11 %
//        Epic       F 12  0.3 %  +0.45          cap 5.5 %
//        Legendary  F 20  0.2 %  +0.1           cap 2.5 %
//        Mythic     F 30 0.05 %  +0.03          cap 1 %
//    Every cap is at least twice the next one's (and Common keeps at least twice Uncommon's), so each step is at least 2x
//    rarer than the one below at every level. A source with luck also multiplies the Rare-and-up shares by 1 + 0.04 x
//    luck (a world boss x1.48).
//    Ordinary finds (a plain body or chest at D = the finder's level, gear only) come out at (C / U / R / E / L / M %):
//        levels 1-4  83 / 17 / 0 / 0 / 0 / 0     5-9  74 / 23 / 3 / 0      10-14  67 / 25 / 7 / 0.5
//        15-19  61 / 25 / 12 / 2.5 / 0           20-29  E ~5.5, L ~0.6     30-39  L ~1.6, M ~0.2     40-50  M ~0.5
// 3. The gate: nothing above the finder's gate top (rarityGateTop: the best rarity whose floor his level reached) plus
//    ONE step ("beat the gate a little, never a lot"): that step only comes from luck (G past its floor). Shops and
//    merchants never above Epic (their cap, atMost, says Rare unless a capital honours the hero).
// 4. Floors: a boss gives at least Uncommon, a named unique, a world boss or a saga finale at least Rare, quests and
//    sagas Uncommon; DropSource::atLeast adds to them. A floor never passes the gate (rarityAllowedTop: a named unique's
//    Rare at level 2 stands, being the one step over Uncommon; a world boss's atLeast Legendary at level 8 gives Epic).
//    What lies below the floor folds onto it; atMost folds what lies above onto the cap.
// 5. The one exception, LootSrc::CampaignFinale ("a campaign's last reward is the main way to a Legendary or Mythic
//    early"): not gated; Legendary (SOURCES sets atLeast = Legendary), with a Mythic chance from level 20 (5 %, +1 a
//    level, at most 30 %). Debug is not gated either (scripts and tests).
// =====================================================================================================================
namespace {
struct Ramp { float at, per, cap; };
const Ramp kRamp[RARITIES] = {{0, 0, 0}, {15.0f, 1.4f, 25.0f}, {1.0f, 0.9f, 11.0f}, {0.3f, 0.45f, 5.5f}, {0.2f, 0.1f, 2.5f}, {0.05f, 0.03f, 1.0f}};
float rampPct(int k, float G) {
  const float F = (float)rarityFloorLevel((Rarity)k);
  if (k <= 0 || G < F) return 0;
  return std::min(kRamp[k].cap, kRamp[k].at + kRamp[k].per * (G - F));
}
bool shopSrc(LootSrc s) { return s == LootSrc::Shop || s == LootSrc::Merchant; }
bool ungated(LootSrc s) { return s == LootSrc::CampaignFinale || s == LootSrc::Debug; }
// the gate's own level: the finder's (a shop's: its D)
int gateLevel(const DropSource& s) { return std::clamp(shopSrc(s.src) ? s.D : s.playerLevel, 1, MAX_D); }
// the floor a source guarantees by itself (before DropSource::atLeast)
Rarity srcFloor(const DropSource& s) {
  int f = 0;
  switch (s.src) {
    case LootSrc::Boss: case LootSrc::Quest: case LootSrc::Saga: f = 1; break;
    case LootSrc::WorldBoss: case LootSrc::SagaFinale: f = 2; break;
    case LootSrc::CampaignFinale: f = 4; break;
    default: break;
  }
  if (s.rank == 4) f = std::max(f, 1);
  if (s.rank == 3 || s.rank >= 5) f = std::max(f, 2);
  return (Rarity)f;
}
}  // namespace

Rarity rarityGateTop(int level) {
  int t = 1;
  for (int k = 2; k < RARITIES; k++) if (level >= rarityFloorLevel((Rarity)k)) t = k;
  return (Rarity)t;
}
Rarity rarityAllowedTop(const DropSource& s) {
  if (ungated(s.src)) return Rarity::Mythic;
  int t = std::min((int)rarityGateTop(gateLevel(s)) + 1, RARITIES - 1);
  if (shopSrc(s.src)) t = std::min(t, (int)Rarity::Epic);
  return (Rarity)t;
}
float lootLuck(const DropSource& s) {
  static const float rankL[6] = {0, 3, 5, 8, 8, 12};
  float srcL = 0;
  switch (s.src) {
    case LootSrc::Chest: srcL = 1; break;
    case LootSrc::Quest: case LootSrc::Saga: srcL = 3; break;
    case LootSrc::Explore: srcL = 5; break;
    case LootSrc::Hoard: srcL = 6; break;
    case LootSrc::Boss: case LootSrc::SagaFinale: srcL = 8; break;
    case LootSrc::WorldBoss: case LootSrc::CampaignFinale: srcL = 12; break;
    default: break;
  }
  // shops and merchants stock by their band: no rank, no luck but a boost (a capital's master smith)
  const float rl = shopSrc(s.src) ? 0.0f : rankL[std::min<int>(s.rank, 5)];
  return std::max(rl, srcL) + 6.0f * (float)std::clamp<int>(s.boost, -3, 3);
}
void rarityOdds(const DropSource& s, float pct[RARITIES]) {
  const int L = gateLevel(s);
  for (int k = 0; k < RARITIES; k++) pct[k] = 0;
  if (s.src == LootSrc::CampaignFinale) {
    const float my = s.playerLevel >= 20 ? std::min(30.0f, 5.0f + (float)(s.playerLevel - 20)) : 0.0f;
    pct[(int)Rarity::Mythic] = my;
    pct[(int)Rarity::Legendary] = 100.0f - my;
  } else {
    // the gate level: the finder's, half the gap to a harder place (+3 at most; an easier one costs up to 6), the luck
    const float luck = lootLuck(s);
    const float gap = shopSrc(s.src) ? 0.0f : std::clamp(0.5f * (float)(s.D - L), -6.0f, 3.0f);
    const float G = (float)L + gap + luck;
    const float mul = 1.0f + 0.04f * std::max(0.0f, luck);
    const int top = (int)rarityAllowedTop(s);
    float rest = 100.0f;
    for (int k = RARITIES - 1; k >= 1; k--) {
      pct[k] = k <= top ? rampPct(k, G) * (k >= 2 ? mul : 1.0f) : 0.0f;
      rest -= pct[k];
    }
    pct[0] = std::max(0.0f, rest);
  }
  // the floors (the source's own and atLeast) and the cap: what lies outside folds onto the nearest allowed step
  const int top = (int)rarityAllowedTop(s);
  int lo = std::max((int)srcFloor(s), std::clamp((int)s.atLeast, 0, RARITIES - 1));
  lo = std::min(lo, top);
  const int hi = std::clamp((int)s.atMost, lo, top);
  for (int k = 0; k < lo; k++) { pct[lo] += pct[k]; pct[k] = 0; }
  for (int k = hi + 1; k < RARITIES; k++) { pct[hi] += pct[k]; pct[k] = 0; }
}
Rarity rollRarity(Rng& r, const DropSource& s) {
  float pct[RARITIES];
  rarityOdds(s, pct);
  // one draw, the best steps first
  const float q = r.f() * 100.0f;
  float acc = 0;
  int low = 0;
  for (int k = RARITIES - 1; k > 0; k--) {
    acc += pct[k];
    if (pct[k] > 0 && q < acc) return (Rarity)k;
  }
  while (low < RARITIES - 1 && pct[low] <= 0) low++;   // (rounding at the top of the draw: the lowest step the odds hold)
  return (Rarity)low;
}
// (M8b NUMBERS, owner 15.27.3) "every item gets an item level from the player level and the source's danger": the
// rounded-up mean of the two, never more than 4 over the lower of them (a level-1 hero in a D-10 crypt finds item level
// 5, as level sync would use it anyway; a level-30 hero farming D 1 finds 5), then a bonus by what it came from: a named
// unique +3, a boss / world boss / campaign finale +2 (7.2), a hoard or an exploration find +1. Shops and merchants stock
// their settlement's band (D alone, 7.5 rule 4); Forge and Debug: D. Monotone in D and the finder's level.
int itemLevelFor(const DropSource& s) {
  const int D = std::clamp(s.D, 1, MAX_D);
  if (shopSrc(s.src) || s.src == LootSrc::Forge || s.src == LootSrc::Debug) return D;
  const int P = std::clamp(s.playerLevel, 1, MAX_D);
  int l = std::min((D + P + 1) / 2, std::min(D, P) + 4);
  int bonus = 0;
  if (s.rank == 3) bonus = 3;
  else if (s.rank >= 4) bonus = 2;
  switch (s.src) {
    case LootSrc::Boss: case LootSrc::WorldBoss: case LootSrc::SagaFinale: case LootSrc::CampaignFinale: bonus = std::max(bonus, 2); break;
    case LootSrc::Hoard: case LootSrc::Explore: bonus = std::max(bonus, 1); break;
    default: break;
  }
  l += bonus;
  return std::clamp(l, 1, MAX_D);
}
int dropIlvl(const DropSource& s) { return itemLevelFor(s); }

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
      // M8b (NUMBERS lane's final ranges; the POWERS lane's effect equals the shown value exactly)
      {6, 30},   {6, 30},  {6, 30},            // damage vs beasts, undead, people %
      {1, 10},                                 // burning damage a second for 3 s (points; a level-1 hit is ~8)
      {3, 15},   {5, 30},                      // attack speed %, armour rating %
      {2, 36},   {8, 60},                      // health healed every 5 s, max stamina (points)
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
                            "SORCERY",  "THE MAGE",   "THE ALCHEMIST", "FORTUNE", "VITALITY",
                            "THE HUNT", "THE GRAVE",  "THE DUEL",   "CINDERS",  "FURY",      "THE WALL",  "MENDING",
                            "ENDURANCE"};
  static_assert(sizeof(s) / sizeof(s[0]) == (size_t)Affix::COUNT, "a suffix for every affix");
  return s[std::clamp((int)a, 0, (int)Affix::COUNT - 1)];
}
// (M8b) what an affix at the middle of its range is worth, as a share of the piece's score (powerScore): direct damage
// and attack speed most, then health, armour and drain, utility least
float affixWeight(Affix a) {
  static const float w[] = {0,
                            0.050f, 0.050f, 0.050f,            // fire, frost, shadow damage
                            0.050f,                            // life drain
                            0.050f, 0.040f,                    // crit chance, crit damage
                            0.030f, 0.040f, 0.030f, 0.040f,    // stamina regen, move speed, roll distance, thorns
                            0.030f, 0.030f, 0.030f,            // resistances
                            0.050f, 0.030f,                    // spell power, magicka
                            0.030f, 0.020f, 0.050f,            // potion effect, gold find, health
                            0.040f, 0.040f, 0.040f,            // vs beasts, undead, people
                            0.050f, 0.060f, 0.050f,            // burning, attack speed, armour %
                            0.040f, 0.030f};                   // health regen, stamina
  static_assert(sizeof(w) / sizeof(w[0]) == (size_t)Affix::COUNT, "a weight for every affix");
  return w[std::clamp((int)a, 0, (int)Affix::COUNT - 1)];
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
    const float exact = nominalPower(it, nullptr);
    if (std::lround(exact) == (long)it.power) p = exact;
  }
  return p * statScale(eff) / statScale(it.ilvl);
}
float affixWorth(Affix a, int value, int eff) {
  if (a == Affix::None || (int)a >= (int)Affix::COUNT || value <= 0) return 0;
  return affixWeight(a) * (float)value / std::max(0.5f, rangeMid(a, eff));
}

// ===================================================================================================== uniques
namespace {
thread_local bool tCompanionRolls = false;
}
CompanionRolls::CompanionRolls() : prev(tCompanionRolls) { tCompanionRolls = true; }
CompanionRolls::~CompanionRolls() { tCompanionRolls = prev; }
bool affixWorksForCompanion(Affix a) {
  switch (a) {
    case Affix::Thorns: case Affix::StaminaRegen: case Affix::RollDist: case Affix::SpellPower: case Affix::Magicka:
    case Affix::PotionEffect: case Affix::GoldFind: case Affix::Stamina:
      return false;
    default: return affixImplemented(a);
  }
}
bool uniqueWorksForCompanion(Unique u) {
  using U = Unique;
  switch (u) {
    case U::ChainLightning: case U::StillCrit: case U::Thunderclap: case U::Echo: case U::Stormcaller: case U::Warcry:
    case U::Quickdraw: case U::Arcanist: case U::BloodMagic: case U::GoldenTouch: case U::Lifebloom: case U::Wayfarer:
    case U::TimeSlow: case U::SecondWind: case U::Ward: case U::FrostNova: case U::Thornmail:
    case U::Inferno: case U::Chronos: case U::Phoenix:
      return false;
    default: return uniqueImplemented(u);
  }
}
// (M8b) uniqueImplemented / affixImplemented live in rpg/sim/powers.cpp (the POWERS lane makes them work)
std::vector<Unique> mythicPowersFor(ItemKind k) {
  using U = Unique;
  std::vector<U> v;
  switch (k) {
    case ItemKind::Weapon: v = {U::Tempest, U::Inferno, U::Soulfeast, U::Starfall, U::Wolfking}; break;
    case ItemKind::Bow: v = {U::Tempest, U::Starfall, U::Wolfking}; break;
    case ItemKind::Staff: v = {U::Tempest, U::Starfall, U::Inferno}; break;
    case ItemKind::Ring: case ItemKind::Amulet: v = {U::Soulfeast, U::Phoenix, U::Chronos, U::Wolfking}; break;
    case ItemKind::Boots: case ItemKind::Cloak: v = {U::Inferno, U::Chronos, U::Phoenix}; break;
    case ItemKind::Armor: case ItemKind::Helmet: case ItemKind::Shield: case ItemKind::Gloves: v = {U::BrambleCrown, U::Phoenix, U::Chronos, U::Soulfeast}; break;
    default: break;
  }
  v.erase(std::remove_if(v.begin(), v.end(), [](U u) { return !uniqueImplemented(u) || (tCompanionRolls && !uniqueWorksForCompanion(u)); }), v.end());
  return v;
}
bool affixConflicts(Affix a, Affix b) {
  if (a == Affix::None || b == Affix::None) return false;
  if (a == b) return true;
  // contradictions (M8b NUMBERS): one element on a piece (fire, frost or shadow; burning is fire's, so never with frost or
  // shadow), and one bane (beasts, the undead or people: a piece is made against one foe)
  auto pair = [&](Affix x, Affix y) { return (a == x && b == y) || (a == y && b == x); };
  using A = Affix;
  return pair(A::FireDmg, A::FrostDmg) || pair(A::FireDmg, A::ShadowDmg) || pair(A::FrostDmg, A::ShadowDmg) || pair(A::FrostDmg, A::Burn) ||
         pair(A::ShadowDmg, A::Burn) || pair(A::VsBeasts, A::VsUndead) || pair(A::VsBeasts, A::VsPeople) || pair(A::VsUndead, A::VsPeople);
}
bool lootValid(const Item& it, std::string* why) {
  auto no = [&](const std::string& m) { if (why) *why = m + ": " + it.name; return false; };
  if (!itemEquippable(it.kind)) return true;
  if ((int)it.rarity >= RARITIES) return no("a rarity past Mythic");
  if (it.rarity == Rarity::Common) return true;
  if (it.ilvl == 0) return no("a " + std::string(rarityName(it.rarity)) + " piece without an item level");
  if (it.affixCount() < 1) return no("a " + std::string(rarityName(it.rarity)) + " piece with no affix");
  for (int i = 0; i < kItemAffixes; i++) {
    const ItemAffix& a = it.affix[i];
    if (a.kind == Affix::None) continue;
    if (!affixAllowed(a.kind, it.kind)) return no(std::string("affix ") + affixInfo(a.kind).name + " not allowed on this kind");
    if (!affixImplemented(a.kind)) return no(std::string("affix ") + affixInfo(a.kind).name + " does nothing yet");
    if (a.value <= 0) return no(std::string("affix ") + affixInfo(a.kind).name + " with no value");
    if (affixText(a.kind, a.value).empty()) return no(std::string("affix ") + affixInfo(a.kind).name + " has no words");
    for (int j = i + 1; j < kItemAffixes; j++)
      if (affixConflicts(a.kind, it.affix[j].kind)) return no(std::string("affixes in conflict: ") + affixInfo(a.kind).name + " / " + affixInfo(it.affix[j].kind).name);
  }
  if (it.rarity >= Rarity::Legendary) {
    if (it.unique == Unique::None || !uniqueImplemented(it.unique)) return no("a " + std::string(rarityName(it.rarity)) + " without a working power");
    // a Mythic carries a Mythic power when its kind has one that works (else a Legendary's: mythicPowersFor's rule)
    if (it.rarity == Rarity::Mythic && !uniqueIsMythic(it.unique) && !mythicPowersFor(it.kind).empty()) return no("a Mythic without a Mythic power");
    if (it.rarity == Rarity::Legendary && uniqueIsMythic(it.unique)) return no("a Legendary with a Mythic power");
  }
  return true;
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
  v.erase(std::remove_if(v.begin(), v.end(), [](U u) { return !uniqueImplemented(u) || (tCompanionRolls && !uniqueWorksForCompanion(u)); }), v.end());
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
  if (it.rarity == Rarity::Mythic) return mythicName(it, c);
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
// (M8b NUMBERS) a Mythic's own grammar, never a Legendary's ("X'S OATH" / "THE ADJ NOUN"): a compound of its power and
// its kind ("EMBERHEART", "STORMBRAND", "THORNCROWN"), then a title ("EMBERHEART, THE FIRST") or its maker's place
// ("STORMBRAND OF VALDHOLM", the culture's phonology) when the whole fits the HUD's 22 letters
std::string mythicName(const Item& it, const cult::Culture* c) {
  const cult::Culture& C = c ? *c : heartland();
  const uint32_t s = it.seed ? it.seed : 1u;
  auto pick = [&](uint32_t k, uint32_t n) {
    uint64_t x = (uint64_t)s * 0xD6E8FEB86659FD93ull ^ ((uint64_t)k + (uint64_t)it.kind * 0x2F1u) * 0x9E3779B97F4A7C15ull;
    x ^= x >> 32; x *= 0xBF58476D1CE4E5B9ull; x ^= x >> 29; x *= 0x94D049BB133111EBull; x ^= x >> 31;
    return (uint32_t)(x % n);
  };
  const char* const* first = nullptr;
  static const char* fTempest[] = {"STORM", "THUNDER", "TEMPEST"};
  static const char* fInferno[] = {"EMBER", "PYRE", "CINDER"};
  static const char* fSoul[] = {"SOUL", "BLOOD", "MAW"};
  static const char* fBramble[] = {"THORN", "BRIAR", "BRAMBLE"};
  static const char* fChronos[] = {"DAWN", "AEON", "HOUR"};
  static const char* fPhoenix[] = {"ASH", "PHOENIX", "REBORN"};
  static const char* fStar[] = {"STAR", "COMET", "SKY"};
  static const char* fWolf[] = {"WOLF", "MOON", "HOWL"};
  static const char* fOther[] = {"DOOM", "WORLD", "ELDER"};
  switch (it.unique) {
    case Unique::Tempest: first = fTempest; break;
    case Unique::Inferno: first = fInferno; break;
    case Unique::Soulfeast: first = fSoul; break;
    case Unique::BrambleCrown: first = fBramble; break;
    case Unique::Chronos: first = fChronos; break;
    case Unique::Phoenix: first = fPhoenix; break;
    case Unique::Starfall: first = fStar; break;
    case Unique::Wolfking: first = fWolf; break;
    default: first = fOther; break;
  }
  static const char* sWeapon[] = {"BRAND", "REAVER", "BANE", "FANG"};
  static const char* sBow[] = {"SONG", "STRING", "WING", "REACH"};
  static const char* sStaff[] = {"CALL", "WAKE", "SPIRE", "WEAVE"};
  // (M8b fixer r3, review: "'Bloodmantle' drawn as steel chain mail") a mantle or a hide is a soft body's word; a mail
  // or plate body takes a metal one
  static const char* sBody[] = {"HEART", "MANTLE", "HIDE", "WARD"};
  static const char* sBodyMetal[] = {"HEART", "WARD", "MAIL", "CORE"};
  static const char* sHelm[] = {"CROWN", "BROW", "HELM", "GAZE"};
  static const char* sShield[] = {"WALL", "WARD", "GUARD", "GATE"};
  static const char* sHands[] = {"GRASP", "HAND", "GRIP", "CLAW"};
  static const char* sFeet[] = {"STRIDE", "WALKER", "TREAD", "RUNNER"};
  static const char* sCloak[] = {"SHROUD", "VEIL", "WING", "PALL"};
  static const char* sJewel[] = {"HEART", "EYE", "SEAL", "TEAR"};
  const char* const* second = sJewel;
  switch (it.kind) {
    case ItemKind::Weapon: second = sWeapon; break;
    case ItemKind::Bow: second = sBow; break;
    case ItemKind::Staff: second = sStaff; break;
    case ItemKind::Armor: second = (it.mat == Mat::Leather || it.mat == Mat::Cloth) ? sBody : sBodyMetal; break;
    case ItemKind::Helmet: second = sHelm; break;
    case ItemKind::Shield: second = sShield; break;
    case ItemKind::Gloves: second = sHands; break;
    case ItemKind::Boots: second = sFeet; break;
    case ItemKind::Cloak: second = sCloak; break;
    default: break;
  }
  const std::string comp = std::string(first[pick(0x61u, 3u)]) + second[pick(0x62u, 4u)];
  static const char* title[] = {"FIRST", "LAST", "UNBOUND", "UNDYING", "ELDER", "UNSEEN", "ENDLESS", "BURNING", "WAKING", "HOLLOW"};
  const std::string a = comp + ", THE " + title[pick(0x63u, 10u)];
  std::string place = cult::placeName(C, hash32(s ^ 0x3A7Fu));
  const std::string b = comp + " OF " + place;
  const bool wantB = pick(0x64u, 3u) == 0;
  const size_t cap = 22;
  if (wantB && b.size() <= cap) return b;
  if (a.size() <= cap) return a;
  if (b.size() <= cap) return b;
  return comp;
}
std::string mythicLore(const Item& it, const cult::Culture* c) {
  const cult::Culture& C = c ? *c : heartland();
  const uint32_t s = it.seed ? it.seed : 1u;
  std::string place = cult::placeName(C, hash32(s ^ 0x5D1u));
  std::string who = cult::personName(C, hash32(s ^ 0x6E2u), (s >> 5) & 1u);
  const size_t sp = who.find(' ');
  if (sp != std::string::npos) who = who.substr(0, sp);
  switch (hash32(s ^ 0x7F3u) % 5u) {
    case 0: return "IT FELL BURNING FROM THE SKY OVER " + place + ".";
    case 1: return "SEVEN SMITHS OF " + place + " DIED TO MAKE IT.";
    case 2: return who + " WOKE IT IN THE DEEP UNDER " + place + ".";
    case 3: return "THE LAST TIME IT WAS RAISED, " + place + " BURNED.";
    default: return "OLDER THAN " + place + ", AND IT REMEMBERS.";
  }
}
std::string legendaryLore(const Item& it, const cult::Culture* c) {
  if (it.rarity == Rarity::Mythic) return mythicLore(it, c);
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
    default: return armourBase(it.kind);   // (M8b: leather's 0.92 is matMult's now)
  }
}
// the band's own material step: bronze 1, iron 1.06, steel 1.12, then +0.04 a band (the heartland's band words, an
// alloy of that tier step)
float bandStep(int band) {
  band = std::clamp(band, 1, BANDS);
  if (band == 1) return 1.00f;
  if (band == 2) return 1.06f;
  return 1.12f + 0.04f * (float)(band - 3);
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
  // (M8b) only affixes that work roll (gear::affixImplemented, rpg/sim/powers.cpp)
  for (int a = 1; a < (int)Affix::COUNT; a++)
    if (affixAllowed((Affix)a, it.kind) && affixImplemented((Affix)a) && (!tCompanionRolls || affixWorksForCompanion((Affix)a))) pool.push_back((Affix)a);
  n = std::min(n, kItemAffixes);
  // (M8b NUMBERS) the rarity's part of the eff range: Uncommon the lower half (one minor affix), Mythic the top quarter
  float qlo, qhi;
  affixQuality(it.rarity, qlo, qhi);
  for (int k = 0; k < n && !pool.empty(); k++) {
    const int pick = r.irange((int)pool.size());
    const Affix a = pool[(size_t)pick];
    // (M8b) no duplicate and no contradiction on one item: what conflicts with the pick leaves the pool
    pool.erase(std::remove_if(pool.begin(), pool.end(), [&](Affix b) { return affixConflicts(a, b); }), pool.end());
    int lo, hi;
    affixRange(a, std::max<int>(1, it.ilvl), lo, hi);
    const int span = hi - lo;
    const int vlo = lo + (int)std::lround((float)span * qlo), vhi = std::max(vlo, lo + (int)std::lround((float)span * qhi));
    it.affix[k].kind = a;
    it.affix[k].value = (int16_t)std::max(1, vlo + r.irange(vhi - vlo + 1));
  }
}
// the power a Legendary / Mythic owes (a Mythic power when its kind has a working one, else a Legendary's unique)
void rollPower(Rng& r, Item& it) {
  if (it.rarity < Rarity::Legendary) { it.unique = Unique::None; return; }
  std::vector<Unique> u = it.rarity == Rarity::Mythic ? mythicPowersFor(it.kind) : std::vector<Unique>();
  if (u.empty()) u = uniquesFor(it.kind);
  it.unique = u.empty() ? Unique::None : u[(size_t)r.irange((int)u.size())];
}
// (M8b NUMBERS, 15.27.2 "no item above Common may roll without at least one working, visible affix") whatever a piece
// above Common still owes (an affix, a working power, a Mythic power on a Mythic), rolled from its own seed so the same
// piece always comes out the same: makeGearC never leaves a debt, this catches hand-made pieces and older code paths
void settleDebts(Item& it) {
  if (!itemEquippable(it.kind) || it.rarity == Rarity::Common) return;
  bool owesAffix = it.affixCount() == 0;
  for (const ItemAffix& a : it.affix)
    if (a.kind != Affix::None && (!affixImplemented(a.kind) || !affixAllowed(a.kind, it.kind) || a.value <= 0)) owesAffix = true;
  bool owesPower = false;
  if (it.rarity >= Rarity::Legendary) {
    if (it.unique == Unique::None || !uniqueImplemented(it.unique)) owesPower = true;
    if (it.rarity == Rarity::Mythic && !uniqueIsMythic(it.unique) && !mythicPowersFor(it.kind).empty()) owesPower = true;
    if (it.rarity == Rarity::Legendary && uniqueIsMythic(it.unique)) owesPower = true;
  }
  if (!owesAffix && !owesPower) return;
  Rng r(hash32((it.seed ? it.seed : 1u) ^ 0x5E771Eu ^ ((uint32_t)it.rarity << 8) ^ (uint32_t)it.kind));
  if (owesAffix) {
    // keep the working ones that do not conflict, roll the rest of the budget
    Item keep = it;
    rollAffixes(r, it, affixSlots(it.rarity));
    if (keep.affixCount() > 0) {
      int n = 0;
      ItemAffix ok[kItemAffixes];
      for (const ItemAffix& a : keep.affix) {
        if (a.kind == Affix::None || !affixImplemented(a.kind) || !affixAllowed(a.kind, it.kind) || a.value <= 0) continue;
        bool clash = false;
        for (int j = 0; j < n; j++) clash |= affixConflicts(a.kind, ok[j].kind);
        if (!clash && n < kItemAffixes) ok[n++] = a;
      }
      for (const ItemAffix& a : it.affix) {
        if (a.kind == Affix::None || n >= std::max(1, affixSlots(it.rarity))) continue;
        bool clash = false;
        for (int j = 0; j < n; j++) clash |= affixConflicts(a.kind, ok[j].kind);
        if (!clash) ok[n++] = a;
      }
      for (int j = 0; j < kItemAffixes; j++) it.affix[j] = j < n ? ok[j] : ItemAffix();
    }
  }
  if (owesPower) rollPower(r, it);
}
}  // namespace

float matMult(const Item& it, const cult::Culture* c) {
  const int band = bandOf(std::max(1, (int)it.ilvl));
  switch (it.mat) {
    case Mat::Leather: return 0.92f;
    case Mat::Bronze: return 1.00f;
    case Mat::Iron: return 1.06f;
    case Mat::Steel: return band >= 4 ? bandStep(band) : 1.12f;   // above band 3 the heartland's band words (GILDED...)
    case Mat::Alloy: {
      const cult::Alloy* A = alloyOf(c, it.alloy);
      return A ? 1.12f + 0.04f * (float)std::max(1, (int)A->tierStep) : bandStep(std::max(4, band));
    }
    default: return bandStep(band);   // wood, cloth, bone, jewellery: their band's step
  }
}
float nominalPower(const Item& it, const cult::Culture* c) {
  if (!itemEquippable(it.kind) || jewel(it.kind)) return 0;
  return powerBase(it) * statScale(std::max(1, (int)it.ilvl)) * matMult(it, c) * rarityMult(it.rarity);
}

void finishGear(Item& it, const cult::Culture* c) {
  const int ilvl = std::clamp<int>(it.ilvl, 1, MAX_D);
  it.tier = (uint8_t)(bandOf(ilvl) - 1);
  it.icon = iconOf(it.kind, it.sub);
  // (M8b) nothing above Common leaves the maker without a working affix (and a Legendary / Mythic without its power)
  settleDebts(it);
  // power at the item's own level (usePower syncs it at use): base x item level x material x rarity
  it.power = (int16_t)std::lround(nominalPower(it, c));
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
  // the name in the maker's words: a Mythic its own grammar, a Legendary its culture's phonology
  if (it.rarity == Rarity::Mythic) it.name = mythicName(it, c);
  else if (it.rarity == Rarity::Legendary) it.name = legendaryName(it, c);
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
  // (M8b fixer r3) s^1.5 as s * sqrt(s): sqrt is exact under IEEE, powf is not (MSVC / glibc may differ by an ulp at a tie)
  const float sv = statScale(ilvl);
  const float lv = sv * std::sqrt(sv);
  float v = (float)valueBase(it.kind) * lv * (1.0f + 0.5f * (float)it.rarity) + 12.0f * (float)it.affixCount() * statScale(ilvl);
  if (it.rarity == Rarity::Legendary) v *= 2.0f;
  if (it.rarity == Rarity::Mythic) v *= 3.0f;
  if (it.mat == Mat::Alloy) v *= 1.25f;
  it.value = std::max(1, (int)std::lround(v));
}

Item makeGearC(Rng& r, ItemKind kind, int sub, int ilvl, Rarity rarity, const cult::Culture* c, Mat mat, uint8_t alloy) {
  Item it;
  it.kind = kind;
  it.sub = kind == ItemKind::Weapon ? (uint8_t)std::clamp(sub, 0, (int)WeaponType::COUNT - 1) : 0;
  it.ilvl = (uint8_t)std::clamp(ilvl, 1, MAX_D);
  it.seed = r.next();
  it.rarity = (Rarity)std::clamp((int)rarity, 0, RARITIES - 1);
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
  rollPower(r, it);
  finishGear(it, c);
  return it;
}

Item makeGear(Rng& r, ItemKind kind, int sub, int ilvl, Rarity rarity, uint64_t culture, Mat mat, uint8_t alloy) {
  Item it = makeGearC(r, kind, sub, ilvl, rarity, nullptr, mat, alloy);
  if (culture) it.culture = culture;   // the id alone (no culture data in hand: heartland words and forms)
  return it;
}

Item rollDropC(Rng& r, const DropSource& s, const cult::Culture* c) {
  // bosses, world bosses and every reward-like source always give gear; a body or a chest the classic loot table's shares
  const bool big = s.rank >= 4 || (s.src != LootSrc::Body && s.src != LootSrc::Chest);
  const float q = big ? 0.99f : r.f();
  if (q < 0.30f) {   // (draws sequenced: one a statement, the size before the kind, as the M6 stream had them)
    const int D = std::max(1, s.D);
    int size = 0;
    if (D > 12) size = r.f() < 0.3f ? 2 : 1;
    else if (D > 5) size = r.f() < 0.5f ? 1 : 0;
    const int pt = r.irange(3);
    return makePotion((PotionType)pt, size);
  }
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
  // (M8b NUMBERS) one roll on the level-gated ladder: the rank, the source, its boost, floor and cap are all in the odds
  const Rarity rar = rollRarity(r, s);
  return makeGearC(r, kind, sub, itemLevelFor(s), rar, c);
}

Item rollDrop(Rng& r, const DropSource& s) { return rollDropC(r, s, nullptr); }

// (M8b NUMBERS) one comparable worth: the synced power (jewellery: a nominal 10 at its level and rarity) x (1 + what the
// affixes are worth (affixWorth, synced values) + a working unique 0.15 / a Mythic power 0.25)
float powerScore(const Item& it, int L) {
  if (!itemEquippable(it.kind)) return 0;
  const int eff = effLevel(it.ilvl, L);
  const float p = jewel(it.kind) ? 10.0f * statScale(eff) * rarityMult(it.rarity) : usePower(it, L);
  float k = 1.0f;
  for (const ItemAffix& a : it.affix) if (a.kind != Affix::None) k += affixWorth(a.kind, affixUse(it, a.kind, L), eff);
  if (it.unique != Unique::None && uniqueImplemented(it.unique)) k += uniqueIsMythic(it.unique) ? 0.25f : 0.15f;
  return p * k;
}

void adoptClassic(Item& it, int playerLevel) {
  if (!itemEquippable(it.kind) || it.ilvl != 0) return;
  it.ilvl = (uint8_t)std::clamp(playerLevel, 1, MAX_D);
  it.tier = (uint8_t)(bandOf(it.ilvl) - 1);
  // its own fields seed it, so the same classic item always becomes the same piece
  uint32_t h = hash32((uint32_t)it.kind * 0x9E37u ^ (uint32_t)it.sub * 0x85EBu ^ (uint32_t)it.power * 0xC2B3u ^ (uint32_t)it.rarity);
  for (char ch : it.name) h = hash32(h ^ (uint8_t)ch);
  if (!it.seed) it.seed = h | 1u;
  if (it.mat == Mat::None) { uint8_t al = 0; it.mat = matFor(it.kind, it.ilvl, it.seed, nullptr, al); if (it.mat == Mat::Alloy) it.mat = Mat::Steel; }
  if (it.kind == ItemKind::Ring || it.kind == ItemKind::Amulet) { if (it.rarity < Rarity::Uncommon) it.rarity = Rarity::Uncommon; }
  Rng r(h);
  if (it.rarity > Rarity::Common && it.affixCount() == 0) rollAffixes(r, it, affixSlots(it.rarity));
  if (it.rarity >= Rarity::Legendary && (it.unique == Unique::None || !uniqueImplemented(it.unique))) rollPower(r, it);
  settleDebts(it);
  // the classic enchantment is folded away (the affixes are the item's words now); power and name are kept
  it.ench = Ench::None;
  it.enchPow = 0;
}

std::string gearName(const Item& it, const cult::Culture* maker) {
  if (!itemEquippable(it.kind) || it.ilvl == 0) return it.name;
  Item t = it;
  finishGear(t, maker);
  return t.name;
}

}  // namespace gear
