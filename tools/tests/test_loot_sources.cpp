// rpg_test --loot-sources [--seeds A..B]: every source that makes gear, audited in real games (VISION_PLAN 15.27.2:
// "Audit every source that makes items: drops, chests, shops, quest and saga rewards, the forge, companions' gear and
// merchants. Fix any that give rarity without affixes."). SOURCES lane.
// Per seed, a new endless game; with the hero at levels 3, 10, 20 and 35 the REAL code paths make loot:
//   bodies of every rank (0 plain .. 5 world boss) and every monster family, human and beast (Game::dropLoot), the
//   dragon Ashfang's hoard; chests in the open (wayside places: Explore) and in every cave, ruin and camp of the window
//   (Game::openChest: plain Chest, a ruin's cache Explore, the boss's Hoard; LootOps::chestSource names each); every
//   shop role in every settlement (Game::shopStock: Shop / Merchant), a capital's shelf at Honoured; a smith's
//   commission (the dialogue); quest completion of every QType (Game::completeQuest); saga stage rewards, saga finales
//   and campaign finales (story::rewardRoll, the reward the story engine pays) and `do loot` (story::storyLoot); the
//   forge (craft::benchMake on the start village's smithy); companions' kits (Game::partyRecruitStranger); the start
//   weapon (Game::giveFirstWeapon).
// Every equippable item must pass gear::lootValid, and each source keeps its own rules (shops capped, a world boss Epic
// or better, a campaign finale Epic / Legendary or better, quests Uncommon to Epic, a saga stage Rare at most, the
// forge under the gate). Prints, per LootSrc, the items audited and the rarity spread at each level. FAILs when a
// LootSrc the game uses was never exercised (Debug is the scripts' and tests' own, not a game source).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/craft.h"
#include "rpg/sim/game.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/loot_ops.h"
#include "rpg/sim/party.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

constexpr int NLV = 4;
const int kLevels[NLV] = {3, 10, 20, 35};
constexpr int SRC_START = (int)gear::LootSrc::COUNT;   // the start weapon: a row of its own
constexpr int SRC_PLAIN = SRC_START + 1;               // (BODY's plain foes alone, rank 0: the ordinary find)
constexpr int NROWS = SRC_PLAIN + 1;
const char* rowName(int row) { return row == SRC_START ? "START WEAPON" : row == SRC_PLAIN ? "  (plain foes)" : gear::lootSrcName((gear::LootSrc)row); }
// the floor a source keeps at a level: its guarantee, never past the gate's one step over (gear::rarityAllowedTop)
Rarity keptFloor(gear::LootSrc src, int L, Rarity want) {
  gear::DropSource ds;
  ds.src = src; ds.playerLevel = L; ds.D = L;
  return std::min(want, gear::rarityAllowedTop(ds));
}

struct Tally {
  int n = 0;
  int by[NLV][RARITIES] = {};
  int at[NLV] = {};
};

struct Audit {
  Tally T[NROWS];
  int bad = 0, items = 0, nonCommon = 0;
  int li = 0;   // the level index of the pass
  void fail(const std::string& m) {
    if (bad < 60) out("FAIL: loot-sources: %s\n", m.c_str());
    bad++;
  }
  // one piece from a source: valid, and its rarity counted
  void check(const Item& it, int row, const char* where) {
    if (!itemEquippable(it.kind)) return;
    items++;
    if (it.rarity > Rarity::Common) nonCommon++;
    Tally& t = T[row];
    t.n++;
    t.at[li]++;
    t.by[li][std::clamp((int)it.rarity, 0, RARITIES - 1)]++;
    std::string why;
    if (!gear::lootValid(it, &why)) fail(std::string(rowName(row)) + " (" + where + "): " + why);
  }
  void check(const Item& it, gear::LootSrc s, const char* where) { check(it, (int)s, where); }
  // a rarity rule of a source
  void rule(bool ok, const Item& it, gear::LootSrc s, const char* what) {
    if (!ok) fail(std::string(gear::lootSrcName(s)) + ": " + what + " (" + rarityName(it.rarity) + " " + it.name + " at level " + std::to_string(kLevels[li]) + ")");
  }
};

