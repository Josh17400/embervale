// M7 "Home": the Game hooks and the actions of rpg/sim/home.h. HOMESTEAD lane.
//
// Phase A (lead, 2026-10-09) wrote first cuts; phase B (HOMESTEAD lane) makes them the game's:
//   - riding: the breed's pace (+15 % on roads, slower on ramps unless sure-footed), the dismount triggers (the horse
//     stays where the rider got off: home_actors.cpp), the last dry tile (a rider pitched off in water leaves the horse
//     on the bank);
//   - homeStep: discovered styles, the day's catch-up of every plot (the live night raid when the player is near),
//     construction finishing, exposure to predators, taxes and arrears reminders, deeds when a settlement changes
//     hands, lost animals' news, the window stamp, the home actors (home_actors.cpp);
//   - stampWindow: owned plots cleared of generated props, solid yard objects as Filler, the house (or its building
//     site) as a Bldg in World::over, FOR SALE signs by the doors of vacant houses;
//   - the actions: buying (with a deed), building (materials and wages x the settlement's prices), placing / removing
//     (world-solid ground refused), tilling, planting (rice beside water), watering, harvesting (yield by quality),
//     animals, products (quality by happiness), hay, the farmhand, cooking, the stores, mounting, horses;
//   - homeInteract and interactLabel share one decision (useAt), so the prompt always says what the button does.
// The dialogue (deeds, taxes, builders, stables, recipes, the miller, the farmhand) is home_talk.cpp; the actors
// (animals, the waiting horse, builders, the farmhand, wolves) home_actors.cpp; the player's house home_interior.cpp.
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/craft.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/home.h"
#include "rpg/world/source.h"

// ---------------------------------------------------------------- HomeOps: shared helpers
void HomeOps::dropAt(Game& g, int i) {
  if (i < 0 || i >= (int)g.inv.size()) return;
  int* eqs[] = {&g.eqWeapon, &g.eqBow, &g.eqStaff, &g.eqArmor, &g.eqHelmet, &g.eqShield, &g.eqRing, &g.eqAmulet, &g.eqGloves, &g.eqBoots, &g.eqCloak};
  bool worn = false;
  for (int* e : eqs) { if (*e == i) { *e = -1; worn = true; } else if (*e > i) (*e)--; }
  g.inv.erase(g.inv.begin() + i);
  // (M7 fixer r2) worn gear put into a store (or used up) comes off: the look, armour and stats follow at once
  if (worn) g.recalcPlayer();
}
bool HomeOps::hasTool(const Game& g, home::Tool t) {
  for (const Item& it : g.inv) if (home::isTool(it, t)) return true;
  return false;
}
int HomeOps::settlementOf(const Game& g, int32_t gx, int32_t gy) {
  const int s = g.world.siteAt(gx - g.world.ox, gy - g.world.oy, 24);
  return s >= 0 && g.world.sites[(size_t)s].settlement() ? s : -1;
}
int HomeOps::siteOfActor(const Game& g, const Actor& a) {
  if (a.site >= 0 && a.site < (int)g.world.sites.size()) return a.site;
  if (a.bldg >= 0 && a.bldg < (int)g.world.over.bldgs.size()) return g.world.over.bldgs[(size_t)a.bldg].site;
  if (g.inside && g.subBldg >= 0 && g.subBldg < (int)g.world.over.bldgs.size()) return g.world.over.bldgs[(size_t)g.subBldg].site;
  return -1;
}
bool HomeOps::residentOf(Game& g, const Actor& a, life::Census*& c, int& idx, int& site) {
  c = nullptr; idx = -1;
  site = siteOfActor(g, a);
  if (site < 0 || !g.world.sites[(size_t)site].settlement()) return false;
  c = g.life.census(g.world, site);
  if (!c || a.resident < 0 || a.resident >= (int)c->res.size()) return false;
  idx = a.resident;
  return true;
}
void HomeOps::travelHomeOnRespawn(Game& g, int32_t gx, int32_t gy) {
  g.travel = Travel();
  g.travel.phase = TravelPhase::Gather;
  g.travel.site = -1;
  g.travel.kind = 2;
  g.travel.gx = gx; g.travel.gy = gy;
  g.journeyWindow();
  g.travel.t = kTravelFadeOut;
  g.sleepFade = 1.0f;
}

namespace {
// the kingdom that holds a settlement now (0: none, the wildlands)
ew::Gid holderOf(const Game& g, int si) {
  if (si < 0 || si >= (int)g.world.sites.size()) return 0;
  const Site& s = g.world.sites[(size_t)si];
  if (const realm::SettlementState* st = g.realm.settlement(s.id)) return st->owner;
  return s.kingdom >= 0 && s.kingdom < (int)g.world.kingdoms.size() ? g.world.kingdoms[(size_t)s.kingdom].id : 0;
}
std::string kingdomNameOf(const Game& g, ew::Gid k) {
  for (const Kingdom& K : g.world.kingdoms) if (K.id == k) return K.name;
  return "THE NEW LORDS";
}
// the deed item of a plot
Item makeDeed(const Game& g, int plot) {
  Item it;
  it.kind = ItemKind::Misc;
  it.sub = home::MISC_DEED;
  it.name = "DEED: " + home::propertyName(g, g.home.plots[(size_t)plot]);
  it.icon = art::Icon::Deed;
  it.value = 0;
  it.seed = (uint32_t)plot;
  return it;
}
// a fence (or gate) on every border tile of the plot
bool fenceRing(const home::Plot& p) {
  std::vector<uint8_t> f((size_t)p.w * p.h, 0);
  for (const home::PlacedObj& o : p.outside)
    if ((home::Obj)o.kind == home::Obj::Fence || (home::Obj)o.kind == home::Obj::Gate)
      if (o.x < p.w && o.y < p.h) f[(size_t)o.y * p.w + o.x] = 1;
  for (int x = 0; x < p.w; x++) if (!f[(size_t)x] || !f[(size_t)(p.h - 1) * p.w + x]) return false;
  for (int y = 0; y < p.h; y++) if (!f[(size_t)y * p.w] || !f[(size_t)y * p.w + p.w - 1]) return false;
  return true;
}
// exposed to predators (VISION_PLAN 8.5): no fence ring, and a den within 60 tiles or the wildlands
bool exposed(const Game& g, const home::Plot& p) {
  if (fenceRing(p)) return false;
  const int si = g.world.siteHandle(p.site);
  if (si < 0 || holderOf(g, si) == 0) return true;
  const int32_t cx = p.gx + p.w / 2 - g.world.ox, cy = p.gy + p.h / 2 - g.world.oy;
  for (const Den& d : g.world.dens)
    if ((int64_t)(d.x - cx) * (d.x - cx) + (int64_t)(d.y - cy) * (d.y - cy) <= 60 * 60) return true;
  return false;
}
// is a window tile ground a yard can use? (no water, cliff, wall, generated building, rock)
bool worldFree(const Game& g, const home::Plot& p, int x, int y, std::string& why) {
  const Map& M = g.world.over;
  const int lx = p.gx + x - g.world.ox, ly = p.gy + y - g.world.oy;
  if (!M.in(lx, ly)) { why = "THAT IS TOO FAR AWAY"; return false; }
  const size_t i = (size_t)ly * M.w + lx;
  const Ground gr = M.at(lx, ly);
  if (groundWater(gr)) { why = "THAT IS WATER"; return false; }
  if (groundSolid(gr) || M.wall[i] || (!M.height.empty() && (M.height[i] & Map::HEIGHT_CLIFF))) { why = "THE GROUND THERE IS NOT FIT"; return false; }
  if (M.bldgAt[i] >= 0) {
    const Bldg& b = M.bldgs[(size_t)M.bldgAt[i]];
    const bool ownHouse = b.id == p.id || (p.kind == home::PlotKind::Lot && p.hw && b.id == home::houseBldg(p, g).id);
    if (!ownHouse) { why = "A BUILDING STANDS THERE"; return false; }
  }
  return true;
}
}  // namespace

