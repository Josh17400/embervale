// rpg_test --home [--seeds A..B] [--fuzz N]: M7 Home contract checks (VISION_PLAN 8, 13 M7, 15.22). Lead (phase A); the
// HOMESTEAD lane owns this file afterwards and extends it (it may tighten, never drop, these checks).
//   - the tables: >= 15 crops with 4 stages, 3..8 days, seasons (a winter crop), objects, animals, breeds, shells
//   - the calendar: day 1 spring, 29 summer, 57 autumn, 85 winter, 113 spring again
//   - growth: a watered crop ripens in its days, an unwatered one wilts and waits; out of season it waits
//   - offline catch-up is deterministic and cheap: catchUp(a, c) == catchUp(a, b) + catchUp(b, c); 1000 days < 5 ms
//   - placement fuzz (VISION_PLAN 13 M7 "a fuzz of 1000 placements"): random objects on a lot with a house; after every
//     accepted one an independent BFS confirms the gate still reaches the door and every usable object
//   - the save: a full farm's plot record <= 4 KB; the home block round-trips byte-identically; damaged blocks are
//     refused or load without a crash
//   - per seed, on a real Game: a lot staged by the start village, a cottage built in the local style (its Bldg in the
//     window, finished on its day), a horse bought and mounted (speed x >= 1.6, more on a road), the dismount triggers
//     (an attack, a blow, entering a building), the whole save round-trips byte-identically with the farm
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <initializer_list>
#include <string>
#include <vector>
#include "rpg/sim/game_internal.h"
#include "rpg/sim/home.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

int g_bad = 0;
void fail(const std::string& m) { out("FAIL: home: %s\n", m.c_str()); g_bad++; }

void tables() {
  if (home::CROPS < 15) fail("fewer than 15 crops");
  bool winter = false;
  for (int k = 0; k < home::CROPS; k++) {
    const home::CropInfo& c = home::cropInfo((home::Crop)k);
    if (c.days < 3 || c.days > 8) fail(std::string(c.name) + ": days outside 3..8");
    if (c.yieldMin < 1 || c.yieldMax < c.yieldMin || c.yieldMax > 3) fail(std::string(c.name) + ": yield outside 1..3");
    if (!c.seasons) fail(std::string(c.name) + ": grows in no season");
    winter |= (c.seasons & home::seasonBit(home::Season::Winter)) != 0;
    if (!c.name || !*c.name) fail("a crop without a name");
  }
  if (!winter) fail("no crop grows in winter");
  for (int o = 0; o < (int)home::Obj::COUNT; o++) {
    const home::ObjInfo& oi = home::objInfo((home::Obj)o);
    if (!(oi.flags & (home::OBJ_INSIDE | home::OBJ_OUTSIDE))) fail(std::string(oi.name) + ": neither inside nor outside");
    if ((oi.flags & home::OBJ_OUTSIDE) && !(oi.flags & home::OBJ_GROUND) && home::farmObjArt((home::Obj)o) == art::FarmObj::COUNT)
      fail(std::string(oi.name) + ": a yard object without its art");
    if ((oi.flags & home::OBJ_INSIDE) && !(oi.flags & home::OBJ_WALLDECOR) && oi.prop == art::Prop::COUNT) fail(std::string(oi.name) + ": furniture without a prop");
  }
  for (int a = 0; a < (int)home::Animal::COUNT; a++) {
    const home::AnimalInfo& ai = home::animalInfo((home::Animal)a);
    if (ai.home != home::Obj::COUNT && !home::objInfo(ai.home).capacity) fail(std::string(ai.name) + ": its home houses nobody");
  }
  for (int b = 0; b < (int)home::Breed::COUNT; b++) {
    const home::BreedInfo& bi = home::breedInfo((home::Breed)b);
    if (bi.speed < 7.0f || bi.speed > 10.0f) fail(std::string(bi.name) + ": speed outside 7..10 tiles/s");
    if (bi.price < 600 || bi.price > 3000) fail(std::string(bi.name) + ": price outside 600..3000");
  }
  for (int s = 0; s < (int)home::Shell::COUNT; s++) {
    const home::ShellInfo& si = home::shellInfo((home::Shell)s);
    const home::LotInfo& li = home::lotInfo(si.minLot);
    if (si.w + 2 > li.w || si.h + 3 > li.h) fail(std::string(si.name) + ": does not fit its smallest lot");
    if (si.days < 2 || si.days > 5) fail(std::string(si.name) + ": construction outside 2..5 days");
  }
  if (home::baseMeals().size() < 8) fail("fewer than 8 base meals");
  // the calendar
  using S = home::Season;
  if (home::seasonOf(1) != S::Spring || home::seasonOf(29) != S::Summer || home::seasonOf(57) != S::Autumn || home::seasonOf(85) != S::Winter ||
      home::seasonOf(113) != S::Spring || home::seasonOf(112) != S::Winter)
    fail("seasons are not 4 x 28 days from day 1");
  // items: seeds and harvests round-trip their identity
  for (int k = 0; k < home::CROPS; k++) {
    home::Crop c;
    if (!home::isSeed(home::makeSeeds((home::Crop)k, 3), c) || (int)c != k) fail("makeSeeds / isSeed disagree");
    if (home::goodOf(home::makeCropItem((home::Crop)k, 2, 1)) == ew::Good::COUNT) fail("a harvest that trades as no good");
  }
}

home::Plot farm(uint64_t seed) {
  home::Plot p;
  p.kind = home::PlotKind::Lot; p.state = home::PlotState::Built; p.id = ew::makeId(3, -2, ew::IdKind::Plot, 7);
  p.gx = 1000; p.gy = -400; p.w = 16; p.h = 12;
  p.shell = (uint8_t)home::Shell::Cottage; p.hx = 5; p.hy = 1; p.hw = 5; p.hh = 4; p.style = 0x1234; p.houseSeed = 99;
  p.name = "BRYNJA'S FARM";
  Rng r(seed);
  for (int x = 0; x < p.w; x++) for (int y : {0, (int)p.h - 1}) { home::PlacedObj o; o.kind = (uint8_t)(x == p.w / 2 && y ? home::Obj::Gate : home::Obj::Fence); o.x = (uint8_t)x; o.y = (uint8_t)y; p.outside.push_back(o); }
  for (int y = 1; y < p.h - 1; y++) for (int x : {0, (int)p.w - 1}) { home::PlacedObj o; o.kind = (uint8_t)home::Obj::Fence; o.x = (uint8_t)x; o.y = (uint8_t)y; p.outside.push_back(o); }
  for (int y = 7; y < 10; y++)
    for (int x = 1; x < 15; x++) {
      home::setGround(p, x, y, 2);
      home::CropRec c;
      c.x = (uint8_t)x; c.y = (uint8_t)y;
      const uint32_t k = r.next();
      c.kind = (uint8_t)(k % home::CROPS);
      c.plantedDay = 1; c.lastGrowDay = 1;
      p.crops.push_back(c);
    }
  for (int i = 0; i < 12; i++) {
    home::AnimalRec a;
    a.kind = (uint8_t)(i % 5); a.seed = r.next(); a.boughtDay = 1; a.lastFedDay = 1; a.lastProduceDay = 1;
    p.animals.push_back(a);
  }
  p.trough = 7;
  p.farmhand = 1; p.farmhandUntil = 40;
  home::Store chest;
  for (int i = 0; i < 12; i++) chest.items.push_back(home::makeCropItem((home::Crop)(i % home::CROPS), 3, 1));
  p.stores.push_back(chest);
  p.lastSeenDay = 1;
  return p;
}

