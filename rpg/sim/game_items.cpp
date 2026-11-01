// EMBERVALE RPG: the inventory, monster loot and chests (cut from game_rpg.cpp, SOLID 1.1).
// Public: Game::giveGold, Game::addItem, Game::equipSlot, Game::useItem, Game::dropItem, Game::dropLoot,
//   Game::openChest, LootOps::chestSource
#include "rpg/sim/game_rpg_detail.h"
#include "rpg/sim/life_hooks.h"
#include "rpg/sim/game_internal.h"

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
      if (it.sub >= home::FOOD_MEAL) HomeOps::ate(*this, it); else LifeHooks::ate(*this, it);
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

