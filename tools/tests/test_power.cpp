// rpg_test --power-curve / --loot-audit: the M6 Steel numbers (VISION_PLAN 7.1 - 7.5, 13 M6 verification). NUMBERS lane.
//   --power-curve   the 7.4 table: levels {1, 5, 10, 20, 30, 40, 50} x danger {L-10, L, L+5, L+10, L+20} for a hero in
//                   full matching gear (item level L, common): hits to kill a trash beast (a wolf) and a brute (a bear),
//                   the time a pack of three trash beasts takes to kill a hero who never dodges, the XP a kill is worth.
//                   FAIL when same-band trash (D = L) falls outside 3-5 hits or same-band death time outside 6-20 s.
//                   Then the pacing model behind gear::xpForNext (the 1.4 hours table).
//   --loot-audit    10,000 gear drops per band: rarity within +-2 points of 55 / 28 / 12 / 4 / 1 %, no legendary below
//                   D 10 (every rank), every affix inside its eff range and allowed on its kind, the item level and the
//                   material by band; and 200+ real shops (Game::shopStock via craft::stockOf, with the hero at level 50):
//                   nothing above the settlement's band + 2, nothing above Rare (Epic only in an honouring capital), never
//                   a legendary.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/craft.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/gear.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

// ---- the model hero and foe (the same rules game.cpp applies)
struct Cell { int L, D; float hitsTrash, hitsBrute, deathSecs; int xp; };
float heroHit(int L, int D) {
  // a common sword of item level L at eff L, +1.5 % a level, against armour 1.5 D (mitigated by the hero's level), the gap
  const float w = gear::weaponBase(WeaponType::Sword) * gear::statScale(L) * gear::playerDamageMul(L);
  return w * (1.0f - gear::mitigation(gear::enemyArmour(D), L)) * gear::gapPlayerDamage(L, D);
}
float heroHp(int L) { return 100.0f + 12.0f * (float)(L - 1) + gear::levelHp(L); }   // every level-up into health
float heroAr(int L) {
  float a = 0;
  for (ItemKind k : {ItemKind::Armor, ItemKind::Helmet, ItemKind::Shield, ItemKind::Gloves, ItemKind::Boots, ItemKind::Cloak})
    a += gear::armourBase(k) * gear::statScale(L);
  return a;
}
Cell cell(int L, int D) {
  Cell c;
  c.L = L; c.D = D;
  const gsim::MStat& wolf = gsim::mstat(art::Monster::Wolf), &bear = gsim::mstat(art::Monster::Bear);
  const float hit = heroHit(L, D);
  c.hitsTrash = wolf.hp * gear::enemyHpMul(D) / hit;
  c.hitsBrute = bear.hp * gear::enemyHpMul(D) / hit;
  // three wolves, one bite each every 2 s (wind-up, strike, the pack's cooldown), no dodging, no potions
  const float bite = wolf.dmg * gear::enemyDamageMul(D) * (1.0f - gear::mitigation(heroAr(L), D)) * gear::gapEnemyDamage(L, D);
  c.deathSecs = heroHp(L) / (3.0f * bite / 2.0f);
  c.xp = gear::killXp(wolf.xp, D, L);
  return c;
}

