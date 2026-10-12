// rpg_test --loot: the M8b Treasure loot rules (VISION_PLAN 15.27, 15.28). NUMBERS lane. The gate 15.27.4 asks for:
//   (a) the rarity spread per level band for every LootSrc (a table). FAIL when, by level 5, an ordinary source gives
//       less than 90 % Common + Uncommon, more than 5 % Rare or any Epic+; Epic+ by level 5 outside bosses, named
//       uniques, hoards and finales; an ordinary step not at least 2x rarer than the one below; a rarity past the
//       finder's gate top + ONE step (CampaignFinale and Debug excepted); a shop above Epic; the measured drops off the
//       odds; item level not monotone in D and the finder's level.
//   (b) every non-Common item from every source passes gear::lootValid (tens of thousands of rolls, all kinds, cultures
//       from cult::Atlas::make), the classic makers too (no legacy enchantment, an item level, the start sword Common);
//   (c) power curves by item level and rarity (a table): each step at least +10 % over the one below at the same item
//       level, and the owner's iron / bronze example as a hard check (player level 10: a level-10 Common iron sword is
//       within +-15 % of a level-2 Rare bronze one; a level-2 Uncommon bronze one is 0-25 % below it; an Epic above it;
//       the best Rare rolls still win through their perks);
//   (d) no duplicate or contradictory affixes; budgets (affixSlots) and the rarity's quality band (affixQuality);
//   (e) determinism: identical items from identical seeds; a golden hash of a fixed stream (the fields the POWERS
//       lane's working set cannot change), so the clang build (CI) must match MSVC; and a source lint: no statement of
//       rpg/sim/gear.cpp or items.cpp holds two RNG draws (their order would be the compiler's).
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/gear.h"
#include "tools/tests/tests.h"

