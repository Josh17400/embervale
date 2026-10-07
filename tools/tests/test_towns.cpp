// rpg_test --towns [--seeds A..B] [--png DIR] [--verbose]: the settlements of rpg/world/settlement.cpp (TOWNS lane,
// VISION_PLAN 15.8), built straight from synthetic contexts: flat plains, a river through the footprint, a hillside, a
// coast (fishing and port archetypes), with and without road bearings, every archetype in turn, a capital every seed.
// For every settlement:
//  - scale: homes in range (villages 10-15, towns 40-60, cities 160-220) and no more buildings than the cap;
//  - services: villages an inn, a shop or smithy and a well; towns an inn, shop, smithy and temple; cities two or more
//    inns and shops, a temple, a mage tower and the keep; capitals the palace and the barracks;
//  - every building's door reachable from the heart, and the heart from beyond the town (through the gates);
//  - no overlapping footprints, no sprite over another building's front (foundation row, doorstep, apron), every
//    footprint on one relief level and off the water;
//  - walls closed except at the openings (a flood from the heart with the openings shut never leaves the ring);
//  - a gate (or for open towns a main street) within 30 degrees of every road bearing;
//  - capitals: the palace's throne hall (throne, king) reachable from the door, stairs up to the private quarters, a
//    council room and bedchambers; the barracks' bunks and weapon racks;
//  - determinism: the same context built twice hashes the same; build times per type (budget about 25 ms native).
//  - (M1 economy, owner 2026-10-05) every village has a well, an inn, a smith and a small market (a stall or a cart on
//    the green), a mill when it farms, its specialisation's building (granary, fishmonger, smelter, sawmill, tannery)
//    and yard (the mine...); towns and cities the full set of trades; markets of the right size laid out in rows
//    (at most three rows, the stalls of a row touching or a walkway apart, never a ring), every counter with Filler
//    either side, its two-row aisle open and its keeper's tile walkable, never on a door's step or apron.
//  --sweep N: N random settlements (every type, archetype, land and specialisation), the same checks, 0 failures.
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "rpg/culture/society.h"
#include "rpg/sim/interior_v4.h"
#include "rpg/world/dmath.h"
#include "rpg/world/settlement.h"
#include "rpg/world/town_gen.h"
#include "rpg/world/town_rules.h"
#include "tools/tests/tests.h"

namespace {

using art::Building;
using art::Prop;

uint64_t fnv(uint64_t h, const void* p, size_t n) {
  const uint8_t* b = (const uint8_t*)p;
  for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; }
  return h;
}
uint64_t townHash(const ew::SettlementOut& o) {
  uint64_t h = 1469598103934665603ull;
  const Map& m = o.buf;
  int hd[6] = {m.w, m.h, o.gx, o.gy, o.ex, o.ey};
  h = fnv(h, hd, sizeof hd);
  h = fnv(h, m.ground.data(), m.ground.size());
  h = fnv(h, m.prop.data(), m.prop.size());
  h = fnv(h, m.wall.data(), m.wall.size());
  h = fnv(h, m.biome.data(), m.biome.size());
  h = fnv(h, m.height.data(), m.height.size());
  h = fnv(h, o.used.data(), o.used.size());
  for (const Bldg& b : m.bldgs) {
    uint64_t v[10] = {b.id, (uint64_t)b.type, (uint64_t)b.r.x, (uint64_t)b.r.y, (uint64_t)b.r.w, (uint64_t)b.r.h, b.seed, b.storeys, b.banner, (uint64_t)b.owner};
    h = fnv(h, v, sizeof v);
  }
  for (const Spawn& s : m.spawns) { int v[4] = {s.x, s.y, (int)s.role, s.slot}; h = fnv(h, v, sizeof v); }
  for (auto& g : o.gates) { int v[2] = {g.first, g.second}; h = fnv(h, v, sizeof v); }
  for (auto& g : o.wallGaps) { int v[4] = {g.x, g.y, g.w, g.h}; h = fnv(h, v, sizeof v); }
  return h;
}

// the synthetic land under a test town
enum class Land { Plains, River, Hill, Coast };
const char* landName(Land l) { static const char* n[] = {"plains", "river", "hill", "coast"}; return n[(int)l]; }
const char* archName(ew::Archetype a) {
  static const char* n[] = {"plain", "farming", "fishing", "port", "mining", "rivercrossing", "hillfort", "market"};
  return n[(int)a];
}

struct Case {
  SiteType type;
  ew::Archetype arch;
  Land land;
  bool capital;
  std::vector<float> roads;
  uint32_t seed;
  ew::Specialty spec = ew::Specialty::None;   // (M1 economy) None: the generator derives it
  // M3: a culture (cult::Atlas::make(culture, seed) as ctx.culture; -1 none), its layout forced (-1: the culture's
  // own), the biome of the land's open ground
  int culture = -1, forceLayout = -1;
  Biome bio = Biome::Plains;
};

// M3: the layout a culture case is built in (as the generator picks it)
cult::Layout caseLayout(const Case& c, const cult::Culture& K) { return ew::townLayoutFor(&K, c.type, c.seed, 1000, -2000); }   // (buildCase's heart)
cult::Culture caseCulture(const Case& c) {
  cult::Culture K = cult::Atlas::make((cult::Archetype)c.culture, (uint32_t)ew::mix64(c.seed ^ 0xC17u), (int)c.bio);
  if (c.forceLayout >= 0) K.town.layout = K.town.altLayout = (cult::Layout)c.forceLayout;
  return K;
}
const char* layoutName(cult::Layout l) {
  static const char* n[] = {"organic", "grid", "radial", "linear", "compound", "terraced", "stilt"};
  return (int)l < 7 ? n[(int)l] : "?";
}

std::pair<double, int> g_phaseMax[3];
int g_reqAsked = 0, g_reqMet = 0;   // (M3b) the society's required buildings asked / placed over the culture cases
double g_phaseTab[3][16] = {};   // (M3) the slowest of each phase per type
std::pair<double, int> g_casePhase;   // (M3) this case's slowest phase   // (M3) the slowest generator phase per type (village, town, city): ms, phase
void buildCase(const Case& c, ew::SitePlan& p, ew::KingdomPlan& k, ew::SettlementOut& so, double& ms) {
  p = ew::SitePlan();
  p.type = c.type;
  p.archetype = c.arch;
  p.special = c.spec;
  p.seed = c.seed;
  ew::settlementFootprint(c.type, c.arch, p.seed, p.w, p.h);
  p.gx = 1000 - p.w / 2; p.gy = -2000 - p.h / 2; p.ex = 1000; p.ey = -2000;
  p.bldgCap = 400;
  p.flags = c.capital ? ew::SPF_CAPITAL : 0;
  p.name = "TESTHOLD";
  k = ew::KingdomPlan();
  k.id = 77; k.name = "TESTMARK"; k.color = rgba(40, 70, 160); k.color2 = rgba(230, 200, 80); k.emblem = (uint8_t)(c.seed % 8);
  ew::SettlementCtx ctx;
  ctx.plan = &p;
  ctx.kingdom = &k;
  cult::Culture K;
  if (c.culture >= 0) { K = caseCulture(c); ctx.culture = &K; }
  ctx.rx = 3; ctx.ry = -8;
  ctx.roadBearings = c.roads;
  const int cxg = 1000, cyg = -2000, hw = p.w / 2;
  const Land land = c.land;
  const Biome open = c.bio;
  ctx.base = [land, cxg, cyg, hw, open](int32_t gx, int32_t gy, Ground& g, Biome& bi, uint8_t& h) {
    g = open == Biome::Desert ? Ground::Sand : (open == Biome::Snow ? Ground::Snow : Ground::Grass); bi = open; h = 1;
    int dx = gx - cxg, dy = gy - cyg;
    switch (land) {
      case Land::River: {   // a river winding north-south through the footprint, a third of the way in
        int rxv = -hw / 3 + (int)(ew::dsin(dy * 0.07f) * 4.0f) + dy / 6;
        if (std::abs(dx - rxv) <= 1) g = Ground::Water;
        else if (std::abs(dx - rxv) == 2) g = Ground::Dirt;
        break;
      }
      case Land::Hill:   // a hillside rising to the north-west in terraces
        h = (uint8_t)std::clamp(3 - (dy + dx / 2) / 13, 0, 6);
        bi = Biome::Forest;
        if ((gx * 7 + gy * 13) % 17 == 0) g = Ground::ForestFloor;
        break;
      case Land::Coast:   // the sea to the east
        if (dx > hw * 2 / 3 + (int)(ew::dsin(dy * 0.11f) * 3.0f)) { g = Ground::Water; bi = Biome::Ocean; }
        else if (dx > hw * 2 / 3 - 3) { g = Ground::Sand; bi = Biome::Beach; }
        break;
      default: break;
    }
  };
  // (M3) built a generator phase at a time, as the web's streaming does (EndlessSource::prepareChunk): the slowest
  // phase is what one web frame pays
  auto t0 = std::chrono::steady_clock::now();
  so = ew::SettlementOut();
  so.gx = p.gx - ew::town::MARGIN;
  so.gy = p.gy - ew::town::MARGIN;
  {
    ew::town::Gen g(ctx, so);
    g_casePhase = {0.0, -1};
    for (int ph = 0;; ph++) {
      const auto s0 = std::chrono::steady_clock::now();
      const bool more = g.step();
      const double pms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - s0).count();
      const int ti = c.type == SiteType::City ? 2 : (c.type == SiteType::Town ? 1 : 0);
      if (pms > g_phaseMax[ti].first) g_phaseMax[ti] = {pms, ph};
      if (ph < 16) g_phaseTab[ti][ph] = std::max(g_phaseTab[ti][ph], pms);
      if (pms > g_casePhase.first) g_casePhase = {pms, ph};
      if (!more) break;
    }
  }
  ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

int countProp(const Map& m, Prop p) {
  int n = 0;
  for (uint8_t v : m.prop) if (v == (int)p + 1) n++;
  return n;
}

