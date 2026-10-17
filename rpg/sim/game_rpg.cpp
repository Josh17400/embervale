// EMBERVALE RPG systems: inventory, loot, quests, dialogue, shops, resting, travel, saving.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/sim/game.h"
#include "rpg/sim/magic_ops.h"
#include "rpg/sim/lord_ops.h"
#include "rpg/sim/sea_ops.h"
#include "rpg/sim/service_ops.h"
#include "rpg/sim/skill_ops.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/home.h"
#include "rpg/sim/loot_ops.h"
#include "rpg/culture/culture.h"
#include "rpg/world/source.h"
#include "rpg/culture/society.h"
#include "rpg/story/story.h"

using art::Monster;
using art::Prop;

// (dialogue actions: DlgAct in rpg/sim/game_internal.h)

// ------------------------------------------------------------------ inventory
void Game::giveGold(int g) {
  gold += g;
  emit(Ev::Text, pl().p + Vec2(0, -20), (int)rgba(255, 220, 90), (float)g, "+" + std::to_string(g) + " GOLD");
  sfx((int)Sfx::Coin, pl().p, 0.9f + rng_.f() * 0.2f);
}

void Game::addItem(const Item& it0, bool announce) {
  Item it = it0;
  // (M8b, 15.27.2 / 15.27.5) the safety net under every source: a classic piece (item level 0, from an older maker)
  // becomes M6 gear at the player's level, with the affixes its rarity owes, so nothing above Common does nothing
  if (itemEquippable(it.kind) && it.ilvl == 0) gear::adoptClassic(it, plLevel);
  // (M6, 7.5 rule 7) at most 10 potions carried: the rest is left behind
  if (it.kind == ItemKind::Potion) {
    int held = 0;
    for (const Item& o : inv) if (o.kind == ItemKind::Potion) held += o.count;
    const int room = gear::POTION_CARRY - held;
    if (room <= 0) { if (announce) say("YOU CAN CARRY NO MORE THAN 10 POTIONS"); return; }
    it.count = std::min(it.count, room);
  }
  if (it.stackable()) {
    for (auto& o : inv)
      if (o.same(it)) {
        o.count += it.count;
        if (announce) { emit(Ev::Notice, pl().p, (int)rarityColor(it.rarity), 0, it.name + (it.count > 1 ? " x" + std::to_string(it.count) : "")); sfx((int)Sfx::Pickup, pl().p); }
        return;
      }
  }
  inv.push_back(it);
  int idx = (int)inv.size() - 1;
  if (announce) { emit(Ev::Notice, pl().p, (int)rarityColor(it.rarity), 0, it.name + (it.count > 1 ? " x" + std::to_string(it.count) : "")); sfx((int)Sfx::Pickup, pl().p); }
  // auto-equip into an empty slot (a legendary only while attunement has room: 7.3)
  int* slot = equipSlot(it.kind);
  if (slot && *slot < 0 && (!gear::attunes(it) || craft.live.stats.legendaries < gear::attunementSlots(plLevel))) { *slot = idx; recalcPlayer(); }
}

int* Game::equipSlot(ItemKind k) {
  switch (k) {
    case ItemKind::Weapon: return &eqWeapon;
    case ItemKind::Bow: return &eqBow;
    case ItemKind::Staff: return &eqStaff;
    case ItemKind::Armor: return &eqArmor;
    case ItemKind::Helmet: return &eqHelmet;
    case ItemKind::Shield: return &eqShield;
    case ItemKind::Ring: return &eqRing;
    case ItemKind::Amulet: return &eqAmulet;
    case ItemKind::Gloves: return &eqGloves;
    case ItemKind::Boots: return &eqBoots;
    case ItemKind::Cloak: return &eqCloak;
    default: return nullptr;
  }
}

void Game::useItem(int i) {
  if (i < 0 || i >= (int)inv.size()) return;
  Item& it = inv[i];
  auto toggle = [&](int& slot) {
    slot = slot == i ? -1 : i;
    recalcPlayer();
    sfx((int)Sfx::MenuSelect, pl().p);
  };
  if (int* slot = equipSlot(it.kind)) {
    // (M6, 7.3) attunement: legendaries worn at once are limited by level (1 at 10, 2 at 25, 3 at 40)
    if (*slot != i && gear::attunes(it)) {
      int worn = 0;
      for (int e : {eqWeapon, eqBow, eqStaff, eqArmor, eqHelmet, eqShield, eqRing, eqAmulet, eqGloves, eqBoots, eqCloak})
        if (e >= 0 && e < (int)inv.size() && e != *slot && gear::attunes(inv[(size_t)e])) worn++;
      const int slots = gear::attunementSlots(plLevel);
      if (worn >= slots) {
        // (M8b integration) Mythic pieces attune too: the notice names the piece's own rarity, and counts "great pieces"
        const std::string kindWord = it.rarity == Rarity::Mythic ? "A MYTHIC" : "A LEGENDARY";
        say(slots == 0 ? kindWord + " ANSWERS ONLY FROM LEVEL 10: YOU CANNOT ATTUNE TO IT YET"
                       : "ATTUNED TO " + std::to_string(worn) + (worn == 1 ? " GREAT PIECE" : " GREAT PIECES") + " ALREADY (LEVEL " +
                             std::to_string(plLevel) + " ALLOWS " + std::to_string(slots) + "): TAKE ONE OFF FIRST");
        sfx((int)Sfx::MenuBack, pl().p);
        return;
      }
    }
    toggle(*slot);
    return;
  }
  switch (it.kind) {
    case ItemKind::Potion: {
      // (M6, 7.5 rule 7) one shared 8 s cooldown; potion effect affixes; Lifebloom heals 10 % more over 10 s
      gear::Live& L6 = craft.live;
      if (L6.potionCd > 0) {
        say("POTION READY IN " + std::to_string((int)std::ceil(L6.potionCd)) + " S");
        sfx((int)Sfx::MenuBack, pl().p, 0.8f);
        return;
      }
      const float k = 1.0f + 0.01f * (float)L6.stats.get(Affix::PotionEffect);
      if (it.sub == (uint8_t)PotionType::Health) { pl().hp = std::min(pl().maxHp, pl().hp + it.power * k); emit(Ev::Heal, pl().p); }
      else if (it.sub == (uint8_t)PotionType::Magicka) mp += it.power * k;
      else stamina += it.power * k;
      if (L6.stats.has(Unique::Lifebloom)) { L6.bloomT = 10.0f; L6.bloomRate = pl().maxHp * 0.10f / 10.0f; }
      L6.potionCd = L6.potionCdMax = gear::POTION_COOLDOWN;
      sfx((int)Sfx::Heal, pl().p, 1.2f);
      break;
    }
    case ItemKind::Food: {
      // M5 (15.2): a snack's Well Fed; (M7) a cooked meal's is its own, set by where it was cooked (home_game.cpp), never
      // the snack rule's (it gave a campfire stew the hearth's hours and pips)
      if (it.sub >= home::FOOD_MEAL) homeAte(it); else lifeAte(it);
      float k = background == Background::Farmhand ? 1.5f : 1.0f;   // farmhand: plain food goes further
      pl().hp = std::min(pl().maxHp, pl().hp + it.power * k);
      stamina += it.power * k;
      sfx((int)Sfx::Pickup, pl().p, 0.7f);
      emit(Ev::Heal, pl().p);
      break;
    }
    case ItemKind::Misc:
      if (craft::isLore(it)) {   // (M6, 15.11) alloy notes or a mould from a ruin: study it (HOW_RUINS)
        if (!world.src || !it.culture) return;
        const cult::Culture& C = world.src->culture(it.culture);
        const craft::SecretKind sk = (craft::SecretKind)std::min<int>(it.form, (int)craft::SecretKind::COUNT - 1);
        const std::string what = craft::secretName(C, it.alloy, sk);
        if (craft.knows(C.id, it.alloy, sk)) { say("YOU KNOW " + what + " ALREADY"); return; }
        if (!craft::howAllowed(C, it.alloy, sk, craft::HOW_RUINS)) { say("THE NOTES ARE TOO WORN TO READ"); return; }
        const bool learned = craft::learnSecret(craft, C, it.alloy, sk, craft::LORE_PROGRESS, craft::HOW_RUINS);
        if (learned) { emit(Ev::QuestUpdate, pl().p, 0, 1, "LEARNED " + what); sfx((int)Sfx::LevelUp, pl().p, 1.2f); }
        else say("YOU STUDY THE NOTES: " + what + " " + std::to_string(craft.progress(C.id, it.alloy, sk)) + "%");
        break;
      }
      if (it.name.rfind("SPELL TOME", 0) == 0) {
        spellsKnown |= (uint8_t)(1 << it.sub);
        say(std::string("LEARNED ") + spellName((Spell)it.sub));
        sfx((int)Sfx::LevelUp, pl().p, 1.3f);
        break;
      }
      return;
    case ItemKind::Book:   // (M9) a skill book (SKILLS lane) or a spell tome (MAGIC lane): read once, then gone
      if (!(skills::isSkillBook(it) ? SkillOps::readBook(*this, i) : MagicOps::readTome(*this, i))) return;
      break;
    default: return;
  }
  it.count--;
  if (it.count <= 0) dropItem(i);
}

void Game::dropItem(int i) {
  if (i < 0 || i >= (int)inv.size()) return;
  inv.erase(inv.begin() + i);
  auto fix = [&](int& e) { if (e == i) e = -1; else if (e > i) e--; };
  fix(eqWeapon); fix(eqBow); fix(eqStaff); fix(eqArmor); fix(eqHelmet); fix(eqShield); fix(eqRing); fix(eqAmulet);
  fix(eqGloves); fix(eqBoots); fix(eqCloak);
  recalcPlayer();
}

namespace {
// (M3c LIFE) a hide or part of the Wildlands wildlife: a misc good of the base kind (8 hide, 9 pelt, 6 gem-like, 1 bone,
// 10 herb) under its own name, value and tint
Item wildGood(int base, const char* name, art::Icon icon, int value, uint32_t tint) {
  Item it = makeMisc(base);
  it.name = name; it.icon = icon; it.value = value; it.tint = tint;
  return it;
}
}  // namespace

void Game::dropLoot(const Actor& a) {
  if (a.npc) return;
  Rng r(hash32((uint32_t)a.id * 7919u) ^ (uint32_t)seed ^ (uint32_t)(time * 10));
  // (M6, 7.2 / 7.6) what a body drops is rolled by gear::rollDrop at the danger it fell at, its rank and its maker culture:
  // a soldier's or a bandit's own (Actor::culture, the FOES lane), else the land's
  gear::DropSource ds;
  ds.D = std::clamp(a.dropD ? a.dropD : a.level, 1, gear::MAX_D);
  ds.rank = a.rank;
  ds.playerLevel = plLevel;
  ds.culture = a.culture;
  if (!ds.culture) {
    if (inside && subSite >= 0) { if (const cult::Culture* c = world.cultureOf(subSite)) ds.culture = c->id; }
    else if (const cult::Culture* c = world.cultureAtTile((int)std::floor(a.p.x / TILE), (int)std::floor(a.p.y / TILE))) ds.culture = c->id;
  }
  const cult::Culture* maker = ds.culture && world.src ? &world.src->culture(ds.culture) : nullptr;
  // (M8b, 15.27.4) the source by how hard the foe was: a world boss's hoard is Epic or better, a dungeon boss's drop is
  // boosted a step, a named unique's is Rare or better (7.6), every other body an ordinary roll (an elite's +1 roll is
  // rollDrop's own)
  auto loot = [&](bool boss) {
    gear::DropSource s2 = ds;
    if (boss && s2.rank < 4) s2.rank = 4;
    if (s2.rank >= 5) { s2.src = gear::LootSrc::WorldBoss; s2.atLeast = Rarity::Epic; }
    else if (s2.rank == 4) { s2.src = gear::LootSrc::Boss; s2.boost = 1; }
    else if (s2.rank == 3) { s2.src = gear::LootSrc::Body; s2.atLeast = Rarity::Rare; }
    else s2.src = gear::LootSrc::Body;
    Item it = gear::rollDropC(r, s2, maker);
    return it;
  };
  auto drop = [&](const Item& it) {
    // (draws sequenced: y first, as MSVC/GCC evaluate the Vec2 arguments right to left; clang goes left to right)
    const float jy = r.range(-6, 6), jx = r.range(-8, 8);
    Pickup k; k.p = a.p + Vec2(jx, jy); k.item = it; pickups.push_back(k);
  };
  auto dropGold = [&](int g) { const float jy = r.range(-4, 4), jx = r.range(-6, 6); Pickup k; k.p = a.p + Vec2(jx, jy); k.gold = g; pickups.push_back(k); };
  if (a.human) {
    dropGold(5 + r.irange(10 + a.level * 3) + (a.boss ? 60 + a.level * 10 : 0));
    if (r.f() < 0.35f || a.boss || a.rank >= 1) drop(loot(a.boss));
    if (a.ranged && r.f() < 0.7f) drop(makeArrows(3 + r.irange(5)));
    return;
  }
  switch (a.mon) {
    case Monster::Wolf: case Monster::IceWolf: if (r.f() < (background == Background::Hunter ? 1.0f : 0.6f)) drop(makeMisc(0)); break;
    case Monster::Boar: if (r.f() < 0.6f) drop(makeFood(1)); break;
    case Monster::Bear: drop(makeMisc(0)); if (background == Background::Hunter) drop(makeMisc(0)); if (r.f() < 0.5f) drop(makeFood(1)); break;
    case Monster::Spider: case Monster::FrostSpider: if (r.f() < 0.5f) drop(makeMisc(5)); break;
    case Monster::Skeleton: case Monster::Draugr: if (r.f() < 0.5f) drop(makeMisc(1)); if (r.f() < 0.3f) dropGold(3 + r.irange(12)); if (r.f() < 0.15f) drop(loot(false)); break;
    case Monster::Troll: drop(makeMisc(4)); break;
    case Monster::Goblin: dropGold(2 + r.irange(8)); if (r.f() < 0.2f) drop(loot(false)); break;
    case Monster::Wraith: if (r.f() < 0.4f) drop(makeMisc(6)); break;
    case Monster::Mudcrab: if (r.f() < 0.3f) drop(makeFood(1)); break;
    // (M3c LIFE) the Wildlands wildlife: hides and parts for M6 crafting (biomes.h HD_CHITIN, HD_SPOTTED, HD_SCALE, HD_FUR;
    // the wisp's, the hound's and the blight's goods are the magic lands' reagents). Misc goods stack by name.
    case Monster::Scorpion:
      if (r.f() < (background == Background::Hunter ? 0.9f : 0.6f)) drop(wildGood(8, "SCORPION CHITIN", art::Icon::Pelt, 16, rgba(196, 140, 72)));
      if (r.f() < 0.25f) drop(wildGood(1, "SCORPION STINGER", art::Icon::Bone, 24, rgba(120, 60, 50)));
      break;
    case Monster::Hyena:
      if (r.f() < (background == Background::Hunter ? 1.0f : 0.55f)) drop(wildGood(8, "SPOTTED HIDE", art::Icon::Pelt, 12, rgba(196, 168, 110)));
      if (r.f() < 0.2f) drop(makeFood(1));
      break;
    case Monster::Lurker:
      if (r.f() < (background == Background::Hunter ? 1.0f : 0.7f)) drop(wildGood(8, "LURKER SCALES", art::Icon::Pelt, 28, rgba(84, 116, 66)));
      if (r.f() < 0.35f) drop(makeFood(1));
      break;
    case Monster::Yeti:
      drop(wildGood(9, "YETI FUR", art::Icon::Pelt, 48, rgba(226, 234, 246)));
      if (background == Background::Hunter) drop(wildGood(9, "YETI FUR", art::Icon::Pelt, 48, rgba(226, 234, 246)));
      if (r.f() < 0.3f) drop(makeFood(1));
      break;
    case Monster::Wisp: if (r.f() < 0.45f) drop(wildGood(6, "WISP ESSENCE", art::Icon::Gem, 60, rgba(120, 230, 230))); break;
    case Monster::EmberHound:
      if (r.f() < (background == Background::Hunter ? 0.9f : 0.5f)) drop(wildGood(9, "EMBER HIDE", art::Icon::Pelt, 36, rgba(90, 40, 34)));
      if (r.f() < 0.2f) drop(wildGood(6, "CINDER HEART", art::Icon::Gem, 72, rgba(255, 140, 50)));
      break;
    case Monster::Blightspawn:
      if (r.f() < 0.5f) drop(wildGood(10, "BLIGHTED THORN", art::Icon::Herb, 18, rgba(120, 70, 120)));
      if (r.f() < 0.2f) drop(makeMisc(10));
      if (r.f() < 0.15f) dropGold(4 + r.irange(10));   // (what it caught up in its roots)
      break;
    default: break;
  }
  // (M6) a world boss (foes::Rank::WorldBoss = 5) pays its own hoard below, never the dungeon boss's purse (every
  // non-dragon world boss is also a.boss: FoeOps::spawnWorldBoss)
  const bool worldBoss = a.rank == 5;
  if (a.boss && a.mon != Monster::Dragon && !worldBoss) { dropGold(40 + a.level * 12); drop(loot(true)); }
  if (!a.human && !a.boss && a.mon != Monster::Dragon) {
    // (M6) ranked beasts carry more: an elite or champion often, a named unique always (Rare+, rollDrop's rules)
    if (a.rank >= 3) drop(loot(false));
    else if (a.rank >= 1 && r.f() < 0.5f) drop(loot(false));
    // (M6, 15.11) hunting gives raw hides for the tanner (the hide -> leather chain)
    const bool hideBeast = a.mon == Monster::Wolf || a.mon == Monster::IceWolf || a.mon == Monster::Bear || a.mon == Monster::Boar ||
                           a.mon == Monster::Hyena || a.mon == Monster::Yeti || a.mon == Monster::Lurker || a.mon == Monster::EmberHound;
    if (hideBeast && r.f() < (background == Background::Hunter ? 0.8f : 0.5f)) drop(craft::makeStuff(craft::Stuff::Hide, a.mon == Monster::Bear || a.mon == Monster::Yeti ? 2 : 1));
  }
  // (M6 integration) a dragon world boss (foes::Rank::WorldBoss = 5) is not Ashfang: a world boss's hoard, not the
  // main quest's crown and greatsword
  if (worldBoss) { dropGold(400 + a.level * 20); drop(loot(true)); drop(loot(true)); }
  if (a.mon == Monster::Dragon && !worldBoss) {
    dropGold(1500);
    // the dragon's hoard: two legendaries of the world boss (VISION 7.6), named as the main quest tells them
    const int il = std::clamp(a.level + 2, 10, gear::MAX_D);
    Item crown = gear::makeGearC(r, ItemKind::Amulet, 0, il, Rarity::Legendary, nullptr);
    crown.name = "EMBER CROWN"; crown.icon = art::Icon::Crown; crown.unique = Unique::Ward; crown.ench = Ench::Fortify; crown.enchPow = 80; crown.value = 5000;
    drop(crown);
    Item w = gear::makeGearC(r, ItemKind::Weapon, (int)WeaponType::Greatsword, il, Rarity::Legendary, nullptr);
    w.name = "DRAGONBONE GREATSWORD"; w.unique = Unique::Dragonbane; w.ench = Ench::Fire; w.enchPow = 30;
    drop(w);
  }
  // (M9) a skill book or a rare tome may lie among the loot (SKILLS / MAGIC lanes)
  SkillOps::loot(*this, a.p, ds.D, gear::LootSrc::Body);
  MagicOps::loot(*this, a.p, ds.D, gear::LootSrc::Body);
}

void Game::openChest(int tx, int ty) {
  if (questChest(tx, ty)) return;   // M2: an Heirloom's chest holds the keepsake (quests.cpp)
  Map& m = map();
  size_t i = (size_t)ty * m.w + tx;
  m.prop[i] = (uint8_t)((int)Prop::ChestOpen + 1);
  const uint64_t lk = lootKey(tx, ty);
  looted.insert(lk);
  m.rebuildSolid();
  sfx((int)Sfx::Chest, pl().p);
  int lvl = inside && subSite >= 0 ? world.sites[subSite].level : world.zoneLevel(tx, ty);
  Rng r(world.endless ? (uint32_t)ew::mix64(lk ^ seed) : hash2(tx, ty, (uint32_t)seed + mapKey()));
  bool rich = inside && subSite >= 0;
  int n = 1 + r.irange(rich ? 3 : 2);
  // (M8b) the find is laid out in a small fan on the open floor before the chest (its front, else a free side), each
  // piece in its own place, never piled on the lid or on the wall behind
  int fdx = 0, fdy = 1;
  {
    static const int sides[4][2] = {{0, 1}, {-1, 0}, {1, 0}, {0, -1}};
    for (const auto& s : sides)
      if (!m.blocked(tx + s[0], ty + s[1])) { fdx = s[0]; fdy = s[1]; break; }
  }
  // (a piece's sprite stands above its point: in front of the chest the point lies nearly two tiles down, so the
  // 16 px icon stands clear of the chest's front face)
  // (M8b fixer r2, review: "the hoard's sword, gold and amulet sit in one overlapping stack against the chest") the
  // fan is wider (icons 18 px apart, rows 14 px apart), the gold takes its own place in it, and a place on a blocked
  // tile moves on to the next one
  Vec2 c(tx * TILE + 8.0f + (float)fdx * 20.0f, ty * TILE + 8.0f + (fdy > 0 ? 28.0f : fdy < 0 ? -14.0f : 6.0f));
  int slot = 0;
  auto place = [&](float jx, float jy) {
    static const float fan[10][2] = {{0, 0}, {-18, 0}, {18, 0}, {-9, 14}, {9, 14}, {-27, 10}, {27, 10}, {0, 26}, {-18, 26}, {18, 26}};
    Vec2 first;
    for (int tries = 0; tries < 10; tries++) {
      const float* f = fan[slot++ % 10];
      // along the free side: forward is (fdx, fdy), across is (-fdy, fdx)
      const float ax = f[0] + jx * 0.12f, fw = f[1] + jy * 0.12f;
      const Vec2 q = c + Vec2(-(float)fdy * ax + (float)fdx * fw, (float)fdx * ax + (float)fdy * fw);
      if (tries == 0) first = q;
      if (!m.blocked((int)std::floor(q.x / TILE), (int)std::floor(q.y / TILE))) return q;
    }
    return first;
  };
  // a spot by the chest (draws sequenced: y first, as MSVC/GCC evaluate the Vec2 arguments right to left)
  auto spot = [&]() {
    const float jy = r.range(0, 8), jx = r.range(-8, 8);
    return place(jx, jy);
  };
  // (M6) the chest's loot by the place's danger (gear::rollDrop) in the make of the land's (or the dungeon's) culture
  const cult::Culture* cc = inside && subSite >= 0 ? world.cultureOf(subSite) : (inside ? nullptr : world.cultureAtTile(tx, ty));
  if (cc && world.src) cc = &world.src->culture(cc->id);
  // (M8b, 15.27.4) what kind of find it is (a dungeon's hoard, an exploration cache, a plain chest): LootOps::chestSource
  gear::DropSource ds = LootOps::chestSource(*this, tx, ty);
  ds.culture = cc ? cc->id : 0;
  const bool hoard = ds.src == gear::LootSrc::Hoard;
  for (int k = 0; k < n; k++) {
    Pickup p; p.p = spot();
    gear::DropSource s2 = ds;
    // a dungeon chest's first piece may be a boss's roll; the hoard by the boss always is (the draw is kept either way)
    const bool bossRoll = rich && k == 0 && r.f() < 0.35f;
    if (bossRoll || (hoard && k == 0)) s2.rank = 4;
    p.item = gear::rollDropC(r, s2, cc);
    pickups.push_back(p);
  }
  Pickup g; g.p = place(0, 0); g.gold = 8 + r.irange(15 + lvl * 4) + (rich ? 20 : 0); pickups.push_back(g);
  // (M6, 15.11) mining country's chests hold ore (the land's richest), a ruin's may hold an alloy's notes or a mould
  if (world.src) {
    int ptx = tx, pty = ty;
    if (inside) overworldTile(*this, ptx, pty);
    const int32_t gx = world.ox + ptx, gy = world.oy + pty;
    if (!(inside && subBldg >= 0) && r.f() < 0.4f) {
      const std::vector<craft::OreSource> ores = craft::oresAt(*world.src, gx, gy);
      if (!ores.empty() && ores[0].richness >= 140) {
        Pickup o; o.p = spot();
        o.item = craft::makeStuff(craft::oreStuff(ores[0].ore), 2 + r.irange(3));
        pickups.push_back(o);
      }
    }
    if (inside && subSite >= 0 && world.sites[(size_t)subSite].type == SiteType::Ruin && cc && cc->id && r.f() < 0.3f) {
      // (M6 fixer r3) only alloys whose secret a ruin can teach (craft::howAllowed HOW_RUINS); otherwise a pattern mould
      std::vector<uint8_t> readable;
      for (size_t ai = 0; ai < cc->arms.alloys.size(); ai++)
        if (craft::howAllowed(*cc, (uint8_t)(ai + 1), craft::SecretKind::AlloyRecipe, craft::HOW_RUINS)) readable.push_back((uint8_t)(ai + 1));
      const uint8_t al = !readable.empty() && r.f() < 0.6f ? readable[(size_t)r.irange((int)readable.size())] : 0;
      const craft::SecretKind sk = al ? craft::SecretKind::AlloyRecipe : (r.f() < 0.5f ? craft::SecretKind::WeaponPattern : craft::SecretKind::ArmourPattern);
      Pickup o; o.p = spot();
      o.item = craft::makeLore(*cc, al, sk, r.next());
      pickups.push_back(o);
    }
  }
  // (M9) a skill book or a rare tome may lie in the chest (SKILLS / MAGIC lanes)
  SkillOps::loot(*this, spot(), ds.D, ds.src);
  MagicOps::loot(*this, spot(), ds.D, ds.src);
}