// ---------------------------------------------------------------- riding
float Game::homeSpeedMul() const {
  if (home.riding < 0) return 1.0f;
  const home::BreedInfo& b = home::breedInfo((home::Breed)home.ridingBreed);
  float mul = b.speed * TILE / home::WALK_SPEED_PX;
  const Actor& p = pl();
  const int tx = (int)std::floor(p.p.x / TILE), ty = (int)std::floor((p.p.y - 2) / TILE);
  const Ground gr = map().at(tx, ty);
  if (gr == Ground::Road || gr == Ground::Plaza || gr == Ground::Bridge) mul *= 1.0f + home::RIDE_ROAD_BONUS;
  // (phase B) a horse that is not sure-footed picks its way down a ramp
  const Map& m = map();
  if (!b.sure && m.in(tx, ty) && !m.height.empty() && (m.height[(size_t)ty * m.w + tx] & Map::HEIGHT_RAMP)) mul *= 0.6f;
  return mul;
}

void Game::homeDismount(home::Dismount why) {
  if (home.riding < 0) return;
  if ((int)why < (int)home::Dismount::COUNT) home.dismounts[(int)why]++;
  HomeOps::dismounted(*this, why);   // the horse stays where the rider got off (home_actors.cpp)
  home.riding = -1;
  if (why == home::Dismount::Player) say("YOU DISMOUNT");
  else if (why == home::Dismount::Water) say("THE HORSE WILL NOT WADE INTO THE WET: YOU DISMOUNT");
}

bool Game::yardBarred(const Actor& a, int tx, int ty) const {
  if (home.plots.empty() || inside || a.player || !a.npc || a.hostile || (a.lifeBits & LB_HOMESTEAD)) return false;
  const int pi = home.plotAt(world.ox + tx, world.oy + ty);
  if (pi < 0) return false;
  const int ax = (int)std::floor(a.p.x / TILE), ay = (int)std::floor((a.p.y - 2) / TILE);
  return home.plotAt(world.ox + ax, world.oy + ay) != pi;
}

// ---------------------------------------------------------------- the step
void Game::homeStep(float dt) {
  // discovered styles: the homeland's, then every settlement the player stands in
  if (app.homeland && !home.knowsStyle(app.homeland)) home.styles.push_back(app.homeland);
  if (curSite >= 0 && curSite < (int)world.sites.size()) {
    const Site& s = world.sites[(size_t)curSite];
    if (s.settlement() && s.culture && !home.knowsStyle(s.culture)) home.styles.push_back(s.culture);
  }
  int32_t px, py;
  {
    int tx, ty;
    overworldTile(*this, tx, ty);
    px = world.ox + tx; py = world.oy + ty;
  }
  bool changed = false;
  for (size_t pi = 0; pi < home.plots.size(); pi++) {
    home::Plot& p = home.plots[pi];
    const bool near = !inside && std::abs(p.gx + p.w / 2 - px) < 60 && std::abs(p.gy + p.h / 2 - py) < 60;
    // the day's catch-up: near the player, the night's raid is a live one (wolves at the pen) instead of the roll
    while ((int)p.lastSeenDay < day) {
      const int d = (int)p.lastSeenDay + 1;
      if (near && day - (int)p.lastSeenDay <= 2) {
        const uint8_t keep = p.flags;
        p.flags &= (uint8_t)~home::PF_EXPOSED;
        home::catchUp(p, d - 1, d, seed);
        p.flags = (uint8_t)((p.flags & ~home::PF_EXPOSED) | (keep & home::PF_EXPOSED));
        if (d == day && home::raidRoll(p, d, seed)) HomeOps::dayTurned(*this, (int)pi, d);
      } else {
        home::catchUp(p, p.lastSeenDay, day, seed);
      }
    }
    if (p.state == home::PlotState::Building && day >= (int)p.buildDoneDay) {
      p.state = home::PlotState::Built;
      changed = true;
      emit(Ev::QuestUpdate, pl().p, 0, 1, "YOUR HOUSE IS FINISHED");
    }
    // the plot's exposure (while its land is in the window: a fence ring closed, a den cleared)
    if (near) {
      const bool ex = exposed(*this, p);
      p.flags = (uint8_t)(ex ? (p.flags | home::PF_EXPOSED) : (p.flags & ~home::PF_EXPOSED));
    }
    // the deed when the settlement changed hands (8.1: honoured at reputation >= 0, else the 25 % re-registration)
    const int si = world.siteHandle(p.site);
    if (si >= 0) {
      const ew::Gid k = holderOf(*this, si);
      if (!p.realm) p.realm = k;
      else if (k && k != p.realm && !(p.flags & home::PF_REREG)) {
        if (realm.rep(k) >= 0) {
          p.realm = k;
          HomeOps::news(*this, kingdomNameOf(*this, k) + " HONOUR YOUR DEED TO " + home::propertyName(*this, p));
        } else {
          p.flags |= home::PF_REREG;
          HomeOps::news(*this, kingdomNameOf(*this, k) + " DO NOT KNOW YOUR DEED TO " + home::propertyName(*this, p) + ": RE-REGISTER IT (" +
                               std::to_string(home::reregFee(*this, p)) + " GOLD)");
        }
      }
    }
    // taxes: a reminder every third day while they are owed
    const int owed = home::taxOwed(p, day);
    if (owed > 0 && day - (int)p.taxNoticeDay >= 3 && hour >= 8.0f) {
      p.taxNoticeDay = (uint16_t)day;
      const std::string town = si >= 0 ? world.sites[(size_t)si].name : std::string("THE TOWN");
      const bool lien = owed >= home::LIEN_WEEKS * std::max(1, home::plotTaxWeek(p));
      HomeOps::notice(*this, std::string(lien ? "LIEN FOR UNPAID TAX ON " : "TAX OWED ON ") + home::propertyName(*this, p) + ": " + std::to_string(owed) +
                                 " GOLD (PAY IN " + town + ")", lien ? rgba(240, 120, 100) : rgba(230, 190, 110));
    }
    // a lost animal: the news, then (a day later) it is gone from the record
    for (size_t k = 0; k < p.animals.size(); k++) {
      if (!(p.animals[k].flags & home::AF_LOST)) continue;
      if (!(p.flags & home::PF_RAIDED)) {
        p.flags |= home::PF_RAIDED;
        const home::AnimalRec& a = p.animals[k];
        HomeOps::news(*this, "WOLVES TOOK " + home::animalName(a) + " THE " + home::animalInfo((home::Animal)a.kind).name + " FROM " + home::propertyName(*this, p));
      }
    }
    if (day > (int)p.raidDay + 1)
      for (size_t k = p.animals.size(); k-- > 0;)
        if (p.animals[k].flags & home::AF_LOST) {
          p.animals.erase(p.animals.begin() + (std::ptrdiff_t)k);
          const int id = (int)pi * 64 + (int)k;
          auto fix = [&](int& r) { if (r == id) r = -1; else if (r > id && r / 64 == (int)pi) r--; };
          fix(home.riding);
          fix(home.horse);
          home::updateDogFlag(p);
          changed = true;
        }
  }
  if (changed) HomeOps::restamp(*this);
  if (homeSerial_ != world.placeSerial) {
    homeSerial_ = world.placeSerial;
    home::stampWindow(*this);
    // (M7 fixer r2) a house, its building site or a solid yard object just stamped over someone (the player built
    // where they stood, a coop's ghost covered their tile, an animal or a villager stood there): step them out onto
    // the nearest free tile (in front of the door when they were on the house), never left trapped in it
    if (!inside)
      for (Actor& a : actors) {
        if (a.hp <= 0 || a.flying || a.fly || bodyFree(a.p, a.radius, false)) continue;
        const int tx = (int)std::floor(a.p.x / TILE), ty = (int)std::floor((a.p.y - 2) / TILE);
        const int pi = home.plotAt(world.ox + tx, world.oy + ty);
        if (pi < 0) continue;
        const home::Plot& P = home.plots[(size_t)pi];
        int sx = tx, sy = ty;
        const int hx = P.gx + P.hx - world.ox, hy = P.gy + P.hy - world.oy;
        if (P.hw && tx >= hx && tx < hx + P.hw && ty >= hy && ty < hy + P.hh) {
          int dx, dy;
          home::doorOf(P, dx, dy);
          sx = P.gx + dx - world.ox; sy = P.gy + dy + 1 - world.oy;
        }
        a.p = freeSpot(sx, sy);
        a.vel = Vec2();
        a.knock = Vec2();
      }
  }
  // riding: off in water (the horse stays on the bank), off indoors (enterBuilding / enterSite already dismount)
  if (home.riding >= 0) {
    if (inside) homeDismount(home::Dismount::Enter);
    else {
      const Actor& p = pl();
      const int tx = (int)std::floor(p.p.x / TILE), ty = (int)std::floor((p.p.y - 2) / TILE);
      const Ground gr = map().at(tx, ty);
      // water is never walked; the wet ground a horse will not wade is the bog (Ground::Swamp) and a ford's shallows
      if (groundWater(gr) || gr == Ground::Swamp) homeDismount(home::Dismount::Water);
      else { homeDryX_ = world.ox + tx; homeDryY_ = world.oy + ty; }
    }
  }
  HomeOps::actorsStep(*this, dt);   // animals, the waiting horse, builders, the farmhand, wolves (home_actors.cpp)
}

