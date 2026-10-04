// EMBERVALE RPG systems: inventory, loot, quests, dialogue, shops, resting, travel, saving.
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/sim/game.h"

using art::Monster;
using art::Prop;

enum DlgAct { A_BYE, A_TRADE, A_REST, A_RUMOR, A_ACCEPT, A_TURNIN, A_MAIN, A_HEAL, A_LEARN, A_CHAT, A_DECLINE, A_ASK };

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
  Map& m = map();
  size_t i = (size_t)ty * m.w + tx;
  m.prop[i] = (uint8_t)((int)Prop::ChestOpen + 1);
  looted.insert(((uint64_t)mapKey() << 32) | (uint32_t)i);
  m.rebuildSolid();
  sfx((int)Sfx::Chest, pl().p);
  int lvl = inside && subSite >= 0 ? world.sites[subSite].level : world.zoneLevel(tx, ty);
  Rng r(hash2(tx, ty, (uint32_t)seed + mapKey()));
  bool rich = inside && subSite >= 0;
  int n = 1 + r.irange(rich ? 3 : 2);
  Vec2 c(tx * TILE + 8.0f, ty * TILE + 18.0f);
  for (int k = 0; k < n; k++) { Pickup p; p.p = c + Vec2(r.range(-8, 8), r.range(0, 8)); p.item = randomLoot(r, lvl, rich && k == 0 && r.f() < 0.35f); pickups.push_back(p); }
  Pickup g; g.p = c; g.gold = 8 + r.irange(15 + lvl * 4) + (rich ? 20 : 0); pickups.push_back(g);
}

// ------------------------------------------------------------------ interaction
int Game::interactTarget() const {
  const Actor& p = pl();
  int best = -1; float bd = 24 * 24;
  for (size_t i = 1; i < actors.size(); i++) {
    const Actor& a = actors[i];
    if (!a.npc || a.st == AState::Dead) continue;
    float d = len2(a.p - p.p);
    // shopkeepers behind counters can be reached across them
    float reach = (a.role == Role::Merchant || a.role == Role::Innkeeper || a.role == Role::Smith) ? 40.0f : 24.0f;
    if (d >= reach * reach) continue;
    // someone with a reward waiting or work to offer wins over a bystander about as close (two villagers side by
    // side: the prompt and the tap go to the one with something to say)
    if (rewardWaiting(a) || hasOffer(a)) d *= 0.45f;
    if (d < bd + (reach * reach - 24 * 24)) { bd = d; best = a.id; }
  }
  return best;
}

bool Game::nearDoorOrExit() const { return false; }

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
                      (prop == Prop::Signpost && !inside && world.siteAt(tx, ty, 3) >= 0) ||
                      (prop == Prop::Bed && inside && subBldg >= 0 && world.over.bldgs[subBldg].type != art::Building::Inn);
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
  if (prop == Prop::Bed) {
    rest(8);
    say("YOU SLEEP SOUNDLY.");
    return;
  }
}

uint64_t Game::npcKey(const Actor& a) const {
  return ((uint64_t)(a.site + 1) << 24) ^ ((uint64_t)(a.bldg + 1) << 12) ^ (uint64_t)(a.slot & 0xFFF);
}

const Quest* Game::questById(int id) const {
  for (auto& q : quests) if (q.id == id) return &q;
  return nullptr;
}

