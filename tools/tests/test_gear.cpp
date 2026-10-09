// rpg_test --gear [--seeds A..B]: M6 Steel contract checks (VISION_PLAN 7.2 - 7.5, 15.11, 15.19). Lead (phase A); the
// NUMBERS lane owns this file afterwards and extends it (it may tighten, never drop, these checks).
//   - the 7.2 / 7.4 / 7.5 formulas: bands, level sync, rarity budget, mitigation cap, the level gap, XP decay, attunement
//   - the item layout of SAVE_VER 12: every new field survives writeItem / readItem; damaged values load as defaults
//   - crafting: the base recipes are well formed and unique; ore -> ingot -> bronze -> a bronze sword works on a pack;
//     the craft block round-trips; every archetype's culture recipes exist and are locked behind its secrets
//   - regional ores (15.11): no sampled tile has every ore; several different primary ores over a world
//   - the new families: a harpy and a golem spawn with their names
#include <algorithm>
#include <initializer_list>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <set>
#include "rpg/culture/culture.h"
#include "rpg/sim/craft.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/gear.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

int formulas() {
  int bad = 0;
  auto fail = [&](const char* m) { out("FAIL: gear: %s\n", m); bad++; };
  const int lo[] = {1, 9, 17, 25, 33, 43, 53}, hi[] = {8, 16, 24, 32, 42, 52, 60};
  for (int b = 1; b <= 7; b++)
    for (int l = lo[b - 1]; l <= hi[b - 1]; l++) if (gear::bandOf(l) != b) { fail("bandOf disagrees with the 7.2 table"); b = 8; break; }
  if (gear::effLevel(30, 8) != 12 || gear::effLevel(5, 8) != 5) fail("level sync (eff = min(iLvl, level + 4))");
  if (std::fabs(gear::rarityMult(Rarity::Legendary) - 1.20f) > 1e-4f || gear::affixSlots(Rarity::Epic) != 3) fail("rarity budget");
  if (gear::mitigation(1e6f, 1) > 0.7001f || gear::mitigation(0, 5) != 0) fail("mitigation cap 70 % / none without armour");
  if (std::fabs(gear::gapPlayerDamage(10, 30) - 0.4f) > 1e-4f || gear::gapPlayerDamage(10, 14) != 1.0f) fail("level-gap damage floor 0.4");
  if (gear::killXp(100, 1, 30) != 5) fail("XP decay: the start zone at level 30 earns 5 %");
  if (gear::attunementSlots(9) != 0 || gear::attunementSlots(10) != 1 || gear::attunementSlots(25) != 2 || gear::attunementSlots(40) != 3)
    fail("attunement 1 / 2 / 3 at 10 / 25 / 40");
  if (gear::merchantGold(0) != 300 || gear::merchantGold(3) != 3000) fail("merchant gold 300 .. 3000");
  // a legendary never below D 10, never from a plain foe
  Rng r(12345);
  gear::DropSource s;
  s.D = 9; s.rank = 5;
  for (int i = 0; i < 20000; i++) if (gear::rollRarity(r, s) == Rarity::Legendary) { fail("a legendary rolled below D 10"); break; }
  s.D = 30; s.rank = 0;
  for (int i = 0; i < 20000; i++) if (gear::rollRarity(r, s) == Rarity::Legendary) { fail("a legendary from a plain foe"); break; }
  return bad;
}