// ---------------------------------------------------------------- the yard in front of the player
namespace {
enum UseKind { U_NONE, U_HARVEST, U_WATER, U_CHECK, U_PLANT, U_TILL, U_COLLECT, U_HONEY, U_RIDE, U_CRATE, U_COOK, U_TROUGH };
struct Use {
  UseKind kind = U_NONE;
  int plot = -1, x = 0, y = 0, arg = 0;
  std::string label;
};
const char* productWord(home::Product p) {
  switch (p) {
    case home::Product::Egg: return "EGGS";
    case home::Product::Milk: return "MILK";
    case home::Product::Wool: return "WOOL";
    case home::Product::Truffle: return "TRUFFLES";
    default: return "PRODUCE";
  }
}
// the object of the yard at plot tile (x, y) (index into outside; -1 none)
int objAt(const home::Plot& p, int x, int y) {
  for (size_t i = 0; i < p.outside.size(); i++) {
    const home::PlacedObj& o = p.outside[i];
    const home::ObjInfo& oi = home::objInfo((home::Obj)o.kind);
    int w = oi.w, h = oi.h;
    if (o.turned() && (oi.flags & home::OBJ_ROTATES)) std::swap(w, h);
    if (x >= o.x && y >= o.y && x < o.x + w && y < o.y + h) return (int)i;
  }
  return -1;
}
Use useAt(const Game& g) {
  Use u;
  if (g.inside || g.home.plots.empty()) return u;
  const Actor& pa = g.pl();
  const Vec2 probe = pa.p + pa.aim * 12.0f + Vec2(0, -4);
  const int32_t gx = g.world.ox + (int)std::floor(probe.x / TILE), gy = g.world.oy + (int)std::floor(probe.y / TILE);
  const int pi = g.home.plotAt(gx, gy);
  if (pi < 0) return u;
  const home::Plot& P = g.home.plots[(size_t)pi];
  u.plot = pi;
  u.x = gx - P.gx; u.y = gy - P.gy;
  const int x = u.x, y = u.y;
  for (const home::CropRec& c : P.crops)
    if (c.x == x && c.y == y) {
      const char* nm = home::cropInfo((home::Crop)c.kind).name;
      if (c.stage >= 3) { u.kind = U_HARVEST; u.label = std::string("HARVEST ") + nm; }
      else if (g.day - (int)c.lastWaterDay >= 1 && HomeOps::hasTool(g, home::Tool::WateringCan)) { u.kind = U_WATER; u.label = "WATER"; }
      else { u.kind = U_CHECK; u.label = std::string("LOOK AT THE ") + nm; }
      return u;
    }
  const int oi = objAt(P, x, y);
  if (oi >= 0) {
    const home::PlacedObj& o = P.outside[(size_t)oi];
    const home::Obj k = (home::Obj)o.kind;
    if (k == home::Obj::Coop || k == home::Obj::Pen) {
      for (const home::AnimalRec& a : P.animals) {
        const home::AnimalInfo& ai = home::animalInfo((home::Animal)a.kind);
        if (ai.home == k && a.produce && !(a.flags & home::AF_LOST)) {
          u.kind = U_COLLECT; u.arg = (int)k; u.label = std::string("COLLECT ") + productWord((home::Product)ai.product);
          return u;
        }
      }
    }
    if (k == home::Obj::Beehive && home::honeyOf(o) > 0) { u.kind = U_HONEY; u.arg = oi; u.label = "COLLECT HONEY"; return u; }
    if (k == home::Obj::Stable && g.home.riding < 0) {
      for (size_t i = 0; i < P.animals.size(); i++)
        if ((home::Animal)P.animals[i].kind == home::Animal::Horse && g.home.horse != pi * 64 + (int)i) {
          u.kind = U_RIDE; u.arg = (int)i; u.label = "RIDE " + home::animalName(P.animals[i]);
          return u;
        }
    }
    if (k == home::Obj::ShippingCrate) { u.kind = U_CRATE; u.arg = (int)o.data; u.label = "OPEN THE CRATE"; return u; }
    if (k == home::Obj::Campfire) { u.kind = U_COOK; u.label = "COOK"; return u; }
    if ((k == home::Obj::Trough || k == home::Obj::HayRack) && P.trough < 7) {
      const int hay = HomeOps::count(g, [](const Item& it) { return it.kind == ItemKind::Misc && it.sub == home::MISC_PRODUCT + (int)home::Product::Hay; });
      bool eaters = false;
      for (const home::AnimalRec& a : P.animals) eaters |= !(a.flags & home::AF_LOST) && home::animalInfo((home::Animal)a.kind).hay > 0;
      if (hay > 0 && eaters) { u.kind = U_TROUGH; u.label = "FILL THE TROUGHS"; return u; }
    }
    return u;
  }
  const int gk = home::groundAt(P, x, y);
  if (gk == 2 || gk == 3) {
    for (const Item& it : g.inv) {
      home::Crop c;
      if (home::isSeed(it, c)) { u.kind = U_PLANT; u.arg = (int)c; u.label = std::string("PLANT ") + home::cropInfo(c).name; return u; }
    }
    return u;
  }
  if (gk == 0 && HomeOps::hasTool(g, home::Tool::Hoe)) {
    const int hx = P.hx, hy = P.hy;
    const bool house = P.hw && x >= hx && y >= hy && x < hx + P.hw && y < hy + P.hh;
    if (!house) { u.kind = U_TILL; u.label = "TILL"; }
  }
  return u;
}
}  // namespace

bool Game::homeInteract() {
  const Use u = useAt(*this);
  if (u.kind == U_NONE) return false;
  std::string why;
  home::Plot& P = home.plots[(size_t)u.plot];
  bool ok = true;
  switch (u.kind) {
    case U_HARVEST: ok = home::harvest(*this, u.plot, u.x, u.y, why); break;
    case U_WATER: ok = home::water(*this, u.plot, u.x, u.y, why); if (ok) sfx((int)Sfx::Pickup, pl().p, 1.4f, 0.5f); break;
    case U_CHECK:
      for (const home::CropRec& c : P.crops)
        if (c.x == u.x && c.y == u.y) {
          static const char* st[] = {"A SEEDLING", "GROWING", "RIPENING", "RIPE"};
          say(std::string(home::cropInfo((home::Crop)c.kind).name) + ": " + (c.wilted(day) ? "WILTING, IT NEEDS WATER" : st[std::min<int>(c.stage, 3)]));
        }
      return true;
    case U_PLANT: ok = home::plant(*this, u.plot, u.x, u.y, (home::Crop)u.arg, why); break;
    case U_TILL: ok = home::till(*this, u.plot, u.x, u.y, why); break;
    case U_COLLECT: {
      int got = 0;
      for (home::AnimalRec& a : P.animals) {
        const home::AnimalInfo& ai = home::animalInfo((home::Animal)a.kind);
        if ((int)ai.home != u.arg || !a.produce || (a.flags & home::AF_LOST)) continue;
        Item it = home::makeProduct((home::Product)ai.product, a.produce);
        if (a.happiness >= 80) { it.tier = 2; it.name = "FINE " + it.name; it.value += it.value / 2; }
        else if (a.happiness >= 50) it.tier = 1;
        HomeOps::give(*this, it);
        got += a.produce;
        a.produce = 0;
      }
      ok = got > 0;
      if (!ok) why = "NOTHING TO COLLECT YET";
      break;
    }
    case U_HONEY: {
      home::PlacedObj& o = P.outside[(size_t)u.arg];
      HomeOps::give(*this, home::makeProduct(home::Product::Honey, home::honeyOf(o)));
      o.data &= ~0xFFu;
      break;
    }
    case U_RIDE: ok = home::mount(*this, u.plot, u.arg, why); break;
    case U_CRATE:
      home.ui = home::Ui();
      home.ui.mode = home::UiMode::Storage; home.ui.plot = u.plot; home.ui.store = u.arg;
      mode = Mode::Build;
      return true;
    case U_COOK:
      home.ui = home::Ui();
      home.ui.mode = home::UiMode::Cook; home.ui.plot = u.plot; home.ui.station = home::Station::Campfire;
      mode = Mode::Build;
      return true;
    case U_TROUGH: {
      const int hay = HomeOps::count(*this, [](const Item& it) { return it.kind == ItemKind::Misc && it.sub == home::MISC_PRODUCT + (int)home::Product::Hay; });
      ok = home::feedHay(*this, u.plot, hay, why);
      if (ok) say("THE TROUGHS ARE FILLED");
      break;
    }
    default: return false;
  }
  if (!ok) say(why);
  return true;
}

