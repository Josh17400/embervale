// M7 "Home": the property's talk (HOMESTEAD lane, phase B). Options in [DLG_HOME, DLG_HOME_END):
//   - deeds (VISION_PLAN 8.1): a village's innkeeper, a town's or city's lord (the seat's keeper or steward) lists the
//     settlement's vacant houses (home::forSaleHouses) and lots (World::lots of that site) at their price x priceFactor;
//     buying gives the Plot and a deed; the weekly tax is paid here (arrears are reminded by homeStep); a deed the new
//     kingdom does not honour is re-registered here for 25 %;
//   - builders (8.2): a labourer or woodcutter offers to BUILD A HOUSE ON your lot (the Shell screen, Mode::Build);
//   - stablemasters (8.5): stablehands (and a steppe people's herders) sell their culture's horses; an innkeeper
//     stables the player's horse for the night and fetches it again;
//   - kitchens (15.2): an innkeeper teaches the culture's three signature recipes for a fee and lets the player cook
//     in the inn's kitchen (Station::InnKitchen);
//   - the chain (15.11): the miller (a windmill's or watermill's worker) grinds grain into flour for a small fee;
//   - the farmhand (8.4): a farmer or labourer of the plot's settlement works the farm for 10 gold a day (a week paid).
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/home.h"
#include "rpg/world/source.h"

namespace {
enum : int {
  H_LIST = DLG_HOME + 1,
  H_BUYHOUSE = DLG_HOME + 10, H_BUYLOT, H_TAX = DLG_HOME + 20, H_REREG,
  H_BUILD = DLG_HOME + 30,
  H_HORSES = DLG_HOME + 40, H_BUYHORSE, H_STABLE = DLG_HOME + 45, H_FETCH,
  H_RECIPES = DLG_HOME + 50, H_LEARN, H_COOK = DLG_HOME + 55,
  H_MILL = DLG_HOME + 60,
  H_HIRE = DLG_HOME + 70,
};

bool isGrain(const Item& it) { return it.kind == ItemKind::Misc && home::ingredientOf(it) == home::Ingr::Grain; }

life::Job jobOf(Game& g, const Actor& a, life::Census*& c, int& idx, int& site) {
  if (!HomeOps::residentOf(g, a, c, idx, site)) return life::Job::None;
  return c->res[(size_t)idx].job;
}
// the deed seller of a settlement: the innkeeper in a village, the lord's household (its master or steward) in a town
bool sellsDeeds(Game& g, const Actor& a, int si) {
  if (si < 0 || !g.world.sites[(size_t)si].settlement()) return false;
  const Site& s = g.world.sites[(size_t)si];
  if (s.type == SiteType::Village) return a.role == Role::Innkeeper;
  if (a.role == Role::Jarl) return true;
  life::Census* c = nullptr;
  int idx = -1, site = -1;
  const life::Job j = jobOf(g, a, c, idx, site);
  return (j == life::Job::Noble || j == life::Job::Servant) && c && c->res[(size_t)idx].work == c->seat && c->seat >= 0;
}
std::string dirTo(const Game& g, int32_t gx, int32_t gy) {
  int tx, ty;
  overworldTile(g, tx, ty);
  return dirWord(gx - (g.world.ox + tx), gy - (g.world.oy + ty), false);
}
const char* shellName(const Bldg& b) { return home::houseInfo(home::houseKindOf(b)).name; }
uint64_t cultureOfSite(const Game& g, int si) { return si >= 0 ? g.world.sites[(size_t)si].culture : 0; }
void done(Game& g, const std::string& text) {
  g.dlg.text = text;
  g.dlg.opts.clear();
  g.dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
}
// the lots and vacant houses a settlement sells now (not the player's already)
void forSale(const Game& g, int si, std::vector<int>& houses, std::vector<int>& lots) {
  houses.clear(); lots.clear();
  if (si < 0) return;
  for (int bi : home::forSaleHouses(g.world, si))
    if (g.home.plotById(g.world.over.bldgs[(size_t)bi].id) < 0) houses.push_back(bi);
  const ew::Gid sid = g.world.sites[(size_t)si].id;
  for (size_t i = 0; i < g.world.lots.size(); i++)
    if (g.world.lots[i].site == sid && g.home.plotById(g.world.lots[i].id) < 0) lots.push_back((int)i);
}
int stablePlot(const Game& g) {
  for (size_t i = 0; i < g.home.plots.size(); i++) {
    const home::Plot& p = g.home.plots[i];
    int room = 0, used = 0;
    for (const home::PlacedObj& o : p.outside) if ((home::Obj)o.kind == home::Obj::Stable) room += home::objInfo(home::Obj::Stable).capacity;
    for (const home::AnimalRec& r : p.animals) if ((home::Animal)r.kind == home::Animal::Horse) used++;
    if (used < room) return (int)i;
  }
  return -1;
}
// the player's horse is here with them (ridden, or waiting within 20 tiles)
bool horseHere(const Game& g) {
  if (g.home.riding >= 0) return true;
  if (g.home.horse < 0 || g.home.horseInn) return false;
  int tx, ty;
  overworldTile(g, tx, ty);
  return std::abs(g.home.horseGx - (g.world.ox + tx)) <= 20 && std::abs(g.home.horseGy - (g.world.oy + ty)) <= 20;
}
}  // namespace

