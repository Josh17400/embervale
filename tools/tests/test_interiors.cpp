// rpg_test lane checks: interiors (BFS validity, free floor, clutter). M0 homes lane.
// Called once per seed after runSeed; returns failures, reports each with out("FAIL: ...").
//  M2: the classic island and its pre-M0b interiors (V2, V3) are retired; every check runs on the buildings of the seed's
//  endless world (the start window: the start village and whatever else it holds).
//  - v7 identity: for seeds 1..3 every floor of every building in the start window hashes to the recorded value.
//  - v7 rooms and storeys (M0b, VISION_PLAN 15.7): every floor of every building of the seed's start window plus
//    re-seeded variants (at least 200 buildings): no bed outside a bedroom-like room, every room and usable object
//    reachable from the floor's way in, stairs iff 2+ storeys and lined up between floors, every doorway legal, every
//    bed's head against a wall, and the per-type facts (inn: common room, kitchen with a fire, bar, rented rooms and
//    the innkeeper's room upstairs; shop: the counter between the customers and the stockroom; smithy: the forge on an
//    outer wall; temple: the altar on the axis; keep: a throne hall and the lord's quarters). One summary line reports
//    rooms per building, misplaced beds and the share of distinct layouts per building type.
//  - M1 palaces and barracks (TOWNS lane, VISION_PLAN 15.8): a dozen of each per seed in every main biome, all the
//    checks above, plus the palace's throne on the axis at the far wall with the king beside it, the royal bedchamber
//    and the council chamber upstairs (reached by the stairs and their doors), and the barracks' bunks upstairs and
//    weapon racks below.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <map>
#include <queue>
#include <set>
#include <string>
#include "rpg/culture/culture.h"
#include "rpg/culture/society.h"
#include "rpg/sim/deco.h"
#include "rpg/sim/interior_v4.h"
#include "tools/tests/tests.h"

namespace {
uint64_t fnv(uint64_t h, const void* p, size_t n) {
  const uint8_t* b = (const uint8_t*)p;
  for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; }
  return h;
}
uint64_t mapHash(const Map& m, uint64_t h) {
  h = fnv(h, &m.w, sizeof m.w); h = fnv(h, &m.h, sizeof m.h);
  h = fnv(h, &m.exitX, sizeof m.exitX); h = fnv(h, &m.exitY, sizeof m.exitY);
  h = fnv(h, m.ground.data(), m.ground.size()); h = fnv(h, m.prop.data(), m.prop.size()); h = fnv(h, m.deco.data(), m.deco.size());
  for (const Spawn& s : m.spawns) {
    int v[8] = {s.x, s.y, (int)s.npc, (int)s.mon, (int)s.role, (int)s.boss, s.site, s.slot};
    h = fnv(h, v, sizeof v);
  }
  return h;
}
// M0b integration: the M0b interiors (genInteriorV4, every floor, with stairs and rooms) of generator-v7 worlds. Once
// M0b ships, saves made on v7 worlds key looted chests, killed spawns and the rented room by tile, slot and room index,
// so any later change to these rooms needs a new WORLDGEN version. Re-record (EMB_INTERIOR_V7HASH=1 prints the values)
// only while v7 is unreleased.
// M0b fix round 1 (stairs two tiles wide, two-tile beds, keep/hut/smithy plans, clutter, upstairs residents)
// M2 phase A: re-recorded on the endless start window (the classic island is retired); the rooms generator is unchanged
// M2 phase B (SIM lane): the start window's buildings change whenever the world generator does (the M2 lanes change its
// output under ENDLESS_GEN_VER 6), which has nothing to do with the rooms generator. The hash now covers a FROZEN sample
// of building inputs (type, footprint, storeys, hearth, biome, urban level, variant: the kinds the endless towns make,
// taken from the start windows of seeds 1-3 on 2026-10-05), three seeds each, so only a rooms-generator change moves it.
struct SampleBldg { int type, w, h, storeys, hearth, biome, urban, variant; };
const SampleBldg kGen7Sample[] = {
    {0, 3, 2, 1, 1, 2, 0, 0}, {0, 3, 2, 1, 1, 2, 3, 0}, {0, 4, 2, 2, 1, 2, 0, 0}, {0, 4, 2, 2, 1, 2, 3, 0},
    {1, 3, 2, 1, 1, 2, 3, 0}, {1, 3, 2, 2, 1, 2, 3, 0}, {2, 5, 3, 2, 1, 2, 0, 0}, {2, 5, 3, 2, 1, 2, 3, 0},
    {3, 5, 3, 1, 1, 2, 0, 0}, {3, 5, 3, 1, 1, 2, 3, 0}, {4, 4, 3, 1, 0, 2, 0, 0}, {4, 4, 3, 1, 0, 2, 3, 0},
    {4, 4, 3, 1, 1, 3, 0, 0}, {4, 4, 3, 1, 1, 5, 3, 0}, {4, 4, 3, 2, 1, 2, 0, 0}, {4, 4, 3, 2, 1, 2, 3, 0},
    {5, 7, 5, 1, 0, 2, 3, 0}, {6, 9, 4, 2, 1, 2, 3, 0}, {7, 3, 3, 3, 0, 2, 3, 0}, {8, 5, 3, 1, 1, 2, 0, 0},
    {9, 3, 2, 1, 1, 2, 0, 0}, {9, 3, 2, 1, 1, 2, 3, 0}, {10, 15, 7, 2, 1, 2, 3, 0}, {11, 7, 4, 2, 1, 2, 3, 0},
    {12, 4, 3, 2, 0, 2, 3, 0}, {15, 4, 3, 1, 1, 2, 3, 0}, {15, 4, 3, 2, 1, 5, 3, 0}, {16, 4, 3, 1, 1, 2, 3, 0},
    {16, 4, 3, 2, 1, 2, 3, 0}, {17, 4, 3, 1, 1, 2, 0, 0}, {17, 4, 3, 1, 1, 2, 3, 0}, {19, 5, 3, 1, 1, 3, 0, 0},
    {20, 5, 3, 1, 0, 5, 3, 0}, {21, 4, 3, 1, 0, 5, 3, 0}, {21, 4, 3, 1, 1, 2, 3, 0},
};
const uint64_t kGen7SampleHash = 0x8f34185b15d8592cull;   // (EMB_INTERIOR_V7HASH=1 prints the value)

uint64_t floorHash(const Map& m, uint64_t h) {
  h = mapHash(m, h);
  int v[10] = {m.floor, m.w, m.h, m.exitX, m.exitY, m.up.x, m.up.y, m.down.x, m.down.y, (int)m.rooms.size()};
  h = fnv(h, v, sizeof v);
  for (const RoomInfo& r : m.rooms) {
    int q[10] = {(int)r.kind, r.r.x, r.r.y, r.r.w, r.r.h, r.doorX, r.doorY, r.bedX, r.bedY, (int)r.guest};
    h = fnv(h, q, sizeof q);
  }
  return h;
}

uint64_t sampleHash() {
  uint64_t h = 1469598103934665603ull;
  int n = 0;
  for (const SampleBldg& sb : kGen7Sample)
    for (int k = 0; k < 3; k++, n++) {
      Bldg b;
      b.type = (art::Building)sb.type;
      b.r = IRect{40, 40, sb.w, sb.h};
      b.storeys = (uint8_t)sb.storeys; b.hearth = sb.hearth != 0; b.biome = (Biome)sb.biome; b.urban = (uint8_t)sb.urban; b.variant = (uint8_t)sb.variant;
      b.seed = hash32((uint32_t)n * 2654435761u + 0x7E57u);
      b.site = 0;
      b.genVer = WORLDGEN_LATEST;
      for (int f = 0; f < b.floors(); f++) {
        Map m;
        genInterior(m, b, b.seed, f);
        h = floorHash(m, h);
      }
    }
  return h;
}

bool wallDecor(art::Prop p) { return p >= art::Prop::Tapestry && p <= art::Prop::HolySymbol; }

using art::Prop;
using art::Building;
bool isProp(const Map& m, int x, int y, Prop p) { return m.propAt(x, y) == (int)p + 1; }
bool wallT(const Map& m, int x, int y) { return !m.in(x, y) || m.at(x, y) == Ground::InteriorWall || m.at(x, y) == Ground::Void; }

struct V7Stats {
  int bldgs = 0, floors = 0, rooms = 0, bad = 0, misplacedBeds = 0, beds = 0;
  // the building being checked (reset per building): an inn's guest rooms over all its floors, the innkeeper's room
  int innGuests = 0;
  std::vector<int> innNums;
  bool innOwner = false;
  std::string tmpl;   // interiorTemplate of the building
  double freeMin = 1;
  std::map<int, std::set<uint64_t>> sigs;   // per type: distinct layouts (walls, doors, room kinds, stairs)
  std::map<int, std::set<uint64_t>> furn;   // per type: distinct furnished layouts (plus every piece of furniture)
  std::map<int, int> count;
};