bool home::yardUseAhead(const Game& g) { return useAt(g).kind != U_NONE; }

std::string home::interactLabel(const Game& g) {
  if (g.mode != Mode::Play) return "";
  const int t = g.interactTarget();
  if (t >= 0) {
    for (const Actor& a : g.actors)
      if (a.id == t) return (a.lifeBits & LB_HOMESTEAD) && a.critter ? HomeOps::critterLabel(g, a) : std::string();
    return "";
  }
  return useAt(g).label;
}

bool Game::homeUseProp(art::Prop p, int tx, int ty) { return HomeOps::useProp(*this, p, tx, ty); }
bool Game::homePropUsable(art::Prop p, int tx, int ty) const { return HomeOps::propUsable(*this, p, tx, ty); }
void Game::homeMapLoaded() { HomeOps::mapLoaded(*this); }
void Game::homeTalk(Actor& a) { HomeOps::talk(*this, a); }
bool Game::homeChoose(const DlgOpt& o) { return HomeOps::choose(*this, o); }
bool Game::homeBed(int tx, int ty) const { return HomeOps::bed(*this, tx, ty); }
void Game::homeAte(const Item& food) {
  float h = 0;
  uint8_t q = 0;
  home::mealBuff(food, h, q);
  if (h <= 0) return;
  // a cooked meal's Well Fed replaces a lesser one (never adds up; potions are separate: their cooldown, no buff); a
  // lesser meal over a better one still being digested leaves it be. (M7 fix r3) the station's quality and hours are
  // the meal's own: a campfire stew is a campfire stew, a hearth's better, an inn's best
  life::PlayerNeeds& n = life.player;
  q = std::max<uint8_t>(q, 1);   // (any cooked meal is at least a snack's worth)
  const uint8_t q0 = n.fedH > 0 ? n.mealQuality : 0;
  if (q >= q0) {
    n.fedH = q == q0 ? std::max(n.fedH, h) : h;
    n.mealQuality = q;
  }
  n.sinceMealH = 0;
}

void Game::homeShopStock(const Actor& merchant, std::vector<Item>& stock) {
  const int si = HomeOps::siteOfActor(*this, merchant);
  if (si < 0 || !world.sites[(size_t)si].settlement()) return;
  const Site& s = world.sites[(size_t)si];
  const art::Building where = merchant.bldg >= 0 && merchant.bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)merchant.bldg].type : art::Building::House;
  const bool general = merchant.role == Role::Merchant && (where == art::Building::Shop || where == art::Building::Exchange || merchant.stallKeeper || merchant.bldg < 0);
  const bool farmer = merchant.role == Role::Farmer;
  Rng r(hash32((uint32_t)npcKey(merchant) ^ 0x5EED5u ^ (uint32_t)(day / 2)));
  if (general || farmer) {
    // seeds of the culture's staples (warm crops only where the land is warm: the staple table says which)
    const std::vector<home::Crop> own = home::seedCrops(s.culture, *this);
    const int kinds = std::min((int)own.size(), farmer ? 3 : 4);
    const int first = own.empty() ? 0 : r.irange((int)own.size());
    for (int k = 0; k < kinds; k++) {
      const int n = 4 + r.irange(6);
      stock.push_back(home::makeSeeds(own[(size_t)((first + k) % (int)own.size())], n));
    }
  }
  if (general) {
    stock.push_back(home::makeTool(home::Tool::Hoe));
    stock.push_back(home::makeTool(home::Tool::WateringCan));
    stock.push_back(home::makeTool(home::Tool::Brush));
  }
  if (merchant.role == Role::Smith) {
    stock.push_back(home::makeTool(home::Tool::Hoe));
    stock.push_back(home::makeTool(home::Tool::Sickle));
  }
  const bool farmland = (ew::Specialty)s.special == ew::Specialty::Farming || (ew::Specialty)s.special == ew::Specialty::Herding || s.type == SiteType::Village;
  if ((general || farmer) && farmland) {
    const int n = 10 + r.irange(11);
    stock.push_back(home::makeProduct(home::Product::Hay, n));
  }
}

namespace home {

// ---------------------------------------------------------------- the window stamp
void stampWindow(Game& g) {
  World& W = g.world;
  Map& M = W.over;
  if (M.ground.empty() || M.solid.size() != M.ground.size()) return;
  // (cheap: it runs after every window shift) solidity is updated tile by tile, never by a whole-window rebuildSolid:
  // a tile no building covers is solid when its prop or a wall says so (Map::rebuildSolid's rule for that tile)
  auto resolid = [&](int x, int y) {
    if (!M.in(x, y)) return;
    const size_t i = (size_t)y * M.w + x;
    if (M.bldgAt[i] >= 0) return;
    const int pr = M.prop[i];
    M.solid[i] = (uint8_t)(((pr && propSolid((art::Prop)(pr - 1))) || M.wall[i]) ? 1 : 0);
  };
  for (const Plot& p : g.home.plots) {
    const int lx = p.gx - W.ox, ly = p.gy - W.oy;
    if (lx + p.w <= 0 || ly + p.h <= 0 || lx >= M.w || ly >= M.h) continue;
    if (p.kind == PlotKind::Lot)
      for (int y = 0; y < p.h; y++)
        for (int x = 0; x < p.w; x++) M.setP(lx + x, ly + y, 0);   // the lot's own ground: no generated props
    for (const PlacedObj& o : p.outside) {
      const ObjInfo& oi = objInfo((Obj)o.kind);
      if (!(oi.flags & OBJ_SOLID)) continue;
      int w = oi.w, h = oi.h;
      if (o.turned() && (oi.flags & OBJ_ROTATES)) std::swap(w, h);
      for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) M.setProp(lx + o.x + x, ly + o.y + y, art::Prop::Filler);
    }
    for (int y = 0; y < p.h; y++)
      for (int x = 0; x < p.w; x++) resolid(lx + x, ly + y);
    if (p.kind == PlotKind::Lot && p.hw && p.state != PlotState::Owned) {
      Bldg b = houseBldg(p, g);
      b.r.x -= W.ox; b.r.y -= W.oy;
      int bi = W.bldgHandle(b.id);
      if (bi < 0) { bi = (int)M.bldgs.size(); W.bldgById[b.id] = bi; M.bldgs.push_back(b); }
      else M.bldgs[(size_t)bi] = b;
      for (int y = b.r.y; y < b.r.y + b.r.h; y++)
        for (int x = b.r.x; x < b.r.x + b.r.w; x++)
          if (M.in(x, y)) { M.setP(x, y, 0); M.solid[(size_t)y * M.w + x] = 1; M.bldgAt[(size_t)y * M.w + x] = bi; }
      if (M.in(b.doorX(), b.doorY())) M.solid[(size_t)b.doorY() * M.w + b.doorX()] = 0;   // the door is walkable (as rebuildSolid)
    } else if (p.kind == PlotKind::House) {
      const int bi = W.bldgHandle(p.id);
      if (bi >= 0) {
        Bldg& b = M.bldgs[(size_t)bi];
        b.home = 2;
        // the FOR SALE sign the house had is taken down
        const int sx = b.doorX() + 1, sy = b.doorY() + 1;
        if (M.propAt(sx, sy) == (int)art::Prop::ForSaleSign + 1) { M.setP(sx, sy, 0); resolid(sx, sy); }
      }
    }
  }
  // (M7 fixer r2, review: "world trees grow through lot fences and the yard") a tree just south of a lot (or right
  // beside it) spreads its crown over the fence and the yard: the trees whose trunks stand in that margin are felled
  // (south: three rows, its crown is ~2.5 tiles tall; east and west: one column), for the lots for sale and the
  // player's own alike
  auto clearMargin = [&](int lx, int ly, int w, int h) {
    if (lx + w + 1 <= 0 || ly + h + 3 <= 0 || lx - 1 >= M.w || ly >= M.h) return;
    for (int y = ly; y < ly + h + 3; y++)
      for (int x = lx - 1; x <= lx + w; x++) {
        if (y < ly + h && x >= lx && x < lx + w) continue;
        if (!M.in(x, y)) continue;
        const int pr = M.propAt(x, y);
        if (pr && art::isTreeProp((art::Prop)(pr - 1))) { M.setP(x, y, 0); resolid(x, y); }
      }
  };
  for (const Plot& p : g.home.plots) clearMargin(p.gx - W.ox, p.gy - W.oy, p.w, p.h);
  for (const ew::PlotPlan& L : W.lots) clearMargin(L.gx - W.ox, L.gy - W.oy, L.w, L.h);
  // FOR SALE signs by the doors of vacant houses (a lot's sign is the generator's)
  for (int si : W.nearSites) {
    if (!W.sites[(size_t)si].settlement()) continue;
    for (int bi : forSaleHouses(W, si)) {
      const Bldg& b = M.bldgs[(size_t)bi];
      if (g.home.plotById(b.id) >= 0) continue;
      const int sx = b.doorX() + 1, sy = b.doorY() + 1;
      if (!M.in(sx, sy) || M.propAt(sx, sy) || M.bldgAt[(size_t)sy * M.w + sx] >= 0 || groundSolid(M.at(sx, sy))) continue;
      M.setProp(sx, sy, art::Prop::ForSaleSign);
      resolid(sx, sy);
    }
  }
}

