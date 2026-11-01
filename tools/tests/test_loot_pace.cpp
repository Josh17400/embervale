// rpg_test --ladder-pace [--worlds A..B] [--report] [--gates] [--seed S] [--rolls N] [--top]: the gear ladder measured
// against its rules (moved here from test_folk_sim.cpp by P3 LOOT; P2c FOLK-SIM-C built the first table).
//
// The P2c table (PLAN "The gear ladder", proposal 3): at each pacing stage of VISION 1.4 the look tiers (gear::lookTier)
// a smith's and a merchant's shelf stock (LootOps::shopStock) and the drops roll (gear::rollDropC: a body, a chest, a
// dungeon boss, the finder at the stage's level), printed beside PLAN's "When each tier shows up". Its checks: LP1 a shop
// never stocks T6; LP2 never above T(band) with the Rare mark (Epic: one tier more); LP3 no T6 from an ordinary body
// below level 20.
//
// P3 LOOT (docs/FIX_BATCH_P3.md 8.1; docs/ARMOUR_SPAWN_RARITY.md section 9) adds the Batch B rows and the fail list, over
// worlds A..B (each a new endless game, so two- and three-alloy peoples both appear):
//   LP4  no rarer piece drawn plainer (lookTier by rarity, Common matching Uncommon allowed), people's and relic pieces
//   LP5  every alloy at its band (everyday 4, warriors' 5, masterwork 6-7), drops at its band, never weaker than the
//        band's step (matMult), never drawn below T4
//   LP6  no shelf or commission masterwork outside the two exemptions (a hidden people; a capital smith at trust 60)
//   LP7  relics only from band 4, only from old places, never with a culture id; never in a people's alloy
//   LP8  relics 8-20 % of the metal armour drops of the assumed explorer's hour (its weights printed)
//   LP9  no Legendary below 17 and no Mythic below 27 from any rolled source (the odds, exactly), outside a campaign
//        finale and the dragon's hoard (neither is rolled through the gate)
//   LP10 war spoils: soldiers about 10 %, captains about 50 % and Uncommon or better, at most 3 a siege, never over the
//        hero's level + 4
// --report prints every world's tables (the metal ranks per band, the relic hour, the spoils, the first level of each
// rarity); --gates ends with the fail list (one PASS / FAIL line each) and exits nonzero on a FAIL (it does anyway).
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/craft_hooks.h"
#include "rpg/sim/game.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/gear_ladder.h"
#include "rpg/sim/loot_ops.h"
#include "rpg/sim/mark_tags.h"
#include "rpg/sim/war_ops.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

std::string fmt(const char* f, ...) {
  char b[512];
  va_list ap;
  va_start(ap, f);
  std::vsnprintf(b, sizeof b, f, ap);
  va_end(ap);
  return b;
}

// one rule of the fail list: its failures over every world, and the first one seen
struct Rule {
  const char* id;
  const char* what;
  int bad = 0, seen = 0;
  std::string first;
  void fail(const std::string& m) { if (!bad++) first = m; }
};
enum { LP1, LP2, LP3, LP4, LP5, LP6, LP7, LP8, LP9, LP10, NRULES };
Rule g_rules[NRULES] = {
    {"LP1", "shops never T6"},
    {"LP2", "shop tier cap (T(band) + Rare, Epic one more)"},
    {"LP3", "no T6 from an ordinary body below level 20"},
    {"LP4", "no rarer piece drawn plainer"},
    {"LP5", "every alloy at its band and strength"},
    {"LP6", "no shelf or commission masterwork outside the two exemptions"},
    {"LP7", "relics only from band 4, only in old places, never with a culture id"},
    {"LP8", "relics 8-20 % of metal armour drops (the assumed hour)"},
    {"LP9", "no Legendary below 17 / Mythic below 27 outside finales and the hoard"},
    {"LP10", "war spoils: 10 % / 50 % Uncommon+, 3 a siege, item level <= hero + 4"},
};

