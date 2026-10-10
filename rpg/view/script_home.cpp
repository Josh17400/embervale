// M7 Home script commands (lead, phase A; the HOMESTEAD lane owns this file and adds to it). They stage property, farms,
// animals and riding for screenshots and checks, never touching the player's save.
//   home lot [small|medium|large]      a made-up lot (home::debugLot), owned, its top-left 2 tiles east of the player and
//                                      centred on the player's row; it becomes the current plot (the last one made)
//   home buylot                        buy the nearest generated lot for sale (World::lots; gold is given if short)
//   home buyhouse                      buy the nearest vacant house for sale (home::forSaleHouses; gold given if short)
//   home build <hut|cottage|longhouse|townhouse|hall> [here|none|<archetype>] [instant]
//                                      start a house on the current plot in that culture's style (the style is marked
//                                      discovered); instant: finished at once
//   home place <object> <x> <y> [turned]   a yard object at plot tile (x, y) (fence gate path farmland well woodpile
//                                      beehive scarecrow coop pen stable trough workbench forge flowerbed sapling bench
//                                      lantern statue banner campfire doghouse hayrack crate); gold given if short
//   home fence                         a fence ring round the current plot with a gate in the middle of the south side
//   home crop <crop> <x> <y> [stage]   farmland and a crop at plot tile (x, y), at a stage 0..3 (default 0)
//   home field <x> <y> <w> <h> [stage|mixed]   a field of farmland with a crop on every tile (the crops in turn; mixed:
//                                      stages 0..3 by column)
//   home animal <chicken|goat|sheep|cow|pig|horse|dog|duck> [n]   buy n (1) animals for the current plot (gold given)
//   home fill                          the M7 showcase farm on the current plot: a cottage in the local style, a fence
//                                      ring and gate, coop, pen, stable, well, beehive, scarecrow, a mixed field, animals
//   home tools                         a hoe, a watering can, a sickle and a grooming brush, 5 seeds of each crop
//   home days <n>                      the clock moves n days on (the plots catch up on the next step)
//   home ride | home dismount          mount the current plot's first horse (one is bought if there is none) / get off
//   home yard | home shell             open Mode::Build on the current plot (the yard / the shell screen)
//   expect home plots <n>              how many properties the player owns
//   expect home riding <0|1>
//   expect home crops <n>              crops growing on the current plot
//   expect home stage <x> <y> <s>      the crop at plot tile (x, y) is at stage s
//   expect home built                  the current plot's house is finished
//   expect home dismounts <why> <n>    home::Dismount counts (attack hurt enter water sleep player)
// phase B (HOMESTEAD lane):
//   home enter <inn|house|vacant>      into the start village's inn / the current plot's house / the nearest vacant house
//   home leave                         out of the building
//   home gold <n>                      the purse holds n gold
//   home stand <x> <y>                 the player stands on plot tile (x, y) of the current plot
//   home talk <innkeeper|lord|stablehand|builder|farmer|miller|any>   the nearest such person in play: a conversation
//   home choose <text...>              the dialogue option whose label starts with the text (case does not matter)
//   home use <x> <y> | home use here   stand south of plot tile (x, y) facing it and press use (till, plant, water,
//                                      harvest, collect, ride...), or press use where the player stands
//   home cook <mealid|culture1..3> [campfire|hearth|pot|inn]   cook a known meal (the ingredients are given if short)
//   home eat                           eat the last cooked meal in the pack
//   home buyhorse [breed]              buy a horse (the start culture's breed): she waits beside the player
//   home horsehere                     the waiting horse walks up beside the player
//   home raid | home hire [days] | home exposed   a night raid now / a farmhand / mark the plot exposed
//   home face <north|south|east|west>  turn the player
//   expect home horse <none|waiting|riding|inn> | wellfed | recipes <n> | lost <n> | animals <n> | deed | store <n> |
//                      raid <outcome> | label <text...> | inhouse
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/home.h"
#include "rpg/sim/life.h"
#include "rpg/view/script_api.h"
#include "rpg/world/source.h"

