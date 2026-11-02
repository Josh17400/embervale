// rpg_test --first-armour --seeds A..B [--report] [--gates] [--record]: the first armour within reach (P4 ECONOMY,
// docs/FIX_BATCH_P4.md 2.4), measured on the real game: newEndlessGame, the start village's own smith (the smithy
// entered, its smith's day-0 shelf through LootOps::shopStock) and innkeeper (Game::offersWork / offerFor), beasts of the
// start land spawned at the start's level and felled as the hero's kills (their drops valued with gear::sellPrice).
//   FA1  the start smith's shelf holds a Common leather body piece at <= 48 gold (L1 + L2), every seed
//   FA2  the goods a beast kill drops at the start (wolf, boar, spider: 200 kills each, sold) are worth >= 3.0 gold
//   FA3  the hunter model: ceil((cheapest coat - 30 start gold) / goods per kill) <= 9 kills (12 minutes at the
//        metrics bot's 0.8 kills a minute), every seed
//   FA4  no faucet and no side effects: the beginner's coat sells back for <= 40 % of its price; on every shelf of the
//        world (smith, merchant, hunter, mage, priest, innkeeper at every settlement, days 0 and 2) every row other than
//        a smith's light coat keeps 25d47da's name, item level and rarity, and only leather gear changes price (to 3/5:
//        the LEATHER crafting material and every other piece keep theirs): a digest per seed against kShelfRecord; every
//        band-1 smith's light coat is a Common leather coat at item level max(1, D - 2) (L2)
// --report prints one row a seed (the smith, the light and the metal coat with prices, the inn's first job, goods per
// kill, kills and minutes to the coat) and names the seeds whose start inn has no job a level-1 newcomer can take (data
// for the owner's decision 1b). --gates ends with one PASS / FAIL line per check. --record (run on a 25d47da build)
// prints kShelfRecord's rows: the digest with leather gear at 3/5, the light coat left out.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/loot_ops.h"
#include "rpg/sim/skill_ops.h"
#include "tools/tests/tests.h"