void growth() {
  home::Plot p;
  p.id = 77; p.gx = 0; p.gy = 0; p.w = 8; p.h = 6;
  home::CropRec c;
  c.x = 1; c.y = 1; c.kind = (uint8_t)home::Crop::Wheat; c.plantedDay = 1; c.lastGrowDay = 1;
  p.crops.push_back(c);
  // the farmhand waters every day: ripe in its days
  p.farmhand = 1; p.farmhandUntil = 200;
  home::Plot q = p;
  q.farmhand = 0;   // nobody waters (rain may): it waits and wilts
  home::catchUp(p, 1, 1 + home::cropInfo(home::Crop::Wheat).days, 5);
  if (p.crops.empty() || p.crops[0].stage != 3) fail("a watered wheat crop is not ripe after its days");
  // (the farmhand did not harvest: no store) -- and with a store it lands in it
  home::Plot s = p;
  s.stores.push_back(home::Store());
  home::catchUp(s, s.lastSeenDay, s.lastSeenDay + 1, 5);
  if (!s.crops.empty() || s.stores[0].items.empty()) fail("the farmhand did not harvest a ripe crop into the store");
  // dry: find a seed with no rain for a week at (0,0) after day 1, then nothing grows
  uint64_t dry = 0;
  for (uint64_t sd = 1; sd < 5000 && !dry; sd++) {
    bool any = false;
    for (int d = 2; d <= 8; d++) any |= home::rainDay(sd, 0, 0, d);
    if (!any) dry = sd;
  }
  if (!dry) fail("no dry week in 5000 seeds (rainDay too wet)");
  else {
    home::catchUp(q, 1, 8, dry);
    if (q.crops.empty() || q.crops[0].stage != 0 || !q.crops[0].wilted(8)) fail("an unwatered crop grew or did not wilt");
  }
  // out of season: wheat planted in winter waits
  home::Plot w = p;
  w.crops[0] = c;
  w.crops[0].plantedDay = 86; w.crops[0].lastGrowDay = 86;
  w.lastSeenDay = 86;
  home::catchUp(w, 86, 100, 5);
  if (w.crops[0].grown != 0) fail("wheat grew in winter");
}

void determinism() {
  for (uint64_t sd : {1ull, 7ull, 99ull}) {
    home::Plot a = farm(sd), b = farm(sd);
    a.flags |= home::PF_EXPOSED; b.flags |= home::PF_EXPOSED;
    home::catchUp(a, 1, 60, sd);
    home::catchUp(b, 1, 25, sd);
    home::catchUp(b, 25, 60, sd);
    home::Homes ha, hb;
    ha.plots.push_back(a); hb.plots.push_back(b);
    std::vector<uint8_t> ba, bb;
    ha.serialize(ba); hb.serialize(bb);
    if (ba != bb) fail("catchUp(1, 60) != catchUp(1, 25) + catchUp(25, 60)");
  }
  home::Plot p = farm(3);
  const auto t0 = std::chrono::steady_clock::now();
  home::catchUp(p, 1, 1001, 3);
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  if (ms > 5.0) fail("a 1000-day catch-up took " + std::to_string(ms) + " ms (over 5)");
  out("home: 1000-day catch-up of a full farm %.2f ms\n", ms);
}

// an independent check of the yard rules: the gate reaches the door step and every usable object's side
bool yardOk(const home::Plot& p) {
  std::vector<uint8_t> g((size_t)p.w * p.h, 0);
  auto mark = [&](int x0, int y0, int w, int h) {
    for (int y = y0; y < y0 + h; y++) for (int x = x0; x < x0 + w; x++) if (x >= 0 && y >= 0 && x < p.w && y < p.h) g[(size_t)y * p.w + x] = 1;
  };
  mark(p.hx, p.hy, p.hw, p.hh);
  int dx, dy;
  home::doorOf(p, dx, dy);
  for (const home::PlacedObj& o : p.outside) {
    const home::ObjInfo& oi = home::objInfo((home::Obj)o.kind);
    if (!(oi.flags & home::OBJ_SOLID)) continue;
    int w = oi.w, h = oi.h;
    if (o.turned() && (oi.flags & home::OBJ_ROTATES)) std::swap(w, h);
    mark(o.x, o.y, w, h);
  }
  int gx, gy;
  home::gateOf(p, gx, gy);
  if (g[(size_t)gy * p.w + gx]) return false;
  std::vector<uint8_t> seen(g.size(), 0);
  std::deque<int> q{gy * p.w + gx};
  seen[(size_t)(gy * p.w + gx)] = 1;
  while (!q.empty()) {
    const int i = q.front();
    q.pop_front();
    const int x = i % p.w, y = i / p.w;
    for (int k = 0; k < 4; k++) {
      const int nx = x + (k == 0) - (k == 1), ny = y + (k == 2) - (k == 3);
      if (nx < 0 || ny < 0 || nx >= p.w || ny >= p.h || g[(size_t)ny * p.w + nx] || seen[(size_t)ny * p.w + nx]) continue;
      seen[(size_t)ny * p.w + nx] = 1;
      q.push_back(ny * p.w + nx);
    }
  }
  if (dx >= 0 && dy + 1 < p.h && !seen[(size_t)(dy + 1) * p.w + dx]) return false;
  return true;
}

void fuzz(int n) {
  int placed = 0, refused = 0, broken = 0;
  for (int round = 0; round * 100 < n; round++) {
    home::Plot p;
    p.kind = home::PlotKind::Lot; p.state = home::PlotState::Built; p.id = 900 + round;
    p.w = 13; p.h = 10; p.shell = (uint8_t)home::Shell::Cottage; p.hx = 4; p.hy = 1; p.hw = 5; p.hh = 4;
    Rng r(0xF022u + (uint32_t)round * 7919u);
    for (int i = 0; i < 100 && round * 100 + i < n; i++) {
      int o = 0;
      do { o = (int)(r.next() % (uint32_t)home::Obj::COUNT); } while (!(home::objInfo((home::Obj)o).flags & home::OBJ_OUTSIDE));
      const uint32_t rx = r.next();
      const uint32_t ry = r.next();
      const uint32_t rt = r.next();
      const int x = (int)(rx % p.w), y = (int)(ry % p.h);
      const bool turned = (rt & 1) != 0;
      std::string why;
      if (!home::canPlaceOutside(p, (home::Obj)o, x, y, turned, why)) { refused++; continue; }
      if (home::objInfo((home::Obj)o).flags & home::OBJ_GROUND) { home::setGround(p, x, y, o == (int)home::Obj::Path ? 1 : 2); placed++; continue; }
      home::PlacedObj po;
      po.kind = (uint8_t)o; po.x = (uint8_t)x; po.y = (uint8_t)y; po.flags = turned ? 1 : 0;
      p.outside.push_back(po);
      placed++;
      if (!yardOk(p)) { broken++; p.outside.pop_back(); }
    }
  }
  out("home: placement fuzz %d tries: %d placed, %d refused, %d broke the yard\n", n, placed, refused, broken);
  if (broken) fail(std::to_string(broken) + " accepted placements cut the gate off from the door");
  if (placed < n / 20) fail("the fuzz placed almost nothing (" + std::to_string(placed) + ")");
}