// BFS over walkable tiles (Map::blocked; door tiles are walkable) from a set of tiles
std::vector<uint8_t> reach(const Map& m, const std::vector<std::pair<int, int>>& from) {
  std::vector<uint8_t> seen((size_t)m.w * m.h, 0);
  std::vector<int> q;
  for (auto& f : from)
    if (m.in(f.first, f.second) && !m.blocked(f.first, f.second) && !seen[(size_t)f.second * m.w + f.first]) {
      seen[(size_t)f.second * m.w + f.first] = 1;
      q.push_back(f.second * m.w + f.first);
    }
  for (size_t h = 0; h < q.size(); h++) {
    int x = q[h] % m.w, y = q[h] / m.w;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) {
      int nx = x + dx[k], ny = y + dy[k];
      if (!m.in(nx, ny) || seen[(size_t)ny * m.w + nx] || m.blocked(nx, ny)) continue;
      seen[(size_t)ny * m.w + nx] = 1;
      q.push_back(ny * m.w + nx);
    }
  }
  return seen;
}

// a picture of a town for debugging (--png DIR)
void dumpPng(const ew::SettlementOut& so, const std::string& path) {
  const Map& m = so.buf;
  const int S = 3;
  std::vector<uint32_t> px((size_t)m.w * S * m.h * S);
  for (int y = 0; y < m.h; y++)
    for (int x = 0; x < m.w; x++) {
      uint32_t c = groundColor(m.at(x, y));
      // (M3) relief: higher levels lighter, cliff faces dark, stairs amber
      {
        const int lv = m.heightAt(x, y), base = m.heightAt(so.ex - so.gx, so.ey - so.gy);
        if (lv > base) c = art::mix(c, rgba(255, 255, 255), std::min(0.5f, 0.14f * (lv - base)));
        if (lv < base) c = art::mix(c, rgba(0, 0, 0), std::min(0.5f, 0.14f * (base - lv)));
        if (m.heightBits(x, y) & Map::HEIGHT_CLIFF) c = rgba(70, 52, 40);
        if (m.heightBits(x, y) & Map::HEIGHT_RAMP) c = rgba(230, 160, 60);
      }
      if (!so.used.empty() && !so.used[(size_t)y * m.w + x]) c = art::mix(c, rgba(0, 0, 0), 0.35f);
      int p = m.propAt(x, y);
      if (p) c = (Prop)(p - 1) == Prop::Banner ? rgba(40, 70, 220) : rgba(30, 90, 30);
      if (p && art::isStall((Prop)(p - 1))) c = rgba(255, 220, 0);
      if (p && (Prop)(p - 1) == Prop::Filler) c = rgba(255, 140, 0);
      if (p && ((Prop)(p - 1) == Prop::MarketTable || (Prop)(p - 1) == Prop::GroundCloth)) c = rgba(255, 60, 220);
      if (p && ((Prop)(p - 1) == Prop::Sheep || (Prop)(p - 1) == Prop::Cow)) c = rgba(255, 255, 255);
      if (p && ((Prop)(p - 1) == Prop::Well || (Prop)(p - 1) == Prop::Fountain || (Prop)(p - 1) == Prop::Statue)) c = rgba(0, 255, 255);
      if (m.wall[(size_t)y * m.w + x]) c = rgba(110, 110, 120);
      int bi = m.bldgAt[(size_t)y * m.w + x];
      if (bi >= 0) {
        Building t = m.bldgs[(size_t)bi].type;
        c = ew::townIsHome(t) ? rgba(170, 80, 60) : (t == Building::Palace ? rgba(250, 210, 60) : rgba(200, 60, 160));
        if (x == m.bldgs[(size_t)bi].doorX() && y == m.bldgs[(size_t)bi].doorY()) c = rgba(255, 255, 255);
      }
      for (int j = 0; j < S; j++)
        for (int i = 0; i < S; i++) px[(size_t)(y * S + j) * m.w * S + x * S + i] = c;
    }
  for (const Spawn& s : m.spawns)
    if (m.in(s.x, s.y)) px[(size_t)(s.y * S + 1) * m.w * S + s.x * S + 1] = s.role == Role::Guard ? rgba(255, 0, 0) : rgba(255, 255, 0);
  writePng(path.c_str(), m.w * S, m.h * S, px);
}

struct Timing { double sum = 0, max = 0; int n = 0; };

// the palace and barracks inside (VISION_PLAN 15.7): returns failures
int checkPalace(const Bldg& b0, const char* what) {
  int bad = 0;
  Bldg b = b0;
  const int floors = b.floors();
  // (M3b: a royal seat built as one storey (the khan's great tent) holds its court on one floor)
  const bool oneFloor = b.form == (uint8_t)bld::Form::Tent || b.form == (uint8_t)bld::Form::Round;
  if (floors < (oneFloor ? 1 : 2)) { out("FAIL: %s: %s has %d floor(s), want 2+\n", what, bldgTypeName(b.type), floors); return 1; }
  bool throne = false, king = false, owner = false, council = false, bunks = false, racks = false;
  int bedrooms = 0;
  Stairs prevUp;
  for (int f = 0; f < floors; f++) {
    Map m;
    genInterior(m, b, b.seed, f);
    // reachable from the way in
    std::vector<std::pair<int, int>> from;
    if (f == 0) from.push_back({m.exitX, m.exitY - 1}); else from.push_back({m.down.ax, m.down.ay});
    auto seen = reach(m, from);
    auto reached = [&](int x, int y) { return m.in(x, y) && seen[(size_t)y * m.w + x]; };
    auto beside = [&](int x, int y) { return reached(x + 1, y) || reached(x - 1, y) || reached(x, y + 1) || reached(x, y - 1); };
    if (f + 1 < floors && (!m.up.valid() || !reached(m.up.x, m.up.y))) { out("FAIL: %s: %s floor %d: no reachable stairs up\n", what, bldgTypeName(b.type), f); bad++; }
    if (f > 0 && (!m.down.valid() || m.down.x != prevUp.x || m.down.y != prevUp.y)) { out("FAIL: %s: %s floor %d: stairs do not line up\n", what, bldgTypeName(b.type), f); bad++; }
    prevUp = m.up;
    for (size_t ri = 0; ri < m.rooms.size(); ri++) {
      const RoomInfo& R = m.rooms[ri];
      bool any = false;
      for (int y = R.r.y; y < R.r.y + R.r.h && !any; y++) for (int x = R.r.x; x < R.r.x + R.r.w; x++) if (m.roomIndexAt(x, y) == (int)ri && reached(x, y)) { any = true; break; }
      if (!any) { out("FAIL: %s: %s floor %d: %s unreachable\n", what, bldgTypeName(b.type), f, roomKindName(R.kind)); bad++; }
      if (R.kind == RoomKind::OwnerRoom) owner = true;
      if (R.kind == RoomKind::Council) council = true;
      if (R.kind == RoomKind::Bedroom) bedrooms++;
    }
    for (int y = 0; y < m.h; y++)
      for (int x = 0; x < m.w; x++) {
        int p = m.propAt(x, y);
        if (!p) continue;
        Prop pp = (Prop)(p - 1);
        if (pp == Prop::Throne) {
          int ri = m.roomIndexAt(x, y);
          if (ri >= 0 && m.rooms[(size_t)ri].kind == RoomKind::ThroneHall && beside(x, y)) throne = true;
        }
        if (pp == Prop::BunkBed) bunks = true;
        if (pp == Prop::WeaponRack) racks = true;
        if ((pp == Prop::Bed || pp == Prop::BunkBed)) {
          int ri = m.roomIndexAt(x, y);
          if (ri < 0 || !roomAllowsBed(m.rooms[(size_t)ri].kind)) { out("FAIL: %s: %s floor %d: a bed in a %s\n", what, bldgTypeName(b.type), f, ri >= 0 ? roomKindName(m.rooms[(size_t)ri].kind) : "wall"); bad++; }
        }
      }
    for (const Spawn& s : m.spawns) {
      if (!reached(s.x, s.y)) { out("FAIL: %s: %s floor %d: spawn role %d unreachable\n", what, bldgTypeName(b.type), f, (int)s.role); bad++; }
      if (s.role == Role::King && f == 0) {
        for (int oy = -3; oy <= 3; oy++) for (int ox = -3; ox <= 3; ox++) if (m.propAt(s.x + ox, s.y + oy) == (int)Prop::Throne + 1) king = true;
      }
    }
  }
  // (M3b: a royal seat that is no palace (a merchant republic's guildhall) holds no throne hall: the doge sits at the
  // head of its hall, where game.cpp seats the ruler when its interior has none)
  // (M3b: a hall seat (the jarl's great hall, the khan's tent court, the elders' stilt hall, the council spire, the
  // tree palace) holds council in the hall itself, round the ruler's seat: no separate council chamber; a one-floor
  // tent court has only the ruler's own sleeping alcove. The castle and the court palace keep the full M1 contract.)
  const int seatKind = b.seat ? (int)b.seat - 1 : -1;
  const bool hallSeat = seatKind == (int)cult::Seat::GreatHall || seatKind == (int)cult::Seat::TentCourt ||
                        seatKind == (int)cult::Seat::StiltHall || seatKind == (int)cult::Seat::CouncilSpire ||
                        seatKind == (int)cult::Seat::TreePalace;
  if (b.type == Building::Palace) {
    if (!throne) { out("FAIL: %s: palace without a reachable throne in its throne hall\n", what); bad++; }
    if (!king) { out("FAIL: %s: palace without the king by the throne\n", what); bad++; }
    if (!owner) { out("FAIL: %s: palace without the royal bedchamber\n", what); bad++; }
    if (!council && !hallSeat) { out("FAIL: %s: palace without a council room (%s, %d floors)\n", what, interiorTemplate(b, bldgBlueprint(b), b.seed), b.floors()); bad++; }
    if (bedrooms < 1 && !hallSeat) { out("FAIL: %s: palace without bedchambers\n", what); bad++; }
  }
  if (b.type == Building::Barracks && (!bunks || !racks)) { out("FAIL: %s: barracks without bunks (%d) or weapon racks (%d)\n", what, (int)bunks, (int)racks); bad++; }
  return bad;
}