namespace {
const char* monsterPlural(Monster m) {
  static const char* mn[] = {"WOLVES", "BOARS", "BEARS", "SLIMES", "SPIDERS", "BATS", "SKELETONS", "DRAUGR", "GOBLINS", "TROLLS", "WRAITHS", "MUDCRABS", "ICE WOLVES", "RIME SPIDERS", "SANDWORMS", "DRAGONS"};
  return mn[(int)m];
}
// compass direction of a tile offset (north is up the map): "NORTH-EAST" or, short, "NE"
std::string dirWord(int dx, int dy, bool shortForm) {
  static const char* lf[8] = {"EAST", "NORTH-EAST", "NORTH", "NORTH-WEST", "WEST", "SOUTH-WEST", "SOUTH", "SOUTH-EAST"};
  static const char* sf[8] = {"E", "NE", "N", "NW", "W", "SW", "S", "SE"};
  if (dx == 0 && dy == 0) return shortForm ? "HERE" : "RIGHT HERE";
  float a = std::atan2((float)-dy, (float)dx);
  int k = ((int)std::lround(a / (3.14159265f / 4)) % 8 + 8) % 8;
  return shortForm ? sf[k] : lf[k];
}
// the giver's town name ("" when unknown)
std::string giverTown(const World& w, const Quest& q) {
  return q.giverSite >= 0 && q.giverSite < (int)w.sites.size() ? w.sites[q.giverSite].name : std::string();
}
// "BOUNTY READY: RETURN TO X IN Y": shown as a notice when a radiant quest's objective is done
std::string rewardReadyMsg(const World& w, const Quest& q) {
  std::string town = giverTown(w, q);
  return std::string(q.type == QType::Clear ? "REWARD READY" : "BOUNTY READY") + ": RETURN TO " + q.giverName + (town.empty() ? "" : " IN " + town);
}
// the first of these that fits a journal line (36 characters), else the last one cut down
std::string fitLine(std::initializer_list<std::string> opts) {
  std::string last;
  for (const std::string& o : opts) { if (o.size() <= 36) return o; last = o; }
  return last.substr(0, 36);
}
// where the player is on the overworld (the door or entrance they went in by, when inside)
void overworldTile(const Game& g, int& x, int& y) {
  if (g.inside && g.subBldg >= 0) { x = g.world.over.bldgs[g.subBldg].doorX(); y = g.world.over.bldgs[g.subBldg].doorY() + 1; return; }
  if (g.inside && g.subSite >= 0) { x = g.world.sites[g.subSite].ex; y = g.world.sites[g.subSite].ey; return; }
  x = (int)std::floor(g.pl().p.x / TILE); y = (int)std::floor(g.pl().p.y / TILE);
}
// is this NPC the one who gave the quest? (the opening's giver is whoever keeps the start village inn)
bool isGiver(const Quest& q, const Actor& a) {
  if (!a.npc) return false;
  if (q.giverSlot < 0) return a.role == Role::Innkeeper && a.bldg >= 0 && a.bldg == q.giverBldg;
  return q.giverSite == a.site && q.giverBldg == a.bldg && q.giverSlot == a.slot;
}
}  // namespace