bool wornKind(const Item& it) {
  return it.kind == ItemKind::Weapon || it.kind == ItemKind::Bow || it.kind == ItemKind::Staff || it.kind == ItemKind::Armor || it.kind == ItemKind::Helmet ||
         it.kind == ItemKind::Shield || it.kind == ItemKind::Gloves || it.kind == ItemKind::Boots || it.kind == ItemKind::Cloak;
}
bool metalArmour(const Item& it) { return gear::metalKind(it.kind) && it.kind != ItemKind::Weapon; }
const cult::Culture* makerOf(const World& w, const Item& it) { return it.culture && w.src ? &w.src->culture(it.culture) : nullptr; }
int bandMid(int band) { return (gear::bandLo(band) + gear::bandHi(band)) / 2; }

struct Ctx {
  Game& g;
  uint64_t seed;
  int rolls;
  bool top, report;
  int si = -1;                  // the town whose shelves and culture the P2c table reads
  const cult::Culture* cc = nullptr;
  std::vector<const cult::Culture*> peoples;   // every culture of the world's settlements (each once)
};

// ---------------------------------------------------------------- the P2c table (LP1-LP3)
void p2cTable(Ctx& X) {
  World& w = X.g.world;
  struct Stage { int level; float hours; int D; };
  const Stage stages[] = {{5, 0.5f, X.top ? 6 : 4}, {10, 3, X.top ? 12 : 9}, {15, 7, X.top ? 18 : 15}, {20, 12, X.top ? 22 : 19},
                          {25, 19, X.top ? 27 : 24}, {30, 28, X.top ? 32 : 29}, {40, 55, X.top ? 42 : 37}, {50, 100, X.top ? 60 : 50}};
  auto planTier = [](float h) { return h < 3 ? 1 : h < 10 ? 2 : h < 25 ? 3 : h < 60 ? 4 : 5; };
  struct Hist { int n = 0, t[7] = {}, rare = 0, flame = 0; void add(const gear::LookTier& lt) { n++; t[std::clamp<int>(lt.tier, 0, 6)]++; rare += lt.rare; flame += lt.flame; } };
  auto row = [](const Hist& h) {
    std::string s;
    for (int t = 1; t <= 6; t++) s += fmt(" %3d", h.n ? (int)std::lround(100.0 * h.t[t] / h.n) : 0);
    s += fmt("  r%3d", h.n ? (int)std::lround(100.0 * h.rare / h.n) : 0);
    return s;
  };
  auto mode = [](const Hist& h) { int b = 1; for (int t = 1; t <= 6; t++) if (h.t[t] > h.t[b]) b = t; return b; };
  if (X.report) {
    std::printf("ladder-pace (seed %llu, %d worn pieces a column; shelves of %s, a %s town): percent of pieces at T1..T6, r = the Rare mark\n",
                (unsigned long long)X.seed, X.rolls, w.sites[(size_t)X.si].name.c_str(), X.cc ? cult::archetypeName(X.cc->archetype) : "?");
    std::printf("  lvl  hours  D  | shops  T1  T2  T3  T4  T5  T6     | bodies T1  T2  T3  T4  T5  T6     | chests T1  T2  T3  T4  T5  T6     | bosses T1  T2  T3  T4  T5  T6     | PLAN worn  shop/body mode\n");
  }
  const int lv0 = w.sites[(size_t)X.si].level;
  for (const Stage& st : stages) {
    Hist shop, body, chest, boss;
    w.sites[(size_t)X.si].level = st.D;
    for (int day = 0, k = 0; shop.n < X.rolls && day < 20000; day += 2, k++) {
      X.g.day = day;
      Actor a;
      a.id = 98000 + (k & 7); a.human = true; a.npc = true; a.site = X.si; a.bldg = -1; a.slot = 6000 + (k & 7);
      a.role = (k & 1) ? Role::Merchant : Role::Smith;
      for (const Item& it : LootOps::shopStock(X.g, a)) {
        if (!wornKind(it) || shop.n >= X.rolls) continue;
        const gear::LookTier lt = gear::lookTier(it, makerOf(w, it));
        if (!lt.tier) continue;
        shop.add(lt);
        if (lt.tier >= 6) g_rules[LP1].fail(fmt("world %llu D %d: a T6 %s on a shelf", (unsigned long long)X.seed, st.D, it.name.c_str()));
        Item x = it;
        x.ilvl = (uint8_t)std::min(gear::MAX_D, st.D + gear::SHOP_BAND_SPREAD);
        x.rarity = Rarity::Rare;
        const gear::LookTier capT = gear::lookTier(x, makerOf(w, it));
        if (lt.tier > capT.tier) g_rules[LP2].fail(fmt("world %llu D %d: T%d over T%d (%s)", (unsigned long long)X.seed, st.D, lt.tier, capT.tier, it.name.c_str()));
      }
    }
    Rng r(hash32((uint32_t)st.D * 7919u ^ (uint32_t)X.seed));
    auto roll = [&](Hist& h, gear::LootSrc src, uint8_t rank, bool isChest) {
      for (int tries = 0; h.n < X.rolls && tries < X.rolls * 400; tries++) {
        gear::DropSource ds;
        ds.D = st.D; ds.rank = rank; ds.chest = isChest; ds.src = src; ds.playerLevel = st.level; ds.culture = X.cc ? X.cc->id : 0;
        const Item it = gear::rollDropC(r, ds, X.cc);
        if (!wornKind(it)) continue;
        const gear::LookTier lt = gear::lookTier(it, makerOf(w, it));
        if (!lt.tier) continue;
        h.add(lt);
        if (src == gear::LootSrc::Body && rank == 0 && st.level < 20 && lt.tier >= 6)
          g_rules[LP3].fail(fmt("world %llu level %d: a T6 %s from a plain body", (unsigned long long)X.seed, st.level, it.name.c_str()));
      }
    };
    roll(body, gear::LootSrc::Body, 0, false);
    roll(chest, gear::LootSrc::Chest, 0, true);
    roll(boss, gear::LootSrc::Boss, 2, false);
    if (X.report)
      std::printf("  %3d %6.1f %3d |      %s |      %s |      %s |      %s |   T%d      T%d/T%d\n", st.level, st.hours, st.D, row(shop).c_str(), row(body).c_str(),
                  row(chest).c_str(), row(boss).c_str(), planTier(st.hours), mode(shop), mode(body));
  }
  w.sites[(size_t)X.si].level = lv0;
  if (X.report)
    std::printf("  PLAN \"When each tier shows up\": T1 hours 0-3, T2 3-10, T3 10-25, T4 25-60, T5 60-100, T6 earned from level 20 (bosses,\n"
                "  hoards, finales; one piece from level 10, two at 25, three at 40 by attunement), never bought\n");
}