namespace {

std::string lw(std::string s) {
  for (char& ch : s) if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
  return s;
}
int cur(Game& g) { return g.home.plots.empty() ? -1 : (int)g.home.plots.size() - 1; }

const char* const kCropWords[] = {"wheat", "barley", "oats", "rye", "potato", "turnip", "cabbage", "carrot", "onion", "flax", "beans",
                                  "grapes", "dates", "maize", "tea", "rice", "herbs"};
static_assert(sizeof(kCropWords) / sizeof(kCropWords[0]) == (size_t)home::CROPS, "a word for every crop");
const char* const kObjWords[] = {"fence", "gate", "path", "farmland", "well", "woodpile", "beehive", "scarecrow", "coop", "pen", "stable",
                                 "trough", "workbench", "forge", "flowerbed", "sapling", "bench", "lantern", "statue", "banner", "campfire",
                                 "doghouse", "hayrack", "crate", "bed", "table", "chair", "shelf", "chest", "rug", "hearth", "cookpot",
                                 "mannequin", "weaponrack", "trophy", "painting", "wardrobe", "bookshelf", "plantpot", "candelabra",
                                 "stool", "cupboard"};
static_assert(sizeof(kObjWords) / sizeof(kObjWords[0]) == (size_t)home::Obj::COUNT, "a word for every object");
const char* const kAnimalWords[] = {"chicken", "goat", "sheep", "cow", "pig", "horse", "dog", "duck"};
const char* const kShellWords[] = {"hut", "cottage", "longhouse", "townhouse", "hall"};
const char* const kDismountWords[] = {"player", "attack", "hurt", "enter", "water", "talk", "sleep", "travel"};

int find(const char* const* t, int n, const std::string& w) {
  for (int i = 0; i < n; i++) if (w == t[i]) return i;
  for (int i = 0; i < n; i++) if (std::string(t[i]).rfind(w, 0) == 0) return i;   // a prefix
  return -1;
}
uint64_t cultureWord(Game& g, const std::string& w) {
  if (w == "none" || !g.world.src) return 0;
  const int32_t gx = g.world.ox + (int32_t)(g.pl().p.x / TILE), gy = g.world.oy + (int32_t)(g.pl().p.y / TILE);
  if (w.empty() || w == "here") return g.world.src->cultureAt(gx, gy);
  for (int a = 0; a < (int)cult::Archetype::COUNT; a++) {
    if (lw(cult::archetypeName((cult::Archetype)a)) != w) continue;
    for (size_t si = 0; si < g.world.sites.size(); si++) {
      const cult::Culture* c = g.world.cultureOf((int)si);
      if (c && (int)c->archetype == a) return c->id;
    }
  }
  return 0;
}
void topUp(Game& g, int need) { if (g.gold < need) g.gold = need; }

bool placeAt(ScriptCtx& c, home::Obj o, int x, int y, bool turned) {
  Game& g = c.game;
  const int pi = cur(g);
  if (pi < 0) { c.fail("home: no plot (home lot first)"); return false; }
  topUp(g, g.gold + home::objInfo(o).price);
  std::string why;
  if (!home::placeObj(g, pi, o, x, y, turned, false, why)) { c.fail("home place " + std::string(kObjWords[(int)o]) + ": " + why); return false; }
  return true;
}
void setCrop(Game& g, int pi, int x, int y, home::Crop k, int stage) {
  home::Plot& p = g.home.plots[(size_t)pi];
  home::setGround(p, x, y, 2);
  for (size_t i = 0; i < p.crops.size(); i++) if (p.crops[i].x == x && p.crops[i].y == y) { p.crops.erase(p.crops.begin() + (std::ptrdiff_t)i); break; }
  home::CropRec r;
  r.x = (uint8_t)x; r.y = (uint8_t)y; r.kind = (uint8_t)k;
  const home::CropInfo& ci = home::cropInfo(k);
  r.stage = (uint8_t)std::clamp(stage, 0, 3);
  r.grown = (uint8_t)(r.stage >= 3 ? ci.days : r.stage * ci.days / 3);
  r.quality = r.stage >= 3 ? 2 : 0;
  r.plantedDay = (uint16_t)std::max(1, g.day - r.grown);
  r.lastWaterDay = (uint16_t)g.day; r.lastGrowDay = (uint16_t)g.day;
  p.crops.push_back(r);
}
bool buyAnimals(ScriptCtx& c, home::Animal a, int n) {
  Game& g = c.game;
  const int pi = cur(g);
  if (pi < 0) { c.fail("home: no plot (home lot first)"); return false; }
  for (int i = 0; i < n; i++) {
    topUp(g, g.gold + 4000);
    std::string why;
    if (!home::buyAnimal(g, pi, a, why)) { c.fail("home animal " + std::string(kAnimalWords[(int)a]) + ": " + why); return false; }
  }
  return true;
}
bool startHouse(ScriptCtx& c, home::Shell s, uint64_t cul, bool instant) {
  Game& g = c.game;
  const int pi = cur(g);
  if (pi < 0) { c.fail("home: no plot (home lot first)"); return false; }
  if (cul && !g.home.knowsStyle(cul)) g.home.styles.push_back(cul);
  topUp(g, g.gold + home::buildPrice(g, pi, s, 0, 0));
  std::string why;
  if (!home::startBuild(g, pi, s, cul, 0, 0, why)) { c.fail("home build: " + why); return false; }
  if (instant) g.home.plots[(size_t)pi].buildDoneDay = (uint16_t)g.day;
  return true;
}

bool cmdHome(ScriptCtx& c) {
  Game& g = c.game;
  const std::string sub = lw(c.arg(1));
  if (sub == "lot") {
    const std::string sz = lw(c.arg(2));
    const home::LotSize s = sz == "small" ? home::LotSize::Small : sz == "large" ? home::LotSize::Large : home::LotSize::Medium;
    const home::LotInfo& li = home::lotInfo(s);
    const int32_t px = g.world.ox + (int32_t)(g.pl().p.x / TILE), py = g.world.oy + (int32_t)(g.pl().p.y / TILE);
    if (home::debugLot(g, px + 2, py - li.h / 2, s) < 0) c.fail("home lot: no room for another plot");
    return true;
  }
  if (sub == "buylot") {
    const int32_t px = g.world.ox + (int32_t)(g.pl().p.x / TILE), py = g.world.oy + (int32_t)(g.pl().p.y / TILE);
    int best = -1;
    int64_t bd = INT64_MAX;
    for (int i = 0; i < (int)g.world.lots.size(); i++) {
      const ew::PlotPlan& L = g.world.lots[(size_t)i];
      if (g.home.plotById(L.id) >= 0) continue;
      const int64_t dx = L.gx - px, dy = L.gy - py, d = dx * dx + dy * dy;
      if (d < bd) { bd = d; best = i; }
    }
    if (best < 0) { c.fail("home buylot: no lot for sale met yet"); return true; }
    topUp(g, g.gold + 20000);
    std::string why;
    if (!home::buyLot(g, best, why)) c.fail("home buylot: " + why);
    return true;
  }
  if (sub == "buyhouse") {
    int best = -1;
    float bd = 1e30f;
    for (int si : g.world.nearSites)
      for (int bi : home::forSaleHouses(g.world, si)) {
        const Bldg& b = g.world.over.bldgs[(size_t)bi];
        const float dx = b.doorX() * TILE - g.pl().p.x, dy = b.doorY() * TILE - g.pl().p.y, d = dx * dx + dy * dy;
        if (g.home.plotById(b.id) < 0 && d < bd) { bd = d; best = bi; }
      }
    if (best < 0) { c.fail("home buyhouse: no house for sale nearby"); return true; }
    topUp(g, g.gold + 60000);
    std::string why;
    if (!home::buyHouse(g, best, why)) c.fail("home buyhouse: " + why);
    return true;
  }
  if (sub == "build") {
    const int s = find(kShellWords, (int)home::Shell::COUNT, lw(c.arg(2)));
    if (s < 0) { c.fail("home build: unknown shell '" + c.arg(2) + "'"); return true; }
    std::string cw = "here";
    bool instant = false;
    for (size_t i = 3; i < c.a.size(); i++) { if (lw(c.a[i]) == "instant") instant = true; else cw = lw(c.a[i]); }
    startHouse(c, (home::Shell)s, cultureWord(g, cw), instant);
    return true;
  }
  if (sub == "place") {
    const int o = find(kObjWords, (int)home::Obj::COUNT, lw(c.arg(2)));
    if (o < 0) { c.fail("home place: unknown object '" + c.arg(2) + "'"); return true; }
    placeAt(c, (home::Obj)o, std::atoi(c.arg(3).c_str()), std::atoi(c.arg(4).c_str()), lw(c.arg(5)) == "turned");
    return true;
  }
  if (sub == "fence") {
    const int pi = cur(g);
    if (pi < 0) { c.fail("home: no plot (home lot first)"); return true; }
    const home::Plot p = g.home.plots[(size_t)pi];
    placeAt(c, home::Obj::Gate, p.w / 2, p.h - 1, false);
    for (int y = 0; y < p.h; y++)
      for (int x = 0; x < p.w; x++) {
        if ((x != 0 && y != 0 && x != p.w - 1 && y != p.h - 1) || (x == p.w / 2 && y == p.h - 1)) continue;
        topUp(g, g.gold + 10);
        std::string why;
        home::placeObj(g, pi, home::Obj::Fence, x, y, false, false, why);
      }
    return true;
  }
  if (sub == "crop") {
    const int pi = cur(g);
    const int k = find(kCropWords, home::CROPS, lw(c.arg(2)));
    if (pi < 0 || k < 0) { c.fail("home crop: no plot or unknown crop '" + c.arg(2) + "'"); return true; }
    setCrop(g, pi, std::atoi(c.arg(3).c_str()), std::atoi(c.arg(4).c_str()), (home::Crop)k, std::atoi(c.arg(5).c_str()));
    return true;
  }
  if (sub == "field") {
    const int pi = cur(g);
    if (pi < 0) { c.fail("home field: no plot"); return true; }
    const int x0 = std::atoi(c.arg(2).c_str()), y0 = std::atoi(c.arg(3).c_str()), w = std::atoi(c.arg(4).c_str()), h = std::atoi(c.arg(5).c_str());
    const bool mixed = lw(c.arg(6)) == "mixed";
    const int st = std::atoi(c.arg(6).c_str());
    int k = 0;
    for (int y = y0; y < y0 + h; y++, k++)
      for (int x = x0; x < x0 + w; x++) setCrop(g, pi, x, y, (home::Crop)(k % home::CROPS), mixed ? (x - x0) % 4 : st);
    return true;
  }
  if (sub == "animal") {
    const int a = find(kAnimalWords, (int)home::Animal::COUNT, lw(c.arg(2)));
    if (a < 0) { c.fail("home animal: unknown animal '" + c.arg(2) + "'"); return true; }
    buyAnimals(c, (home::Animal)a, std::max(1, std::atoi(c.arg(3).c_str())));
    return true;
  }
  if (sub == "fill") {
    const int pi = cur(g);
    if (pi < 0) { c.fail("home fill: no plot (home lot first)"); return true; }
    const home::Plot p0 = g.home.plots[(size_t)pi];
    if (p0.kind == home::PlotKind::Lot && p0.state == home::PlotState::Owned && !startHouse(c, home::Shell::Cottage, cultureWord(g, "here"), true)) return true;
    const home::Plot p = g.home.plots[(size_t)pi];
    placeAt(c, home::Obj::Gate, p.w / 2, p.h - 1, false);
    for (int y = 0; y < p.h; y++)
      for (int x = 0; x < p.w; x++) {
        if ((x != 0 && y != 0 && x != p.w - 1 && y != p.h - 1) || (x == p.w / 2 && y == p.h - 1)) continue;
        topUp(g, g.gold + 10);
        std::string why;
        home::placeObj(g, pi, home::Obj::Fence, x, y, false, false, why);
      }
    placeAt(c, home::Obj::Coop, 1, p.h - 4, false);
    placeAt(c, home::Obj::Well, p.w - 3, p.h - 4, false);
    placeAt(c, home::Obj::Beehive, p.w - 2, 1, false);
    placeAt(c, home::Obj::Scarecrow, 2, p.h - 6, false);
    const home::Plot& ph = g.home.plots[(size_t)pi];
    for (int y = ph.hy + ph.hh + 1; y <= std::min(p.h - 2, ph.hy + ph.hh + 2); y++)
      for (int x = 3; x < std::min(p.w - 4, 9); x++) setCrop(g, pi, x, y, (home::Crop)((x + y) % 6), (x - 3) % 4);
    buyAnimals(c, home::Animal::Chicken, 3);
    buyAnimals(c, home::Animal::Dog, 1);
    return true;
  }
  if (sub == "tools") {
    for (int t = 0; t < (int)home::Tool::COUNT; t++) g.inv.push_back(home::makeTool((home::Tool)t));
    for (int k = 0; k < home::CROPS; k++) g.inv.push_back(home::makeSeeds((home::Crop)k, 5));
    return true;
  }
  if (sub == "days") {
    g.day += std::max(0, std::atoi(c.arg(2).c_str()));
    return true;
  }
  if (sub == "ride") {
    const int pi = cur(g);
    if (pi < 0) { c.fail("home ride: no plot"); return true; }
    int hi = -1;
    for (size_t i = 0; i < g.home.plots[(size_t)pi].animals.size(); i++)
      if ((home::Animal)g.home.plots[(size_t)pi].animals[i].kind == home::Animal::Horse) { hi = (int)i; break; }
    if (hi < 0) {
      bool hasStable = false;
      for (const home::PlacedObj& o : g.home.plots[(size_t)pi].outside) hasStable |= (home::Obj)o.kind == home::Obj::Stable;
      if (!hasStable) {   // a stable where it fits
        const home::Plot& p = g.home.plots[(size_t)pi];
        std::string why;
        bool ok = false;
        for (int y = 1; y + 3 < p.h && !ok; y++)
          for (int x = 1; x + 3 < p.w && !ok; x++)
            if (home::canPlaceOutside(p, home::Obj::Stable, x, y, false, why)) {
              topUp(g, g.gold + 2000);
              ok = home::placeObj(g, pi, home::Obj::Stable, x, y, false, false, why);
            }
      }
      if (!buyAnimals(c, home::Animal::Horse, 1)) return true;
      hi = (int)g.home.plots[(size_t)pi].animals.size() - 1;
    }
    std::string why;
    if (!home::mount(g, pi, hi, why)) c.fail("home ride: " + why);
    return true;
  }
  if (sub == "dismount") { g.homeDismount(home::Dismount::Player); return true; }
  if (sub == "yard" || sub == "shell") {
    const int pi = cur(g);
    if (pi < 0) { c.fail("home: no plot"); return true; }
    home::Ui& U = g.home.ui;
    U = home::Ui();
    U.plot = pi;
    U.mode = sub == "yard" ? home::UiMode::Yard : home::UiMode::Shell;
    U.sel = sub == "yard" ? (int)home::Obj::Coop : (int)home::Shell::Cottage;
    U.gx = g.home.plots[(size_t)pi].w / 2; U.gy = 2;
    if (!g.home.styles.empty()) U.style = g.home.styles.front();
    g.mode = Mode::Build;
    return true;
  }
  c.fail("home: unknown subcommand '" + c.arg(1) + "'");
  return true;
}

bool expHome(ScriptCtx& c) {
  Game& g = c.game;
  const std::string what = lw(c.arg(2));
  const int pi = cur(g);
  if (what == "plots") {
    if ((int)g.home.plots.size() != std::atoi(c.arg(3).c_str())) c.fail("expected " + c.arg(3) + " plots, got " + std::to_string(g.home.plots.size()));
  } else if (what == "riding") {
    if (g.homeRiding() != (std::atoi(c.arg(3).c_str()) != 0)) c.fail(std::string("expected riding ") + c.arg(3));
  } else if (what == "crops") {
    const int n = pi >= 0 ? (int)g.home.plots[(size_t)pi].crops.size() : 0;
    if (n != std::atoi(c.arg(3).c_str())) c.fail("expected " + c.arg(3) + " crops, got " + std::to_string(n));
  } else if (what == "stage") {
    const int x = std::atoi(c.arg(3).c_str()), y = std::atoi(c.arg(4).c_str()), s = std::atoi(c.arg(5).c_str());
    int got = -1;
    if (pi >= 0) for (const home::CropRec& r : g.home.plots[(size_t)pi].crops) if (r.x == x && r.y == y) got = r.stage;
    if (got != s) c.fail("expected stage " + c.arg(5) + " at " + c.arg(3) + "," + c.arg(4) + ", got " + std::to_string(got));
  } else if (what == "built") {
    if (pi < 0 || g.home.plots[(size_t)pi].state != home::PlotState::Built) c.fail("expected the current plot's house built");
  } else if (what == "dismounts") {
    const int k = find(kDismountWords, (int)home::Dismount::COUNT, lw(c.arg(3)));
    if (k < 0) { c.fail("expect home dismounts: unknown reason"); return true; }
    if (g.home.dismounts[k] != std::atoi(c.arg(4).c_str())) c.fail("expected " + c.arg(4) + " dismounts (" + c.arg(3) + "), got " + std::to_string(g.home.dismounts[k]));
  } else c.fail("expect home: unknown check '" + c.arg(2) + "'");
  return true;
}
// ---------------------------------------------------------------- phase B
int nearestActor(Game& g, const std::string& who) {
  int best = -1;
  float bd = 1e30f;
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& a = g.actors[i];
    if (!a.npc || !a.human || a.hostile) continue;
    bool ok = who == "any";
    if (who == "innkeeper") ok = a.role == Role::Innkeeper;
    if (who == "lord") ok = a.role == Role::Jarl;
    if (who == "farmer" || who == "stablehand" || who == "builder" || who == "miller") {
      const int si = a.site >= 0 ? a.site : (a.bldg >= 0 && a.bldg < (int)g.world.over.bldgs.size() ? g.world.over.bldgs[(size_t)a.bldg].site : -1);
      const life::Census* c = si >= 0 && g.world.sites[(size_t)si].settlement() ? g.life.find(g.world.sites[(size_t)si].id) : nullptr;
      if (!c || a.resident < 0 || a.resident >= (int)c->res.size()) continue;
      const life::Job j = c->res[(size_t)a.resident].job;
      if (who == "farmer") ok = j == life::Job::Farmer || j == life::Job::Labourer;
      if (who == "stablehand") ok = j == life::Job::Stablehand || j == life::Job::Herder;
      if (who == "builder") ok = j == life::Job::Labourer || j == life::Job::Woodcutter;
      if (who == "miller") ok = c->mill >= 0 && c->res[(size_t)a.resident].work == c->mill;
    }
    if (!ok) continue;
    const float d = len2(a.p - g.pl().p);
    if (d < bd) { bd = d; best = (int)i; }
  }
  return best;
}
std::string up(std::string s) {
  for (char& ch : s) if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
  return s;
}

