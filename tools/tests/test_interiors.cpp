// rpg_test lane checks: interiors (BFS validity, free floor, clutter). M0 homes lane.
// Called once per seed after runSeed; returns failures, reports each with out("FAIL: ...").
//  - gen-2 identity: for seeds 1..3 every pre-M0 interior (WORLDGEN_V2 world) must hash to the recorded value, so old
//    saves keep their exact rooms.
//  - v3 rooms: every building of the seed's v3 world plus re-seeded variants (at least 200 rooms) must be valid: the
//    door and arrival tiles free, at least 60 % of the floor reachable from the door, every bed/chest/altar and every
//    spawn reachable, clutter only on free floor, wall decor only on the back wall.
//  - v7 rooms and storeys (M0b, VISION_PLAN 15.7): every floor of every building of the seed's v7 world plus
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
// recorded before the M0 interiors existed (rpg_test 1..3 on the phase-A tree)
const uint64_t kGen2Hash[4] = {0, 0x5db166e55ccd2794ull, 0x53a72aac45ef81c3ull, 0x2c2cde60b68b93dbull};
// M0b phase A: the M0 interiors (genInteriorV3) of generator-v6 worlds, recorded before any M0b change. Saves made on
// worlds v3..v6 rebuild exactly these rooms (looted chests and killed spawns are keyed by tile and slot).
const uint64_t kGen6Hash[4] = {0, 0xe71669014ab8e28full, 0x09a784b14128ba17ull, 0x232c62ce948e0729ull};
// M0b integration: the M0b interiors (genInteriorV4, every floor, with stairs and rooms) of generator-v7 worlds. Once
// M0b ships, saves made on v7 worlds key looted chests, killed spawns and the rented room by tile, slot and room index,
// so any later change to these rooms needs a new WORLDGEN version. Re-record (EMB_INTERIOR_V7HASH=1 prints the values)
// only while v7 is unreleased.
// M0b fix round 1 (stairs two tiles wide, two-tile beds, keep/hut/smithy plans, clutter, upstairs residents)
const uint64_t kGen7Hash[4] = {0, 0x1116c859902c3633ull, 0x0692c417dc915513ull, 0xe35048232981c9e2ull};   // M1 fixer round 3 (keep throne halls swept, palace upper rooms varied); before: M1 fixer round 2

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

bool wallDecor(art::Prop p) { return p >= art::Prop::Tapestry && p <= art::Prop::HolySymbol; }

struct Stats { int rooms = 0, bad = 0; double freeSum = 0, freeMin = 1; int clutter = 0, decor = 0, props = 0; };

// returns a failure description, or empty when the room is valid
std::string checkRoom(const Map& m, Stats& st) {
  const int W = m.w, H = m.h;
  auto I = [&](int x, int y) { return (size_t)y * W + x; };
  auto floorT = [&](int x, int y) { return m.in(x, y) && !groundSolid(m.at(x, y)); };
  if (!floorT(m.exitX, m.exitY) || m.solid[I(m.exitX, m.exitY)]) return "door tile blocked";
  if (m.blocked(m.exitX, m.exitY - 1)) return "arrival tile blocked";
  std::vector<int> dist((size_t)W * H, -1);
  std::queue<int> q;
  dist[I(m.exitX, m.exitY)] = 0;
  q.push((int)I(m.exitX, m.exitY));
  while (!q.empty()) {
    int c = q.front(); q.pop();
    int x = c % W, y = c / W;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) {
      int nx = x + dx[k], ny = y + dy[k];
      if (!floorT(nx, ny) || m.solid[I(nx, ny)] || dist[I(nx, ny)] >= 0) continue;
      dist[I(nx, ny)] = dist[(size_t)c] + 1;
      q.push((int)I(nx, ny));
    }
  }
  auto reached = [&](int x, int y) { return m.in(x, y) && dist[I(x, y)] >= 0; };
  int floor = 0, free = 0;
  char buf[160];
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      int p = m.propAt(x, y), d = m.decoAt(x, y);
      if (p) {
        art::Prop pp = (art::Prop)(p - 1);
        st.props++;
        if (wallDecor(pp)) {
          st.decor++;
          if (floorT(x, y) || !floorT(x, y + 1)) { std::snprintf(buf, sizeof buf, "wall decor %d off the back wall at %d,%d", p - 1, x, y); return buf; }
        } else if (!floorT(x, y)) { std::snprintf(buf, sizeof buf, "prop %d inside a wall at %d,%d", p - 1, x, y); return buf; }
        if ((pp == art::Prop::Bed || pp == art::Prop::Chest || pp == art::Prop::Altar) &&
            !(reached(x + 1, y) || reached(x - 1, y) || reached(x, y + 1) || reached(x, y - 1))) {
          std::snprintf(buf, sizeof buf, "usable prop %d at %d,%d unreachable", p - 1, x, y);
          return buf;
        }
      }
      if (d >= (int)Deco::Basket && d <= (int)Deco::Kindling) {
        st.clutter++;
        if (!floorT(x, y) || p) { std::snprintf(buf, sizeof buf, "clutter %d not on free floor at %d,%d", d, x, y); return buf; }
      }
      if (!floorT(x, y)) continue;
      floor++;
      if (reached(x, y)) free++;
    }
  for (const Spawn& s : m.spawns)
    if (!reached(s.x, s.y)) { std::snprintf(buf, sizeof buf, "spawn role %d at %d,%d unreachable", (int)s.role, s.x, s.y); return buf; }
  double f = floor ? (double)free / floor : 0;
  st.freeSum += f;
  st.freeMin = std::min(st.freeMin, f);
  if (f < 0.6) { std::snprintf(buf, sizeof buf, "only %.0f%% of the floor reachable", f * 100); return buf; }
  return "";
}