// ---------------------------------------------------------------- LP4: the look by rarity
void lookOrder(Ctx& X) {
  static const ItemKind kinds[] = {ItemKind::Weapon, ItemKind::Armor, ItemKind::Helmet, ItemKind::Shield, ItemKind::Gloves, ItemKind::Boots};
  // finer = a higher tier, then the Rare trim, then the flame. The Rare trim shows below T5 only (at T5 it waits for the
  // glyph, the Q3 marks D86 deferred: ARMOUR_SPAWN_RARITY section 4), so at T5 a Rare reads as its band's T5
  auto key = [](const gear::LookTier& t) { return (int)t.tier * 4 + (t.rare && t.tier < 5 ? 1 : 0) + (t.flame ? 2 : 0); };
  std::vector<const cult::Culture*> makers = X.peoples;
  makers.push_back(nullptr);   // the old makers (a relic from band 4; the maker-less trade metals below)
  for (const cult::Culture* c : makers)
    for (int band = 1; band <= gear::BANDS; band++)
      for (ItemKind k : kinds)
        for (int v = 0; v < 3; v++) {
          Rng r(hash32((uint32_t)band * 131u ^ (uint32_t)k * 977u ^ (uint32_t)v ^ (uint32_t)X.seed));
          const Item base = gear::makeGearC(r, k, v % 5, gear::bandLo(band) + v * 2, Rarity::Common, c);
          int prev = -1;
          for (int q = 0; q < RARITIES; q++) {
            Item it = base;
            it.rarity = (Rarity)q;
            const int kq = key(gear::lookTier(it, c));
            if (kq < prev)
              g_rules[LP4].fail(fmt("world %llu: %s band %d %s drawn plainer than %s", (unsigned long long)X.seed, base.name.c_str(), band,
                                    rarityName((Rarity)q), rarityName((Rarity)(q - 1))));
            prev = std::max(prev, kq);
          }
        }
}