void HomeOps::talk(Game& g, Actor& a) {
  if (a.critter || !a.human) return;
  const int si = siteOfActor(g, a);
  life::Census* c = nullptr;
  int idx = -1, rsite = -1;
  const life::Job job = jobOf(g, a, c, idx, rsite);
  // deeds and taxes
  if (sellsDeeds(g, a, si)) {
    std::vector<int> houses, lots;
    forSale(g, si, houses, lots);
    if (!houses.empty() || !lots.empty()) g.dlg.opts.push_back({"PROPERTY FOR SALE...", H_LIST, 0});
    const ew::Gid sid = g.world.sites[(size_t)si].id;
    for (size_t i = 0; i < g.home.plots.size(); i++) {
      const home::Plot& p = g.home.plots[i];
      if (p.site != sid) continue;
      if (p.flags & home::PF_REREG) g.dlg.opts.push_back({"RE-REGISTER MY DEED (" + std::to_string(home::reregFee(g, p)) + " GOLD)", H_REREG, (int)i});
      else if (const int owed = home::taxOwed(p, g.day); owed > 0) g.dlg.opts.push_back({"PAY MY TAXES (" + std::to_string(owed) + " GOLD)", H_TAX, (int)i});
    }
  }
  // a builder: a house on the player's bare lot
  if (job == life::Job::Labourer || job == life::Job::Woodcutter) {
    int lot = -1;
    for (size_t i = 0; i < g.home.plots.size() && lot < 0; i++)
      if (g.home.plots[i].kind == home::PlotKind::Lot && g.home.plots[i].state == home::PlotState::Owned) lot = (int)i;
    if (lot >= 0) g.dlg.opts.push_back({"BUILD A HOUSE ON MY PLOT", H_BUILD, lot});
  }
  // a stablemaster: a town's stablehand, a steppe people's herder
  bool steppe = false;
  if (const cult::Culture* cu = si >= 0 ? g.world.cultureOf(si) : nullptr) steppe = cu->archetype == cult::Archetype::Steppe;
  if (job == life::Job::Stablehand || (job == life::Job::Herder && steppe)) g.dlg.opts.push_back({"HORSES FOR SALE...", H_HORSES, 0});
  // the innkeeper: the stable, recipes, the kitchen
  if (a.role == Role::Innkeeper && si >= 0) {
    const ew::Gid sid = g.world.sites[(size_t)si].id;
    if (g.home.horse >= 0 && g.home.horseInn == sid) {
      const int keep = home::stableOwed(g);
      g.dlg.opts.push_back({keep ? "FETCH MY HORSE (" + std::to_string(keep) + " GOLD KEEP)" : std::string("FETCH MY HORSE"), H_FETCH, 0});
    }
    else if (horseHere(g)) g.dlg.opts.push_back({"STABLE MY HORSE (" + std::to_string(home::STABLE_INN_FEE) + " GOLD)", H_STABLE, 0});
    bool unknown = false;
    for (const home::Meal& m : home::cultureMeals(cultureOfSite(g, si), g))
      unknown |= std::find(g.home.recipes.begin(), g.home.recipes.end(), m.id) == g.home.recipes.end();
    if (unknown) g.dlg.opts.push_back({"TEACH ME A RECIPE...", H_RECIPES, 0});
    g.dlg.opts.push_back({"MAY I COOK IN YOUR KITCHEN?", H_COOK, 0});
  }
  // the miller
  bool miller = c && idx >= 0 && c->mill >= 0 && c->res[(size_t)idx].work == c->mill;
  if (a.bldg >= 0 && a.bldg < (int)g.world.over.bldgs.size()) {
    const art::Building t = g.world.over.bldgs[(size_t)a.bldg].type;
    miller |= t == art::Building::Windmill || t == art::Building::Watermill;
  }
  if (miller) {
    const int n = count(g, isGrain);
    if (n > 0) g.dlg.opts.push_back({"GRIND MY GRAIN (" + std::to_string(n) + " SACKS, " + std::to_string(n * home::MILL_FEE) + " GOLD)", H_MILL, n});
  }
  // a farmhand for hire
  if (c && idx >= 0 && (job == life::Job::Farmer || job == life::Job::Labourer) && c->res[(size_t)idx].age >= 16 &&
      !(c->res[(size_t)idx].flags & (life::RF_EMPLOYED | life::RF_DEAD | life::RF_AWAY))) {
    const ew::Gid sid = g.world.sites[(size_t)rsite].id;
    int best = -1, bestW = 0;   // the farm with the most to do (crops, then beasts) that has nobody working it
    for (size_t i = 0; i < g.home.plots.size(); i++) {
      const home::Plot& p = g.home.plots[i];
      if ((p.site != sid && p.site != 0) || (p.farmhandRes != 0xFFFF && (int)p.farmhandUntil >= g.day)) continue;
      const int w = 1 + (int)p.crops.size() * 2 + (int)p.animals.size();
      if (w > bestW) { bestW = w; best = (int)i; }
    }
    if (best >= 0) g.dlg.opts.push_back({"WORK MY FARM? (" + std::to_string(home::FARMHAND_WAGE * 7) + " GOLD A WEEK)", H_HIRE, best});
  }
}