bool cmdHomeB(ScriptCtx& c) {
  Game& g = c.game;
  const std::string sub = lw(c.arg(1));
  if (sub == "enter") {
    const std::string w = lw(c.arg(2));
    int bi = -1;
    if (w == "inn") {
      const Site& s = g.world.sites[(size_t)g.world.startSite];
      for (int b = s.bldgFirst; b < s.bldgFirst + s.bldgCount && bi < 0; b++) if (g.world.over.bldgs[(size_t)b].type == art::Building::Inn) bi = b;
    } else if (w == "house") {
      const int pi = cur(g);
      if (pi >= 0) {
        const home::Plot& p = g.home.plots[(size_t)pi];
        bi = g.world.bldgHandle(p.kind == home::PlotKind::House ? p.id : home::houseBldg(p, g).id);
      }
    } else if (w == "vacant") {
      float bd = 1e30f;
      for (int si : g.world.nearSites)
        for (int b : home::forSaleHouses(g.world, si)) {
          const Bldg& B = g.world.over.bldgs[(size_t)b];
          const float dx = B.doorX() * TILE - g.pl().p.x, dy = B.doorY() * TILE - g.pl().p.y, d = dx * dx + dy * dy;
          if (d < bd) { bd = d; bi = b; }
        }
    }
    if (bi < 0 || !g.debugEnterBuilding(bi, 0)) c.fail("home enter " + w + ": no such building");
    return true;
  }
  if (sub == "leave") { g.debugLeave(); return true; }
  if (sub == "stand") {   // the player stands on plot tile (x, y) of the current plot (the camera with them)
    const int pi = cur(g);
    if (pi < 0) { c.fail("home stand: no plot"); return true; }
    const home::Plot& p = g.home.plots[(size_t)pi];
    g.teleportGlobal(p.gx + std::atoi(c.arg(2).c_str()), p.gy + std::atoi(c.arg(3).c_str()));
    return true;
  }
  if (sub == "gold") { g.gold = std::max(0, std::atoi(c.arg(2).c_str())); return true; }
  if (sub == "seed") {   // keep only this crop's seeds in the pack (the use button plants the first seeds it finds)
    const int k = find(kCropWords, home::CROPS, lw(c.arg(2)));
    if (k < 0) { c.fail("home seed: unknown crop '" + c.arg(2) + "'"); return true; }
    bool have = false;
    for (int i = (int)g.inv.size() - 1; i >= 0; i--) {
      home::Crop s;
      if (!home::isSeed(g.inv[(size_t)i], s)) continue;
      if ((int)s == k) have = true;
      else HomeOps::dropAt(g, i);
    }
    if (!have) g.inv.push_back(home::makeSeeds((home::Crop)k, 10));
    return true;
  }
  if (sub == "goto") {   // inside the player's house: walk up to the bed / chest / hearth (stand beside it, facing it)
    const int pi = home::plotOfInterior(g);
    if (pi < 0) { c.fail("home goto: not in the player's house"); return true; }
    const std::string w = lw(c.arg(2));
    const home::Obj want = w == "bed" ? home::Obj::Bed : w == "hearth" ? home::Obj::Hearth : home::Obj::Chest;
    for (const home::PlacedObj& o : g.home.plots[(size_t)pi].inside) {
      if ((home::Obj)o.kind != want || o.floor() != g.subFloor) continue;
      const home::ObjInfo& oi = home::objInfo(want);
      const int ax = o.x + oi.w / 2, ay = o.y + oi.h - 1;
      static const int dx[4] = {0, 1, -1, 0}, dy[4] = {1, 0, 0, -1};
      static const int face[4] = {1, 3, 2, 0};
      for (int k = 0; k < 4; k++) {
        const int x = ax + dx[k], y = ay + dy[k];
        if (!g.sub.in(x, y) || g.sub.blocked(x, y)) continue;
        static const Vec2 v[4] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
        g.pl().p = Vec2(x * TILE + 8.0f, y * TILE + 10.0f);
        g.pl().face = face[k]; g.pl().aim = v[face[k]];
        return true;
      }
    }
    c.fail("home goto: no reachable " + w);
    return true;
  }
  if (sub == "talk") {
    const int k = nearestActor(g, lw(c.arg(2)));
    if (k < 0) { c.fail("home talk: nobody like '" + c.arg(2) + "' in play"); return true; }
    HomeOps::talkTo(g, g.actors[(size_t)k]);
    return true;
  }
  if (sub == "choose") {
    const std::string want = up(c.rest(2));
    for (size_t i = 0; i < g.dlg.opts.size(); i++)
      if (g.dlg.opts[i].label.rfind(want, 0) == 0) { g.dialogueChoose((int)i); return true; }
    std::string all;
    for (const DlgOpt& o : g.dlg.opts) all += " [" + o.label + "]";
    c.fail("home choose: no option '" + want + "' among" + all);
    return true;
  }
  if (sub == "face") {
    const std::string d = lw(c.arg(2));
    const int f = d == "north" ? 1 : d == "east" ? 2 : d == "west" ? 3 : 0;
    static const Vec2 v[4] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
    g.pl().face = f; g.pl().aim = v[f];
    return true;
  }
  if (sub == "use") {
    const int pi = cur(g);
    if (lw(c.arg(2)) != "here") {
      if (pi < 0) { c.fail("home use: no plot"); return true; }
      const home::Plot& p = g.home.plots[(size_t)pi];
      const int x = std::atoi(c.arg(2).c_str()), y = std::atoi(c.arg(3).c_str());
      g.pl().p = Vec2((float)(p.gx + x - g.world.ox) * TILE + 8.0f, (float)(p.gy + y + 1 - g.world.oy) * TILE + 10.0f);
      g.pl().face = 1; g.pl().aim = Vec2(0, -1);
    }
    HomeOps::interact(g);
    return true;
  }
  if (sub == "cook") {
    const std::string id = lw(c.arg(2)), st = lw(c.arg(3));
    const std::vector<home::Meal> ms = home::knownMeals(g);
    const std::vector<home::Meal> cm = home::cultureMeals(g.world.sites[(size_t)g.world.startSite].culture, g);
    const home::Meal* m = nullptr;
    if (id.rfind("culture", 0) == 0) {
      const int k = std::atoi(id.c_str() + 7);
      if (k >= 1 && k <= (int)cm.size()) m = &cm[(size_t)k - 1];
    } else
      for (const home::Meal& x : ms) if (x.id == id) m = &x;
    if (!m) { c.fail("home cook: unknown meal '" + id + "'"); return true; }
    // the ingredients, given if short
    const Item give[] = {makeFood(1), home::makeCropItem(home::Crop::Cabbage, 1, 0), home::makeProduct(home::Product::Flour, 1), makeFood(4),
                         home::makeProduct(home::Product::Egg, 1), home::makeProduct(home::Product::Milk, 1), home::makeProduct(home::Product::Honey, 1),
                         home::makeCropItem(home::Crop::Herbs, 1, 0), home::makeCropItem(home::Crop::Barley, 1, 0), makeFood(3), makeFood(2), makeFood(0)};
    for (int round = 0; round < 3 && !home::missingFor(g, *m).empty(); round++)
      for (const Item& it : give)
        for (home::Ingr i : m->in)
          if (i != home::Ingr::None && home::ingredientOf(it) == i) g.inv.push_back(it);
    const home::Station s = st == "campfire" ? home::Station::Campfire : st == "pot" ? home::Station::CookPot : st == "inn" ? home::Station::InnKitchen : home::Station::Hearth;
    std::string why;
    if (!home::cook(g, *m, s, why)) c.fail("home cook: " + why);
    return true;
  }
  if (sub == "eat") {
    for (int i = (int)g.inv.size() - 1; i >= 0; i--)
      if (g.inv[(size_t)i].kind == ItemKind::Food && g.inv[(size_t)i].sub >= home::FOOD_MEAL) { g.useItem(i); return true; }
    c.fail("home eat: no cooked meal in the pack");
    return true;
  }
  if (sub == "buyhorse") {
    home::Breed b = home::breedOfCulture(g.world.sites[(size_t)g.world.startSite].culture, g);
    for (int k = 0; k < (int)home::Breed::COUNT; k++)
      if (!c.arg(2).empty() && lw(home::breedInfo((home::Breed)k).name).rfind(lw(c.arg(2)), 0) == 0) b = (home::Breed)k;
    topUp(g, g.gold + home::breedInfo(b).price);
    std::string why;
    if (!home::buyHorse(g, b, g.world.startSite, why)) c.fail("home buyhorse: " + why);
    return true;
  }
  if (sub == "horsehere") {
    if (g.home.horse < 0 || g.home.riding >= 0) { c.fail("home horsehere: no horse waiting"); return true; }
    g.home.horseGx = g.world.ox + (int)(g.pl().p.x / TILE) + 1;
    g.home.horseGy = g.world.oy + (int)(g.pl().p.y / TILE);
    const int k = home::horseActor(g);
    if (k > 0) g.actors[(size_t)k].p = g.actors[(size_t)k].home = g.actors[(size_t)k].goal = g.pl().p + Vec2(TILE, 0);
    return true;
  }
  if (sub == "raid") {
    const int pi = cur(g);
    if (pi < 0) { c.fail("home raid: no plot"); return true; }
    home::forceRaid(g, pi);
    return true;
  }
  if (sub == "hire") {
    const int pi = cur(g);
    if (pi < 0) { c.fail("home hire: no plot"); return true; }
    const int days = c.arg(2).empty() ? 7 : std::atoi(c.arg(2).c_str());
    topUp(g, g.gold + home::FARMHAND_WAGE * days);
    std::string why;
    if (!home::hireFarmhand(g, pi, days, why)) c.fail("home hire: " + why);
    return true;
  }
  if (sub == "killwolves") {   // the raid's wolves fall to the player
    for (int id : g.homeRaid().wolves) g.debugFell(id, true);
    return true;
  }
  if (sub == "exposed") {
    const int pi = cur(g);
    if (pi >= 0) g.home.plots[(size_t)pi].flags |= home::PF_EXPOSED;
    return true;
  }
  return cmdHome(c);
}
EMB_SCRIPT_CMD("home", "home lot|buylot|buyhouse|build|place|fence|crop|field|animal|fill|tools|days|ride|dismount|yard|shell|enter|leave|talk|choose|use|cook|eat|buyhorse|horsehere|raid|hire|exposed|face ...: stage M7 property", cmdHomeB);