// (M1 economy) markets and specialisations. Also counts stalls per settlement type (printed at the end).
std::map<std::string, std::pair<int, int>> g_stallRange;   // type -> min, max stalls
std::map<std::string, int> g_noStall;                      // type -> settlements without a stall
std::map<std::string, std::pair<int, int>> g_vendorRange;  // type -> min, max stalls + tables + cloths
int g_facings[4] = {};                                      // (stall facings) stalls facing S, N, E, W
int g_allFront = 0, g_markets3 = 0;                         // (stalls fixer round 2) markets of 3+ stalls all facing S
int g_mineHills = 0, g_mineAdits = 0, g_pens = 0, g_herders = 0;
int checkEconomy(const Case& c, const ew::SettlementOut& so, const char* what) {
  int bad = 0;
  const Map& m = so.buf;
  auto fail = [&](const char* fmt, auto... args) {
    char buf[300];
    std::snprintf(buf, sizeof buf, fmt, args...);
    out("FAIL: %s: %s\n", what, buf);
    bad++;
  };
  std::map<Building, int> n;
  for (const Bldg& b : m.bldgs) n[b.type]++;
  const ew::Specialty sp = so.special;
  if (sp == ew::Specialty::None) fail("no specialisation recorded");
  // the specialisation's buildings
  switch (sp) {
    case ew::Specialty::Farming:
      if (!n[Building::Windmill] && !n[Building::Watermill]) fail("a farming settlement without a mill");
      // (M1 fixer round 2) a village's mill is found from its heart: within a short walk (24 tiles)
      if (c.type == SiteType::Village) {
        int best = 1 << 30;
        for (const Bldg& b : m.bldgs)
          if (b.type == Building::Windmill || b.type == Building::Watermill)
            best = std::min(best, std::max(std::abs(b.r.cx() - (so.ex - so.gx)), std::abs(b.r.cy() - (so.ey - so.gy))));
        if (best > 24) fail("the village's mill stands %d tiles from its heart (want 24 or less)", best);
      }
      if (!n[Building::Granary]) fail("a farming settlement without a granary");
      break;
    case ew::Specialty::Fishing: if (!n[Building::Fishmonger]) fail("a fishing settlement without a fishmonger"); break;
    case ew::Specialty::Mining: {
      if (!n[Building::Smelter]) fail("a mining settlement without a smelter");
      if (!countProp(m, Prop::MineEntrance) && !countProp(m, Prop::MineHill)) fail("a mining settlement without its mine");
      g_mineHills += countProp(m, Prop::MineHill);
      g_mineAdits += countProp(m, Prop::MineEntrance);
      if (!countProp(m, Prop::MineRail)) fail("a mine without its track");
      // (M1 fixer) the mine's mouth is reached from the heart
      const int hx0 = so.ex - so.gx, hy0 = so.ey - so.gy;
      std::vector<std::pair<int, int>> from;
      for (int oy = -3; oy <= 3; oy++) for (int ox = -3; ox <= 3; ox++) from.push_back({hx0 + ox, hy0 + oy});
      auto seen = reach(m, from);
      for (int y = 0; y + 1 < m.h; y++)
        for (int x = 0; x < m.w; x++)
          if ((m.propAt(x, y) == (int)Prop::MineEntrance + 1 || m.propAt(x, y) == (int)Prop::MineHill + 1) && !seen[(size_t)(y + 1) * m.w + x])
            fail("the mine at %d,%d: no way to its mouth from the heart", x, y);
      break;
    }
    case ew::Specialty::Lumber: if (!n[Building::Sawmill]) fail("a lumber settlement without a sawmill"); break;
    case ew::Specialty::Herding: {
      if (!n[Building::Tanner]) fail("a herding settlement without a tannery");
      // (M1 fixer round 2) the herders' work yard: beasts in a fenced pen by the tannery, with its lean-to
      g_herders++;
      int beasts = 0;
      for (int sy = 0; sy < m.h; sy++)
        for (int sx = 0; sx < m.w; sx++) {
          if (m.propAt(sx, sy) != (int)Prop::PenShelter + 1) continue;
          int here = 0;
          for (int y = sy; y <= sy + 5; y++)
            for (int x = sx - 4; x <= sx + 4; x++) {
              const int q = m.propAt(x, y);
              if (q == (int)Prop::Sheep + 1 || q == (int)Prop::Cow + 1) here++;
            }
          beasts = std::max(beasts, here);
        }
      if (beasts >= 2) g_pens++;
      else if (c.type == SiteType::Village) fail("a herding village without its pen by the tannery (%d beasts)", beasts);
      break;
    }
    default: break;
  }
  if (c.type != SiteType::Village && (!n[Building::Bakery] || !n[Building::Butcher]))
    fail("%s without the trades: bakery %d butcher %d", siteTypeName(c.type), n[Building::Bakery], n[Building::Butcher]);
  // watermills stand with their wheel in the river
  for (const Bldg& b : m.bldgs) {
    if (b.type != Building::Watermill) continue;
    const int sx = (b.variant & 1) ? b.r.x - 1 : b.r.x + b.r.w;
    int wet = 0;
    for (int y = b.r.y; y < b.r.y + b.r.h; y++) if (m.in(sx, y) && groundWater(m.at(sx, y))) wet++;
    if (wet < 2) fail("a watermill at %d,%d without its river beside the wheel", b.r.x, b.r.y);
  }
  // the market. (stall facings) Every stall faces one of four ways (art_props.h): f, its customers' way (fx, fy) and
  // the way its counter runs (ax, ay); its counter's three tiles are its prop and two Filler
  struct St { int x, y, f, fx, fy, ax, ay; };
  std::vector<St> st;
  auto propAt = [&](int x, int y) { return m.propAt(x, y); };
  for (int y = 0; y < m.h; y++)
    for (int x = 0; x < m.w; x++) {
      const int p = m.propAt(x, y);
      if (!p || !art::isStall((Prop)(p - 1))) continue;
      const int f = art::stallFacingAt(propAt, x, y, so.gx, so.gy);
      St s{x, y, f, 0, 1, 1, 0};
      if (f == art::StallN) { s.fy = -1; }
      else if (f == art::StallE) { s.fx = 1; s.fy = 0; s.ax = 0; s.ay = 1; }
      else if (f == art::StallW) { s.fx = -1; s.fy = 0; s.ax = 0; s.ay = 1; }
      st.push_back(s);
      g_facings[f]++;
    }
  // (stalls fixer round 2, "some markets still face the camera only") every market of three or more stalls shows at
  // least one stall from behind or in profile
  if (st.size() >= 3) {
    g_markets3++;
    bool front = true;
    for (const St& s : st) if (s.f != art::StallS) front = false;
    if (front) { g_allFront++; out("note: %s: a market of %zu stalls all facing the camera (a cramped square)\n", what, st.size()); }
  }
  // (M1 fixer round 2) the open tables and cloths: Filler on their east tile, the seller's tile behind and the two
  // rows before them open, and no two of one town selling the same goods
  int tables = 0;
  {
    bool usedT[art::kTableGoods] = {}, usedC[art::kClothGoods] = {};
    for (int y = 0; y < m.h; y++)
      for (int x = 0; x < m.w; x++) {
        const int p = m.propAt(x, y);
        if (p != (int)Prop::MarketTable + 1 && p != (int)Prop::GroundCloth + 1) continue;
        tables++;
        const bool cloth = p == (int)Prop::GroundCloth + 1;
        if (m.propAt(x + 1, y) != (int)Prop::Filler + 1) fail("the %s at %d,%d has no Filler on its east tile", cloth ? "cloth" : "table", x, y);
        for (int dx = 0; dx <= 1; dx++)
          if (m.blocked(x + dx, y - 1) || m.blocked(x + dx, y + 1)) { fail("the %s at %d,%d is shut in", cloth ? "cloth" : "table", x, y); break; }
        const int gx = so.gx + x, gy = so.gy + y;
        bool& u = cloth ? usedC[ew::clothGoodsAt(gx, gy)] : usedT[ew::tableGoodsAt(gx, gy)];
        if (u) fail("two %s sell the same goods (%d,%d)", cloth ? "cloths" : "tables", x, y);
        u = true;
      }
  }
  const int vendors = (int)st.size() + tables;
  const int minVendors = c.type == SiteType::Village ? 1 : (c.type == SiteType::Town ? 3 : (c.capital ? 9 : 6));
  const int minStalls = c.type == SiteType::Village ? 1 : (c.type == SiteType::Town ? 2 : (c.capital ? 4 : 3));
  // (M1 fixer) every settlement has a market of real stalls (a village at least one; a cart alone is no market)
  if ((int)st.size() < minStalls) fail("a market of %zu stalls (want %d+)", st.size(), minStalls);
  if (vendors < minVendors) fail("a market of %d stalls and tables (want %d+)", vendors, minVendors);
  if (c.arch == ew::Archetype::Market && c.type != SiteType::Village && vendors < minVendors + 3)
    fail("a market town's market of %d stalls and tables (want %d+: it is the trading hub)", vendors, minVendors + 3);
  // (M1 fixer round 2) every trade keeps one stall at most in a town; outside villages no stall stands alone (each
  // touches a neighbour), and every run opens at both ends onto walkable ground
  {
    std::map<int, int> trades;
    for (const St& s : st) trades[m.propAt(s.x, s.y) - 1]++;
    for (auto& [t, k] : trades) if (k > 1) { fail("%d stalls of one trade (%d)", k, t - (int)Prop::StallProduce); break; }
    if (c.type != SiteType::Village)
      for (const St& s : st) {
        bool nb = false, lo = false, hi = false;
        for (const St& o : st) {
          if (o.f != s.f) continue;
          // (stalls fixer round 2) a side row's stalls stand a tile apart (4 on), never touching in one column
          const int stp = s.f >= art::StallE ? 4 : 3;
          if (o.x == s.x - stp * s.ax && o.y == s.y - stp * s.ay) nb = lo = true;
          if (o.x == s.x + stp * s.ax && o.y == s.y + stp * s.ay) nb = hi = true;
        }
        // (stalls fixer round 2) a side stall turned across a row's end (an L, a short walkway between) is no lone stall
        if (!nb && s.f >= art::StallE)
          for (const St& o : st)
            if (o.f <= art::StallN && o.y >= s.y - 3 && o.y <= s.y + 1 && std::abs(o.x - s.x) <= 9) nb = true;
        if (!nb) { fail("a lone stall at %d,%d", s.x, s.y); break; }
        // the tile two past each end of the run, on its aisle's first row, is open ground
        const bool ns = s.f >= art::StallE;
        const int la = ns ? -4 : -3, ha = ns ? 2 : 3;
        // (M3) the designed L (a side stall turned across a row's end, a short walkway between: town_market.cpp): its end
        // may meet the row's counter when the walkway before it runs on round the corner
        auto endShut = [&](int k) {
          const int ex = s.x + s.ax * k + s.fx, ey = s.y + s.ay * k + s.fy;
          if (!m.blocked(ex, ey)) return false;
          const int q = m.propAt(ex, ey);
          const bool counter = q == (int)Prop::Filler + 1 || (q && art::isStall((Prop)(q - 1)));
          if (!counter || !ns) return true;
          const int sg = k < 0 ? -1 : 1, wx = ex - s.ax * sg, wy = ey - s.ay * sg;   // the walkway tile before it
          return m.blocked(wx, wy) || m.blocked(wx - s.fx, wy - s.fy);
        };
        if ((!lo && endShut(la)) || (!hi && endShut(ha))) {
          const int ex = !lo && m.blocked(s.x + s.ax * la + s.fx, s.y + s.ay * la + s.fy) ? s.x + s.ax * la + s.fx : s.x + s.ax * ha + s.fx;
          const int ey = !lo && m.blocked(s.x + s.ax * la + s.fx, s.y + s.ay * la + s.fy) ? s.y + s.ay * la + s.fy : s.y + s.ay * ha + s.fy;
          fail("the run at %d,%d is shut in at an end (%d,%d: prop %d, building %d, wall %d, ground %d)", s.x, s.y, ex, ey, m.propAt(ex, ey) - 1,
               m.in(ex, ey) ? m.bldgAt[(size_t)ey * m.w + ex] : -9, m.in(ex, ey) ? (int)m.wall[(size_t)ey * m.w + ex] : -9, (int)m.at(ex, ey));
          if (std::getenv("EMB_DEBUG_TOWNS"))   // the props round it (prop - 1, '.' none)
            for (int y = s.y - 6; y <= s.y + 3; y++) {
              std::string row = "  ";
              for (int x = s.x - 6; x <= s.x + 6; x++) {
                char b[8];
                std::snprintf(b, sizeof b, "%4d", m.propAt(x, y) - 1);
                row += m.propAt(x, y) ? b : "   .";
              }
              out("%s\n", row.c_str());
            }
          break;
        }
      }
  }
  std::string tn = c.capital ? "capital" : siteTypeName(c.type);
  auto& rg = g_stallRange[tn];
  if (!rg.first && !rg.second) rg = {(int)st.size(), (int)st.size()};
  rg.first = std::min(rg.first, (int)st.size()); rg.second = std::max(rg.second, (int)st.size());
  auto& vr = g_vendorRange[tn];
  if (!vr.first && !vr.second) vr = {vendors, vendors};
  vr.first = std::min(vr.first, vendors); vr.second = std::max(vr.second, vendors);
  if (st.empty()) g_noStall[tn]++;
  // rows, per market: the stalls of one market are those within reach of each other (8 tiles across, 6 up or down);
  // a street market's stalls stand apart along the street, each its own short row
  {
    std::vector<int> grp(st.size(), -1);
    int ng = 0;
    for (size_t i = 0; i < st.size(); i++) {
      if (grp[i] >= 0) continue;
      std::vector<size_t> q{i};
      grp[i] = ng;
      for (size_t h = 0; h < q.size(); h++)
        for (size_t j = 0; j < st.size(); j++)
          if (grp[j] < 0 && std::abs(st[j].x - st[q[h]].x) <= 8 && std::abs(st[j].y - st[q[h]].y) <= 6) { grp[j] = ng; q.push_back(j); }
      ng++;
    }
    for (int g = 0; g < ng; g++) {   // (stall facings) a row is a line of one facing: east-west rows by y, side rows by x
      std::map<std::pair<int, int>, int> lines;
      for (size_t i = 0; i < st.size(); i++)
        if (grp[i] == g) lines[{st[i].f, st[i].f >= art::StallE ? st[i].x : st[i].y}]++;
      if (lines.size() > 3) { fail("a market's stalls on %zu rows (a ring, not rows)", lines.size()); break; }
    }
  }
  std::map<std::pair<int, int>, std::vector<int>> rows;   // (facing, line) -> positions along
  for (const St& s : st) rows[{s.f, s.f >= art::StallE ? s.x : s.y}].push_back(s.f >= art::StallE ? s.y : s.x);
  for (auto& [ln, xs] : rows) {
    std::sort(xs.begin(), xs.end());
    for (size_t i = 1; i < xs.size(); i++) {
      const int gap = xs[i] - xs[i - 1];
      // (stalls fixer round 2) a side row (facing E / W): a tile of ground between two stalls of a group, never touching
      const bool sideRow = ln.first >= art::StallE;
      if (sideRow ? (gap != 4 && gap < 5) : (gap != 3 && gap < 5)) { fail("stalls on row %d at %d and %d: neither touching nor a walkway apart", ln.second, xs[i - 1], xs[i]); break; }
    }
  }
  for (const St& s : st) {
    // the counter: its prop and two Filler, read back as the same facing (no neighbour's Filler confuses it)
    bool counter = true;
    for (int i = 1; i < 3; i++) {
      int dx, dy;
      art::stallCounterTile(s.f, i, dx, dy);
      if (m.propAt(s.x + dx, s.y + dy) != (int)Prop::Filler + 1) counter = false;
    }
    if (!counter) fail("the stall at %d,%d (facing %d) has no counter either side", s.x, s.y, s.f);
    if (s.f >= art::StallE && m.propAt(s.x - 1, s.y) == (int)Prop::Filler + 1 && m.propAt(s.x + 1, s.y) == (int)Prop::Filler + 1)
      fail("the side stall at %d,%d reads as an east-west one (Filler either side)", s.x, s.y);
    // its aisle: the two tiles before every counter tile open (the stall faces walking space), the keeper's tile behind
    for (int i = 0; i < 3; i++) {
      int dx, dy;
      art::stallCounterTile(s.f, i, dx, dy);
      const int cx = s.x + dx, cy = s.y + dy;
      for (int d = 1; d <= 2; d++) {
        const int ax = cx + s.fx * d, ay = cy + s.fy * d;
        if (m.blocked(ax, ay) || m.propAt(ax, ay)) { fail("the stall at %d,%d: its aisle is blocked at %d,%d (prop %d)", s.x, s.y, ax, ay, m.propAt(ax, ay) - 1); i = 3; break; }
      }
      for (const Bldg& b : m.bldgs) {
        const int ax = b.doorX(), ay = b.r.y + b.r.h;
        if (std::abs(cx - ax) <= 1 && cy >= ay && cy <= ay + 1) { fail("the stall at %d,%d stands on the doorstep of %s", s.x, s.y, bldgTypeName(b.type)); i = 3; break; }
      }
    }
    const art::StallKeeperSpot k = art::stallKeeperSpot(s.f);
    if (m.blocked(s.x + k.postDx, s.y + k.postDy)) fail("the stall at %d,%d: no room for its keeper", s.x, s.y);
    // its keeper: a merchant posted there, who reads it back as their stall
    bool kept = false;
    for (const Spawn& sp : m.spawns)
      if (sp.npc && sp.role == Role::Merchant && sp.x == s.x + k.postDx && sp.y == s.y + k.postDy) {
        int vx, vy, vf;
        kept = art::stallOfPost(propAt, sp.x, sp.y, so.gx, so.gy, vx, vy, vf) && vx == s.x && vy == s.y && vf == s.f;
      }
    if (!kept) fail("the stall at %d,%d (facing %d) has no keeper on its post", s.x, s.y, s.f);
    // (stalls fixer round 3, owner: "why are the vendors standing on the end?") the keeper stands behind the MIDDLE
    // counter tile, on its inside: their post is that tile's neighbour away from the customers, and the spot they
    // stand on is on their post (or, facing S, inside the middle counter tile against its back edge); the AI finds
    // the stall back from that spot (art::stallOfKeeper)
    {
      // the middle counter tile: the prop's own for S / N (Filler either side), the one north of it for E / W
      const int mx = 0, my = s.f >= art::StallE ? -1 : 0;
      if (k.postDx != mx - s.fx || k.postDy != my - s.fy) fail("the stall at %d,%d (facing %d): its keeper's post is not behind the counter's middle", s.x, s.y, s.f);
      const float hx = s.x * 16.0f + k.standX, hy = s.y * 16.0f + k.standY;
      const int tx = (int)std::floor(hx / 16.0f), ty = (int)std::floor(hy / 16.0f);
      const bool onPost = tx == s.x + k.postDx && ty == s.y + k.postDy, inMid = s.f == art::StallS && tx == s.x + mx && ty == s.y + my;
      if (!onPost && !inMid) fail("the stall at %d,%d (facing %d): its keeper stands off their post (%d,%d)", s.x, s.y, s.f, tx, ty);
      int kx = -1, ky = -1, kf = -1;
      if (!art::stallOfKeeper(propAt, hx, hy, so.gx, so.gy, kx, ky, kf) || kx != s.x || ky != s.y || kf != s.f)
        fail("the stall at %d,%d (facing %d): its keeper's spot reads back as %d,%d facing %d", s.x, s.y, s.f, kx, ky, kf);
    }
  }
  return bad;
}