bool HomeOps::choose(Game& g, const DlgOpt& o) {
  const int ai = g.findActor(g.dlg.actor);
  if (ai < 0) { g.mode = Mode::Play; return true; }
  Actor& a = g.actors[(size_t)ai];
  const int si = siteOfActor(g, a);
  std::string why;
  switch (o.action) {
    case H_LIST: {
      std::vector<int> houses, lots;
      forSale(g, si, houses, lots);
      g.dlg.opts.clear();
      for (int bi : houses) {
        const Bldg& b = g.world.over.bldgs[(size_t)bi];
        const int price = (int)std::lround(home::houseInfo(home::houseKindOf(b)).price * home::priceFactor(g, si));
        g.dlg.opts.push_back({std::string("BUY THE ") + shellName(b) + " TO THE " + dirTo(g, g.world.ox + b.doorX(), g.world.oy + b.doorY()) + " (" +
                                  std::to_string(price) + " GOLD)", H_BUYHOUSE, bi});
      }
      for (int li : lots) {
        const ew::PlotPlan& L = g.world.lots[(size_t)li];
        const home::LotInfo& inf = home::lotInfo((home::LotSize)std::min<int>(L.size, 2));
        const int price = (int)std::lround(inf.price * home::priceFactor(g, si));
        g.dlg.opts.push_back({std::string("BUY A ") + inf.name + ((L.flags & ew::PLOT_RIVERSIDE) ? " BY THE WATER" : "") + " TO THE " +
                                  dirTo(g, L.gx + L.w / 2, L.gy + L.h / 2) + " (" + std::to_string(price) + " GOLD)", H_BUYLOT, li});
      }
      g.dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
      g.dlg.text = "WHAT " + g.world.sites[(size_t)si].name + " HAS FOR SALE. THE FIRST WEEK'S TAX IS IN THE PRICE.";
      return true;
    }
    case H_BUYHOUSE:
      if (!home::buyHouse(g, o.arg, why)) done(g, "NOT TODAY, FRIEND. " + why + ".");
      else { done(g, "SIGNED AND SEALED: THE HOUSE IS YOURS. HERE IS THE DEED. THE TAX IS DUE EACH WEEK, HERE."); sfx(g, (int)Sfx::Coin, g.pl().p); }
      return true;
    case H_BUYLOT:
      if (!home::buyLot(g, o.arg, why)) done(g, "NOT TODAY, FRIEND. " + why + ".");
      else { done(g, "THE LAND IS YOURS: HERE IS THE DEED. ANY LABOURER IN TOWN WILL BUILD ON IT FOR YOU."); sfx(g, (int)Sfx::Coin, g.pl().p); }
      return true;
    case H_TAX: {
      if (o.arg < 0 || o.arg >= (int)g.home.plots.size()) return true;
      home::Plot& p = g.home.plots[(size_t)o.arg];
      const int owed = home::taxOwed(p, g.day);
      if (g.gold < owed) { done(g, "YOU OWE " + std::to_string(owed) + " GOLD. COME BACK WHEN YOU HAVE IT."); return true; }
      g.gold -= owed;
      const int weeks = owed / std::max(1, home::plotTaxWeek(p));
      p.paidUntilDay = (uint16_t)(p.paidUntilDay + weeks * 7);
      done(g, "PAID UP UNTIL DAY " + std::to_string(p.paidUntilDay) + ". THE REGISTER THANKS YOU.");
      sfx(g, (int)Sfx::Coin, g.pl().p);
      return true;
    }
    case H_REREG: {
      if (o.arg < 0 || o.arg >= (int)g.home.plots.size()) return true;
      home::Plot& p = g.home.plots[(size_t)o.arg];
      const int fee = home::reregFee(g, p);
      if (g.gold < fee) { done(g, "THE NEW REGISTER WANTS " + std::to_string(fee) + " GOLD FOR YOUR DEED."); return true; }
      g.gold -= fee;
      p.flags &= (uint8_t)~home::PF_REREG;
      const Site& s = g.world.sites[(size_t)si];
      const realm::SettlementState* st = g.realm.settlement(s.id);
      p.realm = st ? st->owner : (s.kingdom >= 0 && s.kingdom < (int)g.world.kingdoms.size() ? g.world.kingdoms[(size_t)s.kingdom].id : 0);
      p.paidUntilDay = (uint16_t)std::max<int>(p.paidUntilDay, g.day + 7);
      done(g, "YOUR DEED IS IN THE NEW REGISTER. NOBODY WILL TROUBLE YOUR HOUSE NOW.");
      sfx(g, (int)Sfx::Coin, g.pl().p);
      return true;
    }
    case H_BUILD: {
      if (o.arg < 0 || o.arg >= (int)g.home.plots.size()) return true;
      const home::Plot& p = g.home.plots[(size_t)o.arg];
      home::Ui& U = g.home.ui;
      U = home::Ui();
      U.mode = home::UiMode::Shell;
      U.plot = o.arg;
      U.builder = a.id;
      // the biggest shell the lot takes; the style: this settlement's people if seen, else the first known
      U.sel = 0;
      for (int s = 0; s < (int)home::Shell::COUNT; s++) {
        const home::ShellInfo& sh = home::shellInfo((home::Shell)s);
        if (home::shellFits(p, (home::Shell)s) && sh.storeys == 1) U.sel = s;
      }
      const uint64_t cu = cultureOfSite(g, si);
      U.style = cu && g.home.knowsStyle(cu) ? cu : (g.home.styles.empty() ? 0 : g.home.styles.front());
      home::haveMaterials(g, U.timber, U.iron);
      const home::ShellInfo& sh = home::shellInfo((home::Shell)U.sel);
      U.timber = std::min(U.timber, (int)sh.timber);
      U.iron = std::min(U.iron, (int)sh.iron);
      U.gx = p.w / 2; U.gy = 2;
      g.mode = Mode::Build;
      return true;
    }
    case H_HORSES: {
      g.dlg.opts.clear();
      const home::Breed own = home::breedOfCulture(cultureOfSite(g, si), g);
      std::vector<home::Breed> breeds{own};
      if (own != home::Breed::Heartland) breeds.push_back(home::Breed::Heartland);
      // (M7 fix r3, review: "the stablemaster shows horse prices, then refuses") a horse is sold only to a player with a
      // stable with room at home: without one the prices are told, and the rule, before anything is picked
      const bool room = home::horseRoomPlot(g) >= 0;
      if (room)
        for (home::Breed b : breeds)
          g.dlg.opts.push_back({std::string("BUY A ") + home::breedInfo(b).name + " (" + std::to_string(home::breedInfo(b).price) + " GOLD)", H_BUYHORSE, (int)b});
      g.dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
      g.dlg.text = std::string("GOOD BEASTS, ALL OF THEM. THE ") + home::breedInfo(own).name + " IS BRED HERE" +
                   (home::breedInfo(own).sure ? ": SURE-FOOTED ON ANY SLOPE." : ".");
      if (!room) {
        std::string prices;
        for (size_t k = 0; k < breeds.size(); k++)
          prices += std::string(k ? ", " : "") + home::breedInfo(breeds[k]).name + " " + std::to_string(home::breedInfo(breeds[k]).price);
        g.dlg.text += " BUT I SELL ONLY TO A RIDER WITH A STABLE AT HOME THAT HAS ROOM FOR HER: BUILD ONE IN YOUR YARD, THEN COME BACK (" + prices + " GOLD).";
      }
      return true;
    }
    case H_BUYHORSE:
      if (!home::buyHorse(g, (home::Breed)o.arg, si, why)) done(g, why + ".");
      else done(g, "SHE'S YOURS. SHE'S WAITING OUTSIDE: TREAT HER KINDLY AND SHE'LL CARRY YOU ANYWHERE THE ROADS GO.");
      return true;
    case H_STABLE:
    case H_FETCH:
      if (!home::stableAtInn(g, si, why)) done(g, why + ".");
      else done(g, o.action == H_STABLE ? "THE STABLE BOY WILL SEE TO YOUR HORSE. ASK FOR HER WHEN YOU LEAVE." : "HERE SHE IS, FED AND BRUSHED. SHE'S AT THE DOOR.");
      return true;
    case H_RECIPES: {
      g.dlg.opts.clear();
      int k = 0;
      for (const home::Meal& m : home::cultureMeals(cultureOfSite(g, si), g)) {
        k++;
        if (std::find(g.home.recipes.begin(), g.home.recipes.end(), m.id) != g.home.recipes.end()) continue;
        g.dlg.opts.push_back({"LEARN " + m.name + " (" + std::to_string(20 + 20 * k) + " GOLD)", H_LEARN, k});
      }
      g.dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
      g.dlg.text = "MY MOTHER'S RECIPES, AND HERS BEFORE HER. COOKED AT A GOOD HEARTH THEY'LL KEEP YOU ON YOUR FEET ALL DAY.";
      return true;
    }
    case H_LEARN: {
      const std::vector<home::Meal> ms = home::cultureMeals(cultureOfSite(g, si), g);
      if (o.arg < 1 || o.arg > (int)ms.size()) return true;
      const home::Meal& m = ms[(size_t)o.arg - 1];
      const int price = 20 + 20 * o.arg;
      if (g.gold < price) { done(g, "THAT ONE'S WORTH " + std::to_string(price) + " GOLD TO ME."); return true; }
      g.gold -= price;
      g.home.recipes.push_back(m.id);
      std::string ing;
      static const char* nm[] = {"", "GRAIN", "FLOUR", "BREAD", "VEGETABLES", "FRUIT", "MEAT", "FISH", "EGGS", "MILK", "CHEESE", "HERBS", "HONEY"};
      for (home::Ingr i : m.in) if (i != home::Ingr::None) ing += (ing.empty() ? "" : ", ") + std::string(nm[(int)i]);
      done(g, m.name + ": " + ing + ". DON'T RUSH IT.");
      sfx(g, (int)Sfx::Coin, g.pl().p);
      return true;
    }
    case H_COOK:
      g.home.ui = home::Ui();
      g.home.ui.mode = home::UiMode::Cook;
      g.home.ui.station = home::Station::InnKitchen;
      g.mode = Mode::Build;
      return true;
    case H_MILL: {
      const int n = std::min(o.arg, count(g, isGrain));
      if (n <= 0) { done(g, "YOU'VE NO GRAIN FOR ME."); return true; }
      if (g.gold < n * home::MILL_FEE) { done(g, "A COIN A SACK, FRIEND."); return true; }
      g.gold -= n * home::MILL_FEE;
      take(g, n, isGrain);
      give(g, home::makeProduct(home::Product::Flour, n));
      done(g, std::to_string(n) + " SACKS OF FLOUR, FINE AS YOU'LL FIND. A BAKER'S OVEN OR YOUR OWN HEARTH WILL MAKE BREAD OF IT.");
      return true;
    }
    case H_HIRE: {
      if (o.arg < 0 || o.arg >= (int)g.home.plots.size()) return true;
      life::Census* c = nullptr;
      int idx = -1, site = -1;
      if (!residentOf(g, a, c, idx, site)) return true;
      if (!home::hireFarmhand(g, o.arg, 7, why)) { done(g, why + "."); return true; }
      home::Plot& p = g.home.plots[(size_t)o.arg];
      p.farmhand = (uint32_t)life::npcId(g.world.sites[(size_t)site].id, idx);
      if (!p.farmhand) p.farmhand = 1;
      p.farmhandRes = (uint16_t)idx;
      g.life.employ(g.world.sites[(size_t)site].id, idx, g.day);
      done(g, "A WEEK'S WORK? GLADLY. I'LL WATER AND BRING IN WHAT'S RIPE, AND PUT IT BY FOR YOU.");
      sfx(g, (int)Sfx::Coin, g.pl().p);
      return true;
    }
    default: return false;
  }
}