void clearPack(Game& g) {
  g.inv.clear();
  g.eqWeapon = g.eqBow = g.eqStaff = g.eqArmor = g.eqHelmet = g.eqShield = g.eqRing = g.eqAmulet = g.eqGloves = g.eqBoots = g.eqCloak = -1;
}
void setLevel(Game& g, int L) { g.plLevel = L; g.plXp = 0; }

bool talkTo(Game& g, int id) {
  static const Vec2 offs[] = {{0, 12}, {0, 20}, {10, 0}, {-10, 0}, {0, -10}};
  for (Vec2 o : offs) {
    int kk = -1;
    for (size_t j = 1; j < g.actors.size(); j++) if (g.actors[j].id == id) kk = (int)j;
    if (kk < 0) return false;
    g.mode = Mode::Play;
    g.pl().p = g.actors[(size_t)kk].p + o;
    Input in; in.interact = true;
    g.update(SIM_DT, in);
    if (g.mode == Mode::Dialogue && g.dlg.actor == id) return true;
  }
  return false;
}
int opt(const Game& g, const char* words) {
  for (size_t o = 0; o < g.dlg.opts.size(); o++) if (g.dlg.opts[o].label.find(words) != std::string::npos) return (int)o;
  return -1;
}

// ---- bodies: every rank, every monster family, human and beast; the dragon's hoard
void bodies(Game& g, Audit& A, uint64_t seed) {
  const Vec2 at = g.pl().p;
  for (int rank = 0; rank <= 5; rank++)
    for (int m = 0; m < (int)art::Monster::COUNT * (rank ? 1 : 8); m++)   // (plain foes 8 times over: the ordinary find)
      for (int h = 0; h < 2; h++) {
        const art::Monster mon = (art::Monster)(m % (int)art::Monster::COUNT);
        if (mon == art::Monster::Dragon) continue;   // Ashfang below
        Actor a;
        a.id = (int)(900000 + seed * 7 + rank * 10000 + m * 10 + h + A.li * 100000);
        a.human = h != 0; a.mon = mon; a.rank = (uint8_t)rank;
        a.level = std::clamp(kLevels[A.li] - 2 + (m + h) % 6, 1, gear::MAX_D);
        a.boss = rank >= 4; a.p = at;
        g.pickups.clear();
        LootOps::dropLoot(g, a);
        const gear::LootSrc s = rank >= 5 ? gear::LootSrc::WorldBoss : rank == 4 ? gear::LootSrc::Boss : gear::LootSrc::Body;
        for (const Pickup& p : g.pickups) {
          if (p.gold || !itemEquippable(p.item.kind)) continue;
          A.check(p.item, s, a.human ? "human body" : "beast body");
          if (rank == 0) A.check(p.item, SRC_PLAIN, "a plain foe");
          const int L = kLevels[A.li];
          if (rank >= 5) A.rule(p.item.rarity >= keptFloor(s, L, Rarity::Epic), p.item, s, "a world boss's hoard below Epic (or the gate's top)");
          if (rank == 4) A.rule(p.item.rarity >= Rarity::Uncommon, p.item, s, "a boss's drop below Uncommon");
          if (rank == 3) A.rule(p.item.rarity >= keptFloor(s, L, Rarity::Rare), p.item, s, "a named unique's drop below Rare (or the gate's top)");
        }
      }
  // Ashfang (the main quest's dragon: no world-boss rank): two named legendaries
  Actor d;
  d.id = (int)(990000 + seed + A.li); d.mon = art::Monster::Dragon; d.boss = true; d.level = std::max(12, kLevels[A.li]); d.p = at;
  g.pickups.clear();
  LootOps::dropLoot(g, d);
  int leg = 0;
  for (const Pickup& p : g.pickups) {
    if (p.gold || !itemEquippable(p.item.kind)) continue;
    A.check(p.item, gear::LootSrc::WorldBoss, "the dragon's hoard");
    if (p.item.rarity >= Rarity::Legendary) leg++;
  }
  if (leg < 2) A.fail("the dragon's hoard holds " + std::to_string(leg) + " legendaries (2 named ones)");
  g.pickups.clear();
}