// ---------------------------------------------------------------- LP5: the alloys at their bands
void alloyRanks(Ctx& X) {
  if (X.report) std::printf("  metal ranks (LP5): per people and band, the alloy the band gives, its rank and matMult (band step)\n");
  for (const cult::Culture* c : X.peoples) {
    if (!c || c->arms.alloys.empty()) continue;
    std::string line = fmt("    %-12s %d alloys:", c->name.c_str(), (int)c->arms.alloys.size());
    for (int band = 4; band <= gear::BANDS; band++) {
      const uint8_t want = gear::alloyForBand(c, band);
      const int D = bandMid(band);
      Rng r(hash32((uint32_t)band * 7u ^ (uint32_t)(c->id) ^ (uint32_t)X.seed));
      int seen = 0;
      float mm = 0;
      // the band's step: a heartland steel piece of the band (gear.cpp bandStep: GILDED 1.16 .. STARMETAL 1.28)
      Item stepIt;
      stepIt.kind = ItemKind::Armor; stepIt.mat = Mat::Steel; stepIt.ilvl = (uint8_t)D;
      const float step = gear::matMult(stepIt, nullptr);
      for (int i = 0; i < 300; i++) {
        gear::DropSource ds;
        ds.D = D; ds.playerLevel = D; ds.src = gear::LootSrc::Body; ds.rank = 1; ds.culture = c->id;
        const Item it = gear::rollDropC(r, ds, c);
        if (!gear::metalKind(it.kind)) continue;
        if (it.mat != Mat::Alloy) { g_rules[LP5].fail(fmt("world %llu %s band %d: a %s piece, not its alloy", (unsigned long long)X.seed, c->name.c_str(), band, it.name.c_str())); continue; }
        if (it.alloy != want) g_rules[LP5].fail(fmt("world %llu %s band %d: alloy %d (%s), want %d", (unsigned long long)X.seed, c->name.c_str(), band, it.alloy, it.name.c_str(), want));
        mm = gear::matMult(it, c);
        if (mm + 1e-5f < step)
          g_rules[LP5].fail(fmt("world %llu %s band %d: matMult %.3f under the band's step", (unsigned long long)X.seed, c->name.c_str(), band, mm));
        if (gear::lookTier(it, c).tier < 4) g_rules[LP5].fail(fmt("world %llu %s band %d: %s drawn below T4", (unsigned long long)X.seed, c->name.c_str(), band, it.name.c_str()));
        seen++;
      }
      if (!seen) g_rules[LP5].fail(fmt("world %llu %s band %d: its alloy never dropped", (unsigned long long)X.seed, c->name.c_str(), band));
      line += fmt("  b%d %s r%d x%.2f", band, want ? c->arms.alloys[(size_t)want - 1].name.c_str() : "-", want, mm);
    }
    if (X.report) std::printf("%s\n", line.c_str());
  }
}

// ---------------------------------------------------------------- LP6: shelves and commissions
void shelves(Ctx& X) {
  World& w = X.g.world;
  int shelfMw = 0, commMw = 0, exempt = 0, looked = 0;
  for (size_t i = 0; i < w.sites.size(); i++) {
    if (!w.sites[i].settlement()) continue;
    const cult::Culture* c0 = w.cultureOf((int)i);
    if (!c0 || !w.src) continue;
    const cult::Culture* c = &w.src->culture(c0->id);
    if (c->arms.alloys.size() < 3) continue;   // only a three-alloy people has a masterwork to hold back
    const int lv0 = w.sites[i].level;
    for (int D : {44, 50, 56}) {
      w.sites[i].level = D;
      for (int k = 0; k < 6; k++) {
        X.g.day = 2 * k;
        Actor a;
        a.id = 97000 + k; a.human = true; a.npc = true; a.site = (int)i; a.bldg = -1; a.slot = 5000 + k;
        a.role = (k & 1) ? Role::Merchant : Role::Smith;
        for (const Item& it : LootOps::shopStock(X.g, a)) {
          if (it.mat != Mat::Alloy) continue;
          looked++;
          if (it.alloy > gear::SHELF_ALLOY_CAP) {
            if (c->hidden) { exempt++; continue; }
            shelfMw++;
            g_rules[LP6].fail(fmt("world %llu %s D %d: a masterwork %s on a shelf", (unsigned long long)X.seed, w.sites[i].name.c_str(), D, it.name.c_str()));
          }
        }
        a.role = Role::Smith;
        const uint64_t key = X.g.npcKey(a);
        X.g.craft.trust[key] = 0;
        const Item cm = CraftHooks::commissionPiece(X.g, a);
        if (cm.mat == Mat::Alloy && cm.alloy > gear::SHELF_ALLOY_CAP && !c->hidden) {
          commMw++;
          g_rules[LP6].fail(fmt("world %llu %s D %d: a masterwork commission %s", (unsigned long long)X.seed, w.sites[i].name.c_str(), D, cm.name.c_str()));
        }
        if (w.sites[i].capital) {   // the exemption: a capital smith who trusts the hero makes his masterwork
          X.g.craft.trust[key] = (uint8_t)gear::COMMISSION_TRUST;
          const Item tm = CraftHooks::commissionPiece(X.g, a);
          if (tm.mat == Mat::Alloy && tm.alloy > gear::SHELF_ALLOY_CAP) exempt++;
        }
        X.g.craft.trust.erase(key);
      }
    }
    w.sites[i].level = lv0;
  }
  if (X.report) std::printf("  shelves (LP6): %d alloy pieces on band-6+ shelves of three-alloy peoples; masterworks: %d shelf, %d commission, %d exempt\n", looked, shelfMw, commMw, exempt);
}

