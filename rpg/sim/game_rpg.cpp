// EMBERVALE RPG systems: inventory, loot, quests, dialogue, shops, resting, travel, saving.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
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

void Game::addItem(const Item& it, bool announce) {
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
  // auto-equip into an empty slot
  int* slot = equipSlot(it.kind);
  if (slot && *slot < 0) { *slot = idx; recalcPlayer(); }
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
  if (int* slot = equipSlot(it.kind)) { toggle(*slot); return; }
  switch (it.kind) {
    case ItemKind::Potion: {
      if (it.sub == (uint8_t)PotionType::Health) { pl().hp = std::min(pl().maxHp, pl().hp + it.power); emit(Ev::Heal, pl().p); }
      else if (it.sub == (uint8_t)PotionType::Magicka) mp += it.power;
      else stamina += it.power;
      sfx((int)Sfx::Heal, pl().p, 1.2f);
      break;
    }
    case ItemKind::Food: {
      float k = background == Background::Farmhand ? 1.5f : 1.0f;   // farmhand: plain food goes further
      pl().hp = std::min(pl().maxHp, pl().hp + it.power * k);
      stamina += it.power * k;
      sfx((int)Sfx::Pickup, pl().p, 0.7f);
      emit(Ev::Heal, pl().p);
      break;
    }
    case ItemKind::Misc:
      if (it.name.rfind("SPELL TOME", 0) == 0) {
        spellsKnown |= (uint8_t)(1 << it.sub);
        say(std::string("LEARNED ") + spellName((Spell)it.sub));
        sfx((int)Sfx::LevelUp, pl().p, 1.3f);
        break;
      }
      return;
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
  auto drop = [&](const Item& it) {
    Pickup k; k.p = a.p + Vec2(r.range(-8, 8), r.range(-6, 6)); k.item = it; pickups.push_back(k);
  };
  auto dropGold = [&](int g) { Pickup k; k.p = a.p + Vec2(r.range(-6, 6), r.range(-4, 4)); k.gold = g; pickups.push_back(k); };
  if (a.human) {
    dropGold(5 + r.irange(10 + a.level * 3) + (a.boss ? 60 + a.level * 10 : 0));
    if (r.f() < 0.35f || a.boss) drop(randomLoot(r, a.level, a.boss));
    if (a.ranged && r.f() < 0.7f) drop(makeArrows(3 + r.irange(5)));
    return;
  }
  switch (a.mon) {
    case Monster::Wolf: case Monster::IceWolf: if (r.f() < (background == Background::Hunter ? 1.0f : 0.6f)) drop(makeMisc(0)); break;
    case Monster::Boar: if (r.f() < 0.6f) drop(makeFood(1)); break;
    case Monster::Bear: drop(makeMisc(0)); if (background == Background::Hunter) drop(makeMisc(0)); if (r.f() < 0.5f) drop(makeFood(1)); break;
    case Monster::Spider: case Monster::FrostSpider: if (r.f() < 0.5f) drop(makeMisc(5)); break;
    case Monster::Skeleton: case Monster::Draugr: if (r.f() < 0.5f) drop(makeMisc(1)); if (r.f() < 0.3f) dropGold(3 + r.irange(12)); if (r.f() < 0.15f) drop(randomLoot(r, a.level, false)); break;
    case Monster::Troll: drop(makeMisc(4)); break;
    case Monster::Goblin: dropGold(2 + r.irange(8)); if (r.f() < 0.2f) drop(randomLoot(r, a.level, false)); break;
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
  if (a.boss && a.mon != Monster::Dragon) { dropGold(40 + a.level * 12); drop(randomLoot(r, a.level, true)); }
  if (a.mon == Monster::Dragon) {
    dropGold(1500);
    Item crown = makeJewel(r, 30); crown.name = "EMBER CROWN"; crown.icon = art::Icon::Crown; crown.rarity = Rarity::Legendary;
    crown.kind = ItemKind::Amulet; crown.ench = Ench::Fortify; crown.enchPow = 80; crown.value = 5000;
    drop(crown);
    Item w = makeWeapon(r, 30, (int)WeaponType::Greatsword); w.rarity = Rarity::Legendary; w.name = "DRAGONBONE GREATSWORD"; w.ench = Ench::Fire; w.enchPow = 30; w.power = 60;
    drop(w);
  }
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
  Vec2 c(tx * TILE + 8.0f, ty * TILE + 18.0f);
  for (int k = 0; k < n; k++) { Pickup p; p.p = c + Vec2(r.range(-8, 8), r.range(0, 8)); p.item = randomLoot(r, lvl, rich && k == 0 && r.f() < 0.35f); pickups.push_back(p); }
  Pickup g; g.p = c; g.gold = 8 + r.irange(15 + lvl * 4) + (rich ? 20 : 0); pickups.push_back(g);
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
  int best = -1; float bd = 24 * 24;
  for (size_t i = 1; i < actors.size(); i++) {
    const Actor& a = actors[i];
    if (!a.npc || a.st == AState::Dead) continue;
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
                      ((prop == Prop::StandingStone || prop == Prop::GraveCairn) && !inside) ||   // M2 wayside (wayside.cpp)
                      (prop == Prop::Signpost && !inside && world.siteAt(tx, ty, 3) >= 0) ||
                      ((prop == Prop::Hammock || prop == Prop::SleepingMat) && inside && subBldg >= 0) ||   // M3: cultures that sleep so
                      (art::isLoreProp(prop) && (inside ? subSite >= 0 : (prop == Prop::NoticeBoard || prop == Prop::ToppledStatue))) ||   // M4 (story_game.cpp)
                      (prop == Prop::Bed && inside && subBldg >= 0 &&
                       (world.over.bldgs[subBldg].type != art::Building::Inn || world.over.bldgs[subBldg].genVer >= WORLDGEN_V7));
        if (!usable) continue;
        otx = tx; oty = ty;
        return pr;
      }
  return 0;
}

void Game::interact() {
  int t = interactTarget();
  if (t >= 0) { talkTo(actors[findActor(t)]); return; }
  // chest / shrine / bed / bush in front of the player
  const Actor& p = pl();
  int tx = 0, ty = 0;
  int pr = interactProp(tx, ty);
  if (!pr) return;
  Prop prop = (Prop)(pr - 1);
  if (prop == Prop::Chest) { openChest(tx, ty); return; }
  // M4: a story may ask for a wayside prop (standing stones, a cairn) before its own use runs
  if (prop == Prop::StandingStone || prop == Prop::GraveCairn) story.onUseProp(*this, (int)prop, tx, ty);
  if (useWaysideProp(prop, tx, ty)) return;
  if (storyUseProp(prop, tx, ty)) return;   // M4: notice boards, inscriptions, graves, journals (story_game.cpp)
  if (prop == Prop::Shrine || prop == Prop::Altar) {
    static const char* bless[] = {"BLESSING OF SOLMIR", "BLESSING OF VEYNA", "BLESSING OF HALDRUN", "BLESSING OF ORISSA"};
    blessName = bless[hash2(tx, ty) % 4];
    int si = inside ? -1 : world.siteAt(tx, ty, 1);   // a wayside shrine blesses in its own god's name
    if (si >= 0 && world.sites[si].type == SiteType::Shrine && world.sites[si].name.rfind("SHRINE OF ", 0) == 0)
      blessName = "BLESSING OF " + world.sites[si].name.substr(10);
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

std::string Game::greeting(const Actor& a) {
  Rng r(hash32((uint32_t)npcKey(a)) ^ (uint32_t)(day * 31 + (int)hour));
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
      local.push_back("MY COUSIN WENT INTO " + s.name + " LOOKING FOR TREASURE. ALL THEY FOUND WAS " + monsterPlural(s.theme) + ".");
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
                                  "THE ROYAL GUARD SEES EVERYTHING IN THIS HALL. REMEMBER THAT.",
                                  "LONG LIVE THE " + ruler + ". NOW MOVE ALONG."};
        return realm.empty() ? rg[r.irange(3)] : rg[r.irange(3)] + " THIS IS THE COURT OF " + realm + ".";
      }
      if (capitalHere && r.f() < 0.4f) return "LONG LIVE THE " + ruler + " OF " + realm + ". " + court + " IS OFF LIMITS WITHOUT GOOD CAUSE, TRAVELLER.";
      static const char* g[] = {"I WAS A SELLSWORD ONCE. NOW I WATCH A GATE AND COUNT CARTS.",
                                "KEEP YOUR NOSE CLEAN, TRAVELLER.", "LOST SOMETHING? TRY THE INN. EVERYTHING ENDS UP AT THE INN.",
                                "WATCH THE ROADS AT NIGHT. THE DEAD DON'T STAY BURIED HERE.", "EYES OPEN, BLADE SHARP. THAT'S THE WATCH'S WAY."};
      if (isNight() && r.f() < 0.4f) return "QUIET NIGHT. TOO QUIET. KEEP YOUR BLADE CLOSE.";
      return g[r.irange(5)];
    }
    case Role::Innkeeper: {
      if (capitalHere) {   // a capital is unmistakable: its innkeepers always talk of the king
        const std::string c[] = {"WELCOME TO " + town + ", SEAT OF THE " + ruler + " OF " + realm + ". BEDS ARE 10 GOLD, AND THE COURT GOSSIP IS FREE.",
                                 "THE " + ruler + "'S OWN GUARD DRINKS HERE ON FEAST DAYS. SIT, TRAVELLER: THIS IS THE FINEST INN IN " + realm + ".",
                                 "YOU'VE COME TO THE CAPITAL! THE " + ruler + " OF " + realm + " RIDES OUT FROM " + court + " NOW AND THEN. A ROOM IS 10 GOLD."};
        return c[hash32((uint32_t)npcKey(a) ^ (uint32_t)day) % 3];
      }
      const std::string v[] = {"WELCOME TO THE INN OF " + town + ". A WARM BED IS 10 GOLD, AND THE MEAD IS COLD.",
                               "COME IN OUT OF THE " + std::string(isNight() ? "DARK" : "ROAD DUST") + ". STEW'S ON, AND THE ROOMS ARE CLEAN ENOUGH.",
                               "ANOTHER TRAVELLER! " + town + " DOESN'T SEE MANY. SIT, DRINK, TELL ME WHERE YOU'VE BEEN.",
                               "IF YOU'RE LOOKING FOR WORK, ASK. IF YOU'RE LOOKING FOR TROUBLE, TAKE IT OUTSIDE.",
                               "BEDS UPSTAIRS, ALE DOWN HERE, AND RUMOURS ARE FREE WITH EITHER."};
      return withLocal(v[hash32((uint32_t)npcKey(a) ^ (uint32_t)day) % 5]);
    }
    case Role::Merchant: {
      const std::string v[] = {"COME, LOOK AT MY WARES. FINEST GOODS IN " + town + ".", "POTIONS, ARROWS, A BLADE OR TWO. GOLD TALKS, FRIEND.",
                               "BUYING OR SELLING? I DO BOTH, AND I DO THEM FAIRLY. MOSTLY.", "CARAVAN CAME IN LAST WEEK. GOOD STOCK, WHILE IT LASTS."};
      return v[hash32((uint32_t)npcKey(a) ^ (uint32_t)day) % 4];
    }
    case Role::Smith: {
      const std::string v[] = {"STEEL AND FIRE, FRIEND. IF IT CUTS OR STOPS A BLADE, I CAN SELL IT TO YOU.",
                               "MIND THE SPARKS. NEED SOMETHING SHARP, OR SOMETHING THICK?", "BEST STEEL THIS SIDE OF " + world.sites[world.capital].name + ". ASK ANYONE.",
                               "BRING ME ORE AND I'LL... WELL, I'LL BUY IT. THEN SELL YOU A SWORD."};
      return v[hash32((uint32_t)npcKey(a) ^ (uint32_t)day) % 4];
    }
    case Role::Priest: return "THE OLD GODS WATCH OVER YOU. SHALL I MEND YOUR WOUNDS?";
    case Role::Mage: return "MAGIC IS NOT A TOY. BUT IF YOU HAVE GOLD, I HAVE KNOWLEDGE.";
    case Role::Jarl: return "SPEAK, STRANGER. THE " + (homeSite >= 0 ? lordTitleAt(world, homeSite) : std::string("JARL")) + " OF " + town + " IS LISTENING.";
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
      return withLocal(v[hash32((uint32_t)npcKey(a) ^ (uint32_t)day) % 4]);
    }
    case Role::Fisher: {
      const std::string v[] = {"THE FISH ARE BITING TODAY. THEY USUALLY ARE, IF YOU'RE PATIENT.",
                               "TROUT AT DAWN, EEL AT DUSK. SMOKED, IF YOU'RE TRAVELLING FAR.",
                               "SIT A WHILE. THE WATER TELLS YOU THINGS IF YOU LISTEN."};
      return v[hash32((uint32_t)npcKey(a) ^ (uint32_t)day) % 3];
    }
    case Role::Herbalist: {
      const std::string v[] = {"MIND THE NIGHTSHADE. THE BLUE FLOWERS YOU MAY TOUCH.",
                               "EVERY HERB IN THIS GARDEN HEALS SOMETHING, OR KILLS SOMETHING. OFTEN BOTH.",
                               "A TEA FOR THE COLD, A SALVE FOR THE CUT, A DRAUGHT FOR THE ROAD. WHAT AILS YOU?"};
      return v[hash32((uint32_t)npcKey(a) ^ (uint32_t)day) % 3];
    }
    case Role::Traveller: {
      const std::string v[] = {"WARM YOURSELF BY THE FIRE, FRIEND. THE ROAD IS LONG IN BOTH DIRECTIONS.",
                               "I'VE WALKED FROM ONE END OF " + (realm.empty() ? std::string("THIS LAND") : realm) + " TO THE OTHER. ASK ME WHAT I'VE SEEN.",
                               "THE ROADS ARE QUIET TONIGHT. THAT'S WHEN I WORRY.",
                               "SIT. SHARE THE FIRE. NEWS IS THE ONLY COIN I CARRY."};
      return v[hash32((uint32_t)npcKey(a) ^ (uint32_t)day) % 4];
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
  for (auto& q : quests) if (q.state != QState::Done && q.type != QType::Main && q.giverSite == a.site && q.giverBldg == a.bldg && q.giverSlot == a.slot) return false;
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
  emit(Ev::QuestUpdate, pl().p, q.id, 1, "QUEST COMPLETE: " + q.title);
  sfx((int)Sfx::QuestDone, pl().p);
  // occasional item reward
  Rng r(hash32((uint32_t)q.id * 977u) ^ (uint32_t)seed);
  if (r.f() < 0.5f) addItem(randomLoot(r, plLevel + 1, true));
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
  if (background == Background::Blacksmith && seller == Role::Smith) return 0.8f;   // a smith's child pays a fair price
  if (background == Background::Urchin && seller == Role::Merchant) return 0.85f;   // the streets know their own
  return 1.0f;
}

// The opening: the start village innkeeper hands over an old blade (and a draught for the road) for a small favour.
void Game::giveFirstWeapon(Quest& q) {
  Rng r(hash32((uint32_t)seed ^ 0xB1ADEu));
  Item w = makeWeapon(r, 1, (int)WeaponType::Sword, false);
  w.name = "OLD BLADE"; w.power = 7; w.value = 12; w.rarity = Rarity::Common; w.ench = Ench::None; w.enchPow = 0;
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
void Game::talkTo(Actor& a) {
  dlg = Dialogue();
  dlg.actor = a.id;
  dlg.speaker = a.name;
  dlg.role = a.role;
  // M2: a toll bridge's troll asks its toll; a missing person found in a cave asks to be led out
  if (a.npc && !a.human && a.mon == Monster::Troll) { tollTalk(a); return; }
  if (a.quest > 0) {
    const Quest* q = questById(a.quest);
    if (q && q->state == QState::Active && !(q->flags & QF_FOUND)) {
      dlg.text = "YOU CAME FOR ME? " + q->giverName + " SENT YOU? THANK THE GODS. I CAN'T FIND THE WAY OUT, AND THERE ARE THINGS IN THE DARK.";
      dlg.opts.push_back({"STAY CLOSE. I'LL GET YOU OUT.", A_ESCORT, q->id});
    } else if (q && q->state == QState::Active) dlg.text = "I'M RIGHT BEHIND YOU. JUST GET US OUT OF HERE.";
    else dlg.text = "I'LL FIND MY OWN WAY HOME FROM HERE. TELL " + (q ? q->giverName : std::string("THEM")) + " I'M SAFE!";
    dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
    mode = Mode::Dialogue;
    sfx((int)Sfx::Talk, a.p);
    return;
  }
  dlg.text = greeting(a);
  // the background shows in how people talk to you (and what they charge)
  {
    bool say1 = hash32((uint32_t)npcKey(a) ^ (uint32_t)(day * 977)) % 2 == 0;
    if (background == Background::Blacksmith && a.role == Role::Smith) dlg.text = "A SMITH'S CHILD? I CAN SEE IT IN YOUR HANDS. FOR YOU, A FAIR PRICE: A FIFTH OFF ANYTHING ON MY RACK.";
    if (background == Background::Urchin && a.role == Role::Merchant) dlg.text = "YOU'VE GOT THE STREETS IN YOUR EYES, FRIEND. ALRIGHT, ONE OF OUR OWN: I'LL KNOCK A LITTLE OFF.";
    if (background == Background::Noble && a.role == Role::Guard && say1) dlg.text = "FORGIVE ME, I KNOW NOBLE BEARING WHEN I SEE IT. THE WATCH COULD USE SOMEONE OF YOUR... STANDING.";
    if (background == Background::Novice && a.role == Role::Priest) dlg.text = "A NOVICE OF THE TEMPLE! THE GODS' BLESSINGS RUN LONGER IN YOU, CHILD. SHALL I MEND YOUR WOUNDS?";
    if (background == Background::Sailor && a.role == Role::Innkeeper && say1) dlg.text = "SEA LEGS ON DRY LAND, EH? SAILORS DRINK HERE, AND NOBODY ASKS WHAT SANK.";
    if (background == Background::Farmhand && a.role == Role::Innkeeper && !say1) dlg.text = "A FARMHAND, BY THOSE HANDS. THEN YOU KNOW WHAT GOOD BREAD IS WORTH ON THE ROAD.";
    if (background == Background::Hunter && a.role == Role::Villager && say1) dlg.text = "YOU WALK LIKE A HUNTER. THE WOLVES WON'T HEAR YOU COMING, WILL THEY?";
  }
  // a reward waiting comes first: the first option collects it
  for (auto& q : quests)
    if (q.state == QState::Complete && q.type != QType::Main && isGiver(q, a)) {
      bool bounty = q.type == QType::Bounty || q.type == QType::Hunt || q.type == QType::NamedBandit;
      dlg.opts.push_back({std::string(bounty ? "COLLECT BOUNTY (+" : "COLLECT REWARD (+") + std::to_string(q.gold) + " GOLD)", A_TURNIN, q.id});
      dlg.text = turnInLine(q);
    }
  // main quest hooks
  for (auto& q : quests) {
    if (q.type != QType::Main || a.role != Role::Jarl || a.site != world.capital) continue;
    if (q.stage == 0) { dlg.text = "SO YOU'VE HEARD THE RUMOURS. THEY ARE TRUE: A DRAGON, ASHFANG, HAS WOKEN BENEATH " + lairName() + ". STEEL ALONE CANNOT KILL IT."; dlg.opts.push_back({"HOW CAN IT BE STOPPED?", A_MAIN, 1}); }
    if (q.stage == 2) { dlg.text = "THE THREE EMBER SHARDS! I NEVER THOUGHT I WOULD SEE THEM WHOLE AGAIN. LET THE SMITHS FORGE THE CROWN."; dlg.opts.push_back({"I WILL HUNT THE DRAGON.", A_MAIN, 3}); }
  }
  for (auto& q : quests)
    if (q.state == QState::Active && q.type != QType::Main && q.type != QType::Retrieve && isGiver(q, a))
      dlg.text = "HAVE YOU DEALT WITH IT YET? " + q.desc;
  // the opening: the start village innkeeper hands over the old blade (VISION_PLAN 15.1)
  for (auto& q : quests)
    if (q.type == QType::Retrieve && q.state == QState::Active && isGiver(q, a)) {
      giveFirstWeapon(q);
      dlg.text = "THE ROAD TOOK ALL BUT YOUR SHIRT, EH? TAKE MY FATHER'S OLD BLADE FROM OVER THE HEARTH.";
      // point at the job the journal now tracks, so the advice matches the quest (not a made-up favour)
      {
        const Quest* nxt = nullptr;
        for (const auto& o : quests) if (o.id == trackedQuest) nxt = &o;
        if (nxt && nxt->type == QType::Hunt) dlg.text += std::string(" FOLK SAY THE ") + monsterPlural(nxt->mon) + " ARE BAD THIS YEAR.";
        else dlg.text += " NOW GO EARN YOUR KEEP: SOMEONE ROUND HERE ALWAYS NEEDS A HAND.";
      }
      break;
    }
  if (std::string t = questDialogue(a); !t.empty()) dlg.text = t;   // M2: a parcel's recipient (quests.cpp)
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
  if (a.role == Role::Mage) {
    if (!(spellsKnown & (1 << (int)Spell::Heal))) dlg.opts.push_back({"TEACH ME MEND (120 GOLD)", A_LEARN, (int)Spell::Heal});
    if (!(spellsKnown & (1 << (int)Spell::IceSpike))) dlg.opts.push_back({"TEACH ME FROST LANCE (300 GOLD)", A_LEARN, (int)Spell::IceSpike});
  }
  warTalk(a);     // M4: the war's talk (siege commanders, guards' news of the front; war_game.cpp)
  storyTalk(a);   // M4: story quests, gossip, the realm's news (rpg/story/story_game.cpp)
  dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
  mode = Mode::Dialogue;
  sfx((int)Sfx::Talk, a.p);
}

void Game::closeDialogue() { if (mode == Mode::Dialogue) mode = Mode::Play; }

void Game::dialogueChoose(int oi) {
  if (oi < 0 || oi >= (int)dlg.opts.size()) return;
  DlgOpt o = dlg.opts[oi];
  int ai = findActor(dlg.actor);
  sfx((int)Sfx::MenuSelect, pl().p);
  switch (o.action) {
    case A_BYE: mode = Mode::Play; return;
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
      if (gold < o.arg) { dlg.text = "NO GOLD, NO GOSSIP. THE ALE ISN'T FREE EITHER."; return; }
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
      dlg.text = pendingPitch_.empty() ? pendingOffer_.desc + " REWARD: " + std::to_string(pendingOffer_.gold) + " GOLD." : pendingPitch_;
      dlg.opts = {{"I'LL DO IT.", A_ACCEPT, 0}, {"NOT RIGHT NOW.", A_DECLINE, 0}};
      return;
    case A_ACCEPT:
      acceptQuest(pendingOffer_);
      dlg.text = pendingOffer_.type == QType::Deliver ? "HERE'S THE PARCEL. KEEP IT DRY, AND KEEP IT SHUT."
                 : pendingOffer_.type == QType::Protect ? "THEN BE AT THE FIELDS BY NIGHTFALL. THEY COME AFTER DARK."
                 : "THANK YOU. COME BACK WHEN IT'S DONE.";
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
      for (auto& q : quests) if (q.id == o.arg && q.state == QState::Complete) { completeQuest(q); break; }
      dlg.text = "YOU HAVE MY THANKS. HERE, YOU'VE EARNED THIS.";
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
      dlg.text = "GO WITH THE OLD GODS, CHILD.";
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
      mode = Mode::Play;
      return;
  }
}

void Game::rest(int hours) {
  hour += hours;
  while (hour >= 24) { hour -= 24; day++; }
  Actor& p = pl();
  p.hp = p.maxHp; mp = maxMp; stamina = maxSt;
  sleepFade = 1.5f;
  sfx((int)Sfx::Heal, p.p, 0.7f);
}

// ------------------------------------------------------------------ shops
std::vector<Item> Game::shopStock(const Actor& a) {
  Rng r(hash32((uint32_t)npcKey(a)) ^ (uint32_t)(day / 2) ^ (uint32_t)seed);
  std::vector<Item> s;
  int lvl = std::max(plLevel, a.site >= 0 ? world.sites[a.site].level : 1);
  switch (a.role) {
    case Role::Smith:
      for (int i = 0; i < 4; i++) s.push_back(makeWeapon(r, lvl, -1, i > 1));
      for (int i = 0; i < 3; i++) s.push_back(makeArmor(r, lvl, i == 0 ? ItemKind::Armor : (i == 1 ? ItemKind::Helmet : ItemKind::Shield)));
      s.push_back(makeBow(r, lvl));
      s.push_back(makeArrows(25));
      s.push_back(makeArmor(r, lvl, ItemKind::Gloves));   // M0: the new slots (appended: the items above keep their rolls)
      s.push_back(makeArmor(r, lvl, ItemKind::Boots));
      break;
    case Role::Mage:
      s.push_back(makeStaff(r, lvl));
      s.push_back(makePotion(PotionType::Magicka, 1)); s.back().count = 4;
      s.push_back(makeJewel(r, lvl));
      s.push_back(makeJewel(r, lvl));
      break;
    case Role::Priest:
      s.push_back(makePotion(PotionType::Health, 0)); s.back().count = 5;
      s.push_back(makePotion(PotionType::Health, 1)); s.back().count = 3;
      s.push_back(makePotion(PotionType::Magicka, 0)); s.back().count = 3;
      s.push_back(makeJewel(r, lvl));
      break;
    // M2 wayside traders (wayside.cpp): a hunter's pelts, meat and arrows; a fisher's catch; an herbalist's herbs and potions
    case Role::Hunter:
      s.push_back(makeMisc(0)); s.back().count = 3;
      s.push_back(makeMisc(8)); s.back().count = 2;
      if (r.f() < 0.5f) s.push_back(makeMisc(9));
      s.push_back(makeFood(1)); s.back().count = 4;
      s.push_back(makeFood(7)); s.back().count = 3;
      s.push_back(makeArrows(40));
      s.push_back(makeBow(r, lvl));
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
      s.push_back(makeWeapon(r, lvl, (int)WeaponType::Dagger));
      s.push_back(makeArmor(r, lvl, ItemKind::Helmet));
      if (r.f() < 0.5f) s.push_back(makeJewel(r, lvl));
      s.push_back(makeArmor(r, lvl, ItemKind::Cloak));   // M0: the new slots (appended: the items above keep their rolls)
      s.push_back(makeArmor(r, lvl, r.f() < 0.5f ? ItemKind::Boots : ItemKind::Gloves));
      break;
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
  gold -= price;
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
  int price = std::max(1, it.value * 2 / 5);
  if (it.kind == ItemKind::Arrows) price = std::max(1, it.count / 3);
  gold += price;
  Item sold = it;
  sold.count = it.kind == ItemKind::Arrows ? it.count : 1;
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
// SAVE_VER 10 (M4: the realm and story blocks appended after marks). SAVE_VER 9 (M3c: the layout of 8, bumped with the Wildlands world generation). SAVE_VER 8 (M3b: the layout of 7, bumped with the builder's world generation). SAVE_VER 7 (M3: the appearance block gains people, homeland and personal heraldry; nothing else moved).
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
//   story    (v10) u32 byte length + the story block (story::Engine::serialize: its own version byte first)
// refs: site ref = u64 id (0 none); bldg ref = u64 id + u64 owner site id (0 none); map ref = u8 kind (0 overworld,
// 1 cave/ruin, 2 building, 3 dens) + u64 id + u64 owner + u8 floor.
static constexpr uint32_t SAVE_MAGIC = 0x454D4256;   // EMBV
static constexpr uint32_t SAVE_VER = 10;  // 10: M4 Banners (the realm and story blocks after marks; ENDLESS_GEN_VER 13);
                                          // 9: M3c Wildlands (the layout of 8; the world's biomes, flora and wildlife
                                          //    are new, ENDLESS_GEN_VER 12, so older adventures start anew);
                                          // 8: M3b Builders & Societies (the world is built by the builder: a new
                                          //    generation, so older adventures start anew); 7: M3 (appearance: people,
                                          //    homeland, personal heraldry)

int Game::currentSaveVersion() { return (int)SAVE_VER; }
int Game::saveVersion(const std::vector<uint8_t>& in) {
  BinR r(in);
  if (r.u32() != SAVE_MAGIC || r.bad) return 0;
  uint32_t v = r.u32();
  return r.bad ? 0 : (int)v;
}
bool Game::saveFromOlderGenerator(const std::vector<uint8_t>& in) {
  BinR r(in);
  if (r.u32() != SAVE_MAGIC || r.bad) return false;
  if (r.u32() != SAVE_VER || r.bad) return false;
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
  w.u8(spellsKnown); w.u8((uint8_t)spell);
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
  }
}

bool Game::deserialize(const std::vector<uint8_t>& in) {
  BinR r(in);
  if (r.u32() != SAVE_MAGIC) return false;
  if (r.u32() != SAVE_VER || r.bad) return false;   // older (or newer) saves are not loaded: the title says so
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
  // M4 (v10): the realm and story blocks (a block its owner cannot read refuses the whole save)
  for (int blkI = 0; blkI < 2 && !r.bad; blkI++) {
    n = r.u32();
    if (r.bad || n > (64u << 20) || r.p + n > in.size()) return false;
    std::vector<uint8_t> blk(in.begin() + (std::ptrdiff_t)r.p, in.begin() + (std::ptrdiff_t)(r.p + n));
    r.p += n;
    if (blkI == 0 ? !realm.deserialize(blk) : !story.deserialize(blk)) return false;
  }
  if (r.bad) return false;
  // re-apply looted overworld chests (by global tile)
  world.over.rebuildSolid();
  reapplyLooted();
  recalcPlayer();
  // (fixer M4 r1) the realm's owners on the loaded settlements before anyone is spawned: a save made inside a conquered
  // town's building (its palace) loads with the conqueror's guards and ruler, not the generator's (realmStep waits
  // outside). Only the states the realm already holds are applied (noting new sites here would change the realm the
  // save holds: realmStep notes them on the first step outside, as before)
  realmSync();
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
  if (hp <= 0) respawn();   // saved on the death screen: wake in town rather than standing up with 1 HP where you fell
  events.clear();
  updateLocation();
  return true;
}