// ---------------------------------------------------------------------------------------------- v7 (M0b) checks
using art::Building;
using art::Prop;
bool isProp(const Map& m, int x, int y, Prop p) { return m.propAt(x, y) == (int)p + 1; }
bool wallT(const Map& m, int x, int y) { return !m.in(x, y) || m.at(x, y) == Ground::InteriorWall; }

struct V7Stats {
  int bldgs = 0, floors = 0, rooms = 0, bad = 0, misplacedBeds = 0, beds = 0;
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
    if (!roomReached[i]) { std::snprintf(buf, sizeof buf, "room %zu (%s) unreachable", i, roomKindName(m.rooms[i].kind)); return buf; }
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
      if (pp == Prop::Bed || pp == Prop::BunkBed) {
        st.beds++;
        int ri = m.roomIndexAt(x, y);
        if (ri < 0 || !roomAllowsBed(m.rooms[(size_t)ri].kind)) {
          st.misplacedBeds++;
          std::snprintf(buf, sizeof buf, "bed at %d,%d in a %s", x, y, ri >= 0 ? roomKindName(m.rooms[(size_t)ri].kind) : "wall");
          return buf;
        }
        // head to the wall: a one-tile bed right under it, a two-tile bed (M0b fix round) with its head tile (Filler) there
        bool head = wallT(m, x, y - 1) || (pp == Prop::Bed && isProp(m, x, y - 1, Prop::Filler) && wallT(m, x, y - 2));
        if (!head) { std::snprintf(buf, sizeof buf, "bed at %d,%d has no wall at its head", x, y); return buf; }
      }
      if ((pp == Prop::Bed || pp == Prop::BunkBed || pp == Prop::Chest || pp == Prop::Altar) && !beside(x, y)) { std::snprintf(buf, sizeof buf, "usable prop %d at %d,%d unreachable", p - 1, x, y); return buf; }
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
    if (R.bedX >= 0 && !isProp(m, R.bedX, R.bedY, Prop::Bed) && !isProp(m, R.bedX, R.bedY, Prop::BunkBed)) return "room bed without a bed";
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
    for (int i = 0; i < W * H; i++) if (m.prop[(size_t)i] == (int)Prop::Bed + 1) return "a bed on the inn's ground floor";
  }
  if (b.type == Building::Inn && f == 1) {
    int guests = 0;
    for (size_t i = 0; i < m.rooms.size(); i++) {
      const RoomInfo& R = m.rooms[i];
      if (R.kind != RoomKind::GuestRoom) continue;
      guests++;
      if (R.doorX < 0 || R.bedX < 0) return "guest room without a door or a bed";
      if (!propIn((int)i, {Prop::Chest})) return "guest room without a chest";
      if (!propIn((int)i, {Prop::Nightstand, Prop::TableSmall})) return "guest room without a nightstand or table";
    }
    std::vector<int> nums;
    for (const RoomInfo& R : m.rooms) if (R.kind == RoomKind::GuestRoom) nums.push_back(R.guest);
    std::sort(nums.begin(), nums.end());
    for (size_t k = 0; k < nums.size(); k++) if (nums[k] != (int)k) return "guest rooms not numbered 0..n-1";
    if (guests < 2) return "fewer than 2 guest rooms upstairs";
    if (!has(RoomKind::OwnerRoom)) return "no innkeeper's room upstairs";
  }
  if (b.type == Building::Shop && f == 0) {
    int er = m.roomIndexAt(m.exitX, m.exitY - 1);
    if (er < 0 || m.rooms[(size_t)er].kind == RoomKind::Stockroom) return "shop entrance in the stockroom";
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
    bool ok = false;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++)
        if (isProp(m, x, y, Prop::Forge) && (y == 2 || x <= 2 || x >= W - 3)) ok = true;
    if (!ok) return "the forge is not on an outer wall";
  }
  if (b.type == Building::Temple && f == 0 && !isProp(m, m.exitX, 2, Prop::Altar)) return "the altar is not on the temple's axis at the far wall";
  if (b.type == Building::Keep && f == 0 && !has(RoomKind::ThroneHall)) return "keep without a throne hall";
  if (b.type == Building::Palace && f == 0) {
    if (!has(RoomKind::ThroneHall)) return "palace without a throne hall";
    if (!isProp(m, m.exitX, 2, Prop::Throne)) return "the palace's throne is not on the axis at the far wall";
    bool king = false;
    for (const Spawn& s : m.spawns) if (s.role == Role::King && std::abs(s.x - m.exitX) <= 3 && s.y <= 5) king = true;
    if (!king) return "no king by the palace's throne";
    if (!has(RoomKind::Kitchen) || !has(RoomKind::Barracks)) return "palace without its kitchen or guardroom";
  }
  if (b.type == Building::Palace && f == 1 && (!has(RoomKind::OwnerRoom) || !has(RoomKind::Council) || !has(RoomKind::Bedroom)))
    return "palace upstairs lacks the king's bedchamber, the council chamber or the household's bedchambers";
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