int powerCurve(int argc, char** argv) {
  (void)argc; (void)argv;
  int bad = 0;
  const int Ls[] = {1, 5, 10, 20, 30, 40, 50};
  const int dD[] = {-10, 0, 5, 10, 20};
  printf("power curve (VISION_PLAN 7.4): a hero in full matching gear (item level L) against danger D\n");
  printf("  cell: hits to kill trash (wolf) / brute (bear), seconds a pack of 3 trash takes to kill a hero who never dodges, XP a trash kill gives\n");
  printf("  %-6s", "L \\ D");
  for (int d : dD) printf(" | %-27s", (d == 0 ? std::string("D = L") : "D = L" + std::string(d > 0 ? "+" : "") + std::to_string(d)).c_str());
  printf("\n");
  for (int L : Ls) {
    printf("  %-6d", L);
    for (int d : dD) {
      const int D = L + d;
      if (D < 1 || D > gear::MAX_D) { printf(" | %-27s", "-"); continue; }
      const Cell c = cell(L, D);
      char b[64];
      std::snprintf(b, sizeof b, "%4.1f/%5.1f hits %5.1fs %3dxp", c.hitsTrash, c.hitsBrute, c.deathSecs, c.xp);
      printf(" | %-27s", b);
      if (d == 0) {
        if (c.hitsTrash < 3.0f || c.hitsTrash > 5.0f) { out("FAIL: power-curve: level %d same-band trash takes %.1f hits (3-5)\n", L, c.hitsTrash); bad++; }
        if (c.deathSecs < 6.0f || c.deathSecs > 20.0f) { out("FAIL: power-curve: level %d same-band death in %.1f s (6-20)\n", L, c.deathSecs); bad++; }
      }
    }
    printf("\n");
  }
  // a zone 15 levels up is a wall (7.4: "about 4x harder"): hero damage x gap, enemy damage x gap
  {
    const int L = 20, D = 35;
    const Cell a = cell(L, L), b = cell(L, D);
    const float harder = (b.hitsTrash / a.hitsTrash) * (a.deathSecs / b.deathSecs);
    printf("  a zone 15 levels up (L 20, D 35): %.1fx the hits, %.1fx faster death: %.1fx harder (7.4: about 4x)\n", b.hitsTrash / a.hitsTrash,
           a.deathSecs / b.deathSecs, harder);
    if (harder < 2.5f || harder > 8.0f) { out("FAIL: power-curve: a zone 15 levels up is %.1fx harder (want about 4)\n", harder); bad++; }
  }
  // XP decay: farming the start zone at level 30
  printf("  XP decay: a level-1 wolf at hero level 30 gives %d of %d XP (7.4: 5 %%)\n", gear::killXp(12, 1, 30), gear::killXp(12, 1, 1));
  if (gear::killXp(100, 1, 30) != 5) { out("FAIL: power-curve: XP decay at level 30 in the start zone\n"); bad++; }
  // ---- the pacing model behind xpForNext: XP per minute at level L grows with what a same-band kill gives
  //      (1 + 0.12 (L - 1)); its rate is pinned by the 1.4 table's first row (level 5 at 0.5 h)
  {
    struct T { int L; float h; };
    const T tab[] = {{5, 0.5f}, {10, 3}, {15, 7}, {20, 12}, {25, 19}, {30, 28}, {40, 55}, {50, 100}};
    std::vector<double> cum(gear::LEVEL_CAP + 1, 0.0);
    for (int L = 1; L < gear::LEVEL_CAP; L++) cum[(size_t)L + 1] = cum[(size_t)L] + gear::xpForNext(L) / (1.0 + 0.12 * (L - 1));
    const double rate = cum[5] / 30.0;   // base XP a minute
    printf("  pacing (xpForNext a 120, b 100, c 24; level 5 pinned at 0.5 h):");
    double err = 0;
    for (const T& t : tab) {
      const double h = cum[(size_t)t.L] / rate / 60.0;
      printf("  L%d %.1fh (%gh)", t.L, h, (double)t.h);
      if (t.L > 5) err += std::fabs(h - t.h) / t.h;
    }
    err /= 7;
    printf("   mean error %.0f%%\n", err * 100);
    if (err > 0.25) { out("FAIL: power-curve: the XP curve misses the 1.4 hours table by %.0f %% on average\n", err * 100); bad++; }
    printf("  xpForNext: L1 %d  L2 %d  L5 %d  L10 %d  L20 %d  L30 %d  L49 %d\n", gear::xpForNext(1), gear::xpForNext(2), gear::xpForNext(5),
           gear::xpForNext(10), gear::xpForNext(20), gear::xpForNext(30), gear::xpForNext(49));
  }
  printf("power-curve: %d failure(s)\n", bad);
  return bad ? 1 : 0;
}