gear::DropSource LootOps::chestSource(const Game& g, int tx, int ty) {
  gear::DropSource ds;
  const bool dungeon = g.inside && g.subSite >= 0 && g.subSite < (int)g.world.sites.size() && g.subBldg < 0;
  const int lvl = g.inside && g.subSite >= 0 && g.subSite < (int)g.world.sites.size() ? g.world.sites[(size_t)g.subSite].level : g.world.zoneLevel(tx, ty);
  ds.D = std::clamp(lvl, 1, gear::MAX_D);
  ds.chest = true;
  ds.playerLevel = g.plLevel;
  ds.src = gear::LootSrc::Chest;
  if (dungeon) {
    // the hoard: the chest the dungeon's boss guards (a cave's lair chest beside it, a ruin's by the altar of its
    // room): the nearest chest to a boss's spawn, within 8 tiles
    const Map& m = g.map();
    bool byBoss = false;
    for (const Spawn& s : m.spawns) {
      if (!s.boss) continue;
      const int d2 = (s.x - tx) * (s.x - tx) + (s.y - ty) * (s.y - ty);
      if (d2 > 64) continue;
      bool nearer = false;
      for (int y = std::max(0, s.y - 8); y <= std::min(m.h - 1, s.y + 8) && !nearer; y++)
        for (int x = std::max(0, s.x - 8); x <= std::min(m.w - 1, s.x + 8); x++) {
          if (x == tx && y == ty) continue;
          const int p = m.propAt(x, y);
          if (p != (int)Prop::Chest + 1 && p != (int)Prop::ChestOpen + 1) continue;
          if ((s.x - x) * (s.x - x) + (s.y - y) * (s.y - y) < d2) { nearer = true; break; }
        }
      if (!nearer) { byBoss = true; break; }
    }
    const SiteType st = g.world.sites[(size_t)g.subSite].type;
    if (byBoss) { ds.src = gear::LootSrc::Hoard; ds.boost = 1; }
    else if (st == SiteType::Ruin || st == SiteType::Shrine) { ds.src = gear::LootSrc::Explore; ds.boost = 1; }   // a ruin's cache
    // a harder dungeon (beyond the hero: 15.27.4) pays a step more
    if (ds.D >= g.plLevel + 3) ds.boost = (int8_t)(ds.boost + 1);
  } else if (!g.inside) {
    // off the road: a chest at a wayside place (the watchtower's foot, the caravan), a wonder, a ruin or a shrine
    const int si = g.world.siteAt(tx, ty, 2);
    if (si >= 0) {
      const SiteType st = g.world.sites[(size_t)si].type;
      if (st == SiteType::Vignette || st == SiteType::Wonder || st == SiteType::Ruin || st == SiteType::Shrine) { ds.src = gear::LootSrc::Explore; ds.boost = 1; }
    }
  }
  return ds;
}

// ------------------------------------------------------------------ interaction
// (fixer M4 r3) a fight comes first: with an awake foe (a raiding wolf, a soldier of the enemy) in striking reach the
// button is ATTACK, never TALK to the guard who came running to stand beside the hero, nor a use of a prop (phone
// players have no other way to swing)
bool Game::foeInReach() const {
  const Actor& p = pl();
  for (size_t i = 1; i < actors.size(); i++) {
    const Actor& a = actors[i];
    if (a.st == AState::Dead || a.asleep || a.player) continue;
    // (a beast that is fighting: a raider at a villager, a guard's foe; an idle cave dweller beside a captive does not
    // turn the captive's TALK into a swing)
    const bool foe = (a.hostile && a.aggro) || (a.aggro && a.target == p.id) || (a.npc && warFoes(*this, a, p));
    if (foe && len2(a.p - p.p) < 34.0f * 34.0f) return true;
  }
  return false;
}

int Game::interactTarget() const {
  const Actor& p = pl();
  if (foeInReach()) return -1;
  // (fixer M10 r1) aboard, the deck's own verb (DISEMBARK, MOOR, FISH) outranks the crowd on the pier: no TALK flicker
  if (sea.atSea() && !SeaOps::interactLabel(*this).empty()) return -1;
  int best = -1; float bd = 24 * 24;
  // (M7) in the player's own yard, a crop / coop / stable in front wins over a passer-by (the farm's own actors stay)
  const bool yard = !inside && !home.plots.empty() && home::yardUseAhead(*this);
  for (size_t i = 1; i < actors.size(); i++) {
    const Actor& a = actors[i];
    if (!a.npc || a.st == AState::Dead) continue;
    if (yard && !(a.lifeBits & LB_HOMESTEAD)) continue;
    // (M4 integration) a soldier or guard who is the player's foe, or is fighting the player, is not talked to: the
    // tap and E attack them instead of offering TALK mid-fight
    if ((a.aggro && a.target == p.id) || warFoes(*this, a, p)) continue;
    float d = len2(a.p - p.p);
    // shopkeepers behind counters can be reached across them
    float reach = (a.role == Role::Merchant || a.role == Role::Innkeeper || a.role == Role::Smith) ? 40.0f : 24.0f;
    if (d >= reach * reach) continue;
    // (stall facings) a keeper at their counter is talked to across it, from the customers' side (not through its back)
    if (a.stallKeeper && len2(a.p - a.home) < 4.0f) {
      const Vec2 fv = faceVec(a.face);
      if ((p.p.x - a.p.x) * fv.x + (p.p.y - a.p.y) * fv.y < 2.0f) continue;
    }
    // someone with a reward waiting or work to offer wins over a bystander about as close (two villagers side by
    // side: the prompt and the tap go to the one with something to say)
    if (rewardWaiting(a) || hasOffer(a)) d *= 0.45f;
    // (M7c merge, m7_homestead_ride) the player's own horse waiting beside them, turned to, takes the prompt from a
    // passer-by about as near: in the M7c villages a street's foot traffic brushes past the spot where the rider got
    // off, and E: TALK <stranger> stole the RIDE the player had turned round for
    if (a.slot == HOME_SLOT_HORSE && a.critter && (a.lifeBits & LB_HOMESTEAD) && home.riding < 0) {
      const Vec2 to = a.p - p.p;
      if (to.x * p.aim.x + to.y * p.aim.y > 0.5f * std::sqrt(std::max(1.0f, d))) d *= 0.45f;
    }
    // (fixer M8 r2, review: "E: TALK <companion> follows them everywhere") a companion at heel is always the nearest
    // person: they take the prompt only when the player turns to them, and anyone else about as near comes first
    if (a.comp && a.comp <= party.members.size() && party.members[(size_t)a.comp - 1].state == party::State::With) {
      const Vec2 to = a.p - p.p;
      if (to.x * p.aim.x + to.y * p.aim.y < 0.35f * std::sqrt(std::max(1.0f, d))) continue;
      d = std::min(d * 2.5f, 24.0f * 24.0f - 1.0f);
    }
    if (d < bd + (reach * reach - 24 * 24)) { bd = d; best = a.id; }
  }
  return best;
}

bool Game::nearDoorOrExit() const { return false; }

// M0b: a night in a bed lasts until morning: from the afternoon on you wake around 06:00-07:00 (never less than 8
// hours); a nap in the day is 8 hours
static int hoursToMorning(float hour) {
  if (hour >= 15.0f || hour < 6.0f) {
    float until = 6.5f - hour;
    if (until < 0) until += 24.0f;
    return std::max(8, (int)std::lround(until));
  }
  return 8;
}

// M0b fix round 2: hours of sleep in your rented room. On the morning it is due back you wake by 11:00 (the
// innkeeper would knock); from 11:00 on that day there is no time left to sleep (0)
static int lodgingSleepHours(int day, float hour, int untilDay) {
  int h = hoursToMorning(hour);
  if (day == untilDay) {
    if (hour >= 11.0f) return 0;
    h = std::max(1, std::min(h, (int)std::floor(11.0f - hour)));
  } else if (day + 1 == untilDay && hour >= 15.0f) {
    // a night's sleep never runs past 11:00 on the due morning either
    h = std::min(h, (int)std::floor(24.0f - hour + 11.0f));
  }
  return h;
}
static const char* kRoomDue = "IT IS NEARLY NOON: YOUR ROOM IS DUE BACK.";

bool Game::bedIsYours(int tx, int ty) const {
  if (homeBed(tx, ty)) return true;   // M7: a bed in the player's own house
  if (!inside || subBldg < 0) return true;
  const Bldg& B = world.over.bldgs[subBldg];
  if (B.type != art::Building::Inn || B.genVer < WORLDGEN_V7) return true;
  return lodgingActive() && lodging.bldg == subBldg && lodging.floor == subFloor && lodging.room == sub.roomIndexAt(tx, ty);
}

int Game::interactProp(int& otx, int& oty) const {
  const Actor& p = pl();
  const Map& m = map();
  Vec2 probe = p.p + p.aim * 12.0f + Vec2(0, -4);
  for (int r = 0; r <= 1; r++)
    for (int oy = -r; oy <= r; oy++)
      for (int ox = -r; ox <= r; ox++) {
        int tx = (int)std::floor(probe.x / TILE) + ox, ty = (int)std::floor(probe.y / TILE) + oy;
        int pr = m.propAt(tx, ty);
        if (!pr) continue;
        Prop prop = (Prop)(pr - 1);
        bool usable = prop == Prop::Chest || prop == Prop::Shrine || prop == Prop::Altar || prop == Prop::BerryBush ||
                      (prop == Prop::SummitShrine && !inside) ||   // M11: the shrine at the top of a peak's climb
                      ((prop == Prop::StandingStone || prop == Prop::GraveCairn) && !inside) ||   // M2 wayside (wayside.cpp)
                      (prop == Prop::Signpost && !inside && world.siteAt(tx, ty, 3) >= 0) ||
                      ((prop == Prop::Hammock || prop == Prop::SleepingMat) && inside && subBldg >= 0) ||   // M3: cultures that sleep so
                      (prop == Prop::Bedroll && !inside) ||   // M5: a wayfarer's bedroll (Life: rested, the night passes)
                      (art::isLoreProp(prop) && (inside ? subSite >= 0 : (prop == Prop::NoticeBoard || prop == Prop::ToppledStatue))) ||   // M4 (story_game.cpp)
                      homePropUsable(prop, tx, ty) ||   // M7: cooking places, the player's own furniture (home_game.cpp)
                      (prop == Prop::Bed && inside && subBldg >= 0 &&
                       (world.over.bldgs[subBldg].type != art::Building::Inn || world.over.bldgs[subBldg].genVer >= WORLDGEN_V7));
        if (!usable) continue;
        otx = tx; oty = ty;
        return pr;
      }
  return 0;
}

void Game::interact() {
  if (partyInteract()) return;   // M8: bind a downed companion's wounds, a grave of the fallen (party_game.cpp)
  int t = interactTarget();
  if (t >= 0) { talkTo(actors[findActor(t)]); return; }
  if (homeInteract()) return;   // M7: the player's own yard (till, plant, water, harvest, collect, mount; home_game.cpp)
  if (SeaOps::interact(*this)) return;   // M10: board / disembark, cast a line, a pier's use (sea_game.cpp)
  // chest / shrine / bed / bush in front of the player
  const Actor& p = pl();
  int tx = 0, ty = 0;
  int pr = interactProp(tx, ty);
  if (!pr) return;
  Prop prop = (Prop)(pr - 1);
  if (homeUseProp(prop, tx, ty)) return;   // M7: the player's own stores, beds and cooking places (home_game.cpp)
  if (prop == Prop::Chest) { openChest(tx, ty); return; }
  if (lifeUseProp(prop, tx, ty)) return;   // M5: beds and bedrolls (Rested), the townsfolk's furniture (life_game.cpp)
  // M4: a story may ask for a wayside prop (standing stones, a cairn) before its own use runs
  if (prop == Prop::StandingStone || prop == Prop::GraveCairn) story.onUseProp(*this, (int)prop, tx, ty);
  if (useWaysideProp(prop, tx, ty)) return;
  if (storyUseProp(prop, tx, ty)) return;   // M4: notice boards, inscriptions, graves, journals (story_game.cpp)
  if (prop == Prop::SummitShrine) {
    // M11: the summit shrine pays the climb back: its god's blessing for twice as long, full health and a
    // "summit" word so the player knows this one is stronger than a wayside shrine's
    int si = world.siteAt(tx, ty, 2);
    blessName = "BLESSING OF THE SUMMIT";
    if (si >= 0 && world.sites[si].type == SiteType::Shrine && world.sites[si].name.rfind("SHRINE OF ", 0) == 0)
      blessName = "BLESSING OF " + world.sites[si].name.substr(10);
    deed(party::Deed::Prayed, si >= 0 ? world.sites[(size_t)si].id : 0);
    blessT = blessingSecs() * 2.0f;
    pl().hp = pl().maxHp;
    recalcPlayer();
    say(blessName + " (SUMMIT): +ARMOR, FASTER HEALING, TWICE AS LONG");
    emit(Ev::Heal, p.p);
    sfx((int)Sfx::Heal, p.p, 1.0f);
    return;
  }
  if (prop == Prop::Shrine || prop == Prop::Altar) {
    static const char* bless[] = {"BLESSING OF SOLMIR", "BLESSING OF VEYNA", "BLESSING OF HALDRUN", "BLESSING OF ORISSA"};
    blessName = bless[hash2(tx, ty) % 4];
    int si = inside ? -1 : world.siteAt(tx, ty, 1);   // a wayside shrine blesses in its own god's name
    if (si >= 0 && world.sites[si].type == SiteType::Shrine && world.sites[si].name.rfind("SHRINE OF ", 0) == 0)
      blessName = "BLESSING OF " + world.sites[si].name.substr(10);
    deed(party::Deed::Prayed, si >= 0 ? world.sites[(size_t)si].id : 0);   // M8: the companions see it
    blessT = blessingSecs();
    pl().hp = pl().maxHp;
    recalcPlayer();
    say(blessName + (background == Background::Novice ? ": +ARMOR, FASTER HEALING (LONGER FOR A NOVICE)" : ": +ARMOR, FASTER HEALING"));
    emit(Ev::Heal, p.p);
    sfx((int)Sfx::Heal, p.p, 0.8f);
    return;
  }
  if (prop == Prop::BerryBush) {
    Item a = makeFood(2);
    addItem(a);
    map().setProp(tx, ty, Prop::Bush);
    return;
  }
  if (prop == Prop::Signpost) {
    int si = world.siteAt(tx, ty, 3);
    if (si >= 0) say("WELCOME TO " + world.sites[si].name);
    return;
  }
  if (prop == Prop::Hammock || prop == Prop::SleepingMat) {   // M3: a home's hammock or sleeping mat (inns keep beds)
    rest(hoursToMorning(hour));
    say("YOU SLEEP SOUNDLY.");
    return;
  }
  if (prop == Prop::Bed) {
    // M0b: an inn's beds upstairs are let room by room; only the one you rented is yours to sleep in
    const Bldg& B = world.over.bldgs[subBldg];
    if (B.type == art::Building::Inn && B.genVer >= WORLDGEN_V7) {
      int ri = sub.roomIndexAt(tx, ty);
      RoomKind k = ri >= 0 && ri < (int)sub.rooms.size() ? sub.rooms[(size_t)ri].kind : RoomKind::Common;
      bool mine = lodgingActive() && lodging.bldg == subBldg && lodging.floor == subFloor && lodging.room == ri;
      if (mine) {
        // a lie-in on the morning the room is due back ends before noon (the innkeeper would knock)
        int h = lodgingSleepHours(day, hour, lodging.untilDay);
        if (h <= 0) { say(kRoomDue); return; }
        rest(h);
        say("YOU SLEEP SOUNDLY IN YOUR ROOM.");
        return;
      }
      if (k == RoomKind::OwnerRoom) { say("THAT IS THE INNKEEPER'S OWN BED."); return; }
      say(lodgingActive() && lodging.bldg == subBldg ? "THIS ROOM IS TAKEN. YOURS IS ANOTHER." : "THIS ROOM IS TAKEN. RENT ONE FROM THE INNKEEPER.");
      return;
    }
    rest(hoursToMorning(hour));
    say("YOU SLEEP SOUNDLY.");
    return;
  }
}

// M1: an NPC's identity from stable ids (site, home building, slot), so shop stock, greetings and finished-quest
// counts follow the same person across sessions and window moves
uint64_t Game::npcKeyOf(int site, int bldg, int slot) const {
  // the classic island's handles are its indices, so its NPCs keep the M0 key (and with it their offers and greetings)
  if (!world.endless) return ((uint64_t)(site + 1) << 24) ^ ((uint64_t)(bldg + 1) << 12) ^ (uint64_t)(slot & 0xFFF);
  uint64_t s = site >= 0 && site < (int)world.sites.size() ? world.sites[(size_t)site].id : 0;
  uint64_t b = bldg >= 0 && bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)bldg].id : 0;
  return ew::mix64(s * 0x9E3779B97F4A7C15ull ^ ew::mix64(b + 0x51ED27ull) ^ (uint64_t)(uint32_t)(slot & 0xFFF));
}
uint64_t Game::npcKey(const Actor& a) const { return npcKeyOf(a.site, a.bldg, a.slot); }

const Quest* Game::questById(int id) const {
  for (auto& q : quests) if (q.id == id) return &q;
  return nullptr;
}

// (monsterPlural, dirWord, giverTown, rewardReadyMsg, fitLine, overworldTile, isGiver: rpg/sim/game_internal.h)