// one floor: returns a failure, or "" when fine
std::string checkFloorV7(const Map& m, const Bldg& b, int f, V7Stats& st) {
  const int W = m.w, H = m.h, floors = b.floors();
  char buf[200];
  auto I = [&](int x, int y) { return (size_t)y * W + x; };
  auto floorT = [&](int x, int y) { return m.in(x, y) && !groundSolid(m.at(x, y)); };
  if (m.floor != f) return "map floor index wrong";
  if ((int)m.roomAt.size() != W * H || m.rooms.empty()) return "no rooms recorded";
  // stairs iff 2+ storeys: up iff f < floors-1, down iff f > 0
  if (m.up.valid() != (f < floors - 1)) { std::snprintf(buf, sizeof buf, "floor %d of %d: stairs up %d", f, floors, (int)m.up.valid()); return buf; }
  if (m.down.valid() != (f > 0)) { std::snprintf(buf, sizeof buf, "floor %d of %d: stairs down %d", f, floors, (int)m.down.valid()); return buf; }
  if (m.up.valid() && (!isProp(m, m.up.x, m.up.y, Prop::StairsUp) || !wallT(m, m.up.x, m.up.y - 1) || !wallT(m, m.up.x, m.up.y - 2))) return "stairs up not under a wall face";
  if (m.down.valid() && !isProp(m, m.down.x, m.down.y, Prop::StairsDown)) return "stairs down prop missing";
  if ((f == 0) != (m.exitX >= 0)) return "exit on the wrong floor";
  // the way in: the front door, or the stairs you came up by
  int sx = f == 0 ? m.exitX : m.down.ax, sy = f == 0 ? m.exitY : m.down.ay;
  if (!floorT(sx, sy) || m.blocked(sx, sy)) return "way in blocked";
  if (f == 0 && m.blocked(m.exitX, m.exitY - 1)) return "arrival tile blocked";
  // the arrival tile is beside the flight: next to its trigger tile, or (fix round 3, a stairwell down) next to the
  // flight's second tile on the far side
  auto besideFlight = [&](const Stairs& s) {
    if (std::abs(s.ax - s.x) + std::abs(s.ay - s.y) == 1) return true;
    int mx = (s.ax + s.x) / 2;
    return s.ay == s.y && std::abs(s.ax - s.x) == 2 && m.propAt(mx, s.y) == m.propAt(s.x, s.y);
  };
  for (const Stairs* s : {&m.up, &m.down})
    if (s->valid() && (m.blocked(s->ax, s->ay) || m.blocked(s->x, s->y) || !besideFlight(*s))) return "stairs or arrival tile blocked";
  std::vector<int> dist((size_t)W * H, -1);
  std::vector<int> q{(int)I(sx, sy)};
  dist[I(sx, sy)] = 0;
  for (size_t h = 0; h < q.size(); h++) {
    int x = q[h] % W, y = q[h] / W;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) {
      int nx = x + dx[k], ny = y + dy[k];
      if (!floorT(nx, ny) || m.solid[I(nx, ny)] || dist[I(nx, ny)] >= 0) continue;
      dist[I(nx, ny)] = dist[(size_t)q[h]] + 1;
      q.push_back((int)I(nx, ny));
    }
  }
  auto reached = [&](int x, int y) { return m.in(x, y) && dist[I(x, y)] >= 0; };
  auto beside = [&](int x, int y) { return reached(x + 1, y) || reached(x - 1, y) || reached(x, y + 1) || reached(x, y - 1); };
  for (const Stairs* s : {&m.up, &m.down})
    if (s->valid() && (!reached(s->x, s->y) || !reached(s->ax, s->ay))) return "stairs unreachable (floor not returnable)";
  if (f == 0 && !reached(m.exitX, m.exitY - 1)) return "front door unreachable";
  // every room reachable, every floor tile in a room
  std::vector<int> roomReached(m.rooms.size(), 0);
  int floorN = 0, freeN = 0;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (!floorT(x, y)) continue;
      floorN++;
      int ri = m.roomIndexAt(x, y);
      if (ri < 0 || ri >= (int)m.rooms.size()) { std::snprintf(buf, sizeof buf, "floor tile %d,%d in no room", x, y); return buf; }
      if (reached(x, y)) { freeN++; roomReached[(size_t)ri] = 1; }
    }
  for (size_t i = 0; i < m.rooms.size(); i++)
    if (!roomReached[i]) {
      if (std::getenv("EMB_M3B_DUMP"))
        for (int y = 0; y < H; y++) {
          std::string row;
          for (int x = 0; x < W; x++) row += !floorT(x, y) ? '#' : (reached(x, y) ? '*' : (m.solid[I(x, y)] ? 'o' : '.'));
          out("    %s\n", row.c_str());
        }
      std::snprintf(buf, sizeof buf, "room %zu (%s) unreachable", i, roomKindName(m.rooms[i].kind));
      return buf;
    }
  double fr = floorN ? (double)freeN / floorN : 0;
  st.freeMin = std::min(st.freeMin, fr);
  if (fr < 0.6) { std::snprintf(buf, sizeof buf, "only %.0f%% of the floor walkable", fr * 100); return buf; }
  // props: beds, usable objects, doors, wall decor, clutter
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      int p = m.propAt(x, y), d = m.decoAt(x, y);
      if (d >= (int)Deco::Basket && d <= (int)Deco::Kindling && (!floorT(x, y) || p)) { std::snprintf(buf, sizeof buf, "clutter not on free floor at %d,%d", x, y); return buf; }
      if (!p) continue;
      Prop pp = (Prop)(p - 1);
      if (wallDecor(pp)) {
        if (floorT(x, y) || !floorT(x, y + 1)) { std::snprintf(buf, sizeof buf, "wall decor %d not on a wall face at %d,%d", p - 1, x, y); return buf; }
        continue;
      }
      if (!floorT(x, y)) { std::snprintf(buf, sizeof buf, "prop %d inside a wall at %d,%d", p - 1, x, y); return buf; }
      // (M3: a hammock or a sleeping mat is a bed of a culture that sleeps so)
      if (pp == Prop::Bed || pp == Prop::BunkBed || pp == Prop::Hammock || pp == Prop::SleepingMat) {
        st.beds++;
        int ri = m.roomIndexAt(x, y);
        if (ri < 0 || !roomAllowsBed(m.rooms[(size_t)ri].kind)) {
          st.misplacedBeds++;
          std::snprintf(buf, sizeof buf, "bed at %d,%d in a %s", x, y, ri >= 0 ? roomKindName(m.rooms[(size_t)ri].kind) : "wall");
          return buf;
        }
        // head to the wall: a one-tile bed right under it, a two-tile bed (M0b fix round) with its head tile (Filler) there
        bool head = wallT(m, x, y - 1) || (pp != Prop::BunkBed && isProp(m, x, y - 1, Prop::Filler) && wallT(m, x, y - 2));
        if (!head) { std::snprintf(buf, sizeof buf, "bed at %d,%d has no wall at its head", x, y); return buf; }
      }
      if ((pp == Prop::Bed || pp == Prop::BunkBed || pp == Prop::Chest || pp == Prop::Altar || pp == Prop::Hammock) && !beside(x, y)) { std::snprintf(buf, sizeof buf, "usable prop %d at %d,%d unreachable", p - 1, x, y); return buf; }
      if (pp == Prop::DoorH) {
        bool ok = floorT(x, y - 1) && wallT(m, x - 1, y) && wallT(m, x + 1, y) && wallT(m, x - 1, y - 1) && wallT(m, x + 1, y - 1) && floorT(x, y + 1) && floorT(x, y - 2) &&
                  reached(x, y + 1) && reached(x, y - 2);
        if (!ok) { std::snprintf(buf, sizeof buf, "illegal E-W doorway at %d,%d", x, y); return buf; }
      }
      if (pp == Prop::DoorV) {
        bool ok = wallT(m, x, y - 1) && wallT(m, x, y + 1) && floorT(x - 1, y) && floorT(x + 1, y) && reached(x - 1, y) && reached(x + 1, y);
        if (!ok) { std::snprintf(buf, sizeof buf, "illegal N-S doorway at %d,%d", x, y); return buf; }
      }
      if (pp == Prop::DoorH || pp == Prop::DoorV)   // never next to another doorway
        for (int oy = -2; oy <= 2; oy++)
          for (int ox = -1; ox <= 1; ox++) {
            if (!ox && !oy) continue;
            int q2 = m.propAt(x + ox, y + oy);
            if ((q2 == (int)Prop::DoorH + 1 || q2 == (int)Prop::DoorV + 1) && std::abs(oy) <= 1) { std::snprintf(buf, sizeof buf, "doorways side by side at %d,%d", x, y); return buf; }
          }
    }
  for (const Spawn& s : m.spawns) {
    if (!reached(s.x, s.y)) { std::snprintf(buf, sizeof buf, "spawn role %d at %d,%d unreachable", (int)s.role, s.x, s.y); return buf; }
    if (s.slot / 16 != f) { std::snprintf(buf, sizeof buf, "spawn slot %d on floor %d", s.slot, f); return buf; }
  }
  // every carved room has its door recorded where the door prop stands
  for (size_t i = 0; i < m.rooms.size(); i++) {
    const RoomInfo& R = m.rooms[i];
    if (R.doorX >= 0 && !isProp(m, R.doorX, R.doorY, Prop::DoorH) && !isProp(m, R.doorX, R.doorY, Prop::DoorV)) return "room door without a door prop";
    if (R.bedX >= 0 && !isProp(m, R.bedX, R.bedY, Prop::Bed) && !isProp(m, R.bedX, R.bedY, Prop::BunkBed) && !isProp(m, R.bedX, R.bedY, Prop::Hammock) &&
        !isProp(m, R.bedX, R.bedY, Prop::SleepingMat))
      return "room bed without a bed";
  }
  // per type
  auto has = [&](RoomKind k) { for (auto& R : m.rooms) if (R.kind == k) return true; return false; };
  auto propIn = [&](int ri, std::initializer_list<Prop> ps) {
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++)
        if (m.roomIndexAt(x, y) == ri) for (Prop p : ps) if (isProp(m, x, y, p)) return true;
    return false;
  };
  if (b.type == Building::Inn && f == 0) {
    int common = -1, kit = -1;
    for (size_t i = 0; i < m.rooms.size(); i++) {
      if (m.rooms[i].kind == RoomKind::Common) common = (int)i;
      if (m.rooms[i].kind == RoomKind::Kitchen) kit = (int)i;
    }
    if (common < 0 || kit < 0) return "inn ground floor lacks a common room or kitchen";
    if (!propIn(kit, {Prop::Hearth, Prop::Oven})) return "inn kitchen has no hearth or oven";
    if (!propIn(common, {Prop::CounterL, Prop::CounterM, Prop::CounterR})) return "inn common room has no bar counter";
    // (M3b: a single-storey inn lets its rooms on the ground floor, behind partitions; no bed stands in the common room)
    for (int i = 0; i < W * H; i++)
      if (m.prop[(size_t)i] == (int)Prop::Bed + 1 && m.roomAt[(size_t)i] >= 0 && m.rooms[(size_t)m.roomAt[(size_t)i]].kind == RoomKind::Common) return "a bed in the inn's common room";
  }
  if (b.type == Building::Inn) {
    for (size_t i = 0; i < m.rooms.size(); i++) {
      const RoomInfo& R = m.rooms[i];
      if (R.kind != RoomKind::GuestRoom) continue;
      st.innGuests++;
      st.innNums.push_back(R.guest);
      if (R.doorX < 0 || R.bedX < 0) return "guest room without a door or a bed";
      if (!propIn((int)i, {Prop::Chest})) return "guest room without a chest";
      if (!propIn((int)i, {Prop::Nightstand, Prop::TableSmall})) return "guest room without a nightstand or table";
    }
    if (has(RoomKind::OwnerRoom)) st.innOwner = true;
  }
  if (b.type == Building::Shop && f == 0) {
    int er = m.roomIndexAt(m.exitX, m.exitY - 1);
    if (er < 0 || m.rooms[(size_t)er].kind == RoomKind::Stockroom) return "shop entrance in the stockroom";
    // (M3b) behind a front court the shop floor is the room the court's door leads into
    for (size_t i = 0; i < m.rooms.size(); i++) if (m.rooms[i].kind == RoomKind::Shopfloor) { er = (int)i; break; }
    int cy = -1, cx0 = W, cx1 = -1;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++)
        if (m.roomIndexAt(x, y) == er && (isProp(m, x, y, Prop::CounterL) || isProp(m, x, y, Prop::CounterM) || isProp(m, x, y, Prop::CounterR))) { cy = y; cx0 = std::min(cx0, x); cx1 = std::max(cx1, x); }
    if (cy < 0) return "shop without a counter";
    for (const RoomInfo& R : m.rooms)
      if (R.kind == RoomKind::Stockroom && (R.doorY >= cy || R.doorX < cx0 - 1 || R.doorX > cx1 + 1)) return "the counter does not stand between customers and the stockroom";
    if (!has(RoomKind::Stockroom)) return "shop without a stockroom";
  }
  if (b.type == Building::Smithy && f == 0) {
    // (M3b: on a shaped floor, an L's body, the forge's wall may be the wall it shares with the wing, its flue in it)
    bool shaped = false;
    for (int i = 0; i < W * H && !shaped; i++) shaped = m.ground[(size_t)i] == (uint8_t)Ground::Void;
    bool ok = false;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++)
        if (isProp(m, x, y, Prop::Forge) && (y == 2 || x <= 2 || x >= W - 3 || m.decoAt(x, y - 1) == (int)Deco::Shell || (shaped && wallT(m, x, y - 1)))) ok = true;
    if (!ok) return "the forge is not on an outer wall";
  }
  if (b.type == Building::Temple && f == 0 && !isProp(m, m.exitX, 2, Prop::Altar)) return "the altar is not on the temple's axis at the far wall";
  if (b.type == Building::Keep && f == 0 && !has(RoomKind::ThroneHall)) return "keep without a throne hall";
  const bool castle = st.tmpl == "classic" || st.tmpl == "seat: castle";
  if (b.type == Building::Palace && f == 0 && castle) {
    if (!has(RoomKind::ThroneHall)) return "palace without a throne hall";
    if (!isProp(m, m.exitX, 2, Prop::Throne)) return "the palace's throne is not on the axis at the far wall";
    if (!has(RoomKind::Kitchen) || !has(RoomKind::Barracks)) return "palace without its kitchen or guardroom";
  }
  if (b.type == Building::Palace && f == 1 && castle && (!has(RoomKind::OwnerRoom) || !has(RoomKind::Council) || !has(RoomKind::Bedroom)))
    return "palace upstairs lacks the king's bedchamber, the council chamber or the household's bedchambers";
  // (M3b) the royal seat, whatever the society built it as: the king on its ground floor beside his throne or dais
  if (bldgIsRoyalSeat(b) && f == 0) {
    int tx = -1, ty = -1;
    for (int y = 0; y < H && tx < 0; y++)
      for (int x = 0; x < W; x++) if (isProp(m, x, y, Prop::Throne)) { tx = x; ty = y; break; }
    if (tx < 0) return "the royal seat has no throne or high seat on its ground floor";
    bool king = false;
    for (const Spawn& s : m.spawns) if (s.role == Role::King && std::abs(s.x - tx) <= 3 && std::abs(s.y - ty) <= 3) king = true;
    if (!king) return "no king by the royal seat's throne";
  }
  if (b.type == Building::Barracks) {
    bool bunks = false, racks = false;
    for (int i = 0; i < W * H; i++) { bunks |= m.prop[(size_t)i] == (int)Prop::BunkBed + 1; racks |= m.prop[(size_t)i] == (int)Prop::WeaponRack + 1; }
    if (f == 1 && (!has(RoomKind::Barracks) || !bunks)) return "barracks upstairs without a dormitory of bunks";
    if (f == 0 && !racks) return "barracks without weapon racks";
  }
  st.rooms += (int)m.rooms.size();
  return "";
}