// ---- the loot audit
int lootAudit(int argc, char** argv) {
  (void)argc; (void)argv;
  int bad = 0;
  auto fail = [&](const std::string& m) { if (bad < 40) out("FAIL: loot-audit: %s\n", m.c_str()); bad++; };
  // a culture with alloys for the exotic tier (a family of each archetype in turn)
  std::vector<cult::Culture> cultures;
  for (int a = 0; a < (int)cult::Archetype::COUNT; a++) {
    cultures.push_back(cult::Atlas::make((cult::Archetype)a, 4100u + (uint32_t)a));
    cultures.back().id = cult::familyId(a, 3);
  }
  const float want[5] = {55, 28, 12, 4, 1};
  Rng r(0xA0D17u);
  printf("loot audit: 10,000 gear drops per band (champion sources; rarity %% common / uncommon / rare / epic / legendary)\n");
  for (int band = 1; band <= gear::BANDS; band++) {
    int n = 0, cnt[5] = {}, drops = 0, affixes = 0;
    while (n < 10000) {
      const int D = gear::bandLo(band) + (drops % (gear::bandHi(band) - gear::bandLo(band) + 1));
      const cult::Culture* c = (drops % 3) ? &cultures[(size_t)(drops % cultures.size())] : nullptr;
      gear::DropSource s;
      s.D = D; s.rank = 2; s.culture = c ? c->id : 0;
      drops++;
      const Item it = gear::rollDropC(r, s, c);
      if (!itemEquippable(it.kind)) continue;
      n++;
      // jewellery is never common (a ring is worth its stone): count the table on the rest
      const bool jewel = it.kind == ItemKind::Ring || it.kind == ItemKind::Amulet;
      if (!jewel) cnt[(int)it.rarity]++;
      if (it.ilvl != gear::dropIlvl(s)) fail("item level " + std::to_string(it.ilvl) + " != the source's D " + std::to_string(D));
      if (it.rarity == Rarity::Legendary && D < 10) fail("a legendary below D 10");
      if (it.rarity == Rarity::Legendary && !gear::uniqueImplemented(it.unique)) fail("a legendary without a working unique power: " + it.name);
      if (it.affixCount() != gear::affixSlots(it.rarity)) fail("affix count " + std::to_string(it.affixCount()) + " for rarity " + std::to_string((int)it.rarity) + ": " + it.name);
      for (const ItemAffix& a : it.affix) {
        if (a.kind == Affix::None) continue;
        affixes++;
        int lo, hi;
        gear::affixRange(a.kind, it.ilvl, lo, hi);
        if (a.value < lo || a.value > hi) fail(std::string("affix ") + affixInfo(a.kind).name + " " + std::to_string(a.value) + " outside " + std::to_string(lo) + ".." + std::to_string(hi));
        if (!gear::affixAllowed(a.kind, it.kind)) fail(std::string("affix ") + affixInfo(a.kind).name + " on a kind that may not carry it");
      }
      // the material by band: the universal tiers first, a culture's alloys from band 4
      const bool metal = it.kind == ItemKind::Weapon || it.kind == ItemKind::Armor || it.kind == ItemKind::Helmet || it.kind == ItemKind::Shield ||
                         it.kind == ItemKind::Gloves || it.kind == ItemKind::Boots;
      if (metal) {
        const bool okMat = band == 1 ? (it.mat == Mat::Leather || it.mat == Mat::Bronze)
                           : band == 2 ? it.mat == Mat::Iron
                           : band == 3 ? it.mat == Mat::Steel
                           : (c && !c->arms.alloys.empty() ? it.mat == Mat::Alloy : it.mat == Mat::Steel);
        if (!okMat) fail(std::string("band ") + std::to_string(band) + " piece in " + matName(it.mat) + ": " + it.name);
        if (it.kind != ItemKind::Armor && it.kind != ItemKind::Helmet && it.kind != ItemKind::Gloves && it.kind != ItemKind::Boots && it.kind != ItemKind::Shield &&
            it.mat == Mat::Leather) fail("a leather weapon");
      }
      if (it.name.empty() || it.power < 0 || it.value <= 0) fail("an unnamed or valueless item");
    }
    int tot = 0;
    for (int k = 0; k < 5; k++) tot += cnt[k];
    printf("  band %d (D %d-%d): ", band, gear::bandLo(band), gear::bandHi(band));
    for (int k = 0; k < 5; k++) {
      const float pct = 100.0f * cnt[k] / std::max(1, tot);
      printf("%5.1f ", pct);
      // band 1 cannot give a legendary (D < 10): its 1 % rolls epic
      const float w = band == 1 ? (k == 3 ? 5.0f : k == 4 ? 0.0f : want[k]) : want[k];
      if (std::fabs(pct - w) > 2.0f) fail("band " + std::to_string(band) + " rarity " + std::to_string(k) + " at " + std::to_string(pct) + " %");
    }
    printf("  (%d drops, %d affixes)\n", drops, affixes);
  }
  // no legendary below D 10 from any source, and never from a plain foe
  for (int rank = 0; rank <= 5; rank++)
    for (int D = 1; D <= 60; D++) {
      gear::DropSource s;
      s.D = D; s.rank = (uint8_t)rank;
      for (int i = 0; i < 300; i++) {
        const Item it = gear::rollDropC(r, s, nullptr);
        if (it.rarity == Rarity::Legendary && (D < 10 || rank < 2)) { fail("a legendary from rank " + std::to_string(rank) + " at D " + std::to_string(D)); break; }
      }
    }
  // ---- the shops: real ones (Game::shopStock), the hero at level 50 so a leak of the player's level shows at once
  {
    int shops = 0, items = 0, worst = 0;
    for (uint64_t seed = 1; seed <= 4 && shops < 240; seed++) {
      Game g(seed);
      g.newEndlessGame(seed);
      g.mode = Mode::Play;
      g.plLevel = 50;
      // keepers of every trading role (Game::shopStock reads the role, the site and the keeper's identity)
      const Role roles[] = {Role::Smith, Role::Merchant, Role::Mage, Role::Priest, Role::Hunter};
      // each keeper, moved through every settlement the window holds and over several restocks
      for (int si = 0; si < (int)g.world.sites.size() && shops < 240; si++) {
        if (!g.world.sites[(size_t)si].settlement()) continue;
        for (int day = 1; day <= 9 && shops < 240; day += 2) {
          g.day = day;
          for (int k = 0; k < 5; k++) {
            Actor a;
            a.npc = true; a.human = true; a.role = roles[k];
            a.site = si; a.bldg = -1; a.slot = 4000 + k;
            const int D = std::max(1, g.world.sites[(size_t)si].level);
            const std::vector<Item> st = craft::stockOf(g, a);
            shops++;
            for (const Item& it : st) {
              if (!itemEquippable(it.kind)) continue;
              items++;
              worst = std::max(worst, gear::bandOf(it.ilvl) - gear::bandOf(D));
              if (gear::bandOf(it.ilvl) > gear::bandOf(D) + 2) fail("a shop item above its settlement's band + 2: " + it.name);
              if (it.ilvl > D + gear::SHOP_BAND_SPREAD) fail("a shop item above D + 2 (the player's level leaked?): " + it.name + " ilvl " + std::to_string(it.ilvl) + " at D " + std::to_string(D));
              if (it.rarity == Rarity::Legendary) fail("a legendary for sale: " + it.name);
              if (it.rarity == Rarity::Epic && !g.world.sites[(size_t)si].capital) fail("an epic outside a capital: " + it.name);
            }
          }
        }
      }
    }
    printf("  shops: %d stocks (hero at level 50), %d gear items, worst band over the settlement's: %+d\n", shops, items, worst);
    if (shops < 200) fail("only " + std::to_string(shops) + " shops audited (200 wanted)");
  }
  printf("loot-audit: %d failure(s)\n", bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--power-curve", "M6: the 7.4 power table (hits to kill, death time, XP) and the XP pacing model; FAIL outside 3-5 hits / 6-20 s same-band", powerCurve);
RPG_TEST_CMD("--loot-audit", "M6: 10,000 drops per band (rarity, legendaries, affix ranges, materials) and 200 shops (band + 2, Rare cap)", lootAudit);