// (M3, owner note 3: "diagonal city wall runs read as a jagged staircase") the wall's irregular neighbour patterns: a
// clean ring is one tile thick, made of straight runs and 1:1 diagonals. Counted: isolated wall tiles, spurs (a tile
// with one wall neighbour that is no jamb of an opening), 2x2 blocks (double-thick corners and runs), and 2-tile runs
// (the 2-1-2 jitter of a slope rasterised as a staircase). Openings' jambs and ends are not counted.
struct WallIrr { int isolated = 0, spurs = 0, blocks = 0, jitter = 0, walls = 0, fx = -1, fy = -1; int total() const { return isolated + spurs + blocks + jitter; } };
WallIrr g_wallIrr;
int g_walledTowns = 0;
WallIrr wallIrregularities(const Map& m, const std::vector<IRect>& gaps) {
  WallIrr r;
  auto w = [&](int x, int y) { return m.in(x, y) && m.wall[(size_t)y * m.w + x] != 0; };
  auto nearGap = [&](int x, int y) {
    for (const IRect& g : gaps)
      if (x >= g.x - 1 && x <= g.x + g.w && y >= g.y - 1 && y <= g.y + g.h) return true;
    return false;
  };
  for (int y = 0; y < m.h; y++)
    for (int x = 0; x < m.w; x++) {
      if (!w(x, y)) continue;
      r.walls++;
      int n = 0;
      for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if ((ox || oy) && w(x + ox, y + oy)) n++;
      const int t0 = r.total();
      if (n == 0) r.isolated++;
      else if (n == 1 && !nearGap(x, y)) r.spurs++;
      if (w(x + 1, y) && w(x, y + 1) && w(x + 1, y + 1)) r.blocks++;
      // a horizontal run of exactly two (its first tile here) on a slope: the wall goes on diagonally the same way at
      // both its ends (up-left and down-right, or down-left and up-right: a 2-1-2 staircase); a run of two at a peak
      // (both ends turning the same way) is a corner, not jitter. The same for a vertical run.
      if (!w(x - 1, y) && w(x + 1, y) && !w(x + 2, y) && !nearGap(x, y) && !nearGap(x + 1, y)) {
        const int l = w(x - 1, y - 1) ? -1 : (w(x - 1, y + 1) ? 1 : 0), rr = w(x + 2, y - 1) ? -1 : (w(x + 2, y + 1) ? 1 : 0);
        if (l != 0 && rr == -l) r.jitter++;
      }
      if (!w(x, y - 1) && w(x, y + 1) && !w(x, y + 2) && !nearGap(x, y) && !nearGap(x, y + 1)) {
        const int u = w(x - 1, y - 1) ? -1 : (w(x + 1, y - 1) ? 1 : 0), d = w(x - 1, y + 2) ? -1 : (w(x + 1, y + 2) ? 1 : 0);
        if (u != 0 && d == -u) r.jitter++;
      }
      if (r.total() > t0 && r.fx < 0) { r.fx = x; r.fy = y; }
    }
  return r;
}