// debugging aid: EMB_INTERIOR_DUMP=<type>[:<index>] rpg_test <seed> prints the floors of that type's buildings as text
void dumpFloor(const Map& m) {
  for (int y = 0; y < m.h; y++) {
    std::string row;
    for (int x = 0; x < m.w; x++) {
      int p = m.propAt(x, y);
      char c = m.at(x, y) == Ground::InteriorWall ? '#' : '.';
      if (p) {
        Prop pp = (Prop)(p - 1);
        switch (pp) {
          case Prop::Bed: c = 'B'; break; case Prop::BunkBed: c = 'b'; break; case Prop::Chest: c = 'c'; break;
          case Prop::DoorH: c = '='; break; case Prop::DoorV: c = '|'; break; case Prop::StairsUp: c = '^'; break; case Prop::StairsDown: c = 'v'; break;
          case Prop::Hearth: case Prop::Oven: case Prop::Forge: c = 'H'; break; case Prop::Filler: c = 'h'; break;
          case Prop::CounterL: case Prop::CounterM: case Prop::CounterR: c = 'C'; break;
          case Prop::TableL: case Prop::TableM: case Prop::TableR: case Prop::TableSmall: case Prop::TableMeal: case Prop::TableWork: case Prop::DisplayTable: c = 'T'; break;
          case Prop::Chair: case Prop::Stool: case Prop::Bench: c = 's'; break;
          case Prop::Altar: c = 'A'; break; case Prop::Throne: c = 'K'; break; case Prop::Anvil: c = 'a'; break; case Prop::Pillar: c = 'P'; break;
          case Prop::Nightstand: c = 'n'; break; case Prop::Barrel: case Prop::Barrel2: case Prop::Crate: c = 'o'; break;
          case Prop::Window: c = 'w'; break;
          default: c = wallDecor(pp) ? '"' : 'x'; break;
        }
      }
      row += c;
    }
    std::string kinds;
    for (int x = 0; x < m.w; x++) { int ri = m.roomIndexAt(x, y); kinds += ri < 0 ? ' ' : (char)('0' + (int)m.rooms[(size_t)ri].kind % 10 + ((int)m.rooms[(size_t)ri].kind >= 10 ? 17 : 0)); }
    out("  %s   %s\n", row.c_str(), kinds.c_str());
  }
}