namespace {

// the shelf digest per seed at 25d47da, leather gear at 3/5 and every smith's light coat left out (--record on a 25d47da
// build with this file; MSVC Release, seeds 1..20)
struct ShelfRecord { uint64_t seed, digest; };
constexpr ShelfRecord kShelfRecord[] = {
    {1, 0xe39402f898cf6dcdull},
    {2, 0x915ad61623907b13ull},
    {3, 0xc77b163277003e57ull},
    {4, 0x13b3f8639959338full},
    {5, 0x9ce8680ab4aafd2aull},
    {6, 0x995affd0defbbf92ull},
    {7, 0x4eb8c86fe2aac02aull},
    {8, 0x0ae5c84c95d69cd6ull},
    {9, 0x8e2ed79815f00980ull},
    {10, 0x24a408450c406d04ull},
    {11, 0xc1aa9f9a81869380ull},
    {12, 0xd9d2bd4d88b66534ull},
    {13, 0xd139d033eda35906ull},
    {14, 0x591158bce69e67c9ull},
    {15, 0x1556984c90d0eff4ull},
    {16, 0xebae732e40b02cebull},
    {17, 0x5d541e7c5f1f63a3ull},
    {18, 0xac734c9582c265e5ull},
    {19, 0x1ea7ec6e06c5bd3dull},
    {20, 0x0e332c9f367f2544ull},
};

constexpr int kStartGold = 30;          // game.cpp: the purse a hero starts with
constexpr int kCoatCap = 48;            // FA1: the beginner's coat (80 x 3/5)
constexpr double kGoodsMin = 3.0;       // FA2
constexpr int kKillsMax = 9;            // FA3: 12 minutes at 0.8 kills a minute
constexpr double kKillsPerMin = 0.8;    // the metrics bot's 15-minute mean at 25d47da
constexpr int kKillsEach = 200;

struct Check {
  const char* id;
  const char* what;
  int bad = 0;
  std::string first;
  void fail(const std::string& m) { if (!bad++) first = m; }
};

const char* qtName(QType t) {
  static const char* n[] = {"Main", "Clear", "Hunt", "Retrieve", "Bounty", "Deliver", "Heirloom", "Missing", "NamedBandit", "Protect", "Story", "War",
                            "Supply", "Sea", "Campaign", "Envoy", "Fief", "Errand"};
  return (size_t)t < sizeof(n) / sizeof(n[0]) ? n[(size_t)t] : "?";
}
// a job a level-1 newcomer can take near home: a hunt, an errand or a short supply / delivery run
bool newcomerJob(const Quest& q) {
  switch (q.type) {
    case QType::Hunt: case QType::Errand: case QType::Deliver: case QType::Supply: case QType::Retrieve: case QType::Protect: return true;
    default: return false;
  }
}

bool leatherGear(const Item& it) { return itemEquippable(it.kind) && it.mat == Mat::Leather; }

bool enterBldg(Game& g, int b) {
  const Bldg& F = g.world.over.bldgs[(size_t)b];
  if (!g.world.over.in(F.doorX(), F.doorY() + 1)) g.teleportGlobal(g.world.ox + F.doorX(), g.world.oy + F.doorY() + 1);
  const Bldg& B = g.world.over.bldgs[(size_t)b];
  g.pl().p = Vec2(B.doorX() * 16 + 8.0f, B.doorY() * 16 + 10.0f);
  g.update(SIM_DT, Input());
  return g.inside && g.subBldg == b;
}
// the start village's person of a role, inside the nearest building of a type (-1: none)
int startPerson(Game& g, art::Building type, Role role) {
  const Site home = g.world.sites[(size_t)g.world.startSite];
  for (int b = home.bldgFirst; b < home.bldgFirst + home.bldgCount; b++) {
    if (g.world.over.bldgs[(size_t)b].type != type) continue;
    if (!enterBldg(g, b)) continue;
    for (size_t k = 1; k < g.actors.size(); k++)
      if (g.actors[k].npc && g.actors[k].role == role && g.actors[k].st != AState::Dead) return (int)k;
  }
  return -1;
}

// open a dialogue with actors[k] the way a player does (walk up, interact); true when it is them talking
bool talkTo(Game& g, int k) {
  const int id = g.actors[(size_t)k].id;
  for (Vec2 o : {Vec2(0, 12), Vec2(0, 20), Vec2(10, 0), Vec2(-10, 0), Vec2(0, -10)}) {
    int kk = -1;
    for (size_t j = 1; j < g.actors.size(); j++) if (g.actors[j].id == id) kk = (int)j;
    if (kk < 0) return false;
    g.mode = Mode::Play;
    g.pl().p = g.actors[(size_t)kk].p + o;
    Input in;
    in.interact = true;
    g.update(SIM_DT, in);
    if (g.mode == Mode::Dialogue && g.dlg.actor == id) return true;
  }
  return false;
}

uint64_t fnv(uint64_t h, const std::string& s) {
  for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
  return h;
}

// FA4's digest: every shelf of the world (days 0 and 2), a smith's light coat left out; `record`: leather gear at 3/5
uint64_t shelfDigest(Game& g, bool record, Check& l2, uint64_t seed) {
  static const Role roles[] = {Role::Smith, Role::Merchant, Role::Hunter, Role::Mage, Role::Priest, Role::Innkeeper};
  uint64_t h = 1469598103934665603ull;
  const size_t nSites = g.world.sites.size();
  const int day0 = g.day;
  for (size_t i = 0; i < nSites; i++) {
    if (!g.world.sites[i].settlement()) continue;
    const int D = std::clamp(g.world.sites[i].level, 1, gear::MAX_D);
    for (int day : {0, 2}) {
      g.day = day;
      for (size_t ri = 0; ri < sizeof(roles) / sizeof(roles[0]); ri++) {
        Actor a;
        a.id = 97000 + (int)ri; a.human = true; a.npc = true; a.site = (int)i; a.bldg = -1; a.slot = 5000 + (int)ri;
        a.role = roles[ri];
        for (const Item& it : LootOps::shopStock(g, a)) {
          const bool lightCoat = a.role == Role::Smith && it.kind == ItemKind::Armor && it.mat == Mat::Leather;
          if (lightCoat) {
            if (!record && gear::bandOf(D) == 1 && (it.rarity != Rarity::Common || it.ilvl != std::max(1, D - gear::SHOP_BAND_SPREAD)))
              l2.fail("seed " + std::to_string(seed) + " " + g.world.sites[i].name + " (D " + std::to_string(D) + "): the light coat " + it.name +
                      " rarity " + std::to_string((int)it.rarity) + " item level " + std::to_string(it.ilvl));
            continue;
          }
          const int v = record && leatherGear(it) ? std::max(1, (it.value * 3 + 2) / 5) : it.value;
          char b[64];
          std::snprintf(b, sizeof b, "|%d|%d|%d|%d|%d|%d|%d;", (int)it.kind, (int)it.sub, (int)it.ilvl, (int)it.rarity, (int)it.mat, it.count, v);
          h = fnv(h, it.name + b);
        }
      }
    }
  }
  g.day = day0;
  return h;
}

struct SeedRow {
  std::string smith = "-", light = "-", metal = "-", job = "none";
  int lightPrice = 0, metalPrice = 0, cheapest = 0, kills = 0;
  bool newcomer = false;
  double goods = 0;
};

// goods (sold) and coin a start-land beast kill drops, averaged over the hunt targets
double goodsPerKill(Game g) {
  g.mode = Mode::Play; g.godMode = true; g.noWildSpawns = true;
  const int level = std::clamp(g.world.sites[(size_t)g.world.startSite].level, 1, gear::MAX_D);
  double sum = 0;
  int n = 0;
  for (art::Monster m : {art::Monster::Wolf, art::Monster::Boar, art::Monster::Spider}) {
    for (int k = 0; k < kKillsEach; k++) {
      g.pickups.clear();
      const int id = SkillOps::spawnMonster(g, m, g.pl().p + Vec2(48, 0), level);
      if (id < 0) continue;
      g.debugFell(id, true);
      for (const Pickup& p : g.pickups) sum += p.gold + (p.gold ? 0 : gear::sellPrice(p.item) * std::max(1, p.item.stackable() ? p.item.count : 1));
      g.actors.erase(std::remove_if(g.actors.begin() + 1, g.actors.end(), [&](const Actor& a) { return a.id == id; }), g.actors.end());
      if (g.mode != Mode::Play) g.mode = Mode::Play;
      n++;
    }
  }
  g.pickups.clear();
  return n ? sum / n : 0;
}

int firstArmourCmd(int argc, char** argv) {
  uint64_t a = 1, b = 20;
  bool report = false, gates = false, record = false;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!std::strcmp(argv[i], "--report")) report = true;
    else if (!std::strcmp(argv[i], "--gates")) gates = true;
    else if (!std::strcmp(argv[i], "--record")) record = true;
  }
  Check C[] = {{"FA1", "the start smith sells a Common leather body piece at <= 48 gold"},
               {"FA2", "goods per beast kill at the start >= 3.0 gold"},
               {"FA3", "ceil((coat - 30) / goods per kill) <= 9 kills"},
               {"FA4", "no faucet; every other shelf row as at 25d47da, only leather gear at 3/5; band-1 light coat Common at max(1, D-2)"}};
  std::map<uint64_t, uint64_t> rec;
  for (const ShelfRecord& r : kShelfRecord) if (r.seed) rec[r.seed] = r.digest;
  std::vector<std::pair<uint64_t, SeedRow>> rows;
  if (report)
    std::printf("first-armour: seed | smith | light coat (price) | metal coat (price) | the inn's first job | goods/kill | kills | minutes\n");
  for (uint64_t seed = a; seed <= b; seed++) {
    Game base(seed);
    base.newEndlessGame(seed);
    base.mode = Mode::Play; base.godMode = true; base.noWildSpawns = true; base.hour = 11.0f;
    SeedRow R;
    // the shelf digest (FA4) first, on a copy, so nothing the hero does before it moves it
    {
      Game t = base;
      const uint64_t d = shelfDigest(t, record, C[3], seed);
      if (record) std::printf("    {%llu, 0x%016llxull},\n", (unsigned long long)seed, (unsigned long long)d);
      else {
        auto it = rec.find(seed);
        if (it != rec.end() && it->second != d)
          C[3].fail("seed " + std::to_string(seed) + ": the shelves moved beyond leather's price and the light coat");
        else if (it == rec.end() && gates) std::printf("note: first-armour: no 25d47da shelf record for seed %llu\n", (unsigned long long)seed);
      }
    }
    // the start smith's shelf
    {
      Game t = base;
      const int k = startPerson(t, art::Building::Smithy, Role::Smith);
      if (k < 0) C[0].fail("seed " + std::to_string(seed) + ": no smith in the start village");
      else {
        const Actor& sm = t.actors[(size_t)k];
        R.smith = sm.name;
        R.cheapest = 1 << 30;
        for (const Item& it : LootOps::shopStock(t, sm)) {
          if (it.kind != ItemKind::Armor) continue;
          const bool light = it.mat == Mat::Leather;
          (light ? R.light : R.metal) = it.name;
          (light ? R.lightPrice : R.metalPrice) = it.value;
          if (light && it.rarity == Rarity::Common) R.cheapest = std::min(R.cheapest, it.value);
          if (light && 5 * gear::sellPrice(it) > 2 * it.value)
            C[3].fail("seed " + std::to_string(seed) + ": " + it.name + " sells back for " + std::to_string(gear::sellPrice(it)) + " of " + std::to_string(it.value));
        }
        if (R.cheapest > kCoatCap) C[0].fail("seed " + std::to_string(seed) + ": the cheapest Common leather coat " + (R.cheapest == (1 << 30) ? std::string("missing") : std::to_string(R.cheapest)));
      }
    }
    // the start inn's first job (data for decision 1b)
    {
      Game t = base;
      t.life.census(t.world, t.world.startSite);   // (hasOffer asks the census's job pool: worked out on first need)
      int k = startPerson(t, art::Building::Inn, Role::Innkeeper);
      // the opening first (the old blade from his hand), then ANY WORK GOING? as a newcomer asks it, and the job taken
      if (k >= 0 && talkTo(t, k)) {
        t.mode = Mode::Play;
        k = -1;
        for (size_t j = 1; j < t.actors.size(); j++) if (t.actors[j].npc && t.actors[j].role == Role::Innkeeper) k = (int)j;
      }
      if (k >= 0 && talkTo(t, k)) {
        auto opt = [&](const char* w) { for (size_t o = 0; o < t.dlg.opts.size(); o++) if (t.dlg.opts[o].label.find(w) != std::string::npos) return (int)o; return -1; };
        const size_t had = t.quests.size();
        int o = opt("ANY WORK GOING?");
        if (o >= 0) { t.dialogueChoose(o); o = opt("I'LL DO IT."); }
        if (o >= 0) t.dialogueChoose(o);
        if (t.quests.size() > had) {
          const Quest& q = t.quests.back();
          R.job = std::string(qtName(q.type)) + " " + std::to_string(q.gold) + "g";
          R.newcomer = newcomerJob(q);
        }
      }
    }
    // goods per kill, the hunter model
    R.goods = goodsPerKill(base);
    if (R.goods < kGoodsMin) C[1].fail("seed " + std::to_string(seed) + ": " + std::to_string(R.goods) + " gold a kill");
    if (R.cheapest > 0 && R.cheapest < (1 << 30) && R.goods > 0) {
      R.kills = std::max(0, (int)std::ceil((R.cheapest - kStartGold) / R.goods));
      if (R.kills > kKillsMax) C[2].fail("seed " + std::to_string(seed) + ": " + std::to_string(R.kills) + " kills for a " + std::to_string(R.cheapest) + "-gold coat");
    } else C[2].fail("seed " + std::to_string(seed) + ": no coat to price");
    if (report)
      std::printf("  %3llu | %-10s | %-22s %3d | %-22s %3d | %-16s | %4.2f | %2d | %4.1f\n", (unsigned long long)seed, R.smith.c_str(), R.light.c_str(),
                  R.lightPrice, R.metal.c_str(), R.metalPrice, R.job.c_str(), R.goods, R.kills, R.kills / kKillsPerMin);
    rows.push_back({seed, R});
  }
  if (record) return 0;
  if (report) {
    std::string none, far;
    for (const auto& r : rows)
      if (r.second.job == "none") none += " " + std::to_string(r.first);
      else if (!r.second.newcomer) far += " " + std::to_string(r.first);
    std::printf("first-armour: start inns with no job:%s; with no job a level-1 newcomer can take:%s (decision 1b)\n", none.empty() ? " -" : none.c_str(),
                far.empty() ? " -" : far.c_str());
  }
  int fails = 0;
  for (const Check& c : C) {
    if (c.bad) fails++;
    if (gates || c.bad) std::printf("%s %s %s%s%s\n", c.bad ? "FAIL" : "PASS", c.id, c.what, c.bad ? ": " : "", c.first.c_str());
  }
  std::printf("first-armour: %d checks over seeds %llu..%llu, %d failed\n", (int)(sizeof(C) / sizeof(C[0])), (unsigned long long)a, (unsigned long long)b, fails);
  return fails ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--first-armour",
             "the first armour within reach (P4 ECONOMY FA1-FA4: the 48-gold beginner's coat, goods per kill, kills to the coat, no "
             "other shelf row moved) --seeds A..B [--report] [--gates] [--record]",
             firstArmourCmd);