// ---- the chests of the current map (each opened once per level: the prop is put back after)
void chestsHere(Game& g, Audit& A, int cap, int& found) {
  Map& m = g.map();
  std::vector<std::pair<int, int>> spots;
  for (int y = 0; y < m.h; y++)
    for (int x = 0; x < m.w; x++)
      if (m.propAt(x, y) == (int)art::Prop::Chest + 1) spots.push_back({x, y});
  if ((int)spots.size() > cap) spots.resize((size_t)cap);
  for (const auto& sp : spots) {
    found++;
    for (int li = 0; li < NLV; li++) {
      A.li = li;
      setLevel(g, kLevels[li]);
      g.map().setProp(sp.first, sp.second, art::Prop::Chest);
      const gear::DropSource ds = LootOps::chestSource(g, sp.first, sp.second);
      g.pickups.clear();
      LootOps::openChest(g, sp.first, sp.second);
      for (const Pickup& p : g.pickups) if (!p.gold) A.check(p.item, ds.src, g.inside ? "a dungeon chest" : "a chest in the open");
    }
    g.map().setProp(sp.first, sp.second, art::Prop::ChestOpen);
  }
  g.pickups.clear();
}

// ---- every shop role in every settlement; a capital's at Honoured
void shops(Game& g, Audit& A) {
  const Role roles[] = {Role::Smith, Role::Merchant, Role::Mage, Role::Priest, Role::Hunter, Role::Villager};
  for (int si = 0; si < (int)g.world.sites.size(); si++) {
    const Site& S = g.world.sites[(size_t)si];
    if (!S.settlement()) continue;
    const Kingdom* K = S.capital ? g.world.kingdomOf(si) : nullptr;
    for (int honoured = 0; honoured < (K ? 2 : 1); honoured++) {
      if (honoured) g.realm.addRep(K->id, gear::REP_HONOURED + 10 - g.realm.rep(K->id));
      for (int k = 0; k < 6; k++) {
        Actor a;
        a.npc = true; a.human = true; a.role = roles[k]; a.site = si; a.bldg = -1; a.slot = 4000 + k;
        const gear::LootSrc s = LootOps::shopSrc(roles[k]);
        for (const Item& it : LootOps::shopStock(g, a)) {
          if (!itemEquippable(it.kind)) continue;
          A.check(it, s, S.capital ? "a capital's shop" : "a shop");
          A.rule(it.rarity <= (honoured ? Rarity::Epic : Rarity::Rare), it, s, honoured ? "a capital at Honoured sells above Epic" : "a shop sells above Rare");
          A.rule(it.ilvl <= std::max(1, S.level) + gear::SHOP_BAND_SPREAD, it, s, "a shop stocks above its band");
        }
      }
    }
  }
}

// ---- quest completion of every QType
void quests(Game& g, Audit& A, uint64_t seed) {
  std::vector<int> towns, places;
  for (int si = 0; si < (int)g.world.sites.size(); si++) (g.world.sites[(size_t)si].settlement() ? towns : places).push_back(si);
  if (towns.empty()) return;
  const int L = kLevels[A.li];
  for (int t = 0; t < (int)QType::COUNT; t++) {
    if ((QType)t == QType::Story) continue;   // story quests close through the story engine (its rewards below)
    for (int k = 0; k < 24; k++) {
      Quest q;
      q.id = (int)(70000 + seed * 131 + t * 100 + k + A.li * 7919);
      q.type = (QType)t; q.state = QState::Complete;
      q.giverSite = towns[(size_t)(k % (int)towns.size())];
      q.target = places.empty() ? -1 : places[(size_t)((k * 7 + t) % (int)places.size())];
      q.gold = 1; q.xp = 0;
      clearPack(g);
      setLevel(g, L);
      LootOps::completeQuest(g, q);
      for (const Item& it : g.inv) {
        if (!itemEquippable(it.kind)) continue;
        A.check(it, gear::LootSrc::Quest, "a quest's reward");
        A.rule(it.rarity >= Rarity::Uncommon && it.rarity <= Rarity::Epic, it, gear::LootSrc::Quest, "a quest's reward outside Uncommon..Epic");
        A.rule((it.flags & IF_QUESTREWARD) != 0, it, gear::LootSrc::Quest, "a quest's reward without its flag");
      }
    }
  }
  clearPack(g);
}