int debugLot(Game& g, int32_t gx, int32_t gy, LotSize s) {
  if ((int)g.home.plots.size() >= MAX_PLOTS) return -1;
  Plot p;
  p.kind = PlotKind::Lot;
  p.state = PlotState::Owned;
  // (scripts, tests) the nearest clear ground within 30 tiles: no building, wall, water, cliff, street or another plot
  // under the lot or the ring round it (trees and rocks are cleared by the stamp)
  {
    const Map& M = g.world.over;
    const int w = lotInfo(s).w, h = lotInfo(s).h;
    auto clear = [&](int32_t x0, int32_t y0) {
      for (int y = y0 - 1; y <= y0 + h; y++)
        for (int x = x0 - 1; x <= x0 + w; x++) {
          const int lx = x - g.world.ox, ly = y - g.world.oy;
          if (!M.in(lx, ly)) return false;
          const size_t i = (size_t)ly * M.w + lx;
          const Ground gr = M.at(lx, ly);
          if (groundSolid(gr) || gr == Ground::Road || gr == Ground::Plaza || gr == Ground::Bridge || M.bldgAt[i] >= 0 || M.wall[i] ||
              (!M.height.empty() && (M.height[i] & (Map::HEIGHT_CLIFF | Map::HEIGHT_RAMP))) || g.home.plotAt(x, y) >= 0)
            return false;
        }
      // (integration) nor over a settlement's generated lot (World::lots), ring included
      for (const ew::PlotPlan& L : g.world.lots)
        if (x0 - 1 < L.gx + L.w + 1 && L.gx - 1 < x0 + w + 1 && y0 - 1 < L.gy + L.h + 1 && L.gy - 1 < y0 + h + 1) return false;
      return true;
    };
    bool found = false;
    for (int r = 0; r <= 30 && !found; r++)
      for (int dy = -r; dy <= r && !found; dy++)
        for (int dx = -r; dx <= r && !found; dx++) {
          if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
          if (clear(gx + dx, gy + dy)) { gx += dx; gy += dy; found = true; }
        }
  }
  p.id = ew::makeId(ew::regionOf(gx), ew::regionOf(gy), ew::IdKind::Plot, 0x800u + (uint32_t)g.home.plots.size());
  const int si = HomeOps::settlementOf(g, gx, gy);
  p.site = si >= 0 ? g.world.sites[(size_t)si].id : 0;
  p.realm = holderOf(g, si);
  p.gx = gx; p.gy = gy;
  p.w = lotInfo(s).w; p.h = lotInfo(s).h;
  p.lastSeenDay = (uint16_t)g.day;
  p.paidUntilDay = (uint16_t)(g.day + 7);
  p.taxNoticeDay = (uint16_t)g.day;
  g.home.plots.push_back(p);
  HomeOps::restamp(g);
  return (int)g.home.plots.size() - 1;
}

// ---------------------------------------------------------------- buying
namespace {
void ownedNow(Game& g, int plot) {
  Plot& p = g.home.plots[(size_t)plot];
  p.lastSeenDay = (uint16_t)g.day;
  p.paidUntilDay = (uint16_t)(g.day + 7);   // the first week's tax is in the price
  p.taxNoticeDay = (uint16_t)g.day;
  p.realm = holderOf(g, g.world.siteHandle(p.site));
  HomeOps::give(g, makeDeed(g, plot));
  HomeOps::restamp(g);
}
}  // namespace

bool buyLot(Game& g, int lot, std::string& why) {
  if (lot < 0 || lot >= (int)g.world.lots.size()) { why = "NO SUCH PLOT"; return false; }
  const ew::PlotPlan L = g.world.lots[(size_t)lot];
  if (g.home.plotById(L.id) >= 0) { why = "IT IS YOURS ALREADY"; return false; }
  if ((int)g.home.plots.size() >= MAX_PLOTS) { why = "YOU OWN ENOUGH LAND"; return false; }
  if (const int lien = lienOwed(g)) { why = "THE REGISTER HOLDS A LIEN ON YOU: PAY YOUR " + std::to_string(lien) + " GOLD OF TAXES FIRST"; return false; }
  const int si = g.world.siteHandle(L.site);
  const int price = (int)std::lround(lotInfo((LotSize)std::min<int>(L.size, 2)).price * priceFactor(g, si));
  if (g.gold < price) { why = "IT COSTS " + std::to_string(price) + " GOLD"; return false; }
  g.gold -= price;
  Plot p;
  p.kind = PlotKind::Lot; p.state = PlotState::Owned; p.id = L.id; p.site = L.site;
  p.gx = L.gx; p.gy = L.gy; p.w = L.w; p.h = L.h;
  p.flags = (uint8_t)((L.flags & ew::PLOT_RIVERSIDE ? PF_RIVERSIDE : 0) | (L.flags & ew::PLOT_FENCED ? PF_FENCED : 0));
  if (L.flags & ew::PLOT_FENCED) {   // the generator's fence ring and gate become the player's own objects
    const int gx = L.gateX - L.gx, gy = L.gateY - L.gy;
    for (int y = 0; y < p.h; y++)
      for (int x = 0; x < p.w; x++) {
        if (x != 0 && y != 0 && x != p.w - 1 && y != p.h - 1) continue;
        PlacedObj o;
        o.kind = (uint8_t)(x == gx && y == gy ? Obj::Gate : Obj::Fence);
        o.x = (uint8_t)x; o.y = (uint8_t)y;
        p.outside.push_back(o);
      }
  }
  g.home.plots.push_back(p);
  ownedNow(g, (int)g.home.plots.size() - 1);
  return true;
}