// all floors of one building; sig: its layout signature (all floors). bp: the blueprint to plan from (null: the
// building's own, bldgBlueprint)
std::string checkBuildingV7(const Bldg& b, V7Stats& st, uint64_t& sig, const bld::Blueprint* bp = nullptr) {
  int floors = b.floors();
  sig = 1469598103934665603ull;
  uint64_t fsig = 1469598103934665603ull;
  Stairs prevUp;
  bool owner = false;
  const bld::Blueprint own = bp ? *bp : bldgBlueprint(b);
  st.tmpl = interiorTemplate(b, own, b.seed);
  st.innGuests = 0; st.innNums.clear(); st.innOwner = false;
  for (int f = 0; f < floors; f++) {
    Map m;
    genInteriorRooms(m, b, own, b.seed, f);
    st.floors++;
    std::string why = checkFloorV7(m, b, f, st);
    if (!why.empty()) {
      if (std::getenv("EMB_M3B_ALL")) { out("  (%s, plan %d)\n", st.tmpl.c_str(), (int)own.interior.plan); dumpFloor(m); }
      return "floor " + std::to_string(f) + ": " + why;
    }
    if (f > 0 && (m.down.x != prevUp.x || m.down.y != prevUp.y)) return "floor " + std::to_string(f) + ": stairs do not line up with the floor below";
    prevUp = m.up;
    for (auto& R : m.rooms) if (R.kind == RoomKind::OwnerRoom) owner = true;
    sig ^= interiorLayoutSignature(m) + (uint64_t)f * 0x9E3779B97F4A7C15ull;
    sig *= 1099511628211ull;
    fsig = fnv(fsig ^ sig, m.prop.data(), m.prop.size());
  }
  const bool castle = st.tmpl == "classic" || st.tmpl == "seat: castle";
  if ((b.type == Building::Keep || b.type == Building::Palace) && castle && !owner) return "keep or palace without the lord's quarters";
  if (b.type == Building::Inn) {   // (M3b) the rented rooms on whichever floor, numbered across the building
    std::sort(st.innNums.begin(), st.innNums.end());
    for (size_t k = 0; k < st.innNums.size(); k++) if (st.innNums[k] != (int)k) return "guest rooms not numbered 0..n-1 across the building";
    if (st.innGuests < 2) return "an inn with fewer than 2 rooms to let";
    if (!st.innOwner) return "an inn without the innkeeper's own room";
  }
  st.sigs[(int)b.type].insert(sig);
  st.furn[(int)b.type].insert(fsig);
  st.count[(int)b.type]++;
  return "";
}
}  // namespace

// ---- M3b (VISION_PLAN 15.14): interiors derived from the builder's blueprints ------------------------------------
// Every purpose (all art::Building values) x the 12 cultures, on its natural blueprint (what bld::design gives for the
// culture), with the seats of power of every society (the ruler's and the lord's), and every purpose on each forced
// floor plan (Rect, Round, L, Courtyard, Long, Cross; the culture and the footprint rotate with the seed): every 15.7
// check above (checkBuildingV7), plus:
//  - a purpose with a plan of its own never falls back to the plain plan;
//  - a Round blueprint gives a round floor: the corners of the floor area are void, the floor about a disc's share;
//  - a Courtyard blueprint gives an open court: a Court room under the sky (Plaza ground) with rooms round it whose
//    doors open onto it;
//  - inns: at least 8 distinct plan templates and 8 distinct ground-floor layouts across the 12 cultures.
namespace {
const cult::Culture& cultureFor(int a) {
  static std::vector<cult::Culture> cs;
  if (cs.empty())
    for (int k = 0; k < (int)cult::Archetype::COUNT; k++) cs.push_back(cult::Atlas::make((cult::Archetype)k, 1000u + (uint32_t)k * 7919u, 2));
  return cs[(size_t)a];
}
void footprintOf(Building t, int& w, int& h) {
  switch (t) {
    case Building::Inn: w = 6; h = 3; return;
    case Building::Keep: w = 9; h = 4; return;
    case Building::Palace: w = 15; h = 7; return;
    case Building::Barracks: w = 7; h = 4; return;
    case Building::Temple: w = 6; h = 4; return;
    case Building::Hut: w = 3; h = 2; return;
    case Building::Tower: w = 3; h = 3; return;
    case Building::Farmhouse: w = 5; h = 3; return;
    case Building::Guildhall: case Building::MeadHall: case Building::CouncilHall: w = 7; h = 4; return;
    case Building::Exchange: case Building::Bathhouse: case Building::Lodge: w = 6; h = 4; return;
    case Building::TeaHouse: w = 5; h = 3; return;
    case Building::Smithy: case Building::Smelter: case Building::Sawmill: w = 5; h = 3; return;
    default: w = 4; h = 3; return;
  }
}
Role ownerOf(Building t) {
  switch (t) {
    case Building::Inn: case Building::MeadHall: case Building::TeaHouse: case Building::Bathhouse: return Role::Innkeeper;
    case Building::Shop: case Building::Bakery: case Building::Butcher: case Building::Fishmonger: case Building::Weaver: case Building::Exchange: case Building::Guildhall: return Role::Merchant;
    case Building::Smithy: case Building::Smelter: return Role::Smith;
    case Building::Temple: return Role::Priest;
    case Building::Keep: return Role::Jarl;
    case Building::Palace: return Role::King;
    case Building::Barracks: case Building::Lodge: return Role::Guard;
    case Building::Tower: return Role::Mage;
    case Building::Farmhouse: case Building::Windmill: case Building::Watermill: case Building::Granary: return Role::Farmer;
    default: return Role::Villager;
  }
}
// a building of purpose t in culture a (urban 0..3, wealth 0..3), its storeys from its blueprint (what the TOWNS lane
// writes), the seat bits when seat >= 0 (cult::Seat; royal: the capital's)
Bldg cultureBldg(Building t, int a, uint32_t seed, int urban, int wealth, int seat, bool royal, bld::Blueprint& bp, int form = 0) {
  Bldg b;
  b.type = t;
  int w, h;
  footprintOf(t, w, h);
  if (seat >= 0) { w = std::max(w, royal ? 13 : 9); h = std::max(h, royal ? 6 : 5); }
  b.r = IRect{20, 20, w + (int)(seed % 2), h};
  b.owner = ownerOf(t);
  b.seed = hash32(seed ^ ((uint32_t)t * 2654435761u) ^ ((uint32_t)a * 40503u));
  b.genVer = WORLDGEN_LATEST;
  b.biome = Biome::Plains;
  b.urban = (uint8_t)urban;
  b.wealth = (uint8_t)wealth;
  b.hearth = true;
  b.site = 0;
  b.styled = true;
  b.arch = cult::buildingArch(cultureFor(a), 2, urban, wealth, b.seed);
  b.storeys = (uint8_t)bldgStoreysV7(t, b.r.w, b.r.h, b.seed);
  b.form = (uint8_t)form;
  if (seat >= 0) { b.civic = bld::CIVIC_SEAT; b.seat = (uint8_t)(seat + 1); b.urban = royal ? 3 : 2; b.owner = royal ? Role::King : Role::Jarl; }
  bp = bldgBlueprint(b);
  b.storeys = (uint8_t)std::clamp((int)bp.interior.floors, 1, 3);
  return b;
}
// shape audits on floor 0 of a building planned from bp
std::string shapeCheck(const Bldg& b, const bld::Blueprint& bp, bool& court) {
  Map m;
  genInteriorRooms(m, b, bp, b.seed, 0);
  court = false;
  if (bp.interior.plan == bld::Floorplan::Round) {
    int voids = 0, fl = 0, area = 0;
    for (int y = 2; y <= m.h - 2; y++)
      for (int x = 1; x <= m.w - 2; x++) { area++; fl += m.at(x, y) != Ground::Void && m.decoAt(x, y) != (int)Deco::Shell; }
    for (auto c : {std::pair<int, int>{1, 2}, {m.w - 2, 2}, {1, m.h - 2}, {m.w - 2, m.h - 2}}) voids += m.at(c.first, c.second) == Ground::Void;
    int anyVoid = 0;
    for (int i = 0; i < m.w * m.h; i++) anyVoid += m.ground[(size_t)i] == (uint8_t)Ground::Void;
    voids = 0;
    for (auto c : {std::pair<int, int>{1, 2}, {m.w - 2, 2}, {1, m.h - 2}, {m.w - 2, m.h - 2}}) voids += groundSolid(m.at(c.first, c.second));
    if (voids < 4 || !anyVoid) return "a round blueprint's floor has square corners";
    if (fl * 100 < area * 66 || fl * 100 > area * 88) return "a round blueprint's floor is not a disc (" + std::to_string(fl * 100 / std::max(1, area)) + "% of its square)";
  }
  if (bp.interior.plan == bld::Floorplan::Courtyard) {
    int ci = -1;
    for (size_t i = 0; i < m.rooms.size(); i++) if (m.rooms[i].kind == RoomKind::Court) ci = (int)i;
    if (ci < 0) return "a courtyard blueprint without an open court";
    int open = 0, onto = 0;
    for (int y = 0; y < m.h; y++)
      for (int x = 0; x < m.w; x++) if (m.roomIndexAt(x, y) == ci && m.at(x, y) == Ground::Plaza) open++;
    if (open < 6) return "the court is not open to the sky";
    for (size_t i = 0; i < m.rooms.size(); i++) {
      const RoomInfo& R = m.rooms[i];
      if ((int)i == ci || R.doorX < 0) continue;
      for (int k = 0; k < 4; k++) {
        static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        if (m.roomIndexAt(R.doorX + dx[k], R.doorY + dy[k]) == ci && m.roomIndexAt(R.doorX + dx[k], R.doorY + dy[k]) != (int)i) { onto++; break; }
      }
    }
    if ((int)ci != 0 || m.rooms[0].kind == RoomKind::Court) {}
    if (onto < 1) return "no room opens onto the court";
    court = true;
  }
  return "";
}
}  // namespace