int itemLayout() {
  int bad = 0;
  Item it;
  it.kind = ItemKind::Weapon; it.sub = (uint8_t)WeaponType::Spear; it.tier = 6; it.rarity = Rarity::Legendary; it.power = 77; it.name = "TEST";
  it.icon = art::Icon::Spear; it.tint = 0xFF336699u; it.ilvl = 44; it.mat = Mat::Alloy; it.alloy = 2; it.culture = 0x1234567890ABCDEFull;
  it.form = 3; it.seed = 0xDEADBEEF; it.affix[0] = {Affix::FireDmg, 9}; it.affix[2] = {Affix::GoldFind, 12}; it.unique = Unique::Wayfarer;
  it.flags = IF_CRAFTED | IF_STOLEN;
  std::vector<uint8_t> buf;
  BinW w(buf);
  writeItem(w, it);
  BinR rd(buf);
  const Item b = readItem(rd);
  if (rd.bad || rd.p != buf.size() || !(b.same(it)) || b.form != 3 || b.seed != it.seed || b.affixValue(Affix::FireDmg) != 9 ||
      b.affixValue(Affix::GoldFind) != 12 || b.affixCount() != 2 || b.unique != Unique::Wayfarer || b.flags != it.flags || b.tier != 6) {
    out("FAIL: gear: an item with every M6 field did not survive writeItem / readItem\n");
    bad++;
  }
  // damaged values load as defaults
  std::vector<uint8_t> dmg = buf;
  const size_t tail = dmg.size();
  dmg[tail - 2] = 250;   // unique
  dmg[tail - 2 - 3 * 3] = 200;   // the first affix kind (3 affixes of 3 bytes before unique, flags)
  BinR rd2(dmg);
  const Item c = readItem(rd2);
  if (c.unique != Unique::None || c.affix[0].kind != Affix::None) { out("FAIL: gear: out-of-range unique / affix ids were not cleared\n"); bad++; }
  for (int i = 0; i < (int)Affix::COUNT; i++) if (!affixInfo((Affix)i).name) { out("FAIL: gear: affix %d has no name\n", i); bad++; }
  for (int i = 1; i < (int)Unique::COUNT; i++) if (!uniqueInfo((Unique)i).name[0]) { out("FAIL: gear: unique %d has no name\n", i); bad++; }
  return bad;
}

int crafting() {
  int bad = 0;
  auto fail = [&](const std::string& m) { out("FAIL: craft: %s\n", m.c_str()); bad++; };
  std::set<std::string> ids;
  for (const craft::Recipe& r : craft::baseRecipes()) {
    if (!ids.insert(r.id).second) fail("duplicate recipe id " + r.id);
    if (r.in.empty()) fail("recipe without inputs " + r.id);
    if (r.gear() && !itemEquippable(r.kind)) fail("gear recipe makes no gear " + r.id);
  }
  // the chain: 6 copper ore + 2 tin ore -> 3 copper + 1 tin ingots -> 3 bronze ingots -> a bronze sword (with leather)
  std::vector<Item> inv = {craft::makeStuff(craft::Stuff::CopperOre, 6), craft::makeStuff(craft::Stuff::TinOre, 2),
                           craft::makeStuff(craft::Stuff::Leather, 1)};
  craft::Knowledge k;
  k.skill = 1000;
  Rng rng(9);
  auto find = [](const char* id) -> const craft::Recipe* {
    for (const craft::Recipe& r : craft::baseRecipes()) if (r.id == id) return &r;
    return nullptr;
  };
  auto run = [&](const char* id, int times) {
    const craft::Recipe* r = find(id);
    if (!r) { fail(std::string("missing recipe ") + id); return; }
    for (int i = 0; i < times; i++) {
      Item o;
      if (!craft::make(*r, inv, k, rng, 5, nullptr, o)) { fail(std::string("could not make ") + id); return; }
      inv.erase(std::remove_if(inv.begin(), inv.end(), [](const Item& x) { return x.count <= 0; }), inv.end());
      bool merged = false;
      for (Item& x : inv) if (x.stackable() && x.same(o)) { x.count += o.count; merged = true; break; }
      if (!merged) inv.push_back(o);
    }
  };
  run("smelt.copper", 3);
  run("smelt.tin", 1);
  run("smelt.bronze", 1);
  run("forge.bronze.sword", 1);
  bool sword = false;
  for (const Item& x : inv) if (x.kind == ItemKind::Weapon && x.mat == Mat::Bronze && (x.flags & IF_CRAFTED) && x.ilvl == 5) sword = true;
  if (!sword) fail("the bronze chain did not end in a crafted bronze sword");
  // the craft block
  k.advance(cult::familyId(1, 2), 1, craft::SecretKind::AlloyRecipe, 100, craft::HOW_APPRENTICE);
  k.trust[77] = 50;
  std::vector<uint8_t> blk;
  k.serialize(blk);
  craft::Knowledge k2;
  std::vector<uint8_t> blk2;
  if (!k2.deserialize(blk) || (k2.serialize(blk2), blk2 != blk) || !k2.knows(cult::familyId(1, 2), 1, craft::SecretKind::AlloyRecipe))
    fail("the craft block did not round-trip");
  // every archetype's own recipes, locked behind its secrets
  for (int a = 0; a < (int)cult::Archetype::COUNT; a++) {
    cult::Culture C = cult::Atlas::make((cult::Archetype)a, 1000u + (uint32_t)a);
    C.id = cult::familyId(a, 7);   // (Atlas::make leaves the id 0: a heartland stand-in with no recipes of its own)
    const std::vector<craft::Recipe> R = craft::cultureRecipes(C);
    int smelts = 0;
    for (const craft::Recipe& r : R) {
      smelts += !r.gear() && r.out == craft::Stuff::AlloyIngot;
      if (r.culture != C.id) fail("a culture recipe without its culture");
    }
    if (smelts != (int)C.arms.alloys.size()) fail(std::string("alloy smelting recipes != alloys for ") + cult::archetypeName((cult::Archetype)a));
    craft::Knowledge none;
    none.skill = 1000;
    std::vector<Item> rich;
    for (int s = 0; s < (int)craft::Stuff::COUNT; s++) rich.push_back(craft::makeStuff((craft::Stuff)s, 99));
    for (const craft::Recipe& r : R)
      if (craft::canMake(r, rich, none)) { fail(std::string("a culture recipe needs no secret: ") + r.id); break; }
  }
  return bad;
}