std::string Game::greeting(const Actor& a, int salt) {
  Rng r(hash32((uint32_t)npcKey(a)) ^ (uint32_t)(day * 31 + (int)hour) ^ (uint32_t)salt * 0x9E3779B9u);
  // where they live: their site, or for people indoors the site of the building they are in
  const int homeSite = a.site >= 0 ? a.site
                       : (a.bldg >= 0 && a.bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)a.bldg].site : -1);
  const std::string town = homeSite >= 0 ? world.sites[(size_t)homeSite].name : "THESE PARTS";
  // M1 kingdom identity: whose land this is, and whether the king holds court here
  const Kingdom* K = world.kingdomOf(homeSite);
  const std::string realm = K ? K->name : std::string();
  const bool capitalHere = homeSite >= 0 && world.sites[(size_t)homeSite].capital && K;
  // (fix) the realm's ruler and court by its society's titles (KHAN in THE GREAT TENT, DOGE in THE GUILDHALL...)
  cult::Society rsoc;
  if (homeSite >= 0)
    if (const cult::Culture* oc = world.cultureOfKingdom(world.sites[(size_t)homeSite].kingdom)) rsoc = cult::societyOf(*oc);
  const std::string ruler = rsoc.rulerTitle, court = rsoc.seatTitle;
  std::string seat;   // the kingdom's capital by name, when known
  if (K) { int ch = world.siteHandle(K->capitalId); if (ch >= 0) seat = world.sites[(size_t)ch].name; }
  // world-aware small talk, so the same handful of lines doesn't repeat in every village
  std::vector<std::string> local;
  {
    const Site* home = a.site >= 0 ? &world.sites[a.site] : nullptr;
    int hx = home ? home->ex : (int)(a.p.x / TILE), hy = home ? home->ey : (int)(a.p.y / TILE);
    int best = -1; float bd = 1e30f;
    for (int i = 0; i < (int)world.sites.size(); i++) {
      const Site& s = world.sites[i];
      if ((s.type != SiteType::Cave && s.type != SiteType::Ruin) || s.cleared) continue;
      float d = std::hypot((float)(s.ex - hx), (float)(s.ey - hy));
      if (d < bd) { bd = d; best = i; }
    }
    if (best >= 0) {
      const Site& s = world.sites[best];
      // (fixer M7b r2, review: "WENT INTO SILENT CAVE" twice in one talk: a kinsman lost there is a quest's pitch) a
      // stranger's treasure hunt, not the speaker's family
      local.push_back("A TREASURE-HUNTER WENT DIGGING ABOUT IN " + s.name + " LAST SPRING. ALL HE FOUND WAS " + monsterPlural(s.theme) + ".");
      local.push_back("STAY AWAY FROM " + s.name + ". NOTHING GOOD COMES OUT OF THERE.");
    }
    Biome b = world.over.biomeAt(hx, hy);
    if (b == Biome::Snow || b == Biome::Taiga) local.push_back("COLD ENOUGH TO FREEZE A TROLL'S BEARD. KEEP MOVING OR KEEP WARM.");
    if (b == Biome::Desert) local.push_back("WATER IS WORTH MORE THAN GOLD OUT HERE. AND WATCH FOR SAND RIPPLING WHERE THERE IS NO WIND.");
    if (b == Biome::Swamp) local.push_back("THE MARSH LIGHTS AREN'T LANTERNS, FRIEND. DON'T FOLLOW THEM.");
    if (b == Biome::Forest || b == Biome::Autumn) local.push_back("THE WOODS GIVE US TIMBER AND TAKE A HUNTER OR TWO EVERY WINTER.");
    if (isNight()) local.push_back("YOU SHOULDN'T BE WANDERING THIS LATE. THAT'S WHEN THE DEAD WALK.");
    for (auto& q : quests) {
      if (q.type != QType::Main) continue;
      if (q.stage == 0 && a.site != world.capital) local.push_back("THEY SAY THE " + lordTitleAt(world, world.capital) + " OF " + world.sites[world.capital].name + " WANTS SWORDS AGAINST THE DRAGON.");
      if (q.stage >= 1 && q.stage <= 3) local.push_back("THE WHOLE HOLD IS TALKING ABOUT YOU AND THE DRAGON. THE OLD GODS KEEP YOU.");
      if (q.state == QState::Done) local.push_back("IT'S YOU! THE ONE WHO SLEW ASHFANG! YOUR MEAD IS FREE IN " + town + ".");
    }
    if (plLevel >= 10) local.push_back("BY THE OLD GODS, YOU LOOK LIKE YOU'VE WALKED THROUGH A WAR.");
    if (K && !capitalHere) {
      local.push_back("THIS IS " + realm + " LAND. THE " + ruler + "'S TAX MEN COME EVERY AUTUMN, WHETHER THE HARVEST DOES OR NOT.");
      if (!seat.empty()) local.push_back("THEY SAY THE " + ruler + " OF " + realm + " HOLDS COURT IN " + seat + ". I'VE NEVER SEEN THEM MYSELF.");
    }
    if (capitalHere) local.push_back("THE " + ruler + " OF " + realm + " HOLDS COURT IN " + court + ". MIND YOUR MANNERS NEAR THE ROYAL GUARD.");
    // M4 (STORY lane): the town's own trouble, as the realm has it (15.6.3: hardship and war are felt before they are
    // told). Each counts twice: what a town is living through comes up more than the weather.
    if (homeSite >= 0) {
      const Site& hsite = world.sites[(size_t)homeSite];
      const realm::SettlementState* st = this->realm.settlement(hsite.id);
      const uint16_t f = st ? st->flags : 0;
      std::vector<std::string> trouble;
      if (f & realm::SS_FAMINE) trouble.push_back("THE GRANARY IS EMPTY AND THE PRIEST BLAMES THE SKY. I BLAME THE TAX MEN. BREAD IS DEARER THAN BEER NOW.");
      if (f & realm::SS_BESIEGED) trouble.push_back("NOBODY GETS IN OR OUT OF " + town + " WHILE THE SIEGE LASTS. HOW DID YOU GET IN? NO. DON'T TELL ME.");
      if (f & realm::SS_OCCUPIED) {
        const Kingdom* ok = world.kingdomOf(homeSite);
        trouble.push_back((ok ? ok->name : std::string("FOREIGN")) + " SOLDIERS SLEEP IN OUR BEDS NOW. SMILE AT THEM. SMILING IS FREE, AND THEY HAVE SPEARS.");
      }
      if (f & realm::SS_REFUGEES) trouble.push_back("THE CAMP OUTSIDE " + town + " GROWS EVERY DAY. THEY HAVE NOTHING BUT WHAT THEY CARRIED, AND THEY CARRIED THEIR DEAD.");
      if (f & (realm::SS_BURNED | realm::SS_REBUILDING)) trouble.push_back("HALF THE STREET IS ASH. WE'LL REBUILD. WE ALWAYS DO. WE'RE JUST TIRED OF IT.");
      if (hsite.kingdom >= 0) {
        const ew::Gid kid = world.kingdoms[(size_t)hsite.kingdom].id;
        for (const realm::War& w : this->realm.wars())
          if (!w.endDay && (w.attacker == kid || w.defender == kid)) {
            const ew::Gid foe = w.attacker == kid ? w.defender : w.attacker;
            const realm::KingdomState* fk = this->realm.kingdom(foe);
            trouble.push_back("THE WAR WITH " + (fk ? fk->name : std::string("THEM")) + " TOOK MY NEPHEW FOR THE LEVY. HE WRITES THAT THE FOOD IS BAD AND THE SERGEANT IS WORSE.");
            break;
          }
      }
      for (const std::string& t : trouble) { local.push_back(t); local.push_back(t); }
    }
  }
  auto withLocal = [&](const std::string& fixed) { return !local.empty() && r.f() < 0.5f ? local[r.irange((int)local.size())] : fixed; };
  switch (a.role) {
    case Role::Guard: {
      if (a.name == "ROYAL GUARD") {
        const std::string rg[] = {"THE " + ruler + " IS NOT TO BE TROUBLED WITHOUT CAUSE. STATE YOUR BUSINESS.",
                                  "THE ROYAL GUARD SEES EVERYTHING IN THIS HALL. REMEMBER THAT.", "LONG LIVE THE " + ruler + ". NOW MOVE ALONG.",
                                  "KEEP YOUR HANDS WHERE I CAN SEE THEM IN THIS HALL.", "YOU STAND IN THE COURT OF " + (realm.empty() ? std::string("THE REALM") : realm) + ". BOW WHEN THE " + ruler + " PASSES.",
                                  "WEAPONS STAY SHEATHED IN THIS HALL, STRANGER. NO EXCEPTIONS."};
        return rg[(r.irange(6) + salt) % 6];
      }
      if (capitalHere && r.f() < 0.4f) return "LONG LIVE THE " + ruler + " OF " + realm + ". " + court + " IS OFF LIMITS WITHOUT GOOD CAUSE, TRAVELLER.";
      static const char* g[] = {"I WAS A SELLSWORD ONCE. NOW I WATCH A GATE AND COUNT CARTS.",
                                "KEEP YOUR NOSE CLEAN, TRAVELLER.", "LOST SOMETHING? TRY THE INN. EVERYTHING ENDS UP AT THE INN.",
                                "WATCH THE ROADS AT NIGHT. THE DEAD DON'T STAY BURIED HERE.", "EYES OPEN, BLADE SHARP. THAT'S THE WATCH'S WAY.",
                                "NO TROUBLE IN THIS TOWN, TRAVELLER. NOT ON MY WATCH.", "MOVE ALONG. SOME OF US ARE WORKING.",
                                "MY FEET HURT, MY HELMET LEAKS, AND THE PAY IS LATE. WHAT DO YOU WANT?"};
      if (isNight() && r.f() < 0.4f) return "QUIET NIGHT. TOO QUIET. KEEP YOUR BLADE CLOSE.";
      return g[(r.irange(8) + salt) % 8];
    }
    case Role::Innkeeper: {
      if (capitalHere) {   // a capital is unmistakable: its innkeepers always talk of the king
        const std::string c[] = {"WELCOME TO " + town + ", SEAT OF THE " + ruler + " OF " + realm + ". BEDS ARE 10 GOLD, AND THE COURT GOSSIP IS FREE.",
                                 "THE " + ruler + "'S OWN GUARD DRINKS HERE ON FEAST DAYS. SIT, TRAVELLER: THIS IS THE FINEST INN IN " + realm + ".",
                                 "THE " + ruler + " OF " + realm + " RIDES OUT FROM " + court + " NOW AND THEN. CATCH A GLIMPSE IF YOU CAN. A ROOM IS 10 GOLD."};
        return c[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 3];
      }
      const std::string v[] = {"WELCOME TO THE INN OF " + town + ". A WARM BED IS 10 GOLD, AND THE MEAD IS COLD.",
                               "COME IN OUT OF THE " + std::string(isNight() ? "DARK" : "ROAD DUST") + ". STEW'S ON, AND THE ROOMS ARE CLEAN ENOUGH.",
                               "ANOTHER TRAVELLER! " + town + " DOESN'T SEE MANY. SIT, DRINK, TELL ME WHERE YOU'VE BEEN.",
                               "IF YOU'RE LOOKING FOR WORK, ASK. IF YOU'RE LOOKING FOR TROUBLE, TAKE IT OUTSIDE.",
                               "BEDS UPSTAIRS, ALE DOWN HERE, AND RUMOURS ARE FREE WITH EITHER."};
      return withLocal(v[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 5]);
    }
    case Role::Merchant: {
      const std::string v[] = {"COME, LOOK AT MY WARES. FINEST GOODS IN " + town + ".", "POTIONS, ARROWS, A BLADE OR TWO. GOLD TALKS, FRIEND.",
                               "BUYING OR SELLING? I DO BOTH, AND I DO THEM FAIRLY. MOSTLY.", "CARAVAN CAME IN LAST WEEK. GOOD STOCK, WHILE IT LASTS."};
      return v[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 4];
    }
    case Role::Smith: {
      const std::string v[] = {"STEEL AND FIRE, FRIEND. IF IT CUTS OR STOPS A BLADE, I CAN SELL IT TO YOU.",
                               "MIND THE SPARKS. NEED SOMETHING SHARP, OR SOMETHING THICK?", "BEST STEEL THIS SIDE OF " + world.sites[world.capital].name + ". ASK ANYONE.",
                               "BRING ME ORE AND I'LL... WELL, I'LL BUY IT. THEN SELL YOU A SWORD."};
      return v[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 4];
    }
    case Role::Priest: {
      const std::string v[] = {"THE OLD GODS WATCH OVER YOU. SHALL I MEND YOUR WOUNDS?", "YOU LOOK ROAD-WORN, CHILD. THE TEMPLE'S DOOR IS ALWAYS OPEN.",
                               "PEACE IN THESE WALLS, TRAVELLER. IF YOU ARE HURT, THE GODS CAN HEAL IT."};
      return v[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 3];
    }
    case Role::Mage: {
      const std::string v[] = {"MAGIC IS NOT A TOY. BUT IF YOU HAVE GOLD, I HAVE KNOWLEDGE.", "MIND THE CIRCLE ON THE FLOOR. IT TOOK ME A MONTH TO DRAW.",
                               "YOU HAVE THE LOOK OF SOMEONE WHO WANTS TO LEARN. GOOD. LEARNING HAS A PRICE."};
      return v[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 3];
    }
    case Role::Jarl: {
      const std::string v[] = {"SPEAK, STRANGER. WHAT BRINGS YOU BEFORE ME?", "SPEAK. THE PEOPLE OF " + town + " HAVE MY EAR, AND TODAY SO DO YOU.",
                               "SPEAK QUICKLY, TRAVELLER. " + town + " DOES NOT RUN ITSELF."};
      return v[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 3];
    }
    case Role::King: {
      const std::string pre = ruler + " ";
      std::string nm = a.name.rfind(pre, 0) == 0 ? a.name.substr(pre.size()) : (a.name.rfind("KING ", 0) == 0 ? a.name.substr(5) : a.name);
      std::string t = "WELCOME TO MY HALL, TRAVELLER. I AM " + nm + ", " + ruler + " OF " + (realm.empty() ? town : realm) + ".";
      for (const Quest& q : quests) {
        if (q.type != QType::Main) continue;
        if (q.state == QState::Done) t += " ALL " + (realm.empty() ? std::string("THE REALM") : realm) + " SINGS OF THE ONE WHO SLEW ASHFANG. YOU HONOUR MY HALL.";
        else if (q.stage >= 1) t += " MY " + lordTitleAt(world, homeSite) + "S WRITE OF YOU AND THE DRAGON. THE THRONE STANDS WITH YOU.";
        else if (homeSite == world.capital) t += " A DRAGON STIRS IN THE NORTH, THEY TELL ME. MY " + lordTitleAt(world, homeSite) + " GATHERS SWORDS AGAINST IT: GO TO HIM.";
        else t += " A DRAGON STIRS IN THE NORTH, THEY TELL ME. THE " + lordTitleAt(world, world.capital) + " OF " + world.sites[(size_t)world.capital].name + " GATHERS SWORDS AGAINST IT.";
      }
      return t;
    }
    // M2 wayside people (wayside.cpp)
    case Role::Hunter: {
      const std::string v[] = {"QUIET, NOW. YOU'LL SPOOK EVERY DEER FROM HERE TO " + town + ".",
                               "PELTS, MEAT, ARROWS. TAKE YOUR PICK, OR TAKE A BOW AND HELP ME THIN THE WOLVES.",
                               "THE TRACKS SAY THREE WOLVES PASSED AT DAWN. THE TRACKS ARE NEVER WRONG.",
                               "I SLEEP UNDER THE STARS AND EAT WHAT I CATCH. FEW TOWNSFOLK CAN SAY THE SAME."};
      return withLocal(v[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 4]);
    }
    case Role::Fisher: {
      const std::string v[] = {"THE FISH ARE BITING TODAY. THEY USUALLY ARE, IF YOU'RE PATIENT.",
                               "TROUT AT DAWN, EEL AT DUSK. SMOKED, IF YOU'RE TRAVELLING FAR.",
                               "SIT A WHILE. THE WATER TELLS YOU THINGS IF YOU LISTEN."};
      return v[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 3];
    }
    case Role::Herbalist: {
      const std::string v[] = {"MIND THE NIGHTSHADE. THE BLUE FLOWERS YOU MAY TOUCH.",
                               "EVERY HERB IN THIS GARDEN HEALS SOMETHING, OR KILLS SOMETHING. OFTEN BOTH.",
                               "A TEA FOR THE COLD, A SALVE FOR THE CUT, A DRAUGHT FOR THE ROAD. WHAT AILS YOU?"};
      return v[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 3];
    }
    case Role::Traveller: {
      const std::string v[] = {"WARM YOURSELF BY THE FIRE, FRIEND. THE ROAD IS LONG IN BOTH DIRECTIONS.",
                               "I'VE WALKED FROM ONE END OF " + (realm.empty() ? std::string("THIS LAND") : realm) + " TO THE OTHER. ASK ME WHAT I'VE SEEN.",
                               "THE ROADS ARE QUIET TONIGHT. THAT'S WHEN I WORRY.",
                               "SIT. SHARE THE FIRE. NEWS IS THE ONLY COIN I CARRY."};
      return v[(hash32((uint32_t)npcKey(a) ^ (uint32_t)day) + (uint32_t)salt) % 4];
    }
    case Role::Child: { static const char* c[] = {"WANNA PLAY TAG?", "MY DAD SAYS DRAGONS AREN'T REAL.", "ARE YOU AN ADVENTURER? WHOA!"}; return c[r.irange(3)]; }
    default: {
      const std::string v[] = {"FINE WEATHER TODAY. GOOD FOR THE CROPS.", "HAVE YOU HEARD? SOMETHING BIG WAS FLYING OVER THE MOUNTAINS.",
                                "BANDITS HAVE BEEN HITTING THE CARAVANS LATELY.", "MIND YOURSELF IN THE WOODS. WOLVES ARE BOLD THIS YEAR.",
                                "MY GRANDMOTHER SWORE THE OLD BARROWS ARE HAUNTED.", "WELCOME TO " + town + ". NOT MUCH HAPPENS HERE.",
                                "THE MEAD AT THE INN IS THE BEST IN THE HOLD."};
      return withLocal(v[r.irange(7)]);
    }
  }
}

bool Game::hasOffer(const Actor& a) const {
  const bool noble = background == Background::Noble;   // exiled noble: guards and jarls offer more work
  if (!a.npc || (a.role == Role::Guard && !noble) || a.role == Role::Child || a.role == Role::King || a.site < 0) return false;
  if (!a.human || a.quest > 0 || a.role == Role::Traveller) return false;   // M2: a troll, a missing person, a traveller
  uint64_t key = npcKey(a);
  for (auto& q : quests) if (q.state != QState::Done && q.type != QType::Main && isGiver(q, a)) return false;
  { Quest lq; if (lifeOffer(a, lq)) return true; }   // M5: an unmet need asks for help (life_quests.cpp)
  auto it = npcQuestsDone.find(key);
  int done = it == npcQuestsDone.end() ? 0 : it->second;
  if (a.role == Role::Jarl) return done < (noble ? 9 : 6);
  if (a.role == Role::Innkeeper) return done < 6;
  if (a.role == Role::Guard) return done < 2;
  if (a.role == Role::Priest || a.role == Role::Smith || a.role == Role::Mage) return done < 2;
  if (a.role == Role::Farmer || a.role == Role::Hunter) return done < 3;   // M2: the fields, the hunt
  if (a.role == Role::Fisher || a.role == Role::Herbalist) return done < 2;
  return (hash32((uint32_t)key ^ (uint32_t)seed) % 3 == 0) && done < 2;
}

// makeOffer: rpg/sim/quests.cpp (M2: radiant targets over the region plans, the new quest types, the text grammar)

void Game::acceptQuest(const Quest& q0) {
  Quest q = q0;
  q.id = nextQuestId++;
  q.state = QState::Active;
  if (q.target >= 0 && q.target < (int)world.sites.size()) {
    Site& T = world.sites[(size_t)q.target];
    // the place goes on the map: a dungeon as found (the giver told the way), a delivery's settlement as heard of
    if (q.type == QType::Deliver || q.type == QType::Protect) { if (!T.discovered) T.rumoured = true; }
    else T.discovered = true;
    if (T.cleared && (q.type == QType::Clear || q.type == QType::Bounty || q.type == QType::NamedBandit)) q.state = QState::Complete;
    if (!q.targetId) q.targetId = T.id;
  }
  // M2: a parcel to carry; the giver's next job is of another kind (quests.cpp)
  if (q.type == QType::Deliver) {
    Item parcel;
    parcel.kind = ItemKind::Quest; parcel.name = "PARCEL FOR " + q.subject; parcel.icon = art::Icon::Letter;
    parcel.tint = rgba(200, 170, 120); parcel.questId = q.id; parcel.value = 0;
    addItem(parcel, false);
    q.flags |= QF_PARCEL;
  }
  if (q.giverSlot >= 0 || q.giverBldg >= 0) marks[markKey(npcKeyOf(q.giverSite, q.giverBldg, q.giverSlot), Mk::LastOffer)] = (int)q.type + 1;
  quests.push_back(q);
  // the opening stays tracked until the old blade is collected: an unarmed newcomer who takes a villager's job on
  // the way to the inn still has the pin over the inn door
  if (openingQuest() < 0 || q.type == QType::Main) trackedQuest = q.id;
  emit(Ev::QuestUpdate, pl().p, q.id, 0, "QUEST STARTED: " + q.title);
  sfx((int)Sfx::QuestStart, pl().p);
}

void Game::completeQuest(Quest& q) {
  q.state = QState::Done;
  // M2: a quest's own items (a parcel, a keepsake) go to whoever the quest was for
  for (int i = (int)inv.size() - 1; i >= 0; i--) if (inv[(size_t)i].kind == ItemKind::Quest && inv[(size_t)i].questId == q.id) dropItem(i);
  giveGold(q.gold);
  gainXp(q.xp);
  npcQuestsDone[npcKeyOf(q.giverSite, q.giverBldg, q.giverSlot)]++;
  partyQuest(q, true);   // M8: the companions' view of the job done (party_game.cpp)
  SeaOps::questDone(*this, q);   // M10: a hidden land's people warm to the hero; a fishmonger takes the fish (sea_chain.cpp)
  ServiceOps::questDone(*this, q);   // M12: a campaign order or a diplomacy mission done (service_game.cpp)
  LordOps::questDone(*this, q);      // M12: a fief's task done (lord_game.cpp)
  emit(Ev::QuestUpdate, pl().p, q.id, 1, "QUEST COMPLETE: " + q.title);
  sfx((int)Sfx::QuestDone, pl().p);
  // occasional item reward (M6, 7.5 rule 5): by the quest's danger, never by the player's level; Uncommon to Epic
  Rng r(hash32((uint32_t)q.id * 977u) ^ (uint32_t)seed);
  if (r.f() < 0.5f) {
    int D = 1;
    if (q.target >= 0 && q.target < (int)world.sites.size()) D = world.sites[(size_t)q.target].level;
    else if (q.hasPos && world.src) D = world.src->danger(q.tgx, q.tgy);
    else if (q.giverSite >= 0 && q.giverSite < (int)world.sites.size()) D = world.sites[(size_t)q.giverSite].level;
    if (q.giverSite >= 0 && q.giverSite < (int)world.sites.size()) D = std::max(D, world.sites[(size_t)q.giverSite].level);
    const cult::Culture* gc = q.giverSite >= 0 && q.giverSite < (int)world.sites.size() ? world.cultureOf(q.giverSite) : nullptr;
    if (gc && world.src) gc = &world.src->culture(gc->id);
    // (M8b, 15.27.4) a quest's reward: Uncommon to Epic by its danger and the finder's level gate; a hard mission (the
    // target well beyond the hero, or a war's) is boosted a step
    gear::DropSource ds;
    ds.D = std::clamp(D, 1, gear::MAX_D);
    ds.src = gear::LootSrc::Quest;
    ds.playerLevel = plLevel;
    ds.culture = gc ? gc->id : 0;
    ds.atLeast = Rarity::Uncommon;
    ds.atMost = Rarity::Epic;
    const bool hard = ds.D >= plLevel + 3 || (q.flags & QF_DANGER);
    ds.boost = (int8_t)((hard ? 1 : 0) + (q.type == QType::War ? 1 : 0));
    const Rarity rar = gear::rollRarity(r, ds);
    static const ItemKind kinds[] = {ItemKind::Weapon, ItemKind::Armor, ItemKind::Helmet, ItemKind::Shield, ItemKind::Gloves, ItemKind::Boots,
                                     ItemKind::Cloak, ItemKind::Bow, ItemKind::Ring, ItemKind::Amulet};
    const ItemKind k = kinds[r.irange(10)];
    const int sub = k == ItemKind::Weapon ? r.irange(5) : 0;
    Item it = gear::makeGearC(r, k, sub, gear::itemLevelFor(ds), rar, gc);
    it.flags |= IF_QUESTREWARD;
    addItem(it);
  }
  if (trackedQuest == q.id) {
    // next: an open local job first (a nearby, level-fitting step), the main quest only when nothing else is open
    trackedQuest = -1;
    for (auto& o : quests) if (o.state != QState::Done && o.type != QType::Main) { trackedQuest = o.id; break; }
    if (trackedQuest < 0) for (auto& o : quests) if (o.state != QState::Done) { trackedQuest = o.id; break; }
  }
}

void Game::questKill(const Actor& a) {
  for (auto& q : quests) {
    if (q.state != QState::Active || q.type != QType::Hunt) continue;
    if (a.human || a.mon != q.mon) continue;
    q.have++;
    if (q.have >= q.need) {
      q.state = QState::Complete;
      std::string m = rewardReadyMsg(world, q);
      emit(Ev::QuestUpdate, a.p, q.id, 2, m);
      say(m);
      sfx((int)Sfx::QuestStart, a.p, 1.2f);
    }
    else emit(Ev::Text, a.p + Vec2(0, -26), (int)rgba(255, 220, 120), 0, std::to_string(q.have) + "/" + std::to_string(q.need));
  }
}

void Game::checkDungeonCleared(int si) {
  Site& st = world.sites[si];
  if (st.cleared) return;
  st.cleared = true;
  dungeonsCleared++;
  emit(Ev::QuestUpdate, pl().p, -1, 3, st.name + " CLEARED");
  sfx((int)Sfx::QuestDone, pl().p, 0.9f);
  bool forQuest = false;
  for (auto& q : quests) {
    if (q.state == QState::Active && q.target == si && (q.type == QType::Clear || q.type == QType::Bounty || q.type == QType::NamedBandit)) {
      q.state = QState::Complete;
      forQuest = true;
      std::string m = (q.type == QType::NamedBandit ? q.subject + " IS DEAD. " : std::string()) + rewardReadyMsg(world, q);
      emit(Ev::QuestUpdate, pl().p, q.id, 2, m);
      say(m);
    }
    if (q.type == QType::Main && st.mainQuest && q.stage == 1) {
      q.have++;
      Item shard; shard.kind = ItemKind::Quest; shard.name = "EMBER SHARD"; shard.icon = art::Icon::Sigil; shard.tint = rgba(255, 120, 40);
      shard.rarity = Rarity::Legendary; shard.questId = q.id; shard.value = 0;
      addItem(shard);
      if (q.have >= 3) advanceMain(2);
      else emit(Ev::QuestUpdate, pl().p, q.id, 2, "EMBER SHARDS " + std::to_string(q.have) + "/3");
    }
  }
  if (si == world.lair) {
    for (auto& q : quests) if (q.type == QType::Main && q.stage == 3) advanceMain(4);
  }
  // the wrong camp: never silent. Point at the place the bounty or job actually wants cleared.
  if (!forQuest && !st.mainQuest && si != world.lair) {
    const Quest* want = nullptr;
    float bd = 1e30f;
    for (const auto& q : quests) {
      if (q.state != QState::Active || (q.type != QType::Clear && q.type != QType::Bounty) || q.target < 0 || q.target == si) continue;
      const Site& t = world.sites[q.target];
      float d = std::hypot((float)(t.ex - st.ex), (float)(t.ey - st.ey)) * (q.id == trackedQuest ? 0.01f : 1.0f);
      if (d < bd) { bd = d; want = &q; }
    }
    if (want) {
      const Site& t = world.sites[want->target];
      std::string m = "NOT YOUR TARGET: " + t.name + " IS " + dirWord(t.ex - st.ex, t.ey - st.ey, false) + " OF HERE";
      emit(Ev::Notice, pl().p, (int)rgba(255, 170, 70), 0, m);
      say(m);
    }
  }
}

