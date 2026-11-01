// EMBERVALE RPG: shops: what a seller stocks, opening a shop, buying, selling, prices (SOLID 1.1).
// Public: Game::priceFactor, Game::shopStock, Game::openShop, Game::buy, Game::sell
#include "rpg/sim/game_rpg_detail.h"
#include "rpg/sim/game_internal.h"

float Game::priceFactor(Role seller) const {
  if (background == Background::Blacksmith && seller == Role::Smith) return 0.8f * SkillOps::buyMul(*this, seller);   // a smith's child pays a fair price
  if (background == Background::Urchin && seller == Role::Merchant) return 0.85f * SkillOps::buyMul(*this, seller);   // the streets know their own
  return SkillOps::buyMul(*this, seller);   // (M9) Speech
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
  HomeOps::shopStock(*this, a, s);   // M7: seeds by the culture's staples, farm tools, hay, saddlery (home_game.cpp)
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