// all floors of one building; sig: its layout signature (all floors)
std::string checkBuildingV7(const Bldg& b, V7Stats& st, uint64_t& sig) {
  int floors = b.floors();
  sig = 1469598103934665603ull;
  uint64_t fsig = 1469598103934665603ull;
  Stairs prevUp;
  bool owner = false;
  for (int f = 0; f < floors; f++) {
    Map m;
    genInterior(m, b, b.seed, f);
    st.floors++;
    std::string why = checkFloorV7(m, b, f, st);
    if (!why.empty()) return "floor " + std::to_string(f) + ": " + why;
    if (f > 0 && (m.down.x != prevUp.x || m.down.y != prevUp.y)) return "floor " + std::to_string(f) + ": stairs do not line up with the floor below";
    prevUp = m.up;
    for (auto& R : m.rooms) if (R.kind == RoomKind::OwnerRoom) owner = true;
    sig ^= interiorLayoutSignature(m) + (uint64_t)f * 0x9E3779B97F4A7C15ull;
    sig *= 1099511628211ull;
    fsig = fnv(fsig ^ sig, m.prop.data(), m.prop.size());
  }
  if ((b.type == Building::Keep || b.type == Building::Palace) && !owner) return "keep or palace without the lord's quarters";
  st.sigs[(int)b.type].insert(sig);
  st.furn[(int)b.type].insert(fsig);
  st.count[(int)b.type]++;
  return "";
}
}  // namespace