bool buyHouse(Game& g, int bldg, std::string& why) {
  if (bldg < 0 || bldg >= (int)g.world.over.bldgs.size()) { why = "NO SUCH HOUSE"; return false; }
  const Bldg& b = g.world.over.bldgs[(size_t)bldg];
  const std::vector<int> sale = forSaleHouses(g.world, b.site);
  if (std::find(sale.begin(), sale.end(), bldg) == sale.end()) { why = "IT IS NOT FOR SALE"; return false; }
  if (g.home.plotById(b.id) >= 0) { why = "IT IS YOURS ALREADY"; return false; }
  if ((int)g.home.plots.size() >= MAX_PLOTS) { why = "YOU OWN ENOUGH HOUSES"; return false; }
  if (const int lien = lienOwed(g)) { why = "THE REGISTER HOLDS A LIEN ON YOU: PAY YOUR " + std::to_string(lien) + " GOLD OF TAXES FIRST"; return false; }
  const int price = (int)std::lround(houseInfo(houseKindOf(b)).price * priceFactor(g, b.site));
  if (g.gold < price) { why = "IT COSTS " + std::to_string(price) + " GOLD"; return false; }
  g.gold -= price;
  Plot p;
  p.kind = PlotKind::House; p.state = PlotState::Built; p.id = b.id;
  p.site = b.site >= 0 ? g.world.sites[(size_t)b.site].id : 0;
  p.gx = g.world.ox + b.r.x - 1; p.gy = g.world.oy + b.r.y - 1;
  p.w = (uint8_t)(b.r.w + 2); p.h = (uint8_t)(b.r.h + 3);
  p.hx = 1; p.hy = 1; p.hw = (uint8_t)b.r.w; p.hh = (uint8_t)b.r.h;
  p.stores.push_back(Store());   // the house's chest (the furniture taken on the first entry adds its own stores)
  g.home.plots.push_back(p);
  ownedNow(g, (int)g.home.plots.size() - 1);
  return true;
}

void haveMaterials(const Game& g, int& timber, int& iron) {
  timber = HomeOps::count(g, [](const Item& it) { return craft::isStuff(it, craft::Stuff::Timber); });
  iron = HomeOps::count(g, [](const Item& it) { return craft::isStuff(it, craft::Stuff::IronIngot); });
}

int buildPrice(const Game& g, int plot, Shell s, int timber, int iron) {
  const ShellInfo& si = shellInfo(s);
  timber = std::clamp(timber, 0, (int)si.timber);
  iron = std::clamp(iron, 0, (int)si.iron);
  const int site = plot >= 0 && plot < (int)g.home.plots.size() ? g.world.siteHandle(g.home.plots[(size_t)plot].site) : -1;
  const int base = (int)std::lround(si.gold * priceFactor(g, site));
  return std::max(base * 3 / 10, base - timber * si.timberCredit - iron * si.ironCredit);
}

bool startBuild(Game& g, int plot, Shell s, uint64_t culture, int timber, int iron, std::string& why) {
  if (plot < 0 || plot >= (int)g.home.plots.size()) { why = "NO SUCH PLOT"; return false; }
  Plot& p = g.home.plots[(size_t)plot];
  const ShellInfo& si = shellInfo(s);
  if (p.kind != PlotKind::Lot || p.state != PlotState::Owned) { why = "A HOUSE STANDS THERE ALREADY"; return false; }
  if (!shellFits(p, s)) { why = std::string("A ") + si.name + " NEEDS A BIGGER PLOT"; return false; }
  if (culture && !g.home.knowsStyle(culture)) { why = "YOU HAVE NOT SEEN THAT PEOPLE'S BUILDINGS"; return false; }
  if (const int lien = lienOwed(g)) { why = "THE REGISTER HOLDS A LIEN ON YOU: PAY YOUR " + std::to_string(lien) + " GOLD OF TAXES FIRST"; return false; }
  const int hx = (p.w - si.w) / 2, hy = 1;
  for (const PlacedObj& o : p.outside) {
    const ObjInfo& oi = objInfo((Obj)o.kind);
    int w = oi.w, h = oi.h;
    if (o.turned() && (oi.flags & OBJ_ROTATES)) std::swap(w, h);
    if (o.x < hx + si.w && o.x + w > hx && o.y < hy + si.h + 1 && o.y + h > hy) { why = "CLEAR THE GROUND FIRST"; return false; }
  }
  for (const CropRec& c : p.crops)
    if (c.x >= hx && c.x < hx + si.w && c.y >= hy && c.y <= hy + si.h) { why = "CLEAR THE GROUND FIRST"; return false; }
  // the ground under the house must be fit to build on (no water, rock or another building)
  for (int y = hy; y < hy + si.h; y++)
    for (int x = hx; x < hx + si.w; x++)
      if (!worldFree(g, p, x, y, why)) return false;
  timber = std::clamp(timber, 0, (int)si.timber);
  iron = std::clamp(iron, 0, (int)si.iron);
  int haveT = 0, haveI = 0;
  haveMaterials(g, haveT, haveI);
  if (haveT < timber || haveI < iron) { why = "YOU DO NOT HAVE THOSE MATERIALS"; return false; }
  const int price = buildPrice(g, plot, s, timber, iron);
  if (g.gold < price) { why = "THE BUILDERS ASK " + std::to_string(price) + " GOLD"; return false; }
  if (timber) HomeOps::take(g, timber, [](const Item& it) { return craft::isStuff(it, craft::Stuff::Timber); });
  if (iron) HomeOps::take(g, iron, [](const Item& it) { return craft::isStuff(it, craft::Stuff::IronIngot); });
  g.gold -= price;
  p.state = PlotState::Building;
  p.shell = (uint8_t)s;
  p.style = culture;
  p.hx = (int8_t)hx; p.hy = (int8_t)hy; p.hw = si.w; p.hh = si.h;
  const uint64_t hs = ew::mix64(p.id ^ ((uint64_t)(uint32_t)g.day << 20) ^ culture);
  p.houseSeed = (uint32_t)hs;
  p.buildStartDay = (uint16_t)g.day;
  p.buildDoneDay = (uint16_t)(g.day + si.days);
  p.starter = 0;
  p.furnished = 0;
  if (p.stores.empty()) p.stores.push_back(Store());
  HomeOps::restamp(g);
  return true;
}

// ---------------------------------------------------------------- the yard
bool placeObj(Game& g, int plot, Obj o, int x, int y, bool turned, bool inside, std::string& why) {
  if (plot < 0 || plot >= (int)g.home.plots.size()) { why = "NO SUCH PLOT"; return false; }
  Plot& p = g.home.plots[(size_t)plot];
  const ObjInfo& oi = objInfo(o);
  if (inside) {
    if (plotOfInterior(g) != plot) { why = "GO INTO YOUR HOUSE FIRST"; return false; }
    if (!canPlaceInside(p, g.sub, o, x, y, turned, why)) return false;
  } else {
    if (!canPlaceOutside(p, o, x, y, turned, why)) return false;
    int w = oi.w, h = oi.h;
    if (turned && (oi.flags & OBJ_ROTATES)) std::swap(w, h);
    for (int yy = y; yy < y + h; yy++)
      for (int xx = x; xx < x + w; xx++)
        if (!worldFree(g, p, xx, yy, why)) return false;
  }
  if (g.gold < oi.price) { why = "IT COSTS " + std::to_string(oi.price) + " GOLD"; return false; }
  int store = -1;
  if ((oi.flags & OBJ_STORAGE) && (store = allocStore(p)) < 0) { why = "YOU HAVE NO ROOM FOR MORE STORES"; return false; }
  g.gold -= oi.price;
  if (oi.flags & OBJ_GROUND) { setGround(p, x, y, o == Obj::Path ? 1 : 2); return true; }
  PlacedObj po;
  po.kind = (uint8_t)o; po.x = (uint8_t)x; po.y = (uint8_t)y;
  po.flags = (uint8_t)((turned && (oi.flags & OBJ_ROTATES) ? 1 : 0) | (inside ? (g.subFloor << 4) : 0));
  if (oi.flags & OBJ_STORAGE) po.data = (uint32_t)store;
  if (o == Obj::Sapling) po.data = (uint32_t)(uint16_t)g.day;
  if (o == Obj::Beehive) po.data = (uint32_t)(uint16_t)g.day << 8;
  (inside ? p.inside : p.outside).push_back(po);
  updateDogFlag(p);
  if (inside) { HomeOps::stampObj(g.sub, po, false); g.sub.rebuildSolid(); }
  else HomeOps::restamp(g);
  return true;
}