// ---------------------------------------------------------------- LP7 / LP8: relics and the assumed hour
// The assumed explorer's hour at band 4+ (ARMOUR_SPAWN_RARITY section 3): 40 people killed (a body roll 35 % of the
// time), 15 undead (15 %; 5 of them in ruins), 6 plain chests (1.5 rolls), one dungeon (a ruin or shrine one time in
// three: 3 chests of 2 rolls, a hoard of 2, a boss), half an outdoor old-place cache (1.5 rolls), a tenth of a world boss
// (2 rolls) and 2 quests (half the time an item).
struct HourSrc { const char* name; gear::LootSrc src; uint8_t rank; int relicPct; float rolls; };
void relicHour(Ctx& X) {
  const float ruin = 1.0f / 3.0f;
  const HourSrc kHour[] = {
      {"people", gear::LootSrc::Body, 0, 0, 40 * 0.35f},
      {"undead", gear::LootSrc::Body, 0, 0, 10 * 0.15f},
      {"undead (ruin)", gear::LootSrc::Body, 0, gear::RELIC_HALF, 5 * 0.15f},
      {"plain chests", gear::LootSrc::Chest, 0, 0, 6 * 1.5f},
      {"cave chests", gear::LootSrc::Chest, 0, 0, (1 - ruin) * 3 * 2.0f},
      {"ruin caches", gear::LootSrc::Explore, 0, gear::RELIC_HALF, ruin * 3 * 2.0f},
      {"cave hoard", gear::LootSrc::Hoard, 4, 0, (1 - ruin) * 2.0f},
      {"ruin hoard", gear::LootSrc::Hoard, 4, gear::RELIC_HALF, ruin * 2.0f},
      {"cave boss", gear::LootSrc::Boss, 4, 0, (1 - ruin) * 1.0f},
      {"ruin boss", gear::LootSrc::Boss, 4, gear::RELIC_HALF, ruin * 1.0f},
      {"old-place cache", gear::LootSrc::Explore, 0, gear::RELIC_HALF, 0.5f * 1.5f},
      {"world boss", gear::LootSrc::WorldBoss, 5, gear::RELIC_ALL, 0.1f * 2.0f},
      {"quests", gear::LootSrc::Quest, 0, 0, 2 * 0.5f},
  };
  struct Stage { int level, D; };
  const Stage stages[] = {{5, 4}, {10, 9}, {15, 15}, {20, 19}, {25, 26}, {30, 30}, {40, 38}, {50, 50}};
  const int N = 1500;
  if (X.report) std::printf("  relic hour (LP8): per stage, metal armour drops an hour, relics among them, and relic weapons (world %llu, %s)\n",
                            (unsigned long long)X.seed, X.cc ? X.cc->name.c_str() : "?");
  for (const Stage& st : stages) {
    float arm = 0, relArm = 0, relWpn = 0;
    for (const HourSrc& h : kHour) {
      Rng r(hash32((uint32_t)st.D * 9176u ^ (uint32_t)h.src * 31u ^ (uint32_t)h.relicPct ^ (uint32_t)X.seed));
      int a = 0, ra = 0, rw = 0;
      for (int i = 0; i < N; i++) {
        gear::DropSource ds;
        ds.D = st.D; ds.playerLevel = st.level; ds.src = h.src; ds.rank = h.rank; ds.chest = h.src == gear::LootSrc::Chest;
        ds.culture = X.cc ? X.cc->id : 0; ds.relicPct = (uint8_t)h.relicPct;
        if (h.src == gear::LootSrc::Hoard || h.src == gear::LootSrc::Explore || h.src == gear::LootSrc::Boss) ds.boost = 1;
        if (h.src == gear::LootSrc::WorldBoss) ds.atLeast = Rarity::Epic;
        const Item it = gear::rollDropC(r, ds, X.cc);
        if (!gear::metalKind(it.kind)) continue;
        const int band = gear::bandOf(it.ilvl);
        const bool makerless = it.culture == 0;
        // LP7: a maker-less metal piece from a place that is not old, or below band 4; a relic in a people's alloy
        if (makerless && X.cc && (h.relicPct == 0 || band < gear::RELIC_BAND))
          g_rules[LP7].fail(fmt("world %llu level %d %s: a maker-less %s (band %d)", (unsigned long long)X.seed, st.level, h.name, it.name.c_str(), band));
        if (makerless && it.mat == Mat::Alloy) g_rules[LP7].fail(fmt("world %llu: a relic in a people's alloy (%s)", (unsigned long long)X.seed, it.name.c_str()));
        if (gear::isRelic(it) && it.culture) g_rules[LP7].fail(fmt("world %llu: a relic with a culture id (%s)", (unsigned long long)X.seed, it.name.c_str()));
        if (it.kind == ItemKind::Weapon) { rw += gear::isRelic(it); continue; }
        a++;
        ra += gear::isRelic(it);
      }
      arm += h.rolls * (float)a / (float)N;
      relArm += h.rolls * (float)ra / (float)N;
      relWpn += h.rolls * (float)rw / (float)N;
    }
    const float share = arm > 0 ? 100.0f * relArm / arm : 0;
    const bool b4 = gear::bandOf(st.D) >= gear::RELIC_BAND;
    if (b4 && (share < 8.0f || share > 20.0f))
      g_rules[LP8].fail(fmt("world %llu level %d D %d: relics %.1f %% of metal armour drops", (unsigned long long)X.seed, st.level, st.D, share));
    if (X.report)
      std::printf("    level %2d D %2d: %.2f metal armour drops, %.2f relics (%.1f %%), %.2f relic weapons%s\n", st.level, st.D, arm, relArm, share, relWpn,
                  b4 ? "" : " (below band 4: none by rule)");
  }
  if (X.report) {
    std::printf("    the hour's weights (rolls):");
    for (const HourSrc& h : kHour) std::printf(" %s %.2f;", h.name, h.rolls);
    std::printf("\n");
  }
}