std::map<std::string, int> g_plazaMax;   // (M2) the largest empty paved block per settlement kind
struct LayoutStat { int n = 0, homesMin = 1 << 30, homesMax = 0; double ms = 0, msMax = 0, phaseMax = 0; int phase = -1; };
std::map<std::string, LayoutStat> g_layout;   // (M3) "layout type" -> built, homes, ms
int g_layoutArch[7][8] = {};                 // (M3) settlements built (and checked) per layout style and ew::Archetype
int g_squarePeople = 1 << 30;             // (M2) the fewest people round a capital's main square

int checkTown(const Case& c, const ew::SitePlan& p, const ew::SettlementOut& so, const char* what) {
  int bad = 0;
  const Map& m = so.buf;
  auto fail = [&](const char* fmt, auto... args) {
    char buf[300];
    std::snprintf(buf, sizeof buf, fmt, args...);
    out("FAIL: %s: %s\n", what, buf);
    bad++;
  };
  if (m.w <= 0 || so.used.size() != (size_t)m.w * m.h) { fail("no buffer"); return bad; }
  // scale
  const ew::TownScale sc = ew::townScale(c.type);
  if (so.homes < sc.homesMin || so.homes > sc.homesMax) fail("%d homes (want %d..%d)", so.homes, sc.homesMin, sc.homesMax);
  if ((int)m.bldgs.size() > sc.bldgMax) fail("%zu buildings (cap %d)", m.bldgs.size(), sc.bldgMax);
  // services
  std::map<Building, int> n;
  for (const Bldg& b : m.bldgs) n[b.type]++;
  if (c.type == SiteType::Village) {
    if (!n[Building::Inn]) fail("village without an inn");
    if (!n[Building::Smithy]) fail("village without a smithy");
    if (!countProp(m, Prop::Well)) fail("village without a well");
  } else if (c.type == SiteType::Town) {
    if (!n[Building::Inn] || !n[Building::Shop] || !n[Building::Smithy] || !n[Building::Temple])
      fail("town services: inn %d shop %d smithy %d temple %d", n[Building::Inn], n[Building::Shop], n[Building::Smithy], n[Building::Temple]);
  } else {
    // (M3b) the seat of power: whatever the society builds it as (a keep, a great hall, a temple, a guildhall...);
    // a capital's ruler in its royal seat, its steward / jarl in the keep (the main quest's lord)
    int seats = 0, royal = 0;
    for (const Bldg& b : m.bldgs) { if (bldgIsSeat(b)) seats++; if (bldgIsRoyalSeat(b)) royal++; }
    if (n[Building::Inn] < 2 || n[Building::Shop] < 2 || !n[Building::Temple] || !n[Building::Tower] || !seats)
      fail("city services: inn %d shop %d temple %d tower %d seat %d", n[Building::Inn], n[Building::Shop], n[Building::Temple], n[Building::Tower], seats);
    if (c.capital && (royal != 1 || !n[Building::Keep])) fail("capital with %d royal seats (want 1) / keep %d", royal, n[Building::Keep]);
    if (c.capital && c.culture < 0 && !n[Building::Barracks]) fail("capital without barracks");
    if (!c.capital && (n[Building::Palace] || royal)) fail("a palace in a city that is no capital");
  }
  // (M3b) the society's required buildings
  if (c.culture >= 0) {
    const cult::Culture K = caseCulture(c);
    const cult::Society S = cult::societyOf(K);
    const cult::SettleTier tier = c.capital ? cult::SettleTier::Capital : (c.type == SiteType::City ? cult::SettleTier::City : (c.type == SiteType::Town ? cult::SettleTier::Town : cult::SettleTier::Village));
    std::string miss;
    for (const cult::BuildingReq& q : cult::requiredBuildings(S, K, tier, (int)c.arch, c.seed)) {
      if (!q.required) continue;
      int have = 0;
      for (const Bldg& b : m.bldgs) if (b.type == q.purpose && (!(q.civic & bld::CIVIC_SEAT) || (b.civic & bld::CIVIC_SEAT))) have++;
      g_reqAsked++;
      if (have) g_reqMet++; else miss += std::string(" ") + bldgTypeName(q.purpose);
    }
    if (!miss.empty()) fail("society requires%s", miss.c_str());
  }
  bad += checkEconomy(c, so, what);
  // ids unique
  for (size_t i = 0; i < m.bldgs.size(); i++)
    for (size_t j = i + 1; j < m.bldgs.size(); j++)
      if (m.bldgs[i].id == m.bldgs[j].id) { fail("buildings %zu and %zu share an id", i, j); i = m.bldgs.size(); break; }
  // footprints: no overlap, on one level, off the water; sprites clear of fronts
  for (size_t i = 0; i < m.bldgs.size(); i++) {
    const Bldg& a = m.bldgs[i];
    int lv = -1;
    for (int y = a.r.y; y < a.r.y + a.r.h; y++)
      for (int x = a.r.x; x < a.r.x + a.r.w; x++) {
        if (!m.in(x, y)) { fail("%s off the buffer", bldgTypeName(a.type)); continue; }
        if (m.bldgAt[(size_t)y * m.w + x] != (int)i) fail("%s footprint overlaps building %d", bldgTypeName(a.type), m.bldgAt[(size_t)y * m.w + x]);
        if ((groundWater(m.at(x, y)) || m.at(x, y) == Ground::Bridge) && !(a.styled && a.arch.foundation == art::Foundation::Stilts))
          fail("%s stands on water", bldgTypeName(a.type));
        int h = m.heightAt(x, y);
        if (lv >= 0 && h != lv) { fail("%s straddles relief levels", bldgTypeName(a.type)); y = a.r.y + a.r.h; break; }
        lv = h;
      }
    const int up = ew::townRiseTiles(a.type, a.storeys);
    IRect sA{a.r.x - 1, a.r.y - up, a.r.w + 2, a.r.h + up + 1};
    for (size_t j = 0; j < m.bldgs.size(); j++) {
      if (i == j) continue;
      const Bldg& b = m.bldgs[j];
      IRect fB{b.r.x, b.r.y + b.r.h - 1, b.r.w, 1}, aB{b.doorX() - 1, b.r.y + b.r.h, 3, 2};
      if (sA.overlaps(fB) || sA.overlaps(aB)) { fail("%s's sprite covers the front of %s", bldgTypeName(a.type), bldgTypeName(b.type)); break; }
    }
  }
  // reachability: every door from the heart; the heart from the buffer's edge
  const int hx = so.ex - so.gx, hy = so.ey - so.gy;
  std::vector<std::pair<int, int>> from;
  for (int oy = -3; oy <= 3; oy++) for (int ox = -3; ox <= 3; ox++) from.push_back({hx + ox, hy + oy});
  auto seen = reach(m, from);
  int unreached = 0;
  for (const Bldg& b : m.bldgs) {
    int ax = b.doorX(), ay = b.r.y + b.r.h;
    if (!m.in(ax, ay) || !seen[(size_t)ay * m.w + ax]) {
      if (unreached++ < 3) fail("%s at %d,%d: door unreachable", bldgTypeName(b.type), b.r.x, b.r.y);
      if (unreached == 1 && std::getenv("EMB_DEBUG_TOWNS")) {   // the ground round its door: # wall, B building, D door,
        for (int y = ay - 5; y <= ay + 5; y++) {                 // = bridge, ~ water, , swamp, o solid prop, ^ cliff,
          std::string row = "  ";                                // r ramp, * reached, . walkable but not reached
          for (int x = ax - 9; x <= ax + 9; x++) {
            char ch = ' ';
            if (!m.in(x, y)) ch = ' ';
            else if (m.wall[(size_t)y * m.w + x]) ch = '#';
            else if (m.bldgAt[(size_t)y * m.w + x] >= 0) ch = (x == m.bldgs[(size_t)m.bldgAt[(size_t)y * m.w + x]].doorX() && y == m.bldgs[(size_t)m.bldgAt[(size_t)y * m.w + x]].doorY()) ? 'D' : 'B';
            else if (m.heightBits(x, y) & Map::HEIGHT_CLIFF) ch = '^';
            else if (groundWater(m.at(x, y))) ch = '~';
            else if (m.propAt(x, y) && propSolid((Prop)(m.propAt(x, y) - 1))) ch = 'o';
            else if (seen[(size_t)y * m.w + x]) ch = (m.heightBits(x, y) & Map::HEIGHT_RAMP) ? 'r' : m.at(x, y) == Ground::Bridge ? '=' : '*';
            else ch = m.at(x, y) == Ground::Bridge ? '_' : (m.at(x, y) == Ground::Swamp ? ',' : '.');
            row += ch;
          }
          out("%s\n", row.c_str());
        }
      }
    }
  }
  bool out1 = false;
  for (int x = 0; x < m.w && !out1; x++) if (seen[(size_t)x] || seen[(size_t)(m.h - 1) * m.w + x]) out1 = true;
  for (int y = 0; y < m.h && !out1; y++) if (seen[(size_t)y * m.w] || seen[(size_t)y * m.w + m.w - 1]) out1 = true;
  if (!out1) fail("the heart cannot reach the open country");
  // walls closed: with the openings shut, a flood from the heart over everything that is not wall stays inside
  if (c.type == SiteType::City || (c.type == SiteType::Town && c.arch == ew::Archetype::HillFort)) {
    int walls = 0;
    for (uint8_t v : m.wall) walls += v != 0;
    if (!walls) fail("no city wall");
    std::vector<uint8_t> shut = m.wall;
    for (const IRect& g : so.wallGaps)
      for (int y = g.y; y < g.y + g.h; y++) for (int x = g.x; x < g.x + g.w; x++) if (m.in(x, y)) shut[(size_t)y * m.w + x] = 1;
    std::vector<uint8_t> f((size_t)m.w * m.h, 0);
    std::vector<int> par((size_t)m.w * m.h, -1);
    std::vector<int> q{hy * m.w + hx};
    f[(size_t)hy * m.w + hx] = 1;
    bool leaked = false;
    int leakAt = -1;
    for (size_t h = 0; h < q.size() && !leaked; h++) {
      int x = q[h] % m.w, y = q[h] / m.w;
      if (x == 0 || y == 0 || x == m.w - 1 || y == m.h - 1) { leaked = true; leakAt = q[h]; }
      static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
      for (int k = 0; k < 4; k++) {
        int nx = x + dx[k], ny = y + dy[k];
        if (!m.in(nx, ny) || f[(size_t)ny * m.w + nx] || shut[(size_t)ny * m.w + nx]) continue;
        f[(size_t)ny * m.w + nx] = 1;
        par[(size_t)ny * m.w + nx] = q[h];
        q.push_back(ny * m.w + nx);
      }
    }
    if (leaked) {
      // where the flood slipped past the ring: the last tile of its way out that still had a wall beside it
      int at = -1;
      for (int c = leakAt; c >= 0; c = par[(size_t)c]) {
        const int x = c % m.w, y = c / m.w;
        bool wallNb = false;
        for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if (m.in(x + ox, y + oy) && m.wall[(size_t)(y + oy) * m.w + x + ox]) wallNb = true;
        if (wallNb) { at = c; break; }
      }
      fail("the city wall leaks (a flood from the heart leaves the ring with the gates shut) near %d,%d", at < 0 ? -1 : at % m.w, at < 0 ? -1 : at / m.w);
    }
    if (so.gates.empty()) fail("no gatehouse");
    {
      const WallIrr wi = wallIrregularities(m, so.wallGaps);
      g_walledTowns++;
      g_wallIrr.isolated += wi.isolated; g_wallIrr.spurs += wi.spurs; g_wallIrr.blocks += wi.blocks; g_wallIrr.jitter += wi.jitter; g_wallIrr.walls += wi.walls;
      if (wi.total()) fail("wall irregularities: %d isolated, %d spurs, %d 2x2 blocks, %d 2-tile runs (the first at %d,%d)", wi.isolated, wi.spurs, wi.blocks, wi.jitter, wi.fx, wi.fy);
    }
    // gates near the roads
    for (float rb : c.roads) {
      bool ok = false;
      for (const IRect& g : so.wallGaps) {
        float ga = ew::datan2(g.y + g.h * 0.5f - hy, g.x + g.w * 0.5f - hx);
        if (std::fabs(ew::dwrap(ga - rb)) < 0.524f) ok = true;
      }
      if (!ok) fail("no gate within 30 degrees of the road at %.0f degrees", rb * 57.2958f);
    }
  } else {
    for (float rb : c.roads) {
      bool ok = false;
      for (int y = 0; y < m.h && !ok; y++)
        for (int x = 0; x < m.w; x++) {
          Ground g = m.at(x, y);
          if (g != Ground::Road && g != Ground::Dirt && g != Ground::Bridge) continue;
          float dx = (x - hx) / (p.w * 0.5f), dy = (y - hy) / (p.h * 0.5f);
          if (dx * dx + dy * dy < 1.0f) continue;
          if (std::fabs(ew::dwrap(ew::datan2((float)(y - hy), (float)(x - hx)) - rb)) < 0.524f) { ok = true; break; }
        }
      if (!ok) fail("no street leaves within 30 degrees of the road at %.0f degrees", rb * 57.2958f);
    }
  }
  // (M2, owner note 6) no bare paved expanse of 12 x 12 or more; a capital's main square holds at least 12 people
  if (c.type != SiteType::Village) {
    std::vector<uint16_t> dp((size_t)m.w * m.h, 0);
    int best = 0, bx = 0, by = 0;
    for (int y = 0; y < m.h; y++)
      for (int x = 0; x < m.w; x++) {
        const size_t i = (size_t)y * m.w + x;
        if (m.at(x, y) != Ground::Plaza || m.prop[i] || m.bldgAt[i] >= 0 || m.wall[i]) continue;
        int v = 1;
        if (x > 0 && y > 0) v = 1 + std::min({(int)dp[i - 1], (int)dp[i - (size_t)m.w], (int)dp[i - (size_t)m.w - 1]});
        dp[i] = (uint16_t)v;
        if (v > best) { best = v; bx = x - v + 1; by = y - v + 1; }
      }
    g_plazaMax[c.capital ? "capital" : siteTypeName(c.type)] = std::max(g_plazaMax[c.capital ? "capital" : siteTypeName(c.type)], best);
    if (best >= 12) fail("an empty paved block %dx%d at %d,%d", best, best, bx, by);
    // (M2 fixer round 3) nor a long bare band (the square check missed a strip 20 x 6 south of a capital's market):
    // the largest empty paved rectangle at least 5 deep, by area (a stack over each row's column heights)
    {
      std::vector<int> hc((size_t)m.w, 0), stk;
      int bestA = 0, rx = 0, ry = 0, rw = 0, rh = 0;
      for (int y = 0; y < m.h; y++) {
        for (int x = 0; x < m.w; x++) {
          const size_t i = (size_t)y * m.w + x;
          const bool e = m.at(x, y) == Ground::Plaza && !m.prop[i] && m.bldgAt[i] < 0 && !m.wall[i];
          hc[(size_t)x] = e ? hc[(size_t)x] + 1 : 0;
        }
        stk.clear();
        for (int x = 0; x <= m.w; x++) {
          const int h = x < m.w ? hc[(size_t)x] : 0;
          while (!stk.empty() && hc[(size_t)stk.back()] >= h) {
            const int top = stk.back();
            stk.pop_back();
            const int th = hc[(size_t)top], left = stk.empty() ? 0 : stk.back() + 1, w = x - left;
            if (th >= 5 && w >= 5 && th * w > bestA) { bestA = th * w; rx = left; ry = y - th + 1; rw = w; rh = th; }
          }
          stk.push_back(x);
        }
      }
      if (bestA >= 150) fail("an empty paved band %dx%d at %d,%d", rw, rh, rx, ry);
    }
    if (c.capital) {
      int people = 0;
      for (const Spawn& s : m.spawns)
        if (s.npc && s.role != Role::Guard && (s.x - hx) * (s.x - hx) + (s.y - hy) * (s.y - hy) <= 16 * 16) people++;
      g_squarePeople = std::min(g_squarePeople, people);
      if (people < 12) fail("%d townsfolk within 16 tiles of the main square's heart (want 12)", people);
    }
  }
  // capitals inside (M3b: the royal seat, whatever it is built as)
  if (c.capital)
    for (const Bldg& b : m.bldgs)
      if (bldgIsRoyalSeat(b) || b.type == Building::Barracks) {
        if (std::getenv("EMB_TRACE_TOWNS")) { std::fprintf(stderr, "trace: inside %s form %d storeys %d %dx%d\n", bldgTypeName(b.type), b.form, b.storeys, b.r.w, b.r.h); std::fflush(stderr); }
        bad += checkPalace(b, what);
      }
  // banners where a kingdom rules
  if (c.type != SiteType::Village && !countProp(m, Prop::Banner)) fail("no kingdom banners");
  for (const Bldg& b : m.bldgs)
    if ((b.type == Building::Keep || b.type == Building::Palace || b.type == Building::Inn || b.type == Building::Barracks || bldgIsSeat(b)) && !b.banner)
      { fail("%s flies no banner", bldgTypeName(b.type)); break; }
  if (!countProp(m, Prop::Signpost)) fail("no signpost at the road entrances");
  // (M1 fixer round 2) no stray piece of fence enclosing nothing: every fence tile joins another
  {
    auto fence = [&](int x, int y) { const int q = m.propAt(x, y); return q == (int)Prop::FenceH + 1 || q == (int)Prop::FenceV + 1; };
    int lone = 0, lx = 0, ly = 0;
    for (int y = 0; y < m.h; y++)
      for (int x = 0; x < m.w; x++) {
        if (!fence(x, y)) continue;
        bool nb = false;
        for (int oy = -1; oy <= 1 && !nb; oy++)
          for (int ox = -1; ox <= 1; ox++) if ((ox || oy) && fence(x + ox, y + oy)) { nb = true; break; }
        if (!nb) { if (!lone) { lx = x; ly = y; } lone++; }
      }
    if (lone) fail("%d stray fence pieces (the first at %d,%d)", lone, lx, ly);
  }
  return bad;
}