void Game::advanceMain(int stage) {
  int pendingMain = 0;   // a stage that completes immediately chains into the next one
  for (auto& q : quests) {
    if (q.type != QType::Main) continue;
    q.stage = stage;
    const Site& cap = world.sites[world.capital];
    switch (stage) {
      case 1:
        q.title = "THE EMBER SHARDS";
        q.desc = "THE " + lordTitleAt(world, world.capital) + " SAYS ONLY THE EMBER CROWN CAN BIND THE DRAGON ASHFANG. ITS THREE SHARDS LIE WITH DRAUGR WARLORDS IN ANCIENT RUINS. RECOVER ALL THREE.";
        q.need = 3; q.have = 0;
        for (auto& s : world.sites) if (s.mainQuest) s.discovered = true;
        emit(Ev::QuestUpdate, pl().p, q.id, 0, "MAIN QUEST: THE EMBER SHARDS");
        sfx((int)Sfx::QuestStart, pl().p);
        // warlords the player already slew before hearing of the shards still count (otherwise the quest soft-locks)
        for (auto& s : world.sites) {
          if (!s.mainQuest || !s.cleared) continue;
          q.have++;
          Item shard; shard.kind = ItemKind::Quest; shard.name = "EMBER SHARD"; shard.icon = art::Icon::Sigil; shard.tint = rgba(255, 120, 40);
          shard.rarity = Rarity::Legendary; shard.questId = q.id; shard.value = 0;
          addItem(shard);
        }
        if (q.have >= 3) pendingMain = 2;
        break;
      case 2:
        q.title = "RETURN TO THE " + lordTitleAt(world, world.capital);
        q.desc = "YOU HAVE ALL THREE EMBER SHARDS. BRING THEM TO THE " + lordTitleAt(world, world.capital) + " IN " + cap.name + ".";
        q.target = world.capital;
        emit(Ev::QuestUpdate, pl().p, q.id, 2, "ALL SHARDS FOUND! RETURN TO " + cap.name);
        sfx((int)Sfx::QuestDone, pl().p);
        break;
      case 3:
        q.title = "DRAGONSLAYER";
        q.desc = "THE CROWN IS WHOLE AND ASHFANG CAN BE KILLED. CLIMB TO " + lairName() + " AND END THE DRAGON.";
        q.target = world.lair;
        world.sites[world.lair].discovered = true;
        // the shards are consumed
        for (int i = (int)inv.size() - 1; i >= 0; i--) if (inv[i].kind == ItemKind::Quest && inv[i].name == "EMBER SHARD") dropItem(i);
        emit(Ev::QuestUpdate, pl().p, q.id, 0, "MAIN QUEST: DRAGONSLAYER");
        sfx((int)Sfx::QuestStart, pl().p);
        break;
      case 4:
        q.state = QState::Done;
        q.title = "DRAGONSLAYER";
        q.desc = "ASHFANG IS DEAD. THE SONGS OF EMBERVALE WILL CARRY YOUR NAME. THE WORLD IS STILL YOURS TO EXPLORE.";
        gainXp(1500);
        emit(Ev::QuestUpdate, pl().p, q.id, 1, "ASHFANG IS SLAIN! YOU ARE THE DRAGONSLAYER");
        sfx((int)Sfx::QuestDone, pl().p);
        break;
      default: break;
    }
  }
  if (pendingMain) advanceMain(pendingMain);
}

bool Game::questTarget(int qid, int& tx, int& ty) const {
  const Quest* q = questById(qid);
  if (!q || q->state == QState::Done) return false;
  if (q->type == QType::Main) {
    if (q->stage == 1) {
      // nearest uncleared shard ruin
      const Actor& p = pl();
      float bd = 1e30f; int best = -1;
      for (int i = 0; i < (int)world.sites.size(); i++) {
        const Site& s = world.sites[i];
        if (!s.mainQuest || s.cleared) continue;
        float d = std::hypot(s.ex * 16.0f - p.p.x, s.ey * 16.0f - p.p.y);
        if (d < bd) { bd = d; best = i; }
      }
      if (best < 0) return false;
      tx = world.sites[best].ex; ty = world.sites[best].ey;
      return true;
    }
    int t = q->stage == 3 ? world.lair : world.capital;
    if (t < 0) return false;
    if (q->stage == 0 || q->stage == 2) {
      // the keep's door (M3b: the city lord's seat, whatever the society built it as; a keep first, as before)
      const Site& c = world.sites[t];
      for (int pass = 0; pass < 2; pass++)
        for (int b = c.bldgFirst; b < c.bldgFirst + c.bldgCount; b++) {
          const Bldg& B = world.over.bldgs[b];
          if (pass == 0 ? B.type == art::Building::Keep : (bldgIsSeat(B) && !bldgIsRoyalSeat(B))) { tx = B.doorX(); ty = B.doorY(); return true; }
        }
    }
    tx = world.sites[t].ex; ty = world.sites[t].ey;
    return true;
  }
  // (fixer M6b r3) a story's "speak with" step points at the person themself while they are about: a census resident
  // walks the streets, and the binding's tile is only where they stood when the step began (the marker floated over an
  // empty tile while she stood beside the hero)
  if (q->type == QType::Story && q->state == QState::Active && !inside) {
    for (const story::Instance& in : story.running()) {
      if (in.questId != qid || in.done) continue;
      for (size_t i = 1; i < actors.size(); i++) {
        const Actor& a = actors[i];
        if (!a.npc || a.st == AState::Dead || !story.talksTo(*this, in, a)) continue;
        tx = (int)std::floor(a.p.x / TILE);
        ty = (int)std::floor(a.p.y / TILE);
        return true;
      }
      break;
    }
  }
  if (questTargetM2(*q, tx, ty)) return true;   // M2 quest types (quests.cpp)
  // an open hunt points at the quarry: the nearest one about (outdoors), else the nearest den of its kind, else the
  // giver's home (where the beasts are said to roam)
  if (q->type == QType::Hunt && q->state == QState::Active) {
    int px, py;
    overworldTile(*this, px, py);
    float bd = 1e30f;
    if (!inside)
      for (size_t i = 1; i < actors.size(); i++) {
        const Actor& a = actors[i];
        if (a.st == AState::Dead || a.human || a.npc || a.mon != q->mon) continue;
        float d = len2(a.p - pl().p);
        if (d < bd) { bd = d; tx = (int)std::floor(a.p.x / TILE); ty = (int)std::floor(a.p.y / TILE); }
      }
    if (bd < 1e30f) return true;
    bool found = false;
    bd = 90.0f * 90.0f;   // a den within a day's walk; further off, the giver's home is the better hint
    for (int di = 0; di < (int)world.dens.size(); di++) {
      const Den& dn = world.dens[di];
      if (dn.mon != q->mon || denClearedDay(di) >= 0) continue;
      float d = (float)((dn.x - px) * (dn.x - px) + (dn.y - py) * (dn.y - py));
      if (d < bd) { bd = d; tx = dn.x; ty = dn.y; found = true; }
    }
    if (found) return true;
  }
  // a finished job (and the opening) points at whoever pays: their door, or the giver themself out on the street
  bool toGiver = q->state == QState::Complete || q->type == QType::Retrieve;
  int t = toGiver ? q->giverSite : q->target;
  if (t < 0) t = q->giverSite;
  if (t < 0) return false;
  if (toGiver && q->giverBldg >= 0 && q->giverBldg < (int)world.over.bldgs.size()) {
    tx = world.over.bldgs[q->giverBldg].doorX(); ty = world.over.bldgs[q->giverBldg].doorY();
    return true;
  }
  if (toGiver && !inside)
    for (size_t i = 1; i < actors.size(); i++)
      if (isGiver(*q, actors[i]) && actors[i].st != AState::Dead) {
        tx = (int)std::floor(actors[i].p.x / TILE); ty = (int)std::floor(actors[i].p.y / TILE);
        return true;
      }
  tx = world.sites[t].ex; ty = world.sites[t].ey;
  return true;
}

// Bounty clarity (M0): true when a quest's reward waits with this NPC (a finished job, or the opening's old blade).
// The view draws a bobbing "!" over them (render_markers.cpp).
bool Game::rewardWaiting(const Actor& a) const {
  if (!a.npc || a.st == AState::Dead) return false;
  for (const Quest& q : quests) {
    if (q.type == QType::Main) continue;
    bool waiting = q.state == QState::Complete || (q.type == QType::Retrieve && q.state == QState::Active);
    if (waiting && isGiver(q, a)) return true;
  }
  return false;
}

int Game::openingQuest() const {
  for (const Quest& q : quests) if (q.type == QType::Retrieve && q.state == QState::Active && q.giverSlot < 0) return q.id;
  return -1;
}

// One journal line under the quest: what to do next ("RETURN TO X IN Y", "CLEAR <CAMP> (NORTH-EAST)", "3/5 WOLVES").
std::string Game::questStatus(const Quest& q) const {
  if (q.state == QState::Done) return "";
  int px, py;
  overworldTile(*this, px, py);
  auto dirTo = [&](int sx, int sy, bool shortForm) { return dirWord(sx - px, sy - py, shortForm); };
  const std::string town = giverTown(world, q);
  if (q.type == QType::Main) {
    const std::string cap = world.sites[world.capital].name;
    switch (q.stage) {
      case 0: {
        const std::string l = lordTitleAt(world, world.capital);
        // (fixer M4 r3) already in the capital: no "travel to" the place the hero stands in; the seat is here
        const IRect& cr = world.sites[world.capital].r;
        if (inside ? subSite == world.capital : (px >= cr.x - 2 && py >= cr.y - 2 && px < cr.x + cr.w + 2 && py < cr.y + cr.h + 2))
          return fitLine({"YOU ARE IN " + cap + ": SEEK THE " + l + "'S HALL", "SEEK THE " + l + "'S HALL HERE", "SEEK THE " + l + " HERE"});
        return fitLine({"SPEAK TO THE " + l + " IN " + cap, "THE " + l + " IN " + cap, "SPEAK TO THE " + l});
      }
      case 1: {
        int best = -1; float bd = 1e30f;
        for (int i = 0; i < (int)world.sites.size(); i++) {
          const Site& s = world.sites[i];
          if (!s.mainQuest || s.cleared) continue;
          float d = std::hypot((float)(s.ex - px), (float)(s.ey - py));
          if (d < bd) { bd = d; best = i; }
        }
        std::string n = std::to_string(q.have) + "/3 SHARDS";
        if (best < 0) return n;
        const Site& s = world.sites[best];
        return fitLine({n + ": " + s.name + " (" + dirTo(s.ex, s.ey, false) + ")", n + ": " + s.name + " (" + dirTo(s.ex, s.ey, true) + ")",
                        n + " (" + dirTo(s.ex, s.ey, false) + ")"});
      }
      case 2: return fitLine({"RETURN TO THE " + lordTitleAt(world, world.capital) + " IN " + cap, "RETURN TO " + cap});
      case 3: {
        if (world.lair < 0) return "SLAY ASHFANG";
        const Site& L = world.sites[world.lair];
        return fitLine({"SLAY ASHFANG AT " + L.name + " (" + dirTo(L.ex, L.ey, false) + ")", "SLAY ASHFANG (" + dirTo(L.ex, L.ey, false) + ")"});
      }
      default: return "";
    }
  }
  const std::string inTown = town.empty() ? "" : " IN " + town;
  if (q.type == QType::Retrieve)
    return fitLine({"TALK TO THE INNKEEPER" + inTown, "TALK TO THE INNKEEPER"});
  if (std::string m2 = questStatusM2(q); !m2.empty()) return m2;   // M2 quest types (quests.cpp)
  if (q.state == QState::Complete)
    return fitLine({"RETURN TO " + q.giverName + inTown, "RETURN TO " + q.giverName, "RETURN TO" + inTown});
  if (q.type == QType::Hunt) {
    std::string n = std::to_string(q.have) + "/" + std::to_string(q.need) + " " + monsterPlural(q.mon);
    return fitLine({town.empty() ? n : n + " NEAR " + town, n});
  }
  if ((q.type == QType::Clear || q.type == QType::Bounty) && q.target >= 0 && q.target < (int)world.sites.size()) {
    const Site& t = world.sites[q.target];
    const std::string dl = " (" + dirTo(t.ex, t.ey, false) + ")", ds = " (" + dirTo(t.ex, t.ey, true) + ")";
    // a bounty is won by the chief's death (Game::kill), so its line says so
    if (q.type == QType::Bounty)
      return fitLine({"KILL THE CHIEF AT " + t.name + dl, "KILL THE CHIEF AT " + t.name + ds, "KILL THE CHIEF: " + t.name + ds,
                      "CHIEF AT " + t.name + ds, t.name + ds});
    return fitLine({"CLEAR " + t.name + dl, "CLEAR " + t.name + ds, t.name + ds});
  }
  return "";
}

float Game::blessingSecs() const { return background == Background::Novice ? 900.0f : 600.0f; }   // novice: +50 %

float Game::priceFactor(Role seller) const {
  if (background == Background::Blacksmith && seller == Role::Smith) return 0.8f * SkillOps::buyMul(*this, seller);   // a smith's child pays a fair price
  if (background == Background::Urchin && seller == Role::Merchant) return 0.85f * SkillOps::buyMul(*this, seller);   // the streets know their own
  return SkillOps::buyMul(*this, seller);   // (M9) Speech
}

// The opening: the start village innkeeper hands over an old blade (and a draught for the road) for a small favour.
void Game::giveFirstWeapon(Quest& q) {
  Rng r(hash32((uint32_t)seed ^ 0xB1ADEu));
  // (M6) an item-level-1 heirloom in the old heartland make: worn below a new bronze sword's 8
  Item w = gear::makeGearC(r, ItemKind::Weapon, (int)WeaponType::Sword, 1, Rarity::Common, nullptr, Mat::Iron);
  w.name = "OLD BLADE"; w.power = 7; w.value = 12; w.ench = Ench::None; w.enchPow = 0;
  addItem(w);
  Item pot = makePotion(PotionType::Health, 0); pot.count = 1;
  addItem(pot);
  q.state = QState::Done;
  storyFlags |= SF_FIRST_WEAPON;
  gainXp(q.xp);
  emit(Ev::QuestUpdate, pl().p, q.id, 1, "QUEST COMPLETE: " + q.title);
  sfx((int)Sfx::QuestDone, pl().p);
  if (trackedQuest == q.id) {
    // next: an open local job first (a nearby, level-fitting step), the main quest only when nothing else is open
    trackedQuest = -1;
    for (auto& o : quests) if (o.state != QState::Done && o.type != QType::Main) { trackedQuest = o.id; break; }
    if (trackedQuest < 0) for (auto& o : quests) if (o.state != QState::Done) { trackedQuest = o.id; break; }
  }
}

// ------------------------------------------------------------------ dialogue
namespace {
const char* talkRoleName(Role r) {
  static const char* n[] = {"VILLAGER", "GUARD", "MERCHANT", "SMITH", "INNKEEPER", "PRIEST", "JARL", "FARMER", "CHILD", "MAGE", "BANDIT", "KING",
                            "HUNTER", "FISHER", "HERBALIST", "TRAVELLER", "HERALD", "SOLDIER", "CAPTAIN", "REFUGEE",
                            "HARBOURMASTER", "SHIPWRIGHT", "SAILOR", "PIRATE",
                            "STEWARD", "WARLORD", "ENVOY"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Role::COUNT, "a name for every role");
  return (int)r < (int)Role::COUNT ? n[(int)r] : "?";
}

// the site a person belongs to: their own, or for people indoors the site of the building they are in
int talkSiteOf(const Game& g, const Actor& a) {
  if (a.site >= 0 && a.site < (int)g.world.sites.size()) return a.site;
  if (a.bldg >= 0 && a.bldg < (int)g.world.over.bldgs.size()) {
    const int s = g.world.over.bldgs[(size_t)a.bldg].site;
    if (s >= 0 && s < (int)g.world.sites.size()) return s;
  }
  return -1;
}

// a census resident's job (Job::None for anyone else)
life::Job talkJobOf(const Game& g, const Actor& a) {
  const int si = talkSiteOf(g, a);
  if (si < 0 || !g.world.sites[(size_t)si].settlement() || a.resident < 0) return life::Job::None;
  const life::Census* c = g.life.find(g.world.sites[(size_t)si].id);
  if (!c || a.resident >= (int)c->res.size()) return life::Job::None;
  return c->res[(size_t)a.resident].job;
}

// a person with a name of their own (not "GUARD", "ROYAL GUARD", a soldier of the line, a herald on the road)
bool talkNamed(const Actor& a) {
  if (!a.npc || !a.human) return false;
  if (a.role == Role::Guard || a.role == Role::Soldier || a.role == Role::Herald || a.role == Role::Bandit) return false;
  if (a.name.empty() || a.name == talkRoleName(a.role) || a.name.find("GUARD") != std::string::npos) return false;
  return true;
}

// what a person is to the player, as the panel's title says it ("INNKEEPER", "BAKER", "SPEAKER OF VOND")
std::string talkTrade(const Game& g, const Actor& a) {
  const life::Job j = talkJobOf(g, a);
  if (a.role == Role::Villager && j != life::Job::None) return life::jobName(j);
  if (a.role == Role::Jarl) {
    const int si = talkSiteOf(g, a);
    return si >= 0 ? lordTitleAt(g.world, si) : std::string("JARL");
  }
  if (a.role == Role::King) return "RULER";
  return talkRoleName(a.role);
}

// the first words a people say to a stranger (light culture flavour, cult::Archetype order)
const char* welcomeWord(const Game& g, int site) {
  static const char* const w[] = {"HAIL, STRANGER.", "WELL MET, STRANGER.", "WELL MET.", "GOOD DAY TO YOU.", "PEACE BE UPON YOU.",
                                  "THE SKY KEEP YOU, TRAVELLER.", "MIND THE MUD, STRANGER.", "BE WELCOME, HONOURED GUEST.", "WELL MET, FRIEND.",
                                  "THE SUN SEES YOU, STRANGER.", "THE LEAVES WELCOME YOU.", "THE STARS KEEP YOU, STRANGER."};
  static_assert(sizeof(w) / sizeof(w[0]) == (size_t)cult::Archetype::COUNT, "a welcome for every people");
  const cult::Culture* c = site >= 0 ? g.world.cultureOf(site) : nullptr;
  const int k = c ? (int)c->archetype : (int)cult::Archetype::Heartland;
  return k >= 0 && k < (int)cult::Archetype::COUNT ? w[k] : "WELL MET.";
}

// a person telling the player who they are ("I'M SUSUT, AND THIS IS MY INN."); "" for those without a name of their own
std::string introPhrase(const Game& g, const Actor& a) {
  if (!talkNamed(a) || a.role == Role::King || a.role == Role::Captain || a.role == Role::Refugee) return "";
  const std::string n = speech::givenName(a.name);
  const int si = talkSiteOf(g, a);
  const std::string town = si >= 0 ? g.world.sites[(size_t)si].name : std::string();
  switch (a.role) {
    case Role::Innkeeper: return "I'M " + n + ", AND THIS IS MY INN.";
    case Role::Merchant: return "I'M " + n + ", AND THIS IS MY SHOP.";
    case Role::Smith: return "I'M " + n + ", THE SMITH HERE.";
    case Role::Priest: return "I AM " + n + ", PRIEST OF THIS TEMPLE.";
    case Role::Mage: return "I AM " + n + ". I STUDY THE ARTS HERE.";
    case Role::Jarl: return "I AM " + n + ", " + talkTrade(g, a) + (town.empty() ? std::string(".") : " OF " + town + ".");
    case Role::Farmer: return "I'M " + n + ". I WORK THE FIELDS HERE.";
    case Role::Hunter: return "THEY CALL ME " + n + ". I HUNT THESE WOODS.";
    case Role::Fisher: return "I'M " + n + ". I FISH THESE WATERS.";
    case Role::Herbalist: return "I'M " + n + ". I GROW WHAT HEALS.";
    case Role::Traveller: return "THE NAME'S " + n + ". I'M ONLY PASSING THROUGH.";
    case Role::Child: return "I'M " + n + "!";
    default: break;
  }
  switch (talkJobOf(g, a)) {
    case life::Job::None: return "I'M " + n + ".";
    case life::Job::Beggar: case life::Job::Guard: return "";
    case life::Job::Child: return "I'M " + n + "!";
    case life::Job::Elder: return "I'M " + n + ". I'VE LIVED HERE ALL MY LIFE.";
    case life::Job::Noble: return "I AM " + n + ", OF ONE OF THE GOOD HOUSES HERE.";
    case life::Job::Labourer: return "I'M " + n + ". I TAKE WHATEVER WORK COMES.";
    case life::Job::Servant: return "I'M " + n + ". I'M IN SERVICE HERE.";
    case life::Job::Server: return "I'M " + n + ". I SERVE AT THE INN.";
    case life::Job::Bard: return "I'M " + n + ", THE BARD" + (town.empty() ? std::string(".") : " OF " + town + ".");
    case life::Job::Lamplighter: return "I'M " + n + ". I LIGHT THE LAMPS AT DUSK.";
    default: {
      const std::string j = life::jobName(talkJobOf(g, a));
      const bool vowel = !j.empty() && std::string("AEIOU").find(j[0]) != std::string::npos;
      return "I'M " + n + (vowel ? ", AN " : ", A ") + j + " HERE.";
    }
  }
}

// a line that already says who the speaker is ("I'M ARN", "NAME'S ARN", "THEY CALL ME ARN", "I AM QUAVION")
bool introducesSelf(const std::string& text, const std::string& name) {
  const std::string n = speech::givenName(name);
  for (const char* lead : {"I'M ", "I AM ", "NAME'S ", "CALL ME ", "NAME IS "})
    if (text.find(std::string(lead) + n) != std::string::npos) return true;
  return text.find(n + ", THEY CALL ME") != std::string::npos;
}

// the line opens on a greeting of its own (no second "HELLO" in front of it)
bool opensWithGreeting(const std::string& t) {
  static const char* const k[] = {"WELCOME", "GOOD ", "MORNING", "EVENING", "NIGHT", "HAIL", "GREETINGS", "PEACE", "AH,", "AH.", "ANOTHER ",
                                  "COME ", "WELL MET", "YOU'RE BACK", "BACK AGAIN", "YOU AGAIN", "IT'S YOU", "HELLO", "SPEAK,"};
  for (const char* s : k) if (t.rfind(s, 0) == 0) return true;
  return false;
}

// the kind of option a first page can do without: the services (grouped under one header) and the kindnesses
// (offered once the player asks how someone is keeping)
bool serviceOpt(const DlgOpt& o) {
  if (o.action == A_TRADE || o.action == A_REST || o.action == A_RUMOR || o.action == A_HEAL || o.action == A_LEARN) return true;
  if (o.action >= DLG_HOME && o.action < DLG_HOME_END) return true;    // deeds, horses, recipes, the kitchen, the mill
  if (o.action >= DLG_MAGIC && o.action < DLG_MAGIC_END) return true;  // (M9) a teacher's spells, attunement
  if (o.action >= DLG_SEA && o.action < DLG_SEA_END && o.action != DLG_SEA + 95) return true;   // (M10) boats, the yard, passages
  if (o.action >= DLG_SKILLS && o.action < DLG_SKILLS_END) return true;  // (M9 integration) lessons, prices, unlearning
  if (o.action >= DLG_CRAFT && o.action < DLG_M6_END) return true;     // the forge, the craft
  return lifeOptKind(o.action) == 1;                                    // the inn's meal, food for the stores
}
}  // namespace