int m3bInteriorChecks(uint64_t seed, V7Stats& s7) {
  auto t0 = std::chrono::steady_clock::now();
  const int NA = (int)cult::Archetype::COUNT, NB = (int)Building::COUNT;
  int n = 0, bad = 0, plainFalls = 0, rounds = 0, courts = 0;
  std::map<std::string, int> tmplCount;
  auto fail = [&](const char* what, const Bldg& b, int a, const std::string& why) {
    if (bad < (std::getenv("EMB_M3B_ALL") ? 400 : 8)) out("FAIL: m3b %s %s (%s, %dx%d, %d floors, form %d): %s\n", what, bldgTypeName(b.type), cult::archetypeName((cult::Archetype)a), b.r.w, b.r.h, b.floors(), (int)b.form, why.c_str());
    bad++;
  };
  // purposes whose plain fallback means a missing plan
  auto ownPlan = [](Building t) {
    switch (t) {
      case Building::Inn: case Building::Guildhall: case Building::Exchange: case Building::MeadHall: case Building::Bathhouse:
      case Building::TeaHouse: case Building::Lodge: case Building::CouncilHall: case Building::Palace: case Building::Keep:
      case Building::Temple: case Building::Shop: case Building::Smithy: case Building::House: case Building::StoneHouse: return true;
      default: return false;
    }
  };
  // 1. every purpose x every culture on its natural blueprint (urban / wealth rotate)
  for (int t = 0; t < NB; t++)
    for (int a = 0; a < NA; a++) {
      bld::Blueprint bp;
      const int urban = (int)((seed + (uint64_t)t + (uint64_t)a) % 4), wealth = (int)((seed * 3 + (uint64_t)a) % 4);
      Bldg b = cultureBldg((Building)t, a, (uint32_t)seed * 977u + 13u, urban, wealth, -1, false, bp);
      // debugging aid: EMB_M3B_DUMP=<type>:<culture> prints that building's floors and its template
      if (const char* dd = std::getenv("EMB_M3B_DUMP")) {
        const char* c = std::strchr(dd, ':');
        if (std::atoi(dd) == t && c && std::atoi(c + 1) == a) {
          static int why[8192];
          interiorWhy(why, 8192, true);
          out("m3b %s %s: template %s, plan %d, %d floors\n", bldgTypeName(b.type), cult::archetypeName((cult::Archetype)a), interiorTemplate(b, bp, b.seed), (int)bp.interior.plan, b.floors());
          interiorWhy(why, 8192, true);
          std::string ws;
          for (int i = 0; i < 8192; i++) if (why[i]) ws += " line " + std::to_string(i) + " x" + std::to_string(why[i]) + ";";
          out("  turned down at:%s\n", ws.c_str());
          for (int f = 0; f < b.floors(); f++) { Map m; genInteriorRooms(m, b, bp, b.seed, f); dumpFloor(m); }
        }
      }
      uint64_t sig = 0;
      std::string why = checkBuildingV7(b, s7, sig, &bp);
      n++;
      tmplCount[s7.tmpl]++;
      if (why.empty() && ownPlan(b.type) && s7.tmpl == "plain") { why = "fell back to the plain plan"; plainFalls++; }
      if (why.empty()) { bool c; why = shapeCheck(b, bp, c); rounds += bp.interior.plan == bld::Floorplan::Round; courts += c; }
      if (!why.empty()) fail("natural", b, a, why);
    }
  // 2. the seats of power: every culture's society, the ruler's and the lord's
  for (int a = 0; a < NA; a++) {
    const cult::Society S = cult::societyOf(cultureFor(a));
    for (int royal = 0; royal < 2; royal++) {
      bld::Blueprint bp;
      const Building t = cult::seatPurpose(S.seat, !royal);
      Bldg b = cultureBldg(t, a, (uint32_t)seed * 31u + 7u, 3, 3, (int)S.seat, royal != 0, bp);
      if (const char* dd = std::getenv("EMB_M3B_SEAT")) {   // debugging aid: EMB_M3B_SEAT=<culture>:<royal>
        const char* c = std::strchr(dd, ':');
        if (std::atoi(dd) == a && c && std::atoi(c + 1) == royal) {
          static int why[8192];
          interiorWhy(why, 8192, true);
          out("m3b seat %s %s: template %s, plan %d, %d floors\n", bldgTypeName(b.type), cult::archetypeName((cult::Archetype)a), interiorTemplate(b, bp, b.seed), (int)bp.interior.plan, b.floors());
          interiorWhy(why, 8192, true);
          std::string ws;
          for (int i = 0; i < 8192; i++) if (why[i]) ws += " line " + std::to_string(i) + " x" + std::to_string(why[i]) + ";";
          out("  turned down at:%s\n", ws.c_str());
          for (int f = 0; f < b.floors(); f++) { Map m; genInteriorRooms(m, b, bp, b.seed, f); dumpFloor(m); }
        }
      }
      uint64_t sig = 0;
      std::string why = checkBuildingV7(b, s7, sig, &bp);
      n++;
      tmplCount[s7.tmpl]++;
      if (why.empty() && s7.tmpl.rfind("seat", 0) != 0) why = "a seat of power without a seat plan (" + s7.tmpl + ")";
      if (!why.empty()) fail(royal ? "royal seat" : "lord's seat", b, a, why);
    }
  }
  // 3. every purpose on every floor plan (the culture rotates with the seed)
  for (int t = 0; t < NB; t++)
    for (int fp = 0; fp < (int)bld::Floorplan::COUNT; fp++) {
      const int a = (int)((seed + (uint64_t)t * 5 + (uint64_t)fp) % (uint64_t)NA);
      bld::Blueprint bp;
      Bldg b = cultureBldg((Building)t, a, (uint32_t)seed * 4099u + (uint32_t)fp, 1, 1 + fp % 3, -1, false, bp);
      bp.interior.plan = (bld::Floorplan)fp;
      bp.seat = 0;   // (a keep's or palace's people's seat plans in section 2: here the castle planner on every floor plan)
      bp.interior.lCorner = (uint8_t)((seed + (uint64_t)t) % 4);
      if (const char* dd = std::getenv("EMB_M3B_FORCED")) {   // debugging aid: EMB_M3B_FORCED=<type>:<plan>
        const char* c = std::strchr(dd, ':');
        if (std::atoi(dd) == t && c && std::atoi(c + 1) == fp) {
          static int why[8192];
          interiorWhy(why, 8192, true);
          out("m3b forced %s %s: template %s, plan %d, %d floors\n", bldgTypeName(b.type), cult::archetypeName((cult::Archetype)a), interiorTemplate(b, bp, b.seed), fp, b.floors());
          interiorWhy(why, 8192, true);
          std::string ws;
          for (int i = 0; i < 8192; i++) if (why[i]) ws += " line " + std::to_string(i) + " x" + std::to_string(why[i]) + ";";
          out("  turned down at:%s\n", ws.c_str());
          for (int f = 0; f < b.floors(); f++) { Map m; genInteriorRooms(m, b, bp, b.seed, f); dumpFloor(m); }
        }
      }
      uint64_t sig = 0;
      std::string why = checkBuildingV7(b, s7, sig, &bp);
      n++;
      tmplCount[s7.tmpl]++;
      if (why.empty()) { bool c; why = shapeCheck(b, bp, c); rounds += fp == (int)bld::Floorplan::Round; courts += c; }
      if (!why.empty()) fail(bld::formName((bld::Form)0), b, a, std::string("plan ") + std::to_string(fp) + ": " + why);
    }
  // 4. inns across the cultures: distinct plans
  std::set<std::string> innT;
  std::set<uint64_t> innSig;
  for (int a = 0; a < NA; a++) {
    bld::Blueprint bp;
    Bldg b = cultureBldg(Building::Inn, a, 4242u, 1, 1, -1, false, bp);
    innT.insert(interiorTemplate(b, bp, b.seed));
    Map m;
    genInteriorRooms(m, b, bp, b.seed, 0);
    innSig.insert(interiorLayoutSignature(m));
  }
  if (innT.size() < 8 || innSig.size() < 8) { out("FAIL: m3b inns: %zu distinct plan templates, %zu distinct layouts across the 12 cultures (need 8)\n", innT.size(), innSig.size()); bad++; }
  // 4b. (M3b fixer) the people choose the inn, the footprint only its variant: on a plain or a long body, of one storey
  //     or two, only the fjordfolk's inn is a mead hall (a long trench of embers on a marsh stilt floor read wrong) and
  //     only the heartland's the heartland plan (no people falls back to another's)
  for (int a = 0; a < NA; a++)
    for (int fp : {(int)bld::Floorplan::Rect, (int)bld::Floorplan::Long})
      for (int st = 1; st <= 2; st++) {
        bld::Blueprint bp;
        Bldg b = cultureBldg(Building::Inn, a, (uint32_t)seed * 131u + (uint32_t)(fp * 7 + st), 1, 1, -1, false, bp);
        bp.interior.plan = (bld::Floorplan)fp;
        b.storeys = (uint8_t)st;
        static int why0[8192];
        if (std::getenv("EMB_INN_WHY")) interiorWhy(why0, 8192, true);
        const std::string tn = interiorTemplate(b, bp, b.seed);
        if (std::getenv("EMB_INN_WHY") && tn == "inn: heartland") {
          interiorWhy(why0, 8192, true);
          std::string ws;
          for (int i = 0; i < 8192; i++) if (why0[i]) ws += " " + std::to_string(i) + "x" + std::to_string(why0[i]);
          out("  inn %s plan %d %d storeys (%dx%d): turned down at%s\n", cult::archetypeName((cult::Archetype)a), fp, st, b.r.w, b.r.h, ws.c_str());
        }
        const cult::Archetype ar = (cult::Archetype)a;
        std::string why;
        if (tn == "inn: mead hall" && ar != cult::Archetype::Fjordfolk) why = "a mead hall inn";
        if (tn == "inn: heartland" && ar != cult::Archetype::Heartland) why = "the heartland's inn";
        if (tn == "plain" || tn == "classic") why = "no inn plan of its own (" + tn + ")";
        if (why.empty()) { uint64_t sig = 0; why = checkBuildingV7(b, s7, sig, &bp); }
        if (!why.empty()) fail("inn by people", b, a, std::string("plan ") + std::to_string(fp) + ", " + std::to_string(st) + " storeys: " + why);
      }
  // 5. (M3b fix) every purpose x every culture asked as a one-storey tent and as a round body: a barracks of one storey
  //    (a steppe riders' barracks tent) must plan, never crash
  for (int t = 0; t < NB; t++)
    for (int a = 0; a < NA; a++)
      for (int fm : {(int)bld::Form::Tent, (int)bld::Form::Round}) {
        if ((Building)t == Building::Palace || (Building)t == Building::Keep) continue;   // (seats: section 2)
        // (a tent is asked only of homes and halls: a barracks, a mead hall, a lodge, a house, a hut)
        if (fm == (int)bld::Form::Tent && !((Building)t == Building::Barracks || (Building)t == Building::MeadHall || (Building)t == Building::Lodge ||
                                            (Building)t == Building::House || (Building)t == Building::Hut)) continue;
        bld::Blueprint bp;
        Bldg b = cultureBldg((Building)t, a, (uint32_t)seed * 7919u + (uint32_t)fm, 1, (int)((seed + (uint64_t)a) % 4), -1, false, bp, fm);
        uint64_t sig = 0;
        std::string why = checkBuildingV7(b, s7, sig, &bp);
        n++;
        tmplCount[s7.tmpl]++;
        if (why.empty() && (Building)t == Building::Barracks) {   // the guards sleep in bunks, keep their arms in racks
          int bunks = 0;
          for (int f = 0; f < b.floors(); f++) {
            Map m;
            genInterior(m, b, b.seed, f);
            for (uint8_t p : m.prop) bunks += p == (int)Prop::BunkBed + 1;
          }
          if (!bunks) why = "a barracks without bunks (template " + s7.tmpl + ")";
        }
        if (!why.empty()) fail(fm == (int)bld::Form::Tent ? "tent" : "round", b, a, why);
      }
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  std::string tl;
  for (auto& kv : tmplCount) tl += " " + kv.first + " " + std::to_string(kv.second) + ";";
  out("interiors m3b: %d buildings (%d purposes x %d cultures, %d seats, %d purposes x %d plans), %d invalid, %d plain fallbacks, "
      "%d round and %d court floors checked; inns %zu templates / %zu layouts over 12 cultures; %.0f ms\n",
      n, NB, NA, NA * 2, NB, (int)bld::Floorplan::COUNT, bad, plainFalls, rounds, courts, innT.size(), innSig.size(), ms);
  if (std::getenv("EMB_INTERIOR_TEMPLATES")) out("  templates:%s\n", tl.c_str());
  return bad;
}