// ---- story rewards: stages, finales, campaign finales (the reward the engine pays), and `do loot`
void stories(Game& g, Audit& A, uint64_t seed) {
  const int L = kLevels[A.li];
  const gear::LootSrc srcs[] = {gear::LootSrc::Saga, gear::LootSrc::SagaFinale, gear::LootSrc::CampaignFinale};
  const uint64_t cu = g.world.sites.empty() ? 0 : g.world.sites[(size_t)g.world.startSite].culture;
  const cult::Culture* maker = cu && g.world.src ? &g.world.src->culture(cu) : nullptr;
  for (gear::LootSrc s : srcs)
    for (int tier = 2; tier <= 3; tier++)
      for (int k = 0; k < 30; k++) {
        const int D = std::clamp(L - 3 + k % 8, 1, gear::MAX_D);
        const story::RewardRoll R = story::rewardRoll(D, tier, k & 1 ? cu : 0, k & 1 ? maker : nullptr, seed * 1000003ull + (uint64_t)(k * 31 + tier * 7 + (int)s * 977 + A.li * 104729), L, s);
        if (!R.hasItem) { A.fail(std::string(gear::lootSrcName(s)) + ": a rich / great reward without a piece"); continue; }
        A.check(R.item, s, "a story reward");
        const gear::DropSource ds = story::rewardSource(D, tier, 0, L, s);
        A.rule(R.item.rarity >= std::min(ds.atLeast, gear::rarityAllowedTop(ds)) && R.item.rarity <= ds.atMost, R.item, s, "a story reward outside its floor / cap");
        if (s == gear::LootSrc::CampaignFinale) A.rule(R.item.rarity >= Rarity::Legendary, R.item, s, "a campaign's last reward below Legendary");
        if (s == gear::LootSrc::Saga) A.rule(R.item.rarity <= Rarity::Rare, R.item, s, "a saga stage gives above Rare");
      }
  for (int k = 0; k < 20; k++) {
    const Item it = story::storyLoot(std::clamp(L + k % 4, 1, gear::MAX_D), L, seed * 7777ull + (uint64_t)k + (uint64_t)A.li * 99991ull, (k & 1) != 0);
    A.check(it, (k & 1) ? gear::LootSrc::SagaFinale : gear::LootSrc::Saga, "do loot");
  }
}