std::string Game::speakerTitle(const Actor& a) const {
  if (a.comp) return a.name;   // (M8) a companion is known by name (the VOICE lane may add their calling)
  if (!a.npc || !a.human) return a.name;
  const std::string trade = talkTrade(*this, a);
  if (!talkNamed(a)) return a.name;
  if (a.role == Role::King) return a.name;   // a ruler is known by name and title alike
  if (talkMem.introduced(npcKey(a))) return speech::givenName(a.name) + ", " + trade;
  return "THE " + trade;
}

std::string Game::speakerTitle() const {
  for (const Actor& a : actors)
    if (a.id == dlg.actor && dlg.actor >= 0) {
      // (M7b) a line told as a story (a narrated ending) carries the tale's title as its speaker: show that, not the person
      if (!dlg.speaker.empty() && dlg.speaker != a.name) return dlg.speaker;
      return speakerTitle(a);
    }
  return dlg.speaker;
}

// M7b (rpg/sim/talk.h): the opening, then the talk memory (a person spoken to is met: the day, how often) and, in tests
// and the dialogue review, the recorder
void Game::talkTo(Actor& a) {
  talkToInner(a);
  if (mode != Mode::Dialogue || dlg.actor != a.id) return;
  const uint64_t key = a.npc && a.human ? npcKey(a) : 0;
  if (key) {
    speech::Met& m = talkMem.meet(key, day);
    m.lastTopic = dlg.topic;
    m.lastLine = speech::lineHash(dlg.front.empty() ? dlg.text : dlg.front);
  }
  if (talkRec) {
    const int hs = talkSiteOf(*this, a);
    const bool troll = a.npc && !a.human && a.mon == Monster::Troll;
    talkRec->begin(dlg, a.name, troll ? std::string("TROLL") : std::string(talkRoleName(a.role)),
                   hs >= 0 ? world.sites[(size_t)hs].name : std::string(), key);
  }
}

void Game::talkToInner(Actor& a) {
  // M5: a village animal is petted, not talked to (a wag, a purr, a cluck)
  if (a.critter) {
    if (HomeOps::critterUse(*this, a)) return;   // M7: the player's own beasts (pet, groom, ride; home_actors.cpp)
    a.bubble = art::Bubble::Heart; a.bubbleT = 1.6f;
    a.face = faceOf(pl().p - a.p);
    const art::Critter k = (art::Critter)(a.critter - 1);
    sfx((int)(k == art::Critter::Dog ? Sfx::Bark : k == art::Critter::Cat ? Sfx::Meow : Sfx::Cluck), a.p, 1.15f, 0.6f);
    return;
  }
  dlg = Dialogue();
  dlg.actor = a.id;
  dlg.speaker = a.name;
  dlg.role = a.role;
  if (a.comp && partyTalk(a)) return;   // M8: a companion's own conversation (party_talk.cpp, VOICE lane)
  // M2: a toll bridge's troll asks its toll; a missing person found in a cave asks to be led out
  if (a.npc && !a.human && a.mon == Monster::Troll) { tollTalk(a); return; }
  using speech::Topic;
  if (a.quest > 0) {
    const Quest* q = questById(a.quest);
    if (q && q->state == QState::Active && !(q->flags & QF_FOUND)) {
      speech::propose(dlg, speech::P_URGENT, Topic::QuestActive,
                      "YOU CAME FOR ME? " + q->giverName + " SENT YOU? THANK THE GODS. I CAN'T FIND THE WAY OUT, AND THERE ARE THINGS IN THE DARK.");
      dlg.opts.push_back({"STAY CLOSE. I'LL GET YOU OUT.", A_ESCORT, q->id});
    } else if (q && q->state == QState::Active) speech::propose(dlg, speech::P_URGENT, Topic::QuestActive, "I'M RIGHT BEHIND YOU. JUST GET US OUT OF HERE.");
    else speech::propose(dlg, speech::P_URGENT, Topic::QuestDone, "I'LL FIND MY OWN WAY HOME FROM HERE. TELL " + (q ? q->giverName : std::string("THEM")) + " I'M SAFE!");
    dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
    mode = Mode::Dialogue;
    sfx((int)Sfx::Talk, a.p);
    return;
  }
  const uint64_t key = a.npc && a.human ? npcKey(a) : 0;
  const speech::Met* met = key ? talkMem.find(key) : nullptr;
  const bool knownBefore = met && met->times > 0;
  // ---- 1. the greeting (never the same line the player heard from them, or from anyone, lately)
  {
    std::string g = greeting(a);
    for (int salt = 1; salt <= 3; salt++) {
      const uint32_t h = speech::lineHash(g);
      const bool stale = talkMem.saidRecently(h, day, 1) || (met && met->lastLine == h && day - met->last <= 3);
      if (!stale) break;
      g = greeting(a, salt);
    }
    const bool trade = a.role == Role::Merchant || a.role == Role::Smith || a.role == Role::Innkeeper || a.role == Role::Priest || a.role == Role::Mage;
    speech::propose(dlg, trade ? speech::P_SERVICE : speech::P_GREETING, trade ? Topic::Service : Topic::Greeting, g);
  }
  // the background shows in how people talk to you (and what they charge)
  {
    bool say1 = hash32((uint32_t)npcKey(a) ^ (uint32_t)(day * 977)) % 2 == 0;
    auto bg = [&](const char* t) { speech::propose(dlg, speech::P_SERVICE, Topic::Smalltalk, t); };
    if (background == Background::Blacksmith && a.role == Role::Smith) bg("A SMITH'S CHILD? I CAN SEE IT IN YOUR HANDS. FOR YOU, A FAIR PRICE: A FIFTH OFF ANYTHING ON MY RACK.");
    if (background == Background::Urchin && a.role == Role::Merchant) bg("YOU'VE GOT THE STREETS IN YOUR EYES, FRIEND. ALL RIGHT, ONE OF OUR OWN: I'LL KNOCK A LITTLE OFF.");
    if (background == Background::Noble && a.role == Role::Guard && say1) bg("FORGIVE ME, I KNOW NOBLE BEARING WHEN I SEE IT. THE WATCH COULD USE SOMEONE OF YOUR... STANDING.");
    if (background == Background::Novice && a.role == Role::Priest) bg("A NOVICE OF THE TEMPLE! THE GODS' BLESSINGS RUN LONGER IN YOU, CHILD. SHALL I MEND YOUR WOUNDS?");
    if (background == Background::Sailor && a.role == Role::Innkeeper && say1) bg("SEA LEGS ON DRY LAND, EH? SAILORS DRINK HERE, AND NOBODY ASKS WHAT SANK.");
    if (background == Background::Farmhand && a.role == Role::Innkeeper && !say1) bg("A FARMHAND, BY THOSE HANDS. THEN YOU KNOW WHAT GOOD BREAD IS WORTH ON THE ROAD.");
    if (background == Background::Hunter && a.role == Role::Villager && say1) bg("YOU WALK LIKE A HUNTER. THE WOLVES WON'T HEAR YOU COMING, WILL THEY?");
  }
  // ---- 2. a reward waiting comes first: the first option collects it
  for (auto& q : quests)
    if (q.state == QState::Complete && q.type != QType::Main && isGiver(q, a)) {
      bool bounty = q.type == QType::Bounty || q.type == QType::Hunt || q.type == QType::NamedBandit;
      dlg.opts.push_back({std::string(bounty ? "COLLECT BOUNTY (+" : "COLLECT REWARD (+") + std::to_string(q.gold) + " GOLD)", A_TURNIN, q.id});
      speech::propose(dlg, speech::P_REWARD, Topic::QuestDone, turnInLine(q));
    }
  // ---- 3. the main quest's hooks (the lord of the capital)
  for (auto& q : quests) {
    if (q.type != QType::Main || a.role != Role::Jarl || a.site != world.capital) continue;
    if (q.stage == 0) {
      speech::propose(dlg, speech::P_QUEST + 5, Topic::QuestOffer,
                      "SO YOU'VE HEARD THE RUMOURS. THEY ARE TRUE: A DRAGON, ASHFANG, HAS WOKEN BENEATH " + lairName() + ". STEEL ALONE CANNOT KILL IT.");
      dlg.opts.push_back({"HOW CAN IT BE STOPPED?", A_MAIN, 1});
    }
    if (q.stage == 2) {
      speech::propose(dlg, speech::P_QUEST + 5, Topic::QuestDone, "THE THREE EMBER SHARDS! I NEVER THOUGHT I WOULD SEE THEM WHOLE AGAIN. LET THE SMITHS FORGE THE CROWN.");
      dlg.opts.push_back({"I WILL HUNT THE DRAGON.", A_MAIN, 3});
    }
  }
  // ---- 4. a job of theirs under way: what is still to do, in their own words (the journal keeps q.desc)
  for (auto& q : quests)
    if (q.state == QState::Active && q.type != QType::Main && q.type != QType::Retrieve && isGiver(q, a)) {
      speech::propose(dlg, speech::P_QUEST, Topic::QuestActive, questReminder(q));
      break;
    }
  // ---- 5. the opening: the start village innkeeper hands over the old blade (VISION_PLAN 15.1)
  for (auto& q : quests)
    if (q.type == QType::Retrieve && q.state == QState::Active && isGiver(q, a)) {
      giveFirstWeapon(q);
      std::string t = "THE ROAD TOOK ALL BUT YOUR SHIRT, EH? HERE, TAKE MY FATHER'S OLD BLADE FROM OVER THE HEARTH, AND A DRAUGHT FOR THE ROAD.";
      // point at the job the journal now tracks, so the advice matches the quest (not a made-up favour)
      const Quest* nxt = nullptr;
      for (const auto& o : quests) if (o.id == trackedQuest) nxt = &o;
      if (nxt && nxt->type == QType::Hunt) t += std::string(" THEY SAY THE ") + monsterPlural(nxt->mon) + " ARE BAD THIS YEAR, IF YOU NEED WORK.";
      else t += " IF YOU NEED WORK, ASK AROUND: SOMEONE HERE ALWAYS NEEDS A HAND.";
      speech::propose(dlg, speech::P_URGENT, Topic::Reward, t);
      break;
    }
  if (std::string t = questDialogue(a); !t.empty()) speech::propose(dlg, speech::P_REWARD, Topic::QuestDone, t);   // M2: a parcel's recipient
  // ---- 6. the options of their trade
  if (hasOffer(a)) dlg.opts.push_back({a.role == Role::Innkeeper ? "ANY WORK GOING?" : "DO YOU NEED HELP?", A_ASK, 0});
  if (a.role == Role::Merchant || a.role == Role::Smith || a.role == Role::Mage || a.role == Role::Priest || a.role == Role::Innkeeper ||
      a.role == Role::Hunter || a.role == Role::Fisher || a.role == Role::Herbalist)
    dlg.opts.push_back({"LET ME SEE YOUR WARES.", A_TRADE, 0});
  if (a.role == Role::Traveller) dlg.opts.push_back({"ANY NEWS FROM THE ROAD?", A_NEWS, 0});
  if (a.role == Role::Innkeeper) {
    // M0b: a room already paid for is not charged again; in its last hour (from 11:00 on the due day) there is no
    // sleep left in it, so the innkeeper lets you a fresh room instead
    bool mine = lodgingActive() && inside && lodging.bldg == subBldg && lodgingSleepHours(day, hour, lodging.untilDay) > 0;
    dlg.opts.push_back({mine ? "ABOUT MY ROOM..." : "RENT A ROOM (10 GOLD)", A_REST, mine ? 0 : 10});
    dlg.opts.push_back({"BUY A RUMOUR (10 GOLD)", A_RUMOR, 10});
  }
  if (a.role == Role::Priest) dlg.opts.push_back({"BLESS ME.", A_HEAL, 0});
  // (M9) a mage's lessons (MEND, FROST LANCE, the rest of the schools, a land's tradition): MagicOps::talk below
  // ---- 7. the lanes' hooks, in their old order. Each proposes its line (speech::propose); a hook that still writes
  // dlg.text directly is taken at the priority its kind of line has (and gives way to a weightier line already chosen)
  auto hook = [&](int defPri, Topic t, auto&& fn) {
    const std::string before = dlg.text;
    const int bp = dlg.pri;
    const uint8_t bt = dlg.topic;
    fn();
    if (dlg.text != before && dlg.pri == bp && dlg.topic == bt) {
      if (defPri >= bp) { dlg.pri = defPri; dlg.topic = (uint8_t)t; }
      else dlg.text = before;
    }
  };
  hook(speech::P_BARK, Topic::Bark, [&] { lifeTalk(a); });   // M5: barks of need, feed / employ / supply / befriend (life_game.cpp)
  hook(speech::P_NEWS, Topic::War, [&] { warTalk(a); });     // M4: the war's talk (siege commanders, guards' news of the front)
  {   // M4: story quests, gossip, the realm's news (rpg/story/story_game.cpp; the SAGA lane proposes at P_STORY / P_NEWS)
    const std::string before = dlg.text;
    const int bp = dlg.pri;
    const uint8_t bt = dlg.topic;
    storyTalk(a);
    if (dlg.text != before && dlg.pri == bp && dlg.topic == bt) {
      bool scene = false, pitch = false;   // a running story's own scene (its choices), or a story offered (its pitch)
      for (const DlgOpt& o : dlg.opts) {
        if (o.action >= DLG_STORY && o.action < DLG_STORY + story::SA_START) scene = true;
        else if (o.action >= DLG_STORY && o.action < DLG_WAR) pitch = true;
      }
      const int sp = scene ? speech::P_URGENT - 5 : pitch ? speech::P_STORY : speech::P_NEWS;
      if (sp >= bp) { dlg.pri = sp; dlg.topic = (uint8_t)(scene || pitch ? Topic::Story : Topic::News); }
      else dlg.text = before;
    }
  }
  hook(speech::P_NEWS, Topic::Foe, [&] { foeTalk(a); });         // M6: warnings of named beasts and world bosses (foes_game.cpp)
  hook(speech::P_URGENT, Topic::Craft, [&] { craftTalk(a); });   // M6: the forge's work and lessons (a thief barred: urgent)
  hook(speech::P_SERVICE, Topic::Home, [&] { homeTalk(a); });    // M7: deeds, builders, horses, recipes and cooking
  hook(speech::P_SERVICE, Topic::Companion, [&] { partyFolkTalk(a); });   // M8: offers to join, kin of companions and the fallen
  hook(speech::P_SERVICE, Topic::Service, [&] { SkillOps::talk(*this, a); });   // M9: trainers, teaching, persuade / intimidate
  hook(speech::P_SERVICE, Topic::Service, [&] { MagicOps::talk(*this, a); });   // M9: mage teachers, tomes, traditions
  hook(speech::P_SERVICE, Topic::Service, [&] { SeaOps::talk(*this, a); });     // M10: harbourmasters, shipwrights, sailors
  hook(speech::P_SERVICE, Topic::Service, [&] { ServiceOps::talk(*this, a); }); // M12: captains (ENLIST, ORDERS), envoys, rulers
  hook(speech::P_SERVICE, Topic::Service, [&] { LordOps::talk(*this, a); });    // M12: the steward, rulers (lordships, charters)
  // ---- 8. one coherent opening: a stranger is told who this is (and gets the people's own hello), an acquaintance is
  // greeted as one; the core line (what matters most to the speaker now) follows
  const std::string core = dlg.text;
  if (key && talkNamed(a)) {
    const int hs = talkSiteOf(*this, a);
    if (!talkMem.introduced(key)) {
      if (introducesSelf(core, a.name)) talkMem.flag(key, speech::TM_INTRODUCED, day);
      else if (const std::string intro = introPhrase(*this, a); !intro.empty()) {
        const bool light = dlg.pri <= speech::P_NEED && !opensWithGreeting(core);
        std::string t = light ? speech::joinFit(std::string(welcomeWord(*this, hs)) + " " + intro, core) : core;
        if (t == core) t = speech::joinFit(intro, core);
        if (t == core) t = speech::joinFit("I'M " + speech::givenName(a.name) + ".", core);
        if (t != core) { dlg.text = t; talkMem.flag(key, speech::TM_INTRODUCED, day); }
      }
    } else if (knownBefore && dlg.pri <= speech::P_NEED && !opensWithGreeting(core)) {
      const uint32_t h = hash32((uint32_t)key ^ (uint32_t)day * 2654435761u);
      std::string hello;
      if (met->flags & speech::TM_SLIGHTED) hello = "YOU AGAIN.";
      else if (met->flags & speech::TM_THANKED) hello = h % 2 ? "GOOD TO SEE YOU AGAIN, FRIEND." : "MY FRIEND! I HAVEN'T FORGOTTEN WHAT YOU DID FOR US.";
      else if (met->last == day) hello = h % 2 ? "BACK AGAIN?" : "YOU AGAIN? WELCOME BACK.";
      else hello = h % 3 == 0 ? "GOOD TO SEE YOU AGAIN." : h % 3 == 1 ? "AH, IT'S YOU AGAIN." : "BACK IN " + (hs >= 0 ? world.sites[(size_t)hs].name : std::string("TOWN")) + ", ARE YOU?";
      dlg.text = speech::joinFit(hello, core);
    }
  }
  talkMem.said(speech::lineHash(core), day);
  dlg.front = core;
  // ---- 9. the first page holds about six options: the services go under one header, the kindnesses come up when the
  // player asks how the speaker is keeping
  {
    auto shown = [&]() { int n = 1; for (const DlgOpt& o : dlg.opts) if (o.grp == 0) n++; return n; };   // (+ FAREWELL)
    if (shown() > speech::FIRST_PAGE) {
      int nserv = 0;
      for (const DlgOpt& o : dlg.opts) if (o.grp == 0 && serviceOpt(o)) nserv++;
      if (nserv >= 2) {
        size_t at = dlg.opts.size();
        for (size_t i = 0; i < dlg.opts.size(); i++)
          if (dlg.opts[i].grp == 0 && serviceOpt(dlg.opts[i])) { if (at == dlg.opts.size()) at = i; dlg.opts[i].grp = 1; }
        dlg.opts.insert(dlg.opts.begin() + (std::ptrdiff_t)at, DlgOpt{"WHAT CAN YOU OFFER?", A_GROUP, 1, 0});
        // a keeper with more to offer than one page holds: the house and kitchen business gets its own header
        int n1 = 0, nh = 0;
        for (const DlgOpt& o : dlg.opts) if (o.grp == 1) { n1++; if (o.action >= DLG_HOME && o.action < DLG_HOME_END) nh++; }
        if (n1 > speech::FIRST_PAGE - 1 && nh >= 2 && n1 - nh >= 2) {
          for (DlgOpt& o : dlg.opts) if (o.grp == 1 && o.action >= DLG_HOME && o.action < DLG_HOME_END) o.grp = 3;
          dlg.opts.insert(dlg.opts.begin() + (std::ptrdiff_t)at + 1, DlgOpt{"WHAT ELSE DO YOU DO HERE?", A_GROUP, 3, 0});
        }
      }
    }
    bool g1 = false;
    for (const DlgOpt& o : dlg.opts) if (o.action == A_GROUP && o.arg == 1) g1 = true;
    if (shown() > speech::FIRST_PAGE && g1)   // the keeper's word of the beasts about goes with the inn's news
      for (DlgOpt& o : dlg.opts) if (o.grp == 0 && o.action >= DLG_FOES && o.action < DLG_CRAFT) o.grp = 1;
    bool chat = false;
    for (const DlgOpt& o : dlg.opts) if (o.grp == 0 && lifeOptKind(o.action) == 3) chat = true;
    if (shown() > speech::FIRST_PAGE && chat)
      for (DlgOpt& o : dlg.opts) if (o.grp == 0 && lifeOptKind(o.action) == 2) o.grp = 2;   // (L_CHAT shows them: lifeChoose)
  }
  dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
  mode = Mode::Dialogue;
  sfx((int)Sfx::Talk, a.p);
}

void Game::closeDialogue() { if (mode == Mode::Dialogue) mode = Mode::Play; }

void Game::dialogueChoose(int oi) {
  if (oi < 0 || oi >= (int)dlg.opts.size()) return;
  dialogueChooseInner(oi);
  // (M7b) an option group whose options were all replaced (an answer with its own options) closes: back to the page
  if (dlg.group) {
    bool any = false;
    for (const DlgOpt& o : dlg.opts) if (o.grp == dlg.group && o.action != A_BACK) any = true;
    if (!any) {
      dlg.group = 0;
      dlg.opts.erase(std::remove_if(dlg.opts.begin(), dlg.opts.end(), [](const DlgOpt& o) { return o.action == A_BACK; }), dlg.opts.end());
    }
  }
  if (talkRec) talkRec->reply(oi, dlg, mode == Mode::Dialogue);
}

