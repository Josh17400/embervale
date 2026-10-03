// rpg_test lane checks: interiors (BFS validity, free floor, clutter). M0 homes lane.
// Called once per seed after runSeed; returns failures, reports each with out("FAIL: ...").
//  - gen-2 identity: for seeds 1..3 every pre-M0 interior (WORLDGEN_V2 world) must hash to the recorded value, so old
//    saves keep their exact rooms.
//  - v3 rooms: every building of the seed's v3 world plus re-seeded variants (at least 200 rooms) must be valid: the
//    door and arrival tiles free, at least 60 % of the floor reachable from the door, every bed/chest/altar and every
//    spawn reachable, clutter only on free floor, wall decor only on the back wall.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <queue>
#include "rpg/sim/deco.h"
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
}  // namespace

int interiorChecks(uint64_t seed) {
  int bad = 0;
  auto t0 = std::chrono::steady_clock::now();
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
  double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  out("interiors: %d rooms, %d invalid, free floor avg %.0f%% min %.0f%%, clutter %.1f/room, wall decor %.1f/room, props %.1f/room, %.0f ms\n",
      st.rooms, st.bad, st.rooms ? st.freeSum / st.rooms * 100 : 0, st.freeMin * 100, st.rooms ? (double)st.clutter / st.rooms : 0,
      st.rooms ? (double)st.decor / st.rooms : 0, st.rooms ? (double)st.props / st.rooms : 0, ms);
  if (ms > 2000) out("WARN: interiorChecks took %.0f ms (budget ~2000)\n", ms);
  return bad;
}