// ---------------------------------------------------------------- LP9: the luck gate, by the odds
void luckGate(Ctx& X) {
  int firstL[RARITIES], cases = 0;
  for (int& f : firstL) f = 99;
  for (int s = 0; s < (int)gear::LootSrc::COUNT; s++) {
    const gear::LootSrc src = (gear::LootSrc)s;
    if (src == gear::LootSrc::CampaignFinale || src == gear::LootSrc::Debug) continue;
    for (int L = 1; L <= 32; L++)
      for (int dD : {-6, 0, 3, 10})
        for (int rank = 0; rank <= 5; rank++)
          for (int boost = -1; boost <= 3; boost++) {
            gear::DropSource ds;
            ds.src = src; ds.playerLevel = L; ds.D = std::clamp(L + dD, 1, gear::MAX_D); ds.rank = (uint8_t)rank; ds.boost = (int8_t)boost;
            if (src == gear::LootSrc::WorldBoss) ds.atLeast = Rarity::Legendary;
            float pct[RARITIES];
            gear::rarityOdds(ds, pct);
            cases++;
            // a shop's gate reads its D (its "finder" is the settlement), so its level is D
            const int G = (src == gear::LootSrc::Shop || src == gear::LootSrc::Merchant) ? ds.D : L;
            for (int q = 0; q < RARITIES; q++) if (pct[q] > 0) firstL[q] = std::min(firstL[q], G);
            if (pct[(int)Rarity::Legendary] > 0 && G < gear::LUCK_LEGENDARY_FROM)
              g_rules[LP9].fail(fmt("%s at level %d (D %d, rank %d, boost %d): Legendary %.2f %%", gear::lootSrcName(src), G, ds.D, rank, boost, pct[4]));
            if (pct[(int)Rarity::Mythic] > 0 && G < gear::LUCK_MYTHIC_FROM)
              g_rules[LP9].fail(fmt("%s at level %d (D %d, rank %d, boost %d): Mythic %.2f %%", gear::lootSrcName(src), G, ds.D, rank, boost, pct[5]));
          }
  }
  if (X.report)
    std::printf("  luck gate (LP9): %d source cases; first level each rarity can roll: U %d, R %d, E %d, L %d, M %d (campaign finales and the hoard aside)\n",
                cases, firstL[1], firstL[2], firstL[3], firstL[4], firstL[5]);
}