// ---- the forge: every gear recipe of the start village's smithy, and the smith's commissions
void forge(Game& g, Audit& A, uint64_t seed, bool& smithy) {
  const Site home = g.world.sites[(size_t)g.world.startSite];
  int sb = -1;
  for (int b = home.bldgFirst; b < home.bldgFirst + home.bldgCount; b++) if (g.world.over.bldgs[(size_t)b].type == art::Building::Smithy) { sb = b; break; }
  if (sb < 0 || !g.debugEnterBuilding(sb)) return;
  for (int f = 0; f < 3; f++) g.update(SIM_DT, Input());
  int smith = -1;
  for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].npc && g.actors[k].role == Role::Smith && g.actors[k].st != AState::Dead) { smith = g.actors[k].id; break; }
  if (smith < 0) return;
  smithy = true;
  for (int li = 0; li < NLV; li++) {
    A.li = li;
    for (int skill : {60, 300, 900}) {
      clearPack(g);
      setLevel(g, kLevels[li]);
      g.craft.skill = (uint16_t)skill;
      for (craft::Stuff st : {craft::Stuff::BronzeIngot, craft::Stuff::IronIngot, craft::Stuff::SteelIngot, craft::Stuff::Leather, craft::Stuff::Timber, craft::Stuff::Cloth, craft::Stuff::Coal})
        g.inv.push_back(craft::makeStuff(st, 400));
      if (!craft::openBench(g, smith, art::Building::Smithy)) { A.fail("could not open the forge"); return; }
      std::vector<std::string> ids;
      for (const craft::Recipe& r : g.bench.recipes) if (r.gear()) ids.push_back(r.id);
      for (int rep = 0; rep < 3; rep++)
        for (const std::string& id : ids) {
          int ri = -1;
          for (size_t i = 0; i < g.bench.recipes.size(); i++) if (g.bench.recipes[i].id == id) ri = (int)i;
          Item made;
          if (ri < 0 || !craft::benchMake(g, ri, nullptr, &made)) continue;
          A.check(made, gear::LootSrc::Forge, "the forge");
          A.rule(made.rarity == Rarity::Common || gear::rarityFloorLevel(made.rarity) <= kLevels[li], made, gear::LootSrc::Forge, "the forge passes the level gate");
          A.rule(made.rarity < Rarity::Legendary, made, gear::LootSrc::Forge, "the forge makes a Legendary");
          g.time += 0.37f;   // (the forge's roll is seeded by the moment, the skill and the pack: as in play, time passes)
          // the made piece leaves the pack, so the pack stays small
          for (int i = (int)g.inv.size() - 1; i >= 0; i--) if (itemEquippable(g.inv[(size_t)i].kind)) { g.dropItem(i); }
        }
      g.mode = Mode::Play;
    }
    // the smith's commissions (THE CRAFT... COMMISSION)
    clearPack(g);
    setLevel(g, kLevels[li]);
    g.gold = 1000000;
    g.day += 2;
    if (talkTo(g, smith) && opt(g, "THE CRAFT") >= 0) {
      g.dialogueChoose(opt(g, "THE CRAFT"));
      for (int k = 0; k < 6; k++) {
        const int c = opt(g, "COMMISSION");
        if (c < 0) break;
        const size_t n0 = g.inv.size();
        g.dialogueChoose(c);
        for (size_t i = n0; i < g.inv.size(); i++) {
          const Item& it = g.inv[i];
          if (!itemEquippable(it.kind)) continue;
          A.check(it, gear::LootSrc::Shop, "a commission");
          A.rule(it.rarity >= Rarity::Uncommon && it.rarity <= Rarity::Rare, it, gear::LootSrc::Shop, "a commission outside Uncommon..Rare");
        }
      }
    } else A.fail("could not ask the smith for a commission");
    g.mode = Mode::Play;
  }
  (void)seed;
  clearPack(g);
}

// ---- companions' kits
void companions(Game& g, Audit& A, uint64_t seed) {
  const uint64_t cu = g.world.sites.empty() ? 0 : g.world.sites[(size_t)g.world.startSite].culture;
  for (int k = 0; k < 3; k++) {
    const int mi = g.partyRecruitStranger(k ? "ODA" : "TORVALD", k == 1, cu, (uint32_t)(seed * 977 + (uint64_t)k * 31 + 5), party::Origin::Mercenary, g.pl().p + Vec2(10.0f + 8.0f * (float)k, 0));
    if (mi < 0) break;
    for (const Item& it : g.party.members[(size_t)mi].inv) A.check(it, gear::LootSrc::Companion, "a companion's kit");
  }
}