void Game::dialogueChooseInner(int oi) {
  if (oi < 0 || oi >= (int)dlg.opts.size()) return;
  DlgOpt o = dlg.opts[oi];
  int ai = findActor(dlg.actor);
  sfx((int)Sfx::MenuSelect, pl().p);
  switch (o.action) {
    case A_BYE: mode = Mode::Play; return;
    case A_GROUP: {   // (M7b) the services under one header: what they are, in the speaker's words, then the options
      dlg.group = (uint8_t)o.arg;
      std::vector<std::string> what;
      for (const DlgOpt& k : dlg.opts) {
        if (k.grp != dlg.group) continue;
        const char* w = k.action == A_REST ? "A ROOM FOR THE NIGHT" : k.action == A_RUMOR ? "WHATEVER NEWS I'VE HEARD"
                        : k.action == A_TRADE ? (dlg.role == Role::Innkeeper ? "WHAT'S ON MY SHELF" : "MY WARES")
                        : k.action == A_HEAL ? "THE GODS' BLESSING" : k.action == A_LEARN ? "A LESSON OR TWO"
                        : lifeOptKind(k.action) == 1 && dlg.role == Role::Innkeeper ? "A HOT MEAL"
                        : k.label.rfind("PROPERTY", 0) == 0 ? "THE DEEDS OF HOUSES FOR SALE" : k.label.rfind("TEACH ME A RECIPE", 0) == 0 ? "A RECIPE OR TWO"
                        : k.label.rfind("MAY I COOK", 0) == 0 ? "THE USE OF MY KITCHEN" : k.label.rfind("HORSES", 0) == 0 ? "GOOD HORSES"
                        : k.label.find("MY HORSE") != std::string::npos ? "A STALL FOR YOUR HORSE" : k.label.rfind("PAY MY TAXES", 0) == 0 ? "THE TAX BOOKS"
                        : k.label.rfind("GRIND", 0) == 0 ? "THE MILLSTONES" : nullptr;
        if (w && std::find(what.begin(), what.end(), w) == what.end()) what.push_back(w);
      }
      std::string list;
      for (size_t k = 0; k < what.size() && k < 4; k++) list += (k ? (k + 1 == std::min<size_t>(what.size(), 4) ? " AND " : ", ") : "") + what[k];
      dlg.text = list.empty() ? "HAVE A LOOK. WHAT WILL IT BE?" : (dlg.group == 3 ? "I CAN ALSO OFFER YOU " : "I CAN OFFER YOU ") + list + ". WHAT WILL IT BE?";
      if (!speech::fitsPanel(dlg.text)) dlg.text = "HAVE A LOOK. WHAT WILL IT BE?";
      dlg.opts.erase(std::remove_if(dlg.opts.begin(), dlg.opts.end(), [](const DlgOpt& k) { return k.action == A_BACK; }), dlg.opts.end());
      size_t at = dlg.opts.size();
      for (size_t k = 0; k < dlg.opts.size(); k++) if (dlg.opts[k].action == A_BYE) { at = k; break; }
      dlg.opts.insert(dlg.opts.begin() + (std::ptrdiff_t)at, DlgOpt{"SOMETHING ELSE.", A_BACK, 0, dlg.group});
      return;
    }
    case A_BACK:
      dlg.group = 0;
      dlg.opts.erase(std::remove_if(dlg.opts.begin(), dlg.opts.end(), [](const DlgOpt& k) { return k.action == A_BACK; }), dlg.opts.end());
      dlg.text = "OF COURSE. WHAT ELSE CAN I DO FOR YOU?";
      return;
    case A_TRADE: if (ai >= 0) openShop(actors[ai]); return;
    case A_REST: {
      // M0b: an inn lets you one of its rooms (until noon tomorrow) and tells you where it is. (M3b: a single-storey
      // inn lets its rooms on the ground floor, behind partitions; a taller one upstairs: the rooms are on whichever
      // floor has them, the first such floor from the ground up)
      int inn = inside && subBldg >= 0 && world.over.bldgs[subBldg].type == art::Building::Inn && world.over.bldgs[subBldg].genVer >= WORLDGEN_V7 ? subBldg : -1;
      Map up;
      std::vector<int> guestRooms;
      int roomFloor = -1;
      if (inn >= 0) {
        const Bldg& B = world.over.bldgs[inn];
        // a room already rented here is on its own floor
        const bool rented = lodgingActive() && lodging.bldg == inn && lodgingSleepHours(day, hour, lodging.untilDay) > 0;
        for (int f = rented ? std::clamp(lodging.floor, 0, B.floors() - 1) : 0; f < B.floors() && guestRooms.empty(); f++) {
          up = Map();
          genInterior(up, B, B.seed, f);
          for (int i = 0; i < (int)up.rooms.size(); i++) if (up.rooms[(size_t)i].kind == RoomKind::GuestRoom && up.rooms[(size_t)i].bedX >= 0) guestRooms.push_back(i);
          if (!guestRooms.empty()) roomFloor = f;
          if (rented) break;
        }
      }
      if (inn < 0 || guestRooms.empty()) {   // an old inn: beds in the common room, sleep where you stand
        if (gold < o.arg) { dlg.text = "YOU DON'T HAVE ENOUGH GOLD."; return; }
        gold -= o.arg;
        rest(8);
        dlg.text = "YOU WAKE WELL RESTED. GOOD MORNING!";
        dlg.opts = {{"FAREWELL.", A_BYE, 0}};
        return;
      }
      bool mine = lodgingActive() && lodging.bldg == inn && lodgingSleepHours(day, hour, lodging.untilDay) > 0;
      if (!mine) {
        if (gold < o.arg) { dlg.text = "YOU DON'T HAVE ENOUGH GOLD."; return; }
        gold -= o.arg;
        int pickR = guestRooms[hash32((uint32_t)inn * 2654435761u ^ (uint32_t)day * 40503u) % guestRooms.size()];
        lodging.bldg = inn; lodging.floor = roomFloor; lodging.room = pickR; lodging.untilDay = day + 1;
        sfx((int)Sfx::Coin, pl().p);
      }
      if (std::find(guestRooms.begin(), guestRooms.end(), lodging.room) == guestRooms.end()) lodging.room = guestRooms[0];
      lodging.floor = roomFloor;
      // where it is, as you come up the stairs (or, on the ground floor, as you stand at the bar): doors to the left /
      // right of the stairwell (the counter), nearest first
      const RoomInfo& R = up.rooms[(size_t)lodging.room];
      const bool ground = roomFloor == 0;
      int sx = up.down.valid() ? up.down.x : (ground ? (int)std::floor(pl().p.x / TILE) : up.w / 2);
      // (fix round 2: left and right are counted from the middle of the flight, both of its tiles: a door in front of
      // the flight's second tile is the first one on that side, as the player sees it)
      int sx0 = sx, sx1 = sx;
      if (up.down.valid()) {
        const int dn = (int)Prop::StairsDown + 1;
        while (up.propAt(sx0 - 1, up.down.y) == dn) sx0--;
        while (up.propAt(sx1 + 1, up.down.y) == dn) sx1++;
      }
      auto sideOf = [&](int x) { int d = 2 * x - (sx0 + sx1); return d < 0 ? -1 : (d > 0 ? 1 : 0); };
      auto distOf = [&](int x) { return std::abs(2 * x - (sx0 + sx1)); };
      int side = sideOf(R.doorX);
      std::vector<int> rows;
      for (const RoomInfo& Q : up.rooms) if (Q.doorX >= 0 && std::find(rows.begin(), rows.end(), Q.doorY) == rows.end()) rows.push_back(Q.doorY);
      int nth = 1;
      for (const RoomInfo& Q : up.rooms) {
        if (Q.doorX < 0 || &Q == &R || Q.doorY != R.doorY) continue;
        if (sideOf(Q.doorX) == side && distOf(Q.doorX) < distOf(R.doorX)) nth++;
      }
      static const char* ord[] = {"FIRST", "SECOND", "THIRD", "FOURTH", "FIFTH", "SIXTH"};
      std::string where = side == 0 ? "THE DOOR STRAIGHT AHEAD" : std::string("THE ") + ord[std::min(nth, 6) - 1] + " DOOR ON THE " + (side < 0 ? "LEFT" : "RIGHT");
      if (rows.size() >= 2) {
        int minRow = *std::min_element(rows.begin(), rows.end());
        where += R.doorY == minRow ? ", ON THE BACK SIDE" : ", ON THE FRONT SIDE";
      }
      std::string num = std::to_string((int)R.guest + 1);
      dlg.text = (mine ? "YOUR ROOM IS STILL YOURS UNTIL NOON: ROOM " : "ROOM ") + num + (ground ? ". THROUGH THE HALL, " : ". UP THE STAIRS, ") + where + "." +
                 (mine ? "" : " IT'S YOURS UNTIL NOON TOMORROW.");
      dlg.opts = {{ground ? "GO TO BED" : "GO UP TO BED", A_BED, 0}, {"LATER.", A_BYE, 0}};
      return;
    }
    case A_BED: {
      // straight up to your room: wake beside your own bed
      mode = Mode::Play;
      if (!lodgingActive() || !inside || subBldg != lodging.bldg) return;
      const int h = lodgingSleepHours(day, hour, lodging.untilDay);
      if (h <= 0) { say(kRoomDue); return; }
      if (subFloor != lodging.floor) changeFloor(lodging.floor);
      if (lodging.room >= 0 && lodging.room < (int)sub.rooms.size()) {
        const RoomInfo& R = sub.rooms[(size_t)lodging.room];
        int bx = R.bedX >= 0 ? R.bedX : R.r.cx(), by = R.bedX >= 0 ? R.bedY : R.r.cy();
        int best = -1, bd = 1 << 30;
        for (int y = R.r.y; y < R.r.y + R.r.h; y++)
          for (int x = R.r.x; x < R.r.x + R.r.w; x++) {
            if (sub.roomIndexAt(x, y) != lodging.room || sub.blocked(x, y)) continue;
            int pr = sub.propAt(x, y);
            if (pr == (int)Prop::DoorH + 1 || pr == (int)Prop::DoorV + 1) continue;
            int d = std::abs(x - bx) * 2 + std::abs(y - by) * 2 + (y < by ? 3 : 0);
            if (d < bd) { bd = d; best = y * sub.w + x; }
          }
        if (best >= 0) { pl().p = Vec2((best % sub.w) * TILE + 8.0f, (best / sub.w) * TILE + 10.0f); pl().vel = Vec2(); pl().knock = Vec2(); }
        // face the bed
        Vec2 to = Vec2(bx * TILE + 8.0f, by * TILE + 10.0f) - pl().p;
        pl().aim = len2(to) > 1 ? norm(to) : Vec2(0, -1);
        pl().face = std::fabs(to.x) > std::fabs(to.y) ? (to.x > 0 ? 2 : 3) : (to.y < 0 ? 1 : 0);
      }
      rest(h);
      say("YOU SLEEP IN YOUR ROOM.");
      return;
    }
    case A_RUMOR: {
      // M2 rumours (wayside.cpp): the nearest place the player has not heard of goes on the map as rumoured
      if (gold < o.arg) {
        dlg.text = "NO GOLD, NO GOSSIP. THE ALE ISN'T FREE EITHER.";
        // (M9 integration) the refused offer goes from the page: asking again would only hear the same words
        dlg.opts.erase(std::remove_if(dlg.opts.begin(), dlg.opts.end(), [](const DlgOpt& k) { return k.action == A_RUMOR; }), dlg.opts.end());
        return;
      }
      // M4 (VISION_PLAN 4.6): news of the realm that has reached this inn and the player has not heard yet comes first
      {
        int px = 0, py = 0;
        overworldTile(*this, px, py);
        const uint64_t who = ai >= 0 ? npcKey(actors[(size_t)ai]) : 0;
        const realm::WorldEvent* e = story::pickRumour(*this, world.ox + px, world.oy + py, false, who);
        if (e && !e->heard) {
          const int hs = ai >= 0 ? (actors[(size_t)ai].site >= 0 ? actors[(size_t)ai].site : (actors[(size_t)ai].bldg >= 0 ? world.over.bldgs[(size_t)actors[(size_t)ai].bldg].site : -1)) : -1;
          gold -= o.arg;
          sfx((int)Sfx::Coin, pl().p);
          dlg.text = story::hearEvent(*this, *e, hs >= 0 ? world.sites[(size_t)hs].culture : 0);
          for (size_t k = 0; k < dlg.opts.size(); k++) if (dlg.opts[k].action == A_RUMOR) { dlg.opts.erase(dlg.opts.begin() + (std::ptrdiff_t)k); break; }
          return;
        }
      }
      const std::string line = hearRumour();
      if (line.empty()) dlg.text = "CAN'T SAY I'VE HEARD ANYTHING NEW. KEEP YOUR COIN.";
      else {
        gold -= o.arg;
        sfx((int)Sfx::Coin, pl().p);
        dlg.text = "THEY SAY THERE'S " + line + ". I'VE MARKED IT ON YOUR MAP.";
      }
      for (size_t k = 0; k < dlg.opts.size(); k++) if (dlg.opts[k].action == A_RUMOR) { dlg.opts.erase(dlg.opts.begin() + (std::ptrdiff_t)k); break; }
      return;
    }
    case A_NEWS: {
      // a traveller's news: one rumour a day, free
      const uint64_t k = markKey(ai >= 0 ? npcKey(actors[(size_t)ai]) : 0, Mk::News);
      auto it = marks.find(k);
      if (it != marks.end() && it->second == day) dlg.text = "THAT'S ALL I KNOW FOR NOW. ASK ME AGAIN TOMORROW, IF WE'RE BOTH STILL ON THE ROAD.";
      else {
        // M4: half the time a traveller carries the realm's news (an event the player has not heard), else directions
        int px = 0, py = 0;
        overworldTile(*this, px, py);
        const realm::WorldEvent* e = (day + (int)(k & 0xFF)) % 2 == 0 ? story::pickRumour(*this, world.ox + px, world.oy + py, false, k) : nullptr;
        const std::string line = e && !e->heard ? std::string() : hearRumour();
        const cult::Culture* tc = world.cultureAtTile(px, py);
        if (e && !e->heard) { marks[k] = day; dlg.text = story::hearEvent(*this, *e, tc ? tc->id : 0); }
        else if (line.empty()) dlg.text = "THE ROADS HAVE BEEN QUIET. NOTHING WORTH THE TELLING.";
        else { marks[k] = day; dlg.text = "ON MY WAY HERE I PASSED " + line + ". MIND HOW YOU GO."; }
      }
      for (size_t k2 = 0; k2 < dlg.opts.size(); k2++) if (dlg.opts[k2].action == A_NEWS) { dlg.opts.erase(dlg.opts.begin() + (std::ptrdiff_t)k2); break; }
      return;
    }
    case A_ASK:
      if (ai < 0) return;
      pendingOffer_ = makeOffer(actors[ai]);
      dlg.text = pendingPitch_.empty() ? "THERE IS SOMETHING. " + pendingOffer_.desc + " I'LL PAY " + std::to_string(pendingOffer_.gold) + " GOLD." : pendingPitch_;
      // (fixer M7b r3) a giver who has not yet said who they are does so before the pitch (the title then names them)
      if (actors[(size_t)ai].npc && actors[(size_t)ai].human && talkNamed(actors[(size_t)ai]) && actors[(size_t)ai].role != Role::King &&
          !talkMem.introduced(npcKey(actors[(size_t)ai]))) {
        const std::string t = speech::joinFit("I'M " + speech::givenName(actors[(size_t)ai].name) + ".", dlg.text);
        if (t != dlg.text) { dlg.text = t; talkMem.flag(npcKey(actors[(size_t)ai]), speech::TM_INTRODUCED, day); }
      }
      dlg.opts = {{"I'LL DO IT.", A_ACCEPT, 0}, {"NOT RIGHT NOW.", A_DECLINE, 0}};
      return;
    case A_ACCEPT:
      acceptQuest(pendingOffer_);
      dlg.text = acceptLine(pendingOffer_);
      if (ai >= 0 && actors[(size_t)ai].npc && actors[(size_t)ai].human) {
        const Actor& ga = actors[(size_t)ai];
        talkMem.flag(npcKey(ga), speech::TM_KNOWS_PLAYER, day);
        // (fixer M7b r2, review: "the giver stays THE HERDER through the offer, yet the turn-in names them") a giver who
        // has not said who they are does so now, so the journal's and the return's name is one the player has heard
        if (talkNamed(ga) && ga.role != Role::King && !talkMem.introduced(npcKey(ga))) {
          const bool back = dlg.text.find("COME BACK") != std::string::npos;
          std::string t = dlg.text + " I'M " + speech::givenName(ga.name) + (back ? ", BY THE WAY." : ", BY THE WAY. ASK FOR ME WHEN YOU'RE BACK.");
          if (!speech::fitsPanel(t)) t = dlg.text + " I'M " + speech::givenName(ga.name) + ", BY THE WAY.";
          if (speech::fitsPanel(t)) { dlg.text = t; talkMem.flag(npcKey(ga), speech::TM_INTRODUCED, day); }
        }
      }
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return;
    case A_DECLINE: mode = Mode::Play; return;
    case A_DELIVER:
      for (auto& q : quests)
        if (q.id == o.arg && q.type == QType::Deliver && q.state == QState::Active) {
          completeQuest(q);   // the recipient pays (the parcel leaves the pack)
          dlg.text = "AT LAST! YOU HAVE MY THANKS, AND " + std::to_string(q.gold) + " GOLD AS PROMISED. TELL " + q.giverName + " IT CAME SAFE.";
          break;
        }
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return;
    case A_ESCORT:
      for (auto& q : quests)
        if (q.id == o.arg && q.type == QType::Missing && q.state == QState::Active) {
          q.flags |= QF_FOUND;
          const std::string place = q.target >= 0 && q.target < (int)world.sites.size() ? world.sites[(size_t)q.target].name : std::string("HERE");
          emit(Ev::QuestUpdate, pl().p, q.id, 2, q.subject + " FOLLOWS YOU: LEAD THEM OUT OF " + place);
          sfx((int)Sfx::QuestStart, pl().p, 1.1f);
        }
      mode = Mode::Play;
      return;
    case A_TOLLPAY: {
      const int si = o.arg, toll = tollOf(si);
      if (gold < toll) { dlg.text = "NO GOLD, NO CROSSING. COME BACK RICHER."; dlg.opts = {{"FAREWELL.", A_BYE, 0}}; return; }
      gold -= toll;
      if (si >= 0 && si < (int)world.sites.size()) marks[markKey(world.sites[(size_t)si].id, Mk::Toll)] = day;
      sfx((int)Sfx::Coin, pl().p);
      dlg.text = dlg.speaker + " COUNTS THE COINS TWICE. \"CROSS, LITTLE ONE. TODAY ONLY.\"";
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return;
    }
    case A_TOLLREFUSE:
      if (ai >= 0) {
        Actor& t = actors[(size_t)ai];
        t.hostile = true; t.npc = false; t.faction = Faction::Monster; t.aggro = true; t.target = pl().id;
        sfx((int)Sfx::Roar, t.p);
        emit(Ev::Shake, t.p, 0, 3);
      }
      mode = Mode::Play;
      say("THE TROLL ROARS AND COMES FOR YOU!");
      return;
    case A_TURNIN:
    {
      // (M7b) the reward handed over, said as it is: the gold, and the piece of gear when the job brought one
      int gold0 = 0;
      const size_t inv0 = inv.size();
      for (auto& q : quests)
        if (q.id == o.arg && q.state == QState::Complete) { gold0 = q.gold; completeQuest(q); break; }
      std::string extra;
      for (size_t k = inv0; k < inv.size(); k++)
        if (inv[k].flags & IF_QUESTREWARD) {
          const std::string& n = inv[k].name;
          const bool many = n.size() > 2 && n.back() == 'S' && n[n.size() - 2] != 'S';   // BOOTS, GAUNTLETS
          extra = std::string(many ? " AND TAKE THESE " : " AND TAKE THIS ") + n + " AS WELL. " + (many ? "THEY" : "IT") + " SHOULD SERVE YOU BETTER THAN ME.";
        }
      dlg.text = gold0 > 0 ? "HERE, COUNT IT IF YOU LIKE." + (extra.empty() ? std::string(" YOU'VE EARNED EVERY COIN.") : extra)
                           : "YOU HAVE MY THANKS. HERE, YOU'VE EARNED THIS.";
      if (!speech::fitsPanel(dlg.text)) dlg.text = "HERE, COUNT IT IF YOU LIKE, AND TAKE A LITTLE SOMETHING MORE.";
      if (ai >= 0 && actors[(size_t)ai].npc && actors[(size_t)ai].human) talkMem.flag(npcKey(actors[(size_t)ai]), speech::TM_THANKED | speech::TM_KNOWS_PLAYER, day);
    }
      for (size_t k = 0; k < dlg.opts.size(); k++)   // more rewards from the same giver stay on offer
        if (k != (size_t)oi && dlg.opts[k].action == A_TURNIN) { const Quest* q2 = questById(dlg.opts[k].arg); if (q2 && q2->state == QState::Complete) { dlg.opts = {dlg.opts[k], {"FAREWELL.", A_BYE, 0}}; return; } }
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return;
    case A_MAIN:
      advanceMain(o.arg);
      if (o.arg == 1) dlg.text = "LEGEND SAYS THE EMBER CROWN CAN BIND A DRAGON'S SOUL. ITS THREE SHARDS LIE WITH DRAUGR WARLORDS IN THE OLD RUINS. I HAVE MARKED THEM ON YOUR MAP. BRING THEM TO ME.";
      else dlg.text = "THEN GO, WITH THE BLESSINGS OF THE HOLD. ASHFANG NESTS ON " + lairName() + ". TAKE THIS GOLD FOR SUPPLIES.";
      if (o.arg == 3) giveGold(300);
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return;
    case A_HEAL:
      pl().hp = pl().maxHp;
      blessT = blessingSecs(); blessName = "TEMPLE BLESSING";
      recalcPlayer();
      emit(Ev::Heal, pl().p);
      sfx((int)Sfx::Heal, pl().p);
      dlg.text = "THERE. YOUR WOUNDS ARE MENDED. GO WITH THE GODS, AND WALK IN THEIR LIGHT, CHILD.";
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return;
    case A_LEARN: {
      int price = o.arg == (int)Spell::Heal ? 120 : 300;
      if (gold < price) { dlg.text = "COME BACK WHEN YOU HAVE THE GOLD."; return; }
      gold -= price;
      spellsKnown |= (uint8_t)(1 << o.arg);
      spell = (Spell)o.arg;
      sfx((int)Sfx::LevelUp, pl().p, 1.2f);
      dlg.text = std::string("YOU HAVE LEARNED ") + spellName((Spell)o.arg) + ". USE IT WISELY.";
      dlg.opts.erase(dlg.opts.begin() + oi);
      return;
    }
    default:
      // M4: the lanes' own option ranges (game.h DLG_*)
      if (o.action >= DLG_STORY && o.action < DLG_WAR && storyChoose(o)) return;
      if (o.action >= DLG_WAR && o.action < DLG_END && warChoose(o)) return;
      if (o.action >= DLG_LIFE && o.action < DLG_LIFE_END && lifeChoose(o)) return;
      if (o.action >= DLG_FOES && o.action < DLG_CRAFT && foeChoose(o)) return;      // M6
      if (o.action >= DLG_CRAFT && o.action < DLG_M6_END && craftChoose(o)) return;  // M6
      if (o.action >= DLG_HOME && o.action < DLG_HOME_END && homeChoose(o)) return;  // M7
      if (o.action >= DLG_PARTY && o.action < DLG_PARTY_END && partyChoose(o)) return;  // M8 (party_talk.cpp)
      if (o.action >= DLG_SKILLS && o.action < DLG_SKILLS_END && SkillOps::choose(*this, o)) return;  // M9 (skills_game.cpp)
      if (o.action >= DLG_MAGIC && o.action < DLG_MAGIC_END && MagicOps::choose(*this, o)) return;    // M9 (magic_game.cpp)
      if (o.action >= DLG_SEA && o.action < DLG_SEA_END && SeaOps::choose(*this, o)) return;          // M10 (sea_game.cpp)
      if (o.action >= DLG_SERVICE && o.action < DLG_SERVICE_END && ServiceOps::choose(*this, o)) return;  // M12 (service_game.cpp)
      if (o.action >= DLG_LORD && o.action < DLG_LORD_END && LordOps::choose(*this, o)) return;          // M12 (lord_game.cpp)
      mode = Mode::Play;
      return;
  }
}

void Game::rest(int hours) {
  if (homeRiding()) homeDismount(home::Dismount::Sleep);   // M7
  MagicOps::rested(*this, hours);   // M9: the prepared spells may be swapped at rest
  hour += hours;
  while (hour >= 24) { hour -= 24; day++; }
  Actor& p = pl();
  p.hp = p.maxHp; mp = maxMp; stamina = maxSt;
  sleepFade = 1.5f;
  lifeSlept(hours, true);   // M5 (15.2): every rest is in a bed, a hammock, a mat or a bedroll: Rested
  partyRested(hours);       // M8: the companions rest too (party_game.cpp)
  sfx((int)Sfx::Heal, p.p, 0.7f);
}

// ------------------------------------------------------------------ shops
// (M6, VISION_PLAN 7.5 rule 4) shops stock by the settlement's danger (item levels D - 2 .. D + 2), NEVER by the
// player's level; merchants top out at Rare, a capital's at Epic for a hero Honoured there (rep >= 40); legendaries are
// never sold. Gear is made in the settlement's culture (its forms, its alloys from band 4 up). Mining settlements sell
// their local ores and ingots (craft::oresAt), smiths iron and steel ingots, workshops their materials (tanner:
// leather, smelter: ingots, weaver: cloth, sawmill: timber); with a census, material prices follow its stock.
namespace {
int siteOfActor(const World& w, const Actor& a) {
  if (a.site >= 0 && a.site < (int)w.sites.size()) return a.site;
  if (a.bldg >= 0 && a.bldg < (int)w.over.bldgs.size()) return w.over.bldgs[(size_t)a.bldg].site;
  return -1;
}
int urbanOf(const World& w, int si) {
  if (si < 0 || si >= (int)w.sites.size()) return 0;
  const Site& S = w.sites[(size_t)si];
  if (S.capital) return 3;
  return S.type == SiteType::City ? 2 : S.type == SiteType::Town ? 1 : 0;
}
}  // namespace