// ---------------------------------------------------------------- LP10: war spoils
void warSpoils(Ctx& X) {
  Game& g = X.g;
  World& w = g.world;
  ew::Gid realm = 0;
  for (const Kingdom& k : w.kingdoms) if (k.id) { realm = k.id; break; }
  if (!realm) return;
  const Vec2 at = g.pl().p;
  const int L0 = g.plLevel;
  int soldiers = 0, sDrops = 0, captains = 0, cDrops = 0, rar[RARITIES] = {};
  for (int L : {5, 15, 25, 40}) {
    g.plLevel = L;
    for (int i = 0; i < 600; i++) {
      Actor v;
      v.id = 500000 + L * 1000 + i; v.human = true; v.npc = true; v.realm = realm; v.p = at; v.level = 10;
      v.role = (i % 4 == 0) ? Role::Captain : Role::Soldier;
      const uint32_t siege = 0x7000000u + (uint32_t)(L * 1000 + i);   // a siege each: the cap never bites here
      const size_t before = g.pickups.size();
      const bool got = WarOps::spoils(g, v, siege);
      if (v.role == Role::Captain) { captains++; cDrops += got; } else { soldiers++; sDrops += got; }
      if (!got) continue;
      if (g.pickups.size() != before + 1) { g_rules[LP10].fail("a spoils drop laid other than one piece"); continue; }
      const Item& it = g.pickups.back().item;
      rar[(int)it.rarity]++;
      if (!itemEquippable(it.kind)) g_rules[LP10].fail(fmt("spoils gave a non-gear item (%s)", it.name.c_str()));
      if (it.ilvl > L + 4) g_rules[LP10].fail(fmt("spoils item level %d over hero level %d + 4", it.ilvl, L));
      if (v.role == Role::Captain && it.rarity < Rarity::Uncommon) g_rules[LP10].fail("a captain's spoils below Uncommon");
      std::string why;
      if (!gear::lootValid(it, &why)) g_rules[LP10].fail("spoils fail lootValid: " + why);
    }
  }
  // the cap: 200 captains of one siege give exactly 3
  int one = 0;
  for (int i = 0; i < 200; i++) {
    Actor v;
    v.id = 900000 + i; v.human = true; v.npc = true; v.realm = realm; v.p = at; v.role = Role::Captain;
    one += WarOps::spoils(g, v, 0x7F00001u);
  }
  if (one != gsim::WAR_SPOILS_MAX) g_rules[LP10].fail(fmt("one siege gave %d spoils (cap %d)", one, gsim::WAR_SPOILS_MAX));
  const float sp = soldiers ? 100.0f * sDrops / soldiers : 0, cp = captains ? 100.0f * cDrops / captains : 0;
  if (sp < 6 || sp > 14) g_rules[LP10].fail(fmt("soldiers' spoils %.1f %% (10 %%)", sp));
  if (cp < 42 || cp > 58) g_rules[LP10].fail(fmt("captains' spoils %.1f %% (50 %%)", cp));
  if (X.report)
    std::printf("  war spoils (LP10): soldiers %.1f %% of %d, captains %.1f %% of %d; rarities C/U/R/E/L/M %d/%d/%d/%d/%d/%d; one siege's cap %d\n", sp, soldiers, cp,
                captains, rar[0], rar[1], rar[2], rar[3], rar[4], rar[5], one);
  g.pickups.clear();
  g.plLevel = L0;
}