void saveBlock() {
  home::Plot p = farm(42);
  home::catchUp(p, 1, 30, 42);
  // a full farm: the store full of stacks too
  while (p.stores[0].items.size() < (size_t)home::STORE_STACKS) p.stores[0].items.push_back(home::makeProduct(home::Product::Wool, 2));
  const int bytes = home::Homes::plotBytes(p);
  out("home: a full farm's plot record is %d bytes (budget %d)\n", bytes, home::PLOT_BYTES_BUDGET);
  if (bytes > home::PLOT_BYTES_BUDGET) fail("a full farm's plot record is over 4 KB");
  home::Homes h;
  h.plots.push_back(p);
  h.styles = {0x11, 0x22};
  h.recipes = {"4660.1", "4660.3"};
  h.riding = 3; h.ridingBreed = 2; h.ridingSeed = 1234;
  std::vector<uint8_t> a, b;
  h.serialize(a);
  home::Homes h2;
  if (!h2.deserialize(a)) { fail("the home block did not load"); return; }
  h2.serialize(b);
  if (a != b) fail("the home block does not round-trip byte-identically");
  uint64_t x = 0xBADull;
  int loaded = 0;
  for (int i = 0; i < 400; i++) {
    std::vector<uint8_t> bad = a;
    x = ew::mix64(x);
    if (i % 2) bad.resize((size_t)(x % bad.size()));
    else for (int k = 0; k < 1 + (int)(x % 5); k++) bad[(size_t)(ew::mix64(x + k) % bad.size())] ^= (uint8_t)(1 + (x >> 9) % 255);
    home::Homes d;
    if (d.deserialize(bad)) {
      loaded++;
      for (home::Plot& q : d.plots) home::catchUp(q, q.lastSeenDay, q.lastSeenDay + 20, 1);
    }
  }
  out("home: damaged home blocks: 400 tried, %d loaded and caught up\n", loaded);
}

void play(uint64_t seed) {
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.noWildSpawns = true;
  g.update(SIM_DT, Input());
  const int32_t px = g.world.ox + (int32_t)(g.pl().p.x / TILE), py = g.world.oy + (int32_t)(g.pl().p.y / TILE);
  const int pi = home::debugLot(g, px + 3, py - 5, home::LotSize::Medium);
  if (pi < 0) { fail("debugLot refused"); return; }
  const uint64_t cul = g.world.src->cultureAt(px, py);
  if (cul && !g.home.knowsStyle(cul)) g.home.styles.push_back(cul);
  g.gold = 100000;
  std::string why;
  if (!home::startBuild(g, pi, home::Shell::Cottage, cul, 0, 0, why)) { fail("startBuild: " + why); return; }
  g.update(SIM_DT, Input());
  const Bldg hb = home::houseBldg(g.home.plots[(size_t)pi], g);
  const int bi = g.world.bldgHandle(hb.id);
  if (bi < 0) fail("the house under construction is not in the window");
  else if (g.world.over.bldgs[(size_t)bi].home != 3) fail("the house under construction is not marked as a building site");
  g.day += home::shellInfo(home::Shell::Cottage).days;
  g.update(SIM_DT, Input());
  if (g.home.plots[(size_t)pi].state != home::PlotState::Built) fail("the house did not finish on its day");
  else if (bi >= 0 && g.world.over.bldgs[(size_t)bi].home != 1) fail("the finished house is not marked built");
  if (bi >= 0 && !g.world.over.bldgs[(size_t)bi].styled) fail("the house was not built in a culture's style");
  // a stable, a horse, riding
  bool st = false;
  const home::Plot& P = g.home.plots[(size_t)pi];
  for (int y = 1; y + 3 < P.h && !st; y++)
    for (int x = 1; x + 3 < P.w && !st; x++)
      if (home::canPlaceOutside(P, home::Obj::Stable, x, y, false, why)) st = home::placeObj(g, pi, home::Obj::Stable, x, y, false, false, why);
  if (!st) { fail("no room for a stable on a medium lot with a cottage"); return; }
  if (!home::buyAnimal(g, pi, home::Animal::Horse, why)) { fail("buyAnimal horse: " + why); return; }
  const int hi = (int)g.home.plots[(size_t)pi].animals.size() - 1;
  auto ride = [&] { std::string w; if (!home::mount(g, pi, hi, w)) fail("mount: " + w); };
  ride();
  if (!g.homeRiding()) { fail("not riding after mount"); return; }
  if (g.homeSpeedMul() < 1.6f) fail("the horse is not faster than walking by 60 %");
  // an attack gets the rider off
  Input atk;
  atk.attack = true;
  g.update(SIM_DT, atk);
  for (int i = 0; i < 30; i++) g.update(SIM_DT, Input());
  if (g.homeRiding() || g.home.dismounts[(int)home::Dismount::Attack] != 1) fail("an attack did not dismount the rider");
  // a blow
  ride();
  const int wolf = g.debugSpawnAt(art::Monster::Wolf, g.pl().p + Vec2(10, 0), 3);
  (void)wolf;
  for (int i = 0; i < 600 && g.homeRiding(); i++) g.update(SIM_DT, Input());
  if (g.homeRiding() || g.home.dismounts[(int)home::Dismount::Hurt] < 1) fail("a blow did not dismount the rider");
  g.godMode = true;
  for (int i = 0; i < 240; i++) { Input in; in.attack = (i % 10) == 0; g.update(SIM_DT, in); }
  g.godMode = false;
  // entering a building
  ride();
  const Site& sv = g.world.sites[(size_t)g.world.startSite];
  int any = -1;
  for (int b = sv.bldgFirst; b < sv.bldgFirst + sv.bldgCount && any < 0; b++) if (g.world.over.bldgs[(size_t)b].type == art::Building::Inn) any = b;
  if (any >= 0) {
    g.debugEnterBuilding(any, 0);
    if (g.homeRiding() || g.home.dismounts[(int)home::Dismount::Enter] < 1) fail("entering a building did not dismount the rider");
    g.debugLeave();
  }
  // cooking (15.2): venison and a cabbage make a stew at a hearth; eating it gives Well Fed
  {
    g.inv.push_back(makeFood(1));
    g.inv.push_back(home::makeCropItem(home::Crop::Cabbage, 1, 0));
    const std::vector<home::Meal> ms = home::knownMeals(g);
    const home::Meal* stew = nullptr;
    for (const home::Meal& m : ms) if (m.id == "stew") stew = &m;
    std::string w;
    if (!stew) fail("no stew among the known meals");
    else if (!home::cook(g, *stew, home::Station::Hearth, w)) fail("cook stew: " + w);
    else {
      int mi = -1;
      for (int i = 0; i < (int)g.inv.size(); i++) if (g.inv[(size_t)i].kind == ItemKind::Food && g.inv[(size_t)i].sub >= home::FOOD_MEAL) mi = i;
      if (mi < 0) fail("the cooked stew is not in the pack");
      else {
        g.life.player.fedH = 0;
        g.useItem(mi);
        if (g.life.player.fedH < 3.0f) fail("a cooked stew did not give Well Fed");
      }
      // (M7 fix r3) where it was cooked shows: a campfire stew is less than a hearth's, a hearth's less than an inn's
      // (the M5 snack rule no longer lifts every hearty meal to the same 6 h and 2 pips)
      float hrs[3] = {};
      int qs[3] = {};
      const home::Station sts[3] = {home::Station::Campfire, home::Station::Hearth, home::Station::InnKitchen};
      for (int k = 0; k < 3; k++) {
        g.life.player.fedH = 0;
        g.life.player.mealQuality = 0;
        g.inv.push_back(home::makeMeal(*stew, sts[k]));
        g.useItem((int)g.inv.size() - 1);
        hrs[k] = g.life.player.fedH;
        qs[k] = g.life.player.mealQuality;
      }
      if (!(hrs[0] < hrs[1] && hrs[1] < hrs[2]) || !(qs[0] < qs[1] && qs[1] < qs[2]))
        fail("Well Fed does not follow the station: campfire " + std::to_string(hrs[0]) + " h q" + std::to_string(qs[0]) + ", hearth " +
             std::to_string(hrs[1]) + " h q" + std::to_string(qs[1]) + ", inn " + std::to_string(hrs[2]) + " h q" + std::to_string(qs[2]));
    }
  }
  // the farm in the save
  home::Plot& F = g.home.plots[(size_t)pi];
  for (int x = 1; x < 6; x++) {
    home::setGround(F, x, F.h - 3, 2);
    home::CropRec c;
    c.x = (uint8_t)x; c.y = (uint8_t)(F.h - 3); c.kind = (uint8_t)home::Crop::Barley; c.plantedDay = (uint16_t)g.day; c.lastGrowDay = (uint16_t)g.day;
    F.crops.push_back(c);
  }
  std::vector<uint8_t> a, b;
  g.serialize(a);
  Game h(1);
  if (!h.deserialize(a)) { fail("a save with a farm did not load"); return; }
  h.serialize(b);
  if (a != b) fail("a save with a farm does not round-trip byte-identically");
  if (h.home.plots.size() != g.home.plots.size() || h.home.plots[(size_t)pi].crops.size() != 5) fail("the farm did not come back with the save");
  h.mode = Mode::Play;
  h.update(SIM_DT, Input());
  if (h.world.bldgHandle(hb.id) < 0) fail("the loaded game did not stamp the house into the window");
}