int ores(uint64_t seed) {
  Game g(seed);
  g.world.generateEndlessAt(seed, 0, 0);
  if (!g.world.src) return 0;
  int bad = 0, all = 0;
  std::set<int> primaries;
  for (int j = -3; j <= 3; j++)
    for (int i = -3; i <= 3; i++) {
      const std::vector<craft::OreSource> v = craft::oresAt(*g.world.src, i * 3000, j * 3000);
      if (v.size() >= (size_t)ew::Ore::COUNT) all++;
      if (!v.empty()) primaries.insert((int)v[0].ore);
    }
  if (all) { out("FAIL: craft: %d sampled tiles hold every ore (15.11: no region has everything)\n", all); bad++; }
  if (primaries.size() < 3) { out("FAIL: craft: only %zu different primary ores over 49 far-apart tiles\n", primaries.size()); bad++; }
  return bad;
}

int families(uint64_t seed) {
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  int bad = 0;
  for (art::Monster m : {art::Monster::Harpy, art::Monster::Golem}) {
    const int id = g.debugSpawnAt(m, g.pl().p + Vec2(80, 0), 3);
    const Actor* a = nullptr;
    for (const Actor& x : g.actors) if (x.id == id) a = &x;
    if (!a || a->mon != m || a->name.empty() || a->maxHp <= 0) { out("FAIL: gear: monster %d did not spawn whole\n", (int)m); bad++; }
  }
  for (int i = 0; i < 120; i++) g.update(SIM_DT, Input());
  return bad;
}

