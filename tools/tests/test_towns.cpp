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
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "rpg/sim/interior_v4.h"
#include "rpg/world/dmath.h"
#include "rpg/world/settlement.h"
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
};

void buildCase(const Case& c, ew::SitePlan& p, ew::KingdomPlan& k, ew::SettlementOut& so, double& ms) {
  p = ew::SitePlan();
  p.type = c.type;
  p.archetype = c.arch;
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
  ctx.rx = 3; ctx.ry = -8;
  ctx.roadBearings = c.roads;
  const int cxg = 1000, cyg = -2000, hw = p.w / 2;
  const Land land = c.land;
  ctx.base = [land, cxg, cyg, hw](int32_t gx, int32_t gy, Ground& g, Biome& bi, uint8_t& h) {
    g = Ground::Grass; bi = Biome::Plains; h = 1;
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
  auto t0 = std::chrono::steady_clock::now();
  ew::buildSettlement(ctx, so);
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
      if (!so.used.empty() && !so.used[(size_t)y * m.w + x]) c = art::mix(c, rgba(0, 0, 0), 0.35f);
      int p = m.propAt(x, y);
      if (p) c = (Prop)(p - 1) == Prop::Banner ? rgba(40, 70, 220) : rgba(30, 90, 30);
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
  if (floors < 2) { out("FAIL: %s: %s has %d floor(s), want 2+\n", what, bldgTypeName(b.type), floors); return 1; }
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
  if (b.type == Building::Palace) {
    if (!throne) { out("FAIL: %s: palace without a reachable throne in its throne hall\n", what); bad++; }
    if (!king) { out("FAIL: %s: palace without the king by the throne\n", what); bad++; }
    if (!owner) { out("FAIL: %s: palace without the royal bedchamber\n", what); bad++; }
    if (!council) { out("FAIL: %s: palace without a council room\n", what); bad++; }
    if (bedrooms < 1) { out("FAIL: %s: palace without bedchambers\n", what); bad++; }
  }
  if (b.type == Building::Barracks && (!bunks || !racks)) { out("FAIL: %s: barracks without bunks (%d) or weapon racks (%d)\n", what, (int)bunks, (int)racks); bad++; }
  return bad;
}

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
    if (!n[Building::Shop] && !n[Building::Smithy]) fail("village without a shop or smithy");
    if (!countProp(m, Prop::Well)) fail("village without a well");
  } else if (c.type == SiteType::Town) {
    if (!n[Building::Inn] || !n[Building::Shop] || !n[Building::Smithy] || !n[Building::Temple])
      fail("town services: inn %d shop %d smithy %d temple %d", n[Building::Inn], n[Building::Shop], n[Building::Smithy], n[Building::Temple]);
  } else {
    if (n[Building::Inn] < 2 || n[Building::Shop] < 2 || !n[Building::Temple] || !n[Building::Tower] || !n[Building::Keep])
      fail("city services: inn %d shop %d temple %d tower %d keep %d", n[Building::Inn], n[Building::Shop], n[Building::Temple], n[Building::Tower], n[Building::Keep]);
    if (c.capital && (!n[Building::Palace] || !n[Building::Barracks])) fail("capital without palace %d / barracks %d", n[Building::Palace], n[Building::Barracks]);
    if (!c.capital && n[Building::Palace]) fail("a palace in a city that is no capital");
  }
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
        if (groundWater(m.at(x, y)) || m.at(x, y) == Ground::Bridge) fail("%s stands on water", bldgTypeName(a.type));
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
    std::vector<int> q{hy * m.w + hx};
    f[(size_t)hy * m.w + hx] = 1;
    bool leaked = false;
    for (size_t h = 0; h < q.size() && !leaked; h++) {
      int x = q[h] % m.w, y = q[h] / m.w;
      if (x == 0 || y == 0 || x == m.w - 1 || y == m.h - 1) leaked = true;
      static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
      for (int k = 0; k < 4; k++) {
        int nx = x + dx[k], ny = y + dy[k];
        if (!m.in(nx, ny) || f[(size_t)ny * m.w + nx] || shut[(size_t)ny * m.w + nx]) continue;
        f[(size_t)ny * m.w + nx] = 1;
        q.push_back(ny * m.w + nx);
      }
    }
    if (leaked) fail("the city wall leaks (a flood from the heart leaves the ring with the gates shut)");
    if (so.gates.empty()) fail("no gatehouse");
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
  // capitals inside
  if (c.capital)
    for (const Bldg& b : m.bldgs)
      if (b.type == Building::Palace || b.type == Building::Barracks) bad += checkPalace(b, what);
  // banners where a kingdom rules
  if (c.type != SiteType::Village && !countProp(m, Prop::Banner)) fail("no kingdom banners");
  for (const Bldg& b : m.bldgs)
    if ((b.type == Building::Keep || b.type == Building::Palace || b.type == Building::Inn || b.type == Building::Barracks) && !b.banner)
      { fail("%s flies no banner", bldgTypeName(b.type)); break; }
  if (!countProp(m, Prop::Signpost)) fail("no signpost at the road entrances");
  return bad;
}