// ================================================================ phase B (HOMESTEAD lane)
int g_sold = 0, g_lotsBought = 0, g_lotsSkipped = 0;

int optIndex(const Game& g, const std::string& prefix) {
  for (size_t i = 0; i < g.dlg.opts.size(); i++) if (g.dlg.opts[i].label.rfind(prefix, 0) == 0) return (int)i;
  return -1;
}
void settle(Game& g, int steps) { for (int i = 0; i < steps; i++) g.update(SIM_DT, Input()); }

// harvest yields by quality (pure): a better-kept crop never yields less on average, a prize crop more
void yieldsByQuality() {
  for (int k = 0; k < home::CROPS; k++) {
    const home::CropInfo& ci = home::cropInfo((home::Crop)k);
    double avg[4] = {};
    for (int q = 0; q < 4; q++)
      for (uint64_t r = 0; r < 400; r++) avg[q] += home::harvestYield(ci, q, ew::mix64(r * 7919ull + (uint64_t)k));
    for (int q = 1; q < 4; q++) if (avg[q] + 1e-9 < avg[q - 1]) fail(std::string(ci.name) + ": a better crop yields less");
    if (avg[3] <= avg[0]) fail(std::string(ci.name) + ": a prize crop yields no more than a poor one");
    if (home::harvestYield(ci, 0, 0) < ci.yieldMin) fail(std::string(ci.name) + ": a harvest below its least");
  }
  // the beehive makes honey outside winter, and its comb is deterministic over split spans
  home::Plot a;
  a.id = 5; a.w = 8; a.h = 6;
  home::PlacedObj hive;
  hive.kind = (uint8_t)home::Obj::Beehive; hive.x = 2; hive.y = 2; hive.data = 1u << 8;
  a.outside.push_back(hive);
  home::Plot b = a;
  home::catchUp(a, 1, 20, 3);
  home::catchUp(b, 1, 9, 3);
  home::catchUp(b, 9, 20, 3);
  if (home::honeyOf(a.outside[0]) != home::HONEY_MAX || a.outside[0].data != b.outside[0].data) fail("the beehive's honey is wrong or not deterministic");
}