// renting (M0b): in an inn upstairs, the rented room's bed sleeps you, every other guest bed is taken, the
// innkeeper's bed is his. Driven through Game::update with the interact input, like a player.
int lodgingChecks(uint64_t seed) {
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.godMode = true;
  // (M3b) the inn's rooms may be let on any floor: the floor with two rented rooms and the innkeeper's
  int inn = -1, lf = -1;
  for (size_t i = 0; i < g.world.over.bldgs.size() && inn < 0; i++) {
    const Bldg& B = g.world.over.bldgs[i];
    if (B.type != Building::Inn) continue;
    for (int f = 0; f < B.floors() && inn < 0; f++) {
      Map m;
      genInterior(m, B, B.seed, f);
      int guests = 0, own = 0;
      for (const RoomInfo& R : m.rooms) { guests += R.kind == RoomKind::GuestRoom && R.bedX >= 0; own += R.kind == RoomKind::OwnerRoom && R.bedX >= 0; }
      if (guests >= 2 && own) { inn = (int)i; lf = f; }
    }
  }
  if (inn < 0 || !g.debugEnterBuilding(inn, lf)) { out("FAIL: lodging: no inn with its rooms to let\n"); return 1; }
  int mine = -1, other = -1, owner = -1;
  for (int i = 0; i < (int)g.sub.rooms.size(); i++) {
    const RoomInfo& R = g.sub.rooms[(size_t)i];
    if (R.bedX < 0) continue;
    if (R.kind == RoomKind::GuestRoom) { if (mine < 0) mine = i; else if (other < 0) other = i; }
    if (R.kind == RoomKind::OwnerRoom) owner = i;
  }
  if (mine < 0 || other < 0 || owner < 0) { out("FAIL: lodging: the inn upstairs lacks two guest beds and the innkeeper's\n"); return 1; }
  g.lodging.bldg = inn; g.lodging.floor = lf; g.lodging.room = mine; g.lodging.untilDay = g.day + 1;
  g.actors.resize(1);   // the guests upstairs (M0b fix round) would take the interact press: this checks the beds
  auto useBed = [&](int ri) -> std::string {
    const RoomInfo& R = g.sub.rooms[(size_t)ri];
    static const int dx[4] = {0, 1, -1, 0}, dy[4] = {1, 0, 0, -1};
    for (int k = 0; k < 4; k++) {
      int x = R.bedX + dx[k], y = R.bedY + dy[k];
      if (g.sub.blocked(x, y) || g.sub.roomIndexAt(x, y) != ri) continue;
      g.pl().p = Vec2(x * TILE + 8.0f, y * TILE + 10.0f);
      g.pl().aim = norm(Vec2(R.bedX * TILE + 8.0f, R.bedY * TILE + 10.0f) - g.pl().p);
      g.notice.clear();
      Input in;
      in.interact = true;
      g.update(SIM_DT, in);
      g.events.clear();
      return g.notice;
    }
    return "(no free tile beside the bed)";
  };
  int bad = 0;
  float h0 = g.hour;
  std::string a = useBed(mine), b = useBed(other), c = useBed(owner);
  if (a.find("YOUR ROOM") == std::string::npos || std::fabs(g.hour - h0) < 1.0f) { out("FAIL: lodging: own bed did not sleep you (%s)\n", a.c_str()); bad++; }
  if (b.find("TAKEN") == std::string::npos) { out("FAIL: lodging: another guest's bed was not taken (%s)\n", b.c_str()); bad++; }
  if (c.find("INNKEEPER") == std::string::npos) { out("FAIL: lodging: the innkeeper's bed (%s)\n", c.c_str()); bad++; }
  out("lodging: own bed \"%s\", other \"%s\", innkeeper's \"%s\"\n", a.c_str(), b.c_str(), c.c_str());
  return bad;
}