bool expHomeB(ScriptCtx& c) {
  Game& g = c.game;
  const std::string what = lw(c.arg(2));
  const int pi = cur(g);
  if (what == "horse") {
    const std::string w = lw(c.arg(3));
    const std::string got = g.home.riding >= 0 ? "riding" : g.home.horse < 0 ? "none" : g.home.horseInn ? "inn" : "waiting";
    if (got != w) c.fail("expected the horse " + w + ", it is " + got);
  } else if (what == "wellfed") {
    if (g.life.player.fedH <= 0 || g.life.player.mealQuality < 1) c.fail("expected Well Fed by a cooked meal");
  } else if (what == "recipes") {
    if ((int)g.home.recipes.size() != std::atoi(c.arg(3).c_str())) c.fail("expected " + c.arg(3) + " recipes, got " + std::to_string(g.home.recipes.size()));
  } else if (what == "lost" || what == "animals") {
    int lost = 0, n = 0;
    if (pi >= 0) for (const home::AnimalRec& a : g.home.plots[(size_t)pi].animals) { lost += (a.flags & home::AF_LOST) != 0; n++; }
    const int got = what == "lost" ? lost : n;
    if (got != std::atoi(c.arg(3).c_str())) c.fail("expected " + c.arg(3) + " " + what + ", got " + std::to_string(got));
  } else if (what == "deed") {
    bool deed = false;
    for (const Item& it : g.inv) deed |= it.kind == ItemKind::Misc && it.sub == home::MISC_DEED;
    if (!deed) c.fail("expected a deed in the pack");
  } else if (what == "store") {
    int n = 0;
    if (pi >= 0) for (const home::Store& s : g.home.plots[(size_t)pi].stores) n += (int)s.items.size();
    if (n < std::atoi(c.arg(3).c_str())) c.fail("expected at least " + c.arg(3) + " stacks in the stores, got " + std::to_string(n));
  } else if (what == "raid") {
    if (g.homeRaid().outcome != std::atoi(c.arg(3).c_str())) c.fail("expected raid outcome " + c.arg(3) + ", got " + std::to_string(g.homeRaid().outcome));
  } else if (what == "label") {
    const std::string want = up(c.rest(3)), got = home::interactLabel(g);
    if (got != want) {
      std::string who;
      const int t = g.interactTarget();
      for (const Actor& a : g.actors) if (a.id == t) who = " (facing " + a.name + ")";
      c.fail("expected the use prompt '" + want + "', got '" + got + "'" + who);
    }
  } else if (what == "beasts") {   // farm animals (and the waiting horse) in play as actors; wolves of the raid
    int n = 0, wolves = 0;
    for (const Actor& a : g.actors) n += (a.lifeBits & LB_HOMESTEAD) && a.critter;
    for (int id : g.homeRaid().wolves)
      for (const Actor& a : g.actors) wolves += a.id == id && a.st != AState::Dead;
    if (n < std::atoi(c.arg(3).c_str())) {
      std::string where;
      for (const Actor& a : g.actors)
        if ((a.lifeBits & LB_HOMESTEAD) && a.critter) where += " " + a.name + "@" + std::to_string((int)((a.p.x - g.pl().p.x) / TILE)) + "," + std::to_string((int)((a.p.y - g.pl().p.y) / TILE));
      c.fail("expected at least " + c.arg(3) + " farm beasts in play, got " + std::to_string(n) + " (wolves " + std::to_string(wolves) + "):" + where);
    }
  } else if (what == "inhouse") {
    if (home::plotOfInterior(g) < 0) c.fail("expected to be inside the player's own house");
  } else return expHome(c);
  return true;
}
EMB_SCRIPT_CMD("expect:home", "expect home plots N | riding 0|1 | crops N | stage X Y S | built | dismounts WHY N | horse none|waiting|riding|inn | wellfed | recipes N | lost N | animals N | deed | store N | raid OUTCOME | label TEXT | inhouse", expHomeB);

}  // namespace