// the property on a real game: a deed bought by dialogue, a lot, the builder, the house's interior, animals and
// raids, the farmhand, cooking, horses, every dismount
void playB(uint64_t seed) {
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.noWildSpawns = true;
  g.update(SIM_DT, Input());
  g.gold = 200000;
  std::string why;
  auto SV = [&]() -> const Site& { return g.world.sites[(size_t)g.world.startSite]; };   // (sites grow: never keep a reference)
  // ---- a vacant house bought from the innkeeper by the options' text
  {
    const std::vector<int> sale = home::forSaleHouses(g.world, g.world.startSite);
    int inn = -1;
    for (int b = SV().bldgFirst; b < SV().bldgFirst + SV().bldgCount && inn < 0; b++) if (g.world.over.bldgs[(size_t)b].type == art::Building::Inn) inn = b;
    if (!sale.empty() && inn >= 0) {
      // nobody lives in it: the census leaves it empty
      if (const life::Census* c = g.life.census(g.world, g.world.startSite))
        for (const life::Resident& r : c->res)
          if (r.home >= 0 && SV().bldgFirst + r.home == sale[0]) { fail("a resident lives in a house for sale"); break; }
      g.debugEnterBuilding(inn, 0);
      settle(g, 3);
      Actor* keeper = nullptr;
      for (Actor& a : g.actors) if (a.role == Role::Innkeeper) keeper = &a;
      if (!keeper) fail("no innkeeper in the start village's inn");
      else {
        HomeOps::talkTo(g, *keeper);
        const int o = optIndex(g, "PROPERTY FOR SALE");
        if (o < 0) fail("the village innkeeper does not sell the vacant house");
        else {
          g.dialogueChoose(o);
          const int b = optIndex(g, "BUY THE ");
          if (b < 0) fail("no BUY THE ... option among the property for sale");
          else {
            const int before = (int)g.home.plots.size(), gold0 = g.gold;
            g.dialogueChoose(b);
            bool deed = false;
            for (const Item& it : g.inv) deed |= it.kind == ItemKind::Misc && it.sub == home::MISC_DEED;
            if ((int)g.home.plots.size() != before + 1 || g.gold >= gold0 || !deed) fail("buying the house by dialogue gave no plot, cost nothing or gave no deed");
            else g_sold++;
          }
        }
        g.closeDialogue();
      }
      g.debugLeave();
      settle(g, 2);
      // inside the bought house: nobody there, its furniture is the player's, a chest and a bed of their own
      if (!g.home.plots.empty() && g.home.plots.back().kind == home::PlotKind::House) {
        const int pi = (int)g.home.plots.size() - 1;
        const int bi = g.world.bldgHandle(g.home.plots[(size_t)pi].id);
        g.debugEnterBuilding(bi, 0);
        settle(g, 2);
        for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].npc && g.actors[k].human) { fail("someone lives in the house the player bought"); break; }
        const home::Plot& P = g.home.plots[(size_t)pi];
        if (!(P.furnished & 1)) fail("the bought house's furniture was not taken over");
        bool bed = false, store = false;
        for (const home::PlacedObj& o : P.inside) {
          if ((home::Obj)o.kind == home::Obj::Bed) bed = true;
          if (home::objInfo((home::Obj)o.kind).flags & home::OBJ_STORAGE) store = true;
        }
        if (!store && P.stores.empty()) fail("the bought house has no store");
        (void)bed;
        g.debugLeave();
      }
    }
  }
  // ---- a generated lot bought (World::lots: the LAND lane's)
  {
    int best = -1;
    for (int i = 0; i < (int)g.world.lots.size() && best < 0; i++) if (g.home.plotById(g.world.lots[(size_t)i].id) < 0) best = i;
    if (best < 0) g_lotsSkipped++;
    else if (!home::buyLot(g, best, why)) fail("buyLot of a generated lot: " + why);
    else {
      g_lotsBought++;
      const home::Plot& L = g.home.plots.back();
      if (L.gx != g.world.lots[(size_t)best].gx || L.w != g.world.lots[(size_t)best].w) fail("the bought lot is not the generated one");
    }
  }
  // ---- the builder: the dialogue opens the shell screen; the site has builders by day; the house has a starter bed and chest
  const int32_t px = g.world.ox + (int32_t)(g.pl().p.x / TILE), py = g.world.oy + (int32_t)(g.pl().p.y / TILE);
  const int pi = home::debugLot(g, px + 4, py - 6, home::LotSize::Large);
  if (pi < 0) { fail("debugLot refused (phase B)"); return; }
  const uint64_t cul = g.world.src->cultureAt(px, py);
  if (cul && !g.home.knowsStyle(cul)) g.home.styles.push_back(cul);
  {
    int someone = -1;
    for (const Actor& a : g.actors) if (a.npc && a.human) { someone = a.id; break; }
    if (someone >= 0) {
      g.dlg = Dialogue();
      g.dlg.actor = someone;
      g.dlg.opts.push_back({"BUILD A HOUSE ON MY PLOT", DLG_HOME + 30, pi});
      g.mode = Mode::Dialogue;
      g.dialogueChoose(0);
      if (g.mode != Mode::Build || g.home.ui.mode != home::UiMode::Shell || g.home.ui.plot != pi) fail("the builder's option did not open the shell screen");
      g.mode = Mode::Play;
      g.home.ui = home::Ui();
    }
  }
  g.inv.push_back(craft::makeStuff(craft::Stuff::Timber, 20));
  const int full = home::buildPrice(g, pi, home::Shell::Cottage, 0, 0), less = home::buildPrice(g, pi, home::Shell::Cottage, 20, 0);
  if (less >= full) fail("bringing timber does not cut the builders' price");
  g.hour = 10.0f;
  if (!home::startBuild(g, pi, home::Shell::Cottage, cul, 20, 0, why)) { fail("startBuild (phase B): " + why); return; }
  settle(g, 40);
  int builders = 0;
  for (const Actor& a : g.actors) builders += (a.lifeBits & LB_HOMESTEAD) && a.human && a.posture == art::Posture::Hammer;
  if (builders < 1) fail("no builders hammer at the building site by day");
  g.day += home::shellInfo(home::Shell::Cottage).days;
  settle(g, 2);
  if (g.home.plots[(size_t)pi].state != home::PlotState::Built) { fail("the builder's house did not finish"); return; }
  settle(g, 30);
  for (const Actor& a : g.actors) if ((a.lifeBits & LB_HOMESTEAD) && a.human && a.posture == art::Posture::Hammer) { fail("builders still hammer at a finished house"); break; }
  const Bldg hb = home::houseBldg(g.home.plots[(size_t)pi], g);
  const int hbi = g.world.bldgHandle(hb.id);
  if (hbi < 0) { fail("the built house is not in the window"); return; }
  g.debugEnterBuilding(hbi, 0);
  if (home::plotOfInterior(g) != pi) fail("plotOfInterior does not know the player's house");
  {
    const home::Plot& P = g.home.plots[(size_t)pi];
    int bx = -1, by = -1, chest = 0;
    for (const home::PlacedObj& o : P.inside) {
      if ((home::Obj)o.kind == home::Obj::Bed) { bx = o.x; by = o.y + 1; }
      chest += (home::Obj)o.kind == home::Obj::Chest;
    }
    if (bx < 0 || !chest) fail("the finished house has no starter bed and chest");
    else if (g.sub.propAt(bx, by) != (int)art::Prop::Bed + 1) fail("the starter bed is not stamped into the house");
    // ---- the decorate fuzz: 300 tries on the real floor; an independent BFS from the door to the stairs, beds and stores
    int placed = 0, broken = 0;
    Rng r(0xDEC0u + (uint32_t)seed);
    for (int i = 0; i < 300; i++) {
      int o = 0;
      do { o = (int)(r.next() % (uint32_t)home::Obj::COUNT); } while (!(home::objInfo((home::Obj)o).flags & home::OBJ_INSIDE));
      const uint32_t rx = r.next();
      const uint32_t ry = r.next();
      const uint32_t rt = r.next();
      const int x = (int)(rx % (uint32_t)g.sub.w), y = (int)(ry % (uint32_t)g.sub.h);
      const bool turned = (rt & 1) != 0;
      if (!home::canPlaceInside(g.home.plots[(size_t)pi], g.sub, (home::Obj)o, x, y, turned, why)) continue;
      if (!home::placeObj(g, pi, (home::Obj)o, x, y, turned, true, why)) { fail("canPlaceInside accepted what placeObj refused: " + why); continue; }
      placed++;
      // the independent check
      const Map& m = g.sub;
      std::vector<uint8_t> seen((size_t)m.w * m.h, 0);
      std::deque<int> q;
      const int sx = m.exitX, sy = m.exitY - 1;
      if (!m.in(sx, sy) || m.blocked(sx, sy)) { broken++; continue; }
      q.push_back(sy * m.w + sx);
      seen[(size_t)(sy * m.w + sx)] = 1;
      while (!q.empty()) {
        const int c = q.front();
        q.pop_front();
        for (int k = 0; k < 4; k++) {
          const int nx = c % m.w + (k == 0) - (k == 1), ny = c / m.w + (k == 2) - (k == 3);
          if (!m.in(nx, ny) || seen[(size_t)(ny * m.w + nx)] || m.blocked(nx, ny)) continue;
          seen[(size_t)(ny * m.w + nx)] = 1;
          q.push_back(ny * m.w + nx);
        }
      }
      auto reach = [&](int ox, int oy, int ow, int oh) {
        for (int yy = oy - 1; yy <= oy + oh; yy++)
          for (int xx = ox - 1; xx <= ox + ow; xx++)
            if (!(xx >= ox && xx < ox + ow && yy >= oy && yy < oy + oh) && m.in(xx, yy) && seen[(size_t)(yy * m.w + xx)]) return true;
        return false;
      };
      bool ok = m.up.x < 0 || reach(m.up.x, m.up.y, 1, 1);
      for (const home::PlacedObj& po : g.home.plots[(size_t)pi].inside) {
        if (po.floor() != 0 || !(home::objInfo((home::Obj)po.kind).flags & (home::OBJ_BED | home::OBJ_STORAGE))) continue;
        int w = home::objInfo((home::Obj)po.kind).w, h = home::objInfo((home::Obj)po.kind).h;
        if (po.turned() && (home::objInfo((home::Obj)po.kind).flags & home::OBJ_ROTATES)) std::swap(w, h);
        ok = ok && reach(po.x, po.y, w, h);
      }
      if (!ok) broken++;
    }
    out("home: seed %llu decorate fuzz: 300 tries, %d placed, %d broke the house\n", (unsigned long long)seed, placed, broken);
    if (broken) fail(std::to_string(broken) + " furniture placements cut a bed, store or the stairs off");
    if (placed < 3) fail("the decorate fuzz placed almost nothing");
    // the player's own bed: theirs to sleep in, and home is where they wake
    if (bx >= 0) {
      int bedX = -1, bedY = -1;
      for (const home::PlacedObj& o : g.home.plots[(size_t)pi].inside) if ((home::Obj)o.kind == home::Obj::Bed) { bedX = o.x; bedY = o.y + 1; break; }
      if (!g.bedIsYours(bedX, bedY)) fail("the player's own bed is not theirs to sleep in");
    }
  }
  // cooking stations: a hearth at home is a Hearth; an inn's is its kitchen; the meal is better at the inn than at a campfire
  {
    home::Station st;
    if (!home::stationOf(g, art::Prop::Hearth, st) || st != home::Station::Hearth) fail("a hearth at home is not a hearth station");
  }
  g.debugLeave();
  {
    home::Station st;
    if (!home::stationOf(g, art::Prop::Campfire, st) || st != home::Station::Campfire) fail("a campfire is not a cooking place");
    if (home::stationOf(g, art::Prop::Hearth, st)) fail("a hearth prop outdoors counts as a kitchen");
    int inn = -1;
    for (int b = SV().bldgFirst; b < SV().bldgFirst + SV().bldgCount && inn < 0; b++) if (g.world.over.bldgs[(size_t)b].type == art::Building::Inn) inn = b;
    if (inn >= 0) {
      g.debugEnterBuilding(inn, 0);
      if (!home::stationOf(g, art::Prop::Hearth, st) || st != home::Station::InnKitchen) fail("an inn's hearth is not its kitchen");
      // a culture recipe taught by the innkeeper
      Actor* keeper = nullptr;
      for (Actor& a : g.actors) if (a.role == Role::Innkeeper) keeper = &a;
      if (keeper && SV().culture) {
        HomeOps::talkTo(g, *keeper);
        const int o = optIndex(g, "TEACH ME A RECIPE");
        if (o < 0) fail("the innkeeper teaches no recipe");
        else {
          g.dialogueChoose(o);
          const int l = optIndex(g, "LEARN ");
          const size_t n0 = g.home.recipes.size();
          if (l >= 0) g.dialogueChoose(l);
          if (g.home.recipes.size() != n0 + 1) fail("learning a recipe from the innkeeper did not teach it");
        }
        g.closeDialogue();
      }
      g.debugLeave();
    }
    const home::Meal& stew = home::baseMeals()[5];
    if (home::makeMeal(stew, home::Station::InnKitchen).tier <= home::makeMeal(stew, home::Station::Campfire).tier) fail("an inn's kitchen cooks no better than a campfire");
    const std::vector<home::Meal> cm = home::cultureMeals(SV().culture, g);
    if (SV().culture && (cm.size() != 3 || cm[0].name == cm[1].name || cm[1].name == cm[2].name)) fail("a culture has not 3 distinct signature recipes");
    // fish from the existing foods count as fish
    if (home::ingredientOf(makeFood(4)) != home::Ingr::Fish) fail("a river trout is not fish to the cook");
  }
  // back to the farm: the inn can be far across a big town (out of the yard actors' range)
  g.debugEnterBuilding(hbi, 0);
  g.debugLeave();
  // ---- animals, the farmhand and raids on the yard (no fence ring: exposed when a den is near or in the wildlands)
  home::Plot& F = g.home.plots[(size_t)pi];
  {
    auto place = [&](home::Obj o) {
      for (int y = 1; y + 2 < F.h; y++)
        for (int x = 1; x + 3 < F.w; x++)
          if (home::placeObj(g, pi, o, x, y, false, false, why)) return true;
      return false;
    };
    if (!place(home::Obj::Pen) || !place(home::Obj::Coop) || !place(home::Obj::ShippingCrate) || !place(home::Obj::Stable)) { fail("no room on a large lot for a pen, coop, crate and stable"); return; }
    for (int i = 0; i < 2; i++) home::buyAnimal(g, pi, home::Animal::Chicken, why);
    if (!home::buyAnimal(g, pi, home::Animal::Cow, why)) fail("buy a cow: " + why);
    home::buyAnimal(g, pi, home::Animal::Sheep, why);
    g.hour = 11.0f;
    settle(g, 30);
    int beasts = 0;
    for (const Actor& a : g.actors) beasts += (a.lifeBits & LB_HOMESTEAD) && a.critter != 0;
    if (beasts < 3) fail("the farm animals are not in the yard (" + std::to_string(beasts) + " actors)");
    for (const Actor& a : g.actors)
      if ((a.lifeBits & LB_HOMESTEAD) && a.critter) {
        const int32_t ax = g.world.ox + (int32_t)std::floor(a.p.x / TILE), ay = g.world.oy + (int32_t)std::floor(a.p.y / TILE);
        if (g.home.plotAt(ax, ay) != pi) { fail("a farm animal wandered out of its plot"); break; }
      }
    // the farmhand: works the rows by day; ripe crops go into the shipping crate
    for (int x = 2; x < 6; x++) {
      home::setGround(F, x, F.h - 2, 2);
      home::CropRec c;
      c.x = (uint8_t)x; c.y = (uint8_t)(F.h - 2); c.kind = (uint8_t)home::Crop::Turnip; c.plantedDay = (uint16_t)g.day; c.lastGrowDay = (uint16_t)g.day;
      F.crops.push_back(c);
    }
    // hired by talking to a farmer or labourer of the town (when one is out in the street); else the plain action
    {
      Actor* hand = nullptr;
      if (const life::Census* c = g.life.find(SV().id))
        for (Actor& a : g.actors)
          if (a.human && a.npc && a.resident >= 0 && a.resident < (int)c->res.size() && a.site == g.world.startSite &&
              (c->res[(size_t)a.resident].job == life::Job::Farmer || c->res[(size_t)a.resident].job == life::Job::Labourer) &&
              c->res[(size_t)a.resident].age >= 16 && !(c->res[(size_t)a.resident].flags & life::RF_EMPLOYED)) { hand = &a; break; }
      bool hired = false;
      if (hand && F.site == SV().id) {
        HomeOps::talkTo(g, *hand);
        const int o = optIndex(g, "WORK MY FARM");
        if (o < 0) fail("a farmer of the town offers no farm work");
        else {
          g.dialogueChoose(o);
          hired = g.home.plots[(size_t)pi].farmhandRes == (uint16_t)hand->resident;
          if (!hired) fail("hiring the farmer by dialogue did not make them the farmhand");
        }
        g.closeDialogue();
      } else out("home: seed %llu: no farmer of the plot's town in the street: the farmhand is hired by the action\n", (unsigned long long)seed);
      if (!home::hireFarmhand(g, pi, 14, why)) fail("hire a farmhand: " + why);
    }
    settle(g, 30);
    bool hand = false;
    for (const Actor& a : g.actors) hand |= (a.lifeBits & LB_HOMESTEAD) && a.human && a.slot >= HOME_SLOT_HAND;
    if (!hand) fail("the farmhand does not come to work by day");
    g.day += 8;
    settle(g, 2);
    size_t crate = 0;
    for (const home::PlacedObj& o : g.home.plots[(size_t)pi].outside) if ((home::Obj)o.kind == home::Obj::ShippingCrate) crate = o.data;
    if (g.home.plots[(size_t)pi].stores[crate].items.empty()) fail("the farmhand's harvest is not in the shipping crate");
    // a forced exposed night, far: one animal lost (and gone a day later); near: wolves at the pen, driven off
    home::Plot& E = g.home.plots[(size_t)pi];
    E.flags |= home::PF_EXPOSED;
    for (size_t k = E.animals.size(); k-- > 0;) if (E.animals[k].flags & home::AF_LOST) E.animals.erase(E.animals.begin() + (std::ptrdiff_t)k);   // earlier nights' losses
    const size_t n0 = E.animals.size();
    g.debugEnterBuilding(hbi, 0);
    home::forceRaid(g, pi);
    int lost = 0;
    for (const home::AnimalRec& a : g.home.plots[(size_t)pi].animals) lost += (a.flags & home::AF_LOST) != 0;
    if (lost != 1) fail("an offline raid did not take exactly one animal (" + std::to_string(lost) + ")");
    g.debugLeave();
    g.day += 2;
    settle(g, 3);
    if (g.home.plots[(size_t)pi].animals.size() != n0 - 1) fail("the lost animal was not taken off the farm");
    g.hour = 23.0f;
    g.godMode = true;
    const int wolves = home::forceRaid(g, pi);
    if (wolves < 2) fail("a live raid brought no wolves");
    settle(g, 10);
    int alive = 0;
    for (int id : g.homeRaid().wolves)
      for (const Actor& a : g.actors) alive += a.id == id && a.st != AState::Dead;
    if (alive < 2 || g.homeRaid().plot != pi) fail("the raid's wolves are not at the farm");
    for (int id : g.homeRaid().wolves) g.debugFell(id, true);
    settle(g, 10);
    if (g.homeRaid().outcome != 1) fail("killing the wolves did not drive the raid off");
    // (M7 fix r3) stepping indoors mid-raid settles the night by the offline roll (one animal), never a free win
    {
      auto lostNow = [&]() { int n = 0; for (const home::AnimalRec& a : g.home.plots[(size_t)pi].animals) n += (a.flags & home::AF_LOST) != 0; return n; };
      const int lost0 = lostNow();
      if (home::forceRaid(g, pi) < 2) fail("a second live raid brought no wolves");
      settle(g, 2);
      g.debugEnterBuilding(hbi, 0);
      settle(g, 2);
      if (g.homeRaid().outcome != 2) fail("going indoors during a live raid did not settle it as a loss (outcome " + std::to_string(g.homeRaid().outcome) + ")");
      if (lostNow() != lost0 + 1) fail("going indoors during a live raid did not take one animal");
      g.debugLeave();
    }
    g.godMode = false;
    g.hour = 11.0f;
  }
  // ---- horses: bought at a stablemaster's (waiting beside the player), ridden, left, stabled at an inn, saved
  {
    // out of the yard first (dismounted in the yard of a plot with a stable, she goes into it)
    {
      const home::Plot& P = g.home.plots[(size_t)pi];
      g.teleportGlobal(P.gx + P.w / 2, P.gy + P.h + 3);
      settle(g, 2);
    }
    const int nh = (int)g.home.plots[(size_t)pi].animals.size();
    if (!home::buyHorse(g, home::Breed::Steppe, g.world.startSite, why)) { fail("buyHorse: " + why); return; }
    if (g.home.horse != pi * 64 + nh) fail("the bought horse is not out with the player");
    settle(g, 3);
    if (home::horseActor(g) <= 0) fail("the bought horse does not wait beside the player");
    if (!home::mountWaiting(g, why)) fail("mount the waiting horse: " + why);
    if (!g.homeRiding() || g.homeSpeedMul() < 1.6f) fail("riding the bought horse is not 1.6x walking");
    // every dismount: the player, an attack, a bow, a spell, a roll, a blow, entering, water
    const int32_t sx = g.world.ox + (int32_t)(g.pl().p.x / TILE), sy = g.world.oy + (int32_t)(g.pl().p.y / TILE);
    g.homeDismount(home::Dismount::Player);
    if (g.home.horse < 0 || std::abs(g.home.horseGx - sx) > 1 || std::abs(g.home.horseGy - sy) > 1) fail("the horse does not wait where the rider got off");
    settle(g, 3);
    auto ride = [&] {
      std::string w;
      const int k = home::horseActor(g);
      if (k > 0) g.pl().p = g.actors[(size_t)k].p + Vec2(0, 6);   // walk up to her
      if (!g.homeRiding() && !home::mountWaiting(g, w)) fail("mount again: " + w);
    };
    for (int t = 0; t < 4; t++) {
      ride();
      Input in;
      if (t == 0) in.attack = true;
      if (t == 1) in.bow = true;
      if (t == 2) in.spell = true;
      if (t == 3) { in.roll = true; in.move = Vec2(1, 0); }
      g.stamina = g.maxSt; g.mp = g.maxMp;
      g.update(SIM_DT, in);
      settle(g, 20);
      if (g.homeRiding()) fail("an attack / bow / spell / roll did not dismount (" + std::to_string(t) + ")");
    }
    if (g.home.dismounts[(int)home::Dismount::Attack] < 4) fail("attacks, bows, spells and rolls are not all counted as ATTACK");
    // water: the rider is pitched off at the bank, the horse stays on dry ground
    ride();
    int wx = -1, wy = -1;
    const Map& M = g.world.over;
    const int ptx = (int)(g.pl().p.x / TILE), pty = (int)(g.pl().p.y / TILE);
    for (int r = 1; r < 60 && wx < 0; r++)
      for (int dy = -r; dy <= r && wx < 0; dy++)
        for (int dx = -r; dx <= r && wx < 0; dx++)
          if (M.in(ptx + dx, pty + dy) && M.at(ptx + dx, pty + dy) == Ground::Swamp && !M.blocked(ptx + dx, pty + dy) && M.in(ptx + dx, pty + dy - 1) &&
              M.at(ptx + dx, pty + dy - 1) != Ground::Swamp && !groundWater(M.at(ptx + dx, pty + dy - 1)) &&
              !M.blocked(ptx + dx, pty + dy - 1)) { wx = ptx + dx; wy = pty + dy; }
    if (wx < 0) {   // no bog near: a patch of it is laid beside the player (the rule is what is tested)
      for (int dy = 2; dy < 8 && wx < 0; dy++)
        if (M.in(ptx, pty + dy) && !M.blocked(ptx, pty + dy) && !M.blocked(ptx, pty + dy - 1)) {
          wx = ptx; wy = pty + dy;
          g.world.over.ground[(size_t)wy * M.w + wx] = (uint8_t)Ground::Swamp;
        }
    }
    if (wx < 0) out("home: seed %llu: no ground for the bog dismount\n", (unsigned long long)seed);
    else {
      g.pl().p = Vec2(wx * TILE + 8.0f, (wy - 1) * TILE + 10.0f);
      settle(g, 1);
      g.pl().p = Vec2(wx * TILE + 8.0f, wy * TILE + 10.0f);
      settle(g, 1);
      if (g.homeRiding() || g.home.dismounts[(int)home::Dismount::Water] < 1)
        fail("water did not dismount the rider (riding " + std::to_string(g.homeRiding()) + ", ground " + std::to_string((int)M.at((int)(g.pl().p.x / TILE), (int)((g.pl().p.y - 2) / TILE))) + ")");
      else if (M.at(g.home.horseGx - g.world.ox, g.home.horseGy - g.world.oy) == Ground::Swamp) fail("the horse was left standing in the bog");
      g.pl().p = Vec2(wx * TILE + 8.0f, (wy - 1) * TILE + 10.0f);
      settle(g, 3);
    }
    // stabled at an inn and fetched again
    if (!home::stableAtInn(g, g.world.startSite, why)) fail("stable at the inn: " + why);
    else if (!g.home.horseInn) fail("the horse is not at the inn's stable");
    if (!home::stableAtInn(g, g.world.startSite, why) || g.home.horseInn) fail("the horse could not be fetched from the inn");
    // the save keeps where she waits
    settle(g, 3);
    std::vector<uint8_t> a, b;
    g.serialize(a);
    Game h(1);
    if (!h.deserialize(a)) { fail("a save with a waiting horse did not load"); return; }
    h.serialize(b);
    if (a != b) fail("a save with a waiting horse does not round-trip byte-identically");
    if (h.home.horse != g.home.horse || h.home.horseGx != g.home.horseGx || h.home.horseGy != g.home.horseGy) fail("the horse's whereabouts were not saved");
    // left far behind she walks home
    g.teleportGlobal(g.home.horseGx + home::HORSE_LEFT + 40, g.home.horseGy);
    settle(g, 40);
    if (g.home.horse >= 0) fail("a horse left far behind did not walk home");
  }
  // ---- a property seller in a town lists a lot or a house; taxes owed are paid there
  {
    home::Plot& T = g.home.plots[(size_t)pi];
    T.paidUntilDay = (uint16_t)(g.day - 10);
    if (home::taxOwed(T, g.day) != 2 * home::plotTaxWeek(T)) fail("ten days unpaid are not two weeks' tax");
    if (home::lienOwed(g) != 0) fail("two weeks behind already liens the player");
    // (M7 fix) four weeks behind: a lien; no builder and no seller until it is paid
    const uint16_t keepPaid = T.paidUntilDay;
    const int day0 = g.day;
    g.day += 40;
    T.paidUntilDay = (uint16_t)(g.day - 26);
    std::string w;
    if (home::lienOwed(g) <= 0) fail("four weeks of unpaid tax put no lien on the player");
    else if (!g.world.lots.empty() && home::buyLot(g, 0, w)) fail("a seller sold land to a player with a lien");
    T.paidUntilDay = keepPaid;
    g.day = day0;
  }
  // ---- (M7 fix) stores are reused: placing and selling a crate 300 times leaves no trail of empty stores; the doghouse
  // and the dog keep the watch only while they are there
  {
    g.teleportGlobal(g.home.plots[(size_t)pi].gx + g.home.plots[(size_t)pi].w / 2, g.home.plots[(size_t)pi].gy + g.home.plots[(size_t)pi].h + 1);
    settle(g, 5);
    home::Plot& Q = g.home.plots[(size_t)pi];
    g.gold = 1000000;
    std::string w;
    int cx = -1, cy = -1;
    for (int y = 0; y < Q.h && cx < 0; y++)
      for (int x = 0; x < Q.w && cx < 0; x++)
        if (home::placeObj(g, pi, home::Obj::ShippingCrate, x, y, false, false, w)) { cx = x; cy = y; }
    if (cx < 0) fail("no room for a crate on the lot: " + w);
    else {
      const size_t n0 = g.home.plots[(size_t)pi].stores.size();
      for (int i = 0; i < 300; i++) {
        if (!home::removeObj(g, pi, cx, cy, false, w)) { fail("selling the empty crate: " + w); break; }
        if (!home::placeObj(g, pi, home::Obj::ShippingCrate, cx, cy, false, false, w)) { fail("placing the crate again: " + w); break; }
      }
      if (g.home.plots[(size_t)pi].stores.size() != n0) fail("placing and selling a crate grew the stores (" + std::to_string(n0) + " -> " + std::to_string(g.home.plots[(size_t)pi].stores.size()) + ")");
      home::removeObj(g, pi, cx, cy, false, w);
      if (home::placeObj(g, pi, home::Obj::Doghouse, cx, cy, false, false, w)) {
        if (!(g.home.plots[(size_t)pi].flags & home::PF_DOG)) fail("a doghouse does not keep watch");
        home::removeObj(g, pi, cx, cy, false, w);
        bool dog = false;
        for (const home::AnimalRec& a : g.home.plots[(size_t)pi].animals) dog |= (home::Animal)a.kind == home::Animal::Dog && !(a.flags & home::AF_LOST);
        if (!dog && (g.home.plots[(size_t)pi].flags & home::PF_DOG)) fail("the watch stays after the doghouse is sold");
      }
    }
  }
  // ---- (M7 fix) the inn stable charges a night's keep after the first, paid on fetching the horse
  if (g.home.horse < 0 && g.home.riding < 0)
    for (int q = 0; q < (int)g.home.plots.size() && g.home.horse < 0; q++)
      for (int k = 0; k < (int)g.home.plots[(size_t)q].animals.size(); k++)
        if ((home::Animal)g.home.plots[(size_t)q].animals[(size_t)k].kind == home::Animal::Horse) {
          g.home.horse = q * 64 + k;   // (out of her stable, waiting by the player)
          g.home.horseGx = g.world.ox + (int32_t)(g.pl().p.x / TILE) + 1; g.home.horseGy = g.world.oy + (int32_t)(g.pl().p.y / TILE);
          break;
        }
  if (g.home.horse >= 0 || g.home.riding >= 0) {
    std::string w;
    if (home::stableAtInn(g, g.world.startSite, w) && g.home.horseInn) {
      g.day += 3;
      const int owed = home::stableOwed(g), gold0 = g.gold;
      if (owed != 2 * home::STABLE_INN_FEE) fail("three nights at the inn stable do not owe two nights' keep (" + std::to_string(owed) + ")");
      if (!home::stableAtInn(g, g.world.startSite, w) || g.gold != gold0 - owed) fail("fetching the horse did not take the keep: " + w);
    }
  }
}

int homeCmd(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  int n = 1000;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc && !parseSeedRange(argv[++i], a, b)) { printf("bad --seeds\n"); return 2; }
    if (!std::strcmp(argv[i], "--fuzz") && i + 1 < argc) n = std::max(100, std::atoi(argv[++i]));
  }
  g_bad = 0;
  tables();
  growth();
  determinism();
  fuzz(n);
  saveBlock();
  for (uint64_t s = a; s <= b; s++) { g_curSeed = s; play(s); }
  yieldsByQuality();
  g_sold = g_lotsBought = g_lotsSkipped = 0;
  for (uint64_t s = a; s <= b; s++) { g_curSeed = s; playB(s); }
  out("home: houses sold by dialogue %d of %d seeds (a village sells one at 40 %%); generated lots bought %d, skipped %d (World::lots empty)\n", g_sold,
      (int)(b - a + 1), g_lotsBought, g_lotsSkipped);
  printf("home: %d failure(s)\n", g_bad);
  return g_bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--home", "M7 Home contracts: tables, seasons, growth, catch-up determinism, a 1000-placement fuzz, the 4 KB plot budget, the home block, building, riding and dismounts [--seeds A..B] [--fuzz N]", homeCmd);