int interiorChecks(uint64_t seed) {
  int bad = 0;
  if (std::getenv("EMB_INTERIOR_TRACE")) { std::fprintf(stderr, "interiorChecks %d\n", (int)seed); std::fflush(stderr); }
  if (seed <= 2) bad += lodgingChecks(seed);
  // the seed's endless start window: its buildings (the start village, and whatever else the window holds)
  auto t7 = std::chrono::steady_clock::now();
  World w7;
  w7.generateEndless(seed);
  if (seed == 1) {   // (seed-independent: once per run)
    const uint64_t h = sampleHash();
    const char* rec = std::getenv("EMB_INTERIOR_V7HASH");
    if (rec && *rec) out("gen-7 interiors sample hash: 0x%016llxull\n", (unsigned long long)h);
    else if (h != kGen7SampleHash) { out("FAIL: gen-7 (M0b) interiors changed: sample hash %016llx, recorded %016llx\n", (unsigned long long)h, (unsigned long long)kGen7SampleHash); bad++; }
  }
  V7Stats s7;
  const auto& B7 = w7.over.bldgs;
  if (const char* dt = std::getenv("EMB_INTERIOR_DUMP")) {
    int want = std::atoi(dt), shown = 0, only = -1;   // "<type>" or "<type>:<building index>"
    if (const char* c = std::strchr(dt, ':')) only = std::atoi(c + 1);
    for (size_t i = 0; i < B7.size() && shown < 4; i++) {
      if ((int)B7[i].type != want || (only >= 0 && (int)i != only)) continue;
      shown++;
      for (int f = 0; f < B7[i].floors(); f++) {
        Map m;
        genInterior(m, B7[i], B7[i].seed, f);
        out("building %zu floor %d (%dx%d, footprint %dx%d, site %d)\n", i, f, m.w, m.h, B7[i].r.w, B7[i].r.h, B7[i].site);
        dumpFloor(m);
      }
    }
  }
  int twins = 0;
  std::map<std::pair<int, int>, std::vector<uint64_t>> townSigs;   // (site, type) -> layouts in the seed's own world
  for (int k = 0; s7.bldgs < 200 || k == 0; k++) {
    for (size_t i = 0; i < B7.size() && (k == 0 || s7.bldgs < 200); i++) {
      Bldg b = B7[i];
      if (k > 0) b.seed = b.seed * 2654435761u + (uint32_t)k * 40503u + 17u;
      s7.bldgs++;
      uint64_t sig = 0;
      std::string why = checkBuildingV7(b, s7, sig);
      if (!why.empty()) {
        if (s7.bad < 6) out("FAIL: v7 interior %zu (type %d, %d floors, variant %d): %s\n", i, (int)b.type, b.floors(), k, why.c_str());
        s7.bad++;
      } else if (k == 0 && b.site >= 0 && b.type != art::Building::Hut) {
        auto& v = townSigs[{b.site, (int)b.type}];
        if (std::find(v.begin(), v.end(), sig) != v.end()) twins++;
        v.push_back(sig);
      }
    }
    if (B7.empty()) break;
  }
  // M1: palaces and barracks (they stand only in endless capitals, so they are built here from their own facts)
  {
    static const Biome biomes[6] = {Biome::Plains, Biome::Snow, Biome::Desert, Biome::Forest, Biome::Taiga, Biome::Autumn};
    for (int k = 0; k < 12; k++) {
      Bldg b;
      b.type = k % 2 ? Building::Barracks : Building::Palace;
      b.r = b.type == Building::Palace ? IRect{0, 0, 15 + 2 * ((k / 2) % 2), 7} : IRect{0, 0, 7, 4};
      b.owner = b.type == Building::Palace ? Role::King : Role::Guard;
      b.storeys = 2;
      b.hearth = true;
      b.biome = biomes[(k / 2) % 6];
      b.seed = (uint32_t)(seed * 2654435761u) ^ (uint32_t)(k * 40503 + 99);
      b.genVer = WORLDGEN_LATEST;
      s7.bldgs++;
      uint64_t sig = 0;
      std::string why = checkBuildingV7(b, s7, sig);
      if (!why.empty()) {
        if (s7.bad < 6) out("FAIL: v7 %s (%dx%d, %s, variant %d): %s\n", bldgTypeName(b.type), b.r.w, b.r.h, biomeName(b.biome), k, why.c_str());
        s7.bad++;
      }
    }
  }
  // M3 (VISION_PLAN 5.6): interiors by culture. Every furnishing (chairs, benches, cushions round low tables, hammocks
  // and sleeping mats, stools) in the homes, huts, farms, inns, shops, temples and keeps, on the culture walls (adobe,
  // ashlar, felt, living wood, wattle, planks, rubble, logs): all the checks above, and the culture's pieces really
  // there (a cushion people's tables are low tables with cushions; a hammock people's homes sleep in hammocks or on
  // mats while the inn's rented rooms keep their beds)
  {
    static const art::Furniture furns[5] = {art::Furniture::Chairs, art::Furniture::Benches, art::Furniture::Cushions, art::Furniture::Hammocks,
                                            art::Furniture::Stools};
    struct T { Building t; int w, h; Role r; };
    static const T types[] = {{Building::House, 4, 3, Role::Villager}, {Building::StoneHouse, 4, 3, Role::Villager}, {Building::Hut, 3, 2, Role::Villager},
                              {Building::Farmhouse, 5, 3, Role::Farmer},  {Building::Inn, 6, 3, Role::Innkeeper},     {Building::Shop, 4, 3, Role::Merchant},
                              {Building::Temple, 6, 4, Role::Priest},     {Building::Keep, 9, 4, Role::Jarl}};
    static const art::WallMat walls[8] = {art::WallMat::Adobe, art::WallMat::Ashlar, art::WallMat::Felt, art::WallMat::Living,
                                          art::WallMat::Wattle, art::WallMat::Plank, art::WallMat::Rubble, art::WallMat::Log};
    static const uint32_t accents[4] = {rgba(170, 40, 40), rgba(40, 70, 160), rgba(40, 120, 60), rgba(200, 160, 50)};
    int cul = 0, culBad = 0, hammocks = 0, mats = 0, lowT = 0, cushions = 0, homeBeds = 0, innBeds = 0, cushionChairs = 0;
    for (int fi = 0; fi < 5; fi++)
      for (const T& tt : types)
        for (int k = 0; k < 3; k++) {
          Bldg b;
          b.type = tt.t;
          b.r = IRect{0, 0, tt.w + (k == 2 && tt.t != Building::Hut ? 1 : 0), tt.h};
          b.owner = tt.r;
          const uint32_t hs = (uint32_t)(seed * 2654435761u) ^ (uint32_t)(fi * 7919 + (int)tt.t * 104729 + k * 31);
          b.storeys = (uint8_t)bldgStoreysV7(b.type, b.r.w, b.r.h, hs);
          b.hearth = bldgHearthV7(b.type, b.storeys, hash32(hs + 77u));
          b.biome = k == 1 ? Biome::Desert : Biome::Plains;
          b.seed = hash32(hs ^ 0xC17u);
          b.genVer = WORLDGEN_LATEST;
          b.styled = true;
          b.arch = art::ArchStyle();
          b.arch.furniture = furns[fi];
          b.arch.wall = walls[(fi + (int)tt.t + k) % 8];
          b.arch.accentTint = accents[(fi + k) % 4];
          s7.bldgs++;
          cul++;
          uint64_t sig = 0;
          std::string why = checkBuildingV7(b, s7, sig);
          int sleeps = 0;   // (M3 fixer round 2) every home has somewhere to sleep, whatever its people sleep on
          for (int f = 0; f < b.floors() && why.empty(); f++) {
            Map m;
            genInterior(m, b, b.seed, f);
            for (int i = 0; i < m.w * m.h; i++) {
              const int p = m.prop[(size_t)i];
              if (!p) continue;
              const Prop pp = (Prop)(p - 1);
              sleeps += pp == Prop::Bed || pp == Prop::BunkBed || pp == Prop::Hammock || pp == Prop::SleepingMat;
              hammocks += pp == Prop::Hammock; mats += pp == Prop::SleepingMat; lowT += pp == Prop::LowTable; cushions += pp == Prop::Cushion;
              if (pp == Prop::Bed && fi == 3) { if (b.type == Building::Inn || b.type == Building::Keep) innBeds++; else homeBeds++; }
              if (fi == 2 && (pp == Prop::TableMeal || pp == Prop::Chair)) cushionChairs++;   // (a small side table may stay)
            }
          }
          const bool home = b.type == Building::House || b.type == Building::StoneHouse || b.type == Building::Hut || b.type == Building::Farmhouse;
          if (why.empty() && home && !sleeps) {
            why = "a home with no bed, hammock or sleeping mat";
            if (std::getenv("EMB_M3B_ALL"))
              for (int f = 0; f < b.floors(); f++) { Map m; genInterior(m, b, b.seed, f); out("  %s floor %d:\n", s7.tmpl.c_str(), f); dumpFloor(m); }
          }
          if (!why.empty()) {
            if (culBad < 6) out("FAIL: v7 culture %s (furniture %d, wall %d, variant %d): %s\n", bldgTypeName(b.type), fi, (int)b.arch.wall, k, why.c_str());
            culBad++;
          }
        }
    if (homeBeds) { out("FAIL: interiors: %d beds in the homes of a hammock people\n", homeBeds); culBad++; }
    if (cushionChairs) { out("FAIL: interiors: %d chairs or high tables in the rooms of a cushion people\n", cushionChairs); culBad++; }
    if (!hammocks || !lowT || !cushions || !innBeds) { out("FAIL: interiors: culture pieces missing (hammocks %d, low tables %d, cushions %d, inn beds %d)\n", hammocks, lowT, cushions, innBeds); culBad++; }
    out("interiors: culture furnishing: %d buildings, %d invalid; hammocks %d, sleeping mats %d, low tables %d, cushions %d, the hammock people's inn and keep beds %d\n",
        cul, culBad, hammocks, mats, lowT, cushions, innBeds);
    s7.bad += culBad;
  }
  // M1 economy: the production buildings (VISION_PLAN 15.7: purposeful rooms). Every type on a few footprints and biomes,
  // with the generator's storeys and hearth; the rooms the trade needs: the windmill's millstones in its loft above the
  // grain store, the watermill's in its hall, the bakery's oven behind the counter, the smelter's furnace, the sawmill's
  // benches, the tannery's vats, the weaver's loom
  {
    static const Biome biomes[4] = {Biome::Plains, Biome::Snow, Biome::Desert, Biome::Forest};
    struct T { Building t; int w, h; Role r; Prop need; int needFloor; };
    static const T types[] = {
        {Building::Windmill, 4, 3, Role::Farmer, Prop::Grindstone, 1}, {Building::Watermill, 4, 3, Role::Farmer, Prop::Grindstone, 0},
        {Building::Granary, 4, 3, Role::Farmer, Prop::COUNT, 0},       {Building::Bakery, 4, 3, Role::Merchant, Prop::COUNT, 0},
        {Building::Butcher, 4, 3, Role::Merchant, Prop::COUNT, 0},     {Building::Tanner, 4, 3, Role::Villager, Prop::QuenchTub, 0},
        {Building::Fishmonger, 4, 3, Role::Merchant, Prop::COUNT, 0},  {Building::Smelter, 5, 3, Role::Smith, Prop::Forge, 0},
        {Building::Sawmill, 5, 3, Role::Villager, Prop::Workbench, 0}, {Building::Weaver, 4, 3, Role::Merchant, Prop::Loom, 0}};
    int econ = 0, econBad = 0;
    for (const T& tt : types)
      for (int k = 0; k < 8; k++) {
        Bldg b;
        b.type = tt.t;
        b.r = IRect{0, 0, tt.w + (k % 3 == 2 && tt.t != Building::Windmill ? 1 : 0), tt.h};
        b.owner = tt.r;
        const uint32_t hs = (uint32_t)(seed * 2246822519u) ^ (uint32_t)(k * 7919 + (int)tt.t * 104729);
        b.storeys = (uint8_t)bldgStoreysV7(b.type, b.r.w, b.r.h, hs);
        b.hearth = bldgHearthV7(b.type, b.storeys, hash32(hs + 77u));
        b.biome = biomes[k % 4];
        b.seed = hash32(hs ^ 0xEC0u);
        b.genVer = WORLDGEN_LATEST;
        s7.bldgs++;
        econ++;
        uint64_t sig = 0;
        std::string why = checkBuildingV7(b, s7, sig);
        if (why.empty() && b.type == Building::Bakery) {   // the bakehouse: an oven or a hearth behind the counter
          Map m;
          genInterior(m, b, b.seed, 0);
          bool oven = false;
          for (uint8_t p : m.prop) if (p == (int)Prop::Oven + 1 || p == (int)Prop::Hearth + 1) oven = true;
          if (!oven) why = "a bakery without an oven";
        }
        if (why.empty() && tt.need != Prop::COUNT && tt.needFloor < b.floors()) {
          Map m;
          genInterior(m, b, b.seed, tt.needFloor);
          bool has = false;
          for (uint8_t p : m.prop) if (p == (int)tt.need + 1) has = true;
          if (!has) why = std::string("without its ") + std::to_string((int)tt.need) + " on floor " + std::to_string(tt.needFloor);
        }
        if (b.type == Building::Windmill && b.floors() < 2) why = "a windmill with one floor (its exterior shows two)";
        if (!why.empty()) {
          if (econBad < 6) out("FAIL: v7 %s (%dx%d, %s, variant %d): %s\n", bldgTypeName(b.type), b.r.w, b.r.h, biomeName(b.biome), k, why.c_str());
          econBad++;
        }
      }
    out("interiors: %d production buildings (mills, granaries, trades), %d invalid\n", econ, econBad);
    s7.bad += econBad;
  }
  bad += s7.bad;
  bad += m3bInteriorChecks(seed, s7);
  std::string var;
  static const char* tn[] = {"house", "stonehouse", "inn", "smithy", "shop", "temple", "keep", "tower", "farm", "hut", "palace", "barracks",
                             "windmill", "watermill", "granary", "bakery", "butcher", "tannery", "fishmonger", "smelter", "sawmill", "weaver"};
  int distinct = 0, total = 0, furnished = 0;
  for (auto& kv : s7.count) {
    int d = (int)s7.sigs[kv.first].size();
    distinct += d; total += kv.second; furnished += (int)s7.furn[kv.first].size();
    char t[48];
    std::snprintf(t, sizeof t, " %s %d%%", kv.first < 22 ? tn[kv.first] : "?", kv.second ? d * 100 / kv.second : 0);
    var += t;
  }
  double ms7 = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t7).count();
  out("interiors v7: %d buildings, %d floors, %.1f rooms/building, %d beds, misplaced beds %d, %d invalid, free floor min %.0f%%, "
      "distinct layouts %d%% (%s), furnished %d%%, same layout twice in a town %d, %.0f ms\n",
      s7.bldgs, s7.floors, s7.bldgs ? (double)s7.rooms / s7.bldgs : 0, s7.beds, s7.misplacedBeds, s7.bad, s7.freeMin * 100,
      total ? distinct * 100 / total : 0, var.c_str(), total ? furnished * 100 / total : 0, twins, ms7);
  const double ms = ms7;
  if (ms > 2000) out("WARN: interiorChecks took %.0f ms (budget ~2000)\n", ms);
  return bad;
}