bool parseRange(const char* s, uint64_t& a, uint64_t& b) {
  const char* d = std::strstr(s, "..");
  a = std::strtoull(s, nullptr, 10);
  b = d ? std::strtoull(d + 2, nullptr, 10) : a;
  return a >= 1 && b >= a;
}

int ladderPaceCmd(int argc, char** argv) {
  uint64_t a = 1, b = 1;
  int rolls = 1000;
  bool worlds = false, report = false, gates = false, top = false;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) a = b = std::strtoull(argv[++i], nullptr, 10);
    else if (!std::strcmp(argv[i], "--worlds") && i + 1 < argc) { worlds = parseRange(argv[++i], a, b); }
    else if (!std::strcmp(argv[i], "--rolls") && i + 1 < argc) rolls = std::max(50, std::atoi(argv[++i]));
    else if (!std::strcmp(argv[i], "--report")) report = true;
    else if (!std::strcmp(argv[i], "--gates")) gates = true;
    else if (!std::strcmp(argv[i], "--top")) top = true;
  }
  // one world without --worlds: the P2c table as it always printed, then the rules
  if (!worlds) report = true;
  for (Rule& r : g_rules) { r.bad = 0; r.first.clear(); }
  for (uint64_t seed = a; seed <= b; seed++) {
    Game g(seed);
    g.newEndlessGame(seed);
    World& w = g.world;
    Ctx X{g, seed, worlds && !report ? std::min(rolls, 400) : rolls, top, report};
    for (int pass = 0; pass < 3 && X.si < 0; pass++)   // a town or city, else a village; never a capital (its Honoured Epic shelf)
      for (size_t i = 0; i < w.sites.size() && X.si < 0; i++) {
        const SiteType t = w.sites[i].type;
        const bool kind = pass == 0 ? (t == SiteType::Town || t == SiteType::City) : pass == 1 ? t == SiteType::Village : true;
        if (kind && !w.sites[i].capital && w.cultureOf((int)i)) X.si = (int)i;
      }
    if (X.si < 0) { std::printf("FAIL ladder-pace: no town with a culture in seed %llu\n", (unsigned long long)seed); return 1; }
    X.cc = w.cultureOf(X.si);
    if (X.cc && w.src) X.cc = &w.src->culture(X.cc->id);
    std::set<uint64_t> ids;
    for (size_t i = 0; i < w.sites.size(); i++)
      if (const cult::Culture* c = w.cultureOf((int)i))
        if (w.src && ids.insert(c->id).second) X.peoples.push_back(&w.src->culture(c->id));
    if (report && worlds) std::printf("---- world %llu: %zu peoples\n", (unsigned long long)seed, X.peoples.size());
    const int day0 = g.day;
    p2cTable(X);
    lookOrder(X);
    alloyRanks(X);
    shelves(X);
    relicHour(X);
    if (seed == a) luckGate(X);   // the odds do not depend on the world
    warSpoils(X);
    g.day = day0;
  }
  int fails = 0, checks = 0;
  for (const Rule& r : g_rules) {
    checks++;
    if (r.bad) fails++;
    if (gates || r.bad || !worlds)
      std::printf("%s %s %s%s%s\n", r.bad ? "FAIL" : "PASS", r.id, r.what, r.bad ? fmt(": %d, first: ", r.bad).c_str() : "", r.first.c_str());
  }
  std::printf("ladder-pace: %d checks over worlds %llu..%llu, %d failed\n", checks, (unsigned long long)a, (unsigned long long)b, fails);
  return fails ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--ladder-pace",
             "the gear ladder against its rules (P2c table LP1-3; P3 LOOT LP4-10: looks by rarity, alloys in order, shelf caps, relics, the luck "
             "gate, war spoils) [--worlds A..B] [--report] [--gates] [--seed S] [--rolls N] [--top]",
             ladderPaceCmd);