bool removeObj(Game& g, int plot, int x, int y, bool inside, std::string& why) {
  if (plot < 0 || plot >= (int)g.home.plots.size()) { why = "NO SUCH PLOT"; return false; }
  Plot& p = g.home.plots[(size_t)plot];
  std::vector<PlacedObj>& v = inside ? p.inside : p.outside;
  for (size_t i = 0; i < v.size(); i++) {
    const PlacedObj o = v[i];
    const ObjInfo& oi = objInfo((Obj)o.kind);
    if (inside && o.floor() != g.subFloor) continue;
    int w = oi.w, h = oi.h;
    if (o.turned() && (oi.flags & OBJ_ROTATES)) std::swap(w, h);
    if (x < o.x || y < o.y || x >= o.x + w || y >= o.y + h) continue;
    if ((oi.flags & OBJ_STORAGE) && o.data < p.stores.size() && !p.stores[o.data].items.empty()) { why = "EMPTY IT FIRST"; return false; }
    if ((Obj)o.kind == Obj::Bed && inside) {
      int beds = 0;
      for (const PlacedObj& q : p.inside) beds += (Obj)q.kind == Obj::Bed;
      if (beds <= 1) { why = "YOU WOULD HAVE NOWHERE TO SLEEP"; return false; }
    }
    g.gold += oi.price / 2;
    if (inside && g.inside) { HomeOps::stampObj(g.sub, o, true); g.sub.rebuildSolid(); }
    v.erase(v.begin() + (std::ptrdiff_t)i);
    if (oi.flags & OBJ_STORAGE) trimStores(p);
    updateDogFlag(p);
    if (!inside) HomeOps::restamp(g);
    return true;
  }
  if (!inside && groundAt(p, x, y)) {
    for (const CropRec& c : p.crops) if (c.x == x && c.y == y) { why = "A CROP GROWS THERE"; return false; }
    setGround(p, x, y, 0);
    return true;
  }
  why = "NOTHING TO TAKE AWAY THERE";
  return false;
}

bool till(Game& g, int plot, int x, int y, std::string& why) {
  if (plot < 0 || plot >= (int)g.home.plots.size()) { why = "NO SUCH PLOT"; return false; }
  Plot& p = g.home.plots[(size_t)plot];
  if (!HomeOps::hasTool(g, Tool::Hoe)) { why = "YOU NEED A HOE"; return false; }
  if (groundAt(p, x, y) != 0) { why = "IT IS TILLED ALREADY"; return false; }
  if (!canPlaceOutside(p, Obj::Farmland, x, y, false, why)) return false;
  if (!worldFree(g, p, x, y, why)) return false;
  setGround(p, x, y, 2);
  return true;
}

bool paddyOk(const Game& g, const Plot& p, int x, int y) {
  const Map& M = g.world.over;
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      const int lx = p.gx + x + dx - g.world.ox, ly = p.gy + y + dy - g.world.oy;
      if (M.in(lx, ly) && groundWater(M.at(lx, ly))) return true;
    }
  return false;
}

bool plant(Game& g, int plot, int x, int y, Crop c, std::string& why) {
  if (plot < 0 || plot >= (int)g.home.plots.size()) { why = "NO SUCH PLOT"; return false; }
  Plot& p = g.home.plots[(size_t)plot];
  const CropInfo& ci = cropInfo(c);
  const int gk = groundAt(p, x, y);
  if (gk != 2 && gk != 3) { why = "TILL THE SOIL FIRST"; return false; }
  for (const CropRec& r : p.crops) if (r.x == x && r.y == y) { why = "SOMETHING GROWS THERE ALREADY"; return false; }
  if ((int)p.crops.size() >= MAX_CROPS) { why = "THE FIELDS ARE FULL"; return false; }
  if (!(ci.seasons & seasonBit(seasonOf(g.day)))) { why = std::string(ci.name) + " WILL NOT GROW IN " + seasonName(seasonOf(g.day)); return false; }
  if (ci.paddy && !paddyOk(g, p, x, y)) { why = std::string(ci.name) + " NEEDS WATER BESIDE IT"; return false; }
  if (!HomeOps::take(g, 1, [c](const Item& it) { Crop k; return isSeed(it, k) && k == c; })) { why = "YOU HAVE NO " + seedName(c); return false; }
  CropRec r;
  r.x = (uint8_t)x; r.y = (uint8_t)y; r.kind = (uint8_t)c;
  r.plantedDay = (uint16_t)g.day; r.lastWaterDay = 0; r.lastGrowDay = (uint16_t)g.day;
  p.crops.push_back(r);
  return true;
}

bool water(Game& g, int plot, int x, int y, std::string& why) {
  if (plot < 0 || plot >= (int)g.home.plots.size()) { why = "NO SUCH PLOT"; return false; }
  Plot& p = g.home.plots[(size_t)plot];
  if (!HomeOps::hasTool(g, Tool::WateringCan)) { why = "YOU NEED A WATERING CAN"; return false; }
  for (CropRec& r : p.crops)
    if (r.x == x && r.y == y) {
      r.lastWaterDay = (uint16_t)g.day;
      if (groundAt(p, x, y) == 2) setGround(p, x, y, 3);   // the soil darkens (the view's watered farmland)
      return true;
    }
  why = "NOTHING GROWS THERE";
  return false;
}

bool harvest(Game& g, int plot, int x, int y, std::string& why) {
  if (plot < 0 || plot >= (int)g.home.plots.size()) { why = "NO SUCH PLOT"; return false; }
  Plot& p = g.home.plots[(size_t)plot];
  for (size_t i = 0; i < p.crops.size(); i++) {
    CropRec& r = p.crops[i];
    if (r.x != x || r.y != y) continue;
    if (r.stage < 3) { why = "IT IS NOT RIPE YET"; return false; }
    const CropInfo& ci = cropInfo((Crop)r.kind);
    const uint64_t h = ew::mix64(g.seed ^ p.id ^ ((uint64_t)x << 40) ^ ((uint64_t)y << 48) ^ (uint64_t)(uint32_t)g.day);
    const int n = harvestYield(ci, r.quality, h) + (HomeOps::hasTool(g, Tool::Sickle) && !ci.perennial && (h >> 32) % 3 == 0 ? 1 : 0);
    HomeOps::give(g, makeCropItem((Crop)r.kind, n, r.quality));
    if (ci.perennial) { r.grown = (uint8_t)(ci.days / 2); r.stage = 1; r.streak = 0; }
    else {
      p.crops.erase(p.crops.begin() + (std::ptrdiff_t)i);
      if (groundAt(p, x, y) == 3) setGround(p, x, y, 2);
    }
    return true;
  }
  why = "NOTHING GROWS THERE";
  return false;
}

// ---------------------------------------------------------------- animals
bool buyAnimal(Game& g, int plot, Animal a, std::string& why) {
  if (plot < 0 || plot >= (int)g.home.plots.size()) { why = "NO SUCH PLOT"; return false; }
  Plot& p = g.home.plots[(size_t)plot];
  const AnimalInfo& ai = animalInfo(a);
  if ((int)p.animals.size() >= MAX_ANIMALS) { why = "YOU HAVE NO ROOM FOR MORE BEASTS"; return false; }
  if (ai.home != Obj::COUNT) {
    int room = 0, used = 0;
    for (const PlacedObj& o : p.outside) if ((Obj)o.kind == ai.home) room += objInfo(ai.home).capacity;
    for (const AnimalRec& r : p.animals) if (animalInfo((Animal)r.kind).home == ai.home && !(r.flags & AF_LOST)) used++;
    if (used >= room) { why = std::string("BUILD A ") + objInfo(ai.home).name + " FIRST"; return false; }
  }
  int price = ai.price;
  Breed br = Breed::Heartland;
  if (a == Animal::Horse) {
    const int si = g.world.siteHandle(p.site);
    br = breedOfCulture(si >= 0 ? g.world.sites[(size_t)si].culture : 0, g);
    price = breedInfo(br).price;
  }
  if (g.gold < price) { why = "IT COSTS " + std::to_string(price) + " GOLD"; return false; }
  g.gold -= price;
  AnimalRec r;
  r.kind = (uint8_t)a;
  r.breed = (uint8_t)br;
  r.boughtDay = (uint16_t)g.day; r.lastFedDay = (uint16_t)g.day; r.lastProduceDay = (uint16_t)g.day;
  r.seed = (uint32_t)ew::mix64(p.id ^ ((uint64_t)p.animals.size() << 32) ^ (uint64_t)(uint32_t)g.day);
  p.animals.push_back(r);
  updateDogFlag(p);
  return true;
}