// ---- NUMBERS lane (M6 phase B): the generators, the use-time numbers and the play rules
int generation() {
  int bad = 0;
  auto fail = [&](const std::string& m) { if (bad < 30) out("FAIL: gear: %s\n", m.c_str()); bad++; };
  cult::Culture C = cult::Atlas::make(cult::Archetype::Steppe, 77);
  C.id = cult::familyId(5, 9);
  Rng r(4242);
  const ItemKind kinds[] = {ItemKind::Weapon, ItemKind::Bow, ItemKind::Staff, ItemKind::Armor, ItemKind::Helmet, ItemKind::Shield,
                            ItemKind::Gloves, ItemKind::Boots, ItemKind::Cloak, ItemKind::Ring, ItemKind::Amulet};
  for (int il : {1, 4, 8, 12, 20, 28, 40, 50, 60})
    for (ItemKind k : kinds)
      for (int rar = 0; rar <= 4; rar++)
        for (const cult::Culture* c : {(const cult::Culture*)nullptr, (const cult::Culture*)&C}) {
          const Item it = gear::makeGearC(r, k, k == ItemKind::Weapon ? (il % 6) : 0, il, (Rarity)rar, c);
          if (it.ilvl != il || it.tier != gear::bandOf(il) - 1) fail("item level / band tier of " + it.name);
          if (it.name.empty()) fail("an unnamed piece");
          if (it.affixCount() != gear::affixSlots(it.rarity)) fail("affix budget of " + it.name);
          if (it.rarity == Rarity::Legendary && !gear::uniqueImplemented(it.unique)) fail("legendary without a working power: " + it.name);
          if ((k == ItemKind::Ring || k == ItemKind::Amulet) && it.rarity < Rarity::Uncommon) fail("a common ring");
          if (c && it.culture != c->id) fail("maker culture lost on " + it.name);
          if (gear::bandOf(il) >= 4 && c && !c->arms.alloys.empty() && k == ItemKind::Weapon &&
              (it.mat != Mat::Alloy || it.alloy < 1 || it.alloy > c->arms.alloys.size()))
            fail("band 4+ culture weapon not in the culture's alloy: " + it.name);
          if (!c && gear::bandOf(il) >= 4 && k == ItemKind::Weapon && it.mat != Mat::Steel) fail("a heartland band 4+ weapon not steel");
          if (c && k == ItemKind::Helmet && it.form != (uint8_t)C.arms.helm[0] + 1 && it.form != (uint8_t)C.arms.helm[1] + 1) fail("helmet form outside the culture's options");
          if (!c && it.form != 0) fail("a heartland piece with a culture form");
          if (k == ItemKind::Weapon || k == ItemKind::Armor) {
            const float base = k == ItemKind::Weapon ? gear::weaponBase((WeaponType)it.sub) : gear::armourBase(k) * (it.mat == Mat::Leather ? 0.92f : 1.0f);
            const float want = base * gear::statScale(il) * gear::rarityMult(it.rarity);
            if (std::fabs(it.power - want) > 0.6f) fail("power of " + it.name + " " + std::to_string(it.power) + " != " + std::to_string(want));
          }
        }
  // level sync at use: an item level 30 sword at level 8 works as item level 12
  Item s30 = gear::makeGearC(r, ItemKind::Weapon, 0, 30, Rarity::Common, nullptr);
  const float want = gear::weaponBase(WeaponType::Sword) * gear::statScale(12);   // common: the exact 7.2 number at eff 12
  if (std::fabs(gear::usePower(s30, 8) - want) > 0.01f || std::fabs(gear::usePower(s30, 40) - s30.power) > 0.51f) fail("usePower level sync");
  Item hand = s30;
  hand.power = 7;   // a power set by hand scales from what it holds
  if (std::fabs(gear::usePower(hand, 8) - 7.0f * gear::statScale(12) / gear::statScale(30)) > 0.01f) fail("usePower of a hand-set power");
  // affix sync: a synced item's affixes shrink with it
  Item e = gear::makeGearC(r, ItemKind::Ring, 0, 50, Rarity::Epic, nullptr);
  for (const ItemAffix& a : e.affix)
    if (a.kind != Affix::None && gear::affixUse(e, a.kind, 1) > a.value) fail("a synced affix grew");
  // names: culture words above band 3 (the alloy's name), legendaries named and storied
  {
    Item w = gear::makeGearC(r, ItemKind::Weapon, (int)WeaponType::Sword, 30, Rarity::Common, &C);
    if (!C.arms.alloys.empty() && w.alloy >= 1 && w.name.find(C.arms.alloys[(size_t)w.alloy - 1].name) == std::string::npos) fail("band 4 name without the alloy word: " + w.name);
    Item l = gear::makeGearC(r, ItemKind::Weapon, (int)WeaponType::Sword, 30, Rarity::Legendary, &C);
    if (gear::legendaryName(l, &C).empty() || gear::legendaryLore(l, &C).empty()) fail("legendary name / lore");
  }
  // matBandCap: crafted bronze stays in band 1, iron in 2, steel in 3
  if (gear::matBandCap(Mat::Bronze, nullptr, 0) != 1 || gear::matBandCap(Mat::Iron, nullptr, 0) != 2 || gear::matBandCap(Mat::Steel, nullptr, 0) != 3) fail("material band caps");
  // the XP curve keeps the first level's M5 pace
  if (gear::xpForNext(1) != 120) fail("xpForNext(1) moved off the M5 first level-up");
  // at least 16 unique powers work, and every one that rolls is one of them
  int working = 0;
  for (int u = 1; u < (int)Unique::COUNT; u++) working += gear::uniqueImplemented((Unique)u);
  if (working < 16) fail("only " + std::to_string(working) + " unique powers implemented (16 wanted)");
  return bad;
}