int cmdTowns(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  std::string pngDir;
  bool verbose = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--png") && i + 1 < argc) pngDir = argv[++i];
    else if (!strcmp(argv[i], "--verbose")) verbose = true;
  }
  int bad = 0;
  std::map<std::string, Timing> times;
  std::map<std::string, std::pair<int, int>> homes;   // type -> min, max
  static const ew::Archetype archs[] = {ew::Archetype::Plain, ew::Archetype::Farming, ew::Archetype::Fishing, ew::Archetype::Port,
                                        ew::Archetype::Mining, ew::Archetype::RiverCrossing, ew::Archetype::HillFort, ew::Archetype::Market};
  for (uint64_t s = a; s <= b; s++) {
    g_curSeed = s;
    std::vector<Case> cases;
    Rng r(s * 977 + 13);
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
    for (size_t ci = 0; ci < cases.size(); ci++) {
      const Case& c = cases[ci];
      ew::SitePlan p;
      ew::KingdomPlan k;
      ew::SettlementOut so, so2;
      double ms = 0, ms2 = 0;
      buildCase(c, p, k, so, ms);
      buildCase(c, p, k, so2, ms2);
      char what[160];
      std::snprintf(what, sizeof what, "seed %llu %s%s %s on %s%s", (unsigned long long)s, c.capital ? "capital " : "", siteTypeName(c.type),
                    archName(c.arch), landName(c.land), c.roads.empty() ? "" : " with roads");
      int fails = 0;
      if (townHash(so) != townHash(so2)) { out("FAIL: %s: not deterministic (two builds differ)\n", what); fails++; }
      fails += checkTown(c, p, so, what);
      bad += fails;
      std::string tn = c.capital ? "capital" : siteTypeName(c.type);
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
        std::snprintf(fn, sizeof fn, "%s/town_s%llu_%zu_%s_%s.png", pngDir.c_str(), (unsigned long long)s, ci, tn.c_str(), archName(c.arch));
        dumpPng(so, fn);
      }
    }
  }
  for (auto& [tn, T] : times) {
    auto hm = homes[tn];
    printf("towns: %-8s %3d built, homes %d..%d, build %.1f ms avg, %.1f ms max\n", tn.c_str(), T.n, hm.first, hm.second, T.n ? T.sum / T.n : 0.0, T.max);
    if (T.max > 60.0) { printf("FAIL: towns: %s build %.1f ms (budget about 25 ms native)\n", tn.c_str(), T.max); bad++; }
  }
  printf("towns: %d failures\n", bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--towns", "settlements at scale: homes, services, reachability, walls and gates, palaces [--seeds A..B] [--png DIR] [--verbose]", cmdTowns);