std::string Game::greeting(const Actor& a) {
  Rng r(hash32((uint32_t)npcKey(a)) ^ (uint32_t)(day * 31 + (int)hour));
  const std::string town = a.site >= 0 ? world.sites[a.site].name : "THESE PARTS";
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
      if (q.stage == 0 && a.site != world.capital) local.push_back("THEY SAY THE JARL OF " + world.sites[world.capital].name + " WANTS SWORDS AGAINST THE DRAGON.");
      if (q.stage >= 1 && q.stage <= 3) local.push_back("THE WHOLE HOLD IS TALKING ABOUT YOU AND THE DRAGON. THE OLD GODS KEEP YOU.");
      if (q.state == QState::Done) local.push_back("IT'S YOU! THE ONE WHO SLEW ASHFANG! YOUR MEAD IS FREE IN " + town + ".");
    }
    if (plLevel >= 10) local.push_back("BY THE OLD GODS, YOU LOOK LIKE YOU'VE WALKED THROUGH A WAR.");
  }
  auto withLocal = [&](const std::string& fixed) { return !local.empty() && r.f() < 0.5f ? local[r.irange((int)local.size())] : fixed; };
  switch (a.role) {
    case Role::Guard: {
      static const char* g[] = {"I WAS A SELLSWORD ONCE. NOW I WATCH A GATE AND COUNT CARTS.",
                                "KEEP YOUR NOSE CLEAN, TRAVELLER.", "LOST SOMETHING? TRY THE INN. EVERYTHING ENDS UP AT THE INN.",
                                "WATCH THE ROADS AT NIGHT. THE DEAD DON'T STAY BURIED HERE.", "EYES OPEN, BLADE SHARP. THAT'S THE WATCH'S WAY."};
      if (isNight() && r.f() < 0.4f) return "QUIET NIGHT. TOO QUIET. KEEP YOUR BLADE CLOSE.";
      return g[r.irange(5)];
    }
    case Role::Innkeeper: {
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
    case Role::Jarl: return "SPEAK, STRANGER. THE JARL OF " + town + " IS LISTENING.";
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
  if (!a.npc || (a.role == Role::Guard && !noble) || a.role == Role::Child || a.site < 0) return false;
  uint64_t key = npcKey(a);
  for (auto& q : quests) if (q.state != QState::Done && q.type != QType::Main && q.giverSite == a.site && q.giverBldg == a.bldg && q.giverSlot == a.slot) return false;
  auto it = npcQuestsDone.find(key);
  int done = it == npcQuestsDone.end() ? 0 : it->second;
  if (a.role == Role::Jarl) return done < (noble ? 9 : 6);
  if (a.role == Role::Innkeeper) return done < 6;
  if (a.role == Role::Guard) return done < 2;
  if (a.role == Role::Priest || a.role == Role::Smith || a.role == Role::Mage) return done < 2;
  return (hash32((uint32_t)key ^ (uint32_t)seed) % 3 == 0) && done < 2;
}

Quest Game::makeOffer(const Actor& a) {
  uint64_t key = npcKey(a);
  auto it = npcQuestsDone.find(key);
  int done = it == npcQuestsDone.end() ? 0 : it->second;
  Rng r(hash32((uint32_t)key * 31 + done) ^ (uint32_t)seed);
  Quest q;
  q.giverSite = a.site; q.giverBldg = a.bldg; q.giverSlot = a.slot; q.giverName = a.name;
  const Site& home = world.sites[a.site];
  // nearest un-cleared site of a type, not already targeted
  auto pickSite = [&](SiteType t) {
    int best = -1; float bd = 1e30f;
    for (int i = 0; i < (int)world.sites.size(); i++) {
      const Site& s = world.sites[i];
      if (s.type != t || s.cleared || s.mainQuest) continue;
      bool taken = false;
      for (auto& o : quests) if (o.state != QState::Done && o.target == i) taken = true;
      if (taken) continue;
      float d = std::hypot((float)(s.ex - home.ex), (float)(s.ey - home.ey)) * (0.8f + r.f() * 0.5f);
      if (d < bd) { bd = d; best = i; }
    }
    return best;
  };
  int kind = r.irange(3);
  if (a.role == Role::Jarl) kind = r.f() < 0.5f ? 0 : 3;
  if (a.role == Role::Innkeeper) kind = r.f() < 0.6f ? 2 : 1;
  if (a.role == Role::Priest) kind = 3;
  if (a.role == Role::Smith) kind = 1;
  if (a.role == Role::Guard) kind = r.f() < 0.6f ? 2 : 0;   // the watch wants camps and caves cleared
  int lvl = std::max(home.level, plLevel);
  if (kind == 0) {   // clear a cave
    q.type = QType::Clear;
    q.target = pickSite(SiteType::Cave);
    if (q.target < 0) kind = 1;
    else {
      const Site& t = world.sites[q.target];
      q.title = "CLEAR " + t.name;
      q.desc = "SOMETHING FOUL HAS MADE ITS NEST IN " + t.name + ". KILL WHATEVER LEADS THEM AND THE ROADS WILL BE SAFER. RETURN TO " + a.name + " IN " + home.name + ".";
    }
  }
  if (kind == 3) {   // ruin
    q.type = QType::Clear;
    q.target = pickSite(SiteType::Ruin);
    if (q.target < 0) kind = 2;
    else {
      const Site& t = world.sites[q.target];
      q.title = "THE RESTLESS DEAD";
      q.desc = "THE DEAD STIR IN " + t.name + ". PUT THEIR MASTER BACK IN THE GROUND, THEN REPORT TO " + a.name + ".";
    }
  }
  if (kind == 2) {   // bandit bounty
    q.type = QType::Bounty;
    q.target = pickSite(SiteType::BanditCamp);
    if (q.target < 0) kind = 1;
    else {
      const Site& t = world.sites[q.target];
      q.title = "BOUNTY: " + t.name;
      q.desc = "BANDITS AT " + t.name + " HAVE BEEN RAIDING TRAVELLERS. KILL THEIR CHIEF AND COLLECT THE BOUNTY FROM " + a.name + ".";
    }
  }
  if (kind == 1) {   // hunt
    q.type = QType::Hunt;
    Biome b = world.over.biomeAt(home.ex, home.ey);
    Monster opts[3] = {Monster::Wolf, Monster::Boar, Monster::Spider};
    if (b == Biome::Snow) { opts[0] = Monster::IceWolf; opts[1] = Monster::FrostSpider; opts[2] = Monster::Troll; }
    else if (b == Biome::Taiga) { opts[1] = Monster::Bear; opts[2] = Monster::Troll; }
    else if (b == Biome::Swamp) { opts[0] = Monster::Slime; opts[1] = Monster::Mudcrab; }
    else if (b == Biome::Desert) { opts[0] = Monster::Sandworm; opts[1] = Monster::Goblin; opts[2] = Monster::Skeleton; }
    else if (b == Biome::Plains) { opts[2] = Monster::Goblin; }
    q.mon = opts[r.irange(3)];
    q.need = (q.mon == Monster::Troll || q.mon == Monster::Bear || q.mon == Monster::Sandworm) ? 2 : 4 + r.irange(3);
    static const char* mn[] = {"WOLVES", "BOARS", "BEARS", "SLIMES", "SPIDERS", "BATS", "SKELETONS", "DRAUGR", "GOBLINS", "TROLLS", "WRAITHS", "MUDCRABS", "ICE WOLVES", "RIME SPIDERS", "SANDWORMS", "DRAGONS"};
    q.title = std::string("CULL THE ") + mn[(int)q.mon];
    q.desc = std::string("THE ") + mn[(int)q.mon] + " AROUND " + home.name + " ARE GETTING BOLD. KILL " + std::to_string(q.need) + " OF THEM AND " + a.name + " WILL PAY YOU.";
    q.target = -1;
  }
  q.gold = 60 + lvl * 22 + (q.type == QType::Hunt ? 0 : 80) + r.irange(40);
  q.xp = 40 + lvl * 14 + (q.type == QType::Hunt ? 0 : 50);
  return q;
}

void Game::acceptQuest(const Quest& q0) {
  Quest q = q0;
  q.id = nextQuestId++;
  q.state = QState::Active;
  if (q.target >= 0) {
    world.sites[q.target].discovered = true;
    if (world.sites[q.target].cleared) q.state = QState::Complete;
  }
  quests.push_back(q);
  // the opening stays tracked until the old blade is collected: an unarmed newcomer who takes a villager's job on
  // the way to the inn still has the pin over the inn door
  if (openingQuest() < 0 || q.type == QType::Main) trackedQuest = q.id;
  emit(Ev::QuestUpdate, pl().p, q.id, 0, "QUEST STARTED: " + q.title);
  sfx((int)Sfx::QuestStart, pl().p);
}

void Game::completeQuest(Quest& q) {
  q.state = QState::Done;
  giveGold(q.gold);
  gainXp(q.xp);
  npcQuestsDone[((uint64_t)(q.giverSite + 1) << 24) ^ ((uint64_t)(q.giverBldg + 1) << 12) ^ (uint64_t)(q.giverSlot & 0xFFF)]++;
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
    if (q.state == QState::Active && q.target == si && (q.type == QType::Clear || q.type == QType::Bounty)) {
      q.state = QState::Complete;
      forQuest = true;
      std::string m = rewardReadyMsg(world, q);
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
        q.desc = "THE JARL SAYS ONLY THE EMBER CROWN CAN BIND THE DRAGON ASHFANG. ITS THREE SHARDS LIE WITH DRAUGR WARLORDS IN ANCIENT RUINS. RECOVER ALL THREE.";
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
        q.title = "RETURN TO THE JARL";
        q.desc = "YOU HAVE ALL THREE EMBER SHARDS. BRING THEM TO THE JARL IN " + cap.name + ".";
        q.target = world.capital;
        emit(Ev::QuestUpdate, pl().p, q.id, 2, "ALL SHARDS FOUND! RETURN TO " + cap.name);
        sfx((int)Sfx::QuestDone, pl().p);
        break;
      case 3:
        q.title = "DRAGONSLAYER";
        q.desc = "THE CROWN IS WHOLE AND ASHFANG CAN BE KILLED. CLIMB TO SKYFANG PEAK AND END THE DRAGON.";
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
      // the keep's door
      const Site& c = world.sites[t];
      for (int b = c.bldgFirst; b < c.bldgFirst + c.bldgCount; b++)
        if (world.over.bldgs[b].type == art::Building::Keep) { tx = world.over.bldgs[b].doorX(); ty = world.over.bldgs[b].doorY(); return true; }
    }
    tx = world.sites[t].ex; ty = world.sites[t].ey;
    return true;
  }
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
      case 0: return fitLine({"SPEAK TO THE JARL IN " + cap, "THE JARL IN " + cap});
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
      case 2: return fitLine({"RETURN TO THE JARL IN " + cap, "RETURN TO " + cap});
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
      bool bounty = q.type == QType::Bounty || q.type == QType::Hunt;
      dlg.opts.push_back({std::string(bounty ? "COLLECT BOUNTY (+" : "COLLECT REWARD (+") + std::to_string(q.gold) + " GOLD)", A_TURNIN, q.id});
      dlg.text = (q.target >= 0 && q.target < (int)world.sites.size() ? "YOU'RE BACK! THEY SAY " + world.sites[q.target].name + " HAS GONE QUIET."
                                                                        : std::string("YOU'RE BACK, AND THE ") + monsterPlural(q.mon) + " ARE THINNED OUT.") +
                 " I OWE YOU " + std::to_string(q.gold) + " GOLD.";
    }
  // main quest hooks
  for (auto& q : quests) {
    if (q.type != QType::Main || a.role != Role::Jarl || a.site != world.capital) continue;
    if (q.stage == 0) { dlg.text = "SO YOU'VE HEARD THE RUMOURS. THEY ARE TRUE: A DRAGON, ASHFANG, HAS WOKEN BENEATH SKYFANG PEAK. STEEL ALONE CANNOT KILL IT."; dlg.opts.push_back({"HOW CAN IT BE STOPPED?", A_MAIN, 1}); }
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
  if (hasOffer(a)) dlg.opts.push_back({a.role == Role::Innkeeper ? "ANY WORK GOING?" : "DO YOU NEED HELP?", A_ASK, 0});
  if (a.role == Role::Merchant || a.role == Role::Smith || a.role == Role::Mage || a.role == Role::Priest || a.role == Role::Innkeeper)
    dlg.opts.push_back({"LET ME SEE YOUR WARES.", A_TRADE, 0});
  if (a.role == Role::Innkeeper) { dlg.opts.push_back({"RENT A ROOM (10 GOLD)", A_REST, 10}); dlg.opts.push_back({"HEARD ANY RUMORS?", A_RUMOR, 0}); }
  if (a.role == Role::Priest) dlg.opts.push_back({"BLESS ME.", A_HEAL, 0});
  if (a.role == Role::Mage) {
    if (!(spellsKnown & (1 << (int)Spell::Heal))) dlg.opts.push_back({"TEACH ME MEND (120 GOLD)", A_LEARN, (int)Spell::Heal});
    if (!(spellsKnown & (1 << (int)Spell::IceSpike))) dlg.opts.push_back({"TEACH ME FROST LANCE (300 GOLD)", A_LEARN, (int)Spell::IceSpike});
  }
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
    case A_REST:
      if (gold < o.arg) { dlg.text = "YOU DON'T HAVE ENOUGH GOLD."; return; }
      gold -= o.arg;
      rest(8);
      dlg.text = "YOU WAKE WELL RESTED. GOOD MORNING!";
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return;
    case A_RUMOR: {
      // reveal an undiscovered dungeon nearby
      int best = -1; float bd = 1e30f;
      const Actor& p = pl();
      for (int i = 0; i < (int)world.sites.size(); i++) {
        const Site& s = world.sites[i];
        if (s.discovered || (s.type != SiteType::Cave && s.type != SiteType::Ruin && s.type != SiteType::BanditCamp && s.type != SiteType::Shrine)) continue;
        float d = std::hypot(s.ex * 16.0f - p.p.x, s.ey * 16.0f - p.p.y);
        if (d < bd) { bd = d; best = i; }
      }
      if (best < 0) dlg.text = "CAN'T SAY I'VE HEARD ANYTHING NEW.";
      else {
        world.sites[best].discovered = true;
        dlg.text = "THEY SAY THERE'S A PLACE CALLED " + world.sites[best].name + " NOT FAR FROM HERE. I'VE MARKED IT ON YOUR MAP.";
      }
      for (size_t k = 0; k < dlg.opts.size(); k++) if (dlg.opts[k].action == A_RUMOR) { dlg.opts.erase(dlg.opts.begin() + k); break; }
      return;
    }
    case A_ASK:
      if (ai < 0) return;
      pendingOffer_ = makeOffer(actors[ai]);
      {
        // the log keeps the third-person description; spoken aloud, the giver says "me", not their own name
        std::string pitch = pendingOffer_.desc;
        const std::string& nm = pendingOffer_.giverName;
        auto rep = [&](const std::string& from, const std::string& to) {
          for (size_t at = pitch.find(from); at != std::string::npos; at = pitch.find(from, at + to.size())) pitch.replace(at, from.size(), to);
        };
        if (pendingOffer_.giverSite >= 0) rep("TO " + nm + " IN " + world.sites[pendingOffer_.giverSite].name + ".", "TO ME.");
        rep("REPORT TO " + nm + ".", "REPORT BACK TO ME.");
        rep("FROM " + nm + ".", "FROM ME.");
        rep("AND " + nm + " WILL PAY YOU.", "AND I WILL PAY YOU.");
        rep("TO " + nm + ".", "TO ME.");
        dlg.text = pitch + " REWARD: " + std::to_string(pendingOffer_.gold) + " GOLD.";
      }
      dlg.opts = {{"I'LL DO IT.", A_ACCEPT, 0}, {"NOT RIGHT NOW.", A_DECLINE, 0}};
      return;
    case A_ACCEPT:
      acceptQuest(pendingOffer_);
      dlg.text = "THANK YOU. COME BACK WHEN IT'S DONE.";
      dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return;
    case A_DECLINE: mode = Mode::Play; return;
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
      else dlg.text = "THEN GO, WITH THE BLESSINGS OF THE HOLD. ASHFANG NESTS ON SKYFANG PEAK. TAKE THIS GOLD FOR SUPPLIES.";
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
    default: mode = Mode::Play; return;
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

// ------------------------------------------------------------------ travel / death
bool Game::fastTravel(int si) {
  if (si < 0 || si >= (int)world.sites.size() || !world.sites[si].discovered) return false;
  for (const Actor& a : actors) if (a.hostile && a.aggro && a.st != AState::Dead && len2(a.p - pl().p) < 120 * 120) { say("YOU CANNOT TRAVEL WITH ENEMIES NEARBY"); return false; }
  if (inside) leaveSub();
  const Site& s = world.sites[si];
  float dist = std::hypot(s.ex * 16.0f - pl().p.x, s.ey * 16.0f - pl().p.y) / 16.0f;
  hour += dist / 30.0f;
  while (hour >= 24) { hour -= 24; day++; }
  clearNonPlayer();
  int ty = s.ey + (s.type == SiteType::Cave ? 2 : (s.type == SiteType::Ruin ? 4 : 1));
  // arrive at the edge of hostile places, not in the bandit chief's lap or under the dragon
  if (s.type == SiteType::BanditCamp) ty = s.r.y + s.r.h + 4;
  if (s.type == SiteType::DragonLair) ty = s.ey + 5;
  placePlayerAt(s.ex, ty);
  sleepFade = 1.2f;
  mode = Mode::Play;
  updateLocation();
  emit(Ev::MapChange, pl().p);
  return true;
}

void Game::respawn() {
  if (inside) { inside = false; subBldg = -1; subSite = -1; sub = Map(); }
  clearNonPlayer();
  int si = lastTown >= 0 ? lastTown : world.startSite;
  const Site& s = world.sites[si];
  placePlayerAt(s.r.cx(), s.r.cy() + 1);
  Actor& p = pl();
  p.st = AState::Idle; p.stT = 0; p.hp = p.maxHp; p.burnT = 0; p.slowT = 0;
  p.knock = Vec2(); p.vel = Vec2(); p.iframes = 1.5f;   // a moment of grace after waking
  mp = maxMp; stamina = maxSt;
  int lost = gold / 10;
  gold -= lost;
  mode = Mode::Play;
  sleepFade = 1.5f;
  say(lost > 0 ? "YOU WAKE IN " + s.name + ". LOST " + std::to_string(lost) + " GOLD." : "YOU WAKE IN " + s.name + ".");
  updateLocation();
  emit(Ev::MapChange, p.p);
}

// ------------------------------------------------------------------ save / load
// Format history (every version must keep loading; tests/fixtures/save_v1.bin + save_test guard this):
//   v1  magic, ver, seed, ... (the original layout, world always built with WORLDGEN_V1)
//   v2  adds the world-gen version and the world fingerprint right after the version
//   v3  (M0) appends the character block at the end: the gloves/boots/cloak slots, the background, the story flags
//       and the appearance as a length-prefixed block (fields appended later are skipped by older readers)
// To change the format: bump SAVE_VER, write the new layout, and gate each new or changed read on `ver >= N`
// with a default for older saves. Add new fields at the end of the save where possible.
static constexpr uint32_t SAVE_MAGIC = 0x454D4256;   // EMBV
static constexpr uint32_t SAVE_VER = 3;

namespace {
// The appearance block: u16 byte length, then the fields in this order. Append new fields at the end only.
void writeAppearance(BinW& w, const Appearance& a) {
  std::vector<uint8_t> blk;
  BinW b(blk);
  b.str(a.name); b.u8(a.female ? 1 : 0); b.u8(a.build); b.u8(a.skinTone); b.u32(a.skin); b.u8(a.hair); b.u32(a.hairColor);
  b.u8(a.beard ? 1 : 0); b.u32(a.eyeColor); b.u32(a.topColor); b.u32(a.bottomColor); b.u8(a.created ? 1 : 0);
  w.u16((uint16_t)blk.size());
  for (uint8_t c : blk) w.u8(c);
}
bool readAppearance(BinR& r, Appearance& a) {
  int n = r.u16();
  std::vector<uint8_t> blk;
  for (int i = 0; i < n && !r.bad; i++) blk.push_back(r.u8());
  if (r.bad) return false;
  BinR b(blk);
  Appearance d;   // fields missing from an older (shorter) block keep these defaults
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
  if (b.bad) return false;
  a = d;
  return true;
}
}  // namespace

void Game::serialize(std::vector<uint8_t>& out) const {
  out.clear();
  BinW w(out);
  w.u32(SAVE_MAGIC); w.u32(SAVE_VER);
  w.u32((uint32_t)world.genVersion); w.u32(world.fingerprint());   // v2
  w.u64(seed); w.f32(time); w.f32(hour); w.i32(day);
  const Actor& p = pl();
  w.u8(inside ? 1 : 0); w.i32(subSite); w.i32(subBldg);
  w.f32(p.p.x); w.f32(p.p.y); w.f32(p.hp); w.f32(mp); w.f32(stamina);
  w.i32(plLevel); w.i32(plXp); w.i32(gold); w.i32(perkPts);
  w.f32(baseHp); w.f32(maxMp); w.f32(maxSt);
  w.u32((uint32_t)inv.size());
  for (auto& it : inv) writeItem(w, it);
  for (int e : {eqWeapon, eqBow, eqStaff, eqArmor, eqHelmet, eqShield, eqRing, eqAmulet}) w.i32(e);
  w.u8(spellsKnown); w.u8((uint8_t)spell);
  w.i32(kills); w.i32(dungeonsCleared); w.f32(blessT); w.str(blessName); w.i32(lastTown);
  w.u32((uint32_t)quests.size());
  for (auto& q : quests) {
    w.i32(q.id); w.u8((uint8_t)q.type); w.u8((uint8_t)q.state); w.str(q.title); w.str(q.desc); w.str(q.giverName);
    w.i32(q.giverSite); w.i32(q.giverBldg); w.i32(q.giverSlot); w.i32(q.target); w.u8((uint8_t)q.mon);
    w.i32(q.need); w.i32(q.have); w.i32(q.gold); w.i32(q.xp); w.i32(q.stage);
  }
  w.i32(nextQuestId); w.i32(trackedQuest);
  w.u32((uint32_t)npcQuestsDone.size());
  for (auto& kv : npcQuestsDone) { w.u64(kv.first); w.i32(kv.second); }
  w.u32((uint32_t)looted.size());
  for (uint64_t k : looted) w.u64(k);
  uint32_t nk = 0;
  for (auto& kv : killedSlots) if (!kv.second.empty()) nk++;
  w.u32(nk);
  for (auto& kv : killedSlots) {
    if (kv.second.empty()) continue;
    w.i32(kv.first); w.u32((uint32_t)kv.second.size()); for (int s : kv.second) w.i32(s); }
  w.u32((uint32_t)world.sites.size());
  for (auto& s : world.sites) w.u8((uint8_t)((s.discovered ? 1 : 0) | (s.cleared ? 2 : 0)));
  // v3: the character block
  w.i32(eqGloves); w.i32(eqBoots); w.i32(eqCloak);
  w.u8((uint8_t)background); w.u32(storyFlags);
  writeAppearance(w, app);
}

bool Game::deserialize(const std::vector<uint8_t>& in) {
  BinR r(in);
  if (r.u32() != SAVE_MAGIC) return false;
  const uint32_t ver = r.u32();
  if (ver < 1 || ver > SAVE_VER) return false;   // a save from a newer build
  int genVer = WORLDGEN_V1;
  uint32_t fp = 0;
  if (ver >= 2) { genVer = (int)r.u32(); fp = r.u32(); }
  if (r.bad) return false;
  if (genVer < WORLDGEN_V1 || genVer > WORLDGEN_LATEST) return false;   // world from a newer generator
  uint64_t sd = r.u64();
  float t = r.f32(), hr = r.f32(); int dy = r.i32();
  if (r.bad) return false;
  newGame(sd, genVer);   // regenerates the exact world the save was made in
  worldChanged = ver >= 2 && fp != world.fingerprint();
  time = t; hour = hr; day = dy;
  bool ins = r.u8() != 0; int ss = r.i32(), sb = r.i32();
  Vec2 pp; pp.x = r.f32(); pp.y = r.f32();
  float hp = r.f32(); mp = r.f32(); stamina = r.f32();
  plLevel = r.i32(); plXp = r.i32(); gold = r.i32(); perkPts = r.i32();
  baseHp = r.f32(); maxMp = r.f32(); maxSt = r.f32();
  uint32_t n = r.u32();
  if (n > 5000) return false;
  inv.clear();
  for (uint32_t i = 0; i < n && !r.bad; i++) inv.push_back(readItem(r));
  int* eqs[] = {&eqWeapon, &eqBow, &eqStaff, &eqArmor, &eqHelmet, &eqShield, &eqRing, &eqAmulet};
  for (int* e : eqs) { *e = r.i32(); if (*e >= (int)inv.size()) *e = -1; }
  spellsKnown = r.u8(); spell = (Spell)(r.u8() % (int)Spell::COUNT);
  kills = r.i32(); dungeonsCleared = r.i32(); blessT = r.f32(); blessName = r.str(); lastTown = r.i32();
  n = r.u32();
  if (n > 5000) return false;
  quests.clear();
  for (uint32_t i = 0; i < n && !r.bad; i++) {
    Quest q;
    q.id = r.i32(); q.type = (QType)r.u8(); q.state = (QState)r.u8(); q.title = r.str(); q.desc = r.str(); q.giverName = r.str();
    q.giverSite = r.i32(); q.giverBldg = r.i32(); q.giverSlot = r.i32(); q.target = r.i32(); q.mon = (Monster)r.u8();
    q.need = r.i32(); q.have = r.i32(); q.gold = r.i32(); q.xp = r.i32(); q.stage = r.i32();
    quests.push_back(q);
  }
  nextQuestId = r.i32(); trackedQuest = r.i32();
  n = r.u32();
  npcQuestsDone.clear();
  for (uint32_t i = 0; i < n && !r.bad; i++) { uint64_t k = r.u64(); npcQuestsDone[k] = r.i32(); }
  n = r.u32();
  looted.clear();
  for (uint32_t i = 0; i < n && !r.bad; i++) looted.insert(r.u64());
  n = r.u32();
  killedSlots.clear();
  for (uint32_t i = 0; i < n && !r.bad; i++) {
    int k = r.i32(); uint32_t m = r.u32();
    for (uint32_t j = 0; j < m && !r.bad; j++) killedSlots[k].insert(r.i32());
  }
  n = r.u32();
  for (uint32_t i = 0; i < n && i < world.sites.size() && !r.bad; i++) {
    uint8_t f = r.u8();
    world.sites[i].discovered = f & 1; world.sites[i].cleared = (f & 2) != 0;
  }
  if (ver >= 3) {
    for (int* e : {&eqGloves, &eqBoots, &eqCloak}) { *e = r.i32(); if (*e >= (int)inv.size()) *e = -1; }
    uint8_t bg = r.u8();
    background = bg < (uint8_t)Background::COUNT ? (Background)bg : Background::None;
    storyFlags = r.u32();
    if (!readAppearance(r, app)) return false;
  }
  if (r.bad) return false;
  // re-apply looted overworld chests
  for (uint64_t k : looted)
    if ((k >> 32) == 0) {
      uint32_t i = (uint32_t)k;
      if (i < world.over.prop.size() && world.over.prop[i] == (int)Prop::Chest + 1) world.over.prop[i] = (int)Prop::ChestOpen + 1;
    }
  world.over.rebuildSolid();
  recalcPlayer();
  if (ins && sb >= 0 && sb < (int)world.over.bldgs.size()) enterBuilding(sb);
  else if (ins && ss >= 0 && ss < (int)world.sites.size()) enterSite(ss);
  pl().p = pp;
  pl().hp = hp;
  if (hp <= 0) respawn();   // saved on the death screen: wake in town rather than standing up with 1 HP where you fell
  events.clear();
  updateLocation();
  return true;
}