// helpers for the play checks
int roleIn(const Game& g, Role r) {
  for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].npc && g.actors[k].role == r && g.actors[k].st != AState::Dead) return (int)k;
  return -1;
}
bool talkTo(Game& g, int k) {
  const int id = g.actors[(size_t)k].id;
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
int findRecipe(const Game& g, const char* id) {
  for (size_t i = 0; i < g.bench.recipes.size(); i++) if (g.bench.recipes[i].id == id) return (int)i;
  return -1;
}
bool eqValid(const Game& g) {
  for (int e : {g.eqWeapon, g.eqBow, g.eqStaff, g.eqArmor, g.eqHelmet, g.eqShield, g.eqRing, g.eqAmulet, g.eqGloves, g.eqBoots, g.eqCloak})
    if (e >= (int)g.inv.size()) return false;
  return true;
}

int play(uint64_t seed) {
  int bad = 0;
  auto fail = [&](const std::string& m) { out("FAIL: gear play: %s\n", m.c_str()); bad++; };
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.noWildSpawns = true;
  g.hour = 12;
  // ---- the potion cooldown (8 s shared) and the 10-potion belt
  {
    Item pot = makePotion(PotionType::Health, 0); pot.count = 3;
    g.inv.push_back(pot);
    const int pi = (int)g.inv.size() - 1;
    g.pl().hp = 20;
    g.useItem(pi);
    const float h1 = g.pl().hp;
    g.useItem(pi);
    if (h1 <= 20 || g.pl().hp > h1 + 0.01f) fail("a second potion within the 8 s cooldown healed");
    for (int f = 0; f < (int)(8.2f / SIM_DT); f++) g.update(SIM_DT, Input());
    g.pl().hp = 20;
    g.useItem(pi);
    if (g.pl().hp <= 20.5f) fail("the potion did not work again after the cooldown");
    Pickup pk; pk.p = g.pl().p; pk.item = makePotion(PotionType::Health, 0); pk.item.count = 15; pk.t = 1;
    g.pickups.push_back(pk);
    for (int f = 0; f < 30; f++) g.update(SIM_DT, Input());
    int held = 0;
    for (const Item& it : g.inv) if (it.kind == ItemKind::Potion) held += it.count;
    if (held > gear::POTION_CARRY) fail("carrying " + std::to_string(held) + " potions (cap 10)");
  }
  // ---- attunement: none before level 10, one at 10, two at 25
  {
    Rng r(seed);
    g.plLevel = 5;
    g.inv.push_back(gear::makeGearC(r, ItemKind::Weapon, 0, 20, Rarity::Legendary, nullptr));
    const int li = (int)g.inv.size() - 1;
    g.useItem(li);
    if (g.eqWeapon == li) fail("a legendary was attuned at level 5");
    g.plLevel = 10;
    g.useItem(li);
    if (g.eqWeapon != li) fail("a legendary could not be attuned at level 10");
    g.inv.push_back(gear::makeGearC(r, ItemKind::Ring, 0, 20, Rarity::Legendary, nullptr));
    const int ri = (int)g.inv.size() - 1;
    g.useItem(ri);
    if (g.eqRing == ri) fail("a second legendary attuned at level 10 (one slot)");
    g.plLevel = 25;
    g.useItem(ri);
    if (g.eqRing != ri) fail("a second legendary refused at level 25 (two slots)");
    // level sync in play: the hero's weapon damage reads eff
    const float wd = g.weaponDamage();
    const float want = gear::usePower(g.inv[(size_t)li], 25) * gear::playerDamageMul(25);
    if (std::fabs(wd - want) > 0.01f) fail("weaponDamage does not read the synced power");
    g.eqWeapon = -1; g.eqRing = -1;
  }
  // ---- XP decay: a level-1 wolf at level 30 is worth 5 %
  {
    g.plLevel = 30; g.plXp = 0;
    const int id = g.debugSpawnAt(art::Monster::Wolf, g.pl().p + Vec2(60, 0), 1);
    int xp = 0;
    for (const Actor& a : g.actors) if (a.id == id) xp = a.xp;
    g.debugFell(id, true);
    const int want = std::max(1, (int)std::lround(xp * gear::xpDecay(30, 1)));
    if (std::abs(g.plXp - want) > 1) fail("XP decay: got " + std::to_string(g.plXp) + " of a " + std::to_string(xp) + " XP wolf (want " + std::to_string(want) + ")");
    g.plLevel = 1; g.plXp = 0;
  }
  // ---- the forge: the smithy of the start village, the smith's dialogue, the bronze chain on the forge screen
  {
    const Site home = g.world.sites[(size_t)g.world.startSite];
    int smithy = -1;
    for (int b = home.bldgFirst; b < home.bldgFirst + home.bldgCount; b++) if (g.world.over.bldgs[(size_t)b].type == art::Building::Smithy) { smithy = b; break; }
    if (smithy < 0) { out("  (seed %llu: no smithy in the start village; forge checks skipped)\n", (unsigned long long)seed); return bad; }
    if (!g.debugEnterBuilding(smithy)) { fail("could not enter the smithy"); return bad; }
    for (int f = 0; f < 3; f++) g.update(SIM_DT, Input());
    const int k = roleIn(g, Role::Smith);
    if (k < 0) { out("  (seed %llu: nobody at the forge; forge checks skipped)\n", (unsigned long long)seed); return bad; }
    // the pack: a worn helmet between the ore stacks (its index must survive the emptied stacks)
    Rng r(seed + 1);
    g.inv.clear();
    g.eqWeapon = g.eqBow = g.eqStaff = g.eqArmor = g.eqHelmet = g.eqShield = g.eqRing = g.eqAmulet = g.eqGloves = g.eqBoots = g.eqCloak = -1;
    g.inv.push_back(craft::makeStuff(craft::Stuff::CopperOre, 6));
    g.inv.push_back(gear::makeGearC(r, ItemKind::Helmet, 0, 3, Rarity::Common, nullptr));
    g.eqHelmet = 1;
    g.inv.push_back(craft::makeStuff(craft::Stuff::TinOre, 2));
    g.inv.push_back(craft::makeStuff(craft::Stuff::Leather, 1));
    if (!talkTo(g, k)) { fail("could not talk to the smith"); return bad; }
    const int o = opt(g, "WORK THE FORGE");
    if (o < 0) { fail("the smith offers no WORK THE FORGE"); return bad; }
    if (opt(g, "THE CRAFT") < 0) fail("the smith offers no ABOUT THE CRAFT (the apprenticeship)");
    g.dialogueChoose(o);
    if (g.mode != Mode::Forge || g.bench.recipes.empty()) { fail("WORK THE FORGE did not open the forge screen"); return bad; }
    g.craft.skill = 100;   // smithing 10: bronze needs 5
    auto make = [&](const char* id, int n) {
      for (int i = 0; i < n; i++) {
        const int ri = findRecipe(g, id);
        std::string why;
        if (ri < 0 || !craft::benchMake(g, ri, &why)) { fail(std::string("could not make ") + id + ": " + why); return; }
      }
    };
    make("smelt.copper", 3);
    make("smelt.tin", 1);
    make("smelt.bronze", 1);
    make("forge.bronze.sword", 1);
    if (!eqValid(g) || g.eqHelmet < 0 || g.inv[(size_t)g.eqHelmet].kind != ItemKind::Helmet) fail("the worn helmet's index broke when stacks emptied");
    for (const Item& it : g.inv) if (it.count <= 0) fail("an emptied stack stayed in the pack");
    bool sword = false;
    for (const Item& it : g.inv)
      if (it.kind == ItemKind::Weapon && it.mat == Mat::Bronze && (it.flags & IF_CRAFTED)) {
        sword = true;
        const int D = std::max(1, home.level);
        if (it.ilvl != std::min(D, 8)) fail("crafted ilvl " + std::to_string(it.ilvl) + " (the settlement's D " + std::to_string(D) + ", bronze caps at 8)");
      }
    if (!sword) fail("the forge did not make a bronze sword");
    if (g.craft.skill <= 100) fail("smithing did not grow with use");
    g.mode = Mode::Play;
    // the smith's shelf: iron ingots, never above the village's D + 2 with the hero at level 40
    g.plLevel = 40;
    const std::vector<Item> st = craft::stockOf(g, g.actors[(size_t)k]);
    bool iron = false;
    for (const Item& it : st) {
      iron |= craft::isStuff(it, craft::Stuff::IronIngot);
      if (itemEquippable(it.kind) && it.ilvl > std::max(1, home.level) + gear::SHOP_BAND_SPREAD) fail("the smith stocks by the player's level: " + it.name);
      if (it.rarity > Rarity::Rare) fail("a village smith sells " + it.name);
    }
    if (!iron) fail("the smith sells no iron ingots");
    g.plLevel = 1;
    // (M6 fixer r3, review: "smith trust can be farmed by buying and selling the same cheap material back") buying
    // materials earns no trust, the smith's own gear at most TRUST_BUY_CAP a restock, a piece sold back none at all
    {
      g.gold = 100000;
      // (the shop opens the way a player opens it: talk, LET ME SEE YOUR WARES)
      if (!talkTo(g, k) || opt(g, "WARES") < 0) fail("could not open the smith's wares");
      else g.dialogueChoose(opt(g, "WARES"));
      if (g.mode != Mode::Shop) fail("the smith's wares did not open");
      const uint64_t key = g.shop.key;
      const int t0 = g.craft.trust.count(key) ? g.craft.trust[key] : 0;
      int bought = 0;
      for (int round = 0; round < 8; round++) {
        const size_t inv0 = g.inv.size();
        for (int i = 0; i < 12 && !g.shop.stock.empty(); i++)
          if (g.buy((int)((size_t)i % g.shop.stock.size()))) bought++;
        for (int i = (int)g.inv.size() - 1; i >= (int)inv0; i--) {
          g.craft.live.purse[key].second = 100000;   // (the purse is not what is tested here)
          g.sell(i);
        }
      }
      const int t1 = g.craft.trust.count(key) ? g.craft.trust[key] : 0;
      if (bought < 8) fail("the trust check bought only " + std::to_string(bought) + " things");
      if (t1 - t0 > craft::TRUST_BUY_CAP)
        fail("buying and selling back at the smith farmed trust: " + std::to_string(t0) + " -> " + std::to_string(t1));
      g.craft.trust[key] = (uint8_t)t0;
      g.craft.live.trustBuy.clear();
      g.mode = Mode::Play;
    }
    // apprenticeship: commissions build trust; at 60 the culture's patterns are taught
    const cult::Culture* hc = g.world.cultureOf(g.world.startSite);
    if (hc && hc->id) {
      g.gold = 100000;
      if (talkTo(g, k) && opt(g, "THE CRAFT") >= 0) {
        g.dialogueChoose(opt(g, "THE CRAFT"));
        if (opt(g, "COMMISSION") < 0 || opt(g, "TEACH ME") < 0) fail("THE CRAFT offers no COMMISSION / TEACH ME");
        // (M6 fixer r4) many commissions in one restock earn trust once: nine on day 1 must not teach the patterns
        const uint64_t sk = g.npcKey(g.actors[(size_t)k]);
        const int tStart = g.craft.trust.count(sk) ? g.craft.trust[sk] : 0;
        for (int i = 0; i < 9; i++) {
          const int c = opt(g, "COMMISSION");
          if (c < 0) { fail("no COMMISSION option"); break; }
          g.dialogueChoose(c);
        }
        const int tOne = g.craft.trust.count(sk) ? g.craft.trust[sk] : 0;
        if (tOne - tStart > craft::TRUST_COMMISSION)
          fail("nine commissions in one restock farmed trust: " + std::to_string(tStart) + " -> " + std::to_string(tOne));
        if (g.craft.knows(hc->id, 0, craft::SecretKind::WeaponPattern)) fail("one visit of commissions taught the patterns");
        // the cap survives a save and load (craft block v2), as do the merchant purses
        {
          g.craft.live.purse[sk] = {g.day / 2 + 1, 17};
          std::vector<uint8_t> blob;
          g.craft.serialize(blob);
          craft::Knowledge k2;
          if (!k2.deserialize(blob)) fail("craft block v2 did not load");
          else {
            if (!k2.live.commissionAt.count(sk) || k2.live.commissionAt[sk] != g.craft.live.commissionAt[sk]) fail("a load reset the commission trust cap");
            if (!k2.live.purse.count(sk) || k2.live.purse[sk].second != 17) fail("a load refilled the merchant's purse");
          }
        }
        // one commission per restock: five restocks reach the patterns (60), the sixth adds the slower rate
        for (int i = 0; i < 6; i++) {
          g.day += 2;
          const int c = opt(g, "COMMISSION");
          if (c < 0) { fail("no COMMISSION option"); break; }
          g.dialogueChoose(c);
        }
      } else fail("could not open THE CRAFT with the smith");
      g.mode = Mode::Play;
      if (!g.craft.knows(hc->id, 0, craft::SecretKind::WeaponPattern) || !g.craft.knows(hc->id, 0, craft::SecretKind::ArmourPattern))
        fail("commissions over six restocks did not teach the patterns");
      // study a foreign piece: +25 to its maker's secret
      const uint64_t fid = g.world.src ? g.world.src->cultureAt(g.world.ox + 6000, g.world.oy + 6000) : 0;
      if (fid && craft::secretKey(fid) != craft::secretKey(hc->id)) {
        const cult::Culture& FC = g.world.src->culture(fid);
        g.inv.push_back(gear::makeGearC(r, ItemKind::Armor, 0, 5, Rarity::Common, &FC));
        if (talkTo(g, k) && opt(g, "THE CRAFT") >= 0) {
          g.dialogueChoose(opt(g, "THE CRAFT"));
          const int sOpt = opt(g, "STUDY");
          if (sOpt < 0) fail("no STUDY A FOREIGN PIECE with a foreign piece in the pack");
          else {
            g.dialogueChoose(sOpt);
            const int b2 = opt(g, "BREAK DOWN");
            if (b2 < 0) fail("no BREAK DOWN option");
            else {
              g.dialogueChoose(b2);
              if (g.craft.progress(fid, 0, craft::SecretKind::ArmourPattern) != craft::STUDY_PROGRESS) fail("studying a piece did not give +25");
            }
          }
        }
        g.mode = Mode::Play;
        // ruins lore: alloy notes advance an alloy by 50 when the alloy can be learned from ruins
        if (!FC.arms.alloys.empty()) {
          g.inv.push_back(craft::makeLore(FC, 1, craft::SecretKind::AlloyRecipe, 5));
          g.useItem((int)g.inv.size() - 1);
          const bool ruins = craft::howAllowed(FC, 1, craft::SecretKind::AlloyRecipe, craft::HOW_RUINS);
          const int pr = g.craft.progress(fid, 1, craft::SecretKind::AlloyRecipe);
          if (ruins && pr != craft::LORE_PROGRESS) fail("alloy notes did not advance the alloy by 50");
          if (!ruins && pr != 0) fail("alloy notes advanced an alloy its culture does not let ruins teach");
        }
      }
    }
    // the story engine's hook
    cult::Culture Q = cult::Atlas::make(cult::Archetype::Jade, 12);
    Q.id = cult::familyId(40, 40);
    if (!craft::learnSecret(g.craft, Q, 0, craft::SecretKind::WeaponPattern, 100, craft::HOW_QUEST)) fail("learnSecret (HOW_QUEST) did not teach");
  }
  return bad;
}

int gearCmd(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  for (int i = 2; i < argc; i++)
    if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc && !parseSeedRange(argv[++i], a, b)) { printf("bad --seeds\n"); return 2; }
  int bad = formulas() + itemLayout() + crafting() + generation();
  for (uint64_t s = a; s <= b; s++) bad += ores(s) + families(s) + play(s);
  printf("gear: %d failure(s)\n", bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--gear", "M6 Steel contracts: 7.x formulas, the SAVE_VER 12 item layout, crafting chains and secrets, regional ores, harpy / golem [--seeds A..B]", gearCmd);