int lootSourcesCmd(int argc, char** argv) {
  uint64_t A0 = 1, B = 3;
  for (int i = 2; i + 1 < argc; i++)
    if (!std::strcmp(argv[i], "--seeds")) { const char* s = argv[i + 1]; A0 = std::strtoull(s, nullptr, 10); const char* d = std::strstr(s, ".."); B = d ? std::strtoull(d + 2, nullptr, 10) : A0; }
  Audit A;
  int outChests = 0, dungChests = 0, dungeons = 0, smithies = 0;
  for (uint64_t seed = A0; seed <= B; seed++) {
    g_curSeed = seed;
    Game g(seed);
    g.newEndlessGame(seed);
    g.mode = Mode::Play;
    // the start weapon
    {
      clearPack(g);
      Quest q;
      q.id = 1; q.type = QType::Retrieve; q.state = QState::Active;
      LootOps::giveFirstWeapon(g, q);
      bool blade = false;
      for (const Item& it : g.inv) if (itemEquippable(it.kind)) { A.li = 0; A.check(it, SRC_START, "the start weapon"); blade = true; }
      if (!blade) A.fail("the start weapon was not given");
      clearPack(g);
    }
    for (int li = 0; li < NLV; li++) {
      A.li = li;
      setLevel(g, kLevels[li]);
      bodies(g, A, seed);
      setLevel(g, kLevels[li]);
      shops(g, A);
      quests(g, A, seed);
      setLevel(g, kLevels[li]);
      stories(g, A, seed);
    }
    A.li = 0;
    setLevel(g, kLevels[0]);
    companions(g, A, seed);
    // chests in the open (the window's wayside places: the watchtower's foot, the caravan...)
    chestsHere(g, A, 60, outChests);
    bool sm = false;
    forge(g, A, seed, sm);
    smithies += sm ? 1 : 0;
    // every cave, ruin, camp and lair of the window (the nearest first; at most 8 a seed)
    std::vector<std::pair<int, int>> dl;
    const Site& home = g.world.sites[(size_t)g.world.startSite];
    for (int si = 0; si < (int)g.world.sites.size(); si++) {
      const SiteType t = g.world.sites[(size_t)si].type;
      if (t != SiteType::Cave && t != SiteType::Ruin && t != SiteType::BanditCamp) continue;
      const int dx = g.world.sites[(size_t)si].ex - home.ex, dy = g.world.sites[(size_t)si].ey - home.ey;
      dl.push_back({dx * dx + dy * dy, si});
    }
    std::sort(dl.begin(), dl.end());
    // the nearest four ruins (their caches) and the nearest four caves and camps (plain chests, the boss's hoard)
    int ruins = 0, others = 0;
    for (size_t k = 0; k < dl.size(); k++) {
      const bool ruin = g.world.sites[(size_t)dl[k].second].type == SiteType::Ruin;
      if ((ruin ? ruins : others) >= 4) continue;
      (ruin ? ruins : others)++;
      if (!g.debugEnterSite(dl[k].second)) continue;
      dungeons++;
      chestsHere(g, A, 12, dungChests);
    }
    setLevel(g, 1);
  }
  // the table: per source, the items audited and the rarity spread at each hero level
  printf("loot-sources: seeds %llu..%llu: %d gear items (%d above Common) audited; %d chests in the open, %d in %d dungeons; %d smithies\n",
         (unsigned long long)A0, (unsigned long long)B, A.items, A.nonCommon, outChests, dungChests, dungeons, smithies);
  printf("  %-16s %6s", "SOURCE", "ITEMS");
  for (int li = 0; li < NLV; li++) printf("  | L%-2d  C/U/R/E/L/M %%          ", kLevels[li]);
  printf("\n");
  for (int s = 0; s < NROWS; s++) {
    const Tally& t = A.T[s];
    printf("  %-16s %6d", rowName(s), t.n);
    for (int li = 0; li < NLV; li++) {
      if (!t.at[li]) { printf("  | %-27s", "-"); continue; }
      char buf[64];
      int o = 0;
      for (int r = 0; r < RARITIES; r++) o += std::snprintf(buf + o, sizeof(buf) - (size_t)o, r ? "/%.0f" : "%.0f", 100.0 * t.by[li][r] / t.at[li]);
      printf("  | n%-4d %-21s", t.at[li], buf);
    }
    printf("\n");
  }
  for (int s = 0; s < (int)gear::LootSrc::COUNT; s++) {
    if ((gear::LootSrc)s == gear::LootSrc::Debug) continue;   // the scripts' and tests' own
    if (!A.T[s].n) A.fail(std::string("the source ") + gear::lootSrcName((gear::LootSrc)s) + " was never exercised");
  }
  printf("loot-sources: %d failure(s)\n", A.bad);
  return A.bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--loot-sources", "M8b: every item source in real games (bodies, chests, shops, rewards, forge, companions, merchants) gives only valid loot [--seeds A..B]", lootSourcesCmd);