std::vector<Item> Game::shopStock(const Actor& a) {
  Rng r(hash32((uint32_t)npcKey(a)) ^ (uint32_t)(day / 2) ^ (uint32_t)seed);
  std::vector<Item> s;
  const int si = siteOfActor(world, a);
  const int D = std::clamp(si >= 0 ? world.sites[(size_t)si].level : 1, 1, gear::MAX_D);
  const cult::Culture* cc = si >= 0 ? world.cultureOf(si) : nullptr;
  if (cc && world.src) cc = &world.src->culture(cc->id);
  bool epicOk = false;
  if (si >= 0 && world.sites[(size_t)si].capital) {
    const Kingdom* K = world.kingdomOf(si);
    epicOk = K && realm.rep(K->id) >= gear::REP_HONOURED;
  }
  auto gearOf = [&](ItemKind k, int sub) {
    const int il = std::clamp(D - gear::SHOP_BAND_SPREAD + r.irange(2 * gear::SHOP_BAND_SPREAD + 1), 1, gear::MAX_D);
    // (M8b) a shop's or a merchant's shelf: capped at Rare (Epic in a capital at Honoured), and gated by the
    // settlement's band, never by the player's level (7.5 rule 4): the shelf's "finder" is the settlement
    gear::DropSource ds;
    ds.D = D;
    ds.src = LootOps::shopSrc(a.role);
    ds.playerLevel = D;
    ds.culture = cc ? cc->id : 0;
    ds.atMost = epicOk ? Rarity::Epic : Rarity::Rare;
    const Rarity rar = gear::rollRarity(r, ds);
    Item it = gear::makeGearC(r, k, sub, il, rar, cc);
    it.flags |= IF_SHOP;
    s.push_back(it);
  };
  auto stuff = [&](craft::Stuff st, int n) { s.push_back(craft::makeStuff(st, n)); };
  const art::Building where = a.bldg >= 0 && a.bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)a.bldg].type : art::Building::House;
  const bool mining = si >= 0 && (world.sites[(size_t)si].special == (uint8_t)ew::Specialty::Mining || world.sites[(size_t)si].archetype == (uint8_t)ew::Archetype::Mining);
  // the land's ores (richest first) where the settlement stands
  std::vector<craft::OreSource> ores;
  if (world.src && si >= 0) ores = craft::oresAt(*world.src, world.ox + world.sites[(size_t)si].ex, world.oy + world.sites[(size_t)si].ey);
  auto localOres = [&](int kinds, int lo, int hi) {
    int n = 0;
    for (const craft::OreSource& o : ores) {
      if (n >= kinds) break;
      stuff(craft::oreStuff(o.ore), lo + r.irange(hi - lo + 1));
      n++;
    }
    if (n && std::none_of(ores.begin(), ores.end(), [](const craft::OreSource& o) { return o.ore == ew::Ore::Coal; })) stuff(craft::Stuff::Coal, 3 + r.irange(4));
  };
  auto localIngots = [&](int kinds) {
    int n = 0;
    for (const craft::OreSource& o : ores) {
      if (n >= kinds) break;
      const craft::Stuff ig = craft::ingotOf(o.ore);
      if (ig == craft::Stuff::COUNT || ig == craft::Stuff::AlloyIngot) continue;
      stuff(ig, 2 + r.irange(3));
      n++;
    }
  };
  const bool workshop = a.role != Role::Smith && craft::isStation(where);
  if (workshop) {
    switch (where) {
      case art::Building::Tanner: stuff(craft::Stuff::Leather, 4 + r.irange(4)); stuff(craft::Stuff::Hide, 2 + r.irange(3)); break;
      case art::Building::Smelter: localIngots(2); stuff(craft::Stuff::BronzeIngot, 2 + r.irange(3)); stuff(craft::Stuff::Coal, 4 + r.irange(4)); break;
      case art::Building::Weaver: stuff(craft::Stuff::Cloth, 4 + r.irange(4)); break;
      case art::Building::Sawmill: stuff(craft::Stuff::Timber, 6 + r.irange(6)); break;
      default: break;
    }
  } else switch (a.role) {
    case Role::Smith: {
      const bool spear = cc && cc->arms.polearm != cult::Polearm::None;
      for (int i = 0; i < 4; i++) gearOf(ItemKind::Weapon, r.irange(spear ? 6 : 5));
      gearOf(ItemKind::Armor, 0); gearOf(ItemKind::Helmet, 0); gearOf(ItemKind::Shield, 0);
      gearOf(ItemKind::Bow, 0);
      s.push_back(makeArrows(25));
      gearOf(ItemKind::Gloves, 0); gearOf(ItemKind::Boots, 0);
      // the smith's metal: iron always, steel from the second band, bronze in the first; a mining town's own ores
      stuff(craft::Stuff::IronIngot, 3 + r.irange(3));
      if (D >= 9) stuff(craft::Stuff::SteelIngot, 2 + r.irange(2));
      else stuff(craft::Stuff::BronzeIngot, 2 + r.irange(3));
      stuff(craft::Stuff::Leather, 2);
      stuff(craft::Stuff::Timber, 3);
      if (mining) localOres(2, 4, 8);
      // a culture alloy the player has learned: the smith sells a few ingots and its reagent
      if (cc && cc->id)
        for (size_t k = 0; k < cc->arms.alloys.size(); k++)
          if (craft.knows(cc->id, (uint8_t)(k + 1), craft::SecretKind::AlloyRecipe)) {
            s.push_back(craft::makeStuff(craft::Stuff::AlloyIngot, 1 + r.irange(2), cc->id, (uint8_t)(k + 1), cc));
            s.push_back(craft::makeStuff(craft::Stuff::Reagent, 2, cc->id, (uint8_t)(k + 1), cc));
          }
      break;
    }
    case Role::Mage:
      gearOf(ItemKind::Staff, 0);
      s.push_back(makePotion(PotionType::Magicka, 1)); s.back().count = 4;
      gearOf(r.f() < 0.5f ? ItemKind::Ring : ItemKind::Amulet, 0);
      gearOf(r.f() < 0.5f ? ItemKind::Ring : ItemKind::Amulet, 0);
      break;
    case Role::Priest:
      s.push_back(makePotion(PotionType::Health, 0)); s.back().count = 5;
      s.push_back(makePotion(PotionType::Health, 1)); s.back().count = 3;
      s.push_back(makePotion(PotionType::Magicka, 0)); s.back().count = 3;
      gearOf(r.f() < 0.5f ? ItemKind::Ring : ItemKind::Amulet, 0);
      break;
    // M2 wayside traders (wayside.cpp): a hunter's pelts, meat and arrows; a fisher's catch; an herbalist's herbs and potions
    case Role::Hunter:
      s.push_back(makeMisc(0)); s.back().count = 3;
      s.push_back(makeMisc(8)); s.back().count = 2;
      if (r.f() < 0.5f) s.push_back(makeMisc(9));
      s.push_back(makeFood(1)); s.back().count = 4;
      s.push_back(makeFood(7)); s.back().count = 3;
      s.push_back(makeArrows(40));
      gearOf(ItemKind::Bow, 0);
      stuff(craft::Stuff::Hide, 3 + r.irange(3));
      break;
    case Role::Fisher:
      s.push_back(makeFood(4)); s.back().count = 6;
      s.push_back(makeFood(5)); s.back().count = 3;
      s.push_back(makeFood(6)); s.back().count = 2;
      break;
    case Role::Herbalist:
      s.push_back(makeMisc(7)); s.back().count = 4;
      s.push_back(makeMisc(10)); s.back().count = 2;
      s.push_back(makeMisc(11)); s.back().count = 3;
      s.push_back(makeMisc(12)); s.back().count = 5;
      s.push_back(makePotion(PotionType::Health, 0)); s.back().count = 3;
      s.push_back(makePotion(PotionType::Stamina, 0)); s.back().count = 2;
      s.push_back(makePotion(PotionType::Magicka, 0)); s.back().count = 2;
      s.push_back(makeFood(8)); s.back().count = 3;
      break;
    case Role::Innkeeper:
      s.push_back(makeFood(0)); s.back().count = 6;
      s.push_back(makeFood(1)); s.back().count = 4;
      s.push_back(makeFood(3)); s.back().count = 3;
      s.push_back(makeFood(2)); s.back().count = 8;
      break;
    default:
      s.push_back(makePotion(PotionType::Health, 0)); s.back().count = 4;
      s.push_back(makePotion(PotionType::Health, 1)); s.back().count = 2;
      s.push_back(makePotion(PotionType::Stamina, 0)); s.back().count = 3;
      s.push_back(makeArrows(30));
      s.push_back(makeFood(0)); s.back().count = 4;
      gearOf(ItemKind::Weapon, (int)WeaponType::Dagger);
      gearOf(ItemKind::Helmet, 0);
      if (r.f() < 0.5f) gearOf(r.f() < 0.5f ? ItemKind::Ring : ItemKind::Amulet, 0);
      gearOf(ItemKind::Cloak, 0);
      gearOf(r.f() < 0.5f ? ItemKind::Boots : ItemKind::Gloves, 0);
      // a mining settlement's market sells what comes out of its hills: ores and the smelter's ingots
      if (mining) { localOres(2, 5, 10); localIngots(2); }
      break;
  }
  homeShopStock(a, s);   // M7: seeds by the culture's staples, farm tools, hay, saddlery (home_game.cpp)
  MagicOps::shopStock(*this, a, s);   // M9: tomes on a mage's or a scholar's shelf (magic_game.cpp)
  SkillOps::shopStock(*this, a, s);   // M9: skill books on a scholar's or a trainer's shelf (skills_game.cpp)
  SeaOps::shopStock(*this, a, s);     // M10: a fishmonger's catch, a chandler's bait and charts (sea_game.cpp)
  // (M5 census) material prices follow the settlement's stock of the good (price % of list: shortages raise it)
  if (si >= 0) {
    if (const life::Census* cs = life.census(world, si))
      for (Item& it : s)
        if (it.kind == ItemKind::Material) {
          const int pct = cs->price[(size_t)craft::stuffInfo((craft::Stuff)it.sub).good];
          if (pct > 0) it.value = std::max(1, it.value * pct / 100);
        }
  }
  // background prices (blacksmith's child at smiths, urchin at merchants): the shelf shows what you pay
  float pf = priceFactor(a.role);
  if (pf != 1.0f)
    for (Item& it : s) if (it.kind != ItemKind::Arrows) it.value = std::max(1, (int)std::lround(it.value * pf));
  return s;
}

void Game::openShop(Actor& a) {
  shop.actor = a.id;
  shop.title = a.name;
  shop.key = npcKey(a);
  // (M6, 7.5 rule 8) the merchant's purse: 300 (village) .. 3000 (capital) gold per restock (every 2 days)
  {
    auto& pu = craft.live.purse[shop.key];
    if (pu.first != day / 2 + 1) pu = {day / 2 + 1, gear::merchantGold(urbanOf(world, siteOfActor(world, a)))};
  }
  auto it = shopCache_.find(shop.key);
  if (it != shopCache_.end() && it->second.first == day / 2) shop.stock = it->second.second;
  else { shop.stock = shopStock(a); shopCache_[shop.key] = {day / 2, shop.stock}; }
  mode = Mode::Shop;
}

bool Game::buy(int si) {
  if (si < 0 || si >= (int)shop.stock.size()) return false;
  Item it = shop.stock[si];
  int price = it.value * (it.kind == ItemKind::Arrows ? 1 : 1);
  if (it.stackable()) { price = it.value; it.count = it.kind == ItemKind::Arrows ? it.count : 1; if (it.kind == ItemKind::Arrows) price = it.count; }
  if (gold < price) { sfx((int)Sfx::MenuBack, pl().p); return false; }
  if (it.kind == ItemKind::Potion) {   // (M6) the potion belt holds 10
    int held = 0;
    for (const Item& o : inv) if (o.kind == ItemKind::Potion) held += o.count;
    if (held + it.count > gear::POTION_CARRY) { say("YOU CAN CARRY NO MORE THAN 10 POTIONS"); sfx((int)Sfx::MenuBack, pl().p); return false; }
  }
  gold -= price;
  SkillOps::traded(*this, price);   // (M9) Speech
  { auto pu = craft.live.purse.find(shop.key); if (pu != craft.live.purse.end()) pu->second.second += price; }
  // (M6, 15.11) buying from a smith grows the apprenticeship's trust
  // (M6 fixer r3) only the smith's own work counts (gear he forged and stocked: IF_SHOP, never goods the player sold him
  // or plain materials), and at most TRUST_BUY_CAP per restock, so a buy-and-sell-back loop cannot farm it
  {
    const int k = findActor(shop.actor);
    if (k >= 0 && actors[(size_t)k].role == Role::Smith && (it.flags & IF_SHOP) && !it.stackable()) {
      auto& tb = craft.live.trustBuy[shop.key];
      if (tb.first != day / 2 + 1) tb = {day / 2 + 1, 0};
      const int add = std::min(2, craft::TRUST_BUY_CAP - tb.second);
      if (add > 0) { tb.second += add; craft.trust[shop.key] = (uint8_t)std::min(100, (int)craft.trust[shop.key] + add); }
    }
  }
  addItem(it, false);
  if (shop.stock[si].stackable() && shop.stock[si].kind != ItemKind::Arrows && shop.stock[si].count > 1) shop.stock[si].count--;
  else shop.stock.erase(shop.stock.begin() + si);
  shopCache_[shop.key] = {day / 2, shop.stock};
  sfx((int)Sfx::Buy, pl().p);
  return true;
}

bool Game::sell(int ii) {
  if (ii < 0 || ii >= (int)inv.size()) return false;
  Item& it = inv[ii];
  if (it.kind == ItemKind::Quest) return false;
  if (it.kind == ItemKind::Misc && it.sub == home::MISC_DEED) { say("A DEED IS NOT SOLD OVER A COUNTER"); return false; }   // M7
  int price = gear::sellPrice(it);
  price = std::max(1, (int)((float)price * SkillOps::sellMul(*this) + 0.5f));   // (M9 integration) Speech pays more
  // M7: a crop foreign to the merchant's people fetches 25..50 % more (8.4's small trading game), and what is sold
  // joins the settlement's stores (15.11: the census stock)
  int homeSite = -1;
  ew::Good homeGood = ew::Good::COUNT;
  if (home::isHomeGood(it)) {
    const int k = findActor(shop.actor);
    homeSite = k >= 0 ? siteOfActor(world, actors[(size_t)k]) : -1;
    if (homeSite >= 0) price += price * home::foreignPremiumPct(it, world.sites[(size_t)homeSite].culture, *this) / 100;
    homeGood = home::goodOf(it);
  }
  // (M6, 7.5 rule 8) the merchant pays from a purse that refills each restock. (M6 fixer r2) A piece worth more than he
  // has left he takes for all he has (the shop says so on its button); with an empty purse he buys nothing.
  auto pu = craft.live.purse.find(shop.key);
  if (pu != craft.live.purse.end()) {
    if (pu->second.second <= 0) {
      say("THE MERCHANT HAS NO GOLD LEFT UNTIL HE RESTOCKS");
      sfx((int)Sfx::MenuBack, pl().p);
      return false;
    }
    price = std::min(price, pu->second.second);
    pu->second.second -= price;
  }
  gold += price;
  SkillOps::traded(*this, price);   // (M9) Speech
  if (homeSite >= 0 && homeGood != ew::Good::COUNT && world.sites[(size_t)homeSite].settlement() && life.census(world, homeSite))
    life.supply(world.sites[(size_t)homeSite].id, homeGood, 1);
  Item sold = it;
  sold.count = it.kind == ItemKind::Arrows ? it.count : 1;
  sold.flags &= (uint8_t)~IF_SHOP;   // (M6 fixer r3) bought back, it is no longer the smith's own work (no trust)
  if (it.stackable() && it.kind != ItemKind::Arrows && it.count > 1) it.count--;
  else dropItem(ii);
  sfx((int)Sfx::Coin, pl().p);
  // sold goods join the shelf (stacking onto a matching stack) so you can buy them back
  bool stacked = false;
  if (sold.stackable())
    for (auto& s : shop.stock)
      if (s.same(sold)) { s.count += sold.count; stacked = true; break; }
  if (!stacked) shop.stock.push_back(sold);
  shopCache_[shop.key] = {day / 2, shop.stock};
  return true;
}

// travel and death: rpg/sim/travel.cpp (M2)

// ------------------------------------------------------------------ save / load
// SAVE_VER 12 (M6: items gain item level, material, alloy, maker culture, form, seed, affixes, unique power and flags
// (items.cpp writeItem); the craft block appended after the life block). SAVE_VER 11 (M5: the life block appended after the story block). SAVE_VER 10 (M4: the realm and story blocks appended after marks). SAVE_VER 9 (M3c: the layout of 8, bumped with the Wildlands world generation). SAVE_VER 8 (M3b: the layout of 7, bumped with the builder's world generation). SAVE_VER 7 (M3: the appearance block gains people, homeland and personal heraldry; nothing else moved).
// SAVE_VER 6 (M2). Owner, 2026-10-04: old saves are not a concern, so only this version loads; an older file is refused
// and the title offers a new game ("this save is from an older version"). The layout is frozen for M2 after phase A
// (the lanes fill the new fields, they do not move them); any later change bumps SAVE_VER and regenerates
// tests/fixtures/save_v6.bin with `save_test --make-fixture`.
//
// Everything that points into the world is saved by stable id (Gid), never by handle (a vector index), because an
// endless world loads its sites and buildings in whatever order the player walked: sites by Site::id, buildings by
// (Bldg::id, owner Site::id), map keys (looted chests, killed spawns) as (kind, id, floor). Overworld chests of an
// endless world are keyed by global tile (Game::lootKey). M2 retired the classic island: worldKind is always 1.
//
//   header   magic 'EMBV', ver 7, worldKind u8 (1 endless; anything else is refused), genVer u32 (ew::ENDLESS_GEN_VER),
//            seed u64, time f32, hour f32, day i32
//   window   origin ox i32, oy i32 (positions below are window-local pixels)
//   where    inside u8, subSite (site ref), subBldg (bldg ref), subFloor i32, x f32, y f32
//   player   hp, mp, stamina f32; level, xp, gold, perk points i32; baseHp, maxMp, maxSt f32; inventory; 11 equip
//            slots i32; spellsKnown u8, spell u8; kills, dungeons i32; blessT f32, blessName str; lastTown (site ref)
//   quests   count, then id, type, state, title, desc, giverName, giverSite (site ref), giverBldg (bldg ref),
//            giverSlot i32, target (site ref), mon u8, need, have, gold, xp, stage i32,
//            (v6) targetId u64, hasPos u8, tgx i32, tgy i32, giverId u64, subject str, destBldg (bldg ref),
//            deadlineDay i32, flags u32; then nextQuestId, trackedQuest i32
//   npcs     count, then npcKey u64 (already id-based) + done i32
//   looted   count, then (map ref, tile u32) or an endless overworld key (map kind 0xE + the raw key)
//   killed   count, then (map ref, n, n values: site ref + slot for overworld spawns (slot < 4096), den id + day for
//            dens, slot otherwise)
//   sites    count, then site id u64 + flags u8 (1 discovered, 2 cleared, 4 rumoured)
//   char     background u8, storyFlags u32, appearance (u16 length + fields; v7 appends people u8, homeland u64,
//            heraldry: field u32, field2 u32, charge u32, division u8, chargeKind u8, emblem u8, glyphSeed u32, shape u8)
//   lodging  bldg ref, floor i32, room i32, untilDay i32
//   explored count, then rx i32, ry i32, 128 bytes (the fog-of-war bits of one region)
//   marks    (v6) count, then key u64 + value i32, in key order
//   realm    (v10) u32 byte length + the realm block (realm::Realm::serialize: its own version byte first)
//   story    (v10) u32 byte length + the story block (story::Engine::serialize: its own version byte first; v13: block v3)
//   life     (v11) u32 byte length + the life block (life::Life::serialize: its own version byte first)
//   craft    (v12) u32 byte length + the craft block (craft::Knowledge::serialize: its own version byte first)
//   home     (v14) u32 byte length + the home block (home::Homes::serialize: its own version byte first)
//   talk     (v15) u32 byte length + the talk block (speech::Memory::serialize: its own version byte first)
//   party    (v16) u32 byte length + the party block (party::Party::serialize: its own version byte first)
//   skills   (v20) u32 byte length + the skills block (skills::Skills::serialize: its own version byte first)
//   magic    (v20) u32 byte length + the magic block (magic::Book::serialize: its own version byte first)
//   sea      (v21) u32 byte length + the sea block (sea::Sea::serialize: its own version byte first)
//   service  (v22) u32 byte length + the service block (service::Service::serialize: its own version byte first)
//   lordship (v22) u32 byte length + the lordship block (lord::Lordship::serialize: its own version byte first)
// refs: site ref = u64 id (0 none); bldg ref = u64 id + u64 owner site id (0 none); map ref = u8 kind (0 overworld,
// 1 cave/ruin, 2 building, 3 dens) + u64 id + u64 owner + u8 floor.
static constexpr uint32_t SAVE_MAGIC = 0x454D4256;   // EMBV
static constexpr uint32_t SAVE_VER = 22;  // 22: M12 Rise and Fall (the service and lordship blocks after the sea block;
                                          //    the realm block v4: camps, lairs, realm origins, vassals, the player's
                                          //    realm; ENDLESS_GEN_VER unchanged). Old saves are refused.
                                          // 21: M10 Beyond the Mist (the sea block after the magic block: boats, the one
                                          //    aboard, ports known, the lands' chart chains and contact, catches;
                                          //    ENDLESS_GEN_VER 23). Old saves are refused ("start a new adventure").
                                          // 20: M9 Arcana & Mastery (the skills and magic blocks after the party block;
                                          //    ENDLESS_GEN_VER unchanged). A SAVE_VER 19 save still loads (owner
                                          //    15.29.5: "give old saves default skills based on their level"): the
                                          //    same layout without the two blocks; skills::defaultsForLevel fills them;
                                          // 19: M8b Treasure (items carry 4 affix slots, the Mythic rarity and
                                          //    powers; ENDLESS_GEN_VER unchanged);
                                          // 18: fixer M8 r3 (party block v4: the fallen's gender);
                                          // 17: fixer M8 r1 (party block v3: a companion's look fixed when they join);
                                          // 16: M8 Fellowship (the party block after the talk block: the companions,
                                          //    their approval, bond, gear and memories, the fallen and their graves;
                                          //    ENDLESS_GEN_VER 21);
                                          // 15: M7b Cities & Voice (the talk block after the home block: the people
                                          //    met, introductions, the lines said lately; ENDLESS_GEN_VER 17: the
                                          //    cities' plan);
                                          // 14: M7 Home (the home block after the craft block: the player's houses, lots,
                                          //    yards, crops, animals, stores, discovered styles, the horse ridden;
                                          //    ENDLESS_GEN_VER 16);
                                          // 13: M6b Sagas (the story block's own version 3: the generated stories'
                                          //    repetition guard; running sagas are saved by their spec id);
                                          // 12: M6 Steel (the item layout grows: ilvl, material, alloy, culture, form,
                                          //    seed, 3 affixes, unique power, flags; the craft block after the life
                                          //    block: smithing skill, secrets, trust; ENDLESS_GEN_VER 15);
                                          // 11: M5 Hearth and Hall (the life block after the story block: the census
                                          //    deltas, settlement stock and moods, the player's buffs; ENDLESS_GEN_VER 14);
                                          // 10: M4 Banners (the realm and story blocks after marks; ENDLESS_GEN_VER 13);
                                          // 9: M3c Wildlands (the layout of 8; the world's biomes, flora and wildlife
                                          //    are new, ENDLESS_GEN_VER 12, so older adventures start anew);
                                          // 8: M3b Builders & Societies (the world is built by the builder: a new
                                          //    generation, so older adventures start anew); 7: M3 (appearance: people,
                                          //    homeland, personal heraldry)