std::string townsFixture() {
#ifdef EMB_SOURCE_DIR
  return std::string(EMB_SOURCE_DIR) + "/tests/fixtures/golden_towns.txt";
#else
  return "tests/fixtures/golden_towns.txt";
#endif
}

int cmdTowns(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  std::string pngDir;
  bool verbose = false, golden = false, write = false;
  std::map<std::string, uint64_t> hashes;   // --golden: every case's town hash (the layout, the market, the props)
  int sweep = 0, pickS = -1, pickK = -1;
  bool cultures = true;   // (M3) the culture cases too (--no-cultures: the classic cases only)
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--golden")) { golden = true; continue; }
    if (!strcmp(argv[i], "--write")) { write = true; continue; }
    if (!strcmp(argv[i], "--no-cultures")) { cultures = false; continue; }
    if (!strcmp(argv[i], "--pick") && i + 1 < argc) { std::sscanf(argv[++i], "%d,%d", &pickS, &pickK); continue; }
    if (!strcmp(argv[i], "--sweep") && i + 1 < argc) sweep = std::max(1, atoi(argv[++i]));
    else if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--png") && i + 1 < argc) pngDir = argv[++i];
    else if (!strcmp(argv[i], "--verbose")) verbose = true;
  }
  int bad = 0;
  std::map<std::string, Timing> times;
  std::map<std::string, std::pair<int, int>> homes;   // type -> min, max
  static const ew::Archetype archs[] = {ew::Archetype::Plain, ew::Archetype::Farming, ew::Archetype::Fishing, ew::Archetype::Port,
                                        ew::Archetype::Mining, ew::Archetype::RiverCrossing, ew::Archetype::HillFort, ew::Archetype::Market};
  if (sweep) { a = 1; b = (uint64_t)(sweep + 9) / 10; }
  for (uint64_t s = a; s <= b; s++) {
    g_curSeed = s;
    std::vector<Case> cases;
    Rng r(s * 977 + 13);
    if (sweep) {
      // (M1 economy) the sweep: ten random settlements per step, every type, archetype, land and specialisation
      Rng q(s * 104729 + 7);
      for (int k = 0; k < 10 && (int)((s - 1) * 10 + (uint64_t)k) < sweep; k++) {
        Case c;
        const float tq = q.f();
        c.type = tq < 0.5f ? SiteType::Village : (tq < 0.8f ? SiteType::Town : SiteType::City);
        c.arch = archs[q.irange(8)];
        c.land = (Land)q.irange(4);
        c.spec = (ew::Specialty)q.irange((int)ew::Specialty::COUNT);
        if (c.arch == ew::Archetype::Fishing || c.arch == ew::Archetype::Port) c.land = Land::Coast;
        if (c.arch == ew::Archetype::RiverCrossing) c.land = Land::River;
        if (c.spec == ew::Specialty::Fishing && c.land != Land::Coast) c.land = Land::River;
        c.capital = c.type == SiteType::City && q.f() < 0.35f;
        if (q.f() < 0.7f) {
          int nr = c.type == SiteType::Village ? 1 + q.irange(2) : 2 + q.irange(c.type == SiteType::City ? 3 : 2);
          float a0 = q.f() * ew::D_TAU;
          for (int j = 0; j < nr; j++) c.roads.push_back(ew::dwrap(a0 + j * ew::D_TAU / nr + q.range(-0.4f, 0.4f)));
        }
        c.seed = (uint32_t)ew::mix64(s * 1000003 + (uint64_t)k * 7777);
        cases.push_back(c);
      }
    } else {
    for (int li = 0; li < 3; li++) {
      for (SiteType t : {SiteType::Village, SiteType::Town, SiteType::City}) {
        Case c;
        c.type = t;
        c.arch = archs[(s * 3 + (uint64_t)li + (uint64_t)t * 2) % 8];
        c.land = (Land)li;
        if (c.arch == ew::Archetype::Fishing || c.arch == ew::Archetype::Port) c.land = Land::Coast;
        if (c.arch == ew::Archetype::RiverCrossing) c.land = Land::River;
        c.capital = t == SiteType::City && (s + (uint64_t)li) % 3 == 0;
        if ((s + (uint64_t)li + (uint64_t)t) % 2 == 0) {
          int nr = t == SiteType::Village ? 1 + r.irange(2) : 2 + r.irange(t == SiteType::City ? 3 : 2);
          float a0 = r.f() * ew::D_TAU;
          for (int k = 0; k < nr; k++) c.roads.push_back(ew::dwrap(a0 + k * ew::D_TAU / nr + r.range(-0.4f, 0.4f)));
        }
        c.seed = (uint32_t)ew::mix64(s * 31 + (uint64_t)t * 7 + (uint64_t)li * 1013);
        cases.push_back(c);
      }
    }
    {   // a capital every seed, on the plains with three roads
      Case c;
      c.type = SiteType::City; c.arch = ew::Archetype::Plain; c.land = Land::Plains; c.capital = true;
      float a0 = r.f() * ew::D_TAU;
      for (int k = 0; k < 3; k++) c.roads.push_back(ew::dwrap(a0 + k * 2.1f));
      c.seed = (uint32_t)ew::mix64(s * 7919 + 5);
      cases.push_back(c);
    }
    if (cultures) {
      // M3: every culture archetype as a village, a town and a city (a capital now and then), on land that suits it
      // (cult::Atlas::make(archetype, seed) as ctx.culture), then every layout style forced, so each style is built
      // every seed whatever the culture tables pick
      static const Land landOf[12] = {Land::Coast, Land::Plains, Land::Plains, Land::Plains, Land::Plains, Land::Plains,
                                      Land::Plains, Land::Plains, Land::River, Land::Plains, Land::Plains, Land::Plains};
      static const Biome bioOf[12] = {Biome::Taiga, Biome::Forest, Biome::Plains, Biome::Plains, Biome::Desert, Biome::Plains,
                                      Biome::Swamp, Biome::Forest, Biome::Plains, Biome::Forest, Biome::Forest, Biome::Snow};
      // the forced styles' peoples: organic heartland, grid imperial, radial steppe, linear river, compound dune,
      // terraced jade, stilt marsh
      static const int byLayout[7] = {2, 3, 5, 8, 4, 7, 6};
      for (int pass = 0; pass < 2; pass++)
        for (int a = 0; a < (pass == 0 ? 12 : 7); a++)
          for (SiteType t : {SiteType::Village, SiteType::Town, SiteType::City}) {
            Case c;
            c.type = t;
            c.culture = pass == 0 ? a : byLayout[a];
            c.forceLayout = pass == 0 ? -1 : a;
            c.arch = archs[(s + (uint64_t)a * 3 + (uint64_t)t + (uint64_t)pass * 5) % 8];
            c.land = landOf[c.culture];
            c.bio = bioOf[c.culture];
            if (c.arch == ew::Archetype::Fishing || c.arch == ew::Archetype::Port) c.land = Land::Coast;
            if (c.arch == ew::Archetype::RiverCrossing) c.land = Land::River;
            if (c.land == Land::Coast && c.bio == Biome::Desert) c.bio = Biome::Plains;
            c.capital = t == SiteType::City && (s + (uint64_t)a + (uint64_t)pass) % 3 == 0;
            if ((s + (uint64_t)a + (uint64_t)t + (uint64_t)pass) % 2 == 0) {
              int nr = t == SiteType::Village ? 1 + r.irange(2) : 2 + r.irange(t == SiteType::City ? 3 : 2);
              float a0 = r.f() * ew::D_TAU;
              for (int k = 0; k < nr; k++) c.roads.push_back(ew::dwrap(a0 + k * ew::D_TAU / nr + r.range(-0.4f, 0.4f)));
            }
            c.seed = (uint32_t)ew::mix64(s * 131 + (uint64_t)a * 17 + (uint64_t)t * 7 + (uint64_t)pass * 7777 + 99991);
            cases.push_back(c);
          }
    }
    }
    for (size_t ci = 0; ci < cases.size(); ci++) {
      if (pickS >= 0 && ((int)s != pickS || (int)ci != pickK)) continue;   // --pick S,K: one case (with --png)
      const Case& c = cases[ci];
      ew::SitePlan p;
      ew::KingdomPlan k;
      ew::SettlementOut so, so2;
      double ms = 0, ms2 = 0;
      const bool trace = std::getenv("EMB_TRACE_TOWNS") != nullptr;   // (a crash: which case, which step)
      if (trace) { std::fprintf(stderr, "trace: seed %llu case %zu build\n", (unsigned long long)s, ci); std::fflush(stderr); }
      buildCase(c, p, k, so, ms);
      buildCase(c, p, k, so2, ms2);
      if (trace) { std::fprintf(stderr, "trace: check\n"); std::fflush(stderr); }
      char what[220];
      std::string cu;
      if (c.culture >= 0) {
        const cult::Culture K = caseCulture(c);
        cu = std::string(" ") + cult::archetypeName((cult::Archetype)c.culture) + " " + layoutName(caseLayout(c, K));
      }
      std::snprintf(what, sizeof what, "seed %llu #%zu %s%s %s %s on %s%s%s", (unsigned long long)s, ci, c.capital ? "capital " : "", siteTypeName(c.type),
                    archName(c.arch), ew::specialtyName(so.special), landName(c.land), c.roads.empty() ? "" : " with roads", cu.c_str());
      int fails = 0;
      if (townHash(so) != townHash(so2)) { out("FAIL: %s: not deterministic (two builds differ)\n", what); fails++; }
      if (golden) {   // (M1 fixer round 2) the cross-platform check: the hash only
        char key[64];
        std::snprintf(key, sizeof key, "town_s%llu_c%zu", (unsigned long long)s, ci);
        hashes[key] = townHash(so);
        bad += fails;
        continue;
      }
      fails += checkTown(c, p, so, what);
      bad += fails;
      std::string tn = c.capital ? "capital" : siteTypeName(c.type);
      if (c.culture >= 0) {   // (M3) per layout style: scale and build time
        const cult::Culture K = caseCulture(c);
        std::string ln = std::string(layoutName(caseLayout(c, K))) + " " + tn;
        auto& L = g_layout[ln];
        L.n++; L.ms += std::min(ms, ms2); L.msMax = std::max(L.msMax, std::min(ms, ms2));
        L.homesMin = std::min(L.homesMin, so.homes); L.homesMax = std::max(L.homesMax, so.homes);
        if (g_casePhase.first > L.phaseMax) { L.phaseMax = g_casePhase.first; L.phase = g_casePhase.second; }
        g_layoutArch[(int)caseLayout(c, K) % 7][(int)c.arch % 8]++;
      }
      Timing& T = times[tn];
      double best = std::min(ms, ms2);
      T.sum += best; T.n++; T.max = std::max(T.max, best);
      auto& hm = homes[tn];
      if (!hm.first && !hm.second) hm = {so.homes, so.homes};
      hm.first = std::min(hm.first, so.homes); hm.second = std::max(hm.second, so.homes);
      if (verbose || fails)
        out("%s: %dx%d, %zu buildings, %d homes, %zu gates, %zu openings, %zu spawns, %.1f ms\n", what, p.w, p.h, so.buf.bldgs.size(), so.homes,
            so.gates.size(), so.wallGaps.size(), so.buf.spawns.size(), best);
      if (!pngDir.empty()) {
        char fn[300];
        std::string ln = c.culture >= 0 ? std::string("_") + cult::archetypeName((cult::Archetype)c.culture) + "_" + layoutName(caseLayout(c, caseCulture(c))) : "";
        std::snprintf(fn, sizeof fn, "%s/town_s%llu_%zu_%s_%s%s.png", pngDir.c_str(), (unsigned long long)s, ci, tn.c_str(), archName(c.arch), ln.c_str());
        dumpPng(so, fn);
      }
    }
  }
  if (golden) {
    // (M1 fixer round 2) settlement layouts are platform-stable: native and the web build make the same towns. The
    // fixture holds every case's hash (streets, buildings, the market's rows and tables, yards, spawns); regenerate it
    // with --towns --golden --write when the generator changes on purpose.
    if (write) {
      printf("# rpg/world settlement golden values (rpg_test --towns --golden --write; seeds %llu..%llu): every synthetic case's town\n",
             (unsigned long long)a, (unsigned long long)b);
      printf("# hash (ground, props, walls, heights, buildings, spawns, gates). Must match on every platform.\n");
      for (auto& kv : hashes) printf("%s %016llx\n", kv.first.c_str(), (unsigned long long)kv.second);
      return bad ? 1 : 0;
    }
    FILE* f = fopen(townsFixture().c_str(), "r");
    if (!f) { printf("FAIL: cannot read %s\n", townsFixture().c_str()); return 1; }
    int seen = 0;
    char line[256], key[96];
    unsigned long long val;
    while (fgets(line, sizeof line, f)) {
      if (line[0] == '#' || sscanf(line, "%95s %llx", key, &val) != 2) continue;
      auto it = hashes.find(key);
      if (it == hashes.end()) { printf("FAIL: towns golden key %s is not computed (run with the fixture's seeds)\n", key); bad++; continue; }
      seen++;
      if (it->second != val) { printf("FAIL: towns golden %s = %016llx, expected %016llx\n", key, (unsigned long long)it->second, val); bad++; }
    }
    fclose(f);
    if (seen != (int)hashes.size()) { printf("FAIL: towns golden file has %d of %zu keys\n", seen, hashes.size()); bad++; }
    printf("towns golden: %zu hashes: %s\n", hashes.size(), bad ? "FAILED" : "ok");
    return bad ? 1 : 0;
  }
  for (auto& [tn, T] : times) {
    auto hm = homes[tn];
    auto sr = g_stallRange[tn];
    printf("towns: %-8s market stalls %d..%d, with the tables and cloths %d..%d (%d without a stall)\n", tn.c_str(), sr.first, sr.second,
           g_vendorRange[tn].first, g_vendorRange[tn].second, g_noStall[tn]);
    printf("towns: %-8s %3d built, homes %d..%d, build %.1f ms avg, %.1f ms max\n", tn.c_str(), T.n, hm.first, hm.second, T.n ? T.sum / T.n : 0.0, T.max);
    if (T.max > 60.0) { printf("FAIL: towns: %s build %.1f ms (budget about 25 ms native)\n", tn.c_str(), T.max); bad++; }
  }
  printf("towns: mines %d in their hill, %d bare adits; herders' pens by the tannery %d of %d\n", g_mineHills, g_mineAdits, g_pens, g_herders);
  // (stall facings) markets mix facings: rows on the north side face south, rows across the aisle show their backs,
  // side rows stand in profile
  printf("towns: stalls facing south %d, north %d, east %d, west %d; markets of 3+ stalls all facing south %d of %d\n", g_facings[0], g_facings[1],
         g_facings[2], g_facings[3], g_allFront, g_markets3);
  // (stalls fixer round 2) every market of three or more shows a stall from behind or in profile (a square too cramped
  // for any turned row, or an L at a row's end, may keep its fronts: at most one market in fifty)
  if (g_allFront * 50 > g_markets3) {
    printf("FAIL: towns: %d of %d markets of 3+ stalls show only fronts\n", g_allFront, g_markets3);
    bad++;
  }
  if (g_facings[0] + g_facings[1] + g_facings[2] + g_facings[3] >= 60 && (!g_facings[1] || (!g_facings[2] && !g_facings[3]))) {
    printf("FAIL: towns: the markets do not mix their stalls' facings\n");
    bad++;
  }
  for (auto& [ln, L] : g_layout)
    printf("towns: layout %-17s %3d built, homes %d..%d, build %.1f ms avg, %.1f ms max, slowest phase %d %.1f ms\n", ln.c_str(), L.n, L.homesMin, L.homesMax, L.n ? L.ms / L.n : 0.0, L.msMax, L.phase, L.phaseMax);
  printf("towns: slowest generator phase (one web frame's share): village %.1f ms (phase %d), town %.1f ms (phase %d), city %.1f ms (phase %d)\n",
         g_phaseMax[0].first, g_phaseMax[0].second, g_phaseMax[1].first, g_phaseMax[1].second, g_phaseMax[2].first, g_phaseMax[2].second);
  for (int ti = 0; ti < 3; ti++) {
    printf("towns: %s phases (slowest ms):", ti == 0 ? "village" : (ti == 1 ? "town" : "city"));
    for (int ph = 0; ph < 13; ph++) printf(" %.1f", g_phaseTab[ti][ph]);
    printf("\n");
  }
  {   // (M3, PLAN.md task 3) every settlement archetype in every layout style
    static const char* ln[7] = {"organic", "grid", "radial", "linear", "compound", "terraced", "stilt"};
    printf("towns: archetypes per layout (plain farming fishing port mining river hillfort market):\n");
    for (int l = 0; l < 7; l++) {
      printf("towns:   %-9s", ln[l]);
      for (int a2 = 0; a2 < 8; a2++) printf(" %3d", g_layoutArch[l][a2]);
      printf("\n");
    }
  }
  printf("towns: wall irregularities over %d walled settlements (%d wall tiles): %d isolated, %d spurs, %d 2x2 blocks, %d 2-tile runs\n",
         g_walledTowns, g_wallIrr.walls, g_wallIrr.isolated, g_wallIrr.spurs, g_wallIrr.blocks, g_wallIrr.jitter);
  printf("towns: largest empty paved block:");
  for (auto& kv : g_plazaMax) printf(" %s %d", kv.first.c_str(), kv.second);
  printf(" | fewest people round a capital's square: %d\n", g_squarePeople == (1 << 30) ? 0 : g_squarePeople);
  printf("towns: society requirements placed %d of %d (culture cases)\n", g_reqMet, g_reqAsked);
  printf("towns: %d failures\n", bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--towns", "settlements at scale: homes, services, reachability, walls and gates, palaces [--seeds A..B] [--png DIR] [--verbose] [--golden [--write]]", cmdTowns);