namespace {

using gear::LootSrc;

struct Case {
  const char* name;
  LootSrc src;
  uint8_t rank;
  int8_t boost;
  bool ordinary;   // an ordinary find: by level 5 >= 90 % Common + Uncommon, <= 5 % Rare; each step 2x rarer than the last
  bool epicOk;     // may give Epic+ by level 5 (bosses, named uniques, world bosses, hoards, finales, debug)
};
const Case kCases[] = {
    {"BODY", LootSrc::Body, 0, 0, true, false},            {"BODY ELITE", LootSrc::Body, 1, 0, true, false},
    {"BODY CHAMPION", LootSrc::Body, 2, 0, false, false},  {"BODY NAMED", LootSrc::Body, 3, 0, false, true},
    {"BODY +1 BOOST", LootSrc::Body, 0, 1, false, false},  {"CHEST", LootSrc::Chest, 0, 0, true, false},
    {"HOARD", LootSrc::Hoard, 0, 0, false, true},          {"EXPLORE", LootSrc::Explore, 0, 0, false, false},
    {"BOSS", LootSrc::Boss, 4, 0, false, true},            {"WORLD BOSS", LootSrc::WorldBoss, 5, 0, false, true},
    {"SHOP", LootSrc::Shop, 0, 0, true, false},            {"MERCHANT", LootSrc::Merchant, 0, 0, true, false},
    {"QUEST", LootSrc::Quest, 0, 0, true, false},          {"SAGA", LootSrc::Saga, 0, 0, true, false},
    {"SAGA FINALE", LootSrc::SagaFinale, 0, 0, false, true}, {"CAMPAIGN FINALE", LootSrc::CampaignFinale, 0, 0, false, true},
    {"FORGE", LootSrc::Forge, 0, 0, true, false},          {"COMPANION", LootSrc::Companion, 0, 0, true, false},
    {"DEBUG", LootSrc::Debug, 0, 0, false, true},
};
gear::DropSource srcOf(const Case& c, int L, int D) {
  gear::DropSource s;
  s.src = c.src; s.rank = c.rank; s.boost = c.boost; s.playerLevel = L; s.D = std::clamp(D, 1, gear::MAX_D);
  if (c.src == LootSrc::CampaignFinale) s.atLeast = Rarity::Legendary;   // as SOURCES sets it
  return s;
}
const ItemKind kKinds[] = {ItemKind::Weapon, ItemKind::Bow, ItemKind::Staff, ItemKind::Armor, ItemKind::Helmet, ItemKind::Shield,
                           ItemKind::Gloves, ItemKind::Boots, ItemKind::Cloak, ItemKind::Ring, ItemKind::Amulet};
bool jewelK(ItemKind k) { return k == ItemKind::Ring || k == ItemKind::Amulet; }
const int kBands[][2] = {{1, 4}, {5, 9}, {10, 14}, {15, 19}, {20, 29}, {30, 39}, {40, 50}};

struct Ctx {
  int bad = 0;
  void fail(const std::string& m) { if (bad < 60) out("FAIL: loot: %s\n", m.c_str()); bad++; }
};

// the extra checks every generated piece gets beyond lootValid: no duplicate / conflict, the budget, the quality band
void checkPiece(Ctx& X, const Item& it, bool exactBudget) {
  std::string why;
  if (!gear::lootValid(it, &why)) X.fail(why);
  if (!itemEquippable(it.kind)) return;
  if (it.ench != Ench::None || it.enchPow) X.fail("a legacy enchantment on " + it.name);
  if (it.ilvl == 0) X.fail("a piece without an item level: " + it.name);
  if (it.name.empty() || it.value <= 0) X.fail("an unnamed or valueless piece");
  for (int i = 0; i < kItemAffixes; i++)
    for (int j = i + 1; j < kItemAffixes; j++)
      if (it.affix[i].kind != Affix::None && gear::affixConflicts(it.affix[i].kind, it.affix[j].kind))
        X.fail(std::string("duplicate / contradictory affixes ") + affixInfo(it.affix[i].kind).name + " + " + affixInfo(it.affix[j].kind).name + " on " + it.name);
  if (exactBudget && it.affixCount() != gear::affixSlots(it.rarity))
    X.fail("affix budget " + std::to_string(it.affixCount()) + " for a " + rarityName(it.rarity) + ": " + it.name);
  float qlo, qhi;
  gear::affixQuality(it.rarity, qlo, qhi);
  for (const ItemAffix& a : it.affix) {
    if (a.kind == Affix::None) continue;
    int lo, hi;
    gear::affixRange(a.kind, it.ilvl, lo, hi);
    const int vlo = lo + (int)std::lround((float)(hi - lo) * qlo), vhi = std::max(vlo, lo + (int)std::lround((float)(hi - lo) * qhi));
    if (a.value < std::max(1, vlo) || a.value > std::max(1, vhi))
      X.fail(std::string("affix ") + affixInfo(a.kind).name + " " + std::to_string(a.value) + " outside the " + rarityName(it.rarity) + " band " +
             std::to_string(vlo) + ".." + std::to_string(vhi) + " (item level " + std::to_string(it.ilvl) + ")");
    if (affixText(a.kind, a.value).empty()) X.fail("an affix with no words");
  }
  if (it.rarity == Rarity::Mythic && it.name.size() > 22) X.fail("a Mythic name past 22 letters: " + it.name);
}

// ---------------------------------------------------------------- (a) the ladder
void ladder(Ctx& X) {
  printf("loot (a): the odds per level band (D = the finder's level), %% common / uncommon / rare / epic / legendary / mythic\n");
  printf("  %-16s", "source");
  for (const auto& B : kBands) printf(" | L%2d-%-2d                       ", B[0], B[1]);
  printf("\n");
  for (const Case& c : kCases) {
    printf("  %-16s", c.name);
    for (const auto& B : kBands) {
      float avg[RARITIES] = {};
      for (int L = B[0]; L <= B[1]; L++) {
        float p[RARITIES];
        gear::rarityOdds(srcOf(c, L, L), p);
        for (int k = 0; k < RARITIES; k++) avg[k] += p[k] / (float)(B[1] - B[0] + 1);
      }
      printf(" |");
      for (int k = 0; k < RARITIES; k++) printf(k < 2 ? " %4.1f" : " %4.2f", avg[k]);
    }
    printf("\n");
  }
  // the rules, at every level and a range of places
  for (const Case& c : kCases) {
    for (int L = 1; L <= gear::LEVEL_CAP; L++)
      for (int dd : {-10, -3, 0, 3, 6, 10, 20}) {
        const int D = L + dd;
        if (D < 1 || D > gear::MAX_D) continue;
        const gear::DropSource s = srcOf(c, L, D);
        float p[RARITIES];
        gear::rarityOdds(s, p);
        float sum = 0;
        for (int k = 0; k < RARITIES; k++) { sum += p[k]; if (p[k] < 0) X.fail(std::string(c.name) + ": negative odds"); }
        if (std::fabs(sum - 100.0f) > 0.01f) X.fail(std::string(c.name) + ": odds sum to " + std::to_string(sum));
        const std::string at = std::string(c.name) + " at level " + std::to_string(L) + " D " + std::to_string(D);
        // the gate: the gate top of the finder's level (a shop's: its D) + one step
        if (c.src != LootSrc::CampaignFinale && c.src != LootSrc::Debug) {
          const int gl = (c.src == LootSrc::Shop || c.src == LootSrc::Merchant) ? D : L;
          const int top = std::min((int)gear::rarityGateTop(gl) + 1, RARITIES - 1);
          for (int k = top + 1; k < RARITIES; k++)
            if (p[k] > 0) X.fail(at + ": " + rarityName((Rarity)k) + " past the gate (top " + rarityName(gear::rarityGateTop(gl)) + " + 1)");
          if (gear::rarityAllowedTop(s) != (Rarity)(c.src == LootSrc::Shop || c.src == LootSrc::Merchant ? std::min(top, (int)Rarity::Epic) : top))
            X.fail(at + ": rarityAllowedTop disagrees with the gate");
        }
        if ((c.src == LootSrc::Shop || c.src == LootSrc::Merchant) && (p[(int)Rarity::Legendary] > 0 || p[(int)Rarity::Mythic] > 0))
          X.fail(at + ": a shop / merchant may give a Legendary or Mythic");
        // by level 5 (D = L): ordinary sources mostly Common and Uncommon, Rare rare, no Epic+
        if (dd == 0 && L <= 5) {
          const float epicUp = p[3] + p[4] + p[5];
          if (c.ordinary) {
            if (p[0] + p[1] < 90.0f) X.fail(at + ": Common + Uncommon only " + std::to_string(p[0] + p[1]) + " %");
            if (p[2] > 5.0f) X.fail(at + ": Rare at " + std::to_string(p[2]) + " %");
          }
          if (epicUp > 0 && !c.epicOk) X.fail(at + ": Epic or better by level 5 outside bosses, hoards and finales");
        }
        // each ordinary step at least 2x rarer than the one below (where neither is a folded floor)
        if (c.ordinary && (dd == 0 || dd == 10))
          for (int k = 1; k < RARITIES; k++)
            if (p[k] > 0 && p[k - 1] > 0 && p[k] * 2.0f > p[k - 1] + 0.01f)
              X.fail(at + ": " + rarityName((Rarity)k) + " " + std::to_string(p[k]) + " % not 2x rarer than " + rarityName((Rarity)(k - 1)) + " " +
                     std::to_string(p[k - 1]) + " %");
      }
    // the item level: monotone in D and in the finder's level, dropIlvl the same rule, D = L gives about L
    for (int L = 1; L <= gear::LEVEL_CAP; L++)
      for (int D = 1; D < gear::MAX_D; D++) {
        const gear::DropSource s = srcOf(c, L, D), sD = srcOf(c, L, D + 1), sL = srcOf(c, std::min(L + 1, gear::LEVEL_CAP), D);
        const int il = gear::itemLevelFor(s);
        if (gear::itemLevelFor(sD) < il || gear::itemLevelFor(sL) < il) { X.fail(std::string(c.name) + ": item level not monotone at L " + std::to_string(L) + " D " + std::to_string(D)); break; }
        if (gear::dropIlvl(s) != il) X.fail("dropIlvl != itemLevelFor");
        if (c.src == LootSrc::Shop || c.src == LootSrc::Merchant || c.src == LootSrc::Forge || c.src == LootSrc::Debug) {
          if (il != D) X.fail(std::string(c.name) + ": item level reads the player (a shop, the forge and debug stock by D)");
        }
        else if (il > std::min(D, L) + 4 + 3) X.fail(std::string(c.name) + ": item level " + std::to_string(il) + " past min(D, L) + 4 + bonus");
      }
  }
  // the campaign finale: the main way to an early Legendary; Mythic from level 20
  {
    float p[RARITIES];
    gear::rarityOdds(srcOf(kCases[15], 8, 8), p);
    if (p[(int)Rarity::Legendary] < 99.9f) X.fail("a level-8 campaign finale is not a Legendary");
    gear::rarityOdds(srcOf(kCases[15], 30, 30), p);
    if (p[(int)Rarity::Mythic] < 10.0f || p[(int)Rarity::Mythic] > 30.0f) X.fail("a level-30 campaign finale's Mythic chance " + std::to_string(p[5]));
    printf("  campaign finale: level 8 %.0f %% Legendary; level 20 Mythic %.0f %%, level 30 %.0f %%\n", 100.0f, 5.0f, p[5]);
  }
  // the measured drops (rollDropC of an ordinary body, gear only, no jewellery) against the odds
  printf("  measured, an ordinary body (6000 pieces a band):\n");
  Rng r(0x7EA5u);
  for (const auto& B : kBands) {
    int cnt[RARITIES] = {}, n = 0, drops = 0;
    float want[RARITIES] = {};
    while (n < 6000 && drops < 80000) {
      const int L = B[0] + drops % (B[1] - B[0] + 1);
      drops++;
      gear::DropSource s = srcOf(kCases[0], L, L);
      const Item it = gear::rollDropC(r, s, nullptr);
      if (!itemEquippable(it.kind) || jewelK(it.kind)) continue;
      float p[RARITIES];
      gear::rarityOdds(s, p);
      for (int k = 0; k < RARITIES; k++) want[k] += p[k];
      n++;
      cnt[(int)it.rarity]++;
      checkPiece(X, it, false);
    }
    printf("    levels %2d-%2d:", B[0], B[1]);
    for (int k = 0; k < RARITIES; k++) {
      const float got = 100.0f * (float)cnt[k] / (float)std::max(1, n), w = want[k] / (float)std::max(1, n);
      printf(" %5.1f", got);
      if (std::fabs(got - w) > 1.6f) X.fail("levels " + std::to_string(B[0]) + "-" + std::to_string(B[1]) + " " + rarityName((Rarity)k) + " measured " + std::to_string(got) + " %, odds " + std::to_string(w));
    }
    printf("\n");
  }
}

// ---------------------------------------------------------------- (b) every source, every kind, every culture
int validity(Ctx& X) {
  std::vector<cult::Culture> cultures;
  for (int a = 0; a < (int)cult::Archetype::COUNT; a++) {
    cultures.push_back(cult::Atlas::make((cult::Archetype)a, 6100u + (uint32_t)a));
    cultures.back().id = cult::familyId(a, 4);
  }
  int pieces = 0, above = 0;
  Rng r(0xB0B5u);
  // every rarity of every kind at every item level, heartland and every culture
  for (int rar = 0; rar < RARITIES; rar++)
    for (ItemKind k : kKinds)
      for (int il = 1; il <= gear::MAX_D; il += 3)
        for (int ci = -1; ci < (int)cultures.size(); ci += 2) {
          const cult::Culture* c = ci < 0 ? nullptr : &cultures[(size_t)ci];
          const Item it = gear::makeGearC(r, k, k == ItemKind::Weapon ? il % (int)WeaponType::COUNT : 0, il, (Rarity)rar, c);
          pieces++;
          above += it.rarity > Rarity::Common;
          checkPiece(X, it, true);
          if ((int)it.rarity < rar) X.fail(std::string("rarity lost: ") + it.name);
        }
  // every source's real rolls
  for (const Case& cs : kCases)
    for (int L = 1; L <= gear::LEVEL_CAP; L += 3)
      for (int dd : {-6, 0, 8}) {
        const gear::DropSource s = srcOf(cs, L, L + dd);
        for (int i = 0; i < 30; i++) {
          const cult::Culture* c = (i % 3) ? &cultures[(size_t)(i + L) % cultures.size()] : nullptr;
          const Item it = gear::rollDropC(r, s, c);
          if (!itemEquippable(it.kind)) continue;
          pieces++;
          above += it.rarity > Rarity::Common;
          checkPiece(X, it, true);
          if (it.ilvl != gear::itemLevelFor(s)) X.fail(std::string(cs.name) + ": item level " + std::to_string(it.ilvl) + " != itemLevelFor " + std::to_string(gear::itemLevelFor(s)));
          if ((int)it.rarity > (int)gear::rarityAllowedTop(s)) X.fail(std::string(cs.name) + ": rolled past its gate: " + it.name);
        }
      }
  // the classic makers are M6 gear now (15.27.2 audit: tests, debug kits, old paths)
  for (int lv = 1; lv <= 50; lv++)
    for (int i = 0; i < 12; i++) {
      Item v[] = {makeWeapon(r, lv), makeBow(r, lv), makeStaff(r, lv), makeArmor(r, lv, randomArmorSlot(r)), makeJewel(r, lv), randomLoot(r, lv, i % 4 == 0)};
      for (const Item& it : v) {
        if (!itemEquippable(it.kind)) continue;
        pieces++;
        above += it.rarity > Rarity::Common;
        checkPiece(X, it, false);
        if (it.ilvl == 0) X.fail("a classic maker made a piece without an item level: " + it.name);
      }
    }
  {
    Rng s(1);
    const Item sw = makeWeapon(s, 1, (int)WeaponType::Sword, false);
    if (sw.rarity != Rarity::Common || sw.ilvl != 1 || sw.kind != ItemKind::Weapon || sw.sub != (uint8_t)WeaponType::Sword) X.fail("the start sword is not a Common level-1 sword: " + sw.name);
  }
  // the safety nets: a hand-made classic piece of every rarity (adoptClassic) and a hand-made M6 one without affixes
  for (int rar = 0; rar < RARITIES; rar++)
    for (ItemKind k : kKinds) {
      Item c;
      c.kind = k; c.rarity = (Rarity)rar; c.power = 9; c.name = "OLD PIECE"; c.ench = Ench::Fire; c.enchPow = 5;
      gear::adoptClassic(c, 7);
      checkPiece(X, c, false);
      Item m = gear::makeGearC(r, k, 0, 20, (Rarity)rar, nullptr);
      for (ItemAffix& a : m.affix) a = ItemAffix();
      m.unique = Unique::None;
      gear::finishGear(m, nullptr);
      checkPiece(X, m, false);
      pieces += 2;
    }
  // (M8b fix) a companion's kit (PartyOps::kit: gear::CompanionRolls, a Mythic of a kind without a companion-working
  // Mythic power made a Legendary): every affix and power works for a companion, and the piece still passes lootValid
  int comp = 0;
  {
    const ItemKind kitKinds[] = {ItemKind::Weapon, ItemKind::Shield, ItemKind::Bow, ItemKind::Staff, ItemKind::Armor, ItemKind::Helmet, ItemKind::Cloak, ItemKind::Boots};
    for (int lv = 1; lv <= 60; lv += 3)
      for (int rar = 1; rar < RARITIES; rar++)
        for (ItemKind k : kitKinds) {
          gear::CompanionRolls only;
          Rarity rr = (Rarity)rar;
          if (rr == Rarity::Mythic && gear::mythicPowersFor(k).empty()) rr = Rarity::Legendary;
          const Item it = gear::makeGear(r, k, 0, lv, rr, 0, Mat::None);
          comp++;
          std::string why;
          if (!gear::lootValid(it, &why)) X.fail("companion kit: " + why);
          for (const ItemAffix& a : it.affix)
            if (a.kind != Affix::None && !gear::affixWorksForCompanion(a.kind)) X.fail(std::string("companion kit: a hero-only affix ") + affixInfo(a.kind).name + " on " + it.name);
          if (it.unique != Unique::None && !gear::uniqueWorksForCompanion(it.unique)) X.fail("companion kit: a hero-only power on " + it.name);
        }
  }
  printf("loot (b): %d companion kit pieces: every affix and power works for a companion\n", comp);
  printf("loot (b): %d pieces from every source, kind, rarity and culture (%d above Common): all checked by lootValid\n", pieces, above);
  if (pieces < 20000) X.fail("only " + std::to_string(pieces) + " pieces checked (20,000 wanted)");
  return pieces;
}

// ---------------------------------------------------------------- (c) power curves
float meanScore(Rng& r, ItemKind k, int sub, int il, Rarity rar, Mat mat, int L, int n, float* best = nullptr, float* worst = nullptr) {
  double sum = 0;
  float hi = 0, lo = 1e9f;
  for (int i = 0; i < n; i++) {
    const Item it = gear::makeGearC(r, k, sub, il, rar, nullptr, mat);
    const float s = gear::powerScore(it, L);
    sum += s;
    hi = std::max(hi, s);
    lo = std::min(lo, s);
  }
  if (best) *best = hi;
  if (worst) *worst = lo;
  return (float)(sum / n);
}
void powerCurves(Ctx& X) {
  Rng r(0xC0FFEEu);
  printf("loot (c): power curves (mean gear::powerScore of 300 heartland pieces at player level = item level)\n");
  printf("  %-12s %8s %8s %8s %8s %8s %8s   (body armour: C .. M)\n", "item level", "COMMON", "UNCOMMON", "RARE", "EPIC", "LEGEND", "MYTHIC");
  for (int il : {1, 2, 5, 10, 15, 20, 30, 40, 50, 60}) {
    for (ItemKind k : {ItemKind::Weapon, ItemKind::Armor}) {
      float s[RARITIES];
      for (int rar = 0; rar < RARITIES; rar++) s[rar] = meanScore(r, k, (int)WeaponType::Sword, il, (Rarity)rar, Mat::None, std::min(il, gear::LEVEL_CAP), 300);
      if (k == ItemKind::Weapon) {
        printf("  sword %-6d", il);
        for (int rar = 0; rar < RARITIES; rar++) printf(" %8.1f", s[rar]);
      } else {
        printf("   |");
        for (int rar = 0; rar < RARITIES; rar++) printf(" %5.1f", s[rar]);
        printf("\n");
      }
      for (int rar = 1; rar < RARITIES; rar++)
        if (s[rar] < 1.10f * s[rar - 1])
          X.fail(std::string(k == ItemKind::Weapon ? "sword" : "armour") + " item level " + std::to_string(il) + ": " + rarityName((Rarity)rar) + " only " +
                 std::to_string(100.0f * (s[rar] / s[rar - 1] - 1.0f)) + " % over " + rarityName((Rarity)(rar - 1)) + " (10 % wanted)");
    }
  }
  // the owner's feel (15.27.3), at player level 10
  const int L = 10;
  float bestR = 0;
  const float iron = meanScore(r, ItemKind::Weapon, (int)WeaponType::Sword, 10, Rarity::Common, Mat::Iron, L, 50);
  const float u = meanScore(r, ItemKind::Weapon, (int)WeaponType::Sword, 2, Rarity::Uncommon, Mat::Bronze, L, 600);
  const float rr = meanScore(r, ItemKind::Weapon, (int)WeaponType::Sword, 2, Rarity::Rare, Mat::Bronze, L, 600, &bestR);
  const float e = meanScore(r, ItemKind::Weapon, (int)WeaponType::Sword, 2, Rarity::Epic, Mat::Bronze, L, 600);
  const float cb = meanScore(r, ItemKind::Weapon, (int)WeaponType::Sword, 2, Rarity::Common, Mat::Bronze, L, 50);
  printf("  iron / bronze (player level 10): level-10 COMMON IRON sword %.2f | level-2 bronze: COMMON %.2f (%+.0f%%)  UNCOMMON %.2f (%+.0f%%)  RARE %.2f (%+.0f%%, best roll %+.0f%%)  EPIC %.2f (%+.0f%%)\n",
         iron, cb, 100 * (cb / iron - 1), u, 100 * (u / iron - 1), rr, 100 * (rr / iron - 1), 100 * (bestR / iron - 1), e, 100 * (e / iron - 1));
  if (rr < 0.85f * iron || rr > 1.15f * iron) X.fail("iron / bronze: the level-2 Rare bronze sword is " + std::to_string(100 * (rr / iron - 1)) + " % off the level-10 Common iron one (+-15 %)");
  if (u > iron || u < 0.75f * iron) X.fail("iron / bronze: the level-2 Uncommon bronze sword is " + std::to_string(100 * (u / iron - 1)) + " % off (0 to -25 % wanted)");
  if (e <= iron) X.fail("iron / bronze: the level-2 Epic bronze sword does not beat the level-10 Common iron one");
  if (bestR <= iron) X.fail("iron / bronze: no level-2 Rare bronze roll beats the iron sword (its perks should be able to win)");
  // material tiers at one item level: leather < bronze < iron < steel < alloys
  {
    Item a; a.kind = ItemKind::Armor; a.ilvl = 6; a.rarity = Rarity::Common;
    float prev = 0;
    for (Mat m : {Mat::Leather, Mat::Bronze, Mat::Iron, Mat::Steel, Mat::Alloy}) {
      a.mat = m;
      const float v = gear::matMult(a, nullptr);
      if (v <= prev) X.fail(std::string("material ") + matName(m) + " not above the tier below it");
      prev = v;
    }
  }
}

// ---------------------------------------------------------------- (d) the conflict table
void conflicts(Ctx& X) {
  int pairs = 0;
  for (int a = 1; a < (int)Affix::COUNT; a++) {
    if (!gear::affixConflicts((Affix)a, (Affix)a)) X.fail(std::string("an affix may pair with itself: ") + affixInfo((Affix)a).name);
    if (gear::affixConflicts((Affix)a, Affix::None)) X.fail("an affix conflicts with none");
    for (int b = 1; b < (int)Affix::COUNT; b++) {
      if (gear::affixConflicts((Affix)a, (Affix)b) != gear::affixConflicts((Affix)b, (Affix)a)) X.fail("affixConflicts is not symmetric");
      pairs += a < b && gear::affixConflicts((Affix)a, (Affix)b);
    }
  }
  using A = Affix;
  const A must[][2] = {{A::FireDmg, A::FrostDmg}, {A::FireDmg, A::ShadowDmg}, {A::FrostDmg, A::ShadowDmg}, {A::FrostDmg, A::Burn}, {A::VsBeasts, A::VsUndead}, {A::VsUndead, A::VsPeople}};
  for (const auto& m : must) if (!gear::affixConflicts(m[0], m[1])) X.fail(std::string("no conflict between ") + affixInfo(m[0]).name + " and " + affixInfo(m[1]).name);
  printf("loot (d): %d contradictory pairs in the table (plus every affix with itself); no generated piece carries one\n", pairs);
}

// ---------------------------------------------------------------- (e) determinism
uint64_t fnv(uint64_t h, uint64_t v) {
  for (int i = 0; i < 8; i++) { h ^= (v >> (8 * i)) & 0xFFu; h *= 0x100000001B3ull; }
  return h;
}
bool sameItem(const Item& x, const Item& y) {
  bool s = x.kind == y.kind && x.sub == y.sub && x.rarity == y.rarity && x.ilvl == y.ilvl && x.name == y.name && x.power == y.power && x.value == y.value &&
           x.unique == y.unique && x.mat == y.mat && x.alloy == y.alloy && x.seed == y.seed && x.form == y.form && x.tint == y.tint;
  for (int i = 0; i < kItemAffixes; i++) s = s && x.affix[i].kind == y.affix[i].kind && x.affix[i].value == y.affix[i].value;
  return s;
}
// statements of a source file holding two or more RNG draws (comments and strings stripped)
int lintDraws(Ctx& X, const std::string& rel) {
#ifdef EMB_SOURCE_DIR
  const std::string path = std::string(EMB_SOURCE_DIR) + "/" + rel;
#else
  const std::string path = rel;
#endif
  std::ifstream f(path, std::ios::binary);
  if (!f) { X.fail("cannot read " + rel + " for the draw lint"); return 0; }
  std::stringstream ss;
  ss << f.rdbuf();
  const std::string src = ss.str();
  std::string code;
  code.reserve(src.size());
  for (size_t i = 0; i < src.size(); i++) {
    if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/') { while (i < src.size() && src[i] != '\n') i++; code += '\n'; continue; }
    if (src[i] == '"') { i++; while (i < src.size() && src[i] != '"') { if (src[i] == '\\') i++; i++; } code += "\"\""; continue; }
    code += src[i];
  }
  static const char* draws[] = {"r.f()", "r.irange(", "r.next()", "r.range(", "rng.f()", "rng.irange(", "rng.next()", "rng.range("};
  int statements = 0, line = 1, startLine = 1;
  std::string st;
  for (char ch : code) {
    if (ch == '\n') line++;
    if (ch == ';' || ch == '{' || ch == '}') {
      int n = 0;
      for (const char* d : draws) {
        const std::string D = d;
        for (size_t p = st.find(D); p != std::string::npos; p = st.find(D, p + 1)) {
          const bool word = p == 0 || !(std::isalnum((unsigned char)st[p - 1]) || st[p - 1] == '_');
          n += word;
        }
      }
      if (n >= 2) X.fail(rel + ":" + std::to_string(startLine) + ": " + std::to_string(n) + " RNG draws in one statement (unsequenced across compilers)");
      statements++;
      st.clear();
      startLine = line;
      continue;
    }
    if (st.empty() && (ch == ' ' || ch == '\n' || ch == '\t' || ch == '\r')) { startLine = line; continue; }
    st += ch;
  }
  return statements;
}
// the golden stream: rarity, item level, kind, material, form, seed, power, affix count, every affix's kind and value,
// the unique power and the price; MSVC's value, the clang build in CI must match it
constexpr uint64_t kGolden = 0xd3668a99c9a33407ull;   // MSVC /fp:precise (gear.cpp, items.cpp: CMakeLists), 2026-10-12 (M8b fixer r1: + affix kinds, values, the power, the price); re-record when the ladder or makers change
void determinism(Ctx& X) {
  int checked = 0;
  for (uint32_t sd = 1; sd <= 300; sd++) {
    const Case& c = kCases[sd % (sizeof kCases / sizeof kCases[0])];
    const gear::DropSource s = srcOf(c, 1 + (int)(sd % 50), 1 + (int)(sd * 7 % 60));
    Rng a(sd), b(sd);
    const Item x = gear::rollDropC(a, s, nullptr), y = gear::rollDropC(b, s, nullptr);
    if (!sameItem(x, y)) X.fail("the same seed gave two different items: " + x.name + " / " + y.name);
    Rng a2(sd), b2(sd);
    if (!sameItem(makeArmor(a2, (int)sd % 40 + 1, ItemKind::Helmet), makeArmor(b2, (int)sd % 40 + 1, ItemKind::Helmet))) X.fail("makeArmor is not deterministic");
    Rng a3(sd), b3(sd);
    if (!sameItem(gear::makeGearC(a3, ItemKind::Ring, 0, 30, (Rarity)(sd % 6), nullptr), gear::makeGearC(b3, ItemKind::Ring, 0, 30, (Rarity)(sd % 6), nullptr)))
      X.fail("makeGearC is not deterministic");
    checked += 3;
  }
  uint64_t h = 0xCBF29CE484222325ull;
  Rng r(0x601DE7u);
  for (int i = 0; i < 4000; i++) {
    const Case& c = kCases[(size_t)i % (sizeof kCases / sizeof kCases[0])];
    const gear::DropSource s = srcOf(c, 1 + i % 50, 1 + (i * 13) % 60);
    const Item it = gear::rollDropC(r, s, nullptr);
    h = fnv(h, (uint64_t)it.kind | (uint64_t)it.sub << 8 | (uint64_t)it.rarity << 16 | (uint64_t)it.ilvl << 24 | (uint64_t)it.mat << 32 | (uint64_t)it.form << 40 |
                   (uint64_t)it.alloy << 48);
    h = fnv(h, (uint64_t)it.seed | (uint64_t)(uint16_t)it.power << 32 | (uint64_t)it.affixCount() << 48 | (uint64_t)it.count << 52);
    // the affix rolls (float quantiles, lround, std::pow) and the power: a clang / MSVC split there shows in CI too
    for (const ItemAffix& a : it.affix) h = fnv(h, (uint64_t)a.kind | (uint64_t)(uint16_t)a.value << 8);
    h = fnv(h, (uint64_t)it.unique | (uint64_t)(uint32_t)it.value << 8);
  }
  const int st = lintDraws(X, "rpg/sim/gear.cpp") + lintDraws(X, "rpg/sim/items.cpp");
  printf("loot (e): %d same-seed pairs identical; golden stream %016llx%s; %d statements of gear.cpp / items.cpp linted (no two draws in one)\n", checked,
         (unsigned long long)h, kGolden ? (h == kGolden ? " (matches MSVC)" : " (MSVC differs!)") : "", st);
  if (kGolden && h != kGolden) X.fail("the golden loot stream differs from MSVC's (an unsequenced draw or FMA?)");
}

int lootCmd(int argc, char** argv) {
  (void)argc; (void)argv;
  Ctx X;
  ladder(X);
  validity(X);
  powerCurves(X);
  conflicts(X);
  determinism(X);
  printf("loot: %d failure(s)\n", X.bad);
  return X.bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--loot", "M8b: the loot gate: the ladder per level band and source, every non-Common piece valid, power curves and the iron / bronze feel, no conflicting affixes, determinism", lootCmd);