bool collect(Game& g, int plot, std::string& why) {
  if (plot < 0 || plot >= (int)g.home.plots.size()) { why = "NO SUCH PLOT"; return false; }
  Plot& p = g.home.plots[(size_t)plot];
  int got = 0;
  for (AnimalRec& a : p.animals) {
    if (!a.produce || (a.flags & AF_LOST)) continue;
    const AnimalInfo& ai = animalInfo((Animal)a.kind);
    HomeOps::give(g, makeProduct((Product)ai.product, a.produce));
    got += a.produce;
    a.produce = 0;
  }
  if (!got) { why = "NOTHING TO COLLECT YET"; return false; }
  return true;
}

bool feedHay(Game& g, int plot, int hay, std::string& why) {
  if (plot < 0 || plot >= (int)g.home.plots.size()) { why = "NO SUCH PLOT"; return false; }
  Plot& p = g.home.plots[(size_t)plot];
  int eaters = 0;
  for (const AnimalRec& a : p.animals) if (!(a.flags & AF_LOST) && animalInfo((Animal)a.kind).hay) eaters++;
  if (!eaters) { why = "NO BEASTS TO FEED"; return false; }
  const int want = std::min(hay, (7 - (int)p.trough) * eaters);
  if (want < eaters) { why = want <= 0 ? "THE TROUGHS ARE FULL" : "YOU HAVE NOT ENOUGH HAY"; return false; }
  const int use = want / eaters * eaters;
  if (!HomeOps::take(g, use, [](const Item& it) { return it.kind == ItemKind::Misc && it.sub == MISC_PRODUCT + (int)Product::Hay; })) {
    why = "YOU HAVE NOT ENOUGH HAY";
    return false;
  }
  p.trough = (uint8_t)std::min(7, (int)p.trough + use / eaters);
  return true;
}

bool hireFarmhand(Game& g, int plot, int days, std::string& why) {
  if (plot < 0 || plot >= (int)g.home.plots.size()) { why = "NO SUCH PLOT"; return false; }
  Plot& p = g.home.plots[(size_t)plot];
  const int cost = FARMHAND_WAGE * std::max(1, days);
  if (g.gold < cost) { why = "THE FARMHAND ASKS " + std::to_string(cost) + " GOLD"; return false; }
  g.gold -= cost;
  if (!p.farmhand) p.farmhand = 1;
  if (p.stores.empty()) p.stores.push_back(Store());
  p.farmhandUntil = (uint16_t)(std::max<int>(p.farmhandUntil, g.day) + days);
  return true;
}

// ---------------------------------------------------------------- the kitchen and the stores
bool stationOf(const Game& g, art::Prop p, Station& st) {
  using P = art::Prop;
  switch (p) {
    case P::Campfire: st = Station::Campfire; break;
    case P::Hearth: case P::Fireplace: case P::FirePitL: case P::FirePitM: case P::FirePitR: case P::Stove: st = Station::Hearth; break;
    case P::Cauldron: case P::Oven: st = Station::CookPot; break;
    default: return false;
  }
  if (p != P::Campfire && !g.inside) return false;
  if (g.inside && g.subBldg >= 0 && g.subBldg < (int)g.world.over.bldgs.size()) {
    const art::Building t = g.world.over.bldgs[(size_t)g.subBldg].type;
    if (t == art::Building::Inn || t == art::Building::MeadHall || t == art::Building::TeaHouse) st = Station::InnKitchen;
  }
  return true;
}

bool cook(Game& g, const Meal& m, Station st, std::string& why) {
  const std::string miss = missingFor(g, m);
  if (!miss.empty()) { why = "YOU LACK " + miss; return false; }
  int need[(int)Ingr::COUNT] = {};
  for (Ingr i : m.in) if (i != Ingr::None) need[(int)i]++;
  for (int k = 1; k < (int)Ingr::COUNT; k++)
    if (need[k]) HomeOps::take(g, need[k], [k](const Item& it) { return (int)ingredientOf(it) == k; });
  HomeOps::give(g, makeMeal(m, st));
  HomeOps::sfx(g, (int)Sfx::Pickup, g.pl().p, 0.8f, 0.7f);
  return true;
}

bool storePut(Game& g, int plot, int store, int invIndex, std::string& why) {
  if (plot < 0 || plot >= (int)g.home.plots.size()) { why = "NO SUCH PLOT"; return false; }
  Plot& p = g.home.plots[(size_t)plot];
  if (store < 0 || store >= (int)p.stores.size()) { why = "NO SUCH STORE"; return false; }
  if (invIndex < 0 || invIndex >= (int)g.inv.size()) { why = "NOTHING TO PUT AWAY"; return false; }
  const Item it = g.inv[(size_t)invIndex];
  if (it.kind == ItemKind::Quest) { why = "YOU MUST KEEP THAT"; return false; }
  Store& s = p.stores[(size_t)store];
  bool merged = false;
  if (it.stackable()) for (Item& x : s.items) if (x.same(it)) { x.count += it.count; merged = true; break; }
  if (!merged) {
    if ((int)s.items.size() >= STORE_STACKS) { why = "IT IS FULL"; return false; }
    s.items.push_back(it);
  }
  HomeOps::dropAt(g, invIndex);
  return true;
}

bool storeTake(Game& g, int plot, int store, int index, std::string& why) {
  if (plot < 0 || plot >= (int)g.home.plots.size()) { why = "NO SUCH PLOT"; return false; }
  Plot& p = g.home.plots[(size_t)plot];
  if (store < 0 || store >= (int)p.stores.size()) { why = "NO SUCH STORE"; return false; }
  Store& s = p.stores[(size_t)store];
  if (index < 0 || index >= (int)s.items.size()) { why = "NOTHING THERE"; return false; }
  HomeOps::give(g, s.items[(size_t)index]);
  s.items.erase(s.items.begin() + index);
  return true;
}

bool mount(Game& g, int plot, int animal, std::string& why) {
  if (plot < 0 || plot >= (int)g.home.plots.size()) { why = "NO SUCH PLOT"; return false; }
  const Plot& p = g.home.plots[(size_t)plot];
  if (animal < 0 || animal >= (int)p.animals.size() || (Animal)p.animals[(size_t)animal].kind != Animal::Horse) { why = "THAT IS NO HORSE"; return false; }
  if (g.inside) { why = "NOT INDOORS"; return false; }
  // another horse waiting somewhere goes home: one horse out at a time
  g.home.riding = plot * 64 + animal;
  g.home.horse = g.home.riding;
  g.home.horseInn = 0;
  g.home.ridingBreed = p.animals[(size_t)animal].breed;
  g.home.ridingSeed = p.animals[(size_t)animal].seed;
  HomeOps::wakeActors(g);   // her actor (in the yard, or waiting) goes on the next step
  return true;
}

int plotOfInterior(const Game& g) {
  if (!g.inside || g.subBldg < 0 || g.subBldg >= (int)g.world.over.bldgs.size()) return -1;
  const ew::Gid id = g.world.over.bldgs[(size_t)g.subBldg].id;
  for (size_t i = 0; i < g.home.plots.size(); i++) {
    const Plot& p = g.home.plots[i];
    if (p.kind == PlotKind::House ? p.id == id : (p.hw && p.state == PlotState::Built && houseBldg(p, g).id == id)) return (int)i;
  }
  return -1;
}

}  // namespace home