// renting (M0b): in an inn upstairs, the rented room's bed sleeps you, every other guest bed is taken, the
// innkeeper's bed is his. Driven through Game::update with the interact input, like a player.
int lodgingChecks(uint64_t seed) {
  Game g(seed);
  g.newGame(seed, WORLDGEN_V7);
  g.mode = Mode::Play;
  g.godMode = true;
  int inn = -1;
  for (size_t i = 0; i < g.world.over.bldgs.size() && inn < 0; i++)
    if (g.world.over.bldgs[i].type == Building::Inn && g.world.over.bldgs[i].floors() >= 2) inn = (int)i;
  if (inn < 0 || !g.debugEnterBuilding(inn, 1)) { out("FAIL: lodging: no inn to go upstairs in\n"); return 1; }
  int mine = -1, other = -1, owner = -1;
  for (int i = 0; i < (int)g.sub.rooms.size(); i++) {
    const RoomInfo& R = g.sub.rooms[(size_t)i];
    if (R.bedX < 0) continue;
    if (R.kind == RoomKind::GuestRoom) { if (mine < 0) mine = i; else if (other < 0) other = i; }
    if (R.kind == RoomKind::OwnerRoom) owner = i;
  }
  if (mine < 0 || other < 0 || owner < 0) { out("FAIL: lodging: the inn upstairs lacks two guest beds and the innkeeper's\n"); return 1; }
  g.lodging.bldg = inn; g.lodging.floor = 1; g.lodging.room = mine; g.lodging.untilDay = g.day + 1;
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
  auto t0 = std::chrono::steady_clock::now();
  if (seed <= 2) bad += lodgingChecks(seed);
  if (seed >= 1 && seed <= 3) {
    World w;
    w.generate(seed, WORLDGEN_V2);
    uint64_t h = 1469598103934665603ull;
    for (const Bldg& b : w.over.bldgs) {
      Map m;
      genInterior(m, b, b.seed);
      h = mapHash(m, h);
    }
    if (h != kGen2Hash[seed]) { out("FAIL: gen-2 interiors changed: hash %016llx, recorded %016llx\n", (unsigned long long)h, (unsigned long long)kGen2Hash[seed]); bad++; }
  }
  if (seed >= 1 && seed <= 3) {
    World w6;
    w6.generate(seed, WORLDGEN_V6);
    uint64_t h = 1469598103934665603ull;
    for (const Bldg& b : w6.over.bldgs) {
      Map m;
      genInterior(m, b, b.seed);
      h = mapHash(m, h);
    }
    if (h != kGen6Hash[seed]) { out("FAIL: gen-6 (M0) interiors changed: hash %016llx, recorded %016llx\n", (unsigned long long)h, (unsigned long long)kGen6Hash[seed]); bad++; }
  }
  if (seed >= 1 && seed <= 3) {
    World w7;
    w7.generate(seed, WORLDGEN_V7);
    uint64_t h = 1469598103934665603ull;
    for (const Bldg& b : w7.over.bldgs)
      for (int f = 0; f < b.floors(); f++) {
        Map m;
        genInterior(m, b, b.seed, f);
        h = floorHash(m, h);
      }
    const char* rec = std::getenv("EMB_INTERIOR_V7HASH");
    if (rec && *rec) out("gen-7 interiors hash, seed %llu: 0x%016llxull\n", (unsigned long long)seed, (unsigned long long)h);
    else if (h != kGen7Hash[seed]) { out("FAIL: gen-7 (M0b) interiors changed: hash %016llx, recorded %016llx\n", (unsigned long long)h, (unsigned long long)kGen7Hash[seed]); bad++; }
  }
  World w;
  w.generate(seed, WORLDGEN_V3);
  Stats st;
  const auto& B = w.over.bldgs;
  for (int k = 0; st.rooms < 200 || k == 0; k++) {
    for (size_t i = 0; i < B.size() && (k == 0 || st.rooms < 200); i++) {
      Bldg b = B[i];
      b.genVer = WORLDGEN_V3;
      if (k > 0) b.seed = b.seed * 2654435761u + (uint32_t)k * 40503u + 17u;
      Map m;
      genInterior(m, b, b.seed);
      st.rooms++;
      std::string why = checkRoom(m, st);
      if (!why.empty()) {
        if (st.bad < 5) out("FAIL: interior %zu (type %d, variant %d): %s\n", i, (int)b.type, k, why.c_str());
        st.bad++;
      }
    }
    if (B.empty()) break;
  }
  bad += st.bad;
  double msV3 = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

  // v7: rooms and storeys, every floor of every building
  auto t7 = std::chrono::steady_clock::now();
  World w7;
  w7.generate(seed, WORLDGEN_V7);
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
  bad += s7.bad;
  std::string var;
  static const char* tn[] = {"house", "stonehouse", "inn", "smithy", "shop", "temple", "keep", "tower", "farm", "hut", "palace", "barracks"};
  int distinct = 0, total = 0, furnished = 0;
  for (auto& kv : s7.count) {
    int d = (int)s7.sigs[kv.first].size();
    distinct += d; total += kv.second; furnished += (int)s7.furn[kv.first].size();
    char t[48];
    std::snprintf(t, sizeof t, " %s %d%%", kv.first < 12 ? tn[kv.first] : "?", kv.second ? d * 100 / kv.second : 0);
    var += t;
  }
  double ms7 = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t7).count();
  out("interiors v7: %d buildings, %d floors, %.1f rooms/building, %d beds, misplaced beds %d, %d invalid, free floor min %.0f%%, "
      "distinct layouts %d%% (%s), furnished %d%%, same layout twice in a town %d, %.0f ms\n",
      s7.bldgs, s7.floors, s7.bldgs ? (double)s7.rooms / s7.bldgs : 0, s7.beds, s7.misplacedBeds, s7.bad, s7.freeMin * 100,
      total ? distinct * 100 / total : 0, var.c_str(), total ? furnished * 100 / total : 0, twins, ms7);
  double ms = msV3 + ms7;
  out("interiors: %d rooms, %d invalid, free floor avg %.0f%% min %.0f%%, clutter %.1f/room, wall decor %.1f/room, props %.1f/room, %.0f ms\n",
      st.rooms, st.bad, st.rooms ? st.freeSum / st.rooms * 100 : 0, st.freeMin * 100, st.rooms ? (double)st.clutter / st.rooms : 0,
      st.rooms ? (double)st.decor / st.rooms : 0, st.rooms ? (double)st.props / st.rooms : 0, ms);
  if (ms > 2000) out("WARN: interiorChecks took %.0f ms (budget ~2000)\n", ms);
  return bad;
}