int Game::currentSaveVersion() { return (int)SAVE_VER; }
// the oldest SAVE_VER that still loads (M10: only the current one; M9's SAVE_VER 19 exception ended with the generator
// change, owner: old saves are not a concern)
static constexpr uint32_t SAVE_VER_MIN = 22;   // (M12) only the current one
int Game::saveVersion(const std::vector<uint8_t>& in) {
  BinR r(in);
  if (r.u32() != SAVE_MAGIC || r.bad) return 0;
  uint32_t v = r.u32();
  return r.bad ? 0 : (int)v;
}
bool Game::saveFromOlderGenerator(const std::vector<uint8_t>& in) {
  BinR r(in);
  if (r.u32() != SAVE_MAGIC || r.bad) return false;
  { const uint32_t v = r.u32(); if (v < SAVE_VER_MIN || v > SAVE_VER || r.bad) return false; }
  const bool endless = r.u8() == 1;
  const int genVer = (int)r.u32();
  if (r.bad) return false;
  return !endless || genVer < ew::ENDLESS_GEN_VER;
}

namespace {
// The appearance block: u16 byte length, then the fields in this order.
void writeAppearance(BinW& w, const Appearance& a) {
  std::vector<uint8_t> blk;
  BinW b(blk);
  b.str(a.name); b.u8(a.female ? 1 : 0); b.u8(a.build); b.u8(a.skinTone); b.u32(a.skin); b.u8(a.hair); b.u32(a.hairColor);
  b.u8(a.beard ? 1 : 0); b.u32(a.eyeColor); b.u32(a.topColor); b.u32(a.bottomColor); b.u8(a.created ? 1 : 0);
  // v7 (M3): people, homeland, personal heraldry
  b.u8(a.people); b.u64(a.homeland);
  b.u32(a.heraldry.field); b.u32(a.heraldry.field2); b.u32(a.heraldry.charge); b.u8(a.heraldry.division);
  b.u8(a.heraldry.chargeKind); b.u8(a.heraldry.emblem); b.u32(a.heraldry.glyphSeed); b.u8(a.heraldry.shape);
  w.u16((uint16_t)blk.size());
  for (uint8_t c : blk) w.u8(c);
}
bool readAppearance(BinR& r, Appearance& a) {
  int n = r.u16();
  std::vector<uint8_t> blk;
  for (int i = 0; i < n && !r.bad; i++) blk.push_back(r.u8());
  if (r.bad) return false;
  BinR b(blk);
  Appearance d;
  auto more = [&] { return b.p < blk.size() && !b.bad; };
  if (more()) d.name = b.str();
  if (more()) d.female = b.u8() != 0;
  if (more()) d.build = b.u8();
  if (more()) d.skinTone = b.u8();
  if (more()) d.skin = b.u32();
  if (more()) d.hair = b.u8();
  if (more()) d.hairColor = b.u32();
  if (more()) d.beard = b.u8() != 0;
  if (more()) d.eyeColor = b.u32();
  if (more()) d.topColor = b.u32();
  if (more()) d.bottomColor = b.u32();
  if (more()) d.created = b.u8() != 0;
  if (more()) {   // v7 (M3)
    d.people = b.u8(); d.homeland = b.u64();
    d.heraldry.field = b.u32(); d.heraldry.field2 = b.u32(); d.heraldry.charge = b.u32(); d.heraldry.division = b.u8();
    d.heraldry.chargeKind = b.u8(); d.heraldry.emblem = b.u8(); d.heraldry.glyphSeed = b.u32(); d.heraldry.shape = b.u8();
  }
  if (b.bad) return false;
  a = d;
  return true;
}

// handle <-> stable id
struct Refs {
  World& w;
  ew::Gid site(int h) const { return h >= 0 && h < (int)w.sites.size() ? w.sites[(size_t)h].id : 0; }
  // an id that is not a site's (a damaged or foreign save) loads as "none", never as some other record
  int siteFrom(ew::Gid id) const {
    if (!id) return -1;
    return w.ensureSite(id);
  }
  void bldg(BinW& o, int h) const {
    if (h < 0 || h >= (int)w.over.bldgs.size()) { o.u64(0); o.u64(0); return; }
    const Bldg& b = w.over.bldgs[(size_t)h];
    o.u64(b.id); o.u64(site(b.site));
  }
  int bldgFrom(BinR& r) const {
    ew::Gid id = r.u64(), owner = r.u64();
    if (!id || r.bad) return -1;
    int sh = owner ? siteFrom(owner) : -1;
    if (sh >= 0) w.ensureSiteRecords(sh);
    return w.bldgHandle(id);
  }
  // Game::mapKey(): 0 overworld, 1 + site (cave/ruin), 100000 + bldg (ground floor), 10000000 + bldg * 16 + floor
  // (upper floors); -1 the dens' record in killedSlots
  void map(BinW& o, int key) const {
    if (key == 0) { o.u8(0); o.u64(0); o.u64(0); o.u8(0); return; }
    if (key == -1) { o.u8(3); o.u64(0); o.u64(0); o.u8(0); return; }
    if (key < 100000) { o.u8(1); o.u64(site(key - 1)); o.u64(0); o.u8(0); return; }
    int b = key < 10000000 ? key - 100000 : (key - 10000000) / 16, f = key < 10000000 ? 0 : (key - 10000000) % 16;
    o.u8(2); bldg(o, b); o.u8((uint8_t)f);
  }
  bool mapFrom(BinR& r, int& key) const {
    uint8_t kind = r.u8();
    if (kind == 2) {
      int b = bldgFrom(r);
      int f = r.u8();
      if (b < 0) return false;
      key = f > 0 ? 10000000 + b * 16 + f : 100000 + b;
      return true;
    }
    ew::Gid id = r.u64(); r.u64(); r.u8();
    if (kind == 0) { key = 0; return true; }
    if (kind == 3) { key = -1; return true; }
    int s = siteFrom(id);
    if (s < 0) return false;
    key = 1 + s;
    return true;
  }
};
}  // namespace

void Game::serialize(std::vector<uint8_t>& out) const {
  out.clear();
  BinW w(out);
  Refs R{const_cast<World&>(world)};
  w.u32(SAVE_MAGIC); w.u32(SAVE_VER);
  w.u8(1);
  w.u32((uint32_t)ew::ENDLESS_GEN_VER);
  w.u64(seed); w.f32(time); w.f32(hour); w.i32(day);
  w.i32(world.ox); w.i32(world.oy);
  const Actor& p = pl();
  w.u8(inside ? 1 : 0);
  w.u64(R.site(inside ? subSite : -1));
  R.bldg(w, inside ? subBldg : -1);
  w.i32(inside && subBldg >= 0 ? subFloor : 0);
  w.f32(p.p.x); w.f32(p.p.y);
  w.f32(p.hp); w.f32(mp); w.f32(stamina);
  w.i32(plLevel); w.i32(plXp); w.i32(gold); w.i32(perkPts);
  w.f32(baseHp); w.f32(maxMp); w.f32(maxSt);
  w.u32((uint32_t)inv.size());
  for (auto& it : inv) writeItem(w, it);
  for (int e : {eqWeapon, eqBow, eqStaff, eqArmor, eqHelmet, eqShield, eqRing, eqAmulet, eqGloves, eqBoots, eqCloak}) w.i32(e);
  // (M9) the classic spell fields as a load will see them (magic::syncLegacy on copies: the book written below agrees)
  magic::Book bookOut = arcana.book;
  uint8_t knownOut = spellsKnown;
  int spellOut = (int)spell;
  int8_t mirrorOut = arcana.live.legacyMirror;
  magic::syncLegacy(bookOut, knownOut, spellOut, mirrorOut);
  w.u8(knownOut); w.u8((uint8_t)spellOut);
  w.i32(kills); w.i32(dungeonsCleared); w.f32(blessT); w.str(blessName);
  w.u64(R.site(lastTown));
  w.u32((uint32_t)quests.size());
  for (auto& q : quests) {
    w.i32(q.id); w.u8((uint8_t)q.type); w.u8((uint8_t)q.state); w.str(q.title); w.str(q.desc); w.str(q.giverName);
    w.u64(R.site(q.giverSite)); R.bldg(w, q.giverBldg); w.i32(q.giverSlot); w.u64(R.site(q.target)); w.u8((uint8_t)q.mon);
    w.i32(q.need); w.i32(q.have); w.i32(q.gold); w.i32(q.xp); w.i32(q.stage);
    w.u64(q.targetId); w.u8(q.hasPos ? 1 : 0); w.i32(q.tgx); w.i32(q.tgy); w.u64(q.giverId); w.str(q.subject);
    R.bldg(w, q.destBldg); w.i32(q.deadlineDay); w.u32(q.flags);
  }
  w.i32(nextQuestId); w.i32(trackedQuest);
  w.u32((uint32_t)npcQuestsDone.size());
  for (auto& kv : npcQuestsDone) { w.u64(kv.first); w.i32(kv.second); }
  // (records keyed by ids are written in the order of their encoded bytes: an endless session's handles depend on the
  // order the player met things, so handle order would make two saves of the same state differ)
  auto writeSorted = [&](std::vector<std::vector<uint8_t>>& recs) {
    std::sort(recs.begin(), recs.end());
    w.u32((uint32_t)recs.size());
    for (const auto& rec : recs) for (uint8_t c : rec) w.u8(c);
  };
  {
    std::vector<std::vector<uint8_t>> recs;
    for (uint64_t k : looted) {
      recs.emplace_back();
      BinW b(recs.back());
      if ((k >> 60) == 0xE) { b.u8(0xE); b.u64(k); continue; }
      b.u8(0); R.map(b, (int)(k >> 32)); b.u32((uint32_t)k);
    }
    writeSorted(recs);
  }
  {
    std::vector<std::vector<uint8_t>> recs;
    for (auto& kv : killedSlots) {
      if (kv.second.empty()) continue;
      std::vector<std::vector<uint8_t>> vals;
      for (int v : kv.second) {
        vals.emplace_back();
        BinW b(vals.back());
        if (kv.first == 0) { b.u64(R.site(v / 4096)); b.i32(v % 4096); }   // overworld spawns: slot + site * 4096
        else if (kv.first == -1) {                                        // dens: den * 4096 + the day it was cleared
          int d = v / 4096;
          b.u64(d >= 0 && d < (int)world.dens.size() ? world.dens[(size_t)d].id : 0); b.i32(v % 4096);
        } else { b.u64(0); b.i32(v); }
      }
      std::sort(vals.begin(), vals.end());
      recs.emplace_back();
      BinW b(recs.back());
      R.map(b, kv.first);
      b.u32((uint32_t)vals.size());
      for (const auto& v : vals) for (uint8_t c : v) b.u8(c);
    }
    writeSorted(recs);
  }
  {
    std::vector<std::pair<uint64_t, uint8_t>> fl;
    for (const Site& s : world.sites)
      if (s.discovered || s.cleared || s.rumoured) fl.push_back({s.id, (uint8_t)((s.discovered ? 1 : 0) | (s.cleared ? 2 : 0) | (s.rumoured ? 4 : 0))});
    std::sort(fl.begin(), fl.end());
    w.u32((uint32_t)fl.size());
    for (auto& f : fl) { w.u64(f.first); w.u8(f.second); }
  }
  w.u8((uint8_t)background); w.u32(storyFlags);
  writeAppearance(w, app);
  R.bldg(w, lodging.bldg); w.i32(lodging.floor); w.i32(lodging.room); w.i32(lodging.untilDay);
  // explored, in key order so the bytes do not depend on hash-map order
  std::vector<uint64_t> keys;
  for (auto& kv : explored.regions) keys.push_back(kv.first);
  std::sort(keys.begin(), keys.end());
  w.u32((uint32_t)keys.size());
  for (uint64_t k : keys) {
    w.i32(ExploredMask::keyRx(k)); w.i32(ExploredMask::keyRy(k));
    for (uint8_t b : explored.regions.at(k)) w.u8(b);
  }
  w.u32((uint32_t)marks.size());   // (std::map: already in key order)
  for (auto& kv : marks) { w.u64(kv.first); w.i32(kv.second); }
  // M4 (v10): the realm and the stories, each a length-prefixed block with its own version byte
  {
    std::vector<uint8_t> blk;
    realm.serialize(blk);
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    story.serialize(blk);
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    life.serialize(blk);   // M5 (v11)
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    craft.serialize(blk);  // M6 (v12)
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    home.serialize(blk);   // M7 (v14)
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    talkMem.serialize(blk);   // M7b (v15)
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    party.serialize(blk);     // M8 (v16)
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    skills.serialize(blk);    // M9 (v20)
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    bookOut.serialize(blk);   // M9 (v20): the book (synced with the classic fields above)
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    sea::Sea seaOut = sea;    // M10 (v21): the boat aboard takes the hero's tile and heading (on a copy)
    SeaOps::saving(*this, seaOut);
    seaOut.serialize(blk);
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    service.serialize(blk);   // M12 (v22): the army served, rank, orders, missions
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
    lordship.serialize(blk);  // M12 (v22): the holdings, the hall's coffers, tribute, the player's realm
    w.u32((uint32_t)blk.size());
    for (uint8_t c : blk) w.u8(c);
  }
}

bool Game::deserialize(const std::vector<uint8_t>& in) {
  BinR r(in);
  if (r.u32() != SAVE_MAGIC) return false;
  const uint32_t ver = r.u32();
  if (ver < SAVE_VER_MIN || ver > SAVE_VER || r.bad) return false;   // older (or newer) saves are not loaded: the title says so
  const bool endless = r.u8() == 1;
  const int genVer = (int)r.u32();
  const uint64_t sd = r.u64();
  const float t = r.f32(), hr = r.f32();
  const int dy = r.i32();
  if (r.bad || !endless) return false;                 // (M2: the classic island is retired)
  if (genVer != ew::ENDLESS_GEN_VER) return false;     // made by another endless generator
  {
    int32_t wox = r.i32(), woy = r.i32();
    if (r.bad || std::abs(wox) > ew::WORLD_EDGE + 4096 || std::abs(woy) > ew::WORLD_EDGE + 4096) return false;
    // the window is built once, where the save was made (not at the start first): a load behind the fade
    seed = sd;
    rng_ = Rng(sd ^ 0xABCDEF);
    world.generateEndlessAt(sd, wox, woy);
    resetSession();
  }
  Refs R{world};
  time = t; hour = hr; day = dy;
  bool ins = r.u8() != 0;
  int ss = R.siteFrom(r.u64());
  const size_t sbAt = r.p;   // (M7) where the building ref lies: the player's own house is found after the home block
  int sb = R.bldgFrom(r);
  int floorIn = r.i32();
  Vec2 pp; pp.x = r.f32(); pp.y = r.f32();
  float hp = r.f32(); mp = r.f32(); stamina = r.f32();
  plLevel = r.i32(); plXp = r.i32(); gold = r.i32(); perkPts = r.i32();
  baseHp = r.f32(); maxMp = r.f32(); maxSt = r.f32();
  uint32_t n = r.u32();
  if (n > 5000) return false;
  inv.clear();
  for (uint32_t i = 0; i < n && !r.bad; i++) inv.push_back(readItem(r));
  int* eqs[] = {&eqWeapon, &eqBow, &eqStaff, &eqArmor, &eqHelmet, &eqShield, &eqRing, &eqAmulet, &eqGloves, &eqBoots, &eqCloak};
  for (int* e : eqs) { *e = r.i32(); if (*e >= (int)inv.size()) *e = -1; }
  spellsKnown = r.u8(); spell = (Spell)(r.u8() % (int)Spell::COUNT);
  kills = r.i32(); dungeonsCleared = r.i32(); blessT = r.f32(); blessName = r.str();
  lastTown = R.siteFrom(r.u64());
  n = r.u32();
  if (n > 5000) return false;
  quests.clear();
  for (uint32_t i = 0; i < n && !r.bad; i++) {
    Quest q;
    q.id = r.i32(); q.type = (QType)r.u8(); q.state = (QState)r.u8(); q.title = r.str(); q.desc = r.str(); q.giverName = r.str();
    q.giverSite = R.siteFrom(r.u64()); q.giverBldg = R.bldgFrom(r); q.giverSlot = r.i32(); q.target = R.siteFrom(r.u64());
    q.mon = (Monster)r.u8();
    q.need = r.i32(); q.have = r.i32(); q.gold = r.i32(); q.xp = r.i32(); q.stage = r.i32();
    q.targetId = r.u64(); q.hasPos = r.u8() != 0; q.tgx = r.i32(); q.tgy = r.i32(); q.giverId = r.u64(); q.subject = r.str();
    q.destBldg = R.bldgFrom(r); q.deadlineDay = r.i32(); q.flags = r.u32();
    // (a damaged save: out-of-range kinds load as harmless defaults, never as indices past a table)
    if ((int)q.type >= (int)QType::COUNT) q.type = QType::Hunt;
    if ((int)q.state > (int)QState::Done) q.state = QState::Done;
    if ((int)q.mon >= (int)Monster::COUNT) q.mon = Monster::Wolf;
    if (q.type == QType::Main) q.stage = std::clamp(q.stage, 0, 4);
    quests.push_back(q);
  }
  nextQuestId = r.i32(); trackedQuest = r.i32();
  n = r.u32();
  npcQuestsDone.clear();
  for (uint32_t i = 0; i < n && !r.bad; i++) { uint64_t k = r.u64(); npcQuestsDone[k] = r.i32(); }
  n = r.u32();
  looted.clear();
  for (uint32_t i = 0; i < n && !r.bad; i++) {
    if (r.u8() == 0xE) { looted.insert(r.u64()); continue; }
    int key = 0;
    bool ok = R.mapFrom(r, key);
    uint32_t tile = r.u32();
    if (ok) looted.insert(((uint64_t)(uint32_t)key << 32) | tile);
  }
  n = r.u32();
  killedSlots.clear();
  for (uint32_t i = 0; i < n && !r.bad; i++) {
    int key = 0;
    bool ok = R.mapFrom(r, key);
    uint32_t m = r.u32();
    if (r.bad || m > (in.size() - r.p) / 12) return false;   // (M4) more records than bytes left: a damaged save
    for (uint32_t j = 0; j < m && !r.bad; j++) {
      ew::Gid id = r.u64();
      int v = r.i32();
      if (!ok) continue;
      if (key == 0) { int s = R.siteFrom(id); if (s >= 0 && v >= 0 && v < 4096) killedSlots[0].insert(v + s * 4096); }
      else if (key == -1) { int d = world.ensureDen(id); if (d >= 0) killedSlots[-1].insert(d * 4096 + v); }
      else killedSlots[key].insert(v);
    }
  }
  n = r.u32();
  for (uint32_t i = 0; i < n && !r.bad; i++) {
    ew::Gid id = r.u64();
    uint8_t f = r.u8();
    int s = R.siteFrom(id);
    if (s >= 0) {
      world.sites[(size_t)s].discovered = f & 1; world.sites[(size_t)s].cleared = (f & 2) != 0; world.sites[(size_t)s].rumoured = (f & 4) != 0;
    }
  }
  uint8_t bg = r.u8();
  background = bg < (uint8_t)Background::COUNT ? (Background)bg : Background::None;
  storyFlags = r.u32();
  if (!readAppearance(r, app)) return false;
  lodging = Lodging();
  lodging.bldg = R.bldgFrom(r); lodging.floor = r.i32(); lodging.room = r.i32(); lodging.untilDay = r.i32();
  if (lodging.bldg < 0) lodging = Lodging();
  n = r.u32();
  if (n > 1000000) return false;
  explored.clear();
  for (uint32_t i = 0; i < n && !r.bad; i++) {
    int32_t rx = r.i32(), ry = r.i32();
    std::vector<uint8_t> m((size_t)ExploredMask::CELLS * ExploredMask::CELLS / 8);
    for (uint8_t& b : m) b = r.u8();
    explored.regions[ExploredMask::key(rx, ry)] = std::move(m);
  }
  n = r.u32();
  if (n > 1000000) return false;
  marks.clear();
  for (uint32_t i = 0; i < n && !r.bad; i++) { uint64_t k = r.u64(); marks[k] = r.i32(); }
  // M4 (v10): the realm and story blocks, M5 (v11) the life block, M6 (v12) the craft block, M7 (v14) the home block,
  // M7b (v15) the talk block, M8 (v16) the party block (a block its owner cannot read refuses the whole save)
  // M9 (v20) the skills and magic blocks (a SAVE_VER 19 save has neither: defaults from its level below)
  // M10 (v21) the sea block, M12 (v22) the service and lordship blocks
  const int nBlocks = 12;
  skills = skills::Skills();
  arcana = magic::Arcana();
  sea = sea::Sea();
  service = service::Service();
  lordship = lord::Lordship();
  for (int blkI = 0; blkI < nBlocks && !r.bad; blkI++) {
    n = r.u32();
    if (r.bad || n > (64u << 20) || r.p + n > in.size()) return false;
    std::vector<uint8_t> blk(in.begin() + (std::ptrdiff_t)r.p, in.begin() + (std::ptrdiff_t)(r.p + n));
    r.p += n;
    const bool ok = blkI == 0 ? realm.deserialize(blk) : blkI == 1 ? story.deserialize(blk) : blkI == 2 ? life.deserialize(blk)
                    : blkI == 3 ? craft.deserialize(blk) : blkI == 4 ? home.deserialize(blk) : blkI == 5 ? talkMem.deserialize(blk)
                    : blkI == 6 ? party.deserialize(blk) : blkI == 7 ? skills.deserialize(blk) : blkI == 8 ? arcana.book.deserialize(blk)
                    : blkI == 9 ? sea.deserialize(blk) : blkI == 10 ? service.deserialize(blk) : lordship.deserialize(blk);
    if (!ok) return false;
  }
  arcana.live.legacyMirror = (int8_t)spell;   // (the save wrote them synced: nothing to reselect)
  MagicOps::loaded(*this);   // (M9) the traditions of the lands the book has touched made again (their spells known to spellById)
  MagicOps::syncLegacy(*this);
  if (r.bad) return false;
  homeSerial_ = world.placeSerial + 0x80000000u;   // M7: the player's plots are stamped on the first step
  if (ins && sb < 0 && !home.plots.empty()) {      // M7: saved inside the player's own house (a Bldg the home stamps)
    BinR rb(in);
    rb.p = sbAt;
    const ew::Gid hid = rb.u64();
    home::stampWindow(*this);
    sb = world.bldgHandle(hid);
  }
  // re-apply looted overworld chests (by global tile)
  world.over.rebuildSolid();
  reapplyLooted();
  recalcPlayer();
  // (fixer M4 r1) the realm's owners on the loaded settlements before anyone is spawned: a save made inside a conquered
  // town's building (its palace) loads with the conqueror's guards and ruler, not the generator's (realmStep waits
  // outside). Only the states the realm already holds are applied (noting new sites here would change the realm the
  // save holds: realmStep notes them on the first step outside, as before)
  realmSync();
  MagicOps::trialsLoaded(*this);   // (fixer M10 r1) a save inside a Trial loads back as the trial (genRuin reads the registry)
  if (ins && sb >= 0 && sb < (int)world.over.bldgs.size()) {
    enterBuilding(sb);
    if (floorIn > 0 && floorIn < world.over.bldgs[(size_t)sb].floors()) changeFloor(floorIn);
  } else if (ins && ss >= 0 && ss < (int)world.sites.size()) enterSite(ss);
  else clearNonPlayer();
  pl().p = pp;
  // a position off the map (a damaged save) would leave the player stuck in the void: stand them on a free tile
  if (!(std::isfinite(pp.x) && std::isfinite(pp.y)) || !map().in((int)std::floor(pp.x / TILE), (int)std::floor(pp.y / TILE))) {
    if (inside) placePlayerAt(sub.exitX, sub.exitY - 1);
    else placePlayerAt(world.over.w / 2, world.over.h / 2);
  }
  if (!std::isfinite(hp)) hp = 1;
  pl().hp = hp;
  SeaOps::loaded(*this);    // (M10) the boat aboard back under the hero (sea_game.cpp)
  LordOps::loaded(*this);   // (M12) the renown mirror of the lordship (lord_game.cpp)
  ServiceOps::loaded(*this);   // (M12) the renown mirror of the service (service_game.cpp)
  if (hp <= 0) respawn();   // saved on the death screen: wake in town rather than standing up with 1 HP where you fell
  events.clear();
  updateLocation();
  return true;
}
